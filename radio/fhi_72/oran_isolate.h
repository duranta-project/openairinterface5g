/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _ORAN_ISOLATE_H_
#define _ORAN_ISOLATE_H_

#include <stdio.h>

#include <pthread.h>
#include <stdint.h>

#include "xran_fh_o_du.h"
#include "oran-init.h"
#include "openair1/PHY/impl_defs_nr.h"
#include "openair1/PHY/TOOLS/tools_defs.h"
#include "openair1/PHY/defs_nr_common.h"
#include "openair1/PHY/NR_TRANSPORT/nr_transport_proto.h"

void print_fhi_counters(struct xran_fh_init fh_init, struct xran_fh_config *fh_config);

/** @brief Reads PUSCH data of one antenna from its slot buffer dstcp into rxdataF_ant (the slot's position in rxdataF). */
void xran_fh_rx_read_slot_ant(const int fft_size, struct xran_buffer_list *dstcp, int32_t *rxdataF_ant);
/** @brief Reads PRACH data of one antenna from its slot buffer prachdstdecomp into prach_dst, skipping kbar int16 of the PRACH PRBs. */
void xran_fh_rx_prach_read_slot_ant(const struct xran_ru_config *ru_conf, const struct xran_prach_config *prach_conf, const int kbar, struct xran_buffer_list *prachdstdecomp, c16_t *prach_dst);
/** @brief Writes CP DL/UL data for given slot. */
int xran_send_cp_slot(const int tti, const int slot, const int xran_port, const uint8_t nb_ant, uint16_t **beams, struct xran_buffer_list buf[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN]);
/** @brief Writes TX data (PDSCH) of one antenna into its slot buffers srccp (PRB map) and src (U-plane). */
void xran_fh_tx_send_slot_ant(const int fft_size, int32_t *txdataF_ant, struct xran_buffer_list *srccp, struct xran_buffer_list *src);

#endif /* _ORAN_ISOLATE_H_ */
