/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#pragma once
#include <stdint.h>

#if defined(__HIPCC__)
  #include <hip/hip_runtime.h>
#elif defined(__CUDACC__)
  #include <cuda_runtime.h>
#endif

/* IMPORTANT:
   hipcc/nvcc compile each TU in (at least) a host pass and a device pass.
   __HIP_DEVICE_COMPILE__ / __CUDA_ARCH__ are NOT set in the host pass.
   If we gate __device__ on those, we end up with host-only definitions that
   can't be called from kernels.
*/
#if defined(__HIPCC__) || defined(__CUDACC__)
  #define GPUHD __host__ __device__ __forceinline__
#else
  #define GPUHD static inline
#endif

/* Full-width lane mask for __shfl_*_sync(): NVIDIA warps are 32 lanes wide,
   AMD wavefronts are up to 64, so the mask has to be 64-bit there. HIP checks
   the width in both compilation passes, hence the platform macro rather than
   __HIP_DEVICE_COMPILE__. */
#if defined(__HIPCC__) || defined(__HIP_PLATFORM_AMD__)
  #define GPU_SHFL_ALL_LANES (~0ull)
#else
  #define GPU_SHFL_ALL_LANES (0xffffffffu)
#endif

/* ---------- lane helpers ---------- */
typedef union {
  uint32_t u;
  int8_t   s8[4];
  uint8_t  u8[4];
  int16_t  s16[2];
  uint16_t u16[2];
} gpu_u32_lanes;

/* Define vector lane types for HIP in BOTH passes (safe); codegen benefits only in device pass. */
#if defined(__HIPCC__)
  typedef signed char   gpu_sc8;
  typedef unsigned char gpu_uc8;

  typedef gpu_sc8   gpu_i8x4    __attribute__((ext_vector_type(4)));
  typedef gpu_uc8   gpu_u8x4    __attribute__((ext_vector_type(4)));
  typedef short     gpu_i16x2   __attribute__((ext_vector_type(2)));

  /* On your clang (22), vector comparisons yield vector-of-char masks */
  typedef char      gpu_mask8x4 __attribute__((ext_vector_type(4)));

  typedef union { uint32_t u; gpu_i8x4   v; } gpu_u32_i8x4;
  typedef union { uint32_t u; gpu_u8x4   v; } gpu_u32_u8x4;
  typedef union { uint32_t u; gpu_i16x2  v; } gpu_u32_i16x2;

  typedef short           gpu_i16;
  typedef unsigned short  gpu_u16;

  typedef gpu_i16 gpu_i16x2 __attribute__((ext_vector_type(2)));
  typedef gpu_i16 gpu_i16x4 __attribute__((ext_vector_type(4))); /* used for widening i8 -> i16 */

#endif

/* ---------- helper: signed rounded average for int8 ----------
   avg = (a+b + (a+b>=0)) >> 1
*/
/* ===================== Intrinsic equivalents ===================== */

GPUHD uint32_t gpu_vcmplts4(uint32_t a, uint32_t b) {
#if defined(__CUDA_ARCH__)
  return __vcmplts4(a, b);
#elif defined(__HIP_DEVICE_COMPILE__)
  gpu_u32_i8x4 A; A.u = a;
  gpu_u32_i8x4 B; B.u = b;
  gpu_mask8x4 m = (A.v < B.v);
  gpu_u32_u8x4 R; R.v = (gpu_u8x4)m;
  return R.u;
#else
  gpu_u32_lanes A; A.u = a;
  gpu_u32_lanes B; B.u = b;
  gpu_u32_lanes R;
  for (int i = 0; i < 4; ++i) R.u8[i] = ((int)A.s8[i] < (int)B.s8[i]) ? 0xFFu : 0x00u;
  return R.u;
#endif
}

GPUHD uint32_t gpu_vcmpeq4(uint32_t a, uint32_t b) {
#if defined(__CUDA_ARCH__)
  return __vcmpeq4(a, b);
#elif defined(__HIP_DEVICE_COMPILE__)
  gpu_u32_u8x4 A; A.u = a;
  gpu_u32_u8x4 B; B.u = b;
  gpu_mask8x4 m = (A.v == B.v);
  gpu_u32_u8x4 R; R.v = (gpu_u8x4)m;
  return R.u;
#else
  gpu_u32_lanes A; A.u = a;
  gpu_u32_lanes B; B.u = b;
  gpu_u32_lanes R;
  for (int i = 0; i < 4; ++i) R.u8[i] = (A.u8[i] == B.u8[i]) ? 0xFFu : 0x00u;
  return R.u;
#endif
}

