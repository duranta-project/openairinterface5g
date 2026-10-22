/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_mac_gNB.h"
#include "mac_proto.h"
#include "nr_mac_cell.h"
#ifdef E3_AGENT
#include "openair2/E3AP/ran_func_spectrum_extern.h"
#endif /* E3_AGENT */

int nr_mac_get_cell_idx(gNB_MAC_INST *mac, nr_cell_sched_t *cell)
{
  int cell_idx = cell - nr_mac_get_cell_by_idx(mac, 0);
  AssertFatal(cell_idx >= 0 && cell_idx < seq_arr_size(&mac->cells), "Invalid cell_idx %d (only %zu cells configured)\n", cell_idx, seq_arr_size(&mac->cells));
  return cell_idx;
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

nr_cell_sched_t *nr_mac_cell_alloc(gNB_MAC_INST *mac, NR_ServingCellConfigCommon_t *scc, const nr_mac_config_t *config)
{
  nr_cell_sched_t new_cell = {0};
  nr_mac_pcch_queue_init(&new_cell.common_channels);
  for (int n = 0; n < MAX_NUM_OF_SSB; n++)
    new_cell.sib1_pdsch[n].time_domain_allocation = -1;
  new_cell.first_MIB = true;
  // Direct seq_arr access is unavoidable here: we own the array and need the
  // stable pointer to the just-appended element.
  seq_arr_push_back(&mac->cells, &new_cell, sizeof(new_cell));
  nr_cell_sched_t *cell = seq_arr_at(&mac->cells, seq_arr_size(&mac->cells) - 1);

  cell->phy_id = *scc->physCellId;
  cell->common_channels.ServingCellConfigCommon = scc;
  cell->radio_config = *config;
  cell->common_channels.mib = get_new_MIB_NR(scc);
  cell->cset0_bwp_start = 0;
  cell->cset0_bwp_size = 0;
  cell->ul_next = (fsn_t){.mu = *scc->ssbSubcarrierSpacing};
  return cell;
}

static void destroy_periodic_sched(periodic_ue_sched_t p)
{
  free(p.list);
}

void nr_mac_cell_free(nr_cell_sched_t *cell)
{
  NR_COMMON_channels_t *cc = &cell->common_channels;
  free(cell->radio_config.ssb_beams);
  nr_mac_pcch_queue_free(cc);
  ASN_STRUCT_FREE(asn_DEF_NR_BCCH_BCH_Message, cc->mib);
  ASN_STRUCT_FREE(asn_DEF_NR_BCCH_DL_SCH_Message, cc->sib1);
  ASN_STRUCT_FREE(asn_DEF_NR_ServingCellConfigCommon, cc->ServingCellConfigCommon);
#ifdef E3_AGENT
  e3_spectrum_mac_detach_cell(cell);
#endif /* E3_AGENT */
  free(cell->UL_tti_req_ahead);

  for (int i = 0; i < MAX_NUM_BEAM_PERIODS; i++)
    free(cc->vrb_map_UL[i]);

  NR_beam_info_t *bi = &cell->beam_info;
  if (bi->beam_allocation) {
    for (int i = 0; i < bi->beams_per_period; i++)
      free(bi->beam_allocation[i]);
    free(bi->beam_allocation);
  }

  if (cc->du_SIBs) {
    seq_arr_free(cc->du_SIBs, NULL);
    free(cc->du_SIBs);
  }
  destroy_periodic_sched(cell->periodic_srs_config);
}
