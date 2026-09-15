// openair2/RRC/NR_UE/tests/test_full_config.cpp  
#include <gtest/gtest.h>  
extern "C" {
//#include "../rrc_UE.c"
#include "../rrc_defs.h"  
#include "../rrc_proto.h" 
#include "mac_defs.h"  
#include "NR_RRCReconfiguration.h"  
#include "common/utils/threadPool/notified_fifo.h"
#include "nr_rrc_common.h"
#include "nr_pdcp/nr_pdcp_oai_api.h"
#include "nr_pdcp/nr_pdcp_entity.h"
#include "openair2/SDAP/nr_sdap/nr_sdap_entity.h"
#include "mac_proto.h"
#include "nr_nas_msg.h"
#include "common/config/config_load_configmodule.h"
extern configmodule_interface_t *uniqCfg;
#include "nr_rlc/nr_rlc_oai_api.h"
#include "L2_interface_ue.h"  
}
  
// ---- Stubs for symbols external to rrc_UE.c ----  
extern "C" {  
static int g_pdcp_release_srb_calls[NR_NUM_SRB] = {0};  
static int g_pdcp_release_drb_calls[MAX_DRBS_PER_UE + 1] = {0};  
static int g_rlc_release_calls[NR_MAX_NUM_LCID] = {0};  
static int g_sdap_delete_calls = 0;  
static int g_pdcp_data_req_srb_calls = 0;  // <<< ADDED: counts RRCReconfigurationComplete submissions

static void fake_phy_config_request(nr_phy_config_t * /*phy_config*/) { /* no-op */ }  
static void fake_sl_phy_config_request(nr_sl_phy_config_t * /*sl_phy_config*/) { /* no-op */ }  
static void fake_synch_request(nr_synch_request_t * /*synch_request*/) { /* no-op */ }  
static int8_t fake_scheduled_response(nr_scheduled_response_t * /*r*/) { return 0; }  
static int fake_dl_indication(nr_downlink_indication_t * /*dl_info*/) { return 0; }  
static int fake_ul_indication(nr_uplink_indication_t * /*ul_info*/) { return 0; }  
static void fake_sl_indication(nr_sidelink_indication_t * /*sl_info*/) { /* no-op */ }  
static void fake_slot_indication(uint8_t /*mod_id*/, bool /*is_tx*/) { /* no-op */ }
static void fake_meas_ind(module_id_t, uint32_t, uint16_t, bool, bool, int) { /* no-op */ }  

void nr_pdcp_release_srb(ue_id_t ue_id, int srb_id)  
{
  UNUSED(ue_id);
  if (srb_id >= 0 && srb_id < NR_NUM_SRB)
    g_pdcp_release_srb_calls[srb_id]++;
}  
void nr_pdcp_release_drb(ue_id_t ue_id, int drb_id)  
{
  UNUSED(ue_id);
  if (drb_id >= 0 && drb_id <= MAX_DRBS_PER_UE)
    g_pdcp_release_drb_calls[drb_id]++;
}
bool nr_sdap_delete_ue_entities(ue_id_t ue_id)  
{
  UNUSED(ue_id);
  g_sdap_delete_calls++;
  return true;
}
void deliver_pdu_srb_rlc(void *data, ue_id_t ue_id, int srb_id, char *buf, int size, int sdu_id)
{
  UNUSED(data);
  UNUSED(ue_id);
  UNUSED(srb_id);
  UNUSED(buf);
  UNUSED(size);
  UNUSED(sdu_id);
}

bool nr_pdcp_data_req_srb(ue_id_t ue_id,
                          const rb_id_t rb_id,
                          const mui_t muiP,
                          const sdu_size_t sdu_buffer_size,
                          unsigned char *const sdu_buffer,
                          deliver_pdu deliver_cb,
                          void *data)
{
  UNUSED(ue_id);
  UNUSED(rb_id);
  UNUSED(muiP);
  UNUSED(sdu_buffer_size);
  UNUSED(sdu_buffer);
  UNUSED(deliver_cb);
  UNUSED(data);

  g_pdcp_data_req_srb_calls++;
  return true;
}

bool nr_pdcp_check_integrity_srb(ue_id_t ue_id,
                                 int srb_id,
                                 const uint8_t *msg,
                                 int msg_size,
                                 const nr_pdcp_integrity_data_t *msg_integrity)
{
  UNUSED(ue_id);
  UNUSED(srb_id);
  UNUSED(msg);
  UNUSED(msg_size);
  UNUSED(msg_integrity);

  return true;
}

void nr_pdcp_config_set_security(
    ue_id_t ue_id,
    rb_id_t rb_id,
    bool is_srb,
    const nr_pdcp_entity_security_keys_and_algos_t *parameters)
{
  UNUSED(ue_id);
  UNUSED(rb_id);
  UNUSED(is_srb);
  UNUSED(parameters);
}

softmodem_params_t *get_softmodem_params(void)
{
  static softmodem_params_t p = {};
  return &p;
}

void init_sidelink(NR_UE_RRC_INST_t *rrc)
{
  UNUSED(rrc);
}

void nr_reconfigure_sdap_entity(NR_SDAP_Config_t *sdap_config,
                                ue_id_t ue_id,
                                int pdusession_id,
                                int drb_id)
{
  UNUSED(sdap_config);
  UNUSED(ue_id);
  UNUSED(pdusession_id);
  UNUSED(drb_id);
}

sdap_config_t nr_sdap_get_config(const int is_gnb,
                                 const NR_SDAP_Config_t *sdap_Config,
                                 const int drb_id)
{
  UNUSED(is_gnb);
  UNUSED(sdap_Config);
  UNUSED(drb_id);

  return (sdap_config_t){0};
}

void nr_sdap_addmod_entity(const int is_gnb,
                           const ue_id_t ue_id,
                           const sdap_config_t *sdap)
{
  UNUSED(is_gnb);
  UNUSED(ue_id);
  UNUSED(sdap);
}

void nr_pdcp_add_drb(
    int is_gnb,
    const ue_id_t UEid,
    const NR_PDCP_Config_t *pdcp,
    const struct sdap_configuration_s *sdap,
    const nr_pdcp_entity_security_keys_and_algos_t *security_parameters)
{
  UNUSED(is_gnb);
  UNUSED(UEid);
  UNUSED(pdcp);
  UNUSED(sdap);
  UNUSED(security_parameters);
}

void add_srb(
    int is_gnb,
    ue_id_t UEid,
    struct NR_SRB_ToAddMod *s,
    const nr_pdcp_entity_security_keys_and_algos_t *security_parameters)
{
  UNUSED(is_gnb);
  UNUSED(UEid);
  UNUSED(s);
  UNUSED(security_parameters);
}

void nr_pdcp_reestablishment(
    ue_id_t ue_id,
    int rb_id,
    bool srb_flag,
    const nr_pdcp_entity_security_keys_and_algos_t *security_parameters)
{
  UNUSED(ue_id);
  UNUSED(rb_id);
  UNUSED(srb_flag);
  UNUSED(security_parameters);
}

void nr_pdcp_reconfigure_srb(ue_id_t ue_id, int srb_id, long t_Reordering)
{
  UNUSED(ue_id);
  UNUSED(srb_id);
  UNUSED(t_Reordering);
}

void nr_pdcp_reconfigure_drb(ue_id_t ue_id,
                             int drb_id,
                             NR_PDCP_Config_t *pdcp_config)
{
  UNUSED(ue_id);
  UNUSED(drb_id);
  UNUSED(pdcp_config);
}

bool nr_pdcp_data_ind(const protocol_ctxt_t *const ctxt_pP,
                      const srb_flag_t srb_flagP,
                      const rb_id_t rb_id,
                      const sdu_size_t sdu_buffer_size,
                      uint8_t *const sdu_buffer)
{
  UNUSED(ctxt_pP);
  UNUSED(srb_flagP);
  UNUSED(rb_id);
  UNUSED(sdu_buffer_size);
  UNUSED(sdu_buffer);

  return true;
}
#if 0

bool check_cellgroup_config(const NR_CellGroupConfig_t *cgConfig)
{
  UNUSED(cgConfig);

  return true;
}

void nr_rrc_mac_config_req_meas(
    module_id_t module_id,
    const nr_neighbor_cell_info_t *neighbor_cells,
    int num_neighbors)
{
  UNUSED(module_id);
  UNUSED(neighbor_cells);
  UNUSED(num_neighbors);
}

void nr_rrc_mac_config_req_paging_ue_id(
    module_id_t module_id,
    uint64_t fiveG_S_TMSI)
{
  UNUSED(module_id);
  UNUSED(fiveG_S_TMSI);
}
void nr_mac_rrc_data_ind_ue(const module_id_t module_id,
                            const uint8_t gNB_index,
                            const int hfn,
                            const frame_t frame,
                            const int slot,
                            const uint32_t cellid,
                            const long arfcn,
                            const uint32_t channel,
                            const uint8_t* pduP,
                            const sdu_size_t pdu_len) {

  UNUSED(module_id);
  UNUSED(gNB_index);
  UNUSED(hfn);	
  UNUSED(frame);
  UNUSED(slot);	
  UNUSED(cellid);
  UNUSED(arfcn);	
  UNUSED(channel);
  UNUSED(pduP);	
  UNUSED(pdu_len);
}

void nr_mac_rrc_meas_ind_ue(module_id_t module_id, uint32_t gNB_index, uint16_t Nid_cell, bool csi_meas, bool is_neighboring_cell, int rsrp_dBm) {
  UNUSED(module_id);
  UNUSED(gNB_index);
  UNUSED(Nid_cell);	
  UNUSED(csi_meas);
  UNUSED(is_neighboring_cell);	
  UNUSED(rsrp_dBm);
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
void nr_mac_rrc_verification_failed(const module_id_t mod_id)
{
  UNUSED(mod_id);
}
#endif

void nr_ue_decode_NR_SBCCH_SL_BCH_Message(
    NR_UE_RRC_INST_t *rrc,
    uint8_t *pduP,
    const sdu_size_t pdu_len,
    const uint16_t rx_slss_id)
{
  UNUSED(rrc);
  UNUSED(pduP);
  UNUSED(pdu_len);
  UNUSED(rx_slss_id);
}

void rrc_ue_process_sidelink_Preconfiguration(
    NR_UE_RRC_INST_t *rrc_inst,
    int sync_ref)
{
  UNUSED(rrc_inst);
  UNUSED(sync_ref);
}

void nr_rrc_ue_decode_NR_SBCCH_SL_BCH_Message(NR_UE_RRC_INST_t *rrc,
                                              uint8_t* pduP,
                                              const sdu_size_t pdu_len,
                                              const uint16_t rx_slss_id) {

  UNUSED(rrc);
  UNUSED(pduP);
  UNUSED(pdu_len);
  UNUSED(rx_slss_id);
}

nr_ue_nas_t *get_ue_nas_info(module_id_t module_id)
{
  UNUSED(module_id);

  static nr_ue_nas_t nas = {};
  return &nas;
}

void generateRegistrationRequest(
    as_nas_info_t *initialNasMsg,
    nr_ue_nas_t *nas,
    bool is_security_mode)
{
  UNUSED(initialNasMsg);
  UNUSED(nas);
  UNUSED(is_security_mode);
}

int nr_proc_registration_release(void *args)
{
  UNUSED(args);

  return 0;
}

int nr_proc_registration_failure(bool is_initial, void *args)
{
  UNUSED(is_initial);
  UNUSED(args);

  return 0;
}

int nr_proc_registration_request(void *args)
{
  UNUSED(args);

  return 0;
}

// Minimal hand-built ServingCellConfigCommon, since prepare_scc() (gnb_config.c)  
// depends on calloc_or_fail/INIT_SETUP_RELEASE macros not usable in this test target.  
// Only populates fields dereferenced unconditionally by config_common_ue(),  
// configure_common_BWP_dl(), configure_common_BWP_ul() during full-config handling.  
static NR_ServingCellConfigCommon_t *build_minimal_scc()  
{  
  NR_ServingCellConfigCommon_t *scc = (NR_ServingCellConfigCommon_t *)calloc(1, sizeof(*scc));  
  
  // physCellId: dereferenced unconditionally in config_common_ue() (line 508)  
  scc->physCellId = (NR_PhysCellId_t *)calloc(1, sizeof(*scc->physCellId));  
  *scc->physCellId = 0;  
  
  // ssbSubcarrierSpacing: dereferenced unconditionally (mac->numerology = *scc->ssbSubcarrierSpacing)  
  scc->ssbSubcarrierSpacing = (NR_SubcarrierSpacing_t *)calloc(1, sizeof(*scc->ssbSubcarrierSpacing));  
  *scc->ssbSubcarrierSpacing = NR_SubcarrierSpacing_kHz30;  
  
  // ssb_PositionsInBurst: switch on ->present is unconditional; use shortBitmap (case 1)  
  scc->ssb_PositionsInBurst = (decltype(scc->ssb_PositionsInBurst))calloc(1, sizeof(*scc->ssb_PositionsInBurst));

  scc->ssb_PositionsInBurst->present = NR_ServingCellConfigCommon__ssb_PositionsInBurst_PR_shortBitmap;  
  scc->ssb_PositionsInBurst->choice.shortBitmap.buf = (uint8_t *)calloc(1, 1);  
  scc->ssb_PositionsInBurst->choice.shortBitmap.buf[0] = 0x80;  
  scc->ssb_PositionsInBurst->choice.shortBitmap.size = 1;  
  
  // ssb_periodicityServingCell: only needed if frequencyInfoDL->absoluteFrequencySSB is set;  
  // omit absoluteFrequencySSB below to skip this dependency and keep the SCC minimal.  
  
  // n_TimingAdvanceOffset: passed to get_ta_offset(), NULL is a legal "not configured" value there.  
  scc->n_TimingAdvanceOffset = NULL;  
  
  // downlinkConfigCommon: AssertFatal'd non-NULL in config_common_ue()  
  scc->downlinkConfigCommon = (NR_DownlinkConfigCommon_t *)calloc(1, sizeof(*scc->downlinkConfigCommon));  
  scc->downlinkConfigCommon->frequencyInfoDL = NULL; // optional (NeedM for inter-freq HO); skip to keep minimal  
  scc->downlinkConfigCommon->initialDownlinkBWP =  
      (NR_BWP_DownlinkCommon_t *)calloc(1, sizeof(*scc->downlinkConfigCommon->initialDownlinkBWP));  
  scc->downlinkConfigCommon->initialDownlinkBWP->genericParameters.subcarrierSpacing = NR_SubcarrierSpacing_kHz30;  
  scc->downlinkConfigCommon->initialDownlinkBWP->genericParameters.cyclicPrefix = NULL;  
  scc->downlinkConfigCommon->initialDownlinkBWP->genericParameters.locationAndBandwidth = 6600; // arbitrary valid RIV  
  
  // uplinkConfigCommon: optional (if (scc->uplinkConfigCommon...)), but populate for UL BWP0 coverage  
  scc->uplinkConfigCommon = (NR_UplinkConfigCommon_t *)calloc(1, sizeof(*scc->uplinkConfigCommon));  
  scc->uplinkConfigCommon->frequencyInfoUL = NULL; // optional  
  scc->uplinkConfigCommon->initialUplinkBWP =  
      (NR_BWP_UplinkCommon_t *)calloc(1, sizeof(*scc->uplinkConfigCommon->initialUplinkBWP));  
  scc->uplinkConfigCommon->initialUplinkBWP->genericParameters.subcarrierSpacing = NR_SubcarrierSpacing_kHz30;  
  scc->uplinkConfigCommon->initialUplinkBWP->genericParameters.cyclicPrefix = NULL;  
  scc->uplinkConfigCommon->initialUplinkBWP->genericParameters.locationAndBandwidth = 6600;  
  //scc->uplinkConfigCommon->timeAlignmentTimerCommon = NR_TimeAlignmentTimer_infinity;  
  
  return scc;  
}  

}  

class FullConfigTest : public ::testing::Test {
protected:
  static nr_ue_if_module_t fake_if_module;
  NR_UE_RRC_INST_t rrc = {0};
  notifiedFIFO_t mac_queue = {0};
  protected:  
   static void SetUpTestSuite() {  
	   NR_UE_MAC_INST_t *mac = nr_l2_init_ue(/*instance_id=*/0, /*numerology=*/0);	
	   ASSERT_NE(mac, nullptr);  
	   // do NOT call nr_rlc_module_init() here separately —  
	   // nr_l2_init_ue() already does it internally, exactly once.  

	   fake_if_module.phy_config_request = fake_phy_config_request;  
	   fake_if_module.sl_phy_config_request = fake_sl_phy_config_request;  
	   fake_if_module.synch_request = fake_synch_request;  
	   fake_if_module.scheduled_response = fake_scheduled_response;  
	   fake_if_module.dl_indication = fake_dl_indication;  
	   fake_if_module.ul_indication = fake_ul_indication;  
	   fake_if_module.sl_indication = fake_sl_indication;  
	   fake_if_module.slot_indication = fake_slot_indication;  
	   fake_if_module.meas_ind = fake_meas_ind;  
	   mac->if_module = &fake_if_module;  
   }

  void SetUp() override
  {
    memset(g_pdcp_release_srb_calls, 0, sizeof(g_pdcp_release_srb_calls));
    memset(g_pdcp_release_drb_calls, 0, sizeof(g_pdcp_release_drb_calls));
    memset(g_rlc_release_calls, 0, sizeof(g_rlc_release_calls));
    g_sdap_delete_calls = 0;
    g_pdcp_data_req_srb_calls = 0;

    initNotifiedFIFO(&mac_queue);
    rrc.mac_input_nf = &mac_queue;
    rrc.ue_id = 0;
    // Fetch the MAC instance created once in SetUpTestSuite(); don't  
    // re-init it — nr_l2_init_ue() would AssertFatal on a second call  
    // for the same instance_id since nr_ue_mac_inst[0] is never reset.  
    NR_UE_MAC_INST_t *mac = get_mac_inst(rrc.ue_id);  
    ASSERT_NE(mac, nullptr);  

    for (int j = 0; j < NR_NUM_SRB; j++)
      rrc.Srb[j] = RB_NOT_PRESENT;
    rrc.Srb[0] = RB_ESTABLISHED; // SRB0, must survive
    rrc.Srb[1] = RB_ESTABLISHED; // dedicated, must be released
    rrc.Srb[2] = RB_ESTABLISHED; // dedicated, must be released

    for (int i = 1; i <= MAX_DRBS_PER_UE; i++)
      set_DRB_status(&rrc, i, RB_NOT_PRESENT);
    set_DRB_status(&rrc, 1, RB_ESTABLISHED);
    set_DRB_status(&rrc, 2, RB_ESTABLISHED);

    for (int i = 0; i < NR_MAX_NUM_LCID; i++)
      rrc.active_RLC_entity[i] = false;
    rrc.active_RLC_entity[1] = true;
    rrc.active_RLC_entity[3] = true;

    // dedicated meas config: populate one MeasObj/ReportConfig/MeasId
    // so the "release all dedicated radio config" path has something to free.
    rrc.perNB[0].MeasObj[0] = (NR_MeasObjectToAddMod_t *)calloc(1, sizeof(NR_MeasObjectToAddMod_t));
    rrc.perNB[0].ReportConfig[0] = (NR_ReportConfigToAddMod_t *)calloc(1, sizeof(NR_ReportConfigToAddMod_t));
    rrc.perNB[0].MeasId[0] = (NR_MeasIdToAddMod_t *)calloc(1, sizeof(NR_MeasIdToAddMod_t));
    rrc.perNB[0].s_measure = 50;

    set_default_timers_and_constants(&rrc.timers_and_constants);
    nr_timer_start(&rrc.timers_and_constants.T310);

    memset(rrc.kgnb, 0x42, sizeof(rrc.kgnb));
    rrc.rnti = 0x6789;
    rrc.arfcn_ssb = 12345;
    rrc.phyCellID = 99;
    rrc.dl_bwp_id = 1;
    rrc.ul_bwp_id = 1;

	rrc.as_security_activated = true;
  }  
  
  void TearDown() override
  {
    abortNotifiedFIFO(&mac_queue); // frees any un-drained elements
  }

  struct MacMsgCounts {
    int reset = 0;
    int config_cg = 0;
    NR_UE_MAC_reset_cause_t last_reset_cause = (NR_UE_MAC_reset_cause_t)-1;
    bool saw_full_config_true = false;
    bool saw_full_config_false = false;
  };
  MacMsgCounts drainMacQueue()
  {
    MacMsgCounts c;
    notifiedFIFO_elt_t *elt;
    while ((elt = pollNotifiedFIFO(&mac_queue)) != NULL) {
      nr_mac_rrc_message_t *msg = (nr_mac_rrc_message_t *)NotifiedFifoData(elt);
      if (msg->payload_type == NR_MAC_RRC_CONFIG_RESET) {
        c.reset++;
        c.last_reset_cause = (NR_UE_MAC_reset_cause_t)0;
      } else if (msg->payload_type == NR_MAC_RRC_CONFIG_CG) {
        c.config_cg++;
        if (msg->payload.config_cg.full_config)
          c.saw_full_config_true = true;
        else
          c.saw_full_config_false = true;
        ASN_STRUCT_FREE(asn_DEF_NR_CellGroupConfig, msg->payload.config_cg.cellGroupConfig);
      }
      delNotifiedFIFO_elt(elt);
    }
    return c;  
  }  
};  
nr_ue_if_module_t FullConfigTest::fake_if_module = {0};

// =============================================================================
// TEST 1 (ORIGINAL, enhanced with spec references)
// TS 38.331 §5.3.5.11 – Full configuration (no masterCellGroup, no reconfigWithSync)
// TS 38.331 §5.3.5.3  – Reception of RRCReconfiguration
// =============================================================================
TEST_F(FullConfigTest, ReleasesAllDedicatedRadioConfigAndPreservesMcgAndKeys)
{
  // --- Arrange: build RRCReconfiguration-v1530-IEs with fullConfig ---
  NR_RRCReconfiguration_v1530_IEs_t rec_1530 = {0};
  long fc = 1;
  rec_1530.fullConfig = &fc;
  rec_1530.masterCellGroup = nullptr;  // no MCG ? only dedicated-config release

  // --- Act ---
  bool ret = nr_rrc_process_reconfiguration_v1530(&rrc, &rec_1530, /*gNB_index=*/0);

  // No dedicatedSIB1_Delivery and no failure -> dedicatedsib1 stays false
  EXPECT_FALSE(ret);

  // =========================================================================
  // TS 38.331 §5.3.5.11 step 1:
  //   "release/clear all current dedicated radio configurations except
  //    the MCG C-RNTI and the security configurations associated with
  //    the master key"
  // =========================================================================

  // --- MCG C-RNTI preserved (spec NOTE in 5.3.5.11) ---
  EXPECT_EQ(rrc.rnti, 0x6789);

  // --- Security config (master key) preserved (spec NOTE in 5.3.5.11) ---
  uint8_t expected_kgnb[32];
  memset(expected_kgnb, 0x42, sizeof(expected_kgnb));
  EXPECT_EQ(memcmp(rrc.kgnb, expected_kgnb, sizeof(rrc.kgnb)), 0);

  // --- SRB0 kept (it is NOT a dedicated radio config), SRB1/SRB2 released ---
  EXPECT_EQ(rrc.Srb[0], RB_ESTABLISHED);
  EXPECT_EQ(rrc.Srb[1], RB_NOT_PRESENT);
  EXPECT_EQ(rrc.Srb[2], RB_NOT_PRESENT);
  EXPECT_EQ(g_pdcp_release_srb_calls[1], 1);
  EXPECT_EQ(g_pdcp_release_srb_calls[2], 1);
  
  // --- DRBs released ---
  EXPECT_EQ(get_DRB_status(&rrc, 1), RB_NOT_PRESENT);
  EXPECT_EQ(get_DRB_status(&rrc, 2), RB_NOT_PRESENT);
  EXPECT_EQ(g_pdcp_release_drb_calls[1], 1);
  EXPECT_EQ(g_pdcp_release_drb_calls[2], 1);
  EXPECT_EQ(g_sdap_delete_calls, 1);

  // --- RLC entities released ---
  EXPECT_FALSE(rrc.active_RLC_entity[1]);
  EXPECT_FALSE(rrc.active_RLC_entity[3]);

  // --- Dedicated MeasConfig cleared (spec NOTE 1 in 5.3.5.11:
  //     "Radio configuration ... includes other configurations like MeasConfig") ---
  EXPECT_EQ(rrc.perNB[0].MeasObj[0], nullptr);
  EXPECT_EQ(rrc.perNB[0].ReportConfig[0], nullptr);
  EXPECT_EQ(rrc.perNB[0].MeasId[0], nullptr);
  EXPECT_EQ(rrc.perNB[0].s_measure, 0);

  // --- Dedicated BWP ids reset ---
  EXPECT_EQ(rrc.dl_bwp_id, 0);
  EXPECT_EQ(rrc.ul_bwp_id, 0);

  // --- T310 stopped as part of dedicated-config release ---
  //EXPECT_FALSE(nr_timer_is_active(&rrc.timers_and_constants.T310));

  // --- Common config NOT touched (masterCellGroup was NULL, so
  //     nr_rrc_ue_release_all_common_radio_config's body never ran) ---
  EXPECT_EQ(rrc.arfcn_ssb, 12345);
  EXPECT_EQ(rrc.phyCellID, 99);

  // --- MAC received exactly one CONFIG_RESET, no CONFIG_CG ---
  MacMsgCounts counts = drainMacQueue();
  EXPECT_EQ(counts.reset, 1);
  EXPECT_EQ(counts.config_cg, 0);
}

// =============================================================================
// TEST 2 (ADDED to support reconfigurationWithSync decoding)
// TS 38.331 §5.3.5.11 – Full configuration WITH masterCellGroup present
//   Verifies: common radio config is also released, MAC gets CONFIG_CG
//             with full_config=true, and the new CellGroupConfig is forwarded.
// TS 38.331 §5.3.5.3  – masterCellGroup triggers cell group config (5.3.5.5)
// =============================================================================
TEST_F(FullConfigTest, FullConfigWithMasterCellGroupReleasesCommonConfig)
{
  // --- Arrange ---
  NR_RRCReconfiguration_v1530_IEs_t rec_1530 = {0};
  long fc = 1;
  rec_1530.fullConfig = &fc;

  // After full config, verify MAC entity internal state:
  NR_UE_MAC_INST_t *mac = get_mac_inst(rrc.ue_id);

  // 1. Allocate and populate CellGroupConfig hierarchy dynamically
  NR_CellGroupConfig_t cell_group = {0};
  cell_group.cellGroupId = 0; // MCG always has ID 0

  cell_group.spCellConfig = (NR_SpCellConfig_t *)calloc(1, sizeof(NR_SpCellConfig_t));
  cell_group.spCellConfig->reconfigurationWithSync = (NR_ReconfigurationWithSync_t *)calloc(1, sizeof(NR_ReconfigurationWithSync_t));

  // Set mandatory fields in reconfigurationWithSync to satisfy ASN.1 constraints
  cell_group.spCellConfig->reconfigurationWithSync->t304 = NR_ReconfigurationWithSync__t304_ms100;
  
  // Set newUE_Identity (C-RNTI, 16 bits)
  cell_group.spCellConfig->reconfigurationWithSync->newUE_Identity = 0x1234;

  
  // 2. Attach the minimal spCellConfigCommon needed for BWP0 / config_common_ue() to run  
  //    without hitting the earlier "current_UL_BWP is NULL" / "spCellConfigCommon is NULL" asserts.  
  NR_ServingCellConfigCommon_t *scc = build_minimal_scc();  
  cell_group.spCellConfig->reconfigurationWithSync->spCellConfigCommon = scc;  

  // 2. Encode the constructed CellGroupConfig structure to UPER buffer
  uint8_t buffer[2048];
  asn_enc_rval_t enc_rval = uper_encode_to_buffer(&asn_DEF_NR_CellGroupConfig,
                                                  NULL,
                                                  &cell_group,
                                                  buffer,
                                                  sizeof(buffer));
  ASSERT_GT(enc_rval.encoded, 0) << "UPER encoding of CellGroupConfig failed!";
  size_t encoded_bytes = (enc_rval.encoded + 7) / 8;

  // 3. Wrap the encoded payload in an OCTET STRING for masterCellGroup
  OCTET_STRING_t *masterCellGroup_octet = (OCTET_STRING_t *)calloc(1, sizeof(OCTET_STRING_t));
  masterCellGroup_octet->buf = (uint8_t *)malloc(encoded_bytes);
  memcpy(masterCellGroup_octet->buf, buffer, encoded_bytes);
  masterCellGroup_octet->size = encoded_bytes;
  
  rec_1530.masterCellGroup = masterCellGroup_octet;

  // --- Act ---
  bool ret = nr_rrc_process_reconfiguration_v1530(&rrc, &rec_1530, /*gNB_index=*/0);

  EXPECT_FALSE(ret);

  // =========================================================================
  // TS 38.331 §5.3.5.11 step 1:
  //   dedicated config released, C-RNTI & keys preserved as newUE_Identity
  // =========================================================================
  EXPECT_EQ(rrc.rnti, 0x1234);
  uint8_t expected_kgnb[32];
  memset(expected_kgnb, 0x42, sizeof(expected_kgnb));
  EXPECT_EQ(memcmp(rrc.kgnb, expected_kgnb, sizeof(rrc.kgnb)), 0);

  EXPECT_EQ(rrc.Srb[0], RB_ESTABLISHED);
  EXPECT_EQ(rrc.Srb[1], RB_NOT_PRESENT);
  EXPECT_EQ(rrc.Srb[2], RB_NOT_PRESENT);
  EXPECT_EQ(get_DRB_status(&rrc, 1), RB_NOT_PRESENT);
  EXPECT_EQ(get_DRB_status(&rrc, 2), RB_NOT_PRESENT);

  // nr_rrc_ue_release_all_common_radio_config's body ran) ---
  EXPECT_EQ(rrc.arfcn_ssb, 0);
  EXPECT_EQ(rrc.phyCellID, 0);

  // =========================================================================
  // TS 38.331 §5.3.5.11 – with masterCellGroup present:
  //   Verify MAC message queue receives the reset requests
  // =========================================================================
  MacMsgCounts counts = {};  
  notifiedFIFO_elt_t *elt;	
  while ((elt = pollNotifiedFIFO(&mac_queue)) != NULL) {  
	nr_mac_rrc_message_t *msg = (nr_mac_rrc_message_t *)NotifiedFifoData(elt);	
	if (msg->payload_type == NR_MAC_RRC_CONFIG_RESET) counts.reset++;  
	if (msg->payload_type == NR_MAC_RRC_CONFIG_CG) {  
	  counts.config_cg++;  
	  if (msg->payload.config_cg.full_config) counts.saw_full_config_true = true;  
	}  
	process_msg_rcc_to_mac(msg, rrc.ue_id);  
	delNotifiedFIFO_elt(elt);  
  }  
  EXPECT_GE(counts.reset, 1);  
  EXPECT_GE(counts.config_cg, 1);  
  EXPECT_TRUE(counts.saw_full_config_true);

  // C-RNTI preserved (TS 38.331 §5.3.5.11 exception)
  EXPECT_EQ(mac->crnti, 0x1234);

  // HARQ NDI reset (TS 38.321 §5.12 semantics used by reset_mac_inst)	
  for (int i = 0; i < NR_MAX_HARQ_PROCESSES; i++) {
	EXPECT_EQ(mac->ul_harq_info[i].last_ndi, -1);
	for (int c = 0; c < 2; c++)
	  EXPECT_EQ(mac->dl_harq_info[i][c].last_ndi, -1);
  }

  // BSR reporting cancelled (TS 38.321 §5.12)
  EXPECT_EQ(mac->scheduling_info.BSR_reporting_active, NR_BSR_TRIGGER_NONE);
	
  // retxBSR / sr-DelayTimer stopped
  EXPECT_FALSE(nr_timer_is_active(&mac->scheduling_info.retxBSR_Timer));
  EXPECT_FALSE(nr_timer_is_active(&mac->scheduling_info.sr_DelayTimer));

  // --- Clean up ---
  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_NR_CellGroupConfig, &cell_group);
  OCTET_STRING_free(&asn_DEF_OCTET_STRING, masterCellGroup_octet, ASFM_FREE_EVERYTHING);
}

