/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "PHY/sse_intrin.h"
#include "nr_rate_matching.h"
#include "common/utils/LOG/log.h"

// #define RM_DEBUG 1
#define USE128BIT
 
static const uint8_t index_k0[2][4] = {{0, 17, 33, 56}, {0, 13, 25, 43}};

void nr_interleaving_ldpc(uint32_t E, uint8_t Qm, uint8_t *e, uint8_t *f)
{
  const uint32_t EQm = E / Qm;
  memset(f, 0, E * sizeof(uint8_t));
  switch(Qm) {
    case 2: {
      uint8_t *e0 = e;
      uint8_t *e1 = e0 + EQm;
      int i = 0;
#ifdef __AVX512VBMI__
      // clang-format off
      const __m512i p8a = _mm512_set_epi8(95, 31, 94, 30, 93, 29, 92, 28, 91, 27, 90, 26, 89, 25, 88, 24,
                                          87, 23, 86, 22, 85, 21, 84, 20, 83, 19, 82, 18, 81, 17, 80, 16,
                                          79, 15, 78, 14, 77, 13, 76, 12, 75, 11, 74, 10, 73, 9, 72, 8,
                                          71, 7, 70, 6, 69, 5, 68, 4, 67, 3, 66, 2, 65, 1, 64, 0);
      const __m512i p8b = _mm512_set_epi8(127, 63, 126, 62, 125, 61, 124, 60, 123, 59, 122, 58, 121, 57, 120,
                                          56, 119, 55, 118, 54, 117, 53, 116, 52, 115, 51, 114, 50, 113, 49, 112,
                                          48, 111, 47, 110, 46, 109, 45, 108, 44, 107, 43, 106, 42, 105, 41, 104,
                                          40, 103, 39, 102, 38, 101, 37, 100, 36, 99, 35, 98, 34, 97, 33, 96, 32);
      // clang-format on
      __m512i *f_512 = (__m512i *)f;
      __m512i *e0_512 = (__m512i *)e0;
      __m512i *e1_512 = (__m512i *)e1;
      for (; i < (EQm & ~63); i += 64) {
        __m512i e0j = _mm512_loadu_si512(e0_512++);
        __m512i e1j = _mm512_loadu_si512(e1_512++);
        // e0(i) e1(i) e0(i+1) e1(i+1) .... e0(i+15) e1(i+15)
        _mm512_storeu_si512(f_512++, _mm512_permutex2var_epi8(e0j, p8a, e1j));
        _mm512_storeu_si512(f_512++, _mm512_permutex2var_epi8(e0j, p8b, e1j));
      }
      e0 = (uint8_t *)e0_512;
      e1 = (uint8_t *)e1_512;
      f = (uint8_t *)f_512;
#endif
#ifdef USE128BIT
      simde__m128i *f_128 = (simde__m128i *)f;
      simde__m128i *e0_128 = (simde__m128i *)e0;
      simde__m128i *e1_128 = (simde__m128i *)e1;
      for (; i < (EQm & ~15); i += 16) {
        simde__m128i e0j = simde_mm_loadu_si128(e0_128++);
        simde__m128i e1j = simde_mm_loadu_si128(e1_128++);
        simde_mm_storeu_si128(f_128++, simde_mm_unpacklo_epi8(e0j, e1j));
        simde_mm_storeu_si128(f_128++, simde_mm_unpackhi_epi8(e0j, e1j));
      }
      e0 = (uint8_t *)e0_128;
      e1 = (uint8_t *)e1_128;
      f = (uint8_t *)f_128;
#endif
      for (; i < EQm; i++) {
        *f++ = *e0++;
        *f++ = *e1++;
      }
    } break;
    case 4: {
      uint8_t *e0 = e;
      uint8_t *e1 = e0 + EQm;
      uint8_t *e2 = e1 + EQm;
      uint8_t *e3 = e2 + EQm;
      int i = 0;
#ifdef __AVX512VBMI__
      // clang-format off
      const __m512i p8a = _mm512_set_epi8(95, 31, 94, 30, 93, 29, 92, 28, 91, 27, 90, 26, 89, 25, 88, 24, 87, 23, 86,
                                          22, 85, 21, 84, 20, 83, 19, 82, 18, 81, 17, 80, 16, 79, 15, 78, 14, 77, 13,
                                          76, 12, 75, 11, 74, 10, 73, 9, 72, 8, 71, 7, 70, 6, 69, 5, 68, 4, 67, 3, 66, 2, 65, 1, 64, 0);
      const __m512i p8b = _mm512_set_epi8(127, 63, 126, 62, 125, 61, 124, 60, 123, 59, 122, 58, 121, 57, 120, 56, 119,
                                          55, 118, 54, 117, 53, 116, 52, 115, 51, 114, 50, 113, 49, 112, 48, 111, 47,
                                          110, 46, 109, 45, 108, 44, 107, 43, 106, 42, 105, 41, 104, 40, 103, 39, 102,
                                          38, 101, 37, 100, 36, 99, 35, 98, 34, 97, 33, 96, 32);
      const __m512i p16a = _mm512_set_epi16(47, 15, 46, 14, 45, 13, 44, 12, 43, 11, 42, 10, 41, 9, 40, 8, 39, 7, 38, 6,
                                            37, 5, 36, 4, 35, 3, 34, 2, 33, 1, 32, 0);
      const __m512i p16b = _mm512_set_epi16(63, 31, 62, 30, 61, 29, 60, 28, 59, 27, 58, 26, 57, 25, 56, 24, 55, 23, 54,
                                            22, 53, 21, 52, 20, 51, 19, 50, 18, 49, 17, 48, 16);
      // clang-format on
      __m512i *e0_512 = (__m512i *)e0;
      __m512i *e1_512 = (__m512i *)e1;
      __m512i *e2_512 = (__m512i *)e2;
      __m512i *e3_512 = (__m512i *)e3;
      __m512i *f_512 = (__m512i *)f;
      for (; i < (EQm & ~63); i += 64) {
        __m512i e0j = _mm512_loadu_si512(e0_512++);
        __m512i e1j = _mm512_loadu_si512(e1_512++);
        __m512i e2j = _mm512_loadu_si512(e2_512++);
        __m512i e3j = _mm512_loadu_si512(e3_512++);
        __m512i tmp0 = _mm512_permutex2var_epi8(e0j, p8a, e1j); // e0(i) e1(i) e0(i+1) e1(i+1) .... e0(i+15) e1(i+15)
        __m512i tmp1 = _mm512_permutex2var_epi8(e2j, p8a, e3j); // e2(i) e3(i) e2(i+1) e3(i+1) .... e2(i+15) e3(i+15)
        // e0(i) e1(i) e2(i) e3(i) ... e0(i+7) e1(i+7) e2(i+7) e3(i+7)
        _mm512_storeu_si512(f_512++, _mm512_permutex2var_epi16(tmp0, p16a, tmp1));
        _mm512_storeu_si512(f_512++, _mm512_permutex2var_epi16(tmp0, p16b, tmp1));
        tmp0 = _mm512_permutex2var_epi8(e0j, p8b, e1j); // e0(i) e1(i) e0(i+1) e1(i+1) .... e0(i+15) e1(i+15)
        tmp1 = _mm512_permutex2var_epi8(e2j, p8b, e3j); // e2(i) e3(i) e2(i+1) e3(i+1) .... e2(i+15) e3(i+15)
        // e0(i) e1(i) e2(i) e3(i) ... e0(i+7) e1(i+7) e2(i+7) e3(i+7)
        _mm512_storeu_si512(f_512++, _mm512_permutex2var_epi16(tmp0, p16a, tmp1));
        _mm512_storeu_si512(f_512++, _mm512_permutex2var_epi16(tmp0, p16b, tmp1));
      }
      e0 = (uint8_t *)e0_512;
      e1 = (uint8_t *)e1_512;
      e2 = (uint8_t *)e2_512;
      e3 = (uint8_t *)e3_512;
      f = (uint8_t *)f_512;
#endif
#ifdef USE128BIT
      simde__m128i *e0_128 = (simde__m128i *)e0;
      simde__m128i *e1_128 = (simde__m128i *)e1;
      simde__m128i *e2_128 = (simde__m128i *)e2;
      simde__m128i *e3_128 = (simde__m128i *)e3;
      simde__m128i *f_128 = (simde__m128i *)f;
      for (; i < (EQm & ~15); i += 16) {
        simde__m128i e0j = simde_mm_loadu_si128(e0_128++);
        simde__m128i e1j = simde_mm_loadu_si128(e1_128++);
        simde__m128i e2j = simde_mm_loadu_si128(e2_128++);
        simde__m128i e3j = simde_mm_loadu_si128(e3_128++);
        simde__m128i tmp0 = simde_mm_unpacklo_epi8(e0j, e1j); // e0(i) e1(i) e0(i+1) e1(i+1) .... e0(i+7) e1(i+7)
        simde__m128i tmp1 = simde_mm_unpacklo_epi8(e2j, e3j); // e2(i) e3(i) e2(i+1) e3(i+1) .... e2(i+7) e3(i+7)
        simde_mm_storeu_si128(f_128++,
                              simde_mm_unpacklo_epi16(tmp0, tmp1)); // e0(i) e1(i) e2(i) e3(i) ... e0(i+3) e1(i+3) e2(i+3) e3(i+3)
        simde_mm_storeu_si128(
            f_128++,
            simde_mm_unpackhi_epi16(tmp0, tmp1)); // e0(i+4) e1(i+4) e2(i+4) e3(i+4) ... e0(i+7) e1(i+7) e2(i+7) e3(i+7)
        tmp0 = simde_mm_unpackhi_epi8(e0j, e1j); // e0(i+8) e1(i+8) e0(i+9) e1(i+9) .... e0(i+15) e1(i+15)
        tmp1 = simde_mm_unpackhi_epi8(e2j, e3j); // e2(i+8) e3(i+9) e2(i+10) e3(i+10) .... e2(i+31) e3(i+31)
        simde_mm_storeu_si128(f_128++, simde_mm_unpacklo_epi16(tmp0, tmp1));
        simde_mm_storeu_si128(f_128++, simde_mm_unpackhi_epi16(tmp0, tmp1));
      }
      e0 = (uint8_t *)e0_128;
      e1 = (uint8_t *)e1_128;
      e2 = (uint8_t *)e2_128;
      e3 = (uint8_t *)e3_128;
      f = (uint8_t *)f_128;
#endif
      for (; i < EQm; i++) {
        *f++ = *e0++;
        *f++ = *e1++;
        *f++ = *e2++;
        *f++ = *e3++;
      }
    } break;
    case 6: {
      uint8_t *e0 = e;
      uint8_t *e1 = e0 + EQm;
      uint8_t *e2 = e1 + EQm;
      uint8_t *e3 = e2 + EQm;
      uint8_t *e4 = e3 + EQm;
      uint8_t *e5 = e4 + EQm;
      int i = 0;
#ifdef __AVX512VBMI__
      // clang-format off
      const __m512i p8a = _mm512_set_epi8(95, 31, 94, 30, 93, 29, 92, 28, 91, 27, 90, 26, 89, 25, 88, 24, 87, 23,
                                          86, 22, 85, 21, 84, 20, 83, 19, 82, 18, 81, 17, 80, 16, 79, 15, 78, 14,
                                          77, 13, 76, 12, 75, 11, 74, 10, 73, 9, 72, 8, 71, 7, 70, 6, 69, 5, 68, 4, 67, 3, 66, 2, 65, 1, 64, 0);
      const __m512i p8b = _mm512_set_epi8(127, 63, 126, 62, 125, 61, 124, 60, 123, 59, 122, 58, 121, 57, 120, 56, 119,
                                          55, 118, 54, 117, 53, 116, 52, 115, 51, 114, 50, 113, 49, 112, 48, 111, 47, 110,
                                          46, 109, 45, 108, 44, 107, 43, 106, 42, 105, 41, 104, 40, 103, 39, 102, 38, 101, 37, 100, 36, 99, 35, 98, 34, 97, 33, 96, 32);
      const __m512i p16a = _mm512_set_epi16(47, 15, 46, 14, 45, 13, 44, 12, 43, 11, 42, 10, 41, 9, 40, 8, 39, 7, 38, 6, 37, 5, 36, 4, 35, 3, 34, 2, 33, 1, 32, 0);
      const __m512i p16b = _mm512_set_epi16(63, 31, 62, 30, 61, 29, 60, 28, 59, 27, 58, 26, 57, 25, 56, 24, 55, 23, 54, 22, 53, 21, 52, 20, 51, 19, 50, 18, 49, 17, 48, 16);
      const __m512i p16c = _mm512_set_epi16(21, 20, 41, 19, 18, 40, 17, 16, 39, 15, 14, 38, 13, 12, 37, 11, 10, 36, 9, 8, 35, 7, 6, 34, 5, 4, 33, 3, 2, 32, 1, 0);
      const __m512i p16d = _mm512_set_epi16(10, 52, 9, 8, 51, 7, 6, 50, 5, 4, 49, 3, 2, 48, 1, 0, 47, 31, 30, 46, 29, 28, 45, 27, 26, 44, 25, 24, 43, 23, 22, 42);
      const __m512i p16d2 = _mm512_set_epi16(32 + 10, 30, 32 + 9, 32 + 8, 27, 32 + 7, 32 + 6, 24, 32 + 5, 32 + 4, 21, 32 + 3, 32 + 2, 18, 32 + 1, 32 + 0,
                                             15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
      const __m512i p16e = _mm512_set_epi16(63, 31, 30, 62, 29, 28, 61, 27, 26, 60, 25, 24, 59, 23, 22, 58, 21, 20, 57, 19, 18, 56, 17, 16, 55, 15, 14, 54, 13, 12, 53, 11);
      // clang-format on
      __m512i *e0_512 = (__m512i *)e0;
      __m512i *e1_512 = (__m512i *)e1;
      __m512i *e2_512 = (__m512i *)e2;
      __m512i *e3_512 = (__m512i *)e3;
      __m512i *e4_512 = (__m512i *)e4;
      __m512i *e5_512 = (__m512i *)e5;
      __m512i *f_512 = (__m512i *)f;
      for (; i < (EQm & ~63); i += 64) {
        __m512i e0j = _mm512_loadu_si512(e0_512++);
        __m512i e1j = _mm512_loadu_si512(e1_512++);
        __m512i e2j = _mm512_loadu_si512(e2_512++);
        __m512i e3j = _mm512_loadu_si512(e3_512++);
        __m512i e4j = _mm512_loadu_si512(e4_512++);
        __m512i e5j = _mm512_loadu_si512(e5_512++);
        __m512i tmp0 = _mm512_permutex2var_epi8(e0j, p8a, e1j); // e0(i) e1(i) e0(i+1) e1(i+1) .... e0(i+31) e1(i+31)
        __m512i tmp1 = _mm512_permutex2var_epi8(e2j, p8a, e3j); // e2(i) e3(i) e2(i+1) e3(i+1) .... e2(i+31) e3(i+31)
        __m512i tmp2 = _mm512_permutex2var_epi8(e4j, p8a, e5j); // e4(i) e5(i) e4(i+1) e5(i+1) .... e4(i+31) e5(i+31)
        // e0(i) e1(i) e2(i) e3(i) ... e0(i+15) e1(i+15) e2(i+15) e3(i+15)
        __m512i tmp3 = _mm512_permutex2var_epi16(tmp0, p16a, tmp1);
        // e0(i+16) e1(i+16) e2(i+16) e3(i+16) ... e0(i+31) e1(i+31) e2(i+31) e3(i+31)
        __m512i tmp4 = _mm512_permutex2var_epi16(tmp0, p16b, tmp1);
        // e0(i) e1(i) e2(i) e3(i) e4(i) e5(i) ... e0(i+9) e1(i+9) e2(i+9) e3(i+9) e4(i+9) e5(i+9) e0(i+10) e1(i+10)
        _mm512_storeu_si512(f_512++, _mm512_permutex2var_epi16(tmp3, p16c, tmp2));
        // e2(i+10) e3(i+10) e4(i+10) e5(i+10) ... e0(i+15) e1(i+15) e2(i+15) e3(i+15) e4(i+15)
        // e5(i+15) x x x x e4(i+16) e5(i+16) x x x x .... e4(i+20) e5(i+20) x x
        __m512i tmp5 = _mm512_permutex2var_epi16(tmp3, p16d, tmp2);
        // e2(i+10) e3(i+10) e4(i+10) e5(i+10) ... e0(i+15) e1(i+15) e2(i+15) e3(i+15)
        // e4(i+15) e5(i+15) e0(i+16) e1(i+16) e2(i+16) e3(i+16) e4(i+16) e5(i+16)  e0(i+20)
        // e1(i+20) e2(i+20) e3(i+20) e4(i+20) e5(i+20) e0(i+21) e1(i+21)
        _mm512_storeu_si512(f_512++, _mm512_permutex2var_epi16(tmp5, p16d2, tmp4));
        // e2(i+21) e3(i+21) e4(i+21) e5(i+21) .... e0(i+31) e1(i+31) e2(i+31) e3(i+31) e4(i+31) e5(i+31)
        _mm512_storeu_si512(f_512++, _mm512_permutex2var_epi16(tmp4, p16e, tmp2));

        tmp0 = _mm512_permutex2var_epi8(e0j, p8b, e1j); // e0(i+32) e1(i+32) e0(i+32) e1(i+32) .... e0(i+63) e1(i+63)
        tmp1 = _mm512_permutex2var_epi8(e2j, p8b, e3j); // e2(i+32) e3(i+32) e2(i+32) e3(i+32) .... e2(i+63) e3(i+63)
        tmp2 = _mm512_permutex2var_epi8(e4j, p8b, e5j); // e4(i+32) e5(i+32) e4(i+32) e5(i+32) .... e4(i+63) e5(i+63)
        tmp3 = _mm512_permutex2var_epi16(tmp0, p16a, tmp1); // e0(i) e1(i) e2(i) e3(i) ... e0(i+15) e1(i+15) e2(i+15) e3(i+15)
        // e0(i+16) e1(i+16) e2(i+16) e3(i+16) ... e0(i+31) e1(i+31) e2(i+31) e3(i+31)
        tmp4 = _mm512_permutex2var_epi16(tmp0, p16b, tmp1);
        // e0(i) e1(i) e2(i) e3(i) e4(i) e5(i) ... e0(i+9) e1(i+9) e2(i+9) e3(i+9) e4(i+9) e5(i+9) e0(i+10) e1(i+10)
        _mm512_storeu_si512(f_512++, _mm512_permutex2var_epi16(tmp3, p16c, tmp2));
        // e2(i+10) e3(i+10) e4(i+10) e5(i+10) ... e0(i+15) e1(i+15) e2(i+15) e3(i+15)
        // e4(i+15) e5(i+15) x x x x e4(i+16) e5(i+16) x x x x .... e4(i+20) e5(i+20) x x
        tmp5 = _mm512_permutex2var_epi16(tmp3, p16d, tmp2);
        // e2(i+10) e3(i+10) e4(i+10) e5(i+10) ... e0(i+15) e1(i+15) e2(i+15) e3(i+15)
        // e4(i+15) e5(i+15) e0(i+16) e1(i+16) e2(i+16) e3(i+16) e4(i+16) e5(i+16)  e0(i+20)
        // e1(i+20) e2(i+20) e3(i+20) e4(i+20) e5(i+20) e0(i+21) e1(i+21)
        _mm512_storeu_si512(f_512++, _mm512_permutex2var_epi16(tmp5, p16d2, tmp4));
        // e2(i+21) e3(i+21) e4(i+21) e5(i+21) .... e0(i+31) e1(i+31) e2(i+31) e3(i+31) e4(i+31) e5(i+31)
        _mm512_storeu_si512(f_512++, _mm512_permutex2var_epi16(tmp4, p16e, tmp2));
      }
      e0 = (uint8_t *)e0_512;
      e1 = (uint8_t *)e1_512;
      e2 = (uint8_t *)e2_512;
      e3 = (uint8_t *)e3_512;
      e4 = (uint8_t *)e4_512;
      e5 = (uint8_t *)e5_512;
      f = (uint8_t *)f_512;
#endif
      for (; i < EQm; i++) {
        *f++ = *e0++;
        *f++ = *e1++;
        *f++ = *e2++;
        *f++ = *e3++;
        *f++ = *e4++;
        *f++ = *e5++;
      }
    } break;
    case 8: {
      uint8_t *e0 = e;
      uint8_t *e1 = e0 + EQm;
      uint8_t *e2 = e1 + EQm;
      uint8_t *e3 = e2 + EQm;
      uint8_t *e4 = e3 + EQm;
      uint8_t *e5 = e4 + EQm;
      uint8_t *e6 = e5 + EQm;
      uint8_t *e7 = e6 + EQm;
      
      int i = 0;
#ifdef __AVX512VBMI__
      // clang-format off
      const __m512i p8a = _mm512_set_epi8(95, 31, 94, 30, 93, 29, 92, 28, 91, 27, 90, 26, 89, 25, 88, 24, 87, 23,
                                          86, 22, 85, 21, 84, 20, 83, 19, 82, 18, 81, 17, 80, 16, 79, 15, 78, 14, 77, 13,
                                          76, 12, 75, 11, 74, 10, 73, 9, 72, 8, 71, 7, 70, 6, 69, 5, 68, 4, 67, 3, 66, 2, 65, 1, 64, 0);
      const __m512i p8b = _mm512_set_epi8(127, 63, 126, 62, 125, 61, 124, 60, 123, 59, 122, 58, 121, 57, 120, 56, 119, 55,
                                          118, 54, 117, 53, 116, 52, 115, 51, 114, 50, 113, 49, 112, 48, 111, 47, 110, 46,
                                          109, 45, 108, 44, 107, 43, 106, 42, 105, 41, 104, 40, 103, 39, 102, 38, 101, 37, 100, 36, 99, 35, 98, 34, 97, 33, 96, 32);

      const __m512i p16a = _mm512_set_epi16(47, 15, 46, 14, 45, 13, 44, 12, 43, 11, 42, 10, 41, 9, 40, 8, 39, 7, 38, 6, 37, 5, 36, 4, 35, 3, 34, 2, 33, 1, 32, 0);
      const __m512i p16b = _mm512_set_epi16(63, 31, 62, 30, 61, 29, 60, 28, 59, 27, 58, 26, 57, 25, 56, 24, 55, 23, 54, 22, 53, 21, 52, 20, 51, 19, 50, 18, 49, 17, 48, 16);

      const __m512i p32a = _mm512_set_epi32(23, 7, 22, 6, 21, 5, 20, 4, 19, 3, 18, 2, 17, 1, 16, 0);
      const __m512i p32b = _mm512_set_epi32(31, 15, 30, 14, 29, 13, 28, 12, 27, 11, 26, 10, 25, 9, 24, 8);
      // clang-format on
      __m512i *e0_512 = (__m512i *)e0;
      __m512i *e1_512 = (__m512i *)e1;
      __m512i *e2_512 = (__m512i *)e2;
      __m512i *e3_512 = (__m512i *)e3;
      __m512i *e4_512 = (__m512i *)e4;
      __m512i *e5_512 = (__m512i *)e5;
      __m512i *e6_512 = (__m512i *)e6;
      __m512i *e7_512 = (__m512i *)e7;
      __m512i *f_512 = (__m512i *)f;
      for (; i < (EQm & ~63); i += 64) {
        __m512i e0j = _mm512_loadu_si512(e0_512++);
        __m512i e1j = _mm512_loadu_si512(e1_512++);
        __m512i e2j = _mm512_loadu_si512(e2_512++);
        __m512i e3j = _mm512_loadu_si512(e3_512++);
        __m512i e4j = _mm512_loadu_si512(e4_512++);
        __m512i e5j = _mm512_loadu_si512(e5_512++);
        __m512i e6j = _mm512_loadu_si512(e6_512++);
        __m512i e7j = _mm512_loadu_si512(e7_512++);
        __m512i tmp0 = _mm512_permutex2var_epi8(e0j, p8a, e1j); // e0(i) e1(i) e0(i+1) e1(i+1) .... e0(i+15) e1(i+15)
        __m512i tmp1 = _mm512_permutex2var_epi8(e2j, p8a, e3j); // e2(i) e3(i) e2(i+1) e3(i+1) .... e2(i+15) e3(i+15)
        __m512i tmp2 = _mm512_permutex2var_epi8(e4j, p8a, e5j); // e4(i) e5(i) e4(i+1) e5(i+1) .... e4(i+15) e5(i+15)
        __m512i tmp3 = _mm512_permutex2var_epi8(e6j, p8a, e7j); // e6(i) e7(i) e6(i+1) e7(i+1) .... e6(i+15) e7(i+15)
        __m512i tmp4 = _mm512_permutex2var_epi16(tmp0, p16a, tmp1); // e0(i) e1(i) e2(i) e3(i) ... e0(i+7) e1(i+7) e2(i+7) e3(i+7)
        // e4(i) e5(i) e6(i) e7(i) ... e4(i+7) e5(i+7) e6(i+7) e7(i+7)
        // e0(i) e1(i) e2(i) e3(i) e4(i) e5(i) e6(i) e7(i)... e0(i+3) e1(i+3)
        __m512i tmp5 = _mm512_permutex2var_epi16(tmp2, p16a, tmp3);
        // e2(i+3) e3(i+3) e4(i+3) e5(i+3) e6(i+3) e7(i+3))
        _mm512_storeu_si512(f_512++, _mm512_permutex2var_epi32(tmp4, p32a, tmp5));
        // e0(i+4) e1(i+4) e2(i+4) e3(i+4) e4(i+4) e5(i+4) e6(i+4) e7(i+4)...
        // e0(i+7) e1(i+7) e2(i+7) e3(i+7) e4(i+7) e5(i+7) e6(i+7) e7(i+7))
        _mm512_storeu_si512(f_512++, _mm512_permutex2var_epi32(tmp4, p32b, tmp5));
        // e0(i+8) e1(i+8) e2(i+8) e3(i+8) ... e0(i+15) e1(i+15) e2(i+15) e3(i+15)
        tmp4 = _mm512_permutex2var_epi16(tmp0, p16b, tmp1);
        // e4(i+8) e5(i+8) e6(i+8) e7(i+8) ... e4(i+15) e5(i+15) e6(i+15) e7(i+15)
        tmp5 = _mm512_permutex2var_epi16(tmp2, p16b, tmp3);
        // e0(i+8) e1(i+8) e2(i+8) e3(i+8) e4(i+8) e5(i+8) e6(i+8) e7(i+8)... e0(i+11)
        // e1(i+11) e2(i+11) e3(i+11) e4(i+11) e5(i+11) e6(i+11) e7(i+11))
        _mm512_storeu_si512(f_512++, _mm512_permutex2var_epi32(tmp4, p32a, tmp5));
        // e0(i+12) e1(i+12) e2(i+12) e3(i+12) e4(i+12) e5(i+12) e6(i+12) e7(i+12)... e0(i+15)
        // e1(i+15) e2(i+15) e3(i+15) e4(i+15) e5(i+15) e6(i+15) e7(i+15))
        _mm512_storeu_si512(f_512++, _mm512_permutex2var_epi32(tmp4, p32b, tmp5));

        tmp0 = _mm512_permutex2var_epi8(e0j, p8b, e1j); // e0(i+16) e1(i+16) e0(i+17) e1(i+17) .... e0(i+31) e1(i+31)
        tmp1 = _mm512_permutex2var_epi8(e2j, p8b, e3j); // e2(i+16) e3(i+16) e2(i+17) e3(i+17) .... e2(i+31) e3(i+31)
        tmp2 = _mm512_permutex2var_epi8(e4j, p8b, e5j); // e4(i+16) e5(i+16) e4(i+17) e5(i+17) .... e4(i+31) e5(i+31)
        tmp3 = _mm512_permutex2var_epi8(e6j, p8b, e7j); // e6(i+16) e7(i+16) e6(i+17) e7(i+17) .... e6(i+31) e7(i+31)
        // e0(i+!6) e1(i+16) e2(i+16) e3(i+16) ... e0(i+23) e1(i+23) e2(i+23) e3(i+23)
        tmp4 = _mm512_permutex2var_epi16(tmp0, p16a, tmp1);
        // e4(i+16) e5(i+16) e6(i+16) e7(i+16) ... e4(i+23) e5(i+23) e6(i+23) e7(i+23)
        tmp5 = _mm512_permutex2var_epi16(tmp2, p16a, tmp3);
        // e0(i+16) e1(i+16) e2(i+16) e3(i+16) e4(i+16) e5(i+16) e6(i+16) e7(i+16)... e0(i+19)
        // e1(i+19) e2(i+19) e3(i+19) e4(i+19) e5(i+19) e6(i+19) e7(i+19))
        _mm512_storeu_si512(f_512++, _mm512_permutex2var_epi32(tmp4, p32a, tmp5));
        // e0(i+20) e1(i+20) e2(i+20) e3(i+20) e4(i+20) e5(i+20) e6(i+20) e7(i+20)... e0(i+23)
        // e1(i+23) e2(i+23) e3(i+23) e4(i+23) e5(i+23) e6(i+23) e7(i+23))
        _mm512_storeu_si512(f_512++, _mm512_permutex2var_epi32(tmp4, p32b, tmp5));
        // e0(i+24) e1(i+24) e2(i+24) e3(i+24) ... e0(i+31) e1(i+31) e2(i+31) e3(i+31)
        tmp4 = _mm512_permutex2var_epi16(tmp0, p16b, tmp1);
        // e4(i+24) e5(i+24) e6(i+24) e7(i+24) ... e4(i+31) e5(i+31) e6(i+31) e7(i+31)
        tmp5 = _mm512_permutex2var_epi16(tmp2, p16b, tmp3);
        // e0(i+24) e1(i+24) e2(i+24) e3(i+24) e4(i+24) e5(i+24) e6(i+24) e7(i+24)... e0(i+27)
        // e1(i+27) e2(i+27) e3(i+27) e4(i+27) e5(i+27) e6(i+27) e7(i+27))
        _mm512_storeu_si512(f_512++, _mm512_permutex2var_epi32(tmp4, p32a, tmp5));
        // e0(i+28) e1(i+28) e2(i+28) e3(i+28) e4(i+28) e5(i+28) e6(i+28) e7(i+28)... e0(i+31)
        // e1(i+31) e2(i+31) e3(i+31) e4(i+31) e5(i+31) e6(i+31) e7(i+31))
        _mm512_storeu_si512(f_512++, _mm512_permutex2var_epi32(tmp4, p32b, tmp5));
      }
      e0 = (uint8_t *)e0_512;
      e1 = (uint8_t *)e1_512;
      e2 = (uint8_t *)e2_512;
      e3 = (uint8_t *)e3_512;
      e4 = (uint8_t *)e4_512;
      e5 = (uint8_t *)e5_512;
      e6 = (uint8_t *)e6_512;
      e7 = (uint8_t *)e7_512;
      f = (uint8_t *)f_512;
#endif
#ifdef USE128BIT
      simde__m128i *e0_128 = (simde__m128i *)e0;
      simde__m128i *e1_128 = (simde__m128i *)e1;
      simde__m128i *e2_128 = (simde__m128i *)e2;
      simde__m128i *e3_128 = (simde__m128i *)e3;
      simde__m128i *e4_128 = (simde__m128i *)e4;
      simde__m128i *e5_128 = (simde__m128i *)e5;
      simde__m128i *e6_128 = (simde__m128i *)e6;
      simde__m128i *e7_128 = (simde__m128i *)e7;
      simde__m128i *f_128  = (simde__m128i *)f;
      for (; i < (EQm & ~15); i += 16) {
        simde__m128i e0j = simde_mm_loadu_si128(e0_128++);
        simde__m128i e1j = simde_mm_loadu_si128(e1_128++);
        simde__m128i e2j = simde_mm_loadu_si128(e2_128++);
        simde__m128i e3j = simde_mm_loadu_si128(e3_128++);
        simde__m128i e4j = simde_mm_loadu_si128(e4_128++);
        simde__m128i e5j = simde_mm_loadu_si128(e5_128++);
        simde__m128i e6j = simde_mm_loadu_si128(e6_128++);
        simde__m128i e7j = simde_mm_loadu_si128(e7_128++);
        simde__m128i tmp0 = simde_mm_unpacklo_epi8(e0j, e1j); // e0(i) e1(i) e0(i+1) e1(i+1) .... e0(i+7) e1(i+7)
        simde__m128i tmp1 = simde_mm_unpacklo_epi8(e2j, e3j); // e2(i) e3(i) e2(i+1) e3(i+1) .... e2(i+7) e3(i+7)
        // e0(i) e1(i) e2(i) e3(i) e0(i+1) e1(i+1) e2(i+1) e3(i+1) ... e0(i+3) e1(i+3) e2(i+3) e3(i+3)
        simde__m128i tmp2 = simde_mm_unpacklo_epi16(tmp0, tmp1);
        // e0(i+4) e1(i+4) e2(i+4) e3(i+4) e0(i+5) e1(i+5) e2(i+5) e3(i+5) ... e0(i+7) e1(i+7) e2(i+7) e3(i+7)
        simde__m128i tmp3 = simde_mm_unpackhi_epi16(tmp0, tmp1);
        simde__m128i tmp4 = simde_mm_unpacklo_epi8(e4j, e5j); // e4(i) e5(i) e4(i+1) e5(i+1) .... e4(i+7) e5(i+7)
        simde__m128i tmp5 = simde_mm_unpacklo_epi8(e6j, e7j); // e6(i) e7(i) e6(i+1) e7(i+1) .... e6(i+7) e7(i+7)
        // e4(i) e5(i) e6(i) e7(i) e4(i+1) e5(i+1) e6(i+1) e7(i+1) ... e4(i+3) e5(i+3) e6(i+) e7(i+3)
        simde__m128i tmp6 = simde_mm_unpacklo_epi16(tmp4, tmp5);
        // e4(i+4) e5(i+4) e6(i+4) e7(i+4) e4(i+5) e5(i+5) e6(i+5) e7(i+5) ... e4(i+7) e5(i+7) e6(i+7) e7(i+7)
        simde__m128i tmp7 = simde_mm_unpackhi_epi16(tmp4, tmp5);
        // e0(i) e1(i) e2(i) e3(i) e4(i) e5(i) e6(i) e7(i) e0(i+1) ... e7(i+1)
        simde_mm_storeu_si128(f_128++, simde_mm_unpacklo_epi32(tmp2, tmp6));
        // e0(i+2) e1(i+2) e2(i+2) e3(i+2) e4(i+2) e5(i+2) e6(i+2) e7(i+2) e0(i+3) e1(i+3) ... e7(i+3)
        simde_mm_storeu_si128(f_128++, simde_mm_unpackhi_epi32(tmp2, tmp6));
        // e0(i+4) e1(i+4) e2(i+4) e3(i+4) e4(i+4) e5(i+4) e6(i+4) e7+4(i) e0(i+5) ... e7(i+5)
        simde_mm_storeu_si128(f_128++, simde_mm_unpacklo_epi32(tmp3, tmp7));
        // e0(i+6) e1(i+6) e2(i+6) e3(i+6) e4(i+6) e5(i+6) e6(i+6) e7(i+6) e0(i+7) e0(i+7) ... e7(i+7)
        simde_mm_storeu_si128(f_128++, simde_mm_unpackhi_epi32(tmp3, tmp7));

        tmp0 = simde_mm_unpackhi_epi8(e0j, e1j); // e0(i+8) e1(i+8) e0(i+9) e1(i+9) .... e0(i+15) e1(i+15)
        tmp1 = simde_mm_unpackhi_epi8(e2j, e3j); // e2(i+8) e3(i+8) e2(i+9) e3(i+9) .... e2(i+15) e3(i+15)
        // e0(i) e1(i) e2(i) e3(i) e0(i+1) e1(i+1) e2(i+1) e3(i+1) ... e0(i+3) e1(i+3) e2(i+3) e3(i+3)
        tmp2 = simde_mm_unpacklo_epi16(tmp0, tmp1);
        // e0(i+4) e1(i+4) e2(i+4) e3(i+4) e0(i+5) e1(i+5) e2(i+5) e3(i+5) ... e0(i+7) e1(i+7) e2(i+7) e3(i+7)
        tmp3 = simde_mm_unpackhi_epi16(tmp0, tmp1);
        tmp4 = simde_mm_unpackhi_epi8(e4j, e5j); // e4(i+8) e5(i+8) e4(i+9) e5(i+9) .... e4(i+15) e515i+7)
        tmp5 = simde_mm_unpackhi_epi8(e6j, e7j); // e6(i+8) e7(i+8) e6(i+9) e7(i+9) .... e6(i+15) e7(i+15)
        // e4(i) e5(i) e6(i) e7(i) e4(i+1) e5(i+1) e6(i+1) e7(i+1) ... e4(i+3) e5(i+3) e6(i+) e7(i+3)
        tmp6 = simde_mm_unpacklo_epi16(tmp4, tmp5);
        // e4(i+4) e5(i+4) e6(i+4) e7(i+4) e4(i+5) e5(i+5) e6(i+5) e7(i+5) ... e4(i+7) e5(i+7) e6(i+7) e7(i+7)
        tmp7 = simde_mm_unpackhi_epi16(tmp4, tmp5);
        // e0(i) e1(i) e2(i) e3(i) e4(i) e5(i) e6(i) e7(i) e0(i+1) ... e7(i+1)
        simde_mm_storeu_si128(f_128++, simde_mm_unpacklo_epi32(tmp2, tmp6));
        // e0(i+2) e1(i+2) e2(i+2) e3(i+2) e4(i+2) e5(i+2) e6(i+2) e7(i+2) e0(i+3) e1(i+3) ... e7(i+3)
        simde_mm_storeu_si128(f_128++, simde_mm_unpackhi_epi32(tmp2, tmp6));
        // e0(i+4) e1(i+4) e2(i+4) e3(i+4) e4(i+4) e5(i+4) e6(i+4) e7+4(i) e0(i+5) ... e7(i+5)
        simde_mm_storeu_si128(f_128++, simde_mm_unpacklo_epi32(tmp3, tmp7));
        // e0(i+6) e1(i+6) e2(i+6) e3(i+6) e4(i+6) e5(i+6) e6(i+6) e7(i+6) e0(i+7) e0(i+7) ... e7(i+7)
        simde_mm_storeu_si128(f_128++, simde_mm_unpackhi_epi32(tmp3, tmp7));
      }
      e0 = (uint8_t *)e0_128;
      e1 = (uint8_t *)e1_128;
      e2 = (uint8_t *)e2_128;
      e3 = (uint8_t *)e3_128;
      e4 = (uint8_t *)e4_128;
      e5 = (uint8_t *)e5_128;
      e6 = (uint8_t *)e6_128;
      e7 = (uint8_t *)e7_128;
      f = (uint8_t *)f_128;
#endif
      for (; i < EQm; i++) {
        *f++ = *e0++;
        *f++ = *e1++;
        *f++ = *e2++;
        *f++ = *e3++;
        *f++ = *e4++;
        *f++ = *e5++;
        *f++ = *e6++;
        *f++ = *e7++;
      }
    } break;
    default:
      AssertFatal(false, "Should be here!\n");
  }
}

#if defined(__aarch64__)
static inline uint8x16_t tbl96_u8(uint8x16_t b0, uint8x16_t b1, uint8x16_t b2,
                                  uint8x16_t b3, uint8x16_t b4, uint8x16_t b5,
                                  uint8x16_t idx /* 0..95 */)
{
   // Table 0: bytes 0..63
   uint8x16x4_t T0 = {{ b0, b1, b2, b3 }};
   uint8x16_t r0 = vqtbl4q_u8(T0, idx);

   // Table 1: bytes 64..95, presented as a 64-byte table:
   // bytes 0..31 map to original 64..95, bytes 32..63 are dummy (return 0)
   uint8x16_t z = vdupq_n_u8(0);
   uint8x16x4_t T1 = {{ b4, b5, z, z }};

   // idx1 = idx - 64 (wrap-safe via unsigned subtract); only valid when idx>=64
   uint8x16_t idx1 = vsubq_u8(idx, vdupq_n_u8(64));
   uint8x16_t r1 = vqtbl4q_u8(T1, idx1);

   // Select r1 where idx >= 64, else r0
   uint8x16_t sel = vcgeq_u8(idx, vdupq_n_u8(64));  // 0xFF where idx>=64
   return vbslq_u8(sel, r1, r0);
}

static inline uint8x16_t tbl128_u8(uint8x16_t b0, uint8x16_t b1, uint8x16_t b2, uint8x16_t b3,
	                           uint8x16_t b4, uint8x16_t b5, uint8x16_t b6, uint8x16_t b7,
	                           uint8x16_t idx /* 0..127 */)
{
  // Table low: bytes 0..63
     uint8x16x4_t T0 = {{ b0, b1, b2, b3 }};
     uint8x16_t r0 = vqtbl4q_u8(T0, idx);

 // Table high: bytes 64..127, mapped to 0..63 by subtracting 64
     uint8x16x4_t T1 = {{ b4, b5, b6, b7 }};
     uint8x16_t idx1 = vsubq_u8(idx, vdupq_n_u8(64));
     uint8x16_t r1 = vqtbl4q_u8(T1, idx1);

 // Select high where idx >= 64 else low
     uint8x16_t sel = vcgeq_u8(idx, vdupq_n_u8(64));
     return vbslq_u8(sel, r1, r0);
}
#elif defined(__AVX512BW__)

static inline __m512i idx_stride_u16(int ways, int k)
{
    uint16_t idx[32] __attribute__((aligned(64)));
    for (int n = 0; n < 32; n++) idx[n] = (uint16_t)(k + ways*n);
      return _mm512_load_si512((const void*)idx);
}

// Build indices for k = 0..5, lanes n=0..15: p = k + 6*n
static inline __m512i make_idx_ab(int k) {
    uint16_t idx[32] __attribute__((aligned(64))) = { 0 };
    for (int n = 0; n < 16; n++) {
        idx[n] = (uint16_t)(k + 6*n); // p in [0..95]
    }
    return _mm512_load_si512((const void*)idx);
}

static inline __m512i make_idx_bc(int k) {
    uint16_t idx[32] __attribute__((aligned(64))) = { 0 };

    for (int n = 0; n < 16; n++) {
        int p = k + 6*n;
   // For (B,C) pair, global p maps to:
   //   indices 0..31  -> B lanes 0..31 (global 32..63)
   //   indices 32..63 -> C lanes 0..31 (global 64..95)
   // so use (p - 32) as index into [B|C] when p >= 32.
        idx[n] = (uint16_t)(p - 32);
    }
    return _mm512_load_si512((const void*)idx);
}

static inline __mmask32 make_mask_bc(int k) {
   // lanes needing BC are those with p >= 64 (since AB covers global 0..63)
   __mmask32 m = 0;
   for (int n = 0; n < 16; n++) {
       int p = k + 6*n;
       if (p >= 64) m |= (1u << n);
   }
   return m;
}
#endif

void nr_deinterleaving_ldpc(uint32_t E, uint8_t Qm, int16_t *e, int16_t *f)
{
  const uint32_t EQm = E / Qm;
  switch (Qm) {
    case 2: {
      AssertFatal(E % 2 == 0, "");
      int16_t *e1 = e + EQm;
      int i=0;	  
#if defined(__aarch64__)
      for (; i + 8 <= EQm; i += 8) {
        int16x8x2_t v = vld2q_s16(f);  // 8 groups
        f += 16;
        vst1q_s16(e, v.val[0]); e += 8;
        vst1q_s16(e1, v.val[1]); e1 += 8;
      }
#elif defined(__AVX512BW__)
    const __m512i idx0 = idx_stride_u16(2, 0);
    const __m512i idx1 = idx_stride_u16(2, 1);

    for (; i + 32 <= EQm; i += 32) {
        // 32 groups * 2 u16 = 64 u16 = 2x512b
        __m512i A = _mm512_loadu_si512((const void*)(f +  0)); // u16[0..31]
        __m512i B = _mm512_loadu_si512((const void*)(f + 32)); // u16[32..63]
        f += 64;
        __m512i o0 = _mm512_permutex2var_epi16(A, idx0, B);
        __m512i o1 = _mm512_permutex2var_epi16(A, idx1, B);
        _mm512_storeu_si512((void*)e, o0); e += 32;
        _mm512_storeu_si512((void*)e1, o1); e1 += 32;
    }
#else 
      simde__m128i *e0_128 = (simde__m128i *)e;
      simde__m128i *e1_128 = (simde__m128i *)e1;
      simde__m128i *f128   = (simde__m128i *)f;	  
      const uint8_t shuf4[16]  __attribute__((aligned(16))) = {0,1,4,5,8,9,12,13,2,3,6,7,10,11,14,15};
      
      const simde__m128i *shuf4_128 = (const simde__m128i *)shuf4;
      for (i=0; i < (EQm & ~7); i += 8) {      
            simde__m128i f0j = simde_mm_loadu_si128(f128++); // f0(i) f0(i+1) f0(i+2) f0(i+3) f0(i+4) f0(i+5) f0(i+6) f0(i+7)
            simde__m128i f1j = simde_mm_loadu_si128(f128++); // f1(i) f1(i+1) f1(i+2) f1(i+3) f1(i+4) f1(i+5) f1(i+6) f1(i+7)
    	    simde__m128i tmp0 = simde_mm_shuffle_epi8(f0j,*shuf4_128); // f0(i) f0(i+2) f0(i+4) f0(i+6) f0(i+1) f0(i+3) f0(i+5) f0(i+7)
     	    simde__m128i tmp1 = simde_mm_shuffle_epi8(f1j,*shuf4_128); // f1(i) f1(i+2) f1(i+4) f1(i+6) f1(i+1) f1(i+3) f1(i+5) f1(i+7)
            simde_mm_storeu_si128(e0_128++,simde_mm_unpacklo_epi64(tmp0, tmp1));   // f0(i) f0(i+2) f0(i+4) f0(i+6) f1(i) f1(i+2) f1(i+4) f1(i+6)
            simde_mm_storeu_si128(e1_128++,simde_mm_unpackhi_epi64(tmp0, tmp1));   // f0(i+1) f0(i+3) f0(i+5) f0(i+7) f1(i+1) f1(i+3) f1(i+5) f1(i+7)
      }
      e=(int16_t *)e0_128;
      e1=(int16_t *)e1_128;
      f=(int16_t *)f128;      
#endif
      for (; i < EQm; i++) {
        *e++ = *f++;
        *e1++ = *f++;
      }
    } break;

    case 4: {
      AssertFatal(E % 4 == 0, "");
      int i=0;
      int16_t *e1 = e + EQm;
      int16_t *e2 = e1 + EQm;
      int16_t *e3 = e2 + EQm;

#if defined(__aarch64__)
      for (; i + 8 <= EQm; i += 8) {
        int16x8x4_t v = vld4q_s16(f);  // 8 groups
        f += 32;
        vst1q_s16(e, v.val[0]); e += 8;
        vst1q_s16(e1, v.val[1]); e1 += 8;
        vst1q_s16(e2, v.val[2]); e2 += 8;
        vst1q_s16(e3, v.val[3]); e3 += 8;
      }
#elif defined(__AVX512BW__)
      const __m512i idx0 = idx_stride_u16(4, 0);
      const __m512i idx1 = idx_stride_u16(4, 1);
      const __m512i idx2 = idx_stride_u16(4, 2);
      const __m512i idx3 = idx_stride_u16(4, 3);

      for (; i + 32 <= EQm; i += 32) {
        // 32 groups * 4 u16 = 128 u16 = 4x512b
        __m512i A = _mm512_loadu_si512((const void*)(f +  0));
        __m512i B = _mm512_loadu_si512((const void*)(f + 32));
        __m512i C = _mm512_loadu_si512((const void*)(f + 64));
        __m512i D = _mm512_loadu_si512((const void*)(f + 96));
        f += 128;
       // Build results from (A,B) and (C,D) with masking (boundary at 64 u16)
        __m512i o0_ab = _mm512_permutex2var_epi16(A, idx0, B);
        __m512i o1_ab = _mm512_permutex2var_epi16(A, idx1, B);
        __m512i o2_ab = _mm512_permutex2var_epi16(A, idx2, B);
        __m512i o3_ab = _mm512_permutex2var_epi16(A, idx3, B);

        __m512i idx0_cd = _mm512_sub_epi16(idx0, _mm512_set1_epi16(64));
        __m512i idx1_cd = _mm512_sub_epi16(idx1, _mm512_set1_epi16(64));
        __m512i idx2_cd = _mm512_sub_epi16(idx2, _mm512_set1_epi16(64));
        __m512i idx3_cd = _mm512_sub_epi16(idx3, _mm512_set1_epi16(64));

        __m512i o0_cd = _mm512_permutex2var_epi16(C, idx0_cd, D);
        __m512i o1_cd = _mm512_permutex2var_epi16(C, idx1_cd, D);
        __m512i o2_cd = _mm512_permutex2var_epi16(C, idx2_cd, D);
        __m512i o3_cd = _mm512_permutex2var_epi16(C, idx3_cd, D);

        // lanes needing CD are those where idx >= 64
	__mmask32 m0 = _mm512_cmpge_epu16_mask(idx0, _mm512_set1_epi16(64));
	__mmask32 m1 = _mm512_cmpge_epu16_mask(idx1, _mm512_set1_epi16(64));
	__mmask32 m2 = _mm512_cmpge_epu16_mask(idx2, _mm512_set1_epi16(64));
	__mmask32 m3 = _mm512_cmpge_epu16_mask(idx3, _mm512_set1_epi16(64));
	
	__m512i o0 = _mm512_mask_mov_epi16(o0_ab, m0, o0_cd);
	__m512i o1 = _mm512_mask_mov_epi16(o1_ab, m1, o1_cd);
	__m512i o2 = _mm512_mask_mov_epi16(o2_ab, m2, o2_cd);
	__m512i o3 = _mm512_mask_mov_epi16(o3_ab, m3, o3_cd);
	
	_mm512_storeu_si512((void*)e, o0); e += 32;
	_mm512_storeu_si512((void*)e1, o1); e1 += 32;
	_mm512_storeu_si512((void*)e2, o2); e2 += 32;
	_mm512_storeu_si512((void*)e3, o3); e3 += 32;
      }
#else 
      simde__m128i *e0_128 = (simde__m128i *)e;
      simde__m128i *e1_128 = (simde__m128i *)e1;
      simde__m128i *e2_128 = (simde__m128i *)e2;
      simde__m128i *e3_128 = (simde__m128i *)e3;
      simde__m128i *f128   = (simde__m128i *)f;	  
     
      const uint8_t shuf16[16] __attribute__((aligned(16)))= {0,1,8,9,2,3,10,11,4,5,12,13,6,7,14,15};
      const simde__m128i *shuf16_128 = (const simde__m128i *)shuf16;

      for (i=0; i < (EQm & ~7); i += 8) {      
        simde__m128i f0j = simde_mm_loadu_si128(f128++);
        simde__m128i f1j = simde_mm_loadu_si128(f128++);
        simde__m128i f2j = simde_mm_loadu_si128(f128++);
        simde__m128i f3j = simde_mm_loadu_si128(f128++);

        simde__m128i tmp0 = simde_mm_shuffle_epi8(f0j,*shuf16_128); // f0(i) f0(i+4) f0(i+1) f0(i+5) f0(i+2) f0(i+6) f0(i+3) f0(i+7)
        simde__m128i tmp1 = simde_mm_shuffle_epi8(f1j,*shuf16_128); // f1(i) f1(i+4) f1(i+1) f1(i+5) f1(i+2) f1(i+6) f1(i+3) f1(i+7)
        simde__m128i tmp2 = simde_mm_shuffle_epi8(f2j,*shuf16_128); // f2(i) f2(i+4) f2(i+1) f2(i+5) f2(i+2) f2(i+6) f2(i+3) f2(i+7)
	simde__m128i tmp3 = simde_mm_shuffle_epi8(f3j,*shuf16_128); // f3(i) f3(i+2) f3(i+1) f3(i+5) f3(i+2) f3(i+6) f3(i+3) f3(i+7)
        simde__m128i tmp4 = simde_mm_unpacklo_epi32(tmp0,tmp1); // f0(i) f0(i+4) f1(i) f1(i+4) f0(i+1) f0(i+5) f1(i+1) f1(i+5)
        simde__m128i tmp5 = simde_mm_unpacklo_epi32(tmp2,tmp3); // f2(i) f2(i+4) f3(i) f3(i+4) f2(i+1) f2(i+5) f3(i+1) f3(i+5)
       	simde_mm_storeu_si128(e0_128++,simde_mm_unpacklo_epi64(tmp4, tmp5));   // f0(i)   f0(i+4) f1(i)   f1(i+4) f2(i)   f2(i+4) f3(i)   f3(i+4)
        simde_mm_storeu_si128(e1_128++,simde_mm_unpackhi_epi64(tmp4, tmp5));   // f0(i+1) f0(i+5) f1(i+1) f1(i+5) f2(i+1) f2(i+5) f3(i+1) f3(i+5)
        tmp4 = simde_mm_unpackhi_epi32(tmp0,tmp1); // f0(i+2) f0(i+6) f1(i+2) f1(i+6) f0(i+3) f0(i+7) f1(i+3) f1(i+7)
        tmp5 = simde_mm_unpackhi_epi32(tmp2,tmp3); // f2(i+2) f2(i+6) f3(i+2) f3(i+6) f2(i+3) f2(i+7) f3(i+3) f3(i+7)
        simde_mm_storeu_si128(e2_128++,simde_mm_unpacklo_epi64(tmp4, tmp5));   // f0(i+2) f0(i+6) f1(i+2) f1(i+6) f2(i+2) f2(i+6) f3(i+2) f3(i+6)
  	simde_mm_storeu_si128(e3_128++,simde_mm_unpackhi_epi64(tmp4, tmp5));   // f0(i+3) f0(i+7) f1(i+3) f1(i+7) f2(i+3) f2(i+7) f3(i+3) f3(i+7)
      } 
      e=(int16_t *)e0_128;                                   
      e1=(int16_t *)e1_128;                                 
      e2=(int16_t *)e2_128;                                
      e3=(int16_t *)e3_128;                                 
      f=(int16_t *)f128;                                      
#endif
      for (; i < EQm; i++) {
        *e++ = *f++;
        *e1++ = *f++;
        *e2++ = *f++;
        *e3++ = *f++;
      }
    } break;
    case 6: {
      AssertFatal(E % 6 == 0, "");
      int16_t *e1 = e + EQm;
      int16_t *e2 = e1 + EQm;
      int16_t *e3 = e2 + EQm;
      int16_t *e4 = e3 + EQm;
      int16_t *e5 = e4 + EQm;
      int i=0;
#if defined(__aarch64__)

          // Byte indices for extracting each stream
	  // Each s16 occupies 2 bytes → indices are 2*(k + 6*n)
      const uint8x16_t idx0 = {  0,  1, 12, 13, 24, 25, 36, 37,
                                48, 49, 60, 61, 72, 73, 84, 85 };
      const uint8x16_t idx1 = {  2,  3, 14, 15, 26, 27, 38, 39,
                                50, 51, 62, 63, 74, 75, 86, 87 };
      const uint8x16_t idx2 = {  4,  5, 16, 17, 28, 29, 40, 41,
                                52, 53, 64, 65, 76, 77, 88, 89 };
      const uint8x16_t idx3 = {  6,  7, 18, 19, 30, 31, 42, 43,
                                54, 55, 66, 67, 78, 79, 90, 91 };
      const uint8x16_t idx4 = {  8,  9, 20, 21, 32, 33, 44, 45,
                                56, 57, 68, 69, 80, 81, 92, 93 };
      const uint8x16_t idx5 = { 10, 11, 22, 23, 34, 35, 46, 47,
                                58, 59, 70, 71, 82, 83, 94, 95 };

      for (; i + 8 <= EQm; i += 8) {
	// Load 96 bytes (48 u16)
	uint8x16_t b0 = vld1q_u8((const uint8_t*)(f +  0)); // bytes  0..15
	uint8x16_t b1 = vld1q_u8((const uint8_t*)(f +  8)); // bytes 16..31
	uint8x16_t b2 = vld1q_u8((const uint8_t*)(f + 16)); // bytes 32..47
	uint8x16_t b3 = vld1q_u8((const uint8_t*)(f + 24)); // bytes 48..63
	uint8x16_t b4 = vld1q_u8((const uint8_t*)(f + 32)); // bytes 64..79
	uint8x16_t b5 = vld1q_u8((const uint8_t*)(f + 40)); // bytes 80..95
	f += 48;
        uint8x16_t o0b = tbl96_u8(b0,b1,b2,b3,b4,b5, idx0);
        uint8x16_t o1b = tbl96_u8(b0,b1,b2,b3,b4,b5, idx1);
        uint8x16_t o2b = tbl96_u8(b0,b1,b2,b3,b4,b5, idx2);
        uint8x16_t o3b = tbl96_u8(b0,b1,b2,b3,b4,b5, idx3);
        uint8x16_t o4b = tbl96_u8(b0,b1,b2,b3,b4,b5, idx4);
        uint8x16_t o5b = tbl96_u8(b0,b1,b2,b3,b4,b5, idx5);

        vst1q_s16(e, vreinterpretq_s16_u8(o0b)); e += 8;
        vst1q_s16(e1, vreinterpretq_s16_u8(o1b)); e1 += 8;
        vst1q_s16(e2, vreinterpretq_s16_u8(o2b)); e2 += 8;
        vst1q_s16(e3, vreinterpretq_s16_u8(o3b)); e3 += 8;
        vst1q_s16(e4, vreinterpretq_s16_u8(o4b)); e4 += 8;
        vst1q_s16(e5, vreinterpretq_s16_u8(o5b)); e5 += 8;
      }	
#elif defined(__AVX512BW__)

      // Precompute permute control once (all compile-time constant patterns)
      const __m512i idx0_ab = make_idx_ab(0), idx1_ab = make_idx_ab(1);
      const __m512i idx2_ab = make_idx_ab(2), idx3_ab = make_idx_ab(3);
      const __m512i idx4_ab = make_idx_ab(4), idx5_ab = make_idx_ab(5);

      const __m512i idx0_bc = make_idx_bc(0), idx1_bc = make_idx_bc(1);
      const __m512i idx2_bc = make_idx_bc(2), idx3_bc = make_idx_bc(3);
      const __m512i idx4_bc = make_idx_bc(4), idx5_bc = make_idx_bc(5);

      const __mmask32 m0 = make_mask_bc(0), m1 = make_mask_bc(1), m2 = make_mask_bc(2);
      const __mmask32 m3 = make_mask_bc(3), m4 = make_mask_bc(4), m5 = make_mask_bc(5);

      const __mmask32 store16 = 0xFFFFu; // store only first 16 lanes (16x u16)
      for (; i + 16 <= EQm; i += 16) {
        // 16 groups = 16*(6 u16) = 96 u16 = 192 bytes = 3 * 64B
        __m512i A = _mm512_loadu_si512((const void*)(f +  0)); // u16[0..31]
        __m512i B = _mm512_loadu_si512((const void*)(f + 32)); // u16[32..63]
        __m512i C = _mm512_loadu_si512((const void*)(f + 64)); // u16[64..95]
        f += 96;

        // Gather from AB (global 0..63) and BC (global 32..95)
        __m512i o0_ab = _mm512_permutex2var_epi16(A, idx0_ab, B);
        __m512i o1_ab = _mm512_permutex2var_epi16(A, idx1_ab, B);
        __m512i o2_ab = _mm512_permutex2var_epi16(A, idx2_ab, B);
        __m512i o3_ab = _mm512_permutex2var_epi16(A, idx3_ab, B);
        __m512i o4_ab = _mm512_permutex2var_epi16(A, idx4_ab, B);
        __m512i o5_ab = _mm512_permutex2var_epi16(A, idx5_ab, B);

        __m512i o0_bc = _mm512_permutex2var_epi16(B, idx0_bc, C);
        __m512i o1_bc = _mm512_permutex2var_epi16(B, idx1_bc, C);
        __m512i o2_bc = _mm512_permutex2var_epi16(B, idx2_bc, C);
        __m512i o3_bc = _mm512_permutex2var_epi16(B, idx3_bc, C);
        __m512i o4_bc = _mm512_permutex2var_epi16(B, idx4_bc, C);
        __m512i o5_bc = _mm512_permutex2var_epi16(B, idx5_bc, C);

        // Select lanes that crossed the 64-element boundary
   	__m512i o0 = _mm512_mask_mov_epi16(o0_ab, m0, o0_bc);
        __m512i o1 = _mm512_mask_mov_epi16(o1_ab, m1, o1_bc);
        __m512i o2 = _mm512_mask_mov_epi16(o2_ab, m2, o2_bc);
        __m512i o3 = _mm512_mask_mov_epi16(o3_ab, m3, o3_bc);
        __m512i o4 = _mm512_mask_mov_epi16(o4_ab, m4, o4_bc);
        __m512i o5 = _mm512_mask_mov_epi16(o5_ab, m5, o5_bc);

        // Store only the first 16 lanes (16 groups) to each plane
        _mm512_mask_storeu_epi16((void*)e, store16, o0); e += 16;
        _mm512_mask_storeu_epi16((void*)e1, store16, o1); e1 += 16;
        _mm512_mask_storeu_epi16((void*)e2, store16, o2); e2 += 16;
        _mm512_mask_storeu_epi16((void*)e3, store16, o3); e3 += 16;
        _mm512_mask_storeu_epi16((void*)e4, store16, o4); e4 += 16;
        _mm512_mask_storeu_epi16((void*)e5, store16, o5); e5 += 16;
     }
			//
#endif
      for (; i < EQm; i++) {
        *e++ = *f++;
        *e1++ = *f++;
        *e2++ = *f++;
        *e3++ = *f++;
        *e4++ = *f++;
        *e5++ = *f++;
      }
    } break;
    case 8: {
      AssertFatal(E % 8 == 0, "");
      int16_t *e1 = e + EQm;
      int16_t *e2 = e1 + EQm;
      int16_t *e3 = e2 + EQm;
      int16_t *e4 = e3 + EQm;
      int16_t *e5 = e4 + EQm;
      int16_t *e6 = e5 + EQm;
      int16_t *e7 = e6 + EQm;
      int i=0;
#if 0 //defined(__aarch64__)
      // SIMDE version below is more efficient than tbl128
          // For 8 groups: byte indices for stream k are 2*(k + 8*n) for n=0..7.
	  // That is: 2k + 16n (and +1 for the high byte of the u16).
      const uint8x16_t idx0 = {  0,  1, 16, 17, 32, 33, 48, 49, 64, 65, 80, 81, 96, 97,112,113 };
      const uint8x16_t idx1 = {  2,  3, 18, 19, 34, 35, 50, 51, 66, 67, 82, 83, 98, 99,114,115 };
      const uint8x16_t idx2 = {  4,  5, 20, 21, 36, 37, 52, 53, 68, 69, 84, 85,100,101,116,117 };
      const uint8x16_t idx3 = {  6,  7, 22, 23, 38, 39, 54, 55, 70, 71, 86, 87,102,103,118,119 };
      const uint8x16_t idx4 = {  8,  9, 24, 25, 40, 41, 56, 57, 72, 73, 88, 89,104,105,120,121 };
      const uint8x16_t idx5 = { 10, 11, 26, 27, 42, 43, 58, 59, 74, 75, 90, 91,106,107,122,123 };
      const uint8x16_t idx6 = { 12, 13, 28, 29, 44, 45, 60, 61, 76, 77, 92, 93,108,109,124,125 };
      const uint8x16_t idx7 = { 14, 15, 30, 31, 46, 47, 62, 63, 78, 79, 94, 95,110,111,126,127 };

      for (; i + 8 <= EQm; i += 8) {
        // Load 128 bytes = 64 u16 = 8 groups (aligned-friendly, but vld1q_u8 is fine either way)
        uint8x16_t b0 = vld1q_u8((const uint8_t*)(f +  0)); // bytes  0..15
        uint8x16_t b1 = vld1q_u8((const uint8_t*)(f +  8)); // bytes 16..31
        uint8x16_t b2 = vld1q_u8((const uint8_t*)(f + 16)); // bytes 32..47
        uint8x16_t b3 = vld1q_u8((const uint8_t*)(f + 24)); // bytes 48..63
        uint8x16_t b4 = vld1q_u8((const uint8_t*)(f + 32)); // bytes 64..79
        uint8x16_t b5 = vld1q_u8((const uint8_t*)(f + 40)); // bytes 80..95
        uint8x16_t b6 = vld1q_u8((const uint8_t*)(f + 48)); // bytes 96..111
        uint8x16_t b7 = vld1q_u8((const uint8_t*)(f + 56)); // bytes 112..127
        f += 64; // consumed 64 u16
	       
        uint8x16_t o0b = tbl128_u8(b0,b1,b2,b3,b4,b5,b6,b7, idx0);
        uint8x16_t o1b = tbl128_u8(b0,b1,b2,b3,b4,b5,b6,b7, idx1);
        uint8x16_t o2b = tbl128_u8(b0,b1,b2,b3,b4,b5,b6,b7, idx2);
        uint8x16_t o3b = tbl128_u8(b0,b1,b2,b3,b4,b5,b6,b7, idx3);
        uint8x16_t o4b = tbl128_u8(b0,b1,b2,b3,b4,b5,b6,b7, idx4);
        uint8x16_t o5b = tbl128_u8(b0,b1,b2,b3,b4,b5,b6,b7, idx5);
        uint8x16_t o6b = tbl128_u8(b0,b1,b2,b3,b4,b5,b6,b7, idx6);
        uint8x16_t o7b = tbl128_u8(b0,b1,b2,b3,b4,b5,b6,b7, idx7);

        vst1q_s16(e, vreinterpretq_s16_u8(o0b)); e += 8;
        vst1q_s16(e1, vreinterpretq_s16_u8(o1b)); e1 += 8;
        vst1q_s16(e2, vreinterpretq_s16_u8(o2b)); e2 += 8;
        vst1q_s16(e3, vreinterpretq_s16_u8(o3b)); e3 += 8;
        vst1q_s16(e4, vreinterpretq_s16_u8(o4b)); e4 += 8;
        vst1q_s16(e5, vreinterpretq_s16_u8(o5b)); e5 += 8;
        vst1q_s16(e6, vreinterpretq_s16_u8(o6b)); e6 += 8;
        vst1q_s16(e7, vreinterpretq_s16_u8(o7b)); e7 += 8;	
      }
#elif defined(__AVX512BW__)

    // Precompute indices for each stream k=0..7
    const __m512i idx0 = idx_stride_u16(8,0);
    const __m512i idx1 = idx_stride_u16(8,1);
    const __m512i idx2 = idx_stride_u16(8,2);
    const __m512i idx3 = idx_stride_u16(8,3);
    const __m512i idx4 = idx_stride_u16(8,4);
    const __m512i idx5 = idx_stride_u16(8,5);
    const __m512i idx6 = idx_stride_u16(8,6);
    const __m512i idx7 = idx_stride_u16(8,7);

    const __m512i c64  = _mm512_set1_epi16(64);
    const __m512i c128 = _mm512_set1_epi16(128);
    const __m512i c192 = _mm512_set1_epi16(192);
    
    for (; i + 32 <= EQm; i += 32) {
        // 32 groups * 8 u16 = 256 u16 = 8 * 32 u16 = 8 ZMM vectors
	__m512i A = _mm512_loadu_si512((const void*)(f +  0));   // u16[  0.. 31]
	__m512i B = _mm512_loadu_si512((const void*)(f + 32));   // u16[ 32.. 63]
	__m512i C = _mm512_loadu_si512((const void*)(f + 64));   // u16[ 64.. 95]
	__m512i D = _mm512_loadu_si512((const void*)(f + 96));   // u16[ 96..127]
	__m512i E = _mm512_loadu_si512((const void*)(f + 128));  // u16[128..159]
	__m512i F = _mm512_loadu_si512((const void*)(f + 160));  // u16[160..191]
	__m512i G = _mm512_loadu_si512((const void*)(f + 192));  // u16[192..223]
	__m512i H = _mm512_loadu_si512((const void*)(f + 224));  // u16[224..255]
	f += 256;
        // Helper macro: compute one stream (k) by permuting from 4 segments and blending
	#define DO_STREAM(IDX, OUTPTR) do { \
	   /* segment masks based on global index p = k+8*n */ \
	   __mmask32 m1 = _mm512_cmpge_epu16_mask((IDX), c64); \
	   __mmask32 m2 = _mm512_cmpge_epu16_mask((IDX), c128); \
	   __mmask32 m3 = _mm512_cmpge_epu16_mask((IDX), c192); \
	   /* indices relative to each segment base */ \
	   __m512i i0 = (IDX); \
	   __m512i i1 = _mm512_sub_epi16((IDX), c64); \
	   __m512i i2 = _mm512_sub_epi16((IDX), c128); \
	   __m512i i3 = _mm512_sub_epi16((IDX), c192); \
	   /* permute from each 64-u16 segment (two ZMMs per segment) */ \
	   __m512i r0 = _mm512_permutex2var_epi16(A, i0, B); \
	   __m512i r1 = _mm512_permutex2var_epi16(C, i1, D); \
	   __m512i r2 = _mm512_permutex2var_epi16(E, i2, F); \
	   __m512i r3 = _mm512_permutex2var_epi16(G, i3, H); \
           /* blend: later segments overwrite earlier where applicable */ \
	   __m512i r = _mm512_mask_mov_epi16(r0, m1, r1); \
	   r = _mm512_mask_mov_epi16(r,  m2, r2); \
	   r = _mm512_mask_mov_epi16(r,  m3, r3); \
	   _mm512_storeu_si512((void*)(OUTPTR), r); \
	   (OUTPTR) += 32; \
	} while(0)
        DO_STREAM(idx0, e);
	DO_STREAM(idx1, e1);
	DO_STREAM(idx2, e2);
	DO_STREAM(idx3, e3);
	DO_STREAM(idx4, e4);
	DO_STREAM(idx5, e5);
	DO_STREAM(idx6, e6);
	DO_STREAM(idx7, e7);

        #undef DO_STREAM
    }
#else
      simde__m128i *e0_128 = (simde__m128i *)e;
      simde__m128i *e1_128 = (simde__m128i *)e1;
      simde__m128i *e2_128 = (simde__m128i *)e2;
      simde__m128i *e3_128 = (simde__m128i *)e3;
      simde__m128i *e4_128 = (simde__m128i *)e4;
      simde__m128i *e5_128 = (simde__m128i *)e5;
      simde__m128i *e6_128 = (simde__m128i *)e6;
      simde__m128i *e7_128 = (simde__m128i *)e7;
      simde__m128i *f128   = (simde__m128i *)f;

      for (i=0; i < (EQm & ~7); i += 8) {
        simde__m128i f0j = simde_mm_loadu_si128(f128++);
        simde__m128i f1j = simde_mm_loadu_si128(f128++);
        simde__m128i f2j = simde_mm_loadu_si128(f128++);
        simde__m128i f3j = simde_mm_loadu_si128(f128++);
        simde__m128i f4j = simde_mm_loadu_si128(f128++);
        simde__m128i f5j = simde_mm_loadu_si128(f128++);
        simde__m128i f6j = simde_mm_loadu_si128(f128++);
        simde__m128i f7j = simde_mm_loadu_si128(f128++);

        simde__m128i tmp0 = simde_mm_unpacklo_epi16(f0j, f1j);   // f0(i) f1(i) f0(i+1) f1(i+1) f0(i+2) f1(i+2) f0(i+3) f1(i+3) 
        simde__m128i tmp1 = simde_mm_unpacklo_epi16(f2j, f3j);   // f2(i) f3(i) f2(i+1) f3(i+1) f2(i+2) f3(i+2) f2(i+3) f3(i+3) 
        simde__m128i tmp2 = simde_mm_unpacklo_epi16(f4j, f5j);   // f4(i) f5(i) f4(i+1) f5(i+1) f4(i+2) f5(i+2) f4(i+3) f5(i+3)
        simde__m128i tmp3 = simde_mm_unpacklo_epi16(f6j, f7j);   // f6(i) f7(i) f6(i+1) f7(i+1) f6(i+2) f7(i+2) f6(i+3) f7(i+3)
								 //
        simde__m128i tmp4 = simde_mm_unpacklo_epi32(tmp0, tmp1); // f0(i) f1(i) f2(i)   f3(i)   f0(i+1) f1(i+1) f2(i+1) f3(i+1) 
        simde__m128i tmp5 = simde_mm_unpacklo_epi32(tmp2, tmp3); // f4(i) f5(i) f6(i)   f7(i)   f4(i+1) f5(i+1) f6(i+1) f7(i+1) 
        simde_mm_storeu_si128(e0_128++,simde_mm_unpacklo_epi64(tmp4, tmp5)); // f0(i) f1(i) f2(i) f3(i) f4(i) f5(i) f7(i) f7(i)
        simde_mm_storeu_si128(e1_128++,simde_mm_unpackhi_epi64(tmp4, tmp5));// f0(i+1) f1(i+1) f2(i+1) f3(i+1) f4(i+1) f5(i+1) f6(i+1) f7(i+1)
									    //
        tmp4 = simde_mm_unpackhi_epi32(tmp0, tmp1); // f0(i+2) f1(i+2) f2(i+2) f3(i+2) f0(i+3) f1(i+3) f2(i+3) f3(i+3) 
        tmp5 = simde_mm_unpackhi_epi32(tmp2, tmp3); // f4(i+2) f5(i+2) f6(i+2) f7(i+2) f4(i+3) f5(i+3) f6(i+3) f7(i+3) 
	simde_mm_storeu_si128(e2_128++,simde_mm_unpacklo_epi64(tmp4, tmp5));// f0(i+2) f1(i+2) f2(i+2) f3(i+2) f4(i+2) f5(i+2) f7(i+2) f7(i+2) 
        simde_mm_storeu_si128(e3_128++,simde_mm_unpackhi_epi64(tmp4, tmp5)); // f0(i+3) f1(i+3) f2(i+3) f3(i+3) f4(i+3) f5(i+3) f7(i+3) f7(i+3)
									     
        tmp0 = simde_mm_unpackhi_epi16(f0j, f1j);   // f0(i+4) f1(i+4) f0(i+5) f1(i+5) f0(i+6) f1(i+6) f0(i+7) f1(i+7) 
        tmp1 = simde_mm_unpackhi_epi16(f2j, f3j);   // f2(i+4) f3(i+4) f2(i+5) f3(i+5) f2(i+6) f3(i+6) f2(i+7) f3(i+7) 
        tmp2 = simde_mm_unpackhi_epi16(f4j, f5j);   // f4(i+4) f5(i+4) f4(i+5) f5(i+5) f4(i+6) f5(i+6) f4(i+7) f5(i+7)
        tmp3 = simde_mm_unpackhi_epi16(f6j, f7j);   // f6(i+4) f7(i+4) f6(i+5) f7(i+5) f6(i+6) f7(i+6) f6(i+7) f7(i+7)
								 //
        tmp4 = simde_mm_unpacklo_epi32(tmp0, tmp1); // f0(i+4) f1(i+4) f2(i+4)   f3(i+4)   f0(i+5) f1(i+5) f2(i+5) f3(i+5) 
        tmp5 = simde_mm_unpacklo_epi32(tmp2, tmp3); // f4(i+4) f5(i+4) f6(i+4)   f7(i+4)   f4(i+5) f5(i+5) f6(i+5) f7(i+5) 
								 
        simde_mm_storeu_si128(e4_128++,simde_mm_unpacklo_epi64(tmp4, tmp5)); // f0(i+4) f1(i+4) f2(i+4) f3(i+4) f4(i+4) f5(i+4) f7(i+4) f7(i+4)
        simde_mm_storeu_si128(e5_128++,simde_mm_unpackhi_epi64(tmp4, tmp5));// f0(i+5) f1(i+5) f2(i+5) f3(i+5) f4(i+5) f5(i+5) f6(i+5) f7(i+5)
        tmp4 = simde_mm_unpackhi_epi32(tmp0, tmp1); // f0(i+6) f1(i+6) f2(i+6) f3(i+6) f0(i+6) f1(i+6) f2(i+6) f3(i+6) 
        tmp5 = simde_mm_unpackhi_epi32(tmp2, tmp3); // f4(i+6) f5(i+6) f6(i+6) f7(i+6) f4(i+6) f5(i+6) f6(i+6) f7(i+6) 
	simde_mm_storeu_si128(e6_128++,simde_mm_unpacklo_epi64(tmp4, tmp5));// f0(i+6) f1(i+6) f2(i+6) f3(i+6) f4(i+6) f5(i+6) f7(i+6) f7(i+6) 
        simde_mm_storeu_si128(e7_128++,simde_mm_unpackhi_epi64(tmp4, tmp5)); // f0(i+7) f1(i+7) f2(i+7) f3(i+7) f4(i+7) f5(i+7) f7(i+7) f7(i+7)
      }
      e  = (int16_t *)e0_128;
      e1 = (int16_t *)e1_128;
      e2 = (int16_t *)e2_128;
      e3 = (int16_t *)e3_128;
      e4 = (int16_t *)e4_128;
      e5 = (int16_t *)e5_128;
      e6 = (int16_t *)e6_128;
      e7 = (int16_t *)e7_128;
      f  = (int16_t *)f128;
#endif
      for(;i < EQm ; i++) {
        *e++ = *f++;
        *e1++ = *f++;
        *e2++ = *f++;
        *e3++ = *f++;
        *e4++ = *f++;
        *e5++ = *f++;
        *e6++ = *f++;
        *e7++ = *f++;
      }
    } break;
    default:
      AssertFatal(1 == 0, "Should not get here : Qm %d\n", Qm);
      break;
  }

}

