/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */
#include "ntn_radio_time.h"

#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>

#include "common/ran_context.h"
#include "openair1/PHY/defs_gNB.h"

#define NTN_RADIO_MAX_AGE_NS UINT64_C(200000000)

static struct {
  _Atomic(RU_t *) ru;
  atomic_uint_fast64_t generation;
  pthread_mutex_t mutex;
  bool valid;
  nr_ntn_radio_time_t observation;
  NR_DL_FRAME_PARMS fp;
  /* RX writer only; init/disable are serialized by the caller's lifecycle. */
  bool previous_valid;
  int64_t previous_timestamp;
  int64_t previous_offset;
  int previous_frame;
  int previous_slot;
} radio_time = {.mutex = PTHREAD_MUTEX_INITIALIZER};

static bool monotonic_ns(uint64_t *ns)
{
  struct timespec now;
  if (clock_gettime(CLOCK_MONOTONIC, &now) != 0 || now.tv_sec < 0 || now.tv_nsec < 0 || now.tv_nsec >= 1000000000
      || (uint64_t)now.tv_sec > (UINT64_MAX - (uint64_t)now.tv_nsec) / UINT64_C(1000000000))
    return false;
  *ns = (uint64_t)now.tv_sec * UINT64_C(1000000000) + (uint64_t)now.tv_nsec;
  return true;
}

/* One lock-free RMW attempt: concurrent disable already invalidates the copy.
 * Generation zero is reserved for invalid/overflow and cannot become active. */
static uint64_t invalidate(void)
{
  uint_fast64_t previous = atomic_load_explicit(&radio_time.generation, memory_order_acquire);
  if (previous == UINT64_MAX) {
    atomic_store_explicit(&radio_time.ru, NULL, memory_order_release);
    return 0;
  }
  if (!atomic_compare_exchange_strong_explicit(&radio_time.generation,
                                               &previous,
                                               previous + 1,
                                               memory_order_acq_rel,
                                               memory_order_acquire))
    return 0;
  return previous + 1;
}

static bool frame_parameters_valid(const NR_DL_FRAME_PARMS *fp, double rate)
{
  if (fp == NULL || fp->numerology_index > 5 || fp->slots_per_subframe != (1U << fp->numerology_index)
      || fp->slots_per_frame != 10U * fp->slots_per_subframe || fp->samples_per_subframe == 0
      || (uint64_t)fp->samples_per_frame != UINT64_C(10) * fp->samples_per_subframe || !isfinite(rate) || rate <= 0
      || rate != (double)fp->samples_per_frame * 100.0)
    return false;

  uint64_t samples = 0;
  for (unsigned int slot = 0; slot < fp->slots_per_frame; ++slot) {
    const uint32_t count = get_samples_per_slot(slot, fp);
    if (count == 0 || get_samples_slot_timestamp(fp, slot) != samples)
      return false;
    samples += count;
  }
  return samples == fp->samples_per_frame && get_samples_slot_timestamp(fp, fp->slots_per_frame) == samples;
}

bool nr_ntn_radio_time_init(RU_t *ru)
{
  if (!atomic_is_lock_free(&radio_time.ru) || !atomic_is_lock_free(&radio_time.generation)
      || atomic_load_explicit(&radio_time.ru, memory_order_acquire) != NULL || ru == NULL || RC.nb_RU != 1 || RC.ru == NULL
      || RC.ru[0] != ru || RC.nb_nr_L1_inst != 1 || RC.gNB == NULL || ru->idx != 0 || ru->if_south != LOCAL_RF || ru->num_gNB != 1
      || ru->gNB_list[0] == NULL || RC.gNB[0] != ru->gNB_list[0] || ru->gNB_list[0]->Mod_id != 0 || ru->gNB_list[0]->CC_id != 0
      || ru->gNB_list[0]->num_RU != 1 || ru->gNB_list[0]->RU_list[0] != ru || ru->rfdevice.trx_get_time_func == NULL
      || ru->rfdevice.openair0_cfg == NULL || ru->rfdevice.openair0_cfg->sample_rate != ru->openair0_cfg.sample_rate
      || !frame_parameters_valid(ru->nr_frame_parms, ru->openair0_cfg.sample_rate))
    return false;

  pthread_mutex_lock(&radio_time.mutex);
  radio_time.fp = *ru->nr_frame_parms;
  radio_time.valid = false;
  radio_time.previous_valid = false;
  const bool ready = invalidate() != 0;
  if (ready)
    atomic_store_explicit(&radio_time.ru, ru, memory_order_release);
  pthread_mutex_unlock(&radio_time.mutex);
  return ready;
}

