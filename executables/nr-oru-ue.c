/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#define _GNU_SOURCE
#include "nr-oru-ue.h"
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "common/utils/shm_iq_channel/shm_fd_iq_channel.h"
#include "openair1/SIMULATION/TOOLS/cdl_channel.h"
#include "PHY/MODULATION/nr_modulation.h"
#include "PHY/TOOLS/tools_defs.h"
#include "common/utils/LOG/log.h"
#include "common/utils/assertions.h"
#include "common/utils/system.h"

// Q12: 18 dB of headroom above unit gain for fading peaks
#define CHANNEL_SHIFT 12
#define CHANNEL_ONE (1 << CHANNEL_SHIFT)
#define MAX_UE_ANT 8

typedef struct {
  c16_t *h[2]; ///< [ue_nb_rx][nb_txru][n_re]: the active one is read, the other rewritten
  int active;
  bool built;
  cdl_channel_t *cdl;
  double scale; ///< CDL: to Q12 at unit average power, fixed at the first computation
  struct timespec updated;
} ue_model_t;

struct oru_ue {
  oru_ue_config_t cfg;
  NR_DL_FRAME_PARMS fp;
  int nb_txru;
  int n_re;
  ShmFDIQChannel *channel;
  bool started;
  oru_ue_channel_model_t *params; ///< [num_ues]
  ue_model_t *models; ///< [num_ues]
  double (*txru_pos)[3];
  double (*ue_pos)[3];
  pthread_mutex_t publish_lock;
  uint64_t published; ///< the channel's current symbol
  bool dl_late_warned;
  bool prach_warned;
  pthread_t model_thread;
  volatile int stop;
};

static double uniform(struct drand48_data *rng)
{
  double u;
  drand48_r(rng, &u);
  return u;
}

static int16_t sat16(int32_t v)
{
  return v > INT16_MAX ? INT16_MAX : v < INT16_MIN ? INT16_MIN : v;
}

static size_t model_size(const oru_ue_t *ues)
{
  return (size_t)ues->cfg.ue_nb_rx * ues->nb_txru * ues->n_re;
}

// UE c's H, NULL until built
static c16_t *ready_model(oru_ue_t *ues, int c)
{
  ue_model_t *m = &ues->models[c];
  if (!__atomic_load_n(&m->built, __ATOMIC_ACQUIRE))
    return NULL;
  return m->h[__atomic_load_n(&m->active, __ATOMIC_ACQUIRE)];
}

static void flat_model(oru_ue_t *ues, ue_model_t *m, const oru_ue_channel_model_t *p, struct drand48_data *rng)
{
  const double amplitude = CHANNEL_ONE * pow(10, p->gain_db / 20);
  const int next = !m->active;
  c16_t *h = m->h[next];
  for (int r = 0; r < ues->cfg.ue_nb_rx; r++) {
    for (int t = 0; t < ues->nb_txru; t++) {
      const double phase = 2 * M_PI * uniform(rng);
      const c16_t v = r % ues->nb_txru == t ? (c16_t){sat16(lround(amplitude * cos(phase))), sat16(lround(amplitude * sin(phase)))}
                                            : (c16_t){0, 0};
      c16_t *row = h + ((size_t)r * ues->nb_txru + t) * ues->n_re;
      for (int k = 0; k < ues->n_re; k++)
        row[k] = v;
    }
  }
  __atomic_store_n(&m->active, next, __ATOMIC_RELEASE);
}