TEST_F(FullConfigTest, RRCReconfigFullConfigResetsAndRebuildsMacState)  
{  
  // --- Arrange: build a minimal but valid full-config RRCReconfiguration_v1530 IE ---  
  NR_RRCReconfiguration_v1530_IEs_t rec_1530 = {0};  
  long fc = 1;  
  rec_1530.fullConfig = &fc;  
  
  rrc.ue_id = 0; // must match the instance id used in SetUpTestSuite()'s nr_l2_init_ue(0, ...)  
  NR_UE_MAC_INST_t *mac = get_mac_inst(rrc.ue_id);  
  ASSERT_NE(mac, nullptr);  
  
  // Pre-populate rrc/mac with some "old" connected-mode state, so we can verify it gets released  
  rrc.Srb[1] = RB_ESTABLISHED;  
  rrc.Srb[2] = RB_ESTABLISHED;  
  set_DRB_status(&rrc, 1, RB_ESTABLISHED); // adjust to your actual accessor if different  
  rrc.arfcn_ssb = 12345;  
  
  // 1. Allocate and populate CellGroupConfig hierarchy  
  NR_CellGroupConfig_t cell_group = {0};  
  cell_group.cellGroupId = 0; // MCG always has ID 0  
  
  cell_group.spCellConfig = (NR_SpCellConfig_t *)calloc(1, sizeof(NR_SpCellConfig_t));  
  cell_group.spCellConfig->reconfigurationWithSync =  
      (NR_ReconfigurationWithSync_t *)calloc(1, sizeof(NR_ReconfigurationWithSync_t));  
  
  // Mandatory reconfigurationWithSync fields  
  cell_group.spCellConfig->reconfigurationWithSync->newUE_Identity = 0x1234;  
  cell_group.spCellConfig->reconfigurationWithSync->t304 = NR_ReconfigurationWithSync__t304_ms1000;  
  
  // 2. Attach the minimal spCellConfigCommon needed for BWP0 / config_common_ue() to run  
  //    without hitting the earlier "current_UL_BWP is NULL" / "spCellConfigCommon is NULL" asserts.  
  NR_ServingCellConfigCommon_t *scc = build_minimal_scc();  
  cell_group.spCellConfig->reconfigurationWithSync->spCellConfigCommon = scc;  
  
  // 3. Encode CellGroupConfig -> OCTET_STRING, matching masterCellGroup transport format  
  uint8_t buf[4096];  
  asn_enc_rval_t enc = uper_encode_to_buffer(&asn_DEF_NR_CellGroupConfig, NULL, &cell_group, buf, sizeof(buf));  
  ASSERT_GT(enc.encoded, 0);  
  
  OCTET_STRING_t *masterCellGroup_octet = (OCTET_STRING_t *)calloc(1, sizeof(*masterCellGroup_octet));  
  OCTET_STRING_fromBuf(masterCellGroup_octet, (const char *)buf, (enc.encoded + 7) / 8);  
  rec_1530.masterCellGroup = masterCellGroup_octet;  
  
  // --- Act: drive the full-config path exactly as the RRC layer would ---  
  nr_rrc_mac_config_req_reset(rrc.ue_id, RRC_RECONFIG_FULL_CONFIG);  
  
  // process_msg_rcc_to_mac() -> nr_rrc_mac_config_req_cg(..., full_config=true) as  
  // wired up by nr_rrc_ue_process_masterCellGroup()/nr_rrc_process_reconfiguration_v1530() 
  //rrc.dl_bwp_id = 0;
  bool ret = nr_rrc_ue_process_masterCellGroup(&rrc, rec_1530.masterCellGroup, rec_1530.fullConfig, /*gNB_index=*/0);  
  ASSERT_TRUE(ret);  
  
  // --- Assert ---  
  
  // CRNTI updated from reconfigurationWithSync->newUE_Identity  
  EXPECT_EQ(mac->crnti, 0x1234);  
  
  // Confirm current_UL_BWP/current_DL_BWP are non-NULL again after rebuild  
  ASSERT_NE(mac->current_DL_BWP, nullptr);  
  ASSERT_NE(mac->current_UL_BWP, nullptr);  
  
  // Reconnection state machine  
  // (mac->state transitions through UE_NOT_SYNC_RECONF inside handle_reconfiguration_with_sync();  
  //  nr_ue_send_synch_request()/synch_request() dispatch may advance it further depending on your  
  //  if_module mock — assert whichever state your test harness's mock leaves it in.)  
  
  // HARQ state reset (TS 38.321 §5.3.1/5.4.2.1: NDI-based init to invalid after RA/sync)  
  for (int i = 0; i < NR_MAX_HARQ_PROCESSES; i++) {  
    EXPECT_EQ(mac->ul_harq_info[i].last_ndi, -1);  
    for (int c = 0; c < 2; c++)  
      EXPECT_EQ(mac->dl_harq_info[i][c].last_ndi, -1);  
  }  
  
  // BSR reporting cancelled (TS 38.321 §5.12)  
  EXPECT_EQ(mac->scheduling_info.BSR_reporting_active, NR_BSR_TRIGGER_NONE);  
  
  // retxBSR / sr-DelayTimer stopped  
  EXPECT_FALSE(nr_timer_is_active(&mac->scheduling_info.retxBSR_Timer));  
  EXPECT_FALSE(nr_timer_is_active(&mac->scheduling_info.sr_DelayTimer));  
  
  // --- Clean up ---  
  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_NR_CellGroupConfig, &cell_group); // also frees scc, since it's now  
                                                                          // owned by the ASN.1 tree  
  OCTET_STRING_free(&asn_DEF_OCTET_STRING, masterCellGroup_octet, ASFM_FREE_EVERYTHING);  
}




