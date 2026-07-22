/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * 4-step (Bailey) FFT for the RISC-V DFT: full-vl UNIT-STRIDE replacement for
 * the gather-bound Stockham. N = N1*N2 (pow-4 factors so sub-FFTs are radix-4).
 *
 *   n = n1*N2 + n2 (row-major [N1][N2], n2 contiguous), sign s (+1 idft, -1 dft)
 *   A[k1][n2] = size-N1 DFT along n1 (rows), batched over n2 (contiguous)   [unit-stride]
 *   A[k1][n2] *= W_N^{s*k1*n2}                                              [twiddle]
 *   transpose A[N1][N2] -> At[N2][N1]
 *   C[k2][k1] = size-N2 DFT along n2 (rows of At), batched over k1          [unit-stride]
 *   out[k2*N1+k1] = C[k2][k1]  (natural order)
 *
 * This file first proves the STRUCTURE in double vs a naive O(N^2) DFT (naive
 * sub-DFTs), then the radix-4 sub-FFT, then fixed-point + RVV. Stage 1 (double)
 * is host-testable (no RVV).
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sched.h>
#include <unistd.h>

typedef struct { double r, i; } cd;

static int pin_cpu(int c){ cpu_set_t s; CPU_ZERO(&s); CPU_SET(c,&s); return sched_setaffinity(0,sizeof(s),&s); }
static int use_ai(void){ FILE*f=fopen("/proc/set_ai_thread","w"); if(!f)return -1; fprintf(f,"%ld\n",(long)getpid()); return fclose(f); }
static double now_us(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec*1e6 + t.tv_nsec/1e3; }

static void dft_naive(const cd *in, cd *out, int N, int s){
  for (int k = 0; k < N; k++){
    double sr = 0, si = 0;
    for (int n = 0; n < N; n++){
      double a = s * 2.0 * M_PI * (double)n * (double)k / (double)N;
      double c = cos(a), sn = sin(a);
      sr += in[n].r * c - in[n].i * sn;
      si += in[n].r * sn + in[n].i * c;
    }
    out[k].r = sr; out[k].i = si;
  }
}

/* naive size-M DFT along the M (row) dimension of a [M][B] array, all B cols */
static void col_dft_naive(cd *d, int M, int B, int s){
  cd *col = malloc(sizeof(cd) * M), *tmp = malloc(sizeof(cd) * M);
  for (int c = 0; c < B; c++){
    for (int r = 0; r < M; r++) col[r] = d[r * B + c];
    dft_naive(col, tmp, M, s);
    for (int r = 0; r < M; r++) d[r * B + c] = tmp[r];
  }
  free(col); free(tmp);
}

/* double 4-step with naive sub-DFTs -- validates reshape/twiddle/transpose/order */
static void fourstep_double(const cd *in, cd *out, int N, int N1, int N2, int s){
  cd *A = malloc(sizeof(cd) * N), *At = malloc(sizeof(cd) * N);
  memcpy(A, in, sizeof(cd) * N);                 /* [N1][N2] */
  col_dft_naive(A, N1, N2, s);                   /* DFT along n1 */
  for (int k1 = 0; k1 < N1; k1++)                /* twiddle W_N^{s*k1*n2} */
    for (int n2 = 0; n2 < N2; n2++){
      double a = s * 2.0 * M_PI * (double)k1 * (double)n2 / (double)N;
      double c = cos(a), sn = sin(a);
      cd v = A[k1 * N2 + n2];
      A[k1 * N2 + n2].r = v.r * c - v.i * sn;
      A[k1 * N2 + n2].i = v.r * sn + v.i * c;
    }
  for (int k1 = 0; k1 < N1; k1++)                /* transpose -> [N2][N1] */
    for (int n2 = 0; n2 < N2; n2++)
      At[n2 * N1 + k1] = A[k1 * N2 + n2];
  col_dft_naive(At, N2, N1, s);                  /* DFT along n2 */
  memcpy(out, At, sizeof(cd) * N);               /* out[k2*N1+k1] = At[k2][k1] */
  free(A); free(At);
}

