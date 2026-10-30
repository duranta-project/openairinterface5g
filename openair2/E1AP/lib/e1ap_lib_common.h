/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef E1AP_LIB_COMMON_H_
#define E1AP_LIB_COMMON_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "openair3/UTILS/conversions.h"
#include "e1ap_messages_types.h"
#include "common/utils/eq_check.h"
#include "E1AP_ProtocolIE-Container.h"

#define CHECK_E1AP_DEC(exp)                                                             \
  do {                                                                                  \
    if (!(exp)) {                                                                       \
        PRINT_ERROR("Failed executing " #exp " in %s() line %d\n", __func__, __LINE__); \
        return false;                                                                   \
    }                                                                                   \
  } while (0)

// Macro to look up IE. If mandatory and not found, macro will log an error and return false.
#define E1AP_LIB_FIND_IE(IE_TYPE, ie, container, IE_ID, mandatory)                                           \
  do {                                                                                                       \
    ie = NULL;                                                                                               \
    for (int i = 0; i < (container)->protocolIEs->list.count; ++i) {                                          \
      IE_TYPE *current_ie = (container)->protocolIEs->list.array[i];                                          \
      if (current_ie->id == (IE_ID)) {                                                                       \
        ie = current_ie;                                                                                     \
        break;                                                                                               \
      }                                                                                                      \
    }                                                                                                        \
    if (mandatory && ie == NULL) {                                                                           \
      fprintf(stderr, "%s(): Mandatory element not found: ID" #IE_ID " with type " #IE_TYPE "\n", __func__); \
      return false;                                                                                          \
    }                                                                                                        \
  } while (0)

/* deep copy of optional E1AP IE */
#define _E1_CP_OPTIONAL_IE(dest, src, field)                  \
  do {                                                        \
    if ((src)->field) {                                       \
      (dest)->field = malloc_or_fail(sizeof(*(dest)->field)); \
      *(dest)->field = *(src)->field;                         \
    }                                                         \
  } while (0)

struct E1AP_Cause;
struct E1AP_Cause e1_encode_cause_ie(const e1ap_cause_t *cause);
e1ap_cause_t e1_decode_cause_ie(const struct E1AP_Cause *ie);

#endif /* E1AP_LIB_COMMON_H_ */
