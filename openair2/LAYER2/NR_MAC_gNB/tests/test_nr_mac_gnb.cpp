/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* gNB MAC unit tests. The MAC headers are not C++ compatible, so each test is a C function
 * (test_*.c) called from a GoogleTest case, below or in a C++ file of the same name (test_*.cpp).
 * The MAC (and RLC) can be initialized only once per process, so each test runs in its own
 * process (gtest_discover_tests). */

#include "gtest/gtest.h"

extern "C" {
#include "common/config/config_load_configmodule.h"
#include "common/utils/assertions.h"

void nr_mac_gnb_test_init(void);
extern configmodule_interface_t *uniqCfg;

// test_nr_ul_mac_pdu.c, fail with AssertFatal()
void test_ul_mac_pdu_long_bsr(void);
void test_ul_mac_pdu_sdu_and_long_truncated_bsr(void);
void test_ul_mac_pdu_long_truncated_bsr_without_buffer_size(void);
void test_ul_mac_pdu_long_bsr_without_bitmap_at_end_of_tb(void);
}

TEST(UlMacPdu, LongBsr)
{
  test_ul_mac_pdu_long_bsr();
}

TEST(UlMacPdu, SduAndLongTruncatedBsr)
{
  test_ul_mac_pdu_sdu_and_long_truncated_bsr();
}

TEST(UlMacPdu, LongTruncatedBsrWithoutBufferSize)
{
  test_ul_mac_pdu_long_truncated_bsr_without_buffer_size();
}

TEST(UlMacPdu, LongBsrWithoutBitmapAtEndOfTb)
{
  test_ul_mac_pdu_long_bsr_without_bitmap_at_end_of_tb();
}

int main(int argc, char **argv)
{
  uniqCfg = load_configmodule(argc, argv, CONFIG_ENABLECMDLINEONLY);
  AssertFatal(uniqCfg != NULL, "configuration module init failed\n");
  nr_mac_gnb_test_init();
  testing::InitGoogleTest(&argc, argv);
  int ret = RUN_ALL_TESTS();
  end_configmodule(uniqCfg);
  return ret;
}
