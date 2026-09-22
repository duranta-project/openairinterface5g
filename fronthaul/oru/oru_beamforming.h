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
                        c16_t rotation);

// UL Rx beamforming: combine per-antenna FFT output into one beam stream, out[k] =
// sum_a w[beam][a][stream] * fft_data[a][k]
void combine_ul_beam_fd(const c16_t *const rxdataF[],
                        int nb_rx,
                        int n,
                        const oru_codebook_t *cb,
                        int beam_id,
                        int stream_id,
                        c16_t *out);

#ifdef __cplusplus
}
#endif
