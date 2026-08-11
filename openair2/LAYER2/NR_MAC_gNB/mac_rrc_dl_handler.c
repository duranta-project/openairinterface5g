/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

#include "mac_rrc_dl_handler.h"

#include "mac_proto.h"
#include "nr_radio_config.h"
#include "openair2/F1AP/f1ap_ids.h"
#include "openair2/F1AP/f1ap_common.h"
#include "openair2/LAYER2/nr_rlc/nr_rlc_oai_api.h"
#include "F1AP_CauseRadioNetwork.h"
#include "NR_HandoverPreparationInformation.h"
#include "NR_CG-ConfigInfo.h"
#include "openair3/ocp-gtpu/gtp_itf.h"
#include "openair2/LAYER2/nr_pdcp/nr_pdcp_oai_api.h"
#include "lib/f1ap_rrc_message_transfer.h"
#include "lib/f1ap_interface_management.h"
#include "lib/f1ap_ue_context.h"

#include "executables/softmodem-common.h"

#include "uper_decoder.h"
#include "uper_encoder.h"

// Standarized 5QI values and Default Priority levels as mentioned in 3GPP TS 23.501 Table 5.7.4-1
const uint64_t qos_fiveqi[26] = {1, 2, 3, 4, 65, 66, 67, 71, 72, 73, 74, 76, 5, 6, 7, 8, 9, 69, 70, 79, 80, 82, 83, 84, 85, 86};
const uint64_t qos_priority[26] = {20, 40, 30, 50, 7, 20, 15, 56, 56, 56, 56, 56, 10,
                                   60, 70, 80, 90, 5, 55, 65, 68, 19, 22, 24, 21, 18};

static instance_t get_f1_gtp_instance(void)
{
  const f1ap_cudu_inst_t *inst = getCxt(0);
  if (!inst)
    return -1; // means no F1
  return inst->gtpInst;
}

static int drb_gtpu_create(instance_t instance,
                           uint32_t ue_id,
                           int incoming_id,
                           int outgoing_id,
                           int qfi,
                           in_addr_t tlAddress, // only IPv4 now
                           teid_t outgoing_teid,
                           gtpCallback callBack,
                           gtpCallbackSDAP callBackSDAP,
                           gtpv1u_gnb_create_tunnel_resp_t *create_tunnel_resp)
{
  gtpv1u_gnb_create_tunnel_req_t create_tunnel_req = {0};
  create_tunnel_req.incoming_rb_id[0] = incoming_id;
  create_tunnel_req.pdusession_id[0] = outgoing_id;
  memcpy(&create_tunnel_req.dst_addr[0].buffer, &tlAddress, sizeof(uint8_t) * 4);
  create_tunnel_req.dst_addr[0].length = 32;
  create_tunnel_req.outgoing_teid[0] = outgoing_teid;
  create_tunnel_req.outgoing_qfi[0] = qfi;
  create_tunnel_req.num_tunnels = 1;
  create_tunnel_req.ue_id = ue_id;

  // we use gtpv1u_create_ngu_tunnel because it returns the interface
  // address and port of the interface; apart from that, we also might call
  // newGtpuCreateTunnel() directly
  return gtpv1u_create_ngu_tunnel(instance, &create_tunnel_req, create_tunnel_resp, callBack, callBackSDAP);
}

bool DURecvCb(protocol_ctxt_t *ctxt_pP,
              const srb_flag_t srb_flagP,
              const rb_id_t rb_idP,
              const mui_t muiP,
              const confirm_t confirmP,
              const sdu_size_t sdu_buffer_sizeP,
              unsigned char *const sdu_buffer_pP,
              const pdcp_transmission_mode_t modeP,
              const uint32_t *sourceL2Id,
              const uint32_t *destinationL2Id)
{
  // The buffer comes from the stack in gtp-u thread, we have a make a separate buffer to enqueue in a inter-thread message queue
  uint8_t *sdu = malloc16(sdu_buffer_sizeP);
  memcpy(sdu, sdu_buffer_pP, sdu_buffer_sizeP);
  nr_rlc_data_req(ctxt_pP, srb_flagP, rb_idP, muiP, sdu_buffer_sizeP, sdu);
  return true;
}

static bool check_plmn_identity(const plmn_id_t *check_plmn, const plmn_id_t *plmn)
{
  return plmn->mcc == check_plmn->mcc && plmn->mnc_digit_length == check_plmn->mnc_digit_length && plmn->mnc == check_plmn->mnc;
}

/* not static, so we can call it from the outside (in telnet) */
void du_clear_all_ue_states()
{
  gNB_MAC_INST *mac = RC.nrmac[0];
  NR_SCHED_LOCK(&mac->sched_lock);

  NR_UE_info_t *UE = *mac->UE_info.connected_ue_list;

  instance_t f1inst = get_f1_gtp_instance();

  while (UE != NULL) {
    int rnti = UE->rnti;
    nr_mac_release_ue(mac, rnti);
    // free all F1 contexts
    if (du_exists_f1_ue_data(rnti))
      du_remove_f1_ue_data(rnti);
    newGtpuDeleteAllTunnels(f1inst, rnti);
    UE = *mac->UE_info.connected_ue_list;
  }
  NR_SCHED_UNLOCK(&mac->sched_lock);
}

void f1_reset_cu_initiated(const f1ap_reset_t *reset)
{
  LOG_I(MAC, "F1 Reset initiated by CU\n");

  f1ap_reset_ack_t ack = {.transaction_id = reset->transaction_id};
  if(reset->reset_type == F1AP_RESET_ALL) {
    du_clear_all_ue_states();
  } else {
    // reset->reset_type == F1AP_RESET_PART_OF_F1_INTERFACE
    AssertFatal(1==0, "Not implemented yet\n");
  }

  gNB_MAC_INST *mac = RC.nrmac[0];
  mac->mac_rrc.f1_reset_acknowledge(&ack);
}

void f1_reset_acknowledge_du_initiated(const f1ap_reset_ack_t *ack)
{
  (void) ack;
  AssertFatal(false, "%s() not implemented yet\n", __func__);
}

