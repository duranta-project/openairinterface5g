/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "chanmod_engine.h"
#include "log.h"
#include <common/utils/assertions.h>
#include <algorithm>
#include <cmath>
#include <cstring>

static int16_t clamp_to_c16(double v)
{
  return (int16_t)std::clamp(v, -32768.0, 32767.0);
}

void chanmod_apply_effects(c16_t *samples,
                           size_t count,
                           double phase_inc,
                           float *phase_cur,
                           double path_loss_dB,
                           float noise_power_dB)
{
  const bool rotate = phase_cur != nullptr && phase_inc != 0.0;
  const bool scale = path_loss_dB != 0.0;
  const bool add_noise = noise_power_dB > -1000.0f;
  if (!rotate && !scale && !add_noise)
    return;
  double phase = rotate ? *phase_cur : 0.0;
  const double path_loss_linear = pow(10.0, path_loss_dB / 20.0);
  const double noise_per_sample = add_noise ? pow(10.0, noise_power_dB / 10.0) * 256 : 0.0;

  for (size_t i = 0; i < count; i++) {
    double r = samples[i].r;
    double im = samples[i].i;
    if (rotate) {
      const double cos_p = cos(phase);
      const double sin_p = sin(phase);
      const double real = r * cos_p - im * sin_p;
      const double imag = r * sin_p + im * cos_p;
      r = real;
      im = imag;
      phase += phase_inc;
    }
    r *= path_loss_linear;
    im *= path_loss_linear;
    if (add_noise) {
      r += noise_per_sample * gaussZiggurat(0.0, 1.0);
      im += noise_per_sample * gaussZiggurat(0.0, 1.0);
    }
    samples[i].r = clamp_to_c16(r);
    samples[i].i = clamp_to_c16(im);
  }
  if (rotate) {
    phase = std::remainder(phase, 2.0 * M_PI);
    *phase_cur = phase;
  }
}

// History spans [src_ts - size, src_ts). Select a fresh source window and zero-fill unavailable samples.
static void select_window(std::vector<ring_buffer<c16_t>> &history,
                          uint64_t src_ts,
                          c16_t **out,
                          int num_antennas,
                          size_t nsamps,
                          uint64_t window_start)
{
  const uint64_t history_start = src_ts - history[0].size();
  // Interpret wrapped startup timestamps as positions before history_start.
  const int64_t gap = (int64_t)(window_start - history_start);
  const size_t out_offset = gap < 0 ? std::min<size_t>((size_t)-gap, nsamps) : 0;
  const size_t skip_from_tail = gap > 0 ? (size_t)gap : 0;
  for (int a = 0; a < num_antennas; a++) {
    memset(out[a], 0, nsamps * sizeof(c16_t));
    if (out_offset < nsamps)
      history[a].copy_range(out[a] + out_offset, nsamps - out_offset, skip_from_tail);
  }
}

ChanmodTxEngine::ChanmodTxEngine(int num_antennas, uint64_t history_size)
    : doppler_phase_cur_(num_antennas, 0.0f), history_size_(history_size)
{
  history_.reserve(num_antennas);
  for (int a = 0; a < num_antennas; a++)
    history_.emplace_back(history_size);
}

void ChanmodTxEngine::start(uint64_t init_time)
{
  src_ts_ = deliver_ts_ = init_time;
  for (auto &h : history_)
    h.reset();
  std::fill(doppler_phase_cur_.begin(), doppler_phase_cur_.end(), 0.0f);
  source_started_ = false;
  started_ = true;
}

void ChanmodTxEngine::reanchor_if_discontinuous(uint64_t timestamp, size_t nsamps)
{
  if (source_started_ && timestamp != src_ts_) {
    const uint64_t forward_gap = timestamp > src_ts_ ? timestamp - src_ts_ : 0;
    if (forward_gap > 0 && forward_gap <= nsamps) {
      for (auto &h : history_)
        h.push_zeros(forward_gap);
      src_ts_ = timestamp;
      return;
    }
    LOG_W(HW,
          "[chanmod] TX timestamp discontinuity: received %lu, expected %lu; resetting channel history\n",
          (unsigned long)timestamp,
          (unsigned long)src_ts_);
    start(timestamp);
  }
}

