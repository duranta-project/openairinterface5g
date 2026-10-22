/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "oru_beamforming.h"
#include "openair1/PHY/TOOLS/tools_defs.h"
#include "common/utils/nr/nr_common.h"
#include "log.h"
#include <stdbool.h>
#include <string.h>

static void combine_passthrough(c16_t **txDataF,
                                int nb_tx,
                                int n_sc,
                                const dl_iq_stream_t *streams,
                                int num_streams,
                                c16_t rotation)
{
  for (int aatx = 0; aatx < nb_tx; aatx++) {
    memset(txDataF[aatx], 0, n_sc * sizeof(c16_t));
  }

  // Direct overwrite, assume no overalpping streams on the same tx antenna.
  for (int i = 0; i < num_streams; i++) {
    const dl_iq_stream_t *stream = &streams[i];
    if (stream->ant_id >= (unsigned)nb_tx) {
      continue;
    }
    c16_t *dst = &txDataF[stream->ant_id][stream->start_prb * NR_NB_SC_PER_RB];
    const c16_t *src = (const c16_t *)stream->iq;
    rotate_cpx_vector(src, rotation, dst, stream->num_prb * NR_NB_SC_PER_RB, 15);
  }
}

// Splits PRBs [prb, end) at the next bundle boundary of set and returns the end of the run starting at prb. Returns in
// *num_w the dynamic weight count of the run's bundle (weights in w), -1 to skip the run because its weights were
// overwritten, or 0 to use the codebook for *beam_id, which the caller presets to the section beamId and a bundle without
// dynamic weights replaces with its own.
static int bfw_run(const oru_bfw_source_t *bfw, const oru_bfw_set_t *set, int prb, int end, int *beam_id, c16_t *w, int *num_w)
{
  *num_w = 0;
  if (bfw == NULL || set->num_bundles == 0)
    return end;
  int run_end;
  if (prb < set->first_prb) {
    run_end = set->first_prb;
  } else {
    int b = oru_bfw_bundle(set, prb);
    if (b >= set->num_bundles)
      return end;
    run_end = set->first_prb + (b + 1) * set->bundle_prbs;
    uint16_t bundle_beam;
    int n = bfw->get(bfw->ctx, set, b, &bundle_beam, w);
    if (n != 0)
      *num_w = n;
    else
      *beam_id = bundle_beam;
  }
  return run_end < end ? run_end : end;
}

static void combine_codebook(c16_t **txDataF,
                             int nb_tx,
                             int n_sc,
                             const dl_iq_stream_t *streams,
                             int num_streams,
                             const oru_codebook_t *cb,
                             const oru_bfw_source_t *bfw,
                             c16_t rotation)
{
  for (int aatx = 0; aatx < nb_tx; aatx++) {
    memset(txDataF[aatx], 0, n_sc * sizeof(c16_t));
  }

  for (int i = 0; i < num_streams; i++) {
    const dl_iq_stream_t *stream = &streams[i];
    if (stream->ant_id >= cb->nb_fh_streams) {
      LOG_W(PHY, "DL stream ant_id %d exceeds nb_fh_streams %d, dropping\n", stream->ant_id, cb->nb_fh_streams);
      continue;
    }
    const int end = stream->start_prb + stream->num_prb;
    bool warned = false; // once per stream, not per bundle
    for (int prb = stream->start_prb; prb < end;) {
      int bidx = stream->beam_id;
      c16_t w[ORU_MAX_BF_WEIGHTS];
      int num_w;
      const int run_end = bfw_run(bfw, &stream->bfw, prb, end, &bidx, w, &num_w);
      if (num_w < 0) { // txDataF is already zero there
        prb = run_end;
        continue;
      }
      if (num_w == 0) {
        if (bidx >= cb->nb_beams) {
          if (!warned)
            LOG_W(PHY, "beam_id %d out of range (nb_beams=%d), falling back to beam 0\n", bidx, cb->nb_beams);
          warned = true;
          bidx = 0;
        }
        for (int txru = 0; txru < nb_tx; txru++)
          w[txru] = cb->w[bidx][txru][stream->ant_id];
        num_w = nb_tx;
      }
      const c16_t *src = (const c16_t *)stream->iq + (prb - stream->start_prb) * NR_NB_SC_PER_RB;
      const int n_re = (run_end - prb) * NR_NB_SC_PER_RB;
      const int re_off = prb * NR_NB_SC_PER_RB;
      for (int txru = 0; txru < nb_tx && txru < num_w; txru++) {
        // Fold rotation into the weight once, instead of a separate rotate pass afterward. Fused
        // multiply+saturating-accumulate straight into txDataF - no scratch buffer, one pass. The fold
        // saturates too: weights are arbitrary int16 from config or the DU, so w * rotation can exceed Q1.15.
        c16_t w_rot = c16mulShiftSat(w[txru], rotation, 15);
        rotate_add_cpx_vector(src, w_rot, &txDataF[txru][re_off], n_re, 15);
      }
      prb = run_end;
    }
  }
}

