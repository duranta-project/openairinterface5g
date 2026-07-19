/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Defines the functions for bit node processing
 */

#ifndef __NR_LDPC_BNPROC__H__
#define __NR_LDPC_BNPROC__H__
#include "PHY/sse_intrin.h"
#if defined(__riscv) && defined(__riscv_vector)
#include <riscv_vector.h>
#endif
#if defined(__AVX512BW__)
#include <simde/x86/avx512.h>
#endif

#if defined(__riscv) && defined(__riscv_vector)
static inline vint8m1_t nrLDPC_rvv_clip_i16m2_to_i8m1(vint16m2_t v, size_t vl)
{
    v = __riscv_vmax_vx_i16m2(v, -128, vl);
    v = __riscv_vmin_vx_i16m2(v, 127, vl);
    return __riscv_vnclip_wx_i8m1(v, 0, __RISCV_VXRM_RNU, vl);
}
#endif

/**
   \brief Performs first part of BN processing on the BN processing buffer and stores the results in the LLR results buffer.
          At every BN, the sum of the returned LLRs from the connected CNs and the LLR of the receiver input is computed.
   \param p_lut Pointer to decoder LUTs
   \param Z Lifting size
*/
static inline void nrLDPC_bnProcPc(t_nrLDPC_lut* p_lut, int8_t* bnProcBuf, int8_t* bnProcBufRes, int8_t* llrProcBuf, int8_t* llrRes, uint16_t Z)
{
    const uint8_t*  lut_numBnInBnGroups      = p_lut->numBnInBnGroups;
    const uint32_t* lut_startAddrBnGroups    = p_lut->startAddrBnGroups;
    const uint16_t* lut_startAddrBnGroupsLlr = p_lut->startAddrBnGroupsLlr;
    uint8_t idxBnGroup = 0;

    for (uint32_t grp = 0; grp < NR_LDPC_NUM_BN_GROUPS_BG1_R13; grp++) {
        uint32_t numBN = lut_numBnInBnGroups[grp];
        if (numBN == 0) continue;
        uint32_t numCN    = grp + 1;
        uint32_t bnStart  = lut_startAddrBnGroups[idxBnGroup];
        uint32_t llrStart = lut_startAddrBnGroupsLlr[idxBnGroup];
        idxBnGroup++;

#if defined(__riscv) && defined(__riscv_vector)
        {
            int8_t *buf = &bnProcBuf[bnStart];
            int8_t *llr = &llrProcBuf[llrStart];
            int8_t *res = &llrRes[llrStart];
            uint32_t off = numBN * NR_LDPC_ZMAX;
            uint32_t numBytes = numBN * Z;
            for (uint32_t i = 0; i < numBytes;) {
                size_t vl = __riscv_vsetvl_e8m1(numBytes - i);
                vint8m1_t vbuf = __riscv_vle8_v_i8m1(&buf[i], vl);
                vint16m2_t acc = __riscv_vsext_vf2_i16m2(vbuf, vl);
                for (uint32_t k = 1; k < numCN; k++) {
                    vbuf = __riscv_vle8_v_i8m1(&buf[k * off + i], vl);
                    acc = __riscv_vadd_vv_i16m2(acc, __riscv_vsext_vf2_i16m2(vbuf, vl), vl);
                }
                vint8m1_t vllr = __riscv_vle8_v_i8m1(&llr[i], vl);
                acc = __riscv_vadd_vv_i16m2(acc, __riscv_vsext_vf2_i16m2(vllr, vl), vl);
                __riscv_vse8_v_i8m1(&res[i], nrLDPC_rvv_clip_i16m2_to_i8m1(acc, vl), vl);
                i += vl;
            }
        }
#elif defined(__AVX512BW__)
        {
            /* Load 256-bit int8 chunks, accumulate in 512-bit int16, store 256-bit int8. */
            simde__m256i *buf256 = (simde__m256i *) &bnProcBuf[bnStart];
            simde__m256i *llr256 = (simde__m256i *) &llrProcBuf[llrStart];
            simde__m256i *res256 = (simde__m256i *) &llrRes[llrStart];
            uint32_t off = (numBN * NR_LDPC_ZMAX) >> 5;
            uint32_t M   = (numBN * Z + 31) >> 5;
            for (uint32_t i = 0; i < M; i++) {
                simde__m512i acc = simde_mm512_cvtepi8_epi16(buf256[i]);
                for (uint32_t k = 1; k < numCN; k++)
                    acc = simde_mm512_adds_epi16(acc, simde_mm512_cvtepi8_epi16(buf256[k * off + i]));
                acc = simde_mm512_adds_epi16(acc, simde_mm512_cvtepi8_epi16(llr256[i]));
                res256[i] = simde_mm512_cvtsepi16_epi8(acc);
            }
        }
#elif defined(__AVX2__)
        {
            simde__m128i *buf    = (simde__m128i *) &bnProcBuf[bnStart];
            simde__m128i *llr    = (simde__m128i *) &llrProcBuf[llrStart];
            simde__m256i *res    = (simde__m256i *) &llrRes[llrStart];
            uint32_t off128 = (numBN * NR_LDPC_ZMAX) >> 4;
            uint32_t M      = (numBN * Z + 31) >> 5;
            for (uint32_t i = 0, j = 0; i < M; i++, j += 2) {
                simde__m256i res0 = simde_mm256_cvtepi8_epi16(buf[j]);
                simde__m256i res1 = simde_mm256_cvtepi8_epi16(buf[j + 1]);
                for (uint32_t k = 1; k < numCN; k++) {
                    res0 = simde_mm256_adds_epi16(res0, simde_mm256_cvtepi8_epi16(buf[k * off128 + j]));
                    res1 = simde_mm256_adds_epi16(res1, simde_mm256_cvtepi8_epi16(buf[k * off128 + j + 1]));
                }
                res0 = simde_mm256_adds_epi16(res0, simde_mm256_cvtepi8_epi16(llr[j]));
                res1 = simde_mm256_adds_epi16(res1, simde_mm256_cvtepi8_epi16(llr[j + 1]));
                simde__m256i packed = simde_mm256_packs_epi16(res0, res1);
                res[i] = simde_mm256_permute4x64_epi64(packed, 0xD8);
            }
        }
#else
        {
            simde__m128i *buf = (simde__m128i *) &bnProcBuf[bnStart];
            simde__m128i *llr = (simde__m128i *) &llrProcBuf[llrStart];
            simde__m128i *res = (simde__m128i *) &llrRes[llrStart];
            uint32_t off = (numBN * NR_LDPC_ZMAX) >> 4;
            uint32_t M   = (numBN * Z + 15) >> 4;
            for (uint32_t i = 0; i < M; i++) {
                simde__m128i lo = simde_mm_cvtepi8_epi16(buf[i]);
                simde__m128i hi = simde_mm_cvtepi8_epi16(simde_mm_srli_si128(buf[i], 8));
                for (uint32_t k = 1; k < numCN; k++) {
                    lo = simde_mm_adds_epi16(lo, simde_mm_cvtepi8_epi16(buf[k * off + i]));
                    hi = simde_mm_adds_epi16(hi, simde_mm_cvtepi8_epi16(simde_mm_srli_si128(buf[k * off + i], 8)));
                }
                lo = simde_mm_adds_epi16(lo, simde_mm_cvtepi8_epi16(llr[i]));
                hi = simde_mm_adds_epi16(hi, simde_mm_cvtepi8_epi16(simde_mm_srli_si128(llr[i], 8)));
                res[i] = simde_mm_packs_epi16(lo, hi);
            }
        }
#endif
    }
}