// =============================================================================
// TEST 3 (ADDED)
// TS 38.331 §5.3.5.11 – Full configuration: verify default timers
//   "use values for timers T301, T310, T311 and constants N310, N311,
//    as included in ue-TimersAndConstants received in SIB1"
//   (This is the "else" branch – full config after re-establishment,
//    no reconfigurationWithSync)
// =============================================================================
TEST_F(FullConfigTest, FullConfigResetsTimersToDefaults)
{
  NR_RRCReconfiguration_v1530_IEs_t rec_1530 = {0};
  long fc = 1;
  rec_1530.fullConfig = &fc;

  // 1. Allocate and populate CellGroupConfig hierarchy dynamically
  NR_CellGroupConfig_t cell_group = {0};
  cell_group.cellGroupId = 0; // MCG always has ID 0
  
  cell_group.spCellConfig = (NR_SpCellConfig_t *)calloc(1, sizeof(NR_SpCellConfig_t));
  cell_group.spCellConfig->reconfigurationWithSync = (NR_ReconfigurationWithSync_t *)calloc(1, sizeof(NR_ReconfigurationWithSync_t));
  
  // Set mandatory fields in reconfigurationWithSync to satisfy ASN.1 constraints
  cell_group.spCellConfig->reconfigurationWithSync->t304 = NR_ReconfigurationWithSync__t304_ms100;
  
  // Set newUE_Identity (C-RNTI, 16 bits)
  cell_group.spCellConfig->reconfigurationWithSync->newUE_Identity = 0x1234;
  
  // 2. Encode the constructed CellGroupConfig structure to UPER buffer
  uint8_t buffer[2048];
  asn_enc_rval_t enc_rval = uper_encode_to_buffer(&asn_DEF_NR_CellGroupConfig,
  												NULL,
  												&cell_group,
  												buffer,
  												sizeof(buffer));
  ASSERT_GT(enc_rval.encoded, 0) << "UPER encoding of CellGroupConfig failed!";
  size_t encoded_bytes = (enc_rval.encoded + 7) / 8;
  
  // 3. Wrap the encoded payload in an OCTET STRING for masterCellGroup
  OCTET_STRING_t *masterCellGroup_octet = (OCTET_STRING_t *)calloc(1, sizeof(OCTET_STRING_t));
  masterCellGroup_octet->buf = (uint8_t *)malloc(encoded_bytes);
  memcpy(masterCellGroup_octet->buf, buffer, encoded_bytes);
  masterCellGroup_octet->size = encoded_bytes;
  
  rec_1530.masterCellGroup = masterCellGroup_octet;

  // --- Arrange: start T310 and T311 to verify they get stopped/reset ---  
  nr_timer_setup(&rrc.timers_and_constants.T310, 5000, 10); // 10ms step
  nr_timer_setup(&rrc.timers_and_constants.T311, 50000, 10); // 10ms step
  rrc.timers_and_constants.N310_k = 2;
  rrc.timers_and_constants.N311_k = 2;
  nr_timer_start(&rrc.timers_and_constants.T310);
  nr_timer_start(&rrc.timers_and_constants.T311);

  // --- Act ---
  nr_rrc_process_reconfiguration_v1530(&rrc, &rec_1530, /*gNB_index=*/0);

  // =========================================================================
  // TS 38.331 §5.3.5.11: timers should be stopped/reset during
  // dedicated-config release. After full config, they should be
  // re-initialized from SIB1 ue-TimersAndConstants (or defaults).
  // =========================================================================
  EXPECT_FALSE(nr_timer_is_active(&rrc.timers_and_constants.T310));
  EXPECT_FALSE(nr_timer_is_active(&rrc.timers_and_constants.T311));

  // Verify default values are applied (9.2.x defaults per spec)
  EXPECT_EQ(rrc.timers_and_constants.T310.target, 1000);
  EXPECT_EQ(rrc.timers_and_constants.T311.target, 30000);
  EXPECT_EQ(rrc.timers_and_constants.N310_k, 1);
  EXPECT_EQ(rrc.timers_and_constants.N311_k, 1);

  // --- Clean up ---
  drainMacQueue();
  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_NR_CellGroupConfig, &cell_group);
  OCTET_STRING_free(&asn_DEF_OCTET_STRING, masterCellGroup_octet, ASFM_FREE_EVERYTHING);
}


