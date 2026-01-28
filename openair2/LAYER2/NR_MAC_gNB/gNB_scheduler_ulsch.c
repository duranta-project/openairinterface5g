/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/*! \file gNB_scheduler_ulsch.c
 * \brief gNB procedures for the ULSCH transport channel
 * \author Navid Nikaein and Raymond Knopp, Guido Casati
 * \date 2019
 * \email: guido.casati@iis.fraunhofer.de
 * \version 1.0
 * @ingroup _mac
 */


#include "LAYER2/NR_MAC_gNB/mac_proto.h"
#include "executables/softmodem-common.h"
#include "common/utils/nr/nr_common.h"
#include "utils.h"
#include <openair2/UTIL/OPT/opt.h>
#include "LAYER2/nr_rlc/nr_rlc_oai_api.h"

//#define SRS_IND_DEBUG
#define MAX_NUM_DATA_IND 1024

/* \brief Get the number of UL TDAs that could be used in slot, reachable
 * via specific k2. The output parameter first_idx is a pointer to the first
 * suitable TDA, and the function returns the number of suitable TDAs, or 0. */
int get_num_ul_tda(gNB_MAC_INST *nrmac, int slot, int k2, const NR_tda_info_t **first_idx)
{
  /* we assume that this function is mutex-protected from outside */
  NR_SCHED_ENSURE_LOCKED(&nrmac->sched_lock);

  const frame_structure_t *fs = &nrmac->frame_structure;
  const int slot_period = slot % fs->numb_slots_period;
  const tdd_bitmap_t *bm = &fs->period_cfg.tdd_slot_bitmap[slot_period];
  /* For some reason, we only store the number of symbols if it's mixed */
  const int num_ul_symbols = bm->slot_type == TDD_NR_MIXED_SLOT ? bm->num_ul_symbols : 14;
  const uint16_t ul_bitmap = SL_to_bitmap(14 - num_ul_symbols, num_ul_symbols);

  *first_idx = NULL;
  FOR_EACH_SEQ_ARR(NR_tda_info_t *, tda, &nrmac->ul_tda) {
    DevAssert(tda->valid_tda);
    // nr_rrc_config_ul_tda() orders by k2, so skip smaller and return for
    // bigger ones
    if (tda->k2 < k2)
      continue;
    if (tda->k2 > k2)
      break; // there won't be a suitable k2 anymore

    uint16_t tda_bitmap = SL_to_bitmap(tda->startSymbolIndex, tda->nrOfSymbols);
    // nr_rrc_config_ul_tda() assures TDAs are from largest to smallest symbol number.
    // check that this TDA fits entirely into this slot's mask (slot might be
    // smaller in mixed slots)., A mixed slot TDA would be checked last, so a
    // full slot TDA will be used for a full UL slot.
    if ((tda_bitmap & ul_bitmap) == tda_bitmap) { // if TDA fits entiry
      *first_idx = tda;
      break;
    }
  }
  if (*first_idx == NULL) /* nothing fit */
    return 0;

  NR_tda_info_t *end_it = seq_arr_next(&nrmac->ul_tda, *first_idx);
  while (end_it != seq_arr_end(&nrmac->ul_tda) && end_it->k2 == k2) {
    /* the following TDAs should all fit as long as the k2 is the same */
    uint16_t tda_bitmap = SL_to_bitmap(end_it->startSymbolIndex, end_it->nrOfSymbols);
    AssertFatal((tda_bitmap & ul_bitmap) == tda_bitmap,
                "TDA should fit inside slot, but is not the case for k2 %ld bitmap 0x%04x\n",
                end_it->k2,
                tda_bitmap);
    end_it = seq_arr_next(&nrmac->ul_tda, end_it);
  }

  ptrdiff_t diff = seq_arr_dist(&nrmac->ul_tda, *first_idx, end_it);
  AssertFatal(diff > 0 && diff <= seq_arr_size(&nrmac->ul_tda), "dist %ld\n", diff);
  return diff;
}

static void get_max_rb_range(const uint16_t *vrb_map_ul, const uint16_t *ulprbbl, uint16_t mask, int *rb_start, int *rb_len)
{
  int best_start = *rb_start;
  int best_len = 0;
  int current_start = *rb_start;
  int current_len = 0;
  for (int rb = *rb_start; rb < *rb_len; ++rb) {
    if (ulprbbl[rb] == 0 && (mask & vrb_map_ul[rb]) == 0) {
      current_len++;
      continue;
    }

    /* this RB is blocked for UL, or already in use */
    if (current_len > best_len) {
      best_start = current_start;
      best_len = current_len;
    }
    current_start = rb + 1; /* will start in next RB, or updated later */
    current_len = 0;
  }
  if (current_len > best_len) {
    best_start = current_start;
    best_len = current_len;
  }
  *rb_start = best_start;
  *rb_len = best_len;
}

const NR_tda_info_t *get_best_ul_tda(const gNB_MAC_INST *nrmac, int beam, const NR_tda_info_t *tdas, int n_tda, int frame, int slot, int *rb_start, int *rb_len)
{
  /* there is a mixed slot only when in TDD */
  const frame_structure_t *fs = &nrmac->frame_structure;
  const int index = ul_buffer_index(frame, slot, fs->numb_slots_frame, nrmac->vrb_map_UL_size);
  uint16_t *vrb_map_UL = &nrmac->common_channels[0].vrb_map_UL[beam][index * MAX_BWP_SIZE];

  DevAssert(n_tda <= 16);
  const NR_tda_info_t *best_tda = tdas;
  uint64_t score = 0;
  int check_rb_start = *rb_start;
  int check_rb_len = *rb_len;

  for (int i = 0; i < n_tda; ++i, tdas++) {
    int start = check_rb_start;
    int len = check_rb_len;
    uint16_t tda_mask = SL_to_bitmap(tdas->startSymbolIndex, tdas->nrOfSymbols);
    get_max_rb_range(vrb_map_UL, nrmac->ulprbbl, tda_mask, &start, &len);
    uint64_t s = (uint64_t)tdas->nrOfSymbols * len;
    if (s > score) {
      best_tda = tdas;
      score = s;
      *rb_start = start;
      *rb_len = len;
    }
    LOG_D(NR_MAC, "%4d.%2d tda k2 %ld mask 0x%04x has PRB start/len %d/%d score %ld\n", frame, slot, tdas->k2, tda_mask, start, len, s);
  }

  return best_tda;
}

bwp_info_t get_pusch_bwp_start_size(NR_UE_info_t *UE)
{
  NR_UE_UL_BWP_t *ul_bwp = &UE->current_UL_BWP;
  NR_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
  bwp_info_t bwp_info;
  bwp_info.bwpStart = ul_bwp->BWPStart;

  // 3GPP TS 38.214 Section 6.1.2.2.2 Uplink resource allocation type 1
  // In uplink resource allocation of type 1, the resource block assignment information indicates to a scheduled UE a set of
  // contiguously allocated non-interleaved virtual resource blocks within the active bandwidth part of size   PRBs except for the
  // case when DCI format 0_0 is decoded in any common search space in which case the size of the initial UL bandwidth part shall
  // be used.
  if (ul_bwp->dci_format == NR_UL_DCI_FORMAT_0_0
      && sched_ctrl->search_space->searchSpaceType
      && sched_ctrl->search_space->searchSpaceType->present == NR_SearchSpace__searchSpaceType_PR_common) {
    bwp_info.bwpSize = min(ul_bwp->BWPSize, UE->sc_info.initial_ul_BWPSize);
  } else {
    bwp_info.bwpSize = ul_bwp->BWPSize;
  }
  return bwp_info;
}

static int compute_ph_factor(int mu, int tbs_bits, int rb, int n_layers, int n_symbols, int n_dmrs, long *deltaMCS, bool include_bw)
{
  // 38.213 7.1.1
  // if the PUSCH transmission is over more than one layer delta_tf = 0
  float delta_tf = 0;
  if(deltaMCS != NULL && n_layers == 1) {
    const int n_re = (NR_NB_SC_PER_RB * n_symbols - n_dmrs) * rb;
    const float BPRE = (float) tbs_bits/n_re;  //TODO change for PUSCH with CSI
    const float f = pow(2, BPRE * 1.25);
    const float beta = 1.0f; //TODO change for PUSCH with CSI
    delta_tf = (10 * log10((f - 1) * beta));
    LOG_D(NR_MAC,
          "PH factor delta_tf %f (n_re %d, n_rb %d, n_dmrs %d, n_symbols %d, tbs %d BPRE %f f %f)\n",
          delta_tf,
          n_re,
          rb,
          n_dmrs,
          n_symbols,
          tbs_bits,
          BPRE,
          f);
  }
  const float bw_factor = (include_bw) ? 10 * log10(rb << mu) : 0;
  return ((int)roundf(delta_tf + bw_factor));
}

/* \brief over-estimate the BSR index, given real_index.
 *
 * BSR does not account for headers, so we need to estimate. See 38.321
 * 6.1.3.1: "The size of the RLC headers and MAC subheaders are not considered
 * in the buffer size computation." */
static int overestim_bsr_index(int real_index)
{
  /* if UE reports BSR 0, it means "no data"; otherwise, overestimate to
   * account for headers */
  const int add_overestim = 1;
  return real_index > 0 ? real_index + add_overestim : real_index;
}

static int estimate_ul_buffer_short_bsr(const NR_BSR_SHORT *bsr)
{
  /* NOTE: the short BSR might be for different LCGID than 0, but we do not
   * differentiate them */
  int rep_idx = bsr->Buffer_size;
  int estim_idx = overestim_bsr_index(rep_idx);
  int max = NR_SHORT_BSR_TABLE_SIZE - 1;
  int idx = min(estim_idx, max);
  int estim_size = get_short_bsr_value(idx);
  LOG_D(NR_MAC, "short BSR LCGID %d index %d estim index %d size %d\n", bsr->LcgID, rep_idx, estim_idx, estim_size);
  return estim_size;
}

static int estimate_ul_buffer_long_bsr(const NR_BSR_LONG *bsr)
{
  LOG_D(NR_MAC,
        "LONG BSR, LCG ID(7-0) %d/%d/%d/%d/%d/%d/%d/%d\n",
        bsr->LcgID7,
        bsr->LcgID6,
        bsr->LcgID5,
        bsr->LcgID4,
        bsr->LcgID3,
        bsr->LcgID2,
        bsr->LcgID1,
        bsr->LcgID0);
  bool bsr_active[8] = {bsr->LcgID0 != 0, bsr->LcgID1 != 0, bsr->LcgID2 != 0, bsr->LcgID3 != 0, bsr->LcgID4 != 0, bsr->LcgID5 != 0, bsr->LcgID6 != 0, bsr->LcgID7 != 0};

  int estim_size = 0;
  int max = NR_LONG_BSR_TABLE_SIZE - 1;
  uint8_t *payload = ((uint8_t*) bsr) + 1;
  int m = 0;
  const int total_lcgids = 8; /* see 38.321 6.1.3.1 */
  for (int n = 0; n < total_lcgids; n++) {
    if (!bsr_active[n])
      continue;
    int rep_idx = payload[m];
    int estim_idx = overestim_bsr_index(rep_idx);
    int idx = min(estim_idx, max);
    estim_size += get_long_bsr_value(idx);

    LOG_D(NR_MAC, "LONG BSR LCGID/m %d/%d Index %d estim index %d size %d", n, m, rep_idx, estim_idx, estim_size);
    m++;
  }
  return estim_size;
}

//  For both UL-SCH except:
//   - UL-SCH: fixed-size MAC CE(known by LCID)
//   - UL-SCH: padding
//   - UL-SCH: MSG3 48-bits
//  |0|1|2|3|4|5|6|7|  bit-wise
//  |R|F|   LCID    |
//  |       L       |
//  |0|1|2|3|4|5|6|7|  bit-wise
//  |R|F|   LCID    |
//  |       L       |
//  |       L       |
//
//  For:
//   - UL-SCH: fixed-size MAC CE(known by LCID)
//   - UL-SCH: padding, for single/multiple 1-oct padding CE(s)
//   - UL-SCH: MSG3 48-bits
//  |0|1|2|3|4|5|6|7|  bit-wise
//  |R|R|   LCID    |
//
//  LCID: The Logical Channel ID field identifies the logical channel instance of the corresponding MAC SDU or the type of the corresponding MAC CE or padding as described in Tables 6.2.1-1 and 6.2.1-2 for the DL-SCH and UL-SCH respectively. There is one LCID field per MAC subheader. The LCID field size is 6 bits;
//  L: The Length field indicates the length of the corresponding MAC SDU or variable-sized MAC CE in bytes. There is one L field per MAC subheader except for subheaders corresponding to fixed-sized MAC CEs and padding. The size of the L field is indicated by the F field;
//  F: length of L is 0:8 or 1:16 bits wide
//  R: Reserved bit, set to zero.

// return: length of subPdu header
// 3GPP TS 38.321 Section 6
uint8_t decode_ul_mac_sub_pdu_header(uint8_t *pduP, uint8_t *lcid, uint16_t *length)
{
  uint16_t mac_subheader_len = 1;
  *lcid = pduP[0] & 0x3F;

  switch (*lcid) {
    case UL_SCH_LCID_CCCH_64_BITS:
      *length = 8;
      break;
    case UL_SCH_LCID_SRB1:
    case UL_SCH_LCID_SRB2:
    case UL_SCH_LCID_DTCH ...(UL_SCH_LCID_DTCH + 28):
    case UL_SCH_LCID_L_TRUNCATED_BSR:
    case UL_SCH_LCID_L_BSR:
      if (pduP[0] & 0x40) { // F = 1
        mac_subheader_len = 3;
        *length = (pduP[1] << 8) + pduP[2];
      } else { // F = 0
        mac_subheader_len = 2;
        *length = pduP[1];
      }
      break;
    case UL_SCH_LCID_CCCH_48_BITS_REDCAP:
    case UL_SCH_LCID_CCCH_48_BITS:
      *length = 6;
      break;
    case UL_SCH_LCID_SINGLE_ENTRY_PHR:
    case UL_SCH_LCID_C_RNTI:
      *length = 2;
      break;
    case UL_SCH_LCID_S_TRUNCATED_BSR:
    case UL_SCH_LCID_S_BSR:
      *length = 1;
      break;
    case UL_SCH_LCID_PADDING:
      // Nothing to do
      break;
    default:
      LOG_E(NR_MAC, "LCID %0x not handled yet!\n", *lcid);
      break;
  }

  LOG_D(NR_MAC, "Decoded LCID 0x%X, header bytes: %d, payload bytes %d\n", *lcid, mac_subheader_len, *length);

  return mac_subheader_len;
}

static rnti_t lcid_crnti_lookahead(uint8_t *pdu, uint32_t pdu_len)
{
  while (pdu_len > 0) {
    uint16_t mac_len = 0;
    uint8_t lcid = 0;
    uint16_t mac_subheader_len = decode_ul_mac_sub_pdu_header(pdu, &lcid, &mac_len);
    // Check for valid PDU
    if (mac_subheader_len + mac_len > pdu_len) {
      LOG_E(NR_MAC,
            "Invalid PDU! mac_subheader_len: %d, mac_len: %d, remaining pdu_len: %d\n",
            mac_subheader_len,
            mac_len,
            pdu_len);

      LOG_E(NR_MAC, "Residual UL MAC PDU: ");
      uint32_t print_len = pdu_len > 30 ? 30 : pdu_len; // Only printf 1st - 30nd bytes
      for (int i = 0; i < print_len; i++)
        printf("%02x ", pdu[i]);
      printf("\n");
      return 0;
    }

    if (lcid == UL_SCH_LCID_C_RNTI) {
      // Extract C-RNTI value
      rnti_t crnti = ((pdu[1] & 0xFF) << 8) | (pdu[2] & 0xFF);
      LOG_A(NR_MAC, "Received a MAC CE for C-RNTI with %04x\n", crnti);
      return crnti;
    } else if (lcid == UL_SCH_LCID_PADDING) {
      // End of MAC PDU, can ignore the remaining bytes
      return 0;
    }

    pdu += mac_len + mac_subheader_len;
    pdu_len -= mac_len + mac_subheader_len;
  }
  return 0;
}

