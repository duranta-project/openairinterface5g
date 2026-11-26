/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!\file nrLDPC_armral_decoder.c
 * \brief LDPC code block decoder on the Arm RAN Acceleration Library (ArmRAL)
 *
 * libldpc_armral.so has the interface of libldpc.so: the segment coding layer
 * (rate matching, deinterleaving, thread pool) and the encoder are OAI's; only
 * the decoding of one code block calls ArmRAL's layered offset min-sum
 * decoder. Early termination uses the code block CRC (CRC24B, or the transport
 * block CRC for a single code block), which ArmRAL checks after every
 * iteration.
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <arm_neon.h>
#include <armral.h>
#include "assertions.h"
#include "openair1/PHY/CODING/coding_defs.h"
#include "openair1/PHY/CODING/nrLDPC_extern.h"
#include "openair1/PHY/CODING/nrLDPC_decoder/nrLDPCdecoder_defs.h"

/* LLR scaling before decoding, see normalize_llr() */
#define LLR_MEAN_PER_RATE 36 // target mean |LLR| per code rate R = K'/E, up to R = 2/3
#define LLR_MEAN_MIN 6 // lowest target
#define LLR_MEAN_HIGH_RATE 64 // above R = 2/3: up-scaling target, no down-scaling
#define LLR_UPSCALE_MAX 16

/* the first 2 Z_c systematic bits are never transmitted (TS 38.212 5.3.2) */
#define NUM_PUNCTURED_COLS 2

/* systematic columns: K = 22 Z_c (BG1) or 10 Z_c (BG2), TS 38.212 5.2.2 */
static inline int num_sys_cols(int BG)
{
  return BG == 1 ? 22 : 10;
}

/* number of base graph columns OAI transmits for a code rate (nrLDPC_init.h),
 * including the punctured ones */
static int num_cols(int BG, int R)
{
  if (BG == 1)
    return R == 89 ? NR_LDPC_NCOL_BG1_R89 : R == 23 ? NR_LDPC_NCOL_BG1_R23 : NR_LDPC_NCOL_BG1_R13;
  return R == 23 ? NR_LDPC_NCOL_BG2_R23 : R == 13 ? NR_LDPC_NCOL_BG2_R13 : NR_LDPC_NCOL_BG2_R15;
}

/* ArmRAL's decoder work buffer: one per thread pool worker, allocated once at
 * the size of the largest code block (Z_c 384) and freed when the thread
 * exits */
typedef struct {
  uint32_t size;
  void *ptr;
} work_buf_t;

static pthread_key_t work_buf_key;
static pthread_once_t work_buf_once = PTHREAD_ONCE_INIT;

static void free_work_buf(void *arg)
{
  work_buf_t *b = arg;
  free(b->ptr);
  free(b);
}

static void create_work_buf_key(void)
{
  AssertFatal(pthread_key_create(&work_buf_key, free_work_buf) == 0, "pthread_key_create() failed\n");
}

static void *get_work_buf(uint32_t max_it)
{
  pthread_once(&work_buf_once, create_work_buf_key);
  work_buf_t *b = pthread_getspecific(work_buf_key);
  if (!b) {
    b = calloc(1, sizeof(*b));
    AssertFatal(b, "cannot allocate the ArmRAL work buffer descriptor\n");
    AssertFatal(pthread_setspecific(work_buf_key, b) == 0, "pthread_setspecific() failed\n");
  }
  const uint32_t bg1 = armral_ldpc_decode_block_noalloc_buffer_size(LDPC_BASE_GRAPH_1, NR_LDPC_ZMAX, max_it);
  const uint32_t bg2 = armral_ldpc_decode_block_noalloc_buffer_size(LDPC_BASE_GRAPH_2, NR_LDPC_ZMAX, max_it);
  const uint32_t need = bg1 > bg2 ? bg1 : bg2;
  if (need > b->size) { // first call of a thread, or more iterations than before
    free(b->ptr);
    b->ptr = aligned_alloc(64, (need + 63) & ~63u);
    AssertFatal(b->ptr, "cannot allocate %u bytes for the ArmRAL LDPC decoder\n", need);
    b->size = need;
  }
  return b->ptr;
}

int32_t LDPCinit(void)
{
  return 0;
}

int32_t LDPCshutdown(void)
{
  return 0;
}

static uint32_t armral_crc_option(int crc_type)
{
  switch (crc_type) {
    case CRC24_A:
      return ARMRAL_LDPC_CRC_24A;
    case CRC24_B:
      return ARMRAL_LDPC_CRC_24B;
    case CRC16:
      return ARMRAL_LDPC_CRC_16;
  }
  AssertFatal(false, "unsupported CRC type %d\n", crc_type);
  return 0;
}

