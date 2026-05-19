/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.0  (the "License"); you may not use this file
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

/*! \file PHY/NR_TRANSPORT/nr_slsch_decoding.c
* \brief Top-level routines for decoding  LDPC (ULSCH) transport channels from 38.212, V15.4.0 2018-12
* \author Ahmed Hussein
* \date 2019
* \version 0.1
* \company Fraunhofer IIS
* \email: ahmed.hussein@iis.fraunhofer.de
* \note
* \warning
*/


#include "PHY/defs_nr_UE.h"
// [from gNB coding]
#include "PHY/defs_gNB.h"
#include "PHY/CODING/coding_extern.h"
#include "PHY/CODING/coding_defs.h"
#include "PHY/CODING/lte_interleaver_inline.h"
#include "PHY/CODING/nrLDPC_extern.h"
#include "PHY/NR_TRANSPORT/nr_ulsch.h"
#include "openair1/SCHED_NR_UE/defs.h"
#include "common/utils/LOG/vcd_signal_dumper.h"
#include "common/utils/LOG/log.h"
#include <syscall.h>
#include "executables/nr-uesoftmodem.h"
#include "PHY/sse_intrin.h"

//#define DEBUG_ULSCH_DECODING
//#define gNB_DEBUG_TRACE

#define OAI_UL_LDPC_MAX_NUM_LLR 27000//26112 // NR_LDPC_NCOL_BG1*NR_LDPC_ZMAX = 68*384
//#define DEBUG_CRC
#ifdef DEBUG_CRC
#define PRINT_CRC_CHECK(a) a
#else
#define PRINT_CRC_CHECK(a)
#endif

