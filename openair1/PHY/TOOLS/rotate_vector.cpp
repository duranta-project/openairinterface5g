/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdint.h>
#include "openair1/PHY/TOOLS/tools_defs.h"

// Undefine macro 'T' from common/utils/T/T.h to prevent macro collision with Highway template parameter names.
#undef T
#include <hwy/highway.h>

#if HWY_ARCH_ARM_A64 && defined(__ARM_FEATURE_SVE)
#include <arm_sve.h>

// Hand-written SVE2 intrinsics implementation
static inline void rotate_cpx_vector_sve2_shift15(const c16_t *const x, const c16_t alpha, c16_t *y, uint32_t N)
{
  namespace hn = hwy::HWY_NAMESPACE;
  const hn::ScalableTag<int16_t> d16;
  const size_t lanes = hn::Lanes(d16);
  size_t i = 0;
  for (; i + 2 * lanes <= 2 * N; i += 2 * lanes) {
    svint16_t sv_x0 = svld1_s16(svptrue_b16(), reinterpret_cast<const int16_t*>(x) + i);
    svint16_t sv_x1 = svld1_s16(svptrue_b16(), reinterpret_cast<const int16_t*>(x) + i + lanes);

    svint16_t sv_ar = svdup_n_s16(alpha.r);
    svint16_t sv_ai = svdup_n_s16(alpha.i);

    svint16_t sv_br = svuzp1_s16(sv_x0, sv_x1);
    svint16_t sv_bi = svuzp2_s16(sv_x0, sv_x1);

    svint16_t sv_real = svqdmulh_s16(sv_ar, sv_br);
    svint16_t sv_imag = svqdmulh_s16(sv_ar, sv_bi);

    sv_real = svqrdmlsh_s16(sv_real, sv_ai, sv_bi);
    sv_imag = svqrdmlah_s16(sv_imag, sv_ai, sv_br);

    svint16_t sv_y0 = svzip1_s16(sv_real, sv_imag);
    svint16_t sv_y1 = svzip2_s16(sv_real, sv_imag);

    svst1_s16(svptrue_b16(), reinterpret_cast<int16_t*>(y) + i, sv_y0);
    svst1_s16(svptrue_b16(), reinterpret_cast<int16_t*>(y) + i + lanes, sv_y1);
  }

  // Fallback for remaining elements (SVE2 specific tail)
  for (size_t k = i / 2; k < N; ++k) {
    int32_t prod_r1 = x[k].r * alpha.r;
    int32_t prod_r2 = x[k].i * alpha.i;
    int32_t prod_i1 = x[k].r * alpha.i;
    int32_t prod_i2 = x[k].i * alpha.r;

    auto saturate_double_prod = [](int64_t v) -> int32_t {
      if (v > 2147483647) return 2147483647;
      if (v < -2147483648) return -2147483648;
      return (int32_t)v;
    };

    int32_t dp_r1 = saturate_double_prod(2LL * prod_r1);
    int32_t dp_r2 = saturate_double_prod(2LL * prod_r2);
    int32_t dp_i1 = saturate_double_prod(2LL * prod_i1);
    int32_t dp_i2 = saturate_double_prod(2LL * prod_i2);

    int32_t r1 = dp_r1 >> 16;
    int32_t r2 = (dp_r2 + 32768) >> 16;
    int32_t i1 = (dp_i1 + 32768) >> 16;
    int32_t i2 = dp_i2 >> 16;

    int32_t r_final = r1 - r2;
    int32_t im_final = i1 + i2;

    y[k].r = r_final > 32767 ? 32767 : (r_final < -32768 ? -32768 : r_final);
    y[k].i = im_final > 32767 ? 32767 : (im_final < -32768 ? -32768 : im_final);
  }
}
#endif

