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

/* \file        nr_ue_scheduler.c
 * \brief       Routines for UE scheduling
 * \author      Guido Casati
 * \date        Jan 2021
 * \version     0.1
 * \company     Fraunhofer IIS
 * \email       guido.casati@iis.fraunhofer.de
 */

#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include <arpa/inet.h> //Jin add
/* exe */
#include <common/utils/nr/nr_common.h>
#include <inttypes.h>//JIn add
/* PHY */
#include "openair1/PHY/impl_defs_top.h"

/* MAC */
#include "NR_MAC_COMMON/nr_mac.h"
#include "NR_MAC_COMMON/nr_mac_common.h"
#include "NR_MAC_UE/mac_proto.h"
#include "NR_MAC_UE/mac_extern.h"
#include "NR_MAC_UE/nr_ue_sci.h"
/* utils */
#include "assertions.h"
#include "oai_asn1.h"
#include "SIMULATION/TOOLS/sim.h" // for taus
#include "utils.h"

#include <executables/softmodem-common.h>
#include <executables/nr-uesoftmodem.h>
#include "LAYER2/NR_MAC_COMMON/nr_mac_extern.h"
#include "LAYER2/RLC/rlc.h"

extern const int pscch_tda[2];
extern const int pscch_rb_table[5];
//#define SRS_DEBUG
#define SLOT_INFO_DEBUG
#define BITMAP_DEBUG

static prach_association_pattern_t prach_assoc_pattern;
static void nr_ue_prach_scheduler(module_id_t module_idP, frame_t frameP, sub_frame_t slotP);

static void print_candidate_list(List_t *candidate_resources, int line) {
  for (int i = 0; i < candidate_resources->size; i++) {
    sl_resource_info_t *itr_rsrc = (sl_resource_info_t*)((char*)candidate_resources->data + i * candidate_resources->element_size);
    LOG_D(NR_MAC, "line %d, %4d.%2d, %ld, sl_subchan_len %d\n", line, itr_rsrc->sfn.frame, itr_rsrc->sfn.slot, normalize(&itr_rsrc->sfn, 1), itr_rsrc->sl_subchan_len);
  }
}

static void print_reserved_list(List_t *candidate_resources, int line) {
  for (int i = 0; i < candidate_resources->size; i++) {
    reserved_resource_t *itr_rsrc = (reserved_resource_t*)((char*)candidate_resources->data + i * candidate_resources->element_size);
    LOG_D(NR_MAC, "line %d, %4d.%2d, %ld, sl_subchan_len %d\n", line, itr_rsrc->sfn.frame, itr_rsrc->sfn.slot, normalize(&itr_rsrc->sfn, 1), itr_rsrc->sb_ch_length);
  }
}

static void print_sensing_data_list(List_t *sensing_data, int line) {
  for (int i = 0; i < sensing_data->size; i++) {
    sensing_data_t *itr_rsrc = (sensing_data_t*)((char*)sensing_data->data + i * sensing_data->element_size);
    LOG_D(NR_MAC, "line %d, %4d.%2d, %ld, sl_subchan_len %d\n", line, itr_rsrc->frame_slot.frame, itr_rsrc->frame_slot.slot, normalize(&itr_rsrc->frame_slot, 1), itr_rsrc->subch_len);
  }
}

sl_resource_info_t* get_resource_element(List_t* resource_list, frameslot_t sfn) {
  for (int i = 0; i < resource_list->size; i++) {
    sl_resource_info_t *itr_rsrc = (sl_resource_info_t*)((char*)resource_list->data + i * resource_list->element_size);
    LOG_D(NR_MAC, "%s %4d.%2d, %ld, sl_subchan_len %d, current sfn %4d.%2d\n",
          __FUNCTION__, itr_rsrc->sfn.frame, itr_rsrc->sfn.slot, normalize(&itr_rsrc->sfn, 1), itr_rsrc->sl_subchan_len, sfn.frame, sfn.slot);
    if (itr_rsrc->sfn.frame == sfn.frame && itr_rsrc->sfn.slot == sfn.slot) {
      return itr_rsrc;
    }
  }
  return NULL;
}

void fill_ul_config(fapi_nr_ul_config_request_t *ul_config, frame_t frame_tx, int slot_tx, uint8_t pdu_type){

  AssertFatal(ul_config->number_pdus < sizeof(ul_config->ul_config_list) / sizeof(ul_config->ul_config_list[0]),
              "Number of PDUS in ul_config = %d > ul_config_list num elements", ul_config->number_pdus);
  // clear ul_config for new frame/slot
  if ((ul_config->slot != slot_tx || ul_config->sfn != frame_tx) &&
      ul_config->number_pdus != 0 &&
      !get_softmodem_params()->emulate_l1) {
    LOG_D(MAC, "%d.%d %d.%d f clear ul_config %p t %d pdu %d\n", frame_tx, slot_tx, ul_config->sfn, ul_config->slot, ul_config, pdu_type, ul_config->number_pdus);
    ul_config->number_pdus = 0;
    memset(ul_config->ul_config_list, 0, sizeof(ul_config->ul_config_list));
  }
  ul_config->ul_config_list[ul_config->number_pdus].pdu_type = pdu_type;
  //ul_config->slot = slot_tx;
  //ul_config->sfn = frame_tx;
  ul_config->slot = slot_tx;
  ul_config->sfn = frame_tx;
  ul_config->number_pdus++;

  LOG_D(NR_MAC, "In %s: Set config request for UL transmission in [%d.%d], number of UL PDUs: %d\n", __FUNCTION__, ul_config->sfn, ul_config->slot, ul_config->number_pdus);

}

void fill_scheduled_response(nr_scheduled_response_t *scheduled_response,
                             fapi_nr_dl_config_request_t *dl_config,
                             fapi_nr_ul_config_request_t *ul_config,
                             fapi_nr_tx_request_t *tx_request,
                             sl_nr_rx_config_request_t *sl_rx_config,
                             sl_nr_tx_config_request_t *sl_tx_config,
                             module_id_t mod_id,
                             int cc_id,
                             frame_t frame,
                             int slot,
                             void *phy_data){

  scheduled_response->dl_config  = dl_config;
  scheduled_response->ul_config  = ul_config;
  scheduled_response->tx_request = tx_request;
  scheduled_response->module_id  = mod_id;
  scheduled_response->CC_id      = cc_id;
  scheduled_response->frame      = frame;
  scheduled_response->slot       = slot;
  scheduled_response->phy_data   = phy_data;
  scheduled_response->sl_rx_config  = sl_rx_config;
  scheduled_response->sl_tx_config  = sl_tx_config;

}

/*
 * This function returns the UL config corresponding to a given UL slot
 * from MAC instance .
 */
fapi_nr_ul_config_request_t *get_ul_config_request(NR_UE_MAC_INST_t *mac, int slot, int fb_time)
{

  NR_TDD_UL_DL_ConfigCommon_t *tdd_config = mac->scc==NULL ? mac->scc_SIB->tdd_UL_DL_ConfigurationCommon : mac->scc->tdd_UL_DL_ConfigurationCommon;

  //Check if requested on the right slot
  AssertFatal(is_nr_UL_slot(tdd_config, slot, mac->frame_type) != 0, "UL config_request called at wrong slot %d\n", slot);

  int mu = mac->current_UL_BWP.scs;
  const int n = nr_slots_per_frame[mu];
  AssertFatal(fb_time < n, "Cannot schedule to a slot more than 1 frame away, ul_config_request is not big enough\n");
  AssertFatal(mac->ul_config_request != NULL, "mac->ul_config_request not initialized, logic bug\n");
  return &mac->ul_config_request[slot];
}

/*
 * This function returns the DL config corresponding to a given DL slot
 * from MAC instance .
 */
fapi_nr_dl_config_request_t *get_dl_config_request(NR_UE_MAC_INST_t *mac, int slot)
{
  AssertFatal(mac->dl_config_request != NULL, "mac->dl_config_request not initialized, logic bug\n");
  return &mac->dl_config_request[slot];
}

void ul_layers_config(NR_UE_MAC_INST_t *mac, nfapi_nr_ue_pusch_pdu_t *pusch_config_pdu, dci_pdu_rel15_t *dci, nr_dci_format_t dci_format)
{
  NR_UE_UL_BWP_t *current_UL_BWP = &mac->current_UL_BWP;
  NR_SRS_Config_t *srs_config = current_UL_BWP->srs_Config;
  NR_PUSCH_Config_t *pusch_Config = current_UL_BWP->pusch_Config;

  long transformPrecoder = pusch_config_pdu->transform_precoding;

  /* PRECOD_NBR_LAYERS */
  // 0 bits if the higher layer parameter txConfig = nonCodeBook

  if (*pusch_Config->txConfig == NR_PUSCH_Config__txConfig_codebook){

    // The UE shall transmit PUSCH using the same antenna port(s) as the SRS port(s) in the SRS resource indicated by the DCI format 0_1
    // 38.214  Section 6.1.1

    uint8_t n_antenna_port = get_pusch_nb_antenna_ports(pusch_Config, srs_config, dci->srs_resource_indicator);

    // 1 antenna port and the higher layer parameter txConfig = codebook 0 bits

    if (n_antenna_port == 4) { // 4 antenna port and the higher layer parameter txConfig = codebook

      // Table 7.3.1.1.2-2: transformPrecoder=disabled and maxRank = 2 or 3 or 4
      if ((transformPrecoder == NR_PUSCH_Config__transformPrecoder_disabled)
        && ((*pusch_Config->maxRank == 2) ||
        (*pusch_Config->maxRank == 3) ||
        (*pusch_Config->maxRank == 4))){

        if (*pusch_Config->codebookSubset == NR_PUSCH_Config__codebookSubset_fullyAndPartialAndNonCoherent) {
          pusch_config_pdu->nrOfLayers = table_7_3_1_1_2_2_3_4_5[dci->precoding_information.val][0];
          pusch_config_pdu->Tpmi = table_7_3_1_1_2_2_3_4_5[dci->precoding_information.val][1];
        }

        if (*pusch_Config->codebookSubset == NR_PUSCH_Config__codebookSubset_partialAndNonCoherent){
          pusch_config_pdu->nrOfLayers = table_7_3_1_1_2_2_3_4_5[dci->precoding_information.val][2];
          pusch_config_pdu->Tpmi = table_7_3_1_1_2_2_3_4_5[dci->precoding_information.val][3];
        }

        if (*pusch_Config->codebookSubset == NR_PUSCH_Config__codebookSubset_nonCoherent){
          pusch_config_pdu->nrOfLayers = table_7_3_1_1_2_2_3_4_5[dci->precoding_information.val][4];
          pusch_config_pdu->Tpmi = table_7_3_1_1_2_2_3_4_5[dci->precoding_information.val][5];
        }
      }

      // Table 7.3.1.1.2-3: transformPrecoder= enabled, or transformPrecoder=disabled and maxRank = 1
      if (((transformPrecoder == NR_PUSCH_Config__transformPrecoder_enabled)
        || (transformPrecoder == NR_PUSCH_Config__transformPrecoder_disabled))
        && (*pusch_Config->maxRank == 1)) {

        if (*pusch_Config->codebookSubset == NR_PUSCH_Config__codebookSubset_fullyAndPartialAndNonCoherent) {
          pusch_config_pdu->nrOfLayers = table_7_3_1_1_2_2_3_4_5[dci->precoding_information.val][6];
          pusch_config_pdu->Tpmi = table_7_3_1_1_2_2_3_4_5[dci->precoding_information.val][7];
        }

        if (*pusch_Config->codebookSubset == NR_PUSCH_Config__codebookSubset_partialAndNonCoherent){
          pusch_config_pdu->nrOfLayers = table_7_3_1_1_2_2_3_4_5[dci->precoding_information.val][8];
          pusch_config_pdu->Tpmi = table_7_3_1_1_2_2_3_4_5[dci->precoding_information.val][9];
        }

        if (*pusch_Config->codebookSubset == NR_PUSCH_Config__codebookSubset_nonCoherent){
          pusch_config_pdu->nrOfLayers = table_7_3_1_1_2_2_3_4_5[dci->precoding_information.val][10];
          pusch_config_pdu->Tpmi = table_7_3_1_1_2_2_3_4_5[dci->precoding_information.val][11];
        }
      }
    }

    if (n_antenna_port == 2) {
      // 2 antenna port and the higher layer parameter txConfig = codebook
      // Table 7.3.1.1.2-4: transformPrecoder=disabled and maxRank = 2
      if ((transformPrecoder == NR_PUSCH_Config__transformPrecoder_disabled) && (*pusch_Config->maxRank == 2)) {
        if (*pusch_Config->codebookSubset == NR_PUSCH_Config__codebookSubset_fullyAndPartialAndNonCoherent) {
          pusch_config_pdu->nrOfLayers = table_7_3_1_1_2_2_3_4_5[dci->precoding_information.val][12];
          pusch_config_pdu->Tpmi = table_7_3_1_1_2_2_3_4_5[dci->precoding_information.val][13];
        }

        if (*pusch_Config->codebookSubset == NR_PUSCH_Config__codebookSubset_nonCoherent){
          pusch_config_pdu->nrOfLayers = table_7_3_1_1_2_2_3_4_5[dci->precoding_information.val][14];
          pusch_config_pdu->Tpmi = table_7_3_1_1_2_2_3_4_5[dci->precoding_information.val][15];
        }

      }

      // Table 7.3.1.1.2-5: transformPrecoder= enabled, or transformPrecoder= disabled and maxRank = 1
      if (((transformPrecoder == NR_PUSCH_Config__transformPrecoder_enabled)
        || (transformPrecoder == NR_PUSCH_Config__transformPrecoder_disabled))
        && (*pusch_Config->maxRank == 1)) {

        if (*pusch_Config->codebookSubset == NR_PUSCH_Config__codebookSubset_fullyAndPartialAndNonCoherent) {
          pusch_config_pdu->nrOfLayers = table_7_3_1_1_2_2_3_4_5[dci->precoding_information.val][16];
          pusch_config_pdu->Tpmi = table_7_3_1_1_2_2_3_4_5[dci->precoding_information.val][17];
        }

        if (*pusch_Config->codebookSubset == NR_PUSCH_Config__codebookSubset_nonCoherent){
          pusch_config_pdu->nrOfLayers = table_7_3_1_1_2_2_3_4_5[dci->precoding_information.val][18];
          pusch_config_pdu->Tpmi = table_7_3_1_1_2_2_3_4_5[dci->precoding_information.val][19];
        }

      }
    }
  }
}

// todo: this function shall be reviewed completely because of the many comments left by the author
void ul_ports_config(NR_UE_MAC_INST_t *mac, int *n_front_load_symb, nfapi_nr_ue_pusch_pdu_t *pusch_config_pdu, dci_pdu_rel15_t *dci, nr_dci_format_t dci_format)
{
  uint8_t rank = pusch_config_pdu->nrOfLayers;

  NR_PUSCH_Config_t *pusch_Config = mac->current_UL_BWP.pusch_Config;
  AssertFatal(pusch_Config!=NULL,"pusch_Config shouldn't be null\n");

  long transformPrecoder = pusch_config_pdu->transform_precoding;
  LOG_D(NR_MAC,"transformPrecoder %s\n", transformPrecoder==NR_PUSCH_Config__transformPrecoder_disabled ? "disabled" : "enabled");

  long *max_length = NULL;
  long *dmrs_type = NULL;
  if (pusch_Config->dmrs_UplinkForPUSCH_MappingTypeA) {
    max_length = pusch_Config->dmrs_UplinkForPUSCH_MappingTypeA->choice.setup->maxLength;
    dmrs_type = pusch_Config->dmrs_UplinkForPUSCH_MappingTypeA->choice.setup->dmrs_Type;
  }
  else {
    max_length = pusch_Config->dmrs_UplinkForPUSCH_MappingTypeB->choice.setup->maxLength;
    dmrs_type = pusch_Config->dmrs_UplinkForPUSCH_MappingTypeB->choice.setup->dmrs_Type;
  }

  LOG_D(NR_MAC,"MappingType%s max_length %s, dmrs_type %s, antenna_ports %d\n",
        pusch_Config->dmrs_UplinkForPUSCH_MappingTypeA?"A":"B",max_length?"len2":"len1",dmrs_type?"type2":"type1",dci->antenna_ports.val);

  if ((transformPrecoder == NR_PUSCH_Config__transformPrecoder_enabled) &&
      (dmrs_type == NULL) && (max_length == NULL)) { // tables 7.3.1.1.2-6
      pusch_config_pdu->num_dmrs_cdm_grps_no_data = 2;
      pusch_config_pdu->dmrs_ports = 1<<dci->antenna_ports.val;
  }

  if ((transformPrecoder == NR_PUSCH_Config__transformPrecoder_enabled) &&
      (dmrs_type == NULL) && (max_length != NULL)) { // tables 7.3.1.1.2-7

    pusch_config_pdu->num_dmrs_cdm_grps_no_data = 2; //TBC
    pusch_config_pdu->dmrs_ports = 1<<((dci->antenna_ports.val > 3)?(dci->antenna_ports.val-4):(dci->antenna_ports.val));
    *n_front_load_symb = (dci->antenna_ports.val > 3)?2:1;
  }

  if ((transformPrecoder == NR_PUSCH_Config__transformPrecoder_disabled) &&
    (dmrs_type == NULL) && (max_length == NULL)) { // tables 7.3.1.1.2-8/9/10/11

    if (rank == 1) {
      pusch_config_pdu->num_dmrs_cdm_grps_no_data = (dci->antenna_ports.val > 1)?2:1;
      pusch_config_pdu->dmrs_ports =1<<((dci->antenna_ports.val > 1)?(dci->antenna_ports.val-2):(dci->antenna_ports.val));
    }

    if (rank == 2){
      pusch_config_pdu->num_dmrs_cdm_grps_no_data = (dci->antenna_ports.val > 0)?2:1;
      pusch_config_pdu->dmrs_ports = (dci->antenna_ports.val > 1)?((dci->antenna_ports.val> 2)?0x5:0xc):0x3;
    }

    if (rank == 3){
      pusch_config_pdu->num_dmrs_cdm_grps_no_data = 2;
      pusch_config_pdu->dmrs_ports = 0x7;  // ports 0-2
    }

    if (rank == 4){
      pusch_config_pdu->num_dmrs_cdm_grps_no_data = 2;
      pusch_config_pdu->dmrs_ports = 0xf;  // ports 0-3
    }
  }

  if ((transformPrecoder == NR_PUSCH_Config__transformPrecoder_disabled) &&
    (dmrs_type == NULL) && (max_length != NULL)) { // tables 7.3.1.1.2-12/13/14/15

    if (rank == 1){
      pusch_config_pdu->num_dmrs_cdm_grps_no_data = (dci->antenna_ports.val > 1)?2:1; //TBC
      pusch_config_pdu->dmrs_ports = 1<<((dci->antenna_ports.val > 1)?(dci->antenna_ports.val > 5 ?(dci->antenna_ports.val-6):(dci->antenna_ports.val-2)):dci->antenna_ports.val);
      *n_front_load_symb = (dci->antenna_ports.val > 6)?2:1;
    }

    if (rank == 2){
      pusch_config_pdu->num_dmrs_cdm_grps_no_data = (dci->antenna_ports.val > 0)?2:1; //TBC
      pusch_config_pdu->dmrs_ports = 0; //FIXME
      //pusch_config_pdu->dmrs_ports[0] = table_7_3_1_1_2_13[dci->antenna_ports.val][1];
      //pusch_config_pdu->dmrs_ports[1] = table_7_3_1_1_2_13[dci->antenna_ports.val][2];
      //n_front_load_symb = (dci->antenna_ports.val > 3)?2:1; // FIXME
    }

    if (rank == 3){
      pusch_config_pdu->num_dmrs_cdm_grps_no_data = 2; //TBC
      pusch_config_pdu->dmrs_ports = 0; //FIXME
      //pusch_config_pdu->dmrs_ports[0] = table_7_3_1_1_2_14[dci->antenna_ports.val][1];
      //pusch_config_pdu->dmrs_ports[1] = table_7_3_1_1_2_14[dci->antenna_ports.val][2];
      //pusch_config_pdu->dmrs_ports[2] = table_7_3_1_1_2_14[dci->antenna_ports.val][3];
      //n_front_load_symb = (dci->antenna_ports.val > 1)?2:1; //FIXME
    }

    if (rank == 4){
      pusch_config_pdu->num_dmrs_cdm_grps_no_data = 2; //TBC
      pusch_config_pdu->dmrs_ports = 0; //FIXME
      //pusch_config_pdu->dmrs_ports[0] = table_7_3_1_1_2_15[dci->antenna_ports.val][1];
      //pusch_config_pdu->dmrs_ports[1] = table_7_3_1_1_2_15[dci->antenna_ports.val][2];
      //pusch_config_pdu->dmrs_ports[2] = table_7_3_1_1_2_15[dci->antenna_ports.val][3];
      //pusch_config_pdu->dmrs_ports[3] = table_7_3_1_1_2_15[dci->antenna_ports.val][4];
      //n_front_load_symb = (dci->antenna_ports.val > 1)?2:1; //FIXME
    }
  }

  if ((transformPrecoder == NR_PUSCH_Config__transformPrecoder_disabled) &&
    (dmrs_type != NULL) &&
    (max_length == NULL)) { // tables 7.3.1.1.2-16/17/18/19

    if (rank == 1){
      pusch_config_pdu->num_dmrs_cdm_grps_no_data = (dci->antenna_ports.val > 1)?((dci->antenna_ports.val > 5)?3:2):1; //TBC
      pusch_config_pdu->dmrs_ports = (dci->antenna_ports.val > 1)?(dci->antenna_ports.val > 5 ?(dci->antenna_ports.val-6):(dci->antenna_ports.val-2)):dci->antenna_ports.val; //TBC
    }

    if (rank == 2){
      pusch_config_pdu->num_dmrs_cdm_grps_no_data = (dci->antenna_ports.val > 0)?((dci->antenna_ports.val > 2)?3:2):1; //TBC
      pusch_config_pdu->dmrs_ports = 0; //FIXME
      //pusch_config_pdu->dmrs_ports[0] = table_7_3_1_1_2_17[dci->antenna_ports.val][1];
      //pusch_config_pdu->dmrs_ports[1] = table_7_3_1_1_2_17[dci->antenna_ports.val][2];
    }

    if (rank == 3){
      pusch_config_pdu->num_dmrs_cdm_grps_no_data = (dci->antenna_ports.val > 0)?3:2; //TBC
      pusch_config_pdu->dmrs_ports = 0; //FIXME
      //pusch_config_pdu->dmrs_ports[0] = table_7_3_1_1_2_18[dci->antenna_ports.val][1];
      //pusch_config_pdu->dmrs_ports[1] = table_7_3_1_1_2_18[dci->antenna_ports.val][2];
      //pusch_config_pdu->dmrs_ports[2] = table_7_3_1_1_2_18[dci->antenna_ports.val][3];
    }

    if (rank == 4){
      pusch_config_pdu->num_dmrs_cdm_grps_no_data = dci->antenna_ports.val + 2; //TBC
      pusch_config_pdu->dmrs_ports = 0; //FIXME
      //pusch_config_pdu->dmrs_ports[0] = 0;
      //pusch_config_pdu->dmrs_ports[1] = 1;
      //pusch_config_pdu->dmrs_ports[2] = 2;
      //pusch_config_pdu->dmrs_ports[3] = 3;
    }
  }

  if ((transformPrecoder == NR_PUSCH_Config__transformPrecoder_disabled) &&
    (dmrs_type != NULL) && (max_length != NULL)) { // tables 7.3.1.1.2-20/21/22/23

    if (rank == 1){
      pusch_config_pdu->num_dmrs_cdm_grps_no_data = table_7_3_1_1_2_20[dci->antenna_ports.val][0]; //TBC
      pusch_config_pdu->dmrs_ports = table_7_3_1_1_2_20[dci->antenna_ports.val][1]; //TBC
      //n_front_load_symb = table_7_3_1_1_2_20[dci->antenna_ports.val][2]; //FIXME
    }

    if (rank == 2){
      pusch_config_pdu->num_dmrs_cdm_grps_no_data = table_7_3_1_1_2_21[dci->antenna_ports.val][0]; //TBC
      pusch_config_pdu->dmrs_ports = 0; //FIXME
      //pusch_config_pdu->dmrs_ports[0] = table_7_3_1_1_2_21[dci->antenna_ports.val][1];
      //pusch_config_pdu->dmrs_ports[1] = table_7_3_1_1_2_21[dci->antenna_ports.val][2];
      //n_front_load_symb = table_7_3_1_1_2_21[dci->antenna_ports.val][3]; //FIXME
      }

    if (rank == 3){
      pusch_config_pdu->num_dmrs_cdm_grps_no_data = table_7_3_1_1_2_22[dci->antenna_ports.val][0]; //TBC
      pusch_config_pdu->dmrs_ports = 0; //FIXME
      //pusch_config_pdu->dmrs_ports[0] = table_7_3_1_1_2_22[dci->antenna_ports.val][1];
      //pusch_config_pdu->dmrs_ports[1] = table_7_3_1_1_2_22[dci->antenna_ports.val][2];
      //pusch_config_pdu->dmrs_ports[2] = table_7_3_1_1_2_22[dci->antenna_ports.val][3];
      //n_front_load_symb = table_7_3_1_1_2_22[dci->antenna_ports.val][4]; //FIXME
    }

    if (rank == 4){
      pusch_config_pdu->num_dmrs_cdm_grps_no_data = table_7_3_1_1_2_23[dci->antenna_ports.val][0]; //TBC
      pusch_config_pdu->dmrs_ports = 0; //FIXME
      //pusch_config_pdu->dmrs_ports[0] = table_7_3_1_1_2_23[dci->antenna_ports.val][1];
      //pusch_config_pdu->dmrs_ports[1] = table_7_3_1_1_2_23[dci->antenna_ports.val][2];
      //pusch_config_pdu->dmrs_ports[2] = table_7_3_1_1_2_23[dci->antenna_ports.val][3];
      //pusch_config_pdu->dmrs_ports[3] = table_7_3_1_1_2_23[dci->antenna_ports.val][4];
      //n_front_load_symb = table_7_3_1_1_2_23[dci->antenna_ports.val][5]; //FIXME
    }
  }
  LOG_D(NR_MAC,"num_dmrs_cdm_grps_no_data %d, dmrs_ports %d\n",pusch_config_pdu->num_dmrs_cdm_grps_no_data,pusch_config_pdu->dmrs_ports);
}

// Configuration of Msg3 PDU according to clauses:
// - 8.3 of 3GPP TS 38.213 version 16.3.0 Release 16
// - 6.1.2.2 of TS 38.214
// - 6.1.3 of TS 38.214
// - 6.2.2 of TS 38.214
// - 6.1.4.2 of TS 38.214
// - 6.4.1.1.1 of TS 38.211
// - 6.3.1.7 of 38.211
int nr_config_pusch_pdu(NR_UE_MAC_INST_t *mac,
                        NR_tda_info_t *tda_info,
                        nfapi_nr_ue_pusch_pdu_t *pusch_config_pdu,
                        dci_pdu_rel15_t *dci,
                        RAR_grant_t *rar_grant,
                        uint16_t rnti,
                        const nr_dci_format_t *dci_format)
{

  int f_alloc;
  int mask;
  uint8_t nb_dmrs_re_per_rb;

  uint16_t        l_prime_mask = 0;
  uint16_t number_dmrs_symbols = 0;
  int                N_PRB_oh  = 0;

  int rnti_type = get_rnti_type(mac, rnti);
  NR_UE_UL_BWP_t *current_UL_BWP = &mac->current_UL_BWP;

  // Common configuration
  pusch_config_pdu->dmrs_config_type = pusch_dmrs_type1;
  pusch_config_pdu->pdu_bit_map      = PUSCH_PDU_BITMAP_PUSCH_DATA;
  pusch_config_pdu->nrOfLayers       = 1;
  pusch_config_pdu->Tpmi             = 0;
  pusch_config_pdu->rnti             = rnti;

  pusch_dmrs_AdditionalPosition_t add_pos = pusch_dmrs_pos2;
  int dmrslength = 1;
  NR_PUSCH_Config_t *pusch_Config = current_UL_BWP->pusch_Config;

  if (rar_grant) {

    // Note: for Msg3 or MsgA PUSCH transmission the N_PRB_oh is always set to 0
    int ibwp_start = current_UL_BWP->initial_BWPStart;
    int ibwp_size = current_UL_BWP->initial_BWPSize;
    int abwp_start = current_UL_BWP->BWPStart;
    int abwp_size = current_UL_BWP->BWPSize;
    int scs = current_UL_BWP->scs;

      // BWP start selection according to 8.3 of TS 38.213
    if ((ibwp_start < abwp_start) || (ibwp_size > abwp_size)) {
      pusch_config_pdu->bwp_start = abwp_start;
      pusch_config_pdu->bwp_size = abwp_size;
    } else {
      pusch_config_pdu->bwp_start = ibwp_start;
      pusch_config_pdu->bwp_size = ibwp_size;
    }

    //// Resource assignment from RAR
    // Frequency domain allocation according to 8.3 of TS 38.213
    if (ibwp_size < 180)
      mask = (1 << ((int) ceil(log2((ibwp_size*(ibwp_size+1))>>1)))) - 1;
    else
      mask = (1 << (28 - (int)(ceil(log2((ibwp_size*(ibwp_size+1))>>1))))) - 1;

    f_alloc = rar_grant->Msg3_f_alloc & mask;
    if (nr_ue_process_dci_freq_dom_resource_assignment(pusch_config_pdu, NULL, ibwp_size, 0, f_alloc) < 0)
      return -1;

    // virtual resource block to physical resource mapping for Msg3 PUSCH (6.3.1.7 in 38.211)
    //pusch_config_pdu->rb_start += ibwp_start - abwp_start;

    // Time domain allocation
    pusch_config_pdu->start_symbol_index = tda_info->startSymbolIndex;
    pusch_config_pdu->nr_of_symbols = tda_info->nrOfSymbols;

    l_prime_mask =
        get_l_prime(tda_info->nrOfSymbols, tda_info->mapping_type, add_pos, dmrslength, tda_info->startSymbolIndex, mac->scc ? mac->scc->dmrs_TypeA_Position : mac->mib->dmrs_TypeA_Position);
    LOG_D(NR_MAC, "MSG3 start_sym:%d NR Symb:%d mappingtype:%d, DMRS_MASK:%x\n", pusch_config_pdu->start_symbol_index, pusch_config_pdu->nr_of_symbols, tda_info->mapping_type, l_prime_mask);

#ifdef DEBUG_MSG3
    LOG_D(NR_MAC, "In %s BWP assignment (BWP (start %d, size %d) \n", __FUNCTION__, pusch_config_pdu->bwp_start, pusch_config_pdu->bwp_size);
#endif

    // MCS
    pusch_config_pdu->mcs_index = rar_grant->mcs;
    // Frequency hopping
    pusch_config_pdu->frequency_hopping = rar_grant->freq_hopping;

    // DM-RS configuration according to 6.2.2 UE DM-RS transmission procedure in 38.214
    pusch_config_pdu->num_dmrs_cdm_grps_no_data = 2;
    pusch_config_pdu->dmrs_ports = 1;

    // DMRS sequence initialization [TS 38.211, sec 6.4.1.1.1].
    // Should match what is sent in DCI 0_1, otherwise set to 0.
    pusch_config_pdu->scid = 0;

    // Transform precoding according to 6.1.3 UE procedure for applying transform precoding on PUSCH in 38.214
    pusch_config_pdu->transform_precoding = get_transformPrecoding(current_UL_BWP, NR_UL_DCI_FORMAT_0_0, 0); // as if it was DCI 0_0

    // Resource allocation in frequency domain according to 6.1.2.2 in TS 38.214
    pusch_config_pdu->resource_alloc = 1;

    //// Completing PUSCH PDU
    pusch_config_pdu->mcs_table = 0;
    pusch_config_pdu->cyclic_prefix = 0;
    pusch_config_pdu->data_scrambling_id = mac->physCellId;
    pusch_config_pdu->ul_dmrs_scrambling_id = mac->physCellId;
    pusch_config_pdu->subcarrier_spacing = scs;
    pusch_config_pdu->vrb_to_prb_mapping = 0;
    pusch_config_pdu->uplink_frequency_shift_7p5khz = 0;
    //Optional Data only included if indicated in pduBitmap
    pusch_config_pdu->pusch_data.rv_index = 0;  // 8.3 in 38.213
    pusch_config_pdu->pusch_data.harq_process_id = 0;
    pusch_config_pdu->pusch_data.new_data_indicator = 1; // new data
    pusch_config_pdu->pusch_data.num_cb = 0;
    pusch_config_pdu->tbslbrm = 0;

  } else if (dci) {
    pusch_config_pdu->bwp_start = current_UL_BWP->BWPStart;
    pusch_config_pdu->bwp_size = current_UL_BWP->BWPSize;

    // Basic sanity check for MCS value to check for a false or erroneous DCI
    if (dci->mcs > 28) {
      LOG_W(NR_MAC, "MCS value %d out of bounds! Possibly due to false DCI. Ignoring DCI!\n", dci->mcs);
      return -1;
    }

    /* Transform precoding */
    pusch_config_pdu->transform_precoding = get_transformPrecoding(current_UL_BWP, *dci_format, 0);

    /*DCI format-related configuration*/
    int target_ss;
    if (*dci_format == NR_UL_DCI_FORMAT_0_0) {
      target_ss = NR_SearchSpace__searchSpaceType_PR_common;
      if ((pusch_config_pdu->transform_precoding == NR_PUSCH_Config__transformPrecoder_disabled) &&
          pusch_config_pdu->nr_of_symbols < 3)
        pusch_config_pdu->num_dmrs_cdm_grps_no_data = 1;
      else
        pusch_config_pdu->num_dmrs_cdm_grps_no_data = 2;
    } else if (*dci_format == NR_UL_DCI_FORMAT_0_1) {
      target_ss = NR_SearchSpace__searchSpaceType_PR_ue_Specific;
      ul_layers_config(mac, pusch_config_pdu, dci, *dci_format);
      ul_ports_config(mac, &dmrslength, pusch_config_pdu, dci, *dci_format);
    } else {
      LOG_E(NR_MAC, "In %s: UL grant from DCI format %d is not handled...\n", __FUNCTION__, *dci_format);
      return -1;
    }

    int mappingtype = tda_info->mapping_type;

    NR_DMRS_UplinkConfig_t *NR_DMRS_ulconfig = NULL;
    if(pusch_Config) {
      NR_DMRS_ulconfig = (mappingtype == NR_PUSCH_TimeDomainResourceAllocation__mappingType_typeA)
                         ? pusch_Config->dmrs_UplinkForPUSCH_MappingTypeA->choice.setup : pusch_Config->dmrs_UplinkForPUSCH_MappingTypeB->choice.setup;
    }

    pusch_config_pdu->scid = 0;
    pusch_config_pdu->ul_dmrs_scrambling_id = mac->physCellId;
    if(*dci_format == NR_UL_DCI_FORMAT_0_1)
      pusch_config_pdu->scid = dci->dmrs_sequence_initialization.val;

    /* TRANSFORM PRECODING ------------------------------------------------------------------------------------------*/
    if (pusch_config_pdu->transform_precoding == NR_PUSCH_Config__transformPrecoder_enabled) {

      uint32_t n_RS_Id = 0;
      if (NR_DMRS_ulconfig->transformPrecodingEnabled &&
          NR_DMRS_ulconfig->transformPrecodingEnabled->nPUSCH_Identity != NULL)
        n_RS_Id = *NR_DMRS_ulconfig->transformPrecodingEnabled->nPUSCH_Identity;
      else
        n_RS_Id = mac->physCellId;

      // U as specified in section 6.4.1.1.1.2 in 38.211, if sequence hopping and group hopping are disabled
      pusch_config_pdu->dfts_ofdm.low_papr_group_number = n_RS_Id % 30;

      // V as specified in section 6.4.1.1.1.2 in 38.211 V = 0 if sequence hopping and group hopping are disabled
      if (!NR_DMRS_ulconfig || !NR_DMRS_ulconfig->transformPrecodingEnabled ||
          (!NR_DMRS_ulconfig->transformPrecodingEnabled->sequenceGroupHopping && !NR_DMRS_ulconfig->transformPrecodingEnabled->sequenceHopping))
        pusch_config_pdu->dfts_ofdm.low_papr_sequence_number = 0;
      else
        AssertFatal(1==0,"SequenceGroupHopping or sequenceHopping are NOT Supported\n");

      LOG_D(NR_MAC,"TRANSFORM PRECODING IS ENABLED. CDM groups: %d, U: %d \n", pusch_config_pdu->num_dmrs_cdm_grps_no_data,
                pusch_config_pdu->dfts_ofdm.low_papr_group_number);
    }
    else {
      if (pusch_config_pdu->scid == 0 && NR_DMRS_ulconfig &&
          NR_DMRS_ulconfig->transformPrecodingDisabled->scramblingID0)
        pusch_config_pdu->ul_dmrs_scrambling_id = *NR_DMRS_ulconfig->transformPrecodingDisabled->scramblingID0;
      if (pusch_config_pdu->scid == 1 && NR_DMRS_ulconfig &&
          NR_DMRS_ulconfig->transformPrecodingDisabled->scramblingID1)
        pusch_config_pdu->ul_dmrs_scrambling_id = *NR_DMRS_ulconfig->transformPrecodingDisabled->scramblingID1;
    }

    /* TRANSFORM PRECODING --------------------------------------------------------------------------------------------------------*/

    /* IDENTIFIER_DCI_FORMATS */
    /* FREQ_DOM_RESOURCE_ASSIGNMENT_UL */
    if (nr_ue_process_dci_freq_dom_resource_assignment(pusch_config_pdu, NULL, current_UL_BWP->BWPSize, 0, dci->frequency_domain_assignment.val) < 0){
      return -1;
    }

    pusch_config_pdu->start_symbol_index = tda_info->startSymbolIndex;
    pusch_config_pdu->nr_of_symbols = tda_info->nrOfSymbols;

    /* FREQ_HOPPING_FLAG */
    if ((pusch_Config!=NULL) && (pusch_Config->frequencyHopping!=NULL) && (pusch_Config->resourceAllocation != NR_PUSCH_Config__resourceAllocation_resourceAllocationType0)){
      pusch_config_pdu->frequency_hopping = dci->frequency_hopping_flag.val;
    }

    /* MCS */
    pusch_config_pdu->mcs_index = dci->mcs;

    /* MCS TABLE */
    if (pusch_config_pdu->transform_precoding == NR_PUSCH_Config__transformPrecoder_disabled) {
      pusch_config_pdu->mcs_table = get_pusch_mcs_table(pusch_Config ? pusch_Config->mcs_Table : NULL, 0, *dci_format, rnti_type, target_ss, false);
    } else {
      pusch_config_pdu->mcs_table = get_pusch_mcs_table(pusch_Config ? pusch_Config->mcs_TableTransformPrecoder : NULL, 1, *dci_format, rnti_type, target_ss, false);
    }

    /* NDI */
    pusch_config_pdu->pusch_data.new_data_indicator = dci->ndi;
    /* RV */
    pusch_config_pdu->pusch_data.rv_index = dci->rv;
    /* HARQ_PROCESS_NUMBER */
    pusch_config_pdu->pusch_data.harq_process_id = dci->harq_pid;
    /* TPC_PUSCH */
    // according to TS 38.213 Table Table 7.1.1-1
    if (dci->tpc == 0) {
      pusch_config_pdu->absolute_delta_PUSCH = -4;
    }
    if (dci->tpc == 1) {
      pusch_config_pdu->absolute_delta_PUSCH = -1;
    }
    if (dci->tpc == 2) {
      pusch_config_pdu->absolute_delta_PUSCH = 1;
    }
    if (dci->tpc == 3) {
      pusch_config_pdu->absolute_delta_PUSCH = 4;
    }

    if (NR_DMRS_ulconfig != NULL)
      add_pos = (NR_DMRS_ulconfig->dmrs_AdditionalPosition == NULL) ? 2 : *NR_DMRS_ulconfig->dmrs_AdditionalPosition;

    /* DMRS */
    l_prime_mask = get_l_prime(pusch_config_pdu->nr_of_symbols,
                               mappingtype, add_pos, dmrslength,
                               pusch_config_pdu->start_symbol_index,
                               mac->scc ? mac->scc->dmrs_TypeA_Position : mac->mib->dmrs_TypeA_Position);

    // Num PRB Overhead from PUSCH-ServingCellConfig
    if (current_UL_BWP->pusch_servingcellconfig && current_UL_BWP->pusch_servingcellconfig->xOverhead)
      N_PRB_oh = *current_UL_BWP->pusch_servingcellconfig->xOverhead;
    else
      N_PRB_oh = 0;

    if (current_UL_BWP->pusch_servingcellconfig && current_UL_BWP->pusch_servingcellconfig->rateMatching) {
      long *maxMIMO_Layers = current_UL_BWP->pusch_servingcellconfig->ext1->maxMIMO_Layers;
      if (!maxMIMO_Layers)
        maxMIMO_Layers = pusch_Config ? pusch_Config->maxRank : NULL;
      AssertFatal (maxMIMO_Layers != NULL,"Option with max MIMO layers not configured is not supported\n");
      int bw_tbslbrm = get_ulbw_tbslbrm(current_UL_BWP->initial_BWPSize, mac->cg);
      pusch_config_pdu->tbslbrm = nr_compute_tbslbrm(pusch_config_pdu->mcs_table,
                                                     bw_tbslbrm,
                                                     *maxMIMO_Layers);
    } else
      pusch_config_pdu->tbslbrm = 0;

    /* PTRS */
    if (pusch_Config && pusch_Config->dmrs_UplinkForPUSCH_MappingTypeB && pusch_Config->dmrs_UplinkForPUSCH_MappingTypeB->choice.setup->phaseTrackingRS) {
      if (pusch_config_pdu->transform_precoding == NR_PUSCH_Config__transformPrecoder_disabled) {
        nfapi_nr_ue_ptrs_ports_t ptrs_ports_list;
        pusch_config_pdu->pusch_ptrs.ptrs_ports_list = &ptrs_ports_list;
        bool valid_ptrs_setup = set_ul_ptrs_values(pusch_Config->dmrs_UplinkForPUSCH_MappingTypeB->choice.setup->phaseTrackingRS->choice.setup,
                                                   pusch_config_pdu->rb_size,
                                                   pusch_config_pdu->mcs_index,
                                                   pusch_config_pdu->mcs_table,
                                                   &pusch_config_pdu->pusch_ptrs.ptrs_freq_density,
                                                   &pusch_config_pdu->pusch_ptrs.ptrs_time_density,
                                                   &pusch_config_pdu->pusch_ptrs.ptrs_ports_list->ptrs_re_offset,
                                                   &pusch_config_pdu->pusch_ptrs.num_ptrs_ports,
                                                   &pusch_config_pdu->pusch_ptrs.ul_ptrs_power,
                                                   pusch_config_pdu->nr_of_symbols);
        if(valid_ptrs_setup == true) {
          pusch_config_pdu->pdu_bit_map |= PUSCH_PDU_BITMAP_PUSCH_PTRS;
        }
        LOG_D(NR_MAC, "UL PTRS values: PTRS time den: %d, PTRS freq den: %d\n", pusch_config_pdu->pusch_ptrs.ptrs_time_density, pusch_config_pdu->pusch_ptrs.ptrs_freq_density);
      }
    }
  }

  LOG_D(NR_MAC, "In %s: received UL grant (rb_start %d, rb_size %d, start_symbol_index %d, nr_of_symbols %d) for RNTI type %s \n",
    __FUNCTION__,
    pusch_config_pdu->rb_start,
    pusch_config_pdu->rb_size,
    pusch_config_pdu->start_symbol_index,
    pusch_config_pdu->nr_of_symbols,
    rnti_types[rnti_type]);

  pusch_config_pdu->ul_dmrs_symb_pos = l_prime_mask;
  uint16_t R = nr_get_code_rate_ul(pusch_config_pdu->mcs_index, pusch_config_pdu->mcs_table);
  pusch_config_pdu->target_code_rate = R;
  pusch_config_pdu->qam_mod_order = nr_get_Qm_ul(pusch_config_pdu->mcs_index, pusch_config_pdu->mcs_table);

  if (pusch_config_pdu->target_code_rate == 0 || pusch_config_pdu->qam_mod_order == 0) {
    LOG_W(NR_MAC, "In %s: Invalid code rate or Mod order, likely due to unexpected UL DCI. Ignoring DCI! \n", __FUNCTION__);
    return -1;
  }

  int start_symbol = pusch_config_pdu->start_symbol_index;
  int number_of_symbols = pusch_config_pdu->nr_of_symbols;
  for (int i = start_symbol; i < start_symbol + number_of_symbols; i++) {
    if((pusch_config_pdu->ul_dmrs_symb_pos >> i) & 0x01)
      number_dmrs_symbols += 1;
  }

  nb_dmrs_re_per_rb = ((pusch_config_pdu->dmrs_config_type == pusch_dmrs_type1) ? 6:4)*pusch_config_pdu->num_dmrs_cdm_grps_no_data;

  // Compute TBS
  pusch_config_pdu->pusch_data.tb_size = nr_compute_tbs(pusch_config_pdu->qam_mod_order,
                                                        R,
                                                        pusch_config_pdu->rb_size,
                                                        pusch_config_pdu->nr_of_symbols,
                                                        nb_dmrs_re_per_rb*number_dmrs_symbols,
                                                        N_PRB_oh,
                                                        0, // TBR to verify tb scaling
                                                        pusch_config_pdu->nrOfLayers)>>3;
  return 0;

}

