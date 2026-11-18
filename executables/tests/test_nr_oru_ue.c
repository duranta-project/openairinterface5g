/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * O-RU UE south side against a client in shared memory. Flat: DL/UL reciprocity, antenna
 * mapping, silence, PRACH bins and size check. CDL: per-RE reciprocity.
 */

#include <assert.h>
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "executables/nr-oru-ue.h"
#include "executables/ue_low_phy/port.h"
#include "common/utils/LOG/log.h"

#define FFT 512
#define N_RB 24
#define N_RE (N_RB * 12)
#define SPS 14
#define SPF 20

static unsigned seed = 1;

static void fill_random(c16_t *x, int n)
{
  for (int i = 0; i < n; i++)
    x[i] = (c16_t){rand_r(&seed) % 8001 - 4000, rand_r(&seed) % 8001 - 4000};
}

static double complex cx(c16_t v)
{
  return v.r + I * v.i;
}

// y = g x over n entries: g by least squares; true when the residual is within rounding.
static bool scaled(const c16_t *x, const c16_t *y, int n, double complex *g)
{
  double complex num = 0;
  double den = 0;
  for (int i = 0; i < n; i++) {
    num += cx(y[i]) * conj(cx(x[i]));
    den += cabs(cx(x[i])) * cabs(cx(x[i]));
  }
  *g = num / den;
  double worst = 0;
  for (int i = 0; i < n; i++)
    worst = fmax(worst, cabs(cx(y[i]) - *g * cx(x[i])));
  return worst <= 2;
}

static bool silent(const c16_t *y, int n)
{
  for (int i = 0; i < n; i++)
    if (y[i].r || y[i].i)
      return false;
  return true;
}

// Per RE gains y[k] / x[k]
static void gains(const c16_t *x, const c16_t *y, double complex *g)
{
  for (int k = 0; k < N_RE; k++)
    g[k] = cx(y[k]) / cx(x[k]);
}

static void reciprocity(const NR_DL_FRAME_PARMS *fp)
{
  char name[64];
  snprintf(name, sizeof(name), "test_nr_oru_ue_cdl_%d", getpid());
  const oru_ue_config_t cfg = {.port_name = name,
                               .max_ues = 1,
                               .ue_nb_rx = 2,
                               .ue_nb_tx = 1,
                               .channel = ORU_UE_CHANNEL_CDL,
                               .cdl_model = 0,
                               .delay_spread_ns = 300,
                               .txru_rows = 1,
                               .txru_cols = 2,
                               .txru_dv = 0.5,
                               .txru_dh = 0.5,
                               .seed = 3};
  oru_ue_t *ues = oru_ue_create(&cfg, fp, 2);
  const uint64_t first_slot = 2000 * SPF;
  oru_ue_start(ues, first_slot * SPS);
  ue_low_phy_port_t *port = ue_low_phy_port_attach(name, 1000);
  assert(port);
  ue_low_phy_client_t c;
  assert(ue_low_phy_client_open(port, &c) == 0);
  // Constant REs: the gains are the channel itself (no division by small values)
  c16_t x[N_RE] __attribute__((aligned(32))), zero[N_RE] __attribute__((aligned(32)));
  for (int k = 0; k < N_RE; k++)
    x[k] = (c16_t){3000, -2000};
  memset(zero, 0, sizeof(zero));
  // DL from each TXRU alone, UL from UE antenna 0 in one slot, all before the channel thread moves on
  double complex dl[2][N_RE];
  c16_t y[2][FFT];
  c16_t *dst[2] = {y[0], y[1]};
  for (int a = 0; a < 2; a++) {
    const c16_t *re[2] = {a == 0 ? x : zero, a == 1 ? x : zero};
    oru_ue_dl_symbol(ues, first_slot * SPS + a, re);
  }
  const uint64_t ul_slot = first_slot + 1;
  assert(ue_low_phy_client_join(&c, ul_slot, 0));
  c16_t *up = x;
  ue_low_phy_client_tx_symbol(&c, ul_slot * SPS, &up, 1);
  ue_low_phy_client_slot_done(&c, ul_slot, 1, -1, 0);
  oru_ue_ul_take_slot(ues, first_slot);
  oru_ue_ul_take_slot(ues, ul_slot);
  c16_t a0[FFT] __attribute__((aligned(32))), a1[FFT] __attribute__((aligned(32)));
  c16_t *ant[2] = {a0, a1};
  assert(oru_ue_ul_symbol(ues, (ul_slot / SPF) % 1024, ul_slot % SPF, 0, ant));
  oru_ue_dl_publish(ues, (first_slot + 1) * SPS);
  for (int a = 0; a < 2; a++) {
    assert(ue_low_phy_client_rx_symbol(&c, first_slot * SPS + a, dst, 2));
    gains(x, y[0], dl[a]);
  }
  double spread = 0, worst = 0;
  for (int a = 0; a < 2; a++) {
    c16_t shifted[N_RE];
    memcpy(shifted, ant[a] + FFT - N_RE / 2, N_RE / 2 * sizeof(c16_t));
    memcpy(shifted + N_RE / 2, ant[a], N_RE / 2 * sizeof(c16_t));
    double complex ul[N_RE];
    gains(x, shifted, ul);
    for (int k = 0; k < N_RE; k++) {
      worst = fmax(worst, cabs(ul[k] - dl[a][k]));
      spread = fmax(spread, cabs(dl[a][k] - dl[a][0]));
    }
  }
  assert(spread > 0.1); // frequency selective
  assert(worst < 0.002); // rounding of the two products
  ue_low_phy_port_destroy(port);
  oru_ue_stop(ues);
  oru_ue_destroy(ues);
}

