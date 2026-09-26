/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef SPECTRUM_SM_H
#define SPECTRUM_SM_H

#include <libe3/c_api.h>

#include "spectrum_dec.h"
#include <stdbool.h>
#include <pthread.h>
#include <stdint.h>
#include <stddef.h>

/* Sensing-range telemetry stream TIDs (advertised in the setupResponse).
 * Wire values follow the L1-KPM convention (id == bit_position + 1). */
#define SPECTRUM_SM_TID_SENSING_RANGES 1u
#define SPECTRUM_SM_TID_TIMESTAMP 2u
#define SPECTRUM_SM_TID_SFN 3u
#define SPECTRUM_SM_TID_SLOT 4u
#define SPECTRUM_SM_TID_BEAM 5u

e3_c_service_model_desc_t* create_spectrum_sm_model(void);
void spectrum_sm_set_handle(e3_service_model_handle_t* sm_handle);

/**
 * Set the sensing-range telemetry emission cadence.
 *
 * @param period_us  0 = emit on each MAC sensing publish (one indication per
 *                   scheduled UL slot); >0 = periodic, microseconds between
 *                   batches. e3_agent.c derives the value from the subscribed
 *                   dApps' declared subscription periodicity.
 */
void spectrum_telemetry_set_period_us(uint32_t period_us);

/**
 * Lifecycle context for the Spectrum SM (RF=1).
 *
 * PRB-block + sensing-policy controls in (applied directly via
 * set_prb_block_mask() / the MAC's set_sensing_policy) and sensing-range
 * telemetry out (the SM's start/stop drive the telemetry worker, which owns the
 * /e3_l2_sensing shm ring). IQ telemetry is a different SM (L1-KPM, RF=2,
 * /e3_ran_buffers).
 */
typedef struct {
  pthread_mutex_t lock;
  bool running;
  bool initialized;
} spectrum_sm_context_t;

#endif // SPECTRUM_SM_H
