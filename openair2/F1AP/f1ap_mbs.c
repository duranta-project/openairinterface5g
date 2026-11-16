/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*! \file f1ap_mbs.c
 * \brief Broadcast Context Setup over F1AP (TS 38.473, Rel-17 MBS)
 *
 * How the CU tells the DU that a broadcast session exists. The DU is where
 * SIB20 and the group scheduling live, so until this arrives the DU has no
 * way of knowing about a session the core created -- which is why it used to
 * read one from a file beside the binary.
 *
 * Only the setup procedure is here. Modification and release follow.
 */

#include "f1ap_mbs.h"

#include "f1ap_common.h"
#include "f1ap_encoder.h"
#include "f1ap_lib_common.h"
#include "f1ap_itti_messaging.h"
#include "common/utils/oai_asn1.h"
#include "common/utils/assertions.h"
#include "openair2/LAYER2/NR_MAC_gNB/mac_rrc_dl_handler.h"

#include "F1AP_F1AP-PDU.h"
#include "F1AP_InitiatingMessage.h"
#include "F1AP_BroadcastContextSetupRequest.h"
#include "F1AP_ProtocolIE-Field.h"
#include "F1AP_BroadcastMRBs-ToBeSetup-List.h"
#include "F1AP_BroadcastMRBs-ToBeSetup-Item.h"
#include "F1AP_MBS-Session-ID.h"
#include "F1AP_MBS-CUtoDURRCInformation.h"
#include "F1AP_MBS-Broadcast-Cell-Item.h"
#include "F1AP_MBS-Broadcast-MRB-Item.h"
#include "F1AP_MBS-Flows-Mapped-To-MRB-Item.h"
#include "F1AP_MBSF1UInformation.h"
#include "F1AP_NonDynamic5QIDescriptor.h"
#include "F1AP_GTPTunnel.h"

/* Minimal QoS for a broadcast flow. Nothing reads it yet -- the DU has no
 * MBS QoS handling and the payload does not travel this path -- but the IE
 * is mandatory, so it has to be a valid one. */
static F1AP_QoSFlowLevelQoSParameters_t mbs_qos_parameters(void)
{
  F1AP_QoSFlowLevelQoSParameters_t qos = {0};
  qos.qoS_Characteristics.present = F1AP_QoS_Characteristics_PR_non_Dynamic_5QI;
  asn1cCalloc(qos.qoS_Characteristics.choice.non_Dynamic_5QI, nd);
  nd->fiveQI = 9;
  qos.nGRANallocationRetentionPriority.priorityLevel = F1AP_PriorityLevel_highest;
  qos.nGRANallocationRetentionPriority.pre_emptionCapability = F1AP_Pre_emptionCapability_shall_not_trigger_pre_emption;
  qos.nGRANallocationRetentionPriority.pre_emptionVulnerability = F1AP_Pre_emptionVulnerability_not_pre_emptable;
  return qos;
}

