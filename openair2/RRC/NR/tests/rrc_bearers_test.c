/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include "common/utils/utils.h"
#include "common/utils/assertions.h"
#include "openair2/RRC/NR/rrc_gNB_radio_bearers.h"
#include "openair2/RRC/NR/nr_rrc_defs.h"
#include "openair2/COMMON/rrc_messages_types.h"
#include "ds/seq_arr.h"
#include "common/utils/LOG/log.h"
#include "common/5g_platform_types.h"

int nr_rlc_get_available_tx_space(int module_id, int rnti, int drb_id)
{
  UNUSED(module_id);
  UNUSED(rnti);
  UNUSED(drb_id);
  return 0;
}
softmodem_params_t *get_softmodem_params(void) { return NULL; }
configmodule_interface_t *uniqCfg = NULL;

void exit_function(const char *file, const char *function, const int line, const char *s, const int assert)
{
  UNUSED(assert);
  printf("detected error at %s:%d:%s: %s\n", file, line, function, s);
  abort();
}

static void test_rrc_pdu_session(void)
{
  const int pdu_session_id = 3;
  gNB_RRC_UE_t ue = {0};
  seq_arr_init(&ue.pduSessions, sizeof(rrc_pdu_session_param_t));

  /* test add */
  pdusession_t p1 = {0};
  seq_arr_init(&p1.qos, sizeof(nr_rrc_qos_t));
  p1.pdusession_id = pdu_session_id;
  p1.n3_incoming.teid = 2002;
  LOG_I(NR_RRC, "Adding first PDU Session with ID %d\n", p1.pdusession_id);
  rrc_pdu_session_param_t *s1 = add_pduSession(&ue.pduSessions, &p1); // add 1st PDU Session
  AssertFatal(s1 != NULL, "Could not add PDU Session\n");
  AssertFatal(s1->param.pdusession_id == p1.pdusession_id, "PDU Session ID mismatch in added PDU Session\n");
  AssertFatal(s1->param.n3_incoming.teid == p1.n3_incoming.teid, "teid mismatch in added PDU Session\n");
  LOG_A(NR_RRC, "First PDU Session added successfully\n");

  /* test find */
  rrc_pdu_session_param_t *a1 = find_pduSession(&ue.pduSessions, p1.pdusession_id);
  AssertFatal(a1 != NULL, "Could not find PDU Session\n");
  AssertFatal(a1 == s1, "Found PDU Session mismatch\n");
  LOG_A(NR_RRC, "PDU Session find test passed\n");

  /* test add duplicate */
  pdusession_t input2 = {0};
  seq_arr_init(&input2.qos, sizeof(nr_rrc_qos_t));
  input2.pdusession_id = pdu_session_id;
  input2.n3_incoming.teid = 9999;
  rrc_pdu_session_param_t *s2 = add_pduSession(&ue.pduSessions, &input2); // add 2nd PDU Session
  AssertFatal(s2 == NULL, "Duplicated PDU session was added!\n");
  LOG_A(NR_RRC, "Duplicated PDU session test passed\n");
  seq_arr_free(&input2.qos, NULL);

  /* add DRB and fetch PDU Session */
  seq_arr_init(&ue.drbs, sizeof(drb_t));
  nr_pdcp_configuration_t pdcp = {.drb.discard_timer = 100, .drb.sn_size = 18, .drb.t_reordering = 50};
  drb_t *added = nr_rrc_add_drb(&ue.drbs, pdu_session_id, &pdcp);
  AssertFatal(added != NULL, "Failed to add DRB");
  rrc_pdu_session_param_t *a2 = find_pduSession_from_drbId(&ue, added->drb_id);
  AssertFatal(a2 && a2->param.pdusession_id == pdu_session_id, "find_pduSession_from_drbId failed");

  seq_arr_free(&ue.pduSessions, free_pdusession);
  seq_arr_free(&ue.drbs, free_drb);
}

// ---------------- DRB TESTS ----------------

