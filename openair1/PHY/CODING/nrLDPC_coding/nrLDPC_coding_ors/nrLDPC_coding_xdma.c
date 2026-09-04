/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*! \file PHY/CODING/nrLDPC_coding/nrLDPC_coding_xdma/nrLDPC_coding_xdma.c
 * \brief Top-level routines for decoding LDPC (ULSCH) transport channels
 * decoding implemented using a FEC IP core on FPGA through XDMA driver
 */

// [from gNB coding]
#include <syscall.h>

#include <nr_rate_matching.h>
#include "PHY/CODING/coding_defs.h"
#include "PHY/CODING/coding_extern.h"
#include "PHY/CODING/nrLDPC_coding/nrLDPC_coding_ors/nrLDPC_coding_xdma_offload.h"
#include "PHY/CODING/nrLDPC_extern.h"
#include "common/utils/LOG/log.h"
#include "defs.h"
// #define DEBUG_ULSCH_DECODING
// #define gNB_DEBUG_TRACE

#define OAI_UL_LDPC_MAX_NUM_LLR (27000U) // 26112 // NR_LDPC_NCOL_BG1*NR_LDPC_ZMAX = 68*384
#define MAX_CB_SIZE_IN_BYTE_UNITS (1100U) // 8488/8 -> 1056
#define NUMB_OF_MAX_DEC_ITER (63U)
#define NUMB_OF_MIN_DEC_ITER (1U)
// #define DEBUG_CRC
#ifdef DEBUG_CRC
#define PRINT_CRC_CHECK(a) a
#else
#define PRINT_CRC_CHECK(a)
#endif
#define USE_OUTPUT_PARALLELIZATION (false)

#include "nfapi/open-nFAPI/nfapi/public_inc/nfapi_interface.h"
#include "nfapi/open-nFAPI/nfapi/public_inc/nfapi_nr_interface.h"

#include "PHY/CODING/nrLDPC_coding/nrLDPC_coding_interface.h"

#include "xdma_diag.h"

/*!
 * \typedef args_fpga_decode_prepare_t
 * \struct args_fpga_decode_prepare_s
 * \brief arguments structure for passing arguments to the nr_ulsch_FPGA_decoding_prepare_blocks function
 */
typedef struct args_fpga_decode_prepare_s {
  nrLDPC_TB_decoding_parameters_t *TB_params; /*!< transport blocks parameters */

  uint8_t *multi_indata; /*!< pointer to the head of the block destination array that is then passed to the FPGA decoding */
  uint32_t r_first; /*!< index of the first block to be prepared within this function */
  uint32_t r_span; /*!< number of blocks to be prepared within this function */
  int r_offset; /*!< r index expressed in bits */
  int input_CBoffset; /*!< This offset describes where the CB in the FPGA input buffer */
  int Kc; /*!< ratio between the number of columns in the parity check graph and the lifting size */
  int Kprime; /*!< size of payload and CRC bits in a code block */
  int Kb; /*!< Definition in TS 138 212 5.2.2 */
  task_ans_t *ans; /*!< pointer to the answer that is used by thread pool to detect job completion */
} args_fpga_decode_prepare_t;

typedef struct args_fpga_post_decode_s {
  nrLDPC_TB_decoding_parameters_t *TB_params; /*!< transport blocks parameters */

  uint32_t r_first; /*!< index of the first block to be prepared within this function */
  uint32_t r_span; /*!< number of blocks to be prepared within this function */
  int output_CBoffset; /*!< Offset in the FPGA output buffer where the CB starts */
  const uint8_t *multi_outdata; /*!< pointer to the head of the FPGA output buffer */
  uint8_t *dst_c; /*!< pointer to the head of the destination array, where FPGA buffer should be copied to */
  int KbZ; /*!< K_b * Z_c */
  int K; /*!< Number of systematic bits */

  bool check_crc; /*!< Defines, if crc calculation should be done */
  int length_dec; /*!< Number of information bits + CRC in one CB */
  uint8_t crc_type; /*!< Defines which crc type is used: {CRC24_A, CRC24_B, CRC16} */
  task_ans_t *ans; /*!< pointer to the answer that is used by thread pool to detect job completion */
} args_fpga_post_decode_t;

