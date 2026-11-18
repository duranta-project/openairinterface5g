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
#include "executables/ue_low_phy/fd_host.h"
#include "openair1/SIMULATION/TOOLS/cdl_channel.h"
#include "PHY/MODULATION/nr_modulation.h"
#include "PHY/TOOLS/tools_defs.h"
#include "common/utils/LOG/log.h"
#include "common/utils/assertions.h"
#include "common/utils/system.h"

// Channel coefficients in Q12: 18 dB of headroom above unit gain for fading peaks.
#define CHANNEL_SHIFT 12
#define CHANNEL_ONE (1 << CHANNEL_SHIFT)
// UL slots taken and still read by the composers; a slot's entries are released this many slots later.
#define UL_SLOTS 8
#define UL_RELEASE_LAG 4

typedef struct {
  c16_t *h[2]; ///< [ue_nb_rx][nb_txru][n_re], Q12; the active one is read, the other rewritten
  int active;
  cdl_channel_t *cdl;
  double scale; ///< CDL: from response to Q12 at unit average power, fixed at the first computation
  struct timespec updated;
} ue_channel_t;

typedef struct {
  uint64_t slot; ///< 3GPP slot + 1, 0 when free
  ue_low_phy_fd_ul_slot_t ul[UE_LOW_PHY_MAX_CLIENTS];
} ul_slot_t;

struct oru_ue {
  oru_ue_config_t cfg;
  NR_DL_FRAME_PARMS fp;
  int nb_txru;
  int n_re;
  ue_low_phy_fd_host_t *host;
  ue_low_phy_port_t *port;
  ue_channel_t *channels; ///< one per client slot
  double (*txru_pos)[3];
  double (*ue_pos)[3];
  // DL
  uint64_t *written; ///< [rx_ring_symbols]: symbol + 1 once oru_ue_dl_symbol() wrote it
  pthread_mutex_t publish_lock;
  uint64_t published; ///< read without the lock by oru_ue_dl_symbol(): late writes are skipped
  // UL
  ul_slot_t ul_slots[UL_SLOTS];
  uint64_t next_ul_slot;
  bool ul_started;
  bool prach_dftlen_warned;
  // Channel updates
  pthread_t channel_thread;
  volatile int stop;
};

static uint64_t splitmix64(uint64_t *state)
{
  uint64_t z = (*state += 0x9e3779b97f4a7c15ULL);
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
  return z ^ (z >> 31);
}

static double uniform(uint64_t *state)
{
  return (splitmix64(state) >> 11) * 0x1.0p-53;
}

static int16_t sat16(int32_t v)
{
  return v > INT16_MAX ? INT16_MAX : v < INT16_MIN ? INT16_MIN : v;
}

static size_t channel_size(const oru_ue_t *ues)
{
  return (size_t)ues->cfg.ue_nb_rx * ues->nb_txru * ues->n_re;
}

static c16_t *active_channel(ue_channel_t *ch)
{
  return ch->h[__atomic_load_n(&ch->active, __ATOMIC_ACQUIRE)];
}

static void flat_channel(oru_ue_t *ues, int c)
{
  ue_channel_t *ch = &ues->channels[c];
  uint64_t rng = ues->cfg.seed * 1000003 + c;
  const double amplitude = CHANNEL_ONE * pow(10, ues->cfg.gain_db / 20);
  c16_t *h = ch->h[0];
  for (int r = 0; r < ues->cfg.ue_nb_rx; r++) {
    for (int t = 0; t < ues->nb_txru; t++) {
      const double phase = 2 * M_PI * uniform(&rng);
      const c16_t v = r % ues->nb_txru == t ? (c16_t){sat16(lround(amplitude * cos(phase))), sat16(lround(amplitude * sin(phase)))}
                                            : (c16_t){0, 0};
      c16_t *row = h + ((size_t)r * ues->nb_txru + t) * ues->n_re;
      for (int k = 0; k < ues->n_re; k++)
        row[k] = v;
    }
  }
  memcpy(ch->h[1], ch->h[0], channel_size(ues) * sizeof(c16_t));
}