void configure_srs_pdu(NR_UE_MAC_INST_t *mac,
                       NR_SRS_Resource_t *srs_resource,
                       fapi_nr_ul_config_srs_pdu *srs_config_pdu,
                       int period, int offset)
{
  NR_UE_UL_BWP_t *current_UL_BWP = &mac->current_UL_BWP;

  srs_config_pdu->rnti = mac->crnti;
  srs_config_pdu->handle = 0;
  srs_config_pdu->bwp_size = current_UL_BWP->BWPSize;
  srs_config_pdu->bwp_start = current_UL_BWP->BWPStart;
  srs_config_pdu->subcarrier_spacing = current_UL_BWP->scs;
  srs_config_pdu->cyclic_prefix = 0;
  srs_config_pdu->num_ant_ports = srs_resource->nrofSRS_Ports;
  srs_config_pdu->num_symbols = srs_resource->resourceMapping.nrofSymbols;
  srs_config_pdu->num_repetitions = srs_resource->resourceMapping.repetitionFactor;
  srs_config_pdu->time_start_position = srs_resource->resourceMapping.startPosition;
  srs_config_pdu->config_index = srs_resource->freqHopping.c_SRS;
  srs_config_pdu->sequence_id = srs_resource->sequenceId;
  srs_config_pdu->bandwidth_index = srs_resource->freqHopping.b_SRS;
  srs_config_pdu->comb_size = srs_resource->transmissionComb.present - 1;

  switch(srs_resource->transmissionComb.present) {
    case NR_SRS_Resource__transmissionComb_PR_n2:
      srs_config_pdu->comb_offset = srs_resource->transmissionComb.choice.n2->combOffset_n2;
      srs_config_pdu->cyclic_shift = srs_resource->transmissionComb.choice.n2->cyclicShift_n2;
      break;
    case NR_SRS_Resource__transmissionComb_PR_n4:
      srs_config_pdu->comb_offset = srs_resource->transmissionComb.choice.n4->combOffset_n4;
      srs_config_pdu->cyclic_shift = srs_resource->transmissionComb.choice.n4->cyclicShift_n4;
      break;
    default:
      LOG_W(NR_MAC, "Invalid or not implemented comb_size!\n");
  }

  srs_config_pdu->frequency_position = srs_resource->freqDomainPosition;
  srs_config_pdu->frequency_shift = srs_resource->freqDomainShift;
  srs_config_pdu->frequency_hopping = srs_resource->freqHopping.b_hop;
  srs_config_pdu->group_or_sequence_hopping = srs_resource->groupOrSequenceHopping;
  srs_config_pdu->resource_type = srs_resource->resourceType.present - 1;
  if(srs_config_pdu->resource_type > 0) { // not aperiodic
    srs_config_pdu->t_srs = period;
    srs_config_pdu->t_offset = offset;
  }

#ifdef SRS_DEBUG
  LOG_I(NR_MAC,"Frame = %i, slot = %i\n", frame, slot);
  LOG_I(NR_MAC,"srs_config_pdu->rnti = 0x%04x\n", srs_config_pdu->rnti);
  LOG_I(NR_MAC,"srs_config_pdu->handle = %u\n", srs_config_pdu->handle);
  LOG_I(NR_MAC,"srs_config_pdu->bwp_size = %u\n", srs_config_pdu->bwp_size);
  LOG_I(NR_MAC,"srs_config_pdu->bwp_start = %u\n", srs_config_pdu->bwp_start);
  LOG_I(NR_MAC,"srs_config_pdu->subcarrier_spacing = %u\n", srs_config_pdu->subcarrier_spacing);
  LOG_I(NR_MAC,"srs_config_pdu->cyclic_prefix = %u (0: Normal; 1: Extended)\n", srs_config_pdu->cyclic_prefix);
  LOG_I(NR_MAC,"srs_config_pdu->num_ant_ports = %u (0 = 1 port, 1 = 2 ports, 2 = 4 ports)\n", srs_config_pdu->num_ant_ports);
  LOG_I(NR_MAC,"srs_config_pdu->num_symbols = %u (0 = 1 symbol, 1 = 2 symbols, 2 = 4 symbols)\n", srs_config_pdu->num_symbols);
  LOG_I(NR_MAC,"srs_config_pdu->num_repetitions = %u (0 = 1, 1 = 2, 2 = 4)\n", srs_config_pdu->num_repetitions);
  LOG_I(NR_MAC,"srs_config_pdu->time_start_position = %u\n", srs_config_pdu->time_start_position);
  LOG_I(NR_MAC,"srs_config_pdu->config_index = %u\n", srs_config_pdu->config_index);
  LOG_I(NR_MAC,"srs_config_pdu->sequence_id = %u\n", srs_config_pdu->sequence_id);
  LOG_I(NR_MAC,"srs_config_pdu->bandwidth_index = %u\n", srs_config_pdu->bandwidth_index);
  LOG_I(NR_MAC,"srs_config_pdu->comb_size = %u (0 = comb size 2, 1 = comb size 4, 2 = comb size 8)\n", srs_config_pdu->comb_size);
  LOG_I(NR_MAC,"srs_config_pdu->comb_offset = %u\n", srs_config_pdu->comb_offset);
  LOG_I(NR_MAC,"srs_config_pdu->cyclic_shift = %u\n", srs_config_pdu->cyclic_shift);
  LOG_I(NR_MAC,"srs_config_pdu->frequency_position = %u\n", srs_config_pdu->frequency_position);
  LOG_I(NR_MAC,"srs_config_pdu->frequency_shift = %u\n", srs_config_pdu->frequency_shift);
  LOG_I(NR_MAC,"srs_config_pdu->frequency_hopping = %u\n", srs_config_pdu->frequency_hopping);
  LOG_I(NR_MAC,"srs_config_pdu->group_or_sequence_hopping = %u (0 = No hopping, 1 = Group hopping groupOrSequenceHopping, 2 = Sequence hopping)\n", srs_config_pdu->group_or_sequence_hopping);
  LOG_I(NR_MAC,"srs_config_pdu->resource_type = %u (0: aperiodic, 1: semi-persistent, 2: periodic)\n", srs_config_pdu->resource_type);
  LOG_I(NR_MAC,"srs_config_pdu->t_srs = %u\n", srs_config_pdu->t_srs);
  LOG_I(NR_MAC,"srs_config_pdu->t_offset = %u\n", srs_config_pdu->t_offset);
#endif
}

// Aperiodic SRS scheduling
void nr_ue_aperiodic_srs_scheduling(NR_UE_MAC_INST_t *mac, long resource_trigger, int frame, int slot)
{
  NR_UE_UL_BWP_t *current_UL_BWP = &mac->current_UL_BWP;
  NR_SRS_Config_t *srs_config = current_UL_BWP->srs_Config;

  if (!srs_config) {
    LOG_E(NR_MAC, "DCI is triggering aperiodic SRS but there is no SRS configuration\n");
    return;
  }

  int slot_offset = 0;
  NR_SRS_Resource_t *srs_resource = NULL;
  for(int rs = 0; rs < srs_config->srs_ResourceSetToAddModList->list.count; rs++) {

    // Find aperiodic resource set
    NR_SRS_ResourceSet_t *srs_resource_set = srs_config->srs_ResourceSetToAddModList->list.array[rs];
    if(srs_resource_set->resourceType.present != NR_SRS_ResourceSet__resourceType_PR_aperiodic)
      continue;
    // the resource trigger need to match the DCI one
    if(srs_resource_set->resourceType.choice.aperiodic->aperiodicSRS_ResourceTrigger != resource_trigger)
      continue;
    // if slotOffset is null -> offset = 0
    if(srs_resource_set->resourceType.choice.aperiodic->slotOffset)
      slot_offset = *srs_resource_set->resourceType.choice.aperiodic->slotOffset;

    // Find the corresponding srs resource
    for(int r1 = 0; r1 < srs_resource_set->srs_ResourceIdList->list.count; r1++) {
      for (int r2 = 0; r2 < srs_config->srs_ResourceToAddModList->list.count; r2++) {
        if ((*srs_resource_set->srs_ResourceIdList->list.array[r1] == srs_config->srs_ResourceToAddModList->list.array[r2]->srs_ResourceId) &&
            (srs_config->srs_ResourceToAddModList->list.array[r2]->resourceType.present == NR_SRS_Resource__resourceType_PR_aperiodic)) {
          srs_resource = srs_config->srs_ResourceToAddModList->list.array[r2];
          break;
        }
      }
    }
  }

  if(srs_resource == NULL) {
    LOG_E(NR_MAC, "Couldn't find SRS aperiodic resource with trigger %ld\n", resource_trigger);
    return;
  }

  AssertFatal(slot_offset > DURATION_RX_TO_TX,
              "Slot offset between DCI and aperiodic SRS (%d) needs to be higher than DURATION_RX_TO_TX (%d)\n",
              slot_offset, DURATION_RX_TO_TX);
  int n_slots_frame = nr_slots_per_frame[current_UL_BWP->scs];
  int sched_slot = (slot + slot_offset) % n_slots_frame;
  NR_TDD_UL_DL_ConfigCommon_t *tdd_config = mac->scc==NULL ? mac->scc_SIB->tdd_UL_DL_ConfigurationCommon : mac->scc->tdd_UL_DL_ConfigurationCommon;
  if (!is_nr_UL_slot(tdd_config, sched_slot, mac->frame_type)) {
    LOG_E(NR_MAC, "Slot for scheduling aperiodic SRS %d is not an UL slot\n", sched_slot);
    return;
  }
  int sched_frame = frame + (slot + slot_offset >= n_slots_frame) % 1024;

  fapi_nr_ul_config_request_t *ul_config = get_ul_config_request(mac, sched_slot, slot_offset);
  fapi_nr_ul_config_srs_pdu *srs_config_pdu = &ul_config->ul_config_list[ul_config->number_pdus].srs_config_pdu;
  configure_srs_pdu(mac, srs_resource, srs_config_pdu, 0, 0);
  fill_ul_config(ul_config, sched_frame, sched_slot, FAPI_NR_UL_CONFIG_TYPE_SRS);
}


// Periodic SRS scheduling
bool nr_ue_periodic_srs_scheduling(module_id_t mod_id, frame_t frame, slot_t slot)
{
  bool srs_scheduled = false;

  NR_UE_MAC_INST_t *mac = get_mac_inst(mod_id);
  NR_UE_UL_BWP_t *current_UL_BWP = &mac->current_UL_BWP;

  NR_SRS_Config_t *srs_config = current_UL_BWP->srs_Config;

  if (!srs_config) {
    return false;
  }

  for(int rs = 0; rs < srs_config->srs_ResourceSetToAddModList->list.count; rs++) {

    // Find periodic resource set
    NR_SRS_ResourceSet_t *srs_resource_set = srs_config->srs_ResourceSetToAddModList->list.array[rs];
    if(srs_resource_set->resourceType.present != NR_SRS_ResourceSet__resourceType_PR_periodic) {
      continue;
    }

    // Find the corresponding srs resource
    NR_SRS_Resource_t *srs_resource = NULL;
    for(int r1 = 0; r1 < srs_resource_set->srs_ResourceIdList->list.count; r1++) {
      for (int r2 = 0; r2 < srs_config->srs_ResourceToAddModList->list.count; r2++) {
        if ((*srs_resource_set->srs_ResourceIdList->list.array[r1] == srs_config->srs_ResourceToAddModList->list.array[r2]->srs_ResourceId) &&
            (srs_config->srs_ResourceToAddModList->list.array[r2]->resourceType.present == NR_SRS_Resource__resourceType_PR_periodic)) {
          srs_resource = srs_config->srs_ResourceToAddModList->list.array[r2];
          break;
        }
      }
    }

    if(srs_resource == NULL) {
      continue;
    }

    uint16_t period = srs_period[srs_resource->resourceType.choice.periodic->periodicityAndOffset_p.present];
    uint16_t offset = get_nr_srs_offset(srs_resource->resourceType.choice.periodic->periodicityAndOffset_p);

    int n_slots_frame = nr_slots_per_frame[current_UL_BWP->scs];

    // Check if UE should transmit the SRS
    if((frame*n_slots_frame+slot-offset)%period == 0) {

      fapi_nr_ul_config_request_t *ul_config = get_ul_config_request(mac, slot, 0);
      fapi_nr_ul_config_srs_pdu *srs_config_pdu = &ul_config->ul_config_list[ul_config->number_pdus].srs_config_pdu;

      configure_srs_pdu(mac, srs_resource, srs_config_pdu, period, offset);

      fill_ul_config(ul_config, frame, slot, FAPI_NR_UL_CONFIG_TYPE_SRS);
      srs_scheduled = true;
    }
  }
  return srs_scheduled;
}

// Performs :
// 1. TODO: Call RRC for link status return to PHY
// 2. TODO: Perform SR/BSR procedures for scheduling feedback
// 3. TODO: Perform PHR procedures
void nr_ue_dl_scheduler(nr_downlink_indication_t *dl_info)
{
  module_id_t mod_id    = dl_info->module_id;
  uint32_t gNB_index    = dl_info->gNB_index;
  int cc_id             = dl_info->cc_id;
  frame_t rx_frame      = dl_info->frame;
  slot_t rx_slot        = dl_info->slot;
  NR_UE_MAC_INST_t *mac = get_mac_inst(mod_id);

  fapi_nr_dl_config_request_t *dl_config = get_dl_config_request(mac, rx_slot);
  dl_config->sfn  = rx_frame;
  dl_config->slot = rx_slot;

  nr_scheduled_response_t scheduled_response;
  nr_dcireq_t dcireq;

  if(mac->state > UE_NOT_SYNC) {

    dcireq.module_id = mod_id;
    dcireq.gNB_index = gNB_index;
    dcireq.cc_id     = cc_id;
    dcireq.frame     = rx_frame;
    dcireq.slot      = rx_slot;
    dcireq.dl_config_req.number_pdus = 0;
    nr_ue_dcireq(&dcireq); //to be replaced with function pointer later
    *dl_config = dcireq.dl_config_req;

    if(mac->ul_time_alignment.ta_apply)
      schedule_ta_command(dl_config, &mac->ul_time_alignment);
    if(mac->state == UE_CONNECTED) {
      nr_schedule_csirs_reception(mac, rx_frame, rx_slot);
      nr_schedule_csi_for_im(mac, rx_frame, rx_slot);
    }
    dcireq.dl_config_req = *dl_config;

    fill_scheduled_response(&scheduled_response, &dcireq.dl_config_req, NULL, NULL, NULL, NULL, mod_id, cc_id, rx_frame, rx_slot, dl_info->phy_data);
    if(mac->if_module != NULL && mac->if_module->scheduled_response != NULL) {
      LOG_D(NR_MAC,"1# scheduled_response transmitted, %d, %d\n", rx_frame, rx_slot);
      mac->if_module->scheduled_response(&scheduled_response);
    }
  }
  else
    dl_config->number_pdus = 0;
}

void nr_ue_ul_scheduler(nr_uplink_indication_t *ul_info)
{
  int cc_id             = ul_info->cc_id;
  frame_t frame_tx      = ul_info->frame_tx;
  slot_t slot_tx        = ul_info->slot_tx;
  module_id_t mod_id    = ul_info->module_id;
  uint32_t gNB_index    = ul_info->gNB_index;

  NR_UE_MAC_INST_t *mac = get_mac_inst(mod_id);
  RA_config_t *ra       = &mac->ra;

  fapi_nr_ul_config_request_t *ul_config = get_ul_config_request(mac, slot_tx, 0);
  if (!ul_config)
    LOG_E(NR_MAC, "mac->ul_config is null!\n");

  if(mac->state < UE_CONNECTED) {
    nr_ue_get_rach(mod_id, cc_id, frame_tx, gNB_index, slot_tx);
    nr_ue_prach_scheduler(mod_id, frame_tx, slot_tx);
  }

  // Periodic SRS scheduling
  if(mac->state == UE_CONNECTED)
    nr_ue_periodic_srs_scheduling(mod_id, frame_tx, slot_tx);

  // Schedule ULSCH only if the current frame and slot match those in ul_config_req
  // AND if a UL grant (UL DCI or Msg3) has been received (as indicated by num_pdus)
  if (ul_config) {
    pthread_mutex_lock(&ul_config->mutex_ul_config);
    if ((ul_info->slot_tx == ul_config->slot && ul_info->frame_tx == ul_config->sfn) && ul_config->number_pdus > 0){

      LOG_D(NR_MAC, "[%d.%d]: number of UL PDUs: %d with UL transmission in [%d.%d]\n", frame_tx, slot_tx, ul_config->number_pdus, ul_config->sfn, ul_config->slot);

      uint8_t ulsch_input_buffer_array[NFAPI_MAX_NUM_UL_PDU][MAX_ULSCH_PAYLOAD_BYTES];
      nr_scheduled_response_t scheduled_response;
      fapi_nr_tx_request_t tx_req;
      tx_req.slot = slot_tx;
      tx_req.sfn = frame_tx;
      tx_req.number_of_pdus = 0;

      for (int j = 0; j < ul_config->number_pdus; j++) {
        uint8_t *ulsch_input_buffer = ulsch_input_buffer_array[tx_req.number_of_pdus];

        fapi_nr_ul_config_request_pdu_t *ulcfg_pdu = &ul_config->ul_config_list[j];

        if (ulcfg_pdu->pdu_type == FAPI_NR_UL_CONFIG_TYPE_PUSCH) {
          int mac_pdu_exist = 0;
          uint16_t TBS_bytes = ulcfg_pdu->pusch_config_pdu.pusch_data.tb_size;
          LOG_D(NR_MAC,"harq_id %d, NDI %d NDI_DCI %d, TBS_bytes %d (ra_state %d)\n",
                ulcfg_pdu->pusch_config_pdu.pusch_data.harq_process_id,
                mac->UL_ndi[ulcfg_pdu->pusch_config_pdu.pusch_data.harq_process_id],
                ulcfg_pdu->pusch_config_pdu.pusch_data.new_data_indicator,
                TBS_bytes,ra->ra_state);
          if (ra->ra_state == WAIT_RAR && !ra->cfra){
            memcpy(ulsch_input_buffer, mac->ulsch_pdu.payload, TBS_bytes);
            LOG_D(NR_MAC,"[RAPROC] Msg3 to be transmitted:\n");
            for (int k = 0; k < TBS_bytes; k++) {
              LOG_D(NR_MAC,"(%i): 0x%x\n",k,mac->ulsch_pdu.payload[k]);
            }
            LOG_D(NR_MAC,"Flipping NDI for harq_id %d (Msg3)\n",ulcfg_pdu->pusch_config_pdu.pusch_data.new_data_indicator);
            mac->UL_ndi[ulcfg_pdu->pusch_config_pdu.pusch_data.harq_process_id] = ulcfg_pdu->pusch_config_pdu.pusch_data.new_data_indicator;
            mac->first_ul_tx[ulcfg_pdu->pusch_config_pdu.pusch_data.harq_process_id] = 0;
            mac_pdu_exist = 1;
          } else {

            if ((mac->UL_ndi[ulcfg_pdu->pusch_config_pdu.pusch_data.harq_process_id] != ulcfg_pdu->pusch_config_pdu.pusch_data.new_data_indicator ||
                mac->first_ul_tx[ulcfg_pdu->pusch_config_pdu.pusch_data.harq_process_id] == 1) &&
                (mac->state == UE_CONNECTED ||
                (ra->ra_state == WAIT_RAR && ra->cfra))){

              // Getting IP traffic to be transmitted
              nr_ue_get_sdu(mod_id, cc_id,frame_tx, slot_tx, gNB_index, ulsch_input_buffer, TBS_bytes);
              mac_pdu_exist = 1;
            }

            LOG_D(NR_MAC,"Flipping NDI for harq_id %d\n",ulcfg_pdu->pusch_config_pdu.pusch_data.new_data_indicator);
            mac->UL_ndi[ulcfg_pdu->pusch_config_pdu.pusch_data.harq_process_id] = ulcfg_pdu->pusch_config_pdu.pusch_data.new_data_indicator;
            mac->first_ul_tx[ulcfg_pdu->pusch_config_pdu.pusch_data.harq_process_id] = 0;

          }

          // Config UL TX PDU
          if (mac_pdu_exist) {
            tx_req.tx_request_body[tx_req.number_of_pdus].pdu_length = TBS_bytes;
            tx_req.tx_request_body[tx_req.number_of_pdus].pdu_index = j;
            tx_req.tx_request_body[tx_req.number_of_pdus].pdu = ulsch_input_buffer;
            tx_req.number_of_pdus++;
          }
          if (ra->ra_state == WAIT_CONTENTION_RESOLUTION && !ra->cfra){
            LOG_I(NR_MAC,"[RAPROC][%d.%d] RA-Msg3 retransmitted\n", frame_tx, slot_tx);
            // 38.321 restart the ra-ContentionResolutionTimer at each HARQ retransmission in the first symbol after the end of the Msg3 transmission
            nr_Msg3_transmitted(ul_info->module_id, ul_info->cc_id, ul_info->frame_tx, ul_info->slot_tx, ul_info->gNB_index);
          }
          if (ra->ra_state == WAIT_RAR && !ra->cfra){
            LOG_A(NR_MAC, "[RAPROC][%d.%d] RA-Msg3 transmitted\n", frame_tx, slot_tx);
            nr_Msg3_transmitted(ul_info->module_id, ul_info->cc_id, ul_info->frame_tx, ul_info->slot_tx, ul_info->gNB_index);
          }
        }
      }
      pthread_mutex_unlock(&ul_config->mutex_ul_config); // avoid double lock
      fill_scheduled_response(&scheduled_response, NULL, ul_config, &tx_req, NULL,NULL,mod_id, cc_id, frame_tx, slot_tx, ul_info->phy_data);
      if(mac->if_module != NULL && mac->if_module->scheduled_response != NULL){
        LOG_D(NR_MAC,"3# scheduled_response transmitted,%d, %d\n", frame_tx, slot_tx);
        mac->if_module->scheduled_response(&scheduled_response);
      }
      pthread_mutex_lock(&ul_config->mutex_ul_config);
    }
    pthread_mutex_unlock(&ul_config->mutex_ul_config);
  }

  // Call BSR procedure as described in Section 5.4.5 in 38.321

  // First check ReTxBSR Timer because it is always configured
  // Decrement ReTxBSR Timer if it is running and not null
  if ((mac->scheduling_info.retxBSR_SF != MAC_UE_BSR_TIMER_NOT_RUNNING) && (mac->scheduling_info.retxBSR_SF != 0)) {
    mac->scheduling_info.retxBSR_SF--;
  }

  // Decrement Periodic Timer if it is running and not null
  if ((mac->scheduling_info.periodicBSR_SF != MAC_UE_BSR_TIMER_NOT_RUNNING) && (mac->scheduling_info.periodicBSR_SF != 0)) {
    mac->scheduling_info.periodicBSR_SF--;
  }

  //Check whether Regular BSR is triggered
  if (nr_update_bsr(mod_id, frame_tx, slot_tx, gNB_index) == true) {
    // call SR procedure to generate pending SR and BSR for next PUCCH/PUSCH TxOp.  This should implement the procedures
    // outlined in Sections 5.4.4 an 5.4.5 of 38.321
    mac->scheduling_info.SR_pending = 1;
    // Regular BSR trigger
    mac->BSR_reporting_active |= NR_BSR_TRIGGER_REGULAR;
    LOG_D(NR_MAC, "[UE %d][BSR] Regular BSR Triggered Frame %d slot %d SR for PUSCH is pending\n",
          mod_id, frame_tx, slot_tx);
  }

  if(mac->state >= UE_PERFORMING_RA)
    nr_ue_pucch_scheduler(mod_id,frame_tx, slot_tx, ul_info->phy_data);
}