int32_t nrLDPC_coding_init(void);
int32_t nrLDPC_coding_shutdown(void);
int32_t nrLDPC_coding_decoder(nrLDPC_slot_decoding_parameters_t *slot_params, int frame_rx, int slot_rx);
int decoder_xdma(nrLDPC_TB_decoding_parameters_t *TB_params, int frame_rx, int slot_rx, tpool_t *ldpc_threadPool);
void nr_ulsch_FPGA_decoding_prepare_blocks(void *args);
void nr_ulsch_FPGA_post_decoding_p(void *args);
static inline void nr_ulsch_FPGA_post_decoding_s(const args_fpga_post_decode_t *args_post_decode);

static inline size_t get_number_of_parity_bits(const uint32_t K, const uint32_t R, const uint32_t Kc, const uint32_t Z);
static inline uint32_t get_CB_offset(const uint32_t K, const uint32_t Kb, const uint32_t R, const uint32_t Kc, const uint32_t Z);
static inline void pack_16bits_to_8bits_range(const int16_t *const src_ptr, int8_t *const dst_ptr, const size_t dst_range);
static inline uint8_t get_current_R(const nrLDPC_TB_decoding_parameters_t *nrLDPC_TB_decoding_parameters, const size_t current_r);
static inline int get_current_E(const nrLDPC_TB_decoding_parameters_t *nrLDPC_TB_decoding_parameters, const size_t current_r);
static inline int get_current_llr_offset(const nrLDPC_TB_decoding_parameters_t *nrLDPC_TB_decoding_parameters,
                                         const size_t current_r);
static inline void get_exact_BG_Kb(const uint8_t BG, const uint32_t B, uint32_t *o_Kb, uint8_t *o_BG);
static inline uint32_t get_B(const uint32_t A);

/**
 * To support segment decoding as well, the following function are implemented.
 */
int32_t LDPCinit(void);
int32_t LDPCshutdown(void);
int32_t LDPCdecoder(t_nrLDPC_dec_params *p_decParams,
                    int8_t *p_llr,
                    uint8_t *p_out,
                    t_nrLDPC_time_stats *time_stats,
                    decode_abort_t *ab);
int32_t LDPCinit(void)
{
  LOG_I(PHY, "Using Aurora HW LDPC decode accelerator.\n");
  devices_t dev = {.dec_read_device = DEVICE_NAME_DEFAULT_DEC_READ,
                   .dec_write_device = DEVICE_NAME_DEFAULT_DEC_WRITE,
                   .user_device = DEVICE_NAME_DEFAULT_USER};
  int32_t ret = test_dma_init(dev);
  if (ret < 0) {
    LOG_E(PHY, "Unable to use Dec HW ACC!\n");
    exit(1);
  }
  return ret;
}

int32_t LDPCshutdown(void)
{
  dma_close();
  return 0;
}

