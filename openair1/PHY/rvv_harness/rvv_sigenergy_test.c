/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * RVV signal_energy_nodc (signal_energy.c): sum_k(|c_k|^2)/length, the NR gNB-RX
 * energy measure (PUSCH per-symbol energy, SRS noise power, gNB measurements).
 *
 * The SIMDe reference accumulates |c|^2 in FLOAT across 4 lanes (madd_epi16 ->
 * cvtepi32_ps -> add_ps), which (a) loses precision once the running sum exceeds
 * 2^24 and (b) would be VLEN-DEPENDENT if replicated in a VLA float reduction
 * (different lane count -> different rounding on cpu2 vs cpu8). So the RVV port
 * accumulates |c|^2 in int64 (exact, associative, VLEN-independent) and divides
 * once at the end -- strictly MORE accurate than the float reference and
 * identical on every VLEN. This is a power measurement feeding dB_fixed/SNR/AGC,
 * so the sub-0.01% deviation from the float path is immaterial.
 *
 * This test reports the RVV int result, the float reference, and their relative
 * error (expected ~1e-4..1e-3 from the float path's precision loss, NOT a bug),
 * and confirms the RVV result is bit-identical across VLEN (determinism).
 *
 * LAUNCH: main() switches the core FIRST, then run_tests() (noinline) does all
 * vector work -- so its frame is sized at the post-switch VLEN (A100 = 1024).
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sched.h>
#include <unistd.h>
#include <riscv_vector.h>

typedef struct { int16_t r, i; } c16_t;

static int pin_cpu(int c){ cpu_set_t s; CPU_ZERO(&s); CPU_SET(c,&s); return sched_setaffinity(0,sizeof(s),&s); }
static int use_ai(void){ FILE*f=fopen("/proc/set_ai_thread","w"); if(!f)return -1; fprintf(f,"%ld\n",(long)getpid()); return fclose(f); }

/* faithful scalar model of the current RISC-V simde float path (4-lane accumulate) */
static uint32_t nodc_ref_float(const c16_t *in, uint32_t length){
  float lane[4] = {0,0,0,0};
  uint32_t nb = length >> 2;
  for (uint32_t i = 0; i < nb; i++)
    for (int k = 0; k < 4; k++){
      const c16_t c = in[4*i + k];
      lane[k] += (float)((int32_t)c.r*c.r + (int32_t)c.i*c.i);
    }
  float leftover = 0;
  for (uint32_t i = nb*4; i < length; i++)
    leftover += (float)((int32_t)in[i].r*in[i].r + (int32_t)in[i].i*in[i].i);
  return (uint32_t)((lane[0]+lane[1]+lane[2]+lane[3]+leftover) / (float)length);
}
/* exact integer reference (what the RVV computes, order-independent) */
static uint32_t nodc_ref_int(const c16_t *in, uint32_t length){
  int64_t acc = 0;
  for (uint32_t i = 0; i < length; i++)
    acc += (int64_t)in[i].r*in[i].r + (int64_t)in[i].i*in[i].i;
  return (uint32_t)((double)acc / (double)length);
}

/* RVV kernel under test (== the signal_energy.c __riscv branch) */
static uint32_t nodc_rvv(const c16_t *in, uint32_t length){
  const int16_t *p = (const int16_t *)in;
  int64_t acc = 0;
  for (uint32_t n = 0; n < length;){
    size_t vl = __riscv_vsetvl_e16mf2(length - n);
    vint16mf2x2_t v = __riscv_vlseg2e16_v_i16mf2x2(p + 2*n, vl);
    vint16mf2_t cr = __riscv_vget_v_i16mf2x2_i16mf2(v, 0), ci = __riscv_vget_v_i16mf2x2_i16mf2(v, 1);
    vint32m1_t pw = __riscv_vwmacc_vv_i32m1(__riscv_vwmul_vv_i32m1(cr, cr, vl), ci, ci, vl);
    acc += __riscv_vmv_x_s_i64m1_i64(__riscv_vwredsum_vs_i32m1_i64m1(pw, __riscv_vmv_s_x_i64m1(0, 1), vl));
    n += (uint32_t)vl;
  }
  return (uint32_t)((double)acc / (double)length);
}

static uint32_t rng = 0xBEEF77u;
static int16_t rnd_amp(int amp){ rng = rng*1103515245u+12345u; return (int16_t)(((int32_t)(rng>>16) - 32768) * amp / 32768); }

__attribute__((noinline)) static int run_tests(void){
  int lens[] = { 6, 12, 273*12, 100, 99, 4 };
  int amps[] = { 30000, 3000, 300 };     /* channel-estimate magnitudes vary */
  int fail = 0;
  for (unsigned a = 0; a < sizeof(amps)/sizeof(amps[0]); a++){
    for (unsigned t = 0; t < sizeof(lens)/sizeof(lens[0]); t++){
      int len = lens[t];
      c16_t *in = malloc(sizeof(c16_t)*len);
      for (int i = 0; i < len; i++){ in[i].r = rnd_amp(amps[a]); in[i].i = rnd_amp(amps[a]); }
      uint32_t rf = nodc_ref_float(in, len), ri = nodc_ref_int(in, len), rv = nodc_rvv(in, len);
      double rel = rf ? fabs((double)rv - rf) / rf : 0.0;
      int int_exact = (rv == ri);           /* RVV must exactly equal the integer ref */
      printf("  amp=%5d len=%5d: rvv=%u int_ref=%u float_ref=%u  rel_vs_float=%.2e  %s\n",
             amps[a], len, rv, ri, rf, rel, int_exact ? "OK" : "INT-MISMATCH");
      if (!int_exact || rel > 5e-3) fail = 1;
      free(in);
    }
  }
  return fail;
}

int main(int argc, char **argv){
  if (argc > 1){ int c=atoi(argv[1]); if(c>7) use_ai(); pin_cpu(c); }
  printf("VLEN=%zu\n", (size_t)__riscv_vlenb()*8);
  int fail = run_tests();
  printf("rvv signal_energy_nodc test: %s\n", fail?"FAIL":"PASS");
  return fail;
}
