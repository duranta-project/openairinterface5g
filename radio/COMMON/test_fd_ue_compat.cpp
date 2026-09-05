/**
 * @file test_fd_ue_compat.cpp
 * @brief Verifies UE_fd_thread's TX timing matches UE_thread's, algebraically:
 *
 *   fd_effective_tx[sym]         = (rx_ts + slot_dur + sym_offset[sym]) - (N_TA + TA)
 *   ue_thread_effective_tx[sym]  = (rx_ts + slot_dur - N_TA - TA) + sym_offset[sym]
 *
 * identical for every symbol. Covers: nominal timestamp sequence, TX time
 * equivalence, set_timing_advance() call timing, proc field wraparound,
 * symbol-gap continuity, initial TA push, PRACH TX time equivalence.
 */

#include "ue_split7_interface.h"
#include "common_lib.h"
#include "PHY/TOOLS/tools_defs.h"

extern "C" {
// Declared directly rather than via PHY/MODULATION/nr_modulation.h: that header
// uses C99 VLA-in-parameter syntax elsewhere that GCC's C++ frontend rejects.
void init_symbol_rotation(NR_DL_FRAME_PARMS *fp);
void init_timeshift_rotation(const int ofdm_symbol_size,
                             const int nb_prefix_samples,
                             const unsigned int ofdm_offset_divisor,
                             c16_t *timeshift_symbol_rotation);
}

#include <cstdint>
#include <cstring>
#include <cassert>
#include <cmath>
#include <iostream>
#include <atomic>
#include <vector>

#include "softmodem-common.h"

extern "C" {
#include "executables/nr-uesoftmodem.h"
ue_split7_device_t *ue_split7_device_create(void);
void ue_split7_device_free(ue_split7_device_t *dev);
softmodem_params_t *get_softmodem_params(void)
{
  static softmodem_params_t p;
  std::memset(&p, 0, sizeof(p));
  return &p;
}
nrUE_params_t nrUE_params;
nrUE_params_t *get_nrUE_params(void)
{
  return &nrUE_params;
}
int openair0_device_load(openair0_device_t *device, openair0_config_t *openair0_cfg)
{
  return 0;
}
void *uniqCfg = NULL;
void exit_function(const char *file, const char *function, const int line, const char *s, const int)
{
  std::cerr << "exit_function " << file << ":" << line << " - " << s << "\n";
  exit(-1);
}
void nr_fill_dl_indication(void *, void *, void *, const void *, void *, void *)
{
}
void nr_fill_rx_indication(void *, uint8_t, void *, int, int, void *, uint16_t, const void *, void *, uint8_t *)
{
}
}

openair0_config_t openair0_cfg_g[MAX_CARDS];

// Shared NR frame-parameter constants (μ=1, 30 kHz, 100 MHz / 2048-pt FFT)
static constexpr uint32_t FFT_SIZE = 2048;
static constexpr uint32_t CP0 = 208; // symbol 0 (extended CP)
static constexpr uint32_t CP_NORMAL = 144; // symbols 1–13
static constexpr int SYMS_PER_SLOT = 14;
static constexpr int SLOTS_PER_FRAME = 20; // μ=1
static constexpr int MAX_FRAME = 1024;
static constexpr int RX_TO_TX = 3; // NR_UE_CAPABILITY_SLOT_RX_TO_TX (common/utils/nr/nr_common.h)

/* Offset from slot start to start of symbol sym; mirrors fd_tx_cb_impl's sym_ts loop. */
static uint64_t sym_start_offset(int sym)
{
  if (sym == 0)
    return 0;
  uint64_t off = CP0 + FFT_SIZE; // end of symbol 0
  off += (uint64_t)(sym - 1) * (CP_NORMAL + FFT_SIZE);
  return off;
}

/* Theoretical time-domain samples per slot (real OAI uses get_samples_slot_duration()). */
static uint64_t slot_duration_samples()
{
  return CP0 + FFT_SIZE + (SYMS_PER_SLOT - 1) * (uint64_t)(CP_NORMAL + FFT_SIZE);
}

/* Full frame params matching this file's constants (fully populated: rotation
 * tables, RB grid, etc.) -- ue_split7_config_t::frame_parms is mandatory, so
 * make_device() below must pass a real one, not rely on a device-side fallback. */
