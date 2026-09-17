/**
 * @file test_ue_split7.cpp
 * @brief Unit tests for the UE-centric 7.1 functional split interface.
 *
 * Every device here registers exactly one client (client_id 0, from
 * register_client()) -- with a single client, write_symbols()/skip_symbols()'s
 * per-symbol combine set is always just that one client, so every call
 * finalizes (rotate+IDFT+CP+PRACH-overlay+TX) immediately. Multi-client
 * combining itself is covered separately by test_multi_client_combine() below.
 */

#include "ue_split7_interface.h"

#include "common_lib.h"
#include "PHY/TOOLS/tools_defs.h"
extern "C" {
void generate_pss_nr_time(int ofdm_symbol_size, int first_carrier_offset, const int N_ID_2, int ssbFirstSCS, c16_t *pssTime);
}
#include <atomic>
#include <iostream>
#include <vector>
#include <cmath>
#include <cstring>
#include <cassert>
#include <complex>
#include <thread>
#include <chrono>

#include "softmodem-common.h"

extern "C" {
// Declared directly rather than via PHY/MODULATION/nr_modulation.h: that header
// uses C99 VLA-in-parameter syntax elsewhere that GCC's C++ frontend rejects.
void init_symbol_rotation(NR_DL_FRAME_PARMS *fp);
void init_timeshift_rotation(const int ofdm_symbol_size,
                             const int nb_prefix_samples,
                             const unsigned int ofdm_offset_divisor,
                             c16_t *timeshift_symbol_rotation);
#include "executables/nr-uesoftmodem.h"
#include "common/utils/threadPool/thread-pool.h"

softmodem_params_t *get_softmodem_params(void)
{
  static softmodem_params_t params;
  std::memset(&params, 0, sizeof(params));
  return &params;
}
int openair0_device_load(openair0_device_t *device, openair0_config_t *openair0_cfg)
{
  return 0;
}
void *uniqCfg = NULL;
void exit_function(const char *file, const char *function, const int line, const char *s, const int assert)
{
  std::cerr << "Exit function called from " << file << ":" << line << " - " << s << std::endl;
  exit(-1);
}
void nr_fill_dl_indication(void *dl_ind, void *dci_ind, void *rx_ind, const void *proc, void *ue, void *phy_data)
{
}
void nr_fill_rx_indication(void *rx_ind,
                           uint8_t pdu_type,
                           void *ue,
                           int cw_idx,
                           int harq_pid,
                           void *dlsch,
                           uint16_t n_pdus,
                           const void *proc,
                           void *typeSpecific,
                           uint8_t *b)
{
}
nrUE_params_t nrUE_params;
nrUE_params_t *get_nrUE_params(void)
{
  return &nrUE_params;
}
}

openair0_config_t openair0_cfg_g[MAX_CARDS];

// Forward declare device creation/deletion from ue_split7_device.c
extern "C" {
ue_split7_device_t *ue_split7_device_create(void);
void ue_split7_device_free(ue_split7_device_t *dev);
// Partial mirror of ue_split7_device_priv_t's leading fields; must track its field order.
typedef struct {
  openair0_device_t openair0_dev;
  openair0_config_t openair0_cfg;
  uint32_t ta_samples;
  bool is_started;
  int state; // ue_split7_state_t
  int16_t *rx_time_buf;
  int16_t *tx_time_buf;
  uint32_t buf_size_samples;
  int16_t **rx_circ_buf;
  uint32_t circ_buf_size;
  uint64_t latest_write_ts; // _Atomic uint64_t; same layout as plain uint64_t
  uint64_t next_read_ts;
  uint32_t rx_fd_ring_symbols;
  c16_t **rx_fd_buf;
  uint32_t rx_td_buf_samples;
  c16_t **rx_td_buf;
  bool *symbol_fft_done;
  uint32_t *symbol_frame_number;
} test_priv_t;
}

// Full frame params for this file's shared test config (3.5 GHz, 30.72 MHz, 2048-pt
// FFT, 106 RB @ 30 kHz SCS). ue_split7_config_t::frame_parms is mandatory -- the
// device has no standalone derivation fallback -- so every configure() call below
// must pass a real one.
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
  fp.symbols_per_slot = 14;
  fp.slots_per_subframe = 2;
  fp.slots_per_frame = 20;
  fp.ofdm_symbol_size = 2048;
  fp.N_RB_DL = 106;
  fp.N_RB_UL = 106;
  fp.first_carrier_offset = 2048 - (106 * 12 / 2);
  fp.nb_antennas_rx = 1;
  fp.nb_prefix_samples = 144;
  fp.nb_prefix_samples0 = 208;
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

static std::vector<int16_t> mock_tx_channel_buffer;
static std::vector<int16_t> mock_rx_channel_buffer;
static openair0_timestamp_t last_tx_timestamp = 0;

static int mock_trx_start(openair0_device_t *device)
{
  return 0;
}

static void mock_trx_write_clear()
{
  mock_tx_channel_buffer.clear();
  last_tx_timestamp = 0;
}

static int mock_trx_write(openair0_device_t *device, openair0_timestamp_t timestamp, void **buff, int nsamps, int cc, int flags)
{
  last_tx_timestamp = timestamp;
  int16_t *src = (int16_t *)buff[0];
  size_t prev_size = mock_tx_channel_buffer.size();
  mock_tx_channel_buffer.resize(prev_size + nsamps * 2);
  std::memcpy(&mock_tx_channel_buffer[prev_size], src, nsamps * 2 * sizeof(int16_t));
  return nsamps;
}

static int mock_trx_read(openair0_device_t *device, openair0_timestamp_t *timestamp, void **buff, int nsamps, int cc)
{
  static openair0_timestamp_t mock_ts = 1000000;
  *timestamp = mock_ts;
  mock_ts += nsamps;
  int16_t *dest = (int16_t *)buff[0];

  if (mock_rx_channel_buffer.size() >= (size_t)nsamps * 2) {
    std::memcpy(dest, mock_rx_channel_buffer.data(), nsamps * 2 * sizeof(int16_t));
    mock_rx_channel_buffer.erase(mock_rx_channel_buffer.begin(), mock_rx_channel_buffer.begin() + nsamps * 2);
  } else {
    std::memset(dest, 0, nsamps * 2 * sizeof(int16_t));
  }
  return nsamps;
}

static int mock_set_freq_count = 0;
static double mock_last_set_rx_freq = 0.0;
static double mock_last_set_tx_freq = 0.0;

static int mock_trx_set_freq(openair0_device_t *device, openair0_config_t *openair0_cfg)
{
  mock_set_freq_count++;
  mock_last_set_rx_freq = openair0_cfg->rx_freq[0];
  mock_last_set_tx_freq = openair0_cfg->tx_freq[0];
  return 0;
}