static int nr_process_mac_pdu(instance_t module_idP,
                              NR_UE_info_t *UE,
                              uint8_t CC_id,
                              frame_t frameP,
                              slot_t slot,
                              uint8_t *pduP,
                              uint32_t pdu_len,
                              const int8_t harq_pid)
{
  int sdus = 0;
  NR_UE_UL_BWP_t *ul_bwp = &UE->current_UL_BWP;
  NR_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;

  if (pduP[0] != UL_SCH_LCID_PADDING) {
    ws_trace_t tmp = {.nr = true,
                      .direction = DIRECTION_UPLINK,
                      .pdu_buffer = pduP,
                      .pdu_buffer_size = pdu_len,
                      .ueid = 0,
                      .rntiType = WS_C_RNTI,
                      .rnti = UE->rnti,
                      .sysFrame = frameP,
                      .subframe = slot,
                      .harq_pid = harq_pid};
    trace_pdu(&tmp);
  }

#ifdef ENABLE_MAC_PAYLOAD_DEBUG
  LOG_I(NR_MAC, "In %s: dumping MAC PDU in %d.%d:\n", __func__, frameP, slot);
  log_dump(NR_MAC, pduP, pdu_len, LOG_DUMP_CHAR, "\n");
#endif

  nr_rlc_data_ind_t data_ind[MAX_NUM_DATA_IND] = {0};
  int num_data_ind = 0;
  while (pdu_len > 0) {
    uint16_t mac_len = 0;
    uint8_t lcid = 0;

    uint16_t mac_subheader_len = decode_ul_mac_sub_pdu_header(pduP, &lcid, &mac_len);
    // Check for valid PDU
    if (mac_subheader_len + mac_len > pdu_len) {
      LOG_E(NR_MAC,
            "Invalid PDU in %d.%d for RNTI %04X! mac_subheader_len: %d, mac_len: %d, remaining pdu_len: %d\n",
            frameP,
            slot,
            UE->rnti,
            mac_subheader_len,
            mac_len,
            pdu_len);

      LOG_E(NR_MAC, "Residual UL MAC PDU: ");
      int print_len = pdu_len > 30 ? 30 : pdu_len; // Only printf 1st - 30nd bytes
      for (int i = 0; i < print_len; i++)
        printf("%02x ", pduP[i]);
      printf("\n");
      break;
    }

    LOG_D(NR_MAC,
          "Received UL-SCH sub-PDU with LCID 0x%X in %d.%d for RNTI %04X (remaining PDU length %d)\n",
          lcid,
          frameP,
          slot,
          UE->rnti,
          pdu_len);

    unsigned char *ce_ptr;

    switch (lcid) {
      case UL_SCH_LCID_CCCH_64_BITS:
      case UL_SCH_LCID_CCCH_48_BITS_REDCAP:
      case UL_SCH_LCID_CCCH_48_BITS:
        if (lcid == UL_SCH_LCID_CCCH_64_BITS) {
          // Check if it is a valid CCCH1 message, we get all 00's messages very often
          bool valid_pdu = false;
          for (int i = 0; i < (mac_subheader_len + mac_len); i++) {
            if (pduP[i] != 0) {
              valid_pdu = true;
              break;
            }
          }
          if (!valid_pdu) {
            LOG_D(NR_MAC, "%s() Invalid CCCH1 message!, pdu_len: %d\n", __func__, pdu_len);
            return 0;
          }
        }

        LOG_I(MAC, "[RAPROC] Received SDU for CCCH length %d for UE %04x\n", mac_len, UE->rnti);

        if (lcid == UL_SCH_LCID_CCCH_48_BITS_REDCAP) {
          LOG_I(MAC, "UE with RNTI %04x is RedCap\n", UE->rnti);
          UE->is_redcap = true;
        }

        if (prepare_initial_ul_rrc_message(RC.nrmac[module_idP], UE)) {
          nr_rlc_data_ind_t ind = {.ch = 0, .buf = pduP + mac_subheader_len, .len = mac_len};
          data_ind[num_data_ind++] = ind;
          DevAssert(num_data_ind < MAX_NUM_DATA_IND);
        } else {
          LOG_E(NR_MAC, "prepare_initial_ul_rrc_message() returned false, cannot forward CCCH message\n");
        }
        break;

      case UL_SCH_LCID_SRB1:
      case UL_SCH_LCID_SRB2:
        AssertFatal(UE->CellGroup,
                    "UE %04x %d.%d: Received LCID %d which is not configured (UE has no CellGroup)\n",
                    UE->rnti,
                    frameP,
                    slot,
                    lcid);

        const nr_lc_config_t *srbc = nr_mac_get_lc_config(sched_ctrl, lcid);
        if (!srbc || srbc->suspended) {
          LOG_I(NR_MAC, "RNTI %04x LCID %d: ignoring %d bytes\n", UE->rnti, lcid, mac_len);
        } else {
          nr_rlc_data_ind_t ind = {.ch = lcid, .buf = pduP + mac_subheader_len, .len = mac_len};
          data_ind[num_data_ind++] = ind;
          DevAssert(num_data_ind < MAX_NUM_DATA_IND);

          UE->mac_stats.ul.total_sdu_bytes += mac_len;
          UE->mac_stats.ul.lc_bytes[lcid] += mac_len;
        }
        T(T_GNB_MAC_LCID_UL, T_INT(UE->rnti), T_INT(frameP), T_INT(slot), T_INT(lcid), T_INT(mac_len * 8));
        break;

      case UL_SCH_LCID_DTCH ...(UL_SCH_LCID_DTCH + 28):
        LOG_D(NR_MAC,
              "[UE %04x] %d.%d : ULSCH -> UL-%s %d (gNB %ld, %d bytes)\n",
              UE->rnti,
              frameP,
              slot,
              lcid < 4 ? "DCCH" : "DTCH",
              lcid,
              module_idP,
              mac_len);
        const nr_lc_config_t *c = nr_mac_get_lc_config(sched_ctrl, lcid);
        if (!c || c->suspended) {
          LOG_I(NR_MAC, "RNTI %04x LCID %d: ignoring %d bytes\n", UE->rnti, lcid, mac_len);
        } else {
          UE->mac_stats.ul.lc_bytes[lcid] += mac_len;

          nr_rlc_data_ind_t ind = {.ch = lcid, .buf = pduP + mac_subheader_len, .len = mac_len};
          data_ind[num_data_ind++] = ind;
          DevAssert(num_data_ind < MAX_NUM_DATA_IND);

          sdus += 1;
          /* Updated estimated buffer when receiving data */
          if (sched_ctrl->estimated_ul_buffer >= mac_len)
            sched_ctrl->estimated_ul_buffer -= mac_len;
          else
            sched_ctrl->estimated_ul_buffer = 0;
        }
        T(T_GNB_MAC_LCID_UL, T_INT(UE->rnti), T_INT(frameP), T_INT(slot), T_INT(lcid), T_INT(mac_len * 8));
        break;

      case UL_SCH_LCID_RECOMMENDED_BITRATE_QUERY:
        // 38.321 Ch6.1.3.20
        break;

      case UL_SCH_LCID_MULTI_ENTRY_PHR_4_OCT:
        LOG_E(NR_MAC, "Multi entry PHR not supported\n");
        break;

      case UL_SCH_LCID_CONFIGURED_GRANT_CONFIRMATION:
        // 38.321 Ch6.1.3.7
        break;

      case UL_SCH_LCID_MULTI_ENTRY_PHR_1_OCT:
        LOG_E(NR_MAC, "Multi entry PHR not supported\n");
        break;

      case UL_SCH_LCID_SINGLE_ENTRY_PHR:
        if (harq_pid < 0) {
          LOG_E(NR_MAC, "Invalid HARQ PID %d\n", harq_pid);
          continue;
        }
        NR_sched_pusch_t *sched_pusch = &sched_ctrl->ul_harq_processes[harq_pid].sched_pusch;

        /* Extract SINGLE ENTRY PHR elements for PHR calculation */
        ce_ptr = &pduP[mac_subheader_len];
        NR_SINGLE_ENTRY_PHR_MAC_CE *phr = (NR_SINGLE_ENTRY_PHR_MAC_CE *)ce_ptr;
        /* Save the phr info */
        int PH;
        const int PCMAX = phr->PCMAX;
        /* 38.133 Table10.1.17.1-1 */
        if (phr->PH < 55) {
          PH = phr->PH - 32;
        } else if (phr->PH < 63) {
          PH = 24 + (phr->PH - 55) * 2;
        } else {
          PH = 38;
        }
        // in sched_ctrl we set normalized PH wrt MCS and PRBs
        long *deltaMCS = ul_bwp->pusch_Config ? ul_bwp->pusch_Config->pusch_PowerControl->deltaMCS : NULL;
        sched_ctrl->ph = PH
                         + compute_ph_factor(ul_bwp->scs,
                                             sched_pusch->tb_size << 3,
                                             sched_pusch->rbSize,
                                             sched_pusch->nrOfLayers,
                                             sched_pusch->tda_info.nrOfSymbols, // n_symbols
                                             sched_pusch->dmrs_info.num_dmrs_symb * sched_pusch->dmrs_info.N_PRB_DMRS, // n_dmrs
                                             deltaMCS,
                                             true);
        sched_ctrl->ph0 = PH;
        /* 38.133 Table10.1.18.1-1 */
        sched_ctrl->pcmax = PCMAX - 29;
        LOG_D(NR_MAC,
              "SINGLE ENTRY PHR %d.%d R1 %d PH %d (%d dB) R2 %d PCMAX %d (%d dBm)\n",
              frameP,
              slot,
              phr->R1,
              PH,
              sched_ctrl->ph,
              phr->R2,
              PCMAX,
              sched_ctrl->pcmax);
        break;

      case UL_SCH_LCID_C_RNTI:
        if (UE->ra && UE->ra->ra_state == nrRA_gNB_IDLE) {
          // Extract C-RNTI value
          rnti_t crnti = ((pduP[1] & 0xFF) << 8) | (pduP[2] & 0xFF);
          AssertFatal(false,
                      "Received MAC CE for C-RNTI %04x without RA running, procedure exists? Or is it a bug while decoding the "
                      "MAC PDU?\n",
                      crnti);
        }
        break;

      case UL_SCH_LCID_S_TRUNCATED_BSR:
      case UL_SCH_LCID_S_BSR:
        /* Extract short BSR value */
        ce_ptr = &pduP[mac_subheader_len];
        sched_ctrl->estimated_ul_buffer = estimate_ul_buffer_short_bsr((NR_BSR_SHORT *)ce_ptr);
        sched_ctrl->sched_ul_bytes = 0;
        LOG_D(NR_MAC, "SHORT BSR at %4d.%2d, est buf %d\n", frameP, slot, sched_ctrl->estimated_ul_buffer);
        break;

      case UL_SCH_LCID_L_TRUNCATED_BSR:
      case UL_SCH_LCID_L_BSR:
        /* Extract long BSR value */
        ce_ptr = &pduP[mac_subheader_len];
        sched_ctrl->estimated_ul_buffer = estimate_ul_buffer_long_bsr((NR_BSR_LONG *)ce_ptr);
        sched_ctrl->sched_ul_bytes = 0;
        LOG_D(NR_MAC, "LONG BSR at %4d.%2d, estim buf %d\n", frameP, slot, sched_ctrl->estimated_ul_buffer);
        break;

      case UL_SCH_LCID_PADDING:
        // End of MAC PDU, can ignore the rest.
        // 38.321 Sec 6.1.5: "Presence and length of padding is implicit based
        // on TB size, size of MAC subPDU(s)."
        mac_len = pdu_len - mac_subheader_len; // will make pdu_len go to 0
        break;

      default:
        LOG_E(NR_MAC, "RNTI %0x [%d.%d], received unknown MAC header (LCID = 0x%02x)\n", UE->rnti, frameP, slot, lcid);
        mac_len = pdu_len - mac_subheader_len; // will make pdu_len go to 0
        break;
    }


#ifdef ENABLE_MAC_PAYLOAD_DEBUG
    if (lcid < 45 || lcid == 52 || lcid == 63) {
      LOG_I(NR_MAC, "In %s: dumping UL MAC SDU sub-header with length %d (LCID = 0x%02x):\n", __func__, mac_subheader_len, lcid);
      log_dump(NR_MAC, pduP, mac_subheader_len, LOG_DUMP_CHAR, "\n");
      LOG_I(NR_MAC, "In %s: dumping UL MAC SDU with length %d (LCID = 0x%02x):\n", __func__, mac_len, lcid);
      log_dump(NR_MAC, pduP + mac_subheader_len, mac_len, LOG_DUMP_CHAR, "\n");
    } else {
      LOG_I(NR_MAC, "In %s: dumping UL MAC CE with length %d (LCID = 0x%02x):\n", __func__, mac_len, lcid);
      log_dump(NR_MAC, pduP + mac_subheader_len + mac_len, mac_len, LOG_DUMP_CHAR, "\n");
    }
#endif

    pduP += (mac_subheader_len + mac_len);
    pdu_len -= (mac_subheader_len + mac_len);
  }

  nr_mac_rlc_data_ind(module_idP, UE->rnti, true, data_ind, num_data_ind);

  UE->mac_stats.ul.num_mac_sdu += sdus;

  return 0;
}

static void finish_nr_ul_harq(NR_UE_sched_ctrl_t *sched_ctrl, int harq_pid)
{
  NR_UE_ul_harq_t *harq = &sched_ctrl->ul_harq_processes[harq_pid];

  harq->ndi ^= 1;
  harq->round = 0;

  add_tail_nr_list(&sched_ctrl->available_ul_harq, harq_pid);
}

static void abort_nr_ul_harq(NR_UE_info_t *UE, int8_t harq_pid)
{
  NR_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
  NR_UE_ul_harq_t *harq = &sched_ctrl->ul_harq_processes[harq_pid];

  finish_nr_ul_harq(sched_ctrl, harq_pid);
  UE->mac_stats.ul.errors++;

  /* the transmission failed: the UE won't send the data we expected initially,
   * so retrieve to correctly schedule after next BSR */
  sched_ctrl->sched_ul_bytes -= harq->sched_pusch.tb_size;
  if (sched_ctrl->sched_ul_bytes < 0)
    sched_ctrl->sched_ul_bytes = 0;
}

static void handle_nr_ul_harq(gNB_MAC_INST *nrmac,
                              NR_UE_info_t *UE,
                              frame_t frame,
                              slot_t slot,
                              rnti_t rnti,
                              int crc_harq_id,
                              bool crc_status)
{
  if (nrmac->radio_config.disable_harq) {
    LOG_D(NR_MAC, "skipping UL feedback handling as HARQ is disabled\n");
    return;
  }

  NR_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
  int8_t harq_pid = sched_ctrl->feedback_ul_harq.head;
  LOG_D(NR_MAC, "Comparing crc harq_id vs feedback harq_pid = %d %d\n", crc_harq_id, harq_pid);
  while (crc_harq_id != harq_pid || harq_pid < 0) {
    LOG_W(NR_MAC, "Unexpected ULSCH HARQ PID %d (have %d) for RNTI 0x%04x\n", crc_harq_id, harq_pid, rnti);
    if (harq_pid < 0)
      return;

    remove_front_nr_list(&sched_ctrl->feedback_ul_harq);
    sched_ctrl->ul_harq_processes[harq_pid].is_waiting = false;

    if(sched_ctrl->ul_harq_processes[harq_pid].round >= nrmac->ul_bler.harq_round_max - 1) {
      abort_nr_ul_harq(UE, harq_pid);
    } else {
      sched_ctrl->ul_harq_processes[harq_pid].round++;
      add_tail_nr_list(&sched_ctrl->retrans_ul_harq, harq_pid);
    }
    harq_pid = sched_ctrl->feedback_ul_harq.head;
  }
  remove_front_nr_list(&sched_ctrl->feedback_ul_harq);
  NR_UE_ul_harq_t *harq = &sched_ctrl->ul_harq_processes[harq_pid];
  DevAssert(harq->is_waiting);
  harq->feedback_slot = -1;
  harq->is_waiting = false;
  if (!crc_status) {
    finish_nr_ul_harq(sched_ctrl, harq_pid);
    LOG_D(NR_MAC,
          "Ulharq id %d crc passed for RNTI %04x\n",
          harq_pid,
          rnti);
  } else if (harq->round >= nrmac->ul_bler.harq_round_max  - 1) {
    abort_nr_ul_harq(UE, harq_pid);
    LOG_D(NR_MAC,
          "RNTI %04x: Ulharq id %d crc failed in all rounds\n",
          rnti,
          harq_pid);
  } else {
    harq->round++;
    LOG_D(NR_MAC,
          "Ulharq id %d crc failed for RNTI %04x\n",
          harq_pid,
          rnti);
    add_tail_nr_list(&sched_ctrl->retrans_ul_harq, harq_pid);
  }
}

static void handle_msg3_failed_rx(gNB_MAC_INST *mac, NR_RA_t *ra, rnti_t rnti, int harq_round_max)
{
  if (ra->msg3_round >= harq_round_max - 1) {
    LOG_W(NR_MAC, "UE %04x RA failed at state %s (Reached msg3 max harq rounds)\n", rnti, nrra_text[ra->ra_state]);
    nr_release_ra_UE(mac, rnti);
    return;
  }

  LOG_D(NR_MAC, "UE %04x Msg3 CRC did not pass\n", rnti);
  ra->msg3_round++;
  ra->ra_state = nrRA_Msg3_retransmission;
}

