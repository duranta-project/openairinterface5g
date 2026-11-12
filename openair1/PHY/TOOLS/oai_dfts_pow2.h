/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* Internal part of oai_dfts.c: DFT8 core, DFT4..DFT128, split radix and the power-of-two radix-8/16 stages.
 * Included only by oai_dfts.c, in order; not a public header. */

#ifndef OAI_DFTS_INTERNAL
#error "oai_dfts_pow2.h is included only by oai_dfts.c"
#endif

/* DFT8 on eight independent lanes. With mid_scale = 0 the transform is raw
 * (gain sqrt(8)); otherwise the first-layer values are multiplied by
 * mid_scale, so Q15_INV_SQRT8 makes the DFT8 unitary while no intermediate
 * value exceeds sqrt(2) times the input rms. raw_s02/raw_s13, when given,
 * receive the unscaled partial DC sums. */
static __attribute__((always_inline)) inline void dft8x8_q15_256_core(const simde__m256i x0,
                                                                      const simde__m256i x1,
                                                                      const simde__m256i x2,
                                                                      const simde__m256i x3,
                                                                      const simde__m256i x4,
                                                                      const simde__m256i x5,
                                                                      const simde__m256i x6,
                                                                      const simde__m256i x7,
                                                                      simde__m256i *Y0,
                                                                      simde__m256i *Y1,
                                                                      simde__m256i *Y2,
                                                                      simde__m256i *Y3,
                                                                      simde__m256i *Y4,
                                                                      simde__m256i *Y5,
                                                                      simde__m256i *Y6,
                                                                      simde__m256i *Y7,
                                                                      dft_dir_t dir,
                                                                      const int16_t mid_scale,
                                                                      simde__m256i *raw_s02,
                                                                      simde__m256i *raw_s13)
{
  const simde__m256i c = simde_mm256_set1_epi16(Q15_INV_SQRT2);

  simde__m256i s04 = simde_mm256_adds_epi16(x0, x4);
  simde__m256i d04 = simde_mm256_subs_epi16(x0, x4);

  simde__m256i s15 = simde_mm256_adds_epi16(x1, x5);
  simde__m256i d15 = simde_mm256_subs_epi16(x1, x5);

  simde__m256i s26 = simde_mm256_adds_epi16(x2, x6);
  simde__m256i d26 = simde_mm256_subs_epi16(x2, x6);

  simde__m256i s37 = simde_mm256_adds_epi16(x3, x7);
  simde__m256i d37 = simde_mm256_subs_epi16(x3, x7);

  if (raw_s02) {
    *raw_s02 = simde_mm256_adds_epi16(s04, s26);
    *raw_s13 = simde_mm256_adds_epi16(s15, s37);
  }
  if (mid_scale == Q15_HALF) {
    /* (x0 - x4) / 2 = (x0 + x4) / 2 - x4: one rounding per pair. */
    s04 = half_odd_q15_256(s04);
    d04 = simde_mm256_subs_epi16(s04, x4);
    s15 = half_odd_q15_256(s15);
    d15 = simde_mm256_subs_epi16(s15, x5);
    s26 = half_odd_q15_256(s26);
    d26 = simde_mm256_subs_epi16(s26, x6);
    s37 = half_odd_q15_256(s37);
    d37 = simde_mm256_subs_epi16(s37, x7);
  } else if (mid_scale) {
    const simde__m256i h = simde_mm256_set1_epi16(mid_scale);
    s04 = simde_mm256_mulhrs_epi16(s04, h);
    d04 = simde_mm256_mulhrs_epi16(d04, h);
    s15 = simde_mm256_mulhrs_epi16(s15, h);
    d15 = simde_mm256_mulhrs_epi16(d15, h);
    s26 = simde_mm256_mulhrs_epi16(s26, h);
    d26 = simde_mm256_mulhrs_epi16(d26, h);
    s37 = simde_mm256_mulhrs_epi16(s37, h);
    d37 = simde_mm256_mulhrs_epi16(d37, h);
  }

  const simde__m256i s02 = simde_mm256_adds_epi16(s04, s26);
  const simde__m256i d02 = simde_mm256_subs_epi16(s04, s26);

  const simde__m256i s13 = simde_mm256_adds_epi16(s15, s37);
  const simde__m256i d13 = simde_mm256_subs_epi16(s15, s37);

  const simde__m256i s02h = s02, d02h = d02, s13h = s13, d13h = d13;

  *Y0 = simde_mm256_adds_epi16(s02h, s13h);
  *Y4 = simde_mm256_subs_epi16(s02h, s13h);
  add_pm_j_i16_256(d02h, d13h, Y2, Y6, dir);

  simde__m256i p = simde_mm256_adds_epi16(d15, d37);
  simde__m256i q = simde_mm256_subs_epi16(d15, d37);

  simde__m256i base_mj, base_pj, t1_arg, t3_arg;
  add_pm_j_i16_256(d04, d26, &base_mj, &base_pj, dir);
  add_pm_j_i16_256(q, p, &t1_arg, &t3_arg, dir);

  const simde__m256i t1 = simde_mm256_mulhrs_epi16(c, t1_arg);
  const simde__m256i t3 = simde_mm256_mulhrs_epi16(c, t3_arg);

  *Y1 = simde_mm256_adds_epi16(base_mj, t1);
  *Y5 = simde_mm256_subs_epi16(base_mj, t1);

  *Y7 = simde_mm256_adds_epi16(base_pj, t3);
  *Y3 = simde_mm256_subs_epi16(base_pj, t3);
}

static __attribute__((always_inline)) inline void dft8x8_q15_256_dir(const simde__m256i x0,
                                                                     const simde__m256i x1,
                                                                     const simde__m256i x2,
                                                                     const simde__m256i x3,
                                                                     const simde__m256i x4,
                                                                     const simde__m256i x5,
                                                                     const simde__m256i x6,
                                                                     const simde__m256i x7,
                                                                     simde__m256i *Y0,
                                                                     simde__m256i *Y1,
                                                                     simde__m256i *Y2,
                                                                     simde__m256i *Y3,
                                                                     simde__m256i *Y4,
                                                                     simde__m256i *Y5,
                                                                     simde__m256i *Y6,
                                                                     simde__m256i *Y7,
                                                                     dft_dir_t dir)
{
  dft8x8_q15_256_core(x0, x1, x2, x3, x4, x5, x6, x7, Y0, Y1, Y2, Y3, Y4, Y5, Y6, Y7, dir, 0, NULL, NULL);
}

static __attribute__((always_inline)) inline void dft8x8_q15_256_unitary_dir(const simde__m256i x0,
                                                                             const simde__m256i x1,
                                                                             const simde__m256i x2,
                                                                             const simde__m256i x3,
                                                                             const simde__m256i x4,
                                                                             const simde__m256i x5,
                                                                             const simde__m256i x6,
                                                                             const simde__m256i x7,
                                                                             simde__m256i *Y0,
                                                                             simde__m256i *Y1,
                                                                             simde__m256i *Y2,
                                                                             simde__m256i *Y3,
                                                                             simde__m256i *Y4,
                                                                             simde__m256i *Y5,
                                                                             simde__m256i *Y6,
                                                                             simde__m256i *Y7,
                                                                             dft_dir_t dir)
{
  dft8x8_q15_256_core(x0, x1, x2, x3, x4, x5, x6, x7, Y0, Y1, Y2, Y3, Y4, Y5, Y6, Y7, dir, Q15_INV_SQRT8, NULL, NULL);
}

/* DFT8 halved at its first layer (output = raw / 2, gain sqrt(2)). Stages
 * followed by a twiddle multiply use it and put the rest of the scale in the
 * twiddles, which rounds at a higher level than the unitary form. The DC
 * variant also returns the raw partial DC sums for an exact 32-bit DC. */
static __attribute__((always_inline)) inline void dft8x8_q15_256_half_dir(const simde__m256i x[8], simde__m256i h[8], dft_dir_t dir)
{
  dft8x8_q15_256_core(x[0],
                      x[1],
                      x[2],
                      x[3],
                      x[4],
                      x[5],
                      x[6],
                      x[7],
                      &h[0],
                      &h[1],
                      &h[2],
                      &h[3],
                      &h[4],
                      &h[5],
                      &h[6],
                      &h[7],
                      dir,
                      Q15_HALF,
                      NULL,
                      NULL);
}

static __attribute__((always_inline)) inline void dft8x8_q15_256_half_dc_dir(const simde__m256i x[8],
                                                                             simde__m256i h[8],
                                                                             simde__m256i *s02,
                                                                             simde__m256i *s13,
                                                                             dft_dir_t dir)
{
  dft8x8_q15_256_core(x[0],
                      x[1],
                      x[2],
                      x[3],
                      x[4],
                      x[5],
                      x[6],
                      x[7],
                      &h[0],
                      &h[1],
                      &h[2],
                      &h[3],
                      &h[4],
                      &h[5],
                      &h[6],
                      &h[7],
                      dir,
                      Q15_HALF,
                      s02,
                      s13);
}

static inline void dft64_avx(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  const simde__m256i x0 = simde_mm256_loadu_si256((const simde__m256i *)(src + 0));
  const simde__m256i x1 = simde_mm256_loadu_si256((const simde__m256i *)(src + 8));
  const simde__m256i x2 = simde_mm256_loadu_si256((const simde__m256i *)(src + 16));
  const simde__m256i x3 = simde_mm256_loadu_si256((const simde__m256i *)(src + 24));
  const simde__m256i x4 = simde_mm256_loadu_si256((const simde__m256i *)(src + 32));
  const simde__m256i x5 = simde_mm256_loadu_si256((const simde__m256i *)(src + 40));
  const simde__m256i x6 = simde_mm256_loadu_si256((const simde__m256i *)(src + 48));
  const simde__m256i x7 = simde_mm256_loadu_si256((const simde__m256i *)(src + 56));

  simde__m256i H0, H1, H2, H3;
  simde__m256i H4, H5, H6, H7;
  simde__m128i dc;

  {
    const simde__m256i x[8] = {x0, x1, x2, x3, x4, x5, x6, x7};
    simde__m256i h[8], s02, s13;
    dft8x8_q15_256_half_dc_dir(x, h, &s02, &s13, dir);
    H0 = h[0];
    H1 = h[1];
    H2 = h[2];
    H3 = h[3];
    H4 = h[4];
    H5 = h[5];
    H6 = h[6];
    H7 = h[7];
    dc = dft64_dc_from_raw_sums(s02, s13);
  }
  const int ds = dir == DFT_DIR_FORWARD ? 0 : 1;
  const simde__m256i *C64_RE = g_dft64_re;
  const simde__m256i *C64_IM = g_dft64_im[ds];
  H0 = quarter_q15_256(H0);

  /* C64 carries 1/4. */
  H1 = complex_mul8_prepack_small_q15_256(H1, C64_RE[1], C64_IM[1]);
  H2 = complex_mul8_prepack_small_q15_256(H2, C64_RE[2], C64_IM[2]);
  H3 = complex_mul8_prepack_small_q15_256(H3, C64_RE[3], C64_IM[3]);
  H4 = complex_mul8_prepack_small_q15_256(H4, C64_RE[4], C64_IM[4]);
  H5 = complex_mul8_prepack_small_q15_256(H5, C64_RE[5], C64_IM[5]);
  H6 = complex_mul8_prepack_small_q15_256(H6, C64_RE[6], C64_IM[6]);
  H7 = complex_mul8_prepack_small_q15_256(H7, C64_RE[7], C64_IM[7]);

  transpose8_complex_i16_256(&H0, &H1, &H2, &H3, &H4, &H5, &H6, &H7);

  simde__m256i Y0, Y1, Y2, Y3;
  simde__m256i Y4, Y5, Y6, Y7;

  dft8x8_q15_256_dir(H0, H1, H2, H3, H4, H5, H6, H7, &Y0, &Y1, &Y2, &Y3, &Y4, &Y5, &Y6, &Y7, dir);
  Y0 = simde_mm256_blend_epi32(Y0, simde_mm256_castsi128_si256(dc), 0x01);
  simde_mm256_storeu_si256((simde__m256i *)(dst + 0), Y0);
  simde_mm256_storeu_si256((simde__m256i *)(dst + 8), Y1);
  simde_mm256_storeu_si256((simde__m256i *)(dst + 16), Y2);
  simde_mm256_storeu_si256((simde__m256i *)(dst + 24), Y3);
  simde_mm256_storeu_si256((simde__m256i *)(dst + 32), Y4);
  simde_mm256_storeu_si256((simde__m256i *)(dst + 40), Y5);
  simde_mm256_storeu_si256((simde__m256i *)(dst + 48), Y6);
  simde_mm256_storeu_si256((simde__m256i *)(dst + 56), Y7);
}

static inline void dft128_stage0_blk_q15_256_dir(const c16_t *src,
                                                 c16_t *a,
                                                 c16_t *b,
                                                 int blk,
                                                 dft_dir_t dir,
                                                 const simde__m256i *tw_RE,
                                                 const simde__m256i *tw_IM)
{
  (void)dir;
  const simde__m256i x0 = simde_mm256_loadu_si256((const simde__m256i *)(src + 8 * blk));

  const simde__m256i x1 = simde_mm256_loadu_si256((const simde__m256i *)(src + 64 + 8 * blk));

  simde__m256i sum = simde_mm256_adds_epi16(x0, x1);
  simde__m256i diff = simde_mm256_subs_epi16(x0, x1);

  const simde__m256i s = simde_mm256_set1_epi16(Q15_INV_SQRT2);
  sum = simde_mm256_mulhrs_epi16(sum, s);

  diff = complex_mul8_prepack_q15_256(diff, tw_RE[blk], tw_IM[blk]);

  simde_mm256_store_si256((simde__m256i *)(a + 8 * blk), sum);

  simde_mm256_store_si256((simde__m256i *)(b + 8 * blk), diff);
}

static inline void interleave64_complex_q15_256(const c16_t *A, const c16_t *B, c16_t *dst)
{
  for (int blk = 0; blk < 8; blk++) {
    const simde__m256i va = simde_mm256_load_si256((const simde__m256i *)(A + 8 * blk));

    const simde__m256i vb = simde_mm256_load_si256((const simde__m256i *)(B + 8 * blk));

    /*
     * va = [A0 A1 A2 A3 | A4 A5 A6 A7]
     * vb = [B0 B1 B2 B3 | B4 B5 B6 B7]
     *
     * Each A0/B0 is one c16_t = 32 bits.
     */
    const simde__m256i lo = simde_mm256_unpacklo_epi32(va, vb);
    const simde__m256i hi = simde_mm256_unpackhi_epi32(va, vb);

    /*
     * out0 = [A0 B0 A1 B1 A2 B2 A3 B3]
     * out1 = [A4 B4 A5 B5 A6 B6 A7 B7]
     */
    const simde__m256i out0 = simde_mm256_permute2x128_si256(lo, hi, 0x20);
    const simde__m256i out1 = simde_mm256_permute2x128_si256(lo, hi, 0x31);

    simde_mm256_storeu_si256((simde__m256i *)(dst + 16 * blk), out0);

    simde_mm256_storeu_si256((simde__m256i *)(dst + 16 * blk + 8), out1);
  }
}

static inline void dft128_dir(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  c16_t a[64] __attribute__((aligned(32)));
  c16_t b[64] __attribute__((aligned(32)));

  c16_t A[64] __attribute__((aligned(32)));
  c16_t B[64] __attribute__((aligned(32)));
  const int ds = dir == DFT_DIR_FORWARD ? 0 : 1;
  const simde__m256i *W128_RE = g_dft128_re;
  const simde__m256i *W128_IM = g_dft128_im[ds];

  for (int blk = 0; blk < 8; blk++) {
    dft128_stage0_blk_q15_256_dir(src, a, b, blk, dir, W128_RE, W128_IM);
  }

  dft64_avx(a, A, dir);
  dft64_avx(b, B, dir);

  interleave64_complex_q15_256(A, B, dst);
}

static inline void sr_combine_simd(c16_t *E, c16_t *O1, c16_t *O3, c16_t *y, int N, const sr_twiddle_simd_t *tw, dft_dir_t dir)
{
  int half = N / 2;
  int quarter = N / 4;

  const simde__m256i swap_mask = simde_mm256_setr_epi8(2,
                                                       3,
                                                       0,
                                                       1,
                                                       6,
                                                       7,
                                                       4,
                                                       5,
                                                       10,
                                                       11,
                                                       8,
                                                       9,
                                                       14,
                                                       15,
                                                       12,
                                                       13,
                                                       2,
                                                       3,
                                                       0,
                                                       1,
                                                       6,
                                                       7,
                                                       4,
                                                       5,
                                                       10,
                                                       11,
                                                       8,
                                                       9,
                                                       14,
                                                       15,
                                                       12,
                                                       13);

  simde__m256i sign_mask;

  if (dir == DFT_DIR_FORWARD) {
    sign_mask = simde_mm256_setr_epi16(1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1);
  } else {
    sign_mask = simde_mm256_setr_epi16(-1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1);
  }

  const simde__m256i sqrt2_inv = simde_mm256_set1_epi16(Q15_INV_SQRT2);

  for (int b = 0; b < tw->blocks; b++) {
    int k = 8 * b;

    simde__m256i O1v = simde_mm256_load_si256((simde__m256i *)&O1[k]);
    simde__m256i O3v = simde_mm256_load_si256((simde__m256i *)&O3[k]);

    simde__m256i t1 = c16_mul_q15_simd256(O1v, SR_W1_RE_NEGIM(tw)[b], SR_W1_IM_RE(tw)[b]);

    simde__m256i t2 = c16_mul_q15_simd256(O3v, SR_W3_RE_NEGIM(tw)[b], SR_W3_IM_RE(tw)[b]);

    simde__m256i a = simde_mm256_adds_epi16(t1, t2);
    simde__m256i d = simde_mm256_subs_epi16(t1, t2);

    simde__m256i bval = simde_mm256_shuffle_epi8(d, swap_mask);
    bval = simde_mm256_sign_epi16(bval, sign_mask);

    simde__m256i E0 = simde_mm256_load_si256((simde__m256i *)&E[k]);
    simde__m256i E1 = simde_mm256_load_si256((simde__m256i *)&E[k + quarter]);

    E0 = simde_mm256_mulhrs_epi16(E0, sqrt2_inv);
    E1 = simde_mm256_mulhrs_epi16(E1, sqrt2_inv);

    simde__m256i Y0 = simde_mm256_adds_epi16(E0, a);
    simde__m256i Y2 = simde_mm256_subs_epi16(E0, a);
    simde__m256i Y1 = simde_mm256_adds_epi16(E1, bval);
    simde__m256i Y3 = simde_mm256_subs_epi16(E1, bval);

    simde_mm256_store_si256((simde__m256i *)&y[k], Y0);
    simde_mm256_store_si256((simde__m256i *)&y[k + quarter], Y1);
    simde_mm256_store_si256((simde__m256i *)&y[k + half], Y2);
    simde_mm256_store_si256((simde__m256i *)&y[k + 3 * quarter], Y3);
  }
}

