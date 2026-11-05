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

  // Implementation selection (single entry point, chosen internally):
  // For 1-2 layers the vectorised "modulate then deinterleave" path
  // (nr_modulation + nr_layer_mapping) is faster than the fused scalar
  // per-symbol loop below. The fused path only pays off for 3-4 layers, where
  // the intermediate symbol buffer is large enough to be memory-bound and the
  // separate layer mapping is itself poorly vectorised. For 1-2 layers the
  // buffer stays in cache, so the scalar per-symbol modulation (which costs one
  // bit-extract + table lookup per symbol, i.e. scales with symbol count and
  // hurts most at low modulation orders) loses to SIMD modulation.
  if (n_layers == 1) {
    nr_modulation(in, length, mod_order, (int16_t *)tx_layers[0]);
    return true;
  }
  if (n_layers == 2) {
    c16_t mod_symbs[n_symbs] __attribute__((aligned(64)));
    nr_modulation(in, length, mod_order, (int16_t *)mod_symbs);
    c16_t (*ms)[n_symbs] = &mod_symbs;
    nr_layer_mapping(1, n_symbs, ms, n_layers, layerSz, n_symbs, tx_layers);
    return true;
  }

  const uint8_t *in_bytes = (const uint8_t *)in;

  switch (mod_order) {
    case 2: {
      const c16_t *nr_mod_table = nr_qpsk_mod_table;
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
          const uint64_t bits = get_packed_bits(in_bytes, length, sym * mod_order, 24);
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

void nr_dft(c16_t *output, c16_t *input, uint32_t Msc_PUSCH)
{
  const dft_size_idx_t size = get_dft(Msc_PUSCH);

  dft(size,
      (int16_t *)input,
      (int16_t *)output,
      1);
}

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

/* The table is generated FFT shifted, i.e. in the same layout as the frequency domain
   buffers it is applied to: the rotation of the first negative frequency of the carrier is
   at index 0 and the nbins rotations of the carrier are contiguous. */
void init_timeshift_rotation(const int ofdm_symbol_size,
                             const int nbins,
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
  fftshift_inplace(timeshift_symbol_rotation, nbins, ofdm_symbol_size);
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
/* Complex multiply-accumulate for the precoders, in DEINTERLEAVED (split real /
 * imaginary) form: 4 REs per call, reals in x.val[0] and imaginaries in x.val[1].
 * ld2/st2 split and re-merge the c16_t stream for free in the load and the store.
 *
 * The interleaved alternative (cmac0_prec128() below) forms xr = vuzp1q_s16(x, x),
 * whose two halves are identical, and multiplies the full vector -- so every lane's
 * work is done twice.  On ARMv8.0 that doubles the multiply-longs, which is why the
 * generic kernel uses this form there.  On ARMv8.1 the duplication is free, since one
 * vqrdmlah covers 8 lanes either way, and the interleaved form then wins on the
 * load/store side, so the generic kernel keeps it -- see nr_layer_precoder_simd().
 *
 * The cross-polar kernels use this form on every aarch64 core regardless: their
 * co-phasing by +-j is (r,i) -> (-i,r), which in split form is naming the other
 * register rather than rev32+neg+bsl per RE.
 *
 * Q15: the result is round((x * w) / 2^15) per component, saturating. */
static inline __attribute__((always_inline)) int16x4x2_t cmac0_prec4(int16x4x2_t x, int16x4_t wr, int16x4_t wi)
{
  const int16x4_t xr = x.val[0];
  const int16x4_t xi = x.val[1];
#ifdef __ARM_FEATURE_QRDMX
  // ARMv8.1-A: use the rounding doubling multiply-accumulate instructions
  // real = xr*wr - xi*wi
  int16x4_t real = vqdmulh_s16(xr, wr); // ~ round((2*xr*wr)/2^16)
  real = vqrdmlsh_s16(real, xi, wi);
  // imag = xr*wi + xi*wr
  int16x4_t imag = vqdmulh_s16(xr, wi);
  imag = vqrdmlah_s16(imag, xi, wr);
#else
  // ARMv8.0-A: widening multiplies, then round and narrow
  int32x4_t real_prod = vmull_s16(xr, wr);
  real_prod = vmlsl_s16(real_prod, xi, wi);

  int32x4_t imag_prod = vmull_s16(xr, wi);
  imag_prod = vmlal_s16(imag_prod, xi, wr);

  const int16x4_t real = vqrshrn_n_s32(real_prod, 15);
  const int16x4_t imag = vqrshrn_n_s32(imag_prod, 15);
#endif
  int16x4x2_t produ;
  produ.val[0] = real;
  produ.val[1] = imag;
  return produ;
}

static inline __attribute__((always_inline)) int16x4x2_t cmac_prec4(int16x4x2_t y, int16x4x2_t x, int16x4_t wr, int16x4_t wi)
{
  const int16x4x2_t produ = cmac0_prec4(x, wr, wi);
  // saturating add to match the x86 path (adds_epi16); plain vadd_s16 wraps on overflow
  y.val[0] = vqadd_s16(y.val[0], produ.val[0]);
  y.val[1] = vqadd_s16(y.val[1], produ.val[1]);
  return y;
}

#ifdef __ARM_FEATURE_QRDMX
/* Interleaved counterpart, ARMv8.1 only: 4 REs per call as |Re Im| pairs.  The two halves
 * of xr/xi are duplicates, but vqdmulhq/vqrdmlah cover a whole Q register at the same cost
 * as the D-register form, so the duplication is free and the plain ldr q / str q around it
 * are cheaper than ld2/st2.  Not built on ARMv8.0, where computing every product twice is
 * real work. */
static inline __attribute__((always_inline)) int16x8_t cmac0_prec128(int16x8_t x, int16x8_t wr, int16x8_t wi)
{
  const int16x8_t xr = vuzp1q_s16(x, x); // even lanes
  const int16x8_t xi = vuzp2q_s16(x, x); // odd  lanes
  // real = xr*wr - xi*wi  (Q15 scaling via high-half doubling muls)
  int16x8_t real = vqdmulhq_s16(xr, wr);
  real = vqrdmlshq_s16(real, xi, wi);
  // imag = xr*wi + xi*wr
  int16x8_t imag = vqdmulhq_s16(xr, wi);
  imag = vqrdmlahq_s16(imag, xi, wr);
  // Re-interleave [real, imag]
  return vzipq_s16(real, imag).val[0];
}

static inline __attribute__((always_inline)) int16x8_t cmac_prec128(int16x8_t y, int16x8_t x, int16x8_t wr, int16x8_t wi)
{
  // saturating add to match the x86 path (adds_epi16); plain vaddq_s16 wraps on overflow
  return vqaddq_s16(y, cmac0_prec128(x, wr, wi));
}
#endif // __ARM_FEATURE_QRDMX

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
#ifdef __aarch64__
/* The generic precoder loop below is written once and bound to one of two forms.
 *
 * ARMv8.1+ (QRDMX): interleaved.  One vqrdmlah covers a whole Q register, so the
 * duplicated halves the interleaved form produces cost nothing, and ldr q / str q beat
 * ld2/st2.  ARMv8.0: deinterleaved, because there the duplicated halves are real work --
 * 8 multiply-longs per layer where 4 would do.
 *
 * Precoding, 273 PRB, MCS 25, 4 ports, single-threaded, generic kernel:
 *   Cortex-A72  (no QRDMX)  2 layers  781.32 -> 577.54 us,  4 layers 1571.62 -> 960.14 us
 *   Cortex-X925 (QRDMX)     3 layers  343.52 -> 353.02 us,  4 layers  406.48 -> 435.60 us
 * i.e. deinterleaved is a 1.35-1.64x win on ARMv8.0 and a 3-7% loss on ARMv8.1, so each
 * core gets the form that suits it. */
#ifdef __ARM_FEATURE_QRDMX
#define PREC_ACC_T int16x8_t
#define load_consts_arm(Rank)                                   \
  const int16x8_t wr##Rank = vdupq_n_s16(weights[Rank][ant].r); \
  const int16x8_t wi##Rank = vdupq_n_s16(weights[Rank][ant].i); \
  const int16_t *in##Rank = (const int16_t *)(txdataF_res_mapped[Rank] + sc_offset + (out - beginning));
#define PREC_MAC0(In, Wr, Wi) cmac0_prec128(vld1q_s16(In), Wr, Wi)
#define PREC_MAC(Y, In, Wr, Wi) cmac_prec128(Y, vld1q_s16(In), Wr, Wi)
#define PREC_STORE(Out, Y) vst1q_s16((int16_t *)(Out), Y)
#else
#define PREC_ACC_T int16x4x2_t
#define load_consts_arm(Rank)                                  \
  const int16x4_t wr##Rank = vdup_n_s16(weights[Rank][ant].r); \
  const int16x4_t wi##Rank = vdup_n_s16(weights[Rank][ant].i); \
  const int16_t *in##Rank = (const int16_t *)(txdataF_res_mapped[Rank] + sc_offset + (out - beginning));
#define PREC_MAC0(In, Wr, Wi) cmac0_prec4(vld2_s16(In), Wr, Wi)
#define PREC_MAC(Y, In, Wr, Wi) cmac_prec4(Y, vld2_s16(In), Wr, Wi)
#define PREC_STORE(Out, Y) vst2_s16((int16_t *)(Out), Y)
#endif
#endif

/* Fast path for the 2 antenna-port / 2-layer precoder.
 *
 * For 2 CSI ports and 2 layers every 3GPP Type-I codebook weight is either
 * purely real (+/-1) or purely imaginary (+/-j), scaled by the common
 * normalisation constant C = round(SHRT_MAX/sqrt(2)) = 23170. The 2x2
 * precoding therefore reduces to a radix-2 butterfly:
 *
 *   y_ant = C * ( u0*x0 + u1*x1 ),   u0,u1 in {+1,-1,+j,-j}
 *
 * so the complex cross-term multiplies vanish: each u*x is a lane swap and/or
 * negation, the x0/x1 loads are shared between the two antenna outputs, and a
 * single rounding multiply by C is applied per output. This is ~2x faster than
 * two calls to the generic per-antenna kernel on x86 (AVX2/AVX-512) and ~5x on
 * aarch64/NEON, and is marginally more accurate (single rounded scale instead
 * of two truncating shifts). It differs from the generic kernel by <=2 LSB.
 *
 * The classifier below turns a unit weight into (swap, per-lane sign) so the
 * rotation u*x is a swap + sign_epi16 (mullo on AVX-512, vmul on NEON).
 */
typedef struct {
  bool swap;  // true for +/-j (real/imag parts must be swapped)
  c16_t sgn;  // per-lane sign (+/-1) applied after the optional swap
} nr_prec2x2_rot_t;

static inline int16_t nr_prec2x2_sat16(const int v)
{
  return (v > INT16_MAX) ? INT16_MAX : ((v < INT16_MIN) ? INT16_MIN : (int16_t)v);
}

static inline nr_prec2x2_rot_t nr_prec2x2_classify(const c16_t w)
{
  /* Precondition: the weight is a real scale times a unit rotation +/-1 or +/-j,
     i.e. exactly one of its two components is zero. The swap/sign decomposition
     below is only valid under that assumption, and a zero weight (both parts 0)
     would silently be classified as +1, so check it rather than trust it. */
  DevAssert((w.r == 0) != (w.i == 0));
  nr_prec2x2_rot_t r;
  if (w.i == 0) { // purely real: +/-1
    const int16_t s = (w.r >= 0) ? 1 : -1;
    r.swap = false;
    r.sgn = (c16_t){s, s};
  } else { // purely imaginary: +/-j.  j*x = (-x.i, x.r): swap then fix signs
    r.swap = true;
    r.sgn = (w.i >= 0) ? (c16_t){-1, 1} : (c16_t){1, -1};
  }
  return r;
}

#if defined(__aarch64__) && !defined(__ARM_FEATURE_QRDMX)
/* Fused cross-polar precoder for 2 layers onto 4 antenna ports.
 *
 * For 4 CSI ports with XP=2 (two polarisations of N1*N2=2 co-polar elements) the rank-2
 * Type-I codebook satisfies, for the port pair (p, p+2) of the same co-polar element:
 *
 *     W[0][p+2] = +phi * W[0][p]        W[1][p+2] = -phi * W[1][p],     phi in {1, j}
 *
 * (verified against the weights this codebase actually generates). So with
 * a = W[0][p]*x0 and b = W[1][p]*x1, the two polarisations are a butterfly:
 *
 *     y_p     = a + b
 *     y_{p+2} = phi * (a - b)
 *
 * which costs 4 complex multiplies per RE instead of the 8 the generic per-port kernel
 * does, because a and b are shared between the two polarisations. phi is a unit rotation,
 * so applying it is a lane swap plus a negate, not a multiply. Precoding is the dominant
 * term in DL TX: at 273 PRB / 2 layers / 4 ports it is 336.5 us of a 463.2 us PDSCH
 * generation on a Cortex-A78AE, against 43.4 us for resource mapping.
 *
 * NOT bit-exact against nr_layer_precoder_simd(): the stored W[l][p+2] is rounded to int16
 * independently of W[l][p], so deriving one from the other differs by up to ~1 LSB on a
 * weight of ~23170 (about -88 dB, far below the output quantisation). Validate with a
 * tolerance, not memcmp.
 *
 * phi_swap/phi_neg describe phi: {false,false} = +1, {true,X} = +/-j. */
void nr_layer_precoder_2x4_simd(const int symSz,
                                const c16_t txdataF_res_mapped[2][symSz],
                                c16_t weights[NR_MAX_NB_LAYERS][NR_MAX_CSI_PORTS],
                                const int p,
                                const bool phi_swap,
                                const bool phi_neg,
                                const int sc_offset,
                                const int re_cnt,
                                c16_t *out_lo,
                                c16_t *out_hi)
{
  const c16_t w0 = weights[0][p], w1 = weights[1][p];
  const int16x4_t w0r = vdup_n_s16(w0.r), w0i = vdup_n_s16(w0.i);
  const int16x4_t w1r = vdup_n_s16(w1.r), w1i = vdup_n_s16(w1.i);

  /* deinterleaved throughout: ld2/st2 split and merge for free, each product is computed
     once (see cmac0_prec4()), and the phi rotation below is a register naming, not work */
  const int16_t *in0 = (const int16_t *)(txdataF_res_mapped[0] + sc_offset);
  const int16_t *in1 = (const int16_t *)(txdataF_res_mapped[1] + sc_offset);
  int16_t *o_lo = (int16_t *)(out_lo + sc_offset);
  int16_t *o_hi = (int16_t *)(out_hi + sc_offset);

  int done = 0;
  for (; done + 4 <= re_cnt; done += 4) {
    const int16x4x2_t a = cmac0_prec4(vld2_s16(in0), w0r, w0i);
    const int16x4x2_t b = cmac0_prec4(vld2_s16(in1), w1r, w1i);
    in0 += 8;
    in1 += 8;
    /* saturating, to match the generic path's accumulation */
    int16x4x2_t sum, dif;
    sum.val[0] = vqadd_s16(a.val[0], b.val[0]);
    sum.val[1] = vqadd_s16(a.val[1], b.val[1]);
    const int16x4_t dr = vqsub_s16(a.val[0], b.val[0]);
    const int16x4_t di = vqsub_s16(a.val[1], b.val[1]);
    if (phi_swap) { /* multiply by +/-j: (r,i) -> (-i, r) or (i, -r) */
      if (phi_neg) {
        dif.val[0] = di;
        dif.val[1] = vqneg_s16(dr);
      } else {
        dif.val[0] = vqneg_s16(di);
        dif.val[1] = dr;
      }
    } else if (phi_neg) {
      dif.val[0] = vqneg_s16(dr);
      dif.val[1] = vqneg_s16(di);
    } else {
      dif = (int16x4x2_t){{dr, di}};
    }
    vst2_s16(o_lo + 2 * done, sum);
    vst2_s16(o_hi + 2 * done, dif);
  }
  /* re_cnt is always a multiple of NR_NB_SC_PER_RB = 12, so the SIMD loop is exact */
  DevAssert(done == re_cnt);
}
#endif // __aarch64__ && !__ARM_FEATURE_QRDMX

/* General cross-polar fast path for 2-4 layers onto 4 antenna ports.  Every 4-port
 * Type-I codebook entry has the block form W = [[A],[Phi.A]]: ports {p, p+2} are the two
 * polarisations and differ, per layer, by a co-phasing phi_l in {+-1,+-j}.  So
 *   out[p]   = sum_l  W[l][p].x_l            (the r beam products)
 *   out[p+2] = sum_l  phi_l . (W[l][p].x_l)  (the same products, co-phased)
 * sharing the r complex multiplies between both ports of a pair -- 2r MACs for the pair
 * instead of 4r for the generic per-port kernel.
 *
 * Everything here is kept in DEINTERLEAVED (separate real / imaginary) form, which is what
 * makes the saving real rather than nominal:
 *
 *  - the products are 4-lane (D form), through the shared cmac0_prec4(): ld2/st2
 *    deinterleave and re-interleave for free in the load and the store, and each lane is
 *    multiplied once (the interleaved form computes every product twice on ARMv8.0).
 *  - the co-phasing costs nothing.  phi_l takes only 4 values, so with the layers grouped
 *    by phi the second polarisation is
 *      out[p+2] = P + j.Q,  P = sum_{phi=+1} t - sum_{phi=-1} t,  Q = sum_{phi=+j} t - sum_{phi=-j} t
 *    the signs fold into the accumulation (vqsub instead of vqadd, same cost), and in split
 *    form j.(r,i) = (-i, r) is just naming the other accumulator: hi_r = P_r - Q_i,
 *    hi_i = P_i + Q_r.  Applied to interleaved data it would instead cost rev32+neg+bsl per
 *    +-j layer per RE, which is what ate the MAC saving in the first version of this kernel.
 *
 * Like nr_layer_precoder_simd(), the RE loop is specialised per layer count: the layers are
 * sorted into the four phi groups in the prologue, but the loop body then indexes the
 * sorted weights and input pointers with *constants*, so they stay in registers.
 *
 * Like nr_layer_precoder_2x4_simd() this is NOT bit-exact vs the generic kernel (W[l][p+2]
 * is rounded independently of W[l][p], so deriving one from the other differs by up to
 * ~1 LSB per layer); the phi grouping also reorders the saturating accumulation, which can
 * differ from layer order only where an accumulator saturates.  Validate with a tolerance.
 * phi_swap[l]/phi_neg[l] encode phi_l: {f,f}=+1 {f,t}=-1 {t,f}=+j {t,t}=-j. */
void nr_layer_precoder_Nx4_simd(const int n_layers,
                                const int symSz,
                                const c16_t txdataF_res_mapped[n_layers][symSz],
                                c16_t weights[NR_MAX_NB_LAYERS][NR_MAX_CSI_PORTS],
                                const int p,
                                const bool phi_swap[NR_MAX_NB_LAYERS],
                                const bool phi_neg[NR_MAX_NB_LAYERS],
                                const int sc_offset,
                                const int re_cnt,
                                c16_t *out_lo,
                                c16_t *out_hi)
{
  AssertFatal(n_layers >= 2 && n_layers <= 4, "Shouldn't get here, n_layers %d\n", n_layers);
#ifdef __aarch64__
  int16x4_t wr[NR_MAX_NB_LAYERS], wi[NR_MAX_NB_LAYERS];
  const int16_t *in[NR_MAX_NB_LAYERS];
  int end[4]; /* exclusive end of each phi group, in the order +1, -1, +j, -j */
  int n = 0;
  for (int g = 0; g < 4; g++) {
    const bool g_swap = g >= 2, g_neg = (g & 1) != 0;
    for (int l = 0; l < n_layers; l++) {
      if (phi_swap[l] == g_swap && phi_neg[l] == g_neg) {
        wr[n] = vdup_n_s16(weights[l][p].r);
        wi[n] = vdup_n_s16(weights[l][p].i);
        in[n] = (const int16_t *)(txdataF_res_mapped[l] + sc_offset);
        n++;
      }
    }
    end[g] = n;
  }
  DevAssert(n == n_layers); /* the caller classified every layer */
  const int e0 = end[0], e1 = end[1], e2 = end[2];
  int16_t *o_lo = (int16_t *)(out_lo + sc_offset);
  int16_t *o_hi = (int16_t *)(out_hi + sc_offset);

  /* one sorted layer: the group tests are on constants and loop-invariant bounds */
#define NX4_LAYER(I)                                                  \
  do {                                                                \
    const int16x4x2_t t = cmac0_prec4(vld2_s16(in##I), wr[I], wi[I]); \
    in##I += 8;                                                       \
    const int16x4_t tr = t.val[0], ti = t.val[1];                     \
    lo_r = vqadd_s16(lo_r, tr);                                       \
    lo_i = vqadd_s16(lo_i, ti);                                       \
    if ((I) < e0) {                                                   \
      P_r = vqadd_s16(P_r, tr);                                       \
      P_i = vqadd_s16(P_i, ti);                                       \
    } else if ((I) < e1) {                                            \
      P_r = vqsub_s16(P_r, tr);                                       \
      P_i = vqsub_s16(P_i, ti);                                       \
    } else if ((I) < e2) {                                            \
      Q_r = vqadd_s16(Q_r, tr);                                       \
      Q_i = vqadd_s16(Q_i, ti);                                       \
    } else {                                                          \
      Q_r = vqsub_s16(Q_r, tr);                                       \
      Q_i = vqsub_s16(Q_i, ti);                                       \
    }                                                                 \
  } while (0)

#define NX4_RUN(R)                                                                        \
  do {                                                                                    \
    const int16_t *in0 = in[0];                                                           \
    const int16_t *in1 = in[1];                                                           \
    const int16_t *in2 = in[(R) > 2 ? 2 : 0];                                             \
    const int16_t *in3 = in[(R) > 3 ? 3 : 0];                                             \
    (void)in2;                                                                            \
    (void)in3;                                                                            \
    const int16x4_t zero = vdup_n_s16(0);                                                 \
    for (; done + 4 <= re_cnt; done += 4) {                                               \
      int16x4_t lo_r = zero, lo_i = zero, P_r = zero, P_i = zero, Q_r = zero, Q_i = zero; \
      NX4_LAYER(0);                                                                       \
      NX4_LAYER(1);                                                                       \
      if ((R) > 2)                                                                        \
        NX4_LAYER(2);                                                                     \
      if ((R) > 3)                                                                        \
        NX4_LAYER(3);                                                                     \
      /* hi = P + j.Q : j.(r,i) = (-i, r), free in split form */                          \
      int16x4x2_t o;                                                                      \
      o.val[0] = lo_r;                                                                    \
      o.val[1] = lo_i;                                                                    \
      vst2_s16(o_lo + 2 * done, o);                                                       \
      o.val[0] = vqsub_s16(P_r, Q_i);                                                     \
      o.val[1] = vqadd_s16(P_i, Q_r);                                                     \
      vst2_s16(o_hi + 2 * done, o);                                                       \
    }                                                                                     \
  } while (0)

  int done = 0;
  switch (n_layers) {
    case 2:
      NX4_RUN(2);
      break;
    case 3:
      NX4_RUN(3);
      break;
    default:
      NX4_RUN(4);
      break;
  }
#undef NX4_RUN
#undef NX4_LAYER
#else /* x86 and anything else simde covers */
  /* Same algebra, interleaved.  Deinterleaving is an aarch64 idea: VPMADDWD multiplies
     adjacent 16-bit pairs, which already matches the |Re Im| layout, so the products are
     formed with the generic kernel's own cmac0_prec*() and are bit-identical to what it
     computes for out[p].  What the bucketing buys here is the same thing it buys on NEON:
     phi is applied once per output vector rather than once per layer per RE. */
  c16_t w[NR_MAX_NB_LAYERS];
  const c16_t *src[NR_MAX_NB_LAYERS];
  int grp[4]; /* exclusive end of each phi group, in the order +1, -1, +j, -j */
  int n = 0;
  for (int g = 0; g < 4; g++) {
    const bool g_swap = g >= 2, g_neg = (g & 1) != 0;
    for (int l = 0; l < n_layers; l++) {
      if (phi_swap[l] == g_swap && phi_neg[l] == g_neg) {
        w[n] = weights[l][p];
        src[n] = txdataF_res_mapped[l] + sc_offset;
        n++;
      }
    }
    grp[g] = n;
  }
  DevAssert(n == n_layers); /* the caller classified every layer */
  const int e0 = grp[0], e1 = grp[1], e2 = grp[2];
  c16_t *lo_out = out_lo + sc_offset;
  c16_t *hi_out = out_hi + sc_offset;
  int done = 0;
  int lim;

/* One sorted layer: t = W[l][p].x_l, accumulated into out[p] and, with the sign of phi_l
   folded into the accumulate, into the P or Q bucket of out[p+2].  The group tests are on
   constants and loop-invariant bounds, so they fold away per specialisation. */
#define NX4_LAYER(I)                                                      \
  do {                                                                    \
    const NX4_VT t = NX4_CMAC0(NX4_LOADU(src##I + done), w_c##I, w_s##I); \
    lo = NX4_ADDS(lo, t);                                                 \
    if ((I) < e0)                                                         \
      P = NX4_ADDS(P, t);                                                 \
    else if ((I) < e1)                                                    \
      P = NX4_SUBS(P, t);                                                 \
    else if ((I) < e2)                                                    \
      Q = NX4_ADDS(Q, t);                                                 \
    else                                                                  \
      Q = NX4_SUBS(Q, t);                                                 \
  } while (0)

#define NX4_W(Rank, Idx)                                        \
  const NX4_VT w_c##Rank = NX4_SET1(c16toI32(c16conj(w[Idx]))); \
  const NX4_VT w_s##Rank = NX4_SET1(c16toI32(c16swap(w[Idx]))); \
  const c16_t *src##Rank = src[Idx];

#define NX4_RUN(R, W)                                                             \
  do {                                                                            \
    NX4_W(0, 0)                                                                   \
    NX4_W(1, 1)                                                                   \
    NX4_W(2, (R) > 2 ? 2 : 0)                                                     \
    NX4_W(3, (R) > 3 ? 3 : 0)                                                     \
    (void)w_c2;                                                                   \
    (void)w_s2;                                                                   \
    (void)src2;                                                                   \
    (void)w_c3;                                                                   \
    (void)w_s3;                                                                   \
    (void)src3;                                                                   \
    /* j.(r,i) = (-i, r): swap the lanes of each pair, negate the new real one */ \
    const NX4_VT jr = NX4_SET1(c16toI32(((c16_t){-1, 1})));                       \
    for (; done + (W) <= lim; done += (W)) {                                      \
      NX4_VT lo = NX4_ZERO, P = NX4_ZERO, Q = NX4_ZERO;                           \
      NX4_LAYER(0);                                                               \
      NX4_LAYER(1);                                                               \
      if ((R) > 2)                                                                \
        NX4_LAYER(2);                                                             \
      if ((R) > 3)                                                                \
        NX4_LAYER(3);                                                             \
      NX4_STOREU(lo_out + done, lo);                                              \
      NX4_STOREU(hi_out + done, NX4_ADDS(P, NX4_JMUL(NX4_SWAP(Q), jr)));          \
    }                                                                             \
  } while (0)

/* specialise the RE loop per layer count, so the sorted weights and pointers are indexed
   by constants and stay in registers */
#define NX4_TIER(W)     \
  do {                  \
    switch (n_layers) { \
      case 2:           \
        NX4_RUN(2, W);  \
        break;          \
      case 3:           \
        NX4_RUN(3, W);  \
        break;          \
      default:          \
        NX4_RUN(4, W);  \
        break;          \
    }                   \
  } while (0)

  /* widest tier first, then the narrower ones mop up the remainder, as the generic kernel
     does -- re_cnt is a multiple of NR_NB_SC_PER_RB = 12, hence of 4, so 128 bits finishes */
#if defined(__AVX512F__) && defined(__AVX512BW__)
#define NX4_VT __m512i
#define NX4_SET1 _mm512_set1_epi32
#define NX4_ZERO _mm512_setzero_si512()
#define NX4_CMAC0 cmac0_prec512
#define NX4_ADDS _mm512_adds_epi16
#define NX4_SUBS _mm512_subs_epi16
#define NX4_LOADU(P) _mm512_loadu_si512((const void *)(P))
#define NX4_STOREU(P, V) _mm512_storeu_si512((void *)(P), V)
#define NX4_SWAP oai_mm512_swap
#define NX4_JMUL _mm512_mullo_epi16 /* AVX-512BW dropped vpsignw */
  lim = re_cnt & ~15;
  NX4_TIER(16);
#undef NX4_VT
#undef NX4_SET1
#undef NX4_ZERO
#undef NX4_CMAC0
#undef NX4_ADDS
#undef NX4_SUBS
#undef NX4_LOADU
#undef NX4_STOREU
#undef NX4_SWAP
#undef NX4_JMUL
#endif
#ifdef __AVX2__
#define NX4_VT simde__m256i
#define NX4_SET1 simde_mm256_set1_epi32
#define NX4_ZERO simde_mm256_setzero_si256()
#define NX4_CMAC0 cmac0_prec256
#define NX4_ADDS simde_mm256_adds_epi16
#define NX4_SUBS simde_mm256_subs_epi16
#define NX4_LOADU(P) simde_mm256_loadu_si256((const simde__m256i *)(P))
#define NX4_STOREU(P, V) simde_mm256_storeu_si256((simde__m256i *)(P), V)
#define NX4_SWAP oai_mm256_swap
#define NX4_JMUL simde_mm256_sign_epi16
  lim = re_cnt & ~7;
  NX4_TIER(8);
#undef NX4_VT
#undef NX4_SET1
#undef NX4_ZERO
#undef NX4_CMAC0
#undef NX4_ADDS
#undef NX4_SUBS
#undef NX4_LOADU
#undef NX4_STOREU
#undef NX4_SWAP
#undef NX4_JMUL
#endif
#define NX4_VT simde__m128i
#define NX4_SET1 simde_mm_set1_epi32
#define NX4_ZERO simde_mm_setzero_si128()
#define NX4_CMAC0 cmac0_prec128
#define NX4_ADDS simde_mm_adds_epi16
#define NX4_SUBS simde_mm_subs_epi16
#define NX4_LOADU(P) simde_mm_loadu_si128((const simde__m128i *)(P))
#define NX4_STOREU(P, V) simde_mm_storeu_si128((simde__m128i *)(P), V)
#define NX4_SWAP oai_mm_swap
#define NX4_JMUL simde_mm_sign_epi16
  lim = re_cnt & ~3;
  NX4_TIER(4);
#undef NX4_VT
#undef NX4_SET1
#undef NX4_ZERO
#undef NX4_CMAC0
#undef NX4_ADDS
#undef NX4_SUBS
#undef NX4_LOADU
#undef NX4_STOREU
#undef NX4_SWAP
#undef NX4_JMUL
#undef NX4_TIER
#undef NX4_RUN
#undef NX4_W
#undef NX4_LAYER
#endif
  /* re_cnt is always a multiple of NR_NB_SC_PER_RB = 12, so the SIMD loop is exact */
  DevAssert(done == re_cnt);
}

void nr_layer_precoder_2x2_simd(const int symSz,
                                const c16_t txdataF_res_mapped[2][symSz],
                                c16_t weights[NR_MAX_NB_LAYERS][NR_MAX_CSI_PORTS],
                                const int sc_offset,
                                const int re_cnt,
                                c16_t *txdataF_precoded_ant0,
                                c16_t *txdataF_precoded_ant1)
{
  // Unit-weight rotations for each (layer, antenna port) and the common scale C
  const c16_t w00 = weights[0][0], w10 = weights[1][0]; // -> antenna port 0
  const c16_t w01 = weights[0][1], w11 = weights[1][1]; // -> antenna port 1
  const int16_t C = (w00.i == 0) ? (int16_t)abs(w00.r) : (int16_t)abs(w00.i);
  const nr_prec2x2_rot_t r00 = nr_prec2x2_classify(w00), r10 = nr_prec2x2_classify(w10);
  const nr_prec2x2_rot_t r01 = nr_prec2x2_classify(w01), r11 = nr_prec2x2_classify(w11);

  const c16_t *in0 = txdataF_res_mapped[0] + sc_offset;
  const c16_t *in1 = txdataF_res_mapped[1] + sc_offset;
  c16_t *out0 = txdataF_precoded_ant0 + sc_offset;
  c16_t *out1 = txdataF_precoded_ant1 + sc_offset;
  int done = 0;

  // NOTE: each layer is scaled by C *first*, then the unit rotation (swap/sign)
  // is applied and the two contributions are added with saturation. Scaling
  // first is essential for correctness: (x0 +/- x1) can exceed the int16 range
  // even for correctly scaled inputs, so adding before scaling would saturate
  // the intermediate sum and collapse the result. The real scale C commutes
  // with the swap/sign rotation, so this reordering is free.

#if defined(__AVX512F__) && defined(__AVX512BW__)
  {
    const __m512i C512 = _mm512_set1_epi16(C);
    const __m512i s00 = _mm512_set1_epi32(c16toI32(r00.sgn)), s10 = _mm512_set1_epi32(c16toI32(r10.sgn));
    const __m512i s01 = _mm512_set1_epi32(c16toI32(r01.sgn)), s11 = _mm512_set1_epi32(c16toI32(r11.sgn));
    for (; done + 16 <= re_cnt; done += 16) {
      const __m512i sx0 = _mm512_mulhrs_epi16(_mm512_loadu_si512(in0 + done), C512);
      const __m512i sx1 = _mm512_mulhrs_epi16(_mm512_loadu_si512(in1 + done), C512);
      const __m512i sx0s = oai_mm512_swap(sx0), sx1s = oai_mm512_swap(sx1);
      const __m512i a0 = _mm512_adds_epi16(_mm512_mullo_epi16(r00.swap ? sx0s : sx0, s00),
                                           _mm512_mullo_epi16(r10.swap ? sx1s : sx1, s10));
      const __m512i a1 = _mm512_adds_epi16(_mm512_mullo_epi16(r01.swap ? sx0s : sx0, s01),
                                           _mm512_mullo_epi16(r11.swap ? sx1s : sx1, s11));
      _mm512_storeu_si512(out0 + done, a0);
      _mm512_storeu_si512(out1 + done, a1);
    }
  }
#endif
#ifdef __AVX2__
  {
    const simde__m256i C256 = simde_mm256_set1_epi16(C);
    const simde__m256i s00 = simde_mm256_set1_epi32(c16toI32(r00.sgn)), s10 = simde_mm256_set1_epi32(c16toI32(r10.sgn));
    const simde__m256i s01 = simde_mm256_set1_epi32(c16toI32(r01.sgn)), s11 = simde_mm256_set1_epi32(c16toI32(r11.sgn));
    for (; done + 8 <= re_cnt; done += 8) {
      const simde__m256i sx0 = simde_mm256_mulhrs_epi16(simde_mm256_loadu_si256((const simde__m256i *)(in0 + done)), C256);
      const simde__m256i sx1 = simde_mm256_mulhrs_epi16(simde_mm256_loadu_si256((const simde__m256i *)(in1 + done)), C256);
      const simde__m256i sx0s = oai_mm256_swap(sx0), sx1s = oai_mm256_swap(sx1);
      const simde__m256i a0 = simde_mm256_adds_epi16(simde_mm256_sign_epi16(r00.swap ? sx0s : sx0, s00),
                                                     simde_mm256_sign_epi16(r10.swap ? sx1s : sx1, s10));
      const simde__m256i a1 = simde_mm256_adds_epi16(simde_mm256_sign_epi16(r01.swap ? sx0s : sx0, s01),
                                                     simde_mm256_sign_epi16(r11.swap ? sx1s : sx1, s11));
      simde_mm256_storeu_si256((simde__m256i *)(out0 + done), a0);
      simde_mm256_storeu_si256((simde__m256i *)(out1 + done), a1);
    }
  }
#endif
#ifdef __aarch64__
  {
    const int16x8_t Cv = vdupq_n_s16(C);
    const int16x8_t s00 = vreinterpretq_s16_u32(vdupq_n_u32(c16toI32(r00.sgn)));
    const int16x8_t s10 = vreinterpretq_s16_u32(vdupq_n_u32(c16toI32(r10.sgn)));
    const int16x8_t s01 = vreinterpretq_s16_u32(vdupq_n_u32(c16toI32(r01.sgn)));
    const int16x8_t s11 = vreinterpretq_s16_u32(vdupq_n_u32(c16toI32(r11.sgn)));
    for (; done + 4 <= re_cnt; done += 4) {
      const int16x8_t sx0 = vqrdmulhq_s16(vld1q_s16((const int16_t *)(in0 + done)), Cv);
      const int16x8_t sx1 = vqrdmulhq_s16(vld1q_s16((const int16_t *)(in1 + done)), Cv);
      const int16x8_t sx0s = vrev32q_s16(sx0), sx1s = vrev32q_s16(sx1);
      const int16x8_t a0 = vqaddq_s16(vmulq_s16(r00.swap ? sx0s : sx0, s00),
                                      vmulq_s16(r10.swap ? sx1s : sx1, s10));
      const int16x8_t a1 = vqaddq_s16(vmulq_s16(r01.swap ? sx0s : sx0, s01),
                                      vmulq_s16(r11.swap ? sx1s : sx1, s11));
      vst1q_s16((int16_t *)(out0 + done), a0);
      vst1q_s16((int16_t *)(out1 + done), a1);
    }
  }
#else
  {
    const simde__m128i C128 = simde_mm_set1_epi16(C);
    const simde__m128i s00 = simde_mm_set1_epi32(c16toI32(r00.sgn)), s10 = simde_mm_set1_epi32(c16toI32(r10.sgn));
    const simde__m128i s01 = simde_mm_set1_epi32(c16toI32(r01.sgn)), s11 = simde_mm_set1_epi32(c16toI32(r11.sgn));
    for (; done + 4 <= re_cnt; done += 4) {
      const simde__m128i sx0 = simde_mm_mulhrs_epi16(simde_mm_loadu_si128((const simde__m128i *)(in0 + done)), C128);
      const simde__m128i sx1 = simde_mm_mulhrs_epi16(simde_mm_loadu_si128((const simde__m128i *)(in1 + done)), C128);
      const simde__m128i sx0s = oai_mm_swap(sx0), sx1s = oai_mm_swap(sx1);
      const simde__m128i a0 = simde_mm_adds_epi16(simde_mm_sign_epi16(r00.swap ? sx0s : sx0, s00),
                                                  simde_mm_sign_epi16(r10.swap ? sx1s : sx1, s10));
      const simde__m128i a1 = simde_mm_adds_epi16(simde_mm_sign_epi16(r01.swap ? sx0s : sx0, s01),
                                                  simde_mm_sign_epi16(r11.swap ? sx1s : sx1, s11));
      simde_mm_storeu_si128((simde__m128i *)(out0 + done), a0);
      simde_mm_storeu_si128((simde__m128i *)(out1 + done), a1);
    }
  }
#endif

  // Scalar remainder (re_cnt is a multiple of 12 -> multiple of 4, so normally none)
  for (; done < re_cnt; done++) {
    // Scale each layer first (rounding, matching mulhrs), then rotate and add with saturation
    const c16_t sx0 = {(int16_t)((in0[done].r * C + 16384) >> 15), (int16_t)((in0[done].i * C + 16384) >> 15)};
    const c16_t sx1 = {(int16_t)((in1[done].r * C + 16384) >> 15), (int16_t)((in1[done].i * C + 16384) >> 15)};
    const c16_t p00 = r00.swap ? (c16_t){(int16_t)-sx0.i, sx0.r} : sx0;
    const c16_t p10 = r10.swap ? (c16_t){(int16_t)-sx1.i, sx1.r} : sx1;
    const c16_t p01 = r01.swap ? (c16_t){(int16_t)-sx0.i, sx0.r} : sx0;
    const c16_t p11 = r11.swap ? (c16_t){(int16_t)-sx1.i, sx1.r} : sx1;
    const int a0r = r00.sgn.r * p00.r + r10.sgn.r * p10.r;
    const int a0i = r00.sgn.i * p00.i + r10.sgn.i * p10.i;
    const int a1r = r01.sgn.r * p01.r + r11.sgn.r * p11.r;
    const int a1i = r01.sgn.i * p01.i + r11.sgn.i * p11.i;
    out0[done] = (c16_t){nr_prec2x2_sat16(a0r), nr_prec2x2_sat16(a0i)};
    out1[done] = (c16_t){nr_prec2x2_sat16(a1r), nr_prec2x2_sat16(a1i)};
  }
}

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
  /* interleaved on ARMv8.1+, deinterleaved on ARMv8.0; see the PREC_* definitions above */
  load_consts_arm(0);
  if (n_layers == 1) {
    for (; out < end; out += sizeof(int16x8_t) / sizeof(*out)) {
      const PREC_ACC_T y = PREC_MAC0(in0, wr0, wi0);
      in0 += 8;
      // Store the result to txdataF
      PREC_STORE(out, y);
    }
  }
  if (n_layers == 2) {
    load_consts_arm(1);
    for (; out < end; out += sizeof(int16x8_t) / sizeof(*out)) {
      PREC_ACC_T y = PREC_MAC0(in0, wr0, wi0);
      in0 += 8;
      y = PREC_MAC(y, in1, wr1, wi1);
      in1 += 8;
      // Store the result to txdataF
      PREC_STORE(out, y);
    }
  }
  if (n_layers == 3) {
    load_consts_arm(1);
    load_consts_arm(2);
    for (; out < end; out += sizeof(int16x8_t) / sizeof(*out)) {
      PREC_ACC_T y = PREC_MAC0(in0, wr0, wi0);
      in0 += 8;
      y = PREC_MAC(y, in1, wr1, wi1);
      in1 += 8;
      y = PREC_MAC(y, in2, wr2, wi2);
      in2 += 8;
      // Store the result to txdataF
      PREC_STORE(out, y);
    }
  }
  if (n_layers == 4) {
    load_consts_arm(1);
    load_consts_arm(2);
    load_consts_arm(3);
    for (; out < end; out += sizeof(int16x8_t) / sizeof(*out)) {
      PREC_ACC_T y = PREC_MAC0(in0, wr0, wi0);
      in0 += 8;
      y = PREC_MAC(y, in1, wr1, wi1);
      in1 += 8;
      y = PREC_MAC(y, in2, wr2, wi2);
      in2 += 8;
      y = PREC_MAC(y, in3, wr3, wi3);
      in3 += 8;
      // Store the result to txdataF
      PREC_STORE(out, y);
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