static void nr_rx_ra_sdu(const module_id_t mod_id,
                         const int CC_id,
                         const frame_t frame,
                         const sub_frame_t slot,
                         rnti_t rnti,
                         uint8_t *sdu,
                         const uint32_t sdu_len,
                         uint8_t harq_pid,
                         const uint16_t timing_advance,
                         const uint8_t ul_cqi,
                         const uint16_t rssi)
{
  gNB_MAC_INST *mac = RC.nrmac[mod_id];
  NR_ServingCellConfigCommon_t *scc = mac->common_channels[0].ServingCellConfigCommon;
  NR_UE_info_t *UE = find_ra_UE(&mac->UE_info, rnti);
  if (!UE) {
    LOG_E(NR_MAC, "UL SDU discarded. Couldn't finde UE with RNTI %04x \n", rnti);
    return;
  }

  NR_RA_t *ra = UE->ra;
  if (ra->ra_type == RA_4_STEP && ra->ra_state != nrRA_WAIT_Msg3) {
    LOG_W(NR_MAC, "UL SDU discarded for RNTI %04x RA state not waiting for PUSCH\n", UE->rnti);
    return;
  }

  // CFRA: we scheduled Msg3 (which does not exist in CFRA, see also
  // nr_generate_Msg2()). We did not mark RA as complete right away, as the
  // DLSCH scheduler might schedule in the same slot as Msg2 if RLC has data
  // (which can only happen in do-ra), so we mark it as complete now.
  bool cfra = ra->cfra;
  if (ra->cfra) {
    NR_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
    nr_mac_reset_ul_failure(sched_ctrl);
    reset_dl_harq_list(sched_ctrl);
    reset_ul_harq_list(sched_ctrl);
    // we configure the UE using dedicated search space: In SA (CFRA used for
    // handover) and NSA (or do-ra), the UE has the full config already.
    int ss_type = NR_SearchSpace__searchSpaceType_PR_ue_Specific;
    configure_UE_BWP(mac, scc, UE, false, ss_type, -1, -1);
    // initialize ta_frame in case there is no Msg3 received
    UE->UE_sched_ctrl.ta_frame = (frame + 100) % MAX_FRAME_NUMBER;
    if (!transition_ra_connected_nr_ue(mac, UE)) {
      LOG_E(NR_MAC, "cannot add UE %04x: list is full\n", UE->rnti);
      delete_nr_ue_data(UE, NULL, &mac->UE_info.uid_allocator);
    } else {
      LOG_A(NR_MAC, "(rnti 0x%04x) CFRA procedure succeeded!\n", UE->rnti);
    }
  }

  const int target_snrx10 = mac->pusch_target_snrx10;
  if (!sdu) { // NACK
    if (cfra)  // no Msg3 on CFRA, no problem
      return;

    if (ra->ra_state != nrRA_WAIT_Msg3)
      return;

    if((frame!=ra->Msg3_frame) || (slot!=ra->Msg3_slot))
      return;

    if (ul_cqi != 0xff)
      ra->msg3_TPC = nr_get_tpc(target_snrx10, ul_cqi, 30, 0);

    handle_msg3_failed_rx(mac, ra, rnti, mac->ul_bler.harq_round_max);
    return;
  }


  bool no_sig = true;
  for (uint32_t k = 0; k < sdu_len; k++) {
    if (sdu[k] != 0) {
      no_sig = false;
      break;
    }
  }

  T(T_GNB_MAC_UL_PDU_WITH_DATA, T_INT(mod_id), T_INT(CC_id),
    T_INT(rnti), T_INT(frame), T_INT(slot), T_INT(-1) /* harq_pid */,
    T_BUFFER(sdu, sdu_len));


  if (no_sig) {
    LOG_W(NR_MAC, "MSG3 ULSCH with no signal\n");
    if (!cfra)
      handle_msg3_failed_rx(mac, ra, rnti, mac->ul_bler.harq_round_max);
    return;
  }
  if (!cfra && ra->ra_type == RA_2_STEP) {
    // random access pusch with RA-RNTI
    if (ra->RA_rnti != rnti) {
      LOG_E(NR_MAC, "expected TC_RNTI %04x to match current RNTI %04x\n", ra->RA_rnti, rnti);
      return;
    }
  }

  // re-initialize ta update variables after RA procedure completion
  UE->UE_sched_ctrl.ta_frame = (frame + 100) % MAX_FRAME_NUMBER;

  LOG_A(NR_MAC, "%4d.%2d PUSCH with TC_RNTI 0x%04x received correctly\n", frame, slot, rnti);

  NR_UE_sched_ctrl_t *UE_scheduling_control = &UE->UE_sched_ctrl;
  DevAssert(harq_pid >= 0 && harq_pid < 8);
  if (ul_cqi != 0xff) {
    NR_UE_ul_harq_t *harq = &UE_scheduling_control->ul_harq_processes[harq_pid];
    UE_scheduling_control->tpc0 = nr_get_tpc(target_snrx10, ul_cqi, 30, harq->sched_pusch.phr_txpower_calc);
    UE_scheduling_control->pusch_snrx10 = ul_cqi * 5 - 640 - harq->sched_pusch.phr_txpower_calc * 10;
  }
  if (timing_advance != 0xffff)
    UE_scheduling_control->ta_update = timing_advance;
  UE_scheduling_control->raw_rssi = rssi;
  LOG_D(NR_MAC, "[UE %04x] PUSCH TPC %d and TA %d\n", UE->rnti, UE_scheduling_control->tpc0, UE_scheduling_control->ta_update);

  LOG_D(NR_MAC, "[RAPROC] Received %s:\n", ra->ra_type == RA_2_STEP ? "MsgA-PUSCH" : "Msg3");
  for (uint32_t k = 0; k < sdu_len; k++) {
    LOG_D(NR_MAC, "(%i): 0x%x\n", k, sdu[k]);
  }

  // 3GPP TS 38.321 Section 5.4.3 Multiplexing and assembly
  // Logical channels shall be prioritised in accordance with the following order (highest priority listed first):
  // - MAC CE for C-RNTI, or data from UL-CCCH;
  // This way, we need to process MAC CE for C-RNTI if RA is active and it is present in the MAC PDU
  // Search for MAC CE for C-RNTI
  rnti_t crnti = lcid_crnti_lookahead(sdu, sdu_len);
  if (crnti != 0) { // 3GPP TS 38.321 Table 7.1-1: RNTI values, RNTI 0x0000: N/A
    // Replace the current UE by the UE identified by C-RNTI
    NR_UE_info_t *old_UE = find_nr_UE(&mac->UE_info, crnti);
    if (!old_UE) {
      // The UE identified by C-RNTI no longer exists at the gNB
      // Let's abort the current RA, so the UE will trigger a new RA later but using RRCSetupRequest instead. A better
      // solution may be implemented
      LOG_W(NR_MAC, "No UE found with C-RNTI %04x, ignoring Msg3 to have UE come back with new RA attempt\n", UE->rnti);
      nr_release_ra_UE(mac, rnti);
      return;
    }
    // in case UE beam has changed
    old_UE->UE_beam_index = UE->UE_beam_index;
    // Reset UL failure for old UE
    nr_mac_reset_ul_failure(&old_UE->UE_sched_ctrl);
    // Reset HARQ processes
    reset_dl_harq_list(&old_UE->UE_sched_ctrl);
    reset_ul_harq_list(&old_UE->UE_sched_ctrl);

    // Only trigger RRCReconfiguration if UE is not performing RRCReestablishment
    // The RRCReconfiguration will be triggered by the RRCReestablishmentComplete
    if (!old_UE->reconfigCellGroup) {
      LOG_I(NR_MAC, "Received UL_SCH_LCID_C_RNTI with C-RNTI 0x%04x, triggering RRC Reconfiguration\n", crnti);
      // Trigger RRCReconfiguration
      nr_mac_trigger_reconfiguration(mac, old_UE, -1);
      // we configure the UE using common search space with DCIX0 while waiting for a reconfiguration
      configure_UE_BWP(mac, scc, old_UE, false, NR_SearchSpace__searchSpaceType_PR_common, -1, -1);
    }
    nr_release_ra_UE(mac, rnti);
    LOG_A(NR_MAC, "%4d.%2d RA with C-RNTI %04x complete\n", frame, slot, crnti);

    // Decode the entire MAC PDU
    // It may have multiple MAC subPDUs, for example, a MAC subPDU with LCID 1 caring a RRCReestablishmentComplete
    nr_process_mac_pdu(mod_id, old_UE, CC_id, frame, slot, sdu, sdu_len, -1);
    return;
  }

  if (cfra)
    return; // rest not relevant for CFRA

  // UE Contention Resolution Identity
  // Store the first 48 bits belonging to the uplink CCCH SDU within Msg3 to fill in Msg4
  // First byte corresponds to R/LCID MAC sub-header
  memcpy(ra->cont_res_id, &sdu[1], sizeof(uint8_t) * 6);

  // Decode MAC PDU
  // the function is only called to decode the contention resolution sub-header
  // harq_pid set a non-valid value because it is not used in this call
  nr_process_mac_pdu(mod_id, UE, CC_id, frame, slot, sdu, sdu_len, -1);

  LOG_I(NR_MAC,
        "Activating scheduling %s for TC_RNTI 0x%04x (state %s)\n",
        ra->ra_type == RA_2_STEP ? "MsgB" : "Msg4",
        UE->rnti,
        nrra_text[ra->ra_state]);
  ra->ra_state = ra->ra_type == RA_2_STEP ? nrRA_MsgB : nrRA_Msg4;
  LOG_D(NR_MAC, "TC_RNTI 0x%04x next RA state %s\n", UE->rnti, nrra_text[ra->ra_state]);
  return;
}

static void _nr_rx_sdu(const module_id_t gnb_mod_idP,
                       const int CC_idP,
                       const frame_t frameP,
                       const slot_t slotP,
                       const rnti_t rntiP,
                       uint8_t *sduP,
                       const uint32_t sdu_lenP,
                       const int8_t harq_pid,
                       const uint16_t timing_advance,
                       const uint8_t ul_cqi,
                       const uint16_t rssi)
{
  gNB_MAC_INST *gNB_mac = RC.nrmac[gnb_mod_idP];
  const int current_rnti = rntiP;
  LOG_D(NR_MAC, "rx_sdu for rnti %04x\n", current_rnti);
  const int target_snrx10 = gNB_mac->pusch_target_snrx10;
  const int rssi_threshold = gNB_mac->pusch_rssi_threshold;
  const int pusch_failure_thres = gNB_mac->pusch_failure_thres;
  NR_UE_info_t *UE = find_nr_UE(&gNB_mac->UE_info, current_rnti);
  if (UE) {
    NR_UE_sched_ctrl_t *UE_scheduling_control = &UE->UE_sched_ctrl;
    if (sduP)
      T(T_GNB_MAC_UL_PDU_WITH_DATA, T_INT(gnb_mod_idP), T_INT(CC_idP),
        T_INT(rntiP), T_INT(frameP), T_INT(slotP), T_INT(harq_pid),
        T_BUFFER(sduP, sdu_lenP));

    UE->mac_stats.ul.total_bytes += sdu_lenP;
    LOG_D(NR_MAC, "[gNB %d][PUSCH %d] CC_id %d %d.%d Received ULSCH sdu from PHY (rnti %04x) ul_cqi %d TA %d sduP %p, rssi %d\n",
          gnb_mod_idP,
          harq_pid,
          CC_idP,
          frameP,
          slotP,
          current_rnti,
          ul_cqi,
          timing_advance,
          sduP,
          rssi);
    if (harq_pid < 0) {
      LOG_E(NR_MAC, "UE %04x received ULSCH when feedback UL HARQ %d (unexpected ULSCH transmission)\n", rntiP, harq_pid);
      return;
    }

    // if not missed detection (10dB threshold for now)
    if (rssi > 0) {
      int txpower_calc = UE_scheduling_control->ul_harq_processes[harq_pid].sched_pusch.phr_txpower_calc;
      UE->mac_stats.deltaMCS = txpower_calc;
      UE->mac_stats.NPRB = UE_scheduling_control->ul_harq_processes[harq_pid].sched_pusch.rbSize;
      if (ul_cqi != 0xff)
        UE_scheduling_control->tpc0 = nr_get_tpc(target_snrx10, ul_cqi, 30, txpower_calc);
      if (UE_scheduling_control->ph < 0 && UE_scheduling_control->tpc0 > 1)
        UE_scheduling_control->tpc0 = 1;

      UE_scheduling_control->tpc0 = nr_limit_tpc(UE_scheduling_control->tpc0, rssi, rssi_threshold);

      if (timing_advance != 0xffff)
        UE_scheduling_control->ta_update = timing_advance;
      UE_scheduling_control->raw_rssi = rssi;
      UE_scheduling_control->pusch_snrx10 = ul_cqi * 5 - 640 - (txpower_calc * 10);
      if (UE_scheduling_control->tpc0 > 1)
        LOG_D(NR_MAC,
              "[UE %04x] %d.%d. PUSCH TPC %d and TA %d pusch_snrx10 %d rssi %d phrx_tx_power %d PHR (1PRB) %d mcs %d, nb_rb %d\n",
              UE->rnti,
              frameP,
              slotP,
              UE_scheduling_control->tpc0,
              UE_scheduling_control->ta_update,
              UE_scheduling_control->pusch_snrx10,
              UE_scheduling_control->raw_rssi,
              txpower_calc,
              UE_scheduling_control->ph,
              UE_scheduling_control->ul_harq_processes[harq_pid].sched_pusch.mcs,
              UE_scheduling_control->ul_harq_processes[harq_pid].sched_pusch.rbSize);

      NR_UE_ul_harq_t *cur_harq = &UE_scheduling_control->ul_harq_processes[harq_pid];
      if (cur_harq->round == 0)
        UE->mac_stats.pusch_snrx10 = UE_scheduling_control->pusch_snrx10;
      LOG_D(NR_MAC, "[UE %04x] PUSCH TPC %d and TA %d\n",UE->rnti,UE_scheduling_control->tpc0,UE_scheduling_control->ta_update);
    }
    else{
      LOG_D(NR_MAC,"[UE %04x] Detected DTX : increasing UE TX power\n",UE->rnti);
      UE_scheduling_control->tpc0 = 1;
    }

#if defined(ENABLE_MAC_PAYLOAD_DEBUG)

    LOG_I(NR_MAC, "Printing received UL MAC payload at gNB side: %d \n");
    for (uint32_t i = 0; i < sdu_lenP; i++) {
      // harq_process_ul_ue->a[i] = (unsigned char) rand();
      // printf("a[%d]=0x%02x\n",i,harq_process_ul_ue->a[i]);
      printf("%02x ", (unsigned char)sduP[i]);
    }
    printf("\n");

#endif

    if (sduP != NULL) {
      LOG_D(NR_MAC, "Received PDU at MAC gNB \n");
      UE->UE_sched_ctrl.pusch_consecutive_dtx_cnt = 0;
      UE_scheduling_control->sched_ul_bytes -= sdu_lenP;
      if (UE_scheduling_control->sched_ul_bytes < 0)
        UE_scheduling_control->sched_ul_bytes = 0;

      nr_process_mac_pdu(gnb_mod_idP, UE, CC_idP, frameP, slotP, sduP, sdu_lenP, harq_pid);
    } else {
      if (ul_cqi == 0xff || ul_cqi <= 128) {
        UE->UE_sched_ctrl.pusch_consecutive_dtx_cnt++;
        UE->mac_stats.ulsch_DTX++;
      }

      if (!get_softmodem_params()->phy_test && UE->UE_sched_ctrl.pusch_consecutive_dtx_cnt >= pusch_failure_thres) {
        LOG_W(NR_MAC,
              "%4d.%2d UE %04x: Detected UL Failure on PUSCH after %d PUSCH DTX, stopping scheduling\n",
              frameP,
              slotP,
              UE->rnti,
              UE->UE_sched_ctrl.pusch_consecutive_dtx_cnt);
        nr_mac_trigger_ul_failure(&UE->UE_sched_ctrl, UE->current_UL_BWP.scs);
      }
    }
    handle_nr_ul_harq(gNB_mac, UE, frameP, slotP, current_rnti, harq_pid, sduP == NULL);
  } else { 
    nr_rx_ra_sdu(gnb_mod_idP, CC_idP, frameP, slotP, current_rnti, sduP, sdu_lenP, harq_pid, timing_advance, ul_cqi, rssi);
  }
}

void nr_rx_sdu(const module_id_t gnb_mod_idP,
               const int CC_idP,
               const frame_t frameP,
               const slot_t slotP,
               const rnti_t rntiP,
               uint8_t *sduP,
               const uint32_t sdu_lenP,
               const int8_t harq_pid,
               const uint16_t timing_advance,
               const uint8_t ul_cqi,
               const uint16_t rssi)
{
  gNB_MAC_INST *gNB_mac = RC.nrmac[gnb_mod_idP];
  NR_SCHED_LOCK(&gNB_mac->sched_lock);
  start_meas(&gNB_mac->rx_ulsch_sdu);
  _nr_rx_sdu(gnb_mod_idP, CC_idP, frameP, slotP, rntiP, sduP, sdu_lenP, harq_pid, timing_advance, ul_cqi, rssi);
  stop_meas(&gNB_mac->rx_ulsch_sdu);
  NR_SCHED_UNLOCK(&gNB_mac->sched_lock);
}

static uint32_t calc_power_complex(const int16_t *x, const int16_t *y, const uint32_t size)
{
  // Real part value
  int64_t sum_x = 0;
  int64_t sum_x2 = 0;
  for(int k = 0; k<size; k++) {
    sum_x = sum_x + x[k];
    sum_x2 = sum_x2 + x[k]*x[k];
  }
  uint32_t power_re = sum_x2/size - (sum_x/size)*(sum_x/size);

  // Imaginary part power
  int64_t sum_y = 0;
  int64_t sum_y2 = 0;
  for(int k = 0; k<size; k++) {
    sum_y = sum_y + y[k];
    sum_y2 = sum_y2 + y[k]*y[k];
  }
  uint32_t power_im = sum_y2/size - (sum_y/size)*(sum_y/size);

  return power_re+power_im;
}

static c16_t nr_h_times_w(c16_t h, char w)
{
  c16_t output;
    switch (w) {
      case '0': // 0
        output.r = 0;
        output.i = 0;
        break;
      case '1': // 1
        output.r = h.r;
        output.i = h.i;
        break;
      case 'n': // -1
        output.r = -h.r;
        output.i = -h.i;
        break;
      case 'j': // j
        output.r = -h.i;
        output.i = h.r;
        break;
      case 'o': // -j
        output.r = h.i;
        output.i = -h.r;
        break;
      default:
        AssertFatal(1==0,"Invalid precoder value %c\n", w);
    }
  return output;
}

