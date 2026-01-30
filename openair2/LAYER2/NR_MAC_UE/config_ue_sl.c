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

#include "openair2/LAYER2/NR_MAC_UE/mac_defs.h"
#include "openair2/LAYER2/NR_MAC_UE/nr_ue_sci.h"
#include "NR_SidelinkPreconfigNR-r16.h"
#include "mac_proto.h"
#include "common/config/config_paramdesc.h"
#include <executables/nr-uesoftmodem.h>

#define SL_CONFIG_STRING_SL_PRECONFIGURATION                        "SIDELINK_PRECONFIGURATION"

/* Sidelink CSI-RS configuration parameters for MAC*/
#define SL_CONFIG_STRING_SL_CSI_RS_LIST                             "sl_csi_rs"
#define SL_CONFIG_STRING_SL_CSI_RS_SYMB_L0                          "symb_l0"
#define SL_CONFIG_STRING_SL_CSI_RS_CSI_TYPE                         "csi_Type"
#define SL_CONFIG_STRING_SL_CSI_RS_SLOT_OFFSET                      "slot_Offset"
#define SL_CONFIG_STRING_SL_CSI_RS_SLOT_PERIODICITY                 "slot_Periodicity"
#define SL_CONFIG_STRING_SL_CSI_RS_POWER_CONTROL_OFFSET             "sl_powerControlOffset"
#define SL_CONFIG_STRING_SL_CSI_RS_POWER_CONTROL_OFFSET_SS          "sl_powerControlOffsetSS"
#define SL_CONFIG_STRING_SL_CSI_RS_SL_CSI_ACQUISITION               "sl_CSI_Acquisition"
#define SL_CONFIG_STRING_SL_CSI_RS_SL_LATENCYBOUNDCSI_REPORT        "sl_LatencyBoundCSI_Report"

#define SL_CONFIG_STRING_SL_ALLOWED_RESOURCE_SELECTION_CONFIG       "sl_AllowedResourceSelectionConfig"

#define SL_CSI_RS_DESC(sl_csi_info) { \
{SL_CONFIG_STRING_SL_CSI_RS_SYMB_L0,NULL,0,.u8ptr=&sl_csi_info->symb_l0,.defuintval=1,TYPE_UINT8,0}, \
{SL_CONFIG_STRING_SL_CSI_RS_CSI_TYPE,NULL,0,.u8ptr=&sl_csi_info->csi_type,.defuintval=1,TYPE_UINT8,0}, \
{SL_CONFIG_STRING_SL_CSI_RS_POWER_CONTROL_OFFSET,NULL,0,.u8ptr=&sl_csi_info->power_control_offset,.defuintval=1,TYPE_UINT8,0}, \
{SL_CONFIG_STRING_SL_CSI_RS_POWER_CONTROL_OFFSET_SS,NULL,0,.u8ptr=&sl_csi_info->power_control_offset_ss,.defuintval=1,TYPE_UINT8,0}, \
{SL_CONFIG_STRING_SL_CSI_RS_SLOT_OFFSET,NULL,0,.u8ptr=&sl_csi_info->slot_offset,.defuintval=1,TYPE_UINT8,0}, \
{SL_CONFIG_STRING_SL_CSI_RS_SLOT_PERIODICITY,NULL,0,.u8ptr=&sl_csi_info->slot_periodicity,.defuintval=1,TYPE_UINT8,0}, \
{SL_CONFIG_STRING_SL_CSI_RS_SL_CSI_ACQUISITION,NULL,0,.u8ptr=&sl_csi_info->sl_csi_acquisition,.defuintval=1,TYPE_UINT8,0}, \
{SL_CONFIG_STRING_SL_CSI_RS_SL_LATENCYBOUNDCSI_REPORT,NULL,0,.u8ptr=&sl_csi_info->sl_latencyboundcsi_report,.defuintval=8,TYPE_UINT8,0}}

/*Sidelink HARQ configuration parameters */
#define SL_CONFIG_STRING_SL_CONFIGUREDGRANT_LIST               "sl_ConfiguredGrantConfig"
#define SL_CONFIG_STRING_SL_CONFIGUREDGRANT_NROFHARQ_PROCESSES "sl_NrOfHARQ_Processes"
#define SL_CONFIG_STRING_SL_CONFIGUREDGRANT_HARQ_PROCID_OFFSET "sl_HARQ_ProcID_offset"
#define SL_CONFIG_STRING_SL_CONFIGUREDGRANT_HARQ_PERIODIC_RRI "sl_periodic_rsc_rsr_interval"
#define SL_CONFIGUREDGRANT_DESC(sl_harq_info) { \
{SL_CONFIG_STRING_SL_CONFIGUREDGRANT_NROFHARQ_PROCESSES, NULL, 0, .u16ptr=&sl_harq_info->sl_Num_HARQ_Processes, .defuintval=0, TYPE_UINT16, 0}, \
{SL_CONFIG_STRING_SL_CONFIGUREDGRANT_HARQ_PROCID_OFFSET, NULL, 0, .u16ptr=&sl_harq_info->sl_HARQ_ProcID_offset, .defuintval=0, TYPE_UINT16, 0}, \
{SL_CONFIG_STRING_SL_CONFIGUREDGRANT_HARQ_PERIODIC_RRI, NULL, 0, .u16ptr=&sl_harq_info->sl_Periodic_RRI, .defuintval=0, TYPE_UINT16, 0}}

#define SL_CONFIG_RESOURCE_SELECTION(resource_selection_cfg) { \
{SL_CONFIG_STRING_SL_ALLOWED_RESOURCE_SELECTION_CONFIG, NULL, 0, .u16ptr=resource_selection_cfg, .defuintval=3, TYPE_UINT16, 0}}

typedef struct sl_csi_info {
  uint8_t symb_l0;
  uint8_t csi_type;
  uint8_t slot_offset;
  uint8_t slot_periodicity;
  uint8_t power_control_offset;
  uint8_t power_control_offset_ss;
  uint8_t sl_csi_acquisition;
  uint8_t sl_latencyboundcsi_report;
} sl_csi_info_t;

typedef struct sl_harq_info {
  uint16_t sl_Num_HARQ_Processes;
  uint16_t sl_HARQ_ProcID_offset;
  uint16_t sl_Periodic_RRI;
} sl_harq_info_t;

void sl_ue_mac_free(uint8_t module_id)
{

  NR_UE_MAC_INST_t *mac = get_mac_inst(module_id);

  sl_nr_phy_config_request_t *sl_config =
                    &mac->SL_MAC_PARAMS->sl_phy_config.sl_config_req;

  uint8_t syncsource = sl_config->sl_sync_source.sync_source;

  //Allocated by MAC only in case of SYNC_REF_UE
  //else it is freed as part of RRC pre-config structure
  if (syncsource == SL_SYNC_SOURCE_SYNC_REF_UE &&
      mac->SL_MAC_PARAMS->sl_TDD_config) {
    ASN_STRUCT_FREE (asn_DEF_NR_TDD_UL_DL_Pattern, mac->SL_MAC_PARAMS->sl_TDD_config);
    mac->SL_MAC_PARAMS->sl_TDD_config = NULL;
  }

  fapi_nr_max_tdd_periodicity_t *tdd_list =
                            sl_config->tdd_table.max_tdd_periodicity_list;

  // @todo: maybe this should be done by phy
  if (tdd_list) {
    int mu = sl_config->sl_bwp_config.sl_scs;
    int nb_slots_to_set = TDD_CONFIG_NB_FRAMES*(1<<mu)*NR_NUMBER_OF_SUBFRAMES_PER_FRAME;
    for (int i=0; i<nb_slots_to_set; i++) {
      free_and_zero(tdd_list[i].max_num_of_symbol_per_slot_list);
    }
    free_and_zero(sl_config->tdd_table.max_tdd_periodicity_list);
  }

  for (int i=0;i<SL_NR_MAC_NUM_RX_RESOURCE_POOLS;i++) {
    free_and_zero(mac->SL_MAC_PARAMS->sl_RxPool[i]);
  }
  for (int i=0;i<SL_NR_MAC_NUM_TX_RESOURCE_POOLS;i++) {
    free_and_zero(mac->SL_MAC_PARAMS->sl_TxPool[i]);
  }

  free_and_zero(mac->SL_MAC_PARAMS);
}