// Recomputes client c's CDL response into its inactive buffer, then makes it active.
static void cdl_update(oru_ue_t *ues, int c)
{
  ue_channel_t *ch = &ues->channels[c];
  const channel_path_t *paths;
  const int np = cdl_channel_paths(ch->cdl, &paths);
  const antenna_array_t tx = {ues->nb_txru, (const double(*)[3])ues->txru_pos, ANTENNA_PATTERN_TR38901};
  const antenna_array_t rx = {ues->cfg.ue_nb_rx, (const double(*)[3])ues->ue_pos, ANTENNA_PATTERN_ISOTROPIC};
  // One beam per TXRU: the TXRU-domain response
  cd_t weights[ues->nb_txru * ues->nb_txru];
  memset(weights, 0, sizeof(weights));
  for (int t = 0; t < ues->nb_txru; t++)
    weights[t * ues->nb_txru + t] = (cd_t){1, 0};
  const size_t n = channel_size(ues);
  cd_t *G = malloc(n * sizeof(*G));
  AssertFatal(G, "OOM\n");
  const double scs = 15e3 * (1 << ues->fp.numerology_index);
  beam_channel_response(paths, np, &tx, weights, ues->nb_txru, &rx, -ues->n_re / 2 * scs, scs, ues->n_re, G);
  if (ch->scale == 0) {
    double power = 0;
    for (size_t i = 0; i < n; i++)
      power += G[i].r * G[i].r + G[i].i * G[i].i;
    ch->scale = CHANNEL_ONE * pow(10, ues->cfg.gain_db / 20) / sqrt(power / n);
  }
  const int next = !ch->active;
  c16_t *h = ch->h[next];
  for (size_t i = 0; i < n; i++)
    h[i] = (c16_t){sat16(lround(G[i].r * ch->scale)), sat16(lround(G[i].i * ch->scale))};
  free(G);
  __atomic_store_n(&ch->active, next, __ATOMIC_RELEASE);
}

static double seconds_since(const struct timespec *t)
{
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (now.tv_sec - t->tv_sec) + (now.tv_nsec - t->tv_nsec) * 1e-9;
}

// Moves every open client's CDL channel on by the time since its last update, one client at a
// time: the channel evolves at the rate the host can afford, never holding up the data path.
static void *channel_thread(void *arg)
{
  oru_ue_t *ues = arg;
  while (!ues->stop) {
    bool any = false;
    for (int c = 0; c < ues->cfg.max_ues && !ues->stop; c++) {
      if (!ue_low_phy_fd_host_client_open(ues->host, c))
        continue;
      ue_channel_t *ch = &ues->channels[c];
      cdl_channel_advance(ch->cdl, seconds_since(&ch->updated));
      clock_gettime(CLOCK_MONOTONIC, &ch->updated);
      cdl_update(ues, c);
      any = true;
    }
    if (!any)
      usleep(10000);
  }
  return NULL;
}

