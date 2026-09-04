/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "time_manager.h"

#include "time_source.h"
#include "time_server.h"
#include "time_client.h"
#include "time_timer.h"

#include "common/config/config_userapi.h"
#include "common/utils/assertions.h"
#include "common/utils/LOG/log.h"

/* global variables (some may be ununsed, depends on configuration) */
static time_source_t *time_source = NULL;
static time_server_t *time_server = NULL;
static time_client_t *time_client = NULL;

/* tick functions registered at initialization of the time manager */
static time_manager_tick_function_t *tick_functions;
static int tick_functions_count;

/* tick callback, called by time_source every 'millisecond' (can be
 * actual millisecond or simulated millisecond)
 */
static void tick(void *unused)
{
  UNUSED(unused);

  time_timer_ms_tick();

  for (int i = 0; i < tick_functions_count; i++)
    tick_functions[i]();
}

#define TIME_SOURCE "time_source"
#define MODE "mode"
#define SERVER_IP "server_ip"
#define SERVER_PORT "server_port"

static paramdef_t config_parameters[] = {
  {TIME_SOURCE, "time source", 0, .strptr = NULL, .defstrval = NULL, TYPE_STRING, 0},
  {MODE, "time mode", 0, .strptr = NULL, .defstrval = NULL, TYPE_STRING, 0},
  {SERVER_IP, "server ip", 0, .strptr = NULL, .defstrval = NULL, TYPE_STRING, 0},
  {SERVER_PORT, "server port", 0, .iptr = NULL, .defintval = -1, TYPE_INT, 0},
};

static char *get_param_str(char *name)
{
  int idx = config_paramidx_fromname(config_parameters, sizeofArray(config_parameters), name);

  char *param;
  if (config_parameters[idx].strptr != NULL)
    param = *config_parameters[idx].strptr;
  else
    param = NULL;

  return param;
}

static int get_param_int(char *name)
{
  int idx = config_paramidx_fromname(config_parameters, sizeofArray(config_parameters), name);

  return *config_parameters[idx].iptr;
}

void time_manager_start(time_manager_tick_function_t *_tick_functions,
                        int _tick_functions_count,
                        time_source_type_t time_source_type)
{
  bool has_time_server = false;
  bool has_time_client = false;
  char *default_server_ip = "127.0.0.1";
  int default_server_port = 7374;
  char *server_ip = NULL;
  int server_port = -1;

  time_timer_init();

  tick_functions_count = _tick_functions_count;
  tick_functions = calloc_or_fail(tick_functions_count, sizeof(*tick_functions));
  memcpy(tick_functions, _tick_functions, tick_functions_count * sizeof(*tick_functions));

  /* retrieve configuration */
  int ret = config_get(config_get_if(), config_parameters, sizeofArray(config_parameters), "time_management");
  if (ret >= 0) {
    char *param;

    /* time source */
    param = get_param_str(TIME_SOURCE);
    if (param != NULL) {
      if (!strcmp(param, "realtime")) {
        time_source_type = TIME_SOURCE_REALTIME;
      } else if (!strcmp(param, "iq_samples")) {
        time_source_type = TIME_SOURCE_IQ_SAMPLES;
      } else {
        AssertFatal(0, "bad parameter 'time_source' in section 'time_management', unknown value \"%s\". Valid values are \"realtime\" and \"iq_samples\"\n", param);
      }
    }

    /* mode */
    param = get_param_str(MODE);
    if (param != NULL) {
      if (!strcmp(param, "standalone")) {
        has_time_server = false;
        has_time_client = false;
      } else if (!strcmp(param, "server")) {
        has_time_server = true;
        has_time_client = false;
      } else if (!strcmp(param, "client")) {
        has_time_server = false;
        has_time_client = true;
      } else {
        AssertFatal(0, "bad parameter 'mode' in section 'time_management', unknown value \"%s\". Valid values are \"standalone\", \"server\" and \"client\"\n", param);
      }
    }

    /* server ip */
    param = get_param_str(SERVER_IP);
    if (param != NULL) {
      server_ip = param;
    }

    /* server port */
    int port = get_param_int(SERVER_PORT);
    if (port != -1) {
      server_port = port;
    }
  }

  if (server_ip == NULL)
    server_ip = default_server_ip;

  if (server_port == -1)
    server_port = default_server_port;

  /* create entities, according to selected configuration */
  if (!has_time_client) {
    time_source = new_time_source(time_source_type);
    time_source_set_callback(time_source, tick, NULL);
  }

  if (has_time_server) {
    time_server = new_time_server(server_ip, server_port, tick, NULL);
    time_server_attach_time_source(time_server, time_source);
  }

  if (has_time_client) {
    time_client = new_time_client(server_ip, server_port, tick, NULL);
    /* for the log below, there is no time source, we want to print 'server' as time source */
    time_source_type = 2;
  }

  LOG_I(UTIL,
        "time manager configuration: [time source: %s] [mode: %s] [server IP: %s} [server port: %d]%s\n",
        (char *[]){"reatime", "iq_samples", "server"}[time_source_type],
        has_time_server   ? "server"
        : has_time_client ? "client"
                          : "standalone",
        server_ip,
        server_port,
        has_time_server || has_time_client ? "" : " (server IP/port not used)");
}

void time_manager_iq_samples(uint64_t iq_samples_count,
                             uint64_t iq_samples_per_second)
{
  time_source_iq_add(time_source, iq_samples_count, iq_samples_per_second);
}

void time_manager_finish(void)
{
  /* time source has to be shutdown first */
  if (time_source != NULL) {
    free_time_source(time_source);
    time_source = NULL;
  }

  if (time_server != NULL) {
    free_time_server(time_server);
    time_server = NULL;
  }

  if (time_client != NULL) {
    free_time_client(time_client);
    time_client = NULL;
  }

  free(tick_functions);
  tick_functions = NULL;
  tick_functions_count = 0;

  time_timer_free();
}
