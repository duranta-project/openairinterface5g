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

/* \file nr_ue_sci.c
 * \brief handling of sci/slsch procedures for Sidelink UE
 * \author R. Knopp
 * \date 2023
 * \version 0.1
 * \company Eurecom
 * \email: knopp@eurecom.fr
 * \note
 * \warning
 */


#include <stdio.h>
#include <math.h>

/* exe */
#include "executables/nr-softmodem.h"
#include "executables/nr-uesoftmodem.h"

/* RRC*/
#include "RRC/NR_UE/rrc_proto.h"
#include "NR_SL-BWP-ConfigCommon-r16.h"

/* MAC */
#include "NR_MAC_UE/nr_ue_sci.h"
#include "NR_MAC_COMMON/nr_mac.h"
#include "NR_MAC_UE/mac_proto.h"
#include "NR_MAC_UE/mac_extern.h"
#include "NR_MAC_COMMON/nr_mac_extern.h"
#include "common/utils/nr/nr_common.h"
#include "openair2/NR_UE_PHY_INTERFACE/NR_Packet_Drop.h"
#include "openair1/PHY/phy_extern_nr_ue.h"
/* PHY */
#include "executables/softmodem-common.h"
#include "openair1/PHY/defs_nr_UE.h"

/* utils */
#include "assertions.h"
#include "oai_asn1.h"
#include "common/utils/LOG/log.h"
#include "common/utils/LOG/vcd_signal_dumper.h"


const int sl_dmrs_mask2[2][8] = { {34,34,34,264,264,1032,1032,1032},
                                  {34,34,34,272,272,1040,1040,1040}};
const int sl_dmrs_mask3[5]    = {146,146,546,546,2114};
const int sl_dmrs_mask4[3]    = {1170,1170,1170};
const int pscch_rb_table[5] = {10,12,15,20,25};
const int pscch_tda[2] = {2,3};

const int subch_to_rb[8] = {10,12,15,20,25,50,75,100};

uint32_t nr_sci_size(const NR_SL_ResourcePool_r16_t *sl_res_pool,
	             nr_sci_pdu_t *sci_pdu,
	             const nr_sci_format_t format) {

  int size=0;

  switch(format) {
    case NR_SL_SCI_FORMAT_1A:
	    // priority
	    size+=3;
	    // frequency resource assignment
	    long Nsc = *sl_res_pool->sl_NumSubchannel_r16;
            if (sl_res_pool->sl_UE_SelectedConfigRP_r16 &&
                sl_res_pool->sl_UE_SelectedConfigRP_r16->sl_MaxNumPerReserve_r16 &&
                *sl_res_pool->sl_UE_SelectedConfigRP_r16->sl_MaxNumPerReserve_r16 == NR_SL_UE_SelectedConfigRP_r16__sl_MaxNumPerReserve_r16_n2)
	      sci_pdu->frequency_resource_assignment.nbits =  (uint8_t)ceil(log2((Nsc * (Nsc + 1)) >>1));
	    else
	      sci_pdu->frequency_resource_assignment.nbits =  (uint8_t)ceil(log2((Nsc * (Nsc + 1) * (2*Nsc + 1)) /6));
            size += sci_pdu->frequency_resource_assignment.nbits;
	    // time-domain-assignment
            if (*sl_res_pool->sl_UE_SelectedConfigRP_r16->sl_MaxNumPerReserve_r16 == NR_SL_UE_SelectedConfigRP_r16__sl_MaxNumPerReserve_r16_n2)
		sci_pdu->time_resource_assignment.nbits = 5;
	    else
		sci_pdu->time_resource_assignment.nbits = 9;
            size += sci_pdu->time_resource_assignment.nbits;

	    // resource reservation period

	    if (1 /*!sl_res_pool->sl_MultiReserveResource*/) // not defined in 17.4 RRC
	       sci_pdu->resource_reservation_period.nbits = 0;
            size += sci_pdu->resource_reservation_period.nbits;

	    // DMRS pattern
	    int dmrs_pattern_num = sl_res_pool->sl_PSSCH_Config_r16->choice.setup->sl_PSSCH_DMRS_TimePatternList_r16->list.count;
	    sci_pdu->dmrs_pattern.nbits = (uint8_t)ceil(log2(dmrs_pattern_num));
	    size += sci_pdu->dmrs_pattern.nbits;

            // second_stage_sci_format // 2 bits - Table 8.3.1.1-1
	    size += 2;
	    // beta_offset_indicator // 2 bits - depending sl-BetaOffsets2ndSCI and Table 8.3.1.1-2
	    size += 2;
	    // number_of_dmrs_port // 1 bit - Table 8.3.1.1-3
	    size += 1;
            // mcs // 5 bits
	    size += 5;

	    // additional_mcs; // depending on sl-Additional-MCS-Table
	    if (sl_res_pool->sl_Additional_MCS_Table_r16)
	       sci_pdu->additional_mcs.nbits = (*sl_res_pool->sl_Additional_MCS_Table_r16 < 2) ? 1 : 2;
	    else sci_pdu->additional_mcs.nbits=0;
	    size += sci_pdu->additional_mcs.nbits;

	    // psfch_overhead; // depending on sl-PSFCH-Period
	    if (sl_res_pool->sl_PSFCH_Config_r16 && sl_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_Period_r16 && *sl_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_Period_r16>1)
		sci_pdu->psfch_overhead.nbits=1;
	    else sci_pdu->psfch_overhead.nbits=0;
	    size += sci_pdu->psfch_overhead.nbits;

   	    // reserved; // depending on N_reserved (sl-NumReservedBits) and sl-IndicationUE-B
	    // note R17 dependence no sl_IndicationUE-B needs to be added here
	    AssertFatal(sl_res_pool->sl_PSCCH_Config_r16!=NULL,"sl_res_pool->sl_PSCCH_Config_r16 is null\n");
	    AssertFatal(sl_res_pool->sl_PSCCH_Config_r16->choice.setup!=NULL,"sl_res_pool->sl_PSCCH_Config_r16->choice.setup is null\n");
	    AssertFatal(sl_res_pool->sl_PSCCH_Config_r16->choice.setup->sl_NumReservedBits_r16!=NULL, "sl_res_pool->sl_PSCCH_Config_r16->choice.setup->sl_NumReservedBits_r16 is null\n");
            sci_pdu->reserved.nbits = *sl_res_pool->sl_PSCCH_Config_r16->choice.setup->sl_NumReservedBits_r16;
            size+=sci_pdu->reserved.nbits;

            // conflict_information_receiver; // depending on sl-IndicationUE-B
	    // note: R17 field not included here
	    sci_pdu->conflict_information_receiver.nbits=0;
            size+=sci_pdu->conflict_information_receiver.nbits;
	    break;
    case NR_SL_SCI_FORMAT_2A:
    case NR_SL_SCI_FORMAT_2B:
    case NR_SL_SCI_FORMAT_2C:
	    // common components
	    //harq_pid; // 4 bits
	    //ndi; // 1 bit
	    //rv_index; // 2 bits
	    //source_id; // 8 bits
	    //dest_id; // 16 bits
	    //harq_feedback; //1 bit
	    size += (4+1+2+8+16+1);
	    if (format==NR_SL_SCI_FORMAT_2A)
	      //cast_type // 2 bits formac 2A
	      size += 2;
	    if (format==NR_SL_SCI_FORMAT_2C || format==NR_SL_SCI_FORMAT_2A)
	      // csi_req // 1 bit format 2A, format 2C
              size +=1;
	    if (format==NR_SL_SCI_FORMAT_2B) {
	      // zone_id // 12 bits format 2B
	      size +=12;

	      // communication_range; // 4 bits depending on sl-ZoneConfigMCR-Index, format 2B
	      // note fill in for R17
	      if (0) size +=4;
	    }
	    else if (format==NR_SL_SCI_FORMAT_2C) {

       	     // providing_req_ind; // 1 bit, format 2C
	     size += 1;
             // resource_combinations; // depending on n_subChannel^SL (sl-NumSubchennel), N_rsv_period (sl-ResourceReservePeriodList) and sl-MultiReservedResource, format 2C
	     // first_resource_location; // 8 bits, format 2C
	     size += 8;
	     // reference_slot_location; // depending on mu, format 2C
             // resource_set_type; // 1 bit, format 2C
	     size += 1;
	     //	lowest_subchannel_indices; // depending on n_subChannel^SL, format 2C
	     //
	    }
	    break;
  }
  return(size);
}


int get_nREDMRS(const NR_SL_ResourcePool_r16_t *sl_res_pool) {

  int cnt = sl_res_pool->sl_PSSCH_Config_r16->choice.setup->sl_PSSCH_DMRS_TimePatternList_r16->list.count;
  int nREDMRS = 0;
  for (int i=0;i<cnt;i++)
    nREDMRS += *sl_res_pool->sl_PSSCH_Config_r16->choice.setup->sl_PSSCH_DMRS_TimePatternList_r16->list.array[i] * 6;
  return(nREDMRS/cnt);
}

