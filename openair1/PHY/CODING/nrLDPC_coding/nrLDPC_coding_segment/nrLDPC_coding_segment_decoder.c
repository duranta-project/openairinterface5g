/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*! \file PHY/CODING/nrLDPC_coding/nrLDPC_coding_segment/nrLDPC_coding_segment_decoder.c
 * \brief Top-level routines for decoding LDPC transport channels
 */

// [from gNB coding]
#include "nr_rate_matching.h"
#include "PHY/CODING/coding_extern.h"
#include "PHY/CODING/coding_defs.h"
#include "PHY/CODING/nrLDPC_coding/nrLDPC_coding_interface.h"
#include "PHY/CODING/nrLDPC_extern.h"
#include "defs.h"
#include "common/utils/LOG/log.h"

#include <stdalign.h>
#include <stdint.h>
#include <syscall.h>
#include <time.h>
// #define gNB_DEBUG_TRACE

#define OAI_LDPC_DECODER_MAX_NUM_LLR 27000 // 26112 // NR_LDPC_NCOL_BG1*NR_LDPC_ZMAX = 68*384
// #define DEBUG_CRC
#ifdef DEBUG_CRC
#define PRINT_CRC_CHECK(a) a
#else
#define PRINT_CRC_CHECK(a)
#endif

#include "nfapi/open-nFAPI/nfapi/public_inc/nfapi_interface.h"
#include "nfapi/open-nFAPI/nfapi/public_inc/nfapi_nr_interface.h"

/**
 * \typedef nrLDPC_decoding_parameters_t
 * \struct nrLDPC_decoding_parameters_s
 * \brief decoding parameter of transport blocks
 * \var decoderParms decoder parameters
 * \var Qm modulation order
 * \var Kc ratio between the number of columns in the parity check matrix and the lifting size
 * it is fixed for a given base graph while the lifting size is chosen to have a sufficient number of columns
 * \var rv_index
 * \var max_number_iterations maximum number of LDPC iterations
 * \var abort_decode pointer to decode abort flag
 * \var tbslbrm transport block size LBRM in bytes
 * \var A Transport block size (This is A from 38.212 V15.4.0 section 5.1)
 * \var K Code block size at decoder output
 * \var Z lifting size
 * \var F filler bits size
 * \var r segment index in TB
 * \var E input llr segment size
 * \var C number of segments
 * \var llr input llr segment array
 * \var d Pointers to code blocks before LDPC decoding (38.212 V15.4.0 section 5.3.2)
 * \var d_to_be_cleared
 * pointer to the flag used to clear d properly
 * when true, clear d after rate dematching
 * \var c Pointers to code blocks after LDPC decoding (38.212 V15.4.0 section 5.2.2)
 * \var decodeSuccess pointer to the flag indicating that the decoding of the segment was successful
 * \var ans pointer to task answer used by the thread pool to detect task completion
 * \var p_ts_ldpc_decode pointer to decoding time stats
 */
typedef struct nrLDPC_decoding_parameters_s {
  t_nrLDPC_dec_params decoderParms;

  int r;
  uint8_t Qm;

  uint8_t Kc;
  uint8_t rv_index;
  decode_abort_t *abort_decode;

  uint32_t tbslbrm;
  uint32_t A;
  uint32_t K;
  uint32_t Z;
  uint32_t F;

  uint32_t C;

  int E;
  short *llr;
  int16_t *d;
  bool d_to_be_cleared;
  uint8_t *c;
  bool *decodeSuccess;

  task_ans_t *ans;

  time_stats_t ts_ldpc_decode;
} nrLDPC_decoding_parameters_t;

