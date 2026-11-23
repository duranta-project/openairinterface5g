/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* Internal part of oai_dfts.c: radix-25 parents (900..3000) and their leaves.
 * Included only by oai_dfts.c, in order; not a public header. */

#ifndef OAI_DFTS_INTERNAL
#error "oai_dfts_radix25.h is included only by oai_dfts.c"
#endif

/* R25 tiles load their inputs from contiguous, block-major or partial-leaf layouts. */
#define DFT_R25_LOAD_CONTIGUOUS_128(POS)                                                               \
  const simde__m128i x0 = simde_mm_loadu_si128((const simde__m128i *)(leaf + (r0 + 0 * 5) * M + POS)); \
  const simde__m128i x1 = simde_mm_loadu_si128((const simde__m128i *)(leaf + (r0 + 1 * 5) * M + POS)); \
  const simde__m128i x2 = simde_mm_loadu_si128((const simde__m128i *)(leaf + (r0 + 2 * 5) * M + POS)); \
  const simde__m128i x3 = simde_mm_loadu_si128((const simde__m128i *)(leaf + (r0 + 3 * 5) * M + POS)); \
  const simde__m128i x4 = simde_mm_loadu_si128((const simde__m128i *)(leaf + (r0 + 4 * 5) * M + POS));

#define DFT_R25_LOAD_CONTIGUOUS_256(POS)                                                                  \
  const simde__m256i x0 = simde_mm256_loadu_si256((const simde__m256i *)(leaf + (r0 + 0 * 5) * M + POS)); \
  const simde__m256i x1 = simde_mm256_loadu_si256((const simde__m256i *)(leaf + (r0 + 1 * 5) * M + POS)); \
  const simde__m256i x2 = simde_mm256_loadu_si256((const simde__m256i *)(leaf + (r0 + 2 * 5) * M + POS)); \
  const simde__m256i x3 = simde_mm256_loadu_si256((const simde__m256i *)(leaf + (r0 + 3 * 5) * M + POS)); \
  const simde__m256i x4 = simde_mm256_loadu_si256((const simde__m256i *)(leaf + (r0 + 4 * 5) * M + POS));

#define DFT_R25_INPUT_REGISTERS(POS) x0, x1, x2, x3, x4

#define RADIX25_CONTIGUOUS_PARENT(N, M, INNER_N, leaf, dst)                                              \
  const r5_twiddle_t *tw_inner = r5_twiddle_get(INNER_N);                                                \
  const r5_twiddle_t *tw_outer = r5_twiddle_get(N);                                                      \
  AssertFatal(tw_inner &&tw_outer, "Missing radix-25 twiddles N=%d\n", N);                               \
  DFT_BIND_R5_TWIDDLES((tw_inner), dir, in_1r, in_1i, in_2r, in_2i, in_3r, in_3i, in_4r, in_4i);         \
  DFT_BIND_R5_TWIDDLES((tw_outer), dir, out_1r, out_1i, out_2r, out_2i, out_3r, out_3i, out_4r, out_4i); \
  int k = 0;                                                                                             \
  for (; k + 8 <= M; k += 8) {                                                                           \
    DFT_RADIX25_TILE(simde__m256i,                                                                       \
                     radix5_combine8_q15_256_fast,                                                       \
                     DFT_R25_LOAD_CONTIGUOUS_256,                                                        \
                     DFT_R25_INPUT_REGISTERS,                                                            \
                     DFT_R5_COEFFICIENTS_256,                                                            \
                     simde_mm256_storeu_si256,                                                           \
                     k)                                                                                  \
  }                                                                                                      \
  if (k < M) {                                                                                           \
    DFT_RADIX25_TILE(simde__m128i,                                                                       \
                     radix5_combine4_q15_128_fast,                                                       \
                     DFT_R25_LOAD_CONTIGUOUS_128,                                                        \
                     DFT_R25_INPUT_REGISTERS,                                                            \
                     DFT_R5_COEFFICIENTS_128,                                                            \
                     simde_mm_storeu_si128,                                                              \
                     k)                                                                                  \
  }

static __attribute__((noinline)) void dft900_radix25_dft36_pair_batch2(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { N = 900, M = 36, R = 25 };
  c16_t leaf[N] __attribute__((aligned(64)));

  const r3_twiddle_t *tw36 = r3_twiddle_get(M);
  AssertFatal(tw36, "Missing DFT36 radix-3 twiddles\n");

  dft36x8_strided_to_branch_major_900(src, R, leaf, 0, 8, tw36, dir);
  dft36x8_strided_to_branch_major_900(src, R, leaf, 8, 8, tw36, dir);
  dft36x8_strided_to_branch_major_900(src, R, leaf, 16, 8, tw36, dir);
  dft36_radix3_leaf12_strided(src + 24, R, leaf + 24 * M, dir);

  RADIX25_CONTIGUOUS_PARENT(N, M, N / 5, leaf, dst)
}