// decoder interface
/**
   \brief LDPC decoder API type definition
   \param p_decParams LDPC decoder parameters
   \param p_llr Input LLRs
   \param p_llrOut Output vector
   \param time_stats time statistics
   \param ab structure shared between tasks to stop all the tasks if one fails
*/
int32_t LDPCdecoder(t_nrLDPC_dec_params *p_decParams,
                    int8_t *p_llr,
                    uint8_t *p_out,
                    t_nrLDPC_time_stats *time_stats,
                    decode_abort_t *ab)
{
  DecIFConf dec_conf = {0};
  dec_conf.Zc = p_decParams->Z;
  dec_conf.BG = p_decParams->BG;
  // select correct BG
  uint32_t Kb = 0;
  // because just one CB is decoded here the following is valid K'=B'=B.
  const uint32_t B = p_decParams->Kprime;
  get_exact_BG_Kb(p_decParams->BG, B, &Kb, &dec_conf.BG);
  dec_conf.max_iter = min(max(p_decParams->numMaxIter, NUMB_OF_MIN_DEC_ITER), NUMB_OF_MAX_DEC_ITER);
  dec_conf.numCB = 1;
  dec_conf.max_schedule = 0;
  dec_conf.SetIdx = 12;

#define MAX_IN_DEC_ARRAY_SIZE (OAI_UL_LDPC_MAX_NUM_LLR + HEADER_SIZE)
#define MAX_OUT_DEC_ARRAY_SIZE (MAX_CB_SIZE_IN_BYTE_UNITS + HEADER_SIZE)
  int8_t buffer_in[MAX_IN_DEC_ARRAY_SIZE] __attribute__((aligned(PAGE_SIZE)));
  uint8_t buffer_out[MAX_OUT_DEC_ARRAY_SIZE] __attribute__((aligned(PAGE_SIZE)));
  const int K = p_decParams->BG == 2 ? 10 * p_decParams->Z : 22 * p_decParams->Z;
  const int KbZ = Kb * p_decParams->Z;
  // filler bits length
  const int F = KbZ - p_decParams->Kprime;
  start_meas(&time_stats->llr2llrProcBuf);
  // copy all LLRs in internal buffer starting at HEADER_SIZE
  start_meas(&time_stats->total);
  // copy information bits
  memcpy(&buffer_in[HEADER_SIZE], p_llr, p_decParams->Kprime);
  // set filler bits
  memset(&buffer_in[HEADER_SIZE + p_decParams->Kprime], INT8_MAX, F);
  const int Kc = p_decParams->BG == 2 ? 52 : 68;
  // copy parity bits
  const int numb_of_parity_bits = get_number_of_parity_bits(K, p_decParams->R, Kc, p_decParams->Z);
  dec_conf.numb_of_parity_bits_per_CB[0] = numb_of_parity_bits;
  memcpy(&buffer_in[HEADER_SIZE + KbZ], p_llr + K, numb_of_parity_bits);
  stop_meas(&time_stats->llr2llrProcBuf);
  start_meas(&time_stats->llr2bit);
  int32_t niter = nrLDPC_decoder_FPGA((uint8_t *)&buffer_in[0], &buffer_out[0], dec_conf);
  stop_meas(&time_stats->llr2bit);
  start_meas(&time_stats->llrRes2llrOut);
  // copy into out buffer
  const args_fpga_post_decode_t post_decode_arg = {.r_first = 0,
                                                   .KbZ = Kb * p_decParams->Z,
                                                   .r_span = 1,
                                                   .output_CBoffset = 0,
                                                   .multi_outdata = &buffer_out[0],
                                                   .check_crc = false,
                                                   .dst_c = p_out,
                                                   .K = K};
  nr_ulsch_FPGA_post_decoding_s(&post_decode_arg);
  stop_meas(&time_stats->llrRes2llrOut);
  stop_meas(&time_stats->total);
  if (p_decParams->check_crc != NULL) {
    const bool crc_valid = p_decParams->check_crc(p_out, p_decParams->Kprime, p_decParams->crc_type);
    if (!crc_valid) {
      LOG_D(PHY, "Segment CRC NOK!\n");
    }
  }
  return niter;
}

int32_t nrLDPC_coding_init(void)
{
  return LDPCinit();
}

int32_t nrLDPC_coding_shutdown(void)
{
  return LDPCshutdown();
}

int32_t nrLDPC_coding_decoder(nrLDPC_slot_decoding_parameters_t *slot_params, int frame_rx, int slot_rx)
{
  int nbDecode = 0;
  for (int ULSCH_id = 0; ULSCH_id < slot_params->nb_TBs; ULSCH_id++)
    nbDecode += decoder_xdma(&slot_params->TBs[ULSCH_id], frame_rx, slot_rx, slot_params->threadPool);
  return nbDecode;
}

