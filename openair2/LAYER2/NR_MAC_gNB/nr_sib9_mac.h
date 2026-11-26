/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */
#ifndef NR_SIB9_MAC_H
#define NR_SIB9_MAC_H
#include <stdbool.h>
#include <stdint.h>
#include "NR_PHY_INTERFACE/NR_IF_Module.h"

typedef struct {
  bool observed, context_valid, attempted, valid, error_reported;
  int last_slot, rx_to_tx;
  int64_t absolute_slot, window;
} nr_sib9_window_t;

struct nr_cell_sched_s;
void nr_sib9_slot(struct nr_cell_sched_s *cell, int frame, int slot, const nr_slot_time_t *time);
const uint8_t *nr_sib9_payload(struct nr_cell_sched_s *cell, int rel_frame, int rel_slot, int window_slots, int *length);
#endif
