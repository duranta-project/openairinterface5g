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

#define RRC_SL_PRECONFIG
#define RRC_SL_PRECONFIG_C

#include "oai_asn1.h"
#include "NR_SL-PreconfigurationNR-r16.h"
#include "common/utils/LOG/log.h"
#include "sl_preconfig_paramvalues.h"
#include "common/config/config_userapi.h"
#include "rrc_defs.h"
#include "rrc_vars.h"
#include "LAYER2/NR_MAC_UE/mac_proto.h"
#include "RRC/NAS/nas_config.h"
#include "executables/nr-uesoftmodem.h"
#include "LAYER2/nr_rlc/nr_rlc_oai_api.h"
#include "LAYER2/nr_pdcp/nr_pdcp_oai_api.h"

#define GNSS_SUPPORT 0

#define SL_SYNC_SOURCE_NONE  0 //No sync source selected
#define SL_SYNC_SOURCE_GNBENB 1 // GNB/ENB as sync source
#define SL_SYNC_SOURCE_GNSS 2 // GPS as sync source
#define SL_SYNC_SOURCE_SYNC_REF_UE 3 // another SYNC REF UE as sync source
#define SL_SYNC_SOURCE_LOCAL_TIMING 4 //UE acts as sync source



static void prepare_NR_SL_SyncConfig(NR_SL_SyncConfig_r16_t *sl_syncconfig)
{

  // Hysteris when evaluating SyncRef UE
  sl_syncconfig->sl_SyncRefMinHyst_r16 = NULL;

  // Hysteris when evaluating SyncRef UE
  sl_syncconfig->sl_SyncRefDiffHyst_r16 = NULL;

  // Filtering for SL RSRP
  sl_syncconfig->sl_filterCoefficient_r16 = NULL;

  // SSB Periodicity within 16 frames.
  sl_syncconfig->sl_SSB_TimeAllocation1_r16 = calloc(1, sizeof(NR_SL_SSB_TimeAllocation_r16_t));
  sl_syncconfig->sl_SSB_TimeAllocation1_r16->sl_NumSSB_WithinPeriod_r16 = calloc(1, sizeof(long));
  sl_syncconfig->sl_SSB_TimeAllocation1_r16->sl_TimeOffsetSSB_r16 = calloc(1, sizeof(long));
  sl_syncconfig->sl_SSB_TimeAllocation1_r16->sl_TimeInterval_r16 = calloc(1, sizeof(long));
  sl_syncconfig->sl_SSB_TimeAllocation2_r16 = NULL;
  sl_syncconfig->sl_SSB_TimeAllocation3_r16 = NULL;

  //SLSS Id 
  sl_syncconfig->sl_SSID_r16 = NULL;

  // Threshold to be used in coverage
  sl_syncconfig->txParameters_r16.syncTxThreshIC_r16 = NULL;

  // Threshold to be used when Out of coverage
  sl_syncconfig->txParameters_r16.syncTxThreshOoC_r16 = NULL;

  // Syncconfig is used when UE is synced to GNSS if set, else if UE is synced to eNB/gNB
  sl_syncconfig->gnss_Sync_r16 = calloc(1, sizeof(long));
  *sl_syncconfig->gnss_Sync_r16 = 0; // GNSS

  char aprefix[MAX_OPTNAME_SIZE*2 + 8];
  paramdef_t SL_SYNCCFGPARAMS[] = SL_SYNCPARAMS_DESC(sl_syncconfig);
  paramlist_def_t SL_SYNCFGParamList = {SL_CONFIG_STRING_SL_SYNCCONFIG_LIST, NULL, 0};
  sprintf(aprefix, "%s.[%i]", SL_CONFIG_STRING_SL_PRECONFIGURATION, 0);
  config_getlist(&SL_SYNCFGParamList, NULL, 0, aprefix);
  LOG_I(RRC, "NUM SL-SYNCCFG elem in cfg file:%d\n", SL_SYNCFGParamList.numelt);
  sprintf(aprefix, "%s.[%i].%s.[%i]", SL_CONFIG_STRING_SL_PRECONFIGURATION, 0,SL_CONFIG_STRING_SL_SYNCCONFIG_LIST, 0);
  config_get(SL_SYNCCFGPARAMS,sizeof(SL_SYNCCFGPARAMS)/sizeof(paramdef_t),aprefix);
}

