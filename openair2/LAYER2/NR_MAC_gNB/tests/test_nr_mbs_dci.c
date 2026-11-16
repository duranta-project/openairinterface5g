/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* DCI format 4_1 (TS 38.212 §7.3.1.5.2) scheduling multicast PDSCH with CRC scrambled by G-RNTI */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "common/utils/bits.h"
#include "LAYER2/NR_MAC_gNB/mac_proto.h"
#include "LAYER2/NR_MAC_COMMON/nr_mac_common.h"

static NR_SearchSpace_t *common_search_space(void)
{
  static struct NR_SearchSpace__searchSpaceType type = {.present = NR_SearchSpace__searchSpaceType_PR_common};
  static NR_SearchSpace_t ss = {.searchSpaceType = &type};
  return &ss;
}

// returns the size of DCI format 4_1 minus the size of DCI format 1_0 with C-RNTI in a CSS
int test_dci_4_1_size_vs_1_0(int cset0_bwp_size)
{
  dci_pdu_rel15_t dci_41 = {0};
  dci_pdu_rel15_t dci_10 = {0};
  const int ss_type = NR_SearchSpace__searchSpaceType_PR_common;
  int size_41 = nr_dci_size(NULL, NULL, NULL, 0, &dci_41, NR_DL_DCI_FORMAT_4_1, TYPE_G_RNTI_, NULL, ss_type, cset0_bwp_size, 0);
  int size_10 = nr_dci_size(NULL, NULL, NULL, 0, &dci_10, NR_DL_DCI_FORMAT_1_0, TYPE_C_RNTI_, NULL, ss_type, cset0_bwp_size, 0);
  return size_41 - size_10;
}

static uint32_t read_field(uint64_t payload, int size, int *pos, int nbits)
{
  uint32_t val = (payload >> (size - *pos - nbits)) & ((1ULL << nbits) - 1);
  *pos += nbits;
  return val;
}

// packs a DCI format 4_1 and reads back every field at the position given by TS 38.212 §7.3.1.5.2
bool test_dci_4_1_pack(int cset0_bwp_size)
{
  dci_pdu_rel15_t dci = {0};
  const int fda_bits = ceil_log2_u32((cset0_bwp_size * (cset0_bwp_size + 1)) >> 1);
  dci.frequency_domain_assignment.val = ((1U << fda_bits) - 1) & 0x2a5a5;
  dci.time_domain_assignment.val = 0x9;
  dci.vrb_to_prb_mapping.val = 1;
  dci.mcs = 0x13;
  dci.ndi = 1;
  dci.rv = 0x2;
  dci.harq_pid.val = 0xb;
  dci.dai[0].val = 0x1;
  dci.pucch_resource_indicator = 0x5;
  dci.pdsch_to_harq_feedback_timing_indicator.val = 0x6;

  nfapi_nr_dl_dci_pdu_t pdu = {0};
  fill_dci_pdu_rel15(NULL,
                     NULL,
                     NULL,
                     &pdu,
                     &dci,
                     NR_DL_DCI_FORMAT_4_1,
                     TYPE_G_RNTI_,
                     0,
                     common_search_space(),
                     NULL,
                     0,
                     cset0_bwp_size);

  const int size = pdu.PayloadSizeBits;
  if (size != fda_bits + 28)
    return false;
  uint64_t payload;
  memcpy(&payload, pdu.Payload, sizeof(payload));
  int pos = 0;
  bool ok = read_field(payload, size, &pos, fda_bits) == dci.frequency_domain_assignment.val;
  ok &= read_field(payload, size, &pos, 4) == dci.time_domain_assignment.val;
  ok &= read_field(payload, size, &pos, 1) == dci.vrb_to_prb_mapping.val;
  ok &= read_field(payload, size, &pos, 5) == dci.mcs;
  ok &= read_field(payload, size, &pos, 1) == dci.ndi;
  ok &= read_field(payload, size, &pos, 2) == dci.rv;
  ok &= read_field(payload, size, &pos, 4) == dci.harq_pid.val;
  ok &= read_field(payload, size, &pos, 2) == dci.dai[0].val;
  ok &= read_field(payload, size, &pos, 3) == dci.pucch_resource_indicator;
  ok &= read_field(payload, size, &pos, 3) == dci.pdsch_to_harq_feedback_timing_indicator.val;
  ok &= read_field(payload, size, &pos, NR_DCI_FORMAT_4_1_RESERVED_BITS) == 0;
  ok &= (payload >> size) == 0;
  return ok;
}

