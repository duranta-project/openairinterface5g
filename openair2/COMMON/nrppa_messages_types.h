/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NRPPA_MESSAGES_TYPES_H_
#define NRPPA_MESSAGES_TYPES_H_

// Defines to access message fields.

#define NRPPA_TRP_INFORMATION_REQ(mSGpTR) (mSGpTR)->ittiMsg.nrppa_trp_information_req
#define NRPPA_TRP_INFORMATION_RESP(mSGpTR) (mSGpTR)->ittiMsg.nrppa_trp_information_resp
#define NRPPA_TRP_INFORMATION_FAILURE(mSGpTR) (mSGpTR)->ittiMsg.nrppa_trp_information_failure
#define NRPPA_POSITIONING_INFORMATION_REQ(mSGpTR) (mSGpTR)->ittiMsg.nrppa_positioning_information_req
#define NRPPA_POSITIONING_INFORMATION_RESP(mSGpTR) (mSGpTR)->ittiMsg.nrppa_positioning_information_resp
#define NRPPA_POSITIONING_ACTIVATION_REQ(mSGpTR) (mSGpTR)->ittiMsg.nrppa_positioning_activation_req
#define NRPPA_POSITIONING_ACTIVATION_RESP(mSGpTR) (mSGpTR)->ittiMsg.nrppa_positioning_activation_resp
#define NRPPA_MEASUREMENT_REQ(mSGpTR) (mSGpTR)->ittiMsg.nrppa_measurement_req
#define NRPPA_MEASUREMENT_RESP(mSGpTR) (mSGpTR)->ittiMsg.nrppa_measurement_resp
#define NRPPA_MEASUREMENT_FAILURE(mSGpTR) (mSGpTR)->ittiMsg.nrppa_measurement_failure

/* Structure of Positioning related NRPPA messages */
/* IE structures for Positioning related messages as per TS 38.455 V16.7.1*/

typedef enum nrppa_trp_information_type_item_e {
  NRPPA_TRP_INFORMATION_TYPE_ITEM_NR_PCI,
  NRPPA_TRP_INFORMATION_TYPE_ITEM_NG_RAN_CGI,
  NRPPA_TRP_INFORMATION_TYPE_ITEM_NR_ARFCN,
  NRPPA_TRP_INFORMATION_TYPE_ITEM_PRS_CONFIG,
  NRPPA_TRP_INFORMATION_TYPE_ITEM_SSB_CONFIG,
  NRPPA_TRP_INFORMATION_TYPE_ITEM_SFN_INIT_TIME,
  NRPPA_TRP_INFORMATION_TYPE_ITEM_SPATIAL_DIRECTION_INFO,
  NRPPA_TRP_INFORMATION_TYPE_ITEM_GEO_COORDINATES
} nrppa_trp_information_type_item_pr;

typedef struct nrppa_trp_information_type_list_s {
  nrppa_trp_information_type_item_pr *trp_information_type_item;
  uint8_t trp_information_type_list_length;
} nrppa_trp_information_type_list_t;

typedef struct nrppa_trp_list_item_s {
  uint32_t trp_id;
} nrppa_trp_list_item_t;

typedef struct nrppa_trp_list_s {
  nrppa_trp_list_item_t *trp_list_item;
  uint32_t trp_list_length;
} nrppa_trp_list_t;

typedef struct nrppa_access_point_position_s {
  long latitude_sign;
  long latitude;
  long longitude;
  long direction_of_altitude;
  long altitude;
  long uncertainty_semi_major;
  long uncertainty_semi_minor;
  long orientation_of_major_axis;
  long uncertainty_altitude;
  long confidence;
} nrppa_access_point_position_t;

typedef struct nrppa_ngran_high_accuracy_access_point_position_s {
  long latitude;
  long longitude;
  long altitude;
  long uncertainty_semi_major;
  long uncertainty_semi_minor;
  long orientation_of_major_axis;
  long horizontal_confidence;
  long uncertainty_altitude;
  long vertical_confidence;
} nrppa_ngran_high_accuracy_access_point_position_t;

typedef union nrppa_trp_position_direct_accuracy_c {
  nrppa_access_point_position_t trp_position;
  nrppa_ngran_high_accuracy_access_point_position_t trp_HAposition;
} nrppa_trp_position_direct_accuracy_u;

typedef enum nrppa_trp_position_direct_accuracy_e {
  NRPPA_TRP_POSITION_DIRECT_ACCURACY_PR_NOTHING,
  NRPPA_TRP_POSITION_DIRECT_ACCURACY_PR_TRPPOSITION,
  NRPPA_TRP_POSITION_DIRECT_ACCURACY_PR_TRPHAPOSITION
} nrppa_trp_position_direct_accuracy_pr;

typedef struct nrppa_trp_position_direct_accuracy_s {
  nrppa_trp_position_direct_accuracy_pr present;
  nrppa_trp_position_direct_accuracy_u choice;
} nrppa_trp_position_direct_accuracy_t;

typedef struct nrppa_trp_position_direct_s {
  nrppa_trp_position_direct_accuracy_t accuracy;
} nrppa_trp_position_direct_t;

