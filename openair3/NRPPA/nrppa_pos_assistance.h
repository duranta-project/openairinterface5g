/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#ifndef NRPPA_POS_ASSISTANCE_H
#define NRPPA_POS_ASSISTANCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* TS 38.455 sections 9.2.19 and 9.2.20: one PosSIB and one segment. */
bool nrppa_encode_pos_assistance(const uint8_t *segment, size_t segment_len, uint32_t period_frames,
                                 uint8_t **encoded, size_t *encoded_len);
bool nrppa_decode_pos_assistance(const uint8_t *encoded, size_t encoded_len, uint8_t **segment,
                                 size_t *segment_len, uint32_t *period_frames);

#endif