static void cdl_update(oru_ue_t *ues, ue_model_t *m, double gain_db)
{
  const channel_path_t *paths;
  const int np = cdl_channel_paths(m->cdl, &paths);
  const antenna_array_t tx = {ues->nb_txru, (const double(*)[3])ues->txru_pos, ANTENNA_PATTERN_TR38901};
  const antenna_array_t rx = {ues->cfg.ue_nb_rx, (const double(*)[3])ues->ue_pos, ANTENNA_PATTERN_ISOTROPIC};
  // One beam per TXRU: the TXRU-domain response
  cd_t weights[ues->nb_txru * ues->nb_txru];
  memset(weights, 0, sizeof(weights));
  for (int t = 0; t < ues->nb_txru; t++)
    weights[t * ues->nb_txru + t] = (cd_t){1, 0};
  const size_t n = model_size(ues);
  cd_t *G = malloc(n * sizeof(*G));
  AssertFatal(G, "OOM\n");
  const double scs = 15e3 * (1 << ues->fp.numerology_index);
  beam_channel_response(paths, np, &tx, weights, ues->nb_txru, &rx, -ues->n_re / 2 * scs, scs, ues->n_re, G);
  if (m->scale == 0) {
    double power = 0;
    for (size_t i = 0; i < n; i++)
      power += G[i].r * G[i].r + G[i].i * G[i].i;
    m->scale = CHANNEL_ONE * pow(10, gain_db / 20) / sqrt(power / n);
  }
  const int next = !m->active;
  c16_t *h = m->h[next];
  for (size_t i = 0; i < n; i++)
    h[i] = (c16_t){sat16(lround(G[i].r * m->scale)), sat16(lround(G[i].i * m->scale))};
  free(G);
  __atomic_store_n(&m->active, next, __ATOMIC_RELEASE);
}

static double seconds_since(const struct timespec *t)
{
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (now.tv_sec - t->tv_sec) + (now.tv_nsec - t->tv_nsec) * 1e-9;
}

static const char *model_name(oru_ue_channel_model_type_t t)
{
  static const char *names[] = {"flat", "CDL-A", "CDL-B", "CDL-C", "CDL-D", "CDL-E"};
  return t <= ORU_UE_CHANNEL_MODEL_CDL_E ? names[t] : "?";
}

static void build_model(oru_ue_t *ues, int c)
{
  ue_model_t *m = &ues->models[c];
  const oru_ue_channel_model_t *p = &ues->params[c];
  struct drand48_data rng;
  srand48_r(p->seed, &rng);
  cdl_channel_free(m->cdl);
  m->cdl = NULL;
  m->scale = 0;
  if (p->type == ORU_UE_CHANNEL_MODEL_FLAT) {
    flat_model(ues, m, p, &rng);
    LOG_I(PHY, "[oru-ue] UE %d: flat channel model, %.1f dB\n", c, p->gain_db);
    return;
  }
  const double aod = (uniform(&rng) * 120 - 60) * M_PI / 180;
  const double zod = (90 + uniform(&rng) * 10 - 5) * M_PI / 180;
  const double heading = uniform(&rng) * 2 * M_PI;
  const double velocity[3] = {p->speed_mps * cos(heading), p->speed_mps * sin(heading), 0};
  long seed;
  lrand48_r(&rng, &seed);
  const cdl_channel_config_t cdl = {.model = (cdl_model_t)(p->type - ORU_UE_CHANNEL_MODEL_CDL_A),
                                    .delay_spread_s = p->delay_spread_ns * 1e-9,
                                    .carrier_hz = ues->fp.dl_CarrierFreq,
                                    .seed = seed};
  m->cdl = cdl_channel_create(&cdl);
  cdl_channel_set_geometry(m->cdl, aod, zod, velocity);
  cdl_update(ues, m, p->gain_db);
  clock_gettime(CLOCK_MONOTONIC, &m->updated);
  LOG_I(PHY,
        "[oru-ue] UE %d: %s channel model, DS %.0f ns, %.1f m/s, %.1f dB, AoD %.0f deg\n",
        c,
        model_name(p->type),
        p->delay_spread_ns,
        p->speed_mps,
        p->gain_db,
        aod * 180 / M_PI);
}

// Builds the UEs' channel models, then moves their CDL on; rests 1 ms per round.
static void *model_thread(void *arg)
{
  oru_ue_t *ues = arg;
  for (int c = 0; c < ues->cfg.num_ues && !ues->stop; c++) {
    build_model(ues, c);
    __atomic_store_n(&ues->models[c].built, true, __ATOMIC_RELEASE);
  }
  while (!ues->stop) {
    for (int c = 0; c < ues->cfg.num_ues && !ues->stop; c++) {
      ue_model_t *m = &ues->models[c];
      if (m->cdl) {
        cdl_channel_advance(m->cdl, seconds_since(&m->updated));
        clock_gettime(CLOCK_MONOTONIC, &m->updated);
        cdl_update(ues, m, ues->params[c].gain_db);
      }
    }
    usleep(1000);
  }
  return NULL;
}

