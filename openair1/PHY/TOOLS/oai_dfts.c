/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * Fixed-point (Q15, c16_t) DFT/IDFT for the sizes listed in tools_defs.h,
 * x86 AVX2 implementation. Every transform is unitary: the output is the
 * DFT scaled by 1/sqrt(N), so output and input have the same rms.
 *
 * Structure: dft_contiguous_c16_scaled() dispatches each size to a mixed-radix
 * kernel (a radix-R parent stage on eight lanes, then leaf DFTs); powers of
 * two use radix-8/16 stages, and three times a power of two adds a radix-3
 * parent.
 *
 * Scaling rule: a raw (unscaled) butterfly may only run on data whose rms has
 * not grown by more than about sqrt(2) since the last scaling. Composite
 * parents therefore scale between their sub-butterflies and put only the
 * remaining factor into the parent twiddles, so uniform input up to about
 * +/-4096 per component (and, for most sizes, +/-8000) does not saturate.
 */

#if defined(__x86_64__) || defined(__i386__)
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <stddef.h>
#include "../sse_intrin.h"
#include "assertions.h"
#define OAIDFTS_MAIN
#include "tools_defs.h"
#include <pthread.h>

#define OAI_DFTS_INTERNAL
#include "oai_dfts_common.h"
#include "oai_dfts_prune.h"
#include "oai_dfts_pow2.h"
#include "oai_dfts_radix9.h"
#include "oai_dfts_mixed.h"
#include "oai_dfts_radix25.h"
#include "oai_dfts_large.h"
#undef OAI_DFTS_INTERNAL

/* Sizes that gain from it are compiled as one flattened function per direction,
 * so every kernel and leaf is specialised for that direction. */
static __attribute__((always_inline)) inline void dft_small_c16_scaled(const c16_t *src, c16_t *dst, int N, dft_dir_t dir)
{
  switch (N) {
    case 96:
      dft96_radix12_pfa_leaf8(src, dst, dir);
      return;
    case 108:
      dft108_radix27_leaf4(src, dst, dir);
      return;
    case 120:
      dft120_radix15_pfa(src, dst, dir);
      return;
    case 128:
      dft128_dir(src, dst, dir);
      return;
    case 144:
      dft144_radix18_leaf8(src, dst, dir);
      return;
    case 216:
      dft216_radix27_leaf8(src, dst, dir);
      return;
    case 324:
      dft324_radix27_leaf12(src, dst, dir);
      return;
    case 360:
      dft360_radix30_leaf12_twscaled(src, dst, dir);
      return;
    case 432:
      dft432_radix27_leaf16(src, dst, dir);
      return;
    case 540:
      dft540_r5_leaf108_pair_batch2(src, dst, dir);
      return;
    case 864:
      dft864_radix27_leaf32(src, dst, dir);
      return;
    case 972:
      radix81_terminal_leaf12_972(src, dst, dir, 1);
      return;
    case 1080:
      dft1080_radix5_leaf216(src, dst, dir);
      return;
    case 1296:
      radix81_terminal_leaf16_w16folded(src, dst, dir);
      return;
    case 1800:
      dft1800_radix25_pack4(src, dst, dir);
      return;
    default:
      AssertFatal(0, "Unsupported small DFT size N=%d\n", N);
  }
}

static __attribute__((flatten, noinline)) void dft_small_forward(const c16_t *src, c16_t *dst, int N)
{
  dft_small_c16_scaled(src, dst, N, DFT_DIR_FORWARD);
}

static __attribute__((flatten, noinline)) void dft_small_inverse(const c16_t *src, c16_t *dst, int N)
{
  dft_small_c16_scaled(src, dst, N, DFT_DIR_INVERSE);
}

