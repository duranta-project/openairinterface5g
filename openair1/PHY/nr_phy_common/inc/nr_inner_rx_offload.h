/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_INNER_RX_OFFLOAD_H
#define NR_INNER_RX_OFFLOAD_H

#include <stdbool.h>
#include <stdint.h>
#include "common/platform_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Inner-RX offload modules: lib inner_rx<version>.so, selected with --loader.inner_rx.shlibversion <version> (_gpu:
 * CUDA/HIP; other accelerators implement the same entry points). A module runs the 2-layer PUSCH inner RX of a batch of
 * symbols: matched filter with the PTRS common phase error, Gram, detector, layer demap and descrambling into the
 * codeword LLRs. The PHY extracts each symbol into staging buffers the module hands out, so the module chooses their
 * memory (pinned, buffer objects, ...); what supported() declines stays on the CPU.
 */

#define NR_IRX_MAX_SYM 14

/// One OFDM symbol of a batch
typedef struct {
  const c16_t *rxF; ///< extracted RX [nb_rx][stride]
  const c16_t *chF; ///< extracted channel estimates [2][nb_rx][stride]
  const int16_t *scramble; ///< +-1 descrambling sequence of the codeword slice (length * 2 * Qm), or NULL
  int16_t *llr_cw; ///< codeword slice: layer-demapped LLRs (per RE: layer 0 then layer 1), length * 2 * Qm
  uint32_t length; ///< REs of this symbol (< 65536)
  uint32_t stride; ///< row stride of rxF / chF
  c16_t cpe; ///< PTRS common phase error, (32767, 0) without PTRS
} nr_irx_sym_t;

/// detector: 2-layer MMSE, L-best, full ML (the LLR units of nr_compute_MMSE_llr / nr_compute_ML_llr)
enum { NR_IRX_DET_MMSE = 0, NR_IRX_DET_LBEST = 1, NR_IRX_DET_ML = 3 };
/// staging buffers of symbol k of a batch: rxF [nb_rx][stride], chF [2][nb_rx][stride], scramble [length * 2 * Qm]
enum { NR_IRX_STAGING_RX = 0, NR_IRX_STAGING_CH, NR_IRX_STAGING_SCRAMBLE };

typedef struct {
  int nb_layers;
  int nb_rx;
  int mod_order;
  int detector;
  int output_shift; ///< log2_maxh of the matched filter
  float lambda; ///< noise variance >> output_shift (nr_2layer_seed_lambda)
} nr_irx_params_t;

/// per-PUSCH context (stream, staging, scratch): one per PUSCH processed concurrently
typedef struct nr_irx_ctx nr_irx_ctx_t;

/// staging stride and capacity: max_re_per_symbol REs per row, max_nb_rx Rx antennas; nb_ctx contexts
typedef int(nr_irx_init_t)(uint32_t max_re_per_symbol, int max_nb_rx, int nb_ctx);
typedef int(nr_irx_shutdown_t)(void);
typedef bool(nr_irx_supported_t)(int nb_layers, int nb_rx, int mod_order, int detector);
/// NULL if all contexts are busy (the PHY then stays on the CPU)
typedef nr_irx_ctx_t *(nr_irx_acquire_t)(void);
typedef void(nr_irx_release_t)(nr_irx_ctx_t *ctx);
/// host-writable staging buffer `which` of batch symbol k (k < NR_IRX_MAX_SYM)
typedef void *(nr_irx_staging_t)(nr_irx_ctx_t *ctx, int k, int which);
/// synchronous: sym[k] point to the staging of symbol k (llr_cw: host pointer into the codeword buffer); 0 on success
typedef int(nr_irx_run_t)(nr_irx_ctx_t *ctx, const nr_irx_sym_t *sym, int nsym, const nr_irx_params_t *p);

typedef struct {
  nr_irx_init_t *init;
  nr_irx_shutdown_t *shutdown;
  nr_irx_supported_t *supported;
  nr_irx_acquire_t *acquire;
  nr_irx_release_t *release;
  nr_irx_staging_t *staging;
  nr_irx_run_t *run;
} nr_irx_interface_t;

/// Loads and initializes the module named by loader.inner_rx.shlibversion; 0 if loaded, -1 (itf zeroed) if none is
/// configured or it fails to load.
int load_nr_irx_interface(nr_irx_interface_t *itf, uint32_t max_re_per_symbol, int max_nb_rx, int nb_ctx);
void free_nr_irx_interface(nr_irx_interface_t *itf);

#ifdef __cplusplus
}
#endif

#endif