static void nr_process_decode_segment(void *arg)
{
  nrLDPC_decoding_parameters_t *rdata = (nrLDPC_decoding_parameters_t *)arg;
  t_nrLDPC_dec_params *p_decoderParms = &rdata->decoderParms;
  const int K = rdata->K;
  const int Kprime = K - rdata->F;
  const int A = rdata->A;
  const int E = rdata->E;
  const int Qm = rdata->Qm;
  const int rv_index = rdata->rv_index;
  const uint8_t Kc = rdata->Kc;
  short *ulsch_llr = rdata->llr;
  int8_t llrProcBuf[OAI_LDPC_DECODER_MAX_NUM_LLR] __attribute__((aligned(32)));

  t_nrLDPC_time_stats procTime = {0};
  t_nrLDPC_time_stats *p_procTime = &procTime;

  ////////////////////////////////////////////////////////////////////////////////////////////
  ///////////////////////////////// nr_deinterleaving_ldpc ///////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////////////////

  //////////////////////////// ulsch_llr =====> ulsch_harq->e //////////////////////////////

  /// code blocks after bit selection in rate matching for LDPC code (38.212 V15.4.0 section 5.4.2.1)
  int16_t harq_e[E];

  nr_deinterleaving_ldpc(E, Qm, harq_e, ulsch_llr);

  //////////////////////////////////////////////////////////////////////////////////////////
  //////////////////////////////// nr_rate_matching_ldpc_rx ////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////////////////

  ///////////////////////// ulsch_harq->e =====> ulsch_harq->d /////////////////////////

  if (nr_rate_matching_ldpc_rx(rdata->tbslbrm,
                               p_decoderParms->BG,
                               p_decoderParms->Z,
                               rdata->d,
                               harq_e,
                               rdata->C,
                               rv_index,
                               rdata->d_to_be_cleared,
                               E,
                               rdata->F,
                               K - rdata->F - 2 * (p_decoderParms->Z))
      == -1) {
    LOG_E(PHY,
          "nrLDPC_coding_segment_decoder.c: Problem in rate_matching BG %d, Z %d, C %d, rv_index %d, E %d, F %d, K%d, K-F-2*Z %d\n",
          p_decoderParms->BG,
          p_decoderParms->Z,
          rdata->C,
          rv_index,
          E,
          rdata->F,
          K,
          K - rdata->F - 2 * (p_decoderParms->Z));

    // Task completed
    completed_task_ans(rdata->ans);
    return;
  }

  p_decoderParms->crc_type = crcType(rdata->C, A);
  p_decoderParms->Kprime = lenWithCrc(rdata->C, A);

  // set first 2*Z_c bits to zeros

  int16_t z[68 * 384 + 16] __attribute__((aligned(16)));


  memset(z, 0, 2 * rdata->Z * sizeof(*z));
  // set Filler bits
  memset(z + Kprime, 127, rdata->F * sizeof(*z));
  // Move coded bits before filler bits
  memcpy(z + 2 * rdata->Z, rdata->d, (Kprime - 2 * rdata->Z) * sizeof(*z));
  // skip filler bits
  memcpy(z + K, rdata->d + (K - 2 * rdata->Z), (Kc * rdata->Z - K) * sizeof(*z));
  // Saturate coded bits before decoding into 8 bits values
  simde__m128i *pv = (simde__m128i *)&z;
  int8_t l[68 * 384 + 16] __attribute__((aligned(16)));
  simde__m128i *pl = (simde__m128i *)&l;
  for (int i = 0, j = 0; j < ((Kc * rdata->Z) >> 4) + 1; i += 2, j++) {
    pl[j] = simde_mm_packs_epi16(pv[i], pv[i + 1]);
  }
  //////////////////////////////////////////////////////////////////////////////////////////

  //////////////////////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////// nrLDPC_decoder /////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////////////////

  ////////////////////////////////// pl =====> llrProcBuf //////////////////////////////////
  start_meas(&rdata->ts_ldpc_decode);
  int decodeIterations = LDPCdecoder(p_decoderParms, l, (uint8_t *)llrProcBuf, p_procTime, rdata->abort_decode);
  AssertFatal(rdata->c, "rdata->c is null, A %d, K %d\n", rdata->A, rdata->K);
  if (decodeIterations < p_decoderParms->numMaxIter) {
    memcpy(rdata->c, llrProcBuf, K >> 3);
    *rdata->decodeSuccess = true;
  } else {
    LOG_D(PHY, "Decoding failed: K %d, Z %d, rv_index %d\n", K, rdata->Z, rdata->rv_index);
    memset(rdata->c, 0, K >> 3);
    *rdata->decodeSuccess = false;
  }
  stop_meas(&rdata->ts_ldpc_decode);

  // Task completed
  completed_task_ans(rdata->ans);
}

