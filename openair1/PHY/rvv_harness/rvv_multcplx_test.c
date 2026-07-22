/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * RVV mult_complex_vectors (tools_defs.h): out[i] = c16mulShift(in1[i], in2[i],
 * shift), i.e. a per-element complex multiply with a wrapping arithmetic >>shift
 * (no saturation -- matches the scalar c16mulShift cast to int16).
 *
 * Port pattern (production-matching): full-VLMAX vlseg2/vwmul/vnsra/vsseg2 body +
 * the existing scalar c16mulShift tail. Keeping vl==VLMAX on the segment store
 * dodges the spacemit-gcc -O2 partial-vl segment-store over-store (VLEN=1024).
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <unistd.h>
#include <riscv_vector.h>

typedef struct { int16_t r, i; } c16_t;

static int pin_cpu(int c){ cpu_set_t s; CPU_ZERO(&s); CPU_SET(c,&s); return sched_setaffinity(0,sizeof(s),&s); }
static int use_ai(void){ FILE*f=fopen("/proc/set_ai_thread","w"); if(!f)return -1; fprintf(f,"%ld\n",(long)getpid()); return fclose(f); }

/* scalar ground truth (identical to tools_defs.h c16mulShift) */
static c16_t c16mulShift(const c16_t a, const c16_t b, const int Shift){
  return (c16_t){ .r=(int16_t)((a.r*b.r - a.i*b.i) >> Shift),
                  .i=(int16_t)((a.r*b.i + a.i*b.r) >> Shift) };
}
static void mult_ref(const c16_t *in1, const c16_t *in2, c16_t *out, int size, int shift){
  for (int i = 0; i < size; i++) out[i] = c16mulShift(in1[i], in2[i], shift);
}

/* RVV kernel under test (== the tools_defs.h __riscv branch) */
static void mult_rvv(const c16_t *in1, const c16_t *in2, c16_t *out, int size, int shift){
  size_t vlmax = __riscv_vsetvlmax_e16m1();
  int i = 0;
  for (; i + (int)vlmax <= size; i += (int)vlmax) {
    vint16m1x2_t a = __riscv_vlseg2e16_v_i16m1x2((const int16_t *)(in1 + i), vlmax);
    vint16m1x2_t b = __riscv_vlseg2e16_v_i16m1x2((const int16_t *)(in2 + i), vlmax);
    vint16m1_t ar = __riscv_vget_v_i16m1x2_i16m1(a, 0), ai = __riscv_vget_v_i16m1x2_i16m1(a, 1);
    vint16m1_t br = __riscv_vget_v_i16m1x2_i16m1(b, 0), bi = __riscv_vget_v_i16m1x2_i16m1(b, 1);
    vint32m2_t re = __riscv_vsub_vv_i32m2(__riscv_vwmul_vv_i32m2(ar, br, vlmax), __riscv_vwmul_vv_i32m2(ai, bi, vlmax), vlmax);
    vint32m2_t im = __riscv_vadd_vv_i32m2(__riscv_vwmul_vv_i32m2(ar, bi, vlmax), __riscv_vwmul_vv_i32m2(ai, br, vlmax), vlmax);
    vint16m1_t outr = __riscv_vnsra_wx_i16m1(re, shift, vlmax);   /* wrapping >>shift narrow */
    vint16m1_t outi = __riscv_vnsra_wx_i16m1(im, shift, vlmax);
    __riscv_vsseg2e16_v_i16m1x2((int16_t *)(out + i), __riscv_vcreate_v_i16m1x2(outr, outi), vlmax);
  }
  for (; i < size; i++) out[i] = c16mulShift(in1[i], in2[i], shift);
}

static uint32_t rng = 424242u;
static int16_t rnd16(void){ rng = rng*1103515245u+12345u; return (int16_t)(rng >> 16); }

__attribute__((optimize("no-tree-vectorize")))   /* scaffolding: dodge -O2 VLEN=1024 auto-vec bug */
int main(int argc, char **argv){
  if (argc > 1){ int c=atoi(argv[1]); if(c>7) use_ai(); pin_cpu(c); }
  int sizes[] = { 4, 7, 13, 100, 273*12, 4096 };
  int shifts[] = { 0, 8, 14, 15 };
  int fail = 0;
  for (unsigned t = 0; t < sizeof(sizes)/sizeof(sizes[0]); t++){
    int n = sizes[t];
    c16_t *a = malloc(sizeof(c16_t)*n), *b = malloc(sizeof(c16_t)*n), *r = malloc(sizeof(c16_t)*n), *g = malloc(sizeof(c16_t)*n);
    for (int i = 0; i < n; i++){ a[i].r=rnd16(); a[i].i=rnd16(); b[i].r=rnd16(); b[i].i=rnd16(); }
    for (unsigned s = 0; s < sizeof(shifts)/sizeof(shifts[0]); s++){
      int sh = shifts[s];
      mult_ref(a, b, r, n, sh);
      mult_rvv(a, b, g, n, sh);
      int d = 0;
      for (int i = 0; i < n; i++) if (g[i].r != r[i].r || g[i].i != r[i].i) d++;
      printf("  n=%5d shift=%2d: diff=%d %s\n", n, sh, d, d?"FAIL":"OK");
      if (d) fail = 1;
    }
    free(a); free(b); free(r); free(g);
  }
  printf("rvv mult_complex_vectors test: %s\n", fail?"FAIL":"PASS");
  return fail;
}
