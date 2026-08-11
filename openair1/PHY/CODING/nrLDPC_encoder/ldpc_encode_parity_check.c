/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Parity check function used by ldpc encoders
 */

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "assertions.h"
#include "common/utils/LOG/log.h"

#define USE_PERMUTEX
#ifdef __aarch64__
#define USE_ALIGNR
// Use the factored encoders on the 128-bit alignr path.
//
// These recover the base-graph structure that the expanded generator discards:
// the four core parity groups P0..P3 are emitted once and referenced by cyclic
// rotation, instead of being pre-inverted into every row that uses them.
// BG1 2109 -> 319 terms, BG2 1473 -> 173 terms, for every supported lifting
// size, and bit-exact with the expanded encoders.
//
// Only the 128-bit alignr path has factored variants; the AVX2 / AVX512 paths
// keep the stock encoders. Comment this out to fall back everywhere.
#define USE_FACTORED
#endif
#ifdef __AVX512F__
#if defined(__AVX512VBMI__) && defined(USE_PERMUTEX)
// For AVX512 machines, use an AVX512 version of the encoder for Zc=384 only for now. This is used almost exclusively for
// high-throughput cases. The version with "permutex2var" instruction uses less memory (i.e. 1/64th of the memory to store the
// input), but uses more reads instead of creating 64 shifts of the input with memcpy
#include "ldpc384_simd512_permutex_byte.c"
#else 
#include "ldpc384_simd512_byte.c"
#endif
#endif
#ifdef USE_ALIGNR
#ifdef USE_FACTORED
#include "ldpc384_factored_byte_128.c"
#include "ldpc352_factored_byte_128.c"
#include "ldpc320_factored_byte_128.c"
#include "ldpc288_factored_byte_128.c"
#include "ldpc256_factored_byte_128.c"
#include "ldpc240_factored_byte_128.c"
#include "ldpc224_factored_byte_128.c"
#include "ldpc208_factored_byte_128.c"
#include "ldpc192_factored_byte_128.c"
#include "ldpc176_factored_byte_128.c"
#else
#include "ldpc384_alignr_byte_128.c"
#include "ldpc352_alignr_byte_128.c"
#include "ldpc320_alignr_byte_128.c"
#include "ldpc288_alignr_byte_128.c"
#include "ldpc256_alignr_byte_128.c"
#include "ldpc240_alignr_byte_128.c"
#include "ldpc224_alignr_byte_128.c"
#include "ldpc208_alignr_byte_128.c"
#include "ldpc192_alignr_byte_128.c"
#include "ldpc176_alignr_byte_128.c"
#endif
#else
#ifndef __AVX512F__
#include "ldpc384_byte.c"
#endif
#include "ldpc352_byte.c"
#include "ldpc320_byte.c"
#include "ldpc288_byte.c"
#include "ldpc256_byte.c"
#include "ldpc240_byte.c"
#include "ldpc224_byte.c"
#include "ldpc208_byte.c"
#include "ldpc192_byte.c"
#include "ldpc176_byte.c"
#include "ldpc384_byte_128.c"
#include "ldpc352_byte_128.c"
#include "ldpc320_byte_128.c"
#include "ldpc288_byte_128.c"
#include "ldpc256_byte_128.c"
#include "ldpc224_byte_128.c"
#include "ldpc192_byte_128.c"
#endif
#if defined(USE_ALIGNR) && defined(USE_FACTORED)
// BG2, 16-byte-aligned lifting sizes: factored, and on alignr rather than the
// pre-rotated input layout, so only the doubled input buffer is needed.
#include "ldpc_BG2_Zc384_factored_byte_128.c"
#include "ldpc_BG2_Zc352_factored_byte_128.c"
#include "ldpc_BG2_Zc320_factored_byte_128.c"
#include "ldpc_BG2_Zc288_factored_byte_128.c"
#include "ldpc_BG2_Zc256_factored_byte_128.c"
#include "ldpc_BG2_Zc240_factored_byte_128.c"
#include "ldpc_BG2_Zc224_factored_byte_128.c"
#include "ldpc_BG2_Zc208_factored_byte_128.c"
#include "ldpc_BG2_Zc192_factored_byte_128.c"
#include "ldpc_BG2_Zc176_factored_byte_128.c"
#include "ldpc_BG2_Zc160_factored_byte_128.c"
#include "ldpc_BG2_Zc144_factored_byte_128.c"
#include "ldpc_BG2_Zc128_factored_byte_128.c"
#include "ldpc_BG2_Zc112_factored_byte_128.c"
#include "ldpc_BG2_Zc96_factored_byte_128.c"
#include "ldpc_BG2_Zc80_factored_byte_128.c"
// Zc 72/88/104/120 are 8-byte aligned only: no factored variant, keep the stock
// 64-bit encoders. These still need the pre-rotated input layout.
#include "ldpc_BG2_Zc120_byte.c"
#include "ldpc_BG2_Zc104_byte.c"
#include "ldpc_BG2_Zc88_byte.c"
#include "ldpc_BG2_Zc72_byte.c"
#else
#include "ldpc_BG2_Zc384_byte.c"
#include "ldpc_BG2_Zc384_byte_128.c"
#include "ldpc_BG2_Zc352_byte.c"
#include "ldpc_BG2_Zc352_byte_128.c"
#include "ldpc_BG2_Zc320_byte.c"
#include "ldpc_BG2_Zc320_byte_128.c"
#include "ldpc_BG2_Zc288_byte.c"
#include "ldpc_BG2_Zc288_byte_128.c"
#include "ldpc_BG2_Zc256_byte.c"
#include "ldpc_BG2_Zc256_byte_128.c"
#include "ldpc_BG2_Zc240_byte.c"
#include "ldpc_BG2_Zc224_byte.c"
#include "ldpc_BG2_Zc224_byte_128.c"
#include "ldpc_BG2_Zc208_byte.c"
#include "ldpc_BG2_Zc192_byte.c"
#include "ldpc_BG2_Zc192_byte_128.c"
#include "ldpc_BG2_Zc176_byte.c"
#include "ldpc_BG2_Zc160_byte_128.c"
#include "ldpc_BG2_Zc160_byte.c"
#include "ldpc_BG2_Zc144_byte.c"
#include "ldpc_BG2_Zc128_byte.c"
#include "ldpc_BG2_Zc128_byte_128.c"
#include "ldpc_BG2_Zc120_byte.c"
#include "ldpc_BG2_Zc112_byte.c"
#include "ldpc_BG2_Zc104_byte.c"
#include "ldpc_BG2_Zc96_byte.c"
#include "ldpc_BG2_Zc96_byte_128.c"
#include "ldpc_BG2_Zc88_byte.c"
#include "ldpc_BG2_Zc80_byte.c"
#include "ldpc_BG2_Zc72_byte.c"
#endif

