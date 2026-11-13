/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* This module ensures that the runtime host has support
 * for the instruction sets enabled during compile time.
 *
 * PANICS:
 * - If the runtime host does not have support for enabled instruction sets.
 * - If the runtime host does not support `AVX2` instruction set.
 *
 */
#include <stdio.h>
#include <stdint.h>
#include "common/utils/assertions.h"
#include "cpu_features.h"

#if defined(__aarch64__)
#include <sys/auxv.h>
#include <asm/hwcap.h>
#endif

typedef struct {
  uint32_t bit;
  const char *name;
} instructions;

/*
 * This macro checks if a given instruction set `feat` is available to the host
 * system.
 */
#if defined(__x86_64) || defined(__i386__)
#define CHECK_CPU_INSTR(feat, flag, mask) \
  {                                       \
    if (__builtin_cpu_supports(feat)) {   \
      mask |= flag;                       \
    }                                     \
  }
#elif defined(__aarch64__) || defined(__arm__)
#define CHECK_CPU_INSTR(hwcap, feat, flag, mask) \
  {                                              \
    if (hwcap & feat) {                          \
      mask |= flag;                              \
    }                                            \
  }
#endif

/* Checks the instruction sets available to the host machine. */
static uint32_t get_available_instructions(void)
{
  uint32_t available_instructions = 0;
#if defined(__x86_64__) || defined(__i386__)
  CHECK_CPU_INSTR("avx2", CPU_AVX2, available_instructions);
  CHECK_CPU_INSTR("fma", CPU_FMA, available_instructions);
  CHECK_CPU_INSTR("bmi2", CPU_BMI2, available_instructions);
  CHECK_CPU_INSTR("avx512bw", CPU_AVX512BW, available_instructions);
  CHECK_CPU_INSTR("avx512dq", CPU_AVX512DQ, available_instructions);
  CHECK_CPU_INSTR("avx512f", CPU_AVX512F, available_instructions);
  CHECK_CPU_INSTR("avx512vl", CPU_AVX512VL, available_instructions);

#elif defined(__aarch64__) || defined(__arm__)
  unsigned long hwcap = getauxval(AT_HWCAP);
  CHECK_CPU_INSTR(hwcap, HWCAP_ASIMD, CPU_NEON, available_instructions);
  CHECK_CPU_INSTR(hwcap, HWCAP_SVE, CPU_SVE, available_instructions);
  CHECK_CPU_INSTR(getauxval(AT_HWCAP2), HWCAP2_SVE2, CPU_SVE2, available_instructions);

#else
  available_instructions = 0;
#endif
  return available_instructions;
}

static const instructions instruction_set[] = {
    {CPU_AVX2, "AVX2"},
    {CPU_FMA, "FMA"},
    {CPU_BMI2, "BMI2"},
    {CPU_AVX512F, "AVX512F"},
    {CPU_AVX512BW, "AVX512BW"},
    {CPU_AVX512VL, "AVX512VL"},
    {CPU_AVX512DQ, "AVX512DQ"},
    {CPU_NEON, "NEON"},
    {CPU_SVE, "SVE"},
    {CPU_SVE2, "SVE2"},
};

static void print_instruction_set(uint32_t features)
{
  for (size_t i = 0; i < sizeof(instruction_set) / sizeof(instruction_set[0]); i++) {
    if (features & instruction_set[i].bit)
      printf("  %s\n", instruction_set[i].name);
  }
}

static void check_instruction_sets(uint32_t enabled_instructions, uint32_t available_instructions)
{
/* PANIC: `AVX2` is not found. */
#if defined(__x86_64__) || defined(__i386__)
  AssertFatal(__builtin_cpu_supports("avx2"), "The Host does not support `AVX2`.\n");
#endif
  /* PANIC: Host machine does not support all enabled instructions */
  uint32_t missing = enabled_instructions & ~available_instructions;
  if (missing) {
    puts("PANIC: The binary enabled the following instruction sets however, the CPU does not support them.");
    print_instruction_set(missing);
    AssertFatal(missing == 0, "The binary enabled instruction sets that the CPU does not support.\n");
  };
}

__attribute__((constructor(101))) static void cpu_features_check(void)
{
#if defined(__x86_64__) || defined(__i386__)
  __builtin_cpu_init();
#endif
  check_instruction_sets(get_enabled_instructions(), get_available_instructions());
}
