/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openair2/LAYER2/NR_MAC_COMMON/nr_mac_common.h"
#include "executables/softmodem-common.h"
#include "common/utils/LOG/log.h"
#include "NR_CSI-MeasConfig.h"
#include "NR_CSI-ReportConfig.h"
#include "NR_CSI-ResourceConfig.h"

softmodem_params_t *get_softmodem_params(void)
{
  return NULL;
}

/* Test 1: NULL csi_ReportConfigToAddModList and csi_ResourceConfigToAddModList */
static void test_null_meas_config(void)
{
  printf("Running %s...\n", __func__);
  nr_csi_report_t report_template[MAX_CSI_REPORTCONFIG] = {0};

  NR_CSI_MeasConfig_t meas_config = {0};
  /* Should return early without crash when report list is NULL */
  compute_csi_bitlen(&meas_config, report_template);

  /* Set report list with 0 count, but resource list is NULL */
  struct NR_CSI_MeasConfig__csi_ReportConfigToAddModList rep_list = {0};
  meas_config.csi_ReportConfigToAddModList = &rep_list;
  compute_csi_bitlen(&meas_config, report_template);
  printf("  PASS: NULL config handled safely\n");
}

/* Test 2: Unmatched resource config ID */
static void test_unmatched_resource_id(void)
{
  printf("Running %s...\n", __func__);
  nr_csi_report_t report_template[MAX_CSI_REPORTCONFIG] = {0};

  NR_CSI_MeasConfig_t meas_config = {0};
  struct NR_CSI_MeasConfig__csi_ReportConfigToAddModList rep_list = {0};
  struct NR_CSI_MeasConfig__csi_ResourceConfigToAddModList res_list = {0};

  NR_CSI_ReportConfig_t rep_cfg = {0};
  rep_cfg.reportConfigId = 1;
  rep_cfg.resourcesForChannelMeasurement = 99; /* Non-existent ID */
  rep_cfg.reportQuantity.present = NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_CQI;

  NR_CSI_ReportConfig_t *rep_arr[1] = {&rep_cfg};
  rep_list.list.array = rep_arr;
  rep_list.list.count = 1;

  NR_CSI_ResourceConfig_t res_cfg = {0};
  res_cfg.csi_ResourceConfigId = 10; /* Different ID */
  NR_CSI_ResourceConfig_t *res_arr[1] = {&res_cfg};
  res_list.list.array = res_arr;
  res_list.list.count = 1;

  meas_config.csi_ReportConfigToAddModList = &rep_list;
  meas_config.csi_ResourceConfigToAddModList = &res_list;

  /* Should log error and skip without crash or AssertFatal */
  compute_csi_bitlen(&meas_config, report_template);
  printf("  PASS: Unmatched resource ID handled safely\n");
}

/* Test 3: Wrong CHOICE in csi_RS_ResourceSetList (Issue #810 vulnerability scenario 1) */
static void test_wrong_choice(void)
{
  printf("Running %s...\n", __func__);
  nr_csi_report_t report_template[MAX_CSI_REPORTCONFIG] = {0};

  NR_CSI_MeasConfig_t meas_config = {0};
  struct NR_CSI_MeasConfig__csi_ReportConfigToAddModList rep_list = {0};
  struct NR_CSI_MeasConfig__csi_ResourceConfigToAddModList res_list = {0};

  NR_CSI_ReportConfig_t rep_cfg = {0};
  rep_cfg.reportConfigId = 1;
  rep_cfg.resourcesForChannelMeasurement = 10;
  rep_cfg.reportQuantity.present = NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_CQI;

  NR_CSI_ReportConfig_t *rep_arr[1] = {&rep_cfg};
  rep_list.list.array = rep_arr;
  rep_list.list.count = 1;

  NR_CSI_ResourceConfig_t res_cfg = {0};
  res_cfg.csi_ResourceConfigId = 10;
  res_cfg.resourceType = NR_CSI_ResourceConfig__resourceType_periodic;
  /* CHOICE set to csi_IM_ResourceSetList instead of nzp_CSI_RS_SSB */
  res_cfg.csi_RS_ResourceSetList.present = NR_CSI_ResourceConfig__csi_RS_ResourceSetList_PR_csi_IM_ResourceSetList;

  NR_CSI_ResourceConfig_t *res_arr[1] = {&res_cfg};
  res_list.list.array = res_arr;
  res_list.list.count = 1;

  meas_config.csi_ReportConfigToAddModList = &rep_list;
  meas_config.csi_ResourceConfigToAddModList = &res_list;

  /* In unpatched code, this dereferenced union choice.nzp_CSI_RS_SSB and crashed */
  compute_csi_bitlen(&meas_config, report_template);
  printf("  PASS: Wrong CHOICE handled safely\n");
}