void f1_setup_response(const f1ap_setup_resp_t *resp)
{
  LOG_I(MAC, "received F1 Setup Response from CU %s\n", resp->gNB_CU_name);
  LOG_I(MAC, "CU uses RRC version %d.%d.%d\n", resp->rrc_ver[0], resp->rrc_ver[1], resp->rrc_ver[2]);

  if (resp->num_cells_to_activate == 0) {
    LOG_W(NR_MAC, "no cell to activate: cell remains blocked\n");
    return;
  }

  gNB_MAC_INST *mac = RC.nrmac[0];
  NR_SCHED_LOCK(&mac->sched_lock);
  const f1ap_setup_req_t *setup_req = mac->f1_config.setup_req;
  const f1ap_served_cell_info_t *du_cell = &setup_req->cell[0].info;

  AssertFatal(resp->num_cells_to_activate == 1, "can only handle one cell, but %d activated\n", resp->num_cells_to_activate);
  const served_cells_to_activate_t *cu_cell = &resp->cells_to_activate[0];

  AssertFatal(du_cell->nr_cellid  == cu_cell->nr_cellid, "CellID mismatch: DU %ld vs CU %ld\n", du_cell->nr_cellid, cu_cell->nr_cellid);
  AssertFatal(check_plmn_identity(&du_cell->plmn, &cu_cell->plmn), "PLMN mismatch\n");
  AssertFatal(du_cell->nr_pci == cu_cell->nrpci, "PCI mismatch: DU %d vs CU %d\n", du_cell->nr_pci, cu_cell->nrpci);

  // we can configure other SIB only after having received the ones generated by CU
  bool update = nr_mac_configure_other_sib(mac, cu_cell->num_SI, cu_cell->SI_msg);
  /* only if we have to update SIB1, set mib and sib1 to non-NULL to indicate
   * in du_config_update that it has changed (otherwise, gNB-DU config update
   * indicates only cell status */
  NR_BCCH_BCH_Message_t *mib = NULL;
  NR_BCCH_DL_SCH_Message_t *sib1 = NULL;
  if (update) {
    NR_COMMON_channels_t *cc = &mac->common_channels[0];
    mib = cc->mib;
    sib1 = cc->sib1;
  }

  mac->f1_config.setup_resp = malloc(sizeof(*mac->f1_config.setup_resp));
  AssertFatal(mac->f1_config.setup_resp != NULL, "out of memory\n");
  // Copy F1AP message
  *mac->f1_config.setup_resp = cp_f1ap_setup_response(resp);

  f1ap_setup_req_t *sr = mac->f1_config.setup_req;
  DevAssert(sr->num_cells_available == 1);
  prepare_du_configuration_update(mac, &sr->cell[0].info, mib, sib1);
  NR_SCHED_UNLOCK(&mac->sched_lock);

  // NOTE: Before accepting any UEs, we should initialize the UE states.
  // This is to handle cases when DU loses the existing SCTP connection,
  // and reestablishes a new connection to either a new CU or the same CU.
  // This triggers a new F1 Setup Request from DU to CU as per the specs.
  // Reinitializing the UE states is necessary to avoid any inconsistent states
  // between DU and CU.
  // NOTE2: do not reset in phy_test, because there is a pre-configured UE in
  // this case. Once NSA/phy-test use F1, this might be lifted, because
  // creation of a UE will be requested from higher layers.

  // TS38.473 [Sec 8.2.3.1]: "This procedure also re-initialises the F1AP UE-related
  // contexts (if any) and erases all related signalling connections
  // in the two nodes like a Reset procedure would do."
  if (!get_softmodem_params()->phy_test) {
    LOG_I(MAC, "Clearing the DU's UE states before, if any.\n");
    du_clear_all_ue_states();
  }
}

void f1_setup_failure(const f1ap_setup_failure_t *failure)
{
  LOG_E(MAC, "the CU reported F1AP Setup Failure, is there a configuration mismatch?\n");
  exit(1);
}

void gnb_du_configuration_update_acknowledge(const f1ap_gnb_du_configuration_update_acknowledge_t *ack)
{
  (void)ack;
  LOG_I(MAC, "received gNB-DU configuration update acknowledge\n");
}

static NR_RLC_BearerConfig_t *get_bearerconfig_from_srb(const f1ap_srb_to_setup_t *srb,
                                                        const nr_rlc_configuration_t *rlc_config)
{
  long priority = srb->id == 2 ? 3 : 1; // see 38.331 sec 9.2.1
  e_NR_LogicalChannelConfig__ul_SpecificParameters__bucketSizeDuration bucket =
      NR_LogicalChannelConfig__ul_SpecificParameters__bucketSizeDuration_ms5;
  return get_SRB_RLC_BearerConfig(get_lcid_from_srbid(srb->id), priority, bucket, rlc_config);
}

static int handle_ue_context_srbs_setup(NR_UE_info_t *UE,
                                        int srbs_len,
                                        const f1ap_srb_to_setup_t *req_srbs,
                                        f1ap_srb_setup_t **resp_srbs,
                                        NR_CellGroupConfig_t *cellGroupConfig,
                                        const nr_rlc_configuration_t *rlc_config)
{
  DevAssert(req_srbs != NULL && resp_srbs != NULL && cellGroupConfig != NULL);

  *resp_srbs = calloc(srbs_len, sizeof(**resp_srbs));
  AssertFatal(*resp_srbs != NULL, "out of memory\n");
  for (int i = 0; i < srbs_len; i++) {
    const f1ap_srb_to_setup_t *srb = &req_srbs[i];
    NR_RLC_BearerConfig_t *rlc_BearerConfig = get_bearerconfig_from_srb(srb, rlc_config);
    nr_rlc_add_srb(UE->rnti, srb->id, rlc_BearerConfig);

    int priority = rlc_BearerConfig->mac_LogicalChannelConfig->ul_SpecificParameters->priority;
    nr_lc_config_t c = {.lcid = rlc_BearerConfig->logicalChannelIdentity, .priority = priority};
    nr_mac_add_lcid(&UE->UE_sched_ctrl, &c);

    (*resp_srbs)[i].id = srb->id;
    (*resp_srbs)[i].lcid = c.lcid;

    if (rlc_BearerConfig->logicalChannelIdentity == 1) {
      // CU asks to add SRB1: when creating a cellGroupConfig, we always add it
      // (see get_initial_cellGroupConfig())
      const struct NR_CellGroupConfig__rlc_BearerToAddModList *addmod = cellGroupConfig->rlc_BearerToAddModList;
      DevAssert(addmod->list.count >= 1 && addmod->list.array[0]->logicalChannelIdentity == 1);
      ASN_STRUCT_FREE(asn_DEF_NR_RLC_BearerConfig, rlc_BearerConfig);
    } else {
      int ret = ASN_SEQUENCE_ADD(&cellGroupConfig->rlc_BearerToAddModList->list, rlc_BearerConfig);
      DevAssert(ret == 0);
    }
  }
  return srbs_len;
}

static NR_RLC_BearerConfig_t *get_bearerconfig_from_drb(const f1ap_drb_to_setup_t *drb,
                                                        const nr_rlc_configuration_t *rlc_config)
{
  const NR_RLC_Config_PR rlc_conf = drb->rlc_mode == F1AP_RLC_MODE_AM ? NR_RLC_Config_PR_am : NR_RLC_Config_PR_um_Bi_Directional;
  long priority = 13; // hardcoded for the moment
  return get_DRB_RLC_BearerConfig(get_lcid_from_drbid(drb->id), drb->id, rlc_conf, priority, rlc_config);
}

static int get_non_dynamic_priority(int fiveqi)
{
  for (int i = 0; i < sizeofArray(qos_fiveqi); ++i)
    if (qos_fiveqi[i] == fiveqi)
      return qos_priority[i];
  AssertFatal(false, "illegal 5QI value %d\n", fiveqi);
  return 0;
}

static NR_QoS_config_t get_qos_config(const f1ap_qos_flow_param_t *qos)
{
  NR_QoS_config_t qos_c = {0};
  switch (qos->qos_type) {
    case DYNAMIC:
      qos_c.priority = qos->dyn.prio;
      qos_c.fiveQI = 0; // does not exist for non-dynamic
      break;
    case NON_DYNAMIC:
      qos_c.fiveQI = qos->nondyn.fiveQI;
      qos_c.priority = get_non_dynamic_priority(qos_c.fiveQI);
      break;
    default:
      AssertFatal(false, "illegal QoS type %d\n", qos->qos_type);
      break;
  }
  return qos_c;
}

