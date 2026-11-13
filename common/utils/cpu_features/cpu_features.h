/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#ifndef CPU_FEATURES_H
#define CPU_FEATURES_H

#include <stdint.h>

typedef enum {
    CPU_AVX2 = 1 << 0,
    CPU_FMA = 1 << 1,
    CPU_BMI2 = 1 << 2,
    CPU_AVX512F = 1 << 3,
    CPU_AVX512BW = 1 << 4,
    CPU_AVX512VL= 1 << 5,
    CPU_AVX512DQ= 1 << 6,
} INSTRUCTION_SET;

uint32_t get_enabled_instructions(void);

#endif
