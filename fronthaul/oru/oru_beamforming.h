/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#pragma once

#include "oru_packet_processor.h"
#include "common/platform_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ORU_CODEBOOK_MAX_BEAMS 64
#define ORU_CODEBOOK_MAX_NB_TX 8
#define ORU_CODEBOOK_MAX_STREAMS 4

typedef struct {
  int nb_fh_streams;
  int nb_beams;
  c16_t w[ORU_CODEBOOK_MAX_BEAMS][ORU_CODEBOOK_MAX_NB_TX][ORU_CODEBOOK_MAX_STREAMS];
} oru_codebook_t;

// Dynamic weights of a stream or job are read per PRB bundle. get() returns as get_bf_weights(): n Q1.15 weights, one
// per TXRU (DL antennas from n up get nothing from that bundle, UL uses the first nb_rx), 0 to use the codebook for
// *beam_id, or -1 to skip the bundle.
typedef struct {
  void *ctx;
  int (*get)(void *ctx, const oru_bfw_set_t *set, int bundle, uint16_t *beam_id, c16_t *weights);
} oru_bfw_source_t;

// Turns read_dl_iq_streams()'s raw PRB runs into physical-antenna IQ. Pure function (no
// RU_t/ORU_t) so the math is directly unit-testable - see test_oru_beamforming.c. Digital
// beamforming only: no beam_id/GPIO output, since a beam here is a weight, not an antenna state.
//
// Passthrough (cb->nb_fh_streams <= 0): ant_id is the physical antenna; streams are placed
// directly, one stream per PRB range (no accumulation needed there). Codebook (cb->nb_fh_streams >
// 0): ant_id is a logical fronthaul stream index, weighted per PRB run by cb->w[beam][txru][ant_id]
// and summed onto each physical antenna - weighting per run (not per symbol).
//
// rotation (per-symbol phase compensation, Q1.15) is folded into the weight/each sample here -
// callers must not rotate txDataF again afterward.
//
// txDataF: nb_tx buffers of at least n_sc samples, zeroed by this function.
void combine_dl_streams(c16_t **txDataF,
                        int nb_tx,
                        int n_sc,
                        const dl_iq_stream_t *streams,
                        int num_streams,
                        const oru_codebook_t *cb,
                        const oru_bfw_source_t *bfw,
                        c16_t rotation);

// UL Rx beamforming: combine per-antenna FFT output into one beam stream over bins [k0, k0 + nk),
// out[k] = sum_a conj(w[beam][a][stream]) * fft_data[a][k]. The codebook holds transmit weights: a
// UE in the direction of DL beam w is received through a channel proportional to w, so the matched
// receive combiner for the same beam is w^H. Bins outside the range are left untouched.
void combine_ul_beam_fd(const c16_t *const rxdataF[],
                        int nb_rx,
                        int k0,
                        int nk,
                        const oru_codebook_t *cb,
                        int beam_id,
                        int stream_id,
                        c16_t *out);

// combine_ul_beam_fd() over just the PRBs [start_prb, start_prb + num_prb) of an nbins-wide band,
// with out and fft_data in unshifted FFT layout (fft_size bins). Contiguous RE c sits at bin
// c < nbins / 2 ? fft_size - nbins / 2 + c : c - nbins / 2 (the inverse of fftshift()), so the range
// covers at most two bin segments. The range is clamped to the band; other bins are left untouched.
// A bundle of set with dynamic weights (bfw) is combined with conj(weights[a]) instead of the codebook.
void combine_ul_beam_prbs(const c16_t *const fft_data[],
                          int nb_rx,
                          int fft_size,
                          int nbins,
                          int start_prb,
                          int num_prb,
                          const oru_codebook_t *cb,
                          int beam_id,
                          int stream_id,
                          const oru_bfw_source_t *bfw,
                          const oru_bfw_set_t *set,
                          c16_t *out);

// UL Rx beamforming of a PRACH section from start_prb: combine_ul_beam_fd() over sequence bins [0, nk), bin k being RE
// re_offset + k of the section (where write_prach_iq() puts it), in PRB start_prb + (re_offset + k) / 12. A bundle of
// set with dynamic weights (bfw) is combined with conj(weights[a]) instead of the codebook, as in combine_ul_beam_prbs().
void combine_ul_prach_beam(const c16_t *const rxdataF[],
                           int nb_rx,
                           int nk,
                           int re_offset,
                           int start_prb,
                           const oru_codebook_t *cb,
                           int beam_id,
                           int stream_id,
                           const oru_bfw_source_t *bfw,
                           const oru_bfw_set_t *set,
                           c16_t *out);

#ifdef __cplusplus
}
#endif
