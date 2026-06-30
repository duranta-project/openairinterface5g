/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "gtest/gtest.h"

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

  combine_dl_streams(txDataF, 2, N_SC, streams, 1, &cb, rotation);

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

  combine_dl_streams(txDataF, 1, N_SC, streams, 2, &cb, rotation);

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

  combine_dl_streams(txDataF, 2, N_SC, streams, 1, &cb, rotation);

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
  combine_dl_streams(txDataF, 1, N_SC, streams, 2, &cb, rotation);

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
  combine_dl_streams(txDataF, 1, N_SC, streams, 2, &cb, rotation);

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
  combine_dl_streams(txDataF, 1, N_SC, streams, 2, &cb, rotation);

  c16_t *ant0 = (c16_t *)ant0_buf;
  c16_t w_rot = c16mulShift(cb.w[0][0][0], rotation, 15);
  for (int i = 0; i < NR_NB_SC_PER_RB; i++) {
    // Only the second stream (falling back to beam 0) should have contributed.
    c16_t expected = c16mulShift(((c16_t *)iq_fallback)[i], w_rot, 15);
    EXPECT_EQ(ant0[i].r, expected.r);
    EXPECT_EQ(ant0[i].i, expected.i);
  }
}

int main(int argc, char **argv)
{
  logInit();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
