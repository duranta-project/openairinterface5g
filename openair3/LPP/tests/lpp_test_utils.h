/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef LPP_TEST_UTILS_H
#define LPP_TEST_UTILS_H

#include <stdbool.h>
#include "LPP_LPP-Message.h"
#include "common/utils/assertions.h"
#include "common/utils/utils.h"

enum lpp_codec {
  LPP_CODEC_APER,
  LPP_CODEC_UPER,
};

LPP_LPP_Message_t *encode_decode_lpp_message(const LPP_LPP_Message_t *message, enum lpp_codec codec);
bool eq_lpp_message(const LPP_LPP_Message_t *a, const LPP_LPP_Message_t *b);
void free_lpp_message(LPP_LPP_Message_t *message);

#endif
