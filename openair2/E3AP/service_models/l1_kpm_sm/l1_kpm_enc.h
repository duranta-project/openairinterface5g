/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/**
 * @file l1_kpm_enc.h
 * @brief Encoder for the L1-KPM indication payload and RAN-function data.
 *
 * Single entry points for both wire formats: ASN.1 (APER) or JSON is selected
 * at runtime from the config file (E3Configuration.encoding), inside the
 * implementation, so callers carry no encoding awareness. The two forms are field-for-field
 * equivalent; the JSON form matches cuBB's KPM SM so aerial dApps connect
 * unchanged. The ASN.1 schema lives in MESSAGES/ASN1/V1/e3sm_l1_kpm.asn.
 */
#ifndef L1_KPM_ENC_H
#define L1_KPM_ENC_H

#include "e3_ran_buffers.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Encode one slot snapshot as the indication payload into the caller-owned
 * out_buf. Returns the number of bytes written, or -1 on encoder failure,
 * overflow or invalid input.
 */
int l1_kpm_enc_indication(const e3_ran_buffers_slot_info_t *slot, uint8_t *out_buf, size_t out_buf_size);

/**
 * Encode the given RAN-function descriptor (name/version/description) for the E3
 * setup response. Allocates *encoded_data (caller frees). Returns 0 on success,
 * -1 on failure.
 */
int l1_kpm_enc_ran_function_data(const char *name,
                                 int version,
                                 const char *description,
                                 uint8_t **encoded_data,
                                 size_t *encoded_size);

#ifdef __cplusplus
}
#endif

#endif /* L1_KPM_ENC_H */
