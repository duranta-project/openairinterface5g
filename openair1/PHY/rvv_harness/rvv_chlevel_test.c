/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * RVV nr_channel_level + nr_scale_channel (nr_phy_common.c).
 *
 * nr_channel_level: avg = (sum_k ((re_k^2+im_k^2) >> shift)) / scale, summed over
 *   floor(len/4)*4 complex (matches the RISC-V simde_mm_average_sse path, which
 *   loops length>>2 over 4-complex blocks and drops the len%4 tail). vlseg2 (re/im)
 *   + vwmul/vwmacc (|c|^2) + vsra + widening reduction (vwredsum).
 * nr_scale_channel: arithmetic >>shift on every int16 component in place.
 *
 * LAUNCH NOTE (A100 / VLEN=1024): /proc/set_ai_thread changes VLEN at runtime, and
 * VLEN must be constant for a function's frame. So run_tests() (all the vector
 * work) is a separate noinline function called AFTER the core switch -- its
 * prologue then reads the post-switch vlenb and sizes spill slots correctly.
 * main() stays scalar. Equivalently, launch already pinned:
 *   sh -c 'echo $$ >/proc/set_ai_thread; exec ./rvv_chlevel_test 8'
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

/* --- scalar references (match the RISC-V simde path) --- */
static int32_t chlevel_ref(const c16_t *ch, int len, int shift, int16_t scale){
  int64_t avg = 0;
  for (int i = 0; i < (len >> 2); i++)       /* 4-complex blocks; len%4 dropped */
    for (int k = 0; k < 4; k++){
      const c16_t c = ch[i*4 + k];
      avg += ((int32_t)c.r*c.r + (int32_t)c.i*c.i) >> shift;
    }
  return (int32_t)(uint32_t)(avg / scale);
}
static void scale_ref(c16_t *ch, int len, int shift){
  for (int i = 0; i < len; i++){ ch[i].r >>= shift; ch[i].i >>= shift; }
}

/* --- RVV kernels under test (== the nr_phy_common.c __riscv branches) --- */
static int32_t chlevel_rvv(const c16_t *ch, int len, int shift, int16_t scale){
  const int16_t *p = (const int16_t *)ch;
  int64_t acc = 0;
  int nc = len & ~3;
  for (int n = 0; n < nc;){
    size_t vl = __riscv_vsetvl_e16mf2(nc - n);
    vint16mf2x2_t v = __riscv_vlseg2e16_v_i16mf2x2(p + 2*n, vl);
    vint16mf2_t cr = __riscv_vget_v_i16mf2x2_i16mf2(v, 0), ci = __riscv_vget_v_i16mf2x2_i16mf2(v, 1);
    vint32m1_t pw = __riscv_vwmacc_vv_i32m1(__riscv_vwmul_vv_i32m1(cr, cr, vl), ci, ci, vl);
    pw = __riscv_vsra_vx_i32m1(pw, shift, vl);
    acc += __riscv_vmv_x_s_i64m1_i64(__riscv_vwredsum_vs_i32m1_i64m1(pw, __riscv_vmv_s_x_i64m1(0, 1), vl));
    n += (int)vl;
  }
  return (int32_t)(uint32_t)(acc / scale);
}
static void scale_rvv(c16_t *ch, int len, int shift){
  int16_t *p = (int16_t *)ch;
  for (int n = 0; n < 2*len;){
    size_t vl = __riscv_vsetvl_e16m1(2*len - n);
    vint16m1_t v = __riscv_vle16_v_i16m1(p + n, vl);
    __riscv_vse16_v_i16m1(p + n, __riscv_vsra_vx_i16m1(v, shift, vl), vl);
    n += (int)vl;
  }
}

static uint32_t rng = 0xC0FFEEu;
static int16_t rnd16(void){ rng = rng*1103515245u+12345u; return (int16_t)(rng >> 16); }

/* all vector work lives here, entered AFTER the core switch (correct vlenb) */
__attribute__((noinline)) static int run_tests(void){
  int lens[] = { 4, 12, 106*12, 273*12, 100, 99, 26 };   /* incl len%4 != 0 */
  int fail = 0;
  for (unsigned t = 0; t < sizeof(lens)/sizeof(lens[0]); t++){
    int len = lens[t];
    c16_t *ch = malloc(sizeof(c16_t)*len), *tmp = malloc(sizeof(c16_t)*len), *tmp2 = malloc(sizeof(c16_t)*len);
    for (int i = 0; i < len; i++){ ch[i].r = rnd16(); ch[i].i = rnd16(); }
    for (int shift = 0; shift <= 4; shift += 2){
      int16_t scale = (len >> 1) | 1;   /* nonzero */
      int32_t lr = chlevel_ref(ch, len, shift, scale), lg = chlevel_rvv(ch, len, shift, scale);
      for (int i=0;i<len;i++){ tmp[i]=ch[i]; tmp2[i]=ch[i]; }
      scale_ref(tmp, len, shift); scale_rvv(tmp2, len, shift);
      int sd = 0; for (int i=0;i<len;i++) if (tmp[i].r!=tmp2[i].r || tmp[i].i!=tmp2[i].i) sd++;
      printf("  len=%5d shift=%d: level ref=%d rvv=%d %s  scale diff=%d %s\n",
             len, shift, lr, lg, lr==lg?"OK":"FAIL", sd, sd?"FAIL":"OK");
      if (lr != lg || sd) fail = 1;
    }
    free(ch); free(tmp); free(tmp2);
  }
  return fail;
}

int main(int argc, char **argv){
  if (argc > 1){ int c=atoi(argv[1]); if(c>7) use_ai(); pin_cpu(c); }  /* switch core FIRST */
  printf("VLEN=%zu\n", (size_t)__riscv_vlenb()*8);
  int fail = run_tests();                                              /* then all vector work */
  printf("rvv channel_level/scale test: %s\n", fail?"FAIL":"PASS");
  return fail;
}
