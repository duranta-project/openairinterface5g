/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef DU_FHI_PRACH_H
#define DU_FHI_PRACH_H

#include "du_fhi_core.h"
#include "openair2/LAYER2/NR_MAC_COMMON/nr_prach_config.h"
#include "openair1/PHY/defs_gNB.h"

#ifdef __cplusplus
extern "C" {
#endif

// PRACH occasion info + config index, kept separate from du_fhi_state_t (du_fhi_core.h) so
// code that never touches PRACH doesn't need this header (and the L2 library it requires).
typedef struct {
  uint8_t prach_config_index;
  frequency_range_t freq_range; // the PRACH table prach_info was taken from
  nr_prach_info_t prach_info;
  int freq_start; // msg1-FrequencyStart, in PRBs
  int fft_size; // exponent of 2 of the PRACH FFT size
} du_fhi_prach_config_t;

// For every absolute slot in (last_scheduled_absolute_slot, target], issue
// du_fh_expect_prach_occasion() for whichever ones are live PRACH occasions per the configured
// PRACH table -- called alongside du_fhi_advance_ul_schedule_lookahead() (same [start, target]
// range), mirroring what libxran generates internally from the same static PRACH configuration.
void du_fhi_advance_prach_lookahead(du_fhi_state_t *st,
                                    const du_fhi_prach_config_t *prach_cfg,
                                    uint64_t start_absolute_slot,
                                    uint64_t target_absolute_slot,
                                    int nb_rx);

// True for the long-sequence PRACH formats 0-3
bool du_fhi_prach_is_long_format(uint32_t format);

// Pops a pending PRACH RX request (if any) from the gNB's transport-agnostic PRACH job queue
// and fills its buffer from du_fh's ready PRACH jobs of absolute_slot. Jobs of later slots stay
// queued, jobs of earlier (skipped) slots are dropped. No RU_t field is written for PRACH (mirrors
// radio/fhi_72/oran_isolate.c, which likewise never touches RU_t for the PRACH leg).
void du_fhi_south_in_prach(void *du_fh_handle, PHY_VARS_gNB *gNB, uint64_t absolute_slot, int numerology);

#ifdef __cplusplus
}
#endif

#endif /* DU_FHI_PRACH_H */
