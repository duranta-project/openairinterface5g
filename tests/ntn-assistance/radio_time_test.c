/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */
/* Exercise radio-time observations with a simulated backend and clock. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Include the production helper to exercise its private trylock/generation
 * state deterministically; frame/slot arithmetic is linked from nr_parms.c. */
#include "../../executables/ntn_radio_time.c"

RAN_CONTEXT_t RC;
static RU_t ru;
static PHY_VARS_gNB gnb;
static NR_DL_FRAME_PARMS fp;
static RU_t *rus[] = {&ru};
static PHY_VARS_gNB *gnbs[] = {&gnb};
static uint64_t fake_ns;
static bool clock_failure;
static struct {
  openair0_time_t time;
  int status;
  unsigned int calls;
  bool partial_during_query;
  bool disable_during_query;
  bool clock_fail_after_query;
  uint64_t query_elapsed_ns;
} hardware;

int __real_clock_gettime(clockid_t clock, struct timespec *time);
int __wrap_clock_gettime(clockid_t clock, struct timespec *time)
{
  if (clock != CLOCK_MONOTONIC)
    return __real_clock_gettime(clock, time);
  if (clock_failure)
    return -1;
  time->tv_sec = fake_ns / UINT64_C(1000000000);
  time->tv_nsec = fake_ns % UINT64_C(1000000000);
  return 0;
}

static int fake_get_time(openair0_device_t *device, openair0_time_t *output)
{
  assert(device == &ru.rfdevice);
  /* The slow backend read must occur outside the snapshot mutex. */
  assert(pthread_mutex_trylock(&radio_time.mutex) == 0);
  pthread_mutex_unlock(&radio_time.mutex);
  ++hardware.calls;
  *output = hardware.time;
  if (hardware.partial_during_query)
    nr_ntn_radio_time_rx(&ru, 0, 0, false);
  if (hardware.disable_during_query)
    nr_ntn_radio_time_disable();
  fake_ns += hardware.query_elapsed_ns;
  if (hardware.clock_fail_after_query)
    clock_failure = true;
  return hardware.status;
}

static void reset(unsigned int mu, uint32_t samples_per_subframe)
{
  nr_ntn_radio_time_disable();
  memset(&ru, 0, sizeof(ru));
  memset(&gnb, 0, sizeof(gnb));
  memset(&fp, 0, sizeof(fp));
  memset(&hardware, 0, sizeof(hardware));
  RC.nb_RU = 1;
  RC.ru = rus;
  RC.nb_nr_L1_inst = 1;
  RC.gNB = gnbs;
  fp.numerology_index = mu;
  fp.slots_per_subframe = 1U << mu;
  fp.slots_per_frame = 10U * fp.slots_per_subframe;
  fp.samples_per_subframe = samples_per_subframe;
  fp.samples_per_frame = 10U * samples_per_subframe;
  if (mu == 0) {
    fp.samples_per_slot0 = samples_per_subframe;
    fp.samples_per_slotN0 = samples_per_subframe;
  } else {
    fp.samples_per_slotN0 = samples_per_subframe / fp.slots_per_subframe - 8;
    fp.samples_per_slot0 = samples_per_subframe / fp.slots_per_subframe + 8U * (fp.slots_per_subframe / 2 - 1);
  }
  ru.if_south = LOCAL_RF;
  ru.nr_frame_parms = &fp;
  ru.num_gNB = 1;
  ru.gNB_list[0] = &gnb;
  gnb.num_RU = 1;
  gnb.RU_list[0] = &ru;
  ru.openair0_cfg.sample_rate = (double)fp.samples_per_frame * 100.0;
  ru.rfdevice.openair0_cfg = &ru.openair0_cfg;
  ru.rfdevice.trx_get_time_func = fake_get_time;
  hardware.time.timestamp_rate_hz = ru.openair0_cfg.sample_rate;
  hardware.time.tx_advance_ticks = INT64_C(4294967294);
  fake_ns = UINT64_C(1000000000);
  clock_failure = false;
}