static inline void pack_split_radix_input_avx2_fused(const c16_t *__restrict x, c16_t *__restrict sub_in, int N)
{
  _Static_assert(sizeof(c16_t) == 4, "c16_t must be 32-bit");

  const int half = N >> 1;
  const int quarter = N >> 2;

  c16_t *__restrict E_in = sub_in;
  c16_t *__restrict O1_in = sub_in + half;
  c16_t *__restrict O3_in = sub_in + half + quarter;

  const simde__m256i idx = simde_mm256_setr_epi32(0, 2, 4, 6, 1, 5, 3, 7);

  int in = 0;
  int e = 0;
  int o = 0;

  for (; in + 32 <= N; in += 32, e += 16, o += 8) {
    simde__m256i v0 = simde_mm256_loadu_si256((const simde__m256i *)&x[in + 0]);

    simde__m256i v1 = simde_mm256_loadu_si256((const simde__m256i *)&x[in + 8]);

    simde__m256i v2 = simde_mm256_loadu_si256((const simde__m256i *)&x[in + 16]);

    simde__m256i v3 = simde_mm256_loadu_si256((const simde__m256i *)&x[in + 24]);

    /*
     * p0 = [x0,  x2,  x4,  x6,  x1,  x5,  x3,  x7]
     * p1 = [x8,  x10, x12, x14, x9,  x13, x11, x15]
     * p2 = [x16, x18, x20, x22, x17, x21, x19, x23]
     * p3 = [x24, x26, x28, x30, x25, x29, x27, x31]
     */
    simde__m256i p0 = simde_mm256_permutevar8x32_epi32(v0, idx);
    simde__m256i p1 = simde_mm256_permutevar8x32_epi32(v1, idx);
    simde__m256i p2 = simde_mm256_permutevar8x32_epi32(v2, idx);
    simde__m256i p3 = simde_mm256_permutevar8x32_epi32(v3, idx);

    /*
     * E output:
     *
     * low128(p0) = x0,  x2,  x4,  x6
     * low128(p1) = x8,  x10, x12, x14
     * low128(p2) = x16, x18, x20, x22
     * low128(p3) = x24, x26, x28, x30
     */
    simde_mm_store_si128((simde__m128i *)&E_in[e + 0], simde_mm256_castsi256_si128(p0));

    simde_mm_store_si128((simde__m128i *)&E_in[e + 4], simde_mm256_castsi256_si128(p1));

    simde_mm_store_si128((simde__m128i *)&E_in[e + 8], simde_mm256_castsi256_si128(p2));

    simde_mm_store_si128((simde__m128i *)&E_in[e + 12], simde_mm256_castsi256_si128(p3));

    /*
     * high128(p0) = x1,  x5,  x3,  x7
     * high128(p1) = x9,  x13, x11, x15
     * high128(p2) = x17, x21, x19, x23
     * high128(p3) = x25, x29, x27, x31
     */
    simde__m128i h0 = simde_mm256_extracti128_si256(p0, 1);
    simde__m128i h1 = simde_mm256_extracti128_si256(p1, 1);
    simde__m128i h2 = simde_mm256_extracti128_si256(p2, 1);
    simde__m128i h3 = simde_mm256_extracti128_si256(p3, 1);

    /*
     * O1:
     * unpacklo_epi64(h0,h1) = x1,  x5,  x9,  x13
     * unpacklo_epi64(h2,h3) = x17, x21, x25, x29
     *
     * O3:
     * unpackhi_epi64(h0,h1) = x3,  x7,  x11, x15
     * unpackhi_epi64(h2,h3) = x19, x23, x27, x31
     */
    simde__m128i o1_0 = simde_mm_unpacklo_epi64(h0, h1);
    simde__m128i o3_0 = simde_mm_unpackhi_epi64(h0, h1);

    simde__m128i o1_1 = simde_mm_unpacklo_epi64(h2, h3);
    simde__m128i o3_1 = simde_mm_unpackhi_epi64(h2, h3);

    simde_mm_store_si128((simde__m128i *)&O1_in[o + 0], o1_0);
    simde_mm_store_si128((simde__m128i *)&O1_in[o + 4], o1_1);

    simde_mm_store_si128((simde__m128i *)&O3_in[o + 0], o3_0);
    simde_mm_store_si128((simde__m128i *)&O3_in[o + 4], o3_1);
  }
}

static inline simde__m128i dft4_avx(simde__m128i x, dft_dir_t dir)
{
  /*
   * lo = [x0 x1 x0 x1]
   * hi = [x2 x3 x2 x3]
   */
  x = half_q15_128(x);
  const simde__m128i lo = simde_mm_shuffle_epi32(x, _MM_SHUFFLE(1, 0, 1, 0));
  const simde__m128i hi = simde_mm_shuffle_epi32(x, _MM_SHUFFLE(3, 2, 3, 2));

  const simde__m128i s = simde_mm_adds_epi16(lo, hi);
  const simde__m128i d = simde_mm_subs_epi16(lo, hi);

  const simde__m128i s_sw = simde_mm_shuffle_epi32(s, _MM_SHUFFLE(2, 3, 0, 1));
  const simde__m128i d_sw = simde_mm_shuffle_epi32(d, _MM_SHUFFLE(2, 3, 0, 1));

  const simde__m128i y0v = simde_mm_adds_epi16(s, s_sw);
  const simde__m128i y2v = simde_mm_subs_epi16(s, s_sw);

  simde__m128i y1v, y3v;
  add_pm_j_i16_128(d, d_sw, &y1v, &y3v, dir);

  /*
   * y0v lane0 = Y0
   * y1v lane0 = Y1
   * y2v lane0 = Y2
   * y3v lane0 = Y3
   */
  const simde__m128i y01 = simde_mm_unpacklo_epi32(y0v, y1v); // [Y0 Y1 ... ...]
  const simde__m128i y23 = simde_mm_unpacklo_epi32(y2v, y3v); // [Y2 Y3 ... ...]

  return simde_mm_unpacklo_epi64(y01, y23); // [Y0 Y1 Y2 Y3]
}

static inline void dft4_void(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  const simde__m128i x = simde_mm_loadu_si128((const simde__m128i *)src);

  const simde__m128i y = dft4_avx(x, dir);

  simde_mm_storeu_si128((simde__m128i *)dst, y);
}

static inline void dft8_avx(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  /* Load the lower and upper four input samples. */
  const simde__m128i v0 = simde_mm_loadu_si128((const simde__m128i *)(src + 0));

  const simde__m128i v1 = simde_mm_loadu_si128((const simde__m128i *)(src + 4));

  /* Separate even and odd samples for the two DFT4 children. */
  const simde__m128i v0_even = simde_mm_shuffle_epi32(v0, _MM_SHUFFLE(2, 0, 2, 0));

  const simde__m128i v1_even = simde_mm_shuffle_epi32(v1, _MM_SHUFFLE(2, 0, 2, 0));

  const simde__m128i v0_odd = simde_mm_shuffle_epi32(v0, _MM_SHUFFLE(3, 1, 3, 1));

  const simde__m128i v1_odd = simde_mm_shuffle_epi32(v1, _MM_SHUFFLE(3, 1, 3, 1));

  const simde__m128i E_in = simde_mm_unpacklo_epi64(v0_even, v1_even); // [x0 x2 x4 x6]

  const simde__m128i O_in = simde_mm_unpacklo_epi64(v0_odd, v1_odd); // [x1 x3 x5 x7]

  /* E is scaled here; O is scaled by the following twiddle multiplication. */
  const simde__m128i E = q15_mul_i16_128(dft4_avx(E_in, dir), Q15_INV_SQRT2);
  const simde__m128i O = dft4_avx(O_in, dir);

  /* W8/sqrt(2), with each real coefficient duplicated and imaginary
   * coefficients packed as [-wi, wi] for complex multiplication. */
  const simde__m128i W8_RE_RE = simde_mm_setr_epi16(23170, 23170, Q15_HALF_TW, Q15_HALF_TW, 0, 0, -Q15_HALF_TW, -Q15_HALF_TW);

  const simde__m128i W8_IM_SIGNED_FWD =
      simde_mm_setr_epi16(0, 0, Q15_HALF_TW, -Q15_HALF_TW, 23170, -23170, Q15_HALF_TW, -Q15_HALF_TW);

  const simde__m128i W8_IM_SIGNED = twiddle_im_dir_128(W8_IM_SIGNED_FWD, dir);

  /* The packed coefficients include the radix-2 normalization. */
  const simde__m128i T = complex_mul4_prepack_q15_128(O, W8_RE_RE, W8_IM_SIGNED);

  /* Natural order: Y[k] = E[k] + T[k], Y[k+4] = E[k] - T[k]. */
  const simde__m128i Y03 = simde_mm_adds_epi16(E, T); // [Y0 Y1 Y2 Y3]
  const simde__m128i Y47 = simde_mm_subs_epi16(E, T); // [Y4 Y5 Y6 Y7]

  simde_mm_storeu_si128((simde__m128i *)(dst + 0), Y03);
  simde_mm_storeu_si128((simde__m128i *)(dst + 4), Y47);
}

/*
 * Twiddles for DFT16 radix-4 combine.
 *
 * Format:
 *   RE_RE     = [ re0, re0, re1, re1, re2, re2, re3, re3 ]
 *   IM_SIGNED = [-im0, im0,-im1, im1,-im2, im2,-im3, im3]
 *
 * Forward:
 *   W16^k = cos(2*pi*k/16) - j sin(2*pi*k/16)
 */

/* W16^(1*j), j = 0..3 */
static const int16_t W16_1_RE_RE[8] __attribute__((
    aligned(16))) = {Q15_ONE, Q15_ONE, Q15_COS_PI_8, Q15_COS_PI_8, Q15_INV_SQRT2, Q15_INV_SQRT2, Q15_SIN_PI_8, Q15_SIN_PI_8};

static const int16_t W16_1_IM_SIGNED[8]
    __attribute__((aligned(16))) = {0, 0, Q15_SIN_PI_8, -Q15_SIN_PI_8, Q15_INV_SQRT2, -Q15_INV_SQRT2, Q15_COS_PI_8, -Q15_COS_PI_8};

/* W16^(2*j), j = 0..3 */
static const int16_t W16_2_RE_RE[8]
    __attribute__((aligned(16))) = {Q15_ONE, Q15_ONE, Q15_INV_SQRT2, Q15_INV_SQRT2, 0, 0, -Q15_INV_SQRT2, -Q15_INV_SQRT2};

static const int16_t W16_2_IM_SIGNED[8]
    __attribute__((aligned(16))) = {0, 0, Q15_INV_SQRT2, -Q15_INV_SQRT2, Q15_ONE, -Q15_ONE, Q15_INV_SQRT2, -Q15_INV_SQRT2};

/* W16^(3*j), j = 0..3 */
static const int16_t W16_3_RE_RE[8] __attribute__((
    aligned(16))) = {Q15_ONE, Q15_ONE, Q15_SIN_PI_8, Q15_SIN_PI_8, -Q15_INV_SQRT2, -Q15_INV_SQRT2, -Q15_COS_PI_8, -Q15_COS_PI_8};

static const int16_t W16_3_IM_SIGNED[8]
    __attribute__((aligned(16))) = {0, 0, Q15_COS_PI_8, -Q15_COS_PI_8, Q15_INV_SQRT2, -Q15_INV_SQRT2, -Q15_SIN_PI_8, Q15_SIN_PI_8};

/* Halve each input before the saturating DFT4 sums, for either SIMD width. */
#define DEFINE_UNITARY_DFT4(FN, ATTR, VEC, SHIFT, ADD, SUB, WADD, WSUB, PMJ)                                    \
  static ATTR inline void                                                                                       \
  FN(const VEC x0, const VEC x1, const VEC x2, const VEC x3, VEC *Y0, VEC *Y1, VEC *Y2, VEC *Y3, dft_dir_t dir) \
  {                                                                                                             \
    const VEC x0s = SHIFT(x0);                                                                                  \
    const VEC x1s = SHIFT(x1);                                                                                  \
    const VEC x2s = SHIFT(x2);                                                                                  \
    const VEC x3s = SHIFT(x3);                                                                                  \
                                                                                                                \
    /* Halved inputs: the first-layer sums fit in 16 bits, so they use wrapping adds. */                        \
    const VEC s02 = WADD(x0s, x2s);                                                                             \
    const VEC d02 = WSUB(x0s, x2s);                                                                             \
    const VEC s13 = WADD(x1s, x3s);                                                                             \
    const VEC d13 = WSUB(x1s, x3s);                                                                             \
                                                                                                                \
    *Y0 = ADD(s02, s13);                                                                                        \
    *Y2 = SUB(s02, s13);                                                                                        \
    PMJ(d02, d13, Y1, Y3, dir);                                                                                 \
  }

DEFINE_UNITARY_DFT4(dft4x4_q15_128,
                    ,
                    simde__m128i,
                    half_q15_128,
                    simde_mm_adds_epi16,
                    simde_mm_subs_epi16,
                    simde_mm_add_epi16,
                    simde_mm_sub_epi16,
                    add_pm_j_i16_128)
/* Same 4x4 complex transpose for SSE and the two independent AVX2 lanes. */
#define DEFINE_COMPLEX_TRANSPOSE4(FN, VEC, LO32, HI32, LO64, HI64) \
  static inline void FN(VEC *Y0, VEC *Y1, VEC *Y2, VEC *Y3)        \
  {                                                                \
    const VEC a = *Y0;                                             \
    const VEC b = *Y1;                                             \
    const VEC c = *Y2;                                             \
    const VEC d = *Y3;                                             \
                                                                   \
    const VEC ab_lo = LO32(a, b);                                  \
    const VEC ab_hi = HI32(a, b);                                  \
    const VEC cd_lo = LO32(c, d);                                  \
    const VEC cd_hi = HI32(c, d);                                  \
                                                                   \
    *Y0 = LO64(ab_lo, cd_lo);                                      \
    *Y1 = HI64(ab_lo, cd_lo);                                      \
    *Y2 = LO64(ab_hi, cd_hi);                                      \
    *Y3 = HI64(ab_hi, cd_hi);                                      \
  }

DEFINE_COMPLEX_TRANSPOSE4(transpose4_complex_i16_128,
                          simde__m128i,
                          simde_mm_unpacklo_epi32,
                          simde_mm_unpackhi_epi32,
                          simde_mm_unpacklo_epi64,
                          simde_mm_unpackhi_epi64)

static inline void combine16_q15_128(const simde__m128i H[4], c16_t *dst, dft_dir_t dir)
{
  /*
   * H[0] = [H0[0] H0[1] H0[2] H0[3]]
   * H[1] = [H1[0] H1[1] H1[2] H1[3]]
   * H[2] = [H2[0] H2[1] H2[2] H2[3]]
   * H[3] = [H3[0] H3[1] H3[2] H3[3]]
   */
  const simde__m128i W1_RE_RE = simde_mm_load_si128((const simde__m128i *)W16_1_RE_RE);

  const simde__m128i W1_IM_SIGNED = twiddle_im_dir_128(simde_mm_load_si128((const simde__m128i *)W16_1_IM_SIGNED), dir);

  const simde__m128i W2_RE_RE = simde_mm_load_si128((const simde__m128i *)W16_2_RE_RE);

  const simde__m128i W2_IM_SIGNED = twiddle_im_dir_128(simde_mm_load_si128((const simde__m128i *)W16_2_IM_SIGNED), dir);

  const simde__m128i W3_RE_RE = simde_mm_load_si128((const simde__m128i *)W16_3_RE_RE);

  const simde__m128i W3_IM_SIGNED = twiddle_im_dir_128(simde_mm_load_si128((const simde__m128i *)W16_3_IM_SIGNED), dir);
  const simde__m128i A0 = H[0];

  const simde__m128i A1 = complex_mul4_prepack_q15_128(H[1], W1_RE_RE, W1_IM_SIGNED);

  const simde__m128i A2 = complex_mul4_prepack_q15_128(H[2], W2_RE_RE, W2_IM_SIGNED);

  const simde__m128i A3 = complex_mul4_prepack_q15_128(H[3], W3_RE_RE, W3_IM_SIGNED);

  simde__m128i t0 = A0;
  simde__m128i t1 = A1;
  simde__m128i t2 = A2;
  simde__m128i t3 = A3;

  transpose4_complex_i16_128(&t0, &t1, &t2, &t3);

  simde__m128i Y0, Y1, Y2, Y3;

  dft4x4_q15_128(t0, t1, t2, t3, &Y0, &Y1, &Y2, &Y3, dir);

  simde_mm_storeu_si128((simde__m128i *)(dst + 0), Y0);
  simde_mm_storeu_si128((simde__m128i *)(dst + 4), Y1);
  simde_mm_storeu_si128((simde__m128i *)(dst + 8), Y2);
  simde_mm_storeu_si128((simde__m128i *)(dst + 12), Y3);
}

