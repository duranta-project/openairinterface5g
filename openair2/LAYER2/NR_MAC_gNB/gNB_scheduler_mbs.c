/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief gNB MBS multicast scheduling: point-to-multipoint (PTM) transmissions of MBS groups on a G-RNTI with DCI format 4_1
 * in the Type-3 CSS of the CFR (TS 38.300 §16.10.5, TS 38.213 §18)
 *
 * A broadcast session (TS 38.300 §16.10.6) is scheduled the same way, with two differences that follow from its
 * receivers being unknown to the gNB: it has no members, so it is transmitted whether or not anybody is connected, and
 * it has a fixed MCS. INTERIM: it still uses DCI format 4_1 and the HARQ processes of the unicast HARQ entity, where
 * TS 38.213 §18 has DCI format 4_0 and TS 38.321 §5.3.2.1 a broadcast HARQ process of its own.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include "assertions.h"
#include "nr_mac_gNB.h"
#include "mac_proto.h"
#include "nr_radio_config.h"
#include "common/utils/LOG/log.h"
#include "common/utils/nr/nr_common.h"
#include "openair2/LAYER2/nr_rlc/nr_rlc_oai_api.h"

/* DCI format 4_1 has a 4-bit HARQ process number (TS 38.212 §7.3.1.5.2) */
#define NR_MBS_MAX_PTM_HARQ 16
/* entries of dl-DataToUL-ACK, indicated by the 3-bit PDSCH-to-HARQ_feedback timing indicator of DCI format 4_1 */
#define NR_MBS_MAX_DL_DATA_TO_UL_ACK 8

nr_mbs_group_t *find_mbs_group(const NR_UEs_t *UEs, rnti_t g_rnti)
{
  for (nr_mbs_group_t *const *g = UEs->mbs_group_list; *g; g++) {
    if ((*g)->g_rnti == g_rnti)
      return *g;
  }
  return NULL;
}

static bool is_mbs_member(const nr_mbs_group_t *group, rnti_t rnti)
{
  for (int i = 0; i < group->num_members; i++) {
    if (group->member[i] == rnti)
      return true;
  }
  return false;
}

/* Keeps the PTM HARQ processes of a group out of the unicast scheduling of a UE: G-RNTI and C-RNTI share the DL HARQ
 * entity of the UE (TS 38.321 §5.3.2.1). Free processes are reserved now, busy ones when their unicast transport block
 * is finished. Returns false if the UE does not have these processes */
static bool nr_mac_reserve_ptm_harq(const nr_mbs_group_t *group, NR_UE_info_t *UE)
{
  NR_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
  if ((sched_ctrl->ptm_dl_harq_mask & group->harq_mask) == group->harq_mask)
    return true;
  if (group->harq_mask >> sched_ctrl->available_dl_harq.len)
    return false;
  sched_ctrl->ptm_dl_harq_mask |= group->harq_mask;
  sched_ctrl->ptm_pdsch_frame = MAX_FRAME_NUMBER;
  for (int pid = 0; pid < sched_ctrl->available_dl_harq.len; pid++) {
    const NR_UE_harq_t *harq = &sched_ctrl->harq_processes[pid];
    if ((group->harq_mask & (1U << pid)) && !harq->is_waiting && harq->round == 0)
      remove_nr_list(&sched_ctrl->available_dl_harq, pid);
  }
  return true;
}

/* gives the PTM HARQ processes of a group back to the unicast scheduling of a UE */
static void nr_mac_release_ptm_harq(const nr_mbs_group_t *group, NR_UE_info_t *UE)
{
  NR_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
  if (!(sched_ctrl->ptm_dl_harq_mask & group->harq_mask))
    return;
  sched_ctrl->ptm_dl_harq_mask &= ~group->harq_mask;
  for (int pid = 0; pid < sched_ctrl->available_dl_harq.len; pid++) {
    const NR_UE_harq_t *harq = &sched_ctrl->harq_processes[pid];
    if ((group->harq_mask & (1U << pid)) && !harq->is_waiting && harq->round == 0)
      add_tail_nr_list(&sched_ctrl->available_dl_harq, pid);
  }
}

/** @brief Establishes the context of an MBS multicast session in a cell, as done in the gNB-DU by Multicast Context
 * Setup (TS 38.473 §8.14.6): PTM scheduling context and PTM RLC entity of the MRB. The gNB-DU allocates the G-RNTI in
 * the RNTI space of the cell (TS 38.321 Table 7.1-1), so it is never given to a UE. Must be called with the scheduler
 * lock held.
 * @return the new group, or NULL if it cannot be created */
