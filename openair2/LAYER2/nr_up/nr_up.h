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
typedef struct nr_up_dl_user_data_req_s {
  ue_id_t ue_id;
  uint8_t drb_id;
  int sdu_id;
  /** PDCP SN for F1-U Report Delivered */
  uint32_t pdcp_sn;
  /** Upper bound for flow-control precheck */
  size_t pdu_max_size;
  byte_array_t pdu;
  bool report_polling;
} nr_up_user_data_req_t;

typedef enum nr_up_flow_control_action_e {
  NR_UP_FLOW_ALLOW,
  NR_UP_FLOW_ALLOW_RP,
  NR_UP_FLOW_DROP,
} nr_up_flow_control_action_t;

/** Request to send DDDS */
typedef struct nr_up_ddds_req_s {
  ue_id_t ue_id;
  rb_id_t drb_id;
  uint32_t desired_buffer_size;
  bool report_ddds;
  byte_array_t pdu;
} nr_up_ddds_req_t;

typedef void (*nr_up_user_data_fn_t)(const nr_up_user_data_req_t *req);
typedef void (*nr_up_ddds_fn_t)(ue_id_t ue_id, rb_id_t drb_id, uint32_t dbs);
typedef void (*nr_up_send_ddds_fn_t)(const nr_up_ddds_req_t *req);
/** RLC UL DRB leave: F1-U or mono/UE direct */
typedef void (*nr_up_ul_user_data_fn_t)(protocol_ctxt_t *ctxt, ue_id_t rlc_ue_id, rb_id_t drb_id, uint8_t *buf, size_t len, int tx_space);

typedef struct nr_up_if_s {
  nr_up_user_data_fn_t up_user_data;
  /** CU: apply remote Desired Buffer Size into the local budget */
  nr_up_ddds_fn_t up_ddds_indication;
  /** DU: request DDDS TX toward the CU */
  nr_up_send_ddds_fn_t up_send_ddds;
  /** RLC UL DRB delivery (backend set at init) */
  nr_up_ul_user_data_fn_t ul_user_data;
} nr_up_if_t;

/** Last DDDS reported toward the peer (mono sync or F1 DU TX). */
typedef struct nr_up_ddds_s {
  bool has_last_report;
  uint32_t last_desired_buffer_size;
  uint64_t last_report_time;
} nr_up_ddds_t;

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

/** Per-DRB nr-up state: desired buffer size plus DDDS report policy */
typedef struct nr_up_desired_buffer_s {
  // Northbound NR-UP (DL User Data)
  nr_up_dbs_t dbs;
  // Southbound NR-UP (DDDS)
  nr_up_ddds_t ddds_report;
} nr_up_desired_buffer_t;

struct nr_up_if_s *get_nr_up_if(void);

void nr_up_ms_tick(void);
uint64_t nr_up_current_time_ms(void);

void nr_up_manager_init(void);
void nr_up_add_drb(ue_id_t ue_id, rb_id_t drb_id);
void nr_up_release_drb(ue_id_t ue_id, rb_id_t drb_id);
nr_up_dbs_t nr_up_manager_get_dbs(ue_id_t ue_id, rb_id_t drb_id);
void nr_up_update_ue_id(ue_id_t old_ue_id, ue_id_t new_ue_id);
nr_up_ddds_t nr_up_get_ddds(ue_id_t ue_id, rb_id_t drb_id);
void nr_up_set_ddds(ue_id_t ue_id, rb_id_t drb_id, nr_up_ddds_t ddds);
void nr_up_consume_dbs(ue_id_t ue_id, rb_id_t drb_id, size_t bytes);
void nr_up_sync_dbs(ue_id_t ue_id, rb_id_t drb_id, uint32_t dbs);
bool nr_up_report_polling(ue_id_t ue_id, rb_id_t drb_id);

#endif /* NR_UP_H */