typedef enum nrppa_reference_point_e {
  NRPPA_REFERENCE_POINT_PR_NOTHING,
  NRPPA_REFERENCE_POINT_PR_COORDINATEID,
  NRPPA_REFERENCE_POINT_PR_REFERENCEPOINTCOORDINATE,
  NRPPA_REFERENCE_POINT_PR_REFERENCEPOINTCOORDINATEHA
} nrppa_reference_point_pr;

typedef union nrppa_reference_point_c {
  long coordinate_id;
  nrppa_access_point_position_t reference_point_coordinate;
  nrppa_ngran_high_accuracy_access_point_position_t reference_point_coordinateHA;
} nrppa_reference_point_u;

typedef struct nrppa_reference_point_s {
  nrppa_reference_point_pr present;
  nrppa_reference_point_u choice;
} nrppa_reference_point_t;

typedef struct nrppa_location_uncertainty_s {
  long horizontal_uncertainty;
  long horizontal_confidence;
  long vertical_uncertainty;
  long vertical_confidence;
} nrppa_location_uncertainty_t;

typedef struct nrppa_relative_geodetic_location_s {
  long milli_arc_second_units;
  long height_units;
  long delta_latitude;
  long delta_longitude;
  long delta_height;
  nrppa_location_uncertainty_t location_uncertainty;
} nrppa_relative_geodetic_location_t;

typedef struct nrppa_relative_cartesian_location_s {
  long xyz_unit;
  long xvalue;
  long yvalue;
  long zvalue;
  nrppa_location_uncertainty_t location_uncertainty;
} nrppa_relative_cartesian_location_t;

typedef union nrppa_trp_reference_point_type_c {
  nrppa_relative_geodetic_location_t trp_position_relative_geodetic;
  nrppa_relative_cartesian_location_t trp_position_relative_cartesian;
} nrppa_trp_reference_point_type_u;

typedef enum nrppa_trp_reference_point_type_e {
  NRPPA_TRP_REFERENCE_POINT_TYPE_PR_NOTHING,
  NRPPA_TRP_REFERENCE_POINT_TYPE_PR_TRPPOSITION_RELATIVE_GEODETIC,
  NRPPA_TRP_REFERENCE_POINT_TYPE_PR_TRPPOSITION_RELATIVE_CARTESIAN
} nrppa_trp_reference_point_type_pr;

typedef struct nrppa_trp_reference_point_type_t {
  nrppa_trp_reference_point_type_pr present;
  nrppa_trp_reference_point_type_u choice;
} nrppa_trp_reference_point_type_t;

typedef struct nrppa_trp_position_referenced_t {
  nrppa_reference_point_t reference_point;
  nrppa_trp_reference_point_type_t reference_point_type;
} nrppa_trp_position_referenced_t;

typedef union nrppa_trp_position_definition_type_c {
  nrppa_trp_position_direct_t direct;
  nrppa_trp_position_referenced_t referenced;
} nrppa_trp_position_definition_type_u;

typedef enum nrppa_trp_position_definition_type_e {
  NRPPA_TRP_POSITION_DEFINITION_TYPE_PR_NOTHING,
  NRPPA_TRP_POSITION_DEFINITION_TYPE_PR_DIRECT,
  NRPPA_TRP_POSITION_DEFINITION_TYPE_PR_REFERENCED
} nrppa_trp_position_definition_type_pr;

typedef struct nrppa_trp_position_definition_type_s {
  nrppa_trp_position_definition_type_u choice;
  nrppa_trp_position_definition_type_pr present;
} nrppa_trp_position_definition_type_t;

typedef struct nrppa_geographical_coordinates_s {
  nrppa_trp_position_definition_type_t trp_position_definition_type;
} nrppa_geographical_coordinates_t;

typedef struct nrppa_ng_ran_cgi_s {
  plmn_id_t plmn;
  uint64_t nr_cellid;
} nrppa_ng_ran_cgi_t;

typedef struct nrppa_prs_resource_s {
  uint8_t prs_resource_id;
  uint16_t sequence_id;
  uint8_t re_offset;
  uint16_t resource_slot_offset;
  uint8_t resource_symbol_offset;
} nrppa_prs_resource_t;

typedef struct nrppa_prs_resource_set_s {
  uint8_t prs_resource_set_id;
  uint8_t subcarrier_spacing;
  uint16_t num_rb;
  uint16_t start_prb;
  uint32_t point_a;
  uint8_t comb_size;
  uint8_t cp_type;
  uint32_t resource_set_periodicity;
  uint32_t resource_set_slot_offset;
  uint8_t resource_repetition_factor;
  uint8_t resource_time_gap;
  uint8_t resource_number_of_symbols;
  int8_t prs_resource_transmit_power;
  uint8_t prs_resource_list_length;
  nrppa_prs_resource_t *prs_resource;
} nrppa_prs_resource_set_t;

typedef struct nrppa_prs_configuration_s {
  uint8_t prs_resource_set_list_length;
  nrppa_prs_resource_set_t *prs_resource_set;
} nrppa_prs_configuration_t;

typedef union nrppa_trp_information_type_response_item_c {
  uint16_t pci_nr;
  nrppa_ng_ran_cgi_t ng_ran_cgi;
  uint32_t nr_arfcn;
  nrppa_prs_configuration_t prs_configuration;
  nrppa_geographical_coordinates_t geographical_coordinates;
} nrppa_trp_information_type_response_item_u;