int get_nRECSI_RS(uint8_t freq_density,
                  uint16_t nr_of_rbs) {
  AssertFatal(freq_density > 0, "freq_density must be greater than 1\n");
  uint8_t nr_rbs_w_csi_rs = nr_of_rbs / freq_density;
  // Actually, kprime + 1 sub-carriers are used by csi-rs. kprime can be 0 or 1 but nb_antennas_tx can be greater than 2.
  uint8_t subcarriers_used = get_nrUE_params()->nb_antennas_tx > 2 ? 2 : get_nrUE_params()->nb_antennas_tx;
  return nr_rbs_w_csi_rs * subcarriers_used;
}

void fill_pssch_pscch_pdu(sl_nr_ue_mac_params_t *sl_mac_params,
                          sl_nr_tx_config_pscch_pssch_pdu_t *nr_sl_pssch_pscch_pdu,
                          const NR_SL_BWP_ConfigCommon_r16_t *sl_bwp,
                          const NR_SL_ResourcePool_r16_t *sl_res_pool,
                          nr_sci_pdu_t *sci_pdu,
                          nr_sci_pdu_t *sci2_pdu,
                          uint16_t slsch_pdu_length,
                          const nr_sci_format_t format1,
                          const nr_sci_format_t format2,
                          uint16_t slot,
                          sl_resource_info_t *selected_resource)  {
  int pos = 0, fsize;
  uint64_t *sci_payload = (uint64_t *)nr_sl_pssch_pscch_pdu->pscch_sci_payload;
  uint64_t *sci2_payload = (uint64_t *)nr_sl_pssch_pscch_pdu->sci2_payload;
  nr_sl_pssch_pscch_pdu->pscch_sci_payload_len = nr_sci_size(sl_res_pool,sci_pdu,format1);
  nr_sl_pssch_pscch_pdu->sci2_payload_len = nr_sci_size(sl_res_pool,sci2_pdu,format2);
  nr_sl_pssch_pscch_pdu->harq_pid = sci2_pdu->harq_pid;
  int sci_size = nr_sl_pssch_pscch_pdu->pscch_sci_payload_len;
  int sci2_size = nr_sl_pssch_pscch_pdu->sci2_payload_len;

  *sci_payload = 0;
  *sci2_payload = 0;

  // freq domain allocation starts
  nr_sl_pssch_pscch_pdu->startrb= *sl_res_pool->sl_StartRB_Subchannel_r16;
  // Number of symbols used for PSCCH
  nr_sl_pssch_pscch_pdu->pscch_numsym = selected_resource ? selected_resource->sl_pscch_sym_len : pscch_tda[*sl_res_pool->sl_PSCCH_Config_r16->choice.setup->sl_TimeResourcePSCCH_r16];
  // Number of  RBS used for PSCCH
  nr_sl_pssch_pscch_pdu->pscch_numrbs = selected_resource ? selected_resource->num_sl_pscch_rbs : pscch_rb_table[*sl_res_pool->sl_PSCCH_Config_r16->choice.setup->sl_FreqResourcePSCCH_r16];
  // Scrambling Id used for Generation of PSCCH DMRS Symbols
  nr_sl_pssch_pscch_pdu->pscch_dmrs_scrambling_id = *sl_res_pool->sl_PSCCH_Config_r16->choice.setup->sl_DMRS_ScrambleID_r16;
  // num subchannels in a resource pool
  nr_sl_pssch_pscch_pdu->num_subch = *sl_res_pool->sl_NumSubchannel_r16;
  // Size of subchannels in RBs
  nr_sl_pssch_pscch_pdu->subchannel_size = selected_resource ? selected_resource->sl_subchan_size : subch_to_rb[*sl_res_pool->sl_SubchannelSize_r16];
  //_PSCCH PSSCH TX: Size of subchannels in a PSSCH resource (l_subch)
  AssertFatal(sci_pdu->time_resource_assignment.val == 0, "need to handle a non-zero time_resource_assignment (2 or 3 time hops, N=2,3)\n");
  convNRFRIV(sci_pdu->frequency_resource_assignment.val,
             nr_sl_pssch_pscch_pdu->num_subch,
             *sl_res_pool->sl_UE_SelectedConfigRP_r16->sl_MaxNumPerReserve_r16,
             &nr_sl_pssch_pscch_pdu->l_subch,
             NULL,NULL);
  //number of symbols for Sidelink transmission on PSSCH/PSCCH
  //(Total Sidelink symbols available - number of psfch symbols configured - 2)
  //Guard symbol + AGC symbol are also excluded
  //Indicates the number of symbols for PSCCH+PSSCH txn
  int num_psfch_symbols = 0;
  LOG_D(NR_MAC,"startrb %d, pscch_numsym %d, pscch_numrbs %d,pscch_dmrs_scrambling_id %d,num_subch%d,subchannel_size%d\n",
  nr_sl_pssch_pscch_pdu->startrb,
  nr_sl_pssch_pscch_pdu->pscch_numsym,
  nr_sl_pssch_pscch_pdu->pscch_numrbs,
  nr_sl_pssch_pscch_pdu->pscch_dmrs_scrambling_id,
  nr_sl_pssch_pscch_pdu->num_subch,
  nr_sl_pssch_pscch_pdu->subchannel_size);
  if (sl_res_pool->sl_PSFCH_Config_r16 && sl_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_Period_r16 && *sl_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_Period_r16>0) {
     // As per 38214 8.1.3.2, num_psfch_symbols can be 3 if psfch_overhead_indication.nbits is 1; FYI psfch_overhead_indication.nbits is set to 1 in case of PSFCH period 2 or 4 in sl_determine_sci_1a_len()
     num_psfch_symbols = 3;
  }
  nr_sl_pssch_pscch_pdu->pssch_numsym = selected_resource ? selected_resource->sl_pssch_sym_len : 7 + *sl_bwp->sl_BWP_Generic_r16->sl_LengthSymbols_r16 - num_psfch_symbols - 2;
  nr_sl_pssch_pscch_pdu->pssch_startsym = selected_resource ? selected_resource->sl_pssch_sym_start : *sl_bwp->sl_BWP_Generic_r16->sl_StartSymbol_r16;

  nr_sl_pssch_pscch_pdu->sci2_beta_offset = *sl_res_pool->sl_PSSCH_Config_r16->choice.setup->sl_BetaOffsets2ndSCI_r16->list.array[sci_pdu->beta_offset_indicator];
  if (sl_res_pool->sl_PowerControl_r16) {
    nr_sl_pssch_pscch_pdu->sci2_alpha_times_100=50 + (*sl_res_pool->sl_PowerControl_r16->sl_Alpha_PSSCH_PSCCH_r16)*15;
    if (*sl_res_pool->sl_PowerControl_r16->sl_Alpha_PSSCH_PSCCH_r16 == 3) nr_sl_pssch_pscch_pdu->sci2_alpha_times_100=100;
  } else nr_sl_pssch_pscch_pdu->sci2_alpha_times_100 = 100;

  switch(format1) {
    case NR_SL_SCI_FORMAT_1A:
	    // priority 3 bits
	    fsize=3;
            LOG_D(NR_MAC,"SCI1A: priority (%d,%d) in position %d\n",sci_pdu->priority,fsize,pos);
	    for (int i = 0; i < fsize; i++)
		   *sci_payload |= (((uint64_t)sci_pdu->priority >> (fsize - i - 1)) & 1) << (sci_size - pos++ - 1);

	    // frequency resource assignment
	    fsize = sci_pdu->frequency_resource_assignment.nbits;
            LOG_D(NR_MAC,"SCI1A: frequency resource assignment (%d,%d) in position %d\n",sci_pdu->frequency_resource_assignment.val,fsize,pos);
	    for (int i = 0; i < fsize; i++)
		   *sci_payload |= (((uint64_t)sci_pdu->frequency_resource_assignment.val >> (fsize - i - 1)) & 1) << (sci_size - pos++ -1);
	    // time-domain-assignment
	    fsize = sci_pdu->time_resource_assignment.nbits;
            LOG_D(NR_MAC,"SCI1A: time resource assignment (%d,%d) in position %d\n",sci_pdu->time_resource_assignment.val,fsize,pos);
	    for (int i = 0; i < fsize; i++)
		   *sci_payload |= (((uint64_t)sci_pdu->time_resource_assignment.val >> (fsize - i - 1)) & 1) << (sci_size - pos++ - 1);
	    // resource reservation period
            fsize = sci_pdu->resource_reservation_period.nbits;
            LOG_D(NR_MAC,"SCI1A: resource_reservation_period (%d,%d) in position %d\n",sci_pdu->resource_reservation_period.val,fsize,pos);
	    for (int i = 0; i < fsize; i++)
		   *sci_payload |= (((uint64_t)sci_pdu->resource_reservation_period.val >> (fsize - i - 1)) & 1) << (sci_size - pos++ -1);
	    // DMRS pattern
	    fsize = sci_pdu->dmrs_pattern.nbits;
            LOG_D(NR_MAC,"SCI1A: dmrs_pattern (%d,%d) in position %d\n",sci_pdu->dmrs_pattern.val,fsize,pos);
	    for (int i = 0; i < fsize; i++)
		   *sci_payload |= (((uint64_t)sci_pdu->dmrs_pattern.val >> (fsize - i - 1)) & 1) << (sci_size - pos++ -1);
            // second_stage_sci_format // 2 bits - Table 8.3.1.1-1
	    fsize = 2;
            LOG_D(NR_MAC,"SCI1A: second_stage_sci_format (%d,%d) in position %d\n",sci_pdu->second_stage_sci_format,fsize,pos);
	    for (int i = 0; i < fsize; i++)
		   *sci_payload |= (((uint64_t)sci_pdu->second_stage_sci_format >> (fsize - i - 1)) & 1) << (sci_size - pos++ -1);
	    // beta_offset_indicator // 2 bits - depending sl-BetaOffsets2ndSCI and Table 8.3.1.1-2
	    fsize = 2;
            LOG_D(NR_MAC,"SCI1A: beta_offset_indicator (%d,%d) in position %d\n",sci_pdu->beta_offset_indicator,fsize,pos);
	    for (int i = 0; i < fsize; i++)
		   *sci_payload |= (((uint64_t)sci_pdu->beta_offset_indicator >> (fsize - i - 1)) & 1) << (sci_size - pos++ -1);
	    // number_of_dmrs_port // 1 bit - Table 8.3.1.1-3
	    fsize = 1;
            LOG_D(NR_MAC,"SCI1A: number_of_dmrs_port (%d,%d) in pos %d\n",sci_pdu->number_of_dmrs_port,fsize,pos);
	    *sci_payload |= (((uint64_t)sci_pdu->number_of_dmrs_port&1)) << (sci_size - pos++ -1);
            // mcs // 5 bits
	    fsize = 5;
	    for (int i = 0; i < fsize; i++)
		   *sci_payload |= (((uint64_t)sci_pdu->mcs >> (fsize - i - 1)) & 1) << (sci_size - pos++ -1);
            LOG_D(NR_MAC,"SCI1A: mcs (%d,%d) in pos %d\n",sci_pdu->mcs,fsize,pos);
	    // additional_mcs; // depending on sl-Additional-MCS-Table
	    fsize = sci_pdu->additional_mcs.nbits;
	    for (int i = 0; i < fsize; i++)
		   *sci_payload |= (((uint64_t)sci_pdu->additional_mcs.val >> (fsize - i - 1)) & 1) << (sci_size - pos++ -1);
	    // psfch_overhead; // depending on sl-PSFCH-Period
      fsize = sci_pdu->psfch_overhead.nbits;
      for (int i = 0; i < fsize; i++)
        *sci_payload |= (((uint64_t)sci_pdu->psfch_overhead.val >> (fsize - i - 1)) & 1) << (sci_size - pos++ -1);

      // reserved; // depending on N_reserved (sl-NumReservedBits) and sl-IndicationUE-B
      fsize = sci_pdu->reserved.nbits;
	    for (int i = 0; i < fsize; i++)
		   *sci_payload |= (((uint64_t)sci_pdu->reserved.val >> (fsize - i - 1)) & 1) << (sci_size - pos++ -1 );
            // conflict_information_receiver; // depending on sl-IndicationUE-B
	    fsize = sci_pdu->conflict_information_receiver.nbits;
	    for (int i = 0; i < fsize; i++)
		   *sci_payload |= (((uint64_t)sci_pdu->conflict_information_receiver.val >> (fsize - i - 1)) & 1) << (sci_size - pos++ -1);
	    break;
    default:
            AssertFatal(1==0,"Unknown format1 %d\n",format1);
            break;
  }

  int mcs_tb_ind = 0;
  if (sci_pdu->additional_mcs.nbits > 0)
    mcs_tb_ind = sci_pdu->additional_mcs.val;
  nr_sl_pssch_pscch_pdu->mcs = sci_pdu->mcs;
  int nohPRB    = (sl_res_pool->sl_X_Overhead_r16) ? 3*(*sl_res_pool->sl_X_Overhead_r16) : 0;
  int nREDMRS   = get_nREDMRS(sl_res_pool);
  int num_CSI_REs= sci2_pdu->csi_req ? get_nRECSI_RS(sl_mac_params->freq_density, sl_mac_params->nr_of_rbs) : 0;
  int num_CSI_REs_per_RB = sci2_pdu->csi_req ? (num_CSI_REs/sl_mac_params->nr_of_rbs) : 0;
  int N_REprime = 12 * nr_sl_pssch_pscch_pdu->pssch_numsym - nohPRB - nREDMRS - num_CSI_REs_per_RB;
  int N_REsci1  = 12*nr_sl_pssch_pscch_pdu->pscch_numrbs*nr_sl_pssch_pscch_pdu->pscch_numsym;
  AssertFatal(*sl_res_pool->sl_PSSCH_Config_r16->choice.setup->sl_Scaling_r16 < 4, "Illegal index %d to alpha table\n",(int)*sl_res_pool->sl_PSSCH_Config_r16->choice.setup->sl_Scaling_r16);
  int N_REsci2  = get_NREsci2(nr_sl_pssch_pscch_pdu->sci2_alpha_times_100,
                              nr_sl_pssch_pscch_pdu->sci2_payload_len,
                              nr_sl_pssch_pscch_pdu->sci2_beta_offset,
                              nr_sl_pssch_pscch_pdu->pssch_numsym,
                              nr_sl_pssch_pscch_pdu->pscch_numsym,
                              nr_sl_pssch_pscch_pdu->pscch_numrbs,
                              nr_sl_pssch_pscch_pdu->l_subch,
                              nr_sl_pssch_pscch_pdu->subchannel_size,
                              get_softmodem_params()->sl_mode ? 1 : nr_sl_pssch_pscch_pdu->mcs,
                              mcs_tb_ind);
  LOG_D(NR_MAC, "nREDMRS %d, num_CSI_REs %d, N_REprime %d, N_REsci1 %d, N_REsci2 %d\n", nREDMRS, num_CSI_REs, N_REprime, N_REsci1, N_REsci2);
  int N_RE      = N_REprime*nr_sl_pssch_pscch_pdu->l_subch*nr_sl_pssch_pscch_pdu->subchannel_size - N_REsci1 - N_REsci2;

  nr_sl_pssch_pscch_pdu->mod_order = nr_get_Qm_ul(sci_pdu->mcs,mcs_tb_ind);
  nr_sl_pssch_pscch_pdu->target_coderate = nr_get_code_rate_ul(sci_pdu->mcs,mcs_tb_ind);
  nr_sl_pssch_pscch_pdu->tb_size = (nr_compute_tbs_sl(nr_sl_pssch_pscch_pdu->mod_order,
                                                     nr_sl_pssch_pscch_pdu->target_coderate,
						     N_RE,1+(sci_pdu->number_of_dmrs_port&1))+7)>>3;
  nr_sl_pssch_pscch_pdu->mcs = sci_pdu->mcs;
  nr_sl_pssch_pscch_pdu->num_layers = sci_pdu->number_of_dmrs_port+1;
  if (slot % 10 == 6)
    LOG_D(NR_MAC,"PSSCH: mcs %d, coderate %d, Nl %d => tbs %d\n", sci_pdu->mcs,nr_sl_pssch_pscch_pdu->target_coderate,nr_sl_pssch_pscch_pdu->num_layers,nr_sl_pssch_pscch_pdu->tb_size);
  nr_sl_pssch_pscch_pdu->tbslbrm = nr_compute_tbslbrm(mcs_tb_ind,NRRIV2BW(sl_bwp->sl_BWP_Generic_r16->sl_BWP_r16->locationAndBandwidth,273),nr_sl_pssch_pscch_pdu->num_layers);
  nr_sl_pssch_pscch_pdu->mcs_table=mcs_tb_ind;
  nr_sl_pssch_pscch_pdu->rv_index = sci2_pdu->rv_index;
  nr_sl_pssch_pscch_pdu->ndi = sci2_pdu->ndi;
  int num_dmrs_symbols;
  AssertFatal(sci_pdu->dmrs_pattern.val < sl_res_pool->sl_PSSCH_Config_r16->choice.setup->sl_PSSCH_DMRS_TimePatternList_r16->list.count,"dmrs.pattern %d out of bounds for list size %d\n",sci_pdu->dmrs_pattern.val,sl_res_pool->sl_PSSCH_Config_r16->choice.setup->sl_PSSCH_DMRS_TimePatternList_r16->list.count);
  num_dmrs_symbols = *sl_res_pool->sl_PSSCH_Config_r16->choice.setup->sl_PSSCH_DMRS_TimePatternList_r16->list.array[sci_pdu->dmrs_pattern.val];
  if (num_dmrs_symbols == 2) {
    AssertFatal(nr_sl_pssch_pscch_pdu->pssch_numsym>5, "num_pssch_ymbols %d is not ok for 2 DMRS (min 6)\n",nr_sl_pssch_pscch_pdu->pssch_numsym);
    nr_sl_pssch_pscch_pdu->dmrs_symbol_position = sl_dmrs_mask2[nr_sl_pssch_pscch_pdu->pscch_numsym-2][nr_sl_pssch_pscch_pdu->pssch_numsym-6];
  } else if (num_dmrs_symbols == 3) {
    AssertFatal(nr_sl_pssch_pscch_pdu->pssch_numsym>8, "num_pssch_ymbols %d is not ok for 3 DMRS (min 9)\n",nr_sl_pssch_pscch_pdu->pssch_numsym);
    nr_sl_pssch_pscch_pdu->dmrs_symbol_position = sl_dmrs_mask3[nr_sl_pssch_pscch_pdu->pssch_numsym-9];
  } else if (num_dmrs_symbols == 4) {
    AssertFatal(nr_sl_pssch_pscch_pdu->pssch_numsym>10, "num_pssch_ymbols %d is not ok for 4 DMRS (min 11)\n",nr_sl_pssch_pscch_pdu->pssch_numsym);
    nr_sl_pssch_pscch_pdu->dmrs_symbol_position = sl_dmrs_mask4[nr_sl_pssch_pscch_pdu->pssch_numsym-11];
  }

  pos=0;
  switch(format2) {
    case NR_SL_SCI_FORMAT_2A:
    case NR_SL_SCI_FORMAT_2B:
    case NR_SL_SCI_FORMAT_2C:
	    // common components
	    //harq_pid; // 4 bits
            fsize = 4;
	    for (int i = 0; i < fsize; i++)
		   *sci2_payload |= (((uint64_t)sci2_pdu->harq_pid >> (fsize - i - 1)) & 1) << (sci2_size - pos++ -1);
	    //ndi; // 1 bit
	    *sci2_payload |= ((uint64_t)sci2_pdu->ndi  & 1) << (sci2_size - pos++ -1);
	    //rv_index; // 2 bits
            fsize = 2;
	    for (int i = 0; i < fsize; i++)
		   *sci2_payload |= (((uint64_t)sci2_pdu->rv_index >> (fsize - i - 1)) & 1) << (sci2_size - pos++ -1);
	    //source_id; // 8 bits
            fsize = 8;
	    for (int i = 0; i < fsize; i++)
		   *sci2_payload |= (((uint64_t)sci2_pdu->source_id >> (fsize - i - 1)) & 1) << (sci2_size - pos++ -1);
	    //dest_id; // 16 bits
            fsize = 16;
	    for (int i = 0; i < fsize; i++)
		   *sci2_payload |= (((uint64_t)sci2_pdu->dest_id >> (fsize - i - 1)) & 1) << (sci2_size - pos++ -1);
	    //harq_feedback; //1 bit
	    *sci2_payload |= ((uint64_t)sci2_pdu->harq_feedback  & 1) << (sci2_size - pos++ -1);
	    if (format2==NR_SL_SCI_FORMAT_2A) {
	      //cast_type // 2 bits formac 2A
	      fsize = 2;
	      for (int i = 0; i < fsize; i++)
	  	   *sci2_payload |= (((uint64_t)sci2_pdu->cast_type >> (fsize - i - 1)) & 1) << (sci2_size - pos++ -1);
            }
	    if (format2==NR_SL_SCI_FORMAT_2C || format2==NR_SL_SCI_FORMAT_2A)
	      // csi_req // 1 bit format 2A, format 2C
	      *sci2_payload |= ((uint64_t)sci2_pdu->csi_req  & 1) << (sci2_size - pos++ -1);

	    if (format2==NR_SL_SCI_FORMAT_2B) {
	      // zone_id // 12 bits format 2B
	      fsize = 12;
	      for (int i = 0; i < fsize; i++)
	  	   *sci2_payload |= (((uint64_t)sci2_pdu->zone_id >> (fsize - i - 1)) & 1) << (sci2_size - pos++ -1);

	      // communication_range; // 4 bits depending on sl-ZoneConfigMCR-Index, format 2B
	      // note fill in for R17
	      if (0) {
	        fsize = 4;
	        for (int i = 0; i < fsize; i++)
	  	     *sci2_payload |= (((uint64_t)sci2_pdu->communication_range.val >> (fsize - i - 1)) & 1) << (sci2_size - pos++ -1);
              }
	    }
	    else if (format2==NR_SL_SCI_FORMAT_2C) {

       	     // providing_req_ind; // 1 bit, format 2C
	     *sci2_payload |= ((uint64_t)sci2_pdu->providing_req_ind  & 1) << (sci2_size - pos++ -1);
             // resource_combinations; // depending on n_subChannel^SL (sl-NumSubchennel), N_rsv_period (sl-ResourceReservePeriodList) and sl-MultiReservedResource, format 2C
             if (0) {
               fsize = 0;
	       for (int i = 0; i < fsize; i++)
	          *sci2_payload |= (((uint64_t)sci2_pdu->resource_combinations.val >> (fsize - i - 1)) & 1) << (sci2_size - pos++ -1);
             }
	     // first_resource_location; // 8 bits, format 2C
	     fsize = 8;
	     for (int i = 0; i < fsize; i++)
	        *sci2_payload |= (((uint64_t)sci2_pdu->first_resource_location >> (fsize - i - 1)) & 1) << (sci2_size - pos++ -1);
	     // reference_slot_location; // depending on mu, format 2C
	     if (0) {
               fsize = 0;
	       for (int i = 0; i < fsize; i++)
	          *sci2_payload |= (((uint64_t)sci2_pdu->reference_slot_location.val >> (fsize - i - 1)) & 1) << (sci2_size - pos++ -1);
             }
             // resource_set_type; // 1 bit, format 2C
	     *sci2_payload |= ((uint64_t)sci2_pdu->resource_set_type  & 1) << (sci2_size - pos++ -1);
	     //	lowest_subchannel_indices; // depending on n_subChannel^SL, format 2C
	     if (0) {
	       fsize = 0;
	       for (int i = 0; i < fsize; i++)
	          *sci2_payload |= (((uint64_t)sci2_pdu->lowest_subchannel_indices.val >> (fsize - i - 1)) & 1) << (sci2_size - pos++ -1);
             }
	     //
	    }
	    break;
        default:
          AssertFatal(1==0,"Unknown format %d for sci2\n",format2);
          break;
  }
  nr_sl_pssch_pscch_pdu->slsch_payload_length = slsch_pdu_length;
};