oru_ue_t *oru_ue_create(const oru_ue_config_t *cfg, const NR_DL_FRAME_PARMS *fp, int nb_txru)
{
  AssertFatal(cfg->num_ues >= 1, "%d UEs\n", cfg->num_ues);
  AssertFatal(cfg->ue_nb_rx >= 1 && cfg->ue_nb_rx <= MAX_UE_ANT && cfg->ue_nb_tx >= 1 && cfg->ue_nb_tx <= cfg->ue_nb_rx,
              "UE antennas %d RX / %d TX: 1 to %d, TX among the RX ones\n",
              cfg->ue_nb_rx,
              cfg->ue_nb_tx,
              MAX_UE_ANT);
  AssertFatal(fp->N_RB_DL == fp->N_RB_UL, "O-RU with UEs: TDD carrier, %d DL / %d UL RBs\n", fp->N_RB_DL, fp->N_RB_UL);
  const int txru_cols = cfg->txru_cols ? cfg->txru_cols : nb_txru / cfg->txru_rows;
  AssertFatal(cfg->txru_rows >= 1 && cfg->txru_rows * txru_cols == nb_txru,
              "TXRU array %d x %d for %d TXRUs\n",
              cfg->txru_rows,
              txru_cols,
              nb_txru);
  for (int c = 0; c < cfg->num_ues; c++)
    AssertFatal(cfg->models[c].type <= ORU_UE_CHANNEL_MODEL_CDL_E, "UE %d channel model %d\n", c, cfg->models[c].type);
  oru_ue_t *ues = calloc(1, sizeof(*ues));
  AssertFatal(ues, "OOM\n");
  ues->cfg = *cfg;
  ues->fp = *fp;
  ues->nb_txru = nb_txru;
  ues->n_re = fp->N_RB_DL * NR_NB_SC_PER_RB;
  pthread_mutex_init(&ues->publish_lock, NULL);
  ues->params = malloc(cfg->num_ues * sizeof(*ues->params));
  ues->models = calloc(cfg->num_ues, sizeof(*ues->models));
  AssertFatal(ues->params && ues->models, "OOM\n");
  memcpy(ues->params, cfg->models, cfg->num_ues * sizeof(*ues->params));
  ues->cfg.models = ues->params;
  for (int c = 0; c < cfg->num_ues; c++)
    for (int b = 0; b < 2; b++)
      ues->models[c].h[b] = malloc16_clear_or_fail(model_size(ues) * sizeof(c16_t));
  ues->txru_pos = calloc(nb_txru, sizeof(*ues->txru_pos));
  ues->ue_pos = calloc(cfg->ue_nb_rx, sizeof(*ues->ue_pos));
  AssertFatal(ues->txru_pos && ues->ue_pos, "OOM\n");
  antenna_array_upa_positions(cfg->txru_rows, txru_cols, cfg->txru_dv, cfg->txru_dh, ues->txru_pos);
  antenna_array_upa_positions(1, cfg->ue_nb_rx, 0.5, 0.5, ues->ue_pos);
  LOG_I(PHY, "[oru-ue] %d UEs x %d RX / %d TX, TXRU array %d x %d\n", cfg->num_ues, cfg->ue_nb_rx, cfg->ue_nb_tx, cfg->txru_rows, txru_cols);
  threadCreate(&ues->model_thread, model_thread, ues, "oru_ue_model", -1, OAI_PRIORITY_RT_LOW);
  return ues;
}

bool oru_ue_channel_ready(oru_ue_t *ues, int ue)
{
  return ready_model(ues, ue) != NULL;
}

void oru_ue_start(oru_ue_t *ues, uint64_t first_symbol)
{
  const NR_DL_FRAME_PARMS *fp = &ues->fp;
  const ShmFDIQGeometry geo = {.n_re_dl = ues->n_re,
                               .n_re_ul = ues->n_re,
                               .nb_ant_dl = ues->cfg.num_ues * ues->cfg.ue_nb_rx,
                               .nb_ant_ul = ues->cfg.num_ues * ues->cfg.ue_nb_tx,
                               .ring_symbols = FD_CHANNEL_RING_SLOTS * fp->symbols_per_slot,
                               .nb_clients = ues->cfg.num_ues,
                               .nb_msg = 2 * ues->cfg.num_ues,
                               .msg_max_size = sizeof(fd_channel_prach_t)};
  ues->channel = shm_fd_iq_channel_create(ues->cfg.channel_name, &geo, first_symbol);
  ues->published = first_symbol;
  __atomic_store_n(&ues->started, true, __ATOMIC_RELEASE);
  const uint64_t spf = (uint64_t)fp->slots_per_frame * fp->symbols_per_slot;
  LOG_I(PHY, "[oru-ue] FD channel %s started at frame %lu, symbol %lu\n", ues->cfg.channel_name, first_symbol / spf, first_symbol % spf);
}

