/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include "openair2/LAYER2/NR_MAC_COMMON/nr_mac_common.h"
#include "executables/softmodem-common.h"
#include "common/utils/LOG/log.h"

softmodem_params_t *get_softmodem_params(void)
{
  return NULL;
}

static void test_is_periodic_occasion_zero_and_negative(void)
{
  printf("Testing is_periodic_occasion() with period <= 0...\n");
  /* Guard against SIGFPE division by zero when period is 0 */
  assert(!is_periodic_occasion(0, 0, 10, 0, 0));
  assert(!is_periodic_occasion(1, 5, 20, 0, 0));
  assert(!is_periodic_occasion(5, 2, 10, -1, 0));
  assert(!is_periodic_occasion(0, 0, 10, -10, 2));
  printf("  -> Passed period <= 0 checks.\n");
}

static void test_is_periodic_occasion_normal(void)
{
  printf("Testing is_periodic_occasion() with valid periodic parameters...\n");
  /* 10 slots per frame, period 10, offset 4 */
  assert(is_periodic_occasion(0, 4, 10, 10, 4));
  assert(!is_periodic_occasion(0, 5, 10, 10, 4));
  assert(is_periodic_occasion(1, 4, 10, 10, 4));
  assert(is_periodic_occasion(2, 4, 10, 10, 4));
  /* 20 slots per frame, period 20, offset 7 */
  assert(is_periodic_occasion(0, 7, 20, 20, 7));
  assert(is_periodic_occasion(1, 7, 20, 20, 7));
  assert(!is_periodic_occasion(1, 8, 20, 20, 7));
  printf("  -> Passed valid periodic occasion checks.\n");
}

static void test_csi_period_offset_null_guards(void)
{
  printf("Testing csi_period_offset() NULL safety guards...\n");
  int period = -999, offset = -999;
  /* Both NULL */
  csi_period_offset(NULL, NULL, &period, &offset);
  assert(period == 0 && offset == 0);

  /* csirep present but choice.periodic is NULL (non-periodic / aperiodic) */
  NR_CSI_ReportConfig_t csirep = {0};
  csirep.reportConfigType.present = NR_CSI_ReportConfig__reportConfigType_PR_aperiodic;
  csirep.reportConfigType.choice.aperiodic = NULL;
  period = -999;
  offset = -999;
  csi_period_offset(&csirep, NULL, &period, &offset);
  assert(period == 0 && offset == 0);

  /* csirep present with PR_periodic but choice.periodic pointer is NULL */
  csirep.reportConfigType.present = NR_CSI_ReportConfig__reportConfigType_PR_periodic;
  csirep.reportConfigType.choice.periodic = NULL;
  period = -999;
  offset = -999;
  csi_period_offset(&csirep, NULL, &period, &offset);
  assert(period == 0 && offset == 0);
  printf("  -> Passed NULL safety guards.\n");
}

static void test_csi_period_offset_valid_resource(void)
{
  printf("Testing csi_period_offset() with valid resource periodicity...\n");
  struct NR_CSI_ResourcePeriodicityAndOffset res_po = {0};
  int period = 0, offset = 0;

  res_po.present = NR_CSI_ResourcePeriodicityAndOffset_PR_slots4;
  res_po.choice.slots4 = 2;
  csi_period_offset(NULL, &res_po, &period, &offset);
  assert(period == 4 && offset == 2);

  res_po.present = NR_CSI_ResourcePeriodicityAndOffset_PR_slots20;
  res_po.choice.slots20 = 15;
  csi_period_offset(NULL, &res_po, &period, &offset);
  assert(period == 20 && offset == 15);

  res_po.present = NR_CSI_ResourcePeriodicityAndOffset_PR_slots160;
  res_po.choice.slots160 = 42;
  csi_period_offset(NULL, &res_po, &period, &offset);
  assert(period == 160 && offset == 42);
  printf("  -> Passed valid resource periodicity checks.\n");
}

static void test_csi_period_offset_valid_report(void)
{
  printf("Testing csi_period_offset() with valid report periodicity...\n");
  NR_CSI_ReportConfig_t csirep = {0};
  struct NR_CSI_ReportConfig__reportConfigType__periodic periodic_cfg = {0};
  csirep.reportConfigType.present = NR_CSI_ReportConfig__reportConfigType_PR_periodic;
  csirep.reportConfigType.choice.periodic = &periodic_cfg;

  periodic_cfg.reportSlotConfig.present = NR_CSI_ReportPeriodicityAndOffset_PR_slots10;
  periodic_cfg.reportSlotConfig.choice.slots10 = 5;
  int period = 0, offset = 0;
  csi_period_offset(&csirep, NULL, &period, &offset);
  assert(period == 10 && offset == 5);

  periodic_cfg.reportSlotConfig.present = NR_CSI_ReportPeriodicityAndOffset_PR_slots80;
  periodic_cfg.reportSlotConfig.choice.slots80 = 33;
  csi_period_offset(&csirep, NULL, &period, &offset);
  assert(period == 80 && offset == 33);
  printf("  -> Passed valid report periodicity checks.\n");
}

int main(void)
{
  logInit();
  test_is_periodic_occasion_zero_and_negative();
  test_is_periodic_occasion_normal();
  test_csi_period_offset_null_guards();
  test_csi_period_offset_valid_resource();
  test_csi_period_offset_valid_report();
  printf("All CSI scheduling and periodicity tests passed successfully.\n");
  return 0;
}