// --------------------------------------------------------------------------
// TEST CASE 1: Loopback FFT/IFFT & CP insertion/removal verification
// --------------------------------------------------------------------------
void test_loopback_fft_ifft(ue_split7_device_t *dev)
{
  std::cout << "[TEST] 1. Starting FFT/IFFT loopback test..." << std::endl;

  uint32_t fft_size = dev->config.fft_size;
  uint32_t cp_len = dev->config.cp_len_normal;

  // write_symbols()/read_symbols() apply the 38.211 §5.3 symbol-rotation only to the
  // active RE region; guard-band bins are intentionally left untouched, so only
  // populate/verify the active band here.
  uint32_t N_RB_DL = dev->config.N_RB_DL > 0 ? dev->config.N_RB_DL : 106;
  uint32_t first_carrier_offset = fft_size - N_RB_DL * 6;
  auto in_active_band = [&](uint32_t i) { return i < N_RB_DL * 6 || i >= first_carrier_offset; };

  std::vector<c16_t> tx_re(fft_size, {0, 0});

  for (uint32_t i = 0; i < fft_size; ++i) {
    if (!in_active_band(i))
      continue;
    tx_re[i].r = (i % 2 == 0) ? 1000 : -1000;
    tx_re[i].i = (i % 3 == 0) ? 1000 : -1000;
  }
  // write_symbols() rotates the buffer in place, so keep a pristine copy for comparison.
  const std::vector<c16_t> tx_re_orig = tx_re;

  c16_t *tx_bufs[1] = {tx_re.data()};

  c16_t *rx_bufs[1] = {nullptr};

  mock_trx_write_clear();

  ue_split7_status_t rc = dev->write_symbols(dev, 0, 0, 0, 1, 1, tx_bufs, 1); // symbol 1: normal CP
  assert(rc == UE_SPLIT7_SUCCESS);
  assert(mock_tx_channel_buffer.size() == (fft_size + cp_len) * 2);

  mock_rx_channel_buffer = mock_tx_channel_buffer;

  rc = dev->read_symbols(dev, 0, 0, 1, 1, rx_bufs, 1);
  assert(rc == UE_SPLIT7_SUCCESS);
  assert(rx_bufs[0] != nullptr);
  const c16_t *rx_re = rx_bufs[0];

  // The IFFT/CP round trip leaves a single complex gain H from fixed-point
  // quantization; estimate it from RE 0 and normalize every RE by it.
  double error_sum = 0.0;
  double signal_energy = 0.0;
  uint32_t quadrant_errors = 0;

  const double tx0_r = tx_re_orig[0].r, tx0_i = tx_re_orig[0].i;
  const double tx0_mag2 = tx0_r * tx0_r + tx0_i * tx0_i;
  const double H_r = (rx_re[0].r * tx0_r + rx_re[0].i * tx0_i) / tx0_mag2;
  const double H_i = (rx_re[0].i * tx0_r - rx_re[0].r * tx0_i) / tx0_mag2;
  const double H_mag2 = H_r * H_r + H_i * H_i;
  std::cout << "   Complex loopback gain H = (" << H_r << ", " << H_i << ")" << std::endl;

  auto normalize = [&](uint32_t i, double &rx_r_norm, double &rx_i_norm) {
    double rr = rx_re[i].r, ri = rx_re[i].i;
    rx_r_norm = (rr * H_r + ri * H_i) / H_mag2;
    rx_i_norm = (ri * H_r - rr * H_i) / H_mag2;
  };

  uint32_t active_res_checked = 0;
  for (uint32_t i = 0; i < fft_size; ++i) {
    if (!in_active_band(i))
      continue;
    active_res_checked++;

    double tx_r = tx_re_orig[i].r;
    double tx_i = tx_re_orig[i].i;

    double rx_r_norm, rx_i_norm;
    normalize(i, rx_r_norm, rx_i_norm);

    if ((tx_r >= 0) != (rx_r_norm >= 0))
      quadrant_errors++;
    if ((tx_i >= 0) != (rx_i_norm >= 0))
      quadrant_errors++;

    double err_r = rx_r_norm - tx_r;
    double err_i = rx_i_norm - tx_i;

    error_sum += err_r * err_r + err_i * err_i;
    signal_energy += tx_r * tx_r + tx_i * tx_i;
  }
  std::cout << "   Active REs checked: " << active_res_checked << " / " << fft_size << std::endl;

  std::cout << "   First 10 symbols comparison (Compensated):" << std::endl;
  for (int i = 0; i < 10; ++i) {
    double rx_r_norm, rx_i_norm;
    normalize(i, rx_r_norm, rx_i_norm);
    std::cout << "     [" << i << "] TX: (" << tx_re_orig[i].r << ", " << tx_re_orig[i].i << ") | RX (Norm): (" << rx_r_norm << ", "
              << rx_i_norm << ")" << std::endl;
  }

  double evm = std::sqrt(error_sum / signal_energy) * 100.0;
  std::cout << "   Loopback EVM (Compensated): " << evm << "%" << std::endl;
  std::cout << "   QPSK Decision Errors: " << quadrant_errors << " / " << (active_res_checked * 2) << std::endl;

  assert(quadrant_errors == 0); // Must have zero decision boundary errors
  assert(evm < 15.0); // Must be less than 15% EVM after fixed-point Q-channel compensation

  // Multi-symbol batch loopback (symbols 1..4: 4 symbols)
  {
    const uint8_t num_batch_symbols = 4;
    std::vector<c16_t> tx_batch(num_batch_symbols * fft_size, {0, 0});
    for (uint8_t s = 0; s < num_batch_symbols; s++) {
      for (uint32_t i = 0; i < fft_size; ++i) {
        if (!in_active_band(i))
          continue;
        tx_batch[s * fft_size + i].r = ((i + s) % 2 == 0) ? 1000 : -1000;
        tx_batch[s * fft_size + i].i = ((i + s) % 3 == 0) ? 1000 : -1000;
      }
    }
    c16_t *tx_batch_ptrs[1] = {tx_batch.data()};
    mock_trx_write_clear();

    rc = dev->write_symbols(dev, 0, 0, 0, 1, num_batch_symbols, tx_batch_ptrs, 1);
    assert(rc == UE_SPLIT7_SUCCESS);

    mock_rx_channel_buffer = mock_tx_channel_buffer;

    c16_t *rx_batch_ptrs[1] = {nullptr};

    rc = dev->read_symbols(dev, 0, 0, 1, num_batch_symbols, rx_batch_ptrs, 1);
    assert(rc == UE_SPLIT7_SUCCESS);
    assert(rx_batch_ptrs[0] != nullptr);

    for (uint8_t s = 0; s < num_batch_symbols; s++) {
      const c16_t *rx_s = &rx_batch_ptrs[0][s * fft_size];
      const double btx0_r = ((0 + s) % 2 == 0) ? 1000 : -1000;
      const double btx0_i = ((0 + s) % 3 == 0) ? 1000 : -1000;
      const double bmag2 = btx0_r * btx0_r + btx0_i * btx0_i;
      const double bH_r = (rx_s[0].r * btx0_r + rx_s[0].i * btx0_i) / bmag2;
      const double bH_i = (rx_s[0].i * btx0_r - rx_s[0].r * btx0_i) / bmag2;
      const double bH_mag2 = bH_r * bH_r + bH_i * bH_i;

      uint32_t s_quad_errors = 0;
      for (uint32_t i = 0; i < fft_size; ++i) {
        if (!in_active_band(i))
          continue;
        double exp_r = ((i + s) % 2 == 0) ? 1000 : -1000;
        double exp_i = ((i + s) % 3 == 0) ? 1000 : -1000;
        double act_r = (rx_s[i].r * bH_r + rx_s[i].i * bH_i) / bH_mag2;
        double act_i = (rx_s[i].i * bH_r - rx_s[i].r * bH_i) / bH_mag2;
        if ((exp_r >= 0) != (act_r >= 0) || (exp_i >= 0) != (act_i >= 0))
          s_quad_errors++;
      }
      assert(s_quad_errors == 0);
    }
    std::cout << "   Multi-symbol batch (4 symbols): all QPSK decision boundaries matched perfectly." << std::endl;
  }

  std::cout << "[PASS] 1. FFT/IFFT loopback test passed successfully." << std::endl;
}

