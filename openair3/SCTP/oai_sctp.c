/*! \file oai_sctp.c
 * \brief Backend selection and the kernel passthrough. See oai_sctp.h.
 */

#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "oai_sctp.h"
#include "oai_sctp_internal.h"
#include "sctp_common.h"

typedef enum { OAI_SCTP_KERNEL = 0, OAI_SCTP_USRSCTP } oai_sctp_backend_t;

static oai_sctp_backend_t g_backend = OAI_SCTP_KERNEL;
static pthread_once_t g_once = PTHREAD_ONCE_INIT;

/* Ask the kernel, rather than the build system, whether it speaks SCTP.
 *
 * A configure-time probe cannot answer this: OAI is cross-compiled, so it would
 * test the build host; and <netinet/sctp.h> being present in a sysroot says
 * nothing about CONFIG_IP_SCTP in the kernel that ends up running the binary.
 * Opening a socket is the only honest test, it costs one syscall once per run,
 * and it lets the same image follow a reboot into a different kernel.
 *
 * OAI_SCTP_BACKEND=kernel|usrsctp overrides, for testing the userspace path on
 * a kernel that has SCTP. */
static void oai_sctp_select(void)
{
  const char *force = getenv("OAI_SCTP_BACKEND");

  if (force && strcmp(force, "usrsctp") == 0) {
#ifdef ENABLE_USRSCTP
    if (oai_sctp_usr_init() == 0) {
      g_backend = OAI_SCTP_USRSCTP;
      SCTP_WARN("SCTP backend: usrsctp (forced by OAI_SCTP_BACKEND)\n");
      return;
    }
    SCTP_ERROR("OAI_SCTP_BACKEND=usrsctp but the userspace stack failed to start\n");
#else
    SCTP_ERROR("OAI_SCTP_BACKEND=usrsctp but this build has no usrsctp support"
               " (configure with -DENABLE_USRSCTP=ON)\n");
#endif
  }

  if (!force || strcmp(force, "kernel") == 0) {
    const int sd = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
    if (sd >= 0) {
      close(sd);
      g_backend = OAI_SCTP_KERNEL;
      SCTP_DEBUG("SCTP backend: kernel\n");
      return;
    }
    if (force) { /* explicitly asked for kernel; do not silently substitute */
      SCTP_ERROR("OAI_SCTP_BACKEND=kernel but the kernel has no SCTP: %s\n", strerror(errno));
      g_backend = OAI_SCTP_KERNEL;
      return;
    }
    SCTP_WARN("kernel has no SCTP support (%s), falling back to usrsctp\n", strerror(errno));
  }

#ifdef ENABLE_USRSCTP
  if (oai_sctp_usr_init() == 0) {
    g_backend = OAI_SCTP_USRSCTP;
    SCTP_WARN("SCTP backend: usrsctp (userspace stack on a raw IPPROTO_SCTP socket)\n");
    return;
  }
  SCTP_ERROR("usrsctp failed to start; F1-C/N2 will not come up\n");
#else
  SCTP_ERROR("no kernel SCTP and this build has no usrsctp support:"
             " rebuild with -DENABLE_USRSCTP=ON or use a kernel with CONFIG_IP_SCTP\n");
#endif
  g_backend = OAI_SCTP_KERNEL;
}

static inline bool use_usr(void)
{
  pthread_once(&g_once, oai_sctp_select);
  return g_backend == OAI_SCTP_USRSCTP;
}

const char *oai_sctp_backend_name(void)
{
  return use_usr() ? "usrsctp" : "kernel";
}

bool oai_sctp_is_usrsctp(void)
{
  return use_usr();
}

/* From here down: dispatch. The kernel arm is a straight passthrough, so the
 * behaviour with CONFIG_IP_SCTP present is byte-for-byte what it always was. */

#ifdef ENABLE_USRSCTP
#define OAI_SCTP_DISPATCH(fn, ...) \
  do {                             \
    if (use_usr())                 \
      return oai_sctp_usr_##fn(__VA_ARGS__); \
  } while (0)
#else
#define OAI_SCTP_DISPATCH(fn, ...) \
  do {                             \
    (void)use_usr();               \
  } while (0)
