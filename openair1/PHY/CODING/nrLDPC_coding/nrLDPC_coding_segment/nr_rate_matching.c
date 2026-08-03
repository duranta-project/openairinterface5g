/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "PHY/sse_intrin.h"
#include "nr_rate_matching.h"
#include "common/utils/LOG/log.h"

// #define RM_DEBUG 1

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
      for (; i < (EQm & ~15); i += 64) {
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
      e0_128 = (simde__m128i *)e0;
      e1_128 = (simde__m128i *)e1;
      e2_128 = (simde__m128i *)e2;
      e3_128 = (simde__m128i *)e3;
      e4_128 = (simde__m128i *)e4;
      e5_128 = (simde__m128i *)e5;
      e6_128 = (simde__m128i *)e6;
      e7_128 = (simde__m128i *)e7;
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

void nr_deinterleaving_ldpc(uint32_t E, uint8_t Qm, int16_t *e, int16_t *f)
{
  switch (Qm) {
    case 2: {
      AssertFatal(E % 2 == 0, "E: %d", E);
      int16_t *e1 = e + (E / 2);
      int16_t *end = f + E - 1;
      while (f < end) {
        *e++ = *f++;
        *e1++ = *f++;
      }
    } break;
    case 4: {
      AssertFatal(E % 4 == 0, "E: %d", E);
      int16_t *e1 = e + (E / 4);
      int16_t *e2 = e1 + (E / 4);
      int16_t *e3 = e2 + (E / 4);
      int16_t *end = f + E - 3;
      while (f < end) {
        *e++ = *f++;
        *e1++ = *f++;
        *e2++ = *f++;
        *e3++ = *f++;
      }
    } break;
    case 6: {
      AssertFatal(E % 6 == 0, "E: %d", E);
      int16_t *e1 = e + (E / 6);
      int16_t *e2 = e1 + (E / 6);
      int16_t *e3 = e2 + (E / 6);
      int16_t *e4 = e3 + (E / 6);
      int16_t *e5 = e4 + (E / 6);
      int16_t *end = f + E - 5;
      while (f < end) {
        *e++ = *f++;
        *e1++ = *f++;
        *e2++ = *f++;
        *e3++ = *f++;
        *e4++ = *f++;
        *e5++ = *f++;
      }
    } break;
    case 8: {
      AssertFatal(E % 8 == 0, "E: %d", E);
      int16_t *e1 = e + (E / 8);
      int16_t *e2 = e1 + (E / 8);
      int16_t *e3 = e2 + (E / 8);
      int16_t *e4 = e3 + (E / 8);
      int16_t *e5 = e4 + (E / 8);
      int16_t *e6 = e5 + (E / 8);
      int16_t *e7 = e6 + (E / 8);
      int16_t *end = f + E - 7;
      while (f < end) {
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
