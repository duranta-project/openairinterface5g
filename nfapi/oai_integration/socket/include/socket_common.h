/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#ifndef SOCKET_COMMON_H
#define SOCKET_COMMON_H

#define _GNU_SOURCE /* required for pthread_getname_np */
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netinet/ip.h> /* for IPTOS_DSCP_EF */
#include <netinet/sctp.h>
#include <assertions.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netdb.h>

int socket_send_p5_msg(const int sctp,
                       const int socket,
                       const void* remote_addr,
                       const void* msg,
                       const uint32_t len,
                       const uint16_t stream);
int socket_send_p7_msg(const int socket, const void* remote_addr, const void* msg, const uint32_t len);

#endif // SOCKET_COMMON_H
