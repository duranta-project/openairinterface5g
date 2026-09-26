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
#include "E3SM_Spectrum-ConfigControl.h"
#include "E3SM_Spectrum-RanFunctionData.h"
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

/* What applying a PRB-block control did. All instants are CLOCK_REALTIME
 * nanoseconds, on the same clock as the dApp's report timestamp so the xApp can
 * difference them; a zero means "did not happen", not "happened at zero", and
 * is encoded as an absent field. */
typedef struct {
  int64_t built_ts_ns; /* when this outcome was assembled */
  int64_t installed_ts_ns; /* mask written into the MAC */
  int64_t on_air_ts_ns; /* first scheduler tick that put it on air */
  uint16_t sfn;
  uint16_t slot;
} spectrum_apply_outcome_t;

/**
 * @brief Encode the outcome of applying a PRB-block control.
 * @param outcome The instants to report; a zero member is encoded as absent.
 * @param encoded_data Set to a malloc'd buffer the caller must free.
 * @param encoded_size Set to the encoded length in bytes.
 * @return E3_SUCCESS, or an E3_* error code.
 */
int spectrum_encode_apply_outcome(const spectrum_apply_outcome_t *outcome, uint8_t **encoded_data, size_t *encoded_size);

#endif // SPECTRUM_ENC_H