void combine_dl_streams(c16_t **txDataF,
                        int nb_tx,
                        int n_sc,
                        const dl_iq_stream_t *streams,
                        int num_streams,
                        const oru_codebook_t *cb,
                        const oru_bfw_source_t *bfw,
                        c16_t rotation)
{
  if (cb->nb_fh_streams <= 0) {
    combine_passthrough(txDataF, nb_tx, n_sc, streams, num_streams, rotation);
  } else {
    combine_codebook(txDataF, nb_tx, n_sc, streams, num_streams, cb, bfw, rotation);
  }
}

// out[k] = sum_a conj(w[a]) * fft_data[a][k] over bins [k0, k0 + nk), a < nb_ant
static void combine_ul_weights(const c16_t *const fft_data[], int nb_ant, int k0, int nk, const c16_t *w, c16_t *out)
{
  memset(&out[k0], 0, nk * sizeof(c16_t));
  for (int a = 0; a < nb_ant; a++) {
    // Same per-bin saturating multiply-accumulate as the DL codebook path, with the conjugate
    // (receive) weight: see the header for why UL uses w^H.
    const c16_t w_rx = {.r = w[a].r, .i = w[a].i == INT16_MIN ? INT16_MAX : (int16_t)-w[a].i};
    rotate_add_cpx_vector(&fft_data[a][k0], w_rx, &out[k0], nk, 15);
  }
}

// Codebook weights of beam_id for UL stream stream_id into w; returns the antenna count to combine (0: drop stream).
// Logs only while *warned is false, and sets it when it logs.
static int codebook_ul_weights(const oru_codebook_t *cb, int nb_rx, int beam_id, int stream_id, c16_t *w, bool *warned)
{
  if (stream_id >= cb->nb_fh_streams) {
    if (!*warned)
      LOG_W(PHY, "UL stream %d exceeds nb_fh_streams %d, dropping\n", stream_id, cb->nb_fh_streams);
    *warned = true;
    return 0;
  }
  int bidx = beam_id;
  if (bidx >= cb->nb_beams) {
    if (!*warned)
      LOG_W(PHY, "beam_id %d out of range (nb_beams=%d), falling back to beam 0\n", beam_id, cb->nb_beams);
    *warned = true;
    bidx = 0;
  }
  if (nb_rx > ORU_CODEBOOK_MAX_NB_TX) {
    if (!*warned)
      LOG_W(PHY,
            "nb_rx %d exceeds codebook antenna dimension %d, combining first %d antennas\n",
            nb_rx,
            ORU_CODEBOOK_MAX_NB_TX,
            ORU_CODEBOOK_MAX_NB_TX);
    *warned = true;
  }
  const int nb_ant = nb_rx < ORU_CODEBOOK_MAX_NB_TX ? nb_rx : ORU_CODEBOOK_MAX_NB_TX;
  for (int a = 0; a < nb_ant; a++)
    w[a] = cb->w[bidx][a][stream_id];
  return nb_ant;
}