static void prepare_NR_SL_ResourcePool(NR_SL_ResourcePool_r16_t *sl_res_pool,
                                       uint16_t is_txpool,
                                       uint16_t is_sl_syncsource)
{

  // PSCCH configuration
  sl_res_pool->sl_PSCCH_Config_r16 = calloc(1, sizeof(*sl_res_pool->sl_PSCCH_Config_r16));
  sl_res_pool->sl_PSCCH_Config_r16->present = NR_SetupRelease_SL_PSCCH_Config_r16_PR_setup;
  sl_res_pool->sl_PSCCH_Config_r16->choice.setup = calloc(1, sizeof(NR_SL_PSCCH_Config_r16_t));
  // Indicates number of symbols for PSCCH in a resource pool
  sl_res_pool->sl_PSCCH_Config_r16->choice.setup->sl_TimeResourcePSCCH_r16 = calloc(1, sizeof(long));

  // Indicates number of PRBs for PSCCH in a resource pool
  sl_res_pool->sl_PSCCH_Config_r16->choice.setup->sl_FreqResourcePSCCH_r16 = calloc(1, sizeof(long));

  // Inititation during PSCCH DMRS Sequence generation
  sl_res_pool->sl_PSCCH_Config_r16->choice.setup->sl_DMRS_ScrambleID_r16 = calloc(1, sizeof(long));
  *sl_res_pool->sl_PSCCH_Config_r16->choice.setup->sl_DMRS_ScrambleID_r16 = 0;

  // num reserve bits used for first stage SCI
  sl_res_pool->sl_PSCCH_Config_r16->choice.setup->sl_NumReservedBits_r16 = calloc(1, sizeof(long));
  *sl_res_pool->sl_PSCCH_Config_r16->choice.setup->sl_NumReservedBits_r16 = 2;

  //PSSCH Configuration
  sl_res_pool->sl_PSSCH_Config_r16 = calloc(1, sizeof(NR_SetupRelease_SL_PSSCH_Config_r16_t));
  sl_res_pool->sl_PSSCH_Config_r16->present = NR_SetupRelease_SL_PSSCH_Config_r16_PR_setup;
  sl_res_pool->sl_PSSCH_Config_r16->choice.setup = calloc(1, sizeof(NR_SL_PSSCH_Config_r16_t));

  sl_res_pool->sl_PSSCH_Config_r16->choice.setup->sl_BetaOffsets2ndSCI_r16 =
                      calloc(1, sizeof(*sl_res_pool->sl_PSSCH_Config_r16->choice.setup->sl_BetaOffsets2ndSCI_r16));
  for(int i=0; i<4; i++) {
    long *p = calloc(1, sizeof(long));
    *p = i<<2; // valid values: 0...15, for the moment choose 0,4,8,12
    ASN_SEQUENCE_ADD(&sl_res_pool->sl_PSSCH_Config_r16->choice.setup->sl_BetaOffsets2ndSCI_r16->list, p);
  }

  sl_res_pool->sl_PSSCH_Config_r16->choice.setup->sl_PSSCH_DMRS_TimePatternList_r16 =
                      calloc(1, sizeof(*sl_res_pool->sl_PSSCH_Config_r16->choice.setup->sl_PSSCH_DMRS_TimePatternList_r16));
  for(int i=0; i<3; i++) {
    long *p = calloc(1, sizeof(long));
    *p = 2+i; // valid values: 2..4
    ASN_SEQUENCE_ADD(&sl_res_pool->sl_PSSCH_Config_r16->choice.setup->sl_PSSCH_DMRS_TimePatternList_r16->list, p);
  }

  // This should be added to configuration file
  sl_res_pool->sl_PSSCH_Config_r16->choice.setup->sl_Scaling_r16 = calloc(1,sizeof(*sl_res_pool->sl_PSSCH_Config_r16->choice.setup->sl_Scaling_r16));
  *sl_res_pool->sl_PSSCH_Config_r16->choice.setup->sl_Scaling_r16 = NR_SL_PSSCH_Config_r16__sl_Scaling_r16_f0p5;

  // PSFCH configuration
  sl_res_pool->sl_PSFCH_Config_r16 = calloc(1, sizeof(*sl_res_pool->sl_PSFCH_Config_r16));
  sl_res_pool->sl_PSFCH_Config_r16->present = NR_SetupRelease_SL_PSFCH_Config_r16_PR_setup;
  sl_res_pool->sl_PSFCH_Config_r16->choice.setup = calloc(1, sizeof(NR_SL_PSFCH_Config_r16_t));

  // Period of PSFCH resource in the unit of slots within this resource pool. If set to sl0, no resource for PSFCH,
  //and HARQ feedback for all transmissions in the resource pool is disabled.
  // {sl0, sl1, sl2, sl4}
  sl_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_Period_r16 = calloc(1, sizeof(long));

  // Set of PRBs that are actually used for PSFCH transmission and reception (bitmap)
  // 0xFFFFFFFFFFFF  (PRBs bitmap) Multiple of sl_NumSubchannel * sl_PSFCH_Period
  sl_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_RB_Set_r16 = calloc(1, sizeof(*sl_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_RB_Set_r16));

  // Number of cyclic shift pairs used for a PSFCH transmission that can be multiplexed in a PRB
  sl_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_NumMuxCS_Pair_r16 = calloc(1, sizeof(long));

  // The minimum time gap between PSFCH and the associated PSSCH in the unit of slots {sl2, sl3}
  sl_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_MinTimeGapPSFCH_r16 = calloc(1, sizeof(long));

  // Scrambling ID {0..1023} for sequence hopping of the PSFCH used in the resource pool
  sl_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_HopID_r16 = calloc(1, sizeof(long));

  // Indicates the number of PSFCH resources available {startSubCH, allocSubCH} for multiplexing HARQ-ACK information in a PSFCH transmission
  sl_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_CandidateResourceType_r16 = calloc(1, sizeof(long));

  // indicates allowed sync sources which are allowed to use this resource pool
  sl_res_pool->sl_SyncAllowed_r16 = calloc(1, sizeof(NR_SL_SyncAllowed_r16_t));

  //configured resources can be used if UE is directly/indirectly synced to network.
  sl_res_pool->sl_SyncAllowed_r16->gnbEnb_Sync_r16 = NULL;

  //configured resources can be used if UE is directly/indirectly synced to GNSS.
  sl_res_pool->sl_SyncAllowed_r16->gnss_Sync_r16 = calloc(1, sizeof(long));
  *sl_res_pool->sl_SyncAllowed_r16->gnss_Sync_r16 = NR_SL_SyncAllowed_r16__gnss_Sync_r16_true;

  //configured resources can be used if UE is directly/indirectly synced to SYNC REF UE.
  sl_res_pool->sl_SyncAllowed_r16->ue_Sync_r16 = calloc(1, sizeof(long));
  *sl_res_pool->sl_SyncAllowed_r16->ue_Sync_r16 = NR_SL_SyncAllowed_r16__ue_Sync_r16_true;

  //Min freq domain resources used for resource sensing. Size of Subchannels
  sl_res_pool->sl_SubchannelSize_r16 = calloc(1, sizeof(long));

  sl_res_pool->dummy = NULL;

  // lowest RB index of lowest subch in this resource pool
  sl_res_pool->sl_StartRB_Subchannel_r16 = calloc(1, sizeof(long));

  //number of subchannels in this res pool. contiguous PRBs
  sl_res_pool->sl_NumSubchannel_r16 = calloc(1, sizeof(long));


  // 64QAM table is default. in case other MCS tables needs tobe used.
  sl_res_pool->sl_Additional_MCS_Table_r16 = NULL;

  sl_res_pool->sl_ThreshS_RSSI_CBR_r16 = NULL;
  sl_res_pool->sl_TimeWindowSizeCBR_r16 = NULL;
  sl_res_pool->sl_TimeWindowSizeCR_r16 = NULL;
  sl_res_pool->sl_PTRS_Config_r16 = NULL;
  sl_res_pool->sl_UE_SelectedConfigRP_r16 = calloc(1,sizeof(*sl_res_pool->sl_UE_SelectedConfigRP_r16));
  sl_res_pool->sl_UE_SelectedConfigRP_r16->sl_MaxNumPerReserve_r16 = calloc(1,sizeof(*sl_res_pool->sl_UE_SelectedConfigRP_r16->sl_MaxNumPerReserve_r16));

  sl_res_pool->sl_RxParametersNcell_r16 = NULL;
  sl_res_pool->sl_ZoneConfigMCR_List_r16 = NULL;
  sl_res_pool->sl_FilterCoefficient_r16 = NULL;

  //number of contiguous PRBS in this res pool.
  sl_res_pool->sl_RB_Number_r16 = calloc(1, sizeof(long));

  sl_res_pool->sl_PreemptionEnable_r16 = NULL;
  sl_res_pool->sl_PriorityThreshold_UL_URLLC_r16 = NULL;
  sl_res_pool->sl_PriorityThreshold_r16 = NULL;
  sl_res_pool->sl_X_Overhead_r16 = NULL;
  sl_res_pool->sl_PowerControl_r16 = calloc(1,sizeof(*sl_res_pool->sl_PowerControl_r16));
  sl_res_pool->sl_PowerControl_r16->sl_Alpha_PSSCH_PSCCH_r16 = calloc(1,sizeof(*sl_res_pool->sl_PowerControl_r16->sl_Alpha_PSSCH_PSCCH_r16));
  *sl_res_pool->sl_PowerControl_r16->sl_Alpha_PSSCH_PSCCH_r16 = 0;
  
  sl_res_pool->sl_TxPercentageList_r16 = NULL;
  sl_res_pool->sl_MinMaxMCS_List_r16 = NULL;

  sl_res_pool->ext1 = calloc(1, sizeof(*sl_res_pool->ext1));
  sl_res_pool->ext1->sl_TimeResource_r16 = calloc(1, sizeof(*sl_res_pool->ext1->sl_TimeResource_r16));
  sl_res_pool->ext1->sl_TimeResource_r16->size = 8;
  sl_res_pool->ext1->sl_TimeResource_r16->bits_unused = 4;
  sl_res_pool->ext1->sl_TimeResource_r16->buf = calloc(sl_res_pool->ext1->sl_TimeResource_r16->size, sizeof(uint8_t));
  // EX: BITMAP 10101010.. indicating every alternating slot supported for sidelink
  for (int i=0;i<sl_res_pool->ext1->sl_TimeResource_r16->size;i++) {
    if (is_txpool) {
        sl_res_pool->ext1->sl_TimeResource_r16->buf[i] = (is_sl_syncsource) ? 0xF0 //0x88;//0xAA;
                                                                            : 0x0F;//0x11;//0x55;
    } else {
        sl_res_pool->ext1->sl_TimeResource_r16->buf[i] = (is_sl_syncsource) ? 0x0F //0x88;//0xAA;
                                                                            : 0xF0;//0x11;//0x55;
    }
  }

  // mask out unused bits
  sl_res_pool->ext1->sl_TimeResource_r16->buf[sl_res_pool->ext1->sl_TimeResource_r16->size - 1] &= (0 - (1 << (sl_res_pool->ext1->sl_TimeResource_r16->bits_unused)));

  char aprefix[MAX_OPTNAME_SIZE*2 + 8];
  paramdef_t SL_POOLPARAMS[] = SL_RESPOOLPARAMS_DESC(sl_res_pool);
  if (is_txpool)
    sprintf(aprefix, "%s.[%i].%s.[%i]", SL_CONFIG_STRING_SL_PRECONFIGURATION, 0,SL_CONFIG_STRING_SL_TX_RPOOL_LIST, 0);
  else
    sprintf(aprefix, "%s.[%i].%s.[%i]", SL_CONFIG_STRING_SL_PRECONFIGURATION, 0,SL_CONFIG_STRING_SL_RX_RPOOL_LIST, 0);

  config_get(SL_POOLPARAMS,sizeof(SL_POOLPARAMS)/sizeof(paramdef_t),aprefix);


  sl_res_pool->sl_UE_SelectedConfigRP_r16->sl_SelectionWindowList_r16 = calloc(1, sizeof(*sl_res_pool->sl_UE_SelectedConfigRP_r16->sl_SelectionWindowList_r16));
  struct NR_SL_UE_SelectedConfigRP_r16 *nr_sl_ue_Selected_config = sl_res_pool->sl_UE_SelectedConfigRP_r16;
  nr_sl_ue_Selected_config->sl_SelectionWindowList_r16->list.array = (NR_SL_SelectionWindowConfig_r16_t**)malloc16_clear(sizeof(NR_SL_SelectionWindowConfig_r16_t*));
  nr_sl_ue_Selected_config->sl_SelectionWindowList_r16->list.array[0] = (NR_SL_SelectionWindowConfig_r16_t*)malloc16_clear(sizeof(NR_SL_SelectionWindowConfig_r16_t));

  nr_sl_ue_Selected_config->sl_SensingWindow_r16 = calloc(1, sizeof(*nr_sl_ue_Selected_config->sl_SensingWindow_r16));

  nr_sl_ue_Selected_config->sl_Thres_RSRP_List_r16 = calloc(1, sizeof(*nr_sl_ue_Selected_config->sl_Thres_RSRP_List_r16));
  nr_sl_ue_Selected_config->sl_Thres_RSRP_List_r16->list.array = (NR_SL_Thres_RSRP_r16_t**)malloc16_clear(sizeof(NR_SL_Thres_RSRP_r16_t*));
  nr_sl_ue_Selected_config->sl_Thres_RSRP_List_r16->list.array[0] = (NR_SL_Thres_RSRP_r16_t*)malloc16_clear(sizeof(NR_SL_Thres_RSRP_r16_t));

  nr_sl_ue_Selected_config->sl_MaxNumPerReserve_r16 = calloc(1, sizeof(*nr_sl_ue_Selected_config->sl_MaxNumPerReserve_r16));

  nr_sl_ue_Selected_config->sl_ResourceReservePeriodList_r16 = calloc(1, sizeof(*nr_sl_ue_Selected_config->sl_ResourceReservePeriodList_r16));
  nr_sl_ue_Selected_config->sl_ResourceReservePeriodList_r16->list.array = (NR_SL_ResourceReservePeriod_r16_t**)malloc16_clear(sizeof(NR_SL_ResourceReservePeriod_r16_t*));
  nr_sl_ue_Selected_config->sl_ResourceReservePeriodList_r16->list.array[0] = (NR_SL_ResourceReservePeriod_r16_t*)malloc16_clear(sizeof(NR_SL_ResourceReservePeriod_r16_t));

  char aprefix_rsc_sel[MAX_OPTNAME_SIZE*2 + 8];
  paramdef_t SL_RSCSELECTIONPARAMS[] = SL_RSRCSELPARAMS_DESC(sl_res_pool);
  sprintf(aprefix_rsc_sel, "%s.[%i].%s.[%i]", SL_CONFIG_STRING_SL_PRECONFIGURATION, 0,SL_CONFIG_STRING_RSRC_SEL_PARAMS_LIST, 0);
  config_get(SL_RSCSELECTIONPARAMS,sizeof(SL_RSCSELECTIONPARAMS)/sizeof(paramdef_t),aprefix_rsc_sel);
  LOG_D(NR_RRC, "sl_MaxNumPerReserve %ld, sl_SensingWindow %ld, sl_Priority %ld, sl_SelectionWindow %ld, sl_ResourceReservePeriod1 %ld\n",
        *sl_res_pool->sl_UE_SelectedConfigRP_r16->sl_MaxNumPerReserve_r16,
        *sl_res_pool->sl_UE_SelectedConfigRP_r16->sl_SensingWindow_r16,
        sl_res_pool->sl_UE_SelectedConfigRP_r16->sl_SelectionWindowList_r16->list.array[0]->sl_Priority_r16,
        sl_res_pool->sl_UE_SelectedConfigRP_r16->sl_SelectionWindowList_r16->list.array[0]->sl_SelectionWindow_r16,
        sl_res_pool->sl_UE_SelectedConfigRP_r16->sl_ResourceReservePeriodList_r16->list.array[0]->choice.sl_ResourceReservePeriod1_r16);
  struct NR_SL_PSFCH_Config_r16 *nr_sl_psfch_config = sl_res_pool->sl_PSFCH_Config_r16->choice.setup;
  if (*nr_sl_psfch_config->sl_PSFCH_Period_r16 > 0) {
    const uint8_t psfch_periods[] = {0,1,2,4};
    AssertFatal(*nr_sl_psfch_config->sl_PSFCH_Period_r16 < 4, "sl_PSFCH_Period_r16 index must be less than 4\n");
    LOG_D(NR_PHY, "Configuring PSFCH Period %d\n", psfch_periods[*nr_sl_psfch_config->sl_PSFCH_Period_r16]);
    uint8_t psfch_period = psfch_periods[*nr_sl_psfch_config->sl_PSFCH_Period_r16];
    uint16_t prod_numCh_period = *sl_res_pool->sl_NumSubchannel_r16*psfch_period;
    uint16_t num_prbs = (*sl_res_pool->sl_RB_Number_r16 / prod_numCh_period) * prod_numCh_period;
    uint16_t num_bytes = (num_prbs % 8) ? (num_prbs / 8) + 1 : (num_prbs / 8);
    sl_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_RB_Set_r16->size = num_bytes;

    sl_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_RB_Set_r16->bits_unused = (num_prbs % 8) ? 8 - (num_prbs % 8) : 0;
    sl_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_RB_Set_r16->buf = calloc(sl_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_RB_Set_r16->size, sizeof(uint8_t));
    memset(sl_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_RB_Set_r16->buf, 0xFF, num_prbs / 8);
    uint8_t remaining_prbs = 0;
    for (int i = 8 - (num_prbs % 8); i < 8; i++)
      remaining_prbs |= 1 << i;
    if ( num_prbs % 8 != 0 )
      sl_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_RB_Set_r16->buf[num_prbs/8] = remaining_prbs;
    LOG_D(RRC, "M: %d, PRBs %d, size in bytes %d, unused bits %d, full size bytes %d, remaining prbs %d\n", prod_numCh_period, num_prbs, num_bytes, (num_prbs % 8) ? 8 - (num_prbs % 8) : 0, num_prbs / 8, remaining_prbs);
  } else {
    LOG_I(NR_RRC,"Freeing sl_PSFCH_Config_r16\n");
    free(sl_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_CandidateResourceType_r16);
    free(sl_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_HopID_r16);
    free(sl_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_MinTimeGapPSFCH_r16);
    free(sl_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_NumMuxCS_Pair_r16);
    free(sl_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_RB_Set_r16);
    free(sl_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_Period_r16);
    free(sl_res_pool->sl_PSFCH_Config_r16->choice.setup);
    free(sl_res_pool->sl_PSFCH_Config_r16);
    sl_res_pool->sl_PSFCH_Config_r16 = NULL;
  }
}

