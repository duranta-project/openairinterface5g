/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "PHY/CODING/nrSmallBlock/nr_small_block_defs.h"
#include "assertions.h"
#include "PHY/sse_intrin.h"
#if defined(__AVX512F__)
#include <simde/x86/avx512.h>
// simde current version missed this instruction
#define simde_mm512_reduce_add_epi32 _mm512_reduce_add_epi32
#define simde_mm512_cvtepi8_epi32 _mm512_cvtepi8_epi32
#endif

//#define DEBUG_DECODESMALLBLOCK

// Decode 1 or 2 UCI bits using soft LLR combining
uint16_t decode_1_2_uci_bit(int8_t *input, int n_bits, int Qm)
{
  if (n_bits == 1) {
    // Table 5.3.3.1-1 of 38.212
    // Each modulation symbol carries 1 info bit (c0) at n=0
    // all the rest are placeholders
    // we can exploit y placeholders since in that case b(i) = b(i − 1)
    // [c0 y x ...]
    int decision_metric = input[0] + (Qm > 1 ? input[1] : 0);
    // TODO use x placeholder bits stands for b(i) = 1 to increase reliability
    return decision_metric < 0;
  } else {
    AssertFatal(n_bits == 2, "Invalid number of bits %d. It should be 1 or 2!\n", n_bits);
    // Table 5.3.3.1-2 of 38.212
    // c2 = (c0 + c1 ) % 2
    int c0 = input[0], c1 = input[1];
    int  c2 = Qm == 1 ? input[2] : input[Qm];
    if (Qm > 1) {
      c0 += input[Qm + 1];
      c1 += input[Qm * 2];
      c2 += input[Qm * 2 + 1];
    }
    // Positive LLR means bit '0', Negative LLR means bit '1'
    int32_t score00 =  c0 + c1 + c2; // c0=0, c1=0 -> c2=0
    int32_t score10 =  c0 - c1 - c2; // c0=0, c1=1 -> c2=1
    int32_t score01 = -c0 + c1 - c2; // c0=1, c1=0 -> c2=1
    int32_t score11 = -c0 - c1 + c2; // c0=1, c1=1 -> c2=0
    int32_t maxscore = score00;
    uint16_t result = 0; // 00
    if (score01 > maxscore) {
      result = 1; // 01
      maxscore = score01;
    }
    if (score10 > maxscore) {
      result = 2; // 10
      maxscore = score10;
    }
    if (score11 > maxscore)
      result = 3; // 11
    // TODO use x placeholder bits stands for b(i) = 1 to increase reliability
    return result;
  }
}

uint16_t decodeSmallBlock(int8_t *in, uint8_t len)
{
  uint16_t out = 0;
  AssertFatal(len >= 3 && len <= 11,
              "[decodeSmallBlock] Message Length = %d (Small Block Coding is only defined for input lengths 3 to 11)",
              len);

  if(len < 7) {
    int16_t Rhat[NR_SMALL_BLOCK_CODED_BITS] = {0}, Rhatabs[NR_SMALL_BLOCK_CODED_BITS] = {0};
    uint8_t jmax = (1 << (len - 1));
    for (int j = 0; j < jmax; ++j) {
      for (int k = 0; k < NR_SMALL_BLOCK_CODED_BITS; ++k)
        Rhat[j] += in[k] * hadamard32InterleavedTransposed[j][k];
    }
    for (int i = 0; i < NR_SMALL_BLOCK_CODED_BITS; i += 16) {
      simde__m256i a15_a0 = simde_mm256_loadu_si256((simde__m256i*)&Rhat[i]);
      a15_a0 = simde_mm256_abs_epi16(a15_a0);
      simde_mm256_storeu_si256((simde__m256i*)(&Rhatabs[i]), a15_a0);
    }
    uint16_t maxVal = Rhatabs[0];
    uint8_t maxInd = 0;
    for (int k = 1; k < jmax; ++k) {
      if (Rhatabs[k] > maxVal) {
        maxVal = Rhatabs[k];
        maxInd = k;
      }
    }
    out = properOrderedBasis[maxInd] | ( (Rhat[maxInd] > 0) ? (uint16_t)0 : (uint16_t)1);

#ifdef DEBUG_DECODESMALLBLOCK
    for (int k = 0; k < jmax; ++k)
      printf("[decodeSmallBlock]Rhat[%d]=%d %d %d %d\n", k, Rhat[k], maxVal, maxInd, ((uint32_t)out >> k) & 1);
    printf("[decodeSmallBlock]0x%x 0x%x\n", out, properOrderedBasis[maxInd]);
#endif

  } else {
    uint8_t maxRow = 0, maxCol = 0;
    int16_t maxVal = 0;
    int DmatrixElementVal = 0;
#if !defined(__AVX512F__)
    int8_t DmatrixElement[NR_SMALL_BLOCK_CODED_BITS] = {0};
#endif
    simde__m256i _in_256 = simde_mm256_loadu_si256 ((simde__m256i*)&in[0]);
    simde__m256i _maskD_256, _Dmatrixj_256, _maskH_256, _DmatrixElement_256;
    for (int j = 0; j < ( 1<<(len-6) ); ++j) {
      _maskD_256 = simde_mm256_loadu_si256 ((simde__m256i*)(&maskD[j][0]));
      _Dmatrixj_256 = simde_mm256_sign_epi8 (_in_256, _maskD_256);
      for (int k = 0; k < NR_SMALL_BLOCK_CODED_BITS; ++k) {
        _maskH_256 = simde_mm256_loadu_si256 ((simde__m256i*)(&hadamard32InterleavedTransposed[k][0]));
        _DmatrixElement_256 = simde_mm256_sign_epi8 (_Dmatrixj_256, _maskH_256);
#if defined(__AVX512F__)
        DmatrixElementVal = simde_mm512_reduce_add_epi32(
            simde_mm512_add_epi32(simde_mm512_cvtepi8_epi32(simde_mm256_extracti128_si256(_DmatrixElement_256, 1)),
                                  simde_mm512_cvtepi8_epi32(simde_mm256_castsi256_si128(_DmatrixElement_256))));
#else
        simde_mm256_storeu_si256((simde__m256i*)(&DmatrixElement[0]), _DmatrixElement_256);
        for (int i = 0; i < NR_SMALL_BLOCK_CODED_BITS; ++i)
          DmatrixElementVal += DmatrixElement[i];
#endif
        if (abs(DmatrixElementVal) > abs(maxVal)) {
          maxVal = DmatrixElementVal;
          maxRow = j;
          maxCol = k;
        }
        DmatrixElementVal = 0;
      }
    }
    out = properOrderedBasisExtended[maxRow] | properOrderedBasis[maxCol] | ( (maxVal > 0) ? (uint16_t)0 : (uint16_t)1 );
#ifdef DEBUG_DECODESMALLBLOCK
    for (int k = 0; k < NR_SMALL_BLOCK_CODED_BITS; ++k)
      printf("[decodeSmallBlock]maxRow = %d maxCol = %d out[%d]=%d\n", maxRow, maxCol, k, ((uint32_t)out >> k) & 1);
#endif
  }
  return out;
}
