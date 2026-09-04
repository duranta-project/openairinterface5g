/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#if defined(__arm__) || defined(__aarch64__)

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
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

/* -------------------------------------------------------------------------
 * Native AArch64 Q15 DFT/IDFT backend.
 *
 * - One public dftN()/idftN() call processes N contiguous complex int16 samples.
 * - The backend uses unitary scaling; scale_flag is kept for API compatibility
 *   and for future adaptive DFT support.
 * - SVE2 is used when available with an active vector length of 128 bits.
 * - Other AArch64 systems use NEON.
 * ------------------------------------------------------------------------- */


typedef uint8x16_t neon_m128;
typedef uint8x16_t neon_m128i;

/* Eight complex Q15 values packed in two 128-bit NEON vectors. */
typedef int16x8x2_t cq15x8_t;



static inline int16x8_t neon128_as_i16(neon_m128 x) { return vreinterpretq_s16_u8(x); }

static inline int32x4_t neon128_as_i32(neon_m128 x) { return vreinterpretq_s32_u8(x); }


static inline uint64x2_t neon128_as_u64(neon_m128 x) { return vreinterpretq_u64_u8(x); }


static inline neon_m128 neon128_from_i16(int16x8_t x) { return vreinterpretq_u8_s16(x); }

static inline neon_m128 neon128_from_i32(int32x4_t x) { return vreinterpretq_u8_s32(x); }


static inline neon_m128 neon128_from_u64(uint64x2_t x) { return vreinterpretq_u8_u64(x); }


/* Native 128-bit loads/stores. */


static inline neon_m128i neon128_load_i(const neon_m128i *p) { return vld1q_u8((const uint8_t *)(const void *)p); }
static inline neon_m128i neon128_loadu_i(const neon_m128i *p) { return neon128_load_i(p); }
static inline void neon128_store_i(neon_m128i *p, neon_m128i x) { vst1q_u8((uint8_t *)(void *)p, x); }
static inline void neon128_storeu_i(neon_m128i *p, neon_m128i x) { neon128_store_i(p, x); }

/* Constructors and loads for algorithm-level Q15 batches. */
static inline cq15x8_t cq15x8_make(int16x8_t v0, int16x8_t v1) { cq15x8_t r={{v0,v1}}; return r; }
static inline cq15x8_t cq15x8_load(const cq15x8_t *p) {
  const int16_t *q=(const int16_t *)(const void *)p; return cq15x8_make(vld1q_s16(q),vld1q_s16(q+8));
}
static inline cq15x8_t cq15x8_loadu(const cq15x8_t *p) { return cq15x8_load(p); }
static inline void cq15x8_store(cq15x8_t *p, cq15x8_t x) {
  int16_t *q=(int16_t *)(void *)p; vst1q_s16(q,x.val[0]); vst1q_s16(q+8,x.val[1]);
}
static inline void cq15x8_storeu(cq15x8_t *p, cq15x8_t x) { cq15x8_store(p,x); }

static inline cq15x8_t cq15x8_rshr2_i16(cq15x8_t x) {
  return cq15x8_make(vrshrq_n_s16(x.val[0], 2), vrshrq_n_s16(x.val[1], 2));
}
static inline cq15x8_t cq15x8_set1_i16(int x) { int16x8_t q=vdupq_n_s16((int16_t)x); return cq15x8_make(q,q); }
static inline cq15x8_t cq15x8_setr_i16(int a0,int a1,int a2,int a3,int a4,int a5,int a6,int a7,
                                              int a8,int a9,int a10,int a11,int a12,int a13,int a14,int a15) {
  const int16_t lo[8]={(int16_t)a0,(int16_t)a1,(int16_t)a2,(int16_t)a3,(int16_t)a4,(int16_t)a5,(int16_t)a6,(int16_t)a7};
  const int16_t hi[8]={(int16_t)a8,(int16_t)a9,(int16_t)a10,(int16_t)a11,(int16_t)a12,(int16_t)a13,(int16_t)a14,(int16_t)a15};
  return cq15x8_make(vld1q_s16(lo),vld1q_s16(hi));
}

/* 128-bit constructors. */


static inline neon_m128i neon128_set1_i16(int x) { return neon128_from_i16(vdupq_n_s16((int16_t)x)); }

static inline neon_m128i neon128_setr_i16(int a0,int a1,int a2,int a3,int a4,int a5,int a6,int a7) {
  const int16_t v[8]={(int16_t)a0,(int16_t)a1,(int16_t)a2,(int16_t)a3,(int16_t)a4,(int16_t)a5,(int16_t)a6,(int16_t)a7}; return neon128_from_i16(vld1q_s16(v));
}


/* Native integer/Q15 arithmetic and unpack helpers. */


static inline neon_m128i neon128_adds_i16(neon_m128i a,neon_m128i b){return neon128_from_i16(vqaddq_s16(neon128_as_i16(a),neon128_as_i16(b)));}
static inline neon_m128i neon128_subs_i16(neon_m128i a,neon_m128i b){return neon128_from_i16(vqsubq_s16(neon128_as_i16(a),neon128_as_i16(b)));}
static inline neon_m128i neon128_mulhrs_i16(neon_m128i a,neon_m128i b){return neon128_from_i16(vqrdmulhq_s16(neon128_as_i16(a),neon128_as_i16(b)));}


static inline neon_m128i neon128_unpacklo_i32(neon_m128i a,neon_m128i b){return neon128_from_i32(vzip1q_s32(neon128_as_i32(a),neon128_as_i32(b)));}
static inline neon_m128i neon128_unpackhi_i32(neon_m128i a,neon_m128i b){return neon128_from_i32(vzip2q_s32(neon128_as_i32(a),neon128_as_i32(b)));}
static inline neon_m128i neon128_unpacklo_i64(neon_m128i a,neon_m128i b){return neon128_from_u64(vzip1q_u64(neon128_as_u64(a),neon128_as_u64(b)));}
static inline neon_m128i neon128_unpackhi_i64(neon_m128i a,neon_m128i b){return neon128_from_u64(vzip2q_u64(neon128_as_u64(a),neon128_as_u64(b)));}


static inline cq15x8_t cq15x8_adds_i16(cq15x8_t a,cq15x8_t b){return cq15x8_make(vqaddq_s16(a.val[0],b.val[0]),vqaddq_s16(a.val[1],b.val[1]));}
static inline cq15x8_t cq15x8_subs_i16(cq15x8_t a,cq15x8_t b){return cq15x8_make(vqsubq_s16(a.val[0],b.val[0]),vqsubq_s16(a.val[1],b.val[1]));}
static inline cq15x8_t cq15x8_mulhrs_i16(cq15x8_t a,cq15x8_t b){return cq15x8_make(vqrdmulhq_s16(a.val[0],b.val[0]),vqrdmulhq_s16(a.val[1],b.val[1]));}
static inline cq15x8_t cq15x8_srai_i16(cq15x8_t a,int n){int16x8_t sh=vdupq_n_s16((int16_t)-n);return cq15x8_make(vshlq_s16(a.val[0],sh),vshlq_s16(a.val[1],sh));}
static inline cq15x8_t cq15x8_unpacklo_i32(cq15x8_t a,cq15x8_t b){return cq15x8_make(vreinterpretq_s16_s32(vzip1q_s32(vreinterpretq_s32_s16(a.val[0]),vreinterpretq_s32_s16(b.val[0]))),vreinterpretq_s16_s32(vzip1q_s32(vreinterpretq_s32_s16(a.val[1]),vreinterpretq_s32_s16(b.val[1]))));}
static inline cq15x8_t cq15x8_unpackhi_i32(cq15x8_t a,cq15x8_t b){return cq15x8_make(vreinterpretq_s16_s32(vzip2q_s32(vreinterpretq_s32_s16(a.val[0]),vreinterpretq_s32_s16(b.val[0]))),vreinterpretq_s16_s32(vzip2q_s32(vreinterpretq_s32_s16(a.val[1]),vreinterpretq_s32_s16(b.val[1]))));}
static inline cq15x8_t cq15x8_unpacklo_i64(cq15x8_t a,cq15x8_t b){return cq15x8_make(vreinterpretq_s16_u64(vzip1q_u64(vreinterpretq_u64_s16(a.val[0]),vreinterpretq_u64_s16(b.val[0]))),vreinterpretq_s16_u64(vzip1q_u64(vreinterpretq_u64_s16(a.val[1]),vreinterpretq_u64_s16(b.val[1]))));}
static inline cq15x8_t cq15x8_unpackhi_i64(cq15x8_t a,cq15x8_t b){return cq15x8_make(vreinterpretq_s16_u64(vzip2q_u64(vreinterpretq_u64_s16(a.val[0]),vreinterpretq_u64_s16(b.val[0]))),vreinterpretq_s16_u64(vzip2q_u64(vreinterpretq_u64_s16(a.val[1]),vreinterpretq_u64_s16(b.val[1]))));}

static inline cq15x8_t cq15x8_select_halves(cq15x8_t a,cq15x8_t b,int imm){
  int16x8_t src[4]={a.val[0],a.val[1],b.val[0],b.val[1]}; int16x8_t lo=(imm&0x08)?vdupq_n_s16(0):src[imm&3]; int16x8_t hi=(imm&0x80)?vdupq_n_s16(0):src[(imm>>4)&3]; return cq15x8_make(lo,hi);
}




#ifndef Q15_INV_SQRT2
#define Q15_INV_SQRT2 ((int16_t)23170) /* round(0.70710678118 * 32768) */
#endif
#define Q15_HALF_SQRT3 ((int16_t)28378)
#define Q15_INV_SQRT8 ((int16_t)11585) /* round(32767 / sqrt(8)) */

#define Q15_HALF 16384

typedef enum { DFT_DIR_FORWARD = -1, DFT_DIR_INVERSE = 1 } dft_dir_t;

static void dft2048_radix8_q15(const c16_t *src, c16_t *dst, dft_dir_t dir);

static void dft_power2_q15(const c16_t *src, c16_t *dst, int N, dft_dir_t dir);
static inline void idft16_q15_native(const c16_t *src, c16_t *dst);



static inline int16_t sat_q15(long x)
{
  if (x > 32767)
    return 32767;
  if (x < -32767)
    return -32767;
  return (int16_t)x;
}

static inline int16_t q15_from_float(float x)
{
  return sat_q15((long)lrintf(32767.0f * x));
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

static inline neon_m128i mul_j_q15_128(neon_m128i z)
{
  const int16x8_t swapped = vrev32q_s16(neon128_as_i16(z));
  const uint16x8_t neg_re = {0xffff, 0, 0xffff, 0, 0xffff, 0, 0xffff, 0};
  return neon128_from_i16(vbslq_s16(neg_re, vqnegq_s16(swapped), swapped));
}

static inline neon_m128i mul_minus_j_q15_128(neon_m128i z)
{
  const int16x8_t swapped = vrev32q_s16(neon128_as_i16(z));
  const uint16x8_t neg_im = {0, 0xffff, 0, 0xffff, 0, 0xffff, 0, 0xffff};
  return neon128_from_i16(vbslq_s16(neg_im, vqnegq_s16(swapped), swapped));
}

static inline neon_m128i q15_mul_i16_128(neon_m128i x, int16_t q15)
{
  return neon128_mulhrs_i16(x, neon128_set1_i16(q15));
}

#define Q15_INV_SQRT3 18919 /* 1 / sqrt(3) */


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





#ifndef Q15_INV_SQRT5
#define Q15_INV_SQRT5 ((int16_t)14654)
#endif

#define SVE2_MAX_STAGES 3

#if defined(__clang__)
#define SVE2_TARGET __attribute__((target("sve2")))
#elif defined(__GNUC__)
#define SVE2_TARGET __attribute__((target("arch=armv8.5-a+sve2")))
#else
#define SVE2_TARGET
#endif

__attribute__((noinline, format(printf, 5, 6)))
static void sve2_assert_failure(const char *condition,
                                const char *file,
                                const char *function,
                                int line,
                                const char *format, ...)
{
  char detail[512];
  va_list args;
  va_start(args, format);
  vsnprintf(detail, sizeof(detail), format, args);
  va_end(args);

  AssertFatal(false,
              "SVE2 assertion (%s) failed in %s() %s:%d\n%s",
              condition, function, file, line, detail);
}

#define SVE2_ASSERT(condition, format, args...) \
  do { \
    if (!(condition)) \
      sve2_assert_failure(#condition, __FILE__, __FUNCTION__, __LINE__, format, ##args); \
  } while (0)

typedef struct {
  int radix;
  int m;
  int q;
  int16_t scale_q15;
  int16_t *tw_q15;
} sve2_stage_t;

static inline const int16_t *sve2_stage_twiddle(
    const sve2_stage_t *stage, int branch)
{
  return stage->tw_q15 + (size_t)branch * (size_t)(2 * stage->q);
}

typedef struct {
  int N;
  int stage_count;
  sve2_stage_t stage[SVE2_MAX_STAGES];
  uint32_t *perm32;
} sve2_plan_t;

typedef struct {
  int N;
  int initialized;
  sve2_plan_t plan;
} sve2_plan_slot_t;

/*
 * Generic SVE2 fallback leaf-plan slots for DFT24 and DFT60.
 * Public mixed-radix plans decompose these sizes before leaf dispatch.
 */
static sve2_plan_slot_t g_sve2_leaf_plans[] __attribute__((aligned(64))) = {
  {.N = 24},
  {.N = 60},
};
static pthread_mutex_t g_sve2_plan_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Checks SVE and SVE2 availability through Linux HWCAP. */
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

SVE2_TARGET static int sve2_vector_bits(void)
{
  return (int)svcntb() * 8;
}

static int16_t q15_unitary_scale_coeff(int16_t x, int radix)
{
  switch (radix) {
    case 2:
      return sat_q15((long)((float)x / sqrtf(2.0f)));
    case 3:
      return sat_q15((long)((float)x / sqrtf(3.0f)));
    case 4:
      return (int16_t)(x / 2);
    case 5:
      return sat_q15((long)((float)x / sqrtf(5.0f)));
    default:
      abort();
      __builtin_unreachable();
  }
}

static int16_t sve2_stage_scale_q15(int radix)
{
  switch (radix) {
    case 2: return Q15_INV_SQRT2;
    case 3: return Q15_INV_SQRT3;
    case 4: return Q15_HALF;
    case 5: return Q15_INV_SQRT5;
    default:
      abort();
      __builtin_unreachable();
  }
}

static void sve2_plan_reset(sve2_plan_t *p)
{
  for (int s = 0; s < p->stage_count; s++)
    free(p->stage[s].tw_q15);
  free(p->perm32);
  memset(p, 0, sizeof(*p));
}

static int sve2_plan_init(sve2_plan_t *p, int N)
{
  if (!p || N <= 0)
    return 0;

  memset(p, 0, sizeof(*p));
  p->N = N;

  int residual = N;
  int radices[SVE2_MAX_STAGES];
  int nr = 0;

  while ((residual % 3) == 0 && nr < SVE2_MAX_STAGES) {
    radices[nr++] = 3;
    residual /= 3;
  }
  while ((residual % 5) == 0 && nr < SVE2_MAX_STAGES) {
    radices[nr++] = 5;
    residual /= 5;
  }

  if (residual > 1) {
    if (!is_power_of_two_int(residual))
      return 0;
    const int lg = __builtin_ctz((unsigned)residual);
    if (lg & 1) {
      if (nr >= SVE2_MAX_STAGES)
        return 0;
      radices[nr++] = 2;
      residual >>= 1;
    }
    while (residual > 1) {
      if (nr >= SVE2_MAX_STAGES)
        return 0;
      radices[nr++] = 4;
      residual >>= 2;
    }
  }

  if (residual != 1 || nr <= 0 || nr > SVE2_MAX_STAGES)
    return 0;

  p->stage_count = nr;
  p->perm32 = aligned_malloc64((size_t)N * sizeof(*p->perm32));
  if (!p->perm32)
    return 0;

  for (int i = 0; i < N; i++) {
    int z = i;
    int stride = N;
    int src_index = 0;
    for (int s = 0; s < nr; s++) {
      const int r = radices[s];
      const int digit = z % r;
      z /= r;
      stride /= r;
      src_index += digit * stride;
    }
    p->perm32[i] = (uint32_t)src_index;
  }

  int m = 1;
  for (int s = 0; s < nr; s++) {
    sve2_stage_t *st = &p->stage[s];
    st->radix = radices[s];
    m *= st->radix;
    st->m = m;
    st->q = m / st->radix;
    st->scale_q15 = sve2_stage_scale_q15(st->radix);

    const size_t coeffs = (size_t)(st->radix - 1) * (size_t)(2 * st->q);
    st->tw_q15 = aligned_malloc64(coeffs * sizeof(*st->tw_q15));
    if (!st->tw_q15) {
      sve2_plan_reset(p);
      return 0;
    }

    for (int branch = 1; branch < st->radix; branch++) {
      int16_t *twiddle =
          st->tw_q15 + (size_t)(branch - 1) * (size_t)(2 * st->q);
      for (int j = 0; j < st->q; j++) {
        const float angle = -2.0f * (float)M_PI * (float)(branch * j) / (float)m;
        const float wr = cosf(angle);
        const float wi = sinf(angle);
        const int16_t wr_q15 = q15_from_float(wr);
        const int16_t wi_q15 = q15_from_float(wi);
        twiddle[2 * j + 0] = q15_unitary_scale_coeff(wr_q15, st->radix);
        twiddle[2 * j + 1] = q15_unitary_scale_coeff(wi_q15, st->radix);
      }
    }
  }

  return 1;
}

__attribute__((noinline)) static const sve2_plan_t *sve2_plan_get(int N)
{
  sve2_plan_slot_t *slot = NULL;
  for (size_t i = 0; i < sizeof(g_sve2_leaf_plans) / sizeof(g_sve2_leaf_plans[0]); i++) {
    if (g_sve2_leaf_plans[i].N == N) {
      slot = &g_sve2_leaf_plans[i];
      break;
    }
  }
  if (!slot)
    return NULL;

  if (__atomic_load_n(&slot->initialized, __ATOMIC_ACQUIRE))
    return &slot->plan;

  pthread_mutex_lock(&g_sve2_plan_mutex);
  if (!slot->initialized && sve2_plan_init(&slot->plan, N))
    __atomic_store_n(&slot->initialized, 1, __ATOMIC_RELEASE);
  const sve2_plan_t *plan = slot->initialized ? &slot->plan : NULL;
  pthread_mutex_unlock(&g_sve2_plan_mutex);
  return plan;
}

static void sve2_dft64_prepare(void);
static void sve2_dft128_prepare(void);
static void sve2_dft256_prepare(void);
static void sve2_dft512_prepare(void);
static void sve2_tiny_prepare(void);


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


SVE2_TARGET static inline void sve2_dft3_q15(
    svint16_t x0, svint16_t x1, svint16_t x2,
    svint16_t *y0, svint16_t *y1, svint16_t *y2)
{
  const svint16_t sum = svqadd_s16_x(svptrue_b16(), x1, x2);
  const svint16_t diff = svqsub_s16_x(svptrue_b16(), x1, x2);
  const svint16_t base = svqsub_s16_x(svptrue_b16(), x0,
      sve2_q15_real_mul(sum, Q15_HALF));
  const svint16_t imag = sve2_q15_real_mul(diff, Q15_HALF_SQRT3);
  *y0 = svqadd_s16_x(svptrue_b16(), x0, sum);
  *y1 = svqcadd_s16(base, imag, 270);
  *y2 = svqcadd_s16(base, imag, 90);
}


SVE2_TARGET static inline void sve2_dft4_q15(
    svint16_t x0, svint16_t x1, svint16_t x2, svint16_t x3,
    svint16_t *y0, svint16_t *y1, svint16_t *y2, svint16_t *y3)
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


SVE2_TARGET static inline void sve2_dft5_q15(
    svint16_t x0, svint16_t x1, svint16_t x2, svint16_t x3, svint16_t x4,
    svint16_t *y0, svint16_t *y1, svint16_t *y2, svint16_t *y3, svint16_t *y4)
{
  const svbool_t pg = svptrue_b16();
  const svint16_t t1 = svqadd_s16_x(pg, x1, x4);
  const svint16_t t2 = svqadd_s16_x(pg, x2, x3);
  const svint16_t d1 = svqsub_s16_x(pg, x1, x4);
  const svint16_t d2 = svqsub_s16_x(pg, x2, x3);
  *y0 = svqadd_s16_x(pg, x0, svqadd_s16_x(pg, t1, t2));

  const svint16_t b1 = svqadd_s16_x(pg, x0,
      svqadd_s16_x(pg,
        sve2_q15_real_mul(t1, 10126),
        sve2_q15_real_mul(t2, -26510)));
  const svint16_t q1 = svqadd_s16_x(pg,
      sve2_q15_real_mul(d1, 31163),
      sve2_q15_real_mul(d2, 19260));
  *y1 = svqcadd_s16(b1, q1, 270);
  *y4 = svqcadd_s16(b1, q1, 90);

  const svint16_t b2 = svqadd_s16_x(pg, x0,
      svqadd_s16_x(pg,
        sve2_q15_real_mul(t1, -26510),
        sve2_q15_real_mul(t2, 10126)));
  const svint16_t q2 = svqsub_s16_x(pg,
      sve2_q15_real_mul(d1, 19260),
      sve2_q15_real_mul(d2, 31163));
  *y2 = svqcadd_s16(b2, q2, 270);
  *y3 = svqcadd_s16(b2, q2, 90);
}


SVE2_TARGET static inline void sve2_idft3_q15(
    svint16_t x0, svint16_t x1, svint16_t x2,
    svint16_t *y0, svint16_t *y1, svint16_t *y2)
{
  svint16_t f0, f1, f2;
  sve2_dft3_q15(x0, x1, x2, &f0, &f1, &f2);
  *y0 = f0; *y1 = f2; *y2 = f1;
}

SVE2_TARGET static inline void sve2_idft5_q15(
    svint16_t x0, svint16_t x1, svint16_t x2, svint16_t x3, svint16_t x4,
    svint16_t *y0, svint16_t *y1, svint16_t *y2, svint16_t *y3, svint16_t *y4)
{
  svint16_t f0, f1, f2, f3, f4;
  sve2_dft5_q15(x0, x1, x2, x3, x4, &f0, &f1, &f2, &f3, &f4);
  *y0 = f0; *y1 = f4; *y2 = f3; *y3 = f2; *y4 = f1;
}

/* Executes a generic unitary Q15 SVE2 plan. Branch zero is scaled directly;
 * the other branches use twiddles with the stage scale folded in. */
SVE2_TARGET static void sve2_dft_q15(const sve2_plan_t *p,
                                                const c16_t *src,
                                                c16_t *dst)
{
  c16_t *x = dst;
  const uint32_t *src32 = (const uint32_t *)(const void *)src;
  uint32_t *x32 = (uint32_t *)(void *)x;

  for (uint64_t i = 0; i < (uint64_t)p->N; i += svcntw()) {
    const svbool_t pg = svwhilelt_b32(i, (uint64_t)p->N);
    const svuint32_t idx = svld1_u32(pg, p->perm32 + i);
    const svuint32_t v = svld1_gather_u32index_u32(pg, src32, idx);
    svst1_u32(pg, x32 + i, v);
  }

  for (int s = 0; s < p->stage_count; s++) {
    const sve2_stage_t *st = &p->stage[s];
    const int r = st->radix;
    const int m = st->m;
    const int q = st->q;
    const int cplx_per_vec = (int)svcnth() / 2;
    const svint16_t scale0 = svdup_n_s16(st->scale_q15);

    for (int base = 0; base < p->N; base += m) {
      for (int j = 0; j < q; j += cplx_per_vec) {
        const int remain = q - j;
        const svbool_t pg = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * remain));
        svint16_t v0 = svqrdmulh_s16(
            svld1_s16(pg, (const int16_t *)(const void *)(x + base + j)),
            scale0);
        svint16_t v1 = svdup_n_s16(0);
        svint16_t v2 = svdup_n_s16(0);
        svint16_t v3 = svdup_n_s16(0);
        svint16_t v4 = svdup_n_s16(0);
        svint16_t y0 = svdup_n_s16(0);
        svint16_t y1 = svdup_n_s16(0);
        svint16_t y2 = svdup_n_s16(0);
        svint16_t y3 = svdup_n_s16(0);
        svint16_t y4 = svdup_n_s16(0);

        if (r > 1) {
          const svint16_t z = svld1_s16(pg, (const int16_t *)(const void *)(x + base + q + j));
          const svint16_t w = svld1_s16(pg, sve2_stage_twiddle(st, 0) + 2 * j);
          v1 = sve2_cmul_q15(z, w);
        }
        if (r > 2) {
          const svint16_t z = svld1_s16(pg, (const int16_t *)(const void *)(x + base + 2 * q + j));
          const svint16_t w = svld1_s16(pg, sve2_stage_twiddle(st, 1) + 2 * j);
          v2 = sve2_cmul_q15(z, w);
        }
        if (r > 3) {
          const svint16_t z = svld1_s16(pg, (const int16_t *)(const void *)(x + base + 3 * q + j));
          const svint16_t w = svld1_s16(pg, sve2_stage_twiddle(st, 2) + 2 * j);
          v3 = sve2_cmul_q15(z, w);
        }
        if (r > 4) {
          const svint16_t z = svld1_s16(pg, (const int16_t *)(const void *)(x + base + 4 * q + j));
          const svint16_t w = svld1_s16(pg, sve2_stage_twiddle(st, 3) + 2 * j);
          v4 = sve2_cmul_q15(z, w);
        }

        if (r == 2) {
          y0 = svqadd_s16_x(pg, v0, v1);
          y1 = svqsub_s16_x(pg, v0, v1);
        } else if (r == 3) {
          sve2_dft3_q15(v0, v1, v2, &y0, &y1, &y2);
        } else if (r == 4) {
          sve2_dft4_q15(v0, v1, v2, v3, &y0, &y1, &y2, &y3);
        } else {
          sve2_dft5_q15(v0, v1, v2, v3, v4, &y0, &y1, &y2, &y3, &y4);
        }

        svst1_s16(pg, (int16_t *)(void *)(x + base + j), y0);
        if (r > 1) svst1_s16(pg, (int16_t *)(void *)(x + base + q + j), y1);
        if (r > 2) svst1_s16(pg, (int16_t *)(void *)(x + base + 2 * q + j), y2);
        if (r > 3) svst1_s16(pg, (int16_t *)(void *)(x + base + 3 * q + j), y3);
        if (r > 4) svst1_s16(pg, (int16_t *)(void *)(x + base + 4 * q + j), y4);
      }
    }
  }

}


typedef struct {
  int16_t c16_q15[4][4][8] __attribute__((aligned(64)));
  int16_t c64_q15[16][8] __attribute__((aligned(64)));
} sve2_dft64_twiddle_t;

static sve2_dft64_twiddle_t g_sve2_dft64_tw;

static inline int16_t sve2_dft64_q15_half_twiddle(float x)
{
  return (int16_t)(q15_from_float(x) / 2);
}

static void sve2_dft64_prepare(void)
{
  sve2_dft64_twiddle_t *tw = &g_sve2_dft64_tw;
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
}


SVE2_TARGET static inline svint16_t sve2_q15_half_shift(
    svbool_t pg, svint16_t x)
{
  /* Power-of-two 1/2 normalization without a Q15 multiply. */
  return svasr_n_s16_x(pg, x, 1);
}

SVE2_TARGET static inline svint16_t sve2_dft64_c16_q15(
    svbool_t pg, svint16_t x, int k, int r)
{
  /* k=0 has W16^(k*r)=1 for every branch, so only the radix-4
   * 1/2 normalization remains. */
  if (k == 0)
    return sve2_q15_half_shift(pg, x);

  const svint16_t w = svld1rq_s16(pg, g_sve2_dft64_tw.c16_q15[k][r]);
  return sve2_cmul_q15(x, w);
}

/* Dedicated DFT64 bin-0/DC path.
 * Accumulate the 64 complex samples in int32, then apply the complete
 * unitary 1/8 scale once. */
SVE2_TARGET static inline c16_t sve2_dft64_dc_from_src(const c16_t *src)
{
  int32x4_t acc_re = vdupq_n_s32(0);
  int32x4_t acc_im = vdupq_n_s32(0);

  for (int i = 0; i < 64; i += 8) {
    const int16x8x2_t ri = vld2q_s16((const int16_t *)(const void *)(src + i));
    acc_re = vaddq_s32(acc_re, vpaddlq_s16(ri.val[0]));
    acc_im = vaddq_s32(acc_im, vpaddlq_s16(ri.val[1]));
  }

  int32_t re = vaddvq_s32(acc_re);
  int32_t im = vaddvq_s32(acc_im);

  /* Signed rounded division by eight: positive +4 and negative +3
   * before the arithmetic shift. */
  re = (re + 4 + (re >> 31)) >> 3;
  im = (im + 4 + (im >> 31)) >> 3;

  c16_t dc;
  dc.r = (int16_t)re;
  dc.i = (int16_t)im;
  return dc;
}

SVE2_TARGET static inline void sve2_dft64_transpose4_q15_128(
    svint16_t a, svint16_t b, svint16_t c, svint16_t d,
    svint16_t *t0, svint16_t *t1, svint16_t *t2, svint16_t *t3)
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

SVE2_TARGET static inline void sve2_dft64_transpose4_q15_generic(
    svint16_t a, svint16_t b, svint16_t c, svint16_t d,
    svint16_t *t0, svint16_t *t1, svint16_t *t2, svint16_t *t3)
{
  uint32_t m[16] __attribute__((aligned(64)));
  static const uint32_t i0[4] __attribute__((aligned(16))) = {0, 4, 8, 12};
  static const uint32_t i1[4] __attribute__((aligned(16))) = {1, 5, 9, 13};
  static const uint32_t i2[4] __attribute__((aligned(16))) = {2, 6, 10, 14};
  static const uint32_t i3[4] __attribute__((aligned(16))) = {3, 7, 11, 15};
  const svbool_t pg = svwhilelt_b32((uint64_t)0, (uint64_t)4);
  svst1_u32(pg, m + 0, svreinterpret_u32_s16(a));
  svst1_u32(pg, m + 4, svreinterpret_u32_s16(b));
  svst1_u32(pg, m + 8, svreinterpret_u32_s16(c));
  svst1_u32(pg, m + 12, svreinterpret_u32_s16(d));
  const svuint32_t v0 = svld1_gather_u32index_u32(pg, m, svld1_u32(pg, i0));
  const svuint32_t v1 = svld1_gather_u32index_u32(pg, m, svld1_u32(pg, i1));
  const svuint32_t v2 = svld1_gather_u32index_u32(pg, m, svld1_u32(pg, i2));
  const svuint32_t v3 = svld1_gather_u32index_u32(pg, m, svld1_u32(pg, i3));
  *t0 = svreinterpret_s16_u32(v0);
  *t1 = svreinterpret_s16_u32(v1);
  *t2 = svreinterpret_s16_u32(v2);
  *t3 = svreinterpret_s16_u32(v3);
}

/* Runs the unitary Q15 DFT64 as three fixed-topology radix-4 SVE2 stages. */
SVE2_TARGET static void sve2_dft64_q15_leaf(const c16_t *src, c16_t *dst)
{
  const svbool_t pg = svwhilelt_b16((uint64_t)0, (uint64_t)8);
  const c16_t dc = sve2_dft64_dc_from_src(src);
  c16_t hbuf[64] __attribute__((aligned(64)));
  c16_t gbuf[64] __attribute__((aligned(64)));

  for (int i = 0; i < 4; i++) {
    const svint16_t x0 = sve2_q15_half_shift(
        pg, svld1_s16(pg, (const int16_t *)(const void *)(src + 4 * (i + 0))));
    const svint16_t x1 = sve2_q15_half_shift(
        pg, svld1_s16(pg, (const int16_t *)(const void *)(src + 4 * (i + 4))));
    const svint16_t x2 = sve2_q15_half_shift(
        pg, svld1_s16(pg, (const int16_t *)(const void *)(src + 4 * (i + 8))));
    const svint16_t x3 = sve2_q15_half_shift(
        pg, svld1_s16(pg, (const int16_t *)(const void *)(src + 4 * (i + 12))));
    svint16_t y0, y1, y2, y3;
    sve2_dft4_q15(x0, x1, x2, x3, &y0, &y1, &y2, &y3);
    svst1_s16(pg, (int16_t *)(void *)(hbuf + 4 * (4 * i + 0)), y0);
    svst1_s16(pg, (int16_t *)(void *)(hbuf + 4 * (4 * i + 1)), y1);
    svst1_s16(pg, (int16_t *)(void *)(hbuf + 4 * (4 * i + 2)), y2);
    svst1_s16(pg, (int16_t *)(void *)(hbuf + 4 * (4 * i + 3)), y3);
  }

  for (int k = 0; k < 4; k++) {
    const svint16_t h0 = svld1_s16(pg, (const int16_t *)(const void *)(hbuf + 4 * (4 * 0 + k)));
    const svint16_t h1 = svld1_s16(pg, (const int16_t *)(const void *)(hbuf + 4 * (4 * 1 + k)));
    const svint16_t h2 = svld1_s16(pg, (const int16_t *)(const void *)(hbuf + 4 * (4 * 2 + k)));
    const svint16_t h3 = svld1_s16(pg, (const int16_t *)(const void *)(hbuf + 4 * (4 * 3 + k)));
    const svint16_t b0 = sve2_q15_half_shift(pg, h0);
    const svint16_t b1 = sve2_dft64_c16_q15(pg, h1, k, 1);
    const svint16_t b2 = sve2_dft64_c16_q15(pg, h2, k, 2);
    const svint16_t b3 = sve2_dft64_c16_q15(pg, h3, k, 3);
    svint16_t y0, y1, y2, y3;
    sve2_dft4_q15(b0, b1, b2, b3, &y0, &y1, &y2, &y3);
    svst1_s16(pg, (int16_t *)(void *)(gbuf + 4 * (k + 0)), y0);
    svst1_s16(pg, (int16_t *)(void *)(gbuf + 4 * (k + 4)), y1);
    svst1_s16(pg, (int16_t *)(void *)(gbuf + 4 * (k + 8)), y2);
    svst1_s16(pg, (int16_t *)(void *)(gbuf + 4 * (k + 12)), y3);
  }

  for (int i = 0; i < 16; i += 4) {
    const svint16_t w0 = svld1_s16(pg, g_sve2_dft64_tw.c64_q15[i + 0]);
    const svint16_t w1 = svld1_s16(pg, g_sve2_dft64_tw.c64_q15[i + 1]);
    const svint16_t w2 = svld1_s16(pg, g_sve2_dft64_tw.c64_q15[i + 2]);
    const svint16_t w3 = svld1_s16(pg, g_sve2_dft64_tw.c64_q15[i + 3]);
    const svint16_t g0 =
        svld1_s16(pg, (const int16_t *)(const void *)(gbuf + 4 * (i + 0)));
    /* For i=0 the whole SIMD vector sees W64^0 / 2, so this is a pure shift. */
    const svint16_t b0 = (i == 0)
        ? sve2_q15_half_shift(pg, g0)
        : sve2_cmul_q15(g0, w0);
    const svint16_t b1 = sve2_cmul_q15(
        svld1_s16(pg, (const int16_t *)(const void *)(gbuf + 4 * (i + 1))), w1);
    const svint16_t b2 = sve2_cmul_q15(
        svld1_s16(pg, (const int16_t *)(const void *)(gbuf + 4 * (i + 2))), w2);
    const svint16_t b3 = sve2_cmul_q15(
        svld1_s16(pg, (const int16_t *)(const void *)(gbuf + 4 * (i + 3))), w3);

    svint16_t t0, t1, t2, t3;
    if (svcntw() == 4)
      sve2_dft64_transpose4_q15_128(b0, b1, b2, b3, &t0, &t1, &t2, &t3);
    else
      sve2_dft64_transpose4_q15_generic(b0, b1, b2, b3, &t0, &t1, &t2, &t3);

    svint16_t y0, y1, y2, y3;
    sve2_dft4_q15(t0, t1, t2, t3, &y0, &y1, &y2, &y3);
    svst1_s16(pg, (int16_t *)(void *)(dst + i), y0);
    svst1_s16(pg, (int16_t *)(void *)(dst + 16 + i), y1);
    svst1_s16(pg, (int16_t *)(void *)(dst + 32 + i), y2);
    svst1_s16(pg, (int16_t *)(void *)(dst + 48 + i), y3);
  }

  /* Bin zero uses the dedicated DC result; all other bins come from the
   * SIMD DFT64 path. */
  dst[0] = dc;
}



typedef struct {
  int16_t w128_q15[16][8] __attribute__((aligned(64)));
} sve2_dft128_twiddle_t;

static sve2_dft128_twiddle_t g_sve2_dft128_tw;

static inline int16_t sve2_dft128_q15_twiddle_scaled(float x)
{
  const int16_t q = q15_from_float(x);
  return (int16_t)((float)q / sqrtf(2.0f));
}

static void sve2_dft128_prepare(void)
{
  sve2_dft128_twiddle_t *tw = &g_sve2_dft128_tw;
for (int b = 0; b < 16; b++) {
    for (int j = 0; j < 4; j++) {
      const int k = 4 * b + j;
      const float a = -2.0f * (float)M_PI * (float)k / 128.0f;
      tw->w128_q15[b][2 * j + 0] = sve2_dft128_q15_twiddle_scaled(cosf(a));
      tw->w128_q15[b][2 * j + 1] = sve2_dft128_q15_twiddle_scaled(sinf(a));
    }
  }
}

/* Finishes a VL=128 Q15 DFT64 after its first radix-4 stage has already been computed. */
SVE2_TARGET static void sve2_dft64_q15_finish_from_hbuf_128(
    const c16_t *hbuf, c16_t *dst)
{
  const svbool_t pg = svwhilelt_b16((uint64_t)0, (uint64_t)8);
  c16_t gbuf[64] __attribute__((aligned(64)));

  for (int k = 0; k < 4; k++) {
    const svint16_t h0 = svld1_s16(pg, (const int16_t *)(const void *)(hbuf + 4 * (4 * 0 + k)));
    const svint16_t h1 = svld1_s16(pg, (const int16_t *)(const void *)(hbuf + 4 * (4 * 1 + k)));
    const svint16_t h2 = svld1_s16(pg, (const int16_t *)(const void *)(hbuf + 4 * (4 * 2 + k)));
    const svint16_t h3 = svld1_s16(pg, (const int16_t *)(const void *)(hbuf + 4 * (4 * 3 + k)));
    const svint16_t b0 = sve2_q15_real_mul(h0, Q15_HALF);
    const svint16_t b1 = sve2_dft64_c16_q15(pg, h1, k, 1);
    const svint16_t b2 = sve2_dft64_c16_q15(pg, h2, k, 2);
    const svint16_t b3 = sve2_dft64_c16_q15(pg, h3, k, 3);
    svint16_t y0, y1, y2, y3;
    sve2_dft4_q15(b0, b1, b2, b3, &y0, &y1, &y2, &y3);
    svst1_s16(pg, (int16_t *)(void *)(gbuf + 4 * (k + 0)), y0);
    svst1_s16(pg, (int16_t *)(void *)(gbuf + 4 * (k + 4)), y1);
    svst1_s16(pg, (int16_t *)(void *)(gbuf + 4 * (k + 8)), y2);
    svst1_s16(pg, (int16_t *)(void *)(gbuf + 4 * (k + 12)), y3);
  }

  for (int i = 0; i < 16; i += 4) {
    const svint16_t b0 = sve2_cmul_q15(
        svld1_s16(pg, (const int16_t *)(const void *)(gbuf + 4 * (i + 0))),
        svld1_s16(pg, g_sve2_dft64_tw.c64_q15[i + 0]));
    const svint16_t b1 = sve2_cmul_q15(
        svld1_s16(pg, (const int16_t *)(const void *)(gbuf + 4 * (i + 1))),
        svld1_s16(pg, g_sve2_dft64_tw.c64_q15[i + 1]));
    const svint16_t b2 = sve2_cmul_q15(
        svld1_s16(pg, (const int16_t *)(const void *)(gbuf + 4 * (i + 2))),
        svld1_s16(pg, g_sve2_dft64_tw.c64_q15[i + 2]));
    const svint16_t b3 = sve2_cmul_q15(
        svld1_s16(pg, (const int16_t *)(const void *)(gbuf + 4 * (i + 3))),
        svld1_s16(pg, g_sve2_dft64_tw.c64_q15[i + 3]));
    svint16_t t0, t1, t2, t3;
    sve2_dft64_transpose4_q15_128(b0, b1, b2, b3, &t0, &t1, &t2, &t3);
    svint16_t y0, y1, y2, y3;
    sve2_dft4_q15(t0, t1, t2, t3, &y0, &y1, &y2, &y3);
    svst1_s16(pg, (int16_t *)(void *)(dst + i), y0);
    svst1_s16(pg, (int16_t *)(void *)(dst + 16 + i), y1);
    svst1_s16(pg, (int16_t *)(void *)(dst + 32 + i), y2);
    svst1_s16(pg, (int16_t *)(void *)(dst + 48 + i), y3);
  }
}

