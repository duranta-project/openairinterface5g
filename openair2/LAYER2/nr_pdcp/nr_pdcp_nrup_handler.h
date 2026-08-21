/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_PDCP_NRUP_HANDLER_H
#define NR_PDCP_NRUP_HANDLER_H

#include "common/platform_types.h"
#include "common/utils/ds/byte_array.h"

void nr_pdcp_dl_transfer_gnb(ue_id_t ue_id, uint8_t drb_id, const byte_array_t *pdu, int sdu_id);
void nr_pdcp_deliver_pdu_drb(void *data, ue_id_t ue_id, int rb_id, char *buf, int size, int sdu_id);

#endif /* NR_PDCP_NRUP_HANDLER_H */
