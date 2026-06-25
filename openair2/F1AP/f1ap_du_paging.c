/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "f1ap_common.h"
#include "f1ap_du_paging.h"
#include "lib/f1ap_paging.h"
#include "conversions.h"
#include "oai_asn1.h"
#include "openair2/RRC/LTE/rrc_proto.h"
#include "common/utils/LOG/log.h"
#include "openair2/LAYER2/NR_MAC_gNB/mac_rrc_dl_handler.h"
#include "F1AP_F1AP-PDU.h"

/** @brief Handle F1AP Paging message at DU.
 *
 * Decodes the F1AP Paging PDU and, for CN UE identity (5G-S-TMSI), builds the
 * NR RRC Paging message (PCCH, TS 38.331 §8.2) and hands it to MAC for queuing,
 * together with the UE_ID (ue_identity_index_value mod 1024) for PF/PO (TS 38.304 §7).
 * MAC will pull the PDU at paging occasion and schedule it.
 * RAN UE paging identity is not supported.
 *
 * @param instance  DU instance (used as module_id for MAC).
 * @param assoc_id  SCTP association (unused).
 * @param stream    Stream (unused).
 * @param pdu       F1AP Paging PDU to decode and process.
 * @return 0 on success, -1 on decode failure. */
int DU_handle_Paging(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  f1ap_paging_t decoded = {0};

  DevAssert(pdu);
  UNUSED(assoc_id);
  UNUSED(stream);

  if (LOG_DEBUGFLAG(DEBUG_ASN1)) {
    xer_fprint(stdout, &asn_DEF_F1AP_F1AP_PDU, pdu);
  }

  if (!decode_f1ap_paging(&decoded, pdu)) {
    LOG_E(F1AP, "Failed to decode F1AP Paging\n");
    return -1;
  }

  LOG_I(F1AP, "[DU %ld] Decoded F1AP Paging: identity_type=%d, n_cells=%u\n", instance, decoded.identity_type, decoded.n_cells);

  /* Build PCCH from F1AP Paging and enqueue it in MAC (TS 38.331 §8.2). */
  f1_paging(&decoded);

  free_f1ap_paging(&decoded);
  return 0;
}
