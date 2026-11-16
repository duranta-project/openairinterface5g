/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!\file nrLDPC_cnProc_rot.h
 * \brief CN processing with the cyclic shifts folded into its loads and stores.
 *
 * The generic decoder rotates every edge's Z-vector into a CN-ordered buffer before CN processing
 * (llr2CnProcBuf / bn2cnProcBuf) and back after it (cn2bnProcBuf): two copies of the whole edge set per iteration.
 * Here the CN kernel reads each edge at its rotated offset in the BN-ordered buffer (in the input LLRs on the first
 * iteration) and writes its result at the same rotated offset into bnProcBuf, so cnProcBuf and cnProcBufRes are not
 * used. Edge e of a CN reads src[(t + s_e) mod Z] for position t, which is what circ_memcpy puts at cnProcBuf[t], and
 * inv_circ_memcpy puts cnProcBufRes[t] back at that same position: the results are those of nrLDPC_cnProc_BG*_2pass
 * with the copies.
 */

#ifndef __NR_LDPC_CNPROC_ROT__H__
#define __NR_LDPC_CNPROC_ROT__H__

#include "nrLDPCdecoder_defs.h"
#include "nrLDPC_types.h"

#define NR_LDPC_ROT_MAX_CN 46

/** Edges in CN order: offset of the edge's Z-vector in bnProcBuf / bnProcBufRes and in the input LLRs, and its shift */
typedef struct {
  uint16_t numCn;
  uint8_t deg[NR_LDPC_ROT_MAX_CN];
  uint32_t bnOff[NR_LDPC_NUM_EDGE_BG1];
  uint32_t llrOff[NR_LDPC_NUM_EDGE_BG1];
  uint16_t shift[NR_LDPC_NUM_EDGE_BG1];
} nrLDPC_cnEdges_t;

/** Walks the CN groups like nrLDPC_llr2CnProcBuf_BG* / nrLDPC_cn2bnProcBuf_BG* */
static inline void nrLDPC_cnEdges_init(const t_nrLDPC_lut *p_lut, uint8_t BG, uint16_t Z, nrLDPC_cnEdges_t *e)
{
  static const uint8_t deg1[] = {3, 4, 5, 6, 7, 8, 9, 10, 19};
  static const uint8_t deg2[] = {3, 4, 5, 6, 8, 10};
  const uint8_t *deg = BG == 1 ? deg1 : deg2;
  const int numGroups = BG == 1 ? sizeofArray(deg1) : sizeofArray(deg2);
  int n = 0, c = 0;
  for (int g = 0; g < numGroups; g++) {
    for (int i = 0; i < p_lut->numCnInCnGroups[g]; i++, c++) {
      e->deg[c] = deg[g];
      for (int j = 0; j < deg[g]; j++, n++) {
        // BG1 leaves bnPosBnProcBuf[0] unset: its single degree-3 CN is BN position 0 (see nrLDPC_cn2bnProcBuf_BG1)
        const int bnPos = BG == 1 && g == 0 ? 0 : (arrPos(p_lut->bnPosBnProcBuf[g], j))[i];
        e->bnOff[n] = (arrPos(p_lut->startAddrBnProcBuf[g], j))[i] + bnPos * Z;
        e->llrOff[n] = (arrPos(p_lut->posBnInCnProcBuf[g], j))[i] * Z;
        e->shift[n] = (arrPos(p_lut->circShift[g], j))[i];
      }
    }
  }
  e->numCn = c;
}

#if defined(__AVX512BW__)
#include <simde/x86/avx512.h>

static inline simde__mmask64 nrLDPC_rot_lo(uint32_t n)
{
  return n >= 64 ? ~(simde__mmask64)0 : ((simde__mmask64)1 << n) - 1;
}

// Strip [t, t + vl) of an edge rotated by p - t: lanes [0, n1 = Z - p) come from v + p, the rest from v - n1; the
// masked-off lanes of the masked accesses are not read or written.
static inline simde__m512i nrLDPC_rot_load(const int8_t *v, uint32_t p, uint32_t Z, simde__mmask64 mvl)
{
  const simde__mmask64 m1 = nrLDPC_rot_lo(Z - p) & mvl;
  const simde__m512i a = simde_mm512_maskz_loadu_epi8(m1, v + p);
  if (m1 == mvl)
    return a;
  return simde_mm512_mask_loadu_epi8(a, mvl & ~m1, v - (Z - p));
}

