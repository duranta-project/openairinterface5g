/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*! \file telnetsrv_ci.c
 * \brief Implementation of telnet CI functions for gNB
 * \note  This file contains telnet-related functions specific to 5G gNB.
 */

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "openair2/RRC/NR/rrc_gNB_UE_context.h"
#include "openair2/LAYER2/NR_MAC_gNB/nr_mac_gNB.h"
#include "openair2/LAYER2/nr_rlc/nr_rlc_oai_api.h"
#include "openair2/LAYER2/nr_rlc/nr_rlc_ue_manager.h"
#include "openair2/LAYER2/nr_rlc/nr_rlc_entity_am.h"
#include "openair2/LAYER2/NR_MAC_gNB/mac_proto.h"
#include "openair2/LAYER2/NR_MAC_gNB/mac_config.h"
#include "openair2/RRC/NR/rrc_gNB_mobility.h"
#include "openair3/NGAP/ngap_gNB_ue_context.h"
#include "intertask_interface.h"
#include "openair2/RRC/NR/rrc_gNB_du.h"
#define TELNETSERVERCODE
#include "telnetsrv.h"

#define ERROR_MSG_RET(mSG, aRGS...) do { prnt(mSG, ##aRGS); return -1; } while (0)

static int get_single_ue_rnti_mac(void)
{
  NR_UE_info_t *ue = NULL;
  UE_iterator(RC.nrmac[0]->UE_info.connected_ue_list, it) {
    if (it && ue)
      return -1;
    if (it)
      ue = it;
  }
  if (!ue)
    return -1;

  return ue->rnti;
}

int get_single_rnti(char *buf, int debug, telnet_printfunc_t prnt)
{
  UNUSED(debug);
  if (buf)
    ERROR_MSG_RET("no parameter allowed\n");

  int rnti = get_single_ue_rnti_mac();
  if (rnti < 1)
    ERROR_MSG_RET("different number of UEs\n");

  prnt("single UE RNTI %04x\n", rnti);
  return 0;
}

int check_single_rrc_ue(MessageDef *resp_p, telnet_printfunc_t prnt)
{
  if(!resp_p->ittiMsg.rrc_get_single_ue_rnti.has_rrc){
    prnt("no RRC present, cannot list counts\n");
    return 0;
  }
  if (!resp_p->ittiMsg.rrc_get_single_ue_rnti.is_single) {
    prnt("UE count is not exactly one in RRC\n");
    return 0;
  }
  return 1;
}

int get_reestab_count(char *buf, int debug, telnet_printfunc_t prnt)
{
  UNUSED(debug);
  if (!buf) {
    MessageDef *msg_ue_rnti_p = itti_alloc_new_message(TASK_RRC_GNB, 0, RRC_GET_SINGLE_UE_RNTI);
    MessageDef *resp_ue_rnti_p;
    if (!itti_send_and_receive_msg_to_task(TASK_RRC_GNB, TASK_TELNET, msg_ue_rnti_p, &resp_ue_rnti_p, 1000)) {
      ERROR_MSG_RET("Timeout waiting for RRC response\n");
    }
    if (!check_single_rrc_ue(resp_ue_rnti_p, prnt)) {
      free(resp_ue_rnti_p);
      return -1;
    }
    const Rrc_get_single_ue_rnti *resp = &resp_ue_rnti_p->ittiMsg.rrc_get_single_ue_rnti;
    prnt("UE RNTI %04x reestab %d reconfig %d\n", resp->rnti, resp->ue_reestablishment_counter, resp->ue_reconfiguration_counter);
    free(resp_ue_rnti_p);
  } else {
    ue_id_t ue_id = strtol(buf, NULL, 10);
    MessageDef *msg_ue_context_p = itti_alloc_new_message(TASK_RRC_GNB, 0, RRC_GET_UE_CONTEXT_BY_UE_ID);
    msg_ue_context_p->ittiMsg.rrc_get_ue_context_by_ue_id.id = ue_id;
    MessageDef *resp_ue_context_p;
    if (!itti_send_and_receive_msg_to_task(TASK_RRC_GNB, TASK_TELNET, msg_ue_context_p, &resp_ue_context_p, 1000)) {
      ERROR_MSG_RET("Timeout waiting for RRC response\n");
    }
    if (!resp_ue_context_p->ittiMsg.rrc_get_ue_context_by_ue_id.is_single) {
      free(resp_ue_context_p);
      ERROR_MSG_RET("could not find UE with ue_id %d in RRC\n");
    }
    const Rrc_get_single_ue_rnti *resp = &resp_ue_context_p->ittiMsg.rrc_get_ue_context_by_ue_id;
    prnt("UE RNTI %04x reestab %d reconfig %d\n", resp->rnti, resp->ue_reestablishment_counter, resp->ue_reconfiguration_counter);
    free(resp_ue_context_p);
  }

  return 0;
}

