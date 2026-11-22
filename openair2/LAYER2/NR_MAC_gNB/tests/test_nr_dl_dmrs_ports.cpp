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

// TS 38.212 Table 7.3.1.2.2-2 (dmrs-Type 1, maxLength 2, one codeword): value -> CDM groups without data,
// DMRS ports, front-loaded symbols. Values 0..11 are Table 7.3.1.2.2-1.
struct AntPortsValueLen2 {
  int value;
  int cdm_grps_no_data;
  int ports;
  int front_load_symb;
};
static const AntPortsValueLen2 table_7_3_1_2_2_2[] = {
    {0, 1, 0x01, 1},  {1, 1, 0x02, 1},  {2, 1, 0x03, 1},  {3, 2, 0x01, 1},  {4, 2, 0x02, 1},  {5, 2, 0x04, 1},  {6, 2, 0x08, 1},
    {7, 2, 0x03, 1},  {8, 2, 0x0c, 1},  {9, 2, 0x07, 1},  {10, 2, 0x0f, 1}, {11, 2, 0x05, 1}, {12, 2, 0x01, 2}, {13, 2, 0x02, 2},
    {14, 2, 0x04, 2}, {15, 2, 0x08, 2}, {16, 2, 0x10, 2}, {17, 2, 0x20, 2}, {18, 2, 0x40, 2}, {19, 2, 0x80, 2}, {20, 2, 0x03, 2},
    {21, 2, 0x0c, 2}, {22, 2, 0x30, 2}, {23, 2, 0xc0, 2}, {24, 2, 0x11, 2}, {25, 2, 0x44, 2}, {26, 2, 0x13, 2}, {27, 2, 0x4c, 2},
    {28, 2, 0x33, 2}, {29, 2, 0xcc, 2}, {30, 2, 0x55, 2},
};

// type A, DMRS at symbol 2 (dmrs-TypeA-Position pos2), dmrs-AdditionalPosition pos1, PDSCH on symbols 1..13 (TS 38.211
// Tables 7.4.1.1.2-3/4): single-symbol DMRS on symbols 2, 11, double-symbol on 2-3, 10-11
static const int dmrs_symb_pos_len1 = (1 << 2) | (1 << 11);
static const int dmrs_symb_pos_len2 = (1 << 2) | (1 << 3) | (1 << 10) | (1 << 11);

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
  EXPECT_EQ(out.front_load_symb, 1);
  EXPECT_EQ(out.dmrs_symb_pos, TEST_DL_DMRS_UNSET);
  EXPECT_EQ(out.n_dmrs_slot, TEST_DL_DMRS_UNSET);
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
  EXPECT_EQ(out.front_load_symb, 1);
  EXPECT_EQ(out.dmrs_symb_pos, TEST_DL_DMRS_UNSET); // single-symbol DMRS kept: mask unchanged
}

INSTANTIATE_TEST_SUITE_P(Table_7_3_1_2_2_1,
                         DlDmrsPortsTable,
                         testing::ValuesIn(table_7_3_1_2_2_1),
                         [](const testing::TestParamInfo<AntPortsValue> &info) { return "value" + std::to_string(info.param.value); });

class DlDmrsPortsTableLen2 : public testing::TestWithParam<AntPortsValueLen2> {};

// maxLength 2: every value of Table 7.3.1.2.2-2, with the DMRS symbols of single- or double-symbol DMRS
TEST_P(DlDmrsPortsTableLen2, AntennaPortsValue)
{
  const AntPortsValueLen2 &v = GetParam();
  dl_dmrs_ports_in_t in = dci_1_1(v.ports, v.cdm_grps_no_data);
  in.max_length_2 = true;
  in.front_load_symb = v.front_load_symb;
  dl_dmrs_ports_out_t out = test_set_dl_dmrs_ports(in);
  ASSERT_TRUE(out.ok);
  EXPECT_EQ(out.dmrs_ports_id, v.value);
  EXPECT_EQ(out.dmrs_ports, v.ports);
  EXPECT_EQ(out.cdm_grps_no_data, v.cdm_grps_no_data);
  EXPECT_EQ(out.n_prb_dmrs, 6 * v.cdm_grps_no_data);
  EXPECT_EQ(out.front_load_symb, v.front_load_symb);
  if (v.front_load_symb == 2) {
    EXPECT_EQ(out.dmrs_symb_pos, dmrs_symb_pos_len2);
    EXPECT_EQ(out.n_dmrs_slot, 4);
  } else {
    EXPECT_EQ(out.dmrs_symb_pos, TEST_DL_DMRS_UNSET); // mask unchanged
  }
}

