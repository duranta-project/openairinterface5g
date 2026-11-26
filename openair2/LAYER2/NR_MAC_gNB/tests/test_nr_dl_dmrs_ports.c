/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* set_dl_dmrs_ports(): DCI 1_1 PDSCH DMRS ports and nSCID, TS 38.212 Table 7.3.1.2.2-1.
 * Called from test_nr_mac_gnb.cpp, returns the number of failed checks. */

#include <stdio.h>
#include "LAYER2/NR_MAC_gNB/mac_proto.h"

#define UNSET 0x5a // DMRS fields before the call, to check failures leave them unchanged
#define PCI 17
#define SCRAMBLING_ID0 101
#define SCRAMBLING_ID1 202

static int failures = 0;

#define CHECK(cond, ...)                         \
  do {                                           \
    if (!(cond)) {                               \
      printf("FAIL %s:%d: ", __func__, __LINE__); \
      printf(__VA_ARGS__);                       \
      printf("\n");                              \
      failures++;                                \
    }                                            \
  } while (0)

typedef struct {
  bool dci_1_0;
  bool max_length_2;
  bool dmrs_type_2;
  bool scrambling_ids; // scramblingID0/1 configured, else the PCI is used
} dmrs_config_t;

/* set_dl_dmrs_ports() on a DMRS of type 1 (type 2 if configured) with all other fields UNSET */
static bool run(dmrs_config_t c, int layers, int ports, int cdm_grps_no_data, int n_scid, NR_pdsch_dmrs_t *dmrs)
{
  long pci = PCI;
  NR_ServingCellConfigCommon_t scc = {.physCellId = &pci};
  long max_length = NR_DMRS_DownlinkConfig__maxLength_len2;
  long dmrs_type = NR_DMRS_DownlinkConfig__dmrs_Type_type2;
  long id0 = SCRAMBLING_ID0, id1 = SCRAMBLING_ID1;
  NR_DMRS_DownlinkConfig_t dmrs_config = {
      .dmrs_Type = c.dmrs_type_2 ? &dmrs_type : NULL,
      .maxLength = c.max_length_2 ? &max_length : NULL,
      .scramblingID0 = c.scrambling_ids ? &id0 : NULL,
      .scramblingID1 = c.scrambling_ids ? &id1 : NULL,
  };
  NR_SetupRelease_DMRS_DownlinkConfig_t dmrs_setup = {
      .present = NR_SetupRelease_DMRS_DownlinkConfig_PR_setup,
      .choice.setup = &dmrs_config,
  };
  NR_PDSCH_Config_t pdsch_config = {.dmrs_DownlinkForPDSCH_MappingTypeA = &dmrs_setup};
  NR_UE_DL_BWP_t dl_bwp = {
      .pdsch_Config = &pdsch_config,
      .dci_format = c.dci_1_0 ? NR_DL_DCI_FORMAT_1_0 : NR_DL_DCI_FORMAT_1_1,
  };
  NR_tda_info_t tda_info = {.mapping_type = typeA, .startSymbolIndex = 1, .nrOfSymbols = 13};
  *dmrs = (NR_pdsch_dmrs_t){
      .dmrsConfigType = c.dmrs_type_2 ? NFAPI_NR_DMRS_TYPE2 : NFAPI_NR_DMRS_TYPE1,
      .dmrs_ports_id = UNSET,
      .dmrs_ports = UNSET,
      .numDmrsCdmGrpsNoData = UNSET,
      .N_PRB_DMRS = UNSET,
      .n_scid = UNSET,
      .scrambling_id = UNSET,
  };
  return set_dl_dmrs_ports(dmrs, &scc, &dl_bwp, &tda_info, layers, ports, cdm_grps_no_data, n_scid);
}

static const dmrs_config_t dci_1_1 = {0};

static void check_unchanged(const char *what, bool ok, const NR_pdsch_dmrs_t *d)
{
  CHECK(!ok, "%s: accepted", what);
  CHECK(d->dmrs_ports_id == UNSET && d->dmrs_ports == UNSET && d->numDmrsCdmGrpsNoData == UNSET && d->N_PRB_DMRS == UNSET
            && d->n_scid == UNSET && d->scrambling_id == UNSET,
        "%s: DMRS changed although rejected",
        what);
}