typedef enum nrppa_trp_information_type_response_item_e {
  NRPPA_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_NOTHING,
  NRPPA_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_PCI_NR,
  NRPPA_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_NG_RAN_CGI,
  NRPPA_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_NRARFCN,
  NRPPA_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_PRSCONFIGURATION,
  NRPPA_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_SSBINFORMATION,
  NRPPA_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_SFNINITIALISATIONTIME,
  NRPPA_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_SPATIALDIRECTIONINFORMATION,
  NRPPA_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_GEOGRAPHICALCOORDINATES
} nrppa_trp_information_type_response_item_pr;

typedef struct nrppa_trp_information_type_response_item_s {
  nrppa_trp_information_type_response_item_pr present;
  nrppa_trp_information_type_response_item_u choice;
} nrppa_trp_information_type_response_item_t;

typedef struct nrppa_trp_information_type_response_list_s {
  nrppa_trp_information_type_response_item_t *trp_information_type_response_item;
  uint8_t trp_information_type_response_item_length;
} nrppa_trp_information_type_response_list_t;

typedef struct nrppa_trp_information_s {
  uint32_t trp_id;
  nrppa_trp_information_type_response_list_t trp_information_type_response_list;
} nrppa_trp_information_t;

typedef enum nrppa_subcarrier_spacing_e {
  NRPPA_SUBCARRIER_SPACING_15KHZ,
  NRPPA_SUBCARRIER_SPACING_30KHZ,
  NRPPA_SUBCARRIER_SPACING_60KHZ,
  NRPPA_SUBCARRIER_SPACING_120KHZ
} nrppa_subcarrier_spacing_pr;

typedef struct nrppa_scs_specific_carrier_s {
  uint32_t offset_to_carrier;
  nrppa_subcarrier_spacing_pr subcarrier_spacing;
  uint16_t carrier_bandwidth;
} nrppa_scs_specific_carrier_t;

typedef struct nrppa_uplink_channel_bw_per_scs_list_s {
  nrppa_scs_specific_carrier_t *scs_specific_carrier;
  uint32_t scs_specific_carrier_list_length;
} nrppa_uplink_channel_bw_per_scs_list_t;

typedef enum nrppa_transmission_comb_e {
  NRPPA_TRANSMISSION_COMB_PR_NOTHING,
  NRPPA_TRANSMISSION_COMB_PR_N2,
  NRPPA_TRANSMISSION_COMB_PR_N4
} nrppa_transmission_comb_pr;

typedef struct nrppa_transmission_comb_n2_s {
  uint8_t comb_offset_n2;
  uint8_t cyclic_shift_n2;
} nrppa_transmission_comb_n2_t, nrppa_transmission_comb_pos_n2_t;

typedef struct nrppa_transmission_comb_n4_s {
  uint8_t comb_offset_n4;
  uint8_t cyclic_shift_n4;
} nrppa_transmission_comb_n4_t, nrppa_transmission_comb_pos_n4_t;

typedef union nrppa_transmission_comb_c {
  nrppa_transmission_comb_n2_t n2;
  nrppa_transmission_comb_n4_t n4;
} nrppa_transmission_comb_u;

typedef struct nrppa_transmission_comb_s {
  nrppa_transmission_comb_pr present;
  nrppa_transmission_comb_u choice;
} nrppa_transmission_comb_t;

typedef enum nrppa_srs_resource_type_periodicity_e {
  NRPPA_SRS_RESOURCE_TYPE_PERIODICITY_SLOT1 = 0,
  NRPPA_SRS_RESOURCE_TYPE_PERIODICITY_SLOT2,
  NRPPA_SRS_RESOURCE_TYPE_PERIODICITY_SLOT4,
  NRPPA_SRS_RESOURCE_TYPE_PERIODICITY_SLOT5,
  NRPPA_SRS_RESOURCE_TYPE_PERIODICITY_SLOT8,
  NRPPA_SRS_RESOURCE_TYPE_PERIODICITY_SLOT10,
  NRPPA_SRS_RESOURCE_TYPE_PERIODICITY_SLOT16,
  NRPPA_SRS_RESOURCE_TYPE_PERIODICITY_SLOT20,
  NRPPA_SRS_RESOURCE_TYPE_PERIODICITY_SLOT32,
  NRPPA_SRS_RESOURCE_TYPE_PERIODICITY_SLOT40,
  NRPPA_SRS_RESOURCE_TYPE_PERIODICITY_SLOT64,
  NRPPA_SRS_RESOURCE_TYPE_PERIODICITY_SLOT80,
  NRPPA_SRS_RESOURCE_TYPE_PERIODICITY_SLOT160,
  NRPPA_SRS_RESOURCE_TYPE_PERIODICITY_SLOT320,
  NRPPA_SRS_RESOURCE_TYPE_PERIODICITY_SLOT640,
  NRPPA_SRS_RESOURCE_TYPE_PERIODICITY_SLOT1280,
  NRPPA_SRS_RESOURCE_TYPE_PERIODICITY_SLOT2560
} nrppa_srs_resource_type_periodicity_pr;