bool nr_update_bsr(module_id_t module_idP, frame_t frameP, slot_t slotP, uint8_t gNB_index)
{
  bool bsr_regular_triggered = false;
  uint8_t lcid;
  uint8_t lcgid;
  uint8_t num_lcid_with_data = 0; // for LCID with data only if LCGID is defined
  uint32_t lcgid_buffer_remain[NR_MAX_NUM_LCGID] = {0,0,0,0,0,0,0,0};
  int32_t lcid_bytes_in_buffer[NR_MAX_NUM_LCID];
  /* Array for ordering LCID with data per decreasing priority order */
  uint8_t lcid_reordered_array[NR_MAX_NUM_LCID]=
  {NR_MAX_NUM_LCID,NR_MAX_NUM_LCID,NR_MAX_NUM_LCID,NR_MAX_NUM_LCID,NR_MAX_NUM_LCID,NR_MAX_NUM_LCID,NR_MAX_NUM_LCID,NR_MAX_NUM_LCID,
   NR_MAX_NUM_LCID,NR_MAX_NUM_LCID,NR_MAX_NUM_LCID,NR_MAX_NUM_LCID,NR_MAX_NUM_LCID,NR_MAX_NUM_LCID,NR_MAX_NUM_LCID,NR_MAX_NUM_LCID,
   NR_MAX_NUM_LCID,NR_MAX_NUM_LCID,NR_MAX_NUM_LCID,NR_MAX_NUM_LCID,NR_MAX_NUM_LCID,NR_MAX_NUM_LCID,NR_MAX_NUM_LCID,NR_MAX_NUM_LCID,
   NR_MAX_NUM_LCID,NR_MAX_NUM_LCID,NR_MAX_NUM_LCID,NR_MAX_NUM_LCID,NR_MAX_NUM_LCID,NR_MAX_NUM_LCID,NR_MAX_NUM_LCID,NR_MAX_NUM_LCID,
  };
  uint8_t pos_next = 0;
  //uint8_t highest_priority = 16;
  uint8_t array_index = 0;
  // Reset All BSR Infos
  lcid_bytes_in_buffer[0] = 0;
  NR_UE_MAC_INST_t *mac = get_mac_inst(module_idP);
  for (lcid=DCCH; lcid < NR_MAX_NUM_LCID; lcid++) {
    // Reset transmission status
    lcid_bytes_in_buffer[lcid] = 0;
    mac->scheduling_info.LCID_status[lcid]=LCID_EMPTY;
  }

  for (lcgid=0; lcgid < NR_MAX_NUM_LCGID; lcgid++) {
    // Reset Buffer Info
    mac->scheduling_info.BSR[lcgid]=0;
    mac->scheduling_info.BSR_bytes[lcgid]=0;
  }

  //Get Buffer Occupancy and fill lcid_reordered_array
  for (lcid=DCCH; lcid < NR_MAX_NUM_LCID; lcid++) {
    //if (mac->logicalChannelConfig[lcid]) {
    if (mac->logicalChannelBearer_exist[lcid] ) { // todo
      lcgid = mac->scheduling_info.LCGID[lcid];

      // Store already available data to transmit per Group
      if (lcgid < NR_MAX_NUM_LCGID) {
        lcgid_buffer_remain[lcgid] += mac->scheduling_info.LCID_buffer_remain[lcid];
      }

      mac_rlc_status_resp_t rlc_status = mac_rlc_status_ind(module_idP, mac->crnti,gNB_index,frameP,slotP,ENB_FLAG_NO,MBMS_FLAG_NO, lcid, 0, 0);

      lcid_bytes_in_buffer[lcid] = rlc_status.bytes_in_buffer;

      if (rlc_status.bytes_in_buffer > 0) {
        LOG_D(NR_MAC,"[UE %d] PDCCH Tick : LCID%d LCGID%d has data to transmit =%d bytes at frame %d slot %d\n",
              module_idP, lcid,lcgid,rlc_status.bytes_in_buffer,frameP,slotP);
        mac->scheduling_info.LCID_status[lcid] = LCID_NOT_EMPTY;

        //Update BSR_bytes and position in lcid_reordered_array only if Group is defined
        if (lcgid < NR_MAX_NUM_LCGID) {
          num_lcid_with_data ++;
          // sum lcid buffer which has same lcgid
          mac->scheduling_info.BSR_bytes[lcgid] += rlc_status.bytes_in_buffer;
          //Fill in the array
          array_index = 0;

          do {
            //if (mac->logicalChannelConfig[lcid]->ul_SpecificParameters->priority <= highest_priority) {
            if (1) { // todo
              //Insert if priority is higher or equal (lower or equal in value)
              for (pos_next=num_lcid_with_data-1; pos_next > array_index; pos_next--) {
                lcid_reordered_array[pos_next] = lcid_reordered_array[pos_next - 1];
              }

              lcid_reordered_array[array_index] = lcid;
              break;
            }

            array_index ++;
          } while ((array_index < num_lcid_with_data) && (array_index < NR_MAX_NUM_LCID));
        }
      }
    }
  }

  // Check whether a regular BSR can be triggered according to the first cases in 38.321
  if (num_lcid_with_data) {
    LOG_D(NR_MAC, "[UE %d] PDCCH Tick at frame %d slot %d: NumLCID with data=%d Reordered LCID0=%d LCID1=%d LCID2=%d\n",
          module_idP, frameP, slotP, num_lcid_with_data,
          lcid_reordered_array[0], lcid_reordered_array[1],
          lcid_reordered_array[2]);

    for (array_index = 0; array_index < num_lcid_with_data; array_index++) {
      lcid = lcid_reordered_array[array_index];

      /* UL data, for a logical channel which belongs to a LCG, becomes available for transmission in the RLC entity
         either the data belongs to a logical channel with higher priority than the priorities of the logical channels
         which belong to any LCG and for which data is already available for transmission
       */
      {
        bsr_regular_triggered = true;
        LOG_D(NR_MAC, "[UE %d] PDCCH Tick : MAC BSR Triggered LCID%d LCGID%d data become available at frame %d slot %d\n",
              module_idP, lcid,
              mac->scheduling_info.LCGID[lcid],
              frameP, slotP);
        break;
      }
    }

    // Trigger Regular BSR if ReTxBSR Timer has expired and UE has data for transmission
    if (mac->scheduling_info.retxBSR_SF == 0) {
      bsr_regular_triggered = true;

      if ((mac->BSR_reporting_active & NR_BSR_TRIGGER_REGULAR) == 0) {
        LOG_I(NR_MAC, "[UE %d] PDCCH Tick : MAC BSR Triggered ReTxBSR Timer expiry at frame %d slot %d\n",
              module_idP, frameP, slotP);
      }
    }
  }

  //Store Buffer Occupancy in remain buffers for next TTI
  for (lcid = DCCH; lcid < NR_MAX_NUM_LCID; lcid++) {
    mac->scheduling_info.LCID_buffer_remain[lcid] = lcid_bytes_in_buffer[lcid];
  }

  return bsr_regular_triggered;
}

uint8_t
nr_locate_BsrIndexByBufferSize(const uint32_t *table, int size, int value) {
  uint8_t ju, jm, jl;
  int ascend;
  //DevAssert(size > 0);
  //DevAssert(size <= 256);

  if (value == 0) {
    return 0;   //elseif (value > 150000) return 63;
  }

  jl = 0;     // lower bound
  ju = size - 1;    // upper bound
  ascend = (table[ju] >= table[jl]) ? 1 : 0;  // determine the order of the the table:  1 if ascending order of table, 0 otherwise

  while (ju - jl > 1) { //If we are not yet done,
    jm = (ju + jl) >> 1;  //compute a midpoint,

    if ((value >= table[jm]) == ascend) {
      jl = jm;    // replace the lower limit
    } else {
      ju = jm;    //replace the upper limit
    }

    LOG_T(NR_MAC, "[UE] searching BSR index %d for (BSR TABLE %d < value %d)\n",
          jm, table[jm], value);
  }

  if (value == table[jl]) {
    return jl;
  } else {
    return jl + 1;    //equally  ju
  }
}

int nr_get_sf_periodicBSRTimer(uint8_t sf_offset) {
  switch (sf_offset) {
    case NR_BSR_Config__periodicBSR_Timer_sf1:
      return 1;
      break;

    case NR_BSR_Config__periodicBSR_Timer_sf5:
      return 5;
      break;

    case NR_BSR_Config__periodicBSR_Timer_sf10:
      return 10;
      break;

    case NR_BSR_Config__periodicBSR_Timer_sf16:
      return 16;
      break;

    case NR_BSR_Config__periodicBSR_Timer_sf20:
      return 20;
      break;

    case NR_BSR_Config__periodicBSR_Timer_sf32:
      return 32;
      break;

    case NR_BSR_Config__periodicBSR_Timer_sf40:
      return 40;
      break;

    case NR_BSR_Config__periodicBSR_Timer_sf64:
      return 64;
      break;

    case NR_BSR_Config__periodicBSR_Timer_sf80:
      return 80;
      break;

    case NR_BSR_Config__periodicBSR_Timer_sf128:
      return 128;
      break;

    case NR_BSR_Config__periodicBSR_Timer_sf160:
      return 160;
      break;

    case NR_BSR_Config__periodicBSR_Timer_sf320:
      return 320;
      break;

    case NR_BSR_Config__periodicBSR_Timer_sf640:
      return 640;
      break;

    case NR_BSR_Config__periodicBSR_Timer_sf1280:
      return 1280;
      break;

    case NR_BSR_Config__periodicBSR_Timer_sf2560:
      return 2560;
      break;

    case NR_BSR_Config__periodicBSR_Timer_infinity:
    default:
      return 0xFFFF;
      break;
  }
}

int nr_get_sf_retxBSRTimer(uint8_t sf_offset) {
  switch (sf_offset) {
    case NR_BSR_Config__retxBSR_Timer_sf10:
      return 10;
      break;

    case NR_BSR_Config__retxBSR_Timer_sf20:
      return 20;
      break;

    case NR_BSR_Config__retxBSR_Timer_sf40:
      return 40;
      break;

    case NR_BSR_Config__retxBSR_Timer_sf80:
      return 80;
      break;

    case NR_BSR_Config__retxBSR_Timer_sf160:
      return 160;
      break;

    case NR_BSR_Config__retxBSR_Timer_sf320:
      return 320;
      break;

    case NR_BSR_Config__retxBSR_Timer_sf640:
      return 640;
      break;

    case NR_BSR_Config__retxBSR_Timer_sf1280:
      return 1280;
      break;

    case NR_BSR_Config__retxBSR_Timer_sf2560:
      return 2560;
      break;

    case NR_BSR_Config__retxBSR_Timer_sf5120:
      return 5120;
      break;

    case NR_BSR_Config__retxBSR_Timer_sf10240:
      return 10240;
      break;

    default:
      return -1;
      break;
  }
}

// PUSCH scheduler:
// - Calculate the slot in which ULSCH should be scheduled. This is current slot + K2,
// - where K2 is the offset between the slot in which UL DCI is received and the slot
// - in which ULSCH should be scheduled. K2 is configured in RRC configuration.
// PUSCH Msg3 scheduler:
// - scheduled by RAR UL grant according to 8.3 of TS 38.213
// Note: Msg3 tx in the uplink symbols of mixed slot
int nr_ue_pusch_scheduler(NR_UE_MAC_INST_t *mac, uint8_t is_Msg3, frame_t current_frame, int current_slot, frame_t *frame_tx, int *slot_tx, long k2)
{
  AssertFatal(k2 > DURATION_RX_TO_TX,
              "Slot offset K2 (%ld) needs to be higher than DURATION_RX_TO_TX (%d). Please set min_rxtxtime at least to %d in gNB config file or gNBs.[0].min_rxtxtime=%d via command line.\n",
              k2,
              DURATION_RX_TO_TX,
              DURATION_RX_TO_TX,
              DURATION_RX_TO_TX);

  int delta = 0;
  NR_UE_UL_BWP_t *current_UL_BWP = &mac->current_UL_BWP;

  // Get the numerology to calculate the Tx frame and slot
  int mu = current_UL_BWP->scs;

  // k2 as per 3GPP TS 38.214 version 15.9.0 Release 15 ch 6.1.2.1.1
  // PUSCH time domain resource allocation is higher layer configured from uschTimeDomainAllocationList in either pusch-ConfigCommon

  if (is_Msg3) {

    switch (mu) {
      case 0:
        delta = 2;
        break;
      case 1:
        delta = 3;
        break;
      case 2:
        delta = 4;
        break;
      case 3:
        delta = 6;
        break;
      default:
        AssertFatal(1 == 0, "Invalid numerology %i\n", mu);
    }

    AssertFatal((k2 + delta) > DURATION_RX_TO_TX,
                "Slot offset (%ld) for Msg3 needs to be higher than DURATION_RX_TO_TX (%d). Please set min_rxtxtime at least to %d in gNB config file or gNBs.[0].min_rxtxtime=%d via command line.\n",
                k2,
                DURATION_RX_TO_TX,
                DURATION_RX_TO_TX,
                DURATION_RX_TO_TX);

    *slot_tx = (current_slot + k2 + delta) % nr_slots_per_frame[mu];
    if (current_slot + k2 + delta >= nr_slots_per_frame[mu]){
      *frame_tx = (current_frame + 1) % 1024;
    } else {
      *frame_tx = current_frame;
    }

  } else {

    if (k2 < 0) { // This can happen when a false DCI is received
      LOG_W(PHY, "%d.%d. Received k2 %ld\n", current_frame, current_slot, k2);
      return -1;
    }

    // Calculate TX slot and frame
    *slot_tx = (current_slot + k2) % nr_slots_per_frame[mu];
    *frame_tx = ((current_slot + k2) > (nr_slots_per_frame[mu]-1)) ? (current_frame + 1) % 1024 : current_frame;

  }

  LOG_D(NR_MAC, "[%04d.%02d] UL transmission in [%04d.%02d] (k2 %ld delta %d)\n", current_frame, current_slot, *frame_tx, *slot_tx, k2, delta);

  return 0;
}

// Build the list of all the valid RACH occasions in the maximum association pattern period according to the PRACH config
static void build_ro_list(NR_UE_MAC_INST_t *mac) {

  int x,y; // PRACH Configuration Index table variables used to compute the valid frame numbers
  int y2;  // PRACH Configuration Index table additional variable used to compute the valid frame numbers
  uint8_t slot_shift_for_map;
  uint8_t map_shift;
  bool even_slot_invalid;
  int64_t s_map;
  uint8_t prach_conf_start_symbol; // Starting symbol of the PRACH occasions in the PRACH slot
  uint8_t N_t_slot; // Number of PRACH occasions in a 14-symbols PRACH slot
  uint8_t N_dur; // Duration of a PRACH occasion (nb of symbols)
  uint8_t frame; // Maximum is NB_FRAMES_IN_MAX_ASSOCIATION_PATTERN_PERIOD
  uint8_t slot; // Maximum is the number of slots in a frame @ SCS 240kHz
  uint16_t format = 0xffff;
  uint8_t format2 = 0xff;
  int nb_fdm;

  uint8_t config_index;
  int msg1_FDM;

  uint8_t prach_conf_period_idx;
  uint8_t nb_of_frames_per_prach_conf_period;
  uint8_t prach_conf_period_frame_idx;

  NR_RACH_ConfigCommon_t *setup = mac->current_UL_BWP.rach_ConfigCommon;
  NR_RACH_ConfigGeneric_t *rach_ConfigGeneric = &setup->rach_ConfigGeneric;

  config_index = rach_ConfigGeneric->prach_ConfigurationIndex;

  int mu;
  if (setup->msg1_SubcarrierSpacing)
    mu = *setup->msg1_SubcarrierSpacing;
  else
    mu = mac->current_UL_BWP.scs;

  msg1_FDM = rach_ConfigGeneric->msg1_FDM;

  switch (msg1_FDM){
    case 0:
    case 1:
    case 2:
    case 3:
      nb_fdm = 1 << msg1_FDM;
      break;
    default:
      AssertFatal(1 == 0, "Unknown msg1_FDM from rach_ConfigGeneric %d\n", msg1_FDM);
  }

  // Create the PRACH occasions map
  // ==============================
  // WIP: For now assume no rejected PRACH occasions because of conflict with SSB or TDD_UL_DL_ConfigurationCommon schedule

  int unpaired = mac->phy_config.config_req.cell_config.frame_duplex_type;

  const int64_t *prach_config_info_p = get_prach_config_info(mac->frequency_range, config_index, unpaired);

  // Identify the proper PRACH Configuration Index table according to the operating frequency
  LOG_D(NR_MAC,"mu = %u, PRACH config index  = %u, unpaired = %u\n", mu, config_index, unpaired);

  if (mac->frequency_range == FR2) { //FR2

    x = prach_config_info_p[2];
    y = prach_config_info_p[3];
    y2 = prach_config_info_p[4];

    s_map = prach_config_info_p[5];

    prach_conf_start_symbol = prach_config_info_p[6];
    N_t_slot = prach_config_info_p[8];
    N_dur = prach_config_info_p[9];
    if (prach_config_info_p[1] != -1)
      format2 = (uint8_t) prach_config_info_p[1];
    format = ((uint8_t) prach_config_info_p[0]) | (format2<<8);

    slot_shift_for_map = mu-2;
    if ( (mu == 3) && (prach_config_info_p[7] == 1) )
      even_slot_invalid = true;
    else
      even_slot_invalid = false;
  }
  else { // FR1
    x = prach_config_info_p[2];
    y = prach_config_info_p[3];
    y2 = y;

    s_map = prach_config_info_p[4];

    prach_conf_start_symbol = prach_config_info_p[5];
    N_t_slot = prach_config_info_p[7];
    N_dur = prach_config_info_p[8];
    LOG_D(NR_MAC,"N_t_slot %d, N_dur %d\n",N_t_slot,N_dur);
    if (prach_config_info_p[1] != -1)
      format2 = (uint8_t) prach_config_info_p[1];
    format = ((uint8_t) prach_config_info_p[0]) | (format2<<8);

    slot_shift_for_map = mu;
    if ( (mu == 1) && (prach_config_info_p[6] <= 1) )
      // no prach in even slots @ 30kHz for 1 prach per subframe
      even_slot_invalid = true;
    else
      even_slot_invalid = false;
  } // FR2 / FR1

  prach_assoc_pattern.nb_of_prach_conf_period_in_max_period = MAX_NB_PRACH_CONF_PERIOD_IN_ASSOCIATION_PATTERN_PERIOD / x;
  nb_of_frames_per_prach_conf_period = x;

  LOG_D(NR_MAC,"nb_of_prach_conf_period_in_max_period %d\n", prach_assoc_pattern.nb_of_prach_conf_period_in_max_period);

  // Fill in the PRACH occasions table for every slot in every frame in every PRACH configuration periods in the maximum association pattern period
  // ----------------------------------------------------------------------------------------------------------------------------------------------
  // ----------------------------------------------------------------------------------------------------------------------------------------------
  // For every PRACH configuration periods
  // -------------------------------------
  for (prach_conf_period_idx=0; prach_conf_period_idx<prach_assoc_pattern.nb_of_prach_conf_period_in_max_period; prach_conf_period_idx++) {
    prach_assoc_pattern.prach_conf_period_list[prach_conf_period_idx].nb_of_prach_occasion = 0;
    prach_assoc_pattern.prach_conf_period_list[prach_conf_period_idx].nb_of_frame = nb_of_frames_per_prach_conf_period;
    prach_assoc_pattern.prach_conf_period_list[prach_conf_period_idx].nb_of_slot = nr_slots_per_frame[mu];

    LOG_D(NR_MAC,"PRACH Conf Period Idx %d\n", prach_conf_period_idx);

    // For every frames in a PRACH configuration period
    // ------------------------------------------------
    for (prach_conf_period_frame_idx=0; prach_conf_period_frame_idx<nb_of_frames_per_prach_conf_period; prach_conf_period_frame_idx++) {
      frame = (prach_conf_period_idx * nb_of_frames_per_prach_conf_period) + prach_conf_period_frame_idx;

      LOG_D(NR_MAC,"PRACH Conf Period Frame Idx %d - Frame %d\n", prach_conf_period_frame_idx, frame);
      // Is it a valid frame for this PRACH configuration index? (n_sfn mod x = y)
      if ( (frame%x)==y || (frame%x)==y2 ) {

        // For every slot in a frame
        // -------------------------
        for (slot=0; slot<nr_slots_per_frame[mu]; slot++) {
          // Is it a valid slot?
          map_shift = slot >> slot_shift_for_map; // in PRACH configuration index table slots are numbered wrt 60kHz
          if ( (s_map>>map_shift)&0x01 ) {
            // Valid slot

            // Additionally, for 30kHz/120kHz, we must check for the n_RA_Slot param also
            if ( even_slot_invalid && (slot%2 == 0) )
                continue; // no prach in even slots @ 30kHz/120kHz for 1 prach per 60khz slot/subframe

            // We're good: valid frame and valid slot
            // Compute all the PRACH occasions in the slot

            uint8_t n_prach_occ_in_time;
            uint8_t n_prach_occ_in_freq;

            prach_assoc_pattern.prach_conf_period_list[prach_conf_period_idx].prach_occasion_slot_map[prach_conf_period_frame_idx][slot].nb_of_prach_occasion_in_time = N_t_slot;
            prach_assoc_pattern.prach_conf_period_list[prach_conf_period_idx].prach_occasion_slot_map[prach_conf_period_frame_idx][slot].nb_of_prach_occasion_in_freq = nb_fdm;

            for (n_prach_occ_in_time=0; n_prach_occ_in_time<N_t_slot; n_prach_occ_in_time++) {
              uint8_t start_symbol = prach_conf_start_symbol + n_prach_occ_in_time * N_dur;
              LOG_D(NR_MAC,"PRACH Occ in time %d\n", n_prach_occ_in_time);

              for (n_prach_occ_in_freq=0; n_prach_occ_in_freq<nb_fdm; n_prach_occ_in_freq++) {
                prach_occasion_info_t *prach_occasion_p = &prach_assoc_pattern.prach_conf_period_list[prach_conf_period_idx].prach_occasion_slot_map[prach_conf_period_frame_idx][slot].prach_occasion[n_prach_occ_in_time][n_prach_occ_in_freq];

                prach_occasion_p->start_symbol = start_symbol;
                prach_occasion_p->fdm = n_prach_occ_in_freq;
                prach_occasion_p->frame = frame;
                prach_occasion_p->slot = slot;
                prach_occasion_p->format = format;
                prach_assoc_pattern.prach_conf_period_list[prach_conf_period_idx].nb_of_prach_occasion++;

                LOG_D(NR_MAC,"Adding a PRACH occasion: frame %u, slot-symbol %d-%d, occ_in_time-occ_in-freq %d-%d, nb ROs in conf period %d, for this slot: RO# in time %d, RO# in freq %d\n",
                    frame, slot, start_symbol, n_prach_occ_in_time, n_prach_occ_in_freq, prach_assoc_pattern.prach_conf_period_list[prach_conf_period_idx].nb_of_prach_occasion,
                    prach_assoc_pattern.prach_conf_period_list[prach_conf_period_idx].prach_occasion_slot_map[prach_conf_period_frame_idx][slot].nb_of_prach_occasion_in_time,
                    prach_assoc_pattern.prach_conf_period_list[prach_conf_period_idx].prach_occasion_slot_map[prach_conf_period_frame_idx][slot].nb_of_prach_occasion_in_freq);
              } // For every freq in the slot
            } // For every time occasions in the slot
          } // Valid slot?
        } // For every slots in a frame
      } // Valid frame?
    } // For every frames in a prach configuration period
  } // For every prach configuration periods in the maximum association pattern period (160ms)
}

// Build the list of all the valid/transmitted SSBs according to the config
static void build_ssb_list(NR_UE_MAC_INST_t *mac) {

  // Create the list of transmitted SSBs
  // ===================================
  BIT_STRING_t *ssb_bitmap;
  uint64_t ssb_positionsInBurst;
  uint8_t ssb_idx = 0;
  ssb_list_info_t *ssb_list = &mac->ssb_list;

  if (mac->scc) {
    NR_ServingCellConfigCommon_t *scc = mac->scc;
    switch (scc->ssb_PositionsInBurst->present) {
      case NR_ServingCellConfigCommon__ssb_PositionsInBurst_PR_shortBitmap:
        ssb_bitmap = &scc->ssb_PositionsInBurst->choice.shortBitmap;

        ssb_positionsInBurst = BIT_STRING_to_uint8(ssb_bitmap);
        LOG_D(NR_MAC,"SSB config: SSB_positions_in_burst 0x%lx\n", ssb_positionsInBurst);

        for (uint8_t bit_nb=3; bit_nb<=3; bit_nb--) {
          // If SSB is transmitted
          if ((ssb_positionsInBurst>>bit_nb) & 0x01) {
            ssb_list->nb_tx_ssb++;
            ssb_list->tx_ssb[ssb_idx].transmitted = true;
            LOG_D(NR_MAC,"SSB idx %d transmitted\n", ssb_idx);
          }
          ssb_idx++;
        }
        break;
      case NR_ServingCellConfigCommon__ssb_PositionsInBurst_PR_mediumBitmap:
        ssb_bitmap = &scc->ssb_PositionsInBurst->choice.mediumBitmap;

        ssb_positionsInBurst = BIT_STRING_to_uint8(ssb_bitmap);
        LOG_D(NR_MAC,"SSB config: SSB_positions_in_burst 0x%lx\n", ssb_positionsInBurst);

        for (uint8_t bit_nb=7; bit_nb<=7; bit_nb--) {
          // If SSB is transmitted
          if ((ssb_positionsInBurst>>bit_nb) & 0x01) {
            ssb_list->nb_tx_ssb++;
            ssb_list->tx_ssb[ssb_idx].transmitted = true;
            LOG_D(NR_MAC,"SSB idx %d transmitted\n", ssb_idx);
          }
          ssb_idx++;
        }
        break;
      case NR_ServingCellConfigCommon__ssb_PositionsInBurst_PR_longBitmap:
        ssb_bitmap = &scc->ssb_PositionsInBurst->choice.longBitmap;

        ssb_positionsInBurst = BIT_STRING_to_uint64(ssb_bitmap);
        LOG_D(NR_MAC,"SSB config: SSB_positions_in_burst 0x%lx\n", ssb_positionsInBurst);

        for (uint8_t bit_nb=63; bit_nb<=63; bit_nb--) {
          // If SSB is transmitted
          if ((ssb_positionsInBurst>>bit_nb) & 0x01) {
            ssb_list->nb_tx_ssb++;
            ssb_list->tx_ssb[ssb_idx].transmitted = true;
            LOG_D(NR_MAC,"SSB idx %d transmitted\n", ssb_idx);
          }
          ssb_idx++;
        }
        break;
      default:
        AssertFatal(false,"ssb_PositionsInBurst not present\n");
        break;
    }
  } else { // This is configuration from SIB1

    AssertFatal(mac->scc_SIB->ssb_PositionsInBurst.groupPresence == NULL, "Handle case for >8 SSBs\n");
    ssb_bitmap = &mac->scc_SIB->ssb_PositionsInBurst.inOneGroup;

    ssb_positionsInBurst = BIT_STRING_to_uint8(ssb_bitmap);

    LOG_D(NR_MAC,"SSB config: SSB_positions_in_burst 0x%lx\n", ssb_positionsInBurst);

    for (uint8_t bit_nb=7; bit_nb<=7; bit_nb--) {
      // If SSB is transmitted
      if ((ssb_positionsInBurst>>bit_nb) & 0x01) {
        ssb_list->nb_tx_ssb++;
        ssb_list->tx_ssb[ssb_idx].transmitted = true;
        LOG_D(NR_MAC,"SSB idx %d transmitted\n", ssb_idx);
      }
      ssb_idx++;
    }
  }
}

// Map the transmitted SSBs to the ROs and create the association pattern according to the config
static void map_ssb_to_ro(NR_UE_MAC_INST_t *mac) {

  // Map SSBs to PRACH occasions
  // ===========================
  // WIP: Assumption: No PRACH occasion is rejected because of a conflict with SSBs or TDD_UL_DL_ConfigurationCommon schedule
  NR_RACH_ConfigCommon_t *setup = mac->current_UL_BWP.rach_ConfigCommon;
  NR_RACH_ConfigCommon__ssb_perRACH_OccasionAndCB_PreamblesPerSSB_PR ssb_perRACH_config = setup->ssb_perRACH_OccasionAndCB_PreamblesPerSSB->present;

  bool multiple_ssb_per_ro; // true if more than one or exactly one SSB per RACH occasion, false if more than one RO per SSB
  uint8_t ssb_rach_ratio; // Nb of SSBs per RACH or RACHs per SSB
  uint16_t required_nb_of_prach_occasion; // Nb of RACH occasions required to map all the SSBs
  uint8_t required_nb_of_prach_conf_period; // Nb of PRACH configuration periods required to map all the SSBs

  // Determine the SSB to RACH mapping ratio
  // =======================================
  switch (ssb_perRACH_config){
    case NR_RACH_ConfigCommon__ssb_perRACH_OccasionAndCB_PreamblesPerSSB_PR_oneEighth:
      multiple_ssb_per_ro = false;
      ssb_rach_ratio = 8;
      break;
    case NR_RACH_ConfigCommon__ssb_perRACH_OccasionAndCB_PreamblesPerSSB_PR_oneFourth:
      multiple_ssb_per_ro = false;
      ssb_rach_ratio = 4;
      break;
    case NR_RACH_ConfigCommon__ssb_perRACH_OccasionAndCB_PreamblesPerSSB_PR_oneHalf:
      multiple_ssb_per_ro = false;
      ssb_rach_ratio = 2;
      break;
    case NR_RACH_ConfigCommon__ssb_perRACH_OccasionAndCB_PreamblesPerSSB_PR_one:
      multiple_ssb_per_ro = true;
      ssb_rach_ratio = 1;
      break;
    case NR_RACH_ConfigCommon__ssb_perRACH_OccasionAndCB_PreamblesPerSSB_PR_two:
      multiple_ssb_per_ro = true;
      ssb_rach_ratio = 2;
      break;
    case NR_RACH_ConfigCommon__ssb_perRACH_OccasionAndCB_PreamblesPerSSB_PR_four:
      multiple_ssb_per_ro = true;
      ssb_rach_ratio = 4;
      break;
    case NR_RACH_ConfigCommon__ssb_perRACH_OccasionAndCB_PreamblesPerSSB_PR_eight:
      multiple_ssb_per_ro = true;
      ssb_rach_ratio = 8;
      break;
    case NR_RACH_ConfigCommon__ssb_perRACH_OccasionAndCB_PreamblesPerSSB_PR_sixteen:
      multiple_ssb_per_ro = true;
      ssb_rach_ratio = 16;
      break;
    default:
      AssertFatal(1 == 0, "Unsupported ssb_perRACH_config %d\n", ssb_perRACH_config);
      break;
  }
  LOG_D(NR_MAC,"SSB rach ratio %d, Multiple SSB per RO %d\n", ssb_rach_ratio, multiple_ssb_per_ro);

  // Evaluate the number of PRACH configuration periods required to map all the SSBs and set the association period
  // ==============================================================================================================
  // WIP: Assumption for now is that all the PRACH configuration periods within a maximum association pattern period have the same number of PRACH occasions
  //      (No PRACH occasions are conflicting with SSBs nor TDD_UL_DL_ConfigurationCommon schedule)
  //      There is only one possible association period which can contain up to 16 PRACH configuration periods
  LOG_D(NR_MAC,"Evaluate the number of PRACH configuration periods required to map all the SSBs and set the association period\n");
  if (true == multiple_ssb_per_ro) {
    required_nb_of_prach_occasion = ((mac->ssb_list.nb_tx_ssb-1) + ssb_rach_ratio) / ssb_rach_ratio;
  }
  else {
    required_nb_of_prach_occasion = mac->ssb_list.nb_tx_ssb * ssb_rach_ratio;
  }

  AssertFatal(prach_assoc_pattern.prach_conf_period_list[0].nb_of_prach_occasion>0,
              "prach_assoc_pattern.prach_conf_period_list[0].nb_of_prach_occasion shouldn't be 0 (ssb_list.nb_tx_ssb %d, ssb_rach_ratio %d\n",
              mac->ssb_list.nb_tx_ssb,ssb_rach_ratio);
  required_nb_of_prach_conf_period = ((required_nb_of_prach_occasion-1) + prach_assoc_pattern.prach_conf_period_list[0].nb_of_prach_occasion) /
                                     prach_assoc_pattern.prach_conf_period_list[0].nb_of_prach_occasion;

  if (required_nb_of_prach_conf_period == 1) {
    prach_assoc_pattern.prach_association_period_list[0].nb_of_prach_conf_period = 1;
  }
  else if (required_nb_of_prach_conf_period == 2) {
    prach_assoc_pattern.prach_association_period_list[0].nb_of_prach_conf_period = 2;
  }
  else if (required_nb_of_prach_conf_period <= 4) {
    prach_assoc_pattern.prach_association_period_list[0].nb_of_prach_conf_period = 4;
  }
  else if (required_nb_of_prach_conf_period <= 8) {
    prach_assoc_pattern.prach_association_period_list[0].nb_of_prach_conf_period = 8;
  }
  else if (required_nb_of_prach_conf_period <= 16) {
    prach_assoc_pattern.prach_association_period_list[0].nb_of_prach_conf_period = 16;
  }
  else {
    AssertFatal(1 == 0, "Invalid number of PRACH config periods within an association period %d\n", required_nb_of_prach_conf_period);
  }

  prach_assoc_pattern.nb_of_assoc_period = 1; // WIP: only one possible association period
  prach_assoc_pattern.prach_association_period_list[0].nb_of_frame = prach_assoc_pattern.prach_association_period_list[0].nb_of_prach_conf_period * prach_assoc_pattern.prach_conf_period_list[0].nb_of_frame;
  prach_assoc_pattern.nb_of_frame = prach_assoc_pattern.prach_association_period_list[0].nb_of_frame;

  LOG_D(NR_MAC,"Assoc period %d, Nb of frames in assoc period %d\n",
        prach_assoc_pattern.prach_association_period_list[0].nb_of_prach_conf_period,
        prach_assoc_pattern.prach_association_period_list[0].nb_of_frame);

  // Proceed to the SSB to RO mapping
  // ================================
  uint8_t association_period_idx; // Association period index within the association pattern
  uint8_t ssb_idx = 0;
  uint8_t prach_configuration_period_idx; // PRACH Configuration period index within the association pattern
  prach_conf_period_t *prach_conf_period_p;

  // Map all the association periods within the association pattern period
  LOG_D(NR_MAC,"Proceed to the SSB to RO mapping\n");
  for (association_period_idx=0; association_period_idx<prach_assoc_pattern.nb_of_assoc_period; association_period_idx++) {
    uint8_t n_prach_conf=0; // PRACH Configuration period index within the association period
    uint8_t frame=0;
    uint8_t slot=0;
    uint8_t ro_in_time=0;
    uint8_t ro_in_freq=0;

    // Set the starting PRACH Configuration period index in the association_pattern map for this particular association period
    prach_configuration_period_idx = 0;  // WIP: only one possible association period so the starting PRACH configuration period is automatically 0

    // Check if we need to map multiple SSBs per RO or multiple ROs per SSB
    if (true == multiple_ssb_per_ro) {
      // --------------------
      // --------------------
      // Multiple SSBs per RO
      // --------------------
      // --------------------

      // WIP: For the moment, only map each SSB idx once per association period if configuration is multiple SSBs per RO
      //      this is true if no PRACH occasions are conflicting with SSBs nor TDD_UL_DL_ConfigurationCommon schedule
      ssb_idx = 0;

      // Go through the list of PRACH config periods within this association period
      for (n_prach_conf=0; n_prach_conf<prach_assoc_pattern.prach_association_period_list[association_period_idx].nb_of_prach_conf_period; n_prach_conf++, prach_configuration_period_idx++) {
        // Build the association period with its association PRACH Configuration indexes
        prach_conf_period_p = &prach_assoc_pattern.prach_conf_period_list[prach_configuration_period_idx];
        prach_assoc_pattern.prach_association_period_list[association_period_idx].prach_conf_period_list[n_prach_conf] = prach_conf_period_p;

        // Go through all the ROs within the PRACH config period
        for (frame=0; frame<prach_conf_period_p->nb_of_frame; frame++) {
          for (slot=0; slot<prach_conf_period_p->nb_of_slot; slot++) {
            for (ro_in_time=0; ro_in_time<prach_conf_period_p->prach_occasion_slot_map[frame][slot].nb_of_prach_occasion_in_time; ro_in_time++) {
              for (ro_in_freq=0; ro_in_freq<prach_conf_period_p->prach_occasion_slot_map[frame][slot].nb_of_prach_occasion_in_freq; ro_in_freq++) {
                prach_occasion_info_t *ro_p = &prach_conf_period_p->prach_occasion_slot_map[frame][slot].prach_occasion[ro_in_time][ro_in_freq];

                // Go through the list of transmitted SSBs and map the required amount of SSBs to this RO
                // WIP: For the moment, only map each SSB idx once per association period if configuration is multiple SSBs per RO
                //      this is true if no PRACH occasions are conflicting with SSBs nor TDD_UL_DL_ConfigurationCommon schedule
                for (; ssb_idx<MAX_NB_SSB; ssb_idx++) {
                  // Map only the transmitted ssb_idx
                  if (true == mac->ssb_list.tx_ssb[ssb_idx].transmitted) {
                    ro_p->mapped_ssb_idx[ro_p->nb_mapped_ssb] = ssb_idx;
                    ro_p->nb_mapped_ssb++;
                    mac->ssb_list.tx_ssb[ssb_idx].mapped_ro[mac->ssb_list.tx_ssb[ssb_idx].nb_mapped_ro] = ro_p;
                    mac->ssb_list.tx_ssb[ssb_idx].nb_mapped_ro++;
                    AssertFatal(MAX_NB_RO_PER_SSB_IN_ASSOCIATION_PATTERN > mac->ssb_list.tx_ssb[ssb_idx].nb_mapped_ro,
                                "Too many mapped ROs (%d) to a single SSB\n",
                                mac->ssb_list.tx_ssb[ssb_idx].nb_mapped_ro);

                    LOG_D(NR_MAC, "Mapped ssb_idx %u to RO slot-symbol %u-%u, %u-%u-%u/%u\n",
                          ssb_idx, ro_p->slot, ro_p->start_symbol, slot, ro_in_time, ro_in_freq,
                          prach_conf_period_p->prach_occasion_slot_map[frame][slot].nb_of_prach_occasion_in_freq);
                    LOG_D(NR_MAC, "Nb mapped ROs for this ssb idx: in the association period only %u\n",
                          mac->ssb_list.tx_ssb[ssb_idx].nb_mapped_ro);

                    // If all the required SSBs are mapped to this RO, exit the loop of SSBs
                    if (ro_p->nb_mapped_ssb == ssb_rach_ratio) {
                      ssb_idx++;
                      break;
                    }
                  } // if ssb_idx is transmitted
                } // for ssb_idx

                // Exit the loop of ROs if there is no more SSB to map
                if (MAX_NB_SSB == ssb_idx) break;
              } // for ro_in_freq

              // Exit the loop of ROs if there is no more SSB to map
              if (MAX_NB_SSB == ssb_idx) break;
            } // for ro_in_time

            // Exit the loop of slots if there is no more SSB to map
            if (MAX_NB_SSB == ssb_idx) break;
          } // for slot

          // Exit the loop frames if there is no more SSB to map
          if (MAX_NB_SSB == ssb_idx) break;
        } // for frame

        // Exit the loop of PRACH configurations if there is no more SSB to map
        if (MAX_NB_SSB == ssb_idx) break;
      } // for n_prach_conf

      // WIP: note that there is no re-mapping of the SSBs within the association period since there is no invalid ROs in the PRACH config periods that would create this situation

    } // if multiple_ssbs_per_ro

    else {
      // --------------------
      // --------------------
      // Multiple ROs per SSB
      // --------------------
      // --------------------

      n_prach_conf = 0;

      // Go through the list of transmitted SSBs
      for (ssb_idx=0; ssb_idx<MAX_NB_SSB; ssb_idx++) {
        uint8_t nb_mapped_ro_in_association_period=0; // Reset the nb of mapped ROs for the new SSB index
        LOG_D(NR_MAC,"Checking ssb_idx %d => %d\n",
              ssb_idx, mac->ssb_list.tx_ssb[ssb_idx].transmitted);

        // Map only the transmitted ssb_idx
        if (true == mac->ssb_list.tx_ssb[ssb_idx].transmitted) {

          // Map all the required ROs to this SSB
          // Go through the list of PRACH config periods within this association period
          for (; n_prach_conf<prach_assoc_pattern.prach_association_period_list[association_period_idx].nb_of_prach_conf_period; n_prach_conf++, prach_configuration_period_idx++) {

            // Build the association period with its association PRACH Configuration indexes
            prach_conf_period_p = &prach_assoc_pattern.prach_conf_period_list[prach_configuration_period_idx];
            prach_assoc_pattern.prach_association_period_list[association_period_idx].prach_conf_period_list[n_prach_conf] = prach_conf_period_p;

            for (; frame<prach_conf_period_p->nb_of_frame; frame++) {
              for (; slot<prach_conf_period_p->nb_of_slot; slot++) {
                for (; ro_in_time<prach_conf_period_p->prach_occasion_slot_map[frame][slot].nb_of_prach_occasion_in_time; ro_in_time++) {
                  for (; ro_in_freq<prach_conf_period_p->prach_occasion_slot_map[frame][slot].nb_of_prach_occasion_in_freq; ro_in_freq++) {
                    prach_occasion_info_t *ro_p = &prach_conf_period_p->prach_occasion_slot_map[frame][slot].prach_occasion[ro_in_time][ro_in_freq];

                    ro_p->mapped_ssb_idx[0] = ssb_idx;
                    ro_p->nb_mapped_ssb = 1;
                    mac->ssb_list.tx_ssb[ssb_idx].mapped_ro[mac->ssb_list.tx_ssb[ssb_idx].nb_mapped_ro] = ro_p;
                    mac->ssb_list.tx_ssb[ssb_idx].nb_mapped_ro++;
                    AssertFatal(MAX_NB_RO_PER_SSB_IN_ASSOCIATION_PATTERN > mac->ssb_list.tx_ssb[ssb_idx].nb_mapped_ro,
                                "Too many mapped ROs (%d) to a single SSB\n",
                                mac->ssb_list.tx_ssb[ssb_idx].nb_mapped_ro);
                    nb_mapped_ro_in_association_period++;

                    LOG_D(NR_MAC,"Mapped ssb_idx %u to RO slot-symbol %u-%u, %u-%u-%u/%u\n",
                          ssb_idx, ro_p->slot, ro_p->start_symbol, slot, ro_in_time, ro_in_freq,
                          prach_conf_period_p->prach_occasion_slot_map[frame][slot].nb_of_prach_occasion_in_freq);
                    LOG_D(NR_MAC, "Nb mapped ROs for this ssb idx: in the association period only %u / total %u\n",
                          mac->ssb_list.tx_ssb[ssb_idx].nb_mapped_ro, nb_mapped_ro_in_association_period);

                    // Exit the loop if this SSB has been mapped to all the required ROs
                    // WIP: Assuming that ssb_rach_ratio equals the maximum nb of times a given ssb_idx is mapped within an association period:
                    //      this is true if no PRACH occasions are conflicting with SSBs nor TDD_UL_DL_ConfigurationCommon schedule
                    if (nb_mapped_ro_in_association_period == ssb_rach_ratio) {
                      ro_in_freq++;
                      break;
                    }
                  } // for ro_in_freq

                  // Exit the loop if this SSB has been mapped to all the required ROs
                  if (nb_mapped_ro_in_association_period == ssb_rach_ratio) {
                    break;
                  }
                  else ro_in_freq = 0; // else go to the next time symbol in that slot and reset the freq index
                } // for ro_in_time

                // Exit the loop if this SSB has been mapped to all the required ROs
                if (nb_mapped_ro_in_association_period == ssb_rach_ratio) {
                  break;
                }
                else ro_in_time = 0; // else go to the next slot in that PRACH config period and reset the symbol index
              } // for slot

              // Exit the loop if this SSB has been mapped to all the required ROs
              if (nb_mapped_ro_in_association_period == ssb_rach_ratio) {
                break;
              }
              else slot = 0; // else go to the next frame in that PRACH config period and reset the slot index
            } // for frame

            // Exit the loop if this SSB has been mapped to all the required ROs
            if (nb_mapped_ro_in_association_period == ssb_rach_ratio) {
              break;
            }
            else frame = 0; // else go to the next PRACH config period in that association period and reset the frame index
          } // for n_prach_conf

        } // if ssb_idx is transmitted
      } // for ssb_idx
    } // else if multiple_ssbs_per_ro

  } // for association_period_index
}

