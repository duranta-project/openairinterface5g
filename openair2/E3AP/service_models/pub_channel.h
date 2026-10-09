/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * pub_channel_t — lets one producer thread hand its "latest snapshot" to many
 * consumer threads. The producer publishes a new value and wakes everyone
 * waiting; each consumer sleeps until something newer than what it last saw
 * arrives. The actual data lives in the caller's own storage (copied while the
 * lock is held), so this one primitive works for a payload of any type.
 *
 * Both current users (the MAC sensing-range publish and the KPM IQ publish)
 * run their consumer in the blocking on-data mode by default: the worker
 * sleeps in pub_channel_wait and is woken exactly on publish. The timedwait
 * path serves the optional periodic (throttled) mode.
 */
#ifndef OPENAIR2_E3AP_SERVICE_MODELS_PUB_CHANNEL_H
#define OPENAIR2_E3AP_SERVICE_MODELS_PUB_CHANNEL_H

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

typedef struct {
  pthread_mutex_t mutex;
  pthread_cond_t condition_variable;
  bool condition_variable_initialized;
  uint64_t publish_sequence; /* counts up once per publish and per shutdown */
} pub_channel_t;

#define PUB_CHANNEL_INIT               \
  {                                    \
    .mutex = PTHREAD_MUTEX_INITIALIZER \
  }

/* Current CLOCK_MONOTONIC time as a plain nanosecond count. */
static inline uint64_t pub_channel_now_ns(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/* Set up the condition variable on first use so its timed waits use a clock
 * that never jumps. Must be called with the channel's mutex held. */
static inline void pub_channel_ensure_condition_variable_initialized(pub_channel_t *channel)
{
  if (!channel->condition_variable_initialized) {
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    pthread_cond_init(&channel->condition_variable, &attr);
    pthread_condattr_destroy(&attr);
    channel->condition_variable_initialized = true;
  }
}

/* Producer side. Write the new snapshot between lock and publish_and_wake:
 *     pub_channel_lock(channel);
 *     <copy the new snapshot into the caller's storage; any extra under-lock work>
 *     pub_channel_publish_and_wake(channel);
 *     pub_channel_unlock(channel); */
static inline void pub_channel_lock(pub_channel_t *channel)
{
  pthread_mutex_lock(&channel->mutex);
  pub_channel_ensure_condition_variable_initialized(channel);
}
static inline void pub_channel_publish_and_wake(pub_channel_t *channel)
{
  channel->publish_sequence++;
  pthread_cond_broadcast(&channel->condition_variable);
}
static inline void pub_channel_unlock(pub_channel_t *channel)
{
  pthread_mutex_unlock(&channel->mutex);
}

/* Consumer side. Sleep until a newer snapshot than the caller's
 * last_seen_sequence is published, then copy snapshot_size bytes out (when
 * out_snapshot is given) and update the caller's sequence. timeout_ns: 0 =
 * don't wait, UINT64_MAX = wait forever, else a CLOCK_MONOTONIC deadline.
 * Returns true if a publish (or shutdown) happened since the caller last saw. */
static inline bool pub_channel_wait(pub_channel_t *channel,
                                    const void *latest_snapshot,
                                    void *out_snapshot,
                                    size_t snapshot_size,
                                    uint64_t timeout_ns,
                                    uint64_t *caller_sequence)
{
  if (!caller_sequence)
    return false;
  const uint64_t last_seen_sequence = *caller_sequence;

  pub_channel_lock(channel);

  if (timeout_ns == 0) {
    /* Polling mode: no wait. */
  } else if (timeout_ns == UINT64_MAX) {
    while (channel->publish_sequence == last_seen_sequence)
      pthread_cond_wait(&channel->condition_variable, &channel->mutex);
  } else {
    struct timespec deadline;
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    uint64_t ns = (uint64_t)deadline.tv_nsec + timeout_ns;
    deadline.tv_sec += (time_t)(ns / 1000000000ull);
    deadline.tv_nsec = (long)(ns % 1000000000ull);
    while (channel->publish_sequence == last_seen_sequence) {
      if (pthread_cond_timedwait(&channel->condition_variable, &channel->mutex, &deadline) == ETIMEDOUT)
        break;
    }
  }

  const uint64_t current_sequence = channel->publish_sequence;
  if (out_snapshot && latest_snapshot)
    memcpy(out_snapshot, latest_snapshot, snapshot_size);

  pub_channel_unlock(channel);
  *caller_sequence = current_sequence;
  return current_sequence != last_seen_sequence;
}

/* Wake every waiter (e.g. on shutdown) by advancing the sequence and
 * broadcasting. A waiter returns once; its caller must check its own running
 * flag to decide whether to exit. */
static inline void pub_channel_signal_shutdown(pub_channel_t *channel)
{
  pub_channel_lock(channel);
  pub_channel_publish_and_wake(channel);
  pub_channel_unlock(channel);
}

#endif /* OPENAIR2_E3AP_SERVICE_MODELS_PUB_CHANNEL_H */