static inline void dft16_q15_128(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  simde__m128i H[4] __attribute__((aligned(16)));

  const simde__m128i x0 = simde_mm_loadu_si128((const simde__m128i *)(src + 0));

  const simde__m128i x1 = simde_mm_loadu_si128((const simde__m128i *)(src + 4));

  const simde__m128i x2 = simde_mm_loadu_si128((const simde__m128i *)(src + 8));

  const simde__m128i x3 = simde_mm_loadu_si128((const simde__m128i *)(src + 12));

  dft4x4_q15_128(x0, x1, x2, x3, &H[0], &H[1], &H[2], &H[3], dir);

  combine16_q15_128(H, dst, dir);
}

/*
 * Format:
 *   RE_RE     = [ re0, re0, re1, re1, re2, re2, re3, re3 ]
 *   IM_SIGNED = [-im0, im0,-im1, im1,-im2, im2,-im3, im3]
 *
 * Forward:
 *   W12^k = cos(2*pi*k/12) - j sin(2*pi*k/12)
 *
 */

/* W12^k, k = 0..3 */
static const int16_t W12_R3_W1_RE_RE[8] __attribute__((aligned(16))) = {Q15_INV_SQRT3,
                                                                        Q15_INV_SQRT3, /* 1 / sqrt3 */
                                                                        Q15_HALF_TW,
                                                                        Q15_HALF_TW, /* cos(pi/6) / sqrt3 = 1/2 */
                                                                        Q15_INV_2SQRT3,
                                                                        Q15_INV_2SQRT3, /* cos(pi/3) / sqrt3 */
                                                                        0,
                                                                        0};

static const int16_t W12_R3_W1_IM_SIGNED[8] __attribute__((
    aligned(16))) = {0, 0, Q15_INV_2SQRT3, -Q15_INV_2SQRT3, Q15_HALF_TW, -Q15_HALF_TW, Q15_INV_SQRT3, -Q15_INV_SQRT3};

/* W12^(2k), k = 0..3 */
static const int16_t W12_R3_W2_RE_RE[8] __attribute__((aligned(16))) = {Q15_INV_SQRT3,
                                                                        Q15_INV_SQRT3,
                                                                        Q15_INV_2SQRT3,
                                                                        Q15_INV_2SQRT3,
                                                                        -Q15_INV_2SQRT3,
                                                                        -Q15_INV_2SQRT3,
                                                                        -Q15_INV_SQRT3,
                                                                        -Q15_INV_SQRT3};

static const int16_t W12_R3_W2_IM_SIGNED[8]
    __attribute__((aligned(16))) = {0, 0, Q15_HALF_TW, -Q15_HALF_TW, Q15_HALF_TW, -Q15_HALF_TW, 0, 0};

static inline simde__m128i pack3_complex_plus_zero_c16(const c16_t a, const c16_t b, const c16_t c)
{
  int32_t x, y, z;
  memcpy(&x, &a, 4);
  memcpy(&y, &b, 4);
  memcpy(&z, &c, 4);
  return simde_mm_setr_epi32(x, y, z, 0);
}

#if defined(__GNUC__)
#define OAI_DFT_SMALL_HOT __attribute__((hot, aligned(32), section(".text.hot.oai_dft_small")))
#else
#define OAI_DFT_SMALL_HOT
#endif

OAI_DFT_SMALL_HOT void dft16(int16_t *input, int16_t *output, uint8_t scale_flag);

/* Common DFT12 arithmetic after contiguous or strided input packing. */
#define DFT12_SCALAR_FINISH()                                                                                         \
  simde__m128i H0, H1, H2, H3;                                                                                        \
                                                                                                                      \
  dft4x4_q15_128(x0, x1, x2, x3, &H0, &H1, &H2, &H3, dir);                                                            \
                                                                                                                      \
  transpose4_complex_i16_128(&H0, &H1, &H2, &H3);                                                                     \
                                                                                                                      \
  const simde__m128i A = H0;                                                                                          \
  const simde__m128i X1 = H1;                                                                                         \
  const simde__m128i X2 = H2;                                                                                         \
                                                                                                                      \
  const simde__m128i W1_RE = simde_mm_load_si128((const simde__m128i *)W12_R3_W1_RE_RE);                              \
                                                                                                                      \
  const simde__m128i W1_IM = twiddle_im_dir_128(simde_mm_load_si128((const simde__m128i *)W12_R3_W1_IM_SIGNED), dir); \
                                                                                                                      \
  const simde__m128i W2_RE = simde_mm_load_si128((const simde__m128i *)W12_R3_W2_RE_RE);                              \
                                                                                                                      \
  const simde__m128i W2_IM = twiddle_im_dir_128(simde_mm_load_si128((const simde__m128i *)W12_R3_W2_IM_SIGNED), dir); \
                                                                                                                      \
  const simde__m128i As = q15_mul_i16_128(A, Q15_INV_SQRT3);                                                          \
                                                                                                                      \
  const simde__m128i B = complex_mul4_prepack_q15_128(X1, W1_RE, W1_IM);                                              \
                                                                                                                      \
  const simde__m128i C = complex_mul4_prepack_q15_128(X2, W2_RE, W2_IM);                                              \
                                                                                                                      \
  const simde__m128i S = simde_mm_adds_epi16(B, C);                                                                   \
  const simde__m128i D = simde_mm_subs_epi16(B, C);                                                                   \
                                                                                                                      \
  const simde__m128i Y0 = simde_mm_adds_epi16(As, S);                                                                 \
                                                                                                                      \
  const simde__m128i halfS = q15_mul_i16_128(S, Q15_HALF);                                                            \
                                                                                                                      \
  const simde__m128i base = simde_mm_subs_epi16(As, halfS);                                                           \
                                                                                                                      \
  /* (-j)^dir sqrt(3)/2 (B - C), rotation folded into the multiply */                                                 \
  const simde__m128i c3D_mj = mul_minus_j_scaled_dir_i16_128(D, Q15_SQRT3_OVER_2, dir);                               \
  const simde__m128i Y1 = simde_mm_adds_epi16(base, c3D_mj);                                                          \
  const simde__m128i Y2 = simde_mm_subs_epi16(base, c3D_mj);                                                          \
                                                                                                                      \
  simde_mm_storeu_si128((simde__m128i *)(dst + 0), Y0);                                                               \
  simde_mm_storeu_si128((simde__m128i *)(dst + 4), Y1);                                                               \
  simde_mm_storeu_si128((simde__m128i *)(dst + 8), Y2);

static inline void dft12_q15_128(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  /*
   * lane 0 : src[0], src[3], src[6],  src[9]
   * lane 1 : src[1], src[4], src[7],  src[10]
   * lane 2 : src[2], src[5], src[8],  src[11]
   * lane 3 : dummy
   */
  const simde__m128i x0 = pack3_complex_plus_zero_c16(src[0], src[1], src[2]);
  const simde__m128i x1 = pack3_complex_plus_zero_c16(src[3], src[4], src[5]);
  const simde__m128i x2 = pack3_complex_plus_zero_c16(src[6], src[7], src[8]);
  const simde__m128i x3 = pack3_complex_plus_zero_c16(src[9], src[10], src[11]);

  DFT12_SCALAR_FINISH()
}

OAI_DFT_SMALL_HOT void dft12(int16_t *input, int16_t *output, uint8_t scale_flag);

static inline void dft12_q15_128_strided(const c16_t *src, int stride, c16_t *dst, dft_dir_t dir)
{
  const simde__m128i x0 = pack3_complex_plus_zero_c16(src[0 * stride], src[1 * stride], src[2 * stride]);

  const simde__m128i x1 = pack3_complex_plus_zero_c16(src[3 * stride], src[4 * stride], src[5 * stride]);

  const simde__m128i x2 = pack3_complex_plus_zero_c16(src[6 * stride], src[7 * stride], src[8 * stride]);

  const simde__m128i x3 = pack3_complex_plus_zero_c16(src[9 * stride], src[10 * stride], src[11 * stride]);

  DFT12_SCALAR_FINISH()
}

/* Execute two independent DFT12 transforms in the two 128-bit lanes of one
 * AVX2 register.  AVX2 integer arithmetic and unpack operations below are
 * lane-local, so each transform keeps exactly the same Q15 rounding and
 * saturation sequence as dft12_q15_128_strided(). */
static inline simde__m256i join_m128_lanes(simde__m128i lo, simde__m128i hi)
{
  return simde_mm256_inserti128_si256(simde_mm256_castsi128_si256(lo), hi, 1);
}

/* Unitary DFT4 butterflies, instantiated per vector width. */

DEFINE_UNITARY_DFT4(dft4x4_q15_256_lanes,
                    ,
                    simde__m256i,
                    half_q15_256,
                    simde_mm256_adds_epi16,
                    simde_mm256_subs_epi16,
                    simde_mm256_add_epi16,
                    simde_mm256_sub_epi16,
                    add_pm_j_i16_256)

DEFINE_COMPLEX_TRANSPOSE4(transpose4_complex_i16_256_lanes,
                          simde__m256i,
                          simde_mm256_unpacklo_epi32,
                          simde_mm256_unpackhi_epi32,
                          simde_mm256_unpacklo_epi64,
                          simde_mm256_unpackhi_epi64)

static inline void dft12_q15_256_batch2_strided(const c16_t *src0,
                                                int stride0,
                                                c16_t *dst0,
                                                const c16_t *src1,
                                                int stride1,
                                                c16_t *dst1,
                                                dft_dir_t dir)
{
#define DFT12_PACK_PAIR(BASE)                                                                                                      \
  join_m128_lanes(pack3_complex_plus_zero_c16(src0[(BASE + 0) * stride0], src0[(BASE + 1) * stride0], src0[(BASE + 2) * stride0]), \
                  pack3_complex_plus_zero_c16(src1[(BASE + 0) * stride1], src1[(BASE + 1) * stride1], src1[(BASE + 2) * stride1]))

  const simde__m256i x0 = DFT12_PACK_PAIR(0);
  const simde__m256i x1 = DFT12_PACK_PAIR(3);
  const simde__m256i x2 = DFT12_PACK_PAIR(6);
  const simde__m256i x3 = DFT12_PACK_PAIR(9);
#undef DFT12_PACK_PAIR

  simde__m256i H0, H1, H2, H3;
  dft4x4_q15_256_lanes(x0, x1, x2, x3, &H0, &H1, &H2, &H3, dir);
  transpose4_complex_i16_256_lanes(&H0, &H1, &H2, &H3);

  const simde__m256i W1_RE = simde_mm256_broadcastsi128_si256(simde_mm_load_si128((const simde__m128i *)W12_R3_W1_RE_RE));
  const simde__m256i W1_IM =
      simde_mm256_broadcastsi128_si256(twiddle_im_dir_128(simde_mm_load_si128((const simde__m128i *)W12_R3_W1_IM_SIGNED), dir));
  const simde__m256i W2_RE = simde_mm256_broadcastsi128_si256(simde_mm_load_si128((const simde__m128i *)W12_R3_W2_RE_RE));
  const simde__m256i W2_IM =
      simde_mm256_broadcastsi128_si256(twiddle_im_dir_128(simde_mm_load_si128((const simde__m128i *)W12_R3_W2_IM_SIGNED), dir));

  const simde__m256i As = simde_mm256_mulhrs_epi16(H0, simde_mm256_set1_epi16(Q15_INV_SQRT3));
  const simde__m256i B = complex_mul8_prepack_q15_256(H1, W1_RE, W1_IM);
  const simde__m256i C = complex_mul8_prepack_q15_256(H2, W2_RE, W2_IM);

  const simde__m256i S = simde_mm256_adds_epi16(B, C);
  const simde__m256i D = simde_mm256_subs_epi16(B, C);
  const simde__m256i Y0 = simde_mm256_adds_epi16(As, S);
  const simde__m256i halfS = half_q15_256(S);
  const simde__m256i base = simde_mm256_subs_epi16(As, halfS);
  /* (-j)^dir sqrt(3)/2 (B - C), rotation folded into the multiply */
  const simde__m256i c3D_mj = mul_minus_j_scaled_dir_i16_256(D, Q15_SQRT3_OVER_2, dir);
  const simde__m256i Y1 = simde_mm256_adds_epi16(base, c3D_mj);
  const simde__m256i Y2 = simde_mm256_subs_epi16(base, c3D_mj);

#define DFT12_STORE_PAIR(V, OFF)                                                                  \
  do {                                                                                            \
    simde_mm_storeu_si128((simde__m128i *)(dst0 + (OFF)), simde_mm256_castsi256_si128(V));        \
    simde_mm_storeu_si128((simde__m128i *)(dst1 + (OFF)), simde_mm256_extracti128_si256((V), 1)); \
  } while (0)

  DFT12_STORE_PAIR(Y0, 0);
  DFT12_STORE_PAIR(Y1, 4);
  DFT12_STORE_PAIR(Y2, 8);
#undef DFT12_STORE_PAIR
}

/* W32^(1*j), j = 0..7 */

static const int16_t W32_1_RE_RE_LO[8] __attribute__((aligned(16))) = {Q15_ONE,
                                                                       Q15_ONE,
                                                                       DFT32_Q15_COS_PI_16,
                                                                       DFT32_Q15_COS_PI_16,
                                                                       Q15_COS_PI_8,
                                                                       Q15_COS_PI_8,
                                                                       DFT32_Q15_COS_3PI_16,
                                                                       DFT32_Q15_COS_3PI_16};

static const int16_t W32_1_IM_SIGNED_LO[8] __attribute__((aligned(16))) =
    {0, 0, DFT32_Q15_SIN_PI_16, -DFT32_Q15_SIN_PI_16, Q15_SIN_PI_8, -Q15_SIN_PI_8, DFT32_Q15_SIN_3PI_16, -DFT32_Q15_SIN_3PI_16};

static const int16_t W32_1_RE_RE_HI[8] __attribute__((aligned(16))) = {Q15_INV_SQRT2,
                                                                       Q15_INV_SQRT2,
                                                                       DFT32_Q15_SIN_3PI_16,
                                                                       DFT32_Q15_SIN_3PI_16,
                                                                       Q15_SIN_PI_8,
                                                                       Q15_SIN_PI_8,
                                                                       DFT32_Q15_SIN_PI_16,
                                                                       DFT32_Q15_SIN_PI_16};

static const int16_t W32_1_IM_SIGNED_HI[8] __attribute__((aligned(16))) = {Q15_INV_SQRT2,
                                                                           -Q15_INV_SQRT2,
                                                                           DFT32_Q15_COS_3PI_16,
                                                                           -DFT32_Q15_COS_3PI_16,
                                                                           Q15_COS_PI_8,
                                                                           -Q15_COS_PI_8,
                                                                           DFT32_Q15_COS_PI_16,
                                                                           -DFT32_Q15_COS_PI_16};

/* W32^(2*j), j = 0..7 */

static const int16_t W32_2_RE_RE_LO[8] __attribute__((
    aligned(16))) = {Q15_ONE, Q15_ONE, Q15_COS_PI_8, Q15_COS_PI_8, Q15_INV_SQRT2, Q15_INV_SQRT2, Q15_SIN_PI_8, Q15_SIN_PI_8};

static const int16_t W32_2_IM_SIGNED_LO[8]
    __attribute__((aligned(16))) = {0, 0, Q15_SIN_PI_8, -Q15_SIN_PI_8, Q15_INV_SQRT2, -Q15_INV_SQRT2, Q15_COS_PI_8, -Q15_COS_PI_8};

static const int16_t W32_2_RE_RE_HI[8] __attribute__((
    aligned(16))) = {0, 0, -Q15_SIN_PI_8, -Q15_SIN_PI_8, -Q15_INV_SQRT2, -Q15_INV_SQRT2, -Q15_COS_PI_8, -Q15_COS_PI_8};

static const int16_t W32_2_IM_SIGNED_HI[8] __attribute__((
    aligned(16))) = {Q15_ONE, -Q15_ONE, Q15_COS_PI_8, -Q15_COS_PI_8, Q15_INV_SQRT2, -Q15_INV_SQRT2, Q15_SIN_PI_8, -Q15_SIN_PI_8};

/* W32^(3*j), j = 0..7 */

static const int16_t W32_3_RE_RE_LO[8] __attribute__((aligned(16))) = {Q15_ONE,
                                                                       Q15_ONE,
                                                                       DFT32_Q15_COS_3PI_16,
                                                                       DFT32_Q15_COS_3PI_16,
                                                                       Q15_SIN_PI_8,
                                                                       Q15_SIN_PI_8,
                                                                       -DFT32_Q15_SIN_PI_16,
                                                                       -DFT32_Q15_SIN_PI_16};

static const int16_t W32_3_IM_SIGNED_LO[8] __attribute__((aligned(16))) =
    {0, 0, DFT32_Q15_SIN_3PI_16, -DFT32_Q15_SIN_3PI_16, Q15_COS_PI_8, -Q15_COS_PI_8, DFT32_Q15_COS_PI_16, -DFT32_Q15_COS_PI_16};

static const int16_t W32_3_RE_RE_HI[8] __attribute__((aligned(16))) = {-Q15_INV_SQRT2,
                                                                       -Q15_INV_SQRT2,
                                                                       -DFT32_Q15_COS_PI_16,
                                                                       -DFT32_Q15_COS_PI_16,
                                                                       -Q15_COS_PI_8,
                                                                       -Q15_COS_PI_8,
                                                                       -DFT32_Q15_SIN_3PI_16,
                                                                       -DFT32_Q15_SIN_3PI_16};

static const int16_t W32_3_IM_SIGNED_HI[8] __attribute__((aligned(16))) = {Q15_INV_SQRT2,
                                                                           -Q15_INV_SQRT2,
                                                                           DFT32_Q15_SIN_PI_16,
                                                                           -DFT32_Q15_SIN_PI_16,
                                                                           -Q15_SIN_PI_8,
                                                                           Q15_SIN_PI_8,
                                                                           -DFT32_Q15_COS_3PI_16,
                                                                           DFT32_Q15_COS_3PI_16};

/* Two scaled DFT4s followed by the same unitary radix-2 combine. */
#define DFT8_POINTER_OUTPUTS(VEC) VEC *Y0, VEC *Y1, VEC *Y2, VEC *Y3, VEC *Y4, VEC *Y5, VEC *Y6, VEC *Y7

#define DFT8_ARRAY_OUTPUTS(VEC) VEC Y[8]

#define DFT8_POINTER(I) *Y##I