// Returns a RACH occasion if any matches the SSB idx, the frame and the slot
static int get_nr_prach_info_from_ssb_index(uint8_t ssb_idx,
                                            int frame,
                                            int slot,
                                            ssb_list_info_t *ssb_list,
                                            prach_occasion_info_t **prach_occasion_info_pp)
{
  ssb_info_t *ssb_info_p;
  prach_occasion_slot_t *prach_occasion_slot_p = NULL;

  *prach_occasion_info_pp = NULL;

  // Search for a matching RO slot in the SSB_to_RO map
  // A valid RO slot will match:
  //      - ssb_idx mapped to one of the ROs in that RO slot
  //      - exact slot number
  //      - frame offset
  ssb_info_p = &ssb_list->tx_ssb[ssb_idx];
  LOG_D(NR_MAC,"checking for prach : ssb_info_p->nb_mapped_ro %d\n",ssb_info_p->nb_mapped_ro);
  for (uint8_t n_mapped_ro=0; n_mapped_ro<ssb_info_p->nb_mapped_ro; n_mapped_ro++) {
    LOG_D(NR_MAC,"%d.%d: mapped_ro[%d]->frame.slot %d.%d, prach_assoc_pattern.nb_of_frame %d\n",
          frame,slot,n_mapped_ro,ssb_info_p->mapped_ro[n_mapped_ro]->frame,ssb_info_p->mapped_ro[n_mapped_ro]->slot,prach_assoc_pattern.nb_of_frame);
    if ((slot == ssb_info_p->mapped_ro[n_mapped_ro]->slot) &&
        (ssb_info_p->mapped_ro[n_mapped_ro]->frame == (frame % prach_assoc_pattern.nb_of_frame))) {

      uint8_t prach_config_period_nb = ssb_info_p->mapped_ro[n_mapped_ro]->frame / prach_assoc_pattern.prach_conf_period_list[0].nb_of_frame;
      uint8_t frame_nb_in_prach_config_period = ssb_info_p->mapped_ro[n_mapped_ro]->frame % prach_assoc_pattern.prach_conf_period_list[0].nb_of_frame;
      prach_occasion_slot_p = &prach_assoc_pattern.prach_conf_period_list[prach_config_period_nb].prach_occasion_slot_map[frame_nb_in_prach_config_period][slot];
    }
  }

  // If there is a matching RO slot in the SSB_to_RO map
  if (NULL != prach_occasion_slot_p)
  {
    // A random RO mapped to the SSB index should be selected in the slot

    // First count the number of times the SSB index is found in that RO
    uint8_t nb_mapped_ssb = 0;

    for (int ro_in_time=0; ro_in_time < prach_occasion_slot_p->nb_of_prach_occasion_in_time; ro_in_time++) {
      for (int ro_in_freq=0; ro_in_freq < prach_occasion_slot_p->nb_of_prach_occasion_in_freq; ro_in_freq++) {
        prach_occasion_info_t *prach_occasion_info_p = &prach_occasion_slot_p->prach_occasion[ro_in_time][ro_in_freq];

        for (uint8_t ssb_nb=0; ssb_nb<prach_occasion_info_p->nb_mapped_ssb; ssb_nb++) {
          if (prach_occasion_info_p->mapped_ssb_idx[ssb_nb] == ssb_idx) {
            nb_mapped_ssb++;
          }
        }
      }
    }

    // Choose a random SSB nb
    uint8_t random_ssb_nb = 0;

    random_ssb_nb = ((taus()) % nb_mapped_ssb);

    // Select the RO according to the chosen random SSB nb
    nb_mapped_ssb=0;
    for (int ro_in_time=0; ro_in_time < prach_occasion_slot_p->nb_of_prach_occasion_in_time; ro_in_time++) {
      for (int ro_in_freq=0; ro_in_freq < prach_occasion_slot_p->nb_of_prach_occasion_in_freq; ro_in_freq++) {
        prach_occasion_info_t *prach_occasion_info_p = &prach_occasion_slot_p->prach_occasion[ro_in_time][ro_in_freq];

        for (uint8_t ssb_nb=0; ssb_nb<prach_occasion_info_p->nb_mapped_ssb; ssb_nb++) {
          if (prach_occasion_info_p->mapped_ssb_idx[ssb_nb] == ssb_idx) {
            if (nb_mapped_ssb == random_ssb_nb) {
              *prach_occasion_info_pp = prach_occasion_info_p;
              return 1;
            }
            else {
              nb_mapped_ssb++;
            }
          }
        }
      }
    }
  }

  return 0;
}

// Build the SSB to RO mapping upon RRC configuration update
void build_ssb_to_ro_map(NR_UE_MAC_INST_t *mac) {

  // Clear all the lists and maps
  memset(&prach_assoc_pattern, 0, sizeof(prach_association_pattern_t));
  memset(&mac->ssb_list, 0, sizeof(ssb_list_info_t));

  // Build the list of all the valid RACH occasions in the maximum association pattern period according to the PRACH config
  LOG_D(NR_MAC,"Build RO list\n");
  build_ro_list(mac);

  // Build the list of all the valid/transmitted SSBs according to the config
  LOG_D(NR_MAC,"Build SSB list\n");
  build_ssb_list(mac);

  // Map the transmitted SSBs to the ROs and create the association pattern according to the config
  LOG_D(NR_MAC,"Map SSB to RO\n");
  map_ssb_to_ro(mac);
  LOG_D(NR_MAC,"Map SSB to RO done\n");
}

void nr_ue_pucch_scheduler(module_id_t module_idP, frame_t frameP, int slotP, void *phy_data)
{
  NR_UE_MAC_INST_t *mac = get_mac_inst(module_idP);
  PUCCH_sched_t pucch[3] = {0}; // TODO the size might change in the future in case of multiple SR or multiple CSI in a slot

  mac->nr_ue_emul_l1.num_srs = 0;
  mac->nr_ue_emul_l1.num_harqs = 0;
  mac->nr_ue_emul_l1.num_csi_reports = 0;
  int num_res = 0;

  // SR
  if (mac->state == UE_CONNECTED && trigger_periodic_scheduling_request(mac, &pucch[0], frameP, slotP)) {
    num_res++;
    /* sr_payload = 1 means that this is a positive SR, sr_payload = 0 means that it is a negative SR */
    pucch[0].sr_payload = nr_ue_get_SR(module_idP, frameP, slotP);
  }

  // CSI
  int csi_res = 0;
  if (mac->state == UE_CONNECTED)
    csi_res = nr_get_csi_measurements(mac, frameP, slotP, &pucch[num_res]);
  if (csi_res > 0) {
    num_res += csi_res;
  }

  // ACKNACK
  bool any_harq = get_downlink_ack(mac, frameP, slotP, &pucch[num_res]);
  if (any_harq)
    num_res++;

  if (num_res == 0)
    return;
  // do no transmit pucch if only SR scheduled and it is negative
  if (num_res == 1 && pucch[0].n_sr > 0 && pucch[0].sr_payload == 0)
    return;

  if (num_res > 1)
    multiplex_pucch_resource(mac, pucch, num_res);
  fapi_nr_ul_config_request_t *ul_config = get_ul_config_request(mac, slotP, 0);
  pthread_mutex_lock(&ul_config->mutex_ul_config);
  for (int j = 0; j < num_res; j++) {
    if (pucch[j].n_harq + pucch[j].n_sr + pucch[j].n_csi != 0) {
      LOG_D(NR_MAC,
            "%d.%d configure pucch, O_ACK %d, O_SR %d, O_CSI %d\n",
            frameP,
            slotP,
            pucch[j].n_harq,
            pucch[j].n_sr,
            pucch[j].n_csi);
      mac->nr_ue_emul_l1.num_srs = pucch[j].n_sr;
      mac->nr_ue_emul_l1.num_harqs = pucch[j].n_harq;
      mac->nr_ue_emul_l1.num_csi_reports = pucch[j].n_csi;
      AssertFatal(ul_config->number_pdus < FAPI_NR_UL_CONFIG_LIST_NUM,
                  "ul_config->number_pdus %d out of bounds\n",
                  ul_config->number_pdus);
      fapi_nr_ul_config_pucch_pdu *pucch_pdu = &ul_config->ul_config_list[ul_config->number_pdus].pucch_config_pdu;
      fill_ul_config(ul_config, frameP, slotP, FAPI_NR_UL_CONFIG_TYPE_PUCCH);
      mac->nr_ue_emul_l1.active_uci_sfn_slot = NFAPI_SFNSLOT2HEX(frameP, slotP);
      pthread_mutex_unlock(&ul_config->mutex_ul_config);
      nr_ue_configure_pucch(mac,
                            slotP,
                            mac->crnti, // FIXME not sure this is valid for all pucch instances
                            &pucch[j],
                            pucch_pdu);
      nr_scheduled_response_t scheduled_response;
      fill_scheduled_response(&scheduled_response, NULL, ul_config, NULL, NULL,NULL,module_idP, 0 /*TBR fix*/, frameP, slotP, phy_data);
      if (mac->if_module != NULL && mac->if_module->scheduled_response != NULL)
        mac->if_module->scheduled_response(&scheduled_response);
      if (mac->state == UE_WAIT_TX_ACK_MSG4)
        mac->state = UE_CONNECTED;
    }
  }
}

void nr_schedule_csi_for_im(NR_UE_MAC_INST_t *mac, int frame, int slot) {

  NR_UE_UL_BWP_t *current_UL_BWP = &mac->current_UL_BWP;

  if (!current_UL_BWP->csi_MeasConfig)
    return;

  NR_CSI_MeasConfig_t *csi_measconfig = current_UL_BWP->csi_MeasConfig;

  if (csi_measconfig->csi_IM_ResourceToAddModList == NULL)
    return;

  fapi_nr_dl_config_request_t *dl_config = get_dl_config_request(mac, slot);
  NR_CSI_IM_Resource_t *imcsi;
  int period, offset;

  NR_UE_DL_BWP_t *current_DL_BWP = &mac->current_DL_BWP;
  int mu = current_DL_BWP->scs;
  uint16_t bwp_size = current_DL_BWP->BWPSize;
  uint16_t bwp_start = current_DL_BWP->BWPStart;

  for (int id = 0; id < csi_measconfig->csi_IM_ResourceToAddModList->list.count; id++){
    imcsi = csi_measconfig->csi_IM_ResourceToAddModList->list.array[id];
    csi_period_offset(NULL,imcsi->periodicityAndOffset,&period,&offset);
    if((frame*nr_slots_per_frame[mu]+slot-offset)%period != 0)
      continue;
    fapi_nr_dl_config_csiim_pdu_rel15_t *csiim_config_pdu = &dl_config->dl_config_list[dl_config->number_pdus].csiim_config_pdu.csiim_config_rel15;
    csiim_config_pdu->bwp_size = bwp_size;
    csiim_config_pdu->bwp_start = bwp_start;
    csiim_config_pdu->subcarrier_spacing = mu;
    csiim_config_pdu->start_rb = imcsi->freqBand->startingRB;
    csiim_config_pdu->nr_of_rbs = imcsi->freqBand->nrofRBs;
    // As specified in 5.2.2.4 of 38.214
    switch (imcsi->csi_IM_ResourceElementPattern->present) {
      case NR_CSI_IM_Resource__csi_IM_ResourceElementPattern_PR_pattern0:
        for (int i = 0; i<4; i++) {
          csiim_config_pdu->k_csiim[i] = (imcsi->csi_IM_ResourceElementPattern->choice.pattern0->subcarrierLocation_p0<<1) + (i>>1);
          csiim_config_pdu->l_csiim[i] = imcsi->csi_IM_ResourceElementPattern->choice.pattern0->symbolLocation_p0 + (i%2);
        }
        break;
      case NR_CSI_IM_Resource__csi_IM_ResourceElementPattern_PR_pattern1:
        for (int i = 0; i<4; i++) {
          csiim_config_pdu->k_csiim[i] = (imcsi->csi_IM_ResourceElementPattern->choice.pattern1->subcarrierLocation_p1<<2) + i;
          csiim_config_pdu->l_csiim[i] = imcsi->csi_IM_ResourceElementPattern->choice.pattern1->symbolLocation_p1;
        }
        break;
      default:
        AssertFatal(1==0, "Invalid CSI-IM pattern\n");
    }
    dl_config->dl_config_list[dl_config->number_pdus].pdu_type = FAPI_NR_DL_CONFIG_TYPE_CSI_IM;
    dl_config->number_pdus += 1;
  }
}

NR_CSI_ResourceConfigId_t find_CSI_resourceconfig(NR_CSI_MeasConfig_t *csi_measconfig,
                                                  NR_BWP_Id_t dl_bwp_id,
                                                  NR_NZP_CSI_RS_ResourceId_t csi_id)
{
  bool found = false;
  for (int csi_list = 0; csi_list < csi_measconfig->csi_ResourceConfigToAddModList->list.count; csi_list++) {
    NR_CSI_ResourceConfig_t *csires = csi_measconfig->csi_ResourceConfigToAddModList->list.array[csi_list];
    if(csires->bwp_Id != dl_bwp_id)
      continue;
    struct NR_CSI_ResourceConfig__csi_RS_ResourceSetList *resset = &csires->csi_RS_ResourceSetList;
    if(resset->present != NR_CSI_ResourceConfig__csi_RS_ResourceSetList_PR_nzp_CSI_RS_SSB)
      continue;
    if(!resset->choice.nzp_CSI_RS_SSB->nzp_CSI_RS_ResourceSetList)
      continue;
    for(int i = 0; i < resset->choice.nzp_CSI_RS_SSB->nzp_CSI_RS_ResourceSetList->list.count; i++) {
      NR_NZP_CSI_RS_ResourceSetId_t *res_id = resset->choice.nzp_CSI_RS_SSB->nzp_CSI_RS_ResourceSetList->list.array[i];
      AssertFatal(res_id, "NR_NZP_CSI_RS_ResourceSetId shouldn't be NULL\n");
      struct NR_CSI_MeasConfig__nzp_CSI_RS_ResourceSetToAddModList *res_list = csi_measconfig->nzp_CSI_RS_ResourceSetToAddModList;
      AssertFatal(res_list, "nzp_CSI_RS_ResourceSetToAddModList shouldn't be NULL\n");
      for (int j = 0; j < res_list->list.count; j++) {
        NR_NZP_CSI_RS_ResourceSet_t *csi_res = res_list->list.array[j];
        if(*res_id != csi_res->nzp_CSI_ResourceSetId)
          continue;
        for (int k = 0; k < csi_res->nzp_CSI_RS_Resources.list.count; k++) {
          AssertFatal(csi_res->nzp_CSI_RS_Resources.list.array[k],
                      "NZP_CSI_RS_ResourceId shoulan't be NULL\n");
          if (csi_id == *csi_res->nzp_CSI_RS_Resources.list.array[k]) {
            found = true;
            break;
          }
        }
        if (found && csi_res->trs_Info)
          // CRI-RS for Tracking (not implemented yet)
          // in this case we there is no associated CSI report
          // therefore to signal this we return a value higher than
          // maxNrofCSI-ResourceConfigurations
          return NR_maxNrofCSI_ResourceConfigurations + 1;
        else if (found)
          return csires->csi_ResourceConfigId;
      }
    }
  }
  return -1; // not found any CSI-resource in current DL BWP associated with this CSI-RS ID
}

uint8_t set_csirs_measurement_bitmap(NR_CSI_MeasConfig_t *csi_measconfig,
                                     NR_CSI_ResourceConfigId_t csi_res_id)
{
  uint8_t meas_bitmap = 0;
  if (csi_res_id > NR_maxNrofCSI_ResourceConfigurations)
    return meas_bitmap; // CSI-RS for tracking
  for(int i = 0; i < csi_measconfig->csi_ReportConfigToAddModList->list.count; i++) {
    struct NR_CSI_ReportConfig *report_config = csi_measconfig->csi_ReportConfigToAddModList->list.array[i];
    if(report_config->resourcesForChannelMeasurement != csi_res_id)
      continue;
    // bit 0 RSRP bit 1 RI bit 2 LI bit 3 PMI bit 4 CQI bit 5 i1
    switch (report_config->reportQuantity.present) {
      case NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_PMI_CQI :
        meas_bitmap += (1 << 1) + (1 << 3) + (1 << 4);
        break;
      case NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_i1 :
        meas_bitmap += (1 << 1) + (1 << 5);
        break;
      case NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_i1_CQI :
        meas_bitmap += (1 << 1) + (1 << 4) + (1 << 5);
        break;
      case NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_CQI :
        meas_bitmap += (1 << 1) + (1 << 4);
        break;
      case NR_CSI_ReportConfig__reportQuantity_PR_cri_RSRP :
        meas_bitmap += 1;
        break;
      case NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_LI_PMI_CQI :
        meas_bitmap += (1 << 1) + (1 << 2) + (1 << 3) + (1 << 4);
        break;
      default :
        AssertFatal(false, "Unexpected measurement report type %d\n",
                    report_config->reportQuantity.present);
    }
  }
  AssertFatal(meas_bitmap > 0, "Expected to have at least 1 measurement configured for CSI-RS\n");
  return meas_bitmap;
}

void nr_schedule_csirs_reception(NR_UE_MAC_INST_t *mac, int frame, int slot) {

  NR_UE_UL_BWP_t *current_UL_BWP = &mac->current_UL_BWP;

  if (!current_UL_BWP->csi_MeasConfig)
    return;

  NR_CSI_MeasConfig_t *csi_measconfig = current_UL_BWP->csi_MeasConfig;

  if (csi_measconfig->nzp_CSI_RS_ResourceToAddModList == NULL)
    return;

  fapi_nr_dl_config_request_t *dl_config = get_dl_config_request(mac, slot);
  NR_UE_DL_BWP_t *current_DL_BWP = &mac->current_DL_BWP;
  NR_BWP_Id_t dl_bwp_id = current_DL_BWP->bwp_id;

  int mu = current_DL_BWP->scs;
  uint16_t bwp_size = current_DL_BWP->BWPSize;
  uint16_t bwp_start = current_DL_BWP->BWPStart;

  for (int id = 0; id < csi_measconfig->nzp_CSI_RS_ResourceToAddModList->list.count; id++){
    NR_NZP_CSI_RS_Resource_t *nzpcsi = csi_measconfig->nzp_CSI_RS_ResourceToAddModList->list.array[id];
    int period, offset;
    csi_period_offset(NULL, nzpcsi->periodicityAndOffset, &period, &offset);
    if((frame * nr_slots_per_frame[mu] + slot-offset) % period != 0)
      continue;
    NR_CSI_ResourceConfigId_t csi_res_id = find_CSI_resourceconfig(csi_measconfig,
                                                                   dl_bwp_id,
                                                                   nzpcsi->nzp_CSI_RS_ResourceId);
    // do not schedule reseption of this CSI-RS if not associated with current BWP
    if(csi_res_id < 0)
      continue;
    LOG_D(MAC,"Scheduling reception of CSI-RS in frame %d slot %d\n", frame, slot);
    fapi_nr_dl_config_csirs_pdu_rel15_t *csirs_config_pdu = &dl_config->dl_config_list[dl_config->number_pdus].csirs_config_pdu.csirs_config_rel15;
    csirs_config_pdu->measurement_bitmap = set_csirs_measurement_bitmap(csi_measconfig, csi_res_id);
    NR_CSI_RS_ResourceMapping_t  resourceMapping = nzpcsi->resourceMapping;
    csirs_config_pdu->subcarrier_spacing = mu;
    csirs_config_pdu->cyclic_prefix = current_DL_BWP->cyclicprefix ? *current_DL_BWP->cyclicprefix : 0;

    // According to last paragraph of TS 38.214 5.2.2.3.1
    if (resourceMapping.freqBand.startingRB < bwp_start) {
      csirs_config_pdu->start_rb = bwp_start;
    } else {
      csirs_config_pdu->start_rb = resourceMapping.freqBand.startingRB;
    }
    if (resourceMapping.freqBand.nrofRBs > (bwp_start + bwp_size - csirs_config_pdu->start_rb)) {
      csirs_config_pdu->nr_of_rbs = bwp_start + bwp_size - csirs_config_pdu->start_rb;
    } else {
      csirs_config_pdu->nr_of_rbs = resourceMapping.freqBand.nrofRBs;
    }
    AssertFatal(csirs_config_pdu->nr_of_rbs >= 24, "CSI-RS has %d RBs, but the minimum is 24\n", csirs_config_pdu->nr_of_rbs);

    csirs_config_pdu->csi_type = 1; // NZP-CSI-RS
    csirs_config_pdu->symb_l0 = resourceMapping.firstOFDMSymbolInTimeDomain;
    if (resourceMapping.firstOFDMSymbolInTimeDomain2)
      csirs_config_pdu->symb_l1 = *resourceMapping.firstOFDMSymbolInTimeDomain2;
    csirs_config_pdu->cdm_type = resourceMapping.cdm_Type;
    csirs_config_pdu->freq_density = resourceMapping.density.present;
    if ((resourceMapping.density.present == NR_CSI_RS_ResourceMapping__density_PR_dot5)
        && (resourceMapping.density.choice.dot5 == NR_CSI_RS_ResourceMapping__density__dot5_evenPRBs))
      csirs_config_pdu->freq_density--;
    csirs_config_pdu->scramb_id = nzpcsi->scramblingID;
    csirs_config_pdu->power_control_offset = nzpcsi->powerControlOffset + 8;
    if (nzpcsi->powerControlOffsetSS)
      csirs_config_pdu->power_control_offset_ss = *nzpcsi->powerControlOffsetSS;
    else
      csirs_config_pdu->power_control_offset_ss = 1; // 0 dB
    switch(resourceMapping.frequencyDomainAllocation.present){
      case NR_CSI_RS_ResourceMapping__frequencyDomainAllocation_PR_row1:
        csirs_config_pdu->row = 1;
        csirs_config_pdu->freq_domain = ((resourceMapping.frequencyDomainAllocation.choice.row1.buf[0])>>4)&0x0f;
        break;
      case NR_CSI_RS_ResourceMapping__frequencyDomainAllocation_PR_row2:
        csirs_config_pdu->row = 2;
        csirs_config_pdu->freq_domain = (((resourceMapping.frequencyDomainAllocation.choice.row2.buf[1]>>4)&0x0f) |
                                        ((resourceMapping.frequencyDomainAllocation.choice.row2.buf[0]<<4)&0xff0));
        break;
      case NR_CSI_RS_ResourceMapping__frequencyDomainAllocation_PR_row4:
        csirs_config_pdu->row = 4;
        csirs_config_pdu->freq_domain = ((resourceMapping.frequencyDomainAllocation.choice.row4.buf[0])>>5)&0x07;
        break;
      case NR_CSI_RS_ResourceMapping__frequencyDomainAllocation_PR_other:
        csirs_config_pdu->freq_domain = ((resourceMapping.frequencyDomainAllocation.choice.other.buf[0])>>2)&0x3f;
        // determining the row of table 7.4.1.5.3-1 in 38.211
        switch(resourceMapping.nrofPorts){
          case NR_CSI_RS_ResourceMapping__nrofPorts_p1:
            AssertFatal(1==0,"Resource with 1 CSI port shouldn't be within other rows\n");
            break;
          case NR_CSI_RS_ResourceMapping__nrofPorts_p2:
            csirs_config_pdu->row = 3;
            break;
          case NR_CSI_RS_ResourceMapping__nrofPorts_p4:
            csirs_config_pdu->row = 5;
            break;
          case NR_CSI_RS_ResourceMapping__nrofPorts_p8:
            if (resourceMapping.cdm_Type == NR_CSI_RS_ResourceMapping__cdm_Type_cdm4_FD2_TD2)
              csirs_config_pdu->row = 8;
            else{
              int num_k = 0;
              for (int k=0; k<6; k++)
                num_k+=(((csirs_config_pdu->freq_domain)>>k)&0x01);
              if(num_k==4)
                csirs_config_pdu->row = 6;
              else
                csirs_config_pdu->row = 7;
            }
            break;
          case NR_CSI_RS_ResourceMapping__nrofPorts_p12:
            if (resourceMapping.cdm_Type == NR_CSI_RS_ResourceMapping__cdm_Type_cdm4_FD2_TD2)
              csirs_config_pdu->row = 10;
            else
              csirs_config_pdu->row = 9;
            break;
          case NR_CSI_RS_ResourceMapping__nrofPorts_p16:
            if (resourceMapping.cdm_Type == NR_CSI_RS_ResourceMapping__cdm_Type_cdm4_FD2_TD2)
              csirs_config_pdu->row = 12;
            else
              csirs_config_pdu->row = 11;
            break;
          case NR_CSI_RS_ResourceMapping__nrofPorts_p24:
            if (resourceMapping.cdm_Type == NR_CSI_RS_ResourceMapping__cdm_Type_cdm4_FD2_TD2)
              csirs_config_pdu->row = 14;
            else{
              if (resourceMapping.cdm_Type == NR_CSI_RS_ResourceMapping__cdm_Type_cdm8_FD2_TD4)
                csirs_config_pdu->row = 15;
              else
                csirs_config_pdu->row = 13;
            }
            break;
          case NR_CSI_RS_ResourceMapping__nrofPorts_p32:
            if (resourceMapping.cdm_Type == NR_CSI_RS_ResourceMapping__cdm_Type_cdm4_FD2_TD2)
              csirs_config_pdu->row = 17;
            else{
              if (resourceMapping.cdm_Type == NR_CSI_RS_ResourceMapping__cdm_Type_cdm8_FD2_TD4)
                csirs_config_pdu->row = 18;
              else
                csirs_config_pdu->row = 16;
            }
            break;
        default:
          AssertFatal(1==0,"Invalid number of ports in CSI-RS resource\n");
        }
        break;
      default:
        AssertFatal(1==0,"Invalid freqency domain allocation in CSI-RS resource\n");
    }
    dl_config->dl_config_list[dl_config->number_pdus].pdu_type = FAPI_NR_DL_CONFIG_TYPE_CSI_RS;
    dl_config->number_pdus += 1;
  }
}


// This function schedules the PRACH according to prach_ConfigurationIndex and TS 38.211, tables 6.3.3.2.x
// PRACH formats 9, 10, 11 are corresponding to dual PRACH format configurations A1/B1, A2/B2, A3/B3.
// - todo:
// - Partial configuration is actually already stored in (fapi_nr_prach_config_t) &mac->phy_config.config_req->prach_config
static void nr_ue_prach_scheduler(module_id_t module_idP, frame_t frameP, sub_frame_t slotP)
{
  NR_UE_MAC_INST_t *mac = get_mac_inst(module_idP);
  RA_config_t *ra = &mac->ra;
  ra->RA_offset = 2; // to compensate the rx frame offset at the gNB
  if(ra->ra_state != GENERATE_PREAMBLE)
    return;

  fapi_nr_ul_config_request_t *ul_config = get_ul_config_request(mac, slotP, 0);
  if (!ul_config) {
    LOG_E(NR_MAC, "mac->ul_config is null! \n");
    return;
  }

  fapi_nr_ul_config_prach_pdu *prach_config_pdu;
  fapi_nr_config_request_t *cfg = &mac->phy_config.config_req;
  fapi_nr_prach_config_t *prach_config = &cfg->prach_config;
  nr_scheduled_response_t scheduled_response;

  NR_ServingCellConfigCommon_t *scc = mac->scc;
  NR_ServingCellConfigCommonSIB_t *scc_SIB = mac->scc_SIB;
  NR_RACH_ConfigCommon_t *setup = mac->current_UL_BWP.rach_ConfigCommon;
  NR_RACH_ConfigGeneric_t *rach_ConfigGeneric = &setup->rach_ConfigGeneric;

  NR_TDD_UL_DL_ConfigCommon_t *tdd_config = scc==NULL ? scc_SIB->tdd_UL_DL_ConfigurationCommon : scc->tdd_UL_DL_ConfigurationCommon;

  if (is_nr_UL_slot(tdd_config, slotP, mac->frame_type)) {

    // WIP Need to get the proper selected ssb_idx
    //     Initial beam selection functionality is not available yet
    uint8_t selected_gnb_ssb_idx = mac->mib_ssb;

    // Get any valid PRACH occasion in the current slot for the selected SSB index
    prach_occasion_info_t *prach_occasion_info_p;
    int is_nr_prach_slot = get_nr_prach_info_from_ssb_index(selected_gnb_ssb_idx,
                                                            (int)frameP,
                                                            (int)slotP,
                                                            &mac->ssb_list,
                                                            &prach_occasion_info_p);

    if (is_nr_prach_slot) {
      AssertFatal(NULL != prach_occasion_info_p,"PRACH Occasion Info not returned in a valid NR Prach Slot\n");

      init_RA(module_idP, &ra->prach_resources, setup, rach_ConfigGeneric, ra->rach_ConfigDedicated);
      nr_get_RA_window(mac);

      uint16_t format = prach_occasion_info_p->format;
      uint16_t format0 = format & 0xff;        // single PRACH format
      uint16_t format1 = (format >> 8) & 0xff; // dual PRACH format
      AssertFatal(ul_config->number_pdus < sizeof(ul_config->ul_config_list) / sizeof(ul_config->ul_config_list[0]),
                  "Number of PDUS in ul_config = %d > ul_config_list num elements", ul_config->number_pdus);

      pthread_mutex_lock(&ul_config->mutex_ul_config);
      AssertFatal(ul_config->number_pdus<FAPI_NR_UL_CONFIG_LIST_NUM, "ul_config->number_pdus %d out of bounds\n",ul_config->number_pdus);
      prach_config_pdu = &ul_config->ul_config_list[ul_config->number_pdus].prach_config_pdu;
      memset(prach_config_pdu, 0, sizeof(fapi_nr_ul_config_prach_pdu));
      fill_ul_config(ul_config, frameP, slotP, FAPI_NR_UL_CONFIG_TYPE_PRACH);
      pthread_mutex_unlock(&ul_config->mutex_ul_config);
      LOG_D(PHY, "In %s: (%p) %d UL PDUs:\n", __FUNCTION__, ul_config, ul_config->number_pdus);

      memset(prach_config_pdu, 0, sizeof(fapi_nr_ul_config_prach_pdu));

      uint16_t ncs = get_NCS(rach_ConfigGeneric->zeroCorrelationZoneConfig, format0, setup->restrictedSetConfig);

      prach_config_pdu->phys_cell_id = mac->physCellId;
      prach_config_pdu->num_prach_ocas = 1;
      prach_config_pdu->prach_slot = prach_occasion_info_p->slot;
      prach_config_pdu->prach_start_symbol = prach_occasion_info_p->start_symbol;
      prach_config_pdu->num_ra = prach_occasion_info_p->fdm;

      prach_config_pdu->num_cs = ncs;
      prach_config_pdu->root_seq_id = prach_config->num_prach_fd_occasions_list[prach_occasion_info_p->fdm].prach_root_sequence_index;
      prach_config_pdu->restricted_set = prach_config->restricted_set_config;
      prach_config_pdu->freq_msg1 = prach_config->num_prach_fd_occasions_list[prach_occasion_info_p->fdm].k1;

      LOG_I(NR_MAC,"PRACH scheduler: Selected RO Frame %u, Slot %u, Symbol %u, Fdm %u\n",
            frameP, prach_config_pdu->prach_slot, prach_config_pdu->prach_start_symbol, prach_config_pdu->num_ra);

      // Search which SSB is mapped in the RO (among all the SSBs mapped to this RO)
      for (int ssb_nb_in_ro=0; ssb_nb_in_ro<prach_occasion_info_p->nb_mapped_ssb; ssb_nb_in_ro++) {
        if (prach_occasion_info_p->mapped_ssb_idx[ssb_nb_in_ro] == selected_gnb_ssb_idx) {
          ra->ssb_nb_in_ro = ssb_nb_in_ro;
          break;
        }
      }
      AssertFatal(ra->ssb_nb_in_ro<prach_occasion_info_p->nb_mapped_ssb, "%u not found in the mapped SSBs to the PRACH occasion", selected_gnb_ssb_idx);

      if (format1 != 0xff) {
        switch(format0) { // dual PRACH format
          case 0xa1:
            prach_config_pdu->prach_format = 11;
            break;
          case 0xa2:
            prach_config_pdu->prach_format = 12;
            break;
          case 0xa3:
            prach_config_pdu->prach_format = 13;
            break;
        default:
          AssertFatal(1 == 0, "Only formats A1/B1 A2/B2 A3/B3 are valid for dual format");
        }
      } else {
        switch(format0) { // single PRACH format
          case 0:
            prach_config_pdu->prach_format = 0;
            break;
          case 1:
            prach_config_pdu->prach_format = 1;
            break;
          case 2:
            prach_config_pdu->prach_format = 2;
            break;
          case 3:
            prach_config_pdu->prach_format = 3;
            break;
          case 0xa1:
            prach_config_pdu->prach_format = 4;
            break;
          case 0xa2:
            prach_config_pdu->prach_format = 5;
            break;
          case 0xa3:
            prach_config_pdu->prach_format = 6;
            break;
          case 0xb1:
            prach_config_pdu->prach_format = 7;
            break;
          case 0xb4:
            prach_config_pdu->prach_format = 8;
            break;
          case 0xc0:
            prach_config_pdu->prach_format = 9;
            break;
          case 0xc2:
            prach_config_pdu->prach_format = 10;
            break;
          default:
            AssertFatal(1 == 0, "Invalid PRACH format");
        }
      } // if format1

      nr_get_prach_resources(module_idP, 0, 0, &ra->prach_resources, ra->rach_ConfigDedicated);
      prach_config_pdu->ra_PreambleIndex = ra->ra_PreambleIndex;
      prach_config_pdu->prach_tx_power = get_prach_tx_power(module_idP);
      set_ra_rnti(mac, prach_config_pdu);

      fill_scheduled_response(&scheduled_response, NULL, ul_config, NULL, NULL,NULL,module_idP, 0 /*TBR fix*/, frameP, slotP, NULL);
      if(mac->if_module != NULL && mac->if_module->scheduled_response != NULL)
        mac->if_module->scheduled_response(&scheduled_response);

      nr_Msg1_transmitted(module_idP);
    } // is_nr_prach_slot
  } // if is_nr_UL_slot
}

