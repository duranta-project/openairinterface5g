/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* RISC-V DFT/IDFT backend, the sibling of oai_dfts.c (x86) and oai_dfts_neon.c (ARM), with the same contract:
 * every size of FOREACH_DFTSZ / FOREACH_IDFTSZ takes N contiguous c16_t and returns the orthonormal (1/sqrt(N))
 * transform; scale_flag is ignored.
 *
 * Mixed-radix decimation in time, as the x86 backend: radix-5, then radix-3, then radix-4/2 stages, each an
 * r-point DFT matrix pre-scaled by 1/sqrt(r) over the M = N/r sub-transforms (unit-stride over k, so it vectorizes
 * with segment loads), down to N = 1 or to a size the RVV Bailey/Stockham kernels (stk_*) take whole. The combine
 * has a scalar and an RVV version with the same integer arithmetic (Q15 twiddles, RNU rounding, saturating int32
 * accumulation), so the RVV build is bit-exact with the scalar one; OAI_DFTS_GENERIC_HOST builds the scalar
 * path on any host for testing against test_dft. */
#if defined(__riscv) || defined(OAI_DFTS_GENERIC_HOST)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <pthread.h>
#include "assertions.h"
#include "tools_defs.h"
#include "LOG/log.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef __riscv_vector
#include <riscv_vector.h>


/* ===================================================================
 * VLA iterative radix-4 (DIF Stockham autosort) IDFT. One kernel for any
 * VLEN: it vectorizes across the N/4 independent butterflies of each stage
 * (vl = VLMAX, never partial), so there is no fixed-width leaf and no
 * partial-vl store. Validated byte-exact (rvv vs scalar) on VLEN=256 and 1024
 * for N=64/256/1024/4096 in rvv_harness/dft_fft_test.c. Replaces the recursive
 * idft chain for the pure-power-of-4 sizes.
 *
 * Twiddles are conjugated (inverse) and >>1 per stage gives 1/sqrt(N) (scale
 * flag; every OAI idft caller passes scale=1). Reads gather the k*L+j input
 * pattern; writes are unit-stride (Stockham output is contiguous).
 * =================================================================== */
static inline int16_t stk_sat16(int32_t v) { return v > 32767 ? 32767 : (v < -32768 ? -32768 : (int16_t)v); }

#define STK_MAXN 4096
#define STK_MAXST 3  /* log4(64) sub-FFT stages */
#define STK_MAXBF 16 /* 64/4 butterflies per sub-FFT stage */
/* 4-step (Bailey) plan: N = N1*N2 (pow-4 factors, so sub-FFTs are pure radix-4).
 * subtw[dir][which][stage][bf] = the 3 radix-4 twiddles (interleaved re,im) of
 * the size-(N1|N2) sub-FFT; btw[dir] = the [N] inter-pass (step-B) twiddles.
 * dir 0 = inverse (sign +1), dir 1 = forward (sign -1); the sign is folded into
 * every twiddle and into the radix-4 +/-j flip. Read-only after stk_init. */
typedef struct {
  int N, N1, N2, np[2]; /* np[which] = log4(N1|N2); which 0 = pass A (size N1), 1 = pass D (size N2) */
  int16_t subtw[2][2][STK_MAXST][STK_MAXBF][6];
  int16_t *btw[2];
} stk_plan_t;

static const struct { int N, N1, N2; } stk_cfg[] = {{64, 4, 16}, {256, 16, 16}, {1024, 16, 64}, {4096, 64, 64}};
enum { STK_NPLAN = sizeof(stk_cfg) / sizeof(stk_cfg[0]) };
static stk_plan_t stk_plans[STK_NPLAN];

static void stk_build_sub(stk_plan_t *pl, int which, int M)
{
  int np = 0;
  for (int L = M; L >= 4; L /= 4) {
    int Lq = L / 4, st = np++;
    for (int bf = 0; bf < M / 4; bf++) {
      int j = bf % Lq;
      for (int m = 1; m <= 3; m++)
        for (int dir = 0; dir < 2; dir++) {
          double ang = (double)(dir ? -1 : 1) * 2.0 * M_PI * (double)(m * j) / (double)L;
          pl->subtw[dir][which][st][bf][2 * (m - 1)] = stk_sat16((int32_t)lround(cos(ang) * 32767.0));
          pl->subtw[dir][which][st][bf][2 * (m - 1) + 1] = stk_sat16((int32_t)lround(sin(ang) * 32767.0));
        }
    }
  }
  pl->np[which] = np;
}
static void stk_build(stk_plan_t *pl, int N, int N1, int N2)
{
  pl->N = N;
  pl->N1 = N1;
  pl->N2 = N2;
  stk_build_sub(pl, 0, N1);
  stk_build_sub(pl, 1, N2);
  for (int dir = 0; dir < 2; dir++) {
    pl->btw[dir] = (int16_t *)malloc(sizeof(int16_t) * 2 * N);
    for (int k1 = 0; k1 < N1; k1++)
      for (int n2 = 0; n2 < N2; n2++) {
        double ang = (double)(dir ? -1 : 1) * 2.0 * M_PI * (double)k1 * (double)n2 / (double)N;
        pl->btw[dir][2 * (k1 * N2 + n2)] = stk_sat16((int32_t)lround(cos(ang) * 32767.0));
        pl->btw[dir][2 * (k1 * N2 + n2) + 1] = stk_sat16((int32_t)lround(sin(ang) * 32767.0));
      }
  }
}
/* radix-2 sizes N = 2*M (M a pow-4 size handled by the 4-step). One radix-2 DIF
 * stage wraps stk_run on M=N/2. r2tw[dir][scaled] = W_N^{s*n} over [M], pre-scaled
 * by 1/sqrt2 in the scaled variant (so the b-branch cmul yields the 1/sqrt2 that,
 * with the inner 4-step's 1/sqrt(M), gives 1/sqrt(N)). dir 0 inverse, 1 forward. */
