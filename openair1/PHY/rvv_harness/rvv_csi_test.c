/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * RVV csi_rx.c 2x2 CSI helpers (nr_det_A_MF_2x2 / nr_squared_matrix_element /
 * nr_numer_2x2). Each iterates 3*nb_rb blocks of one __m128i (4 complex / 4
 * int32). Pure integer -> byte-exact.
 *
 *  squared: a_sq[k] = re_k^2 + im_k^2          (madd_epi16(a,a))
 *  det:     det[k]  = |(r00*r11 - i00*i11) - (r01*r10 - i01*i10)|  (int32 wrapping)
 *  numer:   num[k]  = a00_sq[k]+a11_sq[k] + a01_sq[k]+a10_sq[k]    (int32)
 *
 * N = 3*nb_rb*4 complex/elements. LAUNCH: main() switches the core first, then
 * run_tests() (noinline) does the vector work at the post-switch VLEN.
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

/* ---- scalar references (exact SIMDe semantics) ---- */
static void squared_ref(const c16_t *a, int32_t *asq, int N){
  for (int k=0;k<N;k++) asq[k] = (int32_t)a[k].r*a[k].r + (int32_t)a[k].i*a[k].i;
}
static void det_ref(const c16_t *a00,const c16_t *a01,const c16_t *a10,const c16_t *a11,int32_t *det,int N){
  for (int k=0;k<N;k++){
    int32_t ad = (int32_t)((uint32_t)((int32_t)a00[k].r*a11[k].r) - (uint32_t)((int32_t)a00[k].i*a11[k].i));
    int32_t bc = (int32_t)((uint32_t)((int32_t)a01[k].r*a10[k].r) - (uint32_t)((int32_t)a01[k].i*a10[k].i));
    int32_t d  = (int32_t)((uint32_t)ad - (uint32_t)bc);
    det[k] = (d < 0) ? (int32_t)(0u - (uint32_t)d) : d;   /* abs, INT32_MIN stays (matches abs_epi32) */
  }
}
static void numer_ref(const int32_t *a00,const int32_t *a01,const int32_t *a10,const int32_t *a11,int32_t *num,int N){
  for (int k=0;k<N;k++)
    num[k] = (int32_t)((uint32_t)a00[k]+(uint32_t)a11[k]+(uint32_t)a01[k]+(uint32_t)a10[k]);
}

/* ---- RVV kernels (== the csi_rx.c __riscv branches) ---- */
static void squared_rvv(const c16_t *a, int32_t *asq, int N){
  const int16_t *p=(const int16_t*)a;
  for (int n=0;n<N;){
    size_t vl=__riscv_vsetvl_e16mf2(N-n);
    vint16mf2x2_t v=__riscv_vlseg2e16_v_i16mf2x2(p+2*n,vl);
    vint16mf2_t r=__riscv_vget_v_i16mf2x2_i16mf2(v,0), i=__riscv_vget_v_i16mf2x2_i16mf2(v,1);
    __riscv_vse32_v_i32m1(asq+n, __riscv_vwmacc_vv_i32m1(__riscv_vwmul_vv_i32m1(r,r,vl), i,i,vl), vl);
    n+=(int)vl;
  }
}
static void det_rvv(const c16_t *a00,const c16_t *a01,const c16_t *a10,const c16_t *a11,int32_t *det,int N){
  const int16_t *p00=(const int16_t*)a00,*p01=(const int16_t*)a01,*p10=(const int16_t*)a10,*p11=(const int16_t*)a11;
  for (int n=0;n<N;){
    size_t vl=__riscv_vsetvl_e16mf2(N-n); int o=2*n;
    vint16mf2x2_t V00=__riscv_vlseg2e16_v_i16mf2x2(p00+o,vl), V11=__riscv_vlseg2e16_v_i16mf2x2(p11+o,vl);
    vint16mf2x2_t V01=__riscv_vlseg2e16_v_i16mf2x2(p01+o,vl), V10=__riscv_vlseg2e16_v_i16mf2x2(p10+o,vl);
    vint16mf2_t r00=__riscv_vget_v_i16mf2x2_i16mf2(V00,0),i00=__riscv_vget_v_i16mf2x2_i16mf2(V00,1);
    vint16mf2_t r11=__riscv_vget_v_i16mf2x2_i16mf2(V11,0),i11=__riscv_vget_v_i16mf2x2_i16mf2(V11,1);
    vint16mf2_t r01=__riscv_vget_v_i16mf2x2_i16mf2(V01,0),i01=__riscv_vget_v_i16mf2x2_i16mf2(V01,1);
    vint16mf2_t r10=__riscv_vget_v_i16mf2x2_i16mf2(V10,0),i10=__riscv_vget_v_i16mf2x2_i16mf2(V10,1);
    vint32m1_t ad=__riscv_vsub_vv_i32m1(__riscv_vwmul_vv_i32m1(r00,r11,vl),__riscv_vwmul_vv_i32m1(i00,i11,vl),vl);
    vint32m1_t bc=__riscv_vsub_vv_i32m1(__riscv_vwmul_vv_i32m1(r01,r10,vl),__riscv_vwmul_vv_i32m1(i01,i10,vl),vl);
    vint32m1_t d =__riscv_vsub_vv_i32m1(ad,bc,vl);
    __riscv_vse32_v_i32m1(det+n, __riscv_vmax_vv_i32m1(d,__riscv_vneg_v_i32m1(d,vl),vl), vl);  /* abs */
    n+=(int)vl;
  }
}
static void numer_rvv(const int32_t *a00,const int32_t *a01,const int32_t *a10,const int32_t *a11,int32_t *num,int N){
  for (int n=0;n<N;){
    size_t vl=__riscv_vsetvl_e32m1(N-n);
    vint32m1_t s=__riscv_vadd_vv_i32m1(
        __riscv_vadd_vv_i32m1(__riscv_vle32_v_i32m1(a00+n,vl),__riscv_vle32_v_i32m1(a11+n,vl),vl),
        __riscv_vadd_vv_i32m1(__riscv_vle32_v_i32m1(a01+n,vl),__riscv_vle32_v_i32m1(a10+n,vl),vl),vl);
    __riscv_vse32_v_i32m1(num+n, s, vl);
    n+=(int)vl;
  }
}

