/*! \file oai_sctp_usrsctp.c
 * \brief usrsctp backend: the FreeBSD SCTP stack in userspace, speaking real
 *        SCTP over IP through a raw IPPROTO_SCTP socket. See oai_sctp.h.
 *
 * THE ONE STRUCTURAL PROBLEM, AND HOW IT IS SOLVED. usrsctp sockets are
 * `struct socket *`. They are not file descriptors and there is no call that
 * turns one into a descriptor, so they cannot be handed to epoll -- and OAI's
 * SCTP task is built around epoll (itti_subscribe_event_fd, and a dispatch that
 * looks up the connection by events[i].data.fd).
 *
 * So every usrsctp socket here is paired with an eventfd, and it is the EVENTFD
 * that is returned as the int handle and registered with epoll. usrsctp calls
 * our upcall when a socket becomes readable; the upcall drains the message into
 * a per-handle queue and posts one token to the eventfd. The task therefore
 * wakes exactly as it always did, and oai_sctp_usr_recvmsg() consumes one token
 * and pops one message. Nothing in sctp_eNB_task.c has to know.
 *
 * The eventfd is EFD_SEMAPHORE deliberately: one token per queued message means
 * the counter and the queue stay in step, and a level-triggered epoll keeps
 * firing while messages remain. Draining the counter in one read (the default
 * eventfd behaviour) would lose wakeups whenever two messages arrived before
 * the task got scheduled.
 *
 * Receiving inside the upcall is the documented usrsctp pattern; the upcall runs
 * on usrsctp's own receive thread, which is why the queue is mutex-protected.
 */

#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <usrsctp.h>

#include "oai_sctp_internal.h"
#include "common/utils/LOG/log.h"

/* sctp_common.h cannot be used here: it reaches <netinet/sctp.h>, which clashes
 * with <usrsctp.h>. Same macros, declared locally. */
#define SCTP_ERROR(x, args...) LOG_E(SCTP, x, ##args)
#define SCTP_WARN(x, args...) LOG_W(SCTP, x, ##args)
#define SCTP_DEBUG(x, args...) LOG_D(SCTP, x, ##args)

#define USR_MAX_SOCKETS 64
#define USR_RECV_BUFSZ  (1 << 16)

typedef struct usr_msg_s {
  struct usr_msg_s *next;
  struct sockaddr_storage from;
  socklen_t fromlen;
  oai_sctp_rcvinfo_t rcv;
  int flags;
  size_t len;
  uint8_t data[];
} usr_msg_t;

typedef struct {
  struct socket *so; /* NULL means the slot is free */
  int evfd;
  bool listening;
  usr_msg_t *head, *tail;
  pthread_mutex_t lock;
} usr_sock_t;

static usr_sock_t g_socks[USR_MAX_SOCKETS];
static pthread_mutex_t g_tbl_lock = PTHREAD_MUTEX_INITIALIZER;
static bool g_inited;

static usr_sock_t *slot_by_fd(int fd)
{
  for (unsigned i = 0; i < USR_MAX_SOCKETS; i++)
    if (g_socks[i].so != NULL && g_socks[i].evfd == fd)
      return &g_socks[i];
  return NULL;
}

/* ---------------------------------------------------------------- receive */