#define STK_MAXM 4096       /* largest inner 4-step size (N=8192 -> M=4096) */
#define STK_SQRT2INV 23170  /* round(32768/sqrt2) = ONE_OVER_SQRT2_Q15 */
static const struct { int N; } stk_r2cfg[] = {{128}, {512}, {2048}, {8192}};
enum { STK_NR2 = sizeof(stk_r2cfg) / sizeof(stk_r2cfg[0]) };
typedef struct { int N, M; int16_t *r2tw[2][2]; } stk_r2plan_t;
static stk_r2plan_t stk_r2plans[STK_NR2];

static void stk_build_r2(stk_r2plan_t *pl, int N)
{
  int M = N / 2;
  pl->N = N;
  pl->M = M;
  for (int dir = 0; dir < 2; dir++)
    for (int sc = 0; sc < 2; sc++) {
      pl->r2tw[dir][sc] = (int16_t *)malloc(sizeof(int16_t) * 2 * M);
      double mag = sc ? (double)STK_SQRT2INV : 32767.0;
      for (int n = 0; n < M; n++) {
        double ang = (double)(dir ? -1 : 1) * 2.0 * M_PI * (double)n / (double)N;
        pl->r2tw[dir][sc][2 * n] = stk_sat16((int32_t)lround(cos(ang) * mag));
        pl->r2tw[dir][sc][2 * n + 1] = stk_sat16((int32_t)lround(sin(ang) * mag));
      }
    }
}
/* radix-3 sizes N = 3*M (M = N/3; M pow-4 -> inner stk_run, M = 2*pow-4 -> inner
 * stk_run2). r3tw{1,2}[dir][scaled] = W_N^{s*n}, W_N^{2s*n} over [M], pre-scaled
 * by 1/sqrt3 in the scaled variant. 1/sqrt3 * 1/sqrt(M) = 1/sqrt(N). */
#define STK_SQRT3INV 18919  /* ONE_OVER_SQRT3_Q15 = round(32768/sqrt3) */
#define STK_SQRT3_2  28378  /* round(sqrt(3)/2 * 32768) */
static const struct { int N, Mrad2; } stk_r3cfg[] = {
    {192, 0}, {768, 0}, {3072, 0}, {12288, 0}, {384, 1}, {1536, 1}, {6144, 1}};
enum { STK_NR3 = sizeof(stk_r3cfg) / sizeof(stk_r3cfg[0]) };
typedef struct { int N, M, Mrad2; int16_t *r3tw1[2][2], *r3tw2[2][2]; } stk_r3plan_t;
static stk_r3plan_t stk_r3plans[STK_NR3];

static void stk_build_r3(stk_r3plan_t *pl, int N, int Mrad2)
{
  int M = N / 3;
  pl->N = N;
  pl->M = M;
  pl->Mrad2 = Mrad2;
  for (int dir = 0; dir < 2; dir++)
    for (int sc = 0; sc < 2; sc++) {
      pl->r3tw1[dir][sc] = (int16_t *)malloc(sizeof(int16_t) * 2 * M);
      pl->r3tw2[dir][sc] = (int16_t *)malloc(sizeof(int16_t) * 2 * M);
      double mag = sc ? (double)STK_SQRT3INV : 32767.0, sgn = dir ? -1.0 : 1.0;
      for (int n = 0; n < M; n++) {
        double a1 = sgn * 2.0 * M_PI * (double)n / (double)N;
        double a2 = sgn * 2.0 * M_PI * (double)(2 * n) / (double)N;
        pl->r3tw1[dir][sc][2 * n] = stk_sat16((int32_t)lround(cos(a1) * mag));
        pl->r3tw1[dir][sc][2 * n + 1] = stk_sat16((int32_t)lround(sin(a1) * mag));
        pl->r3tw2[dir][sc][2 * n] = stk_sat16((int32_t)lround(cos(a2) * mag));
        pl->r3tw2[dir][sc][2 * n + 1] = stk_sat16((int32_t)lround(sin(a2) * mag));
      }
    }
}
__attribute__((constructor)) static void stk_init(void)
{
  for (int i = 0; i < STK_NPLAN; i++)
    stk_build(&stk_plans[i], stk_cfg[i].N, stk_cfg[i].N1, stk_cfg[i].N2);
  for (int i = 0; i < STK_NR2; i++)
    stk_build_r2(&stk_r2plans[i], stk_r2cfg[i].N);
  for (int i = 0; i < STK_NR3; i++)
    stk_build_r3(&stk_r3plans[i], stk_r3cfg[i].N, stk_r3cfg[i].Mrad2);
}
static const stk_plan_t *stk_get(int N)
{
  for (int i = 0; i < STK_NPLAN; i++)
    if (stk_plans[i].N == N)
      return &stk_plans[i];
  return 0;
}

/* Batched radix-4 DIF Stockham along the M rows of an [M][B] complex array whose
 * B columns are contiguous (the vectorized batch: unit-stride vlseg2, no gather).
 * s selects the +/-j flip (+1 inverse, -1 forward); sh = per-stage >>1 (1/sqrt).
 * Ping-pongs a<->b across the log4(M) stages; returns the buffer with the result. */
