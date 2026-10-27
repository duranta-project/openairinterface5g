/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _NR_RLC_ENTITY_UM_H_
#define _NR_RLC_ENTITY_UM_H_

#include "nr_rlc_entity.h"
#include "nr_rlc_sdu.h"
#include "nr_rlc_pdu.h"

typedef struct {
  nr_rlc_entity_t common;

  /* configuration */
  int t_reassembly;
  int sn_field_length;

  int sn_modulus;
  int window_size;

  /* runtime rx */
  int rx_next_highest;
  int rx_next_reassembly;
  int rx_timer_trigger;

  /* runtime tx */
  int tx_next;

  /* set to the latest know time by the user of the module. Unit: ms */
  uint64_t t_current;

  /* deal with logging of buffer full */
  uint64_t t_log_buffer_full;
  int      sdu_rejected;

  /* timers (stores the TTI of activation, 0 means not active) */
  uint64_t t_reassembly_start;

  /* rx management */
  nr_rlc_pdu_t *rx_list;
  int          rx_size;
  int          rx_maxsize;

  /* tx management */
  nr_rlc_sdu_segment_t *tx_list;
  nr_rlc_sdu_segment_t *tx_end;
  int                  tx_size;
  int                  tx_maxsize;
} nr_rlc_entity_um_t;

void nr_rlc_entity_um_recv_sdu(nr_rlc_entity_t *entity, char *buffer, int size, int sdu_id);
void nr_rlc_entity_um_recv_pdu(nr_rlc_entity_t *entity, char *buffer, int size);
nr_rlc_entity_buffer_status_t nr_rlc_entity_um_buffer_status(nr_rlc_entity_t *entity, int maxsize);
int nr_rlc_entity_um_generate_pdu(nr_rlc_entity_t *entity, char *buffer, int size);
void nr_rlc_entity_um_set_time(nr_rlc_entity_t *entity, uint64_t now);
void nr_rlc_entity_um_discard_sdu(nr_rlc_entity_t *_entity, int sdu_id);
void nr_rlc_entity_um_discard_sdus_up_to(nr_rlc_entity_t *_entity, int sdu_id);
void nr_rlc_entity_um_reestablishment(nr_rlc_entity_t *_entity);
void nr_rlc_entity_um_delete(nr_rlc_entity_t *entity);
int nr_rlc_entity_um_available_tx_space(nr_rlc_entity_t *entity);
int nr_rlc_entity_um_tx_list_occupancy(nr_rlc_entity_t *_entity);

#endif /* _NR_RLC_ENTITY_UM_H_ */
