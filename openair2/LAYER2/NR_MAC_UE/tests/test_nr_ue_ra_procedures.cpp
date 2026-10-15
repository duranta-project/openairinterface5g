/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "gtest/gtest.h"
extern "C" {
#include "common/platform_types.h"
#include "openair2/LAYER2/NR_MAC_UE/mac_proto.h"
#include "executables/softmodem-common.h"
#include "openair2/LAYER2/nr_rlc/nr_rlc_oai_api.h"
#include "common/utils/ocp_itti/intertask_interface.h"

static softmodem_params_t softmodem_params;
softmodem_params_t *get_softmodem_params(void)
{
  return &softmodem_params;
}
void nr_mac_rrc_ra_ind(const module_id_t mod_id, bool success)
{
  UNUSED(mod_id);
  UNUSED(success);
}
void nr_mac_rrc_msg3_ind(const module_id_t mod_id, const int rnti, bool prepare_payload)
{
  UNUSED(mod_id);
  UNUSED(rnti);
  UNUSED(prepare_payload);
}
void nr_mac_rlc_status_ind(uint16_t ue_id, frame_t frame, int n_ch, const logical_chan_id_t *ch, mac_rlc_status_resp_t *ret)
{
  UNUSED(ue_id);
  UNUSED(frame);
  UNUSED(n_ch);
  UNUSED(ch);
  UNUSED(ret);
}
void nr_mac_rrc_inactivity_timer_ind(const module_id_t mod_id)
{
  UNUSED(mod_id);
}
tbs_size_t nr_mac_rlc_data_req(const module_id_t module_idP,
                               const uint16_t ue_id,
                               const bool gnb_flagP,
                               const logical_chan_id_t channel_idP,
                               const tb_size_t tb_sizeP,
                               char *buffer_pP)
{
  UNUSED(module_idP);
  UNUSED(ue_id);
  UNUSED(gnb_flagP);
  UNUSED(channel_idP);
  UNUSED(tb_sizeP);
  UNUSED(buffer_pP);
  return 0;
}
void nr_mac_rlc_data_ind(const module_id_t module_idP,
                         const uint16_t ue_id,
                         const bool gnb_flagP,
                         const nr_rlc_data_ind_t *data,
                         int num_data)
{
  UNUSED(module_idP);
  UNUSED(ue_id);
  UNUSED(gnb_flagP);
  UNUSED(data);
  UNUSED(num_data);
}
void nr_mac_rrc_verification_failed(const module_id_t mod_id)
{
  UNUSED(mod_id);
}
bool nr_rlc_activate_srb0(int ue_id,
                          void *data,
                          void (*send_initial_ul_rrc_message)(int ue_id, const uint8_t *sdu, sdu_size_t sdu_len, void *data))
{
  UNUSED(ue_id);
  UNUSED(data);
  UNUSED(send_initial_ul_rrc_message);
  return true;
}
int nr_rlc_module_init(nr_rlc_op_mode_t mode)
{
  UNUSED(mode);
  return 0;
}
MessageDef *itti_alloc_new_message(task_id_t origin_task_id, instance_t originInstance, MessagesIds message_id)
{
  UNUSED(origin_task_id);
  UNUSED(originInstance);
  UNUSED(message_id);
  return NULL;
}
int itti_send_msg_to_task(task_id_t task_id, instance_t instance, MessageDef *message)
{
  UNUSED(task_id);
  UNUSED(instance);
  UNUSED(message);
  return 0;
}
typedef uint32_t channel_t;
int8_t nr_mac_rrc_data_ind_ue(const module_id_t module_id,
                              const int CC_id,
                              const uint8_t gNB_index,
                              const int hfn,
                              const frame_t frame,
                              const int slot,
                              const rnti_t rnti,
                              const uint32_t cellid,
                              const long arfcn,
                              const channel_t channel,
                              const uint8_t *pduP,
                              const sdu_size_t pdu_len)
{
  UNUSED(module_id);
  UNUSED(CC_id);
  UNUSED(gNB_index);
  UNUSED(hfn);
  UNUSED(frame);
  UNUSED(slot);
  UNUSED(rnti);
  UNUSED(cellid);
  UNUSED(arfcn);
  UNUSED(channel);
  UNUSED(pduP);
  UNUSED(pdu_len);
  return 0;
}
bool check_csi_report_consistency(const NR_CSI_MeasConfig_t *meas)
{
  UNUSED(meas);
  return true;
}
void nr_mac_rrc_meas_ind_ue(module_id_t module_id,
                            uint32_t gNB_index,
                            uint16_t Nid_cell,
                            bool csi_meas,
                            bool is_neighboring_cell,
                            int rsrp_dBm)
{
  UNUSED(module_id);
  UNUSED(gNB_index);
  UNUSED(Nid_cell);
  UNUSED(csi_meas);
  UNUSED(is_neighboring_cell);
  UNUSED(rsrp_dBm);
}
}
#include <cstdio>
#include "common/utils/LOG/log.h"

