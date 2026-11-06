/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * Split 7.1 Low-PHY tests: OFDM core against the monolithic UE paths, and the
 * device against a fake radio with a test-driven sample clock: UL combining of a
 * group, each member at its own Timing Advance, deterministic finalization (late member,
 * random interleaving), TA-increase clipping, PRACH, join/leave, backpressure, DL
 * publishing and timing steering, lockstep TX.
 */

#include "gtest/gtest.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>
#include <malloc.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

extern "C" {
#include "split7_device.h"
#include "split7_device_internal.h"
#include "split7_ofdm.h"
}
#include "split7_test_ref.h"

namespace {

NR_DL_FRAME_PARMS fp;

struct Free {
  void operator()(void *p) const
  {
    free(p);
  }
};
using Buf = std::unique_ptr<c16_t[], Free>;

// n zeroed samples, 64-byte aligned for the DFTs.
Buf buf(size_t n)
{
  c16_t *p = static_cast<c16_t *>(memalign(64, n * sizeof(c16_t)));
  memset(p, 0, n * sizeof(c16_t));
  return Buf(p);
}

void fill_random(c16_t *x, int n, unsigned *seed)
{
  for (int i = 0; i < n; i++)
    x[i] = c16_t{static_cast<int16_t>(rand_r(seed) % 4001 - 2000), static_cast<int16_t>(rand_r(seed) % 4001 - 2000)};
}

/* ------------------------------------------------------------------------- */
/* OFDM core vs monolithic                                                   */
/* ------------------------------------------------------------------------- */

TEST(Split7Ofdm, ModMatchesMonolithic)
{
  const int fft = fp.ofdm_symbol_size;
  const int n_re = fp.N_RB_UL * 12;
  unsigned seed = 1;
  for (int slot = 0; slot < 3; slot++) {
    Buf orig = buf(fp.symbols_per_slot * fft);
    Buf ref = buf(fp.samples_per_subframe);
    for (int s = 0; s < fp.symbols_per_slot; s++)
      fill_random(orig.get() + s * fft, n_re, &seed);
    split7_test_ref_mod(&fp, slot, orig.get(), ref.get());

    Buf scratch = buf(fft);
    Buf out = buf(fft + fp.nb_prefix_samples0);
    int off = 0;
    for (int s = 0; s < fp.symbols_per_slot; s++) {
      const int len = split7_symbol_samples(&fp, slot, s);
      split7_ofdm_mod(&fp, link_type_ul, fp.N_RB_UL, slot, s, orig.get() + s * fft, scratch.get(), out.get());
      EXPECT_EQ(memcmp(out.get(), ref.get() + off, len * sizeof(c16_t)), 0) << "slot " << slot << " symbol " << s;
      off += len;
    }
  }
}

TEST(Split7Ofdm, DemodMatchesMonolithic)
{
  const int fft = fp.ofdm_symbol_size;
  const int n_re = fp.N_RB_DL * 12;
  unsigned seed = 2;
  // Two frames of time-domain signal, as nr_slot_fep() expects in rxdata.
  Buf rxdata = buf(2 * fp.samples_per_frame);
  Buf scratch = buf(fft);
  Buf x = buf(fft);
  for (int slot = 0; slot < 3; slot++) {
    int off = get_samples_slot_timestamp(&fp, slot);
    for (int s = 0; s < fp.symbols_per_slot; s++) {
      fill_random(x.get(), n_re, &seed);
      split7_ofdm_mod(&fp, link_type_dl, fp.N_RB_DL, slot, s, x.get(), scratch.get(), rxdata.get() + off);
      off += split7_symbol_samples(&fp, slot, s);
    }
  }
  Buf ref = buf(fft);
  Buf out = buf(fft);
  for (int slot = 0; slot < 3; slot++) {
    for (int s = 0; s < fp.symbols_per_slot; s++) {
      split7_test_ref_demod(&fp, slot, s, rxdata.get(), ref.get());
      const int start = get_samples_slot_timestamp(&fp, slot) + get_samples_symbol_timestamp(&fp, slot, s);
      split7_ofdm_demod(&fp, link_type_dl, fp.N_RB_DL, slot, s, rxdata.get() + start, out.get());
      EXPECT_EQ(memcmp(out.get(), ref.get(), n_re * sizeof(c16_t)), 0) << "slot " << slot << " symbol " << s;
    }
  }
}

/* ------------------------------------------------------------------------- */
/* Fake radio with a test-driven clock                                       */
/* ------------------------------------------------------------------------- */

struct fake_write_t {
  int64_t ts;
  int n;
  int flags;
};

struct fake_radio_t {
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
  int64_t clock; // next sample to deliver
  int64_t limit; // the test lets the clock run up to here
  bool stop;
  const c16_t *rx_signal; // optional, indexed by timestamp
  int64_t rx_signal_len;
  Buf tx_capture; // indexed by timestamp
  int64_t tx_capture_len;
  int64_t tx_written_until;
  int64_t min_tx_budget; // smallest (write timestamp - clock) seen
  int late_writes; // writes vrtsim would drop as too late (start before its clock)
  // > 0: lockstep radio (rfsimulator): no real-time pacing, a read waits until TX
  // covers its end minus this (the peer's own TX advance)
  int64_t lockstep_lead;
  int gaps; // writes not starting where the previous one ended
  std::vector<fake_write_t> writes;
  struct timespec t0; // the clock runs at the sampling rate from here
};

fake_radio_t fake;

int fake_start(openair0_device_t *dev)
{
  return 0;
}

int fake_read(openair0_device_t *dev, openair0_timestamp_t *ts, void **buff, int nsamps, int nbAnt)
{
  pthread_mutex_lock(&fake.mutex);
  while (!fake.stop
         && (fake.clock + nsamps > fake.limit
             || (fake.lockstep_lead && fake.tx_written_until < fake.clock + nsamps - fake.lockstep_lead)))
    pthread_cond_wait(&fake.cond, &fake.mutex);
  if (fake.stop) {
    pthread_mutex_unlock(&fake.mutex);
    usleep(1000);
    return -1;
  }
  if (!fake.lockstep_lead) {
    // Real-time pacing, so the device's deadlines mean something.
    pthread_mutex_unlock(&fake.mutex);
    const int64_t ns = (fake.clock + nsamps) * 1000000LL / fp.samples_per_subframe;
    struct timespec due = {fake.t0.tv_sec + ns / 1000000000, fake.t0.tv_nsec + ns % 1000000000};
    if (due.tv_nsec >= 1000000000) {
      due.tv_sec++;
      due.tv_nsec -= 1000000000;
    }
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &due, NULL);
    pthread_mutex_lock(&fake.mutex);
  }
  *ts = fake.clock;
  for (int a = 0; a < nbAnt; a++) {
    c16_t *dst = static_cast<c16_t *>(buff[a]);
    for (int i = 0; i < nsamps; i++)
      dst[i] = fake.rx_signal && fake.clock + i < fake.rx_signal_len ? fake.rx_signal[fake.clock + i] : c16_t{};
  }
  fake.clock += nsamps;
  pthread_cond_broadcast(&fake.cond);
  pthread_mutex_unlock(&fake.mutex);
  return nsamps;
}

