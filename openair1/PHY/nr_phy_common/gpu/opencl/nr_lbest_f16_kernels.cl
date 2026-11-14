/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * FP16 fused 2-layer inner RX (nr_inner_rx_2layer_f16) in OpenCL, for the inner-RX module nr_irx_cl.c: the source
 * built after nr_lbest_f16_layer.cl and nr_lbest_f16_core.h with -DNRH_NB=3|4 (64 / 256QAM) and -DNRH_FULL=0|1
 * (L-best / full ML), the constant loop bounds of the CPU specializations.
 */

// ---------------------------------------------------------------------------------------------------------------------
// fused 2-layer inner RX (nr_inner_rx_2layer_f16): MRC + Gram in FP16, the detector of both layers, the LLR store
// ---------------------------------------------------------------------------------------------------------------------
// vqabsq_s16: -32768 -> 32767
static inline hi hi_abs_sat(short a)
{
  return (short)min((int)abs(a), 32767);
}

#define NRH_MAX_RX 8 // NR_IRX_MAX_RX

// the ARMv8.2 front end of one RE: rx[rxo[a]] for antenna a (rxo in global memory: a run-time index into a private
// array, or a choice between buffers, stalled the Adreno compiler), ch [2][nb_rx][cbl] at the RE's estimate, scaled by
// 2^ls (ls = 1 - e, e the bit length of the largest RX or channel component), the PTRS phase error applied to the RX;
// the operations of nrh_front_end of nr_lbest_f16.c in their order
static inline hi nrh_front_end_cl(__global const short2 *rx,
                                  __global const uint *rxo,
                                  __global const short2 *ch,
                                  uint cbl,
                                  int nb_rx,
                                  short2 cpe,
                                  hv f[8])
{
  const int rot = !(cpe.x == 32767 && cpe.y == 0);
  const hv CR = hv_set1(cpe.x / 32768.0f), CI = hv_set1(cpe.y / 32768.0f);
  hi amax = 1;
  for (int a = 0; a < nb_rx; a++) {
    const short2 y = rx[rxo[a]], h0 = ch[(size_t)a * cbl], h1 = ch[(size_t)(nb_rx + a) * cbl];
    amax = hi_max(amax, hi_max(hi_abs_sat(y.x), hi_abs_sat(y.y)));
    amax = hi_max(amax, hi_max(hi_abs_sat(h0.x), hi_abs_sat(h0.y)));
    amax = hi_max(amax, hi_max(hi_abs_sat(h1.x), hi_abs_sat(h1.y)));
  }
  const hi ls = (hi)(clz(amax) - 15);
  const hv S = hv_pow2(ls);
  hv z0r = hv_set1(0.0f), z0i = z0r, z1r = z0r, z1i = z0r, P0 = z0r, P1 = z0r, rr = z0r, ri = z0r;
  for (int a = 0; a < nb_rx; a++) {
    const short2 y = rx[rxo[a]], h0 = ch[(size_t)a * cbl], h1 = ch[(size_t)(nb_rx + a) * cbl];
    hv yr = hv_mul(hv_cvt(y.x), S), yi = hv_mul(hv_cvt(y.y), S);
    if (rot) {
      const hv tr = hv_fma(hv_mul(CR, yr), CI, yi);
      yi = hv_fms(hv_mul(CR, yi), CI, yr);
      yr = tr;
    }
    const hv a0r = hv_mul(hv_cvt(h0.x), S), a0i = hv_mul(hv_cvt(h0.y), S);
    const hv a1r = hv_mul(hv_cvt(h1.x), S), a1i = hv_mul(hv_cvt(h1.y), S);
    z0r = hv_fma(hv_fma(z0r, a0r, yr), a0i, yi);
    z0i = hv_fms(hv_fma(z0i, a0r, yi), a0i, yr);
    z1r = hv_fma(hv_fma(z1r, a1r, yr), a1i, yi);
    z1i = hv_fms(hv_fma(z1i, a1r, yi), a1i, yr);
    P0 = hv_fma(hv_fma(P0, a0r, a0r), a0i, a0i);
    P1 = hv_fma(hv_fma(P1, a1r, a1r), a1i, a1i);
    rr = hv_fma(hv_fma(rr, a0r, a1r), a0i, a1i);
    ri = hv_fms(hv_fma(ri, a0r, a1i), a0i, a1r);
  }
  f[0] = z0r;
  f[1] = z0i;
  f[2] = z1r;
  f[3] = z1i;
  f[4] = P0;
  f[5] = P1;
  f[6] = rr;
  f[7] = ri;
  return ls;
}

