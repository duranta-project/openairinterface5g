/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/*! \file PHY/NR_UE_TRANSPORT/pucch_nr.c
* \brief Top-level routines for generating and decoding the PSFCH physical channel
* \author R. Knopp 
* \date 2023
* \version 0.1
* \company Eurecom
* \email:
* \note
* \warning
*/
//#include "PHY/defs.h"
#include "PHY/impl_defs_nr.h"
#include "PHY/defs_nr_common.h"
#include "PHY/defs_nr_UE.h"
//#include "PHY/extern.h"
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
  nr_generate_pucch0(txdataF, frame_parms, amp, nr_slot_tx, &pucch_pdu);
}