static uint8_t get_max_tpmi(const NR_PUSCH_Config_t *pusch_Config,
                            const uint16_t num_ue_srs_ports,
                            const uint8_t *nrOfLayers,
                            int *additional_max_tpmi)
{
  uint8_t max_tpmi = 0;

  if (!pusch_Config
      || (pusch_Config->txConfig != NULL && *pusch_Config->txConfig == NR_PUSCH_Config__txConfig_nonCodebook)
      || num_ue_srs_ports == 1)
    return max_tpmi;

  long max_rank = *pusch_Config->maxRank;
  long *ul_FullPowerTransmission = pusch_Config->ext1 ? pusch_Config->ext1->ul_FullPowerTransmission_r16 : NULL;
  long *codebookSubset = pusch_Config->codebookSubset;

  if (num_ue_srs_ports == 2) {

    if (max_rank == 1) {
      if (ul_FullPowerTransmission && *ul_FullPowerTransmission == NR_PUSCH_Config__ext1__ul_FullPowerTransmission_r16_fullpowerMode1) {
        max_tpmi = 2;
      } else {
        if (codebookSubset && *codebookSubset == NR_PUSCH_Config__codebookSubset_nonCoherent) {
          max_tpmi = 1;
        } else {
          max_tpmi = 5;
        }
      }
    } else {
      if (ul_FullPowerTransmission && *ul_FullPowerTransmission == NR_PUSCH_Config__ext1__ul_FullPowerTransmission_r16_fullpowerMode1) {
        max_tpmi = *nrOfLayers == 1 ? 2 : 0;
      } else {
        if (codebookSubset && *codebookSubset == NR_PUSCH_Config__codebookSubset_nonCoherent) {
          max_tpmi = *nrOfLayers == 1 ? 1 : 0;
        } else {
          max_tpmi = *nrOfLayers == 1 ? 5 : 2;
        }
      }
    }

  } else if (num_ue_srs_ports == 4) {

    if (max_rank == 1) {
      if (ul_FullPowerTransmission && *ul_FullPowerTransmission == NR_PUSCH_Config__ext1__ul_FullPowerTransmission_r16_fullpowerMode1) {
        if (codebookSubset && *codebookSubset == NR_PUSCH_Config__codebookSubset_nonCoherent) {
          max_tpmi = 3;
          *additional_max_tpmi = 13;
        } else {
          max_tpmi = 15;
        }
      } else {
        if (codebookSubset && *codebookSubset == NR_PUSCH_Config__codebookSubset_nonCoherent) {
          max_tpmi = 3;
        } else if (codebookSubset && *codebookSubset == NR_PUSCH_Config__codebookSubset_partialAndNonCoherent) {
          max_tpmi = 11;
        } else {
          max_tpmi = 27;
        }
      }
    } else {
      if (ul_FullPowerTransmission && *ul_FullPowerTransmission == NR_PUSCH_Config__ext1__ul_FullPowerTransmission_r16_fullpowerMode1) {
        if (max_rank == 2) {
          if (codebookSubset && *codebookSubset == NR_PUSCH_Config__codebookSubset_nonCoherent) {
            max_tpmi = *nrOfLayers == 1 ? 3 : 6;
            if (*nrOfLayers == 1) {
              *additional_max_tpmi = 13;
            }
          } else {
            max_tpmi = *nrOfLayers == 1 ? 15 : 13;
          }
        } else {
          if (codebookSubset && *codebookSubset == NR_PUSCH_Config__codebookSubset_nonCoherent) {
            switch (*nrOfLayers) {
              case 1:
                max_tpmi = 3;
                *additional_max_tpmi = 13;
                break;
              case 2:
                max_tpmi = 6;
                break;
              case 3:
                max_tpmi = 1;
                break;
              case 4:
                max_tpmi = 0;
                break;
              default:
                LOG_E(NR_MAC,"Number of layers %d is invalid!\n", *nrOfLayers);
            }
          } else {
            switch (*nrOfLayers) {
              case 1:
                max_tpmi = 15;
                break;
              case 2:
                max_tpmi = 13;
                break;
              case 3:
              case 4:
                max_tpmi = 2;
                break;
              default:
                LOG_E(NR_MAC,"Number of layers %d is invalid!\n", *nrOfLayers);
            }
          }
        }
      } else {
        if (codebookSubset && *codebookSubset == NR_PUSCH_Config__codebookSubset_nonCoherent) {
          switch (*nrOfLayers) {
            case 1:
              max_tpmi = 3;
              break;
            case 2:
              max_tpmi = 5;
              break;
            case 3:
            case 4:
              max_tpmi = 0;
              break;
            default:
              LOG_E(NR_MAC,"Number of layers %d is invalid!\n", *nrOfLayers);
          }
        } else if (codebookSubset && *codebookSubset == NR_PUSCH_Config__codebookSubset_partialAndNonCoherent) {
          switch (*nrOfLayers) {
            case 1:
              max_tpmi = 11;
              break;
            case 2:
              max_tpmi = 13;
              break;
            case 3:
            case 4:
              max_tpmi = 2;
              break;
            default:
              LOG_E(NR_MAC,"Number of layers %d is invalid!\n", *nrOfLayers);
          }
        } else {
          switch (*nrOfLayers) {
            case 1:
              max_tpmi = 28;
              break;
            case 2:
              max_tpmi = 22;
              break;
            case 3:
              max_tpmi = 7;
              break;
            case 4:
              max_tpmi = 5;
              break;
            default:
              LOG_E(NR_MAC,"Number of layers %d is invalid!\n", *nrOfLayers);
          }
        }
      }
    }

  }

  return max_tpmi;
}

// TS 38.211 - Table 6.3.1.5-1: Precoding matrix W for single-layer transmission using two antenna ports, 'n' = -1 and 'o' = -j
const char table_38211_6_3_1_5_1[6][2][1] = {
    {{'1'}, {'0'}}, // tpmi 0
    {{'0'}, {'1'}}, // tpmi 1
    {{'1'}, {'1'}}, // tpmi 2
    {{'1'}, {'n'}}, // tpmi 3
    {{'1'}, {'j'}}, // tpmi 4
    {{'1'}, {'o'}}  // tpmi 5
};

// TS 38.211 - Table 6.3.1.5-2: Precoding matrix W for single-layer transmission using four antenna ports with transform precoding enabled, 'n' = -1 and 'o' = -j
const char table_38211_6_3_1_5_2[28][4][1] = {
    {{'1'}, {'0'}, {'0'}, {'0'}}, // tpmi 0
    {{'0'}, {'1'}, {'0'}, {'0'}}, // tpmi 1
    {{'0'}, {'0'}, {'1'}, {'0'}}, // tpmi 2
    {{'0'}, {'0'}, {'0'}, {'1'}}, // tpmi 3
    {{'1'}, {'0'}, {'1'}, {'0'}}, // tpmi 4
    {{'1'}, {'0'}, {'n'}, {'0'}}, // tpmi 5
    {{'1'}, {'0'}, {'j'}, {'0'}}, // tpmi 6
    {{'1'}, {'0'}, {'o'}, {'0'}}, // tpmi 7
    {{'0'}, {'1'}, {'0'}, {'1'}}, // tpmi 8
    {{'0'}, {'1'}, {'0'}, {'n'}}, // tpmi 9
    {{'0'}, {'1'}, {'0'}, {'j'}}, // tpmi 10
    {{'0'}, {'1'}, {'0'}, {'o'}}, // tpmi 11
    {{'1'}, {'1'}, {'1'}, {'n'}}, // tpmi 12
    {{'1'}, {'1'}, {'j'}, {'j'}}, // tpmi 13
    {{'1'}, {'1'}, {'n'}, {'1'}}, // tpmi 14
    {{'1'}, {'1'}, {'o'}, {'o'}}, // tpmi 15
    {{'1'}, {'j'}, {'1'}, {'j'}}, // tpmi 16
    {{'1'}, {'j'}, {'j'}, {'1'}}, // tpmi 17
    {{'1'}, {'j'}, {'n'}, {'o'}}, // tpmi 18
    {{'1'}, {'j'}, {'o'}, {'n'}}, // tpmi 19
    {{'1'}, {'n'}, {'1'}, {'1'}}, // tpmi 20
    {{'1'}, {'n'}, {'j'}, {'o'}}, // tpmi 21
    {{'1'}, {'n'}, {'n'}, {'n'}}, // tpmi 22
    {{'1'}, {'n'}, {'o'}, {'j'}}, // tpmi 23
    {{'1'}, {'o'}, {'1'}, {'o'}}, // tpmi 24
    {{'1'}, {'o'}, {'j'}, {'n'}}, // tpmi 25
    {{'1'}, {'o'}, {'n'}, {'j'}}, // tpmi 26
    {{'1'}, {'o'}, {'o'}, {'1'}}  // tpmi 27
};

// TS 38.211 - Table 6.3.1.5-3: Precoding matrix W for single-layer transmission using four antenna ports with transform precoding disabled, 'n' = -1 and 'o' = -j
const char table_38211_6_3_1_5_3[28][4][1] = {
    {{'1'}, {'0'}, {'0'}, {'0'}}, // tpmi 0
    {{'0'}, {'1'}, {'0'}, {'0'}}, // tpmi 1
    {{'0'}, {'0'}, {'1'}, {'0'}}, // tpmi 2
    {{'0'}, {'0'}, {'0'}, {'1'}}, // tpmi 3
    {{'1'}, {'0'}, {'1'}, {'0'}}, // tpmi 4
    {{'1'}, {'0'}, {'n'}, {'0'}}, // tpmi 5
    {{'1'}, {'0'}, {'j'}, {'0'}}, // tpmi 6
    {{'1'}, {'0'}, {'o'}, {'0'}}, // tpmi 7
    {{'0'}, {'1'}, {'0'}, {'1'}}, // tpmi 8
    {{'0'}, {'1'}, {'0'}, {'n'}}, // tpmi 9
    {{'0'}, {'1'}, {'0'}, {'j'}}, // tpmi 10
    {{'0'}, {'1'}, {'0'}, {'o'}}, // tpmi 11
    {{'1'}, {'1'}, {'1'}, {'1'}}, // tpmi 12
    {{'1'}, {'1'}, {'j'}, {'j'}}, // tpmi 13
    {{'1'}, {'1'}, {'n'}, {'n'}}, // tpmi 14
    {{'1'}, {'1'}, {'o'}, {'o'}}, // tpmi 15
    {{'1'}, {'j'}, {'1'}, {'j'}}, // tpmi 16
    {{'1'}, {'j'}, {'j'}, {'n'}}, // tpmi 17
    {{'1'}, {'j'}, {'n'}, {'o'}}, // tpmi 18
    {{'1'}, {'j'}, {'o'}, {'1'}}, // tpmi 19
    {{'1'}, {'n'}, {'1'}, {'n'}}, // tpmi 20
    {{'1'}, {'n'}, {'j'}, {'o'}}, // tpmi 21
    {{'1'}, {'n'}, {'n'}, {'1'}}, // tpmi 22
    {{'1'}, {'n'}, {'o'}, {'j'}}, // tpmi 23
    {{'1'}, {'o'}, {'1'}, {'o'}}, // tpmi 24
    {{'1'}, {'o'}, {'j'}, {'1'}}, // tpmi 25
    {{'1'}, {'o'}, {'n'}, {'j'}}, // tpmi 26
    {{'1'}, {'o'}, {'o'}, {'n'}}  // tpmi 27
};

// TS 38.211 - Table 6.3.1.5-4: Precoding matrix W for two-layer transmission using two antenna ports, 'n' = -1 and 'o' = -j
const char table_38211_6_3_1_5_4[3][2][2] = {
    {{'1', '0'}, {'0', '1'}}, // tpmi 0
    {{'1', '1'}, {'1', 'n'}}, // tpmi 1
    {{'1', '1'}, {'j', 'o'}}  // tpmi 2
};

// TS 38.211 - Table 6.3.1.5-5: Precoding matrix W for two-layer transmission using four antenna ports, 'n' = -1 and 'o' = -j
const char table_38211_6_3_1_5_5[22][4][2] = {
    {{'1', '0'}, {'0', '1'}, {'0', '0'}, {'0', '0'}}, // tpmi 0
    {{'1', '0'}, {'0', '0'}, {'0', '1'}, {'0', '0'}}, // tpmi 1
    {{'1', '0'}, {'0', '0'}, {'0', '0'}, {'0', '1'}}, // tpmi 2
    {{'0', '0'}, {'1', '0'}, {'0', '1'}, {'0', '0'}}, // tpmi 3
    {{'0', '0'}, {'1', '0'}, {'0', '0'}, {'0', '1'}}, // tpmi 4
    {{'0', '0'}, {'0', '0'}, {'1', '0'}, {'0', '1'}}, // tpmi 5
    {{'1', '0'}, {'0', '1'}, {'1', '0'}, {'0', 'o'}}, // tpmi 6
    {{'1', '0'}, {'0', '1'}, {'1', '0'}, {'0', 'j'}}, // tpmi 7
    {{'1', '0'}, {'0', '1'}, {'o', '0'}, {'0', '1'}}, // tpmi 8
    {{'1', '0'}, {'0', '1'}, {'o', '0'}, {'0', 'n'}}, // tpmi 9
    {{'1', '0'}, {'0', '1'}, {'n', '0'}, {'0', 'o'}}, // tpmi 10
    {{'1', '0'}, {'0', '1'}, {'n', '0'}, {'0', 'j'}}, // tpmi 11
    {{'1', '0'}, {'0', '1'}, {'j', '0'}, {'0', '1'}}, // tpmi 12
    {{'1', '0'}, {'0', '1'}, {'j', '0'}, {'0', 'n'}}, // tpmi 13
    {{'1', '1'}, {'1', '1'}, {'1', 'n'}, {'1', 'n'}}, // tpmi 14
    {{'1', '1'}, {'1', '1'}, {'j', 'o'}, {'j', 'o'}}, // tpmi 15
    {{'1', '1'}, {'j', 'j'}, {'1', 'n'}, {'j', 'o'}}, // tpmi 16
    {{'1', '1'}, {'j', 'j'}, {'j', 'o'}, {'n', '1'}}, // tpmi 17
    {{'1', '1'}, {'n', 'n'}, {'1', 'n'}, {'n', '1'}}, // tpmi 18
    {{'1', '1'}, {'n', 'n'}, {'j', 'o'}, {'o', 'j'}}, // tpmi 19
    {{'1', '1'}, {'o', 'o'}, {'1', 'n'}, {'o', 'j'}}, // tpmi 20
    {{'1', '1'}, {'o', 'o'}, {'j', 'o'}, {'1', 'n'}}  // tpmi 21
};

static void get_precoder_matrix_coef(char *w,
                                     const uint8_t ul_ri,
                                     const uint16_t num_ue_srs_ports,
                                     const long transform_precoding,
                                     const uint8_t tpmi,
                                     const uint8_t uI,
                                     int layer_idx)
{
  if (ul_ri == 0) {
    if (num_ue_srs_ports == 2) {
      *w = table_38211_6_3_1_5_1[tpmi][uI][layer_idx];
    } else {
      if (transform_precoding == NR_PUSCH_Config__transformPrecoder_enabled) {
        *w = table_38211_6_3_1_5_2[tpmi][uI][layer_idx];
      } else {
        *w = table_38211_6_3_1_5_3[tpmi][uI][layer_idx];
      }
    }
  } else if (ul_ri == 1) {
    if (num_ue_srs_ports == 2) {
      *w = table_38211_6_3_1_5_4[tpmi][uI][layer_idx];
    } else {
      *w = table_38211_6_3_1_5_5[tpmi][uI][layer_idx];
    }
  } else {
    AssertFatal(1 == 0, "Function get_precoder_matrix_coef() does not support %i layers yet!\n", ul_ri + 1);
  }
}

