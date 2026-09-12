/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
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

uint32_t nr_generate_dci(void *gNB, PHY_VARS_NR_UE *ue,
                         nfapi_nr_dl_tti_pdcch_pdu_rel15_t *pdcch_pdu_rel15,
                         int32_t *txdataF,
                         int16_t amp,
                         NR_DL_FRAME_PARMS *frame_parms,
                         int slot);

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
  //?return(nr_generate_dci(NULL,(PHY_VARS_NR_UE *)ue,&pdcch_pdu_rel15,(int32_t *)txdataF,amp,(NR_DL_FRAME_PARMS*)frame_parms,nr_slot_tx)); 
} 
