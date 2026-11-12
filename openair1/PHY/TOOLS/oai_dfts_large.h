/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* Internal part of oai_dfts.c: radix-24 parents (3072..12288) and packed radix-3/radix-5 parents (972..36864).
 * Included only by oai_dfts.c, in order; not a public header. */

#ifndef OAI_DFTS_INTERNAL
#error "oai_dfts_large.h is included only by oai_dfts.c"
#endif

/* Large R24 transforms.
 *
 * 3072  = R24 x DFT128x8
 * 6144  = R24 x DFT256x8
 * 12288 = R24 x DFT512x8
 *
 * The R24 parent runs a unitary DFT8 and a raw radix-3; the parent W_N
 * coefficients carry 1/sqrt(3). Each child transform carries its own
 * normalization. No whole-N final scatter buffer is used. */

/* DFT3072 keeps only the six final DFT64x8 inputs. The R24 results at child
 * indices k and k+64 are paired in registers and immediately passed through
 * the DFT128 R2; no R24 parent result is materialized in the working sets. */
static simde__m256i dft3072_r24_re[16][23] __attribute__((aligned(64)));
static simde__m256i dft3072_r24_im[16][23] __attribute__((aligned(64))); /* forward; inverse negates */
static uint32_t dft3072_r2_re[64] __attribute__((aligned(64)));
static uint32_t dft3072_r2_im[2][64] __attribute__((aligned(64)));

DEFINE_Q15_TABLE_INITIALIZER(
    dft3072_r24_init, const float s24 = 1.0f / sqrtf(3.0f); for (int bl = 0; bl < 16; bl++) {
      const int off = 8 * bl;
      for (int br = 1; br < 24; br++) {
        dft3072_r24_re[bl][br - 1] = pack8_twiddle_q15_re_re_scaled(off, br, 3072, s24);
        dft3072_r24_im[bl][br - 1] = pack8_twiddle_q15_im_signed_scaled(off, br, 3072, s24, DFT_DIR_FORWARD);
      }
    }

                                                            const float s2 = sqrtf(2.0f);
    for (int k = 0; k < 64; k++) {
      const float theta = 2.0f * (float)M_PI * (float)k / 128.0f;
      const int16_t raw_re = q15_from_float(cosf(theta));
      const int16_t wr = (int16_t)(raw_re / s2);
      dft3072_r2_re[k] = (uint16_t)wr | ((uint32_t)(uint16_t)wr << 16);

      for (int ds = 0; ds < 2; ds++) {
        const dft_dir_t dir = ds == 0 ? DFT_DIR_FORWARD : DFT_DIR_INVERSE;
        const int16_t raw_im = q15_from_float((float)dir * sinf(theta));
        const int16_t wi = (int16_t)(raw_im / s2);
        dft3072_r2_im[ds][k] = (uint16_t)(-wi) | ((uint32_t)(uint16_t)wi << 16);
      }
    })

