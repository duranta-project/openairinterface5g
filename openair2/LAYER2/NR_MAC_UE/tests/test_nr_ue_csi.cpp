/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <array>
#include <climits>
#include <cstdint>
#include <cstring>
#include "gtest/gtest.h"

extern "C" {
#include "openair2/LAYER2/NR_MAC_UE/nr_ue_csi.h"
#include "common/utils/LOG/log.h"
}

namespace {

class CsiResourceMapping : public testing::Test {
 protected:
  NR_CSI_RS_ResourceMapping_t mapping = {};
  fapi_nr_dl_config_csirs_pdu_rel15_t pdu = {};
  uint8_t allocation[2] = {0, 0x10};

  void SetUp() override
  {
    mapping.frequencyDomainAllocation.present = NR_CSI_RS_ResourceMapping__frequencyDomainAllocation_PR_row2;
    mapping.frequencyDomainAllocation.choice.row2.buf = allocation;
    mapping.frequencyDomainAllocation.choice.row2.size = sizeof(allocation);
    mapping.frequencyDomainAllocation.choice.row2.bits_unused = 4;
    mapping.nrofPorts = NR_CSI_RS_ResourceMapping__nrofPorts_p1;
    mapping.firstOFDMSymbolInTimeDomain = 4;
    mapping.cdm_Type = NR_CSI_RS_ResourceMapping__cdm_Type_noCDM;
    mapping.density.present = NR_CSI_RS_ResourceMapping__density_PR_one;

    pdu.csi_type = 1;
    pdu.scramb_id = 123;
    pdu.measurement_bitmap = 1;
    pdu.power_control_offset = 8;
    pdu.power_control_offset_ss = 1;
    pdu.symb_l1 = 9;
  }
};

TEST_F(CsiResourceMapping, DerivesBandwidthWithinBwp)
{
  struct TestCase {
    const char *name;
    uint32_t bwp_start;
    uint32_t bwp_size;
    long configured_start;
    long configured_width;
    uint16_t expected_start;
    uint16_t expected_width;
  };
  const TestCase cases[] = {
      {"20 RB BWP", 0, 20, 0, 24, 0, 20},
      {"21 RB BWP", 0, 21, 0, 24, 0, 21},
      {"24 RB boundary", 0, 24, 0, 24, 0, 24},
      {"normal bandwidth", 0, 106, 4, 24, 4, 24},
      {"wide BWP clipping", 0, 50, 0, 52, 0, 50},
      {"nonzero BWP start", 24, 52, 32, 40, 32, 40},
      {"unaligned BWP start", 101, 21, 100, 24, 101, 21},
      // TS 38.214 section 5.2.2.3.1 adjusts the start before limiting the configured width.
      {"start adjustment preserves width", 100, 52, 96, 24, 100, 24},
      {"resource ends at BWP boundary", 0, 52, 28, 24, 28, 24},
      {"maximum configured width", 0, 275, 0, 276, 0, 275},
      {"one RB BWP", 0, 1, 0, 24, 0, 1},
  };

  for (const TestCase &test : cases) {
    SCOPED_TRACE(test.name);
    mapping.freqBand.startingRB = test.configured_start;
    mapping.freqBand.nrofRBs = test.configured_width;
    ASSERT_TRUE(configure_csi_resource_mapping(&pdu, &mapping, test.bwp_size, test.bwp_start));
    EXPECT_EQ(pdu.start_rb, test.expected_start);
    EXPECT_EQ(pdu.nr_of_rbs, test.expected_width);
    EXPECT_EQ(pdu.row, 2);
    EXPECT_EQ(pdu.freq_domain, 1);
    EXPECT_EQ(pdu.symb_l0, 4);
    EXPECT_EQ(pdu.symb_l1, 0);
    EXPECT_EQ(pdu.cdm_type, 0);
    EXPECT_EQ(pdu.freq_density, 2);
    EXPECT_EQ(pdu.csi_type, 1);
    EXPECT_EQ(pdu.scramb_id, 123);
    EXPECT_EQ(pdu.measurement_bitmap, 1);
    EXPECT_EQ(pdu.power_control_offset, 8);
    EXPECT_EQ(pdu.power_control_offset_ss, 1);
  }
}

TEST_F(CsiResourceMapping, RejectsInvalidRangesWithoutChangingPdu)
{
  struct TestCase {
    const char *name;
    uint32_t bwp_start;
    uint32_t bwp_size;
    long configured_start;
    long configured_width;
  };
  const TestCase cases[] = {
      {"partial narrow BWP", 0, 20, 4, 24},
      {"one RB remainder in wide BWP", 0, 25, 24, 24},
      {"resource starts at BWP end", 0, 24, 24, 24},
      {"resource starts beyond BWP end", 0, 24, 28, 24},
      {"empty BWP", 0, 0, 0, 24},
      {"oversized BWP", 0, 276, 0, 24},
      {"BWP starts beyond grid", 275, 1, 272, 24},
      {"BWP ends beyond grid", 252, 24, 252, 24},
      {"BWP start overflow", UINT32_MAX, 24, 0, 24},
      {"BWP size overflow", 4, UINT32_MAX, 4, 24},
      {"negative configured start", 0, 52, -4, 24},
      {"negative configured width", 0, 52, 0, -24},
      {"empty configured resource", 0, 52, 0, 0},
      {"configured width below ASN.1 minimum", 0, 20, 0, 20},
      {"unaligned configured start", 0, 52, 2, 24},
      {"unaligned configured width", 0, 52, 0, 25},
      {"configured start beyond grid", 0, 52, 276, 24},
      {"configured start overflow", 0, 52, LONG_MAX, 24},
      {"oversized configured width", 0, 52, 0, 280},
      {"configured width overflow", 0, 52, 0, LONG_MAX},
  };

  for (const TestCase &test : cases) {
    SCOPED_TRACE(test.name);
    mapping.freqBand.startingRB = test.configured_start;
    mapping.freqBand.nrofRBs = test.configured_width;
    std::memset(&pdu, 0x5a, sizeof(pdu));
    std::array<unsigned char, sizeof(pdu)> before;
    std::memcpy(before.data(), &pdu, sizeof(pdu));
    EXPECT_FALSE(configure_csi_resource_mapping(&pdu, &mapping, test.bwp_size, test.bwp_start));
    EXPECT_EQ(std::memcmp(before.data(), &pdu, sizeof(pdu)), 0);
  }
}

TEST_F(CsiResourceMapping, RejectsEmptyFrequencyAllocationWithoutChangingPdu)
{
  mapping.freqBand.startingRB = 0;
  mapping.freqBand.nrofRBs = 24;
  allocation[1] = 0;
  std::memset(&pdu, 0x5a, sizeof(pdu));
  std::array<unsigned char, sizeof(pdu)> before;
  std::memcpy(before.data(), &pdu, sizeof(pdu));

  EXPECT_FALSE(configure_csi_resource_mapping(&pdu, &mapping, 20, 0));
  EXPECT_EQ(std::memcmp(before.data(), &pdu, sizeof(pdu)), 0);
}

TEST_F(CsiResourceMapping, SupportsClippedNarrowBwpRateMatching)
{
  mapping.freqBand.startingRB = 0;
  mapping.freqBand.nrofRBs = 24;

  for (const uint16_t bwp_size : {1, 20, 21, 24}) {
    SCOPED_TRACE(bwp_size);
    fapi_nr_dl_config_dlsch_pdu_rel15_t dlsch = {};
    dlsch.BWPSize = bwp_size;
    dlsch.numCsiRsForRateMatching = 1;
    ASSERT_TRUE(configure_csi_resource_mapping(&dlsch.csiRsForRateMatching[0], &mapping, bwp_size, 0));
    EXPECT_TRUE(nr_ue_csi_ratematching_is_supported(&dlsch));
  }
}

TEST(CsiRateMatching, ChecksCoverageOfTheDlschBwp)
{
  struct TestCase {
    const char *name;
    uint16_t start_rb;
    uint16_t nr_of_rbs;
    bool supported;
  };
  const TestCase cases[] = {
      {"whole BWP", 12, 24, true},
      {"wider CSI range", 0, 52, true},
      {"earlier CSI start with matching end", 0, 36, true},
      {"matching start with later CSI end", 12, 40, true},
      {"missing first RB", 13, 24, false},
      {"missing last RB", 12, 23, false},
      {"disjoint CSI range", 36, 24, false},
      {"empty CSI range", 12, 0, false},
  };

  for (const TestCase &test : cases) {
    SCOPED_TRACE(test.name);
    fapi_nr_dl_config_dlsch_pdu_rel15_t dlsch = {};
    dlsch.BWPStart = 12;
    dlsch.BWPSize = 24;
    dlsch.numCsiRsForRateMatching = 1;
    dlsch.csiRsForRateMatching[0].start_rb = test.start_rb;
    dlsch.csiRsForRateMatching[0].nr_of_rbs = test.nr_of_rbs;
    EXPECT_EQ(nr_ue_csi_ratematching_is_supported(&dlsch), test.supported);
  }
}

TEST(CsiRateMatching, RejectsAnyUnsupportedResource)
{
  fapi_nr_dl_config_dlsch_pdu_rel15_t dlsch = {};
  dlsch.BWPSize = 52;
  dlsch.numCsiRsForRateMatching = 3;
  for (int i = 0; i < dlsch.numCsiRsForRateMatching; i++)
    dlsch.csiRsForRateMatching[i].nr_of_rbs = 52;
  ASSERT_TRUE(nr_ue_csi_ratematching_is_supported(&dlsch));

  for (int i = 0; i < dlsch.numCsiRsForRateMatching; i++) {
    SCOPED_TRACE(i);
    dlsch.csiRsForRateMatching[i].nr_of_rbs = 24;
    EXPECT_FALSE(nr_ue_csi_ratematching_is_supported(&dlsch));
    dlsch.csiRsForRateMatching[i].nr_of_rbs = 52;
  }
}

TEST(CsiRateMatching, AcceptsNoResources)
{
  fapi_nr_dl_config_dlsch_pdu_rel15_t dlsch = {};
  dlsch.BWPSize = 52;
  EXPECT_TRUE(nr_ue_csi_ratematching_is_supported(&dlsch));
}

} // namespace

int main(int argc, char **argv)
{
  logInit();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