#define DFT_R5_COEFFICIENTS_BROADCAST256(P, INDEX)                                                                               \
  simde_mm256_set1_epi32(P##1r32 [INDEX]), simde_mm256_set1_epi32(P##1i32 [INDEX]), simde_mm256_set1_epi32(P##2r32 [INDEX]),     \
      simde_mm256_set1_epi32(P##2i32 [INDEX]), simde_mm256_set1_epi32(P##3r32 [INDEX]), simde_mm256_set1_epi32(P##3i32 [INDEX]), \
      simde_mm256_set1_epi32(P##4r32 [INDEX]), simde_mm256_set1_epi32(P##4i32 [INDEX])

static __attribute__((noinline)) void dft60x8_strided_to_branch_major_1500(const c16_t *src,
                                                                           int input_stride,
                                                                           c16_t *leaf,
                                                                           int first,
                                                                           const r5_twiddle_t *tw,
                                                                           dft_dir_t dir)
{
  enum { R = 5, M = 12, N = 60 };
  simde__m256i child[R][M];
  simde__m256i out[N];

  const int child_stride = input_stride * R;

  dft12x8_strided_raw_900(src + 0 * input_stride, child_stride, first, 8, child[0], dir);
  dft12x8_strided_raw_900(src + 1 * input_stride, child_stride, first, 8, child[1], dir);
  dft12x8_strided_raw_900(src + 2 * input_stride, child_stride, first, 8, child[2], dir);
  dft12x8_strided_raw_900(src + 3 * input_stride, child_stride, first, 8, child[3], dir);
  dft12x8_strided_raw_900(src + 4 * input_stride, child_stride, first, 8, child[4], dir);

  DFT_BIND_R5_TWIDDLES(tw, dir, w1r, w1i, w2r, w2i, w3r, w3i, w4r, w4i);

  const int *w1r32 = (const int *)w1r;
  const int *w1i32 = (const int *)w1i;
  const int *w2r32 = (const int *)w2r;
  const int *w2i32 = (const int *)w2i;
  const int *w3r32 = (const int *)w3r;
  const int *w3i32 = (const int *)w3i;
  const int *w4r32 = (const int *)w4r;
  const int *w4i32 = (const int *)w4i;

  for (int k = 0; k < M; k++) {
    simde__m256i y0, y1, y2, y3, y4;

    radix5_combine8_q15_256_fast(child[0][k],
                                 child[1][k],
                                 child[2][k],
                                 child[3][k],
                                 child[4][k],
                                 DFT_R5_COEFFICIENTS_BROADCAST256(w, k),
                                 &y0,
                                 &y1,
                                 &y2,
                                 &y3,
                                 &y4,
                                 dir);

    out[k + 0 * M] = y0;
    out[k + 1 * M] = y1;
    out[k + 2 * M] = y2;
    out[k + 3 * M] = y3;
    out[k + 4 * M] = y4;
  }

  /* Output bins 0..55: transpose eight frequencies x eight DFT60 leaves. */
  for (int off = 0; off < 56; off += 8) {
    DFT_LOAD_TRANSPOSE8_ROW(z, out, off)

    DFT_STORE8_STRIDED_ROWS(z, leaf, first, N, off)
  }

  /* Output bins 56..59: transpose four frequencies x eight DFT60 leaves. */
  {
    simde__m128i lo0 = simde_mm256_castsi256_si128(out[56]);
    simde__m128i lo1 = simde_mm256_castsi256_si128(out[57]);
    simde__m128i lo2 = simde_mm256_castsi256_si128(out[58]);
    simde__m128i lo3 = simde_mm256_castsi256_si128(out[59]);
    transpose4_complex_i16_128(&lo0, &lo1, &lo2, &lo3);

    simde_mm_storeu_si128((simde__m128i *)(leaf + (first + 0) * N + 56), lo0);
    simde_mm_storeu_si128((simde__m128i *)(leaf + (first + 1) * N + 56), lo1);
    simde_mm_storeu_si128((simde__m128i *)(leaf + (first + 2) * N + 56), lo2);
    simde_mm_storeu_si128((simde__m128i *)(leaf + (first + 3) * N + 56), lo3);

    simde__m128i hi0 = simde_mm256_extracti128_si256(out[56], 1);
    simde__m128i hi1 = simde_mm256_extracti128_si256(out[57], 1);
    simde__m128i hi2 = simde_mm256_extracti128_si256(out[58], 1);
    simde__m128i hi3 = simde_mm256_extracti128_si256(out[59], 1);
    transpose4_complex_i16_128(&hi0, &hi1, &hi2, &hi3);

    simde_mm_storeu_si128((simde__m128i *)(leaf + (first + 4) * N + 56), hi0);
    simde_mm_storeu_si128((simde__m128i *)(leaf + (first + 5) * N + 56), hi1);
    simde_mm_storeu_si128((simde__m128i *)(leaf + (first + 6) * N + 56), hi2);
    simde_mm_storeu_si128((simde__m128i *)(leaf + (first + 7) * N + 56), hi3);
  }
}

static __attribute__((noinline)) void dft1500_radix25_dft60x8(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { N = 1500, M = 60, R = 25 };
  c16_t leaf[N] __attribute__((aligned(64)));

  const r5_twiddle_t *tw60 = r5_twiddle_get(M);
  AssertFatal(tw60, "Missing DFT60 radix-5 twiddles\n");

  dft60x8_strided_to_branch_major_1500(src, R, leaf, 0, tw60, dir);
  dft60x8_strided_to_branch_major_1500(src, R, leaf, 8, tw60, dir);
  dft60x8_strided_to_branch_major_1500(src, R, leaf, 16, tw60, dir);

  /* Branch 24 has no full x8 group. */
  dft60_radix5_leaf12_strided(src + 24, R, leaf + 24 * M, dir);

  RADIX25_CONTIGUOUS_PARENT(N, M, N / 5, leaf, dst)
}

static __attribute__((noinline)) void dft72x8_strided_to_block_major_1800(const c16_t *src,
                                                                          simde__m256i *leafv,
                                                                          int first,
                                                                          int valid,
                                                                          dft_dir_t dir)
{
  enum { R = 25, M = 72 };
  simde__m256i x[M], r9[9][8];
  const simde__m256i s3 = simde_mm256_set1_epi16(Q15_INV_SQRT3);
  const simde__m256i mask = complex_lane_mask_epi32(valid);

  for (int n = 0; n < M; n++) {
    const int *p = (const int *)(src + n * R + first);
    x[n] = valid == 8 ? simde_mm256_loadu_si256((const simde__m256i *)p) : simde_mm256_maskload_epi32(p, mask);
  }

  DFT72_NORMALIZED_R9_STAGE() simde__m256i y[M];
  for (int br9 = 0; br9 < 9; br9++) {
    simde__m256i z[8];
    dft2160_unitary_dft8x8(r9[br9], z, dir);

    for (int k8 = 0; k8 < 8; k8++)
      y[9 * k8 + br9] = z[k8];
  }

  for (int off = 0; off < M; off += 8) {
    DFT_LOAD_TRANSPOSE8_ROW(q, y, off)

    const simde__m256i rows[8] = {q0, q1, q2, q3, q4, q5, q6, q7};
    simde__m256i *row = leafv + (off >> 3) * R + first;

    for (int lane = 0; lane < valid; lane++)
      row[lane] = rows[lane];
  }
}

#define DFT_R25_LOAD_BLOCK_MAJOR(POS)            \
  const int kb = POS >> 3;                       \
  const simde__m256i x0 = leafv[kb][r0 + 0 * 5]; \
  const simde__m256i x1 = leafv[kb][r0 + 1 * 5]; \
  const simde__m256i x2 = leafv[kb][r0 + 2 * 5]; \
  const simde__m256i x3 = leafv[kb][r0 + 3 * 5]; \
  const simde__m256i x4 = leafv[kb][r0 + 4 * 5];

#define RADIX25_BLOCK_MAJOR_PARENT(N, M, INNER_N, leafv, dst)                                            \
  const r5_twiddle_t *tw_inner = r5_twiddle_get(INNER_N);                                                \
  const r5_twiddle_t *tw_outer = r5_twiddle_get(N);                                                      \
  AssertFatal(tw_inner &&tw_outer, "Missing radix-25 twiddles N=%d\n", N);                               \
  DFT_BIND_R5_TWIDDLES((tw_inner), dir, in_1r, in_1i, in_2r, in_2i, in_3r, in_3i, in_4r, in_4i);         \
  DFT_BIND_R5_TWIDDLES((tw_outer), dir, out_1r, out_1i, out_2r, out_2i, out_3r, out_3i, out_4r, out_4i); \
  for (int k = 0; k < M; k += 8) {                                                                       \
    DFT_RADIX25_TILE(simde__m256i,                                                                       \
                     radix5_combine8_q15_256_fast,                                                       \
                     DFT_R25_LOAD_BLOCK_MAJOR,                                                           \
                     DFT_R25_INPUT_REGISTERS,                                                            \
                     DFT_R5_COEFFICIENTS_256,                                                            \
                     simde_mm256_storeu_si256,                                                           \
                     k)                                                                                  \
  }

/* DFT1800 uses the M=72 pack4 leaf layout with the shared block-major R25 parent. */

static __attribute__((noinline)) void dft1800_radix25_pack4(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { N = 1800, M = 72, R = 25, BLOCKS = M / 8 };
  simde__m256i leafv[BLOCKS][R] __attribute__((aligned(64)));

  dft72x8_strided_to_block_major_1800(src, &leafv[0][0], 0, 8, dir);
  dft72x8_strided_to_block_major_1800(src, &leafv[0][0], 8, 8, dir);
  dft72x8_strided_to_block_major_1800(src, &leafv[0][0], 16, 8, dir);
  dft72x8_strided_to_block_major_1800(src, &leafv[0][0], 24, 1, dir);

  RADIX25_BLOCK_MAJOR_PARENT(N, M, 360, leafv, dst)
}
/* Gathers four strided R25 children into packed rows (4x4 complex transpose), for the leaves of DFT2700 (M = 108)
 * and DFT3000 (M = 120). */
#define DFT_PACK_FOUR_STRIDED_CHILDREN(BOUND)                                               \
  for (int n = 0; n < BOUND; n += 4) {                                                      \
    simde__m128i r0 = simde_mm_loadu_si128((const simde__m128i *)(src + (n + 0) * R + br)); \
    simde__m128i r1 = simde_mm_loadu_si128((const simde__m128i *)(src + (n + 1) * R + br)); \
    simde__m128i r2 = simde_mm_loadu_si128((const simde__m128i *)(src + (n + 2) * R + br)); \
    simde__m128i r3 = simde_mm_loadu_si128((const simde__m128i *)(src + (n + 3) * R + br)); \
                                                                                            \
    transpose4_complex_i16_128(&r0, &r1, &r2, &r3);                                         \
                                                                                            \
    simde_mm_store_si128((simde__m128i *)(packed[0] + n), r0);                              \
    simde_mm_store_si128((simde__m128i *)(packed[1] + n), r1);                              \
    simde_mm_store_si128((simde__m128i *)(packed[2] + n), r2);                              \
    simde_mm_store_si128((simde__m128i *)(packed[3] + n), r3);                              \
  }

static __attribute__((noinline)) void radix25_compute_packed4_leaves_108_stride1(const c16_t *src, c16_t *leaf, dft_dir_t dir)
{
  enum { R = 25, N108 = 108, R9 = 9, M12 = 12 };

  /*
   * Process four independent DFT108 leaves at a time.
   *
   * The DFT108 uses the pair-batched radix-9 stage (as DFT540) and batches
   * the ninth DFT12 leaf of all four transforms in one DFT12x8 call
   * (valid = 4).
   */
  c16_t packed[4][N108] __attribute__((aligned(64)));
  c16_t stage[4 * N108] __attribute__((aligned(64)));
  c16_t tail[4 * M12] __attribute__((aligned(64)));

  const r3_twiddle_t *twB = r3_twiddle_get(N108);
  const r3_twiddle_t *twA = r3_twiddle_get(N108 / 3);
  AssertFatal(twB && twA, "Missing radix-9 twiddles N=%d\n", N108);

  int br = 0;
  for (; br + 3 < R; br += 4) {
    DFT_PACK_FOUR_STRIDED_CHILDREN(N108)

    radix9_stage_head_q15_256_108(packed[0], stage + 0 * N108, dir, twB, twA);
    radix9_stage_head_q15_256_108(packed[1], stage + 1 * N108, dir, twB, twA);
    radix9_stage_tail_q15_256_pair_108(packed[0], stage + 0 * N108, packed[1], stage + 1 * N108, dir, twB, twA);

    radix9_stage_head_q15_256_108(packed[2], stage + 2 * N108, dir, twB, twA);
    radix9_stage_head_q15_256_108(packed[3], stage + 3 * N108, dir, twB, twA);
    radix9_stage_tail_q15_256_pair_108(packed[2], stage + 2 * N108, packed[3], stage + 3 * N108, dir, twB, twA);

    /* The first eight DFT12 leaves are already a full x8 batch per DFT108. */
    for (int t = 0; t < 4; t++)
      dft12x8_twiddle_scaled_store(stage + t * N108, M12, leaf + (br + t) * N108, R9, 0, 8, dir);

    /*
     * Branch 8 is width-limited in every DFT108.  Evaluate the four transforms
     * together as four SIMD lanes.
     */

    dft12x8_twiddle_scaled_store(stage + 8 * M12, N108, tail, 4, 0, 4, dir);

    for (int k = 0; k < M12; k++)
      for (int t = 0; t < 4; t++)
        leaf[(br + t) * N108 + R9 * k + 8] = tail[4 * k + t];
  }

  for (int n = 0; n < N108; n += 4)
    simde_mm_store_si128((simde__m128i *)(packed[0] + n), load4_complex_strided_c16(src + br, R, n));
  radix9_terminal_leaf12(packed[0], leaf + br * N108, dir, 0);
}

static __attribute__((noinline)) void dft2700_radix25_pack4(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { N = 2700, M = 108 };
  c16_t leaf[N] __attribute__((aligned(64)));

  radix25_compute_packed4_leaves_108_stride1(src, leaf, dir);

  RADIX25_CONTIGUOUS_PARENT(N, M, 540, leaf, dst)
}

#undef RADIX25_CONTIGUOUS_PARENT
#undef RADIX25_BIND_R5_TWIDDLES

static __attribute__((noinline)) void radix25_compute_packed4_leaves_120_stride1(const c16_t *src, c16_t *leaf, dft_dir_t dir)
{
  enum { R = 25, M = 120 };
  c16_t packed[4][M] __attribute__((aligned(64)));
  int br = 0;

  for (; br + 3 < R; br += 4) {
    DFT_PACK_FOUR_STRIDED_CHILDREN(M)

    dft120_radix15_pfa(packed[0], leaf + (br + 0) * M, dir);
    dft120_radix15_pfa(packed[1], leaf + (br + 1) * M, dir);
    dft120_radix15_pfa(packed[2], leaf + (br + 2) * M, dir);
    dft120_radix15_pfa(packed[3], leaf + (br + 3) * M, dir);
  }

  for (int n = 0; n < M; n += 4)
    simde_mm_store_si128((simde__m128i *)(packed[0] + n), load4_complex_strided_c16(src + br, R, n));
  dft120_radix15_pfa(packed[0], leaf + br * M, dir);
}

static __attribute__((noinline)) void dft3000_radix25_pack4(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { N = 3000, M = 120 };
  c16_t leaf[N] __attribute__((aligned(64)));

  radix25_compute_packed4_leaves_120_stride1(src, leaf, dir);

  const r5_twiddle_t *tw_inner = r5_twiddle_get(600);
  const r5_twiddle_t *tw_outer = r5_twiddle_get(N);
  AssertFatal(tw_inner && tw_outer, "Missing radix-25 twiddles N=%d\n", N);

#define DFT3000_R5_TW(TW, PREFIX) \
  DFT_BIND_R5_TWIDDLES((TW), dir, PREFIX##1r, PREFIX##1i, PREFIX##2r, PREFIX##2i, PREFIX##3r, PREFIX##3i, PREFIX##4r, PREFIX##4i)

  DFT3000_R5_TW(tw_inner, in_);
  DFT3000_R5_TW(tw_outer, out_);

#undef DFT3000_R5_TW

  /* M=120 is divisible by eight, so this dedicated path has no SSE tail. */
  for (int k = 0; k < M; k += 8) {
    DFT_RADIX25_TILE(simde__m256i,
                     radix5_combine8_q15_256_fast,
                     DFT_R25_LOAD_CONTIGUOUS_256,
                     DFT_R25_INPUT_REGISTERS,
                     DFT_R5_COEFFICIENTS_256,
                     simde_mm256_storeu_si256,
                     k)
  }
}

/*
 * Load eight complex int16 samples separated by three c16_t elements.
 *
 * The R3 -> R25 layouts used by DFT1200/DFT2400 place the eight branch
 * samples at 32-bit indices:
 *
 *   0, 3, 6, 9, 12, 15, 18, 21
 *
 * relative to a 24-complex contiguous window.  Replace AVX2 gather with three
 * ordinary 32-byte loads plus lane permutations; all reads stay inside that
 * 24-complex window.
 */
static __attribute__((always_inline)) inline simde__m256i load8_complex_stride3_avx2(const c16_t *base)
{
  const simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)(base + 0));
  const simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)(base + 8));
  const simde__m256i c = simde_mm256_loadu_si256((const simde__m256i *)(base + 16));

  const simde__m256i ia = simde_mm256_setr_epi32(0, 3, 6, 0, 0, 0, 0, 0);
  const simde__m256i ib = simde_mm256_setr_epi32(0, 0, 0, 1, 4, 7, 0, 0);
  const simde__m256i ic = simde_mm256_setr_epi32(0, 0, 0, 0, 0, 0, 2, 5);

  simde__m256i r = simde_mm256_permutevar8x32_epi32(a, ia);
  r = simde_mm256_blend_epi32(r, simde_mm256_permutevar8x32_epi32(b, ib), 0x38);
  r = simde_mm256_blend_epi32(r, simde_mm256_permutevar8x32_epi32(c, ic), 0xC0);
  return r;
}

