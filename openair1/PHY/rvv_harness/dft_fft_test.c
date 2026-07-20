/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * RVV port harness -- VLA iterative radix-4 Stockham IDFT (DIF autosort).
 *
 * Goal: one VL-agnostic RVV kernel for any VLEN (256b little cores, 1024b A100),
 * no fixed-width leaf, no partial-vl store. Replaces the recursive idft chain.
 *
 * Structure (verified vs naive DFT to ~1e-13 in double, see scratchpad):
 *   DIF radix-4 Stockham, ping-pong buffers, passes L = N, N/4, ..., 4.
 *   per pass: Lq=L/4, r=N/L, Nq=N/4; for butterfly bf=0..Nq-1 (k=bf/Lq, j=bf%Lq):
 *     read  c_leg = a[(k*L+j) + leg*Lq]        (leg 0..3)
 *     radix-4 combine (inverse, +j):
 *       d0=c0+c1+c2+c3; d1=c0+j c1-c2-j c3; d2=c0-c1+c2-c3; d3=c0-j c1-c2+j c3
 *     twiddle (DIF: AFTER combine), then >>1 scale, write:
 *       b[bf+0*Nq]=d0>>1;  b[bf+m*Nq]=(d_m * W_L^{m j})>>15 >>1   (m=1,2,3)
 *   Output (after all passes) is natural order. >>1 per pass over log4(N) passes
 *   gives 1/sqrt(N) (>>6 for N=4096), matching OAI idft(scale=1) magnitude.
 *
 * Why this is VLA-safe: the WRITES are b[bf + band*Nq], bf=0..Nq-1 -- unit-stride,
 * and Nq=N/4 is a multiple of any power-of-2 VLMAX, so every vsseg2e16 store is
 * FULL width (no partial-vl -> no spacemit-gcc -O2 over-store). READS are gathers
 * (loads; correct at any vl, no store-bug). Vectorize across bf; vl=VLMAX except
 * possibly a tail (Nq%VLMAX==0 for our sizes, so no tail either).
 *
 * Validated: scalar-fixedpoint vs double (precision/SNR); RVV vs scalar-fixedpoint
 * BYTE-EXACT on VLEN=256 and 1024.
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sched.h>
#include <unistd.h>
#if defined(__riscv) && defined(__riscv_vector)
#include <riscv_vector.h>
#endif

typedef struct { int16_t r, i; } c16;

static int use_ai_core(void){ FILE*f=fopen("/proc/set_ai_thread","w"); if(!f)return -1; int rc=fprintf(f,"%ld\n",(long)getpid()); return (rc<0||fclose(f)!=0)?-1:0; }
static int pin_cpu(int c){ cpu_set_t s; CPU_ZERO(&s); CPU_SET(c,&s); return sched_setaffinity(0,sizeof(s),&s); }

static int16_t sat16(int32_t v){ return v>32767?32767:(v<-32768?-32768:(int16_t)v); }
static int16_t sadd(int16_t a,int16_t b){ return sat16((int32_t)a+b); }
static int16_t ssub(int16_t a,int16_t b){ return sat16((int32_t)a-b); }

/* ---- twiddle / index tables (per pass), shared by scalar and rvv ---- */
#define MAXPASS 8
static int g_L[MAXPASS], g_np;
static c16 *g_tw[MAXPASS][3];      /* tw[pass][m-1][bf], m=1..3, Q15 (multiply, +j sign) */
static uint32_t *g_baseByte[MAXPASS]; /* byte offset of complex (k*L+j) for gather */

static void build_tables(int N){
  int Nq=N/4; g_np=0;
  for(int L=N; L>=4; L/=4){
    int p=g_np++; g_L[p]=L; int Lq=L/4;
    g_baseByte[p]=malloc(sizeof(uint32_t)*Nq);
    for(int m=0;m<3;m++) g_tw[p][m]=malloc(sizeof(c16)*Nq);
    for(int bf=0;bf<Nq;bf++){
      int k=bf/Lq, j=bf%Lq;
      g_baseByte[p][bf]=(uint32_t)(4*(k*L+j)); /* 4 bytes per complex */
      for(int m=1;m<=3;m++){
        double ang=2.0*M_PI*(double)(m*j)/(double)L; /* +j inverse */
        double wr=cos(ang), wi=sin(ang);
        g_tw[p][m-1][bf].r=sat16((int32_t)lround(wr*32767.0));
        g_tw[p][m-1][bf].i=sat16((int32_t)lround(wi*32767.0));
      }
    }
  }
}
static void free_tables(void){ for(int p=0;p<g_np;p++){ free(g_baseByte[p]); for(int m=0;m<3;m++) free(g_tw[p][m]); } }

