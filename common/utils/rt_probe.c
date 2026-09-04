/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "time_meas.h"
#include "LOG/log.h"
#include "rt_probe.h"

#include "common/config/config_userapi.h"

static inline oai_cputime_t rt_probe_ns_to_us(oai_cputime_t ns)
{
  return ns / (oai_cputime_t)1000;
}

static inline uint64_t rt_probe_ratio_ppm(uint64_t count, uint64_t total)
{
  if (total == 0)
    return 0;

  return (count * 1000000ULL + total / 2) / total;
}

void rt_probe_init(rt_probe_t *p, const char *name)
{
  memset(p, 0, sizeof(*p));
  p->name = name;
  p->initialized = 1;
  p->cfg = rt_probe_default_config();
}

static inline void rt_probe_reset_capture(rt_probe_t *p)
{
  if (p == NULL)
    return;

  if (p->capture_fd != NULL) {
    fclose(p->capture_fd);
    p->capture_fd = NULL;
  }

  if (p->capture_buffer != NULL) {
    free(p->capture_buffer);
    p->capture_buffer = NULL;
  }

  p->capture_count = 0;
  p->capture_capacity = 0;
  p->capture_last_dump_count = 0;
  p->capture_write_index = 0;
  p->capture_read_index = 0;
  p->capture_dropped_count = 0;
  p->capture_header_written = 0;
  p->capture_writer_busy = 0;
  p->capture_dumped = 0;
  p->capture_alloc_failed = 0;
}

static inline void rt_probe_setup_capture(rt_probe_t *p)
{
  if (p == NULL)
    return;

  if (!p->cfg.capture_enabled || p->cfg.capture_records == 0)
    return;

  if (p->capture_buffer != NULL)
    return;

  if (p->cfg.capture_records > (uint64_t)(SIZE_MAX / sizeof(*p->capture_buffer))) {
    p->capture_alloc_failed = 1;
    LOG_E(UTIL,
          "RT_DEADLINE_CAPTURE_ERROR probe=%s reason=too_many_records records=%lu\n",
          p->name,
          p->cfg.capture_records);
    return;
  }

  p->capture_buffer = calloc((size_t)p->cfg.capture_records, sizeof(*p->capture_buffer));
  if (p->capture_buffer == NULL) {
    p->capture_alloc_failed = 1;
    LOG_E(UTIL,
          "RT_DEADLINE_CAPTURE_ERROR probe=%s reason=alloc_failed records=%lu\n",
          p->name,
          p->cfg.capture_records);
    return;
  }

  p->capture_capacity = p->cfg.capture_records;
  p->capture_count = 0;
  p->capture_last_dump_count = 0;
  p->capture_write_index = 0;
  p->capture_read_index = 0;
  p->capture_dropped_count = 0;
  p->capture_fd = NULL;
  p->capture_header_written = 0;
  p->capture_writer_busy = 0;
  p->capture_dumped = 0;
  p->capture_alloc_failed = 0;

  LOG_D(UTIL,
        "RT_DEADLINE_CAPTURE_CONFIG probe=%s enabled=%d async_flush_enabled=%d final_dump_enabled=%d records=%lu path=%s\n",
        p->name,
        p->cfg.capture_enabled,
        p->cfg.capture_async_flush_enabled,
        p->cfg.capture_final_dump_enabled,
        p->cfg.capture_records,
        p->cfg.capture_path);
}

void rt_probe_set_config(rt_probe_t *p,
                         const rt_probe_config_t *cfg)
{
  if (p == NULL || cfg == NULL)
    return;

  rt_probe_reset_capture(p);
  p->cfg = *cfg;
  rt_probe_setup_capture(p);
}

