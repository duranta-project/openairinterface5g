/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "xran_pkt_bfw.h"
#include "xran_pkt_cp.h"

void exit_function(const char *file, const char *function, const int line, const char *s, const int assertflag)
{
  fprintf(stderr, "Error at %s:%s:%d - %s\n", file, function, line, s ? s : "None");
  exit(1);
}

// Builds a well-formed ext1 buffer by hand: 3-byte fixed header, optional 1-byte
// comp param, then n_weights (bfwI, bfwQ) pairs packed at iq_bits width, zero-padded
// to a 4-byte boundary. Returns the total length written.
static size_t build_ext1(uint8_t *buf,
                         uint8_t comp_meth,
                         uint8_t iq_bits_field,
                         int iq_bits,
                         uint8_t comp_param,
                         const int16_t *iq_pairs,
                         int n_weights)
{
  memset(buf, 0, 256);
  buf[0] = 1; // extType=1, ef=0
  buf[2] = (uint8_t)((comp_meth & 0x0F) | ((iq_bits_field & 0x0F) << 4));

  size_t bit_offset = 24; // after the 3 fixed header bytes
  bool has_comp_param = comp_meth != 0;
  if (has_comp_param) {
    buf[3] = comp_param;
    bit_offset += 8;
  }
  for (int i = 0; i < 2 * n_weights; i++) {
    int32_t v = iq_pairs[i];
    uint32_t bits = (uint32_t)(v & ((1 << iq_bits) - 1));
    for (int b = 0; b < iq_bits; b++) {
      size_t bo = bit_offset + i * iq_bits + b;
      int pos = bo / 8;
      int shift = 7 - (bo % 8);
      buf[pos] |= (uint8_t)(((bits >> (iq_bits - 1 - b)) & 1u) << shift);
    }
  }
  size_t total_bits = bit_offset + (size_t)2 * n_weights * iq_bits;
  size_t total_bytes = (total_bits + 7) / 8;
  size_t padded = ((total_bytes + 3) / 4) * 4;
  buf[1] = (uint8_t)(padded / 4);
  return padded;
}

static void test_bfp_known_vector(void)
{
  printf("Testing ext1 BFP decode of hand-constructed vector...\n");
  // BFP, iq_bits=8, exponent=0: weights (I=4,Q=-4), (I=100,Q=-100).
  int16_t iq[4] = {4, -4, 100, -100};
  uint8_t buf[256];
  size_t len = build_ext1(buf, XRAN_BFWCOMPMETHOD_BLKFLOAT, 8, 8, 0 /* exponent */, iq, 2);

  c16_t weights[2];
  int n = xran_decode_bfw_ext1(buf, len, 2, weights);
  assert(n == 2);
  assert(weights[0].r == 4 && weights[0].i == -4);
  assert(weights[1].r == 100 && weights[1].i == -100);
  printf("ext1 BFP known-vector check passed!\n");
}

static void test_bfp_odd_width_known_vector(void)
{
  printf("Testing ext1 BFP decode with 9-bit values crossing byte boundaries...\n");
  // BFP, iq_bits=9, exponent=0: 3 weights -> 54 bits of payload, not byte aligned.
  int16_t iq[6] = {255, -256, 1, -1, -100, 77};
  uint8_t buf[256];
  size_t len = build_ext1(buf, XRAN_BFWCOMPMETHOD_BLKFLOAT, 9, 9, 0, iq, 3);

  c16_t weights[3];
  int n = xran_decode_bfw_ext1(buf, len, 3, weights);
  assert(n == 3);
  for (int i = 0; i < 3; i++)
    assert(weights[i].r == iq[2 * i] && weights[i].i == iq[2 * i + 1]);
  printf("ext1 BFP 9-bit check passed!\n");
}

static void test_none_known_vector(void)
{
  printf("Testing ext1 NONE (uncompressed) decode of hand-constructed vector...\n");
  // NONE, iq_bits=16: no comp param byte, raw 16-bit signed values.
  int16_t iq[4] = {12345, -12345, 1, -1};
  uint8_t buf[256];
  size_t len = build_ext1(buf, XRAN_BFWCOMPMETHOD_NONE, 0 /* -> iq_bits=16 */, 16, 0, iq, 2);

  c16_t weights[2];
  int n = xran_decode_bfw_ext1(buf, len, 2, weights);
  assert(n == 2);
  assert(weights[0].r == 12345 && weights[0].i == -12345);
  assert(weights[1].r == 1 && weights[1].i == -1);
  printf("ext1 NONE known-vector check passed!\n");
}

static void test_blkscale_known_vector(void)
{
  printf("Testing ext1 BLKSCALE decode of hand-constructed vector...\n");
  // BLKSCALE, iq_bits=8, shift=0: weights (I=10,Q=-10).
  int16_t iq[2] = {10, -10};
  uint8_t buf[256];
  size_t len = build_ext1(buf, XRAN_BFWCOMPMETHOD_BLKSCALE, 8, 8, 0 /* shift */, iq, 1);

  c16_t weights[1];
  int n = xran_decode_bfw_ext1(buf, len, 1, weights);
  assert(n == 1);
  assert(weights[0].r == 10 && weights[0].i == -10);
  printf("ext1 BLKSCALE known-vector check passed!\n");
}