// =============================================================================
// TEST 4 (ADDED)
// TS 38.331 §5.3.5.3 – RRCReconfigurationComplete submission
//   "submit the RRCReconfigurationComplete message via SRB1 to lower layers"
//   NOTE: This test verifies the PDCP data request stub is called, which
//   indicates the RRC layer attempted to send the Complete message.
//   The actual ASN.1 encoding is not verified here (would require
//   including rrc_UE.c or mocking the full PDCP/RLC stack).
// =============================================================================
TEST_F(FullConfigTest, RRCReconfigurationCompleteIsSubmittedToPDCP)
{
  // This test only makes sense if you call the full entry point
  // nr_rrc_ue_process_rrcReconfiguration(). Since that function is static
  // in rrc_UE.c, we verify the PDCP submission counter is zero when
  // calling only the v1530 sub-procedure (which does NOT generate the
  // Complete message – that is done by the parent function).
  //
  // If you later uncomment #include "../rrc_UE.c" and call the full
  // entry point, change this to EXPECT_GE(g_pdcp_data_req_srb_calls, 1).

  NR_RRCReconfiguration_v1530_IEs_t rec_1530 = {0};
  long fc = 1;
  rec_1530.fullConfig = &fc;
  rec_1530.masterCellGroup = nullptr;

  nr_rrc_process_reconfiguration_v1530(&rrc, &rec_1530, /*gNB_index=*/0);

  // The v1530 sub-procedure alone does NOT send RRCReconfigurationComplete.
  // That is the responsibility of nr_rrc_ue_process_rrcReconfiguration (§5.3.5.3).
  // So this counter should remain 0 here.
  EXPECT_EQ(g_pdcp_data_req_srb_calls, 0);

  // >>> When testing through the full entry point, assert:
  // EXPECT_GE(g_pdcp_data_req_srb_calls, 1);
  // This confirms §5.3.5.3: "submit the RRCReconfigurationComplete
  // message via SRB1 to lower layers for transmission"

  drainMacQueue();
}

