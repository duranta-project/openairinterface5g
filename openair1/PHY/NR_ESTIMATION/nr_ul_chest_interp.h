/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_UL_CHEST_INTERP_H
#define NR_UL_CHEST_INTERP_H

#include <stdint.h>
#include "PHY/TOOLS/tools_defs.h"

// One filter tap applied to a pilot, as c16multaddVectRealComplex() does: sat16(2 * mulhrs(c, tap))
static inline int16_t nr_chest_tap(int16_t c, int16_t tap)
{
  const int32_t m = ((int32_t)c * tap + (1 << 14)) >> 15;
  const int32_t d = 2 * (int32_t)(int16_t)m;
  return d > INT16_MAX ? INT16_MAX : d < INT16_MIN ? INT16_MIN : (int16_t)d;
}

static inline int16_t nr_chest_sat_add(int16_t a, int16_t b)
{
  const int32_t s = (int32_t)a + b;
  return s > INT16_MAX ? INT16_MAX : s < INT16_MIN ? INT16_MIN : (int16_t)s;
}

/*
 * PUSCH DMRS type 1 frequency-domain interpolation: the filt16_ul_* sequence of nr_pusch_antenna_processing() applied to
 * P = 6 * nb_rb pilots, pilot p = (ls_est[2p] * delay[2p]) >> 8, into ul_ch[0 .. 12 * nb_rb - 1] (zeroed by the caller).
 * Pilot 0 (filt16_ul_p0) and pilots 1, 2 (filt16_ul_p1p2) write at RE 0, the last one (filt16_ul_last) at 2P - 8, and
 * middle pilots 2m+1 and 2m+2 (filt16_ul_middle, 16 taps of 2048) at 4(m - 1). An interior block of 4 REs b, 3 <= b <=
 * 3 nb_rb - 3, thus gets the 8 middle pilots 2b-3 .. 2b+4 with the same weight: the sum of 4 consecutive pilot pairs.
 * Each term is within +-4096, so no partial sum of up to 7 terms saturates and the saturating sequence of the per-pilot
 * calls equals sat16 of the plain sum. The 3 first and 2 last blocks replay the calls in their order, including their
 * writes past the allocation, up to RE 2P + 7.
 */
static inline void nr_pusch_dmrs_type1_interp(const c16_t *ls_est, const c16_t *delay, int nb_rb, c16_t *ul_ch)
{
  static const int16_t tap_p0[16] = {4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 0, 0, 0, 0, 0, 0, 0, 0};
  static const int16_t tap_p1p2[16] = {4096, 4096, 4096, 4096, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 0, 0, 0, 0};
  static const int16_t tap_last[16] = {4096, 4096, 4096, 4096, 8192, 8192, 8192, 8192, 0, 0, 0, 0, 0, 0, 0, 0};
  const int P = 6 * nb_rb, B = P / 2, n_re = 2 * P;
  c16_t pil[P];
  for (int p = 0; p < P; p++)
    pil[p] = c16mulShift(ls_est[2 * p], delay[2 * p], 8);

  // interior blocks
  if (B >= 6) {
    const int16_t *pf = (const int16_t *)pil;
    int16_t g[2 * P];
    for (int k = 0; k < 2 * P; k++)
      g[k] = nr_chest_tap(pf[k], 2048);
    uint32_t *out = (uint32_t *)ul_ch;
    for (int b = 3; b <= B - 3; b++) {
      const int16_t *w = g + 2 * (2 * b - 3);
      int32_t r = 0, i = 0;
      for (int t = 0; t < 8; t++) {
        r += w[2 * t];
        i += w[2 * t + 1];
      }
      const c16_t v = {.r = (int16_t)(r > INT16_MAX ? INT16_MAX : r < INT16_MIN ? INT16_MIN : r),
                       .i = (int16_t)(i > INT16_MAX ? INT16_MAX : i < INT16_MIN ? INT16_MIN : i)};
      const uint32_t v32 = *(const uint32_t *)&v;
      out[4 * b] = out[4 * b + 1] = out[4 * b + 2] = out[4 * b + 3] = v32;
    }
  }

  // edge blocks (all blocks if the allocation is too small for an interior), replaying the calls in pilot order
  for (int p = 0; p < P; p++) {
    // pilots 9 .. P-13 only reach interior blocks
    if (B >= 6 && p == 9 && P - 12 > 9)
      p = P - 12;
    const int16_t *tap;
    int off;
    if (p == 0) {
      tap = tap_p0;
      off = 0;
    } else if (p <= 2) {
      tap = tap_p1p2;
      off = 0;
    } else if (p == P - 1) {
      tap = tap_last;
      off = n_re - 8;
    } else {
      tap = NULL;
      off = 4 * ((p - 1) / 2 - 1);
    }
    for (int k = 0; k < 16; k++) {
      const int j = off + k;
      if (B >= 6 && j >= 12 && j < n_re - 8)
        continue;
      const int16_t t = tap ? tap[k] : 2048;
      ul_ch[j].r = nr_chest_sat_add(ul_ch[j].r, nr_chest_tap(pil[p].r, t));
      ul_ch[j].i = nr_chest_sat_add(ul_ch[j].i, nr_chest_tap(pil[p].i, t));
    }
  }
}

#endif
