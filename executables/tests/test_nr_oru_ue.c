/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * O-RU UE south side against UEs on its FD channel. Flat: DL/UL reciprocity, antenna
 * mapping, silence, PRACH and its configuration check. CDL: per-RE reciprocity. Per-UE
 * channel models.
 */

#include <assert.h>
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "executables/nr-oru-ue.h"
#include <pthread.h>
#include "common/utils/shm_iq_channel/shm_fd_iq_channel.h"
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

static void wait_ready(oru_ue_t *ues, int ue)
{
  for (int i = 0; i < 5000 && !oru_ue_channel_ready(ues, ue); i++)
    usleep(1000);
  assert(oru_ue_channel_ready(ues, ue));
}

typedef struct {
  oru_ue_t *ues;
  uint64_t end;
} nudge_t;

static void *nudge(void *arg)
{
  nudge_t *n = arg;
  usleep(20000);
  oru_ue_dl_publish(n->ues, n->end);
  return NULL;
}

// Connects as UE ue; connecting needs a moving current symbol, so the O-RU publishes up to end
// meanwhile.
static ShmFDIQChannel *attach(oru_ue_t *ues, const char *name, int ue, uint64_t end)
{
  nudge_t n = {ues, end};
  pthread_t t;
  pthread_create(&t, NULL, nudge, &n);
  ShmFDIQChannel *ch = shm_fd_iq_channel_connect(name, 2000);
  pthread_join(t, NULL);
  assert(ch);
  wait_ready(ues, ue);
  return ch;
}

// Two UEs on a flat channel model: one at 0 dB, one at -6 dB.
static void per_ue_gain(const NR_DL_FRAME_PARMS *fp)
{
  char name[64];
  snprintf(name, sizeof(name), "test_nr_oru_ue_gain_%d", getpid());
  const oru_ue_channel_model_t models[2] = {{.type = ORU_UE_CHANNEL_MODEL_FLAT, .seed = 1},
                                           {.type = ORU_UE_CHANNEL_MODEL_FLAT, .gain_db = -6, .seed = 2}};
  const oru_ue_config_t cfg = {.channel_name = name, .num_ues = 2, .ue_nb_rx = 1, .ue_nb_tx = 1, .models = models, .txru_rows = 1};
  oru_ue_t *ues = oru_ue_create(&cfg, fp, 1);
  const uint64_t first_slot = 3000 * SPF;
  oru_ue_start(ues, first_slot * SPS);
  ShmFDIQChannel *a = attach(ues, name, 0, first_slot * SPS + 1);
  ShmFDIQChannel *b = attach(ues, name, 1, first_slot * SPS + 2);
  const uint64_t sym = (first_slot + 1) * SPS;
  c16_t x[N_RE] __attribute__((aligned(32)));
  fill_random(x, N_RE);
  const c16_t *re[1] = {x};
  oru_ue_dl_symbol(ues, sym, re);
  oru_ue_dl_publish(ues, sym + 1);
  c16_t y[N_RE];
  c16_t *dst[1] = {y};
  double complex g;
  assert(shm_fd_iq_channel_rx(a, sym, 0, 1, dst) == CHANNEL_NO_ERROR);
  assert(scaled(x, y, N_RE, &g) && fabs(cabs(g) - 1) < 0.01);
  assert(shm_fd_iq_channel_rx(b, sym, 1, 1, dst) == CHANNEL_NO_ERROR);
  assert(scaled(x, y, N_RE, &g) && fabs(cabs(g) - pow(10, -6.0 / 20)) < 0.01);
  shm_fd_iq_channel_destroy(a);
  shm_fd_iq_channel_destroy(b);
  oru_ue_destroy(ues);
}

