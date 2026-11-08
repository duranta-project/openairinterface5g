/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_INNER_RX_OFFLOAD_H
#define NR_INNER_RX_OFFLOAD_H

#include <stdbool.h>
#include <stddef.h>
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
#define NR_IRX_MAX_CH 4 ///< channel estimates (DMRS symbols) per batch
#define NR_IRX_MAX_RX 8 ///< Rx antennas of rx_ant

/// One OFDM symbol of a batch
typedef struct {
  const c16_t *rx_ant[NR_IRX_MAX_RX]; ///< the symbol's first allocation RE in each antenna's rxdataF, in memory registered with
                                      ///< register_host: the module gathers its data REs through re_map; rx_ant[0] NULL: rxF
  const c16_t *rxF; ///< or the extracted RX [nb_rx][stride] in the RX staging
  const c16_t *chF; ///< channel estimates over the allocation [2][nb_rx][ch_stride], shared by the symbols of a DMRS
  const uint16_t *re_map; ///< chF index (allocation RE) of each extracted RE, or NULL: the identity
  const uint32_t *scramble; ///< packed descrambling bits of the codeword (bit n: word n / 32, bit n % 32; 1 negates), or NULL
  uint32_t scramble_offset; ///< codeword bit of this symbol's first LLR
  int16_t *llr_cw; ///< codeword slice: layer-demapped LLRs (per RE: layer 0 then layer 1), length * 2 * Qm
  uint32_t length; ///< REs of this symbol (< 65536)
  uint32_t stride; ///< row stride of rxF
  uint32_t ch_stride; ///< row stride of chF
  c16_t cpe; ///< PTRS common phase error, (32767, 0) without PTRS
} nr_irx_sym_t;

/// detector: 2-layer MMSE, L-best, full ML (the LLR units of nr_compute_MMSE_llr / nr_compute_ML_llr)
enum { NR_IRX_DET_MMSE = 0, NR_IRX_DET_LBEST = 1, NR_IRX_DET_ML = 3 };
/// staging buffers, index k: RX of batch symbol k [nb_rx][stride] (k < NR_IRX_MAX_SYM), CH estimate k
/// [2][nb_rx][stride] (k < NR_IRX_MAX_CH), REMAP of symbol k [stride] uint16, SCRAMBLE packed bits of the batch (k = 0,
/// NR_IRX_MAX_SYM * stride * 16 bits)
enum { NR_IRX_STAGING_RX = 0, NR_IRX_STAGING_CH, NR_IRX_STAGING_REMAP, NR_IRX_STAGING_SCRAMBLE };

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
/// host-writable staging buffer `which`, index k (see NR_IRX_STAGING_*)
typedef void *(nr_irx_staging_t)(nr_irx_ctx_t *ctx, int k, int which);
/// synchronous: sym[] point into the context's staging (llr_cw: host pointer into the codeword buffer); 0 on success
typedef int(nr_irx_run_t)(nr_irx_ctx_t *ctx, const nr_irx_sym_t *sym, int nsym, const nr_irx_params_t *p);
/// makes [ptr, ptr + bytes) readable by the module (rxdataF), idempotent for a registered ptr: 0 when registered (rx_ant
/// may point into it), 1 when the module reads extracted RX in the RX staging faster, < 0 on error
typedef int(nr_irx_register_host_t)(const void *ptr, size_t bytes);

typedef struct {
  nr_irx_init_t *init;
  nr_irx_shutdown_t *shutdown;
  nr_irx_supported_t *supported;
  nr_irx_acquire_t *acquire;
  nr_irx_release_t *release;
  nr_irx_staging_t *staging;
  nr_irx_run_t *run;
  nr_irx_register_host_t *register_host;
} nr_irx_interface_t;

/// Loads and initializes the module named by loader.inner_rx.shlibversion; 0 if loaded, -1 (itf zeroed) if none is
/// configured or it fails to load.
int load_nr_irx_interface(nr_irx_interface_t *itf, uint32_t max_re_per_symbol, int max_nb_rx, int nb_ctx);
void free_nr_irx_interface(nr_irx_interface_t *itf);

#ifdef __cplusplus
}
#endif

#endif