//Prepares the TDD config to be passed to PHY
static int sl_set_tdd_config_nr_ue(sl_nr_phy_config_request_t *cfg,
                                  int mu,
                                  long *pNumDownlinkSlots, long *pNumDownlinkSymbols,
                                  int nrofUplinkSlots,   int nrofUplinkSymbols)
{


  int slot_number = 0;
  int nb_periods_per_frame = get_nb_periods_per_frame(cfg->tdd_table.tdd_period);
  int nb_slots_to_set = TDD_CONFIG_NB_FRAMES*(1<<mu)*NR_NUMBER_OF_SUBFRAMES_PER_FRAME;

  int nb_slots_per_period = ((1<<mu) * NR_NUMBER_OF_SUBFRAMES_PER_FRAME)/nb_periods_per_frame;
  cfg->tdd_table.tdd_period_in_slots = nb_slots_per_period;

  if ((*pNumDownlinkSlots == 0) && (*pNumDownlinkSymbols == 0)) {
    *pNumDownlinkSymbols = (nrofUplinkSymbols) ? 14 - nrofUplinkSymbols : 0;
    *pNumDownlinkSlots = nb_slots_per_period - nrofUplinkSlots;
    if (*pNumDownlinkSymbols) *pNumDownlinkSlots -= 1;
  }
  int nrofDownlinkSlots = *pNumDownlinkSlots, nrofDownlinkSymbols = *pNumDownlinkSymbols;

  LOG_D(NR_MAC,"Set Phy Sidelink TDD Config: scs:%d,dl:%d-%d, ul:%d-%d, nb_periods_per_frame:%d, nb_slots_per_period:%d\n",
                              mu, nrofDownlinkSlots, nrofDownlinkSymbols, nrofUplinkSlots, nrofUplinkSymbols, nb_periods_per_frame, nb_slots_per_period);

  if ( (nrofDownlinkSymbols + nrofUplinkSymbols) == 0 )
    AssertFatal(nb_slots_per_period == (nrofDownlinkSlots + nrofUplinkSlots),
                "set_tdd_configuration_nr: given period is inconsistent with current tdd configuration, nrofDownlinkSlots %d, nrofUplinkSlots %d, nb_slots_per_period %d \n",
                nrofDownlinkSlots,nrofUplinkSlots,nb_slots_per_period);
  else {
    AssertFatal(nrofDownlinkSymbols + nrofUplinkSymbols <= 14,"illegal symbol configuration DL %d, UL %d\n",nrofDownlinkSymbols,nrofUplinkSymbols);
    AssertFatal(nb_slots_per_period == (nrofDownlinkSlots + nrofUplinkSlots + 1),
                "set_tdd_configuration_nr: given period is inconsistent with current tdd configuration, nrofDownlinkSlots %d, nrofUplinkSlots %d, nrofMixed slots 1, nb_slots_per_period %d \n",
                nrofDownlinkSlots,nrofUplinkSlots,nb_slots_per_period);
  }

  cfg->tdd_table.max_tdd_periodicity_list = (fapi_nr_max_tdd_periodicity_t *) malloc(nb_slots_to_set*sizeof(fapi_nr_max_tdd_periodicity_t));

  for(int memory_alloc =0 ; memory_alloc<nb_slots_to_set; memory_alloc++)
    cfg->tdd_table.max_tdd_periodicity_list[memory_alloc].max_num_of_symbol_per_slot_list = (fapi_nr_max_num_of_symbol_per_slot_t *) malloc(NR_NUMBER_OF_SYMBOLS_PER_SLOT*sizeof(
          fapi_nr_max_num_of_symbol_per_slot_t));

  while(slot_number != nb_slots_to_set) {
    if(nrofDownlinkSlots != 0) {
      for (int number_of_symbol = 0; number_of_symbol < nrofDownlinkSlots*NR_NUMBER_OF_SYMBOLS_PER_SLOT; number_of_symbol++) {
        cfg->tdd_table.max_tdd_periodicity_list[slot_number].max_num_of_symbol_per_slot_list[number_of_symbol%NR_NUMBER_OF_SYMBOLS_PER_SLOT].slot_config= 0;

        if((number_of_symbol+1)%NR_NUMBER_OF_SYMBOLS_PER_SLOT == 0)
          slot_number++;
      }
    }

    if (nrofDownlinkSymbols != 0 || nrofUplinkSymbols != 0) {
      for(int number_of_symbol =0; number_of_symbol < nrofDownlinkSymbols; number_of_symbol++) {
        cfg->tdd_table.max_tdd_periodicity_list[slot_number].max_num_of_symbol_per_slot_list[number_of_symbol].slot_config= 0;
      }

      for(int number_of_symbol = nrofDownlinkSymbols; number_of_symbol < NR_NUMBER_OF_SYMBOLS_PER_SLOT-nrofUplinkSymbols; number_of_symbol++) {
        cfg->tdd_table.max_tdd_periodicity_list[slot_number].max_num_of_symbol_per_slot_list[number_of_symbol].slot_config= 2;
      }

      for(int number_of_symbol = NR_NUMBER_OF_SYMBOLS_PER_SLOT-nrofUplinkSymbols; number_of_symbol < NR_NUMBER_OF_SYMBOLS_PER_SLOT; number_of_symbol++) {
        cfg->tdd_table.max_tdd_periodicity_list[slot_number].max_num_of_symbol_per_slot_list[number_of_symbol].slot_config= 1;
      }

      slot_number++;
    }

    if(nrofUplinkSlots != 0) {
      for (int number_of_symbol = 0; number_of_symbol < nrofUplinkSlots*NR_NUMBER_OF_SYMBOLS_PER_SLOT; number_of_symbol++) {
        cfg->tdd_table.max_tdd_periodicity_list[slot_number].max_num_of_symbol_per_slot_list[number_of_symbol%NR_NUMBER_OF_SYMBOLS_PER_SLOT].slot_config= 1;

        if((number_of_symbol+1)%NR_NUMBER_OF_SYMBOLS_PER_SLOT == 0)
          slot_number++;
      }
    }
  }

  return (0);
}

