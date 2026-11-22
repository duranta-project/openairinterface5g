/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* shared by test_nr_dl_dmrs_ports.c (MAC side, C) and test_nr_dl_dmrs_ports.cpp (GoogleTest) */

#ifndef TEST_NR_DL_DMRS_PORTS_H
#define TEST_NR_DL_DMRS_PORTS_H

#include <stdbool.h>

typedef struct {
  int layers;
  int ports; // bitmap, bit i = port 1000+i
  int cdm_grps_no_data;
  int n_scid;
  int front_load_symb; // 0: 1
  int initial_front_load_symb; // front-loaded symbols of the DMRS before the call, 0: 1
  bool max_length_2; // dmrs-DownlinkForPDSCH-MappingTypeA maxLength len2
  bool dmrs_type_2;
  bool dci_1_0;
  bool scrambling_ids; // scramblingID0/1 configured (else the PCI is used)
} dl_dmrs_ports_in_t;

typedef struct {
  bool ok; // set_dl_dmrs_ports() return value
  int dmrs_ports_id;
  int dmrs_ports;
  int cdm_grps_no_data;
  int n_prb_dmrs;
  int n_scid;
  int scrambling_id;
  int front_load_symb;
  int dmrs_symb_pos; // bitmap of DMRS symbols in the slot
  int n_dmrs_slot; // number of DMRS symbols in the slot
} dl_dmrs_ports_out_t;

// values of the DMRS before set_dl_dmrs_ports(), to check it leaves them unchanged on failure
#define TEST_DL_DMRS_UNSET 0x5a
#define TEST_DL_DMRS_PCI 17
#define TEST_DL_DMRS_SCRAMBLING_ID0 101
#define TEST_DL_DMRS_SCRAMBLING_ID1 202

#ifdef __cplusplus
extern "C" {
#endif
dl_dmrs_ports_out_t test_set_dl_dmrs_ports(dl_dmrs_ports_in_t in);
#ifdef __cplusplus
}
#endif

#endif
