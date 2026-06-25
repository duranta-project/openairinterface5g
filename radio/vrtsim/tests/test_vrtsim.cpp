/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "common_lib.h"
#include <gtest/gtest.h>
#include "common/config/config_userapi.h"
#include <thread>
#include <vector>
#include <string>

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

struct VRTSIMTestCase {
  int server_tx;
  int server_rx;
  int client_tx;
  int client_rx;
  std::string ue_antennas;
};

class VRTSIMTest : public ::testing::TestWithParam<VRTSIMTestCase> {
 protected:
  configmodule_interface_t *cfg1 = nullptr;
  configmodule_interface_t *cfg2 = nullptr;
  openair0_device_t server_device = {0};
  openair0_device_t client_device = {0};
  openair0_config_t server_config = {0};
  openair0_config_t client_config = {0};

  void SetUp() override
  {
    const auto &param = GetParam();

    // Setup server
    std::vector<const char *> server_argv = {"--vrtsim.role",
                                             "server",
                                             "--vrtsim.disable-timing-thread",
                                             "1",
                                             "--vrtsim.ue_config.[0].antennas",
                                             param.ue_antennas.c_str()};
    cfg1 = load_configmodule(server_argv.size(), (char **)server_argv.data(), CONFIG_ENABLECMDLINEONLY);
    uniqCfg = cfg1;
    server_config.tx_num_channels = param.server_tx;
    server_config.rx_num_channels = param.server_rx;
    server_config.sample_rate = 30.72e6;
    ASSERT_EQ(device_init(&server_device, &server_config), 0);
    ASSERT_EQ(server_device.trx_start_func(&server_device), 0);

    // Setup client
    std::vector<const char *> client_argv = {"--vrtsim.role", "client"};
    cfg2 = load_configmodule(client_argv.size(), (char **)client_argv.data(), CONFIG_ENABLECMDLINEONLY);
    uniqCfg = cfg2;
    client_config.tx_num_channels = param.client_tx;
    client_config.rx_num_channels = param.client_rx;
    client_config.sample_rate = 30.72e6;
    ASSERT_EQ(device_init(&client_device, &client_config), 0);
    ASSERT_EQ(client_device.trx_start_func(&client_device), 0);
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
  }
};