static int nr_srs_tpmi_estimation(const NR_PUSCH_Config_t *pusch_Config,
                                  const long transform_precoding,
                                  const uint8_t *channel_matrix,
                                  const uint8_t normalized_iq_representation,
                                  const uint16_t num_gnb_antenna_elements,
                                  const uint16_t num_ue_srs_ports,
                                  const uint16_t prg_size,
                                  const uint16_t num_prgs,
                                  const uint8_t ul_ri)
{
  if (ul_ri > 1) {
    LOG_D(NR_MAC, "TPMI computation for ul_ri %i is not implemented yet!\n", ul_ri);
    return 0;
  }

  uint8_t tpmi_sel = 0;
  const uint8_t nrOfLayers = ul_ri + 1;
  int16_t precoded_channel_matrix_re[num_prgs * num_gnb_antenna_elements];
  int16_t precoded_channel_matrix_im[num_prgs * num_gnb_antenna_elements];
  c16_t *channel_matrix16 = (c16_t *)channel_matrix;
  uint32_t max_precoded_signal_power = 0;
  int additional_max_tpmi = -1;
  char w;

  uint8_t max_tpmi = get_max_tpmi(pusch_Config, num_ue_srs_ports, &nrOfLayers, &additional_max_tpmi);
  uint8_t end_tpmi_loop = additional_max_tpmi > max_tpmi ? additional_max_tpmi : max_tpmi;

  //                      channel_matrix                          x   precoder_matrix
  // [ (gI=0,uI=0) (gI=0,uI=1) ... (gI=0,uI=num_ue_srs_ports-1) ] x   [uI=0]
  // [ (gI=1,uI=0) (gI=1,uI=1) ... (gI=1,uI=num_ue_srs_ports-1) ]     [uI=1]
  // [ (gI=2,uI=0) (gI=2,uI=1) ... (gI=2,uI=num_ue_srs_ports-1) ]     [uI=2]
  //                           ...                                     ...

  for (uint8_t tpmi = 0; tpmi <= end_tpmi_loop && end_tpmi_loop > 0; tpmi++) {
    if (tpmi > max_tpmi) {
      tpmi = end_tpmi_loop;
    }

    for (int pI = 0; pI < num_prgs; pI++) {
      for (int gI = 0; gI < num_gnb_antenna_elements; gI++) {
        uint16_t index_gI_pI = gI * num_prgs + pI;
        precoded_channel_matrix_re[index_gI_pI] = 0;
        precoded_channel_matrix_im[index_gI_pI] = 0;

        for (int uI = 0; uI < num_ue_srs_ports; uI++) {
          for (int layer_idx = 0; layer_idx < nrOfLayers; layer_idx++) {
            uint16_t index = uI * num_gnb_antenna_elements * num_prgs + index_gI_pI;
            get_precoder_matrix_coef(&w, ul_ri, num_ue_srs_ports, transform_precoding, tpmi, uI, layer_idx);
            c16_t h_times_w = nr_h_times_w(channel_matrix16[index], w);

            precoded_channel_matrix_re[index_gI_pI] += h_times_w.r;
            precoded_channel_matrix_im[index_gI_pI] += h_times_w.i;

#ifdef SRS_IND_DEBUG
            LOG_I(NR_MAC, "(pI %i, gI %i,  uI %i, layer_idx %i) w = %c, channel_matrix --> real %i, imag %i\n",
                  pI, gI, uI, layer_idx, w, channel_matrix16[index].r, channel_matrix16[index].i);
#endif
          }
        }

#ifdef SRS_IND_DEBUG
        LOG_I(NR_MAC, "(pI %i, gI %i) precoded_channel_coef --> real %i, imag %i\n",
              pI, gI, precoded_channel_matrix_re[index_gI_pI], precoded_channel_matrix_im[index_gI_pI]);
#endif
      }
    }

    uint32_t precoded_signal_power = calc_power_complex(precoded_channel_matrix_re,
                                                        precoded_channel_matrix_im,
                                                        num_prgs * num_gnb_antenna_elements);

#ifdef SRS_IND_DEBUG
    LOG_I(NR_MAC, "(tpmi %i) precoded_signal_power = %i\n", tpmi, precoded_signal_power);
#endif

    if (precoded_signal_power > max_precoded_signal_power) {
      max_precoded_signal_power = precoded_signal_power;
      tpmi_sel = tpmi;
    }
  }

  return tpmi_sel;
}

void handle_nr_srs_measurements(const module_id_t module_id,
                                const frame_t frame,
                                const slot_t slot,
                                nfapi_nr_srs_indication_pdu_t *srs_ind)
{
  gNB_MAC_INST *nrmac = RC.nrmac[module_id];
  LOG_D(NR_MAC, "(%d.%d) Received SRS indication for UE %04x\n", frame, slot, srs_ind->rnti);
  if (srs_ind->report_type == 0) {
    //SCF 222.10.04 Table 3-129 Report type = 0 means a null report, we can skip unpacking it
    return;
  }

  if (srs_ind->timing_advance_offset == 0xFFFF) {
    LOG_W(NR_MAC, "Invalid timing advance offset for RNTI %04x\n", srs_ind->rnti);
    return;
  }
  NR_SCHED_LOCK(&nrmac->sched_lock);

#ifdef SRS_IND_DEBUG
  LOG_I(NR_MAC, "frame = %i\n", frame);
  LOG_I(NR_MAC, "slot = %i\n", slot);
  LOG_I(NR_MAC, "srs_ind->rnti = %04x\n", srs_ind->rnti);
  LOG_I(NR_MAC, "srs_ind->timing_advance_offset = %i\n", srs_ind->timing_advance_offset);
  LOG_I(NR_MAC, "srs_ind->timing_advance_offset_nsec = %i\n", srs_ind->timing_advance_offset_nsec);
  LOG_I(NR_MAC, "srs_ind->srs_usage = %i\n", srs_ind->srs_usage);
  LOG_I(NR_MAC, "srs_ind->report_type = %i\n", srs_ind->report_type);
#endif

  NR_UE_info_t *UE = find_nr_UE(&RC.nrmac[module_id]->UE_info, srs_ind->rnti);
  if (!UE) {
    LOG_W(NR_MAC, "Could not find UE for RNTI %04x\n", srs_ind->rnti);
    NR_SCHED_UNLOCK(&nrmac->sched_lock);
    return;
  }

  gNB_MAC_INST *nr_mac = RC.nrmac[module_id];
  NR_mac_stats_t *stats = &UE->mac_stats;
  nfapi_srs_report_tlv_t *report_tlv = &srs_ind->report_tlv;

  switch (srs_ind->srs_usage) {
    case NR_SRS_ResourceSet__usage_beamManagement: {
      nfapi_nr_srs_beamforming_report_t nr_srs_bf_report;
      unpack_nr_srs_beamforming_report(report_tlv->value,
                                       report_tlv->length,
                                       &nr_srs_bf_report,
                                       sizeof(nfapi_nr_srs_beamforming_report_t));

      if (nr_srs_bf_report.wide_band_snr == 0xFF) {
        LOG_W(NR_MAC, "Invalid wide_band_snr for RNTI %04x\n", srs_ind->rnti);
        NR_SCHED_UNLOCK(&nrmac->sched_lock);
        return;
      }

      int wide_band_snr_dB = (nr_srs_bf_report.wide_band_snr >> 1) - 64;

#ifdef SRS_IND_DEBUG
      LOG_I(NR_MAC, "nr_srs_bf_report.prg_size = %i\n", nr_srs_bf_report.prg_size);
      LOG_I(NR_MAC, "nr_srs_bf_report.num_symbols = %i\n", nr_srs_bf_report.num_symbols);
      LOG_I(NR_MAC, "nr_srs_bf_report.wide_band_snr = %i (%i dB)\n", nr_srs_bf_report.wide_band_snr, wide_band_snr_dB);
      LOG_I(NR_MAC, "nr_srs_bf_report.num_reported_symbols = %i\n", nr_srs_bf_report.num_reported_symbols);
      LOG_I(NR_MAC, "nr_srs_bf_report.reported_symbol_list[0].num_prgs = %i\n", nr_srs_bf_report.reported_symbol_list[0].num_prgs);
      for (int prg_idx = 0; prg_idx < nr_srs_bf_report.reported_symbol_list[0].num_prgs; prg_idx++) {
        LOG_I(NR_MAC,
              "nr_srs_bf_report.reported_symbol_list[0].prg_list[%3i].rb_snr = %i (%i dB)\n",
              prg_idx,
              nr_srs_bf_report.reported_symbol_list[0].prg_list[prg_idx].rb_snr,
              (nr_srs_bf_report.reported_symbol_list[0].prg_list[prg_idx].rb_snr >> 1) - 64);
      }
#endif

      sprintf(stats->srs_stats, "UL-SNR %i dB", wide_band_snr_dB);

      const int ul_prbblack_SNR_threshold = nr_mac->ul_prbblack_SNR_threshold;
      uint16_t *ulprbbl = nr_mac->ulprbbl;

      uint16_t num_rbs = nr_srs_bf_report.prg_size * nr_srs_bf_report.reported_symbol_list[0].num_prgs;
      memset(ulprbbl, 0, num_rbs * sizeof(uint16_t));
      for (int rb = 0; rb < num_rbs; rb++) {
        int snr = (nr_srs_bf_report.reported_symbol_list[0].prg_list[rb / nr_srs_bf_report.prg_size].rb_snr >> 1) - 64;
        if (snr < wide_band_snr_dB - ul_prbblack_SNR_threshold) {
          ulprbbl[rb] = 0x3FFF; // all symbols taken
        }
        LOG_D(NR_MAC, "ulprbbl[%3i] = 0x%x\n", rb, ulprbbl[rb]);
      }

      break;
    }

    case NR_SRS_ResourceSet__usage_codebook: {
      nfapi_nr_srs_normalized_channel_iq_matrix_t nr_srs_channel_iq_matrix;
      unpack_nr_srs_normalized_channel_iq_matrix(report_tlv->value,
                                                 report_tlv->length,
                                                 &nr_srs_channel_iq_matrix,
                                                 sizeof(nfapi_nr_srs_normalized_channel_iq_matrix_t));

#ifdef SRS_IND_DEBUG
      LOG_I(NR_MAC, "nr_srs_channel_iq_matrix.normalized_iq_representation = %i\n", nr_srs_channel_iq_matrix.normalized_iq_representation);
      LOG_I(NR_MAC, "nr_srs_channel_iq_matrix.num_gnb_antenna_elements = %i\n", nr_srs_channel_iq_matrix.num_gnb_antenna_elements);
      LOG_I(NR_MAC, "nr_srs_channel_iq_matrix.num_ue_srs_ports = %i\n", nr_srs_channel_iq_matrix.num_ue_srs_ports);
      LOG_I(NR_MAC, "nr_srs_channel_iq_matrix.prg_size = %i\n", nr_srs_channel_iq_matrix.prg_size);
      LOG_I(NR_MAC, "nr_srs_channel_iq_matrix.num_prgs = %i\n", nr_srs_channel_iq_matrix.num_prgs);
      c16_t *channel_matrix16 = (c16_t *)nr_srs_channel_iq_matrix.channel_matrix;
      c8_t *channel_matrix8 = (c8_t *)nr_srs_channel_iq_matrix.channel_matrix;
      for (int uI = 0; uI < nr_srs_channel_iq_matrix.num_ue_srs_ports; uI++) {
        for (int gI = 0; gI < nr_srs_channel_iq_matrix.num_gnb_antenna_elements; gI++) {
          for (int pI = 0; pI < nr_srs_channel_iq_matrix.num_prgs; pI++) {
            uint16_t index = uI * nr_srs_channel_iq_matrix.num_gnb_antenna_elements * nr_srs_channel_iq_matrix.num_prgs + gI * nr_srs_channel_iq_matrix.num_prgs + pI;
            LOG_I(NR_MAC,
                  "(uI %i, gI %i, pI %i) channel_matrix --> real %i, imag %i\n",
                  uI,
                  gI,
                  pI,
                  nr_srs_channel_iq_matrix.normalized_iq_representation == 0 ? channel_matrix8[index].r : channel_matrix16[index].r,
                  nr_srs_channel_iq_matrix.normalized_iq_representation == 0 ? channel_matrix8[index].i : channel_matrix16[index].i);
          }
        }
      }
#endif

      NR_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
      NR_UE_UL_BWP_t *current_BWP = &UE->current_UL_BWP;
      sched_ctrl->srs_feedback.sri = NR_SRS_SRI_0;

      start_meas(&nr_mac->nr_srs_ri_computation_timer);
      nr_srs_ri_computation(&nr_srs_channel_iq_matrix, current_BWP, &sched_ctrl->srs_feedback.ul_ri);
      stop_meas(&nr_mac->nr_srs_ri_computation_timer);

      start_meas(&nr_mac->nr_srs_tpmi_computation_timer);
      sched_ctrl->srs_feedback.tpmi = nr_srs_tpmi_estimation(current_BWP->pusch_Config,
                                                             current_BWP->transform_precoding,
                                                             nr_srs_channel_iq_matrix.channel_matrix,
                                                             nr_srs_channel_iq_matrix.normalized_iq_representation,
                                                             nr_srs_channel_iq_matrix.num_gnb_antenna_elements,
                                                             nr_srs_channel_iq_matrix.num_ue_srs_ports,
                                                             nr_srs_channel_iq_matrix.prg_size,
                                                             nr_srs_channel_iq_matrix.num_prgs,
                                                             sched_ctrl->srs_feedback.ul_ri);
      stop_meas(&nr_mac->nr_srs_tpmi_computation_timer);

      sprintf(stats->srs_stats, "UL-RI %d, TPMI %d", sched_ctrl->srs_feedback.ul_ri + 1, sched_ctrl->srs_feedback.tpmi);

      break;
    }

    case NR_SRS_ResourceSet__usage_nonCodebook:
    case NR_SRS_ResourceSet__usage_antennaSwitching:
      LOG_W(NR_MAC, "MAC procedures for this SRS usage are not implemented yet!\n");
      break;

    default:
      AssertFatal(1 == 0, "Invalid SRS usage\n");
  }
  NR_SCHED_UNLOCK(&nrmac->sched_lock);
}

static bool nr_UE_is_to_be_scheduled(const frame_structure_t *fs,
                                     NR_UE_info_t *UE,
                                     frame_t frame,
                                     slot_t slot,
                                     uint32_t ulsch_max_frame_inactivity)
{
  const int n = fs->numb_slots_frame;
  const int now = frame * n + slot;

  const NR_UE_sched_ctrl_t *sched_ctrl =&UE->UE_sched_ctrl;
  /**
   * Force the default transmission in a full slot as early
   * as possible in the UL portion of TDD period (last_ul_slot) */
  int num_slots_per_period = fs->numb_slots_period;
  int last_ul_slot = fs->frame_type == TDD ? get_first_ul_slot(fs, false) : sched_ctrl->last_ul_slot;
  const int last_ul_sched = sched_ctrl->last_ul_frame * n + last_ul_slot;
  const int diff = (now - last_ul_sched + 1024 * n) % (1024 * n);
  /* UE is to be scheduled if
   * (1) we think the UE has more bytes awaiting than what we scheduled
   * (2) there is a scheduling request
   * (3) or we did not schedule it in more than 10 frames */
  const bool has_data = sched_ctrl->estimated_ul_buffer > sched_ctrl->sched_ul_bytes;
  const bool high_inactivity = diff >= (ulsch_max_frame_inactivity > 0 ? ulsch_max_frame_inactivity * n : num_slots_per_period);
  LOG_D(NR_MAC,
        "%4d.%2d UL inactivity %d slots has_data %d SR %d\n",
        frame,
        slot,
        diff,
        has_data,
        sched_ctrl->SR);
  return has_data || sched_ctrl->SR || high_inactivity;
}

static void update_ul_ue_R_Qm(int mcs, int mcs_table, const NR_PUSCH_Config_t *pusch_Config, uint16_t *R, uint8_t *Qm)
{
  *R = nr_get_code_rate_ul(mcs, mcs_table);
  *Qm = nr_get_Qm_ul(mcs, mcs_table);

  if (pusch_Config && pusch_Config->tp_pi2BPSK && ((mcs_table == 3 && mcs < 2) || (mcs_table == 4 && mcs < 6))) {
    *R >>= 1;
    *Qm <<= 1;
  }
}

static void nr_ue_max_mcs_min_rb(int mu,
                                 int ph_limit,
                                 NR_sched_pusch_t *sched_pusch,
                                 NR_UE_UL_BWP_t *ul_bwp,
                                 uint16_t minRb,
                                 uint32_t tbs,
                                 uint16_t *Rb,
                                 uint8_t *mcs)
{
  AssertFatal(*Rb >= minRb, "illegal Rb %d < minRb %d\n", *Rb, minRb);
  AssertFatal(*mcs >= 0 && *mcs <= 28, "illegal MCS %d\n", *mcs);

  int tbs_bits = tbs << 3;
  uint16_t R;
  uint8_t Qm;
  update_ul_ue_R_Qm(*mcs, ul_bwp->mcs_table, ul_bwp->pusch_Config, &R, &Qm);

  long *deltaMCS = ul_bwp->pusch_Config ? ul_bwp->pusch_Config->pusch_PowerControl->deltaMCS : NULL;
  tbs_bits = nr_compute_tbs(Qm, R, *Rb,
                              sched_pusch->tda_info.nrOfSymbols,
                              sched_pusch->dmrs_info.N_PRB_DMRS * sched_pusch->dmrs_info.num_dmrs_symb,
                              0, // nb_rb_oh
                              0,
                              sched_pusch->nrOfLayers);

  int tx_power = compute_ph_factor(mu,
                                   tbs_bits,
                                   *Rb,
                                   sched_pusch->nrOfLayers,
                                   sched_pusch->tda_info.nrOfSymbols,
                                   sched_pusch->dmrs_info.N_PRB_DMRS * sched_pusch->dmrs_info.num_dmrs_symb,
                                   deltaMCS,
                                   true);

  while (ph_limit < tx_power && *Rb > minRb) {
    (*Rb)--;
    tbs_bits = nr_compute_tbs(Qm, R, *Rb,
                              sched_pusch->tda_info.nrOfSymbols,
                              sched_pusch->dmrs_info.N_PRB_DMRS * sched_pusch->dmrs_info.num_dmrs_symb,
                              0, // nb_rb_oh
                              0,
                              sched_pusch->nrOfLayers);
    tx_power = compute_ph_factor(mu,
                                 tbs_bits,
                                 *Rb,
                                 sched_pusch->nrOfLayers,
                                 sched_pusch->tda_info.nrOfSymbols,
                                 sched_pusch->dmrs_info.N_PRB_DMRS * sched_pusch->dmrs_info.num_dmrs_symb,
                                 deltaMCS,
                                 true);
    LOG_D(NR_MAC, "Checking %d RBs, MCS %d, ph_limit %d, tx_power %d\n",*Rb,*mcs,ph_limit,tx_power);
  }

  while (ph_limit < tx_power && *mcs > 0) {
    (*mcs)--;
    update_ul_ue_R_Qm(*mcs, ul_bwp->mcs_table, ul_bwp->pusch_Config, &R, &Qm);
    tbs_bits = nr_compute_tbs(Qm, R, *Rb,
                              sched_pusch->tda_info.nrOfSymbols,
                              sched_pusch->dmrs_info.N_PRB_DMRS * sched_pusch->dmrs_info.num_dmrs_symb,
                              0, // nb_rb_oh
                              0,
                              sched_pusch->nrOfLayers);
    tx_power = compute_ph_factor(mu,
                                 tbs_bits,
                                 *Rb,
                                 sched_pusch->nrOfLayers,
                                 sched_pusch->tda_info.nrOfSymbols,
                                 sched_pusch->dmrs_info.N_PRB_DMRS * sched_pusch->dmrs_info.num_dmrs_symb,
                                 deltaMCS,
                                 true);
    LOG_D(NR_MAC, "Checking %d RBs, MCS %d, ph_limit %d, tx_power %d\n",*Rb,*mcs,ph_limit,tx_power);
  }

  if (ph_limit < tx_power)
    LOG_D(NR_MAC, "Normalized power %d based on current resources (RBs %d, MCS %d) exceed reported PHR %d (normalized value)\n",
          tx_power, *Rb, *mcs, ph_limit);
}

