/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * RVV port harness -- kernel #12: transpose16_ooff_simd256, the inter-stage
 * data reorder used by idft256/1024/4096 between the recursive sub-DFTs and the
 * radix-4 butterfly loop.
 *
 * Viewing each complex as one 32-bit element, the AVX2 kernel (permutevar8x32 +
 * unpacklo/hi_epi64 + insertf128 over 4 input vectors = 32 complex) computes a
 * plain 8x4 -> 4x8 transpose:
 *
 *   out_v[i] = in[4*i + v]      (v = 0..3 output vectors, i = 0..7 lanes)
 *
 * i.e. out0 = in[0,4,8,...,28], out1 = in[1,5,...,29], etc. The 4 outputs are
 * written at stride `off` (in 256-bit-block units) into a larger array.
 *
 * In RVV that permutation IS a 4-field segment load: vlseg4e32 over the 32
 * complex yields field v = out_v directly -- no shuffle/permute, and it uses
 * the fast segment path (strided loads are catastrophically slow on this uarch).
 * The AVX2 "2 DFTs packed per 256-bit register" lane structure is irrelevant
 * here because a segment load expresses the deinterleave length-agnostically.
 *
 * Compared BYTE-FOR-BYTE: scalar / simde / rvv, for off=1 (contiguous) and a
 * realistic off. It is a pure permutation, so byte-exact for any input.
 */

#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <unistd.h>

#include <simde/x86/avx2.h>
#if defined(__riscv) && defined(__riscv_vector)
#include <riscv_vector.h>
#endif

static int use_ai_core(void)
{
  FILE *fp = fopen("/proc/set_ai_thread", "w");
  if (!fp) return -1;
  int rc = fprintf(fp, "%ld\n", (long)getpid());
  return (rc < 0 || fclose(fp) != 0) ? -1 : 0;
}
static int pin_to_cpu(int cpu)
{
  cpu_set_t set; CPU_ZERO(&set); CPU_SET(cpu, &set);
  return sched_setaffinity(0, sizeof(set), &set);
}

/* ---- scalar ground truth: out_v[i] = in[4*i + v] -------------------------- */
static void transpose_scalar(const uint32_t *x, uint32_t *y, int off)
{
  for (int v = 0; v < 4; v++)
    for (int i = 0; i < 8; i++)
      y[v * off * 8 + i] = x[4 * i + v];
}

/* ---- SIMDe baseline: the exact sequence from oai_dfts.c ------------------- */
static void transpose_simde(const simde__m256i *x, simde__m256i *y, int off)
{
  const simde__m256i perm_mask = simde_mm256_set_epi32(7, 3, 5, 1, 6, 2, 4, 0);
  simde__m256i ytmp0 = simde_mm256_permutevar8x32_epi32(x[0], perm_mask);
  simde__m256i ytmp1 = simde_mm256_permutevar8x32_epi32(x[1], perm_mask);
  simde__m256i ytmp2 = simde_mm256_permutevar8x32_epi32(x[2], perm_mask);
  simde__m256i ytmp3 = simde_mm256_permutevar8x32_epi32(x[3], perm_mask);
  simde__m256i ytmp4 = simde_mm256_unpacklo_epi64(ytmp0, ytmp1);
  simde__m256i ytmp5 = simde_mm256_unpackhi_epi64(ytmp0, ytmp1);
  simde__m256i ytmp6 = simde_mm256_unpacklo_epi64(ytmp2, ytmp3);
  simde__m256i ytmp7 = simde_mm256_unpackhi_epi64(ytmp2, ytmp3);
  simde__m256i *y2 = y;
  *y2 = simde_mm256_insertf128_si256(ytmp4, simde_mm256_extracti128_si256(ytmp6, 0), 1); y2 += off;
  *y2 = simde_mm256_insertf128_si256(ytmp6, simde_mm256_extracti128_si256(ytmp4, 1), 0); y2 += off;
  *y2 = simde_mm256_insertf128_si256(ytmp5, simde_mm256_extracti128_si256(ytmp7, 0), 1); y2 += off;
  *y2 = simde_mm256_insertf128_si256(ytmp7, simde_mm256_extracti128_si256(ytmp5, 1), 0);
}

/* ---- RVV port: the transpose is a 4-field segment load -------------------- */
#if defined(__riscv) && defined(__riscv_vector)
static void transpose_rvv(const uint32_t *x, uint32_t *y, int off)
{
  size_t vl = __riscv_vsetvl_e32m1(8);                    /* 8 complex per output vector */
  vuint32m1x4_t s = __riscv_vlseg4e32_v_u32m1x4(x, vl);   /* field v = in[4*i+v] = out_v */
  __riscv_vse32_v_u32m1(y + 0 * off * 8, __riscv_vget_v_u32m1x4_u32m1(s, 0), vl);
  __riscv_vse32_v_u32m1(y + 1 * off * 8, __riscv_vget_v_u32m1x4_u32m1(s, 1), vl);
  __riscv_vse32_v_u32m1(y + 2 * off * 8, __riscv_vget_v_u32m1x4_u32m1(s, 2), vl);
  __riscv_vse32_v_u32m1(y + 3 * off * 8, __riscv_vget_v_u32m1x4_u32m1(s, 3), vl);
}
#endif

static uint32_t rng = 0x9e3779b9u;
static uint32_t rnd32(void) { rng = rng * 1103515245u + 12345u; return rng; }

int main(int argc, char **argv)
{
  int cpu = (argc > 1) ? atoi(argv[1]) : -1;
  if (cpu >= 0) { if (cpu > 7) use_ai_core(); if (pin_to_cpu(cpu) != 0) fprintf(stderr, "warn: no pin %d\n", cpu); }

  const int offs[] = {1, 128};
  enum { MAXOFF = 128, YSZ = 4 * MAXOFF * 8 }; /* outputs span v*off*8 u32, v=0..3 */
  long mism_sm = 0, mism_rv = 0;
  enum { N = 200000 };
  for (int t = 0; t < N; t++) {
    for (unsigned oi = 0; oi < sizeof(offs) / sizeof(offs[0]); oi++) {
      int off = offs[oi];
      uint32_t x[32];
      uint32_t ys[YSZ], ym[YSZ];
      for (int i = 0; i < 32; i++) x[i] = rnd32();
      transpose_scalar(x, ys, off);
      transpose_simde((const simde__m256i *)x, (simde__m256i *)ym, off);
      for (int v = 0; v < 4; v++)
        if (memcmp(ys + v * off * 8, ym + v * off * 8, 32)) mism_sm++;
#if defined(__riscv) && defined(__riscv_vector)
      uint32_t yr[YSZ];
      transpose_rvv(x, yr, off);
      for (int v = 0; v < 4; v++)
        if (memcmp(ys + v * off * 8, yr + v * off * 8, 32)) mism_rv++;
#endif
    }
  }
  printf("correctness (%d runs x {off=1,128}):\n", N);
  printf("  scalar vs simde : %s (%ld mismatches)\n", mism_sm ? "FAIL" : "OK", mism_sm);
#if defined(__riscv) && defined(__riscv_vector)
  printf("  scalar vs rvv   : %s (%ld mismatches)\n", mism_rv ? "FAIL" : "OK", mism_rv);
#else
  printf("  rvv             : (not built -- no __riscv_vector)\n");
#endif
  return (mism_sm || mism_rv) ? 1 : 0;
}