#endif

int oai_sctp_socket(int domain, int type, int protocol)
{
  OAI_SCTP_DISPATCH(socket, domain, type, protocol);
  return socket(domain, type, protocol);
}

int oai_sctp_bind(int sd, const struct sockaddr *addr, socklen_t addrlen)
{
  OAI_SCTP_DISPATCH(bind, sd, addr, addrlen);
  return bind(sd, addr, addrlen);
}

int oai_sctp_bindx(int sd, struct sockaddr *addrs, int addrcnt, int flags)
{
#ifdef ENABLE_USRSCTP
  if (use_usr()) {
    int neutral;
    if (flags == SCTP_BINDX_ADD_ADDR)
      neutral = OAI_SCTP_BINDX_ADD;
    else if (flags == SCTP_BINDX_REM_ADDR)
      neutral = OAI_SCTP_BINDX_REM;
    else {
      SCTP_ERROR("bindx: unknown flag %d\n", flags);
      errno = EINVAL;
      return -1;
    }
    return oai_sctp_usr_bindx(sd, addrs, addrcnt, neutral);
  }
#endif
  return sctp_bindx(sd, addrs, addrcnt, flags);
}

int oai_sctp_connectx(int sd, struct sockaddr *addrs, int addrcnt, sctp_assoc_t *assoc_id)
{
#ifdef ENABLE_USRSCTP
  if (use_usr()) {
    uint32_t id = 0;
    const int rc = oai_sctp_usr_connectx(sd, addrs, addrcnt, &id);
    if (assoc_id)
      *assoc_id = (sctp_assoc_t)id;
    return rc;
  }
#endif
  return sctp_connectx(sd, addrs, addrcnt, assoc_id);
}

int oai_sctp_listen(int sd, int backlog)
{
  OAI_SCTP_DISPATCH(listen, sd, backlog);
  return listen(sd, backlog);
}

int oai_sctp_accept(int sd, struct sockaddr *addr, socklen_t *addrlen)
{
  OAI_SCTP_DISPATCH(accept, sd, addr, addrlen);
  return accept(sd, addr, addrlen);
}

int oai_sctp_peeloff(int sd, sctp_assoc_t assoc_id)
{
  OAI_SCTP_DISPATCH(peeloff, sd, (uint32_t)assoc_id);
  return sctp_peeloff(sd, assoc_id);
}

int oai_sctp_close(int sd)
{
  OAI_SCTP_DISPATCH(close, sd);
  return close(sd);
}

int oai_sctp_setsockopt(int sd, int level, int optname, const void *optval, socklen_t optlen)
{
#ifdef ENABLE_USRSCTP
  if (use_usr()) {
    /* Translate rather than forward: the option numbers and the payload structs
     * both differ between the stacks. Forwarding the Linux ones is what made
     * sctp_set_init_opt() fail with the userspace backend selected. */
    if (level == IPPROTO_SCTP && optname == SCTP_INITMSG) {
      const struct sctp_initmsg *im = (const struct sctp_initmsg *)optval;
      const oai_sctp_initmsg_t m = {.num_ostreams = im->sinit_num_ostreams,
                                    .max_instreams = im->sinit_max_instreams,
                                    .max_attempts = im->sinit_max_attempts,
                                    .max_init_timeo = im->sinit_max_init_timeo};
      return oai_sctp_usr_setopt(sd, OAI_SCTP_OPT_INITMSG, &m);
    }
    if (level == IPPROTO_SCTP && optname == SCTP_NODELAY)
      return oai_sctp_usr_setopt(sd, OAI_SCTP_OPT_NODELAY, optval);
    if (level == SOL_SOCKET && optname == SO_REUSEADDR)
      return oai_sctp_usr_setopt(sd, OAI_SCTP_OPT_REUSEADDR, optval);
    if (level == IPPROTO_SCTP && optname == SCTP_EVENTS) {
      const struct sctp_event_subscribe *e = (const struct sctp_event_subscribe *)optval;
      const oai_sctp_events_t ev = {.data_io = e->sctp_data_io_event != 0,
                                    .association = e->sctp_association_event != 0,
                                    .address = e->sctp_address_event != 0,
                                    .send_failure = e->sctp_send_failure_event != 0,
                                    .peer_error = e->sctp_peer_error_event != 0,
                                    .shutdown = e->sctp_shutdown_event != 0,
                                    .partial_delivery = e->sctp_partial_delivery_event != 0};
      return oai_sctp_usr_setopt(sd, OAI_SCTP_OPT_EVENTS, &ev);
    }
    SCTP_ERROR("usrsctp backend: no translation for setsockopt(level=%d, optname=%d);"
               " add one in oai_sctp.c rather than forwarding it\n", level, optname);
    errno = ENOPROTOOPT;
    return -1;
  }
  (void)optlen;
#endif
  return setsockopt(sd, level, optname, optval, optlen);
}