static inline void nrLDPC_rot_store(int8_t *v, uint32_t p, uint32_t Z, simde__m512i x, simde__mmask64 mvl)
{
  const simde__mmask64 m1 = nrLDPC_rot_lo(Z - p) & mvl;
  simde_mm512_mask_storeu_epi8(v + p, m1, x);
  if (m1 != mvl)
    simde_mm512_mask_storeu_epi8(v - (Z - p), mvl & ~m1, x);
}

/**
   \brief Two-pass min-sum over all CNs, as nrLDPC_cnProc_group_2pass_512: edge n read at src[srcOff[n]] rotated, its
   CN-to-BN message written at bnProcBuf[bnOff[n]] rotated the same way.
*/
static inline void nrLDPC_cnProc_rot(const nrLDPC_cnEdges_t *e,
                                     const int8_t *src,
                                     const uint32_t *srcOff,
                                     int8_t *bnProcBuf,
                                     uint16_t Z)
{
  const simde__m512i maxLLR = simde_mm512_set1_epi8(127);
  const simde__m512i zeros = simde_mm512_setzero_si512();
  const uint32_t *bnOff = e->bnOff;
  const uint16_t *sh = e->shift;
  for (int c = 0, n0 = 0; c < e->numCn; n0 += e->deg[c++]) {
    const int n1 = n0 + e->deg[c];
    for (uint32_t t = 0; t < Z; t += 64) {
      const simde__mmask64 mvl = nrLDPC_rot_lo(Z - t);
      simde__m512i min1 = maxLLR, min2 = maxLLR, sgn = zeros;
      for (int k = n0; k < n1; k++) {
        const uint32_t p = t + sh[k] < Z ? t + sh[k] : t + sh[k] - Z;
        const simde__m512i x = nrLDPC_rot_load(src + srcOff[k], p, Z, mvl);
        const simde__m512i a = simde_mm512_abs_epi8(x);
        sgn = simde_mm512_xor_si512(sgn, x);
        min2 = simde_mm512_min_epu8(min2, simde_mm512_max_epu8(min1, a));
        min1 = simde_mm512_min_epu8(min1, a);
      }
      for (int k = n0; k < n1; k++) {
        const uint32_t p = t + sh[k] < Z ? t + sh[k] : t + sh[k] - Z;
        const simde__m512i x = nrLDPC_rot_load(src + srcOff[k], p, Z, mvl);
        const simde__m512i a = simde_mm512_abs_epi8(x);
        const simde__m512i mag =
            simde_mm512_min_epu8(simde_mm512_mask_blend_epi8(simde_mm512_cmpeq_epi8_mask(a, min1), min1, min2), maxLLR);
        const simde__mmask64 neg = simde_mm512_movepi8_mask(simde_mm512_xor_si512(sgn, x));
        nrLDPC_rot_store(bnProcBuf + bnOff[k], p, Z, simde_mm512_mask_sub_epi8(mag, neg, zeros, mag), mvl);
      }
    }
  }
}

/**
   \brief Parity check on the rotated BN-to-CN (bnProcBufRes) plus CN-to-BN (bnProcBuf) messages, as
   nrLDPC_cnProcPc_BG* on cnProcBuf + cnProcBufRes: 1 at the first unsatisfied check.
*/
static inline uint32_t nrLDPC_cnProcPc_rot(const nrLDPC_cnEdges_t *e,
                                           const int8_t *bnProcBufRes,
                                           const int8_t *bnProcBuf,
                                           uint16_t Z)
{
  const uint32_t *bnOff = e->bnOff;
  const uint16_t *sh = e->shift;
  for (int c = 0, n0 = 0; c < e->numCn; n0 += e->deg[c++]) {
    for (uint32_t t = 0; t < Z; t += 64) {
      const simde__mmask64 mvl = nrLDPC_rot_lo(Z - t);
      simde__m512i par = simde_mm512_setzero_si512();
      for (int k = n0; k < n0 + e->deg[c]; k++) {
        const uint32_t p = t + sh[k] < Z ? t + sh[k] : t + sh[k] - Z;
        const simde__m512i in = nrLDPC_rot_load(bnProcBufRes + bnOff[k], p, Z, mvl);
        const simde__m512i res = nrLDPC_rot_load(bnProcBuf + bnOff[k], p, Z, mvl);
        par = simde_mm512_xor_si512(par, simde_mm512_adds_epi8(in, res));
      }
      if (simde_mm512_movepi8_mask(par) & mvl)
        return 1;
    }
  }
  return 0;
}
#define NR_LDPC_ROT 1
#else
// Without byte-masked loads and stores (AVX2, NEON), the wrapped and partial strips cost more than the copies save.
#define NR_LDPC_ROT 0
#endif

#endif