// --------------------------------------------------------------------------
// TEST CASE 2: Timing Advance (TA) verification
// --------------------------------------------------------------------------
void test_timing_advance(ue_split7_device_t *dev)
{
  std::cout << "[TEST] 2. Starting Timing Advance (TA) verification..." << std::endl;

  uint32_t fft_size = dev->config.fft_size;
  std::vector<c16_t> tx_re(fft_size, {1000, -1000});
  c16_t *tx_bufs[1] = {tx_re.data()};

  uint32_t ta_val = 128;
  ue_split7_status_t rc = dev->set_timing_advance(dev, ta_val);
  assert(rc == UE_SPLIT7_SUCCESS);

  mock_trx_write_clear();
  rc = dev->write_symbols(dev, 0, 0, 0, 1, 1, tx_bufs, 1);
  assert(rc == UE_SPLIT7_SUCCESS);

  // Expected = base_ts (anchor 1000000 + symbol 0 duration 2256) - TA (128).
  // slot_number/frame_number here are already the resolved TX slot, so
  // ts_from_frame_slot_symbol() gives its position directly -- no separate
  // RX-to-TX lead is added on top.
  std::cout << "   Nominal Timestamp: 1002256 | Applied TA: " << ta_val << " | Transmitted Timestamp: " << last_tx_timestamp
            << std::endl;
  assert(last_tx_timestamp == (1000000 + 2256 - ta_val));

  std::cout << "[PASS] 2. Timing Advance (TA) verification passed." << std::endl;
}

// --------------------------------------------------------------------------
// TEST CASE 3: Synchronization Service time-domain cell search verification
// --------------------------------------------------------------------------
static bool sync_callback_called = false;
static ue_split7_status_t sync_callback_status = UE_SPLIT7_ERR_GENERIC;
static ue_split7_sync_result_t sync_callback_result;

static void test_sync_callback(struct ue_split7_device *dev,
                               ue_split7_status_t status,
                               const ue_split7_sync_result_t *result,
                               void *user_data)
{
  sync_callback_called = true;
  sync_callback_status = status;
  if (status == UE_SPLIT7_SUCCESS && result) {
    sync_callback_result = *result;
  }
}

// Regression test: UE_SPLIT7_SUCCESS must require an actual PBCH decode, not just
// PSS detection (see ue_split7_sync_result_t::mib_decoded in ue_split7_interface.h).
// A PSS-only signal should time out, never report false success.
void test_synchronization_service(ue_split7_device_t *dev)
{
  std::cout << "[TEST] 3. Starting Synchronization Service search verification..." << std::endl;

  uint32_t fft_size = dev->config.fft_size;
  uint32_t sample_rate = dev->config.sample_rate_hz;
  uint32_t frame_samples = sample_rate * 0.01; // 10ms frame

  mock_rx_channel_buffer.assign(frame_samples * 2, 0);

  uint32_t target_offset = 15000;
  uint8_t target_nid2 = 1;

  int16_t *pss_time = (int16_t *)aligned_alloc(32, fft_size * 2 * sizeof(int16_t));
  std::memset(pss_time, 0, fft_size * 2 * sizeof(int16_t));

  // PSS only; SSS/PBCH DM-RS/payload are left at zero.
  generate_pss_nr_time(fft_size, 0, target_nid2, 1, (c16_t *)pss_time);
  std::memcpy(&mock_rx_channel_buffer[target_offset * 2], pss_time, fft_size * 2 * sizeof(int16_t));
  free(pss_time);

  ue_split7_sync_config_t sync_cfg;
  std::memset(&sync_cfg, 0, sizeof(sync_cfg));
  sync_cfg.arfcn = 640000;
  sync_cfg.scs_khz = 30;
  sync_cfg.timeout_ms = 1000;
  sync_cfg.expected_pci = -1;

  nrUE_params_t *ue_params = (nrUE_params_t *)get_nrUE_params();
  if (ue_params->Tpool.len_thr == 0) {
    initFloatingCoresTpool(4, &ue_params->Tpool, false, (char *)"Tpool");
  }

  sync_callback_called = false;
  ue_split7_status_t rc = dev->start_sync(dev, &sync_cfg, &ue_params->Tpool, test_sync_callback, nullptr);
  assert(rc == UE_SPLIT7_SUCCESS);

  // Regression check: sample ingestion (latest_write_ts) must keep advancing while
  // the sync search runs, even though it never finds a signal.
  std::cout << "   Scanning for PSS on simulated channel (expecting a timeout: PSS-only, no PBCH)..." << std::endl;
  test_priv_t *priv = (test_priv_t *)dev->priv;
  uint64_t last_write_ts = priv->latest_write_ts;
  int timeout_cnt = 0;
  while (!sync_callback_called && timeout_cnt < 30) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    timeout_cnt++;
    uint64_t write_ts = priv->latest_write_ts;
    assert(write_ts > last_write_ts && "sample ingestion stalled during sync search -- deadlock regression");
    last_write_ts = write_ts;
  }

  assert(sync_callback_called);
  assert(sync_callback_status != UE_SPLIT7_SUCCESS);
  std::cout << "   PSS-only signal correctly did not produce a false sync success (status " << (int)sync_callback_status
            << "), ingestion never stalled." << std::endl;

  std::cout << "[PASS] 3. Synchronization Service correctly requires PBCH, not just PSS." << std::endl;
}

// --------------------------------------------------------------------------
// TEST CASE 5: PRACH Transmission verification
// --------------------------------------------------------------------------
// write_prach() is fire-and-forget: it enqueues the waveform and returns
// immediately (see ue_split7_interface.h). Actually transmitting it requires a
// write_symbols()/skip_symbols() call for whatever symbol(s) it overlaps in
// time -- here, skip_symbols(), since this test only cares about the PRACH
// content, not any concurrent regular UL data. Both sub-cases below choose
// cp_len_samples/repetition_count/time_offset_samples so the waveform exactly
// fills a whole number of consecutive symbols with no partial spillover,
// keeping the byte-for-byte verification below tractable.
static bool any_nonzero(const std::vector<int16_t> &buf)
{
  for (int16_t v : buf) {
    if (v != 0)
      return true;
  }
  return false;
}

