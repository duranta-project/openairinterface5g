/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */
#ifndef NR_PRACH_LUT_H
#define NR_PRACH_LUT_H

#include <stdbool.h>
#include <stdint.h>
#include "common/platform_types.h"

/* Only quantities affecting the IDFT body; CP, repetitions and slot placement are separate. */
typedef struct {
  int sequence_length;
  int root_sequence_index;
  int num_root_sequences;
  int restricted_set;
  int ncs;
  int dftlen;
  int frequency_start;
  int amplitude;
} nr_prach_waveform_key_t;

#define NR_PRACH_LUT_MAX_KEYS 16 /* up to eight FD occasions and two formats */
typedef struct {
  unsigned num_keys;
  unsigned num_preambles;
  nr_prach_waveform_key_t keys[NR_PRACH_LUT_MAX_KEYS];
} nr_prach_lut_config_t;

typedef struct nr_prach_lut_s nr_prach_lut_t;
typedef struct {
  const c16_t *samples;
  int32_t power;
} nr_prach_lut_view_t;

#ifdef __cplusplus
extern "C" {
#endif

bool nr_prach_waveform_key_equal(const nr_prach_waveform_key_t *a, const nr_prach_waveform_key_t *b);

/* roots are private to the caller and computed with compute_nr_prach_seq().
 * Returns the waveform power, or -1 for an unsupported transform/root span. */
int32_t nr_prach_generate_waveform(const nr_prach_waveform_key_t *key,
                                   uint8_t preamble_index,
                                   const c16_t roots[64][839],
                                   c16_t *waveform);

/* Creation, configuration and destruction belong to initialization/configuration threads. */
nr_prach_lut_t *nr_prach_lut_create(void);
void nr_prach_lut_configure(nr_prach_lut_t *lut, const nr_prach_lut_config_t *config);
void nr_prach_lut_destroy(nr_prach_lut_t *lut);

/* A successful acquire pins the immutable samples until release. Neither operation waits or allocates. */
bool nr_prach_lut_acquire(nr_prach_lut_t *lut,
                          const nr_prach_waveform_key_t *key,
                          uint8_t preamble_index,
                          nr_prach_lut_view_t *view);
void nr_prach_lut_release(nr_prach_lut_t *lut);

#ifdef __cplusplus
}
#endif
#endif
