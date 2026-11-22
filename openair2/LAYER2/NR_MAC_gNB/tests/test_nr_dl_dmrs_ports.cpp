/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <string>
#include "gtest/gtest.h"
#include "test_nr_dl_dmrs_ports.h"

// TS 38.212 Table 7.3.1.2.2-1 (antenna ports, dmrs-Type 1, maxLength 1): value -> CDM groups without data, DMRS ports
struct AntPortsValue {
  int value;
  int cdm_grps_no_data;
  int ports;
};
static const AntPortsValue table_7_3_1_2_2_1[] = {
    {0, 1, 0x1}, {1, 1, 0x2}, {2, 1, 0x3}, {3, 2, 0x1}, {4, 2, 0x2}, {5, 2, 0x4},
    {6, 2, 0x8}, {7, 2, 0x3}, {8, 2, 0xc}, {9, 2, 0x7}, {10, 2, 0xf}, {11, 2, 0x5},
};

static dl_dmrs_ports_in_t dci_1_1(int ports, int cdm_grps_no_data, int n_scid = 0)
{
  dl_dmrs_ports_in_t in = {};
  in.layers = __builtin_popcount(ports);
  in.ports = ports;
  in.cdm_grps_no_data = cdm_grps_no_data;
  in.n_scid = n_scid;
  return in;
}

static void expect_unchanged(const dl_dmrs_ports_out_t &out)
{
  EXPECT_FALSE(out.ok);
  EXPECT_EQ(out.dmrs_ports_id, TEST_DL_DMRS_UNSET);
  EXPECT_EQ(out.dmrs_ports, TEST_DL_DMRS_UNSET);
  EXPECT_EQ(out.cdm_grps_no_data, TEST_DL_DMRS_UNSET);
  EXPECT_EQ(out.n_prb_dmrs, TEST_DL_DMRS_UNSET);
  EXPECT_EQ(out.n_scid, TEST_DL_DMRS_UNSET);
  EXPECT_EQ(out.scrambling_id, TEST_DL_DMRS_UNSET);
}

class DlDmrsPortsTable : public testing::TestWithParam<AntPortsValue> {};

// every value of the table: the DCI antenna-ports value, ports and DMRS overhead
TEST_P(DlDmrsPortsTable, AntennaPortsValue)
{
  const AntPortsValue &v = GetParam();
  dl_dmrs_ports_out_t out = test_set_dl_dmrs_ports(dci_1_1(v.ports, v.cdm_grps_no_data));
  ASSERT_TRUE(out.ok);
  EXPECT_EQ(out.dmrs_ports_id, v.value);
  EXPECT_EQ(out.dmrs_ports, v.ports);
  EXPECT_EQ(out.cdm_grps_no_data, v.cdm_grps_no_data);
  EXPECT_EQ(out.n_prb_dmrs, 6 * v.cdm_grps_no_data); // type 1: 6 DMRS REs per PRB and CDM group
  EXPECT_EQ(out.n_scid, 0);
  EXPECT_EQ(out.scrambling_id, TEST_DL_DMRS_PCI);
}

INSTANTIATE_TEST_SUITE_P(Table_7_3_1_2_2_1,
                         DlDmrsPortsTable,
                         testing::ValuesIn(table_7_3_1_2_2_1),
                         [](const testing::TestParamInfo<AntPortsValue> &info) { return "value" + std::to_string(info.param.value); });

// combinations not in the table
TEST(DlDmrsPorts, NotInTable)
{
  expect_unchanged(test_set_dl_dmrs_ports(dci_1_1(0x4, 1))); // port 2 is in CDM group 1
  expect_unchanged(test_set_dl_dmrs_ports(dci_1_1(0xc, 1)));
  expect_unchanged(test_set_dl_dmrs_ports(dci_1_1(0x6, 2))); // ports 1, 2
  expect_unchanged(test_set_dl_dmrs_ports(dci_1_1(0x9, 2))); // ports 0, 3
  expect_unchanged(test_set_dl_dmrs_ports(dci_1_1(0x10, 2))); // port 4 needs maxLength 2
  expect_unchanged(test_set_dl_dmrs_ports(dci_1_1(0x1, 3)));
}

TEST(DlDmrsPorts, LayersMismatch)
{
  dl_dmrs_ports_in_t in = dci_1_1(0x3, 2);
  in.layers = 1;
  expect_unchanged(test_set_dl_dmrs_ports(in));
}

// only DCI 1_1 with dmrs-Type 1 and maxLength 1 is supported
TEST(DlDmrsPorts, Unsupported)
{
  dl_dmrs_ports_in_t in = dci_1_1(0x1, 2);
  in.dci_1_0 = true;
  expect_unchanged(test_set_dl_dmrs_ports(in));
  in = dci_1_1(0x1, 2);
  in.max_length_2 = true;
  expect_unchanged(test_set_dl_dmrs_ports(in));
  in = dci_1_1(0x1, 2);
  in.dmrs_type_2 = true;
  expect_unchanged(test_set_dl_dmrs_ports(in));
}

// TS 38.211 7.4.1.1.1: N_ID^nSCID is scramblingID0/1 if configured, else the PCI
TEST(DlDmrsPorts, NScid)
{
  dl_dmrs_ports_out_t out = test_set_dl_dmrs_ports(dci_1_1(0x4, 2, 1));
  ASSERT_TRUE(out.ok);
  EXPECT_EQ(out.dmrs_ports_id, 5);
  EXPECT_EQ(out.n_scid, 1);
  EXPECT_EQ(out.scrambling_id, TEST_DL_DMRS_PCI);

  dl_dmrs_ports_in_t in = dci_1_1(0x1, 2, 0);
  in.scrambling_ids = true;
  out = test_set_dl_dmrs_ports(in);
  ASSERT_TRUE(out.ok);
  EXPECT_EQ(out.scrambling_id, TEST_DL_DMRS_SCRAMBLING_ID0);
  in.n_scid = 1;
  out = test_set_dl_dmrs_ports(in);
  ASSERT_TRUE(out.ok);
  EXPECT_EQ(out.n_scid, 1);
  EXPECT_EQ(out.scrambling_id, TEST_DL_DMRS_SCRAMBLING_ID1);

  in.n_scid = 2;
  expect_unchanged(test_set_dl_dmrs_ports(in));
  in.n_scid = -1;
  expect_unchanged(test_set_dl_dmrs_ports(in));
}