static int handle_ue_context_drbs_setup(NR_UE_info_t *UE,
                                        int drbs_len,
                                        const f1ap_drb_to_setup_t *req_drbs,
                                        f1ap_drb_setup_t **resp_drbs,
                                        NR_CellGroupConfig_t *cellGroupConfig,
                                        const nr_rlc_configuration_t *rlc_config)
{
  DevAssert(req_drbs != NULL && resp_drbs != NULL && cellGroupConfig != NULL);
  instance_t f1inst = get_f1_gtp_instance();

  /* Note: the actual GTP tunnels are created in the F1AP breanch of
   * ue_context_*_response() */
  *resp_drbs = calloc(drbs_len, sizeof(**resp_drbs));
  AssertFatal(*resp_drbs != NULL, "out of memory\n");
  for (int i = 0; i < drbs_len; i++) {
    const f1ap_drb_to_setup_t *drb = &req_drbs[i];
    AssertFatal(drb->qos_choice == F1AP_QOS_CHOICE_NR, "only NR QoS supported\n");
    f1ap_drb_setup_t *resp_drb = &(*resp_drbs)[i];
    NR_RLC_BearerConfig_t *rlc_BearerConfig = get_bearerconfig_from_drb(drb, rlc_config);
    if (UE->capability && UE->capability->rlc_Parameters && UE->capability->rlc_Parameters->ext2
        && UE->capability->rlc_Parameters->ext2->am_WithLongSN_RedCap_r17 == NULL) {
      *rlc_BearerConfig->rlc_Config->choice.am->dl_AM_RLC.sn_FieldLength = NR_SN_FieldLengthAM_size12;
      *rlc_BearerConfig->rlc_Config->choice.am->ul_AM_RLC.sn_FieldLength = NR_SN_FieldLengthAM_size12;
    }
    AssertFatal(rlc_BearerConfig->rlc_Config, "We expect rlc-Config to be always present when we configure a DRB\n");
    nr_rlc_add_drb(UE->rnti, drb->id, rlc_BearerConfig);

    nr_lc_config_t c = {.lcid = rlc_BearerConfig->logicalChannelIdentity, .nssai = drb->nr.nssai};
    int prio = 100;
    for (int q = 0; q < drb->nr.flows_len; ++q) {
      c.qos_config[q] = get_qos_config(&drb->nr.flows[q].param);
      prio = min(prio, c.qos_config[q].priority);
    }
    c.priority = prio;
    nr_mac_add_lcid(&UE->UE_sched_ctrl, &c);

    resp_drb->id = drb->id;
    resp_drb->lcid = malloc_or_fail(sizeof(*resp_drb->lcid));
    *resp_drb->lcid = c.lcid;
    // just put same number of tunnels in DL as in UL
    DevAssert(drb->up_ul_tnl_len == 1);
    resp_drb->up_dl_tnl_len = drb->up_ul_tnl_len;

    if (f1inst >= 0) { // we actually use F1-U
      int qfi = -1; // don't put PDU session marker in GTP
      gtpv1u_gnb_create_tunnel_resp_t resp_f1 = {0};
      int ret = drb_gtpu_create(f1inst,
                                UE->rnti,
                                drb->id,
                                drb->id,
                                qfi,
                                drb->up_ul_tnl[0].tl_address,
                                drb->up_ul_tnl[0].teid,
                                DURecvCb,
                                NULL,
                                &resp_f1);
      AssertFatal(ret >= 0, "Unable to create GTP Tunnel for F1-U\n");
      memcpy(&resp_drb->up_dl_tnl[0].tl_address, &resp_f1.gnb_addr.buffer, 4);
      resp_drb->up_dl_tnl[0].teid = resp_f1.gnb_NGu_teid[0];
    }

    if (!cellGroupConfig->rlc_BearerToAddModList)
      cellGroupConfig->rlc_BearerToAddModList = calloc_or_fail(1, sizeof(*cellGroupConfig->rlc_BearerToAddModList));
    int ret = ASN_SEQUENCE_ADD(&cellGroupConfig->rlc_BearerToAddModList->list, rlc_BearerConfig);
    DevAssert(ret == 0);
  }
  return drbs_len;
}

static int handle_ue_context_drbs_release(NR_UE_info_t *UE,
                                          int drbs_len,
                                          const f1ap_drb_to_release_t *req_drbs,
                                          NR_CellGroupConfig_t *cellGroupConfig)
{
  DevAssert(req_drbs != NULL && cellGroupConfig != NULL);
  instance_t f1inst = get_f1_gtp_instance();
  cellGroupConfig->rlc_BearerToReleaseList = calloc(1, sizeof(*cellGroupConfig->rlc_BearerToReleaseList));
  AssertFatal(cellGroupConfig->rlc_BearerToReleaseList != NULL, "out of memory\n");

  for (int i = 0; i < drbs_len; i++) {
    const f1ap_drb_to_release_t *drb = &req_drbs[i];

    long lcid = get_lcid_from_drbid(drb->id);
    int idx = 0;
    while (idx < cellGroupConfig->rlc_BearerToAddModList->list.count) {
      const NR_RLC_BearerConfig_t *bc = cellGroupConfig->rlc_BearerToAddModList->list.array[idx];
      if (bc->logicalChannelIdentity == lcid)
        break;
      ++idx;
    }
    if (idx < cellGroupConfig->rlc_BearerToAddModList->list.count) {
      nr_mac_remove_lcid(&UE->UE_sched_ctrl, lcid);
      nr_rlc_release_entity(UE->rnti, lcid);
      if (f1inst >= 0) /* Delete F1 tunnel */
        newGtpuDeleteOneTunnel(f1inst, UE->rnti, drb->id);
      asn_sequence_del(&cellGroupConfig->rlc_BearerToAddModList->list, idx, 1);
      long *plcid = malloc(sizeof(*plcid));
      AssertFatal(plcid, "out of memory\n");
      *plcid = lcid;
      int ret = ASN_SEQUENCE_ADD(&cellGroupConfig->rlc_BearerToReleaseList->list, plcid);
      DevAssert(ret == 0);
    }
  }
  return drbs_len;
}

static NR_UE_NR_Capability_t *get_nr_cap(const NR_UE_CapabilityRAT_ContainerList_t *clist)
{
  for (int i = 0; i < clist->list.count; i++) {
    const NR_UE_CapabilityRAT_Container_t *c = clist->list.array[i];
    if (c->rat_Type != NR_RAT_Type_nr) {
      LOG_W(NR_MAC, "ignoring capability of type %ld\n", c->rat_Type);
      continue;
    }

    NR_UE_NR_Capability_t *cap = NULL;
    asn_dec_rval_t dec_rval = uper_decode(NULL,
                                          &asn_DEF_NR_UE_NR_Capability,
                                          (void **)&cap,
                                          c->ue_CapabilityRAT_Container.buf,
                                          c->ue_CapabilityRAT_Container.size,
                                          0,
                                          0);
    if (dec_rval.code != RC_OK) {
      LOG_W(NR_MAC, "cannot decode NR UE capability, ignoring\n");
      ASN_STRUCT_FREE(asn_DEF_NR_UE_NR_Capability, cap);
      continue;
    }
    return cap;
  }
  return NULL;
}

