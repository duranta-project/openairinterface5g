/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * The FP16 vector layer of nr_lbest_f16.c for OpenCL C, one RE per work-item: hv = half, hi = short, hm = int (0 /
 * non-zero). Each operation is the IEEE FP16 operation of the CPU layers, so the detector core gives the LLRs of the
 * ARMv8.2 build. hv_fma is the fused multiply-add (vfmaq_f16); -DNRH_CL_MAD takes mad() instead, for GPUs that
 * emulate the correctly rounded fma() (Adreno: two orders of magnitude slower), at the cost of exactness.
 */

#pragma OPENCL EXTENSION cl_khr_fp16 : enable
// no contraction of separate multiplies and adds: the operations of the CPU layer, one rounding each
#pragma OPENCL FP_CONTRACT OFF

#define NR_2LAYER_CSI_EPS 0.003f
#ifndef NR_LBEST_WIDE_SINR64
#define NR_LBEST_WIDE_SINR64 10.0f
#endif
#ifndef NR_LBEST_WIDE_SINR256
#define NR_LBEST_WIDE_SINR256 20.0f
#endif

// -DNRH_CL_UNROLL: full unrolling of the core's grid loops, a choice per GPU and kernel variant (it keeps the per-
// candidate arrays in registers but grows the code: faster for the 256QAM grid on Adreno, slower for its 64QAM one)
#ifdef NRH_CL_UNROLL
#define NRH_UNROLL _Pragma("unroll")
#endif

// clang-format off
typedef half hv;
typedef short hi;
typedef int hm;

static inline hv hv_set1(float x) { return (hv)x; }
static inline hv hv_add(hv a, hv b) { return a + b; }
static inline hv hv_sub(hv a, hv b) { return a - b; }
static inline hv hv_mul(hv a, hv b) { return a * b; }
// the correctly rounded FP16 quotient (vdivq_f16), which OpenCL's half division need not be: the fp32 quotient (a few
// fp32 ulp off) rounded to FP16 is right unless the exact quotient is that close to a midpoint between two FP16 values;
// the exact sign of a - m b at the midpoints m to both neighbours (m: 12 significant bits, m b exact in fp32) decides,
// ties to even
static inline hv hv_div(hv a, hv b)
{
  const float A = fabs((float)a), B = fabs((float)b), q = A / B;
  const short sign = (as_short(a) ^ as_short(b)) & (short)0x8000;
  short h = as_short(convert_half(q));
  if (isfinite(q) && q > 0.0f && h < 0x7c00) {
    const float fh = (float)as_half(h);
    const float rhi = fma(-0.5f * (fh + (float)as_half((short)(h + 1))), B, A); // A - mid_up B
    const float rlo = fma(-0.5f * (fh + (float)as_half((short)(h - 1))), B, A); // A - mid_down B
    if (rhi > 0.0f || (rhi == 0.0f && (h & 1)))
      h++;
    else if (rlo < 0.0f || (rlo == 0.0f && (h & 1)))
      h--;
  }
  return as_half((short)(h | sign));
}
#ifdef NRH_CL_MAD
static inline hv hv_fma(hv a, hv b, hv c) { return mad(b, c, a); } // a + b c
static inline hv hv_fms(hv a, hv b, hv c) { return mad(-b, c, a); } // a - b c
#else
static inline hv hv_fma(hv a, hv b, hv c) { return fma(b, c, a); }
static inline hv hv_fms(hv a, hv b, hv c) { return fma(-b, c, a); }
#endif
static inline hv hv_max(hv a, hv b) { return fmax(a, b); }
static inline hv hv_min(hv a, hv b) { return fmin(a, b); }
static inline hv hv_abs(hv a) { return fabs(a); }
static inline hv hv_neg(hv a) { return -a; }
static inline hv hv_rndn(hv a) { return rint(a); } // to nearest, ties to even
static inline hm hv_lt(hv a, hv b) { return a < b; }
static inline hm hv_gt(hv a, hv b) { return a > b; }
static inline hm hv_eq(hv a, hv b) { return a == b; }
static inline hm hm_or(hm a, hm b) { return a | b; }
static inline hm hm_andnot(hm a, hm w) { return a & ~w; }
static inline int hm_any(hm a) { return a != 0; }
static inline hv hv_sel(hm m, hv a, hv b) { return m ? a : b; }
static inline hi hi_sel(hm m, hi a, hi b) { return m ? a : b; }
static inline hi hi_set1(short x) { return x; }
static inline hi hi_add(hi a, hi b) { return a + b; }
static inline hi hi_sub(hi a, hi b) { return a - b; }
static inline hi hi_min(hi a, hi b) { return min(a, b); }
static inline hi hi_max(hi a, hi b) { return max(a, b); }
static inline hi hi_shr1(hi a) { return a >> 1; }
static inline hi hi_shl1(hi a) { return a << 1; }
static inline hv hv_cvt(hi a) { return convert_half(a); }
static inline hi hv_expo(hv a) { return as_short(a) >> 10; } // exponent field (a >= 0)
static inline hv hv_pow2(hi k) { return as_half((short)(clamp(k + 15, 1, 30) << 10)); } // 2^k, normal FP16 range
static inline hi hv_trunc16(hv a) { return convert_short_rtz(a); } // toward zero
// lambda 2^k as FP16 (fp32, clamped below the FP16 overflow)
static inline hv hv_lam(float lam, hi k) { return convert_half(fmin(as_float((k + 127) << 23) * lam, 30000.0f)); }
// clang-format on
