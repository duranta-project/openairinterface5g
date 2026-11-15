/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * Integer PUSCH inner receivers in OpenCL for the inner-RX module nr_irx_cl.c, built with -DNRH_NB=1..4 (QPSK to
 * 256QAM), bit-exact with the CPU (ports of nr_inner_rx_gpu.cu): single layer, the MRC of nr_channel_compensation
 * (nb_layers 1) and the LLRs of nr_compute_llr (k_inner_rx_1layer); 2 layers QPSK / 16QAM (NRH_NB 1, 2), the
 * compensation of nr_channel_compensation (nb_layers 2) and the exact ML of nr_qpsk_llr_2layer / nr_qam16_llr_2layer
 * (k_inner_rx, qpsk_2layer_re / qam16_2layer_re of nr_detector_gpu_dev.cuh). The int16 helpers follow the SSE helpers
 * of sse_intrin.h lane by lane (wrapping madd, arithmetic shift, saturating pack); every int16 wrap-around is explicit.
 */

// QAM16_n1, QAM64_n1/n2, QAM256_n1/n2/n3 of PHY/impl_defs_top.h (magnitude scaling of nr_channel_compensation)
#define NRG_QAM16_N1 20724
#define NRG_QAM64_N1 20225
#define NRG_QAM64_N2 10112
#define NRG_QAM256_N1 20106
#define NRG_QAM256_N2 10053
#define NRG_QAM256_N3 5026

static inline short wrap16(int x)
{
  return as_short((ushort)(x & 0xffff));
}

static inline short sat16(int x)
{
  return (short)clamp(x, -32768, 32767);
}

static inline short neg16(short x)
{
  return wrap16(-(int)x);
}

// a0 b0 + a1 b1 (int16 operands) in int32 with the wrap-around of pmaddwd
static inline int madd(int a0, int b0, int a1, int b1)
{
  return as_int((uint)(a0 * b0) + (uint)(a1 * b1));
}

// conj(a) b >> s, saturated (oai_mm_cpx_mult_conj)
static inline short2 cmul_conj(short2 a, short2 b, int s)
{
  return (short2)(sat16(madd(a.x, b.x, a.y, b.y) >> s), sat16(madd(neg16(a.y), b.x, a.x, b.y) >> s));
}

// a b >> s, saturated (oai_mm_cpx_mult)
static inline short2 cmul(short2 a, short2 b, int s)
{
  return (short2)(sat16(madd(a.x, b.x, neg16(a.y), b.y) >> s), sat16(madd(a.y, b.x, a.x, b.y) >> s));
}

// multiply by the zero-phase CPE (32767, 0): x - 1 for x > 0, max(x, -32767) otherwise (nr_cpe_id_*)
static inline short cpe_id(short x)
{
  return x > 0 ? (short)(x - 1) : (short)max((int)x, -32767);
}

static inline short mulhrs(int a, int b)
{
  return wrap16((a * b + 0x4000) >> 15);
}

static inline short adds16(int a, int b)
{
  return sat16(a + b);
}

static inline short subs16(int a, int b)
{
  return sat16(a - b);
}

// pmulhw
static inline short mulhi16(int a, int b)
{
  return (short)((a * b) >> 16);
}

// psllw: wraps
static inline short slli16(int x, int k)
{
  return wrap16(x * (1 << k));
}

// pabsw: |-32768| stays -32768
static inline short abs16(short x)
{
  return x == -32768 ? x : (short)(x < 0 ? -x : x);
}

// one PAM level of nr_{16,64,256}qam_llr: m - |x|; tail REs (the scalar loop after the 4-RE blocks) take
// saturating_sub's |-32768| = 32768 and +-32767 clamp, the vector blocks protected_abs and the int16 saturation
static inline short pam_level(short m, short x, int tail)
{
  if (tail)
    return (short)clamp((int)m - (int)abs((int)x), -32767, 32767); // abs: uint in OpenCL
  return sat16((int)m - (int)abs(max((int)x, -32767)));
}

