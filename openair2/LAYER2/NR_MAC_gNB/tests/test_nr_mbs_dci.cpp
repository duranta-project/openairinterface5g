/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <tuple>
#include "gtest/gtest.h"

extern "C" int test_dci_4_1_size_vs_1_0(int cset0_bwp_size);
extern "C" bool test_dci_4_1_pack(int cset0_bwp_size);

// parameter: size of CORESET 0 in RBs (TS 38.213 Tables 13-1 to 13-10)
class DciFormat41 : public testing::TestWithParam<int> {};

// TS 38.212 §7.3.1.5.2: same size as DCI format 1_0 in a CSS, the UE monitors both without size alignment
TEST_P(DciFormat41, SameSizeAsFormat10InCss)
{
  EXPECT_EQ(test_dci_4_1_size_vs_1_0(GetParam()), 0);
}

TEST_P(DciFormat41, FieldPositions)
{
  EXPECT_TRUE(test_dci_4_1_pack(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(Coreset0Size, DciFormat41, testing::Values(24, 48, 96), testing::PrintToStringParamName());

extern "C" bool test_dci_4_2_pack(int cfr_size, int tda_count, int k1_count);

class DciFormat42 : public testing::TestWithParam<std::tuple<int, int, int>> {};

TEST_P(DciFormat42, FieldPositionsAndUeExtraction)
{
  EXPECT_TRUE(test_dci_4_2_pack(std::get<0>(GetParam()), std::get<1>(GetParam()), std::get<2>(GetParam())));
}

INSTANTIATE_TEST_SUITE_P(CfrTdaAndK1, DciFormat42,
                        testing::Combine(testing::Values(24, 48, 106, 275), testing::Values(1, 3, 16), testing::Values(1, 5, 8)));
