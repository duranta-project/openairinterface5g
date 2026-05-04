/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_modulation.h"
#include "openair1/PHY/TOOLS/tools_defs.h"
#include "PHY/NR_REFSIG/nr_mod_table.h"
#include "executables/softmodem-common.h"
#include <simde/x86/avx512.h>
// Lacking declaration in older implementations of simde external package, so let's keep it for now to be backwards compatible
#if !defined(simde_mm512_extracti64x2_epi64)
#define simde_mm512_extracti64x2_epi64(a...) _mm512_extracti64x2_epi64(a)
#endif

// #define DEBUG_DLSCH_PRECODING_PRINT_WITH_TRIVIAL // TODO: For debug, to be removed if want to merge to develop
// #define DEBUG_LAYER_MAPPING
#define USE_NEON
// #define USE_GATHER
//  Table 6.3.1.5-1 Precoding Matrix W 1 layer 2 antenna ports 'n' = -1 and 'o' = -j
const char nr_W_1l_2p[6][2][1] = {
    {{'1'}, {'0'}}, // pmi 0
    {{'0'}, {'1'}},
    {{'1'}, {'1'}},
    {{'1'}, {'n'}},
    {{'1'}, {'j'}},
    {{'1'}, {'o'}} // pmi 5
};

// Table 6.3.1.5-3 Precoding Matrix W 1 layer 4 antenna ports 'n' = -1 and 'o' = -j
const char nr_W_1l_4p[28][4][1] = {
    {{'1'}, {'0'}, {'0'}, {'0'}}, // pmi 0
    {{'0'}, {'1'}, {'0'}, {'0'}},
    {{'0'}, {'0'}, {'1'}, {'0'}},
    {{'0'}, {'0'}, {'0'}, {'1'}},
    {{'1'}, {'0'}, {'1'}, {'0'}},
    {{'1'}, {'0'}, {'n'}, {'0'}},
    {{'1'}, {'0'}, {'j'}, {'0'}},
    {{'1'}, {'0'}, {'o'}, {'0'}}, // pmi 7
    {{'0'}, {'1'}, {'0'}, {'1'}}, // pmi 8
    {{'0'}, {'1'}, {'0'}, {'n'}},
    {{'0'}, {'1'}, {'0'}, {'j'}},
    {{'0'}, {'1'}, {'0'}, {'o'}},
    {{'1'}, {'1'}, {'1'}, {'1'}},
    {{'1'}, {'1'}, {'j'}, {'j'}},
    {{'1'}, {'1'}, {'n'}, {'n'}},
    {{'1'}, {'1'}, {'o'}, {'o'}},
    {{'1'}, {'j'}, {'1'}, {'j'}}, // pmi
    // 16
    {{'1'}, {'j'}, {'j'}, {'n'}},
    {{'1'}, {'j'}, {'n'}, {'o'}},
    {{'1'}, {'j'}, {'o'}, {'1'}},
    {{'1'}, {'n'}, {'1'}, {'n'}},
    {{'1'}, {'n'}, {'j'}, {'o'}},
    {{'1'}, {'n'}, {'n'}, {'1'}},
    {{'1'}, {'n'}, {'o'}, {'j'}}, // pmi 23
    {{'1'}, {'o'}, {'1'}, {'o'}}, // pmi 24
    {{'1'}, {'o'}, {'j'}, {'1'}},
    {{'1'}, {'o'}, {'n'}, {'j'}},
    {{'1'}, {'o'}, {'o'}, {'n'}} // pmi 27
};

// Table 6.3.1.5-4 Precoding Matrix W 2 antenna ports layers 2  'n' = -1 and 'o' = -j
const char nr_W_2l_2p[3][2][2] = {
    {{'1', '0'}, {'0', '1'}}, // pmi 0
    {{'1', '1'}, {'1', 'n'}},
    {{'1', '1'}, {'j', 'o'}} // pmi 2
};

// Table 6.3.1.5-5 Precoding Matrix W 2 layers 4 antenna ports 'n' = -1 and 'o' = -j
const char nr_W_2l_4p[22][4][2] = {
    {{'1', '0'}, {'0', '1'}, {'0', '0'}, {'0', '0'}}, // pmi 0
    {{'1', '0'}, {'0', '0'}, {'0', '1'}, {'0', '0'}}, {{'1', '0'}, {'0', '0'}, {'0', '0'}, {'0', '1'}},
    {{'0', '0'}, {'1', '0'}, {'0', '1'}, {'0', '0'}}, // pmi 3
    {{'0', '0'}, {'1', '0'}, {'0', '0'}, {'0', '1'}}, // pmi 4
    {{'0', '0'}, {'0', '0'}, {'1', '0'}, {'0', '1'}}, {{'1', '0'}, {'0', '1'}, {'1', '0'}, {'0', 'o'}},
    {{'1', '0'}, {'0', '1'}, {'1', '0'}, {'0', 'j'}}, {{'1', '0'}, {'0', '1'}, {'o', '0'}, {'0', '1'}}, // pmi 8
    {{'1', '0'}, {'0', '1'}, {'o', '0'}, {'0', 'n'}}, {{'1', '0'}, {'0', '1'}, {'n', '0'}, {'0', 'o'}},
    {{'1', '0'}, {'0', '1'}, {'n', '0'}, {'0', 'j'}}, // pmi 11
    {{'1', '0'}, {'0', '1'}, {'j', '0'}, {'0', '1'}}, // pmi 12
    {{'1', '0'}, {'0', '1'}, {'j', '0'}, {'0', 'n'}}, {{'1', '1'}, {'1', '1'}, {'1', 'n'}, {'1', 'n'}},
    {{'1', '1'}, {'1', '1'}, {'j', 'o'}, {'j', 'o'}}, // pmi 15
    {{'1', '1'}, {'j', 'j'}, {'1', 'n'}, {'j', 'o'}}, // pmi 16
    {{'1', '1'}, {'j', 'j'}, {'j', 'o'}, {'n', '1'}}, {{'1', '1'}, {'n', 'n'}, {'1', 'n'}, {'n', '1'}},
    {{'1', '1'}, {'n', 'n'}, {'j', 'o'}, {'o', 'j'}}, // pmi 19
    {{'1', '1'}, {'o', 'o'}, {'1', 'n'}, {'o', 'j'}}, {{'1', '1'}, {'o', 'o'}, {'j', 'o'}, {'1', 'n'}} // pmi 21
};

// Table 6.3.1.5-6 Precoding Matrix W 3 layers 4 antenna ports 'n' = -1 and 'o' = -j
const char nr_W_3l_4p[7][4][3] = {{{'1', '0', '0'}, {'0', '1', '0'}, {'0', '0', '1'}, {'0', '0', '0'}}, // pmi 0
                                  {{'1', '0', '0'}, {'0', '1', '0'}, {'1', '0', '0'}, {'0', '0', '1'}},
                                  {{'1', '0', '0'}, {'0', '1', '0'}, {'n', '0', '0'}, {'0', '0', '1'}},
                                  {{'1', '1', '1'}, {'1', 'n', '1'}, {'1', '1', 'n'}, {'1', 'n', 'n'}}, // pmi 3
                                  {{'1', '1', '1'}, {'1', 'n', '1'}, {'j', 'j', 'o'}, {'j', 'o', 'o'}}, // pmi 4
                                  {{'1', '1', '1'}, {'n', '1', 'n'}, {'1', '1', 'n'}, {'n', '1', '1'}},
                                  {{'1', '1', '1'}, {'n', '1', 'n'}, {'j', 'j', 'o'}, {'o', 'j', 'j'}}};

// Table 6.3.1.5-7 Precoding Matrix W 4 layers 4 antenna ports 'n' = -1 and 'o' = -j
const char nr_W_4l_4p[5][4][4] = {
    {{'1', '0', '0', '0'}, {'0', '1', '0', '0'}, {'0', '0', '1', '0'}, {'0', '0', '0', '1'}}, // pmi 0
    {{'1', '1', '0', '0'}, {'0', '0', '1', '1'}, {'1', 'n', '0', '0'}, {'0', '0', '1', 'n'}},
    {{'1', '1', '0', '0'}, {'0', '0', '1', '1'}, {'j', 'o', '0', '0'}, {'0', '0', 'j', 'o'}},
    {{'1', '1', '1', '1'}, {'1', 'n', '1', 'n'}, {'1', '1', 'n', 'n'}, {'1', 'n', 'n', '1'}}, // pmi 3
    {{'1', '1', '1', '1'}, {'1', 'n', '1', 'n'}, {'j', 'j', 'o', 'o'}, {'j', 'o', 'o', 'j'}} // pmi 4
};