static NR_UE_NR_Capability_t *get_ue_nr_cap(int rnti, uint8_t *buf, uint32_t len)
{
  if (buf == NULL || len == 0)
    return NULL;

  NR_UE_CapabilityRAT_ContainerList_t *clist = NULL;
  asn_dec_rval_t dec_rval = uper_decode(NULL, &asn_DEF_NR_UE_CapabilityRAT_ContainerList, (void **)&clist, buf, len, 0, 0);
  if (dec_rval.code != RC_OK) {
    LOG_W(NR_MAC, "cannot decode UE capability container list of UE RNTI %04x, ignoring capabilities\n", rnti);
    return NULL;
  }

  NR_UE_NR_Capability_t *cap = get_nr_cap(clist);
  ASN_STRUCT_FREE(asn_DEF_NR_UE_CapabilityRAT_ContainerList, clist);
  return cap;
}

/* \brief return UE capabilties from HandoverPreparationInformation.
 *
 * The HandoverPreparationInformation contains more, but for the moment, let's
 * keep it simple and only handle that. The function asserts if other IEs are
 * present. */
static NR_UE_NR_Capability_t *get_ue_nr_cap_from_ho_prep_info(uint8_t *buf, uint32_t len)
{
  if (buf == NULL || len == 0)
    return NULL;
  NR_HandoverPreparationInformation_t *hpi = NULL;
  asn_dec_rval_t dec_rval = uper_decode_complete(NULL, &asn_DEF_NR_HandoverPreparationInformation, (void **)&hpi, buf, len);
  if (dec_rval.code != RC_OK) {
    LOG_W(NR_MAC, "cannot decode HandoverPreparationInformation, ignoring capabilities\n");
    return NULL;
  }
  NR_UE_NR_Capability_t *cap = NULL;
  if (hpi->criticalExtensions.present != NR_HandoverPreparationInformation__criticalExtensions_PR_c1
      || hpi->criticalExtensions.choice.c1 == NULL
      || hpi->criticalExtensions.choice.c1->present
             != NR_HandoverPreparationInformation__criticalExtensions__c1_PR_handoverPreparationInformation
      || hpi->criticalExtensions.choice.c1->choice.handoverPreparationInformation == NULL) {
  } else {
    const NR_HandoverPreparationInformation_IEs_t *hpi_ie = hpi->criticalExtensions.choice.c1->choice.handoverPreparationInformation;
    cap = get_nr_cap(&hpi_ie->ue_CapabilityRAT_List);
  }
  ASN_STRUCT_FREE(asn_DEF_NR_HandoverPreparationInformation, hpi);
  return cap;
}

static NR_CG_ConfigInfo_t *get_cg_config_info(uint8_t *buf, uint32_t len)
{
  struct NR_CG_ConfigInfo *cg_configinfo = NULL;
  asn_dec_rval_t dec_rval = uper_decode_complete(NULL, &asn_DEF_NR_CG_ConfigInfo, (void **)&cg_configinfo, buf, len);
  if (dec_rval.code != RC_OK) {
    LOG_W(NR_MAC, "cannot decode CG-ConfigInfo, ignoring it\n");
    return NULL;
  }
  //xer_fprint(stdout, &asn_DEF_NR_CG_ConfigInfo, cg_configinfo);
  return cg_configinfo;
}

static NR_UE_NR_Capability_t *get_ue_nr_cap_from_cg_config_info(const NR_CG_ConfigInfo_t *cgci)
{
  /* INTO DU handler */
  if (cgci->criticalExtensions.present != NR_CG_ConfigInfo__criticalExtensions_PR_c1)
    return NULL;
  if (!cgci->criticalExtensions.choice.c1
      || cgci->criticalExtensions.choice.c1->present != NR_CG_ConfigInfo__criticalExtensions__c1_PR_cg_ConfigInfo)
    return NULL;

  const NR_CG_ConfigInfo_IEs_t *cgci_ie = cgci->criticalExtensions.choice.c1->choice.cg_ConfigInfo;
  if (!cgci_ie->ue_CapabilityInfo)
    return NULL;

  // Decode UE-CapabilityRAT-ContainerList
  const OCTET_STRING_t *cap_buf = cgci_ie->ue_CapabilityInfo;
  NR_UE_CapabilityRAT_ContainerList_t *clist = NULL;
  asn_dec_rval_t dec_rval = uper_decode_complete(NULL,
                                        &asn_DEF_NR_UE_CapabilityRAT_ContainerList,
                                        (void **)&clist,
                                        cap_buf->buf,
                                        cap_buf->size);

  if (dec_rval.code != RC_OK) {
    LOG_W(NR_MAC,
          "Failed to decode NR_UE_CapabilityRAT_ContainerList (%zu bits), size of OCTET_STRING %lu\n",
          dec_rval.consumed,
          cap_buf->size);
    return NULL;
  }
  NR_UE_NR_Capability_t *cap = get_nr_cap(clist);
  ASN_STRUCT_FREE(asn_DEF_NR_UE_CapabilityRAT_ContainerList, clist);
  return cap;
}

NR_CellGroupConfig_t *clone_CellGroupConfig(const NR_CellGroupConfig_t *orig)
{
  uint8_t buf[16636];
  asn_enc_rval_t enc_rval = uper_encode_to_buffer(&asn_DEF_NR_CellGroupConfig, NULL, orig, buf, sizeof(buf));
  AssertFatal(enc_rval.encoded > 0, "could not clone CellGroupConfig: problem while encoding\n");
  NR_CellGroupConfig_t *cloned = NULL;
  asn_dec_rval_t dec_rval = uper_decode(NULL, &asn_DEF_NR_CellGroupConfig, (void **)&cloned, buf, enc_rval.encoded, 0, 0);
  AssertFatal(dec_rval.code == RC_OK && dec_rval.consumed == enc_rval.encoded,
              "could not clone CellGroupConfig: problem while decodung\n");
  return cloned;
}

