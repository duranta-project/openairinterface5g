/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <string>
#include <tuple>
#include "gtest/gtest.h"

// returns the number of PMIs for which the gNB decodes the UE report to another precoder than the one of the spec
extern "C" int test_csi_pmi_gnb(int n1, int n2, int rank);
// returns the number of mismatches of the CSI report template for typeI-SinglePanel-ri-Restriction 0b10
extern "C" int test_csi_bitlen_ri_restriction(int n1, int n2);
// returns the number of (ri_restriction, rank) pairs for which nr_ri_from_index() does not invert nr_ri_index()
extern "C" int test_ri_index_round_trip(void);

// parameters: N1, N2, rank
using csi_pmi_gnb_param_t = std::tuple<int, int, int>;

class CsiPmiGnb : public testing::TestWithParam<csi_pmi_gnb_param_t> {};

TEST_P(CsiPmiGnb, ReportDecodesToSpecPrecoder)
{
  const auto [n1, n2, rank] = GetParam();
  EXPECT_EQ(test_csi_pmi_gnb(n1, n2, rank), 0);
}

static std::string csi_pmi_gnb_name(const testing::TestParamInfo<csi_pmi_gnb_param_t> &info)
{
  const auto [n1, n2, rank] = info.param;
  return "N1_" + std::to_string(n1) + "_N2_" + std::to_string(n2) + "_rank_" + std::to_string(rank);
}

// (N1, N2) of TS 38.214 Table 5.2.2.2.1-2 for 4, 8 and 12 ports (the gNB codebook stops below 16 ports)
static const csi_pmi_gnb_param_t csi_pmi_gnb_params[] = {
    {2, 1, 1},
    {2, 1, 2},
    {2, 2, 1},
    {2, 2, 2},
    {4, 1, 1},
    {4, 1, 2},
    {3, 2, 1},
    {3, 2, 2},
    {6, 1, 1},
    {6, 1, 2},
};

INSTANTIATE_TEST_SUITE_P(Type1SinglePanel, CsiPmiGnb, testing::ValuesIn(csi_pmi_gnb_params), csi_pmi_gnb_name);

TEST(CsiPmiGnbRiRestriction, Rank2Only)
{
  EXPECT_EQ(test_csi_bitlen_ri_restriction(2, 1), 0);
  EXPECT_EQ(test_csi_bitlen_ri_restriction(2, 2), 0);
  EXPECT_EQ(test_csi_bitlen_ri_restriction(4, 1), 0);
}

TEST(CsiPmiGnbRiRestriction, RiIndexRoundTrip)
{
  EXPECT_EQ(test_ri_index_round_trip(), 0);
}
