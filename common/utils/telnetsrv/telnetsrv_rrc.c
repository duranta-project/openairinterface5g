/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "intertask_interface.h"

#include "openair2/RRC/NR/rrc_gNB_UE_context.h"
#include "openair2/RRC/NR/rrc_gNB_NGAP.h"
#include "openair3/NGAP/ngap_gNB_ue_context.h"

#define TELNETSERVERCODE
#include "telnetsrv.h"

#define ERROR_MSG_RET(mSG, aRGS...) do { prnt(mSG, ##aRGS); return 1; } while (0)

/**
 * Module brief:
 * This module is used to add RRCRelease commands to the telnet server in the
 * absence of full support for E2SM RAN Control (RC).
 * This provides similar functionality to the ORAN.WG3.E2SM-RC-R003-v05.00
 * 8.4.5.4 RRC Connection Release Control which is initiated by the RIC.
 *
 * Implementation notes:
 * We refer to the method call rrc_gNB_generate_RRCRelease at rrc_gNB_NGAP.c
 * during rrc_gNB_process_NGAP_UE_CONTEXT_RELEASE_COMMAND message generation.
 *
 * Building the telnetsrv and module:
 * ./build_oai --build-lib telnetsrv
 *
 * Loading the module:
 * sudo ./nr-softmodem -E --rfsim --log_config.global_log_options level,nocolor,time -O ~/gnb.sa.band78.106prb.rfsim.conf --telnetsrv --telnetsrv.shrmod rrc
*/

/**
 * @brief Trigger RRC Release for a specific UE
 * @param buf: RRC UE ID
 * @param debug: Debug flag
 * @param prnt: Print function
 * @return 0 on success, -1 on failure
*/
int rrc_gNB_trigger_release(char *buf, int debug, telnet_printfunc_t prnt)
{
  UNUSED(debug);
  ue_id_t ue_id = -1;
  if(buf) {
    ue_id = strtol(buf, NULL, 10);
    if (ue_id < 1 || ue_id >= 0xfffffe) {
      prnt("UE ID needs to be [1,0xfffffe]\n");
      ERROR_MSG_RET("UE ID needs to be [1,0xfffffe]\n");
    }
  } else {
    MessageDef *msg_ue_rnti_p = itti_alloc_new_message(TASK_RRC_GNB, 0, RRC_GET_SINGLE_UE_RNTI);
    MessageDef *resp_ue_rnti_p;
    if (!itti_send_and_receive_msg_to_task(TASK_RRC_GNB, TASK_TELNET, msg_ue_rnti_p, &resp_ue_rnti_p, 1000)) {
      ERROR_MSG_RET("Timeout waiting for RRC response\n");
    }
    if(!resp_ue_rnti_p->ittiMsg.rrc_get_single_ue_rnti.has_rrc){
      free(resp_ue_rnti_p);
      ERROR_MSG_RET("no RRC present, cannot list counts\n");
    }
    if (!resp_ue_rnti_p->ittiMsg.rrc_get_single_ue_rnti.is_single) {
      free(resp_ue_rnti_p);
      ERROR_MSG_RET("UE count is not exactly one in RRC\n");
    }
    ue_id = resp_ue_rnti_p->ittiMsg.rrc_get_single_ue_rnti.id;
    free(resp_ue_rnti_p);
  }

  /* get RRC and UE */

  MessageDef *msg_p = itti_alloc_new_message(TASK_RRC_GNB, 0, RRC_GNB_GENERATE_RRCRELEASE);
  msg_p->ittiMsg.rrc_gnb_generate_rrcrelease.ue_id = ue_id;
  itti_send_msg_to_task(TASK_RRC_GNB, 0, msg_p);

  prnt("RRC Release triggered for UE %u\n", ue_id);

  return 0;
}

