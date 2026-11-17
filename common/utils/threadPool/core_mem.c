/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#define _GNU_SOURCE
#include "core_mem.h"
#include <glob.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "common/utils/LOG/log.h"

static __thread void *core_mem_ptr;
static __thread size_t core_mem_size;

#if defined(__riscv) && defined(__linux__)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/sysinfo.h>
#include <unistd.h>

// SpacemiT K3: /dev/tcm maps 8 blocks of 384 KB back to back, two clusters of four (device tree soc/tcm@0, compatible
// spacemit,k1-tcm). Block k is near A100 core 8 + k (about 10 ns, 45 GB/s); every other core, X100 ones included, sees
// it at about 66 ns and 0.4 GB/s, so only core 8 + k gets block k.
#define K3_TCM_BLOCK 0x60000
#define K3_TCM_FIRST_CORE 8
#define K3_TCM_BLOCKS 8

static atomic_bool k3_tcm_claimed[K3_TCM_BLOCKS];
static pthread_once_t k3_tcm_once = PTHREAD_ONCE_INIT;
static int k3_tcm_present;

static void k3_tcm_probe(void)
{
  glob_t g;
  if (get_nprocs_conf() != K3_TCM_FIRST_CORE + K3_TCM_BLOCKS
      || glob("/sys/firmware/devicetree/base/soc/tcm@*/compatible", 0, NULL, &g) != 0)
    return;
  FILE *f = fopen(g.gl_pathv[0], "r");
  char compat[64] = {0};
  if (f) {
    k3_tcm_present = fread(compat, 1, sizeof(compat) - 1, f) > 0 && strcmp(compat, "spacemit,k1-tcm") == 0
                     && access("/dev/tcm", R_OK | W_OK) == 0;
    fclose(f);
  }
  globfree(&g);
}

static void *k3_tcm_map(int core, size_t *size)
{
  const int blk = core - K3_TCM_FIRST_CORE;
  pthread_once(&k3_tcm_once, k3_tcm_probe);
  if (blk < 0 || blk >= K3_TCM_BLOCKS || !k3_tcm_present)
    return NULL;
  if (atomic_exchange(&k3_tcm_claimed[blk], true))
    return NULL;
  const int fd = open("/dev/tcm", O_RDWR);
  void *m = fd < 0 ? MAP_FAILED : mmap(NULL, K3_TCM_BLOCK, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)blk * K3_TCM_BLOCK);
  if (fd >= 0)
    close(fd);
  if (m == MAP_FAILED) {
    atomic_store(&k3_tcm_claimed[blk], false);
    return NULL;
  }
  *size = K3_TCM_BLOCK;
  return m;
}
#endif

size_t core_mem_claim(void)
{
  if (core_mem_ptr)
    return core_mem_size;
  cpu_set_t set;
  if (pthread_getaffinity_np(pthread_self(), sizeof(set), &set) != 0 || CPU_COUNT(&set) != 1)
    return 0;
  int core = 0;
  while (!CPU_ISSET(core, &set))
    core++;
#if defined(__riscv) && defined(__linux__)
  core_mem_ptr = k3_tcm_map(core, &core_mem_size);
#endif
  if (core_mem_ptr)
    LOG_I(UTIL, "core %d: %zu KB of core-local memory (TCM) for this thread\n", core, core_mem_size >> 10);
  return core_mem_ptr ? core_mem_size : 0;
}

void *core_mem_get(size_t *size)
{
  if (size)
    *size = core_mem_size;
  return core_mem_ptr;
}
