/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// Slot-batched GPU 2-layer inner RX (nr_detector_gpu.h): the integer front end of nr_channel_compensation, the
// detectors of nr_detector_gpu_dev.cuh, the LLR output shifts, layer demap and descrambling, one thread per RE and
// symbol (blockIdx.y). The integer helpers follow the SSE/AVX helpers of sse_intrin.h lane by lane (wrapping madd,
// arithmetic shift, saturating pack), so the front end is bit-exact with the CPU. Every int16 wrap-around is
// computed explicitly in int32 (wrap16): nvcc keeps an out-of-range int16 result (e.g. -(-32768)) unwrapped in its
// 32-bit register, so a conversion to int16_t alone does not reproduce the SIMD wrap.

#include "nr_detector_gpu.h"
#include "nr_detector_gpu_dev.cuh"
#include "nr_detector_gpu_ws.h"

// QAM16_n1, QAM64_n1/n2, QAM256_n1/n2/n3 of PHY/impl_defs_top.h (magnitude scaling of nr_channel_compensation)
#define NRG_QAM16_N1 20724
#define NRG_QAM64_N1 20225
#define NRG_QAM64_N2 10112
#define NRG_QAM256_N1 20106
#define NRG_QAM256_N2 10053
#define NRG_QAM256_N3 5026

using namespace nrg;

