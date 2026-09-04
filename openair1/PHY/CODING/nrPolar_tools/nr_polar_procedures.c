/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "PHY/CODING/nrPolar_tools/nr_polar_defs.h"
#include "bits.h"
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

// TS 38.212 - Section 5.3.1.2 Polar encoding
void nr_polar_generate_u(uint64_t *u,
                         const uint64_t *Cprime,
                         const uint8_t *information_bit_pattern,
                         const uint8_t *parity_check_bit_pattern,
                         uint16_t N,
                         uint8_t n_pc)
{
  int N_array = N >> 6;
  int k = 0;

  if (n_pc > 0) {
    uint64_t y0 = 0, y1 = 0, y2 = 0, y3 = 0, y4 = 0;

    for (int n = 0; n < N; n++) {
      uint64_t yt = y0;
      y0 = y1;
      y1 = y2;
      y2 = y3;
      y3 = y4;
      y4 = yt;

      if (information_bit_pattern[n] == 1) {
        int n1 = n >> 6;
        int n2 = n - (n1 << 6);
        if (parity_check_bit_pattern[n] == 1) {
          u[N_array - 1 - n1] |= y0 << (63 - n2);
        } else {
          int k1 = k >> 6;
          int k2 = k - (k1 << 6);
          uint64_t bit = (Cprime[k1] >> k2) & 1;
          u[N_array - 1 - n1] |= bit << (63 - n2);
          k++;
          y0 = y0 ^ (int)bit;
        }
      }
    }

  } else {
    for (int n = 0; n < N; n++) {
      if (information_bit_pattern[n] == 1) {
        int n1 = n >> 6;
        int n2 = n - (n1 << 6);
        int k1 = k >> 6;
        int k2 = k - (k1 << 6);
        uint64_t bit = (Cprime[k1] >> k2) & 1;
        u[N_array - 1 - n1] |= bit << (63 - n2);
        k++;
      }
    }
  }
}

static void encode_packed_byte(uint8_t *in_out)
{
  //    *in_out ^= 0xAA & (*in_out << 1);
  //    *in_out ^= 0xCC & (*in_out << 2);
  //    *in_out ^= *in_out  in_out << 4;
  //
  *in_out ^= 0x55 & (*in_out >> 1);
  *in_out ^= 0x33 & (*in_out >> 2);
  *in_out ^= *in_out >> 4;
}

static void polar_encode_bits(uint8_t *in_out, size_t N)
{
  size_t const num_bytes_per_block = N >> 3;
  for (size_t i = 0; i < num_bytes_per_block; ++i) {
    encode_packed_byte(&in_out[i]);
  }
}

static uint32_t log2_floor(uint32_t x)
{
  if (x == 0)
    return 0;
  uint32_t clz = __builtin_clz(x);
  return 31U - clz;
}

static void polar_encode_bytes(uint8_t *in_out, size_t N)
{
  size_t brnch_sz = 1;
  size_t n_brnch = N >> 4;
  size_t const blck_pwr = log2_floor(N);
  for (size_t stage = 3; stage < blck_pwr; ++stage) {
    for (size_t brnch = 0; brnch < n_brnch; ++brnch) {
      for (size_t byte = 0; byte < brnch_sz; ++byte) {
        size_t const dst = 2 * brnch_sz * brnch + byte;
        in_out[dst] ^= in_out[dst + brnch_sz];
      }
    }
    n_brnch >>= 1;
    brnch_sz <<= 1;
  }
}

void nr_polar_uxG(uint8_t const *u, size_t N, uint8_t *D2)
{
  assert(N > 7);
  uint8_t tmp[N / 8];
  for (int i = 0; i < N / 8; ++i)
    tmp[i] = u[N / 8 - 1 - i];

  reverse_bits_u8(tmp, N / 8, D2);

  // Do the encoding/xor for the bottom 3 levels of the tree.
  // Thus, data remaining to encode, is in 2^3 type.
  polar_encode_bits(D2, N);
  // Xor the remaining tree levels. Use bytes for it
  polar_encode_bytes(D2, N);
}

void nr_polar_bit_insertion(uint8_t *input, uint8_t *output, uint16_t N, uint16_t K, int16_t *Q_I_N, int16_t *Q_PC_N, uint8_t n_PC)
{
  int k = 0;

  if (n_PC > 0) {
    /*
     *
     */
  } else {
    for (int n = 0; n <= N - 1; n++) {
      output[n] = 0;
      for (int m = 0; m <= (K + n_PC) - 1; m++) {
        if (n == Q_I_N[m]) {
          output[n] = input[k];
          k++;
          break;
        }
      }
    }
  }
}