void nr_modulation(const uint32_t *in, uint32_t length, uint16_t mod_order, int16_t *out)
{
  const uint16_t mask = ((1 << mod_order) - 1);
  int32_t *out32 = (int32_t *)out;
  const uint8_t *in_bytes = (const uint8_t *)in;
  const uint64_t *in64 = (const uint64_t *)in;
  int64_t *out64 = (int64_t *)out;
  uint32_t i = 0;

  LOG_D(PHY, "nr_modulation: length %d, mod_order %d\n", length, mod_order);

  switch (mod_order) {
    case 2: {
      simde__m128i *nr_mod_table128 = (simde__m128i *)nr_qpsk_byte_mod_table;
      simde__m128i *out128 = (simde__m128i *)out;
      for (i = 0; i < length / 8; i++)
        out128[i] = nr_mod_table128[in_bytes[i]];
      // the bits that are left out
      i = i * 8 / 2;
      int32_t *nr_mod_table32 = (int32_t *)nr_qpsk_mod_table;
      while (i < length / 2) {
        const int idx = ((in_bytes[(i * 2) / 8] >> ((i * 2) & 0x7)) & mask);
        out32[i] = nr_mod_table32[idx];
        i++;
      }
    }
      return;

    case 4:
      for (i = 0; i < length / 8; i++)
        out64[i] = nr_16qam_byte_mod_table[in_bytes[i]];
      // the bits that are left out
      i = i * 8 / 4;
      while (i < length / 4) {
        const int idx = ((in_bytes[(i * 4) / 8] >> ((i * 4) & 0x7)) & mask);
        out32[i] = nr_16qam_mod_table[idx];
        i++;
      }
      return;

    case 6:
      if (length > (3 * 64))
        for (i = 0; i < length - 3 * 64; i += 3 * 64) {
          uint64_t x = *in64++;
          uint64_t x1 = x & 0xfff;
          *out64++ = nr_64qam_mod_table[x1];
          x1 = (x >> 12) & 0xfff;
          *out64++ = nr_64qam_mod_table[x1];
          x1 = (x >> 24) & 0xfff;
          *out64++ = nr_64qam_mod_table[x1];
          x1 = (x >> 36) & 0xfff;
          *out64++ = nr_64qam_mod_table[x1];
          x1 = (x >> 48) & 0xfff;
          *out64++ = nr_64qam_mod_table[x1];
          uint64_t x2 = (x >> 60);
          x = *in64++;
          x2 |= x << 4;
          x1 = x2 & 0xfff;
          *out64++ = nr_64qam_mod_table[x1];
          x1 = (x2 >> 12) & 0xfff;
          *out64++ = nr_64qam_mod_table[x1];
          x1 = (x2 >> 24) & 0xfff;
          *out64++ = nr_64qam_mod_table[x1];
          x1 = (x2 >> 36) & 0xfff;
          *out64++ = nr_64qam_mod_table[x1];
          x1 = (x2 >> 48) & 0xfff;
          *out64++ = nr_64qam_mod_table[x1];
          x2 = ((x >> 56) & 0xf0) | (x2 >> 60);
          x = *in64++;
          x2 |= x << 8;
          x1 = x2 & 0xfff;
          *out64++ = nr_64qam_mod_table[x1];
          x1 = (x2 >> 12) & 0xfff;
          *out64++ = nr_64qam_mod_table[x1];
          x1 = (x2 >> 24) & 0xfff;
          *out64++ = nr_64qam_mod_table[x1];
          x1 = (x2 >> 36) & 0xfff;
          *out64++ = nr_64qam_mod_table[x1];
          x1 = (x2 >> 48) & 0xfff;
          *out64++ = nr_64qam_mod_table[x1];
          x2 = ((x >> 52) & 0xff0) | (x2 >> 60);
          *out64++ = nr_64qam_mod_table[x2];
        }

      while (i + 24 <= length) {
        uint32_t xx = 0;
        memcpy(&xx, in_bytes + i / 8, 3);
        uint64_t x1 = xx & 0xfff;
        *out64++ = nr_64qam_mod_table[x1];
        x1 = (xx >> 12) & 0xfff;
        *out64++ = nr_64qam_mod_table[x1];
        i += 24;
      }
      if (i != length) {
        uint32_t xx = 0;
        memcpy(&xx, in_bytes + i / 8, 2);
        uint64_t x1 = xx & 0xfff;
        *out64++ = nr_64qam_mod_table[x1];
      }
      return;

    case 8: {
      int32_t *nr_mod_table32 = (int32_t *)nr_256qam_mod_table;
      for (i = 0; i < length / 8; i++)
        out32[i] = nr_mod_table32[in_bytes[i]];
    }
      return;

    default:
      break;
  }
  AssertFatal(false, "Invalid or unsupported modulation order %d\n", mod_order);
}

static inline uint8_t get_packed_symbol(const uint8_t *in_bytes, uint32_t length, uint16_t mod_order, uint32_t symbol_idx)
{
  const uint32_t bit_offset = symbol_idx * mod_order;
  const uint32_t byte_offset = bit_offset >> 3;
  const uint32_t bit_shift = bit_offset & 0x7;
  const uint32_t num_bytes = (length + 7) >> 3;
  uint16_t packed = in_bytes[byte_offset];
  if (bit_shift > 8 - mod_order && byte_offset + 1 < num_bytes)
    packed |= (uint16_t)in_bytes[byte_offset + 1] << 8;
  return (packed >> bit_shift) & ((1U << mod_order) - 1);
}

static inline uint64_t get_packed_bits(const uint8_t *in_bytes, uint32_t length, uint32_t bit_offset, uint8_t width)
{
  const uint32_t byte_offset = bit_offset >> 3;
  const uint32_t bit_shift = bit_offset & 0x7;
  const uint32_t num_bytes = (length + 7) >> 3;
  const uint32_t bytes_needed = (bit_shift + width + 7) >> 3;
  uint64_t packed = 0;
  for (uint32_t i = 0; i < bytes_needed && byte_offset + i < num_bytes; i++)
    packed |= (uint64_t)in_bytes[byte_offset + i] << (i << 3);
  return (packed >> bit_shift) & ((UINT64_C(1) << width) - 1);
}