void oru_ue_dl_symbol(oru_ue_t *ues, uint64_t symbol, const c16_t *const *re)
{
  if (!__atomic_load_n(&ues->started, __ATOMIC_ACQUIRE))
    return;
  const int n_re = ues->n_re; // a multiple of 4, buffers 16-byte aligned: SIMD helpers apply
  const int nb_rx = ues->cfg.ue_nb_rx;
  c16_t y[nb_rx][n_re] __attribute__((aligned(32)));
  const c16_t *yp[nb_rx];
  for (int r = 0; r < nb_rx; r++)
    yp[r] = y[r];
  for (int c = 0; c < ues->cfg.num_ues; c++) {
    const c16_t *h = ready_model(ues, c);
    if (!h)
      continue;
    for (int r = 0; r < nb_rx; r++) {
      const c16_t *hr = h + (size_t)r * ues->nb_txru * n_re;
      mult_cpx_vector(re[0], hr, y[r], n_re, CHANNEL_SHIFT);
      for (int t = 1; t < ues->nb_txru; t++)
        multadd_cpx_vector(re[t], hr + (size_t)t * n_re, y[r], n_re, CHANNEL_SHIFT);
    }
    const IQChannelErrorType ret = shm_fd_iq_channel_tx(ues->channel, symbol, c * nb_rx, nb_rx, yp);
    if (ret != CHANNEL_NO_ERROR && !ues->dl_late_warned) {
      ues->dl_late_warned = true;
      LOG_W(PHY,
            "[oru-ue] DL symbol %lu %s (published up to %lu): dropped (warned once)\n",
            symbol,
            ret == CHANNEL_ERROR_TOO_LATE ? "after its air time" : "too far ahead",
            __atomic_load_n(&ues->published, __ATOMIC_RELAXED));
    }
  }
}

void oru_ue_dl_publish(oru_ue_t *ues, uint64_t end)
{
  pthread_mutex_lock(&ues->publish_lock);
  if (end > ues->published) {
    shm_fd_iq_channel_produce(ues->channel, end - ues->published);
    __atomic_store_n(&ues->published, end, __ATOMIC_RELEASE);
  }
  pthread_mutex_unlock(&ues->publish_lock);
}

// Newest published symbol at (frame % 1024, slot, symbol)
static uint64_t published_symbol(oru_ue_t *ues, int frame, int slot, int symbol)
{
  const uint64_t sps = ues->fp.symbols_per_slot;
  const uint64_t period = 1024 * ues->fp.slots_per_frame * sps;
  const uint64_t wanted = ((uint64_t)frame * ues->fp.slots_per_frame + slot) * sps + symbol;
  const uint64_t now = __atomic_load_n(&ues->published, __ATOMIC_ACQUIRE);
  const uint64_t s = now - now % period + wanted;
  return s < now ? s : s - period;
}

bool oru_ue_ul_symbol(oru_ue_t *ues, int frame, int slot, int symbol, c16_t *const *ant)
{
  if (!__atomic_load_n(&ues->started, __ATOMIC_ACQUIRE))
    return false;
  const uint64_t abs_symbol = published_symbol(ues, frame, slot, symbol);
  const int n_re = ues->n_re;
  const int nb_tx = ues->cfg.ue_nb_tx;
  c16_t z[ues->nb_txru][n_re] __attribute__((aligned(32)));
  memset(z, 0, sizeof(z));
  c16_t u[nb_tx][n_re] __attribute__((aligned(32)));
  c16_t *up[nb_tx];
  for (int t = 0; t < nb_tx; t++)
    up[t] = u[t];
  for (int c = 0; c < ues->cfg.num_ues; c++) {
    const c16_t *h = ready_model(ues, c);
    if (!h || shm_fd_iq_channel_rx(ues->channel, abs_symbol, c * nb_tx, nb_tx, up) != CHANNEL_NO_ERROR)
      continue;
    // Reciprocal: y_a = sum_t H[t][a] x_t
    for (int a = 0; a < ues->nb_txru; a++) {
      if (!ant[a])
        continue;
      for (int t = 0; t < nb_tx; t++)
        multadd_cpx_vector(u[t], h + ((size_t)t * ues->nb_txru + a) * n_re, z[a], n_re, CHANNEL_SHIFT);
    }
  }
  for (int a = 0; a < ues->nb_txru; a++) {
    if (!ant[a])
      continue;
    memset(ant[a], 0, ues->fp.ofdm_symbol_size * sizeof(c16_t));
    fftshift_inverse(z[a], ant[a], n_re, ues->fp.ofdm_symbol_size);
  }
  return true;
}