uint32_t nr_polar_output_length(uint16_t K, uint16_t E, uint8_t n_max)
{
  int n_1, n_2, n_min = 5;
  double R_min = 1.0 / 8;

  if ((E <= (9.0 / 8) * pow(2, ceil(log2(E)) - 1)) && (K / E < 9.0 / 16)) {
    n_1 = ceil(log2(E)) - 1;
  } else {
    n_1 = ceil(log2(E));
  }

  n_2 = ceil(log2(K / R_min));

  int n = n_max;
  if (n > n_1)
    n = n_1;
  if (n > n_2)
    n = n_2;
  if (n < n_min)
    n = n_min;

  /*printf("nr_polar_output_length: K %d, E %d, n %d (n_max %d,n_min %d, n_1 %d,n_2 %d)\n",
   K,E,n,n_max,n_min,n_1,n_2);
   exit(-1);*/
  return ((uint32_t)pow(2.0, n)); //=polar_code_output_length
}

void nr_polar_info_bit_pattern(uint8_t *ibp,
                               uint8_t *pcbp,
                               int16_t *Q_I_N,
                               int16_t *Q_F_N,
                               int16_t *Q_PC_N,
                               const uint16_t *J,
                               const uint16_t *Q_0_Nminus1,
                               const uint16_t K,
                               const uint16_t N,
                               const uint16_t E,
                               const uint8_t n_PC,
                               const uint8_t n_pc_wm)
{
  int Q_Ftmp_N[N + 1]; // Last element shows the final
  int Q_Itmp_N[N + 1]; // array index assigned a value.

  for (int i = 0; i <= N; i++) {
    Q_Ftmp_N[i] = -1; // Empty array.
    Q_Itmp_N[i] = -1;
  }

  if (E < N) {
    if ((K / (double)E) <= (7.0 / 16)) { // puncturing
      for (int n = 0; n <= N - E - 1; n++) {
        int ind = Q_Ftmp_N[N] + 1;
        Q_Ftmp_N[ind] = J[n];
        Q_Ftmp_N[N] = Q_Ftmp_N[N] + 1;
      }

      if ((E / (double)N) >= (3.0 / 4)) {
        int limit = ceil((double)(3 * N - 2 * E) / 4);
        for (int n = 0; n <= limit - 1; n++) {
          int ind = Q_Ftmp_N[N] + 1;
          Q_Ftmp_N[ind] = n;
          Q_Ftmp_N[N] = Q_Ftmp_N[N] + 1;
        }
      } else {
        int limit = ceil((double)(9 * N - 4 * E) / 16);
        for (int n = 0; n <= limit - 1; n++) {
          int ind = Q_Ftmp_N[N] + 1;
          Q_Ftmp_N[ind] = n;
          Q_Ftmp_N[N] = Q_Ftmp_N[N] + 1;
        }
      }
    } else { // shortening
      for (int n = E; n <= N - 1; n++) {
        int ind = Q_Ftmp_N[N] + 1;
        Q_Ftmp_N[ind] = J[n];
        Q_Ftmp_N[N] = Q_Ftmp_N[N] + 1;
      }
    }
  }

  // Q_I,tmp_N = Q_0_N-1 \ Q_F,tmp_N
  for (int n = 0; n <= N - 1; n++) {
    const int end = Q_Ftmp_N[N];
    int m;
    for (m = 0; m <= end; m++) {
      AssertFatal(m < N + 1, "Buffer boundary overflow");
      if (Q_0_Nminus1[n] == Q_Ftmp_N[m]) {
        break;
      }
    }
    if (m > end) {
      Q_Itmp_N[Q_Itmp_N[N] + 1] = Q_0_Nminus1[n];
      Q_Itmp_N[N]++;
    }
  }

  // Q_I_N comprises (K+n_PC) most reliable bit indices in Q_I,tmp_N
  for (int n = 0; n <= (K + n_PC) - 1; n++) {
    int ind = Q_Itmp_N[N] + n - ((K + n_PC) - 1);
    Q_I_N[n] = Q_Itmp_N[ind];
  }

  if (n_PC > 0) {
    AssertFatal(n_pc_wm <= 1, "n_pc_wm = %i is not valid (only 0 and 1 allowed)\n", n_pc_wm);

    // As per 5.3.1.2 of 38.212, denote the set of bit indices for parity check bits as Q_PC_N
    // A number of (n_PC - n_pc_wm) parity check bits are placed in the (n_PC - n_pc_wm) least reliable bit indices
    for (int n = 0; n < n_PC - n_pc_wm; n++) {
      Q_PC_N[n] = Q_I_N[n];
    }

    // A number of n_pc_wm other parity check bits are placed in the bit indices of minimum row weight of most reliable bit indices
    if (n_pc_wm == 1) {
      // tilde{Q_I}^N = K most reliable indices of bar{Q_I}^N = Q_I_N[n_PC .. K+n_PC-1].
      // Find minimum row weight over this range; on ties, keep the most reliable
      // (later position, since Q_I_N is ascending by reliability).
      int min_weight = INT_MAX;
      int best_pos = -1;
      for (int n = n_PC; n <= (K + n_PC) - 1; n++) {
        const int w = __builtin_popcount((unsigned)Q_I_N[n]);
        if (w <= min_weight) {
          min_weight = w;
          best_pos = n;
        }
      }
      AssertFatal(best_pos >= 0, "No candidate found in tilde{Q_I}^N for n_pc_wm = 1 (K=%u, n_PC=%u)", K, n_PC);
      Q_PC_N[n_PC - 1] = Q_I_N[best_pos];
    }
  }

  // Q_F_N = Q_0_N-1 \ Q_I_N
  for (int n = 0; n <= N - 1; n++) {
    const int sz = (K + n_PC) - 1;
    const int toCmp = Q_0_Nminus1[n];
    int m;
    for (m = 0; m <= sz; m++)
      if (toCmp == Q_I_N[m])
        break;
    if (m > sz) {
      Q_F_N[Q_F_N[N] + 1] = toCmp;
      Q_F_N[N]++;
    }
  }

  // Information and Parity Check Bit Pattern
  for (int n = 0; n <= N - 1; n++) {
    ibp[n] = 0;
    for (int m = 0; m <= (K + n_PC) - 1; m++) {
      if (n == Q_I_N[m]) {
        ibp[n] = 1;
        break;
      }
    }

    pcbp[n] = 0;
    for (int m = 0; m < n_PC; m++) {
      if (n == Q_PC_N[m]) {
        pcbp[n] = 1;
        break;
      }
    }
  }
}