GPUHD uint32_t gpu_vneg4(uint32_t a) {
#if defined(__CUDA_ARCH__)
  return __vneg4(a);
#elif defined(__HIP_DEVICE_COMPILE__)
  gpu_u32_i8x4 A; A.u = a;
  gpu_i8x4 r = -A.v;
  gpu_u32_u8x4 R; R.v = (gpu_u8x4)r;
  return R.u;
#else
  gpu_u32_lanes A; A.u = a;
  gpu_u32_lanes R;
  for (int i = 0; i < 4; ++i) R.u8[i] = (uint8_t)(-(int)A.s8[i]);
  return R.u;
#endif
}

GPUHD uint32_t gpu_vabs4(uint32_t a) {
#if defined(__CUDA_ARCH__)
  return __vabs4(a);
#elif defined(__HIP_DEVICE_COMPILE__)
    gpu_u32_i8x4 A; A.u = a;

    typedef short gpu_i16;
    typedef gpu_i16 gpu_i16x4 __attribute__((ext_vector_type(4)));

    gpu_i16x4 Aw  = __builtin_convertvector(A.v, gpu_i16x4);
    gpu_i16x4 Neg = -Aw;
    gpu_i16x4 Abs = __builtin_elementwise_max(Aw, Neg);
    gpu_i8x4 Abs8 = __builtin_convertvector(Abs, gpu_i8x4);
    gpu_u32_u8x4 R; R.v = (gpu_u8x4)Abs8;
    return R.u;
#else
  gpu_u32_lanes A; A.u = a;
  gpu_u32_lanes R;
  for (int i = 0; i < 4; ++i) {
    int t = (int)A.s8[i];
    if (t < 0) t = -t;
    R.u8[i] = (uint8_t)t;
  }
  return R.u;
#endif
}

GPUHD uint32_t gpu_vminu4(uint32_t a, uint32_t b) {
#if defined(__CUDA_ARCH__)
  return __vminu4(a, b);
#elif defined(__HIP_DEVICE_COMPILE__)


  gpu_u32_u8x4 A; A.u = a;
  gpu_u32_u8x4 B; B.u = b;

  /* Explicitly widen to u16 lanes, do min, then narrow back */
  typedef unsigned short gpu_u16;
  typedef gpu_u16 gpu_u16x4 __attribute__((ext_vector_type(4)));

  gpu_u16x4 Aw = __builtin_convertvector(A.v, gpu_u16x4);
  gpu_u16x4 Bw = __builtin_convertvector(B.v, gpu_u16x4);

  gpu_u16x4 Mw = __builtin_elementwise_min(Aw, Bw);

  gpu_u8x4 M8 = __builtin_convertvector(Mw, gpu_u8x4);

  gpu_u32_u8x4 R; R.v = M8;

  R.v = __builtin_elementwise_min(A.v, B.v);
  return R.u;
#else
  gpu_u32_lanes A; A.u = a;
  gpu_u32_lanes B; B.u = b;
  gpu_u32_lanes R;
  for (int i = 0; i < 4; ++i) { uint8_t x = A.u8[i], y = B.u8[i]; R.u8[i] = (x < y) ? x : y; }
  return R.u;
#endif
}

/* per-halfword signed min */
GPUHD uint32_t gpu_vmins2(uint32_t a, uint32_t b) {
#if defined(__CUDA_ARCH__)
  return __vmins2(a, b);
#elif defined(__HIP_DEVICE_COMPILE__)
  gpu_u32_i16x2 A; A.u = a;
  gpu_u32_i16x2 B; B.u = b;
  gpu_u32_i16x2 R;
  R.v = __builtin_elementwise_min(A.v, B.v);
  return R.u;
#else
  gpu_u32_lanes A; A.u = a;
  gpu_u32_lanes B; B.u = b;
  gpu_u32_lanes R;
  for (int i = 0; i < 2; ++i) {
    int16_t x = A.s16[i], y = B.s16[i];
    R.s16[i] = (x < y) ? x : y;
  }
  return R.u;
#endif
}

GPUHD uint32_t gpu_vmaxu4(uint32_t a, uint32_t b) {
#if defined(__CUDA_ARCH__)
  return __vmaxu4(a, b);
#elif defined(__HIP_DEVICE_COMPILE__)
  gpu_u32_u8x4 A; A.u = a;
  gpu_u32_u8x4 B; B.u = b;
  /* Explicitly widen to u16 lanes, do min, then narrow back */
  typedef unsigned short gpu_u16;
  typedef gpu_u16 gpu_u16x4 __attribute__((ext_vector_type(4)));

  gpu_u16x4 Aw = __builtin_convertvector(A.v, gpu_u16x4);
  gpu_u16x4 Bw = __builtin_convertvector(B.v, gpu_u16x4);

  gpu_u16x4 Mw = __builtin_elementwise_max(Aw, Bw);

  gpu_u8x4 M8 = __builtin_convertvector(Mw, gpu_u8x4);

  gpu_u32_u8x4 R; R.v = M8;

  R.v = __builtin_elementwise_max(A.v, B.v);
  return R.u;
#else
  gpu_u32_lanes A; A.u = a;
  gpu_u32_lanes B; B.u = b;
  gpu_u32_lanes R;
  for (int i = 0; i < 4; ++i) { uint8_t x = A.u8[i], y = B.u8[i]; R.u8[i] = (x > y) ? x : y; }
  return R.u;
#endif
}

