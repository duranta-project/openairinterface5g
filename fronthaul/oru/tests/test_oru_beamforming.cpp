/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "gtest/gtest.h"
#include <algorithm>
#include <vector>

extern "C" {
#include <stdint.h>
#include <string.h>
#include "oru_beamforming.h"
#include "openair1/PHY/TOOLS/tools_defs.h"
#include "common/utils/nr/nr_common.h"
#include "log.h"
#include "common/config/config_userapi.h"

// OAI Linkage Satisfiers
void exit_function(const char *file, const char *function, const int line, const char *s, const int assertflag)
{
  fprintf(stderr, "Error at %s:%s:%d - %s\n", file, function, line, s ? s : "None");
  exit(1);
}
configmodule_interface_t *uniqCfg = NULL;
}

#define N_RB 2
#define N_SC (N_RB * NR_NB_SC_PER_RB)

static void fill_iq(uint32_t *buf, int n_re, int16_t base)
{
  c16_t *iq = (c16_t *)buf;
  for (int i = 0; i < n_re; i++) {
    iq[i].r = (int16_t)(base + i);
    iq[i].i = (int16_t)(base - i);
  }
}

static void assert_zero(const c16_t *buf, int n_re)
{
  for (int i = 0; i < n_re; i++) {
    EXPECT_EQ(buf[i].r, 0);
    EXPECT_EQ(buf[i].i, 0);
  }
}

// No codebook: placement only, but every RE still passes through the rotation multiply exactly
// once - single stream at PRB 0, PRB 1 must stay zero, other antenna must stay zero.
TEST(oru_beamforming, passthrough_single_stream)
{
  oru_codebook_t cb = {.nb_fh_streams = 0};
  c16_t rotation = {.r = 23170, .i = 23170}; // ~45 degrees, non-trivial

  uint32_t iq_buf[NR_NB_SC_PER_RB];
  fill_iq(iq_buf, NR_NB_SC_PER_RB, 100);

  dl_iq_stream_t streams[1] = {{.ant_id = 0, .beam_id = 7, .start_prb = 0, .num_prb = 1, .iq = iq_buf}};

  uint32_t ant0_buf[N_SC], ant1_buf[N_SC];
  c16_t *txDataF[2] = {(c16_t *)ant0_buf, (c16_t *)ant1_buf};

  combine_dl_streams(txDataF, 2, N_SC, streams, 1, &cb, NULL, rotation);

  c16_t *ant0 = (c16_t *)ant0_buf;
  const c16_t *src = (const c16_t *)iq_buf;
  for (int i = 0; i < NR_NB_SC_PER_RB; i++) {
    c16_t expected = c16mulShift(src[i], rotation, 15);
    EXPECT_EQ(ant0[i].r, expected.r);
    EXPECT_EQ(ant0[i].i, expected.i);
  }
  assert_zero(ant0 + NR_NB_SC_PER_RB, NR_NB_SC_PER_RB);
  assert_zero((c16_t *)ant1_buf, N_SC);
}

// Two streams on the same antenna, disjoint PRBs: both get placed at their own offset.
TEST(oru_beamforming, passthrough_multi_stream_placement)
{
  oru_codebook_t cb = {.nb_fh_streams = 0};
  c16_t rotation = {.r = 32767, .i = 0}; // ~identity, exercises the multiply without complicating expected values

  uint32_t iq_a[NR_NB_SC_PER_RB], iq_b[NR_NB_SC_PER_RB];
  fill_iq(iq_a, NR_NB_SC_PER_RB, 10);
  fill_iq(iq_b, NR_NB_SC_PER_RB, -40);

  dl_iq_stream_t streams[2] = {
      {.ant_id = 0, .beam_id = 1, .start_prb = 0, .num_prb = 1, .iq = iq_a},
      {.ant_id = 0, .beam_id = 2, .start_prb = 1, .num_prb = 1, .iq = iq_b},
  };

  uint32_t ant0_buf[N_SC];
  c16_t *txDataF[1] = {(c16_t *)ant0_buf};

  combine_dl_streams(txDataF, 1, N_SC, streams, 2, &cb, NULL, rotation);

  c16_t *ant0 = (c16_t *)ant0_buf;
  for (int i = 0; i < NR_NB_SC_PER_RB; i++) {
    c16_t exp_a = c16mulShift(((c16_t *)iq_a)[i], rotation, 15);
    c16_t exp_b = c16mulShift(((c16_t *)iq_b)[i], rotation, 15);
    EXPECT_EQ(ant0[i].r, exp_a.r);
    EXPECT_EQ(ant0[i].i, exp_a.i);
    EXPECT_EQ(ant0[NR_NB_SC_PER_RB + i].r, exp_b.r);
    EXPECT_EQ(ant0[NR_NB_SC_PER_RB + i].i, exp_b.i);
  }
}

