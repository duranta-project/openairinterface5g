/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "channel_emulation.h"
#include "chanmod_engine.h"
#include "common/config/config_userapi.h"
#include "common/utils/LOG/log.h"
#include "ntn_sim_clock_publisher.h"
#include "openair1/SIMULATION/TOOLS/ntn_channel_profile.h"
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>

extern "C" {
#include "openair1/SIMULATION/TOOLS/sim.h"
}

namespace {

constexpr char chanmod_option[] = "chanmod";

#define CHANNEL_EMULATION_OPTIONS "options"
#define CHANNEL_EMULATION_MODEL "modelname"
#define CHANNEL_EMULATION_NTN_FILE "ntn_file"

#define CHANNEL_EMULATION_PARAMS_DESC                                         \
  {                                                                           \
    STRINGLISTPARAM(CHANNEL_EMULATION_OPTIONS,                                \
                    "list of channel-emulation options\n",                    \
                    PARAMFLAG_CMDLINE_NOPREFIXENABLED,                        \
                    nullptr,                                                  \
                    nullptr),                                                 \
        STRINGPARAM(CHANNEL_EMULATION_MODEL,                                  \
                    "channel model name (with options=chanmod)\n",            \
                    PARAMFLAG_CMDLINE_NOPREFIXENABLED,                        \
                    nullptr,                                                  \
                    "AWGN"),                                                  \
        STRINGPARAM(CHANNEL_EMULATION_NTN_FILE,                               \
                    "NTN delay/doppler profile CSV (with options=chanmod)\n", \
                    PARAMFLAG_CMDLINE_NOPREFIXENABLED,                        \
                    nullptr,                                                  \
                    nullptr),                                                 \
  }

enum class channel_direction { uplink, downlink };

struct channel_emulation_context {
  ~channel_emulation_context()
  {
    if (tx_model != nullptr) {
      free_channel_desc_scm(tx_model);
    }
    if (rx_model != nullptr) {
      free_channel_desc_scm(rx_model);
    }
    free_ntn_channel_profile(ntn_profile);
  }