int oai_sctp_getsockopt(int sd, int level, int optname, void *optval, socklen_t *optlen)
{
#ifdef ENABLE_USRSCTP
  if (use_usr()) {
    if (level == IPPROTO_SCTP && optname == SCTP_STATUS) {
      struct sctp_status *st = (struct sctp_status *)optval;
      oai_sctp_status_t o;
      memset(&o, 0, sizeof o);
      o.assoc_id = (uint32_t)st->sstat_assoc_id;
      if (oai_sctp_usr_getopt(sd, OAI_SCTP_OPT_STATUS, &o) < 0)
        return -1;
      memset(st, 0, sizeof *st);
      st->sstat_assoc_id = (sctp_assoc_t)o.assoc_id;
      st->sstat_state = o.state;
      st->sstat_instrms = o.instrms;
      st->sstat_outstrms = o.outstrms;
      st->sstat_fragmentation_point = o.fragmentation_point;
      st->sstat_penddata = o.penddata;
      if (optlen)
        *optlen = sizeof *st;
      return 0;
    }
    SCTP_ERROR("usrsctp backend: no translation for getsockopt(level=%d, optname=%d)\n",
               level, optname);
    errno = ENOPROTOOPT;
    return -1;
  }
#endif
  return getsockopt(sd, level, optname, optval, optlen);
}