#define R25_LEAF_SECOND_STAGE_DFT4(A, Y, DIR)                           \
  for (int k2 = 0; k2 < 4; k2++) {                                      \
    simde__m256i z[4];                                                  \
    dft4x8_raw_q15_256(A[0][k2], A[1][k2], A[2][k2], A[3][k2], z, DIR); \
    for (int k1 = 0; k1 < 4; k1++)                                      \
      Y[k2 + 4 * k1] = z[k1];                                           \
  }

#define R25_LEAF_SECOND_STAGE_DFT8(A, Y, DIR)                   \
  for (int k2 = 0; k2 < 4; k2++) {                              \
    simde__m256i z0, z1, z2, z3, z4, z5, z6, z7;                \
    dft8x8_q15_256_dir(A[0][k2],                                \
                       A[1][k2],                                \
                       A[2][k2],                                \
                       A[3][k2],                                \
                       A[4][k2],                                \
                       A[5][k2],                                \
                       A[6][k2],                                \
                       A[7][k2],                                \
                       &z0,                                     \
                       &z1,                                     \
                       &z2,                                     \
                       &z3,                                     \
                       &z4,                                     \
                       &z5,                                     \
                       &z6,                                     \
                       &z7,                                     \
                       DIR);                                    \
    const simde__m256i z[8] = {z0, z1, z2, z3, z4, z5, z6, z7}; \
    for (int k1 = 0; k1 < 8; k1++)                              \
      Y[k2 + 4 * k1] = z[k1];                                   \
  }