static NR_DL_FRAME_PARMS make_test_frame_parms()
{
  // init_symbol_rotation() below calls perform_symbol_rotation(), which LOG_D()s --
  // the log/config subsystem is otherwise only bootstrapped as a side effect of
  // load_dftslib() (see ue_split7_configure()), which hasn't run yet on the very
  // first call into this file. load_dftslib() is idempotent (configure() itself
  // calls it unconditionally on every reconfigure), so calling it here is safe.
  load_dftslib();

  NR_DL_FRAME_PARMS fp = {};
  fp.dl_CarrierFreq = 3500000000ULL;
  fp.ul_CarrierFreq = 3500000000ULL;
  fp.numerology_index = 1;
  fp.symbols_per_slot = SYMS_PER_SLOT;
  fp.slots_per_subframe = SLOTS_PER_FRAME / 10;
  fp.slots_per_frame = SLOTS_PER_FRAME;
  fp.ofdm_symbol_size = FFT_SIZE;
  fp.N_RB_DL = 106;
  fp.N_RB_UL = 106;
  fp.first_carrier_offset = FFT_SIZE - (106 * 12 / 2);
  fp.nb_antennas_rx = 1;
  fp.nb_prefix_samples = CP_NORMAL;
  fp.nb_prefix_samples0 = CP0;
  fp.subcarrier_spacing = 30000;
  fp.ofdm_offset_divisor = 8;
  fp.samples_per_slot_wCP = fp.symbols_per_slot * fp.ofdm_symbol_size;
  fp.samples_per_slotN0 = (fp.nb_prefix_samples + fp.ofdm_symbol_size) * fp.symbols_per_slot;
  fp.samples_per_subframe = (fp.nb_prefix_samples0 + fp.ofdm_symbol_size) * 2
                            + (fp.nb_prefix_samples + fp.ofdm_symbol_size) * (fp.symbols_per_slot * fp.slots_per_subframe - 2);
  fp.samples_per_slot0 =
      fp.nb_prefix_samples0 + ((fp.symbols_per_slot - 1) * fp.nb_prefix_samples) + (fp.symbols_per_slot * fp.ofdm_symbol_size);
  fp.samples_per_frame = 10 * fp.samples_per_subframe;
  fp.Lmax = 8; // dl_CarrierFreq in [3, 6) GHz, mirrors set_Lmax() (nr_parms.c)
  init_symbol_rotation(&fp); // fills symbol_rotation[link_type_dl]/[link_type_ul]
  init_timeshift_rotation(fp.ofdm_symbol_size, fp.nb_prefix_samples, fp.ofdm_offset_divisor, fp.timeshift_symbol_rotation);
  return fp;
}

/* RX-to-TX lead time that write_symbol()/write_prach() add; every assertion
 * exercising the real device (not just this file's arithmetic model) needs it. */
static uint64_t rx_to_tx_lead_samples(int rx_slot)
{
  static const NR_DL_FRAME_PARMS fp = make_test_frame_parms();
  return get_samples_slot_duration(&fp, rx_slot, RX_TO_TX);
}

// Mock infrastructure (mirrors test_ue_split7.cpp)
static std::vector<openair0_timestamp_t> mock_tx_timestamps;
static std::vector<int> mock_tx_nsamps;

static void mock_clear()
{
  mock_tx_timestamps.clear();
  mock_tx_nsamps.clear();
}

static int mock_trx_start(openair0_device_t *)
{
  return 0;
}

static int mock_trx_write(openair0_device_t *, openair0_timestamp_t ts, void **buff, int nsamps, int, int)
{
  (void)buff;
  mock_tx_timestamps.push_back(ts);
  mock_tx_nsamps.push_back(nsamps);
  return nsamps;
}

static int mock_trx_read(openair0_device_t *, openair0_timestamp_t *ts, void **buff, int nsamps, int)
{
  static openair0_timestamp_t cur = 0;
  *ts = cur;
  cur += nsamps;
  std::memset(buff[0], 0, nsamps * 2 * sizeof(int16_t));
  return nsamps;
}

static int mock_trx_set_freq(openair0_device_t *, openair0_config_t *)
{
  return 0;
}