ssize_t oai_sctp_recvmsg(int sd, void *buf, size_t len, struct sockaddr *from,
                         socklen_t *fromlen, struct sctp_sndrcvinfo *sinfo, int *msg_flags)
{
#ifdef ENABLE_USRSCTP
  if (use_usr()) {
    /* The backend reports our own POD struct rather than either stack's, so no
     * layout is assumed to be shared between netinet and usrsctp. Convert here. */
    oai_sctp_rcvinfo_t rcv;
    memset(&rcv, 0, sizeof rcv);
    int backend_flags = 0;
    const ssize_t n = oai_sctp_usr_recvmsg(sd, buf, len, from, fromlen, &rcv, &backend_flags);
    if (n > 0 && msg_flags != NULL) {
      /* Re-encode with the system values; see oai_sctp_rcvinfo_t. */
      *msg_flags = backend_flags;
      if (rcv.notification)
        *msg_flags |= MSG_NOTIFICATION;
      if (rcv.eor)
        *msg_flags |= MSG_EOR;
    }
    /* Rewrite the notification in place with the SYSTEM constants. The task
     * compares snp->sn_header.sn_type against Linux's SCTP_ASSOC_CHANGE
     * (0x8001) and reads sac_state against Linux's enum, neither of which
     * matches what usrsctp puts on the wire. Without this the association comes
     * up, heartbeats flow, and F1 Setup is never sent. */
    if (n > 0 && rcv.notification && buf != NULL) {
      uint16_t linux_type = 0;
      switch (rcv.notif_type) {
        case OAI_SCTP_NOTIF_ASSOC_CHANGE: linux_type = SCTP_ASSOC_CHANGE; break;
        case OAI_SCTP_NOTIF_PEER_ADDR_CHANGE: linux_type = SCTP_PEER_ADDR_CHANGE; break;
        case OAI_SCTP_NOTIF_SEND_FAILED: linux_type = SCTP_SEND_FAILED; break;
        case OAI_SCTP_NOTIF_REMOTE_ERROR: linux_type = SCTP_REMOTE_ERROR; break;
        case OAI_SCTP_NOTIF_SHUTDOWN: linux_type = SCTP_SHUTDOWN_EVENT; break;
        case OAI_SCTP_NOTIF_PARTIAL_DELIVERY: linux_type = SCTP_PARTIAL_DELIVERY_EVENT; break;
        case OAI_SCTP_NOTIF_ADAPTATION: linux_type = SCTP_ADAPTATION_INDICATION; break;
        default: break;
      }
      if (linux_type != 0 && (size_t)n >= sizeof(uint16_t))
        memcpy(buf, &linux_type, sizeof linux_type);

      if (rcv.notif_type == OAI_SCTP_NOTIF_ASSOC_CHANGE
          && (size_t)n >= sizeof(struct sctp_assoc_change)) {
        struct sctp_assoc_change *ac = (struct sctp_assoc_change *)buf;
        switch (rcv.sac_state) {
          case OAI_SCTP_SAC_COMM_UP: ac->sac_state = SCTP_COMM_UP; break;
          case OAI_SCTP_SAC_COMM_LOST: ac->sac_state = SCTP_COMM_LOST; break;
          case OAI_SCTP_SAC_RESTART: ac->sac_state = SCTP_RESTART; break;
          case OAI_SCTP_SAC_SHUTDOWN_COMP: ac->sac_state = SCTP_SHUTDOWN_COMP; break;
          case OAI_SCTP_SAC_CANT_STR_ASSOC: ac->sac_state = SCTP_CANT_STR_ASSOC; break;
          default: break;
        }
      }
    }
    if (n > 0 && sinfo != NULL) {
      memset(sinfo, 0, sizeof *sinfo);
      sinfo->sinfo_stream = rcv.stream;
      sinfo->sinfo_ssn = rcv.ssn;
      sinfo->sinfo_flags = rcv.flags;
      sinfo->sinfo_ppid = rcv.ppid;
      sinfo->sinfo_context = rcv.context;
      sinfo->sinfo_timetolive = rcv.timetolive;
      sinfo->sinfo_tsn = rcv.tsn;
      sinfo->sinfo_cumtsn = rcv.cumtsn;
      sinfo->sinfo_assoc_id = (sctp_assoc_t)rcv.assoc_id;
    }
    return n;
  }
#endif
  return sctp_recvmsg(sd, buf, len, from, fromlen, sinfo, msg_flags);
}

ssize_t oai_sctp_sendmsg(int sd, const void *buf, size_t len, const struct sockaddr *to,
                         socklen_t tolen, uint32_t ppid, uint32_t flags,
                         uint16_t stream_no, uint32_t timetolive, uint32_t context)
{
  OAI_SCTP_DISPATCH(sendmsg, sd, buf, len, to, tolen, ppid, flags, stream_no, timetolive, context);
  return sctp_sendmsg(sd, buf, len, (struct sockaddr *)to, tolen, ppid, flags, stream_no,
                      timetolive, context);
}

int oai_sctp_getpaddrs(int sd, sctp_assoc_t id, struct sockaddr **addrs)
{
  OAI_SCTP_DISPATCH(getpaddrs, sd, (uint32_t)id, addrs);
  return sctp_getpaddrs(sd, id, addrs);
}

void oai_sctp_freepaddrs(struct sockaddr *addrs)
{
#ifdef ENABLE_USRSCTP
  if (use_usr()) {
    oai_sctp_usr_freepaddrs(addrs);
    return;
  }
#endif
  sctp_freepaddrs(addrs);
}

int oai_sctp_getladdrs(int sd, sctp_assoc_t id, struct sockaddr **addrs)
{
  OAI_SCTP_DISPATCH(getladdrs, sd, (uint32_t)id, addrs);
  return sctp_getladdrs(sd, id, addrs);
}

void oai_sctp_freeladdrs(struct sockaddr *addrs)
{
#ifdef ENABLE_USRSCTP
  if (use_usr()) {
    oai_sctp_usr_freeladdrs(addrs);
    return;
  }
#endif
  sctp_freeladdrs(addrs);
}
