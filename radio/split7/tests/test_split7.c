/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * Split 7.1 Low-PHY tests: OFDM core against the monolithic UE paths, and the
 * engine against a fake radio with a test-driven sample clock: UL combining of a
 * co-located group at its leader's Timing Advance, deterministic finalization (late member,
 * random interleaving), TA-increase clipping, PRACH, join/leave, backpressure, DL
 * publishing and timing steering.
 */

#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <unistd.h>

#include "split7_engine.h"
#include "split7_engine_internal.h"
#include "split7_ofdm.h"
#include "PHY/defs_nr_UE.h"
#include "PHY/INIT/nr_parms.h"
#include "PHY/MODULATION/modulation_UE.h"
#include "PHY/MODULATION/nr_modulation.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_proto_ue.h"
#include "common/config/config_userapi.h"
#include "common/utils/LOG/log.h"
#include "executables/nr-uesoftmodem.h"
#include "executables/softmodem-common.h"

/* Stubs for what the PHY libraries reference but these tests never reach. */
softmodem_params_t *get_softmodem_params(void)
{
  static softmodem_params_t p;
  return &p;
}
nrUE_params_t nrUE_params;
nrUE_params_t *get_nrUE_params(void)
{
  return &nrUE_params;
}
configmodule_interface_t *uniqCfg = NULL;
void exit_function(const char *file, const char *function, const int line, const char *s, const int assert)
{
  fprintf(stderr, "exit_function %s:%d %s: %s\n", file, line, function, s);
  abort();
}
openair0_config_t openair0_cfg_g[MAX_CARDS];
int openair0_device_load(openair0_device_t *device, openair0_config_t *openair0_cfg)
{
  return -1;
}

static int failures;
#define CHECK(cond, ...)                                   \
  do {                                                     \
    if (!(cond)) {                                         \
      fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
      fprintf(stderr, __VA_ARGS__);                        \
      fprintf(stderr, "\n");                               \
      failures++;                                          \
    }                                                      \
  } while (0)

static NR_DL_FRAME_PARMS fp;

static void init_fp(void)
{
  const nrUE_cell_params_t cell = {.band = 78, .rf_frequency = 3319680000ULL, .numerology = 1, .N_RB_DL = 106, .ssb_start = 516};
  nr_init_frame_parms_ue_sa(&fp, &cell);
  fp.nb_antennas_rx = 1;
  fp.nb_antennas_tx = 1;
  fp.ofdm_offset_divisor = 8;
  init_symbol_rotation(&fp);
  init_timeshift_rotation(fp.ofdm_symbol_size,
                          fp.N_RB_DL * NR_NB_SC_PER_RB,
                          fp.nb_prefix_samples,
                          fp.ofdm_offset_divisor,
                          fp.timeshift_symbol_rotation);
}

static void fill_random(c16_t *x, int n, unsigned *seed)
{
  for (int i = 0; i < n; i++)
    x[i] = (c16_t){.r = (rand_r(seed) % 4001) - 2000, .i = (rand_r(seed) % 4001) - 2000};
}

/* ------------------------------------------------------------------------- */
/* OFDM core vs monolithic                                                   */
/* ------------------------------------------------------------------------- */

static void test_mod_matches_monolithic(void)
{
  const int fft = fp.ofdm_symbol_size;
  const int n_re = fp.N_RB_UL * 12;
  unsigned seed = 1;
  for (int slot = 0; slot < 3; slot++) {
    c16_t *txdataF = malloc16_clear(fp.symbols_per_slot * fft * sizeof(c16_t));
    c16_t *ref = malloc16_clear(fp.samples_per_subframe * sizeof(c16_t));
    c16_t *orig = malloc16_clear(fp.symbols_per_slot * fft * sizeof(c16_t));
    for (int s = 0; s < fp.symbols_per_slot; s++)
      fill_random(orig + s * fft, n_re, &seed);
    memcpy(txdataF, orig, fp.symbols_per_slot * fft * sizeof(c16_t));
    bool used[NR_SYMBOLS_PER_SLOT];
    for (int s = 0; s < NR_SYMBOLS_PER_SLOT; s++)
      used[s] = true;
    c16_t *txf[1] = {txdataF}, *txd[1] = {ref};
    nr_tx_rotation_and_ofdm_mod(slot, &fp, 1, txf, txd, link_type_ul, used, false);

    c16_t scratch[fft] __attribute__((aligned(64)));
    c16_t out[fft + fp.nb_prefix_samples0];
    int off = 0;
    for (int s = 0; s < fp.symbols_per_slot; s++) {
      const int len = split7_symbol_samples(&fp, slot, s);
      split7_ofdm_mod(&fp, link_type_ul, fp.N_RB_UL, slot, s, orig + s * fft, scratch, out);
      CHECK(memcmp(out, ref + off, len * sizeof(c16_t)) == 0,
            "mod slot %d symbol %d differs from nr_tx_rotation_and_ofdm_mod",
            slot,
            s);
      off += len;
    }
    free(txdataF);
    free(ref);
    free(orig);
  }
  printf("ofdm mod == nr_tx_rotation_and_ofdm_mod\n");
}

static void test_demod_matches_monolithic(void)
{
  const int fft = fp.ofdm_symbol_size;
  const int n_re = fp.N_RB_DL * 12;
  unsigned seed = 2;
  // Two frames of time-domain signal, as nr_slot_fep() expects in rxdata.
  const int total = 2 * fp.samples_per_frame;
  c16_t *rxdata = malloc16_clear(total * sizeof(c16_t));
  c16_t scratch[fft] __attribute__((aligned(64)));
  for (int slot = 0; slot < 3; slot++) {
    int off = get_samples_slot_timestamp(&fp, slot);
    for (int s = 0; s < fp.symbols_per_slot; s++) {
      c16_t x[fft];
      fill_random(x, n_re, &seed);
      split7_ofdm_mod(&fp, link_type_dl, fp.N_RB_DL, slot, s, x, scratch, rxdata + off);
      off += split7_symbol_samples(&fp, slot, s);
    }
  }
  PHY_VARS_NR_UE *ue = calloc(1, sizeof(*ue));
  ue->is_synchronized = 1;
  c16_t(*rxdataF)[fp.samples_per_slot_wCP] = malloc16_clear(fp.samples_per_slot_wCP * sizeof(c16_t));
  c16_t *rx[1] = {rxdata};
  for (int slot = 0; slot < 3; slot++) {
    for (int s = 0; s < fp.symbols_per_slot; s++) {
      nr_slot_fep(ue, &fp, 0, slot, s, rxdataF, link_type_dl, 0, rx);
      c16_t out[fft] __attribute__((aligned(64)));
      const int start = get_samples_slot_timestamp(&fp, slot) + get_samples_symbol_timestamp(&fp, slot, s);
      split7_ofdm_demod(&fp, link_type_dl, fp.N_RB_DL, slot, s, rxdata + start, out);
      CHECK(memcmp(out, rxdataF[0] + s * fft, n_re * sizeof(c16_t)) == 0,
            "demod slot %d symbol %d differs from nr_slot_fep",
            slot,
            s);
    }
  }
  free(rxdataF);
  free(ue);
  free(rxdata);
  printf("ofdm demod == nr_slot_fep\n");
}

/* ------------------------------------------------------------------------- */
/* Fake radio with a test-driven clock                                       */
/* ------------------------------------------------------------------------- */

typedef struct {
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  int64_t clock; // next sample to deliver
  int64_t limit; // the test lets the clock run up to here
  bool stop;
  const c16_t *rx_signal; // optional, indexed by timestamp
  int64_t rx_signal_len;
  c16_t *tx_capture; // indexed by timestamp
  int64_t tx_capture_len;
  int64_t tx_written_until;
  int64_t min_tx_budget; // smallest (write timestamp - clock) seen
  int late_writes; // writes vrtsim would drop as too late (start before its clock)
  int num_writes;
  struct {
    int64_t ts;
    int n;
    int flags;
  } writes[4096];
  struct timespec t0; // the clock runs at the sampling rate from here
} fake_radio_t;

static fake_radio_t fake = {.mutex = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};