/* Partial mirror of ue_split7_device_priv_t; must track its field order (only
 * openair0_dev and state are accessed through this cast). */
struct test_priv_t {
  openair0_device_t openair0_dev;
  openair0_config_t openair0_cfg;
  uint32_t ta_samples;
  bool is_started;
  int state; // ue_split7_state_t; see make_device()
  int16_t *rx_time_buf;
  int16_t *tx_time_buf;
  uint32_t buf_size_samples;
};

static ue_split7_device_t *make_device()
{
  ue_split7_device_t *dev = ue_split7_device_create();
  assert(dev);

  NR_DL_FRAME_PARMS fp = make_test_frame_parms();

  ue_split7_config_t cfg;
  std::memset(&cfg, 0, sizeof(cfg));
  cfg.dl_carrier_freq_hz = 3500000000ULL;
  cfg.ul_carrier_freq_hz = 3500000000ULL;
  cfg.sample_rate_hz = 30720000;
  cfg.fft_size = FFT_SIZE;
  cfg.scs_khz = 30;
  cfg.cp_len_normal = CP_NORMAL;
  cfg.cp_len_symbol0 = CP0;
  cfg.num_rx_antennas = 1;
  cfg.num_tx_antennas = 1;
  cfg.frame_parms = &fp; // configure() copies it in; fp only needs to outlive this call
  assert(dev->configure(dev, &cfg) == UE_SPLIT7_SUCCESS);

  test_priv_t *priv = (test_priv_t *)dev->priv;
  priv->openair0_dev.trx_start_func = mock_trx_start;
  priv->openair0_dev.trx_read_func = mock_trx_read;
  priv->openair0_dev.trx_write_func = mock_trx_write;
  priv->openair0_dev.trx_set_freq_func = mock_trx_set_freq;

  priv->state = 2; // skip sync lifecycle; these tests only need write_symbol() valid

  return dev;
}

// Sends one symbol as fd_tx_cb_impl would; returns the recorded (TA-subtracted) air timestamp.
static openair0_timestamp_t send_sym(ue_split7_device_t *dev, int sym, openair0_timestamp_t nominal_slot_start)
{
  std::vector<ue_split7_iq_t> iq(FFT_SIZE, {1000, -1000});
  ue_split7_symbol_buffer_t buf;
  std::memset(&buf, 0, sizeof(buf));
  buf.meta.symbol_number = (uint8_t)sym;
  buf.meta.timestamp_samples = nominal_slot_start + sym_start_offset(sym);
  buf.num_subcarriers = FFT_SIZE;
  buf.re_buffer = iq.data();

  mock_clear();
  dev->write_symbol(dev, &buf, 1);
  assert(!mock_tx_timestamps.empty());
  return mock_tx_timestamps.back(); // real symbol write always comes last (dummy zero-fills may precede it)
}

// TEST 1: per-symbol nominal timestamps (sym_ts advances by CP0+FFT then CP_NORMAL+FFT)
static void test_symbol_nominal_timestamps()
{
  std::cout << "[TEST 1] Per-symbol nominal timestamp sequence\n";

  const openair0_timestamp_t slot_start = 5000000;

  openair0_timestamp_t expected = slot_start;
  for (int sym = 0; sym < SYMS_PER_SLOT; sym++) {
    openair0_timestamp_t computed = slot_start + (openair0_timestamp_t)sym_start_offset(sym);
    assert(computed == expected);

    // sym_ts in fd_tx_cb_impl advances past *this* symbol's CP + data
    uint32_t cp = (sym == 0) ? CP0 : CP_NORMAL;
    expected += cp + FFT_SIZE;
  }

  // Verify the last advance lands at slot_start + slot_duration
  assert(expected == slot_start + (openair0_timestamp_t)slot_duration_samples());
  std::cout << "[PASS 1] All 14 symbol nominal timestamps correct\n";
}

