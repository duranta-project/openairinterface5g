/*
 * OAI device for an OC1 board running our own FPGA image, through libocsdr
 * (https://github.com/arash9638444/ocsdr). Same behaviour as OCUDU's ocsdr radio backend.
 *
 * Select it with --device.name oai_ocsdr (or device = { name = "oai_ocsdr"; } in the config).
 * Options go in the RU's sdr_addrs, comma separated key=value:
 *   board=N       PCIe board index (default 0)
 *   host=ADDR     board over 10G Ethernet instead of PCIe (FPGA release v13+), e.g. 10.0.0.2
 *   tx_shift=N    TX samples are shifted left by N bits, saturating (default 2)
 *   ring=N        DMA ring blocks per direction, power of two 2..256 (default 256)
 *   skip_init=1   keep the clocks, JESD links and AD9371 as they are
 *   frontend=0/1  RF front end on (TX to the connectors, LNAs on), default 1
 *   rx_atten=dB   front-end RX attenuators 0..31 (default 0)
 *   rx1_atten=dB  RX1 attenuator only, overrides rx_atten (RX2 has about 20 dB less front-end gain; this balances them)
 *   tx_atten=dB   AD9371 TX attenuation 0..41.95 (default 1.95)
 *   rx_gain=dB    AD9371 RX gain 0..30 above index 195 (default 20, index 235)
 *   rx2_gain=dB   AD9371 RX2 gain only, overrides rx_gain
 *   tx_advance=N  samples TX is sent ahead of RX time (default 174, measured with "ocsdr stream latency")
 *   tx_gate=0/1   FPGA TX gating (image v16s+, board over Ethernet): TX PAs off between the bursts that the
 *                 write flags end (TX_BURST_END...), i.e. in TDD uplink slots; run without --continuous-tx
 *   tx_gate_lead=N, tx_gate_lag=N  samples the gate opens before / stays open after a burst (128 / 256)
 *   tx_gate_mode=M  0 PAs off between bursts, 1 PAs off + TX switches to the 50 ohm loads, 2 switches only,
 *                   PAs stay on (default; per-burst PA shutdown leaves RX1 ~6 dB noisier for >100 us)
 *   tx_flush_us=N  send a partly filled TX block N us before its start time if no more samples came (default 0:
 *                  off). A block only leaves when full, so the tail of a slot waits for the next slot's write; with
 *                  this a late next write loses its first samples (at most one block) instead of the earlier
 *                  slot losing its tail, which holds the PUCCH.
 *   rx_only=1     receive only: TX amplifiers shut down, TX switches off the connectors, TX samples dropped in the
 *                 driver (nothing is transmitted, e.g. to scan public networks)
 *   tx_lead_stats=1 log once per second how early TX blocks are handed over (block start - board time at commit)
 */

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <string>
#include <sstream>

extern "C" {
#include "common_lib.h"
#include "common/utils/LOG/log.h"
#include "ocsdr/ocsdr.h"
}

static constexpr unsigned MAX_CHANNELS    = 2;
static constexpr int      RING_TIMEOUT_US = 100000;
static constexpr double   SAMPLE_RATE_HZ  = 122.88e6;
static constexpr double   TX_ATTEN_MAX_DB = 41.95;
static constexpr int      RX_GAIN_INDEX_0 = 195;

struct ocsdr_args {
  int         board      = 0;
  std::string host;
  unsigned    tx_shift   = 2;
  unsigned    ring       = 256;
  bool        skip_init  = false;
  bool        frontend   = true;
  double      rx_atten   = 0;
  double      rx1_atten  = -1; // < 0: same as rx_atten
  double      tx_atten   = 1.95;
  double      rx_gain    = 20;
  double      rx2_gain   = -1; // < 0: same as rx_gain
  int         tx_advance = 174;
  bool        tx_gate      = false;
  unsigned    tx_gate_lead = 128;
  unsigned    tx_gate_lag  = 256;
  unsigned    tx_gate_mode = 2;
  unsigned    tx_flush_us  = 0;
  bool        tx_lead_stats = false;
  bool        rx_only      = false;
};

struct ocsdr_state {
  ocsdr_args    args;
  ocsdr_dev    *dev    = nullptr;
  ocsdr_stream *stream = nullptr;
  unsigned      hw_ch  = 1;
  unsigned      bs     = 0;  // sample times per block
  std::atomic<bool> running{false};