static __attribute__((always_inline)) inline void dft_contiguous_c16_scaled(const c16_t *src, c16_t *dst, int N, dft_dir_t dir)
{
  switch (N) {
    case 96:
    case 108:
    case 120:
    case 128:
    case 144:
    case 216:
    case 324:
    case 360:
    case 432:
    case 540:
    case 864:
    case 972:
    case 1080:
    case 1296:
    case 1800:
      if (dir == DFT_DIR_FORWARD)
        dft_small_forward(src, dst, N);
      else
        dft_small_inverse(src, dst, N);
      return;
    case 12:
      dft12_q15_128(src, dst, dir);
      return;
    case 16:
      dft16_q15_128(src, dst, dir);
      return;
    case 24:
      dft24_pfa3x8_q15_128(src, dst, dir);
      return;
    case 32:
      dft32_q15_128(src, dst, dir);
      return;
    case 36:
      dft36_radix9_leaf4(src, dst, dir);
      return;
    case 48:
      dft48_radix8_pfa6(src, dst, dir);
      return;
    case 60:
      dft60_radix15_pfa_leaf4(src, dst, dir);
      return;
    case 64:
      dft64_avx(src, dst, dir);
      return;
    case 72:
      dft72_radix9_leaf8_batch8(src, dst, dir);
      return;
    case 180:
      dft180_radix15_pfa_leaf12(src, dst, dir);
      return;
    case 192:
      dft192_radix12_pfa_leaf16(src, dst, dir);
      return;
    case 240:
      dft240_radix15_pfa(src, dst, dir);
      return;
    case 288:
      radix9_terminal_leaf32(src, dst, dir);
      return;
    case 300:
      dft300_radix25_dft12x8_block_major(src, dst, dir);
      return;
    case 384:
      dft384_radix24_pfa(src, dst, dir);
      return;
    case 480:
      dft480_radix30_pfa(src, dst, dir);
      return;
    case 576:
      dft_radix9_or_radix18(src, dst, N, dir, 1);
      return;
    case 600:
      dft600_radix30_leaf20_twscaled(src, dst, dir);
      return;
    case 648:
      radix81_terminal_leaf8(src, dst, dir, 1);
      return;
    case 720:
      dft720_radix30_leaf24_twscaled(src, dst, dir);
      return;
    case 768:
      dft768_radix24_pfa_leaf32(src, dst, dir);
      return;
    case 900:
      dft900_radix25_dft36_pair_batch2(src, dst, dir);
      return;
    case 960:
      dft960_radix30_pfa(src, dst, dir);
      return;
    case 1152:
      dft_radix9_or_radix18(src, dst, N, dir, 1);
      return;
    case 1200:
      dft1200_radix3_r25_avx2(src, dst, dir);
      return;
    case 1440:
      dft1440_radix3_leaf480(src, dst, dir);
      return;
    case 1500:
      dft1500_radix25_dft60x8(src, dst, dir);
      return;
    case 1536:
      dft1536_radix24_pfa_leaf64(src, dst, dir);
      return;
    case 1620:
      dft1620_radix5_leaf324(src, dst, dir);
      return;
    case 1728:
      dft1728_radix27_leaf64(src, dst, dir);
      return;
    case 1920:
      dft1920_radix30_pfa(src, dst, dir);
      return;
    case 1944:
      dft1944_radix3_leaf648(src, dst, dir);
      return;
    case 2160:
      dft2160_radix30_leaf72x8(src, dst, dir);
      return;
    case 2304:
      dft_radix9_or_radix18(src, dst, N, dir, 1);
      return;
    case 2400:
      dft2400_radix3_r25_avx2(src, dst, dir);
      return;
    case 2592:
      radix81_terminal_leaf32_2592(src, dst, dir);
      return;
    case 2700:
      dft2700_radix25_pack4(src, dst, dir);
      return;
    case 2880:
      dft2880_r5_leaf576(src, dst, dir);
      return;
    case 2916:
      dft2916_radix3_leaf972(src, dst, dir);
      return;
    case 3000:
      dft3000_radix25_pack4(src, dst, dir);
      return;
    case 3072:
      dft3072_r24_dft128x8(src, dst, dir);
      return;
    case 3240:
      dft3240_radix5_leaf648(src, dst, dir);
      return;
    case 6144:
      dft6144_r24_leaf256(src, dst, dir);
      return;
    case 12288:
      dft12288_r24_leaf512(src, dst, dir);
      return;
    case 18432:
      dft18432_radix3_leaf6144(src, dst, dir);
      return;
    case 36864:
      dft36864_radix3_leaf12288(src, dst, dir);
      return;
    default:
      break;
  }

  /* Remaining exported sizes are powers of two or three times a power of two. */
  if (is_power_of_two_int(N)) {
    dft_power2_mixed_large_q15(src, dst, N, dir);
    return;
  }

  AssertFatal(N % 3 == 0 && is_power_of_two_int(N / 3), "Unsupported DFT size N=%d\n", N);
  dft_radix3_pow2(src, dst, N, dir);
}
/* The small inverse transforms share the hot section and alignment of dft12/dft16. */
OAI_DFT_SMALL_HOT void idft12(int16_t *input, int16_t *output, uint8_t scale_flag);
OAI_DFT_SMALL_HOT void idft16(int16_t *input, int16_t *output, uint8_t scale_flag);