int decoder_xdma(nrLDPC_TB_decoding_parameters_t *TB_params, int frame_rx, int slot_rx, tpool_t *ldpc_threadPool)
{
  DevAssert(TB_params->C <= MAX_CB);
  const uint32_t B = get_B(TB_params->A);
  const uint32_t K = TB_params->K;
  // numb of columns in the BG (52 for BG2 and 68 for BG1) -> number of coded words
  const int Kc = TB_params->BG == 2 ? 52 : 68;
  const uint32_t Z = TB_params->Z;
  int r_offset = 0;
  // number of true information bits
  const int Kprime = K - TB_params->F;
// FPGA parameter preprocessing
#define MAX_INPUT_FPGA_SIZE CEIL_UP_16B((OAI_UL_LDPC_MAX_NUM_LLR + HEADER_SIZE) * MAX_CB)
#define MAX_OUTPUT_FPGA_SIZE CEIL_UP_16B((MAX_CB_SIZE_IN_BYTE_UNITS + HEADER_SIZE) * MAX_CB)
  static uint8_t multi_indata[MAX_INPUT_FPGA_SIZE] __attribute__((aligned(PAGE_SIZE))); // FPGA input data
  static uint8_t multi_outdata[MAX_OUTPUT_FPGA_SIZE] __attribute__((aligned(PAGE_SIZE))); // FPGA output data
  // maximum possible K_b value
  const int bg_len = TB_params->BG == 1 ? 22 : 10;

  int input_CBoffset = 0;

  DecIFConf dec_conf = {0};
  dec_conf.Zc = Z;
  uint32_t Kb = 0;
  get_exact_BG_Kb(TB_params->BG, B, &Kb, &dec_conf.BG);
  dec_conf.max_iter = min(max(TB_params->max_ldpc_iterations, NUMB_OF_MIN_DEC_ITER), NUMB_OF_MAX_DEC_ITER);
  dec_conf.numCB = TB_params->C;
  dec_conf.max_schedule = 0;
  dec_conf.SetIdx = 12;

  // Z_c * (10 or 22) is K; Calculate the number of bits in one CB
  int out_CBoffset = Z * bg_len + HEADER_SIZE * 8;
  out_CBoffset = CEIL_UP(out_CBoffset, 128);
  // conv to 8 bit units
  out_CBoffset /= 8;

  const int length_dec = lenWithCrc(TB_params->C, TB_params->A);
  const uint8_t crc_type = crcType(TB_params->C, TB_params->A);

  const size_t num_threads_prepare_max = ldpc_threadPool->len_thr;
  uint32_t num_threads_prepare = 0;
  uint32_t r_spans[MAX_CB] = {};
  // calculate required number of jobs
  if (num_threads_prepare_max == 0) {
    r_spans[0] = TB_params->C;
    num_threads_prepare = 0;
  } else {
    uint32_t r_while = 0;
    while (r_while < TB_params->C) {
      // calculate number of segments processed in the new job
      const uint32_t r_rem = TB_params->C - r_while;
      const uint32_t t_rem = num_threads_prepare_max - num_threads_prepare;
      const uint32_t modulus = r_rem % t_rem;
      const uint32_t quotient = r_rem / t_rem;
      const uint32_t r_span_max = modulus == 0 ? quotient : quotient + 1;

      // saturate to be sure to not go above C
      const uint32_t r_span = min(r_rem, r_span_max);
      r_spans[r_while] = r_span;
      // increment
      num_threads_prepare++;
      r_while += r_span;
    }
  }
  const size_t arr_size = max(num_threads_prepare, 1);
  args_fpga_decode_prepare_t arr[arr_size];
  task_ans_t ans;
  init_task_ans(&ans, arr_size);
  thread_info_tm_t t_info = {.buf = (uint8_t *)arr, .len = 0, .cap = arr_size, .ans = &ans};
  // start the prepare jobs
  for (uint32_t r = 0; r < TB_params->C; /*r+=r_span*/) {
    args_fpga_decode_prepare_t *args = &((args_fpga_decode_prepare_t *)t_info.buf)[t_info.len];
    DevAssert(t_info.len < t_info.cap);
    args->ans = t_info.ans;
    t_info.len += 1;

    args->TB_params = TB_params;
    args->multi_indata = &multi_indata[0];
    args->r_first = r;
    const uint32_t r_span = r_spans[r];
    args->r_span = r_span;
    args->r_offset = r_offset;
    args->input_CBoffset = input_CBoffset;
    args->Kc = Kc;
    args->Kprime = Kprime;
    args->Kb = Kb;

    // add offset before starting threads, because d_to_be_cleared can be reseted in threads
    for (size_t current_r = r; current_r < (r + r_span); ++current_r) {
      const int current_E = get_current_E(TB_params, current_r);
      const uint32_t current_R = get_current_R(TB_params, current_r);
      input_CBoffset += get_CB_offset(K, Kb, current_R, Kc, Z);
      dec_conf.numb_of_parity_bits_per_CB[current_r] = get_number_of_parity_bits(K, current_R, Kc, Z);
      r_offset += current_E;
    }
    task_t t = {.func = &nr_ulsch_FPGA_decoding_prepare_blocks, .args = args};
    pushTpool(ldpc_threadPool, t);
    r += r_span;
  }

  DevAssert(arr_size == t_info.len);

  // wait for the prepare jobs to complete. After all jobs are completed FPGA input buffer is set with all CBs -> ready to decode
  join_task_ans(t_info.ans);
  // launch decode with FPGA
  //==================================================================
  //  Xilinx FPGA LDPC decoding function -> nrLDPC_decoder_FPGA()
  //==================================================================
  start_meas(&TB_params->ts_ldpc_decode);
  (void)nrLDPC_decoder_FPGA(&multi_indata[0], &multi_outdata[0], dec_conf);
  stop_meas(&TB_params->ts_ldpc_decode);
#if USE_OUTPUT_PARALLELIZATION
  // Copy to external buffer using the threadpool
  args_fpga_post_decode_t post_decode_args[MAX_CB];
  init_task_ans(&ans, arr_size);
  for (uint32_t r = 0; r < TB_params->C; /*r+=r_span*/) {
    const uint32_t r_span = r_spans[r];
    post_decode_args[r].ans = &ans;
    post_decode_args[r].r_first = r;
    post_decode_args[r].crc_type = crc_type;
    post_decode_args[r].length_dec = length_dec;
    post_decode_args[r].K = K;
    post_decode_args[r].KbZ = Kb * Z;
    post_decode_args[r].r_span = r_span;
    post_decode_args[r].TB_params = TB_params;
    post_decode_args[r].output_CBoffset = out_CBoffset;
    post_decode_args[r].multi_outdata = &multi_outdata[0];
    post_decode_args[r].check_crc = true;
    post_decode_arg[r].dst_c = TB_params->c;
    task_t t = {.func = &nr_ulsch_FPGA_post_decoding_p, .args = &post_decode_args[r]};
    pushTpool(ldpc_threadPool, t);
    r += r_span;
  }
  join_task_ans(&ans);
#else
  const args_fpga_post_decode_t post_decode_arg = {.r_first = 0,
                                                   .crc_type = crc_type,
                                                   .length_dec = length_dec,
                                                   .KbZ = Kb * Z,
                                                   .K = K,
                                                   .r_span = TB_params->C,
                                                   .TB_params = TB_params,
                                                   .output_CBoffset = out_CBoffset,
                                                   .multi_outdata = &multi_outdata[0],
                                                   .check_crc = true,
                                                   .dst_c = TB_params->c};
  nr_ulsch_FPGA_post_decoding_s(&post_decode_arg);
#endif
  // calculate the number of processed segments
  *TB_params->processedSegments = 0;
  for (uint32_t r = 0; r < TB_params->C; ++r) {
    *TB_params->processedSegments += TB_params->decodeSuccess[r];
  }
  return 0;
}