  // receive
  const int16_t *rx_block = nullptr;
  uint64_t       rx_block_ts = 0;
  uint64_t       rx_next_ts  = 0;
  bool           rx_first    = true;
  uint64_t       rx_dropped  = 0;

  // transmit
  int16_t *tx_block     = nullptr;
  uint64_t tx_block_ts  = 0;
  unsigned tx_fill      = 0;
  bool     tx_closed    = true;
  unsigned tx_since_chk = 0;
  uint32_t last_late = 0, last_underflow = 0;
  std::mutex  tx_mtx;            // tx_* state, shared by ocsdr_write and the flusher
  std::thread tx_flusher;
  uint64_t    tx_sent_until = 0; // samples before this time already went out in an early-flushed block
  uint64_t    tx_early = 0, tx_trimmed = 0;
  // tx_lead_stats: per second, lead time of the committed blocks in 50 us bins (last bin: >= 750 us)
  int64_t     lead_min = INT64_MAX;
  uint64_t    lead_n = 0, lead_neg = 0, lead_hist[16] = {0}, lead_next_log = 0;
  // and the age of the RX samples when ocsdr_read returns them
  int64_t     rx_age_max = 0, rx_age_sum = 0;
  uint64_t    rx_age_n = 0, rx_age_over200 = 0;

  // signal levels, from one sample in LEVEL_STEP, logged about every 4 s
  double   tx_sq = 0, rx_sq = 0;
  uint64_t tx_n = 0, rx_n = 0, tx_clip = 0;
  unsigned tx_blocks = 0, rx_blocks = 0;
};

static constexpr unsigned LEVEL_STEP   = 16;
static constexpr unsigned LEVEL_BLOCKS = 1u << 18;

static double dbfs(double sq, uint64_t n)
{
  return n == 0 || sq == 0 ? -INFINITY : 10 * log10(sq / n / (32768.0 * 32768.0));
}

static bool parse_args(const char *s, ocsdr_args &out)
{
  if (s == nullptr)
    return true;
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, ',')) {
    if (item.empty())
      continue;
    auto eq = item.find('=');
    if (eq == std::string::npos) {
      LOG_E(HW, "ocsdr: sdr_addrs item '%s' is not key=value\n", item.c_str());
      return false;
    }
    std::string key = item.substr(0, eq), value = item.substr(eq + 1);
    if (key == "host") {
      out.host = value;
      continue;
    }
    char *end = nullptr;
    double v = strtod(value.c_str(), &end);
    if (end == value.c_str() || *end != '\0') {
      LOG_E(HW, "ocsdr: sdr_addrs item '%s' needs a number\n", item.c_str());
      return false;
    }
    if (key == "board" && v >= 0)
      out.board = (int)v;
    else if (key == "rx_only")
      out.rx_only = v != 0;
    else if (key == "tx_lead_stats")
      out.tx_lead_stats = v != 0;
    else if (key == "tx_flush_us" && v >= 0 && v <= 10000)
      out.tx_flush_us = (unsigned)v;
    else if (key == "tx_shift" && v >= 0 && v <= 15)
      out.tx_shift = (unsigned)v;
    else if (key == "ring" && v >= 2 && v <= 256 && (((unsigned)v) & ((unsigned)v - 1)) == 0)
      out.ring = (unsigned)v;
    else if (key == "skip_init")
      out.skip_init = v != 0;
    else if (key == "frontend")
      out.frontend = v != 0;
    else if (key == "rx_atten" && v >= 0 && v <= 31.5)
      out.rx_atten = v;
    else if (key == "rx1_atten" && v >= 0 && v <= 31.5)
      out.rx1_atten = v;
    else if (key == "tx_atten" && v >= 0 && v <= TX_ATTEN_MAX_DB)
      out.tx_atten = v;
    else if (key == "rx_gain" && v >= 0 && v <= 30)
      out.rx_gain = v;
    else if (key == "rx2_gain" && v >= 0 && v <= 30)
      out.rx2_gain = v;
    else if (key == "tx_advance" && v >= 0 && v < 10000)
      out.tx_advance = (int)v;
    else if (key == "tx_gate")
      out.tx_gate = v != 0;
    else if (key == "tx_gate_lead" && v >= 0 && v <= 65535)
      out.tx_gate_lead = (unsigned)v;
    else if (key == "tx_gate_lag" && v >= 0 && v <= 16383)
      out.tx_gate_lag = (unsigned)v;
    else if (key == "tx_gate_mode" && v >= 0 && v <= 2)
      out.tx_gate_mode = (unsigned)v;
    else {
      LOG_E(HW,
            "ocsdr: unknown or out-of-range sdr_addrs item '%s' (keys: board, host, tx_shift, ring, skip_init, frontend, "
            "rx_atten, rx1_atten, tx_atten, rx_gain, rx2_gain, tx_advance, tx_gate, tx_gate_lead, tx_gate_lag, "
            "tx_gate_mode, tx_flush_us, tx_lead_stats, rx_only)\n",
            item.c_str());
      return false;
    }
  }
  return true;
}