/* Runs DFT128 by fusing the radix-2 preparation with both DFT64 first stages. */
SVE2_TARGET static void sve2_dft128_q15_fused_st2(
    const c16_t *src, c16_t *dst)
{
  SVE2_ASSERT(svcntb() == 16, "DFT128 SVE2 requires VL=128\n");

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

    const svint16_t x00 = svld1_s16(pg, (const int16_t *)(const void *)(src + 4 * b0));
    const svint16_t x01 = svld1_s16(pg, (const int16_t *)(const void *)(src + 64 + 4 * b0));
    const svint16_t x10 = svld1_s16(pg, (const int16_t *)(const void *)(src + 4 * b1));
    const svint16_t x11 = svld1_s16(pg, (const int16_t *)(const void *)(src + 64 + 4 * b1));
    const svint16_t x20 = svld1_s16(pg, (const int16_t *)(const void *)(src + 4 * b2));
    const svint16_t x21 = svld1_s16(pg, (const int16_t *)(const void *)(src + 64 + 4 * b2));
    const svint16_t x30 = svld1_s16(pg, (const int16_t *)(const void *)(src + 4 * b3));
    const svint16_t x31 = svld1_s16(pg, (const int16_t *)(const void *)(src + 64 + 4 * b3));

    const svint16_t e0 = sve2_q15_real_mul(svqadd_s16_x(pg, x00, x01), Q15_INV_SQRT2);
    const svint16_t e1 = sve2_q15_real_mul(svqadd_s16_x(pg, x10, x11), Q15_INV_SQRT2);
    const svint16_t e2 = sve2_q15_real_mul(svqadd_s16_x(pg, x20, x21), Q15_INV_SQRT2);
    const svint16_t e3 = sve2_q15_real_mul(svqadd_s16_x(pg, x30, x31), Q15_INV_SQRT2);

    const svint16_t o0 = sve2_cmul_q15(
        svqsub_s16_x(pg, x00, x01),
        svld1_s16(pg, g_sve2_dft128_tw.w128_q15[b0]));
    const svint16_t o1 = sve2_cmul_q15(
        svqsub_s16_x(pg, x10, x11),
        svld1_s16(pg, g_sve2_dft128_tw.w128_q15[b1]));
    const svint16_t o2 = sve2_cmul_q15(
        svqsub_s16_x(pg, x20, x21),
        svld1_s16(pg, g_sve2_dft128_tw.w128_q15[b2]));
    const svint16_t o3 = sve2_cmul_q15(
        svqsub_s16_x(pg, x30, x31),
        svld1_s16(pg, g_sve2_dft128_tw.w128_q15[b3]));

    const svint16_t ce0 = sve2_q15_real_mul(e0, Q15_HALF);
    const svint16_t ce1 = sve2_q15_real_mul(e1, Q15_HALF);
    const svint16_t ce2 = sve2_q15_real_mul(e2, Q15_HALF);
    const svint16_t ce3 = sve2_q15_real_mul(e3, Q15_HALF);
    const svint16_t co0 = sve2_q15_real_mul(o0, Q15_HALF);
    const svint16_t co1 = sve2_q15_real_mul(o1, Q15_HALF);
    const svint16_t co2 = sve2_q15_real_mul(o2, Q15_HALF);
    const svint16_t co3 = sve2_q15_real_mul(o3, Q15_HALF);

    svint16_t ey0, ey1, ey2, ey3;
    svint16_t oy0, oy1, oy2, oy3;
    sve2_dft4_q15(ce0, ce1, ce2, ce3, &ey0, &ey1, &ey2, &ey3);
    sve2_dft4_q15(co0, co1, co2, co3, &oy0, &oy1, &oy2, &oy3);

    svst1_s16(pg, (int16_t *)(void *)(h_even + 4 * (4 * i + 0)), ey0);
    svst1_s16(pg, (int16_t *)(void *)(h_even + 4 * (4 * i + 1)), ey1);
    svst1_s16(pg, (int16_t *)(void *)(h_even + 4 * (4 * i + 2)), ey2);
    svst1_s16(pg, (int16_t *)(void *)(h_even + 4 * (4 * i + 3)), ey3);
    svst1_s16(pg, (int16_t *)(void *)(h_odd + 4 * (4 * i + 0)), oy0);
    svst1_s16(pg, (int16_t *)(void *)(h_odd + 4 * (4 * i + 1)), oy1);
    svst1_s16(pg, (int16_t *)(void *)(h_odd + 4 * (4 * i + 2)), oy2);
    svst1_s16(pg, (int16_t *)(void *)(h_odd + 4 * (4 * i + 3)), oy3);
  }

  sve2_dft64_q15_finish_from_hbuf_128(h_even, a);
  sve2_dft64_q15_finish_from_hbuf_128(h_odd, b);

  for (int k = 0; k < 64; k += 4) {
    const svuint32_t va = svreinterpret_u32_s16(
        svld1_s16(pg, (const int16_t *)(const void *)(a + k)));
    const svuint32_t vb = svreinterpret_u32_s16(
        svld1_s16(pg, (const int16_t *)(const void *)(b + k)));
    svst2_u32(pg32, (uint32_t *)(void *)(dst + 2 * k), svcreate2_u32(va, vb));
  }
}

typedef struct {
  int16_t w256_q15[3][16][8] __attribute__((aligned(64)));
} sve2_dft256_twiddle_t;

static sve2_dft256_twiddle_t g_sve2_dft256_tw;

static inline int16_t sve2_dft256_q15_twiddle_scaled(float x)
{
  const int16_t q = q15_from_float(x);
  return (int16_t)(q / 2);
}

static void sve2_dft256_prepare(void)
{
  sve2_dft256_twiddle_t *tw = &g_sve2_dft256_tw;
for (int branch = 1; branch <= 3; branch++) {
    for (int blk = 0; blk < 16; blk++) {
      for (int j = 0; j < 4; j++) {
        const int k = 4 * blk + j;
        const float a = -2.0f * (float)M_PI * (float)(branch * k) / 256.0f;
        tw->w256_q15[branch - 1][blk][2 * j + 0] =
            sve2_dft256_q15_twiddle_scaled(cosf(a));
        tw->w256_q15[branch - 1][blk][2 * j + 1] =
            sve2_dft256_q15_twiddle_scaled(sinf(a));
      }
    }
  }
}

/* Runs the unitary Q15 DFT256 as a radix-4 front end followed by four DFT64 leaf kernels. */
SVE2_TARGET static void sve2_dft256_q15_r4x64(
    const c16_t *src, c16_t *dst)
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
    const svint16_t x0 = svld1_s16(pg, (const int16_t *)(const void *)(src + off));
    const svint16_t x1 = svld1_s16(pg, (const int16_t *)(const void *)(src + 64 + off));
    const svint16_t x2 = svld1_s16(pg, (const int16_t *)(const void *)(src + 128 + off));
    const svint16_t x3 = svld1_s16(pg, (const int16_t *)(const void *)(src + 192 + off));

    svint16_t r0, r1, r2, r3;
    sve2_dft4_q15(x0, x1, x2, x3, &r0, &r1, &r2, &r3);
    r0 = sve2_q15_real_mul(r0, Q15_HALF);
    r1 = sve2_cmul_q15(r1, svld1_s16(pg, g_sve2_dft256_tw.w256_q15[0][blk]));
    r2 = sve2_cmul_q15(r2, svld1_s16(pg, g_sve2_dft256_tw.w256_q15[1][blk]));
    r3 = sve2_cmul_q15(r3, svld1_s16(pg, g_sve2_dft256_tw.w256_q15[2][blk]));
    svst1_s16(pg, (int16_t *)(void *)(b0 + off), r0);
    svst1_s16(pg, (int16_t *)(void *)(b1 + off), r1);
    svst1_s16(pg, (int16_t *)(void *)(b2 + off), r2);
    svst1_s16(pg, (int16_t *)(void *)(b3 + off), r3);
  }

  sve2_dft64_q15_leaf(b0, y0);
  sve2_dft64_q15_leaf(b1, y1);
  sve2_dft64_q15_leaf(b2, y2);
  sve2_dft64_q15_leaf(b3, y3);

  for (int k = 0; k < 64; k += 4) {
    const svuint32_t v0 = svreinterpret_u32_s16(
        svld1_s16(pg, (const int16_t *)(const void *)(y0 + k)));
    const svuint32_t v1 = svreinterpret_u32_s16(
        svld1_s16(pg, (const int16_t *)(const void *)(y1 + k)));
    const svuint32_t v2 = svreinterpret_u32_s16(
        svld1_s16(pg, (const int16_t *)(const void *)(y2 + k)));
    const svuint32_t v3 = svreinterpret_u32_s16(
        svld1_s16(pg, (const int16_t *)(const void *)(y3 + k)));
    svst4_u32(pg32, (uint32_t *)(void *)(dst + 4 * k), svcreate4_u32(v0, v1, v2, v3));
  }
}

/* VL=128 SVE2 helpers for final lane transposition and interleaving. */
SVE2_TARGET static inline void sve2_transpose4x4_u32(
    svuint32_t v0, svuint32_t v1, svuint32_t v2, svuint32_t v3,
    svuint32_t *o0, svuint32_t *o1, svuint32_t *o2, svuint32_t *o3)
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

SVE2_TARGET static inline void sve2_store_interleave8_q15(
    const c16_t *y0, const c16_t *y1, const c16_t *y2, const c16_t *y3,
    const c16_t *y4, const c16_t *y5, const c16_t *y6, const c16_t *y7,
    c16_t *dst, int M)
{
  const svbool_t pg16 = svptrue_b16();
  const svbool_t pg32 = svptrue_b32();
  for (int k = 0; k < M; k += 4) {
    const svuint32_t v0 = svreinterpret_u32_s16(svld1_s16(pg16, (const int16_t *)(const void *)(y0 + k)));
    const svuint32_t v1 = svreinterpret_u32_s16(svld1_s16(pg16, (const int16_t *)(const void *)(y1 + k)));
    const svuint32_t v2 = svreinterpret_u32_s16(svld1_s16(pg16, (const int16_t *)(const void *)(y2 + k)));
    const svuint32_t v3 = svreinterpret_u32_s16(svld1_s16(pg16, (const int16_t *)(const void *)(y3 + k)));
    const svuint32_t v4 = svreinterpret_u32_s16(svld1_s16(pg16, (const int16_t *)(const void *)(y4 + k)));
    const svuint32_t v5 = svreinterpret_u32_s16(svld1_s16(pg16, (const int16_t *)(const void *)(y5 + k)));
    const svuint32_t v6 = svreinterpret_u32_s16(svld1_s16(pg16, (const int16_t *)(const void *)(y6 + k)));
    const svuint32_t v7 = svreinterpret_u32_s16(svld1_s16(pg16, (const int16_t *)(const void *)(y7 + k)));
    svuint32_t a0, a1, a2, a3, b0, b1, b2, b3;
    sve2_transpose4x4_u32(v0, v1, v2, v3, &a0, &a1, &a2, &a3);
    sve2_transpose4x4_u32(v4, v5, v6, v7, &b0, &b1, &b2, &b3);
    uint32_t *p = (uint32_t *)(void *)(dst + 8 * k);
    svst1_u32(pg32, p +  0, a0); svst1_u32(pg32, p +  4, b0);
    svst1_u32(pg32, p +  8, a1); svst1_u32(pg32, p + 12, b1);
    svst1_u32(pg32, p + 16, a2); svst1_u32(pg32, p + 20, b2);
    svst1_u32(pg32, p + 24, a3); svst1_u32(pg32, p + 28, b3);
  }
}


/* ------------------------------------------------------------------------- */
/* DFT512 SVE2 radix-8 twiddle table.                                        */
/* ------------------------------------------------------------------------- */

typedef struct {
  int16_t r8_q15[7][16][8] __attribute__((aligned(64)));
} sve2_dft512_twiddle_t;

static sve2_dft512_twiddle_t g_sve2_dft512_tw;

static inline int16_t sve2_dft512_q15_twiddle_scaled(float x)
{
  const int16_t q = q15_from_float(x);
  return sat_q15((long)((float)q / sqrtf(8.0f)));
}

static void sve2_dft512_prepare(void)
{
  sve2_dft512_twiddle_t *tw = &g_sve2_dft512_tw;
  for (int branch = 1; branch <= 7; branch++) {
    for (int blk = 0; blk < 16; blk++) {
      for (int j = 0; j < 4; j++) {
        const int k = 4 * blk + j;
        const float a = -2.0f * (float)M_PI *
                        (float)(branch * k) / 512.0f;
        tw->r8_q15[branch - 1][blk][2 * j + 0] =
            sve2_dft512_q15_twiddle_scaled(cosf(a));
        tw->r8_q15[branch - 1][blk][2 * j + 1] =
            sve2_dft512_q15_twiddle_scaled(sinf(a));
      }
    }
  }
}

/* Fixed W8 constants used only inside the radix-8 front-end butterfly. */
static const int16_t g_sve2_w8_1_q15[8] __attribute__((aligned(16))) = {
   23170, -23170, 23170, -23170, 23170, -23170, 23170, -23170
};
static const int16_t g_sve2_w8_3_q15[8] __attribute__((aligned(16))) = {
  -23170, -23170, -23170, -23170, -23170, -23170, -23170, -23170
};


SVE2_TARGET static inline void sve2_dft8_q15(
    svint16_t x0, svint16_t x1, svint16_t x2, svint16_t x3,
    svint16_t x4, svint16_t x5, svint16_t x6, svint16_t x7,
    svint16_t *y0, svint16_t *y1, svint16_t *y2, svint16_t *y3,
    svint16_t *y4, svint16_t *y5, svint16_t *y6, svint16_t *y7)
{
  const svbool_t pg = svptrue_b16();
  svint16_t e0, e1, e2, e3;
  svint16_t o0, o1, o2, o3;
  sve2_dft4_q15(x0, x2, x4, x6, &e0, &e1, &e2, &e3);
  sve2_dft4_q15(x1, x3, x5, x7, &o0, &o1, &o2, &o3);

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


/* ------------------------------------------------------------------------- */
/* Tiny fixed-VL128 SVE2 leaf kernels: 4, 8, 12, 16 and 32.                 */
/* Dedicated leaf layouts use fixed permutations and stage sequencing.       */
/* ------------------------------------------------------------------------- */

typedef struct {
  int16_t w8_q15[8] __attribute__((aligned(64)));
  int16_t w16_q15[3][8] __attribute__((aligned(64)));
  int16_t w32_q15[7][8] __attribute__((aligned(64)));
} sve2_tiny_twiddle_t;

static sve2_tiny_twiddle_t g_sve2_tiny_tw;
static int16_t g_dft12_scaled_tw_q15[2][8] __attribute__((aligned(64)));

static void init_dft12_scaled_twiddles(void)
{
  for (int branch = 1; branch < 3; branch++) {
    for (int k = 0; k < 4; k++) {
      const float a = -2.0f * (float)M_PI * (float)(branch * k) / 12.0f;
      g_dft12_scaled_tw_q15[branch - 1][2 * k + 0] =
          q15_unitary_scale_coeff(q15_from_float(cosf(a)), 3);
      g_dft12_scaled_tw_q15[branch - 1][2 * k + 1] =
          q15_unitary_scale_coeff(q15_from_float(sinf(a)), 3);
    }
  }
}

static void sve2_tiny_fill_tw_q15(int16_t *wq,
                                       int N, int branch, int M)
{
  for (int k = 0; k < M; ++k) {
    const float a = -2.0f * (float)M_PI * (float)(branch * k) / (float)N;
    wq[2*k+0] = q15_from_float(cosf(a));
    wq[2*k+1] = q15_from_float(sinf(a));
  }
}

static void sve2_tiny_prepare(void)
{
  sve2_tiny_twiddle_t *tw = &g_sve2_tiny_tw;
sve2_tiny_fill_tw_q15(tw->w8_q15, 8, 1, 4);
  for (int b=1;b<4;b++) sve2_tiny_fill_tw_q15(tw->w16_q15[b-1], 16, b, 4);
  for (int b=1;b<8;b++) sve2_tiny_fill_tw_q15(tw->w32_q15[b-1], 32, b, 4);
}

static const uint16_t g_sve2_tiny_q15_rot2[8] __attribute__((aligned(16))) = {4,5,6,7,0,1,2,3};
static const uint16_t g_sve2_tiny_q15_swap[8] __attribute__((aligned(16))) = {2,3,0,1,6,7,4,5};
static const uint16_t g_sve2_tiny_q15_take_ac[8] __attribute__((aligned(16))) = {0,1,8,9,0,1,8,9};
static const uint16_t g_sve2_tiny_q15_final[8] __attribute__((aligned(16))) = {0,1,2,3,8,9,10,11};

/* Q15 scale and gain constants used by fixed-topology SVE2 leaves. */
#define SVE2_Q15_ONE_FOURTH ((int16_t)8192)
#define SVE2_Q15_ONE_EIGHTH ((int16_t)4096)
#define SVE2_GAIN_SQRT2_Q14 ((int32_t)23170)
#define SVE2_GAIN_SQRT3_Q14 ((int32_t)28378)
#define SVE2_GAIN_2_Q14     ((int32_t)32768)
#define SVE2_GAIN_SQRT5_Q14 ((int32_t)36636)
#define SVE2_GAIN_SQRT8_Q14 ((int32_t)46341)

#define SVE2_Q15_SQRT5_OVER4 ((int16_t)18318) /* sqrt(5)/4 in Q15 */

SVE2_TARGET static inline svint16_t sve2_q15_gain_q14(
    svint16_t v, int32_t gain_q14)
{
  /* Final gain stays in s16 lanes. Each case uses a Q15 multiplier <= 1
   * followed by saturating doublings. */
  const svbool_t pg = svptrue_b16();
  svint16_t r;
  if (gain_q14 == SVE2_GAIN_SQRT2_Q14) {
    /* sqrt(2) = 2 * (1/sqrt(2)). */
    r = sve2_q15_real_mul(v, Q15_INV_SQRT2);
    return svqadd_s16_x(pg, r, r);
  }
  if (gain_q14 == SVE2_GAIN_SQRT3_Q14) {
    /* sqrt(3) = 2 * (sqrt(3)/2). */
    r = sve2_q15_real_mul(v, Q15_HALF_SQRT3);
    return svqadd_s16_x(pg, r, r);
  }
  if (gain_q14 == SVE2_GAIN_2_Q14)
    return svqadd_s16_x(pg, v, v);
  if (gain_q14 == SVE2_GAIN_SQRT5_Q14) {
    /* sqrt(5) = 4 * (sqrt(5)/4). */
    r = sve2_q15_real_mul(v, SVE2_Q15_SQRT5_OVER4);
    r = svqadd_s16_x(pg, r, r);
    return svqadd_s16_x(pg, r, r);
  }
  if (gain_q14 == SVE2_GAIN_SQRT8_Q14) {
    /* sqrt(8) = 4 / sqrt(2). */
    r = sve2_q15_real_mul(v, Q15_INV_SQRT2);
    r = svqadd_s16_x(pg, r, r);
    return svqadd_s16_x(pg, r, r);
  }
  return v;
}

/* Packed unitary DFT4 using a rounded right shift for the 1/2 scaling.
 * With VL=128, one SVE register holds four complex Q15 samples. */
SVE2_TARGET static inline svint16_t sve2_dft4_q15_vec_rshift(svint16_t vin)
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

SVE2_TARGET static inline void sve2_dft4_q15_rshift(const c16_t *src, c16_t *dst)
{
  const svbool_t pg = svptrue_b16();
  const svint16_t vin = svld1_s16(pg, (const int16_t *)(const void *)src);
  svst1_s16(pg, (int16_t *)(void *)dst, sve2_dft4_q15_vec_rshift(vin));
}


/* Packed unitary DFT4 using Q15 multiplication by 1/2 and Q15 rounding. */

SVE2_TARGET static inline void sve2_dft4_q15_mulhalf(const c16_t *src, c16_t *dst)
{
  const svbool_t pg = svptrue_b16();
  const svuint16_t irot = svld1_u16(pg, g_sve2_tiny_q15_rot2);
  const svuint16_t iswp = svld1_u16(pg, g_sve2_tiny_q15_swap);
  const svuint16_t itake = svld1_u16(pg, g_sve2_tiny_q15_take_ac);
  const svuint16_t ifin = svld1_u16(pg, g_sve2_tiny_q15_final);
  const svint16_t v = sve2_q15_real_mul(
      svld1_s16(pg, (const int16_t *)(const void *)src), Q15_HALF);
  const svint16_t vr = svtbl_s16(v, irot);
  const svint16_t s = svqadd_s16_x(pg, v, vr);
  const svint16_t d = svqsub_s16_x(pg, v, vr);
  const svint16_t ss = svtbl_s16(s, iswp);
  const svint16_t ds = svtbl_s16(d, iswp);
  const svint16_t a = svqadd_s16_x(pg, s, ss);
  const svint16_t b = svqsub_s16_x(pg, s, ss);
  const svint16_t c = svqcadd_s16(d, ds, 270);
  const svint16_t e = svqcadd_s16(d, ds, 90);
  const svint16_t ac = svtbl2_s16(svcreate2_s16(a, c), itake);
  const svint16_t be = svtbl2_s16(svcreate2_s16(b, e), itake);
  const svint16_t y = svtbl2_s16(svcreate2_s16(ac, be), ifin);
  svst1_s16(pg, (int16_t *)(void *)dst, y);
}


SVE2_TARGET static void sve2_dft8_q15_leaf(const c16_t *src, c16_t *dst)
{
  const svbool_t pg16=svptrue_b16(), pg32=svptrue_b32();
  c16_t b0[4] __attribute__((aligned(16))), b1[4] __attribute__((aligned(16)));
  c16_t y0[4] __attribute__((aligned(16))), y1[4] __attribute__((aligned(16)));
  svint16_t x0=svld1_s16(pg16,(const int16_t *)(const void *)(src+0));
  svint16_t x1=svld1_s16(pg16,(const int16_t *)(const void *)(src+4));
  x0=sve2_q15_real_mul(x0,Q15_HALF);
  x1=sve2_q15_real_mul(x1,Q15_HALF);
  const svint16_t sum=svqadd_s16_x(pg16,x0,x1);
  const svint16_t dif=svqsub_s16_x(pg16,x0,x1);
  svst1_s16(pg16,(int16_t *)(void *)b0,sum);
  svst1_s16(pg16,(int16_t *)(void *)b1,
      sve2_cmul_q15(dif,svld1_s16(pg16,g_sve2_tiny_tw.w8_q15)));
  sve2_dft4_q15_mulhalf(b0,y0);
  sve2_dft4_q15_mulhalf(b1,y1);
  svint16_t vy0=sve2_q15_gain_q14(
      svld1_s16(pg16,(const int16_t *)(const void *)y0),SVE2_GAIN_SQRT2_Q14);
  svint16_t vy1=sve2_q15_gain_q14(
      svld1_s16(pg16,(const int16_t *)(const void *)y1),SVE2_GAIN_SQRT2_Q14);
  svst2_u32(pg32,(uint32_t *)(void *)dst,
      svcreate2_u32(svreinterpret_u32_s16(vy0),svreinterpret_u32_s16(vy1)));
}


SVE2_TARGET static void sve2_dft12_q15_leaf(const c16_t *src,c16_t *dst)
{
  const svbool_t pg16=svptrue_b16(),pg32=svptrue_b32();
  /* R3 x DFT4: branch zero uses 1/sqrt(3), branches 1 and 2 use
   * W12/sqrt(3), followed by a unitary DFT4. */
  const svint16_t x0=svld1_s16(pg16,(const int16_t *)(const void *)(src+0));
  const svint16_t x1=svld1_s16(pg16,(const int16_t *)(const void *)(src+4));
  const svint16_t x2=svld1_s16(pg16,(const int16_t *)(const void *)(src+8));
  svint16_t z0,z1,z2;
  sve2_dft3_q15(x0,x1,x2,&z0,&z1,&z2);
  z0=sve2_q15_real_mul(z0,Q15_INV_SQRT3);
  z1=sve2_cmul_q15(z1,svld1_s16(pg16,g_dft12_scaled_tw_q15[0]));
  z2=sve2_cmul_q15(z2,svld1_s16(pg16,g_dft12_scaled_tw_q15[1]));
  const svint16_t y0=sve2_dft4_q15_vec_rshift(z0);
  const svint16_t y1=sve2_dft4_q15_vec_rshift(z1);
  const svint16_t y2=sve2_dft4_q15_vec_rshift(z2);
  svst3_u32(pg32,(uint32_t *)(void *)dst,
      svcreate3_u32(svreinterpret_u32_s16(y0),svreinterpret_u32_s16(y1),svreinterpret_u32_s16(y2)));
}


SVE2_TARGET static void sve2_dft16_q15_leaf(const c16_t *src,c16_t *dst)
{
  const svbool_t pg16=svptrue_b16(),pg32=svptrue_b32();
  c16_t b[4][4] __attribute__((aligned(16))),y[4][4] __attribute__((aligned(16)));
  svint16_t x0=sve2_q15_real_mul(svld1_s16(pg16,(const int16_t *)(const void *)(src+0)),SVE2_Q15_ONE_FOURTH);
  svint16_t x1=sve2_q15_real_mul(svld1_s16(pg16,(const int16_t *)(const void *)(src+4)),SVE2_Q15_ONE_FOURTH);
  svint16_t x2=sve2_q15_real_mul(svld1_s16(pg16,(const int16_t *)(const void *)(src+8)),SVE2_Q15_ONE_FOURTH);
  svint16_t x3=sve2_q15_real_mul(svld1_s16(pg16,(const int16_t *)(const void *)(src+12)),SVE2_Q15_ONE_FOURTH);
  svint16_t z0,z1,z2,z3;
  sve2_dft4_q15(x0,x1,x2,x3,&z0,&z1,&z2,&z3);
  z1=sve2_cmul_q15(z1,svld1_s16(pg16,g_sve2_tiny_tw.w16_q15[0]));
  z2=sve2_cmul_q15(z2,svld1_s16(pg16,g_sve2_tiny_tw.w16_q15[1]));
  z3=sve2_cmul_q15(z3,svld1_s16(pg16,g_sve2_tiny_tw.w16_q15[2]));
  svst1_s16(pg16,(int16_t *)(void *)b[0],z0);svst1_s16(pg16,(int16_t *)(void *)b[1],z1);
  svst1_s16(pg16,(int16_t *)(void *)b[2],z2);svst1_s16(pg16,(int16_t *)(void *)b[3],z3);
  for(int r=0;r<4;r++)sve2_dft4_q15_mulhalf(b[r],y[r]);
  svint16_t v0=sve2_q15_gain_q14(svld1_s16(pg16,(const int16_t *)(const void *)y[0]),SVE2_GAIN_2_Q14);
  svint16_t v1=sve2_q15_gain_q14(svld1_s16(pg16,(const int16_t *)(const void *)y[1]),SVE2_GAIN_2_Q14);
  svint16_t v2=sve2_q15_gain_q14(svld1_s16(pg16,(const int16_t *)(const void *)y[2]),SVE2_GAIN_2_Q14);
  svint16_t v3=sve2_q15_gain_q14(svld1_s16(pg16,(const int16_t *)(const void *)y[3]),SVE2_GAIN_2_Q14);
  svst4_u32(pg32,(uint32_t *)(void *)dst,
      svcreate4_u32(svreinterpret_u32_s16(v0),svreinterpret_u32_s16(v1),
                    svreinterpret_u32_s16(v2),svreinterpret_u32_s16(v3)));
}


SVE2_TARGET static void sve2_dft32_q15_leaf(const c16_t *src,c16_t *dst)
{
  const svbool_t pg16=svptrue_b16();
  c16_t b[8][4] __attribute__((aligned(16))),y[8][4] __attribute__((aligned(16)));
  svint16_t x0=sve2_q15_real_mul(svld1_s16(pg16,(const int16_t *)(const void *)(src+0)),SVE2_Q15_ONE_EIGHTH);
  svint16_t x1=sve2_q15_real_mul(svld1_s16(pg16,(const int16_t *)(const void *)(src+4)),SVE2_Q15_ONE_EIGHTH);
  svint16_t x2=sve2_q15_real_mul(svld1_s16(pg16,(const int16_t *)(const void *)(src+8)),SVE2_Q15_ONE_EIGHTH);
  svint16_t x3=sve2_q15_real_mul(svld1_s16(pg16,(const int16_t *)(const void *)(src+12)),SVE2_Q15_ONE_EIGHTH);
  svint16_t x4=sve2_q15_real_mul(svld1_s16(pg16,(const int16_t *)(const void *)(src+16)),SVE2_Q15_ONE_EIGHTH);
  svint16_t x5=sve2_q15_real_mul(svld1_s16(pg16,(const int16_t *)(const void *)(src+20)),SVE2_Q15_ONE_EIGHTH);
  svint16_t x6=sve2_q15_real_mul(svld1_s16(pg16,(const int16_t *)(const void *)(src+24)),SVE2_Q15_ONE_EIGHTH);
  svint16_t x7=sve2_q15_real_mul(svld1_s16(pg16,(const int16_t *)(const void *)(src+28)),SVE2_Q15_ONE_EIGHTH);
  svint16_t z0,z1,z2,z3,z4,z5,z6,z7;
  sve2_dft8_q15(x0,x1,x2,x3,x4,x5,x6,x7,&z0,&z1,&z2,&z3,&z4,&z5,&z6,&z7);
#define TINY32_Q(BR,Z) do{Z=sve2_cmul_q15(Z,svld1_s16(pg16,g_sve2_tiny_tw.w32_q15[(BR)-1]));}while(0)
  TINY32_Q(1,z1);TINY32_Q(2,z2);TINY32_Q(3,z3);TINY32_Q(4,z4);TINY32_Q(5,z5);TINY32_Q(6,z6);TINY32_Q(7,z7);
#undef TINY32_Q
  svst1_s16(pg16,(int16_t *)(void *)b[0],z0);svst1_s16(pg16,(int16_t *)(void *)b[1],z1);
  svst1_s16(pg16,(int16_t *)(void *)b[2],z2);svst1_s16(pg16,(int16_t *)(void *)b[3],z3);
  svst1_s16(pg16,(int16_t *)(void *)b[4],z4);svst1_s16(pg16,(int16_t *)(void *)b[5],z5);
  svst1_s16(pg16,(int16_t *)(void *)b[6],z6);svst1_s16(pg16,(int16_t *)(void *)b[7],z7);
  for(int r=0;r<8;r++)sve2_dft4_q15_mulhalf(b[r],y[r]);
  for(int r=0;r<8;r++){
    svint16_t v=sve2_q15_gain_q14(svld1_s16(pg16,(const int16_t *)(const void *)y[r]),SVE2_GAIN_SQRT8_Q14);
    svst1_s16(pg16,(int16_t *)(void *)y[r],v);
  }
  sve2_store_interleave8_q15(y[0],y[1],y[2],y[3],y[4],y[5],y[6],y[7],dst,4);
}


/* DFT512 = radix-8 x eight DFT64 children. */
SVE2_TARGET static void sve2_dft512_q15_r8x64(
    const c16_t *src, c16_t *dst)
{
  const svbool_t pg = svptrue_b16();
  c16_t b[8][64] __attribute__((aligned(64)));
  c16_t y[8][64] __attribute__((aligned(64)));

  for (int blk = 0; blk < 16; blk++) {
    const int off = 4 * blk;
    const svint16_t x0 = svld1_s16(pg, (const int16_t *)(const void *)(src +   0 + off));
    const svint16_t x1 = svld1_s16(pg, (const int16_t *)(const void *)(src +  64 + off));
    const svint16_t x2 = svld1_s16(pg, (const int16_t *)(const void *)(src + 128 + off));
    const svint16_t x3 = svld1_s16(pg, (const int16_t *)(const void *)(src + 192 + off));
    const svint16_t x4 = svld1_s16(pg, (const int16_t *)(const void *)(src + 256 + off));
    const svint16_t x5 = svld1_s16(pg, (const int16_t *)(const void *)(src + 320 + off));
    const svint16_t x6 = svld1_s16(pg, (const int16_t *)(const void *)(src + 384 + off));
    const svint16_t x7 = svld1_s16(pg, (const int16_t *)(const void *)(src + 448 + off));

    svint16_t z0, z1, z2, z3, z4, z5, z6, z7;
    sve2_dft8_q15(x0, x1, x2, x3, x4, x5, x6, x7,
                        &z0, &z1, &z2, &z3, &z4, &z5, &z6, &z7);

    z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT8);
    z1 = sve2_cmul_q15(z1, svld1_s16(pg, g_sve2_dft512_tw.r8_q15[0][blk]));
    z2 = sve2_cmul_q15(z2, svld1_s16(pg, g_sve2_dft512_tw.r8_q15[1][blk]));
    z3 = sve2_cmul_q15(z3, svld1_s16(pg, g_sve2_dft512_tw.r8_q15[2][blk]));
    z4 = sve2_cmul_q15(z4, svld1_s16(pg, g_sve2_dft512_tw.r8_q15[3][blk]));
    z5 = sve2_cmul_q15(z5, svld1_s16(pg, g_sve2_dft512_tw.r8_q15[4][blk]));
    z6 = sve2_cmul_q15(z6, svld1_s16(pg, g_sve2_dft512_tw.r8_q15[5][blk]));
    z7 = sve2_cmul_q15(z7, svld1_s16(pg, g_sve2_dft512_tw.r8_q15[6][blk]));

    svst1_s16(pg, (int16_t *)(void *)(b[0] + off), z0);
    svst1_s16(pg, (int16_t *)(void *)(b[1] + off), z1);
    svst1_s16(pg, (int16_t *)(void *)(b[2] + off), z2);
    svst1_s16(pg, (int16_t *)(void *)(b[3] + off), z3);
    svst1_s16(pg, (int16_t *)(void *)(b[4] + off), z4);
    svst1_s16(pg, (int16_t *)(void *)(b[5] + off), z5);
    svst1_s16(pg, (int16_t *)(void *)(b[6] + off), z6);
    svst1_s16(pg, (int16_t *)(void *)(b[7] + off), z7);
  }

  for (int r = 0; r < 8; r++)
    sve2_dft64_q15_leaf(b[r], y[r]);

  sve2_store_interleave8_q15(
      y[0], y[1], y[2], y[3], y[4], y[5], y[6], y[7], dst, 64);
}


SVE2_TARGET static void sve2_dft512_q15(
    const c16_t *src, c16_t *dst)
{
  sve2_dft512_q15_r8x64(src, dst);
}


/* ========================================================================
 * Native NEON Q15 helpers.
 *
 * NEON and SVE2 use the same Q15 scaling and twiddle conventions.
 * ======================================================================== */

static inline int16x8_t neon_real_mul_q15(int16x8_t x, int16_t c)
{
  return vqrdmulhq_s16(x, vdupq_n_s16(c));
}

static inline int16x8_t neon_rot90_q15(int16x8_t x)
{
  const int16x8_t sw = vrev32q_s16(x);
  const int16x8_t ng = vqnegq_s16(sw);
  const uint16x8_t m = {0xffffu,0,0xffffu,0,0xffffu,0,0xffffu,0};
  return vbslq_s16(m, ng, sw); /* j*x = [-im,re] */
}

static inline int16x8_t neon_rot270_q15(int16x8_t x)
{
  const int16x8_t sw = vrev32q_s16(x);
  const int16x8_t ng = vqnegq_s16(sw);
  const uint16x8_t m = {0xffffu,0,0xffffu,0,0xffffu,0,0xffffu,0};
  return vbslq_s16(m, sw, ng); /* -j*x = [im,-re] */
}

/* Saturating rounded Q15 complex multiply. */
static inline int16x8_t neon_cmul_q15(int16x8_t a, int16x8_t b)
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
  const uint16x8_t m = {0xffffu,0,0xffffu,0,0xffffu,0,0xffffu,0};
  return vbslq_s16(m, re, im);
}

static inline void neon_dft3_q15(
    int16x8_t x0, int16x8_t x1, int16x8_t x2,
    int16x8_t *y0, int16x8_t *y1, int16x8_t *y2)
{
  const int16x8_t sum = vqaddq_s16(x1, x2);
  const int16x8_t diff = vqsubq_s16(x1, x2);
  const int16x8_t base = vqsubq_s16(x0,
      neon_real_mul_q15(sum, Q15_HALF));
  const int16x8_t imag = neon_real_mul_q15(diff, Q15_HALF_SQRT3);
  *y0 = vqaddq_s16(x0, sum);
  *y1 = vqaddq_s16(base, neon_rot270_q15(imag));
  *y2 = vqaddq_s16(base, neon_rot90_q15(imag));
}


static inline void neon_transpose4_q15(
    int16x8_t a, int16x8_t b, int16x8_t c, int16x8_t d,
    int16x8_t *t0, int16x8_t *t1, int16x8_t *t2, int16x8_t *t3)
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


/* -------------------------------------------------------------------------
 * Mixed-radix parent twiddles.
 * Forward and inverse coefficients are stored contiguously by branch.
 * ------------------------------------------------------------------------- */

typedef struct {
  int initialized;
  int N;
  int radix;
  int M;
  int16_t *q15;
  int16_t *q15_inv;
} mixed_parent_twiddle_t;

static inline const int16_t *mixed_twiddle_forward(
    const mixed_parent_twiddle_t *tw, int branch)
{
  return tw->q15 + (size_t)branch * (size_t)(2 * tw->M);
}

static inline const int16_t *mixed_twiddle_inverse(
    const mixed_parent_twiddle_t *tw, int branch)
{
  return tw->q15_inv + (size_t)branch * (size_t)(2 * tw->M);
}


static void *aligned_calloc64(size_t bytes)
{
  const size_t rounded = (bytes + 63u) & ~(size_t)63u;
  void *p = aligned_alloc(64, rounded ? rounded : 64);
  AssertFatal(p != NULL, "Mixed twiddle allocation failed (%zu bytes)\n", bytes);
  memset(p, 0, rounded ? rounded : 64);
  return p;
}