// Codebook mode: one logical stream weight-combined onto two antennas with different weight
// vectors, with rotation folded into each weight.
TEST(oru_beamforming, codebook_single_stream_two_antennas)
{
  oru_codebook_t cb = {0};
  cb.nb_fh_streams = 1;
  cb.nb_beams = 1;
  cb.w[0][0][0] = (c16_t){.r = 20000, .i = 5000};
  cb.w[0][1][0] = (c16_t){.r = -8000, .i = 15000};
  c16_t rotation = {.r = 23170, .i = -23170};

  uint32_t iq_buf[NR_NB_SC_PER_RB];
  fill_iq(iq_buf, NR_NB_SC_PER_RB, 200);
  dl_iq_stream_t streams[1] = {{.ant_id = 0, .beam_id = 0, .start_prb = 0, .num_prb = 1, .iq = iq_buf}};

  uint32_t ant0_buf[N_SC], ant1_buf[N_SC];
  c16_t *txDataF[2] = {(c16_t *)ant0_buf, (c16_t *)ant1_buf};

  combine_dl_streams(txDataF, 2, N_SC, streams, 1, &cb, NULL, rotation);

  const c16_t *src = (const c16_t *)iq_buf;
  c16_t *ant0 = (c16_t *)ant0_buf;
  c16_t *ant1 = (c16_t *)ant1_buf;
  c16_t w0_rot = c16mulShift(cb.w[0][0][0], rotation, 15);
  c16_t w1_rot = c16mulShift(cb.w[0][1][0], rotation, 15);
  for (int i = 0; i < NR_NB_SC_PER_RB; i++) {
    c16_t exp0 = c16mulShift(src[i], w0_rot, 15);
    c16_t exp1 = c16mulShift(src[i], w1_rot, 15);
    EXPECT_EQ(ant0[i].r, exp0.r);
    EXPECT_EQ(ant0[i].i, exp0.i);
    EXPECT_EQ(ant1[i].r, exp1.r);
    EXPECT_EQ(ant1[i].i, exp1.i);
  }
  assert_zero(ant0 + NR_NB_SC_PER_RB, NR_NB_SC_PER_RB);
  assert_zero(ant1 + NR_NB_SC_PER_RB, NR_NB_SC_PER_RB);
}

// Sub-band beam switching: one stream, two PRB ranges, two beams - each range keeps its own weight.
TEST(oru_beamforming, codebook_sub_band_beam_switch)
{
  oru_codebook_t cb = {0};
  cb.nb_fh_streams = 1;
  cb.nb_beams = 2;
  cb.w[0][0][0] = (c16_t){.r = 30000, .i = 0};
  cb.w[1][0][0] = (c16_t){.r = 0, .i = 30000};
  c16_t rotation = {.r = 32767, .i = 0};

  uint32_t iq_a[NR_NB_SC_PER_RB], iq_b[NR_NB_SC_PER_RB];
  fill_iq(iq_a, NR_NB_SC_PER_RB, 50);
  fill_iq(iq_b, NR_NB_SC_PER_RB, 50); // same source pattern - only the weight/beam differs

  dl_iq_stream_t streams[2] = {
      {.ant_id = 0, .beam_id = 0, .start_prb = 0, .num_prb = 1, .iq = iq_a},
      {.ant_id = 0, .beam_id = 1, .start_prb = 1, .num_prb = 1, .iq = iq_b},
  };

  uint32_t ant0_buf[N_SC];
  c16_t *txDataF[1] = {(c16_t *)ant0_buf};
  combine_dl_streams(txDataF, 1, N_SC, streams, 2, &cb, NULL, rotation);

  c16_t *ant0 = (c16_t *)ant0_buf;
  c16_t w0_rot = c16mulShift(cb.w[0][0][0], rotation, 15);
  c16_t w1_rot = c16mulShift(cb.w[1][0][0], rotation, 15);
  for (int i = 0; i < NR_NB_SC_PER_RB; i++) {
    c16_t exp_lo = c16mulShift(((c16_t *)iq_a)[i], w0_rot, 15);
    c16_t exp_hi = c16mulShift(((c16_t *)iq_b)[i], w1_rot, 15);
    EXPECT_EQ(ant0[i].r, exp_lo.r);
    EXPECT_EQ(ant0[i].i, exp_lo.i);
    EXPECT_EQ(ant0[NR_NB_SC_PER_RB + i].r, exp_hi.r);
    EXPECT_EQ(ant0[NR_NB_SC_PER_RB + i].i, exp_hi.i);
  }
  // The two beams' weights are different (real-only vs imaginary-only), so unless sub-band
  // switching actually worked, the low/high PRB halves would be identical - assert they differ.
  EXPECT_NE(memcmp(ant0, ant0 + NR_NB_SC_PER_RB, NR_NB_SC_PER_RB * sizeof(c16_t)), 0);
}

// Two logical streams contributing to the SAME PRB range of the same physical antenna: their
// weighted contributions must accumulate (spatial combination), not overwrite each other.
TEST(oru_beamforming, codebook_stream_accumulation)
{
  oru_codebook_t cb = {0};
  cb.nb_fh_streams = 2;
  cb.nb_beams = 1;
  cb.w[0][0][0] = (c16_t){.r = 15000, .i = 0};
  cb.w[0][0][1] = (c16_t){.r = 10000, .i = 0};
  c16_t rotation = {.r = 32767, .i = 0};

  uint32_t iq_a[NR_NB_SC_PER_RB], iq_b[NR_NB_SC_PER_RB];
  fill_iq(iq_a, NR_NB_SC_PER_RB, 300);
  fill_iq(iq_b, NR_NB_SC_PER_RB, -100);

  dl_iq_stream_t streams[2] = {
      {.ant_id = 0, .beam_id = 0, .start_prb = 0, .num_prb = 1, .iq = iq_a},
      {.ant_id = 1, .beam_id = 0, .start_prb = 0, .num_prb = 1, .iq = iq_b},
  };

  uint32_t ant0_buf[N_SC];
  c16_t *txDataF[1] = {(c16_t *)ant0_buf};
  combine_dl_streams(txDataF, 1, N_SC, streams, 2, &cb, NULL, rotation);

  c16_t *ant0 = (c16_t *)ant0_buf;
  c16_t wa_rot = c16mulShift(cb.w[0][0][0], rotation, 15);
  c16_t wb_rot = c16mulShift(cb.w[0][0][1], rotation, 15);
  for (int i = 0; i < NR_NB_SC_PER_RB; i++) {
    c16_t term_a = c16mulShift(((c16_t *)iq_a)[i], wa_rot, 15);
    c16_t term_b = c16mulShift(((c16_t *)iq_b)[i], wb_rot, 15);
    EXPECT_EQ(ant0[i].r, (int16_t)(term_a.r + term_b.r));
    EXPECT_EQ(ant0[i].i, (int16_t)(term_a.i + term_b.i));
  }
}