static void prepare_NR_SL_BWPConfigCommon(NR_SL_BWP_ConfigCommon_r16_t *sl_bwp,
                                   uint16_t num_tx_pools,
                                   uint16_t num_rx_pools,
                                   uint16_t sl_syncsource)
{

  sl_bwp->sl_BWP_Generic_r16 = calloc(1, sizeof(NR_SL_BWP_Generic_r16_t));
  sl_bwp->sl_BWP_Generic_r16->sl_BWP_r16 = calloc(1, sizeof(NR_BWP_t));
  // if Cyclicprefix is NULL, then default value Normal cyclic prefix is configured. else EXT CP. 
  sl_bwp->sl_BWP_Generic_r16->sl_BWP_r16->cyclicPrefix = NULL;

  // Value can be between symbols 7 to 14
  sl_bwp->sl_BWP_Generic_r16->sl_LengthSymbols_r16 = calloc(1, sizeof(long));

  // Value can be between symbols 0 to 7
  sl_bwp->sl_BWP_Generic_r16->sl_StartSymbol_r16 = calloc(1, sizeof(long));

  sl_bwp->sl_BWP_Generic_r16->sl_PSBCH_Config_r16 = calloc(1,sizeof(NR_SL_PSBCH_Config_r16_t));
  // PSBCH CONFIG contains PO and alpha for PSBCH powercontrol.
  sl_bwp->sl_BWP_Generic_r16->sl_PSBCH_Config_r16->present = NR_SetupRelease_SL_PSBCH_Config_r16_PR_release;

  sl_bwp->sl_BWP_Generic_r16->sl_TxDirectCurrentLocation_r16 = NULL;

  char aprefix[MAX_OPTNAME_SIZE*2 + 8];
  paramdef_t SL_BWPPARAMS[] = SL_BWPPARAMS_DESC(sl_bwp);
  paramlist_def_t SL_BWPParamList = {SL_CONFIG_STRING_SL_BWP_LIST, NULL, 0};
  sprintf(aprefix, "%s.[%i]", SL_CONFIG_STRING_SL_PRECONFIGURATION, 0);
  config_getlist(&SL_BWPParamList, NULL, 0, aprefix);
  LOG_I(RRC, "NUM SL-BWP elem in cfg file:%d\n", SL_BWPParamList.numelt);
  sprintf(aprefix, "%s.[%i].%s.[%i]", SL_CONFIG_STRING_SL_PRECONFIGURATION, 0, SL_CONFIG_STRING_SL_BWP_LIST, 0);
  config_get( SL_BWPPARAMS,sizeof(SL_BWPPARAMS)/sizeof(paramdef_t),aprefix);

  sl_bwp->sl_BWP_PoolConfigCommon_r16 = calloc(1, sizeof(NR_SL_BWP_PoolConfigCommon_r16_t));

  paramlist_def_t SL_RxPoolParamList = {SL_CONFIG_STRING_SL_RX_RPOOL_LIST, NULL, 0};
  sprintf(aprefix, "%s.[%i]", SL_CONFIG_STRING_SL_PRECONFIGURATION, 0);
  config_getlist(&SL_RxPoolParamList, NULL, 0, aprefix);
  LOG_I(RRC, "NUM Rx RPOOLs in cfg file:%d\n", SL_RxPoolParamList.numelt);
  AssertFatal(SL_RxPoolParamList.numelt <= 1 && num_rx_pools <= 1, "Only Max 1 RX Respool Supported now\n");

  if (num_rx_pools || SL_RxPoolParamList.numelt) {
    // Receiving resource pool.
    NR_SL_ResourcePool_r16_t *respool = calloc(1, sizeof(*respool));
    sl_bwp->sl_BWP_PoolConfigCommon_r16->sl_RxPool_r16 = calloc(1, sizeof(*sl_bwp->sl_BWP_PoolConfigCommon_r16->sl_RxPool_r16));
    ASN_SEQUENCE_ADD(&sl_bwp->sl_BWP_PoolConfigCommon_r16->sl_RxPool_r16->list, respool);
    // Fill RX resource pool
    prepare_NR_SL_ResourcePool(sl_bwp->sl_BWP_PoolConfigCommon_r16->sl_RxPool_r16->list.array[0], 0, sl_syncsource);
  } else 
    sl_bwp->sl_BWP_PoolConfigCommon_r16->sl_RxPool_r16 = NULL;

  paramlist_def_t SL_TxPoolParamList = {SL_CONFIG_STRING_SL_TX_RPOOL_LIST, NULL, 0};
  sprintf(aprefix, "%s.[%i]", SL_CONFIG_STRING_SL_PRECONFIGURATION, 0);
  config_getlist(&SL_TxPoolParamList, NULL, 0, aprefix);
  LOG_I(RRC, "NUM Tx RPOOL in cfg file:%d\n", SL_TxPoolParamList.numelt);
  AssertFatal(SL_TxPoolParamList.numelt <= 1 && num_tx_pools <= 1, "Only Max 1 TX Respool Supported now\n");

  if (num_tx_pools || SL_TxPoolParamList.numelt) {
    //resource pool(s) to transmit NR SL 
    NR_SL_ResourcePoolConfig_r16_t *respoolcfg = calloc(1, sizeof(*respoolcfg));
    sl_bwp->sl_BWP_PoolConfigCommon_r16->sl_TxPoolSelectedNormal_r16 =
                  calloc(1, sizeof(*sl_bwp->sl_BWP_PoolConfigCommon_r16->sl_TxPoolSelectedNormal_r16));
    ASN_SEQUENCE_ADD(&sl_bwp->sl_BWP_PoolConfigCommon_r16->sl_TxPoolSelectedNormal_r16->list, respoolcfg);
    sl_bwp->sl_BWP_PoolConfigCommon_r16->sl_TxPoolSelectedNormal_r16->list.array[0]->sl_ResourcePoolID_r16 = 0;
    sl_bwp->sl_BWP_PoolConfigCommon_r16->sl_TxPoolSelectedNormal_r16->list.array[0]->sl_ResourcePool_r16 = calloc(1, sizeof(NR_SL_ResourcePool_r16_t));
    // Fill tx resource pool
    prepare_NR_SL_ResourcePool(sl_bwp->sl_BWP_PoolConfigCommon_r16->sl_TxPoolSelectedNormal_r16->list.array[0]->sl_ResourcePool_r16, 1, sl_syncsource);
  } else 
    sl_bwp->sl_BWP_PoolConfigCommon_r16->sl_TxPoolSelectedNormal_r16 = NULL;

  sl_bwp->sl_BWP_PoolConfigCommon_r16->sl_TxPoolExceptional_r16 = NULL;
}