oru_ue_t *oru_ue_create(const oru_ue_config_t *cfg, const NR_DL_FRAME_PARMS *fp, int nb_txru)
{
  AssertFatal(cfg->max_ues >= 1 && cfg->max_ues <= UE_LOW_PHY_MAX_CLIENTS,
              "%d UEs, 1 to %d\n",
              cfg->max_ues,
              UE_LOW_PHY_MAX_CLIENTS);
  AssertFatal(cfg->ue_nb_rx >= 1 && cfg->ue_nb_rx <= UE_LOW_PHY_MAX_ANT && cfg->ue_nb_tx >= 1 && cfg->ue_nb_tx <= cfg->ue_nb_rx,
              "UE antennas %d RX / %d TX: 1 to %d, TX among the RX ones\n",
              cfg->ue_nb_rx,
              cfg->ue_nb_tx,
              UE_LOW_PHY_MAX_ANT);
  AssertFatal(fp->N_RB_DL == fp->N_RB_UL, "O-RU with UEs: TDD carrier, %d DL / %d UL RBs\n", fp->N_RB_DL, fp->N_RB_UL);
  oru_ue_t *ues = calloc(1, sizeof(*ues));
  AssertFatal(ues, "OOM\n");
  ues->cfg = *cfg;
  ues->fp = *fp;
  ues->nb_txru = nb_txru;
  ues->n_re = fp->N_RB_DL * NR_NB_SC_PER_RB;

  const ue_low_phy_geometry_t geo = {.fft_size = fp->ofdm_symbol_size,
                                     .n_rb_dl = fp->N_RB_DL,
                                     .n_rb_ul = fp->N_RB_UL,
                                     .nb_rx = cfg->ue_nb_rx,
                                     .nb_tx = cfg->ue_nb_tx,
                                     .symbols_per_slot = fp->symbols_per_slot,
                                     .slots_per_frame = fp->slots_per_frame,
                                     .ring_symbols = 2 * fp->slots_per_frame * fp->symbols_per_slot,
                                     .prach_max_samples = 49152,
                                     .max_clients = cfg->max_ues,
                                     .dl_per_client = 1,
                                     .rx_ring_symbols = 10 * fp->symbols_per_slot};
  ues->host = ue_low_phy_fd_host_create(cfg->port_name, &geo);
  ues->port = ue_low_phy_fd_host_port(ues->host);
  ues->written = calloc(geo.rx_ring_symbols, sizeof(*ues->written));
  AssertFatal(ues->written, "OOM\n");
  pthread_mutex_init(&ues->publish_lock, NULL);

  ues->channels = calloc(cfg->max_ues, sizeof(*ues->channels));
  AssertFatal(ues->channels, "OOM\n");
  for (int c = 0; c < cfg->max_ues; c++)
    for (int b = 0; b < 2; b++)
      ues->channels[c].h[b] = malloc16_clear_or_fail(channel_size(ues) * sizeof(c16_t));

  if (cfg->channel == ORU_UE_CHANNEL_FLAT) {
    for (int c = 0; c < cfg->max_ues; c++)
      flat_channel(ues, c);
    LOG_I(PHY,
          "[oru-ue] flat channel, %.1f dB, %d UEs x %d RX / %d TX, %d TXRU\n",
          cfg->gain_db,
          cfg->max_ues,
          cfg->ue_nb_rx,
          cfg->ue_nb_tx,
          nb_txru);
  } else {
    AssertFatal(cfg->txru_rows * cfg->txru_cols == nb_txru,
                "TXRU array %d x %d for %d TXRUs\n",
                cfg->txru_rows,
                cfg->txru_cols,
                nb_txru);
    ues->txru_pos = calloc(nb_txru, sizeof(*ues->txru_pos));
    ues->ue_pos = calloc(cfg->ue_nb_rx, sizeof(*ues->ue_pos));
    AssertFatal(ues->txru_pos && ues->ue_pos, "OOM\n");
    antenna_array_upa_positions(cfg->txru_rows, cfg->txru_cols, cfg->txru_dv, cfg->txru_dh, ues->txru_pos);
    antenna_array_upa_positions(1, cfg->ue_nb_rx, 0.5, 0.5, ues->ue_pos);
    for (int c = 0; c < cfg->max_ues; c++) {
      // Each client slot's UE somewhere in a 120 degree sector, moving horizontally
      uint64_t rng = cfg->seed * 1000003 + c;
      const double aod = (uniform(&rng) * 120 - 60) * M_PI / 180;
      const double zod = (90 + uniform(&rng) * 10 - 5) * M_PI / 180;
      const double heading = uniform(&rng) * 2 * M_PI;
      const double velocity[3] = {cfg->ue_speed_mps * cos(heading), cfg->ue_speed_mps * sin(heading), 0};
      const cdl_channel_config_t cdl = {.model = cfg->cdl_model,
                                        .delay_spread_s = cfg->delay_spread_ns * 1e-9,
                                        .carrier_hz = fp->dl_CarrierFreq,
                                        .seed = splitmix64(&rng)};
      ue_channel_t *ch = &ues->channels[c];
      ch->cdl = cdl_channel_create(&cdl);
      cdl_channel_set_geometry(ch->cdl, aod, zod, velocity);
      cdl_update(ues, c);
      memcpy(ch->h[!ch->active], ch->h[ch->active], channel_size(ues) * sizeof(c16_t));
      clock_gettime(CLOCK_MONOTONIC, &ch->updated);
    }
    LOG_I(PHY,
          "[oru-ue] CDL-%c channel, DS %.0f ns, %.1f m/s, %.1f dB, %d UEs x %d RX / %d TX, TXRU array %d x %d\n",
          'A' + cfg->cdl_model,
          cfg->delay_spread_ns,
          cfg->ue_speed_mps,
          cfg->gain_db,
          cfg->max_ues,
          cfg->ue_nb_rx,
          cfg->ue_nb_tx,
          cfg->txru_rows,
          cfg->txru_cols);
    threadCreate(&ues->channel_thread, channel_thread, ues, "oru_ue_channel", -1, OAI_PRIORITY_RT_LOW);
  }
  return ues;
}

