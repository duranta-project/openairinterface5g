/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "fh_compression.h"
#include "assertions.h"
#include <limits.h>
#include <string.h>

/* ---- bit-stream helpers ---- */

static void pack_bits(uint8_t *stream, int offset, int32_t val, int width)
{
  uint32_t bits = (uint32_t)(val & ((1u << width) - 1));
  for (int i = 0; i < width; i++) {
    int bit_offset = offset + i;
    int pos = bit_offset >> 3;
    int shift = 7 - (bit_offset & 7);
    stream[pos] |= (uint8_t)(((bits >> (width - 1 - i)) & 1u) << shift);
  }
}

int32_t unpack_bits(const uint8_t *stream, int offset, int width)
{
  uint32_t bits = 0;
  for (int i = 0; i < width; i++) {
    int bit_offset = offset + i;
    int pos = bit_offset >> 3;
    int shift = 7 - (bit_offset & 7);
    bits = (bits << 1) | ((stream[pos] >> shift) & 1u);
  }
  uint32_t sign = 1u << (width - 1);
  return (bits & sign) ? (int32_t)(bits | ~((1u << width) - 1)) : (int32_t)bits;
}

/* ---- BFP ---- */

static uint16_t saturating_abs16(int16_t v)
{
  if (v == INT16_MIN)
    return INT16_MAX;
  return (uint16_t)(v < 0 ? -v : v);
}

static int leading_zero_count16(uint16_t v)
{
  int count = 0;
  for (int bit = 15; bit >= 0; bit--) {
    if (v & (1u << bit))
      break;
    count++;
  }
  return count;
}

static void bfp_compress_prb(const int16_t *src, int8_t *dst, int iq_bits)
{
  uint16_t max_abs = 1;
  for (int i = 0; i < FH_VALS_PER_PRB; i++) {
    uint16_t v = saturating_abs16(src[i]);
    if (v > max_abs)
      max_abs = v;
  }
  int exponent = 16 - iq_bits + 1 - leading_zero_count16(max_abs);
  if (exponent < 0)
    exponent = 0;
  dst[0] = (int8_t)exponent;
  uint8_t *out = (uint8_t *)(dst + 1);
  memset(out, 0, 3 * iq_bits);
  for (int i = 0; i < FH_VALS_PER_PRB; i++)
    pack_bits(out, i * iq_bits, src[i] >> exponent, iq_bits);
}

static void bfp_decompress_block(const int8_t *src, int16_t *dst, int iq_bits, int n_vals)
{
  int exponent = (int)(uint8_t)src[0];
  const uint8_t *in = (const uint8_t *)(src + 1);
  for (int i = 0; i < n_vals; i++)
    dst[i] = (int16_t)(unpack_bits(in, i * iq_bits, iq_bits) << exponent);
}

/* ---- BLKSCALE ---- */

static void blkscale_compress_prb(const int16_t *src, int8_t *dst, int iq_bits)
{
  int32_t max_abs = 1;
  for (int i = 0; i < FH_VALS_PER_PRB; i++) {
    int32_t v = src[i] < 0 ? -(int32_t)src[i] : (int32_t)src[i];
    if (v > max_abs)
      max_abs = v;
  }
  int bit_width = 32 - __builtin_clz((uint32_t)max_abs);
  int shift = bit_width - (iq_bits - 1);
  if (shift < 0)
    shift = 0;
  dst[0] = (int8_t)shift;
  uint8_t *out = (uint8_t *)(dst + 1);
  memset(out, 0, 3 * iq_bits);
  const int32_t clip_max = (1 << (iq_bits - 1)) - 1;
  const int32_t clip_min = -(1 << (iq_bits - 1));
  for (int i = 0; i < FH_VALS_PER_PRB; i++) {
    int32_t val = src[i] >> shift;
    if (val > clip_max)
      val = clip_max;
    else if (val < clip_min)
      val = clip_min;
    pack_bits(out, i * iq_bits, val, iq_bits);
  }
}

static void blkscale_decompress_block(const int8_t *src, int16_t *dst, int iq_bits, int n_vals)
{
  int shift = (int)(uint8_t)src[0];
  const uint8_t *in = (const uint8_t *)(src + 1);
  for (int i = 0; i < n_vals; i++)
    dst[i] = (int16_t)(unpack_bits(in, i * iq_bits, iq_bits) << shift);
}

/* ---- ULAW ---- */

/* G.711 mu-law constants (ITU-T G.711) */
#define ULAW_BIAS      33    /* additive bias before segmentation */
#define ULAW_CLIP      32734 /* saturation level: INT16_MAX - ULAW_BIAS */
#define ULAW_NORM      127   /* segment normalization factor */
#define ULAW_SEG_MASK  0x07  /* 3-bit segment index */

static int16_t ulaw_encode(int16_t s, int iq_bits)
{
  int sign = s < 0 ? -1 : 1;
  int x = s < 0 ? -(int)s : (int)s;
  x = x > ULAW_CLIP ? ULAW_CLIP : x;
  x += ULAW_BIAS;
  int seg = 0;
  for (int t = x >> 5; t > 1 && seg < 7; t >>= 1)
    seg++;
  int code = (((x >> (seg + 1)) & 0x0F) | (seg << 4)) ^ 0x7F;
  int peak = (1 << (iq_bits - 1)) - 1;
  return (int16_t)(sign * (code * peak / ULAW_NORM));
}

