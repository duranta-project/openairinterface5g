/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * RVV descrambling (nr_scrambling.c): unscrambling negates LLR[i] iff scramble
 * bit i is set; the scramble sequence is a packed uint32 (little-endian) bit
 * array. This maps directly to RVV `vlm` (load a mask register straight from a
 * bit array) + a masked `vneg` -- fully VLA. The one thing to verify is the bit
 * ordering: vlm element k must equal bit (base+k) of the sequence.
 *
 * Also validates the _init variant (expand the bitseq to a +/-1 int16 array:
 * s[i] = seq_bit(i) ? -1 : +1) and the sequence-multiply form (llr[i]*s[i]).
 *
 * Reference: the scalar #else path of nr_codeword_unscrambling.
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

/* --- RVV kernels under test --- */
/* VLA loop: mask register straight from the packed bit sequence (vlm), then a
 * masked vneg. Plain unit-stride vse16 (not the segment store hit by the
 * spacemit -O2 partial-vl over-store bug); VLMAX (16/64 at VLEN 256/1024) is a
 * multiple of 8 so bits+i/8 stays byte-aligned every full chunk. */
static void unscramble_rvv(int16_t *llr, const uint32_t *seq, int n){
  const uint8_t *bits = (const uint8_t *)seq;
  for (int i = 0; i < n;) {
    size_t vl = __riscv_vsetvl_e16m1(n - i);
    vbool16_t m = __riscv_vlm_v_b16(bits + i / 8, vl);     /* mask elem k = bit i+k */
    vint16m1_t v = __riscv_vle16_v_i16m1(llr + i, vl);
    v = __riscv_vneg_v_i16m1_mu(m, v, v, vl);              /* negate where bit set */
    __riscv_vse16_v_i16m1(llr + i, v, vl);
    i += (int)vl;
  }
}
static void unscramble_init_rvv(int16_t *s, const uint32_t *seq, int n){
  const uint8_t *bits = (const uint8_t *)seq;
  for (int i = 0; i < n;) {
    size_t vl = __riscv_vsetvl_e16m1(n - i);
    vbool16_t m = __riscv_vlm_v_b16(bits + i / 8, vl);
    vint16m1_t ones = __riscv_vmv_v_x_i16m1(1, vl);
    vint16m1_t pm1 = __riscv_vneg_v_i16m1_mu(m, ones, ones, vl);  /* -1 set, +1 else */
    __riscv_vse16_v_i16m1(s + i, pm1, vl);
    i += (int)vl;
  }
}

static uint32_t rng = 987654321u;
static uint32_t rnd(void){ rng = rng*1103515245u + 12345u; return rng; }

/* no-tree-vectorize on the SCAFFOLDING ONLY: spacemit-gcc -O2 auto-vectorizes
 * these plain int16 reference/compare loops and its VLEN=1024 codegen produces a
 * bad partial-vl store that smashes the stack (crash at exit, after results are
 * already correct). The RVV kernels above use intrinsics and are unaffected;
 * proven by rvv-vs-scalar diff=0 at -O1 and -O2 on both VLENs. */
__attribute__((optimize("no-tree-vectorize")))
int main(int argc, char **argv){
  if (argc > 1){ int c=atoi(argv[1]); if(c>7) use_ai(); pin_cpu(c); }
  int fail = 0;
  int sizes[] = { 8, 120, 997, 1003, 8192, 13456 };   /* incl non-8-multiples (tail) */
  for (unsigned t = 0; t < sizeof(sizes)/sizeof(sizes[0]); t++){
    int n = sizes[t];
    int words = (n + 31) / 32;
    uint32_t *seq = calloc(words + 4, sizeof(uint32_t));
    for (int i = 0; i < words; i++) seq[i] = rnd();
    int16_t *llr = malloc(sizeof(int16_t)*n), *ref = malloc(sizeof(int16_t)*n), *got = malloc(sizeof(int16_t)*n);
    int16_t *sref = malloc(sizeof(int16_t)*n), *sgot = malloc(sizeof(int16_t)*n);
    for (int i = 0; i < n; i++) llr[i] = (int16_t)(rnd() >> 16);

    /* scalar reference */
    for (int i = 0; i < n; i++) ref[i] = (seq[i/32] & (1U << (i%32))) ? (int16_t)-llr[i] : llr[i];
    for (int i = 0; i < n; i++) sref[i] = (seq[i/32] & (1U << (i%32))) ? -1 : 1;

    /* rvv */
    memcpy(got, llr, sizeof(int16_t)*n);
    unscramble_rvv(got, seq, n);
    unscramble_init_rvv(sgot, seq, n);

    int d1 = 0, d2 = 0, d3 = 0;
    for (int i = 0; i < n; i++){
      if (got[i]  != ref[i])  d1++;
      if (sgot[i] != sref[i]) d2++;
      if ((int16_t)(llr[i]*sgot[i]) != ref[i]) d3++;   /* seq-multiply form == unscramble */
    }
    printf("  n=%6d: unscramble diff=%d  init diff=%d  mul-form diff=%d  %s\n",
           n, d1, d2, d3, (d1||d2||d3)?"FAIL":"OK");
    if (d1||d2||d3) fail = 1;
    free(seq); free(llr); free(ref); free(got); free(sref); free(sgot);
  }
  printf("rvv unscramble test: %s\n", fail?"FAIL":"PASS");
  return fail;
}