static void mixed_parent_prepare_one(
    mixed_parent_twiddle_t *tw, int N, int radix)
{
  if (tw->initialized)
    return;

  AssertFatal(radix == 3 || radix == 5,
              "Unsupported mixed component radix %d\n", radix);
  AssertFatal((N % radix) == 0, "Invalid mixed N=%d radix=%d\n", N, radix);

  tw->N = N;
  tw->radix = radix;
  tw->M = N / radix;

  const size_t coeffs = (size_t)(radix - 1) * (size_t)(2 * tw->M);
  tw->q15 = aligned_calloc64((size_t)2 * coeffs * sizeof(*tw->q15));
  tw->q15_inv = tw->q15 + coeffs;

  for (int branch = 1; branch < radix; branch++) {
    int16_t *forward =
        tw->q15 + (size_t)(branch - 1) * (size_t)(2 * tw->M);
    int16_t *inverse =
        tw->q15_inv + (size_t)(branch - 1) * (size_t)(2 * tw->M);

    for (int k = 0; k < tw->M; k++) {
      const float a =
          -2.0f * (float)M_PI * (float)(branch * k) / (float)N;
      const float wr = cosf(a);
      const float wi = sinf(a);
      forward[2 * k + 0] =
          q15_unitary_scale_coeff(q15_from_float(wr), radix);
      forward[2 * k + 1] =
          q15_unitary_scale_coeff(q15_from_float(wi), radix);
      inverse[2 * k + 0] =
          q15_unitary_scale_coeff(q15_from_float(wr), radix);
      inverse[2 * k + 1] =
          q15_unitary_scale_coeff(q15_from_float(-wi), radix);
    }
  }

  tw->initialized = 1;
}


/* ------------------------------------------------------------------------- */
/* Fixed-topology SVE2 power-of-two parents.                                 */
/* Large twiddle tables are built lazily for radix-4 or radix-8 as required. */
/* ------------------------------------------------------------------------- */
typedef void (*sve2_q15_child_fn_t)(const c16_t *, c16_t *);

typedef struct {
  int initialized;
  int N;
  int radix;
  int16_t *q15;
} sve2_large_pow2_twiddle_t;

static inline const int16_t *sve2_large_twiddle(
    const sve2_large_pow2_twiddle_t *tw, int branch)
{
  const int M = tw->N / tw->radix;
  return tw->q15 + (size_t)branch * (size_t)(2 * M);
}

static sve2_large_pow2_twiddle_t g_sve2_large_pow2[7];
static pthread_mutex_t g_sve2_large_twiddle_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Large SVE2 scratch is thread-local and sized to the transform in use. */
static __thread c16_t *g_sve2_large_work;
static __thread size_t g_sve2_large_work_elems;

__attribute__((noinline)) static c16_t *sve2_large_work_get(size_t need)
{
  if (need <= g_sve2_large_work_elems)
    return g_sve2_large_work;

  c16_t *p = aligned_malloc64(need * sizeof(*p));
  if (!p)
    return NULL;

  free(g_sve2_large_work);
  g_sve2_large_work = p;
  g_sve2_large_work_elems = need;
  return p;
}

static inline int sve2_large_pow2_slot(int N)
{
  switch (N) {
    case 1024: return 0;
    case 2048: return 1;
    case 4096: return 2;
    case 8192: return 3;
    case 16384: return 4;
    case 32768: return 5;
    case 65536: return 6;
    default: return -1;
  }
}

static inline int16_t sve2_large_q15_scale_coeff(float x, int radix)
{
  const int16_t q = q15_from_float(x);
  switch (radix) {
    case 4: return (int16_t)(q / 2);
    case 8: return sat_q15((long)((float)q / sqrtf(8.0f)));
    default:
      abort();
      __builtin_unreachable();
  }
}

static void sve2_large_power2_prepare_one(int N)
{
  const int slot = sve2_large_pow2_slot(N);
  AssertFatal(slot >= 0, "Unsupported large SVE2 power2 N=%d\n", N);
  sve2_large_pow2_twiddle_t *tw = &g_sve2_large_pow2[slot];
  if (tw->initialized)
    return;
  tw->N = N;
  tw->radix = (N == 2048) ? 4 : 8;

  if (tw->radix == 4) {
    const int M4 = N / 4;
    tw->q15 = aligned_malloc64((size_t)3 * (size_t)(2 * M4) * sizeof(*tw->q15));
    AssertFatal(tw->q15, "SVE2 radix-4 twiddle allocation failed N=%d\n", N);
    for (int br = 1; br < 4; br++) {
      int16_t *twiddle = tw->q15 + (size_t)(br - 1) * (size_t)(2 * M4);
      for (int k = 0; k < M4; k++) {
        const float a = -2.0f * (float)M_PI * (float)(br * k) / (float)N;
        twiddle[2*k+0] = sve2_large_q15_scale_coeff(cosf(a), 4);
        twiddle[2*k+1] = sve2_large_q15_scale_coeff(sinf(a), 4);
      }
    }
  } else {
    const int M8 = N / 8;
    tw->q15 = aligned_malloc64((size_t)7 * (size_t)(2 * M8) * sizeof(*tw->q15));
    AssertFatal(tw->q15, "SVE2 radix-8 twiddle allocation failed N=%d\n", N);
    for (int br = 1; br < 8; br++) {
      int16_t *twiddle = tw->q15 + (size_t)(br - 1) * (size_t)(2 * M8);
      for (int k = 0; k < M8; k++) {
        const float a = -2.0f * (float)M_PI * (float)(br * k) / (float)N;
        twiddle[2*k+0] = sve2_large_q15_scale_coeff(cosf(a), 8);
        twiddle[2*k+1] = sve2_large_q15_scale_coeff(sinf(a), 8);
      }
    }
  }
  __atomic_store_n(&tw->initialized, 1, __ATOMIC_RELEASE);
}

__attribute__((noinline)) static sve2_large_pow2_twiddle_t *sve2_large_twiddle_get(int N)
{
  const int slot = sve2_large_pow2_slot(N);
  if (slot < 0)
    return NULL;

  sve2_large_pow2_twiddle_t *tw = &g_sve2_large_pow2[slot];
  if (__atomic_load_n(&tw->initialized, __ATOMIC_ACQUIRE))
    return tw;

  pthread_mutex_lock(&g_sve2_large_twiddle_mutex);
  if (!tw->initialized)
    sve2_large_power2_prepare_one(N);
  pthread_mutex_unlock(&g_sve2_large_twiddle_mutex);
  return tw;
}



SVE2_TARGET static void sve2_large_r4_q15(
    const c16_t *src, c16_t *dst, int N, sve2_q15_child_fn_t child)
{
  sve2_large_pow2_twiddle_t *tw = sve2_large_twiddle_get(N);
  SVE2_ASSERT(tw && tw->initialized, "Missing SVE2 large r4 twiddles N=%d\n", N);
  const int M = N / 4;
  const svbool_t pg16 = svptrue_b16();
  const svbool_t pg32 = svptrue_b32();
  c16_t b[N] __attribute__((aligned(64)));
  c16_t y[N] __attribute__((aligned(64)));
  for (int off = 0; off < M; off += 4) {
    const svint16_t x0 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 0*M + off));
    const svint16_t x1 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 1*M + off));
    const svint16_t x2 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 2*M + off));
    const svint16_t x3 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 3*M + off));
    svint16_t z0,z1,z2,z3;
    sve2_dft4_q15(x0,x1,x2,x3,&z0,&z1,&z2,&z3);
    z0 = sve2_q15_real_mul(z0, Q15_HALF);
    z1 = sve2_cmul_q15(z1, svld1_s16(pg16, sve2_large_twiddle(tw, 0) + 2*off));
    z2 = sve2_cmul_q15(z2, svld1_s16(pg16, sve2_large_twiddle(tw, 1) + 2*off));
    z3 = sve2_cmul_q15(z3, svld1_s16(pg16, sve2_large_twiddle(tw, 2) + 2*off));
    svst1_s16(pg16,(int16_t *)(void *)(b+0*M+off),z0);
    svst1_s16(pg16,(int16_t *)(void *)(b+1*M+off),z1);
    svst1_s16(pg16,(int16_t *)(void *)(b+2*M+off),z2);
    svst1_s16(pg16,(int16_t *)(void *)(b+3*M+off),z3);
  }
  for (int r=0;r<4;r++) child(b+r*M,y+r*M);
  for (int k=0;k<M;k+=4) {
    const svuint32_t v0=svreinterpret_u32_s16(svld1_s16(pg16,(const int16_t *)(const void *)(y+0*M+k)));
    const svuint32_t v1=svreinterpret_u32_s16(svld1_s16(pg16,(const int16_t *)(const void *)(y+1*M+k)));
    const svuint32_t v2=svreinterpret_u32_s16(svld1_s16(pg16,(const int16_t *)(const void *)(y+2*M+k)));
    const svuint32_t v3=svreinterpret_u32_s16(svld1_s16(pg16,(const int16_t *)(const void *)(y+3*M+k)));
    svst4_u32(pg32,(uint32_t *)(void *)(dst+4*k),svcreate4_u32(v0,v1,v2,v3));
  }
}


SVE2_TARGET static void sve2_r8_parent_q15(
    const c16_t *src, c16_t *dst, int N, sve2_q15_child_fn_t child)
{
  sve2_large_pow2_twiddle_t *tw=sve2_large_twiddle_get(N);
  SVE2_ASSERT(tw&&tw->initialized,"Missing SVE2 large r8 twiddles N=%d\n",N);
  const int M=N/8; const svbool_t pg16=svptrue_b16();
  c16_t b[N] __attribute__((aligned(64))); c16_t y[N] __attribute__((aligned(64)));
  for(int off=0;off<M;off+=4) {
    const svint16_t x0=svld1_s16(pg16,(const int16_t *)(const void *)(src+0*M+off));
    const svint16_t x1=svld1_s16(pg16,(const int16_t *)(const void *)(src+1*M+off));
    const svint16_t x2=svld1_s16(pg16,(const int16_t *)(const void *)(src+2*M+off));
    const svint16_t x3=svld1_s16(pg16,(const int16_t *)(const void *)(src+3*M+off));
    const svint16_t x4=svld1_s16(pg16,(const int16_t *)(const void *)(src+4*M+off));
    const svint16_t x5=svld1_s16(pg16,(const int16_t *)(const void *)(src+5*M+off));
    const svint16_t x6=svld1_s16(pg16,(const int16_t *)(const void *)(src+6*M+off));
    const svint16_t x7=svld1_s16(pg16,(const int16_t *)(const void *)(src+7*M+off));
    svint16_t z0,z1,z2,z3,z4,z5,z6,z7;
    sve2_dft8_q15(x0,x1,x2,x3,x4,x5,x6,x7,&z0,&z1,&z2,&z3,&z4,&z5,&z6,&z7);
    z0=sve2_q15_real_mul(z0,Q15_INV_SQRT8);
    z1=sve2_cmul_q15(z1,svld1_s16(pg16,sve2_large_twiddle(tw, 0)+2*off));
    z2=sve2_cmul_q15(z2,svld1_s16(pg16,sve2_large_twiddle(tw, 1)+2*off));
    z3=sve2_cmul_q15(z3,svld1_s16(pg16,sve2_large_twiddle(tw, 2)+2*off));
    z4=sve2_cmul_q15(z4,svld1_s16(pg16,sve2_large_twiddle(tw, 3)+2*off));
    z5=sve2_cmul_q15(z5,svld1_s16(pg16,sve2_large_twiddle(tw, 4)+2*off));
    z6=sve2_cmul_q15(z6,svld1_s16(pg16,sve2_large_twiddle(tw, 5)+2*off));
    z7=sve2_cmul_q15(z7,svld1_s16(pg16,sve2_large_twiddle(tw, 6)+2*off));
    svst1_s16(pg16,(int16_t *)(void *)(b+0*M+off),z0); svst1_s16(pg16,(int16_t *)(void *)(b+1*M+off),z1);
    svst1_s16(pg16,(int16_t *)(void *)(b+2*M+off),z2); svst1_s16(pg16,(int16_t *)(void *)(b+3*M+off),z3);
    svst1_s16(pg16,(int16_t *)(void *)(b+4*M+off),z4); svst1_s16(pg16,(int16_t *)(void *)(b+5*M+off),z5);
    svst1_s16(pg16,(int16_t *)(void *)(b+6*M+off),z6); svst1_s16(pg16,(int16_t *)(void *)(b+7*M+off),z7);
  }
  for(int r=0;r<8;r++) child(b+r*M,y+r*M);
  sve2_store_interleave8_q15(y+0*M,y+1*M,y+2*M,y+3*M,y+4*M,y+5*M,y+6*M,y+7*M,dst,M);
}


SVE2_TARGET static void sve2_dft1024_q15_r8x128(const c16_t*s,c16_t*d){sve2_r8_parent_q15(s,d,1024,sve2_dft128_q15_fused_st2);}

SVE2_TARGET static void sve2_dft1024_q15(const c16_t*s,c16_t*d){
sve2_dft1024_q15_r8x128(s,d);
}


SVE2_TARGET static void sve2_dft2048_q15_r4x512(const c16_t*s,c16_t*d){sve2_large_r4_q15(s,d,2048,sve2_dft512_q15);}


SVE2_TARGET static void sve2_dft2048_q15(const c16_t*s,c16_t*d){
sve2_dft2048_q15_r4x512(s,d);
}


SVE2_TARGET static void sve2_dft4096_q15_r8x512(const c16_t*s,c16_t*d){sve2_r8_parent_q15(s,d,4096,sve2_dft512_q15);}

SVE2_TARGET static void sve2_dft4096_q15(const c16_t*s,c16_t*d){
sve2_dft4096_q15_r8x512(s,d);
}


SVE2_TARGET static void sve2_dft8192_q15_r8x1024(const c16_t*s,c16_t*d){sve2_r8_parent_q15(s,d,8192,sve2_dft1024_q15);}

SVE2_TARGET static void sve2_dft8192_q15(const c16_t*s,c16_t*d){
sve2_dft8192_q15_r8x1024(s,d);
}


/* ------------------------------------------------------------------------- */
/* Large one-level SVE2 power-of-two transforms.                             */
/* Branch zero is scaled directly; nonzero branches use scaled twiddles.     */
/* ------------------------------------------------------------------------- */

#if defined(__GNUC__) && !defined(__clang__)
#define SVE2_LARGE_NOIPA __attribute__((noinline,noclone,optimize("O2")))
#else
#define SVE2_LARGE_NOIPA __attribute__((noinline))
#endif


SVE2_TARGET SVE2_LARGE_NOIPA static void sve2_large_r8_q15(
    const c16_t *src,c16_t *dst,int N,sve2_q15_child_fn_t child)
{
  sve2_large_pow2_twiddle_t *tw=sve2_large_twiddle_get(N);
  SVE2_ASSERT(svcntb()==16 && tw && tw->initialized,
              "Missing SVE2 large radix-8 state N=%d\n",N);
  c16_t *work = sve2_large_work_get((size_t)2 * (size_t)N);
  SVE2_ASSERT(work != NULL, "SVE2 large radix-8 scratch allocation failed N=%d\n", N);
  const int M=N/8; const svbool_t pg16=svptrue_b16();
  c16_t *b=work,*y=work+N;
  for(int off=0;off<M;off+=4){
    svint16_t z0,z1,z2,z3,z4,z5,z6,z7;
    sve2_dft8_q15(
      svld1_s16(pg16,(const int16_t *)(const void *)(src+0*M+off)),svld1_s16(pg16,(const int16_t *)(const void *)(src+1*M+off)),
      svld1_s16(pg16,(const int16_t *)(const void *)(src+2*M+off)),svld1_s16(pg16,(const int16_t *)(const void *)(src+3*M+off)),
      svld1_s16(pg16,(const int16_t *)(const void *)(src+4*M+off)),svld1_s16(pg16,(const int16_t *)(const void *)(src+5*M+off)),
      svld1_s16(pg16,(const int16_t *)(const void *)(src+6*M+off)),svld1_s16(pg16,(const int16_t *)(const void *)(src+7*M+off)),
      &z0,&z1,&z2,&z3,&z4,&z5,&z6,&z7);
    z0=sve2_q15_real_mul(z0,Q15_INV_SQRT8);
    z1=sve2_cmul_q15(z1,svld1_s16(pg16,sve2_large_twiddle(tw, 0)+2*off));
    z2=sve2_cmul_q15(z2,svld1_s16(pg16,sve2_large_twiddle(tw, 1)+2*off));
    z3=sve2_cmul_q15(z3,svld1_s16(pg16,sve2_large_twiddle(tw, 2)+2*off));
    z4=sve2_cmul_q15(z4,svld1_s16(pg16,sve2_large_twiddle(tw, 3)+2*off));
    z5=sve2_cmul_q15(z5,svld1_s16(pg16,sve2_large_twiddle(tw, 4)+2*off));
    z6=sve2_cmul_q15(z6,svld1_s16(pg16,sve2_large_twiddle(tw, 5)+2*off));
    z7=sve2_cmul_q15(z7,svld1_s16(pg16,sve2_large_twiddle(tw, 6)+2*off));
    svst1_s16(pg16,(int16_t *)(void *)(b+0*M+off),z0); svst1_s16(pg16,(int16_t *)(void *)(b+1*M+off),z1);
    svst1_s16(pg16,(int16_t *)(void *)(b+2*M+off),z2); svst1_s16(pg16,(int16_t *)(void *)(b+3*M+off),z3);
    svst1_s16(pg16,(int16_t *)(void *)(b+4*M+off),z4); svst1_s16(pg16,(int16_t *)(void *)(b+5*M+off),z5);
    svst1_s16(pg16,(int16_t *)(void *)(b+6*M+off),z6); svst1_s16(pg16,(int16_t *)(void *)(b+7*M+off),z7);
  }
  for(int r=0;r<8;r++) child(b+r*M,y+r*M);
  sve2_store_interleave8_q15(y+0*M,y+1*M,y+2*M,y+3*M,y+4*M,y+5*M,y+6*M,y+7*M,dst,M);
}


SVE2_TARGET __attribute__((noinline)) static void sve2_dft16384_q15_r8x2048(const c16_t*s,c16_t*d){sve2_large_r8_q15(s,d,16384,sve2_dft2048_q15);}


SVE2_TARGET __attribute__((noinline)) static void sve2_dft16384_q15(const c16_t*s,c16_t*d){sve2_dft16384_q15_r8x2048(s,d);}


SVE2_TARGET __attribute__((noinline)) static void sve2_dft32768_q15_r8x4096(const c16_t*s,c16_t*d){sve2_large_r8_q15(s,d,32768,sve2_dft4096_q15);}


SVE2_TARGET __attribute__((noinline)) static void sve2_dft32768_q15(const c16_t*s,c16_t*d){sve2_dft32768_q15_r8x4096(s,d);}


SVE2_TARGET __attribute__((noinline)) static void sve2_dft65536_q15(const c16_t*s,c16_t*d){sve2_large_r8_q15(s,d,65536,sve2_dft8192_q15);}


/* ========================================================================
 * Mixed-radix 2^a*3^b*5^c plan metadata and twiddles.
 * ======================================================================== */

#define MIXED_MAX_STAGES 3

typedef struct {
  int N;
  int depth;
  int leaf_n;
  /* stage_code identifies the factorization; radix[] stores the outer radix. */
  unsigned char stage_code[MIXED_MAX_STAGES];
  unsigned char radix[MIXED_MAX_STAGES];
  mixed_parent_twiddle_t tw[MIXED_MAX_STAGES];

  /* Component twiddles for fused mixed-radix stages. */
  mixed_parent_twiddle_t fused_first_tw[MIXED_MAX_STAGES];
  mixed_parent_twiddle_t fused_second_tw[MIXED_MAX_STAGES];

  size_t workspace_elems;
} mixed_plan_t;

enum {
  MIXED_STAGE_R3 = 3,
  MIXED_STAGE_R5 = 5,
  MIXED_STAGE_R9 = 9,
  /* Outer R3 followed by child R5. */
  MIXED_STAGE_R15_35 = 15,
  /* Outer R5 followed by child R3. */
  MIXED_STAGE_R15_53 = 53,
  MIXED_STAGE_R25 = 25
};

static int mixed_stage_info(
    unsigned char code, int *radix, int *consume3, int *consume5,
    int *first_radix, int *second_radix)
{
  int r = 0, c3 = 0, c5 = 0, first = 0, second = 0;
  switch (code) {
    case MIXED_STAGE_R3:
      r = 3; c3 = 1; first = 3; break;
    case MIXED_STAGE_R5:
      r = 5; c5 = 1; first = 5; break;
    case MIXED_STAGE_R9:
      r = 9; c3 = 2; first = 3; second = 3; break;
    case MIXED_STAGE_R15_35:
      r = 15; c3 = 1; c5 = 1; first = 3; second = 5; break;
    case MIXED_STAGE_R15_53:
      r = 15; c3 = 1; c5 = 1; first = 5; second = 3; break;
    case MIXED_STAGE_R25:
      r = 25; c5 = 2; first = 5; second = 5; break;
    default:
      return 0;
  }
  if (radix) *radix = r;
  if (consume3) *consume3 = c3;
  if (consume5) *consume5 = c5;
  if (first_radix) *first_radix = first;
  if (second_radix) *second_radix = second;
  return 1;
}

static int factor_235(int N, int *e2, int *e3, int *e5)
{
  if (N <= 0)
    return 0;
  int n = N;
  *e2 = *e3 = *e5 = 0;
  while ((n % 2) == 0) { (*e2)++; n /= 2; }
  while ((n % 3) == 0) { (*e3)++; n /= 3; }
  while ((n % 5) == 0) { (*e5)++; n /= 5; }
  return n == 1;
}

static void mixed_twiddle_free(mixed_parent_twiddle_t *tw)
{
  if (!tw || !tw->initialized)
    return;
  free(tw->q15);
  memset(tw, 0, sizeof(*tw));
}

static void mixed_plan_reset(mixed_plan_t *p)
{
  if (!p)
    return;
  for (int i = 0; i < p->depth; i++) {
    mixed_twiddle_free(&p->tw[i]);
    mixed_twiddle_free(&p->fused_first_tw[i]);
    mixed_twiddle_free(&p->fused_second_tw[i]);
  }
  memset(p, 0, sizeof(*p));
}

static inline int mixed_leaf_supported(int n)
{
  /* Leaf sizes accepted by mixed-plan construction. */
  return n == 1 || n == 12 || n == 24 || n == 60 || is_power_of_two_int(n);
}

static int mixed_plan_init(
    mixed_plan_t *p, int N, const unsigned char *stage, int depth)
{
  if (!p || N <= 0 || depth < 0 || depth > MIXED_MAX_STAGES)
    return 0;

  int e2, e3, e5;
  if (!factor_235(N, &e2, &e3, &e5))
    return 0;

  memset(p, 0, sizeof(*p));
  p->N = N;
  p->depth = depth;

  int cur = N;
  int seen3 = 0, seen5 = 0;
  size_t need = 0;
  for (int i = 0; i < depth; i++) {
    int r, c3, c5, first, second;
    const unsigned char code = stage[i];
    if (!mixed_stage_info(code, &r, &c3, &c5, &first, &second) ||
        (cur % r) != 0) {
      mixed_plan_reset(p);
      return 0;
    }

    p->stage_code[i] = code;
    p->radix[i] = (unsigned char)r;
    seen3 += c3;
    seen5 += c5;

    need += (size_t)2 * (size_t)cur;

    if (second == 0) {
      mixed_parent_prepare_one(&p->tw[i], cur, r);
    } else {
      /* Fused stages need the composite N/M/radix metadata, but their
       * arithmetic reads only the two component R3/R5 twiddle tables. */
      p->tw[i].initialized = 1;
      p->tw[i].N = cur;
      p->tw[i].radix = r;
      p->tw[i].M = cur / r;
      mixed_parent_prepare_one(&p->fused_first_tw[i], cur, first);
      mixed_parent_prepare_one(&p->fused_second_tw[i], cur / first, second);
    }
    cur /= r;
  }

  int leaf_e2 = 0, leaf_e3 = 0, leaf_e5 = 0;
  if (!factor_235(cur, &leaf_e2, &leaf_e3, &leaf_e5) ||
      seen3 + leaf_e3 != e3 || seen5 + leaf_e5 != e5 ||
      !mixed_leaf_supported(cur)) {
    mixed_plan_reset(p);
    return 0;
  }

  p->leaf_n = cur;
  p->workspace_elems = need;
  return 1;
}

SVE2_TARGET static inline void sve2_235_leaf_q15(
    const mixed_plan_t *p, const c16_t *src, c16_t *dst)
{
  switch (p->leaf_n) {
    case 1: dst[0] = src[0]; return;
    case 4: sve2_dft4_q15_rshift(src, dst); return;
    case 8: sve2_dft8_q15_leaf(src, dst); return;
    case 12: sve2_dft12_q15_leaf(src, dst); return;
    case 16: sve2_dft16_q15_leaf(src, dst); return;
    case 24:
    case 60: {
      const sve2_plan_t *leaf = sve2_plan_get(p->leaf_n);
      SVE2_ASSERT(leaf != NULL, "Missing generic SVE2 leaf N=%d\n", p->leaf_n);
      sve2_dft_q15(leaf, src, dst);
      return;
    }
    case 32: sve2_dft32_q15_leaf(src, dst); return;
    case 64: sve2_dft64_q15_leaf(src, dst); return;
    case 128: sve2_dft128_q15_fused_st2(src, dst); return;
    case 256: sve2_dft256_q15_r4x64(src, dst); return;
    case 512: sve2_dft512_q15(src, dst); return;
    case 1024: sve2_dft1024_q15(src, dst); return;
    case 2048: sve2_dft2048_q15(src, dst); return;
    case 4096: sve2_dft4096_q15(src, dst); return;
    case 8192: sve2_dft8192_q15(src, dst); return;
    case 16384: sve2_dft16384_q15(src, dst); return;
    case 32768: sve2_dft32768_q15(src, dst); return;
    case 65536: sve2_dft65536_q15(src, dst); return;
    default:
      SVE2_ASSERT(false, "Unsupported SVE2 mixed leaf N=%d\n", p->leaf_n);
      return;
  }
}


static inline void neon_dft12_q15(const c16_t *src, c16_t *dst);
static void neon_leaf_q15(const c16_t *src, c16_t *dst, int N);

static void inverse_small_leaf_q15(
    const c16_t *src, c16_t *dst, int N)
{
  AssertFatal(N == 4 || N == 8 || N == 12 || N == 32,
             "Unsupported small inverse leaf N=%d\n", N);

  if (N == 4 || N == 8) {
    c16_t transformed[8] __attribute__((aligned(64)));
    neon_leaf_q15(src, transformed, N);
    dst[0] = transformed[0];
    for (int k = 1; k < N; k++)
      dst[k] = transformed[N - k];
    return;
  }

  if (N == 12) {
    c16_t conjugated[12] __attribute__((aligned(64)));
    c16_t transformed[12] __attribute__((aligned(64)));

    for (int i = 0; i < 12; i++) {
      conjugated[i].r = src[i].r;
      conjugated[i].i = sat_q15(-(long)src[i].i);
    }

    neon_dft12_q15(conjugated, transformed);

    for (int i = 0; i < 12; i++) {
      dst[i].r = transformed[i].r;
      dst[i].i = sat_q15(-(long)transformed[i].i);
    }
    return;
  }

  c16_t conjugated[32] __attribute__((aligned(64)));
  c16_t transformed[32] __attribute__((aligned(64)));

  for (int i = 0; i < 32; i++) {
    conjugated[i].r = src[i].r;
    conjugated[i].i = sat_q15(-(long)src[i].i);
  }

  neon_leaf_q15(conjugated, transformed, 32);

  for (int i = 0; i < 32; i++) {
    dst[i].r = transformed[i].r;
    dst[i].i = sat_q15(-(long)transformed[i].i);
  }
}

__attribute__((noinline)) static void mixed_inverse_leaf_q15(
    const mixed_plan_t *p, const c16_t *src, c16_t *dst)
{
  if (p->leaf_n == 1) { dst[0] = src[0]; return; }
  if (p->leaf_n == 4 || p->leaf_n == 8 ||
      p->leaf_n == 12 || p->leaf_n == 32) {
    inverse_small_leaf_q15(src, dst, p->leaf_n);
    return;
  }
  if (p->leaf_n == 16) { idft16_q15_native(src, dst); return; }
  AssertFatal(is_power_of_two_int(p->leaf_n) && p->leaf_n >= 64,
              "Direct inverse mixed leaf must be 4, 8, 12, 16, 32 or power-of-two >=64, got N=%d\n",
              p->leaf_n);
  dft_power2_q15(src, dst, p->leaf_n, DFT_DIR_INVERSE);
}

/* Radix-9 parent (R3 x R3) with branch-major output.
 * The two R3 stages are fused before each output tile is stored. */
SVE2_TARGET static inline void sve2_235_r9_prepare_q15(
    const mixed_plan_t *p, int level,
    const c16_t *src, c16_t *b)
{
  const mixed_parent_twiddle_t *tw = &p->tw[level];
  const int M = tw->M;
  SVE2_ASSERT(tw->radix == 9 && p->stage_code[level] == MIXED_STAGE_R9,
              "Invalid SVE2 radix-9 prepare level=%d N=%d radix=%d code=%u\\n",
              level, p->N, tw->radix, (unsigned)p->stage_code[level]);

  const mixed_parent_twiddle_t *twB = &p->fused_first_tw[level];
  const mixed_parent_twiddle_t *twA = &p->fused_second_tw[level];
  SVE2_ASSERT(twB->radix == 3 && twB->M == 3 * M &&
                  twA->radix == 3 && twA->M == M,
              "Bad register SVE2 radix-9 twiddles N=%d M=%d\\n", p->N, M);

  for (int off = 0; off < M; off += 4) {
    const int rem = M - off < 4 ? M - off : 4;
    const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));
    svint16_t s00,s01,s02,s10,s11,s12,s20,s21,s22;

    {
      const svint16_t x0 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 0 * M + off));
      const svint16_t x1 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 3 * M + off));
      const svint16_t x2 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 6 * M + off));
      sve2_dft3_q15(x0,x1,x2,&s00,&s01,&s02);
      s00 = sve2_q15_real_mul(s00,Q15_INV_SQRT3);
      s01 = sve2_cmul_q15(s01,svld1_s16(pg16,mixed_twiddle_forward(twB, 0) + 2 * off));
      s02 = sve2_cmul_q15(s02,svld1_s16(pg16,mixed_twiddle_forward(twB, 1) + 2 * off));
    }
    {
      const int first_off = M + off;
      const svint16_t x0 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 1 * M + off));
      const svint16_t x1 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 4 * M + off));
      const svint16_t x2 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 7 * M + off));
      sve2_dft3_q15(x0,x1,x2,&s10,&s11,&s12);
      s10 = sve2_q15_real_mul(s10,Q15_INV_SQRT3);
      s11 = sve2_cmul_q15(s11,svld1_s16(pg16,mixed_twiddle_forward(twB, 0) + 2 * first_off));
      s12 = sve2_cmul_q15(s12,svld1_s16(pg16,mixed_twiddle_forward(twB, 1) + 2 * first_off));
    }
    {
      const int first_off = 2 * M + off;
      const svint16_t x0 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 2 * M + off));
      const svint16_t x1 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 5 * M + off));
      const svint16_t x2 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 8 * M + off));
      sve2_dft3_q15(x0,x1,x2,&s20,&s21,&s22);
      s20 = sve2_q15_real_mul(s20,Q15_INV_SQRT3);
      s21 = sve2_cmul_q15(s21,svld1_s16(pg16,mixed_twiddle_forward(twB, 0) + 2 * first_off));
      s22 = sve2_cmul_q15(s22,svld1_s16(pg16,mixed_twiddle_forward(twB, 1) + 2 * first_off));
    }

    {
      svint16_t z0,z1,z2;
      sve2_dft3_q15(s00,s10,s20,&z0,&z1,&z2);
      z0 = sve2_q15_real_mul(z0,Q15_INV_SQRT3);
      z1 = sve2_cmul_q15(z1,svld1_s16(pg16,mixed_twiddle_forward(twA, 0) + 2 * off));
      z2 = sve2_cmul_q15(z2,svld1_s16(pg16,mixed_twiddle_forward(twA, 1) + 2 * off));
      svst1_s16(pg16,(int16_t *)(void *)(b + 0 * M + off),z0);
      svst1_s16(pg16,(int16_t *)(void *)(b + 3 * M + off),z1);
      svst1_s16(pg16,(int16_t *)(void *)(b + 6 * M + off),z2);
    }
    {
      svint16_t z0,z1,z2;
      sve2_dft3_q15(s01,s11,s21,&z0,&z1,&z2);
      z0 = sve2_q15_real_mul(z0,Q15_INV_SQRT3);
      z1 = sve2_cmul_q15(z1,svld1_s16(pg16,mixed_twiddle_forward(twA, 0) + 2 * off));
      z2 = sve2_cmul_q15(z2,svld1_s16(pg16,mixed_twiddle_forward(twA, 1) + 2 * off));
      svst1_s16(pg16,(int16_t *)(void *)(b + 1 * M + off),z0);
      svst1_s16(pg16,(int16_t *)(void *)(b + 4 * M + off),z1);
      svst1_s16(pg16,(int16_t *)(void *)(b + 7 * M + off),z2);
    }
    {
      svint16_t z0,z1,z2;
      sve2_dft3_q15(s02,s12,s22,&z0,&z1,&z2);
      z0 = sve2_q15_real_mul(z0,Q15_INV_SQRT3);
      z1 = sve2_cmul_q15(z1,svld1_s16(pg16,mixed_twiddle_forward(twA, 0) + 2 * off));
      z2 = sve2_cmul_q15(z2,svld1_s16(pg16,mixed_twiddle_forward(twA, 1) + 2 * off));
      svst1_s16(pg16,(int16_t *)(void *)(b + 2 * M + off),z0);
      svst1_s16(pg16,(int16_t *)(void *)(b + 5 * M + off),z1);
      svst1_s16(pg16,(int16_t *)(void *)(b + 8 * M + off),z2);
    }
  }
}

SVE2_TARGET static inline void sve2_235_r9_prepare_q15_inverse(
    const mixed_plan_t *p, int level,
    const c16_t *src, c16_t *b)
{
  const mixed_parent_twiddle_t *tw = &p->tw[level];
  const int M = tw->M;
  SVE2_ASSERT(tw->radix == 9 && p->stage_code[level] == MIXED_STAGE_R9,
              "Invalid SVE2 radix-9 prepare level=%d N=%d radix=%d code=%u\\n",
              level, p->N, tw->radix, (unsigned)p->stage_code[level]);

  const mixed_parent_twiddle_t *twB = &p->fused_first_tw[level];
  const mixed_parent_twiddle_t *twA = &p->fused_second_tw[level];
  SVE2_ASSERT(twB->radix == 3 && twB->M == 3 * M &&
                  twA->radix == 3 && twA->M == M,
              "Bad register SVE2 radix-9 twiddles N=%d M=%d\\n", p->N, M);

  for (int off = 0; off < M; off += 4) {
    const int rem = M - off < 4 ? M - off : 4;
    const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));
    svint16_t s00,s01,s02,s10,s11,s12,s20,s21,s22;

    {
      const svint16_t x0 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 0 * M + off));
      const svint16_t x1 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 3 * M + off));
      const svint16_t x2 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 6 * M + off));
      sve2_idft3_q15(x0,x1,x2,&s00,&s01,&s02);
      s00 = sve2_q15_real_mul(s00,Q15_INV_SQRT3);
      s01 = sve2_cmul_q15(s01,svld1_s16(pg16,mixed_twiddle_inverse(twB, 0) + 2 * off));
      s02 = sve2_cmul_q15(s02,svld1_s16(pg16,mixed_twiddle_inverse(twB, 1) + 2 * off));
    }
    {
      const int first_off = M + off;
      const svint16_t x0 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 1 * M + off));
      const svint16_t x1 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 4 * M + off));
      const svint16_t x2 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 7 * M + off));
      sve2_idft3_q15(x0,x1,x2,&s10,&s11,&s12);
      s10 = sve2_q15_real_mul(s10,Q15_INV_SQRT3);
      s11 = sve2_cmul_q15(s11,svld1_s16(pg16,mixed_twiddle_inverse(twB, 0) + 2 * first_off));
      s12 = sve2_cmul_q15(s12,svld1_s16(pg16,mixed_twiddle_inverse(twB, 1) + 2 * first_off));
    }
    {
      const int first_off = 2 * M + off;
      const svint16_t x0 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 2 * M + off));
      const svint16_t x1 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 5 * M + off));
      const svint16_t x2 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 8 * M + off));
      sve2_idft3_q15(x0,x1,x2,&s20,&s21,&s22);
      s20 = sve2_q15_real_mul(s20,Q15_INV_SQRT3);
      s21 = sve2_cmul_q15(s21,svld1_s16(pg16,mixed_twiddle_inverse(twB, 0) + 2 * first_off));
      s22 = sve2_cmul_q15(s22,svld1_s16(pg16,mixed_twiddle_inverse(twB, 1) + 2 * first_off));
    }

    {
      svint16_t z0,z1,z2;
      sve2_idft3_q15(s00,s10,s20,&z0,&z1,&z2);
      z0 = sve2_q15_real_mul(z0,Q15_INV_SQRT3);
      z1 = sve2_cmul_q15(z1,svld1_s16(pg16,mixed_twiddle_inverse(twA, 0) + 2 * off));
      z2 = sve2_cmul_q15(z2,svld1_s16(pg16,mixed_twiddle_inverse(twA, 1) + 2 * off));
      svst1_s16(pg16,(int16_t *)(void *)(b + 0 * M + off),z0);
      svst1_s16(pg16,(int16_t *)(void *)(b + 3 * M + off),z1);
      svst1_s16(pg16,(int16_t *)(void *)(b + 6 * M + off),z2);
    }
    {
      svint16_t z0,z1,z2;
      sve2_idft3_q15(s01,s11,s21,&z0,&z1,&z2);
      z0 = sve2_q15_real_mul(z0,Q15_INV_SQRT3);
      z1 = sve2_cmul_q15(z1,svld1_s16(pg16,mixed_twiddle_inverse(twA, 0) + 2 * off));
      z2 = sve2_cmul_q15(z2,svld1_s16(pg16,mixed_twiddle_inverse(twA, 1) + 2 * off));
      svst1_s16(pg16,(int16_t *)(void *)(b + 1 * M + off),z0);
      svst1_s16(pg16,(int16_t *)(void *)(b + 4 * M + off),z1);
      svst1_s16(pg16,(int16_t *)(void *)(b + 7 * M + off),z2);
    }
    {
      svint16_t z0,z1,z2;
      sve2_idft3_q15(s02,s12,s22,&z0,&z1,&z2);
      z0 = sve2_q15_real_mul(z0,Q15_INV_SQRT3);
      z1 = sve2_cmul_q15(z1,svld1_s16(pg16,mixed_twiddle_inverse(twA, 0) + 2 * off));
      z2 = sve2_cmul_q15(z2,svld1_s16(pg16,mixed_twiddle_inverse(twA, 1) + 2 * off));
      svst1_s16(pg16,(int16_t *)(void *)(b + 2 * M + off),z0);
      svst1_s16(pg16,(int16_t *)(void *)(b + 5 * M + off),z1);
      svst1_s16(pg16,(int16_t *)(void *)(b + 8 * M + off),z2);
    }
  }
}


