/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* gNB MAC unit tests. The MAC (and RLC) can be initialized only once per process, so each test
 * runs in its own process (gtest_discover_tests). */

#include "gtest/gtest.h"
#include "test_nr_mac_gnb.h"

// parameter: ssb_PositionsInBurst bitmap, bit i = SSB index i (e.g. 0x2: only SSB index 1)
class OtherSibSsbIndex : public testing::TestWithParam<uint64_t> {};

TEST_P(OtherSibSsbIndex, OtherSiInCoreset0)
{
  EXPECT_GT(test_other_sib_ssb_index(GetParam()), 0);
}

// 0x1: SSB index 0, 0x2: index 1, 0x6: index 1 and 2
INSTANTIATE_TEST_SUITE_P(SsbBitmap, OtherSibSsbIndex, testing::Values(0x1, 0x2, 0x6), testing::PrintToStringParamName());

int main(int argc, char **argv)
{
  nr_mac_gnb_test_init();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