void combine_ul_beam_fd(const c16_t *const fft_data[],
                        int nb_rx,
                        int k0,
                        int nk,
                        const oru_codebook_t *cb,
                        int beam_id,
                        int stream_id,
                        c16_t *out)
{
  c16_t w[ORU_CODEBOOK_MAX_NB_TX];
  bool warned = false;
  const int nb_ant = codebook_ul_weights(cb, nb_rx, beam_id, stream_id, w, &warned);
  combine_ul_weights(fft_data, nb_ant, k0, nk, w, out);
}

// Antenna count of a UL run whose bfw_run() *num_w was num_w, with its weights in w: the dynamic ones (at most nb_rx),
// none if they were overwritten, or the codebook weights of beam_id
static int ul_run_antennas(const oru_codebook_t *cb, int nb_rx, int num_w, int beam_id, int stream_id, c16_t *w, bool *warned)
{
  if (num_w < 0)
    return 0; // combine_ul_weights() zeroes the run
  if (num_w == 0)
    return codebook_ul_weights(cb, nb_rx, beam_id, stream_id, w, warned);
  return num_w < nb_rx ? num_w : nb_rx;
}

// combine_ul_weights() over contiguous REs [c0, c1) of an nbins-wide band, laid out as for combine_ul_beam_prbs()
static void combine_ul_weights_res(const c16_t *const fft_data[],
                                   int nb_ant,
                                   int fft_size,
                                   int nbins,
                                   int c0,
                                   int c1,
                                   const c16_t *w,
                                   c16_t *out)
{
  const int half = nbins / 2;
  if (c0 < half) { // negative-frequency half: the tail of the FFT window
    const int end = c1 < half ? c1 : half;
    combine_ul_weights(fft_data, nb_ant, fft_size - half + c0, end - c0, w, out);
  }
  if (c1 > half) { // DC and positive-frequency half: the head of the FFT window
    const int lo = c0 > half ? c0 : half;
    combine_ul_weights(fft_data, nb_ant, lo - half, c1 - lo, w, out);
  }
}

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
                          c16_t *out)
{
  const int n_rb = nbins / NR_NB_SC_PER_RB;
  int end = start_prb + num_prb;
  end = end < n_rb ? end : n_rb;
  const oru_bfw_set_t no_bfw = {0};
  if (stream_id >= cb->nb_fh_streams)
    bfw = NULL; // codebook_ul_weights() drops the stream
  bool warned = false; // once per job, not per bundle
  for (int prb = start_prb; prb < end;) {
    int bidx = beam_id;
    c16_t w[ORU_MAX_BF_WEIGHTS];
    int num_w;
    const int run_end = bfw_run(bfw, set ? set : &no_bfw, prb, end, &bidx, w, &num_w);
    const int nb_ant = ul_run_antennas(cb, nb_rx, num_w, bidx, stream_id, w, &warned);
    combine_ul_weights_res(fft_data, nb_ant, fft_size, nbins, prb * NR_NB_SC_PER_RB, run_end * NR_NB_SC_PER_RB, w, out);
    prb = run_end;
  }
}

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
                           c16_t *out)
{
  const int end = start_prb + (re_offset + nk + NR_NB_SC_PER_RB - 1) / NR_NB_SC_PER_RB;
  const oru_bfw_set_t no_bfw = {0};
  if (stream_id >= cb->nb_fh_streams)
    bfw = NULL; // codebook_ul_weights() drops the stream
  bool warned = false; // once per section, not per bundle
  for (int prb = start_prb; prb < end;) {
    int bidx = beam_id;
    c16_t w[ORU_MAX_BF_WEIGHTS];
    int num_w;
    const int run_end = bfw_run(bfw, set ? set : &no_bfw, prb, end, &bidx, w, &num_w);
    const int nb_ant = ul_run_antennas(cb, nb_rx, num_w, bidx, stream_id, w, &warned);
    // Bins of the run's PRBs
    int k0 = (prb - start_prb) * NR_NB_SC_PER_RB - re_offset;
    int k1 = (run_end - start_prb) * NR_NB_SC_PER_RB - re_offset;
    k0 = k0 > 0 ? k0 : 0;
    k1 = k1 < nk ? k1 : nk;
    if (k1 > k0)
      combine_ul_weights(rxdataF, nb_ant, k0, k1 - k0, w, out);
    prb = run_end;
  }
}
