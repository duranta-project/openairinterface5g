/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_channel_compensation.h"
#include "bits.h"
#include <complex.h>
#include "PHY/sse_intrin.h"
#include "PHY/impl_defs_top.h"
#ifdef __aarch64__
#define USE_128BIT
#endif

#if defined(__riscv) && defined(__riscv_vector)
#include <riscv_vector.h>

/* RVV path. The generic (x86/ARM) build below goes through SIMDe; on RISC-V its
 * emulation of the 256-bit shuffle/sign/mulhrs sequence is ~20-40x slower than
 * plain scalar. This hand-written RVV version is vector-length-agnostic (one
 * kernel for any VLEN) and bit-exact with the SIMDe output -- including the
 * wrapping int16 negation in the conjugate (sign_epi16: -(-32768) = -32768).
 * Validated byte-for-byte in openair1/PHY/rvv_harness/rvv_chcomp_test.c. */

#define RVV_LD2(ptr, vl) __riscv_vlseg2e16_v_i16m1x2((const int16_t *)(ptr), (vl))
#define RVV_RE(t) __riscv_vget_v_i16m1x2_i16m1((t), 0)
#define RVV_IM(t) __riscv_vget_v_i16m1x2_i16m1((t), 1)

static inline void rvv_store_c16(c16_t *p, vint16m1_t re, vint16m1_t im, size_t vl)
{
  __riscv_vsseg2e16_v_i16m1x2((int16_t *)p, __riscv_vcreate_v_i16m1x2(re, im), vl);
}

/* (re,im) = conj(a) * b >> shift, saturated to int16 */
static inline void rvv_conj_mult(vint16m1_t ar,
                                 vint16m1_t ai,
                                 vint16m1_t br,
                                 vint16m1_t bi,
                                 int shift,
                                 size_t vl,
                                 vint16m1_t *re16,
                                 vint16m1_t *im16)
{
  vint32m2_t re = __riscv_vwmacc_vv_i32m2(__riscv_vwmul_vv_i32m2(ar, br, vl), ai, bi, vl);
  vint16m1_t nai = __riscv_vneg_v_i16m1(ai, vl); /* wrapping: -(-32768) = -32768 */
  vint32m2_t im = __riscv_vwmacc_vv_i32m2(__riscv_vwmul_vv_i32m2(nai, br, vl), ar, bi, vl);
  *re16 = __riscv_vnclip_wx_i16m1(__riscv_vsra_vx_i32m2(re, (size_t)shift, vl), 0, __RISCV_VXRM_RDN, vl);
  *im16 = __riscv_vnclip_wx_i16m1(__riscv_vsra_vx_i32m2(im, (size_t)shift, vl), 0, __RISCV_VXRM_RDN, vl);
}

/* mulhrs(sat16(|a|^2 >> shift), amp) -- both output lanes get this value */
static inline vint16m1_t rvv_chmag(vint16m1_t ar, vint16m1_t ai, int16_t amp, int shift, size_t vl)
{
  vint32m2_t mg = __riscv_vwmacc_vv_i32m2(__riscv_vwmul_vv_i32m2(ar, ar, vl), ai, ai, vl);
  vint16m1_t mag16 = __riscv_vnclip_wx_i16m1(__riscv_vsra_vx_i32m2(mg, (size_t)shift, vl), 0, __RISCV_VXRM_RDN, vl);
  vint32m2_t p = __riscv_vwmul_vx_i32m2(mag16, amp, vl);
  /* mulhrs = ((x*amp >> 14) + 1) >> 1, truncated to 16 bits (no saturation) */
  p = __riscv_vsra_vx_i32m2(__riscv_vadd_vx_i32m2(__riscv_vsra_vx_i32m2(p, 14, vl), 1, vl), 1, vl);
  return __riscv_vnsra_wx_i16m1(p, 0, vl);
}