  openair0_device_t *device;
  int (*original_start)(openair0_device_t *device);
  int (*original_write)(openair0_device_t *device,
                        openair0_timestamp_t timestamp,
                        void **buff,
                        int nsamps,
                        int nb_antennas,
                        int flags);
  int (*original_read)(openair0_device_t *device, openair0_timestamp_t *timestamp, void **buff, int nsamps, int nb_antennas);
  void (*original_end)(openair0_device_t *device);
  double sample_rate;
  channel_desc_t *tx_model = nullptr;
  channel_desc_t *rx_model = nullptr;
  ntn_channel_profile_t *ntn_profile = nullptr;
  uint64_t ntn_start_timestamp = 0;
  std::unique_ptr<NtnSimClockPublisher> sim_clock;
  std::mutex tx_model_mutex;
  std::mutex rx_model_mutex;
  std::unique_ptr<ChanmodTxEngine> tx_engine;
  std::unique_ptr<ChanmodRxEngine> rx_engine;
};

std::mutex contexts_mutex;
std::map<openair0_device_t *, std::unique_ptr<channel_emulation_context>> contexts;

channel_emulation_context *get_context(openair0_device_t *device)
{
  std::lock_guard<std::mutex> lock(contexts_mutex);
  auto it = contexts.find(device);
  return it != contexts.end() ? it->second.get() : nullptr;
}

channel_desc_t *create_model(const openair0_config_t *device_config,
                             const channel_emulation_config &config,
                             SCM_t model_id,
                             bool is_tx)
{
  char suffixed_name[64];
  snprintf(suffixed_name, sizeof(suffixed_name), "%s_%s", config.model_name.c_str(), is_tx ? "tx" : "rx");
  channel_desc_t *model = config.ntn_profile_path.empty() ? find_channel_desc_fromname(suffixed_name) : nullptr;
  if (model != nullptr) {
    return model;
  }

  model = new_channel_desc_scm(device_config->tx_num_channels,
                               device_config->rx_num_channels,
                               model_id,
                               device_config->sample_rate,
                               static_cast<uint64_t>(device_config->rx_freq[0]),
                               0.0,
                               0.0,
                               0.0,
                               CORR_LEVEL_LOW,
                               0.0,
                               0L,
                               0.0,
                               config.ntn_profile_path.empty() ? -100.0f : -1000.0f);
  if (model == nullptr) {
    return nullptr;
  }
  set_channeldesc_name(model, suffixed_name);
  set_channeldesc_owner(model, RFSIMU_MODULEID);
  set_channeldesc_direction(model, is_tx);
  random_channel(model, false);
  return model;
}

bool update_ntn_model(channel_emulation_context &context,
                      channel_desc_t *model,
                      std::mutex &mutex,
                      channel_direction direction,
                      uint64_t output_timestamp)
{
  if (context.ntn_profile == nullptr) {
    return true;
  }

  std::lock_guard<std::mutex> lock(mutex);
  const double profile_time_s = static_cast<double>(output_timestamp - context.ntn_start_timestamp) / context.sample_rate;
  ntn_channel_update_t update;
  if (!get_ntn_channel_update(context.ntn_profile, profile_time_s, &update)) {
    LOG_E(HW, "[chanmod] NTN profile does not cover sample time %.6f s\n", profile_time_s);
    return false;
  }

  model->channel_offset = static_cast<uint64_t>(update.delay_ms * 1e-3 * model->sampling_rate);
  const double doppler_hz = direction == channel_direction::uplink ? update.ul_doppler_hz : update.dl_doppler_hz;
  model->Doppler_phase_inc = 2.0 * M_PI * doppler_hz / model->sampling_rate;
  return true;
}

int channel_emulation_start(openair0_device_t *device)
{
  channel_emulation_context *context = get_context(device);
  if (context == nullptr) {
    return -1;
  }

  const uint64_t initial_timestamp = static_cast<uint64_t>(context->sample_rate / 100);
  context->tx_model->start_TS = initial_timestamp;
  context->rx_model->start_TS = initial_timestamp;
  context->ntn_start_timestamp = initial_timestamp;
  if (context->sim_clock != nullptr)
    context->sim_clock->start(initial_timestamp);
  context->tx_engine->start(initial_timestamp);
  context->rx_engine->start(initial_timestamp);
  return context->original_start(device);
}

int channel_emulation_write(openair0_device_t *device,
                            openair0_timestamp_t timestamp,
                            void **buff,
                            int nsamps,
                            int nb_antennas,
                            int flags)
{
  channel_emulation_context *context = get_context(device);
  if (context == nullptr) {
    return -1;
  }
  context->tx_engine->reanchor_if_discontinuous(static_cast<uint64_t>(timestamp), static_cast<size_t>(nsamps));
  if (!update_ntn_model(*context,
                        context->tx_model,
                        context->tx_model_mutex,
                        channel_direction::uplink,
                        context->tx_engine->next_output_timestamp())) {
    return -1;
  }
  context->tx_engine->write(reinterpret_cast<c16_t **>(buff),
                            nb_antennas,
                            static_cast<size_t>(nsamps),
                            static_cast<uint64_t>(timestamp),
                            flags,
                            context->tx_model,
                            {},
                            [context](auto ts, auto b, auto n, auto nant, auto f) {
                              return context->original_write(context->device, ts, b, n, nant, f);
                            });
  return nsamps;
}

int channel_emulation_read(openair0_device_t *device, openair0_timestamp_t *timestamp, void **buff, int nsamps, int nb_antennas)
{
  channel_emulation_context *context = get_context(device);
  if (context != nullptr && context->sim_clock != nullptr)
    context->sim_clock->publish(context->rx_engine->next_output_timestamp());
  if (context == nullptr
      || !update_ntn_model(*context,
                           context->rx_model,
                           context->rx_model_mutex,
                           channel_direction::downlink,
                           context->rx_engine->next_output_timestamp())) {
    return -1;
  }
  *timestamp = context->rx_engine->read(
      reinterpret_cast<c16_t **>(buff),
      nb_antennas,
      static_cast<size_t>(nsamps),
      context->rx_model,
      {},
      [context](auto ts, auto b, auto n, auto nant) { return context->original_read(context->device, ts, b, n, nant); });
  return nsamps;
}

void channel_emulation_end(openair0_device_t *device)
{
  std::unique_ptr<channel_emulation_context> context;
  {
    std::lock_guard<std::mutex> lock(contexts_mutex);
    auto it = contexts.find(device);
    if (it == contexts.end()) {
      return;
    }
    context = std::move(it->second);
    contexts.erase(it);
  }
  context->original_end(device);
}

} // namespace