int fake_write(openair0_device_t *dev, openair0_timestamp_t ts, void **buff, int nsamps, int nbAnt, int flags)
{
  pthread_mutex_lock(&fake.mutex);
  const int64_t budget = ts - fake.clock;
  if (budget < fake.min_tx_budget)
    fake.min_tx_budget = budget;
  if (budget < 0)
    fake.late_writes++;
  if (!fake.writes.empty() && (int64_t)ts != fake.writes.back().ts + fake.writes.back().n)
    fake.gaps++;
  fake.writes.push_back({(int64_t)ts, nsamps, flags});
  const c16_t *src = static_cast<const c16_t *>(buff[0]);
  for (int i = 0; i < nsamps; i++)
    if ((int64_t)ts + i >= 0 && (int64_t)ts + i < fake.tx_capture_len)
      fake.tx_capture[ts + i] = src[i];
  if ((int64_t)ts + nsamps > fake.tx_written_until)
    fake.tx_written_until = ts + nsamps;
  pthread_cond_broadcast(&fake.cond);
  pthread_mutex_unlock(&fake.mutex);
  return nsamps;
}

openair0_device_t fake_dev = [] {
  openair0_device_t d{};
  d.trx_start_func = fake_start;
  d.trx_read_func = fake_read;
  d.trx_write_func = fake_write;
  return d;
}();

void fake_reset(int64_t capture_len)
{
  fake.clock = 0;
  fake.limit = 0;
  fake.stop = false;
  fake.rx_signal = NULL;
  fake.rx_signal_len = 0;
  fake.tx_capture = buf(capture_len);
  fake.tx_capture_len = capture_len;
  fake.tx_written_until = 0;
  fake.min_tx_budget = INT64_MAX;
  fake.late_writes = 0;
  fake.lockstep_lead = 0;
  fake.gaps = 0;
  fake.writes.clear();
  fake.writes.reserve(4096);
  clock_gettime(CLOCK_MONOTONIC, &fake.t0);
}

// Lets the radio clock run to ts, waiting until the device has read it.
void run_clock_to(int64_t ts)
{
  pthread_mutex_lock(&fake.mutex);
  fake.limit = ts;
  pthread_cond_broadcast(&fake.cond);
  while (fake.clock + (int64_t)fp.ofdm_symbol_size <= ts)
    pthread_cond_wait(&fake.cond, &fake.mutex);
  pthread_mutex_unlock(&fake.mutex);
}

// Lets the radio clock run to ts; false if the device has not read it within 2 s.
bool run_clock_to_bounded(int64_t ts)
{
  struct timespec deadline;
  clock_gettime(CLOCK_REALTIME, &deadline);
  deadline.tv_sec += 2;
  pthread_mutex_lock(&fake.mutex);
  fake.limit = ts;
  pthread_cond_broadcast(&fake.cond);
  int rc = 0;
  while (rc == 0 && fake.clock + (int64_t)fp.ofdm_symbol_size <= ts)
    rc = pthread_cond_timedwait(&fake.cond, &fake.mutex, &deadline);
  const bool reached = fake.clock + (int64_t)fp.ofdm_symbol_size > ts;
  pthread_mutex_unlock(&fake.mutex);
  return reached;
}

// Waits (bounded) until the device has transmitted (or passed over silence) up to ts.
bool wait_tx_until(split7_device_t *dev, int64_t ts)
{
  return split7_progress_wait(&dev->tx_sent, ts, 2000000) == SPLIT7_REACHED;
}

void fake_stop()
{
  pthread_mutex_lock(&fake.mutex);
  fake.stop = true;
  pthread_cond_broadcast(&fake.cond);
  pthread_mutex_unlock(&fake.mutex);
}

int64_t fake_clock()
{
  pthread_mutex_lock(&fake.mutex);
  const int64_t clock = fake.clock;
  pthread_mutex_unlock(&fake.mutex);
  return clock;
}

int64_t fake_written_until()
{
  pthread_mutex_lock(&fake.mutex);
  const int64_t written = fake.tx_written_until;
  pthread_mutex_unlock(&fake.mutex);
  return written;
}

split7_device_t *make_device(bool lockstep = false)
{
  split7_device_cfg_t cfg{};
  cfg.fp = &fp;
  cfg.rx_link = link_type_dl;
  cfg.tx_link = link_type_ul;
  cfg.radio = &fake_dev;
  cfg.lockstep = lockstep;
  cfg.max_clients = 8;
  split7_device_t *dev = split7_device_create(&cfg);
  AssertFatal(split7_device_start(dev) == 0, "start\n");
  return dev;
}

// Lockstep TX is one continuous stream: START once, then MIDDLE, without gaps.
void expect_stream()
{
  ASSERT_FALSE(fake.writes.empty()) << "nothing written";
  EXPECT_EQ(fake.gaps, 0) << "gaps in the TX stream";
  EXPECT_EQ(fake.writes[0].flags, TX_BURST_START) << "the stream does not start a burst";
  int bad = 0;
  for (size_t i = 1; i < fake.writes.size(); i++)
    bad += fake.writes[i].flags != TX_BURST_MIDDLE;
  EXPECT_EQ(bad, 0) << "writes after the first are not TX_BURST_MIDDLE";
}

int64_t slot_ts(uint64_t abs_slot)
{
  const split7_timebase_t tb = {0, 0};
  return split7_symbol_ts(&tb, &fp, abs_slot * fp.symbols_per_slot);
}

int64_t symbol_ts(uint64_t abs_sym)
{
  const split7_timebase_t tb = {0, 0};
  return split7_symbol_ts(&tb, &fp, abs_sym);
}

int16_t sat16(int32_t x)
{
  return x > 32767 ? 32767 : (x < -32768 ? -32768 : x);
}

// Expected air signal of one symbol: modulated, then placed TA samples early,
// samples before from dropped.
void add_expected_from(c16_t *expected, int64_t len, uint64_t abs_sym, int ta, const c16_t *re, int64_t from)
{
  const int slot = (abs_sym / fp.symbols_per_slot) % fp.slots_per_frame;
  const int sym = abs_sym % fp.symbols_per_slot;
  const int n = split7_symbol_samples(&fp, slot, sym);
  Buf scratch = buf(fp.ofdm_symbol_size);
  Buf out = buf(n);
  split7_ofdm_mod(&fp, link_type_ul, fp.N_RB_UL, slot, sym, re, scratch.get(), out.get());
  const int64_t start = symbol_ts(abs_sym) - ta;
  for (int i = 0; i < n; i++) {
    if (start + i < 0 || start + i >= len || start + i < from)
      continue;
    c16_t &x = expected[start + i];
    x = c16_t{sat16(x.r + out[i].r), sat16(x.i + out[i].i)};
  }
}

int max_abs_diff(const c16_t *a, const c16_t *b, int64_t from, int64_t to)
{
  int m = 0;
  for (int64_t i = from; i < to; i++) {
    m = std::max(m, abs(a[i].r - b[i].r));
    m = std::max(m, abs(a[i].i - b[i].i));
  }
  return m;
}

// Signal-to-difference ratio of a against reference b, in dB.
double sdr_db(const c16_t *a, const c16_t *b, int64_t from, int64_t to)
{
  double sig = 0, err = 0;
  for (int64_t i = from; i < to; i++) {
    sig += (double)b[i].r * b[i].r + (double)b[i].i * b[i].i;
    err += (double)(a[i].r - b[i].r) * (a[i].r - b[i].r) + (double)(a[i].i - b[i].i) * (a[i].i - b[i].i);
  }
  return err ? 10 * log10(sig / err) : 999;
}

int count_diff(const c16_t *a, const c16_t *b, int64_t from, int64_t to)
{
  int diff = 0;
  for (int64_t i = from; i < to; i++)
    diff += a[i].r != b[i].r || a[i].i != b[i].i;
  return diff;
}

int count_diff(const Buf &a, const Buf &b, int64_t from, int64_t to)
{
  return count_diff(a.get(), b.get(), from, to);
}

/* ------------------------------------------------------------------------- */
/* Clients and scripts                                                       */
/* ------------------------------------------------------------------------- */