static int fake_start(openair0_device_t *dev)
{
  return 0;
}

static int fake_read(openair0_device_t *dev, openair0_timestamp_t *ts, void **buff, int nsamps, int nbAnt)
{
  pthread_mutex_lock(&fake.mutex);
  while (!fake.stop && fake.clock + nsamps > fake.limit)
    pthread_cond_wait(&fake.cond, &fake.mutex);
  if (fake.stop) {
    pthread_mutex_unlock(&fake.mutex);
    usleep(1000);
    return -1;
  }
  // Real-time pacing, so the engine's deadlines mean something.
  pthread_mutex_unlock(&fake.mutex);
  const int64_t ns = (fake.clock + nsamps) * 1000000LL / fp.samples_per_subframe;
  struct timespec due = {.tv_sec = fake.t0.tv_sec + ns / 1000000000, .tv_nsec = fake.t0.tv_nsec + ns % 1000000000};
  if (due.tv_nsec >= 1000000000) {
    due.tv_sec++;
    due.tv_nsec -= 1000000000;
  }
  clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &due, NULL);
  pthread_mutex_lock(&fake.mutex);
  *ts = fake.clock;
  for (int a = 0; a < nbAnt; a++) {
    c16_t *dst = buff[a];
    for (int i = 0; i < nsamps; i++)
      dst[i] = fake.rx_signal && fake.clock + i < fake.rx_signal_len ? fake.rx_signal[fake.clock + i] : (c16_t){0};
  }
  fake.clock += nsamps;
  pthread_cond_broadcast(&fake.cond);
  pthread_mutex_unlock(&fake.mutex);
  return nsamps;
}

static int fake_write(openair0_device_t *dev, openair0_timestamp_t ts, void **buff, int nsamps, int nbAnt, int flags)
{
  pthread_mutex_lock(&fake.mutex);
  const int64_t budget = ts - fake.clock;
  if (budget < fake.min_tx_budget)
    fake.min_tx_budget = budget;
  if (budget < 0)
    fake.late_writes++;
  if (fake.num_writes < (int)(sizeof(fake.writes) / sizeof(fake.writes[0])))
    fake.writes[fake.num_writes++] = (typeof(fake.writes[0])){.ts = ts, .n = nsamps, .flags = flags};
  const c16_t *src = buff[0];
  for (int i = 0; i < nsamps; i++)
    if (ts + i >= 0 && ts + i < fake.tx_capture_len)
      fake.tx_capture[ts + i] = src[i];
  if (ts + nsamps > fake.tx_written_until)
    fake.tx_written_until = ts + nsamps;
  pthread_cond_broadcast(&fake.cond);
  pthread_mutex_unlock(&fake.mutex);
  return nsamps;
}

static openair0_device_t fake_dev = {.trx_start_func = fake_start, .trx_read_func = fake_read, .trx_write_func = fake_write};

static void fake_reset(int64_t capture_len)
{
  free(fake.tx_capture);
  fake.clock = 0;
  fake.limit = 0;
  fake.stop = false;
  fake.rx_signal = NULL;
  fake.rx_signal_len = 0;
  fake.tx_capture = calloc(capture_len, sizeof(c16_t));
  fake.tx_capture_len = capture_len;
  fake.tx_written_until = 0;
  fake.min_tx_budget = INT64_MAX;
  fake.late_writes = 0;
  fake.num_writes = 0;
  clock_gettime(CLOCK_MONOTONIC, &fake.t0);
}

// Lets the radio clock run to ts, waiting until the engine has read it.
static void run_clock_to(int64_t ts)
{
  pthread_mutex_lock(&fake.mutex);
  fake.limit = ts;
  pthread_cond_broadcast(&fake.cond);
  while (fake.clock + (int64_t)fp.ofdm_symbol_size <= ts)
    pthread_cond_wait(&fake.cond, &fake.mutex);
  pthread_mutex_unlock(&fake.mutex);
}

// Waits (bounded) until the engine has transmitted (or passed over silence) up to ts.
static bool wait_tx_until(split7_engine_t *e, int64_t ts)
{
  return split7_progress_wait(&e->tx_sent, ts, 2000000) == SPLIT7_REACHED;
}

static void fake_stop(void)
{
  pthread_mutex_lock(&fake.mutex);
  fake.stop = true;
  pthread_cond_broadcast(&fake.cond);
  pthread_mutex_unlock(&fake.mutex);
}

static split7_engine_t *make_engine(void)
{
  split7_engine_cfg_t cfg = {
      .fp = &fp,
      .rx_link = link_type_dl,
      .tx_link = link_type_ul,
      .radio = &fake_dev,
  };
  split7_engine_t *e = split7_engine_create(&cfg);
  AssertFatal(split7_engine_start(e) == 0, "start\n");
  return e;
}

static int64_t slot_ts(uint64_t abs_slot)
{
  const split7_timebase_t tb = {0, 0};
  return split7_symbol_ts(&tb, &fp, abs_slot * fp.symbols_per_slot);
}

// Expected air signal of one symbol: modulated, then placed TA samples early,
// samples before from dropped.
static void add_expected_from(c16_t *expected, int64_t len, uint64_t abs_sym, int ta, const c16_t *re, int64_t from)
{
  const split7_timebase_t tb = {0, 0};
  const int slot = (abs_sym / fp.symbols_per_slot) % fp.slots_per_frame;
  const int sym = abs_sym % fp.symbols_per_slot;
  const int n = split7_symbol_samples(&fp, slot, sym);
  c16_t scratch[fp.ofdm_symbol_size] __attribute__((aligned(64)));
  c16_t out[n];
  split7_ofdm_mod(&fp, link_type_ul, fp.N_RB_UL, slot, sym, re, scratch, out);
  const int64_t start = split7_symbol_ts(&tb, &fp, abs_sym) - ta;
  for (int i = 0; i < n; i++) {
    if (start + i < 0 || start + i >= len || start + i < from)
      continue;
    const int32_t r = expected[start + i].r + out[i].r, im = expected[start + i].i + out[i].i;
    expected[start + i] =
        (c16_t){.r = r > 32767 ? 32767 : (r < -32768 ? -32768 : r), .i = im > 32767 ? 32767 : (im < -32768 ? -32768 : im)};
  }
}

static int max_abs_diff(const c16_t *a, const c16_t *b, int64_t from, int64_t to)
{
  int m = 0;
  for (int64_t i = from; i < to; i++) {
    const int dr = abs(a[i].r - b[i].r), di = abs(a[i].i - b[i].i);
    m = dr > m ? dr : m;
    m = di > m ? di : m;
  }
  return m;
}

// Signal-to-difference ratio of a against reference b, in dB.
static double sdr_db(const c16_t *a, const c16_t *b, int64_t from, int64_t to)
{
  double sig = 0, err = 0;
  for (int64_t i = from; i < to; i++) {
    sig += (double)b[i].r * b[i].r + (double)b[i].i * b[i].i;
    err += (double)(a[i].r - b[i].r) * (a[i].r - b[i].r) + (double)(a[i].i - b[i].i) * (a[i].i - b[i].i);
  }
  return err ? 10 * log10(sig / err) : 999;
}

static int count_diff(const c16_t *a, const c16_t *b, int64_t from, int64_t to)
{
  int diff = 0;
  for (int64_t i = from; i < to; i++)
    diff += a[i].r != b[i].r || a[i].i != b[i].i;
  return diff;
}

/* ------------------------------------------------------------------------- */
/* Engine tests                                                              */
/* ------------------------------------------------------------------------- */

// Deterministic REs of symbol k as posted by the client tagged tag.
static void make_re(c16_t *re, uint64_t k, int tag)
{
  unsigned seed = k * 7919 + tag;
  fill_random(re, fp.N_RB_UL * 12, &seed);
}

static void post(split7_client_t *c, uint64_t k, int tag)
{
  c16_t re[fp.N_RB_UL * 12];
  make_re(re, k, tag);
  c16_t *data[1] = {re};
  split7_client_tx_symbol(c, k, data, 1);
}

static split7_client_state_t client_state(split7_client_t *c)
{
  split7_monitor_lock(&c->mem->monitor);
  const split7_client_state_t state = c->mem->state;
  split7_monitor_unlock(&c->mem->monitor);
  return state;
}