void rt_probe_load_config(rt_probe_config_t *cfg, char *cfg_string)
{
  if (!cfg || !cfg_string)
    return;

  int stats_enabled = cfg->stats_enabled;
  int report_period = (int)cfg->report_period;
  int late_threshold_us = (int)cfg->late_threshold_us;
  int threshold0_us = (int)cfg->threshold_us[0];
  int threshold1_us = (int)cfg->threshold_us[1];
  int threshold2_us = (int)cfg->threshold_us[2];
  int threshold3_us = (int)cfg->threshold_us[3];
  int capture_enabled = cfg->capture_enabled;
  int capture_async_flush_enabled = cfg->capture_async_flush_enabled;
  int capture_final_dump_enabled = cfg->capture_final_dump_enabled;
  int capture_records = (int)cfg->capture_records;

  paramdef_t RTDeadlineL1TXParams[] = {
    {"stats_enabled", NULL, 0, .iptr = &stats_enabled, .defintval = stats_enabled, TYPE_INT, 0, NULL},
    {"report_period", NULL, 0, .iptr = &report_period, .defintval = report_period, TYPE_INT, 0, NULL},
    {"late_threshold_us", NULL, 0, .iptr = &late_threshold_us, .defintval = late_threshold_us, TYPE_INT, 0, NULL},
    {"threshold0_us", NULL, 0, .iptr = &threshold0_us, .defintval = threshold0_us, TYPE_INT, 0, NULL},
    {"threshold1_us", NULL, 0, .iptr = &threshold1_us, .defintval = threshold1_us, TYPE_INT, 0, NULL},
    {"threshold2_us", NULL, 0, .iptr = &threshold2_us, .defintval = threshold2_us, TYPE_INT, 0, NULL},
    {"threshold3_us", NULL, 0, .iptr = &threshold3_us, .defintval = threshold3_us, TYPE_INT, 0, NULL},
    {"capture_enabled", NULL, 0, .iptr = &capture_enabled, .defintval = capture_enabled, TYPE_INT, 0, NULL},
    {"capture_async_flush_enabled", NULL, 0, .iptr = &capture_async_flush_enabled, .defintval = capture_async_flush_enabled, TYPE_INT, 0, NULL},
    {"capture_final_dump_enabled", NULL, 0, .iptr = &capture_final_dump_enabled, .defintval = capture_final_dump_enabled, TYPE_INT, 0, NULL},
    {"capture_records", NULL, 0, .iptr = &capture_records, .defintval = capture_records, TYPE_INT, 0, NULL},
  };

  config_get(config_get_if(), RTDeadlineL1TXParams, sizeofArray(RTDeadlineL1TXParams), cfg_string);

  cfg->stats_enabled = stats_enabled;
  cfg->report_period = report_period > 0 ? (uint64_t)report_period : cfg->report_period;
  cfg->late_threshold_us = late_threshold_us > 0 ? (uint64_t)late_threshold_us : cfg->late_threshold_us;
  cfg->threshold_us[0] = threshold0_us > 0 ? (uint64_t)threshold0_us : cfg->threshold_us[0];
  cfg->threshold_us[1] = threshold1_us > 0 ? (uint64_t)threshold1_us : cfg->threshold_us[1];
  cfg->threshold_us[2] = threshold2_us > 0 ? (uint64_t)threshold2_us : cfg->threshold_us[2];
  cfg->threshold_us[3] = threshold3_us > 0 ? (uint64_t)threshold3_us : cfg->threshold_us[3];
  cfg->capture_enabled = capture_enabled;
  cfg->capture_async_flush_enabled = capture_async_flush_enabled;
  cfg->capture_final_dump_enabled = capture_final_dump_enabled;
  cfg->capture_records = capture_records > 0 ? (uint64_t)capture_records : cfg->capture_records;

  LOG_D(UTIL,
        "Loaded RT probe config %s: stats_enabled=%d report_period=%lu late_threshold_us=%llu "
        "threshold0_us=%llu threshold1_us=%llu threshold2_us=%llu threshold3_us=%llu "
        "capture_enabled=%d capture_async_flush_enabled=%d capture_final_dump_enabled=%d capture_records=%lu capture_path=%s\n",
        cfg_string,
        cfg->stats_enabled,
        cfg->report_period,
        cfg->late_threshold_us,
        cfg->threshold_us[0],
        cfg->threshold_us[1],
        cfg->threshold_us[2],
        cfg->threshold_us[3],
        cfg->capture_enabled,
        cfg->capture_async_flush_enabled,
        cfg->capture_final_dump_enabled,
        cfg->capture_records,
        cfg->capture_path);
}

