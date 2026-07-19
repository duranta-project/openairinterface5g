/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * RVV port harness -- kernel #9: write_task_output bit-packing (the DLSCH
 * "concatenation", nrLDPC_coding_segment_encoder.c). Largest gNB-TX encoding
 * cost on RISC-V (~444 us/slot) because it runs through SIMDe-scalar
 * movemask/slli.
 *
 * Per 32-byte block of the segment-interleaved encoder output, for each
 * segment j: extract bit-plane j (bit j of each of 32 bytes -> a 32-bit word,
 * bit k = byte k's bit j) and OR it into the concatenated output bitstream at
 * that segment's running bit offset. x86 does this with
 * movemask(slli_epi16(block, 7-j)); RVV does vsll + vmslt + vsm (mask store)
 * in ~3 ops.
 *
 * scalar / simde(exact default path) / rvv checked BYTE-FOR-BYTE on the packed
 * output buffer, and benchmarked.
 */

#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sched.h>
#include <simde/x86/avx2.h>
#include <simde/x86/mmx.h>

static int pin_to_cpu(int cpu)
{
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(cpu, &set);
  return sched_setaffinity(0, sizeof(set), &set);
}
static uint64_t rng = 0x853c49e6748fea9bULL;
static uint8_t rnd8(void)
{
  rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
  return (uint8_t)(rng >> 33);
}
static double now_ns(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

/* OR a 32-bit word into the output bitstream at bit offset (byte*32 + bit).
 * uint64 form is UB-free and bit-exact with the simde__m64 dance (incl bit==0). */
static inline void or_out(uint32_t *op, uint32_t tmp, uint32_t byte, uint32_t bit)
{
  uint64_t v = (uint64_t)tmp << bit;
  op[byte] |= (uint32_t)v;
  op[byte + 1] |= (uint32_t)(v >> 32);
}

/* ---- scalar ground truth --------------------------------------------------- */
static inline uint32_t bitplane_c(const uint8_t *p, int j)
{
  uint32_t r = 0;
  for (int k = 0; k < 32; k++)
    r |= (uint32_t)((p[k] >> j) & 1) << k;
  return r;
}
static void wto_scalar(const uint8_t *f, uint32_t E, const uint8_t *f2, uint32_t E2, uint32_t E2_first, uint32_t nseg, uint32_t *op, uint32_t Eoff)
{
  for (uint32_t i = 0; i < E2; i += 32) {
    uint32_t e2 = Eoff;
    if (i < E)
      for (uint32_t j = 0; j < E2_first; j++) {
        or_out(op, bitplane_c(f + i, j), e2 >> 5, e2 & 31);
        e2 += E;
      }
    else
      e2 += E * E2_first;
    for (uint32_t j = E2_first; j < nseg; j++) {
      or_out(op, bitplane_c(f2 + i, j), e2 >> 5, e2 & 31);
      e2 += E2;
    }
    op++;
  }
}

/* ---- SIMDe baseline: exact default (AVX2) path ----------------------------- */
static void wto_simde(const uint8_t *f, uint32_t E, const uint8_t *f2, uint32_t E2, uint32_t E2_first, uint32_t nseg, uint32_t *output_p, uint32_t Eoffset)
{
  for (uint32_t i = 0; i < E2; i += 32) {
    uint32_t Eoffset2 = Eoffset;
    if (i < E) {
      for (uint32_t j = 0; j < E2_first; j++) {
        uint32_t b = Eoffset2 >> 5, bit = Eoffset2 & 31;
        int tmp = simde_mm256_movemask_epi8(simde_mm256_slli_epi16(((simde__m256i *)f)[i >> 5], 7 - j));
        simde__m64 tmp64 = simde_mm_set1_pi32(tmp);
        simde__m64 out64 = simde_mm_set_pi32(*(output_p + b + 1), *(output_p + b));
        simde__m64 tmp64b = simde_mm_or_si64(out64, simde_mm_slli_pi32(tmp64, bit));
        simde__m64 tmp64c = simde_mm_or_si64(out64, simde_mm_srli_pi32(tmp64, (32 - bit)));
        *(output_p + b) = simde_m_to_int(tmp64b);
        *(output_p + b + 1) = simde_m_to_int(simde_mm_srli_si64(tmp64c, 32));
        Eoffset2 += E;
      }
    } else {
      Eoffset2 += E * E2_first;
    }
    for (uint32_t j = E2_first; j < nseg; j++) {
      uint32_t b = Eoffset2 >> 5, bit = Eoffset2 & 31;
      int tmp = simde_mm256_movemask_epi8(simde_mm256_slli_epi16(((simde__m256i *)f2)[i >> 5], 7 - j));
      simde__m64 tmp64 = simde_mm_set1_pi32(tmp);
      simde__m64 out64 = simde_mm_set_pi32(*(output_p + b + 1), *(output_p + b));
      simde__m64 tmp64b = simde_mm_or_si64(out64, simde_mm_slli_pi32(tmp64, bit));
      simde__m64 tmp64c = simde_mm_or_si64(out64, simde_mm_srli_pi32(tmp64, (32 - bit)));
      *(output_p + b) = simde_m_to_int(tmp64b);
      *(output_p + b + 1) = simde_m_to_int(simde_mm_srli_si64(tmp64c, 32));
      Eoffset2 += E2;
    }
    output_p++;
  }
}

/* ---- RVV ------------------------------------------------------------------- */
#if defined(__riscv) && defined(__riscv_vector)
#include <riscv_vector.h>
static inline uint32_t bitplane_rvv(const uint8_t *p, int j)
{
  size_t vl = __riscv_vsetvl_e8m1(32);
  vint8m1_t v = __riscv_vsll_vx_i8m1(__riscv_vle8_v_i8m1((const int8_t *)p, vl), 7 - j, vl);
  vbool8_t m = __riscv_vmslt_vx_i8m1_b8(v, 0, vl); /* MSB (bit j) of each byte */
  uint32_t out = 0;
  __riscv_vsm_v_b8((uint8_t *)&out, m, vl); /* pack vl mask bits -> 32-bit word */
  return out;
}
static void wto_rvv(const uint8_t *f, uint32_t E, const uint8_t *f2, uint32_t E2, uint32_t E2_first, uint32_t nseg, uint32_t *op, uint32_t Eoff)
{
  for (uint32_t i = 0; i < E2; i += 32) {
    uint32_t e2 = Eoff;
    if (i < E)
      for (uint32_t j = 0; j < E2_first; j++) {
        or_out(op, bitplane_rvv(f + i, j), e2 >> 5, e2 & 31);
        e2 += E;
      }
    else
      e2 += E * E2_first;
    for (uint32_t j = E2_first; j < nseg; j++) {
      or_out(op, bitplane_rvv(f2 + i, j), e2 >> 5, e2 & 31);
      e2 += E2;
    }
    op++;
  }
}
#endif

/* ---- driver ---------------------------------------------------------------- */
#define OUTW 65536 /* uint32 words of output space */
static uint8_t f[16384], f2[16384];
static uint32_t os[OUTW], ov[OUTW], oq[OUTW];

int main(int argc, char **argv)
{
  int fails = 0;
  if (argc > 1 && pin_to_cpu(atoi(argv[1])) == 0)
    printf("pinned to CPU %d\n", atoi(argv[1]));
#if defined(__riscv) && defined(__riscv_vector)
  printf("RVV: VLEN = %zu bits\n", (size_t)__riscv_vlenb() * 8);
#else
  printf("built WITHOUT RVV\n");
#endif

  /* a few configs: (E, E2, E2_first, nseg, Eoffset) */
  struct {
    uint32_t E, E2, first, nseg, off;
  } cfg[] = {
      {8192, 8192, 8, 8, 0},
      {8192, 8192, 3, 8, 5},
      {4096, 8192, 3, 6, 17},
      {8192, 4096, 2, 5, 100},
      {2048, 2048, 1, 1, 0},
  };
  for (unsigned c = 0; c < sizeof(cfg) / sizeof(cfg[0]); c++) {
    uint32_t E = cfg[c].E, E2 = cfg[c].E2, first = cfg[c].first, nseg = cfg[c].nseg, off = cfg[c].off;
    for (uint32_t k = 0; k < E2; k++) {
      f[k] = rnd8();
      f2[k] = rnd8();
    }
    memset(os, 0, sizeof(os));
    memset(ov, 0, sizeof(ov));
    memset(oq, 0, sizeof(oq));
    wto_scalar(f, E, f2, E2, first, nseg, os, off);
    wto_simde(f, E, f2, E2, first, nseg, ov, off);
    int bad = memcmp(os, ov, sizeof(os)) != 0;
#if defined(__riscv) && defined(__riscv_vector)
    wto_rvv(f, E, f2, E2, first, nseg, oq, off);
    bad |= memcmp(os, oq, sizeof(os)) != 0;
#endif
    printf("  cfg%u (E=%u E2=%u first=%u nseg=%u off=%u): %s\n", c, E, E2, first, nseg, off, bad ? "FAIL" : "byte-exact OK");
    fails += bad;
  }

  /* benchmark: E=E2=8192, nseg=8 */
  const uint32_t E = 8192, nseg = 8;
  for (uint32_t k = 0; k < E; k++) {
    f[k] = rnd8();
    f2[k] = rnd8();
  }
  const int iters = 4000;
  double t0 = now_ns();
  for (int it = 0; it < iters; it++) {
    memset(ov, 0, sizeof(ov));
    wto_simde(f, E, f2, E, nseg, nseg, ov, 0);
  }
  double tsi = now_ns() - t0;
  printf("\nsimde(OAI) : %.2f us/call\n", tsi / iters / 1000.0);
#if defined(__riscv) && defined(__riscv_vector)
  t0 = now_ns();
  for (int it = 0; it < iters; it++) {
    memset(oq, 0, sizeof(oq));
    wto_rvv(f, E, f2, E, nseg, nseg, oq, 0);
  }
  double tv = now_ns() - t0;
  printf("rvv        : %.2f us/call  (%.1fx vs simde/OAI)\n", tv / iters / 1000.0, tsi / tv);
#endif

  printf("\nRESULT: %s\n", fails ? "FAIL" : "PASS");
  return fails ? 1 : 0;
}
