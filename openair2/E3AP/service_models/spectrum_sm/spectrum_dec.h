/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef SPECTRUM_DEC_H
#define SPECTRUM_DEC_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* Decoded PRB-block control; blacklisted_prbs holds prb_count absolute PRB indices. */
typedef struct spectrum_prb_control_s {
  uint16_t *blacklisted_prbs;
  uint32_t prb_count;
  uint32_t sampling_threshold; // 0 if not specified
  uint32_t validity_period; // 0 if not specified
} spectrum_prb_control_t;

/* Decoded sensing policy; mask_per_slot[i] is a 14-bit symbol bitmap, bit s set = keep symbol s free. */
typedef struct spectrum_sensing_policy_s {
  uint16_t *mask_per_slot;
  uint32_t n_slots;
  bool deactivate; // when true, clear the policy regardless of mask
  uint32_t validity_period; // 0 if not specified
} spectrum_sensing_policy_t;

/** @brief Returns NULL on error; free with spectrum_free_decoded_control(). */
spectrum_prb_control_t *spectrum_decode_prb_control(uint8_t *encoded_data, size_t encoded_size);

/** @brief Returns NULL on error; free with spectrum_free_sensing_policy(). */
spectrum_sensing_policy_t *spectrum_decode_sensing_policy(uint8_t *encoded_data, size_t encoded_size);

void spectrum_free_decoded_control(spectrum_prb_control_t *prb_control);

void spectrum_free_sensing_policy(spectrum_sensing_policy_t *policy);

#endif // SPECTRUM_DEC_H