#if HWY_ARCH_X86
// Legacy hand-written AVX2 implementation (SIMDE intrinsics), byte-for-byte the same algorithm that
// shipped here before the Highway rewrite. Used only when compiling for the plain AVX2 target: the
// portable Highway path below does not beat this on AVX2, so there is no reason to carry the risk of
// deviating from a long-proven implementation for that target specifically.
static inline void rotate_cpx_vector_avx2_legacy(const c16_t *const x, const c16_t alpha, c16_t *y, uint32_t N, uint16_t output_shift)
{
  // output is 32 bytes aligned, but not the input
  const c16_t for_re = {alpha.r, (int16_t)-alpha.i};
  const simde__m256i alpha_for_real = simde_mm256_set1_epi32(*(uint32_t *)&for_re);
  const c16_t for_im = {alpha.i, alpha.r};
  const simde__m256i alpha_for_im = simde_mm256_set1_epi32(*(uint32_t *)&for_im);
  const simde__m256i perm_mask = simde_mm256_set_epi8(31,
                                                      30,
                                                      23,
                                                      22,
                                                      29,
                                                      28,
                                                      21,
                                                      20,
                                                      27,
                                                      26,
                                                      19,
                                                      18,
                                                      25,
                                                      24,
                                                      17,
                                                      16,
                                                      15,
                                                      14,
                                                      7,
                                                      6,
                                                      13,
                                                      12,
                                                      5,
                                                      4,
                                                      11,
                                                      10,
                                                      3,
                                                      2,
                                                      9,
                                                      8,
                                                      1,
                                                      0);
  simde__m256i *xd = (simde__m256i *)x;
  const simde__m256i *end = xd + N / 8;
  for (simde__m256i *yd = (simde__m256i *)y; xd < end; yd++, xd++) {
    const simde__m256i y256 = simde_mm256_lddqu_si256(xd);
    const simde__m256i xre = simde_mm256_srai_epi32(simde_mm256_madd_epi16(y256, alpha_for_real), output_shift);
    const simde__m256i xim = simde_mm256_srai_epi32(simde_mm256_madd_epi16(y256, alpha_for_im), output_shift);
    // a bit faster than unpacklo+unpackhi+packs
    const simde__m256i tmp = simde_mm256_packs_epi32(xre, xim);
    simde_mm256_storeu_si256(yd, simde_mm256_shuffle_epi8(tmp, perm_mask));
  }
  // Saturating tail, to match the main loop's packs_epi32 above and the other targets' tails
  // (the original code used the non-saturating c16mulShift here, an inconsistency with its own
  // main loop that this deliberately fixes).
  c16_t *yLast = ((c16_t *)y) + (N / 8) * 8;
  for (c16_t *xTail = (c16_t *)end; xTail < ((c16_t *)x) + N; xTail++, yLast++) {
    int32_t r = (int32_t)xTail->r * alpha.r - (int32_t)xTail->i * alpha.i;
    int32_t im = (int32_t)xTail->r * alpha.i + (int32_t)xTail->i * alpha.r;
    r >>= output_shift;
    im >>= output_shift;
    yLast->r = r > 32767 ? 32767 : (r < -32768 ? -32768 : r);
    yLast->i = im > 32767 ? 32767 : (im < -32768 ? -32768 : im);
  }
}

// x86-specific implementation. ReorderDemote2To(d16, real, imag) packs two full-width int32
// vectors into one full-width int16 vector with a single instruction (e.g. vpackssdw) -- but x86's
// pack instructions operate per 128-bit lane, so each 128-bit lane of the result independently holds
// [4 reals, 4 imags] rather than a clean concatenation. A single TableLookupBytes (vpshufb), whose
// mask is replicated per 128-bit lane via LoadDup128, interleaves that into Re,Im,Re,Im,... order in
// one more instruction. This mirrors what the hand-tuned AVX2 reference does (pack + one pshufb) but
// stays portable across SSE/AVX2/AVX-512 since vpshufb's per-lane behavior is uniform across all of
// them. rotate_cpx_vector_generic below pays for two separate fixups (OrderedDemote2To's own permute,
// then StoreInterleaved2's block transpose) where this needs only one.
static inline void rotate_cpx_vector_x86(const c16_t *const x, const c16_t alpha, c16_t *y, uint32_t N, uint16_t output_shift)
{
  namespace hn = hwy::HWY_NAMESPACE;
  const hn::ScalableTag<int16_t> d16;
  const hn::ScalableTag<int32_t> d32;
  const hn::Repartition<uint8_t, decltype(d16)> d8;

  const int32_t val_real = (static_cast<uint16_t>(alpha.r)) | (static_cast<uint32_t>(static_cast<uint16_t>(-alpha.i)) << 16);
  const int32_t val_imag = (static_cast<uint16_t>(alpha.i)) | (static_cast<uint32_t>(static_cast<uint16_t>(alpha.r)) << 16);

  const auto valpha_real = hn::BitCast(d16, hn::Set(d32, val_real));
  const auto valpha_imag = hn::BitCast(d16, hn::Set(d32, val_imag));

  // Per-128-bit-lane byte permutation: [Re0,Re1,Re2,Re3,Im0,Im1,Im2,Im3] -> [Re0,Im0,Re1,Im1,Re2,Im2,Re3,Im3]
  alignas(16) static constexpr uint8_t kInterleaveMask[16] = {0, 1, 8, 9, 2, 3, 10, 11, 4, 5, 12, 13, 6, 7, 14, 15};
  const auto perm = hn::LoadDup128(d8, kInterleaveMask);

  const size_t lanes = hn::Lanes(d16);
  size_t i = 0;
  for (; i + lanes <= 2 * N; i += lanes) {
    const auto vx = hn::LoadU(d16, reinterpret_cast<const int16_t*>(x) + i);

    auto p_real = hn::WidenMulPairwiseAdd(d32, vx, valpha_real);
    auto p_imag = hn::WidenMulPairwiseAdd(d32, vx, valpha_imag);

    p_real = hn::ShiftRightSame(p_real, output_shift);
    p_imag = hn::ShiftRightSame(p_imag, output_shift);

    const auto packed = hn::ReorderDemote2To(d16, p_real, p_imag);
    const auto interleaved = hn::BitCast(d16, hn::TableLookupBytes(hn::BitCast(d8, packed), perm));

    hn::StoreU(interleaved, d16, reinterpret_cast<int16_t*>(y) + i);
  }

  // Fallback for remaining elements
  for (size_t k = i / 2; k < N; ++k) {
    int32_t r = (x[k].r * alpha.r - x[k].i * alpha.i);
    int32_t im = (x[k].r * alpha.i + x[k].i * alpha.r);
    r >>= output_shift;
    im >>= output_shift;
    y[k].r = r > 32767 ? 32767 : (r < -32768 ? -32768 : r);
    y[k].i = im > 32767 ? 32767 : (im < -32768 ? -32768 : im);
  }
}
#endif

