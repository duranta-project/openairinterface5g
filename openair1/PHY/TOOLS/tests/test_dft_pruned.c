/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* Pruned DFT/IDFT test: the used bins of dft_pruned() must be bit-identical to dft() (natural layout)
 * and to dft() + FFT shift (packed layout); idft_pruned() must be bit-identical to idft() of the input
 * with zero guard bins (packed layout: the guard bins of the input are zero, as in the NR transmit
 * buffers; natural layout: they hold garbage and must not be used). Writes must stay inside the N-sample
 * output buffer. Inputs are also passed at an odd sample offset (not vector aligned),
 * as the OFDM demodulators do.
 *   dft_pruned_test        correctness over sizes x bands (exit status 0 = pass)
 *   dft_pruned_test -t     timing: full vs pruned, per size and occupancy (pin it with taskset) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "openair1/PHY/TOOLS/tools_defs.h"
#include "common/utils/utils.h"

/* sizes with and without band kernels; 12, 300 and 1200 are not multiples of 8 (fallback path) */
static const int sizes[] = {12, 128, 256, 300, 384, 512, 768, 1024, 1200, 1536, 2048, 3072, 4096, 6144};
static const int prbs[] = {273, 270, 217, 160, 133, 106, 79, 52, 51, 38, 24, 11, 6, 1};