static uint64_t client_join_slot(split7_client_t *c)
{
  split7_monitor_lock(&c->mem->monitor);
  const uint64_t slot = c->mem->join_slot;
  split7_monitor_unlock(&c->mem->monitor);
  return slot;
}

static bool record_written(split7_client_t *c, uint64_t slot)
{
  split7_monitor_lock(&c->mem->monitor);
  const bool written = split7_port_record(c->port, c->id, slot)->tag == slot + 1;
  split7_monitor_unlock(&c->mem->monitor);
  return written;
}

static bool wait_state(split7_client_t *c, split7_client_state_t state)
{
  for (int i = 0; i < 2000; i++) {
    if (client_state(c) == state)
      return true;
    usleep(1000);
  }
  return false;
}

// A member's script: symbols posted per slot (bit masks), TA per slot.
typedef struct {
  split7_client_t c;
  int tag;
  uint32_t used[128];
  int ta[128];
  int prach_slot; // -1 for none
} script_t;

static void script_init(script_t *sc, split7_port_t *port, int tag, int ta)
{
  memset(sc, 0, sizeof(*sc));
  CHECK(split7_client_open(port, &sc->c) == 0, "open client");
  sc->tag = tag;
  sc->prach_slot = -1;
  for (int i = 0; i < 128; i++)
    sc->ta[i] = ta;
}

// Posts and completes slots [first, last], in the given order of slots.
static void script_run(script_t *sc, const uint64_t *order, int n)
{
  const int sps = fp.symbols_per_slot;
  for (int i = 0; i < n; i++) {
    const uint64_t slot = order[i];
    for (int sym = 0; sym < sps; sym++)
      if (sc->used[slot] & (1u << sym))
        post(&sc->c, slot * sps + sym, sc->tag);
    split7_client_slot_done(&sc->c, slot, sc->used[slot], -1, sc->ta[slot]);
  }
}

static void script_run_range(script_t *sc, uint64_t first, uint64_t last)
{
  uint64_t order[128];
  for (uint64_t s = first; s <= last; s++)
    order[s - first] = s;
  script_run(sc, order, last - first + 1);
}

// Expected air signal of a script from first_slot on, at the TAs of the group
// leader's script lead: a slot after a TA increase loses its first samples.
static void script_expected_led(const script_t *sc,
                                const script_t *lead,
                                c16_t *expected,
                                int64_t len,
                                uint64_t first_slot,
                                uint64_t last_slot)
{
  const int sps = fp.symbols_per_slot;
  int64_t reach = slot_ts(first_slot) - lead->ta[first_slot];
  for (uint64_t slot = first_slot; slot <= last_slot; slot++) {
    const int64_t clip = reach;
    c16_t re[fp.N_RB_UL * 12];
    for (int sym = 0; sym < sps; sym++)
      if (sc->used[slot] & (1u << sym)) {
        make_re(re, slot * sps + sym, sc->tag);
        add_expected_from(expected, len, slot * sps + sym, lead->ta[slot], re, clip);
      }
    const int64_t end = slot_ts(slot + 1) - lead->ta[slot];
    reach = end > reach ? end : reach;
  }
}

static void script_expected(const script_t *sc, c16_t *expected, int64_t len, uint64_t first_slot, uint64_t last_slot)
{
  script_expected_led(sc, sc, expected, len, first_slot, last_slot);
}

static void test_ul_combining(void)
{
  const int64_t len = 3 * fp.samples_per_frame;
  fake_reset(len);
  split7_engine_t *e = make_engine();
  split7_engine_set_anchor(e, 0, 0, NULL);
  split7_port_t *port = split7_engine_port(e);
  script_t a, b;
  script_init(&a, port, 1, 0);
  script_init(&b, port, 2, 777);
  split7_client_join(&a.c, 8, 0);
  split7_client_join(&b.c, 8, 777);
  CHECK(wait_state(&a.c, SPLIT7_CLIENT_MEMBER) && wait_state(&b.c, SPLIT7_CLIENT_MEMBER), "join");
  CHECK(client_join_slot(&a.c) == 8 && client_join_slot(&b.c) == 8, "joined later than asked");
  a.used[10] = 0xc;
  a.used[30] = 0x1;
  a.used[12] = 0x8;
  b.used[10] = 0x20;
  b.used[29] = 0x2000;
  script_run_range(&a, 8, 44);
  script_run_range(&b, 8, 44);
  // Posted after its slot was completed and not listed in the record: never sent.
  post(&a.c, 40 * fp.symbols_per_slot + 3, 9);

  c16_t *expected = calloc(len, sizeof(c16_t));
  script_expected(&a, expected, len, 8, 44);
  script_expected_led(&b, &a, expected, len, 8, 44);
  run_clock_to(slot_ts(48));
  CHECK(wait_tx_until(e, slot_ts(42)), "engine did not transmit");
  const int diff = count_diff(fake.tx_capture, expected, slot_ts(7), slot_ts(42));
  CHECK(diff == 0, "UL combining: %d samples differ", diff);

  fake_stop();
  split7_client_close(&a.c);
  split7_client_close(&b.c);
  split7_engine_destroy(e);
  free(expected);
  printf("UL combining: 2 members at the leader's TA, only recorded symbols sent\n");
}

static void test_group_sum(void)
{
  const int64_t len = 2 * fp.samples_per_frame;
  fake_reset(len);
  split7_engine_t *e = make_engine();
  split7_engine_set_anchor(e, 0, 0, NULL);
  split7_port_t *port = split7_engine_port(e);
  script_t a, b, c;
  script_init(&a, port, 1, 600);
  script_init(&b, port, 2, 600);
  script_init(&c, port, 3, 650); // not the leader: its TA is not used
  split7_client_join(&a.c, 8, 600);
  split7_client_join(&b.c, 8, 600);
  split7_client_join(&c.c, 8, 650);
  CHECK(wait_state(&a.c, SPLIT7_CLIENT_MEMBER) && wait_state(&b.c, SPLIT7_CLIENT_MEMBER) && wait_state(&c.c, SPLIT7_CLIENT_MEMBER),
        "join");
  // Symbol 10.4: all three, one IDFT; symbol 11.1: A alone.
  a.used[10] = b.used[10] = c.used[10] = 1 << 4;
  a.used[11] = 1 << 1;
  script_run_range(&a, 8, 30);
  script_run_range(&b, 8, 30);
  script_run_range(&c, 8, 30);
  c16_t *expected = calloc(len, sizeof(c16_t));
  script_expected(&a, expected, len, 8, 30);
  script_expected_led(&b, &a, expected, len, 8, 30);
  script_expected_led(&c, &a, expected, len, 8, 30);
  run_clock_to(slot_ts(20));
  CHECK(wait_tx_until(e, slot_ts(14)), "engine stopped");

  CHECK(split7_engine_stats(e).tx_idft == 2, "%lu IDFTs, expected 2", split7_engine_stats(e).tx_idft);
  // Summing before the fixed-point rotation/IDFT rounds differently than summing after.
  const split7_timebase_t tb = {0, 0};
  const int64_t s1 = split7_symbol_ts(&tb, &fp, 10 * fp.symbols_per_slot + 4) - 600;
  const int s1_len = split7_symbol_samples(&fp, 10, 4);
  const int dev = max_abs_diff(fake.tx_capture, expected, s1, s1 + s1_len);
  const double sdr = sdr_db(fake.tx_capture, expected, s1, s1 + s1_len);
  CHECK(sdr > 50, "group sum is only %.1f dB from per-member modulation (max %d LSB)", sdr, dev);
  const int64_t s2 = split7_symbol_ts(&tb, &fp, 11 * fp.symbols_per_slot + 1) - 600;
  CHECK(count_diff(fake.tx_capture, expected, s2, s2 + split7_symbol_samples(&fp, 11, 1)) == 0,
        "single-member symbol not bit-exact");

  fake_stop();
  split7_client_close(&a.c);
  split7_client_close(&b.c);
  split7_client_close(&c.c);
  split7_engine_destroy(e);
  free(expected);
  printf("group: one IDFT per symbol at the leader's TA, %.1f dB from per-member modulation (max %d LSB)\n", sdr, dev);
}

