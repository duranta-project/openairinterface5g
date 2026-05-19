/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_UP_H
#define NR_UP_H

#include "common/platform_types.h"
#include "common/utils/ds/byte_array.h"

/** Result of deliver_drb backend (RLC enqueue or F1-U send) propagated to PDCP. */
typedef enum nr_up_dl_transfer_result_e {
  NR_UP_DL_OK = 0,
  NR_UP_DL_ERROR,
} nr_up_dl_transfer_result_t;

/** PDCP to nr-up DL DRB transfer request. */
typedef struct nr_up_dl_transfer_req_s {
  ue_id_t ue_id;
  uint8_t drb_id;
  int sdu_id;
  byte_array_t pdu;
} nr_up_dl_transfer_req_t;

typedef nr_up_dl_transfer_result_t (*nr_up_deliver_drb_fn_t)(const nr_up_dl_transfer_req_t *req);

typedef struct nr_up_if_s {
  nr_up_deliver_drb_fn_t deliver_drb;
} nr_up_if_t;

struct nr_up_if_s *get_nr_up_if(void);

#endif /* NR_UP_H */
