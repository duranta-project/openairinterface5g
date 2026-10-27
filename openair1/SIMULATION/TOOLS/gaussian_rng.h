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

/* Each concurrent owner must use its own state. Copying duplicates the stream
 * position. Double and float outputs have distinct sequences; request partitioning
 * within a precision does not change its sequence. Mixed calls share PRNG state
 * and keep each precision's pending values. Reproducibility assumes the same
 * floating-point environment and libm. This stream differs from the legacy RNG. */
typedef struct {
  uint64_t words[4][4], repair[4];
  double pending_double[4];
  float pending_float[8];
  unsigned cursor_double, cursor_float;
} gaussian_rng_t;

/* Zero is a valid seed. Shared tables are initialized safely on first use. */
void gaussian_rng_init(uint64_t seed, gaussian_rng_t *state);
/* Standard-normal samples. Zero count permits a null output and leaves state untouched. */
double gaussian_rng_sample(gaussian_rng_t *state);
void gaussian_rng_fill(gaussian_rng_t *state, size_t count, double *out);
void gaussian_rng_fill_float(gaussian_rng_t *state, size_t count, float *out);
/* Compatibility instance, seeded by randominit(). Not safe for concurrent access. */
gaussian_rng_t *gaussian_rng_default(void);
#ifdef __cplusplus
}
#endif
#endif