TEST(test_init_ra, four_step_cbra)
{
  NR_UE_MAC_INST_t mac = {0};
  RA_config_t *ra = &mac.ra;
  NR_RACH_ConfigCommon_t nr_rach_ConfigCommon = {0};
  NR_RACH_ConfigGeneric_t rach_ConfigGeneric = {0};
  NR_RACH_ConfigDedicated_t rach_ConfigDedicated = {0};
  NR_UE_UL_BWP_t current_bwp;
  NR_UE_DL_BWP_t dl_bwp;
  mac.current_UL_BWP = &current_bwp;
  mac.current_DL_BWP = &dl_bwp;
  mac.mib_ssb = 0;
  long scs = 1;
  current_bwp.scs = scs;
  current_bwp.bwp_id = 0;
  dl_bwp.bwp_id = 0;
  current_bwp.channel_bandwidth = 40;
  nr_rach_ConfigCommon.msg1_SubcarrierSpacing = &scs;
  nr_rach_ConfigCommon.rach_ConfigGeneric = rach_ConfigGeneric;
  current_bwp.rach_ConfigCommon = &nr_rach_ConfigCommon;
  ra->rach_ConfigDedicated = &rach_ConfigDedicated;
  mac.p_Max = 23;
  mac.nr_band = 78;
  mac.frame_structure.frame_type = TDD;
  mac.frame_structure.numb_slots_frame = 20;
  mac.frequency_range = FR1;

  init_RA(&mac);

  EXPECT_EQ(mac.ra.ra_type, RA_4_STEP);
  EXPECT_EQ(mac.state, UE_PERFORMING_RA);
  EXPECT_EQ(mac.ra.RA_active, true);
  EXPECT_EQ(mac.ra.cfra, 0);
}

TEST(test_init_ra, four_step_cfra)
{
  NR_UE_MAC_INST_t mac = {0};
  RA_config_t *ra = &mac.ra;
  NR_RACH_ConfigCommon_t nr_rach_ConfigCommon = {0};
  NR_RACH_ConfigGeneric_t rach_ConfigGeneric = {0};
  NR_UE_UL_BWP_t current_bwp;
  NR_UE_DL_BWP_t dl_bwp;
  mac.current_UL_BWP = &current_bwp;
  mac.current_DL_BWP = &dl_bwp;
  mac.mib_ssb = 0;
  long scs = 1;
  current_bwp.scs = scs;
  current_bwp.bwp_id = 0;
  dl_bwp.bwp_id = 0;
  current_bwp.channel_bandwidth = 40;
  nr_rach_ConfigCommon.msg1_SubcarrierSpacing = &scs;
  nr_rach_ConfigCommon.rach_ConfigGeneric = rach_ConfigGeneric;
  current_bwp.rach_ConfigCommon = &nr_rach_ConfigCommon;
  mac.p_Max = 23;
  mac.nr_band = 78;
  mac.frame_structure.frame_type = TDD;
  mac.frame_structure.numb_slots_frame = 20;
  mac.frequency_range = FR1;

  NR_RACH_ConfigDedicated_t rach_ConfigDedicated = {0};
  NR_CFRA_t cfra;
  rach_ConfigDedicated.cfra = &cfra;
  ra->rach_ConfigDedicated = &rach_ConfigDedicated;

  init_RA(&mac);

  EXPECT_EQ(mac.ra.ra_type, RA_4_STEP);
  EXPECT_EQ(mac.state, UE_PERFORMING_RA);
  EXPECT_EQ(mac.ra.RA_active, true);
  EXPECT_EQ(mac.ra.cfra, 1);
}


// Threshold IE value -> dBm: value - 156. IE 90 -> -66 dBm.  
  
