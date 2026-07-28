/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * RVV nr_qam16_llr_2layer (nr_compute_llr.c) validation. This is intricate enough
 * that the REFERENCE here is the ACTUAL SIMDe 128-bit code (helpers + loop body
 * copied verbatim) = ground truth, compared byte-exact against the RVV kernel.
 * The RVV kernel mirrors the SIMDe structure 1:1 (same rho_rs/psi/bit_mets arrays
 * and index tables) with __riscv ops, vlseg2 for the free re/im deinterleave and
 * vsseg4e16 for the 4-LLR/RE output.
 *
 * LAUNCH: main() switches core first; run_tests() (noinline) runs at post-switch VLEN.
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <unistd.h>
#include <simde/x86/sse2.h>
#include <simde/x86/sse4.1.h>
#include <riscv_vector.h>

typedef struct { int16_t r, i; } c16_t;
static int pin_cpu(int c){ cpu_set_t s; CPU_ZERO(&s); CPU_SET(c,&s); return sched_setaffinity(0,sizeof(s),&s); }
static int use_ai(void){ FILE*f=fopen("/proc/set_ai_thread","w"); if(!f)return -1; fprintf(f,"%ld\n",(long)getpid()); return fclose(f); }

/* ===================== SIMDe ground-truth (verbatim helpers + body) ===================== */
#define SHUFFLE_MASK SIMDE_MM_SHUFFLE(3, 1, 2, 0)
static inline simde__m128i allones128(void){ return simde_mm_set1_epi32(-1); }
static inline simde__m128i presort(simde__m128i x){
  return simde_mm_shuffle_epi32(simde_mm_shufflehi_epi16(simde_mm_shufflelo_epi16(x, SHUFFLE_MASK), SHUFFLE_MASK), SHUFFLE_MASK);
}
static void sep(simde__m128i *re, simde__m128i *im, simde__m128i a, simde__m128i b){
  simde__m128i x0=presort(a), x1=presort(b);
  if(re) *re=simde_mm_unpacklo_epi64(x0,x1);
  if(im) *im=simde_mm_unpackhi_epi64(x0,x1);
}
static inline simde__m128i prodsum_psi_a(simde__m128i pr, simde__m128i ar, simde__m128i pi, simde__m128i ai){
  simde__m128i t=simde_mm_slli_epi16(simde_mm_mulhi_epi16(pr,ar),1);
  simde__m128i t2=simde_mm_slli_epi16(simde_mm_mulhi_epi16(pi,ai),1);
  return simde_mm_adds_epi16(t,t2);
}
static inline simde__m128i interf_abs(simde__m128i psi, simde__m128i icm, simde__m128i c1, simde__m128i c2){
  simde__m128i t=simde_mm_cmplt_epi16(psi,icm), t2=simde_mm_xor_si128(t,allones128());
  return simde_mm_or_si128(simde_mm_and_si128(t,c1), simde_mm_and_si128(t2,c2));
}
static inline simde__m128i square_a(simde__m128i ar, simde__m128i ai, simde__m128i icm, simde__m128i sf){
  simde__m128i t=simde_mm_slli_epi16(simde_mm_mulhi_epi16(ar,ar),1);
  t=simde_mm_slli_epi16(simde_mm_mulhi_epi16(t,sf),1); t=simde_mm_slli_epi16(simde_mm_mulhi_epi16(t,icm),1);
  simde__m128i t2=simde_mm_slli_epi16(simde_mm_mulhi_epi16(ai,ai),1);
  t2=simde_mm_slli_epi16(simde_mm_mulhi_epi16(t2,sf),1); t2=simde_mm_slli_epi16(simde_mm_mulhi_epi16(t2,icm),1);
  return simde_mm_adds_epi16(t,t2);
}
static inline simde__m128i max8(simde__m128i m0,simde__m128i m1,simde__m128i m2,simde__m128i m3,simde__m128i m4,simde__m128i m5,simde__m128i m6,simde__m128i m7){
  simde__m128i a0=simde_mm_max_epi16(m0,m1),a1=simde_mm_max_epi16(m2,m3),a2=simde_mm_max_epi16(m4,m5),a3=simde_mm_max_epi16(m6,m7);
  return simde_mm_max_epi16(simde_mm_max_epi16(a0,a1),simde_mm_max_epi16(a2,a3));
}
static void qam16_ref(c16_t *s0, c16_t *s1, c16_t *chm, c16_t *chmi, int16_t *out, c16_t *rho, uint32_t length){
  simde__m128i *rho01_128i=(simde__m128i*)rho,*stream0_128i_in=(simde__m128i*)s0,*stream1_128i_in=(simde__m128i*)s1;
  simde__m128i *stream0_128i_out=(simde__m128i*)out,*ch_mag_128i=(simde__m128i*)chm,*ch_mag_128i_i=(simde__m128i*)chmi;
  simde__m128i ONE_OVER_SQRT_10=simde_mm_set1_epi16(20724), ONE_OVER_SQRT_10_Q15=simde_mm_set1_epi16(10362);
  simde__m128i THREE_OVER_SQRT_10=simde_mm_set1_epi16(31086), SQRT_10_OVER_FOUR=simde_mm_set1_epi16(25905);
  simde__m128i ONE_OVER_TWO_SQRT_10=simde_mm_set1_epi16(10362), NINE_OVER_TWO_SQRT_10=simde_mm_set1_epi16(23315);
  simde__m128i ch_mag_des,ch_mag_int,xmm0,xmm1,xmm2,xmm3,xmm4,xmm5,xmm6,xmm7;
  simde__m128i rho_rpi,rho_rmi,rho_rs[8],psi_rs[16],psi_is[16],a_rs[16],a_is[16],psi_as[16],a_sqs[16],y0_s[8];
  simde__m128i y0r,y0i,y1r,y1i;
  for (int i=0;i<length>>2;i+=2){
    sep(&xmm2,&xmm3,rho01_128i[i],rho01_128i[i+1]);
    rho_rpi=simde_mm_adds_epi16(xmm2,xmm3); rho_rmi=simde_mm_subs_epi16(xmm2,xmm3);
    rho_rs[0]=simde_mm_mulhi_epi16(rho_rpi,ONE_OVER_SQRT_10); rho_rs[4]=simde_mm_mulhi_epi16(rho_rmi,ONE_OVER_SQRT_10);
    rho_rs[3]=simde_mm_slli_epi16(simde_mm_mulhi_epi16(rho_rpi,THREE_OVER_SQRT_10),1);
    rho_rs[7]=simde_mm_slli_epi16(simde_mm_mulhi_epi16(rho_rmi,THREE_OVER_SQRT_10),1);
    xmm4=simde_mm_mulhi_epi16(xmm2,ONE_OVER_SQRT_10); xmm5=simde_mm_slli_epi16(simde_mm_mulhi_epi16(xmm3,THREE_OVER_SQRT_10),1);
    rho_rs[1]=simde_mm_adds_epi16(xmm4,xmm5); rho_rs[5]=simde_mm_subs_epi16(xmm4,xmm5);
    xmm6=simde_mm_slli_epi16(simde_mm_mulhi_epi16(xmm2,THREE_OVER_SQRT_10),1); xmm7=simde_mm_mulhi_epi16(xmm3,ONE_OVER_SQRT_10);
    rho_rs[2]=simde_mm_adds_epi16(xmm6,xmm7); rho_rs[6]=simde_mm_subs_epi16(xmm6,xmm7);
    sep(&y1r,&y1i,stream1_128i_in[i],stream1_128i_in[i+1]);
    for(int j=0;j<8;j++) psi_rs[j]=simde_mm_abs_epi16(simde_mm_subs_epi16(rho_rs[j],y1r));
    for(int j=8;j<16;j++) psi_rs[j]=simde_mm_abs_epi16(simde_mm_adds_epi16(rho_rs[(j-4)&7],y1r));
    const uint8_t idx[16]={4,6,5,7,0,2,1,3,0,2,1,3,4,6,5,7};
    for(int k=0;k<16;k+=8) for(int j=k;j<k+4;j++){
      psi_is[j]=simde_mm_abs_epi16(simde_mm_subs_epi16(rho_rs[idx[j]],y1i));
      psi_is[j+4]=simde_mm_abs_epi16(simde_mm_adds_epi16(rho_rs[idx[j+4]],y1i));
    }
    sep(&y0r,&y0i,stream0_128i_in[i],stream0_128i_in[i+1]);
    sep(&ch_mag_des,&xmm2,ch_mag_128i[i],ch_mag_128i[i+1]);
    sep(&ch_mag_int,&xmm2,ch_mag_128i_i[i],ch_mag_128i_i[i+1]);
    simde__m128i y0ros=simde_mm_mulhi_epi16(y0r,ONE_OVER_SQRT_10), y0ios=simde_mm_mulhi_epi16(y0i,ONE_OVER_SQRT_10);
    simde__m128i y0r3=simde_mm_slli_epi16(simde_mm_mulhi_epi16(y0r,THREE_OVER_SQRT_10),1), y0i3=simde_mm_slli_epi16(simde_mm_mulhi_epi16(y0i,THREE_OVER_SQRT_10),1);
    y0_s[0]=simde_mm_adds_epi16(y0ros,y0ios); y0_s[4]=simde_mm_subs_epi16(y0ros,y0ios);
    y0_s[1]=simde_mm_adds_epi16(y0ros,y0i3);  y0_s[5]=simde_mm_subs_epi16(y0ros,y0i3);
    y0_s[2]=simde_mm_adds_epi16(y0r3,y0ios);  y0_s[6]=simde_mm_subs_epi16(y0r3,y0ios);
    y0_s[3]=simde_mm_adds_epi16(y0r3,y0i3);   y0_s[7]=simde_mm_subs_epi16(y0r3,y0i3);
    for(int j=0;j<16;j++){
      a_rs[j]=interf_abs(psi_rs[j],ch_mag_int,ONE_OVER_SQRT_10_Q15,THREE_OVER_SQRT_10);
      a_is[j]=interf_abs(psi_is[j],ch_mag_int,ONE_OVER_SQRT_10_Q15,THREE_OVER_SQRT_10);
      psi_as[j]=prodsum_psi_a(psi_rs[j],a_rs[j],psi_is[j],a_is[j]);
      a_sqs[j]=square_a(a_rs[j],a_is[j],ch_mag_int,SQRT_10_OVER_FOUR);
    }
    simde__m128i cm10=simde_mm_mulhi_epi16(ch_mag_des,ONE_OVER_TWO_SQRT_10);
    simde__m128i cm2=simde_mm_slli_epi16(simde_mm_mulhi_epi16(ch_mag_des,SQRT_10_OVER_FOUR),1);
    simde__m128i cm9=simde_mm_slli_epi16(simde_mm_mulhi_epi16(ch_mag_des,NINE_OVER_TWO_SQRT_10),2);
    simde__m128i bm[16];
    for(int j=0;j<8;j+=4){
      bm[j+0]=simde_mm_subs_epi16(simde_mm_adds_epi16(simde_mm_subs_epi16(psi_as[j+0],a_sqs[j+0]),y0_s[j+0]),cm10);
      bm[j+1]=simde_mm_subs_epi16(simde_mm_adds_epi16(simde_mm_subs_epi16(psi_as[j+1],a_sqs[j+1]),y0_s[j+1]),cm2);
      bm[j+2]=simde_mm_subs_epi16(simde_mm_adds_epi16(simde_mm_subs_epi16(psi_as[j+2],a_sqs[j+2]),y0_s[j+2]),cm2);
      bm[j+3]=simde_mm_subs_epi16(simde_mm_adds_epi16(simde_mm_subs_epi16(psi_as[j+3],a_sqs[j+3]),y0_s[j+3]),cm9);
    }
    for(int j=8;j<16;j+=4){
      bm[j+0]=simde_mm_subs_epi16(simde_mm_subs_epi16(simde_mm_subs_epi16(psi_as[j+0],a_sqs[j+0]),y0_s[(j-4)&7]),cm10);
      bm[j+1]=simde_mm_subs_epi16(simde_mm_subs_epi16(simde_mm_subs_epi16(psi_as[j+1],a_sqs[j+1]),y0_s[(j-3)&7]),cm2);
      bm[j+2]=simde_mm_subs_epi16(simde_mm_subs_epi16(simde_mm_subs_epi16(psi_as[j+2],a_sqs[j+2]),y0_s[(j-2)&7]),cm2);
      bm[j+3]=simde_mm_subs_epi16(simde_mm_subs_epi16(simde_mm_subs_epi16(psi_as[j+3],a_sqs[j+3]),y0_s[(j-1)&7]),cm9);
    }
    simde__m128i n_re0=max8(bm[8],bm[9],bm[10],bm[11],bm[12],bm[13],bm[14],bm[15]);
    simde__m128i d_re0=max8(bm[0],bm[1],bm[2],bm[3],bm[4],bm[5],bm[6],bm[7]);
    simde__m128i n_re1=max8(bm[4],bm[5],bm[6],bm[7],bm[12],bm[13],bm[14],bm[15]);
    simde__m128i d_re1=max8(bm[0],bm[1],bm[3],bm[2],bm[8],bm[9],bm[10],bm[11]);
    simde__m128i n_im0=max8(bm[2],bm[3],bm[6],bm[7],bm[10],bm[11],bm[14],bm[15]);
    simde__m128i d_im0=max8(bm[0],bm[1],bm[4],bm[5],bm[8],bm[9],bm[12],bm[13]);
    simde__m128i n_im1=max8(bm[1],bm[3],bm[5],bm[7],bm[9],bm[11],bm[13],bm[15]);
    simde__m128i d_im1=max8(bm[0],bm[2],bm[4],bm[6],bm[8],bm[10],bm[12],bm[14]);
    y0r=simde_mm_subs_epi16(d_re0,n_re0); y1r=simde_mm_subs_epi16(d_re1,n_re1);
    y0i=simde_mm_subs_epi16(d_im0,n_im0); y1i=simde_mm_subs_epi16(d_im1,n_im1);
    xmm0=simde_mm_unpacklo_epi16(y0r,y1r); xmm1=simde_mm_unpackhi_epi16(y0r,y1r);
    xmm2=simde_mm_unpacklo_epi16(y0i,y1i); xmm3=simde_mm_unpackhi_epi16(y0i,y1i);
    stream0_128i_out[2*i+0]=simde_mm_unpacklo_epi32(xmm0,xmm2);
    stream0_128i_out[2*i+1]=simde_mm_unpackhi_epi32(xmm0,xmm2);
    stream0_128i_out[2*i+2]=simde_mm_unpacklo_epi32(xmm1,xmm3);
    stream0_128i_out[2*i+3]=simde_mm_unpackhi_epi32(xmm1,xmm3);
  }
}