namespace {


// a0 b0 + a1 b1 (int16 operands) in int32 with the wrap-around of pmaddwd
__device__ __forceinline__ int32_t madd(int32_t a0, int32_t b0, int32_t a1, int32_t b1)
{
  return (int32_t)((uint32_t)(a0 * b0) + (uint32_t)(a1 * b1));
}

// conj(a) b >> s, saturated (oai_mm_cpx_mult_conj)
__device__ __forceinline__ c16_t cmul_conj(c16_t a, c16_t b, int s)
{
  return (c16_t){sat16(madd(a.r, b.r, a.i, b.i) >> s), sat16(madd(neg16(a.i), b.r, a.r, b.i) >> s)};
}

// a b >> s, saturated (oai_mm_cpx_mult)
__device__ __forceinline__ c16_t cmul(c16_t a, c16_t b, int s)
{
  return (c16_t){sat16(madd(a.r, b.r, neg16(a.i), b.i) >> s), sat16(madd(a.i, b.r, a.r, b.i) >> s)};
}

// multiply by the zero-phase CPE (32767, 0): x - 1 for x > 0, max(x, -32767) otherwise (nr_cpe_id_*)
__device__ __forceinline__ int16_t cpe_id(int16_t x)
{
  return x > 0 ? (int16_t)(x - 1) : (int16_t)max((int32_t)x, -32767);
}

__device__ __forceinline__ int16_t mulhrs(int32_t a, int32_t b)
{
  return (int16_t)wrap16((a * b + 0x4000) >> 15);
}

__device__ __forceinline__ c16_t add16(c16_t a, c16_t b)
{
  return (c16_t){(int16_t)wrap16(a.r + b.r), (int16_t)wrap16(a.i + b.i)};
}

__device__ __forceinline__ c16_t adds16(c16_t a, c16_t b)
{
  return (c16_t){sat16(a.r + b.r), sat16(a.i + b.i)};
}

struct Fe {
  c16_t comp[2], rho01, rho10, rho00, rho11;
  int16_t mag[2];
};

// MRC over the Rx antennas in nr_channel_compensation's order: matched filter with the CPE-rotated channel (wrapping
// sum), magnitude |h|^2 >> s scaled by the QAM amplitude (wrapping sum), Gram terms from the raw channel (saturating)
template <bool MAG, bool DIAG>
// rxF [nb_rx][stride], or rx_ant[aarx] (rxdataF) at re_map[i] when rx_ant; chF [2][nb_rx][ch_stride] at re_map[i]
// (re_map NULL: the identity)
__device__ __forceinline__ Fe front_end(const c16_t *const *rx_ant,
                                        const c16_t *rxF,
                                        const c16_t *chF,
                                        const uint16_t *re_map,
                                        uint32_t stride,
                                        uint32_t ch_stride,
                                        uint32_t i,
                                        int nb_rx,
                                        c16_t cpe,
                                        int s,
                                        int16_t amp)
{
  const bool rot = !(cpe.r == 32767 && cpe.i == 0);
  Fe f;
  for (int aarx = 0; aarx < nb_rx; aarx++) {
    const uint32_t j = re_map ? re_map[i] : i;
    const c16_t y = rx_ant ? rx_ant[aarx][j] : rxF[(size_t)aarx * stride + i];
    const c16_t c[2] = {chF[(size_t)aarx * ch_stride + j], chF[(size_t)(nb_rx + aarx) * ch_stride + j]};
    c16_t t[2];
    int16_t m[2];
#pragma unroll
    for (int l = 0; l < 2; l++) {
      const c16_t cc = rot ? cmul(c[l], cpe, 15) : (c16_t){cpe_id(c[l].r), cpe_id(c[l].i)};
      t[l] = cmul_conj(cc, y, s);
      if (MAG)
        m[l] = mulhrs(sat16(madd(c[l].r, c[l].r, c[l].i, c[l].i) >> s), amp);
    }
    const c16_t r01 = cmul_conj(c[0], c[1], s), r10 = cmul_conj(c[1], c[0], s);
    c16_t r00, r11;
    if (DIAG) {
      r00 = cmul_conj(c[0], c[0], s);
      r11 = cmul_conj(c[1], c[1], s);
    }
    if (aarx == 0) {
      f.comp[0] = t[0];
      f.comp[1] = t[1];
      f.rho01 = r01;
      f.rho10 = r10;
      if (MAG) {
        f.mag[0] = m[0];
        f.mag[1] = m[1];
      }
      if (DIAG) {
        f.rho00 = r00;
        f.rho11 = r11;
      }
    } else {
      f.comp[0] = add16(f.comp[0], t[0]);
      f.comp[1] = add16(f.comp[1], t[1]);
      f.rho01 = adds16(f.rho01, r01);
      f.rho10 = adds16(f.rho10, r10);
      if (MAG) {
        f.mag[0] = (int16_t)wrap16(f.mag[0] + m[0]);
        f.mag[1] = (int16_t)wrap16(f.mag[1] + m[1]);
      }
      if (DIAG) {
        f.rho00 = adds16(f.rho00, r00);
        f.rho11 = adds16(f.rho11, r11);
      }
    }
  }
  return f;
}

// detector input of target layer t
__device__ __forceinline__ Re re_of(const Fe &f, int t)
{
  const c16_t z1 = f.comp[t], z2 = f.comp[1 - t], rho = t ? f.rho10 : f.rho01;
  Re r;
  r.z1r = z1.r;
  r.z1i = z1.i;
  r.z2r = z2.r;
  r.z2i = z2.i;
  r.rr = rho.r;
  r.ri = rho.i;
  r.cm0 = f.mag[t];
  r.cm1 = f.mag[1 - t];
  return r;
}

// LLR p of layer t of RE i into the codeword slice: output shift, then descrambling (int16 multiply by +-1)
__device__ __forceinline__ void store_cw(const nr_gpu_rx_sym_t &s, uint32_t i, int t, int qm, int p, int16_t v, int shift)
{
  const size_t k = ((size_t)i * 2 + t) * qm + p;
  v = (int16_t)(v >> shift);
  if (s.scramble) {
    const uint32_t n = s.scramble_offset + (uint32_t)k;
    v = (s.scramble[n >> 5] >> (n & 31)) & 1 ? (int16_t)wrap16(-v) : v; // the int16 multiply by -1 of the CPU
  }
  s.llr_cw[k] = v;
}

// output shifts of nr_compute_MMSE_llr / nr_compute_ML_llr (shared nr_ml_llr_maxh_off table)
__host__ __device__ constexpr int out_shift(int nb)
{
  return nb == 1 ? 4 : nb == 3 ? 1 : 0; // MMSE and ML alike
}

__host__ __device__ constexpr int16_t qam_amp(int nb)
{
  return nb == 2 ? NRG_QAM16_N1 : nb == 3 ? NRG_QAM64_N1 : nb == 4 ? NRG_QAM256_N1 : 0;
}

__global__ void k_compensation(const c16_t *rxF, const c16_t *chF, uint32_t stride, uint32_t length, int nb_rx, c16_t cpe, int mod_order, int shift, c16_t *o0, c16_t *o1, c16_t *o2, c16_t *o3, c16_t *o4, c16_t *o5, c16_t *o6, c16_t *o7)
{
  const uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= length)
    return;
  const Fe f = front_end<true, true>(NULL, rxF, chF, NULL, stride, stride, i, nb_rx, cpe, shift, qam_amp(mod_order / 2));
  o0[i] = f.comp[0];
  o1[i] = f.comp[1];
  if (mod_order > 2) {
    o2[i] = (c16_t){f.mag[0], f.mag[0]};
    o3[i] = (c16_t){f.mag[1], f.mag[1]};
  }
  o4[i] = f.rho00;
  o5[i] = f.rho01;
  o6[i] = f.rho10;
  o7[i] = f.rho11;
}

