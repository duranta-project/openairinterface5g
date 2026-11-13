/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "remote_sim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "remote_sim_net.h"
#include "common/utils/LOG/log.h"

static int remote_sim_socket = -1;

void remote_sim_init(char *server_port)
{
  /* 'server' is either <IP address> or <IP address>:<TCP port> */
  char *server = strdup(server_port);
  if (server == NULL) {
    LOG_E(SIM, "out of memory\n");
    exit(1);
  }

  int port = 4098;   /* default port (4098 is 0x1002, the last bytes of USIM AID) */

  char *port_string;
  if ((port_string = strchr(server, ':'))) {
    port = atoi(port_string + 1);
    *port_string = 0;
  }

  remote_sim_socket = net_connect_to_server(server, port);
  net_message_t m;
  if (!net_receive(remote_sim_socket, &m) || m.type != NET_CARD_READY) {
    LOG_E(SIM, "cannot connect to SIM server %s:%d\n", server, port);
    exit(1);
  }

  free(server);
}

bool remote_sim_enabled(void)
{
  return remote_sim_socket != -1;
}

remote_sim_authenticate_result_t remote_sim_authenticate(unsigned char *rand, unsigned char *autn)
{
  net_message_t m;

  m.type = NET_AUTN;
  memcpy(m.autn.rand, rand, 16);
  memcpy(m.autn.autn, autn, 16);

  remote_sim_authenticate_result_t res = { .type = AUTHENTICATE_FAILURE };

  if (!net_send(remote_sim_socket, &m)
      || !net_receive(remote_sim_socket, &m)) {
    LOG_E(SIM, "communication with SIM server failed\n");
    return res;
  }

  switch (m.type) {
    case NET_RES:
      /* only MILENAGE supported for the moment */
      /* change this test if other than MILENAGE has to be supported */
      if (m.res.res_len != 8) {
        LOG_E(SIM, "bad RES length (%d), should be 8\n", m.res.res_len);
        break;
      }
      res.type = AUTHENTICATE_SUCCESS;
      res.success.res_length = 8;
      memcpy(res.success.res, m.res.res, 8);
      memcpy(res.success.ck, m.res.ck, 16);
      memcpy(res.success.ik, m.res.ik, 16);
      break;
    case NET_AUTS:
      res.type = AUTHENTICATE_RESYNC;
      memcpy(res.resync.auts, m.auts.auts, 14);
      break;
    case NET_ERR:
      LOG_E(SIM, "AUTHENTICATE failed, check the logs of the SIM server\n");
      break;
    default:
      LOG_E(SIM, "wrong message received (type %d)\n", m.type);
      break;
  }

  return res;
}