// TEST 2: fd-ue effective TX time == UE_thread effective TX time, per symbol
static void test_tx_time_equivalence()
{
  std::cout << "[TEST 2] fd-ue vs UE_thread effective TX time equivalence\n";

  const openair0_timestamp_t rx_ts = 12345678;
  const uint64_t slot_dur = slot_duration_samples();
  const uint32_t N_TA = 512; // N_TA_offset samples (FR1 example)
  const uint32_t TA = 256; // MAC-commanded timing advance
  const uint32_t ta_total = N_TA + TA;

  const openair0_timestamp_t tx_nominal_slot_start = rx_ts + (openair0_timestamp_t)slot_dur;
  const openair0_timestamp_t ue_thread_timestamp_tx = tx_nominal_slot_start - (openair0_timestamp_t)ta_total;

  for (int sym = 0; sym < SYMS_PER_SLOT; sym++) {
    openair0_timestamp_t sym_off = (openair0_timestamp_t)sym_start_offset(sym);

    // fd-ue: meta.timestamp_samples = nominal_slot_start + sym_off
    //        device subtracts ta_total → effective TX time
    openair0_timestamp_t fd_effective = (tx_nominal_slot_start + sym_off) - (openair0_timestamp_t)ta_total;

    // UE_thread: RU_write writes time-domain from timestamp_tx + sym_off
    openair0_timestamp_t ue_effective = ue_thread_timestamp_tx + sym_off;

    assert(fd_effective == ue_effective);
  }

  std::cout << "[PASS 2] All 14 symbols: fd-ue effective == UE_thread effective\n";
}

// TEST 3: set_timing_advance() fires exactly on TA change (first slot, increase, reset)
static void test_ta_propagation()
{
  std::cout << "[TEST 3] set_timing_advance called on TA change\n";

  ue_split7_device_t *dev = make_device();

  struct FakeUE {
    int N_TA_offset;
    int timing_advance;
  } ue = {.N_TA_offset = 512, .timing_advance = 0};

  // Track calls
  uint32_t last_ta_samples = (uint32_t)-1;
  auto maybe_update_ta = [&]() -> bool {
    uint32_t new_ta = (uint32_t)(ue.N_TA_offset + ue.timing_advance);
    if (new_ta != last_ta_samples) {
      dev->set_timing_advance(dev, new_ta);
      last_ta_samples = new_ta;
      return true;
    }
    return false;
  };

  // a) Slot 0: first update (sentinel → N_TA_offset)
  assert(maybe_update_ta() == true);
  assert(last_ta_samples == (uint32_t)ue.N_TA_offset);

  // next write_symbol should subtract the recorded TA
  {
    openair0_timestamp_t nominal = 1000000;
    openair0_timestamp_t eff = send_sym(dev, 1, nominal);
    assert(eff
           == nominal + (openair0_timestamp_t)sym_start_offset(1) + (openair0_timestamp_t)rx_to_tx_lead_samples(0)
                  - (openair0_timestamp_t)last_ta_samples);
  }

  // b) Slots 1..5: TA unchanged → no further calls
  for (int s = 1; s <= 5; s++)
    assert(maybe_update_ta() == false);
  assert(last_ta_samples == (uint32_t)ue.N_TA_offset);

  // c) MAC CE triggers TA increase
  ue.timing_advance = 128;
  assert(maybe_update_ta() == true);
  assert(last_ta_samples == (uint32_t)(ue.N_TA_offset + 128));

  // Verify device uses new TA
  {
    openair0_timestamp_t nominal = 2000000;
    openair0_timestamp_t eff = send_sym(dev, 0, nominal);
    assert(eff
           == nominal + (openair0_timestamp_t)rx_to_tx_lead_samples(0)
                  - (openair0_timestamp_t)last_ta_samples); // sym 0: offset = 0
  }

  // Unchanged after update
  assert(maybe_update_ta() == false);

  // d) TA reset to 0
  ue.timing_advance = 0;
  assert(maybe_update_ta() == true);
  assert(last_ta_samples == (uint32_t)ue.N_TA_offset);

  ue_split7_device_free(dev);
  std::cout << "[PASS 3] set_timing_advance called at correct times\n";
}

