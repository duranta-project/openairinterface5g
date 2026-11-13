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
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "cpu_features.h"

typedef struct {
    uint32_t bit;
    const char *name;
} instructions;

/* Checks the instruction sets available to the host machine. */
static uint32_t get_available_instructions(void) {
    uint32_t available_instructions = 0;
    #if defined(__x86_64__) || defined(__i386__)
        if (__builtin_cpu_supports("avx2")) {
            available_instructions |= CPU_AVX2;
        }
        if (__builtin_cpu_supports("fma")) {
            available_instructions |= CPU_FMA;
        }
        if (__builtin_cpu_supports("bmi2")) {
            available_instructions |= CPU_BMI2;
        }
        if (__builtin_cpu_supports("avx512f")) {
            available_instructions |= CPU_AVX512F;
        }
        if (__builtin_cpu_supports("avx512bw")) {
            available_instructions |= CPU_AVX512BW;
        }
        if (__builtin_cpu_supports("avx512vl")) {
            available_instructions |= CPU_AVX512VL;
        }
        if (__builtin_cpu_supports("avx512dq")) {
            available_instructions |= CPU_AVX512DQ;
        }
    #else
        available_instructions = 0;
    #endif
    return available_instructions;
}

static const instructions instruction_set[] = {
    { CPU_AVX2,     "AVX2" },
    { CPU_FMA,      "FMA" },
    { CPU_BMI2,     "BMI2" },
    { CPU_AVX512F,  "AVX512F" },
    { CPU_AVX512BW, "AVX512BW" },
    { CPU_AVX512VL, "AVX512VL" },
    { CPU_AVX512DQ, "AVX512DQ" },
};


static void print_instruction_set(uint32_t features)
{
    for (size_t i = 0; i < sizeof(instruction_set) / sizeof(instruction_set[0]); i++) {
        if (features & instruction_set[i].bit)
            printf("  %s\n", instruction_set[i].name);
    }
}

static void check_instruction_sets(uint32_t enabled_instructions, uint32_t available_instructions) {
    /* PANIC: `AVX2` is not found. */
    #if defined(__x86_64__) || defined(__i386__)
        if (!__builtin_cpu_supports("avx2")) {
            puts("PANIC: The Host does not support `AVX2`.");
            abort();
        }
    #endif
    /* PANIC: Host machine does not support all enabled instructions */
    uint32_t missing = enabled_instructions & ~available_instructions;
    if (missing) {
        puts("PANIC: The binary enabled the following instruction sets however, the CPU does not support them.");
        print_instruction_set(missing);
        abort();
    };
}


__attribute__((constructor(101))) static void cpu_features_check(void)
{
  __builtin_cpu_init();
  check_instruction_sets(get_enabled_instructions(), get_available_instructions());
}
