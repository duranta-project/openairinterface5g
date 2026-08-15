/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "bits.h"
#include "utils.h"
// we avoid assertions.h as it necessitates other dependencies
#include <assert.h>
#include <simde/x86/gfni.h>

static const uint8_t bit_reverse_table_256[] = {
    0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0, 0x08, 0x88, 0x48, 0xC8,
    0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8, 0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4,
    0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4, 0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC,
    0x3C, 0xBC, 0x7C, 0xFC, 0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
    0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA, 0x06, 0x86, 0x46, 0xC6,
    0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6, 0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE,
    0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE, 0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1,
    0x31, 0xB1, 0x71, 0xF1, 0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
    0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5, 0x0D, 0x8D, 0x4D, 0xCD,
    0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD, 0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3,
    0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3, 0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB,
    0x3B, 0xBB, 0x7B, 0xFB, 0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
    0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF};

void reverse_bits_u8(uint8_t const* in, size_t sz, uint8_t* out)
{
  assert(in != NULL);
  assert(out != NULL);

// Bit reversal implementation based on https://wunkolo.github.io/post/2020/11/gf2p8affineqb-bit-reversal/
#if defined(__GFNI__) && defined(__AVX512F__)
  int simde_sz = 64;
  int i = 0;
  int simde_bound = sz - simde_sz;
  for (; i <= simde_bound; i += simde_sz) {
    __m512i input = _mm512_loadu_epi8(&in[i]);
    __m512i reversed = _mm512_gf2p8affine_epi64_epi8(input, _mm512_set1_epi64(0x8040201008040201), 0);
    _mm512_storeu_epi8(&out[i], reversed);
  }

  for (; i < sz; ++i) {
    out[i] = bit_reverse_table_256[in[i]];
  }
#else
  for(size_t i = 0; i < sz; ++i)
    out[i] = bit_reverse_table_256[in[i]];
#endif
}


// Reverse bits implementation based on http://graphics.stanford.edu/~seander/bithacks.html
uint64_t reverse_bits(uint64_t in, int n_bits)
{
  // Reverse n_bits in uint64_t variable, example:
  // n_bits: 10
  // in:      10 0000 1111
  // return:  11 1100 0001

  assert(n_bits <= 64); // Maximum bits to reverse is 64
  uint64_t rev_bits = 0;
  uint8_t *p = (uint8_t *)&in;
  uint8_t *q = (uint8_t *)&rev_bits;
  int n_bytes = n_bits >> 3;
  for (int n = 0; n < n_bytes; n++) {
    q[n_bytes - 1 - n] = bit_reverse_table_256[p[n]];
  }

  // Reverse remaining bits (not aligned with 8-bit)
  rev_bits = rev_bits << (n_bits % 8);
  for (int i = n_bytes * 8; i < n_bits; i++) {
    rev_bits |= ((in >> i) & 0x1) << (n_bits - i - 1);
  }
  return rev_bits;
}

/**
 * Finds the next contiguous block of set bits.
 * @param bitmap The bit buffer
 * @param size   Total bits in the buffer
 * @param pos    Pointer to current bit index (updated by function)
 * @param start  Output: Start index of the found block
 * @param end    Output: End index of the found block
 * @return true if a block was found, false if no more bits are set
 */
bool find_next_rb_block(const uint32_t *bitmap, int size, int *pos, int *start, int *end)
{
  const int words = (size + 31) / 32;

  // Find start of next block
  int idx = get_first_bit_index_mask(bitmap, words, *pos, size - *pos);
  if (idx < 0)
    return false;
  *start = idx;

  // Find end of block: first zero bit after idx
  // Reuse get_first_bit_index_from on the inverted bitmap
  idx++;
  int word_idx = idx / 32;
  int n = words - word_idx;
  uint32_t inv[n];
  for (int i = 0; i < n; i++)
    inv[i] = ~bitmap[word_idx + i];
  if (size % 32 != 0)
    inv[n - 1] &= (1u << (size % 32)) - 1;

  int end_off = get_first_bit_index_mask(inv, n, idx - word_idx * 32, size - idx);
  idx = (end_off < 0) ? size : word_idx * 32 + end_off;

  *end = (idx > size) ? size - 1 : idx - 1;
  *pos = idx; // Update position for the next call
  return true;
}

bool check_rb_in_bitmap(const freq_alloc_bitmap_t *alloc, int rb)
{
  return (alloc->bitmap[rb / 32] >> (rb % 32)) & 0x01;
}

freq_alloc_bitmap_t set_start_end_from_bitmap(int size, int alloc_size, const uint8_t *bitmap)
{
  freq_alloc_bitmap_t alloc = {0};
  int num_words = (size + 31) / 32;
  memcpy(alloc.bitmap, bitmap, alloc_size);
  alloc.num_rbs = count_bits(alloc.bitmap, num_words);
  alloc.first_rb = get_first_bit_index(alloc.bitmap, num_words);
  alloc.last_rb = get_last_bit_index(alloc.bitmap, num_words);
  AssertFatal(alloc.first_rb >= 0 && alloc.last_rb >=0 , "No valid RB set, no bit set in bitmap\n");
  return alloc;
}

freq_alloc_bitmap_t set_bitmap_from_start_size(int start, int size)
{
  freq_alloc_bitmap_t alloc = {
    .num_rbs = size,
    .first_rb = start,
    .last_rb = start + size - 1
  };
  memset(alloc.bitmap, 0, sizeof(alloc.bitmap));
  for (int i = start; i < size + start; i++)
    alloc.bitmap[i / 32] |= (1U << (i % 32));
  return alloc;
}