// Deterministic REs of symbol k as posted by the client tagged tag.
void make_re(c16_t *re, uint64_t k, int tag)
{
  unsigned seed = k * 7919 + tag;
  fill_random(re, fp.N_RB_UL * 12, &seed);
}

void post(split7_client_t *c, uint64_t k, int tag)
{
  std::vector<c16_t> re(fp.N_RB_UL * 12);
  make_re(re.data(), k, tag);
  c16_t *data[1] = {re.data()};
  split7_client_tx_symbol(c, k, data, 1);
}

split7_client_state_t client_state(split7_client_t *c)
{
  split7_monitor_lock(&c->mem->monitor);
  const split7_client_state_t state = c->mem->state;
  split7_monitor_unlock(&c->mem->monitor);
  return state;
}

uint64_t client_join_slot(split7_client_t *c)
{
  split7_monitor_lock(&c->mem->monitor);
  const uint64_t slot = c->mem->join_slot;
  split7_monitor_unlock(&c->mem->monitor);
  return slot;
}

bool record_written(split7_client_t *c, uint64_t slot)
{
  split7_monitor_lock(&c->mem->monitor);
  const bool written = split7_port_record(c->port, c->id, slot)->tag == slot + 1;
  split7_monitor_unlock(&c->mem->monitor);
  return written;
}

uint64_t prach_tag(split7_client_t *c, int i)
{
  split7_monitor_lock(&c->mem->monitor);
  const uint64_t tag = split7_port_prach_entry(c->port, c->id, i)->tag;
  split7_monitor_unlock(&c->mem->monitor);
  return tag;
}

bool wait_state(split7_client_t *c, split7_client_state_t state)
{
  for (int i = 0; i < 2000; i++) {
    if (client_state(c) == state)
      return true;
    usleep(1000);
  }
  return false;
}

// A member's script: symbols posted per slot (bit masks), TA per slot.
struct script_t {
  split7_client_t c;
  int tag;
  uint32_t used[128];
  int ta[128];
};

void script_init(script_t *sc, split7_port_t *port, int tag, int ta)
{
  *sc = script_t{};
  EXPECT_EQ(split7_client_open(port, &sc->c), 0) << "open client";
  sc->tag = tag;
  for (int i = 0; i < 128; i++)
    sc->ta[i] = ta;
}

// Posts and completes the given slots, in that order.
void script_run(script_t *sc, const uint64_t *order, int n)
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

void script_run_range(script_t *sc, uint64_t first, uint64_t last)
{
  for (uint64_t s = first; s <= last; s++)
    script_run(sc, &s, 1);
}

// One member's script over the slots it is a member for.
struct part_t {
  const script_t *sc;
  uint64_t first, last;
};

// Expected air signal of the group: each member at its own TA. The group starts at
// its first member's TA; a slot is final up to its end at the largest TA of its
// members, and a later slot placed before that (a TA increase) loses its first samples.
void add_group_expected(Buf &expected, int64_t len, const std::vector<part_t> &p)
{
  const int sps = fp.symbols_per_slot;
  uint64_t first = UINT64_MAX, last = 0;
  for (const part_t &i : p) {
    first = std::min(first, i.first);
    last = std::max(last, i.last);
  }
  int32_t ta_max = p[0].sc->ta[p[0].first];
  int64_t reach = slot_ts(first) - ta_max;
  std::vector<c16_t> re(fp.N_RB_UL * 12);
  for (uint64_t slot = first; slot <= last; slot++) {
    int32_t m = -1;
    for (const part_t &i : p)
      if (slot >= i.first && slot <= i.last && i.sc->ta[slot] > m)
        m = i.sc->ta[slot];
    if (m >= 0)
      ta_max = m;
    const int64_t clip = reach;
    for (const part_t &i : p) {
      if (slot < i.first || slot > i.last)
        continue;
      for (int sym = 0; sym < sps; sym++)
        if (i.sc->used[slot] & (1u << sym)) {
          make_re(re.data(), slot * sps + sym, i.sc->tag);
          add_expected_from(expected.get(), len, slot * sps + sym, i.sc->ta[slot], re.data(), clip);
        }
    }
    reach = std::max(reach, slot_ts(slot + 1) - ta_max);
  }
}

Buf group_expected(const std::vector<part_t> &p, int64_t len)
{
  Buf expected = buf(len);
  add_group_expected(expected, len, p);
  return expected;
}

// Runs fn(arg) on its own thread.
template <typename F>
pthread_t spawn(F *fn)
{
  pthread_t t;
  pthread_create(
      &t,
      NULL,
      [](void *f) -> void * {
        (*static_cast<F *>(f))();
        return NULL;
      },
      fn);
  return t;
}

// Every device test starts from a fresh fake radio and a device anchored at 0.
class Split7Device : public ::testing::Test {
 protected:
  split7_device_t *dev = nullptr;
  split7_port_t *port = nullptr;

  // lockstep_lead > 0: a lockstep radio whose peer leads by that many samples.
  void start(int64_t capture_len, int64_t lockstep_lead = 0, bool anchored = true)
  {
    fake_reset(capture_len);
    fake.lockstep_lead = lockstep_lead;
    dev = make_device(lockstep_lead > 0);
    if (anchored)
      anchor();
    port = split7_device_port(dev);
  }

  void anchor()
  {
    split7_device_set_anchor(dev, 0, 0, NULL);
  }

  void TearDown() override
  {
    fake_stop();
  }
};

/* ------------------------------------------------------------------------- */
/* Device tests                                                              */
/* ------------------------------------------------------------------------- */

// 2 members, each at its own TA, only recorded symbols sent.
TEST_F(Split7Device, UlCombining)
{
  const int64_t len = 3 * fp.samples_per_frame;
  start(len);
  script_t a, b;
  script_init(&a, port, 1, 0);
  script_init(&b, port, 2, 777);
  split7_client_join(&a.c, 8, 0);
  split7_client_join(&b.c, 8, 777);
  ASSERT_TRUE(wait_state(&a.c, SPLIT7_CLIENT_MEMBER) && wait_state(&b.c, SPLIT7_CLIENT_MEMBER));
  EXPECT_EQ(client_join_slot(&a.c), 8u);
  EXPECT_EQ(client_join_slot(&b.c), 8u);
  a.used[10] = 0xc;
  a.used[30] = 0x1;
  a.used[12] = 0x8;
  b.used[10] = 0x20;
  b.used[29] = 0x2000;
  script_run_range(&a, 8, 44);
  script_run_range(&b, 8, 44);
  // Posted after its slot was completed and not listed in the record: never sent.
  post(&a.c, 40 * fp.symbols_per_slot + 3, 9);

  Buf expected = group_expected({{&a, 8, 44}, {&b, 8, 44}}, len);
  run_clock_to(slot_ts(48));
  EXPECT_TRUE(wait_tx_until(dev, slot_ts(42)));
  EXPECT_EQ(count_diff(fake.tx_capture, expected, slot_ts(7), slot_ts(42)), 0);

  fake_stop();
  split7_client_close(&a.c);
  split7_client_close(&b.c);
  split7_device_destroy(dev);
}