/**
   \brief Performs second part of BN processing on the BN processing buffer and the LLR results buffer and stores the results in the BN processing results buffer.
          At every BN, the LLR of the corresponding edge is subtracted from the sum computed in bnProcPc.
   \param p_lut Pointer to decoder LUTs
   \param Z Lifting size
*/
static inline void nrLDPC_bnProc(t_nrLDPC_lut* p_lut, int8_t* bnProcBuf, int8_t* bnProcBufRes, int8_t* llrRes, uint16_t Z)
{
    const uint8_t*  lut_numBnInBnGroups      = p_lut->numBnInBnGroups;
    const uint32_t* lut_startAddrBnGroups    = p_lut->startAddrBnGroups;
    const uint16_t* lut_startAddrBnGroupsLlr = p_lut->startAddrBnGroupsLlr;
    uint8_t idxBnGroup = 0;

    for (uint32_t grp = 0; grp < NR_LDPC_NUM_BN_GROUPS_BG1_R13; grp++) {
        uint32_t numBN = lut_numBnInBnGroups[grp];
        if (numBN == 0) continue;
        uint32_t numCN    = grp + 1;
        uint32_t bnStart  = lut_startAddrBnGroups[idxBnGroup];
        uint32_t llrStart = lut_startAddrBnGroupsLlr[idxBnGroup];
        idxBnGroup++;

#if defined(__riscv) && defined(__riscv_vector)
        {
            int8_t *buf = &bnProcBuf[bnStart];
            int8_t *res = &bnProcBufRes[bnStart];
            int8_t *llr = &llrRes[llrStart];
            uint32_t off = numBN * NR_LDPC_ZMAX;
            uint32_t numBytes = numBN * Z;
            for (uint32_t k = 0; k < numCN; k++) {
                int8_t *p_res = &res[k * off];
                for (uint32_t i = 0; i < numBytes;) {
                    size_t vl = __riscv_vsetvl_e8m1(numBytes - i);
                    vint8m1_t vllr = __riscv_vle8_v_i8m1(&llr[i], vl);
                    vint8m1_t vbuf = __riscv_vle8_v_i8m1(&buf[k * off + i], vl);
                    __riscv_vse8_v_i8m1(&p_res[i], __riscv_vssub_vv_i8m1(vllr, vbuf, vl), vl);
                    i += vl;
                }
            }
        }
#elif defined(__AVX512BW__)
        {
            simde__m512i *buf = (simde__m512i *) &bnProcBuf[bnStart];
            simde__m512i *res = (simde__m512i *) &bnProcBufRes[bnStart];
            simde__m512i *llr = (simde__m512i *) &llrRes[llrStart];
            uint32_t off = (numBN * NR_LDPC_ZMAX) >> 6;
            uint32_t M   = (numBN * Z + 63) >> 6;
            for (uint32_t k = 0; k < numCN; k++) {
                simde__m512i *p_res = &res[k * off];
                for (uint32_t i = 0; i < M; i++)
                    p_res[i] = simde_mm512_subs_epi8(llr[i], buf[k * off + i]);
            }
        }
#elif defined(__AVX2__)
        {
            simde__m256i *buf = (simde__m256i *) &bnProcBuf[bnStart];
            simde__m256i *res = (simde__m256i *) &bnProcBufRes[bnStart];
            simde__m256i *llr = (simde__m256i *) &llrRes[llrStart];
            uint32_t off = (numBN * NR_LDPC_ZMAX) >> 5;
            uint32_t M   = (numBN * Z + 31) >> 5;
            for (uint32_t k = 0; k < numCN; k++) {
                simde__m256i *p_res = &res[k * off];
                for (uint32_t i = 0; i < M; i++)
                    p_res[i] = simde_mm256_subs_epi8(llr[i], buf[k * off + i]);
            }
        }
#else
        {
            simde__m128i *buf = (simde__m128i *) &bnProcBuf[bnStart];
            simde__m128i *res = (simde__m128i *) &bnProcBufRes[bnStart];
            simde__m128i *llr = (simde__m128i *) &llrRes[llrStart];
            uint32_t off = (numBN * NR_LDPC_ZMAX) >> 4;
            uint32_t M   = (numBN * Z + 15) >> 4;
            for (uint32_t k = 0; k < numCN; k++) {
                simde__m128i *p_res = &res[k * off];
                for (uint32_t i = 0; i < M; i++)
                    p_res[i] = simde_mm_subs_epi8(llr[i], buf[k * off + i]);
            }
        }
#endif
    }
}

