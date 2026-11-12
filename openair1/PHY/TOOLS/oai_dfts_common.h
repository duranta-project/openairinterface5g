/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* Internal part of oai_dfts.c: constants, SIMD complex helpers and twiddle table infrastructure.
 * Included only by oai_dfts.c, in order; not a public header. */

#ifndef OAI_DFTS_INTERNAL
#error "oai_dfts_common.h is included only by oai_dfts.c"
#endif

/* Every twiddle table is built once, by a constructor of this kind, when the library is loaded. The transforms then
 * read the tables directly, with no readiness check or lock in the real-time path. */
#define DFT_TABLE_BUILDER __attribute__((constructor(101), cold))

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define Q15_INV_SQRT2 ((int16_t)23170)
#define Q15_INV_SQRT3 ((int16_t)18919)
#define Q15_INV_SQRT8 ((int16_t)11585)
#define Q15_INV_SQRT6 ((int16_t)13377) /* 1 / sqrt(6) */
#define Q15_HALF_SQRT3 ((int16_t)28378)

#define Q15_INV_2SQRT3 ((int16_t)9459) /* 1 / (2 * sqrt(3)) */

#define Q15_HALF ((int16_t)16384) /* 1 / 2 */
/* 1 / 2 as a twiddle factor. mulhrs rounds ties up, and x * 16384 is a tie for every odd x: +0.25 LSB
 * on average per multiply, which adds up in the DC bin. With 16383 the ties round toward zero. Data
 * halvings use half_q15_256() (see there). */
#define Q15_HALF_TW ((int16_t)16383)

#define Q15_ONE ((int16_t)32767)

#define DFT32_Q15_COS_PI_16 ((int16_t)32138)
#define DFT32_Q15_SIN_PI_16 ((int16_t)6393)

#define DFT32_Q15_COS_3PI_16 ((int16_t)27246)
#define DFT32_Q15_SIN_3PI_16 ((int16_t)18205)

#define Q15_COS_PI_8 ((int16_t)30274) /* cos(pi/8) */
#define Q15_SIN_PI_8 ((int16_t)12540) /* sin(pi/8) */

#define Q15_SQRT3_OVER_2 ((int16_t)28378) /* sqrt(3)/2 */

#define Q15_INV_SQRT5 ((int16_t)14654)
#define Q15_INV_SQRT15 ((int16_t)8460) /* 1 / sqrt(15) */

#define Q15_COS_2PI_5 ((int16_t)10126) /* cos(2pi/5)  */
#define Q15_COS_4PI_5 ((int16_t)-26510) /* cos(4pi/5)  */
#define Q15_SIN_2PI_5 ((int16_t)31163) /* sin(2pi/5)  */
#define Q15_SIN_4PI_5 ((int16_t)19260) /* sin(4pi/5)  */

#define SR_MIN_LOG2 5
#define SR_MAX_LOG2 16 /* largest power-of-two transform: 65536 */
#define SR_TWIDDLE_SLOT_COUNT (SR_MAX_LOG2 - SR_MIN_LOG2 + 1)

static inline simde__m128i load4_complex_strided_c16(const c16_t *src, int stride, int base)
{
  /* One 32-bit element per complex sample: vmovd + 3 vpinsrd instead of
   * 8 word inserts. */
  int32_t a, b, c, d;
  memcpy(&a, &src[(base + 0) * stride], 4);
  memcpy(&b, &src[(base + 1) * stride], 4);
  memcpy(&c, &src[(base + 2) * stride], 4);
  memcpy(&d, &src[(base + 3) * stride], 4);
  return simde_mm_setr_epi32(a, b, c, d);
}

static inline void transpose4_complex_i16_128(simde__m128i *Y0, simde__m128i *Y1, simde__m128i *Y2, simde__m128i *Y3);

static inline simde__m256i join_m128_lanes(simde__m128i lo, simde__m128i hi);

static __attribute__((always_inline)) inline void radix5_pack8_rows_nogather(const c16_t *src, simde__m256i out[5])
{
  simde__m128i a0 = simde_mm_loadu_si128((const simde__m128i *)(src + 0 * 5));
  simde__m128i a1 = simde_mm_loadu_si128((const simde__m128i *)(src + 1 * 5));
  simde__m128i a2 = simde_mm_loadu_si128((const simde__m128i *)(src + 2 * 5));
  simde__m128i a3 = simde_mm_loadu_si128((const simde__m128i *)(src + 3 * 5));
  transpose4_complex_i16_128(&a0, &a1, &a2, &a3);

  simde__m128i b0 = simde_mm_loadu_si128((const simde__m128i *)(src + 4 * 5));
  simde__m128i b1 = simde_mm_loadu_si128((const simde__m128i *)(src + 5 * 5));
  simde__m128i b2 = simde_mm_loadu_si128((const simde__m128i *)(src + 6 * 5));
  simde__m128i b3 = simde_mm_loadu_si128((const simde__m128i *)(src + 7 * 5));
  transpose4_complex_i16_128(&b0, &b1, &b2, &b3);

  out[0] = join_m128_lanes(a0, b0);
  out[1] = join_m128_lanes(a1, b1);
  out[2] = join_m128_lanes(a2, b2);
  out[3] = join_m128_lanes(a3, b3);

  const simde__m128i r4_lo = load4_complex_strided_c16(src + 4, 5, 0);
  const simde__m128i r4_hi = load4_complex_strided_c16(src + 4, 5, 4);
  out[4] = join_m128_lanes(r4_lo, r4_hi);
}

static __attribute__((noinline)) void radix5_pack_children_nogather_540(const c16_t *src, c16_t *packed)
{
  enum { M = 108 };

  int n = 0;
  for (; n + 8 <= M; n += 8) {
    simde__m256i v[5];
    radix5_pack8_rows_nogather(src + 5 * n, v);

    simde_mm256_storeu_si256((simde__m256i *)(packed + 0 * M + n), v[0]);
    simde_mm256_storeu_si256((simde__m256i *)(packed + 1 * M + n), v[1]);
    simde_mm256_storeu_si256((simde__m256i *)(packed + 2 * M + n), v[2]);
    simde_mm256_storeu_si256((simde__m256i *)(packed + 3 * M + n), v[3]);
    simde_mm256_storeu_si256((simde__m256i *)(packed + 4 * M + n), v[4]);
  }

  /* M=108 leaves exactly four rows. */
  for (int br = 0; br < 5; br++)
    simde_mm_store_si128((simde__m128i *)(packed + br * M + n), load4_complex_strided_c16(src + br, 5, n));
}