//Prepares the PHY config to be sent to PHY. Prepares from the Valus from MAC context.
static void  sl_prepare_phy_config(int module_id,
                                   sl_nr_phy_config_request_t *phycfg,
                                   NR_SL_FreqConfigCommon_r16_t *freqcfg,
                                   uint8_t sync_source,
                                   uint32_t sl_OffsetDFN,
                                   NR_TDD_UL_DL_ConfigCommon_t *sl_TDD_config)
{

  phycfg->sl_sync_source.sync_source = sync_source;
  LOG_I(NR_MAC, "Sidelink CFG: sync source:%d\n", phycfg->sl_sync_source.sync_source);

  uint32_t pointA_ARFCN = freqcfg->sl_AbsoluteFrequencyPointA_r16;
  AssertFatal(pointA_ARFCN, "sl_AbsoluteFrequencyPointA_r16 cannot be 0\n");

  int sl_band = 0;
  if (pointA_ARFCN >= 790334 && pointA_ARFCN <= 795000)
    sl_band = 47;
  else if (pointA_ARFCN >= 514000 && pointA_ARFCN <= 524000)
    sl_band = 38;

  AssertFatal(sl_band, "not valid band for Sidelink operation\n");

  uint32_t SSB_ARFCN = (freqcfg->sl_AbsoluteFrequencySSB_r16)
                            ? *freqcfg->sl_AbsoluteFrequencySSB_r16 : 0;

  AssertFatal(SSB_ARFCN, "sl_AbsoluteFrequencySSB cannot be 0\n");

  LOG_I(NR_MAC, "SIDELINK CONFIGs: AbsFreqSSB:%d, AbsFreqPointA:%d, SL band:%d\n",
                                                        SSB_ARFCN,pointA_ARFCN, sl_band);

#define SL_VALUE_FREQSHIFT_7P5KHZ_DISABLED 0
  phycfg->sl_carrier_config.sl_frequency_shift_7p5khz = SL_VALUE_FREQSHIFT_7P5KHZ_DISABLED;
  phycfg->sl_carrier_config.sl_value_N = freqcfg->valueN_r16;
  phycfg->sl_carrier_config.sl_num_tx_ant = 1;
  phycfg->sl_carrier_config.sl_num_rx_ant = 1;

  NR_SCS_SpecificCarrier_t *carriercfg =
            freqcfg->sl_SCS_SpecificCarrierList_r16.list.array[0];

  AssertFatal(carriercfg, "SCS_SpecificCarrier cannot be NULL");

  int bw_index = get_supported_band_index(carriercfg->subcarrierSpacing,
                                          sl_band,
                                          carriercfg->carrierBandwidth);
  phycfg->sl_carrier_config.sl_bandwidth = get_supported_bw_mhz(FR1, bw_index);

  phycfg->sl_carrier_config.sl_frequency =
              from_nrarfcn(sl_band,carriercfg->subcarrierSpacing,pointA_ARFCN)/1000; // freq in kHz

  phycfg->sl_carrier_config.sl_grid_size = carriercfg->carrierBandwidth;
  //For sidelink offset to carrier is 0. hence not used
  //phycfg->sl_carrier_config.sl_k0 = carriercfg->offsetToCarrier;

  NR_SL_BWP_Generic_r16_t *bwp_generic = NULL;
  if (freqcfg->sl_BWP_List_r16 &&
      freqcfg->sl_BWP_List_r16->list.array[0] &&
      freqcfg->sl_BWP_List_r16->list.array[0]->sl_BWP_Generic_r16)
    bwp_generic = freqcfg->sl_BWP_List_r16->list.array[0]->sl_BWP_Generic_r16;

  AssertFatal(bwp_generic, "SL-BWP Generic cannot be NULL");

  NR_BWP_t *sl_bwp = bwp_generic->sl_BWP_r16;
  AssertFatal(sl_bwp, "SL-BWP cannot be NULL");

  int  locbw = bwp_generic->sl_BWP_r16->locationAndBandwidth;
  phycfg->sl_bwp_config.sl_bwp_size = NRRIV2BW(locbw, MAX_BWP_SIZE);
  phycfg->sl_bwp_config.sl_bwp_start = NRRIV2PRBOFFSET(locbw, MAX_BWP_SIZE);
  phycfg->sl_bwp_config.sl_scs = sl_bwp->subcarrierSpacing;


  int scs_scaling = 1<<(phycfg->sl_bwp_config.sl_scs);

  if (pointA_ARFCN < 600000)
    scs_scaling = scs_scaling*3;
  if (pointA_ARFCN > 2016666)
    scs_scaling = scs_scaling>>2;
  //SSB arfcn points to middle RE of PSBCH 11 RBs
  uint32_t diff = (SSB_ARFCN - 66*scs_scaling) - pointA_ARFCN;
  //the RE offset from pointA where SSB starts
  phycfg->sl_bwp_config.sl_ssb_offset_point_a = diff/scs_scaling;

#ifdef SL_DEBUG
  printf("diff:%d, scaling:%d, pointa:%d, ssb:%d\n", diff, scs_scaling, pointA_ARFCN, SSB_ARFCN);
#endif

  phycfg->sl_bwp_config.sl_dc_location = (bwp_generic->sl_TxDirectCurrentLocation_r16) ?
                                            *bwp_generic->sl_TxDirectCurrentLocation_r16 : 0;

  const uint8_t values[] = {7,8,9,10,11,12,13,14};
  phycfg->sl_bwp_config.sl_num_symbols = (bwp_generic->sl_LengthSymbols_r16) ?
                                            values[*bwp_generic->sl_LengthSymbols_r16] : 0;

  phycfg->sl_bwp_config.sl_start_symbol = (bwp_generic->sl_StartSymbol_r16) ?
                                                  *bwp_generic->sl_StartSymbol_r16 : 0;

  //0-EXTENDED, 1-NORMAL CP
  phycfg->sl_bwp_config.sl_cyclic_prefix = (sl_bwp->cyclicPrefix) ? EXTENDED : NORMAL;

  AssertFatal(phycfg->sl_bwp_config.sl_cyclic_prefix == NORMAL, "Only NORMAL-CP Supported. Ext CP not yet supported\n");


  AssertFatal(phycfg->sl_bwp_config.sl_start_symbol >= 0 && phycfg->sl_bwp_config.sl_start_symbol <=7,
                                                                "Sidelink Start symbol should be in range 0-7\n");

  AssertFatal(phycfg->sl_bwp_config.sl_num_symbols >= 7 && phycfg->sl_bwp_config.sl_num_symbols <=14,
                                                                "Num Sidelink symbols should be in range 7-14\n");

  AssertFatal((phycfg->sl_bwp_config.sl_start_symbol + phycfg->sl_bwp_config.sl_num_symbols) <= 14,
                                                          "Incorrect configuration of Start and num SL symbols\n");

  //Configure PHY with TDD config only if the sync source is known.
  if (sync_source == SL_SYNC_SOURCE_LOCAL_TIMING ||
      sync_source == SL_SYNC_SOURCE_GNSS) {

    phycfg->config_mask = 0xF;//Total config is sent
    phycfg->sl_sync_source.gnss_dfn_offset = sl_OffsetDFN;

    // TDD Table Configuration
    if (sl_TDD_config->pattern1.ext1 == NULL)
      phycfg->tdd_table.tdd_period = sl_TDD_config->pattern1.dl_UL_TransmissionPeriodicity;
    else {
      if (sl_TDD_config->pattern1.ext1->dl_UL_TransmissionPeriodicity_v1530 != NULL)
        phycfg->tdd_table.tdd_period += (1 + *sl_TDD_config->pattern1.ext1->dl_UL_TransmissionPeriodicity_v1530);
    }

    int return_tdd = sl_set_tdd_config_nr_ue(phycfg,
                                             sl_TDD_config->referenceSubcarrierSpacing,
                                             &sl_TDD_config->pattern1.nrofDownlinkSlots,
                                             &sl_TDD_config->pattern1.nrofDownlinkSymbols,
                                             sl_TDD_config->pattern1.nrofUplinkSlots,
                                             sl_TDD_config->pattern1.nrofUplinkSymbols);

    if (return_tdd !=0)
      LOG_E(PHY,"TDD configuration can not be done\n");
    else {
      LOG_I(NR_MAC, "SIDELINK CONFIGs: tdd config period:%d, mu:%ld, DLslots:%ld,ULslots:%ld Mixedslotsym DL:UL %ld:%ld\n",
                          phycfg->tdd_table.tdd_period, sl_TDD_config->referenceSubcarrierSpacing,
                          sl_TDD_config->pattern1.nrofDownlinkSlots, sl_TDD_config->pattern1.nrofUplinkSlots,
                          sl_TDD_config->pattern1.nrofDownlinkSymbols,sl_TDD_config->pattern1.nrofUplinkSymbols);
    }
  } else if (sync_source == SL_SYNC_SOURCE_NONE) {
    //Only Carrier config, BWP config sent
    phycfg->config_mask = 0x9;//partial config is sent
  }

//#ifdef SL_DEBUG
  char str[5][20] = {"NONE","GNBENB","GNSS","SYNC_REF_UE","LOCAL_TIMING"};
  LOG_I(NR_MAC, "UE[%d] Function %s - Phy config preparation:\n",module_id, __FUNCTION__);
  LOG_I(NR_MAC, "UE[%d] Sync source params: sync_source :%d-%s, gnss_dfn_offset:%d, rx_slss_id:%d\n",
                                                        module_id,phycfg->sl_sync_source.sync_source,
                                                        str[phycfg->sl_sync_source.sync_source],
                                                        phycfg->sl_sync_source.gnss_dfn_offset,
                                                        phycfg->sl_sync_source.rx_slss_id);
  LOG_I(NR_MAC, "UE[%d] Carrier CFG Params: freq:%ld, bw:%d, gridsize:%d, rxant:%d, txant:%d, valueN:%d\n",
                                                        module_id,phycfg->sl_carrier_config.sl_frequency,
                                                        phycfg->sl_carrier_config.sl_bandwidth,
                                                        phycfg->sl_carrier_config.sl_grid_size,
                                                        phycfg->sl_carrier_config.sl_num_rx_ant,
                                                        phycfg->sl_carrier_config.sl_num_tx_ant,
                                                        phycfg->sl_carrier_config.sl_value_N);
  LOG_I(NR_MAC, "UE[%d] SL-BWP Params: start:%d, size:%d, scs:%d, Ncp:%d, startsym:%d, numsym:%d,ssb_offset:%d,dcloc:%d\n",
                                                        module_id,phycfg->sl_bwp_config.sl_bwp_start,
                                                        phycfg->sl_bwp_config.sl_bwp_size,
                                                        phycfg->sl_bwp_config.sl_scs,
                                                        phycfg->sl_bwp_config.sl_cyclic_prefix,
                                                        phycfg->sl_bwp_config.sl_start_symbol,
                                                        phycfg->sl_bwp_config.sl_num_symbols,
                                                        phycfg->sl_bwp_config.sl_ssb_offset_point_a,
                                                        phycfg->sl_bwp_config.sl_dc_location);

//#endif

  return;
}

