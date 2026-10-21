/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NGAP_GNB_ERROR_INDICATION_H_
#define NGAP_GNB_ERROR_INDICATION_H_

#include <stdbool.h>
#include <stdint.h>
#include "ngap_messages_types.h"

/** @brief NGAP Error Indication (TS 38.413 clause 8.7.5) */
typedef struct ngap_error_indication_s {
  /* AMF-UE-NGAP-ID (optional) */
  bool has_amf_ue_ngap_id;
  uint64_t amf_ue_ngap_id;
  /* RAN-UE-NGAP-ID (optional) */
  bool has_ran_ue_ngap_id;
  uint32_t ran_ue_ngap_id;
  /* Cause (optional) */
  bool has_cause;
  ngap_cause_t cause;
  /* Criticality Diagnostics (optional) */
  bool has_criticality_diagnostics;
} ngap_error_indication_t;

NGAP_NGAP_PDU_t *encode_ng_error_indication(const ngap_error_indication_t *msg);
bool decode_ng_error_indication(ngap_error_indication_t *out, const NGAP_NGAP_PDU_t *pdu);
bool eq_ng_error_indication(const ngap_error_indication_t *a, const ngap_error_indication_t *b);

#endif /* NGAP_GNB_ERROR_INDICATION_H_ */
