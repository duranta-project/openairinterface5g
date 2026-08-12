/* x86: input preparation + parity generation, per variant, BG1 and BG2 at Zc=384.
 *
 * This is the comparison that matters, and the reason this file exists: measuring
 * the encoder with the input already prepared inverts the ranking of the two
 * stock paths. Stock AVX2 replicates the systematic input simd_size times
 * (1.03 MB per code block at BG1 Zc=384) so that every shift becomes an aligned
 * load; the AVX512 permutex path needs only the doubled input. Score only the
 * encoder and AVX2 looks faster; count the preparation each one requires and
 * permutex wins by 1.6x-2.3x. The factored encoders need only the doubled input
 * too, and no permutes.
 *
 * Requires an AVX512VBMI part, since it includes the permutex stock encoder.
 *
 *   gcc -O3 -march=native -o bench_x86_prep_encode bench_x86_prep_encode.c \
 *       -I. -I<oai> -I<oai>/common/utils -I<oai>/openair1
 *   taskset -c <core> ./bench_x86_prep_encode
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#define ldpc384_byte s512
#if !defined(__AVX512F__) || !defined(__AVX512VBMI__)
#error "build with -march=native on an AVX512VBMI part"
#endif
#include "ldpc384_simd512_permutex_byte.c"
#undef ldpc384_byte
#define ldpc384_byte s256
#include "ldpc384_byte.c"
#undef ldpc384_byte
#include "ldpc384_factored_byte_512.c"
#include "ldpc384_factored_byte_256.c"
#include "ldpc384_factored_byte_128.c"
#define ldpc_BG2_Zc384_byte b2s
#include "ldpc_BG2_Zc384_byte.c"
#undef ldpc_BG2_Zc384_byte
#include "ldpc_BG2_Zc384_factored_byte_512.c"
#include "ldpc_BG2_Zc384_factored_byte_256.c"
#include "ldpc_BG2_Zc384_factored_byte_128.c"
#define Z 384
#define NC 22
#define SIMD 64
static uint8_t *c,*d,*cc;
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+1e-9*t.tv_nsec;}
static uint64_t sink=0;
typedef void(*F)(uint8_t*,uint8_t*);
static inline void prep(int nc,int ss){
  for(int i=0;i<nc;i++){memcpy(&c[2*i*Z],&cc[i*Z],Z);memcpy(&c[(2*i+1)*Z],&cc[i*Z],Z);}
  if(ss>1) for(int i=1;i<ss;i++) memcpy(&c[2*nc*Z*i],&c[i],2*nc*Z-i);
}
static double run(F f,int nc,int ss){
  const int N=20000; double r=1e18;
  for(int w=0;w<2;w++){
    for(int k=0;k<2000;k++){prep(nc,ss);f(c,d);}
    double t0=now(); uint64_t s=0;
    for(int i=0;i<N;i++){cc[(i*7)%(nc*Z)]^=(uint8_t)i; prep(nc,ss); f(c,d); s+=d[(i*13)%(40*Z)];}
    double t1=now(); sink+=s; double x=(t1-t0)/N*1e9; if(x<r)r=x;
  }
  return r;
}
static double enc_only(F f){
  const int N=40000; double r=1e18;
  for(int w=0;w<2;w++){ for(int k=0;k<4000;k++)f(c,d);
    double t0=now();uint64_t s=0;
    for(int i=0;i<N;i++){c[(i*7)%(2*NC*Z)]^=(uint8_t)i;f(c,d);s+=d[(i*13)%(46*Z)];}
    double t1=now();sink+=s;double x=(t1-t0)/N*1e9;if(x<r)r=x;} return r;
}
int main(void){
  c=aligned_alloc(64,2*NC*Z*SIMD+256); d=aligned_alloc(64,46*Z+256); cc=aligned_alloc(64,NC*Z+64);
  memset(c,0,2*NC*Z*SIMD+256); srandom(1);
  for(int i=0;i<NC*Z;i++) cc[i]=random()&0xff;
  printf("  %-26s %10s %10s %10s\n","BG1 Zc=384","enc only","prep+enc","prep");
  struct { const char*n; F f; int nc; int ss; } v[] = {
    {"BG1 stock AVX512 permutex", s512, 22, 1},
    {"BG1 stock AVX2 256",        s256, 22, 64},
    {"BG1 factored 512", ldpc384_byte_512, 22, 1},
    {"BG1 factored 256", ldpc384_byte_256, 22, 1},
    {"BG1 factored 128", ldpc384_byte_128, 22, 1},
    {"BG2 stock",         b2s, 10, 32},
    {"BG2 factored 512", ldpc_BG2_Zc384_byte_512, 10, 1},
    {"BG2 factored 256", ldpc_BG2_Zc384_byte_256, 10, 1},
    {"BG2 factored 128", ldpc_BG2_Zc384_byte_128, 10, 1},
  };
  for(unsigned i=0;i<sizeof v/sizeof*v;i++){
    double e=enc_only(v[i].f), pe=run(v[i].f,v[i].nc,v[i].ss);
    printf("  %-26s %10.1f %10.1f %10.1f%s\n",v[i].n,e,pe,pe-e,
       v[i].ss>1 ? "   <- replicated input" : "");
  }
  fprintf(stderr,"%llu\n",(unsigned long long)sink); return 0;}