static uint64_t rng_state = 0x9E3779B97F4A7C15ull;
static uint64_t rng(void)
{
  uint64_t z = (rng_state += 0x9E3779B97F4A7C15ull);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

static void fill(c16_t *x, int n, int amp)
{
  for (int i = 0; i < n; i++) {
    const uint64_t v = rng();
    x[i].r = (int16_t)((int)((v & 0xffffffffu) % (uint32_t)(2 * amp + 1)) - amp);
    x[i].i = (int16_t)((int)((v >> 32) % (uint32_t)(2 * amp + 1)) - amp);
  }
}

#define GUARD 64 /* canary samples around the output buffer */
static const c16_t canary = {.r = 0x5a5a, .i = -0x2525};

/* One band: compare pruned against full for both layouts and both directions. Returns the number of errors. */
static int check_band(int N, int first, int num, int amp)
{
  const int pad = N + 2 * GUARD;
  c16_t *in = malloc16(sizeof(c16_t) * N);
  c16_t *ref = malloc16(sizeof(c16_t) * N);
  c16_t *buf = malloc16(sizeof(c16_t) * pad);
  c16_t *out = buf + GUARD; /* 32-byte aligned: GUARD * 4 bytes */
  int err = 0;

  /* DFT */
  fill(in, N, amp);
  dft(get_dft(N), (int16_t *)in, (int16_t *)ref, 1);
  for (int layout = 0; layout < 2; layout++) {
    for (int i = 0; i < pad; i++)
      buf[i] = canary;
    dft_pruned(get_dft(N), (int16_t *)in, (int16_t *)out, first, num, layout);
    /* contract: the band. Current implementation: all N bins (PACKED = rotation of the dft() output,
     * which the NR callers rely on to keep their guard-bin reads unchanged). */
    for (int j = 0; j < N; j++) {
      const int k = (first + j) % N;
      const c16_t v = out[layout == DFT_PRUNE_PACKED ? j : k];
      if (v.r != ref[k].r || v.i != ref[k].i) {
        if (err < 5)
          printf("  dft N %d first %d num %d layout %d: bin %d got (%d,%d) want (%d,%d)\n",
                 N,
                 first,
                 num,
                 layout,
                 k,
                 v.r,
                 v.i,
                 ref[k].r,
                 ref[k].i);
        err++;
      }
    }
    /* nothing written outside the N-sample output buffer */
    const int lim = N;
    for (int i = 0; i < pad; i++) {
      const int idx = i - GUARD;
      if ((idx < 0 || idx >= lim) && (buf[i].r != canary.r || buf[i].i != canary.i)) {
        if (err < 5)
          printf("  dft N %d first %d num %d layout %d: write outside at index %d\n", N, first, num, layout, idx);
        err++;
        break;
      }
    }
  }
  /* packed layout from an input that is not vector aligned */
  c16_t *in_buf = malloc16(sizeof(c16_t) * (N + 1));
  c16_t *in_u = in_buf + 1;
  memcpy(in_u, in, sizeof(c16_t) * N);
  dft_pruned(get_dft(N), (int16_t *)in_u, (int16_t *)out, first, num, DFT_PRUNE_PACKED);
  for (int j = 0; j < N; j++) {
    const int k = (first + j) % N;
    if (out[j].r != ref[k].r || out[j].i != ref[k].i) {
      if (err < 5)
        printf("  dft unaligned input N %d first %d num %d: bin %d differs\n", N, first, num, k);
      err++;
      break;
    }
  }

  /* in-place natural layout */
  memcpy(out, in, sizeof(c16_t) * N);
  dft_pruned(get_dft(N), (int16_t *)out, (int16_t *)out, first, num, DFT_PRUNE_NATURAL);
  for (int j = 0; j < num; j++) {
    const int k = (first + j) % N;
    if (out[k].r != ref[k].r || out[k].i != ref[k].i) {
      if (err < 5)
        printf("  dft in-place N %d first %d num %d: bin %d differs\n", N, first, num, k);
      err++;
    }
  }

  /* IDFT: band random, guard zero (packed input and reference); garbage in the guard of the natural input */
  c16_t *nat = malloc16(sizeof(c16_t) * N);
  c16_t *packed = malloc16(sizeof(c16_t) * N);
  memset(nat, 0, sizeof(c16_t) * N);
  memset(packed, 0, sizeof(c16_t) * N);
  fill(packed, num, amp);
  for (int j = 0; j < num; j++)
    nat[(first + j) % N] = packed[j];
  idft(get_idft(N), (int16_t *)nat, (int16_t *)ref, 1);
  for (int layout = 0; layout < 2; layout++) {
    c16_t *src = packed;
    if (layout == DFT_PRUNE_NATURAL) {
      fill(in, N, 30000);
      for (int j = 0; j < num; j++)
        in[(first + j) % N] = packed[j];
      src = in;
    }
    for (int i = 0; i < pad; i++)
      buf[i] = canary;
    /* packed input not vector aligned */
    if (layout == DFT_PRUNE_PACKED) {
      memcpy(in_u, packed, sizeof(c16_t) * N);
      src = in_u;
    }
    idft_pruned(get_idft(N), (int16_t *)src, (int16_t *)out, first, num, layout);
    for (int i = 0; i < N; i++) {
      if (out[i].r != ref[i].r || out[i].i != ref[i].i) {
        if (err < 5)
          printf("  idft N %d first %d num %d layout %d: sample %d got (%d,%d) want (%d,%d)\n",
                 N,
                 first,
                 num,
                 layout,
                 i,
                 out[i].r,
                 out[i].i,
                 ref[i].r,
                 ref[i].i);
        err++;
      }
    }
    for (int i = 0; i < GUARD; i++)
      if (buf[i].r != canary.r || buf[N + GUARD + i].r != canary.r) {
        printf("  idft N %d: write outside the output buffer\n", N);
        err++;
        break;
      }
  }
  free(nat);
  free(packed);
  free(in_buf);
  free(in);
  free(ref);
  free(buf);
  return err;
}

static int run_checks(void)
{
  int errors = 0, cases = 0;
  for (int s = 0; s < (int)(sizeof(sizes) / sizeof(sizes[0])); s++) {
    const int N = sizes[s];
    int serr = 0, scases = 0;
    /* NR bands: nb_sc = 12 * PRB centred on DC (first = N - nb_sc / 2), the full band, and narrow bands */
    for (int p = 0; p < (int)(sizeof(prbs) / sizeof(prbs[0])); p++) {
      const int num = 12 * prbs[p];
      if (num > N)
        continue;
      for (int amp = 1024; amp <= 8192; amp *= 8) {
        serr += check_band(N, N - num / 2, num, amp);
        scases++;
      }
    }
    serr += check_band(N, 0, N, 4096);
    scases++;
    /* arbitrary bands: any first, any length, wrapping or not */
    for (int t = 0; t < 40; t++) {
      const int num = 1 + (int)(rng() % (uint64_t)N);
      const int first = (int)(rng() % (uint64_t)N);
      serr += check_band(N, first, num, 4096);
      scases++;
    }
    printf("N %5d: %3d bands, %s\n", N, scases, serr ? "FAIL" : "bit-identical");
    errors += serr;
    cases += scases;
  }
  printf("%d bands, %d errors\n", cases, errors);
  return errors != 0;
}

static uint64_t now_ns(void)
{
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC_RAW, &t);
  return t.tv_sec * 1000000000ull + t.tv_nsec;
}

/* What the NR receive path does without pruning: full DFT + fftshift_inplace of the band. */
static void full_dft_fftshift(int N, c16_t *in, c16_t *out, int nbins)
{
  dft(get_dft(N), (int16_t *)in, (int16_t *)out, 1);
  const int half = nbins / 2;
  c16_t tmp[half];
  memcpy(tmp, out + N - half, half * sizeof(c16_t));
  memmove(out + half, out, (N - half) * sizeof(c16_t));
  memcpy(out, tmp, half * sizeof(c16_t));
}