static void rx(uint64_t extended_frame, unsigned int slot)
{
  ru.proc.timestamp_rx = extended_frame * fp.samples_per_frame + get_samples_slot_timestamp(&fp, slot);
  const __int128 now = (__int128)ru.proc.timestamp_rx + ru.ts_offset + get_samples_per_slot(slot, &fp);
  hardware.time.timestamp = now > INT64_MAX ? INT64_MAX : now < INT64_MIN ? INT64_MIN : (int64_t)now;
  nr_ntn_radio_time_rx(&ru, extended_frame % 1024, slot, true);
}

static void unchanged_failure(void *opaque)
{
  nr_ntn_radio_time_t output;
  memset(&output, 0xa5, sizeof(output));
  unsigned char before[sizeof(output)];
  memcpy(before, &output, sizeof(output));
  assert(!nr_ntn_radio_time_query(opaque, &output));
  assert(memcmp(before, &output, sizeof(output)) == 0);
}

static nr_ntn_radio_time_t query(void)
{
  nr_ntn_radio_time_t output;
  assert(nr_ntn_radio_time_query(NULL, &output));
  assert(output.now_ticks == hardware.time.timestamp);
  assert(output.tx_advance_ticks == hardware.time.tx_advance_ticks);
  assert(output.timestamp_rate_hz == ru.openair0_cfg.sample_rate);
  assert(output.query_after_ns >= output.query_before_ns);
  return output;
}

static void test_startup(void)
{
  reset(1, 7680);
  assert(!nr_ntn_radio_time_init(NULL));
  RC.nb_RU = 2;
  assert(!nr_ntn_radio_time_init(&ru));
  RC.nb_RU = 1;
  RC.nb_nr_L1_inst = 2;
  assert(!nr_ntn_radio_time_init(&ru));
  RC.nb_nr_L1_inst = 1;
  ru.if_south = REMOTE_IF5;
  assert(!nr_ntn_radio_time_init(&ru));
  ru.if_south = LOCAL_RF;
  gnb.CC_id = 1;
  assert(!nr_ntn_radio_time_init(&ru));
  gnb.CC_id = 0;
  gnb.Mod_id = 1;
  assert(!nr_ntn_radio_time_init(&ru));
  gnb.Mod_id = 0;
  gnb.num_RU = 2;
  assert(!nr_ntn_radio_time_init(&ru));
  gnb.num_RU = 1;
  ru.rfdevice.trx_get_time_func = NULL;
  assert(!nr_ntn_radio_time_init(&ru));
  ru.rfdevice.trx_get_time_func = fake_get_time;
  ru.openair0_cfg.sample_rate = NAN;
  assert(!nr_ntn_radio_time_init(&ru));
  ru.openair0_cfg.sample_rate = (double)fp.samples_per_frame * 100.0 + 1;
  assert(!nr_ntn_radio_time_init(&ru));
  ru.openair0_cfg.sample_rate -= 1;
  ++fp.samples_per_frame;
  assert(!nr_ntn_radio_time_init(&ru));
  --fp.samples_per_frame;
  ++fp.samples_per_slot0;
  assert(!nr_ntn_radio_time_init(&ru));
  --fp.samples_per_slot0;
  ++fp.slots_per_frame;
  assert(!nr_ntn_radio_time_init(&ru));
  --fp.slots_per_frame;
  assert(nr_ntn_radio_time_init(&ru));
  assert(hardware.calls == 0);
  assert(!nr_ntn_radio_time_init(&ru));
  unchanged_failure(NULL);
  rx(0, 1);
  unchanged_failure(NULL);
  assert(hardware.calls == 0);
  rx(1, 0);
  query();
  assert(hardware.calls == 1);
  int other;
  unchanged_failure(&other);
  assert(!nr_ntn_radio_time_query(NULL, NULL));
  assert(hardware.calls == 1);
}