/* ArmRAL clamps its input to [-32, 32] and decodes with scaled offset min-sum
 * on int8 (offset 2, scale 0.75), so the result depends on the LLR scale.
 * Large LLRs saturate the messages during the iterations: error floor at low
 * and medium code rates. Small LLRs lose the check messages to the fixed
 * offset: high code rates fail, and with a weak receiver (mean |LLR| of a few
 * units) every rate fails. OAI's LLR scale depends on the receiver and the
 * modulation. normalize_llr() therefore brings the mean |LLR| of a code block
 * into the range the decoder needs, by code rate R = K'/E. E is the number of
 * rate-matched bits without fillers, taken as the position of the last
 * non-zero LLR (rate matching fills the circular buffer contiguously, so zero
 * LLRs of a weak receiver in between do not count).
 * - R <= 2/3: target T = max(LLR_MEAN_PER_RATE * R, LLR_MEAN_MIN). Above T the
 *   LLRs are scaled down to T (rounded to nearest), below T/2 they are
 *   multiplied by the integer T / mean (lossless, at most LLR_UPSCALE_MAX).
 * - R > 2/3: no down-scaling (the long check rows need the full resolution);
 *   below LLR_MEAN_HIGH_RATE/2 the LLRs are multiplied by the integer
 *   LLR_MEAN_HIGH_RATE / mean (at most LLR_UPSCALE_MAX).
 * Filler bits get the largest value. */
static uint32_t sum_abs(const int8_t *x, uint32_t len)
{
  uint32_t sum = 0, i = 0;
  for (; i + 16 <= len; i += 16)
    sum += vaddlvq_u8(vreinterpretq_u8_s8(vqabsq_s8(vld1q_s8(x + i))));
  for (; i < len; i++)
    sum += x[i] == INT8_MIN ? INT8_MAX : abs(x[i]);
  return sum;
}

/* end of the non-zero LLRs in [start, end): last non-zero index + 1, or start */
static uint32_t nonzero_end(const int8_t *x, uint32_t start, uint32_t end)
{
  uint32_t i = end;
  while (i >= start + 16 && vmaxvq_u8(vreinterpretq_u8_s8(vld1q_s8(x + i - 16))) == 0)
    i -= 16;
  while (i > start && x[i - 1] == 0)
    i--;
  return i;
}

/* a * q8 / 256 per byte, rounded to nearest */
static inline uint8x16_t mul_q8(uint8x16_t a, uint8x8_t q8)
{
  const uint8x8_t lo = vrshrn_n_u16(vmull_u8(vget_low_u8(a), q8), 8);
  const uint8x8_t hi = vrshrn_n_u16(vmull_u8(vget_high_u8(a), q8), 8);
  return vcombine_u8(lo, hi);
}

/* x * k per byte, saturated to int8 */
static inline int8x16_t mul_sat(int8x16_t x, int8x8_t k)
{
  const int8x8_t lo = vqmovn_s16(vmull_s8(vget_low_s8(x), k));
  const int8x8_t hi = vqmovn_s16(vmull_s8(vget_high_s8(x), k));
  return vcombine_s8(lo, hi);
}

/* in place: x = x * q8 / 256 (q8 > 0, rounded to nearest, sign kept) or,
 * with q8 == 0, x = x * k (saturating) */
static void scale(int8_t *x, uint32_t len, uint8_t q8, int8_t k)
{
  uint32_t i = 0;
  for (; i + 16 <= len; i += 16) {
    const int8x16_t v = vld1q_s8(x + i);
    if (q8) {
      const uint8x16_t a = vreinterpretq_u8_s8(vqabsq_s8(v));
      const int8x16_t m = vreinterpretq_s8_u8(mul_q8(a, vdup_n_u8(q8)));
      vst1q_s8(x + i, vbslq_s8(vcltzq_s8(v), vnegq_s8(m), m));
    } else {
      vst1q_s8(x + i, mul_sat(v, vdup_n_s8(k)));
    }
  }
  for (; i < len; i++) {
    const int a = x[i] == INT8_MIN ? INT8_MAX : abs(x[i]);
    const int v = q8 ? (a * q8 + 128) >> 8 : (a * k > INT8_MAX ? INT8_MAX : a * k);
    x[i] = x[i] < 0 ? -v : v;
  }
}

/* in place on the n LLRs of a code block; the fillers are the nfill LLRs from
 * filler_start */