nr_mbs_group_t *nr_mac_add_mbs_group(gNB_MAC_INST *mac, nr_cell_sched_t *cell, const nr_mbs_group_config_t *config)
{
  NR_UEs_t *UE_info = &mac->UE_info;
  if (cell->beam_info.beam_allocation) {
    LOG_E(NR_MAC, "MBS PTM transmissions are not supported with analog beamforming\n");
    return NULL;
  }
  const int num_dlharq = min(cell->radio_config.num_dlharq, NR_MBS_MAX_PTM_HARQ);
  if (config->num_harq < 1 || config->num_harq >= num_dlharq) {
    LOG_E(NR_MAC, "cannot reserve %d of %d DL HARQ processes for PTM transmissions\n", config->num_harq, num_dlharq);
    return NULL;
  }
  int idx = 0;
  while (idx < NR_MAX_MBS_GROUPS && UE_info->mbs_group_list[idx])
    idx++;
  if (idx == NR_MAX_MBS_GROUPS) {
    LOG_E(NR_MAC, "cannot add more than %d MBS groups\n", NR_MAX_MBS_GROUPS);
    return NULL;
  }
  rnti_t g_rnti;
  if (!nr_mac_get_new_rnti(UE_info, &g_rnti)) {
    LOG_E(NR_MAC, "no free RNTI for a G-RNTI\n");
    return NULL;
  }

  nr_mbs_group_t *group = calloc_or_fail(1, sizeof(*group));
  group->g_rnti = g_rnti;
  group->config = *config;
  group->cell = cell;
  group->last_harq_pid = -1;
  /* the highest HARQ process numbers are reserved, the lowest ones are kept for unicast */
  for (int pid = num_dlharq - config->num_harq; pid < num_dlharq; pid++)
    group->harq_mask |= 1U << pid;
  group->ptm_rlc_bearer = get_MRB_PTM_RLC_BearerConfig(config->lcid, config->mrb_id, &mac->rlc_config);
  /* the RLC entity of a broadcast MRB has a 6-bit SN, the only one MRB-RLC-ConfigBroadcast can announce
   * (TS 38.331 §6.3.2) */
  if (config->broadcast)
    *group->ptm_rlc_bearer->rlc_Config->choice.um_Uni_Directional_DL->dl_UM_RLC.sn_FieldLength = NR_SN_FieldLengthUM_size6;
  nr_rlc_add_mrb(g_rnti, group->ptm_rlc_bearer);
  if (!cell->mbs_search_space) {
    cell->mbs_coreset = get_mbs_coreset_config(cell->common_channels.ServingCellConfigCommon, &cell->radio_config);
    cell->mbs_search_space = get_mbs_search_space_config(cell->mbs_coreset, &cell->radio_config);
  }
  UE_info->mbs_group_list[idx] = group;
  LOG_I(NR_MAC,
        "added MBS %s G-RNTI %04x: LCID %d, HARQ feedback %s, PTM HARQ processes 0x%04x\n",
        config->broadcast ? "broadcast session" : "group",
        g_rnti,
        config->lcid,
        config->harq_feedback == NR_MBS_HARQ_FEEDBACK_DISABLED ? "disabled" : "ACK/NACK",
        group->harq_mask);
  return group;
}

/** @brief Removes the PTM scheduling context of an MBS multicast session (Multicast Context Release,
 * TS 38.473 §8.14.7). Must be called with the scheduler lock held. */
void nr_mac_release_mbs_group(gNB_MAC_INST *mac, rnti_t g_rnti)
{
  NR_UEs_t *UE_info = &mac->UE_info;
  int idx = 0;
  while (idx < NR_MAX_MBS_GROUPS && UE_info->mbs_group_list[idx] && UE_info->mbs_group_list[idx]->g_rnti != g_rnti)
    idx++;
  if (idx == NR_MAX_MBS_GROUPS || !UE_info->mbs_group_list[idx]) {
    LOG_W(NR_MAC, "no MBS group with G-RNTI %04x\n", g_rnti);
    return;
  }
  nr_mbs_group_t *group = UE_info->mbs_group_list[idx];
  while (group->num_members > 0) {
    NR_UE_info_t *UE = find_connected_nr_ue(UE_info, group->member[group->num_members - 1]);
    if (UE) {
      nr_mac_remove_mbs_member(mac, group, UE);
    } else {
      group->num_members--;
    }
  }
  if (group->config.broadcast) {
    FOR_EACH_CONNECTED_UE(UE_info, UE) {
      nr_mac_release_ptm_harq(group, UE);
    }
  }
  for (int pid = 0; pid < NR_MAX_HARQ_PROCESSES; pid++)
    free_transportBlock_buffer(&group->harq[pid].transportBlock);
  nr_rlc_remove_ue(g_rnti);
  ASN_STRUCT_FREE(asn_DEF_NR_RLC_BearerConfig, group->ptm_rlc_bearer);
  /* keep the list compact, the last element is always NULL */
  for (; idx < NR_MAX_MBS_GROUPS; idx++)
    UE_info->mbs_group_list[idx] = UE_info->mbs_group_list[idx + 1];
  LOG_I(NR_MAC, "released MBS group G-RNTI %04x\n", g_rnti);
  free(group);
}

/** @brief Makes a UE receive the PTM transmissions of a group, after the gNB-CU indicated that the UE joined the
 * multicast session (Multicast MBS Session List in UE Context Setup/Modification, TS 38.473 §9.3.1.272). The G-RNTI, CFR
 * and PTM RLC entity are added to the CellGroupConfig of the UE, sent in an RRCReconfiguration. The PTM HARQ processes
 * of the group are reserved in the UE: as G-RNTI and C-RNTI share the DL HARQ entity of the UE (TS 38.321 §5.3.2.1),
 * they must not be used by unicast transmissions. Must be called with the scheduler lock held.
 * @return false if the UE cannot join now, e.g., during another reconfiguration */