#define DEFINE_R25_BLOCK_MAJOR_LEAF_BUILDER(NAME, M_VALUE, N1, N2, N3, TW_RE, TW_IM, SECOND_STAGE)                        \
  static __attribute__((always_inline)) inline void NAME(const c16_t *src, simde__m256i *leafv, int first, dft_dir_t dir) \
  {                                                                                                                       \
    enum { M = M_VALUE, BRANCH_STRIDE = 3, SAMPLE_STRIDE = 75 };                                                          \
                                                                                                                          \
    simde__m256i x[M];                                                                                                    \
    for (int n = 0; n < M; n++) {                                                                                         \
      const c16_t *base = src + n * SAMPLE_STRIDE + first * BRANCH_STRIDE;                                                \
      x[n] = load8_complex_stride3_avx2(base);                                                                            \
    }                                                                                                                     \
                                                                                                                          \
    simde__m256i a[N1][4];                                                                                                \
    for (int n1 = 0; n1 < N1; n1++) {                                                                                     \
      simde__m256i z[4];                                                                                                  \
      dft4x8_raw_q15_256(half_q15_256(x[n1]),                                                                             \
                         half_q15_256(x[n1 + N1]),                                                                        \
                         half_q15_256(x[n1 + N2]),                                                                        \
                         half_q15_256(x[n1 + N3]),                                                                        \
                         z,                                                                                               \
                         dir);                                                                                            \
      for (int k2 = 0; k2 < 4; k2++)                                                                                      \
        a[n1][k2] = leaf_twiddle_mul_q15_256(z[k2], TW_RE, TW_IM, M, n1 * k2, dir);                                       \
    }                                                                                                                     \
                                                                                                                          \
    simde__m256i y[M];                                                                                                    \
    SECOND_STAGE(a, y, dir)                                                                                               \
                                                                                                                          \
    for (int off = 0; off < M; off += 8) {                                                                                \
      DFT_LOAD_TRANSPOSE8_ROW(z, y, off)                                                                                  \
                                                                                                                          \
      simde__m256i *row = leafv + (off >> 3) * 25 + first;                                                                \
      row[0] = z0;                                                                                                        \
      row[1] = z1;                                                                                                        \
      row[2] = z2;                                                                                                        \
      row[3] = z3;                                                                                                        \
      row[4] = z4;                                                                                                        \
      row[5] = z5;                                                                                                        \
      row[6] = z6;                                                                                                        \
      row[7] = z7;                                                                                                        \
    }                                                                                                                     \
  }