void oru_ue_start(oru_ue_t *ues, uint64_t first_symbol)
{
  ue_low_phy_port_set_time(ues->port, 0);
  pthread_mutex_lock(&ues->publish_lock);
  __atomic_store_n(&ues->published, first_symbol, __ATOMIC_RELEASE);
  ue_low_phy_fd_host_publish(ues->host, first_symbol);
  pthread_mutex_unlock(&ues->publish_lock);
  const ue_low_phy_3gpp_time_t t = ue_low_phy_3gpp_time(&ues->port->mem->geo, first_symbol);
  LOG_I(PHY, "[oru-ue] UE port %s started at %lu.%d.%d.%d\n", ues->cfg.port_name, t.hfn, t.frame, t.slot, t.symbol);
}

void oru_ue_dl_symbol(oru_ue_t *ues, uint64_t symbol, const c16_t *const *re)
{
  if (symbol < __atomic_load_n(&ues->published, __ATOMIC_ACQUIRE))
    return; // published silent already: the FH was too late for its air time
  const int n_re = ues->n_re; // a multiple of 4, every buffer 16-byte aligned: the SIMD helpers apply
  for (int c = 0; c < ues->cfg.max_ues; c++) {
    if (!ue_low_phy_fd_host_client_open(ues->host, c))
      continue;
    const c16_t *h = active_channel(&ues->channels[c]);
    for (int r = 0; r < ues->cfg.ue_nb_rx; r++) {
      c16_t *y = ue_low_phy_fd_host_dl_symbol(ues->host, c, r, symbol);
      const c16_t *hr = h + (size_t)r * ues->nb_txru * n_re;
      mult_cpx_vector(re[0], hr, y, n_re, CHANNEL_SHIFT);
      for (int t = 1; t < ues->nb_txru; t++)
        multadd_cpx_vector(re[t], hr + (size_t)t * n_re, y, n_re, CHANNEL_SHIFT);
    }
  }
  __atomic_store_n(&ues->written[symbol % ues->port->mem->rx_ring_symbols], symbol + 1, __ATOMIC_RELEASE);
}

void oru_ue_dl_publish(oru_ue_t *ues, uint64_t end)
{
  const uint32_t ring = ues->port->mem->rx_ring_symbols;
  pthread_mutex_lock(&ues->publish_lock);
  for (uint64_t s = ues->published; s < end; s++) {
    if (__atomic_load_n(&ues->written[s % ring], __ATOMIC_ACQUIRE) != s + 1) {
      for (int c = 0; c < ues->cfg.max_ues; c++) {
        if (!ue_low_phy_fd_host_client_open(ues->host, c))
          continue;
        for (int r = 0; r < ues->cfg.ue_nb_rx; r++)
          memset(ue_low_phy_fd_host_dl_symbol(ues->host, c, r, s), 0, ues->n_re * sizeof(c16_t));
      }
    }
    ue_low_phy_fd_host_publish(ues->host, s + 1);
  }
  if (end > ues->published)
    __atomic_store_n(&ues->published, end, __ATOMIC_RELEASE);
  pthread_mutex_unlock(&ues->publish_lock);
}

static ul_slot_t *find_ul_slot(oru_ue_t *ues, int frame, int slot)
{
  const uint64_t period = (uint64_t)1024 * ues->fp.slots_per_frame;
  const uint64_t wanted = (uint64_t)frame * ues->fp.slots_per_frame + slot;
  for (int i = 0; i < UL_SLOTS; i++) {
    const uint64_t s = __atomic_load_n(&ues->ul_slots[i].slot, __ATOMIC_ACQUIRE);
    if (s && (s - 1) % period == wanted)
      return &ues->ul_slots[i];
  }
  return NULL;
}

void oru_ue_ul_take_slot(oru_ue_t *ues, uint64_t slot)
{
  ue_low_phy_fd_host_update(ues->host);
  ul_slot_t *e = &ues->ul_slots[slot % UL_SLOTS];
  __atomic_store_n(&e->slot, 0, __ATOMIC_RELEASE);
  for (int c = 0; c < ues->cfg.max_ues; c++)
    ue_low_phy_fd_host_take_slot(ues->host, c, slot, true, &e->ul[c]);
  __atomic_store_n(&e->slot, slot + 1, __ATOMIC_RELEASE);
  if (!ues->ul_started) {
    ues->ul_started = true;
    ues->next_ul_slot = slot;
  }
  // Composers read a slot's entries until a few slots later: release then
  while (ues->next_ul_slot + UL_RELEASE_LAG <= slot)
    ue_low_phy_fd_host_finish_slot(ues->host, ues->next_ul_slot++);
}

