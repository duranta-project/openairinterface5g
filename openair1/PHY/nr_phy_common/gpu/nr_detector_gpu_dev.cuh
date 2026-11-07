/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// Per-RE device functions of the GPU 2-layer detectors (nr_detector_gpu.cu, nr_inner_rx_gpu.cu). Each follows one SIMD
// lane of the CPU kernels (nr_lbest_simd.c) operation by operation; with contraction off (--fmad=false /
// -ffp-contract=off), IEEE division and round-to-nearest-even conversions the LLRs are bit-identical to the CPU ones
// built without contraction.

#ifndef NR_DETECTOR_GPU_DEV_CUH
#define NR_DETECTOR_GPU_DEV_CUH

#include "PHY/gpu_compat.h"
#include "PHY/gpu_simd_intrin_compat.h" // GPU_SHFL_ALL_LANES
#include "nr_compute_llr.h"

// ch_mag -> Pp of the L-best seeds (Q14), as NR_LBEST_Q_SQRT42_OVER4_Q14 / NR_LBEST_Q_SQRT170_OVER8_Q14_SIMD
#define NRG_SEED_Q14_64 26545
#define NRG_SEED_Q14_256 26705
#define NRG_THREADS 128
// Lanes per RE of the full grid. Full ML (every RE) is throughput-bound: one lane (AMD) or two (NVIDIA). Pass 2 of
// the L-best (a few % of the REs) is latency-bound: four lanes cut it 1.5-1.7x on Radeon 8060S, GH200 and GB10.
#if defined(GPU_USE_HIP)
#define NRG_LANES_FULL 1
#else
#define NRG_LANES_FULL 2
#endif
#define NRG_LANES_WIDE 4