/* ===================== RVV kernel (mirrors the SIMDe structure 1:1) ===================== */
static size_t vl;
#define MH(x,c)  __riscv_vnsra_wx_i16m1(__riscv_vwmul_vx_i32m2((x),(c),vl),16,vl)          /* mulhi_epi16(x,const) */
#define MHV(x,y) __riscv_vnsra_wx_i16m1(__riscv_vwmul_vv_i32m2((x),(y),vl),16,vl)          /* mulhi_epi16(x,y)     */
#define SL1(x)   __riscv_vsll_vx_i16m1((x),1,vl)
#define SL2(x)   __riscv_vsll_vx_i16m1((x),2,vl)
#define AD(a,b)  __riscv_vsadd_vv_i16m1((a),(b),vl)
#define SU(a,b)  __riscv_vssub_vv_i16m1((a),(b),vl)
#define AB(x)    __riscv_vmax_vv_i16m1((x),__riscv_vneg_v_i16m1((x),vl),vl)
#define MX(a,b)  __riscv_vmax_vv_i16m1((a),(b),vl)
enum { C_1S10=20724, C_1S10Q15=10362, C_3S10=31086, C_S10_4=25905, C_1_2S10=10362, C_9_2S10=23315 };
static inline vint16m1_t rv_prodsum(vint16m1_t pr,vint16m1_t ar,vint16m1_t pi,vint16m1_t ai){
  return AD(SL1(MHV(pr,ar)), SL1(MHV(pi,ai)));
}
static inline vint16m1_t rv_interf(vint16m1_t psi,vint16m1_t icm){
  vbool16_t m=__riscv_vmslt_vv_i16m1_b16(psi,icm,vl);   /* psi < icm ? c1 : c2 */
  return __riscv_vmerge_vvm_i16m1(__riscv_vmv_v_x_i16m1(C_3S10,vl), __riscv_vmv_v_x_i16m1(C_1S10Q15,vl), m, vl);
}
static inline vint16m1_t rv_square(vint16m1_t ar,vint16m1_t ai,vint16m1_t icm){
  vint16m1_t t=SL1(MHV(ar,ar)); t=SL1(MH(t,C_S10_4)); t=SL1(MHV(t,icm));
  vint16m1_t t2=SL1(MHV(ai,ai)); t2=SL1(MH(t2,C_S10_4)); t2=SL1(MHV(t2,icm));
  return AD(t,t2);
}
/* sizeless RVV vectors can't be arrayed, and 16 live bit-metrics exceed the 32
 * registers -- so the arrayed intermediates (psi_rs/psi_is/bm) spill to int16
 * scratch rows [.][VM] (VM = max VLMAX at VLEN=1024 = 64). rho_rs/y0_s/cm stay
 * in registers (staged, never all-live). ST/LD spill helpers. */
