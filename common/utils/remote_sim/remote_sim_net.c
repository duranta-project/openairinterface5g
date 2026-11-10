/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "remote_sim_net.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

#include "common/utils/LOG/log.h"

static int fullread(int fd, void *_buf, int count)
{
  char *buf = _buf;
  int ret = count;
  int l;
  while (count) {
    l = read(fd, buf, count);
    if (l <= 0) return -1;
    count -= l;
    buf += l;
  }
  return ret;
}

static int fullwrite(int fd, void *_buf, int count)
{
  char *buf = _buf;
  int ret = count;
  int l;
  while (count) {
    l = write(fd, buf, count);
    if (l == -1 && (errno == EINTR || errno == EAGAIN)) continue;
    if (l <= 0) return -1;
    count -= l;
    buf += l;
  }
  return ret;
}

int net_connect_to_server(char *ip, int port)
{
  int s = socket(AF_INET, SOCK_STREAM, 0);
  if (s == -1) {
    LOG_E(SIM, "socket() failed: %s\n", strerror(errno));
    exit(1);
  }

  int v = 1;
  if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &v, sizeof(v)) == -1) {
    LOG_E(SIM, "setsockopt(NODELAY) failed: %s\n", strerror(errno));
    exit(1);
  }

  struct sockaddr_in a;
  a.sin_family = AF_INET;
  a.sin_port   = htons(port);
  a.sin_addr.s_addr = inet_addr(ip);
  if (connect(s, (struct sockaddr *)&a, sizeof(a)) == -1) {
    LOG_E(SIM, "connect() failed: %s\n", strerror(errno));
    exit(1);
  }

  return s;
}

void net_close(int s)
{
  close(s);
}

bool net_send(int s, net_message_t *m)
{
  unsigned char type = m->type;

  if (fullwrite(s, &type, 1) != 1) return false;

  switch (m->type) {
  case NET_AUTN:
    if (fullwrite(s, m->autn.rand, 16) != 16) return false;
    if (fullwrite(s, m->autn.autn, 16) != 16) return false;
    break;
  case NET_RES:
    if (fullwrite(s, &m->res.res_len, 1) != 1) return false;
    if (fullwrite(s, m->res.res, m->res.res_len) != m->res.res_len) return false;
    if (fullwrite(s, m->res.ck, 16) != 16) return false;
    if (fullwrite(s, m->res.ik, 16) != 16) return false;
    break;
  case NET_AUTS:
    if (fullwrite(s, m->auts.auts, 14) != 14) return false;
    break;
  case NET_IMSI:
    if (fullwrite(s, m->imsi.imsi, 16) != 16) return false;
    break;
  case NET_ERR:
  case NET_GET_IMSI:
  case NET_CARD_READY:
    /* nothing */ break;
  }

  return true;
}

bool net_receive(int s, net_message_t *m)
{
  unsigned char type;

  if (fullread(s, &type, 1) != 1) return false;

  m->type = type;

  switch (type) {
  default: return false;
  case NET_AUTN:
    if (fullread(s, m->autn.rand, 16) != 16) return false;
    if (fullread(s, m->autn.autn, 16) != 16) return false;
    break;
  case NET_RES:
    if (fullread(s, &m->res.res_len, 1) != 1) return false;
    if (m->res.res_len < 4 || m->res.res_len > 16) return false;
    if (fullread(s, m->res.res, m->res.res_len) != m->res.res_len) return false;
    if (fullread(s, m->res.ck, 16) != 16) return false;
    if (fullread(s, m->res.ik, 16) != 16) return false;
    break;
  case NET_AUTS:
    if (fullread(s, m->auts.auts, 14) != 14) return false;
    break;
  case NET_IMSI:
    if (fullread(s, m->imsi.imsi, 16) != 16) return false;
    break;
  case NET_ERR:
  case NET_GET_IMSI:
  case NET_CARD_READY:
    /* nothing */ break;
  }

  return true;
}
