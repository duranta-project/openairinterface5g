/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef CHANMOD_ENGINE_H
#define CHANMOD_ENGINE_H

#include "common/platform_types.h"
#include "common_lib.h"
#include "ring_buffer.h"
#include <cstdint>
#include <functional>
#include <vector>

extern "C" {
#include <openair1/SIMULATION/TOOLS/sim.h>
}

// Match openair0_device_t read/write callbacks.
using ChanmodWriteFn = std::function<int(openair0_timestamp_t timestamp, void **buff, int nsamps, int nb_antennas_tx, int flags)>;
using ChanmodReadFn = std::function<int(openair0_timestamp_t *ptimestamp, void **buff, int nsamps, int num_antennas)>;

// Optional model update, called once per output block before reading its parameters.
using ChanmodUpdateFn = std::function<void(uint64_t timestamp, size_t nsamps)>;

// Apply Doppler, path loss, and noise in place; preserve phase across output blocks.
void chanmod_apply_effects(c16_t *samples,
                           size_t count,
                           double phase_inc,
                           float *phase_cur,
                           double path_loss_dB,
                           float noise_power_dB);

// Buffer contiguous raw TX samples and emit nsamps starting at output_timestamp - channel_offset.
// Zero-fill unavailable history and apply effects only to the selected output window.
class ChanmodTxEngine {
 public:
  ChanmodTxEngine(int num_antennas, uint64_t history_size);

  uint64_t next_output_timestamp() const
  {
    return deliver_ts_;
  }
  void reanchor_if_discontinuous(uint64_t timestamp, size_t nsamps);
  void start(uint64_t init_time);
  void write(c16_t **samples,
             int num_antennas,
             size_t nsamps,
             uint64_t timestamp,
             int flags,
             channel_desc_t *model,
             const ChanmodUpdateFn &update,
             const ChanmodWriteFn &sink);

 private:
  std::vector<ring_buffer<c16_t>> history_;
  std::vector<float> doppler_phase_cur_;
  uint64_t history_size_;
  uint64_t src_ts_ = 0;
  uint64_t deliver_ts_ = 0;
  bool started_ = false;
  bool source_started_ = false;
  bool history_capacity_warned_ = false;
};

// Pull contiguous raw RX samples until history covers output_timestamp - channel_offset, then select and apply effects.
// Returns the first output timestamp; unavailable samples are zero-filled.
class ChanmodRxEngine {
 public:
  ChanmodRxEngine(int num_antennas, uint64_t history_size);

  uint64_t next_output_timestamp() const
  {
    return deliver_ts_;
  }
  void start(uint64_t init_time);
  uint64_t read(c16_t **out,
                int num_antennas,
                size_t nsamps,
                channel_desc_t *model,
                const ChanmodUpdateFn &update,
                const ChanmodReadFn &source);

 private:
  std::vector<ring_buffer<c16_t>> history_;
  std::vector<float> doppler_phase_cur_;
  uint64_t history_size_;
  uint64_t src_ts_ = 0;
  uint64_t deliver_ts_ = 0;
  bool started_ = false;
  bool history_capacity_warned_ = false;
};

#endif