static int16_t *stk_subfft(const stk_plan_t *pl, int dir, int which, int s, int sh,
                           int16_t *a, int16_t *b, int M, int B)
{
  int np = pl->np[which], M4 = M / 4;
  for (int p = 0, L = M; p < np; p++, L /= 4) {
    int Lq = L / 4;
    for (int bf = 0; bf < M4; bf++) {
      int k = bf / Lq, j = bf % Lq, r0 = k * L + j, r1 = r0 + Lq, r2 = r0 + 2 * Lq, r3 = r0 + 3 * Lq;
      const int16_t *w = pl->subtw[dir][which][p][bf];
      const int16_t *A0 = a + 2 * (size_t)r0 * B, *A1 = a + 2 * (size_t)r1 * B;
      const int16_t *A2 = a + 2 * (size_t)r2 * B, *A3 = a + 2 * (size_t)r3 * B;
      int16_t *B0 = b + 2 * (size_t)bf * B, *B1 = b + 2 * (size_t)(bf + M4) * B;
      int16_t *B2 = b + 2 * (size_t)(bf + 2 * M4) * B, *B3 = b + 2 * (size_t)(bf + 3 * M4) * B;
      for (int c = 0; c < B;) {
        size_t vl = __riscv_vsetvl_e16mf2(B - c);
        int o = 2 * c;
        vint16mf2x2_t X0 = __riscv_vlseg2e16_v_i16mf2x2(A0 + o, vl), X1 = __riscv_vlseg2e16_v_i16mf2x2(A1 + o, vl);
        vint16mf2x2_t X2 = __riscv_vlseg2e16_v_i16mf2x2(A2 + o, vl), X3 = __riscv_vlseg2e16_v_i16mf2x2(A3 + o, vl);
        vint16mf2_t x0r = __riscv_vget_v_i16mf2x2_i16mf2(X0, 0), x0i = __riscv_vget_v_i16mf2x2_i16mf2(X0, 1);
        vint16mf2_t x1r = __riscv_vget_v_i16mf2x2_i16mf2(X1, 0), x1i = __riscv_vget_v_i16mf2x2_i16mf2(X1, 1);
        vint16mf2_t x2r = __riscv_vget_v_i16mf2x2_i16mf2(X2, 0), x2i = __riscv_vget_v_i16mf2x2_i16mf2(X2, 1);
        vint16mf2_t x3r = __riscv_vget_v_i16mf2x2_i16mf2(X3, 0), x3i = __riscv_vget_v_i16mf2x2_i16mf2(X3, 1);
        vint16mf2_t d0r = __riscv_vsadd_vv_i16mf2(__riscv_vsadd_vv_i16mf2(x0r, x1r, vl), __riscv_vsadd_vv_i16mf2(x2r, x3r, vl), vl);
        vint16mf2_t d0i = __riscv_vsadd_vv_i16mf2(__riscv_vsadd_vv_i16mf2(x0i, x1i, vl), __riscv_vsadd_vv_i16mf2(x2i, x3i, vl), vl);
        vint16mf2_t d2r = __riscv_vssub_vv_i16mf2(__riscv_vsadd_vv_i16mf2(x0r, x2r, vl), __riscv_vsadd_vv_i16mf2(x1r, x3r, vl), vl);
        vint16mf2_t d2i = __riscv_vssub_vv_i16mf2(__riscv_vsadd_vv_i16mf2(x0i, x2i, vl), __riscv_vsadd_vv_i16mf2(x1i, x3i, vl), vl);
        vint16mf2_t j1r, j1i, j3r, j3i;
        if (s > 0) {
          j1r = __riscv_vneg_v_i16mf2(x1i, vl); j1i = x1r;
          j3r = __riscv_vneg_v_i16mf2(x3i, vl); j3i = x3r;
        } else {
          j1r = x1i; j1i = __riscv_vneg_v_i16mf2(x1r, vl);
          j3r = x3i; j3i = __riscv_vneg_v_i16mf2(x3r, vl);
        }
        vint16mf2_t er = __riscv_vssub_vv_i16mf2(x0r, x2r, vl), ei = __riscv_vssub_vv_i16mf2(x0i, x2i, vl);
        vint16mf2_t fr = __riscv_vssub_vv_i16mf2(j1r, j3r, vl), fi = __riscv_vssub_vv_i16mf2(j1i, j3i, vl);
        vint16mf2_t d1r = __riscv_vsadd_vv_i16mf2(er, fr, vl), d1i = __riscv_vsadd_vv_i16mf2(ei, fi, vl);
        vint16mf2_t d3r = __riscv_vssub_vv_i16mf2(er, fr, vl), d3i = __riscv_vssub_vv_i16mf2(ei, fi, vl);
        __riscv_vsseg2e16_v_i16mf2x2(B0 + o, __riscv_vcreate_v_i16mf2x2(
            __riscv_vssra_vx_i16mf2(d0r, sh, __RISCV_VXRM_RNU, vl), __riscv_vssra_vx_i16mf2(d0i, sh, __RISCV_VXRM_RNU, vl)), vl);
#define STK_TW(DR, DI, WI, DST)                                                                                   \
  do {                                                                                                            \
    int16_t wr = w[2 * (WI)], wi = w[2 * (WI) + 1];                                                               \
    vint32m1_t re = __riscv_vsub_vv_i32m1(__riscv_vwmul_vx_i32m1(DR, wr, vl), __riscv_vwmul_vx_i32m1(DI, wi, vl), vl); \
    vint32m1_t im = __riscv_vadd_vv_i32m1(__riscv_vwmul_vx_i32m1(DR, wi, vl), __riscv_vwmul_vx_i32m1(DI, wr, vl), vl); \
    vint16mf2_t rr = __riscv_vnclip_wx_i16mf2(re, 15 + sh, __RISCV_VXRM_RNU, vl);                                 \
    vint16mf2_t ii = __riscv_vnclip_wx_i16mf2(im, 15 + sh, __RISCV_VXRM_RNU, vl);                                 \
    __riscv_vsseg2e16_v_i16mf2x2((DST) + o, __riscv_vcreate_v_i16mf2x2(rr, ii), vl);                              \
  } while (0)
        STK_TW(d1r, d1i, 0, B1); STK_TW(d2r, d2i, 1, B2); STK_TW(d3r, d3i, 2, B3);
#undef STK_TW
        c += (int)vl;
      }
    }
    int16_t *t = a; a = b; b = t;
  }
  return a;
}

/* step-B: element-wise complex multiply A[i] *= btw[dir][i] over the N contiguous
 * elements (per-element / vector twiddle W_N^{s*k1*n2}). */
