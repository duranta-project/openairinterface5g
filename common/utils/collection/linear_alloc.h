/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef LINEAR_ALLOC_H
#define LINEAR_ALLOC_H

#include <limits.h>
#include <stdint.h>

typedef unsigned int uid_t;
#define UID_LINEAR_ALLOCATOR_SIZE 1024
#define UID_LINEAR_ALLOCATOR_BITMAP_SIZE (((UID_LINEAR_ALLOCATOR_SIZE/8)/sizeof(unsigned int)) + 1)
typedef struct uid_linear_allocator_s {
  unsigned int bitmap[UID_LINEAR_ALLOCATOR_BITMAP_SIZE];
} uid_allocator_t;

static inline void uid_linear_allocator_init(uid_allocator_t *uia) {
  memset(uia, 0, sizeof(uid_allocator_t));
}

static inline uid_t uid_linear_allocator_new(uid_allocator_t *uia) {
  unsigned int bit_index = 1;
  uid_t        uid = 0;

  for (unsigned int i = 0; i < UID_LINEAR_ALLOCATOR_BITMAP_SIZE; i++) {
    if (uia->bitmap[i] != UINT_MAX) {
      bit_index = 1;
      uid       = 0;

      while ((uia->bitmap[i] & bit_index) == bit_index) {
        bit_index = bit_index << 1;
        uid       += 1;
      }

      uia->bitmap[i] |= bit_index;
      return uid + (i*sizeof(unsigned int)*8);
    }
  }

  return UINT_MAX;
}


static inline void uid_linear_allocator_free(uid_allocator_t *uia, uid_t uid) {
  const unsigned int i = uid/sizeof(unsigned int)/8;
  const unsigned int bit = uid % (sizeof(unsigned int) * 8);
  const unsigned int value = ~(1U << bit);

  if (i < UID_LINEAR_ALLOCATOR_BITMAP_SIZE) {
    uia->bitmap[i] &= value;
  }
}

/* short alloc (64 bits case) */

typedef uint64_t uid_allocator_64bits_t;

static inline void uid_linear_allocator_64_bits_init(uid_allocator_64bits_t *uia)
{
  *uia = 0;
}

static inline uid_t uid_linear_allocator_64_bits_new(uid_allocator_64bits_t *uia)
{
  uint64_t mask = 1;
  uint64_t cur = *uia;
  for (int bit = 0; bit < 64; bit++, mask <<= 1) {
    if (!(cur & mask)) {
      *uia |= mask;
      return bit;
    }
  }
  /* full => return 64 */
  return 64;
}

static inline void uid_linear_allocator_64_bits_free(uid_allocator_64bits_t *uia, uid_t uid)
{
  uint64_t mask = (uint64_t)1 << (uint64_t)uid;
  *uia &= ~mask;
}

#endif /* LINEAR_ALLOC_H */