bool nr_modulation_layer_mapping(const uint32_t *in,
                                 uint32_t length,
                                 uint16_t mod_order,
                                 uint8_t n_layers,
                                 int layerSz,
                                 c16_t tx_layers[][layerSz])
{
  if (n_layers < 1 || n_layers > 4)
    return false;

  const uint32_t n_symbs = length / mod_order;
  if ((n_symbs % n_layers) != 0)
    return false;

  const uint8_t *in_bytes = (const uint8_t *)in;

  switch (mod_order) {
    case 2: {
      const c16_t *nr_mod_table = nr_qpsk_mod_table;
      if (n_layers == 1) {
        nr_modulation(in, length, mod_order, (int16_t *)tx_layers[0]);
        return true;
      }

      for (uint32_t sym = 0, layer_sym = 0; sym < n_symbs; sym += n_layers, layer_sym++) {
        for (uint8_t layer = 0; layer < n_layers; layer++) {
          const uint8_t idx = get_packed_symbol(in_bytes, length, mod_order, sym + layer);
          tx_layers[layer][layer_sym] = nr_mod_table[idx];
        }
      }
      return true;
    }

    case 4: {
      const int32_t *nr_mod_table = nr_16qam_mod_table;
      if (n_layers == 1) {
        nr_modulation(in, length, mod_order, (int16_t *)tx_layers[0]);
        return true;
      }

      for (uint32_t sym = 0, layer_sym = 0; sym < n_symbs; sym += n_layers, layer_sym++) {
        for (uint8_t layer = 0; layer < n_layers; layer++) {
          const uint8_t idx = get_packed_symbol(in_bytes, length, mod_order, sym + layer);
          ((int32_t *)tx_layers[layer])[layer_sym] = nr_mod_table[idx];
        }
      }
      return true;
    }

    case 6: {
      const c16_t *nr_mod_table = (const c16_t *)nr_64qam_mod_table;
      if (n_layers == 1) {
        c16_t *tx0 = tx_layers[0];
        uint32_t sym = 0;
        for (; sym + 2 <= n_symbs; sym += 2) {
          const uint16_t idx = get_packed_bits(in_bytes, length, sym * mod_order, 12);
          tx0[sym] = nr_mod_table[idx * 2];
          tx0[sym + 1] = nr_mod_table[idx * 2 + 1];
        }
        if (sym < n_symbs) {
          const uint8_t idx = get_packed_symbol(in_bytes, length, mod_order, sym);
          tx0[sym] = nr_mod_table[idx * 2];
        }
        return true;
      }

      if (n_layers == 2) {
        c16_t *tx0 = tx_layers[0];
        c16_t *tx1 = tx_layers[1];
        for (uint32_t sym = 0, layer_sym = 0; sym < n_symbs; sym += 2, layer_sym++) {
          const uint16_t idx = get_packed_bits(in_bytes, length, sym * mod_order, 12);
          tx0[layer_sym] = nr_mod_table[idx * 2];
          tx1[layer_sym] = nr_mod_table[idx * 2 + 1];
        }
        return true;
      }

      if (n_layers == 3) {
        c16_t *tx0 = tx_layers[0];
        c16_t *tx1 = tx_layers[1];
        c16_t *tx2 = tx_layers[2];
        uint32_t sym = 0;
        uint32_t layer_sym = 0;
        for (; sym + 6 <= n_symbs; sym += 6, layer_sym += 2) {
          const uint64_t bits = get_packed_bits(in_bytes, length, sym * mod_order, 36);
          const uint16_t idx0 = bits & 0xfff;
          const uint16_t idx1 = (bits >> 12) & 0xfff;
          const uint16_t idx2 = (bits >> 24) & 0xfff;
          tx0[layer_sym] = nr_mod_table[idx0 * 2];
          tx1[layer_sym] = nr_mod_table[idx0 * 2 + 1];
          tx2[layer_sym] = nr_mod_table[idx1 * 2];
          tx0[layer_sym + 1] = nr_mod_table[idx1 * 2 + 1];
          tx1[layer_sym + 1] = nr_mod_table[idx2 * 2];
          tx2[layer_sym + 1] = nr_mod_table[idx2 * 2 + 1];
        }
        if (sym < n_symbs) {
          for (uint8_t layer = 0; layer < 3; layer++) {
            const uint8_t idx = get_packed_symbol(in_bytes, length, mod_order, sym + layer);
            tx_layers[layer][layer_sym] = nr_mod_table[idx * 2];
          }
        }
        return true;
      }

      if (n_layers == 4) {
        c16_t *tx0 = tx_layers[0];
        c16_t *tx1 = tx_layers[1];
        c16_t *tx2 = tx_layers[2];
        c16_t *tx3 = tx_layers[3];
        for (uint32_t sym = 0, layer_sym = 0; sym < n_symbs; sym += 4, layer_sym++) {
          const uint32_t bits = get_packed_bits(in_bytes, length, sym * mod_order, 24);
          const uint16_t idx0 = bits & 0xfff;
          const uint16_t idx1 = (bits >> 12) & 0xfff;
          tx0[layer_sym] = nr_mod_table[idx0 * 2];
          tx1[layer_sym] = nr_mod_table[idx0 * 2 + 1];
          tx2[layer_sym] = nr_mod_table[idx1 * 2];
          tx3[layer_sym] = nr_mod_table[idx1 * 2 + 1];
        }
        return true;
      }
      return false;
    }

    case 8: {
      const int32_t *nr_mod_table = nr_256qam_mod_table;
      if (n_layers == 1) {
        nr_modulation(in, length, mod_order, (int16_t *)tx_layers[0]);
        return true;
      }

      for (uint32_t sym = 0, layer_sym = 0; sym < n_symbs; sym += n_layers, layer_sym++) {
        for (uint8_t layer = 0; layer < n_layers; layer++) {
          const uint8_t idx = get_packed_symbol(in_bytes, length, mod_order, sym + layer);
          ((int32_t *)tx_layers[layer])[layer_sym] = nr_mod_table[idx];
        }
      }
      return true;
    }

    default:
      return false;
  }
}

