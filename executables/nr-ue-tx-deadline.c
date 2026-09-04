/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr-ue-tx-deadline.h"

#include <errno.h>
#include <limits.h>

static int timespec_to_ns(const struct timespec *timestamp, uint64_t *nanoseconds)
{
  if (timestamp == NULL || timestamp->tv_sec < 0 || timestamp->tv_nsec < 0 || timestamp->tv_nsec >= 1000000000L)
    return EINVAL;

  const uint64_t seconds = timestamp->tv_sec;
  const uint64_t fractional_ns = timestamp->tv_nsec;
  if (seconds > (UINT64_MAX - fractional_ns) / UINT64_C(1000000000))
    return EOVERFLOW;

  *nanoseconds = seconds * UINT64_C(1000000000) + fractional_ns;
  return 0;
}

static bool sample_offset_to_ns(int64_t sample_offset, int64_t samples_per_subframe, int64_t *nanoseconds)
{
  if (samples_per_subframe <= 0)
    return false;

  /* Round to nearest nanosecond, with exact ties away from zero. */
  __int128 numerator = (__int128)sample_offset * INT64_C(1000000);
  const __int128 rounding = samples_per_subframe / 2;
  numerator += numerator >= 0 ? rounding : -rounding;
  const __int128 result = numerator / samples_per_subframe;
  if (result < INT64_MIN || result > INT64_MAX)
    return false;

  *nanoseconds = result;
  return true;
}

static bool add_signed_ns(uint64_t anchor_ns, int64_t offset_ns, uint64_t *result_ns)
{
  if (offset_ns >= 0) {
    const uint64_t offset = offset_ns;
    if (__builtin_add_overflow(anchor_ns, offset, result_ns))
      return false;
    return true;
  }

  const uint64_t magnitude = (uint64_t)(-(offset_ns + 1)) + 1;
  if (magnitude > anchor_ns)
    return false;
  *result_ns = anchor_ns - magnitude;
  return true;
}

nr_ue_tx_deadline_anchor_t nr_ue_tx_deadline_make_anchor(int64_t first_sample_timestamp,
                                                         int sample_count,
                                                         const struct timespec *monotonic_time,
                                                         int clock_error)
{
  nr_ue_tx_deadline_anchor_t anchor = {0};
  bool radio_valid = false;
  if (sample_count < 0) {
    anchor.error_code = EINVAL;
  } else if (__builtin_add_overflow(first_sample_timestamp, (int64_t)sample_count, &anchor.radio_timestamp)) {
    anchor.error_code = EOVERFLOW;
  } else {
    radio_valid = true;
  }

  int time_error = 0;
  if (monotonic_time == NULL)
    time_error = clock_error != 0 ? clock_error : EIO;
  else
    time_error = timespec_to_ns(monotonic_time, &anchor.monotonic_ns);

  if (anchor.error_code == 0)
    anchor.error_code = time_error;
  anchor.valid = radio_valid && time_error == 0;
  return anchor;
}

nr_ue_tx_deadline_t nr_ue_tx_deadline_compute(const nr_ue_tx_deadline_anchor_t *anchor,
                                              int64_t write_timestamp,
                                              int64_t guard_samples,
                                              int64_t samples_per_subframe)
{
  nr_ue_tx_deadline_t deadline = {0};
  if (anchor == NULL) {
    deadline.error_code = EINVAL;
    return deadline;
  }
  if (!anchor->valid) {
    deadline.error_code = anchor->error_code != 0 ? anchor->error_code : EINVAL;
    return deadline;
  }
  if (guard_samples < 0 || samples_per_subframe <= 0) {
    deadline.error_code = EINVAL;
    return deadline;
  }

  int64_t radio_deadline = 0;
  int64_t sample_offset = 0;
  int64_t offset_ns = 0;
  if (__builtin_sub_overflow(write_timestamp, guard_samples, &radio_deadline)
      || __builtin_sub_overflow(radio_deadline, anchor->radio_timestamp, &sample_offset)
      || !sample_offset_to_ns(sample_offset, samples_per_subframe, &offset_ns)
      || !add_signed_ns(anchor->monotonic_ns, offset_ns, &deadline.monotonic_ns)) {
    deadline.error_code = EOVERFLOW;
    return deadline;
  }

  deadline.valid = true;
  return deadline;
}

nr_ue_tx_deadline_check_t nr_ue_tx_deadline_check(const nr_ue_tx_deadline_t *deadline,
                                                  const struct timespec *monotonic_time,
                                                  int clock_error)
{
  nr_ue_tx_deadline_check_t check = {0};
  if (deadline == NULL) {
    check.error_code = EINVAL;
    return check;
  }
  if (!deadline->valid) {
    check.error_code = deadline->error_code != 0 ? deadline->error_code : EINVAL;
    return check;
  }
  if (monotonic_time == NULL) {
    check.error_code = clock_error != 0 ? clock_error : EIO;
    return check;
  }

  check.error_code = timespec_to_ns(monotonic_time, &check.monotonic_now_ns);
  if (check.error_code != 0)
    return check;

  if (check.monotonic_now_ns >= deadline->monotonic_ns) {
    const uint64_t lateness = check.monotonic_now_ns - deadline->monotonic_ns;
    if (lateness > INT64_MAX) {
      check.error_code = EOVERFLOW;
      return check;
    }
    check.lateness_ns = lateness;
  } else {
    const uint64_t headroom = deadline->monotonic_ns - check.monotonic_now_ns;
    const uint64_t int64_min_magnitude = (uint64_t)INT64_MAX + UINT64_C(1);
    if (headroom > int64_min_magnitude) {
      check.error_code = EOVERFLOW;
      return check;
    }
    check.lateness_ns = headroom == int64_min_magnitude ? INT64_MIN : -(int64_t)headroom;
  }

  check.valid = true;
  check.missed = check.lateness_ns > 0;
  return check;
}

bool nr_ue_tx_deadline_log_due(_Atomic(uint64_t) *counter)
{
  if (counter == NULL)
    return false;
  return atomic_fetch_add_explicit(counter, UINT64_C(1), memory_order_relaxed) % UINT64_C(1000) == 0;
}