#define MAX_LCID 8 // NR_MAX_NUM_LCID shall be used but the mac_rlc_data_req function can fetch data for max 8 LCID
typedef struct {
  uint8_t bsr_len;
  uint8_t bsr_ce_len;
  uint8_t bsr_header_len;
  uint8_t phr_len;
  uint8_t phr_ce_len;
  uint8_t phr_header_len;
  uint16_t sdu_length_total;
  union {
    NR_BSR_SHORT *bsr_s;
    NR_SL_BSR_SHORT *sl_bsr_s;
  };
  NR_BSR_LONG *bsr_l;
  union {
    NR_BSR_SHORT *bsr_t;
    NR_SL_BSR_SHORT *sl_bsr_t;
  };
  //NR_POWER_HEADROOM_CMD *phr_pr;
  int tot_mac_ce_len;
  uint8_t total_mac_pdu_header_len;
} NR_UE_MAC_CE_INFO;

/*
nr_ue_get_sdu_mac_ce_pre finds length in various mac_ce field
Need nothing from mac_ce_p:
Update the following in mac_ce_p:
	bsr_len;
	bsr_ce_len;
	bsr_header_len;
	phr_len; TBD
	phr_ce_len; TBD
	phr_header_len; TBD
*/
int nr_ue_get_sdu_mac_ce_pre(module_id_t module_idP,
                      int CC_id,
                      frame_t frameP,
                      sub_frame_t subframe,
                      uint8_t gNB_index,
                      uint8_t *ulsch_buffer,
                      uint16_t buflen,
                      NR_UE_MAC_CE_INFO *mac_ce_p) {
  NR_UE_MAC_INST_t *mac = get_mac_inst(module_idP);

  int num_lcg_id_with_data = 0;
  // Preparing the MAC CEs sub-PDUs and get the total size
  mac_ce_p->bsr_header_len = 0;
  mac_ce_p->phr_header_len = 0;   //sizeof(SCH_SUBHEADER_FIXED);
  int lcg_id = 0;
  while (lcg_id < NR_MAX_NUM_LCGID) {
    if (mac->scheduling_info.BSR_bytes[lcg_id]) {
      num_lcg_id_with_data++;
    }

    lcg_id++;
  }

  //Restart ReTxBSR Timer at new grant indication (38.321)
  if (mac->scheduling_info.retxBSR_SF != MAC_UE_BSR_TIMER_NOT_RUNNING) {
    mac->scheduling_info.retxBSR_SF = nr_get_sf_retxBSRTimer(mac->scheduling_info.retxBSR_Timer);
  }

  // periodicBSR-Timer expires, trigger BSR
  if ((mac->scheduling_info.periodicBSR_Timer != NR_BSR_Config__periodicBSR_Timer_infinity)
      && (mac->scheduling_info.periodicBSR_SF == 0)) {
    // Trigger BSR Periodic
    mac->BSR_reporting_active |= NR_BSR_TRIGGER_PERIODIC;
    LOG_D(NR_MAC, "[UE %d] MAC BSR Triggered PeriodicBSR Timer expiry at frame%d subframe %d TBS=%d\n",
          module_idP, frameP, subframe, buflen);
  }

  //Compute BSR Length if Regular or Periodic BSR is triggered
  //WARNING: if BSR long is computed, it may be changed to BSR short during or after multiplexing if there remains less than 1 LCGROUP with data after Tx
  if (mac->BSR_reporting_active) {
    AssertFatal((mac->BSR_reporting_active & NR_BSR_TRIGGER_PADDING) == 0,
                "Inconsistent BSR Trigger=%d !\n",
                mac->BSR_reporting_active);

    uint8_t size_bsr = get_softmodem_params()->sl_mode ? sizeof(NR_SL_BSR_SHORT) : sizeof(NR_BSR_SHORT);
    //A Regular or Periodic BSR can only be sent if TBS is sufficient as transmitting only a BSR is not allowed if UE has data to transmit
    if (num_lcg_id_with_data <= 1) {
      if (buflen >= (size_bsr + sizeof(NR_MAC_SUBHEADER_FIXED) + 1)) {
        mac_ce_p->bsr_ce_len = size_bsr;
        mac_ce_p->bsr_header_len = sizeof(NR_MAC_SUBHEADER_FIXED); //1 byte
      }
    } else {
      if (buflen >= (num_lcg_id_with_data+1+sizeof(NR_MAC_SUBHEADER_SHORT)+1)) {
        mac_ce_p->bsr_ce_len = num_lcg_id_with_data * size_bsr + 1; //variable size
        mac_ce_p->bsr_header_len = sizeof(NR_MAC_SUBHEADER_SHORT); //2 bytes
      }
    }
  }

  mac_ce_p->bsr_len = mac_ce_p->bsr_ce_len + mac_ce_p->bsr_header_len;
  return (mac_ce_p->bsr_len + mac_ce_p->phr_len);
}

/*
nr_ue_get_sdu_mac_ce_post recalculates length and prepares the mac_ce field
Need the following from mac_ce_p:
	bsr_ce_len
	bsr_len
	sdu_length_total
	total_mac_pdu_header_len
Update the following in mac_ce_p:
	bsr_ce_len
	bsr_header_len
	bsr_len
	tot_mac_ce_len
	total_mac_pdu_header_len
	bsr_s
	bsr_l
	bsr_t
*/
void nr_ue_get_sdu_mac_ce_post(module_id_t module_idP,
                      int CC_id,
                      frame_t frameP,
                      sub_frame_t subframe,
                      uint8_t gNB_index,
                      uint8_t *ulsch_buffer,
                      uint16_t buflen,
                      NR_UE_MAC_CE_INFO *mac_ce_p) {
  NR_UE_MAC_INST_t *mac = get_mac_inst(module_idP);

  // Compute BSR Values and update Nb LCGID with data after multiplexing
  unsigned short padding_len = 0;
  uint8_t lcid = 0;
  int lcg_id = 0;
  int num_lcg_id_with_data = 0;
  int lcg_id_bsr_trunc = 0;
  for (lcg_id = 0; lcg_id < NR_MAX_NUM_LCGID; lcg_id++) {
	if (mac_ce_p->bsr_ce_len == sizeof(NR_BSR_SHORT)) {
      mac->scheduling_info.BSR[lcg_id] = nr_locate_BsrIndexByBufferSize(NR_SHORT_BSR_TABLE, NR_SHORT_BSR_TABLE_SIZE, mac->scheduling_info.BSR_bytes[lcg_id]);
	} else {
      mac->scheduling_info.BSR[lcg_id] = nr_locate_BsrIndexByBufferSize(NR_LONG_BSR_TABLE, NR_LONG_BSR_TABLE_SIZE, mac->scheduling_info.BSR_bytes[lcg_id]);
	}
    if (mac->scheduling_info.BSR_bytes[lcg_id]) {
      num_lcg_id_with_data++;
      lcg_id_bsr_trunc = lcg_id;
    }
  }

  // TS 38.321 Section 5.4.5
  // Check BSR padding: it is done after PHR according to Logical Channel Prioritization order
  // Check for max padding size, ie MAC Hdr for last RLC PDU = 1
  /* For Padding BSR:
     -  if the number of padding bits is equal to or larger than the size of the Short BSR plus its subheader but smaller than the size of the Long BSR plus its subheader:
     -  if more than one LCG has data available for transmission in the TTI where the BSR is transmitted: report Truncated BSR of the LCG with the highest priority logical channel with data available for transmission;
     -  else report Short BSR.
     -  else if the number of padding bits is equal to or larger than the size of the Long BSR plus its subheader, report Long BSR.
   */
  if (mac_ce_p->sdu_length_total) {
    padding_len = buflen - (mac_ce_p->total_mac_pdu_header_len + mac_ce_p->sdu_length_total);
  }

  if ((padding_len) && (mac_ce_p->bsr_len == 0)) {
    /* if the number of padding bits is equal to or larger than the size of the Long BSR plus its subheader, report Long BSR */
    if (padding_len >= (num_lcg_id_with_data+1+sizeof(NR_MAC_SUBHEADER_SHORT))) {
      mac_ce_p->bsr_ce_len = num_lcg_id_with_data + 1; //variable size
      mac_ce_p->bsr_header_len = sizeof(NR_MAC_SUBHEADER_SHORT); //2 bytes
      // Trigger BSR Padding
      mac->BSR_reporting_active |= NR_BSR_TRIGGER_PADDING;
    } else if (padding_len >= (sizeof(NR_BSR_SHORT)+sizeof(NR_MAC_SUBHEADER_FIXED))) {
      mac_ce_p->bsr_ce_len = sizeof(NR_BSR_SHORT); //1 byte
      mac_ce_p->bsr_header_len = sizeof(NR_MAC_SUBHEADER_FIXED); //1 byte

      if (num_lcg_id_with_data > 1) {
        // REPORT SHORT TRUNCATED BSR
        //Get LCGID of highest priority LCID with data (todo)
        for (lcid = DCCH; lcid < NR_MAX_NUM_LCID; lcid++) {
          lcg_id = mac->scheduling_info.LCGID[lcid];
          if ((lcg_id < NR_MAX_NUM_LCGID) && (mac->scheduling_info.BSR_bytes[lcg_id])) {
            lcg_id_bsr_trunc = lcg_id;
          }
        }
      } else {
        //Report SHORT BSR, clear bsr_t
        mac_ce_p->bsr_t = NULL;
      }

      // Trigger BSR Padding
      mac->BSR_reporting_active |= NR_BSR_TRIGGER_PADDING;
    }

    mac_ce_p->bsr_len = mac_ce_p->bsr_header_len + mac_ce_p->bsr_ce_len;
    mac_ce_p->tot_mac_ce_len += mac_ce_p->bsr_len;
    mac_ce_p->total_mac_pdu_header_len += mac_ce_p->bsr_len;
  }

  //Fill BSR Infos
  if (mac_ce_p->bsr_ce_len == 0) {
    mac_ce_p->bsr_s = NULL;
    mac_ce_p->bsr_l = NULL;
    mac_ce_p->bsr_t = NULL;
  } else if (mac_ce_p->bsr_header_len == sizeof(NR_MAC_SUBHEADER_SHORT)) {
    mac_ce_p->bsr_s = NULL;
    mac_ce_p->bsr_t = NULL;
    mac_ce_p->bsr_l->Buffer_size0 = mac->scheduling_info.BSR[0];
    mac_ce_p->bsr_l->Buffer_size1 = mac->scheduling_info.BSR[1];
    mac_ce_p->bsr_l->Buffer_size2 = mac->scheduling_info.BSR[2];
    mac_ce_p->bsr_l->Buffer_size3 = mac->scheduling_info.BSR[3];
    mac_ce_p->bsr_l->Buffer_size4 = mac->scheduling_info.BSR[4];
    mac_ce_p->bsr_l->Buffer_size5 = mac->scheduling_info.BSR[5];
    mac_ce_p->bsr_l->Buffer_size6 = mac->scheduling_info.BSR[6];
    mac_ce_p->bsr_l->Buffer_size7 = mac->scheduling_info.BSR[7];
    LOG_D(NR_MAC, "[UE %d] Frame %d subframe %d BSR Trig=%d report LONG BSR (level LCGID0 %d,level LCGID1 %d,level LCGID2 %d,level LCGID3 %d level LCGID4 %d,level LCGID5 %d,level LCGID6 %d,level LCGID7 %d)\n",
          module_idP, frameP, subframe,
          mac->BSR_reporting_active,
          mac->scheduling_info.BSR[0],
          mac->scheduling_info.BSR[1],
          mac->scheduling_info.BSR[2],
          mac->scheduling_info.BSR[3],
          mac->scheduling_info.BSR[4],
          mac->scheduling_info.BSR[5],
          mac->scheduling_info.BSR[6],
          mac->scheduling_info.BSR[7]);
  } else if (mac_ce_p->bsr_header_len == sizeof(NR_MAC_SUBHEADER_FIXED)) {
    mac_ce_p->bsr_l = NULL;

    if ((mac_ce_p->bsr_t != NULL) && (mac->BSR_reporting_active & NR_BSR_TRIGGER_PADDING)) {
      //Truncated BSR
      mac_ce_p->bsr_s = NULL;
      mac_ce_p->bsr_t->LcgID = lcg_id_bsr_trunc;
      mac_ce_p->bsr_t->Buffer_size = mac->scheduling_info.BSR[lcg_id_bsr_trunc];
      LOG_D(NR_MAC, "[UE %d] Frame %d subframe %d BSR Trig=%d report TRUNCATED BSR with level %d for LCGID %d\n",
            module_idP, frameP, subframe,
            mac->BSR_reporting_active,
            mac->scheduling_info.BSR[lcg_id_bsr_trunc], lcg_id_bsr_trunc);
    } else {
      mac_ce_p->bsr_t = NULL;
      mac_ce_p->bsr_s->LcgID = lcg_id_bsr_trunc;
      mac_ce_p->bsr_s->Buffer_size = mac->scheduling_info.BSR[lcg_id_bsr_trunc];
      LOG_D(NR_MAC, "[UE %d] Frame %d subframe %d BSR Trig=%d report SHORT BSR with level %d for LCGID %d\n",
            module_idP, frameP, subframe,
            mac->BSR_reporting_active,
            mac->scheduling_info.BSR[lcg_id_bsr_trunc], lcg_id_bsr_trunc);
    }
  }

  LOG_D(NR_MAC, "[UE %d][SR] Gave SDU to PHY, clearing any scheduling request\n", module_idP);
  mac->scheduling_info.SR_pending = 0;
  mac->scheduling_info.SR_COUNTER = 0;

  /* Actions when a BSR is sent */
  if (mac_ce_p->bsr_ce_len) {
    LOG_D(NR_MAC, "[UE %d] MAC BSR Sent !! bsr (ce%d,hdr%d) buff_len %d\n",
          module_idP, mac_ce_p->bsr_ce_len, mac_ce_p->bsr_header_len, buflen);
    // Reset ReTx BSR Timer
    mac->scheduling_info.retxBSR_SF = nr_get_sf_retxBSRTimer(mac->scheduling_info.retxBSR_Timer);
    LOG_D(NR_MAC, "[UE %d] MAC ReTx BSR Timer Reset =%d\n", module_idP, mac->scheduling_info.retxBSR_SF);

    // Reset Periodic Timer except when BSR is truncated
    if ((mac_ce_p->bsr_t == NULL) && (mac->scheduling_info.periodicBSR_Timer != NR_BSR_Config__periodicBSR_Timer_infinity)) {
      mac->scheduling_info.periodicBSR_SF = nr_get_sf_periodicBSRTimer(mac->scheduling_info.periodicBSR_Timer);
      LOG_D(NR_MAC, "[UE %d] MAC Periodic BSR Timer Reset =%d\n",
            module_idP,
            mac->scheduling_info.periodicBSR_SF);
    }

    // Reset BSR Trigger flags
    mac->BSR_reporting_active = BSR_TRIGGER_NONE;
  }
}

/**
 * Function:      to fetch data to be transmitted from RLC, place it in the ULSCH PDU buffer
                  to generate the complete MAC PDU with sub-headers and MAC CEs according to ULSCH MAC PDU generation (6.1.2 TS 38.321)
                  the selected sub-header for the payload sub-PDUs is NR_MAC_SUBHEADER_LONG
 * @module_idP    Module ID
 * @CC_id         Component Carrier index
 * @frameP        current UL frame
 * @subframe      current UL slot
 * @gNB_index     gNB index
 * @ulsch_buffer  Pointer to ULSCH PDU
 * @buflen        TBS
 */
uint8_t nr_ue_get_sdu(module_id_t module_idP,
                      int CC_id,
                      frame_t frameP,
                      sub_frame_t subframe,
                      uint8_t gNB_index,
                      uint8_t *ulsch_buffer,
                      uint16_t buflen) {
  NR_UE_MAC_CE_INFO mac_ce_info;
  NR_UE_MAC_CE_INFO *mac_ce_p=&mac_ce_info;
  int16_t buflen_remain = 0;
  mac_ce_p->bsr_len = 0;
  mac_ce_p->bsr_ce_len = 0;
  mac_ce_p->bsr_header_len = 0;
  mac_ce_p->phr_len = 0;
  //mac_ce_p->phr_ce_len = 0;
  //mac_ce_p->phr_header_len = 0;

  uint8_t lcid = 0;
  uint16_t sdu_length = 0;
  uint16_t num_sdus = 0;
  mac_ce_p->sdu_length_total = 0;
  NR_BSR_SHORT bsr_short, bsr_truncated;
  NR_BSR_LONG bsr_long;
  mac_ce_p->bsr_s = &bsr_short;
  mac_ce_p->bsr_l = &bsr_long;
  mac_ce_p->bsr_t = &bsr_truncated;
  //NR_POWER_HEADROOM_CMD phr;
  //mac_ce_p->phr_p = &phr;
  NR_UE_MAC_INST_t *mac = get_mac_inst(module_idP);
  //int highest_priority = 16;
  const uint8_t sh_size = sizeof(NR_MAC_SUBHEADER_LONG);

  // Pointer used to build the MAC PDU by placing the RLC SDUs in the ULSCH buffer
  uint8_t *pdu = ulsch_buffer;

  //nr_ue_get_sdu_mac_ce_pre updates all mac_ce related header field related to length
  mac_ce_p->tot_mac_ce_len = nr_ue_get_sdu_mac_ce_pre(module_idP, CC_id, frameP, subframe, gNB_index, ulsch_buffer, buflen, mac_ce_p);
  mac_ce_p->total_mac_pdu_header_len = mac_ce_p->tot_mac_ce_len;

  LOG_D(NR_MAC, "In %s: [UE %d] [%d.%d] process UL transport block at with size TBS = %d bytes \n", __FUNCTION__, module_idP, frameP, subframe, buflen);

  // Check for DCCH first
  // TO DO: Multiplex in the order defined by the logical channel prioritization
  for (lcid = UL_SCH_LCID_SRB1; lcid < MAX_LCID; lcid++) {

    buflen_remain = buflen - (mac_ce_p->total_mac_pdu_header_len + mac_ce_p->sdu_length_total + sh_size);

    LOG_D(NR_MAC, "In %s: [UE %d] [%d.%d] UL-DXCH -> ULSCH, RLC with LCID 0x%02x (TBS %d bytes, sdu_length_total %d bytes, MAC header len %d bytes, buflen_remain %d bytes)\n",
          __FUNCTION__,
          module_idP,
          frameP,
          subframe,
          lcid,
          buflen,
          mac_ce_p->sdu_length_total,
          mac_ce_p->tot_mac_ce_len,
          buflen_remain);

    while (buflen_remain > 0){

      // Pointer used to build the MAC sub-PDU headers in the ULSCH buffer for each SDU
      NR_MAC_SUBHEADER_LONG *header = (NR_MAC_SUBHEADER_LONG *) pdu;

      pdu += sh_size;

      sdu_length = mac_rlc_data_req(module_idP,
                                    mac->crnti,
                                    gNB_index,
                                    frameP,
                                    ENB_FLAG_NO,
                                    MBMS_FLAG_NO,
                                    lcid,
                                    buflen_remain,
                                    (char *)pdu,
                                    0,
                                    0);

      AssertFatal(buflen_remain >= sdu_length, "In %s: LCID = 0x%02x RLC has segmented %d bytes but MAC has max %d remaining bytes\n",
                  __FUNCTION__,
                  lcid,
                  sdu_length,
                  buflen_remain);

      if (sdu_length > 0) {

        LOG_D(NR_MAC, "In %s: [UE %d] [%d.%d] UL-DXCH -> ULSCH, Generating UL MAC sub-PDU for SDU %d, length %d bytes, RB with LCID 0x%02x (buflen (TBS) %d bytes)\n",
          __FUNCTION__,
          module_idP,
          frameP,
          subframe,
          num_sdus + 1,
          sdu_length,
          lcid,
          buflen);

        header->R = 0;
        header->F = 1;
        header->LCID = lcid;
        header->L = htons(sdu_length);

        #ifdef ENABLE_MAC_PAYLOAD_DEBUG
        LOG_I(NR_MAC, "In %s: dumping MAC sub-header with length %d: \n", __FUNCTION__, sh_size);
        log_dump(NR_MAC, header, sh_size, LOG_DUMP_CHAR, "\n");
        LOG_I(NR_MAC, "In %s: dumping MAC SDU with length %d \n", __FUNCTION__, sdu_length);
        log_dump(NR_MAC, pdu, sdu_length, LOG_DUMP_CHAR, "\n");
        #endif

        pdu += sdu_length;
        mac_ce_p->sdu_length_total += sdu_length;
        mac_ce_p->total_mac_pdu_header_len += sh_size;

        num_sdus++;

      } else {
        pdu -= sh_size;
        LOG_D(NR_MAC, "In %s: no data to transmit for RB with LCID 0x%02x\n", __FUNCTION__, lcid);
        break;
      }

      buflen_remain = buflen - (mac_ce_p->total_mac_pdu_header_len + mac_ce_p->sdu_length_total + sh_size);

      //Update Buffer remain and BSR bytes after transmission
      mac->scheduling_info.LCID_buffer_remain[lcid] -= sdu_length;
      mac->scheduling_info.BSR_bytes[mac->scheduling_info.LCGID[lcid]] -= sdu_length;
      LOG_D(NR_MAC, "[UE %d] Update BSR [%d.%d] BSR_bytes for LCG%d=%d\n",
            module_idP, frameP, subframe, mac->scheduling_info.LCGID[lcid],
            mac->scheduling_info.BSR_bytes[mac->scheduling_info.LCGID[lcid]]);
      if (mac->scheduling_info.BSR_bytes[mac->scheduling_info.LCGID[lcid]] < 0)
        mac->scheduling_info.BSR_bytes[mac->scheduling_info.LCGID[lcid]] = 0;
    }
  }

  //nr_ue_get_sdu_mac_ce_post recalculates all mac_ce related header fields since buffer has been changed after mac_rlc_data_req.
  //Also, BSR padding is handled here after knowing mac_ce_p->sdu_length_total.
  nr_ue_get_sdu_mac_ce_post(module_idP, CC_id, frameP, subframe, gNB_index, ulsch_buffer, buflen, mac_ce_p);

  if (mac_ce_p->tot_mac_ce_len > 0) {

    LOG_D(NR_MAC, "In %s copying %d bytes of MAC CEs to the UL PDU \n", __FUNCTION__, mac_ce_p->tot_mac_ce_len);
    nr_write_ce_ulsch_pdu(pdu, mac, 0, NULL, mac_ce_p->bsr_t, mac_ce_p->bsr_s, mac_ce_p->bsr_l);
    pdu += (unsigned char) mac_ce_p->tot_mac_ce_len;

    #ifdef ENABLE_MAC_PAYLOAD_DEBUG
    LOG_I(NR_MAC, "In %s: dumping MAC CE with length tot_mac_ce_len %d: \n", __FUNCTION__, mac_ce_p->tot_mac_ce_len);
    log_dump(NR_MAC, mac_header_control_elements, mac_ce_p->tot_mac_ce_len, LOG_DUMP_CHAR, "\n");
    #endif

  }

  buflen_remain = buflen - (mac_ce_p->total_mac_pdu_header_len + mac_ce_p->sdu_length_total);

  // Compute final offset for padding and fill remainder of ULSCH with 0
  if (buflen_remain > 0) {

    LOG_D(NR_MAC, "In %s filling remainder %d bytes to the UL PDU \n", __FUNCTION__, buflen_remain);
    ((NR_MAC_SUBHEADER_FIXED *) pdu)->R = 0;
    ((NR_MAC_SUBHEADER_FIXED *) pdu)->LCID = UL_SCH_LCID_PADDING;

    #ifdef ENABLE_MAC_PAYLOAD_DEBUG
    LOG_I(NR_MAC, "In %s: padding MAC sub-header with length %ld bytes \n", __FUNCTION__, sizeof(NR_MAC_SUBHEADER_FIXED));
    log_dump(NR_MAC, pdu, sizeof(NR_MAC_SUBHEADER_FIXED), LOG_DUMP_CHAR, "\n");
    #endif

    pdu++;
    buflen_remain--;

    if (IS_SOFTMODEM_RFSIM) {
      for (int j = 0; j < buflen_remain; j++) {
        pdu[j] = (unsigned char) rand();
      }
    } else {
      memset(pdu, 0, buflen_remain);
    }

    #ifdef ENABLE_MAC_PAYLOAD_DEBUG
    LOG_I(NR_MAC, "In %s: MAC padding sub-PDU with length %d bytes \n", __FUNCTION__, buflen_remain);
    log_dump(NR_MAC, pdu, buflen_remain, LOG_DUMP_CHAR, "\n");
    #endif

  }

  #ifdef ENABLE_MAC_PAYLOAD_DEBUG
  LOG_I(NR_MAC, "In %s: dumping MAC PDU with length %d: \n", __FUNCTION__, buflen);
  log_dump(NR_MAC, ulsch_buffer, buflen, LOG_DUMP_CHAR, "\n");
  #endif

  return num_sdus > 0 ? 1 : 0;
}

void schedule_ta_command(fapi_nr_dl_config_request_t *dl_config, NR_UL_TIME_ALIGNMENT_t *ul_time_alignment)
{
  fapi_nr_ta_command_pdu *ta = &dl_config->dl_config_list[dl_config->number_pdus].ta_command_pdu;
  ta->ta_frame = ul_time_alignment->frame;
  ta->ta_slot = ul_time_alignment->slot;
  ta->ta_command = ul_time_alignment->ta_command;
  dl_config->dl_config_list[dl_config->number_pdus].pdu_type = FAPI_NR_CONFIG_TA_COMMAND;
  dl_config->number_pdus += 1;
  ul_time_alignment->ta_apply = false;
}

uint16_t sl_adjust_ssb_indices(sl_ssb_timealloc_t *ssb_timealloc,
                               uint32_t slot_in_16frames,
                               uint16_t *ssb_slot_ptr) {

  uint16_t ssb_slot = ssb_timealloc->sl_TimeOffsetSSB;
  uint16_t numssb = 0;
  *ssb_slot_ptr = 0;

  if (ssb_timealloc->sl_NumSSB_WithinPeriod == 0) {
    *ssb_slot_ptr = 0;
    return 0;
  }

  while (slot_in_16frames > ssb_slot) {
    numssb = numssb + 1;
    if (numssb < ssb_timealloc->sl_NumSSB_WithinPeriod)
      ssb_slot = ssb_slot + ssb_timealloc->sl_TimeInterval;
    else
      break;
  }

  *ssb_slot_ptr = ssb_slot;

  return numssb;
}

/*
* This function calculates the indices based on the new timing (frame,slot)
* acquired by the UE.
* NUM SSB, SLOT_SSB needs to be calculated based on current timing
*/
void sl_adjust_indices_based_on_timing(uint32_t frame, uint32_t slot,
                                       uint32_t frame_tx, uint32_t slot_tx,
                                       uint16_t mod_id, uint16_t slots_per_frame)
{

  NR_UE_MAC_INST_t *mac = get_mac_inst(mod_id);
  sl_nr_ue_mac_params_t *sl_mac = mac->SL_MAC_PARAMS;

  uint16_t frame_16 = frame % SL_NR_SSB_REPETITION_IN_FRAMES;
  uint32_t slot_in_16frames = (frame_16 * slots_per_frame) + slot;
  uint16_t frame_tx_16 = frame_tx % SL_NR_SSB_REPETITION_IN_FRAMES;
  uint32_t slot_tx_in_16frames = (frame_tx_16 * slots_per_frame) + slot_tx;
  LOG_I(NR_MAC,"[UE%d]PSBCH params adjusted based on RX current timing %d:%d. frame_16:%d, slot_in_16frames:%d\n",
                                        mod_id, frame, slot, frame_16, slot_in_16frames);
  LOG_I(NR_MAC,"[UE%d]PSBCH params adjusted based on TX current timing %d:%d. frame_16:%d, slot_in_16frames:%d\n",
                                        mod_id, frame_tx, slot_tx, frame_tx_16, slot_tx_in_16frames);

  //Adjust PSBCH Indices based on current RX timing
  sl_ssb_timealloc_t *ssb_timealloc = &sl_mac->rx_sl_bch.ssb_time_alloc;
  sl_mac->rx_sl_bch.num_ssb = sl_adjust_ssb_indices(ssb_timealloc, slot_in_16frames, &sl_mac->rx_sl_bch.ssb_slot);

  //Adjust PSBCH Indices based on current TX timing
  ssb_timealloc = &sl_mac->tx_sl_bch.ssb_time_alloc;
  sl_mac->tx_sl_bch.num_ssb = sl_adjust_ssb_indices(ssb_timealloc, slot_tx_in_16frames, &sl_mac->tx_sl_bch.ssb_slot);

  LOG_I(NR_MAC,"[UE%d]PSBCH params adjusted based on RX current timing %d:%d. NumSSB:%d, ssb_slot:%d\n",
                                                            mod_id, frame, slot, sl_mac->rx_sl_bch.num_ssb,
                                                            sl_mac->rx_sl_bch.ssb_slot);
  LOG_I(NR_MAC,"[UE%d]PSBCH params adjusted based on TX current timing %d:%d. NumSSB:%d, ssb_slot:%d\n",
                                                            mod_id, frame_tx, slot_tx, sl_mac->tx_sl_bch.num_ssb,
                                                            sl_mac->tx_sl_bch.ssb_slot);

}

/*
  DETERMINE IF SLOT IS MARKED AS SSB SLOT
  ACCORDING TO THE SSB TIME ALLOCATION PARAMETERS.
  sl_numSSB_withinPeriod - NUM SSBS in 16frames
  sl_timeoffset_SSB - time offset for first SSB at start of 16 frames cycle
  sl_timeinterval - distance in slots between 2 SSBs
*/
uint8_t sl_determine_if_SSB_slot(uint16_t frame, uint16_t slot, uint16_t slots_per_frame,
                                 sl_bch_params_t *sl_bch,
                                 sl_sidelink_slot_type_t slot_type) {

  uint16_t frame_16 = frame % SL_NR_SSB_REPETITION_IN_FRAMES;
  uint32_t slot_in_16frames = (frame_16 * slots_per_frame) + slot;
  uint16_t sl_NumSSB_WithinPeriod = sl_bch->ssb_time_alloc.sl_NumSSB_WithinPeriod;
  uint16_t sl_TimeOffsetSSB = sl_bch->ssb_time_alloc.sl_TimeOffsetSSB;
  uint16_t sl_TimeInterval = sl_bch->ssb_time_alloc.sl_TimeInterval;
  uint16_t num_ssb = sl_bch->num_ssb, ssb_slot = sl_bch->ssb_slot;

  LOG_D(NR_MAC, "%d:%d. slot_type:%d, num_ssb:%d,ssb_slot:%d, %d-%d-%d, status:%d\n",
                                             frame, slot, slot_type,
                                             sl_bch->num_ssb,sl_bch->ssb_slot,
                                             sl_NumSSB_WithinPeriod, sl_TimeOffsetSSB, sl_TimeInterval, sl_bch->status);

  if (sl_NumSSB_WithinPeriod && sl_bch->status) {

    if (slot_in_16frames == sl_TimeOffsetSSB) {
      num_ssb = 0;
      ssb_slot = sl_TimeOffsetSSB;
    }

    if (num_ssb < sl_NumSSB_WithinPeriod && slot_in_16frames == ssb_slot) {

      num_ssb += 1;
      ssb_slot = (num_ssb < sl_NumSSB_WithinPeriod)
                ? (ssb_slot + sl_TimeInterval) : sl_TimeOffsetSSB;

      //Update the time when the same slot type is called
      if ((slot_type == SIDELINK_SLOT_TYPE_RX) || (slot_type == SIDELINK_SLOT_TYPE_TX)) {
        sl_bch->ssb_slot = ssb_slot;
        sl_bch->num_ssb = num_ssb;
      }

      LOG_D(NR_MAC, "%d:%d is a PSBCH SLOT. Slot type:%d Next PSBCH Slot:%d, num_ssb:%d\n",
                                                frame, slot, slot_type,
                                                sl_bch->ssb_slot,sl_bch->num_ssb);

      return 1;
    }
  }

  LOG_D(NR_MAC, "%d:%d is NOT a PSBCH SLOT. Next PSBCH Slot:%d, num_ssb:%d\n",
                                             frame, slot, sl_bch->ssb_slot,sl_bch->num_ssb);
  return 0;
}


static void nr_store_slsch_buffer(NR_UE_MAC_INST_t *mac, frame_t frame, sub_frame_t slot) {

  NR_SL_UEs_t *UE_info = &mac->sl_info;
  SL_UE_iterator(UE_info->list, UE) {
    NR_SL_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
    sched_ctrl->num_total_bytes = 0;
    sched_ctrl->sl_pdus_total = 0;

    const int lcid = 4; //Jin : origin 4,  test 1
    sched_ctrl->rlc_status[lcid] = mac_rlc_status_ind(0, mac->src_id, 0, frame, slot, ENB_FLAG_NO, MBMS_FLAG_NO, lcid, 0, 0);
 
    if (sched_ctrl->rlc_status[lcid].bytes_in_buffer == 0)
        continue;

    sched_ctrl->sl_pdus_total += sched_ctrl->rlc_status[lcid].pdus_in_buffer;
    sched_ctrl->num_total_bytes += sched_ctrl->rlc_status[lcid].bytes_in_buffer;
    LOG_D(MAC,
          "[%4d.%2d] SLSCH, RLC status for UE: %d bytes in buffer, total DL buffer size = %d bytes, %d total PDU bytes\n",
          frame,
          slot,
          sched_ctrl->rlc_status[lcid].bytes_in_buffer,
          sched_ctrl->num_total_bytes,
          sched_ctrl->sl_pdus_total);
  }
}