int fetch_rnti(char *buf, telnet_printfunc_t prnt)
{
  int rnti = -1;
  if (!buf) {
    rnti = get_single_ue_rnti_mac();
    if (rnti < 1)
      ERROR_MSG_RET("no UE found\n");
  } else {
    rnti = strtol(buf, NULL, 16);
    if (rnti < 1 || rnti >= 0xfffe)
      ERROR_MSG_RET("RNTI needs to be [1,0xfffe]\n");
  }
  return rnti;
}

int trigger_reestab(char *buf, int debug, telnet_printfunc_t prnt)
{
  UNUSED(debug);
  if (!RC.nrmac)
    ERROR_MSG_RET("no MAC/RLC present, cannot trigger reestablishment\n");
  int rnti = fetch_rnti(buf, prnt);
  if (rnti < 0)
    ERROR_MSG_RET("could not identify UE (no UE, no such RNTI, or multiple UEs)\n");
  nr_rlc_test_trigger_reestablishment(rnti);
  prnt("Reset RLC counters of UE RNTI %04x to trigger reestablishment\n", rnti);
  return 0;
}

/** @brief Get connected DU by the UE ID */
int fetch_du_by_ue_id(char *buf, int debug, telnet_printfunc_t prnt)
{
  UNUSED(debug);
  if (!RC.nrrrc)
    ERROR_MSG_RET("no RRC present, cannot list counts\n");

  ue_id_t ue_id = -1;
  if (buf) {
    ue_id = strtol(buf, NULL, 10);
  } else {
    MessageDef *msg_p = itti_alloc_new_message(TASK_RRC_GNB, 0, RRC_GET_SINGLE_UE_RNTI);
    MessageDef *resp_p;
    if (!itti_send_and_receive_msg_to_task(TASK_RRC_GNB, TASK_TELNET, msg_p, &resp_p, 1000)) {
      return false;
    }
    if(!resp_p->ittiMsg.rrc_get_single_ue_rnti.has_rrc){
      ERROR_MSG_RET("No UE connected\n");
    }
    if(!resp_p->ittiMsg.rrc_get_single_ue_rnti.is_single){
      prnt("No ID was provided and multiple UEs are present, first one in list is selected\n");
    }
    ue_id = resp_p->ittiMsg.rrc_get_single_ue_rnti.id;
    free(resp_p);
  }

  MessageDef *msg_p = itti_alloc_new_message(TASK_RRC_GNB, 0, RRC_GET_DU_ID_BY_UE_ID);
  msg_p->ittiMsg.rrc_get_du_id_by_ue_id.ue_id = ue_id;
  MessageDef *resp_p;
  if (!itti_send_and_receive_msg_to_task(TASK_RRC_GNB, TASK_TELNET, msg_p, &resp_p, 1000)) {
    ERROR_MSG_RET("Timeout waiting for RRC response\n");
  }
  if (resp_p->ittiMsg.rrc_get_du_id_by_ue_id.no_du) {
    free(resp_p);
    ERROR_MSG_RET("No DU connected or no UE found with the requested ue_id.\n");
  }
  int du_id = resp_p->ittiMsg.rrc_get_du_id_by_ue_id.du_id;
  prnt("gNB_DU_id %ld is connected to ue_id %ld\n", du_id, resp_p->ittiMsg.rrc_get_du_id_by_ue_id.ue_id);
  free(resp_p);
  return 0;
}