static NR_UE_info_t *create_new_UE(gNB_MAC_INST *mac, uint32_t cu_id, const NR_CG_ConfigInfo_t *cgci)
{
  const bool is_SA = IS_SA_MODE(get_softmodem_params());
  int CC_id = 0;
  rnti_t rnti;
  if (get_softmodem_params()->phy_test) {
    AssertFatal(mac->UE_info.connected_ue_list[0] == NULL, "phytest: UE already present\n");
    rnti = 0x1234;
  } else {
    bool found = nr_mac_get_new_rnti(&mac->UE_info, &rnti);
    if (!found)
      return NULL;
  }

  f1_ue_data_t new_ue_data = {.secondary_ue = cu_id};
  bool success = du_add_f1_ue_data(rnti, &new_ue_data);
  DevAssert(success);

  NR_UE_info_t *UE = get_new_nr_ue_inst(&mac->UE_info.uid_allocator, rnti, NULL);
  AssertFatal(UE->uid < MAX_MOBILES_PER_GNB, "cannot create UE context, UE context setup failure not implemented\n");

  NR_CellGroupConfig_t *cellGroupConfig = NULL;
  NR_COMMON_channels_t *cc = &mac->common_channels[CC_id];
  const NR_ServingCellConfigCommon_t *scc = cc->ServingCellConfigCommon;
  const nr_mac_config_t *configuration = &mac->radio_config;
  if (is_SA) {
    cellGroupConfig = get_initial_cellGroupConfig(UE->uid, scc, &mac->radio_config, &mac->rlc_config);
    cellGroupConfig->spCellConfig->reconfigurationWithSync = get_reconfiguration_with_sync(UE->rnti, UE->uid, scc, mac->frame);
  } else {
    NR_UE_NR_Capability_t *cap = get_ue_nr_cap_from_cg_config_info(cgci);
    cellGroupConfig = get_default_secondaryCellGroup(scc, cap, 1, 1, configuration, UE->uid);
    cellGroupConfig->spCellConfig->reconfigurationWithSync = get_reconfiguration_with_sync(UE->rnti, UE->uid, scc, mac->frame);
    // TODO: in NSA we assign capabilities here, otherwise outside => not logic
    UE->capability = cap;
    UE->local_bwp_id = 1; // get_default_secondaryCellGroup sets 1st active BWP as 1
  }
  // note: we don't pass the cellGroupConfig to add_new_nr_ue() because we need
  // the uid to create the CellGroupConfig (which is in the UE context created
  // by add_new_nr_ue(); it's a kind of chicken-and-egg problem), so below we
  // complete the UE context with the information that add_new_nr_ue() would
  // have added
  AssertFatal(cellGroupConfig != NULL, "out of memory\n");
  UE->CellGroup = cellGroupConfig;

  if (get_softmodem_params()->phy_test) {
    // phytest mode: we don't set up RA, etc
    free_and_zero(UE->ra); // test-mode: UE will not do RA
    bool res = add_connected_nr_ue(mac, UE);
    DevAssert(res);
  } else {
    if (!add_new_UE_RA(mac, UE)) {
      delete_nr_ue_data(UE, /*not used*/ NULL, &mac->UE_info.uid_allocator);
      LOG_E(NR_MAC, "UE list full while creating new UE\n");
      return NULL;
    }
    nr_mac_prepare_ra_ue(mac, UE);

    if (is_SA) {
      /* SRB1 is added to RLC and MAC in the handler later */
      nr_rlc_activate_srb0(UE->rnti, UE, NULL);
    }
  }
  return UE;
}

void ue_context_setup_request(const f1ap_ue_context_setup_req_t *req)
{
  const bool is_SA = IS_SA_MODE(get_softmodem_params());
  gNB_MAC_INST *mac = RC.nrmac[0];

  f1ap_ue_context_setup_resp_t resp = {
    .gNB_CU_ue_id = req->gNB_CU_ue_id,
  };

  bool ue_id_provided = req->gNB_DU_ue_id != NULL;

  const f1ap_cu_to_du_rrc_info_t *cu2du = &req->cu_to_du_rrc_info;
  NR_CG_ConfigInfo_t *cg_configinfo = NULL;
  if (cu2du->cg_configinfo != NULL)
    cg_configinfo = get_cg_config_info(cu2du->cg_configinfo->buf, cu2du->cg_configinfo->len);
  NR_UE_NR_Capability_t *ue_cap = NULL;
  if (cu2du->ho_prep_info != NULL) {
    ue_cap = get_ue_nr_cap_from_ho_prep_info(cu2du->ho_prep_info->buf, cu2du->ho_prep_info->len);
  } else if (cu2du->ue_cap != NULL) {
    ue_cap = get_ue_nr_cap(*req->gNB_DU_ue_id, cu2du->ue_cap->buf, cu2du->ue_cap->len);
  }
  NR_MeasurementTimingConfiguration_t *mtc = NULL;
  if (cu2du->meas_timing_config != NULL)
    mtc = get_nr_mtc(cu2du->meas_timing_config->buf, cu2du->meas_timing_config->len);

  /* 38.473: "For DC operation, the CG-ConfigInfo IE shall be included in the CU
   * to DU RRC Information IE at the gNB acting as secondary node" As of now,
   * we only handle NSA => we check we have CG-ConfigInfo if not SA or have SA
   * and no CG-ConfigInfo */
  AssertFatal(is_SA ^ (cg_configinfo != NULL), "cannot have SA and CG-ConfigInfo: NR-DC not supported xor need CG-ConfigInfo for NSA/phy-test/do-ra\n");

  NR_SCHED_LOCK(&mac->sched_lock);

  NR_UE_info_t *UE = NULL;
  if (!ue_id_provided) {
    UE = create_new_UE(mac, req->gNB_CU_ue_id, cg_configinfo);
    resp.gNB_DU_ue_id = UE->rnti;
    resp.crnti = malloc_or_fail(sizeof(*resp.crnti));
    *resp.crnti = UE->rnti;
  } else {
    DevAssert(is_SA);
    UE = find_nr_UE(&mac->UE_info, *req->gNB_DU_ue_id);
  }
  AssertFatal(UE, "no UE found or could not be created, but UE Context Setup Failed not implemented\n");
  resp.gNB_DU_ue_id = UE->rnti;

  NR_CellGroupConfig_t *new_CellGroup = clone_CellGroupConfig(UE->CellGroup);
  // Needed for DRB Setup (e.g., RLC might reduce SN size)
  UE->capability = ue_cap;

  if (req->srbs_len > 0) {
    resp.srbs_len = handle_ue_context_srbs_setup(UE, req->srbs_len, req->srbs, &resp.srbs, new_CellGroup, &mac->rlc_config);
  }

  if (req->drbs_len > 0) {
    resp.drbs_len =
        handle_ue_context_drbs_setup(UE, req->drbs_len, req->drbs, &resp.drbs, new_CellGroup, &mac->rlc_config);
  }

  if (req->rrc_container != NULL) {
    logical_chan_id_t id = 1;
    nr_rlc_srb_recv_sdu(UE->rnti, id, req->rrc_container->buf, req->rrc_container->len);
  }

  NR_ServingCellConfigCommon_t *scc = mac->common_channels[0].ServingCellConfigCommon;
  if (ue_cap != NULL && cg_configinfo == NULL) {
    // store the new UE capabilities, and update the cellGroupConfig
    // only to be done if we did not already update through the cg_configinfo
    update_cellGroupConfig(new_CellGroup, UE->uid, UE->capability, &mac->radio_config, scc);
  }

  if (!ue_id_provided && cg_configinfo == NULL) {
    /* new UE: tell the UE to reestablish RLC */
    struct NR_CellGroupConfig__rlc_BearerToAddModList *addmod = new_CellGroup->rlc_BearerToAddModList;
    for (int i = 0; i < addmod->list.count; ++i) {
      NR_RLC_BearerConfig_t *bc = addmod->list.array[i];
      asn1cCallocOne(bc->reestablishRLC, NR_RLC_BearerConfig__reestablishRLC_true);
      nr_rlc_reestablish_entity(UE->rnti, bc->logicalChannelIdentity);
    }
  }

  byte_array_t cgc = { .buf = calloc_or_fail(1,1024) };
  asn_enc_rval_t enc_rval =
      uper_encode_to_buffer(&asn_DEF_NR_CellGroupConfig, NULL, new_CellGroup, cgc.buf, 1024);
  AssertFatal(enc_rval.encoded > 0, "Could not encode CellGroup, failed element %s\n", enc_rval.failed_type->name);
  cgc.len = (enc_rval.encoded + 7) >> 3;
  resp.du_to_cu_rrc_info.cell_group_config = cgc;

  ASN_STRUCT_FREE(asn_DEF_NR_CellGroupConfig, UE->reconfigCellGroup);
  UE->reconfigCellGroup = new_CellGroup;
  int ss_type = cg_configinfo ? NR_SearchSpace__searchSpaceType_PR_ue_Specific: NR_SearchSpace__searchSpaceType_PR_common;
  configure_UE_BWP(mac, scc, UE, false, ss_type, -1, -1);

  if (mtc) {
    /* creates a suitable measGap config to be used in the gNB */
    UE->measgap_config = create_measgap_config(mtc, UE->current_DL_BWP.scs, mac->radio_config.minRXTXTIME);
    /* encodes the measGapConfig created, if useful (or not!) */
    byte_array_t *mgc = calloc_or_fail(1, sizeof(*mgc));
    mgc->buf = calloc_or_fail(1, 1024);
    mgc->len = encode_measgap_config(&UE->measgap_config, mgc->buf);
    resp.du_to_cu_rrc_info.meas_gap_config = mgc;
  }

  NR_SCHED_UNLOCK(&mac->sched_lock);

  mac->mac_rrc.ue_context_setup_response(&resp);

  /* free the memory we allocated above */
  free_ue_context_setup_resp(&resp);
  ASN_STRUCT_FREE(asn_DEF_NR_CG_ConfigInfo, cg_configinfo);
  ASN_STRUCT_FREE(asn_DEF_NR_MeasurementTimingConfiguration, mtc);
}