/* Test 4: NULL nzp_CSI_RS_SSB pointer with present set (Issue #810 vulnerability scenario 2) */
static void test_null_choice_pointer(void)
{
  printf("Running %s...\n", __func__);
  nr_csi_report_t report_template[MAX_CSI_REPORTCONFIG] = {0};

  NR_CSI_MeasConfig_t meas_config = {0};
  struct NR_CSI_MeasConfig__csi_ReportConfigToAddModList rep_list = {0};
  struct NR_CSI_MeasConfig__csi_ResourceConfigToAddModList res_list = {0};

  NR_CSI_ReportConfig_t rep_cfg = {0};
  rep_cfg.reportConfigId = 1;
  rep_cfg.resourcesForChannelMeasurement = 10;
  rep_cfg.reportQuantity.present = NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_CQI;

  NR_CSI_ReportConfig_t *rep_arr[1] = {&rep_cfg};
  rep_list.list.array = rep_arr;
  rep_list.list.count = 1;

  NR_CSI_ResourceConfig_t res_cfg = {0};
  res_cfg.csi_ResourceConfigId = 10;
  res_cfg.resourceType = NR_CSI_ResourceConfig__resourceType_periodic;
  res_cfg.csi_RS_ResourceSetList.present = NR_CSI_ResourceConfig__csi_RS_ResourceSetList_PR_nzp_CSI_RS_SSB;
  res_cfg.csi_RS_ResourceSetList.choice.nzp_CSI_RS_SSB = NULL; /* NULL pointer dereference trigger */

  NR_CSI_ResourceConfig_t *res_arr[1] = {&res_cfg};
  res_list.list.array = res_arr;
  res_list.list.count = 1;

  meas_config.csi_ReportConfigToAddModList = &rep_list;
  meas_config.csi_ResourceConfigToAddModList = &res_list;

  /* In unpatched code, this dereferenced NULL pointer and triggered SIGSEGV */
  compute_csi_bitlen(&meas_config, report_template);
  printf("  PASS: NULL choice pointer handled safely\n");
}

/* Test 5: NULL or empty resource set lists */
static void test_null_or_empty_resource_sets(void)
{
  printf("Running %s...\n", __func__);
  nr_csi_report_t report_template[MAX_CSI_REPORTCONFIG] = {0};

  NR_CSI_MeasConfig_t meas_config = {0};
  struct NR_CSI_MeasConfig__csi_ReportConfigToAddModList rep_list = {0};
  struct NR_CSI_MeasConfig__csi_ResourceConfigToAddModList res_list = {0};

  NR_CSI_ReportConfig_t rep_cfg = {0};
  rep_cfg.reportConfigId = 1;
  rep_cfg.resourcesForChannelMeasurement = 10;
  rep_cfg.reportQuantity.present = NR_CSI_ReportConfig__reportQuantity_PR_ssb_Index_RSRP;

  NR_CSI_ReportConfig_t *rep_arr[1] = {&rep_cfg};
  rep_list.list.array = rep_arr;
  rep_list.list.count = 1;

  struct NR_CSI_ResourceConfig__csi_RS_ResourceSetList__nzp_CSI_RS_SSB sets = {0};
  /* csi_SSB_ResourceSetList is NULL */

  NR_CSI_ResourceConfig_t res_cfg = {0};
  res_cfg.csi_ResourceConfigId = 10;
  res_cfg.resourceType = NR_CSI_ResourceConfig__resourceType_periodic;
  res_cfg.csi_RS_ResourceSetList.present = NR_CSI_ResourceConfig__csi_RS_ResourceSetList_PR_nzp_CSI_RS_SSB;
  res_cfg.csi_RS_ResourceSetList.choice.nzp_CSI_RS_SSB = &sets;

  NR_CSI_ResourceConfig_t *res_arr[1] = {&res_cfg};
  res_list.list.array = res_arr;
  res_list.list.count = 1;

  meas_config.csi_ReportConfigToAddModList = &rep_list;
  meas_config.csi_ResourceConfigToAddModList = &res_list;

  /* In unpatched code, this dereferenced sets.csi_SSB_ResourceSetList and crashed */
  compute_csi_bitlen(&meas_config, report_template);
  printf("  PASS: Missing csi_SSB_ResourceSetList handled safely\n");
}

int main(void)
{
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);
  logInit();
  printf("=== Running CSI bitlen security regression tests ===\n");
  printf("Calling test_null_meas_config...\n");
  test_null_meas_config();
  printf("Calling test_unmatched_resource_id...\n");
  test_unmatched_resource_id();
  printf("Calling test_wrong_choice...\n");
  test_wrong_choice();
  printf("Calling test_null_choice_pointer...\n");
  test_null_choice_pointer();
  printf("Calling test_null_or_empty_resource_sets...\n");
  test_null_or_empty_resource_sets();
  printf("=== All CSI bitlen tests passed successfully! ===\n");
  return 0;
}
