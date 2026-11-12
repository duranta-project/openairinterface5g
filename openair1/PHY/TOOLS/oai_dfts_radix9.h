/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* Internal part of oai_dfts.c: radix-9 and radix-81 parents (sizes 36..2916 built on 9 or 81 branches).
 * Included only by oai_dfts.c, in order; not a public header. */

#ifndef OAI_DFTS_INTERNAL
#error "oai_dfts_radix9.h is included only by oai_dfts.c"
#endif

/* Scatter four child-frequency bins from 9 branch-major child transforms to
 * natural output order. AVX2 has no general integer scatter, so two 4x4
 * transposes turn branches 0..7 into contiguous 128-bit stores; branch 8 is
 * handled as one scalar complex value per output row. */

static inline void radix9_scatter8_q15_256(const c16_t *y, c16_t *dst, int M, int k)
{
  simde__m256i row0 = simde_mm256_load_si256((const simde__m256i *)(y + 0 * M + k));
  simde__m256i row1 = simde_mm256_load_si256((const simde__m256i *)(y + 1 * M + k));
  simde__m256i row2 = simde_mm256_load_si256((const simde__m256i *)(y + 2 * M + k));
  simde__m256i row3 = simde_mm256_load_si256((const simde__m256i *)(y + 3 * M + k));
  simde__m256i row4 = simde_mm256_load_si256((const simde__m256i *)(y + 4 * M + k));
  simde__m256i row5 = simde_mm256_load_si256((const simde__m256i *)(y + 5 * M + k));
  simde__m256i row6 = simde_mm256_load_si256((const simde__m256i *)(y + 6 * M + k));
  simde__m256i row7 = simde_mm256_load_si256((const simde__m256i *)(y + 7 * M + k));

  transpose8_complex_i16_256(&row0, &row1, &row2, &row3, &row4, &row5, &row6, &row7);

  c16_t tail[8] __attribute__((aligned(32)));
  simde_mm256_store_si256((simde__m256i *)tail, simde_mm256_load_si256((const simde__m256i *)(y + 8 * M + k)));

#define R9_STORE_ROW8(LANE, ROW)                              \
  do {                                                        \
    c16_t *d = dst + 9 * (k + (LANE));                        \
    simde_mm256_storeu_si256((simde__m256i *)(d + 0), (ROW)); \
    d[8] = tail[(LANE)];                                      \
  } while (0)

  R9_STORE_ROW8(0, row0);
  R9_STORE_ROW8(1, row1);
  R9_STORE_ROW8(2, row2);
  R9_STORE_ROW8(3, row3);
  R9_STORE_ROW8(4, row4);
  R9_STORE_ROW8(5, row5);
  R9_STORE_ROW8(6, row6);
  R9_STORE_ROW8(7, row7);

#undef R9_STORE_ROW8
}

/* Radix-3 butterfly of x / sqrt(3) on unscaled inputs: the pair sum and difference grow the rms by
 * sqrt(2) only; 1/sqrt(3) is applied to x0 and the sum, and folded into the 1/2 and sqrt(3)/2 factors.
 * Four multiplies instead of five and one rounding fewer per path. Only for transform inputs: their
 * components are bounded by the input amplitude, so the pair sums keep the documented headroom; on
 * intermediate data the peaks are several times the rms and the inputs are scaled first instead. */
static inline void radix3_butterfly8_unitary_q15_256(simde__m256i x0,
                                                     simde__m256i x1,
                                                     simde__m256i x2,
                                                     simde__m256i *y0,
                                                     simde__m256i *y1,
                                                     simde__m256i *y2,
                                                     dft_dir_t dir)
{
  const simde__m256i sum = simde_mm256_adds_epi16(x1, x2);
  const simde__m256i diff = simde_mm256_subs_epi16(x1, x2);
  const simde__m256i x0s = simde_mm256_mulhrs_epi16(x0, simde_mm256_set1_epi16(Q15_INV_SQRT3));
  const simde__m256i base = simde_mm256_subs_epi16(x0s, simde_mm256_mulhrs_epi16(sum, simde_mm256_set1_epi16(Q15_INV_2SQRT3)));
  /* not folded into the multiply: by 1/2 every odd input is a rounding tie (-0.5..1 dB) */
  const simde__m256i imag = half_q15_256(diff);
  *y0 = simde_mm256_adds_epi16(x0s, simde_mm256_mulhrs_epi16(sum, simde_mm256_set1_epi16(Q15_INV_SQRT3)));
  add_pm_j_i16_256(base, imag, y1, y2, dir);
}

/* Radix-3 of x / sqrt(3): the unitary form on transform inputs (RAW_INPUT), else scale first. */
#define DFT_R3_SCALED_INPUT_BUTTERFLY(RAW_INPUT, VEC, UNITARY, RAW, SCALE, X0, X1, X2, Y0, Y1, Y2) \
  do {                                                                                             \
    if (RAW_INPUT)                                                                                 \
      UNITARY((X0), (X1), (X2), (Y0), (Y1), (Y2), dir);                                            \
    else                                                                                           \
      RAW(SCALE(X0), SCALE(X1), SCALE(X2), (Y0), (Y1), (Y2), dir);                                 \
  } while (0)

/* Fused R9 = R3 x R3 DIF stage. The first R3 uses the N-point radix-3
 * twiddles, the second uses the (N/3)-point radix-3 twiddles. Output is
 * branch-major for nine child DFT_Ms.
 * Scaling (unitary 1/3 overall): the first stage scales its inputs by
 * 1/sqrt(3) so the raw radix-3 cannot saturate, then multiplies branch 0 by
 * 1/sqrt(3) and branches 1 and 2 by the shared 1/sqrt(3)-scaled twiddles. The
 * second stage then runs a raw radix-3 on these values (rms grows back to the
 * input rms) and uses unit-magnitude twiddles. */
/* Shared R9 stages; coefficient addresses and stores stay width-specific. */
#define DFT_R9_FIRST_STAGE_128(AIDX, O0, O1, O2, B1R, B1I, B2R, B2I)                                          \
  do {                                                                                                        \
    const int first_off = (AIDX) * M + off;                                                                   \
    simde__m128i z0, z1, z2;                                                                                  \
    DFT_R3_SCALED_INPUT_BUTTERFLY(raw_input,                                                                  \
                                  simde__m128i,                                                               \
                                  radix3_butterfly4_unitary_q15_128,                                          \
                                  radix3_butterfly4_q15_128,                                                  \
                                  inv_sqrt3_q15_128,                                                          \
                                  simde_mm_loadu_si128((const simde__m128i *)(src + ((AIDX) + 0) * M + off)), \
                                  simde_mm_loadu_si128((const simde__m128i *)(src + ((AIDX) + 3) * M + off)), \
                                  simde_mm_loadu_si128((const simde__m128i *)(src + ((AIDX) + 6) * M + off)), \
                                  &z0,                                                                        \
                                  &z1,                                                                        \
                                  &z2);                                                                       \
    (O0) = inv_sqrt3_q15_128(z0);                                                                             \
    (O1) = complex_mul4_prepack_small_q15_128(z1, B1R[first_off >> 2], B1I[first_off >> 2]);                  \
    (O2) = complex_mul4_prepack_small_q15_128(z2, B2R[first_off >> 2], B2I[first_off >> 2]);                  \
  } while (0)

#define DFT_R9_SECOND_STAGE_128(BETA, X0, X1, X2, A1R, A1I, A2R, A2I)           \
  do {                                                                          \
    simde__m128i z0, z1, z2;                                                    \
    radix3_butterfly4_q15_128((X0), (X1), (X2), &z0, &z1, &z2, dir);            \
    z1 = complex_mul4_prepack_q15_128(z1, A1R[off >> 2], A1I[off >> 2]);        \
    z2 = complex_mul4_prepack_q15_128(z2, A2R[off >> 2], A2I[off >> 2]);        \
    simde_mm_store_si128((simde__m128i *)(b + ((BETA) + 0 * 3) * M + off), z0); \
    simde_mm_store_si128((simde__m128i *)(b + ((BETA) + 1 * 3) * M + off), z1); \
    simde_mm_store_si128((simde__m128i *)(b + ((BETA) + 2 * 3) * M + off), z2); \
  } while (0)

static inline void radix9_stage_to_branch_major_q15_128_plan(const c16_t *src,
                                                             c16_t *b,
                                                             int N,
                                                             dft_dir_t dir,
                                                             const r3_twiddle_t *twB,
                                                             const r3_twiddle_t *twA)
{
  const int raw_input = 1; /* only used on transform inputs (DFT36) */
  const int M = N / 9;

  DFT_BIND_R3_TWIDDLES(twB, dir, B1r, B1i, B2r, B2i);
  DFT_BIND_R3_UNIT_TWIDDLES(twA, dir, A1r, A1i, A2r, A2i);

  for (int off = 0; off < M; off += 4) {
    simde__m128i s00, s01, s02, s10, s11, s12, s20, s21, s22;

    DFT_R9_FIRST_STAGE_128(0, s00, s01, s02, B1r, B1i, B2r, B2i);
    DFT_R9_FIRST_STAGE_128(1, s10, s11, s12, B1r, B1i, B2r, B2i);
    DFT_R9_FIRST_STAGE_128(2, s20, s21, s22, B1r, B1i, B2r, B2i);

    DFT_R9_SECOND_STAGE_128(0, s00, s10, s20, A1r, A1i, A2r, A2i);
    DFT_R9_SECOND_STAGE_128(1, s01, s11, s21, A1r, A1i, A2r, A2i);
    DFT_R9_SECOND_STAGE_128(2, s02, s12, s22, A1r, A1i, A2r, A2i);
  }
}

DEFINE_RAW_RADIX3_BUTTERFLY(radix3_butterfly8_q15_256,
                            simde__m256i,
                            simde_mm256_adds_epi16,
                            simde_mm256_subs_epi16,
                            half_q15_256(sum),
                            mul_minus_j_scaled_dir_i16_256(diff, Q15_HALF_SQRT3, dir),
                            unused)

/* N=288 R9 parent. Two adjacent 128-bit pre-packed twiddle blocks are
 * contiguous and 32-byte aligned, so each AVX2 load covers eight complex
 * samples without repacking coefficients in the hot loop. */