TEST(test_ssb_rsrp_threshold, absent_threshold_accepts_everything)  
{  
  NR_UE_MAC_INST_t mac = {0};  
  EXPECT_TRUE(ssb_rsrp_above_threshold(&mac, 0, NULL));  
}  
  
TEST(test_ssb_rsrp_threshold, infinity_threshold_rejects_everything)  
{  
  NR_UE_MAC_INST_t mac = {0};  
  mac.ssb_measurements[0].ssb_rsrp_dBm = -50; // even a great SSB fails  
  long thr = 127; // +infinity  
  EXPECT_FALSE(ssb_rsrp_above_threshold(&mac, 0, &thr));  
}  
  
TEST(test_ssb_rsrp_threshold, above_and_below)  
{  
  NR_UE_MAC_INST_t mac = {0};  
  long thr = 90; // -66 dBm  
  mac.ssb_measurements[0].ssb_rsrp_dBm = -60;  
  mac.ssb_measurements[1].ssb_rsrp_dBm = -70;  
  EXPECT_TRUE(ssb_rsrp_above_threshold(&mac, 0, &thr));  
  EXPECT_FALSE(ssb_rsrp_above_threshold(&mac, 1, &thr));  
}  
  
TEST(test_ssb_rsrp_threshold, unmeasured_ssb_fails)  
{  
  NR_UE_MAC_INST_t mac = {0};  
  long thr = 1; // -155 dBm — almost anything real should pass  
  mac.ssb_measurements[3].ssb_rsrp_dBm = -200; // L1 sentinel for unmeasured  
  EXPECT_FALSE(ssb_rsrp_above_threshold(&mac, 3, &thr));  
}  
  
TEST(test_cfra_ssb_selection, picks_only_above_threshold)  
{  
  NR_UE_MAC_INST_t mac = {0};  
  long thr = 90; // -66 dBm  
  mac.ssb_measurements[0].ssb_rsrp_dBm = -80; // below  
  mac.ssb_measurements[1].ssb_rsrp_dBm = -60; // above  
  mac.ssb_measurements[2].ssb_rsrp_dBm = -90; // below  
  mac.ssb_list.nb_tx_ssb = 3;

  NR_CFRA_SSB_Resource_t r0 = {0}, r1 = {0}, r2 = {0};  
  r0.ssb = 0; r1.ssb = 1; r2.ssb = 2;  
  NR_CFRA_SSB_Resource_t *list[3] = {&r0, &r1, &r2};  
  
  // run many trials: every selection must land on SSB 1  
  for (int i = 0; i < 200; i++) {  
    int sel = select_cfra_ssb_index(&mac, list, 3, &thr);  
    EXPECT_EQ(sel, 1);  
  }  
}  
  
TEST(test_cfra_ssb_selection, fallback_to_any_when_none_above)  
{  
  NR_UE_MAC_INST_t mac = {0};  
  long thr = 126; // -30 dBm — nothing reaches it  
  mac.ssb_measurements[0].ssb_rsrp_dBm = -80;  
  mac.ssb_measurements[1].ssb_rsrp_dBm = -70;  
  
  NR_CFRA_SSB_Resource_t r0 = {0}, r1 = {0};  
  r0.ssb = 0; r1.ssb = 1;  
  NR_CFRA_SSB_Resource_t *list[2] = {&r0, &r1};  
  
  // must always return a valid index (spec: "select any SSB")  
  for (int i = 0; i < 100; i++) {  
    int sel = select_cfra_ssb_index(&mac, list, 2, &thr);
    EXPECT_GE(sel, 0);  
    EXPECT_LT(sel, 2);  
  }  
}  

TEST(test_cbra_ssb_selection, transmitted_filter)
{
  NR_UE_MAC_INST_t mac = {0};
  long thr = 90;
  mac.ssb_list.nb_tx_ssb = 2;
  // nb_ssb_per_index maps SSB idx -> progressive idx; only 0 and 1 transmitted
  memset(mac.ssb_list.nb_ssb_per_index, 0xff, sizeof(mac.ssb_list.nb_ssb_per_index));
  mac.ssb_list.nb_ssb_per_index[0] = 0;
  mac.ssb_list.nb_ssb_per_index[1] = 1;
  mac.ssb_measurements[0].ssb_rsrp_dBm = -80; // transmitted, above
  mac.ssb_measurements[1].ssb_rsrp_dBm = -60; // transmitted, below
  mac.mib_ssb = 1;

  EXPECT_EQ(select_cbra_ssb(&mac, &thr), 1);
}
  
