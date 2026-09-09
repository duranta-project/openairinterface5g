/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_UE_PRS_CONFIG_H
#define NR_UE_PRS_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

/* The current PHY implementation allocates one PRS context per target. */
#define NR_MAX_PRS_TARGETS 12
#define NR_MAX_PRS_RESOURCES_PER_TARGET 64
#define NR_MAX_PRS_MUTING_PATTERN_LENGTH 32

typedef enum {
  NR_PRS_SOURCE_CONFIG_FILE,
  NR_PRS_SOURCE_POS_SIB,
  NR_PRS_SOURCE_SUPL,
} nr_prs_config_source_t;

/*
 * This is deliberately ASN.1-free.  It is the configuration contract between
 * LPP transports and the UE PHY; future transports (for example SUPL) use the
 * same object.
 */
typedef struct {
  uint16_t resource_set_period;
  uint16_t resource_set_offset;
  uint16_t resource_offset;
  uint8_t resource_repetition;
  uint8_t resource_time_gap;
  uint16_t num_rbs;
  uint8_t num_symbols;
  uint8_t symbol_start;
  uint16_t rb_offset;
  uint8_t comb_size;
  uint8_t re_offset;
  uint32_t muting_pattern1[NR_MAX_PRS_MUTING_PATTERN_LENGTH];
  uint8_t muting_pattern1_length;
  uint32_t muting_pattern2[NR_MAX_PRS_MUTING_PATTERN_LENGTH];
  uint8_t muting_pattern2_length;
  uint8_t muting_bit_repetition;
  uint16_t nprs_id;
  uint8_t resource_id;
} nr_ue_prs_resource_config_t;

typedef struct {
  uint16_t dl_prs_id;
  uint8_t resource_set_id;
  uint16_t phys_cell_id;
  uint32_t point_a_arfcn;
  uint32_t trp_arfcn;
  uint8_t subcarrier_spacing;
  bool normal_cyclic_prefix;
  uint8_t num_resources;
  nr_ue_prs_resource_config_t resources[NR_MAX_PRS_RESOURCES_PER_TARGET];
} nr_ue_prs_target_config_t;

typedef struct {
  nr_prs_config_source_t source;
  uint32_t generation;
  uint8_t num_targets;
  nr_ue_prs_target_config_t targets[NR_MAX_PRS_TARGETS];
} nr_ue_prs_configuration_t;

#endif /* NR_UE_PRS_CONFIG_H */