void ue_context_modification_request(const f1ap_ue_context_mod_req_t *req)
{
  gNB_MAC_INST *mac = RC.nrmac[0];
  f1ap_ue_context_mod_resp_t resp = {
    .gNB_CU_ue_id = req->gNB_CU_ue_id,
    .gNB_DU_ue_id = req->gNB_DU_ue_id,
  };

  NR_UE_NR_Capability_t *ue_cap = NULL;
  if (req->cu_to_du_rrc_info != NULL) {
    AssertFatal(req->cu_to_du_rrc_info->cg_configinfo == NULL, "CG-ConfigInfo not handled\n");
    if (req->cu_to_du_rrc_info->ue_cap) {
      byte_array_t *b = req->cu_to_du_rrc_info->ue_cap;
      ue_cap = get_ue_nr_cap(req->gNB_DU_ue_id, b->buf, b->len);
    }
  }

  NR_SCHED_LOCK(&mac->sched_lock);
  NR_UE_info_t *UE = find_nr_UE(&RC.nrmac[0]->UE_info, req->gNB_DU_ue_id);
  if (!UE) {
    LOG_E(NR_MAC, "could not find UE with RNTI %04x\n", req->gNB_DU_ue_id);
    NR_SCHED_UNLOCK(&mac->sched_lock);
    return;
  }

  NR_CellGroupConfig_t *new_CellGroup = clone_CellGroupConfig(UE->CellGroup);

  if (req->srbs_len > 0) {
    resp.srbs_len = handle_ue_context_srbs_setup(UE, req->srbs_len, req->srbs, &resp.srbs, new_CellGroup, &mac->rlc_config);
  }

  if (req->drbs_len > 0) {
    resp.drbs_len = handle_ue_context_drbs_setup(UE, req->drbs_len, req->drbs, &resp.drbs, new_CellGroup, &mac->rlc_config);
  }

  if (req->drbs_rel_len > 0) {
    handle_ue_context_drbs_release(UE, req->drbs_rel_len, req->drbs_rel, new_CellGroup);
  }

  if (req->rrc_container != NULL) {
    logical_chan_id_t id = 1;
    nr_rlc_srb_recv_sdu(req->gNB_DU_ue_id, id, req->rrc_container->buf, req->rrc_container->len);
  }

  NR_ServingCellConfigCommon_t *scc = mac->common_channels[0].ServingCellConfigCommon;
  if (req->reconfig_compl && *req->reconfig_compl != RRCreconf_success) {
    LOG_E(NR_MAC,
          "RRC reconfiguration outcome unsuccessful, but no rollback mechanism implemented to come back to old configuration\n");
  } else if (req->reconfig_compl) {
    LOG_I(NR_MAC, "DU received confirmation of successful RRC Reconfiguration\n");
    if (UE->reconfigCellGroup) {
      LOG_W(NR_MAC, "reconfigCellGroup still present, did we miss ACK for RRCReconfiguration?\n");
      ASN_STRUCT_FREE(asn_DEF_NR_CellGroupConfig, UE->CellGroup);
      UE->CellGroup = UE->reconfigCellGroup;
      UE->reconfigCellGroup = NULL;
    }
    if (UE->reestablish_rlc) {
      for (int i = 1; i < seq_arr_size(&UE->UE_sched_ctrl.lc_config); ++i) {
        nr_lc_config_t *c = seq_arr_at(&UE->UE_sched_ctrl.lc_config, i);
        c->suspended = false;
        nr_rlc_reestablish_entity(req->gNB_DU_ue_id, c->lcid);
      }
      UE->reestablish_rlc = false;
    }
    // we re-configure the BWP to apply the CellGroup and to use UE specific Search Space with DCIX1
    configure_UE_BWP(mac, scc, UE, false, NR_SearchSpace__searchSpaceType_PR_ue_Specific, -1, -1);
    nr_mac_clean_cellgroup(UE->CellGroup);
  }

  if (ue_cap != NULL) {
    // store the new UE capabilities, and update the cellGroupConfig
    ASN_STRUCT_FREE(asn_DEF_NR_UE_NR_Capability, UE->capability);
    UE->capability = ue_cap;
    LOG_I(NR_MAC, "UE %04x: received capabilities, updating CellGroupConfig\n", UE->rnti);
    update_cellGroupConfig(new_CellGroup, UE->uid, UE->capability, &mac->radio_config, scc);
  }

  if (req->srbs_len > 0 || req->drbs_len > 0 || req->drbs_rel_len > 0 || ue_cap != NULL) {
    resp.du_to_cu_rrc_info = calloc_or_fail(1, sizeof(du_to_cu_rrc_information_t));
    byte_array_t cgc = { .buf = calloc_or_fail(1, 1024) };
    asn_enc_rval_t enc_rval = uper_encode_to_buffer(&asn_DEF_NR_CellGroupConfig,
                                                    NULL,
                                                    new_CellGroup,
                                                    cgc.buf,
                                                    1024);
    AssertFatal(enc_rval.encoded > 0, "Could not encode CellGroup, failed element %s\n", enc_rval.failed_type->name);
    cgc.len = (enc_rval.encoded + 7) >> 3;
    resp.du_to_cu_rrc_info->cell_group_config = cgc;

    ASN_STRUCT_FREE(asn_DEF_NR_CellGroupConfig, UE->reconfigCellGroup);
    UE->reconfigCellGroup = new_CellGroup;
    configure_UE_BWP(mac, scc, UE, false, NR_SearchSpace__searchSpaceType_PR_common, -1, -1);
  } else {
    ASN_STRUCT_FREE(asn_DEF_NR_CellGroupConfig, new_CellGroup); // we actually don't need it
  }

  if (req->transm_action_ind != NULL) {
    AssertFatal(*req->transm_action_ind == TransmActionInd_STOP, "Transmission Action Indicator restart not handled yet\n");
    nr_transmission_action_indicator_stop(mac, UE);
  }
  NR_SCHED_UNLOCK(&mac->sched_lock);

  mac->mac_rrc.ue_context_modification_response(&resp);

  /* free the memory we allocated above */
  free_ue_context_mod_resp(&resp);
}

