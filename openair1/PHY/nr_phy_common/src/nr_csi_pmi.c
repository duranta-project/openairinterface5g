/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_csi_pmi.h"
#include <complex.h>
#include <math.h>

// Largest N1*O1*N2*O2 over the (N1, N2) of TS 38.214 Table 5.2.2.2.1-2 with 4 and 8 ports: (2, 2)
#define MAX_BEAMS 64

typedef struct {
  double complex v[NR_CSI_PMI_MAX_PORTS / 2]; // DFT beam v_lm, |v_lm[i]| = 1
  double complex r_top[NR_CSI_PMI_MAX_PORTS]; // R * [v_lm; 0]
  double complex r_bottom[NR_CSI_PMI_MAX_PORTS]; // R * [0; v_lm]
} beam_t;

// k1, k2 of TS 38.214 Table 5.2.2.2.1-3, in units of O1 and O2 (N2 = 1 or N1 = N2 with 4 and 8 ports)
static void rank2_beam_offset(int n2, int i13, int *k1, int *k2)
{
  if (n2 == 1) {
    *k1 = i13;
    *k2 = 0;
  } else {
    *k1 = i13 & 1;
    *k2 = i13 >> 1;
  }
}

// w^H y with w = [v; phi * v]
static double complex precoded_dot(const beam_t *b, double complex phi, int half, const double complex *y)
{
  double complex top = 0, bottom = 0;
  for (int i = 0; i < half; i++) {
    top += conj(b->v[i]) * y[i];
    bottom += conj(b->v[i]) * y[half + i];
  }
  return top + conj(phi) * bottom;
}

// w1^H R w2 with w1 = [v1; phi1 * v1], w2 = [v2; phi2 * v2]
static double complex precoded_form(const beam_t *b1, double complex phi1, const beam_t *b2, double complex phi2, int ports)
{
  double complex y[NR_CSI_PMI_MAX_PORTS];
  for (int i = 0; i < ports; i++)
    y[i] = b2->r_top[i] + phi2 * b2->r_bottom[i];
  return precoded_dot(b1, phi1, ports / 2, y);
}

// exp(j 2 pi k / 16): every DFT beam phase for (N1, N2) = (2, 1), (2, 2), (4, 1) with O1 = O2 = 4
#define NUM_PHASES 16
static const double complex exp_2pi_16[NUM_PHASES] = {
    1 + 0 * I,
    0.92387953251128674 + 0.38268343236508976 * I,
    0.70710678118654752 + 0.70710678118654752 * I,
    0.38268343236508976 + 0.92387953251128674 * I,
    0 + 1 * I,
    -0.38268343236508976 + 0.92387953251128674 * I,
    -0.70710678118654752 + 0.70710678118654752 * I,
    -0.92387953251128674 + 0.38268343236508976 * I,
    -1 + 0 * I,
    -0.92387953251128674 - 0.38268343236508976 * I,
    -0.70710678118654752 - 0.70710678118654752 * I,
    -0.38268343236508976 - 0.92387953251128674 * I,
    0 - 1 * I,
    0.38268343236508976 - 0.92387953251128674 * I,
    0.70710678118654752 - 0.70710678118654752 * I,
    0.92387953251128674 - 0.38268343236508976 * I,
};

static bool beam_allowed(uint64_t codebook_subset, int beam)
{
  return (codebook_subset >> beam) & 1;
}

int nr_csi_restrict_rank(int rank, uint8_t ri_restriction)
{
  if ((ri_restriction >> (rank - 1)) & 1)
    return rank;
  for (int r = rank - 1; r >= 1; r--)
    if ((ri_restriction >> (r - 1)) & 1)
      return r;
  for (int r = rank + 1; r <= 8; r++)
    if ((ri_restriction >> (r - 1)) & 1)
      return r;
  return 1;
}

static int to_db(double x)
{
  return x > 1e-3 ? (int)lround(10 * log10(x)) : -30;
}