DEFINE_R25_BLOCK_MAJOR_LEAF_BUILDER(dft16x8_strided_to_block_major_r25,
                                    16,
                                    4,
                                    8,
                                    12,
                                    dft16_leaf_twiddle_re,
                                    dft16_leaf_twiddle_im_fwd,
                                    R25_LEAF_SECOND_STAGE_DFT4)

DEFINE_R25_BLOCK_MAJOR_LEAF_BUILDER(dft32x8_strided_to_block_major_r25,
                                    32,
                                    8,
                                    16,
                                    24,
                                    dft32_leaf_twiddle_re,
                                    dft32_leaf_twiddle_im_fwd,
                                    R25_LEAF_SECOND_STAGE_DFT8)

#undef DEFINE_R25_BLOCK_MAJOR_LEAF_BUILDER
#undef R25_LEAF_SECOND_STAGE_DFT8
#undef R25_LEAF_SECOND_STAGE_DFT4

/* Fixed-size R25 child (e.g. R25(400) for DFT1200): the full x8 batches of leaves are stored directly by
 * 8-frequency block, the order consumed by the fused R25 parent; the leaf size and parent stride are parameters. */
#define DEFINE_RADIX25_STRIDED_CHILD(FN, NVAL, MVAL, BATCH_LEAF, TAIL_LEAF, PARENT_M)   \
  static __attribute__((noinline)) void FN(const c16_t *src, c16_t *dst, dft_dir_t dir) \
  {                                                                                     \
    enum { N = NVAL, M = MVAL, STRIDE = 3, CHILD_STRIDE = 75, BLOCKS = M / 8 };         \
    simde__m256i leafv[BLOCKS][25] __attribute__((aligned(64)));                        \
    c16_t tail[M] __attribute__((aligned(64)));                                         \
                                                                                        \
    BATCH_LEAF(src, &leafv[0][0], 0, dir);                                              \
    BATCH_LEAF(src, &leafv[0][0], 8, dir);                                              \
    BATCH_LEAF(src, &leafv[0][0], 16, dir);                                             \
                                                                                        \
    TAIL_LEAF(src + 24 * STRIDE, CHILD_STRIDE, tail, dir);                              \
    for (int b = 0; b < BLOCKS; b++)                                                    \
      leafv[b][24] = simde_mm256_load_si256((const simde__m256i *)(tail + 8 * b));      \
                                                                                        \
    RADIX25_BLOCK_MAJOR_PARENT(N, M, PARENT_M, leafv, dst)                              \
  }