static void rt_probe_flush_capture_csv(rt_probe_t *p, int final_dump)
{
  if (p == NULL || !p->initialized)
    return;

  if (!p->cfg.capture_enabled)
    return;

  if (p->capture_buffer == NULL || p->capture_capacity == 0)
    return;

  if (p->cfg.capture_path[0] == '\0')
    return;

  if (p->capture_dumped)
    return;

  if (__sync_lock_test_and_set(&p->capture_writer_busy, 1)) {
    if (!final_dump)
      return;

    while (__sync_lock_test_and_set(&p->capture_writer_busy, 1)) {
      const struct timespec wait_ts = {.tv_sec = 0, .tv_nsec = 1000000L};
      nanosleep(&wait_ts, NULL);
    }
  }

  const uint64_t read_index = __atomic_load_n(&p->capture_read_index, __ATOMIC_ACQUIRE);
  const uint64_t write_index = __atomic_load_n(&p->capture_write_index, __ATOMIC_ACQUIRE);
  uint64_t flushed = 0;

  if (write_index > read_index) {
    if (p->capture_fd == NULL) {
      p->capture_fd = fopen(p->cfg.capture_path, p->capture_header_written ? "a" : "w");
      if (p->capture_fd == NULL) {
        LOG_E(UTIL,
              "RT_DEADLINE_CAPTURE_ERROR probe=%s records=%lu path=%s reason=fopen\n",
              p->name,
              write_index - read_index,
              p->cfg.capture_path);
        __sync_lock_release(&p->capture_writer_busy);
        return;
      }

      if (!p->capture_header_written) {
        fprintf(p->capture_fd,
                "capture_index,probe_total,frame,slot,duration_us,late_threshold_us,late\n");
        p->capture_header_written = 1;
      }
    }

    for (uint64_t seq = read_index; seq < write_index; seq++) {
      const rt_probe_capture_record_t *record = &p->capture_buffer[seq % p->capture_capacity];

      fprintf(p->capture_fd,
              "%lu,%lu,%d,%d,%llu,%llu,%d\n",
              record->capture_index,
              record->probe_total,
              record->frame,
              record->slot,
              record->duration_us,
              record->late_threshold_us,
              record->late);
      flushed++;
    }

    if (fflush(p->capture_fd) != 0) {
      LOG_E(UTIL,
            "RT_DEADLINE_CAPTURE_ERROR probe=%s records=%lu path=%s reason=fflush\n",
            p->name,
            flushed,
            p->cfg.capture_path);
      __sync_lock_release(&p->capture_writer_busy);
      return;
    }

    __atomic_store_n(&p->capture_read_index, write_index, __ATOMIC_RELEASE);
    p->capture_last_dump_count = write_index;
  }

  if (final_dump && p->capture_fd != NULL) {
    if (fclose(p->capture_fd) != 0) {
      LOG_E(UTIL,
            "RT_DEADLINE_CAPTURE_ERROR probe=%s records=%lu path=%s reason=fclose\n",
            p->name,
            flushed,
            p->cfg.capture_path);
      p->capture_fd = NULL;
      __sync_lock_release(&p->capture_writer_busy);
      return;
    }
    p->capture_fd = NULL;
  }

  if (final_dump)
    p->capture_dumped = 1;

  if (flushed > 0 || final_dump) {
    LOG_D(UTIL,
          "%s probe=%s flushed=%lu produced=%lu dropped=%lu capacity=%lu final=%d path=%s\n",
          final_dump ? "RT_DEADLINE_CAPTURE_DUMP" : "RT_DEADLINE_CAPTURE_ASYNC_FLUSH",
          p->name,
          flushed,
          write_index,
          __atomic_load_n(&p->capture_dropped_count, __ATOMIC_RELAXED),
          p->capture_capacity,
          final_dump,
          p->cfg.capture_path);
  }

  __sync_lock_release(&p->capture_writer_busy);
}

void rt_probe_dump_capture(rt_probe_t *p)
{
  if (p == NULL || !p->initialized)
    return;

  if (!p->cfg.capture_enabled)
    return;

  /*
   * Final dumps are performed during controlled shutdown, not periodically
   */
  if (!p->cfg.capture_final_dump_enabled)
    return;

  rt_probe_flush_capture_csv(p, 1);
}

void rt_probe_async_flush_capture(rt_probe_t *p)
{
  if (p == NULL || !p->initialized)
    return;

  if (!p->cfg.capture_enabled)
    return;

  /*
   * Asynchonous flush are performed periodically
   */
  if (!p->cfg.capture_async_flush_enabled)
    return;


  rt_probe_flush_capture_csv(p, 0);
}

void rt_probe_capture_record(rt_probe_t *p,
                             int frame,
                             int slot,
                             time_stats_t *ts)
{
  if (p == NULL || !p->initialized)
    return;

  if (!p->cfg.capture_enabled)
    return;

  if (p->capture_buffer == NULL || p->capture_capacity == 0 || p->capture_dumped)
    return;

  const uint64_t read_index = __atomic_load_n(&p->capture_read_index, __ATOMIC_ACQUIRE);
  const uint64_t write_index = __atomic_load_n(&p->capture_write_index, __ATOMIC_RELAXED);

  if (write_index - read_index >= p->capture_capacity) {
    __atomic_add_fetch(&p->capture_dropped_count, 1, __ATOMIC_RELAXED);
    return;
  }

  oai_cputime_t duration_us = rt_probe_ns_to_us(ts->p_time);

  const uint64_t idx = write_index % p->capture_capacity;
  rt_probe_capture_record_t *record = &p->capture_buffer[idx];

  record->capture_index = write_index;
  record->probe_total = p->total;
  record->frame = frame;
  record->slot = slot;
  record->duration_us = duration_us;
  record->late_threshold_us = p->cfg.late_threshold_us;
  record->late = p->cfg.late_threshold_us > 0 && duration_us > p->cfg.late_threshold_us;

  __atomic_store_n(&p->capture_write_index, write_index + 1, __ATOMIC_RELEASE);
  __atomic_store_n(&p->capture_count, write_index + 1, __ATOMIC_RELAXED);
}