/**
 * @brief Trigger F1 handover for UE
 * @param buf: RRC UE ID or NULL for the first UE in list
 * @param debug: Debug flag
 * @param prnt: Print function
 * @return 0 on success, -1 on failure
 */
int rrc_gNB_trigger_f1_ho(char *buf, int debug, telnet_printfunc_t prnt)
{
  UNUSED(debug);
  ue_id_t ue_id = -1;
  if (buf) {
    ue_id = strtol(buf, NULL, 10);
  } else {
    MessageDef *msg_p = itti_alloc_new_message(TASK_RRC_GNB, 0, RRC_GET_SINGLE_UE_RNTI);
    MessageDef *resp_p;
    if (!itti_send_and_receive_msg_to_task(TASK_RRC_GNB, TASK_TELNET, msg_p, &resp_p, 1000)) {
      return false;
    }
    if(!resp_p->ittiMsg.rrc_get_single_ue_rnti.has_rrc){
      ERROR_MSG_RET("No UE connected\n");
    }
    if(!resp_p->ittiMsg.rrc_get_single_ue_rnti.is_single){
      prnt("No ID was provided and multiple UEs are present, first one in list is selected\n");
    }
    ue_id = resp_p->ittiMsg.rrc_get_single_ue_rnti.id;
    free(resp_p);
  }

  MessageDef *msg_p = itti_alloc_new_message(TASK_RRC_GNB, 0, RRC_NR_F1_HO_TRIGGER);
  msg_p->ittiMsg.rrc_trigger_f1_ho.id = ue_id;
  itti_send_msg_to_task(TASK_RRC_GNB, 0, msg_p);
  prnt("RRC F1 handover triggered for UE %u\n", ue_id);
  return 0;
}

/** @brief Trigger N2 handover for UE
 *  @param buf: Neighbour PCI, SCell PCI, RRC UE ID
 *  @param debug: Debug flag
 *  @param prnt: Print function
 *  @return 0 on success, -1 on failure */
int rrc_gNB_trigger_n2_ho(char *buf, int debug, telnet_printfunc_t prnt)
{
  UNUSED(debug);
  if (!buf) {
    ERROR_MSG_RET("Please provide neighbour cell id and ue id\n");
  } else {
    // Parse neighbour cell PCI
    char *token = strtok(buf, ",");
    if (!token) {
      ERROR_MSG_RET("Invalid input. Expected format: Neighbour PCI, ueId\n");
    }
    uint32_t neighbour_pci = strtol(token, NULL, 10);

    // Parse ueId
    token = strtok(NULL, ",");
    if (!token) {
      ERROR_MSG_RET("Missing UE ID\n");
    }
    uint32_t ueId = strtol(token, NULL, 10);

    // Retrieve UE context
    MessageDef *msg_p = itti_alloc_new_message(TASK_RRC_GNB, 0, RRC_CHECK_UE_CONTEXT);
    msg_p->ittiMsg.rrc_check_ue_context.id = ueId;
    MessageDef *resp_p;
    if (!itti_send_and_receive_msg_to_task(TASK_RRC_GNB, TASK_TELNET, msg_p, &resp_p, 1000)) {
      ERROR_MSG_RET("Timeout waiting for RRC response\n");
    }
    if (!resp_p->ittiMsg.rrc_check_ue_context.check) {
      free(resp_p);
      ERROR_MSG_RET("UE with id %u not found\n", ueId);
    }
    free(resp_p);
    // Trigger N2 handover
    MessageDef *msg_ho_p = itti_alloc_new_message(TASK_RRC_GNB, 0, RRC_NR_N2_HO_TRIGGER);
    msg_ho_p->ittiMsg.rrc_trigger_n2_ho.id = ueId;
    msg_ho_p->ittiMsg.rrc_trigger_n2_ho.neighbour_pci = neighbour_pci;
    itti_send_msg_to_task(TASK_RRC_GNB, 0, msg_ho_p);
    // Print success message
    prnt("RRC N2 handover triggered for UE %u with neighbour cell id %u\n",
         ueId,
         neighbour_pci);
  }
  return 0;
}