// CDL-A, not moving: the model thread's updates leave it as it is.
static void reciprocity(const NR_DL_FRAME_PARMS *fp)
{
  char name[64];
  snprintf(name, sizeof(name), "test_nr_oru_ue_cdl_%d", getpid());
  const oru_ue_channel_model_t model = {.type = ORU_UE_CHANNEL_MODEL_CDL_A, .delay_spread_ns = 300, .speed_mps = 0, .seed = 3};
  const oru_ue_config_t cfg = {.channel_name = name,
                               .num_ues = 1,
                               .ue_nb_rx = 2,
                               .ue_nb_tx = 1,
                               .models = &model,
                               .txru_rows = 1,
                               .txru_cols = 2,
                               .txru_dv = 0.5,
                               .txru_dh = 0.5};
  oru_ue_t *ues = oru_ue_create(&cfg, fp, 2);
  const uint64_t first_slot = 2000 * SPF;
  oru_ue_start(ues, first_slot * SPS);
  ShmFDIQChannel *c = attach(ues, name, 0, first_slot * SPS + 1);
  // Constant REs: the gains are the channel itself (no division by small values)
  c16_t x[N_RE] __attribute__((aligned(32))), zero[N_RE] __attribute__((aligned(32)));
  for (int k = 0; k < N_RE; k++)
    x[k] = (c16_t){3000, -2000};
  memset(zero, 0, sizeof(zero));
  // DL from each TXRU alone, UL from UE antenna 0 in the next slot
  const uint64_t sym = (first_slot + 1) * SPS;
  for (int a = 0; a < 2; a++) {
    const c16_t *re[2] = {a == 0 ? x : zero, a == 1 ? x : zero};
    oru_ue_dl_symbol(ues, sym + a, re);
  }
  const uint64_t ul_slot = first_slot + 2;
  const c16_t *up = x;
  assert(shm_fd_iq_channel_tx(c, ul_slot * SPS, 0, 1, &up) == CHANNEL_NO_ERROR);
  oru_ue_dl_publish(ues, (ul_slot + 1) * SPS);
  double complex dl[2][N_RE];
  c16_t y[2][N_RE];
  c16_t *dst[2] = {y[0], y[1]};
  for (int a = 0; a < 2; a++) {
    assert(shm_fd_iq_channel_rx(c, sym + a, 0, 2, dst) == CHANNEL_NO_ERROR);
    gains(x, y[0], dl[a]);
  }
  c16_t a0[FFT] __attribute__((aligned(32))), a1[FFT] __attribute__((aligned(32)));
  c16_t *ant[2] = {a0, a1};
  assert(oru_ue_ul_symbol(ues, (ul_slot / SPF) % 1024, ul_slot % SPF, 0, ant));
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
  shm_fd_iq_channel_destroy(c);
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
  const oru_ue_channel_model_t models[2] = {{.type = ORU_UE_CHANNEL_MODEL_FLAT, .seed = 1}, {.type = ORU_UE_CHANNEL_MODEL_FLAT, .seed = 2}};
  const oru_ue_config_t cfg = {.channel_name = name, .num_ues = 2, .ue_nb_rx = 2, .ue_nb_tx = 1, .models = models, .txru_rows = 1};
  oru_ue_t *ues = oru_ue_create(&cfg, &fp, 2);
  const uint64_t first_slot = 1000 * SPF;
  oru_ue_start(ues, first_slot * SPS);
  ShmFDIQChannel *c = attach(ues, name, 0, first_slot * SPS + 1);

  // DL: UE antenna r receives TXRU r through the UE's gain and phase
  c16_t x[2][N_RE] __attribute__((aligned(32)));
  fill_random(x[0], N_RE);
  fill_random(x[1], N_RE);
  const c16_t *re[2] = {x[0], x[1]};
  const uint64_t dl_symbol = (first_slot + 1) * SPS + 3;
  oru_ue_dl_symbol(ues, dl_symbol, re);
  oru_ue_dl_publish(ues, (first_slot + 2) * SPS);
  c16_t y[2][N_RE];
  c16_t *dst[2] = {y[0], y[1]};
  assert(shm_fd_iq_channel_rx(c, dl_symbol, 0, 2, dst) == CHANNEL_NO_ERROR);
  double complex g[2];
  for (int r = 0; r < 2; r++) {
    assert(scaled(x[r], y[r], N_RE, &g[r]));
    assert(fabs(cabs(g[r]) - 1) < 0.01);
  }
  assert(cabs(g[0] - g[1]) > 0.01); // a phase per antenna pair
  // A symbol nobody wrote is silent
  assert(shm_fd_iq_channel_rx(c, dl_symbol + 2, 0, 2, dst) == CHANNEL_NO_ERROR);
  assert(silent(y[0], N_RE) && silent(y[1], N_RE));

  // UL: the UE's TX antenna 0 (its RX antenna 0) reaches TXRU 0 through the same gain and phase
  const uint64_t ul_slot = first_slot + 3;
  c16_t u[N_RE] __attribute__((aligned(32)));
  fill_random(u, N_RE);
  const c16_t *up = u;
  assert(shm_fd_iq_channel_tx(c, ul_slot * SPS + 4, 0, 1, &up) == CHANNEL_NO_ERROR);
  // PRACH: the sequence from bin 1500 on, wrapping
  fd_channel_prach_t prach = {.ue = 0, .dftlen = 1536, .k = 1500, .n_zc = 139};
  fill_random(prach.bins, 139);
  const c16_t *seq = prach.bins;
  assert(shm_fd_iq_channel_tx_msg(c, ul_slot * SPS, &prach, sizeof(prach)) == CHANNEL_NO_ERROR);
  oru_ue_dl_publish(ues, (ul_slot + 1) * SPS);
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
  // Read once; a symbol the UE did not post is silent
  assert(oru_ue_ul_symbol(ues, frame, slot, 4, ant) && silent(a0, FFT));
  assert(oru_ue_ul_symbol(ues, frame, slot, 5, ant) && silent(a0, FFT));

  // PRACH through the same channel
  c16_t rxsigF[2][NR_PRACH_SEQ_LEN_L];
  oru_ue_prach(ues, frame, slot, 1500, 1536, 139, 2, rxsigF);
  double complex gp;
  assert(scaled(seq, rxsigF[0], 139, &gp));
  assert(cabs(gp - g[0]) < 0.01);
  assert(silent(rxsigF[1], 139));
  oru_ue_prach(ues, frame, slot, 100, 1536, 139, 2, rxsigF); // the O-RU expects another PRACH
  assert(silent(rxsigF[0], 139));

  shm_fd_iq_channel_destroy(c);
  oru_ue_destroy(ues);

  reciprocity(&fp);
  per_ue_gain(&fp);
  printf("test_nr_oru_ue: OK\n");
  return 0;
}
