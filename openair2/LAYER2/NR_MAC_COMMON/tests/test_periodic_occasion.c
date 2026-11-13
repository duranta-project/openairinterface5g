/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "openair2/LAYER2/NR_MAC_COMMON/nr_mac_common.h"
#include "executables/softmodem-common.h"

softmodem_params_t *get_softmodem_params(void)
{
  return NULL;
}

int main(void)
{
  // Test that period <= 0 returns false without divide-by-zero (SIGFPE)
  assert(!is_periodic_occasion(0, 0, 10, 0, 0));
  assert(!is_periodic_occasion(0, 5, 20, 0, 0));
  assert(!is_periodic_occasion(1, 0, 10, -1, 0));
  assert(!is_periodic_occasion(5, 5, 20, -5, 2));

  // Test valid periodic occasions
  assert(is_periodic_occasion(0, 0, 10, 10, 0));
  assert(is_periodic_occasion(1, 0, 10, 10, 0));
  assert(is_periodic_occasion(0, 5, 10, 10, 5));
  assert(!is_periodic_occasion(0, 4, 10, 10, 5));

  // Test csi_period_offset with NULL configurations
  int period = -1, offset = -1;
  bool res = csi_period_offset(NULL, NULL, &period, &offset);
  assert(!res);
  assert(period == 0);
  assert(offset == 0);

  // Test get_monitoring_period_offset with NULL SearchSpace and pointers
  assert(!get_monitoring_period_offset(NULL, &period, &offset));
  assert(period == 0 && offset == 0);
  assert(!get_monitoring_period_offset(NULL, NULL, &offset));
  assert(!get_monitoring_period_offset(NULL, &period, NULL));

  // Test get_monitoring_period_offset with NULL monitoringSlotPeriodicityAndOffset
  NR_SearchSpace_t ss = {0};
  period = -1;
  offset = -1;
  assert(!get_monitoring_period_offset(&ss, &period, &offset));
  assert(period == 0 && offset == 0);

  // Test get_monitoring_period_offset with invalid/nothing choice
  struct NR_SearchSpace__monitoringSlotPeriodicityAndOffset period_choice = {0};
  ss.monitoringSlotPeriodicityAndOffset = &period_choice;
  period_choice.present = NR_SearchSpace__monitoringSlotPeriodicityAndOffset_PR_NOTHING;
  assert(!get_monitoring_period_offset(&ss, &period, &offset));
  assert(period == 0 && offset == 0);

  // Test get_monitoring_period_offset with valid sl1
  period_choice.present = NR_SearchSpace__monitoringSlotPeriodicityAndOffset_PR_sl1;
  assert(get_monitoring_period_offset(&ss, &period, &offset));
  assert(period == 1 && offset == 0);

  // Test get_monitoring_period_offset with valid sl4
  period_choice.present = NR_SearchSpace__monitoringSlotPeriodicityAndOffset_PR_sl4;
  period_choice.choice.sl4 = 3;
  assert(get_monitoring_period_offset(&ss, &period, &offset));
  assert(period == 4 && offset == 3);

  // Test get_monitoring_period_offset with valid sl20
  period_choice.present = NR_SearchSpace__monitoringSlotPeriodicityAndOffset_PR_sl20;
  period_choice.choice.sl20 = 15;
  assert(get_monitoring_period_offset(&ss, &period, &offset));
  assert(period == 20 && offset == 15);

  printf("All periodic occasion unit tests passed.\n");
  return 0;
}
