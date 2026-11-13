/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdint.h>
#include "cpu_features.h"

/* Checks the instruction sets enabled during compile time. */
uint32_t get_enabled_instructions(void) {

    uint32_t enabled_instructions = 0;

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

    return enabled_instructions;
};
