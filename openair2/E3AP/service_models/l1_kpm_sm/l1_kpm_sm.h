/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/**
 * @file l1_kpm_sm.h
 * @brief L1-KPM Service Model (RAN Function ID 2): post-FFT IQ telemetry.
 *
 * Telemetry-only SM that dApps subscribe to. Each indication carries a
 * reference to the IQ data plus its timestamp, SFN and slot; the IQ samples
 * themselves live in the /e3_ran_buffers shared memory (written elsewhere).
 * Wire-compatible with NVIDIA cuBB's KPM SM so aerial dApps connect unchanged.
 */
#ifndef L1_KPM_SM_H
#define L1_KPM_SM_H

#include <libe3/c_api.h>
#include <stdbool.h>
#include <stdint.h>

#include "openair2/E3AP/config/e3_config.h"

/** RAN-function (on-wire) ID, single-sourced from e3_config.h */
#define L1_KPM_SM_RAN_FUNCTION_ID E3_SM_ID_KPM

/* Telemetry IDs aerial subscribes to (wire value == bit position + 1). */
#define L1_KPM_TID_IQ_SAMPLES 1u
#define L1_KPM_TID_TIMESTAMP 4u
#define L1_KPM_TID_SFN 5u
#define L1_KPM_TID_SLOT 6u

/** Build a libe3 SM descriptor for the L1-KPM SM. */
e3_c_service_model_desc_t* create_l1_kpm_sm_model(void);

/** Store the SM handle so the worker thread can emit indications via it. */
void l1_kpm_sm_set_handle(e3_service_model_handle_t* sm_handle);

/**
 * True if at least one dApp is subscribed to RF=2. The PHY tap checks this to
 * skip the per-slot IQ conversion when nobody is listening.
 */
bool l1_kpm_sm_has_subscribers(void);

/**
 * Set how often indications are emitted. period_us == 0 means emit on each PHY
 * publish (one per UL slot); a positive value emits at most once per period.
 * The agent derives the value from the subscribed dApps' declared periodicity.
 * Takes effect on the next worker iteration.
 */
void l1_kpm_sm_set_period_us(uint32_t period_us);

#endif /* L1_KPM_SM_H */