static __attribute__((noinline)) void radix5_pack_children_nogather_generic(const c16_t *src, c16_t *packed, int M)
{
  AssertFatal((M & 3) == 0, "Invalid no-gather radix-5 packed child size M=%d\n", M);

  int n = 0;
  for (; n + 8 <= M; n += 8) {
    simde__m256i v[5];
    radix5_pack8_rows_nogather(src + 5 * n, v);

    simde_mm256_storeu_si256((simde__m256i *)(packed + 0 * M + n), v[0]);
    simde_mm256_storeu_si256((simde__m256i *)(packed + 1 * M + n), v[1]);
    simde_mm256_storeu_si256((simde__m256i *)(packed + 2 * M + n), v[2]);
    simde_mm256_storeu_si256((simde__m256i *)(packed + 3 * M + n), v[3]);
    simde_mm256_storeu_si256((simde__m256i *)(packed + 4 * M + n), v[4]);
  }

  if (n < M) {
    for (int br = 0; br < 5; br++)
      simde_mm_store_si128((simde__m128i *)(packed + br * M + n), load4_complex_strided_c16(src + br, 5, n));
  }
}

/* Stores the first `valid` (0..8) complex lanes of v at dst with plain 16/8/4-byte stores. A masked store
 * (vpmaskmovd to memory) is microcoded on AMD Zen 2..4 and costs tens of cycles; these stores are not. */
static __attribute__((always_inline)) inline void store_complex_lanes_256(c16_t *dst, simde__m256i v, int valid)
{
  if (valid >= 8) {
    simde_mm256_storeu_si256((simde__m256i *)dst, v);
    return;
  }
  simde__m128i part = simde_mm256_castsi256_si128(v);
  if (valid >= 4) {
    simde_mm_storeu_si128((simde__m128i *)dst, part);
    part = simde_mm256_extracti128_si256(v, 1);
    dst += 4;
    valid -= 4;
  }
  if (valid >= 2) {
    simde_mm_storel_epi64((simde__m128i *)dst, part);
    part = simde_mm_srli_si128(part, 8);
    dst += 2;
    valid -= 2;
  }
  if (valid) {
    const int32_t w = simde_mm_cvtsi128_si32(part);
    memcpy(dst, &w, sizeof(w));
  }
}

static inline int16_t sat_i16(long v)
{
  if (v > INT16_MAX)
    return INT16_MAX;

  if (v < INT16_MIN)
    return INT16_MIN;

  return (int16_t)v;
}

static inline int log2_int(unsigned int N)
{
  return __builtin_ctz(N);
}
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

/* AVX2 byte shuffles apply the same 16-byte permutation in both lanes. */
#define DFT_DUPLICATED_BYTE_LANES(SET, ...) SET(__VA_ARGS__, __VA_ARGS__)

static inline simde__m256i c16_mul_q15_simd256(simde__m256i x, simde__m256i w_re_negim, simde__m256i w_im_re)
{
  simde__m256i round = simde_mm256_set1_epi32(1 << 14);

  simde__m256i re32 = simde_mm256_madd_epi16(x, w_re_negim);
  simde__m256i im32 = simde_mm256_madd_epi16(x, w_im_re);

  re32 = simde_mm256_srai_epi32(simde_mm256_add_epi32(re32, round), 15);
  im32 = simde_mm256_srai_epi32(simde_mm256_add_epi32(im32, round), 15);

  simde__m256i packed = simde_mm256_packs_epi32(re32, im32);

  const simde__m256i mask = DFT_DUPLICATED_BYTE_LANES(simde_mm256_set_epi8, 15, 14, 7, 6, 13, 12, 5, 4, 11, 10, 3, 2, 9, 8, 1, 0);

  return simde_mm256_shuffle_epi8(packed, mask);
}

/* Q15 constant for mulhrs without a tie bias (see Q15_HALF_TW): multiples of 4096 move one step toward zero. */
static inline int16_t q15_untie(int16_t v)
{
  return (v & 0xfff) || !v ? v : v > 0 ? v - 1 : v + 1;
}

static inline int16_t q15_from_float(float x)
{
  return q15_untie(sat_i16((int32_t)lrintf(32767.0f * x)));
}

static inline simde__m128i swap_complex_pairs_i16_128(simde__m128i a)
{
  const simde__m128i shuf = simde_mm_set_epi8(13, 12, 15, 14, 9, 8, 11, 10, 5, 4, 7, 6, 1, 0, 3, 2);

  return simde_mm_shuffle_epi8(a, shuf);
}

#define DEFINE_PREPACKED_COMPLEX_MULTIPLY(FN, VEC, SWAP, MUL, ADD) \
  static inline VEC FN(VEC a, VEC w_re_re, VEC w_im_signed)        \
  {                                                                \
    const VEC a_swapped = SWAP(a);                                 \
                                                                   \
    const VEC prod_re = MUL(a, w_re_re);                           \
    const VEC prod_im = MUL(a_swapped, w_im_signed);               \
                                                                   \
    return ADD(prod_re, prod_im);                                  \
  }

DEFINE_PREPACKED_COMPLEX_MULTIPLY(complex_mul4_prepack_q15_128,
                                  simde__m128i,
                                  swap_complex_pairs_i16_128,
                                  simde_mm_mulhrs_epi16,
                                  simde_mm_adds_epi16)

DEFINE_PREPACKED_COMPLEX_MULTIPLY(complex_mul4_prepack_small_q15_128,
                                  simde__m128i,
                                  swap_complex_pairs_i16_128,
                                  simde_mm_mulhrs_epi16,
                                  simde_mm_add_epi16)

static inline simde__m128i complex_mul4_bcast_q15_128(simde__m128i a, int16_t wr, int16_t wi)
{
  const simde__m128i w_re_re = simde_mm_set1_epi16(wr);

  const simde__m128i w_im_signed = simde_mm_set1_epi32((int32_t)((uint16_t)-wi | ((uint32_t)(uint16_t)wi << 16)));

  return complex_mul4_prepack_q15_128(a, w_re_re, w_im_signed);
}

/* +/-j interchanges real/imaginary components and applies the exact signed mask. */
#define DEFINE_COMPLEX_J_ROTATION(FN, VEC, SWAP, MASK, SIGN) \
  static inline VEC FN(VEC z)                                \
  {                                                          \
    const VEC swapped = SWAP(z);                             \
    const VEC sign = MASK;                                   \
    return SIGN(swapped, sign);                              \
  }

DEFINE_COMPLEX_J_ROTATION(mul_q15_128,
                          simde__m128i,
                          swap_complex_pairs_i16_128,
                          simde_mm_setr_epi16(-1, 1, -1, 1, -1, 1, -1, 1),
                          simde_mm_sign_epi16)

DEFINE_COMPLEX_J_ROTATION(mul_minus_q15_128,
                          simde__m128i,
                          swap_complex_pairs_i16_128,
                          simde_mm_setr_epi16(1, -1, 1, -1, 1, -1, 1, -1),
                          simde_mm_sign_epi16)