/* complex multiply d*w >>15 (Q15 w), saturating -- matches vnclip RDN semantics */
static c16 cmul15(c16 d, c16 w){
  int32_t re=((int32_t)d.r*w.r - (int32_t)d.i*w.i)>>15;
  int32_t im=((int32_t)d.r*w.i + (int32_t)d.i*w.r)>>15;
  return (c16){ sat16(re), sat16(im) };
}

/* ---- scalar fixed-point reference (DIF radix-4 Stockham idft) ---- */
static void idft_scalar(const c16 *in, c16 *out, int N){
  int Nq=N/4;
  c16 *a=malloc(sizeof(c16)*N), *b=malloc(sizeof(c16)*N);
  memcpy(a,in,sizeof(c16)*N);
  for(int p=0;p<g_np;p++){
    int L=g_L[p], Lq=L/4, r=N/L;
    for(int k=0;k<r;k++) for(int j=0;j<Lq;j++){
      int bf=k*Lq+j, base=k*L+j;
      c16 c0=a[base], c1=a[base+Lq], c2=a[base+2*Lq], c3=a[base+3*Lq];
      /* radix-4 combine, +j: j*c = (-c.i, c.r) */
      c16 d0={ sadd(sadd(c0.r,c1.r),sadd(c2.r,c3.r)), sadd(sadd(c0.i,c1.i),sadd(c2.i,c3.i)) };
      c16 d2={ ssub(sadd(c0.r,c2.r),sadd(c1.r,c3.r)), ssub(sadd(c0.i,c2.i),sadd(c1.i,c3.i)) };
      /* d1 = c0 + j c1 - c2 - j c3 ; j c1=(-c1.i,c1.r), -j c3=(c3.i,-c3.r) */
      c16 d1={ ssub(sadd(c0.r,c3.i),sadd(c1.i,c2.r)), sadd(ssub(c0.i,c3.r),ssub(c1.r,c2.i)) };
      /* d3 = c0 - j c1 - c2 + j c3 ; -j c1=(c1.i,-c1.r), +j c3=(-c3.i,c3.r) */
      c16 d3={ ssub(sadd(c0.r,c1.i),sadd(c3.i,c2.r)), sadd(ssub(c0.i,c1.r),ssub(c3.r,c2.i)) };
      c16 t1=cmul15(d1,g_tw[p][0][bf]), t2=cmul15(d2,g_tw[p][1][bf]), t3=cmul15(d3,g_tw[p][2][bf]);
      b[bf]       =(c16){ (int16_t)(d0.r>>1),(int16_t)(d0.i>>1) };
      b[bf+Nq]    =(c16){ (int16_t)(t1.r>>1),(int16_t)(t1.i>>1) };
      b[bf+2*Nq]  =(c16){ (int16_t)(t2.r>>1),(int16_t)(t2.i>>1) };
      b[bf+3*Nq]  =(c16){ (int16_t)(t3.r>>1),(int16_t)(t3.i>>1) };
    }
    c16*t=a;a=b;b=t;
  }
  memcpy(out,a,sizeof(c16)*N);
  free(a);free(b);
}

/* ---- double reference for precision (same DIF, float) ---- */
static void idft_double(const c16*in,double*outr,double*outi,int N){
  int Nq=N/4; double *ar=malloc(8*N),*ai=malloc(8*N),*br=malloc(8*N),*bi=malloc(8*N);
  for(int i=0;i<N;i++){ar[i]=in[i].r;ai[i]=in[i].i;}
  for(int p=0;p<g_np;p++){int L=g_L[p],Lq=L/4,r=N/L;
    for(int k=0;k<r;k++)for(int j=0;j<Lq;j++){int bf=k*Lq+j,base=k*L+j;
      double c0r=ar[base],c0i=ai[base],c1r=ar[base+Lq],c1i=ai[base+Lq],c2r=ar[base+2*Lq],c2i=ai[base+2*Lq],c3r=ar[base+3*Lq],c3i=ai[base+3*Lq];
      double d0r=c0r+c1r+c2r+c3r,d0i=c0i+c1i+c2i+c3i;
      double d2r=c0r-c1r+c2r-c3r,d2i=c0i-c1i+c2i-c3i;
      double d1r=c0r-c1i-c2r+c3i,d1i=c0i+c1r-c2i-c3r;
      double d3r=c0r+c1i-c2r-c3i,d3i=c0i-c1r-c2i+c3r;
      double a1=2.0*M_PI*(double)j/L,a2=2*a1,a3=3*a1;
      br[bf]=d0r*0.5; bi[bf]=d0i*0.5;
      br[bf+Nq]=(d1r*cos(a1)-d1i*sin(a1))*0.5; bi[bf+Nq]=(d1r*sin(a1)+d1i*cos(a1))*0.5;
      br[bf+2*Nq]=(d2r*cos(a2)-d2i*sin(a2))*0.5; bi[bf+2*Nq]=(d2r*sin(a2)+d2i*cos(a2))*0.5;
      br[bf+3*Nq]=(d3r*cos(a3)-d3i*sin(a3))*0.5; bi[bf+3*Nq]=(d3r*sin(a3)+d3i*cos(a3))*0.5;
    }
    double*t;t=ar;ar=br;br=t;t=ai;ai=bi;bi=t;
  }
  for(int i=0;i<N;i++){outr[i]=ar[i];outi[i]=ai[i];}
  free(ar);free(ai);free(br);free(bi);
}