/* The transmit side without pruning: the band moved to natural FFT order in a zero-guard buffer + full IDFT. */
static void ifftshift_full_idft(int N, c16_t *in, c16_t *nat, c16_t *out, int nbins)
{
  const int half = nbins / 2;
  memcpy(nat + N - half, in, half * sizeof(c16_t));
  memcpy(nat, in + half, half * sizeof(c16_t));
  idft(get_idft(N), (int16_t *)nat, (int16_t *)out, 1);
}

static void run_timing(int rounds)
{
  printf("%-5s %-4s %6s %5s | %8s %8s %6s %8s %6s | %8s %6s | %8s %8s %6s %8s %6s | %8s %6s\n",
         "N",
         "PRB",
         "nbins",
         "occ",
         "dft",
         "pr_nat",
         "ratio",
         "pr_pack",
         "ratio",
         "dft+shft",
         "pack/",
         "idft",
         "ipr_nat",
         "ratio",
         "ipr_pack",
         "ratio",
         "shft+idft",
         "pack/");
  const int timed_sizes[] = {512, 768, 1024, 1536, 2048, 3072, 4096};
  const int timed_prbs[][4] = {{24, 11, 0, 0},
                               {51, 24, 0, 0},
                               {79, 51, 24, 0},
                               {106, 79, 51, 24},
                               {160, 133, 106, 24},
                               {217, 162, 106, 24},
                               {273, 217, 106, 24}};
  enum { IT = 2000 };
  c16_t *in = malloc16(sizeof(c16_t) * 4096 + 64);
  c16_t *out = malloc16(sizeof(c16_t) * 4096 + 64);
  c16_t *nat = malloc16(sizeof(c16_t) * 4096 + 64);
  memset(nat, 0, sizeof(c16_t) * 4096);
  for (int s = 0; s < 7; s++) {
    const int N = timed_sizes[s];
    for (int p = 0; p < 4 && timed_prbs[s][p]; p++) {
      const int nbins = 12 * timed_prbs[s][p];
      const int first = N - nbins / 2;
      fill(in, N, 2000);
      double best[8];
      for (int c = 0; c < 8; c++)
        best[c] = 1e30;
      for (int r = 0; r < rounds; r++) {
        for (int c = 0; c < 8; c++) {
          const uint64_t t0 = now_ns();
          for (int i = 0; i < IT; i++) {
            switch (c) {
              case 0:
                dft(get_dft(N), (int16_t *)in, (int16_t *)out, 1);
                break;
              case 1:
                dft_pruned(get_dft(N), (int16_t *)in, (int16_t *)out, first, nbins, DFT_PRUNE_NATURAL);
                break;
              case 2:
                dft_pruned(get_dft(N), (int16_t *)in, (int16_t *)out, first, nbins, DFT_PRUNE_PACKED);
                break;
              case 3:
                full_dft_fftshift(N, in, out, nbins);
                break;
              case 4:
                idft(get_idft(N), (int16_t *)in, (int16_t *)out, 1);
                break;
              case 5:
                idft_pruned(get_idft(N), (int16_t *)in, (int16_t *)out, first, nbins, DFT_PRUNE_NATURAL);
                break;
              case 6:
                idft_pruned(get_idft(N), (int16_t *)in, (int16_t *)out, first, nbins, DFT_PRUNE_PACKED);
                break;
              case 7:
                ifftshift_full_idft(N, in, nat, out, nbins);
                break;
            }
          }
          const double ns = (double)(now_ns() - t0) / IT;
          if (ns < best[c])
            best[c] = ns;
        }
      }
      printf("%-5d %-4d %6d %4.0f%% | %8.1f %8.1f %6.3f %8.1f %6.3f | %8.1f %6.3f | %8.1f %8.1f %6.3f %8.1f %6.3f | %8.1f %6.3f\n",
             N,
             nbins / 12,
             nbins,
             100.0 * nbins / N,
             best[0],
             best[1],
             best[1] / best[0],
             best[2],
             best[2] / best[0],
             best[3],
             best[2] / best[3],
             best[4],
             best[5],
             best[5] / best[4],
             best[6],
             best[6] / best[4],
             best[7],
             best[6] / best[7]);
    }
  }
}

int main(int argc, char **argv)
{
  load_dftslib();
  if (argc > 1 && !strcmp(argv[1], "-t")) {
    run_timing(argc > 2 ? atoi(argv[2]) : 7);
    return 0;
  }
  return run_checks();
}