// ---- receive ----

static int ocsdr_read(openair0_device_t *device, openair0_timestamp_t *ptimestamp, void **buff, int nsamps, int nb_ant)
{
  ocsdr_state *s = (ocsdr_state *)device->priv;
  const unsigned hw = s->hw_ch, bs = s->bs;

  auto zero = [&](unsigned from, unsigned n) {
    for (int a = 0; a < nb_ant; a++)
      memset((uint32_t *)buff[a] + from, 0, n * sizeof(uint32_t));
  };

  // The first block fixes the timeline; afterwards every sample is copied from the block that holds its timestamp,
  // or zero when the FPGA dropped that block, so the returned timestamps stay contiguous.
  if (s->rx_first) {
    uint32_t dropped = 0;
    if (ocsdr_stream_rx_acquire(s->stream, &s->rx_block, &s->rx_block_ts, &dropped, RING_TIMEOUT_US) != 0) {
      LOG_E(HW, "ocsdr: no RX block for %d ms\n", RING_TIMEOUT_US / 1000);
      return -1;
    }
    s->rx_next_ts = s->rx_block_ts;
    s->rx_first   = false;
  }
  *ptimestamp = s->rx_next_ts;

  unsigned pos = 0;
  while (pos < (unsigned)nsamps) {
    if (!s->running.load(std::memory_order_relaxed)) {
      zero(pos, nsamps - pos);
      break;
    }
    if (s->rx_block == nullptr) {
      uint32_t dropped = 0;
      if (ocsdr_stream_rx_acquire(s->stream, &s->rx_block, &s->rx_block_ts, &dropped, RING_TIMEOUT_US) != 0) {
        s->rx_block = nullptr;
        LOG_W(HW, "ocsdr: no RX block for %d ms\n", RING_TIMEOUT_US / 1000);
        continue;
      }
      if (dropped != 0) {
        s->rx_dropped += dropped;
        LOG_W(HW, "ocsdr: the FPGA dropped %u RX block(s) before ts=%lu\n", dropped, s->rx_block_ts);
      }
    }
    if (s->rx_block_ts + bs <= s->rx_next_ts) {
      ocsdr_stream_rx_release(s->stream);
      s->rx_block = nullptr;
      continue;
    }
    if (s->rx_block_ts > s->rx_next_ts) {
      unsigned n = (unsigned)std::min<uint64_t>(s->rx_block_ts - s->rx_next_ts, nsamps - pos);
      zero(pos, n);
      pos += n;
      s->rx_next_ts += n;
      continue;
    }
    unsigned off = (unsigned)(s->rx_next_ts - s->rx_block_ts);
    unsigned n   = std::min(bs - off, nsamps - pos);
    for (int a = 0; a < nb_ant; a++) {
      uint32_t *o = (uint32_t *)buff[a] + pos;
      if ((unsigned)a >= hw) {
        memset(o, 0, n * sizeof(uint32_t));
        continue;
      }
      const uint32_t *w = (const uint32_t *)s->rx_block + (size_t)off * hw + a;
      if (hw == 1) {
        memcpy(o, w, n * sizeof(uint32_t));
      } else {
        for (unsigned j = 0; j < n; j++, w += hw)
          o[j] = *w;
      }
    }
    for (unsigned j = 0; j < n; j += LEVEL_STEP) {
      const int16_t *v = (const int16_t *)buff[0] + 2 * (pos + j);
      s->rx_sq += (double)v[0] * v[0] + (double)v[1] * v[1];
      s->rx_n++;
    }
    pos += n;
    s->rx_next_ts += n;
    if (off + n == bs) {
      if (++s->rx_blocks == LEVEL_BLOCKS) {
        LOG_I(HW, "ocsdr: RX1 %.1f dBFS\n", dbfs(s->rx_sq, s->rx_n));
        s->rx_sq = 0;
        s->rx_n = s->rx_blocks = 0;
      }
      ocsdr_stream_rx_release(s->stream);
      s->rx_block = nullptr;
    }
  }
  if (s->args.tx_lead_stats) { // age of the samples handed over: how far behind real time the reader runs
    const int64_t age_us = ((int64_t)ocsdr_stream_time(s->stream) - (int64_t)s->rx_next_ts) * 1e6 / SAMPLE_RATE_HZ;
    s->rx_age_max = std::max(s->rx_age_max, age_us);
    s->rx_age_sum += age_us;
    s->rx_age_n++;
    if (age_us > 200)
      s->rx_age_over200++;
  }
  return nsamps;
}