static bool allocate_ul_retransmission(gNB_MAC_INST *nrmac,
                                       post_process_pusch_t *pp_pusch,
                                       uint16_t *rballoc_mask,
                                       int *n_rb_sched,
                                       int dci_beam_idx,
                                       NR_UE_info_t *UE,
                                       int harq_pid,
                                       const NR_ServingCellConfigCommon_t *scc,
                                       const int tda,
                                       const NR_tda_info_t *tda_info)
{
  const int CC_id = 0;
  int frame = pp_pusch->frame;
  int slot = pp_pusch->slot;
  NR_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
  /* Get previous PUSCH field info */
  const NR_sched_pusch_t *retInfo = &sched_ctrl->ul_harq_processes[harq_pid].sched_pusch;
  NR_sched_pusch_t new_sched = *retInfo; // potential new allocation
  NR_UE_UL_BWP_t *ul_bwp = &UE->current_UL_BWP;

  int rbStart = 0; // wrt BWP start
  bwp_info_t bwp_info = get_pusch_bwp_start_size(UE);
  const uint32_t bwpSize = bwp_info.bwpSize;
  const uint32_t bwpStart = bwp_info.bwpStart;
  const uint8_t nrOfLayers = retInfo->nrOfLayers;
  LOG_D(NR_MAC,"retInfo->time_domain_allocation = %d, tda = %d\n", retInfo->time_domain_allocation, tda);

  /* mark when retransmission will happen */
  int slots_frame = nrmac->frame_structure.numb_slots_frame;
  new_sched.frame = (frame + (slot + tda_info->k2 + get_NTN_Koffset(scc)) / slots_frame) % MAX_FRAME_NUMBER;
  new_sched.slot = (slot + tda_info->k2 + get_NTN_Koffset(scc)) % slots_frame;
  new_sched.bwp_info = bwp_info;
  DevAssert(new_sched.ul_harq_pid == harq_pid);

  bool reuse_old_tda = retInfo->time_domain_allocation == tda;
  if (reuse_old_tda && nrOfLayers == retInfo->nrOfLayers) {
    /* Check the resource is enough for retransmission */
    const uint16_t slbitmap = SL_to_bitmap(retInfo->tda_info.startSymbolIndex, retInfo->tda_info.nrOfSymbols);
    while (rbStart < bwpSize && (rballoc_mask[rbStart + bwpStart] & slbitmap))
      rbStart++;
    if (rbStart + retInfo->rbSize > bwpSize) {
      LOG_D(NR_MAC, "[UE %04x][%4d.%2d] could not allocate UL retransmission: no resources (rbStart %d, retInfo->rbSize %d, bwpSize %d) \n",
            UE->rnti,
            frame,
            slot,
            rbStart,
            retInfo->rbSize,
            bwpSize);
      return false;
    }
    new_sched.rbStart = rbStart;
    LOG_D(NR_MAC, "Retransmission keeping TDA %d and TBS %d\n", tda, retInfo->tb_size);
  } else {
    NR_pusch_dmrs_t dmrs_info = get_ul_dmrs_params(scc, ul_bwp, tda_info, nrOfLayers);
    /* the retransmission will use a different time domain allocation, check
     * that we have enough resources */
    const uint16_t slbitmap = SL_to_bitmap(tda_info->startSymbolIndex, tda_info->nrOfSymbols);
    while (rbStart < bwpSize && (rballoc_mask[rbStart + bwpStart] & slbitmap))
      rbStart++;
    if (rbStart >= bwpSize) {
      LOG_D(NR_MAC, "[UE %04x][%4d.%2d] could not allocate UL retransmission: no resources\n", UE->rnti, frame, slot);
      return false;
    }
    int rbSize = 0;
    while (rbStart + rbSize < bwpSize && !(rballoc_mask[rbStart + bwpStart + rbSize] & slbitmap))
      rbSize++;
    uint32_t new_tbs;
    uint16_t new_rbSize;
    bool success = nr_find_nb_rb(retInfo->Qm,
                                 retInfo->R,
                                 UE->current_UL_BWP.transform_precoding,
                                 nrOfLayers,
                                 tda_info->nrOfSymbols,
                                 dmrs_info.N_PRB_DMRS * dmrs_info.num_dmrs_symb,
                                 retInfo->tb_size,
                                 1, /* minimum of 1RB: need to find exact TBS, don't preclude any number */
                                 rbSize,
                                 &new_tbs,
                                 &new_rbSize);
    if (!success || new_tbs != retInfo->tb_size) {
      LOG_D(NR_MAC, "[UE %04x][%4d.%2d] allocation of UL retransmission failed: new TBsize %d of new TDA does not match old TBS %d \n",
            UE->rnti,
            frame,
            slot,
            new_tbs,
            retInfo->tb_size);
      return false; /* the maximum TBsize we might have is smaller than what we need */
    }
    LOG_D(NR_MAC, "Retransmission with TDA %d->%d and TBS %d -> %d\n", retInfo->time_domain_allocation, tda, retInfo->tb_size, new_tbs);
    /* we can allocate it. Overwrite the time_domain_allocation, the number
     * of RBs, and the new TB size. The rest is done below */
    new_sched.rbSize = new_rbSize;
    new_sched.rbStart = rbStart;
    new_sched.tb_size = new_tbs;
    new_sched.time_domain_allocation = tda;
    new_sched.tda_info = *tda_info;
    new_sched.dmrs_info = dmrs_info;
  }

  /* Find a free CCE */
  int CCEIndex = get_cce_index(nrmac,
                               CC_id,
                               slot,
                               UE->rnti,
                               &sched_ctrl->aggregation_level,
                               dci_beam_idx,
                               sched_ctrl->search_space,
                               sched_ctrl->coreset,
                               &sched_ctrl->sched_pdcch,
                               sched_ctrl->pdcch_cl_adjust);
  if (CCEIndex<0) {
    LOG_D(NR_MAC, "[UE %04x][%4d.%2d] no free CCE for retransmission UL DCI UE\n", UE->rnti, frame, slot);
    sched_ctrl->ul_cce_fail++;
    return false;
  }

  sched_ctrl->cce_index = CCEIndex;
  fill_pdcch_vrb_map(nrmac, CC_id, &sched_ctrl->sched_pdcch, CCEIndex, sched_ctrl->aggregation_level, dci_beam_idx);

  // signal new allocation
  DevAssert(new_sched.time_domain_allocation == tda);
  post_process_ulsch(nrmac, pp_pusch, UE, &new_sched);
  LOG_D(NR_MAC,
        "%4d.%2d Allocate UL retransmission RNTI %04x sched %4d.%2d (%d RBs)\n",
        frame,
        slot,
        UE->rnti,
        new_sched.frame,
        new_sched.slot,
        new_sched.rbSize);

  /* Mark the corresponding RBs as used */
  n_rb_sched -= new_sched.rbSize;
  for (int rb = bwpStart; rb < new_sched.rbSize; rb++)
    rballoc_mask[rb + new_sched.rbStart] |= SL_to_bitmap(new_sched.tda_info.startSymbolIndex, new_sched.tda_info.nrOfSymbols);
  return true;
}

typedef struct UEsched_s {
  float coef;
  bool sched_inactive;
  NR_UE_info_t * UE;
  int selected_mcs;
} UEsched_t;

static int comparator(const void *p, const void *q)
{
  const UEsched_t *pp = p;
  const UEsched_t *qq = q;
  if (pp->sched_inactive && qq->sched_inactive)
    return 0;
  else if (pp->sched_inactive)
    return -1;
  else if (qq->sched_inactive)
    return 1;
  /* both UEs are !sched_inactive */
  if (pp->coef < qq->coef)
    return 1;
  else if (pp->coef > qq->coef)
    return -1;
  return 0;
}

