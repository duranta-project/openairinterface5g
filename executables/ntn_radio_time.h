/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */
#ifndef NTN_RADIO_TIME_H
#define NTN_RADIO_TIME_H

#include <stdbool.h>
#include "openair2/LAYER2/NR_MAC_gNB/ntn_assistance.h"

struct RU_t_s;

/* Startup only, after RU configuration and before releasing the RX thread.
 * Supports exactly one local RU attached to gNB 0 / CC 0. Does not read the
 * hardware clock: a complete aligned RX frame is required before query works. */
#ifdef ENABLE_NTN_ASSISTANCE
bool nr_ntn_radio_time_init(struct RU_t_s *ru);

/* One RX writer, at the end of each slot read. A partial read invalidates the
 * previous generation immediately, even if the single snapshot trylock fails.
 * This path performs no allocation, logging, waiting or backend clock call. */
void nr_ntn_radio_time_rx(struct RU_t_s *ru, int frame, int slot, bool complete);

/* Control worker only. NULL opaque selects the initialized sole RU; a non-NULL
 * value must identify that RU. Output is unchanged on failure. Host and device
 * ages must both be <= 200 ms. Does not establish UTC/PPS or RF-port timing. */
bool nr_ntn_radio_time_query(void *opaque, nr_ntn_radio_time_t *observation);

/* Disable before joining the control worker and freeing its RU. The caller
 * owns startup/shutdown serialization and must quiesce in-flight queries before
 * backend teardown. No RU or device storage is freed here. */
void nr_ntn_radio_time_disable(void);
#else
static inline bool nr_ntn_radio_time_init(struct RU_t_s *ru)
{
  (void)ru;
  return false;
}

static inline void nr_ntn_radio_time_rx(struct RU_t_s *ru, int frame, int slot, bool complete)
{
  (void)ru;
  (void)frame;
  (void)slot;
  (void)complete;
}

static inline bool nr_ntn_radio_time_query(void *opaque, nr_ntn_radio_time_t *observation)
{
  (void)opaque;
  (void)observation;
  return false;
}

static inline void nr_ntn_radio_time_disable(void)
{
}
#endif

#endif