static void test_rrc_drb(void)
{
  LOG_I(NR_RRC, "Starting DRB test\n");
  seq_arr_t pduSessions = {0};
  seq_arr_t drbs = {0};
  seq_arr_init(&pduSessions, sizeof(rrc_pdu_session_param_t));
  seq_arr_init(&drbs, sizeof(drb_t));

  /* test add 1 DRB to 1st PDU session */
  const int id1 = 1;
  pdusession_t s1 = {0};
  seq_arr_init(&s1.qos, sizeof(nr_rrc_qos_t));
  s1.pdusession_id = id1;
  add_pduSession(&pduSessions, &s1); // add PDU session

  nr_pdcp_configuration_t pdcp = {.drb.discard_timer = 100, .drb.sn_size = 18, .drb.t_reordering = 50};
  drb_t *in1 = nr_rrc_add_drb(&drbs, id1, &pdcp); // add DRB
  AssertFatal(in1, "add_rrc_drb failed");
  LOG_A(NR_RRC, "First DRB created with ID %d\n", in1->drb_id);

  drb_t *out1 = get_drb(&drbs, 1); // fetch DRB
  AssertFatal(out1 && out1 == in1, "get_drb failed");
  LOG_A(NR_RRC, "DRB retrieval test passed\n");

  /* test add DRB to 2nd PDU session */
  const int id2 = 2;
  pdusession_t s2 = {0};
  seq_arr_init(&s2.qos, sizeof(nr_rrc_qos_t));
  s2.pdusession_id = id2;
  add_pduSession(&pduSessions, &s2); // add 2nd PDU session
  drb_t *in2 = nr_rrc_add_drb(&drbs, id2, &pdcp); // add DRB to 2nd PDU session
  AssertFatal(in2, "add_rrc_drb failed");
  LOG_A(NR_RRC, "Second DRB created with ID %d\n", in2->drb_id);
  drb_t *out2 = get_drb(&drbs, 2);
  AssertFatal(out2 && out2->drb_id == 2, "get_drb failed");
  LOG_A(NR_RRC, "Second DRB retrieval test passed\n");

  seq_arr_free(&pduSessions, free_pdusession);
  seq_arr_free(&drbs, free_drb);
}

// ---------------- QoS TESTS ----------------