#define DEFINE_MIXED_TRANSFORM(PREFIX, N, DIR)                                \
  void PREFIX##N(int16_t *input, int16_t *output, uint8_t scale_flag)         \
  {                                                                           \
    (void)scale_flag;                                                         \
    dft_contiguous_c16_scaled((const c16_t *)input, (c16_t *)output, N, DIR); \
  }
#define DEFINE_MIXED_DFT(N) DEFINE_MIXED_TRANSFORM(dft, N, DFT_DIR_FORWARD)
#define DEFINE_MIXED_IDFT(N) DEFINE_MIXED_TRANSFORM(idft, N, DFT_DIR_INVERSE)

/* Export the sizes used by the function tables in tools_defs.h. */
FOREACH_DFTSZ(DEFINE_MIXED_DFT)
FOREACH_IDFTSZ(DEFINE_MIXED_IDFT)

#undef DEFINE_MIXED_DFT
#undef DEFINE_MIXED_IDFT
#undef DEFINE_MIXED_TRANSFORM

void dft_implementation(uint8_t sizeidx, int16_t *input, int16_t *output, unsigned char scale_flag)
{
  AssertFatal(sizeidx < DFT_SIZE_IDXTABLESIZE, "Invalid dft size index %i\n", sizeidx);
  const int algn = (dft_ftab[sizeidx].size % 3) != 0 ? 0x1F : 0xF;
  AssertFatal(((intptr_t)output & algn) == 0, "Buffers should be aligned %p", (void *)output);
  /* the kernels load their input unaligned: no copy of a misaligned input */
  AssertFatal(((intptr_t)input & 3) == 0, "DFT input should be c16_t aligned %p", (void *)input);
  dft_ftab[sizeidx].func(input, output, scale_flag);
}

void idft_implementation(uint8_t sizeidx, int16_t *input, int16_t *output, unsigned char scale_flag)
{
  AssertFatal(sizeidx < IDFT_SIZE_IDXTABLESIZE, "Invalid idft size index %i\n", sizeidx);
  AssertFatal(((intptr_t)output & 0x1F) == 0, "Buffers should be 32-byte aligned %p", (void *)output);
  AssertFatal(((intptr_t)input & 3) == 0, "IDFT input should be c16_t aligned %p", (void *)input);
  idft_ftab[sizeidx].func(input, output, scale_flag);
}

/* Pruned interface (see tools_defs.h): only the band of num_used bins starting at first_used
 * (natural order, modulo N) is used. layout DFT_PRUNE_NATURAL keeps bin k at index k,
 * DFT_PRUNE_PACKED puts bin (first_used + j) mod N at index j.
 * NR band (packed, first_used = N - num_used / 2): the kernels of the full transform with the halves of the
 * spectrum swapped and the band offset applied in their output stores (DFT) or input loads (IDFT), see
 * oai_dfts_prune.h. Other bands and sizes without a kernel: full transform and a copy. */
static __attribute__((noinline)) void dft384_band_forward(const c16_t *src, c16_t *out, int d, c16_t *edge_buf)
{
  const dft_band_t band = dft_band_make(NULL, out, d, edge_buf);
  const dft_band_t *pout = &band;
  c16_t b[24 * 16] __attribute__((aligned(64)));
  radix24_pfa_stage_384(src, NULL, b, DFT_DIR_FORWARD);
  for (int first = 0; first < 24; first += 8)
    dft16x8_twiddle_scaled_store_body(b, 16, pout->out, 24, first, 8, pout, DFT_DIR_FORWARD);
}

