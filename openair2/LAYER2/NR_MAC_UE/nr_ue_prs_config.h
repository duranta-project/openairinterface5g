/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_UE_PRS_CONFIG_H
#define NR_UE_PRS_CONFIG_H

#include <stdint.h>

#include "fapi_nr_ue_interface.h"

#define NR_UE_MAX_PRS_GNBS 12
#define NR_UE_MAX_PRS_RESOURCES 64
#define NR_MAX_PRS_MUTING_PATTERN_LENGTH 32

typedef enum {
  NR_PRS_SOURCE_CONFIG_FILE,
  NR_PRS_SOURCE_POS_SIB,
  NR_PRS_SOURCE_SUPL,
} nr_prs_config_source_t;

typedef struct {
  fapi_nr_dl_config_prs_pdu_rel16_t pdu;
  uint16_t slot_offset;
} nr_ue_prs_resource_t;

/* ASN.1-free configuration passed from LPP/RRC to the MAC PRS scheduler. */
typedef struct {
  uint16_t dl_prs_id;
  uint8_t resource_set_id;
  uint16_t phys_cell_id;
  uint32_t point_a_arfcn;
  uint32_t trp_arfcn;
  uint16_t period;
  uint16_t offset;
  uint8_t repetition;
  uint8_t time_gap;
  uint32_t muting_pattern1[NR_MAX_PRS_MUTING_PATTERN_LENGTH];
  uint8_t muting_pattern1_length;
  uint32_t muting_pattern2[NR_MAX_PRS_MUTING_PATTERN_LENGTH];
  uint8_t muting_pattern2_length;
  uint8_t muting_bit_repetition;
  uint8_t num_resources;
  nr_ue_prs_resource_t resources[NR_UE_MAX_PRS_RESOURCES];
} nr_ue_prs_target_config_t;

typedef struct {
  nr_prs_config_source_t source;
  uint32_t generation;
  uint8_t num_targets;
  nr_ue_prs_target_config_t targets[NR_UE_MAX_PRS_GNBS];
} nr_ue_prs_configuration_t;

#endif /* NR_UE_PRS_CONFIG_H */