// One IDFT per symbol and TA.
TEST_F(Split7Device, GroupSum)
{
  const int64_t len = 2 * fp.samples_per_frame;
  start(len);
  script_t a, b, c;
  script_init(&a, port, 1, 600);
  script_init(&b, port, 2, 600);
  script_init(&c, port, 3, 650); // another TA: modulated on its own
  split7_client_join(&a.c, 8, 600);
  split7_client_join(&b.c, 8, 600);
  split7_client_join(&c.c, 8, 650);
  ASSERT_TRUE(wait_state(&a.c, SPLIT7_CLIENT_MEMBER) && wait_state(&b.c, SPLIT7_CLIENT_MEMBER)
              && wait_state(&c.c, SPLIT7_CLIENT_MEMBER));
  // Symbol 10.4: A and B summed, one IDFT, C at its TA, another; symbol 11.1: A alone.
  a.used[10] = b.used[10] = c.used[10] = 1 << 4;
  a.used[11] = 1 << 1;
  script_run_range(&a, 8, 30);
  script_run_range(&b, 8, 30);
  script_run_range(&c, 8, 30);
  Buf expected = group_expected({{&a, 8, 30}, {&b, 8, 30}, {&c, 8, 30}}, len);
  run_clock_to(slot_ts(20));
  EXPECT_TRUE(wait_tx_until(dev, slot_ts(14)));

  EXPECT_EQ(split7_device_stats(dev).tx_idft, 3u);
  // Summing before the fixed-point rotation/IDFT rounds differently than summing after.
  const int64_t s1 = symbol_ts(10 * fp.symbols_per_slot + 4) - 600;
  const int s1_len = split7_symbol_samples(&fp, 10, 4);
  EXPECT_GT(sdr_db(fake.tx_capture.get(), expected.get(), s1, s1 + s1_len), 50)
      << "max " << max_abs_diff(fake.tx_capture.get(), expected.get(), s1, s1 + s1_len) << " LSB";
  const int64_t s2 = symbol_ts(11 * fp.symbols_per_slot + 1) - 600;
  EXPECT_EQ(count_diff(fake.tx_capture, expected, s2, s2 + split7_symbol_samples(&fp, 11, 1)), 0)
      << "single-member symbol not bit-exact";

  fake_stop();
  split7_client_close(&a.c);
  split7_client_close(&b.c);
  split7_client_close(&c.c);
  split7_device_destroy(dev);
}

// Each member at its own TA; timing steered by the lowest id, then the next when it leaves.
TEST_F(Split7Device, MemberTa)
{
  const int64_t len = 2 * fp.samples_per_frame;
  start(len);
  script_t a, b;
  script_init(&a, port, 1, 100);
  script_init(&b, port, 2, 900);
  split7_client_join(&a.c, 8, 100);
  split7_client_join(&b.c, 8, 900);
  ASSERT_TRUE(wait_state(&a.c, SPLIT7_CLIENT_MEMBER) && wait_state(&b.c, SPLIT7_CLIENT_MEMBER));
  EXPECT_TRUE(split7_client_is_timing_master(&a.c));
  EXPECT_FALSE(split7_client_is_timing_master(&b.c));
  a.used[10] = 1 << 2;
  b.used[12] = 1 << 5;
  b.used[26] = 1 << 3;
  script_run_range(&a, 8, 20);
  script_run_range(&b, 8, 40);
  run_clock_to(slot_ts(12));
  split7_client_leave(&a.c, 20, 0); // request only: confirmed once slot 20 is sent
  run_clock_to(slot_ts(44));
  EXPECT_TRUE(wait_state(&a.c, SPLIT7_CLIENT_OPEN)) << "leave";
  EXPECT_TRUE(split7_client_is_timing_master(&b.c)) << "the next member does not steer timing after the first left";
  EXPECT_TRUE(wait_tx_until(dev, slot_ts(38)));
  // Each at its own TA, before and after A left.
  Buf expected = group_expected({{&a, 8, 20}, {&b, 8, 40}}, len);
  EXPECT_EQ(count_diff(fake.tx_capture, expected, slot_ts(7), slot_ts(38)), 0);

  fake_stop();
  split7_client_close(&a.c);
  split7_client_close(&b.c);
  split7_device_destroy(dev);
}

// Everyone late, nothing sent early, content unchanged.
TEST_F(Split7Device, LateMember)
{
  const int64_t len = 3 * fp.samples_per_frame;
  start(len);
  script_t a, b;
  script_init(&a, port, 1, 0);
  script_init(&b, port, 2, 300);
  split7_client_join(&a.c, 8, 0);
  split7_client_join(&b.c, 8, 300);
  ASSERT_TRUE(wait_state(&a.c, SPLIT7_CLIENT_MEMBER) && wait_state(&b.c, SPLIT7_CLIENT_MEMBER));
  for (int s = 8; s <= 40; s++) {
    a.used[s] = 0x0f0f;
    b.used[s] = 0x3000;
  }
  script_run_range(&a, 8, 40);
  // B is silent while the radio runs well past its first slots: nothing may go out.
  run_clock_to(slot_ts(22));
  usleep(5000);
  EXPECT_LE(fake_written_until(), slot_ts(8)) << "transmitted before the late member completed";
  script_run_range(&b, 8, 40);
  run_clock_to(slot_ts(44));
  EXPECT_TRUE(wait_tx_until(dev, slot_ts(40)));

  Buf expected = group_expected({{&a, 8, 40}, {&b, 8, 40}}, len);
  EXPECT_EQ(count_diff(fake.tx_capture, expected, slot_ts(7), slot_ts(40)), 0) << "content must not depend on timing";
  EXPECT_GT(fake.late_writes, 0);
  EXPECT_GT(split7_device_stats(dev).tx_late_writes, 0u);
  EXPECT_GT(split7_client_stats(&b.c).late_stretches, 0u) << "late stretches blamed on the wrong member";
  EXPECT_EQ(split7_client_stats(&a.c).late_stretches, 0u) << "late stretches blamed on the wrong member";

  fake_stop();
  split7_client_close(&a.c);
  split7_client_close(&b.c);
  split7_device_destroy(dev);
}

// Completes slots 8..40 out of order (pairs swapped) with random pauses.
void run_jittered(script_t *sc, unsigned seed)
{
  uint64_t order[33];
  for (int i = 0; i < 33; i++)
    order[i] = 8 + i;
  for (int i = 0; i + 1 < 33; i += 2)
    if (rand_r(&seed) & 1)
      std::swap(order[i], order[i + 1]);
  for (int i = 0; i < 33; i++) {
    usleep(rand_r(&seed) % 400);
    script_run(sc, &order[i], 1);
  }
}

// 3 runs, random interleaving and out-of-order completion, identical output.
TEST_F(Split7Device, Determinism)
{
  const int64_t len = 2 * fp.samples_per_frame;
  Buf first;
  for (int run = 0; run < 3; run++) {
    start(len);
    script_t a, b;
    script_init(&a, port, 1, 600);
    script_init(&b, port, 2, 600);
    split7_client_join(&a.c, 8, 600);
    split7_client_join(&b.c, 8, 600);
    ASSERT_TRUE(wait_state(&a.c, SPLIT7_CLIENT_MEMBER) && wait_state(&b.c, SPLIT7_CLIENT_MEMBER));
    for (int s = 8; s <= 40; s++) {
      a.used[s] = 0x0ff0;
      b.used[s] = 0x00ff;
      a.ta[s] = b.ta[s] = 600 + (s >= 20 ? 24 : 0); // TA command at slot 20: clipped
    }
    auto ja = [&] { run_jittered(&a, 100 + run); };
    auto jb = [&] { run_jittered(&b, 200 + run); };
    pthread_t ta = spawn(&ja), tb = spawn(&jb);
    for (int s = 10; s <= 44; s += 2)
      run_clock_to(slot_ts(s));
    pthread_join(ta, NULL);
    pthread_join(tb, NULL);
    run_clock_to(slot_ts(46));
    EXPECT_TRUE(wait_tx_until(dev, slot_ts(40)));
    if (!first) {
      first = buf(len);
      memcpy(first.get(), fake.tx_capture.get(), len * sizeof(c16_t));
    } else {
      EXPECT_EQ(count_diff(fake.tx_capture, first, 0, slot_ts(40)), 0) << "run " << run << " differs from run 0";
    }
    fake_stop();
    split7_client_close(&a.c);
    split7_client_close(&b.c);
    split7_device_destroy(dev);
  }
}

