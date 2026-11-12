/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "time_server.h"

#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <poll.h>
#include <errno.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <string.h>

#include "common/utils/system.h"
#include "common/utils/LOG/log.h"

typedef struct {
  pthread_t thread_id;
  _Atomic bool exit;
  int server_socket;
  int tick_fd;
  void (*callback)(void *);
  void *callback_data;
} time_server_private_t;

/* remove client i: socket_fds[i] and its poll entry polls[i + 2] are removed together,
 * so that the remaining clients keep matching entries in both arrays */
static void remove_client(int **socket_fds, struct pollfd **polls, int *client_count, int i)
{
  shutdown((*socket_fds)[i], SHUT_RDWR);
  close((*socket_fds)[i]);
  (*client_count)--;
  int after = *client_count - i;
  memmove(&(*socket_fds)[i], &(*socket_fds)[i + 1], sizeof(int) * after);
  memmove(&(*polls)[i + 2], &(*polls)[i + 3], sizeof(struct pollfd) * after);
  if (*client_count == 0) {
    free(*socket_fds);
    *socket_fds = NULL;
  } else {
    *socket_fds = realloc(*socket_fds, sizeof(int) * *client_count);
    AssertFatal(*socket_fds != NULL, "realloc() failed\n");
  }
  *polls = realloc(*polls, sizeof(struct pollfd) * (*client_count + 2));
  AssertFatal(*polls != NULL, "realloc() failed\n");
}

static void *time_server_thread(void *ts)
{
  time_server_private_t *time_server = ts;
  struct pollfd *polls = NULL;
  int *socket_fds = NULL;
  int client_count = 0;

  polls = calloc_or_fail(2, sizeof(struct pollfd));

  while (!time_server->exit) {
    /* polls[0] is to receive ticks */
    polls[0].fd = time_server->tick_fd;
    polls[0].events = POLLIN;
    /* polls[1] is to accept connections from clients */
    polls[1].fd = time_server->server_socket;
    polls[1].events = POLLIN;

    /* add clients */
    for (int i = 0; i < client_count; i++) {
      polls[i + 2].fd = socket_fds[i];
      polls[i + 2].events = POLLIN;
    }

    int ret = poll(polls, client_count + 2, -1);
    AssertFatal(ret >= 1 || (ret == -1 && errno == EINTR), "poll() failed\n");

    if (time_server->exit)
      break;

    /* check tick */
    if (polls[0].revents) {
      /* do not accept errors */
      AssertFatal(polls[0].revents == POLLIN, "poll() failed\n");
      uint64_t ticks;
      int ret = read(time_server->tick_fd, &ticks, 8);
      DevAssert(ret == 8);
      /* call the callback ticks time */
      for (int i = 0; i < ticks; i++)
        time_server->callback(time_server->callback_data);
      /* send ticks by blocks of 255 max (what fits in a uint8_t) */
      while (ticks) {
        /* get min(ticks, 255) */
        uint8_t count = ticks < 255 ? ticks : 255;
        /* send to all clients */
        for (int i = 0; i < client_count; i++) {
          if (write(socket_fds[i], &count, 1) == 1)
            continue;

          /* error writing - remove client */
          LOG_W(UTIL, "time server: client disconnects\n");
          remove_client(&socket_fds, &polls, &client_count, i);
          i--;
        }
        ticks -= count;
      }
    }

    /* check new client */
    if (polls[1].revents) {
      /* do not accept errors */
      AssertFatal(polls[1].revents == POLLIN, "poll() failed\n");

      int new_socket = accept(time_server->server_socket, NULL, NULL);
      AssertFatal(new_socket != -1, "accept() failed: %s\n", strerror(errno));

      /* add client */
      client_count++;
      socket_fds = realloc(socket_fds, sizeof(int) * client_count);
      AssertFatal(socket_fds != NULL, "realloc() failed\n");
      socket_fds[client_count - 1] = new_socket;
      polls = realloc(polls, sizeof(struct pollfd) * (client_count + 2));
      AssertFatal(polls != NULL, "realloc() failed\n");
      continue;
    }

    /* any event on a client socket is to be considered as an error */
    for (int i = 0; i < client_count; i++) {
      if (polls[i + 2].revents == 0)
        continue;

      LOG_W(UTIL,
            "time server: error on client's socket, removing client (revents is %d, fd is %d i is %d)\n",
            polls[i + 2].revents,
            polls[i + 2].fd,
            i);
      remove_client(&socket_fds, &polls, &client_count, i);
      i--;
    }
  }

  /* close all clients' sockets */
  for (int i = 0; i < client_count; i++) {
    shutdown(socket_fds[i], SHUT_RDWR);
    close(socket_fds[i]);
  }

  free(polls);
  free(socket_fds);

  return NULL;
}

time_server_t *new_time_server(const char *ip,
                               int port,
                               void (*callback)(void *),
                               void *callback_data)
{
  time_server_private_t *ts;
  int ret;

  ts = calloc_or_fail(1, sizeof(*ts));

  ts->server_socket = socket(AF_INET, SOCK_STREAM, 0);
  AssertFatal(ts->server_socket != -1, "socket() failed: %s\n", strerror(errno));

  int v = 1;
  ret = setsockopt(ts->server_socket, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v));
  AssertFatal(ret == 0, "setsockopt() failed: %s\n", strerror(errno));
  v = 1;
  ret = setsockopt(ts->server_socket, IPPROTO_TCP, TCP_NODELAY, &v, sizeof(v));
  AssertFatal(ret == 0, "setsockopt() failed: %s\n", strerror(errno));
  ts->tick_fd = eventfd(0, 0);
  AssertFatal(ts->tick_fd != -1, "eventfd() failed: %s\n", strerror(errno));

  struct sockaddr_in ip_addr;
  ip_addr.sin_family = AF_INET;
  ip_addr.sin_port = htons(port);
  ret = inet_aton(ip, &ip_addr.sin_addr);
  AssertFatal(ret != 0, "inet_aton() failed: %s\n", strerror(errno));
  ret = bind(ts->server_socket, (struct sockaddr *)&ip_addr, sizeof(ip_addr));
  AssertFatal(ret == 0, "bind() failed: %s\n", strerror(errno));

  ret = listen(ts->server_socket, 10); /* 10: arbitrary value */
  AssertFatal(ret == 0, "listen() failed: %s\n", strerror(errno));

  ts->callback = callback;
  ts->callback_data = callback_data;

  threadCreate(&ts->thread_id, time_server_thread, ts, "time server", -1, SCHED_OAI);
  return ts;
}

void free_time_server(time_server_t *ts)
{
  time_server_private_t *time_server = ts;

  /* flag exit */
  time_server->exit = true;

  /* send a fake tick to wake up the server thread */
  uint64_t v = 1;
  int ret = write(time_server->tick_fd, &v, 8);
  DevAssert(ret == 8);

  /* wait for thread termination */
  void *retval;
  ret = pthread_join(time_server->thread_id, &retval);

  /* free memory */
  shutdown(time_server->server_socket, SHUT_RDWR);
  close(time_server->server_socket);
  close(time_server->tick_fd);
  free(time_server);
}

static void time_server_tick_callback(void *ts)
{
  time_server_private_t *time_server = ts;
  uint64_t v = 1;
  int ret = write(time_server->tick_fd, &v, 8);
  DevAssert(ret == 8);
}

void time_server_attach_time_source(time_server_t *time_server,
                                    time_source_t *time_source)
{
  time_source_set_callback(time_source, time_server_tick_callback, time_server);
}