static __attribute__((noinline)) void dft512_band_forward(const c16_t *src, c16_t *out, int d, c16_t *edge_buf)
{
  const dft_band_t band = dft_band_make(NULL, out, d, edge_buf);
  const dft_band_t *pout = &band;
  simde__m256i leaf[64] __attribute__((aligned(64)));
  dft512_radix8_leaf_inputs(src, NULL, leaf, DFT_DIR_FORWARD);
  dft64x8_store_body(leaf, pout->out, 8, 0, pout, DFT_DIR_FORWARD);
}

static __attribute__((noinline)) void dft768_band_forward(const c16_t *src, c16_t *out, int d, c16_t *edge_buf)
{
  const dft_band_t band = dft_band_make(NULL, out, d, edge_buf);
  const dft_band_t *pout = &band;
  c16_t b[24 * 32] __attribute__((aligned(64)));
  radix24_pfa_stage_768(src, NULL, b, DFT_DIR_FORWARD);
  for (int first = 0; first < 24; first += 8)
    dft32x8_twiddle_scaled_store_body(b, 32, pout->out, 24, first, 8, pout, DFT_DIR_FORWARD);
}

static __attribute__((noinline)) void dft1024_band_forward(const c16_t *src, c16_t *out, int d, c16_t *edge_buf)
{
  const dft_band_t band = dft_band_make(NULL, out, d, edge_buf);
  const dft_band_t *pout = &band;
  simde__m256i leaf[2][64] __attribute__((aligned(64)));
  dft1024_radix16_leaf_inputs(src, NULL, leaf, DFT_DIR_FORWARD);
  dft64x8_store_body(leaf[0], pout->out, 16, 0, pout, DFT_DIR_FORWARD);
  dft64x8_store_body(leaf[1], pout->out, 16, 8, pout, DFT_DIR_FORWARD);
}

static __attribute__((noinline)) void dft1536_band_forward(const c16_t *src, c16_t *out, int d, c16_t *edge_buf)
{
  const dft_band_t band = dft_band_make(NULL, out, d, edge_buf);
  const dft_band_t *pout = &band;
  c16_t b[24 * 64] __attribute__((aligned(64)));
  radix24_stage_1536(src, b, DFT_DIR_FORWARD);
  for (int first = 0; first < 24; first += 8)
    dft64x8_branch_major_store_band(b, 64, pout, 24, first);
}

static __attribute__((noinline)) void dft2048_band_forward(const c16_t *src, c16_t *out, int d, c16_t *edge_buf)
{
  const dft_band_t band = dft_band_make(NULL, out, d, edge_buf);
  const dft_band_t *pout = &band;
  simde__m256i child_in[16][16] __attribute__((aligned(64)));
  dft2048_radix8x16_leaf_inputs(src, NULL, child_in, DFT_DIR_FORWARD);
  for (int r16 = 0; r16 < 16; r16++) {
    simde__m256i y[16];
    dft16x8_w16folded(child_in[r16], y, DFT_DIR_FORWARD);
    for (int k = 0; k < 16; k++) /* natural bin 128 k + 8 r16 is at swapped index 128 (k ^ 8) + 8 r16 */
      dft_band_store8(pout, 2048, 128 * (k ^ 8) + 8 * r16, k < 8, y[k]);
  }
}

static __attribute__((noinline)) void dft3072_band_forward(const c16_t *src, c16_t *out, int d, c16_t *edge_buf)
{
  const dft_band_t band = dft_band_make(NULL, out, d, edge_buf);
  const dft_band_t *pout = &band;
  simde__m256i work[3][2][64] __attribute__((aligned(64)));
  dft3072_r24_r2_to_batches(src, NULL, work, DFT_DIR_FORWARD);
  for (int group = 0; group < 3; group++) {
    dft64x8_store_with_dc_band_body(work[group][0], pout->out, 48, 8 * group, pout, DFT_DIR_FORWARD);
    dft64x8_store_with_dc_band_body(work[group][1], pout->out, 48, 24 + 8 * group, pout, DFT_DIR_FORWARD);
  }
}

static __attribute__((noinline)) void dft4096_band_forward(const c16_t *src, c16_t *out, int d, c16_t *edge_buf)
{
  const dft_band_t band = dft_band_make(NULL, out, d, edge_buf);
  const dft_band_t *pout = &band;
  simde__m256i leaf_in[8][64] __attribute__((aligned(64)));
  dft4096_radix8x8_leaf_inputs(src, NULL, leaf_in, DFT_DIR_FORWARD);
  for (int r = 0; r < 8; r++)
    dft64x8_store_body(leaf_in[r], pout->out, 64, 8 * r, pout, DFT_DIR_FORWARD);
}

