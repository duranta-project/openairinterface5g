/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#include <stdint.h>
#include <stdio.h>
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

void rt_probe_set_config(rt_probe_t *p,
                         const rt_probe_config_t *cfg)
{
  if (p == NULL || cfg == NULL)
    return;

  p->cfg = *cfg;
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

  paramdef_t RTDeadlineL1TXParams[] = {
    {"stats_enabled", NULL, 0, .iptr = &stats_enabled, .defintval = stats_enabled, TYPE_INT, 0, NULL},
    {"report_period", NULL, 0, .iptr = &report_period, .defintval = report_period, TYPE_INT, 0, NULL},
    {"late_threshold_us", NULL, 0, .iptr = &late_threshold_us, .defintval = late_threshold_us, TYPE_INT, 0, NULL},
    {"threshold0_us", NULL, 0, .iptr = &threshold0_us, .defintval = threshold0_us, TYPE_INT, 0, NULL},
    {"threshold1_us", NULL, 0, .iptr = &threshold1_us, .defintval = threshold1_us, TYPE_INT, 0, NULL},
    {"threshold2_us", NULL, 0, .iptr = &threshold2_us, .defintval = threshold2_us, TYPE_INT, 0, NULL},
    {"threshold3_us", NULL, 0, .iptr = &threshold3_us, .defintval = threshold3_us, TYPE_INT, 0, NULL},
  };

  config_get(config_get_if(), RTDeadlineL1TXParams, sizeofArray(RTDeadlineL1TXParams), cfg_string);

  cfg->stats_enabled = stats_enabled;
  cfg->report_period = report_period > 0 ? (uint64_t)report_period : cfg->report_period;
  cfg->late_threshold_us = late_threshold_us > 0 ? (uint64_t)late_threshold_us : cfg->late_threshold_us;
  cfg->threshold_us[0] = threshold0_us > 0 ? (uint64_t)threshold0_us : cfg->threshold_us[0];
  cfg->threshold_us[1] = threshold1_us > 0 ? (uint64_t)threshold1_us : cfg->threshold_us[1];
  cfg->threshold_us[2] = threshold2_us > 0 ? (uint64_t)threshold2_us : cfg->threshold_us[2];
  cfg->threshold_us[3] = threshold3_us > 0 ? (uint64_t)threshold3_us : cfg->threshold_us[3];

  LOG_D(UTIL,
        "Loaded RT probe config %s: stats_enabled=%d report_period=%lu late_threshold_us=%llu "
        "threshold0_us=%llu threshold1_us=%llu threshold2_us=%llu threshold3_us=%llu ",
        cfg_string,
        cfg->stats_enabled,
        cfg->report_period,
        cfg->late_threshold_us,
        cfg->threshold_us[0],
        cfg->threshold_us[1],
        cfg->threshold_us[2],
        cfg->threshold_us[3]);
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