int force_ul_failure(char *buf, int debug, telnet_printfunc_t prnt)
{
  UNUSED(debug);
  rnti_t rnti;
  if(!buf){
    MessageDef *msg_rnti_p = itti_alloc_new_message(TASK_MAC_GNB, 0, MAC_GET_UE_RNTI);
    MessageDef *resp_rnti_p;
    if (!itti_send_and_receive_msg_to_task(TASK_MAC_GNB, TASK_TELNET, msg_rnti_p, &resp_rnti_p, 1000)) {
      ERROR_MSG_RET("Timeout waiting for MAC response\n");
    }
    rnti = resp_rnti_p->ittiMsg.mac_get_ue_rnti.rnti;
    free(resp_rnti_p);
  } else {
    char *token = strtok(buf, ",");
    if (!token) {
      ERROR_MSG_RET("Invalid input. Expected format: UE_ID\n");
    }
    uid_t ue_id = (uid_t)strtol(token, NULL, 10);

    MessageDef *msg_p = itti_alloc_new_message(TASK_MAC_GNB, 0, MAC_GET_UE_RNTI_BY_UID);
    msg_p->ittiMsg.mac_get_ue_rnti_by_uid.uid = ue_id;
    MessageDef *resp_p;
    if (!itti_send_and_receive_msg_to_task(TASK_MAC_GNB, TASK_TELNET, msg_p, &resp_p, 1000)) {
      ERROR_MSG_RET("Timeout waiting for MAC response\n");
    }

    if (resp_p->ittiMsg.mac_get_ue_rnti_by_uid.rnti == 0) {
      free(resp_p);
      ERROR_MSG_RET("Provided ID does not correspond to any UE\n");
    }

    rnti = resp_p->ittiMsg.mac_get_ue_rnti_by_uid.rnti;
    free(resp_p);
  }

  if (rnti == 0) {
    ERROR_MSG_RET("no MAC/RLC present or could not identify UE (no UE, no such RNTI, or multiple UEs)\n");
  }

  MessageDef *msg_p = itti_alloc_new_message(TASK_MAC_GNB, 0, MAC_FORCE_UL_FAILURE);
  msg_p->ittiMsg.mac_force_ul_failure.rnti = rnti;
  MessageDef *resp_p;
  if (!itti_send_and_receive_msg_to_task(TASK_MAC_GNB, TASK_TELNET, msg_p, &resp_p, 1000)) {
    ERROR_MSG_RET("Timeout waiting for MAC response\n");
  }
  free(resp_p);
  return 0;
}

int force_ue_release(char *buf, int debug, telnet_printfunc_t prnt)
{
  force_ul_failure(buf, debug, prnt);
  int rnti = fetch_rnti(buf, prnt);
  if (rnti < 0)
    ERROR_MSG_RET("could not identify UE (no UE, no such RNTI, or multiple UEs)\n");
  NR_UE_info_t *UE = find_nr_UE(&RC.nrmac[0]->UE_info, rnti);
  NR_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
  sched_ctrl->ul_failure_timer = 2;
  nr_mac_check_ul_failure(RC.nrmac[0], UE->rnti, sched_ctrl);
  return 0;
}

static int get_current_bwp(char *buf, int debug, telnet_printfunc_t prnt)
{
  UNUSED(debug);
  int rnti = fetch_rnti(buf, prnt);
  if (rnti < 0)
    ERROR_MSG_RET("could not identify UE (no UE, no such RNTI, or multiple UEs)\n");
  NR_UE_info_t *UE = find_nr_UE(&RC.nrmac[0]->UE_info, rnti);
  if (!UE)
    ERROR_MSG_RET("could not find UE with RNTI %04x\n", rnti);
  int dl_bwp = UE->current_DL_BWP.bwp_id;
  const char *dl_bwp_text = dl_bwp > 0 ? "dedicated" : "initial";
  int ul_bwp = UE->current_UL_BWP.bwp_id;
  const char *ul_bwp_text = ul_bwp > 0 ? "dedicated" : "initial";

  prnt("UE %04x DL BWP ID %d (%s) UL BWP ID %d (%s)\n", UE->rnti, dl_bwp, dl_bwp_text, ul_bwp, ul_bwp_text);
  return 0;
}

