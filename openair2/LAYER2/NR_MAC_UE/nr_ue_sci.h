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

/* \file nr_ue_sci.h
 * \brief Definitions and Structures for sci/slsch procedures for Sidelink UE
 * \author R. Knopp
 * \date 2023
 * \version 0.1
 * \company Eurecom
 * \email: knopp@eurecom.fr
 * \note
 * \warning
 */

#ifndef __LAYER2_NR_UE_SCI_H__
#define __LAYER2_NR_UE_SCI_H__
#include "NR_MAC_COMMON/nr_mac.h"

typedef enum {
  NR_SL_SCI_FORMAT_1A = 0,
  NR_SL_SCI_FORMAT_2A = 1,
  NR_SL_SCI_FORMAT_2B = 2,
  NR_SL_SCI_FORMAT_2C = 3
} nr_sci_format_t;

typedef struct {
	// 1st stage fields
	uint8_t priority; // 3 bits
	dci_field_t frequency_resource_assignment; // depending on sl-MaxNumPerReserve and N_subChannel^SL 
	dci_field_t time_resource_assignment; // depending on sl_MaxNumPerReserve
	dci_field_t resource_reservation_period; // sl-ResourceReservePeriodList and sl-MultiReserveResource
	dci_field_t dmrs_pattern; // depending on N_pattern and sl-PSSCH-DMRS-TimePatternList
	uint8_t second_stage_sci_format; // 2 bits - Table 8.3.1.1-1
        uint8_t beta_offset_indicator; // 2 bits - depending sl-BetaOffsets2ndSCI and Table 8.3.1.1-2
	uint8_t number_of_dmrs_port; // 1 bit - Table 8.3.1.1-3
	uint8_t mcs; // 5 bits
	dci_field_t additional_mcs; // depending on sl-Additional-MCS-Table
	dci_field_t psfch_overhead; // depending on sl-PSFCH-Period
        dci_field_t reserved; // depending on N_reserved (sl-NumReservedBits) and sl-IndicationUE-B
        dci_field_t conflict_information_receiver; // depending on sl-IndicationUE-B
	// 2nd stage fields
	uint8_t harq_pid; // 4 bits
	uint8_t ndi; // 1 bit
	uint8_t rv_index; // 2 bits
	uint8_t source_id; // 8 bits
	uint16_t dest_id; // 16 bits
	uint8_t harq_feedback; //1 bit
	uint8_t cast_type; // 2 bits formac 2A
	uint8_t csi_req; // 1 bit format 2A, format 2C
	uint16_t zone_id; // 12 bits format 2B
	dci_field_t communication_range; // 4 bits depending on sl-ZoneConfigMCR-Index, format 2B
        uint8_t providing_req_ind; // 1 bit, format 2C
	dci_field_t resource_combinations; // depending on n_subChannel^SL (sl-NumSubchennel), N_rsv_period (sl-ResourceReservePeriodList) and sl-MultiReservedResource, format 2C
        uint8_t first_resource_location; // 8 bits, format 2C
	dci_field_t reference_slot_location; // depending on mu, format 2C
	uint8_t resource_set_type; // 1 bit, format 2C
	dci_field_t lowest_subchannel_indices; // depending on n_subChannel^SL, format 2C
} nr_sci_pdu_t;
#endif
