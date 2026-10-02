/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef __LAYER2_NR_MAC_CONFIG_H__
#define __LAYER2_NR_MAC_CONFIG_H__

#include <stdint.h>

typedef struct vector_s {
  int X;
  int Y;
  int Z;
} vector_t;

// Format similar to values sent in SIB19
typedef struct gnb_sat_position_update_s {
  /// epochTime-r17 of this update, used if epoch_lead_ms is 0
  int sfn;
  int subframe;
  /// if > 0, this update describes the satellite epoch_lead_ms after the current frame and
  /// subframe of the gNB, and epochTime-r17 is set to that instant. For sources that do not
  /// know the SFN of the gNB, e.g. an external channel emulator.
  int epoch_lead_ms;
  uint32_t delay;
  int drift;
  uint32_t accel;
  vector_t position;
  vector_t velocity;
} gnb_sat_position_update_t;

bool nr_update_sib19(const gnb_sat_position_update_t *sat_position);

bool nr_trigger_bwp_switch(uint16_t rnti, int bwp_id);

#endif /*__LAYER2_NR_MAC_CONFIG_H__*/
