/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * Private to the Spectrum RAN function (ran_func_spectrum.c and
 * ran_func_spectrum_aerial.c). What the MAC may call is in
 * ran_func_spectrum_extern.h; what the Spectrum SM may call is in
 * ran_func_spectrum_types.h.
 */
#ifndef RAN_FUNC_SPECTRUM_H
#define RAN_FUNC_SPECTRUM_H

#include "openair2/E3AP/ran_func_spectrum_extern.h"

/* Max configurable sensing-PUSCH beams. Bounded by the FAPI beamforming fanout
 * NFAPI_MAX_NUM_BG_IF (=6); a _Static_assert in the Aerial TU enforces it. */
#define SENSING_MAX_BEAMS 6

/* Shape of the sensing PUSCH PDU (Aerial only). The symbol range comes from the
 * scanner; only MCS / PRBs / layers / beams are operator configuration. */
typedef struct {
  int mcs;
  int rb_size;
  int rb_start;
  int nr_of_layers;
  int num_beams;
  int beams[SENSING_MAX_BEAMS];
} e3_spectrum_pusch_cfg_t;

/* The PUSCH shape configured for a cell, NULL if sensing is not attached. */
const e3_spectrum_pusch_cfg_t *e3_spectrum_pusch_cfg(const nr_cell_sched_t *cell);

#ifdef ENABLE_AERIAL
/* Inject one sensing-RNTI "capture" PUSCH so Aerial's L1 materializes the slot's
 * IQ (it only captures where a PDU points); one per slot suffices. No-op if a
 * real PUSCH/PUCCH/SRS is already scheduled. Defined in
 * ran_func_spectrum_aerial.c; called from nr_mac_sensing_scan_and_publish. */
void nr_fill_sensing_pusch(nr_cell_sched_t *cell, frame_t frame, slot_t slot, const sensing_range_t *ranges, int n_ranges);
#endif

#endif /* RAN_FUNC_SPECTRUM_H */
