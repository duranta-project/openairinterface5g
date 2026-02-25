/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*! \file PHY/NR_TRANSPORT/nr_ulsch_decoding_slot.c
 * \brief Top-level routines for decoding  LDPC (ULSCH) transport channels from 38.212, V15.4.0 2018-12
 */

// [from gNB coding]
#include "PHY/defs_gNB.h"
#include "PHY/CODING/coding_extern.h"
#include "PHY/CODING/coding_defs.h"
#include "PHY/CODING/lte_interleaver_inline.h"
#include "PHY/CODING/nrLDPC_coding/nrLDPC_coding_interface.h"
#include "PHY/CODING/nrLDPC_extern.h"
#include "PHY/NR_TRANSPORT/nr_transport_common_proto.h"
#include "PHY/NR_TRANSPORT/nr_transport_proto.h"
#include "PHY/NR_TRANSPORT/nr_ulsch.h"
#include "PHY/NR_TRANSPORT/nr_dlsch.h"
#include "SCHED_NR/sched_nr.h"
#include "defs.h"
#include "bits.h"
#include "common/utils/LOG/log.h"
#include <syscall.h>
// #define DEBUG_ULSCH_DECODING
// #define gNB_DEBUG_TRACE

#define OAI_UL_LDPC_MAX_NUM_LLR 27000 // 26112 // NR_LDPC_NCOL_BG1*NR_LDPC_ZMAX = 68*384
// #define DEBUG_CRC
#ifdef DEBUG_CRC
#define PRINT_CRC_CHECK(a) a
#else
#define PRINT_CRC_CHECK(a)
#endif

void free_gNB_ulsch(NR_gNB_ULSCH_t *ulsch, uint16_t N_RB_UL)
{
  uint16_t a_segments = MAX_NUM_NR_ULSCH_SEGMENTS; // number of segments to be allocated

  if (N_RB_UL != 273) {
    a_segments = a_segments * N_RB_UL;
    a_segments = a_segments / 273 + 1;
  }

  if (ulsch->harq_process) {
    if (ulsch->harq_process->b) {
      free_and_zero(ulsch->harq_process->b);
      ulsch->harq_process->b = NULL;
    }
    free_and_zero(ulsch->harq_process->c);
    free_and_zero(ulsch->harq_process->d);
    free_and_zero(ulsch->harq_process);
    ulsch->harq_process = NULL;
  }
}

NR_gNB_ULSCH_t new_gNB_ulsch(uint8_t max_ldpc_iterations, uint16_t N_RB_UL)
{
  uint16_t a_segments = MAX_NUM_NR_ULSCH_SEGMENTS; // number of segments to be allocated

  if (N_RB_UL != 273) {
    a_segments = a_segments * N_RB_UL;
    a_segments = a_segments / 273 + 1;
  }

  uint32_t ulsch_bytes = a_segments * 1056; // allocated bytes per segment
  NR_gNB_ULSCH_t ulsch = {0};

  ulsch.max_ldpc_iterations = max_ldpc_iterations;
  ulsch.harq_pid = -1;
  ulsch.active = false;

  NR_UL_gNB_HARQ_t *harq = malloc16_clear(sizeof(*harq));
  init_abort(&harq->abort_decode);
  ulsch.harq_process = harq;
  harq->b = malloc16_clear(ulsch_bytes * sizeof(*harq->b));
  // Allocate one contiguous buffer fr all c/d arrays to simplify addressing for GPU LDPC offload
  harq->c = malloc16_clear(a_segments * 8448 * sizeof(*harq->c));
  harq->d = malloc16_clear(a_segments * 68 * 384 * sizeof(*harq->d));
  return (ulsch);
}

