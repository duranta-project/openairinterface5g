/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief rrc procedures for UE context
 */
#ifndef __RRC_GNB_UE_CONTEXT_H__
#define __RRC_GNB_UE_CONTEXT_H__

#include <common/utils/assertions.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <stdint.h>
#include "collection/tree.h"
#include "nr_rrc_defs.h"

int rrc_gNB_compare_ue_rnti_id(rrc_gNB_ue_context_t* c1_pP, rrc_gNB_ue_context_t* c2_pP);

RB_PROTOTYPE(rrc_nr_ue_tree_s, rrc_gNB_ue_context_s, entries, rrc_gNB_compare_ue_rnti_id);

rrc_gNB_ue_context_t* rrc_gNB_get_ue_context(gNB_RRC_INST* rrc_instance_pP, ue_id_t ue);
rrc_gNB_ue_context_t* rrc_gNB_get_ue_context_by_rnti(gNB_RRC_INST* rrc_instance_pP, sctp_assoc_t assoc_id, rnti_t rntiP);
rrc_gNB_ue_context_t* rrc_gNB_get_ue_context_by_rnti_any_du(gNB_RRC_INST* rrc_instance_pP, rnti_t rntiP, int pci);
rrc_gNB_ue_context_t* rrc_gNB_get_ue_context_by_amf_ue_ngap_id(gNB_RRC_INST* rrc_instance_pP, uint64_t amf_ue_ngap_id);

void rrc_gNB_free_mem_ue_context(rrc_gNB_ue_context_t* const ue_context_pP);

void rrc_gNB_remove_ue_context(gNB_RRC_INST* rrc_instance_pP, rrc_gNB_ue_context_t* ue_context_pP);

rrc_gNB_ue_context_t* rrc_gNB_ue_context_random_exist(gNB_RRC_INST* rrc_instance_pP, const uint64_t ue_identityP);

rrc_gNB_ue_context_t* rrc_gNB_ue_context_5g_s_tmsi_exist(gNB_RRC_INST* rrc_instance_pP, const uint64_t s_TMSI);
rrc_gNB_ue_context_t* rrc_gNB_create_ue_context(sctp_assoc_t assoc_id,
                                                rnti_t rnti,
                                                gNB_RRC_INST* rrc_instance_pP,
                                                const uint64_t ue_identityP,
                                                uint32_t du_ue_id);

#endif