static void usr_upcall(struct socket *so, void *arg, int flags)
{
  usr_sock_t *s = (usr_sock_t *)arg;
  (void)flags;

  if (s == NULL || s->so != so)
    return;

  /* A listening socket has no message to drain: just say "something happened"
   * and let oai_sctp_usr_accept() do the work, mirroring the kernel path where
   * epoll readability on a listener means an association is pending. */
  if (s->listening) {
    const uint64_t one = 1;
    ssize_t unused __attribute__((unused)) = write(s->evfd, &one, sizeof one);
    return;
  }

  for (;;) {
    struct sockaddr_storage from;
    socklen_t fromlen = sizeof from;
    struct sctp_rcvinfo rcv;
    socklen_t infolen = sizeof rcv;
    unsigned int infotype = 0;
    int msg_flags = 0;

    uint8_t *buf = malloc(USR_RECV_BUFSZ);
    if (buf == NULL) {
      SCTP_ERROR("usrsctp: out of memory in upcall\n");
      return;
    }

    const ssize_t n = usrsctp_recvv(so, buf, USR_RECV_BUFSZ, (struct sockaddr *)&from, &fromlen,
                                    &rcv, &infolen, &infotype, &msg_flags);
    if (n <= 0) {
      free(buf);
      return; /* EAGAIN, or the association is gone */
    }

    usr_msg_t *m = malloc(sizeof *m + (size_t)n);
    if (m == NULL) {
      free(buf);
      SCTP_ERROR("usrsctp: out of memory queueing %zd bytes\n", n);
      return;
    }
    m->next = NULL;
    m->from = from;
    m->fromlen = fromlen;
    /* MSG_NOTIFICATION/MSG_EOR here are usrsctp's values (0x2000/0x8), which are
     * NOT the system ones (0x8000/0x80). Decide their meaning in this file,
     * where usrsctp's headers are the ones in scope, and hand up booleans. */
    m->flags = 0;
    m->len = (size_t)n;
    memcpy(m->data, buf, (size_t)n);
    free(buf);

    /* usrsctp reports sctp_rcvinfo; OAI reads the older sctp_sndrcvinfo. Only
     * the three fields the task actually looks at are meaningful. */
    memset(&m->rcv, 0, sizeof m->rcv);
    m->rcv.notification = (msg_flags & MSG_NOTIFICATION) != 0;
    m->rcv.eor = (msg_flags & MSG_EOR) != 0;

    /* Classify the notification here, where usrsctp's constants are the ones in
     * scope. oai_sctp.c rewrites the buffer with the Linux values, because the
     * two numberings do not differ by a constant offset: usrsctp has
     * REMOTE_ERROR 3 / PARTIAL_DELIVERY 7 against Linux's 0x8004 / 0x8006. */
    if (m->rcv.notification && (size_t)n >= sizeof(uint16_t)) {
      uint16_t t;
      memcpy(&t, m->data, sizeof t);
      switch (t) {
        case SCTP_ASSOC_CHANGE: m->rcv.notif_type = OAI_SCTP_NOTIF_ASSOC_CHANGE; break;
        case SCTP_PEER_ADDR_CHANGE: m->rcv.notif_type = OAI_SCTP_NOTIF_PEER_ADDR_CHANGE; break;
        case SCTP_REMOTE_ERROR: m->rcv.notif_type = OAI_SCTP_NOTIF_REMOTE_ERROR; break;
        case SCTP_SEND_FAILED_EVENT: m->rcv.notif_type = OAI_SCTP_NOTIF_SEND_FAILED; break;
        case SCTP_SHUTDOWN_EVENT: m->rcv.notif_type = OAI_SCTP_NOTIF_SHUTDOWN; break;
        case SCTP_PARTIAL_DELIVERY_EVENT: m->rcv.notif_type = OAI_SCTP_NOTIF_PARTIAL_DELIVERY; break;
        case SCTP_ADAPTATION_INDICATION: m->rcv.notif_type = OAI_SCTP_NOTIF_ADAPTATION; break;
        default: m->rcv.notif_type = OAI_SCTP_NOTIF_OTHER; break;
      }
      if (m->rcv.notif_type == OAI_SCTP_NOTIF_ASSOC_CHANGE
          && (size_t)n >= sizeof(struct sctp_assoc_change)) {
        struct sctp_assoc_change ac;
        memcpy(&ac, m->data, sizeof ac);
        switch (ac.sac_state) {
          case SCTP_COMM_UP: m->rcv.sac_state = OAI_SCTP_SAC_COMM_UP; break;
          case SCTP_COMM_LOST: m->rcv.sac_state = OAI_SCTP_SAC_COMM_LOST; break;
          case SCTP_RESTART: m->rcv.sac_state = OAI_SCTP_SAC_RESTART; break;
          case SCTP_SHUTDOWN_COMP: m->rcv.sac_state = OAI_SCTP_SAC_SHUTDOWN_COMP; break;
          case SCTP_CANT_STR_ASSOC: m->rcv.sac_state = OAI_SCTP_SAC_CANT_STR_ASSOC; break;
          default: m->rcv.sac_state = OAI_SCTP_SAC_OTHER; break;
        }
      }
    }
    if (infotype == SCTP_RECVV_RCVINFO) {
      m->rcv.stream = rcv.rcv_sid;
      m->rcv.ssn = rcv.rcv_ssn;
      m->rcv.flags = rcv.rcv_flags;
      m->rcv.ppid = rcv.rcv_ppid;
      m->rcv.context = rcv.rcv_context;
      m->rcv.tsn = rcv.rcv_tsn;
      m->rcv.cumtsn = rcv.rcv_cumtsn;
      m->rcv.assoc_id = rcv.rcv_assoc_id;
    }

    pthread_mutex_lock(&s->lock);
    if (s->tail)
      s->tail->next = m;
    else
      s->head = m;
    s->tail = m;
    pthread_mutex_unlock(&s->lock);

    const uint64_t one = 1;
    ssize_t unused __attribute__((unused)) = write(s->evfd, &one, sizeof one);
  }
}

