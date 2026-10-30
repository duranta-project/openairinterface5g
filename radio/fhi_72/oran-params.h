/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef ORAN_PARAMS_H
#define ORAN_PARAMS_H

#include "stdbool.h"
#include "stdint.h"
#include "common/config/config_userapi.h"

#define CONFIG_STRING_ORAN "fhi_72"

#define ORAN_CONFIG_DPDK_DEVICES "dpdk_devices"
#define ORAN_CONFIG_SYSTEM_CORE "system_core"
#define ORAN_CONFIG_IO_CORE "io_core"
#define ORAN_CONFIG_WORKER_CORES "worker_cores"
#define ORAN_CONFIG_DU_KEYPAIR "du_key_pair" // only needed for M-plane
#define ORAN_CONFIG_DU_ADDR "du_addr" // needed for M-plane or O-RU
#define ORAN_CONFIG_VLAN_TAG "vlan_tag" // only needed for M-plane
#define ORAN_CONFIG_RU_ADDR "ru_addr" // not needed if M-plane used
#define ORAN_CONFIG_RU_USERNAME "ru_username" // only needed for M-plane
#define ORAN_CONFIG_RU_IP_ADDR "ru_ip_addr" // only needed for M-plane
#define ORAN_CONFIG_MTU "mtu" // not needed if M-plane used
#define ORAN_CONFIG_FILE_PREFIX "file_prefix"
#define ORAN_CONFIG_NETHPERPORT "eth_lines"
#define ORAN_CONFIG_NETHSPEED "eth_speed"
#define ORAN_CONFIG_DPDK_MEM_SIZE "dpdk_mem_size"
#define ORAN_CONFIG_DPDK_IOVA_MODE "dpdk_iova_mode"
#define ORAN_CONFIG_ECPRI_OWDM "owdm_enable"
#define ORAN_CONFIG_APP_ID "app_id"

#define APP_ID_O_DU "DU"
#define APP_ID_O_RU "RU"

// Provide a definition since xran is missing one
#define XRAN_APP_ID_O_DU 0
#define XRAN_APP_ID_O_RU 1

