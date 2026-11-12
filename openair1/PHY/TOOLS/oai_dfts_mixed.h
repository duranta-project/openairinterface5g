/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* Internal part of oai_dfts.c: radix-5/12/15/18/24/27/30 parents and the radix-3 x power-of-two path.
 * Included only by oai_dfts.c, in order; not a public header. */

#ifndef OAI_DFTS_INTERNAL
#error "oai_dfts_mixed.h is included only by oai_dfts.c"
#endif

static inline void radix5_butterfly8_q15_256(simde__m256i x0,
                                             simde__m256i x1,
                                             simde__m256i x2,
                                             simde__m256i x3,
                                             simde__m256i x4,
                                             simde__m256i *y0,
                                             simde__m256i *y1,
                                             simde__m256i *y2,
                                             simde__m256i *y3,
                                             simde__m256i *y4,
                                             dft_dir_t dir)
{
  const simde__m256i t1 = simde_mm256_adds_epi16(x1, x4);
  const simde__m256i t2 = simde_mm256_adds_epi16(x2, x3);
  const simde__m256i d1 = simde_mm256_subs_epi16(x1, x4);
  const simde__m256i d2 = simde_mm256_subs_epi16(x2, x3);

  *y0 = simde_mm256_adds_epi16(x0, simde_mm256_adds_epi16(t1, t2));

  const simde__m256i b1 =
      simde_mm256_adds_epi16(x0,
                             simde_mm256_adds_epi16(simde_mm256_mulhrs_epi16(t1, simde_mm256_set1_epi16(Q15_COS_2PI_5)),
                                                    simde_mm256_mulhrs_epi16(t2, simde_mm256_set1_epi16(Q15_COS_4PI_5))));
  /* (-j)^dir rotation folded into the sine multiplies */
  const simde__m256i q1_mj = simde_mm256_adds_epi16(mul_minus_j_scaled_dir_i16_256(d1, Q15_SIN_2PI_5, dir),
                                                    mul_minus_j_scaled_dir_i16_256(d2, Q15_SIN_4PI_5, dir));
  *y1 = simde_mm256_adds_epi16(b1, q1_mj);
  *y4 = simde_mm256_subs_epi16(b1, q1_mj);

  const simde__m256i b2 =
      simde_mm256_adds_epi16(x0,
                             simde_mm256_adds_epi16(simde_mm256_mulhrs_epi16(t1, simde_mm256_set1_epi16(Q15_COS_4PI_5)),
                                                    simde_mm256_mulhrs_epi16(t2, simde_mm256_set1_epi16(Q15_COS_2PI_5))));
  /* (-j)^dir rotation folded into the sine multiplies */
  const simde__m256i q2_mj = simde_mm256_subs_epi16(mul_minus_j_scaled_dir_i16_256(d1, Q15_SIN_4PI_5, dir),
                                                    mul_minus_j_scaled_dir_i16_256(d2, Q15_SIN_2PI_5, dir));
  *y2 = simde_mm256_adds_epi16(b2, q2_mj);
  *y3 = simde_mm256_subs_epi16(b2, q2_mj);
}

/* Unitary radix-5 butterfly (output = DFT5 / sqrt(5)) on unscaled inputs. The pair sums
 * x1 + x4, x2 + x3 and differences grow the rms by sqrt(2) only; 1/sqrt(5) is applied to x0
 * and folded into the cos/sin constants, so no sum of more than two inputs is formed unscaled. */
#define Q15_COS_2PI_5_S5 ((int16_t)4528) /* cos(2pi/5) / sqrt(5) */
#define Q15_COS_4PI_5_S5 ((int16_t)-11855) /* cos(4pi/5) / sqrt(5) */
#define Q15_SIN_2PI_5_S5 ((int16_t)13937) /* sin(2pi/5) / sqrt(5) */
#define Q15_SIN_4PI_5_S5 ((int16_t)8613) /* sin(4pi/5) / sqrt(5) */
/* The same butterfly scaled by 1/sqrt(2) (R30 parents: the DFT6 carries 1/sqrt(15)). */
#define Q15_COS_2PI_5_S2 ((int16_t)7160) /* cos(2pi/5) / sqrt(2) */
#define Q15_COS_4PI_5_S2 ((int16_t)-18745) /* cos(4pi/5) / sqrt(2) */
#define Q15_SIN_2PI_5_S2 ((int16_t)22036) /* sin(2pi/5) / sqrt(2) */
#define Q15_SIN_4PI_5_S2 ((int16_t)13619) /* sin(4pi/5) / sqrt(2) */
static __attribute__((always_inline)) inline void radix5_butterfly8_scaled_q15_256(int16_t scale,
                                                                                   int16_t c2,
                                                                                   int16_t c4,
                                                                                   int16_t s2,
                                                                                   int16_t s4,
                                                                                   simde__m256i x0,
                                                                                   simde__m256i x1,
                                                                                   simde__m256i x2,
                                                                                   simde__m256i x3,
                                                                                   simde__m256i x4,
                                                                                   simde__m256i *y0,
                                                                                   simde__m256i *y1,
                                                                                   simde__m256i *y2,
                                                                                   simde__m256i *y3,
                                                                                   simde__m256i *y4,
                                                                                   dft_dir_t dir)
{
  const simde__m256i t1 = simde_mm256_adds_epi16(x1, x4);
  const simde__m256i t2 = simde_mm256_adds_epi16(x2, x3);
  const simde__m256i d1 = simde_mm256_subs_epi16(x1, x4);
  const simde__m256i d2 = simde_mm256_subs_epi16(x2, x3);
  const simde__m256i s5 = simde_mm256_set1_epi16(scale);
  const simde__m256i x0s = simde_mm256_mulhrs_epi16(x0, s5);

  *y0 = simde_mm256_adds_epi16(x0s, simde_mm256_adds_epi16(simde_mm256_mulhrs_epi16(t1, s5), simde_mm256_mulhrs_epi16(t2, s5)));

  const simde__m256i b1 = simde_mm256_adds_epi16(x0s,
                                                 simde_mm256_adds_epi16(simde_mm256_mulhrs_epi16(t1, simde_mm256_set1_epi16(c2)),
                                                                        simde_mm256_mulhrs_epi16(t2, simde_mm256_set1_epi16(c4))));
  /* (-j)^dir rotation folded into the sine multiplies */
  const simde__m256i q1_mj =
      simde_mm256_adds_epi16(mul_minus_j_scaled_dir_i16_256(d1, s2, dir), mul_minus_j_scaled_dir_i16_256(d2, s4, dir));
  *y1 = simde_mm256_adds_epi16(b1, q1_mj);
  *y4 = simde_mm256_subs_epi16(b1, q1_mj);

  const simde__m256i b2 = simde_mm256_adds_epi16(x0s,
                                                 simde_mm256_adds_epi16(simde_mm256_mulhrs_epi16(t1, simde_mm256_set1_epi16(c4)),
                                                                        simde_mm256_mulhrs_epi16(t2, simde_mm256_set1_epi16(c2))));
  /* (-j)^dir rotation folded into the sine multiplies */
  const simde__m256i q2_mj =
      simde_mm256_subs_epi16(mul_minus_j_scaled_dir_i16_256(d1, s4, dir), mul_minus_j_scaled_dir_i16_256(d2, s2, dir));
  *y2 = simde_mm256_adds_epi16(b2, q2_mj);
  *y3 = simde_mm256_subs_epi16(b2, q2_mj);
}

static inline void radix5_butterfly8_unitary_q15_256(simde__m256i x0,
                                                     simde__m256i x1,
                                                     simde__m256i x2,
                                                     simde__m256i x3,
                                                     simde__m256i x4,
                                                     simde__m256i *y0,
                                                     simde__m256i *y1,
                                                     simde__m256i *y2,
                                                     simde__m256i *y3,
                                                     simde__m256i *y4,
                                                     dft_dir_t dir)
{
  radix5_butterfly8_scaled_q15_256(Q15_INV_SQRT5,
                                   Q15_COS_2PI_5_S5,
                                   Q15_COS_4PI_5_S5,
                                   Q15_SIN_2PI_5_S5,
                                   Q15_SIN_4PI_5_S5,
                                   x0,
                                   x1,
                                   x2,
                                   x3,
                                   x4,
                                   y0,
                                   y1,
                                   y2,
                                   y3,
                                   y4,
                                   dir);
}

/* Radix-18 helpers for DFT144 and DFT1152 use a Good-Thomas 2 x 9 PFA.
 * 18 = 2 x 9 with gcd(2,9)=1, so there are no twiddles between the
 * radix-2 and radix-9 dimensions.  The DFT9 is unitary; the remaining
 * 1/sqrt(2) of the raw radix-2 combine is folded into the parent W_N
 * twiddles.
 *
 * Input CRT mapping:  n = 9*n2 + 10*n9 (mod 18)
 * Output mapping:     k = 9*k2 + 2*k9 (mod 18)
 */

/* Unitary DFT9: 9 = 3 x 3 Cooley-Tukey. The inputs of the first radix-3
 * layer are scaled by 1/sqrt(3) and its outputs by 1/sqrt(3) (folded into W9
 * where one is applied), so the second raw radix-3 ends at unit gain and no
 * sum of full-scale inputs can saturate. */

static __attribute__((always_inline)) inline simde__m256i dft9_twiddle_mul_scaled_q15_256(simde__m256i x, int p, dft_dir_t dir)
{
  /* W9^p / sqrt(3) */
  int16_t wr, wi;
  switch (p) {
    case 1:
      wr = 14492;
      wi = -12160;
      break;
    case 2:
      wr = 3285;
      wi = -18631;
      break;
    default:
      wr = -17777;
      wi = -6470;
      break;
  }
  return complex_mul8_bcast_small_q15_256(x, wr, twiddle_im_scalar_dir_i16(wi, dir));
}
/* raw_input: x is the transform input (see radix3_butterfly8_unitary_q15_256). */
static __attribute__((always_inline)) inline void dft9x8_unitary_q15_256(simde__m256i x[9],
                                                                         simde__m256i y[9],
                                                                         dft_dir_t dir,
                                                                         const int raw_input)
{
  const simde__m256i s3 = simde_mm256_set1_epi16(Q15_INV_SQRT3);
  simde__m256i a[3][3];
  for (int n = 0; n < 3; n++)
    DFT_R3_SCALED_INPUT_BUTTERFLY(raw_input,
                                  simde__m256i,
                                  radix3_butterfly8_unitary_q15_256,
                                  radix3_butterfly8_q15_256,
                                  inv_sqrt3_q15_256,
                                  x[n],
                                  x[n + 3],
                                  x[n + 6],
                                  &a[n][0],
                                  &a[n][1],
                                  &a[n][2]);
  a[0][0] = simde_mm256_mulhrs_epi16(a[0][0], s3);
  a[0][1] = simde_mm256_mulhrs_epi16(a[0][1], s3);
  a[0][2] = simde_mm256_mulhrs_epi16(a[0][2], s3);
  a[1][0] = simde_mm256_mulhrs_epi16(a[1][0], s3);
  a[2][0] = simde_mm256_mulhrs_epi16(a[2][0], s3);
  a[1][1] = dft9_twiddle_mul_scaled_q15_256(a[1][1], 1, dir);
  a[2][1] = dft9_twiddle_mul_scaled_q15_256(a[2][1], 2, dir);
  a[1][2] = dft9_twiddle_mul_scaled_q15_256(a[1][2], 2, dir);
  a[2][2] = dft9_twiddle_mul_scaled_q15_256(a[2][2], 4, dir);
  for (int k = 0; k < 3; k++)
    radix3_butterfly8_q15_256(a[0][k], a[1][k], a[2][k], &y[k], &y[k + 3], &y[k + 6], dir);
}

/* DFT1152: R18 = 2 x 9 PFA, M=64. */

static simde__m256i dft1152_radix18_twiddle_re[8][18] __attribute__((aligned(64))),
    dft1152_radix18_twiddle_im[8][18] __attribute__((aligned(64)));
/* Parent twiddle tables: W_N^(br*n) * 1/sqrt(SCALE_RADIX) for each branch and block of eight n. */
#define DEFINE_PACKED_PARENT_TWIDDLE_INIT(INIT, TAG, NVAL, RADIX, BLOCKS, SCALE_RADIX)       \
  DEFINE_Q15_TABLE_INITIALIZER(                                                              \
      INIT, const float sc = 1.0f / sqrtf(SCALE_RADIX); for (int ds = 0; ds < 2; ds++) {     \
        const dft_dir_t dir = ds == 0 ? DFT_DIR_FORWARD : DFT_DIR_INVERSE;                   \
        for (int bl = 0; bl < BLOCKS; bl++) {                                                \
          int off = 8 * bl;                                                                  \
          for (int br = 0; br < RADIX; br++) {                                               \
            TAG##_re[bl][br] = pack8_twiddle_q15_re_re_scaled(off, br, NVAL, sc);            \
            if (ds == 0)                                                                     \
              TAG##_im[bl][br] = pack8_twiddle_q15_im_signed_scaled(off, br, NVAL, sc, dir); \
          }                                                                                  \
        }                                                                                    \
      })

DEFINE_PACKED_PARENT_TWIDDLE_INIT(dft1152_radix18_twiddles_init, dft1152_radix18_twiddle, 1152, 18, 8, 1.0f)
static inline void dft1152_radix18_stage(const c16_t *src, c16_t *b, dft_dir_t dir)
{
  enum { M = 64 };
  static const int i0[9] = {0, 10, 2, 12, 4, 14, 6, 16, 8}, i1[9] = {9, 1, 11, 3, 13, 5, 15, 7, 17};
  for (int off = 0; off < M; off += 8) {
    simde__m256i x0[9], x1[9], s0[9], s1[9];
    for (int j = 0; j < 9; j++) {
      x0[j] = simde_mm256_loadu_si256((const simde__m256i *)(src + i0[j] * M + off));
      x1[j] = simde_mm256_loadu_si256((const simde__m256i *)(src + i1[j] * M + off));
    }
    dft9x8_unitary_q15_256(x0, s0, dir, 1);
    dft9x8_unitary_q15_256(x1, s1, dir, 1);
    for (int k = 0; k < 9; k++) {
      int b0 = 2 * k, b1 = (9 + 2 * k) % 18;
      /* radix-2 of the two DFT9 outputs scaled by 1/sqrt(2) first: their sum saturates otherwise */
      const simde__m256i h0 = simde_mm256_mulhrs_epi16(s0[k], simde_mm256_set1_epi16(Q15_INV_SQRT2)),
                         h1 = simde_mm256_mulhrs_epi16(s1[k], simde_mm256_set1_epi16(Q15_INV_SQRT2));
      simde__m256i z0 = simde_mm256_adds_epi16(h0, h1), z1 = simde_mm256_subs_epi16(h0, h1);
      simde__m256i v0 = b0 == 0 ? z0
                                : complex_mul8_prepack_q15_256(z0,
                                                               dft1152_radix18_twiddle_re[off >> 3][b0],
                                                               twiddle_im_dir_256(dft1152_radix18_twiddle_im[off >> 3][b0], dir));
      simde__m256i v1 = complex_mul8_prepack_q15_256(z1,
                                                     dft1152_radix18_twiddle_re[off >> 3][b1],
                                                     twiddle_im_dir_256(dft1152_radix18_twiddle_im[off >> 3][b1], dir));
      simde_mm256_store_si256((simde__m256i *)(b + b0 * M + off), v0);
      simde_mm256_store_si256((simde__m256i *)(b + b1 * M + off), v1);
    }
  }
}
static void dft1152_radix18_leaf64(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { M = 64 };
  c16_t b[18 * M] __attribute__((aligned(64)));
  dft1152_radix18_stage(src, b, dir);
  dft64x8_branch_major_store(b, M, dst, 18, 0, dir);
  dft64x8_branch_major_store(b, M, dst, 18, 8, dir);
  c16_t tail[64] __attribute__((aligned(64)));
  for (int br = 16; br < 18; br++) {
    dft64_avx(b + br * M, tail, dir);
    for (int k = 0; k < M; k++)
      dst[18 * k + br] = tail[k];
  }
}