bool nr_mac_add_mbs_member(gNB_MAC_INST *mac, nr_mbs_group_t *group, NR_UE_info_t *UE)
{
  NR_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
  if (is_mbs_member(group, UE->rnti))
    return true;
  if (UE->reconfigCellGroup || UE->cm_info.trigger_info != NO_TRIGGER) {
    LOG_D(NR_MAC, "UE %04x: reconfiguration ongoing, cannot join MBS group G-RNTI %04x now\n", UE->rnti, group->g_rnti);
    return false;
  }
  if (UE->pcell != group->cell) {
    LOG_E(NR_MAC, "UE %04x not served in the cell of MBS group G-RNTI %04x\n", UE->rnti, group->g_rnti);
    return false;
  }
  /* the CFR is configured in the initial DL BWP (TS 38.213 §18: no locationAndBandwidthMulticast) */
  if (UE->current_DL_BWP.bwp_id != 0) {
    LOG_E(NR_MAC, "UE %04x: MBS CFR only configured in the initial DL BWP\n", UE->rnti);
    return false;
  }
  if (group->harq_mask >> sched_ctrl->available_dl_harq.len) {
    LOG_E(NR_MAC,
          "UE %04x has %d DL HARQ processes, PTM uses 0x%04x\n",
          UE->rnti,
          sched_ctrl->available_dl_harq.len,
          group->harq_mask);
    return false;
  }
  DevAssert(group->num_members < MAX_MOBILES_PER_GNB);
  NR_CellGroupConfig_t *cellGroup_for_UE = NULL;
  asn_copy(&asn_DEF_NR_CellGroupConfig, (void **)&cellGroup_for_UE, UE->CellGroup);
  add_mbs_cellGroupConfig(cellGroup_for_UE,
                          group->g_rnti,
                          group->config.config_id,
                          group->config.harq_feedback == NR_MBS_HARQ_FEEDBACK_ACK_NACK,
                          group->cell->mbs_coreset,
                          group->cell->mbs_search_space,
                          group->ptm_rlc_bearer);
  UE->cm_info.trigger_info = MBS_JOIN;
  nr_mac_request_reconfiguration(mac, UE, cellGroup_for_UE);
  group->member[group->num_members++] = UE->rnti;
  sched_ctrl->ptm_dl_harq_mask |= group->harq_mask;
  sched_ctrl->ptm_pdsch_frame = MAX_FRAME_NUMBER;
  /* free processes are reserved now, busy ones when their unicast transport block is finished */
  for (int pid = 0; pid < sched_ctrl->available_dl_harq.len; pid++) {
    const NR_UE_harq_t *harq = &sched_ctrl->harq_processes[pid];
    if ((group->harq_mask & (1U << pid)) && !harq->is_waiting && harq->round == 0)
      remove_nr_list(&sched_ctrl->available_dl_harq, pid);
  }
  LOG_I(NR_MAC, "UE %04x joined MBS group G-RNTI %04x (%d members)\n", UE->rnti, group->g_rnti, group->num_members);
  return true;
}

/** @brief Stops PTM transmissions of a group towards a UE, and gives the reserved HARQ processes back to unicast.
 * Must be called with the scheduler lock held. */
void nr_mac_remove_mbs_member(gNB_MAC_INST *mac, nr_mbs_group_t *group, NR_UE_info_t *UE)
{
  UNUSED(mac);
  int i = 0;
  while (i < group->num_members && group->member[i] != UE->rnti)
    i++;
  if (i == group->num_members)
    return;
  group->member[i] = group->member[--group->num_members];
  NR_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
  sched_ctrl->ptm_dl_harq_mask &= ~group->harq_mask;
  /* the HARQ-ACK of PTM transmissions is not awaited anymore, PTP retransmissions go on as unicast ones */
  for (int pid = 0; pid < sched_ctrl->available_dl_harq.len; pid++) {
    NR_UE_harq_t *harq = &sched_ctrl->harq_processes[pid];
    if (harq->ptm_group != group)
      continue;
    remove_nr_list(&sched_ctrl->feedback_dl_harq, pid);
    harq->ptm_group = NULL;
    harq->is_waiting = false;
    harq->round = 0;
  }
  for (int pid = 0; pid < sched_ctrl->available_dl_harq.len; pid++) {
    const NR_UE_harq_t *harq = &sched_ctrl->harq_processes[pid];
    if ((group->harq_mask & (1U << pid)) && !harq->is_waiting && harq->round == 0)
      add_tail_nr_list(&sched_ctrl->available_dl_harq, pid);
  }
  LOG_I(NR_MAC, "UE %04x left MBS group G-RNTI %04x (%d members)\n", UE->rnti, group->g_rnti, group->num_members);
}

/* a PTM HARQ process can be used once every member has reserved it and finished any unicast transport block on it */
static int get_ptm_harq_pid(const nr_mbs_group_t *group, NR_UE_info_t *const *members, int n_members, int last_pid)
{
  for (int i = 1; i <= NR_MBS_MAX_PTM_HARQ; i++) {
    const int pid = (last_pid + i) % NR_MBS_MAX_PTM_HARQ;
    if (!(group->harq_mask & (1U << pid)))
      continue;
    bool idle = true;
    for (int m = 0; m < n_members && idle; m++) {
      const NR_UE_sched_ctrl_t *sched_ctrl = &members[m]->UE_sched_ctrl;
      const NR_UE_harq_t *harq = &sched_ctrl->harq_processes[pid];
      idle = (sched_ctrl->ptm_dl_harq_mask & (1U << pid)) && !harq->is_waiting && harq->round == 0;
    }
    if (idle)
      return pid;
  }
  return -1;
}

