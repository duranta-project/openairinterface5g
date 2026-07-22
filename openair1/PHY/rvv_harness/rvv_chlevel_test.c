/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * RVV nr_scale_channel (nr_phy_common.c): arithmetic >>shift on every int16
 * component of the extracted channel estimates, in place. Trivial VLA
 * vle16/vsra/vse16 over the c16 array as an int16 stream.
 *
 * (nr_channel_level was prototyped here too but DEFERRED: its power reduction
 * needs a widening integer reduction, and spacemit-gcc's vwredsum corrupts the
 * stack at VLEN=1024 -- results are correct but the program faults at return,
 * regardless of LMUL or full-vl vs partial-vl. Kept on SIMDe until the toolchain
 * is fixed or a reduction that avoids vwredsum is validated.)
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

static void scale_ref(c16_t *ch, int len, int shift){
  for (int i = 0; i < len; i++){ ch[i].r >>= shift; ch[i].i >>= shift; }
}
/* == the nr_phy_common.c __riscv branch == */
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

__attribute__((optimize("no-tree-vectorize")))   /* scaffolding: -O2 VLEN=1024 auto-vec bug */
int main(int argc, char **argv){
  if (argc > 1){ int c=atoi(argv[1]); if(c>7) use_ai(); pin_cpu(c); }
  int lens[] = { 4, 12, 106*12, 273*12, 100, 99, 26 };   /* incl len%4 != 0 */
  int fail = 0;
  for (unsigned t = 0; t < sizeof(lens)/sizeof(lens[0]); t++){
    int len = lens[t];
    c16_t *a = malloc(sizeof(c16_t)*len), *b = malloc(sizeof(c16_t)*len);
    for (int shift = 0; shift <= 4; shift += 2){
      for (int i = 0; i < len; i++){ int16_t r=rnd16(), im=rnd16(); a[i].r=r; a[i].i=im; b[i].r=r; b[i].i=im; }
      scale_ref(a, len, shift); scale_rvv(b, len, shift);
      int d = 0; for (int i=0;i<len;i++) if (a[i].r!=b[i].r || a[i].i!=b[i].i) d++;
      printf("  len=%5d shift=%d: scale diff=%d %s\n", len, shift, d, d?"FAIL":"OK");
      if (d) fail = 1;
    }
    free(a); free(b);
  }
  printf("rvv scale_channel test: %s\n", fail?"FAIL":"PASS");
  return fail;
}