static bool get_control_info(NR_UE_MAC_INST_t *mac,
                             NR_SL_UE_sched_ctrl_t *sched_ctrl,
                             const int nr_slots_per_frame,
                             uint16_t frame,
                             uint16_t slot,
                             int16_t dest_id,
                             NR_SetupRelease_SL_PSFCH_Config_r16_t *configured_PSFCH) {
  int period = 0, offset = 0;
  sl_nr_ue_mac_params_t *sl_mac = mac->SL_MAC_PARAMS;
  // Determine current slot is csi-rs schedule slot
  bool csi_acq = !mac->SL_MAC_PARAMS->sl_CSI_Acquisition;
  bool is_harq_feedback = configured_PSFCH ? is_feedback_scheduled(mac, frame, slot) : false;
  NR_TDD_UL_DL_Pattern_t *tdd = &sl_mac->sl_TDD_config->pattern1;
  // Determine current slot is csi report schedule slot
  SL_CSI_Report_t *sl_csi_report = set_nr_ue_sl_csi_meas_periodicity(tdd, sched_ctrl, mac, dest_id, false);
  nr_ue_sl_csi_period_offset(sl_csi_report,
                              &period,
                              &offset);
  LOG_D(NR_MAC, "frame.slot %4d.%2d period %d offset %d\n", frame, slot, period, offset);
  bool csi_req_slot = !((nr_slots_per_frame * frame + slot - offset) % period);
  bool is_csi_report_sched_slot = ((sched_ctrl->sched_csi_report.frame == frame) &&
                                  (sched_ctrl->sched_csi_report.slot == slot));
  bool control_info = (is_harq_feedback || (csi_acq && csi_req_slot) || is_csi_report_sched_slot);

  LOG_D(NR_MAC, "frame.slot %4d.%2d harq_feedback %d, (csi_acq && csi_req_slot) %d, is_csi_report_sched_slot %d\n",
        frame, slot, is_harq_feedback, (csi_acq && csi_req_slot), is_csi_report_sched_slot);

  return control_info;
}

void preprocess(NR_UE_MAC_INST_t *mac,
                uint16_t frame,
                uint16_t slot,
                int *fb_frame,
                int *fb_slot,
                const NR_SL_BWP_ConfigCommon_r16_t *sl_bwp,
                NR_SetupRelease_SL_PSFCH_Config_r16_t *configured_PSFCH) {

  nr_store_slsch_buffer(mac, frame, slot);
  sl_nr_ue_mac_params_t *sl_mac = mac->SL_MAC_PARAMS;
  int scs = sl_mac->sl_phy_config.sl_config_req.sl_bwp_config.sl_scs;
  const int nr_slots_frame = nr_slots_per_frame[scs];

  NR_SL_UEs_t *UE_info = &mac->sl_info;
  SL_UE_iterator(UE_info->list, UE) {
    NR_SL_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
    UE->mac_sl_stats.sl.current_bytes = 0;
    UE->mac_sl_stats.sl.current_rbs = 0;
    NR_sched_pssch_t *sched_pssch = &sched_ctrl->sched_pssch;
    sched_pssch->sl_harq_pid = configured_PSFCH ? sched_ctrl->retrans_sl_harq.head : -1;

    /* retransmission */
    if (sched_pssch->sl_harq_pid >= 0) {
      if (sched_ctrl->available_sl_harq.head < 0) {
        LOG_W(NR_MAC, "[UE][%4d.%2d] UE has no free SL HARQ process, skipping\n",
              frame,
              slot);
        continue;
      } else {
         sched_ctrl->sched_csi_report.active = false;
      }
    } else {
      if (sched_ctrl->available_sl_harq.head < 0) {
        LOG_W(NR_MAC, "[UE][%4d.%2d] UE has no free SL HARQ process, skipping\n",
              frame,
              slot);
        continue;
      }
      bool control_info = get_control_info(mac, sched_ctrl, nr_slots_frame, frame, slot, UE->uid, configured_PSFCH);
      LOG_D(NR_MAC, "sched_ctrl->num_total_bytes %d, control_info %d\n", sched_ctrl->num_total_bytes, control_info);
      /* Check SL buffer and control info, skip this UE if no bytes and no control info */
      if (sched_ctrl->num_total_bytes == 0) {
        if (!control_info)
          continue;
      }
    }

    /*
    * SLSCH tx computes feedback frame and slot, which will be used by transmitter of PSFCH after receiving SLSCH.
    * Transmitter of SLSCH stores the feedback frame and slot in harq process to use those in retreiving the feedback.
    */
    if (configured_PSFCH) {
      const uint8_t psfch_periods[] = {0, 1, 2, 4};
      NR_SL_PSFCH_Config_r16_t *sl_psfch_config = mac->sl_tx_res_pool->sl_PSFCH_Config_r16->choice.setup;
      long psfch_period = (sl_psfch_config->sl_PSFCH_Period_r16)
                            ? psfch_periods[*sl_psfch_config->sl_PSFCH_Period_r16] : 0;

      int rcv_tx_frame = (frame + ((slot + DURATION_RX_TO_TX) / nr_slots_frame)) % 1024;
      int rcv_tx_slot = (slot + DURATION_RX_TO_TX) % nr_slots_frame;
      int psfch_slot = get_feedback_slot(psfch_period, rcv_tx_slot);
      update_harq_lists(mac, frame, slot, UE);
      *fb_frame = rcv_tx_frame;
      *fb_slot = psfch_slot;
      LOG_D(NR_MAC, "Tx SLSCH %4d.%2d, Expected Feedback: %4d.%2d in current PSFCH: psfch_period %ld\n",
            frame,
            slot,
            *fb_frame,
            *fb_slot,
            psfch_period);
    }
    int locbw = sl_bwp->sl_BWP_Generic_r16->sl_BWP_r16->locationAndBandwidth;
    sched_pssch->mu = scs;
    sched_pssch->frame = frame;
    sched_pssch->slot = slot;
    sched_pssch->rbSize = NRRIV2BW(locbw, MAX_BWP_SIZE);
    sched_pssch->rbStart = NRRIV2PRBOFFSET(locbw, MAX_BWP_SIZE);
  }
}

bool nr_ue_sl_pssch_scheduler(NR_UE_MAC_INST_t *mac,
                              nr_sidelink_indication_t *sl_ind,
                              const NR_SL_BWP_ConfigCommon_r16_t *sl_bwp,
                              const NR_SL_ResourcePool_r16_t *sl_res_pool,
                              sl_nr_tx_config_request_t *tx_config,
                              sl_resource_info_t *resource,
                              uint8_t *config_type) {

  uint16_t slot = sl_ind->slot_tx;
  uint16_t frame = sl_ind->frame_tx;
  int feedback_frame, feedback_slot;
  int lcid = 4;


  int sdu_length = 0;
  uint16_t sdu_length_total = 0;
  uint8_t total_mac_pdu_header_len = 0;
  bool is_resource_allocated = false;
  *config_type = 0;

  sl_nr_ue_mac_params_t* sl_mac_params = mac->SL_MAC_PARAMS;
  NR_SetupRelease_SL_PSFCH_Config_r16_t *configured_PSFCH  = mac->sl_tx_res_pool->sl_PSFCH_Config_r16;
  if ((frame & 127) == 0 && slot == 0) {
    print_meas(&mac->rlc_data_req,"rlc_data_req",NULL,NULL);
  }
  if (sl_ind->slot_type != SIDELINK_SLOT_TYPE_TX) return is_resource_allocated;

  if (slot > 9 && get_nrUE_params()->sync_ref) return is_resource_allocated;   
  if (slot < 10 && !get_nrUE_params()->sync_ref) return is_resource_allocated;   

  LOG_D(NR_MAC,"[UE%d] SL-PSSCH SCHEDULER: Frame:SLOT %d:%d, slot_type:%d\n",
        sl_ind->module_id, frame, slot,sl_ind->slot_type);

  uint16_t slsch_pdu_length_max;
  //tx_config->tx_config_list[0].tx_pscch_pssch_config_pdu.slsch_payload = mac->slsch_payload; //Jin comment for now, wrong

  NR_SL_UEs_t *UE_info = &mac->sl_info;

  if (*(UE_info->list) == NULL) {
    LOG_D(NR_MAC, "UE list is empty\n");
    return is_resource_allocated;
  }
  
  //Jin new add replace previous tx_conf_list[0]
  tx_config->number_pdus = 0;
  tx_config->sfn  = frame;
  tx_config->slot = slot; 
  //jin end
  
  preprocess(mac, frame, slot, &feedback_frame, &feedback_slot, sl_bwp, configured_PSFCH);
 
  /* ---------------- Jin TDMA: minimal, deterministic, slot-parity ---------------- */
  uint16_t chosen_uid = 0;

  if (get_nrUE_params()->sync_ref) {
    /* UE0 transmits in slots 0..9, alternate destination by slot parity */
    chosen_uid = (slot & 1) ? 2 : 1;   /* odd->UE2, even->UE1 */
  } else {
    const uint16_t owner_uid = (slot & 1) ? 2 : 1;  /* odd->UE2, even->UE1 */
    if (mac->src_id != owner_uid)
      return false;                   /* not my TX slot */
    chosen_uid = 0;                   /* always send uplink to sync-ref */
  }

  LOG_D(NR_MAC, "[SL-TDMA-20] me=%u frame=%u slot=%u chosen_uid=%u\n",
        mac->src_id, frame, slot, chosen_uid);
  /* ---------------- end Jin TDMA ---------------- */
 


  SL_UE_iterator(UE_info->list, UE) {
    //Jin apply previous defined tdma slots
     if (UE->uid != chosen_uid)
      continue;
    // Ensure SCI2 destination matches the peer we are scheduling
    mac->sci2_pdu.source_id = mac->src_id;
    mac->sci2_pdu.dest_id   = UE->uid;
    //Jin end



      

    NR_mac_dir_stats_t *sl_mac_stats = &UE->mac_sl_stats.sl;
    NR_SL_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
    sl_mac_stats->current_bytes = 0;
    sl_mac_stats->current_rbs = 0;
    NR_sched_pssch_t *sched_pssch = &sched_ctrl->sched_pssch;
    int8_t harq_id = sched_pssch->sl_harq_pid;

        /*   
        LOG_I(NR_MAC,//jin log
          "[PSSCH-OPP-Jin] frame=%d slot=%d slot_type=%d sync_ref=%d rbSize=%d\n",
          frame, slot, sl_ind->slot_type, get_nrUE_params()->sync_ref,
          sched_pssch->rbSize);*/

    if (sched_pssch->rbSize <= 0)
      continue;

    //Jin add 
    const int pdu_idx = tx_config->number_pdus;
    AssertFatal(pdu_idx < 8,
                "tx_config_list overflow: pdu_idx=%d\n", pdu_idx);

    NR_UE_sl_harq_t *cur_harq = NULL;

    if (harq_id < 0) {
      /* PP has not selected a specific HARQ Process, get a new one */
      harq_id = sched_ctrl->available_sl_harq.head;
      AssertFatal(harq_id >= 0,
                  "no free HARQ process available\n");
      remove_front_nr_list(&sched_ctrl->available_sl_harq);
      sched_pssch->sl_harq_pid = harq_id;
    } else {
      /* PP selected a specific HARQ process. Check whether it will be a new
      * transmission or a retransmission, and remove from the corresponding
      * list */
      if (sched_ctrl->sl_harq_processes[harq_id].round == 0)
        remove_nr_list(&sched_ctrl->available_sl_harq, harq_id);
      else
        remove_nr_list(&sched_ctrl->retrans_sl_harq, harq_id);
    }
    cur_harq = &sched_ctrl->sl_harq_processes[harq_id];
    DevAssert(!cur_harq->is_waiting);
    /* retransmission or bytes to send */
    //Jin disable retx
    /*
    if (configured_PSFCH && ((cur_harq->round != 0) || (sched_ctrl->num_total_bytes > 0))) {
      cur_harq->feedback_slot = feedback_slot;
      cur_harq->feedback_frame = feedback_frame;
      add_tail_nr_list(&sched_ctrl->feedback_sl_harq, harq_id);
      cur_harq->is_waiting = true;
      LOG_D(NR_MAC, "%4d.%2d Sending Data; Expecting feedback at %4d.%2d\n", frame, slot, feedback_frame, feedback_slot);
    }
    else
      add_tail_nr_list(&sched_ctrl->available_sl_harq, harq_id);
    cur_harq->sl_harq_pid = harq_id;
    */  //Jin end

   //Jin disable harq
        cur_harq->feedback_slot  = -1;
        cur_harq->feedback_frame = -1;
        cur_harq->is_waiting     = false;

        add_tail_nr_list(&sched_ctrl->available_sl_harq, harq_id);

        cur_harq->sl_harq_pid = harq_id;

        /*LOG_I(NR_MAC,
              "[SL-HARQ-TRACE] SET PID=%d waiting=%d round=%d feedback=%d.%d (tx=%d.%d)\n",
              harq_id, cur_harq->is_waiting, cur_harq->round,
              cur_harq->feedback_frame, cur_harq->feedback_slot,
              frame, slot);8?
        // --------------------------------------------------------------

  
    /*
    The encoder checks for a change in ndi value everytime, since sci2 changes with every transmission,
    we oscillate the ndi value so the encoder treats the data as new data everytime.
    */
    cur_harq->ndi ^= 1;

    nr_schedule_slsch(mac, frame, slot, &mac->sci1_pdu, &mac->sci2_pdu, NR_SL_SCI_FORMAT_2A,
                      UE, &slsch_pdu_length_max, cur_harq, &sched_ctrl->rlc_status[lcid], resource);

    //Jin comment
    /*
    *config_type = SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH;
    tx_config->number_pdus = 1;
    tx_config->sfn = frame;
    tx_config->slot = slot;
    tx_config->tx_config_list[0].pdu_type = *config_type;
    fill_pssch_pscch_pdu(sl_mac_params,
                        &tx_config->tx_config_list[0].tx_pscch_pssch_config_pdu,
                        sl_bwp,
                        sl_res_pool,
                        &mac->sci1_pdu,
                        &mac->sci2_pdu,
                        slsch_pdu_length_max,
                        NR_SL_SCI_FORMAT_1A,
                        NR_SL_SCI_FORMAT_2A,
                        slot,
                        resource);
    sl_nr_tx_config_pscch_pssch_pdu_t *pscch_pssch_pdu = &tx_config->tx_config_list[0].tx_pscch_pssch_config_pdu;
    */
    //Jin comment end, replace by following 
    *config_type = SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH;
    tx_config->tx_config_list[pdu_idx].pdu_type = *config_type;
    fill_pssch_pscch_pdu(sl_mac_params,
                        &tx_config->tx_config_list[pdu_idx].tx_pscch_pssch_config_pdu,
                        sl_bwp,
                        sl_res_pool,
                        &mac->sci1_pdu,
                        &mac->sci2_pdu,
                        slsch_pdu_length_max,
                        NR_SL_SCI_FORMAT_1A,
                        NR_SL_SCI_FORMAT_2A,
                        slot,
                        resource);
    sl_nr_tx_config_pscch_pssch_pdu_t *pscch_pssch_pdu =
        &tx_config->tx_config_list[pdu_idx].tx_pscch_pssch_config_pdu;
    /* We have now reserved one entry */
    tx_config->number_pdus = pdu_idx + 1;
    //Jin replacement end. apply our defined pdu_idx instead of list[0]
        
    sched_pssch->R = pscch_pssch_pdu->target_coderate;
    sched_pssch->tb_size = pscch_pssch_pdu->tb_size;
    sched_pssch->sl_harq_pid = mac->sci2_pdu.harq_pid;
    sched_pssch->nrOfLayers = pscch_pssch_pdu->num_layers;
    sched_pssch->mcs = pscch_pssch_pdu->mcs;
    sched_pssch->Qm = pscch_pssch_pdu->mod_order;

    LOG_D(NR_MAC, "PSSCH: %4d.%2d SL sched %4d.%2d start %2d RBS %3d MCS %2d nrOfLayers %2d TBS %4d HARQ PID %2d round %d NDI %d sched %6d\n",
          frame,
          slot,
          sched_pssch->frame,
          sched_pssch->slot,
          sched_pssch->rbStart,
          sched_pssch->rbSize,
          sched_pssch->mcs,
          sched_pssch->nrOfLayers,
          sched_pssch->tb_size,
          sched_pssch->sl_harq_pid,
          cur_harq->round,
          cur_harq->ndi,
          sched_ctrl->sched_sl_bytes);

    /* Statistics */
    AssertFatal(cur_harq->round < sl_mac_params->sl_bler.harq_round_max, "Indexing ulsch_rounds[%d] is out of bounds for max harq round %d\n", cur_harq->round, sl_mac_params->sl_bler.harq_round_max);

    sl_mac_stats->rounds[cur_harq->round]++;
    if (cur_harq->round != 0) { // retransmission
      LOG_D(NR_MAC,
            "PSSCH: %d.%2d SL retransmission sched %d.%2d HARQ PID %d round %d NDI %d\n",
            frame,
            slot,
            sched_pssch->frame,
            sched_pssch->slot,
            sched_pssch->sl_harq_pid,
            cur_harq->round,
            cur_harq->ndi);
      sl_mac_stats->total_rbs_retx += sched_pssch->rbSize;
    } else { // initial transmission

      UE->mac_sl_stats.slsch_total_bytes_scheduled += sched_pssch->tb_size;
      /* save which time allocation and nrOfLayers have been used, to be used on
      * retransmissions */
      cur_harq->sched_pssch.nrOfLayers = sched_pssch->nrOfLayers;
      sched_ctrl->sched_sl_bytes += sched_pssch->tb_size;
      sl_mac_stats->total_rbs += sched_pssch->rbSize;


      //int buflen = tx_config->tx_config_list[0].tx_pscch_pssch_config_pdu.tb_size; // Jin origin replaced
      int buflen = tx_config->tx_config_list[pdu_idx].tx_pscch_pssch_config_pdu.tb_size; //Jin replace 

      LOG_D(NR_MAC, "[UE%d] Initial TTI-%d:%d TX PSCCH_PSSCH REQ  TBS %d\n", sl_ind->module_id, frame, slot, buflen);

      uint8_t *pdu = (uint8_t *) cur_harq->transportBlock;
      int buflen_remain = buflen;
      uint8_t *base = pdu;//Jin add
      uint8_t *mac_pdu_base = pdu; //Jin add fix header problem

      NR_SLSCH_MAC_SUBHEADER_FIXED *sl_sch_subheader = (NR_SLSCH_MAC_SUBHEADER_FIXED *) pdu;
      sl_sch_subheader->V = 0;
      sl_sch_subheader->R = 0;
      /* --- JIN:  fix for wrong DST in SL-SCH subheader (sync-ref only) --- */
      /*
      if (mac->src_id == 0) { // sync-ref
        const uint16_t last_rx_src = mac->sci_pdu_rx.source_id;   // last decoded SCI2 source
        const uint16_t cur_dst     = mac->sci2_pdu.dest_id;

        LOG_I(NR_MAC,
              "[JIN][SL-SCH-HDR] pre: me=%u sci2_pdu SRC=%u DST=%u last_rx_src=%u\n",
              mac->src_id, mac->sci2_pdu.source_id, cur_dst, last_rx_src);

        if (last_rx_src != 0 && last_rx_src != mac->src_id && cur_dst != last_rx_src) {
          mac->sci2_pdu.dest_id = last_rx_src;
          LOG_I(NR_MAC,
                "[JIN][SL-SCH-HDR] override DST -> %u\n",
                mac->sci2_pdu.dest_id);
        }
      }
      */
      /* --- end JIN --- */
      sl_sch_subheader->SRC = mac->sci2_pdu.source_id;
      sl_sch_subheader->DST = mac->sci2_pdu.dest_id;      //sci_pdu->source_id jin this is as 2, but here is written as 1...
      LOG_D(NR_MAC, "[Jin Jin !!!!!SL-TX] About to TX: SCI2 SRC=%d DST=%d\n",sl_sch_subheader->SRC,sl_sch_subheader->DST);//Jin debug fixed srcID problem
      pdu += sizeof(NR_SLSCH_MAC_SUBHEADER_FIXED);
      LOG_D(NR_MAC, "%4d.%2d Tx V %d, R %d, SRC %d, DST %d\n", frame, slot, sl_sch_subheader->V, sl_sch_subheader->R, sl_sch_subheader->SRC, sl_sch_subheader->DST);
 


      buflen_remain -= sizeof(NR_SLSCH_MAC_SUBHEADER_FIXED);
      LOG_D(NR_MAC, "buflen_remain after adding SL_SCH_MAC_SUBHEADER_FIXED %d\n", buflen_remain);
      const uint8_t sh_size = sizeof(NR_MAC_SUBHEADER_LONG);

      int num_sdus=0;
      if (sched_ctrl->num_total_bytes > 0) {
        if (sched_ctrl->rlc_status[lcid].bytes_in_buffer > 0) {
          while (buflen_remain > sh_size + 1) {

            // Pointer used to build the MAC sub-PDU headers in the ULSCH buffer for each SDU
            NR_MAC_SUBHEADER_LONG *header = (NR_MAC_SUBHEADER_LONG *) pdu;
            pdu += sh_size;
            buflen_remain -= sh_size;
            //const rlc_buffer_occupancy_t ndata = min(sched_ctrl->rlc_status[lcid].bytes_in_buffer, buflen_remain);
            const rlc_buffer_occupancy_t ndata = buflen_remain;//Jin allow max

            start_meas(&mac->rlc_data_req);

            sdu_length = mac_rlc_data_req(0,
                                          mac->src_id,
                                          0,
                                          frame,
                                          ENB_FLAG_NO,
                                          MBMS_FLAG_NO,
                                          lcid,
                                          ndata,
                                          (char *)pdu,
                                          0,
                                          0);
            stop_meas(&mac->rlc_data_req);
            AssertFatal(buflen_remain >= sdu_length, "In %s: LCID = 0x%02x RLC has segmented %d bytes but MAC has max %d remaining bytes\n",
                        __FUNCTION__,
                        lcid,
                        sdu_length,
                        buflen_remain);
            if (sdu_length > 0) {

              LOG_D(NR_MAC, "In %s: [UE %d] [%d.%d] SL-DXCH -> SLSCH, Generating SL MAC sub-PDU for SDU %d, length %d bytes, RB with LCID 0x%02x (buflen (TBS) %d bytes)\n",
                __FUNCTION__,
                0,
                frame,
                slot,
                num_sdus + 1,
                sdu_length,
                lcid,
                buflen);

              //Jin Log
              LOG_I(NR_MAC,
                  "[SL-MAC nr_ue_scheduler.c] TX SDU LCID=%d LEN=%d frame=%d slot=%d SRC=%d DST=%d\n",
                  lcid, sdu_length, frame, slot,
                  sl_sch_subheader->SRC, sl_sch_subheader->DST);





              header->R = 0;
              header->F = 1;
              header->LCID = lcid;
              header->L = htons(sdu_length);
              pdu += sdu_length;
              sdu_length_total += sdu_length;
              total_mac_pdu_header_len += sh_size;
              buflen_remain -= sdu_length;
              LOG_D(NR_PHY, "buflen_remain %d, subtracting (sh_size + sdu_length) %d, total_mac_pdu_header_len %hhu sdu total length %d, sdu_length %d\n", buflen_remain, (sh_size + sdu_length), total_mac_pdu_header_len, sdu_length_total, sdu_length);
              num_sdus++;

            } else {
              pdu -= sh_size;
              buflen_remain += sh_size;
              LOG_D(NR_MAC, "In %s: no data to transmit for RB with LCID 0x%02x\n", __FUNCTION__, lcid);
              break;
            }
                //Jin log
              LOG_D(NR_MAC, "[SL-TX-DBG] TB base first16: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                    base[0], base[1], base[2], base[3],
                    base[4], base[5], base[6], base[7],
                    base[8], base[9], base[10], base[11],
                    base[12], base[13], base[14], base[15]);
          }

          if (buflen_remain > 0) {
            NR_UE_MAC_CE_INFO *mac_ce_p = (NR_UE_MAC_CE_INFO *) pdu;
            mac_ce_p->bsr_len = 0;
            mac_ce_p->bsr_ce_len = 0;
            mac_ce_p->bsr_header_len = 0;
            mac_ce_p->phr_len = 0;
            mac_ce_p->sdu_length_total = sdu_length_total;
            mac_ce_p->total_mac_pdu_header_len = total_mac_pdu_header_len;

            //nr_ue_get_sdu_mac_ce_pre updates all mac_ce related header field related to length
            mac_ce_p->tot_mac_ce_len = nr_ue_get_sdu_mac_ce_pre(0, 0, frame, slot, 0, pdu, buflen, mac_ce_p);
            buflen_remain -= mac_ce_p->tot_mac_ce_len;
            pdu += mac_ce_p->tot_mac_ce_len;
            LOG_D(NR_PHY, "buflen_remain %d, sdu_length_total %d, total_mac_pdu_header_len %d, adding tot_mac_ce_len %d, \n", buflen_remain, mac_ce_p->sdu_length_total, mac_ce_p->total_mac_pdu_header_len, mac_ce_p->tot_mac_ce_len);
          }
          int used = buflen - buflen_remain;//Jin calculate the actual used bytes. fix header problem.
        }
      }
      uint8_t sizeof_csi_report = (sizeof(NR_MAC_SUBHEADER_FIXED) + sizeof(nr_sl_csi_report_t));
      LOG_D(NR_MAC, "%4d.%2d buflen_remain %d ative %d, report slots: %4d.%2d size %d\n",
            frame,
            slot,
            buflen_remain,
            sched_ctrl->sched_csi_report.active,
            sched_ctrl->sched_csi_report.frame,
            sched_ctrl->sched_csi_report.slot,
            sizeof_csi_report);

      if (sched_ctrl->sched_csi_report.active &&
          (sched_ctrl->sched_csi_report.frame == frame) &&
          (sched_ctrl->sched_csi_report.slot == slot)) {

        if (buflen_remain >= sizeof_csi_report) {
          ((NR_MAC_SUBHEADER_FIXED *) pdu)->R = 0;
          ((NR_MAC_SUBHEADER_FIXED *) pdu)->LCID = SL_SCH_LCID_SL_CSI_REPORT;
          pdu++;
          buflen_remain -= sizeof(NR_MAC_SUBHEADER_FIXED);
          ((nr_sl_csi_report_t *) pdu)->RI = sched_ctrl->sched_csi_report.ri;
          ((nr_sl_csi_report_t *) pdu)->CQI = sched_ctrl->sched_csi_report.cqi;
          ((nr_sl_csi_report_t *) pdu)->R = 0;
          if (!get_nrUE_params()->sync_ref)
            LOG_D(NR_MAC, "%4d.%2d Sending sl_csi_report with CQI %i, RI %i\n",
                 frame,
                 slot,
                 ((nr_sl_csi_report_t *) pdu)->CQI,
                 ((nr_sl_csi_report_t *) pdu)->RI);
          pdu++;
          buflen_remain -= sizeof(nr_sl_csi_report_t);
        }
        sched_ctrl->sched_csi_report.active = false;
      }

      if (buflen_remain > 0) {
        LOG_D(NR_MAC, "In %s filling remainder %d bytes to the UL PDU \n", __FUNCTION__, buflen_remain);
        ((NR_MAC_SUBHEADER_FIXED *) pdu)->R = 0;
        ((NR_MAC_SUBHEADER_FIXED *) pdu)->LCID = SL_SCH_LCID_SL_PADDING;
        pdu++;
        buflen_remain--;

        if (IS_SOFTMODEM_RFSIM) {
          for (int j = 0; j < buflen_remain; j++) {
              pdu[j] = (unsigned char) rand();
          }
        } else {
          memset(pdu, 0, buflen_remain);
        }
      }

      sl_mac_stats->current_bytes = sched_pssch->tb_size;
      sl_mac_stats->current_rbs = sched_pssch->rbSize;
      sl_mac_stats->total_bytes += pscch_pssch_pdu->tb_size;
      sl_mac_stats->num_mac_sdu += num_sdus;
      sl_mac_stats->total_sdu_bytes += sdu_length_total;

      /* Save information on MCS, TBS etc for the current initial transmission
      * so we have access to it when retransmitting */
      cur_harq->sched_pssch = *sched_pssch;
    } // end of initial transmission

    //const uint32_t TBS = pscch_pssch_pdu->tb_size; //Jin replace : origin
    //memcpy(pscch_pssch_pdu->slsch_payload, cur_harq->transportBlock, TBS);//Jin replace : origin
    pscch_pssch_pdu->slsch_payload        = cur_harq->transportBlock;//Jin replace 
    pscch_pssch_pdu->slsch_payload_length = pscch_pssch_pdu->tb_size;//Jin replace : remove memcpy
    uint8_t *b = pscch_pssch_pdu->slsch_payload; //Jin replace add log
    LOG_D(NR_MAC,//jin log
          "[Jin debug !!!!  SL-TX-PDU] me=%u peer_uid=%u idx=%d frame=%d slot=%d tb=%p len=%u first8: "
          "%02x %02x %02x %02x %02x %02x %02x %02x\n",
          mac->src_id, UE->uid, pdu_idx, frame, slot,
          b, pscch_pssch_pdu->slsch_payload_length,
          b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);



    // mark UE as scheduled
    sched_pssch->rbSize = 0;
    is_resource_allocated = true;
  }
  return is_resource_allocated;
}

void nr_ue_sl_pscch_rx_scheduler(nr_sidelink_indication_t *sl_ind,
                              const NR_SL_BWP_ConfigCommon_r16_t *sl_bwp,
                              const NR_SL_ResourcePool_r16_t *sl_res_pool,
                              sl_nr_rx_config_request_t *rx_config,
                              uint8_t *config_type,
                              bool sl_has_psfch) {

  *config_type = SL_NR_CONFIG_TYPE_RX_PSCCH;
  rx_config->number_pdus = 1;
  rx_config->sfn = sl_ind->frame_rx;
  rx_config->slot = sl_ind->slot_rx;
  rx_config->sl_rx_config_list[0].pdu_type = *config_type;
  config_pscch_pdu_rx(&rx_config->sl_rx_config_list[0].rx_pscch_config_pdu,
                       sl_bwp,
                       sl_res_pool,
                       sl_has_psfch);


   LOG_D(NR_MAC, "[UE%d] TTI-%d:%d RX PSCCH REQ \n", sl_ind->module_id,sl_ind->frame_rx, sl_ind->slot_rx);

}
/*
*   determine if sidelink slot is a PSBCH slot
*    If PSBCH rx slot and sync_source == SYNC_REF_UE
*      TTI COMMAND = PSBCH RX
*    if PSBCH tx slot and transmit SLSS == true
*      TTI_COMMAND = PSBCH TX
*   Sidelink UE can rx and tx a SSB however the SSB time
*   allocation will be different
*/
uint8_t nr_ue_sl_psbch_scheduler(nr_sidelink_indication_t *sl_ind,
                                 sl_nr_ue_mac_params_t *sl_mac_params,
                                 sl_nr_rx_config_request_t *rx_config,
                                 sl_nr_tx_config_request_t *tx_config,
                                 uint8_t *config_type) {

  uint8_t ret_status = 0, is_psbch_rx_slot = 0, is_psbch_tx_slot = 0;
  uint16_t slot = sl_ind->slot_rx;
  uint16_t frame = sl_ind->frame_rx;

  // Schedule TX only if slot type is TX.
  if (sl_ind->slot_type == SIDELINK_SLOT_TYPE_TX) {
    slot = sl_ind->slot_tx;
    frame = sl_ind->frame_tx;
  }


  sl_nr_phy_config_request_t *sl_cfg = &sl_mac_params->sl_phy_config.sl_config_req;
  uint16_t scs = sl_cfg->sl_bwp_config.sl_scs;
  uint16_t slots_per_frame = nr_slots_per_frame[scs];

  LOG_D(NR_MAC,"[UE%d] SL-PSBCH SCHEDULER: Frame:SLOT %d:%d, slot_type:%d\n",
                                      sl_ind->module_id, frame, slot,sl_ind->slot_type);

  is_psbch_rx_slot = sl_determine_if_SSB_slot(frame, slot, slots_per_frame,
                                              &sl_mac_params->rx_sl_bch,
                                              sl_ind->slot_type);

  if (is_psbch_rx_slot &&
      sl_ind->slot_type == SIDELINK_SLOT_TYPE_RX) {

    *config_type = SL_NR_CONFIG_TYPE_RX_PSBCH;
    rx_config->number_pdus = 1;
    rx_config->sfn = frame;
    rx_config->slot = slot;
    rx_config->sl_rx_config_list[0].pdu_type = *config_type;

    LOG_D(NR_MAC, "[UE%d] TTI-%d:%d RX PSBCH REQ- rx_slss_id:%d, numSSB:%d, next slot_SSB:%d\n",
                                                         sl_ind->module_id,frame, slot,
                                                         sl_cfg->sl_sync_source.rx_slss_id,
                                                         sl_mac_params->rx_sl_bch.num_ssb,
                                                         sl_mac_params->rx_sl_bch.ssb_slot);

  }
  if (!is_psbch_rx_slot) {

    is_psbch_tx_slot = sl_determine_if_SSB_slot(frame, slot, slots_per_frame,
                                                &sl_mac_params->tx_sl_bch,
                                                sl_ind->slot_type);

    if (is_psbch_tx_slot &&
        sl_ind->slot_type == SIDELINK_SLOT_TYPE_TX) {

      *config_type = SL_NR_CONFIG_TYPE_TX_PSBCH;
      tx_config->number_pdus = 1;
      tx_config->sfn = frame;
      tx_config->slot = slot;
      tx_config->tx_config_list[0].pdu_type = *config_type;
      tx_config->tx_config_list[0].tx_psbch_config_pdu.tx_slss_id = sl_mac_params->tx_sl_bch.slss_id;
      tx_config->tx_config_list[0].tx_psbch_config_pdu.psbch_tx_power = 0;//TBD...
      memcpy(tx_config->tx_config_list[0].tx_psbch_config_pdu.psbch_payload, sl_mac_params->tx_sl_bch.sl_mib, 4);

      if ((frame & 127) == 0) LOG_D(NR_MAC, "[SyncRefUE%d] TTI-%d:%d TX PSBCH REQ- tx_slss_id:%d, sl-mib:%x, numSSB:%d, next SSB slot:%d\n",
                                                            sl_ind->module_id,frame, slot,
                                                            sl_mac_params->tx_sl_bch.slss_id,
                                                            (*(uint32_t *)tx_config->tx_config_list[0].tx_psbch_config_pdu.psbch_payload),
                                                            sl_mac_params->tx_sl_bch.num_ssb,
                                                            sl_mac_params->tx_sl_bch.ssb_slot);
    }

  }

  ret_status = is_psbch_rx_slot | is_psbch_tx_slot;

  LOG_D(NR_MAC,"[UE%d] SL-PSBCH SCHEDULER: %d:%d,is psbch slot:%d, config type:%d\n",
                                              sl_ind->module_id,frame, slot, ret_status, *config_type);
  return ret_status;
}