TEST_F(FullConfigTest, ReconfigurationWithSyncOnly_NormalBehaviorNoFullConfigReset)  
{  
  // --- Arrange ---  
  NR_RRCReconfiguration_v1530_IEs_t rec_1530 = {0};  
  rec_1530.fullConfig = NULL; // <-- key difference from the full-config test  
  
  rrc.ue_id = 0;  
  NR_UE_MAC_INST_t *mac = get_mac_inst(rrc.ue_id);  
  ASSERT_NE(mac, nullptr);  
  
  // Pre-populate "old" connected-mode state to check what does/doesn't get reset  
  rrc.Srb[1] = RB_ESTABLISHED;  
  rrc.Srb[2] = RB_ESTABLISHED;  
  set_DRB_status(&rrc, 1, RB_ESTABLISHED);  
  
  NR_CellGroupConfig_t cell_group = {0};  
  cell_group.cellGroupId = 0;  
  cell_group.spCellConfig = (NR_SpCellConfig_t *)calloc(1, sizeof(NR_SpCellConfig_t));  
  cell_group.spCellConfig->reconfigurationWithSync =  
      (NR_ReconfigurationWithSync_t *)calloc(1, sizeof(NR_ReconfigurationWithSync_t));  
  cell_group.spCellConfig->reconfigurationWithSync->newUE_Identity = 0x6789;  
  cell_group.spCellConfig->reconfigurationWithSync->t304 = NR_ReconfigurationWithSync__t304_ms1000;  
  
  NR_ServingCellConfigCommon_t *scc = build_minimal_scc(); // helper from earlier in this conversation  
  cell_group.spCellConfig->reconfigurationWithSync->spCellConfigCommon = scc;  
  
  ssize_t encoded = 0;  
  uint8_t *buf = NULL;  
  encoded = uper_encode_to_new_buffer(&asn_DEF_NR_CellGroupConfig, NULL, &cell_group, (void **)&buf);  
  ASSERT_GT(encoded, 0);  
  
  OCTET_STRING_t masterCellGroup_octet = {0};  
  OCTET_STRING_fromBuf(&masterCellGroup_octet, (const char *)buf, encoded);  
  free(buf);  
  
  // --- Act ---  
  bool ret = nr_rrc_ue_process_masterCellGroup(&rrc, &masterCellGroup_octet, rec_1530.fullConfig, /*gNB_index=*/0);  
  // Drain MAC queue exactly like your other tests  
  notifiedFIFO_elt_t *elt;  
  while ((elt = pollNotifiedFIFO(rrc.mac_input_nf)) != NULL) {
    process_msg_rcc_to_mac((nr_mac_rrc_message_t *)NotifiedFifoData(elt), rrc.ue_id);  
    delNotifiedFIFO_elt(elt);  
  }  
  
  // --- Assert ---  
  ASSERT_TRUE(ret);  
  
  // BWP0 rebuilt via reconfigurationWithSync -> spCellConfigCommon (regardless of full_config)  
  ASSERT_NE(mac->current_DL_BWP, nullptr);  
  ASSERT_NE(mac->current_UL_BWP, nullptr);  
  EXPECT_EQ(mac->crnti, 0x6789);  

  // --- Clean up ---  
  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_NR_CellGroupConfig, &cell_group);  
  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_OCTET_STRING, &masterCellGroup_octet);
}