// struct nrh_sym1_t: nrh_sym_t of nr_lbest_f16_kernels.cl
typedef struct {
  uint rx[8]; // RX of antenna a: extracted (direct 0: RE i at rx[a] + i) or the allocation in rxdataF (direct 1: map)
  uint ch; // channel estimates [nb_rx][ch_stride]
  uint map; // allocation RE of each RE (uint16), 0xffffffff: the identity
  uint out; // LLRs [length][Qm]
  uint length;
  uint ch_stride;
  uint scr; // codeword bit of the first LLR, 0xffffffff: no descrambling
  short2 cpe;
  uint direct;
} nrh_sym1_t;

// one work-item per RE and symbol: MRC with the magnitudes |h|^2 >> s of the NB - 1 QAM thresholds (wrapping sums over
// the Rx antennas), the LLRs (per RE the I/Q pairs of each level), descrambling
__kernel void nr_inner_rx_1layer(__global const nrh_sym1_t *sym,
                                 __global const short2 *rx,
                                 __global const short2 *ch,
                                 __global const ushort *map,
                                 __global const uint *scr,
                                 __global short *out,
                                 int nb_rx,
                                 int sh)
{
  __global const nrh_sym1_t *dg = &sym[get_global_id(1)];
  const uint i = get_global_id(0), length = dg->length;
  if (i >= length)
    return;
  const short amp[3] = {NRH_NB == 2   ? NRG_QAM16_N1
                        : NRH_NB == 3 ? NRG_QAM64_N1
                                      : NRG_QAM256_N1,
                        NRH_NB == 3 ? NRG_QAM64_N2 : NRG_QAM256_N2,
                        NRG_QAM256_N3};
  const short2 cpe = dg->cpe;
  const int rot = !(cpe.x == 32767 && cpe.y == 0);
  const uint j = dg->map == 0xffffffffu ? i : map[dg->map + i], ri = dg->direct ? j : i;
  short2 comp = (short2)(0, 0);
  short mag[3] = {0, 0, 0};
  for (int a = 0; a < nb_rx; a++) {
    const short2 y = rx[dg->rx[a] + ri];
    const short2 c = ch[dg->ch + (size_t)a * dg->ch_stride + j];
    const short2 cc = rot ? cmul(c, cpe, 15) : (short2)(cpe_id(c.x), cpe_id(c.y));
    const short2 t = cmul_conj(cc, y, sh);
    comp = a ? (short2)(wrap16(comp.x + t.x), wrap16(comp.y + t.y)) : t;
    if (NRH_NB > 1) {
      const short m = sat16(madd(c.x, c.x, c.y, c.y) >> sh);
#pragma unroll
      for (int k = 0; k < NRH_NB - 1; k++)
        mag[k] = a ? wrap16(mag[k] + mulhrs(m, amp[k])) : mulhrs(m, amp[k]);
    }
  }
  short llr[2 * NRH_NB];
  if (NRH_NB == 1) {
    llr[0] = comp.x >> 4;
    llr[1] = comp.y >> 4;
  } else {
    const int tail = i >= (length & ~3u);
    llr[0] = comp.x;
    llr[1] = comp.y;
#pragma unroll
    for (int k = 1; k < NRH_NB; k++) {
      llr[2 * k] = pam_level(mag[k - 1], llr[2 * k - 2], tail);
      llr[2 * k + 1] = pam_level(mag[k - 1], llr[2 * k - 1], tail);
    }
  }
  const uint o = i * 2 * NRH_NB;
#pragma unroll
  for (int p = 0; p < 2 * NRH_NB; p++) {
    short v = llr[p];
    if (dg->scr != 0xffffffffu) {
      const uint n = dg->scr + o + p;
      if ((scr[n >> 5] >> (n & 31)) & 1)
        v = neg16(v);
    }
    out[dg->out + o + p] = v;
  }
}