int nr_rate_matching_ldpc(uint32_t Tbslbrm,
                          uint8_t BG,
                          uint16_t Z,
                          uint8_t *d,
                          uint8_t *e,
                          uint8_t C,
                          uint32_t F,
                          uint32_t Foffset,
                          uint8_t rvidx,
                          uint32_t E)
{
  if (C == 0) {
    LOG_E(PHY, "nr_rate_matching: invalid parameter C %d\n", C);
    return -1;
  }

  //Bit selection
  uint32_t N = (BG == 1) ? (66 * Z) : (50 * Z);
  uint32_t Ncb;
  if (Tbslbrm == 0)
    Ncb = N;
  else {
    uint32_t Nref = 3 * Tbslbrm / (2 * C); //R_LBRM = 2/3
    Ncb = min(N, Nref);
  }

  uint32_t ind = (index_k0[BG - 1][rvidx] * Ncb / N) * Z;

#ifdef RM_DEBUG
  printf("nr_rate_matching_ldpc: E %u, F %u, Foffset %u, k0 %u, Ncb %u, rvidx %d, Tbslbrm %u\n",
         E,
         F,
         Foffset,
         ind,
         Ncb,
         rvidx,
         Tbslbrm);
#endif

  if (Foffset > E) {
    LOG_E(PHY,
          "nr_rate_matching: invalid parameters (Foffset %d > E %d) F %d, k0 %d, Ncb %d, rvidx %d, Tbslbrm %d\n",
          Foffset,
          E,
          F,
          ind,
          Ncb,
          rvidx,
          Tbslbrm);
    return -1;
  }
  if (Foffset > Ncb) {
    LOG_E(PHY, "nr_rate_matching: invalid parameters (Foffset %d > Ncb %d)\n", Foffset, Ncb);
    return -1;
  }

  if (ind >= Foffset && ind < (F + Foffset))
    ind = F + Foffset;

  uint32_t k = 0;
  if (ind < Foffset) { // case where we have some bits before the filler and the rest after
    memcpy((void *)e, (void *)(d + ind), Foffset - ind);

    if (E + F <= Ncb - ind) { // E+F doesn't contain all coded bits
      memcpy((void *)(e + Foffset - ind), (void *)(d + Foffset + F), E - Foffset + ind);
      k = E;
    } else {
      memcpy((void *)(e + Foffset - ind), (void *)(d + Foffset + F), Ncb - Foffset - F);
      k = Ncb - F - ind;
    }
  } else {
    if (E <= Ncb - ind) { // E+F doesn't contain all coded bits
      memcpy((void *)(e), (void *)(d + ind), E);
      k = E;
    } else {
      memcpy((void *)(e), (void *)(d + ind), Ncb - ind);
      k = Ncb - ind;
    }
  }

  while (k < E) { // case where we do repetitions (low mcs)
    // chunk before filler: d[0 .. Foffset)
    if (Foffset > 0) {
      uint32_t n = min(Foffset, E - k);
      memcpy(e + k, d, n);
      k += n;
      if (k >= E)
        break;
    }
    // chunk after filler: d[Foffset+F .. Ncb)
    uint32_t after = Ncb - Foffset - F;
    if (after > 0) {
      uint32_t n = min(after, E - k);
      memcpy(e + k, d + Foffset + F, n);
      k += n;
    }
  }

  return 0;
}