// Only content is written, one START..END burst per run of symbols.
TEST_F(Split7Device, Bursts)
{
  const int64_t len = 2 * fp.samples_per_frame;
  start(len);
  script_t a;
  script_init(&a, port, 1, 600);
  split7_client_join(&a.c, 8, 600);
  ASSERT_TRUE(wait_state(&a.c, SPLIT7_CLIENT_MEMBER));
  // Three runs of content: 10.2-4, 10.8, and 10.13 + 11.0 across the slot (stretch) boundary.
  a.used[10] = 0x7 << 2 | 1 << 8 | 1 << 13;
  a.used[11] = 1 << 0;
  script_run_range(&a, 8, 30);
  run_clock_to(slot_ts(20));
  EXPECT_TRUE(wait_tx_until(dev, slot_ts(14)));
  const int64_t sym = fp.symbols_per_slot;
  const struct {
    int64_t start, end;
  } runs[3] = {{symbol_ts(10 * sym + 2) - 600, symbol_ts(10 * sym + 5) - 600},
               {symbol_ts(10 * sym + 8) - 600, symbol_ts(10 * sym + 9) - 600},
               {symbol_ts(10 * sym + 13) - 600, symbol_ts(11 * sym + 1) - 600}};
  // Writes are exactly the runs, each one burst: START (or START_AND_END) ... END, contiguous.
  int r = 0;
  int64_t next = -1;
  bool in_burst = false;
  for (size_t i = 0; i < fake.writes.size(); i++) {
    const int64_t ts = fake.writes[i].ts, n = fake.writes[i].n;
    const int fl = fake.writes[i].flags;
    ASSERT_LT(r, 3) << "write " << i << " at " << ts << " beyond the three runs";
    if (!in_burst) {
      EXPECT_EQ(ts, runs[r].start) << "run " << r;
      EXPECT_TRUE(fl == TX_BURST_START || fl == TX_BURST_START_AND_END) << "run " << r << " does not start a burst";
      in_burst = true;
    } else {
      EXPECT_EQ(ts, next) << "run " << r << ": write " << i << " not contiguous";
      EXPECT_TRUE(fl == TX_BURST_MIDDLE || fl == TX_BURST_END) << "run " << r << ": write " << i << " bad flags";
    }
    next = ts + n;
    if (fl == TX_BURST_END || fl == TX_BURST_START_AND_END) {
      EXPECT_EQ(next, runs[r].end) << "run " << r;
      in_burst = false;
      r++;
    }
  }
  EXPECT_EQ(r, 3) << "complete bursts";
  EXPECT_FALSE(in_burst);
  // Every slot was complete early: each burst's last content write ended it.
  EXPECT_EQ(split7_device_stats(dev).burst_end_markers, 0u) << "bursts ended late by a separate write";
  Buf expected = group_expected({{&a, 8, 30}}, len);
  EXPECT_EQ(count_diff(fake.tx_capture, expected, 0, slot_ts(14)), 0);
  // The last member leaving: nothing bounds TX any more, its slots are still passed.
  split7_client_leave(&a.c, 30, 0);
  run_clock_to(slot_ts(36));
  EXPECT_TRUE(wait_state(&a.c, SPLIT7_CLIENT_OPEN)) << "the last member's leave was not confirmed";
  fake_stop();
  split7_client_close(&a.c);
  split7_device_destroy(dev);
}

// A TA increase drops the slot's first samples, a decrease leaves a gap of zeros.
TEST_F(Split7Device, TaChange)
{
  const int64_t len = 2 * fp.samples_per_frame;
  start(len);
  script_t a;
  script_init(&a, port, 1, 600);
  split7_client_join(&a.c, 8, 600);
  ASSERT_TRUE(wait_state(&a.c, SPLIT7_CLIENT_MEMBER));
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
  EXPECT_TRUE(wait_tx_until(dev, slot_ts(30)));
  Buf expected = group_expected({{&a, 8, 34}}, len);
  EXPECT_EQ(count_diff(fake.tx_capture, expected, slot_ts(7), slot_ts(30)), 0);
  EXPECT_EQ(split7_client_stats(&a.c).ta_clipped, 50u + 50 + 1000);
  fake_stop();
  split7_client_close(&a.c);
  split7_device_destroy(dev);
}

// PRACH placed at slot + offset - TA.
TEST_F(Split7Device, Prach)
{
  const int64_t len = 2 * fp.samples_per_frame;
  start(len);
  split7_client_t a;
  split7_client_open(port, &a);
  split7_client_join(&a, 8, 300);
  ASSERT_TRUE(wait_state(&a, SPLIT7_CLIENT_MEMBER));

  const int dftlen = 1536, ncp = 144, reps = 12, offset = 200;
  Buf bins = buf(dftlen);
  unsigned seed = 5;
  fill_random(bins.get(), dftlen, &seed);
  const int entry = split7_client_tx_prach(&a, 9, offset, ncp, dftlen, reps, bins.get());
  EXPECT_GE(entry, 0);
  for (int s = 8; s <= 30; s++)
    split7_client_slot_done(&a, s, 0, s == 9 ? entry : -1, 300);
  run_clock_to(slot_ts(24));
  EXPECT_TRUE(wait_tx_until(dev, slot_ts(18)));

  Buf scratch = buf(dftlen);
  Buf expected = buf(len);
  split7_ofdm_window_mod(bins.get(), dftlen, ncp, reps, scratch.get(), expected.get() + slot_ts(9) + offset - 300);
  EXPECT_EQ(count_diff(fake.tx_capture, expected, slot_ts(7), slot_ts(18)), 0);

  fake_stop();
  split7_client_close(&a);
  split7_device_destroy(dev);
}

// Leaving ends the wait, later posts are ignored, rejoining starts after the transmitted part.
TEST_F(Split7Device, JoinLeave)
{
  const int64_t len = 3 * fp.samples_per_frame;
  start(len);
  script_t a, c;
  script_init(&a, port, 1, 0);
  script_init(&c, port, 3, 0);
  split7_client_join(&a.c, 8, 0);
  split7_client_join(&c.c, 8, 0);
  ASSERT_TRUE(wait_state(&a.c, SPLIT7_CLIENT_MEMBER) && wait_state(&c.c, SPLIT7_CLIENT_MEMBER));
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
  EXPECT_TRUE(wait_state(&c.c, SPLIT7_CLIENT_OPEN)) << "leave not confirmed";
  EXPECT_TRUE(wait_tx_until(dev, slot_ts(44))) << "device waited for a member that left";

  Buf expected = group_expected({{&a, 8, 44}}, len);
  add_group_expected(expected, len, {{&c, 8, 13}});
  EXPECT_EQ(count_diff(fake.tx_capture, expected, slot_ts(7), slot_ts(44)), 0);
  // Rejoining works and starts no earlier than the host can still take.
  split7_client_join(&c.c, 10, 0);
  EXPECT_TRUE(wait_state(&c.c, SPLIT7_CLIENT_MEMBER)) << "rejoin";
  EXPECT_GE(slot_ts(client_join_slot(&c.c)), slot_ts(44)) << "rejoined in the transmitted part";

  fake_stop();
  split7_client_close(&a.c);
  split7_client_close(&c.c);
  split7_device_destroy(dev);
}

// Completes slot on its own thread; finished is raised to 1 once that returned.
struct Ahead {
  split7_client_t *c;
  uint64_t slot;
  split7_progress_t finished;
  pthread_t t;