static void prepare_NR_SL_FreqConfigCommon(NR_SL_FreqConfigCommon_r16_t *sl_fcfg,
                                    uint16_t num_tx_pools,
                                    uint16_t num_rx_pools,
                                    uint16_t sl_syncsource)
{

  // Sub carrier spacing used on this frequency configured.
  NR_SCS_SpecificCarrier_t *scs_specific = calloc(1, sizeof(*scs_specific));
  ASN_SEQUENCE_ADD(&sl_fcfg->sl_SCS_SpecificCarrierList_r16.list, scs_specific);

  // NR bands for Sidelink n47, n38. 
  // N47band - 5855Mhz - 5925Mhz
  //sl_fcfg->sl_AbsoluteFrequencyPointA_r16 = 792000; //freq 5880Mhz

  //SL SSB chosen to be located from RB10 to RB21. points to the middle of the SSB block.
  //SSB location should be within Sidelink BWP
  //792000 + 10*12*2 + 66*2. channel raster is 15Khz for band47
  sl_fcfg->sl_AbsoluteFrequencySSB_r16 = calloc(1, sizeof(NR_ARFCN_ValueNR_t));

  //NR SL transmission with a 7.5 Khz shift to the LTE raster. if absent, freq shift is disabled.
  //Required if carrier freq configured for NR SL shared by LTE SL
  sl_fcfg->frequencyShift7p5khzSL_r16 = NULL;

  //NR SL transmission with valueN*5Khz shift to LTE raster.
  sl_fcfg->valueN_r16 = 0;

  char aprefix[MAX_OPTNAME_SIZE*2 + 8];
  paramdef_t SL_FCCPARAMS[] = SL_FCCPARAMS_DESC(sl_fcfg);
  paramlist_def_t SL_FCCParamList = {SL_CONFIG_STRING_SL_FCC_LIST, NULL, 0};
  sprintf(aprefix, "%s.[%i]", SL_CONFIG_STRING_SL_PRECONFIGURATION,0);
  config_getlist(&SL_FCCParamList, NULL, 0, aprefix);
  LOG_I(RRC, "NUM SL-FCC elem in cfg file:%d\n", SL_FCCParamList.numelt);
  sprintf(aprefix, "%s.[%i].%s.[%i]", SL_CONFIG_STRING_SL_PRECONFIGURATION,0, SL_CONFIG_STRING_SL_FCC_LIST, 0);
  config_get( SL_FCCPARAMS,sizeof(SL_FCCPARAMS)/sizeof(paramdef_t),aprefix);

  // Sidelink BWP configuration. 
  // In REL16, 17 SUPPORTS only 1 SIDELINK Bandwidth part
  NR_SL_BWP_ConfigCommon_r16_t *bwpcfgcommon = calloc(1, sizeof(*bwpcfgcommon));
  sl_fcfg->sl_BWP_List_r16 = calloc(1, sizeof(*sl_fcfg->sl_BWP_List_r16));
  ASN_SEQUENCE_ADD(&sl_fcfg->sl_BWP_List_r16->list, bwpcfgcommon);
  prepare_NR_SL_BWPConfigCommon(sl_fcfg->sl_BWP_List_r16->list.array[0], num_tx_pools, num_rx_pools, sl_syncsource);

  // sync prio between GNSS and gNB/eNB
  sl_fcfg->sl_SyncPriority_r16 = calloc(1, sizeof(long));
  *sl_fcfg->sl_SyncPriority_r16 = 0; // Set to GNSS

  // If TRUE/1 - Network can be selected as sync source directly/indirectly in case syncprio = GNSS.
  sl_fcfg->sl_NbAsSync_r16 = calloc(1, sizeof(long));
  *sl_fcfg->sl_NbAsSync_r16 = 1;

  // config info related to rx and tx of SL SYNC SIGNALS (SLSS)
  NR_SL_SyncConfig_r16_t *synccfg = calloc(1, sizeof(*synccfg));
  sl_fcfg->sl_SyncConfigList_r16 = calloc(1, sizeof(*sl_fcfg->sl_SyncConfigList_r16));
  ASN_SEQUENCE_ADD(&sl_fcfg->sl_SyncConfigList_r16->list, synccfg);
  prepare_NR_SL_SyncConfig(sl_fcfg->sl_SyncConfigList_r16->list.array[0]);

}