static __attribute__((always_inline)) inline void dft3072_r24_r2_to_batches(const c16_t *src,
                                                                            const dft_band_t *pin,
                                                                            simde__m256i work[3][2][64],
                                                                            dft_dir_t dir)
{
  const int ds = dir == DFT_DIR_FORWARD ? 0 : 1;
  const simde__m256i s24 = simde_mm256_set1_epi16(Q15_INV_SQRT3);
  const simde__m256i s2 = simde_mm256_set1_epi16(Q15_INV_SQRT2);

  for (int block = 0; block < 8; block++) {
    const int k0 = 8 * block;
    const simde__m256i r2_re = simde_mm256_load_si256((const simde__m256i *)(dft3072_r2_re + k0));
    const simde__m256i r2_im = simde_mm256_load_si256((const simde__m256i *)(dft3072_r2_im[ds] + k0));
    simde__m256i z[2][24];
    raw_radix24_pfa8_body(src, pin, 128, k0, z[0], dir);
    raw_radix24_pfa8_body(src, pin, 128, k0 + 64, z[1], dir);

    for (int group = 0; group < 3; group++) {
      simde__m256i row[2][8];
      const int first = 8 * group;
      for (int lane = 0; lane < 8; lane++) {
        const int br = first + lane;
        const simde__m256i x0 = br == 0 ? simde_mm256_mulhrs_epi16(z[0][0], s24)
                                        : complex_mul8_prepack_q15_256(z[0][br],
                                                                       dft3072_r24_re[block][br - 1],
                                                                       twiddle_im_dir_256(dft3072_r24_im[block][br - 1], dir));
        const simde__m256i x1 = br == 0 ? simde_mm256_mulhrs_epi16(z[1][0], s24)
                                        : complex_mul8_prepack_q15_256(z[1][br],
                                                                       dft3072_r24_re[block + 8][br - 1],
                                                                       twiddle_im_dir_256(dft3072_r24_im[block + 8][br - 1], dir));
        row[0][lane] = simde_mm256_mulhrs_epi16(simde_mm256_adds_epi16(x0, x1), s2);
        row[1][lane] = complex_mul8_prepack_q15_256(simde_mm256_subs_epi16(x0, x1), r2_re, r2_im);
      }
      transpose8_complex_i16_256(&row[0][0], &row[0][1], &row[0][2], &row[0][3], &row[0][4], &row[0][5], &row[0][6], &row[0][7]);
      transpose8_complex_i16_256(&row[1][0], &row[1][1], &row[1][2], &row[1][3], &row[1][4], &row[1][5], &row[1][6], &row[1][7]);
      for (int lane = 0; lane < 8; lane++) {
        work[group][0][k0 + lane] = row[0][lane];
        work[group][1][k0 + lane] = row[1][lane];
      }
    }
  }
}

static inline void dft3072_dft64x8_pair_store(simde__m256i x[2][64], c16_t *dst, int first, dft_dir_t dir)
{
  dft64x8_store_with_dc(x[0], dst, 48, first, dir);
  dft64x8_store_with_dc(x[1], dst, 48, 24 + first, dir);
}

static void dft3072_r24_dft128x8(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  simde__m256i work[3][2][64] __attribute__((aligned(64)));
  dft3072_r24_r2_to_batches(src, NULL, work, dir);
  for (int group = 0; group < 3; group++)
    dft3072_dft64x8_pair_store(work[group], dst, 8 * group, dir);
}

#define DEFINE_R24_PARENT_TABLES(TAG, NVAL, BLOCKS)                                                \
  static simde__m256i TAG##_re[BLOCKS][24] __attribute__((aligned(64)));                           \
  /* forward direction only: the inverse negates it (twiddle_im_dir_256) */                        \
  static simde__m256i TAG##_im[BLOCKS][24] __attribute__((aligned(64)));                           \
  static DFT_TABLE_BUILDER void TAG##_init(void)                                                   \
  {                                                                                                \
    const float sc = 1.0f / sqrtf(3.0f);                                                           \
    for (int bl = 0; bl < BLOCKS; bl++) {                                                          \
      const int off = 8 * bl;                                                                      \
      for (int br = 0; br < 24; br++) {                                                            \
        TAG##_re[bl][br] = pack8_twiddle_q15_re_re_scaled(off, br, NVAL, sc);                      \
        TAG##_im[bl][br] = pack8_twiddle_q15_im_signed_scaled(off, br, NVAL, sc, DFT_DIR_FORWARD); \
      }                                                                                            \
    }                                                                                              \
  }