// Out-of-range ant_id (>= nb_fh_streams) is dropped entirely; out-of-range beam_id falls back to
// beam 0 rather than indexing out of bounds.
TEST(oru_beamforming, codebook_out_of_range_fallbacks)
{
  oru_codebook_t cb = {0};
  cb.nb_fh_streams = 1;
  cb.nb_beams = 1;
  cb.w[0][0][0] = (c16_t){.r = 32767, .i = 0};
  c16_t rotation = {.r = 32767, .i = 0};

  uint32_t iq_dropped[NR_NB_SC_PER_RB], iq_fallback[NR_NB_SC_PER_RB];
  fill_iq(iq_dropped, NR_NB_SC_PER_RB, 500);
  fill_iq(iq_fallback, NR_NB_SC_PER_RB, 500);

  dl_iq_stream_t streams[2] = {
      {.ant_id = 5, .beam_id = 0, .start_prb = 0, .num_prb = 1, .iq = iq_dropped}, // ant_id out of range
      {.ant_id = 0, .beam_id = 99, .start_prb = 0, .num_prb = 1, .iq = iq_fallback}, // beam_id out of range -> beam 0
  };

  uint32_t ant0_buf[N_SC];
  c16_t *txDataF[1] = {(c16_t *)ant0_buf};
  combine_dl_streams(txDataF, 1, N_SC, streams, 2, &cb, NULL, rotation);

  c16_t *ant0 = (c16_t *)ant0_buf;
  c16_t w_rot = c16mulShift(cb.w[0][0][0], rotation, 15);
  for (int i = 0; i < NR_NB_SC_PER_RB; i++) {
    // Only the second stream (falling back to beam 0) should have contributed.
    c16_t expected = c16mulShift(((c16_t *)iq_fallback)[i], w_rot, 15);
    EXPECT_EQ(ant0[i].r, expected.r);
    EXPECT_EQ(ant0[i].i, expected.i);
  }
}

static c16_t conj16(c16_t w)
{
  return (c16_t){.r = w.r, .i = (int16_t)-w.i};
}

// UL Rx beamforming: per-antenna FFT output combined into one beam stream in the frequency
// domain, out[k] = conj(w[0])*X0[k] + conj(w[1])*X1[k], the same saturating multiply-accumulate as
// the DL codebook path. The inputs are FFT windows (what nr_symbol_fep_ul() produces per antenna).
TEST(oru_beamforming, ul_combine_two_antennas)
{
  oru_codebook_t cb = {0};
  cb.nb_fh_streams = 1;
  cb.nb_beams = 1;
  cb.w[0][0][0] = (c16_t){.r = 20000, .i = 5000};
  cb.w[0][1][0] = (c16_t){.r = -8000, .i = 15000};

  uint32_t buf0[N_SC], buf1[N_SC];
  fill_iq(buf0, N_SC, 100);
  fill_iq(buf1, N_SC, -50);
  const c16_t *fft[2] = {(c16_t *)buf0, (c16_t *)buf1};
  c16_t out[N_SC];
  combine_ul_beam_fd(fft, 2, 0, N_SC, &cb, 0, 0, out);

  for (int i = 0; i < N_SC; i++) {
    c16_t term0 = c16mulShift(((c16_t *)buf0)[i], conj16(cb.w[0][0][0]), 15);
    c16_t term1 = c16mulShift(((c16_t *)buf1)[i], conj16(cb.w[0][1][0]), 15);
    EXPECT_EQ(out[i].r, (int16_t)(term0.r + term1.r));
    EXPECT_EQ(out[i].i, (int16_t)(term0.i + term1.i));
  }
}

