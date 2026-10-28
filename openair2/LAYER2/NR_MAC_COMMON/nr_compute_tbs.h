/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_COMPUTE_TBS_H
#define NR_COMPUTE_TBS_H

#include <stdint.h>

/* transport block size (TS 38.214 5.1.3.2 and 6.1.4.2), in bits: in its own library (nr_compute_tbs) so that the PHY
 * can size its buffers without linking the MAC */
uint32_t nr_compute_tbs(uint16_t Qm,
                        uint16_t R,
                        uint16_t nb_rb,
                        uint16_t nb_symb_sch,
                        uint16_t nb_dmrs_prb,
                        uint16_t nb_rb_oh,
                        uint8_t tb_scaling,
                        uint8_t Nl);
/// TBS_LBRM of the limited buffer rate matching (TS 38.212 5.4.2.1)
uint32_t nr_compute_tbslbrm(uint16_t table, uint16_t nb_rb, uint8_t Nl);

#endif
