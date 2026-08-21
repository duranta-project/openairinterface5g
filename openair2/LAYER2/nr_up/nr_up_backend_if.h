/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_UP_BACKEND_IF_H
#define NR_UP_BACKEND_IF_H

#include <stdint.h>
#include <time.h>
#include "nr_up/nr_up.h"

typedef struct nr_up_drb_dl_drop_log_s {
  uint32_t drops_pending;
  time_t last_log_sec;
} nr_up_drb_dl_drop_log_t;

typedef struct nr_up_drb_budget_s {
  // Bytes available for TX on this DRB
  uint32_t available_tx_bytes;
  struct timespec last_rlc_refresh;
  nr_up_drb_dl_drop_log_t dl_drop_log;
} nr_up_drb_budget_t;

size_t nr_up_timespec_diff_ms(const struct timespec *later, const struct timespec *earlier);

void nr_up_manager_init(void);
void nr_up_manager_lock(void);
void nr_up_manager_unlock(void);
nr_up_drb_budget_t *nr_up_manager_lookup_drb(ue_id_t ue_id, rb_id_t drb_id);
void nr_up_manager_release_drb(ue_id_t ue_id, rb_id_t drb_id);
void nr_up_drb_budget_consume(ue_id_t ue_id, rb_id_t drb_id, size_t bytes);
void nr_up_drb_budget_sync(ue_id_t ue_id, rb_id_t drb_id, uint32_t available_tx_bytes);

#endif /* NR_UP_BACKEND_IF_H */