static inline simde__m128i half_q15_128(simde__m128i x);
static inline simde__m128i q15_mul_i16_128(simde__m128i x, int16_t q15)
{
  if (q15 == Q15_HALF)
    return half_q15_128(x);
  return simde_mm_mulhrs_epi16(x, simde_mm_set1_epi16(q15));
}

typedef enum { DFT_DIR_FORWARD = -1, DFT_DIR_INVERSE = 1 } dft_dir_t;

static inline simde__m128i mul_minus_j_dir_i16_128(simde__m128i z, dft_dir_t dir)
{
  return (dir == DFT_DIR_FORWARD) ? mul_minus_q15_128(z) : mul_q15_128(z);
}

static inline simde__m128i mul_plus_j_dir_i16_128(simde__m128i z, dft_dir_t dir)
{
  return (dir == DFT_DIR_FORWARD) ? mul_q15_128(z) : mul_minus_q15_128(z);
}

static inline simde__m128i twiddle_im_dir_128(simde__m128i w_im_signed, dft_dir_t dir)
{
  return (dir == DFT_DIR_FORWARD) ? w_im_signed : simde_mm_sub_epi16(simde_mm_setzero_si128(), w_im_signed);
}

static inline int16_t twiddle_im_scalar_dir_i16(int16_t wi_forward, dft_dir_t dir)
{
  return (dir == DFT_DIR_FORWARD) ? wi_forward : sat_i16(-(long)wi_forward);
}

/* 128-bit form of add_pm_j_i16_256 below. */
static __attribute__((always_inline)) inline void add_pm_j_i16_128(simde__m128i base,
                                                                   simde__m128i z,
                                                                   simde__m128i *y_mj,
                                                                   simde__m128i *y_pj,
                                                                   dft_dir_t dir)
{
  const simde__m128i sw = swap_complex_pairs_i16_128(z);
  const simde__m128i a = simde_mm_adds_epi16(base, sw);
  const simde__m128i d = simde_mm_subs_epi16(base, sw);
  if (dir == DFT_DIR_FORWARD) {
    *y_mj = simde_mm_blend_epi16(a, d, 0xAA);
    *y_pj = simde_mm_blend_epi16(d, a, 0xAA);
  } else {
    *y_mj = simde_mm_blend_epi16(d, a, 0xAA);
    *y_pj = simde_mm_blend_epi16(a, d, 0xAA);
  }
}

static inline simde__m256i swap_complex_pairs_i16_256(simde__m256i a)
{
  const simde__m256i shuf = DFT_DUPLICATED_BYTE_LANES(simde_mm256_setr_epi8, 2, 3, 0, 1, 6, 7, 4, 5, 10, 11, 8, 9, 14, 15, 12, 13);

  return simde_mm256_shuffle_epi8(a, shuf);
}

DEFINE_COMPLEX_J_ROTATION(mul_j_i16_256,
                          simde__m256i,
                          swap_complex_pairs_i16_256,
                          simde_mm256_setr_epi16(-1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1),
                          simde_mm256_sign_epi16)

DEFINE_COMPLEX_J_ROTATION(mul_minus_j_i16_256,
                          simde__m256i,
                          swap_complex_pairs_i16_256,
                          simde_mm256_setr_epi16(+1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1),
                          simde_mm256_sign_epi16)

static inline simde__m256i mul_minus_j_dir_i16_256(simde__m256i z, dft_dir_t dir)
{
  return (dir == DFT_DIR_FORWARD) ? mul_minus_j_i16_256(z) : mul_j_i16_256(z);
}

static inline simde__m256i mul_plus_j_dir_i16_256(simde__m256i z, dft_dir_t dir)
{
  return (dir == DFT_DIR_FORWARD) ? mul_j_i16_256(z) : mul_minus_j_i16_256(z);
}

/* base -/+ j*z for both signs at once: (*y_mj, *y_pj) = (base + J z, base - J z) with J = -j (forward) or +j
 * (inverse). One swap, one saturating add and one saturating subtract; lightweight blends pick the real and
 * imaginary halves. Same result as adding mul_minus_j/mul_plus_j except for a z component equal to -32768,
 * where this form saturates correctly instead of wrapping. */
static __attribute__((always_inline)) inline void add_pm_j_i16_256(simde__m256i base,
                                                                   simde__m256i z,
                                                                   simde__m256i *y_mj,
                                                                   simde__m256i *y_pj,
                                                                   dft_dir_t dir)
{
  const simde__m256i sw = swap_complex_pairs_i16_256(z);
  const simde__m256i a = simde_mm256_adds_epi16(base, sw); /* (br + zi, bi + zr) */
  const simde__m256i d = simde_mm256_subs_epi16(base, sw); /* (br - zi, bi - zr) */
  /* forward: base - j z = (br + zi, bi - zr); inverse: base + j z = (br - zi, bi + zr) */
  if (dir == DFT_DIR_FORWARD) {
    *y_mj = simde_mm256_blend_epi16(a, d, 0xAA);
    *y_pj = simde_mm256_blend_epi16(d, a, 0xAA);
  } else {
    *y_mj = simde_mm256_blend_epi16(d, a, 0xAA);
    *y_pj = simde_mm256_blend_epi16(a, d, 0xAA);
  }
}

/* (-j)^dir * mulhrs(x, c) as one multiply of the swapped input by (+c, -c)
 * (forward) or (-c, +c) (inverse): no separate negation. Equals the rotated
 * product except at exact rounding ties (|x * c| = 2^14 mod 2^15). */
static inline simde__m256i mul_minus_j_scaled_dir_i16_256(simde__m256i x, int16_t c, dft_dir_t dir)
{
  /* one dword broadcast: (e, -e) with e = +/-c by direction */
  const uint32_t f = (uint16_t)c | ((uint32_t)(uint16_t)-c << 16), i = (f >> 16) | (f << 16);
  const simde__m256i k = simde_mm256_set1_epi32((int32_t)(dir == DFT_DIR_FORWARD ? f : i));
  return simde_mm256_mulhrs_epi16(swap_complex_pairs_i16_256(x), k);
}

static inline simde__m128i mul_minus_j_scaled_dir_i16_128(simde__m128i x, int16_t c, dft_dir_t dir)
{
  const uint32_t f = (uint16_t)c | ((uint32_t)(uint16_t)-c << 16), i = (f >> 16) | (f << 16);
  return simde_mm_mulhrs_epi16(swap_complex_pairs_i16_128(x), simde_mm_set1_epi32((int32_t)(dir == DFT_DIR_FORWARD ? f : i)));
}

