/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* PLMN selection helper kept in its own translation unit so unit tests can link
 * it without dragging the ITTI/task dependencies of nr_nas_msg.c. */

#include <string.h>
#include "nr_nas_msg.h"

bool nas_get_selected_plmn(const nr_ue_nas_t *nas, const plmn_id_t *plmns, int num_plmns, long *selected_plmn_identity)
{
  if (!nas || !nas->uicc || !nas->uicc->imsiStr || !plmns || !selected_plmn_identity)
    return false;
  if (num_plmns <= 0 || num_plmns > PLMN_LIST_MAX_SIZE)
    return false;
  const char *imsi = nas->uicc->imsiStr;
  const int mnc_len = nas->uicc->nmc_size; /* 2 or 3 */
  if ((mnc_len != 2 && mnc_len != 3) || strlen(imsi) < (size_t)(3 + mnc_len))
    return false;
  const uint16_t ue_mcc = (imsi[0] - '0') * 100 + (imsi[1] - '0') * 10 + (imsi[2] - '0');
  const uint16_t ue_mnc = (mnc_len == 3) ? (imsi[3] - '0') * 100 + (imsi[4] - '0') * 10 + (imsi[5] - '0')
                                         : (imsi[3] - '0') * 10 + (imsi[4] - '0');
  for (int k = 0; k < num_plmns; k++) {
    if (plmns[k].mnc_digit_length == mnc_len && plmns[k].mcc == ue_mcc && plmns[k].mnc == ue_mnc) {
      *selected_plmn_identity = k + 1; /* 1-based per TS 38.331 */
      return true;
    }
  }
  return false;
}