// RRC calls this API when RRC is configured with Sidelink PRE-configuration I.E
int nr_rrc_mac_config_req_sl_preconfig(module_id_t module_id,
                                       NR_SL_PreconfigurationNR_r16_t *sl_preconfiguration,
                                       uint8_t sync_source)
{

  LOG_I(NR_MAC,"[UE%d] SL RRC->MAC CONFIG RECEIVED. Syncsource:%d\n",
                                                module_id, sync_source);

  NR_UE_MAC_INST_t *mac = get_mac_inst(module_id);
  AssertFatal(sl_preconfiguration !=NULL,"SL-Preconfig Cannot be NULL");
  AssertFatal(mac, "mac should have an instance");

  sl_nr_ue_mac_params_t *sl_mac = mac->SL_MAC_PARAMS;

  NR_SidelinkPreconfigNR_r16_t *sl_preconfig = &sl_preconfiguration->sidelinkPreconfigNR_r16;

  //Only one entry supported in rel16.
  //Carrier freq config used for Sidelink

  NR_SL_FreqConfigCommon_r16_t *freqcfg = (sl_preconfig->sl_PreconfigFreqInfoList_r16)
                                              ? sl_preconfig->sl_PreconfigFreqInfoList_r16->list.array[0]
                                              : NULL;

  AssertFatal(freqcfg !=NULL,"SL fcfg Cannot be NULL");

  //MAx num of consecutive HARQ DTX before triggering RLF.
  const uint8_t MaxNumConsecutiveDTX[] = {1,2,3,4,6,8,16,32};
  sl_mac->sl_MaxNumConsecutiveDTX = (sl_preconfig->sl_MaxNumConsecutiveDTX_r16)
                                        ? MaxNumConsecutiveDTX[*sl_preconfig->sl_MaxNumConsecutiveDTX_r16]
                                        : 0;

  //priority of SL-SSB tx and rx
  sl_mac->sl_SSB_PriorityNR = (sl_preconfig->sl_SSB_PriorityNR_r16)
                                      ? *sl_preconfig->sl_SSB_PriorityNR_r16 : 0;

  //Used for DFN calculation in case Sync source = GNSS.
  uint32_t sl_OffsetDFN = (sl_preconfig->sl_OffsetDFN_r16)
                                ? *sl_preconfig->sl_OffsetDFN_r16 : 0;

  NR_SL_BWP_ConfigCommon_r16_t *bwp = NULL;
  if (freqcfg->sl_BWP_List_r16 &&
      freqcfg->sl_BWP_List_r16->list.array[0])
    bwp = freqcfg->sl_BWP_List_r16->list.array[0];
  mac->sl_bwp = bwp;

  AssertFatal(bwp!=NULL, "BWP config common cannot be NULL\n");
  if (bwp->sl_BWP_PoolConfigCommon_r16) {
    if (bwp->sl_BWP_PoolConfigCommon_r16->sl_RxPool_r16) {

      for (int i=0;i<bwp->sl_BWP_PoolConfigCommon_r16->sl_RxPool_r16->list.count;i++) {
        NR_SL_ResourcePool_r16_t *rxpool = bwp->sl_BWP_PoolConfigCommon_r16->sl_RxPool_r16->list.array[i];
        if (rxpool) {
          if (sl_mac->sl_RxPool[i] == NULL)
            sl_mac->sl_RxPool[i] = malloc16_clear(sizeof(SL_ResourcePool_params_t));
          mac->sl_rx_res_pool = rxpool;
          sl_mac->sl_RxPool[i]->respool = rxpool;
          uint16_t sci_1a_len = 0, num_subch = 0;
          sci_1a_len = sl_determine_sci_1a_len(&num_subch,
                                               sl_mac->sl_RxPool[i]->respool,
                                               &sl_mac->sl_RxPool[i]->sci_1a);
          sl_mac->sl_RxPool[i]->num_subch = num_subch;
          sl_mac->sl_RxPool[i]->sci_1a_len = sci_1a_len;

          LOG_I(NR_MAC,"Rxpool[%d] - num subchannels:%d, sci_1a_len:%d\n",i,
                                                  sl_mac->sl_RxPool[i]->num_subch,
                                                  sl_mac->sl_RxPool[i]->sci_1a_len);

        }
      }
    }

    if (bwp->sl_BWP_PoolConfigCommon_r16->sl_TxPoolSelectedNormal_r16) {

      for (int i=0;i<bwp->sl_BWP_PoolConfigCommon_r16->sl_TxPoolSelectedNormal_r16->list.count;i++) {

        NR_SL_ResourcePool_r16_t *txpool =
                bwp->sl_BWP_PoolConfigCommon_r16->sl_TxPoolSelectedNormal_r16->list.array[0]->sl_ResourcePool_r16;

        if (txpool) {
          mac->sl_tx_res_pool = txpool;
          if (sl_mac->sl_TxPool[i] == NULL)
            sl_mac->sl_TxPool[i] = malloc16_clear(sizeof(SL_ResourcePool_params_t));
          sl_mac->sl_TxPool[i]->respool = txpool;
          uint8_t tproc1_valaues[] = {3, 5, 9, 17};
          uint8_t mu = get_softmodem_params()->numerology;
          struct NR_SL_UE_SelectedConfigRP_r16 *sl_ue_selected_config = sl_mac->sl_TxPool[i]->respool->sl_UE_SelectedConfigRP_r16;
          uint16_t sensing_window_ms = (uint16_t)*sl_ue_selected_config->sl_SensingWindow_r16;
          uint16_t selection_window = (uint16_t)sl_ue_selected_config->sl_SelectionWindowList_r16->list.array[0]->sl_SelectionWindow_r16;
          sl_mac->sl_TxPool[i]->t2min = selection_window;
          sl_mac->sl_TxPool[i]->t0 = time_to_slots(mu, sensing_window_ms);
          sl_mac->sl_TxPool[i]->tproc0 = 1;
          sl_mac->sl_TxPool[i]->tproc1 = tproc1_valaues[mu];
          sl_mac->sl_TxPool[i]->t1 = 1;
          sl_mac->sl_TxPool[i]->t2 = 60; // According to 38214 sec. 8.1.4: T2min <= t2 <= PDB, where T2min  = {1,5,10,20}*2^μ, u = 1, PDB = 20ms = 40 slots
          sl_mac->mac_tx_params.rri = sl_ue_selected_config->sl_ResourceReservePeriodList_r16->list.array[0]->choice.sl_ResourceReservePeriod1_r16;
          sl_mac->mac_tx_params.resel_counter = get_random_reselection_counter(sl_mac->mac_tx_params.rri);
          mac->sl_thresh_rsrp = (-128 + (*sl_ue_selected_config->sl_Thres_RSRP_List_r16->list.array[0] - 1) * 2);
          LOG_D(NR_MAC, "sl_thresh_rsrp %d rri %i sl_ResourceReservePeriod1 %ld, i %d, sensing_window_ms %d, selection_window %d\n",
                mac->sl_thresh_rsrp, sl_mac->mac_tx_params.rri,
                sl_ue_selected_config->sl_ResourceReservePeriodList_r16->list.array[0]->choice.sl_ResourceReservePeriod1_r16,
                i, sensing_window_ms, selection_window);
          uint16_t sci_1a_len = 0, num_subch = 0;
          sci_1a_len = sl_determine_sci_1a_len(&num_subch,
                                               sl_mac->sl_TxPool[i]->respool,
                                               &sl_mac->sl_TxPool[i]->sci_1a);

          sl_mac->sl_TxPool[i]->num_subch = num_subch;
          sl_mac->sl_TxPool[i]->sci_1a_len = sci_1a_len;

          LOG_I(NR_MAC,"Txpool[%d] - num subchannels:%d, sci_1a_len:%d\n",i,
                                                      sl_mac->sl_TxPool[i]->num_subch,
                                                      sl_mac->sl_TxPool[i]->sci_1a_len);
        }
      }
    }
  }

  if (sync_source == SL_SYNC_SOURCE_GNSS ||
      sync_source == SL_SYNC_SOURCE_LOCAL_TIMING) {

    NR_TDD_UL_DL_ConfigCommon_t *tdd_uldl_config = NULL;
    if (sl_preconfig->sl_PreconfigGeneral_r16 &&
        sl_preconfig->sl_PreconfigGeneral_r16->sl_TDD_Configuration_r16)
      tdd_uldl_config = sl_preconfig->sl_PreconfigGeneral_r16->sl_TDD_Configuration_r16;

    AssertFatal((tdd_uldl_config!=NULL), "Sidelink MAC CFG: TDD Config cannot be NULL");
    AssertFatal((tdd_uldl_config->pattern2 == NULL), "Sidelink MAC CFG: pattern2 not yet supported");

    sl_mac->sl_TDD_config = sl_preconfig->sl_PreconfigGeneral_r16->sl_TDD_Configuration_r16;

    //Sync source is identified, timing needs to be adjusted.
    sl_mac->adjust_timing = 1;
  }

  //Do not copy TDD config yet as SYNC source is not yet found
  if (sync_source == SL_SYNC_SOURCE_NONE) {
    if (sl_mac->sl_TDD_config)
      ASN_STRUCT_FREE(asn_DEF_NR_TDD_UL_DL_ConfigCommon, sl_mac->sl_TDD_config);
    sl_mac->sl_TDD_config = NULL;
  }
  if (get_nrUE_params()->sync_ref) {
    int scs = get_softmodem_params()->numerology;
    const int nr_slots_frame = nr_slots_per_frame[scs];
    NR_TDD_UL_DL_Pattern_t *tdd = &sl_mac->sl_TDD_config->pattern1;
    const int n_ul_slots_period = tdd ? tdd->nrofUplinkSlots + (tdd->nrofUplinkSymbols > 0 ? 1 : 0) : nr_slots_frame;
    uint16_t num_subch = sl_get_num_subch(mac->sl_tx_res_pool);
    //Jin replace hardcode list, to support multiple UES.
    //mac->sl_info.list[0]->UE_sched_ctrl.sched_psfch = calloc(n_ul_slots_period * num_subch, sizeof(SL_sched_feedback_t));
    //mac->sl_info.list[0]->UE_sched_ctrl.sched_psfch->feedback_frame = -1;
    //mac->sl_info.list[0]->UE_sched_ctrl.sched_psfch->feedback_slot = -1;
    /******************   Jin replacement   ****************************** */
      SL_UE_iterator(mac->sl_info.list, UE) {
        const int n = sl_num_slsch_feedbacks(mac);
        UE->UE_sched_ctrl.sched_psfch = calloc(n, sizeof(SL_sched_feedback_t));
        for (int i = 0; i < n; i++) {
          UE->UE_sched_ctrl.sched_psfch = calloc(n_ul_slots_period * num_subch, sizeof(SL_sched_feedback_t));
          UE->UE_sched_ctrl.sched_psfch->feedback_frame = -1;
          UE->UE_sched_ctrl.sched_psfch->feedback_slot  = -1;
        }
      }
    /************************************************ */

 
    int nr_slots_period = nr_slots_frame;
    int nr_ulstart_slot = 0;
    if (tdd) {
      nr_ulstart_slot = get_first_ul_slot(tdd->nrofDownlinkSlots, tdd->nrofDownlinkSymbols, tdd->nrofUplinkSymbols);
      nr_slots_period /= get_nb_periods_per_frame(tdd->dl_UL_TransmissionPeriodicity);
    }

    for (int slot = 0; slot < nr_slots_frame; ++slot) {
      mac->ulsch_slot_bitmap[slot / 64] |= (uint64_t)((slot % nr_slots_period) >= nr_ulstart_slot) << (slot % 64);
      LOG_D(NR_MAC,
            "slot %d UL %d\n",
            slot,
            (mac->ulsch_slot_bitmap[slot / 64] & ((uint64_t)1 << (slot % 64))) != 0);
    }

    BIT_STRING_t *sl_tx_time_rsrc = mac->sl_tx_res_pool->ext1->sl_TimeResource_r16;
    int total_downlink_slots_in_bitmap = (((sl_tx_time_rsrc->size << 3) - sl_tx_time_rsrc->bits_unused) / n_ul_slots_period) * (nr_slots_period - n_ul_slots_period);
    int total_uplink_slots_in_bitmap = (((sl_tx_time_rsrc->size << 3) - sl_tx_time_rsrc->bits_unused) / n_ul_slots_period) * (n_ul_slots_period);
    AssertFatal(((sl_tx_time_rsrc->size << 3) - sl_tx_time_rsrc->bits_unused) == total_uplink_slots_in_bitmap, "The computation for total uplink slots is invalid. %ld != %d\n",
                ((sl_tx_time_rsrc->size << 3) - sl_tx_time_rsrc->bits_unused), total_uplink_slots_in_bitmap);
    int phy_sl_size = ((sl_tx_time_rsrc->size << 3) - sl_tx_time_rsrc->bits_unused) + total_downlink_slots_in_bitmap;
    AssertFatal(total_downlink_slots_in_bitmap + total_uplink_slots_in_bitmap == phy_sl_size, "The total number of uplink and downlink slots must equal the total bitmap size!");
    LOG_D(NR_MAC, "size of phy_sl_map  %d total_downlink_slots %d, sl_tx_time_rsrc.size %ld, n_ul_slots_period %d, (nr_slots_period - n_ul_slots_period) %d\n",
          phy_sl_size, total_downlink_slots_in_bitmap, ((sl_tx_time_rsrc->size << 3) - sl_tx_time_rsrc->bits_unused), n_ul_slots_period, (nr_slots_period - n_ul_slots_period));

    uint8_t pool_id = 0;
    size_t byte_capacity = (phy_sl_size + 7) / 8;

    SL_ResourcePool_params_t *sl_tx_rsrc_pool = sl_mac->sl_TxPool[pool_id];
    BIT_STRING_t *phy_sl_tx_bitmap = &sl_tx_rsrc_pool->phy_sl_bitmap;
    phy_sl_tx_bitmap->buf = (uint8_t*)malloc16_clear(byte_capacity);
    phy_sl_tx_bitmap->size = byte_capacity;
    phy_sl_tx_bitmap->bits_unused = ((phy_sl_tx_bitmap->size << 3) - phy_sl_size) % 8;
    uint16_t tx_phy_map_sz = get_physical_sl_pool(mac, sl_tx_time_rsrc, phy_sl_tx_bitmap);


    SL_ResourcePool_params_t *sl_rx_rsrc_pool = sl_mac->sl_RxPool[pool_id];
    BIT_STRING_t *phy_sl_rx_bitmap = &sl_rx_rsrc_pool->phy_sl_bitmap;
    phy_sl_rx_bitmap->buf = (uint8_t*)malloc16_clear(byte_capacity);
    phy_sl_rx_bitmap->size = byte_capacity;
    phy_sl_rx_bitmap->bits_unused = ((phy_sl_rx_bitmap->size << 3) - phy_sl_size) % 8;
    BIT_STRING_t *sl_rx_time_rsrc = mac->sl_rx_res_pool->ext1->sl_TimeResource_r16;
    uint16_t rx_phy_map_sz = get_physical_sl_pool(mac, sl_rx_time_rsrc, phy_sl_rx_bitmap);

    AssertFatal(tx_phy_map_sz == rx_phy_map_sz, "Transmit %d and receive %d phy_map_sz is different.\n",
                tx_phy_map_sz, rx_phy_map_sz);
    mac->sl_res_percentage = 0.5;
  }
  // Configuring CSI-RS parameters locally at MAC.
  nr_sl_params_read_conf(module_id);

  nr_sl_phy_config_t *sl_phy_cfg = &sl_mac->sl_phy_config;
  sl_phy_cfg->Mod_id = module_id;
  sl_phy_cfg->CC_id = 0;

  sl_prepare_phy_config(module_id, &sl_phy_cfg->sl_config_req,
                        freqcfg, sync_source, sl_OffsetDFN, sl_mac->sl_TDD_config);

  sl_mac->mac_tx_params.packet_delay_budget_ms = 30;
  return 0;
}


