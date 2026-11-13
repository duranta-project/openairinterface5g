/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
*/
#include <string.h>
#include "PHY/impl_defs_nr.h"
#include "PHY/defs_nr_common.h"
#include "PHY/defs_nr_UE.h"
#include "PHY/NR_UE_TRANSPORT/pucch_nr.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_proto_ue.h"
#include "PHY/NR_TRANSPORT/nr_transport_common_proto.h"
#include <openair1/PHY/CODING/nrSmallBlock/nr_small_block_defs.h>
#include "common/utils/LOG/log.h"
#include "common/utils/LOG/vcd_signal_dumper.h"

#include "T.h"


void nr_generate_psfch0(const PHY_VARS_NR_UE *ue,
                        c16_t **txdataF,
                        const NR_DL_FRAME_PARMS *frame_parms,
                        const int16_t amp,
                        const int nr_slot_tx,
                        const sl_nr_tx_rx_config_psfch_pdu_t *psfch_pdu)
{

  fapi_nr_ul_config_pucch_pdu pucch_pdu;

  pucch_pdu.start_symbol_index   = psfch_pdu->start_symbol_index;
  pucch_pdu.hopping_id           = psfch_pdu->hopping_id;
  pucch_pdu.prb_start            = psfch_pdu->prb;
  pucch_pdu.initial_cyclic_shift = psfch_pdu->initial_cyclic_shift;
  pucch_pdu.mcs                  = psfch_pdu->mcs;
  pucch_pdu.nr_of_symbols        = psfch_pdu->nr_of_symbols;
  pucch_pdu.n_bit                = psfch_pdu->bit_len_harq;
  pucch_pdu.bwp_start            = psfch_pdu->sl_bwp_start;
  pucch_pdu.freq_hop_flag        = psfch_pdu->freq_hop_flag;
  pucch_pdu.group_hop_flag       = psfch_pdu->group_hop_flag;
  pucch_pdu.second_hop_prb       = psfch_pdu->second_hop_prb;
  pucch_pdu.sequence_hop_flag    = psfch_pdu->sequence_hop_flag;
  const int start_sc = NR_NB_SC_PER_RB * (psfch_pdu->sl_bwp_start + psfch_pdu->prb);
  if (psfch_pdu->nr_of_symbols == 1) {
    AssertFatal(psfch_pdu->start_symbol_index > 0 && psfch_pdu->start_symbol_index < frame_parms->symbols_per_slot,
                "Invalid PSFCH symbol %d\n", psfch_pdu->start_symbol_index);
    AssertFatal(start_sc >= 0 && start_sc + NR_NB_SC_PER_RB <= frame_parms->ofdm_symbol_size,
                "Invalid PSFCH subcarrier %d\n", start_sc);
  }

  nr_generate_pucch0(txdataF, frame_parms, amp, nr_slot_tx, &pucch_pdu);

  // TS 38.211 section 8.3.4.2.2: duplicate the mapped PSFCH REs in the preceding symbol.
  if (psfch_pdu->nr_of_symbols == 1) {
    const int fft_size = frame_parms->ofdm_symbol_size;
    const int symbol = psfch_pdu->start_symbol_index;
    memcpy(&txdataF[0][(symbol - 1) * fft_size + start_sc],
           &txdataF[0][symbol * fft_size + start_sc],
           NR_NB_SC_PER_RB * sizeof(c16_t));
  }
}