  Ahead(split7_client_t *c, uint64_t slot) : c(c), slot(slot)
  {
    split7_progress_init(&finished, 0);
    pthread_create(
        &t,
        NULL,
        [](void *arg) -> void * {
          Ahead *a = static_cast<Ahead *>(arg);
          split7_client_slot_done(a->c, a->slot, 0, -1, 0);
          split7_progress_raise(&a->finished, 1);
          return NULL;
        },
        this);
  }
  ~Ahead()
  {
    pthread_join(t, NULL);
    split7_progress_destroy(&finished);
  }
};

// A member waits instead of running a ring ahead.
TEST_F(Split7Device, Backpressure)
{
  start(2 * fp.samples_per_frame);
  split7_client_t a, b;
  split7_client_open(port, &a);
  split7_client_open(port, &b);
  split7_client_join(&a, 8, 0);
  split7_client_join(&b, 8, 0);
  ASSERT_TRUE(wait_state(&a, SPLIT7_CLIENT_MEMBER) && wait_state(&b, SPLIT7_CLIENT_MEMBER));
  // The non-blocking variant neither waits nor writes without room.
  const uint64_t far = 8 + port->mem->ring_slots - 1;
  EXPECT_FALSE(split7_client_try_slot_done(&a, far, 0, -1, 0)) << "try_slot_done wrote without room";
  EXPECT_FALSE(record_written(&a, far)) << "try_slot_done wrote a record without room";
  EXPECT_TRUE(split7_client_try_slot_done(&a, 9, 0, -1, 0)) << "try_slot_done refused a slot with room";
  {
    // A runs a whole ring ahead of the stalled B: it must wait, not overwrite its own entries.
    Ahead ahead(&a, 8 + port->mem->ring_slots - 1);
    usleep(50000);
    EXPECT_FALSE(split7_progress_get(&ahead.finished)) << "a member ran a ring ahead of the host";
    for (uint64_t s = 8; s < 8 + port->mem->ring_slots - 1; s++) {
      split7_client_slot_done(&a, s, 0, -1, 0);
      split7_client_slot_done(&b, s, 0, -1, 0);
    }
    run_clock_to(slot_ts(8 + port->mem->ring_slots));
    EXPECT_EQ(split7_progress_wait(&ahead.finished, 1, 2000000), SPLIT7_REACHED) << "backpressure never released";
  }
  fake_stop();
  split7_client_close(&a);
  split7_client_close(&b);
  split7_device_destroy(dev);
}

// A member completing slots far ahead of the ring neither stalls nor corrupts.
TEST_F(Split7Device, LongSkip)
{
  const int64_t len = 2 * 6 * fp.samples_per_frame;
  start(len);
  script_t a, b;
  script_init(&a, port, 1, 0);
  script_init(&b, port, 2, 0);
  split7_client_join(&a.c, 8, 0);
  split7_client_join(&b.c, 8, 0);
  ASSERT_TRUE(wait_state(&a.c, SPLIT7_CLIENT_MEMBER) && wait_state(&b.c, SPLIT7_CLIENT_MEMBER));
  for (int s = 8; s <= 120; s++)
    a.used[s] = 1u << (s % 14);
  // B completes a ring of slots with content, then skips far more than a ring and
  // carries on: its content must survive and neither member may stall the other.
  for (int s = 8; s <= 44; s++)
    b.used[s] = 1u << ((s + 7) % 14);
  script_run_range(&b, 8, 44);
  split7_client_skip_through(&b.c, 100);
  auto ra = [&] { script_run_range(&a, 8, 120); };
  auto rb = [&] { script_run_range(&b, 101, 120); };
  pthread_t ta = spawn(&ra), tb = spawn(&rb);
  for (int s = 10; s <= 124; s += 2)
    run_clock_to(slot_ts(s));
  pthread_join(ta, NULL);
  pthread_join(tb, NULL);
  EXPECT_TRUE(wait_tx_until(dev, slot_ts(118))) << "device stalled after a long skip";
  Buf expected = group_expected({{&a, 8, 120}}, len);
  add_group_expected(expected, len, {{&b, 8, 44}});
  EXPECT_EQ(count_diff(fake.tx_capture, expected, slot_ts(7), slot_ts(118)), 0);
  fake_stop();
  split7_client_close(&a.c);
  split7_client_close(&b.c);
  split7_device_destroy(dev);
}

// drop_through: a post waiting for room is released and discarded.
TEST_F(Split7Device, DropReleasesBackpressure)
{
  start(2 * fp.samples_per_frame);
  split7_client_t a, b;
  split7_client_open(port, &a);
  split7_client_open(port, &b);
  split7_client_join(&a, 8, 0);
  split7_client_join(&b, 8, 0);
  ASSERT_TRUE(wait_state(&a, SPLIT7_CLIENT_MEMBER) && wait_state(&b, SPLIT7_CLIENT_MEMBER));
  const uint64_t slot = 8 + port->mem->ring_slots - 1;
  {
    Ahead ahead(&a, slot);
    usleep(50000);
    EXPECT_FALSE(split7_progress_get(&ahead.finished)) << "not blocked";
    // B stalls for good; A gives its slots up: the blocked post returns without writing.
    split7_client_drop_through(&a, slot);
    EXPECT_EQ(split7_progress_wait(&ahead.finished, 1, 100000), SPLIT7_REACHED) << "drop_through did not release the blocked post";
  }
  EXPECT_FALSE(record_written(&a, slot)) << "a dropped slot was recorded";
  fake_stop();
  split7_client_close(&a);
  split7_client_close(&b);
  split7_device_destroy(dev);
}

// A skipped first slot is kept at the join TA, nothing else changes.
TEST_F(Split7Device, SkippedFirstSlotTa)
{
  const int64_t len = 2 * fp.samples_per_frame;
  start(len);
  script_t b, a;
  script_init(&b, port, 2, 600); // id 0
  script_init(&a, port, 1, 600); // id 1
  split7_client_join(&a.c, 8, 600);
  ASSERT_TRUE(wait_state(&a.c, SPLIT7_CLIENT_MEMBER)) << "join a";
  a.used[20] = 0x10;
  a.used[22] = 0x10;
  script_run_range(&a, 8, 12);
  usleep(5000);
  split7_client_join(&b.c, 20, 600);
  ASSERT_TRUE(wait_state(&b.c, SPLIT7_CLIENT_MEMBER)) << "join b";
  // B gives its first slot up: a slot without a record keeps its join TA.
  split7_client_skip_through(&b.c, 20);
  usleep(5000);
  script_run_range(&a, 13, 30);
  script_run_range(&b, 21, 30);
  run_clock_to(slot_ts(34));
  EXPECT_TRUE(wait_tx_until(dev, slot_ts(30)));
  Buf expected = group_expected({{&a, 8, 30}}, len);
  EXPECT_EQ(count_diff(fake.tx_capture, expected, slot_ts(7), slot_ts(30)), 0) << "a skipped first slot changed the placement";
  fake_stop();
  split7_client_close(&a.c);
  split7_client_close(&b.c);
  split7_device_destroy(dev);
}

