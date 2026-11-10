/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

#include "log.h"

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

/* returns the socket where the client is connected */
int net_get_client(char *listen_ip, int listen_port)
{
  int s = socket(AF_INET, SOCK_STREAM, 0);
  if (s == -1) { error("socket() failed: %s\n", strerror(errno)); exit(1); }

  int v = 1;
  if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &v, sizeof(v)) == -1)
    { error("setsockopt(NODELAY) failed: %s\n", strerror(errno)); exit(1); }

  v = 1;
  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(int)) == -1)
    { error("setsockopt(REUSEADDR) failed: %s\n", strerror(errno)); exit(1); }

  struct sockaddr_in a;
  a.sin_family = AF_INET;
  a.sin_port   = htons(listen_port);
  a.sin_addr.s_addr = inet_addr(listen_ip);
  if (bind(s, (struct sockaddr *)&a, sizeof(a)))
    { error("bind() failed: %s\n", strerror(errno)); exit(1); }

  if (listen(s, 5)) { error("listen() failed: %s\n", strerror(errno)); exit(1); }

  int t;

  info("listen on %s:%d, wait for a client\n", listen_ip, listen_port);

  socklen_t alen = sizeof(a);
  if ((t = accept(s, (struct sockaddr *)&a, &alen)) == -1)
    { error("accept() failed: %s\n", strerror(errno)); exit(1); }
  info("connect from %s\n", inet_ntoa(a.sin_addr));

  close(s);

  v = 1;
  if (setsockopt(t, IPPROTO_TCP, TCP_NODELAY, &v, sizeof(v)) == -1)
    { error("setsockopt(NODELAY) failed: %s\n", strerror(errno)); exit(1); }

  return t;
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