bool install_channel_emulation(openair0_device_t *device, const openair0_config_t *device_config, const char *config_section)
{
  paramdef_t params[] = CHANNEL_EMULATION_PARAMS_DESC;
  configmodule_interface_t *config = config_get_if();
  if (config_get(config, params, sizeofArray(params), config_section) < 0) {
    return false;
  }

  const int options_index = config_paramidx_fromname(params, sizeofArray(params), CHANNEL_EMULATION_OPTIONS);
  channel_emulation_config emulation_config;
  for (int i = 0; i < params[options_index].numelt; ++i) {
    if (strcmp(params[options_index].strlistptr[i], chanmod_option) == 0) {
      emulation_config.enabled = true;
      break;
    }
  }
  if (!emulation_config.enabled) {
    return true;
  }

  const char *configured_model = *gpd(params, sizeofArray(params), CHANNEL_EMULATION_MODEL)->strptr;
  emulation_config.model_name = configured_model;
  const int model_id = modelid_fromstrtype(const_cast<char *>(configured_model));
  if (model_id < 0) {
    LOG_E(HW, "[chanmod] Unknown channel model %s\n", configured_model);
    return false;
  }
  const char *ntn_file = *gpd(params, sizeofArray(params), CHANNEL_EMULATION_NTN_FILE)->strptr;
  if (ntn_file != nullptr) {
    emulation_config.ntn_profile_path = ntn_file;
  }
  if (ntn_file != nullptr && model_id != SAT_LEO_TLE) {
    LOG_E(HW, "[chanmod] NTN profile requires model SAT_LEO_TLE\n");
    return false;
  }

  randominit();
  init_channelmod();
  auto context = std::make_unique<channel_emulation_context>();
  context->device = device;
  context->original_start = device->trx_start_func;
  context->original_write = device->trx_write_func;
  context->original_read = device->trx_read_func;
  context->original_end = device->trx_end_func;
  context->sample_rate = device_config->sample_rate;
  context->ntn_profile = ntn_file != nullptr ? load_ntn_channel_profile(emulation_config.ntn_profile_path.c_str()) : nullptr;
  if (ntn_file != nullptr && context->ntn_profile == nullptr) {
    return false;
  }
  if (context->ntn_profile != nullptr) {
    double epoch_unix_s;
    if (!get_ntn_channel_profile_epoch(context->ntn_profile, &epoch_unix_s)) {
      LOG_E(HW, "[chanmod] NTN simulation clock requires # start_unix_s profile metadata\n");
      return false;
    }
    context->sim_clock = NtnSimClockPublisher::create(epoch_unix_s, device_config->sample_rate);
    if (context->sim_clock == nullptr)
      return false;
  }
  context->tx_model = create_model(device_config, emulation_config, static_cast<SCM_t>(model_id), true);
  context->rx_model = create_model(device_config, emulation_config, static_cast<SCM_t>(model_id), false);
  if (context->tx_model == nullptr || context->rx_model == nullptr) {
    return false;
  }
  const uint64_t history_size = static_cast<uint64_t>(device_config->sample_rate);
  context->tx_engine = std::make_unique<ChanmodTxEngine>(device_config->tx_num_channels, history_size);
  context->rx_engine = std::make_unique<ChanmodRxEngine>(device_config->rx_num_channels, history_size);

  {
    std::lock_guard<std::mutex> lock(contexts_mutex);
    contexts.emplace(device, std::move(context));
  }
  device->trx_start_func = channel_emulation_start;
  device->trx_write_func = channel_emulation_write;
  device->trx_read_func = channel_emulation_read;
  device->trx_end_func = channel_emulation_end;
  LOG_I(HW, "[chanmod] Installed channel model %s%s\n", configured_model, ntn_file != nullptr ? " with NTN profile" : "");
  return true;
}