static void stk_twiddle(const stk_plan_t *pl, int dir, int16_t *A, int N)
{
  const int16_t *W = pl->btw[dir];
  for (size_t i = 0, n = (size_t)N; i < n;) {
    size_t vl = __riscv_vsetvl_e16mf2(n - i);
    size_t o = 2 * i;
    vint16mf2x2_t Xa = __riscv_vlseg2e16_v_i16mf2x2(A + o, vl), Xw = __riscv_vlseg2e16_v_i16mf2x2(W + o, vl);
    vint16mf2_t ar = __riscv_vget_v_i16mf2x2_i16mf2(Xa, 0), ai = __riscv_vget_v_i16mf2x2_i16mf2(Xa, 1);
    vint16mf2_t wr = __riscv_vget_v_i16mf2x2_i16mf2(Xw, 0), wi = __riscv_vget_v_i16mf2x2_i16mf2(Xw, 1);
    vint32m1_t re = __riscv_vsub_vv_i32m1(__riscv_vwmul_vv_i32m1(ar, wr, vl), __riscv_vwmul_vv_i32m1(ai, wi, vl), vl);
    vint32m1_t im = __riscv_vadd_vv_i32m1(__riscv_vwmul_vv_i32m1(ar, wi, vl), __riscv_vwmul_vv_i32m1(ai, wr, vl), vl);
    __riscv_vsseg2e16_v_i16mf2x2(A + o, __riscv_vcreate_v_i16mf2x2(
        __riscv_vnclip_wx_i16mf2(re, 15, __RISCV_VXRM_RNU, vl), __riscv_vnclip_wx_i16mf2(im, 15, __RISCV_VXRM_RNU, vl)), vl);
    i += vl;
  }
}

/* transpose the [N1][N2] complex array A into At[N2][N1]. Complex = one u32; one
 * strided store per source row (vsse32 -- the only non-unit-stride access). */
static void stk_transpose(const int16_t *A, int16_t *At, int N1, int N2)
{
  for (int k1 = 0; k1 < N1; k1++) {
    const uint32_t *src = (const uint32_t *)(A + 2 * (size_t)k1 * N2);
    uint32_t *dst = (uint32_t *)At + k1; /* column k1 of At[N2][N1] */
    for (int n2 = 0; n2 < N2;) {
      size_t vl = __riscv_vsetvl_e32m1(N2 - n2);
      __riscv_vsse32_v_u32m1(dst + (size_t)n2 * N1, (ptrdiff_t)N1 * 4, __riscv_vle32_v_u32m1(src + n2, vl), vl);
      n2 += (int)vl;
    }
  }
}

/* 4-step (Bailey) transform, size N = N1*N2 (pow-4 factors). scale!=0 applies
 * >>1 per radix-4 stage (total 1/sqrt(N)). fwd=0 inverse, fwd=1 forward. Every
 * load/store is unit-stride/segment except the one transpose (strided store).
 * Scratch is per-call (stack) so the shared const plan stays reentrant. */
static void stk_run(int N, const int16_t *in16, int16_t *out16, int scale, int fwd)
{
  const stk_plan_t *pl = stk_get(N);
  int N1 = pl->N1, N2 = pl->N2, dir = fwd ? 1 : 0, s = fwd ? -1 : 1, sh = scale ? 1 : 0;
  int16_t A[2 * STK_MAXN] __attribute__((aligned(32)));
  int16_t Bs[2 * STK_MAXN] __attribute__((aligned(32)));
  int16_t T[2 * STK_MAXN] __attribute__((aligned(32)));
  memcpy(A, in16, sizeof(int16_t) * 2 * N);
  int16_t *r = stk_subfft(pl, dir, 0, s, sh, A, Bs, N1, N2); /* pass A: size-N1 FFTs, batched over N2 */
  stk_twiddle(pl, dir, r, N);
  stk_transpose(r, T, N1, N2);                               /* [N1][N2] -> [N2][N1] */
  r = stk_subfft(pl, dir, 1, s, sh, T, A, N2, N1);           /* pass D: size-N2 FFTs, batched over N1 */
  memcpy(out16, r, sizeof(int16_t) * 2 * N);
}
static const stk_r2plan_t *stk_get_r2(int N)
{
  for (int i = 0; i < STK_NR2; i++)
    if (stk_r2plans[i].N == N)
      return &stk_r2plans[i];
  return 0;
}
/* radix-2 (N = 2*M) transform: one DIF butterfly (unit-stride over M points) into
 * a[n]=(x[n]+x[n+M])[*1/sqrt2], b[n]=(x[n]-x[n+M])*W_N^{s*n}[*1/sqrt2]; the inner
 * 4-step on each half; then a stride-2 interleave store (out[2r]=A[r],out[2r+1]=B[r]).
 * Scratch is per-call (stack) -- reentrant like stk_run. */