// ---- transmit ----

static void tx_check_counters(ocsdr_state *s)
{
  ocsdr_stream_counters c;
  if (ocsdr_stream_counters_read(s->stream, &c) != 0)
    return;
  if (c.tx_late != s->last_late)
    LOG_W(HW, "ocsdr: %u TX block(s) arrived late and were dropped\n", c.tx_late - s->last_late);
  if (c.tx_underflow != s->last_underflow)
    LOG_W(HW, "ocsdr: %u TX underflow(s)\n", c.tx_underflow - s->last_underflow);
  s->last_late      = c.tx_late;
  s->last_underflow = c.tx_underflow;
}

static void tx_lead_record(ocsdr_state *s)
{
  const uint64_t now = ocsdr_stream_time(s->stream);
  const int64_t lead_us = ((int64_t)s->tx_block_ts - (int64_t)now) * 1e6 / SAMPLE_RATE_HZ;
  s->lead_min = std::min(s->lead_min, lead_us);
  s->lead_n++;
  if (lead_us < 0)
    s->lead_neg++;
  else
    s->lead_hist[std::min<int64_t>(lead_us / 50, 15)]++;
  if (now < s->lead_next_log)
    return;
  if (s->lead_next_log) {
    char h[16 * 8], *p = h;
    for (int i = 0; i < 16; i++)
      p += sprintf(p, " %lu", s->lead_hist[i]);
    LOG_I(HW, "ocsdr: TX lead: %lu blocks, min %ld us, %lu behind time, hist(50 us bins)%s\n", s->lead_n, s->lead_min, s->lead_neg, h);
    LOG_I(HW, "ocsdr: RX age at read: %lu reads, mean %ld us, max %ld us, %lu over 200 us\n", s->rx_age_n,
          s->rx_age_n ? s->rx_age_sum / (int64_t)s->rx_age_n : 0, s->rx_age_max, s->rx_age_over200);
  }
  s->rx_age_max = s->rx_age_sum = 0;
  s->rx_age_n = s->rx_age_over200 = 0;
  s->lead_min = INT64_MAX;
  s->lead_n = s->lead_neg = 0;
  memset(s->lead_hist, 0, sizeof(s->lead_hist));
  s->lead_next_log = now + (uint64_t)SAMPLE_RATE_HZ;
}

static void tx_flush(ocsdr_state *s, bool end_of_burst)
{
  if (s->tx_block == nullptr) {
    if (end_of_burst && !s->tx_closed) {
      ocsdr_stream_tx_end_burst(s->stream);
      s->tx_closed = true;
    }
    return;
  }
  memset(s->tx_block + 2 * s->hw_ch * s->tx_fill, 0, (size_t)(s->bs - s->tx_fill) * 2 * s->hw_ch * sizeof(int16_t));
  if (s->args.tx_lead_stats)
    tx_lead_record(s);
  ocsdr_stream_tx_commit(s->stream, s->tx_block_ts, OCSDR_TX_TIMED | (end_of_burst ? OCSDR_TX_EOB : 0));
  s->tx_block  = nullptr;
  s->tx_closed = end_of_burst;
  if ((++s->tx_since_chk & 1023) == 0)
    tx_check_counters(s);
  if (++s->tx_blocks == LEVEL_BLOCKS) {
    LOG_I(HW, "ocsdr: TX1 %.1f dBFS (all samples, idle included), %lu of %lu checked samples clipped\n",
          dbfs(s->tx_sq, s->tx_n), s->tx_clip, s->tx_n);
    if (s->args.tx_flush_us)
      LOG_I(HW, "ocsdr: %lu TX block(s) sent early by the flusher, %lu late sample(s) dropped\n", s->tx_early, s->tx_trimmed);
    s->tx_sq = 0;
    s->tx_n = s->tx_clip = s->tx_blocks = 0;
  }
}

