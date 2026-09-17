/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef BITS_H_
#define BITS_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "assertions.h"

uint64_t reverse_bits(uint64_t in, int n_bits);
void reverse_bits_u8(uint8_t const* in, size_t sz, uint8_t* out);

// to compute ceil(log2(n))
static inline int ceil_log2_u32(uint32_t n)
{
  if (n <= 1)
    return 0;
  return 32 - __builtin_clz(n - 1);
}

// to compute floor(log2(n))
static inline uint32_t floor_log2_u32(uint32_t x)
{
  if (x == 0)
    return 0;
  uint32_t clz = __builtin_clz(x);
  return 31U - clz;
}

static inline int get_last_bit_index(const uint32_t *arr, int sz)
{
  AssertFatal(sz > 0, "Invalid size %d to get the first bit of array\n", sz);
  for (int i = sz - 1; i >= 0; i--) {
    if (arr[i] != 0) {
      // 31 - clz gives the index of the highest bit (0-31)
      return (i * 32) + (31 - __builtin_clz(arr[i]));
    }
  }
  return -1;
}

static uint32_t bit_mask(int from_bit, int num_bits)
{
  int total = from_bit + num_bits;
  uint32_t right_mask = total < 32 ? (0xFFFFFFFF >> (32 - total)) : 0xFFFFFFFF;
  return (0xFFFFFFFF << from_bit) & right_mask;
}

static inline int get_first_bit_index_mask(const uint32_t *arr, int sz, int from_bit, int num_bits)
{
  // start i from first_bit
  int i = from_bit / 32;
  from_bit %= 32;
  for (; i < sz && num_bits > 0; i++) {
    uint32_t a = arr[i] & bit_mask(from_bit, num_bits);
    if (a != 0) {
      // Find the first set bit in this 32-bit word
      return (i * 32) + __builtin_ctz(a);
    }
    num_bits -= 32 - from_bit;
    from_bit = 0;
  }
  return -1;
}

static inline int get_first_bit_index(const uint32_t *arr, int sz)
{
  return get_first_bit_index_mask(arr, sz, 0, sz * 32);
}

static inline int count_bits(const uint32_t *arr, int sz)
{
  int ret = 0;
  // sz is the number of uint32_t elements
  for (int i = 0; i < sz; i++)
    ret += __builtin_popcount(arr[i]);
  return ret;
}

static __attribute__((always_inline)) inline int count_bits64(uint64_t v)
{
  return __builtin_popcountll(v);
}

static __attribute__((always_inline)) inline int count_bits64_with_mask(uint64_t v, int start, int num)
{
  AssertFatal(start >= 0 && num > 0 && start + num <= 64, "Invalid range: start=%d num=%d for 64 bits mask\n", start, num);
  uint64_t mask = (num == 64 ? ~0ULL : (1ULL << num) - 1) << start;
  return count_bits64(v & mask);
}

bool find_next_rb_block(const uint32_t *bitmap, int size, int *pos, int *start, int *end);
typedef struct {
  int first_rb;
  int last_rb;
  int num_rbs;
  uint32_t bitmap[9];
} freq_alloc_bitmap_t;
bool check_rb_in_bitmap(const freq_alloc_bitmap_t *alloc, int rb);
freq_alloc_bitmap_t set_start_end_from_bitmap(int size, int alloc_size, const uint8_t *bitmap);
freq_alloc_bitmap_t set_bitmap_from_start_size(int start, int size);

#endif /* BITS_H_ */