/**
 * @brief Trigger RRC Release for all UEs
*/
int rrc_gNB_trigger_release_all(char *buf, int debug, telnet_printfunc_t prnt)
{
  UNUSED(debug);
  UNUSED(buf);
  MessageDef *msg_p = itti_alloc_new_message(TASK_RRC_GNB, 0, RRC_GNB_GENERATE_RRCRELEASE_ALL);
  MessageDef *resp_p;
  if (!itti_send_and_receive_msg_to_task(TASK_RRC_GNB, TASK_TELNET, msg_p, &resp_p, 1000)) {
    ERROR_MSG_RET("Timeout waiting for RRC response\n");
  }
  for (int i = 0; i < resp_p->ittiMsg.rrc_gnb_generate_rrcrelease_all.nb_releases; i++) {
    prnt("RRC Release triggered for UE %u\n",
         resp_p->ittiMsg.rrc_gnb_generate_rrcrelease_all.rrc_gnb_generate_rrcreleases[i].ue_id);
  }
  free(resp_p);
  return 0;
}

static int rrc_gNB_trigger_ue_context_release_req(char *buf, int debug, telnet_printfunc_t prnt)
{
  UNUSED(debug);
  int ue_id = -1;

  if (!buf) {
    MessageDef *msg_ue_rnti_p = itti_alloc_new_message(TASK_RRC_GNB, 0, RRC_GET_SINGLE_UE_RNTI);
    MessageDef *resp_ue_rnti_p;
    if (!itti_send_and_receive_msg_to_task(TASK_RRC_GNB, TASK_TELNET, msg_ue_rnti_p, &resp_ue_rnti_p, 1000)) {
      ERROR_MSG_RET("Timeout waiting for RRC response\n");
    }
    if(!resp_ue_rnti_p->ittiMsg.rrc_get_single_ue_rnti.has_rrc){
      free(resp_ue_rnti_p);
      ERROR_MSG_RET("no RRC present, cannot list counts\n");
    }
    if (!resp_ue_rnti_p->ittiMsg.rrc_get_single_ue_rnti.is_single) {
      free(resp_ue_rnti_p);
      ERROR_MSG_RET("UE count is not exactly one in RRC\n");
    }
    ue_id = resp_ue_rnti_p->ittiMsg.rrc_get_single_ue_rnti.id;
    free(resp_ue_rnti_p);
  } else {
    char *end = NULL;
    errno = 0;
    long parsed_id = strtol(buf, &end, 10);
    if (end == buf || *end != '\0' || errno != 0 || parsed_id < 1 || parsed_id >= 0xfffffe) {
      ERROR_MSG_RET("UE ID needs to be [1,0xfffffe]\n");
    }
    ue_id = parsed_id;
  }

  MessageDef *msg_p = itti_alloc_new_message(TASK_RRC_GNB, 0, RRC_GNB_TRIGGER_UE_CONTEXT_RELEASE_REQ);
  MessageDef *resp_p;
  msg_p->ittiMsg.rrc_gnb_trigger_ue_context_release_req.ue_id = ue_id;
  if (!itti_send_and_receive_msg_to_task(TASK_RRC_GNB, TASK_TELNET, msg_p, &resp_p, 1000)) {
    ERROR_MSG_RET("Timeout waiting for RRC response\n");
  }

  if (!resp_p->ittiMsg.rrc_gnb_trigger_ue_context_release_req.rrc_ue_context) {
    ERROR_MSG_RET("No RRC UE context for ue_id %d\n", resp_p->ittiMsg.rrc_gnb_trigger_ue_context_release_req.ue_id);
  } else if (!resp_p->ittiMsg.rrc_gnb_trigger_ue_context_release_req.ngap_ue_context) {
    ERROR_MSG_RET("No NGAP UE context for ue_id %d\n", resp_p->ittiMsg.rrc_gnb_trigger_ue_context_release_req.ue_id);
  } else {
    prnt("Sent NGAP UE Context Release Request (user-inactivity) for ue_id %d\n",
         resp_p->ittiMsg.rrc_gnb_trigger_ue_context_release_req.ue_id);
  }
  free(resp_p);
  return 0;
}

static telnetshell_cmddef_t rrc_cmds[] = {
  {"release_rrc", "[rrc_ue_id(int,opt)]", rrc_gNB_trigger_release},
  {"release_rrc_all", "", rrc_gNB_trigger_release_all},
  {"ctx_rel_req", "[rrc_ue_id(int,opt)]", rrc_gNB_trigger_ue_context_release_req},
  {"", "", NULL},
};

static telnetshell_vardef_t rrc_vars[] = {
  {"", 0, 0, NULL}
};

void add_rrc_cmds(void) {
  add_telnetcmd("rrc", rrc_vars, rrc_cmds);
}