NR_SL_PreconfigurationNR_r16_t *prepare_NR_SL_PRECONFIGURATION(uint16_t num_tx_pools,
                                                               uint16_t num_rx_pools,
                                                               uint16_t sl_syncsource)
{

  NR_SL_PreconfigurationNR_r16_t *sl_preconfiguration = CALLOC(1, sizeof(NR_SL_PreconfigurationNR_r16_t));
  NR_SidelinkPreconfigNR_r16_t *sl_preconfig = &sl_preconfiguration->sidelinkPreconfigNR_r16;

  //FILL in Frequency config common
  NR_SL_FreqConfigCommon_r16_t *freqcfgcommon = calloc(1, sizeof(*freqcfgcommon));
  sl_preconfig->sl_PreconfigFreqInfoList_r16 = calloc(1, sizeof(*sl_preconfig->sl_PreconfigFreqInfoList_r16));
  //Supported only 1 FREQs for NR SL communication.
  ASN_SEQUENCE_ADD(&sl_preconfig->sl_PreconfigFreqInfoList_r16->list, freqcfgcommon);

  prepare_NR_SL_FreqConfigCommon(sl_preconfig->sl_PreconfigFreqInfoList_r16->list.array[0],
                                 num_tx_pools, num_rx_pools, sl_syncsource);

  // NR Frequency list
  sl_preconfig->sl_PreconfigNR_AnchorCarrierFreqList_r16 = NULL;

  // EUTRA Frequency list
  sl_preconfig->sl_PreconfigEUTRA_AnchorCarrierFreqList_r16 = NULL;

  // NR sidelink radio bearer(s) configuration(s)
  sl_preconfig->sl_RadioBearerPreConfigList_r16 = calloc(1,sizeof(*sl_preconfig->sl_RadioBearerPreConfigList_r16)); // fill later
  struct NR_SL_RadioBearerConfig_r16 *sl_RadioBearerConfig_r16 = calloc(1,sizeof(*sl_RadioBearerConfig_r16));

  sl_RadioBearerConfig_r16->slrb_Uu_ConfigIndex_r16 = 1;
  sl_RadioBearerConfig_r16->sl_SDAP_Config_r16 = calloc(1, sizeof(*sl_RadioBearerConfig_r16->sl_SDAP_Config_r16));
  struct NR_SL_SDAP_Config_r16* sl_SDAP_Config = sl_RadioBearerConfig_r16->sl_SDAP_Config_r16;
  sl_SDAP_Config->sl_SDAP_Header_r16 = NR_SL_SDAP_Config_r16__sl_SDAP_Header_r16_present;
  sl_SDAP_Config->sl_DefaultRB_r16 = true;
  sl_SDAP_Config->sl_CastType_r16 = calloc(1, sizeof(*sl_SDAP_Config->sl_CastType_r16));
  *sl_SDAP_Config->sl_CastType_r16 = NR_SL_SDAP_Config_r16__sl_CastType_r16_unicast;
  sl_SDAP_Config->sl_MappedQoS_Flows_r16 = calloc(1, sizeof(*sl_SDAP_Config->sl_MappedQoS_Flows_r16));
  sl_SDAP_Config->sl_MappedQoS_Flows_r16->choice.sl_MappedQoS_FlowsList_r16 = calloc(1, sizeof(*sl_SDAP_Config->sl_MappedQoS_Flows_r16->choice.sl_MappedQoS_FlowsList_r16));
  struct NR_SL_SDAP_Config_r16__sl_MappedQoS_Flows_r16__sl_MappedQoS_FlowsList_r16* sl_MappedQoS_FlowList = sl_SDAP_Config->sl_MappedQoS_Flows_r16->choice.sl_MappedQoS_FlowsList_r16;

  NR_SL_QoS_Profile_r16_t *sl_QoS_Profile_r16_f1 = calloc(1, sizeof(*sl_QoS_Profile_r16_f1));
  sl_QoS_Profile_r16_f1->sl_PQI_r16 = calloc(1, sizeof(*sl_QoS_Profile_r16_f1->sl_PQI_r16));
  struct NR_SL_PQI_r16 *sl_PQI = sl_QoS_Profile_r16_f1->sl_PQI_r16;
  sl_PQI->choice.sl_StandardizedPQI_r16 = 55;

  ASN_SEQUENCE_ADD(&sl_MappedQoS_FlowList->list, sl_QoS_Profile_r16_f1);

  sl_RadioBearerConfig_r16->sl_TransRange_r16 = NULL;
  sl_RadioBearerConfig_r16->sl_PDCP_Config_r16 = calloc(1,sizeof(*sl_RadioBearerConfig_r16));
  sl_RadioBearerConfig_r16->sl_PDCP_Config_r16->sl_DiscardTimer_r16 = calloc(1,sizeof(*sl_RadioBearerConfig_r16->sl_PDCP_Config_r16->sl_DiscardTimer_r16));
  *sl_RadioBearerConfig_r16->sl_PDCP_Config_r16->sl_DiscardTimer_r16 = NR_SL_PDCP_Config_r16__sl_DiscardTimer_r16_infinity;
  sl_RadioBearerConfig_r16->sl_PDCP_Config_r16->sl_PDCP_SN_Size_r16 = calloc(1,sizeof(*sl_RadioBearerConfig_r16->sl_PDCP_Config_r16->sl_PDCP_SN_Size_r16));
  *sl_RadioBearerConfig_r16->sl_PDCP_Config_r16->sl_PDCP_SN_Size_r16 = NR_SL_PDCP_Config_r16__sl_PDCP_SN_Size_r16_len12bits;
  sl_RadioBearerConfig_r16->sl_PDCP_Config_r16->sl_OutOfOrderDelivery = NULL;
  ASN_SEQUENCE_ADD(&sl_preconfig->sl_RadioBearerPreConfigList_r16->list,sl_RadioBearerConfig_r16);
   
  // NR sidelink RLC bearer(s) configuration(s)
  sl_preconfig->sl_RLC_BearerPreConfigList_r16 = calloc(1,sizeof(*sl_preconfig->sl_RLC_BearerPreConfigList_r16)); 
  struct NR_SL_RLC_BearerConfig_r16 *sl_RLC_BearerConfig_r16 = calloc(1,sizeof(*sl_RLC_BearerConfig_r16));
  // initialize with UM for now
  sl_RLC_BearerConfig_r16->sl_RLC_BearerConfigIndex_r16 = 0;
  sl_RLC_BearerConfig_r16->sl_ServedRadioBearer_r16 = calloc(1,sizeof(*sl_RLC_BearerConfig_r16->sl_ServedRadioBearer_r16));
  *sl_RLC_BearerConfig_r16->sl_ServedRadioBearer_r16 = 1;
  sl_RLC_BearerConfig_r16->sl_RLC_Config_r16 = calloc(1,sizeof(*sl_RLC_BearerConfig_r16->sl_RLC_Config_r16));
  sl_RLC_BearerConfig_r16->sl_RLC_Config_r16->present = NR_SL_RLC_Config_r16_PR_sl_UM_RLC_r16;
  sl_RLC_BearerConfig_r16->sl_RLC_Config_r16->choice.sl_UM_RLC_r16 = calloc(1,sizeof(*sl_RLC_BearerConfig_r16->sl_RLC_Config_r16->choice.sl_UM_RLC_r16));
  sl_RLC_BearerConfig_r16->sl_RLC_Config_r16->choice.sl_UM_RLC_r16->sl_SN_FieldLengthUM_r16=calloc(1,sizeof(*sl_RLC_BearerConfig_r16->sl_MAC_LogicalChannelConfig_r16));  

  *sl_RLC_BearerConfig_r16->sl_RLC_Config_r16->choice.sl_UM_RLC_r16->sl_SN_FieldLengthUM_r16=NR_SN_FieldLengthUM_size6;  
  // Logical Channel Config for default link
  sl_RLC_BearerConfig_r16->sl_MAC_LogicalChannelConfig_r16 = calloc(1,sizeof(*sl_RLC_BearerConfig_r16->sl_MAC_LogicalChannelConfig_r16));
  sl_RLC_BearerConfig_r16->sl_MAC_LogicalChannelConfig_r16->sl_Priority_r16 = 1;
  sl_RLC_BearerConfig_r16->sl_MAC_LogicalChannelConfig_r16->sl_PrioritisedBitRate_r16 = NR_SL_LogicalChannelConfig_r16__sl_PrioritisedBitRate_r16_infinity;
  sl_RLC_BearerConfig_r16->sl_MAC_LogicalChannelConfig_r16->sl_BucketSizeDuration_r16 = NR_SL_LogicalChannelConfig_r16__sl_BucketSizeDuration_r16_ms5;
  sl_RLC_BearerConfig_r16->sl_MAC_LogicalChannelConfig_r16->sl_ConfiguredGrantType1Allowed_r16 = NULL;
  sl_RLC_BearerConfig_r16->sl_MAC_LogicalChannelConfig_r16->sl_HARQ_FeedbackEnabled_r16 = calloc(1,sizeof(*sl_RLC_BearerConfig_r16->sl_MAC_LogicalChannelConfig_r16->sl_HARQ_FeedbackEnabled_r16));
  *sl_RLC_BearerConfig_r16->sl_MAC_LogicalChannelConfig_r16->sl_HARQ_FeedbackEnabled_r16 = NR_SL_LogicalChannelConfig_r16__sl_HARQ_FeedbackEnabled_r16_enabled;
  sl_RLC_BearerConfig_r16->sl_MAC_LogicalChannelConfig_r16->sl_AllowedCG_List_r16 = NULL;
  sl_RLC_BearerConfig_r16->sl_MAC_LogicalChannelConfig_r16->sl_AllowedSCS_List_r16 = NULL; 
  sl_RLC_BearerConfig_r16->sl_MAC_LogicalChannelConfig_r16->sl_LogicalChannelGroup_r16 = calloc(1,sizeof(*sl_RLC_BearerConfig_r16->sl_MAC_LogicalChannelConfig_r16->sl_LogicalChannelGroup_r16)); 
  *sl_RLC_BearerConfig_r16->sl_MAC_LogicalChannelConfig_r16->sl_LogicalChannelGroup_r16 = 1; 
  sl_RLC_BearerConfig_r16->sl_MAC_LogicalChannelConfig_r16->sl_SchedulingRequestId_r16 = NULL;
  sl_RLC_BearerConfig_r16->sl_MAC_LogicalChannelConfig_r16->sl_LogicalChannelSR_DelayTimerApplied_r16 = NULL;
  ASN_SEQUENCE_ADD(&sl_preconfig->sl_RLC_BearerPreConfigList_r16->list,sl_RLC_BearerConfig_r16);
  //Measurement and reporting configuration
  sl_preconfig->sl_MeasPreConfig_r16 = NULL;

  //DFN timing offset used if GNSS is used as sync source.
  //1-.001ms, 2 - .002ms so on. value 0 if absent.
  sl_preconfig->sl_OffsetDFN_r16 = NULL;

  // t400 started upon txn of RRCreconfSidelink.
  sl_preconfig->t400_r16 = NULL;

  //Max num consecutive HARQ DTX before triggering SL RLF.
  sl_preconfig->sl_MaxNumConsecutiveDTX_r16 = NULL;

  //Priority of SSB transmission and reception. used in comparison to UL rxns/txns
  sl_preconfig->sl_SSB_PriorityNR_r16 = NULL;

  //Contains TDD ULDL confiuguration to be used by the sync source UE. 
  //Currently set to the default used in OAI 5G. Changes TBD..
  //For the UE with sync reference as another UE, TDD ULDL config is determined from SL-MIB
  sl_preconfig->sl_PreconfigGeneral_r16 = calloc(1, sizeof(NR_SL_PreconfigGeneral_r16_t));
  sl_preconfig->sl_PreconfigGeneral_r16->sl_TDD_Configuration_r16 = calloc(1, sizeof(NR_TDD_UL_DL_ConfigCommon_t));
  NR_TDD_UL_DL_ConfigCommon_t *tdd_uldl_cfg = sl_preconfig->sl_PreconfigGeneral_r16->sl_TDD_Configuration_r16;
  tdd_uldl_cfg->pattern1.ext1 = NULL;
  tdd_uldl_cfg->pattern2 = NULL;

  char aprefix[MAX_OPTNAME_SIZE*2 + 8];
  sprintf(aprefix, "%s.[%i]", SL_CONFIG_STRING_SL_PRECONFIGURATION,0);
  paramdef_t SLTDDCFG_PARAMS[] = SL_TDDCONFIGPARAMS_DESC(tdd_uldl_cfg);
  config_get(SLTDDCFG_PARAMS,sizeof(SLTDDCFG_PARAMS)/sizeof(paramdef_t),aprefix);

  NR_SL_FreqConfigCommon_r16_t *fcc =  sl_preconfig->sl_PreconfigFreqInfoList_r16->list.array[0];
  tdd_uldl_cfg->referenceSubcarrierSpacing =
                          fcc->sl_SCS_SpecificCarrierList_r16.list.array[0]->subcarrierSpacing;
  NR_SL_BWP_ConfigCommon_r16_t *sl_bwp = fcc->sl_BWP_List_r16->list.array[0];
  sl_bwp->sl_BWP_Generic_r16->sl_BWP_r16->subcarrierSpacing =
                          fcc->sl_SCS_SpecificCarrierList_r16.list.array[0]->subcarrierSpacing;

  // Configurations used for UE autonomous resource selection
  sl_preconfig->sl_UE_SelectedPreConfig_r16 = NULL;

  // indicates if CSI reporting supported in SL unicast.
  sl_preconfig->sl_CSI_Acquisition_r16 = calloc(1, sizeof(*sl_preconfig->sl_CSI_Acquisition_r16));

  // ROHC profiles for NR SL
  sl_preconfig->sl_RoHC_Profiles_r16 = NULL;

  // MaxCID value for PDCP as specified in 38.323
  sl_preconfig->sl_MaxCID_r16 = NULL;


  return sl_preconfiguration;
}

