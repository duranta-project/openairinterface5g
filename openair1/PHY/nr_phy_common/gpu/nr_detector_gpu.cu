/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// GPU 2-layer detectors (nr_detector_gpu.h). Each thread follows one SIMD lane of the fp32 CPU kernels in
// nr_lbest_simd.c operation by operation; with contraction off (--fmad=false / -ffp-contract=off), IEEE division and
// round-to-nearest-even conversions the LLRs are bit-identical to the CPU ones.

#include "PHY/gpu_compat.h"
#include "PHY/gpu_simd_intrin_compat.h" // GPU_SHFL_ALL_LANES
#include "nr_detector_gpu.h"
#include "nr_compute_llr.h"

// ch_mag -> Pp of the L-best seeds (Q14), as NR_LBEST_Q_SQRT42_OVER4_Q14 / NR_LBEST_Q_SQRT170_OVER8_Q14_SIMD
#define NRG_SEED_Q14_64 26545
#define NRG_SEED_Q14_256 26705
#define NRG_THREADS 128

struct nr_gpu_det_ws {
  uint32_t *idx; // REs searched in full by the adaptive window
  uint32_t *cnt;
  uint32_t max_re;
};

namespace {

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

// pass 1 of the L-best: the 3x3 window, and the list of the REs below NR_LBEST_WIDE_SINR* for the full grid
template <int NB>
__global__ void k_lbest(const c16_t *s0,
                        const c16_t *s1,
                        const c16_t *cm0,
                        const c16_t *cm1,
                        int16_t *out,
                        const c16_t *rho,
                        uint32_t length,
                        float lam,
                        uint32_t *idx,
                        uint32_t *cnt)
{
  const uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= length)
    return;
  const Re r = load_re(s0, s1, cm0, cm1, rho, i);
  const float psc = Qam<NB>::sqK() / (float)(Qam<NB>::M / 2);
  if (nr_lbest_wide_re((float)r.cm0 * psc, (float)r.cm1 * psc, r.rr, r.ri, lam, NB)) {
    idx[atomicAdd(cnt, 1u)] = i;
    return;
  }
  float cI, cQ;
  int16_t axis[2 * NB], llr[2 * NB];
  bool uns[2 * NB];
  seed<NB>(r, lam, 0.5f * NR_2LAYER_CSI_EPS, &cI, &cQ, axis);
  grid3<NB>(r, cI, cQ, llr, uns);
#pragma unroll
  for (int p = 0; p < 2 * NB; p++)
    llr[p] = uns[p] ? axis[p] : llr[p];
  store<NB>(out, i, llr);
}

// Full grid (full ML, and pass 2 of the L-best on the listed REs), L lanes per RE: lane l evaluates the rows (target I
// levels) l, l + L, ..., the column and per-bit maxima are combined by shuffles (max: order-independent, exact).
// Every bit is spanned (no seed fallback). idx == NULL: RE j directly, length REs; else the *cnt REs idx[j].
template <int NB, int L>
__global__ void k_grid(const c16_t *s0,
                       const c16_t *s1,
                       const c16_t *cm0,
                       const c16_t *cm1,
                       int16_t *out,
                       const c16_t *rho,
                       uint32_t length,
                       const uint32_t *idx,
                       const uint32_t *cnt)
{
  typedef Qam<NB> Q;
  constexpr int M = Q::M, RPB = NRG_THREADS / L;
  const uint32_t n = idx ? *cnt : length;
  const uint32_t j0 = blockIdx.x * RPB;
  if (j0 >= n) // block-uniform: whole warps leave, the shuffles below see full warps
    return;
  const uint32_t j = j0 + threadIdx.x / L;
  const bool valid = j < n;
  const uint32_t i = idx ? idx[valid ? j : j0] : (valid ? j : j0);
  const int l = threadIdx.x % L;
  const Re r = load_re(s0, s1, cm0, cm1, rho, i);
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
  int16_t llr[2 * NB];
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
  if (valid) // every lane holds all the LLRs of its RE: lane l stores LLRs l, l + L, ...
#pragma unroll
    for (int p = 0; p < 2 * NB; p++)
      if (p % L == l)
        out[(size_t)i * 2 * NB + p] = llr[p];
}