// pass 1: front end + detector of both layers; L-best REs below NR_LBEST_WIDE_SINR* go to the list as
// (symbol << 17 | layer << 16 | RE) for pass 2
template <int NB, int DET>
__global__ void k_inner_rx(const nr_gpu_rx_sym_t *b, int nb_rx, int shift, float lam, uint32_t *idx, uint32_t *cnt)
{
  const nr_gpu_rx_sym_t &s = b[blockIdx.y];
  const uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= s.length)
    return;
  const Fe f = front_end<DET != NR_GPU_DET_MMSE, DET == NR_GPU_DET_MMSE>(s.rx_ant[0] ? s.rx_ant : NULL, s.rxF, s.chF, s.re_map, s.stride, s.ch_stride, i, nb_rx, s.cpe, shift, qam_amp(NB));
#pragma unroll
  for (int t = 0; t < 2; t++) {
    int16_t llr[2 * NB];
    if constexpr (DET == NR_GPU_DET_MMSE) {
      mmse_re<NB>(f.comp[t], f.comp[1 - t], (t ? f.rho11 : f.rho00).r, (t ? f.rho00 : f.rho11).r, t ? f.rho10 : f.rho01, lam, llr);
    } else if constexpr (NB == 1) { // QPSK / 16QAM: exact ML for L-best and ML alike, as nr_compute_ML_llr
      qpsk_2layer_re(f.comp[t], f.comp[1 - t], t ? f.rho10 : f.rho01, llr);
    } else if constexpr (NB == 2) {
      qam16_2layer_re(f.comp[t], f.comp[1 - t], f.mag[t], f.mag[1 - t], t ? f.rho10 : f.rho01, llr);
    } else {
      const Re r = re_of(f, t);
      if (DET == NR_GPU_DET_ML) {
        grid_full<NB, 1>(r, 0, llr);
      } else if (lbest_wide<NB>(r, lam)) {
        idx[atomicAdd(cnt, 1u)] = (blockIdx.y << 17) | (t << 16) | i;
        continue;
      } else {
        lbest_window<NB>(r, lam, llr);
      }
    }
#pragma unroll
    for (int p = 0; p < 2 * NB; p++)
      store_cw(s, i, t, 2 * NB, p, llr[p], out_shift(NB));
  }
}

