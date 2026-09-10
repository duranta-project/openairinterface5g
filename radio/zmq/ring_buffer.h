/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#pragma once
#include <vector>
#include <mutex>
#include <memory>
#include "common/platform_types.h"

// A basic circular sample buffer class
template <typename T = cf_t>
class ring_buffer {
 private:
  std::unique_ptr<T[]> buffer_;
  size_t head_ = 0;
  size_t tail_ = 0;
  size_t size_ = 0;
  size_t max_size_;

 public:
  ring_buffer(size_t max_size = 614400);
  size_t push_samples(const T *samples, size_t nsamps);
  size_t push_zeros(size_t num_zeros);
  size_t pop_samples(T *samples, size_t num_samples);
  // Copy without consuming, skipping skip_from_tail oldest samples; return copied count and leave remaining output untouched.
  size_t copy_range(T *out, size_t count, size_t skip_from_tail) const;
  void reset();
  void clear_samples();
  size_t size() const;
};

// A thread-safe wrapper around ring_buffer that counts overflows
template <typename T = cf_t>
class overflow_buffer {
  ring_buffer<T> buffer_;
  size_t zeros_to_send_;
  std::mutex mutex_;

 public:
  overflow_buffer(size_t max_size = 614400) : buffer_(max_size), zeros_to_send_(0)
  {
  }
  size_t push_samples(const T *samples, size_t nsamps);
  size_t push_zeros(size_t num_zeros);
  size_t pop_samples(T *samples, size_t num_samples);
  void reset();
  void clear_samples();
  size_t size();
};
