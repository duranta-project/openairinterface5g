/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "common_lib.h"
#include <gtest/gtest.h>
#include "common/config/config_userapi.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

configmodule_interface_t *uniqCfg = NULL;

extern "C" {
#include "common/config/config_userapi.h"
#include "openair1/SIMULATION/TOOLS/sim.h"
#include "openair2/LAYER2/NR_MAC_gNB/mac_config.h"
extern int device_init(openair0_device_t *device, openair0_config_t *openair0_cfg);
static softmodem_params_t softmodem_params;
softmodem_params_t *get_softmodem_params(void)
{
  return &softmodem_params;
}
void exit_function(const char *file, const char *function, const int line, const char *s, const int assert_flag)
{
  (void)assert_flag;
  fprintf(stderr, "FATAL: %s at %s:%s:%d\n", s, file, function, line);
  exit(EXIT_FAILURE);
}
// Stub: apply_channelmod.c needs this symbol, normally provided by the gNB MAC library.
bool nr_update_sib19(const gnb_sat_position_update_t *sat_position)
{
  (void)sat_position;
  return true;
}
}

namespace {

// Fresh port per call, so sequential test cases don't collide on a socket left in TIME_WAIT.
std::atomic<int> g_port_salt{0};
uint16_t PickPort()
{
  return static_cast<uint16_t>(23000 + ((getpid() + g_port_salt.fetch_add(1) * 17) % 12000));
}

configmodule_interface_t *StartDevice(openair0_device_t &device,
                                      openair0_config_t &config,
                                      const std::string &progname,
                                      const std::string &serveraddr,
                                      uint16_t port,
                                      int tx_ant,
                                      int rx_ant,
                                      const std::vector<std::string> &extra_args = {})
{
  std::vector<std::string> args = {progname,
                                   "--rfsimulator.serveraddr",
                                   serveraddr,
                                   "--rfsimulator.serverport",
                                   std::to_string(port)};
  args.insert(args.end(), extra_args.begin(), extra_args.end());
  std::vector<const char *> argv;
  for (const auto &a : args)
    argv.push_back(a.c_str());

  configmodule_interface_t *cfg = load_configmodule(argv.size(), (char **)argv.data(), CONFIG_ENABLECMDLINEONLY);
  uniqCfg = cfg;
  config.tx_num_channels = tx_ant;
  config.rx_num_channels = rx_ant;
  config.sample_rate = 30.72e6;
  device_init(&device, &config);
  return cfg;
}

void StopDevice(openair0_device_t &device, configmodule_interface_t *cfg)
{
  if (device.trx_end_func)
    device.trx_end_func(&device);
  if (cfg)
    end_configmodule(cfg);
}

// Triangle wave, deterministic from (sender, sample position, antenna) so tx and rx sides can
// each compute it independently. Period varies per antenna and phase per sender, so a
// wrong-antenna or wrong-sender substitution shows up as an obviously wrong value; the constant
// slope also means an off-by-one sample position is never masked by a flat spot near a peak.
int16_t TxPattern(bool sender_is_server, uint64_t sample_pos, int tx_ant)
{
  const int16_t kAmplitude = 2000;
  const uint64_t period = 40 + 8 * static_cast<uint64_t>(tx_ant);
  const uint64_t phase_offset = sender_is_server ? 0 : period / 3;
  const uint64_t pos = (sample_pos + phase_offset) % period;
  const uint64_t half = period / 2;
  if (pos < half)
    return static_cast<int16_t>(-kAmplitude + (2 * kAmplitude * pos) / half);
  else
    return static_cast<int16_t>(kAmplitude - (2 * kAmplitude * (pos - half)) / half);
}

// Same crosstalk weighting as rfsimulator_read_internal(): 1.0 on the diagonal, 0.2/|tx-rx| off it.
double MixCoeff(int tx_ant, int rx_ant)
{
  int d = std::abs(tx_ant - rx_ant);
  return d ? (0.2 / d) : 1.0;
}

// Same per-antenna accumulation/truncation order as simulator.cpp, for a bit-exact match.
int16_t ExpectedCombined(bool sender_is_server, uint64_t sample_pos, int rx_ant, int num_tx_ant)
{
  double acc = 0.0;
  for (int tx_ant = 0; tx_ant < num_tx_ant; tx_ant++) {
    double term = static_cast<double>(TxPattern(sender_is_server, sample_pos, tx_ant)) * MixCoeff(tx_ant, rx_ant);
    acc = static_cast<double>(static_cast<int16_t>(acc + term));
  }
  return static_cast<int16_t>(acc);
}

void FillTxBlock(std::vector<std::vector<c16_t>> &bufs, bool sender_is_server, uint64_t start_ts, int nant, int nsamps)
{
  for (int a = 0; a < nant; a++) {
    for (int i = 0; i < nsamps; i++) {
      int16_t v = TxPattern(sender_is_server, start_ts + i, a);
      bufs[a][i] = {v, v};
    }
  }
}

struct VerifyResult {
  bool ok = true;
  std::string details;
};

VerifyResult VerifyRxBlock(const std::vector<std::vector<c16_t>> &rx,
                           uint64_t start_ts,
                           int rx_ant,
                           int peer_tx_ant,
                           bool peer_is_server)
{
  // Allow ±1 with >1 tx antenna: vectorized rounding can differ slightly from this reference.
  // A real bug (missing/misrouted block) would miss by far more than that.
  const int16_t kTolerance = peer_tx_ant > 1 ? 1 : 0;
  VerifyResult res;
  int mismatches = 0;
  for (int a = 0; a < rx_ant && mismatches < 3; a++) {
    for (size_t i = 0; i < rx[a].size() && mismatches < 3; i++) {
      int16_t expected = ExpectedCombined(peer_is_server, start_ts + i, a, peer_tx_ant);
      if (std::abs(rx[a][i].r - expected) > kTolerance || std::abs(rx[a][i].i - expected) > kTolerance) {
        res.ok = false;
        res.details += "ant=" + std::to_string(a) + " idx=" + std::to_string(i) + " ts=" + std::to_string(start_ts) + " got=("
                       + std::to_string(rx[a][i].r) + "," + std::to_string(rx[a][i].i) + ")" + " want=" + std::to_string(expected)
                       + "\n";
        mismatches++;
      }
    }
  }
  return res;
}

// One trx_set_beams() switch: old_beam before switch_ts, new_beam from switch_ts on.
struct BeamSchedule {
  uint64_t switch_ts;
  int old_beam;
  int new_beam;
};

int BeamAt(const BeamSchedule &sched, uint64_t sample_pos)
{
  return sample_pos >= sched.switch_ts ? sched.new_beam : sched.old_beam;
}

// Same gain lookup and scaling as get_rx_gain_db()/rfsimulator_read_internal(), including the
// float powf() call, for a bit-exact match.
int16_t ExpectedBeamSample(bool sender_is_server,
                           uint64_t sample_pos,
                           int rx_beam,
                           int tx_beam,
                           const std::vector<float> &gain_diag_db)
{
  int16_t v = TxPattern(sender_is_server, sample_pos, /*tx_ant=*/0);
  float gain_dB = gain_diag_db.at(static_cast<size_t>(std::abs(rx_beam - tx_beam)));
  float gain_linear = powf(10, gain_dB / 20.0);
  double term = static_cast<double>(v) * static_cast<double>(gain_linear);
  return static_cast<int16_t>(term);
}

VerifyResult VerifyBeamBlock(const std::vector<c16_t> &rx,
                             uint64_t start_ts,
                             bool peer_is_server,
                             const BeamSchedule &rx_schedule,
                             const BeamSchedule &peer_tx_schedule,
                             const std::vector<float> &gain_diag_db)
{
  VerifyResult res;
  int mismatches = 0;
  for (size_t i = 0; i < rx.size() && mismatches < 3; i++) {
    uint64_t pos = start_ts + i;
    int rx_beam = BeamAt(rx_schedule, pos);
    int tx_beam = BeamAt(peer_tx_schedule, pos);
    int16_t expected = ExpectedBeamSample(peer_is_server, pos, rx_beam, tx_beam, gain_diag_db);
    if (rx[i].r != expected || rx[i].i != expected) {
      res.ok = false;
      res.details += "idx=" + std::to_string(i) + " ts=" + std::to_string(start_ts) + " rx_beam=" + std::to_string(rx_beam)
                     + " tx_beam=" + std::to_string(tx_beam) + " got=(" + std::to_string(rx[i].r) + "," + std::to_string(rx[i].i)
                     + ")" + " want=" + std::to_string(expected) + "\n";
      mismatches++;
    }
  }
  return res;
}

} // namespace