static void stk_run2(int N, const int16_t *in16, int16_t *out16, int scale, int fwd)
{
  const stk_r2plan_t *pl = stk_get_r2(N);
  int M = pl->M;
  const int16_t *tw = pl->r2tw[fwd ? 1 : 0][scale ? 1 : 0];
  int16_t a[2 * STK_MAXM] __attribute__((aligned(32)));
  int16_t b[2 * STK_MAXM] __attribute__((aligned(32)));
  int16_t Ao[2 * STK_MAXM] __attribute__((aligned(32)));
  int16_t Bo[2 * STK_MAXM] __attribute__((aligned(32)));
  for (int n = 0; n < M;) {
    size_t vl = __riscv_vsetvl_e16mf2(M - n);
    int o = 2 * n;
    vint16mf2x2_t Xl = __riscv_vlseg2e16_v_i16mf2x2(in16 + o, vl);
    vint16mf2x2_t Xh = __riscv_vlseg2e16_v_i16mf2x2(in16 + 2 * M + o, vl);
    vint16mf2_t lr = __riscv_vget_v_i16mf2x2_i16mf2(Xl, 0), li = __riscv_vget_v_i16mf2x2_i16mf2(Xl, 1);
    vint16mf2_t hr = __riscv_vget_v_i16mf2x2_i16mf2(Xh, 0), hi = __riscv_vget_v_i16mf2x2_i16mf2(Xh, 1);
    vint16mf2_t sr = __riscv_vsadd_vv_i16mf2(lr, hr, vl), si = __riscv_vsadd_vv_i16mf2(li, hi, vl);
    vint16mf2_t dr = __riscv_vssub_vv_i16mf2(lr, hr, vl), di = __riscv_vssub_vv_i16mf2(li, hi, vl);
    vint16mf2_t ar, ai;
    if (scale) {
      ar = __riscv_vnclip_wx_i16mf2(__riscv_vwmul_vx_i32m1(sr, STK_SQRT2INV, vl), 15, __RISCV_VXRM_RNU, vl);
      ai = __riscv_vnclip_wx_i16mf2(__riscv_vwmul_vx_i32m1(si, STK_SQRT2INV, vl), 15, __RISCV_VXRM_RNU, vl);
    } else {
      ar = sr;
      ai = si;
    }
    __riscv_vsseg2e16_v_i16mf2x2(a + o, __riscv_vcreate_v_i16mf2x2(ar, ai), vl);
    vint16mf2x2_t W = __riscv_vlseg2e16_v_i16mf2x2(tw + o, vl);
    vint16mf2_t wr = __riscv_vget_v_i16mf2x2_i16mf2(W, 0), wi = __riscv_vget_v_i16mf2x2_i16mf2(W, 1);
    vint32m1_t bre = __riscv_vsub_vv_i32m1(__riscv_vwmul_vv_i32m1(dr, wr, vl), __riscv_vwmul_vv_i32m1(di, wi, vl), vl);
    vint32m1_t bim = __riscv_vadd_vv_i32m1(__riscv_vwmul_vv_i32m1(dr, wi, vl), __riscv_vwmul_vv_i32m1(di, wr, vl), vl);
    __riscv_vsseg2e16_v_i16mf2x2(b + o, __riscv_vcreate_v_i16mf2x2(
        __riscv_vnclip_wx_i16mf2(bre, 15, __RISCV_VXRM_RNU, vl), __riscv_vnclip_wx_i16mf2(bim, 15, __RISCV_VXRM_RNU, vl)), vl);
    n += (int)vl;
  }
  stk_run(M, a, Ao, scale, fwd);
  stk_run(M, b, Bo, scale, fwd);
  uint32_t *dst = (uint32_t *)out16;
  const uint32_t *sA = (const uint32_t *)Ao, *sB = (const uint32_t *)Bo;
  for (int r = 0; r < M;) {
    size_t vl = __riscv_vsetvl_e32m1(M - r);
    __riscv_vsse32_v_u32m1(dst + 2 * r, 8, __riscv_vle32_v_u32m1(sA + r, vl), vl);
    __riscv_vsse32_v_u32m1(dst + 2 * r + 1, 8, __riscv_vle32_v_u32m1(sB + r, vl), vl);
    r += (int)vl;
  }
}
static const stk_r3plan_t *stk_get_r3(int N)
{
  for (int i = 0; i < STK_NR3; i++)
    if (stk_r3plans[i].N == N)
      return &stk_r3plans[i];
  return 0;
}
/* radix-3 (N = 3*M) transform: one 3-point DIF butterfly (unit-stride over M) ->
 * b0/b1/b2, the inner M-DFT on each (stk_run pow-4 M, or stk_run2 for M=2*pow-4),
 * then a stride-3 interleave store (out[3r+q]=Cq[r]). Per-call stack scratch. */
