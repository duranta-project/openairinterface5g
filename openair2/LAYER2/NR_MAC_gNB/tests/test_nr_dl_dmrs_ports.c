/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* set_dl_dmrs_ports(): DCI 1_1 PDSCH DMRS ports, nSCID and front-loaded symbols, TS 38.212 Tables 7.3.1.2.2-1/2 */

#include "test_nr_dl_dmrs_ports.h"
#include "LAYER2/NR_MAC_gNB/mac_proto.h"

dl_dmrs_ports_out_t test_set_dl_dmrs_ports(dl_dmrs_ports_in_t in)
{
  long pci = TEST_DL_DMRS_PCI;
  NR_ServingCellConfigCommon_t scc = {.physCellId = &pci, .dmrs_TypeA_Position = NR_ServingCellConfigCommon__dmrs_TypeA_Position_pos2};

  long max_length = NR_DMRS_DownlinkConfig__maxLength_len2;
  long dmrs_type = NR_DMRS_DownlinkConfig__dmrs_Type_type2;
  long id0 = TEST_DL_DMRS_SCRAMBLING_ID0, id1 = TEST_DL_DMRS_SCRAMBLING_ID1;
  long add_pos = NR_DMRS_DownlinkConfig__dmrs_AdditionalPosition_pos1; // as configured by the gNB
  NR_DMRS_DownlinkConfig_t dmrs_config = {
      .dmrs_Type = in.dmrs_type_2 ? &dmrs_type : NULL,
      .dmrs_AdditionalPosition = &add_pos,
      .maxLength = in.max_length_2 ? &max_length : NULL,
      .scramblingID0 = in.scrambling_ids ? &id0 : NULL,
      .scramblingID1 = in.scrambling_ids ? &id1 : NULL,
  };
  NR_SetupRelease_DMRS_DownlinkConfig_t dmrs_setup = {
      .present = NR_SetupRelease_DMRS_DownlinkConfig_PR_setup,
      .choice.setup = &dmrs_config,
  };
  NR_PDSCH_Config_t pdsch_config = {.dmrs_DownlinkForPDSCH_MappingTypeA = &dmrs_setup};
  NR_UE_DL_BWP_t dl_bwp = {
      .pdsch_Config = &pdsch_config,
      .dci_format = in.dci_1_0 ? NR_DL_DCI_FORMAT_1_0 : NR_DL_DCI_FORMAT_1_1,
  };
  NR_tda_info_t tda_info = {.mapping_type = typeA, .startSymbolIndex = 1, .nrOfSymbols = 13};

  NR_pdsch_dmrs_t dmrs = {
      .dmrsConfigType = in.dmrs_type_2 ? NFAPI_NR_DMRS_TYPE2 : NFAPI_NR_DMRS_TYPE1,
      .dmrs_ports_id = TEST_DL_DMRS_UNSET,
      .dmrs_ports = TEST_DL_DMRS_UNSET,
      .numDmrsCdmGrpsNoData = TEST_DL_DMRS_UNSET,
      .N_PRB_DMRS = TEST_DL_DMRS_UNSET,
      .n_scid = TEST_DL_DMRS_UNSET,
      .scrambling_id = TEST_DL_DMRS_UNSET,
      .n_front_load_symb = in.initial_front_load_symb ? in.initial_front_load_symb : 1, // 1 as from get_dl_dmrs_params()
      .dl_dmrs_symb_pos = TEST_DL_DMRS_UNSET,
      .N_DMRS_SLOT = TEST_DL_DMRS_UNSET,
  };
  const int front_load_symb = in.front_load_symb ? in.front_load_symb : 1;
  bool ok = set_dl_dmrs_ports(&dmrs, &scc, &dl_bwp, &tda_info, in.layers, in.ports, in.cdm_grps_no_data, in.n_scid, front_load_symb);
  return (dl_dmrs_ports_out_t){
      .ok = ok,
      .dmrs_ports_id = dmrs.dmrs_ports_id,
      .dmrs_ports = dmrs.dmrs_ports,
      .cdm_grps_no_data = dmrs.numDmrsCdmGrpsNoData,
      .n_prb_dmrs = dmrs.N_PRB_DMRS,
      .n_scid = dmrs.n_scid,
      .scrambling_id = dmrs.scrambling_id,
      .front_load_symb = dmrs.n_front_load_symb,
      .dmrs_symb_pos = dmrs.dl_dmrs_symb_pos,
      .n_dmrs_slot = dmrs.N_DMRS_SLOT,
  };
}