static __attribute__((noinline)) void dft6144_band_forward(const c16_t *src, c16_t *out, int d, c16_t *edge_buf)
{
  const dft_band_t band = dft_band_make(NULL, out, d, edge_buf);
  const dft_band_t *pout = &band;
  simde__m256i work[3][16][16] __attribute__((aligned(64)));
  dft6144_r24_parent_to_batches(src, NULL, work, DFT_DIR_FORWARD);
  for (int group = 0; group < 3; group++)
    dft6144_dft256x8_store(work[group], pout->out, 8 * group, pout, DFT_DIR_FORWARD);
}

static __attribute__((noinline)) void dft384_band_inverse(const c16_t *in, c16_t *dst, int d, c16_t *edge_buf)
{
  const dft_band_t band = dft_band_make(in, NULL, d, edge_buf);
  c16_t b[24 * 16] __attribute__((aligned(64)));
  radix24_pfa_stage_384(NULL, &band, b, DFT_DIR_INVERSE);
  for (int first = 0; first < 24; first += 8)
    dft16x8_twiddle_scaled_store(b, 16, dst, 24, first, 8, DFT_DIR_INVERSE);
}

static __attribute__((noinline)) void dft512_band_inverse(const c16_t *in, c16_t *dst, int d, c16_t *edge_buf)
{
  const dft_band_t band = dft_band_make(in, NULL, d, edge_buf);
  const dft_band_t *pin = &band;
  simde__m256i leaf[64] __attribute__((aligned(64)));
  dft512_radix8_leaf_inputs(NULL, pin, leaf, DFT_DIR_INVERSE);
  dft64x8_store(leaf, dst, 8, 0, DFT_DIR_INVERSE);
}

static __attribute__((noinline)) void dft1024_band_inverse(const c16_t *in, c16_t *dst, int d, c16_t *edge_buf)
{
  const dft_band_t band = dft_band_make(in, NULL, d, edge_buf);
  const dft_band_t *pin = &band;
  simde__m256i leaf[2][64] __attribute__((aligned(64)));
  dft1024_radix16_leaf_inputs(NULL, pin, leaf, DFT_DIR_INVERSE);
  dft64x8_store(leaf[0], dst, 16, 0, DFT_DIR_INVERSE);
  dft64x8_store(leaf[1], dst, 16, 8, DFT_DIR_INVERSE);
}

static __attribute__((noinline)) void dft2048_band_inverse(const c16_t *in, c16_t *dst, int d, c16_t *edge_buf)
{
  const dft_band_t band = dft_band_make(in, NULL, d, edge_buf);
  const dft_band_t *pin = &band;
  simde__m256i child_in[16][16] __attribute__((aligned(64)));
  dft2048_radix8x16_leaf_inputs(NULL, pin, child_in, DFT_DIR_INVERSE);
  for (int r16 = 0; r16 < 16; r16++) {
    simde__m256i y[16];
    dft16x8_w16folded(child_in[r16], y, DFT_DIR_INVERSE);
    for (int k = 0; k < 16; k++)
      simde_mm256_storeu_si256((simde__m256i *)(dst + 128 * k + 8 * r16), y[k]);
  }
}

static __attribute__((noinline)) void dft4096_band_inverse(const c16_t *in, c16_t *dst, int d, c16_t *edge_buf)
{
  const dft_band_t band = dft_band_make(in, NULL, d, edge_buf);
  const dft_band_t *pin = &band;
  simde__m256i leaf_in[8][64] __attribute__((aligned(64)));
  dft4096_radix8x8_leaf_inputs(NULL, pin, leaf_in, DFT_DIR_INVERSE);
  for (int r = 0; r < 8; r++)
    dft64x8_store(leaf_in[r], dst, 64, 8 * r, DFT_DIR_INVERSE);
}

static __attribute__((noinline)) void dft768_band_inverse(const c16_t *in, c16_t *dst, int d, c16_t *edge_buf)
{
  const dft_band_t band = dft_band_make(in, NULL, d, edge_buf);
  c16_t b[24 * 32] __attribute__((aligned(64)));
  radix24_pfa_stage_768(NULL, &band, b, DFT_DIR_INVERSE);
  for (int first = 0; first < 24; first += 8)
    dft32x8_twiddle_scaled_store(b, 32, dst, 24, first, 8, DFT_DIR_INVERSE);
}