INSTANTIATE_TEST_SUITE_P(Table_7_3_1_2_2_2,
                         DlDmrsPortsTableLen2,
                         testing::ValuesIn(table_7_3_1_2_2_2),
                         [](const testing::TestParamInfo<AntPortsValueLen2> &info) {
                           return "value" + std::to_string(info.param.value);
                         });

// back from double- to single-symbol DMRS (e.g. a retx of a single-symbol transmission on a former
// double-symbol DMRS): the DMRS symbols are rebuilt with one symbol per occasion
TEST(DlDmrsPorts, BackToSingleSymbol)
{
  dl_dmrs_ports_in_t in = dci_1_1(0x1, 2);
  in.max_length_2 = true;
  in.initial_front_load_symb = 2;
  in.front_load_symb = 1;
  dl_dmrs_ports_out_t out = test_set_dl_dmrs_ports(in);
  ASSERT_TRUE(out.ok);
  EXPECT_EQ(out.dmrs_ports_id, 3);
  EXPECT_EQ(out.front_load_symb, 1);
  EXPECT_EQ(out.dmrs_symb_pos, dmrs_symb_pos_len1);
  EXPECT_EQ(out.n_dmrs_slot, 2);
}

// double-symbol values need maxLength 2
TEST(DlDmrsPorts, DoubleSymbolNeedsMaxLength2)
{
  dl_dmrs_ports_in_t in = dci_1_1(0x10, 2); // port 4
  in.front_load_symb = 2;
  expect_unchanged(test_set_dl_dmrs_ports(in));
  in = dci_1_1(0x1, 2);
  in.front_load_symb = 2;
  expect_unchanged(test_set_dl_dmrs_ports(in));
}

// combinations not in the table
TEST(DlDmrsPorts, NotInTable)
{
  expect_unchanged(test_set_dl_dmrs_ports(dci_1_1(0x4, 1))); // port 2 is in CDM group 1
  expect_unchanged(test_set_dl_dmrs_ports(dci_1_1(0xc, 1)));
  expect_unchanged(test_set_dl_dmrs_ports(dci_1_1(0x6, 2))); // ports 1, 2
  expect_unchanged(test_set_dl_dmrs_ports(dci_1_1(0x9, 2))); // ports 0, 3
  expect_unchanged(test_set_dl_dmrs_ports(dci_1_1(0x10, 2))); // port 4 needs maxLength 2
  expect_unchanged(test_set_dl_dmrs_ports(dci_1_1(0x1, 3)));
  dl_dmrs_ports_in_t in = dci_1_1(0x10, 2); // port 4 is double-symbol only
  in.max_length_2 = true;
  expect_unchanged(test_set_dl_dmrs_ports(in));
  in = dci_1_1(0x11, 2); // ports 0, 4
  in.max_length_2 = true;
  expect_unchanged(test_set_dl_dmrs_ports(in));
  in = dci_1_1(0x1, 2);
  in.max_length_2 = true;
  in.front_load_symb = 3;
  expect_unchanged(test_set_dl_dmrs_ports(in));
}

TEST(DlDmrsPorts, LayersMismatch)
{
  dl_dmrs_ports_in_t in = dci_1_1(0x3, 2);
  in.layers = 1;
  expect_unchanged(test_set_dl_dmrs_ports(in));
}

// only DCI 1_1 with dmrs-Type 1 is supported
TEST(DlDmrsPorts, Unsupported)
{
  dl_dmrs_ports_in_t in = dci_1_1(0x1, 2);
  in.dci_1_0 = true;
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