bool nr_csi_pmi_type1_single_panel(int n1,
                                   int n2,
                                   int rank,
                                   const c64_t *R,
                                   int num_re,
                                   uint32_t noise,
                                   uint64_t codebook_subset,
                                   nr_csi_pmi_t *pmi,
                                   int *sinr_db)
{
  const int ports = 2 * n1 * n2;
  // (N1, N2) of TS 38.214 Table 5.2.2.2.1-2 with 4 and 8 ports
  const bool supported = (n1 == 2 && n2 == 1) || (n1 == 2 && n2 == 2) || (n1 == 4 && n2 == 1);
  if (!supported || rank < 1 || rank > 2 || num_re <= 0)
    return false;
  const int half = ports / 2;
  const int o1 = 4;
  const int o2 = n2 > 1 ? 4 : 1;
  const int beams_1 = n1 * o1;
  const int beams_2 = n2 * o2;

  // Covariance per RE, full Hermitian matrix
  double complex Rd[NR_CSI_PMI_MAX_PORTS][NR_CSI_PMI_MAX_PORTS];
  for (int i = 0; i < ports; i++)
    for (int j = i; j < ports; j++) {
      const c64_t r = R[i * ports + j];
      Rd[i][j] = ((double)r.r + I * (double)r.i) / num_re;
      Rd[j][i] = conj(Rd[i][j]);
    }

  beam_t beam[MAX_BEAMS];
  for (int l = 0; l < beams_1; l++)
    for (int m = 0; m < beams_2; m++) {
      beam_t *b = &beam[l * beams_2 + m];
      for (int a1 = 0; a1 < n1; a1++)
        for (int a2 = 0; a2 < n2; a2++)
          b->v[a1 * n2 + a2] = exp_2pi_16[(l * a1 * (NUM_PHASES / beams_1) + m * a2 * (NUM_PHASES / beams_2)) % NUM_PHASES];
      for (int i = 0; i < ports; i++) {
        b->r_top[i] = 0;
        b->r_bottom[i] = 0;
        for (int k = 0; k < half; k++) {
          b->r_top[i] += Rd[i][k] * b->v[k];
          b->r_bottom[i] += Rd[i][half + k] * b->v[k];
        }
      }
    }

  static const double complex phi[4] = {1, I, -1, -I};
  const double n = noise > 0 ? noise : 1;
  *pmi = (nr_csi_pmi_t){0};

  if (rank == 1) {
    // Table 5.2.2.2.1-5: W = [v_lm; phi_n v_lm] / sqrt(P)
    double best = -1;
    for (int l = 0; l < beams_1; l++)
      for (int m = 0; m < beams_2; m++) {
        if (!beam_allowed(codebook_subset, l * beams_2 + m))
          continue;
        const beam_t *b = &beam[l * beams_2 + m];
        for (int i2 = 0; i2 < 4; i2++) {
          const double power = creal(precoded_form(b, phi[i2], b, phi[i2], ports)) / ports;
          if (power > best) {
            best = power;
            *pmi = (nr_csi_pmi_t){.i_1_1 = l, .i_1_2 = m, .i_2 = i2};
          }
        }
      }
    if (best < 0)
      return false;
    *sinr_db = to_db(best / n);
    return true;
  }

  // Table 5.2.2.2.1-6: W = [v_lm, v_l'm'; phi_n v_lm, -phi_n v_l'm'] / sqrt(2P)
  const int num_i13 = (n1 == 2 && n2 == 1) ? 2 : 4;
  double best = -1;
  for (int l = 0; l < beams_1; l++)
    for (int m = 0; m < beams_2; m++)
      for (int i13 = 0; i13 < num_i13; i13++) {
        int k1, k2;
        rank2_beam_offset(n2, i13, &k1, &k2);
        const int beam1 = l * beams_2 + m;
        const int beam2 = ((l + k1 * o1) % beams_1) * beams_2 + (m + k2 * o2) % beams_2;
        // a precoder of two beams is based on both
        if (!beam_allowed(codebook_subset, beam1) || !beam_allowed(codebook_subset, beam2))
          continue;
        const beam_t *b1 = &beam[beam1];
        const beam_t *b2 = &beam[beam2];
        for (int i2 = 0; i2 < 2; i2++) {
          const double g11 = creal(precoded_form(b1, phi[i2], b1, phi[i2], ports)) / (2 * ports * n);
          const double g22 = creal(precoded_form(b2, -phi[i2], b2, -phi[i2], ports)) / (2 * ports * n);
          const double complex g12 = precoded_form(b1, phi[i2], b2, -phi[i2], ports) / (2 * ports * n);
          const double det = (1 + g11) * (1 + g22) - creal(g12 * conj(g12));
          if (det > best) {
            best = det;
            *pmi = (nr_csi_pmi_t){.i_1_1 = l, .i_1_2 = m, .i_1_3 = i13, .i_2 = i2};
          }
        }
      }
  if (best < 0)
    return false;
  // Equal per-layer SINR with the same capacity
  *sinr_db = to_db(sqrt(best) - 1);
  return true;
}
