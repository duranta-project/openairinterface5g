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
 * All SCTP socket operations of the SCTP task go through these functions, so
 * that the underlying SCTP implementation can be exchanged without touching
 * the call sites. The implementation is a thin passthrough to libsctp and
 * kernel sockets.
 *
 * HANDLES. Every call takes and returns an int, exactly like a file
 * descriptor, and that int stays usable with epoll. That keeps
 * itti_subscribe_event_fd(), the epoll loop and the sctp_get_cnx() lookup
 * working unchanged.
 */

#ifndef OAI_SCTP_H_
#define OAI_SCTP_H_

#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/sctp.h>

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