DEFINE_RADIX25_STRIDED_CHILD(radix25_400_dft16_batch2, 400, 16, dft16x8_strided_to_block_major_r25, dft16_q15_128_strided, 80)

DEFINE_RADIX25_STRIDED_CHILD(radix25_800_dft32_batch2, 800, 32, dft32x8_strided_to_block_major_r25, dft32_q15_128_strided, 160)

#undef DEFINE_RADIX25_STRIDED_CHILD

#undef RADIX25_BLOCK_MAJOR_PARENT

/* DFT1200/2400 use R3 -> R25 with a materialized outer radix-3 boundary. */

#define DEFINE_R3_R25_PARENT(FN, NVAL, MVAL, CHILD, MESSAGE)                            \
  static __attribute__((noinline)) void FN(const c16_t *src, c16_t *dst, dft_dir_t dir) \
  {                                                                                     \
    enum { N = NVAL, SIZE = MVAL };                                                     \
    c16_t tmp[N] __attribute__((aligned(64)));                                          \
                                                                                        \
    CHILD(src + 0, tmp + 0 * SIZE, dir);                                                \
    CHILD(src + 1, tmp + 1 * SIZE, dir);                                                \
    CHILD(src + 2, tmp + 2 * SIZE, dir);                                                \
                                                                                        \
    const r3_twiddle_t *tw = r3_twiddle_get(N);                                         \
    AssertFatal(tw, MESSAGE);                                                           \
                                                                                        \
    radix3_combine_contiguous_avx2(tmp, dst, SIZE, tw, dir);                            \
  }

