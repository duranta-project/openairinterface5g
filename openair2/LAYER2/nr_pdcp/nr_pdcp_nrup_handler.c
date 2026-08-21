/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_pdcp_nrup_handler.h"
#include "nr_up/nr_up_pdcp_if.h"
#include "common/utils/LOG/log.h"
#include "assertions.h"

void nr_pdcp_dl_transfer_gnb(ue_id_t ue_id, uint8_t drb_id, const byte_array_t *pdu, int sdu_id)
{
  DevAssert(pdu && pdu->buf && pdu->len > 0);
  const nr_up_dl_transfer_req_t req = {
      .ue_id = ue_id,
      .drb_id = drb_id,
      .sdu_id = sdu_id,
      .pdu = *pdu,
  };
  const nr_up_dl_transfer_result_t rc = nr_up_dl_transfer(&req);
  if (rc == NR_UP_DL_OK) {
    return;
  }
  LOG_W(PDCP, "%s(): (drb %u) nr-up DL transfer failed (%d)\n", __func__, drb_id, rc);
}

void nr_pdcp_deliver_pdu_drb(void *data, ue_id_t ue_id, int rb_id, char *buf, int size, int sdu_id)
{
  UNUSED(data);
  const byte_array_t pdu = {
      .buf = (uint8_t *)buf,
      .len = size,
  };
  nr_pdcp_dl_transfer_gnb(ue_id, rb_id, &pdu, sdu_id);
}
