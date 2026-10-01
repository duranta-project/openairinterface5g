/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef SPECTRUM_DEC_H
#define SPECTRUM_DEC_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/**
 * Spectrum SM control decoding (wire format selected at runtime from the
 * config file): the
 * PRB-block and sensing-policy controls, both carried in the
 * Spectrum-DAppControlData envelope.
 */

/**
 * Decoded PRB-block control.
 *
 * blacklisted_prbs is a heap-allocated array of length prb_count; each entry is an
 * absolute PRB index the dApp wants to block on the UL. The dispatcher turns it
 * into a per-PRB symbol bitmap (0x3FFF per listed PRB) and forwards it into
 * set_prb_block_mask().
 */
typedef struct spectrum_prb_control_s {
  uint16_t *blacklisted_prbs;
  uint32_t prb_count;
  uint32_t sampling_threshold; // 0 if not specified
  uint32_t validity_period; // 0 if not specified
} spectrum_prb_control_t;

/**
 * Decoded sensing policy control.
 *
 * mask_per_slot is a heap-allocated array of length n_slots. Each entry is a
 * 14-bit symbol bitmap (bit s set => "prefer TDAs that don't use symbol s on
 * this slot for the sensing-aware TDA selector"). The dispatcher passes it to
 * set_sensing_policy(), which validates n_slots against the gNB's numb_slots_frame.
 */
typedef struct spectrum_sensing_policy_s {
  uint16_t *mask_per_slot;
  uint32_t n_slots;
  bool deactivate; // when true, clear the policy regardless of mask
  uint32_t validity_period; // 0 if not specified
} spectrum_sensing_policy_t;

/**
 * Decode a PRB-block control from the Spectrum-DAppControlData envelope.
 *
 * @param encoded_data Input encoded data
 * @param encoded_size Size of encoded data
 * @return Allocated spectrum_prb_control_t on success (caller frees via
 *         spectrum_free_decoded_control), NULL on error
 */
spectrum_prb_control_t *spectrum_decode_prb_control(uint8_t *encoded_data, size_t encoded_size);

/**
 * Decode a sensing policy control from the Spectrum-DAppControlData envelope.
 *
 * @param encoded_data Input encoded data
 * @param encoded_size Size of encoded data
 * @return Allocated spectrum_sensing_policy_t on success (caller frees via
 *         spectrum_free_sensing_policy), NULL on error
 */
spectrum_sensing_policy_t *spectrum_decode_sensing_policy(uint8_t *encoded_data, size_t encoded_size);

/**
 * Free a decoded PRB-block control structure.
 *
 * @param prb_control Pointer to the structure to free (NULL tolerated)
 */
void spectrum_free_decoded_control(spectrum_prb_control_t *prb_control);

/**
 * Free a decoded sensing policy structure.
 *
 * @param policy Pointer to the structure to free (NULL tolerated)
 */
void spectrum_free_sensing_policy(spectrum_sensing_policy_t *policy);

#endif // SPECTRUM_DEC_H