TEST(test_cbra_ssb_selection, fallback_to_mib_ssb_when_none_above)  
{  
  NR_UE_MAC_INST_t mac = {0};  
  long thr = 126;  
  mac.ssb_list.nb_tx_ssb = 2;  
  memset(mac.ssb_list.nb_ssb_per_index, 0xff, sizeof(mac.ssb_list.nb_ssb_per_index));  
  mac.ssb_list.nb_ssb_per_index[0] = 0;    
  mac.ssb_list.nb_ssb_per_index[1] = 1;  
  mac.ssb_measurements[0].ssb_rsrp_dBm = -100;  
  mac.ssb_measurements[1].ssb_rsrp_dBm = -120;  
  mac.mib_ssb = 2;  
  EXPECT_EQ(select_cbra_ssb(&mac, &thr), 2);  
}

/*
 *  RA RO Mask indication
 */
TEST(test_ro_mask, restricts_to_permitted_positions)  
{  
  // ssb_per_ro < 1 -> num_ros_per_ssb = 1/ssb_per_ro; use 4 ROs per SSB  
  NR_UE_MAC_INST_t mac = {0};  
  RA_config_t *ra = &mac.ra;  
  memset(ra, 0, sizeof(*ra));  
  ra->ssb_ro_config.ssb_per_ro = 0.25;	
  ra->ra_ssb = 0;  
  ra->ro_mask_index = 1;

  // --- Decide Association Periods ---
  // 1 SSB requires 1/0.25 = 4 ROs to map completely. 
  // Our configuration period has 4 ROs (num_ra_occasions_period = 4).
  // Thus, exactly 1 configuration period is needed to complete the mapping.
  ra->association_periods = 1; 
  
  prach_occasion_info_t ros[4] = {0};  
  for (int k = 0; k < 4; k++) { 
	ros[k].slot = k; 
	ros[k].fdm = 0; 
  } 
  
  // With mask 1, only k=0 is permitted -> temp_idx=0 -> ro_index 0  
  for (int i = 0; i < 100; i++) {  
	select_prach_occasion(ra, /*nb_tx_ssb*/1, 4, ros, 4);  
	EXPECT_EQ(ra->sched_ro_info.slot, 0);  
  }  
}

  
TEST(test_ro_mask, mask_selects_correct_subset)  
{  
  NR_UE_MAC_INST_t mac = {0};  
  RA_config_t *ra = &mac.ra;  
  memset(ra, 0, sizeof(*ra));  
  ra->ssb_ro_config.ssb_per_ro = 0.125; // 8 ROs per SSB  
  ra->ra_ssb = 0;  
  ra->ro_mask_index = 3; // positions k % 8 == 2  
  ra->association_periods = 1; 
  prach_occasion_info_t ros[8] = {0};  
  for (int k = 0; k < 8; k++) ros[k].slot = k;	
  
  for (int i = 0; i < 100; i++) {  
	select_prach_occasion(ra, 1, 8, ros, 8);
	printf("sched ro info slot %d\n", ra->sched_ro_info.slot);
	EXPECT_EQ(ra->sched_ro_info.slot, 2); // only k=2 permitted  
  }  
}  

TEST(test_ro_mask, mask3_forces_position2)  
{  
  NR_UE_MAC_INST_t mac = {0};  
  RA_config_t *ra = &mac.ra;  
  memset(ra, 0, sizeof(*ra));  
  ra->ssb_ro_config.ssb_per_ro = 0.125;  
  ra->ra_ssb = 0;  
  ra->ro_mask_index = 3;   // permits only local RO index 3 (1-based) = k=2  
  ra->association_periods = 1;  
  prach_occasion_info_t ros[8] = {0};  
  for (int k = 0; k < 8; k++) ros[k].slot = k;  
  
  for (int i = 0; i < 20; i++) {  
    select_prach_occasion(ra, 1, 8, ros, 8);  
    EXPECT_EQ(ra->sched_ro_info.slot, 2) << "mask 3 selected non-permitted position";  
  }  
}