#define VM 64
#define ST(buf,v) __riscv_vse16_v_i16m1((buf),(v),vl)
#define LD(buf)   __riscv_vle16_v_i16m1((buf),vl)
#define MXb(a,c,d,e,f,g,h,k) MX(MX(MX(LD(bm[a]),LD(bm[c])),MX(LD(bm[d]),LD(bm[e]))),MX(MX(LD(bm[f]),LD(bm[g])),MX(LD(bm[h]),LD(bm[k]))))
static void qam16_rvv(c16_t *s0,c16_t *s1,c16_t *chm,c16_t *chmi,int16_t *out,c16_t *rho,uint32_t length){
  const int16_t *p0=(const int16_t*)s0,*p1=(const int16_t*)s1,*pr=(const int16_t*)rho,*pm=(const int16_t*)chm,*pmi=(const int16_t*)chmi;
  const uint8_t idx[16]={4,6,5,7,0,2,1,3,0,2,1,3,4,6,5,7};
  int16_t psi_rs[16][VM] __attribute__((aligned(16))), psi_is[16][VM] __attribute__((aligned(16))), bm[16][VM] __attribute__((aligned(16)));
  int16_t y0s[8][VM] __attribute__((aligned(16))), rrs[8][VM] __attribute__((aligned(16)));
  for(uint32_t n=0;n<length;){
    vl=__riscv_vsetvl_e16m1(length-n); uint32_t o=2*n;
    vint16m1x2_t RH=__riscv_vlseg2e16_v_i16m1x2(pr+o,vl); vint16m1_t rr=__riscv_vget_v_i16m1x2_i16m1(RH,0),ri=__riscv_vget_v_i16m1x2_i16m1(RH,1);
    vint16m1_t rho_rpi=AD(rr,ri), rho_rmi=SU(rr,ri);
    ST(rrs[0],MH(rho_rpi,C_1S10)); ST(rrs[4],MH(rho_rmi,C_1S10));
    ST(rrs[3],SL1(MH(rho_rpi,C_3S10))); ST(rrs[7],SL1(MH(rho_rmi,C_3S10)));
    vint16m1_t x4=MH(rr,C_1S10), x5=SL1(MH(ri,C_3S10)); ST(rrs[1],AD(x4,x5)); ST(rrs[5],SU(x4,x5));
    vint16m1_t x6=SL1(MH(rr,C_3S10)), x7=MH(ri,C_1S10); ST(rrs[2],AD(x6,x7)); ST(rrs[6],SU(x6,x7));
    vint16m1x2_t S1=__riscv_vlseg2e16_v_i16m1x2(p1+o,vl); vint16m1_t y1r=__riscv_vget_v_i16m1x2_i16m1(S1,0),y1i=__riscv_vget_v_i16m1x2_i16m1(S1,1);
    for(int j=0;j<8;j++) ST(psi_rs[j], AB(SU(LD(rrs[j]),y1r)));
    for(int j=8;j<16;j++) ST(psi_rs[j], AB(AD(LD(rrs[(j-4)&7]),y1r)));
    for(int k=0;k<16;k+=8) for(int j=k;j<k+4;j++){ ST(psi_is[j], AB(SU(LD(rrs[idx[j]]),y1i))); ST(psi_is[j+4], AB(AD(LD(rrs[idx[j+4]]),y1i))); }
    vint16m1x2_t S0=__riscv_vlseg2e16_v_i16m1x2(p0+o,vl); vint16m1_t y0r=__riscv_vget_v_i16m1x2_i16m1(S0,0),y0i=__riscv_vget_v_i16m1x2_i16m1(S0,1);
    vint16m1x2_t CM=__riscv_vlseg2e16_v_i16m1x2(pm+o,vl); vint16m1_t chd=__riscv_vget_v_i16m1x2_i16m1(CM,0);
    vint16m1x2_t CMI=__riscv_vlseg2e16_v_i16m1x2(pmi+o,vl); vint16m1_t chi=__riscv_vget_v_i16m1x2_i16m1(CMI,0);
    vint16m1_t y0ros=MH(y0r,C_1S10), y0ios=MH(y0i,C_1S10), y0r3=SL1(MH(y0r,C_3S10)), y0i3=SL1(MH(y0i,C_3S10));
    ST(y0s[0],AD(y0ros,y0ios)); ST(y0s[4],SU(y0ros,y0ios)); ST(y0s[1],AD(y0ros,y0i3)); ST(y0s[5],SU(y0ros,y0i3));
    ST(y0s[2],AD(y0r3,y0ios)); ST(y0s[6],SU(y0r3,y0ios)); ST(y0s[3],AD(y0r3,y0i3)); ST(y0s[7],SU(y0r3,y0i3));
    vint16m1_t cm10=MH(chd,C_1_2S10), cm2=SL1(MH(chd,C_S10_4)), cm9=SL2(MH(chd,C_9_2S10));
    for(int j=0;j<16;j++){
      vint16m1_t pr_=LD(psi_rs[j]), pi_=LD(psi_is[j]);
      vint16m1_t ar=rv_interf(pr_,chi), ai=rv_interf(pi_,chi);
      vint16m1_t base=SU(rv_prodsum(pr_,ar,pi_,ai), rv_square(ar,ai,chi));
      vint16m1_t cm=((j&3)==0)?cm10:((j&3)==3)?cm9:cm2;
      vint16m1_t y=(j<8)?LD(y0s[j]):LD(y0s[(j-4)&7]);
      ST(bm[j], (j<8)? SU(AD(base,y),cm) : SU(SU(base,y),cm));
    }
    vint16m1_t L1=SU(MXb(0,1,2,3,4,5,6,7), MXb(8,9,10,11,12,13,14,15));   /* d_re0 - n_re0 */
    vint16m1_t L2=SU(MXb(0,1,3,2,8,9,10,11), MXb(4,5,6,7,12,13,14,15));   /* d_re1 - n_re1 */
    vint16m1_t L3=SU(MXb(0,1,4,5,8,9,12,13), MXb(2,3,6,7,10,11,14,15));   /* d_im0 - n_im0 */
    vint16m1_t L4=SU(MXb(0,2,4,6,8,10,12,14), MXb(1,3,5,7,9,11,13,15));   /* d_im1 - n_im1 */
    __riscv_vsseg4e16_v_i16m1x4(out+4*n, __riscv_vcreate_v_i16m1x4(L1,L2,L3,L4), vl);
    n+=(uint32_t)vl;
  }
}

