/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "split7_ofdm.h"
#include <string.h>
#include "PHY/TOOLS/tools_defs.h"
#include "PHY/MODULATION/nr_modulation.h"

void split7_ofdm_mod(const NR_DL_FRAME_PARMS *fp,
                     enum nr_Link link,
                     int n_rb,
                     int slot,
                     int symbol,
                     const c16_t *in,
                     c16_t *scratch,
                     c16_t *out)
{
  const int fft_size = fp->ofdm_symbol_size;
  const int n_re = n_rb * NR_NB_SC_PER_RB;
  const int cp = split7_symbol_samples(fp, slot, symbol) - fft_size;
  const c16_t rot = fp->symbol_rotation[link][(slot % fp->slots_per_subframe) * fp->symbols_per_slot + symbol];

  rotate_cpx_vector(in, rot, scratch, n_re, 15);
  memset(scratch + n_re, 0, (fft_size - n_re) * sizeof(c16_t));
  fftshift_inverse_inplace(scratch, n_re, fft_size);
  c16_t *body = out + cp;
  if ((uintptr_t)body % 32 == 0) {
    idft(get_idft(fft_size), (int16_t *)scratch, (int16_t *)body, 1);
  } else {
    c16_t tmp[fft_size] __attribute__((aligned(64)));
    idft(get_idft(fft_size), (int16_t *)scratch, (int16_t *)tmp, 1);
    memcpy(body, tmp, sizeof(tmp));
  }
  memcpy(out, body + fft_size - cp, cp * sizeof(c16_t));
}

void split7_ofdm_demod(const NR_DL_FRAME_PARMS *fp, enum nr_Link link, int n_rb, int slot, int symbol, const c16_t *in, c16_t *out)
{
  const int fft_size = fp->ofdm_symbol_size;
  const int cp = split7_symbol_samples(fp, slot, symbol) - fft_size;
  const int window = cp - fp->nb_prefix_samples / fp->ofdm_offset_divisor;

  dft(get_dft(fft_size), (int16_t *)(in + window), (int16_t *)out, 1);
  fftshift_inplace(out, n_rb * NR_NB_SC_PER_RB, fft_size);
  apply_nr_rotation_symbol_fftshifted_RX(fp->symbols_per_slot,
                                         fp->slots_per_subframe,
                                         fp->timeshift_symbol_rotation,
                                         out,
                                         fp->symbol_rotation[link],
                                         n_rb,
                                         slot,
                                         symbol);
}

void split7_ofdm_window_mod(const c16_t *bins, int dftlen, int ncp, int reps, c16_t *scratch, c16_t *out)
{
  idft(get_idft(dftlen), (int16_t *)bins, (int16_t *)scratch, 1);
  memcpy(out, scratch + dftlen - ncp, ncp * sizeof(c16_t));
  for (int i = 0; i < reps; i++)
    memcpy(out + ncp + i * dftlen, scratch, dftlen * sizeof(c16_t));
}