static void stk_run3(int N, const int16_t *in16, int16_t *out16, int scale, int fwd)
{
  const stk_r3plan_t *pl = stk_get_r3(N);
  int M = pl->M, dir = fwd ? 1 : 0;
  const int16_t *w1 = pl->r3tw1[dir][scale ? 1 : 0], *w2 = pl->r3tw2[dir][scale ? 1 : 0];
  int16_t b0[2 * STK_MAXM] __attribute__((aligned(32)));
  int16_t b1[2 * STK_MAXM] __attribute__((aligned(32)));
  int16_t b2[2 * STK_MAXM] __attribute__((aligned(32)));
  int16_t C0[2 * STK_MAXM] __attribute__((aligned(32)));
  int16_t C1[2 * STK_MAXM] __attribute__((aligned(32)));
  int16_t C2[2 * STK_MAXM] __attribute__((aligned(32)));
  for (int n = 0; n < M;) {
    size_t vl = __riscv_vsetvl_e16mf2(M - n);
    int o = 2 * n;
    vint16mf2x2_t X0 = __riscv_vlseg2e16_v_i16mf2x2(in16 + o, vl);
    vint16mf2x2_t X1 = __riscv_vlseg2e16_v_i16mf2x2(in16 + 2 * M + o, vl);
    vint16mf2x2_t X2 = __riscv_vlseg2e16_v_i16mf2x2(in16 + 4 * M + o, vl);
    vint16mf2_t x0r = __riscv_vget_v_i16mf2x2_i16mf2(X0, 0), x0i = __riscv_vget_v_i16mf2x2_i16mf2(X0, 1);
    vint16mf2_t x1r = __riscv_vget_v_i16mf2x2_i16mf2(X1, 0), x1i = __riscv_vget_v_i16mf2x2_i16mf2(X1, 1);
    vint16mf2_t x2r = __riscv_vget_v_i16mf2x2_i16mf2(X2, 0), x2i = __riscv_vget_v_i16mf2x2_i16mf2(X2, 1);
    vint16mf2_t sr = __riscv_vsadd_vv_i16mf2(x1r, x2r, vl), si = __riscv_vsadd_vv_i16mf2(x1i, x2i, vl);
    vint16mf2_t dr = __riscv_vssub_vv_i16mf2(x1r, x2r, vl), di = __riscv_vssub_vv_i16mf2(x1i, x2i, vl);
    vint16mf2_t ar = __riscv_vssub_vv_i16mf2(x0r, __riscv_vssra_vx_i16mf2(sr, 1, __RISCV_VXRM_RNU, vl), vl);
    vint16mf2_t ai = __riscv_vssub_vv_i16mf2(x0i, __riscv_vssra_vx_i16mf2(si, 1, __RISCV_VXRM_RNU, vl), vl);
    vint16mf2_t r3r = __riscv_vnclip_wx_i16mf2(__riscv_vwmul_vx_i32m1(dr, STK_SQRT3_2, vl), 15, __RISCV_VXRM_RNU, vl);
    vint16mf2_t r3i = __riscv_vnclip_wx_i16mf2(__riscv_vwmul_vx_i32m1(di, STK_SQRT3_2, vl), 15, __RISCV_VXRM_RNU, vl);
    vint16mf2_t rotr, roti;
    if (dir) { rotr = r3i; roti = __riscv_vneg_v_i16mf2(r3r, vl); }   /* forward (s=-1) */
    else     { rotr = __riscv_vneg_v_i16mf2(r3i, vl); roti = r3r; }   /* inverse (s=+1) */
    vint16mf2_t c1r = __riscv_vsadd_vv_i16mf2(ar, rotr, vl), c1i = __riscv_vsadd_vv_i16mf2(ai, roti, vl);
    vint16mf2_t c2r = __riscv_vssub_vv_i16mf2(ar, rotr, vl), c2i = __riscv_vssub_vv_i16mf2(ai, roti, vl);
    vint16mf2_t b0r, b0i;
    if (scale) {
      b0r = __riscv_vnclip_wx_i16mf2(__riscv_vwmul_vx_i32m1(__riscv_vsadd_vv_i16mf2(x0r, sr, vl), STK_SQRT3INV, vl), 15, __RISCV_VXRM_RNU, vl);
      b0i = __riscv_vnclip_wx_i16mf2(__riscv_vwmul_vx_i32m1(__riscv_vsadd_vv_i16mf2(x0i, si, vl), STK_SQRT3INV, vl), 15, __RISCV_VXRM_RNU, vl);
    } else {
      b0r = __riscv_vsadd_vv_i16mf2(x0r, sr, vl);
      b0i = __riscv_vsadd_vv_i16mf2(x0i, si, vl);
    }
    __riscv_vsseg2e16_v_i16mf2x2(b0 + o, __riscv_vcreate_v_i16mf2x2(b0r, b0i), vl);
    vint16mf2x2_t W1 = __riscv_vlseg2e16_v_i16mf2x2(w1 + o, vl);
    vint16mf2_t w1r = __riscv_vget_v_i16mf2x2_i16mf2(W1, 0), w1i = __riscv_vget_v_i16mf2x2_i16mf2(W1, 1);
    vint32m1_t b1re = __riscv_vsub_vv_i32m1(__riscv_vwmul_vv_i32m1(c1r, w1r, vl), __riscv_vwmul_vv_i32m1(c1i, w1i, vl), vl);
    vint32m1_t b1im = __riscv_vadd_vv_i32m1(__riscv_vwmul_vv_i32m1(c1r, w1i, vl), __riscv_vwmul_vv_i32m1(c1i, w1r, vl), vl);
    __riscv_vsseg2e16_v_i16mf2x2(b1 + o, __riscv_vcreate_v_i16mf2x2(
        __riscv_vnclip_wx_i16mf2(b1re, 15, __RISCV_VXRM_RNU, vl), __riscv_vnclip_wx_i16mf2(b1im, 15, __RISCV_VXRM_RNU, vl)), vl);
    vint16mf2x2_t W2 = __riscv_vlseg2e16_v_i16mf2x2(w2 + o, vl);
    vint16mf2_t w2r = __riscv_vget_v_i16mf2x2_i16mf2(W2, 0), w2i = __riscv_vget_v_i16mf2x2_i16mf2(W2, 1);
    vint32m1_t b2re = __riscv_vsub_vv_i32m1(__riscv_vwmul_vv_i32m1(c2r, w2r, vl), __riscv_vwmul_vv_i32m1(c2i, w2i, vl), vl);
    vint32m1_t b2im = __riscv_vadd_vv_i32m1(__riscv_vwmul_vv_i32m1(c2r, w2i, vl), __riscv_vwmul_vv_i32m1(c2i, w2r, vl), vl);
    __riscv_vsseg2e16_v_i16mf2x2(b2 + o, __riscv_vcreate_v_i16mf2x2(
        __riscv_vnclip_wx_i16mf2(b2re, 15, __RISCV_VXRM_RNU, vl), __riscv_vnclip_wx_i16mf2(b2im, 15, __RISCV_VXRM_RNU, vl)), vl);
    n += (int)vl;
  }
  if (pl->Mrad2) {
    stk_run2(M, b0, C0, scale, fwd);
    stk_run2(M, b1, C1, scale, fwd);
    stk_run2(M, b2, C2, scale, fwd);
  } else {
    stk_run(M, b0, C0, scale, fwd);
    stk_run(M, b1, C1, scale, fwd);
    stk_run(M, b2, C2, scale, fwd);
  }
  uint32_t *dst = (uint32_t *)out16;
  const uint32_t *s0 = (const uint32_t *)C0, *s1 = (const uint32_t *)C1, *s2 = (const uint32_t *)C2;
  for (int r = 0; r < M;) {   /* interleave out[3r+q]=Cq[r] */
    size_t vl = __riscv_vsetvl_e32m1(M - r);
    __riscv_vsse32_v_u32m1(dst + 3 * r, 12, __riscv_vle32_v_u32m1(s0 + r, vl), vl);
    __riscv_vsse32_v_u32m1(dst + 3 * r + 1, 12, __riscv_vle32_v_u32m1(s1 + r, vl), vl);
    __riscv_vsse32_v_u32m1(dst + 3 * r + 2, 12, __riscv_vle32_v_u32m1(s2 + r, vl), vl);
    r += (int)vl;
  }
}
#endif /* __riscv_vector */

static inline int16_t rv_sat16(int64_t v)
{
  return v > INT16_MAX ? INT16_MAX : (v < INT16_MIN ? INT16_MIN : (int16_t)v);
}
static inline int32_t rv_sat32(int64_t v)
{
  return v > INT32_MAX ? INT32_MAX : (v < INT32_MIN ? INT32_MIN : (int32_t)v);
}
// (v + 2^14) >> 15 saturated to int16: vnclip with __RISCV_VXRM_RNU
static inline int16_t rv_q15(int64_t v)
{
  return rv_sat16((v + (1 << 14)) >> 15);
}

/* One mixed-radix stage of size N = r * M. dir 0 = inverse (exponent sign +1), 1 = forward (-1), as stk_*.
 * tw[dir][(j - 1) * M + k] = W_N^{s j k} (Q15, j = 1..r-1); C[dir][q][j] = W_r^{s j q} / sqrt(r) (Q15). */
#define RV_MAXR 5
typedef struct {
  int N, r, M;
  c16_t *tw[2];
  c16_t C[2][RV_MAXR][RV_MAXR];
} rv_stage_t;

static int rv_radix(int N)
{
  return N % 5 == 0 ? 5 : N % 3 == 0 ? 3 : N % 4 == 0 ? 4 : 2;
}