static void test_rrc_qos(void)
{
  // Setup RRC instance and UE context
  gNB_RRC_INST rrc = {0};
  rrc.configuration.enable_sdap = true;
  rrc.pdcp_config.drb.discard_timer = 100;
  rrc.pdcp_config.drb.sn_size = 18;
  rrc.pdcp_config.drb.t_reordering = 50;

  gNB_RRC_UE_t ue = {0};
  ue.rrc_ue_id = 1;
  seq_arr_init(&ue.pduSessions, sizeof(rrc_pdu_session_param_t));
  seq_arr_init(&ue.drbs, sizeof(drb_t));

  const int ps70 = 70;
  pdusession_t in = {0};
  in.pdusession_id = ps70;
  seq_arr_init(&in.qos, sizeof(nr_rrc_qos_t));
  rrc_pdu_session_param_t *pdu70 = add_pduSession(&ue.pduSessions, &in);
  AssertFatal(pdu70 != NULL, "Failed to add PDU Session %d\n", ps70);
  LOG_I(NR_RRC, "Created PDU Session %d for QoS test\n", ps70);

  // Test single non-dynamic QoS flow
  const int qfi = 4;
  const pdusession_level_qos_parameter_t param = {
      .qfi = qfi,
      .fiveQI_type = NON_DYNAMIC,
      .qos_characteristics.non_dynamic =
          {
              .fiveQI = 9,
          },
  };
  LOG_A(NR_RRC, "Adding QoS flow with QFI %d\n", qfi);
  nr_rrc_qos_t *added = add_qos(&pdu70->param.qos, &param);
  AssertFatal(added, "add_qos failed");
  LOG_A(NR_RRC, "QoS flow added successfully\n");

  nr_rrc_qos_t *found = find_qos(&pdu70->param.qos, qfi);
  AssertFatal(found && found == added, "find_qos failed");
  LOG_A(NR_RRC, "QoS flow find test passed\n");

  // Test dynamic QoS flow
  const int dyn_qfi = 5;
  uint16_t dyn_fiveqi = 7;
  const pdusession_level_qos_parameter_t dyn_param = {
      .qfi = dyn_qfi,
      .fiveQI_type = DYNAMIC,
      .qos_characteristics.dynamic =
          {
              .fiveQI = &dyn_fiveqi,
              .qos_priority = 10,
              .packet_delay_budget = 20,
              .per = {.scalar = 1, .exponent = 6},
          },
  };
  LOG_A(NR_RRC, "Adding Dynamic QoS flow with QFI %d\n", dyn_qfi);
  nr_rrc_qos_t *dyn_added = add_qos(&pdu70->param.qos, &dyn_param);
  AssertFatal(dyn_added, "add_qos (dynamic) failed");

  nr_rrc_qos_t *dyn_found = find_qos(&pdu70->param.qos, dyn_qfi);
  AssertFatal(dyn_found && dyn_found == dyn_added, "find_qos (dynamic) failed");
  AssertFatal(dyn_found->qos.fiveQI_type == DYNAMIC, "dynamic flow type mismatch");
  AssertFatal(dyn_found->qos.qos_characteristics.dynamic.fiveQI != NULL, "dynamic fiveQI missing");
  AssertFatal(*dyn_found->qos.qos_characteristics.dynamic.fiveQI == dyn_fiveqi, "dynamic fiveQI mismatch");
  LOG_A(NR_RRC, "Dynamic QoS flow add/find test passed\n");
  AssertFatal(rm_qos(&pdu70->param.qos, dyn_qfi), "rm_qos (dynamic) failed");
  AssertFatal(find_qos(&pdu70->param.qos, dyn_qfi) == NULL, "dynamic QoS flow still present after remove");

  // Test multiple QoS flows
  LOG_I(NR_RRC, "Testing multiple QoS flows\n");
  const int qf5 = 5, qf6 = 6;
  const pdusession_level_qos_parameter_t param2 = {
      .qfi = qf5,
      .fiveQI_type = NON_DYNAMIC,
      .qos_characteristics.non_dynamic = {.fiveQI = 8, .qos_priority = NULL},
  };
  const pdusession_level_qos_parameter_t param3 = {
      .qfi = qf6,
      .fiveQI_type = NON_DYNAMIC,
      .qos_characteristics.non_dynamic = {.fiveQI = 7, .qos_priority = NULL},
  };

  LOG_I(NR_RRC, "Adding second QoS flow with QFI %d\n", qf5);
  nr_rrc_qos_t *added2 = add_qos(&pdu70->param.qos, &param2);
  AssertFatal(added2, "add_qos failed for QFI %d", qf5);

  LOG_I(NR_RRC, "Adding third QoS flow with QFI %d\n", qf6);
  nr_rrc_qos_t *added3 = add_qos(&pdu70->param.qos, &param3);
  AssertFatal(added3, "add_qos failed for QFI %d", qf6);

  // Verify all QoS flows were added
  AssertFatal(seq_arr_size(&pdu70->param.qos) == 3, "Expected 3 QoS flows, got %ld", seq_arr_size(&pdu70->param.qos));

  // Test finding each QoS flow
  nr_rrc_qos_t *found2 = find_qos(&pdu70->param.qos, qf5);
  nr_rrc_qos_t *found3 = find_qos(&pdu70->param.qos, qf6);

  AssertFatal(found2 && found2 == added2, "find_qos failed for QFI %d", qf5);
  AssertFatal(found3 && found3 == added3, "find_qos failed for QFI %d", qf6);
  LOG_A(NR_RRC, "Multi-QoS flow find tests passed (QFIs: %d, %d, %d)\n", qfi, qf5, qf6);

  // Test DRB creation with multiple QoS flows
  nr_pdcp_configuration_t pdcp = {.drb.discard_timer = 100, .drb.sn_size = 18, .drb.t_reordering = 50};

  drb_t *drb = nr_rrc_add_drb(&ue.drbs, ps70, &pdcp);
  AssertFatal(drb, "Failed to create DRB with multiple QoS flows");
  LOG_A(NR_RRC, "DRB created successfully with ID %d and %zu QoS flows\n", drb->drb_id, seq_arr_size(&pdu70->param.qos));

  // Dynamic5QI with packet delay budget and packet error rate
  LOG_I(NR_RRC, "Testing Dynamic5QI QoS flow\n");
  const int ps80 = 80;
  pdusession_t session2 = {0};
  session2.pdusession_id = ps80;
  seq_arr_init(&session2.qos, sizeof(nr_rrc_qos_t));
  rrc_pdu_session_param_t *pdu80 = add_pduSession(&ue.pduSessions, &session2);
  AssertFatal(pdu80 != NULL, "Failed to add PDU Session %d\n", ps80);

  const int qf10 = 10;
  uint16_t dyn5qi_70 = 70;
  pdusession_level_qos_parameter_t param4 = {
      .qfi = qf10,
      .fiveQI_type = DYNAMIC,
      .qos_characteristics.dynamic =
          {
              .fiveQI = &dyn5qi_70,
              .qos_priority = 10,
              .packet_delay_budget = 100,
              .per = {.scalar = 6, .exponent = 3},
          },
      .arp = {.priority_level = 5, .pre_emp_capability = PEC_MAY_TRIGGER_PREEMPTION, .pre_emp_vulnerability = PEV_PREEMPTABLE}};
  nr_rrc_qos_t *added4 = add_qos(&pdu80->param.qos, &param4);
  AssertFatal(added4, "add_qos failed for Dynamic5QI\n");

  // GBR QoS flow
  LOG_I(NR_RRC, "Testing GBR QoS flow\n");
  const int ps90 = 90;
  pdusession_t session3 = {0};
  session3.pdusession_id = ps90;
  seq_arr_init(&session3.qos, sizeof(nr_rrc_qos_t));
  rrc_pdu_session_param_t *pdu90 = add_pduSession(&ue.pduSessions, &session3);
  AssertFatal(pdu90 != NULL, "Failed to add PDU Session %d\n", ps90);

  const int qf11 = 11;
  qos_priority_level_t qos_priority_1 = 1;
  gbr_qos_flow_information_t gbr_info = {
    .dl = {.guaranteedFlowBitRate = 10000, .maximumFlowBitRate = 20000},
    .ul = {.guaranteedFlowBitRate = 5000, .maximumFlowBitRate = 10000}
  };
  pdusession_level_qos_parameter_t param5 = {.qfi = qf11,
                                             .fiveQI_type = NON_DYNAMIC,
                                             .qos_characteristics.non_dynamic =
                                                 {
                                                     .fiveQI = 1,
                                                     .qos_priority = &qos_priority_1,
                                                 },
                                             .arp = {.priority_level = 3,
                                                     .pre_emp_capability = PEC_SHALL_NOT_TRIGGER_PREEMPTION,
                                                     .pre_emp_vulnerability = PEV_NOT_PREEMPTABLE},
                                             .gbr_qos_flow_information = &gbr_info};
  nr_rrc_qos_t *added5 = add_qos(&pdu90->param.qos, &param5);
  AssertFatal(added5, "add_qos failed for GBR QoS flow\n");

  // Test intelligent QoS-to-DRB mapping (nr_rrc_add_bearers)
  LOG_I(NR_RRC, "Testing intelligent QoS-to-DRB mapping (nr_rrc_add_bearers)\n");
  const int ps100 = 100;
  pdusession_t session4 = {0};
  session4.pdusession_id = ps100;
  seq_arr_init(&session4.qos, sizeof(nr_rrc_qos_t));

  const int qf1 = 1;
  const int qf2 = 2;
  qos_priority_level_t qos_priority_5 = 5;
  qos_priority_level_t qos_priority_6 = 6;
  pdusession_level_qos_parameter_t qos1 = {
      .qfi = qf1,
      .fiveQI_type = NON_DYNAMIC,
      .qos_characteristics.non_dynamic = {.fiveQI = 5, .qos_priority = &qos_priority_5},
      .arp = {.priority_level = 10, .pre_emp_capability = PEC_MAY_TRIGGER_PREEMPTION, .pre_emp_vulnerability = PEV_PREEMPTABLE}};
  pdusession_level_qos_parameter_t qos2 = {
      .qfi = qf2,
      .fiveQI_type = NON_DYNAMIC,
      .qos_characteristics.non_dynamic = {.fiveQI = 6, .qos_priority = &qos_priority_6},
      .arp = {.priority_level = 11, .pre_emp_capability = PEC_MAY_TRIGGER_PREEMPTION, .pre_emp_vulnerability = PEV_PREEMPTABLE}};
  nr_rrc_qos_t *added6 = add_qos(&session4.qos, &qos1);
  nr_rrc_qos_t *added7 = add_qos(&session4.qos, &qos2);
  AssertFatal(added6 && added7, "Failed to add QoS flows\n");

  pdusession_t sessions[1] = {session4};
  nr_rrc_add_bearers(&rrc, &ue, 1, sessions);

  AssertFatal(added6->drb_id > 0, "QoS flow 1 not assigned to DRB\n");
  AssertFatal(added7->drb_id > 0, "QoS flow 2 not assigned to DRB\n");
  rrc_pdu_session_param_t *pdu = find_pduSession(&ue.pduSessions, ps100);
  AssertFatal(pdu != NULL, "PDU session not found\n");
  AssertFatal(seq_arr_size(&pdu->param.qos) == 2, "Expected 2 QoS flows in PDU session\n");
  found = find_qos(&pdu->param.qos, qf1);
  AssertFatal(found != NULL, "QoS flow 1 not found\n");
  AssertFatal(found == added6, "QoS flow 1 not found in PDU session\n");
  found = find_qos(&pdu->param.qos, qf2);
  AssertFatal(found != NULL, "QoS flow 2 not found\n");
  AssertFatal(found == added7, "QoS flow 2 not found in PDU session\n");

  // Test dedicated DRB mapping
  LOG_I(NR_RRC, "Testing dedicated DRB mapping\n");
  const int ps200 = 200;
  pdusession_t session5 = {0};
  session5.pdusession_id = ps200;
  seq_arr_init(&session5.qos, sizeof(nr_rrc_qos_t));

  const int qf20 = 20;
  qos_priority_level_t qos_priority_dedicated = 1;
  pdusession_level_qos_parameter_t qos_dedicated = {
      .qfi = qf20,
      .fiveQI_type = NON_DYNAMIC,
      .qos_characteristics.non_dynamic = {
          .fiveQI = 80,
          .qos_priority = &qos_priority_dedicated,
      },
      .arp = {
          .priority_level = 1,
          .pre_emp_capability = PEC_SHALL_NOT_TRIGGER_PREEMPTION,
          .pre_emp_vulnerability = PEV_NOT_PREEMPTABLE,
      },
  };
  nr_rrc_qos_t *added8 = add_qos(&session5.qos, &qos_dedicated);
  AssertFatal(added8, "Failed to add dedicated DRB QoS flow\n");

  pdusession_t sessions2[1] = {session5};
  nr_rrc_add_bearers(&rrc, &ue, 1, sessions2);

  rrc_pdu_session_param_t *pdu5 = find_pduSession(&ue.pduSessions, ps200);
  AssertFatal(pdu5 != NULL, "PDU session 200 not found\n");
  found = find_qos(&pdu5->param.qos, qf20);
  AssertFatal(found != NULL, "QoS flow 3 not found\n");
  AssertFatal(found == added8, "QoS flow 3 not found in PDU session\n");
  AssertFatal(added8->drb_id > 0, "Dedicated QoS flow not assigned to DRB\n");
  AssertFatal(seq_arr_size(&ue.drbs) > 0, "No DRBs were created\n");

  seq_arr_free(&ue.pduSessions, free_pdusession);
  seq_arr_free(&ue.drbs, free_drb);
  LOG_A(NR_RRC, "QoS test completed successfully\n");
}

