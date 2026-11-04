/*! \file oai_sctp.c
 * \brief Kernel SCTP implementation of oai_sctp.h.
 */

#define _GNU_SOURCE
#include <unistd.h>

#include "oai_sctp.h"

int oai_sctp_socket(int domain, int type, int protocol)
{
  return socket(domain, type, protocol);
}

int oai_sctp_bind(int sd, const struct sockaddr *addr, socklen_t addrlen)
{
  return bind(sd, addr, addrlen);
}

int oai_sctp_bindx(int sd, struct sockaddr *addrs, int addrcnt, int flags)
{
  return sctp_bindx(sd, addrs, addrcnt, flags);
}

int oai_sctp_connectx(int sd, struct sockaddr *addrs, int addrcnt, sctp_assoc_t *assoc_id)
{
  return sctp_connectx(sd, addrs, addrcnt, assoc_id);
}

int oai_sctp_listen(int sd, int backlog)
{
  return listen(sd, backlog);
}

int oai_sctp_accept(int sd, struct sockaddr *addr, socklen_t *addrlen)
{
  return accept(sd, addr, addrlen);
}

int oai_sctp_peeloff(int sd, sctp_assoc_t assoc_id)
{
  return sctp_peeloff(sd, assoc_id);
}

int oai_sctp_close(int sd)
{
  return close(sd);
}

int oai_sctp_setsockopt(int sd, int level, int optname, const void *optval, socklen_t optlen)
{
  return setsockopt(sd, level, optname, optval, optlen);
}

int oai_sctp_getsockopt(int sd, int level, int optname, void *optval, socklen_t *optlen)
{
  return getsockopt(sd, level, optname, optval, optlen);
}

ssize_t oai_sctp_recvmsg(int sd, void *buf, size_t len, struct sockaddr *from,
                         socklen_t *fromlen, struct sctp_sndrcvinfo *sinfo, int *msg_flags)
{
  return sctp_recvmsg(sd, buf, len, from, fromlen, sinfo, msg_flags);
}

ssize_t oai_sctp_sendmsg(int sd, const void *buf, size_t len, const struct sockaddr *to,
                         socklen_t tolen, uint32_t ppid, uint32_t flags,
                         uint16_t stream_no, uint32_t timetolive, uint32_t context)
{
  return sctp_sendmsg(sd, buf, len, (struct sockaddr *)to, tolen, ppid, flags, stream_no,
                      timetolive, context);
}

int oai_sctp_getpaddrs(int sd, sctp_assoc_t id, struct sockaddr **addrs)
{
  return sctp_getpaddrs(sd, id, addrs);
}

void oai_sctp_freepaddrs(struct sockaddr *addrs)
{
  sctp_freepaddrs(addrs);
}

int oai_sctp_getladdrs(int sd, sctp_assoc_t id, struct sockaddr **addrs)
{
  return sctp_getladdrs(sd, id, addrs);
}

void oai_sctp_freeladdrs(struct sockaddr *addrs)
{
  sctp_freeladdrs(addrs);
}
