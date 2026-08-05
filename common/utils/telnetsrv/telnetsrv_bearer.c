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

#define TELNETSERVERCODE
#include "telnetsrv.h"

#define ERROR_MSG_RET(mSG, aRGS...) do { prnt(mSG, ##aRGS); return 1; } while (0)

static int get_single_ue_rnti(void)
{
  MessageDef *msg_p = itti_alloc_new_message(TASK_RRC_GNB, 0, RRC_GET_SINGLE_UE_RNTI);
  MessageDef *resp_p;
  itti_send_and_receive_msg_to_task(TASK_RRC_GNB, TASK_TELNET, msg_p, &resp_p);
  return resp_p->ittiMsg.rrc_get_single_ue_rnti.rnti ? resp_p->ittiMsg.rrc_get_single_ue_rnti.rnti : -1;
}

int get_single_rnti(char *buf, int debug, telnet_printfunc_t prnt)
{
  UNUSED(debug);
  if (buf)
    ERROR_MSG_RET("no parameter allowed\n");

  int rnti = get_single_ue_rnti();
  if (rnti < 1)
    ERROR_MSG_RET("different number of UEs\n");

  prnt("single UE RNTI %04x\n", rnti);
  return 0;
}

//void rrc_gNB_trigger_new_bearer(int rnti);
int add_bearer(char *buf, int debug, telnet_printfunc_t prnt)
{
  UNUSED(debug);
  int rnti = -1;
  if (!buf) {
    rnti = get_single_ue_rnti();
    if (rnti < 1)
      ERROR_MSG_RET("no UE found\n");
  } else {
    rnti = strtol(buf, NULL, 16);
    if (rnti < 1 || rnti >= 0xfffe)
      ERROR_MSG_RET("RNTI needs to be [1,0xfffe]\n");
  }

  // verify it exists in RRC as well
  MessageDef *msg_p = itti_alloc_new_message(TASK_RRC_GNB, 0, RRC_GET_UE_CONTEXT_BY_RNTI_ANY_DU);
  MessageDef *resp_p;
  itti_send_and_receive_msg_to_task(TASK_RRC_GNB, TASK_TELNET, msg_p, &resp_p);
  if (!resp_p->ittiMsg.rrc_get_ue_context_by_rnti_any_du.ue_context_exists)
    ERROR_MSG_RET("could not find UE with RNTI %04x\n", rnti);

  AssertFatal(false, "not implemented\n");
  //rrc_gNB_trigger_new_bearer(rnti);
  free(resp_p);
  prnt("called rrc_gNB_trigger_new_bearer(%04x)\n", rnti);
  return 0;
}

//void rrc_gNB_trigger_release_bearer(int rnti);
int release_bearer(char *buf, int debug, telnet_printfunc_t prnt)
{
  UNUSED(debug);
  int rnti = -1;
  if (!buf) {
    rnti = get_single_ue_rnti();
    if (rnti < 1)
      ERROR_MSG_RET("no UE found\n");
  } else {
    rnti = strtol(buf, NULL, 16);
    if (rnti < 1 || rnti >= 0xfffe)
      ERROR_MSG_RET("RNTI needs to be [1,0xfffe]\n");
  }

  // verify it exists in RRC as well
  MessageDef *msg_p = itti_alloc_new_message(TASK_RRC_GNB, 0, RRC_GET_UE_CONTEXT_BY_RNTI_ANY_DU);
  msg_p->ittiMsg.rrc_get_ue_context_by_rnti_any_du.rnti = rnti;
  MessageDef *resp_p;
  itti_send_and_receive_msg_to_task(TASK_RRC_GNB, TASK_TELNET, msg_p, &resp_p);
  if (!resp_p->ittiMsg.rrc_get_ue_context_by_rnti_any_du.ue_context_exists)
    ERROR_MSG_RET("could not find UE with RNTI %04x\n", rnti);

  AssertFatal(false, "not implemented\n");
  //rrc_gNB_trigger_release_bearer(rnti);
  prnt("called rrc_gNB_trigger_release_bearer(%04x)\n", rnti);
  free(resp_p);
  return 0;
}

static telnetshell_cmddef_t bearercmds[] = {
  {"get_single_rnti", "", get_single_rnti},
  {"add_bearer", "[rnti(hex,opt)]", add_bearer},
  {"release_bearer", "[rnti(hex,opt)]", release_bearer},
  {"", "", NULL},
};

static telnetshell_vardef_t bearervars[] = {

  {"", 0, 0, NULL}
};

void add_bearer_cmds(void) {
  add_telnetcmd("bearer", bearervars, bearercmds);
}
