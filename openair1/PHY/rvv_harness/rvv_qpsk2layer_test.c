/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * RVV nr_qpsk_llr_2layer (nr_compute_llr.c, USE_128BIT branch): 2-layer MIMO QPSK
 * LLR with cross-layer interference rho01. Per RE:
 *   y0r/2 = ((Re(s0)*23170)>>16)<<1   (= Re(s0)/sqrt2), same for y0i
 *   y1r/2 = Re(s1)>>1,  y1i/2 = Im(s1)>>1
 *   rho_p = ((sat(rhor+rhoi))*23170)>>16,  rho_m = ((sat(rhor-rhoi))*23170)>>16
 *   8 |psi| terms = |sat(rho_{p,m} -/+ y1{r,i}/2)|
 *   8 bit-metrics = sat-sums of two |psi| + y0r/2 +/- y0i/2
 *   L1 = sat(max(num_re_p,num_re_m) - max(den_re_p,den_re_m))   (bit 0)
 *   L2 = sat(max(num_im_p,num_im_m) - max(den_im_p,den_im_m))   (bit 1)
 *   out[2k]=L1, out[2k+1]=L2
 * The x86 separate_real_imag / unpack (deinterleave/interleave) is free on RVV
 * via vlseg2/vsseg2. All int16 saturating; abs = max(x,-x); mulhi = (x*C)>>16.
 *
 * LAUNCH: main() switches core first, run_tests() (noinline) does the vector work.
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <unistd.h>
#include <time.h>
#include <riscv_vector.h>

typedef struct { int16_t r, i; } c16_t;
static int pin_cpu(int c){ cpu_set_t s; CPU_ZERO(&s); CPU_SET(c,&s); return sched_setaffinity(0,sizeof(s),&s); }
static int use_ai(void){ FILE*f=fopen("/proc/set_ai_thread","w"); if(!f)return -1; fprintf(f,"%ld\n",(long)getpid()); return fclose(f); }

static inline int16_t sat16(int32_t v){ return v>32767?32767:(v<-32768?-32768:(int16_t)v); }
static inline int16_t sadd(int16_t a,int16_t b){ return sat16((int32_t)a+b); }
static inline int16_t ssub(int16_t a,int16_t b){ return sat16((int32_t)a-b); }
static inline int16_t sabs(int16_t a){ int16_t n=(int16_t)(-(int32_t)a); return a>n?a:n; }  /* abs_epi16: INT16_MIN stays (wrapping neg + max) */
static inline int16_t smax(int16_t a,int16_t b){ return a>b?a:b; }
static inline int16_t mulhi(int16_t x){ return (int16_t)(((int32_t)x*23170)>>16); }        /* mulhi_epi16(x,23170) */

/* scalar reference == the USE_128BIT nr_qpsk_llr_2layer, per RE */
static void qpsk2_ref(const c16_t *s0, const c16_t *s1, const c16_t *rho, int16_t *out, int n){
  for (int k=0;k<n;k++){
    int16_t y0r=(int16_t)(mulhi(s0[k].r)<<1), y0i=(int16_t)(mulhi(s0[k].i)<<1);
    int16_t y1r=(int16_t)(s1[k].r>>1), y1i=(int16_t)(s1[k].i>>1);
    int16_t rho_p=mulhi(sadd(rho[k].r,rho[k].i)), rho_m=mulhi(ssub(rho[k].r,rho[k].i));
    int16_t a_rpm=sabs(ssub(rho_p,y1r)), a_imm=sabs(ssub(rho_m,y1i));
    int16_t a_rmm=sabs(ssub(rho_m,y1r)), a_ipm=sabs(ssub(rho_p,y1i));
    int16_t a_rpp=sabs(sadd(rho_p,y1r)), a_imp=sabs(sadd(rho_m,y1i));
    int16_t a_rmp=sabs(sadd(rho_m,y1r)), a_ipp=sabs(sadd(rho_p,y1i));
    int16_t num_re_p=sadd(sadd(sadd(a_rpm,a_imm),y0r),y0i);
    int16_t num_re_m=ssub(sadd(sadd(a_rmm,a_ipp),y0r),y0i);
    int16_t den_re_p=sadd(ssub(sadd(a_rmp,a_ipm),y0r),y0i);
    int16_t den_re_m=ssub(ssub(sadd(a_rpp,a_imp),y0r),y0i);
    int16_t num_im_p=sadd(sadd(sadd(a_rpm,a_imm),y0r),y0i);
    int16_t num_im_m=sadd(ssub(sadd(a_rmp,a_ipm),y0r),y0i);
    int16_t den_im_p=ssub(sadd(sadd(a_rmm,a_ipp),y0r),y0i);
    int16_t den_im_m=ssub(ssub(sadd(a_rpp,a_imp),y0r),y0i);
    out[2*k]  = ssub(smax(num_re_p,num_re_m), smax(den_re_p,den_re_m));
    out[2*k+1]= ssub(smax(num_im_p,num_im_m), smax(den_im_p,den_im_m));
  }
}