int main(void)
{
  logInit();
  const NR_DL_FRAME_PARMS fp = {.ofdm_symbol_size = FFT,
                                .N_RB_DL = N_RB,
                                .N_RB_UL = N_RB,
                                .symbols_per_slot = SPS,
                                .slots_per_frame = SPF,
                                .numerology_index = 1,
                                .dl_CarrierFreq = 3500000000};
  char name[64];
  snprintf(name, sizeof(name), "test_nr_oru_ue_%d", getpid());
  const oru_ue_config_t cfg = {.port_name = name, .max_ues = 2, .ue_nb_rx = 2, .ue_nb_tx = 1, .channel = ORU_UE_CHANNEL_FLAT};
  oru_ue_t *ues = oru_ue_create(&cfg, &fp, 2);
  const uint64_t first_slot = 1000 * SPF;
  oru_ue_start(ues, first_slot * SPS);

  ue_low_phy_port_t *port = ue_low_phy_port_attach(name, 1000);
  assert(port);
  ue_low_phy_client_t c;
  assert(ue_low_phy_client_open(port, &c) == 0);
  uint64_t symbol0;
  assert(ue_low_phy_client_time(&c, &symbol0, 0) && symbol0 == 0);

  // DL: UE antenna r receives TXRU r through the UE's gain and phase
  c16_t x[2][N_RE] __attribute__((aligned(32)));
  fill_random(x[0], N_RE);
  fill_random(x[1], N_RE);
  const c16_t *re[2] = {x[0], x[1]};
  const uint64_t dl_symbol = first_slot * SPS + 3;
  oru_ue_dl_symbol(ues, dl_symbol, re);
  oru_ue_dl_publish(ues, (first_slot + 1) * SPS);
  c16_t y[2][FFT];
  c16_t *dst[2] = {y[0], y[1]};
  assert(ue_low_phy_client_rx_symbol(&c, dl_symbol, dst, 2));
  double complex g[2];
  for (int r = 0; r < 2; r++) {
    assert(scaled(x[r], y[r], N_RE, &g[r]));
    assert(fabs(cabs(g[r]) - 1) < 0.01);
    assert(silent(y[r] + N_RE, FFT - N_RE));
  }
  assert(cabs(g[0] - g[1]) > 0.01); // a phase per antenna pair
  // A symbol nobody wrote is silent
  assert(ue_low_phy_client_rx_symbol(&c, dl_symbol + 2, dst, 2));
  assert(silent(y[0], FFT) && silent(y[1], FFT));

  // UL: the UE's TX antenna 0 (its RX antenna 0) reaches TXRU 0 through the same gain and phase
  const uint64_t ul_slot = first_slot + 2;
  assert(ue_low_phy_client_join(&c, ul_slot, 0));
  c16_t u[N_RE] __attribute__((aligned(32)));
  fill_random(u, N_RE);
  c16_t *up = u;
  ue_low_phy_client_tx_symbol(&c, ul_slot * SPS + 4, &up, 1);
  c16_t bins[1536];
  fill_random(bins, 1536);
  const int prach = ue_low_phy_client_tx_prach(&c, ul_slot, 0, 0, 1536, 1, bins);
  assert(prach >= 0);
  ue_low_phy_client_slot_done(&c, ul_slot, 1u << 4, prach, 0);
  for (uint64_t s = first_slot; s <= ul_slot; s++)
    oru_ue_ul_take_slot(ues, s);
  const int frame = (ul_slot / SPF) % 1024, slot = ul_slot % SPF;
  c16_t a0[FFT] __attribute__((aligned(32))), a1[FFT] __attribute__((aligned(32)));
  c16_t *ant[2] = {a0, a1};
  assert(oru_ue_ul_symbol(ues, frame, slot, 4, ant));
  // FFT layout: the upper half of the carrier from DC, the lower half at fft - n_re / 2
  c16_t shifted[N_RE];
  memcpy(shifted, a0 + FFT - N_RE / 2, N_RE / 2 * sizeof(c16_t));
  memcpy(shifted + N_RE / 2, a0, N_RE / 2 * sizeof(c16_t));
  double complex gu;
  assert(scaled(u, shifted, N_RE, &gu));
  assert(cabs(gu - g[0]) < 0.01);
  assert(silent(a0 + N_RE / 2, FFT - N_RE));
  assert(silent(a1, FFT));
  // A symbol the UE did not post is silent; an untaken slot is reported
  assert(oru_ue_ul_symbol(ues, frame, slot, 5, ant) && silent(a0, FFT));
  assert(!oru_ue_ul_symbol(ues, frame, slot + 1, 0, ant));

  // PRACH: bins from k on (wrapping), through the same channel
  c16_t rxsigF[2][NR_PRACH_SEQ_LEN_L];
  for (int k = 100; k <= 1500; k += 1400) {
    oru_ue_prach(ues, frame, slot, k, 1536, 139, 2, rxsigF);
    c16_t expected[139];
    for (int j = 0; j < 139; j++)
      expected[j] = bins[(k + j) % 1536];
    double complex gp;
    assert(scaled(expected, rxsigF[0], 139, &gp));
    assert(cabs(gp - g[0]) < 0.01);
    assert(silent(rxsigF[1], 139));
  }
  oru_ue_prach(ues, frame, slot, 100, 1024, 139, 2, rxsigF); // the O-RU expects another PRACH
  assert(silent(rxsigF[0], 139));

  ue_low_phy_port_destroy(port);
  oru_ue_stop(ues);
  oru_ue_destroy(ues);

  reciprocity(&fp);
  printf("test_nr_oru_ue: OK\n");
  return 0;
}