typedef struct nrppa_resource_type_periodic_s {
  nrppa_srs_resource_type_periodicity_pr periodicity;
  uint16_t offset;
} nrppa_resource_type_periodic_t, nrppa_resource_type_semi_persistent_t;

typedef union nrppa_resource_type_c {
  nrppa_resource_type_periodic_t periodic;
  nrppa_resource_type_semi_persistent_t semi_persistent;
  bool aperiodic;
} nrppa_resource_type_u;

typedef enum nrppa_resource_type_e {
  NRPPA_RESOURCE_TYPE_PR_NOTHING,
  NRPPA_RESOURCE_TYPE_PR_PERIODIC,
  NRPPA_RESOURCE_TYPE_PR_SEMI_PERSISTENT,
  NRPPA_RESOURCE_TYPE_PR_APERIODIC
} nrppa_resource_type_pr;

typedef struct nrppa_resource_type_s {
  nrppa_resource_type_pr present;
  nrppa_resource_type_u choice;
} nrppa_resource_type_t;

typedef struct nrppa_transmission_comb_n8_s {
  uint8_t comb_offset_n8;
  uint8_t cyclic_shift_n8;
} nrppa_transmission_comb_pos_n8_t;

typedef union nrppa_transmission_comb_pos_c {
  nrppa_transmission_comb_pos_n2_t n2;
  nrppa_transmission_comb_pos_n4_t n4;
  nrppa_transmission_comb_pos_n8_t n8;
} nrppa_transmission_comb_pos_u;

typedef enum nrppa_transmission_comb_pos_e {
  NRPPA_TRANSMISSION_COMB_POS_PR_NOTHING,
  NRPPA_TRANSMISSION_COMB_POS_PR_N2,
  NRPPA_TRANSMISSION_COMB_POS_PR_N4,
  NRPPA_TRANSMISSION_COMB_POS_PR_N8
} nrppa_transmission_comb_pos_pr;

typedef struct nrppa_transmission_comb_pos_s {
  nrppa_transmission_comb_pos_pr present;
  nrppa_transmission_comb_pos_u choice;
} nrppa_transmission_comb_pos_t;

typedef enum nrppa_srs_resource_type_pos_periodicity_e {
  NRPPA_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT1 = 0,
  NRPPA_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT2,
  NRPPA_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT4,
  NRPPA_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT5,
  NRPPA_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT8,
  NRPPA_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT10,
  NRPPA_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT16,
  NRPPA_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT20,
  NRPPA_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT32,
  NRPPA_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT40,
  NRPPA_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT64,
  NRPPA_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT80,
  NRPPA_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT160,
  NRPPA_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT320,
  NRPPA_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT640,
  NRPPA_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT1280,
  NRPPA_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT2560,
  NRPPA_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT5120,
  NRPPA_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT10240,
  NRPPA_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT20480,
  NRPPA_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT40960,
  NRPPA_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT81920
} nrppa_srs_resource_type_pos_periodicity_pr;

typedef struct nrppa_resource_type_periodic_pos_s {
  nrppa_srs_resource_type_pos_periodicity_pr periodicity;
  uint16_t offset;
} nrppa_resource_type_periodic_pos_t, nrppa_resource_type_semi_persistent_pos_t;

typedef struct nrppa_resource_type_aperiodic_pos_s {
  uint8_t slot_offset;
} nrppa_resource_type_aperiodic_pos_t;

typedef union nrppa_resource_type_pos_c {
  nrppa_resource_type_periodic_pos_t periodic;
  nrppa_resource_type_semi_persistent_pos_t semi_persistent;
  nrppa_resource_type_aperiodic_pos_t aperiodic;
} nrppa_resource_type_pos_u;

typedef enum nrppa_resource_type_pos_e {
  NRPPA_RESOURCE_TYPE_POS_PR_NOTHING,
  NRPPA_RESOURCE_TYPE_POS_PR_PERIODIC,
  NRPPA_RESOURCE_TYPE_POS_PR_SEMI_PERSISTENT,
  NRPPA_RESOURCE_TYPE_POS_PR_APERIODIC
} nrppa_resource_type_pos_pr;

typedef struct nrppa_resource_type_pos_s {
  nrppa_resource_type_pos_pr present;
  nrppa_resource_type_pos_u choice;
} nrppa_resource_type_pos_t;

typedef struct nrppa_srs_resource_id_list_s {
  long *srs_resource_id;
  uint8_t srs_resource_id_list_length;
} nrppa_srs_resource_id_list_t;

typedef struct nrppa_resource_set_type_aperiodic_s {
  uint8_t srs_resource_trigger;
  long slot_offset;
} nrppa_resource_set_type_aperiodic_t;

typedef union nrppa_resource_set_type_c {
  bool periodic;
  bool semi_persistent;
  nrppa_resource_set_type_aperiodic_t aperiodic;
} nrppa_resource_set_type_u;

typedef enum nrppa_resource_set_type_e {
  NRPPA_RESOURCE_SET_TYPE_PR_NOTHING,
  NRPPA_RESOURCE_SET_TYPE_PR_PERIODIC,
  NRPPA_RESOURCE_SET_TYPE_PR_SEMI_PERSISTENT,
  NRPPA_RESOURCE_SET_TYPE_PR_APERIODIC
} nrppa_resource_set_type_pr;