#define RMLOOP for (;ind<(ind2&7);k+=8,ind+=8) \
      simde_mm_storeu_si128(&d[ind],simde_mm_adds_epi16(simde_mm_loadu_si128(&soft_input[k]),simde_mm_loadu_si128(&d[ind])));\
   for (; ind<ind2 ; ind++,k++) d[ind] += soft_input[k];  

int nr_rate_matching_ldpc_rx_simd(uint32_t Tbslbrm,
                                  uint8_t BG,
                                  uint16_t Z,
                                  int16_t *d,
                                  int16_t *soft_input,
                                  uint8_t C,
                                  uint8_t rvidx,
                                  uint8_t clear,
                                  uint32_t E,
                                  uint32_t F,
                                  uint32_t Foffset)
{
  if (C == 0) {
    LOG_E(PHY, "nr_rate_matching: invalid parameter C %d\n", C);
    return -1;
  }

  //Bit selection
  uint32_t N = (BG == 1) ? (66 * Z) : (50 * Z);
  uint32_t Ncb;
  if (Tbslbrm == 0)
    Ncb = N;
  else {
    uint32_t Nref = (3 * Tbslbrm / (2 * C)); //R_LBRM = 2/3
    Ncb = min(N, Nref);
  }

  uint32_t ind = (index_k0[BG - 1][rvidx] * Ncb / N) * Z;
  if (Foffset > E) {
    LOG_E(PHY, "nr_rate_matching: invalid parameters (Foffset %d > E %d)\n", Foffset, E);
    return -1;
  }
  if (Foffset > Ncb) {
    LOG_E(PHY, "nr_rate_matching: invalid parameters (Foffset %d > Ncb %d)\n", Foffset, Ncb);
    return -1;
  }

#ifdef RM_DEBUG
  printf("nr_rate_matching_ldpc_rx: Clear %d, E %u, Foffset %u, k0 %u, Ncb %u, rvidx %d, Tbslbrm %u\n",
         clear,
         E,
         Foffset,
         ind,
         Ncb,
         rvidx,
         Tbslbrm);
#endif

  if (clear == 1)
    memset(d, 0, Ncb * sizeof(int16_t));

  uint32_t k = 0;
  if (ind < Foffset) {
   int ind2 = ind + min(Foffset-ind,E);
   RMLOOP;
  }
  if (ind >= Foffset && ind < Foffset + F)
    ind = Foffset + F;
  int ind2 = ind + min(Ncb-ind,E-k);
  RMLOOP;

  while (k < E) {
   ind=0;
   ind2 = min(Foffset,E-k);
   RMLOOP;
   ind = Foffset+F;
   ind2 = ind + min(Ncb-ind,E-k);
   RMLOOP;
  }
  return 0;
}