TEST(test_ro_mask, no_mask_all_positions_reachable)  
{  
  NR_UE_MAC_INST_t mac = {0};  
  RA_config_t *ra = &mac.ra;  
  memset(ra, 0, sizeof(*ra));  
  ra->ssb_ro_config.ssb_per_ro = 0.125;  
  ra->ra_ssb = 0;  
  ra->ro_mask_index = -1;  
  ra->association_periods = 1; 
  prach_occasion_info_t ros[8] = {0};  
  for (int k = 0; k < 8; k++) ros[k].slot = k;	
  
  bool seen[8] = {0};  
  for (int i = 0; i < 200; i++) {	
	select_prach_occasion(ra, 1, 8, ros, 8);	
	printf("In NO mask all pos reachable --- sched ro info slot %d\n", ra->sched_ro_info.slot);
	seen[ra->sched_ro_info.slot] = true;	
	printf("seen[ra->sched_ro_info.slot]: %d\n", seen[ra->sched_ro_info.slot]);
  }  
  for (int k = 0; k < 8; k++)  
	EXPECT_TRUE(seen[k]) << "position " << k << " never selected";	
}

TEST(test_ro_mask_resolution, dedicated_mask_wins)	
{  
  NR_UE_MAC_INST_t mac = {0};  
  mac.ra.ra_type = RA_4_STEP;  
  mac.ra.ro_mask_index = 5; // set by CFRA/PDCCH order	
  NR_UE_UL_BWP_t bwp = {0};  
  NR_RACH_ConfigCommon_t cfg = {0};  
  bwp.rach_ConfigCommon = &cfg;  
  mac.current_UL_BWP = &bwp;  
  EXPECT_EQ(resolve_ro_mask(&mac), 5);  
}  
  
TEST(test_ro_mask_resolution, none_configured_returns_minus_one)  
{  
  NR_UE_MAC_INST_t mac = {0};  
  mac.ra.ra_type = RA_4_STEP;  
  mac.ra.ro_mask_index = -1;  
  NR_UE_UL_BWP_t bwp = {0};  
  NR_RACH_ConfigCommon_t cfg = {0}; // no ext2	
  bwp.rach_ConfigCommon = &cfg;  
  mac.current_UL_BWP = &bwp;  
  EXPECT_EQ(resolve_ro_mask(&mac), -1);  
}

TEST(test_ro_mask_resolution, common_ext2_feature_combination)
{
  NR_UE_MAC_INST_t mac = {0};
  mac.ra.ra_type = RA_4_STEP;
  mac.ra.ro_mask_index = -1;

  NR_UE_UL_BWP_t bwp = {0};

  NR_RACH_ConfigCommon_t *cfg =
      static_cast<NR_RACH_ConfigCommon_t *>(
          calloc(1, sizeof(*cfg)));

  cfg->ext2 =
      static_cast<decltype(cfg->ext2)>(
          calloc(1, sizeof(*cfg->ext2)));

  cfg->ext2->featureCombinationPreamblesList_r17 =
      static_cast<decltype(
          cfg->ext2->featureCombinationPreamblesList_r17)>(
          calloc(1, sizeof(*cfg->ext2->featureCombinationPreamblesList_r17)));

  NR_FeatureCombinationPreambles_r17_t *fp =
      static_cast<NR_FeatureCombinationPreambles_r17_t *>(
          calloc(1, sizeof(*fp)));

  long *mask =
      static_cast<long *>(calloc(1, sizeof(*mask)));

  *mask = 4;
  fp->ssb_SharedRO_MaskIndex_r17 = mask;

  ASN_SEQUENCE_ADD(
      &cfg->ext2->featureCombinationPreamblesList_r17->list,
      fp);

  bwp.rach_ConfigCommon = cfg;
  mac.current_UL_BWP = &bwp;

  EXPECT_EQ(resolve_ro_mask(&mac), 4);

  ASN_STRUCT_FREE(
      asn_DEF_NR_RACH_ConfigCommon,
      cfg);
}



int main(int argc, char **argv)
{
  logInit();
  configmodule_interface_t *uniqCfg = load_configmodule(argc, argv, CONFIG_ENABLECMDLINEONLY);
  g_log->log_component[MAC].level = OAILOG_DEBUG;
  g_log->log_component[NR_MAC].level = OAILOG_DEBUG;
  testing::InitGoogleTest(&argc, argv);
  int ret = RUN_ALL_TESTS();
  end_configmodule(uniqCfg);
  return ret;
}