/* ================= fixed-point (int16 Q15) 4-step -- the RVV oracle ================= */
typedef struct { int16_t r, i; } c16;
static int16_t sat16(int32_t v){ return v>32767?32767:(v<-32768?-32768:(int16_t)v); }
static int16_t sadd(int16_t a,int16_t b){ return sat16((int32_t)a+b); }
static int16_t ssub(int16_t a,int16_t b){ return sat16((int32_t)a-b); }
/* --- improved scaling: fold the per-stage radix-4 1/2 gain into a single
 * round-to-nearest (RNU) at >>16 (keeps the twiddle at full Q15 precision),
 * and round (not floor) the DC halving. Matches vnclip RNU / vssra RNU. --- */
static int16_t rsra1(int16_t x){ return (int16_t)(((int32_t)x + 1) >> 1); }   /* round(x/2), ties up */
/* d * w * 1/2, round-to-nearest: ((d*w) + 2^15) >> 16 */
static c16 cmul16(c16 d, c16 w){
  int32_t re=(((int32_t)d.r*w.r - (int32_t)d.i*w.i) + 32768)>>16;
  int32_t im=(((int32_t)d.r*w.i + (int32_t)d.i*w.r) + 32768)>>16;
  return (c16){ sat16(re), sat16(im) };
}
/* d * w (unit twiddle), round-to-nearest >>15 (no scaling) -- step-B / radix-2 */
static c16 cmul15n(c16 d, c16 w){
  int32_t re=(((int32_t)d.r*w.r - (int32_t)d.i*w.i) + 16384)>>15;
  int32_t im=(((int32_t)d.r*w.i + (int32_t)d.i*w.r) + 16384)>>15;
  return (c16){ sat16(re), sat16(im) };
}

/* sub-FFT twiddles: per size M, per stage (log4 M), per bf (M/4), 3 complex Q15.
 * Step-B twiddles: [N1][N2]. Built once for the tested N. sign folded in. */
#define MAXST 3
static c16 g_subtw[2][MAXST][64/4][3];  /* [which sub (0=N1,1=N2)][stage][bf][m-1] */
static int g_subnp[2];
static c16 *g_btw;                       /* [N1*N2] step-B twiddles */
static int g_N,g_N1,g_N2,g_s;

static void build_subtw(int which, int M, int s){
  int np=0;
  for (int L=M; L>=4; L/=4){
    int Lq=L/4, st=np++;
    for (int bf=0; bf<M/4; bf++){
      int j=bf%Lq;
      for (int m=1;m<=3;m++){
        double a=s*2.0*M_PI*(double)(m*j)/(double)L;
        g_subtw[which][st][bf][m-1]=(c16){ sat16((int32_t)lround(cos(a)*32767.0)), sat16((int32_t)lround(sin(a)*32767.0)) };
      }
    }
  }
  g_subnp[which]=np;
}
static void build_tables_fx(int N,int N1,int N2,int s){
  g_N=N; g_N1=N1; g_N2=N2; g_s=s;
  build_subtw(0,N1,s); build_subtw(1,N2,s);
  g_btw=malloc(sizeof(c16)*N);
  for (int k1=0;k1<N1;k1++) for(int n2=0;n2<N2;n2++){
    double a=s*2.0*M_PI*(double)k1*(double)n2/(double)N;
    g_btw[k1*N2+n2]=(c16){ sat16((int32_t)lround(cos(a)*32767.0)), sat16((int32_t)lround(sin(a)*32767.0)) };
  }
}
/* batched DIF radix-4 Stockham along M rows of [M][B] (B contiguous), sign s via subtw.
 * a,b are ping-pong [M*B]. Result left in a. */