#if NRH_NB <= 2
// 2-layer exact max-log ML for QPSK, target layer (nr_qpsk_llr_2layer, lane by lane): z0 target / z1 other MF output,
// rho = h0^H h1; llr = {I bit, Q bit}
static inline void qpsk_2layer_re(short2 z0, short2 z1, short2 rho, short llr[2])
{
  const int K = 23170; // round(2^16 / (2 sqrt(2)))
  const short y0r_o2 = slli16(mulhi16(z0.x, K), 1), y0i_o2 = slli16(mulhi16(z0.y, K), 1);
  const short y1r_o2 = z1.x >> 1, y1i_o2 = z1.y >> 1;
  const short rho_p = mulhi16(adds16(rho.x, rho.y), K), rho_m = mulhi16(subs16(rho.x, rho.y), K);
  const short psi_rpm = abs16(subs16(rho_p, y1r_o2)), psi_imm = abs16(subs16(rho_m, y1i_o2));
  const short psi_rmm = abs16(subs16(rho_m, y1r_o2)), psi_ipm = abs16(subs16(rho_p, y1i_o2));
  const short psi_rpp = abs16(adds16(rho_p, y1r_o2)), psi_imp = abs16(adds16(rho_m, y1i_o2));
  const short psi_rmp = abs16(adds16(rho_m, y1r_o2)), psi_ipp = abs16(adds16(rho_p, y1i_o2));
  const short num_re_p = adds16(adds16(adds16(psi_rpm, psi_imm), y0r_o2), y0i_o2);
  const short num_re_m = subs16(adds16(adds16(psi_rmm, psi_ipp), y0r_o2), y0i_o2);
  const short den_re_p = adds16(subs16(adds16(psi_rmp, psi_ipm), y0r_o2), y0i_o2);
  const short den_re_m = subs16(subs16(adds16(psi_rpp, psi_imp), y0r_o2), y0i_o2);
  llr[0] = subs16(max(num_re_p, num_re_m), max(den_re_p, den_re_m));
  // Q bit: num_im = {num_re_p, den_re_p}, den_im = {num_re_m, den_re_m}
  llr[1] = subs16(max(num_re_p, den_re_p), max(num_re_m, den_re_m));
}

static inline short max8(short a, short b, short c, short d, short e, short f, short g, short h)
{
  return max(max(max(a, b), max(c, d)), max(max(e, f), max(g, h)));
}