void nr_polar_info_bit_extraction(uint8_t *input, uint8_t *output, uint8_t *pattern, uint16_t size)
{
  uint16_t j = 0;
  for (int i = 0; i < size; i++) {
    if (pattern[i] > 0) {
      output[j] = input[i];
      j++;
    }
  }
}

void nr_polar_rate_matching_pattern(uint16_t *rmp, uint16_t *J, const uint8_t *P_i_, uint16_t K, uint16_t N, uint16_t E)
{
  uint16_t d[N];
  for (int m = 0; m < N; m++)
    d[m] = m;
  uint16_t y[N];
  memset(y, 0, sizeof(y));

  for (int m = 0; m <= N - 1; m++) {
    int i = floor((32 * m) / N);
    J[m] = (P_i_[i] * (N / 32)) + (m % (N / 32));
    y[m] = d[J[m]];
  }

  if (E >= N) { // repetition
    for (int k = 0; k <= E - 1; k++) {
      int ind = (k % N);
      rmp[k] = y[ind];
    }
  } else {
    if ((K / (double)E) <= (7.0 / 16)) { // puncturing
      for (int k = 0; k <= E - 1; k++) {
        rmp[k] = y[k + N - E];
      }
    } else { // shortening
      for (int k = 0; k <= E - 1; k++) {
        rmp[k] = y[k];
      }
    }
  }
}

void nr_polar_rate_matching(double *input, double *output, uint16_t *rmp, uint16_t K, uint16_t N, uint16_t E)
{
  if (E >= N) { // repetition
    for (int i = 0; i <= N - 1; i++)
      output[i] = 0;
    for (int i = 0; i <= E - 1; i++) {
      output[rmp[i]] += input[i];
    }
  } else {
    if ((K / (double)E) <= (7.0 / 16)) { // puncturing
      for (int i = 0; i <= N - 1; i++)
        output[i] = 0;
    } else { // shortening
      for (int i = 0; i <= N - 1; i++)
        output[i] = INFINITY;
    }

    for (int i = 0; i <= E - 1; i++) {
      output[rmp[i]] = input[i];
    }
  }
}
