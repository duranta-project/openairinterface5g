/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#define _GNU_SOURCE
#include <stdio.h>
#include "time_meas.h"
#include <math.h>
#include <unistd.h>
#include <string.h>
#include "assertions.h"
#include <pthread.h>
#include "common/config/config_userapi.h"
#include "common/utils/LOG/log.h"
#include "common/utils/threadPool/notified_fifo.h"
// global var for openair performance profiler
int cpu_meas_enabled = 0;
double cpu_freq_GHz  __attribute__ ((aligned(32)));

double cpu_freq_GHz  __attribute__ ((aligned(32)))=0.0;
static uint32_t    max_cpumeasur;
static time_stats_t  **measur_table;
notifiedFIFO_t measur_fifo;
double get_cpu_freq_GHz(void)
{
  if (cpu_freq_GHz <0.01 ) {
    time_stats_t ts = {0};
    reset_meas(&ts);
    ts.trials++;
    ts.in = clock_gettime_oai();
    sleep(1);
    ts.diff = (clock_gettime_oai()-ts.in);
    cpu_freq_GHz = (double)ts.diff/1000000000;
  } 
  return cpu_freq_GHz;
}


double get_std_dev(time_stats_t *ptr)
{
  return sqrt((double)ptr->diff_square * 1e-6 / ptr->trials - pow((double)ptr->diff / ptr->trials / 1000, 2));
}


void print_meas_now(time_stats_t *ts, const char *name, FILE *file_name)
{
  UNUSED(name);
  if (cpu_meas_enabled) {
    if (ts->trials > 0) {
      fprintf(file_name, "%15.3f us, diff_now %15.3f \n", (ts->p_time / 1000.0), (double)ts->p_time);
    }
  }
}

void print_meas_header(time_stats_t *total_exec_time,
                       time_stats_t *sf_exec_time)
{
  if ((total_exec_time == NULL) || (sf_exec_time== NULL))
    fprintf(stderr,
            "%25s   %18s  %18s  %18s  %15s  %18s  %18s  %18s  %18s  %18s  %18s %9s %6f\n",
            "Name",
            "Total",
            "Max",
            "Std",
            "Num Trials",
            "min",
            "d1",
            "q1",
            "median",
            "q3",
            "d9",
            "CPU_F_GHz",
            cpu_freq_GHz);
  else
    fprintf(stderr,
            "%25s   %18s  %18s  %15s %9s %6f\n",
            "Name",
            "Total",
            "Average/Frame",
            "Trials",
            "CPU_F_GHz",
            cpu_freq_GHz);
}

void print_meas(time_stats_t *ts,
                const char *name,
                time_stats_t *total_exec_time,
                time_stats_t *sf_exec_time)
{
  if (ts->trials>0) {
    if ((total_exec_time == NULL) || (sf_exec_time == NULL)) {
      fprintf(stderr,
              "%25s:  %15.3f us; %15.3f us; %15.3f us; %15d; %15.3f us; %15.3f us; %15.3f us; %15.3f us; %15.3f us; %15.3f us;\n",
              name,
              ts->diff / ts->trials / 1000.0,
              ts->max / 1000.0,
              get_std_dev(ts),
              ts->trials,
              get_min(&ts->time_hist) / 1000.0,
              get_d1(&ts->time_hist) / 1000.0,
              get_q1(&ts->time_hist) / 1000.0,
              get_median(&ts->time_hist) / 1000.0,
              get_q3(&ts->time_hist) / 1000.0,
              get_d9(&ts->time_hist) / 1000.0);
    } else {
      fprintf(stderr,
              "%30s:  %15.3f ms (%5.2f%%); %15.3f us (%5.2f%%); %15d;\n",
              name,
              ts->diff / 1000000.0,
              ((ts->diff / 1000000.0) / (total_exec_time->diff / 1000000.0)) * 100,  // percentage
              (ts->diff / ts->trials / 1000.0),
              ((ts->diff / ts->trials / 1000.0) / (sf_exec_time->diff / sf_exec_time->trials / 1000.0)) * 100,  // percentage
              ts->trials);
    }
  }
}

size_t print_meas_log_header(time_stats_t *total_exec_time,
                             time_stats_t *sf_exec_time,
                             char *output,
                             size_t outputlen,
                             int cpu_meas_enabled)
{
  const char *begin = output;
  const char *end = output + outputlen;

  if ((total_exec_time == NULL) || (sf_exec_time== NULL))
    output += snprintf(output,
                       end - output,
                       "%25s   %18s  %18s  %18s  %15s  %18s  %18s  %18s  %18s  %18s  %18s %9s %6f\n",
                       "Name",
                       "Total",
                       "Max",
                       "Std",
                       "Num Trials",
                       "min",
                       "d1",
                       "q1",
                       "median",
                       "q3",
                       "d9",
                       "CPU_F_GHz",
                       cpu_freq_GHz);
  else
    output += snprintf(output,
                       end - output,
                       "%25s   %18s  %18s  %15s %9s %6f\n",
                       "Name",
                       "Total",
                       "Average/Frame",
                       "Trials",
                       "CPU_F_GHz",
                       cpu_freq_GHz);

  return output - begin;
}

