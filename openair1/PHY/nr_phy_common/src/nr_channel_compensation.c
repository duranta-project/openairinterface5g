/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_channel_compensation.h"
#include "nr_phy_common.h"
#include "bits.h"
#include <complex.h>
#include "PHY/sse_intrin.h"
#include "PHY/impl_defs_top.h"
#ifdef __aarch64__
#define USE_128BIT
#endif

// ---- width-parameterized MRC compensation core (AVX2 / SSE->NEON; +AVX-512 later) ----
// Same source instantiated per width from nr_channel_comp_simd.c.inc; the public entry dispatches.
#define NRLB_W 256
#include "nr_lbest_simd_width.h"
#include "nr_channel_comp_simd.c.inc"
#undef NRLB_W
#define NRLB_W 128
#include "nr_lbest_simd_width.h"
#include "nr_channel_comp_simd.c.inc"
#undef NRLB_W

// x86 width selection (cached): 1 = w128 (SSE->NEON regression path, OAI_COMP_W128), 0 = w256 (default).
// aarch64 always runs w128 (SIMDe maps 256-bit to 2x128 NEON, slower than native 128).
static int nr_comp_simd_width_mode(void)
{
  static int m = -1;
  if (m < 0)
    m = getenv("OAI_COMP_W128") ? 1 : 0;
  return m;
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
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE) || defined(__aarch64__)
  nr_channel_compensation_w128(buffer_length, pdsch_buf_size_max, nb_rx_ant, nb_layers, rxFext, chFext,
                               ch_maga, ch_magb, ch_magc, rxComp, rho, mod_order, symbol, output_shift);
#else
  if (nr_comp_simd_width_mode() == 1)
    nr_channel_compensation_w128(buffer_length, pdsch_buf_size_max, nb_rx_ant, nb_layers, rxFext, chFext,
                                 ch_maga, ch_magb, ch_magc, rxComp, rho, mod_order, symbol, output_shift);
  else
    nr_channel_compensation_w256(buffer_length, pdsch_buf_size_max, nb_rx_ant, nb_layers, rxFext, chFext,
                                 ch_maga, ch_magb, ch_magc, rxComp, rho, mod_order, symbol, output_shift);
#endif
}

/* Fused single-layer inner RX: MRC channel compensation + per-RE LLR, TILED so the compensated
 * symbols and channel magnitudes live only in L1 scratch and are never materialized as
 * full-symbol arrays (no rxComp/mag DRAM round-trip). Bit-exact with the unfused
 * {nr_channel_compensation(nb_layers==1) + nr_dlsch_llr} path: identical per-RE math, identical
 * Rx-antenna accumulation order. Single-layer only (no rho, no MMSE inversion); the caller must
 * gate out the PTRS case, which sits between compensation and LLR. */
void nr_inner_rx_1layer(uint32_t length,
                        uint32_t buffer_length,
                        int nb_rx_ant,
                        c16_t rxFext[nb_rx_ant][buffer_length],
                        c16_t chFext[nb_rx_ant][buffer_length],
                        int mod_order,
                        int output_shift,
                        int16_t *llr)
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

  // L1-resident per-tile scratch. TILE is a multiple of 8 (SIMD block) and of 16 (RE alloc
  // granularity), so with buffer_length a multiple of 16 every tile spans whole 8-RE blocks.
  enum { TILE = 96 };
  __attribute__((aligned(32))) c16_t rxComp_t[TILE];
  __attribute__((aligned(32))) c16_t maga_t[TILE];
  __attribute__((aligned(32))) c16_t magb_t[TILE];
  __attribute__((aligned(32))) c16_t magc_t[TILE];

  simde__m256i *rxComp_256 = (simde__m256i *)rxComp_t;
  simde__m256i *maga_256 = (simde__m256i *)maga_t;
  simde__m256i *magb_256 = (simde__m256i *)magb_t;
  simde__m256i *magc_256 = (simde__m256i *)magc_t;

  for (uint32_t base = 0; base < length; base += TILE) {
    const uint32_t tre = (length - base < TILE) ? (length - base) : TILE; // valid REs (LLR)
    uint32_t hi = base + TILE;
    if (hi > buffer_length)
      hi = buffer_length;
    const uint32_t nblk = (hi - base) >> 3; // 8-RE SIMD blocks, never reading past buffer_length

    // First Rx antenna: direct store (no pre-memset of the tile scratch needed)
    {
      simde__m256i *rxF_256 = (simde__m256i *)&rxFext[0][base];
      simde__m256i *chF_256 = (simde__m256i *)&chFext[0][base];
      for (uint32_t i = 0; i < nblk; i++) {
        rxComp_256[i] = oai_mm256_cpx_mult_conj(chF_256[i], rxF_256[i], output_shift);
        if (mod_order > 2) {
          simde__m256i mag = oai_mm256_smadd(chF_256[i], chF_256[i], output_shift);
          mag = simde_mm256_packs_epi32(mag, mag);
          mag = simde_mm256_unpacklo_epi16(mag, mag);
          maga_256[i] = simde_mm256_mulhrs_epi16(mag, QAM_ampa_256);
          if (mod_order > 4)
            magb_256[i] = simde_mm256_mulhrs_epi16(mag, QAM_ampb_256);
          if (mod_order > 6)
            magc_256[i] = simde_mm256_mulhrs_epi16(mag, QAM_ampc_256);
        }
      }
    }

    // Remaining Rx antennas: accumulate (MRC)
    for (int aarx = 1; aarx < nb_rx_ant; aarx++) {
      simde__m256i *rxF_256 = (simde__m256i *)&rxFext[aarx][base];
      simde__m256i *chF_256 = (simde__m256i *)&chFext[aarx][base];
      for (uint32_t i = 0; i < nblk; i++) {
        simde__m256i comp = oai_mm256_cpx_mult_conj(chF_256[i], rxF_256[i], output_shift);
        rxComp_256[i] = simde_mm256_add_epi16(rxComp_256[i], comp);
        if (mod_order > 2) {
          simde__m256i mag = oai_mm256_smadd(chF_256[i], chF_256[i], output_shift);
          mag = simde_mm256_packs_epi32(mag, mag);
          mag = simde_mm256_unpacklo_epi16(mag, mag);
          maga_256[i] = simde_mm256_add_epi16(maga_256[i], simde_mm256_mulhrs_epi16(mag, QAM_ampa_256));
          if (mod_order > 4)
            magb_256[i] = simde_mm256_add_epi16(magb_256[i], simde_mm256_mulhrs_epi16(mag, QAM_ampb_256));
          if (mod_order > 6)
            magc_256[i] = simde_mm256_add_epi16(magc_256[i], simde_mm256_mulhrs_epi16(mag, QAM_ampc_256));
        }
      }
    }

    // Per-RE LLR for this tile's valid REs, straight from the L1 scratch
    int16_t *llr_off = llr + (size_t)base * mod_order;
    switch (mod_order) {
      case 2: nr_qpsk_llr(rxComp_t, llr_off, tre); break;
      case 4: nr_16qam_llr(rxComp_t, maga_t, llr_off, tre); break;
      case 6: nr_64qam_llr(rxComp_t, maga_t, magb_t, llr_off, tre); break;
      case 8: nr_256qam_llr(rxComp_t, maga_t, magb_t, magc_t, llr_off, tre); break;
      default: break;
    }
  }
}