//Copies the values of SSB time allocation from ASN format to MAC context
static void sl_mac_config_ssb_time_alloc(uint8_t module_id,
                                         NR_SL_SSB_TimeAllocation_r16_t *sl_SSB_TimeAllocation_r16,
                                         sl_ssb_timealloc_t *ssb_time_alloc)
{

  const uint8_t values[] = {1,2,4,8,16,32,64};
  ssb_time_alloc->sl_NumSSB_WithinPeriod =
                                      (sl_SSB_TimeAllocation_r16->sl_NumSSB_WithinPeriod_r16 != NULL)
                                             ? values[*sl_SSB_TimeAllocation_r16->sl_NumSSB_WithinPeriod_r16] : 0;
  ssb_time_alloc->sl_TimeOffsetSSB = (sl_SSB_TimeAllocation_r16->sl_TimeOffsetSSB_r16 != NULL)
                                             ? *sl_SSB_TimeAllocation_r16->sl_TimeOffsetSSB_r16 : 0;
  ssb_time_alloc->sl_TimeInterval =  (sl_SSB_TimeAllocation_r16->sl_TimeInterval_r16 != NULL)
                                             ? *sl_SSB_TimeAllocation_r16->sl_TimeInterval_r16 : 0;

}



//This API is called by RRC after it determines that UE needs to transmit SL-SSB
// SLSS id and SL-MIB is given to MAC by RRC
void nr_rrc_mac_transmit_slss_req(module_id_t module_id,
                                  uint8_t *sl_mib_payload,
                                  uint16_t tx_slss_id,
                                  NR_SL_SSB_TimeAllocation_r16_t *ssb_ta)
{

  sl_nr_ue_mac_params_t *sl_mac = get_mac_inst(module_id)->SL_MAC_PARAMS;
  AssertFatal(sl_mac,"sidelink MAC cannot be NULL");
  AssertFatal(tx_slss_id < 672,"SLSS id cannot be >= 672. id:%d", tx_slss_id);
  AssertFatal(ssb_ta,"ssb_ta cannot be NULL");

  sl_mac->tx_sl_bch.slss_id = tx_slss_id;
  sl_mac->tx_sl_bch.status = 1;
  memcpy(sl_mac->tx_sl_bch.sl_mib,sl_mib_payload, 4);

  sl_mac->tx_sl_bch.num_ssb = 0;
  sl_mac->tx_sl_bch.ssb_slot = 0;

  sl_mac_config_ssb_time_alloc(module_id,
                               ssb_ta,
                               &sl_mac->tx_sl_bch.ssb_time_alloc);

  LOG_I(NR_MAC,"[UE%d]SL RRC->MAC: TX SLSS REQ SLSS-id:%d, SL-MIB:%x, numssb:%d, offset:%d, interval:%d\n",
                                                  module_id, sl_mac->tx_sl_bch.slss_id,
                                                  *((uint32_t *)sl_mib_payload),
                                                  sl_mac->tx_sl_bch.ssb_time_alloc.sl_NumSSB_WithinPeriod,
                                                  sl_mac->tx_sl_bch.ssb_time_alloc.sl_TimeOffsetSSB,
                                                  sl_mac->tx_sl_bch.ssb_time_alloc.sl_TimeInterval);

  uint8_t byte0 = 0;
  uint8_t byte1 = 0;
  sl_nr_bwp_config_t *cfg = &sl_mac->sl_phy_config.sl_config_req.sl_bwp_config;
  sl_prepare_psbch_payload(sl_mac->sl_TDD_config, &byte0, &byte1,
                            cfg->sl_scs,cfg->sl_num_symbols,cfg->sl_start_symbol);

  sl_mac->tx_sl_bch.sl_mib[0] = byte0;
  sl_mac->tx_sl_bch.sl_mib[1] = byte1 | sl_mac->tx_sl_bch.sl_mib[1];

  LOG_I(NR_MAC, "[UE%d]SL RRC->MAC: TX SLSS REQ - TDD CONFIG STUFFED INSIDE - SL-MIB :%x\n",
                                                    module_id, *((uint32_t *)sl_mac->tx_sl_bch.sl_mib));
}