static inline void transpose8_complex_i16_256(simde__m256i *r0,
                                              simde__m256i *r1,
                                              simde__m256i *r2,
                                              simde__m256i *r3,
                                              simde__m256i *r4,
                                              simde__m256i *r5,
                                              simde__m256i *r6,
                                              simde__m256i *r7)
{
  const simde__m256i a = *r0;
  const simde__m256i b = *r1;
  const simde__m256i c = *r2;
  const simde__m256i d = *r3;
  const simde__m256i e = *r4;
  const simde__m256i f = *r5;
  const simde__m256i g = *r6;
  const simde__m256i h = *r7;

  const simde__m256i t0 = simde_mm256_unpacklo_epi32(a, b);
  const simde__m256i t1 = simde_mm256_unpackhi_epi32(a, b);
  const simde__m256i t2 = simde_mm256_unpacklo_epi32(c, d);
  const simde__m256i t3 = simde_mm256_unpackhi_epi32(c, d);
  const simde__m256i t4 = simde_mm256_unpacklo_epi32(e, f);
  const simde__m256i t5 = simde_mm256_unpackhi_epi32(e, f);
  const simde__m256i t6 = simde_mm256_unpacklo_epi32(g, h);
  const simde__m256i t7 = simde_mm256_unpackhi_epi32(g, h);

  const simde__m256i s0 = simde_mm256_unpacklo_epi64(t0, t2);
  const simde__m256i s1 = simde_mm256_unpackhi_epi64(t0, t2);
  const simde__m256i s2 = simde_mm256_unpacklo_epi64(t1, t3);
  const simde__m256i s3 = simde_mm256_unpackhi_epi64(t1, t3);

  const simde__m256i s4 = simde_mm256_unpacklo_epi64(t4, t6);
  const simde__m256i s5 = simde_mm256_unpackhi_epi64(t4, t6);
  const simde__m256i s6 = simde_mm256_unpacklo_epi64(t5, t7);
  const simde__m256i s7 = simde_mm256_unpackhi_epi64(t5, t7);

  *r0 = simde_mm256_permute2x128_si256(s0, s4, 0x20);
  *r1 = simde_mm256_permute2x128_si256(s1, s5, 0x20);
  *r2 = simde_mm256_permute2x128_si256(s2, s6, 0x20);
  *r3 = simde_mm256_permute2x128_si256(s3, s7, 0x20);

  *r4 = simde_mm256_permute2x128_si256(s0, s4, 0x31);
  *r5 = simde_mm256_permute2x128_si256(s1, s5, 0x31);
  *r6 = simde_mm256_permute2x128_si256(s2, s6, 0x31);
  *r7 = simde_mm256_permute2x128_si256(s3, s7, 0x31);
}

DEFINE_PREPACKED_COMPLEX_MULTIPLY(complex_mul8_prepack_q15_256,
                                  simde__m256i,
                                  swap_complex_pairs_i16_256,
                                  simde_mm256_mulhrs_epi16,
                                  simde_mm256_adds_epi16)

/* Twiddles with |re| + |im| <= 0.82 (magnitude <= 1/sqrt(3)): the two rounded products sum to at most
 * 26756 in magnitude, so the wrapping add gives the same result as the saturating one and is cheaper. */
DEFINE_PREPACKED_COMPLEX_MULTIPLY(complex_mul8_prepack_small_q15_256,
                                  simde__m256i,
                                  swap_complex_pairs_i16_256,
                                  simde_mm256_mulhrs_epi16,
                                  simde_mm256_add_epi16)

static inline void dft4_void(const c16_t *src, c16_t *dst, dft_dir_t dir);
static inline void dft8_avx(const c16_t *src, c16_t *dst, dft_dir_t dir);
static inline void dft16_q15_128(const c16_t *src, c16_t *dst, dft_dir_t dir);
static inline void dft32_q15_128(const c16_t *src, c16_t *dst, dft_dir_t dir);
static inline void dft32_q15_256_contiguous(const c16_t *src, c16_t *dst, dft_dir_t dir);
static void dft96_radix12_pfa_leaf8(const c16_t *, c16_t *, dft_dir_t);
static void dft120_radix15_pfa(const c16_t *src, c16_t *dst, dft_dir_t dir);
static void dft144_radix18_leaf8(const c16_t *src, c16_t *dst, dft_dir_t dir);
static void dft192_radix12_pfa_leaf16(const c16_t *, c16_t *, dft_dir_t);
static void dft216_radix27_leaf8(const c16_t *src, c16_t *dst, dft_dir_t dir);
static void dft240_radix15_pfa(const c16_t *src, c16_t *dst, dft_dir_t dir);
static __attribute__((always_inline)) inline void dft6_pfa8_q15_256(simde__m256i x0,
                                                                    simde__m256i x1,
                                                                    simde__m256i x2,
                                                                    simde__m256i x3,
                                                                    simde__m256i x4,
                                                                    simde__m256i x5,
                                                                    simde__m256i y[6],
                                                                    dft_dir_t dir);
static void dft384_radix24_pfa(const c16_t *src, c16_t *dst, dft_dir_t dir);
static void dft432_radix27_leaf16(const c16_t *src, c16_t *dst, dft_dir_t dir);
static void dft480_radix30_pfa(const c16_t *src, c16_t *dst, dft_dir_t dir);
static void dft864_radix27_leaf32(const c16_t *src, c16_t *dst, dft_dir_t dir);
/* Keep the large R18 child outlined so the shared R9 dispatcher keeps its small stack frame. */
static __attribute__((noinline)) void dft1152_radix18_leaf64(const c16_t *src, c16_t *dst, dft_dir_t dir);
static void dft1536_radix24_pfa_leaf64(const c16_t *, c16_t *, dft_dir_t);
static void dft1728_radix27_leaf64(const c16_t *src, c16_t *dst, dft_dir_t dir);
static void dft1920_radix30_pfa(const c16_t *src, c16_t *dst, dft_dir_t dir);
static void dft2160_radix30_leaf72x8(const c16_t *src, c16_t *dst, dft_dir_t dir);
static inline void dft16_q15_128_strided(const c16_t *src, int stride, c16_t *dst, dft_dir_t dir);
static inline void dft32_q15_128_strided(const c16_t *src, int stride, c16_t *dst, dft_dir_t dir);
static void dft256_radix16(const c16_t *src, c16_t *dst, dft_dir_t dir);
static void dft512_radix8(const c16_t *src, c16_t *dst, dft_dir_t dir);
static void dft1024_radix16(const c16_t *src, c16_t *dst, dft_dir_t dir);
static size_t dft_power2_mixed_large_work_len(int N);
static void dft_power2_mixed_large_core(const c16_t *src, c16_t *dst, int N, dft_dir_t dir, c16_t *work);
static void dft_radix3_pow2(const c16_t *src, c16_t *dst, int N, dft_dir_t dir);

typedef struct {
  int r3_q15_blocks;
  simde__m128i *r3_q15_storage;
  /* Same layout with unit magnitude (no 1/sqrt(3)); only for N <= R3_UNIT_MAX_N,
   * where the table is the second-stage table of a radix-9 parent. */
  simde__m128i *r3_unit_storage;
} r3_twiddle_t;

#define R3_UNIT_MAX_N 1024