static void test_rrc_collect_released_drbs(void)
{
  seq_arr_t pduSessions, drbs;
  seq_arr_init(&pduSessions, sizeof(rrc_pdu_session_param_t));
  seq_arr_init(&drbs, sizeof(drb_t));
  for (int id = 1; id <= 2; id++) {
    pdusession_t in = {.pdusession_id = id};
    seq_arr_init(&in.qos, sizeof(nr_rrc_qos_t));
    AssertFatal(add_pduSession(&pduSessions, &in), "add_pduSession failed\n");
  }
  nr_pdcp_configuration_t pdcp = {.drb.discard_timer = 100, .drb.sn_size = 18, .drb.t_reordering = 50};
  const int drb_used = nr_rrc_add_drb(&drbs, 1, &pdcp)->drb_id;
  const int drb_released = nr_rrc_add_drb(&drbs, 1, &pdcp)->drb_id;
  nr_rrc_add_drb(&drbs, 2, &pdcp); // no QoS flow either, but another PDU session

  rrc_pdu_session_param_t *s1 = find_pduSession(&pduSessions, 1);
  const pdusession_level_qos_parameter_t q = {.qfi = 1, .fiveQI_type = NON_DYNAMIC, .qos_characteristics.non_dynamic.fiveQI = 9};
  add_qos(&s1->param.qos, &q)->drb_id = drb_used;

  int out[MAX_DRBS_PER_UE];
  const int n = nr_rrc_collect_released_drbs(&drbs, s1, out);
  AssertFatal(n == 1 && out[0] == drb_released, "expected only DRB %d, got %d DRB(s)\n", drb_released, n);

  seq_arr_free(&pduSessions, free_pdusession);
  seq_arr_free(&drbs, free_drb);
  LOG_A(NR_RRC, "Released DRB collection test passed\n");
}

int main()
{
  // Initialize logging system
  logInit();
  // Set logging level for NR_RRC to show INFO messages
  set_log(NR_RRC, OAILOG_INFO);
  // PDU Session
  test_rrc_pdu_session();
  // DRB
  test_rrc_drb();
  // QoS (including multi-QoS)
  test_rrc_qos();
  test_rrc_collect_released_drbs();
  LOG_A(NR_RRC, "All RRC Bearers tests passed successfully!\n");
  return 0;
}