static void subfft_fx(c16 *a, c16 *b, int M, int B, int which, int s){
  int np=g_subnp[which];
  for (int p=0, L=M; p<np; p++, L/=4){
    int Lq=L/4;
    for (int bf=0; bf<M/4; bf++){
      int k=bf/Lq, j=bf%Lq, r0=k*L+j, r1=r0+Lq, r2=r0+2*Lq, r3=r0+3*Lq;
      c16 w1=g_subtw[which][p][bf][0], w2=g_subtw[which][p][bf][1], w3=g_subtw[which][p][bf][2];
      for (int c=0;c<B;c++){
        c16 x0=a[r0*B+c], x1=a[r1*B+c], x2=a[r2*B+c], x3=a[r3*B+c];
        /* radix-4 combine, sign s: +j*c=(-c.i,c.r)*s-aware. Use s in the flip. */
        c16 d0={ sadd(sadd(x0.r,x1.r),sadd(x2.r,x3.r)), sadd(sadd(x0.i,x1.i),sadd(x2.i,x3.i)) };
        c16 d2={ ssub(sadd(x0.r,x2.r),sadd(x1.r,x3.r)), ssub(sadd(x0.i,x2.i),sadd(x1.i,x3.i)) };
        /* jf(c) = s>0 ? (-c.i,c.r) : (c.i,-c.r) */
        c16 jx1 = s>0 ? (c16){(int16_t)-x1.i,x1.r} : (c16){x1.i,(int16_t)-x1.r};
        c16 jx3 = s>0 ? (c16){(int16_t)-x3.i,x3.r} : (c16){x3.i,(int16_t)-x3.r};
        c16 e02r={ ssub(x0.r,x2.r), ssub(x0.i,x2.i) };
        c16 d1={ sadd(e02r.r, ssub(jx1.r,jx3.r)), sadd(e02r.i, ssub(jx1.i,jx3.i)) };
        c16 d3={ ssub(e02r.r, ssub(jx1.r,jx3.r)), ssub(e02r.i, ssub(jx1.i,jx3.i)) };
        b[(bf)*B+c]        =(c16){rsra1(d0.r),rsra1(d0.i)};
        b[(bf+M/4)*B+c]    =cmul16(d1,w1);
        b[(bf+2*(M/4))*B+c]=cmul16(d2,w2);
        b[(bf+3*(M/4))*B+c]=cmul16(d3,w3);
      }
    }
    c16 *t=a; a=b; b=t;
  }
  /* np even (M pow-4: np=log4 M): result ends in the buffer 'a' entered with iff np even.
     Caller passes buffers so final is in the first arg -> copy if needed handled by caller. */
}
static void fourstep_fx(const c16 *in, c16 *out, int N, int N1, int N2){
  c16 *A=malloc(sizeof(c16)*N), *B=malloc(sizeof(c16)*N), *At=malloc(sizeof(c16)*N);
  memcpy(A,in,sizeof(c16)*N);
  subfft_fx(A,B,N1,N2,0,g_s);                 /* DFT along n1; result in A if np even */
  if (g_subnp[0]&1){ c16*t=A;A=B;B=t; }        /* if odd passes, result is in B */
  for (int k1=0;k1<N1;k1++) for(int n2=0;n2<N2;n2++)
    A[k1*N2+n2]=cmul15n(A[k1*N2+n2], g_btw[k1*N2+n2]);
  for (int k1=0;k1<N1;k1++) for(int n2=0;n2<N2;n2++) At[n2*N1+k1]=A[k1*N2+n2];  /* transpose */
  subfft_fx(At,B,N2,N1,1,g_s);
  if (g_subnp[1]&1){ memcpy(out,B,sizeof(c16)*N); } else memcpy(out,At,sizeof(c16)*N);
  free(A);free(B);free(At);
}

/* ================= radix-2 wrapper (N = 2*M, M pow-4) =================
 * One radix-2 DIF stage over the full N wrapping the validated 4-step on M=N/2:
 *   a[n] = (x[n]+x[n+M]) * (1/sqrt2)                 -> even outputs = DFT_M(a)
 *   b[n] = (x[n]-x[n+M]) * W_N^{s*n} * (1/sqrt2)     -> odd  outputs = DFT_M(b)
 *   out[2r]=A[r], out[2r+1]=B[r]   (A=fourstep(a), B=fourstep(b), each *1/sqrt(M))
 * so total scale = 1/sqrt2 * 1/sqrt(M) = 1/sqrt(N). The 1/sqrt2 is folded into the
 * radix-2 twiddle for b, and applied as the real constant g_r2c for a.
 * Caller must build_tables_fx(M,M1,M2,s) (for the inner 4-step) AND build_r2tw(N,s). */