// pass 2 of the L-best: full grid on the listed (symbol, layer, RE), L lanes per entry, front end recomputed
template <int NB, int L>
__global__ void k_inner_rx_wide(const nr_gpu_rx_sym_t *b, int nb_rx, int shift, const uint32_t *idx, const uint32_t *cnt)
{
  constexpr int RPB = NRG_THREADS / L;
  const uint32_t n = *cnt, j0 = blockIdx.x * RPB;
  if (j0 >= n) // block-uniform: whole warps leave, the shuffles see full warps
    return;
  const uint32_t j = j0 + threadIdx.x / L;
  const bool valid = j < n;
  const uint32_t e = idx[valid ? j : j0];
  const nr_gpu_rx_sym_t &s = b[e >> 17];
  const int t = (e >> 16) & 1, l = threadIdx.x % L;
  const uint32_t i = e & 0xffff;
  const Fe f = front_end<true, false>(s.rx_ant[0] ? s.rx_ant : NULL, s.rxF, s.chF, s.re_map, s.stride, s.ch_stride, i, nb_rx, s.cpe, shift, qam_amp(NB));
  int16_t llr[2 * NB];
  grid_full<NB, L>(re_of(f, t), l, llr);
  if (valid)
#pragma unroll
    for (int p = 0; p < 2 * NB; p++)
      if (p % L == l)
        store_cw(s, i, t, 2 * NB, p, llr[p], out_shift(NB));
}

// Single layer: MRC of nr_channel_compensation (nb_layers = 1) with the magnitudes |h|^2 >> s of the NB - 1 QAM
// thresholds (a, b, c), wrapping sums over the Rx antennas
template <int NB>
__device__ __forceinline__ void front_end_1layer(const nr_gpu_rx_sym_t &s, uint32_t i, int nb_rx, int sh, c16_t &comp, int16_t mag[3])
{
  constexpr int16_t amp[3] = {NB == 2 ? NRG_QAM16_N1 : NB == 3 ? NRG_QAM64_N1 : NRG_QAM256_N1,
                              NB == 3 ? NRG_QAM64_N2 : NRG_QAM256_N2,
                              NRG_QAM256_N3};
  const bool rot = !(s.cpe.r == 32767 && s.cpe.i == 0);
  const uint32_t j = s.re_map ? s.re_map[i] : i;
  for (int aarx = 0; aarx < nb_rx; aarx++) {
    const c16_t y = s.rx_ant[0] ? s.rx_ant[aarx][j] : s.rxF[(size_t)aarx * s.stride + i];
    const c16_t c = s.chF[(size_t)aarx * s.ch_stride + j];
    const c16_t cc = rot ? cmul(c, s.cpe, 15) : (c16_t){cpe_id(c.r), cpe_id(c.i)};
    const c16_t t = cmul_conj(cc, y, sh);
    comp = aarx ? add16(comp, t) : t;
    if (NB > 1) {
      const int16_t m = sat16(madd(c.r, c.r, c.i, c.i) >> sh);
#pragma unroll
      for (int k = 0; k < NB - 1; k++)
        mag[k] = aarx ? (int16_t)wrap16(mag[k] + mulhrs(m, amp[k])) : mulhrs(m, amp[k]);
    }
  }
}

// one PAM level of nr_{16,64,256}qam_llr: m - |x|; tail REs (the scalar loop after the 4-RE blocks) take
// saturating_sub's |-32768| = 32768 and +-32767 clamp, the vector blocks protected_abs and the int16 saturation
__device__ __forceinline__ int16_t pam_level(int16_t m, int16_t x, bool tail)
{
  if (tail) {
    const int32_t v = (int32_t)m - abs((int32_t)x);
    return (int16_t)max(min(v, 32767), -32767);
  }
  return sat16((int32_t)m - abs(max((int32_t)x, -32767)));
}