// Two beams from the same antennas: each gets its own weight vector, so the outputs differ
// even though the FFT input is identical (sub-band beam switching in UL is the same machinery
// at the job level - one beam per C-plane section).
TEST(oru_beamforming, ul_combine_two_beams)
{
  oru_codebook_t cb = {0};
  cb.nb_fh_streams = 1;
  cb.nb_beams = 2;
  cb.w[0][0][0] = (c16_t){.r = 32767, .i = 0};
  cb.w[1][0][0] = (c16_t){.r = 20000, .i = 10000};

  uint32_t buf[N_SC];
  fill_iq(buf, N_SC, 300);
  const c16_t *fft[1] = {(c16_t *)buf};

  c16_t out0[N_SC], out1[N_SC];
  combine_ul_beam_fd(fft, 1, 0, N_SC, &cb, 0, 0, out0);
  combine_ul_beam_fd(fft, 1, 0, N_SC, &cb, 1, 0, out1);

  for (int i = 0; i < N_SC; i++) {
    c16_t exp0 = c16mulShift(((c16_t *)buf)[i], conj16(cb.w[0][0][0]), 15);
    c16_t exp1 = c16mulShift(((c16_t *)buf)[i], conj16(cb.w[1][0][0]), 15);
    EXPECT_EQ(out0[i].r, exp0.r);
    EXPECT_EQ(out0[i].i, exp0.i);
    EXPECT_EQ(out1[i].r, exp1.r);
    EXPECT_EQ(out1[i].i, exp1.i);
  }
  EXPECT_NE(memcmp(out0, out1, N_SC * sizeof(c16_t)), 0);
}

// Stream index (the section's eaxc in codebook mode) out of range is dropped to silence; beam_id
// out of range falls back to beam 0, mirroring the DL behavior.
TEST(oru_beamforming, ul_combine_fallbacks)
{
  oru_codebook_t cb = {0};
  cb.nb_fh_streams = 1;
  cb.nb_beams = 2;
  cb.w[0][0][0] = (c16_t){.r = 32767, .i = 0};
  cb.w[1][0][0] = (c16_t){.r = 20000, .i = 10000};

  uint32_t buf[N_SC];
  fill_iq(buf, N_SC, 300);
  const c16_t *fft[1] = {(c16_t *)buf};

  c16_t out_zero[N_SC];
  combine_ul_beam_fd(fft, 1, 0, N_SC, &cb, 0, 5, out_zero); // stream out of range
  assert_zero(out_zero, N_SC);

  c16_t out_beam[N_SC];
  combine_ul_beam_fd(fft, 1, 0, N_SC, &cb, 99, 0, out_beam); // beam out of range
  for (int i = 0; i < N_SC; i++) {
    c16_t expected = c16mulShift(((c16_t *)buf)[i], conj16(cb.w[0][0][0]), 15);
    EXPECT_EQ(out_beam[i].r, expected.r);
    EXPECT_EQ(out_beam[i].i, expected.i);
  }
}

// Beam direction, not just arithmetic: a UE in the direction of DL beam 1 is received through a
// channel proportional to beam 1's weights, X_a = w1[a] * s. Combining with beam 1 must recover s
// at full array gain, and the orthogonal beam 2 must null it. (Combining with w instead of w^H
// swaps the two: beam 1 would null the UE it serves.)
TEST(oru_beamforming, ul_combine_steers_to_dl_beam_direction)
{
  const int16_t a = 23170; // 1/sqrt(2): unit-norm 2-element beams
  oru_codebook_t cb = {0};
  cb.nb_fh_streams = 1;
  cb.nb_beams = 2;
  cb.w[0][0][0] = (c16_t){.r = a, .i = 0};
  cb.w[0][1][0] = (c16_t){.r = 0, .i = a}; // beam 0: [1, j] / sqrt(2)
  cb.w[1][0][0] = (c16_t){.r = a, .i = 0};
  cb.w[1][1][0] = (c16_t){.r = 0, .i = (int16_t)-a}; // beam 1: [1, -j] / sqrt(2), orthogonal

  const c16_t sig = {.r = 12000, .i = -7000};
  c16_t x0[N_SC], x1[N_SC];
  for (int i = 0; i < N_SC; i++) {
    x0[i] = c16mulShift(sig, cb.w[0][0][0], 15);
    x1[i] = c16mulShift(sig, cb.w[0][1][0], 15);
  }
  const c16_t *fft[2] = {x0, x1};

  c16_t in_beam[N_SC], other_beam[N_SC];
  combine_ul_beam_fd(fft, 2, 0, N_SC, &cb, 0, 0, in_beam);
  combine_ul_beam_fd(fft, 2, 0, N_SC, &cb, 1, 0, other_beam);
  for (int i = 0; i < N_SC; i++) {
    EXPECT_NEAR(in_beam[i].r, sig.r, 4);
    EXPECT_NEAR(in_beam[i].i, sig.i, 4);
    EXPECT_NEAR(other_beam[i].r, 0, 4);
    EXPECT_NEAR(other_beam[i].i, 0, 4);
  }
}

// Only bins [k0, k0 + nk) are written: the caller combines just the PRBs a job sends.
TEST(oru_beamforming, ul_combine_bin_range_only)
{
  oru_codebook_t cb = {0};
  cb.nb_fh_streams = 1;
  cb.nb_beams = 1;
  cb.w[0][0][0] = (c16_t){.r = 20000, .i = 5000};

  uint32_t buf[N_SC];
  fill_iq(buf, N_SC, 300);
  const c16_t *fft[1] = {(c16_t *)buf};
  const c16_t sentinel = {.r = 1234, .i = -4321};
  c16_t out[N_SC];
  for (int i = 0; i < N_SC; i++) {
    out[i] = sentinel;
  }
  const int k0 = 6, nk = 10;
  combine_ul_beam_fd(fft, 1, k0, nk, &cb, 0, 0, out);
  for (int i = 0; i < N_SC; i++) {
    c16_t expected = i >= k0 && i < k0 + nk ? c16mulShift(((c16_t *)buf)[i], conj16(cb.w[0][0][0]), 15) : sentinel;
    EXPECT_EQ(out[i].r, expected.r) << i;
    EXPECT_EQ(out[i].i, expected.i) << i;
  }
}