SVE2_TARGET static void sve2_235_exec_q15_rec(
    const mixed_plan_t *p, int level,
    const c16_t *src, c16_t *dst, c16_t *work)
{
  if (level == p->depth) {
    sve2_235_leaf_q15(p, src, dst);
    return;
  }

  const mixed_parent_twiddle_t *tw = &p->tw[level];
  const int N = tw->N;
  const int M = tw->M;
  const int r = tw->radix;
  c16_t *b = work;
  c16_t *y = work + N;
  c16_t *child_work = work + 2 * N;

  /* Two consecutive radix-9 stages keep all 81 child branches in
   * branch-major order until the final scatter to natural output order. */
  if (r == 9 && level + 1 < p->depth &&
      p->stage_code[level + 1] == MIXED_STAGE_R9) {
    const mixed_parent_twiddle_t *tw_inner = &p->tw[level + 1];
    const int M1 = M;
    const int M2 = tw_inner->M;
    SVE2_ASSERT(tw_inner->N == M1 && tw_inner->radix == 9 && M1 == 9 * M2,
                "Bad SVE2 R9xR9 topology level=%d N=%d M1=%d M2=%d\\n",
                level, N, M1, M2);

    sve2_235_r9_prepare_q15(p, level, src, b);

    /* Reuse the next level's workspace as one branch-local inner radix-9 input.
     * combined_y uses the outer y region as 81 branch-major child outputs. */
    c16_t *combined_y = y;
    c16_t *inner_b = child_work;
    c16_t *grand_work = inner_b + M1;

    for (int outer = 0; outer < 9; outer++) {
      sve2_235_r9_prepare_q15(p, level + 1,
                                   b + outer * M1, inner_b);
      for (int inner = 0; inner < 9; inner++) {
        const int combined = 9 * inner + outer;
        sve2_235_exec_q15_rec(p, level + 2,
                                   inner_b + inner * M2,
                                   combined_y + combined * M2,
                                   grand_work);
      }
    }

    /* Natural output index is
     *   81*k + 9*inner + outer.
     * combined_y is already laid out in that branch order. */
    uint32_t *dst32 = (uint32_t *)(void *)dst;
    for (int k = 0; k < M2; k += 4) {
      const int rem = M2 - k < 4 ? M2 - k : 4;
      const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));
      const svbool_t pg32 = svwhilelt_b32((uint64_t)0, (uint64_t)rem);
      for (int br = 0; br < 81; br++) {
        const svint16_t a = svld1_s16(pg16,
            (const int16_t *)(const void *)(combined_y + br * M2 + k));
        const svuint32_t idx = svindex_u32((uint32_t)(81 * k + br), 81);
        svst1_scatter_u32index_u32(pg32, dst32, idx, svreinterpret_u32_s16(a));
      }
    }
    return;
  }

  if (r == 9) {
    sve2_235_r9_prepare_q15(p, level, src, b);

    for (int br = 0; br < 9; br++)
      sve2_235_exec_q15_rec(p, level + 1,
                                 b + br * M, y + br * M, child_work);

    uint32_t *dst32 = (uint32_t *)(void *)dst;
    for (int k = 0; k < M; k += 4) {
      const int rem = M - k < 4 ? M - k : 4;
      const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));
      const svbool_t pg32 = svwhilelt_b32((uint64_t)0, (uint64_t)rem);
      for (int br = 0; br < 9; br++) {
        const svint16_t a = svld1_s16(pg16,
            (const int16_t *)(const void *)(y + br * M + k));
        const svuint32_t idx = svindex_u32((uint32_t)(9 * k + br), 9);
        svst1_scatter_u32index_u32(pg32,dst32,idx,svreinterpret_u32_s16(a));
      }
    }
    return;
  }

  if (r == 15 || r == 25) {
    int rr, c3, c5, first, second;
    SVE2_ASSERT(mixed_stage_info(p->stage_code[level],
                                        &rr, &c3, &c5, &first, &second) &&
                rr == r && second != 0,
                "Invalid fused SVE2 235 Q15 stage code=%u radix=%d\n",
                (unsigned)p->stage_code[level], r);
    (void)c3; (void)c5;

    /* Fused R3/R5 parent pair. Each component uses its own Q15 scaling
     * and twiddle table, with a local four-complex intermediate tile. */
    const int B = first;
    const int A = second;
    const mixed_parent_twiddle_t *twB = &p->fused_first_tw[level];
    const mixed_parent_twiddle_t *twA = &p->fused_second_tw[level];
    SVE2_ASSERT(twB->radix == B && twB->M == A * M &&
                twA->radix == A && twA->M == M,
                "Bad fused twiddles N=%d B=%d A=%d M=%d\n", N, B, A, M);

    c16_t stage1[25][4] __attribute__((aligned(64)));

    for (int off = 0; off < M; off += 4) {
      const int rem = M - off < 4 ? M - off : 4;
      const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));

      /* First parent B at each child-input subindex a; twiddles use index a*M+off. */
      for (int a = 0; a < A; a++) {
        const int first_off = a * M + off;
        if (B == 3) {
          svint16_t x0 = svld1_s16(pg16, (const int16_t *)(const void *)(src + (0 * A + a) * M + off));
          svint16_t x1 = svld1_s16(pg16, (const int16_t *)(const void *)(src + (1 * A + a) * M + off));
          svint16_t x2 = svld1_s16(pg16, (const int16_t *)(const void *)(src + (2 * A + a) * M + off));
          svint16_t z0, z1, z2;
          sve2_dft3_q15(x0, x1, x2, &z0, &z1, &z2);
          z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT3);
          z1 = sve2_cmul_q15(z1, svld1_s16(pg16, mixed_twiddle_forward(twB, 0) + 2 * first_off));
          z2 = sve2_cmul_q15(z2, svld1_s16(pg16, mixed_twiddle_forward(twB, 1) + 2 * first_off));
          svst1_s16(pg16, (int16_t *)(void *)stage1[a * B + 0], z0);
          svst1_s16(pg16, (int16_t *)(void *)stage1[a * B + 1], z1);
          svst1_s16(pg16, (int16_t *)(void *)stage1[a * B + 2], z2);
        } else {
          SVE2_ASSERT(B == 5, "Invalid fused first radix B=%d\n", B);
          svint16_t x0 = svld1_s16(pg16, (const int16_t *)(const void *)(src + (0 * A + a) * M + off));
          svint16_t x1 = svld1_s16(pg16, (const int16_t *)(const void *)(src + (1 * A + a) * M + off));
          svint16_t x2 = svld1_s16(pg16, (const int16_t *)(const void *)(src + (2 * A + a) * M + off));
          svint16_t x3 = svld1_s16(pg16, (const int16_t *)(const void *)(src + (3 * A + a) * M + off));
          svint16_t x4 = svld1_s16(pg16, (const int16_t *)(const void *)(src + (4 * A + a) * M + off));
          svint16_t z0, z1, z2, z3, z4;
          sve2_dft5_q15(x0, x1, x2, x3, x4, &z0, &z1, &z2, &z3, &z4);
          z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT5);
          z1 = sve2_cmul_q15(z1, svld1_s16(pg16, mixed_twiddle_forward(twB, 0) + 2 * first_off));
          z2 = sve2_cmul_q15(z2, svld1_s16(pg16, mixed_twiddle_forward(twB, 1) + 2 * first_off));
          z3 = sve2_cmul_q15(z3, svld1_s16(pg16, mixed_twiddle_forward(twB, 2) + 2 * first_off));
          z4 = sve2_cmul_q15(z4, svld1_s16(pg16, mixed_twiddle_forward(twB, 3) + 2 * first_off));
          svst1_s16(pg16, (int16_t *)(void *)stage1[a * B + 0], z0);
          svst1_s16(pg16, (int16_t *)(void *)stage1[a * B + 1], z1);
          svst1_s16(pg16, (int16_t *)(void *)stage1[a * B + 2], z2);
          svst1_s16(pg16, (int16_t *)(void *)stage1[a * B + 3], z3);
          svst1_s16(pg16, (int16_t *)(void *)stage1[a * B + 4], z4);
        }
      }

      /* Second parent A uses its own unitary scale and quantized twiddle table. */
      for (int bidx = 0; bidx < B; bidx++) {
        if (A == 3) {
          svint16_t x0 = svld1_s16(pg16, (const int16_t *)(const void *)stage1[0 * B + bidx]);
          svint16_t x1 = svld1_s16(pg16, (const int16_t *)(const void *)stage1[1 * B + bidx]);
          svint16_t x2 = svld1_s16(pg16, (const int16_t *)(const void *)stage1[2 * B + bidx]);
          svint16_t z0, z1, z2;
          sve2_dft3_q15(x0, x1, x2, &z0, &z1, &z2);
          z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT3);
          z1 = sve2_cmul_q15(z1, svld1_s16(pg16, mixed_twiddle_forward(twA, 0) + 2 * off));
          z2 = sve2_cmul_q15(z2, svld1_s16(pg16, mixed_twiddle_forward(twA, 1) + 2 * off));
          const int br0 = bidx + B * 0;
          const int br1 = bidx + B * 1;
          const int br2 = bidx + B * 2;
          svst1_s16(pg16, (int16_t *)(void *)(b + br0 * M + off), z0);
          svst1_s16(pg16, (int16_t *)(void *)(b + br1 * M + off), z1);
          svst1_s16(pg16, (int16_t *)(void *)(b + br2 * M + off), z2);
        } else {
          SVE2_ASSERT(A == 5, "Invalid fused second radix A=%d\n", A);
          svint16_t x0 = svld1_s16(pg16, (const int16_t *)(const void *)stage1[0 * B + bidx]);
          svint16_t x1 = svld1_s16(pg16, (const int16_t *)(const void *)stage1[1 * B + bidx]);
          svint16_t x2 = svld1_s16(pg16, (const int16_t *)(const void *)stage1[2 * B + bidx]);
          svint16_t x3 = svld1_s16(pg16, (const int16_t *)(const void *)stage1[3 * B + bidx]);
          svint16_t x4 = svld1_s16(pg16, (const int16_t *)(const void *)stage1[4 * B + bidx]);
          svint16_t z0, z1, z2, z3, z4;
          sve2_dft5_q15(x0, x1, x2, x3, x4, &z0, &z1, &z2, &z3, &z4);
          z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT5);
          z1 = sve2_cmul_q15(z1, svld1_s16(pg16, mixed_twiddle_forward(twA, 0) + 2 * off));
          z2 = sve2_cmul_q15(z2, svld1_s16(pg16, mixed_twiddle_forward(twA, 1) + 2 * off));
          z3 = sve2_cmul_q15(z3, svld1_s16(pg16, mixed_twiddle_forward(twA, 2) + 2 * off));
          z4 = sve2_cmul_q15(z4, svld1_s16(pg16, mixed_twiddle_forward(twA, 3) + 2 * off));
          const int br0 = bidx + B * 0;
          const int br1 = bidx + B * 1;
          const int br2 = bidx + B * 2;
          const int br3 = bidx + B * 3;
          const int br4 = bidx + B * 4;
          svst1_s16(pg16, (int16_t *)(void *)(b + br0 * M + off), z0);
          svst1_s16(pg16, (int16_t *)(void *)(b + br1 * M + off), z1);
          svst1_s16(pg16, (int16_t *)(void *)(b + br2 * M + off), z2);
          svst1_s16(pg16, (int16_t *)(void *)(b + br3 * M + off), z3);
          svst1_s16(pg16, (int16_t *)(void *)(b + br4 * M + off), z4);
        }
      }
    }

    for (int br = 0; br < r; br++)
      sve2_235_exec_q15_rec(p, level + 1,
                                 b + br * M, y + br * M, child_work);

    uint32_t *dst32 = (uint32_t *)(void *)dst;
    for (int k = 0; k < M; k += 4) {
      const int rem = M - k < 4 ? M - k : 4;
      const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));
      const svbool_t pg32 = svwhilelt_b32((uint64_t)0, (uint64_t)rem);
      for (int br = 0; br < r; br++) {
        svint16_t a = svld1_s16(pg16, (const int16_t *)(const void *)(y + br * M + k));
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
      svint16_t x0 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 0 * M + off));
      svint16_t x1 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 1 * M + off));
      svint16_t x2 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 2 * M + off));
      svint16_t z0, z1, z2;
      sve2_dft3_q15(x0, x1, x2, &z0, &z1, &z2);
      z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT3);
      z1 = sve2_cmul_q15(z1, svld1_s16(pg16, mixed_twiddle_forward(tw, 0) + 2 * off));
      z2 = sve2_cmul_q15(z2, svld1_s16(pg16, mixed_twiddle_forward(tw, 1) + 2 * off));
      svst1_s16(pg16, (int16_t *)(void *)(b + 0 * M + off), z0);
      svst1_s16(pg16, (int16_t *)(void *)(b + 1 * M + off), z1);
      svst1_s16(pg16, (int16_t *)(void *)(b + 2 * M + off), z2);
    }
    for (int br = 0; br < 3; br++)
      sve2_235_exec_q15_rec(p, level + 1,
                                 b + br * M, y + br * M, child_work);
    for (int k = 0; k < M; k += 4) {
      const int rem = M - k < 4 ? M - k : 4;
      const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));
      const svbool_t pg32 = svwhilelt_b32((uint64_t)0, (uint64_t)rem);
      const svint16_t a0 = svld1_s16(pg16, (const int16_t *)(const void *)(y + 0 * M + k));
      const svint16_t a1 = svld1_s16(pg16, (const int16_t *)(const void *)(y + 1 * M + k));
      const svint16_t a2 = svld1_s16(pg16, (const int16_t *)(const void *)(y + 2 * M + k));
      svst3_u32(pg32, (uint32_t *)(void *)(dst + 3 * k),
                 svcreate3_u32(svreinterpret_u32_s16(a0),
                               svreinterpret_u32_s16(a1),
                               svreinterpret_u32_s16(a2)));
    }
    return;
  }

  SVE2_ASSERT(r == 5, "Invalid SVE2 235 radix %d\n", r);
  for (int off = 0; off < M; off += 4) {
    const int rem = M - off < 4 ? M - off : 4;
    const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));
    svint16_t x0 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 0 * M + off));
    svint16_t x1 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 1 * M + off));
    svint16_t x2 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 2 * M + off));
    svint16_t x3 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 3 * M + off));
    svint16_t x4 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 4 * M + off));
    svint16_t z0, z1, z2, z3, z4;
    sve2_dft5_q15(x0, x1, x2, x3, x4, &z0, &z1, &z2, &z3, &z4);
    z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT5);
    z1 = sve2_cmul_q15(z1, svld1_s16(pg16, mixed_twiddle_forward(tw, 0) + 2 * off));
    z2 = sve2_cmul_q15(z2, svld1_s16(pg16, mixed_twiddle_forward(tw, 1) + 2 * off));
    z3 = sve2_cmul_q15(z3, svld1_s16(pg16, mixed_twiddle_forward(tw, 2) + 2 * off));
    z4 = sve2_cmul_q15(z4, svld1_s16(pg16, mixed_twiddle_forward(tw, 3) + 2 * off));
    svst1_s16(pg16, (int16_t *)(void *)(b + 0 * M + off), z0);
    svst1_s16(pg16, (int16_t *)(void *)(b + 1 * M + off), z1);
    svst1_s16(pg16, (int16_t *)(void *)(b + 2 * M + off), z2);
    svst1_s16(pg16, (int16_t *)(void *)(b + 3 * M + off), z3);
    svst1_s16(pg16, (int16_t *)(void *)(b + 4 * M + off), z4);
  }
  for (int br = 0; br < 5; br++)
    sve2_235_exec_q15_rec(p, level + 1,
                               b + br * M, y + br * M, child_work);
  uint32_t *dst32 = (uint32_t *)(void *)dst;
  for (int k = 0; k < M; k += 4) {
    const int rem = M - k < 4 ? M - k : 4;
    const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));
    const svbool_t pg32 = svwhilelt_b32((uint64_t)0, (uint64_t)rem);
    for (int br = 0; br < 5; br++) {
      const svint16_t a = svld1_s16(pg16, (const int16_t *)(const void *)(y + br * M + k));
      const svuint32_t idx = svindex_u32((uint32_t)(5 * k + br), 5);
      svst1_scatter_u32index_u32(pg32, dst32, idx, svreinterpret_u32_s16(a));
    }
  }
}

SVE2_TARGET static void sve2_235_exec_q15_rec_inverse(
    const mixed_plan_t *p, int level,
    const c16_t *src, c16_t *dst, c16_t *work)
{
  if (level == p->depth) {
    mixed_inverse_leaf_q15(p, src, dst);
    return;
  }

  const mixed_parent_twiddle_t *tw = &p->tw[level];
  const int N = tw->N;
  const int M = tw->M;
  const int r = tw->radix;
  c16_t *b = work;
  c16_t *y = work + N;
  c16_t *child_work = work + 2 * N;

  /* Two consecutive radix-9 stages keep all 81 child branches in
   * branch-major order until the final scatter to natural output order. */
  if (r == 9 && level + 1 < p->depth &&
      p->stage_code[level + 1] == MIXED_STAGE_R9) {
    const mixed_parent_twiddle_t *tw_inner = &p->tw[level + 1];
    const int M1 = M;
    const int M2 = tw_inner->M;
    SVE2_ASSERT(tw_inner->N == M1 && tw_inner->radix == 9 && M1 == 9 * M2,
                "Bad SVE2 R9xR9 topology level=%d N=%d M1=%d M2=%d\\n",
                level, N, M1, M2);

    sve2_235_r9_prepare_q15_inverse(p, level, src, b);

    /* Reuse the next level's workspace as one branch-local inner radix-9 input.
     * combined_y uses the outer y region as 81 branch-major child outputs. */
    c16_t *combined_y = y;
    c16_t *inner_b = child_work;
    c16_t *grand_work = inner_b + M1;

    for (int outer = 0; outer < 9; outer++) {
      sve2_235_r9_prepare_q15_inverse(p, level + 1,
                                   b + outer * M1, inner_b);
      for (int inner = 0; inner < 9; inner++) {
        const int combined = 9 * inner + outer;
        sve2_235_exec_q15_rec_inverse(p, level + 2,
                                   inner_b + inner * M2,
                                   combined_y + combined * M2,
                                   grand_work);
      }
    }

    /* Natural output index is
     *   81*k + 9*inner + outer.
     * combined_y is already laid out in that branch order. */
    uint32_t *dst32 = (uint32_t *)(void *)dst;
    for (int k = 0; k < M2; k += 4) {
      const int rem = M2 - k < 4 ? M2 - k : 4;
      const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));
      const svbool_t pg32 = svwhilelt_b32((uint64_t)0, (uint64_t)rem);
      for (int br = 0; br < 81; br++) {
        const svint16_t a = svld1_s16(pg16,
            (const int16_t *)(const void *)(combined_y + br * M2 + k));
        const svuint32_t idx = svindex_u32((uint32_t)(81 * k + br), 81);
        svst1_scatter_u32index_u32(pg32, dst32, idx, svreinterpret_u32_s16(a));
      }
    }
    return;
  }

  if (r == 9) {
    sve2_235_r9_prepare_q15_inverse(p, level, src, b);

    for (int br = 0; br < 9; br++)
      sve2_235_exec_q15_rec_inverse(p, level + 1,
                                 b + br * M, y + br * M, child_work);

    uint32_t *dst32 = (uint32_t *)(void *)dst;
    for (int k = 0; k < M; k += 4) {
      const int rem = M - k < 4 ? M - k : 4;
      const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));
      const svbool_t pg32 = svwhilelt_b32((uint64_t)0, (uint64_t)rem);
      for (int br = 0; br < 9; br++) {
        const svint16_t a = svld1_s16(pg16,
            (const int16_t *)(const void *)(y + br * M + k));
        const svuint32_t idx = svindex_u32((uint32_t)(9 * k + br), 9);
        svst1_scatter_u32index_u32(pg32,dst32,idx,svreinterpret_u32_s16(a));
      }
    }
    return;
  }

  if (r == 15 || r == 25) {
    int rr, c3, c5, first, second;
    SVE2_ASSERT(mixed_stage_info(p->stage_code[level],
                                        &rr, &c3, &c5, &first, &second) &&
                rr == r && second != 0,
                "Invalid fused SVE2 235 Q15 stage code=%u radix=%d\n",
                (unsigned)p->stage_code[level], r);
    (void)c3; (void)c5;

    /* Fused R3/R5 parent pair. Each component uses its own Q15 scaling
     * and twiddle table, with a local four-complex intermediate tile. */
    const int B = first;
    const int A = second;
    const mixed_parent_twiddle_t *twB = &p->fused_first_tw[level];
    const mixed_parent_twiddle_t *twA = &p->fused_second_tw[level];
    SVE2_ASSERT(twB->radix == B && twB->M == A * M &&
                twA->radix == A && twA->M == M,
                "Bad fused twiddles N=%d B=%d A=%d M=%d\n", N, B, A, M);

    c16_t stage1[25][4] __attribute__((aligned(64)));

    for (int off = 0; off < M; off += 4) {
      const int rem = M - off < 4 ? M - off : 4;
      const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));

      /* First parent B at each child-input subindex a; twiddles use index a*M+off. */
      for (int a = 0; a < A; a++) {
        const int first_off = a * M + off;
        if (B == 3) {
          svint16_t x0 = svld1_s16(pg16, (const int16_t *)(const void *)(src + (0 * A + a) * M + off));
          svint16_t x1 = svld1_s16(pg16, (const int16_t *)(const void *)(src + (1 * A + a) * M + off));
          svint16_t x2 = svld1_s16(pg16, (const int16_t *)(const void *)(src + (2 * A + a) * M + off));
          svint16_t z0, z1, z2;
          sve2_idft3_q15(x0, x1, x2, &z0, &z1, &z2);
          z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT3);
          z1 = sve2_cmul_q15(z1, svld1_s16(pg16, mixed_twiddle_inverse(twB, 0) + 2 * first_off));
          z2 = sve2_cmul_q15(z2, svld1_s16(pg16, mixed_twiddle_inverse(twB, 1) + 2 * first_off));
          svst1_s16(pg16, (int16_t *)(void *)stage1[a * B + 0], z0);
          svst1_s16(pg16, (int16_t *)(void *)stage1[a * B + 1], z1);
          svst1_s16(pg16, (int16_t *)(void *)stage1[a * B + 2], z2);
        } else {
          SVE2_ASSERT(B == 5, "Invalid fused first radix B=%d\n", B);
          svint16_t x0 = svld1_s16(pg16, (const int16_t *)(const void *)(src + (0 * A + a) * M + off));
          svint16_t x1 = svld1_s16(pg16, (const int16_t *)(const void *)(src + (1 * A + a) * M + off));
          svint16_t x2 = svld1_s16(pg16, (const int16_t *)(const void *)(src + (2 * A + a) * M + off));
          svint16_t x3 = svld1_s16(pg16, (const int16_t *)(const void *)(src + (3 * A + a) * M + off));
          svint16_t x4 = svld1_s16(pg16, (const int16_t *)(const void *)(src + (4 * A + a) * M + off));
          svint16_t z0, z1, z2, z3, z4;
          sve2_idft5_q15(x0, x1, x2, x3, x4, &z0, &z1, &z2, &z3, &z4);
          z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT5);
          z1 = sve2_cmul_q15(z1, svld1_s16(pg16, mixed_twiddle_inverse(twB, 0) + 2 * first_off));
          z2 = sve2_cmul_q15(z2, svld1_s16(pg16, mixed_twiddle_inverse(twB, 1) + 2 * first_off));
          z3 = sve2_cmul_q15(z3, svld1_s16(pg16, mixed_twiddle_inverse(twB, 2) + 2 * first_off));
          z4 = sve2_cmul_q15(z4, svld1_s16(pg16, mixed_twiddle_inverse(twB, 3) + 2 * first_off));
          svst1_s16(pg16, (int16_t *)(void *)stage1[a * B + 0], z0);
          svst1_s16(pg16, (int16_t *)(void *)stage1[a * B + 1], z1);
          svst1_s16(pg16, (int16_t *)(void *)stage1[a * B + 2], z2);
          svst1_s16(pg16, (int16_t *)(void *)stage1[a * B + 3], z3);
          svst1_s16(pg16, (int16_t *)(void *)stage1[a * B + 4], z4);
        }
      }

      /* Second parent A uses its own unitary scale and quantized twiddle table. */
      for (int bidx = 0; bidx < B; bidx++) {
        if (A == 3) {
          svint16_t x0 = svld1_s16(pg16, (const int16_t *)(const void *)stage1[0 * B + bidx]);
          svint16_t x1 = svld1_s16(pg16, (const int16_t *)(const void *)stage1[1 * B + bidx]);
          svint16_t x2 = svld1_s16(pg16, (const int16_t *)(const void *)stage1[2 * B + bidx]);
          svint16_t z0, z1, z2;
          sve2_idft3_q15(x0, x1, x2, &z0, &z1, &z2);
          z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT3);
          z1 = sve2_cmul_q15(z1, svld1_s16(pg16, mixed_twiddle_inverse(twA, 0) + 2 * off));
          z2 = sve2_cmul_q15(z2, svld1_s16(pg16, mixed_twiddle_inverse(twA, 1) + 2 * off));
          const int br0 = bidx + B * 0;
          const int br1 = bidx + B * 1;
          const int br2 = bidx + B * 2;
          svst1_s16(pg16, (int16_t *)(void *)(b + br0 * M + off), z0);
          svst1_s16(pg16, (int16_t *)(void *)(b + br1 * M + off), z1);
          svst1_s16(pg16, (int16_t *)(void *)(b + br2 * M + off), z2);
        } else {
          SVE2_ASSERT(A == 5, "Invalid fused second radix A=%d\n", A);
          svint16_t x0 = svld1_s16(pg16, (const int16_t *)(const void *)stage1[0 * B + bidx]);
          svint16_t x1 = svld1_s16(pg16, (const int16_t *)(const void *)stage1[1 * B + bidx]);
          svint16_t x2 = svld1_s16(pg16, (const int16_t *)(const void *)stage1[2 * B + bidx]);
          svint16_t x3 = svld1_s16(pg16, (const int16_t *)(const void *)stage1[3 * B + bidx]);
          svint16_t x4 = svld1_s16(pg16, (const int16_t *)(const void *)stage1[4 * B + bidx]);
          svint16_t z0, z1, z2, z3, z4;
          sve2_idft5_q15(x0, x1, x2, x3, x4, &z0, &z1, &z2, &z3, &z4);
          z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT5);
          z1 = sve2_cmul_q15(z1, svld1_s16(pg16, mixed_twiddle_inverse(twA, 0) + 2 * off));
          z2 = sve2_cmul_q15(z2, svld1_s16(pg16, mixed_twiddle_inverse(twA, 1) + 2 * off));
          z3 = sve2_cmul_q15(z3, svld1_s16(pg16, mixed_twiddle_inverse(twA, 2) + 2 * off));
          z4 = sve2_cmul_q15(z4, svld1_s16(pg16, mixed_twiddle_inverse(twA, 3) + 2 * off));
          const int br0 = bidx + B * 0;
          const int br1 = bidx + B * 1;
          const int br2 = bidx + B * 2;
          const int br3 = bidx + B * 3;
          const int br4 = bidx + B * 4;
          svst1_s16(pg16, (int16_t *)(void *)(b + br0 * M + off), z0);
          svst1_s16(pg16, (int16_t *)(void *)(b + br1 * M + off), z1);
          svst1_s16(pg16, (int16_t *)(void *)(b + br2 * M + off), z2);
          svst1_s16(pg16, (int16_t *)(void *)(b + br3 * M + off), z3);
          svst1_s16(pg16, (int16_t *)(void *)(b + br4 * M + off), z4);
        }
      }
    }

    for (int br = 0; br < r; br++)
      sve2_235_exec_q15_rec_inverse(p, level + 1,
                                 b + br * M, y + br * M, child_work);

    uint32_t *dst32 = (uint32_t *)(void *)dst;
    for (int k = 0; k < M; k += 4) {
      const int rem = M - k < 4 ? M - k : 4;
      const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));
      const svbool_t pg32 = svwhilelt_b32((uint64_t)0, (uint64_t)rem);
      for (int br = 0; br < r; br++) {
        svint16_t a = svld1_s16(pg16, (const int16_t *)(const void *)(y + br * M + k));
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
      svint16_t x0 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 0 * M + off));
      svint16_t x1 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 1 * M + off));
      svint16_t x2 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 2 * M + off));
      svint16_t z0, z1, z2;
      sve2_idft3_q15(x0, x1, x2, &z0, &z1, &z2);
      z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT3);
      z1 = sve2_cmul_q15(z1, svld1_s16(pg16, mixed_twiddle_inverse(tw, 0) + 2 * off));
      z2 = sve2_cmul_q15(z2, svld1_s16(pg16, mixed_twiddle_inverse(tw, 1) + 2 * off));
      svst1_s16(pg16, (int16_t *)(void *)(b + 0 * M + off), z0);
      svst1_s16(pg16, (int16_t *)(void *)(b + 1 * M + off), z1);
      svst1_s16(pg16, (int16_t *)(void *)(b + 2 * M + off), z2);
    }
    for (int br = 0; br < 3; br++)
      sve2_235_exec_q15_rec_inverse(p, level + 1,
                                 b + br * M, y + br * M, child_work);
    for (int k = 0; k < M; k += 4) {
      const int rem = M - k < 4 ? M - k : 4;
      const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));
      const svbool_t pg32 = svwhilelt_b32((uint64_t)0, (uint64_t)rem);
      const svint16_t a0 = svld1_s16(pg16, (const int16_t *)(const void *)(y + 0 * M + k));
      const svint16_t a1 = svld1_s16(pg16, (const int16_t *)(const void *)(y + 1 * M + k));
      const svint16_t a2 = svld1_s16(pg16, (const int16_t *)(const void *)(y + 2 * M + k));
      svst3_u32(pg32, (uint32_t *)(void *)(dst + 3 * k),
                 svcreate3_u32(svreinterpret_u32_s16(a0),
                               svreinterpret_u32_s16(a1),
                               svreinterpret_u32_s16(a2)));
    }
    return;
  }

  SVE2_ASSERT(r == 5, "Invalid SVE2 235 radix %d\n", r);
  for (int off = 0; off < M; off += 4) {
    const int rem = M - off < 4 ? M - off : 4;
    const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));
    svint16_t x0 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 0 * M + off));
    svint16_t x1 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 1 * M + off));
    svint16_t x2 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 2 * M + off));
    svint16_t x3 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 3 * M + off));
    svint16_t x4 = svld1_s16(pg16, (const int16_t *)(const void *)(src + 4 * M + off));
    svint16_t z0, z1, z2, z3, z4;
    sve2_idft5_q15(x0, x1, x2, x3, x4, &z0, &z1, &z2, &z3, &z4);
    z0 = sve2_q15_real_mul(z0, Q15_INV_SQRT5);
    z1 = sve2_cmul_q15(z1, svld1_s16(pg16, mixed_twiddle_inverse(tw, 0) + 2 * off));
    z2 = sve2_cmul_q15(z2, svld1_s16(pg16, mixed_twiddle_inverse(tw, 1) + 2 * off));
    z3 = sve2_cmul_q15(z3, svld1_s16(pg16, mixed_twiddle_inverse(tw, 2) + 2 * off));
    z4 = sve2_cmul_q15(z4, svld1_s16(pg16, mixed_twiddle_inverse(tw, 3) + 2 * off));
    svst1_s16(pg16, (int16_t *)(void *)(b + 0 * M + off), z0);
    svst1_s16(pg16, (int16_t *)(void *)(b + 1 * M + off), z1);
    svst1_s16(pg16, (int16_t *)(void *)(b + 2 * M + off), z2);
    svst1_s16(pg16, (int16_t *)(void *)(b + 3 * M + off), z3);
    svst1_s16(pg16, (int16_t *)(void *)(b + 4 * M + off), z4);
  }
  for (int br = 0; br < 5; br++)
    sve2_235_exec_q15_rec_inverse(p, level + 1,
                               b + br * M, y + br * M, child_work);
  uint32_t *dst32 = (uint32_t *)(void *)dst;
  for (int k = 0; k < M; k += 4) {
    const int rem = M - k < 4 ? M - k : 4;
    const svbool_t pg16 = svwhilelt_b16((uint64_t)0, (uint64_t)(2 * rem));
    const svbool_t pg32 = svwhilelt_b32((uint64_t)0, (uint64_t)rem);
    for (int br = 0; br < 5; br++) {
      const svint16_t a = svld1_s16(pg16, (const int16_t *)(const void *)(y + br * M + k));
      const svuint32_t idx = svindex_u32((uint32_t)(5 * k + br), 5);
      svst1_scatter_u32index_u32(pg32, dst32, idx, svreinterpret_u32_s16(a));
    }
  }
}


/* =========================================================
 * Q15 twiddles used by the native DFT16/32/64/128 leaves.
 * ========================================================= */
typedef struct {
  neon_m128i C16_RE_RE_q15_native[4] __attribute__((aligned(64)));
  neon_m128i C16_IM_SIGNED_q15_native[4] __attribute__((aligned(64)));
  neon_m128i C16_RE_RE_q15_native_inverse[4] __attribute__((aligned(64)));
  neon_m128i C16_IM_SIGNED_q15_native_inverse[4] __attribute__((aligned(64)));
  neon_m128i C32_RE_RE_q15_native[4][2] __attribute__((aligned(64)));
  neon_m128i C32_IM_SIGNED_q15_native[4][2] __attribute__((aligned(64)));

  cq15x8_t C64_RE_RE_q15[64] __attribute__((aligned(64)));
  cq15x8_t C64_IM_SIGNED_q15[64] __attribute__((aligned(64)));
  cq15x8_t C64_RE_RE_q15_inverse[64] __attribute__((aligned(64)));
  cq15x8_t C64_IM_SIGNED_q15_inverse[64] __attribute__((aligned(64)));

  cq15x8_t W128_RE_RE_q15[8] __attribute__((aligned(64)));
  cq15x8_t W128_IM_SIGNED_q15[8] __attribute__((aligned(64)));
  cq15x8_t W128_RE_RE_q15_inverse[8] __attribute__((aligned(64)));
  cq15x8_t W128_IM_SIGNED_q15_inverse[8] __attribute__((aligned(64)));

} dft64_q15_twiddle_t;

static dft64_q15_twiddle_t g_dft64_q15_tw;


/* Broadcast W64^(r*q)/8 tables for eight DFT64 transforms in parallel. */
static cq15x8_t g_dft64_batch_re_re_q15[2][8][8] __attribute__((aligned(64)));
static cq15x8_t g_dft64_batch_im_signed_q15[2][8][8] __attribute__((aligned(64)));

/*
 * Twiddles for the single-transform NEON Q15 DFT64 radix-4 path.
 *
 * [branch-1][block] packs four consecutive k values:
 *   [wr0,wr0, wr1,wr1, wr2,wr2, wr3,wr3]
 * and the signed imaginary multiplier layout expected by
 * complex_mul4_prepack_q15_128().
 */
static neon_m128i g_dft64_r4_re_re_q15[3][4] __attribute__((aligned(64)));
static neon_m128i g_dft64_r4_im_signed_q15[3][4] __attribute__((aligned(64)));


/* Q15 twiddles use 32768 scaling followed by symmetric saturation. */
static inline int16_t q15_twiddle_32768(float x)
{
  int v = (int)lrintf(x * 32768.0f);
  if (v > 32767) v = 32767;
  if (v < -32767) v = -32767;
  return (int16_t)v;
}

static void init_native_q15_leaf_twiddles(void)
{
  dft64_q15_twiddle_t *tw = &g_dft64_q15_tw;

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
        re_re[2*r+0] = wr; re_re[2*r+1] = wr;
        im_signed[2*r+0] = (int16_t)-wi; im_signed[2*r+1] = wi;
      }
      const neon_m128i vr = neon128_load_i((const neon_m128i *)(const void *)re_re);
      const neon_m128i vi = neon128_load_i((const neon_m128i *)(const void *)im_signed);
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
  for (int k = 0; k < 4; k++) {
    for (int half = 0; half < 2; half++) {
      int16_t re_re[8] __attribute__((aligned(16)));
      int16_t im_signed[8] __attribute__((aligned(16)));
      for (int lane = 0; lane < 4; lane++) {
        const int r = 4 * half + lane;
        const float a = -2.0f * (float)M_PI * (float)(k * r) / 32.0f;
        const int16_t wr = q15_twiddle_32768(cosf(a));
        const int16_t wi = q15_twiddle_32768(sinf(a));
        re_re[2*lane+0] = wr; re_re[2*lane+1] = wr;
        im_signed[2*lane+0] = (int16_t)-wi; im_signed[2*lane+1] = wi;
      }
      tw->C32_RE_RE_q15_native[k][half] =
          neon128_load_i((const neon_m128i *)(const void *)re_re);
      tw->C32_IM_SIGNED_q15_native[k][half] =
          neon128_load_i((const neon_m128i *)(const void *)im_signed);
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
        re_re[2*r+0] = wr; re_re[2*r+1] = wr;
        im_signed[2*r+0] = (int16_t)-wi; im_signed[2*r+1] = wi;
      }
      cq15x8_t vr = cq15x8_load((const cq15x8_t *)(const void *)re_re);
      cq15x8_t vi = cq15x8_load((const cq15x8_t *)(const void *)im_signed);
      if (dir_slot == 0) {
        tw->C64_RE_RE_q15[k] = vr;
        tw->C64_IM_SIGNED_q15[k] = vi;
      } else {
        tw->C64_RE_RE_q15_inverse[k] = vr;
        tw->C64_IM_SIGNED_q15_inverse[k] = vi;
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
        re_re[2*r+0] = wr; re_re[2*r+1] = wr;
        im_signed[2*r+0] = (int16_t)-wi; im_signed[2*r+1] = wi;
      }
      cq15x8_t vr = cq15x8_load((const cq15x8_t *)(const void *)re_re);
      cq15x8_t vi = cq15x8_load((const cq15x8_t *)(const void *)im_signed);
      if (dir_slot == 0) {
        tw->W128_RE_RE_q15[b] = vr;
        tw->W128_IM_SIGNED_q15[b] = vi;
      } else {
        tw->W128_RE_RE_q15_inverse[b] = vr;
        tw->W128_IM_SIGNED_q15_inverse[b] = vi;
      }
    }

    /* Batched radix-8 DFT64 coefficients, also scaled by 1/8. */
    for (int r = 0; r < 8; r++) {
      for (int q = 0; q < 8; q++) {
        const float a = sign * 2.0f * (float)M_PI * (float)(r * q) / 64.0f;
        const int16_t wr = q15_twiddle_32768(cosf(a) * 0.125f);
        const int16_t wi = q15_twiddle_32768(sinf(a) * 0.125f);
        g_dft64_batch_re_re_q15[dir_slot][r][q] = cq15x8_set1_i16(wr);
        g_dft64_batch_im_signed_q15[dir_slot][r][q] =
            cq15x8_setr_i16(-wi,wi,-wi,wi,-wi,wi,-wi,wi,
                              -wi,wi,-wi,wi,-wi,wi,-wi,wi);
      }
    }

    /* Native forward radix-4 DFT64 outer-stage coefficients, scale fused as 1/2. */
    if (dir_slot == 0) {
      for (int branch = 1; branch < 4; branch++) {
        for (int block = 0; block < 4; block++) {
          int16_t re_re[8] __attribute__((aligned(16)));
          int16_t im_signed[8] __attribute__((aligned(16)));
          for (int lane = 0; lane < 4; lane++) {
            const int k = 4 * block + lane;
            const float a = sign * 2.0f * (float)M_PI * (float)(branch * k) / 64.0f;
            const int16_t wr = q15_twiddle_32768(0.5f * cosf(a));
            const int16_t wi = q15_twiddle_32768(0.5f * sinf(a));
            re_re[2*lane+0] = wr; re_re[2*lane+1] = wr;
            im_signed[2*lane+0] = (int16_t)-wi; im_signed[2*lane+1] = wi;
          }
          g_dft64_r4_re_re_q15[branch-1][block] =
              neon128_load_i((const neon_m128i *)(const void *)re_re);
          g_dft64_r4_im_signed_q15[branch-1][block] =
              neon128_load_i((const neon_m128i *)(const void *)im_signed);
        }
      }
    }
  }

}