static int ocsdr_write(openair0_device_t *device, openair0_timestamp_t timestamp, void **buff, int nsamps, int nb_ant, int flags)
{
  ocsdr_state *s = (ocsdr_state *)device->priv;
  if (!s->running.load(std::memory_order_relaxed) || s->args.rx_only)
    return nsamps;
  timestamp -= device->openair0_cfg->command_line_sample_advance + device->openair0_cfg->tx_sample_advance;

  const unsigned hw = s->hw_ch, bs = s->bs;
  const unsigned nch = std::min((unsigned)nb_ant, hw);
  const int burst = flags & 0xf;
  uint64_t ts = timestamp;
  std::lock_guard<std::mutex> lock(s->tx_mtx);

  // A block holds consecutive samples. A short forward jump is zero-filled inside the open block; anything else
  // closes the block as the end of a burst and opens a new one at the new timestamp.
  if (s->tx_block != nullptr && ts != s->tx_block_ts + s->tx_fill) {
    if (ts > s->tx_block_ts + s->tx_fill && ts < s->tx_block_ts + bs) {
      unsigned gap = (unsigned)(ts - (s->tx_block_ts + s->tx_fill));
      memset(s->tx_block + 2 * hw * s->tx_fill, 0, (size_t)gap * 2 * hw * sizeof(int16_t));
      s->tx_fill += gap;
    } else {
      tx_flush(s, true);
    }
  }

  const int32_t lim = INT16_MAX;
  const unsigned sh = s->args.tx_shift;
  unsigned pos = 0;
  // the start of this write may belong to a block the flusher already sent (zero-filled): drop those samples
  if (s->tx_block == nullptr && ts < s->tx_sent_until) {
    pos = (unsigned)std::min<uint64_t>(s->tx_sent_until - ts, (uint64_t)nsamps);
    s->tx_trimmed += pos;
  }
  while (pos < (unsigned)nsamps) {
    if (s->tx_block == nullptr) {
      if (ocsdr_stream_tx_acquire(s->stream, &s->tx_block, RING_TIMEOUT_US) != 0) {
        s->tx_block = nullptr;
        LOG_W(HW, "ocsdr: TX ring full for %d ms, dropping %d samples\n", RING_TIMEOUT_US / 1000, nsamps - pos);
        return nsamps;
      }
      s->tx_block_ts = ts + pos;
      s->tx_fill     = 0;
    }
    unsigned n = std::min(bs - s->tx_fill, nsamps - pos);
    for (unsigned c = 0; c < hw; c++) {
      int16_t *dst = s->tx_block + 2 * ((size_t)s->tx_fill * hw + c);
      if (c >= nch) {
        for (unsigned i = 0; i < n; i++, dst += 2 * hw)
          dst[0] = dst[1] = 0;
        continue;
      }
      const int16_t *in = (const int16_t *)buff[c] + 2 * pos;
      for (unsigned i = 0; i < n; i++, dst += 2 * hw) {
        int32_t re = (int32_t)in[2 * i] << sh, im = (int32_t)in[2 * i + 1] << sh;
        dst[0] = (int16_t)std::clamp(re, -lim, lim);
        dst[1] = (int16_t)std::clamp(im, -lim, lim);
        if (c == 0 && (i % LEVEL_STEP) == 0) {
          s->tx_sq += (double)re * re + (double)im * im;
          s->tx_n++;
          s->tx_clip += re > lim || re < -lim || im > lim || im < -lim;
        }
      }
    }
    s->tx_fill += n;
    pos += n;
    if (s->tx_fill == bs)
      tx_flush(s, false);
  }
  if (burst == TX_BURST_END || burst == TX_BURST_START_AND_END || burst == TX_BURST_END_NO_TIME_SPEC)
    tx_flush(s, true);
  return nsamps;
}