static int  pf_ul(gNB_MAC_INST *nrmac,
                  post_process_pusch_t *pp_pusch,
                  int tda,
                  const NR_tda_info_t *tda_info,
                  NR_UE_info_t *UE_list[],
                  int max_num_ue,
                  int num_beams,
                  int n_rb_sched[num_beams])
{
  const int CC_id = 0;
  int frame = pp_pusch->frame;
  int slot = pp_pusch->slot;
  NR_ServingCellConfigCommon_t *scc = nrmac->common_channels[CC_id].ServingCellConfigCommon;
  int slots_per_frame = nrmac->frame_structure.numb_slots_frame;
  DevAssert(tda_info->valid_tda);
  const int k2 = tda_info->k2 + get_NTN_Koffset(scc);
  const int sched_frame = (frame + (slot + k2) / slots_per_frame) % MAX_FRAME_NUMBER;
  const int sched_slot = (slot + k2) % slots_per_frame;
  DevAssert(is_ul_slot(sched_slot, &nrmac->frame_structure));

  const int min_rb = nrmac->min_grant_prb;
  // UEs that could be scheduled
  UEsched_t UE_sched[MAX_MOBILES_PER_GNB + 1] = {0};
  int remainUEs[num_beams];
  for (int i = 0; i < num_beams; i++)
    remainUEs[i] = max_num_ue;
  int numUE = 0;
  bool scheduled_something = false;

  /* Loop UE_list to calculate throughput and coeff */
  UE_iterator(UE_list, UE) {

    NR_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
    if (!nr_mac_ue_is_active(UE))
      continue;

    LOG_D(NR_MAC,"pf_ul: preparing UL scheduling for UE %04x\n",UE->rnti);
    NR_UE_UL_BWP_t *current_BWP = &UE->current_UL_BWP;

    NR_mac_dir_stats_t *stats = &UE->mac_stats.ul;

    /* Calculate throughput */
    const float a = 0.01f;
    const uint32_t b = stats->current_bytes;
    UE->ul_thr_ue = (1 - a) * UE->ul_thr_ue + a * b;

    stats->current_bytes = 0;
    stats->current_rbs = 0;

    int total_rem_ues = 0;
    for (int i = 0; i < num_beams; i++)
      total_rem_ues += remainUEs[i];
    if (total_rem_ues == 0)
      continue;

    NR_beam_alloc_t dci_beam = beam_allocation_procedure(&nrmac->beam_info, frame, slot, UE->UE_beam_index, slots_per_frame);
    if (dci_beam.idx < 0) {
      LOG_D(NR_MAC, "[UE %04x][%4d.%2d] Beam could not be allocated\n", UE->rnti, frame, slot);
      continue;
    }

    NR_beam_alloc_t beam = beam_allocation_procedure(&nrmac->beam_info, sched_frame, sched_slot, UE->UE_beam_index, slots_per_frame);
    if (beam.idx < 0) {
      LOG_D(NR_MAC, "[UE %04x][%4d.%2d] Beam could not be allocated\n", UE->rnti, frame, slot);
      reset_beam_status(&nrmac->beam_info, frame, slot, UE->UE_beam_index, slots_per_frame, dci_beam.new_beam);
      continue;
    }
    const int index = ul_buffer_index(sched_frame, sched_slot, slots_per_frame, nrmac->vrb_map_UL_size);
    uint16_t *rballoc_mask = &nrmac->common_channels[CC_id].vrb_map_UL[beam.idx][index * MAX_BWP_SIZE];

    /* Check if retransmission is necessary */
    int ul_harq_pid = sched_ctrl->retrans_ul_harq.head;
    LOG_D(NR_MAC,"pf_ul: UE %04x harq_pid %d\n", UE->rnti, ul_harq_pid);
    if (ul_harq_pid >= 0) {
      /* Allocate retransmission*/
      bool r = allocate_ul_retransmission(nrmac,
                                          pp_pusch,
                                          rballoc_mask,
                                          &n_rb_sched[beam.idx],
                                          dci_beam.idx,
                                          UE,
                                          ul_harq_pid,
                                          scc,
                                          tda,
                                          tda_info);
      if (!r) {
        LOG_D(NR_MAC, "[UE %04x][%4d.%2d] UL retransmission could not be allocated\n", UE->rnti, frame, slot);
        reset_beam_status(&nrmac->beam_info, sched_frame, sched_slot, UE->UE_beam_index, slots_per_frame, beam.new_beam);
        reset_beam_status(&nrmac->beam_info, frame, slot, UE->UE_beam_index, slots_per_frame, dci_beam.new_beam);
        continue;
      }
      else
        LOG_D(NR_MAC,"%4d.%2d UL Retransmission UE RNTI %04x to be allocated, max_num_ue %d\n", frame, slot, UE->rnti,max_num_ue);

      /* reduce max_num_ue once we are sure UE can be allocated, i.e., has CCE */
      remainUEs[beam.idx]--;
      scheduled_something = true;
      continue;
    }
    DevAssert(ul_harq_pid == -1); // all other allocations must be new ones

    /* skip this UE if there are no free HARQ processes. This can happen e.g.
     * if the UE disconnected in L2sim, in which case the gNB is not notified
     * (this can be considered a design flaw) */
    if (sched_ctrl->available_ul_harq.head < 0) {
      reset_beam_status(&nrmac->beam_info, sched_frame, sched_slot, UE->UE_beam_index, slots_per_frame, beam.new_beam);
      reset_beam_status(&nrmac->beam_info, frame, slot, UE->UE_beam_index, slots_per_frame, dci_beam.new_beam);
      LOG_D(NR_MAC, "[UE %04x][%4d.%2d] has no free UL HARQ process, skipping\n", UE->rnti, frame, slot);
      continue;
    }

    const int B = max(0, sched_ctrl->estimated_ul_buffer - sched_ctrl->sched_ul_bytes);
    /* preprocessor computed sched_frame/sched_slot */
    const bool do_sched = nr_UE_is_to_be_scheduled(&nrmac->frame_structure,
                                                   UE,
                                                   sched_frame,
                                                   sched_slot,
                                                   nrmac->ulsch_max_frame_inactivity);

    LOG_D(NR_MAC,"pf_ul: do_sched UE %04x => %s\n", UE->rnti, do_sched ? "yes" : "no");
    if ((B == 0 && !do_sched) || nr_timer_is_active(&sched_ctrl->transm_interrupt)) {
      reset_beam_status(&nrmac->beam_info, sched_frame, sched_slot, UE->UE_beam_index, slots_per_frame, beam.new_beam);
      reset_beam_status(&nrmac->beam_info, frame, slot, UE->UE_beam_index, slots_per_frame, dci_beam.new_beam);
      continue;
    }

    const NR_bler_options_t *bo = &nrmac->ul_bler;
    const int max_mcs_table = (current_BWP->mcs_table == 0 || current_BWP->mcs_table == 2) ? 28 : 27;
    const int max_mcs = min(bo->max_mcs, max_mcs_table); /* no per-user maximum MCS yet */
    int selected_mcs;
    int nrOfLayers = get_ul_nrOfLayers(sched_ctrl, current_BWP->dci_format);
    if (bo->harq_round_max == 1) {
      selected_mcs = get_mcs_from_SINRx10(current_BWP->mcs_table, sched_ctrl->pusch_snrx10, nrOfLayers);
      selected_mcs = min(max_mcs, selected_mcs);
      selected_mcs = max(bo->min_mcs, selected_mcs);
      sched_ctrl->ul_bler_stats.mcs = selected_mcs;
    } else {
      selected_mcs = get_mcs_from_bler(bo, stats, &sched_ctrl->ul_bler_stats, max_mcs, frame);
      LOG_D(NR_MAC, "%d.%d starting mcs %d bler %f\n", frame, slot, selected_mcs, sched_ctrl->ul_bler_stats.bler);
    }

    /* Create UE_sched for UEs eligibale for new data transmission*/
    /* Calculate coefficient*/
    const uint8_t Qm = nr_get_Qm_ul(selected_mcs, current_BWP->mcs_table);
    const uint16_t R = nr_get_code_rate_ul(selected_mcs, current_BWP->mcs_table);
    const uint32_t tbs = nr_compute_tbs(Qm,
                                        R,
                                        1, /* rbSize */
                                        10, /* hypothetical number of slots */
                                        0, /* N_PRB_DMRS * N_DMRS_SLOT */
                                        0 /* N_PRB_oh, 0 for initialBWP */,
                                        0 /* tb_scaling */,
                                        nrOfLayers)
                         >> 3;
    float coeff_ue = (float) tbs / UE->ul_thr_ue;
    bool sched_inactive = B == 0 && do_sched;
    LOG_D(NR_MAC, "[UE %04x][%4d.%2d] b %d, ul_thr_ue %f, tbs %d, coeff_ue %f, sched_inactive %d\n",
          UE->rnti,
          frame,
          slot,
          b,
          UE->ul_thr_ue,
          tbs,
          coeff_ue,
          sched_inactive);
    UE_sched[numUE].coef = coeff_ue;
    UE_sched[numUE].sched_inactive = sched_inactive;
    UE_sched[numUE].UE = UE;
    UE_sched[numUE].selected_mcs = selected_mcs;
    numUE++;
  }

  qsort(UE_sched, numUE, sizeof(UEsched_t), comparator);
  UEsched_t *iterator=UE_sched;

  /* Loop UE_sched to find max coeff and allocate transmission */
  while (iterator->UE != NULL) {
    NR_UE_UL_BWP_t *current_BWP = &iterator->UE->current_UL_BWP;
    NR_UE_sched_ctrl_t *sched_ctrl = &iterator->UE->UE_sched_ctrl;

    NR_beam_alloc_t beam = beam_allocation_procedure(&nrmac->beam_info, sched_frame, sched_slot, iterator->UE->UE_beam_index, slots_per_frame);
    if (beam.idx < 0) {
      LOG_D(NR_MAC, "[UE %04x][%4d.%2d] Beam could not be allocated\n", iterator->UE->rnti, frame, slot);
      iterator++;
      continue;
    }

    if (remainUEs[beam.idx] == 0 || n_rb_sched[beam.idx] < min_rb) {
      reset_beam_status(&nrmac->beam_info, sched_frame, sched_slot, iterator->UE->UE_beam_index, slots_per_frame, beam.new_beam);
      iterator++;
      continue;
    }

    NR_beam_alloc_t dci_beam = beam_allocation_procedure(&nrmac->beam_info, frame, slot, iterator->UE->UE_beam_index, slots_per_frame);
    if (dci_beam.idx < 0) {
      LOG_D(NR_MAC, "[UE %04x][%4d.%2d] Beam could not be allocated\n", iterator->UE->rnti, frame, slot);
      reset_beam_status(&nrmac->beam_info, sched_frame, sched_slot, iterator->UE->UE_beam_index, slots_per_frame, beam.new_beam);
      iterator++;
      continue;
    }

    int CCEIndex = get_cce_index(nrmac,
                                 CC_id, slot, iterator->UE->rnti,
                                 &sched_ctrl->aggregation_level,
                                 dci_beam.idx,
                                 sched_ctrl->search_space,
                                 sched_ctrl->coreset,
                                 &sched_ctrl->sched_pdcch,
                                 sched_ctrl->pdcch_cl_adjust);

    if (CCEIndex < 0) {
      sched_ctrl->ul_cce_fail++;
      reset_beam_status(&nrmac->beam_info, frame, slot, iterator->UE->UE_beam_index, slots_per_frame, dci_beam.new_beam);
      reset_beam_status(&nrmac->beam_info, sched_frame, sched_slot, iterator->UE->UE_beam_index, slots_per_frame, beam.new_beam);
      LOG_D(NR_MAC, "[UE %04x][%4d.%2d] no free CCE for UL DCI\n", iterator->UE->rnti, frame, slot);
      iterator++;
      continue;
    }
    else
      LOG_D(NR_MAC, "%4d.%2d free CCE for UL DCI UE %04x\n", frame, slot, iterator->UE->rnti);

    const int index = ul_buffer_index(sched_frame, sched_slot, slots_per_frame, nrmac->vrb_map_UL_size);
    uint16_t *rballoc_mask = &nrmac->common_channels[CC_id].vrb_map_UL[beam.idx][index * MAX_BWP_SIZE];

    /* find maximum amount of RBs that we can schedule starting from first free RB */
    int rbStart = 0;
    const uint16_t slbitmap = SL_to_bitmap(tda_info->startSymbolIndex, tda_info->nrOfSymbols);
    bwp_info_t bi = get_pusch_bwp_start_size(iterator->UE);
    while (rbStart < bi.bwpSize && (rballoc_mask[rbStart + bi.bwpStart] & slbitmap))
      rbStart++;
    /* if it's for inactivity, min_grant_prb is enough, otherwise check what
     * would be the maximum */
    uint16_t max_rbSize = iterator->sched_inactive ? min_rb : bi.bwpSize;
    uint16_t available_rb = 1;
    while (rbStart + available_rb < bi.bwpSize && !(rballoc_mask[rbStart + bi.bwpStart + available_rb] & slbitmap) && available_rb < max_rbSize)
      available_rb++;

    if (rbStart + min_rb > bi.bwpSize || available_rb < min_rb) {
      reset_beam_status(&nrmac->beam_info, frame, slot, iterator->UE->UE_beam_index, slots_per_frame, dci_beam.new_beam);
      reset_beam_status(&nrmac->beam_info, sched_frame, sched_slot, iterator->UE->UE_beam_index, slots_per_frame, beam.new_beam);
      LOG_D(NR_MAC, "[UE %04x][%4d.%2d] could not allocate UL data: no resources (rbStart %d, min_rb %d, bwpSize %d)\n",
            iterator->UE->rnti,
            frame,
            slot,
            rbStart,
            min_rb,
            bi.bwpSize);
      iterator++;
      continue;
    } else
      LOG_D(NR_MAC,
            "allocating UL data for RNTI %04x (rbStart %d, min_rb %d, available_rb %d, bwpSize %d)\n",
            iterator->UE->rnti,
            rbStart,
            min_rb,
            available_rb,
            bi.bwpSize);

    int nrOfLayers = get_ul_nrOfLayers(sched_ctrl, current_BWP->dci_format);
    NR_sched_pusch_t sched = {
      .frame = sched_frame,
      .slot = sched_slot,
      // rbSize set further below
      .rbStart = rbStart,
      .mcs = iterator->selected_mcs,
      // R, Qm, tb_size set further below
      .ul_harq_pid = -1, // new transmission
      .nrOfLayers = nrOfLayers,
      // tpmi in post-process
      .time_domain_allocation = tda,
      .tda_info = *tda_info,
      .dmrs_info = get_ul_dmrs_params(scc, current_BWP, tda_info, nrOfLayers),
      .bwp_info = bi,
      // phr_txpower_calc below
    };

    /* Calculate the current scheduling bytes */
    const int B = cmax(sched_ctrl->estimated_ul_buffer - sched_ctrl->sched_ul_bytes, 0);
    /* adjust rbSize and MCS according to PHR and BPRE, only if there is data */
    if((sched_ctrl->pcmax != 0 || sched_ctrl->ph != 0) && B > 0)
      nr_ue_max_mcs_min_rb(current_BWP->scs, sched_ctrl->ph, &sched, current_BWP, min_rb, B, &available_rb, &sched.mcs);

    if (sched.mcs < sched_ctrl->ul_bler_stats.mcs)
      sched_ctrl->ul_bler_stats.mcs = sched.mcs; /* force estimated MCS down */

    update_ul_ue_R_Qm(sched.mcs, current_BWP->mcs_table, current_BWP->pusch_Config, &sched.R, &sched.Qm);
    if (!iterator->sched_inactive) {
      // this UE has data, find optimal number of RB for data and in range
      // [min_rb,available_rb]
      nr_find_nb_rb(sched.Qm,
                    sched.R,
                    current_BWP->transform_precoding,
                    sched.nrOfLayers,
                    sched.tda_info.nrOfSymbols,
                    sched.dmrs_info.N_PRB_DMRS * sched.dmrs_info.num_dmrs_symb,
                    B,
                    min_rb,
                    available_rb,
                    &sched.tb_size,
                    &sched.rbSize);
    } else {
      // this UE is scheduled due to SR or inactivity and no data
      sched.rbSize = min_rb;
      sched.tb_size = nr_compute_tbs(sched.Qm,
                                     sched.R,
                                     sched.rbSize,
                                     sched.tda_info.nrOfSymbols,
                                     sched.dmrs_info.N_PRB_DMRS * sched.dmrs_info.num_dmrs_symb,
                                     0, // nb_rb_oh
                                     0,
                                     sched.nrOfLayers)
                      >> 3;
    }

    // Calacualte the normalized tx_power for PHR
    long *deltaMCS = current_BWP->pusch_Config ? current_BWP->pusch_Config->pusch_PowerControl->deltaMCS : NULL;
    int tbs_bits = sched.tb_size << 3;

    sched.phr_txpower_calc = compute_ph_factor(current_BWP->scs,
                                               tbs_bits,
                                               sched.rbSize,
                                               sched.nrOfLayers,
                                               sched.tda_info.nrOfSymbols,
                                               sched.dmrs_info.N_PRB_DMRS * sched.dmrs_info.num_dmrs_symb,
                                               deltaMCS,
                                               false);

    LOG_D(NR_MAC,
          "rbSize %d (available_rb %d), TBS %d, est buf %d, sched_ul %d, B %d, CCE %d, num_dmrs_symb %d, N_PRB_DMRS %d\n",
          sched.rbSize,
          available_rb,
          sched.tb_size,
          sched_ctrl->estimated_ul_buffer,
          sched_ctrl->sched_ul_bytes,
          B,
          sched_ctrl->cce_index,
          sched.dmrs_info.num_dmrs_symb,
          sched.dmrs_info.N_PRB_DMRS);

    /* Mark the corresponding RBs as used */

    sched_ctrl->cce_index = CCEIndex;
    fill_pdcch_vrb_map(nrmac, CC_id, &sched_ctrl->sched_pdcch, CCEIndex, sched_ctrl->aggregation_level, dci_beam.idx);

    /* save allocation to FAPI structures */
    post_process_ulsch(nrmac, pp_pusch, iterator->UE, &sched);

    n_rb_sched[beam.idx] -= sched.rbSize;
    for (int rb = bi.bwpStart; rb < sched.rbSize; rb++)
      rballoc_mask[rb + sched.rbStart] |= slbitmap;

    /* reduce max_num_ue once we are sure UE can be allocated, i.e., has CCE */
    remainUEs[beam.idx]--;
    iterator++;
    scheduled_something = true;
  }
  int num_ue_sched = num_beams * max_num_ue;
  for (int i = 0; i < num_beams; i++)
    num_ue_sched -= remainUEs[i];
  DevAssert((num_ue_sched == 0 && !scheduled_something) || (num_ue_sched > 0 && scheduled_something));
  return num_ue_sched;
}

nfapi_nr_pusch_pdu_t *prepare_pusch_pdu(nfapi_nr_ul_tti_request_t *future_ul_tti_req,
                                        const NR_UE_info_t *UE,
                                        const NR_ServingCellConfigCommon_t *scc,
                                        const NR_sched_pusch_t *sched_pusch,
                                        int transform_precoding,
                                        int harq_id,
                                        int harq_round,
                                        int fh,
                                        int rnti,
                                        nr_beam_mode_t beam_mode)
{
  nfapi_nr_pusch_pdu_t *pusch_pdu = &future_ul_tti_req->pdus_list[future_ul_tti_req->n_pdus].pusch_pdu;
  memset(pusch_pdu, 0, sizeof(nfapi_nr_pusch_pdu_t));
  const NR_UE_UL_BWP_t *ul_bwp = &UE->current_UL_BWP;

  pusch_pdu->pdu_bit_map = PUSCH_PDU_BITMAP_PUSCH_DATA;
  pusch_pdu->rnti = rnti;
  pusch_pdu->handle = 0; //not yet used
  pusch_pdu->bwp_size = sched_pusch->bwp_info.bwpSize;
  pusch_pdu->bwp_start = sched_pusch->bwp_info.bwpStart;
  pusch_pdu->subcarrier_spacing = ul_bwp->scs;
  pusch_pdu->cyclic_prefix = 0;
  pusch_pdu->target_code_rate = sched_pusch->R;
  pusch_pdu->qam_mod_order = sched_pusch->Qm;
  pusch_pdu->mcs_index = sched_pusch->mcs;
  pusch_pdu->mcs_table = ul_bwp->mcs_table;
  pusch_pdu->transform_precoding = transform_precoding;
  if (ul_bwp->pusch_Config && ul_bwp->pusch_Config->dataScramblingIdentityPUSCH)
    pusch_pdu->data_scrambling_id = *ul_bwp->pusch_Config->dataScramblingIdentityPUSCH;
  else
    pusch_pdu->data_scrambling_id = *scc->physCellId;
  pusch_pdu->nrOfLayers = sched_pusch->nrOfLayers;
  // DMRS
  pusch_pdu->num_dmrs_cdm_grps_no_data = sched_pusch->dmrs_info.num_dmrs_cdm_grps_no_data;
  pusch_pdu->dmrs_ports = ((1 << sched_pusch->nrOfLayers) - 1);
  pusch_pdu->ul_dmrs_symb_pos = sched_pusch->dmrs_info.ul_dmrs_symb_pos;
  pusch_pdu->dmrs_config_type = sched_pusch->dmrs_info.dmrs_config_type;
  pusch_pdu->scid = sched_pusch->dmrs_info.scid; // DMRS sequence initialization [TS38.211, sec 6.4.1.1.1]
  pusch_pdu->pusch_identity = sched_pusch->dmrs_info.pusch_identity;
  pusch_pdu->ul_dmrs_scrambling_id = sched_pusch->dmrs_info.dmrs_scrambling_id;
  /* Allocation in frequency domain */
  pusch_pdu->resource_alloc = 1; //type 1
  pusch_pdu->rb_start = sched_pusch->rbStart;
  pusch_pdu->rb_size = sched_pusch->rbSize;
  pusch_pdu->vrb_to_prb_mapping = 0;
  pusch_pdu->frequency_hopping = fh;
  /* Resource Allocation in time domain */
  pusch_pdu->start_symbol_index = sched_pusch->tda_info.startSymbolIndex;
  pusch_pdu->nr_of_symbols = sched_pusch->tda_info.nrOfSymbols;
  /* PUSCH PDU */
  pusch_pdu->pusch_data.rv_index = nr_get_rv(harq_round % 4);
  pusch_pdu->pusch_data.harq_process_id = harq_id;
  pusch_pdu->pusch_data.new_data_indicator = (harq_round == 0) ? 1 : 0; // not NDI but indicator for new transmission
  pusch_pdu->pusch_data.tb_size = sched_pusch->tb_size;
  pusch_pdu->pusch_data.num_cb = 0; // CBG not supported
  // Beamforming
  pusch_pdu->beamforming.num_prgs = 1;
  pusch_pdu->beamforming.prg_size = pusch_pdu->bwp_size;
  pusch_pdu->beamforming.dig_bf_interface = 1;
  pusch_pdu->beamforming.prgs_list[0].dig_bf_interface_list[0].beam_idx =
      convert_to_fapi_beam(UE->UE_beam_index, beam_mode);
  /* TRANSFORM PRECODING --------------------------------------------------------*/
  if (pusch_pdu->transform_precoding == NR_PUSCH_Config__transformPrecoder_enabled) {
    // U as specified in section 6.4.1.1.1.2 in 38.211, if sequence hopping and group hopping are disabled
    pusch_pdu->dfts_ofdm.low_papr_group_number = sched_pusch->dmrs_info.pusch_identity % 30;
    pusch_pdu->dfts_ofdm.low_papr_sequence_number = sched_pusch->dmrs_info.low_papr_sequence_number;
  }


  pusch_pdu->maintenance_parms_v3.ldpcBaseGraph = get_BG(sched_pusch->tb_size << 3, sched_pusch->R);
  pusch_pdu->maintenance_parms_v3.tbSizeLbrmBytes = 0;
  if (UE->sc_info.rateMatching_PUSCH) {
    // TBS_LBRM according to section 5.4.2.1 of 38.212
    long *maxMIMO_Layers = UE->sc_info.maxMIMO_Layers_PUSCH;
    if (!maxMIMO_Layers && ul_bwp && ul_bwp->pusch_Config)
      maxMIMO_Layers = ul_bwp->pusch_Config->maxRank;
    long lbrm_layers = maxMIMO_Layers ? *maxMIMO_Layers : ue_supported_ul_layers(UE->capability);
    AssertFatal (maxMIMO_Layers != NULL,"Option with max MIMO layers not configured is not supported\n");
    pusch_pdu->maintenance_parms_v3.tbSizeLbrmBytes = nr_compute_tbslbrm(ul_bwp->mcs_table, UE->sc_info.ul_bw_tbslbrm, lbrm_layers);
  }
  /* PUSCH PTRS */
  if (sched_pusch->dmrs_info.ptrsConfig) {
    bool valid_ptrs_setup = false;
    pusch_pdu->pusch_ptrs.ptrs_ports_list = calloc_or_fail(2, sizeof(nfapi_nr_ptrs_ports_t));
    valid_ptrs_setup = set_ul_ptrs_values(sched_pusch->dmrs_info.ptrsConfig,
                                          pusch_pdu->rb_size,
                                          pusch_pdu->mcs_index,
                                          pusch_pdu->mcs_table,
                                          &pusch_pdu->pusch_ptrs.ptrs_freq_density,
                                          &pusch_pdu->pusch_ptrs.ptrs_time_density,
                                          &pusch_pdu->pusch_ptrs.ptrs_ports_list->ptrs_re_offset,
                                          &pusch_pdu->pusch_ptrs.num_ptrs_ports,
                                          &pusch_pdu->pusch_ptrs.ul_ptrs_power,
                                          pusch_pdu->nr_of_symbols);
    if (valid_ptrs_setup == true)
      pusch_pdu->pdu_bit_map |= PUSCH_PDU_BITMAP_PUSCH_PTRS; // enable PUSCH PTRS
  } else
    pusch_pdu->pdu_bit_map &= ~PUSCH_PDU_BITMAP_PUSCH_PTRS; // disable PUSCH PTRS
  return pusch_pdu;
}