struct RFSimAntennaTestCase {
  int server_tx;
  int server_rx;
  int client_tx;
  int client_rx;
};

class RFSimulatorAntennaTest : public ::testing::TestWithParam<RFSimAntennaTestCase> {};

TEST_P(RFSimulatorAntennaTest, DataFlowsBothWaysAfterConnect)
{
  const auto &param = GetParam();
  const uint16_t port = PickPort();
  const int nsamps = 256;
  // Write-ahead buffer, in blocks. Must be >1, or the client's one-sample clock offset from
  // connect can starve the peer's next read and deadlock both sides.
  const int advance_blocks = 3;
  // Skipped: right after connect there's nothing real to verify yet.
  const int settle_reads = 6;
  const int verify_reads = 6;

  openair0_device_t server_device = {0};
  openair0_config_t server_config = {0};
  configmodule_interface_t *server_cfg =
      StartDevice(server_device, server_config, "test_rfsimulator_antenna", "server", port, param.server_tx, param.server_rx);
  ASSERT_EQ(server_device.trx_start_func(&server_device), 0);

  openair0_device_t client_device = {0};
  openair0_config_t client_config = {0};
  configmodule_interface_t *client_cfg =
      StartDevice(client_device, client_config, "test_rfsimulator_antenna", "127.0.0.1", port, param.client_tx, param.client_rx);

  struct SideOutcome {
    bool connect_ok = false;
    bool data_ok = true;
    std::string details;
  };
  std::atomic<bool> client_done{false};
  std::atomic<bool> client_connected{false};
  SideOutcome client_outcome;

  // Write before read on both sides; reading first on both ends would deadlock.
  std::thread client_thread([&] {
    SideOutcome out;
    out.connect_ok = (client_device.trx_start_func(&client_device) == 0);
    client_connected.store(out.connect_ok);
    if (out.connect_ok) {
      std::vector<std::vector<c16_t>> rx_bufs(param.client_rx, std::vector<c16_t>(nsamps));
      std::vector<void *> rx_ptrs(param.client_rx);
      for (int a = 0; a < param.client_rx; a++)
        rx_ptrs[a] = rx_bufs[a].data();
      std::vector<std::vector<c16_t>> tx_bufs(param.client_tx, std::vector<c16_t>(nsamps));
      std::vector<void *> tx_ptrs(param.client_tx);
      for (int a = 0; a < param.client_tx; a++)
        tx_ptrs[a] = tx_bufs[a].data();

      // Unverified; only used to learn the timestamp the handshake synced us to.
      openair0_timestamp_t sync_ts;
      int n = client_device.trx_read_func(&client_device, &sync_ts, rx_ptrs.data(), nsamps, param.client_rx);
      if (n != nsamps) {
        out.data_ok = false;
        out.details += "client sync read returned " + std::to_string(n) + " expected " + std::to_string(nsamps) + "\n";
      } else {
        openair0_timestamp_t next_write_ts = sync_ts + static_cast<uint64_t>(advance_blocks) * nsamps;

        int remaining_settle = settle_reads;
        int remaining_verify = verify_reads;
        while (remaining_verify > 0) {
          FillTxBlock(tx_bufs, /*sender_is_server=*/false, next_write_ts, param.client_tx, nsamps);
          client_device.trx_write_func(&client_device, next_write_ts, tx_ptrs.data(), nsamps, param.client_tx, 0);
          next_write_ts += nsamps;

          openair0_timestamp_t read_ts;
          n = client_device.trx_read_func(&client_device, &read_ts, rx_ptrs.data(), nsamps, param.client_rx);
          if (n != nsamps) {
            out.data_ok = false;
            out.details += "client read returned " + std::to_string(n) + " expected " + std::to_string(nsamps) + "\n";
            break;
          }
          if (remaining_settle > 0) {
            remaining_settle--;
            continue;
          }
          VerifyResult v = VerifyRxBlock(rx_bufs, read_ts, param.client_rx, param.server_tx, /*peer_is_server=*/true);
          if (!v.ok) {
            out.data_ok = false;
            out.details += v.details;
          }
          remaining_verify--;
        }

        // Send a few extra blocks so the peer's last reads don't block waiting for us.
        for (int i = 0; i < advance_blocks + settle_reads + 2; i++) {
          FillTxBlock(tx_bufs, /*sender_is_server=*/false, next_write_ts, param.client_tx, nsamps);
          client_device.trx_write_func(&client_device, next_write_ts, tx_ptrs.data(), nsamps, param.client_tx, 0);
          next_write_ts += nsamps;
        }
      }
    }
    client_outcome = out;
    client_done.store(true);
  });

  std::vector<std::vector<c16_t>> server_rx_bufs(param.server_rx, std::vector<c16_t>(nsamps));
  std::vector<void *> server_rx_ptrs(param.server_rx);
  for (int a = 0; a < param.server_rx; a++)
    server_rx_ptrs[a] = server_rx_bufs[a].data();
  std::vector<std::vector<c16_t>> server_tx_bufs(param.server_tx, std::vector<c16_t>(nsamps));
  std::vector<void *> server_tx_ptrs(param.server_tx);
  for (int a = 0; a < param.server_tx; a++)
    server_tx_ptrs[a] = server_tx_bufs[a].data();

  int remaining_settle = settle_reads;
  int remaining_verify = verify_reads;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  bool server_data_ok = true;
  std::string server_details;
  openair0_timestamp_t server_next_write_ts = 0;

  while (remaining_verify > 0 && std::chrono::steady_clock::now() < deadline) {
    FillTxBlock(server_tx_bufs, /*sender_is_server=*/true, server_next_write_ts, param.server_tx, nsamps);
    server_device.trx_write_func(&server_device, server_next_write_ts, server_tx_ptrs.data(), nsamps, param.server_tx, 0);
    server_next_write_ts += nsamps;

    openair0_timestamp_t read_ts;
    int n = server_device.trx_read_func(&server_device, &read_ts, server_rx_ptrs.data(), nsamps, param.server_rx);
    ASSERT_EQ(n, nsamps);

    if (!client_connected.load()) {
      // No client connected yet, so nothing to verify.
      continue;
    }
    if (remaining_settle > 0) {
      remaining_settle--;
      continue;
    }
    VerifyResult v = VerifyRxBlock(server_rx_bufs, read_ts, param.server_rx, param.client_tx, /*peer_is_server=*/false);
    if (!v.ok) {
      server_data_ok = false;
      server_details += v.details;
    }
    remaining_verify--;
  }

  // Symmetric top-up: see the matching comment on the client side.
  for (int i = 0; i < advance_blocks + settle_reads + 2; i++) {
    FillTxBlock(server_tx_bufs, /*sender_is_server=*/true, server_next_write_ts, param.server_tx, nsamps);
    server_device.trx_write_func(&server_device, server_next_write_ts, server_tx_ptrs.data(), nsamps, param.server_tx, 0);
    server_next_write_ts += nsamps;
  }

  EXPECT_LE(std::chrono::steady_clock::now(), deadline) << "server loop did not complete within the time budget "
                                                           "(possible RX/TX synchronization deadlock)";
  EXPECT_TRUE(server_data_ok) << server_details;

  bool joined = false;
  {
    const auto join_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!client_done.load() && std::chrono::steady_clock::now() < join_deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    if (client_done.load()) {
      client_thread.join();
      joined = true;
    }
  }
  EXPECT_TRUE(joined) << "client side did not finish within the time budget";
  if (joined) {
    EXPECT_TRUE(client_outcome.connect_ok) << "client failed to connect";
    EXPECT_TRUE(client_outcome.data_ok) << client_outcome.details;
  } else {
    client_thread.detach();
  }

  StopDevice(server_device, server_cfg);
  if (joined)
    StopDevice(client_device, client_cfg);
}

