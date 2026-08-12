/* Per-code-block cycles / instructions / L1I refills for the stock and factored
 * encoders, via perf_event_open directly -- the perf userspace tool is not
 * needed, only CONFIG_PERF_EVENTS and permission to read hardware counters
 * (run as root, or set /proc/sys/kernel/perf_event_paranoid to 1 or less).
 *
 * Covers the 128-bit encoders, which index a doubled input. Build for a target
 * without AVX2 so those are the active variants:
 *
 *   aarch64:  gcc -O3 -march=native      -o icache_probe icache_probe.c \
 *   x86:      gcc -O3 -march=x86-64-v2   -o icache_probe icache_probe.c \
 *               -I<nrLDPC_encoder> -I<oai> -I<oai>/common/utils -I<oai>/openair1
 *   taskset -c <core> ./icache_probe
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>
#include "ldpc384_alignr_byte_128.c"      /* stock:    ldpc384_byte      */
#include "ldpc384_factored_byte_128.c"    /* factored: ldpc384_byte_128  */
#include "ldpc_BG2_Zc384_byte_128.c"      /* stock BG2 (aarch64 / non-AVX2) */
#include "ldpc_BG2_Zc384_factored_byte_128.c"

static int ev_open(uint32_t type, uint64_t config, int group)
{
  struct perf_event_attr a;
  memset(&a, 0, sizeof a);
  a.type = type; a.size = sizeof a; a.config = config;
  a.disabled = (group == -1); a.exclude_kernel = 1; a.exclude_hv = 1;
  int fd = syscall(__NR_perf_event_open, &a, 0, -1, group, 0);
  if (fd < 0) fprintf(stderr, "  (event type=%u config=%llx unavailable: %s)\n",
                      type, (unsigned long long)config, strerror(errno));
  return fd;
}
#define L1I_MISS ((PERF_COUNT_HW_CACHE_L1I) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | \
                  (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))
static uint8_t c[2*22*384*64+256] __attribute__((aligned(64)));
static uint8_t d[46*384+256] __attribute__((aligned(64)));
typedef void (*F)(uint8_t*,uint8_t*);
static void run(const char *tag, F f, int N)
{
  int gc = ev_open(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES, -1);
  int gi = ev_open(PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS, gc);
  int gm = ev_open(PERF_TYPE_HW_CACHE, L1I_MISS, gc);
  for (int i = 0; i < 2000; i++) f(c, d);            /* warm */
  ioctl(gc, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
  ioctl(gc, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
  for (int i = 0; i < N; i++) f(c, d);
  ioctl(gc, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
  uint64_t cy = 0, in = 0, ms = 0;
  if (gc >= 0) { if (read(gc,&cy,8) != 8) cy = 0; }
  if (gi >= 0) { if (read(gi,&in,8) != 8) in = 0; }
  if (gm >= 0) { if (read(gm,&ms,8) != 8) ms = 0; }
  printf("  %-22s %9.0f cyc  %9.0f insn  %7.2f IPC  %8.1f L1I-miss   (per code block)\n",
         tag, (double)cy/N, (double)in/N, in?(double)in/(double)cy:0.0, (double)ms/N);
  if (gc>=0) close(gc); if (gi>=0) close(gi); if (gm>=0) close(gm);
}
int main(void)
{
  srandom(1);
  for (unsigned i = 0; i < sizeof c; i++) c[i] = random() & 0xff;
  const int N = 20000;
  puts("BG1 Zc=384:");
  run("stock (expanded)", ldpc384_byte, N);
  run("factored", ldpc384_byte_128, N);
  puts("BG2 Zc=384:");
  run("stock (expanded)", ldpc_BG2_Zc384_byte, N);
  run("factored", ldpc_BG2_Zc384_byte_128, N);
  return 0;
}
