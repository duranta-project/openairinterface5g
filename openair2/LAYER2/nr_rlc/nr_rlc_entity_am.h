/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _NR_RLC_ENTITY_AM_H_
#define _NR_RLC_ENTITY_AM_H_

#include "nr_rlc_entity.h"
#include "nr_rlc_sdu.h"
#include "nr_rlc_pdu.h"
#include "nr_rlc_rx_manager.h"

typedef struct {
  nr_rlc_entity_t common;

  /* configuration */
  int t_poll_retransmit;
  int t_reassembly;
  int t_status_prohibit;
  int poll_pdu;              /* -1 means infinity */
  int poll_byte;             /* -1 means infinity */
  int max_retx_threshold;
  int sn_field_length;

  int sn_modulus;
  int window_size;

  /* runtime rx */
  int rx_next;
  int rx_next_status_trigger;
  int rx_highest_status;
  int rx_next_highest;

  int status_triggered;

  /* runtime tx */
  int tx_next;
  int tx_next_ack;
  int poll_sn;
  int pdu_without_poll;
  int byte_without_poll;
  int force_poll;

  /* set to the latest know time by the user of the module. Unit: ms */
  uint64_t t_current;

  /* deal with logging of buffer full */
  uint64_t t_log_buffer_full;
  int      sdu_rejected;

  /* timers (stores the TTI of activation, 0 means not active) */
  uint64_t t_poll_retransmit_start;
  uint64_t t_reassembly_start;
  uint64_t t_status_prohibit_start;

  /* rx management */
  nr_rlc_rx_manager_t *rx;
  int                 rx_size;
  int                 rx_maxsize;

  /* tx management */
  nr_rlc_sdu_segment_t *tx_list;
  nr_rlc_sdu_segment_t *tx_end;
  int                  tx_size;
  int                  tx_maxsize;

  nr_rlc_sdu_segment_t *wait_list;
  nr_rlc_sdu_segment_t *wait_end;

  nr_rlc_sdu_segment_t *retransmit_list;
} nr_rlc_entity_am_t;

void nr_rlc_entity_am_recv_sdu(nr_rlc_entity_t *entity,
                               char *buffer, int size,
                               int sdu_id);
void nr_rlc_entity_am_recv_pdu(nr_rlc_entity_t *entity,
                               char *buffer, int size);
nr_rlc_entity_buffer_status_t nr_rlc_entity_am_buffer_status(
    nr_rlc_entity_t *entity, int maxsize);
int nr_rlc_entity_am_generate_pdu(nr_rlc_entity_t *entity,
                                  char *buffer, int size);
void nr_rlc_entity_am_set_time(nr_rlc_entity_t *entity, uint64_t now);
void nr_rlc_entity_am_discard_sdu(nr_rlc_entity_t *_entity, int sdu_id);
void nr_rlc_entity_am_discard_sdus_up_to(nr_rlc_entity_t *_entity, int sdu_id);
void nr_rlc_entity_am_reestablishment(nr_rlc_entity_t *_entity);
void nr_rlc_entity_am_delete(nr_rlc_entity_t *entity);
int nr_rlc_entity_am_available_tx_space(nr_rlc_entity_t *entity);
int nr_rlc_entity_am_tx_list_occupancy(nr_rlc_entity_t *_entity);

#endif /* _NR_RLC_ENTITY_AM_H_ */
