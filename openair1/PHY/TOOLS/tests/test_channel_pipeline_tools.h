/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#pragma once
#include "common/platform_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void channel_convolution_cpu(const cf_t **channel,
                             const c16_t **tx_sig0,
                             const c16_t **tx_sig1,
                             int num_samples_tx_sig0,
                             c16_t **rx_sig0,
                             c16_t **rx_sig1,
                             int num_samples_rx_sig0,
                             int num_samples,
                             int channel_length,
                             int nb_tx,
                             int nb_rx);

void generate_random_signal(c16_t *sig, int num_samples);
void generate_random_signal_float(cf_t *sig, int num_samples);
void *init_tpool(int num_threads);
/// Pool with one worker per core of the comma-separated list, pinned to it.
void *init_tpool_cores(const char *cores);
void destroy_tpool(void *tpool);

#ifdef __cplusplus
}
#endif