void test_prach_transmission(ue_split7_device_t *dev)
{
  std::cout << "[TEST] 5. Starting PRACH Transmission verification..." << std::endl;

  uint32_t num_prach_samples = 139; // short preamble
  std::vector<ue_split7_iq_t> prach_re(num_prach_samples);
  for (uint32_t k = 0; k < num_prach_samples; ++k) {
    prach_re[k].r = (k % 2 == 0) ? 2000 : -2000;
    prach_re[k].i = (k % 3 == 0) ? 2000 : -2000;
  }

  // Symbol 0 of any slot gets the longer CP0 prefix in this test's frame_parms
  // (cp_len_symbol0 == 208; half-subframe length == symbols_per_slot for mu=1,
  // so every slot's own symbol 0 lands on a half-subframe boundary). Choosing
  // cp_len_samples == cp_len_symbol0 with repetition_count 1 makes the PRACH
  // waveform exactly as long as symbol 0's own window (2048 + 208 = 2256), so
  // one skip_symbols() call for that one symbol fully drains it with no spillover.
  ue_split7_prach_tx_params_t params;
  std::memset(&params, 0, sizeof(params));
  params.samples = prach_re.data();
  params.num_samples = num_prach_samples;
  params.frame_number = 0;
  params.time_offset_samples = 0;
  params.cp_len_samples = 208;
  params.slot_number = 2;
  params.symbol_number = 0;
  params.frequency_offset_scs = 100;
  params.repetition_count = 1;
  params.antenna_port = 0;

  uint32_t ta_val = 128;
  dev->set_timing_advance(dev, ta_val);

  mock_trx_write_clear();
  ue_split7_status_t rc = dev->write_prach(dev, 0, &params);
  assert(rc == UE_SPLIT7_SUCCESS);

  // Enqueue-only: nothing transmitted yet.
  assert(mock_tx_channel_buffer.empty());
  std::cout << "   write_prach() enqueued without transmitting, as expected." << std::endl;

  // Drains it: this is the only symbol its (zero time_offset, exactly-one-symbol-
  // long) range overlaps.
  assert(dev->skip_symbols(dev, 0, 0, 2, 0, 1) == UE_SPLIT7_SUCCESS);

  // cp_len_samples + fft_size * repetition_count = 208 + 2048 = 2256 complex samples.
  std::cout << "   Channel Buffer size: " << mock_tx_channel_buffer.size() << " | Expected: " << 2256 * 2 << std::endl;
  assert(mock_tx_channel_buffer.size() == 2256 * 2);
  assert(any_nonzero(mock_tx_channel_buffer));

  // base_ts (anchor 1000000 + slot 2 offset 61504) - TA (128) = 1061376.
  // params.slot_number is already the resolved TX slot, so ts_from_frame_slot_symbol()
  // gives its position directly -- no separate RX-to-TX lead is added on top.
  std::cout << "   Transmitted Timestamp: " << last_tx_timestamp << " | Expected: 1061376" << std::endl;
  assert(last_tx_timestamp == 1061376);

  // Custom fft_size with num_samples == fft_size (1-to-1 mapping), spanning
  // exactly 3 consecutive normal-CP symbols (3 * (6144-sized... no: device fft_size
  // 2048 + cp_len_normal 144) = 3 * 2192 = 6576) starting at symbol 1 (not 0, so
  // every symbol involved is normal-CP, matching cp_len_samples's derivation).
  uint32_t custom_fft_size = 6144;
  std::vector<ue_split7_iq_t> prach_custom(custom_fft_size);
  for (uint32_t k = 0; k < custom_fft_size; ++k) {
    prach_custom[k].r = 500;
    prach_custom[k].i = -500;
  }

  ue_split7_prach_tx_params_t params2;
  std::memset(&params2, 0, sizeof(params2));
  params2.samples = prach_custom.data();
  params2.num_samples = custom_fft_size;
  params2.fft_size = custom_fft_size;
  params2.frame_number = 0;
  params2.time_offset_samples = 0;
  params2.cp_len_samples = 3 * (2048 + 144) - custom_fft_size; // 432
  params2.slot_number = 3;
  params2.symbol_number = 1;
  params2.repetition_count = 1;
  params2.antenna_port = 0;

  mock_trx_write_clear();
  rc = dev->write_prach(dev, 0, &params2);
  assert(rc == UE_SPLIT7_SUCCESS);
  assert(mock_tx_channel_buffer.empty());

  // One skip_symbols() batch spanning all 3 symbols the waveform occupies --
  // internally finalizes them one at a time, same as 3 separate calls.
  assert(dev->skip_symbols(dev, 0, 0, 3, 1, 3) == UE_SPLIT7_SUCCESS);

  assert(mock_tx_channel_buffer.size() == (size_t)(3 * (2048 + 144)) * 2);
  assert(any_nonzero(mock_tx_channel_buffer));
  std::cout << "   Custom FFT PRACH drained across 3 symbols, total size " << mock_tx_channel_buffer.size() << " matches."
            << std::endl;

  std::cout << "[PASS] 5. PRACH Transmission verification passed." << std::endl;
}

// --------------------------------------------------------------------------
// TEST CASE 5b: PRACH content continuity across an overlay_prach() symbol
// boundary, matching production's num_samples==fft_size convention exactly
// (unlike test_prach_transmission()'s two scenarios above, which both exercise
// the frequency_offset_scs-centering ELSE branch -- fd_prach_tx_cb_impl always
// passes num_samples==fft_size==dftlen, so the ELSE branch is never live code).
// Ground truth: PRACH placed exactly at a symbol's own start (single
// finalize_tx_symbol() call). Test: byte-identical waveform, same cp_len_samples,
// placed with a nonzero time_offset_samples so it starts mid-symbol and spills
// into the next -- two finalize_tx_symbol() calls, streamed via
// ue_split7_overlay_prach()'s skip_in_symbol/emitted_samples bookkeeping. The
// PRACH-active bytes extracted from the split capture must exactly match the
// ground truth's bytes.
void test_prach_symbol_span_continuity(ue_split7_device_t *dev)
{
  std::cout << "[TEST] 5b. Starting PRACH symbol-span continuity verification..." << std::endl;

  const uint32_t fft_size = dev->config.fft_size; // 2048
  const uint32_t cp_len = dev->config.cp_len_normal; // 144 (normal-CP symbol)
  std::vector<ue_split7_iq_t> pattern(fft_size);
  for (uint32_t k = 0; k < fft_size; ++k) {
    pattern[k].r = (int16_t)(((k % 5) * 300) - 600); // distinctive, non-constant, non-alternating
    pattern[k].i = (int16_t)(((k % 7) * 200) - 600);
  }

  // ---- Ground truth: PRACH starts exactly at symbol 1 of slot 10 (normal CP). ----
  ue_split7_prach_tx_params_t gt_params;
  std::memset(&gt_params, 0, sizeof(gt_params));
  gt_params.samples = pattern.data();
  gt_params.num_samples = fft_size;
  gt_params.fft_size = fft_size;
  gt_params.time_offset_samples = 0;
  gt_params.cp_len_samples = cp_len;
  gt_params.frame_number = 10;
  gt_params.slot_number = 10;
  gt_params.symbol_number = 1;
  gt_params.repetition_count = 1;

  mock_trx_write_clear();
  assert(dev->write_prach(dev, 0, &gt_params) == UE_SPLIT7_SUCCESS);
  assert(mock_tx_channel_buffer.empty());
  assert(dev->skip_symbols(dev, 0, 10, 10, 1, 1) == UE_SPLIT7_SUCCESS);
  assert(mock_tx_channel_buffer.size() == (cp_len + fft_size) * 2);
  std::vector<int16_t> ground_truth = mock_tx_channel_buffer;

  // ---- Split: same waveform, but nominally offset 500 samples into symbol 1 of
  // slot 11 -- total_samples (cp_len+fft_size=2192) - 500 = 1692 fit in symbol 1,
  // remaining 500 spill into symbol 2. Drain both symbols in one skip_symbols(). ----
  const uint32_t offset = 500;
  ue_split7_prach_tx_params_t split_params = gt_params;
  split_params.time_offset_samples = offset;
  split_params.frame_number = 11;
  split_params.slot_number = 11;

  mock_trx_write_clear();
  assert(dev->write_prach(dev, 0, &split_params) == UE_SPLIT7_SUCCESS);
  assert(mock_tx_channel_buffer.empty());
  assert(dev->skip_symbols(dev, 0, 11, 11, 1, 2) == UE_SPLIT7_SUCCESS);
  assert(mock_tx_channel_buffer.size() == 2 * (cp_len + fft_size) * 2);

  // Extract the PRACH-active region: [offset, offset + cp_len+fft_size) complex
  // samples, i.e. *2 for int16 I/Q pairs.
  std::vector<int16_t> extracted(mock_tx_channel_buffer.begin() + offset * 2,
                                 mock_tx_channel_buffer.begin() + (offset + cp_len + fft_size) * 2);

  // Sanity: nothing before/after the PRACH-active region in the split capture.
  bool prefix_zero = true;
  for (uint32_t i = 0; i < offset * 2; i++)
    if (mock_tx_channel_buffer[i] != 0)
      prefix_zero = false;
  if (!prefix_zero) {
    std::cout << "   prefix NOT zero; first 20 int16 values: ";
    for (int i = 0; i < 20; i++)
      std::cout << mock_tx_channel_buffer[i] << " ";
    std::cout << std::endl;
    std::cout << "   values around offset (samples " << (offset - 5) << ".." << (offset + 5) << "): ";
    for (uint32_t i = (offset - 5) * 2; i < (offset + 5) * 2; i++)
      std::cout << mock_tx_channel_buffer[i] << " ";
    std::cout << std::endl;
  }
  bool suffix_zero = true;
  for (size_t i = (offset + cp_len + fft_size) * 2; i < mock_tx_channel_buffer.size(); i++)
    if (mock_tx_channel_buffer[i] != 0)
      suffix_zero = false;
  assert(prefix_zero);
  assert(suffix_zero);

  bool identical = (extracted == ground_truth);
  if (!identical) {
    size_t first_diff = 0;
    for (; first_diff < extracted.size(); first_diff++)
      if (extracted[first_diff] != ground_truth[first_diff])
        break;
    std::cout << "   MISMATCH at int16 offset " << first_diff << ": extracted=" << extracted[first_diff]
              << " ground_truth=" << ground_truth[first_diff] << std::endl;
    std::cout << "   context extracted:      ";
    for (size_t i = (first_diff > 4 ? first_diff - 4 : 0); i < std::min(extracted.size(), first_diff + 8); i++)
      std::cout << extracted[i] << " ";
    std::cout << std::endl << "   context ground_truth:   ";
    for (size_t i = (first_diff > 4 ? first_diff - 4 : 0); i < std::min(ground_truth.size(), first_diff + 8); i++)
      std::cout << ground_truth[i] << " ";
    std::cout << std::endl;
  }
  assert(identical);

  std::cout << "[PASS] 5b. PRACH symbol-span continuity verification passed." << std::endl;
}