typedef struct nrppa_resource_set_type_s {
  nrppa_resource_set_type_pr present;
  nrppa_resource_set_type_u choice;
} nrppa_resource_set_type_t;

typedef struct nrppa_pos_srs_resource_id_list_s {
  long *srs_pos_resource_id;
  uint32_t pos_srs_resource_id_list_length;
} nrppa_pos_srs_resource_id_list_t;

typedef union nrppa_pos_resource_set_type_c {
  bool periodic;
  bool semi_persistent;
  uint8_t srs_resource;
} nrppa_pos_resource_set_type_u;

typedef enum nrppa_pos_resource_set_type_e {
  NRPPA_POS_RESOURCE_SET_TYPE_PR_NOTHING,
  NRPPA_POS_RESOURCE_SET_TYPE_PR_PERIODIC,
  NRPPA_POS_RESOURCE_SET_TYPE_PR_SEMI_PERSISTENT,
  NRPPA_POS_RESOURCE_SET_TYPE_PR_APERIODIC
} nrppa_pos_resource_set_type_pr;

typedef struct nrppa_pos_resource_set_type_s {
  nrppa_pos_resource_set_type_pr present;
  nrppa_pos_resource_set_type_u choice;
} nrppa_pos_resource_set_type_t;

typedef enum nrppa_srs_resource_number_of_ports_e {
  NRPPA_SRS_NUMBER_OF_PORTS_N1,
  NRPPA_SRS_NUMBER_OF_PORTS_N2,
  NRPPA_SRS_NUMBER_OF_PORTS_N4
} nrppa_srs_resource_number_of_ports_pr;

typedef enum nrppa_srs_resource_number_of_symbols_e {
  NRPPA_SRS_NUMBER_OF_SYMBOLS_N1,
  NRPPA_SRS_NUMBER_OF_SYMBOLS_N2,
  NRPPA_SRS_NUMBER_OF_SYMBOLS_N4
} nrppa_srs_resource_number_of_symbols_pr;

typedef enum nrppa_srs_repetition_factor_e {
  NRPPA_SRS_REPETITION_FACTOR_RF1,
  NRPPA_SRS_REPETITION_FACTOR_RF2,
  NRPPA_SRS_REPETITION_FACTOR_RF4
} nrppa_srs_repetition_factor_pr;

typedef enum nrppa_srs_group_or_sequencehopping_e {
  NRPPA_GROUPORSEQUENCEHOPPING_NOTHING,
  NRPPA_GROUPORSEQUENCEHOPPING_GROUPHOPPING,
  NRPPA_GROUPORSEQUENCEHOPPING_SEQUENCEHOPPING
} nrppa_srs_group_or_sequencehopping_pr;

typedef struct nrppa_srs_resource_s {
  uint32_t srs_resource_id;
  nrppa_srs_resource_number_of_ports_pr nr_of_srs_ports;
  nrppa_transmission_comb_t transmission_comb;
  uint8_t start_position;
  nrppa_srs_resource_number_of_symbols_pr nr_of_symbols;
  nrppa_srs_repetition_factor_pr repetition_factor;
  uint8_t freq_domain_position;
  uint16_t freq_domain_shift;
  uint8_t c_srs;
  uint8_t b_srs;
  uint8_t b_hop;
  nrppa_srs_group_or_sequencehopping_pr group_or_sequence_hopping;
  nrppa_resource_type_t resource_type;
  uint16_t sequence_id;
} nrppa_srs_resource_t;

typedef enum nrppa_srs_resource_item_number_of_symbols_e {
  NRPPA_SRS_RESOURCE_ITEM_NUMBER_OF_SYMBOLS_N1,
  NRPPA_SRS_RESOURCE_ITEM_NUMBER_OF_SYMBOLS_N2,
  NRPPA_SRS_RESOURCE_ITEM_NUMBER_OF_SYMBOLS_N4,
  NRPPA_SRS_RESOURCE_ITEM_NUMBER_OF_SYMBOLS_N8,
  NRPPA_SRS_RESOURCE_ITEM_NUMBER_OF_SYMBOLS_N12
} nrppa_srs_resource_item_number_of_symbols_pr;

typedef struct nrppa_pos_srs_resource_item_s {
  uint32_t srs_pos_resource_id;
  nrppa_transmission_comb_pos_t transmission_comb_pos;
  uint8_t start_position;
  nrppa_srs_resource_item_number_of_symbols_pr nr_of_symbols;
  uint16_t freq_domain_shift;
  uint8_t c_srs;
  nrppa_srs_group_or_sequencehopping_pr group_or_sequence_hopping;
  nrppa_resource_type_pos_t resource_type_pos;
  uint32_t sequence_id;
} nrppa_pos_srs_resource_item_t;

typedef struct nrppa_srs_resource_set_s {
  uint8_t srs_resource_set_id;
  nrppa_srs_resource_id_list_t srs_resource_id_list;
  nrppa_resource_set_type_t resource_set_type;
} nrppa_srs_resource_set_t;

typedef struct nrppa_pos_srs_resource_set_item_s {
  uint8_t pos_srs_resource_set_id;
  nrppa_pos_srs_resource_id_list_t pos_srs_resource_id_list;
  nrppa_pos_resource_set_type_t pos_resource_set_type;
} nrppa_pos_srs_resource_set_item_t;