static void normalize_llr(int8_t *llr, uint32_t n, uint32_t filler_start, uint32_t nfill, uint32_t Kprime)
{
  const uint32_t filler_end = filler_start + nfill;
  uint32_t end = nonzero_end(llr, filler_end, n);
  if (end == filler_end)
    end = nonzero_end(llr, 0, filler_start);
  memset(llr + filler_start, INT8_MAX, nfill);
  const uint32_t head = end < filler_start ? end : filler_start; // LLRs before the fillers
  const uint32_t tail = end > filler_end ? end - filler_end : 0; // LLRs after the fillers
  const uint32_t e = head + tail;
  const uint32_t sum = sum_abs(llr, head) + sum_abs(llr + filler_end, tail);
  if (e == 0 || sum == 0)
    return;

  uint32_t target = 0, k = 1;
  if (3 * Kprime <= 2 * e) {
    target = LLR_MEAN_PER_RATE * Kprime / e;
    if (target < LLR_MEAN_MIN)
      target = LLR_MEAN_MIN;
    if (sum <= target * e) {
      if (2 * sum <= target * e)
        k = (uint64_t)target * e / sum;
      target = 0;
    }
  } else if (2 * sum <= LLR_MEAN_HIGH_RATE * e) {
    k = (uint64_t)LLR_MEAN_HIGH_RATE * e / sum;
  }
  if (k > LLR_UPSCALE_MAX)
    k = LLR_UPSCALE_MAX;
  if (target == 0 && k == 1)
    return;
  const uint8_t q8 = target ? ((uint64_t)target * e << 8) / sum : 0; // scale factor < 1 in Q8
  scale(llr, head, q8, k);
  scale(llr + filler_end, tail, q8, k);
}

/* p_llr: int8 LLRs in OAI's layout (punctured columns of zeros, message with
 * filler LLRs, parity). They are the caller's per-code-block copy (segment
 * layer, ldpctest) and are scaled in place.
 * Returns the number of iterations, or numMaxIter if decoding failed. */
int32_t LDPCdecoder(t_nrLDPC_dec_params *p, int8_t *p_llr, uint8_t *p_out, t_nrLDPC_time_stats *ts, decode_abort_t *ab)
{
  (void)ts; // ArmRAL decodes a code block in one call: no per-step profiling
  AssertFatal(p->outMode == nrLDPC_outMode_BIT, "ArmRAL LDPC decoder: only bit output is supported\n");
  const int max_it = p->numMaxIter;
  if (check_abort(ab))
    return max_it;

  const int Z = p->Z;
  const int K = num_sys_cols(p->BG) * Z;
  const int Kprime = p->Kprime > 0 && p->Kprime <= K ? p->Kprime : K;
  const uint32_t nfill = K - Kprime;
  const armral_ldpc_graph_t bg = p->BG == 1 ? LDPC_BASE_GRAPH_1 : LDPC_BASE_GRAPH_2;

  /* ArmRAL's input starts after the punctured columns */
  const uint32_t punct = NUM_PUNCTURED_COLS * Z;
  int8_t *llr = p_llr + punct;
  const uint32_t n = num_cols(p->BG, p->R) * Z - punct;
  const uint32_t filler_start = Kprime > punct ? Kprime - punct : 0;
  normalize_llr(llr, n, filler_start, K - punct - filler_start, Kprime);

  uint32_t options = ARMRAL_LDPC_FILLER_BITS_EXPLICIT;
  if (p->check_crc)
    options |= armral_crc_option(p->crc_type) | ARMRAL_LDPC_CRC_EVERY_ITER | ARMRAL_LDPC_PARITY_CHECK_DISABLE;
  else
    options |= ARMRAL_LDPC_CRC_NO | ARMRAL_LDPC_PARITY_CHECK_ENABLE;

  uint32_t its = 0;
  void *buf = get_work_buf(max_it);
  armral_status st = armral_ldpc_decode_block_noalloc(n, llr, bg, Z, nfill, p_out, max_it, &its, options, buf);
  AssertFatal(st == ARMRAL_SUCCESS || st == ARMRAL_FAIL,
              "armral_ldpc_decode_block_noalloc(): invalid arguments (BG %d Z %d n %u filler %u iterations %d)\n",
              p->BG,
              Z,
              n,
              nfill,
              max_it);

  /* With a CRC, ArmRAL reports success only if the CRC passed. Without one
   * (ldpctest), success means all parity checks passed, which counts only
   * before the last iteration. */
  bool ok = st == ARMRAL_SUCCESS;
  if (!p->check_crc)
    ok = ok && its < max_it;
  if (!ok) {
    set_abort(ab, true);
    return max_it;
  }
  /* the segment layer treats a return value >= numMaxIter as a failure:
   * report a block decoded in the last iteration as numMaxIter - 1 */
  return its < max_it ? its : max_it - 1;
}
