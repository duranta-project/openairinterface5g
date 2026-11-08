/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// GPU 2-layer detectors on compensated inputs (nr_detector_gpu.h); per-RE math in nr_detector_gpu_dev.cuh.

#include "nr_detector_gpu.h"
#include "nr_detector_gpu_dev.cuh"
#include "nr_detector_gpu_ws.h"

using namespace nrg;

namespace {

// QPSK / 16QAM: the exact ML of nr_qpsk_llr_2layer / nr_qam16_llr_2layer (raw output, no shift)
template <int NB>
__global__ void k_ml_int16(const c16_t *s0, const c16_t *s1, const c16_t *cm0, const c16_t *cm1, int16_t *out, const c16_t *rho, uint32_t length)
{
  const uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= length)
    return;
  int16_t llr[2 * NB];
  if constexpr (NB == 1)
    qpsk_2layer_re(s0[i], s1[i], rho[i], llr);
  else
    qam16_2layer_re(s0[i], s1[i], cm0[i].r, cm1[i].r, rho[i], llr);
  store<NB>(out, i, llr);
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
  if (lbest_wide<NB>(r, lam)) {
    idx[atomicAdd(cnt, 1u)] = i;
    return;
  }
  int16_t llr[2 * NB];
  lbest_window<NB>(r, lam, llr);
  store<NB>(out, i, llr);
}

// Full grid (full ML, and pass 2 of the L-best on the listed REs), L lanes per RE. idx == NULL: RE j directly,
// length REs; else the *cnt REs idx[j].
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
  constexpr int RPB = NRG_THREADS / L;
  const uint32_t n = idx ? *cnt : length;
  const uint32_t j0 = blockIdx.x * RPB;
  if (j0 >= n) // block-uniform: whole warps leave, the shuffles see full warps
    return;
  const uint32_t j = j0 + threadIdx.x / L;
  const bool valid = j < n;
  const uint32_t i = idx ? idx[valid ? j : j0] : (valid ? j : j0);
  const int l = threadIdx.x % L;
  int16_t llr[2 * NB];
  grid_full<NB, L>(load_re(s0, s1, cm0, cm1, rho, i), l, llr);
  if (valid) // lane l stores LLRs l, l + L, ...
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
  int16_t llr[2 * NB];
  mmse_re<NB>(z0[i], z1[i], g00[i].r, g11[i].r, rho[i], lam, llr);
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
  const size_t dbytes = NR_IRX_MAX_SYM * sizeof(*ws->desc);
#if defined(GPU_USE_HIP)
  const bool desc_ok = hipHostMalloc((void **)&ws->desc, dbytes, hipHostMallocMapped | hipHostMallocNonCoherent) == hipSuccess;
#else
  const bool desc_ok = gpuHostAlloc((void **)&ws->desc, dbytes, gpuHostAllocMapped) == gpuSuccess;
#endif
  if (!desc_ok || gpuHostGetDevicePointer((void **)&ws->desc_d, ws->desc, 0) != gpuSuccess
      || gpuMalloc((void **)&ws->idx, (size_t)max_re * sizeof(uint32_t)) != gpuSuccess
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
  if (ws->desc)
    (void)gpuFreeHost(ws->desc);
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
  gpuStream_t st = (gpuStream_t)stream;
  if ((mod_order == 2 || mod_order == 4) && length > 0) {
    const unsigned nb = (length + NRG_THREADS - 1) / NRG_THREADS;
    if (mod_order == 2)
      k_ml_int16<1><<<nb, NRG_THREADS, 0, st>>>(stream0_in, stream1_in, ch_mag, ch_mag_i, out, rho01, length);
    else
      k_ml_int16<2><<<nb, NRG_THREADS, 0, st>>>(stream0_in, stream1_in, ch_mag, ch_mag_i, out, rho01, length);
    return launch_ok();
  }
  if ((mod_order != 6 && mod_order != 8) || length == 0 || (pattern != 3 && (!ws || length > ws->max_re)))
    return -1;
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