static void encode_parity_check_part_optim(uint8_t *cc, uint8_t *d, short BG, short Zc, int simd_size, int ncols)
{
  // Encoders on the alignr path index a doubled input buffer directly, so the
  // simd_size pre-rotated copies are skipped and only one copy is needed —
  // avoid a 32x/64x overallocation on the stack.
  //
  // BG1 (Zc >= 176) has always been on alignr. With the factored encoders BG2
  // is too, for the 16-byte-aligned lifting sizes; BG2 Zc 72/88/104/120 have no
  // factored variant and still need the pre-rotated layout.
#if defined(USE_ALIGNR) && defined(USE_FACTORED)
  int vla_simd = ((BG == 1 && Zc >= 176) || (BG == 2 && (Zc & 15) == 0)) ? 1 : simd_size;
#elif defined(USE_ALIGNR)
  int vla_simd = (BG == 1 && Zc >= 176) ? 1 : simd_size;
#else
  int vla_simd = simd_size;
#endif
  unsigned char c[2 * 22 * Zc * vla_simd] __attribute__((aligned(64))); //double size matrix of c
  if (tinput_memcpy)
    start_meas(tinput_memcpy);
  for (int i1 = 0; i1 < ncols; i1++)   {
    memcpy(&c[2 * i1 * Zc], &cc[i1 * Zc], Zc * sizeof(unsigned char));
    memcpy(&c[(2 * i1 + 1) * Zc], &cc[i1 * Zc], Zc * sizeof(unsigned char));
  }
#if (defined(USE_PERMUTEX) && defined(__AVX512VBMI__)) 
  if (BG == 2 || Zc < 384)
#endif
#if defined(USE_ALIGNR) && defined(USE_FACTORED)
  // only the 8-byte-aligned BG2 sizes still need the pre-rotated copies
  if (BG == 2 && (Zc & 15) != 0)
#elif defined(USE_ALIGNR)
  if (BG == 2)
#endif
  {
    for (int i1 = 1; i1 < simd_size; i1++) {
      memcpy(&c[(2 * ncols * Zc * i1)], &c[i1], (2 * ncols * Zc * sizeof(unsigned char)) - i1);
    }
  }
  if (BG == 1) {
    switch (Zc) {
      case 176:
        ldpc176_byte(c, d);
        break;
      case 192:
        ldpc192_byte(c, d);
        break;
      case 208:
        ldpc208_byte(c, d);
        break;
      case 224:
        ldpc224_byte(c, d);
        break;
      case 240:
        ldpc240_byte(c, d);
        break;
      case 256:
        ldpc256_byte(c, d);
        break;
      case 288:
        ldpc288_byte(c, d);
        break;
      case 320:
        ldpc320_byte(c, d);
        break;
      case 352:
        ldpc352_byte(c, d);
        break;
      case 384:
        ldpc384_byte(c, d);
        break;
      default:
        AssertFatal(false, "BG %d Zc %d is not supported yet\n", BG, Zc);
    }
  } else if (BG == 2) {
    switch (Zc) {
      case 72:
        ldpc_BG2_Zc72_byte(c, d);
        break;
      case 80:
        ldpc_BG2_Zc80_byte(c, d);
        break;
      case 88:
        ldpc_BG2_Zc88_byte(c, d);
        break;
      case 96:
        ldpc_BG2_Zc96_byte(c, d);
        break;
      case 104:
        ldpc_BG2_Zc104_byte(c, d);
        break;
      case 112:
        ldpc_BG2_Zc112_byte(c, d);
        break;
      case 120:
        ldpc_BG2_Zc120_byte(c, d);
        break;
      case 128:
        ldpc_BG2_Zc128_byte(c, d);
        break;
      case 144:
        ldpc_BG2_Zc144_byte(c, d);
        break;
      case 160:
        ldpc_BG2_Zc160_byte(c, d);
        break;
      case 176:
        ldpc_BG2_Zc176_byte(c, d);
        break;
      case 192:
        ldpc_BG2_Zc192_byte(c, d);
        break;
      case 208:
        ldpc_BG2_Zc208_byte(c, d);
        break;
      case 224:
        ldpc_BG2_Zc224_byte(c, d);
        break;
      case 240:
        ldpc_BG2_Zc240_byte(c, d);
        break;
      case 256:
        ldpc_BG2_Zc256_byte(c, d);
        break;
      case 288:
        ldpc_BG2_Zc288_byte(c, d);
        break;
      case 320:
        ldpc_BG2_Zc320_byte(c, d);
        break;
      case 352:
        ldpc_BG2_Zc352_byte(c, d);
        break;
      case 384:
        ldpc_BG2_Zc384_byte(c, d);
        break;
      default:
        AssertFatal(false , "BG %d Zc %d is not supported yet\n", BG, Zc);
    }
  } else
    AssertFatal(false, "BG %d is not supported\n", BG);
}