//extern double cpuf;
/*
void free_gNB_ulsch(NR_gNB_ULSCH_t *ulsch, uint16_t N_RB_UL)
{

  uint16_t a_segments = MAX_NUM_NR_ULSCH_SEGMENTS_PER_LAYER*NR_MAX_NB_LAYERS;  //number of segments to be allocated

  if (N_RB_UL != 273) {
    a_segments = a_segments*N_RB_UL;
    a_segments = a_segments/273 +1;
  }

  if (ulsch->harq_process) {
    if (ulsch->harq_process->b) {
      free_and_zero(ulsch->harq_process->b);
      ulsch->harq_process->b = NULL;
    }
    for (int r = 0; r < a_segments; r++) {
      free_and_zero(ulsch->harq_process->c[r]);
      free_and_zero(ulsch->harq_process->d[r]);
    }
    free_and_zero(ulsch->harq_process->c);
    free_and_zero(ulsch->harq_process->d);
    free_and_zero(ulsch->harq_process->d_to_be_cleared);
    free_and_zero(ulsch->harq_process);
    ulsch->harq_process = NULL;
  }
}

NR_gNB_ULSCH_t new_gNB_ulsch(uint8_t max_ldpc_iterations, uint16_t N_RB_UL)
{

  uint16_t a_segments = MAX_NUM_NR_ULSCH_SEGMENTS_PER_LAYER*NR_MAX_NB_LAYERS;  //number of segments to be allocated

  if (N_RB_UL != 273) {
    a_segments = a_segments*N_RB_UL;
    a_segments = a_segments/273 +1;
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
  harq->c = malloc16_clear(a_segments * sizeof(*harq->c));
  harq->d = malloc16_clear(a_segments * sizeof(*harq->d));
  for (int r = 0; r < a_segments; r++) {
    harq->c[r] = malloc16_clear(8448 * sizeof(*harq->c[r]));
    harq->d[r] = malloc16_clear(68 * 384 * sizeof(*harq->d[r]));
  }
  harq->d_to_be_cleared = calloc(a_segments, sizeof(bool));
  AssertFatal(harq->d_to_be_cleared != NULL, "out of memory\n");
  return(ulsch);
}
*/
int nr_slsch_decoding(struct PHY_VARS_NR_UE_s *UE,
                      uint8_t SLSCH_id,
                      short *slsch_llr,
                      NR_DL_FRAME_PARMS *frame_parms,
                      nfapi_nr_pusch_pdu_t *pssch_pdu,
                      uint32_t frame,
                      uint8_t nr_tti_rx,
                      uint8_t harq_pid,
                      uint32_t G,
                      const UE_nr_rxtx_proc_t *proc,
                      nr_phy_data_t *phy_data,
                      int8_t *ack_nack_rcvd,
                      uint8_t num_acks)
{
  nrLDPC_TB_decoding_parameters_t TB;
  memset(&TB, 0, sizeof(TB));
  nrLDPC_slot_decoding_parameters_t slot_parameters = {.frame = frame,
                                                       .slot = nr_tti_rx,
                                                       .nb_TBs = 1,
                                                       .threadPool = &get_nrUE_params()->Tpool,
                                                       .TBs = &TB};

  int max_num_segments = 0;

  NR_gNB_ULSCH_t *slsch = &UE->slsch[SLSCH_id];
  NR_UL_gNB_HARQ_t *harq_process = slsch->harq_process;

  if (!harq_process) {
    LOG_E(PHY, "slsch_decoding.c: NULL harq_process pointer\n");
    return -1;
  }

  TB.G = G;

  // The harq_pid is not unique among the active HARQ processes in the instance so we use ULSCH_id instead
  TB.harq_unique_pid = SLSCH_id;

  // ------------------------------------------------------------------
  TB.nb_rb = pssch_pdu->rb_size;
  TB.Qm = pssch_pdu->qam_mod_order;
  TB.mcs = pssch_pdu->mcs_index;
  TB.nb_layers = pssch_pdu->nrOfLayers;
  // ------------------------------------------------------------------

  TB.processedSegments = &harq_process->processedSegments;
  int TBS = pssch_pdu->pusch_data.tb_size;

  TB.BG = pssch_pdu->maintenance_parms_v3.ldpcBaseGraph;
  TB.A = TBS << 3;
  /*
  NR_gNB_PHY_STATS_t *stats = UE->slsch_stats;
  if (stats) {
    stats->frame = frame;
    stats->ulsch_stats.round_trials[harq_process->round]++;
    for (int aarx = 0; aarx < frame_parms->nb_antennas_rx; aarx++) {
      stats->ulsch_stats.power[aarx] = dB_fixed_x10(pusch->ulsch_power[aarx]);
      stats->ulsch_stats.noise_power[aarx] = dB_fixed_x10(pusch->ulsch_noise_power[aarx]);
    }
    if (!harq_process->harq_to_be_cleared) {
      stats->ulsch_stats.current_Qm = TB.Qm;
      stats->ulsch_stats.current_RI = TB.nb_layers;
      stats->ulsch_stats.total_bytes_tx += TBS;
    }
  }
*/
  LOG_D(PHY,
        "SLSCH Decoding, harq_pid %d rnti %x TBS %d G %d mcs %d Nl %d nb_rb %d, Qm %d, Coderate %f RV %d round %d new RX %d\n",
        harq_pid,
        slsch->rnti,
        TB.A,
        TB.G,
        TB.mcs,
        TB.nb_layers,
        TB.nb_rb,
        TB.Qm,
        pssch_pdu->target_code_rate / 10240.0f,
        pssch_pdu->pusch_data.rv_index,
        harq_process->round,
        harq_process->harq_to_be_cleared);

    // [hna] Perform nr_segmenation with input and output set to NULL to calculate only (C, K, Z, F)
  nr_segmentation(NULL,
                  NULL,
                  lenWithCrc(1, TB.A), // size in case of 1 segment
                  &TB.C,
                  &TB.K,
                  &TB.Z, // [hna] Z is Zc
                  &TB.F,
                  TB.BG);
  harq_process->C = TB.C;
  harq_process->K = TB.K;
  harq_process->Z = TB.Z;
  harq_process->F = TB.F;

  uint16_t a_segments = MAX_NUM_NR_ULSCH_SEGMENTS_PER_LAYER * TB.nb_layers; // number of segments to be allocated
  if (TB.C > a_segments) {
    LOG_E(PHY, "nr_segmentation.c: too many segments %d, A %d\n", harq_process->C, TB.A);
    return (-1);
  }
  if (TB.nb_rb != 273) {
    a_segments = a_segments * TB.nb_rb;
    a_segments = a_segments / 273 + 1;
  }
  if (TB.C > a_segments) {
    LOG_E(PHY, "Illegal harq_process->C %d > %d\n", harq_process->C, a_segments);
    return -1;
  }
  max_num_segments = max(max_num_segments, TB.C);

#ifdef DEBUG_ULSCH_DECODING
  printf("slsch decoding nr segmentation Z %d\n", TB.Z);
  if (!frame % 100)
    printf("K %d C %d Z %d \n", TB.K, TB.C, TB.Z);
  printf("Segmentation: C %d, K %d\n", TB.C, TB.K);
#endif

  TB.max_ldpc_iterations = slsch->max_ldpc_iterations;
  TB.rv_index = pssch_pdu->pusch_data.rv_index;
  TB.tbslbrm = pssch_pdu->maintenance_parms_v3.tbSizeLbrmBytes;
  TB.abort_decode = &harq_process->abort_decode;
  set_abort(&harq_process->abort_decode, false);

  nrLDPC_segment_decoding_parameters_t segments[max_num_segments];
  memset(segments, 0, sizeof(segments));

  TB.segments = segments;

  uint32_t r_offset = 0;
  for (int r = 0; r < TB.C; r++) {
    nrLDPC_segment_decoding_parameters_t *segment_parameters = &TB.segments[r];
    segment_parameters->E = nr_get_E(TB.G, TB.C, TB.Qm, TB.nb_layers, r);
    segment_parameters->R = nr_get_R_ldpc_decoder(TB.rv_index,
                                                  segment_parameters->E,
                                                  TB.BG,
                                                  TB.Z,
                                                  &harq_process->llrLen,
                                                  harq_process->round);
    segment_parameters->llr = slsch_llr + r_offset;
    segment_parameters->d = harq_process->d[r];
    segment_parameters->d_to_be_cleared = &harq_process->d_to_be_cleared[r];
    segment_parameters->c = harq_process->c[r];
    segment_parameters->decodeSuccess = false;

    reset_meas(&segment_parameters->ts_deinterleave);
    reset_meas(&segment_parameters->ts_rate_unmatch);
    reset_meas(&segment_parameters->ts_ldpc_decode);

    r_offset += segment_parameters->E;
  }
  if (harq_process->harq_to_be_cleared) {
    for (int r = 0; r < TB.C; r++) {
      harq_process->d_to_be_cleared[r] = true;
    }
    harq_process->harq_to_be_cleared = false;
  }

  int ret_decoder = UE->nrLDPC_coding_interface.nrLDPC_coding_decoder(&slot_parameters);

  // post decode


  uint32_t offset = 0;
  for (int r = 0; r < TB.C; r++) {
    nrLDPC_segment_decoding_parameters_t nrLDPC_segment_decoding_parameters = TB.segments[r];
    // Copy c to b in case of decoding success
    if (nrLDPC_segment_decoding_parameters.decodeSuccess) {
      memcpy(harq_process->b + offset,
             harq_process->c[r],
             (harq_process->K >> 3) - (harq_process->F >> 3) - ((harq_process->C > 1) ? 3 : 0));
    } else {
      LOG_D(PHY, "sidelink segment error %d/%d\n", r, harq_process->C);
      LOG_D(PHY, "SLSCH %d in error\n", SLSCH_id);
    }
    offset += ((harq_process->K >> 3) - (harq_process->F >> 3) - ((harq_process->C > 1) ? 3 : 0));

  }

  return ret_decoder;
}