static uint32_t rng=0x16a3f;
static int16_t r16(void){ rng=rng*1103515245u+12345u; return (int16_t)(rng>>16); }
static uint16_t rmag(void){ rng=rng*1103515245u+12345u; return (uint16_t)(rng>>17); } /* positive-ish ch_mag */

__attribute__((noinline)) static int run_tests(void){
  int lens[]={8,96,824,3072}; int fail=0;   /* SIMDe qam16 processes 8 REs/iter -> length must be mult of 8 */
  for(unsigned t=0;t<sizeof(lens)/sizeof(lens[0]);t++){
    int n=lens[t];
    c16_t *s0=malloc(n*4+64),*s1=malloc(n*4+64),*chm=malloc(n*4+64),*chmi=malloc(n*4+64),*rho=malloc(n*4+64);
    int16_t *rref=malloc(n*4*2+64),*rgot=malloc(n*4*2+64);
    for(int k=0;k<n;k++){ s0[k]=(c16_t){r16(),r16()}; s1[k]=(c16_t){r16(),r16()}; rho[k]=(c16_t){r16(),r16()};
      int16_t m=rmag(),mi=rmag(); chm[k]=(c16_t){m,m}; chmi[k]=(c16_t){mi,mi}; }
    memset(rref,0,n*4*2); memset(rgot,0,n*4*2);
    qam16_ref(s0,s1,chm,chmi,rref,rho,n); qam16_rvv(s0,s1,chm,chmi,rgot,rho,n);
    int d=0; for(int k=0;k<4*n;k++) if(rref[k]!=rgot[k]) d++;
    printf("  len=%5d: diff=%d %s\n",n,d,d?"FAIL":"OK"); if(d) fail=1;
    free(s0);free(s1);free(chm);free(chmi);free(rho);free(rref);free(rgot);
  }
  return fail;
}
int main(int argc,char**argv){
  if(argc>1){int c=atoi(argv[1]); if(c>7) use_ai(); pin_cpu(c);}
  printf("VLEN=%zu\n",(size_t)__riscv_vlenb()*8);
  int fail=run_tests();
  printf("rvv qam16_llr_2layer test: %s\n", fail?"FAIL":"PASS");
  return fail;
}