/* Tear the associations down on the way out.
 *
 * This matters more than it looks. With kernel SCTP the kernel owns the socket,
 * so when the process dies it sends SHUTDOWN or ABORT and the peer drops the
 * association immediately. usrsctp runs INSIDE this process: if we exit without
 * closing, nothing is ever sent and the peer is left heartbeating an
 * association that will never answer. On F1-C that is not cosmetic -- the CU
 * keeps gNB_DU_id registered and answers the next F1 Setup Request with a
 * Setup Failure until its own timeout expires, so a DU restart appears to work
 * once and then be rejected.
 *
 * Registered with atexit() so it covers OAI's normal shutdown path and any
 * exit() elsewhere. It cannot cover SIGKILL, but nothing can: that is inherent
 * to a userspace stack, and the peer falls back to its heartbeat timeout. */
static void usr_shutdown_atexit(void)
{
  bool any = false;

  pthread_mutex_lock(&g_tbl_lock);
  for (unsigned i = 0; i < USR_MAX_SOCKETS; i++) {
    if (g_socks[i].so != NULL) {
      usrsctp_close(g_socks[i].so);
      g_socks[i].so = NULL;
      any = true;
    }
  }
  pthread_mutex_unlock(&g_tbl_lock);

  /* usrsctp_close() only queues the SHUTDOWN; give its thread a moment to put
   * it on the wire before the process disappears underneath it. Deliberately
   * not usrsctp_finish(), which waits for associations to close and can hang a
   * shutdown path that must not hang. */
  if (any)
    usleep(200000);
}

/* ------------------------------------------------------------------- init */

static void *usr_boot(void *arg)
{
  (void)arg;
  /* port 0 = no UDP encapsulation. F1-C and N2 are SCTP directly over IP, so
   * the peer sees an ordinary association and needs no configuration. */
  usrsctp_init(0, NULL, NULL);
  usrsctp_sysctl_set_sctp_blackhole(2);
  usrsctp_sysctl_set_sctp_no_csum_on_loopback(0);
  return NULL;
}

int oai_sctp_usr_init(void)
{
  pthread_mutex_lock(&g_tbl_lock);
  if (g_inited) {
    pthread_mutex_unlock(&g_tbl_lock);
    return 0;
  }

  /* Start the stack from a thread with DEFAULT scheduling and a full affinity
   * mask. usrsctp's raw-socket mode spawns receive threads, and pthreads
   * inherit the creating thread's policy, priority and CPU mask -- initialising
   * from a real-time thread would strand those receive threads on that thread's
   * core at its priority. Same failure mode, and same fix, as the FastRPC
   * helper threads in the Hexagon offload. */
  pthread_attr_t attr;
  pthread_t boot;
  int rc = -1;

  if (pthread_attr_init(&attr) == 0) {
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
    const struct sched_param sp = {.sched_priority = 0};
    pthread_attr_setschedparam(&attr, &sp);

    cpu_set_t all;
    CPU_ZERO(&all);
    const long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    for (long i = 0; i < (ncpu > 0 ? ncpu : 1); i++)
      CPU_SET((int)i, &all);
    pthread_attr_setaffinity_np(&attr, sizeof all, &all);

    if (pthread_create(&boot, &attr, usr_boot, NULL) == 0) {
      pthread_join(boot, NULL);
      rc = 0;
    }
    pthread_attr_destroy(&attr);
  }

  if (rc != 0) {
    SCTP_ERROR("usrsctp: could not start the bootstrap thread\n");
    pthread_mutex_unlock(&g_tbl_lock);
    return -1;
  }

  for (unsigned i = 0; i < USR_MAX_SOCKETS; i++)
    pthread_mutex_init(&g_socks[i].lock, NULL);

  atexit(usr_shutdown_atexit);
  g_inited = true;
  pthread_mutex_unlock(&g_tbl_lock);
  return 0;
}