// ---- control ----

static int set_gains(ocsdr_state *s, unsigned nch)
{
  int index = RX_GAIN_INDEX_0 + (int)lround(2 * s->args.rx_gain);
  for (unsigned c = 1; c <= nch; c++) {
    const double g = (c == 2 && s->args.rx2_gain >= 0) ? s->args.rx2_gain : s->args.rx_gain;
    const int idx = RX_GAIN_INDEX_0 + (int)lround(2 * g);
    if (ocsdr_rf_tx_atten(s->dev, c, s->args.tx_atten) != 0 || ocsdr_rf_rx_gain(s->dev, c, idx) != 0)
      return -1;
  }
  LOG_I(HW, "ocsdr: TX attenuation %.2f dB, RX gain index %d (%.1f dB) on %u channel(s)\n", s->args.tx_atten, index, s->args.rx_gain, nch);
  return 0;
}

// Sends the open TX block, zero-filled, when its start time is less than tx_flush_us away
static void tx_flusher_loop(ocsdr_state *s)
{
  const uint64_t margin = (uint64_t)(s->args.tx_flush_us * 1e-6 * SAMPLE_RATE_HZ);
  // above OAI's real-time threads (they use max - 2): a starved flusher is no flusher
  struct sched_param sp = {.sched_priority = sched_get_priority_max(SCHED_RR) - 1};
  if (pthread_setschedparam(pthread_self(), SCHED_RR, &sp))
    LOG_W(HW, "ocsdr: TX flusher runs without real-time priority\n");
  while (s->running.load(std::memory_order_relaxed)) {
    usleep(20);
    std::lock_guard<std::mutex> lock(s->tx_mtx);
    if (s->tx_block == nullptr || ocsdr_stream_time(s->stream) + margin < s->tx_block_ts)
      continue;
    s->tx_sent_until = s->tx_block_ts + s->bs;
    s->tx_early++;
    tx_flush(s, false);
  }
}

static int ocsdr_start(openair0_device_t *device)
{
  ocsdr_state *s = (ocsdr_state *)device->priv;
  s->rx_first = true;
  s->rx_block = nullptr;
  s->tx_block = nullptr;
  s->running  = true;
  int ret = ocsdr_stream_rx_start(s->stream);
  if (ret == 0)
    ret = ocsdr_stream_tx_start(s->stream);
  if (ret != 0) {
    LOG_E(HW, "ocsdr: cannot start streaming (%s)\n", strerror(-ret));
    return -1;
  }
  LOG_I(HW, "ocsdr: streaming started at sample clock %lu\n", ocsdr_stream_time(s->stream));
  if (s->args.tx_flush_us) {
    s->tx_sent_until = 0;
    s->tx_flusher = std::thread(tx_flusher_loop, s);
    LOG_I(HW, "ocsdr: TX blocks still open %u us before their start are sent zero-filled\n", s->args.tx_flush_us);
  }
  return 0;
}

static int ocsdr_stop(openair0_device_t *device)
{
  ocsdr_state *s = (ocsdr_state *)device->priv;
  bool expected = true;
  if (!s->running.compare_exchange_strong(expected, false))
    return 0;
  if (s->tx_flusher.joinable())
    s->tx_flusher.join();
  tx_flush(s, true);
  ocsdr_stream_tx_stop(s->stream);
  ocsdr_stream_rx_stop(s->stream);
  tx_check_counters(s);
  LOG_I(HW, "ocsdr: stopped, %lu RX block(s) dropped by the FPGA\n", s->rx_dropped);
  return 0;
}

static void ocsdr_end(openair0_device_t *device)
{
  ocsdr_state *s = (ocsdr_state *)device->priv;
  if (s == nullptr)
    return;
  ocsdr_stop(device);
  if (s->stream != nullptr)
    ocsdr_stream_close(s->stream);
  if (s->dev != nullptr) {
    ocsdr_tx_gate(s->dev, false, 0, 0, 0);
    ocsdr_rf_frontend(s->dev, false, 0.0);
    ocsdr_close(s->dev);
  }
  delete s;
  device->priv = nullptr;
}