#define DEFINE_R24_PARENT_STAGE(TAG, MVAL)                                                                                      \
  static void TAG##_stage(const c16_t *src, c16_t *b, dft_dir_t dir)                                                            \
  {                                                                                                                             \
    for (int off = 0; off < MVAL; off += 8) {                                                                                   \
      simde__m256i z[24];                                                                                                       \
      raw_radix24_pfa8_q15_256(src, MVAL, off, z, dir);                                                                         \
      for (int br = 0; br < 24; br++) {                                                                                         \
        const simde__m256i v =                                                                                                  \
            br == 0                                                                                                             \
                ? simde_mm256_mulhrs_epi16(z[br], simde_mm256_set1_epi16(Q15_INV_SQRT3))                                        \
                : complex_mul8_prepack_q15_256(z[br], TAG##_re[off >> 3][br], twiddle_im_dir_256(TAG##_im[off >> 3][br], dir)); \
        simde_mm256_store_si256((simde__m256i *)(b + br * (MVAL) + off), v);                                                    \
      }                                                                                                                         \
    }                                                                                                                           \
  }

DEFINE_R24_PARENT_TABLES(r24_6144_parent, 6144, 32)
DEFINE_R24_PARENT_TABLES(r24_12288_parent, 12288, 64)
DEFINE_R24_PARENT_STAGE(r24_12288_parent, 512)

/* DFT6144-specific DFT256x8 child.  A vector lane is one R24
 * branch, so the R16 parent and DFT16 leaves below advance eight
 * independent DFT256 transforms together. */
static uint32_t dft6144_leaf256_re[16][16] __attribute__((aligned(64)));
static uint32_t dft6144_leaf256_im[2][16][16] __attribute__((aligned(64)));

DEFINE_Q15_TABLE_INITIALIZER(
    dft6144_leaf256_init,
    /* The DFT16 children are unitary, so these twiddles have unit magnitude. */
    const float s8 = 1.0f;
    for (int r = 0; r < 16; r++) {
      for (int n = 0; n < 16; n++) {
        const float theta = 2.0f * (float)M_PI * (float)(r * n) / 256.0f;
        const int16_t wr = q15_from_float(cosf(theta) * s8);
        dft6144_leaf256_re[r][n] = (uint16_t)wr | ((uint32_t)(uint16_t)wr << 16);
        for (int ds = 0; ds < 2; ds++) {
          const dft_dir_t d = ds == 0 ? DFT_DIR_FORWARD : DFT_DIR_INVERSE;
          const int16_t wi = q15_from_float((float)d * sinf(theta) * s8);
          dft6144_leaf256_im[ds][r][n] = (uint16_t)(-wi) | ((uint32_t)(uint16_t)wi << 16);
        }
      }
    })

static __attribute__((always_inline)) inline void dft6144_r24_parent_to_batches(const c16_t *src,
                                                                                const dft_band_t *pin,
                                                                                simde__m256i work[3][16][16],
                                                                                dft_dir_t dir)
{
  const simde__m256i s24 = simde_mm256_set1_epi16(Q15_INV_SQRT3);

  for (int off = 0; off < 256; off += 8) {
    simde__m256i z[24];
    raw_radix24_pfa8_body(src, pin, 256, off, z, dir);

    for (int group = 0; group < 3; group++) {
      const int first = 8 * group;
      simde__m256i row[8];
      for (int lane = 0; lane < 8; lane++) {
        const int br = first + lane;
        row[lane] = br == 0 ? simde_mm256_mulhrs_epi16(z[0], s24)
                            : complex_mul8_prepack_q15_256(z[br],
                                                           r24_6144_parent_re[off >> 3][br],
                                                           twiddle_im_dir_256(r24_6144_parent_im[off >> 3][br], dir));
      }
      transpose8_complex_i16_256(&row[0], &row[1], &row[2], &row[3], &row[4], &row[5], &row[6], &row[7]);
      for (int lane = 0; lane < 8; lane++) {
        const int n = off + lane;
        work[group][n >> 4][n & 15] = row[lane];
      }
    }
  }
}

static __attribute__((always_inline)) inline void dft6144_dft256x8_store(simde__m256i stage[16][16],
                                                                         c16_t *dst,
                                                                         int first,
                                                                         const dft_band_t *pout,
                                                                         dft_dir_t dir)
{
  const int ds = dir == DFT_DIR_FORWARD ? 0 : 1;

  /* R16 parent.  This is the lane-parallel form of
   * dft256_radix16(), including its exact Q15 rounding order. */
  for (int n = 0; n < 16; n++) {
    simde__m256i x[16], y[16];
    for (int q = 0; q < 16; q++)
      x[q] = stage[q][n];
    dft16x8(x, y, dir);
    stage[0][n] = y[0];
    for (int r = 1; r < 16; r++) {
      stage[r][n] = complex_mul8_prepack_q15_256(y[r],
                                                 simde_mm256_set1_epi32((int32_t)dft6144_leaf256_re[r][n]),
                                                 simde_mm256_set1_epi32((int32_t)dft6144_leaf256_im[ds][r][n]));
    }
  }

  /* Sixteen DFT16 children; each YMM is still eight independent DFT256s. */
  for (int r = 0; r < 16; r++) {
    simde__m256i y[16];
    dft16x8(stage[r], y, dir);
    for (int k = 0; k < 16; k++) {
      if (pout) /* pruned API, NR output; swapped: 16 (k ^ 8) + r */
        dft_band_store8(pout, 6144, 24 * (16 * (k ^ 8) + r) + first, k < 8, y[k]);
      else
        simde_mm256_storeu_si256((simde__m256i *)(dst + 24 * (16 * k + r) + first), y[k]);
    }
  }
}

static void dft6144_r24_leaf256(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  simde__m256i work[3][16][16] __attribute__((aligned(64)));
  dft6144_r24_parent_to_batches(src, NULL, work, dir);
  for (int group = 0; group < 3; group++)
    dft6144_dft256x8_store(work[group], dst, 8 * group, NULL, dir);
}

/* A vector lane holds one of eight independent R24 children throughout the
 * R8 parent and its DFT64 leaves. Keeping that layout between the stages
 * avoids sequential DFT512 calls and a final 8x8 output transpose. */
static uint32_t dft12288_leaf512x8_re[8][64] __attribute__((aligned(64)));
static uint32_t dft12288_leaf512x8_im[2][8][64] __attribute__((aligned(64)));

DEFINE_Q15_TABLE_INITIALIZER(
    dft12288_leaf512x8_init,
    /* The DFT8 below is half-scaled, so these twiddles carry 1/sqrt(2). */
    const float s8 = 1.0f / sqrtf(2.0f);
    for (int r = 0; r < 8; r++) {
      for (int n = 0; n < 64; n++) {
        const float theta = 2.0f * (float)M_PI * (float)(r * n) / 512.0f;
        const int16_t wr = q15_from_float(cosf(theta) * s8);
        dft12288_leaf512x8_re[r][n] = (uint16_t)wr | ((uint32_t)(uint16_t)wr << 16);
        for (int ds = 0; ds < 2; ds++) {
          const dft_dir_t d = ds == 0 ? DFT_DIR_FORWARD : DFT_DIR_INVERSE;
          const int16_t wi = q15_from_float((float)d * sinf(theta) * s8);
          dft12288_leaf512x8_im[ds][r][n] = (uint16_t)(-wi) | ((uint32_t)(uint16_t)wi << 16);
        }
      }
    })

static void dft12288_leaf512x8_store(const c16_t *b, c16_t *dst, int first, dft_dir_t dir)
{
  enum { R = 24, M = 512, LEAF_M = 64 };
  const int ds = dir == DFT_DIR_FORWARD ? 0 : 1;
  simde__m256i leaf[8][LEAF_M] __attribute__((aligned(64)));

  for (int n0 = 0; n0 < LEAF_M; n0 += 8) {
    simde__m256i input[8][8] __attribute__((aligned(64)));

    for (int q = 0; q < 8; q++) {
      simde__m256i z0 = simde_mm256_loadu_si256((const simde__m256i *)(b + (first + 0) * M + q * LEAF_M + n0));
      simde__m256i z1 = simde_mm256_loadu_si256((const simde__m256i *)(b + (first + 1) * M + q * LEAF_M + n0));
      simde__m256i z2 = simde_mm256_loadu_si256((const simde__m256i *)(b + (first + 2) * M + q * LEAF_M + n0));
      simde__m256i z3 = simde_mm256_loadu_si256((const simde__m256i *)(b + (first + 3) * M + q * LEAF_M + n0));
      simde__m256i z4 = simde_mm256_loadu_si256((const simde__m256i *)(b + (first + 4) * M + q * LEAF_M + n0));
      simde__m256i z5 = simde_mm256_loadu_si256((const simde__m256i *)(b + (first + 5) * M + q * LEAF_M + n0));
      simde__m256i z6 = simde_mm256_loadu_si256((const simde__m256i *)(b + (first + 6) * M + q * LEAF_M + n0));
      simde__m256i z7 = simde_mm256_loadu_si256((const simde__m256i *)(b + (first + 7) * M + q * LEAF_M + n0));
      transpose8_complex_i16_256(&z0, &z1, &z2, &z3, &z4, &z5, &z6, &z7);
      input[q][0] = z0;
      input[q][1] = z1;
      input[q][2] = z2;
      input[q][3] = z3;
      input[q][4] = z4;
      input[q][5] = z5;
      input[q][6] = z6;
      input[q][7] = z7;
    }

    for (int lane = 0; lane < 8; lane++) {
      const int n = n0 + lane;
      simde__m256i h[8];
      {
        const simde__m256i xin_[8] = {input[0][lane],
                                      input[1][lane],
                                      input[2][lane],
                                      input[3][lane],
                                      input[4][lane],
                                      input[5][lane],
                                      input[6][lane],
                                      input[7][lane]};
        dft8x8_q15_256_half_dir(xin_, h, dir);
      }
      for (int r = 0; r < 8; r++)
        leaf[r][n] = complex_mul8_prepack_q15_256(h[r],
                                                  simde_mm256_set1_epi32((int32_t)dft12288_leaf512x8_re[r][n]),
                                                  simde_mm256_set1_epi32((int32_t)dft12288_leaf512x8_im[ds][r][n]));
    }
  }

  for (int r = 0; r < 8; r++)
    dft64x8_store(leaf[r], dst, R * 8, first + R * r, dir);
}

static void dft12288_r24_leaf512(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { R = 24, M = 512, N = R * M };
  c16_t b[N] __attribute__((aligned(64)));
  r24_12288_parent_stage(src, b, dir);
  for (int first = 0; first < R; first += 8)
    dft12288_leaf512x8_store(b, dst, first, dir);
}

#undef DEFINE_R24_PARENT_STAGE
#undef DEFINE_R24_PARENT_TABLES

typedef void (*packed_child_dft_fn_t)(const c16_t *, c16_t *, dft_dir_t);

/* DFT648 on intermediate data (child of the radix-3 parent of 1944). */
static void radix81_terminal_leaf8_child(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  radix81_terminal_leaf8(src, dst, dir, 0);
}

static __attribute__((noinline)) void radix3_packed_child(const c16_t *src,
                                                          c16_t *dst,
                                                          int N,
                                                          packed_child_dft_fn_t child,
                                                          dft_dir_t dir)
{
  const int M = N / 3;
  c16_t tmp[N] __attribute__((aligned(64))); /* up to 144 KB (N = 36864) */

  for (int br = 0; br < 3; br++) {
    for (int n = 0; n < M; n += 4)
      simde_mm_store_si128((simde__m128i *)(tmp + br * M + n), load4_complex_strided_c16(src + br, 3, n));
    child(tmp + br * M, tmp + br * M, dir);
  }

  const r3_twiddle_t *tw = r3_twiddle_get(N);
  AssertFatal(tw, "Missing packed radix-3 twiddles N=%d\n", N);
  DFT_BIND_R3_TWIDDLES(tw, dir, w1r, w1i, w2r, w2i);

  for (int k = 0; k < M; k += 4) {
    const int b = k >> 2;
    simde__m128i y0, y1, y2;
    radix3_combine4_q15_128_fast(simde_mm_load_si128((const simde__m128i *)(tmp + 0 * M + k)),
                                 simde_mm_load_si128((const simde__m128i *)(tmp + 1 * M + k)),
                                 simde_mm_load_si128((const simde__m128i *)(tmp + 2 * M + k)),
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

static void dft1440_radix3_leaf480(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { M = 480, N = 1440 };
  c16_t tmp[N] __attribute__((aligned(64)));

  pack_radix3_permd8_1440(src, tmp);
  dft480_radix30_pfa(tmp + 0 * M, tmp + 0 * M, dir);
  dft480_radix30_pfa(tmp + 1 * M, tmp + 1 * M, dir);
  dft480_radix30_pfa(tmp + 2 * M, tmp + 2 * M, dir);

  const r3_twiddle_t *tw = r3_twiddle_get(N);
  AssertFatal(tw, "Missing DFT1440 radix-3 twiddles\n");
  radix3_combine_contiguous_avx2(tmp, dst, M, tw, dir);
}

static void dft1944_radix3_leaf648(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  radix3_packed_child(src, dst, 1944, radix81_terminal_leaf8_child, dir);
}

static void dft18432_radix3_leaf6144(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  radix3_packed_child(src, dst, 18432, dft6144_r24_leaf256, dir);
}

static void dft36864_radix3_leaf12288(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  radix3_packed_child(src, dst, 36864, dft12288_r24_leaf512, dir);
}

static __attribute__((always_inline)) inline void radix5_packed_child(const c16_t *src,
                                                                      c16_t *dst,
                                                                      int N,
                                                                      packed_child_dft_fn_t child,
                                                                      dft_dir_t dir)
{
  /* fixed-size stack buffer (no VLA on worker threads); the only user is N = 1080 */
  enum { N_MAX = 1080 };
  AssertFatal(N <= N_MAX, "radix5_packed_child: N=%d above %d\n", N, N_MAX);
  const int M = N / 5;
  c16_t tmp[N_MAX] __attribute__((aligned(64)));

  radix5_pack_children_nogather_generic(src, tmp, M);
  for (int br = 0; br < 5; br++)
    child(tmp + br * M, tmp + br * M, dir);

  const r5_twiddle_t *tw = r5_twiddle_get(N);
  AssertFatal(tw, "Missing packed radix-5 twiddles N=%d\n", N);
  radix5_combine_contiguous_avx2(tmp, dst, M, tw, dir);
}

static void dft1080_radix5_leaf216(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  radix5_packed_child(src, dst, 1080, dft216_radix27_leaf8, dir);
}

static void radix81_terminal_leaf12_972(const c16_t *src, c16_t *dst, dft_dir_t dir, const int raw_input)
{
  enum { N = 972, M1 = 108, M2 = 12 };
  c16_t work[2 * N] __attribute__((aligned(64)));
  c16_t *outer_b = work;
  c16_t *stage2 = work + N;
  const c16_t *tail_ptr[9];

  const r3_twiddle_t *twB = r3_twiddle_get(N), *twA = r3_twiddle_get(N / 3);
  const r3_twiddle_t *tw_inB = r3_twiddle_get(M1), *tw_inA = r3_twiddle_get(M1 / 3);
  AssertFatal(twB && twA && tw_inB && tw_inA, "Missing radix-81 twiddles N=%d M1=%d\n", N, M1);

  radix9_stage_to_branch_major_q15_256_128_hybrid_972(src, outer_b, dir, twB, twA, raw_input);
  for (int outer = 0; outer < 9; outer++)
    radix9_stage_to_branch_major_q15_256_128_hybrid_108(outer_b + outer * M1, stage2 + outer * M1, dir, tw_inB, tw_inA, 0);

  for (int inner = 0; inner < 9; inner++) {
    const int branch = 9 * inner;
    const c16_t *p[9];
    for (int outer = 0; outer < 9; outer++)
      p[outer] = stage2 + outer * M1 + inner * M2;
    const c16_t *main8[8] = {p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]};
    dft12x8_pointer_batch_store(main8, dst, 81, branch, 8, dir);
    tail_ptr[inner] = p[8];
  }

  c16_t tail8[8 * M2] __attribute__((aligned(64)));
  const c16_t *p8[8] = {tail_ptr[0], tail_ptr[1], tail_ptr[2], tail_ptr[3], tail_ptr[4], tail_ptr[5], tail_ptr[6], tail_ptr[7]};
  dft12x8_pointer_batch_store(p8, tail8, 8, 0, 8, dir);
  for (int k = 0; k < M2; k++)
    for (int inner = 0; inner < 8; inner++)
      dst[81 * k + 9 * inner + 8] = tail8[8 * k + inner];

  c16_t last[M2] __attribute__((aligned(32)));
  dft12_q15_128(tail_ptr[8], last, dir);
  for (int k = 0; k < M2; k++)
    dst[81 * k + 80] = last[k];
}

static void dft2916_radix3_leaf972(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { N = 2916, M = 972 };
  c16_t tmp[N] __attribute__((aligned(64)));
  for (int br = 0; br < 3; br++) {
    for (int n = 0; n < M; n += 4)
      simde_mm_store_si128((simde__m128i *)(tmp + br * M + n), load4_complex_strided_c16(src + br, 3, n));
    radix81_terminal_leaf12_972(tmp + br * M, tmp + br * M, dir, 0);
  }
  const r3_twiddle_t *tw = r3_twiddle_get(N);
  AssertFatal(tw, "Missing DFT2916 radix-3 twiddles\n");
  radix3_combine_contiguous_avx2(tmp, dst, M, tw, dir);
}

static void dft1620_radix5_leaf324(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  enum { R5 = 5, M324 = 324, R27 = 27, M12 = 12, N = 1620 };
  c16_t work[2 * N] __attribute__((aligned(64)));
  c16_t *leaf = work;
  c16_t *branches = work + N;

  radix5_pack_children_nogather_generic(src, leaf, M324);
  for (int t = 0; t < R5; t++)
    radix27_stage_324(leaf + t * M324, branches + t * M324, dir, 0);

  /* Full groups 0..23 stay exactly as in five independent N324 kernels. */
  for (int t = 0; t < R5; t++) {
    dft12x8_twiddle_scaled_store(branches + t * M324, M12, leaf + t * M324, R27, 0, 8, dir);
    dft12x8_twiddle_scaled_store(branches + t * M324, M12, leaf + t * M324, R27, 8, 8, dir);
    dft12x8_twiddle_scaled_store(branches + t * M324, M12, leaf + t * M324, R27, 16, 8, dir);
  }

  /* 5 children x 3 tail branches = 15 independent DFT12s. */
  const c16_t *tail_ptr[15];
  int tail_child[15], tail_branch[15], nt = 0;
  for (int t = 0; t < R5; t++)
    for (int br = 24; br < 27; br++) {
      tail_ptr[nt] = branches + t * M324 + br * M12;
      tail_child[nt] = t;
      tail_branch[nt] = br;
      nt++;
    }

  for (int base = 0; base < 15; base += 8) {
    const int valid = 15 - base < 8 ? 15 - base : 8;
    const c16_t *p[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    for (int l = 0; l < valid; l++)
      p[l] = tail_ptr[base + l];
    c16_t tail_out[8 * M12] __attribute__((aligned(64)));
    dft12x8_pointer_batch_store(p, tail_out, 8, 0, valid, dir);
    for (int k = 0; k < M12; k++)
      for (int l = 0; l < valid; l++) {
        const int idx = base + l;
        leaf[tail_child[idx] * M324 + R27 * k + tail_branch[idx]] = tail_out[8 * k + l];
      }
  }

  const r5_twiddle_t *tw = r5_twiddle_get(N);
  AssertFatal(tw, "Missing DFT1620 radix-5 twiddles\n");
  radix5_combine_contiguous_avx2(leaf, dst, M324, tw, dir);
}