/* -------------------------------------------------------- handle handling */

static int slot_new(struct socket *so, bool listening)
{
  const int fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC | EFD_SEMAPHORE);
  if (fd < 0) {
    SCTP_ERROR("usrsctp: eventfd(): %s\n", strerror(errno));
    usrsctp_close(so);
    return -1;
  }

  pthread_mutex_lock(&g_tbl_lock);
  usr_sock_t *s = NULL;
  for (unsigned i = 0; i < USR_MAX_SOCKETS; i++) {
    if (g_socks[i].so == NULL) {
      s = &g_socks[i];
      break;
    }
  }
  if (s == NULL) {
    pthread_mutex_unlock(&g_tbl_lock);
    SCTP_ERROR("usrsctp: out of socket slots (max %d)\n", USR_MAX_SOCKETS);
    close(fd);
    usrsctp_close(so);
    return -1;
  }
  s->so = so;
  s->evfd = fd;
  s->listening = listening;
  s->head = s->tail = NULL;
  pthread_mutex_unlock(&g_tbl_lock);

  usrsctp_set_non_blocking(so, 1);
  usrsctp_set_upcall(so, usr_upcall, s);
  return fd;
}

int oai_sctp_usr_socket(int domain, int type, int protocol)
{
  struct socket *so = usrsctp_socket(domain, type, protocol, NULL, NULL, 0, NULL);
  if (so == NULL) {
    SCTP_ERROR("usrsctp_socket(): %s\n", strerror(errno));
    return -1;
  }
  return slot_new(so, false);
}

int oai_sctp_usr_close(int sd)
{
  pthread_mutex_lock(&g_tbl_lock);
  usr_sock_t *s = slot_by_fd(sd);
  if (s == NULL) {
    pthread_mutex_unlock(&g_tbl_lock);
    return close(sd); /* not ours: a plain descriptor */
  }
  struct socket *so = s->so;
  s->so = NULL;
  pthread_mutex_unlock(&g_tbl_lock);

  usrsctp_close(so);

  pthread_mutex_lock(&s->lock);
  for (usr_msg_t *m = s->head; m != NULL;) {
    usr_msg_t *n = m->next;
    free(m);
    m = n;
  }
  s->head = s->tail = NULL;
  pthread_mutex_unlock(&s->lock);

  return close(s->evfd);
}

#define SLOT_OR_FAIL(sd, s)                                   \
  usr_sock_t *s = slot_by_fd(sd);                             \
  if ((s) == NULL) {                                          \
    SCTP_ERROR("usrsctp: unknown handle %d\n", (sd));          \
    errno = EBADF;                                            \
    return -1;                                                \
  }

int oai_sctp_usr_bind(int sd, const struct sockaddr *addr, socklen_t addrlen)
{
  SLOT_OR_FAIL(sd, s);
  return usrsctp_bind(s->so, (struct sockaddr *)addr, addrlen);
}

int oai_sctp_usr_bindx(int sd, struct sockaddr *addrs, int addrcnt, int flags)
{
  SLOT_OR_FAIL(sd, s);
  int usr_flags;
  switch (flags) {
    case OAI_SCTP_BINDX_ADD: usr_flags = SCTP_BINDX_ADD_ADDR; break;
    case OAI_SCTP_BINDX_REM: usr_flags = SCTP_BINDX_REM_ADDR; break;
    default:
      SCTP_ERROR("usrsctp: unknown bindx flag %d\n", flags);
      errno = EINVAL;
      return -1;
  }
  return usrsctp_bindx(s->so, addrs, addrcnt, usr_flags);
}

int oai_sctp_usr_connectx(int sd, struct sockaddr *addrs, int addrcnt, uint32_t *assoc_id)
{
  SLOT_OR_FAIL(sd, s);
  return usrsctp_connectx(s->so, addrs, addrcnt, (sctp_assoc_t *)assoc_id);
}

int oai_sctp_usr_listen(int sd, int backlog)
{
  SLOT_OR_FAIL(sd, s);
  s->listening = true;
  return usrsctp_listen(s->so, backlog);
}