typedef struct {
  int r5_q15_blocks;
  simde__m128i *r5_q15_storage;
} r5_twiddle_t;

#define R3_Q15_PTR(TW, SLOT) ((TW)->r3_q15_storage + (size_t)(SLOT) * (size_t)((TW)->r3_q15_blocks))
/* Real tables are shared by both directions; imaginary tables are per direction.
 * Layout: [RE1 RE2][IM1 IM2 forward][IM1 IM2 inverse], one block per 4 samples. */
#define R3_W1_RE(TW) R3_Q15_PTR((TW), 0)
#define R3_W2_RE(TW) R3_Q15_PTR((TW), 1)
#define R3_W1_IM(TW) R3_Q15_PTR((TW), 2)
#define R3_W2_IM(TW) R3_Q15_PTR((TW), 3)
#define R3_W1_IM_INV(TW) R3_Q15_PTR((TW), 4)
#define R3_W2_IM_INV(TW) R3_Q15_PTR((TW), 5)

#define R3_UNIT_PTR(TW, SLOT) ((TW)->r3_unit_storage + (size_t)(SLOT) * (size_t)((TW)->r3_q15_blocks))

#define R5_Q15_PTR(TW, SLOT) ((TW)->r5_q15_storage + (size_t)(SLOT) * (size_t)((TW)->r5_q15_blocks))
/* Same layout rule for radix-5: [RE1..RE4][IM1f..IM4f][IM1i..IM4i]. */
#define R5_W1_RE(TW) R5_Q15_PTR((TW), 0)
#define R5_W2_RE(TW) R5_Q15_PTR((TW), 1)
#define R5_W3_RE(TW) R5_Q15_PTR((TW), 2)
#define R5_W4_RE(TW) R5_Q15_PTR((TW), 3)
#define R5_W1_IM(TW) R5_Q15_PTR((TW), 4)
#define R5_W2_IM(TW) R5_Q15_PTR((TW), 5)
#define R5_W3_IM(TW) R5_Q15_PTR((TW), 6)
#define R5_W4_IM(TW) R5_Q15_PTR((TW), 7)
#define R5_W1_IM_INV(TW) R5_Q15_PTR((TW), 8)
#define R5_W2_IM_INV(TW) R5_Q15_PTR((TW), 9)
#define R5_W3_IM_INV(TW) R5_Q15_PTR((TW), 10)
#define R5_W4_IM_INV(TW) R5_Q15_PTR((TW), 11)

typedef struct {
  int q15_blocks;
  simde__m256i *q15_re;
  simde__m256i *q15_im;
  simde__m256i *q15_im_inv;
} power2_twiddle_t;

static simde__m256i g_dft64_re[8] __attribute__((aligned(64)));
static simde__m256i g_dft64_im[2][8] __attribute__((aligned(64)));
static simde__m256i g_dft128_re[8] __attribute__((aligned(64)));
static simde__m256i g_dft128_im[2][8] __attribute__((aligned(64)));

/* Static metadata exists only for coefficient sets used by the x86 kernels. */
// clang-format off
#define R3_TWIDDLE_SIZES(X) \
  X(12) \
  X(24) \
  X(36) \
  X(48) \
  X(72) \
  X(96) \
  X(108) \
  X(144) \
  X(192) \
  X(216) \
  X(288) \
  X(324) \
  X(432) \
  X(576) \
  X(648) \
  X(768) \
  X(864) \
  X(972) \
  X(1200) \
  X(1296) \
  X(1440) \
  X(1944) \
  X(2304) \
  X(2400) \
  X(2592) \
  X(2916) \
  X(12288) \
  X(18432) \
  X(24576) \
  X(36864) \
  X(49152) \
  X(98304)
// clang-format on

#define R3_TWIDDLE_SLOT(N_) static r3_twiddle_t g_r3_twiddle_##N_ __attribute__((section(".bss.oai_dft_twmeta")));
R3_TWIDDLE_SIZES(R3_TWIDDLE_SLOT)
#undef R3_TWIDDLE_SLOT

#define R5_TWIDDLE_SIZES(X) \
  X(60)                     \
  X(80)                     \
  X(160) X(180) X(300) X(360) X(400) X(540) X(600) X(800) X(900) X(1080) X(1500) X(1620) X(1800) X(2700) X(2880) X(3000) X(3240)

#define R5_TWIDDLE_SLOT(N_) static r5_twiddle_t g_r5_twiddle_##N_ __attribute__((section(".bss.oai_dft_twmeta")));
R5_TWIDDLE_SIZES(R5_TWIDDLE_SLOT)
#undef R5_TWIDDLE_SLOT

#define POWER2_TWIDDLE_SIZES(X) X(2048) X(4096) X(8192) X(32768) X(65536)

#define POWER2_TWIDDLE_SLOT(N_) static power2_twiddle_t g_power2_twiddle_##N_ __attribute__((section(".bss.oai_dft_twmeta")));
POWER2_TWIDDLE_SIZES(POWER2_TWIDDLE_SLOT)
#undef POWER2_TWIDDLE_SLOT

static inline r3_twiddle_t *r3_twiddle_slot(int N)
{
  switch (N) {
#define R3_TWIDDLE_CASE(N_) \
  case N_:                  \
    return &g_r3_twiddle_##N_;
    R3_TWIDDLE_SIZES(R3_TWIDDLE_CASE)
#undef R3_TWIDDLE_CASE
    default:
      return NULL;
  }
}

static inline r5_twiddle_t *r5_twiddle_slot(int N)
{
  switch (N) {
#define R5_TWIDDLE_CASE(N_) \
  case N_:                  \
    return &g_r5_twiddle_##N_;
    R5_TWIDDLE_SIZES(R5_TWIDDLE_CASE)
#undef R5_TWIDDLE_CASE
    default:
      return NULL;
  }
}

static inline power2_twiddle_t *power2_twiddle_slot(int N)
{
  switch (N) {
#define POWER2_TWIDDLE_CASE(N_) \
  case N_:                      \
    return &g_power2_twiddle_##N_;
    POWER2_TWIDDLE_SIZES(POWER2_TWIDDLE_CASE)
#undef POWER2_TWIDDLE_CASE
    default:
      return NULL;
  }
}