#define ONE_OVER_SQRT2_Q15 23170
static c16 *g_r2tw;                       /* [M] W_N^{s*n} pre-scaled by 1/sqrt2 */
static const c16 g_r2c = { ONE_OVER_SQRT2_Q15, 0 };  /* 1/sqrt2 as a complex constant */
static void build_r2tw(int N, int s){
  int M=N/2;
  g_r2tw=malloc(sizeof(c16)*M);
  for (int n=0;n<M;n++){
    double a=s*2.0*M_PI*(double)n/(double)N;
    g_r2tw[n]=(c16){ sat16((int32_t)lround(cos(a)*ONE_OVER_SQRT2_Q15)), sat16((int32_t)lround(sin(a)*ONE_OVER_SQRT2_Q15)) };
  }
}
static void radix2_fx(const c16 *in, c16 *out, int N, int M1, int M2){
  int M=N/2;
  c16 *a=malloc(sizeof(c16)*M),*b=malloc(sizeof(c16)*M),*A=malloc(sizeof(c16)*M),*B=malloc(sizeof(c16)*M);
  for (int n=0;n<M;n++){
    c16 sm={ sadd(in[n].r,in[n+M].r), sadd(in[n].i,in[n+M].i) };
    c16 df={ ssub(in[n].r,in[n+M].r), ssub(in[n].i,in[n+M].i) };
    a[n]=cmul15n(sm,g_r2c);
    b[n]=cmul15n(df,g_r2tw[n]);
  }
  fourstep_fx(a,A,M,M1,M2);
  fourstep_fx(b,B,M,M1,M2);
  for (int r=0;r<M;r++){ out[2*r]=A[r]; out[2*r+1]=B[r]; }
  free(a);free(b);free(A);free(B);
}

