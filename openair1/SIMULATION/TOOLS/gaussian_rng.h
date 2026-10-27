/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef OAI_GAUSSIAN_RNG_H
#define OAI_GAUSSIAN_RNG_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize before use. Concurrent generators must own different state objects.
 * Copying a state copies its position in the stream; distinct seeds do not promise
 * statistically independent streams. */
typedef struct {
  uint32_t jsr;
} gaussian_rng_t;

/* Seed zero maps to 123456789, since the XOR/shift recurrence cannot escape zero. */
void gaussian_rng_init(uint32_t seed, gaussian_rng_t *state);

/* Standard normal sample, with the legacy gaussZiggurat stream for nonzero seeds. */
double gaussian_rng_sample(gaussian_rng_t *state);

/* Equivalent to count scalar draws, independent of batch boundaries. Output may
 * be unaligned to SIMD boundaries. A zero count permits a null output pointer. */
void gaussian_rng_fill(gaussian_rng_t *state, size_t count, double *out);

#ifdef __cplusplus
}
#endif

#endif