void ue_context_modification_confirm(const f1ap_ue_context_modif_confirm_t *confirm)
{
  LOG_I(MAC, "Received UE Context Modification Confirm for UE %04x\n", confirm->gNB_DU_ue_id);

  gNB_MAC_INST *mac = RC.nrmac[0];
  NR_SCHED_LOCK(&mac->sched_lock);
  /* check first that the scheduler knows such UE */
  NR_UE_info_t *UE = find_nr_UE(&mac->UE_info, confirm->gNB_DU_ue_id);
  if (UE == NULL) {
    LOG_E(MAC, "ERROR: unknown UE with RNTI %04x, ignoring UE Context Modification Confirm\n", confirm->gNB_DU_ue_id);
    NR_SCHED_UNLOCK(&mac->sched_lock);
    return;
  }
  NR_SCHED_UNLOCK(&mac->sched_lock);

  if (confirm->rrc_container_length > 0) {
    logical_chan_id_t id = 1;
    nr_rlc_srb_recv_sdu(confirm->gNB_DU_ue_id, id, confirm->rrc_container, confirm->rrc_container_length);
  }
  /* nothing else to be done? */
}

void ue_context_modification_refuse(const f1ap_ue_context_modif_refuse_t *refuse)
{
  /* Currently, we only use the UE Context Modification Required procedure to
   * trigger a RRC reconfigurtion after Msg.3 with C-RNTI MAC CE. If the CU
   * refuses, it cannot do this reconfiguration, leaving the UE in an
   * unconfigured state. Therefore, we just free all RA-related info, and
   * request the release of the UE.  */
  LOG_W(MAC, "Received UE Context Modification Refuse for %04x, requesting release\n", refuse->gNB_DU_ue_id);

  gNB_MAC_INST *mac = RC.nrmac[0];
  NR_SCHED_LOCK(&mac->sched_lock);
  NR_UE_info_t *UE = find_nr_UE(&RC.nrmac[0]->UE_info, refuse->gNB_DU_ue_id);
  if (UE == NULL) {
    LOG_E(MAC, "ERROR: unknown UE with RNTI %04x, ignoring UE Context Modification Refuse\n", refuse->gNB_DU_ue_id);
    NR_SCHED_UNLOCK(&mac->sched_lock);
    return;
  }

  NR_SCHED_UNLOCK(&mac->sched_lock);

  f1ap_ue_context_rel_req_t request = {
    .gNB_CU_ue_id = refuse->gNB_CU_ue_id,
    .gNB_DU_ue_id = refuse->gNB_DU_ue_id,
    .cause = F1AP_CAUSE_RADIO_NETWORK,
    .cause_value = F1AP_CauseRadioNetwork_procedure_cancelled,
  };
  mac->mac_rrc.ue_context_release_request(&request);
}

void ue_context_release_command(const f1ap_ue_context_rel_cmd_t *cmd)
{
  /* mark UE as to be deleted after PUSCH failure */
  gNB_MAC_INST *mac = RC.nrmac[0];
  NR_SCHED_LOCK(&mac->sched_lock);
  NR_UE_info_t *UE = find_nr_UE(&mac->UE_info, cmd->gNB_DU_ue_id);
  UE = UE ? UE : find_ra_UE(&mac->UE_info, cmd->gNB_DU_ue_id);
  if (UE == NULL) {
    NR_SCHED_UNLOCK(&mac->sched_lock);
    f1ap_ue_context_rel_cplt_t complete = {
        .gNB_CU_ue_id = cmd->gNB_CU_ue_id,
        .gNB_DU_ue_id = cmd->gNB_DU_ue_id,
    };
    mac->mac_rrc.ue_context_release_complete(&complete);
    return;
  }

  instance_t f1inst = get_f1_gtp_instance();
  if (f1inst >= 0)
    newGtpuDeleteAllTunnels(f1inst, cmd->gNB_DU_ue_id);

  if (UE->UE_sched_ctrl.ul_failure || !cmd->rrc_container) {
    /* The UE is already not connected anymore or we have nothing to forward*/
    nr_mac_release_ue(mac, cmd->gNB_DU_ue_id);
    nr_mac_trigger_release_complete(mac, cmd->gNB_DU_ue_id);
  } else if (cmd->rrc_container && cmd->srb_id){
    /* UE is in sync: forward release message and mark to be deleted
     * after UL failure */
    byte_array_t *rrc_cont = cmd->rrc_container;
    nr_rlc_srb_recv_sdu(cmd->gNB_DU_ue_id, *cmd->srb_id, rrc_cont->buf, rrc_cont->len);
    nr_mac_trigger_release_timer(&UE->UE_sched_ctrl, UE->current_UL_BWP.scs);
  }
  NR_SCHED_UNLOCK(&mac->sched_lock);
}

// reset_sc_info() introduced in develop in the context of other changes,
// necessary here?
static void reset_sc_info(NR_UE_ServingCell_Info_t *sc_info)
{
  sc_info->csi_MeasConfig = NULL;
  sc_info->nrofHARQ_ProcessesForPDSCH = NULL;
  sc_info->maxMIMO_Layers_PDSCH = NULL;
  sc_info->downlinkHARQ_FeedbackDisabled_r17 = NULL;
  sc_info->pdsch_CGB_Transmission = NULL;
  sc_info->nrofHARQ_ProcessesForPDSCH_v1700 = NULL;
  sc_info->crossCarrierSchedulingConfig = NULL;
  sc_info->supplementaryUplink = NULL;
  sc_info->carrierSwitching = NULL;
  sc_info->maxMIMO_Layers_PUSCH = NULL;
  sc_info->rateMatching_PUSCH = NULL;
  sc_info->pusch_CGB_Transmission = NULL;
  sc_info->nrofHARQ_ProcessesForPUSCH_r17 = NULL;
}

