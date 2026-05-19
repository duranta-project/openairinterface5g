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

/*! \file PHY/NR_UE_TRANSPORT/nr_pscch_tx.c
* \brief Top-level routines for generating and decoding the PSCCH physical channel
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

uint32_t nr_generate_sci1(const PHY_VARS_NR_UE *ue,
                          c16_t *txdataF,
                          const NR_DL_FRAME_PARMS *frame_parms,
                          const int16_t amp,
                          const int nr_slot_tx,
                          const sl_nr_tx_config_pscch_pssch_pdu_t *pscch_pssch_pdu)
{

  nfapi_nr_dl_tti_pdcch_pdu_rel15_t pdcch_pdu_rel15={0};
  // for SCI we put the startRB and number of RBs for PSCCH in the first 2 FAPI FreqDomainResource fields
  pdcch_pdu_rel15.FreqDomainResource[0]          = pscch_pssch_pdu->startrb;
  pdcch_pdu_rel15.FreqDomainResource[1]          = pscch_pssch_pdu->pscch_numrbs;
  pdcch_pdu_rel15.StartSymbolIndex               = 1;
  pdcch_pdu_rel15.DurationSymbols                = pscch_pssch_pdu->pscch_numsym;
  pdcch_pdu_rel15.numDlDci                       = 1;
  pdcch_pdu_rel15.dci_pdu[0].ScramblingId        = pscch_pssch_pdu->pscch_dmrs_scrambling_id;
  pdcch_pdu_rel15.dci_pdu[0].PayloadSizeBits     = pscch_pssch_pdu->pscch_sci_payload_len;
  // for SCI we put the number of PRBs in the FAPI AggregationLevel field
  pdcch_pdu_rel15.dci_pdu[0].AggregationLevel    = pscch_pssch_pdu->pscch_numrbs*pscch_pssch_pdu->pscch_numsym;
  pdcch_pdu_rel15.dci_pdu[0].ScramblingRNTI      = 1010;
  *(uint64_t*)pdcch_pdu_rel15.dci_pdu[0].Payload = *(uint64_t *)pscch_pssch_pdu->pscch_sci_payload; 
  return(nr_generate_sci((PHY_VARS_NR_UE *)ue,&pdcch_pdu_rel15,(c16_t *)txdataF,amp,(NR_DL_FRAME_PARMS*)frame_parms,nr_slot_tx));
} 