void nr_channel_compensation(uint32_t buffer_length,
                             uint32_t pdsch_buf_size_max,
                             int nb_rx_ant,
                             int nb_layers,
                             c16_t rxFext[nb_rx_ant][buffer_length],
                             c16_t chFext[nb_layers][nb_rx_ant][buffer_length],
                             c16_t ch_maga[nb_layers][pdsch_buf_size_max],
                             c16_t ch_magb[nb_layers][pdsch_buf_size_max],
                             c16_t ch_magc[nb_layers][pdsch_buf_size_max],
                             c16_t **rxComp,
                             c16_t (*rho)[nb_layers][pdsch_buf_size_max],
                             int mod_order,
                             uint32_t symbol,
                             uint32_t output_shift)
{
  int16_t ampa = 0, ampb = 0, ampc = 0;
  if (mod_order == 4) {
    ampa = QAM16_n1;
  } else if (mod_order == 6) {
    ampa = QAM64_n1;
    ampb = QAM64_n2;
  } else if (mod_order == 8) {
    ampa = QAM256_n1;
    ampb = QAM256_n2;
    ampc = QAM256_n3;
  }
  const int shift = (int)output_shift;

  for (int aatx = 0; aatx < nb_layers; aatx++) {
    c16_t *rxComp_l = &rxComp[aatx][symbol * buffer_length];
    c16_t *maga = ch_maga[aatx];
    c16_t *magb = ch_magb[aatx];
    c16_t *magc = ch_magc[aatx];

    // First Rx antenna: direct store (avoids pre-memset of the output buffers)
    {
      const c16_t *rxF = rxFext[0];
      const c16_t *chF = chFext[aatx][0];
      for (uint32_t k = 0; k < buffer_length;) {
        size_t vl = __riscv_vsetvl_e16m1(buffer_length - k);
        vint16m1x2_t sc = RVV_LD2(&chF[k], vl), sx = RVV_LD2(&rxF[k], vl);
        vint16m1_t cr = RVV_RE(sc), ci = RVV_IM(sc);
        vint16m1_t re, im;
        rvv_conj_mult(cr, ci, RVV_RE(sx), RVV_IM(sx), shift, vl, &re, &im);
        rvv_store_c16(&rxComp_l[k], re, im, vl);
        if (mod_order > 2) {
          vint16m1_t m = rvv_chmag(cr, ci, ampa, shift, vl);
          rvv_store_c16(&maga[k], m, m, vl);
          if (mod_order > 4) {
            vint16m1_t mb = rvv_chmag(cr, ci, ampb, shift, vl);
            rvv_store_c16(&magb[k], mb, mb, vl);
          }
          if (mod_order > 6) {
            vint16m1_t mc = rvv_chmag(cr, ci, ampc, shift, vl);
            rvv_store_c16(&magc[k], mc, mc, vl);
          }
        }
        k += vl;
      }
      if (rho) {
        for (int atx = 0; atx < nb_layers; atx++) {
          c16_t *rho_l = rho[aatx][atx];
          const c16_t *chF2 = chFext[atx][0];
          for (uint32_t k = 0; k < buffer_length;) {
            size_t vl = __riscv_vsetvl_e16m1(buffer_length - k);
            vint16m1x2_t sc = RVV_LD2(&chF[k], vl), s2 = RVV_LD2(&chF2[k], vl);
            vint16m1_t re, im;
            rvv_conj_mult(RVV_RE(sc), RVV_IM(sc), RVV_RE(s2), RVV_IM(s2), shift, vl, &re, &im);
            rvv_store_c16(&rho_l[k], re, im, vl);
            k += vl;
          }
        }
      }
    }

    // Remaining Rx antennas: MRC accumulate
    for (int aarx = 1; aarx < nb_rx_ant; aarx++) {
      const c16_t *rxF = rxFext[aarx];
      const c16_t *chF = chFext[aatx][aarx];
      for (uint32_t k = 0; k < buffer_length;) {
        size_t vl = __riscv_vsetvl_e16m1(buffer_length - k);
        vint16m1x2_t sc = RVV_LD2(&chF[k], vl), sx = RVV_LD2(&rxF[k], vl);
        vint16m1_t cr = RVV_RE(sc), ci = RVV_IM(sc);
        vint16m1_t re, im;
        rvv_conj_mult(cr, ci, RVV_RE(sx), RVV_IM(sx), shift, vl, &re, &im);
        vint16m1x2_t oc = RVV_LD2(&rxComp_l[k], vl); // add_epi16 (wrapping) accumulate
        re = __riscv_vadd_vv_i16m1(RVV_RE(oc), re, vl);
        im = __riscv_vadd_vv_i16m1(RVV_IM(oc), im, vl);
        rvv_store_c16(&rxComp_l[k], re, im, vl);
        if (mod_order > 2) {
          vint16m1_t m = __riscv_vadd_vv_i16m1(RVV_RE(RVV_LD2(&maga[k], vl)), rvv_chmag(cr, ci, ampa, shift, vl), vl);
          rvv_store_c16(&maga[k], m, m, vl);
          if (mod_order > 4) {
            vint16m1_t mb = __riscv_vadd_vv_i16m1(RVV_RE(RVV_LD2(&magb[k], vl)), rvv_chmag(cr, ci, ampb, shift, vl), vl);
            rvv_store_c16(&magb[k], mb, mb, vl);
          }
          if (mod_order > 6) {
            vint16m1_t mc = __riscv_vadd_vv_i16m1(RVV_RE(RVV_LD2(&magc[k], vl)), rvv_chmag(cr, ci, ampc, shift, vl), vl);
            rvv_store_c16(&magc[k], mc, mc, vl);
          }
        }
        k += vl;
      }
      if (rho) {
        for (int atx = 0; atx < nb_layers; atx++) {
          c16_t *rho_l = rho[aatx][atx];
          const c16_t *chF2 = chFext[atx][aarx];
          for (uint32_t k = 0; k < buffer_length;) {
            size_t vl = __riscv_vsetvl_e16m1(buffer_length - k);
            vint16m1x2_t sc = RVV_LD2(&chF[k], vl), s2 = RVV_LD2(&chF2[k], vl);
            vint16m1_t re, im;
            rvv_conj_mult(RVV_RE(sc), RVV_IM(sc), RVV_RE(s2), RVV_IM(s2), shift, vl, &re, &im);
            vint16m1x2_t old = RVV_LD2(&rho_l[k], vl); // adds_epi16 (saturating) accumulate
            re = __riscv_vsadd_vv_i16m1(RVV_RE(old), re, vl);
            im = __riscv_vsadd_vv_i16m1(RVV_IM(old), im, vl);
            rvv_store_c16(&rho_l[k], re, im, vl);
            k += vl;
          }
        }
      }
    }
  }
}