#define RV_MAXSTAGE 1024
static rv_stage_t *rv_stages[RV_MAXSTAGE];
static int rv_nstage;
static pthread_mutex_t rv_stage_mutex = PTHREAD_MUTEX_INITIALIZER;

static c16_t rv_cexp_q15(double ang, double mag)
{
  return (c16_t){rv_sat16(lround(cos(ang) * mag)), rv_sat16(lround(sin(ang) * mag))};
}

static rv_stage_t *rv_stage_build(int N)
{
  rv_stage_t *st = calloc(1, sizeof(*st));
  AssertFatal(st, "out of memory\n");
  st->N = N;
  st->r = rv_radix(N);
  st->M = N / st->r;
  for (int dir = 0; dir < 2; dir++) {
    const double s = dir ? -1.0 : 1.0;
    st->tw[dir] = malloc(sizeof(c16_t) * (st->r - 1) * st->M);
    AssertFatal(st->tw[dir], "out of memory\n");
    for (int j = 1; j < st->r; j++)
      for (int k = 0; k < st->M; k++)
        st->tw[dir][(j - 1) * st->M + k] = rv_cexp_q15(s * 2.0 * M_PI * j * k / N, 32767.0);
    for (int q = 0; q < st->r; q++)
      for (int j = 0; j < st->r; j++)
        st->C[dir][q][j] = rv_cexp_q15(s * 2.0 * M_PI * ((j * q) % st->r) / st->r, 32768.0 / sqrt(st->r));
  }
  return st;
}

// Stages are built on first use and never freed; readers scan the published prefix lock-free.
static const rv_stage_t *rv_stage_get(int N)
{
  const int n = __atomic_load_n(&rv_nstage, __ATOMIC_ACQUIRE);
  for (int i = 0; i < n; i++)
    if (rv_stages[i]->N == N)
      return rv_stages[i];
  pthread_mutex_lock(&rv_stage_mutex);
  rv_stage_t *st = NULL;
  for (int i = 0; i < rv_nstage; i++)
    if (rv_stages[i]->N == N)
      st = rv_stages[i];
  if (!st) {
    AssertFatal(rv_nstage < RV_MAXSTAGE, "too many DFT stage sizes\n");
    st = rv_stage_build(N);
    rv_stages[rv_nstage] = st;
    __atomic_store_n(&rv_nstage, rv_nstage + 1, __ATOMIC_RELEASE);
  }
  pthread_mutex_unlock(&rv_stage_mutex);
  return st;
}

/* y[q * M + k] = sum_j C[q][j] * a_j[k], a_0 = T_0[k], a_j = q15(T_j[k] * tw_j[k]): saturating int32 accumulation
 * in j order (real: + Cr ar, - Ci ai; imag: + Cr ai, + Ci ar), then q15. */
static inline void rv_combine_scalar(const rv_stage_t *st, int dir, const c16_t *T, c16_t *y, int k0, int k1)
{
  const int r = st->r, M = st->M;
  for (int k = k0; k < k1; k++) {
    c16_t a[RV_MAXR];
    a[0] = T[k];
    for (int j = 1; j < r; j++) {
      const c16_t t = T[j * M + k], w = st->tw[dir][(j - 1) * M + k];
      a[j] = (c16_t){rv_q15((int32_t)t.r * w.r - (int32_t)t.i * w.i), rv_q15((int32_t)t.r * w.i + (int32_t)t.i * w.r)};
    }
    for (int q = 0; q < r; q++) {
      int32_t re = 0, im = 0;
      for (int j = 0; j < r; j++) {
        const c16_t c = st->C[dir][q][j];
        re = rv_sat32((int64_t)re + (int32_t)c.r * a[j].r);
        re = rv_sat32((int64_t)re - (int32_t)c.i * a[j].i);
        im = rv_sat32((int64_t)im + (int32_t)c.r * a[j].i);
        im = rv_sat32((int64_t)im + (int32_t)c.i * a[j].r);
      }
      y[q * M + k] = (c16_t){rv_q15(re), rv_q15(im)};
    }
  }
}

#ifdef __riscv_vector
// RVV types cannot be array elements: the r <= 5 operands are named, and rv_combine_r is specialized per radix
#define RV_TW(J)                                                                                                     \
  if (J < r) {                                                                                                       \
    vint16mf2x2_t X = __riscv_vlseg2e16_v_i16mf2x2(T16 + 2 * (J * M + k), vl);                                       \
    vint16mf2x2_t W = __riscv_vlseg2e16_v_i16mf2x2((const int16_t *)(st->tw[dir] + (J - 1) * M + k), vl);           \
    vint16mf2_t tr = __riscv_vget_v_i16mf2x2_i16mf2(X, 0), ti = __riscv_vget_v_i16mf2x2_i16mf2(X, 1);               \
    vint16mf2_t wr = __riscv_vget_v_i16mf2x2_i16mf2(W, 0), wi = __riscv_vget_v_i16mf2x2_i16mf2(W, 1);               \
    vint32m1_t re = __riscv_vsub_vv_i32m1(__riscv_vwmul_vv_i32m1(tr, wr, vl), __riscv_vwmul_vv_i32m1(ti, wi, vl), vl); \
    vint32m1_t im = __riscv_vadd_vv_i32m1(__riscv_vwmul_vv_i32m1(tr, wi, vl), __riscv_vwmul_vv_i32m1(ti, wr, vl), vl); \
    a##J##r = __riscv_vnclip_wx_i16mf2(re, 15, __RISCV_VXRM_RNU, vl);                                               \
    a##J##i = __riscv_vnclip_wx_i16mf2(im, 15, __RISCV_VXRM_RNU, vl);                                               \
  }
