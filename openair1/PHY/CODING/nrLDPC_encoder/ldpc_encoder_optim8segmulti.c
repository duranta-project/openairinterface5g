/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*! 
 * \brief Defines the optimized LDPC encoder
 */

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "assertions.h"
#include "common/utils/LOG/log.h"
#include "time_meas.h"
#include "openair1/PHY/CODING/nrLDPC_defs.h"
#include "PHY/sse_intrin.h"
#include "openair1/PHY/CODING/nrLDPC_extern.h"

#include "ldpc_encode_parity_check.c"
#include "ldpc_generate_coefficient.c"

int LDPCencoder(uint8_t **input, uint8_t *output, encoder_implemparams_t *impp)
{
  //set_log(PHY, 4);

  int Zc = impp->Zc;
  int Kb = impp->Kb;
  short block_length = impp->K;
  short BG = impp->BG;

  int nrows=0,ncols=0;
  int rate=3;
  int no_punctured_columns,removed_bit;
  //Table of possible lifting sizes
  char temp;
  int simd_size;
  unsigned int macro_segment, macro_segment_end;

  
  macro_segment = impp->first_seg;
  macro_segment_end = (impp->n_segments > impp->first_seg + 8) ? impp->first_seg + 8 : impp->n_segments;
  ///printf("macro_segment: %d\n", macro_segment);
  ///printf("macro_segment_end: %d\n", macro_segment_end );

  //determine number of bits in codeword
  if (BG==1)
    {
      nrows=46; //parity check bits
      ncols=22; //info bits
      rate=3;
    }
    else if (BG==2)
    {
      nrows=42; //parity check bits
      ncols=10; // info bits
      rate=5;
    }

#ifdef DEBUG_LDPC
  LOG_D(PHY,"ldpc_encoder_optim_8seg: BG %d, Zc %d, Kb %d, block_length %d, segments %d\n",BG,Zc,Kb,block_length,impp->n_segments);
  LOG_D(PHY,"ldpc_encoder_optim_8seg: PDU (seg 0) %x %x %x %x\n",input[0][0],input[0][1],input[0][2],input[0][3]);
#endif

  AssertFatal(Zc > 0, "no valid Zc found for block length %d\n", block_length);
  if ((Zc&31) > 0) simd_size = 16;
#ifdef __AVX512F__
  else if ((BG==1) && Zc==384) simd_size =64;
#endif
  else simd_size=32;
  unsigned char cc[22*Zc] __attribute__((aligned(64))); //padded input, unpacked, max size
  unsigned char dd[46*Zc] __attribute__((aligned(64))); //coded parity part output, unpacked, max size

  // calculate number of punctured bits
  no_punctured_columns=(int)((nrows-2)*Zc+block_length-block_length*rate)/Zc;
  removed_bit=(nrows-no_punctured_columns-2) * Zc+block_length-(int)(block_length*rate);
  //printf("%d\n",no_punctured_columns);
  //printf("%d\n",removed_bit);
  // unpack input
  memset(cc,0,sizeof(cc));
  memset(dd,0,sizeof(dd));

  if(impp->tinput != NULL) start_meas(impp->tinput);
  //interleave up to 8 transport-block segements at a time

  unsigned int i_byte = 0;

#if defined(__AVX512F__) && defined(__AVX512BW__) && defined(__AVX512VBMI__)
  const __m512i masks5[8] = { _mm512_set1_epi8(0x1), _mm512_set1_epi8(0x2),
                              _mm512_set1_epi8(0x4), _mm512_set1_epi8(0x8),
                              _mm512_set1_epi8(0x10), _mm512_set1_epi8(0x20),
                              _mm512_set1_epi8(0x40), _mm512_set1_epi8(0x80)};
  const __m512i zero512 = _mm512_setzero_si512();
  const uint8_t perm[64]__attribute__((aligned(64))) = {7,6,5,4,3,2,1,0,         15,14,13,12,11,10,9,8,
                                                        23,22,21,20,19,18,17,16, 31,30,29,28,27,26,25,24,
                                                        39,38,37,36,35,34,33,32, 47,46,45,44,43,42,41,40,
                                                        55,54,53,52,51,50,49,48, 63,62,61,60,59,58,57,56};
  register __m512i c512;

  for (; i_byte < ((block_length >> 6) << 6); i_byte += 64) {
    unsigned int i = i_byte >> 6;
    c512 = _mm512_mask_blend_epi8(((uint64_t *)&input[macro_segment][0])[i], zero512, masks5[0]);
    for (int j = macro_segment + 1; j < macro_segment_end; j++) {
      c512 = _mm512_or_si512(c512, _mm512_mask_blend_epi8(((uint64_t *)&input[j][0])[i], zero512, masks5[j - macro_segment]));
    }
    c512 = _mm512_permutexvar_epi8(*(__m512i*)perm, c512);
    ((__m512i *)cc)[i] = c512;
  }
#endif

#ifndef __aarch64__
  simde__m256i shufmask = simde_mm256_set_epi64x(0x0303030303030303, 0x0202020202020202,0x0101010101010101, 0x0000000000000000);
  simde__m256i andmask  = simde_mm256_set1_epi64x(0x0102040810204080);  // every 8 bits -> 8 bytes, pattern repeats.
  simde__m256i zero256   = simde_mm256_setzero_si256();
  simde__m256i masks[8];
  register simde__m256i c256;
  masks[0] = simde_mm256_set1_epi8(0x1);
  masks[1] = simde_mm256_set1_epi8(0x2);
  masks[2] = simde_mm256_set1_epi8(0x4);
  masks[3] = simde_mm256_set1_epi8(0x8);
  masks[4] = simde_mm256_set1_epi8(0x10);
  masks[5] = simde_mm256_set1_epi8(0x20);
  masks[6] = simde_mm256_set1_epi8(0x40);
  masks[7] = simde_mm256_set1_epi8(0x80);

  for (; i_byte < ((block_length >> 5 ) << 5); i_byte += 32) {
    unsigned int i = i_byte >> 5;
    c256 = simde_mm256_and_si256(simde_mm256_cmpeq_epi8(simde_mm256_andnot_si256(simde_mm256_shuffle_epi8(simde_mm256_set1_epi32(((uint32_t*)input[macro_segment])[i]), shufmask),andmask),zero256),masks[0]);
    for (int j=macro_segment+1; j < macro_segment_end; j++) {    
      c256 = simde_mm256_or_si256(simde_mm256_and_si256(simde_mm256_cmpeq_epi8(simde_mm256_andnot_si256(simde_mm256_shuffle_epi8(simde_mm256_set1_epi32(((uint32_t*)input[j])[i]), shufmask),andmask),zero256),masks[j-macro_segment]),c256);
    }
    ((simde__m256i *)cc)[i] = c256;
  }
#endif

#ifdef __aarch64__
  // Direct NEON: broadcast 1 byte to all 8 lanes (ldr-b + dup), VTST bit-test mask.
  // ns=8 fast path: fully unrolled, 2-level asm-volatile barrier OR-tree, no j-loop
  // overhead.  Barrier 1 forces 4 independent level-1 ORRs; GCC fills the level-2
  // ORR latency gap with scalar loop work, letting A72's OoO engine start the next
  // iteration's address computation while ORRs are still in flight.
#include <arm_neon.h>
  static const uint8_t _amask[16] __attribute__((aligned(16))) =
    {0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01,
     0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01};
  const uint8x16_t amask_v = vld1q_u8(_amask);
  uint8x16_t segmasks[8];
  for (int k = 0; k < 8; k++) segmasks[k] = vdupq_n_u8(1u << k);

#define CONTRIB(p, bi, k) \
  vandq_u8(vtstq_u8(vcombine_u8(vld1_dup_u8((p) + (bi)), \
                                  vld1_dup_u8((p) + (bi) + 1)), amask_v), segmasks[k])

  int ns = (int)(macro_segment_end - macro_segment);
  if (ns == 8) {
    // Pre-extract segment base pointers so GCC holds them in registers across the loop.
    const uint8_t *p0 = input[macro_segment + 0];
    const uint8_t *p1 = input[macro_segment + 1];
    const uint8_t *p2 = input[macro_segment + 2];
    const uint8_t *p3 = input[macro_segment + 3];
    const uint8_t *p4 = input[macro_segment + 4];
    const uint8_t *p5 = input[macro_segment + 5];
    const uint8_t *p6 = input[macro_segment + 6];
    const uint8_t *p7 = input[macro_segment + 7];
    for (; i_byte < ((block_length >> 4) << 4); i_byte += 16) {
      unsigned int bi = i_byte >> 3;
      uint8x16_t r0 = CONTRIB(p0, bi, 0);
      uint8x16_t r1 = CONTRIB(p1, bi, 1);
      uint8x16_t r2 = CONTRIB(p2, bi, 2);
      uint8x16_t r3 = CONTRIB(p3, bi, 3);
      uint8x16_t r4 = CONTRIB(p4, bi, 4);
      uint8x16_t r5 = CONTRIB(p5, bi, 5);
      uint8x16_t r6 = CONTRIB(p6, bi, 6);
      uint8x16_t r7 = CONTRIB(p7, bi, 7);
      // Barrier 1: force all 8 contributions into physical NEON registers before
      // any ORR starts.  Without this GCC greedily issues each ORR as soon as ONE
      // input is ready, producing a 7-step serial left-fold (28-cycle critical path).
      asm volatile ("" : "+w"(r0), "+w"(r1), "+w"(r2), "+w"(r3),
                         "+w"(r4), "+w"(r5), "+w"(r6), "+w"(r7));
      // Level-1 ORRs: all 4 are data-independent; A72's 2-wide NEON issues pairs/cycle.
      uint8x16_t r01 = vorrq_u8(r0, r1);
      uint8x16_t r23 = vorrq_u8(r2, r3);
      uint8x16_t r45 = vorrq_u8(r4, r5);
      uint8x16_t r67 = vorrq_u8(r6, r7);
      // Barrier 2: force all level-1 results into registers before level-2.
      // GCC's scheduler then fills the level-2 ORR latency gap with the loop's
      // scalar work (add i_byte, cmp), placing those instructions between the
      // level-2 and level-3/4 ORRs.  This lets the A72's OoO engine start the
      // next iteration's address computation (lsr bi = i_byte>>3) ~4 cycles earlier
      // than if the scalar work came after all ORRs, hiding load latency for free.
      asm volatile ("" : "+w"(r01), "+w"(r23), "+w"(r45), "+w"(r67));
      vst1q_u8(cc + i_byte, vorrq_u8(vorrq_u8(r01, r23), vorrq_u8(r45, r67)));
    }
  } else {
    uint8x16_t c128_v, vv;
    for (; i_byte < ((block_length >> 4) << 4); i_byte += 16) {
      unsigned int bi = i_byte >> 3;
      vv = vcombine_u8(vld1_dup_u8(input[macro_segment] + bi),
                       vld1_dup_u8(input[macro_segment] + bi + 1));
      c128_v = vandq_u8(vtstq_u8(vv, amask_v), segmasks[0]);
      for (int j = macro_segment + 1; j < macro_segment_end; j++) {
        vv = vcombine_u8(vld1_dup_u8(input[j] + bi), vld1_dup_u8(input[j] + bi + 1));
        c128_v = vorrq_u8(vandq_u8(vtstq_u8(vv, amask_v), segmasks[j - macro_segment]), c128_v);
      }
      vst1q_u8(cc + i_byte, c128_v);
    }
  }
#undef CONTRIB
#endif

  for (; i_byte < block_length; i_byte++) {
    unsigned int i = i_byte;
    for (int j = macro_segment; j < macro_segment_end; j++) {

      temp = (input[j][i/8]&(128>>(i&7)))>>(7-(i&7));
      cc[i] |= (temp << (j-macro_segment));
    }
  }

  if(impp->tinput != NULL) stop_meas(impp->tinput);

  if ((BG == 1 && Zc >= 176) || (BG == 2 && Zc >= 72)) {
    //parity check part
    if(impp->tparity != NULL) start_meas(impp->tparity);
    encode_parity_check_part_optim(cc, dd, BG, Zc, simd_size, ncols, impp->tinput_memcpy);
    if(impp->tparity != NULL) stop_meas(impp->tparity);
  } else {
    if (encode_parity_check_part_orig(cc, dd, BG, Zc, Kb, block_length)!=0) {
      printf("Problem with encoder\n");
      return(-1);
    }
  }
  if (impp->toutput != NULL)
    start_meas(impp->toutput);
  memcpy(output,&cc[2*Zc],(block_length-(2*Zc)));
  memcpy(output+block_length-(2*Zc),dd,((nrows-no_punctured_columns) * Zc-removed_bit));
  if (impp->toutput != NULL)
    stop_meas(impp->toutput);
  return 0;
}