// one OFDM symbol of a batch (nr_irx_cl_sym_t of nr_irx_cl.c): element offsets into the buffers
typedef struct {
  uint rx[NRH_MAX_RX]; // RX of antenna a: extracted (direct 0: RE i at rx[a] + i) or the symbol's first allocation RE
                       // in rxdataF (direct 1: RE i at rx[a] + map[i])
  uint ch; // channel estimates [2][nb_rx][ch_stride] (c16)
  uint map; // allocation RE of each RE (uint16), NRH_NO_MAP: the identity
  uint out; // layer-demapped LLRs [length][2][Qm] (int16)
  uint length;
  uint ch_stride;
  uint scr; // codeword bit of the first LLR, NRH_NO_SCR: no descrambling
  short2 cpe; // PTRS common phase error, (32767, 0) without PTRS
  uint direct;
} nrh_sym_t;
#define NRH_NO_MAP 0xffffffffu
#define NRH_NO_SCR 0xffffffffu

// front end of RE re of symbol d rescaled (max(P0, P1) in [8, 16)): f = z0r z0i z1r z1i P0 P1 rr ri; the LLR scale and
// lambda in the int16 compensation units 2^k, k = 2 ls + lt + output_shift
static inline void nrh_irx_prolog(__global const short2 *rx,
                                  __global const short2 *ch,
                                  __global const ushort *map,
                                  __global const nrh_sym_t *dg,
                                  const nrh_sym_t *d,
                                  int nb_rx,
                                  uint re,
                                  float lambda,
                                  int output_shift,
                                  hv f[8],
                                  hsc *inv,
                                  hv *lam)
{
  const uint cre = d->map == NRH_NO_MAP ? re : map[d->map + re], rre = d->direct ? cre : re;
  const hi ls = nrh_front_end_cl(rx + rre, dg->rx, ch + d->ch + cre, d->ch_stride, nb_rx, d->cpe, f);
  const hi lt = nrh_rescale_exp(f[4], f[5]);
  const hv T = hv_pow2(lt);
  const hi k = hi_add(hi_add(hi_shl1(ls), lt), hi_set1(output_shift));
  *inv = nrh_llr_scale(k);
  *lam = hv_lam(lambda, k);
  for (int i = 0; i < 8; i++)
    f[i] = hv_mul(f[i], T);
}

// seed of target layer t: (z_t, z_o, P_t, P_o, rho) with rho = h_t^H h_o (conjugated for layer 1); selects on constant
// indices (a run-time index into f would move it out of the registers)
static inline void nrh_irx_seed(const hv f[8], int t, hv lam, nrh_seed_t *s)
{
  nrh_seed(t ? f[2] : f[0],
           t ? f[3] : f[1],
           t ? f[0] : f[2],
           t ? f[1] : f[3],
           t ? f[5] : f[4],
           t ? f[4] : f[5],
           f[6],
           t ? hv_neg(f[7]) : f[7],
           lam,
           NRH_NB,
           s);
}

// LLRs of layer t of RE re, layer-demapped (per RE: layer 0 then layer 1) and descrambled with the packed codeword bits
// (bit n of the codeword: word n / 32, bit n % 32; 1 negates); 64QAM with the one-bit cooling of nr_compute_ML_llr
static inline void nrh_irx_store(hi res[8], const nrh_sym_t *d, uint re, int t, __global short *out, __global const uint *scr)
{
  const int Qm = 2 * NRH_NB;
  const uint o = re * 2 * Qm + t * Qm;
  for (int p = 0; p < Qm; p++) {
    short v = NRH_NB == 3 ? hi_shr1(res[p]) : res[p];
    if (d->scr != NRH_NO_SCR) {
      const uint b = d->scr + o + p;
      if ((scr[b >> 5] >> (b & 31)) & 1)
        v = (short)-v;
    }
    out[d->out + o + p] = v;
  }
}

// work-items: [RE, symbol] detecting both layers, or with -DNRH_CL_LAYER_SPLIT [RE, 2 symbol + layer] (the front end
// computed by both layers' work-items, one copy of the detector code); which is faster depends on the GPU (Mali: per
// RE, Adreno: split, where the two copies overflow the registers)