static __attribute__((noinline)) void dft1536_band_inverse(const c16_t *in, c16_t *dst, int d, c16_t *edge_buf)
{
  const dft_band_t band = dft_band_make(in, NULL, d, edge_buf);
  c16_t b[24 * 64] __attribute__((aligned(64)));
  radix24_stage_1536_band(&band, b, DFT_DIR_INVERSE);
  for (int first = 0; first < 24; first += 8)
    dft64x8_branch_major_store(b, 64, dst, 24, first, DFT_DIR_INVERSE);
}

static __attribute__((noinline)) void dft3072_band_inverse(const c16_t *in, c16_t *dst, int d, c16_t *edge_buf)
{
  const dft_band_t band = dft_band_make(in, NULL, d, edge_buf);
  simde__m256i work[3][2][64] __attribute__((aligned(64)));
  dft3072_r24_r2_to_batches(NULL, &band, work, DFT_DIR_INVERSE);
  for (int group = 0; group < 3; group++)
    dft3072_dft64x8_pair_store(work[group], dst, 8 * group, DFT_DIR_INVERSE);
}

static __attribute__((noinline)) void dft6144_band_inverse(const c16_t *in, c16_t *dst, int d, c16_t *edge_buf)
{
  const dft_band_t band = dft_band_make(in, NULL, d, edge_buf);
  simde__m256i work[3][16][16] __attribute__((aligned(64)));
  dft6144_r24_parent_to_batches(NULL, &band, work, DFT_DIR_INVERSE);
  for (int group = 0; group < 3; group++)
    dft6144_dft256x8_store(work[group], dst, 8 * group, NULL, DFT_DIR_INVERSE);
}

/* The band is passed as scalars (registers) and rebuilt in the kernel: a struct argument is copied through the
 * stack, and reading it back as wide loads from narrower stores stalls store forwarding. */
typedef void (*dft_band_forward_t)(const c16_t *src, c16_t *out, int d, c16_t *edge_buf);
typedef void (*dft_band_inverse_t)(const c16_t *in, c16_t *dst, int d, c16_t *edge_buf);

/* Kernel of a size for an NR band, or NULL. */
static dft_band_forward_t dft_band_forward_kernel(int N)
{
  switch (N) {
    case 384:
      return dft384_band_forward;
    case 512:
      return dft512_band_forward;
    case 768:
      return dft768_band_forward;
    case 1024:
      return dft1024_band_forward;
    case 1536:
      return dft1536_band_forward;
    case 2048:
      return dft2048_band_forward;
    case 3072:
      return dft3072_band_forward;
    case 4096:
      return dft4096_band_forward;
    case 6144:
      return dft6144_band_forward;
    default:
      return NULL;
  }
}

static dft_band_inverse_t dft_band_inverse_kernel(int N)
{
  switch (N) {
    case 384:
      return dft384_band_inverse;
    case 512:
      return dft512_band_inverse;
    case 768:
      return dft768_band_inverse;
    case 1024:
      return dft1024_band_inverse;
    case 1536:
      return dft1536_band_inverse;
    case 2048:
      return dft2048_band_inverse;
    case 3072:
      return dft3072_band_inverse;
    case 4096:
      return dft4096_band_inverse;
    case 6144:
      return dft6144_band_inverse;
    default:
      return NULL;
  }
}

static void dft_prune_check(int N, int first_used, int num_used, int layout)
{
  AssertFatal(first_used >= 0 && first_used < N && num_used > 0 && num_used <= N,
              "Invalid pruned DFT band: N %d first %d num %d\n",
              N,
              first_used,
              num_used);
  AssertFatal(layout == DFT_PRUNE_NATURAL || layout == DFT_PRUNE_PACKED, "Invalid pruned DFT layout %d\n", layout);
}

/* The NR band of the kernels: packed, centred on bin 0. */
static int dft_prune_is_nr_band(int N, int first_used, int num_used, int layout)
{
  return layout == DFT_PRUNE_PACKED && !(num_used & 1) && first_used == N - num_used / 2;
}

