/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdint.h>
#include "cpu_features.h"

/* Checks the instruction sets enabled during compile time. */
uint32_t get_enabled_instructions(void)
{
  uint32_t enabled_instructions = 0;
#if defined(__x86_64__) || defined(__i386__)
#ifdef __AVX2__
  enabled_instructions |= CPU_AVX2;
#endif

#ifdef __FMA__
  enabled_instructions |= CPU_FMA;
#endif

#ifdef __BMI2__
  enabled_instructions |= CPU_BMI2;
#endif

#ifdef __AVX512F__
  enabled_instructions |= CPU_AVX512F;
#endif

#ifdef __AVX512BW__
  enabled_instructions |= CPU_AVX512BW;
#endif

#ifdef __AVX512VL__
  enabled_instructions |= CPU_AVX512VL;
#endif

#ifdef __AVX512DQ__
  enabled_instructions |= CPU_AVX512DQ;
#endif

#elif defined(__aarch64__) || defined(__arm__)
#ifdef __ARM_NEON
  enabled_instructions |= CPU_NEON;
#endif
#ifdef __ARM_FEATURE_SVE
  enabled_instructions |= CPU_SVE;
#endif
#ifdef __ARM_FEATURE_SVE2
  enabled_instructions |= CPU_SVE2;
#endif
#endif

  return enabled_instructions;
};
