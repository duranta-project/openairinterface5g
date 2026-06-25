/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <yaml-cpp/yaml.h>
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <string>
#include <complex>
#include <filesystem>
#include <fstream>
#include <tuple>
#include <set>
#include <utility>

#include "common_lib.h"
#include "common/config/config_userapi.h"

namespace fs = std::filesystem;

configmodule_interface_t *uniqCfg = NULL;

extern "C" {
#include "common/config/config_userapi.h"
#include "openair1/SIMULATION/TOOLS/sim.h"
extern int device_init(openair0_device_t *device, openair0_config_t *openair0_cfg);
extern void vrtsim_produce_samples(openair0_device_t *device, size_t num_samples);
static softmodem_params_t softmodem_params;
softmodem_params_t *get_softmodem_params(void)
{
  return &softmodem_params;
}
void exit_function(const char *file, const char *function, const int line, const char *s, const int assert)
{
  fprintf(stderr, "FATAL: %s at %s:%s:%d\n", s, file, function, line);
  exit(EXIT_FAILURE);
}
}

class CIRDBProducer {
  fs::path root;
  std::vector<YAML::Node> entries;

 public:
  CIRDBProducer(fs::path r) : root(r)
  {
    fs::create_directories(root);
  }
  void add_entry(int model_id, int n_tx, int n_rx, int L, int S, double fs, double dt, double ds, double speed)
  {
    YAML::Node entry;
    entry["model_id"] = model_id;
    entry["n_tx"] = n_tx;
    entry["n_rx"] = n_rx;
    entry["L"] = L;
    entry["S"] = S;
    entry["fs_hz"] = fs;
    entry["snapshot_dt_s"] = dt;
    entry["ds_ns"] = ds;
    entry["speed_mps"] = speed;
    entry["offset_bytes"] = entries.size() * n_tx * n_rx * L * sizeof(float) * 2;
    entry["nbytes"] = n_tx * n_rx * L * sizeof(float) * 2;
    entries.push_back(entry);
  }
  void write_files()
  {
    YAML::Node root_node;
    root_node["entries"] = entries;
    std::ofstream out(root / "vrtsim.yaml");
    out << root_node;

    std::ofstream bin(root / "cir_db.bin", std::ios::binary);
    for (const auto &e : entries) {
      int count = e["n_tx"].as<int>() * e["n_rx"].as<int>() * e["L"].as<int>();
      std::vector<float> data(count * 2, 0.0f);
      for (int tx = 0; tx < e["n_tx"].as<int>(); tx++) {
        for (int rx = 0; rx < e["n_rx"].as<int>(); rx++) {
          int L = e["L"].as<int>();
          data[((rx + tx * e["n_rx"].as<int>()) * L + 7) * 2] = 1.0f;
        }
      }
      bin.write(reinterpret_cast<char *>(data.data()), data.size() * sizeof(float));
    }
  }
};

struct CIRDBAntParams {
  int gnb_tx;
  int gnb_rx;
  int ue_tx;
  int ue_rx;
  std::string ue_ant_str;
  CIRDBAntParams(int gt, int gr, int ut, int ur, std::string s) : gnb_tx(gt), gnb_rx(gr), ue_tx(ut), ue_rx(ur), ue_ant_str(s)
  {
  }
};

class VRTSTapsCIRDBTest : public ::testing::TestWithParam<CIRDBAntParams> {
 protected:
  openair0_device_t server_device = {0};
  openair0_device_t client_device = {0};
  openair0_config_t server_config = {0};
  openair0_config_t client_config = {0};
  fs::path tmp_dir;
  std::string shm_name;
  configmodule_interface_t *cfg1 = nullptr;
  configmodule_interface_t *cfg2 = nullptr;