static inline uint32_t get_CB_offset(const uint32_t K, const uint32_t Kb, const uint32_t R, const uint32_t Kc, const uint32_t Z)
{
  const uint32_t numb_of_parity_bits = get_number_of_parity_bits(K, R, Kc, Z);
  const uint32_t offset = Kb * Z + numb_of_parity_bits + HEADER_SIZE; //< Kb is used here, because it can be smaller than K
  const uint32_t offset_16B = CEIL_UP_16B(offset);
  return offset_16B;
}

static inline size_t get_number_of_parity_bits(const uint32_t K, const uint32_t R, const uint32_t Kc, const uint32_t Z)
{
  // default use all parity bits
  uint32_t numb_of_parity_bits = Kc * Z - K;
  // code rate definition: R=K/N=K/(K-2*Z+P)=K/(K-2*Z+N+2*Z-K)=K/N P:numb of parity bits (N+2*Z-K)
  switch (R) {
    case 13:
      // K * 3 - K + 2 * Z;
      numb_of_parity_bits = 2 * K + 2 * Z;
      break;
    case 23:
      if ((K % 2) != 0) {
        LOG_W(PHY, "K isn't a multiple of 2! K %u\n", K);
      } else {
        // numb_of_parity_bits = (3 * K) / 2 - K + 2 * Z;
        numb_of_parity_bits = K / 2 + 2 * Z;
      }
      break;
    case 89:
      if ((K % 8) != 0) {
        LOG_W(PHY, "K isn't a multiple of 8! K: %u\n", K);
      } else {
        //(9 * K) / 8 - K + 2 * Z;
        numb_of_parity_bits = K / 8 + 2 * Z;
      }
      break;
    case 15:
      // 5 * K - K + 2 * Z;
      numb_of_parity_bits = 4 * K + 2 * Z;
      break;
    default:
      LOG_W(PHY, "Code rate %u isn't supported!\n", R);
      break;
  }
  const uint32_t numb_of_parity_bits_ceiled = CEIL_UP(numb_of_parity_bits, Z);
  return numb_of_parity_bits_ceiled;
}