void config_pscch_pdu_rx(sl_nr_rx_config_pscch_pdu_t *nr_sl_pscch_pdu,
                         const NR_SL_BWP_ConfigCommon_r16_t *sl_bwp,
                         const NR_SL_ResourcePool_r16_t *sl_res_pool,
                         bool sl_has_psfch) {

  nr_sci_pdu_t dummy_sci;
  // Starting RE of the lowest subchannel in a resource where PSCCH
  // freq domain allocation starts
  nr_sl_pscch_pdu->pscch_startrb=*sl_res_pool->sl_StartRB_Subchannel_r16;
  // Number of symbols used for PSCCH
  nr_sl_pscch_pdu->pscch_numsym=pscch_tda[*sl_res_pool->sl_PSCCH_Config_r16->choice.setup->sl_TimeResourcePSCCH_r16];
  // Number of  RBS used for PSCCH
  nr_sl_pscch_pdu->pscch_numrbs=pscch_rb_table[*sl_res_pool->sl_PSCCH_Config_r16->choice.setup->sl_FreqResourcePSCCH_r16];
  // Scrambling Id used for Generation of PSCCH DMRS Symbols
  nr_sl_pscch_pdu->pscch_dmrs_scrambling_id=*sl_res_pool->sl_PSCCH_Config_r16->choice.setup->sl_DMRS_ScrambleID_r16;
;
  // num subchannels in a resource pool
  nr_sl_pscch_pdu->num_subch=*sl_res_pool->sl_NumSubchannel_r16;
  // Size of subchannels in RBs
  nr_sl_pscch_pdu->subchannel_size=subch_to_rb[*sl_res_pool->sl_SubchannelSize_r16];
  // PSCCH PSSCH RX: this is set to 1 - Blind decoding for SCI1A done on every subchannel
  // PSCCH SENSING: this is equal to number of subchannels forming a resource.

  nr_sl_pscch_pdu->l_subch=1;
  //number of symbols for Sidelink transmission on PSSCH/PSCCH
  //(Total Sidelink symbols available - number of psfch symbols configured - 2)
  //Guard symbol + AGC symbol are also excluded
  //Indicates the number of symbols for PSCCH+PSSCH txn
  int num_psfch_symbols = 0;
  if (sl_has_psfch && sl_res_pool->sl_PSFCH_Config_r16 && sl_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_Period_r16 &&
      *sl_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_Period_r16 > 0) {
     // As per 38214 8.1.3.2, num_psfch_symbols can be 3 if psfch_overhead_indication.nbits is 1; FYI psfch_overhead_indication.nbits is set to 1 in case of PSFCH period 2 or 4 in sl_determine_sci_1a_len()
     num_psfch_symbols = 3;
  }
  nr_sl_pscch_pdu->pssch_numsym=7+*sl_bwp->sl_BWP_Generic_r16->sl_LengthSymbols_r16-num_psfch_symbols-2;
  //sci 1A length used to decode on PSCCH.
  nr_sl_pscch_pdu->sci_1a_length = nr_sci_size(sl_res_pool,&dummy_sci,NR_SL_SCI_FORMAT_1A);
  //This paramter is set if PSCCH RX is triggered on TX resource pool
  // as part of TX pool sensing procedure.
  nr_sl_pscch_pdu->sense_pscch=0;

  LOG_D(NR_MAC,"Programming PSCCH reception (sci_1a_length %d)\n",nr_sl_pscch_pdu->sci_1a_length);

}

