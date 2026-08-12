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

#if defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
#endif

#define USE_PERMUTEX
#ifdef __aarch64__
#define USE_ALIGNR
#endif

// Use the factored encoders.
//
// These recover the base-graph structure that the expanded generator discards:
// the four core parity groups P0..P3 are emitted once and referenced by cyclic
// rotation, instead of being pre-inverted into every row that uses them.
// BG1 2109 -> 319 terms, BG2 1473 -> 173 terms, for every supported lifting
// size, and bit-exact with the expanded encoders.
//
// They exist at 128 bit everywhere, and additionally at 256/512 bit on x86 for
// the lifting sizes that divide evenly. The 128-bit set is the fallback.
// Define NO_FACTORED to fall back to the stock encoders entirely.
#ifndef NO_FACTORED
#define USE_FACTORED
#ifdef __AVX2__
#define LDPC_HAVE_256
#endif
#ifdef __AVX512F__
#define LDPC_HAVE_512
#endif
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
#ifdef USE_FACTORED
// 128-bit: available for every supported lifting size
#include "ldpc176_factored_byte_128.c"
#include "ldpc192_factored_byte_128.c"
#include "ldpc208_factored_byte_128.c"
#include "ldpc224_factored_byte_128.c"
#include "ldpc240_factored_byte_128.c"
#include "ldpc256_factored_byte_128.c"
#include "ldpc288_factored_byte_128.c"
#include "ldpc320_factored_byte_128.c"
#include "ldpc352_factored_byte_128.c"
#include "ldpc384_factored_byte_128.c"
#include "ldpc_BG2_Zc80_factored_byte_128.c"
#include "ldpc_BG2_Zc96_factored_byte_128.c"
#include "ldpc_BG2_Zc112_factored_byte_128.c"
#include "ldpc_BG2_Zc128_factored_byte_128.c"
#include "ldpc_BG2_Zc144_factored_byte_128.c"
#include "ldpc_BG2_Zc160_factored_byte_128.c"
#include "ldpc_BG2_Zc176_factored_byte_128.c"
#include "ldpc_BG2_Zc192_factored_byte_128.c"
#include "ldpc_BG2_Zc208_factored_byte_128.c"
#include "ldpc_BG2_Zc224_factored_byte_128.c"
#include "ldpc_BG2_Zc240_factored_byte_128.c"
#include "ldpc_BG2_Zc256_factored_byte_128.c"
#include "ldpc_BG2_Zc288_factored_byte_128.c"
#include "ldpc_BG2_Zc320_factored_byte_128.c"
#include "ldpc_BG2_Zc352_factored_byte_128.c"
#include "ldpc_BG2_Zc384_factored_byte_128.c"
// Zc 72/88/104/120 are 8-byte aligned only: no factored variant, stock 64-bit
#include "ldpc_BG2_Zc120_byte.c"
#include "ldpc_BG2_Zc104_byte.c"
#include "ldpc_BG2_Zc88_byte.c"
#include "ldpc_BG2_Zc72_byte.c"
#ifdef LDPC_HAVE_256
#include "ldpc192_factored_byte_256.c"
#include "ldpc224_factored_byte_256.c"
#include "ldpc256_factored_byte_256.c"
#include "ldpc288_factored_byte_256.c"
#include "ldpc320_factored_byte_256.c"
#include "ldpc352_factored_byte_256.c"
#include "ldpc384_factored_byte_256.c"
#include "ldpc_BG2_Zc96_factored_byte_256.c"
#include "ldpc_BG2_Zc128_factored_byte_256.c"
#include "ldpc_BG2_Zc160_factored_byte_256.c"
#include "ldpc_BG2_Zc192_factored_byte_256.c"
#include "ldpc_BG2_Zc224_factored_byte_256.c"
#include "ldpc_BG2_Zc256_factored_byte_256.c"
#include "ldpc_BG2_Zc288_factored_byte_256.c"
#include "ldpc_BG2_Zc320_factored_byte_256.c"
#include "ldpc_BG2_Zc352_factored_byte_256.c"
#include "ldpc_BG2_Zc384_factored_byte_256.c"
#endif
#ifdef LDPC_HAVE_512
#include "ldpc192_factored_byte_512.c"
#include "ldpc256_factored_byte_512.c"
#include "ldpc320_factored_byte_512.c"
#include "ldpc384_factored_byte_512.c"
#include "ldpc_BG2_Zc128_factored_byte_512.c"
#include "ldpc_BG2_Zc192_factored_byte_512.c"
#include "ldpc_BG2_Zc256_factored_byte_512.c"
#include "ldpc_BG2_Zc320_factored_byte_512.c"
#include "ldpc_BG2_Zc384_factored_byte_512.c"
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