// clang-format off
// TODO: PCI addr check
// TODO: ethernet addr check
#define ORAN_GLOBALPARAMS_DESC { \
  {ORAN_CONFIG_DPDK_DEVICES,    "PCI addr of devices for DPDK\n",           PARAMFLAG_MANDATORY, .strlistptr=NULL, .defstrlistval=NULL, TYPE_STRINGLIST, 0}, \
  {ORAN_CONFIG_SYSTEM_CORE,     "DPDK control threads core\n",              PARAMFLAG_MANDATORY, .iptr=NULL,       .defintval=0,        TYPE_INT,        0}, \
  {ORAN_CONFIG_IO_CORE,         "DPDK Core used for IO\n",                  PARAMFLAG_MANDATORY, .iptr=NULL,       .defintval=4,        TYPE_INT,        0}, \
  {ORAN_CONFIG_WORKER_CORES,    "CPU Cores to use for workers\n",           PARAMFLAG_MANDATORY, .uptr=NULL,       .defintarrayval=NULL,TYPE_UINTARRAY,  0}, \
  {ORAN_CONFIG_DU_KEYPAIR,      "DU keypair for RU authentication\n",       PARAMFLAG_MANDATORY, .strlistptr=NULL, .defstrlistval=NULL, TYPE_STRINGLIST, 0}, \
  {ORAN_CONFIG_DU_ADDR,         "Ether addr of DU\n",                       PARAMFLAG_MANDATORY, .strlistptr=NULL, .defstrlistval=NULL, TYPE_STRINGLIST, 0}, \
  {ORAN_CONFIG_VLAN_TAG,        "VLAN tag\n",                               PARAMFLAG_MANDATORY, .iptr=NULL,       .defintarrayval=0,   TYPE_INTARRAY,   0}, \
  {ORAN_CONFIG_RU_ADDR,         "Ether addr of RU\n",                       PARAMFLAG_MANDATORY, .strlistptr=NULL, .defstrlistval=NULL, TYPE_STRINGLIST, 0}, \
  {ORAN_CONFIG_RU_USERNAME,     "Username of RU\n",                         PARAMFLAG_MANDATORY, .strlistptr=NULL, .defstrlistval=NULL, TYPE_STRINGLIST, 0}, \
  {ORAN_CONFIG_RU_IP_ADDR,      "IP addr of RU\n",                          PARAMFLAG_MANDATORY, .strlistptr=NULL, .defstrlistval=NULL, TYPE_STRINGLIST, 0}, \
  {ORAN_CONFIG_MTU,             "MTU of Eth interface\n",                   0,                   .uptr=NULL,       .defuintval=1500,    TYPE_UINT,       0}, \
  {ORAN_CONFIG_FILE_PREFIX,     "DPDK file-prefix\n",                       0,                   .strptr=NULL,     .defstrval="wls_0",  TYPE_STRING,     0}, \
  {ORAN_CONFIG_NETHPERPORT,     "number of links per port\n",               0,                   .uptr=NULL,       .defuintval=1,       TYPE_UINT,       0}, \
  {ORAN_CONFIG_NETHSPEED,       "ethernet speed link\n",                    0,                   .uptr=NULL,       .defuintval=10,      TYPE_UINT,       0}, \
  {ORAN_CONFIG_DPDK_MEM_SIZE,   "DPDK huge page pre-allocation in MiB\n",   0,                   .uptr=NULL,       .defuintval=8192,    TYPE_UINT,       0}, \
  {ORAN_CONFIG_DPDK_IOVA_MODE,  "DPDK IOVA mode\n",                         0,                   .strptr=NULL,     .defstrval="PA",     TYPE_STRING,     0}, \
  {ORAN_CONFIG_ECPRI_OWDM,      "eCPRI One-Way Delay Measurements\n",       PARAMFLAG_BOOL,      .uptr=NULL,       .defuintval=0,       TYPE_UINT,       0}, \
  {ORAN_CONFIG_APP_ID,          "Either DU or RU",                          0,                   .strptr=NULL,     .defstrval=APP_ID_O_DU, TYPE_STRING,  0}  \
}

// clang-format off
#define ORAN_GLOBALPARAMS_CHECK_DESC {           \
    { .s5 = { NULL } },                          \
    { .s5 = { NULL } },                          \
    { .s5 = { NULL } },                          \
    { .s5 = { NULL } },                          \
    { .s5 = { NULL } },                          \
    { .s5 = { NULL } },                          \
    { .s5 = { NULL } },                          \
    { .s5 = { NULL } },                          \
    { .s5 = { NULL } },                          \
    { .s5 = { NULL } },                          \
    { .s5 = { NULL } },                          \
    { .s5 = { NULL } },                          \
    { .s5 = { NULL } },                          \
    { .s5 = { NULL } },                          \
    { .s5 = { NULL } },                          \
    { .s3a = { config_checkstr_assign_integer,   \
	       {"PA", "VA"}, {0, 1}, 2} },           \
    { .s5 = { NULL } },                          \
    { .s3a = { config_checkstr_assign_integer,   \
	       {APP_ID_O_DU, APP_ID_O_RU}, {XRAN_APP_ID_O_DU, XRAN_APP_ID_O_RU}, 2} },             \
}


// clang-format on

#define CONFIG_STRING_ORAN_FH "fh_config"

#define ORAN_CONFIG_CP_MULTISECTION "RunSlotPrbMapBySymbol"
#define ORAN_FH_CONFIG_T1A_CP_DL "T1a_cp_dl"
#define ORAN_FH_CONFIG_T1A_CP_UL "T1a_cp_ul"
#define ORAN_FH_CONFIG_T1A_UP "T1a_up"
#define ORAN_FH_CONFIG_TA4 "Ta4"