void extract_pscch_pdu(uint64_t *sci1_payload, int len,
	  	       const NR_SL_BWP_ConfigCommon_r16_t *sl_bwp,
                       const NR_SL_ResourcePool_r16_t *sl_res_pool,
		       nr_sci_pdu_t *sci_pdu) {
  int pos=0,fsize;
  int sci1_size = nr_sci_size(sl_res_pool,sci_pdu,NR_SL_SCI_FORMAT_1A);
  AssertFatal(sci1_size == len,"sci1a size %d is not the same sci_indication %d\n",sci1_size,len);

    // priority 3 bits
  fsize=3;
  pos=fsize;
  sci_pdu->priority = *sci1_payload>>(sci1_size-pos)&((1<<fsize)-1);
  LOG_D(NR_MAC,"priority (%d) in pos %d\n",sci_pdu->priority,pos-fsize);

  // frequency resource assignment
  fsize = sci_pdu->frequency_resource_assignment.nbits;
  pos+=fsize;
  sci_pdu->frequency_resource_assignment.val = *sci1_payload>>(sci1_size-pos)&((1<<fsize)-1);
  LOG_D(NR_MAC,"frequency_resource_assignment(%d) in pos %d\n",sci_pdu->frequency_resource_assignment.val,pos-fsize);

  // time-domain-assignment
  fsize = sci_pdu->time_resource_assignment.nbits;
  pos+=fsize;
  sci_pdu->time_resource_assignment.val = *sci1_payload>>(sci1_size-pos)&((1<<fsize)-1);
  LOG_D(NR_MAC,"time_resource_assignment(%d) in pos %d\n",sci_pdu->time_resource_assignment.val,pos-fsize);

  // resource reservation period
  fsize = sci_pdu->resource_reservation_period.nbits;
  pos+=fsize;
  sci_pdu->resource_reservation_period.val = *sci1_payload>>(sci1_size-pos)&((1<<fsize)-1);
  LOG_D(NR_MAC,"resource_reservation_period (%d) in pos %d\n",sci_pdu->resource_reservation_period.val,pos-fsize);

  // DMRS pattern
  fsize = sci_pdu->dmrs_pattern.nbits;
  pos+=fsize;
  sci_pdu->dmrs_pattern.val = *sci1_payload>>(sci1_size-pos)&((1<<fsize)-1);
  LOG_D(NR_MAC,"dmrs_pattern_val (%d) in pos %d\n",sci_pdu->dmrs_pattern.val,pos-fsize);

  // second_stage_sci_format // 2 bits - Table 8.3.1.1-1
  fsize = 2;
  pos+=fsize;
  sci_pdu->second_stage_sci_format = *sci1_payload>>(sci1_size-pos)&((1<<fsize)-1);
  LOG_D(NR_MAC,"second_stage_sci_format (%d) in pos %d\n",sci_pdu->second_stage_sci_format,pos-fsize);

  // beta_offset_indicator // 2 bits - depending sl-BetaOffsets2ndSCI and Table 8.3.1.1-2
  fsize = 2;
  pos+=fsize;
  sci_pdu->beta_offset_indicator = *sci1_payload>>(sci1_size-pos)&((1<<fsize)-1);
  LOG_D(NR_MAC,"beta_offset_indicator (%d) in pos %d\n",sci_pdu->beta_offset_indicator,pos-fsize);

  // number_of_dmrs_port // 1 bit - Table 8.3.1.1-3
  fsize = 1;
  pos+=fsize;
  sci_pdu->number_of_dmrs_port = *sci1_payload>>(sci1_size-pos)&((1<<fsize)-1);
  LOG_D(NR_MAC,"number_of_dmrs_port (%d) in pos %d\n",sci_pdu->number_of_dmrs_port,pos-fsize);

  // mcs // 5 bits
  fsize = 5;
  pos+=fsize;
  sci_pdu->mcs = *sci1_payload>>(sci1_size-pos)&((1<<fsize)-1);
  LOG_D(NR_MAC,"mcs (%d,%d) in pos %d\n",sci_pdu->mcs,5,pos-fsize);
  // additional_mcs; // depending on sl-Additional-MCS-Table
  fsize = sci_pdu->additional_mcs.nbits;
  pos+=fsize;
  sci_pdu->additional_mcs.val = *sci1_payload>>(sci1_size-pos)&((1<<fsize)-1);
  LOG_D(NR_MAC,"additional mcs (%d,%d) in pos %d\n",sci_pdu->additional_mcs.val,sci_pdu->additional_mcs.nbits,pos-fsize);

  // psfch_overhead; // depending on sl-PSFCH-Period
  fsize = sci_pdu->psfch_overhead.nbits;
  pos+=fsize;
  sci_pdu->psfch_overhead.val = *sci1_payload>>(sci1_size-pos)&((1<<fsize)-1);
  LOG_D(NR_MAC, "psfch overhead (%d,%d) in pos %d\n", sci_pdu->psfch_overhead.val, sci_pdu->psfch_overhead.nbits, pos - fsize);

  // reserved; // depending on N_reserved (sl-NumReservedBits) and sl-IndicationUE-B
  fsize = sci_pdu->reserved.nbits;
  pos+=fsize;
  sci_pdu->reserved.val = *sci1_payload>>(sci1_size-pos)&((1<<fsize)-1);
  LOG_D(NR_MAC,"reserved (%d,%d) in pos %d, pos=%d\n",sci_pdu->reserved.val,sci_pdu->reserved.nbits,pos-fsize,pos);

  // conflict_information_receiver; // depending on sl-IndicationUE-B
  fsize = sci_pdu->conflict_information_receiver.nbits;
  pos+=fsize;
  sci_pdu->conflict_information_receiver.val = *sci1_payload>>(sci1_size-pos)&((1<<fsize)-1);
  LOG_D(NR_MAC,"conflict_information (%d, %d) in pos %d, pos=%d\n",sci_pdu->conflict_information_receiver.val,sci_pdu->conflict_information_receiver.nbits,pos-fsize,pos);
}