int nrLDPC_prepare_TB_decoding(nrLDPC_slot_decoding_parameters_t *nrLDPC_slot_decoding_parameters,
                               int pusch_id,
                               thread_info_tm_t *t_info)
{
  nrLDPC_TB_decoding_parameters_t *nrLDPC_TB_decoding_parameters = &nrLDPC_slot_decoding_parameters->TBs[pusch_id];

  *nrLDPC_TB_decoding_parameters->processedSegments = 0;
  t_nrLDPC_dec_params decParams = {.check_crc = check_crc};
  decParams.BG = nrLDPC_TB_decoding_parameters->BG;
  decParams.Z = nrLDPC_TB_decoding_parameters->Z;
  decParams.numMaxIter = nrLDPC_TB_decoding_parameters->max_ldpc_iterations;
  decParams.outMode = nrLDPC_outMode_BIT;

  for (int r = 0; r < nrLDPC_TB_decoding_parameters->C; r++) {
    nrLDPC_decoding_parameters_t *rdata = &((nrLDPC_decoding_parameters_t *)t_info->buf)[t_info->len];
    DevAssert(t_info->len < t_info->cap);
    rdata->ans = t_info->ans;
    t_info->len += 1;
    int llr_offset;
    if (r < nrLDPC_TB_decoding_parameters->first_rE2) {
      decParams.R = nrLDPC_TB_decoding_parameters->R;
      rdata->E = nrLDPC_TB_decoding_parameters->E;
      llr_offset = r * rdata->E;
    } else {
      decParams.R = nrLDPC_TB_decoding_parameters->R2;
      rdata->E = nrLDPC_TB_decoding_parameters->E2;
      llr_offset = nrLDPC_TB_decoding_parameters->first_rE2 * nrLDPC_TB_decoding_parameters->E
                   + (r - nrLDPC_TB_decoding_parameters->first_rE2) * rdata->E;
    }
    rdata->r = r;
    rdata->decoderParms = decParams;
    rdata->Kc = decParams.BG == 2 ? 52 : 68;
    rdata->C = nrLDPC_TB_decoding_parameters->C;
    rdata->A = nrLDPC_TB_decoding_parameters->A;
    rdata->Qm = nrLDPC_TB_decoding_parameters->Qm;
    rdata->K = nrLDPC_TB_decoding_parameters->K;
    rdata->Z = nrLDPC_TB_decoding_parameters->Z;
    rdata->F = nrLDPC_TB_decoding_parameters->F;
    rdata->rv_index = nrLDPC_TB_decoding_parameters->rv_index;
    rdata->tbslbrm = nrLDPC_TB_decoding_parameters->tbslbrm;
    rdata->abort_decode = nrLDPC_TB_decoding_parameters->abort_decode;
    rdata->d = nrLDPC_TB_decoding_parameters->d + r * rdata->Kc * rdata->Z;
    rdata->d_to_be_cleared = nrLDPC_TB_decoding_parameters->d_to_be_cleared;
    rdata->c = nrLDPC_TB_decoding_parameters->c + r * (rdata->K >> 3);
    AssertFatal(rdata->c != NULL,
                "rdata->c is null, r %d, K %d, A %d, rv_index %d, TB_decoding_parameters->c %p\n",
                r,
                rdata->K,
                rdata->A,
                rdata->rv_index,
                nrLDPC_TB_decoding_parameters->c);
    rdata->llr = nrLDPC_TB_decoding_parameters->llr + llr_offset; // rdata->Kc*rdata->Z;
    rdata->decodeSuccess = &nrLDPC_TB_decoding_parameters->decodeSuccess[r];
    memset(&rdata->ts_ldpc_decode, 0, sizeof(rdata->ts_ldpc_decode));
    reset_meas(&rdata->ts_ldpc_decode);
    task_t t = {.func = &nr_process_decode_segment, .args = rdata};
    pushTpool(nrLDPC_slot_decoding_parameters->threadPool, t);

    LOG_D(PHY, "Added a block to decode, in pipe: %d, rdata->c %p\n", r, rdata->c);
  }
  return nrLDPC_TB_decoding_parameters->C;
}

int32_t nrLDPC_coding_init(int max_num_pxsch)
{
  return 0;
}

int32_t nrLDPC_coding_shutdown(void)
{
  return 0;
}

int32_t nrLDPC_coding_decoder(nrLDPC_slot_decoding_parameters_t *nrLDPC_slot_decoding_parameters)
{
  int nbSegments = 0;
  for (int pusch_id = 0; pusch_id < nrLDPC_slot_decoding_parameters->nb_TBs; pusch_id++) {
    nrLDPC_TB_decoding_parameters_t *nrLDPC_TB_decoding_parameters = &nrLDPC_slot_decoding_parameters->TBs[pusch_id];
    nbSegments += nrLDPC_TB_decoding_parameters->C;
  }
  nrLDPC_decoding_parameters_t arr[nbSegments];
  task_ans_t ans;
  init_task_ans(&ans, nbSegments);
  thread_info_tm_t t_info = {.buf = (uint8_t *)arr, .len = 0, .cap = nbSegments, .ans = &ans};

  for (int pusch_id = 0; pusch_id < nrLDPC_slot_decoding_parameters->nb_TBs; pusch_id++) {
    (void)nrLDPC_prepare_TB_decoding(nrLDPC_slot_decoding_parameters, pusch_id, &t_info);
  }

  // Execute thread pool tasks
  join_task_ans(t_info.ans);

  size_t r_t_info = 0;
  for (int pusch_id = 0; pusch_id < nrLDPC_slot_decoding_parameters->nb_TBs; pusch_id++) {
    nrLDPC_TB_decoding_parameters_t *nrLDPC_TB_decoding_parameters = &nrLDPC_slot_decoding_parameters->TBs[pusch_id];
    *nrLDPC_TB_decoding_parameters->processedSegments = 0;
    for (int r = 0; r < nrLDPC_TB_decoding_parameters->C; r++) {
      if (nrLDPC_TB_decoding_parameters->decodeSuccess[r])
        *nrLDPC_TB_decoding_parameters->processedSegments = *nrLDPC_TB_decoding_parameters->processedSegments + 1;

      nrLDPC_decoding_parameters_t *rdata = &((nrLDPC_decoding_parameters_t *)t_info.buf)[r_t_info];
      r_t_info += 1;
      merge_meas(&nrLDPC_TB_decoding_parameters->ts_ldpc_decode, &rdata->ts_ldpc_decode);
    }
  }
  return 0;
}
