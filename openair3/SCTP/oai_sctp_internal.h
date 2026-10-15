/*! \file oai_sctp_internal.h
 * \brief Backend entry points behind oai_sctp.h. Not a public interface --
 *        include oai_sctp.h instead. Only oai_sctp.c and the backends use this.
 *
 * DELIBERATELY FREE OF BOTH <netinet/sctp.h> AND <usrsctp.h>. Those two headers
 * declare the same struct and constant names and cannot be included in one
 * translation unit. Worse, usrsctp ships its own netinet/sctp.h and exports its
 * directory as a PUBLIC include, so anything linking the usrsctp target gets
 * FreeBSD's netinet/sctp.h in place of the system one -- which is how a file
 * that never mentions usrsctp ends up failing on a missing MSG_NOTIFICATION.
 *
 * So this header describes the backend boundary in plain integers and one POD
 * struct of our own. The kernel side converts to struct sctp_sndrcvinfo; the
 * usrsctp side converts from struct sctp_rcvinfo. Neither header crosses the
 * boundary, and no struct layout is assumed to be shared between them.
 */

#ifndef OAI_SCTP_INTERNAL_H_
#define OAI_SCTP_INTERNAL_H_

#include <stdbool.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>

/* The receive metadata OAI actually consumes, in a layout we own.
 *
 * The two message flags are booleans rather than a flags word ON PURPOSE. The
 * two stacks number them differently -- MSG_NOTIFICATION is 0x8000 on Linux and
 * 0x2000 in usrsctp, MSG_EOR is 0x80 on Linux and 0x8 -- so passing a raw flags
 * word across this boundary silently means the wrong thing. sctp_eNB_task.c
 * tests both: it would have missed every SCTP_ASSOC_CHANGE and rejected every
 * message as truncated. The backend reports what happened; oai_sctp.c re-encodes
 * it using the system constants. */
/* Notification identity, neutral.
 *
 * The wire-visible sn_type inside a notification differs between the stacks and
 * NOT by a constant offset: usrsctp numbers REMOTE_ERROR 3 and PARTIAL_DELIVERY
 * 7, while Linux (whose types start at SCTP_SN_TYPE_BASE = 1<<15) numbers them
 * 0x8004 and 0x8006. sac_state differs too -- usrsctp's SCTP_COMM_UP is 1,
 * Linux's is 0. The task compares against the Linux constants, so the backend
 * classifies and oai_sctp.c rewrites the buffer with the real Linux values. */
typedef enum {
  OAI_SCTP_NOTIF_NONE = 0,
  OAI_SCTP_NOTIF_ASSOC_CHANGE,
  OAI_SCTP_NOTIF_PEER_ADDR_CHANGE,
  OAI_SCTP_NOTIF_SEND_FAILED,
  OAI_SCTP_NOTIF_REMOTE_ERROR,
  OAI_SCTP_NOTIF_SHUTDOWN,
  OAI_SCTP_NOTIF_PARTIAL_DELIVERY,
  OAI_SCTP_NOTIF_ADAPTATION,
  OAI_SCTP_NOTIF_OTHER,
} oai_sctp_notif_t;

typedef enum {
  OAI_SCTP_SAC_COMM_UP = 0,
  OAI_SCTP_SAC_COMM_LOST,
  OAI_SCTP_SAC_RESTART,
  OAI_SCTP_SAC_SHUTDOWN_COMP,
  OAI_SCTP_SAC_CANT_STR_ASSOC,
  OAI_SCTP_SAC_OTHER,
} oai_sctp_sac_t;

typedef struct {
  uint16_t stream;
  uint16_t ssn;
  uint16_t flags;
  uint32_t ppid;
  uint32_t context;
  uint32_t timetolive;
  uint32_t tsn;
  uint32_t cumtsn;
  uint32_t assoc_id;
  bool notification; /* this is an SCTP notification, not user data */
  bool eor;          /* end of record: the message is complete */
  oai_sctp_notif_t notif_type; /* classified by the backend, rewritten by oai_sctp.c */
  oai_sctp_sac_t sac_state;    /* only for OAI_SCTP_NOTIF_ASSOC_CHANGE */
} oai_sctp_rcvinfo_t;