DEFINE_R3_R25_PARENT(dft1200_radix3_r25_avx2, 1200, 400, radix25_400_dft16_batch2, "Missing DFT1200 R3 parent twiddles\n")

DEFINE_R3_R25_PARENT(dft2400_radix3_r25_avx2, 2400, 800, radix25_800_dft32_batch2, "Missing DFT2400 R3 parent twiddles\n")

/* -------------------------------------------------------------------------
 * Normalized leaves with all normalization carried by twiddle coefficients.
 *
 * R30 parents for N=360/600/720 carry 1/sqrt(2) in W_N (1/sqrt(15) is in the DFT6).
 * Their leaf transforms carry 1/sqrt(M) in the leaf's own Cooley-Tukey
 * twiddles.  No whole-transform 1/sqrt(N) is applied before the leaf.
 * ------------------------------------------------------------------------- */

static const int16_t dft20_parent_leaf_twiddle_re[20] = {14654,  13937,  11856,  8614,  4528,  0, -4528, -8614, -11856, -13937,
                                                         -14654, -13937, -11856, -8614, -4528, 0, 4528,  8614,  11856,  13937};
static const int16_t dft20_parent_leaf_twiddle_im_fwd[20] = {0, -4528, -8614, -11856, -13937, -14654, -13937, -11856, -8614, -4528,
                                                             0, 4528,  8614,  11856,  13937,  14654,  13937,  11856,  8614,  4528};
static const int16_t dft24_parent_leaf_twiddle_re[24] = {11585, 11190, 10033,  8192,   5792,   2998,   0,      -2998,
                                                         -5792, -8192, -10033, -11190, -11585, -11190, -10033, -8192,
                                                         -5792, -2998, 0,      2998,   5792,   8192,   10033,  11190};
static const int16_t dft24_parent_leaf_twiddle_im_fwd[24] = {0,      -2998, -5792, -8192, -10033, -11190, -11585, -11190,
                                                             -10033, -8192, -5792, -2998, 0,      2998,   5792,   8192,
                                                             10033,  11190, 11585, 11190, 10033,  8192,   5792,   2998};

/* 12 = 4 x 3 Cooley-Tukey.
 * DFT3 is raw. W12 carries the complete 1/sqrt(12) leaf normalization.
 * The final DFT4 is raw. Output k = k2 + 3*k1. */
static __attribute__((noinline)) void
dft12x8_parent_twiddle_scaled_store(const c16_t *b, int M, c16_t *dst, int R, int first, int valid, dft_dir_t dir)
{
  enum { PITCH = 32 };
  simde__m256i a[4][3];
  (void)M;

  DFT_R3_FOLDED_LEAF_STAGE(4,
                           12,
                           simde_mm256_loadu_si256((const simde__m256i *)(b + (n1 + 0) * PITCH + first)),
                           simde_mm256_loadu_si256((const simde__m256i *)(b + (n1 + 4) * PITCH + first)),
                           simde_mm256_loadu_si256((const simde__m256i *)(b + (n1 + 8) * PITCH + first)),
                           dft12_leaf_twiddle_re,
                           dft12_leaf_twiddle_im_fwd)

  simde__m256i y[12];
  DFT12_FOLDED_SECOND_STAGE(dft4x8_raw_q15_256)

  for (int k = 0; k < 12; k++)
    store_r30_leaf_row8(dst, R, k, first, valid, y[k]);
}

/* 20 = 5 x 4 Cooley-Tukey.
 * First DFT4 raw, W20/sqrt(20), final DFT5 raw.
 * Output k = k2 + 4*k1. */