/*
  // This function will be called only for SIDELINK CAPABLE SLOTS.
  // UPLINK SLOT OR MIXED SLOT which is SIDELINK SLOT

  //Determine if PSBCH SLOT and if PSBCH RX/TX should be done
  // IF NOT PSBCH SLOT continue ahead

  // IF RX RES POOL CONFIGURED
  // Determine if SLOT is a RX RES POOL RESERVED
  // OR RX RES POOL RESOURCE SLOT according to time resource bitmap
  // IF resource slot PSCCH RX action should be done

  // IF TX RES POOL CONFIGURED
  // Determine if SLOT is a TX RES POOL RESERVED
  // OR RX RES POOL RESOURCE SLOT according to time resource bitmap
  // IF resource slot PSCCH TX action should be done in case TX is scheduled
  // ELSE SENSING SHOULD BE DONE

  // IF TX/RX ACTION SHOULD BE DONE in this slot
  // SEND SIDELINK TX/RX CONFIG REQUEST TO PHY
*/
void nr_ue_sidelink_scheduler(nr_sidelink_indication_t *sl_ind) {

  AssertFatal(sl_ind != NULL, "sl_indication cannot be NULL\n");
  module_id_t mod_id    = sl_ind->module_id;
  frame_t frame     = sl_ind->frame_rx;
  slot_t slot       = sl_ind->slot_rx;

  if (sl_ind->slot_type == SIDELINK_SLOT_TYPE_TX) {
    frame = sl_ind->frame_tx;
    slot = sl_ind->slot_tx;
  }

  NR_UE_MAC_INST_t *mac = get_mac_inst(mod_id);
  sl_nr_ue_mac_params_t *sl_mac = mac->SL_MAC_PARAMS;
  sl_nr_phy_config_request_t *sl_cfg = &sl_mac->sl_phy_config.sl_config_req;

  uint8_t mu = sl_cfg->sl_bwp_config.sl_scs;
  uint8_t slots_per_frame = nr_slots_per_frame[mu];

  NR_UE_SL_SCHED_LOCK(&mac->sl_sched_lock);

  //Adjust indices as new timing is acquired
  if (sl_mac->adjust_timing) {
    sl_adjust_indices_based_on_timing(sl_ind->frame_rx, sl_ind->slot_rx,
                                      sl_ind->frame_tx, sl_ind->slot_tx,
                                      mod_id, slots_per_frame);
    sl_mac->adjust_timing = 0;
  }

  sl_nr_rx_config_request_t rx_config;
  sl_nr_tx_config_request_t tx_config;

  rx_config.number_pdus = 0;
  tx_config.number_pdus = 0;

  nr_scheduled_response_t scheduled_response;
  memset(&scheduled_response,0, sizeof(nr_scheduled_response_t));

 
  uint8_t tti_action = 0, is_psbch_slot = 0;
 
 
  // Check if PSBCH slot and PSBCH should be transmitted or Received
  is_psbch_slot = nr_ue_sl_psbch_scheduler(sl_ind, sl_mac, &rx_config, &tx_config, &tti_action);

  bool tx_allowed=true,rx_allowed=true;
  if (mac->sl_tx_res_pool && mac->sl_tx_res_pool->ext1 && mac->sl_tx_res_pool->ext1->sl_TimeResource_r16) {
     int sl_tx_period = 8*mac->sl_tx_res_pool->ext1->sl_TimeResource_r16->size - mac->sl_tx_res_pool->ext1->sl_TimeResource_r16->bits_unused;
     int slot_mod_period = sl_ind->slot_tx%sl_tx_period;
     uint8_t mask = mac->sl_tx_res_pool->ext1->sl_TimeResource_r16->buf[slot_mod_period>>3];
     if (((1<<slot_mod_period) % mask) == 0) tx_allowed=0;
  }


  frameslot_t frame_slot;
  frame_slot.frame = frame;
  frame_slot.slot = slot;

  sl_resource_info_t *resource = NULL;
  if (mac->sl_candidate_resources && mac->sl_candidate_resources->size > 0 && sl_ind->slot_type == SIDELINK_SLOT_TYPE_TX) {
    LOG_D(NR_MAC, "%4d.%2d sl_candidate_resources %p size %ld, capacity %ld slot_type %d\n", frame, slot, mac->sl_candidate_resources, mac->sl_candidate_resources->size, mac->sl_candidate_resources->capacity, sl_ind->slot_type);
    resource = get_resource_element(mac->sl_candidate_resources, frame_slot);
    if (resource) {
      LOG_D(NR_MAC, "SELECTED_RESOURCE %4d.%2d slot_type %d, num_sl_pscch_rbs %d, sl_max_num_per_reserve %d, sl_min_time_gap_psfch %d, sl_pscch_sym_start %d, \
            sl_pscch_sym_len %d, sl_psfch_period %d, sl_pssch_sym_start %d, sl_pssch_sym_len %d, sl_subchan_len %d, sl_subchan_size %d\n",
            resource->sfn.frame, resource->sfn.slot, sl_ind->slot_type,
            resource->num_sl_pscch_rbs,
            resource->sl_max_num_per_reserve,
            resource->sl_min_time_gap_psfch,
            resource->sl_pscch_sym_start,
            resource->sl_pscch_sym_len,
            resource->sl_psfch_period,
            resource->sl_pssch_sym_start,
            resource->sl_pssch_sym_len,
            resource->sl_subchan_len,
            resource->sl_subchan_size);
    }
  }

  nr_sl_transmission_params_t *sl_tx_params = &sl_mac->mac_tx_params;
  uint16_t p_prime_rsvp_tx = time_to_slots(mu, sl_tx_params->resel_counter);
  static int8_t is_rsrc_selected = false;

  if (mac->rsc_selection_method == c1 ||
      mac->rsc_selection_method == c4 ||
      mac->rsc_selection_method == c5 ||
      mac->rsc_selection_method == c7) {
    LOG_D(NR_MAC, "%4d.%2d is_rsrc_selected %d, reselection_timer %d, p_prime_rsvp_tx %d, slot_type %d\n",
          frame, slot, is_rsrc_selected, mac->reselection_timer, p_prime_rsvp_tx, sl_ind->slot_type);
    if(is_rsrc_selected && (sl_ind->slot_type == 2) && (mac->reselection_timer < p_prime_rsvp_tx)) {
      mac->reselection_timer++;
    } else if (sl_ind->slot_type == 2) {
      if (mac->reselection_timer < p_prime_rsvp_tx) {
        mac->sl_candidate_resources = get_candidate_resources(&frame_slot, mac, &mac->sl_sensing_data, &mac->sl_transmit_history);
        if (mac->sl_candidate_resources) {
          LOG_D(NR_MAC, "%4d.%2d Returned resources %p\n", frame, slot, mac->sl_candidate_resources);
          print_candidate_list(mac->sl_candidate_resources, __LINE__);
        }
        is_rsrc_selected = true;
      } else {
        mac->reselection_timer = 0;
        is_rsrc_selected = false;
      }
    }
  }

  if (mac->sl_rx_res_pool && mac->sl_rx_res_pool->ext1 && mac->sl_rx_res_pool->ext1->sl_TimeResource_r16) {
     int sl_rx_period = 8*mac->sl_rx_res_pool->ext1->sl_TimeResource_r16->size - mac->sl_rx_res_pool->ext1->sl_TimeResource_r16->bits_unused;
     int slot_mod_period = sl_ind->slot_rx%sl_rx_period;

     //uint8_t mask = mac->sl_rx_res_pool->ext1->sl_TimeResource_r16->buf[slot_mod_period>>3];  
     //if (((1<<slot_mod_period) % mask) == 0) rx_allowed=false; 
  }
  if (sl_ind->slot_type==SIDELINK_SLOT_TYPE_TX || sl_ind->phy_data==NULL) rx_allowed=false;
  //Jin test following to bypss tx allowed
 // if (sl_ind->phy_data != NULL && sl_ind->slot_type != SIDELINK_SLOT_TYPE_TX) {
  //  rx_allowed = true;   // bypass only bitmap gating
  //}
  //tx_allowed = true;      

  static uint16_t prev_slot = 0;
  NR_SL_PSFCH_Config_r16_t *sl_psfch_config = mac->sl_tx_res_pool->sl_PSFCH_Config_r16 ? mac->sl_tx_res_pool->sl_PSFCH_Config_r16->choice.setup : NULL;
  const uint8_t psfch_periods[] = {0,1,2,4};
  long psfch_period = (sl_psfch_config && sl_psfch_config->sl_PSFCH_Period_r16)
                      ? psfch_periods[*sl_psfch_config->sl_PSFCH_Period_r16] : 0;
 
  if ((prev_slot != slot) && rx_allowed && !is_psbch_slot) {
      frameslot_t fs;
      fs.frame = frame;
      fs.slot = slot;
      uint64_t rx_abs_slot = normalize(&fs, mu);
      uint8_t pool_id = 0;
      SL_ResourcePool_params_t *sl_rx_rsrc_pool = sl_mac->sl_RxPool[pool_id];
      uint16_t phy_map_sz = ((sl_rx_rsrc_pool->phy_sl_bitmap.size << 3) - sl_rx_rsrc_pool->phy_sl_bitmap.bits_unused);
      bool sl_has_psfch = slot_has_psfch(mac, &sl_rx_rsrc_pool->phy_sl_bitmap, rx_abs_slot, psfch_period, phy_map_sz, mac->SL_MAC_PARAMS->sl_TDD_config);
      LOG_D(NR_MAC, "%4d.%2d RX sl_has_psfch %d, psfch_period %ld\n", frame, slot, sl_has_psfch, psfch_period);
      nr_ue_sl_pscch_rx_scheduler(sl_ind, mac->sl_bwp, mac->sl_rx_res_pool, &rx_config, &tti_action, sl_has_psfch);
      prev_slot = slot;
  }

  if (resource && mac->is_synced && !is_psbch_slot && tx_allowed && sl_ind->slot_type == SIDELINK_SLOT_TYPE_TX) {
    //Check if reserved slot or a sidelink resource configured in Rx/Tx resource pool timeresource bitmap
    bool is_resource_allocated = nr_ue_sl_pssch_scheduler(mac, sl_ind, mac->sl_bwp, mac->sl_tx_res_pool, &tx_config, resource, &tti_action);
 
    if (is_resource_allocated && mac->sci2_pdu.csi_req) {
      nr_ue_sl_csi_rs_scheduler(mac, mu, mac->sl_bwp, &tx_config, NULL, &tti_action);
      LOG_D(NR_MAC, "%4d.%2d Scheduling CSI-RS\n", frame, slot);
    }
    /* JIN_DISABLE_HARQ_FEEDBACK
    bool is_feedback_slot = mac->sl_tx_res_pool->sl_PSFCH_Config_r16 ? is_feedback_scheduled(mac, frame, slot) : false;
    if (is_resource_allocated && is_feedback_slot && mac->sl_tx_res_pool->sl_PSFCH_Config_r16->choice.setup) {
      if (is_feedback_slot) {
        nr_ue_sl_psfch_scheduler(mac, frame, slot, psfch_period, sl_ind, mac->sl_bwp, &tx_config, &tti_action);
        reset_sched_psfch(mac, frame, slot);
      }
    }
  */
  }

  if (((slot % 20) == 6) && ((frame % 100) == 0)) {
    char stats_output[16000] = {0};
    dump_mac_stats_sl(mac, stats_output, sizeof(stats_output), true);
    LOG_D(NR_MAC, "Frame.Slot %d.%d\n%s\n", frame, slot, stats_output);
  }

  if (tti_action == SL_NR_CONFIG_TYPE_RX_PSBCH || tti_action == SL_NR_CONFIG_TYPE_RX_PSCCH || tti_action == SL_NR_CONFIG_TYPE_RX_PSSCH_SCI ||
      tti_action == SL_NR_CONFIG_TYPE_RX_PSSCH_SLSCH) {
    fill_scheduled_response(&scheduled_response, NULL, NULL, NULL,  &rx_config, NULL, mod_id, 0,frame, slot, sl_ind->phy_data);
  }
  if (tti_action == SL_NR_CONFIG_TYPE_TX_PSBCH || tti_action == SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH_PSFCH || tti_action == SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH || tti_action == SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH_CSI_RS) {
    fill_scheduled_response(&scheduled_response, NULL, NULL, NULL, NULL, &tx_config, mod_id, 0,frame, slot, sl_ind->phy_data);
  }


  LOG_D(NR_MAC,"[UE%d]SL-SCHEDULER: TTI-RX-%d:%d, TX-%d:%d is_psbch_slot:%d TTIaction:%d\n",
                                                            mod_id,sl_ind->frame_rx, sl_ind->slot_rx,
                                                            sl_ind->frame_tx, sl_ind->slot_tx,
                                                            is_psbch_slot, tti_action);

  if (tti_action) {
    frameslot_t frame_slot;
    frame_slot.frame = frame;
    frame_slot.slot = slot;
    if (mac->sl_transmit_history.size > 1)
      remove_old_transmit_history(&frame_slot, sl_mac->sl_TxPool[0]->t0, &mac->sl_transmit_history, sl_mac);
    if (sl_ind->slot_type == SIDELINK_SLOT_TYPE_TX) {
      LOG_D(NR_MAC, "Inserting transmit history data: %4d.%2d\n", frame_slot.frame, frame_slot.slot);
      push_back(&mac->sl_transmit_history, &frame_slot);
    }
      // --- JIN: robust TX logging: find PSCCH+PSSCH entry by pdu_type (NOT index 0) ---
      if (tti_action == SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH_PSFCH ||
          tti_action == SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH ||
          tti_action == SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH_CSI_RS) {

        int found = 0;
        for (int i = 0; i < tx_config.number_pdus; i++) {
          uint8_t t = tx_config.tx_config_list[i].pdu_type;

          LOG_D(NR_MAC,
                "[SL-TX-LIST] me=%u frame=%d slot=%d i=%d pdu_type=%u\n",
                mac->src_id, frame, slot, i, t);

          if (t == SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH_PSFCH ||
              t == SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH ||
              t == SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH_CSI_RS) {

            sl_nr_tx_config_pscch_pssch_pdu_t *p =
                &tx_config.tx_config_list[i].tx_pscch_pssch_config_pdu;

            found = 1;

            if (p->slsch_payload && p->slsch_payload_length >= 8) {
              uint8_t *b = p->slsch_payload;
              LOG_D(NR_MAC,
                    "[SL-TX-FINAL] me=%u frame=%d slot=%d idx=%d slsch_len=%u tb_size=%u first8: "
                    "%02x %02x %02x %02x %02x %02x %02x %02x\n",
                    mac->src_id, frame, slot, i,
                    p->slsch_payload_length, p->tb_size,
                    b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
            } else {
              LOG_W(NR_MAC,
                    "[SL-TX-FINAL] me=%u frame=%d slot=%d idx=%d slsch_payload missing/short "
                    "(ptr=%p len=%u tb_size=%u)\n",
                    mac->src_id, frame, slot, i,
                    p->slsch_payload, p->slsch_payload_length, p->tb_size);
            }
          }
        }

        if (!found) {
          LOG_W(NR_MAC,
                "[SL-TX-FINAL] me=%u frame=%d slot=%d no PSCCH+PSSCH PDU found (number_pdus=%u)\n",
                mac->src_id, frame, slot, tx_config.number_pdus);
        }
      }
     // --- Jin end
    if ((mac->if_module != NULL) && (mac->if_module->scheduled_response != NULL))
      mac->if_module->scheduled_response(&scheduled_response);
  }
  NR_UE_SL_SCHED_UNLOCK(&mac->sl_sched_lock);
}

void nr_ue_sl_psfch_scheduler(NR_UE_MAC_INST_t *mac,
                              frame_t frame,
                              uint16_t slot,
                              long psfch_period,
                              nr_sidelink_indication_t *sl_ind,
                              const NR_SL_BWP_ConfigCommon_r16_t *sl_bwp,
                              sl_nr_tx_config_request_t *tx_config,
                              uint8_t *config_type) {
  int num_psfch_symbols = 0;
  if (psfch_period == 1) num_psfch_symbols = 3;
  else if (psfch_period == 2 || psfch_period == 4) {
    num_psfch_symbols = mac->SL_MAC_PARAMS->sl_TxPool[0]->sci_1a.psfch_overhead_indication.nbits ? 3 : 0;
  }

  sl_nr_ue_mac_params_t *sl_mac = mac->SL_MAC_PARAMS;
  int scs = sl_mac->sl_phy_config.sl_config_req.sl_bwp_config.sl_scs;
  const int nr_slots_frame = nr_slots_per_frame[scs];
  NR_TDD_UL_DL_Pattern_t *tdd = &sl_mac->sl_TDD_config->pattern1;
  const int n_ul_slots_period = tdd ? tdd->nrofUplinkSlots + (tdd->nrofUplinkSymbols > 0 ? 1 : 0) : nr_slots_frame;
  uint16_t num_subch = sl_get_num_subch(mac->sl_tx_res_pool);
  tx_config->tx_config_list[0].tx_pscch_pssch_config_pdu.psfch_pdu_list = CALLOC(psfch_period*num_subch, sizeof(sl_nr_tx_rx_config_psfch_pdu_t));
  sl_nr_tx_rx_config_psfch_pdu_t *psfch_pdu_list = tx_config->tx_config_list[0].tx_pscch_pssch_config_pdu.psfch_pdu_list;
  int k = 0;
  for (int i = 0; i < (n_ul_slots_period * num_subch); i++) {
    // SL_sched_feedback_t  *sched_psfch = &mac->sl_info.list[0]->UE_sched_ctrl.sched_psfch[i];  //Jin replace
    //Jin replace
    NR_SL_UE_info_t *UE = NULL;
    uint16_t peer_id = UE->uid;
    NR_SL_UE_info_t *peer = find_UE(mac, peer_id);
    if (!peer || !peer->UE_sched_ctrl.sched_psfch) {
      LOG_W(NR_MAC, "TX sched: no peer/sched_psfch for dest_id=%d\n", peer_id);
      continue;
    }
    SL_sched_feedback_t *sched_psfch = &peer->UE_sched_ctrl.sched_psfch[i];
    //Jin end


    LOG_D(NR_MAC,"frame.slot: feedback %4d.%2d, current (%4d.%2d)\n",
          sched_psfch->feedback_frame, sched_psfch->feedback_slot, frame, slot);
    if (sched_psfch->feedback_slot == slot && sched_psfch->feedback_frame == frame) {
      sl_ind->slot_tx = sched_psfch->feedback_slot;
      sl_ind->frame_tx = sched_psfch->feedback_frame;
      sl_ind->slot_type = SIDELINK_SLOT_TYPE_TX;
      AssertFatal(k < psfch_period*num_subch, "Number of PSFCH pdus cannot exceed %ld\n", psfch_period * num_subch);
      fill_psfch_pdu(sched_psfch, &psfch_pdu_list[k], num_psfch_symbols);
      *config_type = SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH_PSFCH;
      tx_config->number_pdus = 1;
      tx_config->tx_config_list[0].pdu_type = *config_type;
      LOG_D(NR_MAC,"SL-PSFCH SCHEDULER: frame.slot (%d.%d), slot_type:%d\n",
            frame, slot, sl_ind->slot_type);
      sched_psfch->feedback_slot = -1;
      sched_psfch->feedback_frame = -1;
      sched_psfch->dai_c = 0;
      sched_psfch->harq_feedback = -1;
      k++;
    }
  }
  tx_config->tx_config_list[0].tx_pscch_pssch_config_pdu.num_psfch_pdus = k;
}

void fill_psfch_pdu(SL_sched_feedback_t *mac_psfch_pdu,
                    sl_nr_tx_rx_config_psfch_pdu_t *tx_psfch_pdu,
                    int num_psfch_symbols) {
  tx_psfch_pdu->start_symbol_index = mac_psfch_pdu->start_symbol_index;
  tx_psfch_pdu->hopping_id = mac_psfch_pdu->hopping_id;
  tx_psfch_pdu->prb = mac_psfch_pdu->prb;
  tx_psfch_pdu->sl_bwp_start = mac_psfch_pdu->sl_bwp_start;
  tx_psfch_pdu->initial_cyclic_shift = mac_psfch_pdu->initial_cyclic_shift;
  tx_psfch_pdu->mcs = mac_psfch_pdu->mcs;
  tx_psfch_pdu->freq_hop_flag = mac_psfch_pdu->freq_hop_flag;
  tx_psfch_pdu->second_hop_prb = mac_psfch_pdu->second_hop_prb;
  tx_psfch_pdu->group_hop_flag = mac_psfch_pdu->group_hop_flag;
  tx_psfch_pdu->sequence_hop_flag = mac_psfch_pdu->sequence_hop_flag;
  tx_psfch_pdu->nr_of_symbols = num_psfch_symbols ? num_psfch_symbols - 2 : 0; // (num_psfch_symbols - 2) excludes PSFCH AGC and Guard
  AssertFatal(tx_psfch_pdu->nr_of_symbols >= 0, "Number of PSFCH symbols can not be negative!!!\n");
  tx_psfch_pdu->bit_len_harq = mac_psfch_pdu->bit_len_harq;
  LOG_D(PHY,"%s: nr_symbols %d, start_symbol %d, prb_start %d, second_hop_prb %d, \
        group_hop_flag %d, sequence_hop_flag %d, mcs %d initial_cyclic_shift %d \
        hopping_id %d, sl_bwp_start %d freq_hop_flag %d\n",
        __FUNCTION__,
        tx_psfch_pdu->nr_of_symbols,
        tx_psfch_pdu->start_symbol_index,
        tx_psfch_pdu->prb,
        tx_psfch_pdu->second_hop_prb,
        tx_psfch_pdu->group_hop_flag,
        tx_psfch_pdu->sequence_hop_flag,
        tx_psfch_pdu->mcs,
        tx_psfch_pdu->initial_cyclic_shift,
        tx_psfch_pdu->hopping_id,
        tx_psfch_pdu->sl_bwp_start,
        tx_psfch_pdu->freq_hop_flag
        );
}

void nr_ue_sl_csi_rs_scheduler(NR_UE_MAC_INST_t *mac,
                               uint8_t scs,
                               const NR_SL_BWP_ConfigCommon_r16_t *sl_bwp,
                               sl_nr_tx_config_request_t *tx_config,
                               sl_nr_rx_config_request_t *rx_config,
                               uint8_t *config_type) {
  sl_nr_ue_mac_params_t *sl_mac = mac->SL_MAC_PARAMS;
  sl_nr_tti_csi_rs_pdu_t *csi_rs_pdu = NULL;
  if (tx_config != NULL) {
      csi_rs_pdu = &tx_config->tx_config_list[0].tx_pscch_pssch_config_pdu.nr_sl_csi_rs_pdu;
      tx_config->number_pdus = 1;
      *config_type = SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH_CSI_RS;
      tx_config->tx_config_list[0].pdu_type = *config_type;
  } else if (rx_config != NULL) {
      csi_rs_pdu = &rx_config->sl_rx_config_list[0].rx_csi_rs_config_pdu;
      rx_config->number_pdus = 1;
      rx_config->sl_rx_config_list[0].pdu_type = SL_NR_CONFIG_TYPE_RX_PSSCH_SLSCH_CSI_RS;
  }
  AssertFatal(csi_rs_pdu != NULL, "tx_config and rx_config both cannot be NULL\n");
  fill_csi_rs_pdu(sl_mac, csi_rs_pdu, sl_bwp, scs);
}

void fill_csi_rs_pdu(sl_nr_ue_mac_params_t *sl_mac, sl_nr_tti_csi_rs_pdu_t *csi_rs_pdu, const NR_SL_BWP_ConfigCommon_r16_t *sl_bwp, uint8_t scs) {
  long* cyclicPrefix = sl_bwp->sl_BWP_Generic_r16->sl_BWP_r16->cyclicPrefix;
  csi_rs_pdu->cyclic_prefix = cyclicPrefix == NULL ? 0 : *cyclicPrefix; // (0: normal; 1: Extended)
  csi_rs_pdu->measurement_bitmap = sl_mac->measurement_bitmap;
  csi_rs_pdu->subcarrier_spacing = scs;
  csi_rs_pdu->start_rb = sl_mac->start_rb;
  csi_rs_pdu->nr_of_rbs = sl_mac->nr_of_rbs;
  csi_rs_pdu->csi_type = sl_mac->csi_type;
  csi_rs_pdu->row = sl_mac->row;
  csi_rs_pdu->freq_domain = sl_mac->freq_domain;
  csi_rs_pdu->symb_l0 = sl_mac->symb_l0;
  csi_rs_pdu->cdm_type = sl_mac->cdm_type;
  csi_rs_pdu->freq_density = sl_mac->freq_density;
  csi_rs_pdu->power_control_offset = sl_mac->power_control_offset;
  csi_rs_pdu->power_control_offset_ss = sl_mac->power_control_offset_ss;
}

int get_bit_from_map(const uint8_t *buf, size_t bit_pos) {
  size_t byte_index = bit_pos / 8;
  uint8_t bit_index = bit_pos % 8;
  LOG_D(NR_MAC, "buf[%ld] = %d, ((7 - %d)) & 1), (buf[byte_index] >> %d) = %d  %d\n",
        byte_index, buf[byte_index], bit_index, (7 - bit_index), buf[byte_index] >> (7 - bit_index), (buf[byte_index] >> (7 - bit_index)) & 1);
  return (buf[byte_index] >> (7 - bit_index)) & 1;
}

void append_bit(uint8_t *buf, size_t bit_pos, int bit_value) {
  size_t byte_index = bit_pos / 8;
  uint8_t bit_index = bit_pos % 8;
  LOG_D(NR_MAC, "Appending bit_value %d at byte_index %ld bit index %d\n", bit_value, byte_index, bit_index);
  if (bit_value) {
    buf[byte_index] |= (1 << (7 - bit_index));
  } else {
    buf[byte_index] &= ~(1 << (7 - bit_index));
  }
}

void remove_old_sensing_data(frameslot_t *frame_slot,
                             uint16_t sensing_window,
                             List_t* sensing_data,
                             sl_nr_ue_mac_params_t *sl_mac) {

  int new_size = 0;
  int mu = sl_mac->sl_phy_config.sl_config_req.sl_bwp_config.sl_scs;
  for (int i = 0; i < sensing_data->size; i++) {
    sensing_data_t *data = (sensing_data_t*)((char*)sensing_data->data + i * sensing_data->element_size);
    LOG_D(NR_MAC, " i %d, old (%4d.%2d) %ld >=  current (%4d.%2d) %ld staled data slots %ld\n",
          i,
          data->frame_slot.frame,
          data->frame_slot.slot,
          normalize(&data->frame_slot, mu),
          frame_slot->frame,
          frame_slot->slot,
          normalize(frame_slot, mu),
          normalize(frame_slot, mu) - sensing_window);

    int64_t num_max_slots = nr_slots_per_frame[mu] * 1024;
    int64_t diff = (normalize(frame_slot, mu) - normalize(&data->frame_slot, mu) + num_max_slots) % num_max_slots;
    if (diff <= sensing_window) {
      break;
    } else {
      new_size ++;
    }
  }
  if (new_size > 0) {
    LOG_D(NR_MAC, "sensing data: size %ld, element_size %ld new_size %d\n", sensing_data->size, sensing_data->element_size, new_size);
    memmove(sensing_data->data, (char*)sensing_data->data + new_size * sensing_data->element_size, (sensing_data->size - new_size) * sensing_data->element_size);
    LOG_D(NR_MAC, "Subtracting %d from %ld\n", new_size, sensing_data->size);
    sensing_data->size -= new_size;
  }
}

void remove_old_transmit_history(frameslot_t *frame_slot,
                                 uint16_t sensing_window,
                                 List_t* transmit_history,
                                 sl_nr_ue_mac_params_t *sl_mac) {

  int new_size = 0;
  int mu = sl_mac->sl_phy_config.sl_config_req.sl_bwp_config.sl_scs;
  for (int i = 0; i < transmit_history->size; i++) {
    frameslot_t *tr_his_frame_slot = (frameslot_t*)((char*)transmit_history->data + i * transmit_history->element_size);
    LOG_D(NR_MAC, " i %d, Transmit history data: (%4d.%2d) %ld >=  current (%4d.%2d) %ld\n",
           i,
           tr_his_frame_slot->frame,
           tr_his_frame_slot->slot,
           normalize(tr_his_frame_slot, mu),
           frame_slot->frame,
           frame_slot->slot,
           normalize(frame_slot, mu) - sensing_window);
    /*
      normalize(frame_slot, mu) - sensing_window: This condition is to avoid the two cases, where current absolute slot value can be smaller
      than sensing window size. The first case represents the beginning of simulation, there should be more sensing / transmit history data than
      the sensing window size to check for deletion. e.g. if sensing window size is 100ms (200 slots), for first 200 slots, there will not be any
      old data, which should be removed. In the second case, the frame number starts from 0 after completing a cycle of frame numbers (0..1023).
      In that case, current absolute slot value will also be smaller than the sensing window size. When the above condition is true, it checks if
      the sensed data slot lies within the sensing window (implemented by the internal condition), if sensed data absolute slot lies within the
      sensing window, it stops further iterating over the sensing data.
      In the else part, the sensing data / transmit history list contains data from the last part of the frame number cycle (1013..1023) and beginning
      (0..10). In this case, the older data may belong to the range (1013..1023). The new_size contains the older sensed, which should be removed from
      the sensing data / transmit history list.
    */
    if (normalize(frame_slot, mu) - sensing_window > 0) {
      if (normalize(tr_his_frame_slot, mu) >= normalize(frame_slot, mu) - sensing_window) {
        break;
      }
    } else {
      int transmit_history_size = transmit_history->size;
      int prev_frame_data_size = transmit_history_size - normalize(frame_slot, mu);
      if (prev_frame_data_size > 0) {
        new_size += prev_frame_data_size - (abs(normalize(frame_slot, mu) - sensing_window));
      }
      break;
    }
    new_size ++;
  }
  if (new_size > 0) {
    memmove(transmit_history->data, (char*)transmit_history->data + new_size * transmit_history->element_size, (transmit_history->size - new_size) * transmit_history->element_size);
    LOG_D(NR_MAC, "Subtracting %d from %ld\n", new_size, transmit_history->size);
    transmit_history->size -= new_size;
  }
}

bool check_t1_within_tproc1(uint8_t mu, uint16_t t1_slots) {
    if ((mu == 0 && t1_slots <= 3) || (mu == 1 && t1_slots <= 5) ||
        (mu == 2 && t1_slots <= 9) || (mu == 3 && t1_slots <= 17))
    {
        return true;
    }
    return false;
}

NR_SL_ResourcePool_r16_t* get_resource_pool(NR_UE_MAC_INST_t *mac, uint16_t pool_id) {
  return mac->SL_MAC_PARAMS->sl_TxPool[pool_id]->respool;
}

bool slot_has_psfch(NR_UE_MAC_INST_t *mac, BIT_STRING_t *phy_sl_bitmap, uint64_t abs_index_cur_slot, uint8_t psfch_period, size_t phy_sl_map_size, NR_TDD_UL_DL_ConfigCommon_t *conf) {

  if (psfch_period == 0) {
    return false;
  }
  AssertFatal(conf->pattern1.nrofUplinkSlots == 4 && conf->pattern1.nrofDownlinkSlots == 6,
              "Invalid configuration set. Please update the nrofUplinkSlots to 4 and nrofDownlinkSlots to 6.\n");
  bool sl_slot = is_sl_slot(mac, phy_sl_bitmap, phy_sl_map_size, abs_index_cur_slot);
  bool has_psfch = sl_slot && ((conf->pattern1.nrofUplinkSlots % psfch_period) == 0);
  LOG_D(NR_MAC, "num_sl_slots %ld has_psfch %d, abs slot %ld, is_sl_slot %d\n",
        conf->pattern1.nrofUplinkSlots, has_psfch, abs_index_cur_slot, sl_slot);
  return has_psfch;
}

void validate_selected_sl_slot(bool tx, bool rx, NR_TDD_UL_DL_ConfigCommon_t *conf, frameslot_t frame_slot) {
 //Jin disactive these part, since we installed TDD manually. 
  /* 
  AssertFatal(conf->pattern1.nrofUplinkSlots == 4 && conf->pattern1.nrofDownlinkSlots == 6,
              "Invalid configuration set. Please update the nrofUplinkSlots to 4 and nrofDownlinkSlots to 6.\n");
  if (get_nrUE_params()->sync_ref) {
    if (tx) {
      AssertFatal((frame_slot.slot == 6 || frame_slot.slot == 7 || frame_slot.slot == 8 || frame_slot.slot == 9),
                  "As a transmitting syncref UE, based on the current configuration of uplink slots = %ld and downlink = %ld, "
                  "you should be selecting resources with slot 6, 7, 8, or 9 only.\n",
                  conf->pattern1.nrofUplinkSlots, conf->pattern1.nrofDownlinkSlots);
    } else if (rx) {
      AssertFatal((frame_slot.slot == 16 || frame_slot.slot == 17 || frame_slot.slot == 18 || frame_slot.slot == 19),
                  "As a receiving syncref UE, based on the current configuration of uplink slots = %ld and downlink = %ld, "
                  "you should be selecting resources with slot 16, 17, 18, or 19 only.\n",
                  conf->pattern1.nrofUplinkSlots, conf->pattern1.nrofDownlinkSlots);
    }
  } else if (!get_nrUE_params()->sync_ref) {
    if (tx) {
      AssertFatal((frame_slot.slot == 16 || frame_slot.slot == 17 || frame_slot.slot == 18 || frame_slot.slot == 19),
                  "As a transmitting nearby UE, based on the current configuration of uplink slots = %ld and downlink = %ld, "
                  "you should be selecting resources with slot 16, 17,1 8, or 19 only.\n",
                  conf->pattern1.nrofUplinkSlots, conf->pattern1.nrofDownlinkSlots);
    } else if (rx) {
      AssertFatal((frame_slot.slot == 6 || frame_slot.slot == 7 || frame_slot.slot == 8 || frame_slot.slot == 9),
                  "As a receiving nearby UE, based on the current configuration of uplink slots = %ld and downlink = %ld, "
                  "you should be selecting resources with slot 6, 7, 8, or 9 only.\n",
                  conf->pattern1.nrofUplinkSlots, conf->pattern1.nrofDownlinkSlots);
    }
  }
       */
}

bool is_sl_slot(NR_UE_MAC_INST_t *mac, BIT_STRING_t *phy_sl_bitmap, uint16_t phy_map_sz, uint64_t abs_slot) {
  /* The purpose of normalizing the abs_slot value is to ensure that we can handle the cases
    when we wrap beyond the phy_bit_map size. For example, with an uplink and downlink
    slot configuration of 4 and 6 respectively, we have a phy_bit_map size of 150. When
    abs_slot (frame.slot absolute value) exceeds 150, we are not able to proeprly map the bits
    to the resource bitmap. In order to do this, we need to map the abs_slot > 150 value to a
    value within 150. Since (in this particular configuration) the slots in the last frame (7)
    are split in half (since 150 is not divisible by 20 slots/frame) so we have to shift the
    normalization factor by the split (which is ten in this case). In the cases when the original
    abs_slot value is an even multiple of the phy_map_sz (150) we do not need to shift by 10, only
    in the odd cases. */
  int multiple_of_bitmap = floor(abs_slot/phy_map_sz);
  int val_to_normalize_abs_slot = phy_map_sz * multiple_of_bitmap;
  LOG_D(NR_MAC, "This is original abs_slot %ld, multiple_of_bitmap %d, val_to_normalize_abs_slot %d, subtract amount %d\n",
        abs_slot, multiple_of_bitmap, val_to_normalize_abs_slot, (phy_map_sz % nr_slots_per_frame[get_softmodem_params()->numerology]));
  if (multiple_of_bitmap >= 1 && multiple_of_bitmap % 2 == 1) {
    val_to_normalize_abs_slot -= (phy_map_sz % nr_slots_per_frame[get_softmodem_params()->numerology]);
    if ((abs_slot - val_to_normalize_abs_slot < 0) || (abs_slot - val_to_normalize_abs_slot >= phy_map_sz)) {
      val_to_normalize_abs_slot += 2 * (phy_map_sz % nr_slots_per_frame[get_softmodem_params()->numerology]);
    }
  }
  if (val_to_normalize_abs_slot > abs_slot) {
    abs_slot += phy_map_sz;
  }
  bool sl_slot = get_bit_from_map(phy_sl_bitmap->buf, abs_slot - val_to_normalize_abs_slot) ? true : false;
  return sl_slot;
}

List_t get_nr_sl_comm_opportunities(NR_UE_MAC_INST_t *mac,
                                    uint64_t abs_idx_cur_slot,
                                    uint8_t bwp_id,
                                    uint16_t mu,
                                    uint16_t pool_id,
                                    uint8_t t1,
                                    uint16_t t2,
                                    uint8_t psfch_period) {
  frameslot_t frame_slot;
  List_t slot_info_list;
  init_list(&slot_info_list, sizeof(slot_info_t), 1);
  SL_ResourcePool_params_t *sl_tx_rsrc_pool = mac->SL_MAC_PARAMS->sl_TxPool[pool_id];
  uint16_t phy_map_sz = (sl_tx_rsrc_pool->phy_sl_bitmap.size << 3) - sl_tx_rsrc_pool->phy_sl_bitmap.bits_unused;
  LOG_D(NR_MAC, "phy_map_sz %d\n", phy_map_sz);
  NR_SL_ResourcePool_r16_t* resource_pool = get_resource_pool(mac, pool_id);

  uint64_t first_abs_slot_ind = abs_idx_cur_slot + t1;
  uint64_t last_abs_slot_ind = abs_idx_cur_slot + t2;
  uint16_t abs_pool_index = first_abs_slot_ind % phy_map_sz;

  frameslot_t fs0;
  de_normalize(abs_idx_cur_slot, mu, &fs0);

  frameslot_t fs1;
  de_normalize(first_abs_slot_ind, mu, &fs1);

  frameslot_t fs2;
  de_normalize(last_abs_slot_ind, mu, &fs2);

  bool sl_has_psfch = false;
  for (uint64_t i = first_abs_slot_ind; i <= last_abs_slot_ind; i++) {
    if (is_sl_slot(mac, &sl_tx_rsrc_pool->phy_sl_bitmap, phy_map_sz, i)) // slot is a sidelink slot
    {
      // PSCCH
      // Number of  RBs used for PSCCH
      uint8_t num_sl_pscch_rbs = pscch_rb_table[*resource_pool->sl_PSCCH_Config_r16->choice.setup->sl_FreqResourcePSCCH_r16];
      // Starting RE of the lowest subchannel in a resource where PSCCH
      // freq domain allocation starts
      uint8_t pscch_startrb = *resource_pool->sl_StartRB_Subchannel_r16;
      // Number of symbols used for PSCCH
      uint16_t num_sl_pscch_sym = pscch_tda[*resource_pool->sl_PSCCH_Config_r16->choice.setup->sl_TimeResourcePSCCH_r16];
      LOG_D(NR_MAC, "pscch_startrb %d, num_sl_pscch_sym %d, pscch_numrbs %d\n",
            pscch_startrb,
            num_sl_pscch_sym,
            num_sl_pscch_rbs);
      uint8_t start_sl_pscch_sym = 1;
      // PSSCH
      uint16_t sl_pssch_sym_start = *mac->sl_bwp->sl_BWP_Generic_r16->sl_StartSymbol_r16;
      sl_has_psfch = slot_has_psfch(mac, &sl_tx_rsrc_pool->phy_sl_bitmap, i, psfch_period, phy_map_sz, mac->SL_MAC_PARAMS->sl_TDD_config);
      int num_psfch_symbols = 0;
      if (sl_has_psfch && resource_pool->sl_PSFCH_Config_r16 && resource_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_Period_r16
          && *resource_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_Period_r16 > 0) {
        // As per 38214 8.1.3.2, num_psfch_symbols can be 3 if psfch_overhead_indication.nbits is 1; FYI psfch_overhead_indication.nbits is set to 1 in case of PSFCH period 2 or 4 in sl_determine_sci_1a_len()
        num_psfch_symbols = 3;
      }

      // PSFCH requires an additional 3 symbols
      uint16_t sl_pssch_sym_len = 7 + *mac->sl_bwp->sl_BWP_Generic_r16->sl_LengthSymbols_r16 - num_psfch_symbols - 2;
      LOG_D(NR_MAC, "Tx sl_has_psfch %d, %4d.%2d sl_pssch_sym_len %d\n", sl_has_psfch, frame_slot.frame, frame_slot.slot, sl_pssch_sym_len);

      uint16_t sl_subchannel_size = sl_get_subchannel_size(resource_pool);
      uint16_t sl_max_num_reserve = *resource_pool->sl_UE_SelectedConfigRP_r16->sl_MaxNumPerReserve_r16;
      uint64_t abs_slot_idx = i;
      uint64_t st_offset = (i - abs_idx_cur_slot);

      slot_info_t slot_info = {.sl_pscch_sym_start = start_sl_pscch_sym,
                               .sl_pscch_sym_len   = num_sl_pscch_sym,
                               .num_sl_pscch_rbs   = num_sl_pscch_rbs,
                               .sl_pssch_sym_start = sl_pssch_sym_start,
                               .sl_pssch_sym_len   = sl_pssch_sym_len,
                               .slot_offset        = st_offset,
                               .abs_slot_index     = abs_slot_idx,
                               .sl_max_num_per_reserve = sl_max_num_reserve,
                               .sl_sub_chan_size       = sl_subchannel_size,
                               .sl_has_psfch           = sl_has_psfch};
      de_normalize(slot_info.abs_slot_index, mu, &frame_slot);
      LOG_D(NR_MAC, "Pushing %4d.%2d\n", frame_slot.frame, frame_slot.slot);
      validate_selected_sl_slot(true , false, mac->SL_MAC_PARAMS->sl_TDD_config, frame_slot);
      push_back(&slot_info_list, &slot_info);
    }
    abs_pool_index = (abs_pool_index + 1) % phy_map_sz;
  }

  LOG_D(NR_MAC, "Total number of slots available for Sidelink in the selection window = %ld\n", slot_info_list.size);

#ifdef SLOT_INFO_DEBUG
  for (size_t i = 0; i < slot_info_list.size; i++) {
    slot_info_t *slot_inf = (slot_info_t*)((char*)slot_info_list.data + i * slot_info_list.element_size);
    LOG_D(NR_MAC, "sidelink pscch (sym_start %d, sym_len %d, pscch_rbs %d), slot_offset %d, abs_slot_index %ld, max_num_per_reserve %d, sub_chan_size %d\n",
          slot_inf->sl_pscch_sym_start,
          slot_inf->sl_pscch_sym_len,
          slot_inf->num_sl_pscch_rbs,
          slot_inf->slot_offset,
          slot_inf->abs_slot_index,
          slot_inf->sl_max_num_per_reserve,
          slot_inf->sl_sub_chan_size);
  }
#endif
  return slot_info_list;
}

int get_physical_sl_pool(NR_UE_MAC_INST_t *mac, BIT_STRING_t *sl_time_rsrc, BIT_STRING_t *phy_sl_bitmap) {
  /*
    Following code is to create physical sidelink bitmap as mentioned in this paper:
    Ali, Z., Lagén, S., Giupponi, L., & Rouil, R. (2021). 3GPP NR V2X mode 2: Overview, models and system-level evaluation. IEEE Access, 9, 89554-89579.
  */
  sl_nr_ue_mac_params_t *sl_mac = mac->SL_MAC_PARAMS;
  uint8_t mu = get_softmodem_params()->numerology;
  int n_slots_frame = nr_slots_per_frame[mu]; // tdd pattern len
  NR_TDD_UL_DL_Pattern_t *tdd = &sl_mac->sl_TDD_config->pattern1;
  int ul_slots_period = tdd ? tdd->nrofUplinkSlots + (tdd->nrofUplinkSymbols > 0 ? 1 : 0) : n_slots_frame;
  LOG_D(NR_MAC, "n_slots_frame %d, get_nb_periods_per_frame(tdd->dl_UL_TransmissionPeriodicity) %d\n", n_slots_frame, get_nb_periods_per_frame(tdd->dl_UL_TransmissionPeriodicity));
  const int nr_slots_period = tdd ? n_slots_frame / get_nb_periods_per_frame(tdd->dl_UL_TransmissionPeriodicity) : n_slots_frame;
  LOG_D(NR_RRC, "This is the nr_slots_period %d, ul_slots_period %d, mac->sl_bitmap.bits_unused %d size %ld, phy_bitmap size %ld\n",
        nr_slots_period, ul_slots_period, sl_time_rsrc->bits_unused, sl_time_rsrc->size, phy_sl_bitmap->size);

  int tdd_pattern_len = nr_slots_period;
  int8_t sl_bitmap_num_bits = ((sl_time_rsrc->size << 3) - sl_time_rsrc->bits_unused);
  int phy_sl_bits = sl_bitmap_num_bits + (sl_bitmap_num_bits / ul_slots_period * (nr_slots_period - ul_slots_period));
  AssertFatal(ul_slots_period > 0, "No UL slot found in the given TDD pattern");
  AssertFatal(sl_bitmap_num_bits % ul_slots_period == 0, "SL bit map size should be multiple of number of UL slots in the TDD pattern");
  AssertFatal(sl_bitmap_num_bits > tdd_pattern_len, "SL bit map size %ld should be greater than or equal to the TDD pattern size %d", sl_time_rsrc->size, tdd_pattern_len);

#ifdef BITMAP_DEBUG
  for (int k = 0; k < sl_time_rsrc->size; k++) {
    LOG_D(NR_MAC, "sl_bitmap %2x\n", sl_time_rsrc->buf[k]);
  }
#endif

  int tdd_bit_idx = 0;
  bool is_UL = 0;
  int phy_sl_bit_pos = 0;
  int sl_bitmap_pos = 0;
  bool is_sidelink_slot;
  do {
    is_sidelink_slot = get_bit_from_map(sl_time_rsrc->buf, sl_bitmap_pos);
    is_UL = (mac->ulsch_slot_bitmap[tdd_bit_idx / 64] & ((uint64_t)1 << (tdd_bit_idx % 64)));
    if (is_UL == false) {
      append_bit(phy_sl_bitmap->buf, phy_sl_bit_pos, 0);
      phy_sl_bit_pos++;
    } else if (is_sidelink_slot) {
      LOG_D(NR_MAC, "is_SL %d phy_sl_bit_pos %d sl_bitmap_pos %d\n",
            is_sidelink_slot,
            phy_sl_bit_pos,
            sl_bitmap_pos);
      append_bit(phy_sl_bitmap->buf, phy_sl_bit_pos, 1);
      phy_sl_bit_pos++;
      sl_bitmap_pos++;
    } else {
        append_bit(phy_sl_bitmap->buf, phy_sl_bit_pos, 0);
        phy_sl_bit_pos++;
        sl_bitmap_pos++;
    }
    LOG_D(NR_MAC, "tdd_bit_idx %d/%d, sl_bitmap pos: %d/%d\n",
          tdd_bit_idx,
          tdd_pattern_len - 1,
          sl_bitmap_pos,
          sl_bitmap_num_bits);
    if (tdd_bit_idx == (tdd_pattern_len - 1)) {
      if (sl_bitmap_pos == sl_bitmap_num_bits) {
        break;
      } else {
        tdd_bit_idx = 0;
      }
    } else {
      tdd_bit_idx++;
    }
  } while (tdd_bit_idx != (tdd_pattern_len));
  AssertFatal(phy_sl_bit_pos == phy_sl_bits,  "Physical bitmap length and increment counter are not matching!!!");

#ifdef BITMAP_DEBUG
  for (int i = 0; i < (phy_sl_bit_pos + 7) >> 3; i++) {
    LOG_D(NR_MAC, "phy_sl_bitmap[%d] %2x\n", i, phy_sl_bitmap->buf[i]);
  }
#endif

  return phy_sl_bit_pos;
}

List_t* get_candidate_resources_from_slots(frameslot_t *sfn,
                                           uint8_t psfch_period,
                                           uint8_t min_time_gap_psfch,
                                           uint16_t l_subch,
                                           uint16_t total_subch,
                                           List_t* slot_info,
                                           uint8_t mu) {
  LOG_D(NR_MAC, "%4d.%2d, psfch_period %d,  min_time_gap_psfch %d\n",
        sfn->frame, sfn->slot, psfch_period,  min_time_gap_psfch);

  List_t *nr_resource_list = (List_t *)malloc16_clear(sizeof(*nr_resource_list));
  init_list(nr_resource_list, sizeof(sl_resource_info_t), 1);
  sl_resource_info_t *rsrc_info = (sl_resource_info_t *)malloc16_clear(sizeof(*rsrc_info));
  for (int s = 0; s < slot_info->size; s++) {
    for (uint16_t i = 0; i + l_subch <= total_subch; i += l_subch) {
        slot_info_t *s_info = (slot_info_t*)((char*)slot_info->data + s * slot_info->element_size);
        frameslot_t frame_slot;
        de_normalize(normalize(sfn, mu) + s_info->slot_offset, mu, &frame_slot);
        rsrc_info->num_sl_pscch_rbs = s_info->num_sl_pscch_rbs,
        rsrc_info->sl_pscch_sym_start = s_info->sl_pscch_sym_start,
        rsrc_info->sl_pscch_sym_len = s_info->sl_pscch_sym_len,
        rsrc_info->sl_pssch_sym_start = s_info->sl_pssch_sym_start,
        rsrc_info->sl_pssch_sym_len = s_info->sl_pssch_sym_len,
        rsrc_info->sl_subchan_size = s_info->sl_sub_chan_size,
        rsrc_info->sl_subchan_start = i;
        rsrc_info->sl_subchan_len = l_subch,
        rsrc_info->sl_max_num_per_reserve = s_info->sl_max_num_per_reserve,
        rsrc_info->sfn.frame = frame_slot.frame;
        rsrc_info->sfn.slot = frame_slot.slot;
        rsrc_info->sl_psfch_period = psfch_period;
        rsrc_info->sl_min_time_gap_psfch = min_time_gap_psfch;
        LOG_D(NR_MAC, "abs slot %ld, capacity %ld size %ld subchan: %d/%d slot %d/%ld frame_slot %4d.%2d\n",
              normalize(sfn, mu) + s_info->slot_offset,
              nr_resource_list->capacity, nr_resource_list->size, i, total_subch, s, slot_info->size, rsrc_info->sfn.frame, rsrc_info->sfn.slot);
        push_back(nr_resource_list, rsrc_info);
    }
  }
  return nr_resource_list;
}

void exclude_resources_based_on_history(frameslot_t frame_slot,
                                        List_t* transmit_history,
                                        List_t* candidate_resources,
                                        List_t* sl_rsrc_rsrv_period_list,
                                        uint8_t mu) {

  LOG_D(NR_MAC, "abs_slot %ld, size (transmit_history: %ld, candidate_resources: %ld, sl_rsrc_rsrv_period: %ld)\n",
        normalize(&frame_slot, 1), transmit_history->size, candidate_resources->size, sl_rsrc_rsrv_period_list->size);

  List_t sfn_to_exclude; // SFN slot numbers (normalized) to exclude
  init_list(&sfn_to_exclude, sizeof(uint64_t), 1);
  sl_resource_info_t* sl_rsrc_info = (sl_resource_info_t*) get_front(candidate_resources);
  uint64_t first_sfn_norm = normalize(&sl_rsrc_info->sfn, mu); // lowest candidate SFN slot number

  sl_rsrc_info = (sl_resource_info_t*) get_back(candidate_resources);
  uint64_t last_sfn_norm = normalize(&sl_rsrc_info->sfn, mu); // highest candidate SFN slot number
  LOG_D(NR_MAC, "Excluding resources between SFNs (%lu, %lu)\n", first_sfn_norm, last_sfn_norm);

  // Iterate the resource reserve period list and the transmit history to
  // find all slot numbers such that multiples of the reserve period, when
  // added to the history's slot number, are within the candidate resource
  // slots lowest and highest numbers
  for (int k = 0; k < sl_rsrc_rsrv_period_list->size; k++) {
    uint16_t *rsrv_period = (uint16_t*)((char*)sl_rsrc_rsrv_period_list->data + k * sl_rsrc_rsrv_period_list->element_size);
    if (*rsrv_period == 0) {
        continue; // 0ms value is ignored
    }
    *rsrv_period = *rsrv_period * (1 << mu); // Convert from ms to slots
    for (int j = 0; j < transmit_history->size; j++) {
      uint16_t i = 1;
      frameslot_t *sfn = (frameslot_t*)((char*)transmit_history->data + j * transmit_history->element_size);
      uint64_t sfn_to_check = normalize(sfn, mu) + (*rsrv_period);
      while (sfn_to_check <= last_sfn_norm) {
        if (sfn_to_check >= first_sfn_norm) {
          push_back(&sfn_to_exclude, &sfn_to_check);
        }
        i++;
        sfn_to_check = normalize(sfn, mu) + (i) * (*rsrv_period);
      }
    }
  }

  // sfn_to_exclude is a set of SFN normalized slot numbers for which we need
  // to exclude (erase) any candidate resources that match
  for (int k = 0; k < sfn_to_exclude.size; k++) {
    uint64_t *norm_sfn = (uint64_t*)((char*)sfn_to_exclude.data + k * sfn_to_exclude.element_size);
    for (int j = 0; j < candidate_resources->size; j++) {
      sl_resource_info_t *rsrc_info = (sl_resource_info_t*)((char*)candidate_resources->data + j * candidate_resources->element_size);
      uint64_t norm_rsrc_info_sfn = normalize(&rsrc_info->sfn, mu);
      if (norm_rsrc_info_sfn == *norm_sfn)
      {
        LOG_D(NR_MAC, "Erasing candidate resource at %lu\n", *norm_sfn);
        delete_at(candidate_resources, j);
      }
    }
  }
}

List_t exclude_reserved_resources(sensing_data_t *sensed_data,
                                  float slot_period_ms,
                                  uint16_t resv_period_slots,
                                  uint16_t t1,
                                  uint16_t t2,
                                  uint8_t mu) {

  LOG_D(NR_MAC, "sfn %ld, %4d.%2d slot_period %f, resv_period_slots %d, gap_re_tx1 %d, gap_re_tx2 %d\n",
        normalize(&sensed_data->frame_slot, mu),
        sensed_data->frame_slot.frame,
        sensed_data->frame_slot.slot,
        slot_period_ms,
        resv_period_slots,
        sensed_data->gap_re_tx1,
        sensed_data->gap_re_tx2);

  List_t resource_list;
  init_list(&resource_list, sizeof(reserved_resource_t), 1);
  AssertFatal(slot_period_ms <= 1, "Slot length can not exceed 1 ms\n");
  // slot range is [n + T1, n + T2] (both endpoints included)
  uint16_t window_slots = (t2 - t1) + 1; // selection window length in physical slots
  double t_scal_ms = window_slots * slot_period_ms; // Parameter T_scal in the algorithm
  double p_rsvp_ms = (double)(sensed_data->rsvp); // Parameter Pprime_rsvp_rx in algorithm
  uint16_t q = 0;                                        // Parameter Q in the algorithm

  if (sensed_data->rsvp != 0) {
    if (p_rsvp_ms < t_scal_ms) {
      q = (uint16_t)(ceil(t_scal_ms / p_rsvp_ms));
    } else {
      q = 1;
    }
    LOG_D(NR_MAC, "t_scal_ms: %lf, p_rsvp_ms: %lf\n", t_scal_ms, p_rsvp_ms);
  }

  uint16_t p_prime_rsvp_rx = resv_period_slots;
  for (uint16_t i = 1; i <= q; i++) {
    reserved_resource_t resource = {.sfn = sensed_data->frame_slot,
                                    .rsvp = sensed_data->rsvp,
                                    .sb_ch_length = sensed_data->subch_len,
                                    .sb_ch_start = sensed_data->subch_start,
                                    .prio = sensed_data->prio,
                                    .sl_rsrp = sensed_data->sl_rsrp
                                    };
    resource.sfn = add_to_sfn(&resource.sfn, p_prime_rsvp_rx, mu);
    push_back(&resource_list, &resource);
    if (sensed_data->gap_re_tx1 != 0 && sensed_data->gap_re_tx1 != 0xFF) {
      reserved_resource_t re_tx1_slot = resource;
      re_tx1_slot.sfn = add_to_sfn(&re_tx1_slot.sfn, sensed_data->gap_re_tx1, mu);
      re_tx1_slot.sb_ch_length = sensed_data->subch_len;
      re_tx1_slot.sb_ch_start = sensed_data->subch_startre_tx1;
      push_back(&resource_list, &re_tx1_slot);
    }
    if (sensed_data->gap_re_tx1 != 0 && sensed_data->gap_re_tx2 != 0xFF) {
      reserved_resource_t re_tx2_slot = resource;
      re_tx2_slot.sfn = add_to_sfn(&re_tx2_slot.sfn, sensed_data->gap_re_tx2, mu);
      re_tx2_slot.sb_ch_length = sensed_data->subch_len;
      re_tx2_slot.sb_ch_start = sensed_data->subch_startre_tx2;
      push_back(&resource_list, &re_tx2_slot);
    }
  }
  LOG_D(NR_MAC, "q: %d,  Size of resource_list: %ld\n", q, resource_list.size);
  return resource_list;
}

bool overlapped_resource(uint8_t first_start,
                         uint8_t first_length,
                         uint8_t second_start,
                         uint8_t second_length) {
  AssertFatal(first_length && second_length, "Length should not be zero\n");
  return (max(first_start, second_start) < min(first_start + first_length, second_start + second_length));
}

uint8_t get_lower_bound_resel_counter(uint16_t p_rsrv) {
    AssertFatal(p_rsrv < 100, "Resource reservation must be less than 100 ms");
    uint8_t l_bound = (5 * ceil(100 / (max(20, p_rsrv))));
    return l_bound;
}

uint8_t get_upper_bound_resel_counter(uint16_t p_rsrv) {
    AssertFatal(p_rsrv < 100, "Resource reservation must be less than 100 ms");
    uint8_t u_bound = (15 * ceil(100 / (max((20), p_rsrv))));
    return u_bound;
}

uint8_t get_random_reselection_counter(uint16_t rri) {
    uint8_t min_res_cntr = 0;
    uint8_t max_res_cntr = 0;

    switch (rri)
    {
    case 100:
    case 150:
    case 200:
    case 250:
    case 300:
    case 350:
    case 400:
    case 450:
    case 500:
    case 550:
    case 600:
    case 700:
    case 750:
    case 800:
    case 850:
    case 900:
    case 950:
    case 1000:
        min_res_cntr = 5;
        max_res_cntr = 15;
        break;
    default:
        if (rri < 100) {
          min_res_cntr = get_lower_bound_resel_counter(rri);
          max_res_cntr = get_upper_bound_resel_counter(rri);
        } else {
            LOG_E(NR_MAC, "Value not supported!");
        }
        break;
    }

    LOG_D(NR_MAC, "Range to choose random reselection counter. min: %d max: %d\n", min_res_cntr, max_res_cntr);
    return min_res_cntr;
}

List_t* get_candidate_resources(frameslot_t *frame_slot, NR_UE_MAC_INST_t *mac, List_t *sensing_data, List_t *transmit_history) {

  uint16_t pool_id = 0;
  uint8_t bwp_id = 0;
  sl_nr_ue_mac_params_t *sl_mac = mac->SL_MAC_PARAMS;
  uint8_t mu = sl_mac->sl_phy_config.sl_config_req.sl_bwp_config.sl_scs;
  nr_sl_transmission_params_t *sl_tx_params = &sl_mac->mac_tx_params;
  uint8_t t1 = sl_mac->sl_TxPool[pool_id]->t1;
  uint8_t tproc1 = sl_mac->sl_TxPool[pool_id]->tproc1;
  uint16_t t2 = get_t2(pool_id, mu, sl_tx_params, sl_mac);

  AssertFatal(check_t1_within_tproc1(mu, t1), "Configured t1 %d is greater than tproc1 %d for this numerology", t1, tproc1);

  LOG_D(NR_MAC, "Transmit  size: %ld; sensing data size: %ld\n", transmit_history->size, sensing_data->size);

  List_t candidate_slots;
  List_t *candidate_resources;
  uint64_t abs_slot_ind = normalize(frame_slot, mu);

  // Check the validity of the resource selection window configuration (t1 and t2)
  // and the following parameters: numerology and reservation period.
  uint16_t num_slots_mul_s_dur_ms = (t2 - t1 + 1) * (1 / pow(2, mu)); // number of slots multiplied by the slot duration in ms

  uint16_t rsvpMs = sl_tx_params->rri;
  LOG_D(NR_MAC, "abs_slot_ind %ld, %4d.%2d rsvpMs %hu, num_slots_mul_s_dur %d, t2 %d, t1 %d, (t2 - t1 + 1) %d, tproc1 %d\n",
        abs_slot_ind, frame_slot->frame, frame_slot->slot,
        rsvpMs, num_slots_mul_s_dur_ms, t2, t1, (t2 - t1 + 1),
        tproc1);
  AssertFatal(rsvpMs != 0 && num_slots_mul_s_dur_ms <= rsvpMs, "An error may be generated due to the fact that the resource selection window" \
                    "size is higher than the resource reservation period value. Make sure that " \
                    "(T2-T1+1) x (1/(2^numerology)) < reservation period. Modify the values of T1, " \
                    "T2, numerology, and reservation period accordingly.");

  uint16_t l_subch = 1;
  uint16_t total_subch = *mac->sl_tx_res_pool->sl_NumSubchannel_r16;
  uint8_t psfch_time_gaps[] = {2, 3};
  uint8_t min_time_gap_psfch = mac->sl_tx_res_pool->sl_PSFCH_Config_r16 ? psfch_time_gaps[*mac->sl_tx_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_MinTimeGapPSFCH_r16] : 0;

  uint8_t psfch_period = 0;
  const uint8_t psfch_periods[] = {0,1,2,4};
  psfch_period = (mac->sl_tx_res_pool->sl_PSFCH_Config_r16 &&
                  mac->sl_tx_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_Period_r16)
                  ? psfch_periods[*mac->sl_tx_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_Period_r16] : 0;

  // step 4 as per TS 38.214 sec 8.1.4
  // Find sidelink slots from tx_phy_sl_bitmap
  candidate_slots = get_nr_sl_comm_opportunities(mac,
                                                 abs_slot_ind,
                                                 bwp_id,
                                                 mu,
                                                 pool_id,
                                                 t1,
                                                 t2,
                                                 psfch_period);

  if (candidate_slots.size == 0 ) {
    return NULL;
  }

  // Get candidate resources from sidelink slots
  candidate_resources = get_candidate_resources_from_slots(frame_slot,
                                                           psfch_period,
                                                           min_time_gap_psfch,
                                                           l_subch,
                                                           total_subch,
                                                           &candidate_slots,
                                                           mu);
  print_candidate_list(candidate_resources, __LINE__);

  uint64_t m_total = candidate_resources->size; // total number of candidate single-slot resources

  // This is an optimization to skip further null processing below
  if ((sensing_data->size == 0) && (transmit_history->size == 0))
  {
    LOG_D(NR_MAC, "No sensing or data found: Total slots selected %ld\n", m_total);
    return candidate_resources;
  }

  // Copy the buffer so we can trim the buffer as per Tproc0.
  // Note, we do not need to delete the latest measurement
  // from the original buffer because it will be deleted
  // by RemoveOldSensingData method once it is outdated.

  List_t *updated_sensing_data = sensing_data;

  // latest sensing data is at the end of the list
  // now remove the sensing data as per the value of Tproc0. This would
  // keep the size of the buffer equal to [n – T0 , n – Tproc0)

  update_sensing_data(updated_sensing_data, frame_slot, sl_mac, pool_id);

  // Perform a similar operation on the transmit history.
  // latest is at the end of the list
  // keep the size of the buffer equal to [n – T0 , n – Tproc0)
  List_t *updated_history = transmit_history;

  update_transmit_history(updated_history, frame_slot, sl_mac, pool_id);

  // step 5: filter candidateResources based on transmit history, if threshold
  // defined in step 5a) is met
  List_t *remaining_candidates = candidate_resources;
  LOG_D(NR_MAC, "size: (candidate_resources %ld, remaining_candidates %ld, updated_history %ld)\n",
        candidate_resources->size, remaining_candidates->size, updated_history->size);

  // Exclude resources function may not be effective if updated history is empty
  List_t *rsrc_rsrvation_period_list = malloc16_clear(sizeof(*rsrc_rsrvation_period_list));
  init_list(rsrc_rsrvation_period_list, sizeof(long), 1);
  push_back(rsrc_rsrvation_period_list, &sl_mac->mac_tx_params.rri);
  exclude_resources_based_on_history(*frame_slot, updated_history, remaining_candidates, rsrc_rsrvation_period_list, mu);

  LOG_D(NR_MAC, "sl_res_percentage %f, %lf, %lf\n",
        mac->sl_res_percentage, mac->sl_res_percentage / 100.0, (mac->sl_res_percentage / 100.0) * m_total);
  if (remaining_candidates->size >= (mac->sl_res_percentage / 100.0) * m_total) {
    LOG_D(NR_MAC, "Step 5a check allows step 5 to pass: original: %ld  remaining: %ld X: %lf\n",
          candidate_resources->size, remaining_candidates->size, mac->sl_res_percentage / 100.0);
  } else {
    LOG_D(NR_MAC, "Step 5a fails-- too few remaining candidates: original: %ld  updated: %ld  X: %lf", candidate_resources->size, remaining_candidates->size, mac->sl_res_percentage / 100.0);
    remaining_candidates = candidate_resources;
  }

  // step 6

  // calculate all possible transmissions based on sensed SCIs,
  // with past transmissions projected into the selection window.
  // Using a vector of ReservedResource, since we need to check all the SCIs
  // and their possible future transmission that are received during the
  // above trimmed sensing window. Each element of the vector holds a
  // list that holds the info of each received SCI and its possible
  // future transmissions.

  vec_of_list_t sensing_data_projections;
  init_vector(&sensing_data_projections, 1);
  add_list(&sensing_data_projections, sizeof(reserved_resource_t), 1);
  uint8_t nr_slots_per_subframe = pow(2, mu);
  float slot_duraton_ms = (1.0 / nr_slots_per_subframe);
  print_sensing_data_list(updated_sensing_data, __LINE__);
  for (int k = 0; k < updated_sensing_data->size; k++) {
    sensing_data_t *itr_sdata = (sensing_data_t*)((char*)updated_sensing_data->data + k * updated_sensing_data->element_size);
    uint16_t resv_period_slots = time_to_slots(mu, itr_sdata->rsvp);
    LOG_D(NR_MAC, "sfn %ld, %4d.%2d slot_period %f, resv_period_slots %d, gap_re_tx1 %d, gap_re_tx2 %d\n",
          normalize(&itr_sdata->frame_slot, mu),
          itr_sdata->frame_slot.frame,
          itr_sdata->frame_slot.slot,
          slot_duraton_ms,
          resv_period_slots,
          itr_sdata->gap_re_tx1,
          itr_sdata->gap_re_tx2);
    itr_sdata->gap_re_tx1 = 0;
    itr_sdata->gap_re_tx2 = 0;
    List_t temp_rsrc_list = exclude_reserved_resources(itr_sdata,
                                                       slot_duraton_ms,
                                                       resv_period_slots,
                                                       t1,
                                                       t2,
                                                       mu);
    LOG_D(NR_MAC, "k %d, Inserting list of size %ld\n", k, temp_rsrc_list.size);
    push_back_list(&sensing_data_projections, &temp_rsrc_list);
  }

  int rsrp_threshold = mac->sl_thresh_rsrp;
  List_t* candidate_resources_after_step5 = remaining_candidates;
  int counter_c = 0;
  do
  {
    // following assignment is needed since we might have to perform
    // multiple do-while over the same list by increasing the rsrpThreshold
    remaining_candidates = candidate_resources_after_step5;
    LOG_D(NR_MAC, "Step 6 loop iteration checking %ld resources against threshold %d resel counter %d counter_c %d\n",
          remaining_candidates->size, rsrp_threshold, sl_tx_params->resel_counter, counter_c);

    // itr_rsrc is the candidate single-slot resource R_x, y
    // k increment is conditional based on delete action
    int k = 0;
    while ( k < remaining_candidates->size) {
      sl_resource_info_t *itr_rsrc = (sl_resource_info_t*)((char*)remaining_candidates->data + k * remaining_candidates->element_size);
      bool erased = false;
      itr_rsrc->slot_busy = false;
      // calculate all proposed transmissions of current candidate resource within selection
      // window
      List_t *resource_info_list = calloc(1, sizeof(*resource_info_list));
      init_list(resource_info_list, sizeof(sl_resource_info_t), 1);
      uint16_t p_prime_rsvp_tx = time_to_slots(mu, sl_tx_params->rri);
      for (uint16_t i = 0; i < sl_tx_params->resel_counter; i++) {
        sl_resource_info_t sl_resource_info;
        sl_resource_info.sfn = itr_rsrc->sfn;
        frameslot_t fs = sl_resource_info.sfn;
        sl_resource_info.sfn = add_to_sfn(&fs, p_prime_rsvp_tx, mu);
        LOG_D(NR_MAC, "sfn %4d.%2d, %4d.%2d i * p_prime_rsvp_tx %d\n", itr_rsrc->sfn.frame, itr_rsrc->sfn.slot, fs.frame, fs.slot, i * p_prime_rsvp_tx);
        push_back(resource_info_list, &sl_resource_info);
      }

      // Traverse over all the possible transmissions derived from each sensed SCI
      for (int i = 0; i < sensing_data_projections.size; i++) {
        List_t proj_reserved_rsc_list = sensing_data_projections.lists[i];
        print_reserved_list(&proj_reserved_rsc_list, __LINE__);
        // for all proposed transmissions of current candidate resource
        for (int j = 0; j < resource_info_list->size; j++) {
          sl_resource_info_t *future_cand_info = (sl_resource_info_t*)((char*)resource_info_list->data + j * resource_info_list->element_size);

          // Traverse the list of future projected transmissions for the given sensed SCI
          for (int l = 0; l < proj_reserved_rsc_list.size; l++) {
            reserved_resource_t *rsrvd_rsc = (reserved_resource_t*)((char*)proj_reserved_rsc_list.data + l * proj_reserved_rsc_list.element_size);
            LOG_D(NR_MAC, "future candidate %ld rsrvd_rsc candidate %ld\n", normalize(&future_cand_info->sfn, mu), normalize(&rsrvd_rsc->sfn, mu));
            // If overlapped in time ...
            if (normalize(&future_cand_info->sfn, mu) == normalize(&rsrvd_rsc->sfn, mu)) {
              LOG_D(NR_MAC, "%4d.%2d rsrvd_rsc->sl_rsrp %lf, rsrp_threshold %d\n", rsrvd_rsc->sfn.frame, rsrvd_rsc->sfn.slot, rsrvd_rsc->sl_rsrp, rsrp_threshold);
              // And above the current threshold ...
              if (rsrvd_rsc->sl_rsrp > rsrp_threshold) {
                // And overlapped in frequency ...
                if (overlapped_resource(rsrvd_rsc->sb_ch_start,
                                        rsrvd_rsc->sb_ch_length,
                                        itr_rsrc->sl_subchan_start,
                                        itr_rsrc->sl_subchan_len)) {
                  LOG_D(NR_MAC, "%4d.%2d Overlapped resource %ld occupied %d subchannels index %d\n",
                        rsrvd_rsc->sfn.frame, rsrvd_rsc->sfn.slot,
                        normalize(&itr_rsrc->sfn, mu), rsrvd_rsc->sb_ch_length, rsrvd_rsc->sb_ch_start);
                  delete_at(remaining_candidates, k);
                  LOG_D(NR_MAC, "Resource %ld %4d.%2d : [%d,%d] erased. Its rsrp : %lf  Threshold : %d\n",
                        normalize(&itr_rsrc->sfn, mu),
                        itr_rsrc->sfn.frame,
                        itr_rsrc->sfn.slot,
                        itr_rsrc->sl_subchan_start,
                        (itr_rsrc->sl_subchan_start + itr_rsrc->sl_subchan_len - 1),
                        rsrvd_rsc->sl_rsrp,
                        rsrp_threshold);
                  erased = true; // Used to break out of outer for loop of sensed
                                 // data projections
                  break; // Stop further evaluation because candidate is erased
                } else {
                  // Although not overlapping in frequency, overlapped in time
                  future_cand_info->slot_busy = true;
                }
              }
            }
          }
        }
        if (erased) {
          break; // break for proj_reserved_rsc_list
        }
      }
      if (!erased) {
        // Only need to increment if not erased above; if erased, the erase()
        // action will point itCandidate to the next item
        k++;
      }
    } //end of while

    // step 7. If the following while will not break, start over do-while
    // loop with rsrpThreshold increased by 3dB
    rsrp_threshold += 3;
    if (rsrp_threshold > 0) {
      // 0 dBm is the maximum RSRP threshold level so if we reach
      // it, that means all the available slots are overlapping
      // in time and frequency with the sensed slots, and the
      // RSRP of the sensed slots is very high.
      LOG_D(NR_MAC, "Reached maximum RSRP threshold, unable to select resources\n");
      for (int z = 0; z < remaining_candidates->size; z++) {
        delete_at(remaining_candidates, z);
      }
      break; // break do while
    }
    counter_c++;
  } while (remaining_candidates->size < (mac->sl_res_percentage / 100.0) * m_total);

  LOG_D(NR_MAC, "%ld resources selected after sensing resource selection from %ld slots\n", remaining_candidates->size, m_total);
  return remaining_candidates;
}