static void test_ulaw_known_vector(void)
{
  printf("Testing ext1 ULAW decode of hand-constructed vector...\n");
  // ULAW, iq_bits=8: raw packed values (I=32,Q=-32) feed ulaw_decode()'s mu-law expansion.
  // Hand-computed per fh_compression.c's ulaw_decode(): x=32, code = 32*127/127 = 32,
  // code ^= 0x7F -> 95, seg = (95>>4)&7 = 5, decoded = (((95&0xF)<<1)|1)<<(5+2) = 31<<7 = 3968,
  // decoded -= 33 (ULAW_BIAS) -> 3935. Sign follows the raw input's sign.
  int16_t iq[2] = {32, -32};
  uint8_t buf[256];
  size_t len = build_ext1(buf, XRAN_BFWCOMPMETHOD_ULAW, 8, 8, 0 /* comp param unused by ulaw decode */, iq, 1);

  c16_t weights[1];
  int n = xran_decode_bfw_ext1(buf, len, 1, weights);
  assert(n == 1);
  assert(weights[0].r == 3935 && weights[0].i == -3935);
  printf("ext1 ULAW known-vector check passed!\n");
}

static void test_weight_count_mismatch_rejected(void)
{
  printf("Testing ext1 decode rejects a weight count that doesn't match extLen...\n");
  // 1 weight (3 hdr + 1 param + 2 IQ bytes) padded to 8 bytes. The decoder only ever produces
  // the caller's configured count, and a count whose padded size differs from extLen is rejected.
  // (A count that fits in the padding, here 2, gives the same extLen and can't be told apart on
  // the wire, which is why the count comes from configuration and not from extLen.)
  int16_t iq[2] = {10, -10};
  uint8_t buf[256];
  size_t len = build_ext1(buf, XRAN_BFWCOMPMETHOD_BLKSCALE, 8, 8, 0, iq, 1);
  assert(len == 8);

  c16_t weights[8];
  memset(weights, 0x5A, sizeof(weights));
  assert(xran_decode_bfw_ext1(buf, len, 3, weights) == -1);
  assert(xran_decode_bfw_ext1(buf, len, 8, weights) == -1);
  // Nothing written on rejection.
  for (int i = 0; i < 8; i++)
    assert(weights[i].r == 0x5A5A && weights[i].i == 0x5A5A);

  // Fewer weights than carried is also a mismatch (4 weights = 12 bytes, 2 weights = 8 bytes).
  int16_t iq4[8] = {1, -1, 2, -2, 3, -3, 4, -4};
  len = build_ext1(buf, XRAN_BFWCOMPMETHOD_BLKFLOAT, 8, 8, 0, iq4, 4);
  assert(len == 12);
  assert(xran_decode_bfw_ext1(buf, len, 2, weights) == -1);
  assert(xran_decode_bfw_ext1(buf, len, 4, weights) == 4);
  printf("ext1 weight count mismatch rejection passed!\n");
}

static void test_malformed_inputs_rejected(void)
{
  printf("Testing ext1 decode rejects malformed input...\n");
  int16_t iq[2] = {1, -1};
  uint8_t buf[256];
  size_t len = build_ext1(buf, XRAN_BFWCOMPMETHOD_BLKFLOAT, 8, 8, 0, iq, 1);
  c16_t weights[1];

  // Truncated buffer (len smaller than extLen*4 claims).
  assert(xran_decode_bfw_ext1(buf, len - 1, 1, weights) == -1);
  // NULL/zero-sized arguments.
  assert(xran_decode_bfw_ext1(NULL, len, 1, weights) == -1);
  assert(xran_decode_bfw_ext1(buf, len, 1, NULL) == -1);
  assert(xran_decode_bfw_ext1(buf, len, 0, weights) == -1);
  assert(xran_decode_bfw_ext1(buf, 2, 1, weights) == -1);

  // extLen claiming more words than the weights need.
  buf[1]++;
  assert(xran_decode_bfw_ext1(buf, sizeof(buf), 1, weights) == -1);
  buf[1]--;
  // extLen = 0.
  uint8_t saved = buf[1];
  buf[1] = 0;
  assert(xran_decode_bfw_ext1(buf, len, 1, weights) == -1);
  buf[1] = saved;

  // Beamspace and reserved bfwCompMeth values come from the wire and must not abort.
  for (int meth = XRAN_BFWCOMPMETHOD_BEAMSPACE; meth <= 0xF; meth++) {
    buf[2] = (uint8_t)((meth & 0x0F) | (8 << 4));
    assert(xran_decode_bfw_ext1(buf, len, 1, weights) == -1);
  }
  printf("ext1 malformed-input rejection passed!\n");
}

int main(void)
{
  test_bfp_known_vector();
  test_bfp_odd_width_known_vector();
  test_none_known_vector();
  test_blkscale_known_vector();
  test_ulaw_known_vector();
  test_weight_count_mismatch_rejected();
  test_malformed_inputs_rejected();
  printf("All xran_pkt_bfw tests passed!\n");
  return 0;
}