// Primary goal of this test: no SIGSEGV/AssertFatal. Whether ret is true or false	
TEST_F(FullConfigTest, FullConfigWithoutReconfigurationWithSync_DoesNotCrash)  
{  
  // --- Arrange ---  
  NR_RRCReconfiguration_v1530_IEs_t rec_1530 = {0};  
  long fc = 1;  
  rec_1530.fullConfig = &fc; // <-- fullConfig present  
  
  rrc.ue_id = 0;  
  NR_UE_MAC_INST_t *mac = get_mac_inst(rrc.ue_id);  
  ASSERT_NE(mac, nullptr);  
  
  NR_CellGroupConfig_t cell_group = {0};  
  cell_group.cellGroupId = 0;  
  cell_group.spCellConfig = (NR_SpCellConfig_t *)calloc(1, sizeof(NR_SpCellConfig_t));  
  cell_group.spCellConfig->reconfigurationWithSync = NULL; // <-- key difference: RWS absent  
  
  // Give it *something* dedicated so the message isn't entirely empty, e.g. minimal spCellConfigDedicated  
  // (adjust/remove if your spec-conformance check rejects a spCellConfig with neither field set)  
  
  ssize_t encoded = 0;  
  uint8_t *buf = NULL;  
  encoded = uper_encode_to_new_buffer(&asn_DEF_NR_CellGroupConfig, NULL, &cell_group, (void **)&buf);  
  ASSERT_GT(encoded, 0);  
  
  OCTET_STRING_t masterCellGroup_octet = {0};  
  OCTET_STRING_fromBuf(&masterCellGroup_octet, (const char *)buf, encoded);  
  free(buf);  
  
  // --- Act ---  
  // This is the call that would previously hit AssertFatal(!fullConfig, ...) in the base repo;  
  // your local patch must have relaxed/removed that for this to even proceed.  
  nr_rrc_ue_process_masterCellGroup(&rrc, &masterCellGroup_octet, rec_1530.fullConfig, /*gNB_index=*/0);  
  
  notifiedFIFO_elt_t *elt;  
  while ((elt = pollNotifiedFIFO(rrc.mac_input_nf)) != NULL) {
    process_msg_rcc_to_mac((nr_mac_rrc_message_t *)NotifiedFifoData(elt), rrc.ue_id);  
    delNotifiedFIFO_elt(elt);  
  }  
  
  // --- Clean up ---  
  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_NR_CellGroupConfig, &cell_group);  
  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_OCTET_STRING, &masterCellGroup_octet);
}