// single-layer inner RX: front end, nr_compute_llr, descrambling into the codeword slice (LLR order per RE: the
// I/Q pairs of each level)
template <int NB>
__global__ void k_inner_rx_1layer(const nr_gpu_rx_sym_t *b, int nb_rx, int shift)
{
  const nr_gpu_rx_sym_t &s = b[blockIdx.y];
  const uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= s.length)
    return;
  c16_t y;
  int16_t mag[3];
  front_end_1layer<NB>(s, i, nb_rx, shift, y, mag);
  int16_t llr[2 * NB];
  if (NB == 1) {
    llr[0] = (int16_t)(y.r >> 4);
    llr[1] = (int16_t)(y.i >> 4);
  } else {
    const bool tail = i >= (s.length & ~3u);
    llr[0] = y.r;
    llr[1] = y.i;
#pragma unroll
    for (int k = 1; k < NB; k++) {
      llr[2 * k] = pam_level(mag[k - 1], llr[2 * k - 2], tail);
      llr[2 * k + 1] = pam_level(mag[k - 1], llr[2 * k - 1], tail);
    }
  }
#pragma unroll
  for (int p = 0; p < 2 * NB; p++) {
    int16_t v = llr[p];
    if (s.scramble) {
      const uint32_t n = s.scramble_offset + i * 2 * NB + p;
      v = (s.scramble[n >> 5] >> (n & 31)) & 1 ? (int16_t)wrap16(-v) : v;
    }
    s.llr_cw[(size_t)i * 2 * NB + p] = v;
  }
}

template <int NB, int DET>
void launch(const nr_gpu_rx_sym_t *b, int nsym, uint32_t maxlen, uint32_t total, int nb_rx, int shift, float lam, nr_gpu_det_ws_t *ws, gpuStream_t st)
{
  const dim3 grid((maxlen + NRG_THREADS - 1) / NRG_THREADS, nsym);
  constexpr bool wide = DET == NR_GPU_DET_LBEST && NB >= 3; // QPSK / 16QAM L-best is the exact ML
  if (wide)
    (void)gpuMemsetAsync(ws->cnt, 0, sizeof(uint32_t), st);
  k_inner_rx<NB, DET><<<grid, NRG_THREADS, 0, st>>>(b, nb_rx, shift, lam, ws ? ws->idx : NULL, ws ? ws->cnt : NULL);
  if constexpr (wide) {
    constexpr int RPB = NRG_THREADS / NRG_LANES_WIDE;
    k_inner_rx_wide<NB, NRG_LANES_WIDE><<<(2 * total + RPB - 1) / RPB, NRG_THREADS, 0, st>>>(b, nb_rx, shift, ws->idx, ws->cnt);
  }
}

} // namespace

extern "C" int nr_gpu_compensation_2layer(const c16_t *rxF,
                                          const c16_t *chF,
                                          uint32_t stride,
                                          uint32_t length,
                                          int nb_rx,
                                          c16_t cpe,
                                          int mod_order,
                                          int output_shift,
                                          c16_t *out[8],
                                          void *stream)
{
  if (length == 0 || nb_rx < 1)
    return -1;
  k_compensation<<<(length + NRG_THREADS - 1) / NRG_THREADS, NRG_THREADS, 0, (gpuStream_t)stream>>>(rxF, chF, stride, length, nb_rx, cpe, mod_order, output_shift, out[0], out[1], out[2], out[3], out[4], out[5], out[6], out[7]);
  return gpuGetLastError() == gpuSuccess ? 0 : -1;
}

