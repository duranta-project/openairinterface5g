/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * Does __riscv_vsse8_v_u8m1 with a partial vl (< VLMAX) honor vl, or does the
 * spacemit-gcc -O2 codegen over-store VLMAX elements? This is the one strided
 * store in the LDPC input interleaver (ldpc_encoder_optim8segmulti.c:51,93,108);
 * an over-store would write past the intended range into neighboring memory,
 * which the end-to-end sims would not necessarily catch.
 *
 * Method: fill a buffer with a sentinel (0xAA), do a stride-8 vsse8 of 0x55 with
 * vl = VLMAX/2 (partial), then verify EXACTLY the vl intended slots changed and
 * every slot at stride positions k >= vl (up to VLMAX) is still the sentinel.
 * Also exercise a small-tail vl (the encoder's realistic case).
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

/* not inlined: mimic a real call site so the compiler must honor the vl operand */
__attribute__((noinline))
static void do_vsse8(uint8_t *base, ptrdiff_t stride, uint8_t val, size_t vl){
  __riscv_vsse8_v_u8m1(base, stride, __riscv_vmv_v_x_u8m1(val, vl), vl);
}

static int check(size_t vl, const char *tag){
  size_t vlmax = __riscv_vsetvlmax_e8m1();
  const size_t STR = 8, BASE = 64;
  size_t n = BASE + STR * (vlmax + 4) + 64;      /* room for a full VLMAX over-store + slack */
  uint8_t *buf = malloc(n);
  memset(buf, 0xAA, n);
  do_vsse8(buf + BASE, (ptrdiff_t)STR, 0x55, vl);
  int overstore = 0, missing = 0, stray = 0;
  for (size_t i = 0; i < n; i++){
    int is_target = (i >= BASE) && ((i - BASE) % STR == 0) && ((i - BASE) / STR < vl);
    int is_overpos = (i >= BASE) && ((i - BASE) % STR == 0) && ((i - BASE) / STR >= vl);
    if (is_target && buf[i] != 0x55) missing++;
    else if (is_overpos && buf[i] != 0xAA) overstore++;   /* wrote a stride slot beyond vl */
    else if (!is_target && !is_overpos && buf[i] != 0xAA) stray++; /* wrote a non-stride byte */
  }
  printf("  %-10s vl=%3zu (VLMAX=%3zu): %s  (missing=%d overstore=%d stray=%d)\n",
         tag, vl, vlmax, (missing||overstore||stray)?"FAIL":"OK", missing, overstore, stray);
  free(buf);
  return (missing||overstore||stray);
}

int main(int argc, char **argv){
  if (argc > 1){ int cpu=atoi(argv[1]); if(cpu>7) use_ai(); pin_cpu(cpu); }
  size_t vlmax = __riscv_vsetvlmax_e8m1();
  int fail = 0;
  fail |= check(vlmax/2, "half");                 /* clearly partial */
  fail |= check(vlmax>3?vlmax-1:1, "vlmax-1");     /* near-full partial */
  fail |= check(3, "tail3");                       /* small tail like the encoder */
  fail |= check(vlmax, "full");                    /* full (control) */
  printf("vsse8 partial-vl over-store test: %s\n", fail?"FAIL":"PASS");
  return fail?1:0;
}