static void test_leader(void)
{
  const int64_t len = 2 * fp.samples_per_frame;
  fake_reset(len);
  split7_engine_t *e = make_engine();
  split7_engine_set_anchor(e, 0, 0, NULL);
  split7_port_t *port = split7_engine_port(e);
  script_t a, b;
  script_init(&a, port, 1, 100);
  script_init(&b, port, 2, 900);
  split7_client_join(&a.c, 8, 100);
  split7_client_join(&b.c, 8, 900);
  CHECK(wait_state(&a.c, SPLIT7_CLIENT_MEMBER) && wait_state(&b.c, SPLIT7_CLIENT_MEMBER), "join");
  CHECK(split7_client_is_timing_master(&a.c) && !split7_client_is_timing_master(&b.c), "lowest id leads");
  a.used[10] = 1 << 2;
  b.used[12] = 1 << 5;
  b.used[26] = 1 << 3;
  script_run_range(&a, 8, 20);
  script_run_range(&b, 8, 40);
  run_clock_to(slot_ts(12));
  split7_client_leave(&a.c, 20, 0); // request only: confirmed once slot 20 is sent
  run_clock_to(slot_ts(44));
  CHECK(wait_state(&a.c, SPLIT7_CLIENT_OPEN), "leave");
  CHECK(split7_client_is_timing_master(&b.c), "the next member leads after the leader left");
  CHECK(wait_tx_until(e, slot_ts(38)), "engine stopped");
  // Up to slot 20 the group runs on A's TA (B's included), then on B's.
  script_t lead = b;
  for (int sl = 0; sl <= 20; sl++)
    lead.ta[sl] = 100;
  c16_t *expected = calloc(len, sizeof(c16_t));
  script_expected_led(&a, &lead, expected, len, 8, 20);
  script_expected_led(&b, &lead, expected, len, 8, 40);
  CHECK(count_diff(fake.tx_capture, expected, slot_ts(7), slot_ts(38)) == 0, "leader TA not followed");

  fake_stop();
  split7_client_close(&a.c);
  split7_client_close(&b.c);
  split7_engine_destroy(e);
  free(expected);
  printf("leader: its TA is the group's; the next member takes over when it leaves\n");
}

static void test_late_member(void)
{
  const int64_t len = 3 * fp.samples_per_frame;
  fake_reset(len);
  split7_engine_t *e = make_engine();
  split7_engine_set_anchor(e, 0, 0, NULL);
  split7_port_t *port = split7_engine_port(e);
  script_t a, b;
  script_init(&a, port, 1, 0);
  script_init(&b, port, 2, 300);
  split7_client_join(&a.c, 8, 0);
  split7_client_join(&b.c, 8, 300);
  CHECK(wait_state(&a.c, SPLIT7_CLIENT_MEMBER) && wait_state(&b.c, SPLIT7_CLIENT_MEMBER), "join");
  for (int s = 8; s <= 40; s++) {
    a.used[s] = 0x0f0f;
    b.used[s] = 0x3000;
  }
  script_run_range(&a, 8, 40);
  // B is silent while the radio runs well past its first slots: nothing may go out.
  run_clock_to(slot_ts(22));
  usleep(5000);
  pthread_mutex_lock(&fake.mutex);
  const int64_t written = fake.tx_written_until;
  pthread_mutex_unlock(&fake.mutex);
  CHECK(written <= slot_ts(8), "transmitted up to %ld before the late member completed (frontier %ld)", written, slot_ts(8));
  script_run_range(&b, 8, 40);
  run_clock_to(slot_ts(44));
  CHECK(wait_tx_until(e, slot_ts(40)), "engine stopped");

  c16_t *expected = calloc(len, sizeof(c16_t));
  script_expected(&a, expected, len, 8, 40);
  script_expected_led(&b, &a, expected, len, 8, 40);
  const int diff = count_diff(fake.tx_capture, expected, slot_ts(7), slot_ts(40));
  CHECK(diff == 0, "late member: %d samples differ (content must not depend on timing)", diff);
  CHECK(fake.late_writes > 0 && split7_engine_stats(e).tx_late_writes > 0,
        "late writes: radio %d, engine %lu",
        fake.late_writes,
        split7_engine_stats(e).tx_late_writes);
  CHECK(split7_client_stats(&b.c).late_stretches > 0 && split7_client_stats(&a.c).late_stretches == 0,
        "late stretches blamed on the wrong member");

  fake_stop();
  split7_client_close(&a.c);
  split7_client_close(&b.c);
  split7_engine_destroy(e);
  free(expected);
  printf("late member: everyone late, nothing sent early, content unchanged\n");
}

typedef struct {
  script_t *sc;
  unsigned seed;
} jitter_arg_t;

// Completes slots 8..40 out of order (pairs swapped) with random pauses.
static void *jitter_thread(void *arg)
{
  jitter_arg_t *j = arg;
  uint64_t order[33];
  for (int i = 0; i < 33; i++)
    order[i] = 8 + i;
  for (int i = 0; i + 1 < 33; i += 2)
    if (rand_r(&j->seed) & 1) {
      const uint64_t t = order[i];
      order[i] = order[i + 1];
      order[i + 1] = t;
    }
  for (int i = 0; i < 33; i++) {
    usleep(rand_r(&j->seed) % 400);
    script_run(j->sc, &order[i], 1);
  }
  return NULL;
}

static void test_determinism(void)
{
  const int64_t len = 2 * fp.samples_per_frame;
  c16_t *first = NULL;
  for (int run = 0; run < 3; run++) {
    fake_reset(len);
    split7_engine_t *e = make_engine();
    split7_engine_set_anchor(e, 0, 0, NULL);
    split7_port_t *port = split7_engine_port(e);
    script_t a, b;
    script_init(&a, port, 1, 600);
    script_init(&b, port, 2, 600);
    split7_client_join(&a.c, 8, 600);
    split7_client_join(&b.c, 8, 600);
    CHECK(wait_state(&a.c, SPLIT7_CLIENT_MEMBER) && wait_state(&b.c, SPLIT7_CLIENT_MEMBER), "join");
    for (int s = 8; s <= 40; s++) {
      a.used[s] = 0x0ff0;
      b.used[s] = 0x00ff;
      a.ta[s] = b.ta[s] = 600 + (s >= 20 ? 24 : 0); // TA command at slot 20: clipped
    }
    jitter_arg_t ja = {&a, 100 + run}, jb = {&b, 200 + run};
    pthread_t ta, tb;
    pthread_create(&ta, NULL, jitter_thread, &ja);
    pthread_create(&tb, NULL, jitter_thread, &jb);
    for (int s = 10; s <= 44; s += 2)
      run_clock_to(slot_ts(s));
    pthread_join(ta, NULL);
    pthread_join(tb, NULL);
    run_clock_to(slot_ts(46));
    CHECK(wait_tx_until(e, slot_ts(40)), "engine stopped");
    if (!first) {
      first = malloc(len * sizeof(c16_t));
      memcpy(first, fake.tx_capture, len * sizeof(c16_t));
    } else {
      CHECK(count_diff(fake.tx_capture, first, 0, slot_ts(40)) == 0, "run %d differs from run 0", run);
    }
    fake_stop();
    split7_client_close(&a.c);
    split7_client_close(&b.c);
    split7_engine_destroy(e);
  }
  free(first);
  printf("determinism: 3 runs, random interleaving and out-of-order completion, identical output\n");
}

