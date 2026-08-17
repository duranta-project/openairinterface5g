/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Implementation of telnet CI functions for nrUE
 */

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "openair2/LAYER2/NR_MAC_UE/mac_defs.h"
#include "openair2/LAYER2/NR_MAC_UE/mac_proto.h"
#include "openair2/RRC/NR_UE/rrc_proto.h"
#include "openair1/PHY/defs_nr_common.h"
#include "openair1/PHY/defs_nr_UE.h"
#include "openair3/NAS/NR_UE/nr_nas_msg.h"
#include "openair1/PHY/phy_extern_nr_ue.h"

#define TELNETSERVERCODE
#include "telnetsrv.h"

#define ERROR_MSG_RET(mSG, aRGS...) do { prnt(mSG, ##aRGS); return 1; } while (0)

/* UE L2 state string */
const char* NR_UE_L2_STATE_STR[] = {
#define UE_STATE(state) #state,
  NR_UE_L2_STATES
#undef UE_STATE
};

static int get_default_ue_id(void)
{
  NR_UE_RRC_INST_t *rrc = get_NR_UE_rrc_inst(0);
  if (!rrc)
    return -1;
  return rrc->ue_id;
}

/**
 * Get the synchronization state of a UE.
 *
 * @param buf    User input buffer containing UE ID
 * @param debug  Debug flag (not used)
 * @param prnt   Function to print output
 * @return       0 on success, error code otherwise
 */
int get_sync_state(char *buf, int debug, telnet_printfunc_t prnt)
{
  UNUSED(debug);
  int ue_id = -1;
  if (!buf) {
    ERROR_MSG_RET("no UE ID provided to telnet command\n");
  } else {
    ue_id = strtol(buf, NULL, 10);
    if (ue_id < 0)
      ERROR_MSG_RET("UE ID needs to be positive\n");
  }
  /* get sync state */
  int sync_state = nr_ue_get_sync_state(ue_id);
  prnt("UE sync state = %s\n", NR_UE_L2_STATE_STR[sync_state]);
  return 0;
}

/**
 * Force RLF on UE
 */
int force_rlf(char *buf, int debug, telnet_printfunc_t prnt)
{
  UNUSED(debug);
  UNUSED(buf);
  UNUSED(prnt);
  NR_UE_RRC_INST_t *rrc = get_NR_UE_rrc_inst(0);
  handle_rlf_detection(rrc, NR_ReestablishmentCause_otherFailure);
  return 0;
}

/**
 * Send UE to RRC_IDLE
 */
int force_RRC_IDLE(char *buf, int debug, telnet_printfunc_t prnt)
{
  UNUSED(debug);
  UNUSED(buf);
  UNUSED(prnt);
  NR_UE_RRC_INST_t *rrc = get_NR_UE_rrc_inst(0);
  nr_rrc_going_to_IDLE(rrc, OTHER, NULL);
  return 0;
}

/** @brief Trigger RA with Msg3 C-RNTI */
int force_crnti_ra(char *buf, int debug, telnet_printfunc_t prnt)
{
  UNUSED(debug);
  UNUSED(buf);
  UNUSED(prnt);
  NR_UE_MAC_INST_t *mac = get_mac_inst(0);
  trigger_MAC_UE_RA(mac, NULL);
  return 0;
}

static int force_deregistration(char *buf, int debug, telnet_printfunc_t prnt)
{
  UNUSED(debug);
  UNUSED(buf);
  UNUSED(prnt);
  MessageDef *msg = itti_alloc_new_message(TASK_NAS_NRUE, 0, NAS_DEREGISTRATION_REQ);
  NAS_DEREGISTRATION_REQ(msg).cause = AS_DETACH;
  itti_send_msg_to_task(TASK_NAS_NRUE, 0, msg);
  return 0;
}

extern float get_prs_max_dl_toa(prs_meas_t *prs_meas);
static int get_dl_toa(char *buf, int debug, telnet_printfunc_t prnt)
{
  UNUSED(debug);
  UNUSED(buf);
  // TODO multiple antennas, resources, gNBs?
  int gNB_id = 0;
  int rsc_id = 0;
  int ant = 0;

  PHY_VARS_NR_UE *UE = nrPHY_vars_UE_g[0][0];
  if (!UE || !UE->prs_vars[gNB_id])
    ERROR_MSG_RET("no UE/prs_vars found!\n");
  NR_PRS_RESOURCE_t *prs_res = &UE->prs_vars[gNB_id]->prs_resource[rsc_id];
  if (!prs_res->prs_meas || !prs_res->prs_meas[ant])
    ERROR_MSG_RET("prs_meas not initialized!\n");

  float max = get_prs_max_dl_toa(prs_res->prs_meas[ant]);
  prnt("UE max PRS DL ToA %.3f\n", max);
  return 0;
}

static int add_pdu_session(char *buf, int debug, telnet_printfunc_t prnt)
{
  UNUSED(debug);
  int ue_id = -1;
  int pdusession_id = -1;

  if (!buf) {
    ERROR_MSG_RET("Missing argument: expected PDUSessionID[,UE_ID]\n");
  }

  // Try parsing values in the form: "PDUSessionID[,UE_ID]"
  int n = sscanf(buf, "%d,%d", &pdusession_id, &ue_id);
  if (n == 1) {
    // Only PDUSessionID provided: use default UE ID
    ue_id = get_default_ue_id();
    if (ue_id < 0)
      ERROR_MSG_RET("No default UE context found\n");
  } else if (n != 2) {
    ERROR_MSG_RET("Invalid format: expected PDUSessionID[,UE_ID]\n");
  }

  if (pdusession_id < 0 || pdusession_id > 255)
    ERROR_MSG_RET("PDUSessionID must be in range [0,255]\n");
  if (ue_id < 0)
    ERROR_MSG_RET("UE_ID must be >= 0\n");

  nr_ue_nas_t *nas = get_ue_nas_info(ue_id);
  if (!nas)
    ERROR_MSG_RET("No NAS context found for UE_ID %d\n", ue_id);

  DevAssert(nas->uicc);
  nssai_t nssai = {nas->uicc->nssai_sst, nas->uicc->nssai_sd};
  pdu_session_config_t c = {pdusession_id, 1 /* = PDU_SESSION_TYPE_IPV4 */, nssai, nas->uicc->dnnStr};
  nas->uicc->pdu_sessions[nas->uicc->n_pdu_sessions++] = c;
  request_pdusession(nas, &c);
  prnt("Triggered PDU session request for UE %d with ID %d\n", ue_id, pdusession_id);
  return 0;
}

/* Telnet shell command definitions */
static telnetshell_cmddef_t cicmds[] = {
  {"sync_state", "[UE_ID(int,opt)]", get_sync_state},
  {"force_rlf", "", force_rlf},
  {"force_RRC_IDLE", "", force_RRC_IDLE},
  {"force_crnti_ra", "", force_crnti_ra},
  {"deregistration", "", force_deregistration},
  {"get_max_dl_toa", "[ant]", get_dl_toa},
  {"add_pdu_session", "[PDUSessionID(int)],[UE_ID(int,opt)]", add_pdu_session},
  {"", "", NULL},
};

/* Telnet shell variable definitions (if needed) */
static telnetshell_vardef_t civars[] = {
  {"", 0, 0, NULL}
};

/* Add CI UE commands */
void add_ciUE_cmds(void) {
  add_telnetcmd("ciUE", civars, cicmds);
}