// 2-layer exact max-log ML for 16QAM, target layer (nr_qam16_llr_2layer, lane by lane): mag0 / mag1 the target / other
// layer magnitudes; llr = {I MSB, I LSB, Q MSB, Q LSB}
static inline void qam16_2layer_re(short2 z0, short2 z1, short mag0, short mag1, short2 rho, short llr[4])
{
  const int ONE_OVER_SQRT_10 = 20724, ONE_OVER_SQRT_10_Q15 = 10362, THREE_OVER_SQRT_10 = 31086;
  const int SQRT_10_OVER_FOUR = 25905, ONE_OVER_TWO_SQRT_10 = 10362, NINE_OVER_TWO_SQRT_10 = 23315;
  const short rho_rpi = adds16(rho.x, rho.y), rho_rmi = subs16(rho.x, rho.y);
  short rho_rs[8];
  rho_rs[0] = mulhi16(rho_rpi, ONE_OVER_SQRT_10);
  rho_rs[4] = mulhi16(rho_rmi, ONE_OVER_SQRT_10);
  rho_rs[3] = slli16(mulhi16(rho_rpi, THREE_OVER_SQRT_10), 1);
  rho_rs[7] = slli16(mulhi16(rho_rmi, THREE_OVER_SQRT_10), 1);
  const short x4 = mulhi16(rho.x, ONE_OVER_SQRT_10), x5 = slli16(mulhi16(rho.y, THREE_OVER_SQRT_10), 1);
  rho_rs[1] = adds16(x4, x5);
  rho_rs[5] = subs16(x4, x5);
  const short x6 = slli16(mulhi16(rho.x, THREE_OVER_SQRT_10), 1), x7 = mulhi16(rho.y, ONE_OVER_SQRT_10);
  rho_rs[2] = adds16(x6, x7);
  rho_rs[6] = subs16(x6, x7);
  const short y1r = z1.x, y1i = z1.y;
  short psi_r[16], psi_i[16];
#pragma unroll
  for (int j = 0; j < 8; j++)
    psi_r[j] = abs16(subs16(rho_rs[j], y1r));
#pragma unroll
  for (int j = 8; j < 16; j++)
    psi_r[j] = abs16(adds16(rho_rs[(j - 4) & 7], y1r));
  const uchar idx[16] = {4, 6, 5, 7, 0, 2, 1, 3, 0, 2, 1, 3, 4, 6, 5, 7};
#pragma unroll
  for (int k = 0; k < 16; k += 8)
#pragma unroll
    for (int j = k; j < k + 4; j++) {
      psi_i[j] = abs16(subs16(rho_rs[idx[j]], y1i));
      psi_i[j + 4] = abs16(adds16(rho_rs[idx[j + 4]], y1i));
    }
  const short y0r = z0.x, y0i = z0.y;
  const short y0r_1 = mulhi16(y0r, ONE_OVER_SQRT_10), y0i_1 = mulhi16(y0i, ONE_OVER_SQRT_10);
  const short y0r_3 = slli16(mulhi16(y0r, THREE_OVER_SQRT_10), 1), y0i_3 = slli16(mulhi16(y0i, THREE_OVER_SQRT_10), 1);
  const short y0_s[8] = {adds16(y0r_1, y0i_1),
                         adds16(y0r_1, y0i_3),
                         adds16(y0r_3, y0i_1),
                         adds16(y0r_3, y0i_3),
                         subs16(y0r_1, y0i_1),
                         subs16(y0r_1, y0i_3),
                         subs16(y0r_3, y0i_1),
                         subs16(y0r_3, y0i_3)};
  short met[16];
  const short m10 = mulhi16(mag0, ONE_OVER_TWO_SQRT_10);
  const short m2 = slli16(mulhi16(mag0, SQRT_10_OVER_FOUR), 1);
  const short m910 = slli16(mulhi16(mag0, NINE_OVER_TWO_SQRT_10), 2);
  const short chm[4] = {m10, m2, m2, m910};
#pragma unroll
  for (int j = 0; j < 16; j++) {
    // optimal interfering symbol magnitude (interference_abs_epi16), psi . a (prodsum_psi_a_epi16), |a|^2 power
    // (square_a_epi16)
    const int ar = psi_r[j] < mag1 ? ONE_OVER_SQRT_10_Q15 : THREE_OVER_SQRT_10;
    const int ai = psi_i[j] < mag1 ? ONE_OVER_SQRT_10_Q15 : THREE_OVER_SQRT_10;
    const short psi_a = adds16(slli16(mulhi16(psi_r[j], ar), 1), slli16(mulhi16(psi_i[j], ai), 1));
    const short sqr = slli16(mulhi16(slli16(mulhi16(slli16(mulhi16(ar, ar), 1), SQRT_10_OVER_FOUR), 1), mag1), 1);
    const short sqi = slli16(mulhi16(slli16(mulhi16(slli16(mulhi16(ai, ai), 1), SQRT_10_OVER_FOUR), 1), mag1), 1);
    const short a_sq = adds16(sqr, sqi);
    const short y0 = y0_s[j < 8 ? j : (j - 4) & 7];
    met[j] = j < 8 ? subs16(adds16(subs16(psi_a, a_sq), y0), chm[j & 3]) : subs16(subs16(subs16(psi_a, a_sq), y0), chm[j & 3]);
  }
  llr[0] = subs16(max8(met[0], met[1], met[2], met[3], met[4], met[5], met[6], met[7]),
                  max8(met[8], met[9], met[10], met[11], met[12], met[13], met[14], met[15]));
  llr[1] = subs16(max8(met[0], met[1], met[3], met[2], met[8], met[9], met[10], met[11]),
                  max8(met[4], met[5], met[6], met[7], met[12], met[13], met[14], met[15]));
  llr[2] = subs16(max8(met[0], met[1], met[4], met[5], met[8], met[9], met[12], met[13]),
                  max8(met[2], met[3], met[6], met[7], met[10], met[11], met[14], met[15]));
  llr[3] = subs16(max8(met[0], met[2], met[4], met[6], met[8], met[10], met[12], met[14]),
                  max8(met[1], met[3], met[5], met[7], met[9], met[11], met[13], met[15]));
}

