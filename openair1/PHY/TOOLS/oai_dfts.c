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