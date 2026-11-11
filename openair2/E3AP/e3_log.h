/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/**
 * @file e3_log.h
 * @brief Module-tagged logging for E3AP.
 *
 * Everything under openair2/E3AP logs through the single E3AP component
 * (`--log_config.e3ap_log_level`), so one trace interleaves the agent, the
 * shared service-model plumbing and every service model. These macros carry
 * the module in the message so that trace stays readable, and make the tag
 * impossible to forget or spell differently at each call site. Adding a module
 * means adding a block here.
 *
 * Only modules with a fixed identity get a block. e3_sm_worker.c and
 * e3_shm_region.c deliberately do not: they are shared, and already log the
 * caller's own tag dynamically ("[%s]", desc->log_tag), which says which
 * service model is driving them -- more useful than a constant would be.
 *
 * Same idea as radio/fhi_72/mplane/ru-mplane-api.h, applied once centrally
 * because several E3AP modules share one log component.
 */
#ifndef E3_LOG_H
#define E3_LOG_H

#include "common/utils/LOG/log.h"

/* E3 agent core */
#define E3_LOG_E(x, args...) LOG_E(E3AP, "[AGENT] " x, ##args)
#define E3_LOG_W(x, args...) LOG_W(E3AP, "[AGENT] " x, ##args)
#define E3_LOG_I(x, args...) LOG_I(E3AP, "[AGENT] " x, ##args)
#define E3_LOG_D(x, args...) LOG_D(E3AP, "[AGENT] " x, ##args)

/* Spectrum service model. Tag matches the SM's own log_tag, which
 * e3_sm_worker.c prints dynamically -- keep them identical. */
#define SPEC_LOG_E(x, args...) LOG_E(E3AP, "[SPECTRUM-SM] " x, ##args)
#define SPEC_LOG_W(x, args...) LOG_W(E3AP, "[SPECTRUM-SM] " x, ##args)
#define SPEC_LOG_I(x, args...) LOG_I(E3AP, "[SPECTRUM-SM] " x, ##args)
#define SPEC_LOG_D(x, args...) LOG_D(E3AP, "[SPECTRUM-SM] " x, ##args)

/* L1-KPM service model */
#define KPM_LOG_E(x, args...) LOG_E(E3AP, "[KPM-SM] " x, ##args)
#define KPM_LOG_W(x, args...) LOG_W(E3AP, "[KPM-SM] " x, ##args)
#define KPM_LOG_I(x, args...) LOG_I(E3AP, "[KPM-SM] " x, ##args)
#define KPM_LOG_D(x, args...) LOG_D(E3AP, "[KPM-SM] " x, ##args)

/* L1-KPM post-FFT IQ shared-memory producer */
#define E3RB_LOG_E(x, args...) LOG_E(E3AP, "[E3RB] " x, ##args)
#define E3RB_LOG_W(x, args...) LOG_W(E3AP, "[E3RB] " x, ##args)
#define E3RB_LOG_I(x, args...) LOG_I(E3AP, "[E3RB] " x, ##args)
#define E3RB_LOG_D(x, args...) LOG_D(E3AP, "[E3RB] " x, ##args)

#endif /* E3_LOG_H */