/* ---- RVV kernel (VLA; == the intended nr_compute_llr.c __riscv branch) ---- */
#define VADD(a,b) __riscv_vsadd_vv_i16m1((a),(b),vl)
#define VSUB(a,b) __riscv_vssub_vv_i16m1((a),(b),vl)
#define VMAX(a,b) __riscv_vmax_vv_i16m1((a),(b),vl)
#define VABS(a)   __riscv_vmax_vv_i16m1((a), __riscv_vneg_v_i16m1((a),vl), vl)  /* |x| = max(x,-x); wrapping neg matches abs_epi16(INT16_MIN)=INT16_MIN */
static inline vint16m1_t vmulhi(vint16m1_t x, size_t vl){
  return __riscv_vnsra_wx_i16m1(__riscv_vwmul_vx_i32m2(x, 23170, vl), 16, vl);
}
static void qpsk2_rvv(const c16_t *s0, const c16_t *s1, const c16_t *rho, int16_t *out, int n){
  const int16_t *p0=(const int16_t*)s0,*p1=(const int16_t*)s1,*pr=(const int16_t*)rho;
  for (int k=0;k<n;){
    size_t vl=__riscv_vsetvl_e16m1(n-k); int o=2*k;
    vint16m1x2_t S0=__riscv_vlseg2e16_v_i16m1x2(p0+o,vl), S1=__riscv_vlseg2e16_v_i16m1x2(p1+o,vl), R=__riscv_vlseg2e16_v_i16m1x2(pr+o,vl);
    vint16m1_t s0r=__riscv_vget_v_i16m1x2_i16m1(S0,0), s0i=__riscv_vget_v_i16m1x2_i16m1(S0,1);
    vint16m1_t s1r=__riscv_vget_v_i16m1x2_i16m1(S1,0), s1i=__riscv_vget_v_i16m1x2_i16m1(S1,1);
    vint16m1_t rr=__riscv_vget_v_i16m1x2_i16m1(R,0), ri=__riscv_vget_v_i16m1x2_i16m1(R,1);
    vint16m1_t y0r=__riscv_vsll_vx_i16m1(vmulhi(s0r,vl),1,vl), y0i=__riscv_vsll_vx_i16m1(vmulhi(s0i,vl),1,vl);
    vint16m1_t y1r=__riscv_vsra_vx_i16m1(s1r,1,vl), y1i=__riscv_vsra_vx_i16m1(s1i,1,vl);
    vint16m1_t rho_p=vmulhi(VADD(rr,ri),vl), rho_m=vmulhi(VSUB(rr,ri),vl);
    vint16m1_t a_rpm=VABS(VSUB(rho_p,y1r)), a_imm=VABS(VSUB(rho_m,y1i));
    vint16m1_t a_rmm=VABS(VSUB(rho_m,y1r)), a_ipm=VABS(VSUB(rho_p,y1i));
    vint16m1_t a_rpp=VABS(VADD(rho_p,y1r)), a_imp=VABS(VADD(rho_m,y1i));
    vint16m1_t a_rmp=VABS(VADD(rho_m,y1r)), a_ipp=VABS(VADD(rho_p,y1i));
    vint16m1_t num_re_p=VADD(VADD(VADD(a_rpm,a_imm),y0r),y0i);
    vint16m1_t num_re_m=VSUB(VADD(VADD(a_rmm,a_ipp),y0r),y0i);
    vint16m1_t den_re_p=VADD(VSUB(VADD(a_rmp,a_ipm),y0r),y0i);
    vint16m1_t den_re_m=VSUB(VSUB(VADD(a_rpp,a_imp),y0r),y0i);
    vint16m1_t num_im_p=VADD(VADD(VADD(a_rpm,a_imm),y0r),y0i);
    vint16m1_t num_im_m=VADD(VSUB(VADD(a_rmp,a_ipm),y0r),y0i);
    vint16m1_t den_im_p=VSUB(VADD(VADD(a_rmm,a_ipp),y0r),y0i);
    vint16m1_t den_im_m=VSUB(VSUB(VADD(a_rpp,a_imp),y0r),y0i);
    vint16m1_t L1=VSUB(VMAX(num_re_p,num_re_m),VMAX(den_re_p,den_re_m));
    vint16m1_t L2=VSUB(VMAX(num_im_p,num_im_m),VMAX(den_im_p,den_im_m));
    __riscv_vsseg2e16_v_i16m1x2(out+o, __riscv_vcreate_v_i16m1x2(L1,L2), vl);
    k+=(int)vl;
  }
}