int nr_ue_process_sci1_indication_pdu(NR_UE_MAC_INST_t *mac,module_id_t mod_id,frame_t frame, int slot, sl_nr_sci_indication_pdu_t *sci,void *phy_data) {

  nr_sci_pdu_t *sci_pdu = &mac->sci_pdu_rx;  //&mac->def_sci_pdu[slot][sci->sci_format_type];
  sl_nr_ue_mac_params_t *sl_mac = mac->SL_MAC_PARAMS;
  memset(sci_pdu,0,sizeof(*sci_pdu));
  const NR_SL_BWP_ConfigCommon_r16_t *sl_bwp = mac->sl_bwp;
  const NR_SL_ResourcePool_r16_t *sl_res_pool = mac->sl_rx_res_pool;

  LOG_D(NR_MAC,"%4d.%2d Received sci indication (sci format %d, Nid %x, subChannelIndex %d, payloadSize %d,payload %llx) pscch_rsrp %d\n",
        frame, slot, sci->sci_format_type,sci->Nid,sci->subch_index,sci->sci_payloadlen,*(unsigned long long*)sci->sci_payloadBits, sci->pscch_rsrp);
  AssertFatal(sci->sci_format_type == SL_SCI_FORMAT_1A_ON_PSCCH, "need to have format 1A here only\n");
  extract_pscch_pdu((uint64_t *)sci->sci_payloadBits, sci->sci_payloadlen,sl_bwp, sl_res_pool, sci_pdu);

  uint16_t l_subch;
  convNRFRIV(sci_pdu->frequency_resource_assignment.val,
	          *sl_res_pool->sl_NumSubchannel_r16,
	          *sl_res_pool->sl_UE_SelectedConfigRP_r16->sl_MaxNumPerReserve_r16,
	          &l_subch,
	          NULL,NULL);
  sensing_data_t sensing_data;
  sensing_data.frame_slot.frame = frame;
  sensing_data.frame_slot.slot = slot;
  sensing_data.sl_rsrp = sci->pscch_rsrp;
  sensing_data.rsvp = sl_mac->mac_tx_params.rri;
  sensing_data.subch_startre_tx1 = 0;
  sensing_data.subch_startre_tx2 = 0;
  sensing_data.subch_start = *mac->sl_tx_res_pool->sl_StartRB_Subchannel_r16;
  sensing_data.subch_len = l_subch;
  push_back(&mac->sl_sensing_data, &sensing_data);
  if (mac->sl_sensing_data.size > 1)
    remove_old_sensing_data(&sensing_data.frame_slot, sl_mac->sl_RxPool[0]->t0, &mac->sl_sensing_data, sl_mac);

  if (sci_pdu->reserved.val && !mac->is_synced) {
    mac->is_synced = true;
    LOG_D(NR_PHY, "Nearby UE is synced now!!!\n");
  }
  LOG_D(NR_MAC,"SCI1A: frequency_resource %d, time_resource %d, dmrs_pattern %d, beta_offset_indicator %d, mcs %d, number_of_dmrs_port %d, 2nd stage SCI format %d\n",
        sci_pdu->frequency_resource_assignment.val,sci_pdu->time_resource_assignment.val,sci_pdu->dmrs_pattern.val,sci_pdu->beta_offset_indicator,sci_pdu->mcs,sci_pdu->number_of_dmrs_port,sci_pdu->second_stage_sci_format);
  // send schedule response

  sl_nr_rx_config_request_t rx_config;
  rx_config.number_pdus = 1;
  rx_config.sfn = frame;
  rx_config.slot = slot;

  uint8_t psfch_period = 0;
  const uint8_t psfch_periods[] = {0,1,2,4};
  psfch_period = (mac->sl_tx_res_pool->sl_PSFCH_Config_r16 &&
                  mac->sl_tx_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_Period_r16)
                  ? psfch_periods[*mac->sl_tx_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_Period_r16] : 0;
  frameslot_t fs;
  fs.frame = frame;
  fs.slot = slot;
  uint8_t pool_id = 0;
  SL_ResourcePool_params_t *sl_rx_rsrc_pool = mac->SL_MAC_PARAMS->sl_RxPool[pool_id];
  uint8_t mu = sl_mac->sl_phy_config.sl_config_req.sl_bwp_config.sl_scs;
  uint64_t rx_abs_slot = normalize(&fs, mu);
  uint16_t phy_map_sz = ((sl_rx_rsrc_pool->phy_sl_bitmap.size << 3) - sl_rx_rsrc_pool->phy_sl_bitmap.bits_unused);
  uint8_t sl_has_psfch = slot_has_psfch(mac, &sl_rx_rsrc_pool->phy_sl_bitmap, rx_abs_slot, psfch_period, phy_map_sz, mac->SL_MAC_PARAMS->sl_TDD_config);

  int ret = config_pssch_sci_pdu_rx(&rx_config.sl_rx_config_list[0].rx_sci2_config_pdu,
                          NR_SL_SCI_FORMAT_2A,
                          sci_pdu,
                          sci->Nid,
                          sci->subch_index,
                          sl_bwp,
                          sl_res_pool,
                          sl_has_psfch);
  if (ret<0) return(ret);
  rx_config.sl_rx_config_list[0].pdu_type =  SL_NR_CONFIG_TYPE_RX_PSSCH_SCI;

  nr_scheduled_response_t scheduled_response;
  memset(&scheduled_response,0, sizeof(nr_scheduled_response_t));

  fill_scheduled_response(&scheduled_response,NULL,NULL,NULL,&rx_config,NULL,mod_id,0,frame,slot,phy_data);
  LOG_D(NR_MAC, "[UE%d] TTI-%d:%d RX PSSCH_SCI REQ \n", mod_id,frame, slot);
  if ((mac->if_module != NULL) && (mac->if_module->scheduled_response != NULL))
      mac->if_module->scheduled_response(&scheduled_response);
  return 1;
}