// The PRB range maps onto the unshifted FFT window the way fftshift() lays it out: every RE of the
// range equals the full-window combine at its bin, every other bin is untouched. Odd and even PRB
// counts, ranges on either side of DC and straddling it, and a range clamped at the band edge.
TEST(oru_beamforming, ul_combine_prbs_matches_full_window)
{
  oru_codebook_t cb = {0};
  cb.nb_fh_streams = 1;
  cb.nb_beams = 1;
  cb.w[0][0][0] = (c16_t){.r = 20000, .i = 5000};
  cb.w[0][1][0] = (c16_t){.r = -8000, .i = 15000};
  const c16_t sentinel = {.r = 1234, .i = -4321};
  const struct {
    int fft_size, n_rb, start, num;
  } cases[] = {{1024, 51, 0, 51}, {1024, 51, 3, 10}, {1024, 51, 30, 12}, {1024, 51, 20, 15},
               {2048, 106, 0, 106}, {2048, 106, 50, 6}, {2048, 106, 100, 20}, {4096, 273, 131, 17}};
  for (const auto &c : cases) {
    std::vector<c16_t> x0(c.fft_size), x1(c.fft_size), full(c.fft_size), out(c.fft_size, sentinel);
    for (int k = 0; k < c.fft_size; k++) {
      x0[k] = (c16_t){.r = (int16_t)(k * 7 % 3000), .i = (int16_t)(-k * 3 % 2000)};
      x1[k] = (c16_t){.r = (int16_t)(-k * 5 % 2500), .i = (int16_t)(k * 11 % 1500)};
    }
    const c16_t *fft[2] = {x0.data(), x1.data()};
    combine_ul_beam_fd(fft, 2, 0, c.fft_size, &cb, 0, 0, full.data());
    const int nbins = c.n_rb * NR_NB_SC_PER_RB, half = nbins / 2;
    combine_ul_beam_prbs(fft, 2, c.fft_size, nbins, c.start, c.num, &cb, 0, 0, NULL, NULL, out.data());

    std::vector<bool> in_range(c.fft_size, false);
    const int end = std::min(c.start + c.num, c.n_rb) * NR_NB_SC_PER_RB;
    for (int re = c.start * NR_NB_SC_PER_RB; re < end; re++) {
      in_range[re < half ? c.fft_size - half + re : re - half] = true;
    }
    for (int k = 0; k < c.fft_size; k++) {
      const c16_t expected = in_range[k] ? full[k] : sentinel;
      ASSERT_EQ(out[k].r, expected.r) << "fft " << c.fft_size << " rb " << c.n_rb << " start " << c.start << " bin " << k;
      ASSERT_EQ(out[k].i, expected.i) << "fft " << c.fft_size << " rb " << c.n_rb << " start " << c.start << " bin " << k;
    }
  }
}

// Stand-in for get_bf_weights(): bundle b returns ret[b] with beam_id[b] and w[b]
struct fake_bfw {
  int ret[4];
  uint16_t beam_id[4];
  c16_t w[4][ORU_CODEBOOK_MAX_NB_TX];
  int calls;
};

static int fake_get_bf_weights(void *ctx, const oru_bfw_set_t *set, int bundle, uint16_t *beam_id, c16_t *weights)
{
  fake_bfw *f = (fake_bfw *)ctx;
  f->calls++;
  EXPECT_GE(bundle, 0);
  EXPECT_LT(bundle, set->num_bundles);
  if (f->ret[bundle] >= 0)
    *beam_id = f->beam_id[bundle];
  if (f->ret[bundle] > 0)
    memcpy(weights, f->w[bundle], f->ret[bundle] * sizeof(c16_t));
  return f->ret[bundle];
}

// Expected DL output of PRBs [prb, prb + n) of src on antenna txru for weight w
static void expect_dl_run(const c16_t *ant, const c16_t *src, int prb, int n, c16_t w, c16_t rotation, int txru)
{
  c16_t w_rot = c16mulShiftSat(w, rotation, 15);
  for (int re = prb * NR_NB_SC_PER_RB; re < (prb + n) * NR_NB_SC_PER_RB; re++) {
    c16_t expected = c16mulShift(src[re - prb * NR_NB_SC_PER_RB], w_rot, 15);
    ASSERT_EQ(ant[re].r, expected.r) << "txru " << txru << " re " << re;
    ASSERT_EQ(ant[re].i, expected.i) << "txru " << txru << " re " << re;
  }
}