static __attribute__((noinline)) void
dft20x8_parent_twiddle_scaled_store(const c16_t *b, int M, c16_t *dst, int R, int first, int valid, dft_dir_t dir)
{
  enum { PITCH = 32 };
  simde__m256i a[5][4];
  (void)M;

  for (int n1 = 0; n1 < 5; n1++) {
    simde__m256i z[4];
    dft4x8_raw_q15_256(half_q15_256(simde_mm256_loadu_si256((const simde__m256i *)(b + (n1 + 0) * PITCH + first))),
                       half_q15_256(simde_mm256_loadu_si256((const simde__m256i *)(b + (n1 + 5) * PITCH + first))),
                       half_q15_256(simde_mm256_loadu_si256((const simde__m256i *)(b + (n1 + 10) * PITCH + first))),
                       half_q15_256(simde_mm256_loadu_si256((const simde__m256i *)(b + (n1 + 15) * PITCH + first))),
                       z,
                       dir);

    for (int k2 = 0; k2 < 4; k2++)
      a[n1][k2] = leaf_twiddle_mul_q15_256(z[k2], dft20_parent_leaf_twiddle_re, dft20_parent_leaf_twiddle_im_fwd, 20, n1 * k2, dir);
  }

  simde__m256i y[20];
  for (int k2 = 0; k2 < 4; k2++) {
    simde__m256i z0, z1, z2, z3, z4;
    radix5_butterfly8_q15_256(a[0][k2], a[1][k2], a[2][k2], a[3][k2], a[4][k2], &z0, &z1, &z2, &z3, &z4, dir);

    y[k2 + 4 * 0] = z0;
    y[k2 + 4 * 1] = z1;
    y[k2 + 4 * 2] = z2;
    y[k2 + 4 * 3] = z3;
    y[k2 + 4 * 4] = z4;
  }

  for (int k = 0; k < 20; k++)
    store_r30_leaf_row8(dst, R, k, first, valid, y[k]);
}

/* 24 = 8 x 3 Cooley-Tukey.
 * First DFT3 raw, W24/sqrt(24), final DFT8 raw.
 * Output k = k2 + 3*k1. */
static __attribute__((noinline)) void
dft24x8_parent_twiddle_scaled_store(const c16_t *b, int M, c16_t *dst, int R, int first, int valid, dft_dir_t dir)
{
  enum { PITCH = 32 };
  simde__m256i a[8][3];
  (void)M;

  DFT_R3_FOLDED_LEAF_STAGE(8,
                           24,
                           simde_mm256_loadu_si256((const simde__m256i *)(b + (n1 + 0) * PITCH + first)),
                           simde_mm256_loadu_si256((const simde__m256i *)(b + (n1 + 8) * PITCH + first)),
                           simde_mm256_loadu_si256((const simde__m256i *)(b + (n1 + 16) * PITCH + first)),
                           dft24_parent_leaf_twiddle_re,
                           dft24_parent_leaf_twiddle_im_fwd)

  simde__m256i y[24];
  for (int k2 = 0; k2 < 3; k2++) {
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

    y[k2 + 3 * 0] = z0;
    y[k2 + 3 * 1] = z1;
    y[k2 + 3 * 2] = z2;
    y[k2 + 3 * 3] = z3;
    y[k2 + 3 * 4] = z4;
    y[k2 + 3 * 5] = z5;
    y[k2 + 3 * 6] = z6;
    y[k2 + 3 * 7] = z7;
  }

  for (int k = 0; k < 24; k++)
    store_r30_leaf_row8(dst, R, k, first, valid, y[k]);
}

static void dft360_radix30_leaf12_twscaled(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { R = 30, M = 12 };
  c16_t b[32 * M] __attribute__((aligned(64)));
  /* The parent writes all 12 rows, including the two padding branches. */
  radix30_stage_360(src, b, dir);
  DFT_FOUR_LEAF_BATCHES(dft12x8_parent_twiddle_scaled_store, b, M, dst, R, 6)
}

#define DEFINE_R30_PADDED_LEAF_PARENT(FN, MVAL, STAGE, LEAF)     \
  static void FN(const c16_t *src, c16_t *dst, dft_dir_t dir)    \
  {                                                              \
    enum { R = 30, M = MVAL };                                   \
    /* STAGE writes all 32 rows, including zero padding rows. */ \
    c16_t b[32 * M] __attribute__((aligned(64)));                \
    STAGE(src, b, dir);                                          \
    LEAF(b, M, dst, R, 0, 8, dir);                               \
    LEAF(b, M, dst, R, 8, 8, dir);                               \
    LEAF(b, M, dst, R, 16, 8, dir);                              \
    LEAF(b, M, dst, R, 24, 6, dir);                              \
  }

DEFINE_R30_PADDED_LEAF_PARENT(dft600_radix30_leaf20_twscaled, 20, radix30_stage_600, dft20x8_parent_twiddle_scaled_store)

DEFINE_R30_PADDED_LEAF_PARENT(dft720_radix30_leaf24_twscaled, 24, radix30_stage_720, dft24x8_parent_twiddle_scaled_store)
