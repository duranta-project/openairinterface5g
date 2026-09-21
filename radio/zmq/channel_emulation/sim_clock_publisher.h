/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef SIM_CLOCK_PUBLISHER_H
#define SIM_CLOCK_PUBLISHER_H

#include <cstdint>
#include <memory>

class SimClockPublisher {
 public:
  static std::unique_ptr<SimClockPublisher> create(double epoch_unix_s, double sample_rate);
  ~SimClockPublisher();

  void start(uint64_t timestamp);
  void publish(uint64_t timestamp);

 private:
  SimClockPublisher(int fd, void *mapping, double epoch_unix_s, double sample_rate);

  int fd_;
  void *mapping_;
  uint64_t epoch_unix_ns_;
  double sample_rate_;
  uint64_t start_timestamp_ = 0;
};

#endif