static int16_t ulaw_decode(int16_t s, int iq_bits)
{
  int sign = s < 0 ? -1 : 1;
  int x = s < 0 ? -(int)s : (int)s;
  int peak = (1 << (iq_bits - 1)) - 1;
  if (peak == 0)
    return 0;
  int code = x * ULAW_NORM / peak;
  code ^= 0x7F;
  int seg = (code >> 4) & ULAW_SEG_MASK;
  int decoded = (((code & 0x0F) << 1) | 1) << (seg + 2);
  decoded -= ULAW_BIAS;
  if (decoded < 0)
    decoded = 0;
  return (int16_t)(sign * decoded);
}

static void ulaw_compress_prb(const int16_t *src, int8_t *dst, int iq_bits)
{
  dst[0] = 0;
  uint8_t *out = (uint8_t *)(dst + 1);
  memset(out, 0, 3 * iq_bits);
  for (int i = 0; i < FH_VALS_PER_PRB; i++)
    pack_bits(out, i * iq_bits, ulaw_encode(src[i], iq_bits), iq_bits);
}

static void ulaw_decompress_block(const int8_t *src, int16_t *dst, int iq_bits, int n_vals)
{
  const uint8_t *in = (const uint8_t *)(src + 1);
  for (int i = 0; i < n_vals; i++)
    dst[i] = ulaw_decode((int16_t)unpack_bits(in, i * iq_bits, iq_bits), iq_bits);
}

/* ---- public API ---- */

void fh_compress_prbs(fh_comp_method_t method, int iq_bits, int n_prb, const int16_t *src, int8_t *dst)
{
  AssertFatal(method != FH_COMP_NONE, "fh_compress_prbs called with FH_COMP_NONE\n");
  AssertFatal(iq_bits >= 1 && iq_bits <= 16, "iq_bits %d out of range [1..16]\n", iq_bits);
  const int src_stride = FH_VALS_PER_PRB;
  const int dst_stride = FH_COMP_PRB_BYTES(iq_bits);
  for (int p = 0; p < n_prb; p++) {
    switch (method) {
      case FH_COMP_BFP:
        bfp_compress_prb(src + p * src_stride, dst + p * dst_stride, iq_bits);
        break;
      case FH_COMP_BLKSCALE:
        blkscale_compress_prb(src + p * src_stride, dst + p * dst_stride, iq_bits);
        break;
      case FH_COMP_ULAW:
        ulaw_compress_prb(src + p * src_stride, dst + p * dst_stride, iq_bits);
        break;
      default:
        AssertFatal(0, "Unsupported compression method %d\n", method);
    }
  }
}

void fh_decompress_prbs(fh_comp_method_t method, int iq_bits, int n_prb, const int8_t *src, int16_t *dst)
{
  const int src_stride = FH_COMP_PRB_BYTES(iq_bits);
  const int dst_stride = FH_VALS_PER_PRB;
  for (int p = 0; p < n_prb; p++)
    fh_decompress_block(method, iq_bits, FH_VALS_PER_PRB, src + p * src_stride, dst + p * dst_stride);
}

void fh_decompress_block(fh_comp_method_t method, int iq_bits, int n_vals, const int8_t *src, int16_t *dst)
{
  AssertFatal(method != FH_COMP_NONE, "fh_decompress_block called with FH_COMP_NONE\n");
  AssertFatal(iq_bits >= 1 && iq_bits <= 16, "iq_bits %d out of range [1..16]\n", iq_bits);
  switch (method) {
    case FH_COMP_BFP:
      bfp_decompress_block(src, dst, iq_bits, n_vals);
      break;
    case FH_COMP_BLKSCALE:
      blkscale_decompress_block(src, dst, iq_bits, n_vals);
      break;
    case FH_COMP_ULAW:
      ulaw_decompress_block(src, dst, iq_bits, n_vals);
      break;
    default:
      AssertFatal(0, "Unsupported compression method %d\n", method);
  }
}

void fh_compress_prach(fh_comp_method_t method, int iq_bits, int kbar, const int16_t *src, int8_t *dst)
{
  const int total_vals = FH_PRACH_NUM_PRBS * FH_VALS_PER_PRB;
  int16_t padded[FH_PRACH_NUM_PRBS * FH_VALS_PER_PRB];
  AssertFatal(kbar >= 0 && kbar + FH_PRACH_NUM_SUBCARRIERS * 2 <= total_vals, "PRACH kbar %d out of range\n", kbar);
  memset(padded, 0, total_vals * sizeof(int16_t));
  memcpy(padded + kbar, src, FH_PRACH_NUM_SUBCARRIERS * 2 * sizeof(int16_t));
  fh_compress_prbs(method, iq_bits, FH_PRACH_NUM_PRBS, padded, dst);
}