namespace nrg {


__device__ __forceinline__ float clampf(float x, float lo, float hi)
{
  return fminf(fmaxf(x, lo), hi);
}

// nearest odd integer: 2 * round((x - 1) / 2) + 1
__device__ __forceinline__ float round_odd(float x)
{
  return rintf((x - 1.0f) * 0.5f) * 2.0f + 1.0f;
}

__device__ __forceinline__ float dsq(float lvl, float ref)
{
  const float d = lvl - ref;
  return d * d;
}

__device__ __forceinline__ int16_t cvt_sat(float x)
{
  return (int16_t)__float2int_rn(clampf(x, -32768.0f, 32767.0f));
}

template <int NB>
struct Qam {
  static constexpr int M = 1 << NB;
  static constexpr float Lmax = (float)(M - 1);
  __device__ static float sqK() { return NB == 3 ? 6.48074069840786f : 13.038404810405298f; }
  __device__ static float NA() { return NB == 3 ? 1.0f / 6.48074069840786f : 1.0f / 13.038404810405298f; }
  __device__ static float gK() { return NB == 3 ? 0.5f / 42.0f : 0.5f / 170.0f; }
  static constexpr int32_t q14 = NB == 3 ? NRG_SEED_Q14_64 : NRG_SEED_Q14_256;
};

struct Re {
  float z1r, z1i, z2r, z2i, rr, ri;
  int16_t cm0, cm1;
};

__device__ __forceinline__ Re load_re(const c16_t *s0, const c16_t *s1, const c16_t *cm0, const c16_t *cm1, const c16_t *rho, uint32_t i)
{
  Re r;
  const c16_t a = s0[i], b = s1[i], p = rho[i];
  r.z1r = a.r;
  r.z1i = a.i;
  r.z2r = b.r;
  r.z2i = b.i;
  r.rr = p.r;
  r.ri = p.i;
  r.cm0 = cm0[i].r;
  r.cm1 = cm1[i].r;
  return r;
}

// Gray bit k of a PAM level (nrlbw_bits_ps): b0 = sign, then |L| against halving thresholds
template <int NB>
__device__ __forceinline__ bool level_bit(float lvl, int k)
{
  if (k == 0)
    return lvl < 0.0f;
  float r = fabsf(lvl), t = (float)(1 << (NB - 1));
  bool b = false;
  for (int j = 1; j <= k; j++) {
    b = r > t;
    r = fabsf(r - t);
    t *= 0.5f;
  }
  return b;
}

// MMSE seed (nrlbw_seed / nrlbw_seed256): window centre and graded per-axis LLRs for the unspanned bits
template <int NB>
__device__ __forceinline__ void seed(const Re &r, float lam, float epsh, float *cI, float *cQ, int16_t axis[2 * NB])
{
  typedef Qam<NB> Q;
  const float Pp0 = (float)(((int32_t)r.cm0 * Q::q14) >> 14);
  const float Pp1 = (float)(((int32_t)r.cm1 * Q::q14) >> 14);
  const float Pp1s = (Pp1 + lam) + epsh * (Pp0 + Pp1);
  const float det = fmaxf(Pp0 * Pp1s - (r.rr * r.rr + r.ri * r.ri), 1.0f);
  const float numr = Pp1s * r.z1r - (r.rr * r.z2r - r.ri * r.z2i);
  const float numi = Pp1s * r.z1i - (r.rr * r.z2i + r.ri * r.z2r);
  const float est[2] = {(numr / det) * Q::sqK(), (numi / det) * Q::sqK()};
  const float g = Q::gK() * (det / fmaxf(Pp1s, 1.0f));
  for (int ax = 0; ax < 2; ax++) {
    const float e = est[ax], a = fabsf(e);
    const float o = round_odd(e), oa = round_odd(a);
    float l[4] = {0, 0, 0, 0};
    l[0] = g * (dsq(clampf(o, -Q::Lmax, -1.0f), e) - dsq(clampf(o, 1.0f, Q::Lmax), e));
    if (NB == 3) {
      l[1] = g * (dsq(clampf(oa, 5.0f, 7.0f), a) - dsq(clampf(oa, 1.0f, 3.0f), a));
    } else {
      l[1] = g * (dsq(clampf(oa, 9.0f, 15.0f), a) - dsq(clampf(oa, 1.0f, 7.0f), a));
      const float d2_0 = dsq(clampf(oa, 5.0f, 11.0f), a);
      const float d2_1 = fminf(dsq(clampf(oa, 1.0f, 3.0f), a), dsq(clampf(oa, 13.0f, 15.0f), a));
      l[2] = g * (d2_1 - d2_0);
    }
    for (int b = 0; b < NB; b++)
      axis[2 * b + ax] = cvt_sat(l[b]);
  }
  *cI = clampf(round_odd(est[0]), -Q::Lmax, Q::Lmax);
  *cQ = clampf(round_odd(est[1]), -Q::Lmax, Q::Lmax);
}

// Joint metric of one candidate (x1r, x1i) of the target layer with the conditional ML nuisance (nrlbw_grid_f32):
// Re{x1* z1} - P0 |x1|^2 / 2 + Re{x2* z2} - P1 |x2|^2 / 2 - Re{x1* rho x2}, x2 = Q((z2 - conj(rho) x1) / P1)
template <int NB>
__device__ __forceinline__ float metric(const Re &r, float x1r, float x1i, float hP0, float hP1, float sl)
{
  typedef Qam<NB> Q;
  const float NA = Q::NA();
  float m = (x1r * r.z1r + x1i * r.z1i) - hP0 * (x1r * x1r + x1i * x1i);
  const float cr = r.rr * x1r + r.ri * x1i;
  const float ci = r.rr * x1i - r.ri * x1r;
  const float I2 = clampf(round_odd((r.z2r - cr) * sl), -Q::Lmax, Q::Lmax);
  const float Q2 = clampf(round_odd((r.z2i - ci) * sl), -Q::Lmax, Q::Lmax);
  const float x2r = I2 * NA, x2i = Q2 * NA;
  m = m + ((x2r * r.z2r + x2i * r.z2i) - hP1 * (x2r * x2r + x2i * x2i));
  const float cxr = x1r * x2r + x1i * x2i;
  const float cxi = x1r * x2i - x1i * x2r;
  return m - (r.rr * cxr - r.ri * cxi);
}

// P0 / 2, P1 / 2 and the nuisance slice scale sqrt(K) / P1 (0 where P1 == 0) from the ch_mag units
template <int NB>
__device__ __forceinline__ void grid_terms(const Re &r, float *hP0, float *hP1, float *sl)
{
  typedef Qam<NB> Q;
  const float psc = Q::sqK() / (float)(Q::M / 2);
  const float P0 = (float)r.cm0 * psc, P1 = (float)r.cm1 * psc;
  *hP0 = 0.5f * P0;
  *hP1 = 0.5f * P1;
  *sl = P1 != 0.0f ? Q::sqK() / P1 : 0.0f;
}

// per-bit max-log LLR from the best metrics with the bit at 0 and at 1 (truncation as the CPU kernel)
__device__ __forceinline__ int16_t maxlog_llr(float m0, float m1)
{
  return (int16_t)(int)clampf(m0 - m1, -32768.0f, 32767.0f);
}

// 3x3 window around (cI, cQ), max-log per axis; uns = one side of the bit has no candidate
template <int NB>
__device__ __forceinline__ void grid3(const Re &r, float cI, float cQ, int16_t llr[2 * NB], bool uns[2 * NB])
{
  typedef Qam<NB> Q;
  float hP0, hP1, sl;
  grid_terms<NB>(r, &hP0, &hP1, &sl);
  const float NA = Q::NA(), NEG = -1e30f;
  float lvI[3], lvQ[3], rowMax[3], colMax[3];
#pragma unroll
  for (int k = 0; k < 3; k++) {
    lvI[k] = clampf(cI + (float)(2 * k - 2), -Q::Lmax, Q::Lmax);
    lvQ[k] = clampf(cQ + (float)(2 * k - 2), -Q::Lmax, Q::Lmax);
    rowMax[k] = colMax[k] = NEG;
  }
#pragma unroll
  for (int a = 0; a < 3; a++)
#pragma unroll
    for (int b = 0; b < 3; b++) {
      const float m = metric<NB>(r, lvI[a] * NA, lvQ[b] * NA, hP0, hP1, sl);
      rowMax[a] = fmaxf(rowMax[a], m);
      colMax[b] = fmaxf(colMax[b], m);
    }
#pragma unroll
  for (int k = 0; k < NB; k++) {
    float m0I = NEG, m1I = NEG, m0Q = NEG, m1Q = NEG;
#pragma unroll
    for (int a = 0; a < 3; a++) {
      if (level_bit<NB>(lvI[a], k))
        m1I = fmaxf(m1I, rowMax[a]);
      else
        m0I = fmaxf(m0I, rowMax[a]);
      if (level_bit<NB>(lvQ[a], k))
        m1Q = fmaxf(m1Q, colMax[a]);
      else
        m0Q = fmaxf(m0Q, colMax[a]);
    }
    llr[2 * k] = maxlog_llr(m0I, m1I);
    llr[2 * k + 1] = maxlog_llr(m0Q, m1Q);
    uns[2 * k] = m0I < -1e29f || m1I < -1e29f;
    uns[2 * k + 1] = m0Q < -1e29f || m1Q < -1e29f;
  }
}

template <int NB>
__device__ __forceinline__ void store(int16_t *out, uint32_t i, const int16_t v[2 * NB])
{
#pragma unroll
  for (int p = 0; p < 2 * NB; p++)
    out[(size_t)i * 2 * NB + p] = v[p];
}

__device__ __forceinline__ float shfl_max(float v, int s, int width)
{
  return fmaxf(v, __shfl_xor_sync(GPU_SHFL_ALL_LANES, v, s, width));
}

// Full grid of one RE with L lanes (L = 1: one thread): lane l evaluates the rows (target I levels) l, l + L, ...,
// the column and per-bit maxima are combined by shuffles (max: order-independent, exact). Every bit is spanned (no seed
// fallback); every lane returns all the LLRs. With L > 1 all lanes of the warp must call it.
template <int NB, int L>
__device__ __forceinline__ void grid_full(const Re &r, int l, int16_t llr[2 * NB])
{
  typedef Qam<NB> Q;
  constexpr int M = Q::M;
  float hP0, hP1, sl;
  grid_terms<NB>(r, &hP0, &hP1, &sl);
  const float NA = Q::NA(), NEG = -1e30f;
  float col[M], m0I[NB], m1I[NB];
#pragma unroll
  for (int b = 0; b < M; b++)
    col[b] = NEG;
#pragma unroll
  for (int k = 0; k < NB; k++)
    m0I[k] = m1I[k] = NEG;
#pragma unroll 1
  for (int a = l; a < M; a += L) {
    const float lvI = (float)(2 * a - (M - 1)), x1r = lvI * NA;
    float rmax = NEG;
#pragma unroll
    for (int b = 0; b < M; b++) {
      const float m = metric<NB>(r, x1r, (float)(2 * b - (M - 1)) * NA, hP0, hP1, sl);
      rmax = fmaxf(rmax, m);
      col[b] = fmaxf(col[b], m);
    }
#pragma unroll
    for (int k = 0; k < NB; k++) {
      if (level_bit<NB>(lvI, k))
        m1I[k] = fmaxf(m1I[k], rmax);
      else
        m0I[k] = fmaxf(m0I[k], rmax);
    }
  }
  if (L > 1) {
#pragma unroll
    for (int s = L / 2; s > 0; s >>= 1) {
#pragma unroll
      for (int b = 0; b < M; b++)
        col[b] = shfl_max(col[b], s, L);
#pragma unroll
      for (int k = 0; k < NB; k++) {
        m0I[k] = shfl_max(m0I[k], s, L);
        m1I[k] = shfl_max(m1I[k], s, L);
      }
    }
  }
#pragma unroll
  for (int k = 0; k < NB; k++) {
    float m0Q = NEG, m1Q = NEG;
#pragma unroll
    for (int b = 0; b < M; b++) {
      if (level_bit<NB>((float)(2 * b - (M - 1)), k))
        m1Q = fmaxf(m1Q, col[b]);
      else
        m0Q = fmaxf(m0Q, col[b]);
    }
    llr[2 * k] = maxlog_llr(m0I[k], m1I[k]);
    llr[2 * k + 1] = maxlog_llr(m0Q, m1Q);
  }
}

// 3x3 window with the seed fallback for the unspanned bits (L-best pass 1, the REs above NR_LBEST_WIDE_SINR*)
template <int NB>
__device__ __forceinline__ void lbest_window(const Re &r, float lam, int16_t llr[2 * NB])
{
  float cI, cQ;
  int16_t axis[2 * NB];
  bool uns[2 * NB];
  seed<NB>(r, lam, 0.5f * NR_2LAYER_CSI_EPS, &cI, &cQ, axis);
  grid3<NB>(r, cI, cQ, llr, uns);
#pragma unroll
  for (int p = 0; p < 2 * NB; p++)
    llr[p] = uns[p] ? axis[p] : llr[p];
}

// NR_LBEST_WIDE_SINR* test in the ch_mag units of the L-best inputs
template <int NB>
__device__ __forceinline__ bool lbest_wide(const Re &r, float lam)
{
  const float psc = Qam<NB>::sqK() / (float)(Qam<NB>::M / 2);
  return nr_lbest_wide_re((float)r.cm0 * psc, (float)r.cm1 * psc, r.rr, r.ri, lam, NB);
}

// 2-layer MMSE of one RE (nr_llr_2layer_mmse_simd): unbiased estimate, piecewise-linear per-axis LLRs (nrlbw_pam_llr).
// z0 / P0 target layer, z1 / P1 other layer (P = Gram diagonal), rho = h0^H h1.
template <int NB>
__device__ __forceinline__ void mmse_re(c16_t z0, c16_t z1, int16_t P0i, int16_t P1i, c16_t rho, float lam, int16_t llr[2 * NB])
{
  const float SQ = NB == 1 ? 1.4142135623730951f : NB == 2 ? 3.1622776601683795f : NB == 3 ? 6.48074069840786f : 13.038404810405298f;
  const float GK = 2.0f / (NB == 1 ? 2.0f : NB == 2 ? 10.0f : NB == 3 ? 42.0f : 170.0f);
  const float M = (float)(1 << NB);
  const float epsh = 0.5f * NR_2LAYER_CSI_EPS;
  const float z0r = z0.r, z0i = z0.i, z1r = z1.r, z1i = z1.i, rr = rho.r, ri = rho.i;
  const float P0 = P0i, P1 = P1i;
  const float P1s = (P1 + lam) + epsh * (P0 + P1);
  const float det = fmaxf(P0 * P1s - (rr * rr + ri * ri), 1.0f);
  const float numr = P1s * z0r - (rr * z1r - ri * z1i);
  const float numi = P1s * z0i - (rr * z1i + ri * z1r);
  const float est[2] = {(numr / det) * SQ, (numi / det) * SQ};
  const float g = GK * (det / fmaxf(P1s, 1.0f));
#pragma unroll
  for (int ax = 0; ax < 2; ax++) {
    float v = est[ax];
    llr[ax] = cvt_sat(g * v);
    if (NB >= 2) {
      v = M / 2 - fabsf(v);
      llr[2 + ax] = cvt_sat(g * v);
    }
    if (NB >= 3) {
      v = M / 4 - fabsf(v);
      llr[4 + ax] = cvt_sat(g * v);
    }
    if (NB >= 4) {
      v = M / 8 - fabsf(v);
      llr[6 + ax] = cvt_sat(g * v);
    }
  }
}

} // namespace nrg

#endif
