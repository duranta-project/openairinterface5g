/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "ntn_sim_clock_publisher.h"
#include "common/utils/LOG/log.h"
#include "openair1/SIMULATION/TOOLS/ntn_sim_clock.h"
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
int open_sim_clock(const char *name)
{
  int fd = shm_open(name, O_RDWR, 0);
  if (fd >= 0 || errno != ENOENT)
    return fd;

  fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0666);
  if (fd < 0 && errno == EEXIST)
    fd = shm_open(name, O_RDWR, 0);
  return fd;
}
} // namespace

std::unique_ptr<NtnSimClockPublisher> NtnSimClockPublisher::create(double epoch_unix_s, double sample_rate)
{
  const char *configured_name = getenv("OAI_NTN_SIM_CLOCK_SHM");
  const char *name = configured_name != nullptr ? configured_name : NTN_SIM_CLOCK_DEFAULT_SHM;
  const int fd = open_sim_clock(name);
  if (fd < 0) {
    LOG_E(HW, "[chanmod] Could not open NTN simulation clock %s: %s\n", name, strerror(errno));
    return nullptr;
  }
  if (fchmod(fd, 0666) != 0)
    LOG_W(HW, "[chanmod] Could not change NTN simulation clock %s permissions: %s\n", name, strerror(errno));
  if (ftruncate(fd, sizeof(ntn_sim_clock_shared_t)) != 0) {
    LOG_E(HW, "[chanmod] Could not size NTN simulation clock %s: %s\n", name, strerror(errno));
    close(fd);
    return nullptr;
  }
  void *mapping = mmap(nullptr, sizeof(ntn_sim_clock_shared_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mapping == MAP_FAILED) {
    LOG_E(HW, "[chanmod] Could not map NTN simulation clock %s: %s\n", name, strerror(errno));
    close(fd);
    return nullptr;
  }
  return std::unique_ptr<NtnSimClockPublisher>(new NtnSimClockPublisher(fd, mapping, epoch_unix_s, sample_rate));
}

NtnSimClockPublisher::NtnSimClockPublisher(int fd, void *mapping, double epoch_unix_s, double sample_rate)
    : fd_(fd), mapping_(mapping), epoch_unix_ns_(std::llround(epoch_unix_s * 1e9)), sample_rate_(sample_rate)
{
  auto *clock = static_cast<ntn_sim_clock_shared_t *>(mapping_);
  clock->version = NTN_SIM_CLOCK_VERSION;
  clock->reserved = 0;
  __atomic_store_n(&clock->unix_time_ns, epoch_unix_ns_, __ATOMIC_RELAXED);
  __atomic_store_n(&clock->magic, NTN_SIM_CLOCK_MAGIC, __ATOMIC_RELEASE);
}

NtnSimClockPublisher::~NtnSimClockPublisher()
{
  munmap(mapping_, sizeof(ntn_sim_clock_shared_t));
  close(fd_);
}

void NtnSimClockPublisher::start(uint64_t timestamp)
{
  start_timestamp_ = timestamp;
  publish(timestamp);
}

void NtnSimClockPublisher::publish(uint64_t timestamp)
{
  const uint64_t elapsed_samples = timestamp >= start_timestamp_ ? timestamp - start_timestamp_ : 0;
  const uint64_t elapsed_ns = std::llround(static_cast<double>(elapsed_samples) * 1e9 / sample_rate_);
  auto *clock = static_cast<ntn_sim_clock_shared_t *>(mapping_);
  __atomic_store_n(&clock->unix_time_ns, epoch_unix_ns_ + elapsed_ns, __ATOMIC_RELEASE);
}
