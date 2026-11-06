
#include "latency_hist.h"
#include <inttypes.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

static pthread_mutex_t latency_hist_mutex = PTHREAD_MUTEX_INITIALIZER;

/* one shared log file for every profiling point added via latency_hist_record() */
#define LATENCY_HIST_LOG_FILE "profiling_timing.log"
/* number of samples of a given point between two histogram dumps for that point */
#define LATENCY_HIST_LOG_INTERVAL 2000
#define LATENCY_HIST_MAX_POINTS 64

#define ARR_SIZE(a) (sizeof(a) / sizeof((a)[0]))

//static const uint32_t bounds_us[] = {200, 300, 500, 1000, 2000};
static const uint32_t bounds_us[] = {25, 50, 100, 200, 500, 1000, 2000};
#define LATENCY_HIST_NUM_BOUNDS ARR_SIZE(bounds_us)

typedef struct {
  const char *name;
  /* everything below is per-window: cleared after each dump */
  uint64_t counts[LATENCY_HIST_NUM_BOUNDS + 1];
  uint64_t iterations;
  double duration_us;   /* summed execution time of the window, in us */
  double max_us;        /* worst sample of the window */
  /* not cleared: how many windows this point has already dumped */
  uint64_t windows;
} latency_hist_point_t;

static latency_hist_point_t points[LATENCY_HIST_MAX_POINTS];
static int num_points = 0;
static FILE *shared_fp = NULL;
static bool shared_fp_failed = false;

static latency_hist_point_t *get_point(const char *name)
{
  for (int i = 0; i < num_points; i++) {
    /* callers pass string literals, so the fast path is almost always a pointer match */
    if (points[i].name == name || strcmp(points[i].name, name) == 0)
      return &points[i];
  }
  if (num_points >= LATENCY_HIST_MAX_POINTS)
    return NULL;
  latency_hist_point_t *p = &points[num_points++];
  p->name = name;
  return p;
}

/* clears the window accumulators so the next LATENCY_HIST_LOG_INTERVAL samples
 * are counted from zero; the caller must hold latency_hist_mutex */
static void reset_window(latency_hist_point_t *p)
{
  memset(p->counts, 0, sizeof(p->counts));
  p->iterations = 0;
  p->duration_us = 0.0;
  p->max_us = 0.0;
}

void latency_hist_record(const char *name, double duration_us)
{
  pthread_mutex_lock(&latency_hist_mutex);

  latency_hist_point_t *p = get_point(name);
  if (!p) {
    pthread_mutex_unlock(&latency_hist_mutex);
    return;
  }

  int bucket = LATENCY_HIST_NUM_BOUNDS;
  for (uint32_t i = 0; i < LATENCY_HIST_NUM_BOUNDS; i++) {
    if (duration_us < bounds_us[i]) {
      bucket = i;
      break;
    }
  }
  p->counts[bucket]++;
  p->iterations++;
  p->duration_us += duration_us;
  if (duration_us > p->max_us)
    p->max_us = duration_us;

  if (p->iterations < LATENCY_HIST_LOG_INTERVAL) {
    pthread_mutex_unlock(&latency_hist_mutex);
    return;
  }

  if (!shared_fp && !shared_fp_failed) {
    shared_fp = fopen(LATENCY_HIST_LOG_FILE, "w");
    if (!shared_fp)
      shared_fp_failed = true; /* do not retry the open on every sample */
  }
  if (!shared_fp) {
    /* still start a fresh window, else this point retries the dump on every call */
    reset_window(p);
    pthread_mutex_unlock(&latency_hist_mutex);
    return;
  }

  time_t now = time(NULL);
  struct tm tm_now;
  localtime_r(&now, &tm_now);
  char timestr[32];
  strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", &tm_now);

  p->windows++;
  fprintf(shared_fp,
          "[%s] %s window=%" PRIu64 " samples=%" PRIu64 " last_call=%.1fus avg=%.1fus max=%.1fus window_time=%.3fms",
          timestr,
          p->name,
          p->windows,
          p->iterations,
          duration_us,
          p->duration_us / (double)p->iterations,
          p->max_us,
          p->duration_us / 1000.0);
  uint32_t lower = 0;
  for (uint32_t i = 0; i <= LATENCY_HIST_NUM_BOUNDS; i++) {
    if (i < LATENCY_HIST_NUM_BOUNDS) {
      fprintf(shared_fp, " %u-%uus=%" PRIu64, lower, bounds_us[i], p->counts[i]);
      lower = bounds_us[i];
    } else {
      fprintf(shared_fp, " >%uus=%" PRIu64, lower, p->counts[i]);
    }
  }
  fprintf(shared_fp, "\n");
  fflush(shared_fp);

  reset_window(p);

  pthread_mutex_unlock(&latency_hist_mutex);
}
