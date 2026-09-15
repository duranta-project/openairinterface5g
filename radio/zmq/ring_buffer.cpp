/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "ring_buffer.h"
#include <cstring>
#include <iostream>
#include <algorithm>

template <typename T>
ring_buffer<T>::ring_buffer(size_t max_size) : max_size_(max_size)
{
  buffer_ = std::make_unique<T[]>(max_size);
}

template <typename T>
size_t ring_buffer<T>::push_samples(const T *samples, const size_t nsamps)
{
  size_t overflow = 0;
  // if nsamps > max_size skip nsamps - max_size samples
  size_t nsamps_left = nsamps;
  if (nsamps > max_size_) {
    samples += nsamps - max_size_;
    nsamps_left = max_size_;
    overflow += nsamps - max_size_;
  }

  // Detect overflow
  if (size_ + nsamps_left > max_size_) {
    size_t newtail__pos = (head_ + nsamps_left) % max_size_;
    overflow += (size_ + nsamps_left) - max_size_;
    tail_ = newtail__pos;
  }

  size_t first_chunk = std::min(nsamps_left, max_size_ - head_);
  memcpy(&buffer_[head_], samples, first_chunk * sizeof(T));
  head_ = (head_ + first_chunk) % max_size_;
  samples += first_chunk;
  nsamps_left -= first_chunk;
  if (nsamps_left > 0) {
    memcpy(&buffer_[0], samples, nsamps_left * sizeof(T));
    head_ = nsamps_left;
  }

  size_ = std::min(size_ + nsamps, max_size_);

  return overflow;
}

template <typename T>
size_t ring_buffer<T>::push_zeros(const size_t num_zeros)
{
  size_t overflow = 0;
  // if nsamps > max_size skip nsamps - max_size samples
  size_t nsamps_left = num_zeros;
  if (num_zeros > max_size_) {
    nsamps_left = max_size_;
    overflow += num_zeros - max_size_;
  }

  // Detect overflow
  if (size_ + nsamps_left > max_size_) {
    size_t new_tail_pos = (head_ + nsamps_left) % max_size_;
    overflow += (size_ + nsamps_left) - max_size_;
    tail_ = new_tail_pos;
  }

  size_t first_chunk = std::min(nsamps_left, max_size_ - head_);
  memset(&buffer_[head_], 0, first_chunk * sizeof(T));
  head_ = (head_ + first_chunk) % max_size_;
  nsamps_left -= first_chunk;
  if (nsamps_left > 0) {
    memset(&buffer_[0], 0, nsamps_left * sizeof(T));
    head_ = nsamps_left;
  }

  size_ = std::min(size_ + num_zeros, max_size_);

  return overflow;
}

template <typename T>
size_t ring_buffer<T>::pop_samples(T *samples, size_t num_samples)
{
  size_t samples_to_pop = std::min(size_, num_samples);
  if (samples_to_pop > 0) {
    if (tail_ + samples_to_pop > max_size_) {
      size_t first_chunk = max_size_ - tail_;
      memcpy(samples, &buffer_[tail_], first_chunk * sizeof(T));
      memcpy(samples + first_chunk, &buffer_[0], (samples_to_pop - first_chunk) * sizeof(T));
    } else {
      memcpy(samples, &buffer_[tail_], samples_to_pop * sizeof(T));
    }
    tail_ = (tail_ + samples_to_pop) % max_size_;
    size_ -= samples_to_pop;
    return samples_to_pop;
  }
  return 0;
}

template <typename T>
size_t ring_buffer<T>::copy_range(T *out, size_t count, size_t skip_from_tail) const
{
  if (skip_from_tail >= size_)
    return 0;
  const size_t n = std::min(count, size_ - skip_from_tail);
  const size_t start = (tail_ + skip_from_tail) % max_size_;
  if (start + n > max_size_) {
    const size_t first_chunk = max_size_ - start;
    memcpy(out, &buffer_[start], first_chunk * sizeof(T));
    memcpy(out + first_chunk, &buffer_[0], (n - first_chunk) * sizeof(T));
  } else {
    memcpy(out, &buffer_[start], n * sizeof(T));
  }
  return n;
}

template <typename T>
void ring_buffer<T>::clear_samples()
{
  head_ = 0;
  tail_ = 0;
  size_ = 0;
}

template <typename T>
void ring_buffer<T>::reset()
{
  clear_samples();
}

template <typename T>
size_t ring_buffer<T>::size() const
{
  return size_;
}

template <typename T>
size_t overflow_buffer<T>::push_samples(const T *samples, size_t nsamps)
{
  std::lock_guard<std::mutex> lock(mutex_);
  size_t overflow = buffer_.push_samples(samples, nsamps);
  zeros_to_send_ += overflow;
  return overflow;
}

template <typename T>
size_t overflow_buffer<T>::push_zeros(size_t num_zeros)
{
  std::lock_guard<std::mutex> lock(mutex_);
  size_t overflow = buffer_.push_zeros(num_zeros);
  zeros_to_send_ += overflow;
  return overflow;
}

template <typename T>
size_t overflow_buffer<T>::pop_samples(T *samples, size_t num_samples)
{
  std::lock_guard<std::mutex> lock(mutex_);
  size_t samples_popped = 0;
  if (zeros_to_send_ > 0) {
    size_t num_zeros = std::min(zeros_to_send_, num_samples);
    memset(samples, 0, num_zeros * sizeof(T));
    zeros_to_send_ -= num_zeros;
    samples += num_zeros;
    num_samples -= num_zeros;
    samples_popped += num_zeros;
  }

  if (num_samples > 0) {
    samples_popped += buffer_.pop_samples(samples, num_samples);
  }
  return samples_popped;
}

template <typename T>
void overflow_buffer<T>::reset()
{
  std::lock_guard<std::mutex> lock(mutex_);
  buffer_.reset();
  zeros_to_send_ = buffer_.size() / 2;
}

template <typename T>
void overflow_buffer<T>::clear_samples()
{
  std::lock_guard<std::mutex> lock(mutex_);
  buffer_.clear_samples();
  zeros_to_send_ = 0;
}

template <typename T>
size_t overflow_buffer<T>::size()
{
  std::lock_guard<std::mutex> lock(mutex_);
  return buffer_.size() + zeros_to_send_;
}

template class ring_buffer<cf_t>;
template class ring_buffer<c16_t>;
template class overflow_buffer<cf_t>;
template class overflow_buffer<c16_t>;