size_t print_meas_log(time_stats_t *ts,
                      const char *name,
                      time_stats_t *total_exec_time,
                      time_stats_t *sf_exec_time,
                      char *output,
                      size_t outputlen)
{
  const char *begin = output;
  const char *end = output + outputlen;

  if (ts->trials > 0) {
    if ((total_exec_time == NULL) || (sf_exec_time == NULL)) {
      output += snprintf(output,
                         end - output,
                         "%25s:  %15.3f us; %15.3f us; %15.3f us; %15d; %15.3f us; %15.3f us; %15.3f us; %15.3f us; %15.3f us; %15.3f us;\n",
                         name,
                         ts->diff / ts->trials / 1000.0,
                         ts->max / 1000.0,
                         get_std_dev(ts),
                         ts->trials,
                         get_min(&ts->time_hist) / 1000.0,
                         get_d1(&ts->time_hist) / 1000.0,
                         get_q1(&ts->time_hist) / 1000.0,
                         get_median(&ts->time_hist) / 1000.0,
                         get_q3(&ts->time_hist) / 1000.0,
                         get_d9(&ts->time_hist) / 1000.0);
    } else {
      output += snprintf(output,
                         end - output,
                         "%25s:  %15.3f ms (%5.2f%%); %15.3f us (%5.2f%%); %15d;\n",
                         name,
                         ts->diff / 1000000.0,
                         ((ts->diff / 1000000.0) / (total_exec_time->diff / 1000000.0)) * 100,  // percentage
                         (ts->diff / ts->trials / 1000.0),
                         ((ts->diff / ts->trials / 1000.0) / (sf_exec_time->diff / sf_exec_time->trials / 1000.0)) * 100,  // percentage
                         ts->trials);
    }
  }
  return output - begin;
}

double get_time_meas_us(time_stats_t *ts)
{
  static double cpu_freq_GHz = 0.0;

  if (cpu_freq_GHz == 0.0)
    cpu_freq_GHz = get_cpu_freq_GHz();

  if (ts->trials > 0)
    return  (ts->diff / ts->trials / 1000.0);

  return 0;
}

/* function for the asynchronous measurment module: cpu stat are sent to a dedicated thread
 * which is in charge of computing the cpu time spent in a given function/algorithm...
 */

time_stats_t *register_meas(char *name)
{
  for (int i=0; i<max_cpumeasur; i++) {
    if (measur_table[i] == NULL) {
      measur_table[i] = (time_stats_t *)malloc(sizeof(time_stats_t));
      memset(measur_table[i] ,0,sizeof(time_stats_t));
      measur_table[i]->meas_name = strdup(name);
      measur_table[i]->meas_index = i;
      measur_table[i]->tpoolmsg =newNotifiedFIFO_elt(sizeof(time_stats_msg_t),0,NULL,NULL);
      measur_table[i]->tstatptr = (time_stats_msg_t *)NotifiedFifoData(measur_table[i]->tpoolmsg);
      return measur_table[i];
    }
  }
  return NULL;
}

void free_measurtbl(void) {
  for (int i=0; i<max_cpumeasur; i++) {
    if (measur_table[i] != NULL) {
	  free(measur_table[i]->meas_name);
	  delNotifiedFIFO_elt(measur_table[i]->tpoolmsg);
	  free(measur_table[i]);
    }
  }
  //free the fifo...
}