TEST_P(VRTSIMTest, TransparentChannel)
{
  const auto &param = GetParam();
  const int nsamps = 1024;

  // Prepare TX samples for client (UL)
  std::vector<std::vector<c16_t>> client_tx_samples(param.client_tx, std::vector<c16_t>(nsamps));
  std::vector<void *> client_tx_ptrs(param.client_tx);
  for (int a = 0; a < param.client_tx; a++) {
    for (int i = 0; i < nsamps; i++) {
      client_tx_samples[a][i] = {(int16_t)(i + a * 100), (int16_t) - (i + a * 100)};
    }
    client_tx_ptrs[a] = client_tx_samples[a].data();
  }

  // Prepare TX samples for server (DL)
  std::vector<std::vector<c16_t>> server_tx_samples(param.server_tx, std::vector<c16_t>(nsamps));
  std::vector<void *> server_tx_ptrs(param.server_tx);
  for (int a = 0; a < param.server_tx; a++) {
    for (int i = 0; i < nsamps; i++) {
      server_tx_samples[a][i] = {(int16_t)(i + a * 200), (int16_t)(i + a * 200 + 10)};
    }
    server_tx_ptrs[a] = server_tx_samples[a].data();
  }

  // Both sides write at timestamp 0
  ASSERT_EQ(client_device.trx_write_func(&client_device, 0, client_tx_ptrs.data(), nsamps, param.client_tx, 0), nsamps);
  ASSERT_EQ(server_device.trx_write_func(&server_device, 0, server_tx_ptrs.data(), nsamps, param.server_tx, 0), nsamps);

  // Server produces samples (advances time to nsamps)
  vrtsim_produce_samples(&server_device, nsamps);

  // Server reads from client (UL) at timestamp 0
  std::vector<std::vector<c16_t>> server_rx_samples(param.server_rx, std::vector<c16_t>(nsamps));
  std::vector<void *> server_rx_ptrs(param.server_rx);
  for (int a = 0; a < param.server_rx; a++)
    server_rx_ptrs[a] = server_rx_samples[a].data();

  openair0_timestamp_t rx_ts;
  ASSERT_EQ(server_device.trx_read_func(&server_device, &rx_ts, server_rx_ptrs.data(), nsamps, param.server_rx), nsamps);
  EXPECT_EQ(rx_ts, 0);

  // Verify UL mapping (Order mapping: min(client_tx, server_rx))
  int num_ul_mapped = std::min(param.client_tx, param.server_rx);
  for (int a = 0; a < num_ul_mapped; a++) {
    for (int i = 0; i < nsamps; i++) {
      EXPECT_EQ(server_rx_samples[a][i].r, client_tx_samples[a][i].r) << "UL Mismatch at index " << i << " antenna " << a;
      EXPECT_EQ(server_rx_samples[a][i].i, client_tx_samples[a][i].i) << "UL Mismatch at index " << i << " antenna " << a;
    }
  }

  // Client reads from server (DL) at timestamp 0
  std::vector<std::vector<c16_t>> client_rx_samples(param.client_rx, std::vector<c16_t>(nsamps));
  std::vector<void *> client_rx_ptrs(param.client_rx);
  for (int a = 0; a < param.client_rx; a++)
    client_rx_ptrs[a] = client_rx_samples[a].data();

  ASSERT_EQ(client_device.trx_read_func(&client_device, &rx_ts, client_rx_ptrs.data(), nsamps, param.client_rx), nsamps);
  EXPECT_EQ(rx_ts, 0);

  // Verify DL mapping (Order mapping: min(server_tx, client_rx))
  int num_dl_mapped = std::min(param.server_tx, param.client_rx);
  for (int a = 0; a < num_dl_mapped; a++) {
    for (int i = 0; i < nsamps; i++) {
      EXPECT_EQ(client_rx_samples[a][i].r, server_tx_samples[a][i].r) << "DL Mismatch at index " << i << " antenna " << a;
      EXPECT_EQ(client_rx_samples[a][i].i, server_tx_samples[a][i].i) << "DL Mismatch at index " << i << " antenna " << a;
    }
  }
}

INSTANTIATE_TEST_SUITE_P(AntennaVariations,
                         VRTSIMTest,
                         ::testing::Values(VRTSIMTestCase{2, 2, 1, 1, "1x1"},
                                           VRTSIMTestCase{8, 8, 1, 1, "1x1"},
                                           VRTSIMTestCase{8, 8, 2, 2, "2x2"},
                                           VRTSIMTestCase{1, 1, 2, 2, "2x2"}));

struct VRTSIMMultiTestCase {
  int server_tx;
  int server_rx;
  int num_ues;
  std::vector<std::string> ue_antennas;
};

class VRTSIMMultiUETest : public ::testing::TestWithParam<VRTSIMMultiTestCase> {
 protected:
  configmodule_interface_t *cfg_server = nullptr;
  std::vector<configmodule_interface_t *> cfg_clients;
  openair0_device_t server_device = {0};
  std::vector<openair0_device_t> client_devices;
  openair0_config_t server_config = {0};
  std::vector<openair0_config_t> client_configs;