void config_pssch_slsch_pdu_rx(sl_nr_rx_config_pssch_pdu_t *nr_sl_pssch_pdu,
                               nr_sci_pdu_t *sci_pdu,
                               const NR_SL_BWP_ConfigCommon_r16_t *sl_bwp,
                               const NR_SL_ResourcePool_r16_t *sl_res_pool,
                               sl_nr_ue_mac_params_t *sl_mac_params,
                               bool sl_has_psfch) {


  nr_sl_pssch_pdu->target_coderate = nr_get_code_rate_ul(sci_pdu->mcs,sci_pdu->additional_mcs.val);
  nr_sl_pssch_pdu->harq_pid=sci_pdu->harq_pid;
  nr_sl_pssch_pdu->mod_order=nr_get_Qm_ul(sci_pdu->mcs,sci_pdu->additional_mcs.val);
  nr_sl_pssch_pdu->mcs=sci_pdu->mcs;
  nr_sl_pssch_pdu->mcs_table=sci_pdu->additional_mcs.val;
  nr_sl_pssch_pdu->num_layers=1+(sci_pdu->number_of_dmrs_port&1);
  nr_sl_pssch_pdu->rv_index=sci_pdu->rv_index;
  nr_sl_pssch_pdu->ndi=sci_pdu->ndi;
  nr_sl_pssch_pdu->tbslbrm = nr_compute_tbslbrm(sci_pdu->additional_mcs.val,
		                                NRRIV2BW(sl_bwp->sl_BWP_Generic_r16->sl_BWP_r16->locationAndBandwidth,273),
						nr_sl_pssch_pdu->num_layers);
  int num_psfch_symbols = 0;
  if (sl_has_psfch && sl_res_pool->sl_PSFCH_Config_r16 && sl_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_Period_r16 && *sl_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_Period_r16>0) {
     // As per 38214 8.1.3.2, num_psfch_symbols can be 3 if psfch_overhead_indication.nbits is 1; FYI psfch_overhead_indication.nbits is set to 1 in case of PSFCH period 2 or 4 in sl_determine_sci_1a_len()
     num_psfch_symbols = 3;
  }
  int pssch_numsym = 7 + *sl_bwp->sl_BWP_Generic_r16->sl_LengthSymbols_r16 - num_psfch_symbols - 2;
  uint16_t l_subch;
  convNRFRIV(sci_pdu->frequency_resource_assignment.val,
	     *sl_res_pool->sl_NumSubchannel_r16,
	     *sl_res_pool->sl_UE_SelectedConfigRP_r16->sl_MaxNumPerReserve_r16,
	     &l_subch,
	     NULL,NULL);
  int subchannel_size=subch_to_rb[*sl_res_pool->sl_SubchannelSize_r16];
  int nohPRB    = (sl_res_pool->sl_X_Overhead_r16) ? 3*(*sl_res_pool->sl_X_Overhead_r16) : 0;
  int nREDMRS   = get_nREDMRS(sl_res_pool);
  int pscch_numsym = pscch_tda[*sl_res_pool->sl_PSCCH_Config_r16->choice.setup->sl_TimeResourcePSCCH_r16];
  int pscch_numrbs = pscch_rb_table[*sl_res_pool->sl_PSCCH_Config_r16->choice.setup->sl_FreqResourcePSCCH_r16];
  int num_CSI_REs= sci_pdu->csi_req ? get_nRECSI_RS(sl_mac_params->freq_density, sl_mac_params->nr_of_rbs) : 0;
  int num_CSI_REs_per_RB = sci_pdu->csi_req ? (num_CSI_REs/sl_mac_params->nr_of_rbs) : 0;
  int N_REprime = 12 * pssch_numsym - nohPRB - nREDMRS - num_CSI_REs_per_RB;
  int N_REsci1  = 12*pscch_numrbs*pscch_numsym;
  AssertFatal(*sl_res_pool->sl_PSSCH_Config_r16->choice.setup->sl_Scaling_r16 < 4, "Illegal index %d to alpha table\n",(int)*sl_res_pool->sl_PSSCH_Config_r16->choice.setup->sl_Scaling_r16);
  int sci2_beta_offset = *sl_res_pool->sl_PSSCH_Config_r16->choice.setup->sl_BetaOffsets2ndSCI_r16->list.array[sci_pdu->beta_offset_indicator];
  int sci2_alpha_times_100=0;
  if (sl_res_pool->sl_PowerControl_r16) {
    sci2_alpha_times_100=50 + (*sl_res_pool->sl_PowerControl_r16->sl_Alpha_PSSCH_PSCCH_r16)*15;
    if (*sl_res_pool->sl_PowerControl_r16->sl_Alpha_PSSCH_PSCCH_r16 == 3) sci2_alpha_times_100=100;
  } else sci2_alpha_times_100 = 100;
  int sci2_payload_len = nr_sci_size(sl_res_pool, sci_pdu, NR_SL_SCI_FORMAT_2A);
  int N_REsci2  = get_NREsci2(sci2_alpha_times_100,
                              sci2_payload_len,
                              sci2_beta_offset,
                              pssch_numsym,
                              pscch_numsym,
                              pscch_numrbs,
                              l_subch,
                              subchannel_size,
                              get_softmodem_params()->sl_mode ? 1 : nr_sl_pssch_pdu->mcs,
                              nr_sl_pssch_pdu->mcs_table);
  int N_RE      = N_REprime*l_subch*subchannel_size - N_REsci1 - N_REsci2;
  nr_sl_pssch_pdu->tb_size = (nr_compute_tbs_sl(nr_sl_pssch_pdu->mod_order,
                                               nr_sl_pssch_pdu->target_coderate,
                                               N_RE,1+(sci_pdu->number_of_dmrs_port&1))+7)>>3;
}