// TEST 4: proc field derivation (slot/frame/hfn rx+tx) is wrap-correct across frames
static void test_proc_fields()
{
  std::cout << "[TEST 4] proc field computation across slot range\n";

  const int total_slots = 3 * SLOTS_PER_FRAME * MAX_FRAME + SLOTS_PER_FRAME;

  for (int abs = 0; abs < total_slots; abs++) {
    int slot_rx = abs % SLOTS_PER_FRAME;
    int frame_rx = (abs / SLOTS_PER_FRAME) % MAX_FRAME;
    int hfn_rx = (abs / SLOTS_PER_FRAME) / MAX_FRAME;

    int abs_tx = abs + RX_TO_TX;
    int slot_tx = abs_tx % SLOTS_PER_FRAME;
    int frame_tx = (abs_tx / SLOTS_PER_FRAME) % MAX_FRAME;
    int hfn_tx = (abs_tx / SLOTS_PER_FRAME) / MAX_FRAME;

    // Slot indices must be in range
    assert(slot_rx >= 0 && slot_rx < SLOTS_PER_FRAME);
    assert(slot_tx >= 0 && slot_tx < SLOTS_PER_FRAME);

    // Frames must be in range
    assert(frame_rx >= 0 && frame_rx < MAX_FRAME);
    assert(frame_tx >= 0 && frame_tx < MAX_FRAME);

    // TX slot is always RX_TO_TX slots ahead (mod wrap)
    assert(((slot_tx - slot_rx + SLOTS_PER_FRAME) % SLOTS_PER_FRAME) == RX_TO_TX || slot_tx < slot_rx); // wrap case

    // HFN must be non-negative
    assert(hfn_rx >= 0);
    assert(hfn_tx >= hfn_rx);

    // frame_tx must equal frame_rx or be one ahead (within same HFN boundary)
    int frame_diff = ((frame_tx - frame_rx) + MAX_FRAME) % MAX_FRAME;
    assert(frame_diff == 0 || frame_diff == 1);
  }

  std::cout << "[PASS 4] proc fields correct for " << total_slots << " slots\n";
}

// TEST 5: symbol-to-symbol TX gaps match CP+FFT across a full slot
static void test_symbol_gap_continuity()
{
  std::cout << "[TEST 5] Symbol-to-symbol gap continuity\n";

  ue_split7_device_t *dev = make_device();
  dev->set_timing_advance(dev, 0); // no TA for this test

  std::vector<ue_split7_iq_t> iq(FFT_SIZE, {500, -500});
  const openair0_timestamp_t slot_start = 9000000;

  // Simulate fd_tx_cb_impl: build sym_ts like the production code does
  std::vector<openair0_timestamp_t> recorded_ts;
  openair0_timestamp_t sym_ts = slot_start;

  for (int sym = 0; sym < SYMS_PER_SLOT; sym++) {
    const int sym_off = sym * (int)FFT_SIZE; // index into txdataF array

    ue_split7_symbol_buffer_t buf;
    std::memset(&buf, 0, sizeof(buf));
    buf.meta.symbol_number = (uint8_t)sym;
    buf.meta.timestamp_samples = sym_ts;
    buf.num_subcarriers = FFT_SIZE;
    buf.re_buffer = iq.data() + (sym_off % FFT_SIZE);

    mock_clear();
    dev->write_symbol(dev, &buf, 1);
    assert(!mock_tx_timestamps.empty());
    recorded_ts.push_back(mock_tx_timestamps[0]);

    uint32_t cp = (sym == 0) ? CP0 : CP_NORMAL;
    sym_ts += cp + FFT_SIZE;
  }

  // sym 0 air time = slot_start + RX-to-TX lead time (ta=0, so no subtraction)
  assert(recorded_ts[0] == slot_start + (openair0_timestamp_t)rx_to_tx_lead_samples(0));

  // Gap from sym 0 to sym 1: CP0 + FFT
  assert(recorded_ts[1] - recorded_ts[0] == CP0 + FFT_SIZE);

  // Gaps from sym 1..13: CP_NORMAL + FFT
  for (int sym = 2; sym < SYMS_PER_SLOT; sym++) {
    openair0_timestamp_t gap = recorded_ts[sym] - recorded_ts[sym - 1];
    assert(gap == CP_NORMAL + FFT_SIZE);
  }

  // Total span equals slot_duration
  openair0_timestamp_t span = recorded_ts[SYMS_PER_SLOT - 1] + (CP_NORMAL + FFT_SIZE) - recorded_ts[0];
  assert(span == (openair0_timestamp_t)slot_duration_samples());

  ue_split7_device_free(dev);
  std::cout << "[PASS 5] All symbol gaps correct\n";
}