static uint32_t rng=0x2c0ffee;
static int16_t r16(void){ rng=rng*1103515245u+12345u; return (int16_t)(rng>>16); }

__attribute__((noinline)) static int run_tests(void){
  int lens[]={4,8,12,100,3276};
  int fail=0;
  for (unsigned t=0;t<sizeof(lens)/sizeof(lens[0]);t++){
    int n=lens[t];
    c16_t *s0=malloc(n*4),*s1=malloc(n*4),*rho=malloc(n*4);
    int16_t *rref=malloc(n*2*2),*rgot=malloc(n*2*2);
    for(int k=0;k<n;k++){ s0[k]=(c16_t){r16(),r16()}; s1[k]=(c16_t){r16(),r16()}; rho[k]=(c16_t){r16(),r16()}; }
    qpsk2_ref(s0,s1,rho,rref,n); qpsk2_rvv(s0,s1,rho,rgot,n);
    int d=0; for(int k=0;k<2*n;k++) if(rref[k]!=rgot[k]) d++;
    double us=0;
    if(n>=3276){ int R=4000; struct timespec a,b; clock_gettime(CLOCK_MONOTONIC,&a);
      for(int r=0;r<R;r++) qpsk2_rvv(s0,s1,rho,rgot,n);
      clock_gettime(CLOCK_MONOTONIC,&b); us=((b.tv_sec-a.tv_sec)*1e6+(b.tv_nsec-a.tv_nsec)/1e3)/R; }
    printf("  len=%5d: diff=%d %s  t_rvv=%.2fus\n", n, d, d?"FAIL":"OK", us);
    if(d) fail=1;
    free(s0);free(s1);free(rho);free(rref);free(rgot);
  }
  return fail;
}
int main(int argc,char**argv){
  if(argc>1){int c=atoi(argv[1]); if(c>7) use_ai(); pin_cpu(c);}
  printf("VLEN=%zu\n",(size_t)__riscv_vlenb()*8);
  int fail=run_tests();
  printf("rvv qpsk_llr_2layer test: %s\n", fail?"FAIL":"PASS");
  return fail;
}
