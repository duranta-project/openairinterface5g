/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_UP_H
#define NR_UP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "common/platform_constants.h"
#include "common/platform_types.h"
#include "common/utils/ds/byte_array.h"

/** PDCP to nr-up DL user data request. */
typedef struct nr_up_transfer_user_data_req_s {
  ue_id_t ue_id;
  uint8_t drb_id;
  int sdu_id;
  uint32_t pdcp_sn; /** PDCP SN for F1-U Report Delivered */
  byte_array_t pdu;
  bool report_polling;
} nr_up_transfer_user_data_req_t;

typedef enum nr_up_transfer_user_data_action_e {
  NR_UP_TRANSFER_ALLOW,
  NR_UP_TRANSFER_DROP,
} nr_up_transfer_user_data_action_t;

/** Request to send DDS */
typedef struct nr_up_data_delivery_status_req_s {
  ue_id_t ue_id;
  rb_id_t drb_id;
  uint32_t desired_buffer_size;
  bool report_dds;
  byte_array_t pdu;
} nr_up_data_delivery_status_req_t;

typedef void (*nr_up_transfer_user_data_fn_t)(const nr_up_transfer_user_data_req_t *req);
typedef void (*nr_up_data_delivery_status_fn_t)(const nr_up_data_delivery_status_req_t *req);
/** RLC to PDCP DRB path: F1-U toward CU or mono/UE direct (DDS + optional SDU) */
typedef void (*nr_up_dds_user_data_fn_t)(protocol_ctxt_t *ctxt, ue_id_t rlc_ue_id, rb_id_t drb_id, uint8_t *buf, size_t len, int tx_space);

typedef struct nr_up_if_s {
  nr_up_transfer_user_data_fn_t transfer_user_data;
  /** DU: request DDS TX toward the CU */
  nr_up_data_delivery_status_fn_t data_delivery_status;
  /** RLC to PDCP DRB delivery (backend set at init) */
  nr_up_dds_user_data_fn_t dds_user_data;
} nr_up_if_t;

/** Last DDS reported toward the peer (mono sync or F1 DU TX). */
typedef struct nr_up_dds_s {
  bool has_last_report;
  uint32_t last_desired_buffer_size;
  uint64_t last_report_time;
} nr_up_dds_t;

/* Per-DRB desired buffer size tracking */
#define NR_UP_DBS_NEVER_SYNCED UINT64_MAX
typedef struct nr_up_dbs_budget_s {
  // Remaining desired buffer size
  uint32_t available_dbs;
  // Last reported desired buffer size
  uint32_t desired_buffer_size;
  // Managed milliseconds when the desired buffer size was last synced
  uint64_t last_dbs_sync;
  // Managed milliseconds when Report Polling was last requested
  uint64_t last_report_poll;
} nr_up_dbs_t;

/** Per-DRB nr-up state: desired buffer size */
typedef struct nr_up_desired_buffer_s {
  // Northbound NR-UP (DL User Data)
  nr_up_dbs_t dbs;
  // Southbound NR-UP (DDS)
  nr_up_dds_t dds_report;
} nr_up_desired_buffer_t;

/** Per-UE budget: DRB slots indexed as `drb_id - 1` (same as PDCP). */
typedef struct nr_up_ue_state {
  nr_up_desired_buffer_t drb[MAX_DRBS_PER_UE];
} nr_up_ue_state_t;

struct nr_up_if_s *get_nr_up_if(void);

void nr_up_ms_tick(void);
uint64_t nr_up_current_time_ms(void);

void nr_up_manager_init(void);
void nr_up_add_ue(ue_id_t ue_id);
void nr_up_release_ue(ue_id_t ue_id);
void nr_up_release_drb(ue_id_t ue_id, rb_id_t drb_id);
nr_up_dbs_t nr_up_manager_get_dbs(ue_id_t ue_id, rb_id_t drb_id);
void nr_up_update_ue_id(ue_id_t old_ue_id, ue_id_t new_ue_id);
nr_up_dds_t nr_up_get_dds(ue_id_t ue_id, rb_id_t drb_id);
void nr_up_set_dds(ue_id_t ue_id, rb_id_t drb_id, nr_up_dds_t dds);
void nr_up_data_delivery_status(ue_id_t ue_id, rb_id_t drb_id, uint32_t space, bool is_poll, uint8_t *buf, size_t len);
void nr_up_dds_user_data(protocol_ctxt_t *ctxt, ue_id_t rlc_ue_id, rb_id_t drb_id, uint8_t *buf, size_t len, int tx_space);
void nr_up_consume_dbs(ue_id_t ue_id, rb_id_t drb_id, size_t bytes);
void nr_up_sync_dbs(ue_id_t ue_id, rb_id_t drb_id, uint32_t dbs);
bool nr_up_report_polling(ue_id_t ue_id, rb_id_t drb_id);

#endif /* NR_UP_H */
