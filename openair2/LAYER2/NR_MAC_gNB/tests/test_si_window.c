/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */

#include "../gNB_scheduler_bch.c"

int main(void)
{
  // The helper returns false when this is the scheduled occasion.
  DevAssert(!test_other_sib_sched_occasion(1, 10, 1, 20, 0, 0, 0, 0));
  DevAssert(test_other_sib_sched_occasion(1, 10, 1, 20, 0, 1, 0, 0));
  // Window offset 10 plus monitoring offset 15 carries into the next frame.
  DevAssert(!test_other_sib_sched_occasion(2, 10, 1, 20, 1, 5, 0, 15));
  DevAssert(test_other_sib_sched_occasion(2, 10, 1, 20, 0, 5, 0, 15));
  // A window beginning in SFN 1023 remains schedulable after the SFN wrap.
  DevAssert(!test_other_sib_sched_occasion(32, 10, 1, 20, 0, 10, 1, 0));
  DevAssert(test_other_sib_sched_occasion(32, 10, 1, 20, 0, 11, 1, 0));
  return 0;
}
