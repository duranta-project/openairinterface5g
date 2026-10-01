/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* XnAP UE ID management — XNAP allocates the local XnAP UE IDs and keeps, per
 * Xn handover, the XnAP UE IDs of both gNBs and the SCTP association to the peer.
 * RRC only refers to a handover by its rrc_ue_id.
 *
 * Source gNB: allocates s_ng_node_ue_xnap_id when sending HandoverRequest,
 *   learns t_ng_node_ue_xnap_id from HandoverRequestAcknowledge.
 * Target gNB: allocates t_ng_node_ue_xnap_id when receiving HandoverRequest,
 *   RRC binds it to the rrc_ue_id of the UE it creates for the handover. */

#ifndef XNAP_IDS_H_
#define XNAP_IDS_H_

#include <stdbool.h>
#include <stdint.h>
#include "common/platform_types.h"
#include "openair2/COMMON/sctp_messages_types.h"

typedef struct {
  // RRC UE ID at this gNB (0 at target until bound)
  uint32_t rrc_ue_id;
  // Source NG-RAN node UE XnAP ID
  uint32_t s_ng_node_ue_xnap_id;
  // Target NG-RAN node UE XnAP ID (0 at source until HandoverRequestAcknowledge)
  uint32_t t_ng_node_ue_xnap_id;
  // SCTP association to the peer gNB
  sctp_assoc_t assoc_id;
} xn_ue_data_t;

/* Call once when the XNAP task starts */
void xn_init_ue_data(void);

/* Source side: entries keyed on s_ng_node_ue_xnap_id, indexed by rrc_ue_id */
uint32_t xn_add_source_ue(uint32_t rrc_ue_id, sctp_assoc_t target_assoc_id);
bool xn_set_source_ue_target_id(uint32_t s_xnap_id, uint32_t t_xnap_id);
bool xn_get_source_ue(uint32_t rrc_ue_id, xn_ue_data_t *out);
bool xn_get_source_ue_by_xnap_id(uint32_t s_xnap_id, xn_ue_data_t *out);
bool xn_remove_source_ue(uint32_t s_xnap_id);
int xn_remove_source_ues_by_assoc(sctp_assoc_t assoc_id, xn_ue_data_t *out, int max);

/* Target side: entries keyed on t_ng_node_ue_xnap_id, indexed by rrc_ue_id once bound.
 * xn_add_target_ue() returns 0 if the handover s_xnap_id from the peer already has an entry */
uint32_t xn_add_target_ue(uint32_t s_xnap_id, sctp_assoc_t source_assoc_id);
bool xn_bind_target_ue(uint32_t t_xnap_id, uint32_t rrc_ue_id);
bool xn_get_target_ue(uint32_t rrc_ue_id, xn_ue_data_t *out);
bool xn_get_target_ue_by_xnap_id(uint32_t t_xnap_id, xn_ue_data_t *out);
bool xn_remove_target_ue(uint32_t t_xnap_id);
int xn_remove_target_ues_by_assoc(sctp_assoc_t assoc_id, xn_ue_data_t *out, int max);

/* Remove the source and target entries of rrc_ue_id, e.g. the UE context is removed:
 * a late XnAP message must not reach a new UE reusing the rrc_ue_id.
 * Returns true if an entry was removed */
bool xn_remove_ue(uint32_t rrc_ue_id);

#endif /* XNAP_IDS_H_ */
