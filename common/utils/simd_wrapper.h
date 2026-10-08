/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef SIMD_WRAPPER_H
#define SIMD_WRAPPER_H

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Wpsabi"
#endif

#include <stdint.h>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <immintrin.h>
#else
#ifndef __lzcnt16
#define __lzcnt16(x) ((uint16_t)((x) ? (__builtin_clz(x) - 16) : 16))
#endif
#endif

#define SIMDE_ENABLE_NATIVE_ALIASES
#include <simde/x86/avx512.h>

// Add missing intrinsics or fix existing intrinsics in older SIMDe package versions
#if defined(__cplusplus)
#if !defined(__AVX512CD__)
inline simde__m512i safe_mm512_lzcnt_epi32(simde__m512i a) {
  union {
    simde__m512i v;
    uint32_t u32[16];
  } u_in, u_out;
  u_in.v = a;
  for (int i = 0; i < 16; ++i) {
    u_out.u32[i] = u_in.u32[i] ? (uint32_t)__builtin_clz(u_in.u32[i]) : 32U;
  }
  return u_out.v;
}
#undef _mm512_lzcnt_epi32
#define _mm512_lzcnt_epi32(a) safe_mm512_lzcnt_epi32(a)
#endif

#if !defined(__AVX512F__)
inline int32_t safe_mm512_mask_reduce_max_epi32(simde__mmask16 k, simde__m512i a) {
  union {
    simde__m512i v;
    int32_t i32[16];
  } u;
  u.v = a;
  int32_t max_val = -2147483647 - 1; // INT32_MIN
  for (int i = 0; i < 16; ++i) {
    if (k & (1 << i)) {
      if (u.i32[i] > max_val) {
        max_val = u.i32[i];
      }
    }
  }
  return max_val;
}
#undef _mm512_mask_reduce_max_epi32
#define _mm512_mask_reduce_max_epi32(k, a) safe_mm512_mask_reduce_max_epi32(k, a)

inline void safe_mm512_mask_storeu_epi16(void* mem_addr, simde__mmask32 k, simde__m512i a) {
  union {
    simde__m512i v;
    uint16_t u16[32];
  } u;
  u.v = a;
  uint16_t* mem = (uint16_t*)mem_addr;
  for (int i = 0; i < 32; ++i) {
    if (k & (1U << i)) {
      mem[i] = u.u16[i];
    }
  }
}
#undef _mm512_mask_storeu_epi16
#define _mm512_mask_storeu_epi16(mem_addr, k, a) safe_mm512_mask_storeu_epi16(mem_addr, k, a)

inline void safe_mm512_mask_storeu_epi64(void* mem_addr, simde__mmask8 k, simde__m512i a) {
  union {
    simde__m512i v;
    uint64_t u64[8];
  } u;
  u.v = a;
  uint64_t* mem = (uint64_t*)mem_addr;
  for (int i = 0; i < 8; ++i) {
    if (k & (1 << i)) {
      mem[i] = u.u64[i];
    }
  }
}
#undef _mm512_mask_storeu_epi64
#define _mm512_mask_storeu_epi64(mem_addr, k, a) safe_mm512_mask_storeu_epi64(mem_addr, k, a)

inline simde__m512i safe_mm512_srav_epi32(simde__m512i a, simde__m512i count) {
  union {
    simde__m512i v;
    int32_t i32[16];
    uint32_t u32[16];
  } u_a, u_count, u_out;
  u_a.v = a;
  u_count.v = count;
  for (int i = 0; i < 16; ++i) {
    uint32_t shift = u_count.u32[i];
    if (shift < 32) {
      u_out.i32[i] = u_a.i32[i] >> shift;
    } else {
      u_out.i32[i] = (u_a.i32[i] < 0) ? -1 : 0;
    }
  }
  return u_out.v;
}
#undef _mm512_srav_epi32
#define _mm512_srav_epi32(a, count) safe_mm512_srav_epi32(a, count)

inline simde__m512i safe_mm512_srav_epi64(simde__m512i a, simde__m512i count) {
  union {
    simde__m512i v;
    int64_t i64[8];
    uint64_t u64[8];
  } u_a, u_count, u_out;
  u_a.v = a;
  u_count.v = count;
  for (int i = 0; i < 8; ++i) {
    uint64_t shift = u_count.u64[i];
    if (shift < 64) {
      u_out.i64[i] = u_a.i64[i] >> shift;
    } else {
      u_out.i64[i] = (u_a.i64[i] < 0) ? -1 : 0;
    }
  }
  return u_out.v;
}
#undef _mm512_srav_epi64
#define _mm512_srav_epi64(a, count) safe_mm512_srav_epi64(a, count)
#endif

#if !defined(__AVX512DQ__)
#undef _mm512_extracti64x2_epi64
#define _mm512_extracti64x2_epi64(a, imm8) \
  ([](simde__m512i _a) -> simde__m128i { \
    union { \
      simde__m512i v; \
      simde__m128i m128[4]; \
    } u; \
    u.v = _a; \
    return u.m128[(imm8) & 3]; \
  }(a))
#endif

#if !defined(__AVX512BW__)
inline void safe_mm_mask_storeu_epi8(void* mem_addr, simde__mmask16 k, simde__m128i a) {
  union {
    simde__m128i v;
    uint8_t u8[16];
  } u;
  u.v = a;
  uint8_t* mem = (uint8_t*)mem_addr;
  for (int i = 0; i < 16; ++i) {
    if (k & (1 << i)) {
      mem[i] = u.u8[i];
    }
  }
}
#undef _mm_mask_storeu_epi8
#define _mm_mask_storeu_epi8(mem_addr, k, a) safe_mm_mask_storeu_epi8(mem_addr, k, a)

inline void safe_mm256_mask_storeu_epi8(void* mem_addr, simde__mmask32 k, simde__m256i a) {
  union {
    simde__m256i v;
    uint8_t u8[32];
  } u;
  u.v = a;
  uint8_t* mem = (uint8_t*)mem_addr;
  for (int i = 0; i < 32; ++i) {
    if (k & (1U << i)) {
      mem[i] = u.u8[i];
    }
  }
}
#undef _mm256_mask_storeu_epi8
#define _mm256_mask_storeu_epi8(mem_addr, k, a) safe_mm256_mask_storeu_epi8(mem_addr, k, a)

inline void safe_mm512_mask_storeu_epi8(void* mem_addr, simde__mmask64 k, simde__m512i a) {
  union {
    simde__m512i v;
    uint8_t u8[64];
  } u;
  u.v = a;
  uint8_t* mem = (uint8_t*)mem_addr;
  for (int i = 0; i < 64; ++i) {
    if (k & (1ULL << i)) {
      mem[i] = u.u8[i];
    }
  }
}
#undef _mm512_mask_storeu_epi8
#define _mm512_mask_storeu_epi8(mem_addr, k, a) safe_mm512_mask_storeu_epi8(mem_addr, k, a)

inline simde__m256i safe_mm256_sllv_epi16(simde__m256i a, simde__m256i count) {
  union {
    simde__m256i v;
    uint16_t u16[16];
  } u_a, u_count, u_out;
  u_a.v = a;
  u_count.v = count;
  for (int i = 0; i < 16; ++i) {
    u_out.u16[i] = (u_count.u16[i] < 16) ? (u_a.u16[i] << u_count.u16[i]) : 0;
  }
  return u_out.v;
}
#undef _mm256_sllv_epi16
#define _mm256_sllv_epi16(a, count) safe_mm256_sllv_epi16(a, count)
#endif
#endif

#endif // SIMD_WRAPPER_HPP