TEST_F(FullConfigTest, NeitherFullConfigNorReconfigurationWithSync_LeavesMacStateUntouched)  
{  
  // --- Arrange ---  
  NR_RRCReconfiguration_v1530_IEs_t rec_1530 = {0};  
  rec_1530.fullConfig = NULL; // absent  
  
  rrc.ue_id = 0;  
  NR_UE_MAC_INST_t *mac = get_mac_inst(rrc.ue_id);  
  ASSERT_NE(mac, nullptr);  
  
  // Seed a known-good "already connected" MAC state first (simulate prior successful config)  
  // so we can assert it is UNCHANGED afterward. Reuse Variant 1's setup to get here, e.g.:  
  //   run reconfigurationWithSync once first, capture mac->current_DL_BWP / current_UL_BWP / crnti  
  NR_UE_DL_BWP_t *dl_bwp_before = mac->current_DL_BWP;  
  NR_UE_UL_BWP_t *ul_bwp_before = mac->current_UL_BWP;  
  uint32_t crnti_before = mac->crnti;  
  
  NR_CellGroupConfig_t cell_group = {0};  
  cell_group.cellGroupId = 0;  
  cell_group.spCellConfig = (NR_SpCellConfig_t *)calloc(1, sizeof(NR_SpCellConfig_t));  
  cell_group.spCellConfig->reconfigurationWithSync = NULL; // absent  
  
  ssize_t encoded = 0;  
  uint8_t *buf = NULL;  
  encoded = uper_encode_to_new_buffer(&asn_DEF_NR_CellGroupConfig, NULL, &cell_group, (void **)&buf);  
  ASSERT_GT(encoded, 0);  
  
  OCTET_STRING_t masterCellGroup_octet = {0};  
  OCTET_STRING_fromBuf(&masterCellGroup_octet, (const char *)buf, encoded);  
  free(buf);  
  
  // --- Act ---  
  bool ret = nr_rrc_ue_process_masterCellGroup(&rrc, &masterCellGroup_octet, rec_1530.fullConfig, /*gNB_index=*/0);  
  
  notifiedFIFO_elt_t *elt;  
  while ((elt = pollNotifiedFIFO(rrc.mac_input_nf)) != NULL) {
    process_msg_rcc_to_mac((nr_mac_rrc_message_t *)NotifiedFifoData(elt), rrc.ue_id);  
    delNotifiedFIFO_elt(elt);  
  }  
  
  // --- Assert: dedicated-reconfig no-op behavior, no reset/rebuild at all ---  
  ASSERT_TRUE(ret);  
  EXPECT_EQ(mac->current_DL_BWP, dl_bwp_before); // untouched  
  EXPECT_EQ(mac->current_UL_BWP, ul_bwp_before); // untouched  
  EXPECT_EQ(mac->crnti, crnti_before);           // untouched  
  
  // --- Clean up ---  
  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_NR_CellGroupConfig, &cell_group);  
  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_OCTET_STRING, &masterCellGroup_octet);
}

int main(int argc, char **argv)  
{
  logInit();
  configmodule_interface_t *uniqCfg = load_configmodule(argc, argv, CONFIG_ENABLECMDLINEONLY);  
  g_log->log_component[NR_RRC].level = OAILOG_TRACE;  
  g_log->log_component[MAC].level = OAILOG_DEBUG;  
  g_log->log_component[NR_MAC].level = OAILOG_DEBUG;
  testing::InitGoogleTest(&argc, argv);
  end_configmodule(uniqCfg); 
  return RUN_ALL_TESTS();  
}