#define ORAN_FH_HLP_CPLT " parameter of RU in list form (Min&Max, length 2!)\n"

// clang-format off
#define ORAN_FH_DESC { \
  {ORAN_CONFIG_CP_MULTISECTION,         "RunSlotPrbMapBySymbol\n",     PARAMFLAG_BOOL,      .iptr=NULL, .defintval=0,      TYPE_INT32,     0}, \
  {ORAN_FH_CONFIG_T1A_CP_DL,            "T1a_cp_dl" ORAN_FH_HLP_CPLT,  PARAMFLAG_MANDATORY, .uptr=NULL, .defintarrayval=0, TYPE_UINTARRAY, 0}, \
  {ORAN_FH_CONFIG_T1A_CP_UL,            "T1a_cp_ul" ORAN_FH_HLP_CPLT,  PARAMFLAG_MANDATORY, .uptr=NULL, .defintarrayval=0, TYPE_UINTARRAY, 0}, \
  {ORAN_FH_CONFIG_T1A_UP,               "T1a_up" ORAN_FH_HLP_CPLT,     PARAMFLAG_MANDATORY, .uptr=NULL, .defintarrayval=0, TYPE_UINTARRAY, 0}, \
  {ORAN_FH_CONFIG_TA4,                  "Ta4" ORAN_FH_HLP_CPLT,        PARAMFLAG_MANDATORY, .uptr=NULL, .defintarrayval=0, TYPE_UINTARRAY, 0}, \
}
// clang-format on

#define COMP_HDR_TYPE_CHECK                                                     \
  &(checkedparam_t)                                                             \
  {                                                                             \
    .s3a = { config_checkstr_assign_integer, {"dynamic", "static"}, {0, 1}, 2 } \
  }

#define CONFIG_STRING_ORAN_RU "ru_config"

#define ORAN_RU_COMP_HDR_TYPE "comp_hdr_type"
#define ORAN_RU_CONFIG_IQWIDTH "iq_width" // not needed if M-plane used
#define ORAN_RU_CONFIG_IQWIDTH_PRACH "iq_width_prach" // not needed if M-plane used

// clang-format off
#define ORAN_RU_DESC {\
  {ORAN_RU_COMP_HDR_TYPE,        "Compression header type\n",                 0,                   .strptr=NULL, .defstrval="static", TYPE_STRING, 0, COMP_HDR_TYPE_CHECK}, \
  {ORAN_RU_CONFIG_IQWIDTH,       "sample IQ width (16=uncompressed)\n",       PARAMFLAG_MANDATORY, .u8ptr=NULL, .defuintval=16, TYPE_UINT8, 0}, \
  {ORAN_RU_CONFIG_IQWIDTH_PRACH, "PRACH sample IQ width (16=uncompressed)\n", PARAMFLAG_MANDATORY, .u8ptr=NULL, .defuintval=16, TYPE_UINT8, 0}, \
}
// clang-format on

#define CONFIG_STRING_ORAN_PRACH "prach_config"

#define ORAN_PRACH_CONFIG_EAXC_OFFSET "eAxC_offset" // not needed if M-plane used
#define ORAN_PRACH_CONFIG_KBAR "kbar"

// clang-format off
#define ORAN_PRACH_DESC {\
  {ORAN_PRACH_CONFIG_EAXC_OFFSET, "RU's eAxC offset for PRACH\n", 0,                   .u8ptr=NULL, .defuintval=0, TYPE_UINT8, 0}, \
  {ORAN_PRACH_CONFIG_KBAR,        "PRACH sequence offset in the PRACH PRBs, in int16; default: 2 * k_bar of TS 38.211 Table 6.3.3.2-1\n", 0, .uptr=NULL, .type=TYPE_UINT, .numelt=0}, \
}
// clang-format on

#endif /* ORAN_PARAMS_H */
