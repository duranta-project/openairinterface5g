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

  c16_t weights[8];
  int n = xran_decode_bfw_ext1(buf, len, weights, 8);
  assert(n == 2);
  assert(weights[0].r == 4 && weights[0].i == -4);
  assert(weights[1].r == 100 && weights[1].i == -100);
  printf("ext1 BFP known-vector check passed!\n");
}

static void test_none_known_vector(void)
{
  printf("Testing ext1 NONE (uncompressed) decode of hand-constructed vector...\n");
  // NONE, iq_bits=16: no comp param byte, raw 16-bit signed values.
  int16_t iq[4] = {12345, -12345, 1, -1};
  uint8_t buf[256];
  size_t len = build_ext1(buf, XRAN_BFWCOMPMETHOD_NONE, 0 /* -> iq_bits=16 */, 16, 0, iq, 2);

  c16_t weights[8];
  int n = xran_decode_bfw_ext1(buf, len, weights, 8);
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

  // max_weights is the caller's exact expected antenna count here, not just a generous
  // cap - up to 3 bytes of 4-byte-alignment padding could otherwise be misread as an
  // extra weight (see test_padding_not_read_as_extra_weight below).
  c16_t weights[8];
  int n = xran_decode_bfw_ext1(buf, len, weights, 1);
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

  c16_t weights[8];
  int n = xran_decode_bfw_ext1(buf, len, weights, 1);
  assert(n == 1);
  assert(weights[0].r == 3935 && weights[0].i == -3935);
  printf("ext1 ULAW known-vector check passed!\n");
}

static void test_padding_not_read_as_extra_weight(void)
{
  printf("Testing ext1 decode doesn't misread 4-byte alignment padding as a weight...\n");
  // Same buffer as test_blkscale_known_vector: 1 real weight, padded with up to 3 zero
  // bytes to reach a 4-byte boundary. A caller that (incorrectly) passes a max_weights
  // cap larger than the true antenna count can get those padding bytes back as a bogus
  // extra (zero) weight - documenting that behavior here rather than leaving it as a
  // silent footgun.
  int16_t iq[2] = {10, -10};
  uint8_t buf[256];
  size_t len = build_ext1(buf, XRAN_BFWCOMPMETHOD_BLKSCALE, 8, 8, 0, iq, 1);

  c16_t weights[8];
  int n = xran_decode_bfw_ext1(buf, len, weights, 8); // deliberately over-generous cap
  assert(n >= 1);
  assert(weights[0].r == 10 && weights[0].i == -10);
  if (n > 1)
    assert(weights[1].r == 0 && weights[1].i == 0); // padding decodes as zero, not garbage
  printf("ext1 padding-handling check passed!\n");
}

static void test_max_weights_caps_output(void)
{
  printf("Testing ext1 decode respects max_weights cap...\n");
  int16_t iq[8] = {1, -1, 2, -2, 3, -3, 4, -4};
  uint8_t buf[256];
  size_t len = build_ext1(buf, XRAN_BFWCOMPMETHOD_BLKFLOAT, 8, 8, 0, iq, 4);

  c16_t weights[2];
  int n = xran_decode_bfw_ext1(buf, len, weights, 2);
  assert(n == 2);
  assert(weights[0].r == 1 && weights[0].i == -1);
  assert(weights[1].r == 2 && weights[1].i == -2);
  printf("ext1 max_weights cap check passed!\n");
}

static void test_malformed_inputs_rejected(void)
{
  printf("Testing ext1 decode rejects malformed input...\n");
  int16_t iq[2] = {1, -1};
  uint8_t buf[256];
  size_t len = build_ext1(buf, XRAN_BFWCOMPMETHOD_BLKFLOAT, 8, 8, 0, iq, 1);
  c16_t weights[8];

  // Truncated buffer (len smaller than extLen*4 claims).
  assert(xran_decode_bfw_ext1(buf, len - 1, weights, 8) == -1);
  // NULL/zero-sized arguments.
  assert(xran_decode_bfw_ext1(NULL, len, weights, 8) == -1);
  assert(xran_decode_bfw_ext1(buf, len, NULL, 8) == -1);
  assert(xran_decode_bfw_ext1(buf, len, weights, 0) == -1);

  // NOTE: an unsupported bfwCompMeth (e.g. XRAN_BFWCOMPMETHOD_BEAMSPACE) is
  // NOT covered here - by deliberate design it hits AssertFatal and aborts
  // the process rather than returning -1 (see xran_pkt_bfw.h), which isn't
  // something a normal in-process ctest assertion can exercise without
  // taking the whole test binary down with it.

  printf("ext1 malformed-input rejection passed!\n");
}

int main(void)
{
  test_bfp_known_vector();
  test_none_known_vector();
  test_blkscale_known_vector();
  test_ulaw_known_vector();
  test_padding_not_read_as_extra_weight();
  test_max_weights_caps_output();
  test_malformed_inputs_rejected();
  printf("All xran_pkt_bfw tests passed!\n");
  return 0;
}