#define DFT_R9_FIRST_STAGE_256(AIDX, O0, O1, O2, LOAD_TW)                                                        \
  do {                                                                                                           \
    const int first_off = (AIDX) * M + off;                                                                      \
    const int tidx = first_off >> 2;                                                                             \
    simde__m256i z0, z1, z2;                                                                                     \
    DFT_R3_SCALED_INPUT_BUTTERFLY(raw_input,                                                                     \
                                  simde__m256i,                                                                  \
                                  radix3_butterfly8_unitary_q15_256,                                             \
                                  radix3_butterfly8_q15_256,                                                     \
                                  inv_sqrt3_q15_256,                                                             \
                                  simde_mm256_loadu_si256((const simde__m256i *)(src + ((AIDX) + 0) * M + off)), \
                                  simde_mm256_loadu_si256((const simde__m256i *)(src + ((AIDX) + 3) * M + off)), \
                                  simde_mm256_loadu_si256((const simde__m256i *)(src + ((AIDX) + 6) * M + off)), \
                                  &z0,                                                                           \
                                  &z1,                                                                           \
                                  &z2);                                                                          \
    (O0) = inv_sqrt3_q15_256(z0);                                                                                \
    (O1) = complex_mul8_prepack_small_q15_256(z1,                                                                \
                                              LOAD_TW((const simde__m256i *)(B1r128 + tidx)),                    \
                                              LOAD_TW((const simde__m256i *)(B1i128 + tidx)));                   \
    (O2) = complex_mul8_prepack_small_q15_256(z2,                                                                \
                                              LOAD_TW((const simde__m256i *)(B2r128 + tidx)),                    \
                                              LOAD_TW((const simde__m256i *)(B2i128 + tidx)));                   \
  } while (0)

#define DFT_R9_SECOND_STAGE_256(BETA, X0, X1, X2, STORE)             \
  do {                                                               \
    simde__m256i z0, z1, z2;                                         \
    radix3_butterfly8_q15_256((X0), (X1), (X2), &z0, &z1, &z2, dir); \
    z1 = complex_mul8_prepack_q15_256(z1, A1r, A1i);                 \
    z2 = complex_mul8_prepack_q15_256(z2, A2r, A2i);                 \
    STORE((simde__m256i *)(b + ((BETA) + 0 * 3) * M + off), z0);     \
    STORE((simde__m256i *)(b + ((BETA) + 1 * 3) * M + off), z1);     \
    STORE((simde__m256i *)(b + ((BETA) + 2 * 3) * M + off), z2);     \
  } while (0)

#define DFT_R9_VECTOR_INPUT_STAGE(LOAD)                                 \
  simde__m256i s00, s01, s02, s10, s11, s12, s20, s21, s22;             \
  DFT_R9_FIRST_STAGE_256(0, s00, s01, s02, LOAD);                       \
  DFT_R9_FIRST_STAGE_256(1, s10, s11, s12, LOAD);                       \
  DFT_R9_FIRST_STAGE_256(2, s20, s21, s22, LOAD);                       \
  const int aidx = off >> 2;                                            \
  const simde__m256i A1r = LOAD((const simde__m256i *)(A1r128 + aidx)); \
  const simde__m256i A1i = LOAD((const simde__m256i *)(A1i128 + aidx)); \
  const simde__m256i A2r = LOAD((const simde__m256i *)(A2r128 + aidx)); \
  const simde__m256i A2i = LOAD((const simde__m256i *)(A2i128 + aidx));

#define DFT_R9_ALIGNED_PARENT_BODY()                                    \
  DFT_BIND_R3_TWIDDLES(twB, dir, B1r128, B1i128, B2r128, B2i128);       \
  DFT_BIND_R3_UNIT_TWIDDLES(twA, dir, A1r128, A1i128, A2r128, A2i128);  \
                                                                        \
  for (int off = 0; off < M; off += 8) {                                \
    DFT_R9_VECTOR_INPUT_STAGE(simde_mm256_load_si256)                   \
                                                                        \
    DFT_R9_SECOND_STAGE_256(0, s00, s10, s20, simde_mm256_store_si256); \
    DFT_R9_SECOND_STAGE_256(1, s01, s11, s21, simde_mm256_store_si256); \
    DFT_R9_SECOND_STAGE_256(2, s02, s12, s22, simde_mm256_store_si256); \
  }

static inline void radix9_stage_to_branch_major_q15_256_288(const c16_t *src,
                                                            c16_t *b,
                                                            dft_dir_t dir,
                                                            const r3_twiddle_t *twB,
                                                            const r3_twiddle_t *twA)
{
  enum { N = 288, M = 32 };
  const int raw_input = 1;

  DFT_R9_ALIGNED_PARENT_BODY()
}

/* raw_input: src is the transform input (see radix3_butterfly8_unitary_q15_256). */
static inline void radix9_stage_to_branch_major_q15_256(const c16_t *src,
                                                        c16_t *b,
                                                        int N,
                                                        dft_dir_t dir,
                                                        const r3_twiddle_t *twB,
                                                        const r3_twiddle_t *twA,
                                                        const int raw_input)
{
  const int M = N / 9;

  DFT_R9_ALIGNED_PARENT_BODY()
}

/* DFT576 parent R9 output directly in the lane-major format consumed by
 * the eight-way DFT64 child kernel. Branch 8 remains contiguous for the
 * standalone DFT64 tail. */
static inline void radix9_stage_to_dft64x8_576(const c16_t *src,
                                               simde__m256i leaf[64],
                                               c16_t branch8[64],
                                               dft_dir_t dir,
                                               const r3_twiddle_t *twB,
                                               const r3_twiddle_t *twA,
                                               const int raw_input)
{
  enum { M = 64 };

  DFT_BIND_R3_TWIDDLES(twB, dir, B1r128, B1i128, B2r128, B2i128);
  DFT_BIND_R3_UNIT_TWIDDLES(twA, dir, A1r128, A1i128, A2r128, A2i128);

  for (int off = 0; off < M; off += 8) {
    DFT_R9_VECTOR_INPUT_STAGE(simde_mm256_load_si256)

#define R9_SECOND_STAGE_576(X0, X1, X2, O0, O1, O2)                        \
  do {                                                                     \
    radix3_butterfly8_q15_256((X0), (X1), (X2), &(O0), &(O1), &(O2), dir); \
    (O1) = complex_mul8_prepack_small_q15_256((O1), A1r, A1i);             \
    (O2) = complex_mul8_prepack_small_q15_256((O2), A2r, A2i);             \
  } while (0)

    simde__m256i r0, r1, r2, r3, r4, r5, r6, r7, r8;
    R9_SECOND_STAGE_576(s00, s10, s20, r0, r3, r6);
    R9_SECOND_STAGE_576(s01, s11, s21, r1, r4, r7);
    R9_SECOND_STAGE_576(s02, s12, s22, r2, r5, r8);

#undef R9_SECOND_STAGE_576

    DFT_TRANSPOSE8_TO_ROW(r, leaf, off)

    simde_mm256_store_si256((simde__m256i *)(branch8 + off), r8);
  }
}

/* Common SIMD stages for the two hybrid R9 parents. */

#define DFT_R9_VECTOR_LOOP for (int off = 0; off < AVX2_END; off += 8) {
#define DEFINE_R9_HYBRID_PARENT(FN, NVAL, MVAL, ENDVAL, VECTOR_PROLOGUE)       \
  static inline void FN(const c16_t *src,                                      \
                        c16_t *b,                                              \
                        dft_dir_t dir,                                         \
                        const r3_twiddle_t *twB,                               \
                        const r3_twiddle_t *twA,                               \
                        const int raw_input)                                   \
  {                                                                            \
    enum { N = NVAL, M = MVAL, AVX2_END = ENDVAL };                            \
                                                                               \
    DFT_BIND_R3_TWIDDLES(twB, dir, B1r128, B1i128, B2r128, B2i128);            \
    DFT_BIND_R3_UNIT_TWIDDLES(twA, dir, A1r128, A1i128, A2r128, A2i128);       \
                                                                               \
    VECTOR_PROLOGUE                                                            \
    DFT_R9_VECTOR_INPUT_STAGE(simde_mm256_loadu_si256)                         \
                                                                               \
    DFT_R9_SECOND_STAGE_256(0, s00, s10, s20, simde_mm256_storeu_si256);       \
    DFT_R9_SECOND_STAGE_256(1, s01, s11, s21, simde_mm256_storeu_si256);       \
    DFT_R9_SECOND_STAGE_256(2, s02, s12, s22, simde_mm256_storeu_si256);       \
  }                                                                            \
                                                                               \
  /* 128-bit tail for the last four samples (off = AVX2_END). */               \
  {                                                                            \
    const int off = AVX2_END;                                                  \
    simde__m128i s00, s01, s02, s10, s11, s12, s20, s21, s22;                  \
                                                                               \
    DFT_R9_FIRST_STAGE_128(0, s00, s01, s02, B1r128, B1i128, B2r128, B2i128);  \
    DFT_R9_FIRST_STAGE_128(1, s10, s11, s12, B1r128, B1i128, B2r128, B2i128);  \
    DFT_R9_FIRST_STAGE_128(2, s20, s21, s22, B1r128, B1i128, B2r128, B2i128);  \
                                                                               \
    DFT_R9_SECOND_STAGE_128(0, s00, s10, s20, A1r128, A1i128, A2r128, A2i128); \
    DFT_R9_SECOND_STAGE_128(1, s01, s11, s21, A1r128, A1i128, A2r128, A2i128); \
    DFT_R9_SECOND_STAGE_128(2, s02, s12, s22, A1r128, A1i128, A2r128, A2i128); \
  }                                                                            \
  }

DEFINE_R9_HYBRID_PARENT(radix9_stage_to_branch_major_q15_256_128_hybrid_972, 972, 108, 104, DFT_R9_VECTOR_LOOP)