/* the group shares the free RBs with the unicast UEs of the cell that have DL data or retransmissions pending, as one
 * more flow, so that PTM traffic cannot starve unicast traffic (e.g., SRB1 of the members) */
static int get_num_dl_active_ues(NR_UEs_t *UE_info, const nr_cell_sched_t *cell)
{
  int n = 0;
  FOR_EACH_CONNECTED_UE(UE_info, UE) {
    const NR_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
    if (UE->pcell == cell && (sched_ctrl->num_total_bytes > 0 || sched_ctrl->retrans_dl_harq.head >= 0))
      n++;
  }
  return n;
}

static bool rbs_free(const uint16_t *vrb_map, int start, int num, uint16_t slbitmap)
{
  for (int rb = start; rb < start + num; rb++) {
    if (vrb_map[rb] & slbitmap)
      return false;
  }
  return true;
}

/* largest free block of VRBs within the first rb_max RBs of the CFR whose start and size are multiples of K
 * (TS 38.214 §5.1.2.2.3). Returns the size, and the start relative to the CFR in *rb_start */
static int get_ptm_rb_block(const uint16_t *vrb_map, int cfr_start, int rb_max, int K, uint16_t slbitmap, int *rb_start)
{
  int best_size = 0;
  int start = 0;
  while (start + K <= rb_max) {
    int end = start;
    while (end + K <= rb_max && rbs_free(vrb_map, cfr_start + end, K, slbitmap))
      end += K;
    if (end - start > best_size) {
      best_size = end - start;
      *rb_start = start;
    }
    start = end + K;
  }
  return best_size;
}

/* MAC PDU of the G-RNTI: MAC SDUs of the MTCH with their subheaders, then padding (TS 38.321 §6.1.2). MAC CEs are UE
 * specific and never sent with a G-RNTI. Returns the number of MAC SDU bytes */
static int nr_mbs_generate_mac_pdu(gNB_MAC_INST *mac, const nr_mbs_group_t *group, uint8_t *buf, uint32_t TBS)
{
  uint8_t *bufEnd = buf + TBS;
  tb_size_t pdu_siz[MAX_NUM_DATA_REQ];
  const logical_chan_id_t lcid = group->config.lcid;
  int num = nr_mac_rlc_multi_data_req(mac->Mod_id, group->g_rnti, true, lcid, TBS, (char *)buf, pdu_siz, sizeofArray(pdu_siz));
  int sdu_bytes = 0;
  for (int i = 0; i < num; i++) {
    NR_MAC_SUBHEADER_LONG *header = (NR_MAC_SUBHEADER_LONG *)buf;
    header->R = 0;
    header->F = 1;
    header->LCID = lcid;
    header->L = htons(pdu_siz[i]);
    buf += pdu_siz[i] + sizeof(NR_MAC_SUBHEADER_LONG);
    sdu_bytes += pdu_siz[i];
  }
  if (bufEnd - buf > 0) {
    NR_MAC_SUBHEADER_FIXED *padding = (NR_MAC_SUBHEADER_FIXED *)buf;
    padding->R = 0;
    padding->LCID = DL_SCH_LCID_PADDING;
    buf++;
    memset(buf, 0, bufEnd - buf);
  }
  return sdu_bytes;
}

/* PUCCH of the HARQ-ACK of a PTM PDSCH in every member, in the multicast HARQ-ACK codebook of the G-RNTI. The
 * PDSCH-to-HARQ_feedback timing indicator, PUCCH resource indicator and counter DAI are in the DCI format 4_1, hence
 * common to the members (TS 38.213 §9.1.3, §9.2.3). Returns the timing indicator, or -1, and the PUCCH structures of the
 * members in pucch_index */
static int nr_mbs_acknack_scheduling(nr_cell_sched_t *cell,
                                     const nr_mbs_group_t *group,
                                     NR_UE_info_t *const *members,
                                     int n_members,
                                     frame_t frame,
                                     slot_t slot,
                                     int cce_index,
                                     int pucch_index[])
{
  int r_pucch[n_members];
  for (int m = 0; m < n_members; m++)
    r_pucch[m] = nr_get_pucch_resource(cell->mbs_coreset, members[m]->current_UL_BWP.pucch_Config, cce_index);
  for (int f = 0; f < NR_MBS_MAX_DL_DATA_TO_UL_ACK; f++) {
    bool possible = true;
    int dai = -1;
    for (int m = 0; m < n_members && possible; m++) {
      const int idx = nr_acknack_scheduling_ptm(cell, members[m], frame, slot, r_pucch[m], f, group->g_rnti, false);
      if (idx < 0) {
        possible = false;
        break;
      }
      const NR_sched_pucch_t *pucch = &members[m]->UE_sched_ctrl.sched_pucch[idx];
      const int member_dai = pucch->active && pucch->g_rnti_mc == group->g_rnti ? pucch->dai_c_mc : 0;
      possible = dai < 0 || member_dai == dai;
      dai = member_dai;
    }
    if (!possible)
      continue;
    for (int m = 0; m < n_members; m++) {
      pucch_index[m] = nr_acknack_scheduling_ptm(cell, members[m], frame, slot, r_pucch[m], f, group->g_rnti, true);
      DevAssert(pucch_index[m] >= 0);
    }
    return f;
  }
  return -1;
}

