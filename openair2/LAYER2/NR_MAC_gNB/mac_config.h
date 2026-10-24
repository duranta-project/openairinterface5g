/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef __LAYER2_NR_MAC_CONFIG_H__
#define __LAYER2_NR_MAC_CONFIG_H__

#include <stdint.h>
#include <stddef.h>
#include "ntn_assistance.h"

typedef struct vector_s {
  int X;
  int Y;
  int Z;
} vector_t;

// Format similar to values sent in SIB19
typedef struct gnb_sat_position_update_s {
  int sfn;
  int subframe;
  uint32_t delay;
  int drift;
  uint32_t accel;
  vector_t position;
  vector_t velocity;
} gnb_sat_position_update_t;

bool nr_update_sib19(const gnb_sat_position_update_t *sat_position);

struct NR_NTN_Config_r17;
/* Control-thread preparation using the same NTN fields and SI encoder as the
 * legacy update. The caller owns the resulting NTN config on success. */
const char *nr_prepare_sib19(const struct NR_NTN_Config_r17 *ntn_template,
                             const ntn_assistance_state_t *state,
                             uint8_t *buffer,
                             size_t capacity,
                             struct NR_NTN_Config_r17 **config,
                             int *length);

bool nr_trigger_bwp_switch(uint16_t rnti, int bwp_id);

#endif /*__LAYER2_NR_MAC_CONFIG_H__*/