void nr_layer_mapping(int nbCodes,
                      int encoded_len,
                      c16_t mod_symbs[nbCodes][encoded_len],
                      uint8_t n_layers,
                      int layerSz,
                      uint32_t n_symbs,
                      c16_t tx_layers[][layerSz])
{
  LOG_D(PHY, "Doing layer mapping for %d layers, %d symbols\n", n_layers, n_symbs);
  c16_t *mod = mod_symbs[0];
  switch (n_layers) {
    case 1:
      memcpy(tx_layers[0], mod, n_symbs * sizeof(**mod_symbs));
      break;

    case 2: {
      int i = 0;
      c16_t *tx0 = tx_layers[0];
      c16_t *tx1 = tx_layers[1];
#if defined(__AVX512BW__)
      simde__m512i perm2a = simde_mm512_set_epi32(30, 28, 26, 24, 22, 20, 18, 16, 14, 12, 10, 8, 6, 4, 2, 0);
      simde__m512i perm2b = simde_mm512_set_epi32(31, 29, 27, 25, 23, 21, 19, 17, 15, 13, 11, 9, 7, 5, 3, 1);
      for (; i < (n_symbs & ~31); i += 32) {
        simde__m512i a = *(simde__m512i *)(mod + i);
        simde__m512i b = *(simde__m512i *)(mod + i + 16);
        *(simde__m512i *)tx0 = simde_mm512_permutex2var_epi32(a, perm2a, b);
        *(simde__m512i *)tx1 = simde_mm512_permutex2var_epi32(a, perm2b, b);
        tx0 += 16;
        tx1 += 16;
      }
#endif
#ifdef __AVX2__
      simde__m256i perm2 = simde_mm256_set_epi32(7, 5, 3, 1, 6, 4, 2, 0);
      for (; i < (n_symbs & ~7); i += 8) {
        simde__m256i d = simde_mm256_permutevar8x32_epi32(*(simde__m256i *)(mod + i), perm2);
        *(simde__m128i *)tx0 = simde_mm256_extractf128_si256(d, 0);
        *(simde__m128i *)tx1 = simde_mm256_extractf128_si256(d, 1);
        tx0 += 4;
        tx1 += 4;
      }
#endif
#if defined(__aarch64__) && defined(USE_NEON)
      for (; i < (n_symbs & ~7); i += 8) {
        uint32x4x2_t d = vld2q_u32((const uint32_t *)(mod + i));
        vst1q_u32((uint32_t *)tx0, d.val[0]);
        vst1q_u32((uint32_t *)tx1, d.val[1]);
        tx0 += 4;
        tx1 += 4;
      }
#endif
      for (; i < n_symbs; i += 2) {
        *tx0++ = mod[i];
        *tx1++ = mod[i + 1];
      }
    } break;
    case 3: {
      int i = 0;
      c16_t *tx0 = tx_layers[0];
      c16_t *tx1 = tx_layers[1];
      c16_t *tx2 = tx_layers[2];
#if defined(__AVX512F) && defined(__AVX512VBMI__)
      simde__m512i perm3_0 = simde_mm512_set_epi32(13 + 16,
                                                   10 + 16,
                                                   7 + 16,
                                                   4 + 16,
                                                   1 + 16,
                                                   14 + 16,
                                                   11 + 16,
                                                   8 + 16,
                                                   5 + 16,
                                                   2 + 16,
                                                   15,
                                                   12,
                                                   9,
                                                   6,
                                                   3,
                                                   0);
      simde__m512i perm3_0b = simde_mm512_set_epi32(13 + 16, 10 + 16, 7 + 16, 4 + 16, 1 + 16, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
      simde__m512i perm3_1 = simde_mm512_set_epi32(14 + 16,
                                                   11 + 16,
                                                   8 + 16,
                                                   5 + 16,
                                                   2 + 16,
                                                   15 + 16,
                                                   12 + 16,
                                                   9 + 16,
                                                   6 + 16,
                                                   3 + 16,
                                                   0 + 16,
                                                   13,
                                                   10,
                                                   7,
                                                   4,
                                                   1);
      simde__m512i perm3_1b = simde_mm512_set_epi32(14 + 16, 11 + 16, 8 + 16, 5 + 16, 2 + 16, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
      simde__m512i perm3_2 = simde_mm512_set_epi32(15 + 16,
                                                   12 + 16,
                                                   9 + 16,
                                                   6 + 16,
                                                   3 + 16,
                                                   0 + 16,
                                                   13 + 16,
                                                   10 + 16,
                                                   7 + 16,
                                                   4 + 16,
                                                   1 + 16,
                                                   14,
                                                   11,
                                                   8,
                                                   5,
                                                   2);
      simde__m512i perm3_2b = simde_mm512_set_epi32(15 + 16, 12 + 16, 9 + 16, 6 + 16, 3 + 16, 0 + 16, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
      for (; i < (n_symbs & ~63); i += 48) {
        simde__m512i i0 = *(simde__m512i *)(mod + i);
        simde__m512i i1 = *(simde__m512i *)(mod + i + 16);
        simde__m512i i2 = *(simde__m512i *)(mod + i + 32);
        simde__m512i d0 = simde_mm512_permutex2var_epi32(i0, perm3_0, i1);
        *(simde__m512i *)tx0 = simde_mm512_permutex2var_epi32(d0, perm3_0b, i2); // 11000000
        tx0 += 16;
        d0 = simde_mm512_permutex2var_epi32(i0, perm3_1, i1);
        *(simde__m512i *)tx1 = simde_mm512_permutex2var_epi32(d0, perm3_1b, i2); // 11000000
        tx1 += 16;
        d0 = simde_mm512_permutex2var_epi32(i0, perm3_2, i1);
        *(simde__m512i *)tx2 = simde_mm512_permutex2var_epi32(d0, perm3_2b, i2); // 11000000
        tx2 += 16;
      }
#endif
#ifdef __AVX2__
      {
        simde__m256i perm3_0 = simde_mm256_set_epi32(5, 2, 7, 4, 1, 6, 3, 0);
        simde__m256i perm3_1 = simde_mm256_set_epi32(6, 3, 0, 5, 2, 7, 4, 1);
        simde__m256i perm3_2 = simde_mm256_set_epi32(7, 4, 1, 6, 3, 0, 5, 2);
        for (; i < (n_symbs & ~31); i += 24) {
          simde__m256i i0 = *(simde__m256i *)(mod + i);
          simde__m256i i1 = *(simde__m256i *)(mod + i + 8);
          simde__m256i i2 = *(simde__m256i *)(mod + i + 16);
          simde__m256i d0 = simde_mm256_permutevar8x32_epi32(i0, perm3_0);
          simde__m256i d1 = simde_mm256_permutevar8x32_epi32(i1, perm3_0);
          simde__m256i d2 = simde_mm256_permutevar8x32_epi32(i2, perm3_0);
          simde__m256i d3 = simde_mm256_blend_epi32(d0, d1, 0x38); // 00111000
          *(simde__m256i *)tx0 = simde_mm256_blend_epi32(d3, d2, 0xc0); // 11000000
          tx0 += 8;
          d0 = simde_mm256_permutevar8x32_epi32(i0, perm3_1);
          d1 = simde_mm256_permutevar8x32_epi32(i1, perm3_1);
          d2 = simde_mm256_permutevar8x32_epi32(i2, perm3_1);
          d3 = simde_mm256_blend_epi32(d0, d1, 0x18); // 00011000
          *(simde__m256i *)tx1 = simde_mm256_blend_epi32(d3, d2, 0xe0); // 11100000
          tx1 += 8;
          d0 = simde_mm256_permutevar8x32_epi32(i0, perm3_2);
          d1 = simde_mm256_permutevar8x32_epi32(i1, perm3_2);
          d2 = simde_mm256_permutevar8x32_epi32(i2, perm3_2);
          d3 = simde_mm256_blend_epi32(d0, d1, 0x1c); // 00011100
          *(simde__m256i *)tx2 = simde_mm256_blend_epi32(d3, d2, 0xe0); // 11100000
          tx2 += 8;
        }
      }
#endif
#if defined(__aarch64__) && defined(USE_NEON)
      for (; i < (n_symbs & ~11); i += 12) {
        uint32x4x3_t d = vld3q_u32((const uint32_t *)(mod + i));
        vst1q_u32((uint32_t *)tx0, d.val[0]);
        vst1q_u32((uint32_t *)tx1, d.val[1]);
        vst1q_u32((uint32_t *)tx2, d.val[2]);
        tx0 += 4;
        tx1 += 4;
        tx2 += 4;
      }
#endif
      for (; i < n_symbs; i += 3) {
        *tx0++ = mod[i];
        *tx1++ = mod[i + 1];
        *tx2++ = mod[i + 2];
      }

#ifdef DEBUG_LAYER_MAPPING
      printf("\nsymb %d/%u\n", i << 3, n_symbs);
      printf(" layer 0:\t");
      for (int j = 0; j < 8 * 6; j += 6) {
        printf("%d %d ", ((int16_t *)&mod[i << 3])[j], ((int16_t *)&mod[i << 3])[j + 1]);
      }
      printf("\n layer 1:\t");
      for (int j = 2; j < 8 * 6; j += 6) {
        printf("%d %d ", ((int16_t *)&mod[i << 3])[j], ((int16_t *)&mod[i << 3])[j + 1]);
      }
      printf("\n layer 2:\t");
      for (int j = 4; j < 8 * 6; j += 6) {
        printf("%d %d ", ((int16_t *)&mod[i << 3])[j], ((int16_t *)&mod[i << 3])[j + 1]);
      }
      printf("\n Mapping layer 0:\t");
      for (int j = 0; j < 16; j++) {
        printf("%d ", ((int16_t *)&tx_layers[0][n << 3])[j]);
      }
      printf("\n Mapping layer 1:\t");
      for (int j = 0; j < 16; j++) {
        printf("%d ", ((int16_t *)&tx_layers[1][n << 3])[j]);
      }
      printf("\n Mapping layer 2:\t");
      for (int j = 0; j < 16; j++) {
        printf("%d ", ((int16_t *)&tx_layers[2][n << 3])[j]);
      }
#endif
    } break;

    case 4: {
      int i = 0;
      c16_t *tx0 = tx_layers[0];
      c16_t *tx1 = tx_layers[1];
      c16_t *tx2 = tx_layers[2];
      c16_t *tx3 = tx_layers[3];
#if defined(__AVX512VBMI__)
      simde__m512i perm4 = simde_mm512_set_epi32(15, 11, 7, 3, 14, 10, 6, 2, 13, 9, 5, 1, 12, 8, 4, 0);
      for (; i < (n_symbs & ~15); i += 16) {
        simde__m512i e = simde_mm512_permutexvar_epi32(perm4, *(simde__m512i *)(mod + i));
        *(simde__m128i *)tx0 = simde_mm512_extracti64x2_epi64(e, 0);
        tx0 += 4;
        *(simde__m128i *)tx1 = simde_mm512_extracti64x2_epi64(e, 1);
        tx1 += 4;
        *(simde__m128i *)tx2 = simde_mm512_extracti64x2_epi64(e, 2);
        tx2 += 4;
        *(simde__m128i *)tx3 = simde_mm512_extracti64x2_epi64(e, 3);
        tx3 += 4;
      }
#endif
#ifdef __AVX2__
      {
        simde__m256i perm4 = simde_mm256_set_epi32(7, 3, 6, 2, 5, 1, 4, 0);
        for (; i < (n_symbs & ~7); i += 8) {
          simde__m256i e = simde_mm256_permutevar8x32_epi32(*(simde__m256i *)(mod + i), perm4);
          *(uint64_t *)tx0 = simde_mm256_extract_epi64(e, 0);
          tx0 += 2;
          *(uint64_t *)tx1 = simde_mm256_extract_epi64(e, 1);
          tx1 += 2;
          *(uint64_t *)tx2 = simde_mm256_extract_epi64(e, 2);
          tx2 += 2;
          *(uint64_t *)tx3 = simde_mm256_extract_epi64(e, 3);
          tx3 += 2;
        }
      }
#endif
#if defined(__aarch64__) && defined(USE_NEON)
      for (; i < (n_symbs & ~15); i += 16) {
        uint32x4x4_t d = vld4q_u32((const uint32_t *)(mod + i));
        vst1q_u32((uint32_t *)tx0, d.val[0]);
        vst1q_u32((uint32_t *)tx1, d.val[1]);
        vst1q_u32((uint32_t *)tx2, d.val[2]);
        vst1q_u32((uint32_t *)tx3, d.val[3]);
        tx0 += 4;
        tx1 += 4;
        tx2 += 4;
        tx3 += 4;
      }
#endif
      for (; i < n_symbs; i += 4) {
        *tx0++ = mod[i];
        *tx1++ = mod[i + 1];
        *tx2++ = mod[i + 2];
        *tx3++ = mod[i + 3];
      }
    } break;

    case 5:
    case 6:
    case 7:
    case 8:
      /*
      // Layer 0,1
      for (int i = 0; i < n_symbs; i += 2) {
const int txIdx = i / 2;
tx_layer[0][txIdx] = mod_symbs[0][i];
tx_layer[1][txIdx] = mod_symbs[0][i + 1];
      }
      // layers 2,3,4
      else
for (int i = 0; i < n_symbs; i += 3) {
const int txIdx = i / 3;
tx_layer[2][txIdx] = mod_symbs[1][i + 2];
tx_layer[3][txIdx] = mod_symbs[1][i + 3];
tx_layer[4][txIdx] = mod_symbs[1][i + 4];
}
      break;

case 6:
      for (int q=0; q<2; q++)
for (int i = 0; i < n_symbs; i += 3) {
const int txIdx = i / 3;
tx_layer[0][txIdx] = mod_symbs[q][i + layer];
tx_layer[1][txIdx] = mod_symbs[q][i + layer];
tx_layer[2][txIdx] = mod_symbs[q][i + layer];
tx_layer[3][txIdx] = mod_symbs[q][i + layer];
tx_layer[4][txIdx] = mod_symbs[q][i + layer];
tx_layer[5][txIdx] = mod_symbs[q][i + layer];
}
      break;

case 7:
      if (layer < 3)
for (int i = 0; i < n_symbs; i += 3) {
const int txIdx = i / 3;
tx_layer[txIdx] = mod_symbs[1][i + layer];
}
      else
for (int i = 0; i < n_symbs; i += 4) {
const int txIdx = i / 4;
tx_layer[txIdx] = mod_symbs[0][i + layer];
}
      break;

case 8:
      for (int q=0; q<2; q++)
      for (int i = 0; i < n_symbs; i += 4) {
const int txIdx = i / 4;
tx_layer[txIdx] = mod_symbs[q][i + layer];
      }
      break;
*/
    default:
      AssertFatal(0, "Invalid number of layers %d\n", n_layers);
  }
}

void nr_ue_layer_mapping(const c16_t *mod_symbs, const int n_layers, const int n_symbs, c16_t tx_layers[][n_symbs])
{
  for (int l = 0; l < n_layers; l++) {
    for (int i = 0; i < n_symbs; i++) {
      tx_layers[l][i] = c16mulRealShift(mod_symbs[n_layers * i + l], AMP, 15);
    }
  }
}
#if defined(__aarch64__)

void nr_dft(c16_t *z, c16_t *d, uint32_t Msc_PUSCH)
{
  simde__m128i dft_in128[3240];
  simde__m128i dft_out128[3240];

  c16_t *dft_in0 = (c16_t *)dft_in128;
  c16_t *dft_out0 = (c16_t *)dft_out128;

  uint32_t i;
  uint32_t ip;
  simde__m128i norm128;

  if ((Msc_PUSCH % 1536) > 0) {
    for (i = 0, ip = 0; i < Msc_PUSCH; i++, ip += 4)
      dft_in0[ip] = d[i];
  }

  const dft_size_idx_t dftsize = get_dft(Msc_PUSCH);

  switch (Msc_PUSCH) {
    case 12:
      dft(dftsize, (int16_t *)dft_in0, (int16_t *)dft_out0, 0);

      norm128 = simde_mm_set1_epi16(9459);

      for (i = 0; i < 12; i++) {
        ((simde__m128i *)dft_out0)[i] = simde_mm_slli_epi16(simde_mm_mulhi_epi16(((simde__m128i *)dft_out0)[i], norm128), 1);
      }
      break;

    default:
      dft(dftsize, (int16_t *)dft_in0, (int16_t *)dft_out0, 1);
      break;
  }

  if ((Msc_PUSCH % 1536) > 0) {
    for (i = 0, ip = 0; i < Msc_PUSCH; i++, ip += 4)
      z[i] = dft_out0[ip];
  }
}

#else

void nr_dft(c16_t *output, c16_t *input, uint32_t Msc_PUSCH)
{
  const dft_size_idx_t size = get_dft(Msc_PUSCH);

  dft(size,
      (int16_t *)input,
      (int16_t *)output,
      1);
}

#endif

void perform_symbol_rotation(const int nsymb, const int numerology_index, double f0, c16_t *symbol_rotation)
{
  const double Tc = (1 / 480e3 / 4096);
  const double Nu = 2048 * 64 * (1 / (float)(1 << numerology_index));
  const double Ncp0 = 16 * 64 + (144 * 64 * (1 / (float)(1 << numerology_index)));
  const double Ncp1 = (144 * 64 * (1 / (float)(1 << numerology_index)));

  LOG_D(PHY, "Doing symbol rotation calculation for TX/RX, f0 %f Hz, Nsymb %d\n", f0, nsymb);

  double tl = 0.0;
  double poff = 0.0;
  double exp_re = 0.0;
  double exp_im = 0.0;

  for (int l = 0; l < nsymb; l++) {
    double Ncp;
    if (l == 0 || l == (7 * (1 << numerology_index))) {
      Ncp = Ncp0;
    } else {
      Ncp = Ncp1;
    }

    poff = 2 * M_PI * (tl + (Ncp * Tc)) * f0;
    exp_re = cos(poff);
    exp_im = sin(-poff);
    symbol_rotation[l].r = (int16_t)floor(exp_re * 32767);
    symbol_rotation[l].i = (int16_t)floor(exp_im * 32767);

    LOG_D(PHY,
          "Symbol rotation %d/%d => tl %f (%d,%d) (%f)\n",
          l,
          nsymb,
          tl,
          symbol_rotation[l].r,
          symbol_rotation[l].i,
          (poff / 2 / M_PI) - floor(poff / 2 / M_PI));

    tl += (Nu + Ncp) * Tc;
  }
}

void init_symbol_rotation(NR_DL_FRAME_PARMS *fp)
{
  double f[2] = {(double)fp->dl_CarrierFreq, (double)fp->ul_CarrierFreq};

  for (int ll = 0; ll < 2; ll++) {
    double f0 = f[ll];
    if (f0 == 0)
      continue;
    c16_t *rot = fp->symbol_rotation[ll];
    perform_symbol_rotation(fp->symbols_per_slot * fp->slots_per_frame / 10, fp->numerology_index, f0, rot);
  }
}

void init_timeshift_rotation(const int ofdm_symbol_size,
                             const int nb_prefix_samples,
                             const uint ofdm_offset_divisor,
                             c16_t *timeshift_symbol_rotation)
{
  const int sample_offset = nb_prefix_samples / ofdm_offset_divisor;
  for (int i = 0; i < ofdm_symbol_size; i++) {
    double poff = -i * 2.0 * M_PI * sample_offset / ofdm_symbol_size;
    double exp_re = cos(poff);
    double exp_im = sin(-poff);
    timeshift_symbol_rotation[i].r = (int16_t)round(exp_re * 32767);
    timeshift_symbol_rotation[i].i = (int16_t)round(exp_im * 32767);

    if (i < 10)
      LOG_D(PHY,
            "Timeshift symbol rotation %d => (%d,%d) %f\n",
            i,
            timeshift_symbol_rotation[i].r,
            timeshift_symbol_rotation[i].i,
            poff);
  }
}

c16_t nr_layer_precoder(int sz, c16_t datatx_F_precoding[][sz], const char *prec_matrix, uint8_t n_layers, int32_t re_offset)
{
  c16_t precodatatx_F = {0};

  for (int al = 0; al < n_layers; al++) {
    c16_t antenna = datatx_F_precoding[al][re_offset];
    switch (prec_matrix[al]) {
      case '0': // multiply by zero
        break;

      case '1': // multiply by 1
        precodatatx_F = c16add(precodatatx_F, antenna);
        break;

      case 'n': // multiply by -1
        precodatatx_F = c16sub(precodatatx_F, antenna);
        break;

      case 'j': //
        precodatatx_F.r -= antenna.i;
        precodatatx_F.i += antenna.r;
        break;

      case 'o': // -j
        precodatatx_F.r += antenna.i;
        precodatatx_F.i -= antenna.r;
        break;
    }
  }

  return precodatatx_F;
  // normalize
  /*  ((int16_t *)precodatatx_F)[0] = (int16_t)((((int16_t *)precodatatx_F)[0]*ONE_OVER_SQRT2_Q15)>>15);
      ((int16_t *)precodatatx_F)[1] = (int16_t)((((int16_t *)precodatatx_F)[1]*ONE_OVER_SQRT2_Q15)>>15);*/
}

c16_t nr_layer_precoder_cm(int n_layers,
                           int symSz,
                           c16_t datatx_F_precoding[n_layers][symSz],
                           int ap,
                           c16_t weights[NR_MAX_NB_LAYERS][NR_MAX_CSI_PORTS],
                           int offset)
{
  c16_t precodatatx_F = {0};
  for (int al = 0; al < n_layers; al++) {
    c16_t prec_weight = weights[al][ap];
    precodatatx_F = c16maddShift(datatx_F_precoding[al][offset], prec_weight, precodatatx_F, 15);
  }
  return precodatatx_F;
}

#if defined(__AVX512F__) && defined(__AVX512BW__)

static inline __attribute__((always_inline)) __m512i cmac0_prec512(__m512i x, __m512i w_c, __m512i w_s) {

      // Multiplication and shift
      const __m512i reals =
          _mm512_srai_epi32(_mm512_madd_epi16(x, w_c), 15); // (int32_t) .r = (x.r * w.r - x.i * w.i) >> 15
      const __m512i imags =
          _mm512_slli_epi32(_mm512_madd_epi16(x, w_s),  1); // (int32_t) .i = (x.r * w.i + x.i * w.r) << 1, since higher 16 bit of each 32 bit is taken by blend_epi16

      // Re-arrange to match c16_t format
      return _mm512_mask_blend_epi16(0xAAAAAAAA,reals, imags);

}
static inline __attribute__((always_inline)) __m512i cmac_prec512(__m512i y, __m512i x, __m512i w_c, __m512i w_s) {
  const __m512i produ = cmac0_prec512(x, w_c, w_s);
  // Accumulate the product
  return _mm512_adds_epi16(y, produ);
}
#endif
#ifdef __AVX2__
static inline __attribute__((always_inline)) __m256i cmac0_prec256(__m256i x, __m256i w_c, __m256i w_s) {

      // Multiplication and shift
      const __m256i reals =
          _mm256_srai_epi32(_mm256_madd_epi16(x, w_c), 15); // (int32_t) .r = (x.r * w.r - x.i * w.i) >> 15
      const __m256i imags =
          _mm256_slli_epi32(_mm256_madd_epi16(x, w_s),  1); // (int32_t) .i = (x.r * w.i + x.i * w.r) << 1, since higher 16 bit of each 32 bit is taken by blend_epi16

      // Re-arrange to match c16_t format
      return _mm256_blend_epi16(reals, imags,0xAA);

}
static inline __attribute__((always_inline)) __m256i cmac_prec256(__m256i y, __m256i x, __m256i w_c, __m256i w_s) {
  const __m256i produ = cmac0_prec256(x, w_c, w_s);
  // Accumulate the product
  return _mm256_adds_epi16(y, produ);
}
#endif
#ifdef __aarch64__
#ifdef NEWOPTIM
static inline __attribute__((always_inline)) int16x4x2_t cmac0_prec4(int16x8_t x, int16x4_t wr, int16x4_t wi) {
    const int16x4_t x_lo = vget_low_s16(x);
    const int16x4_t x_hi = vget_high_s16(x);
    const int16x4_t xr = vuzp1_s16(x_lo, x_hi);
    const int16x4_t xi = vuzp2_s16(x_lo, x_hi);
#else
static inline __attribute__((always_inline)) int16x8_t cmac0_prec128(int16x8_t x, int16x8_t wr, int16x8_t wi) {
    //
    int16x8_t xr = vuzp1q_s16(x, x);  // even lanes
    int16x8_t xi = vuzp2q_s16(x, x);  // odd  lanes
#endif
#ifdef __ARM_FEATURE_QRDMX
    // ARMv8.1-A: Use RDM instructions
#ifdef NEWOPTIM
    // real = ar*br - ai*bi  (Q15 scaling via high-half doubling muls)
    int16x4_t real = vqdmulh_s16(xr, wr);      // ≈ round((2*xr*wr)/2^16)
    real = vqrdmlsh_s16(real, xi, wi);         // real -= round((2*xi*wi)/2^16)
    // imag = ar*bi + ai*br
    int16x4_t imag = vqdmulh_s16(xr, wi);
    imag = vqrdmlah_s16(imag, xi, wr);         // imag += round((2*xi*wr)/2^16)
#else
    // real = ar*br - ai*bi  (Q15 scaling via high-half doubling muls)
    int16x8_t real = vqdmulhq_s16(xr, wr);      // ≈ round((2*xr*wr)/2^16)
    real = vqrdmlshq_s16(real, xi, wi);         // real -= round((2*xi*wi)/2^16)
    // imag = ar*bi + ai*br
    int16x8_t imag = vqdmulhq_s16(xr, wi);
    imag = vqrdmlahq_s16(imag, xi, wr);         // imag += round((2*xi*wr)/2^16)
#endif
#else
    // ARMv8.0-A fallback: Use standard 32-bit multiply
#ifdef NEWOPTIM 
    int32x4_t real_prod = vmull_s16(xr, wr);
    real_prod = vmlsl_s16(real_prod, xi, wi);

    int32x4_t imag_prod = vmull_s16(xr, wi);
    imag_prod = vmlal_s16(imag_prod, xi, wr);

    int16x4_t real = vqrshrn_n_s32(real_prod, 15);
    int16x4_t imag = vqrshrn_n_s32(imag_prod, 15);
#else
    int32x4_t real_lo = vmull_s16(vget_low_s16(xr), vget_low_s16(wr));
    int32x4_t real_hi = vmull_s16(vget_high_s16(xr), vget_high_s16(wr));
    real_lo = vmlsl_s16(real_lo, vget_low_s16(xi), vget_low_s16(wi));
    real_hi = vmlsl_s16(real_hi, vget_high_s16(xi), vget_high_s16(wi));

    int32x4_t imag_lo = vmull_s16(vget_low_s16(xr), vget_low_s16(wi));
    int32x4_t imag_hi = vmull_s16(vget_high_s16(xr), vget_high_s16(wi));
    imag_lo = vmlal_s16(imag_lo, vget_low_s16(xi), vget_low_s16(wr));
    imag_hi = vmlal_s16(imag_hi, vget_high_s16(xi), vget_high_s16(wr));

    int16x8_t real = vcombine_s16(vqrshrn_n_s32(real_lo, 15), vqrshrn_n_s32(real_hi, 15));
    int16x8_t imag = vcombine_s16(vqrshrn_n_s32(imag_lo, 15), vqrshrn_n_s32(imag_hi, 15));
#endif
#endif
#ifdef NEWOPTIM
    int16x4x2_t produ;
    produ.val[0] = real;
    produ.val[1] = imag;
    return produ;
#else
    // Re-interleave [real, imag]
    int16x8x2_t produ = vzipq_s16(real, imag);
    return produ.val[0];
#endif    
}
#ifdef NEWOPTIM
static inline __attribute__((always_inline)) int16x4x2_t cmac_prec4(int16x4x2_t y, int16x8_t x, int16x4_t wr, int16x4_t wi) {
  const int16x4x2_t produ = cmac0_prec4(x, wr, wi);
  y.val[0] = vadd_s16(y.val[0], produ.val[0]);
  y.val[1] = vadd_s16(y.val[1], produ.val[1]);
  return y;
}
#else
static inline __attribute__((always_inline)) int16x8_t cmac_prec128(int16x8_t y, int16x8_t x, int16x8_t wr, int16x8_t wi) {
  int16x8_t produ = cmac0_prec128(x, wr, wi);
  return vaddq_s16(y, produ);
}
#endif

#else // __x86 128-bit
static inline __attribute__((always_inline)) simde__m128i cmac0_prec128(simde__m128i x, simde__m128i w_c, simde__m128i w_s)
{
  // Multiplication and shift
  const simde__m128i reals = simde_mm_srai_epi32(simde_mm_madd_epi16(x, w_c), 15); // (int32_t) .r = (x.r * w.r - x.i * w.i) >> 15
  const simde__m128i imags = simde_mm_slli_epi32(
      simde_mm_madd_epi16(x, w_s),
      1); // (int32_t) .i = (x.r * w.i + x.i * w.r) << 1, since higher 16 bit of each 32 bit is taken by blend_epi16

  /* Re-arrange to match c16_t format
     bit index: 0            | 16              | 32           | 48              | 64           | 80              | 96 | 112
     reals =   {R0.r[15..30] | R0.r[31] (0)*15 | R1.r[15..30] | R1.r[31] (0)*15 | R2.r[15..30] | R2.r[31] (0)*15 | R3.r[15..30]
     | R3.r[31] (0)*15} imags =   {0 R0.i[0..14]| R0.i[15..30]    | 0 R1.i[0..14]| R1.i[15..30]    | 0 R2.i[0..14]| R2.i[15..30]
     | 0 R3.i[0..14]| R3.i[15..30]   } 16b from  {reals        | imags           | reals        | imags | reals | imags | reals
     | imags          } produ =   {R0.r[15..30] | R0.i[15..30]    | R1.r[15..30] | R1.i[15..30] | R2.r[15..30] | R2.i[15..30] |
     R3.r[15..30] | R3.i[15..30]   }
  */
  return simde_mm_blend_epi16(reals, imags, 0xAA);
}
static inline __attribute__((always_inline)) __m128i cmac_prec128(__m128i y, __m128i x, __m128i w_c, __m128i w_s)
{
  const __m128i produ = cmac0_prec128(x, w_c, w_s);
  // Accumulate the product
  return simde_mm_adds_epi16(y, produ);
}
#endif

#define load_consts(Type, Instruct, Rank)                                          \
  const Type w_c##Rank = Instruct(c16toI32(c16conj(weights[Rank][ant]))); \
  const Type w_s##Rank = Instruct(c16toI32(c16swap(weights[Rank][ant]))); \
  const Type *in##Rank = (Type *)(txdataF_res_mapped[Rank] + sc_offset + (out-beginning));
#if defined(NEWOPTIM) && defined(__aarch64__)
#define load_consts_arm(Rank) \
  const int16x4_t wr##Rank = vdup_n_s16(weights[Rank][ant].r); \
  const int16x4_t wi##Rank = vdup_n_s16(weights[Rank][ant].i); \
  const int16x8_t *in##Rank = (const int16x8_t *)(txdataF_res_mapped[Rank] + sc_offset + (out - beginning));
#endif

void nr_layer_precoder_simd(const int n_layers,
                            const int symSz,
                            const c16_t txdataF_res_mapped[n_layers][symSz],
                            const int ant,
                            c16_t weights[NR_MAX_NB_LAYERS][NR_MAX_CSI_PORTS],
                            const int sc_offset,
                            const int re_cnt,
                            c16_t *txdataF_precoded)
{
  // For x86, use 256 SIMD for every 8 RE and 128 SIMD for last 4 RE
  // For aarch64, use 128 SIMD for every 4 RE
  AssertFatal(n_layers > 0 && n_layers <= 4, "Shouldn't get here, n_layers %d\n", n_layers);

  // 512/256 SIMD: Do 16/8 RE in one iteration, 3 iterations for 2 RB
  c16_t *beginning = txdataF_precoded + sc_offset;
  c16_t *out=beginning;
#if defined(__AVX512F__) && defined(__AVX512BW__)
  {
    c16_t *end = out + (re_cnt & ~15);
    load_consts(__m512i, _mm512_set1_epi32, 0);
    if (n_layers == 1) {
      for (; out < end; out += sizeof(__m512i) / sizeof(*out)) {
        const __m512i x = _mm512_loadu_si512(in0++);
        // Matrix multiplication for 4 elements of the result (sizeof(simde__m256i) / sizeof(*prec_matrix) = 8)
        __m512i y = cmac0_prec512(x, w_c0, w_s0);
        _mm512_storeu_si512(out, y);
      }
    } else if (n_layers == 2) {
      load_consts(__m512i, _mm512_set1_epi32, 1);
      for (; out < end; out += sizeof(__m512i) / sizeof(*out)) {
        const __m512i x = _mm512_loadu_si512(in0++);
        const __m512i x1 = _mm512_loadu_si512(in1++);
        // Matrix multiplication for 4 elements of the result (sizeof(simde__m256i) / sizeof(*prec_matrix) = 8)
        __m512i y = cmac0_prec512(x, w_c0, w_s0);
        y = cmac_prec512(y, x1, w_c1, w_s1);
        _mm512_storeu_si512(out, y);
      }
    } else if (n_layers == 3) {
      load_consts(__m512i, _mm512_set1_epi32, 1);
      load_consts(__m512i, _mm512_set1_epi32, 2);
      for (; out < end; out += sizeof(__m512i) / sizeof(*out)) {
        const __m512i x = _mm512_loadu_si512(in0++);
        const __m512i x1 = _mm512_loadu_si512(in1++);
        const __m512i x2 = _mm512_loadu_si512(in2++);
        // Matrix multiplication for 4 elements of the result (sizeof(simde__m256i) / sizeof(*prec_matrix) = 8)
        __m512i y = cmac0_prec512(x, w_c0, w_s0);
        y = cmac_prec512(y, x1, w_c1, w_s1);
        y = cmac_prec512(y, x2, w_c2, w_s2);
        _mm512_storeu_si512(out, y);
      }
    } else if (n_layers == 4) {
      load_consts(__m512i, _mm512_set1_epi32, 1);
      load_consts(__m512i, _mm512_set1_epi32, 2);
      load_consts(__m512i, _mm512_set1_epi32, 3);
      for (; out < end; out += sizeof(__m512i) / sizeof(*out)) {
        const __m512i x = _mm512_loadu_si512(in0++);
        const __m512i x1 = _mm512_loadu_si512(in1++);
        const __m512i x2 = _mm512_loadu_si512(in2++);
        const __m512i x3 = _mm512_loadu_si512(in3++);
        // Matrix multiplication for 4 elements of the result (sizeof(simde__m256i) / sizeof(*prec_matrix) = 8)
        __m512i y = cmac0_prec512(x, w_c0, w_s0);
        y = cmac_prec512(y, x1, w_c1, w_s1);
        y = cmac_prec512(y, x2, w_c2, w_s2);
        y = cmac_prec512(y, x3, w_c3, w_s3);
        _mm512_storeu_si512(out, y);
      }
    }
  }
#endif
#ifdef __AVX2__
  {
    c16_t *end = beginning + (re_cnt & ~7);
    load_consts(simde__m256i, simde_mm256_set1_epi32, 0);
    if (n_layers == 1) {
      for (; out < end; out += sizeof(simde__m256i) / sizeof(*out)) {
        const simde__m256i x0 = simde_mm256_loadu_si256(in0++);
        // Accumulate the product
        simde__m256i y = cmac0_prec256(x0, w_c0, w_s0);
        // Store the result to txdataF
        simde_mm256_storeu_si256(out, y);
      }
    } else if (n_layers == 2) {
      load_consts(simde__m256i, simde_mm256_set1_epi32, 1);
      for (; out < end; out += sizeof(simde__m256i) / sizeof(*out)) {
        const simde__m256i x0 = simde_mm256_loadu_si256(in0++);
        const simde__m256i x1 = simde_mm256_loadu_si256(in1++);
        // Accumulate the product
        simde__m256i y = cmac0_prec256(x0, w_c0, w_s0);
        y = cmac_prec256(y, x1, w_c1, w_s1);
        // Store the result to txdataF
        simde_mm256_storeu_si256(out, y);
      }
    } else if (n_layers == 3) {
      load_consts(simde__m256i, simde_mm256_set1_epi32, 1);
      load_consts(simde__m256i, simde_mm256_set1_epi32, 2);
      for (; out < end; out += sizeof(simde__m256i) / sizeof(*out)) {
        const simde__m256i x0 = simde_mm256_loadu_si256(in0++);
        const simde__m256i x1 = simde_mm256_loadu_si256(in1++);
        const simde__m256i x2 = simde_mm256_loadu_si256(in2++);
        simde__m256i y = cmac0_prec256(x0, w_c0, w_s0);
        y = cmac_prec256(y, x1, w_c1, w_s1);
        y = cmac_prec256(y, x2, w_c2, w_s2);
        // Store the result to txdataF
        simde_mm256_storeu_si256(out, y);
      }
    } else if (n_layers == 4) {
      load_consts(simde__m256i, simde_mm256_set1_epi32, 1);
      load_consts(simde__m256i, simde_mm256_set1_epi32, 2);
      load_consts(simde__m256i, simde_mm256_set1_epi32, 3);
      for (; out < end; out += sizeof(simde__m256i) / sizeof(*out)) {
        const simde__m256i x0 = simde_mm256_loadu_si256(in0++);
        const simde__m256i x1 = simde_mm256_loadu_si256(in1++);
        const simde__m256i x2 = simde_mm256_loadu_si256(in2++);
        const simde__m256i x3 = simde_mm256_loadu_si256(in3++);
        simde__m256i y = cmac0_prec256(x0, w_c0, w_s0);
        y = cmac_prec256(y, x1, w_c1, w_s1);
        y = cmac_prec256(y, x2, w_c2, w_s2);
        y = cmac_prec256(y, x3, w_c3, w_s3);
        // Store the result to txdataF
        simde_mm256_storeu_si256(out, y);
      }
    }
  }
#endif
  c16_t *end = beginning + (re_cnt & ~3);
#ifdef DEBUG_DLSCH_PRECODING_PRINT_WITH_TRIVIAL // Get result with trivial solution, TODO: To be removed
  // 128 SIMD: Do 4 RE in one iteration, 3 iterations for 1 RB
  for (; out < end; out += sizeof(simde__m128i) / sizeof(*out)) {
    c16_t y_triv[4];
    for (int i = 0; i < 4; i++)
      y_triv[i] = nr_layer_precoder_cm(n_layers, symSz, txdataF_res_mapped, ant, pmi_pdu, sc + i);
    memcpy(out, y_triv, sizeof(y_triv));
  }
#endif
#ifdef __aarch64__
#ifdef NEWOPTIM
  load_consts_arm(0);
#else
  load_consts(int16x8_t, vdupq_n_s16, 0);  
#endif
  if (n_layers == 1) {
    for (; out < end; out += sizeof(int16x8_t) / sizeof(*out)) {
#ifdef NEWOPTIM
      const int16x4x2_t y = cmac0_prec4(vld1q_s16((const int16_t *)in0++), wr0, wi0);
      vst2_s16((int16_t *)out, y);
#else
      const int16x8_t x0 = vld1q_s16((const int16_t *)in0++);
      // Accumulate the product
      int16x8_t y = cmac0_prec128(x0, w_c0, w_s0);
      // Store the result to txdataF
      *(int16x8_t *)out = y;
#endif
    }
  }
  if (n_layers == 2) {
#ifdef NEWOPTIM
    load_consts_arm(1);
#else
    load_consts(int16x8_t, vdupq_n_s16, 1);  
#endif
    for (; out < end; out += sizeof(int16x8_t) / sizeof(*out)) {
#ifdef NEWOPTIM
      int16x4x2_t y = cmac0_prec4(vld1q_s16((const int16_t *)in0++), wr0, wi0);
      y = cmac_prec4(y, vld1q_s16((const int16_t *)in1++), wr1, wi1);
      vst2_s16((int16_t *)out, y);
#else
      const int16x8_t x0 = vld1q_s16((const int16_t *)in0++);
      const int16x8_t x1 = vld1q_s16((const int16_t *)in1++);
      // Accumulate the product
      int16x8_t y = cmac0_prec128(x0, w_c0, w_s0);
      y = cmac_prec128(y, x1, w_c1, w_s1);
      // Store the result to txdataF
      *(int16x8_t *)out = y;
#endif
    }
  }
  if (n_layers == 3) {
#ifdef NEWOPTIM
    load_consts_arm(1);
    load_consts_arm(2);
#else
    load_consts(int16x8_t, vdupq_n_s16, 1);
    load_consts(int16x8_t, vdupq_n_s16, 2);
#endif
    for (; out < end; out += sizeof(int16x8_t) / sizeof(*out)) {
#ifdef NEWOPTIM
      int16x4x2_t y = cmac0_prec4(vld1q_s16((const int16_t *)in0++), wr0, wi0);
      y = cmac_prec4(y, vld1q_s16((const int16_t *)in1++), wr1, wi1);
      y = cmac_prec4(y, vld1q_s16((const int16_t *)in2++), wr2, wi2);
      vst2_s16((int16_t *)out, y);
#else
      const int16x8_t x0 = vld1q_s16((const int16_t *)in0++);
      const int16x8_t x1 = vld1q_s16((const int16_t *)in1++);
      const int16x8_t x2 = vld1q_s16((const int16_t *)in2++);
      // Accumulate the product
      int16x8_t y = cmac0_prec128(x0, w_c0, w_s0);
      y = cmac_prec128(y, x1, w_c1, w_s1);
      y = cmac_prec128(y, x2, w_c2, w_s2);
      // Store the result to txdataF
      *(int16x8_t *)out = y;
#endif
    }
  }
  if (n_layers == 4) {
#ifdef NEWOPTIM
    load_consts_arm(1);
    load_consts_arm(2);
    load_consts_arm(3);
#else
    load_consts(int16x8_t, vdupq_n_s16, 1);
    load_consts(int16x8_t, vdupq_n_s16, 2);
    load_consts(int16x8_t, vdupq_n_s16, 3);
#endif
    for (; out < end; out += sizeof(int16x8_t) / sizeof(*out)) {
#ifdef NEWOPTIM
      int16x4x2_t y = cmac0_prec4(vld1q_s16((const int16_t *)in0++), wr0, wi0);
      y = cmac_prec4(y, vld1q_s16((const int16_t *)in1++), wr1, wi1);
      y = cmac_prec4(y, vld1q_s16((const int16_t *)in2++), wr2, wi2);
      y = cmac_prec4(y, vld1q_s16((const int16_t *)in3++), wr3, wi3);
      vst2_s16((int16_t *)out, y);
#else
      const int16x8_t x0 = vld1q_s16((const int16_t *)in0++);
      const int16x8_t x1 = vld1q_s16((const int16_t *)in1++);
      const int16x8_t x2 = vld1q_s16((const int16_t *)in2++);
      const int16x8_t x3 = vld1q_s16((const int16_t *)in3++);
      // Accumulate the product
      int16x8_t y = cmac0_prec128(x0, w_c0, w_s0);
      y = cmac_prec128(y, x1, w_c1, w_s1);
      y = cmac_prec128(y, x2, w_c2, w_s2);
      y = cmac_prec128(y, x3, w_c3, w_s3);
      // Store the result to txdataF
      *(int16x8_t *)out = y;      
#endif
    }
  }
#else
  load_consts(simde__m128i, simde_mm_set1_epi32, 0);
  if (n_layers == 1) {
    for (; out < end; out += sizeof(simde__m128i) / sizeof(*out)) {
      const simde__m128i x0 = simde_mm_loadu_si128(in0++);
      // Accumulate the product
      simde__m128i y = cmac0_prec128(x0, w_c0, w_s0);
      // Store the result to txdataF
      simde_mm_storeu_si128(out, y);
    }
  } else if (n_layers == 2) {
    load_consts(simde__m128i, simde_mm_set1_epi32, 1);
    for (; out < end; out += sizeof(simde__m128i) / sizeof(*out)) {
      const simde__m128i x0 = simde_mm_loadu_si128(in0++);
      const simde__m128i x1 = simde_mm_loadu_si128(in1++);
      // Accumulate the product
      simde__m128i y = cmac0_prec128(x0, w_c0, w_s0);
      y = cmac_prec128(y, x1, w_c1, w_s1);
      // Store the result to txdataF
      simde_mm_storeu_si128(out, y);
    }
  } else if (n_layers == 3) {
    load_consts(simde__m128i, simde_mm_set1_epi32, 1);
    load_consts(simde__m128i, simde_mm_set1_epi32, 2);
    for (; out < end; out += sizeof(simde__m128i) / sizeof(*out)) {
      const simde__m128i x0 = simde_mm_loadu_si128(in0++);
      const simde__m128i x1 = simde_mm_loadu_si128(in1++);
      const simde__m128i x2 = simde_mm_loadu_si128(in2++);
      simde__m128i y = cmac0_prec128(x0, w_c0, w_s0);
      y = cmac_prec128(y, x1, w_c1, w_s1);
      y = cmac_prec128(y, x2, w_c2, w_s2);
      // Store the result to txdataF
      simde_mm_storeu_si128(out, y);
    }
  } else if (n_layers == 4) {
    load_consts(simde__m128i, simde_mm_set1_epi32, 1);
    load_consts(simde__m128i, simde_mm_set1_epi32, 2);
    load_consts(simde__m128i, simde_mm_set1_epi32, 3);
    for (; out < end; out += sizeof(simde__m128i) / sizeof(*out)) {
      const simde__m128i x0 = simde_mm_loadu_si128(in0++);
      const simde__m128i x1 = simde_mm_loadu_si128(in1++);
      const simde__m128i x2 = simde_mm_loadu_si128(in2++);
      const simde__m128i x3 = simde_mm_loadu_si128(in3++);
      simde__m128i y = cmac0_prec128(x0, w_c0, w_s0);
      y = cmac_prec128(y, x1, w_c1, w_s1);
      y = cmac_prec128(y, x2, w_c2, w_s2);
      y = cmac_prec128(y, x3, w_c3, w_s3);
      // Store the result to txdataF
      simde_mm_storeu_si128(out, y);
    }
  }
#endif
#ifdef DEBUG_DLSCH_PRECODING_PRINT_WITH_TRIVIAL // Print simd and trivial result, TODO: To be removed
  c16_t *y_simd = (c16_t *)&y;
  printf("debug_to_be_removed re_cnt=%d, sc=%u, y_simd=(%+4d,%+4d), (%+4d,%+4d), (%+4d,%+4d), (%+4d,%+4d)\n",
         re_cnt,
         sc,
         y_simd[0].r,
         y_simd[0].i,
         y_simd[1].r,
         y_simd[1].i,
         y_simd[2].r,
         y_simd[2].i,
         y_simd[3].r,
         y_simd[3].i);
  printf("debug_to_be_removed re_cnt=%d, sc=%u, y_triv=(%+4d,%+4d), (%+4d,%+4d), (%+4d,%+4d), (%+4d,%+4d)\n",
         re_cnt,
         sc,
         y_triv[0].r,
         y_triv[0].i,
         y_triv[1].r,
         y_triv[1].i,
         y_triv[2].r,
         y_triv[2].i,
         y_triv[3].r,
         y_triv[3].i);
#endif
}