int oai_sctp_usr_accept(int sd, struct sockaddr *addr, socklen_t *addrlen)
{
  SLOT_OR_FAIL(sd, s);
  uint64_t tok;
  ssize_t unused __attribute__((unused)) = read(s->evfd, &tok, sizeof tok);

  struct socket *ns = usrsctp_accept(s->so, addr, addrlen);
  if (ns == NULL)
    return -1;
  return slot_new(ns, false);
}

int oai_sctp_usr_peeloff(int sd, uint32_t assoc_id)
{
  SLOT_OR_FAIL(sd, s);
  struct socket *ns = usrsctp_peeloff(s->so, assoc_id);
  if (ns == NULL)
    return -1;
  return slot_new(ns, false);
}

/* Socket options are TRANSLATED, never passed through: the constants and the
 * payload structs both differ between the stacks (SCTP_INITMSG 2 vs 3,
 * SCTP_NODELAY 3 vs 4, SCTP_STATUS 14 vs 0x100), and usrsctp has no SCTP_EVENTS
 * at all. The names below resolve against usrsctp's headers, which are the ones
 * in scope in this file. */
int oai_sctp_usr_setopt(int sd, oai_sctp_opt_t opt, const void *val)
{
  SLOT_OR_FAIL(sd, s);

  switch (opt) {
    case OAI_SCTP_OPT_INITMSG: {
      const oai_sctp_initmsg_t *m = (const oai_sctp_initmsg_t *)val;
      struct sctp_initmsg im;
      memset(&im, 0, sizeof im);
      im.sinit_num_ostreams = m->num_ostreams;
      im.sinit_max_instreams = m->max_instreams;
      im.sinit_max_attempts = m->max_attempts;
      im.sinit_max_init_timeo = m->max_init_timeo;
      return usrsctp_setsockopt(s->so, IPPROTO_SCTP, SCTP_INITMSG, &im, sizeof im);
    }

    case OAI_SCTP_OPT_NODELAY: {
      const int on = *(const int *)val;
      return usrsctp_setsockopt(s->so, IPPROTO_SCTP, SCTP_NODELAY, &on, sizeof on);
    }

    case OAI_SCTP_OPT_REUSEADDR: {
      /* Best effort. Address reuse is a convenience for restarts, not something
       * F1-C needs to associate, so a stack that declines it must not abort the
       * gNB the way a failed AssertFatal would. */
      const int on = *(const int *)val;
      if (usrsctp_setsockopt(s->so, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on) < 0)
        SCTP_DEBUG("usrsctp: SO_REUSEADDR declined (%s), continuing\n", strerror(errno));
      return 0;
    }

    case OAI_SCTP_OPT_EVENTS: {
      const oai_sctp_events_t *e = (const oai_sctp_events_t *)val;
      /* No SCTP_EVENTS here: subscribe per notification type with SCTP_EVENT. */
      const struct {
        uint16_t type;
        bool on;
      } wanted[] = {
          {SCTP_ASSOC_CHANGE, e->association},
          {SCTP_PEER_ADDR_CHANGE, e->address},
          {SCTP_REMOTE_ERROR, e->peer_error},
          {SCTP_SEND_FAILED_EVENT, e->send_failure},
          {SCTP_SHUTDOWN_EVENT, e->shutdown},
          {SCTP_PARTIAL_DELIVERY_EVENT, e->partial_delivery},
      };
      for (unsigned i = 0; i < sizeof wanted / sizeof wanted[0]; i++) {
        struct sctp_event ev;
        memset(&ev, 0, sizeof ev);
        ev.se_assoc_id = SCTP_FUTURE_ASSOC; /* applies to the association to come */
        ev.se_type = wanted[i].type;
        ev.se_on = wanted[i].on ? 1 : 0;
        if (usrsctp_setsockopt(s->so, IPPROTO_SCTP, SCTP_EVENT, &ev, sizeof ev) < 0)
          SCTP_WARN("usrsctp: SCTP_EVENT type 0x%x: %s\n", wanted[i].type, strerror(errno));
      }
      /* sctp_data_io_event asks for per-message metadata. usrsctp delivers that
       * through recvv's rcvinfo, which SCTP_RECVRCVINFO enables -- without it
       * usrsctp_recvv reports no infotype and sinfo_assoc_id/stream/ppid would
       * all read back as zero. */
      if (e->data_io) {
        const int on = 1;
        if (usrsctp_setsockopt(s->so, IPPROTO_SCTP, SCTP_RECVRCVINFO, &on, sizeof on) < 0)
          SCTP_WARN("usrsctp: SCTP_RECVRCVINFO: %s\n", strerror(errno));
      }
      return 0;
    }

    default:
      SCTP_ERROR("usrsctp: unsupported set option %d\n", (int)opt);
      errno = ENOPROTOOPT;
      return -1;
  }
}