int nr_rate_matching_ldpc_rx(uint32_t Tbslbrm,
                             uint8_t BG,
                             uint16_t Z,
                             int16_t *d,
                             int16_t *soft_input,
                             uint8_t C,
                             uint8_t rvidx,
                             uint8_t clear,
                             uint32_t E,
                             uint32_t F,
                             uint32_t Foffset)
{
  if (BG == 1) {
     nr_rate_matching_ldpc_rx_simd(Tbslbrm,
                                   BG,
                                   Z,
                                   d,
                                   soft_input,
                                   C,
                                   rvidx,
                                   clear,
                                   E,
                                   F,
                                   Foffset);
     return 0;
  }

  if (C == 0) {
    LOG_E(PHY, "nr_rate_matching: invalid parameter C %d\n", C);
    return -1;
  }

  //Bit selection
  uint32_t N = (BG == 1) ? (66 * Z) : (50 * Z);
  uint32_t Ncb;
  if (Tbslbrm == 0)
    Ncb = N;
  else {
    uint32_t Nref = (3 * Tbslbrm / (2 * C)); //R_LBRM = 2/3
    Ncb = min(N, Nref);
  }

  uint32_t ind = (index_k0[BG - 1][rvidx] * Ncb / N) * Z;
  if (Foffset > E) {
    LOG_E(PHY, "nr_rate_matching: invalid parameters (Foffset %d > E %d)\n", Foffset, E);
    return -1;
  }
  if (Foffset > Ncb) {
    LOG_E(PHY, "nr_rate_matching: invalid parameters (Foffset %d > Ncb %d)\n", Foffset, Ncb);
    return -1;
  }

#ifdef RM_DEBUG
  printf("nr_rate_matching_ldpc_rx: Clear %d, E %u, Foffset %u, k0 %u, Ncb %u, rvidx %d, Tbslbrm %u\n",
         clear,
         E,
         Foffset,
         ind,
         Ncb,
         rvidx,
         Tbslbrm);
#endif

  if (clear == 1)
    memset(d, 0, Ncb * sizeof(int16_t));

  uint32_t k = 0;
  if (ind < Foffset)
    for (; (ind < Foffset) && (k < E); ind++) {
#ifdef RM_DEBUG
      printf("RM_RX k%u Ind %u(before filler): %d (%d)=>", k, ind, d[ind], soft_input[k]);
#endif
      d[ind] += soft_input[k++];
#ifdef RM_DEBUG
      printf("%d\n", d[ind]);
#endif
    }
  if (ind >= Foffset && ind < Foffset + F)
    ind = Foffset + F;

  for (; (ind < Ncb) && (k < E); ind++) {
#ifdef RM_DEBUG
    printf("RM_RX k%u Ind %u(after filler) %d (%d)=>", k, ind, d[ind], soft_input[k]);
#endif
    d[ind] += soft_input[k++];
#ifdef RM_DEBUG
    printf("%d\n", d[ind]);
#endif
  }

  while (k < E) {
    for (ind = 0; (ind < Foffset) && (k < E); ind++) {
#ifdef RM_DEBUG
      printf("RM_RX k%u Ind %u(before filler) %d(%d)=>", k, ind, d[ind], soft_input[k]);
#endif
      d[ind] += soft_input[k++];
#ifdef RM_DEBUG
      printf("%d\n", d[ind]);
#endif
    }
    for (ind = Foffset + F; (ind < Ncb) && (k < E); ind++) {
#ifdef RM_DEBUG
      printf("RM_RX (after filler) k%u Ind: %u (%d)(soft in %d)=>", k, ind, d[ind], soft_input[k]);
#endif
      d[ind] += soft_input[k++];
#ifdef RM_DEBUG
      printf("%d\n", d[ind]);
#endif
    }
  }
  return 0;
}