static void test_bursts(void)
{
  const int64_t len = 2 * fp.samples_per_frame;
  fake_reset(len);
  split7_engine_t *e = make_engine();
  split7_engine_set_anchor(e, 0, 0, NULL);
  script_t a;
  script_init(&a, split7_engine_port(e), 1, 600);
  split7_client_join(&a.c, 8, 600);
  CHECK(wait_state(&a.c, SPLIT7_CLIENT_MEMBER), "join");
  // Three runs of content: 10.2-4, 10.8, and 10.13 + 11.0 across the slot (stretch) boundary.
  a.used[10] = 0x7 << 2 | 1 << 8 | 1 << 13;
  a.used[11] = 1 << 0;
  script_run_range(&a, 8, 30);
  run_clock_to(slot_ts(20));
  CHECK(wait_tx_until(e, slot_ts(14)), "engine stopped");
  const split7_timebase_t tb = {0, 0};
  const int64_t sym = fp.symbols_per_slot;
  struct {
    int64_t start, end;
  } runs[3] = {{split7_symbol_ts(&tb, &fp, 10 * sym + 2) - 600, split7_symbol_ts(&tb, &fp, 10 * sym + 5) - 600},
               {split7_symbol_ts(&tb, &fp, 10 * sym + 8) - 600, split7_symbol_ts(&tb, &fp, 10 * sym + 9) - 600},
               {split7_symbol_ts(&tb, &fp, 10 * sym + 13) - 600, split7_symbol_ts(&tb, &fp, 11 * sym + 1) - 600}};
  // Writes are exactly the runs, each one burst: START (or START_AND_END) ... END, contiguous.
  int r = 0;
  int64_t next = -1;
  bool in_burst = false;
  for (int i = 0; i < fake.num_writes; i++) {
    const int64_t ts = fake.writes[i].ts, n = fake.writes[i].n;
    const int fl = fake.writes[i].flags;
    CHECK(r < 3, "write %d at %ld beyond the three runs", i, ts);
    if (r >= 3)
      break;
    if (!in_burst) {
      CHECK(ts == runs[r].start && (fl == TX_BURST_START || fl == TX_BURST_START_AND_END),
            "run %d does not start a burst at %ld",
            r,
            ts);
      in_burst = true;
    } else {
      CHECK(ts == next && (fl == TX_BURST_MIDDLE || fl == TX_BURST_END), "run %d: write %d not contiguous or bad flags", r, i);
    }
    next = ts + n;
    if (fl == TX_BURST_END || fl == TX_BURST_START_AND_END) {
      CHECK(next == runs[r].end, "run %d ends at %ld, expected %ld", r, next, runs[r].end);
      in_burst = false;
      r++;
    }
  }
  CHECK(r == 3 && !in_burst, "%d complete bursts, expected 3", r);
  // Every slot was complete early: each burst's last content write ended it.
  CHECK(split7_engine_stats(e).burst_end_markers == 0, "bursts ended late by a separate write");
  c16_t *expected = calloc(len, sizeof(c16_t));
  script_expected(&a, expected, len, 8, 30);
  CHECK(count_diff(fake.tx_capture, expected, 0, slot_ts(14)) == 0, "burst content differs");
  // The last member leaving: nothing bounds TX any more, its slots are still passed.
  split7_client_leave(&a.c, 30, 0);
  run_clock_to(slot_ts(36));
  CHECK(wait_state(&a.c, SPLIT7_CLIENT_OPEN), "the last member's leave was not confirmed");
  fake_stop();
  split7_client_close(&a.c);
  split7_engine_destroy(e);
  free(expected);
  printf("bursts: only content is written, one START..END burst per run of symbols\n");
}

static void test_ta_change(void)
{
  const int64_t len = 2 * fp.samples_per_frame;
  fake_reset(len);
  split7_engine_t *e = make_engine();
  split7_engine_set_anchor(e, 0, 0, NULL);
  script_t a;
  script_init(&a, split7_engine_port(e), 1, 600);
  split7_client_join(&a.c, 8, 600);
  CHECK(wait_state(&a.c, SPLIT7_CLIENT_MEMBER), "join");
  for (int s = 11; s < 128; s++)
    a.ta[s] = 650; // (a) +50: the slot loses its first 50 samples (CP)
  a.used[10] = 1 << 13;
  a.used[11] = 1 << 0;
  for (int s = 16; s < 128; s++)
    a.ta[s] = 700; // (b) +50 after an empty slot: shortened all the same
  a.used[16] = 1 << 0;
  for (int s = 21; s < 128; s++)
    a.ta[s] = 700 + 1000; // (c) +1000: more than the CP is lost
  a.used[21] = 1 << 0;
  for (int s = 26; s < 128; s++)
    a.ta[s] = 1650; // (d) -50: a gap, nothing lost
  a.used[25] = 1 << 13;
  a.used[26] = 1 << 0;
  script_run_range(&a, 8, 34);
  run_clock_to(slot_ts(36));
  CHECK(wait_tx_until(e, slot_ts(30)), "engine stopped");
  c16_t *expected = calloc(len, sizeof(c16_t));
  script_expected(&a, expected, len, 8, 34);
  CHECK(count_diff(fake.tx_capture, expected, slot_ts(7), slot_ts(30)) == 0, "TA changes not handled as specified");
  CHECK(split7_client_stats(&a.c).ta_clipped == 50 + 50 + 1000,
        "%lu samples clipped, expected 1100",
        split7_client_stats(&a.c).ta_clipped);
  fake_stop();
  split7_client_close(&a.c);
  split7_engine_destroy(e);
  free(expected);
  printf("TA change: increase drops the slot's first samples, decrease leaves a gap of zeros\n");
}

static void test_prach(void)
{
  const int64_t len = 2 * fp.samples_per_frame;
  fake_reset(len);
  split7_engine_t *e = make_engine();
  split7_engine_set_anchor(e, 0, 0, NULL);
  split7_client_t a;
  split7_client_open(split7_engine_port(e), &a);
  split7_client_join(&a, 8, 300);
  CHECK(wait_state(&a, SPLIT7_CLIENT_MEMBER), "join");

  const int dftlen = 1536, ncp = 144, reps = 12, offset = 200;
  c16_t bins[dftlen] __attribute__((aligned(64)));
  unsigned seed = 5;
  fill_random(bins, dftlen, &seed);
  const int entry = split7_client_tx_prach(&a, 9, offset, ncp, dftlen, reps, bins);
  CHECK(entry >= 0, "post PRACH");
  for (int s = 8; s <= 30; s++)
    split7_client_slot_done(&a, s, 0, s == 9 ? entry : -1, 300);
  run_clock_to(slot_ts(24));
  CHECK(wait_tx_until(e, slot_ts(18)), "engine stopped");

  c16_t scratch[dftlen] __attribute__((aligned(64)));
  c16_t *wave = malloc((ncp + reps * dftlen) * sizeof(c16_t));
  split7_ofdm_window_mod(bins, dftlen, ncp, reps, scratch, wave);
  c16_t *expected = calloc(len, sizeof(c16_t));
  memcpy(expected + slot_ts(9) + offset - 300, wave, (ncp + reps * dftlen) * sizeof(c16_t));
  const int diff = count_diff(fake.tx_capture, expected, slot_ts(7), slot_ts(18));
  CHECK(diff == 0, "PRACH: %d samples differ", diff);

  fake_stop();
  split7_client_close(&a);
  split7_engine_destroy(e);
  free(wave);
  free(expected);
  printf("PRACH placed at slot + offset - TA\n");
}