static inline void raw_radix12_pfa8_q15_256(const c16_t *src, int M, int off, simde__m256i y[12], dft_dir_t dir)
{
  static const int ids[3][4] = {{0, 9, 6, 3}, {4, 1, 10, 7}, {8, 5, 2, 11}};
  simde__m256i s[3][4];
  for (int n = 0; n < 3; n++) {
    simde__m256i x0 = simde_mm256_loadu_si256((const simde__m256i *)(src + ids[n][0] * M + off)),
                 x1 = simde_mm256_loadu_si256((const simde__m256i *)(src + ids[n][1] * M + off)),
                 x2 = simde_mm256_loadu_si256((const simde__m256i *)(src + ids[n][2] * M + off)),
                 x3 = simde_mm256_loadu_si256((const simde__m256i *)(src + ids[n][3] * M + off));
    /* Unitary DFT4: halve between the two add layers, so neither layer grows more than 2x the input
     * (the first layer is exact up to half full scale, the second sees halved values). */
    const simde__m256i s02 = half_q15_256(simde_mm256_adds_epi16(x0, x2)), d02 = half_q15_256(simde_mm256_subs_epi16(x0, x2)),
                       s13 = half_q15_256(simde_mm256_adds_epi16(x1, x3)), d13 = half_q15_256(simde_mm256_subs_epi16(x1, x3));
    s[n][0] = simde_mm256_adds_epi16(s02, s13);
    s[n][2] = simde_mm256_subs_epi16(s02, s13);
    add_pm_j_i16_256(d02, d13, &s[n][1], &s[n][3], dir);
  }
  /* Radix-3 on intermediate data: scale its inputs by 1/sqrt(3) first (peaks are several times the rms),
   * so the parents apply unscaled twiddles. */
  for (int k = 0; k < 4; k++) {
    simde__m256i z0, z1, z2;
    radix3_butterfly8_q15_256(inv_sqrt3_q15_256(s[0][k]),
                              inv_sqrt3_q15_256(s[1][k]),
                              inv_sqrt3_q15_256(s[2][k]),
                              &z0,
                              &z1,
                              &z2,
                              dir);
    y[(3 * k) % 12] = z0;
    y[(4 + 3 * k) % 12] = z1;
    y[(8 + 3 * k) % 12] = z2;
  }
}

/* DFT96: R12 PFA, M=8. */
static simde__m256i dft96_radix12_twiddle_re[1][12] __attribute__((aligned(64))),
    dft96_radix12_twiddle_im[1][12] __attribute__((aligned(64)));
