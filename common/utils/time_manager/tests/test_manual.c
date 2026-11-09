/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "../time_source.h"
#include "../time_server.h"
#include "../time_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>

#include "common/utils/system.h"
#include "common/utils/LOG/log.h"

#include "common/config/config_userapi.h"

configmodule_interface_t *uniqCfg = NULL;

_Atomic bool test_exit;

static void *iq_generate_thread(void *ts)
{
  time_source_t *time_source = ts;

  while (!test_exit) {
    printf("iq_generate_thread calls time_source_iq_add(time_source, 100, 10000)\n");
    time_source_iq_add(time_source, 100, 10000);
    sleep(1);
  }

  return NULL;
}

#define STANDALONE 0
#define SERVER     1
#define CLIENT     2

static void usage(void)
{
  printf("options:\n");
  printf("  -client\n");
  printf("      run as client (default is standalone)\n");
  printf("  -server\n");
  printf("      run as server (default is standalone)\n");
  printf("  -ip <ip address (default 127.0.0.1)>\n");
  printf("      use this ip address for server\n");
  printf("  -port <port (default 7473)\n");
  printf("      use this port for server\n");
  printf("  -realtime\n");
  printf("      run with realtime clock (default is simulated iq samples)\n");
  exit(0);
}

static void server_callback(void *callback_data)
{
  printf("server_callback called (callback_data %p)\n", callback_data);
  DevAssert(callback_data == (void *)1);
}

static void client_callback(void *callback_data)
{
  printf("client_callback called (callback_data %p)\n", callback_data);
  DevAssert(callback_data == (void *)2);
}

/* dummy sig handler to quit immediately when pressing enter
 * (we send a signal to iq_generate_thread to interrupt sleep())
 */
static void sig_handle(int n)
{
  /* nothing */
}

int main(int n, char **v)
{
  time_source_t *ts = NULL;
  time_server_t *server = NULL;
  time_client_t *client = NULL;
  int mode = STANDALONE;
  char *server_ip = "127.0.0.1";
  int server_port = 7473;
  time_source_type_t time_source_type = TIME_SOURCE_IQ_SAMPLES;
  pthread_t iq_thread;

  for (int i = 1; i < n; i++) {
    if (!strcmp(v[i], "-client")) { mode = CLIENT; continue; }
    if (!strcmp(v[i], "-server")) { mode = SERVER; continue; }
    if (!strcmp(v[i], "-ip")) { if (i>n-2) usage(); server_ip = v[++i]; continue; }
    if (!strcmp(v[i], "-port")) { if (i>n-2) usage(); server_port = atoi(v[++i]); continue; }
    if (!strcmp(v[i], "-realtime")) { time_source_type = TIME_SOURCE_REALTIME; continue; }
    usage();
  }

  logInit();
  /* hack: set log to info (logInit doesn't do it because we don't call load_configmodule()) */
  for (int i = 0; i < MAX_LOG_COMPONENTS; i++)
    g_log->log_component[i].level = OAILOG_INFO;

  if (mode != CLIENT) {
    ts = new_time_source(time_source_type);
    if (time_source_type == TIME_SOURCE_IQ_SAMPLES) {
      void *ret = signal(SIGHUP, sig_handle);
      DevAssert(ret != SIG_ERR);
      threadCreate(&iq_thread, iq_generate_thread, ts, "iq samples generator", -1, SCHED_OAI);
    }
  }

  if (mode == SERVER) {
    /* (void*)1 to check if callback data is passed correctly */
    server = new_time_server(server_ip, server_port, server_callback, (void *)1);
    time_server_attach_time_source(server, ts);
  }

  if (mode == CLIENT) {
    /* (void*)2 to check if callback data is passed correctly */
    client = new_time_client(server_ip, server_port, client_callback, (void *)2);
  }

  printf("main: press enter to quit\n");
  getchar();

  test_exit = true;

  if (mode != CLIENT)
    if (time_source_type == TIME_SOURCE_IQ_SAMPLES) {
      (void)pthread_kill(iq_thread, SIGHUP);
      int ret = pthread_join(iq_thread, NULL);
      DevAssert(ret == 0);
    }

  if (mode != CLIENT) {
    free_time_source(ts);
  }

  if (mode == SERVER) {
    free_time_server(server);
  }

  if (mode == CLIENT) {
    free_time_client(client);
  }

  logTerm();

  return 0;
}
