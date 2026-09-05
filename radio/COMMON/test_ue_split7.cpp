/**
 * @file test_ue_split7.cpp
 * @brief Unit tests for the UE-centric 7.1 functional split interface.
 *
 * Test order matters: write_prach() must run before the first write_symbol()
 * call, since write_symbol() auto-promotes the device past the PRACH-valid
 * state (see ue_split7_state_t in ue_split7_device.c).
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

  // write_symbol()/read_symbol() apply the 38.211 §5.3 symbol-rotation only to the
  // active RE region; guard-band bins are intentionally left untouched, so only
  // populate/verify the active band here.
  uint32_t N_RB_DL = dev->config.N_RB_DL > 0 ? dev->config.N_RB_DL : 106;
  uint32_t first_carrier_offset = fft_size - N_RB_DL * 6;
  auto in_active_band = [&](uint32_t i) { return i < N_RB_DL * 6 || i >= first_carrier_offset; };

  std::vector<ue_split7_iq_t> tx_re(fft_size, {0, 0});

  for (uint32_t i = 0; i < fft_size; ++i) {
    if (!in_active_band(i))
      continue;
    tx_re[i].r = (i % 2 == 0) ? 1000 : -1000;
    tx_re[i].i = (i % 3 == 0) ? 1000 : -1000;
  }
  // write_symbol() rotates re_buffer in place, so keep a pristine copy for comparison.
  const std::vector<ue_split7_iq_t> tx_re_orig = tx_re;

  ue_split7_symbol_buffer_t tx_sym;
  tx_sym.meta.symbol_number = 1; // normal CP symbol
  tx_sym.meta.slot_number = 0;
  tx_sym.meta.timestamp_samples = 1000000;
  tx_sym.num_subcarriers = fft_size;
  tx_sym.re_buffer = tx_re.data();

  ue_split7_symbol_buffer_t rx_sym;
  rx_sym.meta.symbol_number = 1;
  rx_sym.meta.slot_number = 0;
  rx_sym.num_subcarriers = fft_size;
  rx_sym.re_buffer = nullptr; // output-only; read_symbol() fills this in

  mock_trx_write_clear();

  ue_split7_status_t rc = dev->write_symbol(dev, &tx_sym, 1);
  assert(rc == UE_SPLIT7_SUCCESS);
  assert(mock_tx_channel_buffer.size() == (fft_size + cp_len) * 2);

  mock_rx_channel_buffer = mock_tx_channel_buffer;

  rc = dev->read_symbol(dev, &rx_sym, 1);
  assert(rc == UE_SPLIT7_SUCCESS);
  assert(rx_sym.re_buffer != nullptr);
  const ue_split7_iq_t *rx_re = rx_sym.re_buffer;

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
  std::cout << "[PASS] 1. FFT/IFFT loopback test passed successfully." << std::endl;
}

// --------------------------------------------------------------------------
// TEST CASE 2: Timing Advance (TA) verification
// --------------------------------------------------------------------------
void test_timing_advance(ue_split7_device_t *dev)
{
  std::cout << "[TEST] 2. Starting Timing Advance (TA) verification..." << std::endl;

  uint32_t fft_size = dev->config.fft_size;
  std::vector<ue_split7_iq_t> tx_re(fft_size, {1000, -1000});

  ue_split7_symbol_buffer_t tx_sym;
  tx_sym.meta.symbol_number = 1;
  tx_sym.meta.slot_number = 0;
  tx_sym.meta.timestamp_samples = 1000000;
  tx_sym.num_subcarriers = fft_size;
  tx_sym.re_buffer = tx_re.data();

  uint32_t ta_val = 128;
  ue_split7_status_t rc = dev->set_timing_advance(dev, ta_val);
  assert(rc == UE_SPLIT7_SUCCESS);

  mock_trx_write_clear();
  rc = dev->write_symbol(dev, &tx_sym, 1);
  assert(rc == UE_SPLIT7_SUCCESS);

  // Expected = timestamp_samples + RX-to-TX lead (3 slots via get_samples_slot_duration(),
  // CP0=208/CP_NORMAL=144/FFT=2048/mu=1 => 92256) - TA = 1000000 + 92256 - 128 = 1092128.
  std::cout << "   Nominal Timestamp: 1000000 | Applied TA: " << ta_val << " | Transmitted Timestamp: " << last_tx_timestamp
            << std::endl;
  assert(last_tx_timestamp == (1000000 + 92256 - ta_val));

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
void test_prach_transmission(ue_split7_device_t *dev)
{
  std::cout << "[TEST] 5. Starting PRACH Transmission verification..." << std::endl;

  uint32_t fft_size = dev->config.fft_size;
  uint32_t num_prach_samples = 139; // short preamble
  std::vector<ue_split7_iq_t> prach_re(num_prach_samples);
  for (uint32_t k = 0; k < num_prach_samples; ++k) {
    prach_re[k].r = (k % 2 == 0) ? 2000 : -2000;
    prach_re[k].i = (k % 3 == 0) ? 2000 : -2000;
  }

  ue_split7_prach_tx_params_t params;
  std::memset(&params, 0, sizeof(params));
  params.samples = prach_re.data();
  params.num_samples = num_prach_samples;
  params.timestamp_samples = 1000000;
  params.time_offset_samples = 100;
  params.cp_len_samples = 512;
  params.slot_number = 2;
  params.symbol_number = 0;
  params.frequency_offset_scs = 100;
  params.repetition_count = 2;
  params.antenna_port = 0;

  uint32_t ta_val = 128;
  dev->set_timing_advance(dev, ta_val);

  mock_trx_write_clear();
  ue_split7_status_t rc = dev->write_prach(dev, &params);
  assert(rc == UE_SPLIT7_SUCCESS);

  // cp_len_samples + fft_size * repetition_count = 512 + 2048*2 = 4608 complex samples.
  std::cout << "   Channel Buffer size: " << mock_tx_channel_buffer.size() << " | Expected: " << (512 + fft_size * 2) * 2
            << std::endl;
  assert(mock_tx_channel_buffer.size() == (512 + fft_size * 2) * 2);

  // timestamp_samples + time_offset_samples + RX-to-TX lead (92256, see test_timing_advance) - TA
  // = 1000000 + 100 + 92256 - 128 = 1092228.
  std::cout << "   Transmitted Timestamp: " << last_tx_timestamp << " | Expected: 1092228" << std::endl;
  assert(last_tx_timestamp == 1092228);

  // Custom fft_size with num_samples == fft_size (1-to-1 mapping).
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
  params2.timestamp_samples = 2000000;
  params2.time_offset_samples = 250; // unaligned transmission
  params2.cp_len_samples = 3168;
  params2.slot_number = 3;
  params2.symbol_number = 0;
  params2.repetition_count = 4;
  params2.antenna_port = 0;

  mock_trx_write_clear();
  rc = dev->write_prach(dev, &params2);
  assert(rc == UE_SPLIT7_SUCCESS);

  // cp_len_samples + fft_size * repetition_count = 3168 + 6144*4 = 27744 complex samples.
  assert(mock_tx_channel_buffer.size() == (3168 + custom_fft_size * 4) * 2);

  // Lead time (92256) comes from the device's own frame_parms, not params->fft_size:
  // 2000000 + 250 + 92256 - 128 = 2092378.
  assert(last_tx_timestamp == 2092378);
  std::cout << "   Unaligned Custom FFT PRACH Transmitted Timestamp: " << last_tx_timestamp << " | Expected: 2092378" << std::endl;

  std::cout << "[PASS] 5. PRACH Transmission verification passed." << std::endl;
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

  // write_symbol()'s DFT input requires 32-byte-aligned re_buffer; std::vector doesn't guarantee that.
  uint32_t fft_size = dev->config.fft_size;
  ue_split7_iq_t *re = (ue_split7_iq_t *)aligned_alloc(32, fft_size * sizeof(ue_split7_iq_t));
  assert(re != nullptr);
  for (uint32_t i = 0; i < fft_size; i++)
    re[i] = {1000, -1000};

  // Separate buffers for RX (read_symbol() output-only, device fills it in) and
  // TX (write_symbol()'s caller-owned input) -- they must not alias each other.
  ue_split7_symbol_buffer_t rx_sym;
  rx_sym.meta.symbol_number = 1;
  rx_sym.meta.slot_number = 0;
  rx_sym.num_subcarriers = fft_size;
  rx_sym.re_buffer = nullptr;

  ue_split7_symbol_buffer_t tx_sym;
  tx_sym.meta.symbol_number = 1;
  tx_sym.meta.slot_number = 0;
  tx_sym.meta.timestamp_samples = 1000000;
  tx_sym.num_subcarriers = fft_size;
  tx_sym.re_buffer = re;

  ue_split7_prach_tx_params_t prach_params;
  std::memset(&prach_params, 0, sizeof(prach_params));
  prach_params.samples = re;
  prach_params.num_samples = 139;
  prach_params.timestamp_samples = 1000000;
  prach_params.cp_len_samples = 512;
  prach_params.repetition_count = 2;

  // UNSYNCED: nothing is valid yet.
  assert(dev->read_symbol(dev, &rx_sym, 1) == UE_SPLIT7_ERR_STATE);
  assert(dev->write_symbol(dev, &tx_sym, 1) == UE_SPLIT7_ERR_STATE);
  assert(dev->write_prach(dev, &prach_params) == UE_SPLIT7_ERR_STATE);
  std::cout << "   UNSYNCED: read/write_symbol/write_prach all rejected, as expected." << std::endl;

  // Drive state directly; this test is about state gating, not sync correctness.
  priv->state = 1; // UE_SPLIT7_STATE_DL_SYNCED

  // DL_SYNCED: read_symbol valid; UL still isn't.
  assert(dev->read_symbol(dev, &rx_sym, 1) == UE_SPLIT7_SUCCESS);
  assert(dev->write_symbol(dev, &tx_sym, 1) == UE_SPLIT7_ERR_STATE);
  assert(dev->write_prach(dev, &prach_params) == UE_SPLIT7_ERR_STATE);
  std::cout << "   DL_SYNCED: read_symbol valid, write_symbol/write_prach still rejected." << std::endl;

  // UL_PRACH_ONLY: established by the first set_timing_advance() after sync.
  rc = dev->set_timing_advance(dev, 0);
  assert(rc == UE_SPLIT7_SUCCESS);
  mock_trx_write_clear();
  assert(dev->write_prach(dev, &prach_params) == UE_SPLIT7_SUCCESS);
  std::cout << "   UL_PRACH_ONLY: write_prach valid." << std::endl;

  // First write_symbol() auto-promotes to UL_NORMAL; write_prach must now be rejected.
  assert(dev->write_symbol(dev, &tx_sym, 1) == UE_SPLIT7_SUCCESS);
  assert(dev->write_prach(dev, &prach_params) == UE_SPLIT7_ERR_STATE);
  assert(dev->read_symbol(dev, &rx_sym, 1) == UE_SPLIT7_SUCCESS);
  std::cout << "   UL_NORMAL: write_prach now rejected; read/write_symbol still valid." << std::endl;

  free(re);
  dev->stop(dev);
  ue_split7_device_free(dev);

  std::cout << "[PASS] 7. State lifecycle verification passed." << std::endl;
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

  test_priv_t *priv = (test_priv_t *)dev->priv;
  priv->openair0_dev.trx_start_func = mock_trx_start;
  priv->openair0_dev.trx_read_func = mock_trx_read;
  priv->openair0_dev.trx_write_func = mock_trx_write;
  priv->openair0_dev.trx_set_freq_func = mock_trx_set_freq;

  // Jump straight to UL_PRACH_ONLY rather than running a real sync cycle to flip one bit.
  // PRACH must run before the first write_symbol() (which auto-promotes past it).
  // test_synchronization_service runs last: start_sync() starts a background read
  // thread that would otherwise race the other tests' single-shot mock reads.
  priv->state = 2; // UE_SPLIT7_STATE_UL_PRACH_ONLY
  test_prach_transmission(dev);
  std::cout << "---------------------------------------------------------" << std::endl;
  test_loopback_fft_ifft(dev);
  std::cout << "---------------------------------------------------------" << std::endl;
  test_timing_advance(dev);
  std::cout << "---------------------------------------------------------" << std::endl;
  test_synchronization_service(dev);
  std::cout << "---------------------------------------------------------" << std::endl;

  dev->stop(dev);
  ue_split7_device_free(dev);

  // Own fresh device, so UNSYNCED-state rejections can be checked from a clean state.
  test_state_lifecycle();

  std::cout << "=========================================================" << std::endl;
  std::cout << "          ALL UE SPLIT 7.1 LOW-PHY TESTS COMPLETED: OK   " << std::endl;
  std::cout << "=========================================================" << std::endl;
  return 0;
}