// --------------------------------------------------------------------------
// TEST CASE 5c: same as 5b, but with repetition_count=4 (matching production's
// PRACH format, which repeats the fft_size cycle multiple times) -- 5b only
// covered repetition_count=1, so it never exercised overlay_prach()'s modulo
// repetition-replay (src_idx wrapping within e->fft_size) continuing correctly
// across a finalize_tx_symbol() boundary that does NOT coincide with a
// repetition boundary. total_samples = cp_len + fft_size*4 = 8336, spanning 4
// symbols at offset 0 (ground truth) or 5 symbols at offset 500 (split) --
// several repetition boundaries fall strictly inside a symbol, and at least
// one symbol boundary falls strictly inside a repetition.
// --------------------------------------------------------------------------
void test_prach_repetition_continuity(ue_split7_device_t *dev)
{
  std::cout << "[TEST] 5c. Starting PRACH repetition-continuity verification..." << std::endl;

  const uint32_t fft_size = dev->config.fft_size; // 2048
  const uint32_t cp_len = dev->config.cp_len_normal; // 144 (normal-CP symbol)
  const uint32_t repetition_count = 4;
  const uint32_t total_samples = cp_len + fft_size * repetition_count; // 8336
  std::vector<ue_split7_iq_t> pattern(fft_size);
  for (uint32_t k = 0; k < fft_size; ++k) {
    pattern[k].r = (int16_t)(((k % 5) * 300) - 600);
    pattern[k].i = (int16_t)(((k % 7) * 200) - 600);
  }

  // ---- Ground truth: PRACH starts exactly at symbol 1 of slot 12, 4 repetitions,
  // spans symbols 1..4 (8336 < 4*2192=8768). ----
  ue_split7_prach_tx_params_t gt_params;
  std::memset(&gt_params, 0, sizeof(gt_params));
  gt_params.samples = pattern.data();
  gt_params.num_samples = fft_size;
  gt_params.fft_size = fft_size;
  gt_params.time_offset_samples = 0;
  gt_params.cp_len_samples = cp_len;
  gt_params.frame_number = 12;
  gt_params.slot_number = 12;
  gt_params.symbol_number = 1;
  gt_params.repetition_count = repetition_count;

  mock_trx_write_clear();
  assert(dev->write_prach(dev, 0, &gt_params) == UE_SPLIT7_SUCCESS);
  assert(mock_tx_channel_buffer.empty());
  assert(dev->skip_symbols(dev, 0, 12, 12, 1, 4) == UE_SPLIT7_SUCCESS);
  assert(mock_tx_channel_buffer.size() == 4 * (cp_len + fft_size) * 2);
  std::vector<int16_t> ground_truth(mock_tx_channel_buffer.begin(), mock_tx_channel_buffer.begin() + total_samples * 2);

  // ---- Split: same waveform, offset 500 samples into symbol 1 of slot 13 --
  // spans symbols 1..5 (500+8336=8836 < 5*2192=10960). ----
  const uint32_t offset = 500;
  ue_split7_prach_tx_params_t split_params = gt_params;
  split_params.time_offset_samples = offset;
  split_params.frame_number = 13;
  split_params.slot_number = 13;

  mock_trx_write_clear();
  assert(dev->write_prach(dev, 0, &split_params) == UE_SPLIT7_SUCCESS);
  assert(mock_tx_channel_buffer.empty());
  assert(dev->skip_symbols(dev, 0, 13, 13, 1, 5) == UE_SPLIT7_SUCCESS);
  assert(mock_tx_channel_buffer.size() == 5 * (cp_len + fft_size) * 2);

  std::vector<int16_t> extracted(mock_tx_channel_buffer.begin() + offset * 2,
                                 mock_tx_channel_buffer.begin() + (offset + total_samples) * 2);

  bool prefix_zero = true;
  for (uint32_t i = 0; i < offset * 2; i++)
    if (mock_tx_channel_buffer[i] != 0)
      prefix_zero = false;
  bool suffix_zero = true;
  for (size_t i = (offset + total_samples) * 2; i < mock_tx_channel_buffer.size(); i++)
    if (mock_tx_channel_buffer[i] != 0)
      suffix_zero = false;
  if (!prefix_zero)
    std::cout << "   prefix NOT zero (see values around offset below)" << std::endl;
  if (!suffix_zero)
    std::cout << "   suffix NOT zero" << std::endl;
  assert(prefix_zero);
  assert(suffix_zero);

  bool identical = (extracted == ground_truth);
  if (!identical) {
    size_t first_diff = 0;
    for (; first_diff < extracted.size(); first_diff++)
      if (extracted[first_diff] != ground_truth[first_diff])
        break;
    std::cout << "   MISMATCH at int16 offset " << first_diff << " (sample " << first_diff / 2
              << ") of " << extracted.size() << ": extracted=" << extracted[first_diff]
              << " ground_truth=" << ground_truth[first_diff] << std::endl;
    std::cout << "   context extracted:      ";
    for (size_t i = (first_diff > 8 ? first_diff - 8 : 0); i < std::min(extracted.size(), first_diff + 16); i++)
      std::cout << extracted[i] << " ";
    std::cout << std::endl << "   context ground_truth:   ";
    for (size_t i = (first_diff > 8 ? first_diff - 8 : 0); i < std::min(ground_truth.size(), first_diff + 16); i++)
      std::cout << ground_truth[i] << " ";
    std::cout << std::endl;
    // Count total mismatches too, to distinguish "one bad junction" from
    // "everything after the first symbol is wrong".
    size_t mismatch_count = 0;
    for (size_t i = 0; i < extracted.size(); i++)
      if (extracted[i] != ground_truth[i])
        mismatch_count++;
    std::cout << "   total mismatched int16 values: " << mismatch_count << " / " << extracted.size() << std::endl;
  }
  assert(identical);

  std::cout << "[PASS] 5c. PRACH repetition-continuity verification passed." << std::endl;
}