#if defined(__riscv) && defined(__riscv_vector)
/* ---- RVV: vectorize across bf; gather reads, full-VL unit-stride writes ---- */
static void idft_rvv(const c16 *in, c16 *out, int N){
  int Nq=N/4;
  c16 *a=malloc(sizeof(c16)*N), *b=malloc(sizeof(c16)*N);
  memcpy(a,in,sizeof(c16)*N);
  for(int p=0;p<g_np;p++){
    int L=g_L[p], Lq=L/4;
    const uint32_t *bb=g_baseByte[p];
    const int16_t *a0=(const int16_t*)a, *a1p=(const int16_t*)(a+Lq), *a2p=(const int16_t*)(a+2*Lq), *a3p=(const int16_t*)(a+3*Lq);
    int16_t *bo=(int16_t*)b;
    (void)a0;(void)a1p;(void)a2p;(void)a3p;
    for(int bf=0;bf<Nq;){
      size_t vl=__riscv_vsetvl_e32m1(Nq-bf);              /* vl complex; e16mf2 shares this vl */
      vuint32m1_t idx=__riscv_vle32_v_u32m1(bb+bf, vl);   /* byte offset of complex base (k*L+j) */
      /* gather each input complex as one u32, then split lo=re / hi=im (mf2) */
      #define GLEG(LEG,RE,IM) \
        vuint32m1_t pk##LEG=__riscv_vluxei32_v_u32m1((const uint32_t*)a + (LEG)*Lq, idx, vl); \
        vint16mf2_t RE=__riscv_vreinterpret_v_u16mf2_i16mf2(__riscv_vnsrl_wx_u16mf2(pk##LEG,0,vl)); \
        vint16mf2_t IM=__riscv_vreinterpret_v_u16mf2_i16mf2(__riscv_vnsrl_wx_u16mf2(pk##LEG,16,vl))
      GLEG(0,c0r,c0i); GLEG(1,c1r,c1i); GLEG(2,c2r,c2i); GLEG(3,c3r,c3i);
      #undef GLEG
      /* radix-4 combine (saturating int16, mf2) */
      vint16mf2_t d0r=__riscv_vsadd_vv_i16mf2(__riscv_vsadd_vv_i16mf2(c0r,c1r,vl),__riscv_vsadd_vv_i16mf2(c2r,c3r,vl),vl);
      vint16mf2_t d0i=__riscv_vsadd_vv_i16mf2(__riscv_vsadd_vv_i16mf2(c0i,c1i,vl),__riscv_vsadd_vv_i16mf2(c2i,c3i,vl),vl);
      vint16mf2_t d2r=__riscv_vssub_vv_i16mf2(__riscv_vsadd_vv_i16mf2(c0r,c2r,vl),__riscv_vsadd_vv_i16mf2(c1r,c3r,vl),vl);
      vint16mf2_t d2i=__riscv_vssub_vv_i16mf2(__riscv_vsadd_vv_i16mf2(c0i,c2i,vl),__riscv_vsadd_vv_i16mf2(c1i,c3i,vl),vl);
      vint16mf2_t d1r=__riscv_vssub_vv_i16mf2(__riscv_vsadd_vv_i16mf2(c0r,c3i,vl),__riscv_vsadd_vv_i16mf2(c1i,c2r,vl),vl);
      vint16mf2_t d1i=__riscv_vsadd_vv_i16mf2(__riscv_vssub_vv_i16mf2(c0i,c3r,vl),__riscv_vssub_vv_i16mf2(c1r,c2i,vl),vl);
      vint16mf2_t d3r=__riscv_vssub_vv_i16mf2(__riscv_vsadd_vv_i16mf2(c0r,c1i,vl),__riscv_vsadd_vv_i16mf2(c3i,c2r,vl),vl);
      vint16mf2_t d3i=__riscv_vsadd_vv_i16mf2(__riscv_vssub_vv_i16mf2(c0i,c1r,vl),__riscv_vssub_vv_i16mf2(c3r,c2i,vl),vl);
      /* band0: d0>>1 (full-VL unit-stride store) */
      __riscv_vsseg2e16_v_i16mf2x2(bo+2*bf, __riscv_vcreate_v_i16mf2x2(
          __riscv_vsra_vx_i16mf2(d0r,1,vl), __riscv_vsra_vx_i16mf2(d0i,1,vl)), vl);
      /* bands 1..3: (d*w)>>15 then >>1 ; tw unit-stride deinterleaved */
      #define BAND(M,DR,DI,OFF) do { \
        vint16mf2x2_t W=__riscv_vlseg2e16_v_i16mf2x2((const int16_t*)g_tw[p][M]+2*bf, vl); \
        vint16mf2_t wr=__riscv_vget_v_i16mf2x2_i16mf2(W,0), wi=__riscv_vget_v_i16mf2x2_i16mf2(W,1); \
        vint32m1_t re=__riscv_vsub_vv_i32m1(__riscv_vwmul_vv_i32m1(DR,wr,vl),__riscv_vwmul_vv_i32m1(DI,wi,vl),vl); \
        vint32m1_t im=__riscv_vadd_vv_i32m1(__riscv_vwmul_vv_i32m1(DR,wi,vl),__riscv_vwmul_vv_i32m1(DI,wr,vl),vl); \
        vint16mf2_t rr=__riscv_vnclip_wx_i16mf2(re,15,__RISCV_VXRM_RDN,vl); \
        vint16mf2_t ii=__riscv_vnclip_wx_i16mf2(im,15,__RISCV_VXRM_RDN,vl); \
        __riscv_vsseg2e16_v_i16mf2x2(bo+2*(bf+(OFF)*Nq), __riscv_vcreate_v_i16mf2x2( \
            __riscv_vsra_vx_i16mf2(rr,1,vl), __riscv_vsra_vx_i16mf2(ii,1,vl)), vl); \
      } while(0)
      BAND(0,d1r,d1i,1); BAND(1,d2r,d2i,2); BAND(2,d3r,d3i,3);
      #undef BAND
      bf+=(int)vl;
    }
    c16*t=a;a=b;b=t;
  }
  memcpy(out,a,sizeof(c16)*N);
  free(a);free(b);
}
#endif

static uint32_t rng=12345;
static int16_t rnd(void){ rng=rng*1103515245u+12345u; return (int16_t)((rng>>17)-16384); } /* +-16k */

int main(int argc,char**argv){
  int cpu=(argc>1)?atoi(argv[1]):-1;
  if(cpu>=0){ if(cpu>7) use_ai_core(); if(pin_cpu(cpu)!=0) fprintf(stderr,"warn pin\n"); }
  int Ns[]={64,256,1024,4096};
  for(unsigned s=0;s<sizeof(Ns)/sizeof(int);s++){
    int N=Ns[s]; build_tables(N);
    c16 *x=malloc(sizeof(c16)*N),*ys=malloc(sizeof(c16)*N);
    for(int i=0;i<N;i++){x[i].r=rnd()/2;x[i].i=rnd()/2;} /* moderate input to limit saturation */
    idft_scalar(x,ys,N);
    /* precision vs double */
    double *dr=malloc(8*N),*di=malloc(8*N); idft_double(x,dr,di,N);
    double num=0,den=0; for(int i=0;i<N;i++){ double er=ys[i].r-dr[i],ei=ys[i].i-di[i]; num+=er*er+ei*ei; den+=dr[i]*dr[i]+di[i]*di[i]; }
    double snr=10*log10(den/(num+1e-9));
    printf("N=%5d  scalar-vs-double SNR=%.1f dB",N,snr);
#if defined(__riscv) && defined(__riscv_vector)
    c16 *yr=malloc(sizeof(c16)*N); idft_rvv(x,yr,N);
    int mm=memcmp(ys,yr,sizeof(c16)*N);
    printf("   rvv-vs-scalar: %s", mm?"FAIL":"BYTE-EXACT");
    free(yr);
#endif
    printf("\n");
    free(x);free(ys);free(dr);free(di); free_tables();
  }
  return 0;
}