void run_cpumeasur(void) {
    struct sched_param schedp;
    pthread_setname_np(pthread_self(), "measur");
    schedp.sched_priority=0;
    int rt=pthread_setschedparam(pthread_self(), SCHED_IDLE, &schedp);
    AssertFatal(rt==0, "couldn't set measur thread priority: %s\n",strerror(errno));
    initNotifiedFIFO(&measur_fifo);
    while(1) {
      notifiedFIFO_elt_t *msg = pullNotifiedFIFO(&measur_fifo);
      time_stats_msg_t *tsm = (time_stats_msg_t *)NotifiedFifoData(msg);
        switch(tsm->msgid) {
          case TIMESTAT_MSGID_START:
             measur_table[tsm->timestat_id]->in=tsm->ts;
             (measur_table[tsm->timestat_id]->trials)++;
          break;
          case TIMESTAT_MSGID_STOP:
    /// process duration is the difference between two clock points
             measur_table[tsm->timestat_id]->p_time = (tsm->ts - measur_table[tsm->timestat_id]->in);
             measur_table[tsm->timestat_id]->diff += measur_table[tsm->timestat_id]->p_time;
             if ( measur_table[tsm->timestat_id]->p_time > measur_table[tsm->timestat_id]->max )
               measur_table[tsm->timestat_id]->max = measur_table[tsm->timestat_id]->p_time;
          break;
          case TIMESTAT_MSGID_DISPLAY:
            {
            char aline[256];
            int start, stop;
             if (tsm->displayFunc != NULL) {
               if(tsm->timestat_id >= 0) {
                 start=tsm->timestat_id ;
                 stop=start+1;
               }
               else {
                  start=0;
                  stop=max_cpumeasur ;
               }
               for (int i=start ; i<stop ; i++) {
                 if (measur_table[i] != NULL) {
                   sprintf(aline,"%s: %15.3f us ",measur_table[i]->meas_name, measur_table[i]->trials==0?0:(  (measur_table[i]->trials/measur_table[i]->diff )/ cpu_freq_GHz /1000 ));
                   tsm->displayFunc(aline);
                   }
                }
             }
            }
          break;
          case TIMESTAT_MSGID_END:
            free_measurtbl();
            delNotifiedFIFO_elt(msg);
            pthread_exit(NULL);
          break;
          default:
          break;
      }
    delNotifiedFIFO_elt(msg);
    }
}


void init_meas(void) {
  pthread_t thid;
  paramdef_t cpumeasur_params[] = CPUMEASUR_PARAMS_DESC;
  int numparams = sizeofArray(cpumeasur_params);
  int rt = config_get(config_get_if(), cpumeasur_params, numparams, CPUMEASUR_SECTION);
  AssertFatal(rt >= 0, "cpumeasur configuration couldn't be performed");
  measur_table=calloc(max_cpumeasur,sizeof( time_stats_t *));
  AssertFatal(measur_table!=NULL, "couldn't allocate %u cpu measurements entries\n",max_cpumeasur);
  rt=pthread_create(&thid,NULL, (void *(*)(void *))run_cpumeasur, NULL);
  AssertFatal(rt==0, "couldn't create cpu measurment thread: %s\n",strerror(errno));
}

void send_meas(time_stats_t *ts, int msgid) {
    if (MEASURE_ENABLED(ts) ) {
      ts->tstatptr->timestat_id=ts->meas_index;
      ts->tstatptr->msgid = msgid ;
      ts->tstatptr->ts = clock_gettime_oai();
      pushNotifiedFIFO(&measur_fifo, ts->tpoolmsg);
    }
  }

void end_meas(void) {
    notifiedFIFO_elt_t *nfe = newNotifiedFIFO_elt(sizeof(time_stats_msg_t),0,NULL,NULL);
	time_stats_msg_t *msg = (time_stats_msg_t *)NotifiedFifoData(nfe);
    msg->msgid = TIMESTAT_MSGID_END ;
    pushNotifiedFIFO(&measur_fifo, nfe);
}

/**
 * \brief resets histogram (clears all counts)
 * \param hist histogram to be reset
 */
void reset_time_hist(time_hist_t *hist)
{
  if (hist == NULL)
    return;

  memset(hist->counts, 0, sizeof(hist->counts));
  hist->out_of_range = 0;
}

/**
 * \brief inserts value into histogram
 * if value is out of range, it is dropped
 * \param hist histogram to insert in
 * \param time time value to insert in nanoseconds
 */
void insert_in_time_hist(time_hist_t *hist, oai_cputime_t time)
{
  if (hist == NULL)
    return;

  // Check if time is within histogram range [0, TIME_MEAS_HISTOGRAM_SPAN_NS)
  if (time < 0) {
    hist->out_of_range++;
    LOG_W(UTIL, "time stats histogram: out of range sample: %llu ns < 0 ns (%lu records out of range since reset)\n", time, hist->out_of_range);
    return; // Drop out-of-range values
  }
  if (time >= TIME_MEAS_HISTOGRAM_SPAN_NS) {
    hist->out_of_range++;
    LOG_W(UTIL, "time stats histogram: out of range sample: %llu ns >= %u ns (%lu records out of range since reset)\n", time, TIME_MEAS_HISTOGRAM_SPAN_NS, hist->out_of_range);
    return; // Drop out-of-range values
  }

  // Calculate bin index
  uint64_t bin_index = time / TIME_MEAS_HISTOGRAM_BIN_WIDTH_NS;
  
  // Ensure bin index is within bounds (should be due to range check above)
  AssertFatal(bin_index < TIME_MEAS_HISTOGRAM_NUM_BINS, "[UTIL] time stats histogram: index out of range: %lu >= %u\n", bin_index, TIME_MEAS_HISTOGRAM_NUM_BINS);

  hist->counts[bin_index]++;
}