static void test_join_leave(void)
{
  const int64_t len = 3 * fp.samples_per_frame;
  fake_reset(len);
  split7_engine_t *e = make_engine();
  split7_engine_set_anchor(e, 0, 0, NULL);
  split7_port_t *port = split7_engine_port(e);
  script_t a, c;
  script_init(&a, port, 1, 0);
  script_init(&c, port, 3, 0);
  split7_client_join(&a.c, 8, 0);
  split7_client_join(&c.c, 8, 0);
  CHECK(wait_state(&a.c, SPLIT7_CLIENT_MEMBER) && wait_state(&c.c, SPLIT7_CLIENT_MEMBER), "join");
  for (int s = 8; s <= 44; s++) {
    a.used[s] = 0x0003;
    c.used[s] = 0x0300;
  }
  script_run_range(&c, 8, 13); // 14 and 15 never completed: leaving declares them empty
  script_run_range(&a, 8, 44);
  run_clock_to(slot_ts(12));
  split7_client_leave(&c.c, 15, 0); // request only: confirmed once slot 15 is sent
  // Leaving: records and symbols it still posts are ignored, and it no longer holds anyone up.
  script_run_range(&c, 16, 20);
  run_clock_to(slot_ts(48));
  CHECK(wait_state(&c.c, SPLIT7_CLIENT_OPEN), "leave not confirmed");
  CHECK(wait_tx_until(e, slot_ts(44)), "engine waited for a member that left");

  c16_t *expected = calloc(len, sizeof(c16_t));
  script_expected(&a, expected, len, 8, 44);
  script_expected(&c, expected, len, 8, 13);
  CHECK(count_diff(fake.tx_capture, expected, slot_ts(7), slot_ts(44)) == 0, "join/leave: wrong samples");
  // Rejoining works and starts no earlier than the host can still take.
  split7_client_join(&c.c, 10, 0);
  CHECK(wait_state(&c.c, SPLIT7_CLIENT_MEMBER), "rejoin");
  CHECK(slot_ts(client_join_slot(&c.c)) >= slot_ts(44), "rejoined at slot %lu, already transmitted", client_join_slot(&c.c));

  fake_stop();
  split7_client_close(&a.c);
  split7_client_close(&c.c);
  split7_engine_destroy(e);
  free(expected);
  printf("join/leave: leaving ends the wait, later posts ignored, rejoin after the transmitted part\n");
}

typedef struct {
  split7_client_t *c;
  uint64_t slot;
  split7_progress_t finished; ///< 1 once the post returned
} ahead_arg_t;

static void *ahead_thread(void *arg)
{
  ahead_arg_t *a = arg;
  split7_client_slot_done(a->c, a->slot, 0, -1, 0);
  split7_progress_raise(&a->finished, 1);
  return NULL;
}

static void test_backpressure(void)
{
  fake_reset(2 * fp.samples_per_frame);
  split7_engine_t *e = make_engine();
  split7_engine_set_anchor(e, 0, 0, NULL);
  split7_port_t *port = split7_engine_port(e);
  split7_client_t a, b;
  split7_client_open(port, &a);
  split7_client_open(port, &b);
  split7_client_join(&a, 8, 0);
  split7_client_join(&b, 8, 0);
  CHECK(wait_state(&a, SPLIT7_CLIENT_MEMBER) && wait_state(&b, SPLIT7_CLIENT_MEMBER), "join");
  // The non-blocking variant neither waits nor writes without room.
  const uint64_t far = 8 + port->mem->ring_slots - 1;
  CHECK(!split7_client_try_slot_done(&a, far, 0, -1, 0), "try_slot_done wrote without room");
  CHECK(!record_written(&a, far), "try_slot_done wrote a record without room");
  CHECK(split7_client_try_slot_done(&a, 9, 0, -1, 0), "try_slot_done refused a slot with room");
  // A runs a whole ring ahead of the stalled B: it must wait, not overwrite its own entries.
  ahead_arg_t arg = {.c = &a, .slot = 8 + port->mem->ring_slots - 1};
  split7_progress_init(&arg.finished, 0);
  pthread_t t;
  pthread_create(&t, NULL, ahead_thread, &arg);
  usleep(50000);
  CHECK(!split7_progress_get(&arg.finished), "a member ran a ring ahead of the host");
  for (uint64_t s = 8; s < 8 + port->mem->ring_slots - 1; s++) {
    split7_client_slot_done(&a, s, 0, -1, 0);
    split7_client_slot_done(&b, s, 0, -1, 0);
  }
  run_clock_to(slot_ts(8 + port->mem->ring_slots));
  pthread_join(t, NULL);
  CHECK(split7_progress_get(&arg.finished), "backpressure never released");
  split7_progress_destroy(&arg.finished);
  fake_stop();
  split7_client_close(&a);
  split7_client_close(&b);
  split7_engine_destroy(e);
  printf("backpressure: a member waits instead of running a ring ahead\n");
}

typedef struct {
  script_t *sc;
  uint64_t first, last;
} range_arg_t;

static void *range_thread(void *arg)
{
  range_arg_t *r = arg;
  script_run_range(r->sc, r->first, r->last);
  return NULL;
}

static void test_long_skip(void)
{
  const int64_t len = 2 * 6 * fp.samples_per_frame;
  fake_reset(len);
  split7_engine_t *e = make_engine();
  split7_engine_set_anchor(e, 0, 0, NULL);
  split7_port_t *port = split7_engine_port(e);
  script_t a, b;
  script_init(&a, port, 1, 0);
  script_init(&b, port, 2, 0);
  split7_client_join(&a.c, 8, 0);
  split7_client_join(&b.c, 8, 0);
  CHECK(wait_state(&a.c, SPLIT7_CLIENT_MEMBER) && wait_state(&b.c, SPLIT7_CLIENT_MEMBER), "join");
  for (int s = 8; s <= 120; s++)
    a.used[s] = 1u << (s % 14);
  // B completes a ring of slots with content, then skips far more than a ring and
  // carries on: its content must survive and neither member may stall the other.
  for (int s = 8; s <= 44; s++)
    b.used[s] = 1u << ((s + 7) % 14);
  script_run_range(&b, 8, 44);
  split7_client_skip_through(&b.c, 100);
  range_arg_t ra = {&a, 8, 120}, rb = {&b, 101, 120};
  pthread_t ta, tb;
  pthread_create(&ta, NULL, range_thread, &ra);
  pthread_create(&tb, NULL, range_thread, &rb);
  for (int s = 10; s <= 124; s += 2)
    run_clock_to(slot_ts(s));
  pthread_join(ta, NULL);
  pthread_join(tb, NULL);
  CHECK(wait_tx_until(e, slot_ts(118)), "engine stalled after a long skip");
  c16_t *expected = calloc(len, sizeof(c16_t));
  script_expected(&a, expected, len, 8, 120);
  script_expected(&b, expected, len, 8, 44);
  CHECK(count_diff(fake.tx_capture, expected, slot_ts(7), slot_ts(118)) == 0, "long skip: wrong samples");
  fake_stop();
  split7_client_close(&a.c);
  split7_client_close(&b.c);
  split7_engine_destroy(e);
  free(expected);
  printf("long skip: a member completing slots far ahead of the ring neither stalls nor corrupts\n");
}

static void test_drop_releases_backpressure(void)
{
  fake_reset(2 * fp.samples_per_frame);
  split7_engine_t *e = make_engine();
  split7_engine_set_anchor(e, 0, 0, NULL);
  split7_port_t *port = split7_engine_port(e);
  split7_client_t a, b;
  split7_client_open(port, &a);
  split7_client_open(port, &b);
  split7_client_join(&a, 8, 0);
  split7_client_join(&b, 8, 0);
  CHECK(wait_state(&a, SPLIT7_CLIENT_MEMBER) && wait_state(&b, SPLIT7_CLIENT_MEMBER), "join");
  ahead_arg_t arg = {.c = &a, .slot = 8 + port->mem->ring_slots - 1};
  split7_progress_init(&arg.finished, 0);
  pthread_t t;
  pthread_create(&t, NULL, ahead_thread, &arg);
  usleep(50000);
  CHECK(!split7_progress_get(&arg.finished), "not blocked");
  // B stalls for good; A gives its slots up: the blocked post returns without writing.
  split7_client_drop_through(&a, arg.slot);
  CHECK(split7_progress_wait(&arg.finished, 1, 100000) == SPLIT7_REACHED, "drop_through did not release the blocked post");
  pthread_join(t, NULL);
  split7_progress_destroy(&arg.finished);
  CHECK(!record_written(&a, arg.slot), "a dropped slot was recorded");
  fake_stop();
  split7_client_close(&a);
  split7_client_close(&b);
  split7_engine_destroy(e);
  printf("drop_through: a post waiting for room is released and discarded\n");
}