template <int NB, int L>
void launch_grid(const c16_t *s0,
                 const c16_t *s1,
                 const c16_t *cm0,
                 const c16_t *cm1,
                 int16_t *out,
                 const c16_t *rho,
                 uint32_t length,
                 const uint32_t *idx,
                 const uint32_t *cnt,
                 gpuStream_t st)
{
  constexpr int RPB = NRG_THREADS / L;
  k_grid<NB, L><<<(length + RPB - 1) / RPB, NRG_THREADS, 0, st>>>(s0, s1, cm0, cm1, out, rho, length, idx, cnt);
}

// Lanes per RE of the full grid. Full ML (every RE) is throughput-bound: one lane (AMD) or two (NVIDIA). Pass 2 of
// the L-best (a few % of the REs) is latency-bound: four lanes cut it 1.5-1.7x on Radeon 8060S, GH200 and GB10.
#if defined(GPU_USE_HIP)
#define NRG_LANES_FULL 1
#else
#define NRG_LANES_FULL 2
#endif
#define NRG_LANES_WIDE 4

template <int NB>
void run_grid(int lanes,
              const c16_t *s0,
              const c16_t *s1,
              const c16_t *cm0,
              const c16_t *cm1,
              int16_t *out,
              const c16_t *rho,
              uint32_t length,
              const uint32_t *idx,
              const uint32_t *cnt,
              gpuStream_t st)
{
  switch (lanes) {
    case 1:
      launch_grid<NB, 1>(s0, s1, cm0, cm1, out, rho, length, idx, cnt, st);
      break;
    case 2:
      launch_grid<NB, 2>(s0, s1, cm0, cm1, out, rho, length, idx, cnt, st);
      break;
    default:
      launch_grid<NB, 4>(s0, s1, cm0, cm1, out, rho, length, idx, cnt, st);
      break;
  }
}

// 2-layer MMSE (nr_llr_2layer_mmse_simd): unbiased estimate, piecewise-linear per-axis LLRs (nrlbw_pam_llr)
template <int NB>
__global__ void k_mmse(const c16_t *z0,
                       const c16_t *z1,
                       const c16_t *g00,
                       const c16_t *g11,
                       const c16_t *rho,
                       int16_t *out,
                       uint32_t length,
                       float lam)
{
  const uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= length)
    return;
  const float SQ = NB == 1 ? 1.4142135623730951f : NB == 2 ? 3.1622776601683795f : NB == 3 ? 6.48074069840786f : 13.038404810405298f;
  const float GK = 2.0f / (NB == 1 ? 2.0f : NB == 2 ? 10.0f : NB == 3 ? 42.0f : 170.0f);
  const float M = (float)(1 << NB);
  const float epsh = 0.5f * NR_2LAYER_CSI_EPS;
  const c16_t a = z0[i], b = z1[i], p = rho[i];
  const float z0r = a.r, z0i = a.i, z1r = b.r, z1i = b.i, rr = p.r, ri = p.i;
  const float P0 = g00[i].r, P1 = g11[i].r;
  const float P1s = (P1 + lam) + epsh * (P0 + P1);
  const float det = fmaxf(P0 * P1s - (rr * rr + ri * ri), 1.0f);
  const float numr = P1s * z0r - (rr * z1r - ri * z1i);
  const float numi = P1s * z0i - (rr * z1i + ri * z1r);
  const float est[2] = {(numr / det) * SQ, (numi / det) * SQ};
  const float g = GK * (det / fmaxf(P1s, 1.0f));
  int16_t llr[2 * NB];
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
  store<NB>(out, i, llr);
}

inline int launch_ok()
{
  return gpuGetLastError() == gpuSuccess ? 0 : -1;
}

} // namespace

