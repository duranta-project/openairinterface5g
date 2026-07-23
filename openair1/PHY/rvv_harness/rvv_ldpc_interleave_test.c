/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * RVV nr_interleaving_ldpc (nr_rate_matching.c): the LDPC/modulation Qm-way byte
 * interleave. Input e is Qm contiguous streams e0..e{Qm-1} of EQm=E/Qm bytes;
 * output f[Qm*i+k] = e_k[i]. On RISC-V this runs the plain scalar tail today
 * (USE128BIT/AVX512 are x86-only). The interleave IS an RVV segment store:
 * vle8 each stream (unit stride) + vsseg{Qm}e8 writes the interleaved output.
 *
 * Full-VLMAX body + scalar tail: keeps every segment store at vl==VLMAX (no
 * partial-vl vsseg over-store), matching the mitigation used elsewhere.
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <unistd.h>
#include <riscv_vector.h>

static int pin_cpu(int c){ cpu_set_t s; CPU_ZERO(&s); CPU_SET(c,&s); return sched_setaffinity(0,sizeof(s),&s); }
static int use_ai(void){ FILE*f=fopen("/proc/set_ai_thread","w"); if(!f)return -1; fprintf(f,"%ld\n",(long)getpid()); return fclose(f); }

/* scalar reference == the OAI scalar tail */
static void il_ref(uint32_t E, uint8_t Qm, const uint8_t *e, uint8_t *f){
  uint32_t EQm = E / Qm;
  memset(f, 0, E);
  for (uint32_t i = 0; i < EQm; i++)
    for (int k = 0; k < Qm; k++)
      f[Qm*i + k] = e[k*EQm + i];
}

/* RVV: full-vl vle8 x Qm + vsseg{Qm}e8, scalar tail */
static void il_rvv(uint32_t E, uint8_t Qm, const uint8_t *e, uint8_t *f){
  uint32_t EQm = E / Qm;
  memset(f, 0, E);
  const uint8_t *ek[8];
  for (int k = 0; k < Qm; k++) ek[k] = e + (uint32_t)k*EQm;
  size_t vlmax = __riscv_vsetvlmax_e8m1();
  uint32_t i = 0;
  for (; i + vlmax <= EQm; i += vlmax) {
    uint8_t *o = f + (size_t)Qm*i;
    if (Qm == 2) {
      vuint8m1_t v0=__riscv_vle8_v_u8m1(ek[0]+i,vlmax), v1=__riscv_vle8_v_u8m1(ek[1]+i,vlmax);
      __riscv_vsseg2e8_v_u8m1x2(o, __riscv_vcreate_v_u8m1x2(v0,v1), vlmax);
    } else if (Qm == 4) {
      vuint8m1_t v0=__riscv_vle8_v_u8m1(ek[0]+i,vlmax), v1=__riscv_vle8_v_u8m1(ek[1]+i,vlmax);
      vuint8m1_t v2=__riscv_vle8_v_u8m1(ek[2]+i,vlmax), v3=__riscv_vle8_v_u8m1(ek[3]+i,vlmax);
      __riscv_vsseg4e8_v_u8m1x4(o, __riscv_vcreate_v_u8m1x4(v0,v1,v2,v3), vlmax);
    } else if (Qm == 6) {
      vuint8m1_t v0=__riscv_vle8_v_u8m1(ek[0]+i,vlmax), v1=__riscv_vle8_v_u8m1(ek[1]+i,vlmax);
      vuint8m1_t v2=__riscv_vle8_v_u8m1(ek[2]+i,vlmax), v3=__riscv_vle8_v_u8m1(ek[3]+i,vlmax);
      vuint8m1_t v4=__riscv_vle8_v_u8m1(ek[4]+i,vlmax), v5=__riscv_vle8_v_u8m1(ek[5]+i,vlmax);
      __riscv_vsseg6e8_v_u8m1x6(o, __riscv_vcreate_v_u8m1x6(v0,v1,v2,v3,v4,v5), vlmax);
    } else { /* Qm == 8 */
      vuint8m1_t v0=__riscv_vle8_v_u8m1(ek[0]+i,vlmax), v1=__riscv_vle8_v_u8m1(ek[1]+i,vlmax);
      vuint8m1_t v2=__riscv_vle8_v_u8m1(ek[2]+i,vlmax), v3=__riscv_vle8_v_u8m1(ek[3]+i,vlmax);
      vuint8m1_t v4=__riscv_vle8_v_u8m1(ek[4]+i,vlmax), v5=__riscv_vle8_v_u8m1(ek[5]+i,vlmax);
      vuint8m1_t v6=__riscv_vle8_v_u8m1(ek[6]+i,vlmax), v7=__riscv_vle8_v_u8m1(ek[7]+i,vlmax);
      __riscv_vsseg8e8_v_u8m1x8(o, __riscv_vcreate_v_u8m1x8(v0,v1,v2,v3,v4,v5,v6,v7), vlmax);
    }
  }
  for (; i < EQm; i++)
    for (int k = 0; k < Qm; k++)
      f[(size_t)Qm*i + k] = ek[k][i];
}

static uint32_t rng = 0x1234567u;
static uint8_t rnd8(void){ rng = rng*1103515245u+12345u; return (uint8_t)(rng >> 16); }

int main(int argc, char **argv){
  if (argc > 1){ int c=atoi(argv[1]); if(c>7) use_ai(); pin_cpu(c); }
  int qms[] = {2,4,6,8};
  int Es[]  = {3276*2, 8496, 100, 66};   /* various; incl not-multiple-of-VLMAX and odd tails */
  int fail = 0;
  for (unsigned q=0;q<4;q++){
    int Qm = qms[q];
    for (unsigned t=0;t<4;t++){
      int E = (Es[t] / Qm) * Qm;   /* E must be a multiple of Qm */
      if (E < Qm) continue;
      uint8_t *e = malloc(E), *fr = malloc(E), *fg = malloc(E);
      for (int i=0;i<E;i++) e[i] = rnd8();
      il_ref(E, Qm, e, fr);
      il_rvv(E, Qm, e, fg);
      int d = memcmp(fr, fg, E);
      printf("  Qm=%d E=%6d: diff=%d %s\n", Qm, E, d, d?"FAIL":"OK");
      if (d) fail = 1;
      free(e); free(fr); free(fg);
    }
  }
  printf("rvv ldpc-interleave test: %s\n", fail?"FAIL":"PASS");
  return fail;
}