// --------------------------------------------------------------------------
// TEST CASE 7: sync -> PRACH -> normal-UL state lifecycle
// --------------------------------------------------------------------------
void test_state_lifecycle()
{
  std::cout << "[TEST] 7. Starting state lifecycle verification..." << std::endl;

  ue_split7_device_t *dev = ue_split7_device_create();
  assert(dev != nullptr);

  NR_DL_FRAME_PARMS fp = make_test_frame_parms();

  ue_split7_config_t config;
  std::memset(&config, 0, sizeof(config));
  config.dl_carrier_freq_hz = 3500000000ULL;
  config.ul_carrier_freq_hz = 3500000000ULL;
  config.sample_rate_hz = 30720000;
  config.fft_size = 2048;
  config.scs_khz = 30;
  config.cp_len_normal = 144;
  config.cp_len_symbol0 = 208;
  config.num_rx_antennas = 1;
  config.num_tx_antennas = 1;
  config.frame_parms = &fp; // configure() copies it in; fp only needs to outlive this call

  ue_split7_status_t rc = dev->configure(dev, &config);
  assert(rc == UE_SPLIT7_SUCCESS);

  test_priv_t *priv = (test_priv_t *)dev->priv;
  priv->openair0_dev.trx_start_func = mock_trx_start;
  priv->openair0_dev.trx_read_func = mock_trx_read;
  priv->openair0_dev.trx_write_func = mock_trx_write;
  priv->openair0_dev.trx_set_freq_func = mock_trx_set_freq;

  // write_symbols()'s DFT input requires 32-byte-aligned buffers; std::vector doesn't guarantee that.
  uint32_t fft_size = dev->config.fft_size;
  ue_split7_iq_t *re = (ue_split7_iq_t *)aligned_alloc(32, fft_size * sizeof(ue_split7_iq_t));
  assert(re != nullptr);
  for (uint32_t i = 0; i < fft_size; i++)
    re[i] = {1000, -1000};
  c16_t *tx_bufs[1] = {reinterpret_cast<c16_t *>(re)};

  c16_t *rx_bufs[1] = {nullptr};

  uint32_t client_id = 0;
  assert(dev->register_client(dev, &client_id) == UE_SPLIT7_SUCCESS);
  assert(client_id == 0);

  // Symbol 3, well clear of symbol 0/1 (used by write_symbols below), so this
  // entry's overlay doesn't interact with those calls' transmitted content --
  // this test is about state gating, not the PRACH-overlay mechanism itself
  // (see test_prach_transmission() for that).
  ue_split7_prach_tx_params_t prach_params;
  std::memset(&prach_params, 0, sizeof(prach_params));
  prach_params.samples = re;
  prach_params.num_samples = 139;
  prach_params.frame_number = 0;
  prach_params.cp_len_samples = 144; // symbol 3 has the normal (not symbol-0) CP
  prach_params.repetition_count = 1;
  prach_params.symbol_number = 3;

  // UNSYNCED: nothing is valid yet.
  assert(dev->read_symbols(dev, 0, 0, 1, 1, rx_bufs, 1) == UE_SPLIT7_ERR_STATE);
  assert(dev->write_symbols(dev, client_id, 0, 0, 1, 1, tx_bufs, 1) == UE_SPLIT7_ERR_STATE);
  assert(dev->write_prach(dev, client_id, &prach_params) == UE_SPLIT7_ERR_STATE);
  std::cout << "   UNSYNCED: read/write_symbols/write_prach all rejected, as expected." << std::endl;

  // Drive state directly; this test is about state gating, not sync correctness.
  priv->state = 1; // UE_SPLIT7_STATE_DL_SYNCED
  dev->seed_slot_tracking(dev, client_id, 0);

  // DL_SYNCED: read_symbols valid; UL still isn't.
  assert(dev->read_symbols(dev, 0, 0, 1, 1, rx_bufs, 1) == UE_SPLIT7_SUCCESS);
  assert(dev->write_symbols(dev, client_id, 0, 0, 1, 1, tx_bufs, 1) == UE_SPLIT7_ERR_STATE);
  assert(dev->write_prach(dev, client_id, &prach_params) == UE_SPLIT7_ERR_STATE);
  std::cout << "   DL_SYNCED: read_symbols valid, write_symbols/write_prach still rejected." << std::endl;

  // UL_READY: established by the first set_timing_advance() after sync.
  rc = dev->set_timing_advance(dev, 0);
  assert(rc == UE_SPLIT7_SUCCESS);
  mock_trx_write_clear();
  assert(dev->write_prach(dev, client_id, &prach_params) == UE_SPLIT7_SUCCESS);
  std::cout << "   UL_READY: write_prach valid (enqueued)." << std::endl;

  // write_symbols() doesn't disable PRACH -- both stay valid indefinitely once
  // UL_READY, since with multiple co-located clients one can still be doing
  // PRACH while another is already in normal UL.
  assert(dev->write_symbols(dev, client_id, 0, 0, 1, 1, tx_bufs, 1) == UE_SPLIT7_SUCCESS);
  assert(dev->write_prach(dev, client_id, &prach_params) == UE_SPLIT7_SUCCESS);
  assert(dev->read_symbols(dev, 0, 0, 1, 1, rx_bufs, 1) == UE_SPLIT7_SUCCESS);
  std::cout << "   UL_READY: write_prach/write_symbols/read_symbols all stay valid indefinitely." << std::endl;

  // Drain the still-queued PRACH entries (symbol 3) so device_free() doesn't
  // have to; also exercises the overlay path once more for good measure.
  assert(dev->skip_symbols(dev, client_id, 0, 0, 3, 1) == UE_SPLIT7_SUCCESS);

  free(re);
  dev->stop(dev);
  ue_split7_device_free(dev);

  std::cout << "[PASS] 7. State lifecycle verification passed." << std::endl;
}