int config_pssch_sci_pdu_rx(sl_nr_rx_config_pssch_sci_pdu_t *nr_sl_pssch_sci_pdu,
                            nr_sci_format_t sci2_format,
                            nr_sci_pdu_t *sci_pdu,
                            uint32_t pscch_Nid,
                            int pscch_subchannel_index,
                            const NR_SL_BWP_ConfigCommon_r16_t *sl_bwp,
                            const NR_SL_ResourcePool_r16_t *sl_res_pool,
                            bool sl_has_psfch) {

  AssertFatal(sci2_format>NR_SL_SCI_FORMAT_1A,"cannot use format 1A with this function\n");
  // Expected Length of SCI2 in bits
  nr_sl_pssch_sci_pdu->sci2_len = nr_sci_size(sl_res_pool,sci_pdu,sci2_format);
  // Used to determine number of SCI2 modulated symbols
  nr_sl_pssch_sci_pdu->sci2_beta_offset = *sl_res_pool->sl_PSSCH_Config_r16->choice.setup->sl_BetaOffsets2ndSCI_r16->list.array[sci_pdu->beta_offset_indicator];
  // Used to determine number of SCI2 modulated symbols
 //Values will be sl-scaling*100 (sl-scaling values 0.5, 0.65, 0.8, 1)
  nr_sl_pssch_sci_pdu->sci2_alpha_times_100=50 + (*sl_res_pool->sl_PowerControl_r16->sl_Alpha_PSSCH_PSCCH_r16)*15;
  if (*sl_res_pool->sl_PowerControl_r16->sl_Alpha_PSSCH_PSCCH_r16 == 3) nr_sl_pssch_sci_pdu->sci2_alpha_times_100=100;

  int mcs_tb_ind = 0;
  if (sci_pdu->additional_mcs.nbits > 0)
    mcs_tb_ind = sci_pdu->additional_mcs.val;

  nr_sl_pssch_sci_pdu->targetCodeRate = nr_get_code_rate_ul(sci_pdu->mcs,mcs_tb_ind);
  nr_sl_pssch_sci_pdu->mod_order      = nr_get_Qm_ul(sci_pdu->mcs,mcs_tb_ind);
  nr_sl_pssch_sci_pdu->num_layers     = 1+sci_pdu->number_of_dmrs_port;

  // Derived from PSCCH CRC Refer 38.211 section 8.3.1.1
  // to be used for PSSCH DMRS and PSSCH 38.211 Scrambling
  nr_sl_pssch_sci_pdu->Nid = pscch_Nid;

  // Starting RE of the lowest subchannel.
  //In Sym with PSCCH - Start of PSCCH
  //In Sym without PSCCH - Start of PSSCH
  // freq domain allocation starts
  nr_sl_pssch_sci_pdu->startrb = pscch_subchannel_index*12*(*sl_res_pool->sl_SubchannelSize_r16);
  // Number of symbols used for PSCCH
  nr_sl_pssch_sci_pdu->pscch_numsym = pscch_tda[*sl_res_pool->sl_PSCCH_Config_r16->choice.setup->sl_TimeResourcePSCCH_r16];
  // Number of  RBS used for PSCCH
  nr_sl_pssch_sci_pdu->pscch_numrbs = pscch_rb_table[*sl_res_pool->sl_PSCCH_Config_r16->choice.setup->sl_FreqResourcePSCCH_r16];
 ;
  // num subchannels in a resource pool
  nr_sl_pssch_sci_pdu->num_subch = *sl_res_pool->sl_NumSubchannel_r16;
  // Size of subchannels in RBs
  nr_sl_pssch_sci_pdu->subchannel_size = subch_to_rb[*sl_res_pool->sl_SubchannelSize_r16];
  // In case of PSCCH PSSCH RX: this is always 1. Blind decoding done for every channel
  // In case of RESOURCE SENSING: this is equal to number of subchannels forming a resource.
  nr_sl_pssch_sci_pdu->l_subch = 1;
  //number of symbols for Sidelink transmission on PSSCH/PSCCH
  //(Total Sidelink symbols available - number of psfch symbols configured - 2)
  //Guard symbol + AGC symbol are also excluded
  //Indicates the number of symbols for PSCCH+PSSCH txn
  int num_psfch_symbols = 0;
  if (sl_has_psfch && sl_res_pool->sl_PSFCH_Config_r16 && sl_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_Period_r16 && *sl_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_Period_r16>0) {
     // As per 38214 8.1.3.2, num_psfch_symbols can be 3 if psfch_overhead_indication.nbits is 1; FYI psfch_overhead_indication.nbits is set to 1 in case of PSFCH period 2 or 4 in sl_determine_sci_1a_len()
     num_psfch_symbols = 3;
  }
  nr_sl_pssch_sci_pdu->pssch_numsym = 7+*sl_bwp->sl_BWP_Generic_r16->sl_LengthSymbols_r16-num_psfch_symbols-2;

  //DMRS SYMBOL MASK. If bit set to 1 indicates it is a DMRS symbol. LSB is symbol 0
  // Table from SPEC 38.211, Table 8.4.1.1.2-1
  int num_dmrs_symbols;
  if (sci_pdu->dmrs_pattern.val >= sl_res_pool->sl_PSSCH_Config_r16->choice.setup->sl_PSSCH_DMRS_TimePatternList_r16->list.count) {
	  LOG_W(NR_MAC,"dmrs.pattern %d out of bounds for list size %d\n",sci_pdu->dmrs_pattern.val,sl_res_pool->sl_PSSCH_Config_r16->choice.setup->sl_PSSCH_DMRS_TimePatternList_r16->list.count);
	  sci_pdu->dmrs_pattern.val = 0;
	  return(-1);
  }
  num_dmrs_symbols = *sl_res_pool->sl_PSSCH_Config_r16->choice.setup->sl_PSSCH_DMRS_TimePatternList_r16->list.array[sci_pdu->dmrs_pattern.val];
  if (num_dmrs_symbols == 2) {
    if (nr_sl_pssch_sci_pdu->pssch_numsym<=5){
        LOG_I(NR_MAC,"num_pssch_ymbols %d is not ok for 2 DMRS (min 6)\n",nr_sl_pssch_sci_pdu->pssch_numsym);
	return(-1);
    }
    nr_sl_pssch_sci_pdu->dmrs_symbol_position = sl_dmrs_mask2[nr_sl_pssch_sci_pdu->pscch_numsym-2][nr_sl_pssch_sci_pdu->pssch_numsym-6];
  } else if (num_dmrs_symbols == 3) {
    if (nr_sl_pssch_sci_pdu->pssch_numsym<=8) {
	LOG_I(NR_MAC,"num_pssch_ymbols %d is not ok for 3 DMRS (min 9)\n",nr_sl_pssch_sci_pdu->pssch_numsym);
        return(-1);
    }
    nr_sl_pssch_sci_pdu->dmrs_symbol_position = sl_dmrs_mask3[nr_sl_pssch_sci_pdu->pssch_numsym-9];
  } else if (num_dmrs_symbols == 4) {
    if (nr_sl_pssch_sci_pdu->pssch_numsym<=10) {
	LOG_I(NR_MAC,"num_pssch_ymbols %d is not ok for 4 DMRS (min 11) sci_pdu->dmrs_pattern.val %d\n",nr_sl_pssch_sci_pdu->pssch_numsym,sci_pdu->dmrs_pattern.val);
        return(-1);
    }
    nr_sl_pssch_sci_pdu->dmrs_symbol_position = sl_dmrs_mask4[nr_sl_pssch_sci_pdu->pssch_numsym-11];
  }

  //This paramter is set if PSSCH sensing (PSSCH DMRS RSRP measurement)
  // is triggred as part of TX pool sensing procedure.
  nr_sl_pssch_sci_pdu->sense_pssch = 0;
  return(0);

}