F1AP_F1AP_PDU_t *encode_f1ap_broadcast_context_setup_request(const f1ap_broadcast_ctx_setup_t *in)
{
  F1AP_F1AP_PDU_t *pdu = calloc_or_fail(1, sizeof(*pdu));
  pdu->present = F1AP_F1AP_PDU_PR_initiatingMessage;
  asn1cCalloc(pdu->choice.initiatingMessage, msg);
  msg->procedureCode = F1AP_ProcedureCode_id_BroadcastContextSetup;
  msg->criticality = F1AP_Criticality_reject;
  msg->value.present = F1AP_InitiatingMessage__value_PR_BroadcastContextSetupRequest;
  F1AP_BroadcastContextSetupRequest_t *out = &msg->value.choice.BroadcastContextSetupRequest;

  /* mandatory: the CU's identifier for this MBS context */
  {
    asn1cSequenceAdd(out->protocolIEs.list, F1AP_BroadcastContextSetupRequestIEs_t, ie);
    ie->id = F1AP_ProtocolIE_ID_id_gNB_CU_MBS_F1AP_ID;
    ie->criticality = F1AP_Criticality_reject;
    ie->value.present = F1AP_BroadcastContextSetupRequestIEs__value_PR_GNB_CU_MBS_F1AP_ID;
    ie->value.choice.GNB_CU_MBS_F1AP_ID = in->gNB_cu_mbs_f1ap_id;
  }

  /* mandatory: which session, as the core named it */
  {
    asn1cSequenceAdd(out->protocolIEs.list, F1AP_BroadcastContextSetupRequestIEs_t, ie);
    ie->id = F1AP_ProtocolIE_ID_id_MBS_Session_ID;
    ie->criticality = F1AP_Criticality_reject;
    ie->value.present = F1AP_BroadcastContextSetupRequestIEs__value_PR_MBS_Session_ID;
    OCTET_STRING_fromBuf(&ie->value.choice.MBS_Session_ID.tMGI, (const char *)in->tmgi, sizeof(in->tmgi));
  }

  /* mandatory: the cells to broadcast on, and the MRB configuration the UE
   * will be told about. One cell, because the DU has one. */
  {
    asn1cSequenceAdd(out->protocolIEs.list, F1AP_BroadcastContextSetupRequestIEs_t, ie);
    ie->id = F1AP_ProtocolIE_ID_id_MBS_CUtoDURRCInformation;
    ie->criticality = F1AP_Criticality_reject;
    ie->value.present = F1AP_BroadcastContextSetupRequestIEs__value_PR_MBS_CUtoDURRCInformation;
    F1AP_MBS_CUtoDURRCInformation_t *rrcinfo = &ie->value.choice.MBS_CUtoDURRCInformation;

    asn1cSequenceAdd(rrcinfo->mBS_Broadcast_Cell_List.list, F1AP_MBS_Broadcast_Cell_Item_t, cell);
    MCC_MNC_TO_PLMNID(in->plmn.mcc, in->plmn.mnc, in->plmn.mnc_digit_length, &cell->nRCGI.pLMN_Identity);
    NR_CELL_ID_TO_BIT_STRING(in->nr_cellid, &cell->nRCGI.nRCellIdentity);

    asn1cSequenceAdd(rrcinfo->mBS_Broadcast_MRB_List.list, F1AP_MBS_Broadcast_MRB_Item_t, mrb);
    mrb->mRB_ID = in->mrb_id;
    /* The PDCP configuration the UE is given for this MRB, as an encoded
     * RRC structure. The DU builds its own from nr_mbs_rlc_params() today,
     * so this is empty rather than wrong -- it becomes the real thing when
     * the CU owns the MRB configuration. */
    OCTET_STRING_fromBuf(&mrb->mRB_PDCP_Config_Broadcast, "", 0);
  }

  /* mandatory: the slice */
  {
    asn1cSequenceAdd(out->protocolIEs.list, F1AP_BroadcastContextSetupRequestIEs_t, ie);
    ie->id = F1AP_ProtocolIE_ID_id_SNSSAI;
    ie->criticality = F1AP_Criticality_reject;
    ie->value.present = F1AP_BroadcastContextSetupRequestIEs__value_PR_SNSSAI;
    const nssai_t nssai = {.sst = in->sst, .sd = 0xffffff};
    ie->value.choice.SNSSAI = encode_nssai(&nssai);
  }

  /* mandatory: the bearers */
  {
    asn1cSequenceAdd(out->protocolIEs.list, F1AP_BroadcastContextSetupRequestIEs_t, ie);
    ie->id = F1AP_ProtocolIE_ID_id_BroadcastMRBs_ToBeSetup_List;
    ie->criticality = F1AP_Criticality_reject;
    ie->value.present = F1AP_BroadcastContextSetupRequestIEs__value_PR_BroadcastMRBs_ToBeSetup_List;

    asn1cSequenceAdd(ie->value.choice.BroadcastMRBs_ToBeSetup_List.list, F1AP_BroadcastMRBs_ToBeSetup_ItemIEs_t, item_ies);
    item_ies->id = F1AP_ProtocolIE_ID_id_BroadcastMRBs_ToBeSetup_Item;
    item_ies->criticality = F1AP_Criticality_reject;
    item_ies->value.present = F1AP_BroadcastMRBs_ToBeSetup_ItemIEs__value_PR_BroadcastMRBs_ToBeSetup_Item;
    F1AP_BroadcastMRBs_ToBeSetup_Item_t *mrb = &item_ies->value.choice.BroadcastMRBs_ToBeSetup_Item;

    mrb->mRB_ID = in->mrb_id;
    mrb->mRB_QoSInformation = mbs_qos_parameters();

    asn1cSequenceAdd(mrb->mBS_Flows_Mapped_To_MRB_List.list, F1AP_MBS_Flows_Mapped_To_MRB_Item_t, flow);
    flow->mBS_QoSFlowIdentifier = 1;
    flow->mbs_QoSFlowLevelQoSParameters = mbs_qos_parameters();

    /* The F1-U tunnel the CU-UP will send this MRB on.
     *
     * There is no such tunnel yet: the broadcast user plane is not
     * implemented, so nothing will ever arrive on it. The IE is mandatory,
     * so it carries the CU's own address and a zero TEID, and it is the one
     * field in this message that is a placeholder rather than a fact. It
     * becomes real when the CU-UP terminates the broadcast tunnel. */
    mrb->bcBearerCtxtF1U_TNLInfoatCU.present = F1AP_BCBearerContextF1U_TNLInfo_PR_locationindpendent;
    asn1cCalloc(mrb->bcBearerCtxtF1U_TNLInfoatCU.choice.locationindpendent, f1u);
    f1u->mbs_f1u_info.present = F1AP_UPTransportLayerInformation_PR_gTPTunnel;
    asn1cCalloc(f1u->mbs_f1u_info.choice.gTPTunnel, gtp);
    TRANSPORT_LAYER_ADDRESS_IPv4_TO_BIT_STRING(0, &gtp->transportLayerAddress);
    INT32_TO_OCTET_STRING(0, &gtp->gTP_TEID);
  }

  return pdu;
}

