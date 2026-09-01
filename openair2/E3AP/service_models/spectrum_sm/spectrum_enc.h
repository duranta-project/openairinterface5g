/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef SPECTRUM_ENC_H
#define SPECTRUM_ENC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * Spectrum SM Encoding Functions
 *
 * Provides encoding for spectrum indication and control messages
 * with runtime format selection (ASN.1 or JSON, from the config file)
 */

/* Both encoders are compiled in; the active one is selected at runtime from
 * the config file (E3Configuration.encoding, via e3_get_encoding()). */
#include "Spectrum-ConfigControl.h"
#include "Spectrum-RanFunctionData.h"
#include <json-c/json.h>

#include "LAYER2/NR_MAC_gNB/gNB_scheduler_ul_sensing_types.h"

/**
 * Encode a sensing indication (shm-reference form): the ranges live in the
 * /e3_l2_sensing ring, the payload carries only the ring reference (shm name +
 * write index + count) plus timestamp/sfn/slot/beam for correlation. The two
 * wire formats are field-for-field equivalent; the config file selects one
 * per run (E3Configuration.encoding).
 *
 * @param meta Publish metadata (timestamp, sfn, slot, beam)
 * @param write_idx /e3_l2_sensing ring index holding this slot's ranges
 * @param n_ranges Number of ranges at that index (0..128)
 * @param out_buf Caller-owned output buffer
 * @param out_buf_size Size of out_buf
 * @return Bytes written (>0), or -1 on encoder failure/overflow
 */
int spectrum_encode_indication(const nr_mac_sensing_publish_meta_t *meta,
                               uint32_t write_idx,
                               uint8_t n_ranges,
                               uint8_t *out_buf,
                               size_t out_buf_size);

/**
 * Encode Spectrum RAN Function data for setup/registration.
 */
int spectrum_encode_ran_function_data(uint8_t **encoded_data, size_t *encoded_size);

#endif // SPECTRUM_ENC_H