#define DFT8_ARRAY(I) Y[I]

#define DFT8_SCALE_128(X) q15_mul_i16_128(X, Q15_INV_SQRT2)

#define DFT8_SCALE_256(X) simde_mm256_mulhrs_epi16(X, qinv)

#define DEFINE_UNITARY_DFT8(FN, ATTR, VEC, OUTPUT_ARGS, DFT4, SETUP, SCALE, CMUL, OUTPUT, ADD, SUB) \
  static ATTR inline void FN(const VEC x0,                                                          \
                             const VEC x1,                                                          \
                             const VEC x2,                                                          \
                             const VEC x3,                                                          \
                             const VEC x4,                                                          \
                             const VEC x5,                                                          \
                             const VEC x6,                                                          \
                             const VEC x7,                                                          \
                             OUTPUT_ARGS(VEC),                                                      \
                             dft_dir_t dir)                                                         \
  {                                                                                                 \
    VEC E0, E1, E2, E3;                                                                             \
    VEC O0, O1, O2, O3;                                                                             \
    DFT4(x0, x2, x4, x6, &E0, &E1, &E2, &E3, dir);                                                  \
    DFT4(x1, x3, x5, x7, &O0, &O1, &O2, &O3, dir);                                                  \
    SETUP                                                                                           \
    const VEC E0s = SCALE(E0);                                                                      \
    const VEC E1s = SCALE(E1);                                                                      \
    const VEC E2s = SCALE(E2);                                                                      \
    const VEC E3s = SCALE(E3);                                                                      \
    const VEC T0 = SCALE(O0);                                                                       \
    const VEC T1 = CMUL(O1, Q15_HALF_TW, twiddle_im_scalar_dir_i16(-Q15_HALF_TW, dir));             \
    const VEC T2 = CMUL(O2, 0, twiddle_im_scalar_dir_i16(-Q15_INV_SQRT2, dir));                     \
    const VEC T3 = CMUL(O3, -Q15_HALF_TW, twiddle_im_scalar_dir_i16(-Q15_HALF_TW, dir));            \
    OUTPUT(0) = ADD(E0s, T0);                                                                       \
    OUTPUT(4) = SUB(E0s, T0);                                                                       \
    OUTPUT(1) = ADD(E1s, T1);                                                                       \
    OUTPUT(5) = SUB(E1s, T1);                                                                       \
    OUTPUT(2) = ADD(E2s, T2);                                                                       \
    OUTPUT(6) = SUB(E2s, T2);                                                                       \
    OUTPUT(3) = ADD(E3s, T3);                                                                       \
    OUTPUT(7) = SUB(E3s, T3);                                                                       \
  }

DEFINE_UNITARY_DFT8(dft8x4_q15_128,
                    ,
                    simde__m128i,
                    DFT8_POINTER_OUTPUTS,
                    dft4x4_q15_128,
                    ,
                    DFT8_SCALE_128,
                    complex_mul4_bcast_q15_128,
                    DFT8_POINTER,
                    simde_mm_adds_epi16,
                    simde_mm_subs_epi16)

#define DFT32_AVX2_TW __attribute__((section(".rodata.oai_dft32_avx2"), aligned(32)))

static const int16_t W32_1_RE_RE_256[16] DFT32_AVX2_TW = {Q15_ONE,
                                                          Q15_ONE,
                                                          DFT32_Q15_COS_PI_16,
                                                          DFT32_Q15_COS_PI_16,
                                                          Q15_COS_PI_8,
                                                          Q15_COS_PI_8,
                                                          DFT32_Q15_COS_3PI_16,
                                                          DFT32_Q15_COS_3PI_16,
                                                          Q15_INV_SQRT2,
                                                          Q15_INV_SQRT2,
                                                          DFT32_Q15_SIN_3PI_16,
                                                          DFT32_Q15_SIN_3PI_16,
                                                          Q15_SIN_PI_8,
                                                          Q15_SIN_PI_8,
                                                          DFT32_Q15_SIN_PI_16,
                                                          DFT32_Q15_SIN_PI_16};

static const int16_t W32_1_IM_SIGNED_256[16] DFT32_AVX2_TW = {0,
                                                              0,
                                                              DFT32_Q15_SIN_PI_16,
                                                              -DFT32_Q15_SIN_PI_16,
                                                              Q15_SIN_PI_8,
                                                              -Q15_SIN_PI_8,
                                                              DFT32_Q15_SIN_3PI_16,
                                                              -DFT32_Q15_SIN_3PI_16,
                                                              Q15_INV_SQRT2,
                                                              -Q15_INV_SQRT2,
                                                              DFT32_Q15_COS_3PI_16,
                                                              -DFT32_Q15_COS_3PI_16,
                                                              Q15_COS_PI_8,
                                                              -Q15_COS_PI_8,
                                                              DFT32_Q15_COS_PI_16,
                                                              -DFT32_Q15_COS_PI_16};

static const int16_t W32_2_RE_RE_256[16] DFT32_AVX2_TW = {Q15_ONE,
                                                          Q15_ONE,
                                                          Q15_COS_PI_8,
                                                          Q15_COS_PI_8,
                                                          Q15_INV_SQRT2,
                                                          Q15_INV_SQRT2,
                                                          Q15_SIN_PI_8,
                                                          Q15_SIN_PI_8,
                                                          0,
                                                          0,
                                                          -Q15_SIN_PI_8,
                                                          -Q15_SIN_PI_8,
                                                          -Q15_INV_SQRT2,
                                                          -Q15_INV_SQRT2,
                                                          -Q15_COS_PI_8,
                                                          -Q15_COS_PI_8};

static const int16_t W32_2_IM_SIGNED_256[16] DFT32_AVX2_TW = {0,
                                                              0,
                                                              Q15_SIN_PI_8,
                                                              -Q15_SIN_PI_8,
                                                              Q15_INV_SQRT2,
                                                              -Q15_INV_SQRT2,
                                                              Q15_COS_PI_8,
                                                              -Q15_COS_PI_8,
                                                              Q15_ONE,
                                                              -Q15_ONE,
                                                              Q15_COS_PI_8,
                                                              -Q15_COS_PI_8,
                                                              Q15_INV_SQRT2,
                                                              -Q15_INV_SQRT2,
                                                              Q15_SIN_PI_8,
                                                              -Q15_SIN_PI_8};

static const int16_t W32_3_RE_RE_256[16] DFT32_AVX2_TW = {Q15_ONE,
                                                          Q15_ONE,
                                                          DFT32_Q15_COS_3PI_16,
                                                          DFT32_Q15_COS_3PI_16,
                                                          Q15_SIN_PI_8,
                                                          Q15_SIN_PI_8,
                                                          -DFT32_Q15_SIN_PI_16,
                                                          -DFT32_Q15_SIN_PI_16,
                                                          -Q15_INV_SQRT2,
                                                          -Q15_INV_SQRT2,
                                                          -DFT32_Q15_COS_PI_16,
                                                          -DFT32_Q15_COS_PI_16,
                                                          -Q15_COS_PI_8,
                                                          -Q15_COS_PI_8,
                                                          -DFT32_Q15_SIN_3PI_16,
                                                          -DFT32_Q15_SIN_3PI_16};

static const int16_t W32_3_IM_SIGNED_256[16] DFT32_AVX2_TW = {0,
                                                              0,
                                                              DFT32_Q15_SIN_3PI_16,
                                                              -DFT32_Q15_SIN_3PI_16,
                                                              Q15_COS_PI_8,
                                                              -Q15_COS_PI_8,
                                                              DFT32_Q15_COS_PI_16,
                                                              -DFT32_Q15_COS_PI_16,
                                                              Q15_INV_SQRT2,
                                                              -Q15_INV_SQRT2,
                                                              DFT32_Q15_SIN_PI_16,
                                                              -DFT32_Q15_SIN_PI_16,
                                                              -Q15_SIN_PI_8,
                                                              Q15_SIN_PI_8,
                                                              -DFT32_Q15_COS_3PI_16,
                                                              DFT32_Q15_COS_3PI_16};

#undef DFT32_AVX2_TW

static inline simde__m256i twiddle_im_dir_256(simde__m256i w_im_signed, dft_dir_t dir)
{
  return (dir == DFT_DIR_FORWARD) ? w_im_signed : simde_mm256_sub_epi16(simde_mm256_setzero_si256(), w_im_signed);
}

/* Two independent 4x4 transposes, one in each 128-bit lane. Complex Q15
 * samples are treated as 32-bit lanes. */
static inline void transpose4_complex_i16_256x2_shuffle(simde__m256i *Y0, simde__m256i *Y1, simde__m256i *Y2, simde__m256i *Y3)
{
  const simde__m256 a = simde_mm256_castsi256_ps(*Y0);
  const simde__m256 b = simde_mm256_castsi256_ps(*Y1);
  const simde__m256 c = simde_mm256_castsi256_ps(*Y2);
  const simde__m256 d = simde_mm256_castsi256_ps(*Y3);

  const simde__m256 ab_lo = simde_mm256_shuffle_ps(a, b, _MM_SHUFFLE(1, 0, 1, 0));
  const simde__m256 ab_hi = simde_mm256_shuffle_ps(a, b, _MM_SHUFFLE(3, 2, 3, 2));
  const simde__m256 cd_lo = simde_mm256_shuffle_ps(c, d, _MM_SHUFFLE(1, 0, 1, 0));
  const simde__m256 cd_hi = simde_mm256_shuffle_ps(c, d, _MM_SHUFFLE(3, 2, 3, 2));

  *Y0 = simde_mm256_castps_si256(simde_mm256_shuffle_ps(ab_lo, cd_lo, _MM_SHUFFLE(2, 0, 2, 0)));
  *Y1 = simde_mm256_castps_si256(simde_mm256_shuffle_ps(ab_lo, cd_lo, _MM_SHUFFLE(3, 1, 3, 1)));
  *Y2 = simde_mm256_castps_si256(simde_mm256_shuffle_ps(ab_hi, cd_hi, _MM_SHUFFLE(2, 0, 2, 0)));
  *Y3 = simde_mm256_castps_si256(simde_mm256_shuffle_ps(ab_hi, cd_hi, _MM_SHUFFLE(3, 1, 3, 1)));
}