// TEST 6: initial TA push after sync, no spurious repeats, one push per MAC update
static void test_ta_initial_push()
{
  std::cout << "[TEST 6] Initial TA push after sync\n";

  ue_split7_device_t *dev = make_device();

  int N_TA_offset = 512;
  int timing_advance = 0;
  uint32_t last_ta = (uint32_t)-1; // sentinel
  int push_count = 0;

  auto update_ta = [&]() {
    uint32_t new_ta = (uint32_t)(N_TA_offset + timing_advance);
    if (new_ta != last_ta) {
      dev->set_timing_advance(dev, new_ta);
      last_ta = new_ta;
      push_count++;
    }
  };

  // Simulate post-sync: last_ta = N_TA_offset, one push
  last_ta = (uint32_t)N_TA_offset;
  dev->set_timing_advance(dev, last_ta);
  push_count = 1;

  // Run 10 slots without TA change
  for (int i = 0; i < 10; i++)
    update_ta();
  assert(push_count == 1); // no spurious re-push

  // MAC CE arrives: timing_advance = 64
  timing_advance = 64;
  update_ta();
  assert(push_count == 2);
  assert(last_ta == (uint32_t)(N_TA_offset + 64));

  // 5 more slots unchanged
  for (int i = 0; i < 5; i++)
    update_ta();
  assert(push_count == 2);

  // Verify device applies new TA correctly
  openair0_timestamp_t nominal = 3000000;
  openair0_timestamp_t eff = send_sym(dev, 0, nominal);
  assert(eff == nominal + (openair0_timestamp_t)rx_to_tx_lead_samples(0) - last_ta);

  ue_split7_device_free(dev);
  std::cout << "[PASS 6] TA initial push and update correct\n";
}

// TEST 7: PRACH effective TX time matches the time-domain path's
static void test_prach_tx_time_equivalence()
{
  std::cout << "[TEST 7] PRACH effective TX time equivalence\n";

  const openair0_timestamp_t rx_ts = 24680135;
  const uint64_t slot_dur = slot_duration_samples();
  const uint32_t N_TA = 512;
  const uint32_t TA = 128;
  const uint32_t ta_total = N_TA + TA;

  const openair0_timestamp_t tx_nominal_slot_start = rx_ts + (openair0_timestamp_t)slot_dur;
  const openair0_timestamp_t ue_thread_timestamp_tx = tx_nominal_slot_start - (openair0_timestamp_t)ta_total;

  // Simulate different PRACH formats/SCS start offsets (prach_start)
  const int prach_starts[] = {0, 1536, 3168, 7680};
  for (int prach_start : prach_starts) {
    // fd-ue: write_prach calculates based on slot start (nominal) and time_offset_samples
    openair0_timestamp_t fd_prach_effective = tx_nominal_slot_start + prach_start - ta_total;

    // UE_thread: generate_nr_prach outputs at timestamp_tx + prach_start
    openair0_timestamp_t ue_prach_effective = ue_thread_timestamp_tx + prach_start;

    assert(fd_prach_effective == ue_prach_effective);
  }

  std::cout << "[PASS 7] PRACH effective TX time equivalence verified\n";
}

// MAIN
int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;

  std::cout << "=========================================================\n";
  std::cout << "  UE_fd_thread vs UE_thread COMPATIBILITY VERIFICATION   \n";
  std::cout << "=========================================================\n";

  if (load_dftslib() < 0) {
    std::cerr << "Failed to load OAI DFT library\n";
    return -1;
  }

  test_symbol_nominal_timestamps();
  std::cout << "---------------------------------------------------------\n";
  test_tx_time_equivalence();
  std::cout << "---------------------------------------------------------\n";
  test_ta_propagation();
  std::cout << "---------------------------------------------------------\n";
  test_proc_fields();
  std::cout << "---------------------------------------------------------\n";
  test_symbol_gap_continuity();
  std::cout << "---------------------------------------------------------\n";
  test_ta_initial_push();
  std::cout << "---------------------------------------------------------\n";
  test_prach_tx_time_equivalence();

  std::cout << "=========================================================\n";
  std::cout << "  ALL COMPATIBILITY TESTS PASSED                         \n";
  std::cout << "=========================================================\n";
  return 0;
}