/* Eight-complex NEON Q15 helpers and DFT64 batch kernels. */

static inline cq15x8_t swap_complex_pairs_q15x8(cq15x8_t a)
{
  return cq15x8_make(vrev32q_s16(a.val[0]), vrev32q_s16(a.val[1]));
}

static inline cq15x8_t mul_j_q15x8(cq15x8_t z)
{
  const uint16x8_t neg_re = {0xffff, 0, 0xffff, 0, 0xffff, 0, 0xffff, 0};
  const int16x8_t lo = vrev32q_s16(z.val[0]);
  const int16x8_t hi = vrev32q_s16(z.val[1]);
  return cq15x8_make(vbslq_s16(neg_re, vqnegq_s16(lo), lo),
                     vbslq_s16(neg_re, vqnegq_s16(hi), hi));
}

static inline cq15x8_t mul_minus_j_q15x8(cq15x8_t z)
{
  const uint16x8_t neg_im = {0, 0xffff, 0, 0xffff, 0, 0xffff, 0, 0xffff};
  const int16x8_t lo = vrev32q_s16(z.val[0]);
  const int16x8_t hi = vrev32q_s16(z.val[1]);
  return cq15x8_make(vbslq_s16(neg_im, vqnegq_s16(lo), lo),
                     vbslq_s16(neg_im, vqnegq_s16(hi), hi));
}

static inline cq15x8_t mul_minus_j_dir_q15x8(cq15x8_t z, dft_dir_t dir)
{
  return (dir == DFT_DIR_FORWARD) ? mul_minus_j_q15x8(z) : mul_j_q15x8(z);
}

static inline cq15x8_t mul_plus_j_dir_q15x8(cq15x8_t z, dft_dir_t dir)
{
  return (dir == DFT_DIR_FORWARD) ? mul_j_q15x8(z) : mul_minus_j_q15x8(z);
}

static inline void
transpose8_complex_q15x8(cq15x8_t *r0, cq15x8_t *r1, cq15x8_t *r2, cq15x8_t *r3, cq15x8_t *r4, cq15x8_t *r5, cq15x8_t *r6, cq15x8_t *r7)
{
  const cq15x8_t a = *r0;
  const cq15x8_t b = *r1;
  const cq15x8_t c = *r2;
  const cq15x8_t d = *r3;
  const cq15x8_t e = *r4;
  const cq15x8_t f = *r5;
  const cq15x8_t g = *r6;
  const cq15x8_t h = *r7;

  const cq15x8_t t0 = cq15x8_unpacklo_i32(a, b);
  const cq15x8_t t1 = cq15x8_unpackhi_i32(a, b);
  const cq15x8_t t2 = cq15x8_unpacklo_i32(c, d);
  const cq15x8_t t3 = cq15x8_unpackhi_i32(c, d);
  const cq15x8_t t4 = cq15x8_unpacklo_i32(e, f);
  const cq15x8_t t5 = cq15x8_unpackhi_i32(e, f);
  const cq15x8_t t6 = cq15x8_unpacklo_i32(g, h);
  const cq15x8_t t7 = cq15x8_unpackhi_i32(g, h);

  const cq15x8_t s0 = cq15x8_unpacklo_i64(t0, t2);
  const cq15x8_t s1 = cq15x8_unpackhi_i64(t0, t2);
  const cq15x8_t s2 = cq15x8_unpacklo_i64(t1, t3);
  const cq15x8_t s3 = cq15x8_unpackhi_i64(t1, t3);

  const cq15x8_t s4 = cq15x8_unpacklo_i64(t4, t6);
  const cq15x8_t s5 = cq15x8_unpackhi_i64(t4, t6);
  const cq15x8_t s6 = cq15x8_unpacklo_i64(t5, t7);
  const cq15x8_t s7 = cq15x8_unpackhi_i64(t5, t7);

  *r0 = cq15x8_select_halves(s0, s4, 0x20);
  *r1 = cq15x8_select_halves(s1, s5, 0x20);
  *r2 = cq15x8_select_halves(s2, s6, 0x20);
  *r3 = cq15x8_select_halves(s3, s7, 0x20);

  *r4 = cq15x8_select_halves(s0, s4, 0x31);
  *r5 = cq15x8_select_halves(s1, s5, 0x31);
  *r6 = cq15x8_select_halves(s2, s6, 0x31);
  *r7 = cq15x8_select_halves(s3, s7, 0x31);
}

static inline cq15x8_t complex_mul8_prepack_q15(cq15x8_t a, cq15x8_t w_re_re, cq15x8_t w_im_signed)
{
  const cq15x8_t a_swapped = swap_complex_pairs_q15x8(a);

  const cq15x8_t prod_re = cq15x8_mulhrs_i16(a, w_re_re);
  const cq15x8_t prod_im = cq15x8_mulhrs_i16(a_swapped, w_im_signed);

  return cq15x8_adds_i16(prod_re, prod_im);
}

static inline void dft8x8_q15_dir(const cq15x8_t x0,
                                         const cq15x8_t x1,
                                         const cq15x8_t x2,
                                         const cq15x8_t x3,
                                         const cq15x8_t x4,
                                         const cq15x8_t x5,
                                         const cq15x8_t x6,
                                         const cq15x8_t x7,
                                         cq15x8_t *Y0,
                                         cq15x8_t *Y1,
                                         cq15x8_t *Y2,
                                         cq15x8_t *Y3,
                                         cq15x8_t *Y4,
                                         cq15x8_t *Y5,
                                         cq15x8_t *Y6,
                                         cq15x8_t *Y7,
                                         dft_dir_t dir)
{
  const cq15x8_t c = cq15x8_set1_i16(Q15_INV_SQRT2);

  const cq15x8_t s04 = cq15x8_adds_i16(x0, x4);
  const cq15x8_t d04 = cq15x8_subs_i16(x0, x4);

  const cq15x8_t s15 = cq15x8_adds_i16(x1, x5);
  const cq15x8_t d15 = cq15x8_subs_i16(x1, x5);

  const cq15x8_t s26 = cq15x8_adds_i16(x2, x6);
  const cq15x8_t d26 = cq15x8_subs_i16(x2, x6);

  const cq15x8_t s37 = cq15x8_adds_i16(x3, x7);
  const cq15x8_t d37 = cq15x8_subs_i16(x3, x7);

  const cq15x8_t s02 = cq15x8_adds_i16(s04, s26);
  const cq15x8_t d02 = cq15x8_subs_i16(s04, s26);

  const cq15x8_t s13 = cq15x8_adds_i16(s15, s37);
  const cq15x8_t d13 = cq15x8_subs_i16(s15, s37);

  *Y0 = cq15x8_adds_i16(s02, s13);
  *Y4 = cq15x8_subs_i16(s02, s13);

  *Y2 = cq15x8_adds_i16(d02, mul_minus_j_dir_q15x8(d13, dir));
  *Y6 = cq15x8_adds_i16(d02, mul_plus_j_dir_q15x8(d13, dir));

  const cq15x8_t p = cq15x8_adds_i16(d15, d37);
  const cq15x8_t q = cq15x8_subs_i16(d15, d37);

  const cq15x8_t d26_mj = mul_minus_j_dir_q15x8(d26, dir);
  const cq15x8_t d26_pj = mul_plus_j_dir_q15x8(d26, dir);

  const cq15x8_t base_mj = cq15x8_adds_i16(d04, d26_mj);
  const cq15x8_t base_pj = cq15x8_adds_i16(d04, d26_pj);

  const cq15x8_t t1_arg = cq15x8_adds_i16(q, mul_minus_j_dir_q15x8(p, dir));
  const cq15x8_t t3_arg = cq15x8_adds_i16(q, mul_plus_j_dir_q15x8(p, dir));

  const cq15x8_t t1 = cq15x8_mulhrs_i16(c, t1_arg);
  const cq15x8_t t3 = cq15x8_mulhrs_i16(c, t3_arg);

  *Y1 = cq15x8_adds_i16(base_mj, t1);
  *Y5 = cq15x8_subs_i16(base_mj, t1);

  *Y7 = cq15x8_adds_i16(base_pj, t3);
  *Y3 = cq15x8_subs_i16(base_pj, t3);
}


/*
 * Eight independent DFT64 transforms in parallel, with the final DFT8 stage
 * stored directly into the enclosing transform's natural output layout.
 *
 * For local DFT64 bin k, the NEON vector contains eight enclosing-radix bins
 * and is written at:
 *
 *   dst[output_radix * k + output_offset].
 *
 * The final DFT8 stage writes directly to the enclosing transform layout,
 * without a separate Y[64] result buffer.
 */
static inline void dft64x8_batch_q15_store(const cq15x8_t x[64],
                                                c16_t *restrict dst,
                                                int output_radix,
                                                int output_offset,
                                                dft_dir_t dir)
{
  const int dir_slot = (dir == DFT_DIR_FORWARD) ? 0 : 1;
  cq15x8_t T[8][8] __attribute__((aligned(64)));

  for (int q = 0; q < 8; q++) {
    cq15x8_t H[8];
    dft8x8_q15_dir(x[q + 0 * 8],
                          x[q + 1 * 8],
                          x[q + 2 * 8],
                          x[q + 3 * 8],
                          x[q + 4 * 8],
                          x[q + 5 * 8],
                          x[q + 6 * 8],
                          x[q + 7 * 8],
                          &H[0], &H[1], &H[2], &H[3],
                          &H[4], &H[5], &H[6], &H[7], dir);

    T[0][q] = cq15x8_srai_i16(H[0], 3);
    for (int r = 1; r < 8; r++)
      T[r][q] = complex_mul8_prepack_q15(
          H[r],
          g_dft64_batch_re_re_q15[dir_slot][r][q],
          g_dft64_batch_im_signed_q15[dir_slot][r][q]);
  }

  for (int r = 0; r < 8; r++) {
    cq15x8_t K[8];
    dft8x8_q15_dir(T[r][0], T[r][1], T[r][2], T[r][3],
                          T[r][4], T[r][5], T[r][6], T[r][7],
                          &K[0], &K[1], &K[2], &K[3],
                          &K[4], &K[5], &K[6], &K[7], dir);

    for (int q = 0; q < 8; q++) {
      const int k = 8 * q + r;
      cq15x8_storeu(
          (cq15x8_t *)(void *)(dst + output_radix * k + output_offset),
          K[q]);
    }
  }
}

static inline void dft64_q15_dir(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  const cq15x8_t x0 = cq15x8_loadu((const cq15x8_t *)(const void *)(src + 0));
  const cq15x8_t x1 = cq15x8_loadu((const cq15x8_t *)(const void *)(src + 8));
  const cq15x8_t x2 = cq15x8_loadu((const cq15x8_t *)(const void *)(src + 16));
  const cq15x8_t x3 = cq15x8_loadu((const cq15x8_t *)(const void *)(src + 24));
  const cq15x8_t x4 = cq15x8_loadu((const cq15x8_t *)(const void *)(src + 32));
  const cq15x8_t x5 = cq15x8_loadu((const cq15x8_t *)(const void *)(src + 40));
  const cq15x8_t x6 = cq15x8_loadu((const cq15x8_t *)(const void *)(src + 48));
  const cq15x8_t x7 = cq15x8_loadu((const cq15x8_t *)(const void *)(src + 56));

  cq15x8_t H0, H1, H2, H3;
  cq15x8_t H4, H5, H6, H7;

  dft8x8_q15_dir(x0, x1, x2, x3, x4, x5, x6, x7, &H0, &H1, &H2, &H3, &H4, &H5, &H6, &H7, dir);
  const cq15x8_t *C64_RE = (dir == DFT_DIR_FORWARD) ? g_dft64_q15_tw.C64_RE_RE_q15 : g_dft64_q15_tw.C64_RE_RE_q15_inverse;

  const cq15x8_t *C64_IM = (dir == DFT_DIR_FORWARD) ? g_dft64_q15_tw.C64_IM_SIGNED_q15 : g_dft64_q15_tw.C64_IM_SIGNED_q15_inverse;
  H0 = cq15x8_srai_i16(H0, 3);

  H1 = complex_mul8_prepack_q15(H1, C64_RE[1], C64_IM[1]);
  H2 = complex_mul8_prepack_q15(H2, C64_RE[2], C64_IM[2]);
  H3 = complex_mul8_prepack_q15(H3, C64_RE[3], C64_IM[3]);
  H4 = complex_mul8_prepack_q15(H4, C64_RE[4], C64_IM[4]);
  H5 = complex_mul8_prepack_q15(H5, C64_RE[5], C64_IM[5]);
  H6 = complex_mul8_prepack_q15(H6, C64_RE[6], C64_IM[6]);
  H7 = complex_mul8_prepack_q15(H7, C64_RE[7], C64_IM[7]);

  /* Transpose the eight local-frequency vectors across batch lanes. */
  transpose8_complex_q15x8(&H0, &H1, &H2, &H3, &H4, &H5, &H6, &H7);

  cq15x8_t Y0, Y1, Y2, Y3;
  cq15x8_t Y4, Y5, Y6, Y7;

  dft8x8_q15_dir(H0, H1, H2, H3, H4, H5, H6, H7, &Y0, &Y1, &Y2, &Y3, &Y4, &Y5, &Y6, &Y7, dir);
  cq15x8_storeu((cq15x8_t *)(void *)(dst + 0), Y0);
  cq15x8_storeu((cq15x8_t *)(void *)(dst + 8), Y1);
  cq15x8_storeu((cq15x8_t *)(void *)(dst + 16), Y2);
  cq15x8_storeu((cq15x8_t *)(void *)(dst + 24), Y3);
  cq15x8_storeu((cq15x8_t *)(void *)(dst + 32), Y4);
  cq15x8_storeu((cq15x8_t *)(void *)(dst + 40), Y5);
  cq15x8_storeu((cq15x8_t *)(void *)(dst + 48), Y6);
  cq15x8_storeu((cq15x8_t *)(void *)(dst + 56), Y7);
}

static inline cq15x8_t scale_q15_inv_sqrt2_q15x8(cq15x8_t x)
{
  const cq15x8_t s = cq15x8_set1_i16(Q15_INV_SQRT2);
  return cq15x8_mulhrs_i16(x, s);
}

static inline void dft128_stage0_blk_q15_dir(const c16_t *src, c16_t *a, c16_t *b, int blk, dft_dir_t dir)
{
  const cq15x8_t x0 = cq15x8_loadu((const cq15x8_t *)(const void *)(src + 8 * blk));

  const cq15x8_t x1 = cq15x8_loadu((const cq15x8_t *)(const void *)(src + 64 + 8 * blk));

  cq15x8_t sum = cq15x8_adds_i16(x0, x1);
  cq15x8_t diff = cq15x8_subs_i16(x0, x1);

  sum = scale_q15_inv_sqrt2_q15x8(sum);

  const cq15x8_t *W128_RE = (dir == DFT_DIR_FORWARD) ? g_dft64_q15_tw.W128_RE_RE_q15 : g_dft64_q15_tw.W128_RE_RE_q15_inverse;

  const cq15x8_t *W128_IM =
      (dir == DFT_DIR_FORWARD) ? g_dft64_q15_tw.W128_IM_SIGNED_q15 : g_dft64_q15_tw.W128_IM_SIGNED_q15_inverse;

  diff = complex_mul8_prepack_q15(diff, W128_RE[blk], W128_IM[blk]);

  cq15x8_store((cq15x8_t *)(void *)(a + 8 * blk), sum);

  cq15x8_store((cq15x8_t *)(void *)(b + 8 * blk), diff);
}

static inline void interleave64_complex_q15(const c16_t *A, const c16_t *B, c16_t *dst)
{
  for (int blk = 0; blk < 8; blk++) {
    const cq15x8_t va = cq15x8_load((const cq15x8_t *)(const void *)(A + 8 * blk));

    const cq15x8_t vb = cq15x8_load((const cq15x8_t *)(const void *)(B + 8 * blk));

    /*
     * va = [A0 A1 A2 A3 | A4 A5 A6 A7]
     * vb = [B0 B1 B2 B3 | B4 B5 B6 B7]
     *
     * Each A0/B0 is one c16_t = 32 bits.
     */
    const cq15x8_t lo = cq15x8_unpacklo_i32(va, vb);
    const cq15x8_t hi = cq15x8_unpackhi_i32(va, vb);

    /*
     * out0 = [A0 B0 A1 B1 A2 B2 A3 B3]
     * out1 = [A4 B4 A5 B5 A6 B6 A7 B7]
     */
    const cq15x8_t out0 = cq15x8_select_halves(lo, hi, 0x20);
    const cq15x8_t out1 = cq15x8_select_halves(lo, hi, 0x31);

    cq15x8_storeu((cq15x8_t *)(void *)(dst + 16 * blk), out0);

    cq15x8_storeu((cq15x8_t *)(void *)(dst + 16 * blk + 8), out1);
  }
}

static inline void dft128_staged_q15_dir(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  c16_t a[64] __attribute__((aligned(32)));
  c16_t b[64] __attribute__((aligned(32)));

  c16_t A[64] __attribute__((aligned(32)));
  c16_t B[64] __attribute__((aligned(32)));

  for (int blk = 0; blk < 8; blk++) {
    dft128_stage0_blk_q15_dir(src, a, b, blk, dir);
  }

  dft64_q15_dir(a, A, dir);
  dft64_q15_dir(b, B, dir);

  interleave64_complex_q15(A, B, dst);
}



/*
 * DFT128 = radix-2 x two DFT64 children.
 * The two 64-bin child outputs are interleaved in eight-complex NEON batches
 * into the natural DFT128 output order.
 */

static inline void dft128_q15_dir(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  dft128_staged_q15_dir(src, dst, dir);
}

/*
 * Native unitary Q15 DFT4 on four independent complex lanes.
 * Signed halving add/subtract applies the 1/2 unitary scale before the
 * second butterfly addition, followed by saturating add/subtract operations.
 */
static inline void dft4x4_q15(const neon_m128i x0,
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
  const int16x8_t d13_mj = neon128_as_i16(mul_minus_j_q15_128(d13_raw));
  const int16x8_t d13_pj = neon128_as_i16(mul_j_q15_128(d13_raw));

  *Y0 = neon128_from_i16(vqaddq_s16(s02, s13));
  *Y2 = neon128_from_i16(vqsubq_s16(s02, s13));
  *Y1 = neon128_from_i16(vqaddq_s16(d02, d13_mj));
  *Y3 = neon128_from_i16(vqaddq_s16(d02, d13_pj));
}


/*
 * Non-unitary radix-4 butterfly used by DFT64. The 1/2 scale is already
 * applied to A0 and folded into the A1/A2/A3 twiddles, so this helper performs
 * the saturating add/sub butterfly without additional scaling.
 */
static inline void dft4x4_q15_noscale(const neon_m128i x0,
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
  const int16x8_t d13_mj = neon128_as_i16(mul_minus_j_q15_128(d13_raw));
  const int16x8_t d13_pj = neon128_as_i16(mul_j_q15_128(d13_raw));

  *Y0 = neon128_from_i16(vqaddq_s16(s02, s13));
  *Y2 = neon128_from_i16(vqsubq_s16(s02, s13));
  *Y1 = neon128_from_i16(vqaddq_s16(d02, d13_mj));
  *Y3 = neon128_from_i16(vqaddq_s16(d02, d13_pj));
}

/*
 * DFT64 DC calculation. A0..A3 contain DFT16 child outputs before the outer
 * radix-4 scaling. Their DC values are accumulated in int32, rounded once,
 * scaled by the outer radix-4 factor, and written to output bin zero.
 */
static inline int16x4_t dft64_dc_from_child_k0(neon_m128i A0,
                                               neon_m128i A1,
                                               neon_m128i A2,
                                               neon_m128i A3)
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

/*
 * Hierarchical Q15 power-of-two paths:
 *   256   = radix-16 x DFT16
 *   512   = radix-8 x DFT64
 *   1024  = radix-16 x DFT64
 *   2048  = radix-8 x radix-16 x DFT16
 *   4096  = radix-8 x radix-8 x DFT64
 *   8192  = radix-8 x radix-16 x DFT64
 *   16384 = radix-8 x DFT2048
 *   32768 = radix-8 x DFT4096
 *   65536 = radix-8 x DFT8192
 */


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
  cq15x8_t *W_RE_RE;
  cq15x8_t *W_IM_SIGNED;
} dft_radix8_twiddle_q15_t;

/* Forward and inverse tables for the large NEON radix-8 parents. */
static dft_radix8_twiddle_q15_t g_dft_radix8_twiddles[7][2];

static inline int dft_radix8_size_slot(int N)
{
  switch (N) {
    case 512:
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

/*
 * DFT1024 radix-16 outer stage:
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
  cq15x8_t *W_RE_RE;
  cq15x8_t *W_IM_SIGNED;

  /* Broadcast W_16^q / sqrt(2) for the odd radix-2 branch. */
  cq15x8_t W16_RE_RE[8];
  cq15x8_t W16_IM_SIGNED[8];
} dft1024_radix16_twiddle_q15_t;

static dft1024_radix16_twiddle_q15_t g_dft1024_radix16_twiddles[2];

/*
 * DFT256 radix-16 outer stage: DFT256 = 16 x DFT16.
 * The radix-2 component contributes 1/sqrt(2) and the radix-8 component
 * contributes 1/sqrt(8), for a total radix-16 scale of 1/4.
 */
typedef struct {
  int initialized;
  int blocks;

  /* W_256^(r*n) / sqrt(8), r=0..15, eight n values per block. */
  cq15x8_t *W_RE_RE;
  cq15x8_t *W_IM_SIGNED;

  /* Broadcast W_16^q / sqrt(2) for the odd radix-2 branch. */
  cq15x8_t W16_RE_RE[8];
  cq15x8_t W16_IM_SIGNED[8];

  /* W_16^q / 4 for a complete unitary DFT16 child (1/sqrt(16)). */
  cq15x8_t W16_UNITARY_RE_RE[8];
  cq15x8_t W16_UNITARY_IM_SIGNED[8];
} dft256_radix16_twiddle_q15_t;

static dft256_radix16_twiddle_q15_t g_dft256_radix16_twiddles[2];
static pthread_mutex_t g_neon_twiddle_init_mutex = PTHREAD_MUTEX_INITIALIZER;

static void init_dft256_radix16_q15_twiddles(void)
{
  if (__atomic_load_n(&g_dft256_radix16_twiddles[0].initialized, __ATOMIC_ACQUIRE) &&
      __atomic_load_n(&g_dft256_radix16_twiddles[1].initialized, __ATOMIC_ACQUIRE))
    return;

  pthread_mutex_lock(&g_neon_twiddle_init_mutex);
  const int N = 256;
  const int M = 16;
  const int blocks = M / 8;
  const float outer_scale = 1.0f / sqrtf(8.0f);

  for (int dir_slot = 0; dir_slot < 2; dir_slot++) {
    dft256_radix16_twiddle_q15_t *tw = &g_dft256_radix16_twiddles[dir_slot];

    if (tw->initialized)
      continue;

    tw->blocks = blocks;
    const size_t coeff_count = (size_t)16 * (size_t)blocks;
    tw->W_RE_RE = aligned_malloc64((size_t)2 * coeff_count * sizeof(cq15x8_t));
    AssertFatal(tw->W_RE_RE != NULL, "DFT256 radix-16 twiddle allocation failed\n");
    tw->W_IM_SIGNED = tw->W_RE_RE + coeff_count;

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

      tw->W16_RE_RE[q] = cq15x8_load((const cq15x8_t *)(const void *)re_re);
      tw->W16_IM_SIGNED[q] = cq15x8_load((const cq15x8_t *)(const void *)im_signed);
      tw->W16_UNITARY_RE_RE[q] = cq15x8_load((const cq15x8_t *)(const void *)re_re_unitary);
      tw->W16_UNITARY_IM_SIGNED[q] = cq15x8_load((const cq15x8_t *)(const void *)im_signed_unitary);
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
        tw->W_RE_RE[idx] = cq15x8_load((const cq15x8_t *)(const void *)re_re);
        tw->W_IM_SIGNED[idx] = cq15x8_load((const cq15x8_t *)(const void *)im_signed);
      }
    }

    __atomic_store_n(&tw->initialized, 1, __ATOMIC_RELEASE);
  }
  pthread_mutex_unlock(&g_neon_twiddle_init_mutex);
}

static void init_dft1024_radix16_q15_twiddles(void)
{
  if (__atomic_load_n(&g_dft1024_radix16_twiddles[0].initialized, __ATOMIC_ACQUIRE) &&
      __atomic_load_n(&g_dft1024_radix16_twiddles[1].initialized, __ATOMIC_ACQUIRE))
    return;

  pthread_mutex_lock(&g_neon_twiddle_init_mutex);
  const int N = 1024;
  const int M = 64;
  const int blocks = M / 8;
  const float outer_scale = 1.0f / sqrtf(8.0f);

  for (int dir_slot = 0; dir_slot < 2; dir_slot++) {
    dft1024_radix16_twiddle_q15_t *tw = &g_dft1024_radix16_twiddles[dir_slot];

    if (tw->initialized)
      continue;

    tw->blocks = blocks;
    const size_t coeff_count = (size_t)16 * (size_t)blocks;
    tw->W_RE_RE = aligned_malloc64((size_t)2 * coeff_count * sizeof(cq15x8_t));
    AssertFatal(tw->W_RE_RE != NULL, "DFT1024 radix-16 twiddle allocation failed\n");
    tw->W_IM_SIGNED = tw->W_RE_RE + coeff_count;

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

      tw->W16_RE_RE[q] = cq15x8_load((const cq15x8_t *)(const void *)re_re);
      tw->W16_IM_SIGNED[q] = cq15x8_load((const cq15x8_t *)(const void *)im_signed);
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
        tw->W_RE_RE[idx] = cq15x8_load((const cq15x8_t *)(const void *)re_re);
        tw->W_IM_SIGNED[idx] = cq15x8_load((const cq15x8_t *)(const void *)im_signed);
      }
    }

    __atomic_store_n(&tw->initialized, 1, __ATOMIC_RELEASE);
  }
  pthread_mutex_unlock(&g_neon_twiddle_init_mutex);
}


static void init_dft_radix8_q15_twiddles(int N)
{
  const int size_slot = dft_radix8_size_slot(N);
  AssertFatal(size_slot >= 0, "Unsupported radix-8 twiddle size N=%d\n", N);

  if (__atomic_load_n(&g_dft_radix8_twiddles[size_slot][0].initialized, __ATOMIC_ACQUIRE) &&
      __atomic_load_n(&g_dft_radix8_twiddles[size_slot][1].initialized, __ATOMIC_ACQUIRE))
    return;

  pthread_mutex_lock(&g_neon_twiddle_init_mutex);
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
    const size_t coeff_count = (size_t)8 * (size_t)blocks;
    tw->W_RE_RE = aligned_malloc64((size_t)2 * coeff_count * sizeof(cq15x8_t));
    AssertFatal(tw->W_RE_RE != NULL, "Radix-8 twiddle allocation failed N=%d\n", N);
    tw->W_IM_SIGNED = tw->W_RE_RE + coeff_count;

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
        tw->W_RE_RE[idx] = cq15x8_load((const cq15x8_t *)(const void *)re_re);
        tw->W_IM_SIGNED[idx] = cq15x8_load((const cq15x8_t *)(const void *)im_signed);
      }
    }

    __atomic_store_n(&tw->initialized, 1, __ATOMIC_RELEASE);
  }
  pthread_mutex_unlock(&g_neon_twiddle_init_mutex);
}

static inline void radix8_blocked_to_natural_q15(const c16_t *blocked,
                                                    c16_t *dst,
                                                    int M)
{
  /*
   * blocked[r * M + q] = X[8 * q + r]
   *
   * Convert the internal child-major layout to the public natural order.
   */
  for (int q = 0; q < M; q += 8) {
    cq15x8_t y0 = cq15x8_load((const cq15x8_t *)(const void *)(blocked + 0 * M + q));
    cq15x8_t y1 = cq15x8_load((const cq15x8_t *)(const void *)(blocked + 1 * M + q));
    cq15x8_t y2 = cq15x8_load((const cq15x8_t *)(const void *)(blocked + 2 * M + q));
    cq15x8_t y3 = cq15x8_load((const cq15x8_t *)(const void *)(blocked + 3 * M + q));
    cq15x8_t y4 = cq15x8_load((const cq15x8_t *)(const void *)(blocked + 4 * M + q));
    cq15x8_t y5 = cq15x8_load((const cq15x8_t *)(const void *)(blocked + 5 * M + q));
    cq15x8_t y6 = cq15x8_load((const cq15x8_t *)(const void *)(blocked + 6 * M + q));
    cq15x8_t y7 = cq15x8_load((const cq15x8_t *)(const void *)(blocked + 7 * M + q));

    transpose8_complex_q15x8(&y0, &y1, &y2, &y3, &y4, &y5, &y6, &y7);

    cq15x8_storeu((cq15x8_t *)(void *)(dst + 8 * (q + 0)), y0);
    cq15x8_storeu((cq15x8_t *)(void *)(dst + 8 * (q + 1)), y1);
    cq15x8_storeu((cq15x8_t *)(void *)(dst + 8 * (q + 2)), y2);
    cq15x8_storeu((cq15x8_t *)(void *)(dst + 8 * (q + 3)), y3);
    cq15x8_storeu((cq15x8_t *)(void *)(dst + 8 * (q + 4)), y4);
    cq15x8_storeu((cq15x8_t *)(void *)(dst + 8 * (q + 5)), y5);
    cq15x8_storeu((cq15x8_t *)(void *)(dst + 8 * (q + 6)), y6);
    cq15x8_storeu((cq15x8_t *)(void *)(dst + 8 * (q + 7)), y7);
  }
}

static void dft_radix8_q15_stage(const c16_t *src,
                                  c16_t *stage,
                                  int N,
                                  dft_dir_t dir)
{
  init_dft_radix8_q15_twiddles(N);

  const int size_slot = dft_radix8_size_slot(N);
  const int dir_slot = (dir == DFT_DIR_FORWARD) ? 0 : 1;
  const dft_radix8_twiddle_q15_t *tw = &g_dft_radix8_twiddles[size_slot][dir_slot];
  const int M = tw->M;
  const int blocks = tw->blocks;

  for (int b = 0; b < blocks; b++) {
    const int n = 8 * b;

    const cq15x8_t x0 = cq15x8_loadu((const cq15x8_t *)(const void *)(src + 0 * M + n));
    const cq15x8_t x1 = cq15x8_loadu((const cq15x8_t *)(const void *)(src + 1 * M + n));
    const cq15x8_t x2 = cq15x8_loadu((const cq15x8_t *)(const void *)(src + 2 * M + n));
    const cq15x8_t x3 = cq15x8_loadu((const cq15x8_t *)(const void *)(src + 3 * M + n));
    const cq15x8_t x4 = cq15x8_loadu((const cq15x8_t *)(const void *)(src + 4 * M + n));
    const cq15x8_t x5 = cq15x8_loadu((const cq15x8_t *)(const void *)(src + 5 * M + n));
    const cq15x8_t x6 = cq15x8_loadu((const cq15x8_t *)(const void *)(src + 6 * M + n));
    const cq15x8_t x7 = cq15x8_loadu((const cq15x8_t *)(const void *)(src + 7 * M + n));

    cq15x8_t H[8];
    dft8x8_q15_dir(x0, x1, x2, x3, x4, x5, x6, x7,
                          &H[0], &H[1], &H[2], &H[3],
                          &H[4], &H[5], &H[6], &H[7], dir);

    for (int r = 0; r < 8; r++) {
      const int idx = r * blocks + b;
      H[r] = complex_mul8_prepack_q15(H[r], tw->W_RE_RE[idx], tw->W_IM_SIGNED[idx]);
      cq15x8_store((cq15x8_t *)(void *)(stage + r * M + n), H[r]);
    }
  }
}

/* Large NEON radix-8 scratch is thread-local and grows only when needed. */
static __thread c16_t *g_neon_radix8_work;
static __thread size_t g_neon_radix8_work_elems;

static c16_t *neon_radix8_work_get(size_t need)
{
  if (need <= g_neon_radix8_work_elems)
    return g_neon_radix8_work;

  c16_t *p = aligned_malloc64(need * sizeof(*p));
  if (!p)
    return NULL;

  free(g_neon_radix8_work);
  g_neon_radix8_work = p;
  g_neon_radix8_work_elems = need;
  return p;
}

static void dft_radix8_q15(const c16_t *src,
                           c16_t *dst,
                           int N,
                           dft_dir_t dir,
                           dft_child_q15_fn_t child_dft)
{
  const int M = N / 8;
  c16_t *work = neon_radix8_work_get((size_t)2 * (size_t)N);
  AssertFatal(work != NULL, "NEON radix-8 scratch allocation failed N=%d\n", N);

  c16_t *stage = work;
  c16_t *blocked = work + N;

  dft_radix8_q15_stage(src, stage, N, dir);
  for (int r = 0; r < 8; r++)
    child_dft(stage + r * M, blocked + r * M, dir);

  radix8_blocked_to_natural_q15(blocked, dst, M);
}


/*
 * Direct-output DFT1024 = radix-16 x sixteen DFT64 children.
 * The outer stage remains branch-major, but eight child branches are
 * transposed into NEON lanes and the batched DFT64 final stage writes
 * directly to X[16*k+r]; child outputs are not materialized separately and
 * no final radix-16 interleave pass is needed.
 */
static void dft1024_radix16_direct_q15(const c16_t *src,
                                       c16_t *dst,
                                       dft_dir_t dir)
{
  init_dft1024_radix16_q15_twiddles();

  enum { N = 1024, M = 64 };
  const int dir_slot = (dir == DFT_DIR_FORWARD) ? 0 : 1;
  const dft1024_radix16_twiddle_q15_t *tw =
      &g_dft1024_radix16_twiddles[dir_slot];
  const int blocks = tw->blocks;
  const cq15x8_t inv_sqrt2 = cq15x8_set1_i16(Q15_INV_SQRT2);
  c16_t stage[N] __attribute__((aligned(64)));

  for (int b = 0; b < blocks; b++) {
    const int n = 8 * b;
    cq15x8_t A[8];
    cq15x8_t B[8];
    cq15x8_t E[8];
    cq15x8_t O[8];

    for (int q = 0; q < 8; q++) {
      const cq15x8_t lo = cq15x8_loadu(
          (const cq15x8_t *)(const void *)(src + q * M + n));
      const cq15x8_t hi = cq15x8_loadu(
          (const cq15x8_t *)(const void *)(src + (q + 8) * M + n));

      A[q] = cq15x8_mulhrs_i16(cq15x8_adds_i16(lo, hi), inv_sqrt2);
      B[q] = complex_mul8_prepack_q15(
          cq15x8_subs_i16(lo, hi),
          tw->W16_RE_RE[q],
          tw->W16_IM_SIGNED[q]);
    }

    dft8x8_q15_dir(A[0], A[1], A[2], A[3], A[4], A[5], A[6], A[7],
                          &E[0], &E[1], &E[2], &E[3],
                          &E[4], &E[5], &E[6], &E[7], dir);
    dft8x8_q15_dir(B[0], B[1], B[2], B[3], B[4], B[5], B[6], B[7],
                          &O[0], &O[1], &O[2], &O[3],
                          &O[4], &O[5], &O[6], &O[7], dir);

    for (int k = 0; k < 8; k++) {
      const int r_even = 2 * k;
      const int r_odd = r_even + 1;
      const int idx_even = r_even * blocks + b;
      const int idx_odd = r_odd * blocks + b;
      const cq15x8_t even = complex_mul8_prepack_q15(
          E[k], tw->W_RE_RE[idx_even], tw->W_IM_SIGNED[idx_even]);
      const cq15x8_t odd = complex_mul8_prepack_q15(
          O[k], tw->W_RE_RE[idx_odd], tw->W_IM_SIGNED[idx_odd]);

      cq15x8_store((cq15x8_t *)(void *)(stage + r_even * M + n), even);
      cq15x8_store((cq15x8_t *)(void *)(stage + r_odd * M + n), odd);
    }
  }

  for (int group = 0; group < 2; group++) {
    cq15x8_t leaf_in[64] __attribute__((aligned(64)));
    const int r0 = 8 * group;

    for (int n = 0; n < M; n += 8) {
      cq15x8_t z0 = cq15x8_load((const cq15x8_t *)(const void *)(stage + (r0 + 0) * M + n));
      cq15x8_t z1 = cq15x8_load((const cq15x8_t *)(const void *)(stage + (r0 + 1) * M + n));
      cq15x8_t z2 = cq15x8_load((const cq15x8_t *)(const void *)(stage + (r0 + 2) * M + n));
      cq15x8_t z3 = cq15x8_load((const cq15x8_t *)(const void *)(stage + (r0 + 3) * M + n));
      cq15x8_t z4 = cq15x8_load((const cq15x8_t *)(const void *)(stage + (r0 + 4) * M + n));
      cq15x8_t z5 = cq15x8_load((const cq15x8_t *)(const void *)(stage + (r0 + 5) * M + n));
      cq15x8_t z6 = cq15x8_load((const cq15x8_t *)(const void *)(stage + (r0 + 6) * M + n));
      cq15x8_t z7 = cq15x8_load((const cq15x8_t *)(const void *)(stage + (r0 + 7) * M + n));

      transpose8_complex_q15x8(&z0, &z1, &z2, &z3, &z4, &z5, &z6, &z7);
      leaf_in[n + 0] = z0;
      leaf_in[n + 1] = z1;
      leaf_in[n + 2] = z2;
      leaf_in[n + 3] = z3;
      leaf_in[n + 4] = z4;
      leaf_in[n + 5] = z5;
      leaf_in[n + 6] = z6;
      leaf_in[n + 7] = z7;
    }

    dft64x8_batch_q15_store(leaf_in, dst, 16, r0, dir);
  }
}


static inline void dft16x8_radix2x8_q15_unitary_dir(
    const cq15x8_t x[16],
    cq15x8_t Y[16],
    const cq15x8_t W16_RE_RE_DIV4[8],
    const cq15x8_t W16_IM_SIGNED_DIV4[8],
    dft_dir_t dir)
{
  cq15x8_t A[8];
  cq15x8_t B[8];
  cq15x8_t E[8];
  cq15x8_t O[8];

  /* W=1 branches: the 1/4 unitary coefficient has no rotation to absorb it. */
  A[0] = cq15x8_rshr2_i16(cq15x8_adds_i16(x[0], x[8]));
  B[0] = cq15x8_rshr2_i16(cq15x8_subs_i16(x[0], x[8]));

  for (int q = 1; q < 8; q++) {
    A[q] = cq15x8_rshr2_i16(cq15x8_adds_i16(x[q], x[q + 8]));
    B[q] = complex_mul8_prepack_q15(
        cq15x8_subs_i16(x[q], x[q + 8]),
        W16_RE_RE_DIV4[q],
        W16_IM_SIGNED_DIV4[q]);
  }

  dft8x8_q15_dir(A[0], A[1], A[2], A[3], A[4], A[5], A[6], A[7],
                        &E[0], &E[1], &E[2], &E[3], &E[4], &E[5], &E[6], &E[7], dir);
  dft8x8_q15_dir(B[0], B[1], B[2], B[3], B[4], B[5], B[6], B[7],
                        &O[0], &O[1], &O[2], &O[3], &O[4], &O[5], &O[6], &O[7], dir);

  for (int k = 0; k < 8; k++) {
    Y[2 * k + 0] = E[k];
    Y[2 * k + 1] = O[k];
  }
}

