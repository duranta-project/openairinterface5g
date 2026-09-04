/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _CHANNEL_CONVOLUTION_H_
#define _CHANNEL_CONVOLUTION_H_

#ifdef __cplusplus
extern "C" {
#endif
#include "common/platform_types.h"

#ifdef CHANNEL_SIM_CUDA
void *cuda_channel_pipeline_init(int max_samples, int num_tx_antenna);
void cuda_channel_pipeline_shutdown(void *context_handle);
void cuda_channel_pipeline(void *context_handle,
                           const cf_t **channel,
                           const c16_t **tx_sig0,
                           const c16_t **tx_sig1,
                           int num_samples_tx_sig0,
                           c16_t **rx_sig0,
                           c16_t **rx_sig1,
                           int num_samples_rx_sig0,
                           int num_samples,
                           int channel_length,
                           int nb_tx,
                           int nb_rx,
                           float noise_power);
#endif
void channel_pipeline_init(float noise_power);
void channel_pipeline_shutdown(void);
void channel_pipeline(void *tpool,
                      const cf_t **channel,
                      const c16_t **tx_sig0,
                      const c16_t **tx_sig1,
                      int num_samples_tx_sig0,
                      c16_t **rx_sig0,
                      c16_t **rx_sig1,
                      int num_samples_rx_sig0,
                      int num_samples,
                      int channel_length,
                      int nb_tx,
                      int nb_rx,
                      float noise_power);
#ifdef __cplusplus
}
#endif

#endif
