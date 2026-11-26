/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef RAN_FUNC_SM_KPM_SUBSCRIPTION_AGENT_H
#define RAN_FUNC_SM_KPM_SUBSCRIPTION_AGENT_H

#include "openair2/E2AP/flexric/src/sm/kpm_sm/kpm_data_ie_wrapper.h"

#include "openair2/LAYER2/NR_MAC_gNB/mac_proto.h"
#include "openair2/LAYER2/nr_pdcp/nr_pdcp_oai_api.h"
#include "openair2/LAYER2/nr_rlc/nr_rlc_oai_api.h"

typedef struct {
  uint32_t rrc_ue_id;
  NR_UE_info_t* ue;

} cudu_ue_info_pair_t;

typedef struct {
  dlul_mac_stats_t mac_stats;

  uint64_t rrc_conn_count_sum;
  uint64_t rrc_conn_count_samples;
} e2_node_level_stats_t;

e2_node_level_stats_t cp_node_level_stats(const e2_node_level_stats_t *src);

typedef meas_record_lst_t (*kpm_meas_fp)(const label_info_lst_t label, uint32_t gran_period_ms, cudu_ue_info_pair_t ue_info, e2_node_level_stats_t* node_stats);

typedef struct{ 
  char* key; 
  kpm_meas_fp value;
} kv_measure_t;


void init_kpm_subs_data(void);

meas_record_lst_t get_kpm_meas_value(char* kpm_meas_name, const label_info_lst_t label, uint32_t gran_period_ms, cudu_ue_info_pair_t ue_info, e2_node_level_stats_t* node_stats);

#endif