/* Socket options, translated rather than passed through.
 *
 * The two stacks number them differently -- SCTP_INITMSG is 2 on Linux and 3 in
 * usrsctp, SCTP_NODELAY 3 vs 4, SCTP_STATUS 14 vs 0x100 -- and usrsctp has no
 * SCTP_EVENTS at all (it uses per-event SCTP_EVENT). The payload structs differ
 * too: Linux's sctp_event_subscribe has 14 fields, usrsctp's has 11. So the
 * option and its payload are both described neutrally here and re-encoded on
 * each side. Passing the Linux optname and struct straight through is what made
 * sctp_set_init_opt() fail. */
typedef enum {
  OAI_SCTP_OPT_INITMSG = 1,
  OAI_SCTP_OPT_NODELAY,
  OAI_SCTP_OPT_REUSEADDR,
  OAI_SCTP_OPT_EVENTS,
  OAI_SCTP_OPT_STATUS,
} oai_sctp_opt_t;

typedef struct {
  uint16_t num_ostreams, max_instreams, max_attempts, max_init_timeo;
} oai_sctp_initmsg_t;

/* bindx flags, neutral. Linux uses 0x01/0x02; usrsctp uses 0x8001/0x8002 AND
 * feeds the value straight to setsockopt() as the option number, so passing
 * Linux's value is not merely a different constant -- it names an option that
 * does not exist. usrsctp validates the flag first and returns EFAULT, which is
 * how this showed up: "sctp_bindx() SCTP_BINDX_ADD_ADDR failed: errno 14". */
#define OAI_SCTP_BINDX_ADD 1
#define OAI_SCTP_BINDX_REM 2

typedef struct {
  bool data_io, association, address, send_failure, peer_error, shutdown, partial_delivery;
} oai_sctp_events_t;

typedef struct {
  uint32_t assoc_id;
  int32_t state;
  uint16_t instrms, outstrms;
  uint32_t fragmentation_point, penddata;
} oai_sctp_status_t;

#ifdef ENABLE_USRSCTP
/* Brings up the userspace stack. Must be called from a thread with DEFAULT
 * scheduling and an unrestricted affinity mask: usrsctp's raw-socket mode
 * creates receive threads, and pthreads inherit the creating thread's policy,
 * priority and CPU mask. Starting it from a real-time thread would pin those
 * receive threads onto that thread's core at its priority. Returns 0 on
 * success. Idempotent. */
int oai_sctp_usr_init(void);

int oai_sctp_usr_socket(int domain, int type, int protocol);
int oai_sctp_usr_bind(int sd, const struct sockaddr *addr, socklen_t addrlen);
int oai_sctp_usr_bindx(int sd, struct sockaddr *addrs, int addrcnt, int flags);
int oai_sctp_usr_connectx(int sd, struct sockaddr *addrs, int addrcnt, uint32_t *assoc_id);
int oai_sctp_usr_listen(int sd, int backlog);
int oai_sctp_usr_accept(int sd, struct sockaddr *addr, socklen_t *addrlen);
int oai_sctp_usr_peeloff(int sd, uint32_t assoc_id);
int oai_sctp_usr_close(int sd);
int oai_sctp_usr_setopt(int sd, oai_sctp_opt_t opt, const void *val);
int oai_sctp_usr_getopt(int sd, oai_sctp_opt_t opt, void *val);
ssize_t oai_sctp_usr_recvmsg(int sd, void *buf, size_t len, struct sockaddr *from,
                             socklen_t *fromlen, oai_sctp_rcvinfo_t *rcv, int *msg_flags);
ssize_t oai_sctp_usr_sendmsg(int sd, const void *buf, size_t len, const struct sockaddr *to,
                             socklen_t tolen, uint32_t ppid, uint32_t flags,
                             uint16_t stream_no, uint32_t timetolive, uint32_t context);
int oai_sctp_usr_getpaddrs(int sd, uint32_t id, struct sockaddr **addrs);
void oai_sctp_usr_freepaddrs(struct sockaddr *addrs);
int oai_sctp_usr_getladdrs(int sd, uint32_t id, struct sockaddr **addrs);
void oai_sctp_usr_freeladdrs(struct sockaddr *addrs);
#endif /* ENABLE_USRSCTP */

#endif /* OAI_SCTP_INTERNAL_H_ */