DEFINE_PACKED_PARENT_TWIDDLE_INIT(dft96_radix12_twiddles_init, dft96_radix12_twiddle, 96, 12, 1, (float)1)
/* Fixed-size PFA parents share one normalized vector stage and packed output layout. */
#define DEFINE_PACKED_PFA_PARENT(FN, MVAL, RADIX, RAW, TAG, SCALE)                                                              \
  static inline void FN(const c16_t *src, c16_t *b, dft_dir_t dir)                                                              \
  {                                                                                                                             \
    enum { M = MVAL };                                                                                                          \
    for (int off = 0; off < M; off += 8) {                                                                                      \
      simde__m256i z[RADIX];                                                                                                    \
      RAW(src, M, off, z, dir);                                                                                                 \
      for (int br = 0; br < RADIX; br++) {                                                                                      \
        simde__m256i v =                                                                                                        \
            br == 0                                                                                                             \
                ? ((SCALE) ? simde_mm256_mulhrs_epi16(z[br], simde_mm256_set1_epi16(SCALE)) : z[br])                            \
                : complex_mul8_prepack_q15_256(z[br], TAG##_re[off >> 3][br], twiddle_im_dir_256(TAG##_im[off >> 3][br], dir)); \
        simde_mm256_store_si256((simde__m256i *)(b + br * M + off), v);                                                         \
      }                                                                                                                         \
    }                                                                                                                           \
  }

DEFINE_PACKED_PFA_PARENT(dft96_radix12_stage, 8, 12, raw_radix12_pfa8_q15_256, dft96_radix12_twiddle, 0)
static void dft96_radix12_pfa_leaf8(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { R = 12, M = 8 };
  c16_t b[R * M] __attribute__((aligned(64)));
  dft96_radix12_stage(src, b, dir);
  dft8x8_twiddle_scaled_store(b, M, dst, R, 0, 8, dir);
  dft8x8_twiddle_scaled_store(b, M, dst, R, 8, 4, dir);
}

/* DFT120: R15 = 3 x 5 Good-Thomas PFA. */

/* Radix-15 parents run the unitary radix-5 butterfly on the inputs and scale
 * its outputs by 1/sqrt(3) before the raw radix-3; the parent twiddles have
 * unit magnitude. */
#define DEFINE_RAW_RADIX15_PFA8(FN)                                                                                                \
  static inline void FN(const c16_t *src, int M, int off, simde__m256i y[15], dft_dir_t dir)                                       \
  {                                                                                                                                \
    static const int ids[3][5] = {{0, 6, 12, 3, 9}, {10, 1, 7, 13, 4}, {5, 11, 2, 8, 14}};                                         \
    simde__m256i s[3][5];                                                                                                          \
    for (int n3 = 0; n3 < 3; n3++) {                                                                                               \
      simde__m256i x[5];                                                                                                           \
      for (int j = 0; j < 5; j++)                                                                                                  \
        x[j] = simde_mm256_loadu_si256((const simde__m256i *)(src + ids[n3][j] * M + off));                                        \
      radix5_butterfly8_unitary_q15_256(x[0], x[1], x[2], x[3], x[4], &s[n3][0], &s[n3][1], &s[n3][2], &s[n3][3], &s[n3][4], dir); \
    }                                                                                                                              \
    for (int n3 = 0; n3 < 3; n3++)                                                                                                 \
      for (int k5 = 0; k5 < 5; k5++)                                                                                               \
        s[n3][k5] = simde_mm256_mulhrs_epi16(s[n3][k5], simde_mm256_set1_epi16(Q15_INV_SQRT3));                                    \
    for (int k5 = 0; k5 < 5; k5++) {                                                                                               \
      simde__m256i z0, z1, z2;                                                                                                     \
      radix3_butterfly8_q15_256(s[0][k5], s[1][k5], s[2][k5], &z0, &z1, &z2, dir);                                                 \
      y[(3 * k5) % 15] = z0;                                                                                                       \
      y[(5 + 3 * k5) % 15] = z1;                                                                                                   \
      y[(10 + 3 * k5) % 15] = z2;                                                                                                  \
    }                                                                                                                              \
  }

DEFINE_RAW_RADIX15_PFA8(r15_pfa_raw_block_n120)

static simde__m256i dft120_r15p_re[1][15] __attribute__((aligned(64))), dft120_r15p_im[1][15] __attribute__((aligned(64)));
DEFINE_PACKED_PARENT_TWIDDLE_INIT(dft120_r15p_tw_init, dft120_r15p, 120, 15, 1, 1.0f)
DEFINE_PACKED_PFA_PARENT(radix15_pfa_stage_120, 8, 15, r15_pfa_raw_block_n120, dft120_r15p, 0)
static void dft120_radix15_pfa(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { R = 15, M = 8 };
  c16_t b[R * M] __attribute__((aligned(64)));
  radix15_pfa_stage_120(src, b, dir);
  dft8x8_twiddle_scaled_store(b, M, dst, R, 0, 8, dir);
  dft8x8_twiddle_scaled_store(b, M, dst, R, 8, 7, dir);
}

/* DFT144: R18 = 2 x 9 PFA, M=8. */

static simde__m256i dft144_radix18_twiddle_re[1][18] __attribute__((aligned(64))),
    dft144_radix18_twiddle_im[1][18] __attribute__((aligned(64)));
DEFINE_PACKED_PARENT_TWIDDLE_INIT(dft144_radix18_twiddles_init, dft144_radix18_twiddle, 144, 18, 1, 1.0f)
static inline void dft144_radix18_stage(const c16_t *src, c16_t *b, dft_dir_t dir)
{
  enum { M = 8 };
  static const int i0[9] = {0, 10, 2, 12, 4, 14, 6, 16, 8}, i1[9] = {9, 1, 11, 3, 13, 5, 15, 7, 17};
  for (int off = 0; off < M; off += 8) {
    simde__m256i x0[9], x1[9], s0[9], s1[9];
    for (int j = 0; j < 9; j++) {
      x0[j] = simde_mm256_loadu_si256((const simde__m256i *)(src + i0[j] * M + off));
      x1[j] = simde_mm256_loadu_si256((const simde__m256i *)(src + i1[j] * M + off));
    }
    dft9x8_unitary_q15_256(x0, s0, dir, 1);
    dft9x8_unitary_q15_256(x1, s1, dir, 1);
    for (int k = 0; k < 9; k++) {
      int b0 = 2 * k, b1 = (9 + 2 * k) % 18;
      /* radix-2 of the two DFT9 outputs scaled by 1/sqrt(2) first: their sum saturates otherwise */
      const simde__m256i h0 = simde_mm256_mulhrs_epi16(s0[k], simde_mm256_set1_epi16(Q15_INV_SQRT2)),
                         h1 = simde_mm256_mulhrs_epi16(s1[k], simde_mm256_set1_epi16(Q15_INV_SQRT2));
      simde__m256i z0 = simde_mm256_adds_epi16(h0, h1), z1 = simde_mm256_subs_epi16(h0, h1);
      simde__m256i v0 = b0 == 0 ? z0
                                : complex_mul8_prepack_q15_256(z0,
                                                               dft144_radix18_twiddle_re[off >> 3][b0],
                                                               twiddle_im_dir_256(dft144_radix18_twiddle_im[off >> 3][b0], dir));
      simde__m256i v1 = complex_mul8_prepack_q15_256(z1,
                                                     dft144_radix18_twiddle_re[off >> 3][b1],
                                                     twiddle_im_dir_256(dft144_radix18_twiddle_im[off >> 3][b1], dir));
      simde_mm256_store_si256((simde__m256i *)(b + b0 * M + off), v0);
      simde_mm256_store_si256((simde__m256i *)(b + b1 * M + off), v1);
    }
  }
}
static void dft144_radix18_leaf8(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { R = 18, M = 8 };
  c16_t b[R * M] __attribute__((aligned(64)));
  dft144_radix18_stage(src, b, dir);
  dft8x8_twiddle_scaled_store(b, M, dst, R, 0, 8, dir);
  dft8x8_twiddle_scaled_store(b, M, dst, R, 8, 8, dir);
  dft8x8_twiddle_scaled_store(b, M, dst, R, 16, 2, dir);
}

/* DFT192: R12 PFA, M=16. */

static simde__m256i dft192_radix12_twiddle_re[2][12] __attribute__((aligned(64))),
    dft192_radix12_twiddle_im[2][12] __attribute__((aligned(64)));
DEFINE_PACKED_PARENT_TWIDDLE_INIT(dft192_radix12_twiddles_init, dft192_radix12_twiddle, 192, 12, 2, (float)1)
DEFINE_PACKED_PFA_PARENT(dft192_radix12_stage, 16, 12, raw_radix12_pfa8_q15_256, dft192_radix12_twiddle, 0)
static void dft192_radix12_pfa_leaf16(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { R = 12, M = 16 };
  c16_t b[R * M] __attribute__((aligned(64)));
  dft192_radix12_stage(src, b, dir);
  dft16x8_twiddle_scaled_store(b, M, dst, R, 0, 8, dir);
  dft16x8_twiddle_scaled_store(b, M, dst, R, 8, 4, dir);
}

/* Raw R27 helpers shared by the R27 parent specializations. */
static __attribute__((always_inline)) inline simde__m256i raw_radix27_internal_twiddle_q15_256(simde__m256i x,
                                                                                               int n1,
                                                                                               int k,
                                                                                               dft_dir_t dir)
{
  /* W27^(n1*k) / sqrt(3): the last radix-3 of the R27 parents runs on 1/sqrt(3)-scaled inputs. */
  static const int16_t wr[2][9] = {{18918, 18408, 16906, 14492, 11297, 7493, 3285, -1100, -5426},
                                   {18918, 16906, 11297, 3285, -5426, -12982, -17777, -18790, -15806}};
  static const int16_t wi[2][9] = {{0, -4363, -8490, -12160, -15175, -17371, -18631, -18886, -18123},
                                   {0, -8490, -15175, -18631, -18123, -13760, -6470, 2196, 10396}};
  return complex_mul8_bcast_small_q15_256(x, wr[n1 - 1][k], twiddle_im_scalar_dir_i16(wi[n1 - 1][k], dir));
}
static __attribute__((always_inline)) inline void raw_radix27x8_q15_256(const c16_t *src,
                                                                        int M,
                                                                        int off,
                                                                        simde__m256i y[27],
                                                                        dft_dir_t dir,
                                                                        const int raw_input)
{
  simde__m256i a[3][9];
  for (int n1 = 0; n1 < 3; n1++) {
    simde__m256i x[9];
    for (int n9 = 0; n9 < 9; n9++)
      x[n9] = simde_mm256_loadu_si256((const simde__m256i *)(src + (n1 + 3 * n9) * M + off));
    dft9x8_unitary_q15_256(x, a[n1], dir, raw_input);
  }
  for (int n1 = 1; n1 < 3; n1++)
    for (int k = 1; k < 9; k++)
      a[n1][k] = raw_radix27_internal_twiddle_q15_256(a[n1][k], n1, k, dir);
  {
    const simde__m256i s3 = simde_mm256_set1_epi16(Q15_INV_SQRT3);
    for (int k = 0; k < 9; k++)
      a[0][k] = simde_mm256_mulhrs_epi16(a[0][k], s3);
    a[1][0] = simde_mm256_mulhrs_epi16(a[1][0], s3);
    a[2][0] = simde_mm256_mulhrs_epi16(a[2][0], s3);
  }
  for (int k = 0; k < 9; k++) {
    simde__m256i z0, z1, z2;
    radix3_butterfly8_q15_256(a[0][k], a[1][k], a[2][k], &z0, &z1, &z2, dir);
    y[k] = z0;
    y[k + 9] = z1;
    y[k + 18] = z2;
  }
}

/* R27 front end on the transform input (PFA parents of 432, 864, 1728). */
static __attribute__((always_inline)) inline void raw_radix27x8_input_q15_256(const c16_t *src,
                                                                              int M,
                                                                              int off,
                                                                              simde__m256i y[27],
                                                                              dft_dir_t dir)
{
  raw_radix27x8_q15_256(src, M, off, y, dir, 1);
}

static simde__m256i dft216_r27_re[1][27] __attribute__((aligned(64))), dft216_r27_im[1][27] __attribute__((aligned(64)));
DEFINE_Q15_TABLE_INITIALIZER(
    dft216_r27_tw_init, const float sc = 1.0f; for (int ds = 0; ds < 2; ds++) {
      const dft_dir_t dir = ds == 0 ? DFT_DIR_FORWARD : DFT_DIR_INVERSE;
      for (int br = 0; br < 27; br++) {
        dft216_r27_re[0][br] = pack8_twiddle_q15_re_re_scaled(0, br, 216, sc);
        if (ds == 0)
          dft216_r27_im[0][br] = pack8_twiddle_q15_im_signed_scaled(0, br, 216, sc, dir);
      }
    })
static inline void radix27_stage_216(const c16_t *src, c16_t *b, dft_dir_t dir)
{
  enum { M = 8 };
  simde__m256i z[27];
  raw_radix27x8_q15_256(src, M, 0, z, dir, 1);
  for (int br = 0; br < 27; br++) {
    simde__m256i v =
        br == 0 ? z[br] : complex_mul8_prepack_q15_256(z[br], dft216_r27_re[0][br], twiddle_im_dir_256(dft216_r27_im[0][br], dir));
    simde_mm256_store_si256((simde__m256i *)(b + br * M), v);
  }
}
static void dft216_radix27_leaf8(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { R = 27, M = 8 };
  c16_t b[R * M] __attribute__((aligned(64)));
  radix27_stage_216(src, b, dir);
  for (int first = 0; first < R; first += 8) {
    const int valid = (R - first) < 8 ? (R - first) : 8;
    dft8x8_twiddle_scaled_store(b, M, dst, R, first, valid, dir);
  }
}

/* DFT324: R27 x DFT12. The last four samples of each branch are
 * zero-padded to eight SIMD lanes for the R27 stage; partial stores keep
 * only the four valid samples. */

static __attribute__((always_inline)) inline void store_radix27_partial8(c16_t *dst, simde__m256i value, int valid)
{
  if (valid == 8)
    simde_mm256_storeu_si256((simde__m256i *)dst, value);
  else
    simde_mm_storeu_si128((simde__m128i *)dst, simde_mm256_castsi256_si128(value));
}

static __attribute__((always_inline)) inline void
radix27_raw_partial8(const c16_t *src, int M, int off, int valid, simde__m256i y[27], dft_dir_t dir, const int raw_input)
{
  simde__m256i a[3][9];
  for (int n1 = 0; n1 < 3; n1++) {
    simde__m256i x[9];
    for (int n9 = 0; n9 < 9; n9++)
      x[n9] = load_complex8_partial(src + (n1 + 3 * n9) * M + off, valid);
    dft9x8_unitary_q15_256(x, a[n1], dir, raw_input);
  }
  for (int n1 = 1; n1 < 3; n1++)
    for (int k = 1; k < 9; k++)
      a[n1][k] = raw_radix27_internal_twiddle_q15_256(a[n1][k], n1, k, dir);
  {
    const simde__m256i s3 = simde_mm256_set1_epi16(Q15_INV_SQRT3);
    for (int k = 0; k < 9; k++)
      a[0][k] = simde_mm256_mulhrs_epi16(a[0][k], s3);
    a[1][0] = simde_mm256_mulhrs_epi16(a[1][0], s3);
    a[2][0] = simde_mm256_mulhrs_epi16(a[2][0], s3);
  }
  for (int k = 0; k < 9; k++)
    radix3_butterfly8_q15_256(a[0][k], a[1][k], a[2][k], &y[k], &y[k + 9], &y[k + 18], dir);
}

static simde__m256i dft324_r27_re[2][27] __attribute__((aligned(64)));
static simde__m256i dft324_r27_im[2][27] __attribute__((aligned(64)));

DEFINE_Q15_TABLE_INITIALIZER(
    dft324_r27_tw_init, const float scale = 1.0f; for (int ds = 0; ds < 2; ds++) {
      const dft_dir_t dir = ds == 0 ? DFT_DIR_FORWARD : DFT_DIR_INVERSE;
      for (int block = 0; block < 2; block++) {
        const int off = 8 * block;
        for (int br = 0; br < 27; br++) {
          dft324_r27_re[block][br] = pack8_twiddle_q15_re_re_scaled(off, br, 324, scale);
          if (ds == 0)
            dft324_r27_im[block][br] = pack8_twiddle_q15_im_signed_scaled(off, br, 324, scale, dir);
        }
      }
    })

static inline void radix27_stage_324(const c16_t *src, c16_t *branches, dft_dir_t dir, const int raw_input)
{
  enum { M = 12 };
  for (int off = 0; off < M; off += 8) {
    const int valid = M - off >= 8 ? 8 : 4;
    simde__m256i z[27];
    radix27_raw_partial8(src, M, off, valid, z, dir, raw_input);
    for (int br = 0; br < 27; br++) {
      const simde__m256i value = br == 0 ? z[br]
                                         : complex_mul8_prepack_q15_256(z[br],
                                                                        dft324_r27_re[off >> 3][br],
                                                                        twiddle_im_dir_256(dft324_r27_im[off >> 3][br], dir));
      store_radix27_partial8(branches + br * M + off, value, valid);
    }
  }
}

static void dft324_radix27_leaf12(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { R = 27, M = 12 };
  c16_t branches[R * M] __attribute__((aligned(64)));
  radix27_stage_324(src, branches, dir, 1);
  for (int first = 0; first < R; first += 8) {
    const int valid = R - first < 8 ? R - first : 8;
    dft12x8_twiddle_scaled_store(branches, M, dst, R, first, valid, dir);
  }
}

static simde__m256i dft108_r27_re[27] __attribute__((aligned(64)));
static simde__m256i dft108_r27_im[27] __attribute__((aligned(64)));

DEFINE_Q15_TABLE_INITIALIZER(
    dft108_r27_tw_init, const float scale = 1.0f; for (int ds = 0; ds < 2; ds++) {
      const dft_dir_t dir = ds == 0 ? DFT_DIR_FORWARD : DFT_DIR_INVERSE;
      for (int br = 0; br < 27; br++) {
        dft108_r27_re[br] = pack8_twiddle_q15_re_re_scaled(0, br, 108, scale);
        if (ds == 0)
          dft108_r27_im[br] = pack8_twiddle_q15_im_signed_scaled(0, br, 108, scale, dir);
      }
    })

#define DEFINE_DFT4_LEAF_STORE(FN, FIRST, SECOND)                                                                              \
  static __attribute__((always_inline)) inline void FN(const c16_t *branches, c16_t *dst, int first, int valid, dft_dir_t dir) \
  {                                                                                                                            \
    enum { FIRST, SECOND };                                                                                                    \
    const simde__m128i zero = simde_mm_setzero_si128();                                                                        \
                                                                                                                               \
    simde__m128i r0 = valid > 0 ? simde_mm_loadu_si128((const simde__m128i *)(branches + (first + 0) * M)) : zero;             \
    simde__m128i r1 = valid > 1 ? simde_mm_loadu_si128((const simde__m128i *)(branches + (first + 1) * M)) : zero;             \
    simde__m128i r2 = valid > 2 ? simde_mm_loadu_si128((const simde__m128i *)(branches + (first + 2) * M)) : zero;             \
    simde__m128i r3 = valid > 3 ? simde_mm_loadu_si128((const simde__m128i *)(branches + (first + 3) * M)) : zero;             \
                                                                                                                               \
    transpose4_complex_i16_128(&r0, &r1, &r2, &r3);                                                                            \
                                                                                                                               \
    simde__m128i y0, y1, y2, y3;                                                                                               \
    dft4x4_q15_128(r0, r1, r2, r3, &y0, &y1, &y2, &y3, dir);                                                                   \
    const simde__m128i y[4] = {y0, y1, y2, y3};                                                                                \
                                                                                                                               \
    for (int k = 0; k < M; k++) {                                                                                              \
      if (valid == 4) {                                                                                                        \
        simde_mm_storeu_si128((simde__m128i *)(dst + R * k + first), y[k]);                                                    \
      } else {                                                                                                                 \
        c16_t lane[4] __attribute__((aligned(16)));                                                                            \
        simde_mm_store_si128((simde__m128i *)lane, y[k]);                                                                      \
        for (int l = 0; l < valid; l++)                                                                                        \
          dst[R * k + first + l] = lane[l];                                                                                    \
      }                                                                                                                        \
    }                                                                                                                          \
  }

DEFINE_DFT4_LEAF_STORE(dft108_leaf4x4_store, M = 4, R = 27)

static void dft108_radix27_leaf4(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { N = 108, R = 27, M = 4 };
  c16_t branches[N] __attribute__((aligned(64)));
  simde__m256i z[R];

  radix27_raw_partial8(src, M, 0, M, z, dir, 1);

  for (int br = 0; br < R; br++) {
    const simde__m256i value =
        br == 0 ? z[br] : complex_mul8_prepack_q15_256(z[br], dft108_r27_re[br], twiddle_im_dir_256(dft108_r27_im[br], dir));

    simde_mm_storeu_si128((simde__m128i *)(branches + br * M), simde_mm256_castsi256_si128(value));
  }

  for (int first = 0; first < R; first += 4) {
    const int valid = R - first < 4 ? R - first : 4;
    dft108_leaf4x4_store(branches, dst, first, valid, dir);
  }
}

/* DFT240: R15 = 3 x 5 Good-Thomas PFA. */

DEFINE_RAW_RADIX15_PFA8(r15_pfa_raw_block_n240)

#undef DEFINE_RAW_RADIX15_PFA8

static simde__m256i dft240_r15p_re[2][15] __attribute__((aligned(64))), dft240_r15p_im[2][15] __attribute__((aligned(64)));
DEFINE_PACKED_PARENT_TWIDDLE_INIT(dft240_r15p_tw_init, dft240_r15p, 240, 15, 2, 1.0f)
DEFINE_PACKED_PFA_PARENT(radix15_pfa_stage_240, 16, 15, r15_pfa_raw_block_n240, dft240_r15p, 0)
static void dft240_radix15_pfa(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { R = 15, M = 16 };
  c16_t b[R * M] __attribute__((aligned(64)));
  radix15_pfa_stage_240(src, b, dir);
  dft16x8_twiddle_scaled_store(b, M, dst, R, 0, 8, dir);
  dft16x8_twiddle_scaled_store(b, M, dst, R, 8, 7, dir);
}

/* These internal tile blocks use the caller's named SIMD registers and arrays. */
#define DFT_LOAD_TRANSPOSE8_ROW(P, SRC, POS) \
  simde__m256i P##0 = SRC[POS + 0];          \
  simde__m256i P##1 = SRC[POS + 1];          \
  simde__m256i P##2 = SRC[POS + 2];          \
  simde__m256i P##3 = SRC[POS + 3];          \
  simde__m256i P##4 = SRC[POS + 4];          \
  simde__m256i P##5 = SRC[POS + 5];          \
  simde__m256i P##6 = SRC[POS + 6];          \
  simde__m256i P##7 = SRC[POS + 7];          \
  transpose8_complex_i16_256(&P##0, &P##1, &P##2, &P##3, &P##4, &P##5, &P##6, &P##7);

#define DFT_STORE8_STRIDED_ROWS(P, DST, FIRST, STRIDE, OFF)                           \
  simde_mm256_storeu_si256((simde__m256i *)(DST + (FIRST + 0) * STRIDE + OFF), P##0); \
  simde_mm256_storeu_si256((simde__m256i *)(DST + (FIRST + 1) * STRIDE + OFF), P##1); \
  simde_mm256_storeu_si256((simde__m256i *)(DST + (FIRST + 2) * STRIDE + OFF), P##2); \
  simde_mm256_storeu_si256((simde__m256i *)(DST + (FIRST + 3) * STRIDE + OFF), P##3); \
  simde_mm256_storeu_si256((simde__m256i *)(DST + (FIRST + 4) * STRIDE + OFF), P##4); \
  simde_mm256_storeu_si256((simde__m256i *)(DST + (FIRST + 5) * STRIDE + OFF), P##5); \
  simde_mm256_storeu_si256((simde__m256i *)(DST + (FIRST + 6) * STRIDE + OFF), P##6); \
  simde_mm256_storeu_si256((simde__m256i *)(DST + (FIRST + 7) * STRIDE + OFF), P##7);

#define DFT_STORE_R30_FREQUENCY_TILE()      \
  DFT_LOAD_TRANSPOSE8_ROW(q, branch, first) \
  DFT_STORE8_STRIDED_ROWS(q, b, off, PITCH, first)

static __attribute__((always_inline)) inline void store_r30_freqmajor_block32(c16_t *b, int off, int valid, simde__m256i branch[32])
{
  enum { PITCH = 32 };

  for (int first = 0; first < 32; first += 8) {
    if (valid == 8) {
      DFT_STORE_R30_FREQUENCY_TILE()
      continue;
    }

    simde__m128i a0 = simde_mm256_castsi256_si128(branch[first + 0]);
    simde__m128i a1 = simde_mm256_castsi256_si128(branch[first + 1]);
    simde__m128i a2 = simde_mm256_castsi256_si128(branch[first + 2]);
    simde__m128i a3 = simde_mm256_castsi256_si128(branch[first + 3]);
    simde__m128i b0 = simde_mm256_castsi256_si128(branch[first + 4]);
    simde__m128i b1 = simde_mm256_castsi256_si128(branch[first + 5]);
    simde__m128i b2 = simde_mm256_castsi256_si128(branch[first + 6]);
    simde__m128i b3 = simde_mm256_castsi256_si128(branch[first + 7]);

    transpose4_complex_i16_128(&a0, &a1, &a2, &a3);
    transpose4_complex_i16_128(&b0, &b1, &b2, &b3);

    const simde__m256i q0 = simde_mm256_inserti128_si256(simde_mm256_castsi128_si256(a0), b0, 1);
    const simde__m256i q1 = simde_mm256_inserti128_si256(simde_mm256_castsi128_si256(a1), b1, 1);
    const simde__m256i q2 = simde_mm256_inserti128_si256(simde_mm256_castsi128_si256(a2), b2, 1);
    const simde__m256i q3 = simde_mm256_inserti128_si256(simde_mm256_castsi128_si256(a3), b3, 1);

    simde_mm256_storeu_si256((simde__m256i *)(b + (off + 0) * PITCH + first), q0);
    simde_mm256_storeu_si256((simde__m256i *)(b + (off + 1) * PITCH + first), q1);
    simde_mm256_storeu_si256((simde__m256i *)(b + (off + 2) * PITCH + first), q2);
    simde_mm256_storeu_si256((simde__m256i *)(b + (off + 3) * PITCH + first), q3);
  }
}

/* DFT360: R30 PFA parent + twiddle-scaled DFT12 leaves (1/sqrt12). The R30 parents scale by
 * 1/sqrt(15) inside the DFT6 and 1/sqrt(2) in W_N. */

static __attribute__((always_inline)) inline void store_r30_leaf_row8(c16_t *dst,
                                                                      int R,
                                                                      int k,
                                                                      int first,
                                                                      int valid,
                                                                      simde__m256i y)
{
  c16_t *d = dst + R * k + first;
  if (valid == 8)
    simde_mm256_storeu_si256((simde__m256i *)d, y);
  else {
    simde_mm_storeu_si128((simde__m128i *)d, simde_mm256_castsi256_si128(y));
    simde_mm_storel_epi64((simde__m128i *)(d + 4), simde_mm256_extracti128_si256(y, 1));
  }
}

static simde__m256i dft360_r30_re[2][30] __attribute__((aligned(64))), dft360_r30_im[2][30] __attribute__((aligned(64)));
DEFINE_PACKED_PARENT_TWIDDLE_INIT(dft360_r30_tw_init, dft360_r30, 360, 30, 2, 1.0f)
/* DFT6 for radix-30 parents: the radix-2 outputs are scaled by 1/sqrt(15)
 * before the raw radix-3; the radix-5 that follows applies the remaining
 * 1/sqrt(2) before its sums (DFT_R30_RADIX5). This keeps every intermediate
 * value within sqrt(2) times the input rms while rounding stays well above
 * the LSB. */
static __attribute__((always_inline)) inline void dft6_pfa8_prescaled_q15_256(simde__m256i x0,
                                                                              simde__m256i x1,
                                                                              simde__m256i x2,
                                                                              simde__m256i x3,
                                                                              simde__m256i x4,
                                                                              simde__m256i x5,
                                                                              simde__m256i y[6],
                                                                              dft_dir_t dir,
                                                                              int16_t scale)
{
  const simde__m256i c = simde_mm256_set1_epi16(scale);
  const simde__m256i a0 = simde_mm256_mulhrs_epi16(simde_mm256_adds_epi16(x0, x3), c);
  const simde__m256i b0 = simde_mm256_mulhrs_epi16(simde_mm256_subs_epi16(x0, x3), c);
  const simde__m256i a1 = simde_mm256_mulhrs_epi16(simde_mm256_adds_epi16(x4, x1), c);
  const simde__m256i b1 = simde_mm256_mulhrs_epi16(simde_mm256_subs_epi16(x4, x1), c);
  const simde__m256i a2 = simde_mm256_mulhrs_epi16(simde_mm256_adds_epi16(x2, x5), c);
  const simde__m256i b2 = simde_mm256_mulhrs_epi16(simde_mm256_subs_epi16(x2, x5), c);
  radix3_butterfly8_q15_256(a0, a1, a2, &y[0], &y[2], &y[4], dir);
  radix3_butterfly8_q15_256(b0, b1, b2, &y[3], &y[5], &y[1], dir);
}

static __attribute__((always_inline)) inline void dft6_pfa8_r30_prescaled_q15_256(simde__m256i x0,
                                                                                  simde__m256i x1,
                                                                                  simde__m256i x2,
                                                                                  simde__m256i x3,
                                                                                  simde__m256i x4,
                                                                                  simde__m256i x5,
                                                                                  simde__m256i y[6],
                                                                                  dft_dir_t dir)
{
  dft6_pfa8_prescaled_q15_256(x0, x1, x2, x3, x4, x5, y, dir, Q15_INV_SQRT15);
}

/* Good-Thomas R30 input/output mappings, with layout-specific loads and stores. */
#define DFT_R30_FULL_INPUT(I) simde_mm256_loadu_si256((const simde__m256i *)(src + (I) * M + off))

#define DFT_R30_PARTIAL_INPUT(I) load_complex8_partial(src + (I) * M + off, valid)

#define DFT_R30_EIGHT_INPUT(I) load_complex8_partial(src + (I) * M + off, 8)

/* The DFT6 carries 1/sqrt(15) (see dft6_pfa8_r30_prescaled_q15_256) and the radix-5 the remaining
 * 1/sqrt(2): a raw radix-5 sums five values, which saturates from QAM rms 8192, so the scaling is
 * done before its sums and the parent twiddles have unit magnitude. */
#define DFT_R30_RADIX5(K, Z0, Z1, Z2, Z3, Z4)        \
  radix5_butterfly8_scaled_q15_256(Q15_INV_SQRT2,    \
                                   Q15_COS_2PI_5_S2, \
                                   Q15_COS_4PI_5_S2, \
                                   Q15_SIN_2PI_5_S2, \
                                   Q15_SIN_4PI_5_S2, \
                                   s[0][K],          \
                                   s[1][K],          \
                                   s[2][K],          \
                                   s[3][K],          \
                                   s[4][K],          \
                                   &(Z0),            \
                                   &(Z1),            \
                                   &(Z2),            \
                                   &(Z3),            \
                                   &(Z4),            \
                                   dir)
#define DFT_R30_LOAD_SIX(LOAD, ROW, I0, I1, I2, I3, I4, I5) \
  dft6_pfa8_r30_prescaled_q15_256(LOAD(I0), LOAD(I1), LOAD(I2), LOAD(I3), LOAD(I4), LOAD(I5), s[ROW], dir)

#define DFT_R30_INPUT_PERMUTATION(LOAD6) \
  LOAD6(0, 0, 25, 20, 15, 10, 5);        \
  LOAD6(1, 6, 1, 26, 21, 16, 11);        \
  LOAD6(2, 12, 7, 2, 27, 22, 17);        \
  LOAD6(3, 18, 13, 8, 3, 28, 23);        \
  LOAD6(4, 24, 19, 14, 9, 4, 29);

#define DFT_R30_OUTPUT_PERMUTATION(COMBINE5) \
  COMBINE5(0, 0, 6, 12, 18, 24);             \
  COMBINE5(1, 5, 11, 17, 23, 29);            \
  COMBINE5(2, 10, 16, 22, 28, 4);            \
  COMBINE5(3, 15, 21, 27, 3, 9);             \
  COMBINE5(4, 20, 26, 2, 8, 14);             \
  COMBINE5(5, 25, 1, 7, 13, 19);

#define DFT_R30_COMBINE_FIVE(K, STORE, B0, B1, B2, B3, B4) \
  do {                                                     \
    simde__m256i z0, z1, z2, z3, z4;                       \
    DFT_R30_RADIX5(K, z0, z1, z2, z3, z4);                 \
    STORE(B0, z0);                                         \
    STORE(B1, z1);                                         \
    STORE(B2, z2);                                         \
    STORE(B3, z3);                                         \
    STORE(B4, z4);                                         \
  } while (0)

/* Common R30 PFA path for parents ending with four valid samples. */
#define R30_TAIL_LOAD_6(C, I0, I1, I2, I3, I4, I5) DFT_R30_LOAD_SIX(DFT_R30_PARTIAL_INPUT, C, I0, I1, I2, I3, I4, I5)
#define R30_TAIL_SCALE_BRANCH(TAG, BR, Z)                                                                                     \
  do {                                                                                                                        \
    branch[(BR)] =                                                                                                            \
        (BR) == 0                                                                                                             \
            ? (Z)                                                                                                             \
            : complex_mul8_prepack_q15_256((Z), TAG##_re[off >> 3][(BR)], twiddle_im_dir_256(TAG##_im[off >> 3][(BR)], dir)); \
  } while (0)
#define R30_TAIL_COMBINE_5(TAG, K, B0, B1, B2, B3, B4) \
  do {                                                 \
    simde__m256i z0, z1, z2, z3, z4;                   \
    DFT_R30_RADIX5(K, z0, z1, z2, z3, z4);             \
    R30_TAIL_SCALE_BRANCH(TAG, B0, z0);                \
    R30_TAIL_SCALE_BRANCH(TAG, B1, z1);                \
    R30_TAIL_SCALE_BRANCH(TAG, B2, z2);                \
    R30_TAIL_SCALE_BRANCH(TAG, B3, z3);                \
    R30_TAIL_SCALE_BRANCH(TAG, B4, z4);                \
  } while (0)

#define DEFINE_R30_TAIL_STAGE(FUNCTION, TAG, MVAL)                       \
  static inline void FUNCTION(const c16_t *src, c16_t *b, dft_dir_t dir) \
  {                                                                      \
    enum { M = MVAL };                                                   \
                                                                         \
    for (int off = 0; off < M; off += 8) {                               \
      const int valid = M - off >= 8 ? 8 : 4;                            \
      simde__m256i s[5][6];                                              \
      simde__m256i branch[32];                                           \
                                                                         \
      DFT_R30_INPUT_PERMUTATION(R30_TAIL_LOAD_6)                         \
                                                                         \
      R30_TAIL_COMBINE_5(TAG, 0, 0, 6, 12, 18, 24);                      \
      R30_TAIL_COMBINE_5(TAG, 1, 5, 11, 17, 23, 29);                     \
      R30_TAIL_COMBINE_5(TAG, 2, 10, 16, 22, 28, 4);                     \
      R30_TAIL_COMBINE_5(TAG, 3, 15, 21, 27, 3, 9);                      \
      R30_TAIL_COMBINE_5(TAG, 4, 20, 26, 2, 8, 14);                      \
      R30_TAIL_COMBINE_5(TAG, 5, 25, 1, 7, 13, 19);                      \
                                                                         \
      branch[30] = simde_mm256_setzero_si256();                          \
      branch[31] = simde_mm256_setzero_si256();                          \
      store_r30_freqmajor_block32(b, off, valid, branch);                \
    }                                                                    \
  }

DEFINE_R30_TAIL_STAGE(radix30_stage_360, dft360_r30, 12)

/* DFT384: R24 = 3 x 8 Good-Thomas PFA, M=16. */
static simde__m256i dft384_r24_re[2][24] __attribute__((aligned(64)));
static simde__m256i dft384_r24_im[2][24] __attribute__((aligned(64)));
DEFINE_Q15_TABLE_INITIALIZER(
    dft384_r24_twiddles_init, const float sc = 1.0f / sqrtf(3.0f); for (int ds = 0; ds < 2; ds++) {
      const dft_dir_t dir = ds == 0 ? DFT_DIR_FORWARD : DFT_DIR_INVERSE;
      for (int bl = 0; bl < 2; bl++) {
        int off = 8 * bl;
        for (int br = 0; br < 24; br++) {
          dft384_r24_re[bl][br] = pack8_twiddle_q15_re_re_scaled(off, br, 384, sc);
          if (ds == 0)
            dft384_r24_im[bl][br] = pack8_twiddle_q15_im_signed_scaled(off, br, 384, sc, dir);
        }
      }
    })
/* Input vector x[j] of the radix-24 PFA input group ids[j] (24 rows of M bins) at column off. With pin, from the
 * NR layout of the pruned API: the swapped row of ids[j] is (ids[j] + 12) % 24 = ids[j ^ 4] in each group (the
 * callers unroll their loops, so the row and its half are constants). */
static __attribute__((always_inline)) inline simde__m256i r24_row_load8(const c16_t *src,
                                                                        const dft_band_t *pin,
                                                                        int M,
                                                                        const int ids[8],
                                                                        int j,
                                                                        int off)
{
  if (pin)
    return dft_band_load8(pin, 24 * M, ids[j ^ 4] * M + off, ids[j ^ 4] >= 12);
  return simde_mm256_loadu_si256((const simde__m256i *)(src + ids[j] * M + off));
}

static __attribute__((always_inline)) inline void radix24_pfa_stage_384(const c16_t *src,
                                                                        const dft_band_t *pin,
                                                                        c16_t *b,
                                                                        dft_dir_t dir)
{
  enum { M = 16 };
  static const int ids[3][8] = {{0, 9, 18, 3, 12, 21, 6, 15}, {16, 1, 10, 19, 4, 13, 22, 7}, {8, 17, 2, 11, 20, 5, 14, 23}};
  for (int off = 0; off < M; off += 8) {
    simde__m256i s[3][8];
#pragma GCC unroll 3
    for (int n3 = 0; n3 < 3; n3++) {
      simde__m256i x[8];
#pragma GCC unroll 8
      for (int j = 0; j < 8; j++)
        x[j] = r24_row_load8(src, pin, M, ids[n3], j, off);
      dft8x8_q15_256_unitary_dir(x[0],
                                 x[1],
                                 x[2],
                                 x[3],
                                 x[4],
                                 x[5],
                                 x[6],
                                 x[7],
                                 &s[n3][0],
                                 &s[n3][1],
                                 &s[n3][2],
                                 &s[n3][3],
                                 &s[n3][4],
                                 &s[n3][5],
                                 &s[n3][6],
                                 &s[n3][7],
                                 dir);
    }
    for (int k8 = 0; k8 < 8; k8++) {
      simde__m256i z0, z1, z2;
      radix3_butterfly8_q15_256(s[0][k8], s[1][k8], s[2][k8], &z0, &z1, &z2, dir);
      const int brs[3] = {(3 * k8) % 24, (8 + 3 * k8) % 24, (16 + 3 * k8) % 24};
      const simde__m256i zs[3] = {z0, z1, z2};
      for (int j = 0; j < 3; j++) {
        int br = brs[j];
        simde__m256i v = br == 0 ? simde_mm256_mulhrs_epi16(zs[j], simde_mm256_set1_epi16(Q15_INV_SQRT3))
                                 : complex_mul8_prepack_q15_256(zs[j],
                                                                dft384_r24_re[off >> 3][br],
                                                                twiddle_im_dir_256(dft384_r24_im[off >> 3][br], dir));
        simde_mm256_store_si256((simde__m256i *)(b + br * M + off), v);
      }
    }
  }
}
static void dft384_radix24_pfa(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { R = 24, M = 16 };
  c16_t b[R * M] __attribute__((aligned(64)));
  radix24_pfa_stage_384(src, NULL, b, dir);
  dft16x8_twiddle_scaled_store(b, M, dst, R, 0, 8, dir);
  dft16x8_twiddle_scaled_store(b, M, dst, R, 8, 8, dir);
  dft16x8_twiddle_scaled_store(b, M, dst, R, 16, 8, dir);
}

/* DFT432: R27 x DFT16. */

static simde__m256i dft432_r27_re[2][27] __attribute__((aligned(64))), dft432_r27_im[2][27] __attribute__((aligned(64)));
DEFINE_PACKED_PARENT_TWIDDLE_INIT(dft432_r27_tw_init, dft432_r27, 432, 27, 2, 1.0f)
DEFINE_PACKED_PFA_PARENT(radix27_stage_432, 16, 27, raw_radix27x8_input_q15_256, dft432_r27, 0)
/* Three full x8 groups and one explicitly sized tail, in natural branch order. */
#define DFT_FOUR_LEAF_BATCHES(LEAF, B, M, DST, R, TAIL) \
  LEAF(B, M, DST, R, 0, 8, dir);                        \
  LEAF(B, M, DST, R, 8, 8, dir);                        \
  LEAF(B, M, DST, R, 16, 8, dir);                       \
  LEAF(B, M, DST, R, 24, TAIL, dir);

static void dft432_radix27_leaf16(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { R = 27, M = 16 };
  c16_t b[R * M] __attribute__((aligned(64)));
  radix27_stage_432(src, b, dir);
  DFT_FOUR_LEAF_BATCHES(dft16x8_twiddle_scaled_store, b, M, dst, R, 3)
}

/* DFT600: R30 PFA parent + twiddle-scaled DFT20 leaves (1/sqrt20). */

static simde__m256i dft600_r30_re[3][30] __attribute__((aligned(64))), dft600_r30_im[3][30] __attribute__((aligned(64)));
DEFINE_PACKED_PARENT_TWIDDLE_INIT(dft600_r30_tw_init, dft600_r30, 600, 30, 3, 1.0f)
DEFINE_R30_TAIL_STAGE(radix30_stage_600, dft600_r30, 20)

#undef DEFINE_R30_TAIL_STAGE
#undef R30_TAIL_LOAD_6
#undef R30_TAIL_SCALE_BRANCH
#undef R30_TAIL_COMBINE_5

/* DFT720: R30 PFA parent + twiddle-scaled DFT24 leaves (1/sqrt24). */

static simde__m256i dft720_r30_re[3][30] __attribute__((aligned(64))), dft720_r30_im[3][30] __attribute__((aligned(64)));
DEFINE_PACKED_PARENT_TWIDDLE_INIT(dft720_r30_tw_init, dft720_r30, 720, 30, 3, 1.0f)
static inline void radix30_stage_720(const c16_t *src, c16_t *b, dft_dir_t dir)
{
  enum { M = 24, PITCH = 32 };

  for (int off = 0; off < M; off += 8) {
    simde__m256i s[5][6];
    simde__m256i branch[32];

#define DN(C, I0, I1, I2, I3, I4, I5) DFT_R30_LOAD_SIX(DFT_R30_EIGHT_INPUT, C, I0, I1, I2, I3, I4, I5)

    DFT_R30_INPUT_PERMUTATION(DN)
#undef DN

#define SN(BR, Z)                                                                                                    \
  do {                                                                                                               \
    branch[(BR)] = (BR) == 0 ? (Z)                                                                                   \
                             : complex_mul8_prepack_q15_256((Z),                                                     \
                                                            dft720_r30_re[off >> 3][(BR)],                           \
                                                            twiddle_im_dir_256(dft720_r30_im[off >> 3][(BR)], dir)); \
  } while (0)

#define FN(K, B0, B1, B2, B3, B4) DFT_R30_COMBINE_FIVE(K, SN, B0, B1, B2, B3, B4)

    DFT_R30_OUTPUT_PERMUTATION(FN)

#undef FN
#undef SN

    branch[30] = simde_mm256_setzero_si256();
    branch[31] = simde_mm256_setzero_si256();

    /*
     * branch[] is branch-major with eight frequency bins per vector.
     * Transpose once here so the DFT24x8 consumer sees eight consecutive
     * branches with ordinary AVX2 loads instead of i32gather.
     */
    for (int first = 0; first < 32; first += 8) {
      DFT_STORE_R30_FREQUENCY_TILE()
    }
  }
}

/* DFT768: R24 = 3 x 8 Good-Thomas PFA, M=32. */
static simde__m256i dft768_r24_re[4][24] __attribute__((aligned(64)));
static simde__m256i dft768_r24_im[4][24] __attribute__((aligned(64)));

DEFINE_Q15_TABLE_INITIALIZER(
    dft768_r24_twiddles_init, const float sc = 1.0f / sqrtf(3.0f); for (int ds = 0; ds < 2; ds++) {
      const dft_dir_t dir = ds == 0 ? DFT_DIR_FORWARD : DFT_DIR_INVERSE;
      for (int bl = 0; bl < 4; bl++) {
        const int off = 8 * bl;
        for (int br = 0; br < 24; br++) {
          dft768_r24_re[bl][br] = pack8_twiddle_q15_re_re_scaled(off, br, 768, sc);
          if (ds == 0)
            dft768_r24_im[bl][br] = pack8_twiddle_q15_im_signed_scaled(off, br, 768, sc, dir);
        }
      }
    })

static __attribute__((always_inline)) inline void radix24_pfa_stage_768(const c16_t *src,
                                                                        const dft_band_t *pin,
                                                                        c16_t *b,
                                                                        dft_dir_t dir)
{
  enum { M = 32 };
  static const int ids[3][8] = {
      {0, 9, 18, 3, 12, 21, 6, 15},
      {16, 1, 10, 19, 4, 13, 22, 7},
      {8, 17, 2, 11, 20, 5, 14, 23},
  };

  for (int off = 0; off < M; off += 8) {
    simde__m256i s[3][8];

#pragma GCC unroll 3
    for (int n3 = 0; n3 < 3; n3++) {
      simde__m256i x[8];
#pragma GCC unroll 8
      for (int j = 0; j < 8; j++)
        x[j] = r24_row_load8(src, pin, M, ids[n3], j, off);

      dft8x8_q15_256_unitary_dir(x[0],
                                 x[1],
                                 x[2],
                                 x[3],
                                 x[4],
                                 x[5],
                                 x[6],
                                 x[7],
                                 &s[n3][0],
                                 &s[n3][1],
                                 &s[n3][2],
                                 &s[n3][3],
                                 &s[n3][4],
                                 &s[n3][5],
                                 &s[n3][6],
                                 &s[n3][7],
                                 dir);
    }

#define STORE_R24_768_CONST(BR, Z)                                                              \
  do {                                                                                          \
    simde__m256i v;                                                                             \
    if ((BR) == 0)                                                                              \
      v = simde_mm256_mulhrs_epi16((Z), simde_mm256_set1_epi16(Q15_INV_SQRT3));                 \
    else                                                                                        \
      v = complex_mul8_prepack_q15_256((Z),                                                     \
                                       dft768_r24_re[off >> 3][(BR)],                           \
                                       twiddle_im_dir_256(dft768_r24_im[off >> 3][(BR)], dir)); \
    simde_mm256_store_si256((simde__m256i *)(b + (BR) * M + off), v);                           \
  } while (0)

#define R24_K8(K, B0, B1, B2)                                                       \
  do {                                                                              \
    simde__m256i z0, z1, z2;                                                        \
    radix3_butterfly8_q15_256(s[0][(K)], s[1][(K)], s[2][(K)], &z0, &z1, &z2, dir); \
    STORE_R24_768_CONST((B0), z0);                                                  \
    STORE_R24_768_CONST((B1), z1);                                                  \
    STORE_R24_768_CONST((B2), z2);                                                  \
  } while (0)

    R24_K8(0, 0, 8, 16);
    R24_K8(1, 3, 11, 19);
    R24_K8(2, 6, 14, 22);
    R24_K8(3, 9, 17, 1);
    R24_K8(4, 12, 20, 4);
    R24_K8(5, 15, 23, 7);
    R24_K8(6, 18, 2, 10);
    R24_K8(7, 21, 5, 13);

#undef R24_K8
#undef STORE_R24_768_CONST
  }
}

static void dft768_radix24_pfa_leaf32(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { R = 24, M = 32 };
  c16_t b[R * M] __attribute__((aligned(64)));

  radix24_pfa_stage_768(src, NULL, b, dir);

  dft32x8_twiddle_scaled_store(b, M, dst, R, 0, 8, dir);
  dft32x8_twiddle_scaled_store(b, M, dst, R, 8, 8, dir);
  dft32x8_twiddle_scaled_store(b, M, dst, R, 16, 8, dir);
}

/* DFT864: R27 x DFT32. */

static simde__m256i dft864_r27_re[4][27] __attribute__((aligned(64))), dft864_r27_im[4][27] __attribute__((aligned(64)));
DEFINE_PACKED_PARENT_TWIDDLE_INIT(dft864_r27_tw_init, dft864_r27, 864, 27, 4, 1.0f)
DEFINE_PACKED_PFA_PARENT(radix27_stage_864, 32, 27, raw_radix27x8_input_q15_256, dft864_r27, 0)
static void dft864_radix27_leaf32(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { R = 27, M = 32 };
  c16_t b[R * M] __attribute__((aligned(64)));
  radix27_stage_864(src, b, dir);
  DFT_FOUR_LEAF_BATCHES(dft32x8_twiddle_scaled_store, b, M, dst, R, 3)
}

/* DFT1536: R24 PFA, M=64. */
static __attribute__((always_inline)) inline void raw_radix24_pfa8_body(const c16_t *src,
                                                                        const dft_band_t *pin,
                                                                        int M,
                                                                        int off,
                                                                        simde__m256i y[24],
                                                                        dft_dir_t dir)
{
  static const int ids[3][8] = {{0, 9, 18, 3, 12, 21, 6, 15}, {16, 1, 10, 19, 4, 13, 22, 7}, {8, 17, 2, 11, 20, 5, 14, 23}};
  simde__m256i s[3][8];
#pragma GCC unroll 3
  for (int n = 0; n < 3; n++) {
    simde__m256i x[8];
#pragma GCC unroll 8
    for (int j = 0; j < 8; j++)
      x[j] = r24_row_load8(src, pin, M, ids[n], j, off);
    dft8x8_q15_256_unitary_dir(x[0],
                               x[1],
                               x[2],
                               x[3],
                               x[4],
                               x[5],
                               x[6],
                               x[7],
                               &s[n][0],
                               &s[n][1],
                               &s[n][2],
                               &s[n][3],
                               &s[n][4],
                               &s[n][5],
                               &s[n][6],
                               &s[n][7],
                               dir);
  }
  for (int k = 0; k < 8; k++) {
    simde__m256i z0, z1, z2;
    radix3_butterfly8_q15_256(s[0][k], s[1][k], s[2][k], &z0, &z1, &z2, dir);
    y[(3 * k) % 24] = z0;
    y[(8 + 3 * k) % 24] = z1;
    y[(16 + 3 * k) % 24] = z2;
  }
}

static inline void raw_radix24_pfa8_q15_256(const c16_t *src, int M, int off, simde__m256i y[24], dft_dir_t dir)
{
  raw_radix24_pfa8_body(src, NULL, M, off, y, dir);
}

static simde__m256i dft1536_r24_re[8][24] __attribute__((aligned(64))), dft1536_r24_im[8][24] __attribute__((aligned(64)));
DEFINE_PACKED_PARENT_TWIDDLE_INIT(dft1536_r24_tw_init, dft1536_r24, 1536, 24, 8, (float)3)
DEFINE_PACKED_PFA_PARENT(radix24_stage_1536, 64, 24, raw_radix24_pfa8_q15_256, dft1536_r24, Q15_INV_SQRT3)
/* radix24_stage_1536 reading the NR layout of the pruned API (inverse). */
static __attribute__((always_inline)) inline void radix24_stage_1536_band(const dft_band_t *pin, c16_t *b, dft_dir_t dir)
{
  enum { M = 64 };
  for (int off = 0; off < M; off += 8) {
    simde__m256i z[24];
    raw_radix24_pfa8_body(NULL, pin, M, off, z, dir);
    for (int br = 0; br < 24; br++) {
      const simde__m256i v = br == 0 ? simde_mm256_mulhrs_epi16(z[br], simde_mm256_set1_epi16(Q15_INV_SQRT3))
                                     : complex_mul8_prepack_q15_256(z[br],
                                                                    dft1536_r24_re[off >> 3][br],
                                                                    twiddle_im_dir_256(dft1536_r24_im[off >> 3][br], dir));
      simde_mm256_store_si256((simde__m256i *)(b + br * M + off), v);
    }
  }
}
/* dft64x8_branch_major_store with the packed output of the pruned API (forward). */
static __attribute__((always_inline)) inline void dft64x8_branch_major_store_band(const c16_t *b,
                                                                                  int M,
                                                                                  const dft_band_t *pout,
                                                                                  int R,
                                                                                  int first)
{
  /* local copy: the fields stay in registers (stores through c16_t * may alias *pout otherwise) */
  const dft_band_t band_local = *pout;
  pout = &band_local;
  simde__m256i leaf[64] __attribute__((aligned(64)));
  for (int n = 0; n < 64; n += 8) {
    simde__m256i z[8];
    for (int br = 0; br < 8; br++)
      z[br] = simde_mm256_loadu_si256((const simde__m256i *)(b + (first + br) * M + n));
    transpose8_complex_i16_256(&z[0], &z[1], &z[2], &z[3], &z[4], &z[5], &z[6], &z[7]);
    for (int l = 0; l < 8; l++)
      leaf[n + l] = z[l];
  }
  dft64x8_store_with_dc_band_body(leaf, pout->out, R, first, pout, DFT_DIR_FORWARD);
}

static void dft1536_radix24_pfa_leaf64(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { M = 64 };
  c16_t b[24 * M] __attribute__((aligned(64)));
  radix24_stage_1536(src, b, dir);
  dft64x8_branch_major_store(b, M, dst, 24, 0, dir);
  dft64x8_branch_major_store(b, M, dst, 24, 8, dir);
  dft64x8_branch_major_store(b, M, dst, 24, 16, dir);
}

static simde__m256i dft1728_r27_re[8][27] __attribute__((aligned(64))), dft1728_r27_im[8][27] __attribute__((aligned(64)));
DEFINE_PACKED_PARENT_TWIDDLE_INIT(dft1728_r27_tw_init, dft1728_r27, 1728, 27, 8, 1.0f)
DEFINE_PACKED_PFA_PARENT(radix27_stage_1728, 64, 27, raw_radix27x8_input_q15_256, dft1728_r27, 0)
static void dft1728_radix27_leaf64(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { R = 27, M = 64 };
  c16_t b[R * M] __attribute__((aligned(64)));
  radix27_stage_1728(src, b, dir);
  dft64x8_parent_store_from_branches(b, M, dst, R, 0, dir);
  dft64x8_parent_store_from_branches(b, M, dst, R, 8, dir);
  dft64x8_parent_store_from_branches(b, M, dst, R, 16, dir);
  c16_t tail[64] __attribute__((aligned(64)));
  for (int br = 24; br < R; br++) {
    dft64_avx(b + br * M, tail, dir);
    for (int k = 0; k < M; k++)
      dst[R * k + br] = tail[k];
  }
}

/* DFT960-only radix-30 AVX2 parent using a Good-Thomas 6 x 5 PFA.
 *
 * 30 = 6 x 5 with gcd(6,5)=1, so the parent needs no internal twiddle
 * multiplications between the radix-6 and radix-5 transforms.  The radix-6
 * itself is a Good-Thomas 2 x 3 transform, also twiddle-free.
 *
 * Input CRT mapping for the 6 x 5 PFA:
 *   n = 25*n6 + 6*n5 (mod 30)
 * Output mapping:
 *   k = 5*k6 + 6*k5 (mod 30)
 *
 * The DFT6 scales by 1/sqrt(15) after its radix-2 layer; the remaining
 * 1/sqrt(2) of the unitary 1/sqrt(30) is applied by the radix-5.
 */
static simde__m256i dft960_r30_pfa_re[4][30] __attribute__((aligned(64)));
static simde__m256i dft960_r30_pfa_im[4][30] __attribute__((aligned(64)));

DEFINE_Q15_TABLE_INITIALIZER(
    dft960_r30_pfa_twiddles_init, const float scale30 = 1.0f; for (int ds = 0; ds < 2; ds++) {
      const dft_dir_t dir = ds == 0 ? DFT_DIR_FORWARD : DFT_DIR_INVERSE;
      for (int block = 0; block < 4; block++) {
        const int off = 8 * block;
        for (int br = 0; br < 30; br++) {
          dft960_r30_pfa_re[block][br] = pack8_twiddle_q15_re_re_scaled(off, br, 960, scale30);
          if (ds == 0)
            dft960_r30_pfa_im[block][br] = pack8_twiddle_q15_im_signed_scaled(off, br, 960, scale30, dir);
        }
      }
    })

/* Unnormalized DFT6 using the Good-Thomas 2 x 3 decomposition.
 * Input: n = 3*n2 + 4*n3 (mod 6). Output: k = 3*k2 + 2*k3 (mod 6).
 * The radix-30 parent twiddles apply normalization after the radix-5 stage. */
static __attribute__((always_inline)) inline void dft6_pfa8_q15_256(simde__m256i x0,
                                                                    simde__m256i x1,
                                                                    simde__m256i x2,
                                                                    simde__m256i x3,
                                                                    simde__m256i x4,
                                                                    simde__m256i x5,
                                                                    simde__m256i y[6],
                                                                    dft_dir_t dir)
{
  const simde__m256i a0 = simde_mm256_adds_epi16(x0, x3);
  const simde__m256i b0 = simde_mm256_subs_epi16(x0, x3);
  const simde__m256i a1 = simde_mm256_adds_epi16(x4, x1);
  const simde__m256i b1 = simde_mm256_subs_epi16(x4, x1);
  const simde__m256i a2 = simde_mm256_adds_epi16(x2, x5);
  const simde__m256i b2 = simde_mm256_subs_epi16(x2, x5);

  simde__m256i e0, e1, e2, o0, o1, o2;
  radix3_butterfly8_q15_256(a0, a1, a2, &e0, &e1, &e2, dir);
  radix3_butterfly8_q15_256(b0, b1, b2, &o0, &o1, &o2, dir);

  /* k = 3*k2 + 2*k3 (mod 6). */
  y[0] = e0;
  y[2] = e1;
  y[4] = e2;
  y[3] = o0;
  y[5] = o1;
  y[1] = o2;
}

/* DFT480: R30 x DFT16. The parent uses the same 6 x 5 Good-Thomas mapping
 * and scaling as DFT960. */
static simde__m256i dft480_r30_re[2][30] __attribute__((aligned(64)));
static simde__m256i dft480_r30_im[2][30] __attribute__((aligned(64)));

DEFINE_Q15_TABLE_INITIALIZER(
    dft480_r30_twiddles_init, const float scale30 = 1.0f; for (int ds = 0; ds < 2; ds++) {
      const dft_dir_t dir = ds == 0 ? DFT_DIR_FORWARD : DFT_DIR_INVERSE;
      for (int block = 0; block < 2; block++) {
        const int off = 8 * block;
        for (int br = 0; br < 30; br++) {
          dft480_r30_re[block][br] = pack8_twiddle_q15_re_re_scaled(off, br, 480, scale30);
          if (ds == 0)
            dft480_r30_im[block][br] = pack8_twiddle_q15_im_signed_scaled(off, br, 480, scale30, dir);
        }
      }
    })

static inline void radix30_pfa_stage_to_branch_major_q15_256_480(const c16_t *src, c16_t *b, dft_dir_t dir)
{
  enum { M = 16 };
  for (int off = 0; off < M; off += 8) {
    simde__m256i s[5][6] __attribute__((aligned(64)));
#define R30_480_DFT6(C, I0, I1, I2, I3, I4, I5)                        \
  do {                                                                 \
    DFT_R30_LOAD_SIX(DFT_R30_FULL_INPUT, (C), I0, I1, I2, I3, I4, I5); \
  } while (0)
    DFT_R30_INPUT_PERMUTATION(R30_480_DFT6)
#undef R30_480_DFT6
#define R30_480_STORE(BR, Z)                                                                    \
  do {                                                                                          \
    simde__m256i v;                                                                             \
    if ((BR) == 0)                                                                              \
      v = (Z);                                                                                  \
    else                                                                                        \
      v = complex_mul8_prepack_q15_256((Z),                                                     \
                                       dft480_r30_re[off >> 3][(BR)],                           \
                                       twiddle_im_dir_256(dft480_r30_im[off >> 3][(BR)], dir)); \
    simde_mm256_store_si256((simde__m256i *)(b + (BR) * M + off), v);                           \
  } while (0)
#define R30_480_DFT5(K6, B0, B1, B2, B3, B4) DFT_R30_COMBINE_FIVE(K6, R30_480_STORE, B0, B1, B2, B3, B4)
    DFT_R30_OUTPUT_PERMUTATION(R30_480_DFT5)
#undef R30_480_DFT5
#undef R30_480_STORE
  }
}

static void dft480_radix30_pfa(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { R = 30, M = 16 };
  c16_t b[R * M] __attribute__((aligned(64)));
  radix30_pfa_stage_to_branch_major_q15_256_480(src, b, dir);
  DFT_FOUR_LEAF_BATCHES(dft16x8_twiddle_scaled_store, b, M, dst, R, 6)
}

/* DFT1920: R30 PFA, M=64. */
static simde__m256i dft1920_r30_re[8][30] __attribute__((aligned(64))), dft1920_r30_im[8][30] __attribute__((aligned(64)));
DEFINE_PACKED_PARENT_TWIDDLE_INIT(dft1920_r30_tw_init, dft1920_r30, 1920, 30, 8, 1.0f)

#undef DEFINE_PACKED_PARENT_TWIDDLE_INIT
static inline void radix30_stage_1920(const c16_t *src, c16_t *b, dft_dir_t dir)
{
  enum { M = 64 };
  for (int off = 0; off < M; off += 8) {
    simde__m256i s[5][6];
#define D619(C, I0, I1, I2, I3, I4, I5) DFT_R30_LOAD_SIX(DFT_R30_FULL_INPUT, C, I0, I1, I2, I3, I4, I5)
    DFT_R30_INPUT_PERMUTATION(D619)
#undef D619
#define R30_1920_STORE(BR, Z)                                                                                         \
  do {                                                                                                                \
    simde__m256i v = (BR) == 0 ? (Z)                                                                                  \
                               : complex_mul8_prepack_q15_256((Z),                                                    \
                                                              dft1920_r30_re[off >> 3][BR],                           \
                                                              twiddle_im_dir_256(dft1920_r30_im[off >> 3][BR], dir)); \
    simde_mm256_store_si256((simde__m256i *)(b + (BR) * M + off), v);                                                 \
  } while (0)
#define D519(K, B0, B1, B2, B3, B4) DFT_R30_COMBINE_FIVE(K, R30_1920_STORE, B0, B1, B2, B3, B4)
    DFT_R30_OUTPUT_PERMUTATION(D519)
#undef D519
#undef R30_1920_STORE
  }
}
static void dft1920_radix30_pfa(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { R = 30, M = 64 };
  c16_t b[R * M] __attribute__((aligned(64)));
  radix30_stage_1920(src, b, dir);
  dft64x8_parent_store_from_branches(b, M, dst, R, 0, dir);
  dft64x8_parent_store_from_branches(b, M, dst, R, 8, dir);
  dft64x8_parent_store_from_branches(b, M, dst, R, 16, dir);
  dft64x6_tail_store_1920(b, dst, dir);
}

static inline void radix30_pfa_stage_to_branch_major_q15_256_960(const c16_t *src, c16_t *b, dft_dir_t dir)
{
  enum { N = 960, R = 30, M = 32 };

  for (int off = 0; off < M; off += 8) {
    simde__m256i s[5][6] __attribute__((aligned(64)));

#define R30_PFA_DFT6(C, I0, I1, I2, I3, I4, I5)                        \
  do {                                                                 \
    DFT_R30_LOAD_SIX(DFT_R30_FULL_INPUT, (C), I0, I1, I2, I3, I4, I5); \
  } while (0)

    /* n = 25*n6 + 6*n5 (mod 30), n6 = 0..5. */
    DFT_R30_INPUT_PERMUTATION(R30_PFA_DFT6)
#undef R30_PFA_DFT6

#define R30_PFA_STORE(BR, Z)                                                                         \
  do {                                                                                               \
    simde__m256i _v;                                                                                 \
    if ((BR) == 0)                                                                                   \
      _v = (Z);                                                                                      \
    else                                                                                             \
      _v = complex_mul8_prepack_q15_256((Z),                                                         \
                                        dft960_r30_pfa_re[off >> 3][(BR)],                           \
                                        twiddle_im_dir_256(dft960_r30_pfa_im[off >> 3][(BR)], dir)); \
    simde_mm256_store_si256((simde__m256i *)(b + (BR) * M + off), _v);                               \
  } while (0)

#define R30_PFA_DFT5(K6, B0, B1, B2, B3, B4) DFT_R30_COMBINE_FIVE((K6), R30_PFA_STORE, (B0), (B1), (B2), (B3), (B4))

    /* k = 5*k6 + 6*k5 (mod 30), k5 = 0..4. */
    DFT_R30_OUTPUT_PERMUTATION(R30_PFA_DFT5)
#undef R30_PFA_DFT5
#undef R30_PFA_STORE
  }
}

static void dft960_radix30_pfa(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { R = 30, M = 32 };
  c16_t b[R * M] __attribute__((aligned(64)));

  radix30_pfa_stage_to_branch_major_q15_256_960(src, b, dir);

  DFT_FOUR_LEAF_BATCHES(dft32x8_twiddle_scaled_store, b, M, dst, R, 6)
}

static simde__m256i dft2160_r30_re[9][30] __attribute__((aligned(64)));
static simde__m256i dft2160_r30_im_fwd[9][30] __attribute__((aligned(64)));
static int16_t dft2160_r9_w72_re[2][24], dft2160_r9_w72_im_fwd[2][24];
static int16_t dft2160_r9_w24_re[2][8], dft2160_r9_w24_im_fwd[2][8];

static DFT_TABLE_BUILDER void dft2160_twiddles_init(void)
{
  const float s30 = 1.0f, s3 = 1.0f / sqrtf(3.0f);
  for (int block = 0; block < 9; block++)
    for (int br = 0; br < 30; br++) {
      dft2160_r30_re[block][br] = pack8_twiddle_q15_re_re_scaled(8 * block, br, 2160, s30);
      dft2160_r30_im_fwd[block][br] = pack8_twiddle_q15_im_signed_scaled(8 * block, br, 2160, s30, DFT_DIR_FORWARD);
    }
  for (int mul = 1; mul <= 2; mul++) {
    for (int k = 0; k < 24; k++) {
      const float a = 2.0f * (float)M_PI * (float)((mul * k) % 72) / 72.0f;
      dft2160_r9_w72_re[mul - 1][k] = q15_from_float(cosf(a) * s3);
      dft2160_r9_w72_im_fwd[mul - 1][k] = q15_from_float(-sinf(a) * s3);
    }
    for (int k = 0; k < 8; k++) {
      const float a = 2.0f * (float)M_PI * (float)((mul * k) % 24) / 24.0f;
      /* The inputs carry the second 1/sqrt(3), so W24 has unit magnitude. */
      dft2160_r9_w24_re[mul - 1][k] = q15_from_float(cosf(a));
      dft2160_r9_w24_im_fwd[mul - 1][k] = q15_from_float(-sinf(a));
    }
  }
}

static __attribute__((always_inline)) inline simde__m256i dft2160_r9_tw_mul(simde__m256i x,
                                                                            int16_t wr,
                                                                            int16_t wi_fwd,
                                                                            dft_dir_t dir)
{
  return complex_mul8_bcast_q15_256_parent(x, wr, twiddle_im_scalar_dir_i16(wi_fwd, dir));
}

static __attribute__((always_inline)) inline void dft2160_unitary_dft8x8(const simde__m256i x[8], simde__m256i y[8], dft_dir_t dir)
{
  simde__m256i a[4][2];
  DFT8_FOLDED_LEAF_STAGES(n)
}

#define DFT72_NORMALIZED_R9_STAGE()                                                                                 \
  for (int off = 0; off < 8; off++) {                                                                               \
    simde__m256i s[3][3];                                                                                           \
    for (int a = 0; a < 3; a++) {                                                                                   \
      simde__m256i z0, z1, z2;                                                                                      \
      radix3_butterfly8_q15_256(simde_mm256_mulhrs_epi16(x[(a + 0) * 8 + off], s3),                                 \
                                simde_mm256_mulhrs_epi16(x[(a + 3) * 8 + off], s3),                                 \
                                simde_mm256_mulhrs_epi16(x[(a + 6) * 8 + off], s3),                                 \
                                &z0,                                                                                \
                                &z1,                                                                                \
                                &z2,                                                                                \
                                dir);                                                                               \
      const int k = a * 8 + off;                                                                                    \
      s[a][0] = simde_mm256_mulhrs_epi16(z0, s3);                                                                   \
      s[a][1] = dft2160_r9_tw_mul(z1, dft2160_r9_w72_re[0][k], dft2160_r9_w72_im_fwd[0][k], dir);                   \
      s[a][2] = dft2160_r9_tw_mul(z2, dft2160_r9_w72_re[1][k], dft2160_r9_w72_im_fwd[1][k], dir);                   \
    }                                                                                                               \
    for (int beta = 0; beta < 3; beta++) {                                                                          \
      simde__m256i z0, z1, z2;                                                                                      \
      radix3_butterfly8_q15_256(s[0][beta], s[1][beta], s[2][beta], &z0, &z1, &z2, dir);                            \
      r9[beta + 0 * 3][off] = z0;                                                                                   \
      r9[beta + 1 * 3][off] = dft2160_r9_tw_mul(z1, dft2160_r9_w24_re[0][off], dft2160_r9_w24_im_fwd[0][off], dir); \
      r9[beta + 2 * 3][off] = dft2160_r9_tw_mul(z2, dft2160_r9_w24_re[1][off], dft2160_r9_w24_im_fwd[1][off], dir); \
    }                                                                                                               \
  }

static __attribute__((
    always_inline)) inline void dft2160_dft72x8_store_body(const c16_t *b, c16_t *dst, int first, int valid, dft_dir_t dir)
{
  simde__m256i x[72], r9[9][8];
  const simde__m256i s3 = simde_mm256_set1_epi16(Q15_INV_SQRT3);
  load_batched_leaf_inputs(b, 72, first, valid, 72, x);

  DFT72_NORMALIZED_R9_STAGE()

  for (int br9 = 0; br9 < 9; br9++) {
    simde__m256i y[8];
    dft2160_unitary_dft8x8(r9[br9], y, dir);
    for (int k8 = 0; k8 < 8; k8++)
      store_complex_lanes_256(dst + 30 * (9 * k8 + br9) + first, y[k8], valid);
  }
}

/* One copy per direction, so the direction is a constant inside. */
static __attribute__((noinline)) void dft2160_dft72x8_store(const c16_t *b, c16_t *dst, int first, int valid, dft_dir_t dir)
{
  if (dir == DFT_DIR_FORWARD)
    dft2160_dft72x8_store_body(b, dst, first, valid, DFT_DIR_FORWARD);
  else
    dft2160_dft72x8_store_body(b, dst, first, valid, DFT_DIR_INVERSE);
}

static inline void dft2160_radix30_stage(const c16_t *src, c16_t *b, dft_dir_t dir)
{
  enum { M = 72 };
  for (int off = 0; off < M; off += 8) {
    simde__m256i s[5][6];
#define DFT2160_DFT6(C, I0, I1, I2, I3, I4, I5) DFT_R30_LOAD_SIX(DFT_R30_FULL_INPUT, C, I0, I1, I2, I3, I4, I5)
    DFT_R30_INPUT_PERMUTATION(DFT2160_DFT6)
#undef DFT2160_DFT6
#define DFT2160_STORE(BR, Z)                                                                                        \
  do {                                                                                                              \
    const simde__m256i _im = twiddle_im_dir_256(dft2160_r30_im_fwd[off >> 3][BR], dir);                             \
    const simde__m256i _v = (BR) == 0 ? (Z) : complex_mul8_prepack_q15_256((Z), dft2160_r30_re[off >> 3][BR], _im); \
    simde_mm256_store_si256((simde__m256i *)(b + (BR) * M + off), _v);                                              \
  } while (0)
#define DFT2160_DFT5(K, B0, B1, B2, B3, B4) DFT_R30_COMBINE_FIVE(K, DFT2160_STORE, B0, B1, B2, B3, B4)
    DFT_R30_OUTPUT_PERMUTATION(DFT2160_DFT5)
#undef DFT2160_DFT5
#undef DFT2160_STORE
  }
}

static void dft2160_radix30_leaf72x8(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  c16_t b[2160] __attribute__((aligned(64)));
  dft2160_radix30_stage(src, b, dir);
  dft2160_dft72x8_store(b, dst, 0, 8, dir);
  dft2160_dft72x8_store(b, dst, 8, 8, dir);
  dft2160_dft72x8_store(b, dst, 16, 8, dir);
  dft2160_dft72x8_store(b, dst, 24, 6, dir);
}

static void dft48_radix8_pfa6(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  const int ds = dir == DFT_DIR_FORWARD ? 0 : 1;

  const simde__m256i x0 = simde_mm256_loadu_si256((const simde__m256i *)(src + 0));
  const simde__m256i x1 = simde_mm256_loadu_si256((const simde__m256i *)(src + 8));
  const simde__m256i x2 = simde_mm256_loadu_si256((const simde__m256i *)(src + 16));
  const simde__m256i x3 = simde_mm256_loadu_si256((const simde__m256i *)(src + 24));
  const simde__m256i x4 = simde_mm256_loadu_si256((const simde__m256i *)(src + 32));
  const simde__m256i x5 = simde_mm256_loadu_si256((const simde__m256i *)(src + 40));

  simde__m256i h[6];
  /* DFT6 / sqrt(3) on the transform input: two unitary radix-3 on unscaled inputs,
   * then a raw radix-2 (rms growth sqrt(2)); W48 carries the remaining 1/4. */
  {
    simde__m256i e0, e1, e2, f0, f1, f2;
    radix3_butterfly8_unitary_q15_256(x0, x4, x2, &e0, &e1, &e2, dir);
    radix3_butterfly8_unitary_q15_256(x3, x1, x5, &f0, &f1, &f2, dir);
    /* k = 3*k2 + 2*k3 (mod 6) */
    h[0] = simde_mm256_adds_epi16(e0, f0);
    h[2] = simde_mm256_adds_epi16(e1, f1);
    h[4] = simde_mm256_adds_epi16(e2, f2);
    h[3] = simde_mm256_subs_epi16(e0, f0);
    h[5] = simde_mm256_subs_epi16(e1, f1);
    h[1] = simde_mm256_subs_epi16(e2, f2);
  }

  h[0] = simde_mm256_mulhrs_epi16(h[0], n48_r4p12_scale8);
  for (int k2 = 1; k2 < 6; k2++)
    h[k2] = complex_mul8_prepack_small_q15_256(h[k2], n48_r8p6_re[k2], n48_r8p6_im[ds][k2]);

  simde__m256i r0 = h[0], r1 = h[1], r2 = h[2], r3 = h[3];
  simde__m256i r4 = h[4], r5 = h[5];
  simde__m256i r6 = simde_mm256_setzero_si256();
  simde__m256i r7 = simde_mm256_setzero_si256();
  transpose8_complex_i16_256(&r0, &r1, &r2, &r3, &r4, &r5, &r6, &r7);

  simde__m256i z0, z1, z2, z3, z4, z5, z6, z7;
  dft8x8_q15_256_dir(r0, r1, r2, r3, r4, r5, r6, r7, &z0, &z1, &z2, &z3, &z4, &z5, &z6, &z7, dir);

  /* Each z register contains six useful complex outputs followed by two dead
   * lanes.  Store z0..z6 as full 256-bit vectors at six-complex strides.
   * The two dead lanes intentionally overlap the first two destinations of
   * the following block and are overwritten by its full store.  The final
   * z7 block uses 128 + 64-bit stores to remain within dst[0..47]. */
  simde_mm256_storeu_si256((simde__m256i *)(dst + 0), z0);
  simde_mm256_storeu_si256((simde__m256i *)(dst + 6), z1);
  simde_mm256_storeu_si256((simde__m256i *)(dst + 12), z2);
  simde_mm256_storeu_si256((simde__m256i *)(dst + 18), z3);
  simde_mm256_storeu_si256((simde__m256i *)(dst + 24), z4);
  simde_mm256_storeu_si256((simde__m256i *)(dst + 30), z5);
  simde_mm256_storeu_si256((simde__m256i *)(dst + 36), z6);
  simde_mm_storeu_si128((simde__m128i *)(dst + 42), simde_mm256_castsi256_si128(z7));
  simde_mm_storel_epi64((simde__m128i *)(dst + 46), simde_mm256_extracti128_si256(z7, 1));
}

static void dft_radix3_pow2(const c16_t *src, c16_t *dst, int N, dft_dir_t dir)
{
  const int size = N / 3;
  const size_t need = 2u * (size_t)N + dft_power2_mixed_large_work_len(size);
  c16_t work[need] __attribute__((aligned(64))); /* up to 1 MB (N = 98304) */

  c16_t *sub = work;
  c16_t *tmp = work + N;
  c16_t *child_work = work + 2 * N;
  pack_radix3(src, sub, size);
  dft_power2_child(sub, tmp, size, dir, child_work);
  dft_power2_child(sub + size, tmp + size, size, dir, child_work);
  dft_power2_child(sub + 2 * size, tmp + 2 * size, size, dir, child_work);

  const r3_twiddle_t *tw = r3_twiddle_get(N);
  AssertFatal(tw, "Missing radix-3 twiddles N=%d\n", N);
  DFT_BIND_R3_TWIDDLES(tw, dir, w1r, w1i, w2r, w2i);

  for (int k = 0; k < size; k += 4) {
    const int b = k >> 2;
    simde__m128i y0, y1, y2;
    radix3_combine4_q15_128_fast(simde_mm_load_si128((const simde__m128i *)(tmp + k)),
                                 simde_mm_load_si128((const simde__m128i *)(tmp + size + k)),
                                 simde_mm_load_si128((const simde__m128i *)(tmp + 2 * size + k)),
                                 DFT_R3_COEFFICIENTS_128(w, b),
                                 &y0,
                                 &y1,
                                 &y2,
                                 dir);
    simde_mm_storeu_si128((simde__m128i *)(dst + k), y0);
    simde_mm_storeu_si128((simde__m128i *)(dst + size + k), y1);
    simde_mm_storeu_si128((simde__m128i *)(dst + 2 * size + k), y2);
  }
}

#define DFT_R5_ACCUMULATE_RIGHT(ADD, A, BC) ADD(BC, A)

DEFINE_RADIX5_COMBINE(radix5_combine4_q15_128_fast,
                      simde__m128i,
                      simde_mm_adds_epi16,
                      simde_mm_subs_epi16,
                      complex_mul4_prepack_small_q15_128,
                      q15_mul_i16_128,
                      mul_minus_j_scaled_dir_i16_128,
                      DFT_R5_ACCUMULATE_RIGHT,
                      14654,
                      10126,
                      -26510,
                      31163,
                      19260)

#define DFT_R5_SCALE_256(X, C) simde_mm256_mulhrs_epi16(X, simde_mm256_set1_epi16(C))

DEFINE_RADIX5_COMBINE(radix5_combine8_q15_256_fast,
                      simde__m256i,
                      simde_mm256_adds_epi16,
                      simde_mm256_subs_epi16,
                      complex_mul8_prepack_small_q15_256,
                      DFT_R5_SCALE_256,
                      mul_minus_j_scaled_dir_i16_256,
                      DFT_R5_ACCUMULATE_RIGHT,
                      Q15_INV_SQRT5,
                      Q15_COS_2PI_5,
                      Q15_COS_4PI_5,
                      Q15_SIN_2PI_5,
                      Q15_SIN_4PI_5)

/* Table aliases exist only in the calling function. */
#define DFT_BIND_R5_TWIDDLES(TABLE, DIR, R1, I1, R2, I2, R3, I3, R4, I4)                   \
  const simde__m128i *R1 = R5_W1_RE(TABLE);                                                \
  const simde__m128i *I1 = DIR == DFT_DIR_FORWARD ? R5_W1_IM(TABLE) : R5_W1_IM_INV(TABLE); \
  const simde__m128i *R2 = R5_W2_RE(TABLE);                                                \
  const simde__m128i *I2 = DIR == DFT_DIR_FORWARD ? R5_W2_IM(TABLE) : R5_W2_IM_INV(TABLE); \
  const simde__m128i *R3 = R5_W3_RE(TABLE);                                                \
  const simde__m128i *I3 = DIR == DFT_DIR_FORWARD ? R5_W3_IM(TABLE) : R5_W3_IM_INV(TABLE); \
  const simde__m128i *R4 = R5_W4_RE(TABLE);                                                \
  const simde__m128i *I4 = DIR == DFT_DIR_FORWARD ? R5_W4_IM(TABLE) : R5_W4_IM_INV(TABLE)

#define DFT_R5_COEFFICIENTS_128(P, INDEX) \
  P##1r [INDEX], P##1i [INDEX], P##2r [INDEX], P##2i [INDEX], P##3r [INDEX], P##3i [INDEX], P##4r [INDEX], P##4i [INDEX]

#define DFT_R5_COEFFICIENTS_256(P, INDEX)                                                                                         \
  simde_mm256_loadu_si256((const simde__m256i *)(P##1r + INDEX)), simde_mm256_loadu_si256((const simde__m256i *)(P##1i + INDEX)), \
      simde_mm256_loadu_si256((const simde__m256i *)(P##2r + INDEX)),                                                             \
      simde_mm256_loadu_si256((const simde__m256i *)(P##2i + INDEX)),                                                             \
      simde_mm256_loadu_si256((const simde__m256i *)(P##3r + INDEX)),                                                             \
      simde_mm256_loadu_si256((const simde__m256i *)(P##3i + INDEX)),                                                             \
      simde_mm256_loadu_si256((const simde__m256i *)(P##4r + INDEX)),                                                             \
      simde_mm256_loadu_si256((const simde__m256i *)(P##4i + INDEX))

static __attribute__((always_inline)) inline void radix5_combine_contiguous_avx2_body(const c16_t *src,
                                                                                      c16_t *dst,
                                                                                      int M,
                                                                                      const r5_twiddle_t *tw,
                                                                                      dft_dir_t dir)
{
  AssertFatal((M & 3) == 0, "Radix-5 combine requires M multiple of 4, got %d\n", M);

  DFT_BIND_R5_TWIDDLES(tw, dir, w1r, w1i, w2r, w2i, w3r, w3i, w4r, w4i);

  int k = 0;
  for (; k + 8 <= M; k += 8) {
    const int b = k >> 2;
    simde__m256i y0, y1, y2, y3, y4;
    radix5_combine8_q15_256_fast(simde_mm256_loadu_si256((const simde__m256i *)(src + 0 * M + k)),
                                 simde_mm256_loadu_si256((const simde__m256i *)(src + 1 * M + k)),
                                 simde_mm256_loadu_si256((const simde__m256i *)(src + 2 * M + k)),
                                 simde_mm256_loadu_si256((const simde__m256i *)(src + 3 * M + k)),
                                 simde_mm256_loadu_si256((const simde__m256i *)(src + 4 * M + k)),
                                 DFT_R5_COEFFICIENTS_256(w, b),
                                 &y0,
                                 &y1,
                                 &y2,
                                 &y3,
                                 &y4,
                                 dir);
    simde_mm256_storeu_si256((simde__m256i *)(dst + 0 * M + k), y0);
    simde_mm256_storeu_si256((simde__m256i *)(dst + 1 * M + k), y1);
    simde_mm256_storeu_si256((simde__m256i *)(dst + 2 * M + k), y2);
    simde_mm256_storeu_si256((simde__m256i *)(dst + 3 * M + k), y3);
    simde_mm256_storeu_si256((simde__m256i *)(dst + 4 * M + k), y4);
  }

  if (k < M) {
    AssertFatal(M - k == 4, "Unexpected radix-5 AVX2 tail M=%d k=%d\n", M, k);
    const int b = k >> 2;
    simde__m128i y0, y1, y2, y3, y4;
    radix5_combine4_q15_128_fast(simde_mm_loadu_si128((const simde__m128i *)(src + 0 * M + k)),
                                 simde_mm_loadu_si128((const simde__m128i *)(src + 1 * M + k)),
                                 simde_mm_loadu_si128((const simde__m128i *)(src + 2 * M + k)),
                                 simde_mm_loadu_si128((const simde__m128i *)(src + 3 * M + k)),
                                 simde_mm_loadu_si128((const simde__m128i *)(src + 4 * M + k)),
                                 DFT_R5_COEFFICIENTS_128(w, b),
                                 &y0,
                                 &y1,
                                 &y2,
                                 &y3,
                                 &y4,
                                 dir);
    simde_mm_storeu_si128((simde__m128i *)(dst + 0 * M + k), y0);
    simde_mm_storeu_si128((simde__m128i *)(dst + 1 * M + k), y1);
    simde_mm_storeu_si128((simde__m128i *)(dst + 2 * M + k), y2);
    simde_mm_storeu_si128((simde__m128i *)(dst + 3 * M + k), y3);
    simde_mm_storeu_si128((simde__m128i *)(dst + 4 * M + k), y4);
  }
}

/* One copy per direction, so the direction is a constant inside. */
static __attribute__((noinline)) void radix5_combine_contiguous_avx2(const c16_t *src,
                                                                     c16_t *dst,
                                                                     int M,
                                                                     const r5_twiddle_t *tw,
                                                                     dft_dir_t dir)
{
  if (dir == DFT_DIR_FORWARD)
    radix5_combine_contiguous_avx2_body(src, dst, M, tw, DFT_DIR_FORWARD);
  else
    radix5_combine_contiguous_avx2_body(src, dst, M, tw, DFT_DIR_INVERSE);
}

static __attribute__((always_inline)) inline void dft12x8_strided_raw_900(const c16_t *src,
                                                                          int sample_stride,
                                                                          int first,
                                                                          int valid,
                                                                          simde__m256i y[12],
                                                                          dft_dir_t dir)
{
  simde__m256i x[12], a[4][3];
  const simde__m256i mask = complex_lane_mask_epi32(valid);

  for (int n = 0; n < 12; n++) {
    const int *p = (const int *)(src + n * sample_stride + first);
    x[n] = valid == 8 ? simde_mm256_loadu_si256((const simde__m256i *)p) : simde_mm256_maskload_epi32(p, mask);
  }

  DFT_R3_FOLDED_LEAF_STAGE(4, 12, x[n1], x[n1 + 4], x[n1 + 8], dft12_leaf_twiddle_re, dft12_leaf_twiddle_im_fwd)

  DFT12_FOLDED_SECOND_STAGE(dft4x8_raw_q15_256)
}

static __attribute__((noinline)) void dft36x8_strided_to_branch_major_900(const c16_t *src,
                                                                          int input_stride,
                                                                          c16_t *leaf,
                                                                          int first,
                                                                          int valid,
                                                                          const r3_twiddle_t *tw,
                                                                          dft_dir_t dir)
{
  enum { M = 12, N = 36 };
  simde__m256i child[3][M];
  simde__m256i out[N];

  const int child_stride = input_stride * 3;
  dft12x8_strided_raw_900(src + 0 * input_stride, child_stride, first, valid, child[0], dir);
  dft12x8_strided_raw_900(src + 1 * input_stride, child_stride, first, valid, child[1], dir);
  dft12x8_strided_raw_900(src + 2 * input_stride, child_stride, first, valid, child[2], dir);

  DFT_BIND_R3_TWIDDLES(tw, dir, w1r, w1i, w2r, w2i);

  const int *w1r32 = (const int *)w1r;
  const int *w1i32 = (const int *)w1i;
  const int *w2r32 = (const int *)w2r;
  const int *w2i32 = (const int *)w2i;

  for (int k = 0; k < M; k++) {
    simde__m256i y0, y1, y2;
    const simde__m256i tw1r = simde_mm256_set1_epi32(w1r32[k]);
    const simde__m256i tw1i = simde_mm256_set1_epi32(w1i32[k]);
    const simde__m256i tw2r = simde_mm256_set1_epi32(w2r32[k]);
    const simde__m256i tw2i = simde_mm256_set1_epi32(w2i32[k]);

    radix3_combine8_q15_256_fast(child[0][k], child[1][k], child[2][k], tw1r, tw1i, tw2r, tw2i, &y0, &y1, &y2, dir);
    out[k + 0 * M] = y0;
    out[k + 1 * M] = y1;
    out[k + 2 * M] = y2;
  }

  /* 32 full output bins: transpose 8 frequencies x 8 transforms. */
  for (int off = 0; off < 32; off += 8) {
    DFT_LOAD_TRANSPOSE8_ROW(z, out, off)
    const simde__m256i rows[8] = {z0, z1, z2, z3, z4, z5, z6, z7};

    for (int lane = 0; lane < valid; lane++)
      simde_mm256_storeu_si256((simde__m256i *)(leaf + (first + lane) * N + off), rows[lane]);
  }

  /* Final bins 32..35: transpose 4 frequencies x 8 transforms. */
  {
    simde__m128i lo0 = simde_mm256_castsi256_si128(out[32]);
    simde__m128i lo1 = simde_mm256_castsi256_si128(out[33]);
    simde__m128i lo2 = simde_mm256_castsi256_si128(out[34]);
    simde__m128i lo3 = simde_mm256_castsi256_si128(out[35]);
    transpose4_complex_i16_128(&lo0, &lo1, &lo2, &lo3);
    const simde__m128i lo_rows[4] = {lo0, lo1, lo2, lo3};

    for (int lane = 0; lane < valid && lane < 4; lane++)
      simde_mm_storeu_si128((simde__m128i *)(leaf + (first + lane) * N + 32), lo_rows[lane]);

    if (valid > 4) {
      simde__m128i hi0 = simde_mm256_extracti128_si256(out[32], 1);
      simde__m128i hi1 = simde_mm256_extracti128_si256(out[33], 1);
      simde__m128i hi2 = simde_mm256_extracti128_si256(out[34], 1);
      simde__m128i hi3 = simde_mm256_extracti128_si256(out[35], 1);
      transpose4_complex_i16_128(&hi0, &hi1, &hi2, &hi3);
      const simde__m128i hi_rows[4] = {hi0, hi1, hi2, hi3};

      for (int lane = 4; lane < valid; lane++)
        simde_mm_storeu_si128((simde__m128i *)(leaf + (first + lane) * N + 32), hi_rows[lane - 4]);
    }
  }
}

#define DEFINE_STRIDED_DFT12_PARENT(FN, RADIX, MESSAGE, ...)                                                                     \
  static inline void FN(const c16_t *src, int stride, c16_t *dst, dft_dir_t dir)                                                 \
  {                                                                                                                              \
    enum { R = RADIX, M = 12, N = R * M };                                                                                       \
    c16_t tmp[N] __attribute__((aligned(64)));                                                                                   \
    const int child_stride = stride * R;                                                                                         \
                                                                                                                                 \
    dft12_q15_256_batch2_strided(src + 0 * stride, child_stride, tmp + 0 * M, src + 1 * stride, child_stride, tmp + 1 * M, dir); \
    __VA_ARGS__                                                                                                                  \
                                                                                                                                 \
    const r##RADIX##_twiddle_t *tw = r##RADIX##_twiddle_get(N);                                                                  \
    AssertFatal(tw, MESSAGE);                                                                                                    \
    radix##RADIX##_combine_contiguous_avx2(tmp, dst, M, tw, dir);                                                                \
  }

DEFINE_STRIDED_DFT12_PARENT(dft36_radix3_leaf12_strided,
                            3,
                            "Missing DFT36 radix-3 twiddles\n",
                            dft12_q15_128_strided(src + 2 * stride, child_stride, tmp + 2 * M, dir);)

DEFINE_DFT4_LEAF_STORE(dft36_leaf4x4_store, R = 9, M = 4)

static void dft36_radix9_leaf4(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { N = 36, R = 9 };
  c16_t branches[N] __attribute__((aligned(64)));

  const r3_twiddle_t *tw36 = r3_twiddle_get(36);
  const r3_twiddle_t *tw12 = r3_twiddle_get(12);
  AssertFatal(tw36 && tw12, "Missing N=36 radix-9 twiddles\n");

  radix9_stage_to_branch_major_q15_128_plan(src, branches, N, dir, tw36, tw12);

  dft36_leaf4x4_store(branches, dst, 0, 4, dir);
  dft36_leaf4x4_store(branches, dst, 4, 4, dir);
  dft36_leaf4x4_store(branches, dst, 8, 1, dir);
}

DEFINE_STRIDED_DFT12_PARENT(
    dft60_radix5_leaf12_strided,
    5,
    "Missing DFT60 radix-5 twiddles\n",
    dft12_q15_256_batch2_strided(src + 2 * stride, child_stride, tmp + 2 * M, src + 3 * stride, child_stride, tmp + 3 * M, dir);
    dft12_q15_128_strided(src + 4 * stride, child_stride, tmp + 4 * M, dir);)

static simde__m256i dft60_r15p_re[15] __attribute__((aligned(64)));
static simde__m256i dft60_r15p_im[15] __attribute__((aligned(64)));

DEFINE_Q15_TABLE_INITIALIZER(
    dft60_r15p_tw_init, const float sc = 1.0f; const simde__m128i zero = simde_mm_setzero_si128();

    for (int ds = 0; ds < 2; ds++) {
      const dft_dir_t dir = ds == 0 ? DFT_DIR_FORWARD : DFT_DIR_INVERSE;
      for (int br = 0; br < 15; br++) {
        const simde__m128i re4 = pack4_twiddle_q15_re_re_scaled(0, br, 60, sc);
        const simde__m128i im4 = pack4_twiddle_q15_im_signed_scaled(0, br, 60, sc, dir);
        dft60_r15p_re[br] = simde_mm256_inserti128_si256(simde_mm256_castsi128_si256(re4), zero, 1);
        if (ds == 0)
          dft60_r15p_im[br] = simde_mm256_inserti128_si256(simde_mm256_castsi128_si256(im4), zero, 1);
      }
    })

#define DFT_R15_PARTIAL_STAGE(ZERO, ADDRESS)                                                                                     \
  static const int ids[3][5] = {                                                                                                 \
      {0, 6, 12, 3, 9},                                                                                                          \
      {10, 1, 7, 13, 4},                                                                                                         \
      {5, 11, 2, 8, 14},                                                                                                         \
  };                                                                                                                             \
                                                                                                                                 \
  const simde__m128i ZERO = simde_mm_setzero_si128();                                                                            \
  simde__m256i s[3][5];                                                                                                          \
                                                                                                                                 \
  for (int n3 = 0; n3 < 3; n3++) {                                                                                               \
    simde__m256i x[5];                                                                                                           \
    for (int j = 0; j < 5; j++) {                                                                                                \
      const simde__m128i lo = simde_mm_loadu_si128((const simde__m128i *)(ADDRESS));                                             \
      x[j] = simde_mm256_inserti128_si256(simde_mm256_castsi128_si256(lo), ZERO, 1);                                             \
    }                                                                                                                            \
                                                                                                                                 \
    radix5_butterfly8_unitary_q15_256(x[0], x[1], x[2], x[3], x[4], &s[n3][0], &s[n3][1], &s[n3][2], &s[n3][3], &s[n3][4], dir); \
  }                                                                                                                              \
                                                                                                                                 \
  for (int n3 = 0; n3 < 3; n3++)                                                                                                 \
    for (int k5 = 0; k5 < 5; k5++)                                                                                               \
      s[n3][k5] = simde_mm256_mulhrs_epi16(s[n3][k5], simde_mm256_set1_epi16(Q15_INV_SQRT3));                                    \
  for (int k5 = 0; k5 < 5; k5++) {                                                                                               \
    simde__m256i z0, z1, z2;                                                                                                     \
    radix3_butterfly8_q15_256(s[0][k5], s[1][k5], s[2][k5], &z0, &z1, &z2, dir);                                                 \
    y[(3 * k5) % 15] = z0;                                                                                                       \
    y[(5 + 3 * k5) % 15] = z1;                                                                                                   \
    y[(10 + 3 * k5) % 15] = z2;                                                                                                  \
  }

static __attribute__((always_inline)) inline void dft60_r15_pfa_raw4(const c16_t *src, simde__m256i y[15], dft_dir_t dir)
{
  enum { M = 4 };
  DFT_R15_PARTIAL_STAGE(zero, src + ids[n3][j] * M)
}

static __attribute__((always_inline)) inline void dft60_leaf4x4_store_body(const c16_t *b,
                                                                           c16_t *dst,
                                                                           int first,
                                                                           int valid,
                                                                           dft_dir_t dir)
{
  const simde__m128i zero = simde_mm_setzero_si128();
  simde__m128i r0 = valid > 0 ? simde_mm_loadu_si128((const simde__m128i *)(b + (first + 0) * 4)) : zero;
  simde__m128i r1 = valid > 1 ? simde_mm_loadu_si128((const simde__m128i *)(b + (first + 1) * 4)) : zero;
  simde__m128i r2 = valid > 2 ? simde_mm_loadu_si128((const simde__m128i *)(b + (first + 2) * 4)) : zero;
  simde__m128i r3 = valid > 3 ? simde_mm_loadu_si128((const simde__m128i *)(b + (first + 3) * 4)) : zero;

  transpose4_complex_i16_128(&r0, &r1, &r2, &r3);

  simde__m128i y0, y1, y2, y3;
  dft4x4_q15_128(r0, r1, r2, r3, &y0, &y1, &y2, &y3, dir);

  const simde__m128i y[4] = {y0, y1, y2, y3};
  for (int k = 0; k < 4; k++) {
    if (valid == 4) {
      simde_mm_storeu_si128((simde__m128i *)(dst + 15 * k + first), y[k]);
    } else {
      c16_t lane[4] __attribute__((aligned(16)));
      simde_mm_store_si128((simde__m128i *)lane, y[k]);
      for (int l = 0; l < valid; l++)
        dst[15 * k + first + l] = lane[l];
    }
  }
}

/* One copy per direction, so the direction is a constant inside. */
static __attribute__((noinline)) void dft60_leaf4x4_store(const c16_t *b, c16_t *dst, int first, int valid, dft_dir_t dir)
{
  if (dir == DFT_DIR_FORWARD)
    dft60_leaf4x4_store_body(b, dst, first, valid, DFT_DIR_FORWARD);
  else
    dft60_leaf4x4_store_body(b, dst, first, valid, DFT_DIR_INVERSE);
}

static void dft60_radix15_pfa_leaf4(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { R = 15, M = 4, N = R * M };
  c16_t b[N] __attribute__((aligned(64)));
  simde__m256i z[R];

  dft60_r15_pfa_raw4(src, z, dir);

  for (int br = 0; br < R; br++) {
    const simde__m256i v =
        br == 0 ? z[br] : complex_mul8_prepack_q15_256(z[br], dft60_r15p_re[br], twiddle_im_dir_256(dft60_r15p_im[br], dir));
    simde_mm_storeu_si128((simde__m128i *)(b + br * M), simde_mm256_castsi256_si128(v));
  }

  dft60_leaf4x4_store(b, dst, 0, 4, dir);
  dft60_leaf4x4_store(b, dst, 4, 4, dir);
  dft60_leaf4x4_store(b, dst, 8, 4, dir);
  dft60_leaf4x4_store(b, dst, 12, 3, dir);
}

static simde__m256i dft180_r15p_re[2][15] __attribute__((aligned(64)));
static simde__m256i dft180_r15p_im[2][15] __attribute__((aligned(64)));

DEFINE_Q15_TABLE_INITIALIZER(
    dft180_r15p_tw_init, const float sc = 1.0f; for (int ds = 0; ds < 2; ds++) {
      const dft_dir_t dir = ds == 0 ? DFT_DIR_FORWARD : DFT_DIR_INVERSE;
      for (int br = 0; br < 15; br++) {
        dft180_r15p_re[0][br] = pack8_twiddle_q15_re_re_scaled(0, br, 180, sc);
        if (ds == 0)
          dft180_r15p_im[0][br] = pack8_twiddle_q15_im_signed_scaled(0, br, 180, sc, dir);

        const simde__m128i re4 = pack4_twiddle_q15_re_re_scaled(8, br, 180, sc);
        const simde__m128i im4 = pack4_twiddle_q15_im_signed_scaled(8, br, 180, sc, dir);
        dft180_r15p_re[1][br] = simde_mm256_inserti128_si256(simde_mm256_castsi128_si256(re4), simde_mm_setzero_si128(), 1);
        if (ds == 0)
          dft180_r15p_im[1][br] = simde_mm256_inserti128_si256(simde_mm256_castsi128_si256(im4), simde_mm_setzero_si128(), 1);
      }
    })

static __attribute__((always_inline)) inline void dft180_r15_raw_block4(const c16_t *src, simde__m256i y[15], dft_dir_t dir)
{
  enum { M = 12, OFF = 8 };
  DFT_R15_PARTIAL_STAGE(zero128, src + ids[n3][j] * M + OFF)
}

static inline void dft180_radix15_pfa_stage(const c16_t *src, c16_t *b, dft_dir_t dir)
{
  enum { M = 12 };

  {
    simde__m256i z[15];
    r15_pfa_raw_block_n120(src, M, 0, z, dir);
    for (int br = 0; br < 15; br++) {
      const simde__m256i v =
          br == 0 ? z[br]
                  : complex_mul8_prepack_q15_256(z[br], dft180_r15p_re[0][br], twiddle_im_dir_256(dft180_r15p_im[0][br], dir));
      simde_mm256_storeu_si256((simde__m256i *)(b + br * M), v);
    }
  }

  {
    simde__m256i z[15];
    dft180_r15_raw_block4(src, z, dir);
    for (int br = 0; br < 15; br++) {
      const simde__m256i v =
          br == 0 ? z[br]
                  : complex_mul8_prepack_q15_256(z[br], dft180_r15p_re[1][br], twiddle_im_dir_256(dft180_r15p_im[1][br], dir));
      simde_mm_storeu_si128((simde__m128i *)(b + br * M + 8), simde_mm256_castsi256_si128(v));
    }
  }
}

static void dft180_radix15_pfa_leaf12(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { R = 15, M = 12 };
  c16_t b[R * M] __attribute__((aligned(64)));

  dft180_radix15_pfa_stage(src, b, dir);

  dft12x8_twiddle_scaled_store(b, M, dst, R, 0, 8, dir);
  dft12x8_twiddle_scaled_store(b, M, dst, R, 8, 7, dir);
}

/* DFT3240 = radix-5 x DFT648. Each child is packed, then run through the radix-81 DFT648 kernel. */
static void dft3240_radix5_leaf648(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { R = 5, M = 648, N = R * M };
  c16_t tmp[2 * N] __attribute__((aligned(64)));
  c16_t *packed = tmp;
  c16_t *leaf = tmp + N;

  radix5_pack_children_nogather_generic(src, packed, M);
  for (int br = 0; br < R; br++)
    radix81_terminal_leaf8(packed + br * M, leaf + br * M, dir, 0);

  const r5_twiddle_t *tw = r5_twiddle_get(N);
  AssertFatal(tw, "Missing radix-5 twiddles N=%d\n", N);
  radix5_combine_contiguous_avx2(leaf, dst, M, tw, dir);
}

static void dft540_r5_leaf108_pair_batch2(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { R5 = 5, N108 = 108, R9 = 9, M12 = 12, N = R5 * N108 };

  c16_t work[3 * N + 8] __attribute__((aligned(64)));
  c16_t *packed = work;
  c16_t *stage = work + N;

  c16_t *leaf = work + 2 * N + 8;

  const r3_twiddle_t *twB = r3_twiddle_get(N108);
  const r3_twiddle_t *twA = r3_twiddle_get(N108 / 3);
  AssertFatal(twB && twA, "Missing radix-9 twiddles N=%d\n", N108);

  radix5_pack_children_nogather_540(src, packed);

  radix9_stage_head_q15_256_108(packed + 0 * N108, stage + 0 * N108, dir, twB, twA);
  radix9_stage_head_q15_256_108(packed + 1 * N108, stage + 1 * N108, dir, twB, twA);
  radix9_stage_tail_q15_256_pair_108(packed + 0 * N108, stage + 0 * N108, packed + 1 * N108, stage + 1 * N108, dir, twB, twA);

  radix9_stage_head_q15_256_108(packed + 2 * N108, stage + 2 * N108, dir, twB, twA);
  radix9_stage_head_q15_256_108(packed + 3 * N108, stage + 3 * N108, dir, twB, twA);
  radix9_stage_tail_q15_256_pair_108(packed + 2 * N108, stage + 2 * N108, packed + 3 * N108, stage + 3 * N108, dir, twB, twA);

  /* The fifth transform has no partner for its 4-sample stage tail. */
  radix9_stage_to_branch_major_q15_256_128_hybrid_108(packed + 4 * N108, stage + 4 * N108, dir, twB, twA, 0);

  /* Branches 0..7 are already a full x8 batch inside each DFT108. */
  for (int t = 0; t < R5; t++)
    dft12x8_twiddle_scaled_store(stage + t * N108, M12, leaf + t * N108, R9, 0, 8, dir);

  /*
   * Branch 8 is the only width-limited DFT12 in every DFT108.
   * The five stage buffers are contiguous and exactly N108 samples apart,
   * so one DFT12x8 call can evaluate the five transforms as five SIMD lanes.
   */

  c16_t tail[R5 * M12] __attribute__((aligned(64)));
  dft12x8_twiddle_scaled_store(stage + 8 * M12, N108, tail, R5, 0, R5, dir);

  for (int k = 0; k < M12; k++)
    for (int t = 0; t < R5; t++)
      leaf[t * N108 + R9 * k + 8] = tail[R5 * k + t];

  const r5_twiddle_t *tw = r5_twiddle_get(N);
  AssertFatal(tw, "Missing DFT540 radix-5 twiddles\n");
  radix5_combine_contiguous_avx2(leaf, dst, N108, tw, dir);
}

static void dft2880_r5_leaf576(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { R = 5, M = 576, N = R * M };
  c16_t tmp[N] __attribute__((aligned(64)));

  radix5_pack_children_nogather_generic(src, tmp, M);
  for (int br = 0; br < R; br++)
    dft_radix9_or_radix18(tmp + br * M, tmp + br * M, M, dir, 0);

  const r5_twiddle_t *tw = r5_twiddle_get(N);
  AssertFatal(tw, "Missing DFT2880 radix-5 twiddles\n");
  DFT_BIND_R5_TWIDDLES(tw, dir, w1r, w1i, w2r, w2i, w3r, w3i, w4r, w4i);

  for (int k = 0; k < M; k += 8) {
    const int b = k >> 2;
    simde__m256i y0, y1, y2, y3, y4;
    radix5_combine8_q15_256_fast(simde_mm256_load_si256((const simde__m256i *)(tmp + 0 * M + k)),
                                 simde_mm256_load_si256((const simde__m256i *)(tmp + 1 * M + k)),
                                 simde_mm256_load_si256((const simde__m256i *)(tmp + 2 * M + k)),
                                 simde_mm256_load_si256((const simde__m256i *)(tmp + 3 * M + k)),
                                 simde_mm256_load_si256((const simde__m256i *)(tmp + 4 * M + k)),
                                 simde_mm256_load_si256((const simde__m256i *)(w1r + b)),
                                 simde_mm256_load_si256((const simde__m256i *)(w1i + b)),
                                 simde_mm256_load_si256((const simde__m256i *)(w2r + b)),
                                 simde_mm256_load_si256((const simde__m256i *)(w2i + b)),
                                 simde_mm256_load_si256((const simde__m256i *)(w3r + b)),
                                 simde_mm256_load_si256((const simde__m256i *)(w3i + b)),
                                 simde_mm256_load_si256((const simde__m256i *)(w4r + b)),
                                 simde_mm256_load_si256((const simde__m256i *)(w4i + b)),
                                 &y0,
                                 &y1,
                                 &y2,
                                 &y3,
                                 &y4,
                                 dir);
    simde_mm256_storeu_si256((simde__m256i *)(dst + 0 * M + k), y0);
    simde_mm256_storeu_si256((simde__m256i *)(dst + 1 * M + k), y1);
    simde_mm256_storeu_si256((simde__m256i *)(dst + 2 * M + k), y2);
    simde_mm256_storeu_si256((simde__m256i *)(dst + 3 * M + k), y3);
    simde_mm256_storeu_si256((simde__m256i *)(dst + 4 * M + k), y4);
  }
}

/* Fuse the two radix-5 levels that the recursive x86 path would execute for
 * N=25*M.  The 25 terminal DFT_M results are materialized once.  For each
 * group of eight frequency bins, the intermediate 5x5 radix result stays in a
 * small local vector tile and feeds the second radix immediately, avoiding a
 * full-N intermediate store/reload between the two radix-5 stages. */

#define DFT_R25_LOAD_NONE(POS)

#define DFT_R25_INPUT_VECTORS(POS) leafv[r0 + 0 * 5], leafv[r0 + 1 * 5], leafv[r0 + 2 * 5], leafv[r0 + 3 * 5], leafv[r0 + 4 * 5]

#define DFT_R25_INPUT_TAIL(POS) tailv[r0 + 0 * 5], tailv[r0 + 1 * 5], tailv[r0 + 2 * 5], tailv[r0 + 3 * 5], tailv[r0 + 4 * 5]

/* One R5 x R5 tile, specialized at preprocessing time for each leaf layout. */
#define DFT_RADIX25_TILE(VEC, COMBINE, LOAD_INPUTS, INPUTS, COEFFICIENTS, STORE, POS) \
  VEC stage1[5][5];                                                                   \
  const int ib = POS >> 2;                                                            \
  for (int r0 = 0; r0 < 5; r0++) {                                                    \
    LOAD_INPUTS(POS)                                                                  \
    COMBINE(INPUTS(POS),                                                              \
            COEFFICIENTS(in_, ib),                                                    \
            &stage1[r0][0],                                                           \
            &stage1[r0][1],                                                           \
            &stage1[r0][2],                                                           \
            &stage1[r0][3],                                                           \
            &stage1[r0][4],                                                           \
            dir);                                                                     \
  }                                                                                   \
  for (int j1 = 0; j1 < 5; j1++) {                                                    \
    const int outer_k = j1 * M + POS;                                                 \
    const int ob = outer_k >> 2;                                                      \
    VEC y0, y1, y2, y3, y4;                                                           \
    COMBINE(stage1[0][j1],                                                            \
            stage1[1][j1],                                                            \
            stage1[2][j1],                                                            \
            stage1[3][j1],                                                            \
            stage1[4][j1],                                                            \
            COEFFICIENTS(out_, ob),                                                   \
            &y0,                                                                      \
            &y1,                                                                      \
            &y2,                                                                      \
            &y3,                                                                      \
            &y4,                                                                      \
            dir);                                                                     \
    STORE((VEC *)(dst + (0 * 5 + j1) * M + POS), y0);                                 \
    STORE((VEC *)(dst + (1 * 5 + j1) * M + POS), y1);                                 \
    STORE((VEC *)(dst + (2 * 5 + j1) * M + POS), y2);                                 \
    STORE((VEC *)(dst + (3 * 5 + j1) * M + POS), y3);                                 \
    STORE((VEC *)(dst + (4 * 5 + j1) * M + POS), y4);                                 \
  }

static __attribute__((noinline)) void dft300_radix25_dft12x8_block_major(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { N = 300, M = 12, R = 25 };
  simde__m256i leafv[R] __attribute__((aligned(64)));
  simde__m128i tailv[R] __attribute__((aligned(64)));

  dft12x8_strided_to_block_major_300(src, R, leafv, tailv, 0, 8, dir);
  dft12x8_strided_to_block_major_300(src, R, leafv, tailv, 8, 8, dir);
  dft12x8_strided_to_block_major_300(src, R, leafv, tailv, 16, 8, dir);
  dft12x8_strided_to_block_major_300(src, R, leafv, tailv, 24, 1, dir);

  const r5_twiddle_t *tw_inner = r5_twiddle_get(N / 5);
  const r5_twiddle_t *tw_outer = r5_twiddle_get(N);
  AssertFatal(tw_inner && tw_outer, "Missing radix-25 twiddles N=%d\n", N);

#define DFT300_R5_TW(TW, PREFIX) \
  DFT_BIND_R5_TWIDDLES((TW), dir, PREFIX##1r, PREFIX##1i, PREFIX##2r, PREFIX##2i, PREFIX##3r, PREFIX##3i, PREFIX##4r, PREFIX##4i)

  DFT300_R5_TW(tw_inner, in_);
  DFT300_R5_TW(tw_outer, out_);

#undef DFT300_R5_TW

  {
    enum { K = 0 };
    DFT_RADIX25_TILE(simde__m256i,
                     radix5_combine8_q15_256_fast,
                     DFT_R25_LOAD_NONE,
                     DFT_R25_INPUT_VECTORS,
                     DFT_R5_COEFFICIENTS_256,
                     simde_mm256_storeu_si256,
                     K)
  }

  {
    enum { K = 8 };
    DFT_RADIX25_TILE(simde__m128i,
                     radix5_combine4_q15_128_fast,
                     DFT_R25_LOAD_NONE,
                     DFT_R25_INPUT_TAIL,
                     DFT_R5_COEFFICIENTS_128,
                     simde_mm_storeu_si128,
                     K)
  }
}

#define RADIX25_BIND_R5_TWIDDLES(TW, PREFIX) \
  DFT_BIND_R5_TWIDDLES((TW), dir, PREFIX##1r, PREFIX##1i, PREFIX##2r, PREFIX##2i, PREFIX##3r, PREFIX##3i, PREFIX##4r, PREFIX##4i)