void post_process_ulsch(gNB_MAC_INST *nr_mac, post_process_pusch_t *pusch, NR_UE_info_t *UE, NR_sched_pusch_t *sched_pusch)
{
  frame_t frame = pusch->frame;
  slot_t slot = pusch->slot;

  NR_ServingCellConfigCommon_t *scc = nr_mac->common_channels[0].ServingCellConfigCommon;
  NR_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
  NR_UE_UL_BWP_t *current_BWP = &UE->current_UL_BWP;

  /* the UE now has the grant for the request */
  sched_ctrl->SR = false;

  int8_t harq_id = sched_pusch->ul_harq_pid;
  if (harq_id < 0) {
    /* PP has not selected a specific HARQ Process, get a new one */
    harq_id = sched_ctrl->available_ul_harq.head;
    AssertFatal(harq_id >= 0, "no free HARQ process available for UE %04x\n", UE->rnti);
    remove_front_nr_list(&sched_ctrl->available_ul_harq);
    sched_pusch->ul_harq_pid = harq_id;
  } else {
    /* PP selected a specific HARQ process. Check whether it will be a new
     * transmission or a retransmission, and remove from the corresponding
     * list */
    if (sched_ctrl->ul_harq_processes[harq_id].round == 0)
      remove_nr_list(&sched_ctrl->available_ul_harq, harq_id);
    else
      remove_nr_list(&sched_ctrl->retrans_ul_harq, harq_id);
  }
  NR_UE_ul_harq_t *cur_harq = &sched_ctrl->ul_harq_processes[harq_id];
  DevAssert(!cur_harq->is_waiting);
  if (nr_mac->radio_config.disable_harq) {
    finish_nr_ul_harq(sched_ctrl, harq_id);
  } else {
    add_tail_nr_list(&sched_ctrl->feedback_ul_harq, harq_id);
    cur_harq->feedback_slot = sched_pusch->slot;
    cur_harq->is_waiting = true;
  }

  /* Statistics */
  AssertFatal(cur_harq->round < nr_mac->ul_bler.harq_round_max, "Indexing ulsch_rounds[%d] is out of bounds\n", cur_harq->round);
  UE->mac_stats.ul.rounds[cur_harq->round]++;
  /* Save information on MCS, TBS etc for the current initial transmission
   * so we have access to it when retransmitting */
  cur_harq->sched_pusch = *sched_pusch;
  if (cur_harq->round == 0) {
    UE->mac_stats.ulsch_total_bytes_scheduled += sched_pusch->tb_size;
    sched_ctrl->sched_ul_bytes += sched_pusch->tb_size;
    UE->mac_stats.ul.total_rbs += sched_pusch->rbSize;

  } else {
    UE->mac_stats.ul.total_rbs_retx += sched_pusch->rbSize;
  }
  UE->mac_stats.ul.current_bytes = sched_pusch->tb_size;
  UE->mac_stats.ul.current_rbs = sched_pusch->rbSize;
  nr_mac->mac_stats.ul.used_prb_aggregate += sched_pusch->rbSize;
  sched_ctrl->last_ul_frame = sched_pusch->frame;
  sched_ctrl->last_ul_slot = sched_pusch->slot;

  LOG_D(NR_MAC,
        "ULSCH/PUSCH: %4d.%2d RNTI %04x UL sched %4d.%2d DCI L %d start %2d RBS %3d TDA %2d dmrs_pos %x MCS "
        "Table %2d MCS %2d nrOfLayers %2d num_dmrs_cdm_grps_no_data %2d TBS %4d HARQ PID %2d round %d RV %d NDI %d est %6d sched "
        "%6d est BSR %6d TPC %d\n",
        frame,
        slot,
        UE->rnti,
        sched_pusch->frame,
        sched_pusch->slot,
        sched_ctrl->aggregation_level,
        sched_pusch->rbStart,
        sched_pusch->rbSize,
        sched_pusch->time_domain_allocation,
        sched_pusch->dmrs_info.ul_dmrs_symb_pos,
        current_BWP->mcs_table,
        sched_pusch->mcs,
        sched_pusch->nrOfLayers,
        sched_pusch->dmrs_info.num_dmrs_cdm_grps_no_data,
        sched_pusch->tb_size,
        harq_id,
        cur_harq->round,
        nr_get_rv(cur_harq->round % 4),
        cur_harq->ndi,
        sched_ctrl->estimated_ul_buffer,
        sched_ctrl->sched_ul_bytes,
        sched_ctrl->estimated_ul_buffer - sched_ctrl->sched_ul_bytes,
        sched_ctrl->tpc0);

  T(T_GNB_MAC_UL, T_INT(UE->rnti), T_INT(frame), T_INT(slot), T_INT(sched_pusch->mcs), T_INT(sched_pusch->tb_size));

  /* PUSCH in a later slot, but corresponding DCI now! */
  const int index = ul_buffer_index(sched_pusch->frame,
                                    sched_pusch->slot,
                                    nr_mac->frame_structure.numb_slots_frame,
                                    nr_mac->UL_tti_req_ahead_size);
  nfapi_nr_ul_tti_request_t *req = &nr_mac->UL_tti_req_ahead[0][index];
  if (req->SFN != sched_pusch->frame || req->Slot != sched_pusch->slot)
    LOG_W(NR_MAC,
          "%d.%d future UL_tti_req's frame.slot %d.%d does not match PUSCH %d.%d\n",
          frame,
          slot,
          req->SFN,
          req->Slot,
          sched_pusch->frame,
          sched_pusch->slot);
  AssertFatal(req->n_pdus < sizeof(req->pdus_list) / sizeof(req->pdus_list[0]),
              "Invalid UL_tti_req_ahead->n_pdus %d\n",
              req->n_pdus);

  req->pdus_list[req->n_pdus].pdu_type = NFAPI_NR_UL_CONFIG_PUSCH_PDU_TYPE;
  req->pdus_list[req->n_pdus].pdu_size = sizeof(nfapi_nr_pusch_pdu_t);
  nfapi_nr_pusch_pdu_t *pusch_pdu = prepare_pusch_pdu(req,
                                                      UE,
                                                      scc,
                                                      sched_pusch,
                                                      current_BWP->transform_precoding,
                                                      harq_id,
                                                      cur_harq->round,
                                                      current_BWP->pusch_Config && current_BWP->pusch_Config->frequencyHopping,
                                                      UE->rnti,
                                                      nr_mac->beam_info.beam_mode);
  req->n_pdus += 1;

  // Calculate the normalized tx_power for PHR
  long *deltaMCS = current_BWP->pusch_Config ? current_BWP->pusch_Config->pusch_PowerControl->deltaMCS : NULL;
  int tbs_bits = pusch_pdu->pusch_data.tb_size << 3;
  sched_pusch->phr_txpower_calc = compute_ph_factor(current_BWP->scs,
                                                    tbs_bits,
                                                    sched_pusch->rbSize,
                                                    sched_pusch->nrOfLayers,
                                                    sched_pusch->tda_info.nrOfSymbols,
                                                    sched_pusch->dmrs_info.N_PRB_DMRS * sched_pusch->dmrs_info.num_dmrs_symb,
                                                    deltaMCS,
                                                    false);

  /* a PDCCH PDU groups DCIs per BWP and CORESET. Save a pointer to each
   * allocated PDCCH so we can easily allocate UE's DCIs independent of any
   * CORESET order */
  NR_SearchSpace_t *ss = sched_ctrl->search_space;
  NR_ControlResourceSet_t *coreset = sched_ctrl->coreset;
  const int coresetid = coreset->controlResourceSetId;
  nfapi_nr_dl_tti_pdcch_pdu_rel15_t *pdcch_pdu = pusch->pdcch_pdu_coreset[coresetid];
  if (!pdcch_pdu) {
    nfapi_nr_ul_dci_request_pdus_t *ul_dci_request_pdu = &pusch->ul_dci_req->ul_dci_pdu_list[pusch->ul_dci_req->numPdus];
    memset(ul_dci_request_pdu, 0, sizeof(nfapi_nr_ul_dci_request_pdus_t));
    ul_dci_request_pdu->PDUType = NFAPI_NR_DL_TTI_PDCCH_PDU_TYPE;
    ul_dci_request_pdu->PDUSize = (uint8_t)(4 + sizeof(nfapi_nr_dl_tti_pdcch_pdu));
    pdcch_pdu = &ul_dci_request_pdu->pdcch_pdu.pdcch_pdu_rel15;
    pusch->ul_dci_req->numPdus += 1;
    nr_configure_pdcch(pdcch_pdu, coreset, &sched_ctrl->sched_pdcch);
    pusch->pdcch_pdu_coreset[coresetid] = pdcch_pdu;
  }

  LOG_D(NR_MAC, "Configuring ULDCI/PDCCH in %d.%d at CCE %d, rnti %04x\n", frame, slot, sched_ctrl->cce_index, UE->rnti);

  /* Fill PDCCH DL DCI PDU */
  nfapi_nr_dl_dci_pdu_t *dci_pdu = prepare_dci_pdu(pdcch_pdu,
                                                   scc,
                                                   ss,
                                                   coreset,
                                                   sched_ctrl->aggregation_level,
                                                   sched_ctrl->cce_index,
                                                   convert_to_fapi_beam(UE->UE_beam_index, nr_mac->beam_info.beam_mode),
                                                   UE->rnti);
  pdcch_pdu->numDlDci++;

  dci_pdu_rel15_t uldci_payload;
  memset(&uldci_payload, 0, sizeof(uldci_payload));
  if (current_BWP->dci_format == NR_UL_DCI_FORMAT_0_1)
    LOG_D(NR_MAC_DCI,
          "add ul dci harq %d for %d.%d %d.%d round %d\n",
          harq_id,
          frame,
          slot,
          sched_pusch->frame,
          sched_pusch->slot,
          sched_ctrl->ul_harq_processes[harq_id].round);

  // If nrOfLayers is the same as in srs_feedback, we use the best TPMI, i.e. the one in srs_feedback.
  // Otherwise, we use the valid TPMI that we saved in the first transmission.
  int *tpmi = NULL;
  if (pusch_pdu->nrOfLayers != (sched_ctrl->srs_feedback.ul_ri + 1))
    tpmi = &sched_pusch->tpmi;
  config_uldci(&UE->sc_info,
               pusch_pdu,
               &uldci_payload,
               &sched_ctrl->srs_feedback,
               tpmi,
               sched_pusch->time_domain_allocation,
               UE->UE_sched_ctrl.tpc0,
               cur_harq->ndi,
               current_BWP,
               ss->searchSpaceType->present);

  // Reset TPC to 0 dB to not request new gain multiple times before computing new value for SNR
  UE->UE_sched_ctrl.tpc0 = 1;

  fill_dci_pdu_rel15(&UE->sc_info,
                     &UE->current_DL_BWP,
                     current_BWP,
                     dci_pdu,
                     &uldci_payload,
                     current_BWP->dci_format,
                     TYPE_C_RNTI_,
                     ss,
                     coreset,
                     UE->pdsch_HARQ_ACK_Codebook,
                     nr_mac->cset0_bwp_size);
}

fsn_t fs_add_delta(const frame_structure_t *fs, uint32_t delta, fsn_t fsn)
{
  const int slots_frame = fs->numb_slots_frame;
  fsn_t res = {
    .f = (fsn.f + (fsn.s + delta) / slots_frame) % MAX_FRAME_NUMBER,
    .s = (fsn.s + delta) % slots_frame,
  };
  return res;
}
int fs_get_diff(const frame_structure_t *fs, fsn_t a, fsn_t b)
{
  const int slots_frame = fs->numb_slots_frame;
  int diff = (a.f * slots_frame + a.s) - (b.f * slots_frame + b.s);
  if (diff < -512 * slots_frame)
    diff += 1024 * slots_frame;
  else if (diff > 512 * slots_frame)
    diff -= 1024 * slots_frame;
  return diff;
}
fsn_t fs_get_max(const frame_structure_t *fs, fsn_t a, fsn_t b)
{
  if (fs_get_diff(fs, a, b) <= 0) {
    return b;
  } else {
    return a;
  }
}

static void nr_ulsch_preprocessor(gNB_MAC_INST *nr_mac, post_process_pusch_t *pp_pusch)
{
  int frame = pp_pusch->frame;
  int slot = pp_pusch->slot;

  NR_COMMON_channels_t *cc = nr_mac->common_channels;
  NR_ServingCellConfigCommon_t *scc = cc->ServingCellConfigCommon;
  AssertFatal(scc, "We need one serving cell config common\n");
  const frame_structure_t *fs = &nr_mac->frame_structure;

  // we assume the same K2 for all UEs
  const int koffset = get_NTN_Koffset(scc);
  const int min_rxtx = nr_mac->radio_config.minRXTXTIME + koffset;

  int num_beams = nr_mac->beam_info.beam_allocation ? nr_mac->beam_info.beams_per_period : 1;
  int bw = scc->uplinkConfigCommon->frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth;

  int average_agg_level = 4; // TODO find a better estimation
  int max_dci = bw / (average_agg_level * NR_NB_REG_PER_CCE);

  // FAPI cannot handle more than MAX_DCI_CORESET DCIs
  max_dci = min(max_dci, MAX_DCI_CORESET);

  fsn_t current = {frame, slot};
  fsn_t min_next = fs_add_delta(fs, min_rxtx, current);
  /* if it's the last DL slot, we should try all TDAs to make sure that the
   * scheduler can reach e.g. the next mixed slot. Otherwise, if we don't, we
   * might starve HARQ processes that need a retransmission in a specific slot
   * but we might not necessarily reach it */
  bool last_dl = (current.s % fs->numb_slots_period) == (fs->period_cfg.num_dl_slots - 1);
  fsn_t *next = &nr_mac->ul_next;
  while (max_dci > 0) {
    /* go to the next UL slot, skipping DL if necessary */
    *next = fs_get_max(fs, *next, min_next);
    while (!is_ul_slot(next->s, fs))
      *next = fs_add_delta(fs, 1, *next);
    if (!is_dl_slot(current.s, fs)) // if current slot is not DL, nothing to do
      break;

    /* get a TDA so that next UL scheduling slot can be reached from current,
     * and exit if there is no such TDA. Remove koffset, as it is
     * "cell-specific", i.e., the UE will add it on the computation. */
    int k2 = fs_get_diff(fs, *next, current) - koffset;
    DevAssert(k2 > 0);
    // we assume that all beams have the same symbol utilization in all RBs for
    // simplification (but this might not be true). Otherwise, we would need to
    // check it separately for all beams and give a list of TDAs (one for each
    // beam) in pf_ul(), which is complex. We should rewrite pf_ul() to
    // schedule for one beam only instead (and for one BWP).
    int beam = 0;
    const NR_tda_info_t *tda_info = NULL;
    int n_tda = get_num_ul_tda(nr_mac, next->s, k2, &tda_info);
    if (n_tda == 0) /* no TDA fulfills this */
      break;
    int rb_start = 0;
    int rb_len = bw;
    tda_info = get_best_ul_tda(nr_mac, beam, tda_info, n_tda, next->f, next->s, &rb_start, &rb_len);
    DevAssert(tda_info->valid_tda);
    int tda = seq_arr_dist(&nr_mac->ul_tda, seq_arr_front(&nr_mac->ul_tda), tda_info);
    AssertFatal(tda >= 0 && tda < 16, "illegal TDA index %d\n", tda);

    nr_mac->mac_stats.ul.total_prb_aggregate += bw;
    int len[num_beams];
    for (int i = 0; i < num_beams; i++)
      len[i] = rb_len;
    /* proportional fair scheduling algorithm */
    int sched = pf_ul(nr_mac, pp_pusch, tda, tda_info, nr_mac->UE_info.connected_ue_list, max_dci, num_beams, len);
    LOG_D(NR_MAC, "run pf_ul() at %4d.%2d with tda %d k2 %d (ULSCH at %4d.%2d) scheduled %d last_dl %d\n", frame, slot, tda, k2, next->f, next->s, sched, last_dl);
    /* if we did not schedule anything, and it's not the last slot, break. In
     * the case we did schedule or it's the last slot (see above!), continue
     * advancing till there is no TDA anymore */
    if (sched == 0 && !last_dl)
      break;
    max_dci -= sched;

    *next = fs_add_delta(fs, 1, *next);
  }
}

nr_pp_impl_ul nr_init_ulsch_preprocessor(int CC_id)
{
  return nr_ulsch_preprocessor;
}

void nr_schedule_ulsch(module_id_t module_id, frame_t frame, slot_t slot, nfapi_nr_ul_dci_request_t *ul_dci_req)
{
  gNB_MAC_INST *nr_mac = RC.nrmac[module_id];
  /* already mutex protected: held in gNB_dlsch_ulsch_scheduler() */
  NR_SCHED_ENSURE_LOCKED(&nr_mac->sched_lock);

  ul_dci_req->SFN = frame;
  ul_dci_req->Slot = slot;
  post_process_pusch_t pusch = { .frame = frame, .slot = slot, .ul_dci_req = ul_dci_req, .pdcch_pdu_coreset = {NULL}, };

  nr_mac->pre_processor_ul(nr_mac, &pusch);
}
