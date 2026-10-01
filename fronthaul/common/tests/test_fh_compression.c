/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "fh_compression.h"

void exit_function(const char *file, const char *function, const int line, const char *s, const int assertflag)
{
  fprintf(stderr, "Error at %s:%s:%d - %s\n", file, function, line, s ? s : "None");
  exit(1);
}

static void test_bfp_roundtrip(void)
{
  printf("Testing BFP compress/decompress round-trip...\n");
  const int n_prb = 1;
  const int iq_bits = 9;
  int16_t src[FH_VALS_PER_PRB];
  for (int i = 0; i < FH_VALS_PER_PRB; i += 2) {
    src[i] = 100;
    src[i + 1] = -200;
  }
  int8_t compressed[FH_COMP_PRB_BYTES(iq_bits)];
  memset(compressed, 0, sizeof(compressed));
  int16_t recovered[FH_VALS_PER_PRB];

  fh_compress_prbs(FH_COMP_BFP, iq_bits, n_prb, src, compressed);
  /* I=100, Q=-200: max_abs=200, lzc=8, exponent=16-9+1-8=0 -> exact round-trip */
  assert((uint8_t)compressed[0] == 0);
  fh_decompress_prbs(FH_COMP_BFP, iq_bits, n_prb, compressed, recovered);

  for (int i = 0; i < FH_VALS_PER_PRB; i++)
    assert(recovered[i] == src[i]);
  printf("BFP round-trip passed!\n");
}

static void test_bfp_known_vector(void)
{
  printf("Testing BFP decompression of hand-constructed vector...\n");
  /* For iq_bits=8, packed values land on byte boundaries so the compressed
   * bytes can be written by hand without bit-arithmetic.
   * exponent=0, I=4 (0x04), Q=-4 (0xFC) for all 12 subcarriers. */
  const int iq_bits = 8;
  const int n_prb = 1;
  int8_t known[FH_COMP_PRB_BYTES(iq_bits)]; /* 1 + 3*8 = 25 bytes */
  memset(known, 0, sizeof(known));
  known[0] = 0; /* exponent */
  for (int i = 0; i < FH_VALS_PER_PRB; i += 2) {
    known[1 + i] = 0x04;          /* I = 4 */
    known[1 + i + 1] = (int8_t)0xFC; /* Q = -4 in 8-bit two's complement */
  }
  int16_t recovered[FH_VALS_PER_PRB];
  fh_decompress_prbs(FH_COMP_BFP, iq_bits, n_prb, known, recovered);
  for (int i = 0; i < FH_VALS_PER_PRB; i += 2) {
    assert(recovered[i] == 4);
    assert(recovered[i + 1] == -4);
  }
  printf("BFP known-vector check passed!\n");
}

int main(void)
{
  test_bfp_roundtrip();
  test_bfp_known_vector();
  printf("All compression tests passed!\n");
  return 0;
}
