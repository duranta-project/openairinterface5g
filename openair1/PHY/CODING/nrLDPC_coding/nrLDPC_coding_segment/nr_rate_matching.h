/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef __NR_RATE_MATCHING__H__
#define __NR_RATE_MATCHING__H__

#include <stdint.h>

/// Mother code extent N, LBRM soft buffer limit Ncb and RV start k0 of a code segment (38.212 5.4.2.1)
typedef struct {
  uint32_t N;
  uint32_t Ncb;
  uint32_t k0;
} nr_ldpc_geometry_t;

/* static inline because the rate matching sources link only into the dlopen'd LDPC modules, while the
 * DLSCH receive path needs the same geometry. Tbslbrm is in bits, despite the FAPI field name. */
static inline nr_ldpc_geometry_t nr_ldpc_soft_buffer_geometry(uint32_t Tbslbrm, uint8_t BG, uint16_t Z, uint8_t C, uint8_t rvidx)
{
  static const uint8_t index_k0[2][4] = {{0, 17, 33, 56}, {0, 13, 25, 43}}; // 38.212 table 5.4.2.1-2, in units of Zc
  const uint32_t N = (BG == 1) ? 66 * Z : 50 * Z;
  const uint32_t Nref = (Tbslbrm && C) ? 3 * Tbslbrm / (2 * C) : N; // R_LBRM = 2/3
  const uint32_t Ncb = Nref < N ? Nref : N;
  return (nr_ldpc_geometry_t){.N = N, .Ncb = Ncb, .k0 = (index_k0[BG - 1][rvidx & 3] * Ncb / N) * Z};
}

/**
 * \brief interleave a code segment after encoding and rate matching
 * \param E size of the code segment in bits
 * \param Qm modulation order
 * \param e input rate matched segment
 * \param f output interleaved segment
 */
void nr_interleaving_ldpc(uint32_t E, uint8_t Qm, uint8_t *e, uint8_t *f);

/**
 * \brief deinterleave a code segment before RX rate matching and decoding
 * \param E size of the code segment in bits
 * \param Qm modulation order
 * \param e output deinterleaved segment
 * \param f input llr segment
 */
void nr_deinterleaving_ldpc(uint32_t E, uint8_t Qm, int16_t *e, int16_t *f);

/**
 * \brief rate match a code segment after encoding
 * \Tbslbrm Transport Block size LBRM
 * \param BG LDPC base graph number
 * \param Z segment lifting size
 * \param d input encoded segment
 * \param e output rate matched segment
 * \param C number of segments in the Transport Block
 * \param F number of filler bits in the segment
 * \param Foffset offset of the filler bits in the segment
 * \param rvidx redundancy version index
 * \param E size of the code segment in bits
 */
int nr_rate_matching_ldpc(uint32_t Tbslbrm,
                          uint8_t BG,
                          uint16_t Z,
                          uint8_t *d,
                          uint8_t *e,
                          uint8_t C,
                          uint32_t F,
                          uint32_t Foffset,
                          uint8_t rvidx,
                          uint32_t E);

/**
 * \brief rate match a code segment before decoding
 * \Tbslbrm Transport Block size LBRM
 * \param BG LDPC base graph number
 * \param Z segment lifting size
 * \param d output rate matched segment
 * \param soft_input input deinterleaved segment
 * \param C number of segments in the Transport Block
 * \param rvidx redundancy version index
 * \param clear flag to clear d on the first round of a new HARQ process
 * \param E size of the code segment in bits
 * \param F number of filler bits in the segment
 * \param Foffset offset of the filler bits in the segment
 */
int nr_rate_matching_ldpc_rx(uint32_t Tbslbrm,
                             uint8_t BG,
                             uint16_t Z,
                             int16_t *d,
                             int16_t *soft_input,
                             uint8_t C,
                             uint8_t rvidx,
                             uint8_t clear,
                             uint32_t E,
                             uint32_t F,
                             uint32_t Foffset);

#endif
