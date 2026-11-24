/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef F1AP_LIB_COMMON_H_
#define F1AP_LIB_COMMON_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "openair3/UTILS/conversions.h"
#include "common/5g_platform_types.h"
#include "common/utils/ds/byte_array.h"

#include "F1AP_Cause.h"
#include "F1AP_SNSSAI.h"
#include "f1ap_messages_types.h"
#include "common/utils/eq_check.h"

#define _F1_CHECK_EXP(EXP)                                \
  do {                                                    \
    const bool f1_check_ok_ = (EXP);                      \
    if (!f1_check_ok_) {                                  \
      PRINT_ERROR("%s(): '%s' failed\n", __func__, #EXP); \
      return f1_check_ok_;                                \
    }                                                     \
  } while (0)

/* macro to look up IE. If mandatory and not found, macro will print
 * descriptive debug message to stderr and force exit in calling function */
#define F1AP_LIB_FIND_IE(IE_TYPE, ie, IE_LIST, IE_ID, mandatory)                                     \
  do {                                                                                               \
    ie = NULL;                                                                                       \
    for (IE_TYPE **ptr = (IE_LIST)->array; ptr < &(IE_LIST)->array[(IE_LIST)->count]; ptr++) {       \
      if ((*ptr)->id == IE_ID) {                                                                     \
        ie = *ptr;                                                                                   \
        break;                                                                                       \
      }                                                                                              \
    }                                                                                                \
    if (mandatory && ie == NULL) {                                                                   \
      fprintf(stderr, "%s(): could not find element " #IE_ID " with type " #IE_TYPE "\n", __func__); \
      return false;                                                                                  \
    }                                                                                                \
  } while (0)

#define CP_OPT_BYTE_ARRAY(dst, src)          \
  do {                                       \
    if (src) {                               \
      dst = calloc_or_fail(1, sizeof(*dst)); \
      *(dst) = copy_byte_array(*(src));      \
    }                                        \
  } while (0)

#define FREE_OPT_BYTE_ARRAY(a) \
  do {                         \
    if (a) {                   \
      free_byte_array(*(a));   \
    } \
    free(a);                   \
  } while (0)

/* similar to asn1cCallocOne(), duplicated to not confuse with asn.1 types */
#define _F1_MALLOC(VaR, VaLue)          \
  do {                                  \
    VaR = malloc_or_fail(sizeof(*VaR)); \
    *VaR = VaLue;                       \
  } while (0)

bool eq_f1ap_plmn(const plmn_id_t *a, const plmn_id_t *b);
bool eq_f1ap_cell_info(const f1ap_served_cell_info_t *a, const f1ap_served_cell_info_t *b);
bool eq_f1ap_sys_info(const f1ap_gnb_du_system_info_t *a, const f1ap_gnb_du_system_info_t *b);
bool eq_f1ap_freq_info(const f1ap_nr_frequency_info_t *a, const f1ap_nr_frequency_info_t *b);
bool eq_f1ap_tx_bandwidth(const f1ap_transmission_bandwidth_t *a, const f1ap_transmission_bandwidth_t *b);

struct OCTET_STRING;
uint8_t *cp_octet_string(const struct OCTET_STRING *os, int *len);

F1AP_SNSSAI_t encode_nssai(const nssai_t *nssai);
nssai_t decode_nssai(const F1AP_SNSSAI_t *nssai);

F1AP_Cause_t encode_f1ap_cause(f1ap_Cause_t cause, long cause_value);
bool decode_f1ap_cause(F1AP_Cause_t f1_cause, f1ap_Cause_t *cause, long *cause_value);

#endif /* F1AP_LIB_COMMON_H_ */