typedef struct nrppa_srs_resource_list_s {
  nrppa_srs_resource_t *srs_resource;
  uint32_t srs_resource_list_length;
} nrppa_srs_resource_list_t;

typedef struct nrppa_pos_srs_resource_list_s {
  nrppa_pos_srs_resource_item_t *pos_srs_resource_item;
  uint32_t pos_srs_resource_list_length;
} nrppa_pos_srs_resource_list_t;

typedef struct nrppa_srs_resource_set_list_s {
  nrppa_srs_resource_set_t *srs_resource_set;
  uint32_t srs_resource_set_list_length;
} nrppa_srs_resource_set_list_t;

typedef struct nrppa_pos_srs_resource_set_list_s {
  nrppa_pos_srs_resource_set_item_t *pos_srs_resource_set_item;
  uint32_t pos_srs_resource_set_list_length;
} nrppa_pos_srs_resource_set_list_t;

typedef struct nrppa_srs_config_s {
  nrppa_srs_resource_list_t *srs_resource_list;
  nrppa_pos_srs_resource_list_t *pos_srs_resource_list;
  nrppa_srs_resource_set_list_t *srs_resource_set_list;
  nrppa_pos_srs_resource_set_list_t *pos_srs_resource_set_list;
} nrppa_srs_config_t;

typedef enum nrppa_cp_type_e { NRPPA_CP_TYPE_NORMAL, NRPPA_CP_TYPE_EXTENDED } nrppa_cp_type_pr;

typedef struct nrppa_active_ul_bwp_s {
  uint32_t location_and_bandwidth;
  nrppa_subcarrier_spacing_pr subcarrier_spacing;
  nrppa_cp_type_pr cyclic_prefix;
  uint32_t tx_direct_current_location;
  uint8_t shift_7_dot_5kHz;
  nrppa_srs_config_t srs_config;
} nrppa_active_ul_bwp_t;

typedef struct nrppa_srs_carrier_list_item_s {
  uint32_t pointA;
  nrppa_uplink_channel_bw_per_scs_list_t uplink_channel_bw_per_scs_list;
  nrppa_active_ul_bwp_t active_ul_bwp;
  uint16_t pci;
} nrppa_srs_carrier_list_item_t;

typedef struct nrppa_srs_carrier_list_s {
  nrppa_srs_carrier_list_item_t *srs_carrier_list_item;
  uint32_t srs_carrier_list_length;
} nrppa_srs_carrier_list_t;

typedef struct nrppa_srs_configuration_s {
  nrppa_srs_carrier_list_t srs_carrier_list;
} nrppa_srs_configuration_t;

// IE 9.3.1.176 (TS 38.473 V16.21.0)
typedef struct nrppa_trp_information_list_s {
  nrppa_trp_information_t *trp_information_item;
  uint32_t trp_information_item_length;
} nrppa_trp_information_list_t;

typedef union nrppa_srs_type_c {
  uint8_t *srs_resource_set_id;
  bool *aperiodic;
} nrppa_srs_type_u;

typedef enum nrppa_srs_type_e {
  NRPPA_SRS_TYPE_PR_NOTHING,
  NRPPA_SRS_TYPE_PR_SEMIPERSISTENTSRS,
  NRPPA_SRS_TYPE_PR_APERIODICSRS
} nrppa_srs_type_pr;

typedef struct nrppa_srs_type_s {
  nrppa_srs_type_pr present;
  nrppa_srs_type_u choice;
} nrppa_srs_type_t;

typedef struct nrppa_trp_measurement_request_item_s {
  uint32_t trp_id;
} nrppa_trp_measurement_request_item_t;

typedef struct nrppa_trp_measurement_request_list_s {
  nrppa_trp_measurement_request_item_t *trp_measurement_request_item;
  uint32_t trp_measurement_request_list_length;
} nrppa_trp_measurement_request_list_t;

typedef enum nrppa_pos_report_characteristics_e {
  NRPPA_POSREPORTCHARACTERISTICS_ONDEMAND = 0,
  NRPPA_POSREPORTCHARACTERISTICS_PERIODIC = 1
} nrppa_report_characteristics_pr;

typedef enum nrppa_pos_measurement_periodicity_e {
  NRPPA_POSMEASUREMENTPERIODICITY_MS120 = 0,
  NRPPA_POSMEASUREMENTPERIODICITY_MS240 = 1,
  NRPPA_POSMEASUREMENTPERIODICITY_MS480 = 2,
  NRPPA_POSMEASUREMENTPERIODICITY_MS640 = 3,
  NRPPA_POSMEASUREMENTPERIODICITY_MS1024 = 4,
  NRPPA_POSMEASUREMENTPERIODICITY_MS2048 = 5,
  NRPPA_POSMEASUREMENTPERIODICITY_MS5120 = 6,
  NRPPA_POSMEASUREMENTPERIODICITY_MS10240 = 7,
  NRPPA_POSMEASUREMENTPERIODICITY_MIN1 = 8,
  NRPPA_POSMEASUREMENTPERIODICITY_MIN6 = 9,
  NRPPA_POSMEASUREMENTPERIODICITY_MIN12 = 10,
  NRPPA_POSMEASUREMENTPERIODICITY_MIN30 = 11,
  NRPPA_POSMEASUREMENTPERIODICITY_MIN60 = 12
} nrppa_measurement_periodicity_pr;