void nr_ntn_radio_time_rx(RU_t *ru, int frame, int slot, bool complete)
{
  if (ru == NULL || atomic_load_explicit(&radio_time.ru, memory_order_acquire) != ru)
    return;

  const NR_DL_FRAME_PARMS *fp = &radio_time.fp;
  const int64_t timestamp = ru->proc.timestamp_rx;
  const int64_t offset = ru->ts_offset;
  if (!complete || ru->proc.first_rx || timestamp < 0 || frame < 0 || frame >= 1024 || slot < 0 || slot >= fp->slots_per_frame
      || timestamp % fp->samples_per_frame != get_samples_slot_timestamp(fp, slot)
      || (timestamp / fp->samples_per_frame) % 1024 != frame) {
    radio_time.previous_valid = false;
    invalidate();
    return;
  }

  if (radio_time.previous_valid) {
    const uint32_t previous_count = get_samples_per_slot(radio_time.previous_slot, fp);
    const int next_slot = (radio_time.previous_slot + 1) % fp->slots_per_frame;
    const int next_frame = (radio_time.previous_frame + (next_slot == 0)) % 1024;
    if (offset != radio_time.previous_offset || radio_time.previous_timestamp > INT64_MAX - previous_count
        || timestamp != radio_time.previous_timestamp + previous_count || slot != next_slot || frame != next_frame)
      invalidate();
  }
  radio_time.previous_valid = true;
  radio_time.previous_timestamp = timestamp;
  radio_time.previous_offset = offset;
  radio_time.previous_frame = frame;
  radio_time.previous_slot = slot;

  if (slot != 0)
    return;

  uint64_t observed_ns;
  if ((offset > 0 && timestamp > INT64_MAX - offset) || (offset < 0 && timestamp < INT64_MIN - offset)
      || !monotonic_ns(&observed_ns)) {
    invalidate();
    return;
  }
  const uint64_t generation = atomic_load_explicit(&radio_time.generation, memory_order_acquire);
  const nr_ntn_radio_time_t observation = {.generation = generation,
                                           .observed_monotonic_ns = observed_ns,
                                           .rx_frame_ticks = timestamp + offset,
                                           .timestamp_rate_hz = (double)fp->samples_per_frame * 100.0,
                                           .sfn = frame};
  if (pthread_mutex_trylock(&radio_time.mutex) != 0)
    return;
  if (generation != 0 && atomic_load_explicit(&radio_time.ru, memory_order_acquire) == ru
      && atomic_load_explicit(&radio_time.generation, memory_order_acquire) == generation) {
    radio_time.observation = observation;
    radio_time.valid = true;
  }
  pthread_mutex_unlock(&radio_time.mutex);
}

static bool current(const RU_t *ru, uint64_t generation)
{
  return generation != 0 && atomic_load_explicit(&radio_time.ru, memory_order_acquire) == ru
         && atomic_load_explicit(&radio_time.generation, memory_order_acquire) == generation;
}

static bool fresh(uint64_t observed_ns, uint64_t now_ns)
{
  return now_ns >= observed_ns && now_ns - observed_ns <= NTN_RADIO_MAX_AGE_NS;
}

bool nr_ntn_radio_time_query(void *opaque, nr_ntn_radio_time_t *observation)
{
  RU_t *ru = atomic_load_explicit(&radio_time.ru, memory_order_acquire);
  if (observation == NULL || ru == NULL || (opaque != NULL && opaque != ru))
    return false;

  pthread_mutex_lock(&radio_time.mutex);
  const bool valid = radio_time.valid;
  nr_ntn_radio_time_t result = radio_time.observation;
  const uint64_t max_device_age_ticks = UINT64_C(20) * radio_time.fp.samples_per_frame;
  pthread_mutex_unlock(&radio_time.mutex);
  if (!valid || !current(ru, result.generation) || !monotonic_ns(&result.query_before_ns)
      || !fresh(result.observed_monotonic_ns, result.query_before_ns) || ru->rfdevice.trx_get_time_func == NULL)
    return false;

  openair0_time_t hardware;
  if (ru->rfdevice.trx_get_time_func(&ru->rfdevice, &hardware) != 0 || !monotonic_ns(&result.query_after_ns)
      || result.query_after_ns < result.query_before_ns || !fresh(result.observed_monotonic_ns, result.query_after_ns)
      || !current(ru, result.generation) || !isfinite(hardware.timestamp_rate_hz)
      || hardware.timestamp_rate_hz != result.timestamp_rate_hz || hardware.timestamp < result.rx_frame_ticks)
    return false;

  /* Unsigned subtraction is exact even if the signed timestamps straddle zero;
   * a signed difference could overflow. At the validated rate, 200 ms is exactly
   * twenty frame sample counts, with no floating-point threshold conversion. */
  const uint64_t device_age_ticks = (uint64_t)hardware.timestamp - (uint64_t)result.rx_frame_ticks;
  if (device_age_ticks > max_device_age_ticks)
    return false;
  result.now_ticks = hardware.timestamp;
  result.tx_advance_ticks = hardware.tx_advance_ticks;
  *observation = result;
  return true;
}

void nr_ntn_radio_time_disable(void)
{
  atomic_store_explicit(&radio_time.ru, NULL, memory_order_release);
  invalidate();
  pthread_mutex_lock(&radio_time.mutex);
  radio_time.valid = false;
  pthread_mutex_unlock(&radio_time.mutex);
}
