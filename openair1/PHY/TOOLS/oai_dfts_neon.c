/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#if defined(__arm__) || defined(__aarch64__)

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <math.h>
#include <complex.h>
#include <pthread.h>

#if defined(__aarch64__)
#include <arm_sve.h>
#if defined(__linux__)
#include <sys/auxv.h>
#include <asm/hwcap.h>
#endif
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define OAIDFTS_MAIN
#include "../sse_intrin.h"
#include "assertions.h"
#include "tools_defs.h"

#if defined(__aarch64__)

/* AArch64 Q15 DFT/IDFT backend.
 * SVE2 is used only with VL=128; otherwise execution falls back to NEON.
 * Per-size plans and transform-specific twiddles are created lazily and cached.
 */

typedef uint8x16_t neon_m128;
typedef uint8x16_t neon_m128i;
typedef uint8x16_t neon_m128d;

typedef int16x8x2_t oc_cq15x8_t; /* eight complex Q15 values */

static inline int16x8_t neon128_as_i16(neon_m128 x)
{
  return vreinterpretq_s16_u8(x);
}

static inline int32x4_t neon128_as_i32(neon_m128 x)
{
  return vreinterpretq_s32_u8(x);
}

static inline uint64x2_t neon128_as_u64(neon_m128 x)
{
  return vreinterpretq_u64_u8(x);
}

static inline neon_m128 neon128_from_i16(int16x8_t x)
{
  return vreinterpretq_u8_s16(x);
}

static inline neon_m128 neon128_from_i32(int32x4_t x)
{
  return vreinterpretq_u8_s32(x);
}

static inline neon_m128 neon128_from_u64(uint64x2_t x)
{
  return vreinterpretq_u8_u64(x);
}

static inline neon_m128i neon128_load_i(const neon_m128i *p)
{
  return vld1q_u8((const uint8_t *)p);
}
static inline neon_m128i neon128_loadu_i(const neon_m128i *p)
{
  return neon128_load_i(p);
}
static inline void neon128_store_i(neon_m128i *p, neon_m128i x)
{
  vst1q_u8((uint8_t *)p, x);
}
static inline void neon128_storeu_i(neon_m128i *p, neon_m128i x)
{
  neon128_store_i(p, x);
}

static inline oc_cq15x8_t oc_cq15x8_make(int16x8_t v0, int16x8_t v1)
{
  oc_cq15x8_t r = {{v0, v1}};
  return r;
}
static inline oc_cq15x8_t oc_cq15x8_load(const oc_cq15x8_t *p)
{
  const int16_t *q = (const int16_t *)p;
  return oc_cq15x8_make(vld1q_s16(q), vld1q_s16(q + 8));
}
static inline oc_cq15x8_t oc_cq15x8_loadu(const oc_cq15x8_t *p)
{
  return oc_cq15x8_load(p);
}
static inline void oc_cq15x8_store(oc_cq15x8_t *p, oc_cq15x8_t x)
{
  int16_t *q = (int16_t *)p;
  vst1q_s16(q, x.val[0]);
  vst1q_s16(q + 8, x.val[1]);
}
static inline void oc_cq15x8_storeu(oc_cq15x8_t *p, oc_cq15x8_t x)
{
  oc_cq15x8_store(p, x);
}

static inline oc_cq15x8_t oc_cq15x8_rshr2_i16(oc_cq15x8_t x)
{
  return oc_cq15x8_make(vrshrq_n_s16(x.val[0], 2), vrshrq_n_s16(x.val[1], 2));
}
static inline oc_cq15x8_t oc_cq15x8_set1_i16(int x)
{
  int16x8_t q = vdupq_n_s16((int16_t)x);
  return oc_cq15x8_make(q, q);
}
static inline oc_cq15x8_t oc_cq15x8_set1_i32(int x)
{
  int32x4_t q = vdupq_n_s32(x);
  return oc_cq15x8_make(vreinterpretq_s16_s32(q), vreinterpretq_s16_s32(q));
}
static inline oc_cq15x8_t oc_cq15x8_setr_i16(int a0,
                                             int a1,
                                             int a2,
                                             int a3,
                                             int a4,
                                             int a5,
                                             int a6,
                                             int a7,
                                             int a8,
                                             int a9,
                                             int a10,
                                             int a11,
                                             int a12,
                                             int a13,
                                             int a14,
                                             int a15)
{
  const int16_t lo[8] __attribute__((
      aligned(16))) = {(int16_t)a0, (int16_t)a1, (int16_t)a2, (int16_t)a3, (int16_t)a4, (int16_t)a5, (int16_t)a6, (int16_t)a7};
  const int16_t hi[8] __attribute__((aligned(
      16))) = {(int16_t)a8, (int16_t)a9, (int16_t)a10, (int16_t)a11, (int16_t)a12, (int16_t)a13, (int16_t)a14, (int16_t)a15};
  return oc_cq15x8_make(vld1q_s16(lo), vld1q_s16(hi));
}
static inline oc_cq15x8_t oc_cq15x8_setr_i32(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7)
{
  const int32_t lo[4] __attribute__((aligned(16))) = {a0, a1, a2, a3}, hi[4] __attribute__((aligned(16))) = {a4, a5, a6, a7};
  return oc_cq15x8_make(vreinterpretq_s16_s32(vld1q_s32(lo)), vreinterpretq_s16_s32(vld1q_s32(hi)));
}
static inline oc_cq15x8_t oc_cq15x8_setr_i8(int a0,
                                            int a1,
                                            int a2,
                                            int a3,
                                            int a4,
                                            int a5,
                                            int a6,
                                            int a7,
                                            int a8,
                                            int a9,
                                            int a10,
                                            int a11,
                                            int a12,
                                            int a13,
                                            int a14,
                                            int a15,
                                            int a16,
                                            int a17,
                                            int a18,
                                            int a19,
                                            int a20,
                                            int a21,
                                            int a22,
                                            int a23,
                                            int a24,
                                            int a25,
                                            int a26,
                                            int a27,
                                            int a28,
                                            int a29,
                                            int a30,
                                            int a31)
{
  const int8_t lo[16] __attribute__((aligned(16))) = {(int8_t)a0,
                                                      (int8_t)a1,
                                                      (int8_t)a2,
                                                      (int8_t)a3,
                                                      (int8_t)a4,
                                                      (int8_t)a5,
                                                      (int8_t)a6,
                                                      (int8_t)a7,
                                                      (int8_t)a8,
                                                      (int8_t)a9,
                                                      (int8_t)a10,
                                                      (int8_t)a11,
                                                      (int8_t)a12,
                                                      (int8_t)a13,
                                                      (int8_t)a14,
                                                      (int8_t)a15};
  const int8_t hi[16] __attribute__((aligned(16))) = {(int8_t)a16,
                                                      (int8_t)a17,
                                                      (int8_t)a18,
                                                      (int8_t)a19,
                                                      (int8_t)a20,
                                                      (int8_t)a21,
                                                      (int8_t)a22,
                                                      (int8_t)a23,
                                                      (int8_t)a24,
                                                      (int8_t)a25,
                                                      (int8_t)a26,
                                                      (int8_t)a27,
                                                      (int8_t)a28,
                                                      (int8_t)a29,
                                                      (int8_t)a30,
                                                      (int8_t)a31};
  return oc_cq15x8_make(vreinterpretq_s16_s8(vld1q_s8(lo)), vreinterpretq_s16_s8(vld1q_s8(hi)));
}
static inline oc_cq15x8_t oc_cq15x8_set_i8(int a31,
                                           int a30,
                                           int a29,
                                           int a28,
                                           int a27,
                                           int a26,
                                           int a25,
                                           int a24,
                                           int a23,
                                           int a22,
                                           int a21,
                                           int a20,
                                           int a19,
                                           int a18,
                                           int a17,
                                           int a16,
                                           int a15,
                                           int a14,
                                           int a13,
                                           int a12,
                                           int a11,
                                           int a10,
                                           int a9,
                                           int a8,
                                           int a7,
                                           int a6,
                                           int a5,
                                           int a4,
                                           int a3,
                                           int a2,
                                           int a1,
                                           int a0)
{
  return oc_cq15x8_setr_i8(a0,
                           a1,
                           a2,
                           a3,
                           a4,
                           a5,
                           a6,
                           a7,
                           a8,
                           a9,
                           a10,
                           a11,
                           a12,
                           a13,
                           a14,
                           a15,
                           a16,
                           a17,
                           a18,
                           a19,
                           a20,
                           a21,
                           a22,
                           a23,
                           a24,
                           a25,
                           a26,
                           a27,
                           a28,
                           a29,
                           a30,
                           a31);
}

/* 128-bit constructors. */

static inline neon_m128i neon128_set1_i16(int x)
{
  return neon128_from_i16(vdupq_n_s16((int16_t)x));
}

static inline neon_m128i neon128_setr_i16(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7)
{
  const int16_t v[8] __attribute__((
      aligned(16))) = {(int16_t)a0, (int16_t)a1, (int16_t)a2, (int16_t)a3, (int16_t)a4, (int16_t)a5, (int16_t)a6, (int16_t)a7};
  return neon128_from_i16(vld1q_s16(v));
}

/* Lane permutations; all are implemented with native NEON table/zip operations. */

/* Native integer/Q15 arithmetic. */

static inline neon_m128i neon128_adds_i16(neon_m128i a, neon_m128i b)
{
  return neon128_from_i16(vqaddq_s16(neon128_as_i16(a), neon128_as_i16(b)));
}
static inline neon_m128i neon128_subs_i16(neon_m128i a, neon_m128i b)
{
  return neon128_from_i16(vqsubq_s16(neon128_as_i16(a), neon128_as_i16(b)));
}
static inline neon_m128i neon128_mulhrs_i16(neon_m128i a, neon_m128i b)
{
  return neon128_from_i16(vqrdmulhq_s16(neon128_as_i16(a), neon128_as_i16(b)));
}

static inline neon_m128i neon128_unpacklo_i32(neon_m128i a, neon_m128i b)
{
  return neon128_from_i32(vzip1q_s32(neon128_as_i32(a), neon128_as_i32(b)));
}
static inline neon_m128i neon128_unpackhi_i32(neon_m128i a, neon_m128i b)
{
  return neon128_from_i32(vzip2q_s32(neon128_as_i32(a), neon128_as_i32(b)));
}
static inline neon_m128i neon128_unpacklo_i64(neon_m128i a, neon_m128i b)
{
  return neon128_from_u64(vzip1q_u64(neon128_as_u64(a), neon128_as_u64(b)));
}
static inline neon_m128i neon128_unpackhi_i64(neon_m128i a, neon_m128i b)
{
  return neon128_from_u64(vzip2q_u64(neon128_as_u64(a), neon128_as_u64(b)));
}

static inline oc_cq15x8_t oc_cq15x8_add_i16(oc_cq15x8_t a, oc_cq15x8_t b)
{
  return oc_cq15x8_make(vaddq_s16(a.val[0], b.val[0]), vaddq_s16(a.val[1], b.val[1]));
}
static inline oc_cq15x8_t oc_cq15x8_sub_i16(oc_cq15x8_t a, oc_cq15x8_t b)
{
  return oc_cq15x8_make(vsubq_s16(a.val[0], b.val[0]), vsubq_s16(a.val[1], b.val[1]));
}
static inline oc_cq15x8_t oc_cq15x8_adds_i16(oc_cq15x8_t a, oc_cq15x8_t b)
{
  return oc_cq15x8_make(vqaddq_s16(a.val[0], b.val[0]), vqaddq_s16(a.val[1], b.val[1]));
}
static inline oc_cq15x8_t oc_cq15x8_subs_i16(oc_cq15x8_t a, oc_cq15x8_t b)
{
  return oc_cq15x8_make(vqsubq_s16(a.val[0], b.val[0]), vqsubq_s16(a.val[1], b.val[1]));
}
static inline oc_cq15x8_t oc_cq15x8_mulhrs_i16(oc_cq15x8_t a, oc_cq15x8_t b)
{
  return oc_cq15x8_make(vqrdmulhq_s16(a.val[0], b.val[0]), vqrdmulhq_s16(a.val[1], b.val[1]));
}
static inline oc_cq15x8_t oc_cq15x8_srai_i16(oc_cq15x8_t a, int n)
{
  int16x8_t sh = vdupq_n_s16((int16_t)-n);
  return oc_cq15x8_make(vshlq_s16(a.val[0], sh), vshlq_s16(a.val[1], sh));
}
static inline oc_cq15x8_t oc_cq15x8_add_i32(oc_cq15x8_t a, oc_cq15x8_t b)
{
  return oc_cq15x8_make(vreinterpretq_s16_s32(vaddq_s32(vreinterpretq_s32_s16(a.val[0]), vreinterpretq_s32_s16(b.val[0]))),
                        vreinterpretq_s16_s32(vaddq_s32(vreinterpretq_s32_s16(a.val[1]), vreinterpretq_s32_s16(b.val[1]))));
}
static inline oc_cq15x8_t oc_cq15x8_srai_i32(oc_cq15x8_t a, int n)
{
  int32x4_t sh = vdupq_n_s32(-n);
  return oc_cq15x8_make(vreinterpretq_s16_s32(vshlq_s32(vreinterpretq_s32_s16(a.val[0]), sh)),
                        vreinterpretq_s16_s32(vshlq_s32(vreinterpretq_s32_s16(a.val[1]), sh)));
}
static inline int32x4_t oc_madd4_i16(int16x8_t a, int16x8_t b)
{
  return vpaddq_s32(vmull_s16(vget_low_s16(a), vget_low_s16(b)), vmull_s16(vget_high_s16(a), vget_high_s16(b)));
}
static inline oc_cq15x8_t oc_cq15x8_madd_i16(oc_cq15x8_t a, oc_cq15x8_t b)
{
  return oc_cq15x8_make(vreinterpretq_s16_s32(oc_madd4_i16(a.val[0], b.val[0])),
                        vreinterpretq_s16_s32(oc_madd4_i16(a.val[1], b.val[1])));
}
static inline int16x8_t oc_sign8_i16(int16x8_t a, int16x8_t b)
{
  uint16x8_t neg = vcltq_s16(b, vdupq_n_s16(0)), zero = vceqq_s16(b, vdupq_n_s16(0));
  int16x8_t r = vbslq_s16(neg, vnegq_s16(a), a);
  return vbslq_s16(zero, vdupq_n_s16(0), r);
}
static inline oc_cq15x8_t oc_cq15x8_sign_i16(oc_cq15x8_t a, oc_cq15x8_t b)
{
  return oc_cq15x8_make(oc_sign8_i16(a.val[0], b.val[0]), oc_sign8_i16(a.val[1], b.val[1]));
}
static inline int16x8_t oc_shuffle16_bytes(int16x8_t a, int16x8_t m)
{
  uint8x16_t mask = vreinterpretq_u8_s16(m);
  uint8x16_t low = vandq_u8(mask, vdupq_n_u8(0x0f)), high = vandq_u8(mask, vdupq_n_u8(0x80));
  return vreinterpretq_s16_u8(vqtbl1q_u8(vreinterpretq_u8_s16(a), vorrq_u8(low, high)));
}
static inline oc_cq15x8_t oc_cq15x8_shuffle_i8(oc_cq15x8_t a, oc_cq15x8_t m)
{
  return oc_cq15x8_make(oc_shuffle16_bytes(a.val[0], m.val[0]), oc_shuffle16_bytes(a.val[1], m.val[1]));
}
static inline oc_cq15x8_t oc_cq15x8_packs_i32(oc_cq15x8_t a, oc_cq15x8_t b)
{
  int16x8_t lo = vcombine_s16(vqmovn_s32(vreinterpretq_s32_s16(a.val[0])), vqmovn_s32(vreinterpretq_s32_s16(b.val[0])));
  int16x8_t hi = vcombine_s16(vqmovn_s32(vreinterpretq_s32_s16(a.val[1])), vqmovn_s32(vreinterpretq_s32_s16(b.val[1])));
  return oc_cq15x8_make(lo, hi);
}
static inline oc_cq15x8_t oc_cq15x8_unpacklo_i32(oc_cq15x8_t a, oc_cq15x8_t b)
{
  return oc_cq15x8_make(vreinterpretq_s16_s32(vzip1q_s32(vreinterpretq_s32_s16(a.val[0]), vreinterpretq_s32_s16(b.val[0]))),
                        vreinterpretq_s16_s32(vzip1q_s32(vreinterpretq_s32_s16(a.val[1]), vreinterpretq_s32_s16(b.val[1]))));
}
static inline oc_cq15x8_t oc_cq15x8_unpackhi_i32(oc_cq15x8_t a, oc_cq15x8_t b)
{
  return oc_cq15x8_make(vreinterpretq_s16_s32(vzip2q_s32(vreinterpretq_s32_s16(a.val[0]), vreinterpretq_s32_s16(b.val[0]))),
                        vreinterpretq_s16_s32(vzip2q_s32(vreinterpretq_s32_s16(a.val[1]), vreinterpretq_s32_s16(b.val[1]))));
}
static inline oc_cq15x8_t oc_cq15x8_unpacklo_i64(oc_cq15x8_t a, oc_cq15x8_t b)
{
  return oc_cq15x8_make(vreinterpretq_s16_u64(vzip1q_u64(vreinterpretq_u64_s16(a.val[0]), vreinterpretq_u64_s16(b.val[0]))),
                        vreinterpretq_s16_u64(vzip1q_u64(vreinterpretq_u64_s16(a.val[1]), vreinterpretq_u64_s16(b.val[1]))));
}
static inline oc_cq15x8_t oc_cq15x8_unpackhi_i64(oc_cq15x8_t a, oc_cq15x8_t b)
{
  return oc_cq15x8_make(vreinterpretq_s16_u64(vzip2q_u64(vreinterpretq_u64_s16(a.val[0]), vreinterpretq_u64_s16(b.val[0]))),
                        vreinterpretq_s16_u64(vzip2q_u64(vreinterpretq_u64_s16(a.val[1]), vreinterpretq_u64_s16(b.val[1]))));
}
static inline neon_m128i oc_cq15x8_extract_half(oc_cq15x8_t a, int lane)
{
  return neon128_from_i16(a.val[lane & 1]);
}
static inline neon_m128i oc_cq15x8_low128(oc_cq15x8_t a)
{
  return neon128_from_i16(a.val[0]);
}

static inline oc_cq15x8_t oc_cq15x8_select_halves(oc_cq15x8_t a, oc_cq15x8_t b, int imm)
{
  int16x8_t src[4] = {a.val[0], a.val[1], b.val[0], b.val[1]};
  int16x8_t lo = (imm & 0x08) ? vdupq_n_s16(0) : src[imm & 3];
  int16x8_t hi = (imm & 0x80) ? vdupq_n_s16(0) : src[(imm >> 4) & 3];
  return oc_cq15x8_make(lo, hi);
}
static inline oc_cq15x8_t oc_cq15x8_permute_i32(oc_cq15x8_t a, oc_cq15x8_t idx)
{
  uint32_t iv[8] __attribute__((aligned(16)));
  uint8_t o0[16] __attribute__((aligned(16))), o1[16] __attribute__((aligned(16)));
  vst1q_u32(iv, vreinterpretq_u32_s16(idx.val[0]));
  vst1q_u32(iv + 4, vreinterpretq_u32_s16(idx.val[1]));
  for (int j = 0; j < 8; j++)
    for (int q = 0; q < 4; q++) {
      uint8_t z = (uint8_t)(4 * (iv[j] & 7u) + q);
      if (j < 4)
        o0[4 * j + q] = z;
      else
        o1[4 * (j - 4) + q] = z;
    }
  uint8x16x2_t tbl = {{vreinterpretq_u8_s16(a.val[0]), vreinterpretq_u8_s16(a.val[1])}};
  return oc_cq15x8_make(vreinterpretq_s16_u8(vqtbl2q_u8(tbl, vld1q_u8(o0))), vreinterpretq_s16_u8(vqtbl2q_u8(tbl, vld1q_u8(o1))));
}

#define ONE_OVER_SQRT2_Q15 23170
#ifndef Q15_INV_SQRT2
#define Q15_INV_SQRT2 ((int16_t)23170) /* round(0.70710678118 * 32768) */
#endif
#define Q15_HALF_SQRT3 ((int16_t)28378)
#define Q15_INV_SQRT8 ((int16_t)11585) /* round(32767 / sqrt(8)) */

typedef enum { DFT_DIR_FORWARD = -1, DFT_DIR_INVERSE = 1 } dft_dir_t;

static void dft2048_radix8_q15(const c16_t *src, c16_t *dst, dft_dir_t dir);

static void dft_split_radix_pure_simd(const c16_t *x, c16_t *y, int N, dft_dir_t dir);
static void dft_split_radix_pure_simd_core(const c16_t *x, c16_t *y, c16_t *work, int N, dft_dir_t dir);
static c16_t *oai_dft_tls_inverse_leaf_work(size_t need);
static inline void neon_mono_idft4_q15(const c16_t *src, c16_t *dst);
static inline void neon_mono_idft8_q15(const c16_t *src, c16_t *dst);
static inline void neon_mono_idft12_fast_q15(const c16_t *src, c16_t *dst);
static inline void idft16lts_q15_native(const c16_t *src, c16_t *dst);
static inline void idft32lts_q15_native(const c16_t *src, c16_t *dst);
static inline void neon_dft8x4_branch_major_q15(const c16_t *src, c16_t *dst, int src_stride, int dst_stride);
static inline void neon_idft8x4_branch_major_q15(const c16_t *src, c16_t *dst, int src_stride, int dst_stride);

typedef struct {
  int N;
  int blocks;
  oc_cq15x8_t *W1_RE_NEGIM;
  oc_cq15x8_t *W1_IM_RE;
  oc_cq15x8_t *W3_RE_NEGIM;
  oc_cq15x8_t *W3_IM_RE;
} sr_twiddle_simd_t;

#define SR_MAX_LOG2 17

static inline int log2_int(unsigned int N)
{
  return __builtin_ctz(N);
}
static sr_twiddle_simd_t sr_twiddles_fwd[SR_MAX_LOG2 + 1];
static sr_twiddle_simd_t sr_twiddles_bwd[SR_MAX_LOG2 + 1];

#define MAX_N 98304

static inline int16_t sat16_i32(int32_t x)
{
  if (x > 32767)
    return 32767;
  if (x < -32767)
    return -32767;
  return (int16_t)x;
}

static inline int16_t q15_from_float(float x)
{
  return sat16_i32((int32_t)lrintf(32767.0f * x));
}

static inline neon_m128i swap_complex_pairs_i16_128(neon_m128i a)
{
  return neon128_from_i16(vrev32q_s16(neon128_as_i16(a)));
}

static inline neon_m128i complex_mul4_prepack_q15_128(neon_m128i a, neon_m128i w_re_re, neon_m128i w_im_signed)
{
  const neon_m128i a_swapped = swap_complex_pairs_i16_128(a);

  const neon_m128i prod_re = neon128_mulhrs_i16(a, w_re_re);
  const neon_m128i prod_im = neon128_mulhrs_i16(a_swapped, w_im_signed);

  return neon128_adds_i16(prod_re, prod_im);
}

static inline neon_m128i mullts_q15_128(neon_m128i z)
{
  const int16x8_t swapped = vrev32q_s16(neon128_as_i16(z));
  const int16x8_t sign = {-1, 1, -1, 1, -1, 1, -1, 1};
  return neon128_from_i16(vmulq_s16(swapped, sign));
}

static inline neon_m128i mul_minuslts_q15_128(neon_m128i z)
{
  const int16x8_t swapped = vrev32q_s16(neon128_as_i16(z));
  const int16x8_t sign = {1, -1, 1, -1, 1, -1, 1, -1};
  return neon128_from_i16(vmulq_s16(swapped, sign));
}

static inline neon_m128i q15_mul_i16_128(neon_m128i x, int16_t q15)
{
  return neon128_mulhrs_i16(x, neon128_set1_i16(q15));
}

#define Q15_INV_SQRT3 18919 /* 1 / sqrt(3) */
#define Q15_HALF 16384 /* 1 / 2 */

static void *aligned_malloc64(size_t size)
{
  void *ptr = NULL;

  if (posix_memalign(&ptr, 64, size) != 0) {
    return NULL;
  }

  return ptr;
}

static inline int is_power_of_two_int(int x)
{
  return x > 0 && ((x & (x - 1)) == 0);
}

static inline int16_t sat_i16(long v)
{
  if (v > 32767)
    return 32767;
  if (v < -32768)
    return -32767;
  return (int16_t)v;
}

#ifndef Q15_INV_SQRT5
#define Q15_INV_SQRT5 ((int16_t)14654)
#endif

#if defined(__clang__)
#define SVE2_TARGET __attribute__((target("sve2")))
#elif defined(__GNUC__)
#define SVE2_TARGET __attribute__((target("+sve2")))
#else
#define SVE2_TARGET
#endif

/* Returns whether the current Linux AArch64 CPU exposes SVE and SVE2. */
static int sve2_runtime_available(void)
{
#if defined(__aarch64__) && defined(__linux__)
  const unsigned long hwcap = getauxval(AT_HWCAP);
  const unsigned long hwcap2 = getauxval(AT_HWCAP2);
  return (hwcap & HWCAP_SVE) != 0 && (hwcap2 & HWCAP2_SVE2) != 0;
#else
  return 0;
#endif
}

/* Returns the active SVE vector length in bits. */
SVE2_TARGET static int sve2_vector_bits(void)
{
  return (int)svcntb() * 8;
}

static int16_t sve2_q15_scale_coeff(int16_t x, int radix)
{
  switch (radix) {
    case 2:
      return sat_i16((long)((float)x / sqrtf(2.0f)));
    case 3:
      return sat_i16((long)((float)x / sqrtf(3.0f)));
    case 4:
      return (int16_t)(x / 2);
    case 5:
      return sat_i16((long)((float)x / sqrtf(5.0f)));
    default:
      abort();
      __builtin_unreachable();
  }
}

static void sve2_dft64_special_prepare(void);
static void sve2_dft128_special_prepare(void);
static void sve2_dft256_special_prepare(void);
static void sve2_dft512_special_prepare(void);
static void sve2_tiny_special_prepare(void);

SVE2_TARGET static inline svint16_t sve2_cmul_q15(svint16_t a, svint16_t b)
{
  svint16_t y = svdup_n_s16(0);
  y = svqrdcmlah_s16(y, a, b, 0);
  y = svqrdcmlah_s16(y, a, b, 90);
  return y;
}

SVE2_TARGET static inline svint16_t sve2_q15_real_mul(svint16_t x, int16_t c)
{
  return svqrdmulh_s16(x, svdup_n_s16(c));
}

SVE2_TARGET static inline void sve2_dft3_q15(svint16_t x0, svint16_t x1, svint16_t x2, svint16_t *y0, svint16_t *y1, svint16_t *y2)
{
  const svint16_t sum = svqadd_s16_x(svptrue_b16(), x1, x2);
  const svint16_t diff = svqsub_s16_x(svptrue_b16(), x1, x2);
  const svint16_t base = svqsub_s16_x(svptrue_b16(), x0, sve2_q15_real_mul(sum, Q15_HALF));
  const svint16_t imag = sve2_q15_real_mul(diff, Q15_HALF_SQRT3);
  *y0 = svqadd_s16_x(svptrue_b16(), x0, sum);
  *y1 = svqcadd_s16(base, imag, 270);
  *y2 = svqcadd_s16(base, imag, 90);
}

SVE2_TARGET static inline void sve2_dft4_butterfly_q15(svint16_t x0,
                                                       svint16_t x1,
                                                       svint16_t x2,
                                                       svint16_t x3,
                                                       svint16_t *y0,
                                                       svint16_t *y1,
                                                       svint16_t *y2,
                                                       svint16_t *y3)
{
  const svbool_t pg = svptrue_b16();
  const svint16_t s02 = svqadd_s16_x(pg, x0, x2);
  const svint16_t d02 = svqsub_s16_x(pg, x0, x2);
  const svint16_t s13 = svqadd_s16_x(pg, x1, x3);
  const svint16_t d13 = svqsub_s16_x(pg, x1, x3);
  *y0 = svqadd_s16_x(pg, s02, s13);
  *y2 = svqsub_s16_x(pg, s02, s13);
  *y1 = svqcadd_s16(d02, d13, 270);
  *y3 = svqcadd_s16(d02, d13, 90);
}

SVE2_TARGET static inline void sve2_dft5_q15(svint16_t x0,
                                             svint16_t x1,
                                             svint16_t x2,
                                             svint16_t x3,
                                             svint16_t x4,
                                             svint16_t *y0,
                                             svint16_t *y1,
                                             svint16_t *y2,
                                             svint16_t *y3,
                                             svint16_t *y4)
{
  const svbool_t pg = svptrue_b16();
  const svint16_t t1 = svqadd_s16_x(pg, x1, x4);
  const svint16_t t2 = svqadd_s16_x(pg, x2, x3);
  const svint16_t d1 = svqsub_s16_x(pg, x1, x4);
  const svint16_t d2 = svqsub_s16_x(pg, x2, x3);
  *y0 = svqadd_s16_x(pg, x0, svqadd_s16_x(pg, t1, t2));

  const svint16_t b1 = svqadd_s16_x(pg, x0, svqadd_s16_x(pg, sve2_q15_real_mul(t1, 10126), sve2_q15_real_mul(t2, -26510)));
  const svint16_t q1 = svqadd_s16_x(pg, sve2_q15_real_mul(d1, 31163), sve2_q15_real_mul(d2, 19260));
  *y1 = svqcadd_s16(b1, q1, 270);
  *y4 = svqcadd_s16(b1, q1, 90);

  const svint16_t b2 = svqadd_s16_x(pg, x0, svqadd_s16_x(pg, sve2_q15_real_mul(t1, -26510), sve2_q15_real_mul(t2, 10126)));
  const svint16_t q2 = svqsub_s16_x(pg, sve2_q15_real_mul(d1, 19260), sve2_q15_real_mul(d2, 31163));
  *y2 = svqcadd_s16(b2, q2, 270);
  *y3 = svqcadd_s16(b2, q2, 90);
}

SVE2_TARGET static inline void sve2_idft3_q15(svint16_t x0, svint16_t x1, svint16_t x2, svint16_t *y0, svint16_t *y1, svint16_t *y2)
{
  svint16_t f0, f1, f2;
  sve2_dft3_q15(x0, x1, x2, &f0, &f1, &f2);
  *y0 = f0;
  *y1 = f2;
  *y2 = f1;
}

SVE2_TARGET static inline void sve2_idft5_q15(svint16_t x0,
                                              svint16_t x1,
                                              svint16_t x2,
                                              svint16_t x3,
                                              svint16_t x4,
                                              svint16_t *y0,
                                              svint16_t *y1,
                                              svint16_t *y2,
                                              svint16_t *y3,
                                              svint16_t *y4)
{
  svint16_t f0, f1, f2, f3, f4;
  sve2_dft5_q15(x0, x1, x2, x3, x4, &f0, &f1, &f2, &f3, &f4);
  *y0 = f0;
  *y1 = f4;
  *y2 = f3;
  *y3 = f2;
  *y4 = f1;
}

typedef struct {
  int initialized;
  int16_t c16_q15[4][4][8] __attribute__((aligned(64)));
  int16_t c64_q15[16][8] __attribute__((aligned(64)));
} sve2_dft64_special_twiddle_t;

static sve2_dft64_special_twiddle_t g_sve2_dft64_special_tw;

static inline int16_t sve2_dft64_q15_half_twiddle(float x)
{
  long q = lrintf(32767.0f * x);
  if (q > INT16_MAX)
    q = INT16_MAX;
  if (q < INT16_MIN)
    q = INT16_MIN;
  return (int16_t)((int16_t)q / 2);
}

/* Prepare DFT64 radix-4 twiddles. */
static void sve2_dft64_special_prepare(void)
{
  sve2_dft64_special_twiddle_t *tw = &g_sve2_dft64_special_tw;
  if (tw->initialized)
    return;

  for (int k = 0; k < 4; k++) {
    for (int r = 0; r < 4; r++) {
      const float a = -2.0f * (float)M_PI * (float)(k * r) / 16.0f;
      const float wr = cosf(a);
      const float wi = sinf(a);
      const int16_t qr = sve2_dft64_q15_half_twiddle(wr);
      const int16_t qi = sve2_dft64_q15_half_twiddle(wi);
      for (int lane = 0; lane < 4; lane++) {
        tw->c16_q15[k][r][2 * lane + 0] = qr;
        tw->c16_q15[k][r][2 * lane + 1] = qi;
      }
    }
  }

  for (int k = 0; k < 16; k++) {
    for (int r = 0; r < 4; r++) {
      const float a = -2.0f * (float)M_PI * (float)(k * r) / 64.0f;
      const float wr = cosf(a);
      const float wi = sinf(a);
      tw->c64_q15[k][2 * r + 0] = sve2_dft64_q15_half_twiddle(wr);
      tw->c64_q15[k][2 * r + 1] = sve2_dft64_q15_half_twiddle(wi);
    }
  }

  tw->initialized = 1;
}

SVE2_TARGET static inline svint16_t sve2_q15_half_shift(svbool_t pg, svint16_t x)
{
  /* Exact power-of-two 1/2 normalization: no Q15 multiply. */
  return svasr_n_s16_x(pg, x, 1);
}

SVE2_TARGET static inline svint16_t sve2_dft64_c16_q15(svbool_t pg, svint16_t x, int k, int r)
{
  if (k == 0)
    return sve2_q15_half_shift(pg, x);

  const svint16_t w = svld1rq_s16(pg, g_sve2_dft64_special_tw.c16_q15[k][r]);
  return sve2_cmul_q15(x, w);
}

/* DFT64 DC is accumulated in int32 and scaled once by 1/8 to avoid
 * repeated Q15 rounding. */
SVE2_TARGET static inline c16_t sve2_dft64_dc_from_src(const c16_t *src)
{
  int32x4_t acc_re = vdupq_n_s32(0);
  int32x4_t acc_im = vdupq_n_s32(0);

  for (int i = 0; i < 64; i += 8) {
    const int16x8x2_t ri = vld2q_s16((const int16_t *)(src + i));
    acc_re = vaddq_s32(acc_re, vpaddlq_s16(ri.val[0]));
    acc_im = vaddq_s32(acc_im, vpaddlq_s16(ri.val[1]));
  }

  int32_t re = vaddvq_s32(acc_re);
  int32_t im = vaddvq_s32(acc_im);

  /* Signed rounded /8: add 4 for non-negative values and 3 for negative values. */
  re = (re + 4 + (re >> 31)) >> 3;
  im = (im + 4 + (im >> 31)) >> 3;

  if (re > INT16_MAX)
    re = INT16_MAX;
  if (re < INT16_MIN)
    re = INT16_MIN;
  if (im > INT16_MAX)
    im = INT16_MAX;
  if (im < INT16_MIN)
    im = INT16_MIN;

  c16_t dc;
  dc.r = (int16_t)re;
  dc.i = (int16_t)im;
  return dc;
}

SVE2_TARGET static inline void sve2_dft64_transpose4_q15_128(svint16_t a,
                                                             svint16_t b,
                                                             svint16_t c,
                                                             svint16_t d,
                                                             svint16_t *t0,
                                                             svint16_t *t1,
                                                             svint16_t *t2,
                                                             svint16_t *t3)
{
  const svuint32_t au = svreinterpret_u32_s16(a);
  const svuint32_t bu = svreinterpret_u32_s16(b);
  const svuint32_t cu = svreinterpret_u32_s16(c);
  const svuint32_t du = svreinterpret_u32_s16(d);
  const svuint32_t ab0 = svzip1_u32(au, bu);
  const svuint32_t ab1 = svzip2_u32(au, bu);
  const svuint32_t cd0 = svzip1_u32(cu, du);
  const svuint32_t cd1 = svzip2_u32(cu, du);
  const svuint64_t z0 = svzip1_u64(svreinterpret_u64_u32(ab0), svreinterpret_u64_u32(cd0));
  const svuint64_t z1 = svzip2_u64(svreinterpret_u64_u32(ab0), svreinterpret_u64_u32(cd0));
  const svuint64_t z2 = svzip1_u64(svreinterpret_u64_u32(ab1), svreinterpret_u64_u32(cd1));
  const svuint64_t z3 = svzip2_u64(svreinterpret_u64_u32(ab1), svreinterpret_u64_u32(cd1));
  *t0 = svreinterpret_s16_u64(z0);
  *t1 = svreinterpret_s16_u64(z1);
  *t2 = svreinterpret_s16_u64(z2);
  *t3 = svreinterpret_s16_u64(z3);
}

/* Runs the unitary Q15 DFT64 as three specialized radix-4 SVE2 stages. */
SVE2_TARGET static void sve2_dft64_q15_special(const c16_t *src, c16_t *dst)
{
  const svbool_t pg = svwhilelt_b16((uint64_t)0, (uint64_t)8);
  const c16_t dc = sve2_dft64_dc_from_src(src);
  c16_t hbuf[64] __attribute__((aligned(64)));
  c16_t gbuf[64] __attribute__((aligned(64)));

  for (int i = 0; i < 4; i++) {
    const svint16_t x0 = sve2_q15_half_shift(pg, svld1_s16(pg, (const int16_t *)(src + 4 * (i + 0))));
    const svint16_t x1 = sve2_q15_half_shift(pg, svld1_s16(pg, (const int16_t *)(src + 4 * (i + 4))));
    const svint16_t x2 = sve2_q15_half_shift(pg, svld1_s16(pg, (const int16_t *)(src + 4 * (i + 8))));
    const svint16_t x3 = sve2_q15_half_shift(pg, svld1_s16(pg, (const int16_t *)(src + 4 * (i + 12))));
    svint16_t y0, y1, y2, y3;
    sve2_dft4_butterfly_q15(x0, x1, x2, x3, &y0, &y1, &y2, &y3);
    svst1_s16(pg, (int16_t *)(hbuf + 4 * (4 * i + 0)), y0);
    svst1_s16(pg, (int16_t *)(hbuf + 4 * (4 * i + 1)), y1);
    svst1_s16(pg, (int16_t *)(hbuf + 4 * (4 * i + 2)), y2);
    svst1_s16(pg, (int16_t *)(hbuf + 4 * (4 * i + 3)), y3);
  }

  for (int k = 0; k < 4; k++) {
    const svint16_t h0 = svld1_s16(pg, (const int16_t *)(hbuf + 4 * (4 * 0 + k)));
    const svint16_t h1 = svld1_s16(pg, (const int16_t *)(hbuf + 4 * (4 + k)));
    const svint16_t h2 = svld1_s16(pg, (const int16_t *)(hbuf + 4 * (4 * 2 + k)));
    const svint16_t h3 = svld1_s16(pg, (const int16_t *)(hbuf + 4 * (4 * 3 + k)));
    const svint16_t b0 = sve2_q15_half_shift(pg, h0);
    const svint16_t b1 = sve2_dft64_c16_q15(pg, h1, k, 1);
    const svint16_t b2 = sve2_dft64_c16_q15(pg, h2, k, 2);
    const svint16_t b3 = sve2_dft64_c16_q15(pg, h3, k, 3);
    svint16_t y0, y1, y2, y3;
    sve2_dft4_butterfly_q15(b0, b1, b2, b3, &y0, &y1, &y2, &y3);
    svst1_s16(pg, (int16_t *)(gbuf + 4 * (k + 0)), y0);
    svst1_s16(pg, (int16_t *)(gbuf + 4 * (k + 4)), y1);
    svst1_s16(pg, (int16_t *)(gbuf + 4 * (k + 8)), y2);
    svst1_s16(pg, (int16_t *)(gbuf + 4 * (k + 12)), y3);
  }

  for (int i = 0; i < 16; i += 4) {
    const svint16_t w0 = svld1_s16(pg, g_sve2_dft64_special_tw.c64_q15[i + 0]);
    const svint16_t w1 = svld1_s16(pg, g_sve2_dft64_special_tw.c64_q15[i + 1]);
    const svint16_t w2 = svld1_s16(pg, g_sve2_dft64_special_tw.c64_q15[i + 2]);
    const svint16_t w3 = svld1_s16(pg, g_sve2_dft64_special_tw.c64_q15[i + 3]);
    const svint16_t g0 = svld1_s16(pg, (const int16_t *)(gbuf + 4 * (i + 0)));
    /* For i=0 the whole SIMD vector sees W64^0 / 2, so this is a pure shift. */
    const svint16_t b0 = (i == 0) ? sve2_q15_half_shift(pg, g0) : sve2_cmul_q15(g0, w0);
    const svint16_t b1 = sve2_cmul_q15(svld1_s16(pg, (const int16_t *)(gbuf + 4 * (i + 1))), w1);
    const svint16_t b2 = sve2_cmul_q15(svld1_s16(pg, (const int16_t *)(gbuf + 4 * (i + 2))), w2);
    const svint16_t b3 = sve2_cmul_q15(svld1_s16(pg, (const int16_t *)(gbuf + 4 * (i + 3))), w3);

    svint16_t t0, t1, t2, t3;
    /* The SVE2 production path is entered only after initialization has
     * confirmed VL=128, so no vector-length test is needed here. */
    sve2_dft64_transpose4_q15_128(b0, b1, b2, b3, &t0, &t1, &t2, &t3);

    svint16_t y0, y1, y2, y3;
    sve2_dft4_butterfly_q15(t0, t1, t2, t3, &y0, &y1, &y2, &y3);
    svst1_s16(pg, (int16_t *)(dst + i), y0);
    svst1_s16(pg, (int16_t *)(dst + 16 + i), y1);
    svst1_s16(pg, (int16_t *)(dst + 32 + i), y2);
    svst1_s16(pg, (int16_t *)(dst + 48 + i), y3);
  }

  /* Preserve the high-accuracy DC: only bin zero is
   * replaced; every other bin remains from the normal SIMD DFT64 path. */
  dst[0] = dc;
}

typedef struct {
  int initialized;
  int16_t w128_q15[16][8] __attribute__((aligned(64)));
} sve2_dft128_special_twiddle_t;

static sve2_dft128_special_twiddle_t g_sve2_dft128_special_tw;

static inline int16_t sve2_dft128_fused_twiddle_q15(float x)
{
  /* The radix-2 1/sqrt(2) and the first DFT64 stage 1/2 are folded
   * into one coefficient: 1/sqrt(8). */
  return q15_from_float(x / sqrtf(8.0f));
}

/* Prepare DFT128 radix-2 twiddles. */
static void sve2_dft128_special_prepare(void)
{
  sve2_dft128_special_twiddle_t *tw = &g_sve2_dft128_special_tw;
  if (tw->initialized)
    return;

  for (int b = 0; b < 16; b++) {
    for (int j = 0; j < 4; j++) {
      const int k = 4 * b + j;
      const float a = -2.0f * (float)M_PI * (float)k / 128.0f;
      tw->w128_q15[b][2 * j + 0] = sve2_dft128_fused_twiddle_q15(cosf(a));
      tw->w128_q15[b][2 * j + 1] = sve2_dft128_fused_twiddle_q15(sinf(a));
    }
  }

  tw->initialized = 1;
}

/* Finishes a VL=128 Q15 DFT64 after its first radix-4 stage has already been computed. */
SVE2_TARGET static void sve2_dft64_continue_after_first_r4_q15(const c16_t *hbuf, c16_t *dst)
{
  const svbool_t pg = svwhilelt_b16((uint64_t)0, (uint64_t)8);
  c16_t gbuf[64] __attribute__((aligned(64)));

  for (int k = 0; k < 4; k++) {
    const svint16_t h0 = svld1_s16(pg, (const int16_t *)(hbuf + 4 * (4 * 0 + k)));
    const svint16_t h1 = svld1_s16(pg, (const int16_t *)(hbuf + 4 * (4 + k)));
    const svint16_t h2 = svld1_s16(pg, (const int16_t *)(hbuf + 4 * (4 * 2 + k)));
    const svint16_t h3 = svld1_s16(pg, (const int16_t *)(hbuf + 4 * (4 * 3 + k)));
    const svint16_t b0 = sve2_q15_real_mul(h0, Q15_HALF);
    const svint16_t b1 = sve2_dft64_c16_q15(pg, h1, k, 1);
    const svint16_t b2 = sve2_dft64_c16_q15(pg, h2, k, 2);
    const svint16_t b3 = sve2_dft64_c16_q15(pg, h3, k, 3);
    svint16_t y0, y1, y2, y3;
    sve2_dft4_butterfly_q15(b0, b1, b2, b3, &y0, &y1, &y2, &y3);
    svst1_s16(pg, (int16_t *)(gbuf + 4 * (k + 0)), y0);
    svst1_s16(pg, (int16_t *)(gbuf + 4 * (k + 4)), y1);
    svst1_s16(pg, (int16_t *)(gbuf + 4 * (k + 8)), y2);
    svst1_s16(pg, (int16_t *)(gbuf + 4 * (k + 12)), y3);
  }

  for (int i = 0; i < 16; i += 4) {
    const svint16_t b0 =
        sve2_cmul_q15(svld1_s16(pg, (const int16_t *)(gbuf + 4 * (i + 0))), svld1_s16(pg, g_sve2_dft64_special_tw.c64_q15[i + 0]));
    const svint16_t b1 =
        sve2_cmul_q15(svld1_s16(pg, (const int16_t *)(gbuf + 4 * (i + 1))), svld1_s16(pg, g_sve2_dft64_special_tw.c64_q15[i + 1]));
    const svint16_t b2 =
        sve2_cmul_q15(svld1_s16(pg, (const int16_t *)(gbuf + 4 * (i + 2))), svld1_s16(pg, g_sve2_dft64_special_tw.c64_q15[i + 2]));
    const svint16_t b3 =
        sve2_cmul_q15(svld1_s16(pg, (const int16_t *)(gbuf + 4 * (i + 3))), svld1_s16(pg, g_sve2_dft64_special_tw.c64_q15[i + 3]));
    svint16_t t0, t1, t2, t3;
    sve2_dft64_transpose4_q15_128(b0, b1, b2, b3, &t0, &t1, &t2, &t3);
    svint16_t y0, y1, y2, y3;
    sve2_dft4_butterfly_q15(t0, t1, t2, t3, &y0, &y1, &y2, &y3);
    svst1_s16(pg, (int16_t *)(dst + i), y0);
    svst1_s16(pg, (int16_t *)(dst + 16 + i), y1);
    svst1_s16(pg, (int16_t *)(dst + 32 + i), y2);
    svst1_s16(pg, (int16_t *)(dst + 48 + i), y3);
  }
}

/* Runs DFT128 by fusing the radix-2 preparation with both DFT64 first stages. */
SVE2_TARGET static void sve2_dft128_q15_special_fused_st2(const c16_t *src, c16_t *dst)
{
  /* ISA/VL support was resolved once during initialization.  This function
   * is reached only by the cached SVE2 VL=128 dispatch. */
  const svbool_t pg = svwhilelt_b16((uint64_t)0, (uint64_t)8);
  const svbool_t pg32 = svptrue_b32();
  c16_t h_even[64] __attribute__((aligned(64)));
  c16_t h_odd[64] __attribute__((aligned(64)));
  c16_t a[64] __attribute__((aligned(64)));
  c16_t b[64] __attribute__((aligned(64)));

  for (int i = 0; i < 4; i++) {
    const int b0 = i + 0;
    const int b1 = i + 4;
    const int b2 = i + 8;
    const int b3 = i + 12;

    const svint16_t x00 = svld1_s16(pg, (const int16_t *)(src + 4 * b0));
    const svint16_t x01 = svld1_s16(pg, (const int16_t *)(src + 64 + 4 * b0));
    const svint16_t x10 = svld1_s16(pg, (const int16_t *)(src + 4 * b1));
    const svint16_t x11 = svld1_s16(pg, (const int16_t *)(src + 64 + 4 * b1));
    const svint16_t x20 = svld1_s16(pg, (const int16_t *)(src + 4 * b2));
    const svint16_t x21 = svld1_s16(pg, (const int16_t *)(src + 64 + 4 * b2));
    const svint16_t x30 = svld1_s16(pg, (const int16_t *)(src + 4 * b3));
    const svint16_t x31 = svld1_s16(pg, (const int16_t *)(src + 64 + 4 * b3));

    /* 1/sqrt(8) already includes the 1/sqrt(2) radix-2 normalization and
     * the 1/2 normalization of the first radix-4 stage in each DFT64 child.
     * This removes a second Q15 multiply and its extra rounding. */
    const svint16_t e0 = sve2_q15_real_mul(svqadd_s16_x(pg, x00, x01), Q15_INV_SQRT8);
    const svint16_t e1 = sve2_q15_real_mul(svqadd_s16_x(pg, x10, x11), Q15_INV_SQRT8);
    const svint16_t e2 = sve2_q15_real_mul(svqadd_s16_x(pg, x20, x21), Q15_INV_SQRT8);
    const svint16_t e3 = sve2_q15_real_mul(svqadd_s16_x(pg, x30, x31), Q15_INV_SQRT8);

    /* The odd-branch twiddle table already contains W128^k/sqrt(8), so the
     * twiddle multiplication also performs all required scaling once. */
    const svint16_t o0 = sve2_cmul_q15(svqsub_s16_x(pg, x00, x01), svld1_s16(pg, g_sve2_dft128_special_tw.w128_q15[b0]));
    const svint16_t o1 = sve2_cmul_q15(svqsub_s16_x(pg, x10, x11), svld1_s16(pg, g_sve2_dft128_special_tw.w128_q15[b1]));
    const svint16_t o2 = sve2_cmul_q15(svqsub_s16_x(pg, x20, x21), svld1_s16(pg, g_sve2_dft128_special_tw.w128_q15[b2]));
    const svint16_t o3 = sve2_cmul_q15(svqsub_s16_x(pg, x30, x31), svld1_s16(pg, g_sve2_dft128_special_tw.w128_q15[b3]));

    svint16_t ey0, ey1, ey2, ey3;
    svint16_t oy0, oy1, oy2, oy3;
    sve2_dft4_butterfly_q15(e0, e1, e2, e3, &ey0, &ey1, &ey2, &ey3);
    sve2_dft4_butterfly_q15(o0, o1, o2, o3, &oy0, &oy1, &oy2, &oy3);

    svst1_s16(pg, (int16_t *)(h_even + 4 * (4 * i + 0)), ey0);
    svst1_s16(pg, (int16_t *)(h_even + 4 * (4 * i + 1)), ey1);
    svst1_s16(pg, (int16_t *)(h_even + 4 * (4 * i + 2)), ey2);
    svst1_s16(pg, (int16_t *)(h_even + 4 * (4 * i + 3)), ey3);
    svst1_s16(pg, (int16_t *)(h_odd + 4 * (4 * i + 0)), oy0);
    svst1_s16(pg, (int16_t *)(h_odd + 4 * (4 * i + 1)), oy1);
    svst1_s16(pg, (int16_t *)(h_odd + 4 * (4 * i + 2)), oy2);
    svst1_s16(pg, (int16_t *)(h_odd + 4 * (4 * i + 3)), oy3);
  }

  sve2_dft64_continue_after_first_r4_q15(h_even, a);
  sve2_dft64_continue_after_first_r4_q15(h_odd, b);

  for (int k = 0; k < 64; k += 4) {
    const svuint32_t va = svreinterpret_u32_s16(svld1_s16(pg, (const int16_t *)(a + k)));
    const svuint32_t vb = svreinterpret_u32_s16(svld1_s16(pg, (const int16_t *)(b + k)));
    svst2_u32(pg32, (uint32_t *)(dst + 2 * k), svcreate2_u32(va, vb));
  }
}

typedef struct {
  int initialized;
  int16_t w256_q15[3][16][8] __attribute__((aligned(64)));
} sve2_dft256_special_twiddle_t;

static sve2_dft256_special_twiddle_t g_sve2_dft256_special_tw;

static inline int16_t sve2_dft256_q15_twiddle_scaled(float x)
{
  const int16_t q = q15_from_float(x);
  return (int16_t)(q / 2);
}

/* Prepare DFT256 radix-4 twiddles. */
static void sve2_dft256_special_prepare(void)
{
  sve2_dft256_special_twiddle_t *tw = &g_sve2_dft256_special_tw;
  if (tw->initialized)
    return;

  for (int branch = 1; branch <= 3; branch++) {
    for (int blk = 0; blk < 16; blk++) {
      for (int j = 0; j < 4; j++) {
        const int k = 4 * blk + j;
        const float a = -2.0f * (float)M_PI * (float)(branch * k) / 256.0f;
        tw->w256_q15[branch - 1][blk][2 * j + 0] = sve2_dft256_q15_twiddle_scaled(cosf(a));
        tw->w256_q15[branch - 1][blk][2 * j + 1] = sve2_dft256_q15_twiddle_scaled(sinf(a));
      }
    }
  }

  tw->initialized = 1;
}

/* Runs the unitary Q15 DFT256 as a radix-4 front end followed by four specialized DFT64 kernels. */
SVE2_TARGET static void sve2_dft256_q15_special_r4x64(const c16_t *src, c16_t *dst)
{
  const svbool_t pg = svptrue_b16();
  const svbool_t pg32 = svptrue_b32();
  c16_t b0[64] __attribute__((aligned(64)));
  c16_t b1[64] __attribute__((aligned(64)));
  c16_t b2[64] __attribute__((aligned(64)));
  c16_t b3[64] __attribute__((aligned(64)));
  c16_t y0[64] __attribute__((aligned(64)));
  c16_t y1[64] __attribute__((aligned(64)));
  c16_t y2[64] __attribute__((aligned(64)));
  c16_t y3[64] __attribute__((aligned(64)));

  for (int blk = 0; blk < 16; blk++) {
    const int off = 4 * blk;
    const svint16_t x0 = svld1_s16(pg, (const int16_t *)(src + off));
    const svint16_t x1 = svld1_s16(pg, (const int16_t *)(src + 64 + off));
    const svint16_t x2 = svld1_s16(pg, (const int16_t *)(src + 128 + off));
    const svint16_t x3 = svld1_s16(pg, (const int16_t *)(src + 192 + off));

    svint16_t r0, r1, r2, r3;
    sve2_dft4_butterfly_q15(x0, x1, x2, x3, &r0, &r1, &r2, &r3);
    r0 = sve2_q15_real_mul(r0, Q15_HALF);
    r1 = sve2_cmul_q15(r1, svld1_s16(pg, g_sve2_dft256_special_tw.w256_q15[0][blk]));
    r2 = sve2_cmul_q15(r2, svld1_s16(pg, g_sve2_dft256_special_tw.w256_q15[1][blk]));
    r3 = sve2_cmul_q15(r3, svld1_s16(pg, g_sve2_dft256_special_tw.w256_q15[2][blk]));
    svst1_s16(pg, (int16_t *)(b0 + off), r0);
    svst1_s16(pg, (int16_t *)(b1 + off), r1);
    svst1_s16(pg, (int16_t *)(b2 + off), r2);
    svst1_s16(pg, (int16_t *)(b3 + off), r3);
  }

  sve2_dft64_q15_special(b0, y0);
  sve2_dft64_q15_special(b1, y1);
  sve2_dft64_q15_special(b2, y2);
  sve2_dft64_q15_special(b3, y3);

  for (int k = 0; k < 64; k += 4) {
    const svuint32_t v0 = svreinterpret_u32_s16(svld1_s16(pg, (const int16_t *)(y0 + k)));
    const svuint32_t v1 = svreinterpret_u32_s16(svld1_s16(pg, (const int16_t *)(y1 + k)));
    const svuint32_t v2 = svreinterpret_u32_s16(svld1_s16(pg, (const int16_t *)(y2 + k)));
    const svuint32_t v3 = svreinterpret_u32_s16(svld1_s16(pg, (const int16_t *)(y3 + k)));
    svst4_u32(pg32, (uint32_t *)(dst + 4 * k), svcreate4_u32(v0, v1, v2, v3));
  }
}

/* Fixed-VL128 SVE2 final-layout helpers.  They transpose complex lanes in
 * registers instead of falling back to scalar r-way interleave loops. */
SVE2_TARGET static inline void sve2_transpose4x4_u32(svuint32_t v0,
                                                     svuint32_t v1,
                                                     svuint32_t v2,
                                                     svuint32_t v3,
                                                     svuint32_t *o0,
                                                     svuint32_t *o1,
                                                     svuint32_t *o2,
                                                     svuint32_t *o3)
{
  const svuint32_t a01 = svzip1_u32(v0, v1);
  const svuint32_t a23 = svzip1_u32(v2, v3);
  const svuint32_t b01 = svzip2_u32(v0, v1);
  const svuint32_t b23 = svzip2_u32(v2, v3);
  *o0 = svreinterpret_u32_u64(svzip1_u64(svreinterpret_u64_u32(a01), svreinterpret_u64_u32(a23)));
  *o1 = svreinterpret_u32_u64(svzip2_u64(svreinterpret_u64_u32(a01), svreinterpret_u64_u32(a23)));
  *o2 = svreinterpret_u32_u64(svzip1_u64(svreinterpret_u64_u32(b01), svreinterpret_u64_u32(b23)));
  *o3 = svreinterpret_u32_u64(svzip2_u64(svreinterpret_u64_u32(b01), svreinterpret_u64_u32(b23)));
}

SVE2_TARGET static inline void sve2_store_interleave8_q15(const c16_t *y0,
                                                          const c16_t *y1,
                                                          const c16_t *y2,
                                                          const c16_t *y3,
                                                          const c16_t *y4,
                                                          const c16_t *y5,
                                                          const c16_t *y6,
                                                          const c16_t *y7,
                                                          c16_t *dst,
                                                          int M)
{
  const svbool_t pg16 = svptrue_b16();
  const svbool_t pg32 = svptrue_b32();
  for (int k = 0; k < M; k += 4) {
    const svuint32_t v0 = svreinterpret_u32_s16(svld1_s16(pg16, (const int16_t *)(y0 + k)));
    const svuint32_t v1 = svreinterpret_u32_s16(svld1_s16(pg16, (const int16_t *)(y1 + k)));
    const svuint32_t v2 = svreinterpret_u32_s16(svld1_s16(pg16, (const int16_t *)(y2 + k)));
    const svuint32_t v3 = svreinterpret_u32_s16(svld1_s16(pg16, (const int16_t *)(y3 + k)));
    const svuint32_t v4 = svreinterpret_u32_s16(svld1_s16(pg16, (const int16_t *)(y4 + k)));
    const svuint32_t v5 = svreinterpret_u32_s16(svld1_s16(pg16, (const int16_t *)(y5 + k)));
    const svuint32_t v6 = svreinterpret_u32_s16(svld1_s16(pg16, (const int16_t *)(y6 + k)));
    const svuint32_t v7 = svreinterpret_u32_s16(svld1_s16(pg16, (const int16_t *)(y7 + k)));
    svuint32_t a0, a1, a2, a3, b0, b1, b2, b3;
    sve2_transpose4x4_u32(v0, v1, v2, v3, &a0, &a1, &a2, &a3);
    sve2_transpose4x4_u32(v4, v5, v6, v7, &b0, &b1, &b2, &b3);
    uint32_t *p = (uint32_t *)(dst + 8 * k);
    svst1_u32(pg32, p + 0, a0);
    svst1_u32(pg32, p + 4, b0);
    svst1_u32(pg32, p + 8, a1);
    svst1_u32(pg32, p + 12, b1);
    svst1_u32(pg32, p + 16, a2);
    svst1_u32(pg32, p + 20, b2);
    svst1_u32(pg32, p + 24, a3);
    svst1_u32(pg32, p + 28, b3);
  }
}

typedef struct {
  int initialized;

  int16_t r8_q15[7][16][8] __attribute__((aligned(64)));
} sve2_dft512_special_twiddle_t;

static sve2_dft512_special_twiddle_t g_sve2_dft512_special_tw;

static inline int16_t sve2_dft512_q15_twiddle_scaled(float x, int radix)
{
  const int16_t q = q15_from_float(x);
  if (radix == 2)
    return (int16_t)((float)q / sqrtf(2.0f));
  if (radix == 4)
    return (int16_t)(q / 2);
  if (radix == 8)
    return (int16_t)((float)q / sqrtf(8.0f));
  abort();
  __builtin_unreachable();
}

/* Prepare only the frozen DFT512 winner twiddles (R8 x DFT64).
 * Called lazily while publishing the cached general plan. */
static void sve2_dft512_special_prepare(void)
{
  sve2_dft512_special_twiddle_t *tw = &g_sve2_dft512_special_tw;
  if (tw->initialized)
    return;

  for (int branch = 1; branch <= 7; branch++) {
    for (int blk = 0; blk < 16; blk++) {
      for (int j = 0; j < 4; j++) {
        const int k = 4 * blk + j;
        const float a = -2.0f * (float)M_PI * (float)(branch * k) / 512.0f;
        tw->r8_q15[branch - 1][blk][2 * j + 0] = sve2_dft512_q15_twiddle_scaled(cosf(a), 8);
        tw->r8_q15[branch - 1][blk][2 * j + 1] = sve2_dft512_q15_twiddle_scaled(sinf(a), 8);
      }
    }
  }

  tw->initialized = 1;
}

/* Fixed W8 constants used only inside the radix-8 front-end butterfly. */
static const int16_t g_sve2_w8_1_q15[8] __attribute__((aligned(16))) = {23170, -23170, 23170, -23170, 23170, -23170, 23170, -23170};
static const int16_t g_sve2_w8_3_q15[8]
    __attribute__((aligned(16))) = {-23170, -23170, -23170, -23170, -23170, -23170, -23170, -23170};

SVE2_TARGET static inline void sve2_dft8_q15(svint16_t x0,
                                             svint16_t x1,
                                             svint16_t x2,
                                             svint16_t x3,
                                             svint16_t x4,
                                             svint16_t x5,
                                             svint16_t x6,
                                             svint16_t x7,
                                             svint16_t *y0,
                                             svint16_t *y1,
                                             svint16_t *y2,
                                             svint16_t *y3,
                                             svint16_t *y4,
                                             svint16_t *y5,
                                             svint16_t *y6,
                                             svint16_t *y7)
{
  const svbool_t pg = svptrue_b16();
  svint16_t e0, e1, e2, e3;
  svint16_t o0, o1, o2, o3;
  sve2_dft4_butterfly_q15(x0, x2, x4, x6, &e0, &e1, &e2, &e3);
  sve2_dft4_butterfly_q15(x1, x3, x5, x7, &o0, &o1, &o2, &o3);

  const svint16_t w1 = svld1_s16(pg, g_sve2_w8_1_q15);
  const svint16_t w3 = svld1_s16(pg, g_sve2_w8_3_q15);
  const svint16_t t1 = sve2_cmul_q15(o1, w1);
  const svint16_t t2 = svqcadd_s16(svdup_n_s16(0), o2, 270);
  const svint16_t t3 = sve2_cmul_q15(o3, w3);

  *y0 = svqadd_s16_x(pg, e0, o0);
  *y4 = svqsub_s16_x(pg, e0, o0);
  *y1 = svqadd_s16_x(pg, e1, t1);
  *y5 = svqsub_s16_x(pg, e1, t1);
  *y2 = svqadd_s16_x(pg, e2, t2);
  *y6 = svqsub_s16_x(pg, e2, t2);
  *y3 = svqadd_s16_x(pg, e3, t3);
  *y7 = svqsub_s16_x(pg, e3, t3);
}

/* Complete unitary radix-4 stage. The 1/2 factor is applied before the raw
 * butterfly; callers that already fold this factor into twiddles use
 * sve2_dft4_butterfly_q15() directly.
 */
SVE2_TARGET static inline void sve2_dft4_unitary_q15(svint16_t x0,
                                                     svint16_t x1,
                                                     svint16_t x2,
                                                     svint16_t x3,
                                                     svint16_t *y0,
                                                     svint16_t *y1,
                                                     svint16_t *y2,
                                                     svint16_t *y3)
{
  const svbool_t pg = svptrue_b16();
  x0 = sve2_q15_half_shift(pg, x0);
  x1 = sve2_q15_half_shift(pg, x1);
  x2 = sve2_q15_half_shift(pg, x2);
  x3 = sve2_q15_half_shift(pg, x3);
  sve2_dft4_butterfly_q15(x0, x1, x2, x3, y0, y1, y2, y3);
}

/* Unitary radix-8 built as two DFT4 children followed by a radix-2 combine.
 * The DFT4 children contribute 1/2 and the combine contributes 1/sqrt(2),
 * giving the required 1/sqrt(8) normalization.
 */
SVE2_TARGET static inline void sve2_dft8_unitary_q15(svint16_t x0,
                                                     svint16_t x1,
                                                     svint16_t x2,
                                                     svint16_t x3,
                                                     svint16_t x4,
                                                     svint16_t x5,
                                                     svint16_t x6,
                                                     svint16_t x7,
                                                     svint16_t *y0,
                                                     svint16_t *y1,
                                                     svint16_t *y2,
                                                     svint16_t *y3,
                                                     svint16_t *y4,
                                                     svint16_t *y5,
                                                     svint16_t *y6,
                                                     svint16_t *y7)
{
  const svbool_t pg = svptrue_b16();
  static const int16_t w1_over_sqrt2_q15[8]
      __attribute__((aligned(16))) = {Q15_HALF, -Q15_HALF, Q15_HALF, -Q15_HALF, Q15_HALF, -Q15_HALF, Q15_HALF, -Q15_HALF};
  static const int16_t w3_over_sqrt2_q15[8]
      __attribute__((aligned(16))) = {-Q15_HALF, -Q15_HALF, -Q15_HALF, -Q15_HALF, -Q15_HALF, -Q15_HALF, -Q15_HALF, -Q15_HALF};

  svint16_t e0, e1, e2, e3;
  svint16_t o0, o1, o2, o3;
  sve2_dft4_unitary_q15(x0, x2, x4, x6, &e0, &e1, &e2, &e3);
  sve2_dft4_unitary_q15(x1, x3, x5, x7, &o0, &o1, &o2, &o3);

  const svint16_t e0s = sve2_q15_real_mul(e0, Q15_INV_SQRT2);
  const svint16_t e1s = sve2_q15_real_mul(e1, Q15_INV_SQRT2);
  const svint16_t e2s = sve2_q15_real_mul(e2, Q15_INV_SQRT2);
  const svint16_t e3s = sve2_q15_real_mul(e3, Q15_INV_SQRT2);

  const svint16_t t0 = sve2_q15_real_mul(o0, Q15_INV_SQRT2);
  const svint16_t t1 = sve2_cmul_q15(o1, svld1_s16(pg, w1_over_sqrt2_q15));
  const svint16_t t2 = svqcadd_s16(svdup_n_s16(0), sve2_q15_real_mul(o2, Q15_INV_SQRT2), 270);
  const svint16_t t3 = sve2_cmul_q15(o3, svld1_s16(pg, w3_over_sqrt2_q15));

  *y0 = svqadd_s16_x(pg, e0s, t0);
  *y4 = svqsub_s16_x(pg, e0s, t0);
  *y1 = svqadd_s16_x(pg, e1s, t1);
  *y5 = svqsub_s16_x(pg, e1s, t1);
  *y2 = svqadd_s16_x(pg, e2s, t2);
  *y6 = svqsub_s16_x(pg, e2s, t2);
  *y3 = svqadd_s16_x(pg, e3s, t3);
  *y7 = svqsub_s16_x(pg, e3s, t3);
}

SVE2_TARGET static inline void sve2_idft8_unitary_q15(svint16_t x0,
                                                      svint16_t x1,
                                                      svint16_t x2,
                                                      svint16_t x3,
                                                      svint16_t x4,
                                                      svint16_t x5,
                                                      svint16_t x6,
                                                      svint16_t x7,
                                                      svint16_t *y0,
                                                      svint16_t *y1,
                                                      svint16_t *y2,
                                                      svint16_t *y3,
                                                      svint16_t *y4,
                                                      svint16_t *y5,
                                                      svint16_t *y6,
                                                      svint16_t *y7)
{
  svint16_t f0, f1, f2, f3, f4, f5, f6, f7;
  sve2_dft8_unitary_q15(x0, x1, x2, x3, x4, x5, x6, x7, &f0, &f1, &f2, &f3, &f4, &f5, &f6, &f7);
  *y0 = f0;
  *y1 = f7;
  *y2 = f6;
  *y3 = f5;
  *y4 = f4;
  *y5 = f3;
  *y6 = f2;
  *y7 = f1;
}

typedef struct {
  int initialized;
  int16_t w8_q15[8] __attribute__((aligned(64)));
  int16_t w12_q15[2][8] __attribute__((aligned(64)));
  /* W12^(r*k)/sqrt(3), quantize first then scale, for the twiddle-only Q15 terminal. */
  int16_t w12_scaled_q15[2][8] __attribute__((aligned(64)));
  int16_t w12_scaled_q15_inv[2][8] __attribute__((aligned(64)));
  int16_t w16_q15[3][8] __attribute__((aligned(64)));
  int16_t w16_q15_inv[3][8] __attribute__((aligned(64)));
  int16_t w24_q15[2][16] __attribute__((aligned(64)));
  int16_t w32_q15[7][8] __attribute__((aligned(64)));
  int16_t w32_q15_inv[7][8] __attribute__((aligned(64)));
} sve2_tiny_twiddle_t;

static sve2_tiny_twiddle_t g_sve2_tiny_tw;

static void sve2_tiny_fill_tw_q15(int16_t *wq, int N, int branch, int M)
{
  for (int k = 0; k < M; ++k) {
    const float a = -2.0f * (float)M_PI * (float)(branch * k) / (float)N;
    wq[2 * k + 0] = q15_from_float(cosf(a));
    wq[2 * k + 1] = q15_from_float(sinf(a));
  }
}

static void sve2_tiny_special_prepare(void)
{
  sve2_tiny_twiddle_t *tw = &g_sve2_tiny_tw;
  if (tw->initialized)
    return;
  sve2_tiny_fill_tw_q15(tw->w8_q15, 8, 1, 4);
  for (int b = 1; b < 3; b++)
    sve2_tiny_fill_tw_q15(tw->w12_q15[b - 1], 12, b, 4);
  for (int b = 0; b < 2; b++) {
    for (int k = 0; k < 4; k++) {
      const int re = 2 * k;
      const int im = re + 1;
      tw->w12_scaled_q15[b][re] = sve2_q15_scale_coeff(tw->w12_q15[b][re], 3);
      tw->w12_scaled_q15[b][im] = sve2_q15_scale_coeff(tw->w12_q15[b][im], 3);
      tw->w12_scaled_q15_inv[b][re] = tw->w12_scaled_q15[b][re];
      tw->w12_scaled_q15_inv[b][im] = (int16_t)-tw->w12_scaled_q15[b][im];
    }
  }
  for (int b = 1; b < 4; b++) {
    sve2_tiny_fill_tw_q15(tw->w16_q15[b - 1], 16, b, 4);
    for (int k = 0; k < 4; k++) {
      tw->w16_q15_inv[b - 1][2 * k + 0] = tw->w16_q15[b - 1][2 * k + 0];
      tw->w16_q15_inv[b - 1][2 * k + 1] = (int16_t)-tw->w16_q15[b - 1][2 * k + 1];
    }
  }
  for (int b = 1; b < 3; b++)
    sve2_tiny_fill_tw_q15(tw->w24_q15[b - 1], 24, b, 8);
  for (int b = 1; b < 8; b++) {
    sve2_tiny_fill_tw_q15(tw->w32_q15[b - 1], 32, b, 4);
    for (int k = 0; k < 4; k++) {
      tw->w32_q15_inv[b - 1][2 * k + 0] = tw->w32_q15[b - 1][2 * k + 0];
      tw->w32_q15_inv[b - 1][2 * k + 1] = (int16_t)-tw->w32_q15[b - 1][2 * k + 1];
    }
  }
  tw->initialized = 1;
}

static const uint16_t g_sve2_tiny_q15_rot2[8] __attribute__((aligned(16))) = {4, 5, 6, 7, 0, 1, 2, 3};
static const uint16_t g_sve2_tiny_q15_swap[8] __attribute__((aligned(16))) = {2, 3, 0, 1, 6, 7, 4, 5};
static const uint16_t g_sve2_tiny_q15_take_ac[8] __attribute__((aligned(16))) = {0, 1, 8, 9, 0, 1, 8, 9};
static const uint16_t g_sve2_tiny_q15_final[8] __attribute__((aligned(16))) = {0, 1, 2, 3, 8, 9, 10, 11};

/* Direct unitary DFT4. With VL=128, one SVE register contains the four
 * complex Q15 inputs. The initial rounded shift implements 1/sqrt(4)=1/2;
 * table permutations then form the four outputs without scalar extraction.
 */
SVE2_TARGET static inline svint16_t sve2_dft4_direct_packed_q15(svint16_t vin)
{
  const svbool_t pg = svptrue_b16();
  const svuint16_t irot = svld1_u16(pg, g_sve2_tiny_q15_rot2);
  const svuint16_t iswp = svld1_u16(pg, g_sve2_tiny_q15_swap);
  const svuint16_t itake = svld1_u16(pg, g_sve2_tiny_q15_take_ac);
  const svuint16_t ifin = svld1_u16(pg, g_sve2_tiny_q15_final);
  const svint16_t v = svrshr_n_s16_x(pg, vin, 1);
  const svint16_t vr = svtbl_s16(v, irot);
  const svint16_t sum = svqadd_s16_x(pg, v, vr);
  const svint16_t dif = svqsub_s16_x(pg, v, vr);
  const svint16_t ss = svtbl_s16(sum, iswp);
  const svint16_t ds = svtbl_s16(dif, iswp);
  const svint16_t a = svqadd_s16_x(pg, sum, ss);
  const svint16_t b = svqsub_s16_x(pg, sum, ss);
  const svint16_t c = svqcadd_s16(dif, ds, 270);
  const svint16_t e = svqcadd_s16(dif, ds, 90);
  const svint16_t ac = svtbl2_s16(svcreate2_s16(a, c), itake);
  const svint16_t be = svtbl2_s16(svcreate2_s16(b, e), itake);
  return svtbl2_s16(svcreate2_s16(ac, be), ifin);
}

SVE2_TARGET static inline void sve2_dft4_direct_q15(const c16_t *src, c16_t *dst)
{
  const svbool_t pg = svptrue_b16();
  const svint16_t vin = svld1_s16(pg, (const int16_t *)src);
  svst1_s16(pg, (int16_t *)dst, sve2_dft4_direct_packed_q15(vin));
}
static const uint16_t g_sve2_tiny_q15_idft4_order[8] __attribute__((aligned(16))) = {0, 1, 6, 7, 4, 5, 2, 3};

SVE2_TARGET static inline svint16_t sve2_idft4_direct_packed_q15(svint16_t vin)
{
  const svbool_t pg = svptrue_b16();
  const svint16_t f = sve2_dft4_direct_packed_q15(vin);
  return svtbl_s16(f, svld1_u16(pg, g_sve2_tiny_q15_idft4_order));
}


SVE2_TARGET static void sve2_dft8_q15_special(const c16_t *src, c16_t *dst)
{
  const svbool_t pg16 = svptrue_b16(), pg32 = svptrue_b32();

  /* DFT8 = R2 x DFT4. The R2 stage is normalized by 1/sqrt(2);
   * each DFT4 child contributes 1/2, so the full transform is unitary. */
  const svint16_t x0 = sve2_q15_real_mul(svld1_s16(pg16, (const int16_t *)(src + 0)), Q15_INV_SQRT2);
  const svint16_t x1 = sve2_q15_real_mul(svld1_s16(pg16, (const int16_t *)(src + 4)), Q15_INV_SQRT2);
  const svint16_t sum = svqadd_s16_x(pg16, x0, x1);
  const svint16_t dif = svqsub_s16_x(pg16, x0, x1);
  const svint16_t odd = sve2_cmul_q15(dif, svld1_s16(pg16, g_sve2_tiny_tw.w8_q15));

  const svint16_t y0 = sve2_dft4_direct_packed_q15(sum);
  const svint16_t y1 = sve2_dft4_direct_packed_q15(odd);
  svst2_u32(pg32, (uint32_t *)dst, svcreate2_u32(svreinterpret_u32_s16(y0), svreinterpret_u32_s16(y1)));
}

/* DFT12 = R3 x DFT4. The R3 normalization is folded into branch 0 and
 * W12 twiddles; the three DFT4 children stay register-resident. */
SVE2_TARGET static inline void sve2_dft12_q15_vectors(const c16_t *src,
                                                        svint16_t *y0,
                                                        svint16_t *y1,
                                                        svint16_t *y2)
{
  const svbool_t pg16 = svptrue_b16();
  const svint16_t x0 = svld1_s16(pg16, (const int16_t *)(src + 0));
  const svint16_t x1 = svld1_s16(pg16, (const int16_t *)(src + 4));
  const svint16_t x2 = svld1_s16(pg16, (const int16_t *)(src + 8));
  svint16_t z0, z1, z2;
  sve2_dft3_q15(x0, x1, x2, &z0, &z1, &z2);
  z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT3);
  z1 = sve2_cmul_q15(z1, svld1_s16(pg16, g_sve2_tiny_tw.w12_scaled_q15[0]));
  z2 = sve2_cmul_q15(z2, svld1_s16(pg16, g_sve2_tiny_tw.w12_scaled_q15[1]));
  *y0 = sve2_dft4_direct_packed_q15(z0);
  *y1 = sve2_dft4_direct_packed_q15(z1);
  *y2 = sve2_dft4_direct_packed_q15(z2);
}

SVE2_TARGET static inline void sve2_idft12_q15_vectors(const c16_t *src,
                                                         svint16_t *y0,
                                                         svint16_t *y1,
                                                         svint16_t *y2)
{
  const svbool_t pg16 = svptrue_b16();
  const svint16_t x0 = svld1_s16(pg16, (const int16_t *)(src + 0));
  const svint16_t x1 = svld1_s16(pg16, (const int16_t *)(src + 4));
  const svint16_t x2 = svld1_s16(pg16, (const int16_t *)(src + 8));
  svint16_t z0, z1, z2;
  sve2_idft3_q15(x0, x1, x2, &z0, &z1, &z2);
  z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT3);
  z1 = sve2_cmul_q15(z1, svld1_s16(pg16, g_sve2_tiny_tw.w12_scaled_q15_inv[0]));
  z2 = sve2_cmul_q15(z2, svld1_s16(pg16, g_sve2_tiny_tw.w12_scaled_q15_inv[1]));
  *y0 = sve2_idft4_direct_packed_q15(z0);
  *y1 = sve2_idft4_direct_packed_q15(z1);
  *y2 = sve2_idft4_direct_packed_q15(z2);
}

SVE2_TARGET static void sve2_dft12_q15_special(const c16_t *src, c16_t *dst)
{
  const svbool_t pg32 = svptrue_b32();
  svint16_t y0, y1, y2;
  sve2_dft12_q15_vectors(src, &y0, &y1, &y2);
  svst3_u32(pg32,
            (uint32_t *)dst,
            svcreate3_u32(svreinterpret_u32_s16(y0), svreinterpret_u32_s16(y1), svreinterpret_u32_s16(y2)));
}

SVE2_TARGET static void sve2_idft12_q15_special(const c16_t *src, c16_t *dst)
{
  const svbool_t pg32 = svptrue_b32();
  svint16_t y0, y1, y2;
  sve2_idft12_q15_vectors(src, &y0, &y1, &y2);
  svst3_u32(pg32,
            (uint32_t *)dst,
            svcreate3_u32(svreinterpret_u32_s16(y0), svreinterpret_u32_s16(y1), svreinterpret_u32_s16(y2)));
}

SVE2_TARGET static inline void sve2_parent_scatter_q15(svint16_t v,
                                                        c16_t *dst,
                                                        int base,
                                                        int step)
{
  const svbool_t pg32 = svptrue_b32();
  const svuint32_t idx = svindex_u32((uint32_t)base, (uint32_t)step);
  svst1_scatter_u32index_u32(pg32, (uint32_t *)dst, idx, svreinterpret_u32_s16(v));
}

SVE2_TARGET static inline void sve2_dft12_q15_parent_scatter(const c16_t *src,
                                                              c16_t *dst,
                                                              int parent_radix,
                                                              int parent_branch)
{
  svint16_t y0, y1, y2;
  sve2_dft12_q15_vectors(src, &y0, &y1, &y2);
  const int step = 3 * parent_radix;
  sve2_parent_scatter_q15(y0, dst, parent_branch + 0 * parent_radix, step);
  sve2_parent_scatter_q15(y1, dst, parent_branch + 1 * parent_radix, step);
  sve2_parent_scatter_q15(y2, dst, parent_branch + 2 * parent_radix, step);
}

SVE2_TARGET static inline void sve2_idft12_q15_parent_scatter(const c16_t *src,
                                                               c16_t *dst,
                                                               int parent_radix,
                                                               int parent_branch)
{
  svint16_t y0, y1, y2;
  sve2_idft12_q15_vectors(src, &y0, &y1, &y2);
  const int step = 3 * parent_radix;
  sve2_parent_scatter_q15(y0, dst, parent_branch + 0 * parent_radix, step);
  sve2_parent_scatter_q15(y1, dst, parent_branch + 1 * parent_radix, step);
  sve2_parent_scatter_q15(y2, dst, parent_branch + 2 * parent_radix, step);
}

SVE2_TARGET static inline void sve2_dft16_q15_vectors(const c16_t *src,
                                                        svint16_t *y0,
                                                        svint16_t *y1,
                                                        svint16_t *y2,
                                                        svint16_t *y3)
{
  const svbool_t pg16 = svptrue_b16();

  /* DFT16 = R4 x DFT4. Parent and child each contribute 1/2, giving
   * 1/4 = 1/sqrt(16). */
  const svint16_t x0 = sve2_q15_half_shift(pg16, svld1_s16(pg16, (const int16_t *)(src + 0)));
  const svint16_t x1 = sve2_q15_half_shift(pg16, svld1_s16(pg16, (const int16_t *)(src + 4)));
  const svint16_t x2 = sve2_q15_half_shift(pg16, svld1_s16(pg16, (const int16_t *)(src + 8)));
  const svint16_t x3 = sve2_q15_half_shift(pg16, svld1_s16(pg16, (const int16_t *)(src + 12)));

  svint16_t z0, z1, z2, z3;
  sve2_dft4_butterfly_q15(x0, x1, x2, x3, &z0, &z1, &z2, &z3);
  z1 = sve2_cmul_q15(z1, svld1_s16(pg16, g_sve2_tiny_tw.w16_q15[0]));
  z2 = sve2_cmul_q15(z2, svld1_s16(pg16, g_sve2_tiny_tw.w16_q15[1]));
  z3 = sve2_cmul_q15(z3, svld1_s16(pg16, g_sve2_tiny_tw.w16_q15[2]));

  *y0 = sve2_dft4_direct_packed_q15(z0);
  *y1 = sve2_dft4_direct_packed_q15(z1);
  *y2 = sve2_dft4_direct_packed_q15(z2);
  *y3 = sve2_dft4_direct_packed_q15(z3);
}

SVE2_TARGET static inline void sve2_idft16_q15_vectors(const c16_t *src,
                                                         svint16_t *y0,
                                                         svint16_t *y1,
                                                         svint16_t *y2,
                                                         svint16_t *y3)
{
  const svbool_t pg16 = svptrue_b16();
  const svint16_t x0 = sve2_q15_half_shift(pg16, svld1_s16(pg16, (const int16_t *)(src + 0)));
  const svint16_t x1 = sve2_q15_half_shift(pg16, svld1_s16(pg16, (const int16_t *)(src + 4)));
  const svint16_t x2 = sve2_q15_half_shift(pg16, svld1_s16(pg16, (const int16_t *)(src + 8)));
  const svint16_t x3 = sve2_q15_half_shift(pg16, svld1_s16(pg16, (const int16_t *)(src + 12)));

  /* IDFT4 is the unitary DFT4 with bins 1 and 3 exchanged. */
  svint16_t f0, f1, f2, f3;
  sve2_dft4_butterfly_q15(x0, x1, x2, x3, &f0, &f1, &f2, &f3);
  svint16_t z0 = f0;
  svint16_t z1 = f3;
  svint16_t z2 = f2;
  svint16_t z3 = f1;
  z1 = sve2_cmul_q15(z1, svld1_s16(pg16, g_sve2_tiny_tw.w16_q15_inv[0]));
  z2 = sve2_cmul_q15(z2, svld1_s16(pg16, g_sve2_tiny_tw.w16_q15_inv[1]));
  z3 = sve2_cmul_q15(z3, svld1_s16(pg16, g_sve2_tiny_tw.w16_q15_inv[2]));

  *y0 = sve2_idft4_direct_packed_q15(z0);
  *y1 = sve2_idft4_direct_packed_q15(z1);
  *y2 = sve2_idft4_direct_packed_q15(z2);
  *y3 = sve2_idft4_direct_packed_q15(z3);
}

SVE2_TARGET static void sve2_dft16_q15_special(const c16_t *src, c16_t *dst)
{
  const svbool_t pg32 = svptrue_b32();
  svint16_t y0, y1, y2, y3;
  sve2_dft16_q15_vectors(src, &y0, &y1, &y2, &y3);
  svst4_u32(pg32,
            (uint32_t *)dst,
            svcreate4_u32(svreinterpret_u32_s16(y0),
                          svreinterpret_u32_s16(y1),
                          svreinterpret_u32_s16(y2),
                          svreinterpret_u32_s16(y3)));
}

SVE2_TARGET static void sve2_idft16_q15_special(const c16_t *src, c16_t *dst)
{
  const svbool_t pg32 = svptrue_b32();
  svint16_t y0, y1, y2, y3;
  sve2_idft16_q15_vectors(src, &y0, &y1, &y2, &y3);
  svst4_u32(pg32,
            (uint32_t *)dst,
            svcreate4_u32(svreinterpret_u32_s16(y0),
                          svreinterpret_u32_s16(y1),
                          svreinterpret_u32_s16(y2),
                          svreinterpret_u32_s16(y3)));
}

SVE2_TARGET static inline void sve2_dft16_q15_parent_scatter(const c16_t *src,
                                                              c16_t *dst,
                                                              int parent_radix,
                                                              int parent_branch)
{
  svint16_t y0, y1, y2, y3;
  sve2_dft16_q15_vectors(src, &y0, &y1, &y2, &y3);
  const int step = 4 * parent_radix;
  sve2_parent_scatter_q15(y0, dst, parent_branch + 0 * parent_radix, step);
  sve2_parent_scatter_q15(y1, dst, parent_branch + 1 * parent_radix, step);
  sve2_parent_scatter_q15(y2, dst, parent_branch + 2 * parent_radix, step);
  sve2_parent_scatter_q15(y3, dst, parent_branch + 3 * parent_radix, step);
}

SVE2_TARGET static inline void sve2_idft16_q15_parent_scatter(const c16_t *src,
                                                               c16_t *dst,
                                                               int parent_radix,
                                                               int parent_branch)
{
  svint16_t y0, y1, y2, y3;
  sve2_idft16_q15_vectors(src, &y0, &y1, &y2, &y3);
  const int step = 4 * parent_radix;
  sve2_parent_scatter_q15(y0, dst, parent_branch + 0 * parent_radix, step);
  sve2_parent_scatter_q15(y1, dst, parent_branch + 1 * parent_radix, step);
  sve2_parent_scatter_q15(y2, dst, parent_branch + 2 * parent_radix, step);
  sve2_parent_scatter_q15(y3, dst, parent_branch + 3 * parent_radix, step);
}

/* DFT32 = R8 x DFT4. The parent contributes 1/sqrt(8) and each child
 * contributes 1/2, giving 1/sqrt(32). */
SVE2_TARGET static inline void sve2_dft32_q15_vectors(const c16_t *src,
                                                        svint16_t *y0,
                                                        svint16_t *y1,
                                                        svint16_t *y2,
                                                        svint16_t *y3,
                                                        svint16_t *y4,
                                                        svint16_t *y5,
                                                        svint16_t *y6,
                                                        svint16_t *y7)
{
  const svbool_t pg16 = svptrue_b16();
  const svint16_t x0 = svld1_s16(pg16, (const int16_t *)(src + 0));
  const svint16_t x1 = svld1_s16(pg16, (const int16_t *)(src + 4));
  const svint16_t x2 = svld1_s16(pg16, (const int16_t *)(src + 8));
  const svint16_t x3 = svld1_s16(pg16, (const int16_t *)(src + 12));
  const svint16_t x4 = svld1_s16(pg16, (const int16_t *)(src + 16));
  const svint16_t x5 = svld1_s16(pg16, (const int16_t *)(src + 20));
  const svint16_t x6 = svld1_s16(pg16, (const int16_t *)(src + 24));
  const svint16_t x7 = svld1_s16(pg16, (const int16_t *)(src + 28));

  svint16_t z0, z1, z2, z3, z4, z5, z6, z7;
  sve2_dft8_unitary_q15(x0, x1, x2, x3, x4, x5, x6, x7, &z0, &z1, &z2, &z3, &z4, &z5, &z6, &z7);
#define TINY32_Q(BR, Z)                                                       \
  do {                                                                        \
    Z = sve2_cmul_q15(Z, svld1_s16(pg16, g_sve2_tiny_tw.w32_q15[(BR) - 1])); \
  } while (0)
  TINY32_Q(1, z1);
  TINY32_Q(2, z2);
  TINY32_Q(3, z3);
  TINY32_Q(4, z4);
  TINY32_Q(5, z5);
  TINY32_Q(6, z6);
  TINY32_Q(7, z7);
#undef TINY32_Q

  *y0 = sve2_dft4_direct_packed_q15(z0);
  *y1 = sve2_dft4_direct_packed_q15(z1);
  *y2 = sve2_dft4_direct_packed_q15(z2);
  *y3 = sve2_dft4_direct_packed_q15(z3);
  *y4 = sve2_dft4_direct_packed_q15(z4);
  *y5 = sve2_dft4_direct_packed_q15(z5);
  *y6 = sve2_dft4_direct_packed_q15(z6);
  *y7 = sve2_dft4_direct_packed_q15(z7);
}

SVE2_TARGET static inline void sve2_idft32_q15_vectors(const c16_t *src,
                                                         svint16_t *y0,
                                                         svint16_t *y1,
                                                         svint16_t *y2,
                                                         svint16_t *y3,
                                                         svint16_t *y4,
                                                         svint16_t *y5,
                                                         svint16_t *y6,
                                                         svint16_t *y7)
{
  const svbool_t pg16 = svptrue_b16();
  const svint16_t x0 = svld1_s16(pg16, (const int16_t *)(src + 0));
  const svint16_t x1 = svld1_s16(pg16, (const int16_t *)(src + 4));
  const svint16_t x2 = svld1_s16(pg16, (const int16_t *)(src + 8));
  const svint16_t x3 = svld1_s16(pg16, (const int16_t *)(src + 12));
  const svint16_t x4 = svld1_s16(pg16, (const int16_t *)(src + 16));
  const svint16_t x5 = svld1_s16(pg16, (const int16_t *)(src + 20));
  const svint16_t x6 = svld1_s16(pg16, (const int16_t *)(src + 24));
  const svint16_t x7 = svld1_s16(pg16, (const int16_t *)(src + 28));

  svint16_t z0, z1, z2, z3, z4, z5, z6, z7;
  sve2_idft8_unitary_q15(x0, x1, x2, x3, x4, x5, x6, x7, &z0, &z1, &z2, &z3, &z4, &z5, &z6, &z7);
#define TINY32_IQ(BR, Z)                                                          \
  do {                                                                             \
    Z = sve2_cmul_q15(Z, svld1_s16(pg16, g_sve2_tiny_tw.w32_q15_inv[(BR) - 1])); \
  } while (0)
  TINY32_IQ(1, z1);
  TINY32_IQ(2, z2);
  TINY32_IQ(3, z3);
  TINY32_IQ(4, z4);
  TINY32_IQ(5, z5);
  TINY32_IQ(6, z6);
  TINY32_IQ(7, z7);
#undef TINY32_IQ

  *y0 = sve2_idft4_direct_packed_q15(z0);
  *y1 = sve2_idft4_direct_packed_q15(z1);
  *y2 = sve2_idft4_direct_packed_q15(z2);
  *y3 = sve2_idft4_direct_packed_q15(z3);
  *y4 = sve2_idft4_direct_packed_q15(z4);
  *y5 = sve2_idft4_direct_packed_q15(z5);
  *y6 = sve2_idft4_direct_packed_q15(z6);
  *y7 = sve2_idft4_direct_packed_q15(z7);
}

SVE2_TARGET static void sve2_dft32_q15_special(const c16_t *src, c16_t *dst)
{
  const svbool_t pg16 = svptrue_b16();
  c16_t y[8][4] __attribute__((aligned(16)));
  svint16_t y0, y1, y2, y3, y4, y5, y6, y7;
  sve2_dft32_q15_vectors(src, &y0, &y1, &y2, &y3, &y4, &y5, &y6, &y7);
  svst1_s16(pg16, (int16_t *)y[0], y0);
  svst1_s16(pg16, (int16_t *)y[1], y1);
  svst1_s16(pg16, (int16_t *)y[2], y2);
  svst1_s16(pg16, (int16_t *)y[3], y3);
  svst1_s16(pg16, (int16_t *)y[4], y4);
  svst1_s16(pg16, (int16_t *)y[5], y5);
  svst1_s16(pg16, (int16_t *)y[6], y6);
  svst1_s16(pg16, (int16_t *)y[7], y7);
  sve2_store_interleave8_q15(y[0], y[1], y[2], y[3], y[4], y[5], y[6], y[7], dst, 4);
}

SVE2_TARGET static void sve2_idft32_q15_special(const c16_t *src, c16_t *dst)
{
  const svbool_t pg16 = svptrue_b16();
  c16_t y[8][4] __attribute__((aligned(16)));
  svint16_t y0, y1, y2, y3, y4, y5, y6, y7;
  sve2_idft32_q15_vectors(src, &y0, &y1, &y2, &y3, &y4, &y5, &y6, &y7);
  svst1_s16(pg16, (int16_t *)y[0], y0);
  svst1_s16(pg16, (int16_t *)y[1], y1);
  svst1_s16(pg16, (int16_t *)y[2], y2);
  svst1_s16(pg16, (int16_t *)y[3], y3);
  svst1_s16(pg16, (int16_t *)y[4], y4);
  svst1_s16(pg16, (int16_t *)y[5], y5);
  svst1_s16(pg16, (int16_t *)y[6], y6);
  svst1_s16(pg16, (int16_t *)y[7], y7);
  sve2_store_interleave8_q15(y[0], y[1], y[2], y[3], y[4], y[5], y[6], y[7], dst, 4);
}

SVE2_TARGET static inline void sve2_dft32_q15_parent_scatter(const c16_t *src,
                                                              c16_t *dst,
                                                              int parent_radix,
                                                              int parent_branch)
{
  svint16_t y0, y1, y2, y3, y4, y5, y6, y7;
  sve2_dft32_q15_vectors(src, &y0, &y1, &y2, &y3, &y4, &y5, &y6, &y7);
  const int step = 8 * parent_radix;
  sve2_parent_scatter_q15(y0, dst, parent_branch + 0 * parent_radix, step);
  sve2_parent_scatter_q15(y1, dst, parent_branch + 1 * parent_radix, step);
  sve2_parent_scatter_q15(y2, dst, parent_branch + 2 * parent_radix, step);
  sve2_parent_scatter_q15(y3, dst, parent_branch + 3 * parent_radix, step);
  sve2_parent_scatter_q15(y4, dst, parent_branch + 4 * parent_radix, step);
  sve2_parent_scatter_q15(y5, dst, parent_branch + 5 * parent_radix, step);
  sve2_parent_scatter_q15(y6, dst, parent_branch + 6 * parent_radix, step);
  sve2_parent_scatter_q15(y7, dst, parent_branch + 7 * parent_radix, step);
}

SVE2_TARGET static inline void sve2_idft32_q15_parent_scatter(const c16_t *src,
                                                               c16_t *dst,
                                                               int parent_radix,
                                                               int parent_branch)
{
  svint16_t y0, y1, y2, y3, y4, y5, y6, y7;
  sve2_idft32_q15_vectors(src, &y0, &y1, &y2, &y3, &y4, &y5, &y6, &y7);
  const int step = 8 * parent_radix;
  sve2_parent_scatter_q15(y0, dst, parent_branch + 0 * parent_radix, step);
  sve2_parent_scatter_q15(y1, dst, parent_branch + 1 * parent_radix, step);
  sve2_parent_scatter_q15(y2, dst, parent_branch + 2 * parent_radix, step);
  sve2_parent_scatter_q15(y3, dst, parent_branch + 3 * parent_radix, step);
  sve2_parent_scatter_q15(y4, dst, parent_branch + 4 * parent_radix, step);
  sve2_parent_scatter_q15(y5, dst, parent_branch + 5 * parent_radix, step);
  sve2_parent_scatter_q15(y6, dst, parent_branch + 6 * parent_radix, step);
  sve2_parent_scatter_q15(y7, dst, parent_branch + 7 * parent_radix, step);
}

SVE2_TARGET static void sve2_dft512_q15_r8x64(const c16_t *src, c16_t *dst)
{
  const svbool_t pg = svptrue_b16();
  c16_t b[8][64] __attribute__((aligned(64)));
  c16_t y[8][64] __attribute__((aligned(64)));

  for (int blk = 0; blk < 16; blk++) {
    const int off = 4 * blk;
    const svint16_t x0 = svld1_s16(pg, (const int16_t *)(src + 0 + off));
    const svint16_t x1 = svld1_s16(pg, (const int16_t *)(src + 64 + off));
    const svint16_t x2 = svld1_s16(pg, (const int16_t *)(src + 128 + off));
    const svint16_t x3 = svld1_s16(pg, (const int16_t *)(src + 192 + off));
    const svint16_t x4 = svld1_s16(pg, (const int16_t *)(src + 256 + off));
    const svint16_t x5 = svld1_s16(pg, (const int16_t *)(src + 320 + off));
    const svint16_t x6 = svld1_s16(pg, (const int16_t *)(src + 384 + off));
    const svint16_t x7 = svld1_s16(pg, (const int16_t *)(src + 448 + off));

    svint16_t z0, z1, z2, z3, z4, z5, z6, z7;
    sve2_dft8_q15(x0, x1, x2, x3, x4, x5, x6, x7, &z0, &z1, &z2, &z3, &z4, &z5, &z6, &z7);

    z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT8);
    z1 = sve2_cmul_q15(z1, svld1_s16(pg, g_sve2_dft512_special_tw.r8_q15[0][blk]));
    z2 = sve2_cmul_q15(z2, svld1_s16(pg, g_sve2_dft512_special_tw.r8_q15[1][blk]));
    z3 = sve2_cmul_q15(z3, svld1_s16(pg, g_sve2_dft512_special_tw.r8_q15[2][blk]));
    z4 = sve2_cmul_q15(z4, svld1_s16(pg, g_sve2_dft512_special_tw.r8_q15[3][blk]));
    z5 = sve2_cmul_q15(z5, svld1_s16(pg, g_sve2_dft512_special_tw.r8_q15[4][blk]));
    z6 = sve2_cmul_q15(z6, svld1_s16(pg, g_sve2_dft512_special_tw.r8_q15[5][blk]));
    z7 = sve2_cmul_q15(z7, svld1_s16(pg, g_sve2_dft512_special_tw.r8_q15[6][blk]));

    svst1_s16(pg, (int16_t *)(b[0] + off), z0);
    svst1_s16(pg, (int16_t *)(b[1] + off), z1);
    svst1_s16(pg, (int16_t *)(b[2] + off), z2);
    svst1_s16(pg, (int16_t *)(b[3] + off), z3);
    svst1_s16(pg, (int16_t *)(b[4] + off), z4);
    svst1_s16(pg, (int16_t *)(b[5] + off), z5);
    svst1_s16(pg, (int16_t *)(b[6] + off), z6);
    svst1_s16(pg, (int16_t *)(b[7] + off), z7);
  }

  for (int r = 0; r < 8; r++)
    sve2_dft64_q15_special(b[r], y[r]);

  sve2_store_interleave8_q15(y[0], y[1], y[2], y[3], y[4], y[5], y[6], y[7], dst, 64);
}

/* Direct SVE2 radix-3 parents for the forward VL=128 path.
 * Their radix-3 twiddles include the 1/sqrt(3) parent normalization. */
typedef struct {
  int initialized;
  int16_t w768_q15[2][64][8] __attribute__((aligned(64)));
  int16_t w1536_q15[2][128][8] __attribute__((aligned(64)));
} sve2_dft_r3_fast_twiddle_t;

static sve2_dft_r3_fast_twiddle_t g_sve2_dft_r3_fast_tw;

static inline int16_t sve2_r3_fast_q15_twiddle_scaled(float x)
{
  return q15_from_float(x * 0.57735026918962576451f);
}

static void sve2_dft_r3_fast_prepare(void)
{
  sve2_dft_r3_fast_twiddle_t *tw = &g_sve2_dft_r3_fast_tw;
  if (tw->initialized)
    return;

  for (int branch = 1; branch <= 2; branch++) {
    for (int blk = 0; blk < 64; blk++) {
      for (int j = 0; j < 4; j++) {
        const int k = 4 * blk + j;
        const float a = -2.0f * (float)M_PI * (float)(branch * k) / 768.0f;
        tw->w768_q15[branch - 1][blk][2 * j + 0] = sve2_r3_fast_q15_twiddle_scaled(cosf(a));
        tw->w768_q15[branch - 1][blk][2 * j + 1] = sve2_r3_fast_q15_twiddle_scaled(sinf(a));
      }
    }

    for (int blk = 0; blk < 128; blk++) {
      for (int j = 0; j < 4; j++) {
        const int k = 4 * blk + j;
        const float a = -2.0f * (float)M_PI * (float)(branch * k) / 1536.0f;
        tw->w1536_q15[branch - 1][blk][2 * j + 0] = sve2_r3_fast_q15_twiddle_scaled(cosf(a));
        tw->w1536_q15[branch - 1][blk][2 * j + 1] = sve2_r3_fast_q15_twiddle_scaled(sinf(a));
      }
    }
  }

  tw->initialized = 1;
}

SVE2_TARGET static void sve2_dft768_q15_r3x256_fast(const c16_t *src, c16_t *dst)
{
  const svbool_t pg = svptrue_b16();
  const svbool_t pg32 = svptrue_b32();
  c16_t b[3][256] __attribute__((aligned(64)));
  c16_t y[3][256] __attribute__((aligned(64)));

  for (int blk = 0; blk < 64; blk++) {
    const int off = 4 * blk;
    const svint16_t x0 = svld1_s16(pg, (const int16_t *)(src + off));
    const svint16_t x1 = svld1_s16(pg, (const int16_t *)(src + 256 + off));
    const svint16_t x2 = svld1_s16(pg, (const int16_t *)(src + 512 + off));
    svint16_t z0, z1, z2;

    sve2_dft3_q15(x0, x1, x2, &z0, &z1, &z2);
    z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT3);
    z1 = sve2_cmul_q15(z1, svld1_s16(pg, g_sve2_dft_r3_fast_tw.w768_q15[0][blk]));
    z2 = sve2_cmul_q15(z2, svld1_s16(pg, g_sve2_dft_r3_fast_tw.w768_q15[1][blk]));

    svst1_s16(pg, (int16_t *)(b[0] + off), z0);
    svst1_s16(pg, (int16_t *)(b[1] + off), z1);
    svst1_s16(pg, (int16_t *)(b[2] + off), z2);
  }

  sve2_dft256_q15_special_r4x64(b[0], y[0]);
  sve2_dft256_q15_special_r4x64(b[1], y[1]);
  sve2_dft256_q15_special_r4x64(b[2], y[2]);

  for (int k = 0; k < 256; k += 4) {
    const svuint32_t v0 = svreinterpret_u32_s16(svld1_s16(pg, (const int16_t *)(y[0] + k)));
    const svuint32_t v1 = svreinterpret_u32_s16(svld1_s16(pg, (const int16_t *)(y[1] + k)));
    const svuint32_t v2 = svreinterpret_u32_s16(svld1_s16(pg, (const int16_t *)(y[2] + k)));
    svst3_u32(pg32, (uint32_t *)(dst + 3 * k), svcreate3_u32(v0, v1, v2));
  }
}

SVE2_TARGET static void sve2_dft1536_q15_r3x512_fast(const c16_t *src, c16_t *dst)
{
  const svbool_t pg = svptrue_b16();
  const svbool_t pg32 = svptrue_b32();
  c16_t b[3][512] __attribute__((aligned(64)));
  c16_t y[3][512] __attribute__((aligned(64)));

  for (int blk = 0; blk < 128; blk++) {
    const int off = 4 * blk;
    const svint16_t x0 = svld1_s16(pg, (const int16_t *)(src + off));
    const svint16_t x1 = svld1_s16(pg, (const int16_t *)(src + 512 + off));
    const svint16_t x2 = svld1_s16(pg, (const int16_t *)(src + 1024 + off));
    svint16_t z0, z1, z2;

    sve2_dft3_q15(x0, x1, x2, &z0, &z1, &z2);
    z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT3);
    z1 = sve2_cmul_q15(z1, svld1_s16(pg, g_sve2_dft_r3_fast_tw.w1536_q15[0][blk]));
    z2 = sve2_cmul_q15(z2, svld1_s16(pg, g_sve2_dft_r3_fast_tw.w1536_q15[1][blk]));

    svst1_s16(pg, (int16_t *)(b[0] + off), z0);
    svst1_s16(pg, (int16_t *)(b[1] + off), z1);
    svst1_s16(pg, (int16_t *)(b[2] + off), z2);
  }

  sve2_dft512_q15_r8x64(b[0], y[0]);
  sve2_dft512_q15_r8x64(b[1], y[1]);
  sve2_dft512_q15_r8x64(b[2], y[2]);

  for (int k = 0; k < 512; k += 4) {
    const svuint32_t v0 = svreinterpret_u32_s16(svld1_s16(pg, (const int16_t *)(y[0] + k)));
    const svuint32_t v1 = svreinterpret_u32_s16(svld1_s16(pg, (const int16_t *)(y[1] + k)));
    const svuint32_t v2 = svreinterpret_u32_s16(svld1_s16(pg, (const int16_t *)(y[2] + k)));
    svst3_u32(pg32, (uint32_t *)(dst + 3 * k), svcreate3_u32(v0, v1, v2));
  }
}

/*   768  = 3 x 256                                                          */
/*   1536 = 3 x 512                                                          */
/* Q15 children are already unitary, so the radix-3 front end contributes    */

static inline int16x8_t matched_neon_real_mul_q15(int16x8_t x, int16_t c)
{
  return vqrdmulhq_s16(x, vdupq_n_s16(c));
}

static inline int16x8_t matched_neon_rot90_q15(int16x8_t x)
{
  const int16x8_t sw = vrev32q_s16(x);
  const int16x8_t ng = vqnegq_s16(sw);
  const uint16x8_t m = {0xffffu, 0, 0xffffu, 0, 0xffffu, 0, 0xffffu, 0};
  return vbslq_s16(m, ng, sw); /* j*x = [-im,re] */
}

static inline int16x8_t matched_neon_rot270_q15(int16x8_t x)
{
  const int16x8_t sw = vrev32q_s16(x);
  const int16x8_t ng = vqnegq_s16(sw);
  const uint16x8_t m = {0xffffu, 0, 0xffffu, 0, 0xffffu, 0, 0xffffu, 0};
  return vbslq_s16(m, sw, ng); /* -j*x = [im,-re] */
}

/* Q15 complex multiply using the same qrdmulh-style rounding as the SVE2 path. */
static inline int16x8_t matched_neon_cmul_q15(int16x8_t a, int16x8_t b)
{
  const int16x8_t ae = vuzp1q_s16(a, a);
  const int16x8_t ao = vuzp2q_s16(a, a);
  const int16x8_t be = vuzp1q_s16(b, b);
  const int16x8_t bo = vuzp2q_s16(b, b);
  const int16x8_t ar = vzip1q_s16(ae, ae);
  const int16x8_t ai = vzip1q_s16(ao, ao);
  const int16x8_t br = vzip1q_s16(be, be);
  const int16x8_t bi = vzip1q_s16(bo, bo);
  const int16x8_t rr = vqrdmulhq_s16(ar, br);
  const int16x8_t ii = vqrdmulhq_s16(ai, bi);
  const int16x8_t ri = vqrdmulhq_s16(ar, bi);
  const int16x8_t ir = vqrdmulhq_s16(ai, br);
  const int16x8_t re = vqsubq_s16(rr, ii);
  const int16x8_t im = vqaddq_s16(ri, ir);
  const uint16x8_t m = {0xffffu, 0, 0xffffu, 0, 0xffffu, 0, 0xffffu, 0};
  return vbslq_s16(m, re, im);
}

static inline void matched_neon_dft3_q15(int16x8_t x0, int16x8_t x1, int16x8_t x2, int16x8_t *y0, int16x8_t *y1, int16x8_t *y2)
{
  const int16x8_t sum = vqaddq_s16(x1, x2);
  const int16x8_t diff = vqsubq_s16(x1, x2);
  const int16x8_t base = vqsubq_s16(x0, matched_neon_real_mul_q15(sum, Q15_HALF));
  const int16x8_t imag = matched_neon_real_mul_q15(diff, Q15_HALF_SQRT3);
  *y0 = vqaddq_s16(x0, sum);
  *y1 = vqaddq_s16(base, matched_neon_rot270_q15(imag));
  *y2 = vqaddq_s16(base, matched_neon_rot90_q15(imag));
}

static inline void matched_neon_transpose4_q15(int16x8_t a,
                                               int16x8_t b,
                                               int16x8_t c,
                                               int16x8_t d,
                                               int16x8_t *t0,
                                               int16x8_t *t1,
                                               int16x8_t *t2,
                                               int16x8_t *t3)
{
  const uint32x4_t au = vreinterpretq_u32_s16(a);
  const uint32x4_t bu = vreinterpretq_u32_s16(b);
  const uint32x4_t cu = vreinterpretq_u32_s16(c);
  const uint32x4_t du = vreinterpretq_u32_s16(d);
  const uint32x4_t ab0 = vzip1q_u32(au, bu);
  const uint32x4_t ab1 = vzip2q_u32(au, bu);
  const uint32x4_t cd0 = vzip1q_u32(cu, du);
  const uint32x4_t cd1 = vzip2q_u32(cu, du);
  const uint64x2_t z0 = vzip1q_u64(vreinterpretq_u64_u32(ab0), vreinterpretq_u64_u32(cd0));
  const uint64x2_t z1 = vzip2q_u64(vreinterpretq_u64_u32(ab0), vreinterpretq_u64_u32(cd0));
  const uint64x2_t z2 = vzip1q_u64(vreinterpretq_u64_u32(ab1), vreinterpretq_u64_u32(cd1));
  const uint64x2_t z3 = vzip2q_u64(vreinterpretq_u64_u32(ab1), vreinterpretq_u64_u32(cd1));
  *t0 = vreinterpretq_s16_u64(z0);
  *t1 = vreinterpretq_s16_u64(z1);
  *t2 = vreinterpretq_s16_u64(z2);
  *t3 = vreinterpretq_s16_u64(z3);
}

/*                                                                           */
/*   radix-3: 48=3x16, 96=3x32, 192=3x64, 384=3x128                         */
/*   radix-5: 120=5x24, 240=5x48                                             */
/*                                                                           */
/* 48 is intentionally reusable as the child of the 240-point radix-5 path. */
/* Q15 child transforms are unitary; the parent contributes 1/sqrt(radix).   */

typedef struct {
  int initialized;
  int N;
  int radix;
  int M;
  int16_t *q15[24];
  int16_t *q15_inv[24];
} sve2_mixed_parent_twiddle_t;

static void *sve2_aligned_calloc64(size_t bytes)
{
  const size_t rounded = (bytes + 63u) & ~(size_t)63u;
  void *p = aligned_alloc(64, rounded ? rounded : 64);
  AssertFatal(p != NULL, "SVE2 mixed twiddle allocation failed (%zu bytes)\n", bytes);
  memset(p, 0, rounded ? rounded : 64);
  return p;
}

static void sve2_mixed_parent_prepare_one(sve2_mixed_parent_twiddle_t *tw, int N, int radix)
{
  if (tw->initialized)
    return;

  AssertFatal(radix == 3 || radix == 5 || radix == 9 || radix == 15 || radix == 25, "Unsupported mixed SVE2 radix %d\n", radix);
  AssertFatal((N % radix) == 0, "Invalid mixed SVE2 N=%d radix=%d\n", N, radix);

  tw->N = N;
  tw->radix = radix;
  tw->M = N / radix;
  for (int branch = 1; branch < radix; branch++) {
    tw->q15[branch - 1] = sve2_aligned_calloc64((size_t)tw->M * sizeof(c16_t));
    tw->q15_inv[branch - 1] = sve2_aligned_calloc64((size_t)tw->M * sizeof(c16_t));

    for (int k = 0; k < tw->M; k++) {
      const float a = -2.0f * (float)M_PI * (float)(branch * k) / (float)N;
      const float wr = cosf(a);
      const float wi = sinf(a);
      if (radix == 3 || radix == 5) {
        tw->q15[branch - 1][2 * k + 0] = sve2_q15_scale_coeff(q15_from_float(wr), radix);
        tw->q15[branch - 1][2 * k + 1] = sve2_q15_scale_coeff(q15_from_float(wi), radix);
        tw->q15_inv[branch - 1][2 * k + 0] = sve2_q15_scale_coeff(q15_from_float(wr), radix);
        tw->q15_inv[branch - 1][2 * k + 1] = sve2_q15_scale_coeff(q15_from_float(-wi), radix);
      } else {
        tw->q15[branch - 1][2 * k + 0] = q15_from_float(wr);
        tw->q15[branch - 1][2 * k + 1] = q15_from_float(wi);
        tw->q15_inv[branch - 1][2 * k + 0] = q15_from_float(wr);
        tw->q15_inv[branch - 1][2 * k + 1] = q15_from_float(-wi);
      }
    }
  }

  tw->initialized = 1;
}

/* Terminal DFT60 = R5 x DFT12.                                             */
/*                                                                           */
/* Q15 is genuinely fused across the stage boundary: the three R5 chunks     */
/* needed by each DFT12 child remain in SVE registers, then feed the exact    */
/* DFT12 R3->DFT4 arithmetic directly.  No 5x12 R5 scratch matrix and no      */
/* child-output matrix are materialized.  Scaling/twiddles remain identical: */
/*   /5 -> R5 -> W60 -> /3 -> R3 -> W12 -> exact DFT4 -> sqrt(3) -> sqrt(5). */
/*                                                                           */

#if defined(__GNUC__) && !defined(__clang__)
#define SVE2_DFT60_NOIPA __attribute__((noinline, noclone, optimize("O2")))
#else
#define SVE2_DFT60_NOIPA __attribute__((noinline))
#endif

#undef SVE2_DFT60_NOIPA

/* Cached SVE2 power-of-two parent executors.                                 */
/* The general planner selects R4/R8 once and stores the child plan.          */
typedef struct oai_dft_plan_s oai_dft_plan_t;
static inline void execute_inverse_leaf_q15(const oai_dft_plan_t *p, const c16_t *src, c16_t *dst);

/* Power-of-two parents execute the cached child plan directly.  This avoids
 * one size-specific wrapper function per power-of-two transform. */
SVE2_TARGET static inline void sve2_execute_leaf_q15(const oai_dft_plan_t *p, const c16_t *src, c16_t *dst);

enum { SVE2_POW2_TW_R4 = 1u << 0, SVE2_POW2_TW_R8 = 1u << 1 };

typedef struct {
  unsigned prepared_mask;
  int N;
  int16_t *r4_q15[3];
  int16_t *r8_q15[7];
} sve2_large_pow2_twiddle_t;

static sve2_large_pow2_twiddle_t g_sve2_large_pow2[7];

/* N<=8192 uses fixed-size local scratch arrays in the specialized kernels.
 * Larger power-of-two transforms use grow-on-demand per-thread heap storage,
 * avoiding both large stack frames and shared mutable global scratch. */
static __thread c16_t *g_sve2_big_q15_b;
static __thread c16_t *g_sve2_big_q15_y;
static __thread size_t g_sve2_big_q15_elems;

static inline int sve2_large_pow2_slot(int N)
{
  switch (N) {
    case 1024:
      return 0;
    case 2048:
      return 1;
    case 4096:
      return 2;
    case 8192:
      return 3;
    case 16384:
      return 4;
    case 32768:
      return 5;
    case 65536:
      return 6;
    default:
      return -1;
  }
}

static inline int16_t sve2_large_q15_scale_coeff(float x, int radix)
{
  const int16_t q = q15_from_float(x);
  switch (radix) {
    case 4:
      return (int16_t)(q / 2);
    case 8:
      return sat_i16((long)((float)q / sqrtf(8.0f)));
    default:
      abort();
      __builtin_unreachable();
  }
}

static inline unsigned sve2_large_pow2_radix_mask(int radix)
{
  switch (radix) {
    case 4:
      return SVE2_POW2_TW_R4;
    case 8:
      return SVE2_POW2_TW_R8;
    default:
      return 0;
  }
}

/* Prepare only the twiddle family that belongs to the selected production
 * plan.  This function is called while the production plan mutex is held, so
 * the allocation is performed once before the immutable plan is published. */
static void sve2_large_power2_prepare_one(int N, int radix)
{
  const int slot = sve2_large_pow2_slot(N);
  AssertFatal(slot >= 0, "Unsupported large SVE2 power2 N=%d\n", N);

  const unsigned mask = sve2_large_pow2_radix_mask(radix);
  AssertFatal(mask != 0, "Unsupported large SVE2 power2 radix=%d N=%d\n", radix, N);

  sve2_large_pow2_twiddle_t *tw = &g_sve2_large_pow2[slot];
  if (tw->prepared_mask & mask)
    return;

  if (tw->N == 0)
    tw->N = N;
  AssertFatal(tw->N == N, "Corrupt SVE2 power2 twiddle slot N=%d stored=%d\n", N, tw->N);

  if (radix == 4) {
    const int M = N / 4;
    for (int br = 1; br < 4; br++) {
      tw->r4_q15[br - 1] = aligned_malloc64((size_t)M * sizeof(c16_t));
      AssertFatal(tw->r4_q15[br - 1], "SVE2 r4 twiddle alloc failed N=%d\n", N);
      for (int k = 0; k < M; k++) {
        const float a = -2.0f * (float)M_PI * (float)(br * k) / (float)N;
        tw->r4_q15[br - 1][2 * k + 0] = sve2_large_q15_scale_coeff(cosf(a), 4);
        tw->r4_q15[br - 1][2 * k + 1] = sve2_large_q15_scale_coeff(sinf(a), 4);
      }
    }
  } else {
    const int M = N / 8;
    for (int br = 1; br < 8; br++) {
      tw->r8_q15[br - 1] = aligned_malloc64((size_t)M * sizeof(c16_t));
      AssertFatal(tw->r8_q15[br - 1], "SVE2 r8 twiddle alloc failed N=%d\n", N);
      for (int k = 0; k < M; k++) {
        const float a = -2.0f * (float)M_PI * (float)(br * k) / (float)N;
        tw->r8_q15[br - 1][2 * k + 0] = sve2_large_q15_scale_coeff(cosf(a), 8);
        tw->r8_q15[br - 1][2 * k + 1] = sve2_large_q15_scale_coeff(sinf(a), 8);
      }
    }
  }

  tw->prepared_mask |= mask;
}

static inline int sve2_big_workspace_get(size_t need, c16_t **b_out, c16_t **y_out)
{
  if (need > g_sve2_big_q15_elems) {
    c16_t *new_b = aligned_malloc64(need * sizeof(*new_b));
    c16_t *new_y = aligned_malloc64(need * sizeof(*new_y));
    if (!new_b || !new_y) {
      free(new_b);
      free(new_y);
      return 0;
    }

    free(g_sve2_big_q15_b);
    free(g_sve2_big_q15_y);
    g_sve2_big_q15_b = new_b;
    g_sve2_big_q15_y = new_y;
    g_sve2_big_q15_elems = need;
  }

  *b_out = g_sve2_big_q15_b;
  *y_out = g_sve2_big_q15_y;
  return *b_out != NULL && *y_out != NULL;
}

static inline sve2_large_pow2_twiddle_t *sve2_large_twiddle_for_n(int N)
{
  const int slot = sve2_large_pow2_slot(N);
  return slot >= 0 ? &g_sve2_large_pow2[slot] : NULL;
}

SVE2_TARGET static void sve2_large_r4_q15(const c16_t *src, c16_t *dst, int N, const oai_dft_plan_t *child_plan)
{
  sve2_large_pow2_twiddle_t *tw = sve2_large_twiddle_for_n(N);
  AssertFatal(tw && (tw->prepared_mask & SVE2_POW2_TW_R4), "Missing SVE2 large r4 twiddles N=%d\n", N);
  const int M = N / 4;
  const svbool_t pg16 = svptrue_b16();
  const svbool_t pg32 = svptrue_b32();
  c16_t b[8192] __attribute__((aligned(64)));
  c16_t y[8192] __attribute__((aligned(64)));
  for (int off = 0; off < M; off += 4) {
    const svint16_t x0 = svld1_s16(pg16, (const int16_t *)(src + off));
    const svint16_t x1 = svld1_s16(pg16, (const int16_t *)(src + M + off));
    const svint16_t x2 = svld1_s16(pg16, (const int16_t *)(src + 2 * M + off));
    const svint16_t x3 = svld1_s16(pg16, (const int16_t *)(src + 3 * M + off));
    svint16_t z0, z1, z2, z3;
    sve2_dft4_butterfly_q15(x0, x1, x2, x3, &z0, &z1, &z2, &z3);
    z0 = sve2_q15_real_mul(z0, Q15_HALF);
    z1 = sve2_cmul_q15(z1, svld1_s16(pg16, tw->r4_q15[0] + 2 * off));
    z2 = sve2_cmul_q15(z2, svld1_s16(pg16, tw->r4_q15[1] + 2 * off));
    z3 = sve2_cmul_q15(z3, svld1_s16(pg16, tw->r4_q15[2] + 2 * off));
    svst1_s16(pg16, (int16_t *)(b + off), z0);
    svst1_s16(pg16, (int16_t *)(b + M + off), z1);
    svst1_s16(pg16, (int16_t *)(b + 2 * M + off), z2);
    svst1_s16(pg16, (int16_t *)(b + 3 * M + off), z3);
  }
  AssertFatal(child_plan != NULL, "Missing cached SVE2 R4 child plan N=%d\n", N);
  for (int r = 0; r < 4; r++)
    sve2_execute_leaf_q15(child_plan, b + r * M, y + r * M);
  for (int k = 0; k < M; k += 4) {
    const svuint32_t v0 = svreinterpret_u32_s16(svld1_s16(pg16, (const int16_t *)(y + k)));
    const svuint32_t v1 = svreinterpret_u32_s16(svld1_s16(pg16, (const int16_t *)(y + M + k)));
    const svuint32_t v2 = svreinterpret_u32_s16(svld1_s16(pg16, (const int16_t *)(y + 2 * M + k)));
    const svuint32_t v3 = svreinterpret_u32_s16(svld1_s16(pg16, (const int16_t *)(y + 3 * M + k)));
    svst4_u32(pg32, (uint32_t *)(dst + 4 * k), svcreate4_u32(v0, v1, v2, v3));
  }
}

SVE2_TARGET static void sve2_large_r8_q15(const c16_t *src, c16_t *dst, int N, const oai_dft_plan_t *child_plan)
{
  sve2_large_pow2_twiddle_t *tw = sve2_large_twiddle_for_n(N);
  AssertFatal(tw && (tw->prepared_mask & SVE2_POW2_TW_R8), "Missing SVE2 large r8 twiddles N=%d\n", N);
  const int M = N / 8;
  const svbool_t pg16 = svptrue_b16();
  c16_t b[8192] __attribute__((aligned(64)));
  c16_t y[8192] __attribute__((aligned(64)));
  for (int off = 0; off < M; off += 4) {
    const svint16_t x0 = svld1_s16(pg16, (const int16_t *)(src + off));
    const svint16_t x1 = svld1_s16(pg16, (const int16_t *)(src + M + off));
    const svint16_t x2 = svld1_s16(pg16, (const int16_t *)(src + 2 * M + off));
    const svint16_t x3 = svld1_s16(pg16, (const int16_t *)(src + 3 * M + off));
    const svint16_t x4 = svld1_s16(pg16, (const int16_t *)(src + 4 * M + off));
    const svint16_t x5 = svld1_s16(pg16, (const int16_t *)(src + 5 * M + off));
    const svint16_t x6 = svld1_s16(pg16, (const int16_t *)(src + 6 * M + off));
    const svint16_t x7 = svld1_s16(pg16, (const int16_t *)(src + 7 * M + off));
    svint16_t z0, z1, z2, z3, z4, z5, z6, z7;
    sve2_dft8_q15(x0, x1, x2, x3, x4, x5, x6, x7, &z0, &z1, &z2, &z3, &z4, &z5, &z6, &z7);
    z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT8);
    z1 = sve2_cmul_q15(z1, svld1_s16(pg16, tw->r8_q15[0] + 2 * off));
    z2 = sve2_cmul_q15(z2, svld1_s16(pg16, tw->r8_q15[1] + 2 * off));
    z3 = sve2_cmul_q15(z3, svld1_s16(pg16, tw->r8_q15[2] + 2 * off));
    z4 = sve2_cmul_q15(z4, svld1_s16(pg16, tw->r8_q15[3] + 2 * off));
    z5 = sve2_cmul_q15(z5, svld1_s16(pg16, tw->r8_q15[4] + 2 * off));
    z6 = sve2_cmul_q15(z6, svld1_s16(pg16, tw->r8_q15[5] + 2 * off));
    z7 = sve2_cmul_q15(z7, svld1_s16(pg16, tw->r8_q15[6] + 2 * off));
    svst1_s16(pg16, (int16_t *)(b + off), z0);
    svst1_s16(pg16, (int16_t *)(b + M + off), z1);
    svst1_s16(pg16, (int16_t *)(b + 2 * M + off), z2);
    svst1_s16(pg16, (int16_t *)(b + 3 * M + off), z3);
    svst1_s16(pg16, (int16_t *)(b + 4 * M + off), z4);
    svst1_s16(pg16, (int16_t *)(b + 5 * M + off), z5);
    svst1_s16(pg16, (int16_t *)(b + 6 * M + off), z6);
    svst1_s16(pg16, (int16_t *)(b + 7 * M + off), z7);
  }
  AssertFatal(child_plan != NULL, "Missing cached SVE2 R8 child plan N=%d\n", N);
  for (int r = 0; r < 8; r++)
    sve2_execute_leaf_q15(child_plan, b + r * M, y + r * M);
  sve2_store_interleave8_q15(y, y + M, y + 2 * M, y + 3 * M, y + 4 * M, y + 5 * M, y + 6 * M, y + 7 * M, dst, M);
}

#if defined(__GNUC__) && !defined(__clang__)
#define SVE2_BIG_NOIPA __attribute__((noinline, noclone, optimize("O2")))
#else
#define SVE2_BIG_NOIPA __attribute__((noinline))
#endif

SVE2_TARGET SVE2_BIG_NOIPA static void sve2_big_r8_q15(const c16_t *src, c16_t *dst, int N, const oai_dft_plan_t *child_plan)
{
  sve2_large_pow2_twiddle_t *tw = sve2_large_twiddle_for_n(N);
  AssertFatal(tw && (tw->prepared_mask & SVE2_POW2_TW_R8), "Missing SVE2 big r8 twiddles N=%d\n", N);
  const int M = N / 8;
  const svbool_t pg16 = svptrue_b16();
  c16_t *b = NULL;
  c16_t *y = NULL;
  AssertFatal(sve2_big_workspace_get((size_t)N, &b, &y), "SVE2 big power2 workspace allocation failed N=%d\n", N);
  for (int off = 0; off < M; off += 4) {
    svint16_t z0, z1, z2, z3, z4, z5, z6, z7;
    sve2_dft8_q15(svld1_s16(pg16, (const int16_t *)(src + off)),
                  svld1_s16(pg16, (const int16_t *)(src + M + off)),
                  svld1_s16(pg16, (const int16_t *)(src + 2 * M + off)),
                  svld1_s16(pg16, (const int16_t *)(src + 3 * M + off)),
                  svld1_s16(pg16, (const int16_t *)(src + 4 * M + off)),
                  svld1_s16(pg16, (const int16_t *)(src + 5 * M + off)),
                  svld1_s16(pg16, (const int16_t *)(src + 6 * M + off)),
                  svld1_s16(pg16, (const int16_t *)(src + 7 * M + off)),
                  &z0,
                  &z1,
                  &z2,
                  &z3,
                  &z4,
                  &z5,
                  &z6,
                  &z7);
    z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT8);
    z1 = sve2_cmul_q15(z1, svld1_s16(pg16, tw->r8_q15[0] + 2 * off));
    z2 = sve2_cmul_q15(z2, svld1_s16(pg16, tw->r8_q15[1] + 2 * off));
    z3 = sve2_cmul_q15(z3, svld1_s16(pg16, tw->r8_q15[2] + 2 * off));
    z4 = sve2_cmul_q15(z4, svld1_s16(pg16, tw->r8_q15[3] + 2 * off));
    z5 = sve2_cmul_q15(z5, svld1_s16(pg16, tw->r8_q15[4] + 2 * off));
    z6 = sve2_cmul_q15(z6, svld1_s16(pg16, tw->r8_q15[5] + 2 * off));
    z7 = sve2_cmul_q15(z7, svld1_s16(pg16, tw->r8_q15[6] + 2 * off));
    svst1_s16(pg16, (int16_t *)(b + off), z0);
    svst1_s16(pg16, (int16_t *)(b + M + off), z1);
    svst1_s16(pg16, (int16_t *)(b + 2 * M + off), z2);
    svst1_s16(pg16, (int16_t *)(b + 3 * M + off), z3);
    svst1_s16(pg16, (int16_t *)(b + 4 * M + off), z4);
    svst1_s16(pg16, (int16_t *)(b + 5 * M + off), z5);
    svst1_s16(pg16, (int16_t *)(b + 6 * M + off), z6);
    svst1_s16(pg16, (int16_t *)(b + 7 * M + off), z7);
  }
  AssertFatal(child_plan != NULL, "Missing cached SVE2 big R8 child plan N=%d\n", N);
  for (int r = 0; r < 8; r++)
    sve2_execute_leaf_q15(child_plan, b + r * M, y + r * M);
  sve2_store_interleave8_q15(y, y + M, y + 2 * M, y + 3 * M, y + 4 * M, y + 5 * M, y + 6 * M, y + 7 * M, dst, M);
}

/* Mixed stages consume factors 3 and 5; the remaining power-of-two size is
 * executed through its cached leaf plan. */

#ifndef SVE2_235_MAX_MIXED_STAGES
#define SVE2_235_MAX_MIXED_STAGES 5
#endif

struct oai_dft_plan_s {
  int N;
  int depth;
  int leaf_n;

  unsigned char sve2_pow2_radix;
  const struct oai_dft_plan_s *sve2_pow2_child_plan;

  const struct oai_dft_plan_s *sve2_leaf_plan;
  unsigned char stage_code[SVE2_235_MAX_MIXED_STAGES];
  sve2_mixed_parent_twiddle_t tw[SVE2_235_MAX_MIXED_STAGES];
  sve2_mixed_parent_twiddle_t fused_first_tw[SVE2_235_MAX_MIXED_STAGES];
  sve2_mixed_parent_twiddle_t fused_second_tw[SVE2_235_MAX_MIXED_STAGES];

  size_t workspace_elems;
};

enum {
  SVE2_235_STAGE_R3 = 3,
  SVE2_235_STAGE_R5 = 5,
  SVE2_235_STAGE_R9 = 9,
  /* first outer stage 3, then child stage 5 */
  SVE2_235_STAGE_R15_35 = 15,
  /* first outer stage 5, then child stage 3 */
  SVE2_235_STAGE_R15_53 = 53,
  SVE2_235_STAGE_R25 = 25
};

static int mixed_stage_decode(unsigned char code, int *radix, int *consume3, int *consume5, int *first_radix, int *second_radix)
{
  int r = 0, c3 = 0, c5 = 0, first = 0, second = 0;
  switch (code) {
    case SVE2_235_STAGE_R3:
      r = 3;
      c3 = 1;
      first = 3;
      break;
    case SVE2_235_STAGE_R5:
      r = 5;
      c5 = 1;
      first = 5;
      break;
    case SVE2_235_STAGE_R9:
      r = 9;
      c3 = 2;
      first = 3;
      second = 3;
      break;
    case SVE2_235_STAGE_R15_35:
      r = 15;
      c3 = 1;
      c5 = 1;
      first = 3;
      second = 5;
      break;
    case SVE2_235_STAGE_R15_53:
      r = 15;
      c3 = 1;
      c5 = 1;
      first = 5;
      second = 3;
      break;
    case SVE2_235_STAGE_R25:
      r = 25;
      c5 = 2;
      first = 5;
      second = 5;
      break;
    default:
      return 0;
  }
  if (radix)
    *radix = r;
  if (consume3)
    *consume3 = c3;
  if (consume5)
    *consume5 = c5;
  if (first_radix)
    *first_radix = first;
  if (second_radix)
    *second_radix = second;
  return 1;
}

static int sve2_235_factor(int N, int *e2, int *e3, int *e5)
{
  if (N <= 0)
    return 0;
  int n = N;
  *e2 = *e3 = *e5 = 0;
  while ((n % 2) == 0) {
    (*e2)++;
    n /= 2;
  }
  while ((n % 3) == 0) {
    (*e3)++;
    n /= 3;
  }
  while ((n % 5) == 0) {
    (*e5)++;
    n /= 5;
  }
  return n == 1;
}

static void sve2_235_free_twiddle(sve2_mixed_parent_twiddle_t *tw)
{
  if (!tw || !tw->initialized)
    return;
  for (int branch = 1; branch < tw->radix; branch++) {
    free(tw->q15[branch - 1]);
    free(tw->q15_inv[branch - 1]);
    tw->q15[branch - 1] = NULL;
    tw->q15_inv[branch - 1] = NULL;
  }
  memset(tw, 0, sizeof(*tw));
}

static void oai_dft_plan_reset(oai_dft_plan_t *p)
{
  if (!p)
    return;
  for (int i = 0; i < p->depth; i++) {
    sve2_235_free_twiddle(&p->tw[i]);
    sve2_235_free_twiddle(&p->fused_first_tw[i]);
    sve2_235_free_twiddle(&p->fused_second_tw[i]);
  }
  memset(p, 0, sizeof(*p));
}

static inline int sve2_235_leaf_supported(int n)
{
  return n == 1 || n == 12 || (n >= 4 && is_power_of_two_int(n));
}

static oai_dft_plan_t *oai_dft_mixed_plan_create(oai_dft_plan_t *p, int N, const unsigned char *stage, int depth)
{
  if (N <= 0 || N > MAX_N || depth < 0 || depth > SVE2_235_MAX_MIXED_STAGES)
    return NULL;

  int e2, e3, e5;
  if (!sve2_235_factor(N, &e2, &e3, &e5))
    return NULL;

  if (!p)
    return NULL;
  memset(p, 0, sizeof(*p));
  p->N = N;
  p->depth = depth;

  int cur = N;
  int seen3 = 0, seen5 = 0;
  size_t need = 0;
  for (int i = 0; i < depth; i++) {
    int r, c3, c5, first, second;
    const unsigned char code = stage[i];
    if (!mixed_stage_decode(code, &r, &c3, &c5, &first, &second) || (cur % r) != 0) {
      oai_dft_plan_reset(p);
      return NULL;
    }

    p->stage_code[i] = code;
    seen3 += c3;
    seen5 += c5;

    need += (size_t)2 * (size_t)cur;
    sve2_mixed_parent_prepare_one(&p->tw[i], cur, r);

    if (second != 0) {
      sve2_mixed_parent_prepare_one(&p->fused_first_tw[i], cur, first);
      sve2_mixed_parent_prepare_one(&p->fused_second_tw[i], cur / first, second);
    }
    cur /= r;
  }

  int leaf_e2 = 0, leaf_e3 = 0, leaf_e5 = 0;
  if (!sve2_235_factor(cur, &leaf_e2, &leaf_e3, &leaf_e5) || seen3 + leaf_e3 != e3 || seen5 + leaf_e5 != e5
      || !sve2_235_leaf_supported(cur)) {
    oai_dft_plan_reset(p);
    return NULL;
  }

  p->leaf_n = cur;

  /* Every terminal produced by the current production decomposition is
   * handled by a dedicated leaf implementation. */

  /* Store only the required scratch size in the immutable plan.
   * The actual buffer is acquired from per-thread reusable storage at runtime. */
  p->workspace_elems = need;
  return p;
}

SVE2_TARGET static inline void oai_prod_sve2_pow2_leaf_q15(const oai_dft_plan_t *p, const c16_t *src, c16_t *dst)
{
  AssertFatal(p && p->sve2_pow2_radix && p->sve2_pow2_child_plan, "Missing cached SVE2 power2 plan N=%d\n", p ? p->leaf_n : -1);

  const int N = p->leaf_n;

  /* DFT512 has a hand-specialized R8x64 kernel/twiddle layout. */
  if (N == 512) {
    AssertFatal(p->sve2_pow2_radix == 8 && p->sve2_pow2_child_plan->leaf_n == 64,
                "Bad cached DFT512 topology radix=%u child=%d\n",
                (unsigned)p->sve2_pow2_radix,
                p->sve2_pow2_child_plan->leaf_n);
    sve2_dft512_q15_r8x64(src, dst);
    return;
  }

  if (N <= 8192) {
    if (p->sve2_pow2_radix == 4) {
      sve2_large_r4_q15(src, dst, N, p->sve2_pow2_child_plan);
      return;
    }
    AssertFatal(p->sve2_pow2_radix == 8, "Unsupported cached SVE2 power2 radix=%u N=%d\n", (unsigned)p->sve2_pow2_radix, N);
    sve2_large_r8_q15(src, dst, N, p->sve2_pow2_child_plan);
    return;
  }

  AssertFatal(p->sve2_pow2_radix == 8, "Unsupported cached big SVE2 power2 radix=%u N=%d\n", (unsigned)p->sve2_pow2_radix, N);
  sve2_big_r8_q15(src, dst, N, p->sve2_pow2_child_plan);
}

SVE2_TARGET static inline void sve2_execute_leaf_q15(const oai_dft_plan_t *p, const c16_t *src, c16_t *dst)
{
  const oai_dft_plan_t *leaf_p = p->sve2_leaf_plan ? p->sve2_leaf_plan : p;

  switch (leaf_p->leaf_n) {
    case 1:
      dst[0] = src[0];
      return;
    case 4:
      sve2_dft4_direct_q15(src, dst);
      return;
    case 8:
      sve2_dft8_q15_special(src, dst);
      return;
    case 12:
      sve2_dft12_q15_special(src, dst);
      return;
    case 16:
      sve2_dft16_q15_special(src, dst);
      return;
    case 32:
      sve2_dft32_q15_special(src, dst);
      return;
    case 64:
      sve2_dft64_q15_special(src, dst);
      return;
    case 128:
      sve2_dft128_q15_special_fused_st2(src, dst);
      return;
    case 256:
      sve2_dft256_q15_special_r4x64(src, dst);
      return;
    case 512:
    case 1024:
    case 2048:
    case 4096:
    case 8192:
    case 16384:
    case 32768:
    case 65536:
      oai_prod_sve2_pow2_leaf_q15(leaf_p, src, dst);
      return;
    default:
      AssertFatal(0, "Unexpected SVE2 leaf N=%d\n", leaf_p->leaf_n);
      return;
  }
}

SVE2_TARGET static inline void sve2_execute_inverse_leaf_q15(const oai_dft_plan_t *p, const c16_t *src, c16_t *dst)
{
  const oai_dft_plan_t *leaf_p = p->sve2_leaf_plan ? p->sve2_leaf_plan : p;
  if (leaf_p->leaf_n == 12) {
    sve2_idft12_q15_special(src, dst);
    return;
  }
  if (leaf_p->leaf_n == 16) {
    sve2_idft16_q15_special(src, dst);
    return;
  }
  if (leaf_p->leaf_n == 32) {
    sve2_idft32_q15_special(src, dst);
    return;
  }
  execute_inverse_leaf_q15(leaf_p, src, dst);
}

static inline void execute_inverse_leaf_q15(const oai_dft_plan_t *p, const c16_t *src, c16_t *dst)
{
  switch (p->leaf_n) {
    case 1:
      dst[0] = src[0];
      return;
    case 4:
      neon_mono_idft4_q15(src, dst);
      return;
    case 8:
      neon_mono_idft8_q15(src, dst);
      return;
    case 12:
      neon_mono_idft12_fast_q15(src, dst);
      return;
    case 16:
      idft16lts_q15_native(src, dst);
      return;
    case 32:
      idft32lts_q15_native(src, dst);
      return;
    default:
      break;
  }

  AssertFatal(is_power_of_two_int(p->leaf_n) && p->leaf_n >= 64,
              "Unsupported inverse leaf N=%d\n",
              p->leaf_n);
  c16_t *leaf_work = oai_dft_tls_inverse_leaf_work((size_t)4 * (size_t)p->leaf_n);
  AssertFatal(leaf_work != NULL, "inverse leaf workspace allocation failed N=%d\n", p->leaf_n);
  dft_split_radix_pure_simd_core(src, dst, leaf_work, p->leaf_n, DFT_DIR_INVERSE);
}

/* Radix-9 stage implemented as R3 x R3. For q=3*b+a, the first R3 acts
 * across b and the second across a. Each stage contributes 1/sqrt(3), so
 * the combined normalization is 1/3 = 1/sqrt(9). Results are stored
 * branch-major with branch = beta + 3*alpha for the child DFT_M.
 */
SVE2_TARGET static inline void sve2_radix9_stage_to_branch_major_q15(const oai_dft_plan_t *p, int level, const c16_t *src, c16_t *b)
{
  const sve2_mixed_parent_twiddle_t *tw = &p->tw[level];
  const int M = tw->M;
  AssertFatal(tw->radix == 9 && p->stage_code[level] == SVE2_235_STAGE_R9,
              "Invalid SVE2 radix-9 stage level=%d N=%d radix=%d code=%u\\n",
              level,
              p->N,
              tw->radix,
              (unsigned)p->stage_code[level]);

  const sve2_mixed_parent_twiddle_t *twB = &p->fused_first_tw[level];
  const sve2_mixed_parent_twiddle_t *twA = &p->fused_second_tw[level];
  AssertFatal(twB->radix == 3 && twB->M == 3 * M && twA->radix == 3 && twA->M == M,
              "Invalid SVE2 radix-9 twiddles N=%d M=%d\\n",
              p->N,
              M);

  for (int off = 0; off < M; off += 4) {
    const int rem = M - off < 4 ? M - off : 4;
    const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));
    svint16_t s00, s01, s02, s10, s11, s12, s20, s21, s22;

    {
      const svint16_t x0 = svld1_s16(pg16, (const int16_t *)(src + off));
      const svint16_t x1 = svld1_s16(pg16, (const int16_t *)(src + 3 * M + off));
      const svint16_t x2 = svld1_s16(pg16, (const int16_t *)(src + 6 * M + off));
      sve2_dft3_q15(x0, x1, x2, &s00, &s01, &s02);
      s00 = sve2_q15_real_mul(s00, Q15_INV_SQRT3);
      s01 = sve2_cmul_q15(s01, svld1_s16(pg16, twB->q15[0] + 2 * off));
      s02 = sve2_cmul_q15(s02, svld1_s16(pg16, twB->q15[1] + 2 * off));
    }
    {
      const int first_off = M + off;
      const svint16_t x0 = svld1_s16(pg16, (const int16_t *)(src + M + off));
      const svint16_t x1 = svld1_s16(pg16, (const int16_t *)(src + 4 * M + off));
      const svint16_t x2 = svld1_s16(pg16, (const int16_t *)(src + 7 * M + off));
      sve2_dft3_q15(x0, x1, x2, &s10, &s11, &s12);
      s10 = sve2_q15_real_mul(s10, Q15_INV_SQRT3);
      s11 = sve2_cmul_q15(s11, svld1_s16(pg16, twB->q15[0] + 2 * first_off));
      s12 = sve2_cmul_q15(s12, svld1_s16(pg16, twB->q15[1] + 2 * first_off));
    }
    {
      const int first_off = 2 * M + off;
      const svint16_t x0 = svld1_s16(pg16, (const int16_t *)(src + 2 * M + off));
      const svint16_t x1 = svld1_s16(pg16, (const int16_t *)(src + 5 * M + off));
      const svint16_t x2 = svld1_s16(pg16, (const int16_t *)(src + 8 * M + off));
      sve2_dft3_q15(x0, x1, x2, &s20, &s21, &s22);
      s20 = sve2_q15_real_mul(s20, Q15_INV_SQRT3);
      s21 = sve2_cmul_q15(s21, svld1_s16(pg16, twB->q15[0] + 2 * first_off));
      s22 = sve2_cmul_q15(s22, svld1_s16(pg16, twB->q15[1] + 2 * first_off));
    }

    {
      svint16_t z0, z1, z2;
      sve2_dft3_q15(s00, s10, s20, &z0, &z1, &z2);
      z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT3);
      z1 = sve2_cmul_q15(z1, svld1_s16(pg16, twA->q15[0] + 2 * off));
      z2 = sve2_cmul_q15(z2, svld1_s16(pg16, twA->q15[1] + 2 * off));
      svst1_s16(pg16, (int16_t *)(b + off), z0);
      svst1_s16(pg16, (int16_t *)(b + 3 * M + off), z1);
      svst1_s16(pg16, (int16_t *)(b + 6 * M + off), z2);
    }
    {
      svint16_t z0, z1, z2;
      sve2_dft3_q15(s01, s11, s21, &z0, &z1, &z2);
      z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT3);
      z1 = sve2_cmul_q15(z1, svld1_s16(pg16, twA->q15[0] + 2 * off));
      z2 = sve2_cmul_q15(z2, svld1_s16(pg16, twA->q15[1] + 2 * off));
      svst1_s16(pg16, (int16_t *)(b + M + off), z0);
      svst1_s16(pg16, (int16_t *)(b + 4 * M + off), z1);
      svst1_s16(pg16, (int16_t *)(b + 7 * M + off), z2);
    }
    {
      svint16_t z0, z1, z2;
      sve2_dft3_q15(s02, s12, s22, &z0, &z1, &z2);
      z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT3);
      z1 = sve2_cmul_q15(z1, svld1_s16(pg16, twA->q15[0] + 2 * off));
      z2 = sve2_cmul_q15(z2, svld1_s16(pg16, twA->q15[1] + 2 * off));
      svst1_s16(pg16, (int16_t *)(b + 2 * M + off), z0);
      svst1_s16(pg16, (int16_t *)(b + 5 * M + off), z1);
      svst1_s16(pg16, (int16_t *)(b + 8 * M + off), z2);
    }
  }
}

/* Inverse radix-9 counterpart using inverse R3 butterflies and conjugated twiddles. */
SVE2_TARGET static inline void sve2_radix9_inverse_stage_to_branch_major_q15(const oai_dft_plan_t *p,
                                                                             int level,
                                                                             const c16_t *src,
                                                                             c16_t *b)
{
  const sve2_mixed_parent_twiddle_t *tw = &p->tw[level];
  const int M = tw->M;
  AssertFatal(tw->radix == 9 && p->stage_code[level] == SVE2_235_STAGE_R9,
              "Invalid SVE2 radix-9 stage level=%d N=%d radix=%d code=%u\\n",
              level,
              p->N,
              tw->radix,
              (unsigned)p->stage_code[level]);

  const sve2_mixed_parent_twiddle_t *twB = &p->fused_first_tw[level];
  const sve2_mixed_parent_twiddle_t *twA = &p->fused_second_tw[level];
  AssertFatal(twB->radix == 3 && twB->M == 3 * M && twA->radix == 3 && twA->M == M,
              "Invalid SVE2 radix-9 twiddles N=%d M=%d\\n",
              p->N,
              M);

  for (int off = 0; off < M; off += 4) {
    const int rem = M - off < 4 ? M - off : 4;
    const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));
    svint16_t s00, s01, s02, s10, s11, s12, s20, s21, s22;

    {
      const svint16_t x0 = svld1_s16(pg16, (const int16_t *)(src + off));
      const svint16_t x1 = svld1_s16(pg16, (const int16_t *)(src + 3 * M + off));
      const svint16_t x2 = svld1_s16(pg16, (const int16_t *)(src + 6 * M + off));
      sve2_idft3_q15(x0, x1, x2, &s00, &s01, &s02);
      s00 = sve2_q15_real_mul(s00, Q15_INV_SQRT3);
      s01 = sve2_cmul_q15(s01, svld1_s16(pg16, twB->q15_inv[0] + 2 * off));
      s02 = sve2_cmul_q15(s02, svld1_s16(pg16, twB->q15_inv[1] + 2 * off));
    }
    {
      const int first_off = M + off;
      const svint16_t x0 = svld1_s16(pg16, (const int16_t *)(src + M + off));
      const svint16_t x1 = svld1_s16(pg16, (const int16_t *)(src + 4 * M + off));
      const svint16_t x2 = svld1_s16(pg16, (const int16_t *)(src + 7 * M + off));
      sve2_idft3_q15(x0, x1, x2, &s10, &s11, &s12);
      s10 = sve2_q15_real_mul(s10, Q15_INV_SQRT3);
      s11 = sve2_cmul_q15(s11, svld1_s16(pg16, twB->q15_inv[0] + 2 * first_off));
      s12 = sve2_cmul_q15(s12, svld1_s16(pg16, twB->q15_inv[1] + 2 * first_off));
    }
    {
      const int first_off = 2 * M + off;
      const svint16_t x0 = svld1_s16(pg16, (const int16_t *)(src + 2 * M + off));
      const svint16_t x1 = svld1_s16(pg16, (const int16_t *)(src + 5 * M + off));
      const svint16_t x2 = svld1_s16(pg16, (const int16_t *)(src + 8 * M + off));
      sve2_idft3_q15(x0, x1, x2, &s20, &s21, &s22);
      s20 = sve2_q15_real_mul(s20, Q15_INV_SQRT3);
      s21 = sve2_cmul_q15(s21, svld1_s16(pg16, twB->q15_inv[0] + 2 * first_off));
      s22 = sve2_cmul_q15(s22, svld1_s16(pg16, twB->q15_inv[1] + 2 * first_off));
    }

    {
      svint16_t z0, z1, z2;
      sve2_idft3_q15(s00, s10, s20, &z0, &z1, &z2);
      z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT3);
      z1 = sve2_cmul_q15(z1, svld1_s16(pg16, twA->q15_inv[0] + 2 * off));
      z2 = sve2_cmul_q15(z2, svld1_s16(pg16, twA->q15_inv[1] + 2 * off));
      svst1_s16(pg16, (int16_t *)(b + off), z0);
      svst1_s16(pg16, (int16_t *)(b + 3 * M + off), z1);
      svst1_s16(pg16, (int16_t *)(b + 6 * M + off), z2);
    }
    {
      svint16_t z0, z1, z2;
      sve2_idft3_q15(s01, s11, s21, &z0, &z1, &z2);
      z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT3);
      z1 = sve2_cmul_q15(z1, svld1_s16(pg16, twA->q15_inv[0] + 2 * off));
      z2 = sve2_cmul_q15(z2, svld1_s16(pg16, twA->q15_inv[1] + 2 * off));
      svst1_s16(pg16, (int16_t *)(b + M + off), z0);
      svst1_s16(pg16, (int16_t *)(b + 4 * M + off), z1);
      svst1_s16(pg16, (int16_t *)(b + 7 * M + off), z2);
    }
    {
      svint16_t z0, z1, z2;
      sve2_idft3_q15(s02, s12, s22, &z0, &z1, &z2);
      z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT3);
      z1 = sve2_cmul_q15(z1, svld1_s16(pg16, twA->q15_inv[0] + 2 * off));
      z2 = sve2_cmul_q15(z2, svld1_s16(pg16, twA->q15_inv[1] + 2 * off));
      svst1_s16(pg16, (int16_t *)(b + 2 * M + off), z0);
      svst1_s16(pg16, (int16_t *)(b + 5 * M + off), z1);
      svst1_s16(pg16, (int16_t *)(b + 8 * M + off), z2);
    }
  }
}

/* Inverse radix-5 stage, kept branch-major for nested terminal fusion. */
SVE2_TARGET static inline void sve2_radix5_inverse_stage_to_branch_major_q15(const oai_dft_plan_t *p,
                                                                             int level,
                                                                             const c16_t *src,
                                                                             c16_t *b)
{
  const sve2_mixed_parent_twiddle_t *tw = &p->tw[level];
  const int M = tw->M;
  AssertFatal(tw->radix == 5 && p->stage_code[level] == SVE2_235_STAGE_R5,
              "Invalid SVE2 radix-5 stage level=%d N=%d radix=%d code=%u\\n",
              level,
              p->N,
              tw->radix,
              (unsigned)p->stage_code[level]);

  for (int off = 0; off < M; off += 4) {
    const int rem = M - off < 4 ? M - off : 4;
    const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));
    const svint16_t x0 = svld1_s16(pg16, (const int16_t *)(src + off));
    const svint16_t x1 = svld1_s16(pg16, (const int16_t *)(src + M + off));
    const svint16_t x2 = svld1_s16(pg16, (const int16_t *)(src + 2 * M + off));
    const svint16_t x3 = svld1_s16(pg16, (const int16_t *)(src + 3 * M + off));
    const svint16_t x4 = svld1_s16(pg16, (const int16_t *)(src + 4 * M + off));
    svint16_t z0, z1, z2, z3, z4;
    sve2_idft5_q15(x0, x1, x2, x3, x4, &z0, &z1, &z2, &z3, &z4);
    z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT5);
    z1 = sve2_cmul_q15(z1, svld1_s16(pg16, tw->q15_inv[0] + 2 * off));
    z2 = sve2_cmul_q15(z2, svld1_s16(pg16, tw->q15_inv[1] + 2 * off));
    z3 = sve2_cmul_q15(z3, svld1_s16(pg16, tw->q15_inv[2] + 2 * off));
    z4 = sve2_cmul_q15(z4, svld1_s16(pg16, tw->q15_inv[3] + 2 * off));
    svst1_s16(pg16, (int16_t *)(b + off), z0);
    svst1_s16(pg16, (int16_t *)(b + M + off), z1);
    svst1_s16(pg16, (int16_t *)(b + 2 * M + off), z2);
    svst1_s16(pg16, (int16_t *)(b + 3 * M + off), z3);
    svst1_s16(pg16, (int16_t *)(b + 4 * M + off), z4);
  }
}

/* Inverse R25(5x5) stage, kept branch-major for nested terminal fusion. */
SVE2_TARGET static inline void sve2_r25_inverse_stage_to_branch_major_q15(const oai_dft_plan_t *p,
                                                                          int level,
                                                                          const c16_t *src,
                                                                          c16_t *b)
{
  const sve2_mixed_parent_twiddle_t *tw = &p->tw[level];
  const int M = tw->M;
  AssertFatal(tw->radix == 25 && p->stage_code[level] == SVE2_235_STAGE_R25,
              "Invalid nested inverse R25 stage level=%d N=%d radix=%d code=%u\\n",
              level,
              p->N,
              tw->radix,
              (unsigned)p->stage_code[level]);

  const sve2_mixed_parent_twiddle_t *twB = &p->fused_first_tw[level];
  const sve2_mixed_parent_twiddle_t *twA = &p->fused_second_tw[level];
  AssertFatal(twB->radix == 5 && twB->M == 5 * M && twA->radix == 5 && twA->M == M,
              "Bad nested inverse R25 twiddles N=%d M=%d\\n",
              p->N,
              M);

  c16_t stage1[25][4] __attribute__((aligned(64)));
  for (int off = 0; off < M; off += 4) {
    const int rem = M - off < 4 ? M - off : 4;
    const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));

    for (int a = 0; a < 5; a++) {
      const int first_off = a * M + off;
      const svint16_t x0 = svld1_s16(pg16, (const int16_t *)(src + a * M + off));
      const svint16_t x1 = svld1_s16(pg16, (const int16_t *)(src + (5 + a) * M + off));
      const svint16_t x2 = svld1_s16(pg16, (const int16_t *)(src + (10 + a) * M + off));
      const svint16_t x3 = svld1_s16(pg16, (const int16_t *)(src + (15 + a) * M + off));
      const svint16_t x4 = svld1_s16(pg16, (const int16_t *)(src + (20 + a) * M + off));
      svint16_t z0, z1, z2, z3, z4;
      sve2_idft5_q15(x0, x1, x2, x3, x4, &z0, &z1, &z2, &z3, &z4);
      z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT5);
      z1 = sve2_cmul_q15(z1, svld1_s16(pg16, twB->q15_inv[0] + 2 * first_off));
      z2 = sve2_cmul_q15(z2, svld1_s16(pg16, twB->q15_inv[1] + 2 * first_off));
      z3 = sve2_cmul_q15(z3, svld1_s16(pg16, twB->q15_inv[2] + 2 * first_off));
      z4 = sve2_cmul_q15(z4, svld1_s16(pg16, twB->q15_inv[3] + 2 * first_off));
      svst1_s16(pg16, (int16_t *)stage1[a * 5 + 0], z0);
      svst1_s16(pg16, (int16_t *)stage1[a * 5 + 1], z1);
      svst1_s16(pg16, (int16_t *)stage1[a * 5 + 2], z2);
      svst1_s16(pg16, (int16_t *)stage1[a * 5 + 3], z3);
      svst1_s16(pg16, (int16_t *)stage1[a * 5 + 4], z4);
    }

    for (int bidx = 0; bidx < 5; bidx++) {
      const svint16_t x0 = svld1_s16(pg16, (const int16_t *)stage1[bidx]);
      const svint16_t x1 = svld1_s16(pg16, (const int16_t *)stage1[5 + bidx]);
      const svint16_t x2 = svld1_s16(pg16, (const int16_t *)stage1[10 + bidx]);
      const svint16_t x3 = svld1_s16(pg16, (const int16_t *)stage1[15 + bidx]);
      const svint16_t x4 = svld1_s16(pg16, (const int16_t *)stage1[20 + bidx]);
      svint16_t z0, z1, z2, z3, z4;
      sve2_idft5_q15(x0, x1, x2, x3, x4, &z0, &z1, &z2, &z3, &z4);
      z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT5);
      z1 = sve2_cmul_q15(z1, svld1_s16(pg16, twA->q15_inv[0] + 2 * off));
      z2 = sve2_cmul_q15(z2, svld1_s16(pg16, twA->q15_inv[1] + 2 * off));
      z3 = sve2_cmul_q15(z3, svld1_s16(pg16, twA->q15_inv[2] + 2 * off));
      z4 = sve2_cmul_q15(z4, svld1_s16(pg16, twA->q15_inv[3] + 2 * off));
      svst1_s16(pg16, (int16_t *)(b + (bidx + 0 * 5) * M + off), z0);
      svst1_s16(pg16, (int16_t *)(b + (bidx + 1 * 5) * M + off), z1);
      svst1_s16(pg16, (int16_t *)(b + (bidx + 2 * 5) * M + off), z2);
      svst1_s16(pg16, (int16_t *)(b + (bidx + 3 * 5) * M + off), z3);
      svst1_s16(pg16, (int16_t *)(b + (bidx + 4 * 5) * M + off), z4);
    }
  }
}

/* Exact branch-major R15(5x3) stage used by nested mixed-radix fast paths.
 * It preserves the generic fused-stage arithmetic and twiddle locations, but
 * stops before the terminal child transform so an outer parent can compose
 * its final scatter directly with the 15-way child order. */
SVE2_TARGET static inline void sve2_r15_53_stage_to_branch_major_q15(const oai_dft_plan_t *p,
                                                                     int level,
                                                                     const c16_t *src,
                                                                     c16_t *b)
{
  const sve2_mixed_parent_twiddle_t *tw = &p->tw[level];
  const int M = tw->M;
  AssertFatal(tw->radix == 15 && p->stage_code[level] == SVE2_235_STAGE_R15_53,
              "Invalid nested SVE2 R15_53 stage level=%d N=%d radix=%d code=%u\\n",
              level,
              p->N,
              tw->radix,
              (unsigned)p->stage_code[level]);

  const sve2_mixed_parent_twiddle_t *twB = &p->fused_first_tw[level];
  const sve2_mixed_parent_twiddle_t *twA = &p->fused_second_tw[level];
  AssertFatal(twB->radix == 5 && twB->M == 3 * M && twA->radix == 3 && twA->M == M,
              "Bad nested R15_53 twiddles N=%d M=%d\\n",
              p->N,
              M);

  c16_t stage1[15][4] __attribute__((aligned(64)));
  for (int off = 0; off < M; off += 4) {
    const int rem = M - off < 4 ? M - off : 4;
    const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));

    /* First fused radix: 5. */
    for (int a = 0; a < 3; a++) {
      const int first_off = a * M + off;
      svint16_t x0 = svld1_s16(pg16, (const int16_t *)(src + a * M + off));
      svint16_t x1 = svld1_s16(pg16, (const int16_t *)(src + (3 + a) * M + off));
      svint16_t x2 = svld1_s16(pg16, (const int16_t *)(src + (6 + a) * M + off));
      svint16_t x3 = svld1_s16(pg16, (const int16_t *)(src + (9 + a) * M + off));
      svint16_t x4 = svld1_s16(pg16, (const int16_t *)(src + (12 + a) * M + off));
      svint16_t z0, z1, z2, z3, z4;
      sve2_dft5_q15(x0, x1, x2, x3, x4, &z0, &z1, &z2, &z3, &z4);
      z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT5);
      z1 = sve2_cmul_q15(z1, svld1_s16(pg16, twB->q15[0] + 2 * first_off));
      z2 = sve2_cmul_q15(z2, svld1_s16(pg16, twB->q15[1] + 2 * first_off));
      z3 = sve2_cmul_q15(z3, svld1_s16(pg16, twB->q15[2] + 2 * first_off));
      z4 = sve2_cmul_q15(z4, svld1_s16(pg16, twB->q15[3] + 2 * first_off));
      svst1_s16(pg16, (int16_t *)stage1[a * 5 + 0], z0);
      svst1_s16(pg16, (int16_t *)stage1[a * 5 + 1], z1);
      svst1_s16(pg16, (int16_t *)stage1[a * 5 + 2], z2);
      svst1_s16(pg16, (int16_t *)stage1[a * 5 + 3], z3);
      svst1_s16(pg16, (int16_t *)stage1[a * 5 + 4], z4);
    }

    /* Second fused radix: 3. Branch order is b + 5*a, identical to the
     * generic R15_53 executor. */
    for (int bidx = 0; bidx < 5; bidx++) {
      svint16_t x0 = svld1_s16(pg16, (const int16_t *)stage1[bidx]);
      svint16_t x1 = svld1_s16(pg16, (const int16_t *)stage1[5 + bidx]);
      svint16_t x2 = svld1_s16(pg16, (const int16_t *)stage1[10 + bidx]);
      svint16_t z0, z1, z2;
      sve2_dft3_q15(x0, x1, x2, &z0, &z1, &z2);
      z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT3);
      z1 = sve2_cmul_q15(z1, svld1_s16(pg16, twA->q15[0] + 2 * off));
      z2 = sve2_cmul_q15(z2, svld1_s16(pg16, twA->q15[1] + 2 * off));
      svst1_s16(pg16, (int16_t *)(b + (bidx + 0 * 5) * M + off), z0);
      svst1_s16(pg16, (int16_t *)(b + (bidx + 1 * 5) * M + off), z1);
      svst1_s16(pg16, (int16_t *)(b + (bidx + 2 * 5) * M + off), z2);
    }
  }
}


/* Exact branch-major R25(5x5) stage used by nested terminal fast paths. */
SVE2_TARGET static inline void sve2_r25_stage_to_branch_major_q15(const oai_dft_plan_t *p,
                                                                  int level,
                                                                  const c16_t *src,
                                                                  c16_t *b)
{
  const sve2_mixed_parent_twiddle_t *tw = &p->tw[level];
  const int M = tw->M;
  AssertFatal(tw->radix == 25 && p->stage_code[level] == SVE2_235_STAGE_R25,
              "Invalid nested SVE2 R25 stage level=%d N=%d radix=%d code=%u\\n",
              level,
              p->N,
              tw->radix,
              (unsigned)p->stage_code[level]);

  const sve2_mixed_parent_twiddle_t *twB = &p->fused_first_tw[level];
  const sve2_mixed_parent_twiddle_t *twA = &p->fused_second_tw[level];
  AssertFatal(twB->radix == 5 && twB->M == 5 * M && twA->radix == 5 && twA->M == M,
              "Bad nested R25 twiddles N=%d M=%d\\n",
              p->N,
              M);

  c16_t stage1[25][4] __attribute__((aligned(64)));
  for (int off = 0; off < M; off += 4) {
    const int rem = M - off < 4 ? M - off : 4;
    const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));

    for (int a = 0; a < 5; a++) {
      const int first_off = a * M + off;
      svint16_t x0 = svld1_s16(pg16, (const int16_t *)(src + a * M + off));
      svint16_t x1 = svld1_s16(pg16, (const int16_t *)(src + (5 + a) * M + off));
      svint16_t x2 = svld1_s16(pg16, (const int16_t *)(src + (10 + a) * M + off));
      svint16_t x3 = svld1_s16(pg16, (const int16_t *)(src + (15 + a) * M + off));
      svint16_t x4 = svld1_s16(pg16, (const int16_t *)(src + (20 + a) * M + off));
      svint16_t z0, z1, z2, z3, z4;
      sve2_dft5_q15(x0, x1, x2, x3, x4, &z0, &z1, &z2, &z3, &z4);
      z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT5);
      z1 = sve2_cmul_q15(z1, svld1_s16(pg16, twB->q15[0] + 2 * first_off));
      z2 = sve2_cmul_q15(z2, svld1_s16(pg16, twB->q15[1] + 2 * first_off));
      z3 = sve2_cmul_q15(z3, svld1_s16(pg16, twB->q15[2] + 2 * first_off));
      z4 = sve2_cmul_q15(z4, svld1_s16(pg16, twB->q15[3] + 2 * first_off));
      svst1_s16(pg16, (int16_t *)stage1[a * 5 + 0], z0);
      svst1_s16(pg16, (int16_t *)stage1[a * 5 + 1], z1);
      svst1_s16(pg16, (int16_t *)stage1[a * 5 + 2], z2);
      svst1_s16(pg16, (int16_t *)stage1[a * 5 + 3], z3);
      svst1_s16(pg16, (int16_t *)stage1[a * 5 + 4], z4);
    }

    for (int bidx = 0; bidx < 5; bidx++) {
      svint16_t x0 = svld1_s16(pg16, (const int16_t *)stage1[bidx]);
      svint16_t x1 = svld1_s16(pg16, (const int16_t *)stage1[5 + bidx]);
      svint16_t x2 = svld1_s16(pg16, (const int16_t *)stage1[10 + bidx]);
      svint16_t x3 = svld1_s16(pg16, (const int16_t *)stage1[15 + bidx]);
      svint16_t x4 = svld1_s16(pg16, (const int16_t *)stage1[20 + bidx]);
      svint16_t z0, z1, z2, z3, z4;
      sve2_dft5_q15(x0, x1, x2, x3, x4, &z0, &z1, &z2, &z3, &z4);
      z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT5);
      z1 = sve2_cmul_q15(z1, svld1_s16(pg16, twA->q15[0] + 2 * off));
      z2 = sve2_cmul_q15(z2, svld1_s16(pg16, twA->q15[1] + 2 * off));
      z3 = sve2_cmul_q15(z3, svld1_s16(pg16, twA->q15[2] + 2 * off));
      z4 = sve2_cmul_q15(z4, svld1_s16(pg16, twA->q15[3] + 2 * off));
      svst1_s16(pg16, (int16_t *)(b + (bidx + 0 * 5) * M + off), z0);
      svst1_s16(pg16, (int16_t *)(b + (bidx + 1 * 5) * M + off), z1);
      svst1_s16(pg16, (int16_t *)(b + (bidx + 2 * 5) * M + off), z2);
      svst1_s16(pg16, (int16_t *)(b + (bidx + 3 * 5) * M + off), z3);
      svst1_s16(pg16, (int16_t *)(b + (bidx + 4 * 5) * M + off), z4);
    }
  }
}

SVE2_TARGET static void sve2_execute_mixed_plan_q15(const oai_dft_plan_t *p, int level, const c16_t *src, c16_t *dst, c16_t *work)
{
  if (level == p->depth) {
    sve2_execute_leaf_q15(p, src, dst);
    return;
  }

  const sve2_mixed_parent_twiddle_t *tw = &p->tw[level];
  const int N = tw->N;
  const int M = tw->M;
  const int r = tw->radix;
  c16_t *b = work;
  c16_t *y = work + N;
  c16_t *child_work = work + 2 * N;

  /* Two adjacent radix-9 stages are fused as radix-81: the 81 child outputs
   * remain branch-major and are scattered to natural order only once. */
  if (r == 9 && level + 1 < p->depth && p->stage_code[level + 1] == SVE2_235_STAGE_R9) {
    const sve2_mixed_parent_twiddle_t *tw_inner = &p->tw[level + 1];
    const int M1 = M;
    const int M2 = tw_inner->M;
    AssertFatal(tw_inner->N == M1 && tw_inner->radix == 9 && M1 == 9 * M2,
                "Bad SVE2 F81 topology level=%d N=%d M1=%d M2=%d\\n",
                level,
                N,
                M1,
                M2);

    sve2_radix9_stage_to_branch_major_q15(p, level, src, b);

    c16_t *combined_y = y;
    c16_t *inner_b = child_work;
    c16_t *grand_work = inner_b + M1;
    const int direct_leaf12 = M2 == 12 && level + 2 == p->depth;
    const int direct_leaf16 = M2 == 16 && level + 2 == p->depth;
    const int direct_leaf32 = M2 == 32 && level + 2 == p->depth;

    for (int outer = 0; outer < 9; outer++) {
      sve2_radix9_stage_to_branch_major_q15(p, level + 1, b + outer * M1, inner_b);
      if (direct_leaf12) {
        for (int inner = 0; inner < 9; inner++) {
          const int combined = 9 * inner + outer;
          sve2_dft12_q15_parent_scatter(inner_b + inner * M2, dst, 81, combined);
        }
      } else if (direct_leaf16) {
        for (int inner = 0; inner < 9; inner++) {
          const int combined = 9 * inner + outer;
          sve2_dft16_q15_parent_scatter(inner_b + inner * M2, dst, 81, combined);
        }
      } else if (direct_leaf32) {
        for (int inner = 0; inner < 9; inner++) {
          const int combined = 9 * inner + outer;
          sve2_dft32_q15_parent_scatter(inner_b + inner * M2, dst, 81, combined);
        }
      } else if (M2 == 8 && level + 2 == p->depth) {
        int inner = 0;
        for (; inner + 3 < 9; inner += 4)
          neon_dft8x4_branch_major_q15(inner_b + inner * M2, combined_y + (9 * inner + outer) * M2, M2, 9 * M2);
        for (; inner < 9; inner++) {
          const int combined = 9 * inner + outer;
          sve2_dft8_q15_special(inner_b + inner * M2, combined_y + combined * M2);
        }
      } else {
        for (int inner = 0; inner < 9; inner++) {
          const int combined = 9 * inner + outer;
          sve2_execute_mixed_plan_q15(p, level + 2, inner_b + inner * M2, combined_y + combined * M2, grand_work);
        }
      }
    }

    if (direct_leaf12 || direct_leaf16 || direct_leaf32)
      return;

    /* Natural output index: 81*k + 9*inner + outer. */
    uint32_t *dst32 = (uint32_t *)dst;
    for (int k = 0; k < M2; k += 4) {
      const int rem = M2 - k < 4 ? M2 - k : 4;
      const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));
      const svbool_t pg32 = svwhilelt_b32((uint64_t)0, (uint64_t)rem);
      for (int br = 0; br < 81; br++) {
        const svint16_t a = svld1_s16(pg16, (const int16_t *)(combined_y + br * M2 + k));
        const svuint32_t idx = svindex_u32((uint32_t)(81 * k + br), 81);
        svst1_scatter_u32index_u32(pg32, dst32, idx, svreinterpret_u32_s16(a));
      }
    }
    return;
  }

  if (r == 9) {
    sve2_radix9_stage_to_branch_major_q15(p, level, src, b);

    /* Compose R9 -> R15_53 -> DFT16 into the final 135-way output order.
     * The generic recursion writes nine 240-point child outputs to y[] and
     * immediately reloads them for the outer R9 scatter. Here each terminal
     * DFT16 is scattered directly to 135*k + 9*inner + outer instead. */
    if (level + 2 == p->depth &&
        p->stage_code[level + 1] == SVE2_235_STAGE_R15_53 &&
        p->tw[level + 1].radix == 15 &&
        p->tw[level + 1].M == 16) {
      const int inner_M = M;
      AssertFatal(inner_M == 15 * 16,
                  "Bad nested R9xR15x16 topology level=%d N=%d M=%d\n",
                  level,
                  N,
                  inner_M);
      c16_t *inner_b = child_work;
      for (int outer = 0; outer < 9; outer++) {
        sve2_r15_53_stage_to_branch_major_q15(p, level + 1, b + outer * inner_M, inner_b);
        for (int inner = 0; inner < 15; inner++) {
          const int combined = 9 * inner + outer;
          sve2_dft16_q15_parent_scatter(inner_b + inner * 16, dst, 135, combined);
        }
      }
      return;
    }

    if (M == 12 && level + 1 == p->depth) {
      for (int br = 0; br < 9; br++)
        sve2_dft12_q15_parent_scatter(b + br * M, dst, 9, br);
      return;
    }
    if (M == 16 && level + 1 == p->depth) {
      for (int br = 0; br < 9; br++)
        sve2_dft16_q15_parent_scatter(b + br * M, dst, 9, br);
      return;
    }
    if (M == 32 && level + 1 == p->depth) {
      for (int br = 0; br < 9; br++)
        sve2_dft32_q15_parent_scatter(b + br * M, dst, 9, br);
      return;
    }
    if (M == 8 && level + 1 == p->depth) {
      int br = 0;
      for (; br + 3 < 9; br += 4)
        neon_dft8x4_branch_major_q15(b + br * M, y + br * M, M, M);
      for (; br < 9; br++)
        sve2_dft8_q15_special(b + br * M, y + br * M);
    } else {
      for (int br = 0; br < 9; br++)
        sve2_execute_mixed_plan_q15(p, level + 1, b + br * M, y + br * M, child_work);
    }

    uint32_t *dst32 = (uint32_t *)dst;
    for (int k = 0; k < M; k += 4) {
      const int rem = M - k < 4 ? M - k : 4;
      const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));
      const svbool_t pg32 = svwhilelt_b32((uint64_t)0, (uint64_t)rem);
      for (int br = 0; br < 9; br++) {
        const svint16_t a = svld1_s16(pg16, (const int16_t *)(y + br * M + k));
        const svuint32_t idx = svindex_u32((uint32_t)(9 * k + br), 9);
        svst1_scatter_u32index_u32(pg32, dst32, idx, svreinterpret_u32_s16(a));
      }
    }
    return;
  }

  if (r == 15 || r == 25) {
    int rr, c3, c5, first, second;
    AssertFatal(mixed_stage_decode(p->stage_code[level], &rr, &c3, &c5, &first, &second) && rr == r && second != 0,
                "Invalid fused SVE2 235 Q15 stage code=%u radix=%d\n",
                (unsigned)p->stage_code[level],
                r);
    (void)c3;
    (void)c5;

    /* R15/R25 fusion preserves both parent butterflies and their Q15 twiddle
     * points while removing the intermediate full-N store/reload. */
    const int B = first;
    const int A = second;
    const sve2_mixed_parent_twiddle_t *twB = &p->fused_first_tw[level];
    const sve2_mixed_parent_twiddle_t *twA = &p->fused_second_tw[level];
    AssertFatal(twB->radix == B && twB->M == A * M && twA->radix == A && twA->M == M,
                "Bad exact fused twiddles N=%d B=%d A=%d M=%d\n",
                N,
                B,
                A,
                M);

    c16_t stage1[25][4] __attribute__((aligned(64)));

    for (int off = 0; off < M; off += 4) {
      const int rem = M - off < 4 ? M - off : 4;
      const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));

      for (int a = 0; a < A; a++) {
        const int first_off = a * M + off;
        if (B == 3) {
          svint16_t x0 = svld1_s16(pg16, (const int16_t *)(src + a * M + off));
          svint16_t x1 = svld1_s16(pg16, (const int16_t *)(src + (A + a) * M + off));
          svint16_t x2 = svld1_s16(pg16, (const int16_t *)(src + (2 * A + a) * M + off));
          svint16_t z0, z1, z2;
          sve2_dft3_q15(x0, x1, x2, &z0, &z1, &z2);
          z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT3);
          z1 = sve2_cmul_q15(z1, svld1_s16(pg16, twB->q15[0] + 2 * first_off));
          z2 = sve2_cmul_q15(z2, svld1_s16(pg16, twB->q15[1] + 2 * first_off));
          svst1_s16(pg16, (int16_t *)stage1[a * B + 0], z0);
          svst1_s16(pg16, (int16_t *)stage1[a * B + 1], z1);
          svst1_s16(pg16, (int16_t *)stage1[a * B + 2], z2);
        } else {
          AssertFatal(B == 5, "Invalid exact fused first radix B=%d\n", B);
          svint16_t x0 = svld1_s16(pg16, (const int16_t *)(src + a * M + off));
          svint16_t x1 = svld1_s16(pg16, (const int16_t *)(src + (A + a) * M + off));
          svint16_t x2 = svld1_s16(pg16, (const int16_t *)(src + (2 * A + a) * M + off));
          svint16_t x3 = svld1_s16(pg16, (const int16_t *)(src + (3 * A + a) * M + off));
          svint16_t x4 = svld1_s16(pg16, (const int16_t *)(src + (4 * A + a) * M + off));
          svint16_t z0, z1, z2, z3, z4;
          sve2_dft5_q15(x0, x1, x2, x3, x4, &z0, &z1, &z2, &z3, &z4);
          z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT5);
          z1 = sve2_cmul_q15(z1, svld1_s16(pg16, twB->q15[0] + 2 * first_off));
          z2 = sve2_cmul_q15(z2, svld1_s16(pg16, twB->q15[1] + 2 * first_off));
          z3 = sve2_cmul_q15(z3, svld1_s16(pg16, twB->q15[2] + 2 * first_off));
          z4 = sve2_cmul_q15(z4, svld1_s16(pg16, twB->q15[3] + 2 * first_off));
          svst1_s16(pg16, (int16_t *)stage1[a * B + 0], z0);
          svst1_s16(pg16, (int16_t *)stage1[a * B + 1], z1);
          svst1_s16(pg16, (int16_t *)stage1[a * B + 2], z2);
          svst1_s16(pg16, (int16_t *)stage1[a * B + 3], z3);
          svst1_s16(pg16, (int16_t *)stage1[a * B + 4], z4);
        }
      }

      for (int bidx = 0; bidx < B; bidx++) {
        if (A == 3) {
          svint16_t x0 = svld1_s16(pg16, (const int16_t *)stage1[bidx]);
          svint16_t x1 = svld1_s16(pg16, (const int16_t *)stage1[B + bidx]);
          svint16_t x2 = svld1_s16(pg16, (const int16_t *)stage1[2 * B + bidx]);
          svint16_t z0, z1, z2;
          sve2_dft3_q15(x0, x1, x2, &z0, &z1, &z2);
          z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT3);
          z1 = sve2_cmul_q15(z1, svld1_s16(pg16, twA->q15[0] + 2 * off));
          z2 = sve2_cmul_q15(z2, svld1_s16(pg16, twA->q15[1] + 2 * off));
          const int br0 = bidx;
          const int br1 = bidx + B;
          const int br2 = bidx + B * 2;
          svst1_s16(pg16, (int16_t *)(b + br0 * M + off), z0);
          svst1_s16(pg16, (int16_t *)(b + br1 * M + off), z1);
          svst1_s16(pg16, (int16_t *)(b + br2 * M + off), z2);
        } else {
          AssertFatal(A == 5, "Invalid exact fused second radix A=%d\n", A);
          svint16_t x0 = svld1_s16(pg16, (const int16_t *)stage1[bidx]);
          svint16_t x1 = svld1_s16(pg16, (const int16_t *)stage1[B + bidx]);
          svint16_t x2 = svld1_s16(pg16, (const int16_t *)stage1[2 * B + bidx]);
          svint16_t x3 = svld1_s16(pg16, (const int16_t *)stage1[3 * B + bidx]);
          svint16_t x4 = svld1_s16(pg16, (const int16_t *)stage1[4 * B + bidx]);
          svint16_t z0, z1, z2, z3, z4;
          sve2_dft5_q15(x0, x1, x2, x3, x4, &z0, &z1, &z2, &z3, &z4);
          z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT5);
          z1 = sve2_cmul_q15(z1, svld1_s16(pg16, twA->q15[0] + 2 * off));
          z2 = sve2_cmul_q15(z2, svld1_s16(pg16, twA->q15[1] + 2 * off));
          z3 = sve2_cmul_q15(z3, svld1_s16(pg16, twA->q15[2] + 2 * off));
          z4 = sve2_cmul_q15(z4, svld1_s16(pg16, twA->q15[3] + 2 * off));
          const int br0 = bidx;
          const int br1 = bidx + B;
          const int br2 = bidx + B * 2;
          const int br3 = bidx + B * 3;
          const int br4 = bidx + B * 4;
          svst1_s16(pg16, (int16_t *)(b + br0 * M + off), z0);
          svst1_s16(pg16, (int16_t *)(b + br1 * M + off), z1);
          svst1_s16(pg16, (int16_t *)(b + br2 * M + off), z2);
          svst1_s16(pg16, (int16_t *)(b + br3 * M + off), z3);
          svst1_s16(pg16, (int16_t *)(b + br4 * M + off), z4);
        }
      }
    }

    if (M == 12 && level + 1 == p->depth) {
      for (int br = 0; br < r; br++)
        sve2_dft12_q15_parent_scatter(b + br * M, dst, r, br);
      return;
    }
    if (M == 16 && level + 1 == p->depth) {
      for (int br = 0; br < r; br++)
        sve2_dft16_q15_parent_scatter(b + br * M, dst, r, br);
      return;
    }
    if (M == 32 && level + 1 == p->depth) {
      for (int br = 0; br < r; br++)
        sve2_dft32_q15_parent_scatter(b + br * M, dst, r, br);
      return;
    }
    if (M == 8 && level + 1 == p->depth) {
      int br = 0;
      for (; br + 3 < r; br += 4)
        neon_dft8x4_branch_major_q15(b + br * M, y + br * M, M, M);
      for (; br < r; br++)
        sve2_dft8_q15_special(b + br * M, y + br * M);
    } else {
      for (int br = 0; br < r; br++)
        sve2_execute_mixed_plan_q15(p, level + 1, b + br * M, y + br * M, child_work);
    }

    uint32_t *dst32 = (uint32_t *)dst;
    for (int k = 0; k < M; k += 4) {
      const int rem = M - k < 4 ? M - k : 4;
      const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));
      const svbool_t pg32 = svwhilelt_b32((uint64_t)0, (uint64_t)rem);
      for (int br = 0; br < r; br++) {
        svint16_t a = svld1_s16(pg16, (const int16_t *)(y + br * M + k));
        const svuint32_t idx = svindex_u32((uint32_t)(r * k + br), (uint32_t)r);
        svst1_scatter_u32index_u32(pg32, dst32, idx, svreinterpret_u32_s16(a));
      }
    }
    return;
  }

  if (r == 3) {
    for (int off = 0; off < M; off += 4) {
      const int rem = M - off < 4 ? M - off : 4;
      const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));
      svint16_t x0 = svld1_s16(pg16, (const int16_t *)(src + off));
      svint16_t x1 = svld1_s16(pg16, (const int16_t *)(src + M + off));
      svint16_t x2 = svld1_s16(pg16, (const int16_t *)(src + 2 * M + off));
      svint16_t z0, z1, z2;
      sve2_dft3_q15(x0, x1, x2, &z0, &z1, &z2);
      z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT3);
      z1 = sve2_cmul_q15(z1, svld1_s16(pg16, tw->q15[0] + 2 * off));
      z2 = sve2_cmul_q15(z2, svld1_s16(pg16, tw->q15[1] + 2 * off));
      svst1_s16(pg16, (int16_t *)(b + off), z0);
      svst1_s16(pg16, (int16_t *)(b + M + off), z1);
      svst1_s16(pg16, (int16_t *)(b + 2 * M + off), z2);
    }
    /* If an outer R3 is followed by a terminal mixed stage ending in DFT16,
     * compose both output permutations and skip the complete child output
     * buffer.  This covers R3->R9->16, R3->R15_53->16 and R3->R25->16. */
    if (level + 2 == p->depth && p->tw[level + 1].M == 16) {
      const uint8_t next_code = p->stage_code[level + 1];
      int child_radix = 0;
      if (next_code == SVE2_235_STAGE_R9)
        child_radix = 9;
      else if (next_code == SVE2_235_STAGE_R15_53)
        child_radix = 15;
      else if (next_code == SVE2_235_STAGE_R25)
        child_radix = 25;

      if (child_radix != 0) {
        const int inner_M = M;
        AssertFatal(inner_M == child_radix * 16,
                    "Bad nested R3xR%dx16 topology level=%d N=%d M=%d\\n",
                    child_radix,
                    level,
                    N,
                    inner_M);
        c16_t *inner_b = child_work;
        for (int outer = 0; outer < 3; outer++) {
          const c16_t *child_src = b + outer * inner_M;
          if (child_radix == 9)
            sve2_radix9_stage_to_branch_major_q15(p, level + 1, child_src, inner_b);
          else if (child_radix == 15)
            sve2_r15_53_stage_to_branch_major_q15(p, level + 1, child_src, inner_b);
          else
            sve2_r25_stage_to_branch_major_q15(p, level + 1, child_src, inner_b);

          for (int inner = 0; inner < child_radix; inner++) {
            const int combined = 3 * inner + outer;
            sve2_dft16_q15_parent_scatter(inner_b + inner * 16, dst, 3 * child_radix, combined);
          }
        }
        return;
      }
    }

    if (M == 12 && level + 1 == p->depth) {
      for (int br = 0; br < 3; br++)
        sve2_dft12_q15_parent_scatter(b + br * M, dst, 3, br);
      return;
    }
    if (M == 16 && level + 1 == p->depth) {
      for (int br = 0; br < 3; br++)
        sve2_dft16_q15_parent_scatter(b + br * M, dst, 3, br);
      return;
    }
    if (M == 32 && level + 1 == p->depth) {
      for (int br = 0; br < 3; br++)
        sve2_dft32_q15_parent_scatter(b + br * M, dst, 3, br);
      return;
    }
    for (int br = 0; br < 3; br++)
      sve2_execute_mixed_plan_q15(p, level + 1, b + br * M, y + br * M, child_work);
    for (int k = 0; k < M; k += 4) {
      const int rem = M - k < 4 ? M - k : 4;
      const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));
      const svbool_t pg32 = svwhilelt_b32((uint64_t)0, (uint64_t)rem);
      const svint16_t a0 = svld1_s16(pg16, (const int16_t *)(y + k));
      const svint16_t a1 = svld1_s16(pg16, (const int16_t *)(y + M + k));
      const svint16_t a2 = svld1_s16(pg16, (const int16_t *)(y + 2 * M + k));
      svst3_u32(pg32,
                (uint32_t *)(dst + 3 * k),
                svcreate3_u32(svreinterpret_u32_s16(a0), svreinterpret_u32_s16(a1), svreinterpret_u32_s16(a2)));
    }
    return;
  }

  AssertFatal(r == 5, "Invalid SVE2 235 radix %d\n", r);
  for (int off = 0; off < M; off += 4) {
    const int rem = M - off < 4 ? M - off : 4;
    const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));
    svint16_t x0 = svld1_s16(pg16, (const int16_t *)(src + off));
    svint16_t x1 = svld1_s16(pg16, (const int16_t *)(src + M + off));
    svint16_t x2 = svld1_s16(pg16, (const int16_t *)(src + 2 * M + off));
    svint16_t x3 = svld1_s16(pg16, (const int16_t *)(src + 3 * M + off));
    svint16_t x4 = svld1_s16(pg16, (const int16_t *)(src + 4 * M + off));
    svint16_t z0, z1, z2, z3, z4;
    sve2_dft5_q15(x0, x1, x2, x3, x4, &z0, &z1, &z2, &z3, &z4);
    z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT5);
    z1 = sve2_cmul_q15(z1, svld1_s16(pg16, tw->q15[0] + 2 * off));
    z2 = sve2_cmul_q15(z2, svld1_s16(pg16, tw->q15[1] + 2 * off));
    z3 = sve2_cmul_q15(z3, svld1_s16(pg16, tw->q15[2] + 2 * off));
    z4 = sve2_cmul_q15(z4, svld1_s16(pg16, tw->q15[3] + 2 * off));
    svst1_s16(pg16, (int16_t *)(b + off), z0);
    svst1_s16(pg16, (int16_t *)(b + M + off), z1);
    svst1_s16(pg16, (int16_t *)(b + 2 * M + off), z2);
    svst1_s16(pg16, (int16_t *)(b + 3 * M + off), z3);
    svst1_s16(pg16, (int16_t *)(b + 4 * M + off), z4);
  }
  if (M == 12 && level + 1 == p->depth) {
    for (int br = 0; br < 5; br++)
      sve2_dft12_q15_parent_scatter(b + br * M, dst, 5, br);
    return;
  }
  if (M == 16 && level + 1 == p->depth) {
    for (int br = 0; br < 5; br++)
      sve2_dft16_q15_parent_scatter(b + br * M, dst, 5, br);
    return;
  }
  if (M == 32 && level + 1 == p->depth) {
    for (int br = 0; br < 5; br++)
      sve2_dft32_q15_parent_scatter(b + br * M, dst, 5, br);
    return;
  }
  if (M == 8 && level + 1 == p->depth) {
    neon_dft8x4_branch_major_q15(b, y, M, M);
    sve2_dft8_q15_special(b + 4 * M, y + 4 * M);
  } else {
    for (int br = 0; br < 5; br++)
      sve2_execute_mixed_plan_q15(p, level + 1, b + br * M, y + br * M, child_work);
  }
  uint32_t *dst32 = (uint32_t *)dst;
  for (int k = 0; k < M; k += 4) {
    const int rem = M - k < 4 ? M - k : 4;
    const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));
    const svbool_t pg32 = svwhilelt_b32((uint64_t)0, (uint64_t)rem);
    for (int br = 0; br < 5; br++) {
      const svint16_t a = svld1_s16(pg16, (const int16_t *)(y + br * M + k));
      const svuint32_t idx = svindex_u32((uint32_t)(5 * k + br), 5);
      svst1_scatter_u32index_u32(pg32, dst32, idx, svreinterpret_u32_s16(a));
    }
  }
}

SVE2_TARGET static void sve2_execute_mixed_plan_inverse_q15(const oai_dft_plan_t *p,
                                                            int level,
                                                            const c16_t *src,
                                                            c16_t *dst,
                                                            c16_t *work)
{
  if (level == p->depth) {
    sve2_execute_inverse_leaf_q15(p, src, dst);
    return;
  }

  const sve2_mixed_parent_twiddle_t *tw = &p->tw[level];
  const int N = tw->N;
  const int M = tw->M;
  const int r = tw->radix;
  c16_t *b = work;
  c16_t *y = work + N;
  c16_t *child_work = work + 2 * N;

  /* Two adjacent radix-9 stages are fused as radix-81: the 81 child outputs
   * remain branch-major and are scattered to natural order only once. */
  if (r == 9 && level + 1 < p->depth && p->stage_code[level + 1] == SVE2_235_STAGE_R9) {
    const sve2_mixed_parent_twiddle_t *tw_inner = &p->tw[level + 1];
    const int M1 = M;
    const int M2 = tw_inner->M;
    AssertFatal(tw_inner->N == M1 && tw_inner->radix == 9 && M1 == 9 * M2,
                "Bad SVE2 F81 topology level=%d N=%d M1=%d M2=%d\\n",
                level,
                N,
                M1,
                M2);

    sve2_radix9_inverse_stage_to_branch_major_q15(p, level, src, b);

    c16_t *combined_y = y;
    c16_t *inner_b = child_work;
    c16_t *grand_work = inner_b + M1;
    const int direct_leaf12 = M2 == 12 && level + 2 == p->depth;
    const int direct_leaf16 = M2 == 16 && level + 2 == p->depth;
    const int direct_leaf32 = M2 == 32 && level + 2 == p->depth;

    for (int outer = 0; outer < 9; outer++) {
      sve2_radix9_inverse_stage_to_branch_major_q15(p, level + 1, b + outer * M1, inner_b);
      if (direct_leaf12) {
        for (int inner = 0; inner < 9; inner++) {
          const int combined = 9 * inner + outer;
          sve2_idft12_q15_parent_scatter(inner_b + inner * M2, dst, 81, combined);
        }
      } else if (direct_leaf16) {
        for (int inner = 0; inner < 9; inner++) {
          const int combined = 9 * inner + outer;
          sve2_idft16_q15_parent_scatter(inner_b + inner * M2, dst, 81, combined);
        }
      } else if (direct_leaf32) {
        for (int inner = 0; inner < 9; inner++) {
          const int combined = 9 * inner + outer;
          sve2_idft32_q15_parent_scatter(inner_b + inner * M2, dst, 81, combined);
        }
      } else if (M2 == 8 && level + 2 == p->depth) {
        int inner = 0;
        for (; inner + 3 < 9; inner += 4)
          neon_idft8x4_branch_major_q15(inner_b + inner * M2, combined_y + (9 * inner + outer) * M2, M2, 9 * M2);
        for (; inner < 9; inner++) {
          const int combined = 9 * inner + outer;
          execute_inverse_leaf_q15(p, inner_b + inner * M2, combined_y + combined * M2);
        }
      } else {
        for (int inner = 0; inner < 9; inner++) {
          const int combined = 9 * inner + outer;
          sve2_execute_mixed_plan_inverse_q15(p, level + 2, inner_b + inner * M2, combined_y + combined * M2, grand_work);
        }
      }
    }

    if (direct_leaf12 || direct_leaf16 || direct_leaf32)
      return;

    /* Natural output index: 81*k + 9*inner + outer. */
    uint32_t *dst32 = (uint32_t *)dst;
    for (int k = 0; k < M2; k += 4) {
      const int rem = M2 - k < 4 ? M2 - k : 4;
      const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));
      const svbool_t pg32 = svwhilelt_b32((uint64_t)0, (uint64_t)rem);
      for (int br = 0; br < 81; br++) {
        const svint16_t a = svld1_s16(pg16, (const int16_t *)(combined_y + br * M2 + k));
        const svuint32_t idx = svindex_u32((uint32_t)(81 * k + br), 81);
        svst1_scatter_u32index_u32(pg32, dst32, idx, svreinterpret_u32_s16(a));
      }
    }
    return;
  }

  if (r == 9) {
    sve2_radix9_inverse_stage_to_branch_major_q15(p, level, src, b);

    /* Compose inverse R9 -> R5 -> IDFT16 directly into the 45-way output. */
    if (level + 2 == p->depth && p->stage_code[level + 1] == SVE2_235_STAGE_R5 &&
        p->tw[level + 1].M == 16) {
      const int inner_M = M;
      AssertFatal(inner_M == 5 * 16,
                  "Bad nested inverse R9xR5x16 topology level=%d N=%d M=%d\n",
                  level,
                  N,
                  inner_M);
      c16_t *inner_b = child_work;
      for (int outer = 0; outer < 9; outer++) {
        sve2_radix5_inverse_stage_to_branch_major_q15(p, level + 1, b + outer * inner_M, inner_b);
        for (int inner = 0; inner < 5; inner++) {
          const int combined = 9 * inner + outer;
          sve2_idft16_q15_parent_scatter(inner_b + inner * 16, dst, 45, combined);
        }
      }
      return;
    }

    if (M == 12 && level + 1 == p->depth) {
      for (int br = 0; br < 9; br++)
        sve2_idft12_q15_parent_scatter(b + br * M, dst, 9, br);
      return;
    }
    if (M == 16 && level + 1 == p->depth) {
      for (int br = 0; br < 9; br++)
        sve2_idft16_q15_parent_scatter(b + br * M, dst, 9, br);
      return;
    }
    if (M == 32 && level + 1 == p->depth) {
      for (int br = 0; br < 9; br++)
        sve2_idft32_q15_parent_scatter(b + br * M, dst, 9, br);
      return;
    }
    if (M == 8 && level + 1 == p->depth) {
      int br = 0;
      for (; br + 3 < 9; br += 4)
        neon_idft8x4_branch_major_q15(b + br * M, y + br * M, M, M);
      for (; br < 9; br++)
        execute_inverse_leaf_q15(p, b + br * M, y + br * M);
    } else {
      for (int br = 0; br < 9; br++)
        sve2_execute_mixed_plan_inverse_q15(p, level + 1, b + br * M, y + br * M, child_work);
    }

    uint32_t *dst32 = (uint32_t *)dst;
    for (int k = 0; k < M; k += 4) {
      const int rem = M - k < 4 ? M - k : 4;
      const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));
      const svbool_t pg32 = svwhilelt_b32((uint64_t)0, (uint64_t)rem);
      for (int br = 0; br < 9; br++) {
        const svint16_t a = svld1_s16(pg16, (const int16_t *)(y + br * M + k));
        const svuint32_t idx = svindex_u32((uint32_t)(9 * k + br), 9);
        svst1_scatter_u32index_u32(pg32, dst32, idx, svreinterpret_u32_s16(a));
      }
    }
    return;
  }

  if (r == 15 || r == 25) {
    int rr, c3, c5, first, second;
    AssertFatal(mixed_stage_decode(p->stage_code[level], &rr, &c3, &c5, &first, &second) && rr == r && second != 0,
                "Invalid fused SVE2 235 Q15 stage code=%u radix=%d\n",
                (unsigned)p->stage_code[level],
                r);
    (void)c3;
    (void)c5;

    /* R15/R25 fusion preserves both parent butterflies and their Q15 twiddle
     * points while removing the intermediate full-N store/reload. */
    const int B = first;
    const int A = second;
    const sve2_mixed_parent_twiddle_t *twB = &p->fused_first_tw[level];
    const sve2_mixed_parent_twiddle_t *twA = &p->fused_second_tw[level];
    AssertFatal(twB->radix == B && twB->M == A * M && twA->radix == A && twA->M == M,
                "Bad exact fused twiddles N=%d B=%d A=%d M=%d\n",
                N,
                B,
                A,
                M);

    c16_t stage1[25][4] __attribute__((aligned(64)));

    for (int off = 0; off < M; off += 4) {
      const int rem = M - off < 4 ? M - off : 4;
      const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));

      for (int a = 0; a < A; a++) {
        const int first_off = a * M + off;
        if (B == 3) {
          svint16_t x0 = svld1_s16(pg16, (const int16_t *)(src + a * M + off));
          svint16_t x1 = svld1_s16(pg16, (const int16_t *)(src + (A + a) * M + off));
          svint16_t x2 = svld1_s16(pg16, (const int16_t *)(src + (2 * A + a) * M + off));
          svint16_t z0, z1, z2;
          sve2_idft3_q15(x0, x1, x2, &z0, &z1, &z2);
          z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT3);
          z1 = sve2_cmul_q15(z1, svld1_s16(pg16, twB->q15_inv[0] + 2 * first_off));
          z2 = sve2_cmul_q15(z2, svld1_s16(pg16, twB->q15_inv[1] + 2 * first_off));
          svst1_s16(pg16, (int16_t *)stage1[a * B + 0], z0);
          svst1_s16(pg16, (int16_t *)stage1[a * B + 1], z1);
          svst1_s16(pg16, (int16_t *)stage1[a * B + 2], z2);
        } else {
          AssertFatal(B == 5, "Invalid exact fused first radix B=%d\n", B);
          svint16_t x0 = svld1_s16(pg16, (const int16_t *)(src + a * M + off));
          svint16_t x1 = svld1_s16(pg16, (const int16_t *)(src + (A + a) * M + off));
          svint16_t x2 = svld1_s16(pg16, (const int16_t *)(src + (2 * A + a) * M + off));
          svint16_t x3 = svld1_s16(pg16, (const int16_t *)(src + (3 * A + a) * M + off));
          svint16_t x4 = svld1_s16(pg16, (const int16_t *)(src + (4 * A + a) * M + off));
          svint16_t z0, z1, z2, z3, z4;
          sve2_idft5_q15(x0, x1, x2, x3, x4, &z0, &z1, &z2, &z3, &z4);
          z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT5);
          z1 = sve2_cmul_q15(z1, svld1_s16(pg16, twB->q15_inv[0] + 2 * first_off));
          z2 = sve2_cmul_q15(z2, svld1_s16(pg16, twB->q15_inv[1] + 2 * first_off));
          z3 = sve2_cmul_q15(z3, svld1_s16(pg16, twB->q15_inv[2] + 2 * first_off));
          z4 = sve2_cmul_q15(z4, svld1_s16(pg16, twB->q15_inv[3] + 2 * first_off));
          svst1_s16(pg16, (int16_t *)stage1[a * B + 0], z0);
          svst1_s16(pg16, (int16_t *)stage1[a * B + 1], z1);
          svst1_s16(pg16, (int16_t *)stage1[a * B + 2], z2);
          svst1_s16(pg16, (int16_t *)stage1[a * B + 3], z3);
          svst1_s16(pg16, (int16_t *)stage1[a * B + 4], z4);
        }
      }

      for (int bidx = 0; bidx < B; bidx++) {
        if (A == 3) {
          svint16_t x0 = svld1_s16(pg16, (const int16_t *)stage1[bidx]);
          svint16_t x1 = svld1_s16(pg16, (const int16_t *)stage1[B + bidx]);
          svint16_t x2 = svld1_s16(pg16, (const int16_t *)stage1[2 * B + bidx]);
          svint16_t z0, z1, z2;
          sve2_idft3_q15(x0, x1, x2, &z0, &z1, &z2);
          z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT3);
          z1 = sve2_cmul_q15(z1, svld1_s16(pg16, twA->q15_inv[0] + 2 * off));
          z2 = sve2_cmul_q15(z2, svld1_s16(pg16, twA->q15_inv[1] + 2 * off));
          const int br0 = bidx;
          const int br1 = bidx + B;
          const int br2 = bidx + B * 2;
          svst1_s16(pg16, (int16_t *)(b + br0 * M + off), z0);
          svst1_s16(pg16, (int16_t *)(b + br1 * M + off), z1);
          svst1_s16(pg16, (int16_t *)(b + br2 * M + off), z2);
        } else {
          AssertFatal(A == 5, "Invalid exact fused second radix A=%d\n", A);
          svint16_t x0 = svld1_s16(pg16, (const int16_t *)stage1[bidx]);
          svint16_t x1 = svld1_s16(pg16, (const int16_t *)stage1[B + bidx]);
          svint16_t x2 = svld1_s16(pg16, (const int16_t *)stage1[2 * B + bidx]);
          svint16_t x3 = svld1_s16(pg16, (const int16_t *)stage1[3 * B + bidx]);
          svint16_t x4 = svld1_s16(pg16, (const int16_t *)stage1[4 * B + bidx]);
          svint16_t z0, z1, z2, z3, z4;
          sve2_idft5_q15(x0, x1, x2, x3, x4, &z0, &z1, &z2, &z3, &z4);
          z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT5);
          z1 = sve2_cmul_q15(z1, svld1_s16(pg16, twA->q15_inv[0] + 2 * off));
          z2 = sve2_cmul_q15(z2, svld1_s16(pg16, twA->q15_inv[1] + 2 * off));
          z3 = sve2_cmul_q15(z3, svld1_s16(pg16, twA->q15_inv[2] + 2 * off));
          z4 = sve2_cmul_q15(z4, svld1_s16(pg16, twA->q15_inv[3] + 2 * off));
          const int br0 = bidx;
          const int br1 = bidx + B;
          const int br2 = bidx + B * 2;
          const int br3 = bidx + B * 3;
          const int br4 = bidx + B * 4;
          svst1_s16(pg16, (int16_t *)(b + br0 * M + off), z0);
          svst1_s16(pg16, (int16_t *)(b + br1 * M + off), z1);
          svst1_s16(pg16, (int16_t *)(b + br2 * M + off), z2);
          svst1_s16(pg16, (int16_t *)(b + br3 * M + off), z3);
          svst1_s16(pg16, (int16_t *)(b + br4 * M + off), z4);
        }
      }
    }

    /* Compose terminal inverse R15_35 -> R9 -> IDFT16 directly into the
     * 135-way output, avoiding the 144-point child output in y[]. */
    if (r == 15 && p->stage_code[level] == SVE2_235_STAGE_R15_35 &&
        level + 2 == p->depth && p->stage_code[level + 1] == SVE2_235_STAGE_R9 &&
        p->tw[level + 1].M == 16) {
      const int inner_M = M;
      AssertFatal(inner_M == 9 * 16,
                  "Bad nested inverse R15xR9x16 topology level=%d N=%d M=%d\n",
                  level,
                  N,
                  inner_M);
      c16_t *inner_b = child_work;
      for (int outer = 0; outer < 15; outer++) {
        sve2_radix9_inverse_stage_to_branch_major_q15(p, level + 1, b + outer * inner_M, inner_b);
        for (int inner = 0; inner < 9; inner++) {
          const int combined = 15 * inner + outer;
          sve2_idft16_q15_parent_scatter(inner_b + inner * 16, dst, 135, combined);
        }
      }
      return;
    }

    if (M == 12 && level + 1 == p->depth) {
      for (int br = 0; br < r; br++)
        sve2_idft12_q15_parent_scatter(b + br * M, dst, r, br);
      return;
    }
    if (M == 16 && level + 1 == p->depth) {
      for (int br = 0; br < r; br++)
        sve2_idft16_q15_parent_scatter(b + br * M, dst, r, br);
      return;
    }
    if (M == 32 && level + 1 == p->depth) {
      for (int br = 0; br < r; br++)
        sve2_idft32_q15_parent_scatter(b + br * M, dst, r, br);
      return;
    }
    if (M == 8 && level + 1 == p->depth) {
      int br = 0;
      for (; br + 3 < r; br += 4)
        neon_idft8x4_branch_major_q15(b + br * M, y + br * M, M, M);
      for (; br < r; br++)
        execute_inverse_leaf_q15(p, b + br * M, y + br * M);
    } else {
      for (int br = 0; br < r; br++)
        sve2_execute_mixed_plan_inverse_q15(p, level + 1, b + br * M, y + br * M, child_work);
    }

    uint32_t *dst32 = (uint32_t *)dst;
    for (int k = 0; k < M; k += 4) {
      const int rem = M - k < 4 ? M - k : 4;
      const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));
      const svbool_t pg32 = svwhilelt_b32((uint64_t)0, (uint64_t)rem);
      for (int br = 0; br < r; br++) {
        svint16_t a = svld1_s16(pg16, (const int16_t *)(y + br * M + k));
        const svuint32_t idx = svindex_u32((uint32_t)(r * k + br), (uint32_t)r);
        svst1_scatter_u32index_u32(pg32, dst32, idx, svreinterpret_u32_s16(a));
      }
    }
    return;
  }

  if (r == 3) {
    for (int off = 0; off < M; off += 4) {
      const int rem = M - off < 4 ? M - off : 4;
      const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));
      svint16_t x0 = svld1_s16(pg16, (const int16_t *)(src + off));
      svint16_t x1 = svld1_s16(pg16, (const int16_t *)(src + M + off));
      svint16_t x2 = svld1_s16(pg16, (const int16_t *)(src + 2 * M + off));
      svint16_t z0, z1, z2;
      sve2_idft3_q15(x0, x1, x2, &z0, &z1, &z2);
      z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT3);
      z1 = sve2_cmul_q15(z1, svld1_s16(pg16, tw->q15_inv[0] + 2 * off));
      z2 = sve2_cmul_q15(z2, svld1_s16(pg16, tw->q15_inv[1] + 2 * off));
      svst1_s16(pg16, (int16_t *)(b + off), z0);
      svst1_s16(pg16, (int16_t *)(b + M + off), z1);
      svst1_s16(pg16, (int16_t *)(b + 2 * M + off), z2);
    }
    /* Compose inverse R3 -> R9/R25 -> IDFT16 into the final output. */
    if (level + 2 == p->depth && p->tw[level + 1].M == 16) {
      const uint8_t next_code = p->stage_code[level + 1];
      int child_radix = 0;
      if (next_code == SVE2_235_STAGE_R9)
        child_radix = 9;
      else if (next_code == SVE2_235_STAGE_R25)
        child_radix = 25;

      if (child_radix != 0) {
        const int inner_M = M;
        AssertFatal(inner_M == child_radix * 16,
                    "Bad nested inverse R3xR%dx16 topology level=%d N=%d M=%d\n",
                    child_radix,
                    level,
                    N,
                    inner_M);
        c16_t *inner_b = child_work;
        for (int outer = 0; outer < 3; outer++) {
          const c16_t *child_src = b + outer * inner_M;
          if (child_radix == 9)
            sve2_radix9_inverse_stage_to_branch_major_q15(p, level + 1, child_src, inner_b);
          else
            sve2_r25_inverse_stage_to_branch_major_q15(p, level + 1, child_src, inner_b);

          for (int inner = 0; inner < child_radix; inner++) {
            const int combined = 3 * inner + outer;
            sve2_idft16_q15_parent_scatter(inner_b + inner * 16, dst, 3 * child_radix, combined);
          }
        }
        return;
      }
    }

    if (M == 12 && level + 1 == p->depth) {
      for (int br = 0; br < 3; br++)
        sve2_idft12_q15_parent_scatter(b + br * M, dst, 3, br);
      return;
    }
    if (M == 16 && level + 1 == p->depth) {
      for (int br = 0; br < 3; br++)
        sve2_idft16_q15_parent_scatter(b + br * M, dst, 3, br);
      return;
    }
    if (M == 32 && level + 1 == p->depth) {
      for (int br = 0; br < 3; br++)
        sve2_idft32_q15_parent_scatter(b + br * M, dst, 3, br);
      return;
    }
    for (int br = 0; br < 3; br++)
      sve2_execute_mixed_plan_inverse_q15(p, level + 1, b + br * M, y + br * M, child_work);
    for (int k = 0; k < M; k += 4) {
      const int rem = M - k < 4 ? M - k : 4;
      const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));
      const svbool_t pg32 = svwhilelt_b32((uint64_t)0, (uint64_t)rem);
      const svint16_t a0 = svld1_s16(pg16, (const int16_t *)(y + k));
      const svint16_t a1 = svld1_s16(pg16, (const int16_t *)(y + M + k));
      const svint16_t a2 = svld1_s16(pg16, (const int16_t *)(y + 2 * M + k));
      svst3_u32(pg32,
                (uint32_t *)(dst + 3 * k),
                svcreate3_u32(svreinterpret_u32_s16(a0), svreinterpret_u32_s16(a1), svreinterpret_u32_s16(a2)));
    }
    return;
  }

  AssertFatal(r == 5, "Invalid SVE2 235 radix %d\n", r);
  for (int off = 0; off < M; off += 4) {
    const int rem = M - off < 4 ? M - off : 4;
    const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));
    svint16_t x0 = svld1_s16(pg16, (const int16_t *)(src + off));
    svint16_t x1 = svld1_s16(pg16, (const int16_t *)(src + M + off));
    svint16_t x2 = svld1_s16(pg16, (const int16_t *)(src + 2 * M + off));
    svint16_t x3 = svld1_s16(pg16, (const int16_t *)(src + 3 * M + off));
    svint16_t x4 = svld1_s16(pg16, (const int16_t *)(src + 4 * M + off));
    svint16_t z0, z1, z2, z3, z4;
    sve2_idft5_q15(x0, x1, x2, x3, x4, &z0, &z1, &z2, &z3, &z4);
    z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT5);
    z1 = sve2_cmul_q15(z1, svld1_s16(pg16, tw->q15_inv[0] + 2 * off));
    z2 = sve2_cmul_q15(z2, svld1_s16(pg16, tw->q15_inv[1] + 2 * off));
    z3 = sve2_cmul_q15(z3, svld1_s16(pg16, tw->q15_inv[2] + 2 * off));
    z4 = sve2_cmul_q15(z4, svld1_s16(pg16, tw->q15_inv[3] + 2 * off));
    svst1_s16(pg16, (int16_t *)(b + off), z0);
    svst1_s16(pg16, (int16_t *)(b + M + off), z1);
    svst1_s16(pg16, (int16_t *)(b + 2 * M + off), z2);
    svst1_s16(pg16, (int16_t *)(b + 3 * M + off), z3);
    svst1_s16(pg16, (int16_t *)(b + 4 * M + off), z4);
  }
  if (M == 12 && level + 1 == p->depth) {
    for (int br = 0; br < 5; br++)
      sve2_idft12_q15_parent_scatter(b + br * M, dst, 5, br);
    return;
  }
  if (M == 16 && level + 1 == p->depth) {
    for (int br = 0; br < 5; br++)
      sve2_idft16_q15_parent_scatter(b + br * M, dst, 5, br);
    return;
  }
  if (M == 32 && level + 1 == p->depth) {
    for (int br = 0; br < 5; br++)
      sve2_idft32_q15_parent_scatter(b + br * M, dst, 5, br);
    return;
  }
  if (M == 8 && level + 1 == p->depth) {
    neon_idft8x4_branch_major_q15(b, y, M, M);
    execute_inverse_leaf_q15(p, b + 4 * M, y + 4 * M);
  } else {
    for (int br = 0; br < 5; br++)
      sve2_execute_mixed_plan_inverse_q15(p, level + 1, b + br * M, y + br * M, child_work);
  }
  uint32_t *dst32 = (uint32_t *)dst;
  for (int k = 0; k < M; k += 4) {
    const int rem = M - k < 4 ? M - k : 4;
    const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));
    const svbool_t pg32 = svwhilelt_b32((uint64_t)0, (uint64_t)rem);
    for (int br = 0; br < 5; br++) {
      const svint16_t a = svld1_s16(pg16, (const int16_t *)(y + br * M + k));
      const svuint32_t idx = svindex_u32((uint32_t)(5 * k + br), 5);
      svst1_scatter_u32index_u32(pg32, dst32, idx, svreinterpret_u32_s16(a));
    }
  }
}

typedef struct {
  neon_m128i C16_RE_RE_q15_native[4] __attribute__((aligned(64)));
  neon_m128i C16_IM_SIGNED_q15_native[4] __attribute__((aligned(64)));
  neon_m128i C16_RE_RE_q15_native_inverse[4] __attribute__((aligned(64)));
  neon_m128i C16_IM_SIGNED_q15_native_inverse[4] __attribute__((aligned(64)));
  neon_m128i C32_RE_RE_q15_native[4][2] __attribute__((aligned(64)));
  neon_m128i C32_IM_SIGNED_q15_native[4][2] __attribute__((aligned(64)));
  neon_m128i C32_RE_RE_q15_native_inverse[4][2] __attribute__((aligned(64)));
  neon_m128i C32_IM_SIGNED_q15_native_inverse[4][2] __attribute__((aligned(64)));

  oc_cq15x8_t C64_RE_RE_q15_256[64] __attribute__((aligned(64)));
  oc_cq15x8_t C64_IM_SIGNED_q15_256[64] __attribute__((aligned(64)));
  oc_cq15x8_t C64_RE_RE_q15_256_inverse[64] __attribute__((aligned(64)));
  oc_cq15x8_t C64_IM_SIGNED_q15_256_inverse[64] __attribute__((aligned(64)));

  oc_cq15x8_t W128_RE_RE_q15_256[8] __attribute__((aligned(64)));
  oc_cq15x8_t W128_IM_SIGNED_q15_256[8] __attribute__((aligned(64)));
  oc_cq15x8_t W128_RE_RE_q15_256_inverse[8] __attribute__((aligned(64)));
  oc_cq15x8_t W128_IM_SIGNED_q15_256_inverse[8] __attribute__((aligned(64)));

  int initialized;
} dft64_q15_twiddle_t;

static dft64_q15_twiddle_t g_dft64f_tw;

/* Broadcast W64^(r*q)/8 tables for eight DFT64 transforms in parallel. */
static oc_cq15x8_t g_dft64_batch_re_re_q15[2][8][8] __attribute__((aligned(64)));
static oc_cq15x8_t g_dft64_batch_im_signed_q15[2][8][8] __attribute__((aligned(64)));

/* DFT64 R4 twiddles packed for four consecutive complex bins. */
static neon_m128i g_dft64_r4_re_re_q15[2][3][4] __attribute__((aligned(64)));
static neon_m128i g_dft64_r4_im_signed_q15[2][3][4] __attribute__((aligned(64)));

static inline int16_t q15_twiddle_32768(float x)
{
  int v = (int)lrintf(x * 32768.0f);
  if (v > 32767)
    v = 32767;
  if (v < -32767)
    v = -32767;
  return (int16_t)v;
}

static void init_native_q15_leaf_twiddles(void)
{
  dft64_q15_twiddle_t *tw = &g_dft64f_tw;
  if (tw->initialized)
    return;

  /* Native DFT16 coefficients, forward and inverse. */
  for (int dir_slot = 0; dir_slot < 2; dir_slot++) {
    const float sign = dir_slot == 0 ? -1.0f : 1.0f;
    for (int k = 0; k < 4; k++) {
      int16_t re_re[8] __attribute__((aligned(16)));
      int16_t im_signed[8] __attribute__((aligned(16)));
      for (int r = 0; r < 4; r++) {
        const float a = sign * 2.0f * (float)M_PI * (float)(k * r) / 16.0f;
        const int16_t wr = q15_twiddle_32768(cosf(a));
        const int16_t wi = q15_twiddle_32768(sinf(a));
        re_re[2 * r + 0] = wr;
        re_re[2 * r + 1] = wr;
        im_signed[2 * r + 0] = (int16_t)-wi;
        im_signed[2 * r + 1] = wi;
      }
      const neon_m128i vr = neon128_load_i((const neon_m128i *)re_re);
      const neon_m128i vi = neon128_load_i((const neon_m128i *)im_signed);
      if (dir_slot == 0) {
        tw->C16_RE_RE_q15_native[k] = vr;
        tw->C16_IM_SIGNED_q15_native[k] = vi;
      } else {
        tw->C16_RE_RE_q15_native_inverse[k] = vr;
        tw->C16_IM_SIGNED_q15_native_inverse[k] = vi;
      }
    }
  }

  /* Native DFT32 coefficients, low/high groups of four complex lanes. */
  for (int dir_slot = 0; dir_slot < 2; dir_slot++) {
    const float sign = dir_slot == 0 ? -1.0f : 1.0f;
    for (int k = 0; k < 4; k++) {
      for (int half = 0; half < 2; half++) {
        int16_t re_re[8] __attribute__((aligned(16)));
        int16_t im_signed[8] __attribute__((aligned(16)));
        for (int lane = 0; lane < 4; lane++) {
          const int r = 4 * half + lane;
          const float a = sign * 2.0f * (float)M_PI * (float)(k * r) / 32.0f;
          const int16_t wr = q15_twiddle_32768(cosf(a));
          const int16_t wi = q15_twiddle_32768(sinf(a));
          re_re[2 * lane + 0] = wr;
          re_re[2 * lane + 1] = wr;
          im_signed[2 * lane + 0] = (int16_t)-wi;
          im_signed[2 * lane + 1] = wi;
        }
        const neon_m128i vr = neon128_load_i((const neon_m128i *)re_re);
        const neon_m128i vi = neon128_load_i((const neon_m128i *)im_signed);
        if (dir_slot == 0) {
          tw->C32_RE_RE_q15_native[k][half] = vr;
          tw->C32_IM_SIGNED_q15_native[k][half] = vi;
        } else {
          tw->C32_RE_RE_q15_native_inverse[k][half] = vr;
          tw->C32_IM_SIGNED_q15_native_inverse[k][half] = vi;
        }
      }
    }
  }

  /* Eight-complex DFT64 coefficient packs, including the 1/8 unitary scale. */
  for (int dir_slot = 0; dir_slot < 2; dir_slot++) {
    const float sign = dir_slot == 0 ? -1.0f : 1.0f;
    for (int k = 0; k < 64; k++) {
      int16_t re_re[16] __attribute__((aligned(32)));
      int16_t im_signed[16] __attribute__((aligned(32)));
      for (int r = 0; r < 8; r++) {
        const float a = sign * 2.0f * (float)M_PI * (float)(k * r) / 64.0f;
        const int16_t wr = q15_twiddle_32768(cosf(a) * 0.125f);
        const int16_t wi = q15_twiddle_32768(sinf(a) * 0.125f);
        re_re[2 * r + 0] = wr;
        re_re[2 * r + 1] = wr;
        im_signed[2 * r + 0] = (int16_t)-wi;
        im_signed[2 * r + 1] = wi;
      }
      oc_cq15x8_t vr = oc_cq15x8_load((const oc_cq15x8_t *)re_re);
      oc_cq15x8_t vi = oc_cq15x8_load((const oc_cq15x8_t *)im_signed);
      if (dir_slot == 0) {
        tw->C64_RE_RE_q15_256[k] = vr;
        tw->C64_IM_SIGNED_q15_256[k] = vi;
      } else {
        tw->C64_RE_RE_q15_256_inverse[k] = vr;
        tw->C64_IM_SIGNED_q15_256_inverse[k] = vi;
      }
    }

    /* Outer DFT128 radix-2 twiddles, scale fused as 1/sqrt(2). */
    for (int b = 0; b < 8; b++) {
      int16_t re_re[16] __attribute__((aligned(32)));
      int16_t im_signed[16] __attribute__((aligned(32)));
      for (int r = 0; r < 8; r++) {
        const int n = 8 * b + r;
        const float a = sign * 2.0f * (float)M_PI * (float)n / 128.0f;
        const int16_t wr = q15_twiddle_32768(cosf(a) * (1.0f / sqrtf(2.0f)));
        const int16_t wi = q15_twiddle_32768(sinf(a) * (1.0f / sqrtf(2.0f)));
        re_re[2 * r + 0] = wr;
        re_re[2 * r + 1] = wr;
        im_signed[2 * r + 0] = (int16_t)-wi;
        im_signed[2 * r + 1] = wi;
      }
      oc_cq15x8_t vr = oc_cq15x8_load((const oc_cq15x8_t *)re_re);
      oc_cq15x8_t vi = oc_cq15x8_load((const oc_cq15x8_t *)im_signed);
      if (dir_slot == 0) {
        tw->W128_RE_RE_q15_256[b] = vr;
        tw->W128_IM_SIGNED_q15_256[b] = vi;
      } else {
        tw->W128_RE_RE_q15_256_inverse[b] = vr;
        tw->W128_IM_SIGNED_q15_256_inverse[b] = vi;
      }
    }

    /* Batched radix-8 DFT64 coefficients, also scaled by 1/8. */
    for (int r = 0; r < 8; r++) {
      for (int q = 0; q < 8; q++) {
        const float a = sign * 2.0f * (float)M_PI * (float)(r * q) / 64.0f;
        const int16_t wr = q15_twiddle_32768(cosf(a) * 0.125f);
        const int16_t wi = q15_twiddle_32768(sinf(a) * 0.125f);
        g_dft64_batch_re_re_q15[dir_slot][r][q] = oc_cq15x8_set1_i16(wr);
        g_dft64_batch_im_signed_q15[dir_slot][r][q] =
            oc_cq15x8_setr_i16(-wi, wi, -wi, wi, -wi, wi, -wi, wi, -wi, wi, -wi, wi, -wi, wi, -wi, wi);
      }
    }

    /* Native radix-4 DFT64 outer-stage coefficients, scale fused as 1/2. */
    for (int branch = 1; branch < 4; branch++) {
      for (int block = 0; block < 4; block++) {
        int16_t re_re[8] __attribute__((aligned(16)));
        int16_t im_signed[8] __attribute__((aligned(16)));
        for (int lane = 0; lane < 4; lane++) {
          const int k = 4 * block + lane;
          const float a = sign * 2.0f * (float)M_PI * (float)(branch * k) / 64.0f;
          const int16_t wr = q15_twiddle_32768(0.5f * cosf(a));
          const int16_t wi = q15_twiddle_32768(0.5f * sinf(a));
          re_re[2 * lane + 0] = wr;
          re_re[2 * lane + 1] = wr;
          im_signed[2 * lane + 0] = (int16_t)-wi;
          im_signed[2 * lane + 1] = wi;
        }
        g_dft64_r4_re_re_q15[dir_slot][branch - 1][block] = neon128_load_i((const neon_m128i *)re_re);
        g_dft64_r4_im_signed_q15[dir_slot][branch - 1][block] = neon128_load_i((const neon_m128i *)im_signed);
      }
    }
  }

  tw->initialized = 1;
}

static inline oc_cq15x8_t c16_mul_q15_simd256(oc_cq15x8_t x, oc_cq15x8_t w_re_negim, oc_cq15x8_t w_im_re)
{
  oc_cq15x8_t round = oc_cq15x8_set1_i32(1 << 14);

  oc_cq15x8_t re32 = oc_cq15x8_madd_i16(x, w_re_negim);
  oc_cq15x8_t im32 = oc_cq15x8_madd_i16(x, w_im_re);

  re32 = oc_cq15x8_srai_i32(oc_cq15x8_add_i32(re32, round), 15);
  im32 = oc_cq15x8_srai_i32(oc_cq15x8_add_i32(im32, round), 15);

  oc_cq15x8_t packed = oc_cq15x8_packs_i32(re32, im32);

  const oc_cq15x8_t mask =
      oc_cq15x8_set_i8(15, 14, 7, 6, 13, 12, 5, 4, 11, 10, 3, 2, 9, 8, 1, 0, 15, 14, 7, 6, 13, 12, 5, 4, 11, 10, 3, 2, 9, 8, 1, 0);

  return oc_cq15x8_shuffle_i8(packed, mask);
}

static inline void
sr_combine_simd(const c16_t *E, const c16_t *O1, const c16_t *O3, c16_t *y, int N, const sr_twiddle_simd_t *tw, dft_dir_t dir)
{
  int half = N / 2;
  int quarter = N / 4;

  const oc_cq15x8_t swap_mask =
      oc_cq15x8_setr_i8(2, 3, 0, 1, 6, 7, 4, 5, 10, 11, 8, 9, 14, 15, 12, 13, 2, 3, 0, 1, 6, 7, 4, 5, 10, 11, 8, 9, 14, 15, 12, 13);

  oc_cq15x8_t sign_mask;

  if (dir == DFT_DIR_FORWARD) {
    sign_mask = oc_cq15x8_setr_i16(1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1);
  } else {
    sign_mask = oc_cq15x8_setr_i16(-1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1);
  }

  const oc_cq15x8_t sqrt2_inv = oc_cq15x8_set1_i16(ONE_OVER_SQRT2_Q15);

  for (int b = 0; b < tw->blocks; b++) {
    int k = 8 * b;

    oc_cq15x8_t O1v = oc_cq15x8_load((const oc_cq15x8_t *)(O1 + k));
    oc_cq15x8_t O3v = oc_cq15x8_load((const oc_cq15x8_t *)(O3 + k));

    oc_cq15x8_t t1 = c16_mul_q15_simd256(O1v, tw->W1_RE_NEGIM[b], tw->W1_IM_RE[b]);

    oc_cq15x8_t t2 = c16_mul_q15_simd256(O3v, tw->W3_RE_NEGIM[b], tw->W3_IM_RE[b]);

    oc_cq15x8_t a = oc_cq15x8_add_i16(t1, t2);
    oc_cq15x8_t d = oc_cq15x8_sub_i16(t1, t2);

    oc_cq15x8_t bval = oc_cq15x8_shuffle_i8(d, swap_mask);
    bval = oc_cq15x8_sign_i16(bval, sign_mask);

    oc_cq15x8_t E0 = oc_cq15x8_load((const oc_cq15x8_t *)(E + k));
    oc_cq15x8_t E1 = oc_cq15x8_load((const oc_cq15x8_t *)(E + k + quarter));

    E0 = oc_cq15x8_mulhrs_i16(E0, sqrt2_inv);
    E1 = oc_cq15x8_mulhrs_i16(E1, sqrt2_inv);

    oc_cq15x8_t Y0 = oc_cq15x8_add_i16(E0, a);
    oc_cq15x8_t Y2 = oc_cq15x8_sub_i16(E0, a);
    oc_cq15x8_t Y1 = oc_cq15x8_add_i16(E1, bval);
    oc_cq15x8_t Y3 = oc_cq15x8_sub_i16(E1, bval);

    oc_cq15x8_store((oc_cq15x8_t *)&y[k], Y0);
    oc_cq15x8_store((oc_cq15x8_t *)&y[k + quarter], Y1);
    oc_cq15x8_store((oc_cq15x8_t *)&y[k + half], Y2);
    oc_cq15x8_store((oc_cq15x8_t *)&y[k + 3 * quarter], Y3);
  }
}

static inline void pack_split_radix_input_neon_fused(const c16_t *__restrict x, c16_t *__restrict sub_in, int N)
{
  _Static_assert(sizeof(c16_t) == 4, "c16_t must be 32-bit");

  const int half = N >> 1;
  const int quarter = N >> 2;

  c16_t *__restrict E_in = sub_in;
  c16_t *__restrict O1_in = sub_in + half;
  c16_t *__restrict O3_in = sub_in + half + quarter;

  const oc_cq15x8_t idx = oc_cq15x8_setr_i32(0, 2, 4, 6, 1, 5, 3, 7);

  int in = 0;
  int e = 0;
  int o = 0;

  for (; in + 32 <= N; in += 32, e += 16, o += 8) {
    oc_cq15x8_t v0 = oc_cq15x8_loadu((const oc_cq15x8_t *)&x[in + 0]);

    oc_cq15x8_t v1 = oc_cq15x8_loadu((const oc_cq15x8_t *)&x[in + 8]);

    oc_cq15x8_t v2 = oc_cq15x8_loadu((const oc_cq15x8_t *)&x[in + 16]);

    oc_cq15x8_t v3 = oc_cq15x8_loadu((const oc_cq15x8_t *)&x[in + 24]);

    /*
     * p0 = [x0,  x2,  x4,  x6,  x1,  x5,  x3,  x7]
     * p1 = [x8,  x10, x12, x14, x9,  x13, x11, x15]
     * p2 = [x16, x18, x20, x22, x17, x21, x19, x23]
     * p3 = [x24, x26, x28, x30, x25, x29, x27, x31]
     */
    oc_cq15x8_t p0 = oc_cq15x8_permute_i32(v0, idx);
    oc_cq15x8_t p1 = oc_cq15x8_permute_i32(v1, idx);
    oc_cq15x8_t p2 = oc_cq15x8_permute_i32(v2, idx);
    oc_cq15x8_t p3 = oc_cq15x8_permute_i32(v3, idx);

    /*
     * E output:
     *
     * low128(p0) = x0,  x2,  x4,  x6
     * low128(p1) = x8,  x10, x12, x14
     * low128(p2) = x16, x18, x20, x22
     * low128(p3) = x24, x26, x28, x30
     */
    neon128_store_i((neon_m128i *)&E_in[e + 0], oc_cq15x8_low128(p0));

    neon128_store_i((neon_m128i *)&E_in[e + 4], oc_cq15x8_low128(p1));

    neon128_store_i((neon_m128i *)&E_in[e + 8], oc_cq15x8_low128(p2));

    neon128_store_i((neon_m128i *)&E_in[e + 12], oc_cq15x8_low128(p3));

    /*
     * high128(p0) = x1,  x5,  x3,  x7
     * high128(p1) = x9,  x13, x11, x15
     * high128(p2) = x17, x21, x19, x23
     * high128(p3) = x25, x29, x27, x31
     */
    neon_m128i h0 = oc_cq15x8_extract_half(p0, 1);
    neon_m128i h1 = oc_cq15x8_extract_half(p1, 1);
    neon_m128i h2 = oc_cq15x8_extract_half(p2, 1);
    neon_m128i h3 = oc_cq15x8_extract_half(p3, 1);

    /*
     * O1:
     * unpacklo_epi64(h0,h1) = x1,  x5,  x9,  x13
     * unpacklo_epi64(h2,h3) = x17, x21, x25, x29
     *
     * O3:
     * unpackhi_epi64(h0,h1) = x3,  x7,  x11, x15
     * unpackhi_epi64(h2,h3) = x19, x23, x27, x31
     */
    neon_m128i o1_0 = neon128_unpacklo_i64(h0, h1);
    neon_m128i o3_0 = neon128_unpackhi_i64(h0, h1);

    neon_m128i o1_1 = neon128_unpacklo_i64(h2, h3);
    neon_m128i o3_1 = neon128_unpackhi_i64(h2, h3);

    neon128_store_i((neon_m128i *)&O1_in[o + 0], o1_0);
    neon128_store_i((neon_m128i *)&O1_in[o + 4], o1_1);

    neon128_store_i((neon_m128i *)&O3_in[o + 0], o3_0);
    neon128_store_i((neon_m128i *)&O3_in[o + 4], o3_1);
  }
}

/*
 * Four independent DFT16 butterflies in parallel.
 *
 * The vector form avoids a store/reload round trip when the caller already
 * owns the 16 input vectors, as in the outer radix-16 and radix-32 stages.
 */

/*
 * Specialized contiguous DFT512 / DFT1024 kernels.
 *
 * DFT512  = radix-2 DIF -> two DFT256 -> 2-way interleave.
 * DFT1024 = radix-4 DIF -> four DFT256 -> 4-way interleave.
 *
 * These are not split-radix kernels. All branch inputs are contiguous.
 */

static inline oc_cq15x8_t swap_complex_pairs_i16_256(oc_cq15x8_t a)
{
  return oc_cq15x8_make(vrev32q_s16(a.val[0]), vrev32q_s16(a.val[1]));
}

static inline oc_cq15x8_t mul_j_i16_256(oc_cq15x8_t z)
{
  const int16x8_t sign = {-1, 1, -1, 1, -1, 1, -1, 1};
  return oc_cq15x8_make(vmulq_s16(vrev32q_s16(z.val[0]), sign), vmulq_s16(vrev32q_s16(z.val[1]), sign));
}

static inline oc_cq15x8_t mul_minus_j_i16_256(oc_cq15x8_t z)
{
  const int16x8_t sign = {1, -1, 1, -1, 1, -1, 1, -1};
  return oc_cq15x8_make(vmulq_s16(vrev32q_s16(z.val[0]), sign), vmulq_s16(vrev32q_s16(z.val[1]), sign));
}

static inline oc_cq15x8_t mul_minus_j_dir_i16_256(oc_cq15x8_t z, dft_dir_t dir)
{
  return (dir == DFT_DIR_FORWARD) ? mul_minus_j_i16_256(z) : mul_j_i16_256(z);
}

static inline oc_cq15x8_t mul_plus_j_dir_i16_256(oc_cq15x8_t z, dft_dir_t dir)
{
  return (dir == DFT_DIR_FORWARD) ? mul_j_i16_256(z) : mul_minus_j_i16_256(z);
}

static inline void transpose8_complex_i16_256(oc_cq15x8_t *r0,
                                              oc_cq15x8_t *r1,
                                              oc_cq15x8_t *r2,
                                              oc_cq15x8_t *r3,
                                              oc_cq15x8_t *r4,
                                              oc_cq15x8_t *r5,
                                              oc_cq15x8_t *r6,
                                              oc_cq15x8_t *r7)
{
  const oc_cq15x8_t a = *r0;
  const oc_cq15x8_t b = *r1;
  const oc_cq15x8_t c = *r2;
  const oc_cq15x8_t d = *r3;
  const oc_cq15x8_t e = *r4;
  const oc_cq15x8_t f = *r5;
  const oc_cq15x8_t g = *r6;
  const oc_cq15x8_t h = *r7;

  const oc_cq15x8_t t0 = oc_cq15x8_unpacklo_i32(a, b);
  const oc_cq15x8_t t1 = oc_cq15x8_unpackhi_i32(a, b);
  const oc_cq15x8_t t2 = oc_cq15x8_unpacklo_i32(c, d);
  const oc_cq15x8_t t3 = oc_cq15x8_unpackhi_i32(c, d);
  const oc_cq15x8_t t4 = oc_cq15x8_unpacklo_i32(e, f);
  const oc_cq15x8_t t5 = oc_cq15x8_unpackhi_i32(e, f);
  const oc_cq15x8_t t6 = oc_cq15x8_unpacklo_i32(g, h);
  const oc_cq15x8_t t7 = oc_cq15x8_unpackhi_i32(g, h);

  const oc_cq15x8_t s0 = oc_cq15x8_unpacklo_i64(t0, t2);
  const oc_cq15x8_t s1 = oc_cq15x8_unpackhi_i64(t0, t2);
  const oc_cq15x8_t s2 = oc_cq15x8_unpacklo_i64(t1, t3);
  const oc_cq15x8_t s3 = oc_cq15x8_unpackhi_i64(t1, t3);

  const oc_cq15x8_t s4 = oc_cq15x8_unpacklo_i64(t4, t6);
  const oc_cq15x8_t s5 = oc_cq15x8_unpackhi_i64(t4, t6);
  const oc_cq15x8_t s6 = oc_cq15x8_unpacklo_i64(t5, t7);
  const oc_cq15x8_t s7 = oc_cq15x8_unpackhi_i64(t5, t7);

  *r0 = oc_cq15x8_select_halves(s0, s4, 0x20);
  *r1 = oc_cq15x8_select_halves(s1, s5, 0x20);
  *r2 = oc_cq15x8_select_halves(s2, s6, 0x20);
  *r3 = oc_cq15x8_select_halves(s3, s7, 0x20);

  *r4 = oc_cq15x8_select_halves(s0, s4, 0x31);
  *r5 = oc_cq15x8_select_halves(s1, s5, 0x31);
  *r6 = oc_cq15x8_select_halves(s2, s6, 0x31);
  *r7 = oc_cq15x8_select_halves(s3, s7, 0x31);
}

static inline oc_cq15x8_t complex_mul8_prepack_q15_256(oc_cq15x8_t a, oc_cq15x8_t w_re_re, oc_cq15x8_t w_im_signed)
{
  const oc_cq15x8_t a_swapped = swap_complex_pairs_i16_256(a);

  const oc_cq15x8_t prod_re = oc_cq15x8_mulhrs_i16(a, w_re_re);
  const oc_cq15x8_t prod_im = oc_cq15x8_mulhrs_i16(a_swapped, w_im_signed);

  return oc_cq15x8_adds_i16(prod_re, prod_im);
}

static inline void dft8x8lts_q15_256_dir(const oc_cq15x8_t x0,
                                         const oc_cq15x8_t x1,
                                         const oc_cq15x8_t x2,
                                         const oc_cq15x8_t x3,
                                         const oc_cq15x8_t x4,
                                         const oc_cq15x8_t x5,
                                         const oc_cq15x8_t x6,
                                         const oc_cq15x8_t x7,
                                         oc_cq15x8_t *Y0,
                                         oc_cq15x8_t *Y1,
                                         oc_cq15x8_t *Y2,
                                         oc_cq15x8_t *Y3,
                                         oc_cq15x8_t *Y4,
                                         oc_cq15x8_t *Y5,
                                         oc_cq15x8_t *Y6,
                                         oc_cq15x8_t *Y7,
                                         dft_dir_t dir)
{
  const oc_cq15x8_t c = oc_cq15x8_set1_i16(Q15_INV_SQRT2);

  const oc_cq15x8_t s04 = oc_cq15x8_adds_i16(x0, x4);
  const oc_cq15x8_t d04 = oc_cq15x8_subs_i16(x0, x4);

  const oc_cq15x8_t s15 = oc_cq15x8_adds_i16(x1, x5);
  const oc_cq15x8_t d15 = oc_cq15x8_subs_i16(x1, x5);

  const oc_cq15x8_t s26 = oc_cq15x8_adds_i16(x2, x6);
  const oc_cq15x8_t d26 = oc_cq15x8_subs_i16(x2, x6);

  const oc_cq15x8_t s37 = oc_cq15x8_adds_i16(x3, x7);
  const oc_cq15x8_t d37 = oc_cq15x8_subs_i16(x3, x7);

  const oc_cq15x8_t s02 = oc_cq15x8_adds_i16(s04, s26);
  const oc_cq15x8_t d02 = oc_cq15x8_subs_i16(s04, s26);

  const oc_cq15x8_t s13 = oc_cq15x8_adds_i16(s15, s37);
  const oc_cq15x8_t d13 = oc_cq15x8_subs_i16(s15, s37);

  *Y0 = oc_cq15x8_adds_i16(s02, s13);
  *Y4 = oc_cq15x8_subs_i16(s02, s13);

  *Y2 = oc_cq15x8_adds_i16(d02, mul_minus_j_dir_i16_256(d13, dir));
  *Y6 = oc_cq15x8_adds_i16(d02, mul_plus_j_dir_i16_256(d13, dir));

  const oc_cq15x8_t p = oc_cq15x8_adds_i16(d15, d37);
  const oc_cq15x8_t q = oc_cq15x8_subs_i16(d15, d37);

  const oc_cq15x8_t d26_mj = mul_minus_j_dir_i16_256(d26, dir);
  const oc_cq15x8_t d26_pj = mul_plus_j_dir_i16_256(d26, dir);

  const oc_cq15x8_t base_mj = oc_cq15x8_adds_i16(d04, d26_mj);
  const oc_cq15x8_t base_pj = oc_cq15x8_adds_i16(d04, d26_pj);

  const oc_cq15x8_t t1_arg = oc_cq15x8_adds_i16(q, mul_minus_j_dir_i16_256(p, dir));
  const oc_cq15x8_t t3_arg = oc_cq15x8_adds_i16(q, mul_plus_j_dir_i16_256(p, dir));

  const oc_cq15x8_t t1 = oc_cq15x8_mulhrs_i16(c, t1_arg);
  const oc_cq15x8_t t3 = oc_cq15x8_mulhrs_i16(c, t3_arg);

  *Y1 = oc_cq15x8_adds_i16(base_mj, t1);
  *Y5 = oc_cq15x8_subs_i16(base_mj, t1);

  *Y7 = oc_cq15x8_adds_i16(base_pj, t3);
  *Y3 = oc_cq15x8_subs_i16(base_pj, t3);
}

/* Store the lane-parallel DFT64 result directly in the parent output layout. */
static inline void dft64x8_batch_q15_256_store(const oc_cq15x8_t x[64],
                                               c16_t *restrict dst,
                                               int output_radix,
                                               int output_offset,
                                               dft_dir_t dir)
{
  const int dir_slot = (dir == DFT_DIR_FORWARD) ? 0 : 1;
  oc_cq15x8_t T[8][8] __attribute__((aligned(64)));

  for (int q = 0; q < 8; q++) {
    oc_cq15x8_t H[8];
    dft8x8lts_q15_256_dir(x[q],
                          x[q + 8],
                          x[q + 2 * 8],
                          x[q + 3 * 8],
                          x[q + 4 * 8],
                          x[q + 5 * 8],
                          x[q + 6 * 8],
                          x[q + 7 * 8],
                          &H[0],
                          &H[1],
                          &H[2],
                          &H[3],
                          &H[4],
                          &H[5],
                          &H[6],
                          &H[7],
                          dir);

    T[0][q] = oc_cq15x8_srai_i16(H[0], 3);
    for (int r = 1; r < 8; r++)
      T[r][q] =
          complex_mul8_prepack_q15_256(H[r], g_dft64_batch_re_re_q15[dir_slot][r][q], g_dft64_batch_im_signed_q15[dir_slot][r][q]);
  }

  for (int r = 0; r < 8; r++) {
    oc_cq15x8_t K[8];
    dft8x8lts_q15_256_dir(T[r][0],
                          T[r][1],
                          T[r][2],
                          T[r][3],
                          T[r][4],
                          T[r][5],
                          T[r][6],
                          T[r][7],
                          &K[0],
                          &K[1],
                          &K[2],
                          &K[3],
                          &K[4],
                          &K[5],
                          &K[6],
                          &K[7],
                          dir);

    for (int q = 0; q < 8; q++) {
      const int k = 8 * q + r;
      oc_cq15x8_storeu((oc_cq15x8_t *)(dst + output_radix * k + output_offset), K[q]);
    }
  }
}

static inline void dft64ltslts(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  const oc_cq15x8_t x0 = oc_cq15x8_loadu((const oc_cq15x8_t *)(src + 0));
  const oc_cq15x8_t x1 = oc_cq15x8_loadu((const oc_cq15x8_t *)(src + 8));
  const oc_cq15x8_t x2 = oc_cq15x8_loadu((const oc_cq15x8_t *)(src + 16));
  const oc_cq15x8_t x3 = oc_cq15x8_loadu((const oc_cq15x8_t *)(src + 24));
  const oc_cq15x8_t x4 = oc_cq15x8_loadu((const oc_cq15x8_t *)(src + 32));
  const oc_cq15x8_t x5 = oc_cq15x8_loadu((const oc_cq15x8_t *)(src + 40));
  const oc_cq15x8_t x6 = oc_cq15x8_loadu((const oc_cq15x8_t *)(src + 48));
  const oc_cq15x8_t x7 = oc_cq15x8_loadu((const oc_cq15x8_t *)(src + 56));

  oc_cq15x8_t H0, H1, H2, H3;
  oc_cq15x8_t H4, H5, H6, H7;

  dft8x8lts_q15_256_dir(x0, x1, x2, x3, x4, x5, x6, x7, &H0, &H1, &H2, &H3, &H4, &H5, &H6, &H7, dir);
  const oc_cq15x8_t *C64_RE = (dir == DFT_DIR_FORWARD) ? g_dft64f_tw.C64_RE_RE_q15_256 : g_dft64f_tw.C64_RE_RE_q15_256_inverse;

  const oc_cq15x8_t *C64_IM =
      (dir == DFT_DIR_FORWARD) ? g_dft64f_tw.C64_IM_SIGNED_q15_256 : g_dft64f_tw.C64_IM_SIGNED_q15_256_inverse;
  H0 = oc_cq15x8_srai_i16(H0, 3);

  H1 = complex_mul8_prepack_q15_256(H1, C64_RE[1], C64_IM[1]);
  H2 = complex_mul8_prepack_q15_256(H2, C64_RE[2], C64_IM[2]);
  H3 = complex_mul8_prepack_q15_256(H3, C64_RE[3], C64_IM[3]);
  H4 = complex_mul8_prepack_q15_256(H4, C64_RE[4], C64_IM[4]);
  H5 = complex_mul8_prepack_q15_256(H5, C64_RE[5], C64_IM[5]);
  H6 = complex_mul8_prepack_q15_256(H6, C64_RE[6], C64_IM[6]);
  H7 = complex_mul8_prepack_q15_256(H7, C64_RE[7], C64_IM[7]);

  /*
   * Transpose complex 8x8.
   */
  transpose8_complex_i16_256(&H0, &H1, &H2, &H3, &H4, &H5, &H6, &H7);

  oc_cq15x8_t Y0, Y1, Y2, Y3;
  oc_cq15x8_t Y4, Y5, Y6, Y7;

  /*
   * Second stage.
   */
  dft8x8lts_q15_256_dir(H0, H1, H2, H3, H4, H5, H6, H7, &Y0, &Y1, &Y2, &Y3, &Y4, &Y5, &Y6, &Y7, dir);
  oc_cq15x8_storeu((oc_cq15x8_t *)(dst + 0), Y0);
  oc_cq15x8_storeu((oc_cq15x8_t *)(dst + 8), Y1);
  oc_cq15x8_storeu((oc_cq15x8_t *)(dst + 16), Y2);
  oc_cq15x8_storeu((oc_cq15x8_t *)(dst + 24), Y3);
  oc_cq15x8_storeu((oc_cq15x8_t *)(dst + 32), Y4);
  oc_cq15x8_storeu((oc_cq15x8_t *)(dst + 40), Y5);
  oc_cq15x8_storeu((oc_cq15x8_t *)(dst + 48), Y6);
  oc_cq15x8_storeu((oc_cq15x8_t *)(dst + 56), Y7);
}

static inline oc_cq15x8_t scale_q15_inv_sqrt2_256(oc_cq15x8_t x)
{
  const oc_cq15x8_t s = oc_cq15x8_set1_i16(Q15_INV_SQRT2);
  return oc_cq15x8_mulhrs_i16(x, s);
}

static inline void dft128_stage0_blk_q15_256_dir(const c16_t *src, c16_t *a, c16_t *b, int blk, dft_dir_t dir)
{
  const oc_cq15x8_t x0 = oc_cq15x8_loadu((const oc_cq15x8_t *)(src + 8 * blk));

  const oc_cq15x8_t x1 = oc_cq15x8_loadu((const oc_cq15x8_t *)(src + 64 + 8 * blk));

  oc_cq15x8_t sum = oc_cq15x8_adds_i16(x0, x1);
  oc_cq15x8_t diff = oc_cq15x8_subs_i16(x0, x1);

  sum = scale_q15_inv_sqrt2_256(sum);

  const oc_cq15x8_t *W128_RE = (dir == DFT_DIR_FORWARD) ? g_dft64f_tw.W128_RE_RE_q15_256 : g_dft64f_tw.W128_RE_RE_q15_256_inverse;

  const oc_cq15x8_t *W128_IM =
      (dir == DFT_DIR_FORWARD) ? g_dft64f_tw.W128_IM_SIGNED_q15_256 : g_dft64f_tw.W128_IM_SIGNED_q15_256_inverse;

  diff = complex_mul8_prepack_q15_256(diff, W128_RE[blk], W128_IM[blk]);

  oc_cq15x8_store((oc_cq15x8_t *)(a + 8 * blk), sum);

  oc_cq15x8_store((oc_cq15x8_t *)(b + 8 * blk), diff);
}

static inline void interleave64_complex_q15_256(const c16_t *A, const c16_t *B, c16_t *dst)
{
  for (int blk = 0; blk < 8; blk++) {
    const oc_cq15x8_t va = oc_cq15x8_load((const oc_cq15x8_t *)(A + 8 * blk));

    const oc_cq15x8_t vb = oc_cq15x8_load((const oc_cq15x8_t *)(B + 8 * blk));

    /*
     * va = [A0 A1 A2 A3 | A4 A5 A6 A7]
     * vb = [B0 B1 B2 B3 | B4 B5 B6 B7]
     *
     * Each A0/B0 is one c16_t = 32 bits.
     */
    const oc_cq15x8_t lo = oc_cq15x8_unpacklo_i32(va, vb);
    const oc_cq15x8_t hi = oc_cq15x8_unpackhi_i32(va, vb);

    /*
     * out0 = [A0 B0 A1 B1 A2 B2 A3 B3]
     * out1 = [A4 B4 A5 B5 A6 B6 A7 B7]
     */
    const oc_cq15x8_t out0 = oc_cq15x8_select_halves(lo, hi, 0x20);
    const oc_cq15x8_t out1 = oc_cq15x8_select_halves(lo, hi, 0x31);

    oc_cq15x8_storeu((oc_cq15x8_t *)(dst + 16 * blk), out0);

    oc_cq15x8_storeu((oc_cq15x8_t *)(dst + 16 * blk + 8), out1);
  }
}

static inline void dft128lts_staged_dir(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  c16_t a[64] __attribute__((aligned(32)));
  c16_t b[64] __attribute__((aligned(32)));

  c16_t A[64] __attribute__((aligned(32)));
  c16_t B[64] __attribute__((aligned(32)));

  for (int blk = 0; blk < 8; blk++) {
    dft128_stage0_blk_q15_256_dir(src, a, b, blk, dir);
  }

  dft64ltslts(a, A, dir);
  dft64ltslts(b, B, dir);

  interleave64_complex_q15_256(A, B, dst);
}

/*
 * DFT128 = radix-2 x two DFT64 children with direct final interleaving.
 * The two DFT64 kernels return their final native NEON batches in registers; each
 * pair is interleaved and stored directly as sixteen natural DFT128 bins.
 */

static inline void dft128lts_dir(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  dft128lts_staged_dir(src, dst, dir);
}

/* Unitary NEON DFT4 on four independent lanes. Butterfly arithmetic is widened,
 * then the 1/2 normalization is applied once. */
static inline void neon_dft4x4_unitary_q15(const neon_m128i x0,
                                           const neon_m128i x1,
                                           const neon_m128i x2,
                                           const neon_m128i x3,
                                           neon_m128i *Y0,
                                           neon_m128i *Y1,
                                           neon_m128i *Y2,
                                           neon_m128i *Y3)
{
  const int16x8_t a0 = neon128_as_i16(x0);
  const int16x8_t a1 = neon128_as_i16(x1);
  const int16x8_t a2 = neon128_as_i16(x2);
  const int16x8_t a3 = neon128_as_i16(x3);

  const int16x8_t s02 = vhaddq_s16(a0, a2);
  const int16x8_t d02 = vhsubq_s16(a0, a2);
  const int16x8_t s13 = vhaddq_s16(a1, a3);
  const int16x8_t d13 = vhsubq_s16(a1, a3);

  const neon_m128i d13_raw = neon128_from_i16(d13);
  const int16x8_t d13_mj = neon128_as_i16(mul_minuslts_q15_128(d13_raw));
  const int16x8_t d13_pj = neon128_as_i16(mullts_q15_128(d13_raw));

  *Y0 = neon128_from_i16(vqaddq_s16(s02, s13));
  *Y2 = neon128_from_i16(vqsubq_s16(s02, s13));
  *Y1 = neon128_from_i16(vqaddq_s16(d02, d13_mj));
  *Y3 = neon128_from_i16(vqaddq_s16(d02, d13_pj));
}

/*
 * Non-unitary radix-4 butterfly used by DFT64 after its 1/2 scale has already
 * been applied to A0 and fused into the A1/A2/A3 twiddles.  This is therefore
 * only the original saturating add/sub butterfly: no shift, no widening and
 * no extra scale arithmetic.
 */
static inline void neon_dft4x4_butterfly_q15(const neon_m128i x0,
                                             const neon_m128i x1,
                                             const neon_m128i x2,
                                             const neon_m128i x3,
                                             neon_m128i *Y0,
                                             neon_m128i *Y1,
                                             neon_m128i *Y2,
                                             neon_m128i *Y3)
{
  const int16x8_t a0 = neon128_as_i16(x0);
  const int16x8_t a1 = neon128_as_i16(x1);
  const int16x8_t a2 = neon128_as_i16(x2);
  const int16x8_t a3 = neon128_as_i16(x3);

  const int16x8_t s02 = vqaddq_s16(a0, a2);
  const int16x8_t d02 = vqsubq_s16(a0, a2);
  const int16x8_t s13 = vqaddq_s16(a1, a3);
  const int16x8_t d13 = vqsubq_s16(a1, a3);

  const neon_m128i d13_raw = neon128_from_i16(d13);
  const int16x8_t d13_mj = neon128_as_i16(mul_minuslts_q15_128(d13_raw));
  const int16x8_t d13_pj = neon128_as_i16(mullts_q15_128(d13_raw));

  *Y0 = neon128_from_i16(vqaddq_s16(s02, s13));
  *Y2 = neon128_from_i16(vqsubq_s16(s02, s13));
  *Y1 = neon128_from_i16(vqaddq_s16(d02, d13_mj));
  *Y3 = neon128_from_i16(vqaddq_s16(d02, d13_pj));
}

static inline int16x4_t dft64_dc_from_child_k0(neon_m128i A0, neon_m128i A1, neon_m128i A2, neon_m128i A3)
{
  int32x4_t dc32 = vmovl_s16(vget_low_s16(neon128_as_i16(A0)));
  dc32 = vaddq_s32(dc32, vmovl_s16(vget_low_s16(neon128_as_i16(A1))));
  dc32 = vaddq_s32(dc32, vmovl_s16(vget_low_s16(neon128_as_i16(A2))));
  dc32 = vaddq_s32(dc32, vmovl_s16(vget_low_s16(neon128_as_i16(A3))));

  /* Signed rounded division by two: positive +1, negative +0. */
  const int32x4_t sign = vshrq_n_s32(dc32, 31);
  dc32 = vaddq_s32(dc32, vaddq_s32(vdupq_n_s32(1), sign));
  dc32 = vshrq_n_s32(dc32, 1);

  return vqmovn_s32(dc32);
}

static inline void transpose4_complex_epi16(neon_m128i *Y0, neon_m128i *Y1, neon_m128i *Y2, neon_m128i *Y3)
{
  neon_m128i a = *Y0;
  neon_m128i b = *Y1;
  neon_m128i c = *Y2;
  neon_m128i d = *Y3;

  neon_m128i ab_lo = neon128_unpacklo_i32(a, b); // [a0 b0 a1 b1]
  neon_m128i ab_hi = neon128_unpackhi_i32(a, b); // [a2 b2 a3 b3]

  neon_m128i cd_lo = neon128_unpacklo_i32(c, d); // [c0 d0 c1 d1]
  neon_m128i cd_hi = neon128_unpackhi_i32(c, d); // [c2 d2 c3 d3]

  *Y0 = neon128_unpacklo_i64(ab_lo, cd_lo); // [a0 b0 c0 d0]
  *Y1 = neon128_unpackhi_i64(ab_lo, cd_lo); // [a1 b1 c1 d1]
  *Y2 = neon128_unpacklo_i64(ab_hi, cd_hi); // [a2 b2 c2 d2]
  *Y3 = neon128_unpackhi_i64(ab_hi, cd_hi); // [a3 b3 c3 d3]
}

typedef void (*dft_child_q15_fn_t)(const c16_t *src, c16_t *dst, dft_dir_t dir);

typedef struct {
  int N;
  int M;
  int blocks;
  int initialized;

  /*
   * W_N^(r*n) / sqrt(8), packed as eight complex Q15 twiddles.
   * Layout is [r * blocks + block].
   */
  oc_cq15x8_t *W_RE_RE;
  oc_cq15x8_t *W_IM_SIGNED;
} dft_radix8_twiddle_q15_t;

/* One forward and one inverse table for N = 512 through 65536. */
static dft_radix8_twiddle_q15_t g_dft_radix8_twiddles[8][2];

static inline int dft_radix8_size_slot(int N)
{
  switch (N) {
    case 512:
      return 0;
    case 1024:
      return 1;
    case 2048:
      return 2;
    case 4096:
      return 3;
    case 8192:
      return 4;
    case 16384:
      return 5;
    case 32768:
      return 6;
    case 65536:
      return 7;
    default:
      return -1;
  }
}

/*
 * Alternative DFT1024 outer stage:
 *
 *   DFT1024 = 16 x DFT64.
 *
 * The radix-16 butterfly is implemented as radix-2 followed by two
 * radix-8 butterflies.  The radix-2 branches are scaled by 1/sqrt(2),
 * and the outer twiddles by 1/sqrt(8), giving the required unitary
 * radix-16 scale 1/sqrt(16) = 1/4.
 */
typedef struct {
  int initialized;
  int blocks;

  /* W_1024^(r*n) / sqrt(8), r=0..15, eight n values per block. */
  oc_cq15x8_t *W_RE_RE;
  oc_cq15x8_t *W_IM_SIGNED;

  /* Broadcast W_16^q / sqrt(2) for the odd radix-2 branch. */
  oc_cq15x8_t W16_RE_RE[8];
  oc_cq15x8_t W16_IM_SIGNED[8];
} dft1024_radix16_twiddle_q15_t;

static dft1024_radix16_twiddle_q15_t g_dft1024_radix16_twiddles[2];

/*
 * Alternative DFT256 outer stage:
 *
 *   DFT256 = 16 x DFT16.
 *
 * It uses the same radix-2 x radix-8 outer butterfly as the 1024-point
 * variant.  The radix-2 contributes 1/sqrt(2), while the outer twiddle
 * table contributes 1/sqrt(8), so the complete radix-16 stage is scaled
 * by 1/sqrt(16) = 1/4.
 */
typedef struct {
  int initialized;
  int blocks;

  /* W_256^(r*n) / sqrt(8), r=0..15, eight n values per block. */
  oc_cq15x8_t *W_RE_RE;
  oc_cq15x8_t *W_IM_SIGNED;

  /* Broadcast W_16^q / sqrt(2) for the odd radix-2 branch. */
  oc_cq15x8_t W16_RE_RE[8];
  oc_cq15x8_t W16_IM_SIGNED[8];

  /* W_16^q / 4 for a complete unitary DFT16 child (1/sqrt(16)). */
  oc_cq15x8_t W16_UNITARY_RE_RE[8];
  oc_cq15x8_t W16_UNITARY_IM_SIGNED[8];
} dft256_radix16_twiddle_q15_t;

static dft256_radix16_twiddle_q15_t g_dft256_radix16_twiddles[2];

/*
 * Alternative DFT2048 outer stage:
 *
 *   DFT2048 = 32 x DFT64.
 *
 * The radix-32 butterfly is implemented as radix-2 followed by two
 * unitary radix-16 butterflies. The two radix-2 levels contribute
 * 1/sqrt(2) each, and the final radix-8 stage inside each radix-16
 * contributes 1/sqrt(8), giving 1/sqrt(32).
 */
typedef struct {
  int initialized;
  int blocks;

  /* W_2048^(r*n) / sqrt(8), r=0..31, eight n values per block. */
  oc_cq15x8_t *W_RE_RE;
  oc_cq15x8_t *W_IM_SIGNED;

  /* Broadcast W_16^q / sqrt(2) for the inner radix-16 butterflies. */
  oc_cq15x8_t W16_RE_RE[8];
  oc_cq15x8_t W16_IM_SIGNED[8];

  /* Broadcast W_32^q / sqrt(2) for the outer radix-2 odd branch. */
  oc_cq15x8_t W32_RE_RE[16];
  oc_cq15x8_t W32_IM_SIGNED[16];
} dft2048_radix32_twiddle_q15_t;

static void init_dft256_radix16_q15_twiddles(void)
{
  const int N = 256;
  const int M = 16;
  const int blocks = M / 8;
  const float outer_scale = 1.0f / sqrtf(8.0f);

  for (int dir_slot = 0; dir_slot < 2; dir_slot++) {
    dft256_radix16_twiddle_q15_t *tw = &g_dft256_radix16_twiddles[dir_slot];

    if (tw->initialized)
      continue;

    tw->blocks = blocks;
    tw->W_RE_RE = aligned_malloc64((size_t)16 * (size_t)blocks * sizeof(oc_cq15x8_t));
    tw->W_IM_SIGNED = aligned_malloc64((size_t)16 * (size_t)blocks * sizeof(oc_cq15x8_t));

    AssertFatal(tw->W_RE_RE && tw->W_IM_SIGNED, "DFT256 radix16 twiddle allocation failed\n");

    const dft_dir_t dir = (dir_slot == 0) ? DFT_DIR_FORWARD : DFT_DIR_INVERSE;

    for (int q = 0; q < 8; q++) {
      const float theta = (float)dir * 2.0f * (float)M_PI * (float)q / 16.0f;
      const int16_t wr = q15_from_float(cosf(theta) * (1.0f / sqrtf(2.0f)));
      const int16_t wi = q15_from_float(sinf(theta) * (1.0f / sqrtf(2.0f)));
      const int16_t wr_unitary = q15_from_float(cosf(theta) * 0.25f);
      const int16_t wi_unitary = q15_from_float(sinf(theta) * 0.25f);

      int16_t re_re[16] __attribute__((aligned(32)));
      int16_t im_signed[16] __attribute__((aligned(32)));
      int16_t re_re_unitary[16] __attribute__((aligned(32)));
      int16_t im_signed_unitary[16] __attribute__((aligned(32)));

      for (int lane = 0; lane < 8; lane++) {
        re_re[2 * lane + 0] = wr;
        re_re[2 * lane + 1] = wr;
        im_signed[2 * lane + 0] = (int16_t)-wi;
        im_signed[2 * lane + 1] = wi;
        re_re_unitary[2 * lane + 0] = wr_unitary;
        re_re_unitary[2 * lane + 1] = wr_unitary;
        im_signed_unitary[2 * lane + 0] = (int16_t)-wi_unitary;
        im_signed_unitary[2 * lane + 1] = wi_unitary;
      }

      tw->W16_RE_RE[q] = oc_cq15x8_load((const oc_cq15x8_t *)re_re);
      tw->W16_IM_SIGNED[q] = oc_cq15x8_load((const oc_cq15x8_t *)im_signed);
      tw->W16_UNITARY_RE_RE[q] = oc_cq15x8_load((const oc_cq15x8_t *)re_re_unitary);
      tw->W16_UNITARY_IM_SIGNED[q] = oc_cq15x8_load((const oc_cq15x8_t *)im_signed_unitary);
    }

    for (int r = 0; r < 16; r++) {
      for (int b = 0; b < blocks; b++) {
        int16_t re_re[16] __attribute__((aligned(32)));
        int16_t im_signed[16] __attribute__((aligned(32)));

        for (int lane = 0; lane < 8; lane++) {
          const int n = 8 * b + lane;
          const float theta = (float)dir * 2.0f * (float)M_PI * (float)(r * n) / (float)N;
          const int16_t wr = q15_from_float(cosf(theta) * outer_scale);
          const int16_t wi = q15_from_float(sinf(theta) * outer_scale);

          re_re[2 * lane + 0] = wr;
          re_re[2 * lane + 1] = wr;
          im_signed[2 * lane + 0] = (int16_t)-wi;
          im_signed[2 * lane + 1] = wi;
        }

        const int idx = r * blocks + b;
        tw->W_RE_RE[idx] = oc_cq15x8_load((const oc_cq15x8_t *)re_re);
        tw->W_IM_SIGNED[idx] = oc_cq15x8_load((const oc_cq15x8_t *)im_signed);
      }
    }

    tw->initialized = 1;
  }
}

static void init_dft1024_radix16_q15_twiddles(void)
{
  const int N = 1024;
  const int M = 64;
  const int blocks = M / 8;
  const float outer_scale = 1.0f / sqrtf(8.0f);

  for (int dir_slot = 0; dir_slot < 2; dir_slot++) {
    dft1024_radix16_twiddle_q15_t *tw = &g_dft1024_radix16_twiddles[dir_slot];

    if (tw->initialized)
      continue;

    tw->blocks = blocks;
    tw->W_RE_RE = aligned_malloc64((size_t)16 * (size_t)blocks * sizeof(oc_cq15x8_t));
    tw->W_IM_SIGNED = aligned_malloc64((size_t)16 * (size_t)blocks * sizeof(oc_cq15x8_t));

    AssertFatal(tw->W_RE_RE && tw->W_IM_SIGNED, "DFT1024 radix16 twiddle allocation failed\n");

    const dft_dir_t dir = (dir_slot == 0) ? DFT_DIR_FORWARD : DFT_DIR_INVERSE;

    for (int q = 0; q < 8; q++) {
      const float theta = (float)dir * 2.0f * (float)M_PI * (float)q / 16.0f;
      const int16_t wr = q15_from_float(cosf(theta) * (1.0f / sqrtf(2.0f)));
      const int16_t wi = q15_from_float(sinf(theta) * (1.0f / sqrtf(2.0f)));

      int16_t re_re[16] __attribute__((aligned(32)));
      int16_t im_signed[16] __attribute__((aligned(32)));

      for (int lane = 0; lane < 8; lane++) {
        re_re[2 * lane + 0] = wr;
        re_re[2 * lane + 1] = wr;
        im_signed[2 * lane + 0] = (int16_t)-wi;
        im_signed[2 * lane + 1] = wi;
      }

      tw->W16_RE_RE[q] = oc_cq15x8_load((const oc_cq15x8_t *)re_re);
      tw->W16_IM_SIGNED[q] = oc_cq15x8_load((const oc_cq15x8_t *)im_signed);
    }

    for (int r = 0; r < 16; r++) {
      for (int b = 0; b < blocks; b++) {
        int16_t re_re[16] __attribute__((aligned(32)));
        int16_t im_signed[16] __attribute__((aligned(32)));

        for (int lane = 0; lane < 8; lane++) {
          const int n = 8 * b + lane;
          const float theta = (float)dir * 2.0f * (float)M_PI * (float)(r * n) / (float)N;
          const int16_t wr = q15_from_float(cosf(theta) * outer_scale);
          const int16_t wi = q15_from_float(sinf(theta) * outer_scale);

          re_re[2 * lane + 0] = wr;
          re_re[2 * lane + 1] = wr;
          im_signed[2 * lane + 0] = (int16_t)-wi;
          im_signed[2 * lane + 1] = wi;
        }

        const int idx = r * blocks + b;
        tw->W_RE_RE[idx] = oc_cq15x8_load((const oc_cq15x8_t *)re_re);
        tw->W_IM_SIGNED[idx] = oc_cq15x8_load((const oc_cq15x8_t *)im_signed);
      }
    }

    tw->initialized = 1;
  }
}

/*
 * Radix-4 outer stage used by DFT2048 = 4 x DFT512.
 * One forward and one inverse table are kept for N=2048.
 */
typedef struct {
  int N;
  int M;
  int blocks;
  int initialized;

  /* W_N^(r*n) / sqrt(4), packed as eight complex Q15 twiddles. */
  oc_cq15x8_t *W_RE_RE;
  oc_cq15x8_t *W_IM_SIGNED;
} dft_radix4_twiddle_q15_t;

static void init_dft_radix8_q15_twiddles(int N)
{
  const int size_slot = dft_radix8_size_slot(N);
  AssertFatal(size_slot >= 0, "Unsupported radix8 twiddle size N=%d\n", N);

  const int M = N / 8;
  const int blocks = M / 8;
  const float scale = 1.0f / sqrtf(8.0f);

  for (int dir_slot = 0; dir_slot < 2; dir_slot++) {
    dft_radix8_twiddle_q15_t *tw = &g_dft_radix8_twiddles[size_slot][dir_slot];

    if (tw->initialized)
      continue;

    tw->N = N;
    tw->M = M;
    tw->blocks = blocks;
    tw->W_RE_RE = aligned_malloc64((size_t)8 * (size_t)blocks * sizeof(oc_cq15x8_t));
    tw->W_IM_SIGNED = aligned_malloc64((size_t)8 * (size_t)blocks * sizeof(oc_cq15x8_t));

    AssertFatal(tw->W_RE_RE && tw->W_IM_SIGNED, "Radix8 twiddle allocation failed for N=%d\n", N);

    const dft_dir_t dir = (dir_slot == 0) ? DFT_DIR_FORWARD : DFT_DIR_INVERSE;

    for (int r = 0; r < 8; r++) {
      for (int b = 0; b < blocks; b++) {
        int16_t re_re[16] __attribute__((aligned(32)));
        int16_t im_signed[16] __attribute__((aligned(32)));

        for (int lane = 0; lane < 8; lane++) {
          const int n = 8 * b + lane;
          const float theta = (float)dir * 2.0f * (float)M_PI * (float)(r * n) / (float)N;
          const int16_t wr = q15_from_float(cosf(theta) * scale);
          const int16_t wi = q15_from_float(sinf(theta) * scale);

          re_re[2 * lane + 0] = wr;
          re_re[2 * lane + 1] = wr;

          im_signed[2 * lane + 0] = (int16_t)-wi;
          im_signed[2 * lane + 1] = wi;
        }

        const int idx = r * blocks + b;
        tw->W_RE_RE[idx] = oc_cq15x8_load((const oc_cq15x8_t *)re_re);
        tw->W_IM_SIGNED[idx] = oc_cq15x8_load((const oc_cq15x8_t *)im_signed);
      }
    }

    tw->initialized = 1;
  }
}

static inline void radix8_blocked_to_natural_q15(const c16_t *blocked, c16_t *dst, int M)
{
  /*
   * blocked[r * M + q] = X[8 * q + r]
   *
   * Convert the internal child-major layout to the public natural order.
   */
  for (int q = 0; q < M; q += 8) {
    oc_cq15x8_t y0 = oc_cq15x8_load((const oc_cq15x8_t *)(blocked + q));
    oc_cq15x8_t y1 = oc_cq15x8_load((const oc_cq15x8_t *)(blocked + M + q));
    oc_cq15x8_t y2 = oc_cq15x8_load((const oc_cq15x8_t *)(blocked + 2 * M + q));
    oc_cq15x8_t y3 = oc_cq15x8_load((const oc_cq15x8_t *)(blocked + 3 * M + q));
    oc_cq15x8_t y4 = oc_cq15x8_load((const oc_cq15x8_t *)(blocked + 4 * M + q));
    oc_cq15x8_t y5 = oc_cq15x8_load((const oc_cq15x8_t *)(blocked + 5 * M + q));
    oc_cq15x8_t y6 = oc_cq15x8_load((const oc_cq15x8_t *)(blocked + 6 * M + q));
    oc_cq15x8_t y7 = oc_cq15x8_load((const oc_cq15x8_t *)(blocked + 7 * M + q));

    transpose8_complex_i16_256(&y0, &y1, &y2, &y3, &y4, &y5, &y6, &y7);

    oc_cq15x8_storeu((oc_cq15x8_t *)(dst + 8 * (q + 0)), y0);
    oc_cq15x8_storeu((oc_cq15x8_t *)(dst + 8 * (q + 1)), y1);
    oc_cq15x8_storeu((oc_cq15x8_t *)(dst + 8 * (q + 2)), y2);
    oc_cq15x8_storeu((oc_cq15x8_t *)(dst + 8 * (q + 3)), y3);
    oc_cq15x8_storeu((oc_cq15x8_t *)(dst + 8 * (q + 4)), y4);
    oc_cq15x8_storeu((oc_cq15x8_t *)(dst + 8 * (q + 5)), y5);
    oc_cq15x8_storeu((oc_cq15x8_t *)(dst + 8 * (q + 6)), y6);
    oc_cq15x8_storeu((oc_cq15x8_t *)(dst + 8 * (q + 7)), y7);
  }
}

static void dft_radix8_q15_stage(const c16_t *src, c16_t *stage, int N, dft_dir_t dir)
{
  init_dft_radix8_q15_twiddles(N);

  const int size_slot = dft_radix8_size_slot(N);
  const int dir_slot = (dir == DFT_DIR_FORWARD) ? 0 : 1;
  const dft_radix8_twiddle_q15_t *tw = &g_dft_radix8_twiddles[size_slot][dir_slot];
  const int M = tw->M;
  const int blocks = tw->blocks;

  for (int b = 0; b < blocks; b++) {
    const int n = 8 * b;

    const oc_cq15x8_t x0 = oc_cq15x8_loadu((const oc_cq15x8_t *)(src + n));
    const oc_cq15x8_t x1 = oc_cq15x8_loadu((const oc_cq15x8_t *)(src + M + n));
    const oc_cq15x8_t x2 = oc_cq15x8_loadu((const oc_cq15x8_t *)(src + 2 * M + n));
    const oc_cq15x8_t x3 = oc_cq15x8_loadu((const oc_cq15x8_t *)(src + 3 * M + n));
    const oc_cq15x8_t x4 = oc_cq15x8_loadu((const oc_cq15x8_t *)(src + 4 * M + n));
    const oc_cq15x8_t x5 = oc_cq15x8_loadu((const oc_cq15x8_t *)(src + 5 * M + n));
    const oc_cq15x8_t x6 = oc_cq15x8_loadu((const oc_cq15x8_t *)(src + 6 * M + n));
    const oc_cq15x8_t x7 = oc_cq15x8_loadu((const oc_cq15x8_t *)(src + 7 * M + n));

    oc_cq15x8_t H[8];
    dft8x8lts_q15_256_dir(x0, x1, x2, x3, x4, x5, x6, x7, &H[0], &H[1], &H[2], &H[3], &H[4], &H[5], &H[6], &H[7], dir);

    for (int r = 0; r < 8; r++) {
      const int idx = r * blocks + b;
      H[r] = complex_mul8_prepack_q15_256(H[r], tw->W_RE_RE[idx], tw->W_IM_SIGNED[idx]);
      oc_cq15x8_store((oc_cq15x8_t *)(stage + r * M + n), H[r]);
    }
  }
}

static void dft_radix8_q15_blocked(const c16_t *src, c16_t *dst, int N, dft_dir_t dir, dft_child_q15_fn_t child_dft)
{
  const int M = N / 8;
  c16_t stage[N] __attribute__((aligned(64)));

  dft_radix8_q15_stage(src, stage, N, dir);

  for (int r = 0; r < 8; r++)
    child_dft(stage + r * M, dst + r * M, dir);
}

static void dft_radix8_q15(const c16_t *src, c16_t *dst, int N, dft_dir_t dir, dft_child_q15_fn_t child_dft)
{
  const int M = N / 8;
  c16_t blocked[N] __attribute__((aligned(64)));

  dft_radix8_q15_blocked(src, blocked, N, dir, child_dft);
  radix8_blocked_to_natural_q15(blocked, dst, M);
}

/*
 * Direct-output DFT1024 = radix-16 x sixteen DFT64 children.
 * The outer stage remains branch-major, but eight child branches are
 * transposed into NEON lanes and the batched DFT64 final stage writes
 * directly to X[16*k+r].  There is no child[1024] array and no final
 * radix-16 interleave pass.
 */
static void dft1024_radix16_direct_q15(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  init_dft1024_radix16_q15_twiddles();

  enum { N = 1024, M = 64 };
  const int dir_slot = (dir == DFT_DIR_FORWARD) ? 0 : 1;
  const dft1024_radix16_twiddle_q15_t *tw = &g_dft1024_radix16_twiddles[dir_slot];
  const int blocks = tw->blocks;
  const oc_cq15x8_t inv_sqrt2 = oc_cq15x8_set1_i16(Q15_INV_SQRT2);
  c16_t stage[N] __attribute__((aligned(64)));

  for (int b = 0; b < blocks; b++) {
    const int n = 8 * b;
    oc_cq15x8_t A[8];
    oc_cq15x8_t B[8];
    oc_cq15x8_t E[8];
    oc_cq15x8_t O[8];

    for (int q = 0; q < 8; q++) {
      const oc_cq15x8_t lo = oc_cq15x8_loadu((const oc_cq15x8_t *)(src + q * M + n));
      const oc_cq15x8_t hi = oc_cq15x8_loadu((const oc_cq15x8_t *)(src + (q + 8) * M + n));

      A[q] = oc_cq15x8_mulhrs_i16(oc_cq15x8_adds_i16(lo, hi), inv_sqrt2);
      B[q] = complex_mul8_prepack_q15_256(oc_cq15x8_subs_i16(lo, hi), tw->W16_RE_RE[q], tw->W16_IM_SIGNED[q]);
    }

    dft8x8lts_q15_256_dir(A[0],
                          A[1],
                          A[2],
                          A[3],
                          A[4],
                          A[5],
                          A[6],
                          A[7],
                          &E[0],
                          &E[1],
                          &E[2],
                          &E[3],
                          &E[4],
                          &E[5],
                          &E[6],
                          &E[7],
                          dir);
    dft8x8lts_q15_256_dir(B[0],
                          B[1],
                          B[2],
                          B[3],
                          B[4],
                          B[5],
                          B[6],
                          B[7],
                          &O[0],
                          &O[1],
                          &O[2],
                          &O[3],
                          &O[4],
                          &O[5],
                          &O[6],
                          &O[7],
                          dir);

    for (int k = 0; k < 8; k++) {
      const int r_even = 2 * k;
      const int r_odd = r_even + 1;
      const int idx_even = r_even * blocks + b;
      const int idx_odd = r_odd * blocks + b;
      const oc_cq15x8_t even = complex_mul8_prepack_q15_256(E[k], tw->W_RE_RE[idx_even], tw->W_IM_SIGNED[idx_even]);
      const oc_cq15x8_t odd = complex_mul8_prepack_q15_256(O[k], tw->W_RE_RE[idx_odd], tw->W_IM_SIGNED[idx_odd]);

      oc_cq15x8_store((oc_cq15x8_t *)(stage + r_even * M + n), even);
      oc_cq15x8_store((oc_cq15x8_t *)(stage + r_odd * M + n), odd);
    }
  }

  for (int group = 0; group < 2; group++) {
    oc_cq15x8_t leaf_in[64] __attribute__((aligned(64)));
    const int r0 = 8 * group;

    for (int n = 0; n < M; n += 8) {
      oc_cq15x8_t z0 = oc_cq15x8_load((const oc_cq15x8_t *)(stage + (r0 + 0) * M + n));
      oc_cq15x8_t z1 = oc_cq15x8_load((const oc_cq15x8_t *)(stage + (r0 + 1) * M + n));
      oc_cq15x8_t z2 = oc_cq15x8_load((const oc_cq15x8_t *)(stage + (r0 + 2) * M + n));
      oc_cq15x8_t z3 = oc_cq15x8_load((const oc_cq15x8_t *)(stage + (r0 + 3) * M + n));
      oc_cq15x8_t z4 = oc_cq15x8_load((const oc_cq15x8_t *)(stage + (r0 + 4) * M + n));
      oc_cq15x8_t z5 = oc_cq15x8_load((const oc_cq15x8_t *)(stage + (r0 + 5) * M + n));
      oc_cq15x8_t z6 = oc_cq15x8_load((const oc_cq15x8_t *)(stage + (r0 + 6) * M + n));
      oc_cq15x8_t z7 = oc_cq15x8_load((const oc_cq15x8_t *)(stage + (r0 + 7) * M + n));

      transpose8_complex_i16_256(&z0, &z1, &z2, &z3, &z4, &z5, &z6, &z7);
      leaf_in[n + 0] = z0;
      leaf_in[n + 1] = z1;
      leaf_in[n + 2] = z2;
      leaf_in[n + 3] = z3;
      leaf_in[n + 4] = z4;
      leaf_in[n + 5] = z5;
      leaf_in[n + 6] = z6;
      leaf_in[n + 7] = z7;
    }

    dft64x8_batch_q15_256_store(leaf_in, dst, 16, r0, dir);
  }
}

static inline void dft16x8_radix2x8_q15_256_unitary_dir(const oc_cq15x8_t x[16],
                                                        oc_cq15x8_t Y[16],
                                                        const oc_cq15x8_t W16_RE_RE_DIV4[8],
                                                        const oc_cq15x8_t W16_IM_SIGNED_DIV4[8],
                                                        dft_dir_t dir)
{
  oc_cq15x8_t A[8];
  oc_cq15x8_t B[8];
  oc_cq15x8_t E[8];
  oc_cq15x8_t O[8];

  /* W=1 branches: the 1/4 unitary coefficient has no rotation to absorb it. */
  A[0] = oc_cq15x8_rshr2_i16(oc_cq15x8_adds_i16(x[0], x[8]));
  B[0] = oc_cq15x8_rshr2_i16(oc_cq15x8_subs_i16(x[0], x[8]));

  for (int q = 1; q < 8; q++) {
    A[q] = oc_cq15x8_rshr2_i16(oc_cq15x8_adds_i16(x[q], x[q + 8]));
    B[q] = complex_mul8_prepack_q15_256(oc_cq15x8_subs_i16(x[q], x[q + 8]), W16_RE_RE_DIV4[q], W16_IM_SIGNED_DIV4[q]);
  }

  dft8x8lts_q15_256_dir(A[0],
                        A[1],
                        A[2],
                        A[3],
                        A[4],
                        A[5],
                        A[6],
                        A[7],
                        &E[0],
                        &E[1],
                        &E[2],
                        &E[3],
                        &E[4],
                        &E[5],
                        &E[6],
                        &E[7],
                        dir);
  dft8x8lts_q15_256_dir(B[0],
                        B[1],
                        B[2],
                        B[3],
                        B[4],
                        B[5],
                        B[6],
                        B[7],
                        &O[0],
                        &O[1],
                        &O[2],
                        &O[3],
                        &O[4],
                        &O[5],
                        &O[6],
                        &O[7],
                        dir);

  for (int k = 0; k < 8; k++) {
    Y[2 * k + 0] = E[k];
    Y[2 * k + 1] = O[k];
  }
}

static inline void dft256_radix16_prepare_q15(const c16_t *src, oc_cq15x8_t child_in[2][16], dft_dir_t dir)
{
  const int M = 16;
  const int dir_slot = (dir == DFT_DIR_FORWARD) ? 0 : 1;
  const dft256_radix16_twiddle_q15_t *tw = &g_dft256_radix16_twiddles[dir_slot];
  const int blocks = tw->blocks;
  const oc_cq15x8_t inv_sqrt2 = oc_cq15x8_set1_i16(Q15_INV_SQRT2);
  const oc_cq15x8_t inv_sqrt8 = oc_cq15x8_set1_i16(Q15_INV_SQRT8);

  for (int b = 0; b < blocks; b++) {
    const int n = 8 * b;
    oc_cq15x8_t A[8];
    oc_cq15x8_t B[8];
    oc_cq15x8_t E[8];
    oc_cq15x8_t O[8];
    oc_cq15x8_t R[16];

    /* q = 0: W16^0 = 1. */
    {
      const oc_cq15x8_t lo = oc_cq15x8_loadu((const oc_cq15x8_t *)(src + n));
      const oc_cq15x8_t hi = oc_cq15x8_loadu((const oc_cq15x8_t *)(src + 8 * M + n));
      A[0] = oc_cq15x8_mulhrs_i16(oc_cq15x8_adds_i16(lo, hi), inv_sqrt2);
      B[0] = oc_cq15x8_mulhrs_i16(oc_cq15x8_subs_i16(lo, hi), inv_sqrt2);
    }

    for (int q = 1; q < 8; q++) {
      const oc_cq15x8_t lo = oc_cq15x8_loadu((const oc_cq15x8_t *)(src + q * M + n));
      const oc_cq15x8_t hi = oc_cq15x8_loadu((const oc_cq15x8_t *)(src + (q + 8) * M + n));

      A[q] = oc_cq15x8_mulhrs_i16(oc_cq15x8_adds_i16(lo, hi), inv_sqrt2);
      B[q] = complex_mul8_prepack_q15_256(oc_cq15x8_subs_i16(lo, hi), tw->W16_RE_RE[q], tw->W16_IM_SIGNED[q]);
    }

    dft8x8lts_q15_256_dir(A[0],
                          A[1],
                          A[2],
                          A[3],
                          A[4],
                          A[5],
                          A[6],
                          A[7],
                          &E[0],
                          &E[1],
                          &E[2],
                          &E[3],
                          &E[4],
                          &E[5],
                          &E[6],
                          &E[7],
                          dir);
    dft8x8lts_q15_256_dir(B[0],
                          B[1],
                          B[2],
                          B[3],
                          B[4],
                          B[5],
                          B[6],
                          B[7],
                          &O[0],
                          &O[1],
                          &O[2],
                          &O[3],
                          &O[4],
                          &O[5],
                          &O[6],
                          &O[7],
                          dir);

    /* r = 0: W256^(0*n) / sqrt(8) is purely real. */
    R[0] = oc_cq15x8_mulhrs_i16(E[0], inv_sqrt8);
    R[1] = complex_mul8_prepack_q15_256(O[0], tw->W_RE_RE[blocks + b], tw->W_IM_SIGNED[blocks + b]);

    for (int k = 1; k < 8; k++) {
      const int r_even = 2 * k;
      const int r_odd = r_even + 1;
      const int idx_even = r_even * blocks + b;
      const int idx_odd = r_odd * blocks + b;

      R[r_even] = complex_mul8_prepack_q15_256(E[k], tw->W_RE_RE[idx_even], tw->W_IM_SIGNED[idx_even]);
      R[r_odd] = complex_mul8_prepack_q15_256(O[k], tw->W_RE_RE[idx_odd], tw->W_IM_SIGNED[idx_odd]);
    }

    transpose8_complex_i16_256(&R[0], &R[1], &R[2], &R[3], &R[4], &R[5], &R[6], &R[7]);
    transpose8_complex_i16_256(&R[8], &R[9], &R[10], &R[11], &R[12], &R[13], &R[14], &R[15]);

    for (int lane = 0; lane < 8; lane++) {
      child_in[0][n + lane] = R[lane];
      child_in[1][n + lane] = R[8 + lane];
    }
  }
}

static void dft256_radix16_q15(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  const int dir_slot = (dir == DFT_DIR_FORWARD) ? 0 : 1;
  if (__builtin_expect(!g_dft256_radix16_twiddles[dir_slot].initialized, 0))
    init_dft256_radix16_q15_twiddles();

  const dft256_radix16_twiddle_q15_t *tw = &g_dft256_radix16_twiddles[dir_slot];
  oc_cq15x8_t child_in[2][16] __attribute__((aligned(64)));

  dft256_radix16_prepare_q15(src, child_in, dir);

  for (int group = 0; group < 2; group++) {
    oc_cq15x8_t Y[16];
    const int r0 = 8 * group;

    dft16x8_radix2x8_q15_256_unitary_dir(child_in[group], Y, tw->W16_UNITARY_RE_RE, tw->W16_UNITARY_IM_SIGNED, dir);

    for (int k = 0; k < 16; k++)
      oc_cq15x8_storeu((oc_cq15x8_t *)(dst + 16 * k + r0), Y[k]);
  }
}

static void dft1024_selected_q15(const c16_t *src, c16_t *dst, dft_dir_t dir);

static void dft1024_selected_q15(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  dft1024_radix16_direct_q15(src, dst, dir);
}

static void dft2048_selected_q15(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  dft2048_radix8_q15(src, dst, dir);
}

static void dft512_radix8_direct_q15(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { N = 512, M = 64 };
  c16_t stage[N] __attribute__((aligned(64)));
  oc_cq15x8_t leaf_in[M] __attribute__((aligned(64)));

  dft_radix8_q15_stage(src, stage, N, dir);

  for (int n = 0; n < M; n += 8) {
    oc_cq15x8_t z0 = oc_cq15x8_load((const oc_cq15x8_t *)(stage + n));
    oc_cq15x8_t z1 = oc_cq15x8_load((const oc_cq15x8_t *)(stage + M + n));
    oc_cq15x8_t z2 = oc_cq15x8_load((const oc_cq15x8_t *)(stage + 2 * M + n));
    oc_cq15x8_t z3 = oc_cq15x8_load((const oc_cq15x8_t *)(stage + 3 * M + n));
    oc_cq15x8_t z4 = oc_cq15x8_load((const oc_cq15x8_t *)(stage + 4 * M + n));
    oc_cq15x8_t z5 = oc_cq15x8_load((const oc_cq15x8_t *)(stage + 5 * M + n));
    oc_cq15x8_t z6 = oc_cq15x8_load((const oc_cq15x8_t *)(stage + 6 * M + n));
    oc_cq15x8_t z7 = oc_cq15x8_load((const oc_cq15x8_t *)(stage + 7 * M + n));

    transpose8_complex_i16_256(&z0, &z1, &z2, &z3, &z4, &z5, &z6, &z7);
    leaf_in[n + 0] = z0;
    leaf_in[n + 1] = z1;
    leaf_in[n + 2] = z2;
    leaf_in[n + 3] = z3;
    leaf_in[n + 4] = z4;
    leaf_in[n + 5] = z5;
    leaf_in[n + 6] = z6;
    leaf_in[n + 7] = z7;
  }

  dft64x8_batch_q15_256_store(leaf_in, dst, 8, 0, dir);
}

static void dft512_radix8_q15(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  dft512_radix8_direct_q15(src, dst, dir);
}

/*
 * Fused DFT2048 = radix-8 x radix-16 x DFT16.
 *
 * The eight DFT256 children are evaluated in lockstep.  Their final NEON
 * registers are transposed across the outer radix-8 dimension and written
 * directly as natural DFT2048 bins.  No DFT256 output arrays and no outer
 * result transpose are materialized.
 */
static void dft2048_radix8x16_fused_q15(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { N = 2048, CHILD_N = 256, M = 16 };
  c16_t outer_stage[N] __attribute__((aligned(64)));

  /*
   * child_in[r16][n] is one NEON vector whose eight complex lanes are the
   * eight outer radix-8 branches.  Therefore each DFT16 below evaluates the
   * same radix-16 branch for all outer children in parallel, and its result
   * can be stored directly as eight consecutive final DFT2048 bins.
   */
  oc_cq15x8_t child_in[16][16] __attribute__((aligned(64)));

  dft_radix8_q15_stage(src, outer_stage, N, dir);
  init_dft256_radix16_q15_twiddles();

  const int dir_slot = (dir == DFT_DIR_FORWARD) ? 0 : 1;
  const dft256_radix16_twiddle_q15_t *tw = &g_dft256_radix16_twiddles[dir_slot];
  const oc_cq15x8_t inv_sqrt2 = oc_cq15x8_set1_i16(Q15_INV_SQRT2);

  for (int b = 0; b < 2; b++) {
    const int n = 8 * b;
    oc_cq15x8_t R[8][16] __attribute__((aligned(64)));

    for (int outer_r = 0; outer_r < 8; outer_r++) {
      const c16_t *child_src = outer_stage + outer_r * CHILD_N;
      oc_cq15x8_t A[8];
      oc_cq15x8_t B[8];
      oc_cq15x8_t E[8];
      oc_cq15x8_t O[8];

      for (int q = 0; q < 8; q++) {
        const oc_cq15x8_t lo = oc_cq15x8_loadu((const oc_cq15x8_t *)(child_src + q * M + n));
        const oc_cq15x8_t hi = oc_cq15x8_loadu((const oc_cq15x8_t *)(child_src + (q + 8) * M + n));

        A[q] = oc_cq15x8_mulhrs_i16(oc_cq15x8_adds_i16(lo, hi), inv_sqrt2);
        B[q] = complex_mul8_prepack_q15_256(oc_cq15x8_subs_i16(lo, hi), tw->W16_RE_RE[q], tw->W16_IM_SIGNED[q]);
      }

      dft8x8lts_q15_256_dir(A[0],
                            A[1],
                            A[2],
                            A[3],
                            A[4],
                            A[5],
                            A[6],
                            A[7],
                            &E[0],
                            &E[1],
                            &E[2],
                            &E[3],
                            &E[4],
                            &E[5],
                            &E[6],
                            &E[7],
                            dir);
      dft8x8lts_q15_256_dir(B[0],
                            B[1],
                            B[2],
                            B[3],
                            B[4],
                            B[5],
                            B[6],
                            B[7],
                            &O[0],
                            &O[1],
                            &O[2],
                            &O[3],
                            &O[4],
                            &O[5],
                            &O[6],
                            &O[7],
                            dir);

      for (int k = 0; k < 8; k++) {
        const int r_even = 2 * k;
        const int r_odd = r_even + 1;
        const int idx_even = r_even * 2 + b;
        const int idx_odd = r_odd * 2 + b;

        R[outer_r][r_even] = complex_mul8_prepack_q15_256(E[k], tw->W_RE_RE[idx_even], tw->W_IM_SIGNED[idx_even]);
        R[outer_r][r_odd] = complex_mul8_prepack_q15_256(O[k], tw->W_RE_RE[idx_odd], tw->W_IM_SIGNED[idx_odd]);
      }
    }

    /* Transpose outer-child x n entirely in registers. */
    for (int r16 = 0; r16 < 16; r16++) {
      oc_cq15x8_t z0 = R[0][r16];
      oc_cq15x8_t z1 = R[1][r16];
      oc_cq15x8_t z2 = R[2][r16];
      oc_cq15x8_t z3 = R[3][r16];
      oc_cq15x8_t z4 = R[4][r16];
      oc_cq15x8_t z5 = R[5][r16];
      oc_cq15x8_t z6 = R[6][r16];
      oc_cq15x8_t z7 = R[7][r16];

      transpose8_complex_i16_256(&z0, &z1, &z2, &z3, &z4, &z5, &z6, &z7);

      child_in[r16][n + 0] = z0;
      child_in[r16][n + 1] = z1;
      child_in[r16][n + 2] = z2;
      child_in[r16][n + 3] = z3;
      child_in[r16][n + 4] = z4;
      child_in[r16][n + 5] = z5;
      child_in[r16][n + 6] = z6;
      child_in[r16][n + 7] = z7;
    }
  }

  for (int r16 = 0; r16 < 16; r16++) {
    oc_cq15x8_t Y[16];
    dft16x8_radix2x8_q15_256_unitary_dir(child_in[r16], Y, tw->W16_UNITARY_RE_RE, tw->W16_UNITARY_IM_SIGNED, dir);

    for (int k = 0; k < 16; k++)
      oc_cq15x8_storeu((oc_cq15x8_t *)(dst + 128 * k + 8 * r16), Y[k]);
  }
}

/* DFT2048 = 8 x DFT256, using the selected direct DFT256 leaf. */
static void dft2048_radix8_q15(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  dft2048_radix8x16_fused_q15(src, dst, dir);
}

/*
 * DFT4096 = radix-8 x radix-8 x DFT64, with the eight outer children
 * evaluated in parallel.  Every DFT64 result vector already contains the
 * eight consecutive outer-radix bins expected by natural DFT4096 order.
 */
static void dft4096_radix8x8_fused_q15(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { N = 4096, CHILD_N = 512, M = 64 };
  c16_t outer_stage[N] __attribute__((aligned(64)));
  oc_cq15x8_t leaf_in[8][64] __attribute__((aligned(64)));

  dft_radix8_q15_stage(src, outer_stage, N, dir);
  init_dft_radix8_q15_twiddles(CHILD_N);

  const int dir_slot = (dir == DFT_DIR_FORWARD) ? 0 : 1;
  const dft_radix8_twiddle_q15_t *tw = &g_dft_radix8_twiddles[dft_radix8_size_slot(CHILD_N)][dir_slot];

  for (int b = 0; b < 8; b++) {
    const int n = 8 * b;
    oc_cq15x8_t R[8][8] __attribute__((aligned(64)));

    for (int outer_r = 0; outer_r < 8; outer_r++) {
      const c16_t *child_src = outer_stage + outer_r * CHILD_N;
      oc_cq15x8_t H[8];

      const oc_cq15x8_t x0 = oc_cq15x8_loadu((const oc_cq15x8_t *)(child_src + n));
      const oc_cq15x8_t x1 = oc_cq15x8_loadu((const oc_cq15x8_t *)(child_src + M + n));
      const oc_cq15x8_t x2 = oc_cq15x8_loadu((const oc_cq15x8_t *)(child_src + 2 * M + n));
      const oc_cq15x8_t x3 = oc_cq15x8_loadu((const oc_cq15x8_t *)(child_src + 3 * M + n));
      const oc_cq15x8_t x4 = oc_cq15x8_loadu((const oc_cq15x8_t *)(child_src + 4 * M + n));
      const oc_cq15x8_t x5 = oc_cq15x8_loadu((const oc_cq15x8_t *)(child_src + 5 * M + n));
      const oc_cq15x8_t x6 = oc_cq15x8_loadu((const oc_cq15x8_t *)(child_src + 6 * M + n));
      const oc_cq15x8_t x7 = oc_cq15x8_loadu((const oc_cq15x8_t *)(child_src + 7 * M + n));

      dft8x8lts_q15_256_dir(x0, x1, x2, x3, x4, x5, x6, x7, &H[0], &H[1], &H[2], &H[3], &H[4], &H[5], &H[6], &H[7], dir);

      for (int r = 0; r < 8; r++) {
        const int idx = r * 8 + b;
        R[outer_r][r] = complex_mul8_prepack_q15_256(H[r], tw->W_RE_RE[idx], tw->W_IM_SIGNED[idx]);
      }
    }

    for (int r = 0; r < 8; r++) {
      oc_cq15x8_t z0 = R[0][r];
      oc_cq15x8_t z1 = R[1][r];
      oc_cq15x8_t z2 = R[2][r];
      oc_cq15x8_t z3 = R[3][r];
      oc_cq15x8_t z4 = R[4][r];
      oc_cq15x8_t z5 = R[5][r];
      oc_cq15x8_t z6 = R[6][r];
      oc_cq15x8_t z7 = R[7][r];
      transpose8_complex_i16_256(&z0, &z1, &z2, &z3, &z4, &z5, &z6, &z7);
      leaf_in[r][n + 0] = z0;
      leaf_in[r][n + 1] = z1;
      leaf_in[r][n + 2] = z2;
      leaf_in[r][n + 3] = z3;
      leaf_in[r][n + 4] = z4;
      leaf_in[r][n + 5] = z5;
      leaf_in[r][n + 6] = z6;
      leaf_in[r][n + 7] = z7;
    }
  }

  for (int r = 0; r < 8; r++)
    dft64x8_batch_q15_256_store(leaf_in[r], dst, 64, 8 * r, dir);
}

/*
 * DFT8192 = radix-8 x radix-16 x DFT64, also batched across the eight
 * outer radix-8 children.  It is bit-for-bit equivalent to
 * dft_radix8_q15(..., dft1024_radix16_q15), but has no child result arrays
 * and no outer result transpose.
 */
static void dft8192_radix8x16_fused_q15(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { N = 8192, CHILD_N = 1024, M = 64 };
  c16_t outer_stage[N] __attribute__((aligned(64)));
  oc_cq15x8_t leaf_in[16][64] __attribute__((aligned(64)));

  dft_radix8_q15_stage(src, outer_stage, N, dir);
  init_dft1024_radix16_q15_twiddles();

  const int dir_slot = (dir == DFT_DIR_FORWARD) ? 0 : 1;
  const dft1024_radix16_twiddle_q15_t *tw = &g_dft1024_radix16_twiddles[dir_slot];
  const oc_cq15x8_t inv_sqrt2 = oc_cq15x8_set1_i16(Q15_INV_SQRT2);

  for (int b = 0; b < 8; b++) {
    const int n = 8 * b;
    oc_cq15x8_t R[8][16] __attribute__((aligned(64)));

    for (int outer_r = 0; outer_r < 8; outer_r++) {
      const c16_t *child_src = outer_stage + outer_r * CHILD_N;
      oc_cq15x8_t A[8];
      oc_cq15x8_t B[8];
      oc_cq15x8_t E[8];
      oc_cq15x8_t O[8];

      for (int q = 0; q < 8; q++) {
        const oc_cq15x8_t lo = oc_cq15x8_loadu((const oc_cq15x8_t *)(child_src + q * M + n));
        const oc_cq15x8_t hi = oc_cq15x8_loadu((const oc_cq15x8_t *)(child_src + (q + 8) * M + n));
        A[q] = oc_cq15x8_mulhrs_i16(oc_cq15x8_adds_i16(lo, hi), inv_sqrt2);
        B[q] = complex_mul8_prepack_q15_256(oc_cq15x8_subs_i16(lo, hi), tw->W16_RE_RE[q], tw->W16_IM_SIGNED[q]);
      }

      dft8x8lts_q15_256_dir(A[0],
                            A[1],
                            A[2],
                            A[3],
                            A[4],
                            A[5],
                            A[6],
                            A[7],
                            &E[0],
                            &E[1],
                            &E[2],
                            &E[3],
                            &E[4],
                            &E[5],
                            &E[6],
                            &E[7],
                            dir);
      dft8x8lts_q15_256_dir(B[0],
                            B[1],
                            B[2],
                            B[3],
                            B[4],
                            B[5],
                            B[6],
                            B[7],
                            &O[0],
                            &O[1],
                            &O[2],
                            &O[3],
                            &O[4],
                            &O[5],
                            &O[6],
                            &O[7],
                            dir);

      for (int k = 0; k < 8; k++) {
        const int r_even = 2 * k;
        const int r_odd = r_even + 1;
        const int idx_even = r_even * 8 + b;
        const int idx_odd = r_odd * 8 + b;
        R[outer_r][r_even] = complex_mul8_prepack_q15_256(E[k], tw->W_RE_RE[idx_even], tw->W_IM_SIGNED[idx_even]);
        R[outer_r][r_odd] = complex_mul8_prepack_q15_256(O[k], tw->W_RE_RE[idx_odd], tw->W_IM_SIGNED[idx_odd]);
      }
    }

    for (int r = 0; r < 16; r++) {
      oc_cq15x8_t z0 = R[0][r];
      oc_cq15x8_t z1 = R[1][r];
      oc_cq15x8_t z2 = R[2][r];
      oc_cq15x8_t z3 = R[3][r];
      oc_cq15x8_t z4 = R[4][r];
      oc_cq15x8_t z5 = R[5][r];
      oc_cq15x8_t z6 = R[6][r];
      oc_cq15x8_t z7 = R[7][r];
      transpose8_complex_i16_256(&z0, &z1, &z2, &z3, &z4, &z5, &z6, &z7);
      leaf_in[r][n + 0] = z0;
      leaf_in[r][n + 1] = z1;
      leaf_in[r][n + 2] = z2;
      leaf_in[r][n + 3] = z3;
      leaf_in[r][n + 4] = z4;
      leaf_in[r][n + 5] = z5;
      leaf_in[r][n + 6] = z6;
      leaf_in[r][n + 7] = z7;
    }
  }

  for (int r = 0; r < 16; r++)
    dft64x8_batch_q15_256_store(leaf_in[r], dst, 128, 8 * r, dir);
}

/*
 * Prepare the internal direct state of DFT2048 = radix-8 x radix-16 x DFT16.
 * No final DFT16 is executed here; the state can therefore be finished
 * directly into a still larger enclosing radix.
 */
/* Prepare DFT4096 = radix-8 x radix-8 x DFT64 before the final DFT64s. */

/* Prepare DFT8192 = radix-8 x radix-16 x DFT64 before final DFT64s. */
/* True direct radix-8 x DFT2048 for N=16384. */
/* True direct radix-8 x DFT4096 for N=32768. */

/* True direct radix-8 x DFT8192 for N=65536. */

static void dft4096_selected_q15(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  dft4096_radix8x8_fused_q15(src, dst, dir);
}

static void dft8192_radix8_q15(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  dft8192_radix8x16_fused_q15(src, dst, dir);
}

/* Exact large-size arithmetic retained from Texte colle(178). */

static void dft32768_radix8_best4096_q15(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  dft_radix8_q15(src, dst, 32768, dir, dft4096_selected_q15);
}

static void dft16384_split_178_q15(const c16_t *__restrict x, c16_t *__restrict y, c16_t *__restrict work, dft_dir_t dir)
{
  const int N = 16384;
  const int half = N >> 1;
  const int quarter = N >> 2;

  c16_t *sub_in = work;
  c16_t *sub_out = work + N;
  c16_t *E = sub_out;
  c16_t *O1 = sub_out + half;
  c16_t *O3 = sub_out + half + quarter;

  pack_split_radix_input_neon_fused(x, sub_in, N);

  /*
   * The sub-transforms write natural order directly from batched SIMD
   * registers.  No DFT8192/DFT4096 result transpose is materialized.
   */
  dft8192_radix8x16_fused_q15(sub_in, E, dir);
  dft4096_radix8x8_fused_q15(sub_in + half, O1, dir);
  dft4096_radix8x8_fused_q15(sub_in + half + quarter, O3, dir);

  const sr_twiddle_simd_t *table = (dir == DFT_DIR_FORWARD) ? sr_twiddles_fwd : sr_twiddles_bwd;
  sr_combine_simd(E, O1, O3, y, N, &table[log2_int(N)], dir);
}

static void dft65536_radix8_q15(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  dft_radix8_q15(src, dst, 65536, dir, dft8192_radix8_q15);
}

static inline void dft64_radix4_q15_true_neon(const c16_t *__restrict src, c16_t *__restrict dst);

static void dft_split_radix_pure_simd_core(const c16_t *__restrict x,
                                           c16_t *__restrict y,
                                           c16_t *__restrict work,
                                           int N,
                                           dft_dir_t dir)
{
  if (N == 64) {
    if (dir == DFT_DIR_FORWARD)
      dft64_radix4_q15_true_neon(x, y);
    else
      dft64ltslts(x, y, dir);
    return;
  }

  if (N == 128) {
    dft128lts_dir(x, y, dir);
    return;
  }

  if (N == 256) {
    dft256_radix16_q15(x, y, dir);
    return;
  }

  if (N == 512) {
    dft512_radix8_q15(x, y, dir);
    return;
  }

  if (N == 1024) {
    dft1024_selected_q15(x, y, dir);
    return;
  }

  if (N == 2048) {
    dft2048_selected_q15(x, y, dir);
    return;
  }

  if (N == 4096) {
    dft4096_selected_q15(x, y, dir);
    return;
  }

  if (N == 8192) {
    dft8192_radix8_q15(x, y, dir);
    return;
  }

  if (N == 16384) {
    dft16384_split_178_q15(x, y, work, dir);
    return;
  }

  if (N == 32768) {
    dft32768_radix8_best4096_q15(x, y, dir);
    return;
  }

  if (N == 65536) {
    dft65536_radix8_q15(x, y, dir);
    return;
  }

  const int half = N >> 1;
  const int quarter = N >> 2;

  c16_t *sub_in = work;
  c16_t *sub_out = work + N;

  c16_t *E = sub_out;
  c16_t *O1 = sub_out + half;
  c16_t *O3 = sub_out + half + quarter;

  pack_split_radix_input_neon_fused(x, sub_in, N);

  dft_split_radix_pure_simd_core(sub_in, E, work + 2 * N, half, dir);

  dft_split_radix_pure_simd_core(sub_in + half, O1, work + 2 * N, quarter, dir);

  dft_split_radix_pure_simd_core(sub_in + half + quarter, O3, work + 2 * N, quarter, dir);
  const sr_twiddle_simd_t *table = (dir == DFT_DIR_FORWARD) ? sr_twiddles_fwd : sr_twiddles_bwd;

  sr_combine_simd(E, O1, O3, y, N, &table[log2_int(N)], dir);
}

static void dft_split_radix_pure_simd(const c16_t *x, c16_t *y, int N, dft_dir_t dir)
{
  c16_t work[262144] __attribute__((aligned(64)));
  dft_split_radix_pure_simd_core(x, y, work, N, dir);
}

static inline void dft16lts_q15_native(const c16_t *__restrict src, c16_t *__restrict dst)
{
  neon_m128i H0, H1, H2, H3;
  const neon_m128i x0 = neon128_loadu_i((const neon_m128i *)(src + 0));
  const neon_m128i x1 = neon128_loadu_i((const neon_m128i *)(src + 4));
  const neon_m128i x2 = neon128_loadu_i((const neon_m128i *)(src + 8));
  const neon_m128i x3 = neon128_loadu_i((const neon_m128i *)(src + 12));

  /* First unitary DFT4 stage: scale 1/2. */
  neon_dft4x4_unitary_q15(x0, x1, x2, x3, &H0, &H1, &H2, &H3);

  H1 = complex_mul4_prepack_q15_128(H1, g_dft64f_tw.C16_RE_RE_q15_native[1], g_dft64f_tw.C16_IM_SIGNED_q15_native[1]);
  H2 = complex_mul4_prepack_q15_128(H2, g_dft64f_tw.C16_RE_RE_q15_native[2], g_dft64f_tw.C16_IM_SIGNED_q15_native[2]);
  H3 = complex_mul4_prepack_q15_128(H3, g_dft64f_tw.C16_RE_RE_q15_native[3], g_dft64f_tw.C16_IM_SIGNED_q15_native[3]);

  transpose4_complex_epi16(&H0, &H1, &H2, &H3);

  neon_m128i Y0, Y1, Y2, Y3;
  /* Second unitary DFT4 stage: another scale 1/2. */
  neon_dft4x4_unitary_q15(H0, H1, H2, H3, &Y0, &Y1, &Y2, &Y3);

  neon128_storeu_i((neon_m128i *)(dst + 0), Y0);
  neon128_storeu_i((neon_m128i *)(dst + 4), Y1);
  neon128_storeu_i((neon_m128i *)(dst + 8), Y2);
  neon128_storeu_i((neon_m128i *)(dst + 12), Y3);
}

/* Native unitary inverse DFT4 on four independent complex lanes. */
static inline void neon_idft4x4_unitary_q15(const neon_m128i x0,
                                            const neon_m128i x1,
                                            const neon_m128i x2,
                                            const neon_m128i x3,
                                            neon_m128i *Y0,
                                            neon_m128i *Y1,
                                            neon_m128i *Y2,
                                            neon_m128i *Y3)
{
  neon_m128i f0, f1, f2, f3;
  neon_dft4x4_unitary_q15(x0, x1, x2, x3, &f0, &f1, &f2, &f3);
  *Y0 = f0;
  *Y1 = f3;
  *Y2 = f2;
  *Y3 = f1;
}

/* Native unitary IDFT16: inverse radix-4 x radix-4, with conjugated W16. */
static inline void idft16lts_q15_native(const c16_t *__restrict src, c16_t *__restrict dst)
{
  neon_m128i H0, H1, H2, H3;
  const neon_m128i x0 = neon128_loadu_i((const neon_m128i *)(src + 0));
  const neon_m128i x1 = neon128_loadu_i((const neon_m128i *)(src + 4));
  const neon_m128i x2 = neon128_loadu_i((const neon_m128i *)(src + 8));
  const neon_m128i x3 = neon128_loadu_i((const neon_m128i *)(src + 12));
  neon_idft4x4_unitary_q15(x0, x1, x2, x3, &H0, &H1, &H2, &H3);
  H1 = complex_mul4_prepack_q15_128(H1,
                                    g_dft64f_tw.C16_RE_RE_q15_native_inverse[1],
                                    g_dft64f_tw.C16_IM_SIGNED_q15_native_inverse[1]);
  H2 = complex_mul4_prepack_q15_128(H2,
                                    g_dft64f_tw.C16_RE_RE_q15_native_inverse[2],
                                    g_dft64f_tw.C16_IM_SIGNED_q15_native_inverse[2]);
  H3 = complex_mul4_prepack_q15_128(H3,
                                    g_dft64f_tw.C16_RE_RE_q15_native_inverse[3],
                                    g_dft64f_tw.C16_IM_SIGNED_q15_native_inverse[3]);
  transpose4_complex_epi16(&H0, &H1, &H2, &H3);
  neon_m128i Y0, Y1, Y2, Y3;
  neon_idft4x4_unitary_q15(H0, H1, H2, H3, &Y0, &Y1, &Y2, &Y3);
  neon128_storeu_i((neon_m128i *)(dst + 0), Y0);
  neon128_storeu_i((neon_m128i *)(dst + 4), Y1);
  neon128_storeu_i((neon_m128i *)(dst + 8), Y2);
  neon128_storeu_i((neon_m128i *)(dst + 12), Y3);
}

static inline void dft64_radix4_q15_true_neon(const c16_t *__restrict src, c16_t *__restrict dst)
{
  c16_t branch[64] __attribute__((aligned(16)));
  c16_t child[64] __attribute__((aligned(16)));
  int16x4_t dc = vdup_n_s16(0);

  /* Four 4x4 complex transposes split x[4*n+r] without scalar gathers. */
  for (int group = 0; group < 4; group++) {
    neon_m128i r0 = neon128_loadu_i((const neon_m128i *)(src + 16 * group + 0));
    neon_m128i r1 = neon128_loadu_i((const neon_m128i *)(src + 16 * group + 4));
    neon_m128i r2 = neon128_loadu_i((const neon_m128i *)(src + 16 * group + 8));
    neon_m128i r3 = neon128_loadu_i((const neon_m128i *)(src + 16 * group + 12));

    transpose4_complex_epi16(&r0, &r1, &r2, &r3);

    neon128_storeu_i((neon_m128i *)(branch + 4 * group), r0);
    neon128_storeu_i((neon_m128i *)(branch + 16 + 4 * group), r1);
    neon128_storeu_i((neon_m128i *)(branch + 2 * 16 + 4 * group), r2);
    neon128_storeu_i((neon_m128i *)(branch + 3 * 16 + 4 * group), r3);
  }

  dft16lts_q15_native(branch, child);
  dft16lts_q15_native(branch + 16, child + 16);
  dft16lts_q15_native(branch + 2 * 16, child + 2 * 16);
  dft16lts_q15_native(branch + 3 * 16, child + 3 * 16);

  for (int block = 0; block < 4; block++) {
    neon_m128i A0 = neon128_loadu_i((const neon_m128i *)(child + 4 * block));
    neon_m128i A1 = neon128_loadu_i((const neon_m128i *)(child + 16 + 4 * block));
    neon_m128i A2 = neon128_loadu_i((const neon_m128i *)(child + 2 * 16 + 4 * block));
    neon_m128i A3 = neon128_loadu_i((const neon_m128i *)(child + 3 * 16 + 4 * block));

    if (block == 0)
      dc = dft64_dc_from_child_k0(A0, A1, A2, A3);

    /*
     * Branch zero has no twiddle multiply in the optimized algorithm, so its
     * 1/2 scale is the original single arithmetic shift.  For branches 1..3
     * the same scale is already fused into the packed W64 coefficients.
     */
    A0 = neon128_from_i16(vshrq_n_s16(neon128_as_i16(A0), 1));
    A1 = complex_mul4_prepack_q15_128(A1, g_dft64_r4_re_re_q15[0][0][block], g_dft64_r4_im_signed_q15[0][0][block]);
    A2 = complex_mul4_prepack_q15_128(A2, g_dft64_r4_re_re_q15[0][1][block], g_dft64_r4_im_signed_q15[0][1][block]);
    A3 = complex_mul4_prepack_q15_128(A3, g_dft64_r4_re_re_q15[0][2][block], g_dft64_r4_im_signed_q15[0][2][block]);

    neon_m128i Y0, Y1, Y2, Y3;
    neon_dft4x4_butterfly_q15(A0, A1, A2, A3, &Y0, &Y1, &Y2, &Y3);

    neon128_storeu_i((neon_m128i *)(dst + 4 * block), Y0);
    neon128_storeu_i((neon_m128i *)(dst + 16 + 4 * block), Y1);
    neon128_storeu_i((neon_m128i *)(dst + 2 * 16 + 4 * block), Y2);
    neon128_storeu_i((neon_m128i *)(dst + 3 * 16 + 4 * block), Y3);
  }

  /* Replace only bin zero with the dedicated one-rounding DC result. */
  int16x8_t first = vld1q_s16((const int16_t *)dst);
  first = vsetq_lane_s16(vget_lane_s16(dc, 0), first, 0);
  first = vsetq_lane_s16(vget_lane_s16(dc, 1), first, 1);
  vst1q_s16((int16_t *)dst, first);
}

static inline void dft8x4_q15_unitary_native(const neon_m128i x0,
                                             const neon_m128i x1,
                                             const neon_m128i x2,
                                             const neon_m128i x3,
                                             const neon_m128i x4,
                                             const neon_m128i x5,
                                             const neon_m128i x6,
                                             const neon_m128i x7,
                                             neon_m128i *Y0,
                                             neon_m128i *Y1,
                                             neon_m128i *Y2,
                                             neon_m128i *Y3,
                                             neon_m128i *Y4,
                                             neon_m128i *Y5,
                                             neon_m128i *Y6,
                                             neon_m128i *Y7)
{
  neon_m128i E0, E1, E2, E3;
  neon_m128i O0, O1, O2, O3;
  neon_dft4x4_unitary_q15(x0, x2, x4, x6, &E0, &E1, &E2, &E3);
  neon_dft4x4_unitary_q15(x1, x3, x5, x7, &O0, &O1, &O2, &O3);

  E0 = q15_mul_i16_128(E0, Q15_INV_SQRT2);
  E1 = q15_mul_i16_128(E1, Q15_INV_SQRT2);
  E2 = q15_mul_i16_128(E2, Q15_INV_SQRT2);
  E3 = q15_mul_i16_128(E3, Q15_INV_SQRT2);

  const neon_m128i half_re = neon128_set1_i16(Q15_HALF);
  const neon_m128i half_minus_j =
      neon128_setr_i16(Q15_HALF, -Q15_HALF, Q15_HALF, -Q15_HALF, Q15_HALF, -Q15_HALF, Q15_HALF, -Q15_HALF);

  const neon_m128i T0 = q15_mul_i16_128(O0, Q15_INV_SQRT2);
  /* W8^1 / sqrt(2) = (1-j)/2. */
  const neon_m128i T1 = complex_mul4_prepack_q15_128(O1, half_re, half_minus_j);
  /* W8^2 / sqrt(2) = -j/sqrt(2). */
  const neon_m128i T2 = mul_minuslts_q15_128(q15_mul_i16_128(O2, Q15_INV_SQRT2));
  /* W8^3 / sqrt(2) = (-1-j)/2. */
  const neon_m128i T3 = complex_mul4_prepack_q15_128(O3, neon128_set1_i16((int16_t)-Q15_HALF), half_minus_j);

  *Y0 = neon128_adds_i16(E0, T0);
  *Y4 = neon128_subs_i16(E0, T0);
  *Y1 = neon128_adds_i16(E1, T1);
  *Y5 = neon128_subs_i16(E1, T1);
  *Y2 = neon128_adds_i16(E2, T2);
  *Y6 = neon128_subs_i16(E2, T2);
  *Y3 = neon128_adds_i16(E3, T3);
  *Y7 = neon128_subs_i16(E3, T3);
}

static inline void idft8x4_q15_unitary_native(const neon_m128i x0,
                                               const neon_m128i x1,
                                               const neon_m128i x2,
                                               const neon_m128i x3,
                                               const neon_m128i x4,
                                               const neon_m128i x5,
                                               const neon_m128i x6,
                                               const neon_m128i x7,
                                               neon_m128i *Y0,
                                               neon_m128i *Y1,
                                               neon_m128i *Y2,
                                               neon_m128i *Y3,
                                               neon_m128i *Y4,
                                               neon_m128i *Y5,
                                               neon_m128i *Y6,
                                               neon_m128i *Y7)
{
  neon_m128i E0, E1, E2, E3;
  neon_m128i O0, O1, O2, O3;
  neon_idft4x4_unitary_q15(x0, x2, x4, x6, &E0, &E1, &E2, &E3);
  neon_idft4x4_unitary_q15(x1, x3, x5, x7, &O0, &O1, &O2, &O3);

  E0 = q15_mul_i16_128(E0, Q15_INV_SQRT2);
  E1 = q15_mul_i16_128(E1, Q15_INV_SQRT2);
  E2 = q15_mul_i16_128(E2, Q15_INV_SQRT2);
  E3 = q15_mul_i16_128(E3, Q15_INV_SQRT2);

  const neon_m128i half_re = neon128_set1_i16(Q15_HALF);
  const neon_m128i half_plus_j =
      neon128_setr_i16(-Q15_HALF, Q15_HALF, -Q15_HALF, Q15_HALF, -Q15_HALF, Q15_HALF, -Q15_HALF, Q15_HALF);

  const neon_m128i T0 = q15_mul_i16_128(O0, Q15_INV_SQRT2);
  const neon_m128i T1 = complex_mul4_prepack_q15_128(O1, half_re, half_plus_j);
  const neon_m128i T2 = mullts_q15_128(q15_mul_i16_128(O2, Q15_INV_SQRT2));
  const neon_m128i T3 = complex_mul4_prepack_q15_128(O3, neon128_set1_i16((int16_t)-Q15_HALF), half_plus_j);

  *Y0 = neon128_adds_i16(E0, T0);
  *Y4 = neon128_subs_i16(E0, T0);
  *Y1 = neon128_adds_i16(E1, T1);
  *Y5 = neon128_subs_i16(E1, T1);
  *Y2 = neon128_adds_i16(E2, T2);
  *Y6 = neon128_subs_i16(E2, T2);
  *Y3 = neon128_adds_i16(E3, T3);
  *Y7 = neon128_subs_i16(E3, T3);
}

/* Execute four independent 8-point transforms in parallel. Input and output
 * are branch-major; separate strides also support fused layouts such as F81. */
static inline void neon_dft8x4_branch_major_q15(const c16_t *src, c16_t *dst, int src_stride, int dst_stride)
{
  neon_m128i x0 = neon128_loadu_i((const neon_m128i *)(src + 0 * src_stride + 0));
  neon_m128i x1 = neon128_loadu_i((const neon_m128i *)(src + 1 * src_stride + 0));
  neon_m128i x2 = neon128_loadu_i((const neon_m128i *)(src + 2 * src_stride + 0));
  neon_m128i x3 = neon128_loadu_i((const neon_m128i *)(src + 3 * src_stride + 0));
  transpose4_complex_epi16(&x0, &x1, &x2, &x3);

  neon_m128i x4 = neon128_loadu_i((const neon_m128i *)(src + 0 * src_stride + 4));
  neon_m128i x5 = neon128_loadu_i((const neon_m128i *)(src + 1 * src_stride + 4));
  neon_m128i x6 = neon128_loadu_i((const neon_m128i *)(src + 2 * src_stride + 4));
  neon_m128i x7 = neon128_loadu_i((const neon_m128i *)(src + 3 * src_stride + 4));
  transpose4_complex_epi16(&x4, &x5, &x6, &x7);

  neon_m128i y0, y1, y2, y3, y4, y5, y6, y7;
  dft8x4_q15_unitary_native(x0, x1, x2, x3, x4, x5, x6, x7, &y0, &y1, &y2, &y3, &y4, &y5, &y6, &y7);

  transpose4_complex_epi16(&y0, &y1, &y2, &y3);
  transpose4_complex_epi16(&y4, &y5, &y6, &y7);

  neon128_storeu_i((neon_m128i *)(dst + 0 * dst_stride + 0), y0);
  neon128_storeu_i((neon_m128i *)(dst + 1 * dst_stride + 0), y1);
  neon128_storeu_i((neon_m128i *)(dst + 2 * dst_stride + 0), y2);
  neon128_storeu_i((neon_m128i *)(dst + 3 * dst_stride + 0), y3);
  neon128_storeu_i((neon_m128i *)(dst + 0 * dst_stride + 4), y4);
  neon128_storeu_i((neon_m128i *)(dst + 1 * dst_stride + 4), y5);
  neon128_storeu_i((neon_m128i *)(dst + 2 * dst_stride + 4), y6);
  neon128_storeu_i((neon_m128i *)(dst + 3 * dst_stride + 4), y7);
}

static inline void neon_idft8x4_branch_major_q15(const c16_t *src, c16_t *dst, int src_stride, int dst_stride)
{
  neon_m128i x0 = neon128_loadu_i((const neon_m128i *)(src + 0 * src_stride + 0));
  neon_m128i x1 = neon128_loadu_i((const neon_m128i *)(src + 1 * src_stride + 0));
  neon_m128i x2 = neon128_loadu_i((const neon_m128i *)(src + 2 * src_stride + 0));
  neon_m128i x3 = neon128_loadu_i((const neon_m128i *)(src + 3 * src_stride + 0));
  transpose4_complex_epi16(&x0, &x1, &x2, &x3);

  neon_m128i x4 = neon128_loadu_i((const neon_m128i *)(src + 0 * src_stride + 4));
  neon_m128i x5 = neon128_loadu_i((const neon_m128i *)(src + 1 * src_stride + 4));
  neon_m128i x6 = neon128_loadu_i((const neon_m128i *)(src + 2 * src_stride + 4));
  neon_m128i x7 = neon128_loadu_i((const neon_m128i *)(src + 3 * src_stride + 4));
  transpose4_complex_epi16(&x4, &x5, &x6, &x7);

  neon_m128i y0, y1, y2, y3, y4, y5, y6, y7;
  idft8x4_q15_unitary_native(x0, x1, x2, x3, x4, x5, x6, x7, &y0, &y1, &y2, &y3, &y4, &y5, &y6, &y7);

  transpose4_complex_epi16(&y0, &y1, &y2, &y3);
  transpose4_complex_epi16(&y4, &y5, &y6, &y7);

  neon128_storeu_i((neon_m128i *)(dst + 0 * dst_stride + 0), y0);
  neon128_storeu_i((neon_m128i *)(dst + 1 * dst_stride + 0), y1);
  neon128_storeu_i((neon_m128i *)(dst + 2 * dst_stride + 0), y2);
  neon128_storeu_i((neon_m128i *)(dst + 3 * dst_stride + 0), y3);
  neon128_storeu_i((neon_m128i *)(dst + 0 * dst_stride + 4), y4);
  neon128_storeu_i((neon_m128i *)(dst + 1 * dst_stride + 4), y5);
  neon128_storeu_i((neon_m128i *)(dst + 2 * dst_stride + 4), y6);
  neon128_storeu_i((neon_m128i *)(dst + 3 * dst_stride + 4), y7);
}

static inline void dft32lts_q15_native(const c16_t *__restrict src, c16_t *__restrict dst)
{
  const neon_m128i x0_lo = neon128_loadu_i((const neon_m128i *)(src + 0));
  const neon_m128i x0_hi = neon128_loadu_i((const neon_m128i *)(src + 4));
  const neon_m128i x1_lo = neon128_loadu_i((const neon_m128i *)(src + 8));
  const neon_m128i x1_hi = neon128_loadu_i((const neon_m128i *)(src + 12));
  const neon_m128i x2_lo = neon128_loadu_i((const neon_m128i *)(src + 16));
  const neon_m128i x2_hi = neon128_loadu_i((const neon_m128i *)(src + 20));
  const neon_m128i x3_lo = neon128_loadu_i((const neon_m128i *)(src + 24));
  const neon_m128i x3_hi = neon128_loadu_i((const neon_m128i *)(src + 28));

  neon_m128i H0_lo, H1_lo, H2_lo, H3_lo;
  neon_m128i H0_hi, H1_hi, H2_hi, H3_hi;
  neon_dft4x4_unitary_q15(x0_lo, x1_lo, x2_lo, x3_lo, &H0_lo, &H1_lo, &H2_lo, &H3_lo);
  neon_dft4x4_unitary_q15(x0_hi, x1_hi, x2_hi, x3_hi, &H0_hi, &H1_hi, &H2_hi, &H3_hi);

#define DFT32_Q15_TWIDDLE(K, VLO, VHI)                                                  \
  do {                                                                                  \
    (VLO) = complex_mul4_prepack_q15_128((VLO),                                         \
                                         g_dft64f_tw.C32_RE_RE_q15_native[(K)][0],      \
                                         g_dft64f_tw.C32_IM_SIGNED_q15_native[(K)][0]); \
    (VHI) = complex_mul4_prepack_q15_128((VHI),                                         \
                                         g_dft64f_tw.C32_RE_RE_q15_native[(K)][1],      \
                                         g_dft64f_tw.C32_IM_SIGNED_q15_native[(K)][1]); \
  } while (0)
  DFT32_Q15_TWIDDLE(1, H1_lo, H1_hi);
  DFT32_Q15_TWIDDLE(2, H2_lo, H2_hi);
  DFT32_Q15_TWIDDLE(3, H3_lo, H3_hi);
#undef DFT32_Q15_TWIDDLE

  transpose4_complex_epi16(&H0_lo, &H1_lo, &H2_lo, &H3_lo);
  transpose4_complex_epi16(&H0_hi, &H1_hi, &H2_hi, &H3_hi);

  neon_m128i Y0, Y1, Y2, Y3, Y4, Y5, Y6, Y7;
  dft8x4_q15_unitary_native(H0_lo, H1_lo, H2_lo, H3_lo, H0_hi, H1_hi, H2_hi, H3_hi, &Y0, &Y1, &Y2, &Y3, &Y4, &Y5, &Y6, &Y7);

  neon128_storeu_i((neon_m128i *)(dst + 0), Y0);
  neon128_storeu_i((neon_m128i *)(dst + 4), Y1);
  neon128_storeu_i((neon_m128i *)(dst + 8), Y2);
  neon128_storeu_i((neon_m128i *)(dst + 12), Y3);
  neon128_storeu_i((neon_m128i *)(dst + 16), Y4);
  neon128_storeu_i((neon_m128i *)(dst + 20), Y5);
  neon128_storeu_i((neon_m128i *)(dst + 24), Y6);
  neon128_storeu_i((neon_m128i *)(dst + 28), Y7);
}

static inline void idft32lts_q15_native(const c16_t *__restrict src, c16_t *__restrict dst)
{
  const neon_m128i x0_lo = neon128_loadu_i((const neon_m128i *)(src + 0));
  const neon_m128i x0_hi = neon128_loadu_i((const neon_m128i *)(src + 4));
  const neon_m128i x1_lo = neon128_loadu_i((const neon_m128i *)(src + 8));
  const neon_m128i x1_hi = neon128_loadu_i((const neon_m128i *)(src + 12));
  const neon_m128i x2_lo = neon128_loadu_i((const neon_m128i *)(src + 16));
  const neon_m128i x2_hi = neon128_loadu_i((const neon_m128i *)(src + 20));
  const neon_m128i x3_lo = neon128_loadu_i((const neon_m128i *)(src + 24));
  const neon_m128i x3_hi = neon128_loadu_i((const neon_m128i *)(src + 28));

  neon_m128i H0_lo, H1_lo, H2_lo, H3_lo;
  neon_m128i H0_hi, H1_hi, H2_hi, H3_hi;
  neon_idft4x4_unitary_q15(x0_lo, x1_lo, x2_lo, x3_lo, &H0_lo, &H1_lo, &H2_lo, &H3_lo);
  neon_idft4x4_unitary_q15(x0_hi, x1_hi, x2_hi, x3_hi, &H0_hi, &H1_hi, &H2_hi, &H3_hi);

#define IDFT32_Q15_TWIDDLE(K, VLO, VHI)                                                         \
  do {                                                                                           \
    (VLO) = complex_mul4_prepack_q15_128((VLO),                                                 \
                                         g_dft64f_tw.C32_RE_RE_q15_native_inverse[(K)][0],       \
                                         g_dft64f_tw.C32_IM_SIGNED_q15_native_inverse[(K)][0]);  \
    (VHI) = complex_mul4_prepack_q15_128((VHI),                                                 \
                                         g_dft64f_tw.C32_RE_RE_q15_native_inverse[(K)][1],       \
                                         g_dft64f_tw.C32_IM_SIGNED_q15_native_inverse[(K)][1]);  \
  } while (0)
  IDFT32_Q15_TWIDDLE(1, H1_lo, H1_hi);
  IDFT32_Q15_TWIDDLE(2, H2_lo, H2_hi);
  IDFT32_Q15_TWIDDLE(3, H3_lo, H3_hi);
#undef IDFT32_Q15_TWIDDLE

  transpose4_complex_epi16(&H0_lo, &H1_lo, &H2_lo, &H3_lo);
  transpose4_complex_epi16(&H0_hi, &H1_hi, &H2_hi, &H3_hi);

  neon_m128i Y0, Y1, Y2, Y3, Y4, Y5, Y6, Y7;
  idft8x4_q15_unitary_native(H0_lo, H1_lo, H2_lo, H3_lo, H0_hi, H1_hi, H2_hi, H3_hi, &Y0, &Y1, &Y2, &Y3, &Y4, &Y5, &Y6, &Y7);

  neon128_storeu_i((neon_m128i *)(dst + 0), Y0);
  neon128_storeu_i((neon_m128i *)(dst + 4), Y1);
  neon128_storeu_i((neon_m128i *)(dst + 8), Y2);
  neon128_storeu_i((neon_m128i *)(dst + 12), Y3);
  neon128_storeu_i((neon_m128i *)(dst + 16), Y4);
  neon128_storeu_i((neon_m128i *)(dst + 20), Y5);
  neon128_storeu_i((neon_m128i *)(dst + 24), Y6);
  neon128_storeu_i((neon_m128i *)(dst + 28), Y7);
}

typedef struct {
  int initialized;
  int N;
  int radix;
  int M;
  int16_t *q15[4];
} neon_mono_twiddle_t;

static inline neon_m128i neon_mono_broadcast_c16(c16_t z)
{
  uint32_t bits;
  memcpy(&bits, &z, sizeof(bits));
  return neon128_from_i16(vreinterpretq_s16_u32(vdupq_n_u32(bits)));
}

static inline c16_t neon_mono_lane0_c16(neon_m128i v)
{
  const uint32_t bits = vgetq_lane_u32(vreinterpretq_u32_s16(neon128_as_i16(v)), 0);
  c16_t z;
  memcpy(&z, &bits, sizeof(bits));
  return z;
}

static inline __attribute__((always_inline)) int16x8_t neon_dft4_packed_fast_q15(int16x8_t vin)
{
  /* 1/sqrt(4) = 1/2: one rounded shift, no multiplier. */
  const int16x8_t v = vrshrq_n_s16(vin, 1);
  const int16x8_t vr = vextq_s16(v, v, 4);
  const int16x8_t sum = vqaddq_s16(v, vr);
  const int16x8_t dif = vqsubq_s16(v, vr);
  const int16x8_t ss = vreinterpretq_s16_u32(vrev64q_u32(vreinterpretq_u32_s16(sum)));
  const int16x8_t ds = vreinterpretq_s16_u32(vrev64q_u32(vreinterpretq_u32_s16(dif)));
  const int16x8_t a = vqaddq_s16(sum, ss); /* Y0 replicated. */
  const int16x8_t b = vqsubq_s16(sum, ss); /* Y2 in lane-complex 0. */

  const int16x8_t dsw = vrev32q_s16(ds);
  const int16x8_t dsn = vqnegq_s16(dsw);
  const uint16x8_t even_mask = {0xffffu, 0, 0xffffu, 0, 0xffffu, 0, 0xffffu, 0};
  const int16x8_t minus_j = vbslq_s16(even_mask, dsw, dsn);
  const int16x8_t plus_j = vbslq_s16(even_mask, dsn, dsw);
  const int16x8_t c = vqaddq_s16(dif, minus_j); /* Y1 in lane-complex 0. */
  const int16x8_t e = vqaddq_s16(dif, plus_j); /* Y3 in lane-complex 0. */

  const uint32x4_t ac = vzip1q_u32(vreinterpretq_u32_s16(a), vreinterpretq_u32_s16(c));
  const uint32x4_t be = vzip1q_u32(vreinterpretq_u32_s16(b), vreinterpretq_u32_s16(e));
  return vreinterpretq_s16_u64(vzip1q_u64(vreinterpretq_u64_u32(ac), vreinterpretq_u64_u32(be)));
}

static inline __attribute__((always_inline)) int16x8_t neon_idft4_packed_fast_q15(int16x8_t vin)
{
  const int16x8_t v = vrshrq_n_s16(vin, 1);
  const int16x8_t vr = vextq_s16(v, v, 4);
  const int16x8_t sum = vqaddq_s16(v, vr);
  const int16x8_t dif = vqsubq_s16(v, vr);
  const int16x8_t ss = vreinterpretq_s16_u32(vrev64q_u32(vreinterpretq_u32_s16(sum)));
  const int16x8_t ds = vreinterpretq_s16_u32(vrev64q_u32(vreinterpretq_u32_s16(dif)));
  const int16x8_t a = vqaddq_s16(sum, ss);
  const int16x8_t b = vqsubq_s16(sum, ss);

  const int16x8_t dsw = vrev32q_s16(ds);
  const int16x8_t dsn = vqnegq_s16(dsw);
  const uint16x8_t even_mask = {0xffffu, 0, 0xffffu, 0, 0xffffu, 0, 0xffffu, 0};
  const int16x8_t minus_j = vbslq_s16(even_mask, dsw, dsn);
  const int16x8_t plus_j = vbslq_s16(even_mask, dsn, dsw);
  const int16x8_t c = vqaddq_s16(dif, plus_j);
  const int16x8_t e = vqaddq_s16(dif, minus_j);

  const uint32x4_t ac = vzip1q_u32(vreinterpretq_u32_s16(a), vreinterpretq_u32_s16(c));
  const uint32x4_t be = vzip1q_u32(vreinterpretq_u32_s16(b), vreinterpretq_u32_s16(e));
  return vreinterpretq_s16_u64(vzip1q_u64(vreinterpretq_u64_u32(ac), vreinterpretq_u64_u32(be)));
}

static inline void neon_mono_dft4_q15(const c16_t *src, c16_t *dst)
{
  const int16x8_t x = vld1q_s16((const int16_t *)src);
  vst1q_s16((int16_t *)dst, neon_dft4_packed_fast_q15(x));
}

static inline void neon_mono_idft4_q15(const c16_t *src, c16_t *dst)
{
  const int16x8_t x = vld1q_s16((const int16_t *)src);
  vst1q_s16((int16_t *)dst, neon_idft4_packed_fast_q15(x));
}

/* Fully fused NEON DFT12 = R3 x DFT4 for twiddle-only scaling.
 * Three branch vectors stay in registers through the child DFT4s. */
static inline void neon_mono_dft12_fast_q15(const c16_t *src, c16_t *dst)
{
  const int16x8_t x0 = vld1q_s16((const int16_t *)(src + 0));
  const int16x8_t x1 = vld1q_s16((const int16_t *)(src + 4));
  const int16x8_t x2 = vld1q_s16((const int16_t *)(src + 8));
  int16x8_t z0, z1, z2;
  matched_neon_dft3_q15(x0, x1, x2, &z0, &z1, &z2);
  z0 = matched_neon_real_mul_q15(z0, Q15_INV_SQRT3);
  z1 = matched_neon_cmul_q15(z1, vld1q_s16(g_sve2_tiny_tw.w12_scaled_q15[0]));
  z2 = matched_neon_cmul_q15(z2, vld1q_s16(g_sve2_tiny_tw.w12_scaled_q15[1]));
  const uint32x4x3_t o = {.val = {vreinterpretq_u32_s16(neon_dft4_packed_fast_q15(z0)),
                                  vreinterpretq_u32_s16(neon_dft4_packed_fast_q15(z1)),
                                  vreinterpretq_u32_s16(neon_dft4_packed_fast_q15(z2))}};
  vst3q_u32((uint32_t *)dst, o);
}

static inline void neon_mono_dft8_q15(const c16_t *src, c16_t *dst)
{
  neon_m128i y0, y1, y2, y3, y4, y5, y6, y7;
  dft8x4_q15_unitary_native(neon_mono_broadcast_c16(src[0]),
                            neon_mono_broadcast_c16(src[1]),
                            neon_mono_broadcast_c16(src[2]),
                            neon_mono_broadcast_c16(src[3]),
                            neon_mono_broadcast_c16(src[4]),
                            neon_mono_broadcast_c16(src[5]),
                            neon_mono_broadcast_c16(src[6]),
                            neon_mono_broadcast_c16(src[7]),
                            &y0,
                            &y1,
                            &y2,
                            &y3,
                            &y4,
                            &y5,
                            &y6,
                            &y7);
  dst[0] = neon_mono_lane0_c16(y0);
  dst[1] = neon_mono_lane0_c16(y1);
  dst[2] = neon_mono_lane0_c16(y2);
  dst[3] = neon_mono_lane0_c16(y3);
  dst[4] = neon_mono_lane0_c16(y4);
  dst[5] = neon_mono_lane0_c16(y5);
  dst[6] = neon_mono_lane0_c16(y6);
  dst[7] = neon_mono_lane0_c16(y7);
}

static inline void neon_mono_idft8_q15(const c16_t *src, c16_t *dst)
{
  neon_m128i y0, y1, y2, y3, y4, y5, y6, y7;
  idft8x4_q15_unitary_native(neon_mono_broadcast_c16(src[0]),
                             neon_mono_broadcast_c16(src[1]),
                             neon_mono_broadcast_c16(src[2]),
                             neon_mono_broadcast_c16(src[3]),
                             neon_mono_broadcast_c16(src[4]),
                             neon_mono_broadcast_c16(src[5]),
                             neon_mono_broadcast_c16(src[6]),
                             neon_mono_broadcast_c16(src[7]),
                             &y0,
                             &y1,
                             &y2,
                             &y3,
                             &y4,
                             &y5,
                             &y6,
                             &y7);
  dst[0] = neon_mono_lane0_c16(y0);
  dst[1] = neon_mono_lane0_c16(y1);
  dst[2] = neon_mono_lane0_c16(y2);
  dst[3] = neon_mono_lane0_c16(y3);
  dst[4] = neon_mono_lane0_c16(y4);
  dst[5] = neon_mono_lane0_c16(y5);
  dst[6] = neon_mono_lane0_c16(y6);
  dst[7] = neon_mono_lane0_c16(y7);
}

static int g_neon_mono_p2_tw_init;
static int16_t g_neon_mono_tw16[3][8] __attribute__((aligned(64)));
static int16_t g_neon_mono_tw128[1][128] __attribute__((aligned(64)));
static int16_t g_neon_mono_tw256[3][128] __attribute__((aligned(64)));

static void neon_mono_init_safe_p2_twiddles(void)
{
  if (g_neon_mono_p2_tw_init)
    return;
  struct spec {
    int N, R, M;
    int16_t *base;
    int stride;
  } sp[] = {
      {16, 4, 4, &g_neon_mono_tw16[0][0], 8},
      {128, 2, 64, &g_neon_mono_tw128[0][0], 128},
      {256, 4, 64, &g_neon_mono_tw256[0][0], 128},
  };
  for (unsigned si = 0; si < sizeof(sp) / sizeof(sp[0]); ++si) {
    for (int br = 1; br < sp[si].R; ++br) {
      int16_t *tw = sp[si].base + (br - 1) * sp[si].stride;
      for (int k = 0; k < sp[si].M; k++) {
        const float a = -2.0f * (float)M_PI * (float)(br * k) / (float)sp[si].N;
        tw[2 * k + 0] = q15_from_float(cosf(a));
        tw[2 * k + 1] = q15_from_float(sinf(a));
      }
    }
  }
  g_neon_mono_p2_tw_init = 1;
}

typedef void (*neon_mono_child_q15_fn_t)(const c16_t *, c16_t *);

static void neon_mono_r2_parent_q15(const c16_t *src, c16_t *dst, int N, neon_mono_child_q15_fn_t child, const int16_t *tw1)
{
  const int M = N / 2;
  c16_t b0[64] __attribute__((aligned(64)));
  c16_t b1[64] __attribute__((aligned(64)));
  c16_t y0[64] __attribute__((aligned(64)));
  c16_t y1[64] __attribute__((aligned(64)));
  AssertFatal(M <= 64, "safe R2 child too large N=%d\n", N);
  for (int off = 0; off < M; off += 4) {
    int16x8_t x0 = vld1q_s16((const int16_t *)(src + off));
    int16x8_t x1 = vld1q_s16((const int16_t *)(src + M + off));
    /* Scale operands before add/sub: no transient saturation. */
    x0 = matched_neon_real_mul_q15(x0, Q15_INV_SQRT2);
    x1 = matched_neon_real_mul_q15(x1, Q15_INV_SQRT2);
    int16x8_t z0 = vqaddq_s16(x0, x1);
    int16x8_t z1 = vqsubq_s16(x0, x1);
    z1 = matched_neon_cmul_q15(z1, vld1q_s16(tw1 + 2 * off));
    vst1q_s16((int16_t *)(b0 + off), z0);
    vst1q_s16((int16_t *)(b1 + off), z1);
  }
  child(b0, y0);
  child(b1, y1);
  for (int k = 0; k < M; k += 4) {
    const uint32x4_t a = vreinterpretq_u32_s16(vld1q_s16((const int16_t *)(y0 + k)));
    const uint32x4_t b = vreinterpretq_u32_s16(vld1q_s16((const int16_t *)(y1 + k)));
    vst1q_u32((uint32_t *)(dst + 2 * k + 0), vzip1q_u32(a, b));
    vst1q_u32((uint32_t *)(dst + 2 * k + 4), vzip2q_u32(a, b));
  }
}

static void neon_mono_r4_parent_q15(const c16_t *src,
                                    c16_t *dst,
                                    int N,
                                    neon_mono_child_q15_fn_t child,
                                    const int16_t *tw1,
                                    const int16_t *tw2,
                                    const int16_t *tw3)
{
  const int M = N / 4;
  c16_t b[4][64] __attribute__((aligned(64)));
  c16_t y[4][64] __attribute__((aligned(64)));
  AssertFatal(M <= 64, "safe R4 child too large N=%d\n", N);
  for (int off = 0; off < M; off += 4) {
    int16x8_t x0 = vld1q_s16((const int16_t *)(src + off));
    int16x8_t x1 = vld1q_s16((const int16_t *)(src + M + off));
    int16x8_t x2 = vld1q_s16((const int16_t *)(src + 2 * M + off));
    int16x8_t x3 = vld1q_s16((const int16_t *)(src + 3 * M + off));
    x0 = matched_neon_real_mul_q15(x0, Q15_HALF);
    x1 = matched_neon_real_mul_q15(x1, Q15_HALF);
    x2 = matched_neon_real_mul_q15(x2, Q15_HALF);
    x3 = matched_neon_real_mul_q15(x3, Q15_HALF);
    neon_m128i z0, z1, z2, z3;
    neon_dft4x4_butterfly_q15(neon128_from_i16(x0),
                              neon128_from_i16(x1),
                              neon128_from_i16(x2),
                              neon128_from_i16(x3),
                              &z0,
                              &z1,
                              &z2,
                              &z3);
    int16x8_t q0 = neon128_as_i16(z0);
    int16x8_t q1 = matched_neon_cmul_q15(neon128_as_i16(z1), vld1q_s16(tw1 + 2 * off));
    int16x8_t q2 = matched_neon_cmul_q15(neon128_as_i16(z2), vld1q_s16(tw2 + 2 * off));
    int16x8_t q3 = matched_neon_cmul_q15(neon128_as_i16(z3), vld1q_s16(tw3 + 2 * off));
    vst1q_s16((int16_t *)(b[0] + off), q0);
    vst1q_s16((int16_t *)(b[1] + off), q1);
    vst1q_s16((int16_t *)(b[2] + off), q2);
    vst1q_s16((int16_t *)(b[3] + off), q3);
  }
  for (int br = 0; br < 4; br++)
    child(b[br], y[br]);
  for (int k = 0; k < M; k += 4) {
    uint32x4x4_t o;
    o.val[0] = vreinterpretq_u32_s16(vld1q_s16((const int16_t *)(y[0] + k)));
    o.val[1] = vreinterpretq_u32_s16(vld1q_s16((const int16_t *)(y[1] + k)));
    o.val[2] = vreinterpretq_u32_s16(vld1q_s16((const int16_t *)(y[2] + k)));
    o.val[3] = vreinterpretq_u32_s16(vld1q_s16((const int16_t *)(y[3] + k)));
    vst4q_u32((uint32_t *)(dst + 4 * k), o);
  }
}

static void neon_mono_dft16_safe_q15(const c16_t *src, c16_t *dst)
{
  neon_mono_init_safe_p2_twiddles();
  neon_mono_r4_parent_q15(src, dst, 16, neon_mono_dft4_q15, g_neon_mono_tw16[0], g_neon_mono_tw16[1], g_neon_mono_tw16[2]);
}

static void neon_mono_dft128_safe_q15(const c16_t *src, c16_t *dst)
{
  neon_mono_init_safe_p2_twiddles();
  neon_mono_r2_parent_q15(src, dst, 128, dft64_radix4_q15_true_neon, g_neon_mono_tw128[0]);
}

static void neon_mono_dft256_safe_q15(const c16_t *src, c16_t *dst)
{
  neon_mono_init_safe_p2_twiddles();
  neon_mono_r4_parent_q15(src,
                          dst,
                          256,
                          dft64_radix4_q15_true_neon,
                          g_neon_mono_tw256[0],
                          g_neon_mono_tw256[1],
                          g_neon_mono_tw256[2]);
}

static inline void neon_mono_dft5_q15(int16x8_t x0,
                                      int16x8_t x1,
                                      int16x8_t x2,
                                      int16x8_t x3,
                                      int16x8_t x4,
                                      int16x8_t *y0,
                                      int16x8_t *y1,
                                      int16x8_t *y2,
                                      int16x8_t *y3,
                                      int16x8_t *y4)
{
  const int16x8_t t1 = vqaddq_s16(x1, x4), t2 = vqaddq_s16(x2, x3);
  const int16x8_t d1 = vqsubq_s16(x1, x4), d2 = vqsubq_s16(x2, x3);
  *y0 = vqaddq_s16(x0, vqaddq_s16(t1, t2));

  const int16x8_t b1 = vqaddq_s16(x0, vqaddq_s16(matched_neon_real_mul_q15(t1, 10126), matched_neon_real_mul_q15(t2, -26510)));
  const int16x8_t q1 = vqaddq_s16(matched_neon_real_mul_q15(d1, 31163), matched_neon_real_mul_q15(d2, 19260));
  *y1 = vqaddq_s16(b1, matched_neon_rot270_q15(q1));
  *y4 = vqaddq_s16(b1, matched_neon_rot90_q15(q1));

  const int16x8_t b2 = vqaddq_s16(x0, vqaddq_s16(matched_neon_real_mul_q15(t1, -26510), matched_neon_real_mul_q15(t2, 10126)));
  const int16x8_t q2 = vqsubq_s16(matched_neon_real_mul_q15(d1, 19260), matched_neon_real_mul_q15(d2, 31163));
  *y2 = vqaddq_s16(b2, matched_neon_rot270_q15(q2));
  *y3 = vqaddq_s16(b2, matched_neon_rot90_q15(q2));
}

static inline void matched_neon_idft3_q15(int16x8_t x0, int16x8_t x1, int16x8_t x2, int16x8_t *y0, int16x8_t *y1, int16x8_t *y2)
{
  int16x8_t f0, f1, f2;
  matched_neon_dft3_q15(x0, x1, x2, &f0, &f1, &f2);
  *y0 = f0;
  *y1 = f2;
  *y2 = f1;
}

static inline void neon_mono_idft12_fast_q15(const c16_t *src, c16_t *dst)
{
  const int16x8_t x0 = vld1q_s16((const int16_t *)(src + 0));
  const int16x8_t x1 = vld1q_s16((const int16_t *)(src + 4));
  const int16x8_t x2 = vld1q_s16((const int16_t *)(src + 8));
  int16x8_t z0, z1, z2;
  matched_neon_idft3_q15(x0, x1, x2, &z0, &z1, &z2);
  z0 = matched_neon_real_mul_q15(z0, Q15_INV_SQRT3);
  z1 = matched_neon_cmul_q15(z1, vld1q_s16(g_sve2_tiny_tw.w12_scaled_q15_inv[0]));
  z2 = matched_neon_cmul_q15(z2, vld1q_s16(g_sve2_tiny_tw.w12_scaled_q15_inv[1]));
  const uint32x4x3_t o = {.val = {vreinterpretq_u32_s16(neon_idft4_packed_fast_q15(z0)),
                                  vreinterpretq_u32_s16(neon_idft4_packed_fast_q15(z1)),
                                  vreinterpretq_u32_s16(neon_idft4_packed_fast_q15(z2))}};
  vst3q_u32((uint32_t *)dst, o);
}

static inline void neon_mono_idft5_q15(int16x8_t x0,
                                       int16x8_t x1,
                                       int16x8_t x2,
                                       int16x8_t x3,
                                       int16x8_t x4,
                                       int16x8_t *y0,
                                       int16x8_t *y1,
                                       int16x8_t *y2,
                                       int16x8_t *y3,
                                       int16x8_t *y4)
{
  int16x8_t f0, f1, f2, f3, f4;
  neon_mono_dft5_q15(x0, x1, x2, x3, x4, &f0, &f1, &f2, &f3, &f4);
  *y0 = f0;
  *y1 = f4;
  *y2 = f3;
  *y3 = f2;
  *y4 = f1;
}

static void neon_mono_leaf_q15(const c16_t *src, c16_t *dst, int N)
{
  switch (N) {
    case 1:
      dst[0] = src[0];
      return;
    case 4:
      neon_mono_dft4_q15(src, dst);
      return;
    case 8:
      neon_mono_dft8_q15(src, dst);
      return;
    case 16:
      neon_mono_dft16_safe_q15(src, dst);
      return;
    case 32:
      dft32lts_q15_native(src, dst);
      return;
    case 64:
      dft64_radix4_q15_true_neon(src, dst);
      return;
    case 128:
      neon_mono_dft128_safe_q15(src, dst);
      return;
    case 256:
      neon_mono_dft256_safe_q15(src, dst);
      return;
    default:
      AssertFatal(N >= 512 && is_power_of_two_int(N), "Invalid NEON mono power2 leaf N=%d\n", N);
      dft_split_radix_pure_simd(src, dst, N, DFT_DIR_FORWARD);
      return;
  }
}

/* NEON mixed-radix fixed-plan executor.                              */
/*                                                                           */

/* operation is native NEON.  The scalar plan metadata/twiddle storage is     */
/* reused so NEON and SVE2 share the same mathematical stage metadata.   */
/* Production uses a deterministic fixed stage sequence; no runtime    */
/* autotuning is performed. Q15 scaling is folded into R3/R5 twiddles. */

static inline void neon_execute_leaf_q15(const oai_dft_plan_t *p, const c16_t *src, c16_t *dst)
{
  AssertFatal(p->leaf_n == 1 || p->leaf_n == 12 || is_power_of_two_int(p->leaf_n),
              "NEON fused planner requires a power-of-two/DFT12 leaf, got N=%d\n",
              p->leaf_n);
  if (p->leaf_n == 12) {
    neon_mono_dft12_fast_q15(src, dst);
    return;
  }
  neon_mono_leaf_q15(src, dst, p->leaf_n);
}

static void neon_execute_mixed_plan_q15(const oai_dft_plan_t *p, int level, const c16_t *src, c16_t *dst, c16_t *work)
{
  if (level == p->depth) {
    neon_execute_leaf_q15(p, src, dst);
    return;
  }

  const sve2_mixed_parent_twiddle_t *tw = &p->tw[level];
  const int N = tw->N;
  const int M = tw->M;
  const int r = tw->radix;
  c16_t *b = work;
  c16_t *y = work + N;
  c16_t *child_work = work + 2 * N;
  AssertFatal((M & 3) == 0, "NEON fused mixed child must be multiple of four: N=%d r=%d M=%d\n", N, r, M);

  if (r == 9) {
    int rr, c3, c5, first, second;
    AssertFatal(mixed_stage_decode(p->stage_code[level], &rr, &c3, &c5, &first, &second) && rr == 9 && first == 3 && second == 3,
                "Invalid NEON F9 stage code=%u radix=%d\n",
                (unsigned)p->stage_code[level],
                r);
    (void)c3;
    (void)c5;

    const sve2_mixed_parent_twiddle_t *twB = &p->fused_first_tw[level];
    const sve2_mixed_parent_twiddle_t *twA = &p->fused_second_tw[level];
    AssertFatal(twB->radix == 3 && twB->M == 3 * M && twA->radix == 3 && twA->M == M, "Bad register F9 twiddles N=%d M=%d\n", N, M);

    for (int off = 0; off < M; off += 4) {
      int16x8_t s00, s01, s02, s10, s11, s12, s20, s21, s22;

      {
        const int16x8_t x0 = vld1q_s16((const int16_t *)(src + off));
        const int16x8_t x1 = vld1q_s16((const int16_t *)(src + 3 * M + off));
        const int16x8_t x2 = vld1q_s16((const int16_t *)(src + 6 * M + off));
        matched_neon_dft3_q15(x0, x1, x2, &s00, &s01, &s02);
        s00 = matched_neon_real_mul_q15(s00, Q15_INV_SQRT3);
        s01 = matched_neon_cmul_q15(s01, vld1q_s16(twB->q15[0] + 2 * off));
        s02 = matched_neon_cmul_q15(s02, vld1q_s16(twB->q15[1] + 2 * off));
      }
      {
        const int first_off = M + off;
        const int16x8_t x0 = vld1q_s16((const int16_t *)(src + M + off));
        const int16x8_t x1 = vld1q_s16((const int16_t *)(src + 4 * M + off));
        const int16x8_t x2 = vld1q_s16((const int16_t *)(src + 7 * M + off));
        matched_neon_dft3_q15(x0, x1, x2, &s10, &s11, &s12);
        s10 = matched_neon_real_mul_q15(s10, Q15_INV_SQRT3);
        s11 = matched_neon_cmul_q15(s11, vld1q_s16(twB->q15[0] + 2 * first_off));
        s12 = matched_neon_cmul_q15(s12, vld1q_s16(twB->q15[1] + 2 * first_off));
      }
      {
        const int first_off = 2 * M + off;
        const int16x8_t x0 = vld1q_s16((const int16_t *)(src + 2 * M + off));
        const int16x8_t x1 = vld1q_s16((const int16_t *)(src + 5 * M + off));
        const int16x8_t x2 = vld1q_s16((const int16_t *)(src + 8 * M + off));
        matched_neon_dft3_q15(x0, x1, x2, &s20, &s21, &s22);
        s20 = matched_neon_real_mul_q15(s20, Q15_INV_SQRT3);
        s21 = matched_neon_cmul_q15(s21, vld1q_s16(twB->q15[0] + 2 * first_off));
        s22 = matched_neon_cmul_q15(s22, vld1q_s16(twB->q15[1] + 2 * first_off));
      }

      {
        int16x8_t z0, z1, z2;
        matched_neon_dft3_q15(s00, s10, s20, &z0, &z1, &z2);
        z0 = matched_neon_real_mul_q15(z0, Q15_INV_SQRT3);
        z1 = matched_neon_cmul_q15(z1, vld1q_s16(twA->q15[0] + 2 * off));
        z2 = matched_neon_cmul_q15(z2, vld1q_s16(twA->q15[1] + 2 * off));
        vst1q_s16((int16_t *)(b + off), z0);
        vst1q_s16((int16_t *)(b + 3 * M + off), z1);
        vst1q_s16((int16_t *)(b + 6 * M + off), z2);
      }
      {
        int16x8_t z0, z1, z2;
        matched_neon_dft3_q15(s01, s11, s21, &z0, &z1, &z2);
        z0 = matched_neon_real_mul_q15(z0, Q15_INV_SQRT3);
        z1 = matched_neon_cmul_q15(z1, vld1q_s16(twA->q15[0] + 2 * off));
        z2 = matched_neon_cmul_q15(z2, vld1q_s16(twA->q15[1] + 2 * off));
        vst1q_s16((int16_t *)(b + M + off), z0);
        vst1q_s16((int16_t *)(b + 4 * M + off), z1);
        vst1q_s16((int16_t *)(b + 7 * M + off), z2);
      }
      {
        int16x8_t z0, z1, z2;
        matched_neon_dft3_q15(s02, s12, s22, &z0, &z1, &z2);
        z0 = matched_neon_real_mul_q15(z0, Q15_INV_SQRT3);
        z1 = matched_neon_cmul_q15(z1, vld1q_s16(twA->q15[0] + 2 * off));
        z2 = matched_neon_cmul_q15(z2, vld1q_s16(twA->q15[1] + 2 * off));
        vst1q_s16((int16_t *)(b + 2 * M + off), z0);
        vst1q_s16((int16_t *)(b + 5 * M + off), z1);
        vst1q_s16((int16_t *)(b + 8 * M + off), z2);
      }
    }

    if (M == 8 && level + 1 == p->depth) {
      int br = 0;
      for (; br + 3 < 9; br += 4)
        neon_dft8x4_branch_major_q15(b + br * M, y + br * M, M, M);
      for (; br < 9; br++)
        neon_mono_dft8_q15(b + br * M, y + br * M);
    } else {
      for (int br = 0; br < 9; br++)
        neon_execute_mixed_plan_q15(p, level + 1, b + br * M, y + br * M, child_work);
    }

    for (int k = 0; k < M; k += 4) {
      const int16x8_t v0 = vld1q_s16((const int16_t *)(y + k));
      const int16x8_t v1 = vld1q_s16((const int16_t *)(y + M + k));
      const int16x8_t v2 = vld1q_s16((const int16_t *)(y + 2 * M + k));
      const int16x8_t v3 = vld1q_s16((const int16_t *)(y + 3 * M + k));
      const int16x8_t v4 = vld1q_s16((const int16_t *)(y + 4 * M + k));
      const int16x8_t v5 = vld1q_s16((const int16_t *)(y + 5 * M + k));
      const int16x8_t v6 = vld1q_s16((const int16_t *)(y + 6 * M + k));
      const int16x8_t v7 = vld1q_s16((const int16_t *)(y + 7 * M + k));
      const uint32x4_t v8 = vreinterpretq_u32_s16(vld1q_s16((const int16_t *)(y + 8 * M + k)));
      int16x8_t a0, a1, a2, a3, c0, c1, c2, c3;
      matched_neon_transpose4_q15(v0, v1, v2, v3, &a0, &a1, &a2, &a3);
      matched_neon_transpose4_q15(v4, v5, v6, v7, &c0, &c1, &c2, &c3);

#define NEON_F9_STORE_ROW(LANE, A, C)                    \
  do {                                                   \
    uint32_t *d9 = (uint32_t *)(dst + 9 * (k + (LANE))); \
    vst1q_u32(d9 + 0, vreinterpretq_u32_s16((A)));       \
    vst1q_u32(d9 + 4, vreinterpretq_u32_s16((C)));       \
    vst1q_lane_u32(d9 + 8, v8, (LANE));                  \
  } while (0)
      NEON_F9_STORE_ROW(0, a0, c0);
      NEON_F9_STORE_ROW(1, a1, c1);
      NEON_F9_STORE_ROW(2, a2, c2);
      NEON_F9_STORE_ROW(3, a3, c3);
#undef NEON_F9_STORE_ROW
    }
    return;
  }

  if (r == 15 || r == 25) {
    int rr, c3, c5, first, second;
    AssertFatal(mixed_stage_decode(p->stage_code[level], &rr, &c3, &c5, &first, &second) && rr == r && second != 0,
                "Invalid fused NEON 235 stage code=%u radix=%d\n",
                (unsigned)p->stage_code[level],
                r);
    (void)c3;
    (void)c5;

    /* Same two recursive Q15 parents as the unfused form, but the first
     * parent's full-size scratch/interleave round-trip is removed. */
    const int B = first;
    const int A = second;
    const sve2_mixed_parent_twiddle_t *twB = &p->fused_first_tw[level];
    const sve2_mixed_parent_twiddle_t *twA = &p->fused_second_tw[level];
    AssertFatal(twB->radix == B && twB->M == A * M && twA->radix == A && twA->M == M,
                "Bad fused NEON twiddles N=%d B=%d A=%d M=%d\n",
                N,
                B,
                A,
                M);

    for (int off = 0; off < M; off += 4) {
      c16_t stage1[25][4] __attribute__((aligned(64)));

      /* First parent B for every subindex a of the second parent A. */
      for (int a = 0; a < A; a++) {
        const int first_off = a * M + off;
        if (B == 3) {
          const int16x8_t x0 = vld1q_s16((const int16_t *)(src + a * M + off));
          const int16x8_t x1 = vld1q_s16((const int16_t *)(src + (A + a) * M + off));
          const int16x8_t x2 = vld1q_s16((const int16_t *)(src + (2 * A + a) * M + off));
          int16x8_t z0, z1, z2;
          matched_neon_dft3_q15(x0, x1, x2, &z0, &z1, &z2);
          z0 = matched_neon_real_mul_q15(z0, Q15_INV_SQRT3);
          z1 = matched_neon_cmul_q15(z1, vld1q_s16(twB->q15[0] + 2 * first_off));
          z2 = matched_neon_cmul_q15(z2, vld1q_s16(twB->q15[1] + 2 * first_off));
          vst1q_s16((int16_t *)stage1[a * B + 0], z0);
          vst1q_s16((int16_t *)stage1[a * B + 1], z1);
          vst1q_s16((int16_t *)stage1[a * B + 2], z2);
        } else {
          AssertFatal(B == 5, "Invalid fused NEON first radix B=%d\n", B);
          const int16x8_t x0 = vld1q_s16((const int16_t *)(src + a * M + off));
          const int16x8_t x1 = vld1q_s16((const int16_t *)(src + (A + a) * M + off));
          const int16x8_t x2 = vld1q_s16((const int16_t *)(src + (2 * A + a) * M + off));
          const int16x8_t x3 = vld1q_s16((const int16_t *)(src + (3 * A + a) * M + off));
          const int16x8_t x4 = vld1q_s16((const int16_t *)(src + (4 * A + a) * M + off));
          int16x8_t z0, z1, z2, z3, z4;
          neon_mono_dft5_q15(x0, x1, x2, x3, x4, &z0, &z1, &z2, &z3, &z4);
          z0 = matched_neon_real_mul_q15(z0, Q15_INV_SQRT5);
          z1 = matched_neon_cmul_q15(z1, vld1q_s16(twB->q15[0] + 2 * first_off));
          z2 = matched_neon_cmul_q15(z2, vld1q_s16(twB->q15[1] + 2 * first_off));
          z3 = matched_neon_cmul_q15(z3, vld1q_s16(twB->q15[2] + 2 * first_off));
          z4 = matched_neon_cmul_q15(z4, vld1q_s16(twB->q15[3] + 2 * first_off));
          vst1q_s16((int16_t *)stage1[a * B + 0], z0);
          vst1q_s16((int16_t *)stage1[a * B + 1], z1);
          vst1q_s16((int16_t *)stage1[a * B + 2], z2);
          vst1q_s16((int16_t *)stage1[a * B + 3], z3);
          vst1q_s16((int16_t *)stage1[a * B + 4], z4);
        }
      }

      /* Fuse the two parent stages through a small local tile.
       * Results from the first parent remain in a 9/15/25 x 4-complex tile
       * and are consumed immediately by the second parent.  This avoids
       * writing a full-N intermediate matrix and reading it back, reducing
       * memory traffic and improving cache locality. */
      for (int bidx = 0; bidx < B; bidx++) {
        if (A == 3) {
          const int16x8_t x0 = vld1q_s16((const int16_t *)stage1[bidx]);
          const int16x8_t x1 = vld1q_s16((const int16_t *)stage1[B + bidx]);
          const int16x8_t x2 = vld1q_s16((const int16_t *)stage1[2 * B + bidx]);
          int16x8_t z0, z1, z2;
          matched_neon_dft3_q15(x0, x1, x2, &z0, &z1, &z2);
          z0 = matched_neon_real_mul_q15(z0, Q15_INV_SQRT3);
          z1 = matched_neon_cmul_q15(z1, vld1q_s16(twA->q15[0] + 2 * off));
          z2 = matched_neon_cmul_q15(z2, vld1q_s16(twA->q15[1] + 2 * off));
          vst1q_s16((int16_t *)(b + (bidx)*M + off), z0);
          vst1q_s16((int16_t *)(b + (bidx + B) * M + off), z1);
          vst1q_s16((int16_t *)(b + (bidx + B * 2) * M + off), z2);
        } else {
          AssertFatal(A == 5, "Invalid fused NEON second radix A=%d\n", A);
          const int16x8_t x0 = vld1q_s16((const int16_t *)stage1[bidx]);
          const int16x8_t x1 = vld1q_s16((const int16_t *)stage1[B + bidx]);
          const int16x8_t x2 = vld1q_s16((const int16_t *)stage1[2 * B + bidx]);
          const int16x8_t x3 = vld1q_s16((const int16_t *)stage1[3 * B + bidx]);
          const int16x8_t x4 = vld1q_s16((const int16_t *)stage1[4 * B + bidx]);
          int16x8_t z0, z1, z2, z3, z4;
          neon_mono_dft5_q15(x0, x1, x2, x3, x4, &z0, &z1, &z2, &z3, &z4);
          z0 = matched_neon_real_mul_q15(z0, Q15_INV_SQRT5);
          z1 = matched_neon_cmul_q15(z1, vld1q_s16(twA->q15[0] + 2 * off));
          z2 = matched_neon_cmul_q15(z2, vld1q_s16(twA->q15[1] + 2 * off));
          z3 = matched_neon_cmul_q15(z3, vld1q_s16(twA->q15[2] + 2 * off));
          z4 = matched_neon_cmul_q15(z4, vld1q_s16(twA->q15[3] + 2 * off));
          vst1q_s16((int16_t *)(b + (bidx)*M + off), z0);
          vst1q_s16((int16_t *)(b + (bidx + B) * M + off), z1);
          vst1q_s16((int16_t *)(b + (bidx + B * 2) * M + off), z2);
          vst1q_s16((int16_t *)(b + (bidx + B * 3) * M + off), z3);
          vst1q_s16((int16_t *)(b + (bidx + B * 4) * M + off), z4);
        }
      }
    }

    if (M == 8 && level + 1 == p->depth) {
      int br = 0;
      for (; br + 3 < r; br += 4)
        neon_dft8x4_branch_major_q15(b + br * M, y + br * M, M, M);
      for (; br < r; br++)
        neon_mono_dft8_q15(b + br * M, y + br * M);
    } else {
      for (int br = 0; br < r; br++)
        neon_execute_mixed_plan_q15(p, level + 1, b + br * M, y + br * M, child_work);
    }

    for (int k = 0; k < M; k += 4) {
      c16_t tile[25][4] __attribute__((aligned(64)));
      for (int br = 0; br < r; br++)
        vst1q_s16((int16_t *)tile[br], vld1q_s16((const int16_t *)(y + br * M + k)));
      for (int lane = 0; lane < 4; lane++)
        for (int br = 0; br < r; br++)
          dst[r * (k + lane) + br] = tile[br][lane];
    }
    return;
  }

  if (r == 3) {
    for (int off = 0; off < M; off += 4) {
      const int16x8_t x0 = vld1q_s16((const int16_t *)(src + off));
      const int16x8_t x1 = vld1q_s16((const int16_t *)(src + M + off));
      const int16x8_t x2 = vld1q_s16((const int16_t *)(src + 2 * M + off));
      int16x8_t z0, z1, z2;
      matched_neon_dft3_q15(x0, x1, x2, &z0, &z1, &z2);
      z0 = matched_neon_real_mul_q15(z0, Q15_INV_SQRT3);
      z1 = matched_neon_cmul_q15(z1, vld1q_s16(tw->q15[0] + 2 * off));
      z2 = matched_neon_cmul_q15(z2, vld1q_s16(tw->q15[1] + 2 * off));
      vst1q_s16((int16_t *)(b + off), z0);
      vst1q_s16((int16_t *)(b + M + off), z1);
      vst1q_s16((int16_t *)(b + 2 * M + off), z2);
    }
    for (int br = 0; br < 3; br++)
      neon_execute_mixed_plan_q15(p, level + 1, b + br * M, y + br * M, child_work);
    for (int k = 0; k < M; k += 4) {
      uint32x4x3_t out;
      out.val[0] = vreinterpretq_u32_s16(vld1q_s16((const int16_t *)(y + k)));
      out.val[1] = vreinterpretq_u32_s16(vld1q_s16((const int16_t *)(y + M + k)));
      out.val[2] = vreinterpretq_u32_s16(vld1q_s16((const int16_t *)(y + 2 * M + k)));
      vst3q_u32((uint32_t *)(dst + 3 * k), out);
    }
    return;
  }

  AssertFatal(r == 5, "Invalid planned NEON radix %d N=%d\n", r, N);
  for (int off = 0; off < M; off += 4) {
    const int16x8_t x0 = vld1q_s16((const int16_t *)(src + off));
    const int16x8_t x1 = vld1q_s16((const int16_t *)(src + M + off));
    const int16x8_t x2 = vld1q_s16((const int16_t *)(src + 2 * M + off));
    const int16x8_t x3 = vld1q_s16((const int16_t *)(src + 3 * M + off));
    const int16x8_t x4 = vld1q_s16((const int16_t *)(src + 4 * M + off));
    int16x8_t z0, z1, z2, z3, z4;
    neon_mono_dft5_q15(x0, x1, x2, x3, x4, &z0, &z1, &z2, &z3, &z4);
    z0 = matched_neon_real_mul_q15(z0, Q15_INV_SQRT5);
    z1 = matched_neon_cmul_q15(z1, vld1q_s16(tw->q15[0] + 2 * off));
    z2 = matched_neon_cmul_q15(z2, vld1q_s16(tw->q15[1] + 2 * off));
    z3 = matched_neon_cmul_q15(z3, vld1q_s16(tw->q15[2] + 2 * off));
    z4 = matched_neon_cmul_q15(z4, vld1q_s16(tw->q15[3] + 2 * off));
    vst1q_s16((int16_t *)(b + off), z0);
    vst1q_s16((int16_t *)(b + M + off), z1);
    vst1q_s16((int16_t *)(b + 2 * M + off), z2);
    vst1q_s16((int16_t *)(b + 3 * M + off), z3);
    vst1q_s16((int16_t *)(b + 4 * M + off), z4);
  }
  if (M == 8 && level + 1 == p->depth) {
    neon_dft8x4_branch_major_q15(b, y, M, M);
    neon_mono_dft8_q15(b + 4 * M, y + 4 * M);
  } else {
    for (int br = 0; br < 5; br++)
      neon_execute_mixed_plan_q15(p, level + 1, b + br * M, y + br * M, child_work);
  }
  for (int k = 0; k < M; k += 4) {
    c16_t tile[5][4] __attribute__((aligned(64)));
    for (int br = 0; br < 5; br++)
      vst1q_s16((int16_t *)tile[br], vld1q_s16((const int16_t *)(y + br * M + k)));
    for (int lane = 0; lane < 4; lane++)
      for (int br = 0; br < 5; br++)
        dst[5 * (k + lane) + br] = tile[br][lane];
  }
}

static void neon_execute_mixed_plan_inverse_q15(const oai_dft_plan_t *p, int level, const c16_t *src, c16_t *dst, c16_t *work)
{
  if (level == p->depth) {
    execute_inverse_leaf_q15(p, src, dst);
    return;
  }

  const sve2_mixed_parent_twiddle_t *tw = &p->tw[level];
  const int N = tw->N;
  const int M = tw->M;
  const int r = tw->radix;
  c16_t *b = work;
  c16_t *y = work + N;
  c16_t *child_work = work + 2 * N;
  AssertFatal((M & 3) == 0, "NEON fused mixed child must be multiple of four: N=%d r=%d M=%d\n", N, r, M);

  if (r == 9) {
    int rr, c3, c5, first, second;
    AssertFatal(mixed_stage_decode(p->stage_code[level], &rr, &c3, &c5, &first, &second) && rr == 9 && first == 3 && second == 3,
                "Invalid NEON F9 stage code=%u radix=%d\n",
                (unsigned)p->stage_code[level],
                r);
    (void)c3;
    (void)c5;

    const sve2_mixed_parent_twiddle_t *twB = &p->fused_first_tw[level];
    const sve2_mixed_parent_twiddle_t *twA = &p->fused_second_tw[level];
    AssertFatal(twB->radix == 3 && twB->M == 3 * M && twA->radix == 3 && twA->M == M, "Bad register F9 twiddles N=%d M=%d\n", N, M);

    for (int off = 0; off < M; off += 4) {
      int16x8_t s00, s01, s02, s10, s11, s12, s20, s21, s22;

      {
        const int16x8_t x0 = vld1q_s16((const int16_t *)(src + off));
        const int16x8_t x1 = vld1q_s16((const int16_t *)(src + 3 * M + off));
        const int16x8_t x2 = vld1q_s16((const int16_t *)(src + 6 * M + off));
        matched_neon_idft3_q15(x0, x1, x2, &s00, &s01, &s02);
        s00 = matched_neon_real_mul_q15(s00, Q15_INV_SQRT3);
        s01 = matched_neon_cmul_q15(s01, vld1q_s16(twB->q15_inv[0] + 2 * off));
        s02 = matched_neon_cmul_q15(s02, vld1q_s16(twB->q15_inv[1] + 2 * off));
      }
      {
        const int first_off = M + off;
        const int16x8_t x0 = vld1q_s16((const int16_t *)(src + M + off));
        const int16x8_t x1 = vld1q_s16((const int16_t *)(src + 4 * M + off));
        const int16x8_t x2 = vld1q_s16((const int16_t *)(src + 7 * M + off));
        matched_neon_idft3_q15(x0, x1, x2, &s10, &s11, &s12);
        s10 = matched_neon_real_mul_q15(s10, Q15_INV_SQRT3);
        s11 = matched_neon_cmul_q15(s11, vld1q_s16(twB->q15_inv[0] + 2 * first_off));
        s12 = matched_neon_cmul_q15(s12, vld1q_s16(twB->q15_inv[1] + 2 * first_off));
      }
      {
        const int first_off = 2 * M + off;
        const int16x8_t x0 = vld1q_s16((const int16_t *)(src + 2 * M + off));
        const int16x8_t x1 = vld1q_s16((const int16_t *)(src + 5 * M + off));
        const int16x8_t x2 = vld1q_s16((const int16_t *)(src + 8 * M + off));
        matched_neon_idft3_q15(x0, x1, x2, &s20, &s21, &s22);
        s20 = matched_neon_real_mul_q15(s20, Q15_INV_SQRT3);
        s21 = matched_neon_cmul_q15(s21, vld1q_s16(twB->q15_inv[0] + 2 * first_off));
        s22 = matched_neon_cmul_q15(s22, vld1q_s16(twB->q15_inv[1] + 2 * first_off));
      }

      {
        int16x8_t z0, z1, z2;
        matched_neon_idft3_q15(s00, s10, s20, &z0, &z1, &z2);
        z0 = matched_neon_real_mul_q15(z0, Q15_INV_SQRT3);
        z1 = matched_neon_cmul_q15(z1, vld1q_s16(twA->q15_inv[0] + 2 * off));
        z2 = matched_neon_cmul_q15(z2, vld1q_s16(twA->q15_inv[1] + 2 * off));
        vst1q_s16((int16_t *)(b + off), z0);
        vst1q_s16((int16_t *)(b + 3 * M + off), z1);
        vst1q_s16((int16_t *)(b + 6 * M + off), z2);
      }
      {
        int16x8_t z0, z1, z2;
        matched_neon_idft3_q15(s01, s11, s21, &z0, &z1, &z2);
        z0 = matched_neon_real_mul_q15(z0, Q15_INV_SQRT3);
        z1 = matched_neon_cmul_q15(z1, vld1q_s16(twA->q15_inv[0] + 2 * off));
        z2 = matched_neon_cmul_q15(z2, vld1q_s16(twA->q15_inv[1] + 2 * off));
        vst1q_s16((int16_t *)(b + M + off), z0);
        vst1q_s16((int16_t *)(b + 4 * M + off), z1);
        vst1q_s16((int16_t *)(b + 7 * M + off), z2);
      }
      {
        int16x8_t z0, z1, z2;
        matched_neon_idft3_q15(s02, s12, s22, &z0, &z1, &z2);
        z0 = matched_neon_real_mul_q15(z0, Q15_INV_SQRT3);
        z1 = matched_neon_cmul_q15(z1, vld1q_s16(twA->q15_inv[0] + 2 * off));
        z2 = matched_neon_cmul_q15(z2, vld1q_s16(twA->q15_inv[1] + 2 * off));
        vst1q_s16((int16_t *)(b + 2 * M + off), z0);
        vst1q_s16((int16_t *)(b + 5 * M + off), z1);
        vst1q_s16((int16_t *)(b + 8 * M + off), z2);
      }
    }

    if (M == 8 && level + 1 == p->depth) {
      int br = 0;
      for (; br + 3 < 9; br += 4)
        neon_idft8x4_branch_major_q15(b + br * M, y + br * M, M, M);
      for (; br < 9; br++)
        neon_mono_idft8_q15(b + br * M, y + br * M);
    } else {
      for (int br = 0; br < 9; br++)
        neon_execute_mixed_plan_inverse_q15(p, level + 1, b + br * M, y + br * M, child_work);
    }

    for (int k = 0; k < M; k += 4) {
      const int16x8_t v0 = vld1q_s16((const int16_t *)(y + k));
      const int16x8_t v1 = vld1q_s16((const int16_t *)(y + M + k));
      const int16x8_t v2 = vld1q_s16((const int16_t *)(y + 2 * M + k));
      const int16x8_t v3 = vld1q_s16((const int16_t *)(y + 3 * M + k));
      const int16x8_t v4 = vld1q_s16((const int16_t *)(y + 4 * M + k));
      const int16x8_t v5 = vld1q_s16((const int16_t *)(y + 5 * M + k));
      const int16x8_t v6 = vld1q_s16((const int16_t *)(y + 6 * M + k));
      const int16x8_t v7 = vld1q_s16((const int16_t *)(y + 7 * M + k));
      const uint32x4_t v8 = vreinterpretq_u32_s16(vld1q_s16((const int16_t *)(y + 8 * M + k)));
      int16x8_t a0, a1, a2, a3, c0, c1, c2, c3;
      matched_neon_transpose4_q15(v0, v1, v2, v3, &a0, &a1, &a2, &a3);
      matched_neon_transpose4_q15(v4, v5, v6, v7, &c0, &c1, &c2, &c3);

#define NEON_F9_STORE_ROW(LANE, A, C)                    \
  do {                                                   \
    uint32_t *d9 = (uint32_t *)(dst + 9 * (k + (LANE))); \
    vst1q_u32(d9 + 0, vreinterpretq_u32_s16((A)));       \
    vst1q_u32(d9 + 4, vreinterpretq_u32_s16((C)));       \
    vst1q_lane_u32(d9 + 8, v8, (LANE));                  \
  } while (0)
      NEON_F9_STORE_ROW(0, a0, c0);
      NEON_F9_STORE_ROW(1, a1, c1);
      NEON_F9_STORE_ROW(2, a2, c2);
      NEON_F9_STORE_ROW(3, a3, c3);
#undef NEON_F9_STORE_ROW
    }
    return;
  }

  if (r == 15 || r == 25) {
    int rr, c3, c5, first, second;
    AssertFatal(mixed_stage_decode(p->stage_code[level], &rr, &c3, &c5, &first, &second) && rr == r && second != 0,
                "Invalid fused NEON 235 stage code=%u radix=%d\n",
                (unsigned)p->stage_code[level],
                r);
    (void)c3;
    (void)c5;

    /* Same two recursive Q15 parents as the unfused form, but the first
     * parent's full-size scratch/interleave round-trip is removed. */
    const int B = first;
    const int A = second;
    const sve2_mixed_parent_twiddle_t *twB = &p->fused_first_tw[level];
    const sve2_mixed_parent_twiddle_t *twA = &p->fused_second_tw[level];
    AssertFatal(twB->radix == B && twB->M == A * M && twA->radix == A && twA->M == M,
                "Bad fused NEON twiddles N=%d B=%d A=%d M=%d\n",
                N,
                B,
                A,
                M);

    for (int off = 0; off < M; off += 4) {
      c16_t stage1[25][4] __attribute__((aligned(64)));

      /* First parent B for every subindex a of the second parent A. */
      for (int a = 0; a < A; a++) {
        const int first_off = a * M + off;
        if (B == 3) {
          const int16x8_t x0 = vld1q_s16((const int16_t *)(src + a * M + off));
          const int16x8_t x1 = vld1q_s16((const int16_t *)(src + (A + a) * M + off));
          const int16x8_t x2 = vld1q_s16((const int16_t *)(src + (2 * A + a) * M + off));
          int16x8_t z0, z1, z2;
          matched_neon_idft3_q15(x0, x1, x2, &z0, &z1, &z2);
          z0 = matched_neon_real_mul_q15(z0, Q15_INV_SQRT3);
          z1 = matched_neon_cmul_q15(z1, vld1q_s16(twB->q15_inv[0] + 2 * first_off));
          z2 = matched_neon_cmul_q15(z2, vld1q_s16(twB->q15_inv[1] + 2 * first_off));
          vst1q_s16((int16_t *)stage1[a * B + 0], z0);
          vst1q_s16((int16_t *)stage1[a * B + 1], z1);
          vst1q_s16((int16_t *)stage1[a * B + 2], z2);
        } else {
          AssertFatal(B == 5, "Invalid fused NEON first radix B=%d\n", B);
          const int16x8_t x0 = vld1q_s16((const int16_t *)(src + a * M + off));
          const int16x8_t x1 = vld1q_s16((const int16_t *)(src + (A + a) * M + off));
          const int16x8_t x2 = vld1q_s16((const int16_t *)(src + (2 * A + a) * M + off));
          const int16x8_t x3 = vld1q_s16((const int16_t *)(src + (3 * A + a) * M + off));
          const int16x8_t x4 = vld1q_s16((const int16_t *)(src + (4 * A + a) * M + off));
          int16x8_t z0, z1, z2, z3, z4;
          neon_mono_idft5_q15(x0, x1, x2, x3, x4, &z0, &z1, &z2, &z3, &z4);
          z0 = matched_neon_real_mul_q15(z0, Q15_INV_SQRT5);
          z1 = matched_neon_cmul_q15(z1, vld1q_s16(twB->q15_inv[0] + 2 * first_off));
          z2 = matched_neon_cmul_q15(z2, vld1q_s16(twB->q15_inv[1] + 2 * first_off));
          z3 = matched_neon_cmul_q15(z3, vld1q_s16(twB->q15_inv[2] + 2 * first_off));
          z4 = matched_neon_cmul_q15(z4, vld1q_s16(twB->q15_inv[3] + 2 * first_off));
          vst1q_s16((int16_t *)stage1[a * B + 0], z0);
          vst1q_s16((int16_t *)stage1[a * B + 1], z1);
          vst1q_s16((int16_t *)stage1[a * B + 2], z2);
          vst1q_s16((int16_t *)stage1[a * B + 3], z3);
          vst1q_s16((int16_t *)stage1[a * B + 4], z4);
        }
      }

      /* Fuse the two parent stages through a small local tile.
       * Results from the first parent remain in a 9/15/25 x 4-complex tile
       * and are consumed immediately by the second parent.  This avoids
       * writing a full-N intermediate matrix and reading it back, reducing
       * memory traffic and improving cache locality. */
      for (int bidx = 0; bidx < B; bidx++) {
        if (A == 3) {
          const int16x8_t x0 = vld1q_s16((const int16_t *)stage1[bidx]);
          const int16x8_t x1 = vld1q_s16((const int16_t *)stage1[B + bidx]);
          const int16x8_t x2 = vld1q_s16((const int16_t *)stage1[2 * B + bidx]);
          int16x8_t z0, z1, z2;
          matched_neon_idft3_q15(x0, x1, x2, &z0, &z1, &z2);
          z0 = matched_neon_real_mul_q15(z0, Q15_INV_SQRT3);
          z1 = matched_neon_cmul_q15(z1, vld1q_s16(twA->q15_inv[0] + 2 * off));
          z2 = matched_neon_cmul_q15(z2, vld1q_s16(twA->q15_inv[1] + 2 * off));
          vst1q_s16((int16_t *)(b + (bidx)*M + off), z0);
          vst1q_s16((int16_t *)(b + (bidx + B) * M + off), z1);
          vst1q_s16((int16_t *)(b + (bidx + B * 2) * M + off), z2);
        } else {
          AssertFatal(A == 5, "Invalid fused NEON second radix A=%d\n", A);
          const int16x8_t x0 = vld1q_s16((const int16_t *)stage1[bidx]);
          const int16x8_t x1 = vld1q_s16((const int16_t *)stage1[B + bidx]);
          const int16x8_t x2 = vld1q_s16((const int16_t *)stage1[2 * B + bidx]);
          const int16x8_t x3 = vld1q_s16((const int16_t *)stage1[3 * B + bidx]);
          const int16x8_t x4 = vld1q_s16((const int16_t *)stage1[4 * B + bidx]);
          int16x8_t z0, z1, z2, z3, z4;
          neon_mono_idft5_q15(x0, x1, x2, x3, x4, &z0, &z1, &z2, &z3, &z4);
          z0 = matched_neon_real_mul_q15(z0, Q15_INV_SQRT5);
          z1 = matched_neon_cmul_q15(z1, vld1q_s16(twA->q15_inv[0] + 2 * off));
          z2 = matched_neon_cmul_q15(z2, vld1q_s16(twA->q15_inv[1] + 2 * off));
          z3 = matched_neon_cmul_q15(z3, vld1q_s16(twA->q15_inv[2] + 2 * off));
          z4 = matched_neon_cmul_q15(z4, vld1q_s16(twA->q15_inv[3] + 2 * off));
          vst1q_s16((int16_t *)(b + (bidx)*M + off), z0);
          vst1q_s16((int16_t *)(b + (bidx + B) * M + off), z1);
          vst1q_s16((int16_t *)(b + (bidx + B * 2) * M + off), z2);
          vst1q_s16((int16_t *)(b + (bidx + B * 3) * M + off), z3);
          vst1q_s16((int16_t *)(b + (bidx + B * 4) * M + off), z4);
        }
      }
    }

    if (M == 8 && level + 1 == p->depth) {
      int br = 0;
      for (; br + 3 < r; br += 4)
        neon_idft8x4_branch_major_q15(b + br * M, y + br * M, M, M);
      for (; br < r; br++)
        neon_mono_idft8_q15(b + br * M, y + br * M);
    } else {
      for (int br = 0; br < r; br++)
        neon_execute_mixed_plan_inverse_q15(p, level + 1, b + br * M, y + br * M, child_work);
    }

    for (int k = 0; k < M; k += 4) {
      c16_t tile[25][4] __attribute__((aligned(64)));
      for (int br = 0; br < r; br++)
        vst1q_s16((int16_t *)tile[br], vld1q_s16((const int16_t *)(y + br * M + k)));
      for (int lane = 0; lane < 4; lane++)
        for (int br = 0; br < r; br++)
          dst[r * (k + lane) + br] = tile[br][lane];
    }
    return;
  }

  if (r == 3) {
    for (int off = 0; off < M; off += 4) {
      const int16x8_t x0 = vld1q_s16((const int16_t *)(src + off));
      const int16x8_t x1 = vld1q_s16((const int16_t *)(src + M + off));
      const int16x8_t x2 = vld1q_s16((const int16_t *)(src + 2 * M + off));
      int16x8_t z0, z1, z2;
      matched_neon_idft3_q15(x0, x1, x2, &z0, &z1, &z2);
      z0 = matched_neon_real_mul_q15(z0, Q15_INV_SQRT3);
      z1 = matched_neon_cmul_q15(z1, vld1q_s16(tw->q15_inv[0] + 2 * off));
      z2 = matched_neon_cmul_q15(z2, vld1q_s16(tw->q15_inv[1] + 2 * off));
      vst1q_s16((int16_t *)(b + off), z0);
      vst1q_s16((int16_t *)(b + M + off), z1);
      vst1q_s16((int16_t *)(b + 2 * M + off), z2);
    }
    for (int br = 0; br < 3; br++)
      neon_execute_mixed_plan_inverse_q15(p, level + 1, b + br * M, y + br * M, child_work);
    for (int k = 0; k < M; k += 4) {
      uint32x4x3_t out;
      out.val[0] = vreinterpretq_u32_s16(vld1q_s16((const int16_t *)(y + k)));
      out.val[1] = vreinterpretq_u32_s16(vld1q_s16((const int16_t *)(y + M + k)));
      out.val[2] = vreinterpretq_u32_s16(vld1q_s16((const int16_t *)(y + 2 * M + k)));
      vst3q_u32((uint32_t *)(dst + 3 * k), out);
    }
    return;
  }

  AssertFatal(r == 5, "Invalid planned NEON radix %d N=%d\n", r, N);
  for (int off = 0; off < M; off += 4) {
    const int16x8_t x0 = vld1q_s16((const int16_t *)(src + off));
    const int16x8_t x1 = vld1q_s16((const int16_t *)(src + M + off));
    const int16x8_t x2 = vld1q_s16((const int16_t *)(src + 2 * M + off));
    const int16x8_t x3 = vld1q_s16((const int16_t *)(src + 3 * M + off));
    const int16x8_t x4 = vld1q_s16((const int16_t *)(src + 4 * M + off));
    int16x8_t z0, z1, z2, z3, z4;
    neon_mono_idft5_q15(x0, x1, x2, x3, x4, &z0, &z1, &z2, &z3, &z4);
    z0 = matched_neon_real_mul_q15(z0, Q15_INV_SQRT5);
    z1 = matched_neon_cmul_q15(z1, vld1q_s16(tw->q15_inv[0] + 2 * off));
    z2 = matched_neon_cmul_q15(z2, vld1q_s16(tw->q15_inv[1] + 2 * off));
    z3 = matched_neon_cmul_q15(z3, vld1q_s16(tw->q15_inv[2] + 2 * off));
    z4 = matched_neon_cmul_q15(z4, vld1q_s16(tw->q15_inv[3] + 2 * off));
    vst1q_s16((int16_t *)(b + off), z0);
    vst1q_s16((int16_t *)(b + M + off), z1);
    vst1q_s16((int16_t *)(b + 2 * M + off), z2);
    vst1q_s16((int16_t *)(b + 3 * M + off), z3);
    vst1q_s16((int16_t *)(b + 4 * M + off), z4);
  }
  if (M == 8 && level + 1 == p->depth) {
    neon_idft8x4_branch_major_q15(b, y, M, M);
    neon_mono_idft8_q15(b + 4 * M, y + 4 * M);
  } else {
    for (int br = 0; br < 5; br++)
      neon_execute_mixed_plan_inverse_q15(p, level + 1, b + br * M, y + br * M, child_work);
  }
  for (int k = 0; k < M; k += 4) {
    c16_t tile[5][4] __attribute__((aligned(64)));
    for (int br = 0; br < 5; br++)
      vst1q_s16((int16_t *)tile[br], vld1q_s16((const int16_t *)(y + br * M + k)));
    for (int lane = 0; lane < 4; lane++)
      for (int br = 0; br < 5; br++)
        dst[5 * (k + lane) + br] = tile[br][lane];
  }
}

/* One immutable plan per supported DFT slot. ISA selection is fixed once at
 * initialization, so SVE2 and NEON do not need separate per-size caches. */
enum { OAI_DFT_PLAN_SLOT_1 = DFT_SIZE_IDXTABLESIZE, OAI_DFT_PLAN_SLOT_4, OAI_DFT_PLAN_SLOT_8, OAI_DFT_PLAN_SLOT_COUNT };

static pthread_mutex_t g_oai_dft_plan_mutex = PTHREAD_MUTEX_INITIALIZER;
static oai_dft_plan_t g_oai_dft_plan[OAI_DFT_PLAN_SLOT_COUNT];
static unsigned char g_oai_dft_plan_ready[OAI_DFT_PLAN_SLOT_COUNT];

static int oai_dft_plan_slot(int N)
{
  switch (N) {
    case 1:
      return OAI_DFT_PLAN_SLOT_1;
    case 4:
      return OAI_DFT_PLAN_SLOT_4;
    case 8:
      return OAI_DFT_PLAN_SLOT_8;
    default:
      break;
  }

  const dft_size_idx_t idx = get_dft(N);
  if (idx == DFT_SIZE_IDXTABLESIZE)
    return -1;
  return (int)idx;
}

static inline oai_dft_plan_t *oai_dft_plan_cache_lookup_slot(int slot)
{
  if ((unsigned)slot >= OAI_DFT_PLAN_SLOT_COUNT)
    return NULL;

  if (!__atomic_load_n(&g_oai_dft_plan_ready[slot], __ATOMIC_ACQUIRE))
    return NULL;

  return &g_oai_dft_plan[slot];
}

static __thread c16_t *g_oai_dft_q15_work;
static __thread size_t g_oai_dft_q15_work_elems;
static __thread c16_t *g_oai_dft_inverse_leaf_work;
static __thread size_t g_oai_dft_inverse_leaf_work_elems;

static pthread_once_t g_oai_dft_init_once = PTHREAD_ONCE_INIT;
static int g_oai_dft_init_ready;
static int g_oai_dft_use_sve2_vl128_flag;

static void oai_dft_initialize_once(void)
{
  init_native_q15_leaf_twiddles();
  sve2_tiny_special_prepare();
  sve2_dft_r3_fast_prepare();
  /* Detect ARM ISA/SVE2 support once during initialization.
   * Runtime DFT calls reuse this cached decision, while transform-specific
   * twiddle tables are created once with their cached plans and then reused.
   * No getauxval() or SVE vector-length query is needed in dedicated hot paths. */
  g_oai_dft_use_sve2_vl128_flag = sve2_runtime_available() && sve2_vector_bits() == 128;
  __atomic_store_n(&g_oai_dft_init_ready, 1, __ATOMIC_RELEASE);
}

static inline void oai_dft_ensure_initialized(void)
{
  if (__builtin_expect(__atomic_load_n(&g_oai_dft_init_ready, __ATOMIC_ACQUIRE), 1))
    return;
  pthread_once(&g_oai_dft_init_once, oai_dft_initialize_once);
}

/* Pre-selected mixed-radix plans from offline benchmarks.
 * Runtime dispatch uses these fixed choices directly; no benchmarking or
 * timing is performed during DFT execution. */
static int oai_dft_frozen_mixed_sequence(int N, int sve2, unsigned char *stage, int *depth)
{
  if (sve2) {
    switch (N) {
      case 24:
        stage[0] = SVE2_235_STAGE_R3;
        *depth = 1;
        return 1; /* 3x8 */
      case 36:
        stage[0] = SVE2_235_STAGE_R3;
        *depth = 1;
        return 1; /* 3x12 */
      case 48:
        stage[0] = SVE2_235_STAGE_R3;
        *depth = 1;
        return 1; /* 3x16 */
      case 60:
        stage[0] = SVE2_235_STAGE_R5;
        *depth = 1;
        return 1; /* 5x12 */
      case 72:
        stage[0] = SVE2_235_STAGE_R9;
        *depth = 1;
        return 1; /* F9x8 */
      case 96:
        stage[0] = SVE2_235_STAGE_R3;
        *depth = 1;
        return 1; /* 3x32 */
      case 108:
        stage[0] = SVE2_235_STAGE_R9;
        *depth = 1;
        return 1; /* F9x12 */
      case 120:
        stage[0] = SVE2_235_STAGE_R15_53;
        *depth = 1;
        return 1; /* F15bx8 */
      case 144:
        stage[0] = SVE2_235_STAGE_R9;
        *depth = 1;
        return 1; /* F9x16 */
      case 180:
        stage[0] = SVE2_235_STAGE_R15_53;
        *depth = 1;
        return 1; /* F15bx12 */
      case 192:
        stage[0] = SVE2_235_STAGE_R3;
        *depth = 1;
        return 1; /* 3x64 */
      case 216:
        stage[0] = SVE2_235_STAGE_R3;
        stage[1] = SVE2_235_STAGE_R9;
        *depth = 2;
        return 1; /* 3xF9x8 */
      case 240:
        stage[0] = SVE2_235_STAGE_R15_53;
        *depth = 1;
        return 1; /* F15bx16 */
      case 288:
        stage[0] = SVE2_235_STAGE_R9;
        *depth = 1;
        return 1; /* F9x32 */
      case 300:
        stage[0] = SVE2_235_STAGE_R25;
        *depth = 1;
        return 1; /* F25x12 */
      case 324:
        stage[0] = SVE2_235_STAGE_R3;
        stage[1] = SVE2_235_STAGE_R9;
        *depth = 2;
        return 1; /* 3xF9x12 */
      case 360:
        stage[0] = SVE2_235_STAGE_R3;
        stage[1] = SVE2_235_STAGE_R15_53;
        *depth = 2;
        return 1; /* 3xF15bx8 */
      case 384:
        stage[0] = SVE2_235_STAGE_R3;
        *depth = 1;
        return 1; /* 3x128 */
      case 432:
        stage[0] = SVE2_235_STAGE_R3;
        stage[1] = SVE2_235_STAGE_R9;
        *depth = 2;
        return 1; /* 3xF9x16 */
      case 480:
        stage[0] = SVE2_235_STAGE_R15_53;
        *depth = 1;
        return 1; /* F15bx32 */
      case 540:
        stage[0] = SVE2_235_STAGE_R3;
        stage[1] = SVE2_235_STAGE_R15_53;
        *depth = 2;
        return 1; /* 3xF15bx12 */
      case 576:
        stage[0] = SVE2_235_STAGE_R9;
        *depth = 1;
        return 1; /* F9x64 */
      case 600:
        stage[0] = SVE2_235_STAGE_R3;
        stage[1] = SVE2_235_STAGE_R25;
        *depth = 2;
        return 1; /* 3xF25x8 */
      case 648:
        stage[0] = SVE2_235_STAGE_R9;
        stage[1] = SVE2_235_STAGE_R9;
        *depth = 2;
        return 1; /* F81x8 */
      case 720:
        stage[0] = SVE2_235_STAGE_R3;
        stage[1] = SVE2_235_STAGE_R15_53;
        *depth = 2;
        return 1; /* 3xF15bx16 */
      case 768:
        stage[0] = SVE2_235_STAGE_R3;
        *depth = 1;
        return 1; /* 3x256 */
      case 864:
        stage[0] = SVE2_235_STAGE_R3;
        stage[1] = SVE2_235_STAGE_R9;
        *depth = 2;
        return 1; /* 3xF9x32 */
      case 900:
        stage[0] = SVE2_235_STAGE_R3;
        stage[1] = SVE2_235_STAGE_R25;
        *depth = 2;
        return 1; /* 3xF25x12 */
      case 960:
        stage[0] = SVE2_235_STAGE_R15_53;
        *depth = 1;
        return 1; /* F15bx64 */
      case 972:
        stage[0] = SVE2_235_STAGE_R9;
        stage[1] = SVE2_235_STAGE_R9;
        *depth = 2;
        return 1; /* F81x12 */
      case 1080:
        stage[0] = SVE2_235_STAGE_R9;
        stage[1] = SVE2_235_STAGE_R15_53;
        *depth = 2;
        return 1; /* F9xF15bx8 */
      case 1152:
        stage[0] = SVE2_235_STAGE_R9;
        *depth = 1;
        return 1; /* F9x128 */
      case 1200:
        stage[0] = SVE2_235_STAGE_R3;
        stage[1] = SVE2_235_STAGE_R25;
        *depth = 2;
        return 1; /* 3xF25x16 */
      case 1296:
        stage[0] = SVE2_235_STAGE_R9;
        stage[1] = SVE2_235_STAGE_R9;
        *depth = 2;
        return 1; /* F81x16 */
      case 1440:
        stage[0] = SVE2_235_STAGE_R3;
        stage[1] = SVE2_235_STAGE_R15_35;
        *depth = 2;
        return 1; /* 3xF15ax32 */
      case 1500:
        stage[0] = SVE2_235_STAGE_R5;
        stage[1] = SVE2_235_STAGE_R25;
        *depth = 2;
        return 1; /* 5xF25x12 */
      case 1536:
        stage[0] = SVE2_235_STAGE_R3;
        *depth = 1;
        return 1; /* 3x512 */
      case 1620:
        stage[0] = SVE2_235_STAGE_R9;
        stage[1] = SVE2_235_STAGE_R15_53;
        *depth = 2;
        return 1; /* F9xF15bx12 */
      case 1728:
        stage[0] = SVE2_235_STAGE_R3;
        stage[1] = SVE2_235_STAGE_R9;
        *depth = 2;
        return 1; /* 3xF9x64 */
      case 1800:
        stage[0] = SVE2_235_STAGE_R9;
        stage[1] = SVE2_235_STAGE_R25;
        *depth = 2;
        return 1; /* F9xF25x8 */
      case 1920:
        stage[0] = SVE2_235_STAGE_R15_53;
        *depth = 1;
        return 1; /* F15bx128 */
      case 1944:
        stage[0] = SVE2_235_STAGE_R3;
        stage[1] = SVE2_235_STAGE_R9;
        stage[2] = SVE2_235_STAGE_R9;
        *depth = 3;
        return 1; /* 3xF9xF9x8 */
      case 2160:
        stage[0] = SVE2_235_STAGE_R9;
        stage[1] = SVE2_235_STAGE_R15_53;
        *depth = 2;
        return 1; /* F9xF15bx16 */
      case 2304:
        stage[0] = SVE2_235_STAGE_R9;
        *depth = 1;
        return 1; /* F9x256 */
      case 2400:
        stage[0] = SVE2_235_STAGE_R3;
        stage[1] = SVE2_235_STAGE_R25;
        *depth = 2;
        return 1; /* 3xF25x32 */
      case 2592:
        stage[0] = SVE2_235_STAGE_R9;
        stage[1] = SVE2_235_STAGE_R9;
        *depth = 2;
        return 1; /* F81x32 */
      case 3072:
        stage[0] = SVE2_235_STAGE_R3;
        *depth = 1;
        return 1; /* 3x1024 */
      case 6144:
        stage[0] = SVE2_235_STAGE_R3;
        *depth = 1;
        return 1; /* 3x2048 */
      case 12288:
        stage[0] = SVE2_235_STAGE_R3;
        *depth = 1;
        return 1; /* 3x4096 */
      default:
        break;
    }
  } else {
    switch (N) {
      case 24:
        stage[0] = SVE2_235_STAGE_R3;
        *depth = 1;
        return 1;
      case 36:
        stage[0] = SVE2_235_STAGE_R3;
        *depth = 1;
        return 1;
      case 48:
        stage[0] = SVE2_235_STAGE_R3;
        *depth = 1;
        return 1;
      case 60:
        stage[0] = SVE2_235_STAGE_R5;
        *depth = 1;
        return 1;
      case 72:
        stage[0] = SVE2_235_STAGE_R9;
        *depth = 1;
        return 1;
      case 96:
        stage[0] = SVE2_235_STAGE_R3;
        *depth = 1;
        return 1;
      case 108:
        stage[0] = SVE2_235_STAGE_R9;
        *depth = 1;
        return 1;
      case 120:
        stage[0] = SVE2_235_STAGE_R15_35;
        *depth = 1;
        return 1;
      case 144:
        stage[0] = SVE2_235_STAGE_R9;
        *depth = 1;
        return 1;
      case 180:
        stage[0] = SVE2_235_STAGE_R15_35;
        *depth = 1;
        return 1;
      case 192:
        stage[0] = SVE2_235_STAGE_R3;
        *depth = 1;
        return 1;
      case 216:
        stage[0] = SVE2_235_STAGE_R3;
        stage[1] = SVE2_235_STAGE_R9;
        *depth = 2;
        return 1;
      case 240:
        stage[0] = SVE2_235_STAGE_R15_35;
        *depth = 1;
        return 1;
      case 288:
        stage[0] = SVE2_235_STAGE_R9;
        *depth = 1;
        return 1;
      case 300:
        stage[0] = SVE2_235_STAGE_R25;
        *depth = 1;
        return 1;
      case 324:
        stage[0] = SVE2_235_STAGE_R3;
        stage[1] = SVE2_235_STAGE_R9;
        *depth = 2;
        return 1;
      case 360:
        stage[0] = SVE2_235_STAGE_R9;
        stage[1] = SVE2_235_STAGE_R5;
        *depth = 2;
        return 1;
      case 384:
        stage[0] = SVE2_235_STAGE_R3;
        *depth = 1;
        return 1;
      case 432:
        stage[0] = SVE2_235_STAGE_R3;
        stage[1] = SVE2_235_STAGE_R9;
        *depth = 2;
        return 1;
      case 480:
        stage[0] = SVE2_235_STAGE_R15_35;
        *depth = 1;
        return 1;
      case 540:
        stage[0] = SVE2_235_STAGE_R9;
        stage[1] = SVE2_235_STAGE_R5;
        *depth = 2;
        return 1;
      case 576:
        stage[0] = SVE2_235_STAGE_R9;
        *depth = 1;
        return 1;
      case 600:
        stage[0] = SVE2_235_STAGE_R3;
        stage[1] = SVE2_235_STAGE_R25;
        *depth = 2;
        return 1;
      case 648:
        stage[0] = SVE2_235_STAGE_R9;
        stage[1] = SVE2_235_STAGE_R9;
        *depth = 2;
        return 1;
      case 720:
        stage[0] = SVE2_235_STAGE_R9;
        stage[1] = SVE2_235_STAGE_R5;
        *depth = 2;
        return 1;
      case 768:
        stage[0] = SVE2_235_STAGE_R3;
        *depth = 1;
        return 1;
      case 864:
        stage[0] = SVE2_235_STAGE_R3;
        stage[1] = SVE2_235_STAGE_R9;
        *depth = 2;
        return 1;
      case 900:
        stage[0] = SVE2_235_STAGE_R3;
        stage[1] = SVE2_235_STAGE_R25;
        *depth = 2;
        return 1;
      case 960:
        stage[0] = SVE2_235_STAGE_R15_35;
        *depth = 1;
        return 1;
      case 972:
        stage[0] = SVE2_235_STAGE_R9;
        stage[1] = SVE2_235_STAGE_R9;
        *depth = 2;
        return 1;
      case 1080:
        stage[0] = SVE2_235_STAGE_R15_35;
        stage[1] = SVE2_235_STAGE_R9;
        *depth = 2;
        return 1;
      case 1152:
        stage[0] = SVE2_235_STAGE_R9;
        *depth = 1;
        return 1;
      case 1200:
        stage[0] = SVE2_235_STAGE_R3;
        stage[1] = SVE2_235_STAGE_R25;
        *depth = 2;
        return 1;
      case 1296:
        stage[0] = SVE2_235_STAGE_R9;
        stage[1] = SVE2_235_STAGE_R9;
        *depth = 2;
        return 1;
      case 1440:
        stage[0] = SVE2_235_STAGE_R3;
        stage[1] = SVE2_235_STAGE_R15_35;
        *depth = 2;
        return 1;
      case 1500:
        stage[0] = SVE2_235_STAGE_R25;
        stage[1] = SVE2_235_STAGE_R5;
        *depth = 2;
        return 1;
      case 1536:
        stage[0] = SVE2_235_STAGE_R3;
        *depth = 1;
        return 1;
      case 1620:
        stage[0] = SVE2_235_STAGE_R15_35;
        stage[1] = SVE2_235_STAGE_R9;
        *depth = 2;
        return 1;
      case 1728:
        stage[0] = SVE2_235_STAGE_R9;
        stage[1] = SVE2_235_STAGE_R3;
        *depth = 2;
        return 1;
      case 1800:
        stage[0] = SVE2_235_STAGE_R25;
        stage[1] = SVE2_235_STAGE_R9;
        *depth = 2;
        return 1;
      case 1920:
        stage[0] = SVE2_235_STAGE_R15_35;
        *depth = 1;
        return 1;
      case 1944:
        stage[0] = SVE2_235_STAGE_R9;
        stage[1] = SVE2_235_STAGE_R3;
        stage[2] = SVE2_235_STAGE_R9;
        *depth = 3;
        return 1;
      case 2160:
        stage[0] = SVE2_235_STAGE_R15_35;
        stage[1] = SVE2_235_STAGE_R9;
        *depth = 2;
        return 1;
      case 2304:
        stage[0] = SVE2_235_STAGE_R9;
        *depth = 1;
        return 1;
      case 2400:
        stage[0] = SVE2_235_STAGE_R3;
        stage[1] = SVE2_235_STAGE_R25;
        *depth = 2;
        return 1;
      case 2592:
        stage[0] = SVE2_235_STAGE_R9;
        stage[1] = SVE2_235_STAGE_R9;
        *depth = 2;
        return 1;
      case 3072:
        stage[0] = SVE2_235_STAGE_R3;
        *depth = 1;
        return 1;
      case 6144:
        stage[0] = SVE2_235_STAGE_R3;
        *depth = 1;
        return 1;
      case 12288:
        stage[0] = SVE2_235_STAGE_R3;
        *depth = 1;
        return 1;
      default:
        break;
    }
  }
  return 0;
}

/* Fallback decomposition for 2^a*3^b*5^c sizes not listed in the frozen table. */
static int oai_dft_build_mixed_sequence(int N, int sve2, unsigned char *stage, int *depth)
{
  if (oai_dft_frozen_mixed_sequence(N, sve2, stage, depth))
    return 1;

  int e2, e3, e5;
  if (!sve2_235_factor(N, &e2, &e3, &e5))
    return 0;

  if (N == 12) {
    *depth = 0;
    return 1;
  }

  int d = 0;

  /* For unlisted 4*3^a*5^b sizes, reserve one factor 3 for the fast DFT12
   * terminal instead of decomposing all the way to DFT4. */
  if (e2 == 2 && e3 > 0)
    --e3;

  /* Keep repeated radix-3 pairs adjacent: SVE2 turns R9,R9 into F81. */
  while (e3 >= 2) {
    if (d >= SVE2_235_MAX_MIXED_STAGES)
      return 0;
    stage[d++] = SVE2_235_STAGE_R9;
    e3 -= 2;
  }

  /* Expose the remaining R3 before F25 (e.g. 600 = 3 x 25 x 8). */
  if (e3 == 1 && e5 >= 2) {
    if (d >= SVE2_235_MAX_MIXED_STAGES)
      return 0;
    stage[d++] = SVE2_235_STAGE_R3;
    e3 = 0;
  }

  /* Pair one remaining 3 and 5.  SVE2 generally favored 5->3 (F15b);
   * NEON generally favored 3->5 (F15a) in the validation runs. */
  if (e3 && e5) {
    if (d >= SVE2_235_MAX_MIXED_STAGES)
      return 0;
    stage[d++] = sve2 ? SVE2_235_STAGE_R15_53 : SVE2_235_STAGE_R15_35;
    --e3;
    --e5;
  }

  if (e3) {
    if (d >= SVE2_235_MAX_MIXED_STAGES)
      return 0;
    stage[d++] = SVE2_235_STAGE_R3;
    --e3;
  }

  while (e5 >= 2) {
    if (d >= SVE2_235_MAX_MIXED_STAGES)
      return 0;
    stage[d++] = SVE2_235_STAGE_R25;
    e5 -= 2;
  }

  if (e5) {
    if (d >= SVE2_235_MAX_MIXED_STAGES)
      return 0;
    stage[d++] = SVE2_235_STAGE_R5;
    --e5;
  }

  *depth = d;
  return 1;
}

/* Frozen SVE2 VL=128 power-of-two topology. */
static int oai_dft_sve2_power2_winner(int N, int *radix, int *child_n)
{
  int r = 0;
  int child = 0;

  switch (N) {
    case 512:
      r = 8;
      child = 64;
      break;
    case 1024:
      r = 8;
      child = 128;
      break;
    case 2048:
      r = 4;
      child = 512;
      break;
    case 4096:
      r = 8;
      child = 512;
      break;
    case 8192:
      r = 8;
      child = 1024;
      break;
    case 16384:
      r = 8;
      child = 2048;
      break;
    case 32768:
      r = 8;
      child = 4096;
      break;
    case 65536:
      r = 8;
      child = 8192;
      break;
    default:
      return 0;
  }

  if (radix)
    *radix = r;
  if (child_n)
    *child_n = child;
  return 1;
}

static void oai_dft_prepare_sve2_direct_leaf_tables_locked(int N)
{
  switch (N) {
    case 8:
    case 12:
    case 16:
    case 32:
      sve2_tiny_special_prepare();
      return;

    case 64:
      sve2_dft64_special_prepare();
      return;

    case 128:
      sve2_dft64_special_prepare();
      sve2_dft128_special_prepare();
      return;

    case 256:
      sve2_dft64_special_prepare();
      sve2_dft256_special_prepare();
      return;

    case 512:
      sve2_dft64_special_prepare();
      sve2_dft512_special_prepare();
      return;

    default:
      return;
  }
}

static oai_dft_plan_t *oai_dft_get_plan_locked(int N, int sve2);

static inline int oai_dft_sve2_leaf_has_canonical_plan(int N)
{
  return N == 1 || N == 4 || N == 8 || N == 12 || N == 16 || N == 32 || N == 64 || N == 128 || N == 256 || N == 512 || N == 1024
         || N == 2048 || N == 4096 || N == 8192 || N == 16384 || N == 32768 || N == 65536;
}

static int oai_dft_attach_sve2_leaf_plan_locked(oai_dft_plan_t *plan)
{
  AssertFatal(plan != NULL, "NULL SVE2 DFT plan\n");

  const int leaf_n = plan->leaf_n;
  if (plan->depth > 0 && leaf_n != plan->N && oai_dft_sve2_leaf_has_canonical_plan(leaf_n)) {
    plan->sve2_leaf_plan = oai_dft_get_plan_locked(leaf_n, 1);
    return plan->sve2_leaf_plan != NULL;
  }

  int radix = 0;
  int child_n = 0;
  if (!oai_dft_sve2_power2_winner(leaf_n, &radix, &child_n)) {
    oai_dft_prepare_sve2_direct_leaf_tables_locked(leaf_n);
    return 1;
  }

  plan->sve2_pow2_radix = (unsigned char)radix;

  if (leaf_n == 512) {
    oai_dft_prepare_sve2_direct_leaf_tables_locked(512);
  } else {
    sve2_large_power2_prepare_one(leaf_n, radix);
  }

  plan->sve2_pow2_child_plan = oai_dft_get_plan_locked(child_n, 1);
  return plan->sve2_pow2_child_plan != NULL;
}

/* Build or reuse the complete plan for N. Twiddles and child plans are ready
 * before the plan is published.
 */
static oai_dft_plan_t *oai_dft_get_plan_slot_locked(int slot, int N, int sve2)
{
  if (N <= 0 || N > MAX_N || (unsigned)slot >= OAI_DFT_PLAN_SLOT_COUNT)
    return NULL;

  oai_dft_plan_t *plan = oai_dft_plan_cache_lookup_slot(slot);
  if (plan)
    return plan;

  plan = &g_oai_dft_plan[slot];

  unsigned char stage[SVE2_235_MAX_MIXED_STAGES] = {0};
  int depth = 0;
  if (!oai_dft_build_mixed_sequence(N, sve2, stage, &depth))
    return NULL;

  if (!oai_dft_mixed_plan_create(plan, N, stage, depth))
    return NULL;

  if (sve2 && !oai_dft_attach_sve2_leaf_plan_locked(plan)) {
    oai_dft_plan_reset(plan);
    return NULL;
  }

  __atomic_store_n(&g_oai_dft_plan_ready[slot], 1, __ATOMIC_RELEASE);
  return plan;
}

static oai_dft_plan_t *oai_dft_get_plan_locked(int N, int sve2)
{
  if (N <= 0 || N > MAX_N)
    return NULL;

  const int slot = oai_dft_plan_slot(N);
  if (slot < 0)
    return NULL;

  return oai_dft_get_plan_slot_locked(slot, N, sve2);
}

static oai_dft_plan_t *oai_dft_get_plan_slot(int slot, int N, int sve2)
{
  oai_dft_plan_t *plan = oai_dft_plan_cache_lookup_slot(slot);
  if (plan)
    return plan;

  pthread_mutex_lock(&g_oai_dft_plan_mutex);
  plan = oai_dft_get_plan_slot_locked(slot, N, sve2);
  pthread_mutex_unlock(&g_oai_dft_plan_mutex);
  return plan;
}

static c16_t *oai_dft_tls_work(size_t need)
{
  if (need <= g_oai_dft_q15_work_elems)
    return g_oai_dft_q15_work;

  c16_t *p = aligned_malloc64(need * sizeof(*p));
  if (!p)
    return NULL;

  free(g_oai_dft_q15_work);
  g_oai_dft_q15_work = p;
  g_oai_dft_q15_work_elems = need;
  return p;
}

static c16_t *oai_dft_tls_inverse_leaf_work(size_t need)
{
  if (need <= g_oai_dft_inverse_leaf_work_elems)
    return g_oai_dft_inverse_leaf_work;

  c16_t *p = aligned_malloc64(need * sizeof(*p));
  if (!p)
    return NULL;

  free(g_oai_dft_inverse_leaf_work);
  g_oai_dft_inverse_leaf_work = p;
  g_oai_dft_inverse_leaf_work_elems = need;
  return p;
}

static int oai_dft_use_sve2_vl128(void)
{
  return g_oai_dft_use_sve2_vl128_flag;
}

static void dft_mixed_radix_c16_scaled_slot(const c16_t *src, c16_t *dst, int N, int slot, dft_dir_t dir)
{
  oai_dft_ensure_initialized();

  const int use_sve2 = oai_dft_use_sve2_vl128();

  if (dir == DFT_DIR_FORWARD && use_sve2) {
    if (N == 768) {
      sve2_dft768_q15_r3x256_fast(src, dst);
      return;
    }
    if (N == 1536) {
      sve2_dft1536_q15_r3x512_fast(src, dst);
      return;
    }
  }

  oai_dft_plan_t *plan = oai_dft_get_plan_slot(slot, N, use_sve2);
  AssertFatal(plan != NULL, "AArch64 DFT: unsupported N=%d\n", N);

  c16_t *work = NULL;
  if (plan->workspace_elems) {
    work = oai_dft_tls_work(plan->workspace_elems);
    AssertFatal(work != NULL, "AArch64 DFT: scratch allocation failed N=%d\n", N);
  }

  if (dir == DFT_DIR_FORWARD) {
    if (use_sve2) {
      if (plan->depth == 0)
        sve2_execute_leaf_q15(plan, src, dst);
      else
        sve2_execute_mixed_plan_q15(plan, 0, src, dst, work);
    } else {
      if (plan->depth == 0)
        neon_execute_leaf_q15(plan, src, dst);
      else
        neon_execute_mixed_plan_q15(plan, 0, src, dst, work);
    }
    return;
  }

  if (plan->depth == 0)
    execute_inverse_leaf_q15(plan, src, dst);
  else if (use_sve2)
    sve2_execute_mixed_plan_inverse_q15(plan, 0, src, dst, work);
  else
    neon_execute_mixed_plan_inverse_q15(plan, 0, src, dst, work);
}

static void dft_mixed_radix_c16_scaled(const c16_t *src, c16_t *dst, int N, dft_dir_t dir)
{
  const int slot = oai_dft_plan_slot(N);
  AssertFatal(slot >= 0, "AArch64 DFT: unsupported N=%d\n", N);
  dft_mixed_radix_c16_scaled_slot(src, dst, N, slot, dir);
}

#define DEFINE_MIXED_DFT(N)                                                                  \
  void dft##N(int16_t *input, int16_t *output, uint8_t scale_flag)                           \
  {                                                                                          \
    (void)scale_flag;                                                                        \
    dft_mixed_radix_c16_scaled((const c16_t *)input, (c16_t *)output, (N), DFT_DIR_FORWARD); \
  }

#define DEFINE_MIXED_IDFT(N)                                                                 \
  void idft##N(int16_t *input, int16_t *output, uint8_t scale_flag)                          \
  {                                                                                          \
    (void)scale_flag;                                                                        \
    dft_mixed_radix_c16_scaled((const c16_t *)input, (c16_t *)output, (N), DFT_DIR_INVERSE); \
  }

#define OAI_ARM_DFT_SIZES(X)                                                                                                      \
  X(12)                                                                                                                           \
  X(16)                                                                                                                           \
  X(24)                                                                                                                           \
  X(32) X(36) X(48) X(60) X(64) X(72) X(96) X(108) X(120) X(128) X(144) X(180) X(192) X(216) X(240) X(256) X(288) X(300) X(324)   \
      X(360) X(384) X(432) X(480) X(512) X(540) X(576) X(600) X(648) X(720) X(768) X(864) X(900) X(960) X(972) X(1024) X(1080)    \
          X(1152) X(1200) X(1296) X(1440) X(1500) X(1536) X(1620) X(1728) X(1800) X(1920) X(1944) X(2048) X(2160) X(2304) X(2400) \
              X(2592) X(2700) X(2880) X(2916) X(3000) X(3072) X(3240) X(4096) X(6144) X(8192) X(12288) X(16384) X(18432) X(24576) \
                  X(32768) X(36864) X(49152) X(65536) X(98304)

#define OAI_ARM_IDFT_SIZES(X)                                                                                                     \
  X(12)                                                                                                                           \
  X(16)                                                                                                                           \
  X(24)                                                                                                                           \
  X(32) X(36) X(48) X(60) X(64) X(72) X(96) X(108) X(120) X(128) X(144) X(180) X(192) X(216) X(240) X(256) X(288) X(300) X(324)   \
      X(360) X(384) X(432) X(480) X(512) X(540) X(576) X(600) X(648) X(720) X(768) X(864) X(900) X(960) X(972) X(1024) X(1080)    \
          X(1152) X(1200) X(1296) X(1440) X(1500) X(1536) X(1620) X(1728) X(1800) X(1920) X(1944) X(2048) X(2160) X(2304) X(2400) \
              X(2592) X(2700) X(2880) X(2916) X(3000) X(3072) X(3240) X(4096) X(6144) X(8192) X(12288) X(16384) X(18432) X(24576) \
                  X(32768) X(36864) X(49152) X(65536) X(98304)

OAI_ARM_DFT_SIZES(DEFINE_MIXED_DFT)
OAI_ARM_IDFT_SIZES(DEFINE_MIXED_IDFT)

#undef DEFINE_MIXED_DFT
#undef DEFINE_MIXED_IDFT

void idft16f(int16_t *input, int16_t *output)
{
  dft_mixed_radix_c16_scaled((const c16_t *)input, (c16_t *)output, 16, DFT_DIR_INVERSE);
}

#endif /* __aarch64__ */

#if defined(__arm__) && !defined(__aarch64__)
#error "This replacement oai_dfts_neon backend requires AArch64"
#endif

/* Resolve shared ISA state; per-size plans remain lazy. */
int dfts_autoinit(void)
{
  oai_dft_ensure_initialized();
  return 0;
}

void dft_implementation(uint8_t sizeidx, int16_t *input, int16_t *output, unsigned char scale_flag)
{
  (void)scale_flag;
  AssertFatal(sizeidx < DFT_SIZE_IDXTABLESIZE, "Invalid dft size index %i\n", sizeidx);
  AssertFatal((((intptr_t)output) & 0xF) == 0, "Buffers should be 16-byte aligned %p", output);
  dft_mixed_radix_c16_scaled_slot((const c16_t *)input, (c16_t *)output, dft_ftab[sizeidx].size, sizeidx, DFT_DIR_FORWARD);
}

void idft_implementation(uint8_t sizeidx, int16_t *input, int16_t *output, unsigned char scale_flag)
{
  (void)scale_flag;
  AssertFatal(sizeidx < DFT_SIZE_IDXTABLESIZE, "Invalid idft size index %i\n", sizeidx);
  AssertFatal((((intptr_t)output) & 0xF) == 0, "Buffers should be 16-byte aligned %p", output);
  dft_mixed_radix_c16_scaled_slot((const c16_t *)input, (c16_t *)output, idft_ftab[sizeidx].size, sizeidx, DFT_DIR_INVERSE);
}

#endif /* __arm__ || __aarch64__ */
