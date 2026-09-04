/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#ifndef RT_DEADLINE_PROBE_H
#define RT_DEADLINE_PROBE_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "time_meas.h"

#define RT_DEADLINE_NUM_THRESHOLDS 4
#define RT_DEADLINE_CAPTURE_PATH_MAX 256

/**
 * \typedef rt_probe_config_t
 * \brief real-time probe configuration
 * \var stats_enabled true is stats summary display in log is enabled
 * \var report_period period in records to dispaly stats summary
 * \var late_threshold_us threshold in microsecond
 * that determines is processing is late
 * \var threshold_us array of threshold in microsecond
 * for theshold overrun a histogram
 * \var capture_enabled true if records capture is enabled
 * \var capture_async_flush_enabled
 * true if capture should be flushed asynchronously
 * \var capture_final_dump_enabled
 * true if captured records not yet flushed
 * should be dumped at shutdown
 * \var capture_records capture records capacity
 * \var capture_path path of the capture file
 */
typedef struct {
  int stats_enabled;
  uint64_t report_period;
  oai_cputime_t late_threshold_us;
  // It is Assumed that thresholds are sorted increasingly
  oai_cputime_t threshold_us[RT_DEADLINE_NUM_THRESHOLDS];

  int capture_enabled;
  int capture_async_flush_enabled;
  int capture_final_dump_enabled;
  uint64_t capture_records;
  char capture_path[RT_DEADLINE_CAPTURE_PATH_MAX];
} rt_probe_config_t;

/**
 * \typedef rt_probe_capture_record_t
 * \brief captured real-time probe record
 * \var capture_index record index
 * \var probe_total total probed records
 * when record is captured
 * \var frame frame when record is captured
 * \var slot slot when the record is captured
 * \var duration_us recorded time
 * \var late_threshold_us
 * late completion threshold for this record
 * \var late true if the record
 * is a late processing
 */
typedef struct {
  uint64_t capture_index;
  uint64_t probe_total;
  int frame;
  int slot;
  oai_cputime_t duration_us;
  oai_cputime_t late_threshold_us;
  int late;
} rt_probe_capture_record_t;

/**
 * \brief returns a default config
 */
static inline rt_probe_config_t rt_probe_default_config(void)
{
  rt_probe_config_t cfg = {
      .stats_enabled = 0,
      .report_period = 20000,
      .late_threshold_us = 500,
      .threshold_us = {100, 200, 500, 1000},
      .capture_enabled = 0,
      .capture_async_flush_enabled = 1,
      .capture_final_dump_enabled = 1,
      .capture_records = 20000,
      .capture_path = "/tmp/rt_probe_records.csv",
  };
  return cfg;
}

/**
 * \typedef rt_probe_t
 * \brief real-time probe
 * \var name name of the probe
 * \var initialized indicates whether the probe
 * was initialized with a default config
 * \var cfg probe configuration
 * \var total total number of records
 * \var last_report_total total number of records
 * at the last probe display
 * \var sum_us sum of the recorded times in microseconds
 * \var max_us maximum recorded time in micro seconds
 * \var late_count number of records with late processing
 * \var over_threshold number of overrun for each thresholds
 * \var histogram histogram of times between the thresholds
 * \var capture_buffer buffer for captured records
 * \var capture_count number of captured records
 * \var capture_last_dump_count index of last flushed record
 * \var capture_write_index written record index in buffer 
 * \var capture_read_index read records index in buffer
 * \var capture_dropped_count counter of dropped record
 * \var capture_fd capture file descriptor
 * \var capture_header_written true if header was written
 * \var capture_writer_busy true if writer is busy
 * \var capture_dumped true if capture was dumped on shutdown
 * the capture file shall not be written anymore
 * \var capture_alloc_failed true if buffer allocation failed
 */
typedef struct {
  const char *name;
  int initialized;

  rt_probe_config_t cfg;

  uint64_t total;
  uint64_t last_report_total;
  oai_cputime_t sum_us;
  oai_cputime_t max_us;
  uint64_t late_count;

  uint64_t over_threshold[RT_DEADLINE_NUM_THRESHOLDS];
  uint64_t histogram[RT_DEADLINE_NUM_THRESHOLDS + 1];

  rt_probe_capture_record_t *capture_buffer;
  uint64_t capture_count;
  uint64_t capture_capacity;
  uint64_t capture_last_dump_count;
  uint64_t capture_write_index;
  uint64_t capture_read_index;
  uint64_t capture_dropped_count;
  FILE *capture_fd;
  int capture_header_written;
  int capture_writer_busy;
  int capture_dumped;
  int capture_alloc_failed;
} rt_probe_t;

/**
 * \brief initializes a probe with given name and default config
 * \param p pointer to the probe to initialize
 * \param name name to give to the probe
 */
void rt_probe_init(rt_probe_t *p, const char *name);

/**
 * \brief set probe configuration
 * \param p pointer to the probe to initialize
 * \param cfg configuration to set on the probe
 */
void rt_probe_set_config(rt_probe_t *p,
                         const rt_probe_config_t *cfg);

/**
 * \brief load a probe configuaration from configuration file
 * \param cfg pointer to the configuration struct to edit
 * \param cfg_string section of the configuration file to query
 */
void rt_probe_load_config(rt_probe_config_t *cfg, char *cfg_string);

/**
 * \brief dump captured records on shutdown
 * \param p pointer to the probe to dump
 */
void rt_probe_dump_capture(rt_probe_t *p);

/**
 * \brief flush captured records
 * \param p pointer to the probe to flush
 */
void rt_probe_async_flush_capture(rt_probe_t *p);

/**
 * \brief capture record with probe from timer
 * \param p pointer to the probe to capture
 * \param frame current frame to record
 * \param slot current slot to record
 * \param ts timer to record
 */
void rt_probe_capture_record(rt_probe_t *p,
                             int frame,
                             int slot,
                             time_stats_t *ts);

/**
 * \brief record last time measured by the given timer
 * \param p pointer to the recording probe
 * \param ts timer to record time from
 */
void rt_probe_record(rt_probe_t *p, time_stats_t *ts);

/**
 * \brief display probe stats summary to log
 * if the given period has elapsed
 * \param p pointer to the probe to dispaly
 * \param report_period_record number of records to elapse
 * between two display of the probe stats summary
 * 0 disables display
 * the report period in the probe config (if not 0)
 * overrides this argument
 */
void rt_probe_report(rt_probe_t *p, uint64_t report_period_records);

#endif /* RT_DEADLINE_PROBE_H */