/* Twiddle packers for 4-lane (SSE) and 8-lane (AVX2) vectors: W_N^(mul*k) * scale, re/re and -im/im pairs. */
#define DEFINE_SCALED_TWIDDLE_PACKERS(RE_FN, IM_FN, VEC, LENGTH, LANES, ATTR, FIRST_INDEX, LOAD) \
  static inline VEC RE_FN(int k0, int mul, int N, float scale)                                   \
  {                                                                                              \
    int16_t v[LENGTH] ATTR;                                                                      \
    for (int j = 0; j < LANES; j++) {                                                            \
      const int k = (mul * (k0 + j)) % N;                                                        \
      const float theta = 2.0f * (float)M_PI * (float)k / (float)N;                              \
      const int16_t wr = q15_from_float(cosf(theta) * scale);                                    \
      v[FIRST_INDEX] = wr;                                                                       \
      v[2 * j + 1] = wr;                                                                         \
    }                                                                                            \
    return LOAD((const VEC *)v);                                                                 \
  }                                                                                              \
                                                                                                 \
  static inline VEC IM_FN(int k0, int mul, int N, float scale, dft_dir_t dir)                    \
  {                                                                                              \
    int16_t v[LENGTH] ATTR;                                                                      \
    for (int j = 0; j < LANES; j++) {                                                            \
      const int k = (mul * (k0 + j)) % N;                                                        \
      const float theta = 2.0f * (float)M_PI * (float)k / (float)N;                              \
      const int16_t wi = q15_from_float((float)dir * sinf(theta) * scale);                       \
      v[FIRST_INDEX] = -wi;                                                                      \
      v[2 * j + 1] = wi;                                                                         \
    }                                                                                            \
    return LOAD((const VEC *)v);                                                                 \
  }

DEFINE_SCALED_TWIDDLE_PACKERS(pack4_twiddle_q15_re_re_scaled,
                              pack4_twiddle_q15_im_signed_scaled,
                              simde__m128i,
                              8,
                              4,
                              ,
                              2 * j + 0,
                              simde_mm_loadu_si128)

DEFINE_SCALED_TWIDDLE_PACKERS(pack8_twiddle_q15_re_re_scaled,
                              pack8_twiddle_q15_im_signed_scaled,
                              simde__m256i,
                              16,
                              8,
                              __attribute__((aligned(32))),
                              2 * j,
                              simde_mm256_load_si256)

static inline simde__m256i pack8_twiddle_q15_re_re256i(int k0, int mul, int N)
{
  int16_t v[16] __attribute__((aligned(32)));
  for (int j = 0; j < 8; j++) {
    const int k = (mul * (k0 + j)) % N;
    const float theta = 2.0f * (float)M_PI * (float)k / (float)N;
    const int16_t raw = q15_from_float(cosf(theta));
    const int16_t wr = (N == 64) ? q15_from_float(cosf(theta) / 4.0f) : raw / sqrtf(2.0f);
    v[2 * j + 0] = wr;
    v[2 * j + 1] = wr;
  }
  return simde_mm256_load_si256((const simde__m256i *)v);
}

static inline simde__m256i pack8_twiddle_q15_im_signed256i(int k0, int mul, int N, dft_dir_t dir)
{
  int16_t v[16] __attribute__((aligned(32)));
  for (int j = 0; j < 8; j++) {
    const int k = (mul * (k0 + j)) % N;
    const float theta = 2.0f * (float)M_PI * (float)k / (float)N;
    const int16_t raw = q15_from_float((float)dir * sinf(theta));
    const int16_t wi = (N == 64) ? q15_from_float((float)dir * sinf(theta) / 4.0f) : raw / sqrtf(2.0f);
    v[2 * j + 0] = -wi;
    v[2 * j + 1] = wi;
  }
  return simde_mm256_load_si256((const simde__m256i *)v);
}

static void fixed_64_128_twiddles_init(int N)
{
  for (int m = 0; m < 8; m++) {
    if (N == 64) {
      g_dft64_re[m] = pack8_twiddle_q15_re_re256i(0, m, N);
      g_dft64_im[0][m] = pack8_twiddle_q15_im_signed256i(0, m, N, DFT_DIR_FORWARD);
      g_dft64_im[1][m] = pack8_twiddle_q15_im_signed256i(0, m, N, DFT_DIR_INVERSE);
    } else {
      g_dft128_re[m] = pack8_twiddle_q15_re_re256i(8 * m, 1, N);
      g_dft128_im[0][m] = pack8_twiddle_q15_im_signed256i(8 * m, 1, N, DFT_DIR_FORWARD);
      g_dft128_im[1][m] = pack8_twiddle_q15_im_signed256i(8 * m, 1, N, DFT_DIR_INVERSE);
    }
  }
}
/* Component tables share allocation geometry and publication, with no cached views. */
#define DFT_COMPONENT_TWO(APPLY, RADIX) APPLY(RADIX, 1) APPLY(RADIX, 2)
#define DFT_COMPONENT_FOUR(APPLY, RADIX) DFT_COMPONENT_TWO(APPLY, RADIX) APPLY(RADIX, 3) APPLY(RADIX, 4)
#define DFT_COMPONENT_FORWARD(RADIX, K)                                                    \
  R##RADIX##_W##K##_RE(table)[b] = pack4_twiddle_q15_re_re_scaled(k0, K, N, scale##RADIX); \
  R##RADIX##_W##K##_IM(table)[b] = pack4_twiddle_q15_im_signed_scaled(k0, K, N, scale##RADIX, DFT_DIR_FORWARD);
#define DFT_COMPONENT_INVERSE(RADIX, K) \
  R##RADIX##_W##K##_IM_INV(table)[b] = pack4_twiddle_q15_im_signed_scaled(k0, K, N, scale##RADIX, DFT_DIR_INVERSE);