static int ocsdr_set_freq(openair0_device_t *device, openair0_config_t *cfg)
{
  ocsdr_state *s = (ocsdr_state *)device->priv;
  LOG_I(HW, "ocsdr: TX %.3f MHz, RX %.3f MHz\n", cfg[0].tx_freq[0] / 1e6, cfg[0].rx_freq[0] / 1e6);
  return ocsdr_rf_set_freq(s->dev, (uint64_t)llround(cfg[0].tx_freq[0]), (uint64_t)llround(cfg[0].rx_freq[0])) == 0 ? 0 : -1;
}

static int ocsdr_set_gains(openair0_device_t *device, openair0_config_t *cfg)
{
  // gains come from sdr_addrs (tx_atten, rx_gain), as in the OCUDU backend
  return 0;
}

static int ocsdr_get_stats(openair0_device_t *device)
{
  return 0;
}

static int ocsdr_reset_stats(openair0_device_t *device)
{
  return 0;
}

extern "C" int device_init(openair0_device_t *device, openair0_config_t *openair0_cfg)
{
  if (device->priv != nullptr) {
    LOG_E(HW, "ocsdr: device already initialised\n");
    return 0;
  }
  ocsdr_state *s = new ocsdr_state;
  if (!parse_args(openair0_cfg[0].sdr_addrs, s->args)) {
    delete s;
    return -1;
  }
  if (fabs(openair0_cfg[0].sample_rate - SAMPLE_RATE_HZ) >= 1.0) {
    LOG_E(HW, "ocsdr: the sampling rate must be 122.88 MHz (got %.0f Hz; do not use -E)\n", openair0_cfg[0].sample_rate);
    delete s;
    return -1;
  }
  const unsigned nch = std::max(openair0_cfg[0].tx_num_channels, openair0_cfg[0].rx_num_channels);
  if (openair0_cfg[0].tx_num_channels != openair0_cfg[0].rx_num_channels || nch < 1 || nch > MAX_CHANNELS) {
    LOG_E(HW, "ocsdr: nb_tx and nb_rx must be equal, 1 or 2 (got %d and %d)\n", openair0_cfg[0].tx_num_channels, openair0_cfg[0].rx_num_channels);
    delete s;
    return -1;
  }

  s->dev = s->args.host.empty() ? ocsdr_open(s->args.board) : ocsdr_open_net(s->args.host.c_str());
  if (s->dev == nullptr) {
    LOG_E(HW, "ocsdr: cannot open board %s (our FPGA image)\n", s->args.host.empty() ? std::to_string(s->args.board).c_str() : s->args.host.c_str());
    delete s;
    return -1;
  }

  const uint64_t tx_hz = (uint64_t)llround(openair0_cfg[0].tx_freq[0]), rx_hz = (uint64_t)llround(openair0_cfg[0].rx_freq[0]);
  if (!s->args.skip_init) {
    LOG_I(HW, "ocsdr: bringing up clocks, JESD204 links and AD9371 (about 5 s)\n");
    if (ocsdr_clock_init(s->dev) != 0) {
      LOG_E(HW, "ocsdr: HMC7044 clock init failed\n");
      ocsdr_close(s->dev);
      delete s;
      return -1;
    }
    ocsdr_rf_config rf = {};
    rf.tx_hz = tx_hz;
    rf.rx_hz = rx_hz;
    ocsdr_rf_report rep = {};
    if (ocsdr_rf_init(s->dev, &rf, &rep) != 0) {
      LOG_E(HW,
            "ocsdr: RF bring-up failed: MCS 0x%02x, PLLs 0x%02x, deframer 0x%02x, FPGA links TX %s RX %s\n",
            rep.mcs_status, rep.pll_lock, rep.deframer,
            ocsdr_jesd_tx_state_name(rep.fpga_tx_state), ocsdr_jesd_rx_state_name(rep.fpga_rx_state));
      ocsdr_close(s->dev);
      delete s;
      return -1;
    }
    LOG_I(HW, "ocsdr: RF up, TX %.3f MHz, RX %.3f MHz\n", tx_hz / 1e6, rx_hz / 1e6);
  } else if (ocsdr_rf_set_freq(s->dev, tx_hz, rx_hz) != 0) {
    LOG_E(HW, "ocsdr: cannot tune the LOs\n");
  }

  int ret = ocsdr_stream_open(s->dev, s->args.ring, &s->stream);
  if (ret != 0) {
    LOG_E(HW, "ocsdr: cannot open the streaming rings (%s): is the ocsdr_dma driver loaded?\n", strerror(-ret));
    ocsdr_close(s->dev);
    delete s;
    return -1;
  }
  s->hw_ch = ocsdr_stream_channels(s->stream);
  s->bs    = ocsdr_stream_block_samples(s->stream);
  if (nch > s->hw_ch) {
    LOG_E(HW, "ocsdr: %u antennas configured, but this FPGA image carries %u channel(s)\n", nch, s->hw_ch);
    ocsdr_stream_close(s->stream);
    ocsdr_close(s->dev);
    delete s;
    return -1;
  }
  if (s->args.frontend) {
    if (ocsdr_rf_frontend(s->dev, true, s->args.rx_atten) != 0)
      LOG_E(HW, "ocsdr: cannot set the RF front end\n");
    else
      LOG_I(HW, "ocsdr: RF front end on, RX attenuators %.1f dB\n", s->args.rx_atten);
    if (s->args.rx1_atten >= 0 && ocsdr_rx_atten_set(s->dev, 1, s->args.rx1_atten) == 0)
      LOG_I(HW, "ocsdr: RX1 attenuator %.1f dB\n", s->args.rx1_atten);
  } else {
    LOG_W(HW, "ocsdr: RF front end left in its safe state (TX off): leakage only\n");
  }
  if (s->args.rx_only) {
    // keep the RX path as set above, shut both transmitters and take their switches off the connectors
    uint32_t v = ocsdr_rfctrl(s->dev);
    v &= ~(OCSDR_RF_TX1_SPDT1 | OCSDR_RF_TX1_SPDT2 | OCSDR_RF_TX2_SPDT1 | OCSDR_RF_TX2_SPDT2 | OCSDR_RF_ORX1_SPDT
           | OCSDR_RF_ORX2_SPDT);
    v |= OCSDR_RF_TX1_SHUTDOWN | OCSDR_RF_TX2_SHUTDOWN;
    ocsdr_rfctrl_set(s->dev, v);
    s->args.tx_atten = TX_ATTEN_MAX_DB;
    LOG_I(HW, "ocsdr: receive only: TX shut down (RF control 0x%08x), TX samples dropped\n", ocsdr_rfctrl(s->dev));
  }
  if (s->args.tx_gate) {
    if (s->args.host.empty()
        || ocsdr_tx_gate(s->dev,
                         true,
                         s->args.tx_gate_lead,
                         s->args.tx_gate_lag,
                         s->args.tx_gate_mode == 0   ? 0u
                         : s->args.tx_gate_mode == 1 ? OCSDR_TXGATE_LOAD
                                                     : OCSDR_TXGATE_LOAD | OCSDR_TXGATE_PA_ON)
               != 0) {
      LOG_E(HW, "ocsdr: TX gating needs the board over Ethernet with FPGA image v16s or later\n");
      return -1;
    }
    LOG_I(HW,
          "ocsdr: TX gating on between bursts, mode %u (lead %u, lag %u samples)\n",
          s->args.tx_gate_mode,
          s->args.tx_gate_lead,
          s->args.tx_gate_lag);
  }
  set_gains(s, s->hw_ch);

  openair0_cfg[0].tx_sample_advance = s->args.tx_advance;
  openair0_cfg[0].tx_bw             = 100e6;
  openair0_cfg[0].rx_bw             = 100e6;
  LOG_I(HW, "ocsdr: %uT%uR on a %u-channel image, %u samples per block, TX advance %d samples, TX << %u\n",
        nch, nch, s->hw_ch, s->bs, s->args.tx_advance, s->args.tx_shift);

  device->priv                 = s;
  device->type                 = OCSDR_DEV;
  device->host_type            = RAU_HOST;
  device->openair0_cfg         = openair0_cfg;
  device->trx_start_func       = ocsdr_start;
  device->trx_stop_func        = ocsdr_stop;
  device->trx_end_func         = ocsdr_end;
  device->trx_read_func        = ocsdr_read;
  device->trx_write_func       = ocsdr_write;
  device->trx_get_stats_func   = ocsdr_get_stats;
  device->trx_reset_stats_func = ocsdr_reset_stats;
  device->trx_set_freq_func    = ocsdr_set_freq;
  device->trx_set_gains_func   = ocsdr_set_gains;
  return 0;
}