static void nr_mbs_fill_pdcch_pdsch(nr_cell_sched_t *cell,
                                    post_process_pdsch_t *pp_pdsch,
                                    const nr_mbs_group_t *group,
                                    const NR_sched_pdsch_t *sched_pdsch,
                                    const NR_sched_pucch_t *pucch,
                                    NR_sched_pdcch_t *sched_pdcch,
                                    NR_ControlResourceSet_t *coreset,
                                    int aggregation_level,
                                    int cce_index,
                                    int beam_idx,
                                    int pdu_index)
{
  NR_ServingCellConfigCommon_t *scc = cell->common_channels.ServingCellConfigCommon;
  NR_SearchSpace_t *ss = cell->mbs_search_space;
  const nr_mbs_harq_t *harq = &group->harq[sched_pdsch->dl_harq_pid];
  nfapi_nr_dl_tti_request_body_t *dl_req = pp_pdsch->dl_req;

  nfapi_nr_dl_tti_pdcch_pdu_rel15_t *pdcch_pdu = cell->pdcch_pdu_idx[coreset->controlResourceSetId];
  if (!pdcch_pdu) {
    nfapi_nr_dl_tti_request_pdu_t *dl_tti_pdcch_pdu = &dl_req->dl_tti_pdu_list[dl_req->nPDUs];
    memset(dl_tti_pdcch_pdu, 0, sizeof(*dl_tti_pdcch_pdu));
    dl_tti_pdcch_pdu->PDUType = NFAPI_NR_DL_TTI_PDCCH_PDU_TYPE;
    dl_tti_pdcch_pdu->PDUSize = (uint8_t)(2 + sizeof(nfapi_nr_dl_tti_pdcch_pdu));
    dl_req->nPDUs += 1;
    pdcch_pdu = &dl_tti_pdcch_pdu->pdcch_pdu.pdcch_pdu_rel15;
    nr_configure_pdcch(pdcch_pdu, coreset, sched_pdcch);
    cell->pdcch_pdu_idx[coreset->controlResourceSetId] = pdcch_pdu;
  }

  nfapi_nr_dl_tti_request_pdu_t *dl_tti_pdsch_pdu = &dl_req->dl_tti_pdu_list[dl_req->nPDUs];
  memset(dl_tti_pdsch_pdu, 0, sizeof(*dl_tti_pdsch_pdu));
  dl_tti_pdsch_pdu->PDUType = NFAPI_NR_DL_TTI_PDSCH_PDU_TYPE;
  dl_tti_pdsch_pdu->PDUSize = (uint8_t)(2 + sizeof(nfapi_nr_dl_tti_pdsch_pdu));
  dl_req->nPDUs += 1;

  /* TS 38.212 §5.4.2.1: LBRM with one layer (no maxMIMO-Layers in pdsch-ConfigMulticast) over the CFR */
  const int nl_tbslbrm = 1;
  const uint16_t fapi_beam = convert_to_fapi_beam(beam_idx, &cell->beam_info);
  nfapi_nr_dl_tti_pdsch_pdu_rel15_t *pdsch_pdu = prepare_pdsch_pdu(dl_tti_pdsch_pdu,
                                                                   cell,
                                                                   NULL,
                                                                   sched_pdsch,
                                                                   NULL,
                                                                   false,
                                                                   harq->round,
                                                                   group->g_rnti,
                                                                   fapi_beam,
                                                                   nl_tbslbrm,
                                                                   pdu_index);

  nfapi_nr_dl_dci_pdu_t *dci_pdu = prepare_dci_pdu(pdcch_pdu,
                                                   scc,
                                                   ss,
                                                   coreset,
                                                   sched_pdsch->ant_port_idx.spatialStreamIndices,
                                                   aggregation_level,
                                                   cce_index,
                                                   fapi_beam,
                                                   group->g_rnti);
  pdcch_pdu->numDlDci++;

  /* TS 38.214 §5.1.2.2.3: RIV of RB'start = RBstart / K and L'RBs = LRBs / K relative to the CFR, sized on CORESET 0 */
  const int K = get_multicast_type1_rb_granularity(sched_pdsch->bwp_info.bwpSize, cell->cset0_bwp_size);
  dci_pdu_rel15_t dci_payload = {0};
  dci_payload.frequency_domain_assignment.val =
      PRBalloc_to_locationandbandwidth0(sched_pdsch->rbSize / K, sched_pdsch->rbStart / K, cell->cset0_bwp_size);
  dci_payload.time_domain_assignment.val = sched_pdsch->time_domain_allocation;
  dci_payload.vrb_to_prb_mapping.val = 0;
  dci_payload.mcs = sched_pdsch->mcs;
  dci_payload.ndi = harq->ndi;
  dci_payload.rv = pdsch_pdu->rvIndex[0];
  dci_payload.harq_pid.val = sched_pdsch->dl_harq_pid;
  /* without harq-FeedbackEnablerMulticast, no HARQ-ACK (TS 38.213 §18) and DAI, PRI and K1 are not used */
  dci_payload.dai[0].val = pucch ? (pucch->dai_c_mc - 1) & 3 : 0;
  dci_payload.pucch_resource_indicator = pucch ? pucch->resource_indicator : 0;
  dci_payload.pdsch_to_harq_feedback_timing_indicator.val = pucch ? pucch->timing_indicator : 0;
  fill_dci_pdu_rel15(NULL,
                     NULL,
                     NULL,
                     dci_pdu,
                     &dci_payload,
                     NR_DL_DCI_FORMAT_4_1,
                     TYPE_G_RNTI_,
                     0,
                     ss,
                     coreset,
                     0,
                     cell->cset0_bwp_size);
}