void ChanmodTxEngine::write(c16_t **samples,
                            int num_antennas,
                            size_t nsamps,
                            uint64_t timestamp,
                            int flags,
                            channel_desc_t *model,
                            const ChanmodUpdateFn &update,
                            const ChanmodWriteFn &sink)
{
  AssertFatal(started_, "ChanmodTxEngine::write() called before start()\n");
  reanchor_if_discontinuous(timestamp, nsamps);
  if (!source_started_) {
    src_ts_ = deliver_ts_ = timestamp;
    source_started_ = true;
  }
  // Update on the output clock before selecting the delayed source window.
  if (update)
    update(deliver_ts_, nsamps);
  const uint64_t channel_offset = model ? model->channel_offset : 0;
  // Warn about insufficient capacity, not routine eviction of old samples.
  if (!history_capacity_warned_ && channel_offset + nsamps > history_size_) {
    LOG_W(HW,
          "[chanmod] TX channel_offset (%lu) leaves less than nsamps (%zu) of margin in history_ "
          "(capacity %lu), window will be partially zero-filled\n",
          (unsigned long)channel_offset,
          nsamps,
          (unsigned long)history_size_);
    history_capacity_warned_ = true;
  }

  for (int a = 0; a < num_antennas; a++)
    history_[a].push_samples(samples[a], nsamps);
  src_ts_ += nsamps;

  // select_window() handles unsigned underflow when delay precedes timestamp zero.
  const uint64_t window_start = deliver_ts_ - channel_offset;

  std::vector<std::vector<c16_t>> out(num_antennas, std::vector<c16_t>(nsamps));
  std::vector<void *> ptrs(num_antennas);
  std::vector<c16_t *> out_ptrs(num_antennas);
  for (int a = 0; a < num_antennas; a++)
    out_ptrs[a] = out[a].data();
  select_window(history_, src_ts_, out_ptrs.data(), num_antennas, nsamps, window_start);

  const double phase_inc = model ? model->Doppler_phase_inc : 0.0;
  const double path_loss_dB = model ? model->path_loss_dB : 0.0;
  const float noise_power_dB = model ? model->noise_power_dB : -1000.0f;
  for (int a = 0; a < num_antennas; a++) {
    // Advance phase only for delivered samples, not buffered history.
    chanmod_apply_effects(out[a].data(), nsamps, phase_inc, &doppler_phase_cur_[a], path_loss_dB, noise_power_dB);
    ptrs[a] = out[a].data();
  }
  int wrote = sink(deliver_ts_, ptrs.data(), (int)nsamps, num_antennas, flags);
  if ((size_t)std::max(wrote, 0) != nsamps)
    LOG_W(HW, "[chanmod] TX wrote %d of %zu samples\n", wrote, nsamps);
  deliver_ts_ += nsamps;
}

ChanmodRxEngine::ChanmodRxEngine(int num_antennas, uint64_t history_size)
    : doppler_phase_cur_(num_antennas, 0.0f), history_size_(history_size)
{
  history_.reserve(num_antennas);
  for (int a = 0; a < num_antennas; a++)
    history_.emplace_back(history_size);
}

void ChanmodRxEngine::start(uint64_t init_time)
{
  src_ts_ = deliver_ts_ = init_time;
  for (auto &h : history_)
    h.reset();
  std::fill(doppler_phase_cur_.begin(), doppler_phase_cur_.end(), 0.0f);
  started_ = true;
}

uint64_t ChanmodRxEngine::read(c16_t **out,
                               int num_antennas,
                               size_t nsamps,
                               channel_desc_t *model,
                               const ChanmodUpdateFn &update,
                               const ChanmodReadFn &source)
{
  AssertFatal(started_, "ChanmodRxEngine::read() called before start()\n");

  // Update once per output block, not once per source pull.
  if (update)
    update(deliver_ts_, nsamps);
  const uint64_t channel_offset = model ? model->channel_offset : 0;
  if (!history_capacity_warned_ && channel_offset + nsamps > history_size_) {
    LOG_W(HW,
          "[chanmod] RX channel_offset (%lu) leaves less than nsamps (%zu) of margin in history_ "
          "(capacity %lu), window will be partially zero-filled\n",
          (unsigned long)channel_offset,
          nsamps,
          (unsigned long)history_size_);
    history_capacity_warned_ = true;
  }
  const uint64_t window_start = deliver_ts_ - channel_offset;

  // Keep the end signed so windows before timestamp zero do not trigger source pulls.
  const int64_t window_end = (int64_t)deliver_ts_ - (int64_t)channel_offset + (int64_t)nsamps;

  while ((int64_t)src_ts_ < window_end) {
    std::vector<std::vector<c16_t>> block(num_antennas, std::vector<c16_t>(nsamps));
    std::vector<void *> ptrs(num_antennas);
    for (int a = 0; a < num_antennas; a++)
      ptrs[a] = block[a].data();
    openair0_timestamp_t src_ts;
    const int got = source(&src_ts, ptrs.data(), nsamps, num_antennas);
    const size_t got_n = (size_t)std::max(got, 0);
    if (got_n == 0) {
      LOG_W(HW, "[chanmod] RX source returned no samples, stopping fill early\n");
      break;
    }
    AssertFatal((uint64_t)src_ts == src_ts_, "ChanmodRxEngine::read() requires a contiguous source stream\n");
    for (int a = 0; a < num_antennas; a++)
      history_[a].push_samples(block[a].data(), got_n);
    src_ts_ += got_n;
  }

  select_window(history_, src_ts_, out, num_antennas, nsamps, window_start);

  const double phase_inc = model ? model->Doppler_phase_inc : 0.0;
  const double path_loss_dB = model ? model->path_loss_dB : 0.0;
  const float noise_power_dB = model ? model->noise_power_dB : -1000.0f;
  for (int a = 0; a < num_antennas; a++) {
    chanmod_apply_effects(out[a], nsamps, phase_inc, &doppler_phase_cur_[a], path_loss_dB, noise_power_dB);
  }
  const uint64_t ts = deliver_ts_;
  deliver_ts_ += nsamps;
  return ts;
}