/* TS 38.212 §7.3.1.5.3: check the production packer and UE extractor against the specified field order. */
bool test_dci_4_2_pack(int cfr_size, int tda_count, int k1_count)
{
  NR_PDSCH_Config_t p = {0};
  p.resourceAllocation = NR_PDSCH_Config__resourceAllocation_resourceAllocationType1;
  p.prb_BundlingType.present = NR_PDSCH_Config__prb_BundlingType_PR_staticBundling;
  p.pdsch_TimeDomainAllocationList = calloc(1, sizeof(*p.pdsch_TimeDomainAllocationList));
  p.pdsch_TimeDomainAllocationList->present = NR_SetupRelease_PDSCH_TimeDomainResourceAllocationList_PR_setup;
  p.pdsch_TimeDomainAllocationList->choice.setup = calloc(1, sizeof(*p.pdsch_TimeDomainAllocationList->choice.setup));
  p.pdsch_TimeDomainAllocationList->choice.setup->list.count = tda_count;
  NR_PUCCH_Config_t u = {0};
  u.dl_DataToUL_ACK = calloc(1, sizeof(*u.dl_DataToUL_ACK));
  u.dl_DataToUL_ACK->list.count = k1_count;
  NR_UE_DL_BWP_t dl = {.BWPSize = cfr_size, .pdsch_Config = &p};
  NR_UE_UL_BWP_t ul = {.BWPSize = cfr_size, .pucch_Config = &u};
  NR_UE_ServingCell_Info_t sc = {0};
  NR_ControlResourceSet_t coreset = {0};
  const int dynamic = NR_PhysicalCellGroupConfig__pdsch_HARQ_ACK_Codebook_dynamic;
  const int fda_bits = ceil_log2_u32(cfr_size * (cfr_size + 1) / 2);
  const int tda_bits = ceil_log2_u32(tda_count);
  const int k1_bits = ceil_log2_u32(k1_count);
  const int expected_size = fda_bits + tda_bits + k1_bits + 22;
  dci_pdu_rel15_t dci = {0};
  dci.frequency_domain_assignment.val = ((1U << fda_bits) - 1) & 0x125;
  dci.time_domain_assignment.val = tda_count - 1;
  dci.mcs = 19;
  dci.ndi = 1;
  dci.rv = 2;
  dci.harq_pid.val = 13;
  dci.dai[0].val = 2;
  dci.pucch_resource_indicator = 5;
  dci.pdsch_to_harq_feedback_timing_indicator.val = k1_count - 1;
  dci.antenna_ports.val = 3;
  dci.dmrs_sequence_initialization.val = 1;
  nfapi_nr_dl_dci_pdu_t pdu = {0};
  /* CORESET 0 deliberately differs from the CFR: DCI 4_2 addresses the CFR directly, without K scaling. */
  fill_dci_pdu_rel15(&sc, &dl, &ul, &pdu, &dci, NR_DL_DCI_FORMAT_4_2, TYPE_G_RNTI_, 0,
                     common_search_space(), &coreset, dynamic, 24);
  uint64_t bits;
  memcpy(&bits, pdu.Payload, sizeof(bits));
  int pos = 0;
  bool ok = pdu.PayloadSizeBits == expected_size;
  ok &= read_field(bits, expected_size, &pos, fda_bits) == dci.frequency_domain_assignment.val;
  ok &= read_field(bits, expected_size, &pos, tda_bits) == tda_count - 1;
  ok &= read_field(bits, expected_size, &pos, 5) == 19;
  ok &= read_field(bits, expected_size, &pos, 1) == 1;
  ok &= read_field(bits, expected_size, &pos, 2) == 2;
  ok &= read_field(bits, expected_size, &pos, 4) == 13;
  ok &= read_field(bits, expected_size, &pos, 2) == 2;
  ok &= read_field(bits, expected_size, &pos, 3) == 5;
  ok &= read_field(bits, expected_size, &pos, k1_bits) == k1_count - 1;
  ok &= read_field(bits, expected_size, &pos, 4) == 3;
  ok &= read_field(bits, expected_size, &pos, 1) == 1;
  ok &= pos == expected_size && (bits >> expected_size) == 0;
  dci_pdu_rel15_t rx = {0};
  ok &= nr_dci_size(&dl, &ul, &sc, dynamic, &rx, NR_DL_DCI_FORMAT_4_2, TYPE_G_RNTI_,
                    &coreset, NR_SearchSpace__searchSpaceType_PR_common, 24, 0) == expected_size;
  ok &= nr_dci_4_2_unpack(&rx, pdu.Payload, expected_size);
  ok &= rx.frequency_domain_assignment.val == dci.frequency_domain_assignment.val
        && rx.time_domain_assignment.val == dci.time_domain_assignment.val && rx.mcs == dci.mcs && rx.ndi == dci.ndi
        && rx.rv == dci.rv && rx.harq_pid.val == dci.harq_pid.val && rx.dai[0].val == dci.dai[0].val
        && rx.pucch_resource_indicator == dci.pucch_resource_indicator
        && rx.pdsch_to_harq_feedback_timing_indicator.val == dci.pdsch_to_harq_feedback_timing_indicator.val
        && rx.antenna_ports.val == 3 && rx.dmrs_sequence_initialization.val == 1;
  /* Fields not present in the profile cannot be silently added to a configured payload. */
  p.resourceAllocation = NR_PDSCH_Config__resourceAllocation_resourceAllocationType0;
  ok &= nr_dci_size(&dl, &ul, &sc, dynamic, &rx, NR_DL_DCI_FORMAT_4_2, TYPE_G_RNTI_,
                    &coreset, NR_SearchSpace__searchSpaceType_PR_common, 24, 0) == 0;
  p.resourceAllocation = NR_PDSCH_Config__resourceAllocation_resourceAllocationType1;
  long two_codewords = 1;
  p.maxNrofCodeWordsScheduledByDCI = &two_codewords;
  ok &= nr_dci_size(&dl, &ul, &sc, dynamic, &rx, NR_DL_DCI_FORMAT_4_2, TYPE_G_RNTI_,
                    &coreset, NR_SearchSpace__searchSpaceType_PR_common, 24, 0) == 0;
  p.maxNrofCodeWordsScheduledByDCI = NULL;
  p.ext3 = calloc(1, sizeof(*p.ext3));
  long configured_size = expected_size;
  p.ext3->sizeDCI_4_2_r17 = &configured_size;
  ok &= nr_dci_size(&dl, &ul, &sc, dynamic, &rx, NR_DL_DCI_FORMAT_4_2, TYPE_G_RNTI_,
                    &coreset, NR_SearchSpace__searchSpaceType_PR_common, 24, 0) == expected_size;
  configured_size++;
  ok &= nr_dci_size(&dl, &ul, &sc, dynamic, &rx, NR_DL_DCI_FORMAT_4_2, TYPE_G_RNTI_,
                    &coreset, NR_SearchSpace__searchSpaceType_PR_common, 24, 0) == 0;
  free(p.ext3);
  dci.antenna_ports.val = 16;
  ok &= !nr_dci_4_2_pack(&dci, pdu.Payload, expected_size);
  ok &= !nr_dci_4_2_unpack(&rx, pdu.Payload, 65);
  free(p.pdsch_TimeDomainAllocationList->choice.setup);
  free(p.pdsch_TimeDomainAllocationList);
  free(u.dl_DataToUL_ACK);
  return ok;
}