static inline void dft256_radix16_prepare_q15(const c16_t *src,
                                                   cq15x8_t child_in[2][16],
                                                   dft_dir_t dir)
{
  const int M = 16;
  const int dir_slot = (dir == DFT_DIR_FORWARD) ? 0 : 1;
  const dft256_radix16_twiddle_q15_t *tw = &g_dft256_radix16_twiddles[dir_slot];
  const int blocks = tw->blocks;
  const cq15x8_t inv_sqrt2 = cq15x8_set1_i16(Q15_INV_SQRT2);
  const cq15x8_t inv_sqrt8 = cq15x8_set1_i16(Q15_INV_SQRT8);

  for (int b = 0; b < blocks; b++) {
    const int n = 8 * b;
    cq15x8_t A[8];
    cq15x8_t B[8];
    cq15x8_t E[8];
    cq15x8_t O[8];
    cq15x8_t R[16];

    /* q=0 has no W16 rotation. */
    {
      const cq15x8_t lo = cq15x8_loadu((const cq15x8_t *)(const void *)(src + n));
      const cq15x8_t hi = cq15x8_loadu((const cq15x8_t *)(const void *)(src + 8 * M + n));
      A[0] = cq15x8_mulhrs_i16(cq15x8_adds_i16(lo, hi), inv_sqrt2);
      B[0] = cq15x8_mulhrs_i16(cq15x8_subs_i16(lo, hi), inv_sqrt2);
    }

    for (int q = 1; q < 8; q++) {
      const cq15x8_t lo = cq15x8_loadu((const cq15x8_t *)(const void *)(src + q * M + n));
      const cq15x8_t hi = cq15x8_loadu((const cq15x8_t *)(const void *)(src + (q + 8) * M + n));

      A[q] = cq15x8_mulhrs_i16(cq15x8_adds_i16(lo, hi), inv_sqrt2);
      B[q] = complex_mul8_prepack_q15(cq15x8_subs_i16(lo, hi), tw->W16_RE_RE[q], tw->W16_IM_SIGNED[q]);
    }

    dft8x8_q15_dir(A[0], A[1], A[2], A[3], A[4], A[5], A[6], A[7],
                          &E[0], &E[1], &E[2], &E[3], &E[4], &E[5], &E[6], &E[7], dir);
    dft8x8_q15_dir(B[0], B[1], B[2], B[3], B[4], B[5], B[6], B[7],
                          &O[0], &O[1], &O[2], &O[3], &O[4], &O[5], &O[6], &O[7], dir);

    /* Outer branch r=0 needs only the 1/sqrt(8) scale. */
    R[0] = cq15x8_mulhrs_i16(E[0], inv_sqrt8);
    R[1] = complex_mul8_prepack_q15(
        O[0], tw->W_RE_RE[blocks + b], tw->W_IM_SIGNED[blocks + b]);

    for (int k = 1; k < 8; k++) {
      const int r_even = 2 * k;
      const int r_odd = r_even + 1;
      const int idx_even = r_even * blocks + b;
      const int idx_odd = r_odd * blocks + b;

      R[r_even] = complex_mul8_prepack_q15(
          E[k], tw->W_RE_RE[idx_even], tw->W_IM_SIGNED[idx_even]);
      R[r_odd] = complex_mul8_prepack_q15(
          O[k], tw->W_RE_RE[idx_odd], tw->W_IM_SIGNED[idx_odd]);
    }

    transpose8_complex_q15x8(&R[0], &R[1], &R[2], &R[3], &R[4], &R[5], &R[6], &R[7]);
    transpose8_complex_q15x8(&R[8], &R[9], &R[10], &R[11], &R[12], &R[13], &R[14], &R[15]);

    for (int lane = 0; lane < 8; lane++) {
      child_in[0][n + lane] = R[lane];
      child_in[1][n + lane] = R[8 + lane];
    }
  }
}

static void dft256_radix16_q15(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  const int dir_slot = (dir == DFT_DIR_FORWARD) ? 0 : 1;
  if (__builtin_expect(
          !__atomic_load_n(&g_dft256_radix16_twiddles[dir_slot].initialized, __ATOMIC_ACQUIRE),
          0))
    init_dft256_radix16_q15_twiddles();

  const dft256_radix16_twiddle_q15_t *tw = &g_dft256_radix16_twiddles[dir_slot];
  cq15x8_t child_in[2][16] __attribute__((aligned(64)));

  dft256_radix16_prepare_q15(src, child_in, dir);

  for (int group = 0; group < 2; group++) {
    cq15x8_t Y[16];
    const int r0 = 8 * group;

    dft16x8_radix2x8_q15_unitary_dir(
        child_in[group], Y, tw->W16_UNITARY_RE_RE, tw->W16_UNITARY_IM_SIGNED, dir);

    for (int k = 0; k < 16; k++)
      cq15x8_storeu((cq15x8_t *)(void *)(dst + 16 * k + r0), Y[k]);
  }
}



static void dft1024_q15(const c16_t *src, c16_t *dst, dft_dir_t dir);


static void dft1024_q15(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  dft1024_radix16_direct_q15(src, dst, dir);
}



static void dft2048_q15(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  dft2048_radix8_q15(src, dst, dir);
}


static void dft512_radix8_direct_q15(const c16_t *src,
                                      c16_t *dst,
                                      dft_dir_t dir)
{
  enum { N = 512, M = 64 };
  c16_t stage[N] __attribute__((aligned(64)));
  cq15x8_t leaf_in[M] __attribute__((aligned(64)));

  dft_radix8_q15_stage(src, stage, N, dir);

  for (int n = 0; n < M; n += 8) {
    cq15x8_t z0 = cq15x8_load((const cq15x8_t *)(const void *)(stage + 0 * M + n));
    cq15x8_t z1 = cq15x8_load((const cq15x8_t *)(const void *)(stage + 1 * M + n));
    cq15x8_t z2 = cq15x8_load((const cq15x8_t *)(const void *)(stage + 2 * M + n));
    cq15x8_t z3 = cq15x8_load((const cq15x8_t *)(const void *)(stage + 3 * M + n));
    cq15x8_t z4 = cq15x8_load((const cq15x8_t *)(const void *)(stage + 4 * M + n));
    cq15x8_t z5 = cq15x8_load((const cq15x8_t *)(const void *)(stage + 5 * M + n));
    cq15x8_t z6 = cq15x8_load((const cq15x8_t *)(const void *)(stage + 6 * M + n));
    cq15x8_t z7 = cq15x8_load((const cq15x8_t *)(const void *)(stage + 7 * M + n));

    transpose8_complex_q15x8(&z0, &z1, &z2, &z3, &z4, &z5, &z6, &z7);
    leaf_in[n + 0] = z0;
    leaf_in[n + 1] = z1;
    leaf_in[n + 2] = z2;
    leaf_in[n + 3] = z3;
    leaf_in[n + 4] = z4;
    leaf_in[n + 5] = z5;
    leaf_in[n + 6] = z6;
    leaf_in[n + 7] = z7;
  }

  dft64x8_batch_q15_store(leaf_in, dst, 8, 0, dir);
}

static void dft512_radix8_q15(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  dft512_radix8_direct_q15(src, dst, dir);
}


/*
 * Fused DFT2048 = radix-8 x radix-16 x DFT16.
 * The eight DFT256 children are computed in lockstep; their final NEON
 * vectors are transposed across the outer radix-8 dimension and stored in
 * natural output order without materializing complete DFT256 child outputs.
 */