static void test_skipped_first_slot_ta(void)
{
  const int64_t len = 2 * fp.samples_per_frame;
  fake_reset(len);
  split7_engine_t *e = make_engine();
  split7_engine_set_anchor(e, 0, 0, NULL);
  split7_port_t *port = split7_engine_port(e);
  script_t b, a;
  script_init(&b, port, 2, 600); // id 0: leads once it sends
  script_init(&a, port, 1, 600); // id 1
  split7_client_join(&a.c, 8, 600);
  CHECK(wait_state(&a.c, SPLIT7_CLIENT_MEMBER), "join a");
  a.used[20] = 0x10;
  a.used[22] = 0x10;
  script_run_range(&a, 8, 12);
  usleep(5000);
  split7_client_join(&b.c, 20, 600);
  CHECK(wait_state(&b.c, SPLIT7_CLIENT_MEMBER), "join b");
  // B gives its first slot up: a slot without a record does not set the group TA.
  split7_client_skip_through(&b.c, 20);
  usleep(5000);
  script_run_range(&a, 13, 30);
  script_run_range(&b, 21, 30);
  run_clock_to(slot_ts(34));
  CHECK(wait_tx_until(e, slot_ts(30)), "engine stopped");
  c16_t *expected = calloc(len, sizeof(c16_t));
  script_expected(&a, expected, len, 8, 30);
  CHECK(count_diff(fake.tx_capture, expected, slot_ts(7), slot_ts(30)) == 0, "a skipped slot set the group TA");
  fake_stop();
  split7_client_close(&a.c);
  split7_client_close(&b.c);
  split7_engine_destroy(e);
  free(expected);
  printf("skipped first slot: the group TA comes from members that send the slot\n");
}

static void test_shift_after_member_left(void)
{
  fake_reset(2 * fp.samples_per_frame);
  split7_engine_t *e = make_engine();
  split7_engine_set_anchor(e, 0, 0, NULL);
  split7_port_t *port = split7_engine_port(e);
  split7_client_t a, b;
  split7_client_open(port, &a);
  split7_client_open(port, &b);
  split7_client_join(&a, 8, 0);
  split7_client_join(&b, 8, 0);
  CHECK(wait_state(&a, SPLIT7_CLIENT_MEMBER) && wait_state(&b, SPLIT7_CLIENT_MEMBER), "join");
  for (int s = 8; s <= 10; s++)
    split7_client_slot_done(&a, s, 0, -1, 0);
  for (int s = 8; s <= 40; s++)
    split7_client_slot_done(&b, s, 0, -1, 0);
  usleep(5000);
  split7_client_adjust_timing(&a, 5);
  run_clock_to(slot_ts(2));
  usleep(5000);
  // The member furthest ahead goes: the next shift must still start after the last one.
  split7_client_close(&b);
  usleep(5000);
  split7_client_adjust_timing(&a, 5);
  run_clock_to(slot_ts(4));
  usleep(20000);
  pthread_mutex_lock(&e->tb_mutex);
  CHECK(e->tb.num_shifts >= 1 && e->tb.shift[e->tb.num_shifts - 1].samples == 10, "shifts lost (%d)", e->tb.num_shifts);
  for (int i = 1; i < e->tb.num_shifts; i++)
    CHECK(e->tb.shift[i].from_slot >= e->tb.shift[i - 1].from_slot, "shift %d starts before the previous one", i);
  pthread_mutex_unlock(&e->tb_mutex);
  fake_stop();
  split7_client_close(&a);
  split7_engine_destroy(e);
  printf("timing shift after the furthest member left: in order\n");
}

static void test_leave_while_joining(void)
{
  fake_reset(2 * fp.samples_per_frame);
  split7_engine_t *e = make_engine();
  split7_engine_set_anchor(e, 0, 0, NULL);
  split7_port_t *port = split7_engine_port(e);
  int lost = 0;
  unsigned seed = 5;
  for (int i = 0; i < 300; i++) {
    split7_client_t c;
    if (split7_client_open(port, &c) != 0) {
      usleep(2000);
      continue;
    }
    split7_client_join(&c, 20, 0);
    for (volatile int k = 0; k < (int)(rand_r(&seed) % 60) * 50; k++) {
    }
    split7_client_leave(&c, 25, 0);
    // Whichever of join acceptance and leave came first, the client ends up leaving or left.
    usleep(1000);
    const split7_client_state_t st = client_state(&c);
    lost += st == SPLIT7_CLIENT_MEMBER || st == SPLIT7_CLIENT_JOINING;
    split7_client_close(&c);
  }
  CHECK(lost == 0, "leave lost against join acceptance %d/300 times", lost);
  fake_stop();
  split7_engine_destroy(e);
  printf("leave racing join acceptance: never lost\n");
}

static uint64_t prach_tag(split7_client_t *c, int i)
{
  split7_monitor_lock(&c->mem->monitor);
  const uint64_t tag = split7_port_prach_entry(c->port, c->id, i)->tag;
  split7_monitor_unlock(&c->mem->monitor);
  return tag;
}

static void test_close_reopen(void)
{
  fake_reset(2 * fp.samples_per_frame);
  split7_engine_t *e = make_engine();
  split7_engine_set_anchor(e, 0, 0, NULL);
  split7_port_t *port = split7_engine_port(e);
  c16_t bins[139] = {0};
  for (int i = 0; i < 50; i++) {
    // A member with a pending PRACH closes without leaving; the next client gets its slot.
    split7_client_t a, b;
    CHECK(split7_client_open(port, &a) == 0, "open");
    split7_client_join(&a, 8, 0);
    CHECK(wait_state(&a, SPLIT7_CLIENT_MEMBER), "join");
    CHECK(split7_client_tx_prach(&a, 9, 0, 0, 139, 1, bins) == 0, "prach");
    split7_client_close(&a);
    CHECK(client_state(&a) == SPLIT7_CLIENT_FREE && prach_tag(&a, 0) == 0, "closed client not released");
    CHECK(split7_client_open(port, &b) == 0 && b.id == a.id, "slot not reused");
    // Its PRACH is not released on behalf of the previous client.
    CHECK(split7_client_tx_prach(&b, 9, 0, 0, 139, 1, bins) == 0, "prach");
    usleep(3000);
    CHECK(prach_tag(&b, 0) == 10, "new client's PRACH released");
    CHECK(split7_client_join(&b, 8, 0) && wait_state(&b, SPLIT7_CLIENT_MEMBER), "new client cannot join");
    split7_client_close(&b);
  }
  fake_stop();
  split7_engine_destroy(e);
  printf("close: the host lets go of the client before its slot is reused\n");
}

static void test_rejoin_after_leave(void)
{
  fake_reset(2 * fp.samples_per_frame);
  split7_engine_t *e = make_engine();
  split7_engine_set_anchor(e, 0, 0, NULL);
  split7_port_t *port = split7_engine_port(e);
  split7_client_t c;
  split7_client_open(port, &c);
  split7_client_join(&c, 8, 0);
  CHECK(wait_state(&c, SPLIT7_CLIENT_MEMBER), "join");
  CHECK(!split7_client_join(&c, 9, 0), "joined twice");
  split7_client_leave(&c, 10, 0); // request only
  run_clock_to(slot_ts(14));
  // Waits until the host let it go, then asks for the new slot only.
  CHECK(split7_client_join(&c, 40, 0), "rejoin refused");
  CHECK(wait_state(&c, SPLIT7_CLIENT_MEMBER), "rejoin");
  CHECK(client_join_slot(&c) >= 40, "rejoined at slot %lu, asked for 40", client_join_slot(&c));
  fake_stop();
  split7_client_close(&c);
  split7_engine_destroy(e);
  printf("rejoin after a leave request: waits for the leave, then joins at the new slot\n");
}

static void test_rejoin_after_drop(void)
{
  fake_reset(2 * fp.samples_per_frame);
  split7_engine_t *e = make_engine();
  split7_engine_set_anchor(e, 0, 0, NULL);
  split7_client_t c;
  split7_client_open(split7_engine_port(e), &c);
  split7_client_join(&c, 8, 0);
  CHECK(wait_state(&c, SPLIT7_CLIENT_MEMBER), "join");
  // Gives up far ahead, leaves earlier, and comes back below what it gave up.
  split7_client_drop_through(&c, 60);
  split7_client_leave(&c, 10, 0);
  run_clock_to(slot_ts(14));
  CHECK(split7_client_join(&c, 20, 0), "rejoin refused");
  CHECK(wait_state(&c, SPLIT7_CLIENT_MEMBER), "rejoin");
  const uint64_t slot = client_join_slot(&c);
  CHECK(slot < 60, "rejoined at slot %lu", slot);
  c16_t re[fp.N_RB_UL * 12];
  memset(re, 0, sizeof(re));
  c16_t *data[1] = {re};
  split7_client_tx_symbol(&c, slot * fp.symbols_per_slot, data, 1);
  CHECK(split7_client_stats(&c).tx_symbols == 1, "post after rejoin discarded as given up");
  fake_stop();
  split7_client_close(&c);
  split7_engine_destroy(e);
  printf("rejoin after giving slots up: the new membership posts its slots\n");
}