INSTANTIATE_TEST_SUITE_P(AntennaVariations,
                         RFSimulatorAntennaTest,
                         ::testing::Values(RFSimAntennaTestCase{1, 1, 1, 1},
                                           RFSimAntennaTestCase{2, 2, 1, 1},
                                           RFSimAntennaTestCase{2, 2, 2, 2},
                                           RFSimAntennaTestCase{4, 4, 2, 2},
                                           // Uneven tx/rx per side: gNB 2T4R (extra RX antennas for
                                           // UL diversity) against a UE 1T2R.
                                           RFSimAntennaTestCase{2, 4, 1, 2},
                                           // Fully crossed: gNB 1T2R against a UE 2T1R.
                                           RFSimAntennaTestCase{1, 2, 2, 1}));

// Tests the beam_gains matrix and a trx_set_beams() switch landing at the exact scheduled sample.
// Single antenna per side keeps this bit-exact.
TEST(RFSimulatorBeamTest, GainMatrixAndScheduledSwitchApplyAtExactSample)
{
  const uint16_t port = PickPort();
  const int nsamps = 256;
  const int advance_blocks = 3;
  const int settle_reads = 6;
  const int verify_reads = 10;
  const std::vector<float> gain_diag_db = {0.0f, -6.0f, -20.0f};
  const std::vector<std::string> beam_args =
      {"--rfsimulator.enable_beams", "1", "--rfsimulator.beam_gains", "0,-6,-20", "--rfsimulator.beam_ids", "0"};

  openair0_device_t server_device = {0};
  openair0_config_t server_config = {0};
  configmodule_interface_t *server_cfg =
      StartDevice(server_device, server_config, "test_rfsimulator_beam", "server", port, 1, 1, beam_args);
  ASSERT_EQ(server_device.trx_start_func(&server_device), 0);
  ASSERT_NE(server_device.trx_set_beams, nullptr);

  openair0_device_t client_device = {0};
  openair0_config_t client_config = {0};
  configmodule_interface_t *client_cfg =
      StartDevice(client_device, client_config, "test_rfsimulator_beam", "127.0.0.1", port, 1, 1, beam_args);

  // Only the server's beam switches, at server_switch_ts (set by the server thread below).
  const BeamSchedule client_beam_schedule{0, 0, 0};
  std::atomic<uint64_t> server_switch_ts{UINT64_MAX};
  std::atomic<bool> client_connected{false};

  struct SideOutcome {
    bool connect_ok = false;
    bool data_ok = true;
    std::string details;
  };
  std::atomic<bool> client_done{false};
  SideOutcome client_outcome;

  std::thread client_thread([&] {
    SideOutcome out;
    out.connect_ok = (client_device.trx_start_func(&client_device) == 0);
    client_connected.store(out.connect_ok);
    if (out.connect_ok) {
      std::vector<std::vector<c16_t>> rx_bufs(1, std::vector<c16_t>(nsamps));
      std::vector<void *> rx_ptrs = {rx_bufs[0].data()};
      std::vector<std::vector<c16_t>> tx_bufs(1, std::vector<c16_t>(nsamps));
      std::vector<void *> tx_ptrs = {tx_bufs[0].data()};

      openair0_timestamp_t sync_ts;
      int n = client_device.trx_read_func(&client_device, &sync_ts, rx_ptrs.data(), nsamps, 1);
      if (n != nsamps) {
        out.data_ok = false;
        out.details += "client sync read returned " + std::to_string(n) + " expected " + std::to_string(nsamps) + "\n";
      } else {
        openair0_timestamp_t next_write_ts = sync_ts + static_cast<uint64_t>(advance_blocks) * nsamps;
        int remaining_settle = settle_reads;
        int remaining_verify = verify_reads;
        while (remaining_verify > 0) {
          FillTxBlock(tx_bufs, /*sender_is_server=*/false, next_write_ts, 1, nsamps);
          client_device.trx_write_func(&client_device, next_write_ts, tx_ptrs.data(), nsamps, 1, 0);
          next_write_ts += nsamps;

          openair0_timestamp_t read_ts;
          n = client_device.trx_read_func(&client_device, &read_ts, rx_ptrs.data(), nsamps, 1);
          if (n != nsamps) {
            out.data_ok = false;
            out.details += "client read returned " + std::to_string(n) + " expected " + std::to_string(nsamps) + "\n";
            break;
          }
          if (remaining_settle > 0) {
            remaining_settle--;
            continue;
          }
          uint64_t switch_ts = server_switch_ts.load();
          ASSERT_NE(switch_ts, UINT64_MAX) << "server never scheduled its beam switch";
          BeamSchedule server_beam_schedule{switch_ts, 0, 1};
          VerifyResult v = VerifyBeamBlock(rx_bufs[0],
                                           read_ts,
                                           /*peer_is_server=*/true,
                                           client_beam_schedule,
                                           server_beam_schedule,
                                           gain_diag_db);
          if (!v.ok) {
            out.data_ok = false;
            out.details += v.details;
          }
          remaining_verify--;
        }

        for (int i = 0; i < advance_blocks + settle_reads + 2; i++) {
          FillTxBlock(tx_bufs, /*sender_is_server=*/false, next_write_ts, 1, nsamps);
          client_device.trx_write_func(&client_device, next_write_ts, tx_ptrs.data(), nsamps, 1, 0);
          next_write_ts += nsamps;
        }
      }
    }
    client_outcome = out;
    client_done.store(true);
  });

  std::vector<std::vector<c16_t>> server_rx_bufs(1, std::vector<c16_t>(nsamps));
  std::vector<void *> server_rx_ptrs = {server_rx_bufs[0].data()};
  std::vector<std::vector<c16_t>> server_tx_bufs(1, std::vector<c16_t>(nsamps));
  std::vector<void *> server_tx_ptrs = {server_tx_bufs[0].data()};

  int remaining_settle = settle_reads;
  int remaining_verify = verify_reads;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  bool server_data_ok = true;
  std::string server_details;
  openair0_timestamp_t server_next_write_ts = 0;
  bool switch_scheduled = false;

  while (remaining_verify > 0 && std::chrono::steady_clock::now() < deadline) {
    FillTxBlock(server_tx_bufs, /*sender_is_server=*/true, server_next_write_ts, 1, nsamps);
    server_device.trx_write_func(&server_device, server_next_write_ts, server_tx_ptrs.data(), nsamps, 1, 0);
    server_next_write_ts += nsamps;

    if (!switch_scheduled && client_connected.load()) {
      // Generous margin so it lands inside the window both sides actually verify.
      uint64_t switch_ts = server_next_write_ts + static_cast<uint64_t>(settle_reads) * nsamps;
      uint16_t new_beam = 1;
      ASSERT_EQ(server_device.trx_set_beams(&server_device, &new_beam, 1, switch_ts), 0);
      server_switch_ts.store(switch_ts);
      switch_scheduled = true;
    }

    openair0_timestamp_t read_ts;
    int n = server_device.trx_read_func(&server_device, &read_ts, server_rx_ptrs.data(), nsamps, 1);
    ASSERT_EQ(n, nsamps);

    if (!client_connected.load()) {
      // No client connected yet, so nothing to verify.
      continue;
    }
    if (remaining_settle > 0) {
      remaining_settle--;
      continue;
    }
    ASSERT_NE(server_switch_ts.load(), UINT64_MAX) << "switch should have been scheduled before settle elapsed";
    BeamSchedule server_beam_schedule{server_switch_ts.load(), 0, 1};
    VerifyResult v = VerifyBeamBlock(server_rx_bufs[0],
                                     read_ts,
                                     /*peer_is_server=*/false,
                                     server_beam_schedule,
                                     client_beam_schedule,
                                     gain_diag_db);
    if (!v.ok) {
      server_data_ok = false;
      server_details += v.details;
    }
    remaining_verify--;
  }

  for (int i = 0; i < advance_blocks + settle_reads + 2; i++) {
    FillTxBlock(server_tx_bufs, /*sender_is_server=*/true, server_next_write_ts, 1, nsamps);
    server_device.trx_write_func(&server_device, server_next_write_ts, server_tx_ptrs.data(), nsamps, 1, 0);
    server_next_write_ts += nsamps;
  }

  EXPECT_LE(std::chrono::steady_clock::now(), deadline) << "server loop did not complete within the time budget "
                                                           "(possible RX/TX synchronization deadlock)";
  EXPECT_TRUE(server_data_ok) << server_details;

  bool joined = false;
  {
    const auto join_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!client_done.load() && std::chrono::steady_clock::now() < join_deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    if (client_done.load()) {
      client_thread.join();
      joined = true;
    }
  }
  EXPECT_TRUE(joined) << "client side did not finish within the time budget";
  if (joined) {
    EXPECT_TRUE(client_outcome.connect_ok) << "client failed to connect";
    EXPECT_TRUE(client_outcome.data_ok) << client_outcome.details;
  } else {
    client_thread.detach();
  }

  StopDevice(server_device, server_cfg);
  if (joined)
    StopDevice(client_device, client_cfg);
}

int main(int argc, char **argv)
{
  logInit();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