bool oru_ue_ul_symbol(oru_ue_t *ues, int frame, int slot, int symbol, c16_t *const *ant)
{
  const ul_slot_t *e = find_ul_slot(ues, frame, slot);
  if (!e)
    return false;
  const uint64_t abs_symbol = (e->slot - 1) * ues->fp.symbols_per_slot + symbol;
  const int n_re = ues->n_re;
  c16_t z[n_re] __attribute__((aligned(32)));
  for (int a = 0; a < ues->nb_txru; a++) {
    if (!ant[a])
      continue;
    memset(z, 0, sizeof(z));
    for (int c = 0; c < ues->cfg.max_ues; c++) {
      const ue_low_phy_fd_ul_slot_t *ul = &e->ul[c];
      if (!ul->member || !(ul->used_symbols & (1u << symbol)))
        continue;
      const c16_t *h = active_channel(&ues->channels[c]);
      for (int t = 0; t < ues->cfg.ue_nb_tx; t++)
        multadd_cpx_vector(ue_low_phy_fd_host_ul_symbol(ues->host, c, t, abs_symbol),
                           h + ((size_t)t * ues->nb_txru + a) * n_re,
                           z,
                           n_re,
                           CHANNEL_SHIFT);
    }
    memset(ant[a], 0, ues->fp.ofdm_symbol_size * sizeof(c16_t));
    fftshift_inverse(z, ant[a], n_re, ues->fp.ofdm_symbol_size);
  }
  return true;
}

void oru_ue_prach(oru_ue_t *ues, int frame, int slot, int k, int dftlen, int n_zc, int nb_ant, c16_t (*rxsigF)[NR_PRACH_SEQ_LEN_L])
{
  for (int a = 0; a < nb_ant; a++)
    memset(rxsigF[a], 0, n_zc * sizeof(c16_t));
  const ul_slot_t *e = find_ul_slot(ues, frame, slot);
  if (!e)
    return;
  const int n_re = ues->n_re;
  for (int c = 0; c < ues->cfg.max_ues; c++) {
    const ue_low_phy_entry_hdr_t *p = e->ul[c].prach;
    if (!p)
      continue;
    if (p->dftlen != dftlen) {
      if (!ues->prach_dftlen_warned)
        LOG_W(PHY, "[oru-ue] UE PRACH of %d bins, the O-RU expects %d: configurations differ, PRACH dropped\n", p->dftlen, dftlen);
      ues->prach_dftlen_warned = true;
      continue;
    }
    const c16_t *bins = (const c16_t *)(p + 1);
    const c16_t *h = active_channel(&ues->channels[c]);
    for (int j = 0; j < n_zc; j++) {
      const int b = (k + j) % dftlen;
      // The bin's frequency on the RE grid: dftlen bins and fft_size REs span the same sample rate
      const int s = b < dftlen / 2 ? b : b - dftlen;
      int re = n_re / 2 + (int)lround((double)s * ues->fp.ofdm_symbol_size / dftlen);
      re = re < 0 ? 0 : re >= n_re ? n_re - 1 : re;
      for (int a = 0; a < nb_ant && a < ues->nb_txru; a++) {
        const c16_t hv = h[(size_t)a * n_re + re]; // UE antenna 0 to O-RU antenna a
        const int32_t r = ((int32_t)hv.r * bins[b].r - (int32_t)hv.i * bins[b].i) >> CHANNEL_SHIFT;
        const int32_t i = ((int32_t)hv.r * bins[b].i + (int32_t)hv.i * bins[b].r) >> CHANNEL_SHIFT;
        rxsigF[a][j] = (c16_t){sat16(rxsigF[a][j].r + r), sat16(rxsigF[a][j].i + i)};
      }
    }
  }
}

void oru_ue_stop(oru_ue_t *ues)
{
  ues->stop = 1;
  ue_low_phy_fd_host_stop(ues->host);
}

void oru_ue_destroy(oru_ue_t *ues)
{
  if (!ues)
    return;
  if (ues->cfg.channel == ORU_UE_CHANNEL_CDL) {
    ues->stop = 1;
    pthread_join(ues->channel_thread, NULL);
  }
  for (int c = 0; c < ues->cfg.max_ues; c++) {
    free(ues->channels[c].h[0]);
    free(ues->channels[c].h[1]);
    cdl_channel_free(ues->channels[c].cdl);
  }
  free(ues->channels);
  free(ues->txru_pos);
  free(ues->ue_pos);
  free(ues->written);
  pthread_mutex_destroy(&ues->publish_lock);
  ue_low_phy_fd_host_destroy(ues->host);
  free(ues);
}