static void test_rates_wrap_and_age(void)
{
  static const struct {
    unsigned int mu;
    uint32_t samples_per_subframe;
  } profiles[] =
      {{0, 1920}, {0, 7680}, {0, 30720}, {1, 7680}, {1, 15360}, {2, 30720}, {3, 61440}, {4, 122880}, {5, 245760}, {0, 429496704}};
  for (unsigned int i = 0; i < sizeof(profiles) / sizeof(profiles[0]); ++i) {
    reset(profiles[i].mu, profiles[i].samples_per_subframe);
    ru.ts_offset = 12345;
    assert(nr_ntn_radio_time_init(&ru));
    for (unsigned int slot = 0; slot < fp.slots_per_frame; ++slot)
      rx(1023, slot);
    nr_ntn_radio_time_t before = query();
    assert(before.sfn == 1023);
    assert(before.rx_frame_ticks == INT64_C(1023) * fp.samples_per_frame + ru.ts_offset);
    rx(1024, 0);
    nr_ntn_radio_time_t after = query();
    assert(after.sfn == 0 && after.generation == before.generation);
    assert(after.rx_frame_ticks == INT64_C(1024) * fp.samples_per_frame + ru.ts_offset);
    assert(hardware.calls == 2);

    hardware.time.timestamp = after.rx_frame_ticks + INT64_C(20) * fp.samples_per_frame;
    query(); // Device age exactly 200 ms is eligible at every configured rate.
    ++hardware.time.timestamp;
    unchanged_failure(NULL);
    hardware.time.timestamp = after.rx_frame_ticks - 1;
    unchanged_failure(NULL);
    hardware.time.timestamp = after.rx_frame_ticks;
    fake_ns = after.observed_monotonic_ns + NTN_RADIO_MAX_AGE_NS;
    query(); // Host age exactly 200 ms.
    ++fake_ns;
    const unsigned int calls = hardware.calls;
    unchanged_failure(NULL);
    assert(hardware.calls == calls);
    fake_ns = after.observed_monotonic_ns - 1;
    unchanged_failure(NULL);
  }
}

static void test_discontinuities_and_contention(void)
{
  reset(1, 7680);
  assert(nr_ntn_radio_time_init(&ru));
  rx(0, 0);
  nr_ntn_radio_time_t first = query();
  assert(pthread_mutex_lock(&radio_time.mutex) == 0);
  ++ru.ts_offset;
  rx(0, 1); // Offset discontinuity invalidates even under mutex contention.
  pthread_mutex_unlock(&radio_time.mutex);
  unchanged_failure(NULL);
  rx(1, 0);
  nr_ntn_radio_time_t second = query();
  assert(second.generation != first.generation && second.rx_frame_ticks == fp.samples_per_frame + 1);
  assert(pthread_mutex_lock(&radio_time.mutex) == 0);
  nr_ntn_radio_time_rx(&ru, 1, 1, false);
  rx(2, 0); // Complete frame cannot publish while mutex is held.
  pthread_mutex_unlock(&radio_time.mutex);
  unchanged_failure(NULL);
  rx(2, 1);
  rx(3, 0);
  nr_ntn_radio_time_t third = query();
  assert(third.generation != second.generation);
  rx(5, 0); // Aligned two-frame timestamp/label jump starts a new generation.
  nr_ntn_radio_time_t fourth = query();
  assert(fourth.generation != third.generation);
  ++ru.proc.timestamp_rx;
  nr_ntn_radio_time_rx(&ru, 5, 0, true); // Misaligned sample timestamp.
  unchanged_failure(NULL);
  rx(6, 0);
  query();
  nr_ntn_radio_time_rx(&ru, 7, 0, true); // Frame label disagrees with timestamp.
  unchanged_failure(NULL);
  rx(7, 0);
  query();
  ru.proc.first_rx = 1;
  nr_ntn_radio_time_rx(&ru, 7, 0, true);
  unchanged_failure(NULL);
  ru.proc.first_rx = 0;
  rx(8, 0);
  query();
  clock_failure = true;
  rx(9, 0); // Failed RX host timestamp must not leave an old generation usable.
  clock_failure = false;
  unchanged_failure(NULL);
  rx(10, 0);
  query();
}