typedef enum nrppa_pos_measurement_type_e {
  NRPPA_POSMEASUREMENTTYPE_GNB_RX_TX = 0,
  NRPPA_POSMEASUREMENTTYPE_UL_SRS_RSRP = 1,
  NRPPA_POSMEASUREMENTTYPE_UL_AOA = 2,
  NRPPA_POSMEASUREMENTTYPE_UL_RTOA = 3
} nrppa_measurement_type_pr;

typedef struct nrppa_pos_measurement_quantities_item_s {
  nrppa_measurement_type_pr measurement_type;
} nrppa_measurement_quantities_item_t;

typedef struct nrppa_pos_measurement_quantities_s {
  nrppa_measurement_quantities_item_t *measurement_quantities_item;
  uint32_t measurement_quantities_length;
} nrppa_measurement_quantities_t;

typedef struct nrppa_lcs_to_gcs_translationaoa_c {
  uint16_t alpha;
  uint16_t beta;
  uint16_t gamma;
} nrppa_lcs_to_gcs_translationaoa_t;

typedef struct nrppa_ul_aoa_s {
  uint16_t azimuth_aoa;
  uint16_t *zenith_aoa;
  nrppa_lcs_to_gcs_translationaoa_t *lcs_to_gcs_translation_aoa;
} nrppa_ul_aoa_t;

typedef union nrppa_relative_path_delay_c {
  uint32_t k0;
  uint32_t k1;
  uint32_t k2;
  uint32_t k3;
  uint32_t k4;
  uint32_t k5;
} nrppa_relative_path_delay_u, nrppa_ul_rtoa_measurement_u, nrppa_gnb_rx_tx_time_diff_u;

typedef enum nrppa_gnb_rx_tx_time_diff_meas_e {
  NRPPA_GNBRXTXTIMEDIFFMEAS_PR_NOTHING,
  NRPPA_GNBRXTXTIMEDIFFMEAS_PR_K0,
  NRPPA_GNBRXTXTIMEDIFFMEAS_PR_K1,
  NRPPA_GNBRXTXTIMEDIFFMEAS_PR_K2,
  NRPPA_GNBRXTXTIMEDIFFMEAS_PR_K3,
  NRPPA_GNBRXTXTIMEDIFFMEAS_PR_K4,
  NRPPA_GNBRXTXTIMEDIFFMEAS_PR_K5
} nrppa_gnb_rx_tx_time_diff_pr;

typedef struct nrppa_gnb_rx_tx_time_diff_s {
  nrppa_gnb_rx_tx_time_diff_pr present;
  nrppa_gnb_rx_tx_time_diff_u choice;
} nrppa_gnb_rx_tx_time_diff_t;

typedef enum nrppa_ul_rtoa_measurement_e {
  NRPPA_ULRTOAMEAS_PR_NOTHING,
  NRPPA_ULRTOAMEAS_PR_K0,
  NRPPA_ULRTOAMEAS_PR_K1,
  NRPPA_ULRTOAMEAS_PR_K2,
  NRPPA_ULRTOAMEAS_PR_K3,
  NRPPA_ULRTOAMEAS_PR_K4,
  NRPPA_ULRTOAMEAS_PR_K5
} nrppa_ul_rtoa_measurement_pr;

typedef struct nrppa_ul_rtoa_measurement_s {
  nrppa_ul_rtoa_measurement_pr present;
  nrppa_ul_rtoa_measurement_u choice;
} nrppa_ul_rtoa_measurement_t;

typedef union nrppa_measured_results_value_c {
  nrppa_ul_aoa_t ul_angle_of_arrival;
  uint8_t ul_srs_rsrp;
  nrppa_ul_rtoa_measurement_t ul_rtoa;
  nrppa_gnb_rx_tx_time_diff_t gnb_rx_tx_time_diff;
} nrppa_measured_results_value_u;

typedef enum nrppa_measured_results_value_e {
  NRPPA_MEASURED_RESULTS_VALUE_PR_NOTHING,
  NRPPA_MEASURED_RESULTS_VALUE_PR_UL_ANGLEOFARRIVAL,
  NRPPA_MEASURED_RESULTS_VALUE_PR_UL_SRS_RSRP,
  NRPPA_MEASURED_RESULTS_VALUE_PR_UL_RTOA,
  NRPPA_MEASURED_RESULTS_VALUE_PR_GNB_RXTXTIMEDIFF
} nrppa_measured_results_value_pr;

typedef struct nrppa_measured_results_value_s {
  nrppa_measured_results_value_pr present;
  nrppa_measured_results_value_u choice;
} nrppa_measured_results_value_t;

typedef union nrppa_time_stamp_slot_index_c {
  uint8_t scs_15;
  uint8_t scs_30;
  uint8_t scs_60;
  uint8_t scs_120;
} nrppa_time_stamp_slot_index_u;