int configure_NR_SL_Preconfig(uint8_t id,int sync_source)
{

  NR_SL_PreconfigurationNR_r16_t *sl_preconfig = NULL;
  int num_txpools = 0, num_rxpools = 0;

  if (sync_source) {
    //SL-Preconfiguration with 1 txpool, 0 rxpool if UE is a syncsource
    num_txpools = 1;
    sl_preconfig = prepare_NR_SL_PRECONFIGURATION(num_txpools,num_rxpools,sync_source);

    //if (LOG_DEBUGFLAG(DEBUG_ASN1)) {
      xer_fprint(stdout, &asn_DEF_NR_SL_PreconfigurationNR_r16, sl_preconfig);
    //}
  } else {
    //SL-Preconfiguration with 0 txpool, 1 rxpool if UE is not a syncsource
    num_rxpools = 1;
    sl_preconfig = prepare_NR_SL_PRECONFIGURATION(num_txpools,num_rxpools,sync_source);

    if (LOG_DEBUGFLAG(DEBUG_ASN1)) {
      xer_fprint(stdout, &asn_DEF_NR_SL_PreconfigurationNR_r16, sl_preconfig);
    }
  }

  NR_UE_RRC_INST_t *rrc = &NR_UE_rrc_inst[id];
  rrc->sl_preconfig = sl_preconfig;

  return 0;
}