void rt_probe_record(rt_probe_t *p, time_stats_t *ts)
{
  if (!p || !p->initialized)
    return;

  if (!p->cfg.stats_enabled)
    return;

  oai_cputime_t duration_us = rt_probe_ns_to_us(ts->p_time);
  p->total++;
  p->sum_us += duration_us;

  if (duration_us > p->max_us)
    p->max_us = duration_us;

  if (p->cfg.late_threshold_us > 0 && duration_us > p->cfg.late_threshold_us)
    p->late_count++;

  // It is Assumed that thresholds are sorted increasingly
  int i = 0;
  for (; i < RT_DEADLINE_NUM_THRESHOLDS && duration_us > p->cfg.threshold_us[i]; i++)
    p->over_threshold[i]++;
  p->histogram[i]++;
}

void rt_probe_report(rt_probe_t *p, uint64_t report_period_records)
{
  if (!p || !p->initialized)
    return;

  if (!p->cfg.stats_enabled)
    return;

  if (p->cfg.report_period > 0)
    report_period_records = p->cfg.report_period;

  if (report_period_records == 0)
    return;

  if (p->total == 0)
    return;

  if ((p->total - p->last_report_total) < report_period_records)
    return;

  p->last_report_total = p->total;

  const oai_cputime_t avg_us = p->sum_us / p->total;
  const uint64_t late_ratio_ppm = rt_probe_ratio_ppm(p->late_count, p->total);
  const uint64_t over_threshold0_ratio_ppm = rt_probe_ratio_ppm(p->over_threshold[0], p->total);
  const uint64_t over_threshold1_ratio_ppm = rt_probe_ratio_ppm(p->over_threshold[1], p->total);
  const uint64_t over_threshold2_ratio_ppm = rt_probe_ratio_ppm(p->over_threshold[2], p->total);
  const uint64_t over_threshold3_ratio_ppm = rt_probe_ratio_ppm(p->over_threshold[3], p->total);

  printf("RT_DEADLINE_STATS probe=%s total=%lu avg_us=%llu max_us=%llu "
         "stats_enabled=%d report_period=%lu late_threshold_us=%llu "
         "late_count=%lu late_ratio_ppm=%lu "
         "threshold0_us=%llu over_threshold0=%lu over_threshold0_ratio_ppm=%lu "
         "threshold1_us=%llu over_threshold1=%lu over_threshold1_ratio_ppm=%lu "
         "threshold2_us=%llu over_threshold2=%lu over_threshold2_ratio_ppm=%lu "
         "threshold3_us=%llu over_threshold3=%lu over_threshold3_ratio_ppm=%lu "
         "hist_0_%llu=%lu hist_%llu_%llu=%lu  hist_%llu_%llu=%lu hist_%llu_%llu=%lu hist_over_%llu=%lu\n",
         p->name,
         p->total,
         avg_us,
         p->max_us,
         p->cfg.stats_enabled,
         p->cfg.report_period,
         p->cfg.late_threshold_us,
         p->late_count,
         late_ratio_ppm,
         p->cfg.threshold_us[0],
         p->over_threshold[0],
         over_threshold0_ratio_ppm,
         p->cfg.threshold_us[1],
         p->over_threshold[1],
         over_threshold1_ratio_ppm,
         p->cfg.threshold_us[2],
         p->over_threshold[2],
         over_threshold2_ratio_ppm,
         p->cfg.threshold_us[3],
         p->over_threshold[3],
         over_threshold3_ratio_ppm,
         p->cfg.threshold_us[0],
         p->histogram[0],
         p->cfg.threshold_us[0],
         p->cfg.threshold_us[1],
         p->histogram[1],
         p->cfg.threshold_us[1],
         p->cfg.threshold_us[2],
         p->histogram[2],
         p->cfg.threshold_us[2],
         p->cfg.threshold_us[3],
         p->histogram[3],
         p->cfg.threshold_us[3],
         p->histogram[4]);
  fflush(stdout);
}