static void test_query_failures(void)
{
  reset(0, 7680);
  assert(nr_ntn_radio_time_init(&ru));
  rx(0, 0);
  nr_ntn_radio_time_t initial = query();
  hardware.status = -1;
  unchanged_failure(NULL);
  hardware.status = 0;
  hardware.time.timestamp_rate_hz = NAN;
  unchanged_failure(NULL);
  hardware.time.timestamp_rate_hz = INFINITY;
  unchanged_failure(NULL);
  hardware.time.timestamp_rate_hz = 0;
  unchanged_failure(NULL);
  hardware.time.timestamp_rate_hz = ru.openair0_cfg.sample_rate + 1;
  unchanged_failure(NULL);
  hardware.time.timestamp_rate_hz = ru.openair0_cfg.sample_rate;
  hardware.query_elapsed_ns = NTN_RADIO_MAX_AGE_NS + 1;
  unchanged_failure(NULL); // Observation expires during the backend read.
  hardware.query_elapsed_ns = 0;
  fake_ns = initial.observed_monotonic_ns;
  hardware.clock_fail_after_query = true;
  unchanged_failure(NULL);
  hardware.clock_fail_after_query = false;
  clock_failure = false;
  hardware.partial_during_query = true;
  unchanged_failure(NULL); // Generation changes between read brackets.
  hardware.partial_during_query = false;
  rx(1, 0);
  query();
  hardware.disable_during_query = true;
  unchanged_failure(NULL);
  hardware.disable_during_query = false;
  unchanged_failure(NULL);
  assert(nr_ntn_radio_time_init(&ru));
  unchanged_failure(NULL); // Reinit cannot revive the previous observation.
  rx(2, 0);
  query();
  ru.rfdevice.trx_get_time_func = NULL;
  unchanged_failure(NULL);
  ru.rfdevice.trx_get_time_func = fake_get_time;
  nr_ntn_radio_time_disable();
  unchanged_failure(NULL);
}

static void test_integer_boundaries(void)
{
  reset(0, 7680);
  ru.ts_offset = INT64_MIN;
  assert(nr_ntn_radio_time_init(&ru));
  rx(0, 0);
  nr_ntn_radio_time_t first = query();
  assert(first.rx_frame_ticks == INT64_MIN);
  hardware.time.timestamp = INT64_MAX;
  unchanged_failure(NULL); // Unsigned age calculation spans the full int64 range.
  ru.ts_offset = INT64_MAX;
  rx(1, 0); // Raw timestamp addition would overflow; fail closed before addition.
  unchanged_failure(NULL);
  ru.ts_offset = 0;
  const uint64_t last_frame = INT64_MAX / fp.samples_per_frame;
  rx(last_frame, 0);
  hardware.time.timestamp = ru.proc.timestamp_rx;
  query();
  rx(0, 0);
  nr_ntn_radio_time_t second = query();
  assert(second.generation != first.generation);
  atomic_store_explicit(&radio_time.generation, UINT64_MAX, memory_order_release);
  nr_ntn_radio_time_rx(&ru, 0, 0, false);
  unchanged_failure(NULL);
  assert(!nr_ntn_radio_time_init(&ru)); // Generation wrap cannot silently redatestale data.
  atomic_store_explicit(&radio_time.generation, 1, memory_order_release);
}

int main(void)
{
  test_startup();
  test_rates_wrap_and_age();
  test_discontinuities_and_contention();
  test_query_failures();
  test_integer_boundaries();
  nr_ntn_radio_time_disable();
  puts("radio-time production helper: startup/rates/wrap/age/discontinuity/contention/error-atomic checks passed");
  return 0;
}