/* Which SIMD width the factored encoders should use.
 *
 * This is a microarchitecture question, not an ISA one. Zen 4, Zen 5 and
 * Sapphire Rapids all define __AVX512F__, but Zen 4 executes 512-bit ops as
 * 2x256, so AVX512 buys nothing there and measures slightly worse than AVX2.
 * Zen 5 and Sapphire Rapids have the full-width datapath and gain from it.
 *
 * Measured, BG1 Zc=384, encoder only, ns/CB:
 *
 *                                    128    256    512   -> pick
 *   EPYC 9374F  Genoa   (Zen 4)     1922    808    872      256
 *   EPYC 9575F  Turin   (Zen 5)     1516    630    438      512
 *   Xeon 6433N  Sapphire Rapids     2488   1012    827      512
 *
 * Only the ranking within a machine is meaningful above; the absolute figures
 * move with binary layout (the encoders are I-cache sensitive) and with clock.
 *
 * Set OAI_LDPC_SIMD_WIDTH=128|256|512 to override, e.g. for benchmarking.
 */
static int ldpc_simd_width(void)
{
  static int w;
  if (w)
    return w;
  const char *e = getenv("OAI_LDPC_SIMD_WIDTH");
  if (e) {
    int v = atoi(e);
    w = (v == 128 || v == 256 || v == 512) ? v : 128;
    return w;
  }
  w = 128;
#ifdef LDPC_HAVE_256
  w = 256;
#endif
#if defined(LDPC_HAVE_512) && (defined(__x86_64__) || defined(__i386__))
  unsigned eax, ebx, ecx, edx;
  char vendor[13] = {0};
  if (__get_cpuid(0, &eax, &ebx, &ecx, &edx)) {
    memcpy(vendor, &ebx, 4);
    memcpy(vendor + 4, &edx, 4);
    memcpy(vendor + 8, &ecx, 4);
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
      int family = (eax >> 8) & 0xF;
      if (family == 0xF)
        family += (eax >> 20) & 0xFF;
      if (!strcmp(vendor, "AuthenticAMD"))
        // Zen 4 (family 0x19) is double-pumped; Zen 5 (0x1A) is full width.
        // Both verified by measurement on Genoa and Turin.
        w = (family >= 0x1A) ? 512 : 256;
      else
        // AVX512-capable Intel parts have the full-width datapath.
        w = 512;
    }
  }
#endif
  return w;
}

/* Pick the widest factored encoder the lifting size and the CPU both allow.
 * SEL3 is for Zc divisible by 64, SEL2 by 32, SEL1 by 16 only. */
#if defined(LDPC_HAVE_512)
#define LDPC_SEL3(b) ((w) >= 512 ? b##_512 : (w) >= 256 ? b##_256 : b##_128)
#define LDPC_SEL2(b) ((w) >= 256 ? b##_256 : b##_128)
#elif defined(LDPC_HAVE_256)
#define LDPC_SEL3(b) ((w) >= 256 ? b##_256 : b##_128)
#define LDPC_SEL2(b) ((w) >= 256 ? b##_256 : b##_128)
#else
#define LDPC_SEL3(b) (b##_128)
#define LDPC_SEL2(b) (b##_128)
#endif
#define LDPC_SEL1(b) (b##_128)

