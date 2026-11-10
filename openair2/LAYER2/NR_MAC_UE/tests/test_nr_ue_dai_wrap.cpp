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

static void setup_mac_for_harq(NR_UE_MAC_INST_t *mac,
                               NR_UE_UL_BWP_t *current_UL_BWP,
                               NR_PUCCH_ConfigCommon_t *pucch_ConfigCommon,
                               long *pucch_ResourceCommon)
{
  *pucch_ResourceCommon = 0;
  pucch_ConfigCommon->pucch_ResourceCommon = pucch_ResourceCommon;
  current_UL_BWP->pucch_ConfigCommon = pucch_ConfigCommon;
  mac->current_UL_BWP = current_UL_BWP;
  mac->frame_structure.numb_slots_frame = 20;
}

TEST(test_dai_wrap, normal_wrap)
{
  NR_UE_MAC_INST_t mac = {0};
  NR_UE_UL_BWP_t current_UL_BWP = {0};
  NR_PUCCH_ConfigCommon_t pucch_ConfigCommon = {0};
  long pucch_ResourceCommon = 0;
  setup_mac_for_harq(&mac, &current_UL_BWP, &pucch_ConfigCommon, &pucch_ResourceCommon);

  // Normal sequence of 5 DCIs: DAI = 0, 1, 2, 3, 0 scheduled for the same UL slot (0.4)
  set_harq_status(&mac, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0); // DAI 0 -> dai_cumul = 1
  set_harq_status(&mac, 0, 1, 0, 0, 4, 1, 0, 0, 0, 0); // DAI 1 -> dai_cumul = 2
  set_harq_status(&mac, 0, 2, 0, 0, 4, 2, 0, 0, 0, 0); // DAI 2 -> dai_cumul = 3
  set_harq_status(&mac, 0, 3, 0, 0, 4, 3, 0, 0, 0, 0); // DAI 3 -> dai_cumul = 4
  set_harq_status(&mac, 0, 4, 0, 0, 4, 0, 0, 0, 0, 0); // DAI 0 -> dai_cumul = 5 (wrapped)

  EXPECT_EQ(mac.dl_harq_info[0][0].dai_cumul, 1);
  EXPECT_EQ(mac.dl_harq_info[1][0].dai_cumul, 2);
  EXPECT_EQ(mac.dl_harq_info[2][0].dai_cumul, 3);
  EXPECT_EQ(mac.dl_harq_info[3][0].dai_cumul, 4);
  EXPECT_EQ(mac.dl_harq_info[4][0].dai_cumul, 5);

  PUCCH_sched_t pucch = {0};
  for (int i = 0; i < 5; i++) {
    mac.dl_harq_info[i][0].ack_received = true;
    mac.dl_harq_info[i][0].ack = 1;
  }
  EXPECT_TRUE(get_downlink_ack(&mac, 0, 4, &pucch));
}

TEST(test_dai_wrap, missed_dai_3)
{
  NR_UE_MAC_INST_t mac = {0};
  NR_UE_UL_BWP_t current_UL_BWP = {0};
  NR_PUCCH_ConfigCommon_t pucch_ConfigCommon = {0};
  long pucch_ResourceCommon = 0;
  setup_mac_for_harq(&mac, &current_UL_BWP, &pucch_ConfigCommon, &pucch_ResourceCommon);

  // Sequence where DAI 3 was missed: DAI = 0, 1, 2, [3 missed], 0
  set_harq_status(&mac, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0); // DAI 0 -> dai_cumul = 1
  set_harq_status(&mac, 0, 1, 0, 0, 4, 1, 0, 0, 0, 0); // DAI 1 -> dai_cumul = 2
  set_harq_status(&mac, 0, 2, 0, 0, 4, 2, 0, 0, 0, 0); // DAI 2 -> dai_cumul = 3
  // DCI with DAI 3 was missed by the UE
  set_harq_status(&mac, 0, 4, 0, 0, 4, 0, 0, 0, 0, 0); // DAI 0 -> dai_cumul = 5 (wrapped)

  EXPECT_EQ(mac.dl_harq_info[0][0].dai_cumul, 1);
  EXPECT_EQ(mac.dl_harq_info[1][0].dai_cumul, 2);
  EXPECT_EQ(mac.dl_harq_info[2][0].dai_cumul, 3);
  EXPECT_EQ(mac.dl_harq_info[4][0].dai_cumul, 5);

  PUCCH_sched_t pucch = {0};
  mac.dl_harq_info[0][0].ack_received = true;
  mac.dl_harq_info[0][0].ack = 1;
  mac.dl_harq_info[1][0].ack_received = true;
  mac.dl_harq_info[1][0].ack = 1;
  mac.dl_harq_info[2][0].ack_received = true;
  mac.dl_harq_info[2][0].ack = 1;
  mac.dl_harq_info[4][0].ack_received = true;
  mac.dl_harq_info[4][0].ack = 1;

  // get_downlink_ack() detects the gap at dai_index 3, fills NACK, and succeeds without PUCCH error
  EXPECT_TRUE(get_downlink_ack(&mac, 0, 4, &pucch));
}

TEST(test_dai_wrap, repeated_dai)
{
  NR_UE_MAC_INST_t mac = {0};
  NR_UE_UL_BWP_t current_UL_BWP = {0};
  NR_PUCCH_ConfigCommon_t pucch_ConfigCommon = {0};
  long pucch_ResourceCommon = 0;
  setup_mac_for_harq(&mac, &current_UL_BWP, &pucch_ConfigCommon, &pucch_ResourceCommon);

  // Sequence with repeated DAI due to full modulo-4 cycle miss:
  // DAI = 0, 1, 2, [3, 0, 1 missed], 2
  set_harq_status(&mac, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0); // DAI 0 -> dai_cumul = 1
  set_harq_status(&mac, 0, 1, 0, 0, 4, 1, 0, 0, 0, 0); // DAI 1 -> dai_cumul = 2
  set_harq_status(&mac, 0, 2, 0, 0, 4, 2, 0, 0, 0, 0); // DAI 2 -> dai_cumul = 3
  // DCI with DAI 2 received again after wrap
  set_harq_status(&mac, 0, 4, 0, 0, 4, 2, 0, 0, 0, 0); // DAI 2 -> dai_cumul = 7 (wrapped)

  EXPECT_EQ(mac.dl_harq_info[0][0].dai_cumul, 1);
  EXPECT_EQ(mac.dl_harq_info[1][0].dai_cumul, 2);
  EXPECT_EQ(mac.dl_harq_info[2][0].dai_cumul, 3);
  EXPECT_EQ(mac.dl_harq_info[4][0].dai_cumul, 7);

  PUCCH_sched_t pucch = {0};
  mac.dl_harq_info[0][0].ack_received = true;
  mac.dl_harq_info[0][0].ack = 1;
  mac.dl_harq_info[1][0].ack_received = true;
  mac.dl_harq_info[1][0].ack = 1;
  mac.dl_harq_info[2][0].ack_received = true;
  mac.dl_harq_info[2][0].ack = 1;
  mac.dl_harq_info[4][0].ack_received = true;
  mac.dl_harq_info[4][0].ack = 1;

  EXPECT_TRUE(get_downlink_ack(&mac, 0, 4, &pucch));
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