bool decode_f1ap_broadcast_context_setup_request(f1ap_broadcast_ctx_setup_t *out, const F1AP_F1AP_PDU_t *pdu)
{
  memset(out, 0, sizeof(*out));
  const F1AP_BroadcastContextSetupRequest_t *in = &pdu->choice.initiatingMessage->value.choice.BroadcastContextSetupRequest;
  for (int i = 0; i < in->protocolIEs.list.count; i++) {
    const F1AP_BroadcastContextSetupRequestIEs_t *ie = in->protocolIEs.list.array[i];
    switch (ie->id) {
      case F1AP_ProtocolIE_ID_id_gNB_CU_MBS_F1AP_ID:
        out->gNB_cu_mbs_f1ap_id = ie->value.choice.GNB_CU_MBS_F1AP_ID;
        break;
      case F1AP_ProtocolIE_ID_id_MBS_Session_ID: {
        const OCTET_STRING_t *t = &ie->value.choice.MBS_Session_ID.tMGI;
        if (t->size != (int)sizeof(out->tmgi))
          return false;
        memcpy(out->tmgi, t->buf, sizeof(out->tmgi));
        break;
      }
      case F1AP_ProtocolIE_ID_id_BroadcastMRBs_ToBeSetup_List: {
        const F1AP_BroadcastMRBs_ToBeSetup_List_t *l = &ie->value.choice.BroadcastMRBs_ToBeSetup_List;
        if (l->list.count > 0) {
          const F1AP_BroadcastMRBs_ToBeSetup_ItemIEs_t *it =
              (const F1AP_BroadcastMRBs_ToBeSetup_ItemIEs_t *)l->list.array[0];
          out->mrb_id = it->value.choice.BroadcastMRBs_ToBeSetup_Item.mRB_ID;
        }
        break;
      }
      default:
        break;
    }
  }
  return true;
}

void CU_send_BROADCAST_CONTEXT_SETUP_REQUEST(sctp_assoc_t assoc_id, const f1ap_broadcast_ctx_setup_t *req)
{
  F1AP_F1AP_PDU_t *pdu = encode_f1ap_broadcast_context_setup_request(req);
  uint8_t *buffer = NULL;
  uint32_t len = 0;
  if (f1ap_encode_pdu(pdu, &buffer, &len) < 0) {
    LOG_E(F1AP, "MBS: failed to encode Broadcast Context Setup Request\n");
    ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
    return;
  }
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  f1ap_itti_send_sctp_data_req(assoc_id, buffer, len);
  LOG_A(F1AP, "MBS: Broadcast Context Setup Request sent to the DU\n");
}

int DU_handle_BROADCAST_CONTEXT_SETUP_REQUEST(instance_t instance,
                                              sctp_assoc_t assoc_id,
                                              uint32_t stream,
                                              F1AP_F1AP_PDU_t *pdu)
{
  f1ap_broadcast_ctx_setup_t req = {0};
  if (!decode_f1ap_broadcast_context_setup_request(&req, pdu)) {
    LOG_E(F1AP, "MBS: failed to decode Broadcast Context Setup Request\n");
    return -1;
  }
  LOG_A(F1AP,
        "MBS: Broadcast Context Setup for TMGI %02x%02x%02x%02x%02x%02x, MRB %ld\n",
        req.tmgi[0], req.tmgi[1], req.tmgi[2], req.tmgi[3], req.tmgi[4], req.tmgi[5],
        req.mrb_id);
  broadcast_context_setup(&req);
  return 0;
}