// Weights from ext1 (one bundle covering the section) replace the codebook row of the section beamId
TEST(oru_beamforming, dl_dynamic_weights_override_codebook)
{
  oru_codebook_t cb = {0};
  cb.nb_fh_streams = 1;
  cb.nb_beams = 4;
  cb.w[3][0][0] = (c16_t){.r = 20000, .i = 5000};
  cb.w[3][1][0] = (c16_t){.r = -8000, .i = 15000};
  fake_bfw f = {.ret = {2}, .beam_id = {3}, .w = {{{.r = 1000, .i = -30000}, {.r = 25000, .i = 7000}}}};
  const oru_bfw_source_t bfw = {.ctx = &f, .get = fake_get_bf_weights};
  c16_t rotation = {.r = 23170, .i = -23170};

  uint32_t iq_buf[N_SC];
  fill_iq(iq_buf, N_SC, 200);
  dl_iq_stream_t streams[1] = {{.ant_id = 0, .beam_id = 3, .start_prb = 0, .num_prb = N_RB, .iq = iq_buf}};
  streams[0].bfw = (oru_bfw_set_t){.num_bundles = 1, .bundle_prbs = N_RB, .first_prb = 0};

  uint32_t ant_buf[2][N_SC];
  c16_t *txDataF[2] = {(c16_t *)ant_buf[0], (c16_t *)ant_buf[1]};
  combine_dl_streams(txDataF, 2, N_SC, streams, 1, &cb, &bfw, rotation);
  EXPECT_EQ(f.calls, 1);
  for (int txru = 0; txru < 2; txru++)
    expect_dl_run(txDataF[txru], (const c16_t *)iq_buf, 0, N_RB, f.w[0][txru], rotation, txru);
}

// Without dynamic weights the codebook result is the same as with no weight source, and get() isn't called
TEST(oru_beamforming, dl_no_dynamic_weights_uses_codebook)
{
  oru_codebook_t cb = {0};
  cb.nb_fh_streams = 1;
  cb.nb_beams = 2;
  cb.w[1][0][0] = (c16_t){.r = 20000, .i = 5000};
  cb.w[1][1][0] = (c16_t){.r = -8000, .i = 15000};
  fake_bfw f = {.ret = {2}, .beam_id = {1}, .w = {{{.r = 1000, .i = -30000}, {.r = 25000, .i = 7000}}}};
  const oru_bfw_source_t bfw = {.ctx = &f, .get = fake_get_bf_weights};
  c16_t rotation = {.r = 23170, .i = 23170};

  uint32_t iq_buf[N_SC];
  fill_iq(iq_buf, N_SC, -300);
  dl_iq_stream_t streams[1] = {{.ant_id = 0, .beam_id = 1, .start_prb = 0, .num_prb = N_RB, .iq = iq_buf}};

  uint32_t with_src[2][N_SC], without_src[2][N_SC];
  c16_t *tx_with[2] = {(c16_t *)with_src[0], (c16_t *)with_src[1]};
  c16_t *tx_without[2] = {(c16_t *)without_src[0], (c16_t *)without_src[1]};
  combine_dl_streams(tx_with, 2, N_SC, streams, 1, &cb, &bfw, rotation);
  combine_dl_streams(tx_without, 2, N_SC, streams, 1, &cb, NULL, rotation);
  EXPECT_EQ(f.calls, 0);
  EXPECT_EQ(memcmp(with_src, without_src, sizeof(with_src)), 0);
  for (int txru = 0; txru < 2; txru++)
    expect_dl_run(tx_with[txru], (const c16_t *)iq_buf, 0, N_RB, cb.w[1][txru][0], rotation, txru);
}

// ext11 bundles of 2 PRBs starting one PRB before the section (bundleOffset 1): bundle 0 has weights, bundle 1 only a
// beamId (codebook row of that beam), bundle 2 was overwritten (no output). A fragment that starts inside a bundle reads
// its IQ from the right offset.
TEST(oru_beamforming, dl_dynamic_weights_per_bundle)
{
  enum { NRB = 4, NSC = NRB * NR_NB_SC_PER_RB };
  oru_codebook_t cb = {0};
  cb.nb_fh_streams = 1;
  cb.nb_beams = 2;
  cb.w[0][0][0] = (c16_t){.r = 32767, .i = 0};
  cb.w[0][1][0] = (c16_t){.r = 0, .i = 32767};
  cb.w[1][0][0] = (c16_t){.r = 20000, .i = 5000};
  cb.w[1][1][0] = (c16_t){.r = -8000, .i = 15000};
  fake_bfw f = {.ret = {2, 0, -1}, .beam_id = {9, 1, 0}, .w = {{{.r = 1000, .i = -30000}, {.r = 25000, .i = 7000}}}};
  const oru_bfw_source_t bfw = {.ctx = &f, .get = fake_get_bf_weights};
  const oru_bfw_set_t set = {.num_bundles = 3, .bundle_prbs = 2, .first_prb = -1};
  c16_t rotation = {.r = 23170, .i = -23170};
  const c16_t zero = {0};
  const c16_t *expected_w[NRB][2] = {{&f.w[0][0], &f.w[0][1]},
                                     {&cb.w[1][0][0], &cb.w[1][1][0]},
                                     {&cb.w[1][0][0], &cb.w[1][1][0]},
                                     {&zero, &zero}};

  uint32_t iq_buf[NSC];
  fill_iq(iq_buf, NSC, 150);
  const struct {
    int start, num;
  } frags[] = {{0, NRB}, {2, 2}};
  for (const auto &fr : frags) {
    dl_iq_stream_t streams[1] = {{.ant_id = 0, .beam_id = 0, .start_prb = fr.start, .num_prb = fr.num, .iq = iq_buf}};
    streams[0].bfw = set;
    uint32_t ant_buf[2][NSC];
    c16_t *txDataF[2] = {(c16_t *)ant_buf[0], (c16_t *)ant_buf[1]};
    combine_dl_streams(txDataF, 2, NSC, streams, 1, &cb, &bfw, rotation);
    for (int txru = 0; txru < 2; txru++) {
      assert_zero(txDataF[txru], fr.start * NR_NB_SC_PER_RB);
      for (int prb = fr.start; prb < fr.start + fr.num; prb++)
        expect_dl_run(txDataF[txru],
                      (const c16_t *)iq_buf + (prb - fr.start) * NR_NB_SC_PER_RB,
                      prb,
                      1,
                      *expected_w[prb][txru],
                      rotation,
                      txru);
    }
  }
}