int nr_ulsch_decoding(PHY_VARS_gNB *phy_vars_gNB,
                      NR_DL_FRAME_PARMS *frame_parms,
                      uint32_t frame,
                      uint8_t nr_tti_rx,
                      int *ULSCH_ids,
                      int nb_pusch)
{
  nrLDPC_TB_decoding_parameters_t TBs[nb_pusch];
  memset(TBs, 0, sizeof(TBs));
  nrLDPC_slot_decoding_parameters_t slot_parameters = {.frame = frame,
                                                       .slot = nr_tti_rx,
                                                       .nb_TBs = nb_pusch,
                                                       .threadPool = &phy_vars_gNB->threadPool,
                                                       .TBs = TBs};

  int max_num_segments = 0;

  for (uint8_t pusch_id = 0; pusch_id < nb_pusch; pusch_id++) {
    uint8_t ULSCH_id = ULSCH_ids[pusch_id];
    NR_gNB_ULSCH_t *ulsch = &phy_vars_gNB->ulsch[ULSCH_id];
    NR_gNB_PUSCH *pusch = &phy_vars_gNB->pusch_vars[ULSCH_id];
    NR_UL_gNB_HARQ_t *harq_process = ulsch->harq_process;
    const nfapi_nr_pusch_pdu_t *pusch_pdu = &harq_process->ulsch_pdu;
    uint8_t harq_pid = ulsch->harq_pid;

    nrLDPC_TB_decoding_parameters_t *TB_parameters = &TBs[pusch_id];

    if (!harq_process) {
      LOG_E(PHY, "ulsch_decoding.c: NULL harq_process pointer\n");
      return -1;
    }

    uint8_t number_dmrs_symbols = count_bits64_with_mask(pusch_pdu->ul_dmrs_symb_pos, pusch_pdu->start_symbol_index, pusch_pdu->nr_of_symbols);
    int factor = pusch_pdu->dmrs_config_type == pusch_dmrs_type1 ? 6 : 4;
    int nb_re_dmrs = factor * pusch_pdu->num_dmrs_cdm_grps_no_data;
    uint32_t G = nr_get_G(pusch_pdu->rb_size,
                          pusch_pdu->nr_of_symbols,
                          nb_re_dmrs,
                          number_dmrs_symbols, // number of dmrs symbols irrespective of single or double symbol dmrs
                          ulsch->unav_res,
                          pusch_pdu->qam_mod_order,
                          pusch_pdu->nrOfLayers);
    DevAssert(G > 0);
    TB_parameters->G = G;


    // The harq_pid is not unique among the active HARQ processes in the instance so we use ULSCH_id instead
    TB_parameters->harq_unique_pid = (phy_vars_gNB->max_nb_pusch * harq_pid) + ULSCH_id;
    AssertFatal(TB_parameters->harq_unique_pid < 0xffffffff,"harq_unique_pid >= %d, harq_pid %d, ULSCH_id %d\n",16*phy_vars_gNB->max_nb_pusch,harq_pid,ULSCH_id);

    // ------------------------------------------------------------------
    TB_parameters->nb_rb = pusch_pdu->rb_size;
    TB_parameters->Qm = pusch_pdu->qam_mod_order;
    TB_parameters->mcs = pusch_pdu->mcs_index;
    TB_parameters->nb_layers = pusch_pdu->nrOfLayers;
    // ------------------------------------------------------------------

    TB_parameters->processedSegments = &harq_process->processedSegments;

    TB_parameters->BG = pusch_pdu->maintenance_parms_v3.ldpcBaseGraph;
    TB_parameters->A = pusch_pdu->pusch_data.tb_size << 3;
    NR_gNB_PHY_STATS_t *stats = get_phy_stats(phy_vars_gNB, ulsch->rnti);
    if (stats) {
      stats->frame = frame;
      stats->ulsch_stats.round_trials[harq_process->round]++;
      for (int aarx = 0; aarx < frame_parms->nb_antennas_rx; aarx++) {
        stats->ulsch_stats.power[aarx] = dB_fixed_x10(pusch->ulsch_power[aarx]);
        stats->ulsch_stats.noise_power[aarx] = dB_fixed_x10(pusch->ulsch_noise_power[aarx]);
      }
      if (!harq_process->harq_to_be_cleared) {
        stats->ulsch_stats.current_Qm = TB_parameters->Qm;
        stats->ulsch_stats.current_RI = TB_parameters->nb_layers;
        stats->ulsch_stats.total_bytes_tx += pusch_pdu->pusch_data.tb_size;
      }
    }

    LOG_D(PHY,
          "ULSCH Decoding, harq_pid %d rnti %x TBS %d G %d mcs %d Nl %d nb_rb %d, Qm %d, Coderate %f RV %d round %d new RX %d\n",
          harq_pid,
          ulsch->rnti,
          TB_parameters->A,
          TB_parameters->G,
          TB_parameters->mcs,
          TB_parameters->nb_layers,
          TB_parameters->nb_rb,
          TB_parameters->Qm,
          pusch_pdu->target_code_rate / 10240.0f,
          pusch_pdu->pusch_data.rv_index,
          harq_process->round,
          harq_process->harq_to_be_cleared);

    // [hna] Perform nr_segmenation with input and output set to NULL to calculate only (C, K, Z, F)
    nr_segmentation(NULL,
                    NULL,
                    lenWithCrc(1, TB_parameters->A), // size in case of 1 segment
                    &TB_parameters->C,
                    &TB_parameters->K,
                    &TB_parameters->Z, // [hna] Z is Zc
                    &TB_parameters->F,
                    TB_parameters->BG);
    harq_process->C = TB_parameters->C;
    harq_process->K = TB_parameters->K;
    harq_process->Z = TB_parameters->Z;
    harq_process->F = TB_parameters->F;

    uint16_t a_segments = MAX_NUM_NR_ULSCH_SEGMENTS_PER_LAYER * TB_parameters->nb_layers; // number of segments to be allocated
    if (TB_parameters->C > a_segments) {
      LOG_E(PHY, "nr_segmentation.c: too many segments %d, A %d\n", harq_process->C, TB_parameters->A);
      return (-1);
    }
    if (TB_parameters->nb_rb != 273) {
      a_segments = a_segments * TB_parameters->nb_rb;
      a_segments = a_segments / 273 + 1;
    }
    if (TB_parameters->C > a_segments) {
      LOG_E(PHY, "Illegal harq_process->C %d > %d\n", harq_process->C, a_segments);
      return -1;
    }
    max_num_segments = max(max_num_segments, TB_parameters->C);

#ifdef DEBUG_ULSCH_DECODING
    printf("ulsch decoding nr segmentation Z %d\n", TB_parameters->Z);
    if (!frame % 100)
      printf("K %d C %d Z %d \n", TB_parameters->K, TB_parameters->C, TB_parameters->Z);
    printf("Segmentation: C %d, K %d\n", TB_parameters->C, TB_parameters->K);
#endif

    TB_parameters->max_ldpc_iterations = ulsch->max_ldpc_iterations;
    TB_parameters->rv_index = pusch_pdu->pusch_data.rv_index;
    TB_parameters->tbslbrm = pusch_pdu->maintenance_parms_v3.tbSizeLbrmBytes;
    TB_parameters->abort_decode = &harq_process->abort_decode;
    set_abort(&harq_process->abort_decode, false);
  }

  for (uint8_t pusch_id = 0; pusch_id < nb_pusch; pusch_id++) {
    uint8_t ULSCH_id = ULSCH_ids[pusch_id];
    NR_gNB_ULSCH_t *ulsch = &phy_vars_gNB->ulsch[ULSCH_id];
    NR_UL_gNB_HARQ_t *harq_process = ulsch->harq_process;
    short *ulsch_llr = phy_vars_gNB->pusch_vars[ULSCH_id].llr;

    if (!ulsch_llr) {
      LOG_E(PHY, "ulsch_decoding.c: NULL ulsch_llr pointer\n");
      return -1;
    }

    nrLDPC_TB_decoding_parameters_t *TB_parameters = &TBs[pusch_id];

    TB_parameters->llr = ulsch_llr;
    TB_parameters->d = harq_process->d;
    TB_parameters->c = harq_process->c;
    TB_parameters->E = nr_get_E(TB_parameters->G, TB_parameters->C, TB_parameters->Qm, TB_parameters->nb_layers, 0);
    TB_parameters->first_rE2 = TB_parameters->C;
    TB_parameters->E2 = TB_parameters->E;
    TB_parameters->R = nr_get_R_ldpc_decoder(TB_parameters->rv_index,
                                             TB_parameters->E,
                                             TB_parameters->BG,
                                             TB_parameters->Z,
                                             &harq_process->llrLen,
                                             harq_process->round);
    for (int r = 0; r < TB_parameters->C; r++)
      TB_parameters->decodeSuccess[r] = false;
    TB_parameters->d_to_be_cleared = harq_process->harq_to_be_cleared;
    reset_meas(&TB_parameters->ts_deinterleave);
    reset_meas(&TB_parameters->ts_rate_unmatch);
    reset_meas(&TB_parameters->ts_seg_prep);
    reset_meas(&TB_parameters->ts_ldpc_decode);
    for (int r = 0; r < TB_parameters->C; r++) {
      int Etmp = nr_get_E(TB_parameters->G, TB_parameters->C, TB_parameters->Qm, TB_parameters->nb_layers, r);
      if (TB_parameters->E != Etmp) {
        TB_parameters->E2 = Etmp;
        TB_parameters->R2 = nr_get_R_ldpc_decoder(TB_parameters->rv_index,
                                                  TB_parameters->E2,
                                                  TB_parameters->BG,
                                                  TB_parameters->Z,
                                                  &harq_process->llrLen,
                                                  harq_process->round);
        TB_parameters->first_rE2 = r;
        break;
      }
    }
  }

  int ret_decoder = phy_vars_gNB->nrLDPC_coding_interface.nrLDPC_coding_decoder(&slot_parameters);
  // post decode
  for (uint8_t pusch_id = 0; pusch_id < nb_pusch; pusch_id++) {
    uint8_t ULSCH_id = ULSCH_ids[pusch_id];
    NR_gNB_ULSCH_t *ulsch = &phy_vars_gNB->ulsch[ULSCH_id];
    NR_UL_gNB_HARQ_t *harq_process = ulsch->harq_process;

    nrLDPC_TB_decoding_parameters_t *TB_parameters = &TBs[pusch_id];

    uint32_t offset = 0, r_offset = 0;
    bool crcok = true;
    LOG_D(PHY, "C = %d\n", TB_parameters->C);
    for (int r = 0; r < TB_parameters->C; r++) {
      LOG_D(PHY, "Segment %d %d\n", r, TB_parameters->decodeSuccess[r]);
      if (TB_parameters->decodeSuccess[r] == false) {
        LOG_D(PHY, "Segment %d/%d in error\n", r, TB_parameters->C);
        crcok = false;
        break;
      }
    }
    if (crcok) {
      for (int r = 0; r < TB_parameters->C; r++) {
        // Copy c to b in case of decoding success
        memcpy(harq_process->b + offset,
               harq_process->c + r_offset,
               (harq_process->K >> 3) - (harq_process->F >> 3) - ((harq_process->C > 1) ? 3 : 0));
        offset += ((harq_process->K >> 3) - (harq_process->F >> 3) - ((harq_process->C > 1) ? 3 : 0));
        r_offset += (harq_process->K >> 3);
      }
    } else {
      LOG_D(PHY, "ULSCH %d in error\n", ULSCH_id);
    }
    merge_meas(&phy_vars_gNB->ts_deinterleave, &TB_parameters->ts_deinterleave);
    merge_meas(&phy_vars_gNB->ts_rate_unmatch, &TB_parameters->ts_rate_unmatch);
    merge_meas(&phy_vars_gNB->ts_seg_prep, &TB_parameters->ts_seg_prep);
    merge_meas(&phy_vars_gNB->ts_ldpc_decode, &TB_parameters->ts_ldpc_decode);
    harq_process->harq_to_be_cleared = false;
  }

  return ret_decoder;
}