#define DEFINE_COMPONENT_TWIDDLE_CREATE(RADIX, COUNT, RADIX_FLOAT, EACH)                                     \
  static int r##RADIX##_twiddle_create_q15_simd(r##RADIX##_twiddle_t *table, int N)                          \
  {                                                                                                          \
    const int size = N / RADIX;                                                                              \
    const int blocks = (size + 3) / 4;                                                                       \
    table->r##RADIX##_q15_blocks = blocks;                                                                   \
    table->r##RADIX##_q15_storage = aligned_malloc64((size_t)COUNT * (size_t)blocks * sizeof(simde__m128i)); \
    if (!table->r##RADIX##_q15_storage)                                                                      \
      return 0;                                                                                              \
    const float scale##RADIX = 1.0f / sqrtf(RADIX_FLOAT);                                                    \
    for (int b = 0; b < blocks; b++) {                                                                       \
      const int k0 = 4 * b;                                                                                  \
      EACH(DFT_COMPONENT_FORWARD, RADIX)                                                                     \
      EACH(DFT_COMPONENT_INVERSE, RADIX)                                                                     \
    }                                                                                                        \
    return 1;                                                                                                \
  }

DEFINE_COMPONENT_TWIDDLE_CREATE(3, 6, 3.0f, DFT_COMPONENT_TWO)

static int r3_unit_twiddle_create(r3_twiddle_t *table, int N)
{
  const int blocks = table->r3_q15_blocks;
  table->r3_unit_storage = aligned_malloc64((size_t)6 * (size_t)blocks * sizeof(simde__m128i));
  if (!table->r3_unit_storage)
    return 0;
  for (int b = 0; b < blocks; b++) {
    const int k0 = 4 * b;
    for (int K = 1; K <= 2; K++) {
      R3_UNIT_PTR(table, K - 1)[b] = pack4_twiddle_q15_re_re_scaled(k0, K, N, 1.0f);
      R3_UNIT_PTR(table, K + 1)[b] = pack4_twiddle_q15_im_signed_scaled(k0, K, N, 1.0f, DFT_DIR_FORWARD);
      R3_UNIT_PTR(table, K + 3)[b] = pack4_twiddle_q15_im_signed_scaled(k0, K, N, 1.0f, DFT_DIR_INVERSE);
    }
  }
  return 1;
}

/* Tables small enough to be a radix-9 second-stage table also get the unit-magnitude copy. */
static int r3_twiddle_create_extra(r3_twiddle_t *table, int N)
{
  return N > R3_UNIT_MAX_N || r3_unit_twiddle_create(table, N);
}

DEFINE_COMPONENT_TWIDDLE_CREATE(5, 12, 5.0f, DFT_COMPONENT_FOUR)

static int r5_twiddle_create_extra(r5_twiddle_t *table, int N)
{
  (void)table;
  (void)N;
  return 1;
}

#define R3_TWIDDLE_BUILD(N_)                                                                                         \
  AssertFatal(r3_twiddle_create_q15_simd(&g_r3_twiddle_##N_, N_) && r3_twiddle_create_extra(&g_r3_twiddle_##N_, N_), \
              "radix-3 twiddles for N=%d: allocation failed\n",                                                      \
              N_);
#define R5_TWIDDLE_BUILD(N_)                                                                                         \
  AssertFatal(r5_twiddle_create_q15_simd(&g_r5_twiddle_##N_, N_) && r5_twiddle_create_extra(&g_r5_twiddle_##N_, N_), \
              "radix-5 twiddles for N=%d: allocation failed\n",                                                      \
              N_);

// clang-format off
static DFT_TABLE_BUILDER void component_twiddles_build(void)
{
  R3_TWIDDLE_SIZES(R3_TWIDDLE_BUILD)
  R5_TWIDDLE_SIZES(R5_TWIDDLE_BUILD)
}
// clang-format on
#undef R3_TWIDDLE_BUILD
#undef R5_TWIDDLE_BUILD
#undef R3_TWIDDLE_SIZES
#undef R5_TWIDDLE_SIZES

/* The table of size N, built at load; N is a constant at the call sites, so this is a direct address. */
#define DEFINE_COMPONENT_TWIDDLE_GET(RADIX)                                                              \
  static __attribute__((always_inline)) inline const r##RADIX##_twiddle_t *r##RADIX##_twiddle_get(int N) \
  {                                                                                                      \
    const r##RADIX##_twiddle_t *table = r##RADIX##_twiddle_slot(N);                                      \
    AssertFatal(table, "No radix-" #RADIX " twiddles for N=%d\n", N);                                    \
    return table;                                                                                        \
  }

DEFINE_COMPONENT_TWIDDLE_GET(3) DEFINE_COMPONENT_TWIDDLE_GET(5)

    static int power2_twiddle_build(power2_twiddle_t *table, int N, int radix)
{
  const int size = N / radix;
  if ((radix != 8 && radix != 16) || size < 8 || (size & 7))
    return 0;

  const int blocks = size / 8;
  const size_t count = (size_t)radix * (size_t)blocks;
  /* The real parts are the same for both directions and are stored once. */
  table->q15_re = aligned_malloc64(count * sizeof(simde__m256i));
  table->q15_im = aligned_malloc64(count * sizeof(simde__m256i));
  table->q15_im_inv = aligned_malloc64(count * sizeof(simde__m256i));
  if (!table->q15_re || !table->q15_im || !table->q15_im_inv)
    return 0;

  /* The radix-8 stage runs a half-scaled DFT8; the twiddles carry 1/sqrt(2). */
  const float scale = 1.0f / sqrtf(2.0f);
  for (int r = 0; r < radix; r++) {
    for (int b = 0; b < blocks; b++) {
      /* block-major: the eight row twiddles of a block are contiguous (one stream per table) */
      const int idx = b * radix + r;
      const int k0 = 8 * b;
      table->q15_re[idx] = pack8_twiddle_q15_re_re_scaled(k0, r, N, scale);
      table->q15_im[idx] = pack8_twiddle_q15_im_signed_scaled(k0, r, N, scale, DFT_DIR_FORWARD);
      table->q15_im_inv[idx] = pack8_twiddle_q15_im_signed_scaled(k0, r, N, scale, DFT_DIR_INVERSE);
    }
  }

  table->q15_blocks = blocks;
  return 1;
}

#define POWER2_TWIDDLE_BUILD(N_) \
  AssertFatal(power2_twiddle_build(&g_power2_twiddle_##N_, N_, 8), "radix-8 twiddles for N=%d: allocation failed\n", N_);

static DFT_TABLE_BUILDER void power2_twiddles_build(void)
{
  POWER2_TWIDDLE_SIZES(POWER2_TWIDDLE_BUILD)
}
#undef POWER2_TWIDDLE_BUILD
#undef POWER2_TWIDDLE_SIZES

typedef struct {
  int blocks;
  simde__m256i *storage;
} sr_twiddle_simd_t;

#define SR_W1_RE_NEGIM(TW) ((TW)->storage)
#define SR_W1_IM_RE(TW) ((TW)->storage + (TW)->blocks)
#define SR_W3_RE_NEGIM(TW) ((TW)->storage + 2 * (TW)->blocks)
#define SR_W3_IM_RE(TW) ((TW)->storage + 3 * (TW)->blocks)

static sr_twiddle_simd_t sr_twiddles_fwd[SR_TWIDDLE_SLOT_COUNT];
static sr_twiddle_simd_t sr_twiddles_bwd[SR_TWIDDLE_SLOT_COUNT];

static int init_sr_twiddle_simd(sr_twiddle_simd_t *tw, int N, dft_dir_t dir)
{
  int quarter = N / 4;
  int blocks = quarter / 8;

  tw->blocks = blocks;

  tw->storage = aligned_alloc(32, (size_t)4 * (size_t)blocks * sizeof(simde__m256i));
  if (!tw->storage)
    return 0;

  for (int b = 0; b < blocks; b++) {
    int16_t w1_re_negim[16] __attribute__((aligned(64)));
    int16_t w1_im_re[16] __attribute__((aligned(64)));
    int16_t w3_re_negim[16] __attribute__((aligned(64)));
    int16_t w3_im_re[16] __attribute__((aligned(64)));

    for (int j = 0; j < 8; j++) {
      int k = 8 * b + j;

      float theta1 = (float)dir * 2.0f * (float)M_PI * k / (float)N;
      float theta3 = (float)dir * 6.0f * (float)M_PI * k / (float)N;

      int16_t w1r = q15_untie(sat_i16(lrintf(32767.0f * cosf(theta1))) / 2);
      int16_t w1i = q15_untie(sat_i16(lrintf(32767.0f * sinf(theta1))) / 2);

      int16_t w3r = q15_untie(sat_i16(lrintf(32767.0f * cosf(theta3))) / 2);
      int16_t w3i = q15_untie(sat_i16(lrintf(32767.0f * sinf(theta3))) / 2);

      w1_re_negim[2 * j] = w1r;
      w1_re_negim[2 * j + 1] = sat_i16(-(long)w1i);
      w1_im_re[2 * j] = w1i;
      w1_im_re[2 * j + 1] = w1r;

      w3_re_negim[2 * j] = w3r;
      w3_re_negim[2 * j + 1] = sat_i16(-(long)w3i);
      w3_im_re[2 * j] = w3i;
      w3_im_re[2 * j + 1] = w3r;
    }

    SR_W1_RE_NEGIM(tw)[b] = simde_mm256_load_si256((simde__m256i *)w1_re_negim);
    SR_W1_IM_RE(tw)[b] = simde_mm256_load_si256((simde__m256i *)w1_im_re);
    SR_W3_RE_NEGIM(tw)[b] = simde_mm256_load_si256((simde__m256i *)w3_re_negim);
    SR_W3_IM_RE(tw)[b] = simde_mm256_load_si256((simde__m256i *)w3_im_re);
  }
  return 1;
}

static sr_twiddle_simd_t *sr_twiddle_table_create(int N, dft_dir_t dir)
{
  const int idx = log2_int((unsigned int)N) - SR_MIN_LOG2;

  sr_twiddle_simd_t *tw = (dir == DFT_DIR_FORWARD) ? &sr_twiddles_fwd[idx] : &sr_twiddles_bwd[idx];

  memset(tw, 0, sizeof(*tw));

  if (!init_sr_twiddle_simd(tw, N, dir)) {
    return NULL;
  }

  return tw;
}

/* N = 16384 is the only split-radix size (dft16384_split_radix). */
static DFT_TABLE_BUILDER void sr_twiddles_build(void)
{
  AssertFatal(sr_twiddle_table_create(16384, DFT_DIR_FORWARD) && sr_twiddle_table_create(16384, DFT_DIR_INVERSE),
              "Split-radix twiddles: allocation failed\n");
}

static __attribute__((always_inline)) inline const sr_twiddle_simd_t *sr_twiddle_table_get(int N, dft_dir_t dir)
{
  const int idx = log2_int((unsigned int)N) - SR_MIN_LOG2;
  return dir == DFT_DIR_FORWARD ? &sr_twiddles_fwd[idx] : &sr_twiddles_bwd[idx];
}

#define DFT_MIXED_POWER2_MAX_N 65536

/* Rounded halving. Leaf kernels halve their inputs before the first add so a
 * sum of two full-scale samples cannot saturate; their twiddles carry x2.
 * mulhrs by 16384 rounds every tie (odd x) up: +0.25 LSB mean error per halving, which adds up in
 * the DC bin. Here the ties round toward zero in the even complex lanes (16383) and away from zero
 * in the odd ones (16385): no mean error, and the gain errors of the two sets of lanes cancel. */
#define Q15_HALF_UNBIASED_8(c) c - 1, c - 1, c + 1, c + 1, c - 1, c - 1, c + 1, c + 1
static inline simde__m256i half_q15_256(simde__m256i x)
{
  return simde_mm256_mulhrs_epi16(x, simde_mm256_setr_epi16(Q15_HALF_UNBIASED_8(16384), Q15_HALF_UNBIASED_8(16384)));
}

/* x / 2 with ties to the odd neighbour, (x >> 1) | (x & 1): no mean error and no gain error, one or
 * two operations more than half_q15_256(). Used by the DFT8 core, where the rounding matters most. */
static inline simde__m256i half_odd_q15_256(simde__m256i x)
{
  return simde_mm256_or_si256(simde_mm256_srai_epi16(x, 1), simde_mm256_and_si256(x, simde_mm256_set1_epi16(1)));
}

/* x / 4, same tie handling as half_q15_256(). */
static inline simde__m256i quarter_q15_256(simde__m256i x)
{
  return simde_mm256_mulhrs_epi16(x, simde_mm256_setr_epi16(Q15_HALF_UNBIASED_8(8192), Q15_HALF_UNBIASED_8(8192)));
}

static inline simde__m256i scale_q15_256(simde__m256i x, int16_t q15)
{
  return simde_mm256_mulhrs_epi16(x, simde_mm256_set1_epi16(q15));
}

static inline simde__m128i half_q15_128(simde__m128i x)
{
  return simde_mm_mulhrs_epi16(x, simde_mm_setr_epi16(Q15_HALF_UNBIASED_8(16384)));
}

/* Radix-9 first stages scale their inputs by 1/sqrt(3) so the raw radix-3
 * cannot saturate. */
static inline simde__m256i inv_sqrt3_q15_256(simde__m256i x)
{
  return simde_mm256_mulhrs_epi16(x, simde_mm256_set1_epi16(Q15_INV_SQRT3));
}

static inline simde__m128i inv_sqrt3_q15_128(simde__m128i x)
{
  return simde_mm_mulhrs_epi16(x, simde_mm_set1_epi16(Q15_INV_SQRT3));
}

static DFT_TABLE_BUILDER void fixed_64_128_twiddles_build(void)
{
  fixed_64_128_twiddles_init(64);
  fixed_64_128_twiddles_init(128);
}

static inline simde__m128i dft64_dc_from_raw_sums(simde__m256i a, simde__m256i b)
{
  /* Interleave a and b per component and add the pairs in 32 bits: [re, im, re, im] per 128-bit lane. */
  const simde__m256i ones = simde_mm256_set1_epi16(1);
  simde__m256i sum = simde_mm256_add_epi32(simde_mm256_madd_epi16(simde_mm256_unpacklo_epi16(a, b), ones),
                                           simde_mm256_madd_epi16(simde_mm256_unpackhi_epi16(a, b), ones));
  simde__m128i dc32 = simde_mm_add_epi32(simde_mm256_castsi256_si128(sum), simde_mm256_extracti128_si256(sum, 1));
  dc32 = simde_mm_add_epi32(dc32, simde_mm_shuffle_epi32(dc32, 0x4E)); /* [re, im] in lanes 0 and 1 */

  /* Rounded division by 8. */
  const simde__m128i sign = simde_mm_srai_epi32(dc32, 31);

  dc32 = simde_mm_add_epi32(dc32, simde_mm_add_epi32(simde_mm_set1_epi32(4), sign));

  dc32 = simde_mm_srai_epi32(dc32, 3);

  return simde_mm_packs_epi32(dc32, simde_mm_setzero_si128());
}
