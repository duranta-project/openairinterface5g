/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "NR_MAC_gNB/nr_mac_gNB.h"
#include "NR_MAC_gNB/nr_mac_cell.h"
#include "NR_MAC_gNB/mac_proto.h"

int nr_mac_get_cell_idx(gNB_MAC_INST *mac, nr_cell_sched_t *cell)
{
  return cell - nr_mac_get_cell_by_idx(mac, 0);
}

nr_cell_sched_t *nr_mac_get_cell_by_idx(gNB_MAC_INST *mac, uint16_t idx)
{
  AssertFatal(idx < seq_arr_size(&mac->cells), "Invalid idx %d (only %zu cells configured)\n", idx, seq_arr_size(&mac->cells));
  return (nr_cell_sched_t *)seq_arr_at(&mac->cells, idx);
}

nr_cell_sched_t *nr_mac_get_cell_by_phy_id(gNB_MAC_INST *mac, uint16_t phy_id)
{
  FOR_EACH_SEQ_ARR(nr_cell_sched_t *, cell, &mac->cells) {
    if (cell->phy_id == phy_id)
      return cell;
  }
  AssertFatal(false, "No cell with phy_id %d\n", phy_id);
}

//TS 38.473 §9.3.1.12 NR CGI defined by PLMN Identity and NR Cell Identity
nr_cell_sched_t *nr_mac_get_cell_by_cgi(gNB_MAC_INST *mac, plmn_id_t plmn, uint64_t nr_cellid)
{
  FOR_EACH_SEQ_ARR(nr_cell_sched_t *, cell, &mac->cells) {
    if (cell->nr_cellid == nr_cellid && cell->plmn.mcc == plmn.mcc && cell->plmn.mnc == plmn.mnc
        && cell->plmn.mnc_digit_length == plmn.mnc_digit_length)
      return cell;
  }
  return NULL;
}