// A timing shift after the furthest member left stays in order.
TEST_F(Split7Device, ShiftAfterMemberLeft)
{
  start(2 * fp.samples_per_frame);
  split7_client_t a, b;
  split7_client_open(port, &a);
  split7_client_open(port, &b);
  split7_client_join(&a, 8, 0);
  split7_client_join(&b, 8, 0);
  ASSERT_TRUE(wait_state(&a, SPLIT7_CLIENT_MEMBER) && wait_state(&b, SPLIT7_CLIENT_MEMBER));
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
  pthread_mutex_lock(&dev->tb_mutex);
  EXPECT_GE(dev->tb.num_shifts, 1) << "shifts lost";
  if (dev->tb.num_shifts >= 1) {
    EXPECT_EQ(dev->tb.shift[dev->tb.num_shifts - 1].samples, 10);
  }
  for (int i = 1; i < dev->tb.num_shifts; i++)
    EXPECT_GE(dev->tb.shift[i].from_slot, dev->tb.shift[i - 1].from_slot) << "shift " << i << " starts before the previous one";
  pthread_mutex_unlock(&dev->tb_mutex);
  fake_stop();
  split7_client_close(&a);
  split7_device_destroy(dev);
}

// A leave racing join acceptance is never lost.
TEST_F(Split7Device, LeaveWhileJoining)
{
  start(2 * fp.samples_per_frame);
  int lost = 0;
  unsigned seed = 5;
  for (int i = 0; i < 300; i++) {
    split7_client_t c;
    if (split7_client_open(port, &c) != 0) {
      usleep(2000);
      continue;
    }
    split7_client_join(&c, 20, 0);
    for (volatile int k = 0; k < (int)(rand_r(&seed) % 60) * 50; k = k + 1) {
    }
    split7_client_leave(&c, 25, 0);
    // Whichever of join acceptance and leave came first, the client ends up leaving or left.
    usleep(1000);
    const split7_client_state_t st = client_state(&c);
    lost += st == SPLIT7_CLIENT_MEMBER || st == SPLIT7_CLIENT_JOINING;
    split7_client_close(&c);
  }
  EXPECT_EQ(lost, 0) << "leaves lost against join acceptance out of 300";
  fake_stop();
  split7_device_destroy(dev);
}

// The host lets go of a closed client before its slot is reused.
TEST_F(Split7Device, CloseReopen)
{
  start(2 * fp.samples_per_frame);
  c16_t bins[139] = {};
  for (int i = 0; i < 50; i++) {
    // A member with a pending PRACH closes without leaving; the next client gets its slot.
    split7_client_t a, b;
    ASSERT_EQ(split7_client_open(port, &a), 0);
    split7_client_join(&a, 8, 0);
    ASSERT_TRUE(wait_state(&a, SPLIT7_CLIENT_MEMBER));
    EXPECT_EQ(split7_client_tx_prach(&a, 9, 0, 0, 139, 1, bins), 0);
    split7_client_close(&a);
    EXPECT_EQ(client_state(&a), SPLIT7_CLIENT_FREE) << "closed client not released";
    EXPECT_EQ(prach_tag(&a, 0), 0u) << "closed client's PRACH not released";
    ASSERT_EQ(split7_client_open(port, &b), 0);
    EXPECT_EQ(b.id, a.id) << "slot not reused";
    // Its PRACH is not released on behalf of the previous client.
    EXPECT_EQ(split7_client_tx_prach(&b, 9, 0, 0, 139, 1, bins), 0);
    usleep(3000);
    EXPECT_EQ(prach_tag(&b, 0), 10u) << "new client's PRACH released";
    EXPECT_TRUE(split7_client_join(&b, 8, 0) && wait_state(&b, SPLIT7_CLIENT_MEMBER)) << "new client cannot join";
    split7_client_close(&b);
  }
  fake_stop();
  split7_device_destroy(dev);
}

// Rejoin after a leave request: waits for the leave, then joins at the new slot.
TEST_F(Split7Device, RejoinAfterLeave)
{
  start(2 * fp.samples_per_frame);
  split7_client_t c;
  split7_client_open(port, &c);
  split7_client_join(&c, 8, 0);
  ASSERT_TRUE(wait_state(&c, SPLIT7_CLIENT_MEMBER));
  EXPECT_FALSE(split7_client_join(&c, 9, 0)) << "joined twice";
  split7_client_leave(&c, 10, 0); // request only
  run_clock_to(slot_ts(14));
  // Waits until the host let it go, then asks for the new slot only.
  EXPECT_TRUE(split7_client_join(&c, 40, 0)) << "rejoin refused";
  EXPECT_TRUE(wait_state(&c, SPLIT7_CLIENT_MEMBER)) << "rejoin";
  EXPECT_GE(client_join_slot(&c), 40u);
  fake_stop();
  split7_client_close(&c);
  split7_device_destroy(dev);
}

// Rejoin after giving slots up: the new membership posts its slots.
TEST_F(Split7Device, RejoinAfterDrop)
{
  start(2 * fp.samples_per_frame);
  split7_client_t c;
  split7_client_open(port, &c);
  split7_client_join(&c, 8, 0);
  ASSERT_TRUE(wait_state(&c, SPLIT7_CLIENT_MEMBER));
  // Gives up far ahead, leaves earlier, and comes back below what it gave up.
  split7_client_drop_through(&c, 60);
  split7_client_leave(&c, 10, 0);
  run_clock_to(slot_ts(14));
  EXPECT_TRUE(split7_client_join(&c, 20, 0)) << "rejoin refused";
  EXPECT_TRUE(wait_state(&c, SPLIT7_CLIENT_MEMBER)) << "rejoin";
  const uint64_t slot = client_join_slot(&c);
  EXPECT_LT(slot, 60u);
  std::vector<c16_t> re(fp.N_RB_UL * 12);
  c16_t *data[1] = {re.data()};
  split7_client_tx_symbol(&c, slot * fp.symbols_per_slot, data, 1);
  EXPECT_EQ(split7_client_stats(&c).tx_symbols, 1u) << "post after rejoin discarded as given up";
  fake_stop();
  split7_client_close(&c);
  split7_device_destroy(dev);
}

// PRACH entries of given-up slots are released.
TEST_F(Split7Device, PrachReleased)
{
  start(3 * fp.samples_per_frame);
  split7_client_t a;
  split7_client_open(port, &a);
  split7_client_join(&a, 8, 0);
  ASSERT_TRUE(wait_state(&a, SPLIT7_CLIENT_MEMBER));
  Buf bins = buf(1024);
  for (int s = 8; s < 10; s++)
    split7_client_slot_done(&a, s, 0, -1, 0);
  // A PRACH is posted for each of four slots, which are then given up before their records.
  for (int i = 0; i < SPLIT7_MAX_PRACH; i++) {
    EXPECT_GE(split7_client_tx_prach(&a, 10 + i, 0, 64, 1024, 1, bins.get()), 0) << "PRACH entry " << i;
    split7_client_drop_through(&a, 10 + i);
  }
  for (uint64_t s = 14; s < 40; s++)
    split7_client_slot_done(&a, s, 0, -1, 0);
  run_clock_to(slot_ts(44));
  EXPECT_TRUE(wait_tx_until(dev, slot_ts(38)));
  EXPECT_GE(split7_client_tx_prach(&a, 41, 0, 64, 1024, 1, bins.get()), 0) << "PRACH entries of given-up slots never released";
  fake_stop();
  split7_client_close(&a);
  split7_device_destroy(dev);
}