#else

void nr_channel_compensation(uint32_t buffer_length,
                             uint32_t pdsch_buf_size_max,
                             int nb_rx_ant,
                             int nb_layers,
                             c16_t rxFext[nb_rx_ant][buffer_length],
                             c16_t chFext[nb_layers][nb_rx_ant][buffer_length],
                             c16_t ch_maga[nb_layers][pdsch_buf_size_max],
                             c16_t ch_magb[nb_layers][pdsch_buf_size_max],
                             c16_t ch_magc[nb_layers][pdsch_buf_size_max],
                             c16_t **rxComp,
                             c16_t (*rho)[nb_layers][pdsch_buf_size_max],
                             int mod_order,
                             uint32_t symbol,
                             uint32_t output_shift)
{
  simde__m256i QAM_ampa_256 = simde_mm256_setzero_si256();
  simde__m256i QAM_ampb_256 = simde_mm256_setzero_si256();
  simde__m256i QAM_ampc_256 = simde_mm256_setzero_si256();

  if (mod_order == 4) {
    QAM_ampa_256 = simde_mm256_set1_epi16(QAM16_n1);
  } else if (mod_order == 6) {
    QAM_ampa_256 = simde_mm256_set1_epi16(QAM64_n1);
    QAM_ampb_256 = simde_mm256_set1_epi16(QAM64_n2);
  } else if (mod_order == 8) {
    QAM_ampa_256 = simde_mm256_set1_epi16(QAM256_n1);
    QAM_ampb_256 = simde_mm256_set1_epi16(QAM256_n2);
    QAM_ampc_256 = simde_mm256_set1_epi16(QAM256_n3);
  }

  for (int aatx = 0; aatx < nb_layers; aatx++) {
    simde__m256i *rxComp_256 = (simde__m256i *)&rxComp[aatx][symbol * buffer_length];
    simde__m256i *ch_maga_256 = (simde__m256i *)ch_maga[aatx];
    simde__m256i *ch_magb_256 = (simde__m256i *)ch_magb[aatx];
    simde__m256i *ch_magc_256 = (simde__m256i *)ch_magc[aatx];

    // First Rx antenna: direct store — eliminates need to pre memset the output buffers
    {
      simde__m256i *rxF_256 = (simde__m256i *)rxFext[0];
      simde__m256i *chF_256 = (simde__m256i *)chFext[aatx][0];

      for (uint32_t i = 0; i < buffer_length >> 3; i++) {
        rxComp_256[i] = oai_mm256_cpx_mult_conj(chF_256[i], rxF_256[i], output_shift);

        if (mod_order > 2) {
          simde__m256i mag = oai_mm256_smadd(chF_256[i], chF_256[i], output_shift);
          mag = simde_mm256_packs_epi32(mag, mag);
          mag = simde_mm256_unpacklo_epi16(mag, mag);
          ch_maga_256[i] = simde_mm256_mulhrs_epi16(mag, QAM_ampa_256);

          if (mod_order > 4)
            ch_magb_256[i] = simde_mm256_mulhrs_epi16(mag, QAM_ampb_256);

          if (mod_order > 6)
            ch_magc_256[i] = simde_mm256_mulhrs_epi16(mag, QAM_ampc_256);
        }
      }

      if (rho) {
        for (int atx = 0; atx < nb_layers; atx++) {
          simde__m256i *rho_256 = (simde__m256i *)rho[aatx][atx];
          simde__m256i *chF2_256 = (simde__m256i *)chFext[atx][0];
          for (uint32_t i = 0; i < buffer_length >> 3; i++)
            rho_256[i] = oai_mm256_cpx_mult_conj(chF_256[i], chF2_256[i], output_shift);
        }
      }
    }

    // Remaining Rx antennas: accumulate (MRC)
    for (int aarx = 1; aarx < nb_rx_ant; aarx++) {
      simde__m256i *rxF_256 = (simde__m256i *)rxFext[aarx];
      simde__m256i *chF_256 = (simde__m256i *)chFext[aatx][aarx];

      for (uint32_t i = 0; i < buffer_length >> 3; i++) {
        simde__m256i comp = oai_mm256_cpx_mult_conj(chF_256[i], rxF_256[i], output_shift);
        rxComp_256[i] = simde_mm256_add_epi16(rxComp_256[i], comp);

        if (mod_order > 2) {
          simde__m256i mag = oai_mm256_smadd(chF_256[i], chF_256[i], output_shift);
          mag = simde_mm256_packs_epi32(mag, mag);
          mag = simde_mm256_unpacklo_epi16(mag, mag);
          ch_maga_256[i] = simde_mm256_add_epi16(ch_maga_256[i], simde_mm256_mulhrs_epi16(mag, QAM_ampa_256));

          if (mod_order > 4)
            ch_magb_256[i] = simde_mm256_add_epi16(ch_magb_256[i], simde_mm256_mulhrs_epi16(mag, QAM_ampb_256));

          if (mod_order > 6)
            ch_magc_256[i] = simde_mm256_add_epi16(ch_magc_256[i], simde_mm256_mulhrs_epi16(mag, QAM_ampc_256));
        }
      }

      if (rho) {
        for (int atx = 0; atx < nb_layers; atx++) {
          simde__m256i *rho_256 = (simde__m256i *)rho[aatx][atx];
          simde__m256i *chF2_256 = (simde__m256i *)chFext[atx][aarx];
          for (uint32_t i = 0; i < buffer_length >> 3; i++)
            rho_256[i] = simde_mm256_adds_epi16(rho_256[i], oai_mm256_cpx_mult_conj(chF_256[i], chF2_256[i], output_shift));
        }
      }
    }
  }
}

#endif /* __riscv && __riscv_vector */