typedef enum nrppa_time_stamp_slot_index_e {
  NRPPA_TIME_STAMP_SLOT_INDEX_PR_NOTHING,
  NRPPA_TIME_STAMP_SLOT_INDEX_PR_SCS_15,
  NRPPA_TIME_STAMP_SLOT_INDEX_PR_SCS_30,
  NRPPA_TIME_STAMP_SLOT_INDEX_PR_SCS_60,
  NRPPA_TIME_STAMP_SLOT_INDEX_PR_SCS_120
} nrppa_time_stamp_slot_index_pr;

typedef struct nrppa_time_stamp_slot_index_s {
  nrppa_time_stamp_slot_index_pr present;
  nrppa_time_stamp_slot_index_u choice;
} nrppa_time_stamp_slot_index_t;

typedef struct nrppa_time_stamp_s {
  uint16_t system_frame_number;
  nrppa_time_stamp_slot_index_t slot_index;
} nrppa_time_stamp_t;

typedef struct nrppa_measurement_result_item_s {
  nrppa_measured_results_value_t measured_results_value;
  nrppa_time_stamp_t time_stamp;
} nrppa_measurement_result_item_t;

typedef struct nrppa_measurement_result_s {
  nrppa_measurement_result_item_t *measurement_result_item;
  uint32_t measurement_result_item_length;
} nrppa_measurement_result_t;

typedef struct nrppa_measurement_response_item_s {
  nrppa_measurement_result_t measurement_result;
  uint32_t trp_id;
} nrppa_measurement_response_item_t;

typedef struct nrppa_measurement_response_list_s {
  nrppa_measurement_response_item_t *measurement_response_item;
  uint32_t measurement_response_list_length;
} nrppa_measurement_response_list_t;

typedef enum nrppa_cause_type_e {
  NRPPA_CAUSE_NOTHING, /* No components present */
  NRPPA_CAUSE_RADIO_NETWORK,
  NRPPA_CAUSE_PROTOCOL,
  NRPPA_CAUSE_MISC,
} nrppa_cause_type_t;

typedef struct nrppa_cause_e {
  nrppa_cause_type_t type;
  uint8_t value;
} nrppa_cause_t;

typedef struct nrppa_trp_information_req_s {
  // IE 9.2.4 (mandatory)
  uint16_t transaction_id;
  bool has_trp_list;
  // mandatory
  nrppa_trp_list_t trp_list;
  // mandatory
  nrppa_trp_information_type_list_t trp_information_type_list;
} nrppa_trp_information_req_t;

typedef struct nrppa_trp_information_resp_s {
  // IE 9.2.4 (mandatory)
  uint16_t transaction_id;
  // mandatory
  nrppa_trp_information_list_t trp_information_list;
} nrppa_trp_information_resp_t;

typedef struct nrppa_trp_information_failure_s {
  // IE 9.2.4 (mandatory)
  uint16_t transaction_id;
  // IE 9.2.1 (mandatory)
  nrppa_cause_t cause;
} nrppa_trp_information_failure_t;

typedef struct nrppa_positioning_information_req_s {
  // IE 9.2.4 (mandatory)
  uint16_t transaction_id;
} nrppa_positioning_information_req_t;

typedef struct nrppa_positioning_information_resp_s {
  // IE 9.2.4 (mandatory)
  uint16_t transaction_id;
  // IE 9.2.28 (optional)
  nrppa_srs_configuration_t *srs_configuration;
} nrppa_positioning_information_resp_t;

typedef struct nrppa_positioning_activation_req_s {
  // IE 9.2.4 (mandatory)
  uint16_t transaction_id;
  // (mandatory)
  nrppa_srs_type_t srs_type;
} nrppa_positioning_activation_req_t;

typedef struct nrppa_positioning_activation_resp_s {
  // IE 9.2.4 (mandatory)
  uint16_t transaction_id;
} nrppa_positioning_activation_resp_t;

typedef struct nrppa_measurement_req_s {
  // IE 9.2.4 (mandatory)
  uint16_t transaction_id;
  // (mandatory)
  uint32_t lmf_measurement_id;
  // (mandatory)
  nrppa_trp_measurement_request_list_t trp_measurement_request_list;
  // (mandatory) ondemand = 0, periodic = 1
  nrppa_report_characteristics_pr report_characteristics;
  // if report characteristics periodic
  nrppa_measurement_periodicity_pr measurement_periodicity;
  // (mandatory)
  nrppa_measurement_quantities_t measurement_quantities;
  // IE 9.2.28 (optional)
  nrppa_srs_configuration_t *srs_configuration;
} nrppa_measurement_req_t;

typedef struct nrppa_measurement_resp_s {
  // IE 9.2.4 (mandatory)
  uint16_t transaction_id;
  // (mandatory)
  uint32_t lmf_measurement_id;
  // (mandatory)
  uint32_t ran_measurement_id;
  // (optional)
  nrppa_measurement_response_list_t *measurement_response_list;
} nrppa_measurement_resp_t;

typedef struct nrppa_measurement_failure_s {
  // IE 9.2.4 (mandatory)
  uint16_t transaction_id;
  // (mandatory)
  uint32_t lmf_measurement_id;
  // IE 9.2.1 (mandatory)
  nrppa_cause_t cause;
} nrppa_measurement_failure_t;

#endif // NRPPA_MESSAGES_TYPES_H_
