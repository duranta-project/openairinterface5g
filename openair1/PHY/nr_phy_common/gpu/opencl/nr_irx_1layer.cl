/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * Single-layer PUSCH inner RX in OpenCL for the inner-RX module nr_irx_cl.c, built with -DNRH_NB=1..4 (QPSK to
 * 256QAM): the integer MRC of nr_channel_compensation (nb_layers 1) and the LLRs of nr_compute_llr, bit-exact with the
 * CPU (the port of k_inner_rx_1layer of nr_inner_rx_gpu.cu). The int16 helpers follow the SSE helpers of sse_intrin.h
 * lane by lane (wrapping madd, arithmetic shift, saturating pack); every int16 wrap-around is explicit.
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