/** @brief Trigger NGAP PDU Session Release for one or more PDU sessions associated with a UE ID/
 *  Syntax: trigger_pdu_session_release [ue_id=gNB_ue_ngap_id(int,opt)],pdusession_id(int)[,pdusession_id(int)...]
 *  - If the gNB_ue_ngap_id is omitted, it is fetched from the only UE present in the RRC layer
 *  - At least one valid PDU session ID must be provided
 * @param[in] buf   Comma-separated input string: [ue_id=gNB_ue_ngap_id(int,opt)],PDU1[,PDU2,...]
 * @param[in] debug Not used.
 * @param[in] prnt  Callback for telnet output printing.
 * @return 0 on success; negative value on error. */
static int trigger_ngap_pdu_session_release(char *buf, int debug, telnet_printfunc_t prnt)
{
  UNUSED(debug);
  if (buf == NULL) {
    ERROR_MSG_RET("Missing input. Usage: trigger_pdu_session_release [ue_id=gNB_ue_ngap_id(int,opt)],pdusession_id(int)[,pdusession_id(int)...]\n");
  }

  char *tokens[NR_MAX_NB_PDU_SESSIONS + 1];
  int count = 0;

  for (char *tok = strtok(buf, ","); tok != NULL && count < (int)sizeofArray(tokens); tok = strtok(NULL, ",")) {
    tokens[count++] = tok;
  }

  if (count < 1) {
    ERROR_MSG_RET("Invalid input. Usage: trigger_pdu_session_release [ue_id=gNB_ue_ngap_id(int,opt)],pdusession_id(int)[,pdusession_id(int)...]\n");
  }

  int gNB_ue_ngap_id = -1;
  int pdu_start_index = 0;

  if (strncmp(tokens[0], "ue_id=", 6) == 0) {
    gNB_ue_ngap_id = atoi(tokens[0] + 6);
    pdu_start_index = 1;
  } else {
    MessageDef *msg_ue_rnti_p = itti_alloc_new_message(TASK_RRC_GNB, 0, RRC_GET_SINGLE_UE_RNTI);
    MessageDef *resp_ue_rnti_p;
    if (!itti_send_and_receive_msg_to_task(TASK_RRC_GNB, TASK_TELNET, msg_ue_rnti_p, &resp_ue_rnti_p, 1000)) {
      ERROR_MSG_RET("Timeout waiting for RRC response\n");
    }
    if (!check_single_rrc_ue(resp_ue_rnti_p, prnt)) {
      free(resp_ue_rnti_p);
      return -1;
    }
    gNB_ue_ngap_id = resp_ue_rnti_p->ittiMsg.rrc_get_single_ue_rnti.id;
    free(resp_ue_rnti_p);
  }

  if (pdu_start_index >= count) {
    ERROR_MSG_RET("No pdusession_id(int) provided\n");
  }

  MessageDef *msg_p = itti_alloc_new_message(TASK_RRC_GNB, 0, RRC_GET_NGAP_UE_ID);
  msg_p->ittiMsg.rrc_get_ngap_ue_id.gNB_ue_ngap_id = gNB_ue_ngap_id;
  MessageDef *resp_p;
  if (!itti_send_and_receive_msg_to_task(TASK_RRC_GNB, TASK_TELNET, msg_p, &resp_p, 1000)) {
    ERROR_MSG_RET("Timeout waiting for RRC response\n");
  }
  int amf_ue_ngap_id = resp_p->ittiMsg.rrc_get_ngap_ue_id.amf_ue_ngap_id;
  free(resp_p);
  if (!amf_ue_ngap_id) {
    ERROR_MSG_RET("No NGAP UE context for gNB_ue_ngap_id %d\n", gNB_ue_ngap_id);
  }

  MessageDef *message_p = itti_alloc_new_message(TASK_NGAP, 0, NGAP_PDUSESSION_RELEASE_COMMAND);
  ngap_pdusession_release_command_t *msg = &NGAP_PDUSESSION_RELEASE_COMMAND(message_p);
  memset(msg, 0, sizeof(*msg));

  msg->amf_ue_ngap_id = amf_ue_ngap_id;
  msg->gNB_ue_ngap_id = gNB_ue_ngap_id;

  int nb_sessions = 0;
  for (int i = pdu_start_index; i < count; ++i) {
    int sid = atoi(tokens[i]);
    if (sid < 1 || sid > 255) {
      ERROR_MSG_RET("Invalid pdusession_id(int): %s (must be between 1 and 255)\n", tokens[i]);
    }
    msg->pdusession_ids[nb_sessions++] = sid;
  }

  msg->nb_pdusessions_torelease = nb_sessions;

  if (prnt) {
    prnt("Triggering NGAP PDU Session Release for gNB_ue_ngap_id=%d: releasing pdusession_id=%d", gNB_ue_ngap_id);
    for (int i = 0; i < nb_sessions; ++i) {
      prnt(" %d,", msg->pdusession_ids[i]);
    }
    prnt("\n");
  }

  itti_send_msg_to_task(TASK_RRC_GNB, 0, message_p);
  return 0;
}

