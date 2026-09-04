/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef __RRC_GNB_NRDC_H__
#define __RRC_GNB_NRDC_H__

#include "nr_rrc_defs.h"
#include "NR_DL-DCCH-Message.h"
#include "NR_UL-DCCH-Message.h"

void rrc_gnb_nrdc_start(gNB_RRC_INST *rrc, gNB_RRC_UE_t *ue);
void rrc_gnb_free_nrdc(gNB_RRC_UE_t *ue);
void rrc_gnb_nrdc_ue_capabilities_received(gNB_RRC_INST *rrc, gNB_RRC_UE_t *ue,  const NR_UECapabilityInformation_t *ue_cap);
void rrc_gnb_nrdc_rrc_reconfiguration_complete_received(gNB_RRC_INST *rrc, gNB_RRC_UE_t *ue, int xid);
void rrc_gnb_nrdc_measurement_received(gNB_RRC_INST *rrc, gNB_RRC_UE_t *ue, NR_MeasurementReport_t *meas);
void rrc_gnb_nrdc_timeout(gNB_RRC_INST *rrc, nr_rrc_nrdc_timeout_t *timeout);

int get_scg_measurement_id(gNB_RRC_UE_t *ue);

bool rrc_gnb_nrdc_wait_for_f1_context_setup_response(gNB_RRC_UE_t *ue);
void nrdc_rrc_CU_process_ue_context_setup_response(gNB_RRC_UE_t *ue, gNB_RRC_INST *rrc, f1ap_ue_context_setup_resp_t *resp);

bool is_nrdc_bearer(gNB_RRC_UE_t *ue, int rb_id);
void nrdc_release_bearer(const gNB_RRC_INST *rrc, gNB_RRC_UE_t *ue, int bearer_id);

bool rrc_gnb_nrdc_wait_for_f1_context_modification_response(gNB_RRC_UE_t *ue);
void nrdc_rrc_CU_process_ue_context_modification_response(gNB_RRC_UE_t *ue, gNB_RRC_INST *rrc, f1ap_ue_context_mod_resp_t *resp);

bool nrdc_handle_f1_context_release_complete(gNB_RRC_INST *rrc, gNB_RRC_UE_t *ue, f1ap_ue_context_rel_cplt_t *complete);

void nrdc_scg_ue_release(gNB_RRC_INST *rrc, gNB_RRC_UE_t *ue);

void nrdc_handle_scg_failure_information(gNB_RRC_INST *rrc, gNB_RRC_UE_t *ue, NR_SCGFailureInformation_t *msg);
bool rrc_gnb_nrdc_wait_for_scg_failure_context_modification_response(gNB_RRC_UE_t *ue);
void nrdc_scg_failure_context_modification_response(gNB_RRC_UE_t *ue, gNB_RRC_INST *rrc, f1ap_ue_context_mod_resp_t *resp);

#endif /* __RRC_GNB_NRDC_H__ */