// --------------------------------------------------------------------------
// TEST CASE 9: multi-client UL combining
// --------------------------------------------------------------------------
// Two independent "clients" (standing in for two co-located UE instances)
// sharing one device: verifies the RF write is withheld until every
// registered, synchronized client has checked in for a given symbol (via
// write_symbols() or skip_symbols()), fires exactly once when it does, and
// that two clients' REs are genuinely summed (not one overwriting the other).
void test_multi_client_combine()
{
  std::cout << "[TEST] 9. Starting multi-client UL combining verification..." << std::endl;

  ue_split7_device_t *dev = ue_split7_device_create();
  assert(dev != nullptr);

  NR_DL_FRAME_PARMS fp = make_test_frame_parms();

  ue_split7_config_t config;
  std::memset(&config, 0, sizeof(config));
  config.dl_carrier_freq_hz = 3500000000ULL;
  config.ul_carrier_freq_hz = 3500000000ULL;
  config.sample_rate_hz = 30720000;
  config.fft_size = 2048;
  config.scs_khz = 30;
  config.cp_len_normal = 144;
  config.cp_len_symbol0 = 208;
  config.num_rx_antennas = 1;
  config.num_tx_antennas = 1;
  config.frame_parms = &fp;

  assert(dev->configure(dev, &config) == UE_SPLIT7_SUCCESS);

  test_priv_t *priv = (test_priv_t *)dev->priv;
  priv->openair0_dev.trx_start_func = mock_trx_start;
  priv->openair0_dev.trx_read_func = mock_trx_read;
  priv->openair0_dev.trx_write_func = mock_trx_write;
  priv->openair0_dev.trx_set_freq_func = mock_trx_set_freq;

  priv->state = 2; // UE_SPLIT7_STATE_UL_READY
  priv->next_read_ts = 1000000;

  uint32_t client_a = UINT32_MAX, client_b = UINT32_MAX;
  assert(dev->register_client(dev, &client_a) == UE_SPLIT7_SUCCESS);
  assert(dev->register_client(dev, &client_b) == UE_SPLIT7_SUCCESS);
  assert(client_a == 0 && client_b == 1);

  dev->seed_slot_tracking(dev, client_a, 0); // anchors the shared clock
  dev->seed_slot_tracking(dev, client_b, 0); // joins it -- from now on both are expected every symbol

  uint32_t fft_size = dev->config.fft_size;
  uint32_t N_RB_DL = dev->config.N_RB_DL > 0 ? dev->config.N_RB_DL : 106;
  uint32_t first_carrier_offset = fft_size - N_RB_DL * 6;
  auto in_active_band = [&](uint32_t i) { return i < N_RB_DL * 6 || i >= first_carrier_offset; };

  // Alternating per-bin, matching test_loopback_fft_ifft's pattern -- a CONSTANT
  // value across every active subcarrier IDFTs into a huge time-domain impulse
  // that clips hard in fixed-point, which would make this test's amplitude
  // comparison meaningless.
  std::vector<c16_t> pattern(fft_size, {0, 0});
  for (uint32_t i = 0; i < fft_size; ++i) {
    if (!in_active_band(i))
      continue;
    pattern[i].r = (i % 2 == 0) ? 1000 : -1000;
    pattern[i].i = (i % 3 == 0) ? 1000 : -1000;
  }
  c16_t *pattern_bufs[1] = {pattern.data()};

  // --- Barrier: the RF write must wait for BOTH clients, then fire exactly once. ---
  mock_trx_write_clear();
  assert(dev->write_symbols(dev, client_a, 0, 0, 1, 1, pattern_bufs, 1) == UE_SPLIT7_SUCCESS);
  assert(mock_tx_channel_buffer.empty()); // client_b hasn't checked in for symbol 1 yet
  assert(dev->skip_symbols(dev, client_b, 0, 0, 1, 1) == UE_SPLIT7_SUCCESS); // completes the set
  assert(!mock_tx_channel_buffer.empty());
  assert(mock_tx_channel_buffer.size() == (fft_size + dev->config.cp_len_normal) * 2); // exactly one write
  std::cout << "   Barrier: RF write withheld until both clients checked in, then fired exactly once." << std::endl;

  // --- Sum: two clients transmitting the identical pattern should read back
  // at roughly double a single client's amplitude (energy ratio ~4x). ---

  // Control: client_a alone (client_b skips) on symbol 2.
  mock_trx_write_clear();
  assert(dev->write_symbols(dev, client_a, 0, 0, 2, 1, pattern_bufs, 1) == UE_SPLIT7_SUCCESS);
  assert(dev->skip_symbols(dev, client_b, 0, 0, 2, 1) == UE_SPLIT7_SUCCESS);
  mock_rx_channel_buffer = mock_tx_channel_buffer;
  c16_t *rx_control[1] = {nullptr};
  assert(dev->read_symbols(dev, 0, 0, 2, 1, rx_control, 1) == UE_SPLIT7_SUCCESS);
  double control_mag2 = (double)rx_control[0][0].r * rx_control[0][0].r + (double)rx_control[0][0].i * rx_control[0][0].i;

  // Test: both clients transmit the identical pattern on symbol 3.
  std::vector<c16_t> pattern_b = pattern; // each caller needs its own buffer instance
  c16_t *pattern_b_bufs[1] = {pattern_b.data()};
  mock_trx_write_clear();
  assert(dev->write_symbols(dev, client_a, 0, 0, 3, 1, pattern_bufs, 1) == UE_SPLIT7_SUCCESS);
  assert(dev->write_symbols(dev, client_b, 0, 0, 3, 1, pattern_b_bufs, 1) == UE_SPLIT7_SUCCESS);
  mock_rx_channel_buffer = mock_tx_channel_buffer;
  c16_t *rx_test[1] = {nullptr};
  assert(dev->read_symbols(dev, 0, 0, 3, 1, rx_test, 1) == UE_SPLIT7_SUCCESS);
  double test_mag2 = (double)rx_test[0][0].r * rx_test[0][0].r + (double)rx_test[0][0].i * rx_test[0][0].i;

  double ratio = test_mag2 / control_mag2; // energy ratio; amplitude ratio is sqrt(ratio)
  std::cout << "   Two-client sum energy ratio vs single client: " << ratio << " (expect ~4.0, i.e. 2x amplitude)" << std::endl;
  assert(ratio > 3.0 && ratio < 5.0);

  dev->stop(dev);
  ue_split7_device_free(dev);
  std::cout << "[PASS] 9. Multi-client UL combining verification passed." << std::endl;
}

// --------------------------------------------------------------------------
// TEST CASE 6: RX and TX Timing Offset Dynamic Adjustment verification
// --------------------------------------------------------------------------
void test_timing_offset_adjustments(ue_split7_device_t *dev)
{
  std::cout << "[TEST] 6. Starting RX and TX timing offset adjustment verification..." << std::endl;

  uint32_t fft_size = dev->config.fft_size;
  uint32_t cp_len = dev->config.cp_len_normal;
  uint32_t total_sym_samples = fft_size + cp_len;

  // --- TX positive offset: transmit dummy samples for offset duration ---
  mock_trx_write_clear();
  dev->adjust_tx_timing_offset(dev, 200);

  std::vector<c16_t> tx_data(fft_size, {1000, 1000});
  c16_t *tx_bufs[1] = {tx_data.data()};

  ue_split7_status_t rc = dev->write_symbols(dev, 0, 0, 0, 1, 1, tx_bufs, 1);
  assert(rc == UE_SPLIT7_SUCCESS);

  // Buffer should contain 200 dummy samples (zeros) followed by total_sym_samples of OFDM
  assert(mock_tx_channel_buffer.size() == (200 + total_sym_samples) * 2);
  for (size_t i = 0; i < 200 * 2; ++i) {
    assert(mock_tx_channel_buffer[i] == 0);
  }
  std::cout << "   TX positive offset (+200): 200 dummy samples correctly transmitted before symbol." << std::endl;

  // --- TX negative offset: transmit fewer samples (trimmed from start of buffer) ---
  mock_trx_write_clear();
  dev->adjust_tx_timing_offset(dev, -100);

  rc = dev->write_symbols(dev, 0, 0, 0, 1, 1, tx_bufs, 1);
  assert(rc == UE_SPLIT7_SUCCESS);

  // Buffer should contain total_sym_samples - 100 samples
  assert(mock_tx_channel_buffer.size() == (total_sym_samples - 100) * 2);
  std::cout << "   TX negative offset (-100): 100 fewer samples correctly transmitted (trimmed from buffer start)." << std::endl;

  // --- RX positive offset: trash samples before receiving into time-domain buffer ---
  // Seed mock_rx_channel_buffer with 50 trash samples then symbol samples
  mock_rx_channel_buffer.clear();
  for (int i = 0; i < 50 * 2; ++i)
    mock_rx_channel_buffer.push_back(9999);
  for (size_t i = 0; i < total_sym_samples * 2; ++i)
    mock_rx_channel_buffer.push_back(1234);

  dev->adjust_rx_timing_offset(dev, 50);

  c16_t *rx_ptrs[1] = {nullptr};

  rc = dev->read_symbols(dev, 0, 0, 1, 1, rx_ptrs, 1);
  assert(rc == UE_SPLIT7_SUCCESS);
  // The 50 trash samples should have been popped and discarded, leaving 0 remaining in mock_rx_channel_buffer
  assert(mock_rx_channel_buffer.empty());
  std::cout << "   RX positive offset (+50): 50 samples correctly trashed before receiving into time-domain buffer." << std::endl;

  // --- RX negative offset: copy previous samples and receive proportionally fewer from antenna ---
  // Currently prev_rx_buf has the 1234 samples from the previous read.
  // Populate mock_rx_channel_buffer with total_sym_samples - 40 samples of value 5678
  mock_rx_channel_buffer.clear();
  for (size_t i = 0; i < (total_sym_samples - 40) * 2; ++i)
    mock_rx_channel_buffer.push_back(5678);

  dev->adjust_rx_timing_offset(dev, -40);

  rc = dev->read_symbols(dev, 0, 0, 2, 1, rx_ptrs, 1);
  assert(rc == UE_SPLIT7_SUCCESS);
  // All (total_sym_samples - 40) samples were consumed from mock_rx_channel_buffer
  assert(mock_rx_channel_buffer.empty());
  std::cout << "   RX negative offset (-40): 40 previous samples copied, 40 fewer samples received from antenna." << std::endl;

  // --- Verify that adjust_rx_timing_offset also adjusted TX timing in lockstep ---
  // The net RX adjustments were (+50) + (-40) = +10 samples.
  // Next TX should transmit 10 dummy samples.
  mock_trx_write_clear();
  rc = dev->write_symbols(dev, 0, 0, 0, 1, 1, tx_bufs, 1);
  assert(rc == UE_SPLIT7_SUCCESS);
  assert(mock_tx_channel_buffer.size() == (10 + total_sym_samples) * 2);
  for (size_t i = 0; i < 10 * 2; ++i) {
    assert(mock_tx_channel_buffer[i] == 0);
  }
  std::cout << "   RX-to-TX lockstep: adjusting RX timing by (+50, -40 = +10) correctly shifted TX timing by +10 dummy samples." << std::endl;

  std::cout << "[PASS] 6. RX and TX timing offset adjustments verified successfully." << std::endl;
}