// UL bundles over a PRB range straddling DC: each run equals the codebook combine for that run's weights (dynamic weights
// as a codebook beam), every other bin is untouched. PRBs before the first bundle use the section beamId, an overwritten
// bundle is zero. More weights than RX antennas use the first nb_rx.
TEST(oru_beamforming, ul_dynamic_weights_per_bundle)
{
  const int fft_size = 1024, n_rb = 51, nbins = n_rb * NR_NB_SC_PER_RB, start = 16, num = 16;
  oru_codebook_t cb = {0};
  cb.nb_fh_streams = 1;
  cb.nb_beams = 2;
  cb.w[0][0][0] = (c16_t){.r = 32767, .i = 0};
  cb.w[0][1][0] = (c16_t){.r = 0, .i = 32767};
  cb.w[1][0][0] = (c16_t){.r = 20000, .i = 5000};
  cb.w[1][1][0] = (c16_t){.r = -8000, .i = 15000};
  fake_bfw f = {.ret = {2, 0, 3, -1},
                .beam_id = {5, 1, 6, 0},
                .w = {{{.r = 1000, .i = -30000}, {.r = 25000, .i = 7000}},
                      {},
                      {{.r = -12000, .i = 4000}, {.r = 9000, .i = 21000}, {.r = 5, .i = 5}}}};
  const oru_bfw_source_t bfw = {.ctx = &f, .get = fake_get_bf_weights};
  // Bundles of 4 PRBs from PRB 18: [18, 22), [22, 26), [26, 30), [30, 34)
  const oru_bfw_set_t set = {.num_bundles = 4, .bundle_prbs = 4, .first_prb = 18};

  // Reference codebook: beams 0 and 1 as above, 2 and 3 the dynamic weights of bundles 0 and 2, 4 all zero
  oru_codebook_t ref = cb;
  ref.nb_beams = 5;
  for (int a = 0; a < 2; a++) {
    ref.w[2][a][0] = f.w[0][a];
    ref.w[3][a][0] = f.w[2][a];
  }
  const struct {
    int start, num, beam;
  } runs[] = {{16, 2, 0}, {18, 4, 2}, {22, 4, 1}, {26, 4, 3}, {30, 2, 4}};

  const c16_t sentinel = {.r = 1234, .i = -4321};
  std::vector<c16_t> x0(fft_size), x1(fft_size), out(fft_size, sentinel), expected(fft_size, sentinel);
  for (int k = 0; k < fft_size; k++) {
    x0[k] = (c16_t){.r = (int16_t)(k * 7 % 3000), .i = (int16_t)(-k * 3 % 2000)};
    x1[k] = (c16_t){.r = (int16_t)(-k * 5 % 2500), .i = (int16_t)(k * 11 % 1500)};
  }
  const c16_t *fft[2] = {x0.data(), x1.data()};
  combine_ul_beam_prbs(fft, 2, fft_size, nbins, start, num, &cb, 0, 0, &bfw, &set, out.data());
  EXPECT_EQ(f.calls, 4);
  for (const auto &r : runs)
    combine_ul_beam_prbs(fft, 2, fft_size, nbins, r.start, r.num, &ref, r.beam, 0, NULL, NULL, expected.data());
  for (int k = 0; k < fft_size; k++) {
    ASSERT_EQ(out[k].r, expected[k].r) << "bin " << k;
    ASSERT_EQ(out[k].i, expected[k].i) << "bin " << k;
  }
}

// Without dynamic weights the UL combine is the same as with no weight source
TEST(oru_beamforming, ul_no_dynamic_weights_uses_codebook)
{
  const int fft_size = 1024, n_rb = 51, nbins = n_rb * NR_NB_SC_PER_RB;
  oru_codebook_t cb = {0};
  cb.nb_fh_streams = 1;
  cb.nb_beams = 2;
  cb.w[1][0][0] = (c16_t){.r = 20000, .i = 5000};
  cb.w[1][1][0] = (c16_t){.r = -8000, .i = 15000};
  fake_bfw f = {.ret = {2}};
  const oru_bfw_source_t bfw = {.ctx = &f, .get = fake_get_bf_weights};
  const oru_bfw_set_t set = {0};

  std::vector<c16_t> x0(fft_size), x1(fft_size), with_src(fft_size), without_src(fft_size);
  for (int k = 0; k < fft_size; k++) {
    x0[k] = (c16_t){.r = (int16_t)(k * 7 % 3000), .i = (int16_t)(-k * 3 % 2000)};
    x1[k] = (c16_t){.r = (int16_t)(-k * 5 % 2500), .i = (int16_t)(k * 11 % 1500)};
  }
  const c16_t *fft[2] = {x0.data(), x1.data()};
  combine_ul_beam_prbs(fft, 2, fft_size, nbins, 10, 30, &cb, 1, 0, &bfw, &set, with_src.data());
  combine_ul_beam_prbs(fft, 2, fft_size, nbins, 10, 30, &cb, 1, 0, NULL, NULL, without_src.data());
  EXPECT_EQ(f.calls, 0);
  EXPECT_EQ(memcmp(with_src.data(), without_src.data(), fft_size * sizeof(c16_t)), 0);
}

