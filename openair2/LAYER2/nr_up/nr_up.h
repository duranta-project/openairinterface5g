/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_UP_H
#define NR_UP_H

#include "common/platform_types.h"
#include "common/utils/ds/byte_array.h"

/** PDCP to nr-up DL user data request. */
typedef struct nr_up_dl_user_data_req_s {
  ue_id_t ue_id;
  uint8_t drb_id;
  int sdu_id;
  byte_array_t pdu;
} nr_up_user_data_req_t;

typedef void (*nr_up_user_data_fn_t)(const nr_up_user_data_req_t *req);

typedef struct nr_up_if_s {
  nr_up_user_data_fn_t up_user_data;
} nr_up_if_t;

struct nr_up_if_s *get_nr_up_if(void);

#endif /* NR_UP_H */