static int trigger_bwp_switch(char *buf, int debug, telnet_printfunc_t prnt)
{
  UNUSED(debug);
  char *sbwpId = strtok(buf, " ");
  int bwpId = atoi(sbwpId);
  char *srnti = strtok(NULL, " ");
  prnt("bwpId %d rnti %s\n", bwpId, srnti);
  int rnti = fetch_rnti(srnti, prnt);
  if (rnti < 0)
    ERROR_MSG_RET("could not identify UE (no UE, no such RNTI, or multiple UEs)\n");
  if (!nr_trigger_bwp_switch(rnti, bwpId)) {
    ERROR_MSG_RET("failed trigger BWP switch for UE %04x BWP ID %d\n", rnti, bwpId);
  } else {
    prnt("triggered BWP switch to BWP ID %d for UE %04x\n", bwpId, rnti);
    return 0;
  }
}

static int set_pusch_target_snr(char *buf, int debug, telnet_printfunc_t prnt)
{
  if (!buf)
    ERROR_MSG_RET("need an SNR to read\n");

  char *end;
  long new_snr = strtol(buf, &end, 0);
  if (*end != 0)
    ERROR_MSG_RET("error: could not parse number in '%s'\n", buf);

  gNB_MAC_INST *nrmac = RC.nrmac[0];
  NR_SCHED_LOCK(&nrmac->sched_lock);
  UE_iterator(nrmac->UE_info.connected_ue_list, it) {
    nr_mac_set_target_snrx10(&it->UE_sched_ctrl.pusch_pc, new_snr * 10);
  }
  NR_SCHED_UNLOCK(&nrmac->sched_lock);
  prnt("set new PUSCH target SNR %d for all UEs\n", new_snr);

  return 0;
}

static telnetshell_cmddef_t cicmds[] = {
    {"get_single_rnti", "", get_single_rnti},
    {"force_reestab", "[rnti(hex,opt)]", trigger_reestab},
    {"get_reestab_count", "[rnti(hex,opt)]", get_reestab_count},
    {"force_ue_release", "[rnti(hex,opt)]", force_ue_release},
    {"force_ul_failure", "[rnti(hex,opt)]", force_ul_failure},
    {"trigger_f1_ho", "[rrc_ue_id(int,opt)]", rrc_gNB_trigger_f1_ho},
    {"fetch_du_by_ue_id", "[rrc_ue_id(int,opt)]", fetch_du_by_ue_id},
    {"get_current_bwp", "[rnti(hex,opt)]", get_current_bwp},
    {"trigger_bwp_switch", "newBWPId [rnti(hex,opt)]", trigger_bwp_switch},
    {"trigger_n2_ho", "[neighbour_pci(uint32_t),ueId(uint32_t)]", rrc_gNB_trigger_n2_ho},
    {"set_pusch_target_snr", "[somelongSNR(dec)]", set_pusch_target_snr},
    {"pdu_session_release", "[gNB_ue_ngap_id(int,opt)]", trigger_ngap_pdu_session_release},
    {"", "", NULL},
};

static telnetshell_vardef_t civars[] = {

  {"", 0, 0, NULL}
};

void add_ci_cmds(void) {
  add_telnetcmd("ci", civars, cicmds);
}