static void nr_schedule_mbs_group(gNB_MAC_INST *mac, nr_cell_sched_t *cell, post_process_pdsch_t *pp_pdsch, nr_mbs_group_t *group)
{
  const frame_t frame = pp_pdsch->frame;
  const slot_t slot = pp_pdsch->slot;
  NR_ServingCellConfigCommon_t *scc = cell->common_channels.ServingCellConfigCommon;

  NR_UE_info_t *members[MAX_MOBILES_PER_GNB];
  int n_members = 0;
  /* a member receives PTM transmissions once the CellGroupConfig with the G-RNTI is applied, i.e., its RRCReconfiguration
   * acknowledged */
  for (int i = 0; i < group->num_members; i++) {
    NR_UE_info_t *UE = find_connected_nr_ue(&mac->UE_info, group->member[i]);
    if (UE && nr_mac_ue_is_active(UE) && !UE->reconfigCellGroup)
      members[n_members++] = UE;
  }
  if (group->config.broadcast) {
    /* A broadcast session has no members: it is announced in the cell, and transmitted whether or not anybody is
     * connected. The connected UEs of the cell are still looked at, as any of them may be receiving it: the PTM HARQ
     * processes are kept out of their unicast scheduling, and they get no unicast PDSCH in the slot of a PTM one */
    FOR_EACH_CONNECTED_UE(&mac->UE_info, UE) {
      if (UE->pcell == cell && nr_mac_ue_is_active(UE) && nr_mac_reserve_ptm_harq(group, UE))
        members[n_members++] = UE;
    }
  } else if (n_members == 0) {
    return;
  }

  const logical_chan_id_t lcid = group->config.lcid;
  mac_rlc_status_resp_t rlc_status;
  nr_mac_rlc_status_ind(group->g_rnti, frame, 1, &lcid, &rlc_status);
  group->pending_bytes = rlc_status.bytes_in_buffer;
  if (group->pending_bytes == 0)
    return;

  const int pid = get_ptm_harq_pid(group, members, n_members, group->last_harq_pid);
  if (pid < 0) {
    LOG_D(NR_MAC, "%4d.%2d G-RNTI %04x: no PTM HARQ process free in all members\n", frame, slot, group->g_rnti);
    return;
  }

  /* single beam: transmitted on the beam of the first SSB as other common channels */
  const int n_slots_frame = cell->frame_structure.numb_slots_frame;
  const NR_COMMON_channels_t *cc = &cell->common_channels;
  const int beam_index = get_beam_from_ssbidx(cell, cc->num_active_ssb > 0 ? cc->ssb_index[0] : 0);
  NR_beam_alloc_t beam = beam_allocation_procedure(&cell->beam_info, frame, slot, beam_index, n_slots_frame);
  if (beam.idx < 0)
    return;

  NR_SearchSpace_t *ss = cell->mbs_search_space;
  NR_ControlResourceSet_t *coreset = cell->mbs_coreset;
  NR_BWP_t *initial_dl_bwp = &scc->downlinkConfigCommon->initialDownlinkBWP->genericParameters;
  NR_sched_pdcch_t sched_pdcch = set_pdcch_structure(cell, ss, coreset, scc, initial_dl_bwp, cell->type0_PDCCH_CSS_config);
  int aggregation_level = 0;
  const int cce_index = get_cce_index(cell, slot, group->g_rnti, &aggregation_level, beam.idx, ss, coreset, &sched_pdcch, 0.0f);
  if (cce_index < 0) {
    LOG_D(NR_MAC, "%4d.%2d G-RNTI %04x: no free CCE for DCI format 4_1\n", frame, slot, group->g_rnti);
    reset_beam_status(&cell->beam_info, frame, slot, beam_index, n_slots_frame, beam.new_beam);
    return;
  }

  /* TS 38.214 Table 5.1.2.1.1-1: pdsch-TimeDomainAllocationList of PDSCH-ConfigCommon for G-RNTI in a Type-3 CSS */
  NR_PDSCH_TimeDomainResourceAllocationList_t *tdalist =
      scc->downlinkConfigCommon->initialDownlinkBWP->pdsch_ConfigCommon->choice.setup->pdsch_TimeDomainAllocationList;
  const int tda = get_dl_tda(mac, cell, slot);
  NR_tda_info_t tda_info = set_tda_info_from_list(tdalist, tda);
  DevAssert(tda_info.valid_tda);
  NR_pdsch_dmrs_t dmrs_parms = get_dl_dmrs_params(scc, NULL, &tda_info, 1);
  const uint16_t slbitmap = SL_to_bitmap(tda_info.startSymbolIndex, tda_info.nrOfSymbols);

  /* the CFR is the initial DL BWP; DCI format 4_1 addresses its first K * N_CORESET0 RBs */
  const bwp_info_t cfr = {.bwpStart = NRRIV2PRBOFFSET(initial_dl_bwp->locationAndBandwidth, MAX_BWP_SIZE),
                          .bwpSize = NRRIV2BW(initial_dl_bwp->locationAndBandwidth, MAX_BWP_SIZE)};
  const int K = get_multicast_type1_rb_granularity(cfr.bwpSize, cell->cset0_bwp_size);
  uint16_t *vrb_map = cell->common_channels.vrb_map[beam.idx];
  int rb_start = 0;
  const int free_rbs = get_ptm_rb_block(vrb_map, cfr.bwpStart, min(cfr.bwpSize, K * cell->cset0_bwp_size), K, slbitmap, &rb_start);
  const int max_rbs = free_rbs / (1 + get_num_dl_active_ues(&mac->UE_info, cell)) / K * K;
  if (max_rbs == 0) {
    reset_beam_status(&cell->beam_info, frame, slot, beam_index, n_slots_frame, beam.new_beam);
    return;
  }

  const int mcs = mac->mbs_mcs_select(cell, group, members, n_members);
  const int mcs_table = 0;
  NR_sched_pdsch_t sched_pdsch = {
      .bwp_info = cfr,
      .time_domain_allocation = tda,
      .tda_info = tda_info,
      .dmrs_parms = dmrs_parms,
      .nrOfLayers = 1,
      .mcs = mcs,
      .Qm = nr_get_Qm_dl(mcs, mcs_table),
      .R = nr_get_code_rate_dl(mcs, mcs_table),
      .alloc_type = PDSCH_TYPE1,
      .rbStart = rb_start,
      .dl_harq_pid = pid,
      .pucch_allocation = -1,
      .ant_port_idx = {.numSpatialStreamIndices = 1, .spatialStreamIndices[0] = cell->radio_config.spatial_stream_index[beam.idx]},
  };
  /* the RLC buffer status includes the RLC headers, add one long MAC subheader per RLC PDU, at least one for the
   * remaining segment of an SDU */
  const uint32_t bytes = group->pending_bytes + sizeof(NR_MAC_SUBHEADER_LONG) * max(1, rlc_status.pdus_in_buffer);
  uint16_t rb_size = 0;
  nr_find_nb_rb(sched_pdsch.Qm,
                sched_pdsch.R,
                1,
                sched_pdsch.nrOfLayers,
                tda_info.nrOfSymbols,
                dmrs_parms.N_PRB_DMRS * dmrs_parms.N_DMRS_SLOT,
                bytes,
                K,
                max_rbs,
                &sched_pdsch.tb_size,
                &rb_size);
  sched_pdsch.rbSize = min((rb_size + K - 1) / K * K, max_rbs);
  sched_pdsch.tb_size = nr_compute_tbs(sched_pdsch.Qm,
                                       sched_pdsch.R,
                                       sched_pdsch.rbSize,
                                       tda_info.nrOfSymbols,
                                       dmrs_parms.N_PRB_DMRS * dmrs_parms.N_DMRS_SLOT,
                                       0, // xOverhead-Multicast not configured (TS 38.214 §5.1.3.2)
                                       0,
                                       sched_pdsch.nrOfLayers)
                        >> 3;

  int pucch_index[n_members + 1];
  const bool harq_feedback = group->config.harq_feedback == NR_MBS_HARQ_FEEDBACK_ACK_NACK;
  if (harq_feedback && nr_mbs_acknack_scheduling(cell, group, members, n_members, frame, slot, cce_index, pucch_index) < 0) {
    LOG_D(NR_MAC, "%4d.%2d G-RNTI %04x: no common PUCCH occasion for the HARQ-ACK of the members\n", frame, slot, group->g_rnti);
    reset_beam_status(&cell->beam_info, frame, slot, beam_index, n_slots_frame, beam.new_beam);
    return;
  }

  fill_pdcch_vrb_map(cell, &sched_pdcch, cce_index, aggregation_level, beam.idx);
  for (int rb = 0; rb < sched_pdsch.rbSize; rb++)
    vrb_map[cfr.bwpStart + sched_pdsch.rbStart + rb] |= slbitmap;

  nr_mbs_harq_t *harq = &group->harq[pid];
  harq->ndi ^= 1;
  harq->round = 0;
  harq->sched_pdsch = sched_pdsch;
  uint8_t *buf = allocate_transportBlock_buffer(&harq->transportBlock, sched_pdsch.tb_size);
  const int sdu_bytes = nr_mbs_generate_mac_pdu(mac, group, buf, sched_pdsch.tb_size);
  group->last_harq_pid = pid;

  /* the members await the HARQ-ACK of the PTM transmission in the HARQ process, for PTP retransmissions of the TB */
  const uint32_t ptm_tbslbrm = nr_compute_tbslbrm(mcs_table, cfr.bwpSize, sched_pdsch.nrOfLayers);
  for (int m = 0; harq_feedback && m < n_members; m++) {
    NR_UE_sched_ctrl_t *sched_ctrl = &members[m]->UE_sched_ctrl;
    NR_UE_harq_t *member_harq = &sched_ctrl->harq_processes[pid];
    const NR_sched_pucch_t *pucch = &sched_ctrl->sched_pucch[pucch_index[m]];
    member_harq->ptm_group = group;
    member_harq->is_waiting = true;
    member_harq->feedback_frame = pucch->frame;
    member_harq->feedback_slot = pucch->ul_slot;
    member_harq->ndi = harq->ndi;
    member_harq->round = 0;
    member_harq->sched_pdsch = sched_pdsch;
    member_harq->ptm_tbslbrm = ptm_tbslbrm;
    add_tail_nr_list(&sched_ctrl->feedback_dl_harq, pid);
  }

  const int pdu_index = cell->pdu_index++;
  nr_mbs_fill_pdcch_pdsch(cell,
                          pp_pdsch,
                          group,
                          &sched_pdsch,
                          harq_feedback ? &members[0]->UE_sched_ctrl.sched_pucch[pucch_index[0]] : NULL,
                          &sched_pdcch,
                          coreset,
                          aggregation_level,
                          cce_index,
                          beam.idx,
                          pdu_index);
  fill_dl_tx_request(pp_pdsch, buf, pdu_index, sched_pdsch.tb_size, frame, slot);
  group->ptm_tx++;
  group->ptm_sdu_bytes += sdu_bytes;
  for (int m = 0; m < n_members; m++) {
    members[m]->UE_sched_ctrl.ptm_pdsch_frame = frame;
    members[m]->UE_sched_ctrl.ptm_pdsch_slot = slot;
  }

  LOG_D(NR_MAC,
        "%4d.%2d [PTM] G-RNTI %04x members %d CCE %d L %d RBs %d+%d (K %d) symbols %d+%d MCS %d TBS %d SDU bytes %d "
        "HARQ %d NDI %d\n",
        frame,
        slot,
        group->g_rnti,
        n_members,
        cce_index,
        aggregation_level,
        sched_pdsch.rbStart,
        sched_pdsch.rbSize,
        K,
        tda_info.startSymbolIndex,
        tda_info.nrOfSymbols,
        mcs,
        sched_pdsch.tb_size,
        sdu_bytes,
        pid,
        harq->ndi);
}