/*
* This functions configures SIdelink operation in the UE.
* RRC configures MAC with sidelink parameters
* In case UE is a sync source/Master UE - then sends transmit SLSS REQ
*/
void nr_UE_configure_Sidelink(uint8_t id, uint8_t is_sync_source, ueinfo_t *ueinfo) {

  NR_UE_RRC_INST_t *rrc = &NR_UE_rrc_inst[id];

  AssertFatal(rrc, "Check if rrc instance was created.");

  NR_SL_PreconfigurationNR_r16_t *sl_preconfig = rrc->sl_preconfig;
  AssertFatal(sl_preconfig, "Check if SL-preconfig was created.");

  uint8_t sync_source = SL_SYNC_SOURCE_NONE;

  if (is_sync_source) {
    sync_source = (GNSS_SUPPORT) ? SL_SYNC_SOURCE_GNSS
                                 : SL_SYNC_SOURCE_LOCAL_TIMING;
  }

  LOG_D(NR_RRC, "SL L2 SRCid %x, SL ipv4 addr X.X.%d.%d\n", ueinfo->srcid, ueinfo->thirdOctet, ueinfo->fourthOctet);
  nas_config(1 + ueinfo->srcid, ueinfo->thirdOctet, ueinfo->fourthOctet, "oai_sl_tun");
  nr_rrc_mac_config_req_sl_preconfig(id, sl_preconfig, sync_source);


  // SL RadioBearers
  for (int i=0; i<sl_preconfig->sidelinkPreconfigNR_r16.sl_RadioBearerPreConfigList_r16->list.count; i++) {
    add_drb_sl(ueinfo->srcid, (NR_SL_RadioBearerConfig_r16_t *)sl_preconfig->sidelinkPreconfigNR_r16.sl_RadioBearerPreConfigList_r16->list.array[i], 0, 0, NULL, NULL);
  }
  // configure RLC
  for (int i=0; i<sl_preconfig->sidelinkPreconfigNR_r16.sl_RLC_BearerPreConfigList_r16->list.count; i++) {
    nr_rlc_add_drb_sl(ueinfo->srcid, 1, (NR_SL_RLC_BearerConfig_r16_t *)sl_preconfig->sidelinkPreconfigNR_r16.sl_RLC_BearerPreConfigList_r16->list.array[i]);
  }
    
  //TBD.. These should be chosen by RRC according to 3GPP 38.331 RRC specification.
  //Currently hardcoding the values to these
  uint16_t slss_id = 671, ssb_ta_index = 1;
  //12 bits -sl-TDD-config will be filled by MAC
  //Incoverage 1bit is FALSE
  //DFN, sfn will be filled by PHY
  uint8_t sl_mib_payload[4] = {0,0,0,0};

  NR_SL_SSB_TimeAllocation_r16_t *ssb_ta = NULL;
  NR_SL_FreqConfigCommon_r16_t *fcfg = NULL;
  NR_SL_SyncConfig_r16_t *synccfg = NULL;
  if (rrc->sl_preconfig->sidelinkPreconfigNR_r16.sl_PreconfigFreqInfoList_r16)
    fcfg = rrc->sl_preconfig->sidelinkPreconfigNR_r16.sl_PreconfigFreqInfoList_r16->list.array[0];
  AssertFatal(fcfg, "Fcfg cannot be NULL\n");
  if (fcfg->sl_SyncConfigList_r16)
    synccfg = fcfg->sl_SyncConfigList_r16->list.array[0];
  AssertFatal(synccfg, "Synccfg cannot be NULL\n");

  if (ssb_ta_index == 1)
    ssb_ta = synccfg->sl_SSB_TimeAllocation1_r16;
  else if (ssb_ta_index == 2)
    ssb_ta = synccfg->sl_SSB_TimeAllocation2_r16;
  else if (ssb_ta_index == 3)
    ssb_ta = synccfg->sl_SSB_TimeAllocation3_r16;
  else DevAssert(0);

  AssertFatal(ssb_ta, "SSB_timeallocation cannot be NULL\n");

  if (sync_source == SL_SYNC_SOURCE_LOCAL_TIMING || sync_source == SL_SYNC_SOURCE_GNSS)
    nr_rrc_mac_transmit_slss_req(id, sl_mib_payload, slss_id, ssb_ta);

}