/* ================= RVV 4-step ================= */
#if defined(__riscv) && defined(__riscv_vector)
#include <riscv_vector.h>
#define V16 vint16mf2_t
#define GET(X,f) __riscv_vget_v_i16mf2x2_i16mf2((X),(f))
#define LD2(p) __riscv_vlseg2e16_v_i16mf2x2((p),vl)
#define ADD(a,b) __riscv_vsadd_vv_i16mf2((a),(b),vl)
#define SUB(a,b) __riscv_vssub_vv_i16mf2((a),(b),vl)
#define NEG(a) __riscv_vneg_v_i16mf2((a),vl)
#define SRA1(a) __riscv_vsra_vx_i16mf2((a),1,vl)
#define RSRA1(a) __riscv_vssra_vx_i16mf2((a),1,__RISCV_VXRM_RNU,vl) /* round(x/2) */
#define ST2(p,re,im) __riscv_vsseg2e16_v_i16mf2x2((p),__riscv_vcreate_v_i16mf2x2((re),(im)),vl)
static void subfft_rvv(c16 *a, c16 *b, int M, int B, int which, int s){
  int np=g_subnp[which], M4=M/4;
  for (int p=0,L=M;p<np;p++,L/=4){
    int Lq=L/4;
    for (int bf=0;bf<M4;bf++){
      int k=bf/Lq,j=bf%Lq,r0=k*L+j,r1=r0+Lq,r2=r0+2*Lq,r3=r0+3*Lq;
      c16 w1=g_subtw[which][p][bf][0],w2=g_subtw[which][p][bf][1],w3=g_subtw[which][p][bf][2];
      const int16_t *A0=(const int16_t*)(a+r0*B),*A1=(const int16_t*)(a+r1*B),*A2=(const int16_t*)(a+r2*B),*A3=(const int16_t*)(a+r3*B);
      int16_t *B0=(int16_t*)(b+bf*B),*B1=(int16_t*)(b+(bf+M4)*B),*B2=(int16_t*)(b+(bf+2*M4)*B),*B3=(int16_t*)(b+(bf+3*M4)*B);
      for (int c=0;c<B;){
        size_t vl=__riscv_vsetvl_e16mf2(B-c); int o=2*c;
        vint16mf2x2_t X0=LD2(A0+o),X1=LD2(A1+o),X2=LD2(A2+o),X3=LD2(A3+o);
        V16 x0r=GET(X0,0),x0i=GET(X0,1),x1r=GET(X1,0),x1i=GET(X1,1),x2r=GET(X2,0),x2i=GET(X2,1),x3r=GET(X3,0),x3i=GET(X3,1);
        V16 d0r=ADD(ADD(x0r,x1r),ADD(x2r,x3r)), d0i=ADD(ADD(x0i,x1i),ADD(x2i,x3i));
        V16 d2r=SUB(ADD(x0r,x2r),ADD(x1r,x3r)), d2i=SUB(ADD(x0i,x2i),ADD(x1i,x3i));
        V16 j1r,j1i,j3r,j3i;
        if(s>0){ j1r=NEG(x1i); j1i=x1r; j3r=NEG(x3i); j3i=x3r; }
        else   { j1r=x1i; j1i=NEG(x1r); j3r=x3i; j3i=NEG(x3r); }
        V16 er=SUB(x0r,x2r), ei=SUB(x0i,x2i), fr=SUB(j1r,j3r), fi=SUB(j1i,j3i);
        V16 d1r=ADD(er,fr), d1i=ADD(ei,fi), d3r=SUB(er,fr), d3i=SUB(ei,fi);
        ST2(B0+o, RSRA1(d0r), RSRA1(d0i));
        /* fold the radix-4 1/2 into a single RNU round at >>16 (twiddle stays full Q15) */
        #define TW(DR,DI,W,DST) do{ \
          vint32m1_t re=__riscv_vsub_vv_i32m1(__riscv_vwmul_vx_i32m1(DR,(W).r,vl),__riscv_vwmul_vx_i32m1(DI,(W).i,vl),vl); \
          vint32m1_t im=__riscv_vadd_vv_i32m1(__riscv_vwmul_vx_i32m1(DR,(W).i,vl),__riscv_vwmul_vx_i32m1(DI,(W).r,vl),vl); \
          V16 rr=__riscv_vnclip_wx_i16mf2(re,16,__RISCV_VXRM_RNU,vl), ii=__riscv_vnclip_wx_i16mf2(im,16,__RISCV_VXRM_RNU,vl); \
          ST2(DST+o, rr, ii); }while(0)
        TW(d1r,d1i,w1,B1); TW(d2r,d2i,w2,B2); TW(d3r,d3i,w3,B3);
        #undef TW
        c+=(int)vl;
      }
    }
    c16*t=a;a=b;b=t;
  }
}
static void twiddle_rvv(c16 *A, const c16 *W, int rows, int B){
  for (int r=0;r<rows;r++){
    const int16_t *pa=(const int16_t*)(A+r*B),*pw=(const int16_t*)(W+r*B); int16_t *po=(int16_t*)(A+r*B);
    for (int c=0;c<B;){
      size_t vl=__riscv_vsetvl_e16mf2(B-c); int o=2*c;
      vint16mf2x2_t Xa=LD2(pa+o),Xw=LD2(pw+o);
      V16 ar=GET(Xa,0),ai=GET(Xa,1),wr=GET(Xw,0),wi=GET(Xw,1);
      vint32m1_t re=__riscv_vsub_vv_i32m1(__riscv_vwmul_vv_i32m1(ar,wr,vl),__riscv_vwmul_vv_i32m1(ai,wi,vl),vl);
      vint32m1_t im=__riscv_vadd_vv_i32m1(__riscv_vwmul_vv_i32m1(ar,wi,vl),__riscv_vwmul_vv_i32m1(ai,wr,vl),vl);
      ST2(po+o, __riscv_vnclip_wx_i16mf2(re,15,__RISCV_VXRM_RNU,vl), __riscv_vnclip_wx_i16mf2(im,15,__RISCV_VXRM_RNU,vl));
      c+=(int)vl;
    }
  }
}
static void transpose_rvv(const c16 *A, c16 *At, int N1, int N2){
  for (int k1=0;k1<N1;k1++){
    const uint32_t *src=(const uint32_t*)(A+k1*N2); uint32_t *dst=(uint32_t*)(At+k1);
    for (int n2=0;n2<N2;){
      size_t vl=__riscv_vsetvl_e32m1(N2-n2);
      __riscv_vsse32_v_u32m1(dst+(size_t)n2*N1, (ptrdiff_t)N1*4, __riscv_vle32_v_u32m1(src+n2,vl), vl);
      n2+=(int)vl;
    }
  }
}
static c16 g4A[4096],g4B[4096],g4T[4096];
static void fourstep_rvv(const c16 *in, c16 *out, int N,int N1,int N2){
  memcpy(g4A,in,sizeof(c16)*N);
  c16 *A=g4A,*B=g4B;
  subfft_rvv(A,B,N1,N2,0,g_s); if(g_subnp[0]&1){c16*t=A;A=B;B=t;}
  twiddle_rvv(A,g_btw,N1,N2);
  transpose_rvv(A,g4T,N1,N2);
  A=g4T;B=g4A;  /* g4T is a distinct scratch buffer; g4A/g4B are free to clobber now */
  subfft_rvv(A,B,N2,N1,1,g_s);
  memcpy(out,(g_subnp[1]&1)?B:A,sizeof(c16)*N);
}
/* radix-2 DIF stage (unit-stride) wrapping fourstep_rvv on M=N/2, then a stride-2
 * interleave store. Tables (g_* for M, g_r2tw for N) built by the caller. */