  void SetUp() override
  {
    const CIRDBAntParams &p = GetParam();
    tmp_dir = fs::temp_directory_path() / ("vrtsim_cirdb_test_" + std::to_string(p.gnb_tx) + "_" + p.ue_ant_str);
    CIRDBProducer producer(tmp_dir);
    // Add entries for both directions to both model IDs to ensure test robustness against unintended configuration state leakage
    producer.add_entry(0, p.gnb_tx, p.ue_rx, 8, 1, 30.72e6, 0.5, 10.0, 1.5);
    producer.add_entry(1, p.gnb_tx, p.ue_rx, 8, 1, 30.72e6, 0.5, 10.0, 1.5);
    producer.add_entry(0, p.ue_tx, p.gnb_rx, 8, 1, 30.72e6, 0.5, 10.0, 1.5);
    producer.add_entry(1, p.ue_tx, p.gnb_rx, 8, 1, 30.72e6, 0.5, 10.0, 1.5);
    producer.write_files();
    shm_name = "shm_cirdb_" + std::to_string(p.gnb_tx) + "_" + p.ue_ant_str;
  }

  void TearDown() override
  {
    if (server_device.trx_end_func)
      server_device.trx_end_func(&server_device);
    if (client_device.trx_end_func)
      client_device.trx_end_func(&client_device);
    if (cfg1)
      end_configmodule(cfg1);
    if (cfg2)
      end_configmodule(cfg2);
    fs::remove_all(tmp_dir);
    server_device = {0};
    client_device = {0};
    cfg1 = nullptr;
    cfg2 = nullptr;
    uniqCfg = nullptr;
  }
};

TEST_P(VRTSTapsCIRDBTest, CIRDBDelayDL)
{
  const CIRDBAntParams &p = GetParam();

  // Setup Server
  {
    const char *s_argv[] = {"test_vrtsim_cirdb",
                            "--vrtsim.role",
                            "server",
                            "--vrtsim.shm_channel_name",
                            shm_name.c_str(),
                            "--vrtsim.disable-timing-thread",
                            "1",
                            "--vrtsim.cirdb",
                            "1",
                            "--vrtsim.cirdb-path",
                            tmp_dir.c_str(),
                            "--vrtsim.cirdb_model_id",
                            "0",
                            "--vrtsim.ue_config.[0].antennas",
                            p.ue_ant_str.c_str()};
    cfg1 = load_configmodule(sizeof(s_argv) / sizeof(char *), (char **)s_argv, CONFIG_ENABLECMDLINEONLY);
    uniqCfg = cfg1;
    server_config.tx_num_channels = p.gnb_tx;
    server_config.rx_num_channels = p.gnb_rx;
    server_config.sample_rate = 30.72e6;
    ASSERT_EQ(device_init(&server_device, &server_config), 0);
    ASSERT_EQ(server_device.trx_start_func(&server_device), 0);
  }

  // Setup Client
  {
    const char *c_argv[] = {"test_vrtsim_cirdb",
                            "--vrtsim.role",
                            "client",
                            "--vrtsim.shm_channel_name",
                            shm_name.c_str(),
                            "--vrtsim.cirdb",
                            "0",
                            "--vrtsim.ue_config.[0].antennas",
                            p.ue_ant_str.c_str()};
    cfg2 = load_configmodule(sizeof(c_argv) / sizeof(char *), (char **)c_argv, CONFIG_ENABLECMDLINEONLY);
    uniqCfg = cfg2;
    client_config.tx_num_channels = p.ue_tx;
    client_config.rx_num_channels = p.ue_rx;
    client_config.sample_rate = 30.72e6;
    ASSERT_EQ(device_init(&client_device, &client_config), 0);
    ASSERT_EQ(client_device.trx_start_func(&client_device), 0);
  }

  const int nsamps = 1024;
  std::vector<std::vector<c16_t>> tx_chan_samples(p.gnb_tx, std::vector<c16_t>(nsamps));
  std::vector<void *> tx_ptrs(p.gnb_tx);
  for (int ch = 0; ch < p.gnb_tx; ch++) {
    for (int i = 0; i < nsamps; i++)
      tx_chan_samples[ch][i] = {(int16_t)(i + 1 + ch * 100), (int16_t) - (i + 1 + ch * 100)};
    tx_ptrs[ch] = tx_chan_samples[ch].data();
  }
  ASSERT_EQ(server_device.trx_write_func(&server_device, 0, tx_ptrs.data(), nsamps, p.gnb_tx, 0), nsamps);
  vrtsim_produce_samples(&server_device, nsamps);

  std::vector<std::vector<c16_t>> rx_chan_samples(p.ue_rx, std::vector<c16_t>(nsamps));
  std::vector<void *> rx_ptrs(p.ue_rx);
  for (int ch = 0; ch < p.ue_rx; ch++)
    rx_ptrs[ch] = rx_chan_samples[ch].data();
  openair0_timestamp_t rx_ts;
  ASSERT_EQ(client_device.trx_read_func(&client_device, &rx_ts, rx_ptrs.data(), nsamps, p.ue_rx), nsamps);

  for (int rx_ch = 0; rx_ch < p.ue_rx; rx_ch++) {
    for (int i = 7; i < nsamps; i++) {
      int32_t expected_r = 0;
      for (int tx_ch = 0; tx_ch < p.gnb_tx; tx_ch++)
        expected_r += tx_chan_samples[tx_ch][i - 7].r;
      EXPECT_EQ(rx_chan_samples[rx_ch][i].r, (int16_t)expected_r);
    }
  }
}