/* per-halfword signed max */
GPUHD uint32_t gpu_vmaxs2(uint32_t a, uint32_t b) {
#if defined(__CUDA_ARCH__)
  return __vmaxs2(a, b);
#elif defined(__HIP_DEVICE_COMPILE__)
  gpu_u32_i16x2 A; A.u = a;
  gpu_u32_i16x2 B; B.u = b;
  gpu_u32_i16x2 R;
  R.v = __builtin_elementwise_max(A.v, B.v);
  return R.u;
#else
  gpu_u32_lanes A; A.u = a;
  gpu_u32_lanes B; B.u = b;
  gpu_u32_lanes R;
  for (int i = 0; i < 2; ++i) {
    int16_t x = A.s16[i], y = B.s16[i];
    R.s16[i] = (x > y) ? x : y;
  }
  return R.u;
#endif
}

/* per-byte signed saturating subtract: clamp(a - b) to [-128,127] */
GPUHD uint32_t gpu_vsubss4(uint32_t a, uint32_t b) {
#if defined(__CUDA_ARCH__)
  return __vsubss4(a, b);
#elif defined(__HIP_DEVICE_COMPILE__)
  /* Widen to i16x4, subtract, clamp, narrow back.
   *      This avoids relying on add_sat(a, -b) which can overflow on negation. */
  gpu_u32_i8x4 A8; A8.u = a;
  gpu_u32_i8x4 B8; B8.u = b;

  gpu_i16x4 A16 = __builtin_convertvector(A8.v, gpu_i16x4);
  gpu_i16x4 B16 = __builtin_convertvector(B8.v, gpu_i16x4);

  gpu_i16x4 D = A16 - B16;

  /* clamp to [-128, 127] in i16 lanes */
  const gpu_i16x4 lo = (gpu_i16x4){ -128, -128, -128, -128 };
  const gpu_i16x4 hi = (gpu_i16x4){  127,  127,  127,  127 };
  D = __builtin_elementwise_max(D, lo);
  D = __builtin_elementwise_min(D, hi);

  gpu_i8x4 R8 = __builtin_convertvector(D, gpu_i8x4);

  gpu_u32_u8x4 R;
  R.v = (gpu_u8x4)R8;
  return R.u;
#else
  gpu_u32_lanes A; A.u = a;
  gpu_u32_lanes B; B.u = b;
  gpu_u32_lanes R;
  for (int i = 0; i < 4; ++i) {
    int t = (int)A.s8[i] - (int)B.s8[i];
    if (t >  127) t =  127;
    if (t < -128) t = -128;
    R.s8[i] = (int8_t)t;
  }
  return R.u;
#endif
}

GPUHD uint32_t gpu_vaddss2(uint32_t a, uint32_t b) {
#if defined(__CUDA_ARCH__)
  return __vaddss2(a, b);
#elif defined(__HIP_DEVICE_COMPILE__)
  gpu_u32_i16x2 A; A.u = a;
  gpu_u32_i16x2 B; B.u = b;
  gpu_u32_i16x2 R;
  R.v = __builtin_elementwise_add_sat(A.v, B.v);
  return R.u;
#else
  gpu_u32_lanes A; A.u = a;
  gpu_u32_lanes B; B.u = b;
  gpu_u32_lanes R;
  for (int i = 0; i < 2; ++i) {
    int t = (int)A.s16[i] + (int)B.s16[i];
    if (t >  32767) t =  32767;
    if (t < -32768) t = -32768;
    R.s16[i] = (int16_t)t;
  }
  return R.u;
#endif
}

/* per-halfword subtract (wrap-around): a - b */
GPUHD uint32_t gpu_vsub2(uint32_t a, uint32_t b) {
#if defined(__CUDA_ARCH__)
  return __vsub2(a, b);
#elif defined(__HIP_DEVICE_COMPILE__)
  gpu_u32_i16x2 A; A.u = a;
  gpu_u32_i16x2 B; B.u = b;
  gpu_u32_i16x2 R;
  R.v = A.v - B.v;
  return R.u;
#else
  gpu_u32_lanes A; A.u = a;
  gpu_u32_lanes B; B.u = b;
  gpu_u32_lanes R;
  for (int i = 0; i < 2; ++i) {
   /* wrap-around in int16_t */
    R.s16[i] = (int16_t)((int)A.s16[i] - (int)B.s16[i]);
  }
  return R.u;
#endif
}