/**
   \brief Performs hard-decision on output LLRs, one bit per byte.
   \param out   Output buffer (one uint8 per bit)
   \param llrOut Input LLR buffer
   \param numLLR Number of LLRs
*/
static inline void nrLDPC_llr2bit(uint8_t* out, int8_t* llrOut, uint16_t numLLR)
{
    simde__m256i* p_llrOut = (simde__m256i*) llrOut;
    simde__m256i* p_out    = (simde__m256i*) out;
    const uint32_t M  = numLLR >> 5;
    const uint32_t Mr = numLLR & 31;
    const simde__m256i* p_zeros = (simde__m256i*) zeros256_epi8;
    const simde__m256i* p_ones  = (simde__m256i*) ones256_epi8;

    for (uint32_t i = 0; i < M; i++) {
        *p_out++ = simde_mm256_and_si256(*p_ones, simde_mm256_cmpgt_epi8(*p_zeros, *p_llrOut));
        p_llrOut++;
    }
    int8_t* p_llrOut8 = (int8_t*) p_llrOut;
    uint8_t* p_out8   = (uint8_t*) p_out;
    for (uint32_t i = 0; i < Mr; i++)
        p_out8[i] = p_llrOut8[i] < 0;
}

/**
   \brief Performs hard-decision on output LLRs and packs the output byte-aligned per TS 38.321 Section 6.1.1.
   OUT byte i: bit7=a[8i], bit6=a[8i+1], ..., bit0=a[8i+7]
   \param out   Output buffer (packed bits)
   \param llrOut Input LLR buffer
   \param numLLR Number of LLRs
*/
static inline void nrLDPC_llr2bitPacked(uint8_t* out, int8_t* llrOut, uint16_t numLLR)
{
    const uint8_t constShuffle[32] __attribute__((aligned(32))) =
        {7,6,5,4,3,2,1,0, 15,14,13,12,11,10,9,8, 7,6,5,4,3,2,1,0, 15,14,13,12,11,10,9,8};
    const simde__m256i* p_shuffle = (simde__m256i*) constShuffle;
    simde__m256i* p_llrOut = (simde__m256i*) llrOut;
    uint32_t* p_bits = (uint32_t*) out;
    const uint32_t M  = numLLR >> 5;
    const uint32_t Mr = numLLR & 31;

    for (uint32_t i = 0; i < M; i++) {
        const simde__m256i inPerm = simde_mm256_shuffle_epi8(*p_llrOut, *p_shuffle);
        *p_bits++ = simde_mm256_movemask_epi8(inPerm);
        p_llrOut++;
    }
    if (Mr) {
        const int8_t* p_llrOut8 = (int8_t*) p_llrOut;
        uint32_t bitsTmp = 0;
        for (uint32_t i = 0; i < Mr; i++)
            bitsTmp |= (uint32_t)(p_llrOut8[i] < 0) << ((7 - i) + (16 * (i / 8)));
        *p_bits = bitsTmp;
    }
}

#endif