static inline void combine32_q15_128(const simde__m128i H_lo[4], const simde__m128i H_hi[4], c16_t *dst, dft_dir_t dir)
{
  const simde__m128i W1_RE_LO = simde_mm_load_si128((const simde__m128i *)W32_1_RE_RE_LO);
  const simde__m128i W1_IM_LO = twiddle_im_dir_128(simde_mm_load_si128((const simde__m128i *)W32_1_IM_SIGNED_LO), dir);

  const simde__m128i W1_RE_HI = simde_mm_load_si128((const simde__m128i *)W32_1_RE_RE_HI);
  const simde__m128i W1_IM_HI = twiddle_im_dir_128(simde_mm_load_si128((const simde__m128i *)W32_1_IM_SIGNED_HI), dir);

  const simde__m128i W2_RE_LO = simde_mm_load_si128((const simde__m128i *)W32_2_RE_RE_LO);
  const simde__m128i W2_IM_LO = twiddle_im_dir_128(simde_mm_load_si128((const simde__m128i *)W32_2_IM_SIGNED_LO), dir);

  const simde__m128i W2_RE_HI = simde_mm_load_si128((const simde__m128i *)W32_2_RE_RE_HI);
  const simde__m128i W2_IM_HI = twiddle_im_dir_128(simde_mm_load_si128((const simde__m128i *)W32_2_IM_SIGNED_HI), dir);

  const simde__m128i W3_RE_LO = simde_mm_load_si128((const simde__m128i *)W32_3_RE_RE_LO);
  const simde__m128i W3_IM_LO = twiddle_im_dir_128(simde_mm_load_si128((const simde__m128i *)W32_3_IM_SIGNED_LO), dir);

  const simde__m128i W3_RE_HI = simde_mm_load_si128((const simde__m128i *)W32_3_RE_RE_HI);
  const simde__m128i W3_IM_HI = twiddle_im_dir_128(simde_mm_load_si128((const simde__m128i *)W32_3_IM_SIGNED_HI), dir);

  const simde__m128i A0_lo = H_lo[0];
  const simde__m128i A0_hi = H_hi[0];

  const simde__m128i A1_lo = complex_mul4_prepack_q15_128(H_lo[1], W1_RE_LO, W1_IM_LO);
  const simde__m128i A1_hi = complex_mul4_prepack_q15_128(H_hi[1], W1_RE_HI, W1_IM_HI);

  const simde__m128i A2_lo = complex_mul4_prepack_q15_128(H_lo[2], W2_RE_LO, W2_IM_LO);
  const simde__m128i A2_hi = complex_mul4_prepack_q15_128(H_hi[2], W2_RE_HI, W2_IM_HI);

  const simde__m128i A3_lo = complex_mul4_prepack_q15_128(H_lo[3], W3_RE_LO, W3_IM_LO);
  const simde__m128i A3_hi = complex_mul4_prepack_q15_128(H_hi[3], W3_RE_HI, W3_IM_HI);

  simde__m128i lo0 = A0_lo;
  simde__m128i lo1 = A1_lo;
  simde__m128i lo2 = A2_lo;
  simde__m128i lo3 = A3_lo;

  simde__m128i hi0 = A0_hi;
  simde__m128i hi1 = A1_hi;
  simde__m128i hi2 = A2_hi;
  simde__m128i hi3 = A3_hi;

  transpose4_complex_i16_128(&lo0, &lo1, &lo2, &lo3);
  transpose4_complex_i16_128(&hi0, &hi1, &hi2, &hi3);

  simde__m128i Y0, Y1, Y2, Y3;
  simde__m128i Y4, Y5, Y6, Y7;

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

static inline void dft32_q15_128(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  dft32_q15_256_contiguous(src, dst, dir);
}

static inline void dft32_q15_128_strided(const c16_t *src, int stride, c16_t *dst, dft_dir_t dir)
{
  simde__m128i H_lo[4] __attribute__((aligned(16)));
  simde__m128i H_hi[4] __attribute__((aligned(16)));

  const simde__m128i x0_lo = load4_complex_strided_c16(src, stride, 0);
  const simde__m128i x0_hi = load4_complex_strided_c16(src, stride, 4);

  const simde__m128i x1_lo = load4_complex_strided_c16(src, stride, 8);
  const simde__m128i x1_hi = load4_complex_strided_c16(src, stride, 12);

  const simde__m128i x2_lo = load4_complex_strided_c16(src, stride, 16);
  const simde__m128i x2_hi = load4_complex_strided_c16(src, stride, 20);

  const simde__m128i x3_lo = load4_complex_strided_c16(src, stride, 24);
  const simde__m128i x3_hi = load4_complex_strided_c16(src, stride, 28);

  dft4x4_q15_128(x0_lo, x1_lo, x2_lo, x3_lo, &H_lo[0], &H_lo[1], &H_lo[2], &H_lo[3], dir);

  dft4x4_q15_128(x0_hi, x1_hi, x2_hi, x3_hi, &H_hi[0], &H_hi[1], &H_hi[2], &H_hi[3], dir);

  combine32_q15_128(H_lo, H_hi, dst, dir);
}

static inline void radix3_combine4_q15_128_scaled(simde__m128i A,
                                                  simde__m128i X1,
                                                  simde__m128i X2,
                                                  simde__m128i w1_re,
                                                  simde__m128i w1_im,
                                                  simde__m128i w2_re,
                                                  simde__m128i w2_im,
                                                  simde__m128i *Y0,
                                                  simde__m128i *Y1,
                                                  simde__m128i *Y2,
                                                  dft_dir_t dir)
{
  const simde__m128i As = q15_mul_i16_128(A, Q15_INV_SQRT3);

  /*
   * B[k] = W_N^k    * X1[k] / sqrt(3)
   * C[k] = W_N^(2k) * X2[k] / sqrt(3)
   */
  const simde__m128i B = complex_mul4_prepack_q15_128(X1, w1_re, w1_im);

  const simde__m128i C = complex_mul4_prepack_q15_128(X2, w2_re, w2_im);

  const simde__m128i S = simde_mm_adds_epi16(B, C);
  const simde__m128i D = simde_mm_subs_epi16(B, C);

  /*
   * Y0 = A + B + C
   */
  *Y0 = simde_mm_adds_epi16(As, S);

  /*
   * base = A - 1/2 * (B + C)
   */
  const simde__m128i halfS = q15_mul_i16_128(S, Q15_HALF);

  const simde__m128i base = simde_mm_subs_epi16(As, halfS);

  /*
   * c3D_mj = (-j)^dir sqrt(3)/2 * (B - C) (rotation folded into the multiply)
   * Y1 = base + c3D_mj, Y2 = base - c3D_mj
   */
  const simde__m128i c3D_mj = mul_minus_j_scaled_dir_i16_128(D, Q15_SQRT3_OVER_2, dir);
  *Y1 = simde_mm_adds_epi16(base, c3D_mj);
  *Y2 = simde_mm_subs_epi16(base, c3D_mj);
}

static inline void dft24_pfa3x8_q15_128(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  static const int input_index[8][3] = {
      {0, 8, 16},
      {3, 11, 19},
      {6, 14, 22},
      {9, 17, 1},
      {12, 20, 4},
      {15, 23, 7},
      {18, 2, 10},
      {21, 5, 13},
  };
  static const int16_t scale_re[8] __attribute__((aligned(16))) = {
      Q15_INV_SQRT3,
      Q15_INV_SQRT3,
      Q15_INV_SQRT3,
      Q15_INV_SQRT3,
      Q15_INV_SQRT3,
      Q15_INV_SQRT3,
      Q15_INV_SQRT3,
      Q15_INV_SQRT3,
  };
  static const int16_t scale_im[8] __attribute__((aligned(16))) = {0};
  simde__m128i x[8], h[8], y[3][2];

  for (int n8 = 0; n8 < 8; n8++)
    x[n8] = pack3_complex_plus_zero_c16(src[input_index[n8][0]], src[input_index[n8][1]], src[input_index[n8][2]]);

  dft8x4_q15_128(x[0], x[1], x[2], x[3], x[4], x[5], x[6], x[7], &h[0], &h[1], &h[2], &h[3], &h[4], &h[5], &h[6], &h[7], dir);

  const simde__m128i wr = simde_mm_load_si128((const simde__m128i *)scale_re);
  const simde__m128i wi = simde_mm_load_si128((const simde__m128i *)scale_im);
  for (int half = 0; half < 2; half++) {
    const int off = 4 * half;
    transpose4_complex_i16_128(&h[off + 0], &h[off + 1], &h[off + 2], &h[off + 3]);
    radix3_combine4_q15_128_scaled(h[off + 0], h[off + 1], h[off + 2], wr, wi, wr, wi, &y[0][half], &y[1][half], &y[2][half], dir);
  }

  const simde__m256i b0 = simde_mm256_inserti128_si256(simde_mm256_castsi128_si256(y[0][0]), y[0][1], 1);
  const simde__m256i b1 = simde_mm256_inserti128_si256(simde_mm256_castsi128_si256(y[1][0]), y[1][1], 1);
  const simde__m256i b2 = simde_mm256_inserti128_si256(simde_mm256_castsi128_si256(y[2][0]), y[2][1], 1);
  const simde__m256i out0 = simde_mm256_blend_epi32(simde_mm256_blend_epi32(b0, b1, 0x92), b2, 0x24);
  const simde__m256i out8 = simde_mm256_blend_epi32(simde_mm256_blend_epi32(b0, b1, 0x24), b2, 0x49);
  const simde__m256i out16 = simde_mm256_blend_epi32(simde_mm256_blend_epi32(b1, b2, 0x92), b0, 0x24);
  simde_mm256_storeu_si256((simde__m256i *)(dst + 0), out0);
  simde_mm256_storeu_si256((simde__m256i *)(dst + 8), out8);
  simde_mm256_storeu_si256((simde__m256i *)(dst + 16), out16);
}

#define DFT_R5_ACCUMULATE_LEFT(ADD, A, BC) ADD(A, BC)

/* Radix-5 combines, instantiated per vector width and scaling policy. */
#define DEFINE_RADIX5_COMBINE(FN, VEC, ADD, SUB, CMUL, SCALE, PMJ, ACCUMULATE, C0, C1, C2, S1, S2) \
  static inline void FN(VEC A,                                                                     \
                        VEC X1,                                                                    \
                        VEC X2,                                                                    \
                        VEC X3,                                                                    \
                        VEC X4,                                                                    \
                        VEC w1_re,                                                                 \
                        VEC w1_im,                                                                 \
                        VEC w2_re,                                                                 \
                        VEC w2_im,                                                                 \
                        VEC w3_re,                                                                 \
                        VEC w3_im,                                                                 \
                        VEC w4_re,                                                                 \
                        VEC w4_im,                                                                 \
                        VEC *Y0,                                                                   \
                        VEC *Y1,                                                                   \
                        VEC *Y2,                                                                   \
                        VEC *Y3,                                                                   \
                        VEC *Y4,                                                                   \
                        dft_dir_t dir)                                                             \
  {                                                                                                \
    /* X1..X4 use twiddles scaled by 1/sqrt(5); A is scaled here. */                               \
    const VEC B = CMUL(X1, w1_re, w1_im);                                                          \
    const VEC C = CMUL(X2, w2_re, w2_im);                                                          \
    const VEC D = CMUL(X3, w3_re, w3_im);                                                          \
    const VEC E = CMUL(X4, w4_re, w4_im);                                                          \
                                                                                                   \
    const VEC BE = ADD(B, E);                                                                      \
    const VEC BEminus = SUB(B, E);                                                                 \
    const VEC CD = ADD(C, D);                                                                      \
    const VEC CDminus = SUB(C, D);                                                                 \
    const VEC As = SCALE(A, C0);                                                                   \
    *Y0 = ACCUMULATE(ADD, As, ADD(BE, CD));                                                        \
                                                                                                   \
    const VEC base1 = ACCUMULATE(ADD, As, ADD(SCALE(BE, C1), SCALE(CD, C2)));                      \
    /* (-j)^dir rotation folded into the S1/S2 multiplies (PMJ: rotating multiply) */              \
    const VEC rot1 = ADD(PMJ(BEminus, S1, dir), PMJ(CDminus, S2, dir));                            \
    *Y1 = ADD(base1, rot1);                                                                        \
    *Y4 = SUB(base1, rot1);                                                                        \
                                                                                                   \
    const VEC base2 = ACCUMULATE(ADD, As, ADD(SCALE(BE, C2), SCALE(CD, C1)));                      \
    const VEC rot2 = SUB(PMJ(BEminus, S2, dir), PMJ(CDminus, S1, dir));                            \
    *Y2 = ADD(base2, rot2);                                                                        \
    *Y3 = SUB(base2, rot2);                                                                        \
  }

static inline void dft16_q15_128_from_regs(const simde__m128i x0,
                                           const simde__m128i x1,
                                           const simde__m128i x2,
                                           const simde__m128i x3,
                                           c16_t *dst,
                                           dft_dir_t dir)
{
  simde__m128i H[4] __attribute__((aligned(16)));

  dft4x4_q15_128(x0, x1, x2, x3, &H[0], &H[1], &H[2], &H[3], dir);

  combine16_q15_128(H, dst, dir);
}

static inline void dft16_q15_128_strided(const c16_t *src, int stride, c16_t *dst, dft_dir_t dir)
{
  const simde__m128i x0 = load4_complex_strided_c16(src, stride, 0);

  const simde__m128i x1 = load4_complex_strided_c16(src, stride, 4);

  const simde__m128i x2 = load4_complex_strided_c16(src, stride, 8);

  const simde__m128i x3 = load4_complex_strided_c16(src, stride, 12);

  dft16_q15_128_from_regs(x0, x1, x2, x3, dst, dir);
}

static simde__m256i w16_re[8] __attribute__((aligned(64)));
static simde__m256i w16_im[2][8] __attribute__((aligned(64)));
/* W16 coefficients with the R16 1/sqrt(2) factor folded in.
 * Paths using this table combine normalization and W16 multiplication in one
 * Q15 operation; other paths apply W16 and 1/sqrt(2) as two multiplies. */
static simde__m256i w16_s2_re[8] __attribute__((aligned(64)));
static simde__m256i w16_s2_im[2][8] __attribute__((aligned(64)));

static simde__m256i n48_r4p12_scale8 __attribute__((aligned(32)));

static simde__m256i n48_r8p6_re[6] __attribute__((aligned(64)));
static simde__m256i n48_r8p6_im[2][6] __attribute__((aligned(64)));
static simde__m256i dft64_re[8][8] __attribute__((aligned(64)));
static simde__m256i dft64_im[2][8][8] __attribute__((aligned(64)));
static simde__m256i dft256_re[16][2] __attribute__((aligned(64)));
static simde__m256i dft256_im[2][16][2] __attribute__((aligned(64)));
static simde__m256i dft512_re[8][8] __attribute__((aligned(64)));
static simde__m256i dft512_im[2][8][8] __attribute__((aligned(64)));
static simde__m256i dft1024_re[16][8] __attribute__((aligned(64)));
static simde__m256i dft1024_im[2][16][8] __attribute__((aligned(64)));

static inline simde__m256i twiddle_re_vec(int16_t wr)
{
  return simde_mm256_set1_epi16(wr);
}

static inline simde__m256i twiddle_im_vec(int16_t wi)
{
  return simde_mm256_setr_epi16(-wi, wi, -wi, wi, -wi, wi, -wi, wi, -wi, wi, -wi, wi, -wi, wi, -wi, wi);
}

/* A table builder, run once at library load (DFT_TABLE_BUILDER). */
#define DEFINE_Q15_TABLE_INITIALIZER(FN, ...) \
  static DFT_TABLE_BUILDER void FN(void)      \
  {                                           \
    __VA_ARGS__                               \
  }

#define DFT_SELECTED_CHILD_TABLES(RADIX, BLOCKS, N_FLOAT, TABLE, SCALE)             \
  for (int r = 0; r < RADIX; r++) {                                                 \
    for (int b = 0; b < BLOCKS; b++) {                                              \
      int16_t re[16] __attribute__((aligned(32)));                                  \
      int16_t im[16] __attribute__((aligned(32)));                                  \
      for (int lane = 0; lane < 8; lane++) {                                        \
        const int n = 8 * b + lane;                                                 \
        const float a = (float)dir * 2.0f * (float)M_PI * (float)(r * n) / N_FLOAT; \
        const int16_t wr = q15_from_float(cosf(a) * (SCALE));                       \
        const int16_t wi = q15_from_float(sinf(a) * (SCALE));                       \
        re[2 * lane] = wr;                                                          \
        re[2 * lane + 1] = wr;                                                      \
        im[2 * lane] = -wi;                                                         \
        im[2 * lane + 1] = wi;                                                      \
      }                                                                             \
      TABLE##_re[r][b] = simde_mm256_load_si256((const simde__m256i *)re);          \
      TABLE##_im[ds][r][b] = simde_mm256_load_si256((const simde__m256i *)im);      \
    }                                                                               \
  }

DEFINE_Q15_TABLE_INITIALIZER(
    power2_leaf_twiddles_init, const float s2_fold = 1.0f / sqrtf(2.0f); for (int ds = 0; ds < 2; ds++) {
      const dft_dir_t dir = ds == 0 ? DFT_DIR_FORWARD : DFT_DIR_INVERSE;

      for (int q = 0; q < 8; q++) {
        const float a = (float)dir * 2.0f * (float)M_PI * (float)q / 16.0f;
        const int16_t wr = q15_from_float(cosf(a));
        const int16_t wi = q15_from_float(sinf(a));
        w16_re[q] = twiddle_re_vec(wr);
        w16_im[ds][q] = twiddle_im_vec(wi);
        w16_s2_re[q] = twiddle_re_vec(q15_from_float(cosf(a) * s2_fold));
        w16_s2_im[ds][q] = twiddle_im_vec(q15_from_float(sinf(a) * s2_fold));
      }

      {
        /* The DFT6 in dft48_radix8_pfa6 carries 1/sqrt(3); the twiddles carry the remaining 1/4. */
        const float scale48 = 0.25f;
        n48_r4p12_scale8 = simde_mm256_set1_epi16(q15_from_float(scale48));
        for (int k2 = 0; k2 < 6; k2++) {
          n48_r8p6_re[k2] = pack8_twiddle_q15_re_re_scaled(0, k2, 48, scale48);
          n48_r8p6_im[ds][k2] = pack8_twiddle_q15_im_signed_scaled(0, k2, 48, scale48, dir);
        }
      }

      for (int r = 0; r < 8; r++) {
        for (int q = 0; q < 8; q++) {
          const float a = (float)dir * 2.0f * (float)M_PI * (float)(r * q) / 64.0f;
          const int16_t wr = q15_from_float(cosf(a) * 0.25f);
          const int16_t wi = q15_from_float(sinf(a) * 0.25f);
          dft64_re[r][q] = twiddle_re_vec(wr);
          dft64_im[ds][r][q] = twiddle_im_vec(wi);
        }
      }

      DFT_SELECTED_CHILD_TABLES(16, 2, 256.0f, dft256, 1.0f)

      /* Radix-16 stages run unitary DFT8s (unit-magnitude twiddles); the radix-8
       * stages run half-scaled DFT8s, so their twiddles carry 1/sqrt(2). */
      DFT_SELECTED_CHILD_TABLES(8, 8, 512.0f, dft512, s2_fold)

      DFT_SELECTED_CHILD_TABLES(16, 8, 1024.0f, dft1024, 1.0f)
    })

/* Eight DFT64 (one per lane); output k of lane l goes to bin output_radix * k + output_offset + l, stored at
 * dst + bin, or in the NR layout of the pruned API through pout (dft_band_store8). */
static __attribute__((always_inline)) inline void dft64x8_store_body(const simde__m256i x[64],
                                                                     c16_t *dst,
                                                                     int output_radix,
                                                                     int output_offset,
                                                                     const dft_band_t *pout,
                                                                     dft_dir_t dir)
{
  const int ds = dir == DFT_DIR_FORWARD ? 0 : 1;
  simde__m256i t[8][8] __attribute__((aligned(64)));

  {
    simde__m256i h[8];
    const simde__m256i xq[8] = {x[0], x[8], x[16], x[24], x[32], x[40], x[48], x[56]};
    dft8x8_q15_256_half_dir(xq, h, dir);
    t[0][0] = quarter_q15_256(h[0]);
    for (int r = 1; r < 8; r++)
      t[r][0] = simde_mm256_mulhrs_epi16(h[r], dft64_re[r][0]);
  }

  for (int q = 1; q < 8; q++) {
    simde__m256i h[8];
    const simde__m256i xq[8] = {x[q], x[q + 8], x[q + 16], x[q + 24], x[q + 32], x[q + 40], x[q + 48], x[q + 56]};
    dft8x8_q15_256_half_dir(xq, h, dir);
    t[0][q] = quarter_q15_256(h[0]);
    for (int r = 1; r < 8; r++)
      t[r][q] = complex_mul8_prepack_small_q15_256(h[r], dft64_re[r][q], dft64_im[ds][r][q]);
  }

  for (int r = 0; r < 8; r++) {
    simde__m256i y[8];
    dft8x8_q15_256_dir(t[r][0],
                       t[r][1],
                       t[r][2],
                       t[r][3],
                       t[r][4],
                       t[r][5],
                       t[r][6],
                       t[r][7],
                       &y[0],
                       &y[1],
                       &y[2],
                       &y[3],
                       &y[4],
                       &y[5],
                       &y[6],
                       &y[7],
                       dir);
    for (int q = 0; q < 8; q++) {
      const int k = 8 * q + r;
      if (pout) /* swapped: leaf index k ^ 32 = 8 (q ^ 4) + r */
        dft_band_store8(pout, 64 * output_radix, output_radix * (8 * (q ^ 4) + r) + output_offset, q < 4, y[q]);
      else
        simde_mm256_storeu_si256((simde__m256i *)(dst + output_radix * k + output_offset), y[q]);
    }
  }
}

static inline void dft64x8_store(const simde__m256i x[64], c16_t *dst, int output_radix, int output_offset, dft_dir_t dir)
{
  dft64x8_store_body(x, dst, output_radix, output_offset, NULL, dir);
}

#define DFT64X8_EXACT_DC_STAGE()                                                                                                  \
  const int ds = dir == DFT_DIR_FORWARD ? 0 : 1;                                                                                  \
  simde__m256i t[8][8] __attribute__((aligned(64)));                                                                              \
                                                                                                                                  \
  simde__m256i sum_lo = simde_mm256_setzero_si256();                                                                              \
  simde__m256i sum_hi = simde_mm256_setzero_si256();                                                                              \
  const simde__m256i ones16 = simde_mm256_set1_epi16(1);                                                                          \
                                                                                                                                  \
  for (int q = 0; q < 8; q++) {                                                                                                   \
    simde__m256i h[8], s02, s13;                                                                                                  \
    const simde__m256i xq[8] = {x[q], x[q + 8], x[q + 16], x[q + 24], x[q + 32], x[q + 40], x[q + 48], x[q + 56]};                \
    dft8x8_q15_256_half_dc_dir(xq, h, &s02, &s13, dir);                                                                           \
                                                                                                                                  \
    /* (s02 + s13) per component in 32 bits: interleave and add pairs with vpmaddwd. sum_lo holds                                 \
     * lanes 0,1 | 4,5 and sum_hi lanes 2,3 | 6,7, so packing them restores the lane order. */                                    \
    sum_lo = simde_mm256_add_epi32(sum_lo, simde_mm256_madd_epi16(simde_mm256_unpacklo_epi16(s02, s13), ones16));                 \
    sum_hi = simde_mm256_add_epi32(sum_hi, simde_mm256_madd_epi16(simde_mm256_unpackhi_epi16(s02, s13), ones16));                 \
                                                                                                                                  \
    t[0][q] = quarter_q15_256(h[0]);                                                                                              \
    if (q == 0) {                                                                                                                 \
      for (int r = 1; r < 8; r++)                                                                                                 \
        t[r][0] = simde_mm256_mulhrs_epi16(h[r], dft64_re[r][0]);                                                                 \
    } else {                                                                                                                      \
      for (int r = 1; r < 8; r++)                                                                                                 \
        t[r][q] = complex_mul8_prepack_small_q15_256(h[r], dft64_re[r][q], dft64_im[ds][r][q]);                                   \
    }                                                                                                                             \
  }                                                                                                                               \
                                                                                                                                  \
  const simde__m256i round = simde_mm256_set1_epi32(4);                                                                           \
  sum_lo =                                                                                                                        \
      simde_mm256_srai_epi32(simde_mm256_add_epi32(sum_lo, simde_mm256_add_epi32(round, simde_mm256_srai_epi32(sum_lo, 31))), 3); \
  sum_hi =                                                                                                                        \
      simde_mm256_srai_epi32(simde_mm256_add_epi32(sum_hi, simde_mm256_add_epi32(round, simde_mm256_srai_epi32(sum_hi, 31))), 3); \
                                                                                                                                  \
  const simde__m256i dc = simde_mm256_packs_epi32(sum_lo, sum_hi);

#define DFT_DC_STORE_FULL(ADDRESS, Q, R, VALUE) simde_mm256_storeu_si256((simde__m256i *)(ADDRESS), VALUE)
#define DFT_DC_STORE_MASKED(ADDRESS, Q, R, VALUE) store_complex_lanes_256(ADDRESS, VALUE, valid_lanes)
#define DFT_DC_MASK_PARAM int valid_lanes,
#define DEFINE_DFT64X8_DC_STORE(FN, MASK_PARAM, STORE)                           \
  static __attribute__((always_inline)) inline void FN(const simde__m256i x[64], \
                                                       c16_t *dst,               \
                                                       int output_radix,         \
                                                       int output_offset,        \
                                                       MASK_PARAM dft_dir_t dir) \
  {                                                                              \
    DFT64X8_EXACT_DC_STAGE()                                                     \
                                                                                 \
    for (int r = 0; r < 8; r++) {                                                \
      simde__m256i y[8];                                                         \
      dft8x8_q15_256_dir(t[r][0],                                                \
                         t[r][1],                                                \
                         t[r][2],                                                \
                         t[r][3],                                                \
                         t[r][4],                                                \
                         t[r][5],                                                \
                         t[r][6],                                                \
                         t[r][7],                                                \
                         &y[0],                                                  \
                         &y[1],                                                  \
                         &y[2],                                                  \
                         &y[3],                                                  \
                         &y[4],                                                  \
                         &y[5],                                                  \
                         &y[6],                                                  \
                         &y[7],                                                  \
                         dir);                                                   \
      for (int q = 0; q < 8; q++) {                                              \
        if (r == 0 && q == 0)                                                    \
          continue; /* exact DC is stored below */                               \
        STORE(dst + output_radix * (8 * q + r) + output_offset, q, r, y[q]);     \
      }                                                                          \
    }                                                                            \
                                                                                 \
    STORE(dst + output_offset, 0, 0, dc);                                        \
  }

DEFINE_DFT64X8_DC_STORE(dft64x8_store_with_dc, , DFT_DC_STORE_FULL)

DEFINE_DFT64X8_DC_STORE(dft64x8_store_with_dc_masked, DFT_DC_MASK_PARAM, DFT_DC_STORE_MASKED)

/* Pruned API, NR output: leaf index 8 Q + R at swapped index 8 (Q ^ 4) + R. */
#define DFT_DC_STORE_BAND(ADDRESS, Q, R, VALUE) \
  dft_band_store8(pout, 64 * output_radix, output_radix * (8 * ((Q) ^ 4) + (R)) + output_offset, (Q) < 4, VALUE)
#define DFT_DC_BAND_PARAM const dft_band_t *pout,
DEFINE_DFT64X8_DC_STORE(dft64x8_store_with_dc_band_body, DFT_DC_BAND_PARAM, DFT_DC_STORE_BAND)
#undef DFT_DC_STORE_BAND
#undef DFT_DC_BAND_PARAM

#define DFT_TRANSPOSE8_TO_ROW(P, DST, POS)                                            \
  transpose8_complex_i16_256(&P##0, &P##1, &P##2, &P##3, &P##4, &P##5, &P##6, &P##7); \
  DST[POS + 0] = P##0;                                                                \
  DST[POS + 1] = P##1;                                                                \
  DST[POS + 2] = P##2;                                                                \
  DST[POS + 3] = P##3;                                                                \
  DST[POS + 4] = P##4;                                                                \
  DST[POS + 5] = P##5;                                                                \
  DST[POS + 6] = P##6;                                                                \
  DST[POS + 7] = P##7;

static __attribute__((always_inline)) inline void dft64x6_tail_store_1920(const c16_t *b, c16_t *dst, dft_dir_t dir)
{
  enum { M = 64, R = 30, FIRST = 24 };
  const simde__m256i zero = simde_mm256_setzero_si256();
  simde__m256i leaf[64] __attribute__((aligned(64)));

  for (int n = 0; n < 64; n += 8) {
    simde__m256i z0 = simde_mm256_loadu_si256((const simde__m256i *)(b + (FIRST + 0) * M + n));
    simde__m256i z1 = simde_mm256_loadu_si256((const simde__m256i *)(b + (FIRST + 1) * M + n));
    simde__m256i z2 = simde_mm256_loadu_si256((const simde__m256i *)(b + (FIRST + 2) * M + n));
    simde__m256i z3 = simde_mm256_loadu_si256((const simde__m256i *)(b + (FIRST + 3) * M + n));
    simde__m256i z4 = simde_mm256_loadu_si256((const simde__m256i *)(b + (FIRST + 4) * M + n));
    simde__m256i z5 = simde_mm256_loadu_si256((const simde__m256i *)(b + (FIRST + 5) * M + n));
    simde__m256i z6 = zero;
    simde__m256i z7 = zero;

    DFT_TRANSPOSE8_TO_ROW(z, leaf, n)
  }

  dft64x8_store_with_dc_masked(leaf, dst, R, FIRST, 6, dir);
}

static __attribute__((noinline)) void dft64x8_parent_store_from_branches(const c16_t *b,
                                                                         int M,
                                                                         c16_t *dst,
                                                                         int output_stride,
                                                                         int first_branch,
                                                                         dft_dir_t dir)
{
  simde__m256i leaf[64] __attribute__((aligned(64)));
  for (int n = 0; n < 64; n += 8) {
    simde__m256i z[8];
    for (int br = 0; br < 8; br++)
      z[br] = simde_mm256_loadu_si256((const simde__m256i *)(b + (first_branch + br) * M + n));
    transpose8_complex_i16_256(&z[0], &z[1], &z[2], &z[3], &z[4], &z[5], &z[6], &z[7]);
    for (int lane = 0; lane < 8; lane++)
      leaf[n + lane] = z[lane];
  }
  dft64x8_store_with_dc(leaf, dst, output_stride, first_branch, dir);
}

static __attribute__((always_inline)) inline void dft64x8_branch_major_store_body(const c16_t *b,
                                                                                  int M,
                                                                                  c16_t *dst,
                                                                                  int R,
                                                                                  int first,
                                                                                  dft_dir_t dir)
{
  simde__m256i leaf[64] __attribute__((aligned(64)));
  for (int n = 0; n < 64; n += 8) {
    simde__m256i z[8];
    for (int br = 0; br < 8; br++)
      z[br] = simde_mm256_loadu_si256((const simde__m256i *)(b + (first + br) * M + n));
    transpose8_complex_i16_256(&z[0], &z[1], &z[2], &z[3], &z[4], &z[5], &z[6], &z[7]);
    for (int l = 0; l < 8; l++)
      leaf[n + l] = z[l];
  }
  dft64x8_store_with_dc(leaf, dst, R, first, dir);
}

/* One copy per direction, so the direction is a constant inside. */
static __attribute__((noinline)) void dft64x8_branch_major_store(const c16_t *b, int M, c16_t *dst, int R, int first, dft_dir_t dir)
{
  if (dir == DFT_DIR_FORWARD)
    dft64x8_branch_major_store_body(b, M, dst, R, first, DFT_DIR_FORWARD);
  else
    dft64x8_branch_major_store_body(b, M, dst, R, first, DFT_DIR_INVERSE);
}

/* Both DFT8 halves of the radix-16 split are unitary, so callers apply no
 * further 1/sqrt(8): the R16 parent tables have unit magnitude and DFT16
 * children are used as is. */
#define DFT_R16_SPLIT_CHILDREN()    \
  dft8x8_q15_256_unitary_dir(a[0],  \
                             a[1],  \
                             a[2],  \
                             a[3],  \
                             a[4],  \
                             a[5],  \
                             a[6],  \
                             a[7],  \
                             &e[0], \
                             &e[1], \
                             &e[2], \
                             &e[3], \
                             &e[4], \
                             &e[5], \
                             &e[6], \
                             &e[7], \
                             dir);  \
  dft8x8_q15_256_unitary_dir(b[0],  \
                             b[1],  \
                             b[2],  \
                             b[3],  \
                             b[4],  \
                             b[5],  \
                             b[6],  \
                             b[7],  \
                             &o[0], \
                             &o[1], \
                             &o[2], \
                             &o[3], \
                             &o[4], \
                             &o[5], \
                             &o[6], \
                             &o[7], \
                             dir);

#define DEFINE_SELECTED_DFT16X8(FN, ...)                                                                            \
  static __attribute__((always_inline)) inline void FN(const simde__m256i x[16], simde__m256i y[16], dft_dir_t dir) \
  {                                                                                                                 \
    const int ds = dir == DFT_DIR_FORWARD ? 0 : 1;                                                                  \
    const simde__m256i s2 = simde_mm256_set1_epi16(Q15_INV_SQRT2);                                                  \
    simde__m256i a[8], b[8], e[8], o[8];                                                                            \
                                                                                                                    \
    a[0] = simde_mm256_mulhrs_epi16(simde_mm256_adds_epi16(x[0], x[8]), s2);                                        \
    b[0] = simde_mm256_mulhrs_epi16(simde_mm256_subs_epi16(x[0], x[8]), s2);                                        \
    for (int q = 1; q < 8; q++) {                                                                                   \
      a[q] = simde_mm256_mulhrs_epi16(simde_mm256_adds_epi16(x[q], x[q + 8]), s2);                                  \
      __VA_ARGS__                                                                                                   \
    }                                                                                                               \
                                                                                                                    \
    DFT_R16_SPLIT_CHILDREN()                                                                                        \
                                                                                                                    \
    for (int k = 0; k < 8; k++) {                                                                                   \
      y[2 * k] = e[k];                                                                                              \
      y[2 * k + 1] = o[k];                                                                                          \
    }                                                                                                               \
  }

DEFINE_SELECTED_DFT16X8(dft16x8, b[q] = simde_mm256_mulhrs_epi16(simde_mm256_subs_epi16(x[q], x[q + 8]), s2);
                        b[q] = complex_mul8_prepack_q15_256(b[q], w16_re[q], w16_im[ds][q]);)

/* R16 helper with 1/sqrt(2) folded into the non-trivial W16 coefficients
 * (q = 0 keeps the cheaper real multiply). Used only where it is the more
 * accurate choice; for e.g. 768/2304/3072/18432 the two-multiply form is. */
DEFINE_SELECTED_DFT16X8(dft16x8_w16folded, const simde__m256i d = simde_mm256_subs_epi16(x[q], x[q + 8]);
                        b[q] = complex_mul8_prepack_q15_256(d, w16_s2_re[q], w16_s2_im[ds][q]);)

/* R16 helper for inputs whose peaks can exceed half full scale. The unitary 1/4 is split as 1/2 on every
 * input before the radix-2 layer and 1/2 after the first layer of each DFT8 child (instead of 1/sqrt(2)
 * after the radix-2 sums and 1/sqrt(8) in the children), so no sum can saturate. */
static __attribute__((always_inline)) inline void dft16x8_prescaled(const simde__m256i x[16], simde__m256i y[16], dft_dir_t dir)
{
  const int ds = dir == DFT_DIR_FORWARD ? 0 : 1;
  simde__m256i a[8], b[8], e[8], o[8];
  for (int q = 0; q < 8; q++) {
    const simde__m256i lo = half_q15_256(x[q]), hi = half_q15_256(x[q + 8]);
    a[q] = simde_mm256_adds_epi16(lo, hi);
    b[q] = simde_mm256_subs_epi16(lo, hi);
    if (q)
      b[q] = complex_mul8_prepack_q15_256(b[q], w16_re[q], w16_im[ds][q]);
  }
  dft8x8_q15_256_core(a[0],
                      a[1],
                      a[2],
                      a[3],
                      a[4],
                      a[5],
                      a[6],
                      a[7],
                      &e[0],
                      &e[1],
                      &e[2],
                      &e[3],
                      &e[4],
                      &e[5],
                      &e[6],
                      &e[7],
                      dir,
                      Q15_HALF,
                      NULL,
                      NULL);
  dft8x8_q15_256_core(b[0],
                      b[1],
                      b[2],
                      b[3],
                      b[4],
                      b[5],
                      b[6],
                      b[7],
                      &o[0],
                      &o[1],
                      &o[2],
                      &o[3],
                      &o[4],
                      &o[5],
                      &o[6],
                      &o[7],
                      dir,
                      Q15_HALF,
                      NULL,
                      NULL);
  for (int k = 0; k < 8; k++) {
    y[2 * k] = e[k];
    y[2 * k + 1] = o[k];
  }
}

/* Keep the child normalization choice explicit in both DFT256 specializations. */
#define DFT_R16_W16_ALWAYS() b[q] = complex_mul8_prepack_q15_256(b[q], w16_re[q], w16_im[ds][q]);
#define DFT_R16_W16_NONZERO() \
  if (q)                      \
  DFT_R16_W16_ALWAYS()
#define DFT_R16_LOAD_SPLIT(FIRST, INPUT, STRIDE, TWIDDLE)                                                  \
  for (int q = FIRST; q < 8; q++) {                                                                        \
    const simde__m256i lo = simde_mm256_loadu_si256((const simde__m256i *)(INPUT + q * STRIDE + n));       \
    const simde__m256i hi = simde_mm256_loadu_si256((const simde__m256i *)(INPUT + (q + 8) * STRIDE + n)); \
    a[q] = simde_mm256_mulhrs_epi16(simde_mm256_adds_epi16(lo, hi), s2);                                   \
    b[q] = simde_mm256_mulhrs_epi16(simde_mm256_subs_epi16(lo, hi), s2);                                   \
    TWIDDLE()                                                                                              \
  }

#define DEFINE_DFT256_RADIX16(FN, CHILD)                                                                      \
  static void FN(const c16_t *src, c16_t *dst, dft_dir_t dir)                                                 \
  {                                                                                                           \
    const int ds = dir == DFT_DIR_FORWARD ? 0 : 1;                                                            \
    const simde__m256i s2 = simde_mm256_set1_epi16(Q15_INV_SQRT2);                                            \
    simde__m256i child[2][16] __attribute__((aligned(64)));                                                   \
                                                                                                              \
    for (int bidx = 0; bidx < 2; bidx++) {                                                                    \
      const int n = 8 * bidx;                                                                                 \
      simde__m256i a[8], b[8], e[8], o[8], r[16];                                                             \
                                                                                                              \
      const simde__m256i lo0 = simde_mm256_loadu_si256((const simde__m256i *)(src + n));                      \
      const simde__m256i hi0 = simde_mm256_loadu_si256((const simde__m256i *)(src + 128 + n));                \
      a[0] = simde_mm256_mulhrs_epi16(simde_mm256_adds_epi16(lo0, hi0), s2);                                  \
      b[0] = simde_mm256_mulhrs_epi16(simde_mm256_subs_epi16(lo0, hi0), s2);                                  \
                                                                                                              \
      DFT_R16_LOAD_SPLIT(1, src, 16, DFT_R16_W16_ALWAYS)                                                      \
                                                                                                              \
      DFT_R16_SPLIT_CHILDREN()                                                                                \
                                                                                                              \
      r[0] = e[0];                                                                                            \
      r[1] = complex_mul8_prepack_q15_256(o[0], dft256_re[1][bidx], dft256_im[ds][1][bidx]);                  \
      for (int k = 1; k < 8; k++) {                                                                           \
        const int re = 2 * k;                                                                                 \
        const int ro = re + 1;                                                                                \
        r[re] = (re == 0) ? simde_mm256_mulhrs_epi16(e[k], dft256_re[re][bidx]) /* real twiddle */            \
                          : complex_mul8_prepack_q15_256(e[k], dft256_re[re][bidx], dft256_im[ds][re][bidx]); \
        r[ro] = complex_mul8_prepack_q15_256(o[k], dft256_re[ro][bidx], dft256_im[ds][ro][bidx]);             \
      }                                                                                                       \
                                                                                                              \
      transpose8_complex_i16_256(&r[0], &r[1], &r[2], &r[3], &r[4], &r[5], &r[6], &r[7]);                     \
      transpose8_complex_i16_256(&r[8], &r[9], &r[10], &r[11], &r[12], &r[13], &r[14], &r[15]);               \
      for (int lane = 0; lane < 8; lane++) {                                                                  \
        child[0][n + lane] = r[lane];                                                                         \
        child[1][n + lane] = r[8 + lane];                                                                     \
      }                                                                                                       \
    }                                                                                                         \
                                                                                                              \
    for (int group = 0; group < 2; group++) {                                                                 \
      simde__m256i y[16];                                                                                     \
      CHILD(child[group], y, dir);                                                                            \
      for (int k = 0; k < 16; k++) {                                                                          \
        simde_mm256_storeu_si256((simde__m256i *)(dst + 16 * k + 8 * group), y[k]);                           \
      }                                                                                                       \
    }                                                                                                         \
  }

DEFINE_DFT256_RADIX16(dft256_radix16, dft16x8)

DEFINE_DFT256_RADIX16(dft256_radix16_w16folded, dft16x8_w16folded)

#undef DEFINE_DFT256_RADIX16

/* DFT512 up to the DFT64x8 leaf: leaf[] holds the eight DFT64 inputs (one per lane). */
static __attribute__((always_inline)) inline void dft512_radix8_leaf_inputs(const c16_t *src,
                                                                            const dft_band_t *pin,
                                                                            simde__m256i leaf[64],
                                                                            dft_dir_t dir)
{
  const int ds = dir == DFT_DIR_FORWARD ? 0 : 1;
  c16_t stage[512] __attribute__((aligned(64)));

  for (int b = 0; b < 8; b++) {
    const int n = 8 * b;
    simde__m256i x[8], h[8];
    for (int r = 0; r < 8; r++)
      x[r] = pin ? dft_band_load8(pin, 512, (r ^ 4) * 64 + n, r < 4)
                 : simde_mm256_loadu_si256((const simde__m256i *)(src + r * 64 + n));
    {
      const simde__m256i xin_[8] = {x[0], x[1], x[2], x[3], x[4], x[5], x[6], x[7]};
      dft8x8_q15_256_half_dir(xin_, h, dir);
    }
    for (int r = 0; r < 8; r++) {
      h[r] = (r == 0) ? simde_mm256_mulhrs_epi16(h[r], dft512_re[r][b]) /* real twiddle */
                      : complex_mul8_prepack_q15_256(h[r], dft512_re[r][b], dft512_im[ds][r][b]);
      simde_mm256_store_si256((simde__m256i *)(stage + r * 64 + n), h[r]);
    }
  }

  for (int n = 0; n < 64; n += 8) {
    simde__m256i z[8];
    for (int r = 0; r < 8; r++)
      z[r] = simde_mm256_load_si256((const simde__m256i *)(stage + r * 64 + n));
    transpose8_complex_i16_256(&z[0], &z[1], &z[2], &z[3], &z[4], &z[5], &z[6], &z[7]);
    for (int lane = 0; lane < 8; lane++)
      leaf[n + lane] = z[lane];
  }
}

static void dft512_radix8(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  simde__m256i leaf[64] __attribute__((aligned(64)));
  dft512_radix8_leaf_inputs(src, NULL, leaf, dir);
  dft64x8_store(leaf, dst, 8, 0, dir);
}

/* DFT1024 up to the two DFT64x8 leaves (bins 16k + lane and 16k + 8 + lane). */
static __attribute__((always_inline)) inline void dft1024_radix16_leaf_inputs(const c16_t *src,
                                                                              const dft_band_t *pin,
                                                                              simde__m256i leaf[2][64],
                                                                              dft_dir_t dir)
{
  const int ds = dir == DFT_DIR_FORWARD ? 0 : 1;
  const simde__m256i s2 = simde_mm256_set1_epi16(Q15_INV_SQRT2);

  for (int bidx = 0; bidx < 8; bidx++) {
    const int n = 8 * bidx;
    simde__m256i a[8], b[8], e[8], o[8];

    /* DFT_R16_LOAD_SPLIT(0, src, 64, DFT_R16_W16_NONZERO) with band-aware loads */
    for (int q = 0; q < 8; q++) {
      /* band: bins q * 64 + n are in the upper swapped half, (q + 8) * 64 + n in the lower one */
      const simde__m256i lo =
          pin ? dft_band_load8(pin, 1024, (q + 8) * 64 + n, 1) : simde_mm256_loadu_si256((const simde__m256i *)(src + q * 64 + n));
      const simde__m256i hi =
          pin ? dft_band_load8(pin, 1024, q * 64 + n, 0) : simde_mm256_loadu_si256((const simde__m256i *)(src + (q + 8) * 64 + n));
      a[q] = simde_mm256_mulhrs_epi16(simde_mm256_adds_epi16(lo, hi), s2);
      b[q] = simde_mm256_mulhrs_epi16(simde_mm256_subs_epi16(lo, hi), s2);
      DFT_R16_W16_NONZERO()
    }

    DFT_R16_SPLIT_CHILDREN()

    simde__m256i z0 = simde_mm256_mulhrs_epi16(e[0], dft1024_re[0][bidx]); /* row 0: real twiddle */
    simde__m256i z1 = complex_mul8_prepack_q15_256(o[0], dft1024_re[1][bidx], dft1024_im[ds][1][bidx]);
    simde__m256i z2 = complex_mul8_prepack_q15_256(e[1], dft1024_re[2][bidx], dft1024_im[ds][2][bidx]);
    simde__m256i z3 = complex_mul8_prepack_q15_256(o[1], dft1024_re[3][bidx], dft1024_im[ds][3][bidx]);
    simde__m256i z4 = complex_mul8_prepack_q15_256(e[2], dft1024_re[4][bidx], dft1024_im[ds][4][bidx]);
    simde__m256i z5 = complex_mul8_prepack_q15_256(o[2], dft1024_re[5][bidx], dft1024_im[ds][5][bidx]);
    simde__m256i z6 = complex_mul8_prepack_q15_256(e[3], dft1024_re[6][bidx], dft1024_im[ds][6][bidx]);
    simde__m256i z7 = complex_mul8_prepack_q15_256(o[3], dft1024_re[7][bidx], dft1024_im[ds][7][bidx]);
    DFT_TRANSPOSE8_TO_ROW(z, leaf[0], n)

    z0 = complex_mul8_prepack_q15_256(e[4], dft1024_re[8][bidx], dft1024_im[ds][8][bidx]);
    z1 = complex_mul8_prepack_q15_256(o[4], dft1024_re[9][bidx], dft1024_im[ds][9][bidx]);
    z2 = complex_mul8_prepack_q15_256(e[5], dft1024_re[10][bidx], dft1024_im[ds][10][bidx]);
    z3 = complex_mul8_prepack_q15_256(o[5], dft1024_re[11][bidx], dft1024_im[ds][11][bidx]);
    z4 = complex_mul8_prepack_q15_256(e[6], dft1024_re[12][bidx], dft1024_im[ds][12][bidx]);
    z5 = complex_mul8_prepack_q15_256(o[6], dft1024_re[13][bidx], dft1024_im[ds][13][bidx]);
    z6 = complex_mul8_prepack_q15_256(e[7], dft1024_re[14][bidx], dft1024_im[ds][14][bidx]);
    z7 = complex_mul8_prepack_q15_256(o[7], dft1024_re[15][bidx], dft1024_im[ds][15][bidx]);
    DFT_TRANSPOSE8_TO_ROW(z, leaf[1], n)
  }
}

static void dft1024_radix16(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  simde__m256i leaf[2][64] __attribute__((aligned(64)));
  dft1024_radix16_leaf_inputs(src, NULL, leaf, dir);
  dft64x8_store(leaf[0], dst, 16, 0, dir);
  dft64x8_store(leaf[1], dst, 16, 8, dir);
}

static __attribute__((always_inline)) inline void dft_power2_radix8_stage_large_body(const c16_t *src,
                                                                                     const dft_band_t *pin,
                                                                                     c16_t *stage,
                                                                                     int N,
                                                                                     dft_dir_t dir)
{
  const power2_twiddle_t *tw = power2_twiddle_slot(N);
  AssertFatal(tw, "No radix-8 twiddles for N=%d\n", N);

  const int M = N / 8;
  const int blocks = tw->q15_blocks;
  const simde__m256i *re = tw->q15_re; /* the real parts are the same in both directions */
  const simde__m256i *im = dir == DFT_DIR_FORWARD ? tw->q15_im : tw->q15_im_inv;

  for (int b = 0; b < blocks; b++) {
    const int n = 8 * b;
    simde__m256i x[8], y[8];
    for (int r = 0; r < 8; r++) {
      if (!pin) {
        x[r] = simde_mm256_loadu_si256((const simde__m256i *)(src + r * M + n));
        continue;
      }
      /* band: natural bin r * M + n is at swapped index (r ^ 4) * M + n, in the upper half for r < 4 */
      const int R = (r ^ 4) * M + n;
      x[r] = dft_band_load8(pin, N, R, r < 4);
    }

    {
      const simde__m256i xin_[8] = {x[0], x[1], x[2], x[3], x[4], x[5], x[6], x[7]};
      dft8x8_q15_256_half_dir(xin_, y, dir);
    }

    for (int r = 0; r < 8; r++) {
      const int idx = b * 8 + r;
      y[r] = (r == 0) ? simde_mm256_mulhrs_epi16(y[r], re[idx]) /* real twiddle */
                      : complex_mul8_prepack_q15_256(y[r], re[idx], im[idx]);
      simde_mm256_store_si256((simde__m256i *)(stage + r * M + n), y[r]);
    }
  }
}

static void dft_power2_radix8_stage_large(const c16_t *src, c16_t *stage, int N, dft_dir_t dir)
{
  dft_power2_radix8_stage_large_body(src, NULL, stage, N, dir);
}

/* Pruned API, IDFT input in the NR layout (band passed as scalars, see dft_band_make). */
static __attribute__((noinline)) void dft_power2_radix8_stage_large_band(const c16_t *in,
                                                                         int d,
                                                                         c16_t *edge_buf,
                                                                         c16_t *stage,
                                                                         int N,
                                                                         dft_dir_t dir)
{
  const dft_band_t band = dft_band_make(in, NULL, d, edge_buf);
  const dft_band_t *pin = &band;
  if (dir == DFT_DIR_FORWARD)
    dft_power2_radix8_stage_large_body(NULL, pin, stage, N, DFT_DIR_FORWARD);
  else
    dft_power2_radix8_stage_large_body(NULL, pin, stage, N, DFT_DIR_INVERSE);
}

#define DFT_TRANSPOSE_CHILD_COLUMN(P, SRC, COL, DST, POS)                                      \
  simde__m256i P##0 = SRC[0][COL], P##1 = SRC[1][COL], P##2 = SRC[2][COL], P##3 = SRC[3][COL]; \
  simde__m256i P##4 = SRC[4][COL], P##5 = SRC[5][COL], P##6 = SRC[6][COL], P##7 = SRC[7][COL]; \
  DFT_TRANSPOSE8_TO_ROW(P, DST, POS)

#define DFT_FUSED_R8_PARENT_WORKSPACE(NVAL, CHILDVAL, MVAL, ARRAY, ROWS, COLS) \
  enum { N = NVAL, CHILD_N = CHILDVAL, M = MVAL };                             \
  c16_t outer_stage[N] __attribute__((aligned(64)));                           \
  simde__m256i ARRAY[ROWS][COLS] __attribute__((aligned(64)));                 \
                                                                               \
  dft_power2_radix8_stage_large(src, outer_stage, N, dir);

/* DFT2048 up to the sixteen DFT16x8 leaves: child_in[r16] gives bins 128k + 8 r16 + lane. */
static __attribute__((always_inline)) inline void dft2048_radix8x16_leaf_inputs(const c16_t *src,
                                                                                const dft_band_t *pin,
                                                                                simde__m256i child_in[16][16],
                                                                                dft_dir_t dir)
{
  enum { N = 2048, CHILD_N = 256, M = 16 };
  c16_t outer_stage[N] __attribute__((aligned(64)));
  if (pin)
    dft_power2_radix8_stage_large_band(pin->in, pin->d, pin->edge_buf, outer_stage, N, dir);
  else
    dft_power2_radix8_stage_large(src, outer_stage, N, dir);
  const int ds = dir == DFT_DIR_FORWARD ? 0 : 1;
  const simde__m256i s2 = simde_mm256_set1_epi16(Q15_INV_SQRT2);

  for (int bidx = 0; bidx < 2; bidx++) {
    const int n = 8 * bidx;
    simde__m256i R[8][16] __attribute__((aligned(64)));

    for (int outer_r = 0; outer_r < 8; outer_r++) {
      const c16_t *child_src = outer_stage + outer_r * CHILD_N;
      simde__m256i a[8], b[8], e[8], o[8];

      a[0] = simde_mm256_mulhrs_epi16(simde_mm256_adds_epi16(simde_mm256_loadu_si256((const simde__m256i *)(child_src + n)),
                                                             simde_mm256_loadu_si256((const simde__m256i *)(child_src + 128 + n))),
                                      s2);
      b[0] = simde_mm256_mulhrs_epi16(simde_mm256_subs_epi16(simde_mm256_loadu_si256((const simde__m256i *)(child_src + n)),
                                                             simde_mm256_loadu_si256((const simde__m256i *)(child_src + 128 + n))),
                                      s2);

      DFT_R16_LOAD_SPLIT(1, child_src, M, DFT_R16_W16_ALWAYS)

      DFT_R16_SPLIT_CHILDREN()

      for (int k = 0; k < 8; k++) {
        const int re = 2 * k;
        const int ro = re + 1;
        R[outer_r][re] = (re == 0) ? simde_mm256_mulhrs_epi16(e[k], dft256_re[re][bidx]) /* real twiddle */
                                   : complex_mul8_prepack_q15_256(e[k], dft256_re[re][bidx], dft256_im[ds][re][bidx]);
        R[outer_r][ro] = complex_mul8_prepack_q15_256(o[k], dft256_re[ro][bidx], dft256_im[ds][ro][bidx]);
      }
    }

    for (int r16 = 0; r16 < 16; r16++) {
      DFT_TRANSPOSE_CHILD_COLUMN(z, R, r16, child_in[r16], n)
    }
  }
}

static void dft2048_radix8x16_fused_w16folded(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  simde__m256i child_in[16][16] __attribute__((aligned(64)));
  dft2048_radix8x16_leaf_inputs(src, NULL, child_in, dir);
  for (int r16 = 0; r16 < 16; r16++) {
    simde__m256i y[16];
    dft16x8_w16folded(child_in[r16], y, dir);
    for (int k = 0; k < 16; k++) {
      simde_mm256_storeu_si256((simde__m256i *)(dst + 128 * k + 8 * r16), y[k]);
    }
  }
}

/* DFT4096 up to the eight DFT64x8 leaves: leaf_in[r] gives bins 64k + 8r + lane. */
static __attribute__((always_inline)) inline void dft4096_radix8x8_leaf_inputs(const c16_t *src,
                                                                               const dft_band_t *pin,
                                                                               simde__m256i leaf_in[8][64],
                                                                               dft_dir_t dir)
{
  enum { N = 4096, CHILD_N = 512, M = 64 };
  c16_t outer_stage[N] __attribute__((aligned(64)));
  if (pin)
    dft_power2_radix8_stage_large_band(pin->in, pin->d, pin->edge_buf, outer_stage, N, dir);
  else
    dft_power2_radix8_stage_large(src, outer_stage, N, dir);
  const int ds = dir == DFT_DIR_FORWARD ? 0 : 1;

  for (int bidx = 0; bidx < 8; bidx++) {
    const int n = 8 * bidx;
    simde__m256i R[8][8] __attribute__((aligned(64)));

    for (int outer_r = 0; outer_r < 8; outer_r++) {
      const c16_t *child_src = outer_stage + outer_r * CHILD_N;
      simde__m256i x[8], h[8];
      for (int r = 0; r < 8; r++)
        x[r] = simde_mm256_loadu_si256((const simde__m256i *)(child_src + r * M + n));

      {
        const simde__m256i xin_[8] = {x[0], x[1], x[2], x[3], x[4], x[5], x[6], x[7]};
        dft8x8_q15_256_half_dir(xin_, h, dir);
      }

      for (int r = 0; r < 8; r++)
        R[outer_r][r] = (r == 0) ? simde_mm256_mulhrs_epi16(h[r], dft512_re[r][bidx]) /* real twiddle */
                                 : complex_mul8_prepack_q15_256(h[r], dft512_re[r][bidx], dft512_im[ds][r][bidx]);
    }

    for (int r = 0; r < 8; r++) {
      DFT_TRANSPOSE_CHILD_COLUMN(z, R, r, leaf_in[r], n)
    }
  }
}

static void dft4096_radix8x8_fused(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  simde__m256i leaf_in[8][64] __attribute__((aligned(64)));
  dft4096_radix8x8_leaf_inputs(src, NULL, leaf_in, dir);
  for (int r = 0; r < 8; r++)
    dft64x8_store(leaf_in[r], dst, 64, 8 * r, dir);
}

static void dft8192_radix8x16_fused(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  DFT_FUSED_R8_PARENT_WORKSPACE(8192, 1024, 64, leaf_in, 16, 64) const int ds = dir == DFT_DIR_FORWARD ? 0 : 1;
  const simde__m256i s2 = simde_mm256_set1_epi16(Q15_INV_SQRT2);

  for (int bidx = 0; bidx < 8; bidx++) {
    const int n = 8 * bidx;
    simde__m256i R[8][16] __attribute__((aligned(64)));

    for (int outer_r = 0; outer_r < 8; outer_r++) {
      const c16_t *child_src = outer_stage + outer_r * CHILD_N;
      simde__m256i a[8], b[8], e[8], o[8];

      DFT_R16_LOAD_SPLIT(0, child_src, M, DFT_R16_W16_NONZERO)

      DFT_R16_SPLIT_CHILDREN()

      for (int k = 0; k < 8; k++) {
        const int re = 2 * k;
        const int ro = re + 1;
        R[outer_r][re] = (re == 0) ? simde_mm256_mulhrs_epi16(e[k], dft1024_re[re][bidx]) /* real twiddle */
                                   : complex_mul8_prepack_q15_256(e[k], dft1024_re[re][bidx], dft1024_im[ds][re][bidx]);
        R[outer_r][ro] = complex_mul8_prepack_q15_256(o[k], dft1024_re[ro][bidx], dft1024_im[ds][ro][bidx]);
      }
    }

    for (int r = 0; r < 16; r++) {
      DFT_TRANSPOSE_CHILD_COLUMN(z, R, r, leaf_in[r], n)
    }
  }

  for (int r = 0; r < 16; r++)
    dft64x8_store(leaf_in[r], dst, 128, 8 * r, dir);
}

static inline void radix8_blocked_to_natural_q15_large(const c16_t *blocked, c16_t *dst, int M)
{
  for (int q = 0; q < M; q += 8) {
    simde__m256i y[8];
    for (int r = 0; r < 8; r++)
      y[r] = simde_mm256_load_si256((const simde__m256i *)(blocked + r * M + q));
    transpose8_complex_i16_256(&y[0], &y[1], &y[2], &y[3], &y[4], &y[5], &y[6], &y[7]);
    for (int lane = 0; lane < 8; lane++)
      simde_mm256_storeu_si256((simde__m256i *)(dst + 8 * (q + lane)), y[lane]);
  }
}

static void dft16384_split_radix(const c16_t *src, c16_t *dst, c16_t *work, dft_dir_t dir)
{
  const int N = 16384;
  const int half = N >> 1;
  const int quarter = N >> 2;
  c16_t *sub_in = work;
  c16_t *sub_out = work + N;
  c16_t *E = sub_out;
  c16_t *O1 = sub_out + half;
  c16_t *O3 = sub_out + half + quarter;

  pack_split_radix_input_avx2_fused(src, sub_in, N);
  dft8192_radix8x16_fused(sub_in, E, dir);
  dft4096_radix8x8_fused(sub_in + half, O1, dir);
  dft4096_radix8x8_fused(sub_in + half + quarter, O3, dir);

  const sr_twiddle_simd_t *table = sr_twiddle_table_get(N, dir);
  sr_combine_simd(E, O1, O3, dst, N, table, dir);
}

static size_t dft_power2_mixed_large_work_len(int N)
{
  size_t need = 0;
  /* Dedicated kernels through N=8192 use their own stack storage. */
  while (N > 8192 && N <= DFT_MIXED_POWER2_MAX_N) {
    need += 2u * (size_t)N;
    N /= 8;
  }
  return need;
}

static void dft_power2_mixed_large_core(const c16_t *src, c16_t *dst, int N, dft_dir_t dir, c16_t *work)
{
  if (N == 256) {
    dft256_radix16_w16folded(src, dst, dir);
    return;
  }
  if (N == 512) {
    dft512_radix8(src, dst, dir);
    return;
  }
  if (N == 1024) {
    dft1024_radix16(src, dst, dir);
    return;
  }
  if (N == 2048) {
    dft2048_radix8x16_fused_w16folded(src, dst, dir);
    return;
  }
  if (N == 4096) {
    dft4096_radix8x8_fused(src, dst, dir);
    return;
  }
  if (N == 8192) {
    dft8192_radix8x16_fused(src, dst, dir);
    return;
  }
  if (N == 16384) {
    dft16384_split_radix(src, dst, work, dir);
    return;
  }
  AssertFatal(N <= DFT_MIXED_POWER2_MAX_N, "No power-of-two DFT for N=%d\n", N);

  const int M = N / 8;
  c16_t *stage = work;
  c16_t *blocked = work + N;
  c16_t *child_work = work + 2 * N;

  dft_power2_radix8_stage_large(src, stage, N, dir);

  for (int r = 0; r < 8; r++)
    dft_power2_mixed_large_core(stage + r * M, blocked + r * M, M, dir, child_work);

  radix8_blocked_to_natural_q15_large(blocked, dst, M);
}

/* Large power-of-two transforms: stage workspace on the stack (up to 512 KB, N = 65536). */
static __attribute__((noinline)) void dft_power2_mixed_large_q15(const c16_t *src, c16_t *dst, int N, dft_dir_t dir)
{
  AssertFatal(N <= DFT_MIXED_POWER2_MAX_N, "No power-of-two DFT for N=%d\n", N);

  const size_t need = dft_power2_mixed_large_work_len(N);
  if (!need) {
    dft_power2_mixed_large_core(src, dst, N, dir, NULL);
    return;
  }

  c16_t work[need] __attribute__((aligned(64)));
  dft_power2_mixed_large_core(src, dst, N, dir, work);
}

static inline void pack_radix3(const c16_t *src, c16_t *packed, int size)
{
  int n = 0;
  for (; n + 3 < size; n += 4) {
    const simde__m128 l0 = simde_mm_castsi128_ps(simde_mm_loadu_si128((const simde__m128i *)(src + 3 * n)));
    const simde__m128 l1 = simde_mm_castsi128_ps(simde_mm_loadu_si128((const simde__m128i *)(src + 3 * n + 4)));
    const simde__m128 l2 = simde_mm_castsi128_ps(simde_mm_loadu_si128((const simde__m128i *)(src + 3 * n + 8)));

    const simde__m128 a01 = simde_mm_shuffle_ps(l0, l0, _MM_SHUFFLE(3, 0, 3, 0));
    const simde__m128 a23 = simde_mm_shuffle_ps(l1, l2, _MM_SHUFFLE(1, 1, 2, 2));
    const simde__m128 b01 = simde_mm_shuffle_ps(l0, l1, _MM_SHUFFLE(0, 0, 1, 1));
    const simde__m128 b23 = simde_mm_shuffle_ps(l1, l2, _MM_SHUFFLE(2, 2, 3, 3));
    const simde__m128 c01 = simde_mm_shuffle_ps(l0, l1, _MM_SHUFFLE(1, 1, 2, 2));
    const simde__m128 c23 = simde_mm_shuffle_ps(l2, l2, _MM_SHUFFLE(3, 3, 0, 0));

    simde_mm_storeu_si128((simde__m128i *)(packed + n),
                          simde_mm_castps_si128(simde_mm_shuffle_ps(a01, a23, _MM_SHUFFLE(2, 0, 1, 0))));
    simde_mm_storeu_si128((simde__m128i *)(packed + size + n),
                          simde_mm_castps_si128(simde_mm_shuffle_ps(b01, b23, _MM_SHUFFLE(2, 0, 2, 0))));
    simde_mm_storeu_si128((simde__m128i *)(packed + 2 * size + n),
                          simde_mm_castps_si128(simde_mm_shuffle_ps(c01, c23, _MM_SHUFFLE(2, 0, 2, 0))));
  }
  for (; n < size; n++) {
    packed[n] = src[3 * n];
    packed[size + n] = src[3 * n + 1];
    packed[2 * size + n] = src[3 * n + 2];
  }
}

static __attribute__((always_inline)) inline void pack_radix3_permd8_1440(const c16_t *src, c16_t *packed)
{
  enum { M = 480 };

  const simde__m256i a0_idx = simde_mm256_setr_epi32(0, 3, 6, 0, 0, 0, 0, 0);
  const simde__m256i a1_idx = simde_mm256_setr_epi32(0, 0, 0, 1, 4, 7, 0, 0);
  const simde__m256i a2_idx = simde_mm256_setr_epi32(0, 0, 0, 0, 0, 0, 2, 5);

  const simde__m256i b0_idx = simde_mm256_setr_epi32(1, 4, 7, 0, 0, 0, 0, 0);
  const simde__m256i b1_idx = simde_mm256_setr_epi32(0, 0, 0, 2, 5, 0, 0, 0);
  const simde__m256i b2_idx = simde_mm256_setr_epi32(0, 0, 0, 0, 0, 0, 3, 6);

  const simde__m256i c0_idx = simde_mm256_setr_epi32(2, 5, 0, 0, 0, 0, 0, 0);
  const simde__m256i c1_idx = simde_mm256_setr_epi32(0, 0, 0, 3, 6, 0, 0, 0);
  const simde__m256i c2_idx = simde_mm256_setr_epi32(0, 0, 0, 0, 0, 1, 4, 7);

  for (int n = 0; n < M; n += 8) {
    const simde__m256i x0 = simde_mm256_loadu_si256((const simde__m256i *)(src + 3 * n + 0));
    const simde__m256i x1 = simde_mm256_loadu_si256((const simde__m256i *)(src + 3 * n + 8));
    const simde__m256i x2 = simde_mm256_loadu_si256((const simde__m256i *)(src + 3 * n + 16));

    const simde__m256i a0 = simde_mm256_permutevar8x32_epi32(x0, a0_idx);
    const simde__m256i a1 = simde_mm256_permutevar8x32_epi32(x1, a1_idx);
    const simde__m256i a2 = simde_mm256_permutevar8x32_epi32(x2, a2_idx);
    simde__m256i a = simde_mm256_blend_epi32(a0, a1, 0x38);
    a = simde_mm256_blend_epi32(a, a2, 0xc0);

    const simde__m256i b0 = simde_mm256_permutevar8x32_epi32(x0, b0_idx);
    const simde__m256i b1 = simde_mm256_permutevar8x32_epi32(x1, b1_idx);
    const simde__m256i b2 = simde_mm256_permutevar8x32_epi32(x2, b2_idx);
    simde__m256i b = simde_mm256_blend_epi32(b0, b1, 0x18);
    b = simde_mm256_blend_epi32(b, b2, 0xe0);

    const simde__m256i c0 = simde_mm256_permutevar8x32_epi32(x0, c0_idx);
    const simde__m256i c1 = simde_mm256_permutevar8x32_epi32(x1, c1_idx);
    const simde__m256i c2 = simde_mm256_permutevar8x32_epi32(x2, c2_idx);
    simde__m256i c = simde_mm256_blend_epi32(c0, c1, 0x1c);
    c = simde_mm256_blend_epi32(c, c2, 0xe0);

    simde_mm256_store_si256((simde__m256i *)(packed + 0 * M + n), a);
    simde_mm256_store_si256((simde__m256i *)(packed + 1 * M + n), b);
    simde_mm256_store_si256((simde__m256i *)(packed + 2 * M + n), c);
  }
}

static inline void radix3_combine4_q15_128_fast(simde__m128i A,
                                                simde__m128i X1,
                                                simde__m128i X2,
                                                simde__m128i w1_re_re,
                                                simde__m128i w1_im_im,
                                                simde__m128i w2_re_re,
                                                simde__m128i w2_im_im,
                                                simde__m128i *Y0,
                                                simde__m128i *Y1,
                                                simde__m128i *Y2,
                                                dft_dir_t dir)
{
  /* W1/W2 already include 1/sqrt(3); only A needs a separate scale. */
  const simde__m128i Bs = complex_mul4_prepack_small_q15_128(X1, w1_re_re, w1_im_im);
  const simde__m128i Cs = complex_mul4_prepack_small_q15_128(X2, w2_re_re, w2_im_im);

  /* The half-sum below is rounded without bias (half_q15_128). */
  const simde__m128i As = q15_mul_i16_128(A, Q15_INV_SQRT3);
  const simde__m128i S = simde_mm_adds_epi16(Bs, Cs);
  const simde__m128i D = simde_mm_subs_epi16(Bs, Cs);
  *Y0 = simde_mm_adds_epi16(As, S);

  /* Bs/Cs are already normalized. */
  const simde__m128i Sh = half_q15_128(S);

  const simde__m128i base = simde_mm_subs_epi16(As, Sh);

  /* Apply sqrt(3)/2 to the already scaled branch difference. */

  const simde__m128i Z = q15_mul_i16_128(D, Q15_HALF_SQRT3);

  /* Rotate Z according to the transform direction. */
  *Y1 = simde_mm_adds_epi16(base, (dir == DFT_DIR_FORWARD) ? mul_minus_q15_128(Z) : mul_q15_128(Z));
  *Y2 = simde_mm_adds_epi16(base, (dir == DFT_DIR_FORWARD) ? mul_q15_128(Z) : mul_minus_q15_128(Z));
}

static inline void radix3_combine8_q15_256_fast(simde__m256i A,
                                                simde__m256i X1,
                                                simde__m256i X2,
                                                simde__m256i w1_re_re,
                                                simde__m256i w1_im_im,
                                                simde__m256i w2_re_re,
                                                simde__m256i w2_im_im,
                                                simde__m256i *Y0,
                                                simde__m256i *Y1,
                                                simde__m256i *Y2,
                                                dft_dir_t dir)
{
  const simde__m256i Bs = complex_mul8_prepack_small_q15_256(X1, w1_re_re, w1_im_im);
  const simde__m256i Cs = complex_mul8_prepack_small_q15_256(X2, w2_re_re, w2_im_im);
  const simde__m256i As = simde_mm256_mulhrs_epi16(A, simde_mm256_set1_epi16(Q15_INV_SQRT3));
  const simde__m256i S = simde_mm256_adds_epi16(Bs, Cs);
  const simde__m256i D = simde_mm256_subs_epi16(Bs, Cs);
  const simde__m256i base = simde_mm256_subs_epi16(As, half_q15_256(S));
  const simde__m256i Z_mj = mul_minus_j_scaled_dir_i16_256(D, Q15_HALF_SQRT3, dir);

  *Y0 = simde_mm256_adds_epi16(As, S);
  *Y1 = simde_mm256_adds_epi16(base, Z_mj);
  *Y2 = simde_mm256_subs_epi16(base, Z_mj);
}

/* Table aliases exist only in the calling function. */
#define DFT_BIND_R3_TWIDDLES(TABLE, DIR, R1, I1, R2, I2)                                   \
  const simde__m128i *R1 = R3_W1_RE(TABLE);                                                \
  const simde__m128i *I1 = DIR == DFT_DIR_FORWARD ? R3_W1_IM(TABLE) : R3_W1_IM_INV(TABLE); \
  const simde__m128i *R2 = R3_W2_RE(TABLE);                                                \
  const simde__m128i *I2 = DIR == DFT_DIR_FORWARD ? R3_W2_IM(TABLE) : R3_W2_IM_INV(TABLE)

/* Unit-magnitude radix-3 twiddles of the same table (N <= R3_UNIT_MAX_N). */
#define DFT_BIND_R3_UNIT_TWIDDLES(TABLE, DIR, R1, I1, R2, I2)                                      \
  const simde__m128i *R1 = R3_UNIT_PTR(TABLE, 0);                                                  \
  const simde__m128i *I1 = DIR == DFT_DIR_FORWARD ? R3_UNIT_PTR(TABLE, 2) : R3_UNIT_PTR(TABLE, 4); \
  const simde__m128i *R2 = R3_UNIT_PTR(TABLE, 1);                                                  \
  const simde__m128i *I2 = DIR == DFT_DIR_FORWARD ? R3_UNIT_PTR(TABLE, 3) : R3_UNIT_PTR(TABLE, 5)

#define DFT_R3_COEFFICIENTS_128(P, INDEX) P##1r [INDEX], P##1i [INDEX], P##2r [INDEX], P##2i [INDEX]

#define DFT_R3_COEFFICIENTS_256(P, INDEX)                                                                                         \
  simde_mm256_loadu_si256((const simde__m256i *)(P##1r + INDEX)), simde_mm256_loadu_si256((const simde__m256i *)(P##1i + INDEX)), \
      simde_mm256_loadu_si256((const simde__m256i *)(P##2r + INDEX)),                                                             \
      simde_mm256_loadu_si256((const simde__m256i *)(P##2i + INDEX))

static __attribute__((always_inline)) inline void radix3_combine_contiguous_avx2_body(const c16_t *src,
                                                                                      c16_t *dst,
                                                                                      int M,
                                                                                      const r3_twiddle_t *tw,
                                                                                      dft_dir_t dir)
{
  AssertFatal((M & 3) == 0, "Radix-3 combine requires M multiple of 4, got %d\n", M);

  DFT_BIND_R3_TWIDDLES(tw, dir, w1r, w1i, w2r, w2i);

  int k = 0;
  for (; k + 8 <= M; k += 8) {
    const int b = k >> 2;
    simde__m256i y0, y1, y2;
    radix3_combine8_q15_256_fast(simde_mm256_loadu_si256((const simde__m256i *)(src + 0 * M + k)),
                                 simde_mm256_loadu_si256((const simde__m256i *)(src + 1 * M + k)),
                                 simde_mm256_loadu_si256((const simde__m256i *)(src + 2 * M + k)),
                                 DFT_R3_COEFFICIENTS_256(w, b),
                                 &y0,
                                 &y1,
                                 &y2,
                                 dir);
    simde_mm256_storeu_si256((simde__m256i *)(dst + 0 * M + k), y0);
    simde_mm256_storeu_si256((simde__m256i *)(dst + 1 * M + k), y1);
    simde_mm256_storeu_si256((simde__m256i *)(dst + 2 * M + k), y2);
  }

  if (k < M) {
    AssertFatal(M - k == 4, "Unexpected radix-3 AVX2 tail M=%d k=%d\n", M, k);
    const int b = k >> 2;
    simde__m128i y0, y1, y2;
    radix3_combine4_q15_128_fast(simde_mm_loadu_si128((const simde__m128i *)(src + 0 * M + k)),
                                 simde_mm_loadu_si128((const simde__m128i *)(src + 1 * M + k)),
                                 simde_mm_loadu_si128((const simde__m128i *)(src + 2 * M + k)),
                                 DFT_R3_COEFFICIENTS_128(w, b),
                                 &y0,
                                 &y1,
                                 &y2,
                                 dir);
    simde_mm_storeu_si128((simde__m128i *)(dst + 0 * M + k), y0);
    simde_mm_storeu_si128((simde__m128i *)(dst + 1 * M + k), y1);
    simde_mm_storeu_si128((simde__m128i *)(dst + 2 * M + k), y2);
  }
}

/* One copy per direction, so the direction is a constant inside. */
static __attribute__((noinline)) void radix3_combine_contiguous_avx2(const c16_t *src,
                                                                     c16_t *dst,
                                                                     int M,
                                                                     const r3_twiddle_t *tw,
                                                                     dft_dir_t dir)
{
  if (dir == DFT_DIR_FORWARD)
    radix3_combine_contiguous_avx2_body(src, dst, M, tw, DFT_DIR_FORWARD);
  else
    radix3_combine_contiguous_avx2_body(src, dst, M, tw, DFT_DIR_INVERSE);
}

/* Raw radix-3 butterfly used by the fused radix-9 DIF front end.
 * Scaling and parent twiddles are applied by the caller after the butterfly. */
#define DEFINE_RAW_RADIX3_BUTTERFLY(FN, VEC, ADD, SUB, HALF_SUM, IMAGINARY, PMJ)          \
  static inline void FN(VEC x0, VEC x1, VEC x2, VEC *y0, VEC *y1, VEC *y2, dft_dir_t dir) \
  {                                                                                       \
    const VEC sum = ADD(x1, x2);                                                          \
    const VEC diff = SUB(x1, x2);                                                         \
    const VEC base = SUB(x0, HALF_SUM);                                                   \
    const VEC imag_mj = IMAGINARY; /* (-j)^dir sqrt(3)/2 diff */                          \
    *y0 = ADD(x0, sum);                                                                   \
    *y1 = ADD(base, imag_mj);                                                             \
    *y2 = SUB(base, imag_mj);                                                             \
  }

DEFINE_RAW_RADIX3_BUTTERFLY(radix3_butterfly4_q15_128,
                            simde__m128i,
                            simde_mm_adds_epi16,
                            simde_mm_subs_epi16,
                            q15_mul_i16_128(sum, Q15_HALF),
                            mul_minus_j_scaled_dir_i16_128(diff, Q15_HALF_SQRT3, dir),
                            unused)

/* Radix-3 butterfly of x / sqrt(3) on unscaled inputs: the pair sum and difference grow the rms by
 * sqrt(2) only; 1/sqrt(3) is applied to x0 and the sum, and folded into the 1/2 and sqrt(3)/2 factors.
 * Four multiplies instead of five, one rounding fewer per path. */
static inline void radix3_butterfly4_unitary_q15_128(simde__m128i x0,
                                                     simde__m128i x1,
                                                     simde__m128i x2,
                                                     simde__m128i *y0,
                                                     simde__m128i *y1,
                                                     simde__m128i *y2,
                                                     dft_dir_t dir)
{
  const simde__m128i sum = simde_mm_adds_epi16(x1, x2);
  const simde__m128i diff = simde_mm_subs_epi16(x1, x2);
  const simde__m128i x0s = simde_mm_mulhrs_epi16(x0, simde_mm_set1_epi16(Q15_INV_SQRT3));
  const simde__m128i base = simde_mm_subs_epi16(x0s, simde_mm_mulhrs_epi16(sum, simde_mm_set1_epi16(Q15_INV_2SQRT3)));
  /* not folded into the multiply: by 1/2 every odd input is a rounding tie (-0.5..1 dB) */
  const simde__m128i imag = half_q15_128(diff);
  *y0 = simde_mm_adds_epi16(x0s, simde_mm_mulhrs_epi16(sum, simde_mm_set1_epi16(Q15_INV_SQRT3)));
  add_pm_j_i16_128(base, imag, y1, y2, dir);
}
