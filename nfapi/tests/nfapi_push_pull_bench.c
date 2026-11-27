/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* Benchmark for nFAPI push/pull serialisation functions.
 *
 * Run this at any point in the git history to measure throughput.
 * Compare two runs (before and after optimisation commits) manually.
 *
 * Build: included in the test suite when ENABLE_TESTS=ON.
 * Run:   ./nfapi_push_pull_bench
 */

#include "nfapi/tests/nr_fapi_test.h"
#include "nfapi.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/* ---- sizing ---- */
#define SCALAR_WORDS  1024
#define SCALAR_ITERS  5000000
#define ARRAY_WORDS   38016   /* ~152 KB as uint32_t, matches max TLV payload */
#define ARRAY_ITERS   10000

static double now_sec(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static void print_row(const char *name, double elapsed, double bytes)
{
  printf("  %-20s  %8.3f s   %7.2f GB/s\n", name, elapsed, bytes / elapsed / 1e9);
}

/* ---- scalar benchmarks ---- */

static double bench_push16(uint16_t *src, uint8_t *wire, size_t cap)
{
  uint8_t *end = wire + cap;
  double t0 = now_sec();
  for (int it = 0; it < SCALAR_ITERS; it++) {
    uint8_t *p = wire;
    for (int i = 0; i < SCALAR_WORDS; i++)
      push16(src[i], &p, end);
  }
  return now_sec() - t0;
}

static double bench_pull16(uint8_t *wire, size_t cap, uint16_t *dst)
{
  uint8_t *end = wire + cap;
  double t0 = now_sec();
  for (int it = 0; it < SCALAR_ITERS; it++) {
    uint8_t *p = wire;
    for (int i = 0; i < SCALAR_WORDS; i++)
      pull16(&p, &dst[i % SCALAR_WORDS], end);
  }
  return now_sec() - t0;
}

static double bench_push32(uint32_t *src, uint8_t *wire, size_t cap)
{
  uint8_t *end = wire + cap;
  double t0 = now_sec();
  for (int it = 0; it < SCALAR_ITERS; it++) {
    uint8_t *p = wire;
    for (int i = 0; i < SCALAR_WORDS; i++)
      push32(src[i], &p, end);
  }
  return now_sec() - t0;
}

static double bench_pull32(uint8_t *wire, size_t cap, uint32_t *dst)
{
  uint8_t *end = wire + cap;
  double t0 = now_sec();
  for (int it = 0; it < SCALAR_ITERS; it++) {
    uint8_t *p = wire;
    for (int i = 0; i < SCALAR_WORDS; i++)
      pull32(&p, &dst[i % SCALAR_WORDS], end);
  }
  return now_sec() - t0;
}

/* ---- array benchmarks ---- */

static double bench_pusharray16(uint16_t *src, uint8_t *wire, size_t cap)
{
  uint8_t *end = wire + cap;
  double t0 = now_sec();
  for (int it = 0; it < ARRAY_ITERS; it++) {
    uint8_t *p = wire;
    pusharray16(src, ARRAY_WORDS, ARRAY_WORDS, &p, end);
  }
  return now_sec() - t0;
}

static double bench_pullarray16(uint8_t *wire, size_t cap, uint16_t *dst)
{
  uint8_t *end = wire + cap;
  double t0 = now_sec();
  for (int it = 0; it < ARRAY_ITERS; it++) {
    uint8_t *p = wire;
    pullarray16(&p, dst, ARRAY_WORDS, ARRAY_WORDS, end);
  }
  return now_sec() - t0;
}

static double bench_pusharray32(uint32_t *src, uint8_t *wire, size_t cap)
{
  uint8_t *end = wire + cap;
  double t0 = now_sec();
  for (int it = 0; it < ARRAY_ITERS; it++) {
    uint8_t *p = wire;
    pusharray32(src, ARRAY_WORDS, ARRAY_WORDS, &p, end);
  }
  return now_sec() - t0;
}

static double bench_pullarray32(uint8_t *wire, size_t cap, uint32_t *dst)
{
  uint8_t *end = wire + cap;
  double t0 = now_sec();
  for (int it = 0; it < ARRAY_ITERS; it++) {
    uint8_t *p = wire;
    pullarray32(&p, dst, ARRAY_WORDS, ARRAY_WORDS, end);
  }
  return now_sec() - t0;
}

static int check_roundtrip(void)
{
  uint8_t buf[4];
  uint8_t *p;
  uint16_t v16 = 0xABCD, r16;
  uint32_t v32 = 0xDEADBEEF, r32;
  uint16_t arr16_in[] = {0x0000, 0x8000, 0xFFFF, 0x1234};
  uint16_t arr16_out[4];
  uint32_t arr32_in[] = {0x00000000, 0x80000000, 0xFFFFFFFF, 0x12345678};
  uint32_t arr32_out[4];
  static uint8_t wire[16];

  p = buf; if (push16(v16, &p, buf + 2) != 2) return 1;
  p = buf; if (pull16(&p, &r16, buf + 2) != 2 || r16 != v16) return 1;

  p = buf; if (push32(v32, &p, buf + 4) != 4) return 1;
  p = buf; if (pull32(&p, &r32, buf + 4) != 4 || r32 != v32) return 1;

  p = wire;
  if (pusharray16(arr16_in, 4, 4, &p, wire + sizeof(wire)) != 8) return 1;
  p = wire;
  if (pullarray16(&p, arr16_out, 4, 4, wire + sizeof(wire)) != 8) return 1;
  for (int i = 0; i < 4; i++) if (arr16_out[i] != arr16_in[i]) return 1;

  p = wire;
  if (pusharray32(arr32_in, 4, 4, &p, wire + sizeof(wire)) != 16) return 1;
  p = wire;
  if (pullarray32(&p, arr32_out, 4, 4, wire + sizeof(wire)) != 16) return 1;
  for (int i = 0; i < 4; i++) if (arr32_out[i] != arr32_in[i]) return 1;

  return 0;
}

int main(void)
{
  if (check_roundtrip()) {
    fprintf(stderr, "push/pull correctness check failed\n");
    return 1;
  }

  /* detect wire endianness at runtime */
  {
    uint8_t probe[2]; uint8_t *p = probe;
    push16(0x0102, &p, probe + 2);
    const int swap_needed = (probe[0] != 0x02);
    printf("Wire format: %s (byte swap %s)\n\n",
           swap_needed ? "big-endian" : "little-endian",
           swap_needed ? "needed" : "not needed");
  }

  /* ---- allocate buffers ---- */
  static uint16_t src16[SCALAR_WORDS], dst16[SCALAR_WORDS];
  static uint32_t src32[SCALAR_WORDS], dst32[SCALAR_WORDS];
  static uint16_t asrc16[ARRAY_WORDS], adst16[ARRAY_WORDS];
  static uint32_t asrc32[ARRAY_WORDS], adst32[ARRAY_WORDS];

  const size_t scalar16_cap = SCALAR_WORDS * 2 + 8;
  const size_t scalar32_cap = SCALAR_WORDS * 4 + 8;
  const size_t array16_cap  = (size_t)ARRAY_WORDS * 2 + 8;
  const size_t array32_cap  = (size_t)ARRAY_WORDS * 4 + 8;

  uint8_t *ws16 = malloc(scalar16_cap);
  uint8_t *ws32 = malloc(scalar32_cap);
  uint8_t *wa16 = malloc(array16_cap);
  uint8_t *wa32 = malloc(array32_cap);

  /* fill with pseudo-random data */
  uint32_t x = 0xdeadbeef;
  for (int i = 0; i < SCALAR_WORDS; i++) {
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    src16[i] = (uint16_t)x;
    src32[i] = x;
  }
  for (int i = 0; i < ARRAY_WORDS; i++) {
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    asrc16[i] = (uint16_t)x;
    asrc32[i] = x;
  }

  /* pre-fill wire buffers for pull benchmarks */
  { uint8_t *p = ws16; uint8_t *e = ws16 + scalar16_cap;
    for (int i = 0; i < SCALAR_WORDS; i++) push16(src16[i], &p, e); }
  { uint8_t *p = ws32; uint8_t *e = ws32 + scalar32_cap;
    for (int i = 0; i < SCALAR_WORDS; i++) push32(src32[i], &p, e); }
  { uint8_t *p = wa16;
    pusharray16(asrc16, ARRAY_WORDS, ARRAY_WORDS, &p, wa16 + array16_cap); }
  { uint8_t *p = wa32;
    pusharray32(asrc32, ARRAY_WORDS, ARRAY_WORDS, &p, wa32 + array32_cap); }

  /* warm up */
  bench_push32(src32, ws32, scalar32_cap);
  bench_pusharray32(asrc32, wa32, array32_cap);

  /* ---- report ---- */
  const double scalar16_bytes = (double)SCALAR_WORDS * 2 * SCALAR_ITERS;
  const double scalar32_bytes = (double)SCALAR_WORDS * 4 * SCALAR_ITERS;
  const double array16_bytes  = (double)ARRAY_WORDS  * 2 * ARRAY_ITERS;
  const double array32_bytes  = (double)ARRAY_WORDS  * 4 * ARRAY_ITERS;

  printf("nFAPI push/pull benchmark  (%d-word scalar x %d iters | %d-word array x %d iters)\n\n",
         SCALAR_WORDS, SCALAR_ITERS, ARRAY_WORDS, ARRAY_ITERS);

  printf("Scalar 16-bit:\n");
  print_row("push16",      bench_push16(src16, ws16, scalar16_cap), scalar16_bytes);
  print_row("pull16",      bench_pull16(ws16, scalar16_cap, dst16), scalar16_bytes);

  printf("Scalar 32-bit:\n");
  print_row("push32",      bench_push32(src32, ws32, scalar32_cap), scalar32_bytes);
  print_row("pull32",      bench_pull32(ws32, scalar32_cap, dst32), scalar32_bytes);

  printf("Array 16-bit:\n");
  print_row("pusharray16", bench_pusharray16(asrc16, wa16, array16_cap), array16_bytes);
  print_row("pullarray16", bench_pullarray16(wa16, array16_cap, adst16), array16_bytes);

  printf("Array 32-bit:\n");
  print_row("pusharray32", bench_pusharray32(asrc32, wa32, array32_cap), array32_bytes);
  print_row("pullarray32", bench_pullarray32(wa32, array32_cap, adst32), array32_bytes);

  free(ws16); free(ws32); free(wa16); free(wa32);
  return 0;
}
