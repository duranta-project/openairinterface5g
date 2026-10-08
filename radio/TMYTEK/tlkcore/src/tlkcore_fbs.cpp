/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <nlohmann/json.hpp>
#include <tlkcore_lib.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

using json = nlohmann::json;
using tlkcore::tlkcore_lib;

std::atomic_bool stop_requested{false};

void handle_signal(int)
{
  stop_requested.store(true);
}

struct beamformer_config {
  std::string serial_number;
  std::string beam_config;
  float frequency;
  int rf_mode;
};

struct downconverter_config {
  std::string serial_number;
  int frequency_hz;
  int rf_frequency_hz;
  int if_frequency_hz;
};

struct application_config {
  std::vector<beamformer_config> beamformers;
  std::vector<downconverter_config> downconverters;
};

void print_usage(const char *program)
{
  std::cout << "Usage: " << program << " [--config PATH] [--tlkcore-root PATH]\n"
            << "  --config PATH        TLKCore device configuration JSON\n"
            << "  --tlkcore-root PATH Python TLKCore package root\n";
}

std::string require_string(const json &object, const char *key, const std::string &context)
{
  if (!object.contains(key) || !object.at(key).is_string())
    throw std::runtime_error("Missing string '" + std::string(key) + "' in " + context);
  return object.at(key).get<std::string>();
}

float require_frequency(const json &object, const std::string &context)
{
  if (!object.contains("targetFreq") || !object.at("targetFreq").is_number())
    throw std::runtime_error("Missing numeric 'targetFreq' in " + context);
  return object.at("targetFreq").get<float>();
}

int require_rf_mode(const json &object, const std::string &context)
{
  if (!object.contains("RFMode") || !object.at("RFMode").is_number_integer())
    throw std::runtime_error("Missing integer 'RFMode' in " + context);
  const int rf_mode = object.at("RFMode").get<int>();
  if (rf_mode < tlkcore::MODE_TX || rf_mode > tlkcore::MODE_RX)
    throw std::runtime_error("Invalid RFMode in " + context);
  return rf_mode;
}

application_config read_config(const std::filesystem::path &config_path)
{
  std::ifstream input(config_path);
  if (!input)
    throw std::runtime_error("Cannot open configuration: " + config_path.string());

  json root;
  input >> root;
  application_config config;

  const auto &beamformers = root.at("BF_LAYERS");
  for (const auto &[serial_number, value] : beamformers.items()) {
    const std::string context = "BF_LAYERS." + serial_number;
    beamformer_config beamformer{
        serial_number,
        require_string(value, "BEAM_CONFIG", context),
        require_frequency(value, context),
      require_rf_mode(value, context),
    };
    config.beamformers.push_back(std::move(beamformer));
  }

  const auto &downconverters = root.at("UD_LAYERS");
  for (const auto &[serial_number, value] : downconverters.items()) {
    const std::string context = "UD_LAYERS." + serial_number;
    const float target_frequency = require_frequency(value, context);
    if (!value.contains("ifFrequency") || !value.at("ifFrequency").is_number())
      throw std::runtime_error("Missing numeric 'ifFrequency' in " + context);
    const int if_frequency = static_cast<int>(value.at("ifFrequency").get<float>() * 1e6F);
    const int rf_frequency = static_cast<int>(target_frequency * 1e6F);
    config.downconverters.push_back(
        {serial_number, rf_frequency - if_frequency, rf_frequency, if_frequency});
  }

  if (config.beamformers.empty())
    throw std::runtime_error("BF_LAYERS must contain at least one device");
  return config;
}

void set_fast_parallel_mode(const tlkcore_lib::tlkcore_ptr &service,
                            const application_config &config,
                            bool enabled)
{
  for (const auto &beamformer : config.beamformers) {
    bool current = false;
    if (service->get_fast_parallel_mode(beamformer.serial_number, current) < 0)
      throw std::runtime_error("Cannot read fast-parallel mode for " + beamformer.serial_number);
    if (current != enabled &&
        service->set_fast_parallel_mode(beamformer.serial_number, enabled, beamformer.frequency) < 0)
      throw std::runtime_error("Cannot set fast-parallel mode for " + beamformer.serial_number);
  }
}

void initialize_devices(const tlkcore_lib::tlkcore_ptr &service, const application_config &config)
{
  for (const auto &downconverter : config.downconverters) {
    if (service->set_ud_freq(downconverter.serial_number,
                             downconverter.frequency_hz,
                             downconverter.rf_frequency_hz,
                             downconverter.if_frequency_hz) < 0)
      throw std::runtime_error("Cannot configure downconverter " + downconverter.serial_number);
  }

  for (const auto &beamformer : config.beamformers) {
    if (service->set_beam_angle(beamformer.serial_number,
                                beamformer.frequency,
                                static_cast<tlkcore::rf_mode_t>(beamformer.rf_mode),
                                0.0F,
                                0,
                                0) < 0)
      throw std::runtime_error("Cannot set RF mode for " + beamformer.serial_number);

    bool fast_parallel = false;
    if (service->get_fast_parallel_mode(beamformer.serial_number, fast_parallel) < 0)
      throw std::runtime_error("Cannot read fast-parallel mode for " + beamformer.serial_number);
    if (!fast_parallel && service->apply_beam_patterns(beamformer.serial_number, beamformer.frequency) < 0)
      throw std::runtime_error("Cannot apply beam patterns for " + beamformer.serial_number);
  }
}

} // namespace

int main(int argc, char **argv)
{
  std::filesystem::path config_path = "config/device.conf";
  std::string tlkcore_root;
  application_config config;
  tlkcore_lib::tlkcore_ptr service;
  bool fast_parallel_cleanup_required = false;

  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--config" && index + 1 < argc) {
      config_path = argv[++index];
    } else if (argument == "--tlkcore-root" && index + 1 < argc) {
      tlkcore_root = argv[++index];
    } else {
      print_usage(argv[0]);
      return 2;
    }
  }

  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);

  try {
    config = read_config(config_path);
    service =
        tlkcore_root.empty() ? tlkcore_lib::make() : tlkcore_lib::make(tlkcore_root);

    if (service->scan_init_dev(config_path.string()) < 0)
      throw std::runtime_error("TLKCore device discovery failed");
    initialize_devices(service, config);
    fast_parallel_cleanup_required = true;
    set_fast_parallel_mode(service, config, true);

    std::cout << "TMYTEK arrays are in fast-parallel mode; OAI may control beams.\n"
              << "Press Ctrl+C to stop and restore normal mode.\n";
    while (!stop_requested.load())
      std::this_thread::sleep_for(std::chrono::milliseconds(200));

    set_fast_parallel_mode(service, config, false);
    fast_parallel_cleanup_required = false;
  } catch (const std::exception &error) {
    if (fast_parallel_cleanup_required && service) {
      try {
        set_fast_parallel_mode(service, config, false);
      } catch (const std::exception &cleanup_error) {
        std::cerr << "TLKCore cleanup failed: " << cleanup_error.what() << '\n';
      }
    }
    std::cerr << "TLKCore controller failed: " << error.what() << '\n';
    return 1;
  }

  return 0;
}