/* every value of TS 38.212 Table 7.3.1.2.2-1 (dmrs-Type 1, maxLength 1): DCI value, ports, DMRS overhead */
static void test_table(void)
{
  static const struct {
    int cdm_grps_no_data;
    int ports;
  } table[] = {
      {1, 0x1}, {1, 0x2}, {1, 0x3}, {2, 0x1}, {2, 0x2}, {2, 0x4}, {2, 0x8}, {2, 0x3}, {2, 0xc}, {2, 0x7}, {2, 0xf}, {2, 0x5},
  };
  for (int v = 0; v < sizeofArray(table); v++) {
    NR_pdsch_dmrs_t d;
    bool ok = run(dci_1_1, __builtin_popcount(table[v].ports), table[v].ports, table[v].cdm_grps_no_data, 0, &d);
    CHECK(ok, "value %d: rejected", v);
    CHECK(d.dmrs_ports_id == v, "value %d: got DCI value %d", v, d.dmrs_ports_id);
    CHECK(d.dmrs_ports == table[v].ports, "value %d: got ports 0x%x", v, d.dmrs_ports);
    CHECK(d.numDmrsCdmGrpsNoData == table[v].cdm_grps_no_data, "value %d: got %d CDM groups", v, d.numDmrsCdmGrpsNoData);
    CHECK(d.N_PRB_DMRS == 6 * table[v].cdm_grps_no_data, "value %d: got N_PRB_DMRS %d", v, d.N_PRB_DMRS);
    CHECK(d.n_scid == 0 && d.scrambling_id == PCI, "value %d: got nSCID %d scrambling ID %u", v, d.n_scid, d.scrambling_id);
  }
}

/* combinations not in the table, and unsupported configurations */
static void test_rejected(void)
{
  NR_pdsch_dmrs_t d;
  check_unchanged("port 2, 1 CDM group", run(dci_1_1, 1, 0x4, 1, 0, &d), &d);
  check_unchanged("ports 2,3, 1 CDM group", run(dci_1_1, 2, 0xc, 1, 0, &d), &d);
  check_unchanged("ports 1,2", run(dci_1_1, 2, 0x6, 2, 0, &d), &d);
  check_unchanged("ports 0,3", run(dci_1_1, 2, 0x9, 2, 0, &d), &d);
  check_unchanged("port 4", run(dci_1_1, 1, 0x10, 2, 0, &d), &d);
  check_unchanged("3 CDM groups", run(dci_1_1, 1, 0x1, 3, 0, &d), &d);
  check_unchanged("2 ports, 1 layer", run(dci_1_1, 1, 0x3, 2, 0, &d), &d);
  check_unchanged("DCI 1_0", run((dmrs_config_t){.dci_1_0 = true}, 1, 0x1, 2, 0, &d), &d);
  check_unchanged("maxLength 2", run((dmrs_config_t){.max_length_2 = true}, 1, 0x1, 2, 0, &d), &d);
  check_unchanged("dmrs-Type 2", run((dmrs_config_t){.dmrs_type_2 = true}, 1, 0x1, 2, 0, &d), &d);
  check_unchanged("nSCID 2", run(dci_1_1, 1, 0x1, 2, 2, &d), &d);
  check_unchanged("nSCID -1", run(dci_1_1, 1, 0x1, 2, -1, &d), &d);
}

/* TS 38.211 7.4.1.1.1: N_ID^nSCID is scramblingID0/1 if configured, else the PCI */
static void test_nscid(void)
{
  NR_pdsch_dmrs_t d;
  bool ok = run(dci_1_1, 1, 0x4, 2, 1, &d);
  CHECK(ok && d.dmrs_ports_id == 5 && d.n_scid == 1 && d.scrambling_id == PCI,
        "port 2 nSCID 1 without scrambling IDs: ok %d value %d nSCID %d scrambling ID %u",
        ok,
        d.dmrs_ports_id,
        d.n_scid,
        d.scrambling_id);
  const dmrs_config_t ids = {.scrambling_ids = true};
  ok = run(ids, 1, 0x1, 2, 0, &d);
  CHECK(ok && d.n_scid == 0 && d.scrambling_id == SCRAMBLING_ID0, "nSCID 0: ok %d scrambling ID %u", ok, d.scrambling_id);
  ok = run(ids, 1, 0x1, 2, 1, &d);
  CHECK(ok && d.n_scid == 1 && d.scrambling_id == SCRAMBLING_ID1, "nSCID 1: ok %d scrambling ID %u", ok, d.scrambling_id);
}

int test_dl_dmrs_ports(void)
{
  failures = 0;
  test_table();
  test_rejected();
  test_nscid();
  return failures;
}