// Google Highway implementation
static inline void rotate_cpx_vector_generic(const c16_t *const x, const c16_t alpha, c16_t *y, uint32_t N, uint16_t output_shift)
{
  namespace hn = hwy::HWY_NAMESPACE;
  const hn::ScalableTag<int16_t> d16;
  const hn::ScalableTag<int32_t> d32;

  const int32_t val_real = (static_cast<uint16_t>(alpha.r)) | (static_cast<uint32_t>(static_cast<uint16_t>(-alpha.i)) << 16);
  const int32_t val_imag = (static_cast<uint16_t>(alpha.i)) | (static_cast<uint32_t>(static_cast<uint16_t>(alpha.r)) << 16);

  const auto valpha_real = hn::BitCast(d16, hn::Set(d32, val_real));
  const auto valpha_imag = hn::BitCast(d16, hn::Set(d32, val_imag));

  const size_t lanes = hn::Lanes(d16);
  size_t i = 0;
  for (; i + 2 * lanes <= 2 * N; i += 2 * lanes) {
    const auto vx0 = hn::LoadU(d16, reinterpret_cast<const int16_t*>(x) + i);
    const auto vx1 = hn::LoadU(d16, reinterpret_cast<const int16_t*>(x) + i + lanes);

    auto p_real0 = hn::WidenMulPairwiseAdd(d32, vx0, valpha_real);
    auto p_imag0 = hn::WidenMulPairwiseAdd(d32, vx0, valpha_imag);
    auto p_real1 = hn::WidenMulPairwiseAdd(d32, vx1, valpha_real);
    auto p_imag1 = hn::WidenMulPairwiseAdd(d32, vx1, valpha_imag);

    p_real0 = hn::ShiftRightSame(p_real0, output_shift);
    p_imag0 = hn::ShiftRightSame(p_imag0, output_shift);
    p_real1 = hn::ShiftRightSame(p_real1, output_shift);
    p_imag1 = hn::ShiftRightSame(p_imag1, output_shift);

    const auto y_re = hn::OrderedDemote2To(d16, p_real0, p_real1);
    const auto y_im = hn::OrderedDemote2To(d16, p_imag0, p_imag1);

    hn::StoreInterleaved2(y_re, y_im, d16, reinterpret_cast<int16_t*>(y) + i);
  }

  // Fallback for remaining elements
  for (size_t k = i / 2; k < N; ++k) {
    int32_t r = (x[k].r * alpha.r - x[k].i * alpha.i);
    int32_t im = (x[k].r * alpha.i + x[k].i * alpha.r);
    r >>= output_shift;
    im >>= output_shift;
    y[k].r = r > 32767 ? 32767 : (r < -32768 ? -32768 : r);
    y[k].i = im > 32767 ? 32767 : (im < -32768 ? -32768 : im);
  }
}

// C-compliant dynamic shift function API
extern "C" void rotate_cpx_vector(const c16_t *const x, const c16_t alpha, c16_t *y, uint32_t N, uint16_t output_shift)
{
#if HWY_ARCH_ARM_A64 && defined(__ARM_FEATURE_SVE)
  if (output_shift == 15) {
    rotate_cpx_vector_sve2_shift15(x, alpha, y, N);
    return;
  }
#endif
#if HWY_ARCH_X86
#if HWY_TARGET == HWY_AVX2
  rotate_cpx_vector_avx2_legacy(x, alpha, y, N, output_shift);
#else
  rotate_cpx_vector_x86(x, alpha, y, N, output_shift);
#endif
#else
  rotate_cpx_vector_generic(x, alpha, y, N, output_shift);
#endif
}