// 2 layers, QPSK / 16QAM: one work-item per RE and symbol. The compensation of nr_channel_compensation (matched filters
// with the CPE-rotated channel and the 16QAM magnitudes: wrapping sums; rho01 = h0^H h1 and rho10 from the raw channel:
// saturating sums), the exact ML of both layers (the same for L-best and ML, as nr_compute_ML_llr), the output shift
// of nr_compute_ML_llr (QPSK >> 4), layer demap (per RE layer 0 then layer 1) and descrambling
__kernel void nr_inner_rx_2layer_int(__global const nrh_sym1_t *sym,
                                     __global const short2 *rx,
                                     __global const short2 *ch,
                                     __global const ushort *map,
                                     __global const uint *scr,
                                     __global short *out,
                                     int nb_rx,
                                     int sh)
{
  __global const nrh_sym1_t *dg = &sym[get_global_id(1)];
  const uint i = get_global_id(0);
  if (i >= dg->length)
    return;
  const short2 cpe = dg->cpe;
  const int rot = !(cpe.x == 32767 && cpe.y == 0);
  const uint j = dg->map == 0xffffffffu ? i : map[dg->map + i], ri = dg->direct ? j : i;
  short2 comp0 = (short2)(0, 0), comp1 = comp0, rho01 = comp0, rho10 = comp0;
  short mag0 = 0, mag1 = 0;
  for (int a = 0; a < nb_rx; a++) {
    const short2 y = rx[dg->rx[a] + ri];
    const short2 c0 = ch[dg->ch + (size_t)a * dg->ch_stride + j], c1 = ch[dg->ch + (size_t)(nb_rx + a) * dg->ch_stride + j];
    const short2 cc0 = rot ? cmul(c0, cpe, 15) : (short2)(cpe_id(c0.x), cpe_id(c0.y));
    const short2 cc1 = rot ? cmul(c1, cpe, 15) : (short2)(cpe_id(c1.x), cpe_id(c1.y));
    const short2 t0 = cmul_conj(cc0, y, sh), t1 = cmul_conj(cc1, y, sh);
    const short2 r01 = cmul_conj(c0, c1, sh), r10 = cmul_conj(c1, c0, sh);
    if (a == 0) {
      comp0 = t0;
      comp1 = t1;
      rho01 = r01;
      rho10 = r10;
    } else {
      comp0 = (short2)(wrap16(comp0.x + t0.x), wrap16(comp0.y + t0.y));
      comp1 = (short2)(wrap16(comp1.x + t1.x), wrap16(comp1.y + t1.y));
      rho01 = (short2)(adds16(rho01.x, r01.x), adds16(rho01.y, r01.y));
      rho10 = (short2)(adds16(rho10.x, r10.x), adds16(rho10.y, r10.y));
    }
    if (NRH_NB == 2) {
      const short m0 = mulhrs(sat16(madd(c0.x, c0.x, c0.y, c0.y) >> sh), NRG_QAM16_N1);
      const short m1 = mulhrs(sat16(madd(c1.x, c1.x, c1.y, c1.y) >> sh), NRG_QAM16_N1);
      mag0 = a ? wrap16(mag0 + m0) : m0;
      mag1 = a ? wrap16(mag1 + m1) : m1;
    }
  }
  const int Qm = 2 * NRH_NB;
  for (int t = 0; t < 2; t++) {
    short llr[4];
    if (NRH_NB == 1)
      qpsk_2layer_re(t ? comp1 : comp0, t ? comp0 : comp1, t ? rho10 : rho01, llr);
    else
      qam16_2layer_re(t ? comp1 : comp0, t ? comp0 : comp1, t ? mag1 : mag0, t ? mag0 : mag1, t ? rho10 : rho01, llr);
    const uint o = (i * 2 + t) * Qm;
    for (int p = 0; p < Qm; p++) {
      short v = NRH_NB == 1 ? llr[p] >> 4 : llr[p];
      if (dg->scr != 0xffffffffu) {
        const uint n = dg->scr + o + p;
        if ((scr[n >> 5] >> (n & 31)) & 1)
          v = neg16(v);
      }
      out[dg->out + o + p] = v;
    }
  }
}
#endif