TEST_P(VRTSTapsCIRDBTest, CIRDBDelayUL)
{
  const CIRDBAntParams &p = GetParam();

  // Setup Server
  {
    const char *s_argv[] = {"test_vrtsim_cirdb",
                            "--vrtsim.role",
                            "server",
                            "--vrtsim.shm_channel_name",
                            shm_name.c_str(),
                            "--vrtsim.disable-timing-thread",
                            "1",
                            "--vrtsim.cirdb",
                            "0",
                            "--vrtsim.ue_config.[0].antennas",
                            p.ue_ant_str.c_str()};
    cfg1 = load_configmodule(sizeof(s_argv) / sizeof(char *), (char **)s_argv, CONFIG_ENABLECMDLINEONLY);
    uniqCfg = cfg1;
    server_config.tx_num_channels = p.gnb_tx;
    server_config.rx_num_channels = p.gnb_rx;
    server_config.sample_rate = 30.72e6;
    ASSERT_EQ(device_init(&server_device, &server_config), 0);
    ASSERT_EQ(server_device.trx_start_func(&server_device), 0);
  }

  // Setup Client
  {
    const char *c_argv[] = {"test_vrtsim_cirdb",
                            "--vrtsim.role",
                            "client",
                            "--vrtsim.shm_channel_name",
                            shm_name.c_str(),
                            "--vrtsim.cirdb",
                            "1",
                            "--vrtsim.cirdb-path",
                            tmp_dir.c_str(),
                            "--vrtsim.cirdb_model_id",
                            "1",
                            "--vrtsim.ue_config.[0].antennas",
                            p.ue_ant_str.c_str()};
    cfg2 = load_configmodule(sizeof(c_argv) / sizeof(char *), (char **)c_argv, CONFIG_ENABLECMDLINEONLY);
    uniqCfg = cfg2;
    client_config.tx_num_channels = p.ue_tx;
    client_config.rx_num_channels = p.ue_rx;
    client_config.sample_rate = 30.72e6;
    ASSERT_EQ(device_init(&client_device, &client_config), 0);
    ASSERT_EQ(client_device.trx_start_func(&client_device), 0);
  }

  const int nsamps = 1024;
  std::vector<std::vector<c16_t>> tx_chan_samples(p.ue_tx, std::vector<c16_t>(nsamps));
  std::vector<void *> tx_ptrs(p.ue_tx);
  for (int ch = 0; ch < p.ue_tx; ch++) {
    for (int i = 0; i < nsamps; i++)
      tx_chan_samples[ch][i] = {(int16_t)(i + 1 + ch * 100), (int16_t) - (i + 1 + ch * 100)};
    tx_ptrs[ch] = tx_chan_samples[ch].data();
  }
  ASSERT_EQ(client_device.trx_write_func(&client_device, 0, tx_ptrs.data(), nsamps, p.ue_tx, 0), nsamps);
  vrtsim_produce_samples(&server_device, nsamps);

  std::vector<std::vector<c16_t>> rx_chan_samples(p.gnb_rx, std::vector<c16_t>(nsamps));
  std::vector<void *> rx_ptrs(p.gnb_rx);
  for (int ch = 0; ch < p.gnb_rx; ch++)
    rx_ptrs[ch] = rx_chan_samples[ch].data();
  openair0_timestamp_t rx_ts;
  ASSERT_EQ(server_device.trx_read_func(&server_device, &rx_ts, rx_ptrs.data(), nsamps, p.gnb_rx), nsamps);

  for (int rx_ch = 0; rx_ch < p.gnb_rx; rx_ch++) {
    for (int i = 7; i < nsamps; i++) {
      int32_t expected_r = 0;
      for (int tx_ch = 0; tx_ch < p.ue_tx; tx_ch++)
        expected_r += tx_chan_samples[tx_ch][i - 7].r;
      EXPECT_EQ(rx_chan_samples[rx_ch][i].r, (int16_t)expected_r);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(AntennaVariations,
                         VRTSTapsCIRDBTest,
                         ::testing::Values(CIRDBAntParams(1, 1, 1, 1, "1x1"),
                                           CIRDBAntParams(2, 2, 1, 1, "1x1"),
                                           CIRDBAntParams(4, 4, 2, 2, "2x2"),
                                           CIRDBAntParams(8, 8, 2, 2, "2x2")));

struct CIRDBMultiUEParams {
  int server_tx;
  int server_rx;
  int num_ues;
  std::vector<std::string> ue_antennas;
  CIRDBMultiUEParams(int s_tx, int s_rx, int n_ue, std::vector<std::string> ants)
      : server_tx(s_tx), server_rx(s_rx), num_ues(n_ue), ue_antennas(ants)
  {
  }
};

class VRTSIMCIRDBMultiUETest : public ::testing::TestWithParam<CIRDBMultiUEParams> {
 protected:
  configmodule_interface_t *cfg_server = nullptr;
  std::vector<configmodule_interface_t *> cfg_clients;
  openair0_device_t server_device = {0};
  std::vector<openair0_device_t> client_devices;
  openair0_config_t server_config = {0};
  std::vector<openair0_config_t> client_configs;
  fs::path tmp_dir;
  std::string shm_name;

  void SetUp() override
  {
    const auto &p = GetParam();
    std::string key = std::to_string(p.server_tx) + "_" + std::to_string(p.server_rx);
    for (int i = 0; i < p.num_ues; i++) {
      key += "_" + p.ue_antennas[i];
    }
    tmp_dir = fs::temp_directory_path() / ("vrtsim_cirdb_multi_test_" + key);
    shm_name = "shm_cirdb_multi_" + key;

    CIRDBProducer producer(tmp_dir);
    // Add entries for both DL and UL directions, for both model IDs
    // Collect all unique configurations of (n_tx, n_rx)
    std::set<std::pair<int, int>> unique_configs;
    for (int i = 0; i < p.num_ues; i++) {
      int client_tx = 0, client_rx = 0;
      sscanf(p.ue_antennas[i].c_str(), "%dx%d", &client_tx, &client_rx);
      unique_configs.insert({p.server_tx, client_rx});
      unique_configs.insert({client_tx, p.server_rx});
    }

    for (const auto &cfg : unique_configs) {
      producer.add_entry(0, cfg.first, cfg.second, 8, 1, 30.72e6, 0.5, 10.0, 1.5);
      producer.add_entry(1, cfg.first, cfg.second, 8, 1, 30.72e6, 0.5, 10.0, 1.5);
    }
    producer.write_files();
  }

  void TearDown() override
  {
    for (auto &client_device : client_devices) {
      if (client_device.trx_end_func) {
        client_device.trx_end_func(&client_device);
      }
    }
    if (server_device.trx_end_func) {
      server_device.trx_end_func(&server_device);
    }
    for (auto *cfg : cfg_clients) {
      if (cfg) {
        end_configmodule(cfg);
      }
    }
    if (cfg_server) {
      end_configmodule(cfg_server);
    }
    fs::remove_all(tmp_dir);
    client_devices.clear();
    client_configs.clear();
    cfg_clients.clear();
    cfg_server = nullptr;
    server_device = {0};
    uniqCfg = nullptr;
  }
};

TEST_P(VRTSIMCIRDBMultiUETest, CIRDBDelayDL)
{
  const auto &param = GetParam();

  // Setup Server
  {
    std::vector<std::string> server_args_store = {"test_vrtsim_cirdb",
                                                  "--vrtsim.role",
                                                  "server",
                                                  "--vrtsim.shm_channel_name",
                                                  shm_name.c_str(),
                                                  "--vrtsim.disable-timing-thread",
                                                  "1",
                                                  "--vrtsim.cirdb",
                                                  "1",
                                                  "--vrtsim.cirdb-path",
                                                  tmp_dir.c_str(),
                                                  "--vrtsim.cirdb_model_id",
                                                  "0",
                                                  "--vrtsim.num_ues",
                                                  std::to_string(param.num_ues)};
    for (int i = 0; i < param.num_ues; i++) {
      server_args_store.push_back("--vrtsim.ue_config.[" + std::to_string(i) + "].antennas");
      server_args_store.push_back(param.ue_antennas[i]);
    }

    std::vector<const char *> server_argv;
    for (const auto &arg : server_args_store) {
      server_argv.push_back(arg.c_str());
    }

    cfg_server = load_configmodule(server_argv.size(), (char **)server_argv.data(), CONFIG_ENABLECMDLINEONLY);
    uniqCfg = cfg_server;
    server_config.tx_num_channels = param.server_tx;
    server_config.rx_num_channels = param.server_rx;
    server_config.sample_rate = 30.72e6;
    ASSERT_EQ(device_init(&server_device, &server_config), 0);
    ASSERT_EQ(server_device.trx_start_func(&server_device), 0);
  }

  // Setup Clients
  client_devices.resize(param.num_ues);
  client_configs.resize(param.num_ues);
  cfg_clients.resize(param.num_ues);

  for (int i = 0; i < param.num_ues; i++) {
    std::vector<std::string> client_args_store = {"test_vrtsim_cirdb",
                                                  "--vrtsim.role",
                                                  "client",
                                                  "--vrtsim.shm_channel_name",
                                                  shm_name.c_str(),
                                                  "--vrtsim.cirdb",
                                                  "0",
                                                  "--vrtsim.ue_id",
                                                  std::to_string(i),
                                                  "--vrtsim.ue_config.[" + std::to_string(i) + "].antennas",
                                                  param.ue_antennas[i]};
    std::vector<const char *> client_argv;
    for (const auto &arg : client_args_store) {
      client_argv.push_back(arg.c_str());
    }
    cfg_clients[i] = load_configmodule(client_argv.size(), (char **)client_argv.data(), CONFIG_ENABLECMDLINEONLY);
    uniqCfg = cfg_clients[i];

    int client_tx = 0, client_rx = 0;
    sscanf(param.ue_antennas[i].c_str(), "%dx%d", &client_tx, &client_rx);

    client_configs[i].tx_num_channels = client_tx;
    client_configs[i].rx_num_channels = client_rx;
    client_configs[i].sample_rate = 30.72e6;
    ASSERT_EQ(device_init(&client_devices[i], &client_configs[i]), 0);
    ASSERT_EQ(client_devices[i].trx_start_func(&client_devices[i]), 0);
  }

  const int nsamps = 1024;
  // Prepare TX samples for Server (DL)
  std::vector<std::vector<c16_t>> server_tx_samples(param.server_tx, std::vector<c16_t>(nsamps));
  std::vector<void *> server_tx_ptrs(param.server_tx);
  for (int ch = 0; ch < param.server_tx; ch++) {
    for (int i = 0; i < nsamps; i++)
      server_tx_samples[ch][i] = {(int16_t)(i + 1 + ch * 100), (int16_t) - (i + 1 + ch * 100)};
    server_tx_ptrs[ch] = server_tx_samples[ch].data();
  }
  ASSERT_EQ(server_device.trx_write_func(&server_device, 0, server_tx_ptrs.data(), nsamps, param.server_tx, 0), nsamps);
  vrtsim_produce_samples(&server_device, nsamps);

  // Each client reads from server (DL) and checks delayed signal
  for (int u = 0; u < param.num_ues; u++) {
    int client_tx = 0, client_rx = 0;
    sscanf(param.ue_antennas[u].c_str(), "%dx%d", &client_tx, &client_rx);

    std::vector<std::vector<c16_t>> client_rx_samples(client_rx, std::vector<c16_t>(nsamps));
    std::vector<void *> client_rx_ptrs(client_rx);
    for (int ch = 0; ch < client_rx; ch++)
      client_rx_ptrs[ch] = client_rx_samples[ch].data();
    openair0_timestamp_t rx_ts;
    ASSERT_EQ(client_devices[u].trx_read_func(&client_devices[u], &rx_ts, client_rx_ptrs.data(), nsamps, client_rx), nsamps);
    EXPECT_EQ(rx_ts, 0);

    for (int rx_ch = 0; rx_ch < client_rx; rx_ch++) {
      for (int i = 7; i < nsamps; i++) {
        int32_t expected_r = 0;
        int32_t expected_i = 0;
        for (int tx_ch = 0; tx_ch < param.server_tx; tx_ch++) {
          expected_r += server_tx_samples[tx_ch][i - 7].r;
          expected_i += server_tx_samples[tx_ch][i - 7].i;
        }
        int16_t sat_r = (int16_t)((expected_r > 32767) ? 32767 : (expected_r < -32768) ? -32768 : expected_r);
        int16_t sat_i = (int16_t)((expected_i > 32767) ? 32767 : (expected_i < -32768) ? -32768 : expected_i);
        EXPECT_EQ(client_rx_samples[rx_ch][i].r, sat_r) << "DL Mismatch UE " << u << " rx_ant " << rx_ch << " sample " << i;
        EXPECT_EQ(client_rx_samples[rx_ch][i].i, sat_i) << "DL Mismatch UE " << u << " rx_ant " << rx_ch << " sample " << i;
      }
    }
  }
}

TEST_P(VRTSIMCIRDBMultiUETest, CIRDBDelayUL)
{
  const auto &param = GetParam();

  // Setup Server
  {
    std::vector<std::string> server_args_store = {"test_vrtsim_cirdb",
                                                  "--vrtsim.role",
                                                  "server",
                                                  "--vrtsim.shm_channel_name",
                                                  shm_name.c_str(),
                                                  "--vrtsim.disable-timing-thread",
                                                  "1",
                                                  "--vrtsim.cirdb",
                                                  "0",
                                                  "--vrtsim.num_ues",
                                                  std::to_string(param.num_ues)};
    for (int i = 0; i < param.num_ues; i++) {
      server_args_store.push_back("--vrtsim.ue_config.[" + std::to_string(i) + "].antennas");
      server_args_store.push_back(param.ue_antennas[i]);
    }

    std::vector<const char *> server_argv;
    for (const auto &arg : server_args_store) {
      server_argv.push_back(arg.c_str());
    }

    cfg_server = load_configmodule(server_argv.size(), (char **)server_argv.data(), CONFIG_ENABLECMDLINEONLY);
    uniqCfg = cfg_server;
    server_config.tx_num_channels = param.server_tx;
    server_config.rx_num_channels = param.server_rx;
    server_config.sample_rate = 30.72e6;
    ASSERT_EQ(device_init(&server_device, &server_config), 0);
    ASSERT_EQ(server_device.trx_start_func(&server_device), 0);
  }

  // Setup Clients
  client_devices.resize(param.num_ues);
  client_configs.resize(param.num_ues);
  cfg_clients.resize(param.num_ues);

  for (int i = 0; i < param.num_ues; i++) {
    std::vector<std::string> client_args_store = {"test_vrtsim_cirdb",
                                                  "--vrtsim.role",
                                                  "client",
                                                  "--vrtsim.shm_channel_name",
                                                  shm_name.c_str(),
                                                  "--vrtsim.cirdb",
                                                  "1",
                                                  "--vrtsim.cirdb-path",
                                                  tmp_dir.c_str(),
                                                  "--vrtsim.cirdb_model_id",
                                                  "1",
                                                  "--vrtsim.ue_id",
                                                  std::to_string(i),
                                                  "--vrtsim.ue_config.[" + std::to_string(i) + "].antennas",
                                                  param.ue_antennas[i]};
    std::vector<const char *> client_argv;
    for (const auto &arg : client_args_store) {
      client_argv.push_back(arg.c_str());
    }
    cfg_clients[i] = load_configmodule(client_argv.size(), (char **)client_argv.data(), CONFIG_ENABLECMDLINEONLY);
    uniqCfg = cfg_clients[i];

    int client_tx = 0, client_rx = 0;
    sscanf(param.ue_antennas[i].c_str(), "%dx%d", &client_tx, &client_rx);

    client_configs[i].tx_num_channels = client_tx;
    client_configs[i].rx_num_channels = client_rx;
    client_configs[i].sample_rate = 30.72e6;
    ASSERT_EQ(device_init(&client_devices[i], &client_configs[i]), 0);
    ASSERT_EQ(client_devices[i].trx_start_func(&client_devices[i]), 0);
  }

  const int nsamps = 1024;
  // Prepare TX samples for each client (UL)
  std::vector<std::vector<std::vector<c16_t>>> client_tx_samples(param.num_ues);
  std::vector<std::vector<void *>> client_tx_ptrs(param.num_ues);
  for (int u = 0; u < param.num_ues; u++) {
    int client_tx = 0, client_rx = 0;
    sscanf(param.ue_antennas[u].c_str(), "%dx%d", &client_tx, &client_rx);

    client_tx_samples[u].resize(client_tx, std::vector<c16_t>(nsamps));
    client_tx_ptrs[u].resize(client_tx);
    for (int a = 0; a < client_tx; a++) {
      for (int i = 0; i < nsamps; i++) {
        client_tx_samples[u][a][i] = {(int16_t)(i + u * 1000 + a * 100), (int16_t) - (i + u * 1000 + a * 100)};
      }
      client_tx_ptrs[u][a] = client_tx_samples[u][a].data();
    }
  }

  // All clients write at timestamp 0
  for (int u = 0; u < param.num_ues; u++) {
    int client_tx = 0, client_rx = 0;
    sscanf(param.ue_antennas[u].c_str(), "%dx%d", &client_tx, &client_rx);
    ASSERT_EQ(client_devices[u].trx_write_func(&client_devices[u], 0, client_tx_ptrs[u].data(), nsamps, client_tx, 0), nsamps);
  }

  // Server produces samples
  vrtsim_produce_samples(&server_device, nsamps);

  // Server reads from clients (UL combined) at timestamp 0
  std::vector<std::vector<c16_t>> server_rx_samples(param.server_rx, std::vector<c16_t>(nsamps));
  std::vector<void *> server_rx_ptrs(param.server_rx);
  for (int a = 0; a < param.server_rx; a++) {
    server_rx_ptrs[a] = server_rx_samples[a].data();
  }

  openair0_timestamp_t rx_ts;
  ASSERT_EQ(server_device.trx_read_func(&server_device, &rx_ts, server_rx_ptrs.data(), nsamps, param.server_rx), nsamps);
  EXPECT_EQ(rx_ts, 0);

  // Verify UL combining (with CIRDB delay of 7)
  for (int aarx = 0; aarx < param.server_rx; aarx++) {
    for (int i = 7; i < nsamps; i++) {
      int32_t expected_r = 0;
      int32_t expected_i = 0;
      for (int u = 0; u < param.num_ues; u++) {
        int client_tx = 0, client_rx = 0;
        sscanf(param.ue_antennas[u].c_str(), "%dx%d", &client_tx, &client_rx);
        for (int tx_ch = 0; tx_ch < client_tx; tx_ch++) {
          expected_r += client_tx_samples[u][tx_ch][i - 7].r;
          expected_i += client_tx_samples[u][tx_ch][i - 7].i;
        }
      }
      int16_t sat_r = (int16_t)((expected_r > 32767) ? 32767 : (expected_r < -32768) ? -32768 : expected_r);
      int16_t sat_i = (int16_t)((expected_i > 32767) ? 32767 : (expected_i < -32768) ? -32768 : expected_i);
      EXPECT_EQ(server_rx_samples[aarx][i].r, sat_r) << "UL Mismatch at sample " << i << " rx_ant " << aarx;
      EXPECT_EQ(server_rx_samples[aarx][i].i, sat_i) << "UL Mismatch at sample " << i << " rx_ant " << aarx;
    }
  }
}

INSTANTIATE_TEST_SUITE_P(MultiUEVariations,
                         VRTSIMCIRDBMultiUETest,
                         ::testing::Values(CIRDBMultiUEParams(2, 2, 2, {"2x2", "1x2"}),
                                           CIRDBMultiUEParams(2, 2, 2, {"1x2", "1x2"})));

int main(int argc, char **argv)
{
  logInit();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