// --------------------------------------------------------------------------
// TEST CASE 8: RX TD/FD buffer ownership and single-FFT caching verification
// --------------------------------------------------------------------------
void test_rx_td_fd_fft_caching(ue_split7_device_t *dev)
{
  std::cout << "[TEST] 8. Starting RX TD/FD buffer ownership and FFT caching verification..." << std::endl;

  assert(dev->rx_td_buffers != nullptr);
  assert(dev->rx_td_buffers[0] != nullptr);
  assert(dev->rx_fd_buffers != nullptr);
  assert(dev->rx_fd_buffers[0] != nullptr);

  test_priv_t *priv = (test_priv_t *)dev->priv;
  assert(priv->symbol_fft_done != nullptr);

  uint32_t fft_size = dev->config.fft_size;
  uint32_t cp_len = dev->config.cp_len_normal;
  uint32_t sym_samples = fft_size + cp_len;

  // We test symbol 7 of frame 0, slot 0
  const uint8_t test_sym = 7;
  const uint32_t ring_sym = test_sym; // slot 0, symbol 7 in 2-frame ring

  // Initially, symbol 7 FFT should not be marked done
  assert(!priv->symbol_fft_done[ring_sym]);

  // Provide synthetic OFDM time-domain signal in mock_rx_channel_buffer
  mock_rx_channel_buffer.assign(sym_samples * 2, 0);
  for (size_t i = 0; i < sym_samples * 2; ++i) {
    mock_rx_channel_buffer[i] = (int16_t)(1234 + (i % 100));
  }

  c16_t *rx_ptrs[1] = {nullptr};
  ue_split7_status_t rc = dev->read_symbols(dev, 0, 0, test_sym, 1, rx_ptrs, 1);
  assert(rc == UE_SPLIT7_SUCCESS);
  assert(rx_ptrs[0] != nullptr);

  // 1. Verify that mock_rx_channel_buffer was consumed and saved into rx_td_buf
  assert(mock_rx_channel_buffer.empty());
  assert(priv->symbol_fft_done[ring_sym] == true);

  // Save the frequency-domain result of the first FFT
  std::vector<c16_t> first_fft(rx_ptrs[0], rx_ptrs[0] + fft_size);

  // 2. Request the same symbol again with empty mock_rx_channel_buffer.
  // It MUST return the cached FD buffer data directly without reading antenna or redoing FFT.
  c16_t *rx_cached_ptrs[1] = {nullptr};
  rc = dev->read_symbols(dev, 0, 0, test_sym, 1, rx_cached_ptrs, 1);
  assert(rc == UE_SPLIT7_SUCCESS);
  assert(rx_cached_ptrs[0] == rx_ptrs[0]); // Returns the exact same device-owned FD buffer pointer

  for (uint32_t k = 0; k < fft_size; ++k) {
    assert(rx_cached_ptrs[0][k].r == first_fft[k].r);
    assert(rx_cached_ptrs[0][k].i == first_fft[k].i);
  }

  std::cout << "   Single FFT verification: subsequent read_symbols() returned cached FD buffer without redoing FFT." << std::endl;
  std::cout << "[PASS] 8. RX TD/FD buffer ownership and FFT caching verification passed." << std::endl;
}

// --------------------------------------------------------------------------
// MAIN EXECUTION
// --------------------------------------------------------------------------
int main(int argc, char **argv)
{
  std::cout << "=========================================================" << std::endl;
  std::cout << "            RUNNING UE SPLIT 7.1 LOW-PHY UNIT TESTS      " << std::endl;
  std::cout << "=========================================================" << std::endl;

  ue_split7_device_t *dev = ue_split7_device_create();
  assert(dev != nullptr);

  NR_DL_FRAME_PARMS fp = make_test_frame_parms();

  ue_split7_config_t config;
  std::memset(&config, 0, sizeof(config));
  config.dl_carrier_freq_hz = 3500000000ULL; // 3.5 GHz
  config.ul_carrier_freq_hz = 3500000000ULL;
  config.sample_rate_hz = 30720000; // 30.72 MHz
  config.fft_size = 2048;
  config.scs_khz = 30;
  config.cp_len_normal = 144;
  config.cp_len_symbol0 = 208; // symbol 0 has longer prefix
  config.num_rx_antennas = 1;
  config.num_tx_antennas = 1;
  config.frame_parms = &fp; // configure() copies it in; fp only needs to outlive this call

  ue_split7_status_t rc = dev->configure(dev, &config);
  assert(rc == UE_SPLIT7_SUCCESS);
  assert(dev->rx_td_buffers != nullptr && dev->rx_td_buffers[0] != nullptr);
  assert(dev->rx_fd_buffers != nullptr && dev->rx_fd_buffers[0] != nullptr);

  test_priv_t *priv = (test_priv_t *)dev->priv;
  priv->openair0_dev.trx_start_func = mock_trx_start;
  priv->openair0_dev.trx_read_func = mock_trx_read;
  priv->openair0_dev.trx_write_func = mock_trx_write;
  priv->openair0_dev.trx_set_freq_func = mock_trx_set_freq;

  uint32_t client_id = 0;
  assert(dev->register_client(dev, &client_id) == UE_SPLIT7_SUCCESS);
  assert(client_id == 0);

  // Jump straight to UL_READY rather than running a real sync cycle to flip one bit.
  // test_synchronization_service runs last: start_sync() starts a background read
  // thread that would otherwise race the other tests' single-shot mock reads.
  priv->state = 2; // UE_SPLIT7_STATE_UL_READY
  priv->next_read_ts = 1000000;
  dev->seed_slot_tracking(dev, client_id, 0);
  test_prach_transmission(dev);
  std::cout << "---------------------------------------------------------" << std::endl;
  test_prach_symbol_span_continuity(dev);
  std::cout << "---------------------------------------------------------" << std::endl;
  test_prach_repetition_continuity(dev);
  std::cout << "---------------------------------------------------------" << std::endl;
  test_loopback_fft_ifft(dev);
  std::cout << "---------------------------------------------------------" << std::endl;
  test_timing_advance(dev);
  std::cout << "---------------------------------------------------------" << std::endl;
  test_timing_offset_adjustments(dev);
  std::cout << "---------------------------------------------------------" << std::endl;
  test_rx_td_fd_fft_caching(dev);
  std::cout << "---------------------------------------------------------" << std::endl;
  test_synchronization_service(dev);
  std::cout << "---------------------------------------------------------" << std::endl;

  dev->stop(dev);
  ue_split7_device_free(dev);

  // Own fresh device, so UNSYNCED-state rejections can be checked from a clean state.
  test_state_lifecycle();
  std::cout << "---------------------------------------------------------" << std::endl;

  test_multi_client_combine();

  std::cout << "=========================================================" << std::endl;
  std::cout << "          ALL UE SPLIT 7.1 LOW-PHY TESTS COMPLETED: OK   " << std::endl;
  std::cout << "=========================================================" << std::endl;
  return 0;
}