void dft_pruned_implementation(uint8_t sizeidx, int16_t *input, int16_t *output, int first_used, int num_used, int layout)
{
  AssertFatal(sizeidx < DFT_SIZE_IDXTABLESIZE, "Invalid dft size index %i\n", sizeidx);
  const int N = dft_ftab[sizeidx].size;
  dft_prune_check(N, first_used, num_used, layout);
  if (layout == DFT_PRUNE_NATURAL) {
    dft_implementation(sizeidx, input, output, 1);
    return;
  }
  const dft_band_forward_t kernel = dft_prune_is_nr_band(N, first_used, num_used, layout) ? dft_band_forward_kernel(N) : NULL;
  if (kernel) {
    c16_t edge[8] __attribute__((aligned(32)));
    const int d = (N - num_used) / 2;
    kernel((const c16_t *)input, (c16_t *)output, d, edge);
    dft_band_finish((c16_t *)output, N, d, edge);
    return;
  }
  /* No kernel: full transform into a work buffer, then rotate it into the output. */
  c16_t nat[N] __attribute__((aligned(32)));
  dft_implementation(sizeidx, input, (int16_t *)nat, 1);
  memcpy(output, nat + first_used, sizeof(c16_t) * (N - first_used));
  memcpy((c16_t *)output + N - first_used, nat, sizeof(c16_t) * first_used);
}

void idft_pruned_implementation(uint8_t sizeidx, int16_t *input, int16_t *output, int first_used, int num_used, int layout)
{
  AssertFatal(sizeidx < IDFT_SIZE_IDXTABLESIZE, "Invalid idft size index %i\n", sizeidx);
  const int N = idft_ftab[sizeidx].size;
  dft_prune_check(N, first_used, num_used, layout);
  AssertFatal(((intptr_t)output & 0x1F) == 0, "Buffers should be 32-byte aligned %p", (void *)output);
  const dft_band_inverse_t kernel = dft_prune_is_nr_band(N, first_used, num_used, layout) ? dft_band_inverse_kernel(N) : NULL;
  if (kernel) {
    c16_t edge[8] __attribute__((aligned(32)));
    const int d = (N - num_used) / 2;
    dft_band_edge_load((const c16_t *)input, d, edge);
    kernel((const c16_t *)input, (c16_t *)output, d, edge);
    return;
  }
  /* No kernel: gather the band into a natural-order buffer with zero guard bins. */
  const int packed = layout == DFT_PRUNE_PACKED;
  c16_t nat[N] __attribute__((aligned(32)));
  const c16_t *in = (const c16_t *)input;
  const int n1 = first_used + num_used <= N ? num_used : N - first_used; /* bins first_used .. */
  const int n2 = num_used - n1; /* bins 0 .. n2-1 */
  memcpy(nat + first_used, in + (packed ? 0 : first_used), sizeof(c16_t) * n1);
  memcpy(nat, in + (packed ? n1 : 0), sizeof(c16_t) * n2);
  if (n2) /* guard = [n2, first_used) */
    memset(nat + n2, 0, sizeof(c16_t) * (first_used - n2));
  else { /* guard = [first_used + num_used, N) and [0, first_used) */
    memset(nat + first_used + num_used, 0, sizeof(c16_t) * (N - first_used - num_used));
    memset(nat, 0, sizeof(c16_t) * first_used);
  }
  idft_ftab[sizeidx].func((int16_t *)nat, output, 1);
}
#undef DFT_BIND_R3_TWIDDLES
#undef DFT_BIND_R5_TWIDDLES
#undef DFT_R3_COEFFICIENTS_128
#undef DFT_R3_COEFFICIENTS_256
#undef DFT_R5_COEFFICIENTS_128
#undef DFT_R5_COEFFICIENTS_256
#undef DFT_R5_COEFFICIENTS_BROADCAST256

#undef DFT_R25_LOAD_CONTIGUOUS_128
#undef DFT_R25_LOAD_CONTIGUOUS_256
#undef DFT_R25_LOAD_BLOCK_MAJOR
#undef DFT_R25_LOAD_NONE
#undef DFT_R25_INPUT_REGISTERS
#undef DFT_R25_INPUT_VECTORS
#undef DFT_R25_INPUT_TAIL
#undef DFT_RADIX25_TILE
#undef DFT64X8_EXACT_DC_STAGE
#undef DFT_R9_FIRST_STAGE_256
#undef DFT_R9_SECOND_STAGE_256