static inline void nr_ulsch_FPGA_post_decoding_s(const args_fpga_post_decode_t *args_post_decode)
{
  const uint32_t r_end = args_post_decode->r_first + args_post_decode->r_span;
  const int KbZ = args_post_decode->KbZ;
  const int K = args_post_decode->K;
  const int K_diff = K - KbZ;
  const size_t cK = KbZ / 8;
  const size_t rem_cK = KbZ % 8;
  uint8_t *dst_c = args_post_decode->dst_c;
  for (uint32_t r = args_post_decode->r_first; r < r_end; r++) {
    uint8_t *local_c = dst_c + r * cK;
    const uint8_t *CB_out_ptr = &args_post_decode->multi_outdata[r * args_post_decode->output_CBoffset + HEADER_SIZE];
    memcpy(local_c, CB_out_ptr, cK);
    if (rem_cK != 0) {
      // set last bits
      const uint8_t b0 = CB_out_ptr[cK];
      local_c[cK] = 0;
      local_c[cK] |= (b0 & ((1 << rem_cK) - 1));
    }
    // set K_diff bits to zero
    const size_t ck_8 = cK + (rem_cK > 0);
    memset(local_c + ck_8, 0, K_diff / 8);

    if (args_post_decode->check_crc) {
      const bool crc_successful = check_crc(local_c, args_post_decode->length_dec, args_post_decode->crc_type);
      args_post_decode->TB_params->decodeSuccess[r] = crc_successful;
    }
  }
}

void nr_ulsch_FPGA_post_decoding_p(void *args)
{
  args_fpga_post_decode_t *arguments = (args_fpga_post_decode_t *)args;
  nr_ulsch_FPGA_post_decoding_s(arguments);
  completed_task_ans(arguments->ans);
}

static inline size_t pack_16bits_to_8bits_range_128(const int16_t *const src_ptr, int8_t *const dst_ptr, const size_t dst_range)
{
  const size_t dst_range_in_units = dst_range / sizeof(simde__m128i);
  simde__m128i *const dst_ptr_simd = (simde__m128i *const)dst_ptr;
  const simde__m128i *const src_ptr_simd = (const simde__m128i *const)src_ptr;
  size_t j = 0;
  for (size_t i = 0; j < dst_range_in_units; i += 2, ++j) {
    dst_ptr_simd[j] = simde_mm_packs_epi16(src_ptr_simd[i], src_ptr_simd[i + 1]);
  }
  return j * sizeof(simde__m128i);
}

