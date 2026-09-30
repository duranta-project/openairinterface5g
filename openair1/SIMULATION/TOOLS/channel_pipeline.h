/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _CHANNEL_CONVOLUTION_H_
#define _CHANNEL_CONVOLUTION_H_

#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>
#include "common/platform_types.h"

// Kernel instruction set used by the CPU pipeline. AUTO (the default) picks the best one the CPU
// supports at runtime.
typedef enum {
  CHANNEL_PIPELINE_ISA_AUTO,
  CHANNEL_PIPELINE_ISA_SCALAR,
  CHANNEL_PIPELINE_ISA_AVX2,
  CHANNEL_PIPELINE_ISA_AVX512,
  CHANNEL_PIPELINE_ISA_NEON,
  CHANNEL_PIPELINE_ISA_SVE2,
} channel_pipeline_isa_t;

// Convolution method of the CPU pipeline. AUTO (the default) picks per call.
typedef enum {
  CHANNEL_PIPELINE_METHOD_AUTO,
  CHANNEL_PIPELINE_METHOD_DIRECT,
  CHANNEL_PIPELINE_METHOD_FFT,
} channel_pipeline_method_t;

// CPU pipeline instance: owns its channel cache, scratch memory and noise device. Calls on one
// instance must not overlap; use one instance per calling thread.
typedef struct channel_pipeline_s channel_pipeline_t;

// Forcing the ISA or method is for tests and benchmarks only.
/// Force an ISA; returns false (and keeps the current one) if not compiled in or not supported.
bool channel_pipeline_set_isa(channel_pipeline_t *p, channel_pipeline_isa_t isa);
channel_pipeline_isa_t channel_pipeline_get_isa(const channel_pipeline_t *p);
const char *channel_pipeline_isa_name(channel_pipeline_isa_t isa);
void channel_pipeline_set_method(channel_pipeline_t *p, channel_pipeline_method_t method);

#ifdef CHANNEL_SIM_CUDA
void *cuda_channel_pipeline_init(int max_samples, int num_tx_antenna, int num_rx_antenna, int max_channel_length);
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
channel_pipeline_t *channel_pipeline_init(float noise_power);
void channel_pipeline_shutdown(channel_pipeline_t *p);
void channel_pipeline(channel_pipeline_t *p,
                      void *tpool,
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