static c16 r2a[4096],r2b[4096],r2A[4096],r2B[4096];   /* M<=4096 */
static void radix2_rvv(const c16 *in, c16 *out, int N, int M1, int M2){
  int M=N/2;
  const int16_t *xr=(const int16_t*)in, *wr2=(const int16_t*)g_r2tw;
  int16_t *pa=(int16_t*)r2a, *pb=(int16_t*)r2b;
  for (int n=0;n<M;){
    size_t vl=__riscv_vsetvl_e16mf2(M-n); int o=2*n;
    vint16mf2x2_t Xl=LD2(xr+o), Xh=LD2(xr+2*M+o);       /* in[n], in[n+M] */
    V16 lr=GET(Xl,0),li=GET(Xl,1),hr=GET(Xh,0),hi=GET(Xh,1);
    V16 sr=ADD(lr,hr), si=ADD(li,hi), dr=SUB(lr,hr), di=SUB(li,hi);
    /* a = sum * 1/sqrt2 (real constant) */
    V16 ar=__riscv_vnclip_wx_i16mf2(__riscv_vwmul_vx_i32m1(sr,ONE_OVER_SQRT2_Q15,vl),15,__RISCV_VXRM_RNU,vl);
    V16 ai=__riscv_vnclip_wx_i16mf2(__riscv_vwmul_vx_i32m1(si,ONE_OVER_SQRT2_Q15,vl),15,__RISCV_VXRM_RNU,vl);
    ST2(pa+o, ar, ai);
    /* b = diff * (W_N^{s*n} scaled by 1/sqrt2) */
    vint16mf2x2_t W=LD2(wr2+o); V16 wrr=GET(W,0),wii=GET(W,1);
    vint32m1_t bre=__riscv_vsub_vv_i32m1(__riscv_vwmul_vv_i32m1(dr,wrr,vl),__riscv_vwmul_vv_i32m1(di,wii,vl),vl);
    vint32m1_t bim=__riscv_vadd_vv_i32m1(__riscv_vwmul_vv_i32m1(dr,wii,vl),__riscv_vwmul_vv_i32m1(di,wrr,vl),vl);
    ST2(pb+o, __riscv_vnclip_wx_i16mf2(bre,15,__RISCV_VXRM_RNU,vl), __riscv_vnclip_wx_i16mf2(bim,15,__RISCV_VXRM_RNU,vl));
    n+=(int)vl;
  }
  fourstep_rvv(r2a,r2A,M,M1,M2);
  fourstep_rvv(r2b,r2B,M,M1,M2);
  uint32_t *dst=(uint32_t*)out;                          /* interleave even/odd */
  const uint32_t *sA=(const uint32_t*)r2A, *sB=(const uint32_t*)r2B;
  for (int r=0;r<M;){
    size_t vl=__riscv_vsetvl_e32m1(M-r);
    __riscv_vsse32_v_u32m1(dst+2*r,   8, __riscv_vle32_v_u32m1(sA+r,vl), vl);
    __riscv_vsse32_v_u32m1(dst+2*r+1, 8, __riscv_vle32_v_u32m1(sB+r,vl), vl);
    r+=(int)vl;
  }
}
#endif

