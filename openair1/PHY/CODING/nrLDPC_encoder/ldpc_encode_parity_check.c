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
#include <time.h>
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
// NO_FACTORED fallback: the stock encoders, as before the factored path existed.
#ifdef USE_ALIGNR
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

/* Measure the available widths instead of inferring from CPUID.
 *
 * The AVX512 datapath width is not architectural and CPUID does not expose it.
 * It varies within a family -- Zen 5 server is full width while Zen 5 mobile is
 * reported to be double-pumped, both family 0x1A -- and on some Intel parts
 * frequency licensing can cost more than the width gains. A family table cannot
 * express any of that, and gets progressively more wrong on hardware that did
 * not exist when it was written.
 *
 * So: encode a dummy block at each compiled width and keep the fastest. Costs
 * well under a millisecond, once per process. The narrower width wins ties,
 * since a wider one that is not clearly better is not worth the register
 * pressure; the margin also stops measurement noise flipping the choice.
 *
 * OAI_LDPC_NO_CALIBRATE falls back to the CPUID guess; OAI_LDPC_SIMD_WIDTH
 * overrides both.
 */
#if defined(LDPC_HAVE_256) || defined(LDPC_HAVE_512)
static double ldpc_time_enc(void (*f)(uint8_t *, uint8_t *), uint8_t *c, uint8_t *d, int iters)
{
  struct timespec a, b;
  clock_gettime(CLOCK_MONOTONIC, &a);
  for (int i = 0; i < iters; i++)
    f(c, d);
  clock_gettime(CLOCK_MONOTONIC, &b);
  return (b.tv_sec - a.tv_sec) + 1e-9 * (b.tv_nsec - a.tv_nsec);
}

static int ldpc_calibrate_width(void)
{
  enum { Z = 384, ITERS = 200, ROUNDS = 3 };
  static uint8_t cb[2 * 22 * Z] __attribute__((aligned(64)));
  static uint8_t db[46 * Z] __attribute__((aligned(64)));
  for (unsigned i = 0; i < sizeof cb; i++)
    cb[i] = (uint8_t)(i * 31u + 7u);

  double t[3] = {1e9, 1e9, 1e9}; /* 128, 256, 512 */
  for (int r = 0; r < ROUNDS; r++) {
    double x = ldpc_time_enc(ldpc384_byte_128, cb, db, ITERS);
    if (x < t[0])
      t[0] = x;
#ifdef LDPC_HAVE_256
    x = ldpc_time_enc(ldpc384_byte_256, cb, db, ITERS);
    if (x < t[1])
      t[1] = x;
#endif
#ifdef LDPC_HAVE_512
    x = ldpc_time_enc(ldpc384_byte_512, cb, db, ITERS);
    if (x < t[2])
      t[2] = x;
#endif
  }
  int w = 128;
  double best = t[0];
  if (t[1] < best * 0.97) {
    w = 256;
    best = t[1];
  }
  if (t[2] < best * 0.97)
    w = 512;
  if (getenv("OAI_LDPC_SIMD_VERBOSE"))
    fprintf(stderr, "ldpc: calibrated widths (us/CB) 128:%.3f 256:%.3f 512:%.3f -> %d\n",
            t[0] * 1e6 / ITERS, t[1] * 1e6 / ITERS, t[2] * 1e6 / ITERS, w);
  return w;
}
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
        // Verified on Genoa, Turin and Strix Halo (0x1A model 0x70, an APU --
        // the most likely counterexample, and it is full width too).
        w = (family >= 0x1A) ? 512 : 256;
      else
        // AVX512-capable Intel parts have the full-width datapath.
        w = 512;
    }
  }
#endif
#if defined(LDPC_HAVE_256) || defined(LDPC_HAVE_512)
  // the table above is only the fallback; prefer measuring the actual part
  if (!getenv("OAI_LDPC_NO_CALIBRATE"))
    w = ldpc_calibrate_width();
#endif
  return w;
}

/* Resolve the width when the module is loaded, rather than inside the first
 * encode. libldpc.so is dlopen'd by load_module_version_shlib, so an ELF
 * constructor runs at exactly that point and the first code block does not pay
 * the ~0.6 ms calibration inside a slot.
 *
 * A constructor rather than a call from LDPCinit(): that lives in
 * nrLDPC_decoder.c, which is shared with libldpc_orig.so, and ldpc_orig does not
 * include this file -- calling into it from there would not link.
 *
 * Idempotent, and the lazy path in ldpc_simd_width() remains for any caller that
 * reaches the encoder without the module being dlopen'd (ldpctest links it that
 * way in some configurations).
 *
 * Caveat: this runs on whichever core performs the dlopen, before thread
 * affinity is set. That is only a concern on a heterogeneous CPU, and the x86
 * parts with AVX512 are all homogeneous.
 */
#if defined(LDPC_HAVE_256) || defined(LDPC_HAVE_512)
__attribute__((constructor)) static void ldpc_simd_width_init(void)
{
  (void)ldpc_simd_width();
}
#endif

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
#else
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
#endif
}