#ifdef NRH_CL_LAYER_SPLIT
#define NRH_SYM ((int)get_global_id(1) >> 1)
#define NRH_T0 ((int)get_global_id(1) & 1)
#define NRH_T1 (NRH_T0 + 1)
#else
#define NRH_SYM ((int)get_global_id(1))
#define NRH_T0 0
#define NRH_T1 2
#endif

// Single pass: the 3x3 window with the full grid on its wide REs (NRH_FULL 0) or the full grid (NRH_FULL 1)
__kernel void nr_inner_rx_2layer_f16(__global const nrh_sym_t *sym,
                                     __global const short2 *rx,
                                     __global const short2 *ch,
                                     __global const ushort *map,
                                     __global const uint *scr,
                                     __global short *out,
                                     int nb_rx,
                                     float lambda,
                                     int output_shift)
{
  __global const nrh_sym_t *dg = &sym[NRH_SYM];
  const nrh_sym_t d = *dg;
  const uint re = get_global_id(0);
  if (re >= d.length)
    return;
  hv f[8], lam;
  hsc inv;
  nrh_irx_prolog(rx, ch, map, dg, &d, nb_rx, re, lambda, output_shift, f, &inv, &lam);
#ifndef NRH_CL_LAYER_SPLIT
#pragma unroll
#endif
  for (int t = NRH_T0; t < NRH_T1; t++) {
    nrh_seed_t s;
    nrh_irx_seed(f, t, lam, &s);
    hi res[8];
    if (NRH_FULL)
      nrh_layer_full(&s, NRH_NB, &inv, res);
    else
      nrh_layer_window(&s, NRH_NB, &inv, 0, res);
    nrh_irx_store(res, &d, re, t, out, scr);
  }
}

// Two-pass L-best, pass 1: the 3x3 window of every RE and layer, without the full-grid code; the wide ones (nrh_wide)
// appended to wide[] as symbol << 17 | re << 1 | layer, their count in nwide[0] (zero before the launch)
__kernel void nr_inner_rx_2layer_f16_p1(__global const nrh_sym_t *sym,
                                        __global const short2 *rx,
                                        __global const short2 *ch,
                                        __global const ushort *map,
                                        __global const uint *scr,
                                        __global short *out,
                                        int nb_rx,
                                        float lambda,
                                        int output_shift,
                                        __global uint *wide,
                                        __global volatile uint *nwide)
{
  const int k = NRH_SYM;
  __global const nrh_sym_t *dg = &sym[k];
  const nrh_sym_t d = *dg;
  const uint re = get_global_id(0);
  if (re >= d.length)
    return;
  hv f[8], lam;
  hsc inv;
  nrh_irx_prolog(rx, ch, map, dg, &d, nb_rx, re, lambda, output_shift, f, &inv, &lam);
#ifndef NRH_CL_LAYER_SPLIT
#pragma unroll
#endif
  for (int t = NRH_T0; t < NRH_T1; t++) {
    nrh_seed_t s;
    nrh_irx_seed(f, t, lam, &s);
    hi res[8];
    if (nrh_layer_window(&s, NRH_NB, &inv, 1, res))
      wide[atomic_inc(nwide)] = (uint)k << 17 | re << 1 | t;
    nrh_irx_store(res, &d, re, t, out, scr);
  }
}

// pass 2: the full grid of the nwide[0] (RE, layer) of pass 1, one work-item each, replacing their LLRs; the count read
// on the device, so the host enqueues both passes at once (global size: the maximum count)
__kernel void nr_inner_rx_2layer_f16_p2(__global const nrh_sym_t *sym,
                                        __global const short2 *rx,
                                        __global const short2 *ch,
                                        __global const ushort *map,
                                        __global const uint *scr,
                                        __global short *out,
                                        int nb_rx,
                                        float lambda,
                                        int output_shift,
                                        __global const uint *wide,
                                        __global const uint *nwide)
{
  const uint i = get_global_id(0);
  if (i >= nwide[0])
    return;
  const uint w = wide[i];
  __global const nrh_sym_t *dg = &sym[w >> 17];
  const nrh_sym_t d = *dg;
  const uint re = (w >> 1) & 0xffff;
  const int t = w & 1;
  hv f[8], lam;
  hsc inv;
  nrh_irx_prolog(rx, ch, map, dg, &d, nb_rx, re, lambda, output_shift, f, &inv, &lam);
  nrh_seed_t s;
  nrh_irx_seed(f, t, lam, &s);
  hi res[8];
  nrh_layer_full(&s, NRH_NB, &inv, res);
  nrh_irx_store(res, &d, re, t, out, scr);
}