extern "C" int nr_gpu_inner_rx_2layer(const nr_gpu_rx_sym_t *sym,
                                      int nsym,
                                      int nb_rx,
                                      int mod_order,
                                      int detector,
                                      float lambda,
                                      int output_shift,
                                      nr_gpu_det_ws_t *ws,
                                      void *stream)
{
  if (nsym < 1 || nsym > NR_GPU_MAX_SYM || nb_rx < 1 || nb_rx > NR_IRX_MAX_RX || !ws)
    return -1;
  uint32_t maxlen = 0, total = 0;
  for (int k = 0; k < nsym; k++) {
    if (sym[k].length >= 65536)
      return -1;
    ws->desc[k] = sym[k];
    maxlen = sym[k].length > maxlen ? sym[k].length : maxlen;
    total += sym[k].length;
  }
  if (maxlen == 0)
    return 0;
  if (detector == NR_GPU_DET_LBEST && mod_order >= 6 && 2 * total > ws->max_re)
    return -1;
  gpuStream_t st = (gpuStream_t)stream;
  const nr_gpu_rx_sym_t *b = ws->desc_d;
#define NRG_L(NB, DET) launch<NB, DET>(b, nsym, maxlen, total, nb_rx, output_shift, lambda, ws, st)
  switch (detector * 16 + mod_order) {
    case NR_GPU_DET_MMSE * 16 + 2:
      NRG_L(1, NR_GPU_DET_MMSE);
      break;
    case NR_GPU_DET_MMSE * 16 + 4:
      NRG_L(2, NR_GPU_DET_MMSE);
      break;
    case NR_GPU_DET_MMSE * 16 + 6:
      NRG_L(3, NR_GPU_DET_MMSE);
      break;
    case NR_GPU_DET_MMSE * 16 + 8:
      NRG_L(4, NR_GPU_DET_MMSE);
      break;
    case NR_GPU_DET_LBEST * 16 + 2:
    case NR_GPU_DET_ML * 16 + 2:
      NRG_L(1, NR_GPU_DET_ML);
      break;
    case NR_GPU_DET_LBEST * 16 + 4:
    case NR_GPU_DET_ML * 16 + 4:
      NRG_L(2, NR_GPU_DET_ML);
      break;
    case NR_GPU_DET_LBEST * 16 + 6:
      NRG_L(3, NR_GPU_DET_LBEST);
      break;
    case NR_GPU_DET_LBEST * 16 + 8:
      NRG_L(4, NR_GPU_DET_LBEST);
      break;
    case NR_GPU_DET_ML * 16 + 6:
      NRG_L(3, NR_GPU_DET_ML);
      break;
    case NR_GPU_DET_ML * 16 + 8:
      NRG_L(4, NR_GPU_DET_ML);
      break;
    default:
      return -1;
  }
#undef NRG_L
  return gpuGetLastError() == gpuSuccess ? 0 : -1;
}

extern "C" int nr_gpu_inner_rx_1layer(const nr_gpu_rx_sym_t *sym, int nsym, int nb_rx, int mod_order, int output_shift, nr_gpu_det_ws_t *ws, void *stream)
{
  if (nsym < 1 || nsym > NR_GPU_MAX_SYM || nb_rx < 1 || nb_rx > NR_IRX_MAX_RX || !ws)
    return -1;
  uint32_t maxlen = 0;
  for (int k = 0; k < nsym; k++) {
    ws->desc[k] = sym[k];
    maxlen = sym[k].length > maxlen ? sym[k].length : maxlen;
  }
  if (maxlen == 0)
    return 0;
  const dim3 grid((maxlen + NRG_THREADS - 1) / NRG_THREADS, nsym);
  gpuStream_t st = (gpuStream_t)stream;
  switch (mod_order) {
    case 2:
      k_inner_rx_1layer<1><<<grid, NRG_THREADS, 0, st>>>(ws->desc_d, nb_rx, output_shift);
      break;
    case 4:
      k_inner_rx_1layer<2><<<grid, NRG_THREADS, 0, st>>>(ws->desc_d, nb_rx, output_shift);
      break;
    case 6:
      k_inner_rx_1layer<3><<<grid, NRG_THREADS, 0, st>>>(ws->desc_d, nb_rx, output_shift);
      break;
    case 8:
      k_inner_rx_1layer<4><<<grid, NRG_THREADS, 0, st>>>(ws->desc_d, nb_rx, output_shift);
      break;
    default:
      return -1;
  }
  return gpuGetLastError() == gpuSuccess ? 0 : -1;
}
