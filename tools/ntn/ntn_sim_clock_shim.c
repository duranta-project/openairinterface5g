/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#define _GNU_SOURCE
#include "openair1/SIMULATION/TOOLS/ntn_sim_clock.h"
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

static ntn_sim_clock_shared_t *shared_clock;
static unsigned int map_attempts;

static ntn_sim_clock_shared_t *get_shared_clock(void)
{
  ntn_sim_clock_shared_t *clock = __atomic_load_n(&shared_clock, __ATOMIC_ACQUIRE);
  if (clock != NULL)
    return clock;

  if ((__atomic_add_fetch(&map_attempts, 1, __ATOMIC_RELAXED) & 0xfff) != 1)
    return NULL;

  const char *configured_name = getenv("OAI_NTN_SIM_CLOCK_SHM");
  const char *name = configured_name != NULL ? configured_name : NTN_SIM_CLOCK_DEFAULT_SHM;
  const int fd = shm_open(name, O_RDONLY, 0);
  if (fd < 0)
    return NULL;
  void *mapping = mmap(NULL, sizeof(ntn_sim_clock_shared_t), PROT_READ, MAP_SHARED, fd, 0);
  close(fd);
  if (mapping == MAP_FAILED)
    return NULL;

  ntn_sim_clock_shared_t *expected = NULL;
  if (!__atomic_compare_exchange_n(&shared_clock, &expected, mapping, false, __ATOMIC_RELEASE, __ATOMIC_ACQUIRE)) {
    munmap(mapping, sizeof(ntn_sim_clock_shared_t));
  }
  return __atomic_load_n(&shared_clock, __ATOMIC_ACQUIRE);
}

static int read_simulated_time(struct timespec *tp, clockid_t clock_id)
{
  ntn_sim_clock_shared_t *clock = get_shared_clock();
  if (clock == NULL || __atomic_load_n(&clock->magic, __ATOMIC_ACQUIRE) != NTN_SIM_CLOCK_MAGIC
      || clock->version != NTN_SIM_CLOCK_VERSION)
    return -1;

  uint64_t time_ns = __atomic_load_n(&clock->unix_time_ns, __ATOMIC_ACQUIRE);
  if (clock_id == CLOCK_TAI) {
    const char *configured_offset = getenv("OAI_NTN_TAI_OFFSET");
    const long tai_offset = configured_offset != NULL ? strtol(configured_offset, NULL, 10) : 37;
    time_ns += (uint64_t)tai_offset * 1000000000ULL;
  }
  tp->tv_sec = (time_t)(time_ns / 1000000000ULL);
  tp->tv_nsec = (long)(time_ns % 1000000000ULL);
  return 0;
}

int clock_gettime(clockid_t clock_id, struct timespec *tp)
{
  if ((clock_id == CLOCK_REALTIME || clock_id == CLOCK_TAI) && read_simulated_time(tp, clock_id) == 0)
    return 0;
  return (int)syscall(SYS_clock_gettime, clock_id, tp);
}

int __clock_gettime(clockid_t clock_id, struct timespec *tp)
{
  return clock_gettime(clock_id, tp);
}

time_t time(time_t *result)
{
  struct timespec ts;
  if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
    return (time_t)-1;
  if (result != NULL)
    *result = ts.tv_sec;
  return ts.tv_sec;
}

int gettimeofday(struct timeval *tv, void *timezone)
{
  (void)timezone;
  struct timespec ts;
  if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
    return -1;
  tv->tv_sec = ts.tv_sec;
  tv->tv_usec = ts.tv_nsec / 1000;
  return 0;
}
