
/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/** @addtogroup _PHY_TRANSPORT_
 * @{
 */

#ifndef __NR_TRANSPORT_COMMON_PROTO__H__
#define __NR_TRANSPORT_COMMON_PROTO__H__

#include "PHY/defs_nr_common.h"

void nr_group_sequence_hopping(pucch_GroupHopping_t PUCCH_GroupHopping,
                               uint32_t n_id,
                               int n_hop,
                               int nr_slot_tx,
                               uint8_t *u,
                               uint8_t *v);

double nr_cyclic_shift_hopping(uint32_t n_id,
                               uint8_t m0,
                               uint8_t mcs,
                               uint8_t lnormal,
                               uint8_t lprime,
                               int nr_slot_tx);

/** \brief Computes available bits G. */
uint32_t nr_get_G(uint16_t nb_rb,
                  uint16_t nb_symb_sch,
                  uint8_t nb_re_dmrs,
                  uint16_t length_dmrs,
                  uint32_t unav_res,
                  uint8_t Qm,
                  uint8_t Nl);

int nr_get_E(uint32_t G, uint8_t C, uint8_t Qm, uint8_t Nl, uint8_t r);

void compute_nr_prach_seq(uint8_t short_sequence, uint8_t num_sequences, uint8_t rootSequenceIndex, c16_t X_u[64][839]);

void nr_fill_du(uint16_t N_ZC, const uint16_t *prach_root_sequence_map, uint16_t nr_du[NR_PRACH_SEQ_LEN_L - 1]);

void nr_codeword_scrambling(uint8_t *in,
                            uint32_t size,
                            uint8_t q,
                            uint32_t Nid,
                            uint32_t n_RNTI,
                            uint32_t* out);

void nr_codeword_unscrambling(int16_t* llr, uint32_t size, uint8_t q, uint32_t Nid, uint32_t n_RNTI);
void nr_codeword_unscrambling_init(int16_t *s, uint32_t size, uint8_t q, uint32_t Nid, uint32_t n_RNTI);

/**@}*/

void init_pucch2_3_luts(void);
void set_prach_tables(int N_ZC, c16_t** ru, uint32_t** zc_inv);
#endif
