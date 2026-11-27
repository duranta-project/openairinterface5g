/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdint.h>
#include "cpu_features.h"

/* Checks the instruction sets enabled during compile time. */
uint32_t get_enabled_instructions(void)
{
  uint32_t enabled_instructions = 0;
#if (defined(__x86_64__) || defined(__i386__)) && defined(__AVX2__)
  enabled_instructions |= CPU_AVX2;
#endif
#if (defined(__x86_64__) || defined(__i386__)) && defined(__FMA__)
  enabled_instructions |= CPU_FMA;
#endif
#if (defined(__x86_64__) || defined(__i386__)) && defined(__BMI2__)
  enabled_instructions |= CPU_BMI2;
#endif
#if (defined(__x86_64__) || defined(__i386__)) && defined(__AVX512F__)
  enabled_instructions |= CPU_AVX512F;
#endif
#if (defined(__x86_64__) || defined(__i386__)) && defined(__AVX512BW__)
  enabled_instructions |= CPU_AVX512BW;
#endif
#if (defined(__x86_64__) || defined(__i386__)) && defined(__AVX512VL__)
  enabled_instructions |= CPU_AVX512VL;
#endif
#if (defined(__x86_64__) || defined(__i386__)) && defined(__AVX512DQ__)
  enabled_instructions |= CPU_AVX512DQ;
#endif

#if (defined(__aarch64__) || defined(__arm__)) && defined(__ARM_NEON)
  enabled_instructions |= CPU_NEON;
#endif
#if (defined(__aarch64__) || defined(__arm__)) && defined(__ARM_FEATURE_SVE)
  enabled_instructions |= CPU_SVE;
#endif
#if (defined(__aarch64__) || defined(__arm__)) && defined(__ARM_FEATURE_SVE2)
  enabled_instructions |= CPU_SVE2;
#endif
  return enabled_instructions;
};