/*decode SL-BCH (SL-MIB) message*/
static int8_t nr_sl_rrc_ue_decode_SL_MIB(const module_id_t module_id,
                                          const uint8_t gNB_index,
                                          uint8_t *const bufferP,
                                          const uint8_t buffer_len)
{
  NR_MasterInformationBlockSidelink_t *sl_mib = NULL;

  asn_dec_rval_t dec_rval = uper_decode_complete(NULL, &asn_DEF_NR_MasterInformationBlockSidelink,
                                                 (void **)&sl_mib,
                                                 (const void *)bufferP, buffer_len);

  uint16_t val_fn = sl_mib->directFrameNumber_r16.buf[0];
  val_fn = (val_fn << 2) + (sl_mib->directFrameNumber_r16.buf[1] >> sl_mib->directFrameNumber_r16.bits_unused);

  uint8_t val_slot = sl_mib->slotIndex_r16.buf[0];

  LOG_D(NR_RRC, "SL-MIB Contents - DFN:%d\n" , val_fn);
  LOG_D(NR_RRC, "SL-MIB Contents - SLOT:%d\n" , val_slot >> 1);
  LOG_D(NR_RRC, "SL-MIB Contents - Incoverage:%d\n", sl_mib->inCoverage_r16);
  LOG_D(NR_RRC, "SL-MIB Contents - sl-TDD-Config:%x\n" , *((uint16_t *)(sl_mib->sl_TDD_Config_r16.buf)));

  int ret = 1;
  if ((dec_rval.code != RC_OK) || (dec_rval.consumed == 0)) {
    LOG_E(NR_RRC, "SL-MIB decode error\n");
    ret = -1;
  } else  {
    ret = 0;
    if (NR_UE_rrc_inst[module_id].sl_mib == NULL) {
      LOG_I(NR_RRC, "Sidelink RRC first MIB reception\n");
    } else {
      ASN_STRUCT_FREE(asn_DEF_NR_MasterInformationBlockSidelink, NR_UE_rrc_inst[module_id].sl_mib);
    }
    NR_UE_rrc_inst[module_id].sl_mib = sl_mib;
  }

  return ret;
}



void nr_mac_rrc_sl_mib_ind(const module_id_t module_id,
                              const int CC_id,
                              const uint8_t gNB_index,
                              const frame_t frame,
                              const int slot,
                              const channel_t channel,
                              uint8_t* pduP,
                              const sdu_size_t pdu_len,
                              const uint16_t rx_slss_id)
{

  nr_sl_rrc_ue_decode_SL_MIB(module_id, gNB_index, (uint8_t*)pduP, pdu_len);

  DevAssert(NR_UE_rrc_inst[module_id].sl_preconfig);

  NR_SL_FreqConfigCommon_r16_t *fcfg = NULL;
  if (NR_UE_rrc_inst[module_id].sl_preconfig->sidelinkPreconfigNR_r16.sl_PreconfigFreqInfoList_r16)
    fcfg = NR_UE_rrc_inst[module_id].sl_preconfig->sidelinkPreconfigNR_r16.sl_PreconfigFreqInfoList_r16->list.array[0];
  DevAssert(fcfg);

  NR_SL_SSB_TimeAllocation_r16_t *sl_SSB_TimeAllocation = NULL;

  //Current implementation only supports one SSB Timeallocation
  //Extend RRC to use multiple SSB Time allocations TBD....
  if (fcfg->sl_SyncConfigList_r16)
    sl_SSB_TimeAllocation = fcfg->sl_SyncConfigList_r16->list.array[0]->sl_SSB_TimeAllocation1_r16;
  DevAssert(sl_SSB_TimeAllocation);

  nr_rrc_mac_config_req_sl_mib(module_id,
                               sl_SSB_TimeAllocation,
                               rx_slss_id,
                               pduP);

  return;
}


void free_sl_rrc(uint8_t id) {

  NR_UE_RRC_INST_t *rrc = &NR_UE_rrc_inst[id];

  if (rrc->sl_preconfig) {
    ASN_STRUCT_FREE(asn_DEF_NR_SL_PreconfigurationNR_r16, rrc->sl_preconfig);
  }
  if (rrc->sl_mib) {
    ASN_STRUCT_FREE(asn_DEF_NR_MasterInformationBlockSidelink, rrc->sl_mib);
  }
}