/**
 * \brief copy histogram src into dst
 * \param dst destination histogram
 * \param src source histogram
 */
void copy_time_hist(time_hist_t *dst, const time_hist_t *src)
{
  if (dst == NULL || src == NULL)
    return;

  *dst = *src;
}

/**
 * \brief merges histogram src into dst
 * \param dst destination histogram
 * \param src source histogram
 */
void merge_time_hist(time_hist_t *dst, const time_hist_t *src)
{
  if (dst == NULL || src == NULL)
    return;

  for (uint64_t i = 0; i < TIME_MEAS_HISTOGRAM_NUM_BINS; i++)
    dst->counts[i] += src->counts[i];
  dst->out_of_range += src->out_of_range;
}

/**
 * \brief helper function to compute cumulative count up to a given bin
 */
static uint64_t get_cumulative_count(const time_hist_t *hist, uint64_t bin_index)
{
  uint64_t total = 0;
  for (uint64_t i = 0; i <= bin_index && i < TIME_MEAS_HISTOGRAM_NUM_BINS; i++)
    total += hist->counts[i];
  return total;
}

/**
 * \brief helper function to compute total count in histogram
 */
static uint64_t get_total_count(const time_hist_t *hist)
{
  return get_cumulative_count(hist, TIME_MEAS_HISTOGRAM_NUM_BINS - 1);
}

/**
 * \brief get the minimum from a histogram
 * returns 0 if no entries, otherwise the lower bound of the first non-empty bin
 * \param hist histogram to query
 */
oai_cputime_t get_min(time_hist_t *hist)
{
  if (hist == NULL)
    return 0;

  for (uint64_t i = 0; i < TIME_MEAS_HISTOGRAM_NUM_BINS; i++)
    if (hist->counts[i] > 0)
      return i * TIME_MEAS_HISTOGRAM_BIN_WIDTH_NS;

  return 0;
}

/**
 * \brief get a percentile value from histogram
 * \param hist histogram to query
 * \param percentile percentile to get (0.0 to 1.0)
 * \return approximate time value for the percentile
 */
static oai_cputime_t get_percentile_histogram(time_hist_t *hist, double percentile)
{
  if (hist == NULL)
    return 0;

  uint64_t total_count = get_total_count(hist);
  if (total_count == 0)
    return 0;

  // Calculate target count for the percentile
  uint64_t target_count = (uint64_t)(percentile * total_count);
  
  // Find the bin where cumulative count reaches or exceeds target
  uint64_t cumulative = 0;
  for (uint64_t i = 0; i < TIME_MEAS_HISTOGRAM_NUM_BINS; i++) {
    cumulative += hist->counts[i];
    if (cumulative > target_count)
      return i * TIME_MEAS_HISTOGRAM_BIN_WIDTH_NS;
  }

  // If we get here, return the upper bound
  return (TIME_MEAS_HISTOGRAM_NUM_BINS - 1) * TIME_MEAS_HISTOGRAM_BIN_WIDTH_NS;
}

/**
 * \brief get the median from a histogram
 * returns 0 if no entries, otherwise an approximation from the histogram
 * \param hist histogram to query
 */
oai_cputime_t get_median(time_hist_t *hist)
{
  return get_percentile_histogram(hist, 0.5);
}

/**
 * \brief get the first quartile from a histogram
 * returns 0 if no entries, otherwise an approximation from the histogram
 * \param hist histogram to query
 */
oai_cputime_t get_q1(time_hist_t *hist)
{
  return get_percentile_histogram(hist, 0.25);
}

/**
 * \brief get the third quartile from a histogram
 * returns 0 if no entries, otherwise an approximation from the histogram
 * \param hist histogram to query
 */
oai_cputime_t get_q3(time_hist_t *hist)
{
  return get_percentile_histogram(hist, 0.75);
}

/**
 * \brief get the first decile from a histogram
 * returns 0 if no entries, otherwise an approximation from the histogram
 * \param hist histogram to query
 */
oai_cputime_t get_d1(time_hist_t *hist)
{
  return get_percentile_histogram(hist, 0.1);
}

/**
 * \brief get the nineth decile from a histogram
 * returns 0 if no entries, otherwise an approximation from the histogram
 * \param hist histogram to query
 */
oai_cputime_t get_d9(time_hist_t *hist)
{
  return get_percentile_histogram(hist, 0.9);
}
