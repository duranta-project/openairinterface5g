/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this
 * file except in compliance with the License.  You may obtain a copy of the
 * License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/*! \file oai_sctp.h
 * \brief Backend-neutral SCTP calls for F1-C and N2.
 *
 * Two backends sit behind this interface:
 *
 *   kernel   thin passthrough to libsctp and kernel sockets. What OAI has
 *            always used, and what runs when the kernel has CONFIG_IP_SCTP.
 *   usrsctp  the FreeBSD SCTP stack in userspace, sending real SCTP over IP
 *            through a raw IPPROTO_SCTP socket. Needs CAP_NET_RAW but NOT
 *            kernel SCTP, because a raw socket only has to carry the packets,
 *            not implement the protocol.
 *
 * WHY THIS EXISTS. Some kernels ship with CONFIG_IP_SCTP unset and cannot be
 * rebuilt with it -- on the Qualcomm SA8775P the SCTP-enabled rebuild is what
 * breaks the compute-DSP glink endpoints, so SCTP and the DSP offloads are
 * mutually exclusive there. usrsctp lifts that constraint without touching the
 * kernel and without any change at the peer: it is wire-compatible SCTP over
 * IP, not UDP-encapsulated, so the CU or AMF sees an ordinary association.
 *
 * WHY THE CHOICE IS MADE AT RUNTIME, NOT IN CMAKE. The obvious thing is to
 * probe for kernel SCTP at configure time, but that is wrong twice over: OAI is
 * routinely cross-compiled, so a configure-time probe tests the BUILD host's
 * kernel rather than the target's, and the presence of <netinet/sctp.h> in a
 * sysroot says nothing about CONFIG_IP_SCTP in the kernel that will run the
 * binary. A single image also has to work across a reboot into a different
 * kernel. So CMake decides only whether the usrsctp backend is COMPILED IN
 * (ENABLE_USRSCTP); which backend is USED is decided on first use by actually
 * trying to open a kernel SCTP socket. See oai_sctp_backend_name().
 *
 * HANDLES. Every call takes and returns an int, exactly like a file
 * descriptor, and that int stays usable with epoll. For the kernel backend it
 * IS the descriptor. For usrsctp -- whose sockets are `struct socket *` and
 * have no descriptor at all -- it is an eventfd that the backend signals when
 * the association has a message ready. That keeps itti_subscribe_event_fd(),
 * the epoll loop and the sctp_get_cnx() lookup working unchanged.
 */

#ifndef OAI_SCTP_H_
#define OAI_SCTP_H_

#include <stdbool.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/sctp.h>

/* Which backend is in use. Safe to call at any time; forces selection on the
 * first call. Returns "kernel" or "usrsctp". */
const char *oai_sctp_backend_name(void);

/* True once the usrsctp backend has been selected. Only for logging and for
 * the few places that must know; prefer adding a call here over branching on
 * this at a call site. */
bool oai_sctp_is_usrsctp(void);

int oai_sctp_socket(int domain, int type, int protocol);
int oai_sctp_bind(int sd, const struct sockaddr *addr, socklen_t addrlen);
int oai_sctp_bindx(int sd, struct sockaddr *addrs, int addrcnt, int flags);
int oai_sctp_connectx(int sd, struct sockaddr *addrs, int addrcnt, sctp_assoc_t *assoc_id);
int oai_sctp_listen(int sd, int backlog);
int oai_sctp_accept(int sd, struct sockaddr *addr, socklen_t *addrlen);
int oai_sctp_peeloff(int sd, sctp_assoc_t assoc_id);
int oai_sctp_close(int sd);

int oai_sctp_setsockopt(int sd, int level, int optname, const void *optval, socklen_t optlen);
int oai_sctp_getsockopt(int sd, int level, int optname, void *optval, socklen_t *optlen);

ssize_t oai_sctp_recvmsg(int sd,
                         void *buf,
                         size_t len,
                         struct sockaddr *from,
                         socklen_t *fromlen,
                         struct sctp_sndrcvinfo *sinfo,
                         int *msg_flags);

ssize_t oai_sctp_sendmsg(int sd,
                         const void *buf,
                         size_t len,
                         const struct sockaddr *to,
                         socklen_t tolen,
                         uint32_t ppid,
                         uint32_t flags,
                         uint16_t stream_no,
                         uint32_t timetolive,
                         uint32_t context);

int oai_sctp_getpaddrs(int sd, sctp_assoc_t id, struct sockaddr **addrs);
void oai_sctp_freepaddrs(struct sockaddr *addrs);
int oai_sctp_getladdrs(int sd, sctp_assoc_t id, struct sockaddr **addrs);
void oai_sctp_freeladdrs(struct sockaddr *addrs);

#endif /* OAI_SCTP_H_ */