// PRACH: bin k is in PRB start_prb + (re_offset + k) / 12, so bundles split the sequence bins at PRB boundaries shifted by
// re_offset. Each run equals the codebook combine of its bins for that run's weights; a bundle without dynamic weights
// uses the codebook for its beamId, an overwritten bundle is zero, more weights than RX antennas use the first nb_rx, and
// bins from nk up are untouched.
TEST(oru_beamforming, prach_dynamic_weights_per_bundle)
{
  const int nk = 139, re_offset = 2, start_prb = 5, n_bins = 160;
  oru_codebook_t cb = {0};
  cb.nb_fh_streams = 1;
  cb.nb_beams = 2;
  cb.w[0][0][0] = (c16_t){.r = 32767, .i = 0};
  cb.w[0][1][0] = (c16_t){.r = 0, .i = 32767};
  cb.w[1][0][0] = (c16_t){.r = 20000, .i = 5000};
  cb.w[1][1][0] = (c16_t){.r = -8000, .i = 15000};
  fake_bfw f = {.ret = {2, 0, 3, -1},
                .beam_id = {5, 1, 6, 0},
                .w = {{{.r = 1000, .i = -30000}, {.r = 25000, .i = 7000}},
                      {},
                      {{.r = -12000, .i = 4000}, {.r = 9000, .i = 21000}, {.r = 5, .i = 5}}}};
  const oru_bfw_source_t bfw = {.ctx = &f, .get = fake_get_bf_weights};
  // Bundles of 4 PRBs from PRB 4 (bundleOffset 1): [4, 8), [8, 12), [12, 16), [16, 20). The sequence covers PRBs 5-16.
  const oru_bfw_set_t set = {.num_bundles = 4, .bundle_prbs = 4, .first_prb = 4};

  // Reference codebook: beams 0 and 1 as above, 2 and 3 the dynamic weights of bundles 0 and 2, 4 all zero
  oru_codebook_t ref = cb;
  ref.nb_beams = 5;
  for (int a = 0; a < 2; a++) {
    ref.w[2][a][0] = f.w[0][a];
    ref.w[3][a][0] = f.w[2][a];
  }
  // Bins of PRBs 5-7, 8-11, 12-15 and 16: PRB p starts at bin (p - start_prb) * 12 - re_offset
  const struct {
    int k0, nk, beam;
  } runs[] = {{0, 34, 2}, {34, 48, 1}, {82, 48, 3}, {130, 9, 4}};

  const c16_t sentinel = {.r = 1234, .i = -4321};
  std::vector<c16_t> x0(n_bins), x1(n_bins), out(n_bins, sentinel), expected(n_bins, sentinel);
  for (int k = 0; k < n_bins; k++) {
    x0[k] = (c16_t){.r = (int16_t)(k * 7 % 3000), .i = (int16_t)(-k * 3 % 2000)};
    x1[k] = (c16_t){.r = (int16_t)(-k * 5 % 2500), .i = (int16_t)(k * 11 % 1500)};
  }
  const c16_t *fft[2] = {x0.data(), x1.data()};
  combine_ul_prach_beam(fft, 2, nk, re_offset, start_prb, &cb, 0, 0, &bfw, &set, out.data());
  EXPECT_EQ(f.calls, 4);
  for (const auto &r : runs)
    combine_ul_beam_fd(fft, 2, r.k0, r.nk, &ref, r.beam, 0, expected.data());
  for (int k = 0; k < n_bins; k++) {
    ASSERT_EQ(out[k].r, expected[k].r) << "bin " << k;
    ASSERT_EQ(out[k].i, expected[k].i) << "bin " << k;
  }
}

// Without dynamic weights the PRACH combine is combine_ul_beam_fd() with the section beamId over all nk bins
TEST(oru_beamforming, prach_no_dynamic_weights_uses_codebook)
{
  const int nk = 139;
  oru_codebook_t cb = {0};
  cb.nb_fh_streams = 1;
  cb.nb_beams = 2;
  cb.w[1][0][0] = (c16_t){.r = 20000, .i = 5000};
  cb.w[1][1][0] = (c16_t){.r = -8000, .i = 15000};
  fake_bfw f = {.ret = {2}};
  const oru_bfw_source_t bfw = {.ctx = &f, .get = fake_get_bf_weights};
  const oru_bfw_set_t set = {0};

  std::vector<c16_t> x0(nk), x1(nk), with_src(nk), expected(nk);
  for (int k = 0; k < nk; k++) {
    x0[k] = (c16_t){.r = (int16_t)(k * 7 % 3000), .i = (int16_t)(-k * 3 % 2000)};
    x1[k] = (c16_t){.r = (int16_t)(-k * 5 % 2500), .i = (int16_t)(k * 11 % 1500)};
  }
  const c16_t *fft[2] = {x0.data(), x1.data()};
  combine_ul_prach_beam(fft, 2, nk, 2, 7, &cb, 1, 0, &bfw, &set, with_src.data());
  combine_ul_beam_fd(fft, 2, 0, nk, &cb, 1, 0, expected.data());
  EXPECT_EQ(f.calls, 0);
  EXPECT_EQ(memcmp(with_src.data(), expected.data(), nk * sizeof(c16_t)), 0);
}

int main(int argc, char **argv)
{
  logInit();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