void oru_ue_prach(oru_ue_t *ues, int frame, int slot, int k, int dftlen, int n_zc, int nb_ant, c16_t (*rxsigF)[NR_PRACH_SEQ_LEN_L])
{
  for (int a = 0; a < nb_ant; a++)
    memset(rxsigF[a], 0, n_zc * sizeof(c16_t));
  if (!__atomic_load_n(&ues->started, __ATOMIC_ACQUIRE))
    return;
  const uint64_t abs_symbol = published_symbol(ues, frame, slot, 0);
  const int n_re = ues->n_re;
  const int nb_msg = shm_fd_iq_channel_geometry(ues->channel)->nb_msg;
  for (int e = 0; e < nb_msg; e++) {
    fd_channel_prach_t p;
    size_t len;
    if (shm_fd_iq_channel_rx_msg(ues->channel, abs_symbol, e, &p, sizeof(p), &len) != CHANNEL_NO_ERROR)
      continue;
    if (p.ue < 0 || p.ue >= ues->cfg.num_ues || p.n_zc > FD_CHANNEL_PRACH_MAX_BINS)
      continue;
    const c16_t *h = ready_model(ues, p.ue);
    if (!h)
      continue;
    if (p.dftlen != dftlen || p.k != k || p.n_zc != n_zc) {
      if (!ues->prach_warned)
        LOG_W(PHY,
              "[oru-ue] UE %d PRACH at bin %d of %d (%d long), the O-RU expects bin %d of %d (%d long): dropped (warned once)\n",
              p.ue,
              p.k,
              p.dftlen,
              p.n_zc,
              k,
              dftlen,
              n_zc);
      ues->prach_warned = true;
      continue;
    }
    for (int j = 0; j < n_zc; j++) {
      // The bin's frequency on the RE grid: dftlen bins and fft_size REs span the same sample rate
      const int b = (k + j) % dftlen;
      const int s = b < dftlen / 2 ? b : b - dftlen;
      int re = n_re / 2 + (int)lround((double)s * ues->fp.ofdm_symbol_size / dftlen);
      re = re < 0 ? 0 : re >= n_re ? n_re - 1 : re;
      for (int a = 0; a < nb_ant && a < ues->nb_txru; a++) {
        const c16_t hv = h[(size_t)a * n_re + re]; // UE antenna 0 to O-RU antenna a
        const int32_t r = ((int32_t)hv.r * p.bins[j].r - (int32_t)hv.i * p.bins[j].i) >> CHANNEL_SHIFT;
        const int32_t i = ((int32_t)hv.r * p.bins[j].i + (int32_t)hv.i * p.bins[j].r) >> CHANNEL_SHIFT;
        rxsigF[a][j] = (c16_t){sat16(rxsigF[a][j].r + r), sat16(rxsigF[a][j].i + i)};
      }
    }
  }
}

void oru_ue_destroy(oru_ue_t *ues)
{
  if (!ues)
    return;
  ues->stop = 1;
  pthread_join(ues->model_thread, NULL);
  shm_fd_iq_channel_destroy(ues->channel);
  for (int c = 0; c < ues->cfg.num_ues; c++) {
    free(ues->models[c].h[0]);
    free(ues->models[c].h[1]);
    cdl_channel_free(ues->models[c].cdl);
  }
  free(ues->models);
  free(ues->params);
  free(ues->txru_pos);
  free(ues->ue_pos);
  pthread_mutex_destroy(&ues->publish_lock);
  free(ues);
}
