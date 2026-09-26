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
  int sfn;
  int subframe;
  uint32_t delay;
  int drift;
  uint32_t accel;
  vector_t position;
  vector_t velocity;

  bool use_orbital;  
  double semi_major_axis;  
  double eccentricity;	
  double periapsis;  
  double longitude;  
  double inclination;  
  double mean_anomaly; 
} gnb_sat_position_update_t;

bool nr_update_sib19(const gnb_sat_position_update_t *sat_position);

bool nr_trigger_bwp_switch(uint16_t rnti, int bwp_id);

#endif /*__LAYER2_NR_MAC_CONFIG_H__*/