int nr_ue_process_sci2_indication_pdu(NR_UE_MAC_INST_t *mac, module_id_t mod_id, int cc_id, frame_t frame, int slot, sl_nr_sci_indication_pdu_t *sci, void *phy_data) {

  nr_sci_pdu_t *sci_pdu = &mac->sci_pdu_rx;  //&mac->def_sci_pdu[slot][sci->sci_format_type];
  sl_nr_ue_mac_params_t *sl_mac_params = mac->SL_MAC_PARAMS;
  const NR_SL_BWP_ConfigCommon_r16_t *sl_bwp = mac->sl_bwp;
  const NR_SL_ResourcePool_r16_t *sl_res_pool = mac->sl_rx_res_pool;
  LOG_D(NR_MAC,"Received sci indication (sci format %d, Nid %x, subChannelIndex %d, payloadSize %d,payload %llx)\n",
        sci->sci_format_type,sci->Nid,sci->subch_index,sci->sci_payloadlen,*(unsigned long long*)sci->sci_payloadBits);
  AssertFatal(sci->sci_format_type == SL_SCI_FORMAT_2_ON_PSSCH, "need to have format 2 here only\n");
  extract_pssch_sci_pdu((uint64_t *)sci->sci_payloadBits, sci->sci_payloadlen,sl_bwp, sl_res_pool, sci_pdu);

  LOG_D(NR_MAC,"SCI2A: harq_pid %d ndi %d RV %d SRC %x DST %x HARQ_FB %d Cast %d CSI_Req %d\n", sci_pdu->harq_pid,sci_pdu->ndi,sci_pdu->rv_index,sci_pdu->source_id,sci_pdu->dest_id,sci_pdu->harq_feedback,sci_pdu->cast_type,sci_pdu->csi_req);
  // send schedule response

  sl_nr_rx_config_request_t rx_config;
  rx_config.number_pdus = 1;
  rx_config.sfn = frame;
  rx_config.slot = slot;

  uint8_t psfch_period = 0;
  const uint8_t psfch_periods[] = {0,1,2,4};
  psfch_period = (mac->sl_rx_res_pool->sl_PSFCH_Config_r16 &&
                  mac->sl_rx_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_Period_r16)
                  ? psfch_periods[*mac->sl_rx_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_Period_r16] : 0;
  frameslot_t fs;
  fs.frame = frame;
  fs.slot = slot;
  uint8_t mu = sl_mac_params->sl_phy_config.sl_config_req.sl_bwp_config.sl_scs;
  uint64_t tx_abs_slot = normalize(&fs, mu);
  uint8_t pool_id = 0;
  SL_ResourcePool_params_t *sl_rx_rsrc_pool = sl_mac_params->sl_RxPool[pool_id];
  uint16_t phy_map_sz = ((sl_rx_rsrc_pool->phy_sl_bitmap.size << 3) - sl_rx_rsrc_pool->phy_sl_bitmap.bits_unused);
  uint8_t sl_has_psfch = slot_has_psfch(mac, &sl_rx_rsrc_pool->phy_sl_bitmap, tx_abs_slot, psfch_period, phy_map_sz, mac->SL_MAC_PARAMS->sl_TDD_config);

  config_pssch_slsch_pdu_rx(&rx_config.sl_rx_config_list[0].rx_pssch_config_pdu,
                            sci_pdu,
                            sl_bwp,
                            sl_res_pool,
                            sl_mac_params,
                            sl_has_psfch);
  rx_config.sl_rx_config_list[0].pdu_type =  SL_NR_CONFIG_TYPE_RX_PSSCH_SLSCH;
  if ((!mac->SL_MAC_PARAMS->sl_CSI_Acquisition) && sci_pdu->csi_req) {
    sl_nr_phy_config_request_t *sl_cfg = &sl_mac_params->sl_phy_config.sl_config_req;
    uint8_t mu = sl_cfg->sl_bwp_config.sl_scs;
    nr_ue_sl_csi_rs_scheduler(mac, mu, mac->sl_bwp, NULL, &rx_config, NULL);
  }

  LOG_D(NR_MAC, "%4d.%2d psfch_overhead %d harq_feedback %d action %d\n", frame, slot, mac->sci_pdu_rx.psfch_overhead.val, sci_pdu->harq_feedback, SL_NR_CONFIG_TYPE_RX_PSSCH_SLSCH);
  LOG_D(NR_MAC, "%4d.%2d psfch_period %d, psfch_overhead %d nbits %d\n", frame, slot, psfch_period, mac->sci_pdu_rx.psfch_overhead.val, mac->sci_pdu_rx.psfch_overhead.nbits);
  if ((psfch_period == 2 || psfch_period == 4) && mac->sci_pdu_rx.psfch_overhead.nbits && mac->sci_pdu_rx.psfch_overhead.val) {
    configure_psfch_params_rx(mod_id,
                              mac,
                              &rx_config);
  } else if (psfch_period == 1) {
    configure_psfch_params_rx(mod_id,
                              mac,
                              &rx_config);
  }
  nr_scheduled_response_t scheduled_response;
  memset(&scheduled_response,0, sizeof(nr_scheduled_response_t));

  fill_scheduled_response(&scheduled_response,NULL,NULL,NULL,&rx_config,NULL,mod_id,0,frame,slot,phy_data);
  LOG_D(NR_MAC, "[UE%d] TTI-%d:%d RX PSSCH_SLSCH REQ \n", mod_id,frame, slot);
  if ((mac->if_module != NULL) && (mac->if_module->scheduled_response != NULL))
      mac->if_module->scheduled_response(&scheduled_response);
  return 1;
}
void extract_pssch_sci_pdu(uint64_t *sci2_payload, int len,
                           const NR_SL_BWP_ConfigCommon_r16_t *sl_bwp,
                           const NR_SL_ResourcePool_r16_t *sl_res_pool,
                           nr_sci_pdu_t *sci_pdu) {
  int pos=0,fsize;
  int sci2_size = nr_sci_size(sl_res_pool,sci_pdu,NR_SL_SCI_FORMAT_2A);
  AssertFatal(sci2_size == len,"sci2a size %d is not the same sci_indication %d\n",sci2_size,len);


  //harq_pid; // 4 bits
  fsize=4;
  pos=fsize;
  sci_pdu->harq_pid = *sci2_payload>>(sci2_size-pos)&((1<<fsize)-1);
  LOG_D(NR_MAC,"harq_pid (%d) in pos %d\n",sci_pdu->harq_pid,pos-fsize);


  //ndi; // 1 bit
  fsize = 1;
  pos+=fsize;
  sci_pdu->ndi = *sci2_payload>>(sci2_size-pos)&((1<<fsize)-1);
  LOG_D(NR_MAC,"ndi (%d) in pos %d\n",sci_pdu->ndi,pos-fsize);

  //rv_index; // 2 bits
  fsize = 2;
  pos+=fsize;
  sci_pdu->rv_index = *sci2_payload>>(sci2_size-pos)&((1<<fsize)-1);
  LOG_D(NR_MAC,"rv_index (%d) in pos %d\n",sci_pdu->rv_index,pos-fsize);

  //source_id; // 8 bits
  fsize = 8;
  pos+=fsize;
  sci_pdu->source_id = *sci2_payload>>(sci2_size-pos)&((1<<fsize)-1);
  LOG_D(NR_MAC,"source_id (%d) in pos %d\n",sci_pdu->source_id,pos-fsize);

  //dest_id; // 16 bits
  fsize = 16;
  pos+=fsize;
  sci_pdu->dest_id = *sci2_payload>>(sci2_size-pos)&((1<<fsize)-1);
  LOG_D(NR_MAC,"dest_id (%d) in pos %d\n",sci_pdu->dest_id,pos-fsize);

  //harq_feedback; //1 bit
  fsize = 1;
  pos+=fsize;
  sci_pdu->harq_feedback = *sci2_payload>>(sci2_size-pos)&((1<<fsize)-1);
  LOG_D(NR_MAC,"harq_feedback (%d) in pos %d\n",sci_pdu->harq_feedback,pos-fsize);

  //cast_type // 2 bits formac 2A
  fsize = 2;
  pos+=fsize;
  sci_pdu->cast_type = *sci2_payload>>(sci2_size-pos)&((1<<fsize)-1);
  LOG_D(NR_MAC,"cast_type (%d) in pos %d\n",sci_pdu->cast_type,pos-fsize);

  // csi_req // 1 bit format 2A, format 2C
  fsize = 1;
  pos+=fsize;
  sci_pdu->csi_req = *sci2_payload>>(sci2_size-pos)&((1<<fsize)-1);
  LOG_D(NR_MAC,"csi_req (%d) in pos %d\n",sci_pdu->csi_req,pos-fsize);


}