static inline size_t pack_16bits_to_8bits_range_256(const int16_t *const src_ptr, int8_t *const dst_ptr, const size_t dst_range)
{
  const size_t dst_range_in_units = dst_range / sizeof(simde__m256i);
  simde__m256i *const dst_ptr_simd = (simde__m256i *const)dst_ptr;
  const simde__m128i *const src_ptr_simd = (const simde__m128i *const)src_ptr;
  size_t j = 0;
  for (size_t i = 0; j < dst_range_in_units; i += 4, ++j) {
    // creating SIMD256 with the layout A0B0 where A0 is the first 128bit of the first 256bit and B0 is the first 128bit of the
    // second 256bit
    const simde__m256i a = simde_mm256_set_m128i(src_ptr_simd[i + 2], src_ptr_simd[i + 0]);
    // creating SIMD256 with the layout A1B1 where A1 is the second 128bit of the first 256bit and B1 is the second 128bit of the
    // second 256bit
    const simde__m256i b = simde_mm256_set_m128i(src_ptr_simd[i + 3], src_ptr_simd[i + 1]);
    // packing A0B0A1B1 to sat16to8(A0)sat16to8(A1)|sat16to8(B0)sat16to8(B1)
    //                     first 128 bit           | second 128bit
    dst_ptr_simd[j] = simde_mm256_packs_epi16(a, b);
  }
  return j * sizeof(simde__m256i);
}

static inline void pack_16bits_to_8bits_range(const int16_t *const src_ptr, int8_t *const dst_ptr, const size_t dst_range)
{
  size_t offset = 0;
  if ((((uintptr_t)dst_ptr) % 32U) == 0) {
    // src ptr has to be a multiple of 32
    DevAssert((((uintptr_t)src_ptr)) % 32U == 0);
    offset = pack_16bits_to_8bits_range_256(src_ptr, dst_ptr, dst_range);
  } else {
    offset = pack_16bits_to_8bits_range_128(src_ptr, dst_ptr, dst_range);
  }
  // set last bytes in the case dst range isn't a multiple of the respective SIMD type
  for (size_t i = offset; i < dst_range; ++i) {
    int16_t tmp = max(src_ptr[i], ((int16_t)INT8_MIN));
    tmp = min(tmp, ((int16_t)INT8_MAX));
    dst_ptr[i] = (int8_t)tmp;
  }
}

/*!
 * \fn nr_ulsch_FPGA_decoding_prepare_blocks(void *args)
 * \brief prepare blocks for LDPC decoding on FPGA
 *
 * \param args pointer to the arguments of the function in a structure of type args_fpga_decode_prepare_t
 */