static uint32_t rng=0xC5117u;
static int16_t r16(void){ rng=rng*1103515245u+12345u; return (int16_t)(rng>>16); }
static int32_t r32(void){ rng=rng*1103515245u+12345u; return (int32_t)rng; }

__attribute__((noinline)) static int run_tests(void){
  int rbs[] = {1, 4, 106, 273};
  int fail=0;
  for (unsigned t=0;t<sizeof(rbs)/sizeof(rbs[0]);t++){
    int N = 3*rbs[t]*4;
    c16_t *a00=malloc(N*4),*a01=malloc(N*4),*a10=malloc(N*4),*a11=malloc(N*4);
    int32_t *s00=malloc(N*4),*s01=malloc(N*4),*s10=malloc(N*4),*s11=malloc(N*4);
    int32_t *rref=malloc(N*4),*rgot=malloc(N*4);
    for (int k=0;k<N;k++){ a00[k]=(c16_t){r16(),r16()}; a01[k]=(c16_t){r16(),r16()}; a10[k]=(c16_t){r16(),r16()}; a11[k]=(c16_t){r16(),r16()};
                           s00[k]=r32(); s01[k]=r32(); s10[k]=r32(); s11[k]=r32(); }
    int d=0;
    squared_ref(a00,rref,N); squared_rvv(a00,rgot,N); for(int k=0;k<N;k++) d+=(rref[k]!=rgot[k]);
    int d1=d; d=0;
    det_ref(a00,a01,a10,a11,rref,N); det_rvv(a00,a01,a10,a11,rgot,N); for(int k=0;k<N;k++) d+=(rref[k]!=rgot[k]);
    int d2=d; d=0;
    numer_ref(s00,s01,s10,s11,rref,N); numer_rvv(s00,s01,s10,s11,rgot,N); for(int k=0;k<N;k++) d+=(rref[k]!=rgot[k]);
    int d3=d;
    printf("  nb_rb=%3d (N=%5d): squared=%d det=%d numer=%d  %s\n", rbs[t], N, d1, d2, d3, (d1||d2||d3)?"FAIL":"OK");
    if (d1||d2||d3) fail=1;
    free(a00);free(a01);free(a10);free(a11);free(s00);free(s01);free(s10);free(s11);free(rref);free(rgot);
  }
  return fail;
}

int main(int argc, char **argv){
  if (argc>1){ int c=atoi(argv[1]); if(c>7) use_ai(); pin_cpu(c); }
  printf("VLEN=%zu\n",(size_t)__riscv_vlenb()*8);
  int fail=run_tests();
  printf("rvv csi 2x2 helpers test: %s\n", fail?"FAIL":"PASS");
  return fail;
}
