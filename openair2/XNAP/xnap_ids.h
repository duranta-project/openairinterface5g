/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* XnAP UE ID management — maps locally-assigned XnAP UE IDs to RRC UE IDs.
 *
 * Source gNB: allocates s_ng_node_ue_xnap_id when sending HandoverRequest;
 *   stores {rrc_ue_id, target_assoc_id} so ACK/Failure can be routed back.
 * Target gNB: allocates t_ng_node_ue_xnap_id when sending HandoverRequestAck;
 *   stores {rrc_ue_id, source_assoc_id} so subsequent messages can be routed. */

#ifndef XNAP_IDS_H_
#define XNAP_IDS_H_

#include <stdbool.h>
#include <stdint.h>
#include "common/platform_types.h"
#include "openair2/COMMON/sctp_messages_types.h"

/* Source-side entry: keyed on s_ng_node_ue_xnap_id */
typedef struct {
  // RRC UE ID at source gNB
  uint32_t rrc_ue_id;
  // SCTP association to the target gNB
  sctp_assoc_t target_assoc_id;
} xn_ue_data_t;

/* Target-side entry: keyed on t_ng_node_ue_xnap_id */
typedef struct {
  // RRC UE ID at target gNB
  uint32_t rrc_ue_id;
  // SCTP association back to the source gNB
  sctp_assoc_t source_assoc_id;
} xn_target_ue_data_t;

/* Call once when the XNAP task starts */
void xn_init_ue_data(void);

/* Source-side table: keyed on s_ng_node_ue_xnap_id */
uint32_t xn_alloc_ue_id(void);
bool xn_add_ue_data(uint32_t xn_ue_id, const xn_ue_data_t *data);
bool xn_exists_ue_data(uint32_t xn_ue_id);
xn_ue_data_t xn_get_ue_data(uint32_t xn_ue_id);

/* Target-side table: keyed on t_ng_node_ue_xnap_id */
uint32_t xn_alloc_target_ue_id(void);
bool xn_add_target_ue_data(uint32_t t_xn_ue_id, const xn_target_ue_data_t *data);

#endif /* XNAP_IDS_H_ */