/** @brief Handles the HARQ-ACK of a member for a PTM transmission. After a NACK, or no HARQ-ACK, the TB is retransmitted
 * to the member with PTP on its C-RNTI (TS 38.213 §18), as a unicast retransmission in the same HARQ process, of the
 * same NDI: the UE then combines it with the PTM transmission (TS 38.321 §5.3.1) */
void nr_mbs_handle_dl_harq(NR_UE_info_t *UE, int harq_pid, bool success, int harq_round_max)
{
  NR_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
  NR_UE_harq_t *harq = &sched_ctrl->harq_processes[harq_pid];
  nr_mbs_group_t *group = harq->ptm_group;
  harq->ptm_group = NULL;
  harq->feedback_slot = -1;
  harq->is_waiting = false;
  if (success) {
    group->harq_ack++;
    finish_nr_dl_harq(sched_ctrl, harq_pid);
    return;
  }
  group->harq_nack++;
  if (harq_round_max <= 1) {
    abort_nr_dl_harq(UE, harq_pid);
    return;
  }
  /* the PTM TB stays in the HARQ process of the group until every member has finished with it */
  const nr_mbs_harq_t *ptm_harq = &group->harq[harq_pid];
  const uint32_t tb_size = ptm_harq->sched_pdsch.tb_size;
  memcpy(allocate_transportBlock_buffer(&harq->transportBlock, tb_size), ptm_harq->transportBlock.buf, tb_size);
  harq->round++;
  add_tail_nr_list(&sched_ctrl->retrans_dl_harq, harq_pid);
  LOG_D(NR_MAC,
        "UE %04x: NACK for PTM transmission of G-RNTI %04x in HARQ process %d, PTP retransmission\n",
        UE->rnti,
        group->g_rnti,
        harq_pid);
}

/** @brief Schedules PTM transmissions of the MBS groups of the cell in the current DL slot. Called before unicast
 * scheduling, so that group traffic is served first. */
void nr_schedule_mbs(gNB_MAC_INST *mac, nr_cell_sched_t *cell, post_process_pdsch_t *pp_pdsch)
{
  /* already mutex protected: held in gNB_dlsch_ulsch_scheduler() */
  AssertFatal(pthread_mutex_trylock(&mac->sched_lock) == EBUSY, "this function should be called with the scheduler mutex locked\n");
  if (!is_dl_slot(pp_pdsch->slot, &cell->frame_structure))
    return;
  for (nr_mbs_group_t **g = mac->UE_info.mbs_group_list; *g; g++) {
    if ((*g)->cell == cell)
      nr_schedule_mbs_group(mac, cell, pp_pdsch, *g);
  }
}
