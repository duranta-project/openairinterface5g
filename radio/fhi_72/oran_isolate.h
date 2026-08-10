/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _ORAN_ISOLATE_H_
#define _ORAN_ISOLATE_H_

#include <stdio.h>

#include <pthread.h>
#include <stdint.h>

#include "xran_fh_o_du.h"
#include "openair1/PHY/impl_defs_nr.h"
#include "openair1/PHY/TOOLS/tools_defs.h"
#include "openair1/PHY/defs_nr_common.h"
#include "openair1/PHY/NR_TRANSPORT/nr_transport_proto.h"

/*
 * Structure added to bear the information needed from OAI RU
 */
typedef struct ru_info_s {
  // Needed for UL
  int nb_rx;
  int32_t **rxdataF;

  // Needed for DL
  int nb_tx;
  int32_t **txdataF_BF;

  /// \brief Anaglogue beam ID for each OFDM symbol (used when beamforming not done in RU)
  /// - first index: symbol index [0.. symbols_per_frame)
  /// - second index: beam_id [0..num_ports)
  uint16_t **beam_id;

} ru_info_t;

void print_fhi_counters(ru_info_t *ru, const int frame, const int slot);

/** @brief Reads RX data PUSCH of next slot.
 *
 * @param ru pointer to structure keeping pointers to OAI data.
 * @param frame output of the frame which has been read.
 * @param slot output of the slot which has been read. */
int xran_fh_rx_read_slot(ru_info_t *ru, int *frame, int *slot);
/** @brief Writes CP UL data for given slot. */
int xran_send_cp_ul_slot(ru_info_t *ru, int frame, int slot);
/** @brief Writes TX data (PDSCH) of given slot. */
int xran_fh_tx_send_slot(ru_info_t *ru, int frame, int slot, uint64_t timestamp);

#endif /* _ORAN_ISOLATE_H_ */