static void dft2048_radix8x16_fused_q15(const c16_t *src,
                                         c16_t *dst,
                                         dft_dir_t dir)
{
  enum { N = 2048, CHILD_N = 256, M = 16 };
  c16_t outer_stage[N] __attribute__((aligned(64)));

  /*
   * child_in[r16][n] is one NEON vector whose eight complex lanes are the
   * eight outer radix-8 branches. Each DFT16 call computes the same radix-16
   * branch for all outer children in parallel, and its result
   * can be stored directly as eight consecutive final DFT2048 bins.
   */
  cq15x8_t child_in[16][16] __attribute__((aligned(64)));

  dft_radix8_q15_stage(src, outer_stage, N, dir);
  init_dft256_radix16_q15_twiddles();

  const int dir_slot = (dir == DFT_DIR_FORWARD) ? 0 : 1;
  const dft256_radix16_twiddle_q15_t *tw = &g_dft256_radix16_twiddles[dir_slot];
  const cq15x8_t inv_sqrt2 = cq15x8_set1_i16(Q15_INV_SQRT2);

  for (int b = 0; b < 2; b++) {
    const int n = 8 * b;
    cq15x8_t R[8][16] __attribute__((aligned(64)));

    for (int outer_r = 0; outer_r < 8; outer_r++) {
      const c16_t *child_src = outer_stage + outer_r * CHILD_N;
      cq15x8_t A[8];
      cq15x8_t B[8];
      cq15x8_t E[8];
      cq15x8_t O[8];

      for (int q = 0; q < 8; q++) {
        const cq15x8_t lo = cq15x8_loadu(
            (const cq15x8_t *)(const void *)(child_src + q * M + n));
        const cq15x8_t hi = cq15x8_loadu(
            (const cq15x8_t *)(const void *)(child_src + (q + 8) * M + n));

        A[q] = cq15x8_mulhrs_i16(cq15x8_adds_i16(lo, hi), inv_sqrt2);
        B[q] = complex_mul8_prepack_q15(cq15x8_subs_i16(lo, hi), tw->W16_RE_RE[q], tw->W16_IM_SIGNED[q]);
      }

      dft8x8_q15_dir(A[0], A[1], A[2], A[3], A[4], A[5], A[6], A[7],
                            &E[0], &E[1], &E[2], &E[3],
                            &E[4], &E[5], &E[6], &E[7], dir);
      dft8x8_q15_dir(B[0], B[1], B[2], B[3], B[4], B[5], B[6], B[7],
                            &O[0], &O[1], &O[2], &O[3],
                            &O[4], &O[5], &O[6], &O[7], dir);

      for (int k = 0; k < 8; k++) {
        const int r_even = 2 * k;
        const int r_odd = r_even + 1;
        const int idx_even = r_even * 2 + b;
        const int idx_odd = r_odd * 2 + b;

        R[outer_r][r_even] = complex_mul8_prepack_q15(
            E[k], tw->W_RE_RE[idx_even], tw->W_IM_SIGNED[idx_even]);
        R[outer_r][r_odd] = complex_mul8_prepack_q15(
            O[k], tw->W_RE_RE[idx_odd], tw->W_IM_SIGNED[idx_odd]);
      }
    }

    /* Transpose the outer-child x n vectors before storing. */
    for (int r16 = 0; r16 < 16; r16++) {
      cq15x8_t z0 = R[0][r16];
      cq15x8_t z1 = R[1][r16];
      cq15x8_t z2 = R[2][r16];
      cq15x8_t z3 = R[3][r16];
      cq15x8_t z4 = R[4][r16];
      cq15x8_t z5 = R[5][r16];
      cq15x8_t z6 = R[6][r16];
      cq15x8_t z7 = R[7][r16];

      transpose8_complex_q15x8(&z0, &z1, &z2, &z3, &z4, &z5, &z6, &z7);

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
    cq15x8_t Y[16];
    dft16x8_radix2x8_q15_unitary_dir(
        child_in[r16],
        Y,
        tw->W16_UNITARY_RE_RE,
        tw->W16_UNITARY_IM_SIGNED,
        dir);

    for (int k = 0; k < 16; k++)
      cq15x8_storeu((cq15x8_t *)(void *)(dst + 128 * k + 8 * r16), Y[k]);
  }
}

static void dft2048_radix8_q15(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  dft2048_radix8x16_fused_q15(src, dst, dir);
}


/*
 * DFT4096 = radix-8 x radix-8 x DFT64, with the eight outer children
 * computed in parallel. Every DFT64 result vector already contains the
 * eight consecutive outer-radix bins expected by natural DFT4096 order.
 */
static void dft4096_radix8x8_fused_q15(const c16_t *src,
                                        c16_t *dst,
                                        dft_dir_t dir)
{
  enum { N = 4096, CHILD_N = 512, M = 64 };
  c16_t outer_stage[N] __attribute__((aligned(64)));
  cq15x8_t leaf_in[8][64] __attribute__((aligned(64)));

  dft_radix8_q15_stage(src, outer_stage, N, dir);
  init_dft_radix8_q15_twiddles(CHILD_N);

  const int dir_slot = (dir == DFT_DIR_FORWARD) ? 0 : 1;
  const dft_radix8_twiddle_q15_t *tw = &g_dft_radix8_twiddles[dft_radix8_size_slot(CHILD_N)][dir_slot];

  for (int b = 0; b < 8; b++) {
    const int n = 8 * b;
    cq15x8_t R[8][8] __attribute__((aligned(64)));

    for (int outer_r = 0; outer_r < 8; outer_r++) {
      const c16_t *child_src = outer_stage + outer_r * CHILD_N;
      cq15x8_t H[8];

      const cq15x8_t x0 = cq15x8_loadu((const cq15x8_t *)(const void *)(child_src + 0 * M + n));
      const cq15x8_t x1 = cq15x8_loadu((const cq15x8_t *)(const void *)(child_src + 1 * M + n));
      const cq15x8_t x2 = cq15x8_loadu((const cq15x8_t *)(const void *)(child_src + 2 * M + n));
      const cq15x8_t x3 = cq15x8_loadu((const cq15x8_t *)(const void *)(child_src + 3 * M + n));
      const cq15x8_t x4 = cq15x8_loadu((const cq15x8_t *)(const void *)(child_src + 4 * M + n));
      const cq15x8_t x5 = cq15x8_loadu((const cq15x8_t *)(const void *)(child_src + 5 * M + n));
      const cq15x8_t x6 = cq15x8_loadu((const cq15x8_t *)(const void *)(child_src + 6 * M + n));
      const cq15x8_t x7 = cq15x8_loadu((const cq15x8_t *)(const void *)(child_src + 7 * M + n));

      dft8x8_q15_dir(x0, x1, x2, x3, x4, x5, x6, x7,
                            &H[0], &H[1], &H[2], &H[3],
                            &H[4], &H[5], &H[6], &H[7], dir);

      for (int r = 0; r < 8; r++) {
        const int idx = r * 8 + b;
        R[outer_r][r] = complex_mul8_prepack_q15(
            H[r], tw->W_RE_RE[idx], tw->W_IM_SIGNED[idx]);
      }
    }

    for (int r = 0; r < 8; r++) {
      cq15x8_t z0 = R[0][r];
      cq15x8_t z1 = R[1][r];
      cq15x8_t z2 = R[2][r];
      cq15x8_t z3 = R[3][r];
      cq15x8_t z4 = R[4][r];
      cq15x8_t z5 = R[5][r];
      cq15x8_t z6 = R[6][r];
      cq15x8_t z7 = R[7][r];
      transpose8_complex_q15x8(&z0, &z1, &z2, &z3, &z4, &z5, &z6, &z7);
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
    dft64x8_batch_q15_store(leaf_in[r], dst, 64, 8 * r, dir);
}

/*
 * DFT8192 = radix-8 x radix-16 x DFT64, batched across the eight outer
 * radix-8 children and written without separate child result arrays.
 */
static void dft8192_radix8x16_fused_q15(const c16_t *src,
                                         c16_t *dst,
                                         dft_dir_t dir)
{
  enum { N = 8192, CHILD_N = 1024, M = 64 };
  c16_t outer_stage[N] __attribute__((aligned(64)));
  cq15x8_t leaf_in[16][64] __attribute__((aligned(64)));

  dft_radix8_q15_stage(src, outer_stage, N, dir);
  init_dft1024_radix16_q15_twiddles();

  const int dir_slot = (dir == DFT_DIR_FORWARD) ? 0 : 1;
  const dft1024_radix16_twiddle_q15_t *tw = &g_dft1024_radix16_twiddles[dir_slot];
  const cq15x8_t inv_sqrt2 = cq15x8_set1_i16(Q15_INV_SQRT2);

  for (int b = 0; b < 8; b++) {
    const int n = 8 * b;
    cq15x8_t R[8][16] __attribute__((aligned(64)));

    for (int outer_r = 0; outer_r < 8; outer_r++) {
      const c16_t *child_src = outer_stage + outer_r * CHILD_N;
      cq15x8_t A[8];
      cq15x8_t B[8];
      cq15x8_t E[8];
      cq15x8_t O[8];

      for (int q = 0; q < 8; q++) {
        const cq15x8_t lo = cq15x8_loadu(
            (const cq15x8_t *)(const void *)(child_src + q * M + n));
        const cq15x8_t hi = cq15x8_loadu(
            (const cq15x8_t *)(const void *)(child_src + (q + 8) * M + n));
        A[q] = cq15x8_mulhrs_i16(cq15x8_adds_i16(lo, hi), inv_sqrt2);
        B[q] = complex_mul8_prepack_q15(cq15x8_subs_i16(lo, hi), tw->W16_RE_RE[q], tw->W16_IM_SIGNED[q]);
      }

      dft8x8_q15_dir(A[0], A[1], A[2], A[3], A[4], A[5], A[6], A[7],
                            &E[0], &E[1], &E[2], &E[3],
                            &E[4], &E[5], &E[6], &E[7], dir);
      dft8x8_q15_dir(B[0], B[1], B[2], B[3], B[4], B[5], B[6], B[7],
                            &O[0], &O[1], &O[2], &O[3],
                            &O[4], &O[5], &O[6], &O[7], dir);

      for (int k = 0; k < 8; k++) {
        const int r_even = 2 * k;
        const int r_odd = r_even + 1;
        const int idx_even = r_even * 8 + b;
        const int idx_odd = r_odd * 8 + b;
        R[outer_r][r_even] = complex_mul8_prepack_q15(
            E[k], tw->W_RE_RE[idx_even], tw->W_IM_SIGNED[idx_even]);
        R[outer_r][r_odd] = complex_mul8_prepack_q15(
            O[k], tw->W_RE_RE[idx_odd], tw->W_IM_SIGNED[idx_odd]);
      }
    }

    for (int r = 0; r < 16; r++) {
      cq15x8_t z0 = R[0][r];
      cq15x8_t z1 = R[1][r];
      cq15x8_t z2 = R[2][r];
      cq15x8_t z3 = R[3][r];
      cq15x8_t z4 = R[4][r];
      cq15x8_t z5 = R[5][r];
      cq15x8_t z6 = R[6][r];
      cq15x8_t z7 = R[7][r];
      transpose8_complex_q15x8(&z0, &z1, &z2, &z3, &z4, &z5, &z6, &z7);
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
    dft64x8_batch_q15_store(leaf_in[r], dst, 128, 8 * r, dir);
}


static void dft4096_q15(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  dft4096_radix8x8_fused_q15(src, dst, dir);
}

static void dft8192_radix8_q15(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  dft8192_radix8x16_fused_q15(src, dst, dir);
}



static void dft32768_radix8_q15(const c16_t *src,
                                         c16_t *dst,
                                         dft_dir_t dir)
{
  dft_radix8_q15(src, dst, 32768, dir, dft4096_q15);
}

static void dft16384_radix8_q15(const c16_t *src,
                                  c16_t *dst,
                                  dft_dir_t dir)
{
  dft_radix8_q15(src, dst, 16384, dir, dft2048_q15);
}

static void dft65536_radix8_q15(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  dft_radix8_q15(src, dst, 65536, dir, dft8192_radix8_q15);
}


static inline void dft64_radix4_q15_neon(const c16_t *__restrict src,
                                                     c16_t *__restrict dst);

static void dft_power2_q15(const c16_t *src,
                           c16_t *dst,
                           int N,
                           dft_dir_t dir)
{
  switch (N) {
    case 64:
      if (dir == DFT_DIR_FORWARD)
        dft64_radix4_q15_neon(src, dst);
      else
        dft64_q15_dir(src, dst, dir);
      return;
    case 128:
      dft128_q15_dir(src, dst, dir);
      return;
    case 256:
      dft256_radix16_q15(src, dst, dir);
      return;
    case 512:
      dft512_radix8_q15(src, dst, dir);
      return;
    case 1024:
      dft1024_q15(src, dst, dir);
      return;
    case 2048:
      dft2048_q15(src, dst, dir);
      return;
    case 4096:
      dft4096_q15(src, dst, dir);
      return;
    case 8192:
      dft8192_radix8_q15(src, dst, dir);
      return;
    case 16384:
      dft16384_radix8_q15(src, dst, dir);
      return;
    case 32768:
      dft32768_radix8_q15(src, dst, dir);
      return;
    case 65536:
      dft65536_radix8_q15(src, dst, dir);
      return;
    default:
      AssertFatal(false, "Unsupported NEON power-of-two DFT/IDFT N=%d\n", N);
  }
}

/* =========================================================
 * Native unitary Q15 DFT16 / DFT32.
 * Scaling is applied stage-by-stage for a total gain of 1/sqrt(N).
 * ========================================================= */

static inline void dft16_q15_native(const c16_t *__restrict src,
                                        c16_t *__restrict dst)
{
  neon_m128i H0, H1, H2, H3;
  const neon_m128i x0 = neon128_loadu_i((const neon_m128i *)(const void *)(src + 0));
  const neon_m128i x1 = neon128_loadu_i((const neon_m128i *)(const void *)(src + 4));
  const neon_m128i x2 = neon128_loadu_i((const neon_m128i *)(const void *)(src + 8));
  const neon_m128i x3 = neon128_loadu_i((const neon_m128i *)(const void *)(src + 12));

  /* First unitary DFT4 stage: scale 1/2. */
  dft4x4_q15(x0, x1, x2, x3, &H0, &H1, &H2, &H3);

  H1 = complex_mul4_prepack_q15_128(
      H1,
      g_dft64_q15_tw.C16_RE_RE_q15_native[1],
      g_dft64_q15_tw.C16_IM_SIGNED_q15_native[1]);
  H2 = complex_mul4_prepack_q15_128(
      H2,
      g_dft64_q15_tw.C16_RE_RE_q15_native[2],
      g_dft64_q15_tw.C16_IM_SIGNED_q15_native[2]);
  H3 = complex_mul4_prepack_q15_128(
      H3,
      g_dft64_q15_tw.C16_RE_RE_q15_native[3],
      g_dft64_q15_tw.C16_IM_SIGNED_q15_native[3]);

  transpose4_complex_epi16(&H0, &H1, &H2, &H3);

  neon_m128i Y0, Y1, Y2, Y3;
  /* Second unitary DFT4 stage: another scale 1/2. */
  dft4x4_q15(H0, H1, H2, H3, &Y0, &Y1, &Y2, &Y3);

  neon128_storeu_i((neon_m128i *)(void *)(dst + 0), Y0);
  neon128_storeu_i((neon_m128i *)(void *)(dst + 4), Y1);
  neon128_storeu_i((neon_m128i *)(void *)(dst + 8), Y2);
  neon128_storeu_i((neon_m128i *)(void *)(dst + 12), Y3);
}

/* Native unitary inverse DFT4 on four independent complex lanes. */
static inline void idft4x4_q15(const neon_m128i x0,
                                  const neon_m128i x1,
                                  const neon_m128i x2,
                                  const neon_m128i x3,
                                  neon_m128i *Y0, neon_m128i *Y1,
                                  neon_m128i *Y2, neon_m128i *Y3)
{
  neon_m128i f0, f1, f2, f3;
  dft4x4_q15(x0, x1, x2, x3, &f0, &f1, &f2, &f3);
  *Y0 = f0; *Y1 = f3; *Y2 = f2; *Y3 = f1;
}

/* Native unitary IDFT16: inverse radix-4 x radix-4, with conjugated W16. */
static inline void idft16_q15_native(const c16_t *__restrict src,
                                        c16_t *__restrict dst)
{
  neon_m128i H0,H1,H2,H3;
  const neon_m128i x0=neon128_loadu_i((const neon_m128i *)(const void *)(src+0));
  const neon_m128i x1=neon128_loadu_i((const neon_m128i *)(const void *)(src+4));
  const neon_m128i x2=neon128_loadu_i((const neon_m128i *)(const void *)(src+8));
  const neon_m128i x3=neon128_loadu_i((const neon_m128i *)(const void *)(src+12));
  idft4x4_q15(x0,x1,x2,x3,&H0,&H1,&H2,&H3);
  H1=complex_mul4_prepack_q15_128(H1,g_dft64_q15_tw.C16_RE_RE_q15_native_inverse[1],g_dft64_q15_tw.C16_IM_SIGNED_q15_native_inverse[1]);
  H2=complex_mul4_prepack_q15_128(H2,g_dft64_q15_tw.C16_RE_RE_q15_native_inverse[2],g_dft64_q15_tw.C16_IM_SIGNED_q15_native_inverse[2]);
  H3=complex_mul4_prepack_q15_128(H3,g_dft64_q15_tw.C16_RE_RE_q15_native_inverse[3],g_dft64_q15_tw.C16_IM_SIGNED_q15_native_inverse[3]);
  transpose4_complex_epi16(&H0,&H1,&H2,&H3);
  neon_m128i Y0,Y1,Y2,Y3;
  idft4x4_q15(H0,H1,H2,H3,&Y0,&Y1,&Y2,&Y3);
  neon128_storeu_i((neon_m128i *)(void *)(dst+0),Y0);
  neon128_storeu_i((neon_m128i *)(void *)(dst+4),Y1);
  neon128_storeu_i((neon_m128i *)(void *)(dst+8),Y2);
  neon128_storeu_i((neon_m128i *)(void *)(dst+12),Y3);
}


/*
 * Native AArch64 NEON unitary Q15 DFT64 = radix-4 x DFT16.
 * Stage 1 applies four unitary DFT16 transforms (gain 1/4).
 * Stage 2 applies W64^(r*k) and four unitary DFT4 butterflies (gain 1/2).
 * Total gain: 1/8 = 1/sqrt(64).
 */
static inline void dft64_radix4_q15_neon(const c16_t *__restrict src,
                                               c16_t *__restrict dst)
{
  c16_t branch[64] __attribute__((aligned(16)));
  c16_t child[64] __attribute__((aligned(16)));
  int16x4_t dc = vdup_n_s16(0);

  /* Four 4x4 complex transposes split x[4*n+r] without scalar gathers. */
  for (int group = 0; group < 4; group++) {
    neon_m128i r0 = neon128_loadu_i(
        (const neon_m128i *)(const void *)(src + 16 * group + 0));
    neon_m128i r1 = neon128_loadu_i(
        (const neon_m128i *)(const void *)(src + 16 * group + 4));
    neon_m128i r2 = neon128_loadu_i(
        (const neon_m128i *)(const void *)(src + 16 * group + 8));
    neon_m128i r3 = neon128_loadu_i(
        (const neon_m128i *)(const void *)(src + 16 * group + 12));

    transpose4_complex_epi16(&r0, &r1, &r2, &r3);

    neon128_storeu_i((neon_m128i *)(void *)(branch + 0 * 16 + 4 * group), r0);
    neon128_storeu_i((neon_m128i *)(void *)(branch + 1 * 16 + 4 * group), r1);
    neon128_storeu_i((neon_m128i *)(void *)(branch + 2 * 16 + 4 * group), r2);
    neon128_storeu_i((neon_m128i *)(void *)(branch + 3 * 16 + 4 * group), r3);
  }

  dft16_q15_native(branch + 0 * 16, child + 0 * 16);
  dft16_q15_native(branch + 1 * 16, child + 1 * 16);
  dft16_q15_native(branch + 2 * 16, child + 2 * 16);
  dft16_q15_native(branch + 3 * 16, child + 3 * 16);

  for (int block = 0; block < 4; block++) {
    neon_m128i A0 = neon128_loadu_i(
        (const neon_m128i *)(const void *)(child + 0 * 16 + 4 * block));
    neon_m128i A1 = neon128_loadu_i(
        (const neon_m128i *)(const void *)(child + 1 * 16 + 4 * block));
    neon_m128i A2 = neon128_loadu_i(
        (const neon_m128i *)(const void *)(child + 2 * 16 + 4 * block));
    neon_m128i A3 = neon128_loadu_i(
        (const neon_m128i *)(const void *)(child + 3 * 16 + 4 * block));

    if (block == 0)
      dc = dft64_dc_from_child_k0(A0, A1, A2, A3);

    /* Branch zero applies the 1/2 scale with one arithmetic shift. For
     * branches 1..3 the same scale is folded into the packed W64 coefficients. */
    A0 = neon128_from_i16(vshrq_n_s16(neon128_as_i16(A0), 1));
    A1 = complex_mul4_prepack_q15_128(
        A1,
        g_dft64_r4_re_re_q15[0][block],
        g_dft64_r4_im_signed_q15[0][block]);
    A2 = complex_mul4_prepack_q15_128(
        A2,
        g_dft64_r4_re_re_q15[1][block],
        g_dft64_r4_im_signed_q15[1][block]);
    A3 = complex_mul4_prepack_q15_128(
        A3,
        g_dft64_r4_re_re_q15[2][block],
        g_dft64_r4_im_signed_q15[2][block]);

    neon_m128i Y0, Y1, Y2, Y3;
    dft4x4_q15_noscale(A0, A1, A2, A3, &Y0, &Y1, &Y2, &Y3);

    neon128_storeu_i((neon_m128i *)(void *)(dst + 0 * 16 + 4 * block), Y0);
    neon128_storeu_i((neon_m128i *)(void *)(dst + 1 * 16 + 4 * block), Y1);
    neon128_storeu_i((neon_m128i *)(void *)(dst + 2 * 16 + 4 * block), Y2);
    neon128_storeu_i((neon_m128i *)(void *)(dst + 3 * 16 + 4 * block), Y3);
  }

  /* Bin zero uses the dedicated one-rounding DC result. */
  int16x8_t first = vld1q_s16((const int16_t *)(const void *)dst);
  first = vsetq_lane_s16(vget_lane_s16(dc, 0), first, 0);
  first = vsetq_lane_s16(vget_lane_s16(dc, 1), first, 1);
  vst1q_s16((int16_t *)(void *)dst, first);
}

static inline void dft8x4_q15_unitary_native(
    const neon_m128i x0,
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
  dft4x4_q15(x0, x2, x4, x6, &E0, &E1, &E2, &E3);
  dft4x4_q15(x1, x3, x5, x7, &O0, &O1, &O2, &O3);

  E0 = q15_mul_i16_128(E0, Q15_INV_SQRT2);
  E1 = q15_mul_i16_128(E1, Q15_INV_SQRT2);
  E2 = q15_mul_i16_128(E2, Q15_INV_SQRT2);
  E3 = q15_mul_i16_128(E3, Q15_INV_SQRT2);

  const neon_m128i half_re = neon128_set1_i16(Q15_HALF);
  const neon_m128i half_minus_j =
      neon128_setr_i16(Q15_HALF, -Q15_HALF, Q15_HALF, -Q15_HALF,
                     Q15_HALF, -Q15_HALF, Q15_HALF, -Q15_HALF);

  const neon_m128i T0 = q15_mul_i16_128(O0, Q15_INV_SQRT2);
  /* W8^1 / sqrt(2) = (1-j)/2. */
  const neon_m128i T1 = complex_mul4_prepack_q15_128(O1, half_re, half_minus_j);
  /* W8^2 / sqrt(2) = -j/sqrt(2). */
  const neon_m128i T2 = mul_minus_j_q15_128(q15_mul_i16_128(O2, Q15_INV_SQRT2));
  /* W8^3 / sqrt(2) = (-1-j)/2. */
  const neon_m128i T3 = complex_mul4_prepack_q15_128(
      O3, neon128_set1_i16((int16_t)-Q15_HALF), half_minus_j);

  *Y0 = neon128_adds_i16(E0, T0);
  *Y4 = neon128_subs_i16(E0, T0);
  *Y1 = neon128_adds_i16(E1, T1);
  *Y5 = neon128_subs_i16(E1, T1);
  *Y2 = neon128_adds_i16(E2, T2);
  *Y6 = neon128_subs_i16(E2, T2);
  *Y3 = neon128_adds_i16(E3, T3);
  *Y7 = neon128_subs_i16(E3, T3);
}

static inline void dft32_q15_native(const c16_t *__restrict src,
                                        c16_t *__restrict dst)
{
  const neon_m128i x0_lo = neon128_loadu_i((const neon_m128i *)(const void *)(src + 0));
  const neon_m128i x0_hi = neon128_loadu_i((const neon_m128i *)(const void *)(src + 4));
  const neon_m128i x1_lo = neon128_loadu_i((const neon_m128i *)(const void *)(src + 8));
  const neon_m128i x1_hi = neon128_loadu_i((const neon_m128i *)(const void *)(src + 12));
  const neon_m128i x2_lo = neon128_loadu_i((const neon_m128i *)(const void *)(src + 16));
  const neon_m128i x2_hi = neon128_loadu_i((const neon_m128i *)(const void *)(src + 20));
  const neon_m128i x3_lo = neon128_loadu_i((const neon_m128i *)(const void *)(src + 24));
  const neon_m128i x3_hi = neon128_loadu_i((const neon_m128i *)(const void *)(src + 28));

  neon_m128i H0_lo, H1_lo, H2_lo, H3_lo;
  neon_m128i H0_hi, H1_hi, H2_hi, H3_hi;
  dft4x4_q15(x0_lo, x1_lo, x2_lo, x3_lo,
                 &H0_lo, &H1_lo, &H2_lo, &H3_lo);
  dft4x4_q15(x0_hi, x1_hi, x2_hi, x3_hi,
                 &H0_hi, &H1_hi, &H2_hi, &H3_hi);

#define DFT32_Q15_TWIDDLE(K, VLO, VHI) do {     (VLO) = complex_mul4_prepack_q15_128(         (VLO),         g_dft64_q15_tw.C32_RE_RE_q15_native[(K)][0],         g_dft64_q15_tw.C32_IM_SIGNED_q15_native[(K)][0]);     (VHI) = complex_mul4_prepack_q15_128(         (VHI),         g_dft64_q15_tw.C32_RE_RE_q15_native[(K)][1],         g_dft64_q15_tw.C32_IM_SIGNED_q15_native[(K)][1]);   } while (0)
  DFT32_Q15_TWIDDLE(1, H1_lo, H1_hi);
  DFT32_Q15_TWIDDLE(2, H2_lo, H2_hi);
  DFT32_Q15_TWIDDLE(3, H3_lo, H3_hi);
#undef DFT32_Q15_TWIDDLE

  transpose4_complex_epi16(&H0_lo, &H1_lo, &H2_lo, &H3_lo);
  transpose4_complex_epi16(&H0_hi, &H1_hi, &H2_hi, &H3_hi);

  neon_m128i Y0, Y1, Y2, Y3, Y4, Y5, Y6, Y7;
  dft8x4_q15_unitary_native(
      H0_lo, H1_lo, H2_lo, H3_lo,
      H0_hi, H1_hi, H2_hi, H3_hi,
      &Y0, &Y1, &Y2, &Y3, &Y4, &Y5, &Y6, &Y7);

  neon128_storeu_i((neon_m128i *)(void *)(dst + 0), Y0);
  neon128_storeu_i((neon_m128i *)(void *)(dst + 4), Y1);
  neon128_storeu_i((neon_m128i *)(void *)(dst + 8), Y2);
  neon128_storeu_i((neon_m128i *)(void *)(dst + 12), Y3);
  neon128_storeu_i((neon_m128i *)(void *)(dst + 16), Y4);
  neon128_storeu_i((neon_m128i *)(void *)(dst + 20), Y5);
  neon128_storeu_i((neon_m128i *)(void *)(dst + 24), Y6);
  neon128_storeu_i((neon_m128i *)(void *)(dst + 28), Y7);
}


/* ========================================================================
 * Native NEON 5-smooth transform helpers.
 * ======================================================================== */

static inline neon_m128i neon_broadcast_c16(c16_t z)
{
  uint32_t bits;
  memcpy(&bits, &z, sizeof(bits));
  return neon128_from_i16(vreinterpretq_s16_u32(vdupq_n_u32(bits)));
}

static inline c16_t neon_lane0_c16(neon_m128i v)
{
  const uint32_t bits = vgetq_lane_u32(vreinterpretq_u32_s16(neon128_as_i16(v)), 0);
  c16_t z;
  memcpy(&z, &bits, sizeof(bits));
  return z;
}

/* Single-vector native NEON unitary DFT4.
 * Input/output layout is four contiguous c16 values in one int16x8_t. */
static inline __attribute__((always_inline)) int16x8_t
neon_dft4_packed_q15(int16x8_t vin)
{
  /* 1/sqrt(4) = 1/2: one rounded shift, no multiplier. */
  const int16x8_t v = vrshrq_n_s16(vin, 1);
  const int16x8_t vr = vextq_s16(v, v, 4);
  const int16x8_t sum = vqaddq_s16(v, vr);
  const int16x8_t dif = vqsubq_s16(v, vr);
  const int16x8_t ss = vreinterpretq_s16_u32(
      vrev64q_u32(vreinterpretq_u32_s16(sum)));
  const int16x8_t ds = vreinterpretq_s16_u32(
      vrev64q_u32(vreinterpretq_u32_s16(dif)));
  const int16x8_t a = vqaddq_s16(sum, ss); /* Y0 replicated across complex lanes. */
  const int16x8_t b = vqsubq_s16(sum, ss); /* Y2 in complex lane 0. */

  const int16x8_t dsw = vrev32q_s16(ds);
  const int16x8_t dsn = vqnegq_s16(dsw);
  const uint16x8_t even_mask = {0xffffu,0,0xffffu,0,0xffffu,0,0xffffu,0};
  const int16x8_t minus_j = vbslq_s16(even_mask, dsw, dsn);
  const int16x8_t plus_j  = vbslq_s16(even_mask, dsn, dsw);
  const int16x8_t c = vqaddq_s16(dif, minus_j); /* Y1 in complex lane 0. */
  const int16x8_t e = vqaddq_s16(dif, plus_j);  /* Y3 in complex lane 0. */

  const uint32x4_t ac = vzip1q_u32(vreinterpretq_u32_s16(a),
                                    vreinterpretq_u32_s16(c));
  const uint32x4_t be = vzip1q_u32(vreinterpretq_u32_s16(b),
                                    vreinterpretq_u32_s16(e));
  return vreinterpretq_s16_u64(vzip1q_u64(vreinterpretq_u64_u32(ac),
                                           vreinterpretq_u64_u32(be)));
}

static inline void neon_dft4_q15(const c16_t *src, c16_t *dst)
{
  const int16x8_t x = vld1q_s16((const int16_t *)(const void *)src);
  vst1q_s16((int16_t *)(void *)dst, neon_dft4_packed_q15(x));
}

/* NEON DFT12 = radix-3 x DFT4. Branch zero applies 1/sqrt(3) directly;
 * branches 1 and 2 use W12 twiddles with the same scale folded in. */
static inline void neon_dft12_q15(const c16_t *src, c16_t *dst)
{
  const int16x8_t x0=vld1q_s16((const int16_t *)(const void *)(src+0));
  const int16x8_t x1=vld1q_s16((const int16_t *)(const void *)(src+4));
  const int16x8_t x2=vld1q_s16((const int16_t *)(const void *)(src+8));
  int16x8_t z0,z1,z2;
  neon_dft3_q15(x0,x1,x2,&z0,&z1,&z2);
  z0=neon_real_mul_q15(z0,Q15_INV_SQRT3);
  z1=neon_cmul_q15(z1,vld1q_s16(g_dft12_scaled_tw_q15[0]));
  z2=neon_cmul_q15(z2,vld1q_s16(g_dft12_scaled_tw_q15[1]));
  const uint32x4x3_t o = {
    .val = {
      vreinterpretq_u32_s16(neon_dft4_packed_q15(z0)),
      vreinterpretq_u32_s16(neon_dft4_packed_q15(z1)),
      vreinterpretq_u32_s16(neon_dft4_packed_q15(z2))
    }
  };
  vst3q_u32((uint32_t *)(void *)dst,o);
}

static inline void neon_dft8_q15(const c16_t *src, c16_t *dst)
{
  neon_m128i y0,y1,y2,y3,y4,y5,y6,y7;
  dft8x4_q15_unitary_native(
      neon_broadcast_c16(src[0]), neon_broadcast_c16(src[1]),
      neon_broadcast_c16(src[2]), neon_broadcast_c16(src[3]),
      neon_broadcast_c16(src[4]), neon_broadcast_c16(src[5]),
      neon_broadcast_c16(src[6]), neon_broadcast_c16(src[7]),
      &y0,&y1,&y2,&y3,&y4,&y5,&y6,&y7);
  dst[0]=neon_lane0_c16(y0); dst[1]=neon_lane0_c16(y1);
  dst[2]=neon_lane0_c16(y2); dst[3]=neon_lane0_c16(y3);
  dst[4]=neon_lane0_c16(y4); dst[5]=neon_lane0_c16(y5);
  dst[6]=neon_lane0_c16(y6); dst[7]=neon_lane0_c16(y7);
}


/* ------------------------------------------------------------------------
 * Native NEON power-of-two transforms used as mixed-radix terminal leaves.
 * Each entry processes one unitary N-point transform.
 *
 *   16  = radix-4 x DFT4
 *   128 = radix-2 x DFT64
 *   256 = radix-4 x DFT64
 * ------------------------------------------------------------------------ */
static int16_t g_neon_tw16[3][8] __attribute__((aligned(64)));
static int16_t g_neon_tw128[1][128] __attribute__((aligned(64)));
static int16_t g_neon_tw256[3][128] __attribute__((aligned(64)));

static void neon_init_p2_twiddles(void)
{
  struct spec { int N,R,M; int16_t *base; int stride; } sp[] = {
    {16, 4, 4, &g_neon_tw16[0][0], 8},
    {128,2,64, &g_neon_tw128[0][0],128},
    {256,4,64, &g_neon_tw256[0][0],128},
  };
  for (unsigned si=0; si<sizeof(sp)/sizeof(sp[0]); ++si) {
    for (int br=1; br<sp[si].R; ++br) {
      int16_t *tw=sp[si].base+(br-1)*sp[si].stride;
      for (int k=0;k<sp[si].M;k++) {
        const float a=-2.0f*(float)M_PI*(float)(br*k)/(float)sp[si].N;
        tw[2*k+0]=q15_from_float(cosf(a));
        tw[2*k+1]=q15_from_float(sinf(a));
      }
    }
  }
}

typedef void (*neon_child_q15_fn_t)(const c16_t *, c16_t *);

static void neon_r2_parent_q15(const c16_t *src,c16_t *dst,int N,
                                          neon_child_q15_fn_t child,
                                          const int16_t *tw1)
{
  const int M=N/2;
  c16_t b0[64] __attribute__((aligned(64)));
  c16_t b1[64] __attribute__((aligned(64)));
  c16_t y0[64] __attribute__((aligned(64)));
  c16_t y1[64] __attribute__((aligned(64)));
  AssertFatal(M<=64,"NEON radix-2 child too large N=%d\n",N);
  for(int off=0;off<M;off+=4){
    int16x8_t x0=vld1q_s16((const int16_t *)(const void *)(src+off));
    int16x8_t x1=vld1q_s16((const int16_t *)(const void *)(src+M+off));
    /* Apply the radix-2 1/sqrt(2) scale before the add/sub butterfly. */
    x0=neon_real_mul_q15(x0,Q15_INV_SQRT2);
    x1=neon_real_mul_q15(x1,Q15_INV_SQRT2);
    int16x8_t z0=vqaddq_s16(x0,x1);
    int16x8_t z1=vqsubq_s16(x0,x1);
    z1=neon_cmul_q15(z1,vld1q_s16(tw1+2*off));
    vst1q_s16((int16_t *)(void *)(b0+off),z0);
    vst1q_s16((int16_t *)(void *)(b1+off),z1);
  }
  child(b0,y0); child(b1,y1);
  for(int k=0;k<M;k+=4){
    const uint32x4_t a=vreinterpretq_u32_s16(vld1q_s16((const int16_t *)(const void *)(y0+k)));
    const uint32x4_t b=vreinterpretq_u32_s16(vld1q_s16((const int16_t *)(const void *)(y1+k)));
    vst1q_u32((uint32_t *)(void *)(dst+2*k+0),vzip1q_u32(a,b));
    vst1q_u32((uint32_t *)(void *)(dst+2*k+4),vzip2q_u32(a,b));
  }
}

static void neon_r4_parent_q15(const c16_t *src,c16_t *dst,int N,
                                          neon_child_q15_fn_t child,
                                          const int16_t *tw1,const int16_t *tw2,const int16_t *tw3)
{
  const int M=N/4;
  c16_t b[4][64] __attribute__((aligned(64)));
  c16_t y[4][64] __attribute__((aligned(64)));
  AssertFatal(M<=64,"NEON radix-4 child too large N=%d\n",N);
  for(int off=0;off<M;off+=4){
    int16x8_t x0=vld1q_s16((const int16_t *)(const void *)(src+0*M+off));
    int16x8_t x1=vld1q_s16((const int16_t *)(const void *)(src+1*M+off));
    int16x8_t x2=vld1q_s16((const int16_t *)(const void *)(src+2*M+off));
    int16x8_t x3=vld1q_s16((const int16_t *)(const void *)(src+3*M+off));
    x0=neon_real_mul_q15(x0,Q15_HALF);
    x1=neon_real_mul_q15(x1,Q15_HALF);
    x2=neon_real_mul_q15(x2,Q15_HALF);
    x3=neon_real_mul_q15(x3,Q15_HALF);
    neon_m128i z0,z1,z2,z3;
    dft4x4_q15_noscale(neon128_from_i16(x0),neon128_from_i16(x1),
                        neon128_from_i16(x2),neon128_from_i16(x3),&z0,&z1,&z2,&z3);
    int16x8_t q0=neon128_as_i16(z0);
    int16x8_t q1=neon_cmul_q15(neon128_as_i16(z1),vld1q_s16(tw1+2*off));
    int16x8_t q2=neon_cmul_q15(neon128_as_i16(z2),vld1q_s16(tw2+2*off));
    int16x8_t q3=neon_cmul_q15(neon128_as_i16(z3),vld1q_s16(tw3+2*off));
    vst1q_s16((int16_t *)(void *)(b[0]+off),q0);
    vst1q_s16((int16_t *)(void *)(b[1]+off),q1);
    vst1q_s16((int16_t *)(void *)(b[2]+off),q2);
    vst1q_s16((int16_t *)(void *)(b[3]+off),q3);
  }
  for(int br=0;br<4;br++) child(b[br],y[br]);
  for(int k=0;k<M;k+=4){
    uint32x4x4_t o;
    o.val[0]=vreinterpretq_u32_s16(vld1q_s16((const int16_t *)(const void *)(y[0]+k)));
    o.val[1]=vreinterpretq_u32_s16(vld1q_s16((const int16_t *)(const void *)(y[1]+k)));
    o.val[2]=vreinterpretq_u32_s16(vld1q_s16((const int16_t *)(const void *)(y[2]+k)));
    o.val[3]=vreinterpretq_u32_s16(vld1q_s16((const int16_t *)(const void *)(y[3]+k)));
    vst4q_u32((uint32_t *)(void *)(dst+4*k),o);
  }
}

static void neon_dft16_q15(const c16_t *src,c16_t *dst)
{
  neon_r4_parent_q15(src,dst,16,neon_dft4_q15,
      g_neon_tw16[0],g_neon_tw16[1],g_neon_tw16[2]);
}

static void neon_dft64_child_q15(const c16_t *src,c16_t *dst)
{
  dft64_radix4_q15_neon(src,dst);
}

static void neon_dft128_q15(const c16_t *src,c16_t *dst)
{
  neon_r2_parent_q15(src,dst,128,neon_dft64_child_q15,g_neon_tw128[0]);
}

static void neon_dft256_q15(const c16_t *src,c16_t *dst)
{
  neon_r4_parent_q15(src,dst,256,neon_dft64_child_q15,
      g_neon_tw256[0],g_neon_tw256[1],g_neon_tw256[2]);
}

static inline void neon_dft5_q15(
    int16x8_t x0, int16x8_t x1, int16x8_t x2, int16x8_t x3, int16x8_t x4,
    int16x8_t *y0, int16x8_t *y1, int16x8_t *y2, int16x8_t *y3, int16x8_t *y4)
{
  const int16x8_t t1=vqaddq_s16(x1,x4), t2=vqaddq_s16(x2,x3);
  const int16x8_t d1=vqsubq_s16(x1,x4), d2=vqsubq_s16(x2,x3);
  *y0=vqaddq_s16(x0,vqaddq_s16(t1,t2));

  const int16x8_t b1=vqaddq_s16(x0,vqaddq_s16(
      neon_real_mul_q15(t1,10126),
      neon_real_mul_q15(t2,-26510)));
  const int16x8_t q1=vqaddq_s16(
      neon_real_mul_q15(d1,31163),
      neon_real_mul_q15(d2,19260));
  *y1=vqaddq_s16(b1,neon_rot270_q15(q1));
  *y4=vqaddq_s16(b1,neon_rot90_q15(q1));

  const int16x8_t b2=vqaddq_s16(x0,vqaddq_s16(
      neon_real_mul_q15(t1,-26510),
      neon_real_mul_q15(t2,10126)));
  const int16x8_t q2=vqsubq_s16(
      neon_real_mul_q15(d1,19260),
      neon_real_mul_q15(d2,31163));
  *y2=vqaddq_s16(b2,neon_rot270_q15(q2));
  *y3=vqaddq_s16(b2,neon_rot90_q15(q2));
}

static inline void neon_idft3_q15(
    int16x8_t x0, int16x8_t x1, int16x8_t x2,
    int16x8_t *y0, int16x8_t *y1, int16x8_t *y2)
{
  int16x8_t f0, f1, f2;
  neon_dft3_q15(x0, x1, x2, &f0, &f1, &f2);
  *y0 = f0; *y1 = f2; *y2 = f1;
}

static inline void neon_idft5_q15(
    int16x8_t x0, int16x8_t x1, int16x8_t x2, int16x8_t x3, int16x8_t x4,
    int16x8_t *y0, int16x8_t *y1, int16x8_t *y2, int16x8_t *y3, int16x8_t *y4)
{
  int16x8_t f0, f1, f2, f3, f4;
  neon_dft5_q15(x0, x1, x2, x3, x4, &f0, &f1, &f2, &f3, &f4);
  *y0 = f0; *y1 = f4; *y2 = f3; *y3 = f2; *y4 = f1;
}



static void neon_leaf_q15(const c16_t *src,c16_t *dst,int N)
{
  switch(N){
    case 1: dst[0]=src[0]; return;
    case 4: neon_dft4_q15(src,dst); return;
    case 8: neon_dft8_q15(src,dst); return;
    case 16: neon_dft16_q15(src,dst); return;
    case 32: dft32_q15_native(src,dst); return;
    case 64: dft64_radix4_q15_neon(src,dst); return;
    case 128: neon_dft128_q15(src,dst); return;
    case 256: neon_dft256_q15(src,dst); return;
    default:
      AssertFatal(N>=512 && is_power_of_two_int(N),"Invalid NEON power-of-two leaf N=%d\n",N);
      dft_power2_q15(src, dst, N, DFT_DIR_FORWARD);
      return;
  }
}


/* -------------------------------------------------------------------------
 * NEON mixed-radix fixed-plan executor.
 * Branch zero is scaled directly; nonzero branches use scaled twiddles.
 * ------------------------------------------------------------------------- */

static inline void neon_235_leaf_q15(const mixed_plan_t *p,
                                           const c16_t *src,
                                           c16_t *dst)
{
  AssertFatal(p->leaf_n == 1 || p->leaf_n == 12 || is_power_of_two_int(p->leaf_n),
              "NEON fused planner requires a power-of-two/DFT12 leaf, got N=%d\n",
              p->leaf_n);
  if (p->leaf_n == 12) {
    neon_dft12_q15(src, dst);
    return;
  }
  neon_leaf_q15(src, dst, p->leaf_n);
}

static void neon_235_exec_q15_rec(const mixed_plan_t *p,
                                        int level,
                                        const c16_t *src,
                                        c16_t *dst,
                                        c16_t *work)
{
  if (level == p->depth) {
    neon_235_leaf_q15(p, src, dst);
    return;
  }

  const mixed_parent_twiddle_t *tw = &p->tw[level];
  const int N = tw->N;
  const int M = tw->M;
  const int r = tw->radix;
  c16_t *b = work;
  c16_t *y = work + N;
  c16_t *child_work = work + 2 * N;
  AssertFatal((M & 3) == 0,
              "NEON fused mixed child must be multiple of four: N=%d r=%d M=%d\n",
              N, r, M);

  /* Radix-9 = R3 x R3; the first R3 outputs feed the second R3 directly. */
  if (r == 9) {
    int rr, c3, c5, first, second;
    AssertFatal(mixed_stage_info(p->stage_code[level],
                                        &rr, &c3, &c5, &first, &second) &&
                    rr == 9 && first == 3 && second == 3,
                "Invalid NEON radix-9 stage code=%u radix=%d\n",
                (unsigned)p->stage_code[level], r);
    (void)c3;
    (void)c5;

    const mixed_parent_twiddle_t *twB = &p->fused_first_tw[level];
    const mixed_parent_twiddle_t *twA = &p->fused_second_tw[level];
    AssertFatal(twB->radix == 3 && twB->M == 3 * M &&
                    twA->radix == 3 && twA->M == M,
                "Bad register radix-9 twiddles N=%d M=%d\n", N, M);

    for (int off = 0; off < M; off += 4) {
      int16x8_t s00,s01,s02,s10,s11,s12,s20,s21,s22;

      {
        const int16x8_t x0 = vld1q_s16((const int16_t *)(const void *)(src + 0 * M + off));
        const int16x8_t x1 = vld1q_s16((const int16_t *)(const void *)(src + 3 * M + off));
        const int16x8_t x2 = vld1q_s16((const int16_t *)(const void *)(src + 6 * M + off));
        neon_dft3_q15(x0,x1,x2,&s00,&s01,&s02);
        s00 = neon_real_mul_q15(s00,Q15_INV_SQRT3);
        s01 = neon_cmul_q15(s01,vld1q_s16(mixed_twiddle_forward(twB, 0) + 2 * off));
        s02 = neon_cmul_q15(s02,vld1q_s16(mixed_twiddle_forward(twB, 1) + 2 * off));
      }
      {
        const int first_off = M + off;
        const int16x8_t x0 = vld1q_s16((const int16_t *)(const void *)(src + 1 * M + off));
        const int16x8_t x1 = vld1q_s16((const int16_t *)(const void *)(src + 4 * M + off));
        const int16x8_t x2 = vld1q_s16((const int16_t *)(const void *)(src + 7 * M + off));
        neon_dft3_q15(x0,x1,x2,&s10,&s11,&s12);
        s10 = neon_real_mul_q15(s10,Q15_INV_SQRT3);
        s11 = neon_cmul_q15(s11,vld1q_s16(mixed_twiddle_forward(twB, 0) + 2 * first_off));
        s12 = neon_cmul_q15(s12,vld1q_s16(mixed_twiddle_forward(twB, 1) + 2 * first_off));
      }
      {
        const int first_off = 2 * M + off;
        const int16x8_t x0 = vld1q_s16((const int16_t *)(const void *)(src + 2 * M + off));
        const int16x8_t x1 = vld1q_s16((const int16_t *)(const void *)(src + 5 * M + off));
        const int16x8_t x2 = vld1q_s16((const int16_t *)(const void *)(src + 8 * M + off));
        neon_dft3_q15(x0,x1,x2,&s20,&s21,&s22);
        s20 = neon_real_mul_q15(s20,Q15_INV_SQRT3);
        s21 = neon_cmul_q15(s21,vld1q_s16(mixed_twiddle_forward(twB, 0) + 2 * first_off));
        s22 = neon_cmul_q15(s22,vld1q_s16(mixed_twiddle_forward(twB, 1) + 2 * first_off));
      }

      {
        int16x8_t z0,z1,z2;
        neon_dft3_q15(s00,s10,s20,&z0,&z1,&z2);
        z0 = neon_real_mul_q15(z0,Q15_INV_SQRT3);
        z1 = neon_cmul_q15(z1,vld1q_s16(mixed_twiddle_forward(twA, 0) + 2 * off));
        z2 = neon_cmul_q15(z2,vld1q_s16(mixed_twiddle_forward(twA, 1) + 2 * off));
        vst1q_s16((int16_t *)(void *)(b + 0 * M + off),z0);
        vst1q_s16((int16_t *)(void *)(b + 3 * M + off),z1);
        vst1q_s16((int16_t *)(void *)(b + 6 * M + off),z2);
      }
      {
        int16x8_t z0,z1,z2;
        neon_dft3_q15(s01,s11,s21,&z0,&z1,&z2);
        z0 = neon_real_mul_q15(z0,Q15_INV_SQRT3);
        z1 = neon_cmul_q15(z1,vld1q_s16(mixed_twiddle_forward(twA, 0) + 2 * off));
        z2 = neon_cmul_q15(z2,vld1q_s16(mixed_twiddle_forward(twA, 1) + 2 * off));
        vst1q_s16((int16_t *)(void *)(b + 1 * M + off),z0);
        vst1q_s16((int16_t *)(void *)(b + 4 * M + off),z1);
        vst1q_s16((int16_t *)(void *)(b + 7 * M + off),z2);
      }
      {
        int16x8_t z0,z1,z2;
        neon_dft3_q15(s02,s12,s22,&z0,&z1,&z2);
        z0 = neon_real_mul_q15(z0,Q15_INV_SQRT3);
        z1 = neon_cmul_q15(z1,vld1q_s16(mixed_twiddle_forward(twA, 0) + 2 * off));
        z2 = neon_cmul_q15(z2,vld1q_s16(mixed_twiddle_forward(twA, 1) + 2 * off));
        vst1q_s16((int16_t *)(void *)(b + 2 * M + off),z0);
        vst1q_s16((int16_t *)(void *)(b + 5 * M + off),z1);
        vst1q_s16((int16_t *)(void *)(b + 8 * M + off),z2);
      }
    }

    for (int br = 0; br < 9; br++)
      neon_235_exec_q15_rec(p, level + 1,
                                 b + br * M, y + br * M, child_work);

    for (int k = 0; k < M; k += 4) {
      const int16x8_t v0 = vld1q_s16((const int16_t *)(const void *)(y + 0 * M + k));
      const int16x8_t v1 = vld1q_s16((const int16_t *)(const void *)(y + 1 * M + k));
      const int16x8_t v2 = vld1q_s16((const int16_t *)(const void *)(y + 2 * M + k));
      const int16x8_t v3 = vld1q_s16((const int16_t *)(const void *)(y + 3 * M + k));
      const int16x8_t v4 = vld1q_s16((const int16_t *)(const void *)(y + 4 * M + k));
      const int16x8_t v5 = vld1q_s16((const int16_t *)(const void *)(y + 5 * M + k));
      const int16x8_t v6 = vld1q_s16((const int16_t *)(const void *)(y + 6 * M + k));
      const int16x8_t v7 = vld1q_s16((const int16_t *)(const void *)(y + 7 * M + k));
      const uint32x4_t v8 = vreinterpretq_u32_s16(
          vld1q_s16((const int16_t *)(const void *)(y + 8 * M + k)));
      int16x8_t a0,a1,a2,a3,c0,c1,c2,c3;
      neon_transpose4_q15(v0,v1,v2,v3,&a0,&a1,&a2,&a3);
      neon_transpose4_q15(v4,v5,v6,v7,&c0,&c1,&c2,&c3);

#define NEON_R9_STORE_ROW(LANE, A, C) do { \
        uint32_t *d9 = (uint32_t *)(void *)(dst + 9 * (k + (LANE))); \
        vst1q_u32(d9 + 0, vreinterpretq_u32_s16((A))); \
        vst1q_u32(d9 + 4, vreinterpretq_u32_s16((C))); \
        vst1q_lane_u32(d9 + 8, v8, (LANE)); \
      } while (0)
      NEON_R9_STORE_ROW(0,a0,c0);
      NEON_R9_STORE_ROW(1,a1,c1);
      NEON_R9_STORE_ROW(2,a2,c2);
      NEON_R9_STORE_ROW(3,a3,c3);
#undef NEON_R9_STORE_ROW
    }
    return;
  }

  if (r == 15 || r == 25) {
    int rr, c3, c5, first, second;
    AssertFatal(mixed_stage_info(p->stage_code[level],
                                        &rr, &c3, &c5, &first, &second) &&
                    rr == r && second != 0,
                "Invalid fused NEON 235 stage code=%u radix=%d\n",
                (unsigned)p->stage_code[level], r);
    (void)c3;
    (void)c5;

    /* Fused R3/R5 parent pair using a local four-complex intermediate tile. */
    const int B = first;
    const int A = second;
    const mixed_parent_twiddle_t *twB = &p->fused_first_tw[level];
    const mixed_parent_twiddle_t *twA = &p->fused_second_tw[level];
    AssertFatal(twB->radix == B && twB->M == A * M &&
                    twA->radix == A && twA->M == M,
                "Bad fused NEON twiddles N=%d B=%d A=%d M=%d\n",
                N, B, A, M);

    for (int off = 0; off < M; off += 4) {
      c16_t stage1[25][4] __attribute__((aligned(64)));

      /* First parent B for every subindex a of the second parent A. */
      for (int a = 0; a < A; a++) {
        const int first_off = a * M + off;
        if (B == 3) {
          const int16x8_t x0 = vld1q_s16((const int16_t *)(const void *)(src + (0 * A + a) * M + off));
          const int16x8_t x1 = vld1q_s16((const int16_t *)(const void *)(src + (1 * A + a) * M + off));
          const int16x8_t x2 = vld1q_s16((const int16_t *)(const void *)(src + (2 * A + a) * M + off));
          int16x8_t z0, z1, z2;
          neon_dft3_q15(x0, x1, x2, &z0, &z1, &z2);
          z0 = neon_real_mul_q15(z0, Q15_INV_SQRT3);
          z1 = neon_cmul_q15(z1, vld1q_s16(mixed_twiddle_forward(twB, 0) + 2 * first_off));
          z2 = neon_cmul_q15(z2, vld1q_s16(mixed_twiddle_forward(twB, 1) + 2 * first_off));
          vst1q_s16((int16_t *)(void *)stage1[a * B + 0], z0);
          vst1q_s16((int16_t *)(void *)stage1[a * B + 1], z1);
          vst1q_s16((int16_t *)(void *)stage1[a * B + 2], z2);
        } else {
          AssertFatal(B == 5, "Invalid fused NEON first radix B=%d\n", B);
          const int16x8_t x0 = vld1q_s16((const int16_t *)(const void *)(src + (0 * A + a) * M + off));
          const int16x8_t x1 = vld1q_s16((const int16_t *)(const void *)(src + (1 * A + a) * M + off));
          const int16x8_t x2 = vld1q_s16((const int16_t *)(const void *)(src + (2 * A + a) * M + off));
          const int16x8_t x3 = vld1q_s16((const int16_t *)(const void *)(src + (3 * A + a) * M + off));
          const int16x8_t x4 = vld1q_s16((const int16_t *)(const void *)(src + (4 * A + a) * M + off));
          int16x8_t z0, z1, z2, z3, z4;
          neon_dft5_q15(x0, x1, x2, x3, x4, &z0, &z1, &z2, &z3, &z4);
          z0 = neon_real_mul_q15(z0, Q15_INV_SQRT5);
          z1 = neon_cmul_q15(z1, vld1q_s16(mixed_twiddle_forward(twB, 0) + 2 * first_off));
          z2 = neon_cmul_q15(z2, vld1q_s16(mixed_twiddle_forward(twB, 1) + 2 * first_off));
          z3 = neon_cmul_q15(z3, vld1q_s16(mixed_twiddle_forward(twB, 2) + 2 * first_off));
          z4 = neon_cmul_q15(z4, vld1q_s16(mixed_twiddle_forward(twB, 3) + 2 * first_off));
          vst1q_s16((int16_t *)(void *)stage1[a * B + 0], z0);
          vst1q_s16((int16_t *)(void *)stage1[a * B + 1], z1);
          vst1q_s16((int16_t *)(void *)stage1[a * B + 2], z2);
          vst1q_s16((int16_t *)(void *)stage1[a * B + 3], z3);
          vst1q_s16((int16_t *)(void *)stage1[a * B + 4], z4);
        }
      }

      /* Second parent A consumes the local first-parent tile. */
      for (int bidx = 0; bidx < B; bidx++) {
        if (A == 3) {
          const int16x8_t x0 = vld1q_s16((const int16_t *)(const void *)stage1[0 * B + bidx]);
          const int16x8_t x1 = vld1q_s16((const int16_t *)(const void *)stage1[1 * B + bidx]);
          const int16x8_t x2 = vld1q_s16((const int16_t *)(const void *)stage1[2 * B + bidx]);
          int16x8_t z0, z1, z2;
          neon_dft3_q15(x0, x1, x2, &z0, &z1, &z2);
          z0 = neon_real_mul_q15(z0, Q15_INV_SQRT3);
          z1 = neon_cmul_q15(z1, vld1q_s16(mixed_twiddle_forward(twA, 0) + 2 * off));
          z2 = neon_cmul_q15(z2, vld1q_s16(mixed_twiddle_forward(twA, 1) + 2 * off));
          vst1q_s16((int16_t *)(void *)(b + (bidx + B * 0) * M + off), z0);
          vst1q_s16((int16_t *)(void *)(b + (bidx + B * 1) * M + off), z1);
          vst1q_s16((int16_t *)(void *)(b + (bidx + B * 2) * M + off), z2);
        } else {
          AssertFatal(A == 5, "Invalid fused NEON second radix A=%d\n", A);
          const int16x8_t x0 = vld1q_s16((const int16_t *)(const void *)stage1[0 * B + bidx]);
          const int16x8_t x1 = vld1q_s16((const int16_t *)(const void *)stage1[1 * B + bidx]);
          const int16x8_t x2 = vld1q_s16((const int16_t *)(const void *)stage1[2 * B + bidx]);
          const int16x8_t x3 = vld1q_s16((const int16_t *)(const void *)stage1[3 * B + bidx]);
          const int16x8_t x4 = vld1q_s16((const int16_t *)(const void *)stage1[4 * B + bidx]);
          int16x8_t z0, z1, z2, z3, z4;
          neon_dft5_q15(x0, x1, x2, x3, x4, &z0, &z1, &z2, &z3, &z4);
          z0 = neon_real_mul_q15(z0, Q15_INV_SQRT5);
          z1 = neon_cmul_q15(z1, vld1q_s16(mixed_twiddle_forward(twA, 0) + 2 * off));
          z2 = neon_cmul_q15(z2, vld1q_s16(mixed_twiddle_forward(twA, 1) + 2 * off));
          z3 = neon_cmul_q15(z3, vld1q_s16(mixed_twiddle_forward(twA, 2) + 2 * off));
          z4 = neon_cmul_q15(z4, vld1q_s16(mixed_twiddle_forward(twA, 3) + 2 * off));
          vst1q_s16((int16_t *)(void *)(b + (bidx + B * 0) * M + off), z0);
          vst1q_s16((int16_t *)(void *)(b + (bidx + B * 1) * M + off), z1);
          vst1q_s16((int16_t *)(void *)(b + (bidx + B * 2) * M + off), z2);
          vst1q_s16((int16_t *)(void *)(b + (bidx + B * 3) * M + off), z3);
          vst1q_s16((int16_t *)(void *)(b + (bidx + B * 4) * M + off), z4);
        }
      }
    }

    for (int br = 0; br < r; br++)
      neon_235_exec_q15_rec(p, level + 1,
                                 b + br * M, y + br * M, child_work);

    for (int k = 0; k < M; k += 4) {
      c16_t tile[25][4] __attribute__((aligned(64)));
      for (int br = 0; br < r; br++)
        vst1q_s16((int16_t *)(void *)tile[br],
                  vld1q_s16((const int16_t *)(const void *)(y + br * M + k)));
      for (int lane = 0; lane < 4; lane++)
        for (int br = 0; br < r; br++)
          dst[r * (k + lane) + br] = tile[br][lane];
    }
    return;
  }

  if (r == 3) {
    for (int off = 0; off < M; off += 4) {
      const int16x8_t x0 = vld1q_s16((const int16_t *)(const void *)(src + 0 * M + off));
      const int16x8_t x1 = vld1q_s16((const int16_t *)(const void *)(src + 1 * M + off));
      const int16x8_t x2 = vld1q_s16((const int16_t *)(const void *)(src + 2 * M + off));
      int16x8_t z0, z1, z2;
      neon_dft3_q15(x0, x1, x2, &z0, &z1, &z2);
      z0 = neon_real_mul_q15(z0, Q15_INV_SQRT3);
      z1 = neon_cmul_q15(z1, vld1q_s16(mixed_twiddle_forward(tw, 0) + 2 * off));
      z2 = neon_cmul_q15(z2, vld1q_s16(mixed_twiddle_forward(tw, 1) + 2 * off));
      vst1q_s16((int16_t *)(void *)(b + 0 * M + off), z0);
      vst1q_s16((int16_t *)(void *)(b + 1 * M + off), z1);
      vst1q_s16((int16_t *)(void *)(b + 2 * M + off), z2);
    }
    for (int br = 0; br < 3; br++)
      neon_235_exec_q15_rec(p, level + 1,
                                 b + br * M, y + br * M, child_work);
    for (int k = 0; k < M; k += 4) {
      uint32x4x3_t out;
      out.val[0] = vreinterpretq_u32_s16(vld1q_s16((const int16_t *)(const void *)(y + 0 * M + k)));
      out.val[1] = vreinterpretq_u32_s16(vld1q_s16((const int16_t *)(const void *)(y + 1 * M + k)));
      out.val[2] = vreinterpretq_u32_s16(vld1q_s16((const int16_t *)(const void *)(y + 2 * M + k)));
      vst3q_u32((uint32_t *)(void *)(dst + 3 * k), out);
    }
    return;
  }

  AssertFatal(r == 5, "Invalid planned NEON radix %d N=%d\n", r, N);
  for (int off = 0; off < M; off += 4) {
    const int16x8_t x0 = vld1q_s16((const int16_t *)(const void *)(src + 0 * M + off));
    const int16x8_t x1 = vld1q_s16((const int16_t *)(const void *)(src + 1 * M + off));
    const int16x8_t x2 = vld1q_s16((const int16_t *)(const void *)(src + 2 * M + off));
    const int16x8_t x3 = vld1q_s16((const int16_t *)(const void *)(src + 3 * M + off));
    const int16x8_t x4 = vld1q_s16((const int16_t *)(const void *)(src + 4 * M + off));
    int16x8_t z0, z1, z2, z3, z4;
    neon_dft5_q15(x0, x1, x2, x3, x4, &z0, &z1, &z2, &z3, &z4);
    z0 = neon_real_mul_q15(z0, Q15_INV_SQRT5);
    z1 = neon_cmul_q15(z1, vld1q_s16(mixed_twiddle_forward(tw, 0) + 2 * off));
    z2 = neon_cmul_q15(z2, vld1q_s16(mixed_twiddle_forward(tw, 1) + 2 * off));
    z3 = neon_cmul_q15(z3, vld1q_s16(mixed_twiddle_forward(tw, 2) + 2 * off));
    z4 = neon_cmul_q15(z4, vld1q_s16(mixed_twiddle_forward(tw, 3) + 2 * off));
    vst1q_s16((int16_t *)(void *)(b + 0 * M + off), z0);
    vst1q_s16((int16_t *)(void *)(b + 1 * M + off), z1);
    vst1q_s16((int16_t *)(void *)(b + 2 * M + off), z2);
    vst1q_s16((int16_t *)(void *)(b + 3 * M + off), z3);
    vst1q_s16((int16_t *)(void *)(b + 4 * M + off), z4);
  }
  for (int br = 0; br < 5; br++)
    neon_235_exec_q15_rec(p, level + 1,
                               b + br * M, y + br * M, child_work);
  for (int k = 0; k < M; k += 4) {
    c16_t tile[5][4] __attribute__((aligned(64)));
    for (int br = 0; br < 5; br++)
      vst1q_s16((int16_t *)(void *)tile[br],
                vld1q_s16((const int16_t *)(const void *)(y + br * M + k)));
    for (int lane = 0; lane < 4; lane++)
      for (int br = 0; br < 5; br++)
        dst[5 * (k + lane) + br] = tile[br][lane];
  }
}

static void neon_235_exec_q15_rec_inverse(const mixed_plan_t *p,
                                        int level,
                                        const c16_t *src,
                                        c16_t *dst,
                                        c16_t *work)
{
  if (level == p->depth) {
    mixed_inverse_leaf_q15(p, src, dst);
    return;
  }

  const mixed_parent_twiddle_t *tw = &p->tw[level];
  const int N = tw->N;
  const int M = tw->M;
  const int r = tw->radix;
  c16_t *b = work;
  c16_t *y = work + N;
  c16_t *child_work = work + 2 * N;
  AssertFatal((M & 3) == 0,
              "NEON fused mixed child must be multiple of four: N=%d r=%d M=%d\n",
              N, r, M);

  /* Radix-9 = R3 x R3; the first R3 outputs feed the second R3 directly. */
  if (r == 9) {
    int rr, c3, c5, first, second;
    AssertFatal(mixed_stage_info(p->stage_code[level],
                                        &rr, &c3, &c5, &first, &second) &&
                    rr == 9 && first == 3 && second == 3,
                "Invalid NEON radix-9 stage code=%u radix=%d\n",
                (unsigned)p->stage_code[level], r);
    (void)c3;
    (void)c5;

    const mixed_parent_twiddle_t *twB = &p->fused_first_tw[level];
    const mixed_parent_twiddle_t *twA = &p->fused_second_tw[level];
    AssertFatal(twB->radix == 3 && twB->M == 3 * M &&
                    twA->radix == 3 && twA->M == M,
                "Bad register radix-9 twiddles N=%d M=%d\n", N, M);

    for (int off = 0; off < M; off += 4) {
      int16x8_t s00,s01,s02,s10,s11,s12,s20,s21,s22;

      {
        const int16x8_t x0 = vld1q_s16((const int16_t *)(const void *)(src + 0 * M + off));
        const int16x8_t x1 = vld1q_s16((const int16_t *)(const void *)(src + 3 * M + off));
        const int16x8_t x2 = vld1q_s16((const int16_t *)(const void *)(src + 6 * M + off));
        neon_idft3_q15(x0,x1,x2,&s00,&s01,&s02);
        s00 = neon_real_mul_q15(s00,Q15_INV_SQRT3);
        s01 = neon_cmul_q15(s01,vld1q_s16(mixed_twiddle_inverse(twB, 0) + 2 * off));
        s02 = neon_cmul_q15(s02,vld1q_s16(mixed_twiddle_inverse(twB, 1) + 2 * off));
      }
      {
        const int first_off = M + off;
        const int16x8_t x0 = vld1q_s16((const int16_t *)(const void *)(src + 1 * M + off));
        const int16x8_t x1 = vld1q_s16((const int16_t *)(const void *)(src + 4 * M + off));
        const int16x8_t x2 = vld1q_s16((const int16_t *)(const void *)(src + 7 * M + off));
        neon_idft3_q15(x0,x1,x2,&s10,&s11,&s12);
        s10 = neon_real_mul_q15(s10,Q15_INV_SQRT3);
        s11 = neon_cmul_q15(s11,vld1q_s16(mixed_twiddle_inverse(twB, 0) + 2 * first_off));
        s12 = neon_cmul_q15(s12,vld1q_s16(mixed_twiddle_inverse(twB, 1) + 2 * first_off));
      }
      {
        const int first_off = 2 * M + off;
        const int16x8_t x0 = vld1q_s16((const int16_t *)(const void *)(src + 2 * M + off));
        const int16x8_t x1 = vld1q_s16((const int16_t *)(const void *)(src + 5 * M + off));
        const int16x8_t x2 = vld1q_s16((const int16_t *)(const void *)(src + 8 * M + off));
        neon_idft3_q15(x0,x1,x2,&s20,&s21,&s22);
        s20 = neon_real_mul_q15(s20,Q15_INV_SQRT3);
        s21 = neon_cmul_q15(s21,vld1q_s16(mixed_twiddle_inverse(twB, 0) + 2 * first_off));
        s22 = neon_cmul_q15(s22,vld1q_s16(mixed_twiddle_inverse(twB, 1) + 2 * first_off));
      }

      {
        int16x8_t z0,z1,z2;
        neon_idft3_q15(s00,s10,s20,&z0,&z1,&z2);
        z0 = neon_real_mul_q15(z0,Q15_INV_SQRT3);
        z1 = neon_cmul_q15(z1,vld1q_s16(mixed_twiddle_inverse(twA, 0) + 2 * off));
        z2 = neon_cmul_q15(z2,vld1q_s16(mixed_twiddle_inverse(twA, 1) + 2 * off));
        vst1q_s16((int16_t *)(void *)(b + 0 * M + off),z0);
        vst1q_s16((int16_t *)(void *)(b + 3 * M + off),z1);
        vst1q_s16((int16_t *)(void *)(b + 6 * M + off),z2);
      }
      {
        int16x8_t z0,z1,z2;
        neon_idft3_q15(s01,s11,s21,&z0,&z1,&z2);
        z0 = neon_real_mul_q15(z0,Q15_INV_SQRT3);
        z1 = neon_cmul_q15(z1,vld1q_s16(mixed_twiddle_inverse(twA, 0) + 2 * off));
        z2 = neon_cmul_q15(z2,vld1q_s16(mixed_twiddle_inverse(twA, 1) + 2 * off));
        vst1q_s16((int16_t *)(void *)(b + 1 * M + off),z0);
        vst1q_s16((int16_t *)(void *)(b + 4 * M + off),z1);
        vst1q_s16((int16_t *)(void *)(b + 7 * M + off),z2);
      }
      {
        int16x8_t z0,z1,z2;
        neon_idft3_q15(s02,s12,s22,&z0,&z1,&z2);
        z0 = neon_real_mul_q15(z0,Q15_INV_SQRT3);
        z1 = neon_cmul_q15(z1,vld1q_s16(mixed_twiddle_inverse(twA, 0) + 2 * off));
        z2 = neon_cmul_q15(z2,vld1q_s16(mixed_twiddle_inverse(twA, 1) + 2 * off));
        vst1q_s16((int16_t *)(void *)(b + 2 * M + off),z0);
        vst1q_s16((int16_t *)(void *)(b + 5 * M + off),z1);
        vst1q_s16((int16_t *)(void *)(b + 8 * M + off),z2);
      }
    }

    for (int br = 0; br < 9; br++)
      neon_235_exec_q15_rec_inverse(p, level + 1,
                                 b + br * M, y + br * M, child_work);

    for (int k = 0; k < M; k += 4) {
      const int16x8_t v0 = vld1q_s16((const int16_t *)(const void *)(y + 0 * M + k));
      const int16x8_t v1 = vld1q_s16((const int16_t *)(const void *)(y + 1 * M + k));
      const int16x8_t v2 = vld1q_s16((const int16_t *)(const void *)(y + 2 * M + k));
      const int16x8_t v3 = vld1q_s16((const int16_t *)(const void *)(y + 3 * M + k));
      const int16x8_t v4 = vld1q_s16((const int16_t *)(const void *)(y + 4 * M + k));
      const int16x8_t v5 = vld1q_s16((const int16_t *)(const void *)(y + 5 * M + k));
      const int16x8_t v6 = vld1q_s16((const int16_t *)(const void *)(y + 6 * M + k));
      const int16x8_t v7 = vld1q_s16((const int16_t *)(const void *)(y + 7 * M + k));
      const uint32x4_t v8 = vreinterpretq_u32_s16(
          vld1q_s16((const int16_t *)(const void *)(y + 8 * M + k)));
      int16x8_t a0,a1,a2,a3,c0,c1,c2,c3;
      neon_transpose4_q15(v0,v1,v2,v3,&a0,&a1,&a2,&a3);
      neon_transpose4_q15(v4,v5,v6,v7,&c0,&c1,&c2,&c3);

#define NEON_R9_STORE_ROW(LANE, A, C) do { \
        uint32_t *d9 = (uint32_t *)(void *)(dst + 9 * (k + (LANE))); \
        vst1q_u32(d9 + 0, vreinterpretq_u32_s16((A))); \
        vst1q_u32(d9 + 4, vreinterpretq_u32_s16((C))); \
        vst1q_lane_u32(d9 + 8, v8, (LANE)); \
      } while (0)
      NEON_R9_STORE_ROW(0,a0,c0);
      NEON_R9_STORE_ROW(1,a1,c1);
      NEON_R9_STORE_ROW(2,a2,c2);
      NEON_R9_STORE_ROW(3,a3,c3);
#undef NEON_R9_STORE_ROW
    }
    return;
  }

  if (r == 15 || r == 25) {
    int rr, c3, c5, first, second;
    AssertFatal(mixed_stage_info(p->stage_code[level],
                                        &rr, &c3, &c5, &first, &second) &&
                    rr == r && second != 0,
                "Invalid fused NEON 235 stage code=%u radix=%d\n",
                (unsigned)p->stage_code[level], r);
    (void)c3;
    (void)c5;

    /* Fused R3/R5 parent pair using a local four-complex intermediate tile. */
    const int B = first;
    const int A = second;
    const mixed_parent_twiddle_t *twB = &p->fused_first_tw[level];
    const mixed_parent_twiddle_t *twA = &p->fused_second_tw[level];
    AssertFatal(twB->radix == B && twB->M == A * M &&
                    twA->radix == A && twA->M == M,
                "Bad fused NEON twiddles N=%d B=%d A=%d M=%d\n",
                N, B, A, M);

    for (int off = 0; off < M; off += 4) {
      c16_t stage1[25][4] __attribute__((aligned(64)));

      /* First parent B for every subindex a of the second parent A. */
      for (int a = 0; a < A; a++) {
        const int first_off = a * M + off;
        if (B == 3) {
          const int16x8_t x0 = vld1q_s16((const int16_t *)(const void *)(src + (0 * A + a) * M + off));
          const int16x8_t x1 = vld1q_s16((const int16_t *)(const void *)(src + (1 * A + a) * M + off));
          const int16x8_t x2 = vld1q_s16((const int16_t *)(const void *)(src + (2 * A + a) * M + off));
          int16x8_t z0, z1, z2;
          neon_idft3_q15(x0, x1, x2, &z0, &z1, &z2);
          z0 = neon_real_mul_q15(z0, Q15_INV_SQRT3);
          z1 = neon_cmul_q15(z1, vld1q_s16(mixed_twiddle_inverse(twB, 0) + 2 * first_off));
          z2 = neon_cmul_q15(z2, vld1q_s16(mixed_twiddle_inverse(twB, 1) + 2 * first_off));
          vst1q_s16((int16_t *)(void *)stage1[a * B + 0], z0);
          vst1q_s16((int16_t *)(void *)stage1[a * B + 1], z1);
          vst1q_s16((int16_t *)(void *)stage1[a * B + 2], z2);
        } else {
          AssertFatal(B == 5, "Invalid fused NEON first radix B=%d\n", B);
          const int16x8_t x0 = vld1q_s16((const int16_t *)(const void *)(src + (0 * A + a) * M + off));
          const int16x8_t x1 = vld1q_s16((const int16_t *)(const void *)(src + (1 * A + a) * M + off));
          const int16x8_t x2 = vld1q_s16((const int16_t *)(const void *)(src + (2 * A + a) * M + off));
          const int16x8_t x3 = vld1q_s16((const int16_t *)(const void *)(src + (3 * A + a) * M + off));
          const int16x8_t x4 = vld1q_s16((const int16_t *)(const void *)(src + (4 * A + a) * M + off));
          int16x8_t z0, z1, z2, z3, z4;
          neon_idft5_q15(x0, x1, x2, x3, x4, &z0, &z1, &z2, &z3, &z4);
          z0 = neon_real_mul_q15(z0, Q15_INV_SQRT5);
          z1 = neon_cmul_q15(z1, vld1q_s16(mixed_twiddle_inverse(twB, 0) + 2 * first_off));
          z2 = neon_cmul_q15(z2, vld1q_s16(mixed_twiddle_inverse(twB, 1) + 2 * first_off));
          z3 = neon_cmul_q15(z3, vld1q_s16(mixed_twiddle_inverse(twB, 2) + 2 * first_off));
          z4 = neon_cmul_q15(z4, vld1q_s16(mixed_twiddle_inverse(twB, 3) + 2 * first_off));
          vst1q_s16((int16_t *)(void *)stage1[a * B + 0], z0);
          vst1q_s16((int16_t *)(void *)stage1[a * B + 1], z1);
          vst1q_s16((int16_t *)(void *)stage1[a * B + 2], z2);
          vst1q_s16((int16_t *)(void *)stage1[a * B + 3], z3);
          vst1q_s16((int16_t *)(void *)stage1[a * B + 4], z4);
        }
      }

      /* Second parent A consumes the local first-parent tile. */
      for (int bidx = 0; bidx < B; bidx++) {
        if (A == 3) {
          const int16x8_t x0 = vld1q_s16((const int16_t *)(const void *)stage1[0 * B + bidx]);
          const int16x8_t x1 = vld1q_s16((const int16_t *)(const void *)stage1[1 * B + bidx]);
          const int16x8_t x2 = vld1q_s16((const int16_t *)(const void *)stage1[2 * B + bidx]);
          int16x8_t z0, z1, z2;
          neon_idft3_q15(x0, x1, x2, &z0, &z1, &z2);
          z0 = neon_real_mul_q15(z0, Q15_INV_SQRT3);
          z1 = neon_cmul_q15(z1, vld1q_s16(mixed_twiddle_inverse(twA, 0) + 2 * off));
          z2 = neon_cmul_q15(z2, vld1q_s16(mixed_twiddle_inverse(twA, 1) + 2 * off));
          vst1q_s16((int16_t *)(void *)(b + (bidx + B * 0) * M + off), z0);
          vst1q_s16((int16_t *)(void *)(b + (bidx + B * 1) * M + off), z1);
          vst1q_s16((int16_t *)(void *)(b + (bidx + B * 2) * M + off), z2);
        } else {
          AssertFatal(A == 5, "Invalid fused NEON second radix A=%d\n", A);
          const int16x8_t x0 = vld1q_s16((const int16_t *)(const void *)stage1[0 * B + bidx]);
          const int16x8_t x1 = vld1q_s16((const int16_t *)(const void *)stage1[1 * B + bidx]);
          const int16x8_t x2 = vld1q_s16((const int16_t *)(const void *)stage1[2 * B + bidx]);
          const int16x8_t x3 = vld1q_s16((const int16_t *)(const void *)stage1[3 * B + bidx]);
          const int16x8_t x4 = vld1q_s16((const int16_t *)(const void *)stage1[4 * B + bidx]);
          int16x8_t z0, z1, z2, z3, z4;
          neon_idft5_q15(x0, x1, x2, x3, x4, &z0, &z1, &z2, &z3, &z4);
          z0 = neon_real_mul_q15(z0, Q15_INV_SQRT5);
          z1 = neon_cmul_q15(z1, vld1q_s16(mixed_twiddle_inverse(twA, 0) + 2 * off));
          z2 = neon_cmul_q15(z2, vld1q_s16(mixed_twiddle_inverse(twA, 1) + 2 * off));
          z3 = neon_cmul_q15(z3, vld1q_s16(mixed_twiddle_inverse(twA, 2) + 2 * off));
          z4 = neon_cmul_q15(z4, vld1q_s16(mixed_twiddle_inverse(twA, 3) + 2 * off));
          vst1q_s16((int16_t *)(void *)(b + (bidx + B * 0) * M + off), z0);
          vst1q_s16((int16_t *)(void *)(b + (bidx + B * 1) * M + off), z1);
          vst1q_s16((int16_t *)(void *)(b + (bidx + B * 2) * M + off), z2);
          vst1q_s16((int16_t *)(void *)(b + (bidx + B * 3) * M + off), z3);
          vst1q_s16((int16_t *)(void *)(b + (bidx + B * 4) * M + off), z4);
        }
      }
    }

    for (int br = 0; br < r; br++)
      neon_235_exec_q15_rec_inverse(p, level + 1,
                                 b + br * M, y + br * M, child_work);

    for (int k = 0; k < M; k += 4) {
      c16_t tile[25][4] __attribute__((aligned(64)));
      for (int br = 0; br < r; br++)
        vst1q_s16((int16_t *)(void *)tile[br],
                  vld1q_s16((const int16_t *)(const void *)(y + br * M + k)));
      for (int lane = 0; lane < 4; lane++)
        for (int br = 0; br < r; br++)
          dst[r * (k + lane) + br] = tile[br][lane];
    }
    return;
  }

  if (r == 3) {
    for (int off = 0; off < M; off += 4) {
      const int16x8_t x0 = vld1q_s16((const int16_t *)(const void *)(src + 0 * M + off));
      const int16x8_t x1 = vld1q_s16((const int16_t *)(const void *)(src + 1 * M + off));
      const int16x8_t x2 = vld1q_s16((const int16_t *)(const void *)(src + 2 * M + off));
      int16x8_t z0, z1, z2;
      neon_idft3_q15(x0, x1, x2, &z0, &z1, &z2);
      z0 = neon_real_mul_q15(z0, Q15_INV_SQRT3);
      z1 = neon_cmul_q15(z1, vld1q_s16(mixed_twiddle_inverse(tw, 0) + 2 * off));
      z2 = neon_cmul_q15(z2, vld1q_s16(mixed_twiddle_inverse(tw, 1) + 2 * off));
      vst1q_s16((int16_t *)(void *)(b + 0 * M + off), z0);
      vst1q_s16((int16_t *)(void *)(b + 1 * M + off), z1);
      vst1q_s16((int16_t *)(void *)(b + 2 * M + off), z2);
    }
    for (int br = 0; br < 3; br++)
      neon_235_exec_q15_rec_inverse(p, level + 1,
                                 b + br * M, y + br * M, child_work);
    for (int k = 0; k < M; k += 4) {
      uint32x4x3_t out;
      out.val[0] = vreinterpretq_u32_s16(vld1q_s16((const int16_t *)(const void *)(y + 0 * M + k)));
      out.val[1] = vreinterpretq_u32_s16(vld1q_s16((const int16_t *)(const void *)(y + 1 * M + k)));
      out.val[2] = vreinterpretq_u32_s16(vld1q_s16((const int16_t *)(const void *)(y + 2 * M + k)));
      vst3q_u32((uint32_t *)(void *)(dst + 3 * k), out);
    }
    return;
  }

  AssertFatal(r == 5, "Invalid planned NEON radix %d N=%d\n", r, N);
  for (int off = 0; off < M; off += 4) {
    const int16x8_t x0 = vld1q_s16((const int16_t *)(const void *)(src + 0 * M + off));
    const int16x8_t x1 = vld1q_s16((const int16_t *)(const void *)(src + 1 * M + off));
    const int16x8_t x2 = vld1q_s16((const int16_t *)(const void *)(src + 2 * M + off));
    const int16x8_t x3 = vld1q_s16((const int16_t *)(const void *)(src + 3 * M + off));
    const int16x8_t x4 = vld1q_s16((const int16_t *)(const void *)(src + 4 * M + off));
    int16x8_t z0, z1, z2, z3, z4;
    neon_idft5_q15(x0, x1, x2, x3, x4, &z0, &z1, &z2, &z3, &z4);
    z0 = neon_real_mul_q15(z0, Q15_INV_SQRT5);
    z1 = neon_cmul_q15(z1, vld1q_s16(mixed_twiddle_inverse(tw, 0) + 2 * off));
    z2 = neon_cmul_q15(z2, vld1q_s16(mixed_twiddle_inverse(tw, 1) + 2 * off));
    z3 = neon_cmul_q15(z3, vld1q_s16(mixed_twiddle_inverse(tw, 2) + 2 * off));
    z4 = neon_cmul_q15(z4, vld1q_s16(mixed_twiddle_inverse(tw, 3) + 2 * off));
    vst1q_s16((int16_t *)(void *)(b + 0 * M + off), z0);
    vst1q_s16((int16_t *)(void *)(b + 1 * M + off), z1);
    vst1q_s16((int16_t *)(void *)(b + 2 * M + off), z2);
    vst1q_s16((int16_t *)(void *)(b + 3 * M + off), z3);
    vst1q_s16((int16_t *)(void *)(b + 4 * M + off), z4);
  }
  for (int br = 0; br < 5; br++)
    neon_235_exec_q15_rec_inverse(p, level + 1,
                               b + br * M, y + br * M, child_work);
  for (int k = 0; k < M; k += 4) {
    c16_t tile[5][4] __attribute__((aligned(64)));
    for (int br = 0; br < 5; br++)
      vst1q_s16((int16_t *)(void *)tile[br],
                vld1q_s16((const int16_t *)(const void *)(y + br * M + k)));
    for (int lane = 0; lane < 4; lane++)
      for (int br = 0; br < 5; br++)
        dst[5 * (k + lane) + br] = tile[br][lane];
  }
}


/* Plan metadata and twiddles are shared read-only after initialization.
 * One embedded slot is reserved for each DFT size exposed by tools_defs.h. */
typedef struct __attribute__((aligned(64))) {
  int initialized;
  mixed_plan_t plan;
} mixed_plan_slot_t;

static pthread_mutex_t g_mixed_plan_mutex = PTHREAD_MUTEX_INITIALIZER;
static mixed_plan_slot_t g_sve2_mixed_plans[DFT_SIZE_IDXTABLESIZE];
static mixed_plan_slot_t g_neon_mixed_plans[DFT_SIZE_IDXTABLESIZE];

static __thread c16_t *g_mixed_work;
static __thread size_t g_mixed_work_elems;

static pthread_once_t g_backend_prepare_once = PTHREAD_ONCE_INIT;
static pthread_once_t g_sve2_prepare_once = PTHREAD_ONCE_INIT;
static int g_backend_prepare_ready;
static int g_sve2_prepare_ready;
static int g_sve2_hw_available;

static void prepare_backend_once(void)
{
  init_native_q15_leaf_twiddles();
  init_dft12_scaled_twiddles();
  neon_init_p2_twiddles();
  g_sve2_hw_available = sve2_runtime_available();
  __atomic_store_n(&g_backend_prepare_ready, 1, __ATOMIC_RELEASE);
}

static void prepare_sve2_once(void)
{
  sve2_dft64_prepare();
  sve2_dft128_prepare();
  sve2_dft256_prepare();
  sve2_dft512_prepare();
  sve2_tiny_prepare();
  __atomic_store_n(&g_sve2_prepare_ready, 1, __ATOMIC_RELEASE);
}

static inline void ensure_backend_prepared(void)
{
  if (__builtin_expect(__atomic_load_n(&g_backend_prepare_ready, __ATOMIC_ACQUIRE), 1))
    return;
  pthread_once(&g_backend_prepare_once, prepare_backend_once);
}

static inline void ensure_sve2_prepared(void)
{
  if (__builtin_expect(__atomic_load_n(&g_sve2_prepare_ready, __ATOMIC_ACQUIRE), 1))
    return;
  pthread_once(&g_sve2_prepare_once, prepare_sve2_once);
}

/* Mixed-radix stage sequences. */
static int mixed_sequence_for_size(int N, int sve2,
                                        unsigned char *stage, int *depth)
{
  if (sve2) {
    switch (N) {
      case 24: stage[0]=MIXED_STAGE_R3; *depth=1; return 1; /* R3 x 8 */
      case 36: stage[0]=MIXED_STAGE_R3; *depth=1; return 1; /* R3 x 12 */
      case 48: stage[0]=MIXED_STAGE_R3; *depth=1; return 1; /* R3 x 16 */
      case 60: stage[0]=MIXED_STAGE_R5; *depth=1; return 1; /* R5 x 12 */
      case 72: stage[0]=MIXED_STAGE_R9; *depth=1; return 1; /* R9 x 8 */
      case 96: stage[0]=MIXED_STAGE_R3; *depth=1; return 1; /* R3 x 32 */
      case 108: stage[0]=MIXED_STAGE_R9; *depth=1; return 1; /* R9 x 12 */
      case 120: stage[0]=MIXED_STAGE_R15_53; *depth=1; return 1; /* R15_53 x 8 */
      case 144: stage[0]=MIXED_STAGE_R9; *depth=1; return 1; /* R9 x 16 */
      case 180: stage[0]=MIXED_STAGE_R15_53; *depth=1; return 1; /* R15_53 x 12 */
      case 192: stage[0]=MIXED_STAGE_R3; *depth=1; return 1; /* R3 x 64 */
      case 216: stage[0]=MIXED_STAGE_R3; stage[1]=MIXED_STAGE_R9; *depth=2; return 1; /* R3 x R9 x 8 */
      case 240: stage[0]=MIXED_STAGE_R15_53; *depth=1; return 1; /* R15_53 x 16 */
      case 288: stage[0]=MIXED_STAGE_R9; *depth=1; return 1; /* R9 x 32 */
      case 300: stage[0]=MIXED_STAGE_R25; *depth=1; return 1; /* R25 x 12 */
      case 324: stage[0]=MIXED_STAGE_R3; stage[1]=MIXED_STAGE_R9; *depth=2; return 1; /* R3 x R9 x 12 */
      case 360: stage[0]=MIXED_STAGE_R3; stage[1]=MIXED_STAGE_R15_53; *depth=2; return 1; /* R3 x R15_53 x 8 */
      case 384: stage[0]=MIXED_STAGE_R3; *depth=1; return 1; /* R3 x 128 */
      case 432: stage[0]=MIXED_STAGE_R3; stage[1]=MIXED_STAGE_R9; *depth=2; return 1; /* R3 x R9 x 16 */
      case 480: stage[0]=MIXED_STAGE_R15_53; *depth=1; return 1; /* R15_53 x 32 */
      case 540: stage[0]=MIXED_STAGE_R3; stage[1]=MIXED_STAGE_R15_53; *depth=2; return 1; /* R3 x R15_53 x 12 */
      case 576: stage[0]=MIXED_STAGE_R9; *depth=1; return 1; /* R9 x 64 */
      case 600: stage[0]=MIXED_STAGE_R3; stage[1]=MIXED_STAGE_R25; *depth=2; return 1; /* R3 x R25 x 8 */
      case 648: stage[0]=MIXED_STAGE_R9; stage[1]=MIXED_STAGE_R9; *depth=2; return 1; /* R9 x R9 x 8 */
      case 720: stage[0]=MIXED_STAGE_R3; stage[1]=MIXED_STAGE_R15_53; *depth=2; return 1; /* R3 x R15_53 x 16 */
      case 768: stage[0]=MIXED_STAGE_R3; *depth=1; return 1; /* R3 x 256 */
      case 864: stage[0]=MIXED_STAGE_R3; stage[1]=MIXED_STAGE_R9; *depth=2; return 1; /* R3 x R9 x 32 */
      case 900: stage[0]=MIXED_STAGE_R3; stage[1]=MIXED_STAGE_R25; *depth=2; return 1; /* R3 x R25 x 12 */
      case 960: stage[0]=MIXED_STAGE_R15_53; *depth=1; return 1; /* R15_53 x 64 */
      case 972: stage[0]=MIXED_STAGE_R9; stage[1]=MIXED_STAGE_R9; *depth=2; return 1; /* R9 x R9 x 12 */
      case 1080: stage[0]=MIXED_STAGE_R9; stage[1]=MIXED_STAGE_R15_53; *depth=2; return 1; /* R9 x R15_53 x 8 */
      case 1152: stage[0]=MIXED_STAGE_R9; *depth=1; return 1; /* R9 x 128 */
      case 1200: stage[0]=MIXED_STAGE_R3; stage[1]=MIXED_STAGE_R25; *depth=2; return 1; /* R3 x R25 x 16 */
      case 1296: stage[0]=MIXED_STAGE_R9; stage[1]=MIXED_STAGE_R9; *depth=2; return 1; /* R9 x R9 x 16 */
      case 1440: stage[0]=MIXED_STAGE_R3; stage[1]=MIXED_STAGE_R15_35; *depth=2; return 1; /* R3 x R15_35 x 32 */
      case 1500: stage[0]=MIXED_STAGE_R5; stage[1]=MIXED_STAGE_R25; *depth=2; return 1; /* R5 x R25 x 12 */
      case 1536: stage[0]=MIXED_STAGE_R3; *depth=1; return 1; /* R3 x 512 */
      case 1620: stage[0]=MIXED_STAGE_R9; stage[1]=MIXED_STAGE_R15_53; *depth=2; return 1; /* R9 x R15_53 x 12 */
      case 1728: stage[0]=MIXED_STAGE_R3; stage[1]=MIXED_STAGE_R9; *depth=2; return 1; /* R3 x R9 x 64 */
      case 1800: stage[0]=MIXED_STAGE_R9; stage[1]=MIXED_STAGE_R25; *depth=2; return 1; /* R9 x R25 x 8 */
      case 1920: stage[0]=MIXED_STAGE_R15_53; *depth=1; return 1; /* R15_53 x 128 */
      case 1944: stage[0]=MIXED_STAGE_R3; stage[1]=MIXED_STAGE_R9; stage[2]=MIXED_STAGE_R9; *depth=3; return 1; /* R3 x R9 x R9 x 8 */
      case 2160: stage[0]=MIXED_STAGE_R9; stage[1]=MIXED_STAGE_R15_53; *depth=2; return 1; /* R9 x R15_53 x 16 */
      case 2304: stage[0]=MIXED_STAGE_R9; *depth=1; return 1; /* R9 x 256 */
      case 2400: stage[0]=MIXED_STAGE_R3; stage[1]=MIXED_STAGE_R25; *depth=2; return 1; /* R3 x R25 x 32 */
      case 2592: stage[0]=MIXED_STAGE_R9; stage[1]=MIXED_STAGE_R9; *depth=2; return 1; /* R9 x R9 x 32 */
      case 3072: stage[0]=MIXED_STAGE_R3; *depth=1; return 1; /* R3 x 1024 */
      case 6144: stage[0]=MIXED_STAGE_R3; *depth=1; return 1; /* R3 x 2048 */
      case 12288: stage[0]=MIXED_STAGE_R3; *depth=1; return 1; /* R3 x 4096 */
      default: break;
    }
  } else {
    switch (N) {
      case 24: stage[0]=MIXED_STAGE_R3; *depth=1; return 1;
      case 36: stage[0]=MIXED_STAGE_R3; *depth=1; return 1;
      case 48: stage[0]=MIXED_STAGE_R3; *depth=1; return 1;
      case 60: stage[0]=MIXED_STAGE_R5; *depth=1; return 1;
      case 72: stage[0]=MIXED_STAGE_R9; *depth=1; return 1;
      case 96: stage[0]=MIXED_STAGE_R3; *depth=1; return 1;
      case 108: stage[0]=MIXED_STAGE_R9; *depth=1; return 1;
      case 120: stage[0]=MIXED_STAGE_R15_35; *depth=1; return 1;
      case 144: stage[0]=MIXED_STAGE_R9; *depth=1; return 1;
      case 180: stage[0]=MIXED_STAGE_R15_35; *depth=1; return 1;
      case 192: stage[0]=MIXED_STAGE_R3; *depth=1; return 1;
      case 216: stage[0]=MIXED_STAGE_R3; stage[1]=MIXED_STAGE_R9; *depth=2; return 1;
      case 240: stage[0]=MIXED_STAGE_R15_35; *depth=1; return 1;
      case 288: stage[0]=MIXED_STAGE_R9; *depth=1; return 1;
      case 300: stage[0]=MIXED_STAGE_R25; *depth=1; return 1;
      case 324: stage[0]=MIXED_STAGE_R3; stage[1]=MIXED_STAGE_R9; *depth=2; return 1;
      case 360: stage[0]=MIXED_STAGE_R9; stage[1]=MIXED_STAGE_R5; *depth=2; return 1;
      case 384: stage[0]=MIXED_STAGE_R3; *depth=1; return 1;
      case 432: stage[0]=MIXED_STAGE_R3; stage[1]=MIXED_STAGE_R9; *depth=2; return 1;
      case 480: stage[0]=MIXED_STAGE_R15_35; *depth=1; return 1;
      case 540: stage[0]=MIXED_STAGE_R9; stage[1]=MIXED_STAGE_R5; *depth=2; return 1;
      case 576: stage[0]=MIXED_STAGE_R9; *depth=1; return 1;
      case 600: stage[0]=MIXED_STAGE_R3; stage[1]=MIXED_STAGE_R25; *depth=2; return 1;
      case 648: stage[0]=MIXED_STAGE_R9; stage[1]=MIXED_STAGE_R9; *depth=2; return 1;
      case 720: stage[0]=MIXED_STAGE_R9; stage[1]=MIXED_STAGE_R5; *depth=2; return 1;
      case 768: stage[0]=MIXED_STAGE_R3; *depth=1; return 1;
      case 864: stage[0]=MIXED_STAGE_R3; stage[1]=MIXED_STAGE_R9; *depth=2; return 1;
      case 900: stage[0]=MIXED_STAGE_R3; stage[1]=MIXED_STAGE_R25; *depth=2; return 1;
      case 960: stage[0]=MIXED_STAGE_R15_35; *depth=1; return 1;
      case 972: stage[0]=MIXED_STAGE_R9; stage[1]=MIXED_STAGE_R9; *depth=2; return 1;
      case 1080: stage[0]=MIXED_STAGE_R15_35; stage[1]=MIXED_STAGE_R9; *depth=2; return 1;
      case 1152: stage[0]=MIXED_STAGE_R9; *depth=1; return 1;
      case 1200: stage[0]=MIXED_STAGE_R3; stage[1]=MIXED_STAGE_R25; *depth=2; return 1;
      case 1296: stage[0]=MIXED_STAGE_R9; stage[1]=MIXED_STAGE_R9; *depth=2; return 1;
      case 1440: stage[0]=MIXED_STAGE_R3; stage[1]=MIXED_STAGE_R15_35; *depth=2; return 1;
      case 1500: stage[0]=MIXED_STAGE_R25; stage[1]=MIXED_STAGE_R5; *depth=2; return 1;
      case 1536: stage[0]=MIXED_STAGE_R3; *depth=1; return 1;
      case 1620: stage[0]=MIXED_STAGE_R15_35; stage[1]=MIXED_STAGE_R9; *depth=2; return 1;
      case 1728: stage[0]=MIXED_STAGE_R9; stage[1]=MIXED_STAGE_R3; *depth=2; return 1;
      case 1800: stage[0]=MIXED_STAGE_R25; stage[1]=MIXED_STAGE_R9; *depth=2; return 1;
      case 1920: stage[0]=MIXED_STAGE_R15_35; *depth=1; return 1;
      case 1944: stage[0]=MIXED_STAGE_R9; stage[1]=MIXED_STAGE_R3; stage[2]=MIXED_STAGE_R9; *depth=3; return 1;
      case 2160: stage[0]=MIXED_STAGE_R15_35; stage[1]=MIXED_STAGE_R9; *depth=2; return 1;
      case 2304: stage[0]=MIXED_STAGE_R9; *depth=1; return 1;
      case 2400: stage[0]=MIXED_STAGE_R3; stage[1]=MIXED_STAGE_R25; *depth=2; return 1;
      case 2592: stage[0]=MIXED_STAGE_R9; stage[1]=MIXED_STAGE_R9; *depth=2; return 1;
      case 3072: stage[0]=MIXED_STAGE_R3; *depth=1; return 1;
      case 6144: stage[0]=MIXED_STAGE_R3; *depth=1; return 1;
      case 12288: stage[0]=MIXED_STAGE_R3; *depth=1; return 1;
      default: break;
    }
  }
  return 0;
}

/* Fallback 5-smooth decomposition. */
static int build_mixed_sequence(int N, int sve2,
                                       unsigned char *stage, int *depth)
{
  if (mixed_sequence_for_size(N, sve2, stage, depth))
    return 1;

  int e2, e3, e5;
  if (!factor_235(N, &e2, &e3, &e5))
    return 0;

  /* Use DFT12 as the terminal leaf for this decomposition. */
  if (N == 12) {
    *depth = 0;
    return 1;
  }

  int d = 0;

  /* For 4*3^a*5^b sizes, reserve one factor 3 for a DFT12 terminal
   * instead of decomposing all the way to DFT4. */
  if (e2 == 2 && e3 > 0)
    --e3;

  /* Keep repeated radix-3 pairs adjacent. */
  while (e3 >= 2) {
    if (d >= MIXED_MAX_STAGES) return 0;
    stage[d++] = MIXED_STAGE_R9;
    e3 -= 2;
  }

  /* When one radix-3 remains beside at least two radix-5 factors, place the
   * radix-3 stage before the radix-25 stage. */
  if (e3 == 1 && e5 >= 2) {
    if (d >= MIXED_MAX_STAGES) return 0;
    stage[d++] = MIXED_STAGE_R3;
    e3 = 0;
  }

  /* Pair one remaining radix-3 and radix-5 factor. SVE2 uses radix-5 then
   * radix-3; NEON uses radix-3 then radix-5. */
  if (e3 && e5) {
    if (d >= MIXED_MAX_STAGES) return 0;
    stage[d++] = sve2 ? MIXED_STAGE_R15_53
                      : MIXED_STAGE_R15_35;
    --e3;
    --e5;
  }

  if (e3) {
    if (d >= MIXED_MAX_STAGES) return 0;
    stage[d++] = MIXED_STAGE_R3;
    --e3;
  }

  while (e5 >= 2) {
    if (d >= MIXED_MAX_STAGES) return 0;
    stage[d++] = MIXED_STAGE_R25;
    e5 -= 2;
  }

  if (e5) {
    if (d >= MIXED_MAX_STAGES) return 0;
    stage[d++] = MIXED_STAGE_R5;
    --e5;
  }

  *depth = d;
  return 1;
}

static mixed_plan_t *get_mixed_plan(int N, int sve2)
{
  const dft_size_idx_t idx = get_dft(N);
  if (idx >= DFT_SIZE_IDXTABLESIZE)
    return NULL;

  mixed_plan_slot_t *slots = sve2 ? g_sve2_mixed_plans : g_neon_mixed_plans;
  mixed_plan_slot_t *slot = &slots[idx];

  if (__atomic_load_n(&slot->initialized, __ATOMIC_ACQUIRE))
    return &slot->plan;

  pthread_mutex_lock(&g_mixed_plan_mutex);
  if (!slot->initialized) {
    unsigned char stage[MIXED_MAX_STAGES] = {0};
    int depth = 0;
    if (build_mixed_sequence(N, sve2, stage, &depth) &&
        mixed_plan_init(&slot->plan, N, stage, depth))
      __atomic_store_n(&slot->initialized, 1, __ATOMIC_RELEASE);
  }
  mixed_plan_t *plan = slot->initialized ? &slot->plan : NULL;
  pthread_mutex_unlock(&g_mixed_plan_mutex);
  return plan;
}

static c16_t *mixed_work_get(size_t need)
{
  if (need <= g_mixed_work_elems)
    return g_mixed_work;

  c16_t *p = aligned_malloc64(need * sizeof(*p));
  if (!p)
    return NULL;

  free(g_mixed_work);
  g_mixed_work = p;
  g_mixed_work_elems = need;
  return p;
}

static int sve2_vl128_usable(void)
{
  if (!g_sve2_hw_available)
    return 0;

  /* SVE vector length is thread-local on Linux, so check the current thread. */
  if (sve2_vector_bits() != 128)
    return 0;

  ensure_sve2_prepared();
  return 1;
}

SVE2_TARGET static int sve2_forward_direct_leaf(
    const c16_t *src, c16_t *dst, int N)
{
  switch (N) {
    case 4: sve2_dft4_q15_rshift(src,dst); return 1;
    case 8: sve2_dft8_q15_leaf(src,dst); return 1;
    case 12: sve2_dft12_q15_leaf(src,dst); return 1;
    case 16: sve2_dft16_q15_leaf(src,dst); return 1;
    case 32: sve2_dft32_q15_leaf(src,dst); return 1;
    case 64: sve2_dft64_q15_leaf(src,dst); return 1;
    case 128: sve2_dft128_q15_fused_st2(src,dst); return 1;
    case 256: sve2_dft256_q15_r4x64(src,dst); return 1;
    case 512: sve2_dft512_q15(src,dst); return 1;
    case 1024: sve2_dft1024_q15(src,dst); return 1;
    case 2048: sve2_dft2048_q15(src,dst); return 1;
    case 4096: sve2_dft4096_q15(src,dst); return 1;
    case 8192: sve2_dft8192_q15(src,dst); return 1;
    case 16384: sve2_dft16384_q15(src,dst); return 1;
    case 32768: sve2_dft32768_q15(src,dst); return 1;
    case 65536: sve2_dft65536_q15(src,dst); return 1;
    default: return 0;
  }
}

static int forward_direct_leaf(const c16_t *src, c16_t *dst,
                                        int N, int use_sve2)
{
  if (use_sve2)
    return sve2_forward_direct_leaf(src,dst,N);

  if (N == 12) {
    neon_dft12_q15(src,dst);
    return 1;
  }
  if (N >= 4 && is_power_of_two_int(N)) {
    neon_leaf_q15(src,dst,N);
    return 1;
  }
  return 0;
}

static int inverse_direct_leaf(const c16_t *src, c16_t *dst, int N)
{
  if (N == 4 || N == 8 || N == 12 || N == 32) {
    inverse_small_leaf_q15(src, dst, N);
    return 1;
  }
  if (N == 16) {
    idft16_q15_native(src,dst);
    return 1;
  }
  if (N >= 64 && is_power_of_two_int(N)) {
    dft_power2_q15(src, dst, N, DFT_DIR_INVERSE);
    return 1;
  }
  return 0;
}

static void dft_mixed_radix_c16_scaled(const c16_t *src,
                                        c16_t *dst,
                                        int N,
                                        dft_dir_t dir)
{
  ensure_backend_prepared();

  const int use_sve2 = sve2_vl128_usable();
  if (dir == DFT_DIR_FORWARD && forward_direct_leaf(src,dst,N,use_sve2))
    return;
  if (dir == DFT_DIR_INVERSE && inverse_direct_leaf(src,dst,N))
    return;

  mixed_plan_t *p = get_mixed_plan(N, use_sve2);
  AssertFatal(p != NULL, "AArch64 mixed-radix: unsupported N=%d\n", N);

  c16_t *work = NULL;
  if (p->workspace_elems) {
    work = mixed_work_get(p->workspace_elems);
    AssertFatal(work != NULL, "AArch64 mixed-radix: scratch allocation failed N=%d\n", N);
  }

  if (dir == DFT_DIR_FORWARD) {
    if (use_sve2) {
      if (p->depth == 0)
        sve2_235_leaf_q15(p, src, dst);
      else
        sve2_235_exec_q15_rec(p, 0, src, dst, work);
    } else {
      if (p->depth == 0)
        neon_235_leaf_q15(p, src, dst);
      else
        neon_235_exec_q15_rec(p, 0, src, dst, work);
    }
  } else {
    if (p->depth == 0) {
      mixed_inverse_leaf_q15(p, src, dst);
    } else if (use_sve2) {
      sve2_235_exec_q15_rec_inverse(p, 0, src, dst, work);
    } else {
      neon_235_exec_q15_rec_inverse(p, 0, src, dst, work);
    }
  }

}



/* -------------------------------------------------------------------------
 * OAI public symbols.
 * One generic transform receives N and direction; macros expose the ABI
 * symbols used by the DFT/IDFT function tables.
 * ------------------------------------------------------------------------- */
#define DEFINE_MIXED_DFT(N)                                                \
  void dft##N(int16_t *input, int16_t *output, uint8_t scale_flag)         \
  {                                                                        \
    (void)scale_flag;                                                       \
    dft_mixed_radix_c16_scaled((const c16_t *)(const void *)input,         \
                               (c16_t *)(void *)output,                     \
                               (N), DFT_DIR_FORWARD);                       \
  }

#define DEFINE_MIXED_IDFT(N)                                               \
  void idft##N(int16_t *input, int16_t *output, uint8_t scale_flag)        \
  {                                                                        \
    (void)scale_flag;                                                       \
    dft_mixed_radix_c16_scaled((const c16_t *)(const void *)input,         \
                               (c16_t *)(void *)output,                     \
                               (N), DFT_DIR_INVERSE);                       \
  }

/* Export exactly the ABI symbol set used by tools_defs.h function tables. */
FOREACH_DFTSZ(DEFINE_MIXED_DFT)
FOREACH_IDFTSZ(DEFINE_MIXED_IDFT)

#undef DEFINE_MIXED_DFT
#undef DEFINE_MIXED_IDFT

#endif /* __aarch64__ */

#if defined(__arm__) && !defined(__aarch64__)
#error "oai_dfts_neon requires AArch64"
#endif



/* OAI initialization entry point. Common NEON tables are prepared once;
 * SVE2 tables and per-size mixed-radix plans remain lazy. */
int dfts_autoinit(void)
{
  ensure_backend_prepared();
  return 0;
}

/* OAI indexed DFT/IDFT entry points. */
void dft_implementation(uint8_t sizeidx, int16_t *input, int16_t *output, unsigned char scale_flag)
{
  (void)scale_flag;
  AssertFatal(sizeidx < DFT_SIZE_IDXTABLESIZE, "Invalid dft size index %i\n", sizeidx);
  AssertFatal((((intptr_t)output) & 0xF) == 0, "Output buffer should be 16-byte aligned %p", output);
  dft_mixed_radix_c16_scaled((const c16_t *)(const void *)input,
                             (c16_t *)(void *)output,
                             dft_ftab[sizeidx].size,
                             DFT_DIR_FORWARD);
}

void idft_implementation(uint8_t sizeidx, int16_t *input, int16_t *output, unsigned char scale_flag)
{
  (void)scale_flag;
  AssertFatal(sizeidx < DFT_SIZE_IDXTABLESIZE, "Invalid idft size index %i\n", sizeidx);
  AssertFatal((((intptr_t)output) & 0xF) == 0, "Output buffer should be 16-byte aligned %p", output);
  dft_mixed_radix_c16_scaled((const c16_t *)(const void *)input,
                             (c16_t *)(void *)output,
                             idft_ftab[sizeidx].size,
                             DFT_DIR_INVERSE);
}

#endif /* __arm__ || __aarch64__ */