#define RV_ACC(J)                                                                                \
  if (J < r) {                                                                                   \
    const c16_t c = st->C[dir][q][J];                                                            \
    re = __riscv_vsadd_vv_i32m1(re, __riscv_vwmul_vx_i32m1(a##J##r, c.r, vl), vl);              \
    re = __riscv_vssub_vv_i32m1(re, __riscv_vwmul_vx_i32m1(a##J##i, c.i, vl), vl);              \
    im = __riscv_vsadd_vv_i32m1(im, __riscv_vwmul_vx_i32m1(a##J##i, c.r, vl), vl);              \
    im = __riscv_vsadd_vv_i32m1(im, __riscv_vwmul_vx_i32m1(a##J##r, c.i, vl), vl);              \
  }
static inline __attribute__((always_inline)) void rv_combine_r(const rv_stage_t *st, int dir, const c16_t *T, c16_t *y, const int r)
{
  const int M = st->M;
  const int16_t *T16 = (const int16_t *)T;
  int16_t *y16 = (int16_t *)y;
  for (int k = 0; k < M;) {
    const size_t vl = __riscv_vsetvl_e16mf2(M - k);
    vint16mf2x2_t X0 = __riscv_vlseg2e16_v_i16mf2x2(T16 + 2 * k, vl);
    vint16mf2_t a0r = __riscv_vget_v_i16mf2x2_i16mf2(X0, 0), a0i = __riscv_vget_v_i16mf2x2_i16mf2(X0, 1);
    vint16mf2_t a1r = a0r, a1i = a0i, a2r = a0r, a2i = a0i, a3r = a0r, a3i = a0i, a4r = a0r, a4i = a0i;
    RV_TW(1) RV_TW(2) RV_TW(3) RV_TW(4)
    for (int q = 0; q < r; q++) {
      vint32m1_t re = __riscv_vmv_v_x_i32m1(0, vl), im = __riscv_vmv_v_x_i32m1(0, vl);
      RV_ACC(0) RV_ACC(1) RV_ACC(2) RV_ACC(3) RV_ACC(4)
      __riscv_vsseg2e16_v_i16mf2x2(y16 + 2 * (q * M + k),
                                   __riscv_vcreate_v_i16mf2x2(__riscv_vnclip_wx_i16mf2(re, 15, __RISCV_VXRM_RNU, vl),
                                                              __riscv_vnclip_wx_i16mf2(im, 15, __RISCV_VXRM_RNU, vl)),
                                   vl);
    }
    k += (int)vl;
  }
}
#undef RV_TW
#undef RV_ACC

static void rv_combine(const rv_stage_t *st, int dir, const c16_t *T, c16_t *y)
{
  switch (st->r) {
    case 2:
      rv_combine_r(st, dir, T, y, 2);
      break;
    case 3:
      rv_combine_r(st, dir, T, y, 3);
      break;
    case 4:
      rv_combine_r(st, dir, T, y, 4);
      break;
    default:
      rv_combine_r(st, dir, T, y, 5);
  }
}

// whole-transform RVV kernels (contiguous in/out, scale = 1/sqrt(N))
static bool rv_stk(int N, const c16_t *x, c16_t *y, int fwd)
{
  if (stk_get(N))
    stk_run(N, (const int16_t *)x, (int16_t *)y, 1, fwd);
  else if (stk_get_r2(N))
    stk_run2(N, (const int16_t *)x, (int16_t *)y, 1, fwd);
  else if (stk_get_r3(N))
    stk_run3(N, (const int16_t *)x, (int16_t *)y, 1, fwd);
  else
    return false;
  return true;
}
static bool rv_stk_has(int N)
{
  return stk_get(N) || stk_get_r2(N) || stk_get_r3(N);
}
#else
static void rv_combine(const rv_stage_t *st, int dir, const c16_t *T, c16_t *y)
{
  rv_combine_scalar(st, dir, T, y, 0, st->M);
}
static bool rv_stk(int N, const c16_t *x, c16_t *y, int fwd)
{
  return false;
}
static bool rv_stk_has(int N)
{
  return false;
}
#endif

/* DFT of the N points src[0], src[stride], ... into contiguous dst. scr: scratch of rv_scratch_len(N) points. */
static void rv_dft(const c16_t *src, int stride, c16_t *dst, int N, int dir, c16_t *scr)
{
  if (N == 1) {
    dst[0] = src[0];
    return;
  }
  if (rv_stk_has(N)) {
    if (stride != 1) {
      for (int n = 0; n < N; n++)
        scr[n] = src[n * stride];
      src = scr;
    }
    rv_stk(N, src, dst, dir);
    return;
  }
  const rv_stage_t *st = rv_stage_get(N);
  const int r = st->r, M = st->M;
  for (int j = 0; j < r; j++)
    rv_dft(src + j * stride, stride * r, scr + j * M, M, dir, scr + N);
  rv_combine(st, dir, scr, dst);
}

// each level holds its N sub-transform outputs, plus the gather buffer of a whole-kernel leaf: < 3N
static size_t rv_scratch_len(int N)
{
  return 3 * (size_t)N + 16;
}

static __thread c16_t *rv_scr;
static __thread size_t rv_scr_len;

static void rv_transform(int N, const int16_t *input, int16_t *output, int dir)
{
  const size_t need = rv_scratch_len(N);
  if (rv_scr_len < need) {
    free(rv_scr);
    rv_scr = aligned_alloc(64, ((need * sizeof(c16_t)) + 63) & ~(size_t)63);
    AssertFatal(rv_scr, "out of memory\n");
    rv_scr_len = need;
  }
  rv_dft((const c16_t *)input, 1, (c16_t *)output, N, dir, rv_scr);
}

#define RV_SZ(Sz) Sz,
static const int rv_dft_size[] = {FOREACH_DFTSZ(RV_SZ)};
static const int rv_idft_size[] = {FOREACH_IDFTSZ(RV_SZ)};
#undef RV_SZ

int dfts_autoinit(void)
{
  return 0;
}

void dft_implementation(uint8_t sizeidx, int16_t *input, int16_t *output, unsigned char scale_flag)
{
  (void)scale_flag;
  AssertFatal(sizeidx < DFT_SIZE_IDXTABLESIZE, "Invalid dft size index %i\n", sizeidx);
  rv_transform(rv_dft_size[sizeidx], input, output, 1);
}

void idft_implementation(uint8_t sizeidx, int16_t *input, int16_t *output, unsigned char scale_flag)
{
  (void)scale_flag;
  AssertFatal(sizeidx < IDFT_SIZE_IDXTABLESIZE, "Invalid idft size index %i\n", sizeidx);
  rv_transform(rv_idft_size[sizeidx], input, output, 0);
}

#endif /* __riscv || OAI_DFTS_GENERIC_HOST */