//This API is called by RRC after it determines that UE needs to keep
// receiving SL-SSB from the sync ref UE
void nr_rrc_mac_config_req_sl_mib(module_id_t module_id,
                                  NR_SL_SSB_TimeAllocation_r16_t *ssb_ta,
                                  uint16_t rx_slss_id,
                                  uint8_t *sl_mib)
{

  NR_UE_MAC_INST_t *mac = get_mac_inst(module_id);
  sl_nr_ue_mac_params_t *sl_mac = mac->SL_MAC_PARAMS;
  AssertFatal(sl_mac,"Sidelink MAC instance cannot be NULL");

  AssertFatal(ssb_ta,"ssb_ta cannot be NULL");
  AssertFatal(rx_slss_id < 672,"SLSS id cannot be >= 672. id:%d", rx_slss_id);
  AssertFatal(sl_mib,"sl_mib cannot be NULL");

  sl_nr_phy_config_request_t *sl_config = &sl_mac->sl_phy_config.sl_config_req;

  //Update configs if Sync source is not set else nothing to be done
  if (sl_config->sl_sync_source.sync_source == SL_SYNC_SOURCE_NONE) {
    //Set SYNC source as SYNC REF UE and send the remaining config to PHY
    sl_config->config_mask = 0xF;//all configs done.
    sl_config->sl_sync_source.sync_source = SL_SYNC_SOURCE_SYNC_REF_UE;
    sl_config->sl_sync_source.rx_slss_id = rx_slss_id;

    sl_mac->adjust_timing = 1;

    sl_mac->rx_sl_bch.status = 1;
    sl_mac->rx_sl_bch.slss_id = rx_slss_id;

    sl_mac->rx_sl_bch.num_ssb = 0;
    sl_mac->rx_sl_bch.ssb_slot = 0;

    sl_mac_config_ssb_time_alloc(module_id,
                                ssb_ta,
                                &sl_mac->rx_sl_bch.ssb_time_alloc);

    LOG_I(NR_MAC,"[UE%d]SL RRC->MAC: RX SLSS REQ SLSS-id:%d, SL-MIB:%x, numssb:%d, offset:%d, interval:%d\n",
                                                      module_id, sl_mac->rx_sl_bch.slss_id,
                                                      *((uint32_t *)sl_mib),
                                                    sl_mac->rx_sl_bch.ssb_time_alloc.sl_NumSSB_WithinPeriod,
                                                    sl_mac->rx_sl_bch.ssb_time_alloc.sl_TimeOffsetSSB,
                                                    sl_mac->rx_sl_bch.ssb_time_alloc.sl_TimeInterval);

    if (sl_mac->sl_TDD_config == NULL)
      sl_mac->sl_TDD_config = CALLOC(sizeof(NR_TDD_UL_DL_ConfigCommon_t), 1);

    sl_nr_phy_config_request_t *cfg = &sl_mac->sl_phy_config.sl_config_req;
    int ret = 1;
    ret = sl_decode_sl_TDD_Config(sl_mac->sl_TDD_config,
                                  sl_mib[0], sl_mib[1]&0xF0,
                                  cfg->sl_bwp_config.sl_scs,
                                  cfg->sl_bwp_config.sl_num_symbols,
                                  cfg->sl_bwp_config.sl_start_symbol);

    if (ret == 0) {
      //sl_tdd_config bytes are all 1's - no TDD config present use all slots for sidelink.
      //Spec not clear -- TBD...
      sl_config->tdd_table.tdd_period = 7;// set it to frame period
      sl_mac->sl_TDD_config->pattern1.nrofUplinkSlots =
                        NR_NUMBER_OF_SUBFRAMES_PER_FRAME*(1<<cfg->sl_bwp_config.sl_scs);
    } else {
      if (sl_mac->sl_TDD_config->pattern1.ext1 == NULL)
        sl_config->tdd_table.tdd_period = sl_mac->sl_TDD_config->pattern1.dl_UL_TransmissionPeriodicity;
      else {
        if (sl_mac->sl_TDD_config->pattern1.ext1->dl_UL_TransmissionPeriodicity_v1530 != NULL)
          sl_config->tdd_table.tdd_period += (1 + *sl_mac->sl_TDD_config->pattern1.ext1->dl_UL_TransmissionPeriodicity_v1530);
      }
    }

    uint8_t return_tdd = 0;
    return_tdd = sl_set_tdd_config_nr_ue(cfg,
                                        cfg->sl_bwp_config.sl_scs,
                                        &sl_mac->sl_TDD_config->pattern1.nrofDownlinkSlots,
                                        &sl_mac->sl_TDD_config->pattern1.nrofDownlinkSymbols,
                                        sl_mac->sl_TDD_config->pattern1.nrofUplinkSlots,
                                        sl_mac->sl_TDD_config->pattern1.nrofUplinkSymbols);
    if (return_tdd !=0)
      LOG_E(PHY,"TDD configuration can not be done\n");

    AssertFatal(get_nrUE_params()->sync_ref == 0, "Expecting Nearby UE\n");
    int scs = get_softmodem_params()->numerology;
    const int nr_slots_frame = nr_slots_per_frame[scs];
    NR_TDD_UL_DL_Pattern_t *tdd = &sl_mac->sl_TDD_config->pattern1;

    const int n_ul_slots_period = tdd ? tdd->nrofUplinkSlots + (tdd->nrofUplinkSymbols > 0 ? 1 : 0) : nr_slots_frame;
    uint16_t num_subch = sl_get_num_subch(mac->sl_tx_res_pool);
    //Jin replace hardcode list, to support multiple UES.
    //mac->sl_info.list[0]->UE_sched_ctrl.sched_psfch = calloc(n_ul_slots_period * num_subch, sizeof(SL_sched_feedback_t)); //Jin
    //mac->sl_info.list[0]->UE_sched_ctrl.sched_psfch->feedback_frame = -1;
    //mac->sl_info.list[0]->UE_sched_ctrl.sched_psfch->feedback_slot = -1;
    /******************   Jin replacement   ****************************** */
      SL_UE_iterator(mac->sl_info.list, UE) {
        const int n = sl_num_slsch_feedbacks(mac);
        UE->UE_sched_ctrl.sched_psfch = calloc(n, sizeof(SL_sched_feedback_t));
        for (int i = 0; i < n; i++) {
          UE->UE_sched_ctrl.sched_psfch = calloc(n_ul_slots_period * num_subch, sizeof(SL_sched_feedback_t));
          UE->UE_sched_ctrl.sched_psfch->feedback_frame = -1;
          UE->UE_sched_ctrl.sched_psfch->feedback_slot  = -1;
        }
      }
    /************************************************ */


    LOG_I(MAC, "SIDELINK CONFIGs: tdd config period:%d, mu:%ld, DLslots:%ld,ULslots:%ld Mixedslotsym DL:UL %ld:%ld\n",
                            sl_config->tdd_table.tdd_period, sl_mac->sl_TDD_config->referenceSubcarrierSpacing,
                            sl_mac->sl_TDD_config->pattern1.nrofDownlinkSlots, sl_mac->sl_TDD_config->pattern1.nrofUplinkSlots,
                            sl_mac->sl_TDD_config->pattern1.nrofDownlinkSymbols,sl_mac->sl_TDD_config->pattern1.nrofUplinkSymbols);

    int nr_slots_period = nr_slots_frame;
    int nr_ulstart_slot = 0;
    if (tdd) {
      nr_ulstart_slot = get_first_ul_slot(tdd->nrofDownlinkSlots, tdd->nrofDownlinkSymbols, tdd->nrofUplinkSymbols);
      nr_slots_period /= get_nb_periods_per_frame(tdd->dl_UL_TransmissionPeriodicity);
    }

    for (int slot = 0; slot < nr_slots_frame; ++slot) {
      mac->ulsch_slot_bitmap[slot / 64] |= (uint64_t)((slot % nr_slots_period) >= nr_ulstart_slot) << (slot % 64);
      LOG_D(NR_MAC,
            "slot %d UL %d\n",
            slot,
            (mac->ulsch_slot_bitmap[slot / 64] & ((uint64_t)1 << (slot % 64))) != 0);
    }

    BIT_STRING_t *sl_time_rsrc = mac->sl_tx_res_pool->ext1->sl_TimeResource_r16;
    int total_downlink_slots = ((sl_time_rsrc->size << 3) - sl_time_rsrc->bits_unused) / n_ul_slots_period * (nr_slots_period - n_ul_slots_period);
    int phy_sl_size = ((sl_time_rsrc->size << 3) - sl_time_rsrc->bits_unused) + total_downlink_slots;
    LOG_D(NR_MAC, "size of phy_sl_map  %d total_downlink_slots %d, sl_time_rsrc->size %ld, n_ul_slots_period %d, (nr_slots_period - n_ul_slots_period) %d\n", phy_sl_size, total_downlink_slots, ((sl_time_rsrc->size << 3) - sl_time_rsrc->bits_unused), n_ul_slots_period, (nr_slots_period - n_ul_slots_period));

    size_t byte_capacity = (phy_sl_size + 7) / 8;
    uint8_t pool_id = 0;
    SL_ResourcePool_params_t *sl_tx_rsrc_pool = sl_mac->sl_TxPool[pool_id];
    SL_ResourcePool_params_t *sl_rx_rsrc_pool = sl_mac->sl_RxPool[pool_id];

    sl_tx_rsrc_pool->phy_sl_bitmap.buf = (uint8_t*)malloc16_clear(byte_capacity);
    sl_rx_rsrc_pool->phy_sl_bitmap.buf = (uint8_t*)malloc16_clear(byte_capacity);
    sl_tx_rsrc_pool->phy_sl_bitmap.size = (phy_sl_size + 7) >> 3;
    sl_tx_rsrc_pool->phy_sl_bitmap.bits_unused = ((sl_tx_rsrc_pool->phy_sl_bitmap.size << 3) - phy_sl_size) % 8;

    BIT_STRING_t *phy_sl_bitmap = &sl_tx_rsrc_pool->phy_sl_bitmap;
    uint16_t tx_phy_map_sz = get_physical_sl_pool(mac, sl_time_rsrc, phy_sl_bitmap);

    sl_time_rsrc = mac->sl_rx_res_pool->ext1->sl_TimeResource_r16;
    phy_sl_bitmap = &sl_rx_rsrc_pool->phy_sl_bitmap;
    sl_rx_rsrc_pool->phy_sl_bitmap.size = (phy_sl_size + 7) >> 3;
    sl_rx_rsrc_pool->phy_sl_bitmap.bits_unused = ((sl_rx_rsrc_pool->phy_sl_bitmap.size << 3) - phy_sl_size) % 8;

    uint16_t rx_phy_map_sz = get_physical_sl_pool(mac, sl_time_rsrc, phy_sl_bitmap);

    AssertFatal(tx_phy_map_sz == rx_phy_map_sz, "Transmit and Receive physical sidelink bitmap does not have same length!!!");

    DevAssert(mac->if_module != NULL && mac->if_module->sl_phy_config_request != NULL);
    mac->if_module->sl_phy_config_request(&sl_mac->sl_phy_config);
  }

}

