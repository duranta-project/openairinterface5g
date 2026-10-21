/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * Width abstraction for the 2-layer L-best SIMD kernels. Included once per width
 * (define NRLB_W = 128, 256 or 512 before including). Provides:
 *   NRLB_VI / NRLB_VF   : int16 / float vector types for this width
 *   NRLB_N              : int16 lanes (8 for 128, 16 for 256)
 *   NRLB_MM(op)         : the width's simde intrinsic prefix, e.g. NRLB_MM(mulhi_epi16)
 *   NRLB_MMF(op)        : same for float ops
 *   NRLB_NAME(base)     : mangle a function name by lane count (base##_16 / base##_8)
 *   cross-lane helpers  : nr_lbest_widen_W / load_W / pack32_W / store_llr_W (width-specialised)
 * The core kernel/slicer/seed use only NRLB_MM(...) + these helpers, so one source
 * compiles to AVX2 (256), SSE->NEON (128), and later AVX-512 (512).
 */
// (no include guard: this header is intentionally re-included per width)

#undef NRLB_VI
#undef NRLB_VF
#undef NRLB_N
#undef NRLB_MM
#undef NRLB_MMF
#undef NRLB_NAME
#undef NRLB_SETZERO
#undef NRLB_CASTPS_SI
#undef NRLB_CASTSI_PS
#undef NRLB_OR
#undef NRLB_AND
#undef NRLB_ANDNOT
#undef NRLB_BLENDV
#undef NRLB_CMPGT16
#undef NRLB_CMPEQ16
#undef NRLB_CMPPS_SI
#undef NRLB_ROUNDPS
#undef NRLB_CC1
#undef NRLB_CC

#define NRLB_CC1(a, b) a##b
#define NRLB_CC(a, b) NRLB_CC1(a, b)

#if NRLB_W == 256
  #define NRLB_VI simde__m256i
  #define NRLB_VF simde__m256
  #define NRLB_N 16
  #define NRLB_MM(op) NRLB_CC(simde_mm256_, op)
  #define NRLB_MMF(op) NRLB_CC(simde_mm256_, op)
  #define NRLB_NAME(base) NRLB_CC(base, _w256)
  #define NRLB_SETZERO() simde_mm256_setzero_si256()
  #define NRLB_CASTPS_SI(x) simde_mm256_castps_si256(x)
  #define NRLB_CASTSI_PS(x) simde_mm256_castsi256_ps(x)
  #define NRLB_OR(a, b) simde_mm256_or_si256(a, b)
  #define NRLB_AND(a, b) simde_mm256_and_si256(a, b)
  #define NRLB_ANDNOT(a, b) simde_mm256_andnot_si256(a, b)
  #define NRLB_BLENDV(a, b, m) simde_mm256_blendv_epi8(a, b, m)
  #define NRLB_CMPGT16(a, b) simde_mm256_cmpgt_epi16(a, b)
  #define NRLB_CMPEQ16(a, b) simde_mm256_cmpeq_epi16(a, b)
  #define NRLB_CMPPS_SI(a, b, imm) simde_mm256_castps_si256(simde_mm256_cmp_ps(a, b, imm))
  #define NRLB_ROUNDPS(x, imm) simde_mm256_round_ps(x, imm)
#elif NRLB_W == 128
  #define NRLB_VI simde__m128i
  #define NRLB_VF simde__m128
  #define NRLB_N 8
  #define NRLB_MM(op) NRLB_CC(simde_mm_, op)
  #define NRLB_MMF(op) NRLB_CC(simde_mm_, op)
  #define NRLB_NAME(base) NRLB_CC(base, _w128)
  #define NRLB_SETZERO() simde_mm_setzero_si128()
  #define NRLB_CASTPS_SI(x) simde_mm_castps_si128(x)
  #define NRLB_CASTSI_PS(x) simde_mm_castsi128_ps(x)
  #define NRLB_OR(a, b) simde_mm_or_si128(a, b)
  #define NRLB_AND(a, b) simde_mm_and_si128(a, b)
  #define NRLB_ANDNOT(a, b) simde_mm_andnot_si128(a, b)
  #define NRLB_BLENDV(a, b, m) simde_mm_blendv_epi8(a, b, m)
  #define NRLB_CMPGT16(a, b) simde_mm_cmpgt_epi16(a, b)
  #define NRLB_CMPEQ16(a, b) simde_mm_cmpeq_epi16(a, b)
  #define NRLB_CMPPS_SI(a, b, imm) simde_mm_castps_si128(simde_mm_cmp_ps(a, b, imm))
  #define NRLB_ROUNDPS(x, imm) simde_mm_round_ps(x, imm)
#elif NRLB_W == 512
  #define NRLB_VI simde__m512i
  #define NRLB_VF simde__m512
  #define NRLB_N 32
  #define NRLB_MM(op) NRLB_CC(simde_mm512_, op)
  #define NRLB_MMF(op) NRLB_CC(simde_mm512_, op)
  #define NRLB_NAME(base) NRLB_CC(base, _w512)
  #define NRLB_SETZERO() simde_mm512_setzero_si512()
  #define NRLB_CASTPS_SI(x) simde_mm512_castps_si512(x)
  #define NRLB_CASTSI_PS(x) simde_mm512_castsi512_ps(x)
  #define NRLB_OR(a, b) simde_mm512_or_si512(a, b)
  #define NRLB_AND(a, b) simde_mm512_and_si512(a, b)
  #define NRLB_ANDNOT(a, b) simde_mm512_andnot_si512(a, b)
  // AVX-512 compares yield k-masks; lift them back to vector (-1/0) via movm so the
  // vector-mask kernel body is unchanged. blendv_epi8 has no 512 form -> emulate with
  // movepi8_mask (high bit -> k) + mask_blend_epi8 (2 native insns: vpmovb2m + vpblendmb).
  #define NRLB_BLENDV(a, b, m) simde_mm512_mask_blend_epi8(simde_mm512_movepi8_mask(m), a, b)
  #define NRLB_CMPGT16(a, b) simde_mm512_movm_epi16(simde_mm512_cmpgt_epi16_mask(a, b))
  // equality is sign-agnostic; use the unsigned mask -- some SIMDe versions ship only cmpeq_epu16_mask.
  #define NRLB_CMPEQ16(a, b) simde_mm512_movm_epi16(simde_mm512_cmpeq_epu16_mask(a, b))
  #define NRLB_CMPPS_SI(a, b, imm) simde_mm512_movm_epi32(simde_mm512_cmp_ps_mask(a, b, imm))
  #define NRLB_ROUNDPS(x, imm) simde_mm512_roundscale_ps(x, imm)
#else
  #error "NRLB_W must be 128, 256 or 512"
#endif

// ---- cross-lane helpers (the only structurally width-specific pieces) ----

// widen int16 lane-half `hi` to int32 (N/2 lanes).
static inline NRLB_VI NRLB_NAME(nrlbw_widen)(NRLB_VI v, int hi)
{
#if NRLB_W == 512
  return hi ? simde_mm512_cvtepi16_epi32(simde_mm512_extracti64x4_epi64(v, 1))
            : simde_mm512_cvtepi16_epi32(simde_mm512_castsi512_si256(v));
#elif NRLB_W == 256
  return hi ? simde_mm256_cvtepi16_epi32(simde_mm256_extracti128_si256(v, 1))
            : simde_mm256_cvtepi16_epi32(simde_mm256_castsi256_si128(v));
#else
  // 128-bit: low half = cvt of low 4 int16; high half = cvt after an 8-byte shift
  return hi ? simde_mm_cvtepi16_epi32(simde_mm_srli_si128(v, 8))
            : simde_mm_cvtepi16_epi32(v);
#endif
}

// pack two int32 vectors (lo=lanes 0..N/2-1, hi=N/2..N-1) into one int16 vector, lane order preserved.
static inline NRLB_VI NRLB_NAME(nrlbw_pack32)(NRLB_VI lo, NRLB_VI hi)
{
#if NRLB_W == 512
  // packs_epi32 packs per-128-lane -> 64-bit words [lo0,hi0,lo1,hi1,lo2,hi2,lo3,hi3];
  // gather all lo words then all hi words to restore lane order (== 256's permute4x64).
  return simde_mm512_permutexvar_epi64(simde_mm512_set_epi64(7, 5, 3, 1, 6, 4, 2, 0), simde_mm512_packs_epi32(lo, hi));
#elif NRLB_W == 256
  return simde_mm256_permute4x64_epi64(simde_mm256_packs_epi32(lo, hi), SIMDE_MM_SHUFFLE(3, 1, 2, 0));
#else
  return simde_mm_packs_epi32(lo, hi);
#endif
}

// deinterleave N complex int16 (re,im) at p into separate re/im vectors, lane l == element l.
static inline void NRLB_NAME(nrlbw_load)(const c16_t *p, NRLB_VI *re, NRLB_VI *im)
{
#if NRLB_W == 512
  // deinterleave 32 c16: shuffle within each 128-lane groups [re0..3|im0..3] per lane,
  // then permutex2var gathers the 4 re words (even) / 4 im words (odd) from A(0..15),B(16..31).
  const simde__m512i sh = simde_mm512_broadcast_i32x4(simde_mm_setr_epi8(0, 1, 4, 5, 8, 9, 12, 13, 2, 3, 6, 7, 10, 11, 14, 15));
  const simde__m512i A = simde_mm512_shuffle_epi8(simde_mm512_loadu_si512((const void *)p), sh);
  const simde__m512i B = simde_mm512_shuffle_epi8(simde_mm512_loadu_si512((const void *)(p + 16)), sh);
  *re = simde_mm512_permutex2var_epi64(A, simde_mm512_set_epi64(14, 12, 10, 8, 6, 4, 2, 0), B);
  *im = simde_mm512_permutex2var_epi64(A, simde_mm512_set_epi64(15, 13, 11, 9, 7, 5, 3, 1), B);
#elif NRLB_W == 256
  const simde__m256i sh = simde_mm256_setr_epi8(0, 1, 4, 5, 8, 9, 12, 13, 2, 3, 6, 7, 10, 11, 14, 15,
                                                0, 1, 4, 5, 8, 9, 12, 13, 2, 3, 6, 7, 10, 11, 14, 15);
  const simde__m256i v0 = simde_mm256_permute4x64_epi64(simde_mm256_shuffle_epi8(simde_mm256_loadu_si256((const simde__m256i *)p), sh), SIMDE_MM_SHUFFLE(3, 1, 2, 0));
  const simde__m256i v1 = simde_mm256_permute4x64_epi64(simde_mm256_shuffle_epi8(simde_mm256_loadu_si256((const simde__m256i *)(p + 8)), sh), SIMDE_MM_SHUFFLE(3, 1, 2, 0));
  *re = simde_mm256_set_m128i(simde_mm256_castsi256_si128(v1), simde_mm256_castsi256_si128(v0));
  *im = simde_mm256_set_m128i(simde_mm256_extracti128_si256(v1, 1), simde_mm256_extracti128_si256(v0, 1));
#else
  // 128-bit: 8 c16 (re,im) -> shuffle so the low 8 bytes are the 4... need 8 REs: load 8 c16 = 16 int16
  const simde__m128i sh = simde_mm_setr_epi8(0, 1, 4, 5, 8, 9, 12, 13, 2, 3, 6, 7, 10, 11, 14, 15);
  const simde__m128i v0 = simde_mm_shuffle_epi8(simde_mm_loadu_si128((const simde__m128i *)p), sh);       // re0..3 | im0..3
  const simde__m128i v1 = simde_mm_shuffle_epi8(simde_mm_loadu_si128((const simde__m128i *)(p + 4)), sh); // re4..7 | im4..7
  *re = simde_mm_unpacklo_epi64(v0, v1); // re0..7
  *im = simde_mm_unpackhi_epi64(v0, v1); // im0..7
#endif
}

// store Qm LLR vectors (res[Qm], N REs across the lanes) to stream0_out[(re+r)*Qm + p].
static inline void NRLB_NAME(nrlbw_store_llr)(int16_t *stream0_out, uint32_t re, const NRLB_VI *res, int Qm)
{
#if NRLB_W == 512
  // 32 REs across the lanes. All unpack ops are per-128-lane, so the 8x8 int16 transpose
  // runs independently in each of the 4 lanes: 128-lane L holds REs 8L..8L+7, and after the
  // transpose c[j] lane-L = the Qm LLRs for RE (8L+j).
  simde__m512i r[8];
  for (int p = 0; p < Qm; p++) r[p] = res[p];
  for (int p = Qm; p < 8; p++) r[p] = simde_mm512_setzero_si512();
  const simde__m512i a0 = simde_mm512_unpacklo_epi16(r[0], r[1]), a1 = simde_mm512_unpackhi_epi16(r[0], r[1]);
  const simde__m512i a2 = simde_mm512_unpacklo_epi16(r[2], r[3]), a3 = simde_mm512_unpackhi_epi16(r[2], r[3]);
  const simde__m512i a4 = simde_mm512_unpacklo_epi16(r[4], r[5]), a5 = simde_mm512_unpackhi_epi16(r[4], r[5]);
  const simde__m512i a6 = simde_mm512_unpacklo_epi16(r[6], r[7]), a7 = simde_mm512_unpackhi_epi16(r[6], r[7]);
  const simde__m512i b0 = simde_mm512_unpacklo_epi32(a0, a2), b1 = simde_mm512_unpackhi_epi32(a0, a2);
  const simde__m512i b2 = simde_mm512_unpacklo_epi32(a1, a3), b3 = simde_mm512_unpackhi_epi32(a1, a3);
  const simde__m512i b4 = simde_mm512_unpacklo_epi32(a4, a6), b5 = simde_mm512_unpackhi_epi32(a4, a6);
  const simde__m512i b6 = simde_mm512_unpacklo_epi32(a5, a7), b7 = simde_mm512_unpackhi_epi32(a5, a7);
  simde__m512i c[8];
  c[0] = simde_mm512_unpacklo_epi64(b0, b4); c[1] = simde_mm512_unpackhi_epi64(b0, b4);
  c[2] = simde_mm512_unpacklo_epi64(b1, b5); c[3] = simde_mm512_unpackhi_epi64(b1, b5);
  c[4] = simde_mm512_unpacklo_epi64(b2, b6); c[5] = simde_mm512_unpackhi_epi64(b2, b6);
  c[6] = simde_mm512_unpacklo_epi64(b3, b7); c[7] = simde_mm512_unpackhi_epi64(b3, b7);
  for (int j = 0; j < 8; j++) {
    // extracti32x4 needs a compile-time-constant lane -> extract all 4 with literal indices,
    // then index at runtime (a loop variable in the lane arg fails at -O0, where nothing unrolls).
    const simde__m128i cc[4] = {simde_mm512_extracti32x4_epi32(c[j], 0), simde_mm512_extracti32x4_epi32(c[j], 1),
                                simde_mm512_extracti32x4_epi32(c[j], 2), simde_mm512_extracti32x4_epi32(c[j], 3)};
    for (int L = 0; L < 4; L++) {
      int16_t *o = &stream0_out[(re + (uint32_t)(L * 8 + j)) * Qm];
      if (Qm == 8) simde_mm_storeu_si128((simde__m128i *)o, cc[L]);
      else { simde_mm_storel_epi64((simde__m128i *)o, cc[L]); *(int32_t *)(o + 4) = simde_mm_extract_epi32(cc[L], 2); }
    }
  }
#elif NRLB_W == 256
  simde__m256i r[8];
  for (int p = 0; p < Qm; p++) r[p] = res[p];
  for (int p = Qm; p < 8; p++) r[p] = simde_mm256_setzero_si256();
  const simde__m256i a0 = simde_mm256_unpacklo_epi16(r[0], r[1]), a1 = simde_mm256_unpackhi_epi16(r[0], r[1]);
  const simde__m256i a2 = simde_mm256_unpacklo_epi16(r[2], r[3]), a3 = simde_mm256_unpackhi_epi16(r[2], r[3]);
  const simde__m256i a4 = simde_mm256_unpacklo_epi16(r[4], r[5]), a5 = simde_mm256_unpackhi_epi16(r[4], r[5]);
  const simde__m256i a6 = simde_mm256_unpacklo_epi16(r[6], r[7]), a7 = simde_mm256_unpackhi_epi16(r[6], r[7]);
  const simde__m256i b0 = simde_mm256_unpacklo_epi32(a0, a2), b1 = simde_mm256_unpackhi_epi32(a0, a2);
  const simde__m256i b2 = simde_mm256_unpacklo_epi32(a1, a3), b3 = simde_mm256_unpackhi_epi32(a1, a3);
  const simde__m256i b4 = simde_mm256_unpacklo_epi32(a4, a6), b5 = simde_mm256_unpackhi_epi32(a4, a6);
  const simde__m256i b6 = simde_mm256_unpacklo_epi32(a5, a7), b7 = simde_mm256_unpackhi_epi32(a5, a7);
  simde__m256i c[8];
  c[0] = simde_mm256_unpacklo_epi64(b0, b4); c[1] = simde_mm256_unpackhi_epi64(b0, b4);
  c[2] = simde_mm256_unpacklo_epi64(b1, b5); c[3] = simde_mm256_unpackhi_epi64(b1, b5);
  c[4] = simde_mm256_unpacklo_epi64(b2, b6); c[5] = simde_mm256_unpackhi_epi64(b2, b6);
  c[6] = simde_mm256_unpacklo_epi64(b3, b7); c[7] = simde_mm256_unpackhi_epi64(b3, b7);
  for (int j = 0; j < 8; j++) {
    const simde__m128i lo = simde_mm256_castsi256_si128(c[j]), hi = simde_mm256_extracti128_si256(c[j], 1);
    int16_t *o0 = &stream0_out[(re + j) * Qm], *o8 = &stream0_out[(re + j + 8) * Qm];
    if (Qm == 8) { simde_mm_storeu_si128((simde__m128i *)o0, lo); simde_mm_storeu_si128((simde__m128i *)o8, hi); }
    else { simde_mm_storel_epi64((simde__m128i *)o0, lo); *(int32_t *)(o0 + 4) = simde_mm_extract_epi32(lo, 2);
           simde_mm_storel_epi64((simde__m128i *)o8, hi); *(int32_t *)(o8 + 4) = simde_mm_extract_epi32(hi, 2); }
  }
#else
  // 128-bit: 8 REs in one lane. Transpose 8x8 int16 (padded from Qm) via unpack, store Qm/RE.
  simde__m128i r[8];
  for (int p = 0; p < Qm; p++) r[p] = res[p];
  for (int p = Qm; p < 8; p++) r[p] = simde_mm_setzero_si128();
  const simde__m128i a0 = simde_mm_unpacklo_epi16(r[0], r[1]), a1 = simde_mm_unpackhi_epi16(r[0], r[1]);
  const simde__m128i a2 = simde_mm_unpacklo_epi16(r[2], r[3]), a3 = simde_mm_unpackhi_epi16(r[2], r[3]);
  const simde__m128i a4 = simde_mm_unpacklo_epi16(r[4], r[5]), a5 = simde_mm_unpackhi_epi16(r[4], r[5]);
  const simde__m128i a6 = simde_mm_unpacklo_epi16(r[6], r[7]), a7 = simde_mm_unpackhi_epi16(r[6], r[7]);
  const simde__m128i b0 = simde_mm_unpacklo_epi32(a0, a2), b1 = simde_mm_unpackhi_epi32(a0, a2);
  const simde__m128i b2 = simde_mm_unpacklo_epi32(a1, a3), b3 = simde_mm_unpackhi_epi32(a1, a3);
  const simde__m128i b4 = simde_mm_unpacklo_epi32(a4, a6), b5 = simde_mm_unpackhi_epi32(a4, a6);
  const simde__m128i b6 = simde_mm_unpacklo_epi32(a5, a7), b7 = simde_mm_unpackhi_epi32(a5, a7);
  simde__m128i c[8];
  c[0] = simde_mm_unpacklo_epi64(b0, b4); c[1] = simde_mm_unpackhi_epi64(b0, b4);
  c[2] = simde_mm_unpacklo_epi64(b1, b5); c[3] = simde_mm_unpackhi_epi64(b1, b5);
  c[4] = simde_mm_unpacklo_epi64(b2, b6); c[5] = simde_mm_unpackhi_epi64(b2, b6);
  c[6] = simde_mm_unpacklo_epi64(b3, b7); c[7] = simde_mm_unpackhi_epi64(b3, b7);
  for (int j = 0; j < 8; j++) {
    int16_t *o = &stream0_out[(re + j) * Qm];
    if (Qm == 8) simde_mm_storeu_si128((simde__m128i *)o, c[j]);
    else { simde_mm_storel_epi64((simde__m128i *)o, c[j]); *(int32_t *)(o + 4) = simde_mm_extract_epi32(c[j], 2); }
  }
#endif
}