static double snr(const cd *a, const cd *b, int N){
  double num = 0, den = 0;
  for (int i = 0; i < N; i++){ double er = a[i].r - b[i].r, ei = a[i].i - b[i].i; num += er*er+ei*ei; den += b[i].r*b[i].r + b[i].i*b[i].i; }
  return 10.0 * log10(den / (num + 1e-30));
}

static uint32_t rng = 12345;
static double rnd(void){ rng = rng*1103515245u+12345u; return (double)((int)(rng>>16) - 32768) / 32768.0; }

static double snr_fx(const c16 *a, const cd *b, int N){  /* a fixed vs b double (already scaled) */
  double num=0,den=0;
  for(int i=0;i<N;i++){ double er=a[i].r-b[i].r, ei=a[i].i-b[i].i; num+=er*er+ei*ei; den+=b[i].r*b[i].r+b[i].i*b[i].i; }
  return 10.0*log10(den/(num+1e-30));
}
int main(int argc, char **argv){
  int cpu = argc>1 ? atoi(argv[1]) : -1;
  if (cpu>=0){ if(cpu>7) use_ai(); pin_cpu(cpu); }
  int amp = argc>2 ? atoi(argv[2]) : 8000;   /* input amplitude (headroom probe) */
  struct { int N, N1, N2; } cfg[] = { {64,4,16}, {256,16,16}, {1024,16,64}, {4096,64,64} };
  int allok = 1;
  for (unsigned t = 0; t < sizeof(cfg)/sizeof(cfg[0]); t++){
    int N = cfg[t].N, N1 = cfg[t].N1, N2 = cfg[t].N2;
    cd *x = malloc(sizeof(cd)*N), *ref = malloc(sizeof(cd)*N), *got = malloc(sizeof(cd)*N), *xd = malloc(sizeof(cd)*N);
    c16 *xi = malloc(sizeof(c16)*N), *goti = malloc(sizeof(c16)*N), *gotr = malloc(sizeof(c16)*N);
    for (int i = 0; i < N; i++){ x[i].r = rnd(); x[i].i = rnd(); xi[i]=(c16){(int16_t)(x[i].r*amp),(int16_t)(x[i].i*amp)}; xd[i]=(cd){xi[i].r,xi[i].i}; }
    for (int s = 1; s >= -1; s -= 2){
      dft_naive(x, ref, N, s);
      fourstep_double(x, got, N, N1, N2, s);
      double sd = snr(got, ref, N);
      /* fixed-point: ref = DFT(xi)/sqrt(N) (double), compare to fourstep_fx(xi) */
      dft_naive(xd, ref, N, s);
      double inv = 1.0/sqrt((double)N);
      for (int i=0;i<N;i++){ ref[i].r*=inv; ref[i].i*=inv; }
      build_tables_fx(N,N1,N2,s); fourstep_fx(xi, goti, N, N1, N2);
      double rvv_snr = -999; int diff = -1; double us_rvv = 0, us_fx = 0;
#if defined(__riscv) && defined(__riscv_vector)
      fourstep_rvv(xi, gotr, N, N1, N2);
      diff = 0; for (int i=0;i<N;i++) if (gotr[i].r!=goti[i].r || gotr[i].i!=goti[i].i) diff++;
      rvv_snr = snr_fx(gotr, ref, N);
      { int R = N>=1024?2000:8000;
        double t0=now_us(); for(int r=0;r<R;r++) fourstep_rvv(xi,gotr,N,N1,N2); us_rvv=(now_us()-t0)/R;
        t0=now_us(); for(int r=0;r<R;r++) fourstep_fx(xi,goti,N,N1,N2); us_fx=(now_us()-t0)/R; }
#endif
      printf("N=%5d (%dx%d) s=%+d  4step-vs-naive(dbl)=%.0fdB  fixed-vs-dbl=%.1fdB",
             N, N1, N2, s, sd, snr_fx(goti, ref, N));
#if defined(__riscv) && defined(__riscv_vector)
      printf("  rvv-vs-fx diff=%d rvv-snr=%.1fdB  t_rvv=%.2fus t_fx=%.2fus", diff, rvv_snr, us_rvv, us_fx);
      if (diff!=0) allok = 0;
#endif
      printf("\n");
      free(g_btw);
    }
    free(x); free(ref); free(got); free(xd); free(xi); free(goti); free(gotr);
  }
  /* radix-2 sizes: N = 2*M, inner 4-step on M=N/2 (M1xM2) */
  struct { int N, M1, M2; } r2cfg[] = { {128,4,16}, {512,16,16}, {2048,16,64}, {8192,64,64} };
  for (unsigned t = 0; t < sizeof(r2cfg)/sizeof(r2cfg[0]); t++){
    int N = r2cfg[t].N, M = N/2, M1 = r2cfg[t].M1, M2 = r2cfg[t].M2;
    cd *ref = malloc(sizeof(cd)*N), *xd = malloc(sizeof(cd)*N);
    c16 *xi = malloc(sizeof(c16)*N), *goti = malloc(sizeof(c16)*N), *gotr = malloc(sizeof(c16)*N);
    for (int i = 0; i < N; i++){ double r=rnd(), im=rnd(); xi[i]=(c16){(int16_t)(r*amp),(int16_t)(im*amp)}; xd[i]=(cd){xi[i].r,xi[i].i}; }
    for (int s = 1; s >= -1; s -= 2){
      dft_naive(xd, ref, N, s);
      double inv = 1.0/sqrt((double)N);
      for (int i=0;i<N;i++){ ref[i].r*=inv; ref[i].i*=inv; }
      build_tables_fx(M,M1,M2,s); build_r2tw(N,s);
      radix2_fx(xi, goti, N, M1, M2);
      double rvv_snr = -999; int diff = -1; double us_rvv = 0;
#if defined(__riscv) && defined(__riscv_vector)
      radix2_rvv(xi, gotr, N, M1, M2);
      diff = 0; for (int i=0;i<N;i++) if (gotr[i].r!=goti[i].r || gotr[i].i!=goti[i].i) diff++;
      rvv_snr = snr_fx(gotr, ref, N);
      { int R = N>=1024?2000:8000;
        double t0=now_us(); for(int r=0;r<R;r++) radix2_rvv(xi,gotr,N,M1,M2); us_rvv=(now_us()-t0)/R; }
#endif
      printf("N=%5d (2x%d=%dx%d) s=%+d  fixed-vs-dbl=%.1fdB", N, M, M1, M2, s, snr_fx(goti, ref, N));
#if defined(__riscv) && defined(__riscv_vector)
      printf("  rvv-vs-fx diff=%d rvv-snr=%.1fdB  t_rvv=%.2fus", diff, rvv_snr, us_rvv);
      if (diff!=0) allok = 0;
#endif
      printf("\n");
      free(g_btw); free(g_r2tw);
    }
    free(ref); free(xd); free(xi); free(goti); free(gotr);
  }
  return allok?0:1;
}
