
#ifndef __LATENCY_HIST_H__
#define __LATENCY_HIST_H__

#include <time.h>

/* Generic call-duration profiling: at any call site, bracket the call with
 * LATENCY_HIST_START()/latency_hist_record() -- no per-point declaration or
 * init call needed, just start and end. Every named point is bucketed
 * separately, but all of them log into the single shared
 * LATENCY_HIST_LOG_FILE (common/utils/latency_hist.c), one line per point
 * every LATENCY_HIST_LOG_INTERVAL samples of that point, so the whole run's
 * profiling data can be tailed from one place. Each line covers only the
 * LATENCY_HIST_LOG_INTERVAL samples since the previous line of that point:
 * the counters are cleared after every dump, so the buckets, the summed time,
 * the average and the max are per-window, not cumulative.
 *
 * It is safe to call latency_hist_record() from any thread and for any number
 * of points; every point keeps its own counters and dumps on its own schedule.
 *
 * Usage:
 *   struct timespec ts;
 *   LATENCY_HIST_START(ts);
 *   my_function(...);
 *   latency_hist_record("my_function", latency_hist_elapsed_us(&ts));
 */

#define LATENCY_HIST_START(ts) clock_gettime(CLOCK_MONOTONIC, &(ts))

static inline double latency_hist_elapsed_us(const struct timespec *start)
{
  struct timespec end;
  clock_gettime(CLOCK_MONOTONIC, &end);
  return (end.tv_sec - start->tv_sec) * 1e6 + (end.tv_nsec - start->tv_nsec) / 1e3;
}

/* records one sample for the named profiling point and, every
 * LATENCY_HIST_LOG_INTERVAL samples of that point, appends the histogram of
 * that window as one line to the shared LATENCY_HIST_LOG_FILE and clears it.
 * 'name' must remain valid for the lifetime of the process (e.g. a string literal). */
void latency_hist_record(const char *name, double duration_us);

#endif /* __LATENCY_HIST_H__ */