extern "C" nr_gpu_det_ws_t *nr_gpu_det_ws_alloc(uint32_t max_re)
{
  nr_gpu_det_ws_t *ws = (nr_gpu_det_ws_t *)calloc(1, sizeof(*ws));
  if (!ws)
    return NULL;
  ws->max_re = max_re;
  if (gpuMalloc((void **)&ws->idx, (size_t)max_re * sizeof(uint32_t)) != gpuSuccess
      || gpuMalloc((void **)&ws->cnt, sizeof(uint32_t)) != gpuSuccess) {
    nr_gpu_det_ws_free(ws);
    return NULL;
  }
  return ws;
}

extern "C" void nr_gpu_det_ws_free(nr_gpu_det_ws_t *ws)
{
  if (!ws)
    return;
  (void)gpuFree(ws->idx);
  (void)gpuFree(ws->cnt);
  free(ws);
}

extern "C" int nr_gpu_llr_2layer_lbest(const c16_t *stream0_in,
                                       const c16_t *stream1_in,
                                       const c16_t *ch_mag,
                                       const c16_t *ch_mag_i,
                                       int16_t *out,
                                       const c16_t *rho01,
                                       uint32_t length,
                                       int mod_order,
                                       int pattern,
                                       float lambda,
                                       nr_gpu_det_ws_t *ws,
                                       void *stream)
{
  if ((mod_order != 6 && mod_order != 8) || length == 0 || (pattern != 3 && (!ws || length > ws->max_re)))
    return -1;
  gpuStream_t st = (gpuStream_t)stream;
  const unsigned nblk = (length + NRG_THREADS - 1) / NRG_THREADS;
  if (pattern == 3) {
    if (mod_order == 6)
      run_grid<3>(NRG_LANES_FULL, stream0_in, stream1_in, ch_mag, ch_mag_i, out, rho01, length, NULL, NULL, st);
    else
      run_grid<4>(NRG_LANES_FULL, stream0_in, stream1_in, ch_mag, ch_mag_i, out, rho01, length, NULL, NULL, st);
    return launch_ok();
  }
  (void)gpuMemsetAsync(ws->cnt, 0, sizeof(uint32_t), st);
  if (mod_order == 6) {
    k_lbest<3><<<nblk, NRG_THREADS, 0, st>>>(stream0_in, stream1_in, ch_mag, ch_mag_i, out, rho01, length, lambda, ws->idx, ws->cnt);
    run_grid<3>(NRG_LANES_WIDE, stream0_in, stream1_in, ch_mag, ch_mag_i, out, rho01, length, ws->idx, ws->cnt, st);
  } else {
    k_lbest<4><<<nblk, NRG_THREADS, 0, st>>>(stream0_in, stream1_in, ch_mag, ch_mag_i, out, rho01, length, lambda, ws->idx, ws->cnt);
    run_grid<4>(NRG_LANES_WIDE, stream0_in, stream1_in, ch_mag, ch_mag_i, out, rho01, length, ws->idx, ws->cnt, st);
  }
  return launch_ok();
}

extern "C" int nr_gpu_llr_2layer_mmse(const c16_t *z0,
                                      const c16_t *z1,
                                      const c16_t *g00,
                                      const c16_t *g11,
                                      const c16_t *rho01,
                                      int16_t *out,
                                      uint32_t length,
                                      int mod_order,
                                      float lambda,
                                      void *stream)
{
  if (length == 0)
    return -1;
  gpuStream_t st = (gpuStream_t)stream;
  const unsigned nblk = (length + NRG_THREADS - 1) / NRG_THREADS;
  switch (mod_order) {
    case 2:
      k_mmse<1><<<nblk, NRG_THREADS, 0, st>>>(z0, z1, g00, g11, rho01, out, length, lambda);
      break;
    case 4:
      k_mmse<2><<<nblk, NRG_THREADS, 0, st>>>(z0, z1, g00, g11, rho01, out, length, lambda);
      break;
    case 6:
      k_mmse<3><<<nblk, NRG_THREADS, 0, st>>>(z0, z1, g00, g11, rho01, out, length, lambda);
      break;
    case 8:
      k_mmse<4><<<nblk, NRG_THREADS, 0, st>>>(z0, z1, g00, g11, rho01, out, length, lambda);
      break;
    default:
      return -1;
  }
  return launch_ok();
}