int oai_sctp_usr_getopt(int sd, oai_sctp_opt_t opt, void *val)
{
  SLOT_OR_FAIL(sd, s);

  if (opt != OAI_SCTP_OPT_STATUS) {
    SCTP_ERROR("usrsctp: unsupported get option %d\n", (int)opt);
    errno = ENOPROTOOPT;
    return -1;
  }

  oai_sctp_status_t *out = (oai_sctp_status_t *)val;
  struct sctp_status st;
  memset(&st, 0, sizeof st);
  st.sstat_assoc_id = out->assoc_id;
  socklen_t len = sizeof st;
  if (usrsctp_getsockopt(s->so, IPPROTO_SCTP, SCTP_STATUS, &st, &len) < 0)
    return -1;

  out->assoc_id = st.sstat_assoc_id;
  out->state = st.sstat_state;
  out->instrms = st.sstat_instrms;
  out->outstrms = st.sstat_outstrms;
  out->fragmentation_point = st.sstat_fragmentation_point;
  out->penddata = st.sstat_penddata;
  return 0;
}

ssize_t oai_sctp_usr_recvmsg(int sd, void *buf, size_t len, struct sockaddr *from,
                             socklen_t *fromlen, oai_sctp_rcvinfo_t *rcvout, int *msg_flags)
{
  SLOT_OR_FAIL(sd, s);

  uint64_t tok;
  ssize_t unused __attribute__((unused)) = read(s->evfd, &tok, sizeof tok);

  pthread_mutex_lock(&s->lock);
  usr_msg_t *m = s->head;
  if (m != NULL) {
    s->head = m->next;
    if (s->head == NULL)
      s->tail = NULL;
  }
  pthread_mutex_unlock(&s->lock);

  if (m == NULL) {
    errno = EAGAIN;
    return -1;
  }

  const size_t n = m->len < len ? m->len : len;
  memcpy(buf, m->data, n);
  if (rcvout)
    *rcvout = m->rcv;
  if (msg_flags)
    *msg_flags = m->flags; /* always 0: the real flags travel as booleans in rcv */
  if (from && fromlen) {
    const socklen_t fl = m->fromlen < *fromlen ? m->fromlen : *fromlen;
    memcpy(from, &m->from, fl);
    *fromlen = fl;
  }
  free(m);
  return (ssize_t)n;
}

ssize_t oai_sctp_usr_sendmsg(int sd, const void *buf, size_t len, const struct sockaddr *to,
                             socklen_t tolen, uint32_t ppid, uint32_t flags,
                             uint16_t stream_no, uint32_t timetolive, uint32_t context)
{
  SLOT_OR_FAIL(sd, s);
  (void)tolen;

  struct sctp_sndinfo snd;
  memset(&snd, 0, sizeof snd);
  snd.snd_sid = stream_no;
  snd.snd_flags = (uint16_t)flags;
  snd.snd_ppid = ppid;
  snd.snd_context = context;
  (void)timetolive; /* PR-SCTP lifetime is unused by F1-C/N2 */

  return usrsctp_sendv(s->so, buf, len, (struct sockaddr *)to, to ? 1 : 0, &snd, sizeof snd,
                       SCTP_SENDV_SNDINFO, 0);
}

int oai_sctp_usr_getpaddrs(int sd, uint32_t id, struct sockaddr **addrs)
{
  SLOT_OR_FAIL(sd, s);
  return usrsctp_getpaddrs(s->so, id, addrs);
}

void oai_sctp_usr_freepaddrs(struct sockaddr *addrs)
{
  usrsctp_freepaddrs(addrs);
}

int oai_sctp_usr_getladdrs(int sd, uint32_t id, struct sockaddr **addrs)
{
  SLOT_OR_FAIL(sd, s);
  return usrsctp_getladdrs(s->so, id, addrs);
}

void oai_sctp_usr_freeladdrs(struct sockaddr *addrs)
{
  usrsctp_freeladdrs(addrs);
}