#define DFT_R9_SINGLE_VECTOR \
  {                          \
    const int off = 0;

DEFINE_R9_HYBRID_PARENT(radix9_stage_to_branch_major_q15_256_128_hybrid_108, 108, 12, 8, DFT_R9_SINGLE_VECTOR)

static inline void dft32_q15_256_contiguous(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  const simde__m256i x0 = simde_mm256_loadu_si256((const simde__m256i *)(src + 0));
  const simde__m256i x1 = simde_mm256_loadu_si256((const simde__m256i *)(src + 8));
  const simde__m256i x2 = simde_mm256_loadu_si256((const simde__m256i *)(src + 16));
  const simde__m256i x3 = simde_mm256_loadu_si256((const simde__m256i *)(src + 24));

  simde__m256i H0, H1, H2, H3;
  dft4x4_q15_256_lanes(x0, x1, x2, x3, &H0, &H1, &H2, &H3, dir);

  const simde__m256i W1_RE = simde_mm256_load_si256((const simde__m256i *)W32_1_RE_RE_256);
  const simde__m256i W1_IM = twiddle_im_dir_256(simde_mm256_load_si256((const simde__m256i *)W32_1_IM_SIGNED_256), dir);
  const simde__m256i W2_RE = simde_mm256_load_si256((const simde__m256i *)W32_2_RE_RE_256);
  const simde__m256i W2_IM = twiddle_im_dir_256(simde_mm256_load_si256((const simde__m256i *)W32_2_IM_SIGNED_256), dir);
  const simde__m256i W3_RE = simde_mm256_load_si256((const simde__m256i *)W32_3_RE_RE_256);
  const simde__m256i W3_IM = twiddle_im_dir_256(simde_mm256_load_si256((const simde__m256i *)W32_3_IM_SIGNED_256), dir);

  H1 = complex_mul8_prepack_q15_256(H1, W1_RE, W1_IM);
  H2 = complex_mul8_prepack_q15_256(H2, W2_RE, W2_IM);
  H3 = complex_mul8_prepack_q15_256(H3, W3_RE, W3_IM);

  transpose4_complex_i16_256x2_shuffle(&H0, &H1, &H2, &H3);

  const simde__m128i lo0 = simde_mm256_castsi256_si128(H0);
  const simde__m128i lo1 = simde_mm256_castsi256_si128(H1);
  const simde__m128i lo2 = simde_mm256_castsi256_si128(H2);
  const simde__m128i lo3 = simde_mm256_castsi256_si128(H3);
  const simde__m128i hi0 = simde_mm256_extracti128_si256(H0, 1);
  const simde__m128i hi1 = simde_mm256_extracti128_si256(H1, 1);
  const simde__m128i hi2 = simde_mm256_extracti128_si256(H2, 1);
  const simde__m128i hi3 = simde_mm256_extracti128_si256(H3, 1);

  simde__m128i Y0, Y1, Y2, Y3, Y4, Y5, Y6, Y7;
  dft8x4_q15_128(lo0, lo1, lo2, lo3, hi0, hi1, hi2, hi3, &Y0, &Y1, &Y2, &Y3, &Y4, &Y5, &Y6, &Y7, dir);

  simde_mm_storeu_si128((simde__m128i *)(dst + 0), Y0);
  simde_mm_storeu_si128((simde__m128i *)(dst + 4), Y1);
  simde_mm_storeu_si128((simde__m128i *)(dst + 8), Y2);
  simde_mm_storeu_si128((simde__m128i *)(dst + 12), Y3);
  simde_mm_storeu_si128((simde__m128i *)(dst + 16), Y4);
  simde_mm_storeu_si128((simde__m128i *)(dst + 20), Y5);
  simde_mm_storeu_si128((simde__m128i *)(dst + 24), Y6);
  simde_mm_storeu_si128((simde__m128i *)(dst + 28), Y7);
}

/* Eight complex lanes times one broadcast Q15 twiddle (wr, wi). */
static inline simde__m256i complex_mul8_bcast_q15_256_parent(simde__m256i a, int16_t wr, int16_t wi)
{
  const simde__m256i w_re_re = simde_mm256_set1_epi16(wr);
  /* (-wi, wi) pairs as one dword broadcast (a 16-word setr from runtime values becomes 16 word inserts) */
  const simde__m256i w_im_signed = simde_mm256_set1_epi32((int32_t)((uint16_t)-wi | ((uint32_t)(uint16_t)wi << 16)));
  return complex_mul8_prepack_q15_256(a, w_re_re, w_im_signed);
}

/* Broadcast twiddle of magnitude <= 1/sqrt(3); see complex_mul8_prepack_small_q15_256. */
static inline simde__m256i complex_mul8_bcast_small_q15_256(simde__m256i a, int16_t wr, int16_t wi)
{
  const simde__m256i w_re_re = simde_mm256_set1_epi16(wr);
  const simde__m256i w_im_signed = simde_mm256_setr_epi16(-wi, wi, -wi, wi, -wi, wi, -wi, wi, -wi, wi, -wi, wi, -wi, wi, -wi, wi);
  return complex_mul8_prepack_small_q15_256(a, w_re_re, w_im_signed);
}

/* -------------------------------------------------------------------------
 * AVX2 batched twiddle-scaled leaf kernels.
 *
 * Leaf normalization is carried by the internal Cooley-Tukey twiddles:
 *
 *   DFT8  = x/sqrt(2) -> DFT2 -> W8  / 2        -> raw DFT4
 *   DFT16 = x/2       -> DFT4 -> W16 / 2        -> raw DFT4
 *   DFT32 = x/2       -> DFT4 -> W32 / sqrt(8)  -> raw DFT8
 *   DFT12 = x/sqrt(3) -> DFT3 -> W12 / 2        -> raw DFT4
 *
 * The input scaling keeps the first sums of full-scale samples in range.
 *
 * Eight independent leaves are carried in the eight 32-bit complex lanes of
 * one YMM register. Partial final groups are stored with 16/8/4-byte stores
 * (store_complex_lanes_256()).
 * ------------------------------------------------------------------------- */

static const int16_t dft8_leaf_twiddle_re[8] = {16383, 11585, 0, -11585, -16383, -11585, 0, 11585};
static const int16_t dft8_leaf_twiddle_im_fwd[8] = {0, -11585, -16383, -11585, 0, 11585, 16383, 11585};
/* Leaf twiddles: round(S cos(2 pi k / N)), S = 16383 (DFT8/12/16, see Q15_HALF_TW), 32768 / sqrt(8) (DFT32), 32768 / sqrt(5)
 * (DFT20). */
static const int16_t dft16_leaf_twiddle_re[16] =
    {16383, 15137, 11585, 6270, 0, -6270, -11585, -15137, -16383, -15137, -11585, -6270, 0, 6270, 11585, 15137};
static const int16_t dft16_leaf_twiddle_im_fwd[16] =
    {0, -6270, -11585, -15137, -16383, -15137, -11585, -6270, 0, 6270, 11585, 15137, 16383, 15137, 11585, 6270};
static const int16_t dft32_leaf_twiddle_re[32] = {11585, 11363, 10703, 9633,   8191,   6436,   4433,   2260,   0,     -2260, -4433,
                                                  -6436, -8191, -9633, -10703, -11363, -11585, -11363, -10703, -9633, -8191, -6436,
                                                  -4433, -2260, 0,     2260,   4433,   6436,   8191,   9633,   10703, 11363};
static const int16_t dft32_leaf_twiddle_im_fwd[32] = {
    0, -2260, -4433, -6436, -8191, -9633, -10703, -11363, -11585, -11363, -10703, -9633, -8191, -6436, -4433, -2260,
    0, 2260,  4433,  6436,  8191,  9633,  10703,  11363,  11585,  11363,  10703,  9633,  8191,  6436,  4433,  2260};

static __attribute__((always_inline)) inline simde__m256i complex_lane_mask_epi32(int valid)
{
  return simde_mm256_setr_epi32(valid > 0 ? -1 : 0,
                                valid > 1 ? -1 : 0,
                                valid > 2 ? -1 : 0,
                                valid > 3 ? -1 : 0,
                                valid > 4 ? -1 : 0,
                                valid > 5 ? -1 : 0,
                                valid > 6 ? -1 : 0,
                                valid > 7 ? -1 : 0);
}

static __attribute__((always_inline)) inline simde__m256i leaf_twiddle_mul_q15_256(simde__m256i x,
                                                                                   const int16_t *wr,
                                                                                   const int16_t *wi,
                                                                                   int N,
                                                                                   int power,
                                                                                   dft_dir_t dir)
{
  power %= N;
  if (power < 0)
    power += N;
  /* Leaf tables have magnitude <= 1/2. */
  const simde__m256i w_re_re = simde_mm256_set1_epi16(wr[power]);
  const int16_t wim = twiddle_im_scalar_dir_i16(wi[power], dir);
  const simde__m256i w_im_signed =
      simde_mm256_setr_epi16(-wim, wim, -wim, wim, -wim, wim, -wim, wim, -wim, wim, -wim, wim, -wim, wim, -wim, wim);
  return complex_mul8_prepack_small_q15_256(x, w_re_re, w_im_signed);
}

static __attribute__((always_inline)) inline void dft4x8_raw_q15_256(simde__m256i x0,
                                                                     simde__m256i x1,
                                                                     simde__m256i x2,
                                                                     simde__m256i x3,
                                                                     simde__m256i y[4],
                                                                     dft_dir_t dir)
{
  const simde__m256i s02 = simde_mm256_adds_epi16(x0, x2);
  const simde__m256i d02 = simde_mm256_subs_epi16(x0, x2);
  const simde__m256i s13 = simde_mm256_adds_epi16(x1, x3);
  const simde__m256i d13 = simde_mm256_subs_epi16(x1, x3);
  y[0] = simde_mm256_adds_epi16(s02, s13);
  y[2] = simde_mm256_subs_epi16(s02, s13);
  add_pm_j_i16_256(d02, d13, &y[1], &y[3], dir);
}

/* Partial SIMD blocks contain four complex samples; upper lanes are zero. */
static __attribute__((always_inline)) inline simde__m256i load_complex8_partial(const c16_t *src, int valid)
{
  if (valid == 8)
    return simde_mm256_loadu_si256((const simde__m256i *)src);
  return simde_mm256_inserti128_si256(simde_mm256_setzero_si256(), simde_mm_loadu_si128((const simde__m128i *)src), 0);
}

static __attribute__((always_inline)) inline void load_batched_leaf_inputs(const c16_t *b,
                                                                           int M,
                                                                           int first,
                                                                           int valid,
                                                                           int leaf_n,
                                                                           simde__m256i *x)
{
  const simde__m256i zero = simde_mm256_setzero_si256();
  for (int off = 0; off < leaf_n; off += 8) {
    simde__m256i z[8];
    for (int lane = 0; lane < 8; lane++)
      z[lane] = lane < valid ? load_complex8_partial(b + (first + lane) * M + off, leaf_n - off >= 8 ? 8 : 4) : zero;
    transpose8_complex_i16_256(&z[0], &z[1], &z[2], &z[3], &z[4], &z[5], &z[6], &z[7]);
    for (int lane = 0; lane < 8; lane++)
      x[off + lane] = z[lane];
  }
}

static __attribute__((always_inline)) inline void store_batched_leaf_outputs(c16_t *dst,
                                                                             int R,
                                                                             int first,
                                                                             int valid,
                                                                             int leaf_n,
                                                                             const simde__m256i *y)
{
  if (valid == 8) {
    for (int k = 0; k < leaf_n; k++)
      simde_mm256_storeu_si256((simde__m256i *)(dst + R * k + first), y[k]);
    return;
  }

  for (int k = 0; k < leaf_n; k++)
    store_complex_lanes_256(dst + R * k + first, y[k], valid);
}

#define DFT8_FOLDED_LEAF_STAGES(INDEX)                                                                               \
  for (int INDEX = 0; INDEX < 4; INDEX++) {                                                                          \
    const simde__m256i xa = scale_q15_256(x[INDEX], Q15_INV_SQRT2), xb = scale_q15_256(x[INDEX + 4], Q15_INV_SQRT2); \
    const simde__m256i s = simde_mm256_adds_epi16(xa, xb);                                                           \
    const simde__m256i d = simde_mm256_subs_epi16(xa, xb);                                                           \
    a[INDEX][0] = leaf_twiddle_mul_q15_256(s, dft8_leaf_twiddle_re, dft8_leaf_twiddle_im_fwd, 8, 0, dir);            \
    a[INDEX][1] = leaf_twiddle_mul_q15_256(d, dft8_leaf_twiddle_re, dft8_leaf_twiddle_im_fwd, 8, INDEX, dir);        \
  }                                                                                                                  \
  for (int k2 = 0; k2 < 2; k2++) {                                                                                   \
    simde__m256i z[4];                                                                                               \
    dft4x8_raw_q15_256(a[0][k2], a[1][k2], a[2][k2], a[3][k2], z, dir);                                              \
    for (int k1 = 0; k1 < 4; k1++)                                                                                   \
      y[k2 + 2 * k1] = z[k1];                                                                                        \
  }

static __attribute__((always_inline)) inline void
dft8x8_twiddle_scaled_store(const c16_t *b, int M, c16_t *dst, int R, int first, int valid, dft_dir_t dir)
{
  simde__m256i x[8], a[4][2], y[8];
  load_batched_leaf_inputs(b, M, first, valid, 8, x);

  DFT8_FOLDED_LEAF_STAGES(n1) store_batched_leaf_outputs(dst, R, first, valid, 8, y);
}

static __attribute__((always_inline)) inline void
dft16x8_twiddle_scaled_store_body(const c16_t *b, int M, c16_t *dst, int R, int first, int valid, dft_dir_t dir)
{
  const simde__m256i zero = simde_mm256_setzero_si256();
#define LOAD16_ROW(LANE, OFF) \
  ((LANE) < valid ? simde_mm256_loadu_si256((const simde__m256i *)(b + (first + (LANE)) * M + (OFF))) : zero)

  simde__m256i l0 = LOAD16_ROW(0, 0);
  simde__m256i l1 = LOAD16_ROW(1, 0);
  simde__m256i l2 = LOAD16_ROW(2, 0);
  simde__m256i l3 = LOAD16_ROW(3, 0);
  simde__m256i l4 = LOAD16_ROW(4, 0);
  simde__m256i l5 = LOAD16_ROW(5, 0);
  simde__m256i l6 = LOAD16_ROW(6, 0);
  simde__m256i l7 = LOAD16_ROW(7, 0);
  transpose8_complex_i16_256(&l0, &l1, &l2, &l3, &l4, &l5, &l6, &l7);

  simde__m256i h0 = LOAD16_ROW(0, 8);
  simde__m256i h1 = LOAD16_ROW(1, 8);
  simde__m256i h2 = LOAD16_ROW(2, 8);
  simde__m256i h3 = LOAD16_ROW(3, 8);
  simde__m256i h4 = LOAD16_ROW(4, 8);
  simde__m256i h5 = LOAD16_ROW(5, 8);
  simde__m256i h6 = LOAD16_ROW(6, 8);
  simde__m256i h7 = LOAD16_ROW(7, 8);
  transpose8_complex_i16_256(&h0, &h1, &h2, &h3, &h4, &h5, &h6, &h7);
#undef LOAD16_ROW

  simde__m256i a[4][4];
#define FIRST4(N1, X0, X1, X2, X3)                                                                                         \
  do {                                                                                                                     \
    simde__m256i z[4];                                                                                                     \
    dft4x8_raw_q15_256(half_q15_256(X0), half_q15_256(X1), half_q15_256(X2), half_q15_256(X3), z, dir);                    \
    for (int k2 = 0; k2 < 4; k2++)                                                                                         \
      a[(N1)][k2] = leaf_twiddle_mul_q15_256(z[k2], dft16_leaf_twiddle_re, dft16_leaf_twiddle_im_fwd, 16, (N1) * k2, dir); \
  } while (0)

  FIRST4(0, l0, l4, h0, h4);
  FIRST4(1, l1, l5, h1, h5);
  FIRST4(2, l2, l6, h2, h6);
  FIRST4(3, l3, l7, h3, h7);
#undef FIRST4

  for (int k2 = 0; k2 < 4; k2++) {
    simde__m256i z[4];
    dft4x8_raw_q15_256(a[0][k2], a[1][k2], a[2][k2], a[3][k2], z, dir);
    for (int k1 = 0; k1 < 4; k1++) {
      c16_t *out = dst + R * (k2 + 4 * k1) + first;
      store_complex_lanes_256(out, z[k1], valid);
    }
  }
}

/* One copy per direction, so the direction is a constant inside. */
static __attribute__((noinline)) void
dft16x8_twiddle_scaled_store(const c16_t *b, int M, c16_t *dst, int R, int first, int valid, dft_dir_t dir)
{
  if (dir == DFT_DIR_FORWARD)
    dft16x8_twiddle_scaled_store_body(b, M, dst, R, first, valid, DFT_DIR_FORWARD);
  else
    dft16x8_twiddle_scaled_store_body(b, M, dst, R, first, valid, DFT_DIR_INVERSE);
}

static __attribute__((always_inline)) inline void
dft32x8_twiddle_scaled_store_body(const c16_t *b, int M, c16_t *dst, int R, int first, int valid, dft_dir_t dir)
{
  const simde__m256i zero = simde_mm256_setzero_si256();
#define LOAD32_ROW(LANE, OFF) \
  ((LANE) < valid ? simde_mm256_loadu_si256((const simde__m256i *)(b + (first + (LANE)) * M + (OFF))) : zero)

  /*
   * Keep only half of the transposed DFT32 input materialized.
   * Samples 0..15 are stored in xlo[16]; samples 16..31 stay in
   * registers and are consumed directly by the first radix-4 stage.
   */
  simde__m256i xlo[16] __attribute__((aligned(64)));

  {
    simde__m256i q0 = LOAD32_ROW(0, 0), q1 = LOAD32_ROW(1, 0), q2 = LOAD32_ROW(2, 0), q3 = LOAD32_ROW(3, 0), q4 = LOAD32_ROW(4, 0),
                 q5 = LOAD32_ROW(5, 0), q6 = LOAD32_ROW(6, 0), q7 = LOAD32_ROW(7, 0);
    transpose8_complex_i16_256(&q0, &q1, &q2, &q3, &q4, &q5, &q6, &q7);
    xlo[0] = q0;
    xlo[1] = q1;
    xlo[2] = q2;
    xlo[3] = q3;
    xlo[4] = q4;
    xlo[5] = q5;
    xlo[6] = q6;
    xlo[7] = q7;
  }

  {
    simde__m256i q0 = LOAD32_ROW(0, 8), q1 = LOAD32_ROW(1, 8), q2 = LOAD32_ROW(2, 8), q3 = LOAD32_ROW(3, 8), q4 = LOAD32_ROW(4, 8),
                 q5 = LOAD32_ROW(5, 8), q6 = LOAD32_ROW(6, 8), q7 = LOAD32_ROW(7, 8);
    transpose8_complex_i16_256(&q0, &q1, &q2, &q3, &q4, &q5, &q6, &q7);
    xlo[8] = q0;
    xlo[9] = q1;
    xlo[10] = q2;
    xlo[11] = q3;
    xlo[12] = q4;
    xlo[13] = q5;
    xlo[14] = q6;
    xlo[15] = q7;
  }

  simde__m256i m0 = LOAD32_ROW(0, 16), m1 = LOAD32_ROW(1, 16), m2 = LOAD32_ROW(2, 16), m3 = LOAD32_ROW(3, 16),
               m4 = LOAD32_ROW(4, 16), m5 = LOAD32_ROW(5, 16), m6 = LOAD32_ROW(6, 16), m7 = LOAD32_ROW(7, 16);
  transpose8_complex_i16_256(&m0, &m1, &m2, &m3, &m4, &m5, &m6, &m7);

  simde__m256i h0 = LOAD32_ROW(0, 24), h1 = LOAD32_ROW(1, 24), h2 = LOAD32_ROW(2, 24), h3 = LOAD32_ROW(3, 24),
               h4 = LOAD32_ROW(4, 24), h5 = LOAD32_ROW(5, 24), h6 = LOAD32_ROW(6, 24), h7 = LOAD32_ROW(7, 24);
  transpose8_complex_i16_256(&h0, &h1, &h2, &h3, &h4, &h5, &h6, &h7);
#undef LOAD32_ROW

  simde__m256i a[8][4];

#define FIRST4_32(N1, MREG, HREG)                                                                                             \
  do {                                                                                                                        \
    simde__m256i z[4];                                                                                                        \
    dft4x8_raw_q15_256(half_q15_256(xlo[(N1)]), half_q15_256(xlo[8 + (N1)]), half_q15_256(MREG), half_q15_256(HREG), z, dir); \
    for (int k2 = 0; k2 < 4; k2++)                                                                                            \
      a[(N1)][k2] = leaf_twiddle_mul_q15_256(z[k2], dft32_leaf_twiddle_re, dft32_leaf_twiddle_im_fwd, 32, (N1) * k2, dir);    \
  } while (0)

  FIRST4_32(0, m0, h0);
  FIRST4_32(1, m1, h1);
  FIRST4_32(2, m2, h2);
  FIRST4_32(3, m3, h3);
  FIRST4_32(4, m4, h4);
  FIRST4_32(5, m5, h5);
  FIRST4_32(6, m6, h6);
  FIRST4_32(7, m7, h7);
#undef FIRST4_32

  for (int k2 = 0; k2 < 4; k2++) {
    simde__m256i z0, z1, z2, z3, z4, z5, z6, z7;
    dft8x8_q15_256_dir(a[0][k2],
                       a[1][k2],
                       a[2][k2],
                       a[3][k2],
                       a[4][k2],
                       a[5][k2],
                       a[6][k2],
                       a[7][k2],
                       &z0,
                       &z1,
                       &z2,
                       &z3,
                       &z4,
                       &z5,
                       &z6,
                       &z7,
                       dir);

    const simde__m256i z[8] = {z0, z1, z2, z3, z4, z5, z6, z7};
    for (int k1 = 0; k1 < 8; k1++) {
      c16_t *out = dst + R * (k2 + 4 * k1) + first;
      store_complex_lanes_256(out, z[k1], valid);
    }
  }
}

/* One copy per direction, so the direction is a constant inside. */
static __attribute__((noinline)) void
dft32x8_twiddle_scaled_store(const c16_t *b, int M, c16_t *dst, int R, int first, int valid, dft_dir_t dir)
{
  if (dir == DFT_DIR_FORWARD)
    dft32x8_twiddle_scaled_store_body(b, M, dst, R, first, valid, DFT_DIR_FORWARD);
  else
    dft32x8_twiddle_scaled_store_body(b, M, dst, R, first, valid, DFT_DIR_INVERSE);
}

static const int16_t dft12_leaf_twiddle_re[12] = {16383, 14189, 8191, 0, -8191, -14189, -16383, -14189, -8191, 0, 8191, 14189};
static const int16_t dft12_leaf_twiddle_im_fwd[12] = {0, -8191, -14189, -16383, -14189, -8191, 0, 8191, 14189, 16383, 14189, 8191};

static __attribute__((always_inline)) inline void load_four_leaf_inputs_gathered(const c16_t *b,
                                                                                 int M,
                                                                                 int first,
                                                                                 int valid,
                                                                                 int leaf_n,
                                                                                 simde__m256i *x)
{
  const simde__m256i idx = simde_mm256_setr_epi32(0, M, 2 * M, 3 * M, 0, 0, 0, 0);
  const simde__m256i mask =
      simde_mm256_setr_epi32(valid > 0 ? -1 : 0, valid > 1 ? -1 : 0, valid > 2 ? -1 : 0, valid > 3 ? -1 : 0, 0, 0, 0, 0);
  const simde__m256i zero = simde_mm256_setzero_si256();
  for (int k = 0; k < leaf_n; k++)
    x[k] = simde_mm256_mask_i32gather_epi32(zero, (const int *)(b + first * M + k), idx, mask, 4);
}
/* Common folded R3 stage; input expressions select the fixed leaf geometry. */
#define DFT_R3_FOLDED_LEAF_STAGE(ROWS, NVAL, X0, X1, X2, RE, IM)        \
  for (int n1 = 0; n1 < ROWS; n1++) {                                   \
    simde__m256i z0, z1, z2;                                            \
    radix3_butterfly8_q15_256(scale_q15_256(X0, Q15_INV_SQRT3),         \
                              scale_q15_256(X1, Q15_INV_SQRT3),         \
                              scale_q15_256(X2, Q15_INV_SQRT3),         \
                              &z0,                                      \
                              &z1,                                      \
                              &z2,                                      \
                              dir);                                     \
    a[n1][0] = leaf_twiddle_mul_q15_256(z0, RE, IM, NVAL, 0, dir);      \
    a[n1][1] = leaf_twiddle_mul_q15_256(z1, RE, IM, NVAL, n1, dir);     \
    a[n1][2] = leaf_twiddle_mul_q15_256(z2, RE, IM, NVAL, 2 * n1, dir); \
  }

#define DFT12_FOLDED_SECOND_STAGE(BUTTERFLY)                   \
  for (int k2 = 0; k2 < 3; k2++) {                             \
    simde__m256i z[4];                                         \
    BUTTERFLY(a[0][k2], a[1][k2], a[2][k2], a[3][k2], z, dir); \
    for (int k1 = 0; k1 < 4; k1++)                             \
      y[k2 + 3 * k1] = z[k1];                                  \
  }

static __attribute__((always_inline)) inline void
dft12x8_twiddle_scaled_store(const c16_t *b, int M, c16_t *dst, int R, int first, int valid, dft_dir_t dir)
{
  simde__m256i x[16], a[4][3], y[12];
  load_batched_leaf_inputs(b, M, first, valid, 12, x);
  DFT_R3_FOLDED_LEAF_STAGE(4, 12, x[n1], x[n1 + 4], x[n1 + 8], dft12_leaf_twiddle_re, dft12_leaf_twiddle_im_fwd)
  DFT12_FOLDED_SECOND_STAGE(dft4x8_raw_q15_256)
  store_batched_leaf_outputs(dst, R, first, valid, 12, y);
}
static __attribute__((always_inline)) inline void
dft12x4_twiddle_scaled_store(const c16_t *b, int M, c16_t *dst, int R, int first, int valid, dft_dir_t dir)
{
  simde__m256i x[12], a[4][3], y[12];
  load_four_leaf_inputs_gathered(b, M, first, valid, 12, x);
  DFT_R3_FOLDED_LEAF_STAGE(4, 12, x[n1], x[n1 + 4], x[n1 + 8], dft12_leaf_twiddle_re, dft12_leaf_twiddle_im_fwd)
  DFT12_FOLDED_SECOND_STAGE(dft4x8_raw_q15_256)
  store_batched_leaf_outputs(dst, R, first, valid, 12, y);
}

static __attribute__((always_inline)) inline void dft12x8_strided_to_block_major_300(const c16_t *src,
                                                                                     int input_stride,
                                                                                     simde__m256i *leafv,
                                                                                     simde__m128i *tailv,
                                                                                     int first,
                                                                                     int valid,
                                                                                     dft_dir_t dir)
{
  simde__m256i x[12], a[4][3], y[12];
  const simde__m256i mask = complex_lane_mask_epi32(valid);
  const simde__m256i zero = simde_mm256_setzero_si256();

  for (int n = 0; n < 12; n++) {
    const int *p = (const int *)(src + n * input_stride + first);
    x[n] = valid == 8 ? simde_mm256_loadu_si256((const simde__m256i *)p) : simde_mm256_maskload_epi32(p, mask);
    if (valid == 0)
      x[n] = zero;
  }

  DFT_R3_FOLDED_LEAF_STAGE(4, 12, x[n1], x[n1 + 4], x[n1 + 8], dft12_leaf_twiddle_re, dft12_leaf_twiddle_im_fwd)

  DFT12_FOLDED_SECOND_STAGE(dft4x8_raw_q15_256)

  {
    simde__m256i z0 = y[0], z1 = y[1], z2 = y[2], z3 = y[3];
    simde__m256i z4 = y[4], z5 = y[5], z6 = y[6], z7 = y[7];
    transpose8_complex_i16_256(&z0, &z1, &z2, &z3, &z4, &z5, &z6, &z7);
    const simde__m256i rows[8] = {z0, z1, z2, z3, z4, z5, z6, z7};
    for (int lane = 0; lane < valid; lane++)
      leafv[first + lane] = rows[lane];
  }

  {
    simde__m128i lo0 = simde_mm256_castsi256_si128(y[8]);
    simde__m128i lo1 = simde_mm256_castsi256_si128(y[9]);
    simde__m128i lo2 = simde_mm256_castsi256_si128(y[10]);
    simde__m128i lo3 = simde_mm256_castsi256_si128(y[11]);
    transpose4_complex_i16_128(&lo0, &lo1, &lo2, &lo3);
    const simde__m128i lo_rows[4] = {lo0, lo1, lo2, lo3};

    for (int lane = 0; lane < valid && lane < 4; lane++)
      tailv[first + lane] = lo_rows[lane];

    if (valid > 4) {
      simde__m128i hi0 = simde_mm256_extracti128_si256(y[8], 1);
      simde__m128i hi1 = simde_mm256_extracti128_si256(y[9], 1);
      simde__m128i hi2 = simde_mm256_extracti128_si256(y[10], 1);
      simde__m128i hi3 = simde_mm256_extracti128_si256(y[11], 1);
      transpose4_complex_i16_128(&hi0, &hi1, &hi2, &hi3);
      const simde__m128i hi_rows[4] = {hi0, hi1, hi2, hi3};

      for (int lane = 4; lane < valid; lane++)
        tailv[first + lane] = hi_rows[lane - 4];
    }
  }
}

static __attribute__((always_inline)) inline void load_batched_leaf_pointers(const c16_t *const p[8],
                                                                             int valid,
                                                                             int leaf_n,
                                                                             simde__m256i *x)
{
  const simde__m256i zero = simde_mm256_setzero_si256();
  for (int off = 0; off < leaf_n; off += 8) {
    simde__m256i z[8];
    for (int lane = 0; lane < 8; lane++)
      z[lane] = lane < valid ? load_complex8_partial(p[lane] + off, leaf_n - off >= 8 ? 8 : 4) : zero;
    transpose8_complex_i16_256(&z[0], &z[1], &z[2], &z[3], &z[4], &z[5], &z[6], &z[7]);
    for (int lane = 0; lane < 8; lane++)
      x[off + lane] = z[lane];
  }
}
static __attribute__((always_inline)) inline void store_batched_leaf_pointer_outputs(c16_t *dst,
                                                                                     int R,
                                                                                     int branch,
                                                                                     int valid,
                                                                                     int leaf_n,
                                                                                     const simde__m256i *y)
{
  for (int k = 0; k < leaf_n; k++)
    store_complex_lanes_256(dst + R * k + branch, y[k], valid);
}
static __attribute__((always_inline)) inline void dft12x8_pointer_batch_store(const c16_t *const p[8],
                                                                              c16_t *dst,
                                                                              int R,
                                                                              int branch,
                                                                              int valid,
                                                                              dft_dir_t dir)
{
  simde__m256i x[16], a[4][3], y[12];
  load_batched_leaf_pointers(p, valid, 12, x);
  DFT_R3_FOLDED_LEAF_STAGE(4, 12, x[n1], x[n1 + 4], x[n1 + 8], dft12_leaf_twiddle_re, dft12_leaf_twiddle_im_fwd)
  DFT12_FOLDED_SECOND_STAGE(dft4x8_raw_q15_256) store_batched_leaf_pointer_outputs(dst, R, branch, valid, 12, y);
}

DEFINE_UNITARY_DFT4(dft4x8_q15_256_exact,
                    __attribute__((always_inline)),
                    simde__m256i,
                    half_q15_256,
                    simde_mm256_adds_epi16,
                    simde_mm256_subs_epi16,
                    simde_mm256_add_epi16,
                    simde_mm256_sub_epi16,
                    add_pm_j_i16_256)

DEFINE_UNITARY_DFT8(dft8x8_q15_256_unitary_exact,
                    __attribute__((always_inline)),
                    simde__m256i,
                    DFT8_ARRAY_OUTPUTS,
                    dft4x8_q15_256_exact,
                    const simde__m256i qinv = simde_mm256_set1_epi16(Q15_INV_SQRT2);
                    , DFT8_SCALE_256, complex_mul8_bcast_q15_256_parent, DFT8_ARRAY, simde_mm256_adds_epi16, simde_mm256_subs_epi16)

static inline void dft8x8_q15_256_parent(const simde__m256i x0,
                                         const simde__m256i x1,
                                         const simde__m256i x2,
                                         const simde__m256i x3,
                                         const simde__m256i x4,
                                         const simde__m256i x5,
                                         const simde__m256i x6,
                                         const simde__m256i x7,
                                         simde__m256i Y[8],
                                         dft_dir_t dir)
{
  /* The DFT32 decomposition requires a unitary DFT8. Its inputs come from the radix-9 stage, whose peaks
   * can exceed half full scale, so they are halved (rounded) before the first layer and the core applies
   * the remaining 1/sqrt(2) after it: no intermediate value exceeds the input peak times sqrt(2). */
  dft8x8_q15_256_core(half_q15_256(x0),
                      half_q15_256(x1),
                      half_q15_256(x2),
                      half_q15_256(x3),
                      half_q15_256(x4),
                      half_q15_256(x5),
                      half_q15_256(x6),
                      half_q15_256(x7),
                      &Y[0],
                      &Y[1],
                      &Y[2],
                      &Y[3],
                      &Y[4],
                      &Y[5],
                      &Y[6],
                      &Y[7],
                      dir,
                      Q15_INV_SQRT2,
                      NULL,
                      NULL);
}

static __attribute__((always_inline)) inline void dft32_front_q15_256_parent(const c16_t *src,
                                                                             const simde__m256i W1_RE,
                                                                             const simde__m256i W1_IM,
                                                                             const simde__m256i W2_RE,
                                                                             const simde__m256i W2_IM,
                                                                             const simde__m256i W3_RE,
                                                                             const simde__m256i W3_IM,
                                                                             simde__m256i *H0,
                                                                             simde__m256i *H1,
                                                                             simde__m256i *H2,
                                                                             simde__m256i *H3,
                                                                             dft_dir_t dir)
{
  const simde__m256i x0 = simde_mm256_loadu_si256((const simde__m256i *)(src + 0));
  const simde__m256i x1 = simde_mm256_loadu_si256((const simde__m256i *)(src + 8));
  const simde__m256i x2 = simde_mm256_loadu_si256((const simde__m256i *)(src + 16));
  const simde__m256i x3 = simde_mm256_loadu_si256((const simde__m256i *)(src + 24));

  dft4x4_q15_256_lanes(x0, x1, x2, x3, H0, H1, H2, H3, dir);

  *H1 = complex_mul8_prepack_q15_256(*H1, W1_RE, W1_IM);
  *H2 = complex_mul8_prepack_q15_256(*H2, W2_RE, W2_IM);
  *H3 = complex_mul8_prepack_q15_256(*H3, W3_RE, W3_IM);

  transpose4_complex_i16_256x2_shuffle(H0, H1, H2, H3);
}

/* Two DFT32s share the W32 coefficient loads. Their four parallel DFT8
 * tails are packed into the two 128-bit lanes of YMM registers and executed
 * together, so the final radix-8 stage handles eight transforms per AVX2 op. */
static __attribute__((always_inline)) inline void dft32x2_q15_256_parent(const c16_t *src0,
                                                                         const c16_t *src1,
                                                                         simde__m256i Y[8],
                                                                         dft_dir_t dir)
{
  const simde__m256i W1_RE = simde_mm256_load_si256((const simde__m256i *)W32_1_RE_RE_256);
  const simde__m256i W1_IM = twiddle_im_dir_256(simde_mm256_load_si256((const simde__m256i *)W32_1_IM_SIGNED_256), dir);
  const simde__m256i W2_RE = simde_mm256_load_si256((const simde__m256i *)W32_2_RE_RE_256);
  const simde__m256i W2_IM = twiddle_im_dir_256(simde_mm256_load_si256((const simde__m256i *)W32_2_IM_SIGNED_256), dir);
  const simde__m256i W3_RE = simde_mm256_load_si256((const simde__m256i *)W32_3_RE_RE_256);
  const simde__m256i W3_IM = twiddle_im_dir_256(simde_mm256_load_si256((const simde__m256i *)W32_3_IM_SIGNED_256), dir);

  simde__m256i A0, A1, A2, A3;
  simde__m256i B0, B1, B2, B3;

  dft32_front_q15_256_parent(src0, W1_RE, W1_IM, W2_RE, W2_IM, W3_RE, W3_IM, &A0, &A1, &A2, &A3, dir);
  dft32_front_q15_256_parent(src1, W1_RE, W1_IM, W2_RE, W2_IM, W3_RE, W3_IM, &B0, &B1, &B2, &B3, dir);

  const simde__m256i x0 = simde_mm256_permute2x128_si256(A0, B0, 0x20);
  const simde__m256i x1 = simde_mm256_permute2x128_si256(A1, B1, 0x20);
  const simde__m256i x2 = simde_mm256_permute2x128_si256(A2, B2, 0x20);
  const simde__m256i x3 = simde_mm256_permute2x128_si256(A3, B3, 0x20);
  const simde__m256i x4 = simde_mm256_permute2x128_si256(A0, B0, 0x31);
  const simde__m256i x5 = simde_mm256_permute2x128_si256(A1, B1, 0x31);
  const simde__m256i x6 = simde_mm256_permute2x128_si256(A2, B2, 0x31);
  const simde__m256i x7 = simde_mm256_permute2x128_si256(A3, B3, 0x31);

  dft8x8_q15_256_parent(x0, x1, x2, x3, x4, x5, x6, x7, Y, dir);
}

/* Four DFT32 branches are packed into the final interleaved R9 layout. Two adjacent four-output blocks are
 * transposed in parallel in the two 128-bit lanes of one YMM register. */
static __attribute__((always_inline)) inline void dft32x4_parent_store_body(const c16_t *src0,
                                                                            const c16_t *src1,
                                                                            const c16_t *src2,
                                                                            const c16_t *src3,
                                                                            c16_t *dst,
                                                                            int output_stride,
                                                                            int parent_branch,
                                                                            dft_dir_t dir)
{
  simde__m256i ab[8] __attribute__((aligned(32)));
  simde__m256i cd[8] __attribute__((aligned(32)));

  dft32x2_q15_256_parent(src0, src1, ab, dir);
  dft32x2_q15_256_parent(src2, src3, cd, dir);

  for (int block = 0; block < 8; block += 2) {
    simde__m256i A = simde_mm256_permute2x128_si256(ab[block], ab[block + 1], 0x20);
    simde__m256i B = simde_mm256_permute2x128_si256(ab[block], ab[block + 1], 0x31);
    simde__m256i C = simde_mm256_permute2x128_si256(cd[block], cd[block + 1], 0x20);
    simde__m256i D = simde_mm256_permute2x128_si256(cd[block], cd[block + 1], 0x31);

    transpose4_complex_i16_256x2_shuffle(&A, &B, &C, &D);

    const int k0 = 4 * block;

#define DFT32X4_STORE_ROW(ROW, KOFF)                                                            \
  do {                                                                                          \
    simde_mm_storeu_si128((simde__m128i *)(dst + output_stride * ((KOFF) + 0) + parent_branch), \
                          simde_mm256_castsi256_si128((ROW)));                                  \
    simde_mm_storeu_si128((simde__m128i *)(dst + output_stride * ((KOFF) + 4) + parent_branch), \
                          simde_mm256_extracti128_si256((ROW), 1));                             \
  } while (0)

    DFT32X4_STORE_ROW(A, k0);
    DFT32X4_STORE_ROW(B, k0 + 1);
    DFT32X4_STORE_ROW(C, k0 + 2);
    DFT32X4_STORE_ROW(D, k0 + 3);

#undef DFT32X4_STORE_ROW
  }
}

/* One copy per direction, so the direction is a constant inside. */
static __attribute__((noinline)) void dft32x4_parent_store(const c16_t *src0,
                                                           const c16_t *src1,
                                                           const c16_t *src2,
                                                           const c16_t *src3,
                                                           c16_t *dst,
                                                           int output_stride,
                                                           int parent_branch,
                                                           dft_dir_t dir)
{
  if (dir == DFT_DIR_FORWARD)
    dft32x4_parent_store_body(src0, src1, src2, src3, dst, output_stride, parent_branch, DFT_DIR_FORWARD);
  else
    dft32x4_parent_store_body(src0, src1, src2, src3, dst, output_stride, parent_branch, DFT_DIR_INVERSE);
}

static void dft72_radix9_leaf8_batch8(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { N = 72, R = 9, M = 8 };
  c16_t b[N] __attribute__((aligned(64)));

  const r3_twiddle_t *twB = r3_twiddle_get(N), *twA = r3_twiddle_get(N / 3);
  AssertFatal(twB && twA, "Missing radix-9 twiddles N=%d\n", N);

  radix9_stage_to_branch_major_q15_256(src, b, N, dir, twB, twA, 1);

  dft8x8_twiddle_scaled_store(b, M, dst, R, 0, 8, dir);
  dft8x8_twiddle_scaled_store(b, M, dst, R, 8, 1, dir);
}

static __attribute__((always_inline)) inline void radix9_terminal_leaf32(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { N = 288, M = 32 };
  c16_t b[N] __attribute__((aligned(64)));
  c16_t tail[M] __attribute__((aligned(64)));

  const r3_twiddle_t *twB = r3_twiddle_get(N), *twA = r3_twiddle_get(N / 3);
  AssertFatal(twB && twA, "Missing radix-9 twiddles N=%d\n", N);
  radix9_stage_to_branch_major_q15_256_288(src, b, dir, twB, twA);
  dft32x4_parent_store(b + 0 * M, b + 1 * M, b + 2 * M, b + 3 * M, dst, 9, 0, dir);
  dft32x4_parent_store(b + 4 * M, b + 5 * M, b + 6 * M, b + 7 * M, dst, 9, 4, dir);

  dft32_q15_128(b + 8 * M, tail, dir);
  for (int k = 0; k < M; k++)
    dst[9 * k + 8] = tail[k];
}

static void radix9_terminal_leaf12(const c16_t *src, c16_t *dst, dft_dir_t dir, const int raw_input)
{
  enum { N = 108, R = 9, M = 12 };
  c16_t b[N] __attribute__((aligned(64)));
  const r3_twiddle_t *twB = r3_twiddle_get(N), *twA = r3_twiddle_get(N / 3);
  AssertFatal(twB && twA, "Missing radix-9 twiddles N=%d\n", N);
  radix9_stage_to_branch_major_q15_256_128_hybrid_108(src, b, dir, twB, twA, raw_input);
  dft12x8_twiddle_scaled_store(b, M, dst, R, 0, 8, dir);
  dft12x4_twiddle_scaled_store(b, M, dst, R, 8, 1, dir);
}

static __attribute__((always_inline)) inline void radix9_stage_head_q15_256_108(const c16_t *src,
                                                                                c16_t *b,
                                                                                dft_dir_t dir,
                                                                                const r3_twiddle_t *twB,
                                                                                const r3_twiddle_t *twA)
{
  enum { M = 12 };
  DFT_BIND_R3_TWIDDLES(twB, dir, B1r128, B1i128, B2r128, B2i128);
  DFT_BIND_R3_UNIT_TWIDDLES(twA, dir, A1r128, A1i128, A2r128, A2i128);

  simde__m256i s00, s01, s02, s10, s11, s12, s20, s21, s22;

#define R9_108_PAIR_HEAD_FIRST(AIDX, O0, O1, O2)                                                                          \
  do {                                                                                                                    \
    const int first_off = (AIDX) * M;                                                                                     \
    const int tidx = first_off >> 2;                                                                                      \
    simde__m256i z0, z1, z2;                                                                                              \
    radix3_butterfly8_q15_256(inv_sqrt3_q15_256(simde_mm256_loadu_si256((const simde__m256i *)(src + ((AIDX) + 0) * M))), \
                              inv_sqrt3_q15_256(simde_mm256_loadu_si256((const simde__m256i *)(src + ((AIDX) + 3) * M))), \
                              inv_sqrt3_q15_256(simde_mm256_loadu_si256((const simde__m256i *)(src + ((AIDX) + 6) * M))), \
                              &z0,                                                                                        \
                              &z1,                                                                                        \
                              &z2,                                                                                        \
                              dir);                                                                                       \
    (O0) = inv_sqrt3_q15_256(z0);                                                                                         \
    (O1) = complex_mul8_prepack_small_q15_256(z1,                                                                         \
                                              simde_mm256_loadu_si256((const simde__m256i *)(B1r128 + tidx)),             \
                                              simde_mm256_loadu_si256((const simde__m256i *)(B1i128 + tidx)));            \
    (O2) = complex_mul8_prepack_small_q15_256(z2,                                                                         \
                                              simde_mm256_loadu_si256((const simde__m256i *)(B2r128 + tidx)),             \
                                              simde_mm256_loadu_si256((const simde__m256i *)(B2i128 + tidx)));            \
  } while (0)

  R9_108_PAIR_HEAD_FIRST(0, s00, s01, s02);
  R9_108_PAIR_HEAD_FIRST(1, s10, s11, s12);
  R9_108_PAIR_HEAD_FIRST(2, s20, s21, s22);
#undef R9_108_PAIR_HEAD_FIRST

  const simde__m256i A1r = simde_mm256_loadu_si256((const simde__m256i *)A1r128);
  const simde__m256i A1i = simde_mm256_loadu_si256((const simde__m256i *)A1i128);
  const simde__m256i A2r = simde_mm256_loadu_si256((const simde__m256i *)A2r128);
  const simde__m256i A2i = simde_mm256_loadu_si256((const simde__m256i *)A2i128);

#define R9_108_PAIR_HEAD_SECOND(BETA, X0, X1, X2)                             \
  do {                                                                        \
    simde__m256i z0, z1, z2;                                                  \
    radix3_butterfly8_q15_256((X0), (X1), (X2), &z0, &z1, &z2, dir);          \
    z1 = complex_mul8_prepack_q15_256(z1, A1r, A1i);                          \
    z2 = complex_mul8_prepack_q15_256(z2, A2r, A2i);                          \
    simde_mm256_storeu_si256((simde__m256i *)(b + ((BETA) + 0 * 3) * M), z0); \
    simde_mm256_storeu_si256((simde__m256i *)(b + ((BETA) + 1 * 3) * M), z1); \
    simde_mm256_storeu_si256((simde__m256i *)(b + ((BETA) + 2 * 3) * M), z2); \
  } while (0)

  R9_108_PAIR_HEAD_SECOND(0, s00, s10, s20);
  R9_108_PAIR_HEAD_SECOND(1, s01, s11, s21);
  R9_108_PAIR_HEAD_SECOND(2, s02, s12, s22);
#undef R9_108_PAIR_HEAD_SECOND
}

static __attribute__((always_inline)) inline void radix9_stage_tail_q15_256_pair_108(const c16_t *src0,
                                                                                     c16_t *b0,
                                                                                     const c16_t *src1,
                                                                                     c16_t *b1,
                                                                                     dft_dir_t dir,
                                                                                     const r3_twiddle_t *twB,
                                                                                     const r3_twiddle_t *twA)
{
  enum { M = 12, OFF = 8 };
  DFT_BIND_R3_TWIDDLES(twB, dir, B1r128, B1i128, B2r128, B2i128);
  DFT_BIND_R3_UNIT_TWIDDLES(twA, dir, A1r128, A1i128, A2r128, A2i128);
  simde__m256i s00, s01, s02, s10, s11, s12, s20, s21, s22;

#define R9_108_PAIR_LOAD(AIDX, DELTA)                                                                \
  join_m128_lanes(simde_mm_loadu_si128((const simde__m128i *)(src0 + ((AIDX) + (DELTA)) * M + OFF)), \
                  simde_mm_loadu_si128((const simde__m128i *)(src1 + ((AIDX) + (DELTA)) * M + OFF)))

#define R9_108_PAIR_TAIL_FIRST(AIDX, O0, O1, O2)                                               \
  do {                                                                                         \
    const int first_off = (AIDX) * M + OFF;                                                    \
    const int tidx = first_off >> 2;                                                           \
    simde__m256i z0, z1, z2;                                                                   \
    radix3_butterfly8_q15_256(inv_sqrt3_q15_256(R9_108_PAIR_LOAD((AIDX), 0)),                  \
                              inv_sqrt3_q15_256(R9_108_PAIR_LOAD((AIDX), 3)),                  \
                              inv_sqrt3_q15_256(R9_108_PAIR_LOAD((AIDX), 6)),                  \
                              &z0,                                                             \
                              &z1,                                                             \
                              &z2,                                                             \
                              dir);                                                            \
    (O0) = inv_sqrt3_q15_256(z0);                                                              \
    (O1) = complex_mul8_prepack_small_q15_256(z1,                                              \
                                              simde_mm256_broadcastsi128_si256(B1r128[tidx]),  \
                                              simde_mm256_broadcastsi128_si256(B1i128[tidx])); \
    (O2) = complex_mul8_prepack_small_q15_256(z2,                                              \
                                              simde_mm256_broadcastsi128_si256(B2r128[tidx]),  \
                                              simde_mm256_broadcastsi128_si256(B2i128[tidx])); \
  } while (0)

  R9_108_PAIR_TAIL_FIRST(0, s00, s01, s02);
  R9_108_PAIR_TAIL_FIRST(1, s10, s11, s12);
  R9_108_PAIR_TAIL_FIRST(2, s20, s21, s22);
#undef R9_108_PAIR_TAIL_FIRST
#undef R9_108_PAIR_LOAD

  const simde__m256i A1r = simde_mm256_broadcastsi128_si256(A1r128[OFF >> 2]);
  const simde__m256i A1i = simde_mm256_broadcastsi128_si256(A1i128[OFF >> 2]);
  const simde__m256i A2r = simde_mm256_broadcastsi128_si256(A2r128[OFF >> 2]);
  const simde__m256i A2i = simde_mm256_broadcastsi128_si256(A2i128[OFF >> 2]);

#define R9_108_PAIR_STORE(BETA, V, GROUP)                                                                                  \
  do {                                                                                                                     \
    simde_mm_storeu_si128((simde__m128i *)(b0 + ((BETA) + (GROUP) * 3) * M + OFF), simde_mm256_castsi256_si128(V));        \
    simde_mm_storeu_si128((simde__m128i *)(b1 + ((BETA) + (GROUP) * 3) * M + OFF), simde_mm256_extracti128_si256((V), 1)); \
  } while (0)

#define R9_108_PAIR_TAIL_SECOND(BETA, X0, X1, X2)                    \
  do {                                                               \
    simde__m256i z0, z1, z2;                                         \
    radix3_butterfly8_q15_256((X0), (X1), (X2), &z0, &z1, &z2, dir); \
    z1 = complex_mul8_prepack_q15_256(z1, A1r, A1i);                 \
    z2 = complex_mul8_prepack_q15_256(z2, A2r, A2i);                 \
    R9_108_PAIR_STORE((BETA), z0, 0);                                \
    R9_108_PAIR_STORE((BETA), z1, 1);                                \
    R9_108_PAIR_STORE((BETA), z2, 2);                                \
  } while (0)

  R9_108_PAIR_TAIL_SECOND(0, s00, s10, s20);
  R9_108_PAIR_TAIL_SECOND(1, s01, s11, s21);
  R9_108_PAIR_TAIL_SECOND(2, s02, s12, s22);
#undef R9_108_PAIR_TAIL_SECOND
#undef R9_108_PAIR_STORE
}

static __attribute__((always_inline)) inline void radix81_dft8x8_parent_store(const c16_t *src0,
                                                                              const c16_t *src1,
                                                                              const c16_t *src2,
                                                                              const c16_t *src3,
                                                                              const c16_t *src4,
                                                                              const c16_t *src5,
                                                                              const c16_t *src6,
                                                                              const c16_t *src7,
                                                                              c16_t *dst,
                                                                              int parent_branch,
                                                                              dft_dir_t dir)
{
  simde__m256i x0 = simde_mm256_load_si256((const simde__m256i *)src0);
  simde__m256i x1 = simde_mm256_load_si256((const simde__m256i *)src1);
  simde__m256i x2 = simde_mm256_load_si256((const simde__m256i *)src2);
  simde__m256i x3 = simde_mm256_load_si256((const simde__m256i *)src3);
  simde__m256i x4 = simde_mm256_load_si256((const simde__m256i *)src4);
  simde__m256i x5 = simde_mm256_load_si256((const simde__m256i *)src5);
  simde__m256i x6 = simde_mm256_load_si256((const simde__m256i *)src6);
  simde__m256i x7 = simde_mm256_load_si256((const simde__m256i *)src7);

  transpose8_complex_i16_256(&x0, &x1, &x2, &x3, &x4, &x5, &x6, &x7);

  simde__m256i y[8];
  dft8x8_q15_256_unitary_exact(x0, x1, x2, x3, x4, x5, x6, x7, y, dir);

  for (int k = 0; k < 8; k++)
    simde_mm256_storeu_si256((simde__m256i *)(dst + 81 * k + parent_branch), y[k]);
}

#define DFT_RADIX81_PARENT_STAGES()                                                             \
  const r3_twiddle_t *twB = r3_twiddle_get(N), *twA = r3_twiddle_get(N / 3);                    \
  const r3_twiddle_t *tw_inB = r3_twiddle_get(M1), *tw_inA = r3_twiddle_get(M1 / 3);            \
  AssertFatal(twB && twA && tw_inB && tw_inA, "Missing radix-81 twiddles N=%d M1=%d\n", N, M1); \
  radix9_stage_to_branch_major_q15_256(src, outer_b, N, dir, twB, twA, raw_input);              \
  for (int outer = 0; outer < 9; outer++)                                                       \
    radix9_stage_to_branch_major_q15_256(outer_b + outer * M1, stage2 + outer * M1, M1, dir, tw_inB, tw_inA, 0);

#define DFT_RADIX81_CHILD_VIEWS()                 \
  const int branch = 9 * inner;                   \
  const c16_t *p0 = stage2 + 0 * M1 + inner * M2; \
  const c16_t *p1 = stage2 + 1 * M1 + inner * M2; \
  const c16_t *p2 = stage2 + 2 * M1 + inner * M2; \
  const c16_t *p3 = stage2 + 3 * M1 + inner * M2; \
  const c16_t *p4 = stage2 + 4 * M1 + inner * M2; \
  const c16_t *p5 = stage2 + 5 * M1 + inner * M2; \
  const c16_t *p6 = stage2 + 6 * M1 + inner * M2; \
  const c16_t *p7 = stage2 + 7 * M1 + inner * M2; \
  const c16_t *p8 = stage2 + 8 * M1 + inner * M2;

static void radix81_terminal_leaf8(const c16_t *src, c16_t *dst, dft_dir_t dir, const int raw_input)
{
  enum { N = 648, M1 = 72, M2 = 8 };
  c16_t work[2 * N] __attribute__((aligned(64)));
  c16_t *outer_b = work;
  c16_t *stage2 = work + N;
  c16_t tail[M2] __attribute__((aligned(16)));

  DFT_RADIX81_PARENT_STAGES()
  for (int inner = 0; inner < 9; inner++) {
    DFT_RADIX81_CHILD_VIEWS()

    radix81_dft8x8_parent_store(p0, p1, p2, p3, p4, p5, p6, p7, dst, branch, dir);
    dft8_avx(p8, tail, dir);
    for (int k = 0; k < M2; k++)
      dst[81 * k + branch + 8] = tail[k];
  }
}

static void radix81_terminal_leaf16_w16folded(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  const int raw_input = 1;
  enum { N = 1296, M1 = 144, M2 = 16 };
  c16_t work[2 * N] __attribute__((aligned(64)));
  c16_t *outer_b = work;
  c16_t *stage2 = work + N;
  c16_t tail[M2] __attribute__((aligned(16)));

  DFT_RADIX81_PARENT_STAGES()

  for (int inner = 0; inner < 9; inner++) {
    DFT_RADIX81_CHILD_VIEWS()

    simde__m256i x[16] __attribute__((aligned(64)));
    simde__m256i y[16] __attribute__((aligned(64)));

    simde__m256i z0 = simde_mm256_loadu_si256((const simde__m256i *)(p0 + 0));
    simde__m256i z1 = simde_mm256_loadu_si256((const simde__m256i *)(p1 + 0));
    simde__m256i z2 = simde_mm256_loadu_si256((const simde__m256i *)(p2 + 0));
    simde__m256i z3 = simde_mm256_loadu_si256((const simde__m256i *)(p3 + 0));
    simde__m256i z4 = simde_mm256_loadu_si256((const simde__m256i *)(p4 + 0));
    simde__m256i z5 = simde_mm256_loadu_si256((const simde__m256i *)(p5 + 0));
    simde__m256i z6 = simde_mm256_loadu_si256((const simde__m256i *)(p6 + 0));
    simde__m256i z7 = simde_mm256_loadu_si256((const simde__m256i *)(p7 + 0));
    transpose8_complex_i16_256(&z0, &z1, &z2, &z3, &z4, &z5, &z6, &z7);
    x[0] = z0;
    x[1] = z1;
    x[2] = z2;
    x[3] = z3;
    x[4] = z4;
    x[5] = z5;
    x[6] = z6;
    x[7] = z7;

    z0 = simde_mm256_loadu_si256((const simde__m256i *)(p0 + 8));
    z1 = simde_mm256_loadu_si256((const simde__m256i *)(p1 + 8));
    z2 = simde_mm256_loadu_si256((const simde__m256i *)(p2 + 8));
    z3 = simde_mm256_loadu_si256((const simde__m256i *)(p3 + 8));
    z4 = simde_mm256_loadu_si256((const simde__m256i *)(p4 + 8));
    z5 = simde_mm256_loadu_si256((const simde__m256i *)(p5 + 8));
    z6 = simde_mm256_loadu_si256((const simde__m256i *)(p6 + 8));
    z7 = simde_mm256_loadu_si256((const simde__m256i *)(p7 + 8));
    transpose8_complex_i16_256(&z0, &z1, &z2, &z3, &z4, &z5, &z6, &z7);
    x[8] = z0;
    x[9] = z1;
    x[10] = z2;
    x[11] = z3;
    x[12] = z4;
    x[13] = z5;
    x[14] = z6;
    x[15] = z7;

    dft16x8_prescaled(x, y, dir); /* radix-81 outputs: peaks above half full scale */
    for (int k = 0; k < M2; k++) {
      simde_mm256_storeu_si256((simde__m256i *)(dst + 81 * k + branch), y[k]);
    }

    dft16_q15_128(p8, tail, dir);
    for (int k = 0; k < M2; k++)
      dst[81 * k + branch + 8] = tail[k];
  }
}

static void radix81_terminal_leaf32_2592(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  const int raw_input = 1;
  enum { N = 2592, M1 = 288, M2 = 32 };
  c16_t work[2 * N] __attribute__((aligned(64)));
  c16_t *outer_b = work;
  c16_t *stage2 = work + N;
  c16_t tail[M2] __attribute__((aligned(64)));

  DFT_RADIX81_PARENT_STAGES()
  for (int inner = 0; inner < 9; inner++) {
    DFT_RADIX81_CHILD_VIEWS()

    dft32x4_parent_store(p0, p1, p2, p3, dst, 81, branch + 0, dir);
    dft32x4_parent_store(p4, p5, p6, p7, dst, 81, branch + 4, dir);

    dft32_q15_128(p8, tail, dir);
    for (int k = 0; k < M2; k++)
      dst[81 * k + branch + 8] = tail[k];
  }
}

static void dft_radix9_or_radix18(const c16_t *src, c16_t *dst, int N, dft_dir_t dir, const int raw_input)
{
  const int M = N / 9;
  AssertFatal((N % 9) == 0 && (M & 3) == 0, "Invalid radix-9 selected N=%d M=%d\n", N, M);

  if (N == 1152) {
    dft1152_radix18_leaf64(src, dst, dir);
    return;
  }

  const r3_twiddle_t *twB = r3_twiddle_get(N), *twA = r3_twiddle_get(N / 3);
  AssertFatal(twB && twA, "Missing radix-9 twiddles N=%d\n", N);

  /* DFT576 uses an AVX2 radix-9 parent. Branches 0..7 are packed directly
   * for the lane-parallel DFT64x8 leaf; branch 8 uses one DFT64 leaf. */
  if (N == 576) {
    simde__m256i leaf[64] __attribute__((aligned(64)));
    c16_t branch8[64] __attribute__((aligned(64)));
    c16_t tail[64] __attribute__((aligned(64)));

    radix9_stage_to_dft64x8_576(src, leaf, branch8, dir, twB, twA, raw_input);

    dft64x8_store_with_dc(leaf, dst, 9, 0, dir);

    dft64_avx(branch8, tail, dir);
    for (int k = 0; k < 64; k++)
      dst[9 * k + 8] = tail[k];
    return;
  }

  c16_t work[2 * N] __attribute__((aligned(64)));
  c16_t *b = work;
  c16_t *y = work + N;

  /* The N=576 and N=1152 paths return above; this is N=2304. */
  radix9_stage_to_branch_major_q15_256(src, b, N, dir, twB, twA, raw_input);

  for (int br = 0; br < 9; br++)
    dft256_radix16(b + br * M, y + br * M, dir);

  for (int k = 0; k < M; k += 8)
    radix9_scatter8_q15_256(y, dst, M, k);
}

static inline void dft_power2_child(const c16_t *src, c16_t *dst, int N, dft_dir_t dir, c16_t *work)
{
  switch (N) {
    case 8:
      dft8_avx(src, dst, dir);
      return;
    case 16:
      dft16_q15_128(src, dst, dir);
      return;
    case 32:
      dft32_q15_128(src, dst, dir);
      return;
    case 64:
      dft64_avx(src, dst, dir);
      return;
    case 128:
      dft128_dir(src, dst, dir);
      return;
    case 256:
      /* Nested DFT256 uses the unfused W16 scaling order required by its
       * parent path; the contiguous entry uses folded W16 scaling. */
      dft256_radix16(src, dst, dir);
      return;
    default:
      dft_power2_mixed_large_core(src, dst, N, dir, work);
      return;
  }
}