void nr_sl_params_read_conf(module_id_t module_id) {
  NR_UE_MAC_INST_t *mac = get_mac_inst(module_id);
  sl_nr_ue_mac_params_t *sl_mac = mac->SL_MAC_PARAMS;

  char aprefix[MAX_OPTNAME_SIZE*2 + 8];
  sl_csi_info_t *sl_csi_rs_info = (sl_csi_info_t *)malloc16_clear(sizeof(sl_csi_info_t));
  paramdef_t SL_CRI_RS_INFO[] = SL_CSI_RS_DESC(sl_csi_rs_info);
  paramlist_def_t SL_CRI_RS_List = {SL_CONFIG_STRING_SL_CSI_RS_LIST, NULL, 0};
  sprintf(aprefix, "%s.[%d]", SL_CONFIG_STRING_SL_PRECONFIGURATION, 0);
  config_getlist(&SL_CRI_RS_List, NULL, 0, aprefix);
  sprintf(aprefix, "%s.[%i].%s.[%i]", SL_CONFIG_STRING_SL_PRECONFIGURATION, 0, SL_CONFIG_STRING_SL_CSI_RS_LIST, 0);
  config_get(SL_CRI_RS_INFO, sizeof(SL_CRI_RS_INFO)/sizeof(paramdef_t), aprefix);

  char aprefix1[MAX_OPTNAME_SIZE*2 + 8];
  sl_harq_info_t *sl_harq_info = (sl_harq_info_t*)malloc16_clear(sizeof(sl_harq_info_t));
  paramdef_t SL_HARQ_INFO[] = SL_CONFIGUREDGRANT_DESC(sl_harq_info);
  sprintf(aprefix1, "%s.[%i].%s.[%i]", SL_CONFIG_STRING_SL_PRECONFIGURATION, 0, SL_CONFIG_STRING_SL_CONFIGUREDGRANT_LIST, 0);
  config_get(SL_HARQ_INFO, sizeof(SL_HARQ_INFO)/sizeof(paramdef_t), aprefix1);
  sl_mac->sl_Num_HARQ_Processes = sl_harq_info->sl_Num_HARQ_Processes;
  sl_mac->sl_HARQ_ProcID_offset = sl_harq_info->sl_HARQ_ProcID_offset;
  sl_mac->sl_Periodic_RRI = sl_harq_info->sl_Periodic_RRI;

  sl_mac->csi_type = sl_csi_rs_info->csi_type;
  sl_mac->symb_l0 = sl_csi_rs_info->symb_l0;
  sl_mac->power_control_offset = sl_csi_rs_info->power_control_offset;
  sl_mac->power_control_offset_ss = sl_csi_rs_info->power_control_offset_ss;
  sl_mac->measurement_bitmap = 0b00011011;
  sl_mac->sl_CSI_Acquisition = sl_csi_rs_info->sl_csi_acquisition;
  sl_mac->sl_LatencyBoundCSI_Report = sl_csi_rs_info->sl_latencyboundcsi_report;
  // Based on 38211 Table 7.4.1.5.3-1, for density of 1 and ports 1 & 2 ENUMERATED {noCDM, fd-CDM2}
  sl_mac->cdm_type = get_nrUE_params()->nb_antennas_tx == 1 ? 0 : 1;
  sl_mac->row = get_nrUE_params()->nb_antennas_tx == 1 ? 2 : 3;
  // Only 1 is supported (38211, 8.4.1.5.3) - means CSI-RS transmission in every resource block
  sl_mac->freq_density = 1;
  sl_mac->freq_domain = 0b000000000001; //bitmap size is dependent upon row size; for row 2 length is 12 bits else 6 bits;
  int loc_bw = mac->sl_bwp->sl_BWP_Generic_r16->sl_BWP_r16->locationAndBandwidth;
  uint16_t bwp_start = NRRIV2PRBOFFSET(loc_bw, MAX_BWP_SIZE);
  // PRB where this CSI resource starts in relation to common resource block #0 (CRB#0) on the common resource block grid.
  // Only multiples of 4 are allowed (0, 4, ...)
  // INTEGER (0..maxNrofPhysicalResourceBlocks-1)
  sl_mac->start_rb = (bwp_start % 4 == 0) ? bwp_start : ((bwp_start >> 2) << 2) + 4;
  // Number of PRBs across which this CSI resource spans. The smallest configurable number is the minimum of 24 and the width of the associated BWP
  // Only multiples of 4 are allowed.
  // INTEGER (24..maxNrofPhysicalResourceBlocksPlus1)
  uint16_t max_allowable_num_rbs = (NRRIV2BW(loc_bw, MAX_BWP_SIZE) >> 2) << 2;
  sl_mac->nr_of_rbs = min(24, max_allowable_num_rbs);

  LOG_D(NR_MAC, "loc_bw %i, start_rb %i, nr_of_rbs %i, max_allowable_num_rbs %i\n", loc_bw, sl_mac->start_rb, sl_mac->nr_of_rbs, max_allowable_num_rbs);

  char aprefix_rsc[MAX_OPTNAME_SIZE*2 + 8];
  sprintf(aprefix_rsc, "%s.[%d]", SL_CONFIG_STRING_SL_PRECONFIGURATION, 0);

  uint16_t* resource_selection_cfg = (uint16_t *)malloc16_clear(sizeof(*resource_selection_cfg));
  paramdef_t SL_CONFIG_RSR_INFO[] = SL_CONFIG_RESOURCE_SELECTION(resource_selection_cfg);
  config_get(SL_CONFIG_RSR_INFO, sizeof(SL_CONFIG_RSR_INFO) / sizeof(paramdef_t), aprefix_rsc);

  switch(*resource_selection_cfg) {
    case 0:
      mac->rsc_selection_method = c1;
      break;
    case 3:
      mac->rsc_selection_method = c4;
      break;
    case 4:
      mac->rsc_selection_method = c5;
      break;
    case 6:
      mac->rsc_selection_method = c7;
      break;
    default:
      LOG_D(NR_MAC, "Provided resource selection mechanism is not supported!!!\n");
      break;
  }
}