#undef DFT12_SCALAR_FINISH
#undef DFT_R9_VECTOR_LOOP
#undef DFT_R9_SINGLE_VECTOR
#undef DEFINE_R9_HYBRID_PARENT
#undef DEFINE_DFT4_LEAF_STORE
#undef DFT_R3_FOLDED_LEAF_STAGE
#undef DFT12_FOLDED_SECOND_STAGE
#undef DFT_R16_SPLIT_CHILDREN
#undef DFT_RADIX81_PARENT_STAGES
#undef DFT_RADIX81_CHILD_VIEWS

#undef DFT_LOAD_TRANSPOSE8_ROW
#undef DFT_TRANSPOSE8_TO_ROW
#undef DFT_STORE8_STRIDED_ROWS
#undef DFT_TRANSPOSE_CHILD_COLUMN
#undef DFT_STORE_R30_FREQUENCY_TILE

#undef DFT_R5_SCALE_256
#undef DFT_R5_ACCUMULATE_LEFT
#undef DFT_R5_ACCUMULATE_RIGHT
#undef DEFINE_RADIX5_COMBINE

#undef DEFINE_Q15_TABLE_INITIALIZER

#undef DEFINE_COMPLEX_TRANSPOSE4
#undef DFT_R15_PARTIAL_STAGE
#undef DFT72_NORMALIZED_R9_STAGE
#undef DFT_R9_FIRST_STAGE_128
#undef DFT_R9_SECOND_STAGE_128

#undef DEFINE_SCALED_TWIDDLE_PACKERS
#undef DEFINE_UNITARY_DFT4
#undef DEFINE_UNITARY_DFT8
#undef DFT8_POINTER_OUTPUTS
#undef DFT8_ARRAY_OUTPUTS
#undef DFT8_POINTER
#undef DFT8_ARRAY
#undef DFT8_SCALE_128
#undef DFT8_SCALE_256
#undef DFT_R30_FULL_INPUT
#undef DFT_R30_PARTIAL_INPUT
#undef DFT_R30_EIGHT_INPUT
#undef DFT_R30_LOAD_SIX
#undef DFT_R30_INPUT_PERMUTATION
#undef DFT_R30_OUTPUT_PERMUTATION
#undef DFT_R30_COMBINE_FIVE
#undef DFT_R30_RADIX5
#undef DEFINE_PACKED_PFA_PARENT
#undef DEFINE_R3_R25_PARENT
#undef DEFINE_R30_PADDED_LEAF_PARENT
#undef DFT_R9_VECTOR_INPUT_STAGE
#undef DFT_R9_ALIGNED_PARENT_BODY
#undef DFT_DUPLICATED_BYTE_LANES
#undef DEFINE_PREPACKED_COMPLEX_MULTIPLY
#undef DEFINE_COMPLEX_J_ROTATION
#undef DEFINE_RAW_RADIX3_BUTTERFLY
#undef DFT_FOUR_LEAF_BATCHES
#undef DEFINE_COMPONENT_TWIDDLE_CREATE
#undef DEFINE_COMPONENT_TWIDDLE_ENSURE_Q15
#undef DEFINE_COMPONENT_TWIDDLE_GET
#undef DFT_COMPONENT_TWO
#undef DFT_COMPONENT_FOUR
#undef DFT_COMPONENT_FORWARD
#undef DFT_COMPONENT_INVERSE
#undef DFT_SELECTED_CHILD_TABLES
#undef DFT8_FOLDED_LEAF_STAGES
#undef DEFINE_SELECTED_DFT16X8
#undef DEFINE_DFT64X8_DC_STORE
#undef DFT_DC_STORE_FULL
#undef DFT_DC_STORE_MASKED
#undef DFT_DC_MASK_PARAM
#undef DFT_FUSED_R8_PARENT_WORKSPACE
#undef DFT_R16_LOAD_SPLIT
#undef DFT_R16_W16_ALWAYS
#undef DFT_R16_W16_NONZERO
#undef DEFINE_STRIDED_DFT12_PARENT
#undef DFT_PACK_FOUR_STRIDED_CHILDREN
#endif