static void test_prach_released(void)
{
  fake_reset(3 * fp.samples_per_frame);
  split7_engine_t *e = make_engine();
  split7_engine_set_anchor(e, 0, 0, NULL);
  split7_port_t *port = split7_engine_port(e);
  split7_client_t a;
  split7_client_open(port, &a);
  split7_client_join(&a, 8, 0);
  CHECK(wait_state(&a, SPLIT7_CLIENT_MEMBER), "join");
  c16_t bins[1024] __attribute__((aligned(64))) = {0};
  for (int s = 8; s < 10; s++)
    split7_client_slot_done(&a, s, 0, -1, 0);
  // A PRACH is posted for each of four slots, which are then given up before their records.
  for (int i = 0; i < SPLIT7_MAX_PRACH; i++) {
    CHECK(split7_client_tx_prach(&a, 10 + i, 0, 64, 1024, 1, bins) >= 0, "PRACH entry %d", i);
    split7_client_drop_through(&a, 10 + i);
  }
  for (uint64_t s = 14; s < 40; s++)
    split7_client_slot_done(&a, s, 0, -1, 0);
  run_clock_to(slot_ts(44));
  CHECK(wait_tx_until(e, slot_ts(38)), "engine stopped");
  CHECK(split7_client_tx_prach(&a, 41, 0, 64, 1024, 1, bins) >= 0, "PRACH entries of given-up slots never released");
  fake_stop();
  split7_client_close(&a);
  split7_engine_destroy(e);
  printf("PRACH entries of given-up slots released\n");
}

static void test_dl_publish_and_steering(void)
{
  const int fft = fp.ofdm_symbol_size;
  const int n_re = fp.N_RB_DL * 12;
  const int64_t len = 2 * fp.samples_per_frame;
  c16_t *signal = calloc(len + 2 * fft, sizeof(c16_t));
  c16_t scratch[fft] __attribute__((aligned(64)));
  unsigned seed = 6;
  for (int slot = 0; slot < 2 * fp.slots_per_frame; slot++) {
    int64_t off = slot_ts(slot);
    for (int s = 0; s < fp.symbols_per_slot; s++) {
      c16_t x[fft];
      fill_random(x, n_re, &seed);
      split7_ofdm_mod(&fp, link_type_dl, fp.N_RB_DL, slot % fp.slots_per_frame, s, x, scratch, signal + off);
      off += split7_symbol_samples(&fp, slot % fp.slots_per_frame, s);
    }
  }
  fake_reset(len);
  fake.rx_signal = signal;
  fake.rx_signal_len = len;
  split7_engine_t *e = make_engine();
  split7_engine_set_anchor(e, 0, 0, NULL);
  split7_client_t a;
  split7_client_open(split7_engine_port(e), &a);

  run_clock_to(slot_ts(8));
  CHECK(split7_client_wait_slot(&a, 3, 1000000) == 0, "slot 3 not published");
  c16_t out[fft] __attribute__((aligned(64)));
  for (int s = 0; s < fp.symbols_per_slot; s++) {
    split7_ofdm_demod(&fp, link_type_dl, fp.N_RB_DL, 3, s, signal + slot_ts(3) + get_samples_symbol_timestamp(&fp, 3, s), out);
    CHECK(memcmp(out, a.rx_ring[0] + (3 * fp.symbols_per_slot + s) * fft, n_re * sizeof(c16_t)) == 0,
          "DL symbol %d of slot 3 differs",
          s);
  }

  // Timing steering: only the first member is listened to; the shift starts after
  // every slot already completed, at the same slot for RX and TX.
  split7_client_t b;
  split7_client_open(split7_engine_port(e), &b);
  split7_client_join(&a, 14, 0);
  CHECK(wait_state(&a, SPLIT7_CLIENT_MEMBER), "join a");
  split7_client_join(&b, 14, 0);
  CHECK(wait_state(&b, SPLIT7_CLIENT_MEMBER), "join b");
  CHECK(split7_client_is_timing_master(&a) && !split7_client_is_timing_master(&b), "first member is the timing master");
  for (int s = 14; s <= 18; s++) {
    split7_client_slot_done(&a, s, 0, -1, 0);
    split7_client_slot_done(&b, s, 0, -1, 0);
  }
  usleep(5000);
  split7_client_adjust_timing(&b, 50);
  split7_client_adjust_timing(&a, 7);
  run_clock_to(slot_ts(12));
  CHECK(split7_client_wait_slot(&a, 10, 1000000) == 0, "slot 10 not published");
  pthread_mutex_lock(&e->tb_mutex);
  CHECK(e->tb.num_shifts == 1 && e->tb.shift[0].samples == 7 && e->tb.shift[0].from_slot >= 20,
        "shift %d: %ld samples from slot %lu, expected 7 from slot >= 20",
        e->tb.num_shifts,
        e->tb.shift[0].samples,
        e->tb.shift[0].from_slot);
  pthread_mutex_unlock(&e->tb_mutex);

  fake_stop();
  split7_client_close(&a);
  split7_client_close(&b);
  split7_engine_destroy(e);
  free(signal);
  printf("DL published == demod of the received signal; timing steered by the master after completed slots\n");
}

static void test_rx_symbol_overwritten(void)
{
  const int fft = fp.ofdm_symbol_size;
  fake_reset(0);
  split7_engine_t *e = make_engine();
  split7_engine_set_anchor(e, 0, 0, NULL);
  split7_client_t a;
  split7_client_open(split7_engine_port(e), &a);
  const uint64_t ring_slots = split7_engine_port(e)->mem->ring_slots;
  c16_t out[fft] __attribute__((aligned(64)));
  c16_t *dst[1] = {out};

  run_clock_to(slot_ts(8));
  CHECK(split7_client_wait_slot(&a, 3, 1000000) == 0, "slot 3 not published");
  CHECK(split7_client_rx_symbol(&a, 3 * fp.symbols_per_slot + 2, dst, 1), "published symbol refused");
  CHECK(!split7_client_rx_symbol(&a, 9 * fp.symbols_per_slot, dst, 1), "unpublished symbol read");
  // The host reaches slot 3's ring position again: its symbols are gone.
  run_clock_to(slot_ts(3 + ring_slots + 2));
  CHECK(split7_client_wait_slot(&a, 3 + ring_slots, 1000000) == 0, "slot 3 + ring not published");
  CHECK(!split7_client_rx_symbol(&a, 3 * fp.symbols_per_slot + 2, dst, 1), "overwritten symbol read");

  fake_stop();
  split7_client_close(&a);
  split7_engine_destroy(e);
  printf("RX symbol reads refuse unpublished and overwritten symbols\n");
}

int main(int argc, char **argv)
{
  logInit();
  set_glog(OAILOG_WARNING);
  load_dftslib();
  init_fp();

  test_mod_matches_monolithic();
  test_demod_matches_monolithic();
  test_ul_combining();
  test_group_sum();
  test_leader();
  test_late_member();
  test_determinism();
  test_bursts();
  test_ta_change();
  test_prach();
  test_join_leave();
  test_backpressure();
  test_long_skip();
  test_drop_releases_backpressure();
  test_skipped_first_slot_ta();
  test_shift_after_member_left();
  test_leave_while_joining();
  test_close_reopen();
  test_rejoin_after_leave();
  test_rejoin_after_drop();
  test_prach_released();
  test_dl_publish_and_steering();
  test_rx_symbol_overwritten();

  if (failures) {
    printf("%d check(s) FAILED\n", failures);
    return 1;
  }
  printf("all split7 tests passed\n");
  return 0;
}