void nr_ulsch_FPGA_decoding_prepare_blocks(void *args)
{
  // extract the arguments
  args_fpga_decode_prepare_t *arguments = (args_fpga_decode_prepare_t *)args;

  const nrLDPC_TB_decoding_parameters_t *TB_params = arguments->TB_params;

  const uint8_t Qm = TB_params->Qm;

  const uint8_t BG = TB_params->BG;
  const uint8_t rv_index = TB_params->rv_index;

  const uint32_t tbslbrm = TB_params->tbslbrm;
  const uint32_t K = TB_params->K;
  const uint32_t Z = TB_params->Z;
  const uint32_t F = TB_params->F;

  const uint32_t C = TB_params->C;

  short *ulsch_llr = TB_params->llr;

  uint8_t *multi_indata = arguments->multi_indata;
  uint32_t r_first = arguments->r_first;
  uint32_t r_span = arguments->r_span;
  int r_offset = arguments->r_offset;
  int input_CBoffset = arguments->input_CBoffset;
  const int Kc = arguments->Kc;
  const int Kprime = arguments->Kprime;
  const int Kb = arguments->Kb;
  const int KbZ = Kb * Z;
  // Filler bits regarding Kb value
  const int FF = KbZ - Kprime;
  int16_t z[68 * 384] __attribute__((aligned(32)));
  // the function processes r_span blocks starting from block at index r_first in ulsch_llr
  for (uint32_t r = r_first; r < (r_first + r_span); r++) {
    const int E = get_current_E(TB_params, r);
    const uint32_t R = get_current_R(TB_params, r);
    const size_t offset = get_CB_offset(K, Kb, R, Kc, Z);
    // ----------------------- FPGA pre process ------------------------
    int8_t *const temp_multi_indata = (int8_t *const)&multi_indata[input_CBoffset + HEADER_SIZE];
    // -----------------------------------------------------------------

    // code blocks after bit selection in rate matching for LDPC code (38.212 V15.4.0 section 5.4.2.1)
    int16_t harq_e[E];
    // -------------------------------------------------------------------------------------------
    // deinterleaving
    // -------------------------------------------------------------------------------------------
    nr_deinterleaving_ldpc(E, Qm, harq_e, ulsch_llr + r_offset);
    // -------------------------------------------------------------------------------------------
    // dematching
    // -------------------------------------------------------------------------------------------
    int16_t *local_d = TB_params->d + r * Kc * Z;

    // Check, if this error occurs
    if (nr_rate_matching_ldpc_rx(tbslbrm, BG, Z, local_d, harq_e, C, rv_index, TB_params->d_to_be_cleared, E, F, K - F - 2 * Z)
        == -1) {
      LOG_E(PHY, "ulsch_decoding.c: Problem in rate_matching\n");
      completed_task_ans(arguments->ans);
      return;
    }
    // set first 2*Z_c bits to zeros; these are the punctured bits
    memset(&z[0], 0, 2 * Z * sizeof(int16_t));
    // set Filler bits
    memset((&z[0] + Kprime), INT8_MAX, FF * sizeof(int16_t));
    // Move coded bits before filler bits
    memcpy((&z[0] + 2 * Z), local_d, (Kprime - 2 * Z) * sizeof(int16_t));
    const uint32_t numb_of_parity_bits = get_number_of_parity_bits(K, R, Kc, Z);
    // skip filler bits, set paraity bits
    memcpy((&z[0] + KbZ), local_d + (K - 2 * Z), numb_of_parity_bits * sizeof(int16_t));
    const size_t numb_to_copy = CEIL_UP_16B(KbZ + numb_of_parity_bits);
    pack_16bits_to_8bits_range(&z[0], &temp_multi_indata[0], numb_to_copy);
    r_offset += E;
    input_CBoffset += offset;
  }

  completed_task_ans(arguments->ans);
}

static inline uint8_t get_current_R(const nrLDPC_TB_decoding_parameters_t *nrLDPC_TB_decoding_parameters, const size_t current_r)
{
  if (current_r < nrLDPC_TB_decoding_parameters->first_rE2)
    return nrLDPC_TB_decoding_parameters->R;
  return nrLDPC_TB_decoding_parameters->R2;
}

static inline int get_current_E(const nrLDPC_TB_decoding_parameters_t *nrLDPC_TB_decoding_parameters, const size_t current_r)
{
  if (current_r < nrLDPC_TB_decoding_parameters->first_rE2)
    return nrLDPC_TB_decoding_parameters->E;
  return nrLDPC_TB_decoding_parameters->E2;
}

static inline int get_current_llr_offset(const nrLDPC_TB_decoding_parameters_t *nrLDPC_TB_decoding_parameters,
                                         const size_t current_r)
{
  if (current_r < nrLDPC_TB_decoding_parameters->first_rE2)
    return current_r * nrLDPC_TB_decoding_parameters->E;
  return nrLDPC_TB_decoding_parameters->first_rE2 * nrLDPC_TB_decoding_parameters->E
         + (current_r - nrLDPC_TB_decoding_parameters->first_rE2) * nrLDPC_TB_decoding_parameters->E2;
}

static inline uint32_t get_B(const uint32_t A)
{
  return lenWithCrc(0, A);
}

static inline void get_exact_BG_Kb(const uint8_t BG, const uint32_t B, uint32_t *o_Kb, uint8_t *o_BG)
{
  switch (BG) {
    case 1:
      *o_BG = 1;
      *o_Kb = 22;
      break;
    case 2:
      if (B > 640) {
        *o_Kb = 10;
        *o_BG = 2;
      } else if (B > 560) {
        *o_Kb = 9;
        *o_BG = 3;
      } else if (B > 192) {
        *o_Kb = 8;
        *o_BG = 4;
      } else {
        *o_Kb = 6;
        *o_BG = 5;
      }
      break;
  }
}