static void process_reestablishment(gNB_MAC_INST *mac, uint32_t new_dl_rrc_id, uint32_t old_dl_rrc_id)
{
  /* check first that the scheduler knows such UE */
  NR_UE_info_t *UE = find_ra_UE(&mac->UE_info, new_dl_rrc_id);
  if (!UE) {
    LOG_E(MAC, "ERROR: couldn't find UE with RNTI %04x, ignoring DL RRC Message Transfer\n", new_dl_rrc_id);
    return;
  }

  NR_UE_info_t *oldUE = find_nr_UE(&mac->UE_info, old_dl_rrc_id);
  if (!oldUE) {
    /* No matching UE-associated logical F1-connection for the old gNB-DU UE F1AP ID.
     * Per TS 38.473, if there's no matching connection, there's nothing to release. */
    LOG_W(NR_MAC,
          "DL RRC Message Transfer: old gNB-DU UE F1AP ID %04x has no matching UE-associated F1-connection, nothing to relese\n",
          old_dl_rrc_id);
    /* Clean up any F1 UE data associated with the old gNB-DU UE F1AP ID */
    if (du_exists_f1_ue_data(old_dl_rrc_id))
      du_remove_f1_ue_data(old_dl_rrc_id);
    return;
  }

  // Per TS 38.401: "Find UE context based on old gNB-DU UE F1AP ID, replace old C-RNTI/PCI with new C-RNTI/PCI"
  rnti_t new_rnti = UE->rnti;
  // assigning the old RNTI to the new UE so that mac_remove_nr_ue prints correct RNTI when removing
  UE->rnti = oldUE->rnti;
  oldUE->rnti = new_rnti;
  for (int i = 1; i < seq_arr_size(&oldUE->UE_sched_ctrl.lc_config); ++i) {
    const nr_lc_config_t *c = seq_arr_at(&oldUE->UE_sched_ctrl.lc_config, i);
    nr_lc_config_t new = *c;
    new.suspended = true;
    nr_mac_add_lcid(&UE->UE_sched_ctrl, &new);
  }
  // need to move the oldUE to RA list because it still needs to transmit MSG4
  NR_RA_t *temp_ra = UE->ra;
  oldUE->ra = temp_ra;
  UE->ra = NULL;
  NR_UE_sched_ctrl_t temp_sc;
  memcpy(&temp_sc, &UE->UE_sched_ctrl, sizeof(NR_UE_sched_ctrl_t));
  memcpy(&UE->UE_sched_ctrl, &oldUE->UE_sched_ctrl, sizeof(NR_UE_sched_ctrl_t));
  memcpy(&oldUE->UE_sched_ctrl, &temp_sc, sizeof(NR_UE_sched_ctrl_t));
  mac_remove_nr_ue(mac, UE->rnti);
  NR_UE_info_t *r = remove_UE_from_list(MAX_MOBILES_PER_GNB + 1, mac->UE_info.connected_ue_list, oldUE->rnti);
  DevAssert(r == oldUE);
  add_UE_to_list(NR_NB_RA_PROC_MAX, mac->UE_info.access_ue_list, oldUE);
  nr_rlc_remove_ue(new_dl_rrc_id);
  nr_rlc_update_id(old_dl_rrc_id, new_dl_rrc_id);
  instance_t f1inst = get_f1_gtp_instance();
  if (f1inst >= 0) // we actually use F1-U
    gtpv1u_update_ue_id(f1inst, old_dl_rrc_id, new_dl_rrc_id);

  /* Per TS 38.331 5.3.7.2: the UE releases the spCellConfig, so we drop it
   * from the current configuration. It will be reapplied when the
   * reconfiguration has succeeded (indicated by the CU).
   * Guard against double reestablishment: if reestablish_rlc is already set,
   * reconfigCellGroup was saved by the first reestablishment and
   * CellGroup.spCellConfig is already NULL — don't overwrite. */
  if (!oldUE->reestablish_rlc) {
    asn_copy(&asn_DEF_NR_CellGroupConfig, (void **)&oldUE->reconfigCellGroup, oldUE->CellGroup);
    ASN_STRUCT_FREE(asn_DEF_NR_SpCellConfig, oldUE->CellGroup->spCellConfig);
    oldUE->CellGroup->spCellConfig = NULL;
    reset_sc_info(&oldUE->sc_info);
    configure_UE_BWP(mac,
                     mac->common_channels[0].ServingCellConfigCommon,
                     oldUE,
                     true,
                     NR_SearchSpace__searchSpaceType_PR_common,
                     -1,
                     -1);
  } else {
    LOG_W(NR_MAC,
          "UE %04x: reestablishment while other reestablishment still pending keeping saved reconfigCellGroup with spCellConfig\n",
          oldUE->rnti);
  }
  oldUE->reestablish_rlc = true;
  /* Per TS 38.331 clause 5.3.7.4: apply gNB RLC configuration for SRB1 to match the UE RLC configuration defined in 9.2.1.
   * Use configuration file values for timers t_poll_retransmit, t_reassembly and t_status_prohibit */
  nr_rlc_configuration_t rlc_configuration = mac->rlc_config;
  rlc_configuration.srb.poll_pdu = -1;
  rlc_configuration.srb.poll_byte = -1;
  rlc_configuration.srb.max_retx_threshold = 8;
  rlc_configuration.srb.sn_field_length = 12;
  NR_RLC_Config_t *rlc_Config = nr_srb_config(&rlc_configuration);
  nr_rlc_reconfigure_entity(new_dl_rrc_id, 1, rlc_Config);
  ASN_STRUCT_FREE(asn_DEF_NR_RLC_Config, rlc_Config);
}

/** @brief Process a DL RRC MESSAGE TRANSFER. Handles delivery of an RRC message to a UE
 * via the F1AP DL RRC MESSAGE TRANSFER procedure, as specified in TS 38.473. This procedure
 * is also responsible for re-establishing UE context when required (e.g., during RRC connection
 * reestablishment).
 * @param dl_rrc Pointer to the DL RRC MESSAGE TRANSFER data structure. */
void dl_rrc_message_transfer(const f1ap_dl_rrc_message_t *dl_rrc)
{
  LOG_D(NR_MAC,
        "DL RRC Message Transfer with %d bytes for RNTI %04x SRB %d\n",
        dl_rrc->rrc_container_length,
        dl_rrc->gNB_DU_ue_id,
        dl_rrc->srb_id);

  gNB_MAC_INST *mac = RC.nrmac[0];
  pthread_mutex_lock(&mac->sched_lock);
  /* check first that the scheduler knows such UE */
  NR_UE_info_t *UE = find_nr_UE(&mac->UE_info, dl_rrc->gNB_DU_ue_id);
  UE = UE ? UE : find_ra_UE(&mac->UE_info, dl_rrc->gNB_DU_ue_id);
  if (UE == NULL) {
    LOG_E(MAC, "ERROR: unknown UE with RNTI %04x, ignoring DL RRC Message Transfer\n", dl_rrc->gNB_DU_ue_id);
    pthread_mutex_unlock(&mac->sched_lock);
    return;
  }
  pthread_mutex_unlock(&mac->sched_lock);

  if (!du_exists_f1_ue_data(dl_rrc->gNB_DU_ue_id)) {
    LOG_D(NR_MAC, "No CU UE ID stored for UE RNTI %04x, adding CU UE ID %d\n", dl_rrc->gNB_DU_ue_id, dl_rrc->gNB_CU_ue_id);
    f1_ue_data_t new_ue_data = {.secondary_ue = dl_rrc->gNB_CU_ue_id};
    bool success = du_add_f1_ue_data(dl_rrc->gNB_DU_ue_id, &new_ue_data);
    DevAssert(success);
  }


  /* if we get the old-gNB-DU-UE-ID, this means there is a reestablishment
   * ongoing. */
  if (dl_rrc->old_gNB_DU_ue_id != NULL) {
    if (*dl_rrc->old_gNB_DU_ue_id != dl_rrc->gNB_DU_ue_id) {
      NR_SCHED_LOCK(&mac->sched_lock);
      process_reestablishment(mac, dl_rrc->gNB_DU_ue_id, *dl_rrc->old_gNB_DU_ue_id);
      NR_SCHED_UNLOCK(&mac->sched_lock);
    } else
      LOG_E(NR_MAC, "Current and old gNB DU UE ID are the same (%04x), cannot do reestablishment\n", dl_rrc->gNB_DU_ue_id);
  }
  /* the DU ue id is the RNTI */
  nr_rlc_srb_recv_sdu(dl_rrc->gNB_DU_ue_id, dl_rrc->srb_id, dl_rrc->rrc_container, dl_rrc->rrc_container_length);
}