static void encode_parity_check_part_optim(uint8_t *cc, uint8_t *d, short BG, short Zc, int simd_size, int ncols, time_stats_t *tinput_memcpy)
{
  // The factored encoders all index a doubled input buffer directly, so the
  // simd_size pre-rotated copies are only needed for lifting sizes that have no
  // factored variant (BG2 Zc 72/88/104/120, 8-byte aligned only). That avoids a
  // 32x/64x stack overallocation and, at BG2 Zc=384, ~245 KB of memcpy per code
  // block.
#ifdef USE_FACTORED
  const int need_replication = ((Zc & 15) != 0);
#elif defined(USE_ALIGNR)
  const int need_replication = (BG == 2);
#elif defined(USE_PERMUTEX) && defined(__AVX512VBMI__)
  const int need_replication = (BG == 2 || Zc < 384);
#else
  const int need_replication = 1;
#endif
  const int vla_simd = need_replication ? simd_size : 1;
  unsigned char c[2 * 22 * Zc * vla_simd] __attribute__((aligned(64))); //double size matrix of c
  if (tinput_memcpy)
    start_meas(tinput_memcpy);
  for (int i1 = 0; i1 < ncols; i1++) {
    memcpy(&c[2 * i1 * Zc], &cc[i1 * Zc], Zc * sizeof(unsigned char));
    memcpy(&c[(2 * i1 + 1) * Zc], &cc[i1 * Zc], Zc * sizeof(unsigned char));
  }
  if (need_replication) {
    for (int i1 = 1; i1 < simd_size; i1++) {
      memcpy(&c[(2 * ncols * Zc * i1)], &c[i1], (2 * ncols * Zc * sizeof(unsigned char)) - i1);
    }
  }
  if (tinput_memcpy)
    stop_meas(tinput_memcpy);
#ifdef USE_FACTORED
  const int w = ldpc_simd_width();
  if (BG == 1) {
    switch (Zc) {
      case 176: LDPC_SEL1(ldpc176_byte)(c, d); break;
      case 192: LDPC_SEL3(ldpc192_byte)(c, d); break;
      case 208: LDPC_SEL1(ldpc208_byte)(c, d); break;
      case 224: LDPC_SEL2(ldpc224_byte)(c, d); break;
      case 240: LDPC_SEL1(ldpc240_byte)(c, d); break;
      case 256: LDPC_SEL3(ldpc256_byte)(c, d); break;
      case 288: LDPC_SEL2(ldpc288_byte)(c, d); break;
      case 320: LDPC_SEL3(ldpc320_byte)(c, d); break;
      case 352: LDPC_SEL2(ldpc352_byte)(c, d); break;
      case 384: LDPC_SEL3(ldpc384_byte)(c, d); break;
      default: AssertFatal(false, "BG %d Zc %d is not supported yet\n", BG, Zc);
    }
  } else if (BG == 2) {
    switch (Zc) {
      // 8-byte aligned only: no factored variant
      case 72: ldpc_BG2_Zc72_byte(c, d); break;
      case 88: ldpc_BG2_Zc88_byte(c, d); break;
      case 104: ldpc_BG2_Zc104_byte(c, d); break;
      case 120: ldpc_BG2_Zc120_byte(c, d); break;
      case 80: LDPC_SEL1(ldpc_BG2_Zc80_byte)(c, d); break;
      case 96: LDPC_SEL2(ldpc_BG2_Zc96_byte)(c, d); break;
      case 112: LDPC_SEL1(ldpc_BG2_Zc112_byte)(c, d); break;
      case 128: LDPC_SEL3(ldpc_BG2_Zc128_byte)(c, d); break;
      case 144: LDPC_SEL1(ldpc_BG2_Zc144_byte)(c, d); break;
      case 160: LDPC_SEL2(ldpc_BG2_Zc160_byte)(c, d); break;
      case 176: LDPC_SEL1(ldpc_BG2_Zc176_byte)(c, d); break;
      case 192: LDPC_SEL3(ldpc_BG2_Zc192_byte)(c, d); break;
      case 208: LDPC_SEL1(ldpc_BG2_Zc208_byte)(c, d); break;
      case 224: LDPC_SEL2(ldpc_BG2_Zc224_byte)(c, d); break;
      case 240: LDPC_SEL1(ldpc_BG2_Zc240_byte)(c, d); break;
      case 256: LDPC_SEL3(ldpc_BG2_Zc256_byte)(c, d); break;
      case 288: LDPC_SEL2(ldpc_BG2_Zc288_byte)(c, d); break;
      case 320: LDPC_SEL3(ldpc_BG2_Zc320_byte)(c, d); break;
      case 352: LDPC_SEL2(ldpc_BG2_Zc352_byte)(c, d); break;
      case 384: LDPC_SEL3(ldpc_BG2_Zc384_byte)(c, d); break;
      default: AssertFatal(false, "BG %d Zc %d is not supported yet\n", BG, Zc);
    }
  } else
    AssertFatal(false, "BG %d is not supported\n", BG);
}
#else
static void encode_parity_check_part_optim_stock(uint8_t *cc, uint8_t *d, short BG, short Zc, int simd_size, int ncols, time_stats_t *tinput_memcpy)
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
#endif