// DL published == demod of the received signal; timing steered by the master after completed slots.
TEST_F(Split7Device, DlPublishAndSteering)
{
  const int fft = fp.ofdm_symbol_size;
  const int n_re = fp.N_RB_DL * 12;
  const int64_t len = 2 * fp.samples_per_frame;
  Buf signal = buf(len + 2 * fft);
  Buf scratch = buf(fft);
  Buf x = buf(fft);
  unsigned seed = 6;
  for (int slot = 0; slot < 2 * fp.slots_per_frame; slot++) {
    int64_t off = slot_ts(slot);
    for (int s = 0; s < fp.symbols_per_slot; s++) {
      fill_random(x.get(), n_re, &seed);
      split7_ofdm_mod(&fp, link_type_dl, fp.N_RB_DL, slot % fp.slots_per_frame, s, x.get(), scratch.get(), signal.get() + off);
      off += split7_symbol_samples(&fp, slot % fp.slots_per_frame, s);
    }
  }
  start(len);
  fake.rx_signal = signal.get();
  fake.rx_signal_len = len;
  split7_client_t a;
  split7_client_open(port, &a);

  run_clock_to(slot_ts(8));
  ASSERT_EQ(split7_client_wait_slot(&a, 3, 1000000), 0) << "slot 3 not published";
  Buf out = buf(fft);
  for (int s = 0; s < fp.symbols_per_slot; s++) {
    split7_ofdm_demod(&fp,
                      link_type_dl,
                      fp.N_RB_DL,
                      3,
                      s,
                      signal.get() + slot_ts(3) + get_samples_symbol_timestamp(&fp, 3, s),
                      out.get());
    EXPECT_EQ(memcmp(out.get(), a.rx_ring[0] + (3 * fp.symbols_per_slot + s) * fft, n_re * sizeof(c16_t)), 0)
        << "DL symbol " << s << " of slot 3";
  }

  // Timing steering: only the first member is listened to; the shift starts after
  // every slot already completed, at the same slot for RX and TX.
  split7_client_t b;
  split7_client_open(port, &b);
  split7_client_join(&a, 14, 0);
  ASSERT_TRUE(wait_state(&a, SPLIT7_CLIENT_MEMBER)) << "join a";
  split7_client_join(&b, 14, 0);
  ASSERT_TRUE(wait_state(&b, SPLIT7_CLIENT_MEMBER)) << "join b";
  EXPECT_TRUE(split7_client_is_timing_master(&a));
  EXPECT_FALSE(split7_client_is_timing_master(&b));
  for (int s = 14; s <= 18; s++) {
    split7_client_slot_done(&a, s, 0, -1, 0);
    split7_client_slot_done(&b, s, 0, -1, 0);
  }
  usleep(5000);
  split7_client_adjust_timing(&b, 50);
  split7_client_adjust_timing(&a, 7);
  run_clock_to(slot_ts(12));
  EXPECT_EQ(split7_client_wait_slot(&a, 10, 1000000), 0) << "slot 10 not published";
  pthread_mutex_lock(&dev->tb_mutex);
  EXPECT_EQ(dev->tb.num_shifts, 1);
  EXPECT_EQ(dev->tb.shift[0].samples, 7);
  EXPECT_GE(dev->tb.shift[0].from_slot, 20u);
  pthread_mutex_unlock(&dev->tb_mutex);

  fake_stop();
  split7_client_close(&a);
  split7_client_close(&b);
  split7_device_destroy(dev);
}

// RX symbol reads refuse unpublished and overwritten symbols.
TEST_F(Split7Device, RxSymbolOverwritten)
{
  start(0);
  split7_client_t a;
  split7_client_open(port, &a);
  const uint64_t ring_slots = port->mem->ring_slots;
  Buf out = buf(fp.ofdm_symbol_size);
  c16_t *dst[1] = {out.get()};

  run_clock_to(slot_ts(8));
  ASSERT_EQ(split7_client_wait_slot(&a, 3, 1000000), 0) << "slot 3 not published";
  EXPECT_TRUE(split7_client_rx_symbol(&a, 3 * fp.symbols_per_slot + 2, dst, 1)) << "published symbol refused";
  EXPECT_FALSE(split7_client_rx_symbol(&a, 9 * fp.symbols_per_slot, dst, 1)) << "unpublished symbol read";
  // The host reaches slot 3's ring position again: its symbols are gone.
  run_clock_to(slot_ts(3 + ring_slots + 2));
  ASSERT_EQ(split7_client_wait_slot(&a, 3 + ring_slots, 1000000), 0) << "slot 3 + ring not published";
  EXPECT_FALSE(split7_client_rx_symbol(&a, 3 * fp.symbols_per_slot + 2, dst, 1)) << "overwritten symbol read";

  fake_stop();
  split7_client_close(&a);
  split7_device_destroy(dev);
}

// Lockstep radio, nobody transmitting: the radio delivers samples only as TX covers
// them, so silence must be written from the first read on, anchored or not, ahead of
// the reads: with a peer leading by one read, TX starting at the first read's end and
// following the reads would never let the second one through.
TEST_F(Split7Device, LockstepIdle)
{
  start(4 * fp.samples_per_frame, fp.ofdm_symbol_size /* the device's read size */, false);
  EXPECT_TRUE(run_clock_to_bounded(slot_ts(30))) << "radio stalled before anchoring";
  anchor();
  EXPECT_TRUE(run_clock_to_bounded(slot_ts(70))) << "radio stalled while anchored without members";
  fake_stop();
  expect_stream();
  EXPECT_EQ(count_diff(fake.tx_capture, buf(fake.tx_capture_len), 0, fake.tx_capture_len), 0) << "non-zero samples sent while idle";
  split7_device_destroy(dev);
}

// Lockstep radio with a member: the radio runs no further than the member's TX
// (plus the peer's lead), the stream is continuous, its content the member's.
TEST_F(Split7Device, LockstepMember)
{
  const int slot = fp.samples_per_subframe / fp.slots_per_subframe;
  const int ta = 600;
  const int64_t len = 4 * fp.samples_per_frame;
  start(len, slot, false);
  EXPECT_TRUE(run_clock_to_bounded(slot_ts(4))) << "radio stalled before anchoring";
  anchor();
  script_t a;
  script_init(&a, port, 1, ta);
  split7_client_join(&a.c, 8, ta);
  ASSERT_TRUE(wait_state(&a.c, SPLIT7_CLIENT_MEMBER));
  EXPECT_EQ(client_join_slot(&a.c), 8u);
  for (int s = 8; s <= 40; s++)
    a.used[s] = s % 5 == 0 ? 0x3fff : (s % 5 == 2 ? 0x3 << 6 : 0); // full, partial and empty slots
  // The member is slow: the radio must wait for it, not run ahead.
  pthread_mutex_lock(&fake.mutex);
  fake.limit = INT64_MAX;
  pthread_cond_broadcast(&fake.cond);
  pthread_mutex_unlock(&fake.mutex);
  for (uint64_t s = 8; s <= 15; s++) {
    usleep(5000);
    script_run_range(&a, s, s);
  }
  usleep(100000);
  const int64_t frontier = slot_ts(16) - ta;
  const int64_t clock = fake_clock();
  EXPECT_LE(clock, frontier + slot) << "radio ran past the member's TX + lead";
  EXPECT_GT(clock, frontier + slot - 2 * fp.ofdm_symbol_size) << "radio stopped short of the member's TX";
  script_run_range(&a, 16, 40);
  split7_client_leave(&a.c, 40, 0);
  EXPECT_TRUE(wait_state(&a.c, SPLIT7_CLIENT_OPEN)) << "leave not confirmed";
  EXPECT_TRUE(run_clock_to_bounded(slot_ts(70))) << "radio stalled after the member left";
  fake_stop();
  expect_stream();
  Buf expected = group_expected({{&a, 8, 40}}, len);
  EXPECT_EQ(count_diff(fake.tx_capture, expected, 0, len), 0) << "stream content differs from the member's symbols";
  EXPECT_EQ(split7_device_stats(dev).tx_late_writes, 0u) << "late writes counted on a lockstep radio";
  split7_client_close(&a.c);
  split7_device_destroy(dev);
}

} // namespace

int main(int argc, char **argv)
{
  split7_test_init(&fp);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