  void SetUp() override
  {
    const auto &param = GetParam();

    // Setup server
    std::vector<std::string> server_args_store = {"test_vrtsim",
                                                  "--vrtsim.role",
                                                  "server",
                                                  "--vrtsim.disable-timing-thread",
                                                  "1",
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

    // Setup clients
    client_devices.resize(param.num_ues);
    client_configs.resize(param.num_ues);
    cfg_clients.resize(param.num_ues);

    for (int i = 0; i < param.num_ues; i++) {
      std::vector<std::string> client_args_store = {"test_vrtsim", "--vrtsim.role", "client", "--vrtsim.ue_id", std::to_string(i)};
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
  }
};

TEST_P(VRTSIMMultiUETest, TransparentChannel)
{
  const auto &param = GetParam();
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

  // Prepare TX samples for server (DL)
  std::vector<std::vector<c16_t>> server_tx_samples(param.server_tx, std::vector<c16_t>(nsamps));
  std::vector<void *> server_tx_ptrs(param.server_tx);
  for (int a = 0; a < param.server_tx; a++) {
    for (int i = 0; i < nsamps; i++) {
      server_tx_samples[a][i] = {(int16_t)(i + a * 200), (int16_t)(i + a * 200 + 10)};
    }
    server_tx_ptrs[a] = server_tx_samples[a].data();
  }

  // All clients write at timestamp 0
  for (int u = 0; u < param.num_ues; u++) {
    int client_tx = 0, client_rx = 0;
    sscanf(param.ue_antennas[u].c_str(), "%dx%d", &client_tx, &client_rx);
    ASSERT_EQ(client_devices[u].trx_write_func(&client_devices[u], 0, client_tx_ptrs[u].data(), nsamps, client_tx, 0), nsamps);
  }
  // Server writes at timestamp 0
  ASSERT_EQ(server_device.trx_write_func(&server_device, 0, server_tx_ptrs.data(), nsamps, param.server_tx, 0), nsamps);

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

  // Verify UL combining (Order mapping)
  for (int aarx = 0; aarx < param.server_rx; aarx++) {
    for (int i = 0; i < nsamps; i++) {
      int32_t expected_r = 0;
      int32_t expected_i = 0;
      for (int u = 0; u < param.num_ues; u++) {
        int client_tx = 0, client_rx = 0;
        sscanf(param.ue_antennas[u].c_str(), "%dx%d", &client_tx, &client_rx);
        if (aarx < client_tx) {
          expected_r += client_tx_samples[u][aarx][i].r;
          expected_i += client_tx_samples[u][aarx][i].i;
        }
      }
      int16_t sat_r = (int16_t)((expected_r > 32767) ? 32767 : (expected_r < -32768) ? -32768 : expected_r);
      int16_t sat_i = (int16_t)((expected_i > 32767) ? 32767 : (expected_i < -32768) ? -32768 : expected_i);
      EXPECT_EQ(server_rx_samples[aarx][i].r, sat_r) << "UL Mismatch at sample " << i << " rx_ant " << aarx;
      EXPECT_EQ(server_rx_samples[aarx][i].i, sat_i) << "UL Mismatch at sample " << i << " rx_ant " << aarx;
    }
  }

  // Each client reads from server (DL) at timestamp 0
  for (int u = 0; u < param.num_ues; u++) {
    int client_tx = 0, client_rx = 0;
    sscanf(param.ue_antennas[u].c_str(), "%dx%d", &client_tx, &client_rx);

    std::vector<std::vector<c16_t>> client_rx_samples(client_rx, std::vector<c16_t>(nsamps));
    std::vector<void *> client_rx_ptrs(client_rx);
    for (int a = 0; a < client_rx; a++) {
      client_rx_ptrs[a] = client_rx_samples[a].data();
    }

    ASSERT_EQ(client_devices[u].trx_read_func(&client_devices[u], &rx_ts, client_rx_ptrs.data(), nsamps, client_rx), nsamps);
    EXPECT_EQ(rx_ts, 0);

    // Verify DL mapping for UE u
    int num_dl_mapped = std::min(param.server_tx, client_rx);
    for (int a = 0; a < num_dl_mapped; a++) {
      for (int i = 0; i < nsamps; i++) {
        EXPECT_EQ(client_rx_samples[a][i].r, server_tx_samples[a][i].r)
            << "DL Mismatch UE " << u << " ant " << a << " sample " << i;
        EXPECT_EQ(client_rx_samples[a][i].i, server_tx_samples[a][i].i)
            << "DL Mismatch UE " << u << " ant " << a << " sample " << i;
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(MultiUEVariations,
                         VRTSIMMultiUETest,
                         ::testing::Values(VRTSIMMultiTestCase{2, 2, 2, {"2x2", "1x2"}},
                                           VRTSIMMultiTestCase{2, 2, 2, {"1x2", "1x2"}}));

int main(int argc, char **argv)
{
  logInit();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
