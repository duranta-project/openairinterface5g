/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief definition of configuration parameters for all gNodeB modules
 */

#ifndef __GNB_APP_GNB_PARAMDEF__H__
#define __GNB_APP_GNB_PARAMDEF__H__

#include "common/config/config_paramdesc.h"
#include "common/config/config_userapi.h"
#include "common/ngran_types.h"
#include "RRC_nr_paramsvalues.h"

#ifdef LIBCONFIG_LONG
#define libconfig_int long
#else
#define libconfig_int int
#endif

typedef enum {
	NRRU     = 0,
	NRL1     = 1,
	NRL2     = 2,
	NRL3     = 3,
	NRS1     = 4,
	NRlastel = 5
} NRRC_config_functions_t;

/*------------------------------------------------------------------------------------------------------------------------------------------*/

/*    RUs  configuration for gNB is the same for eNB */
/*    Check file enb_paramdef.h */

/*---------------------------------------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------------------------------------*/
/* value definitions for ASN1 verbosity parameter */
#define GNB_CONFIG_STRING_ASN1_VERBOSITY_NONE              "none"

/* global parameters, not under a specific section   */
#define GNB_CONFIG_STRING_ASN1_VERBOSITY                   "Asn1_verbosity"
#define GNB_CONFIG_STRING_ACTIVE_GNBS                      "Active_gNBs"
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*                                            global configuration parameters                                                                                   */
/*   optname                                   helpstr   paramflags    XXXptr        defXXXval                                        type           numelt     */
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------*/
#define GNBSPARAMS_DESC {                                                                                             \
{GNB_CONFIG_STRING_ASN1_VERBOSITY,             NULL,     0,       .uptr=NULL,   .defstrval=GNB_CONFIG_STRING_ASN1_VERBOSITY_NONE,   TYPE_STRING,      0},   \
{GNB_CONFIG_STRING_ACTIVE_GNBS,                NULL,     0,       .uptr=NULL,   .defstrval=NULL, 				   TYPE_STRINGLIST,  0}    \
}

#define GNB_ASN1_VERBOSITY_IDX                     0
#define GNB_ACTIVE_GNBS_IDX                        1

/*------------------------------------------------------------------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------------------------------------------------------------------*/


/* cell configuration parameters names */
#define GNB_CONFIG_STRING_GNB_ID                        "gNB_ID"
#define GNB_CONFIG_STRING_CELL_TYPE                     "cell_type"
#define GNB_CONFIG_STRING_GNB_NAME                      "gNB_name"
#define GNB_CONFIG_STRING_TRACKING_AREA_CODE            "tracking_area_code"
#define GNB_CONFIG_STRING_MOBILE_COUNTRY_CODE_OLD       "mobile_country_code"
#define GNB_CONFIG_STRING_MOBILE_NETWORK_CODE_OLD       "mobile_network_code"
#define GNB_CONFIG_STRING_TRANSPORT_S_PREFERENCE        "tr_s_preference"
#define GNB_CONFIG_STRING_LOCAL_S_ADDRESS               "local_s_address"
#define GNB_CONFIG_STRING_REMOTE_S_ADDRESS              "remote_s_address"
#define GNB_CONFIG_STRING_LOCAL_S_PORTC                 "local_s_portc"
#define GNB_CONFIG_STRING_REMOTE_S_PORTC                "remote_s_portc"
#define GNB_CONFIG_STRING_LOCAL_S_PORTD                 "local_s_portd"
#define GNB_CONFIG_STRING_REMOTE_S_PORTD                "remote_s_portd"
#define GNB_CONFIG_STRING_PDSCHANTENNAPORTS_N1          "pdsch_AntennaPorts_N1"
#define GNB_CONFIG_STRING_PDSCHANTENNAPORTS_N2          "pdsch_AntennaPorts_N2"
#define GNB_CONFIG_STRING_PDSCHANTENNAPORTS_XP          "pdsch_AntennaPorts_XP"
#define GNB_CONFIG_STRING_PUSCHANTENNAPORTS             "pusch_AntennaPorts"
#define GNB_CONFIG_STRING_DOTCI                         "do_TCI"
#define GNB_CONFIG_STRING_DOCSIRS                       "do_CSIRS"
#define GNB_CONFIG_STRING_DOSRS                         "do_SRS"
#define GNB_CONFIG_STRING_NRCELLID                      "nr_cellid"
#define GNB_CONFIG_STRING_MINRXTXTIME                   "min_rxtxtime"
#define GNB_CONFIG_STRING_ULPRBBLACKLIST                "ul_prbblacklist"
#define GNB_CONFIG_STRING_UMONDEFAULTDRB                "um_on_default_drb"
#define GNB_CONFIG_STRING_FORCE256QAMOFF                "force_256qam_off"
#define GNB_CONFIG_STRING_MAXMIMOLAYERS                 "maxMIMO_layers"
#define GNB_CONFIG_STRING_DISABLE_HARQ                  "disable_harq"
#define GNB_CONFIG_STRING_ENABLE_SDAP                   "enable_sdap"
#define GNB_CONFIG_STRING_USE_DELTA_MCS                 "use_deltaMCS"
#define GNB_CONFIG_HLP_USE_DELTA_MCS                    "Use deltaMCS-based power headroom reporting in PUSCH-Config"
#define GNB_CONFIG_HLP_FORCEUL256QAMOFF                 "suppress activation of UL 256 QAM despite UE support"
#define GNB_CONFIG_STRING_FORCEUL256QAMOFF              "force_UL256qam_off"
#define GNB_CONFIG_STRING_GNB_DU_ID                     "gNB_DU_ID"
#define GNB_CONFIG_STRING_GNB_CU_UP_ID                  "gNB_CU_UP_ID"
#define GNB_CONFIG_STRING_NUM_DL_HARQPROCESSES          "num_dlharq"
#define GNB_CONFIG_STRING_NUM_UL_HARQPROCESSES          "num_ulharq"
#define GNB_CONFIG_STRING_UESS_AGG_LEVEL_LIST           "uess_agg_levels"
#define GNB_CONFIG_STRING_CU_SIB_LIST                   "cu_sibs"
#define GNB_CONFIG_STRING_DU_SIB_LIST                   "du_sibs"
#define GNB_CONFIG_STRING_CONFIG_REP                    "CSI_report_type"
#define GNB_CONFIG_STRING_1ST_ACTIVE_BWP                "first_active_bwp"
#define GNB_CONFIG_STRING_LIMIT_RSRP_REPORT             "max_num_RSRP_reported"

#define GNB_CONFIG_HLP_STRING_ENABLE_SDAP               "set sdap-HeaderUL/DL present in RRC SDAP-Config (false = both absent)\n"
#define GNB_CONFIG_HLP_FORCE256QAMOFF                   "suppress activation of 256 QAM despite UE support"
#define GNB_CONFIG_HLP_MAXMIMOLAYERS                    "limit on maxMIMO-layers for DL"
#define GNB_CONFIG_HLP_DISABLE_HARQ                     "disable feedback for all HARQ processes (REL17 feature)"
#define GNB_CONFIG_HLP_GNB_DU_ID                        "defines the gNB-DU ID (only applicable for DU)"
#define GNB_CONFIG_HLP_GNB_CU_UP_ID                     "defines the gNB-CU-UP ID (only applicable for CU-UP)"
#define GNB_CONFIG_HLP_NUM_DL_HARQ                      "Set Num DL harq processes. Valid values 2,4,6,8,10,12,16,32. Default 16"
#define GNB_CONFIG_HLP_NUM_UL_HARQ                      "Set Num UL harq processes. Valid values 16,32. Default 16"
#define GNB_CONFIG_HLP_UESS_AGG_LEVEL_LIST              "List of aggregation levels with number of candidates per level. Element 0 - aggregation level 1"
#define GNB_CONFIG_HLP_CU_SIBS                          "List of CU generated SIBs to be transmitted"
#define GNB_CONFIG_HLP_DU_SIBS                          "List of DU generated SIBs to be transmitted"
#define GNB_CONFIG_HLP_CONFIG_REP                       "Define quantity for CSI report (options: ssb_rsrp, ssb_sinr and cri_rsrp)"
#define GNB_CONFIG_HLP_DOSRS                            "defines the type of SRS to be scheduled (none, periodic, or aperiodic)"

/*-----------------------------------------------------------------------------------------------------------------------------------------*/
/*                                            cell configuration parameters                                                                */
/*   optname                                   helpstr   paramflags    XXXptr        defXXXval                   type           numelt     */
/*-----------------------------------------------------------------------------------------------------------------------------------------*/
// clang-format off
#define GNBPARAMS_DESC {\
{GNB_CONFIG_STRING_GNB_ID,                       NULL,   0,           .uptr=NULL,   .defintval=0,                 TYPE_UINT,      0},  \
{GNB_CONFIG_STRING_CELL_TYPE,                    NULL,   0,           .strptr=NULL, .defstrval="CELL_MACRO_GNB",  TYPE_STRING,    0},  \
{GNB_CONFIG_STRING_GNB_NAME,                     NULL,   0,           .strptr=NULL, .defstrval="OAIgNodeB",       TYPE_STRING,    0},  \
{GNB_CONFIG_STRING_TRACKING_AREA_CODE,           NULL,   0,           .uptr=NULL,   .defuintval=0,                TYPE_UINT,      0},  \
{GNB_CONFIG_STRING_MOBILE_COUNTRY_CODE_OLD,      NULL,   0,           .strptr=NULL, .defstrval=NULL,              TYPE_STRING,    0},  \
{GNB_CONFIG_STRING_MOBILE_NETWORK_CODE_OLD,      NULL,   0,           .strptr=NULL, .defstrval=NULL,              TYPE_STRING,    0},  \
{GNB_CONFIG_STRING_TRANSPORT_S_PREFERENCE,       NULL,   0,           .strptr=NULL, .defstrval="local_mac",       TYPE_STRING,    0},  \
{GNB_CONFIG_STRING_LOCAL_S_ADDRESS,              NULL,   0,           .strptr=NULL, .defstrval="127.0.0.1",       TYPE_STRING,    0},  \
{GNB_CONFIG_STRING_REMOTE_S_ADDRESS,             NULL,   0,           .strptr=NULL, .defstrval="127.0.0.2",       TYPE_STRING,    0},  \
{GNB_CONFIG_STRING_LOCAL_S_PORTC,                NULL,   0,           .uptr=NULL,   .defuintval=50000,            TYPE_UINT,      0},  \
{GNB_CONFIG_STRING_REMOTE_S_PORTC,               NULL,   0,           .uptr=NULL,   .defuintval=50000,            TYPE_UINT,      0},  \
{GNB_CONFIG_STRING_LOCAL_S_PORTD,                NULL,   0,           .uptr=NULL,   .defuintval=50001,            TYPE_UINT,      0},  \
{GNB_CONFIG_STRING_REMOTE_S_PORTD,               NULL,   0,           .uptr=NULL,   .defuintval=50001,            TYPE_UINT,      0},  \
{GNB_CONFIG_STRING_PDSCHANTENNAPORTS_N1, "horiz. log. antenna ports", 0,.iptr=NULL, .defintval=1,                 TYPE_INT,       0},  \
{GNB_CONFIG_STRING_PDSCHANTENNAPORTS_N2, "vert. log. antenna ports", 0, .iptr=NULL, .defintval=1,                 TYPE_INT,       0},  \
{GNB_CONFIG_STRING_PDSCHANTENNAPORTS_XP, "XP log. antenna ports",   0, .iptr=NULL,  .defintval=1,                 TYPE_INT,       0},  \
{GNB_CONFIG_STRING_PUSCHANTENNAPORTS,            NULL,   0,            .iptr=NULL,  .defintval=1,                 TYPE_INT,       0},  \
{GNB_CONFIG_STRING_DOTCI,                        NULL,   0,            .iptr=NULL,  .defintval=0,                 TYPE_INT,       0},  \
{GNB_CONFIG_STRING_DOCSIRS,                      NULL,   0,            .iptr=NULL,  .defintval=0,                 TYPE_INT,       0},  \
{GNB_CONFIG_STRING_DOSRS,        GNB_CONFIG_HLP_DOSRS,   0,           .strptr=NULL, .defstrval="none",            TYPE_STRING,    0},  \
{GNB_CONFIG_STRING_NRCELLID,                     NULL,   0,            .u64ptr=NULL,.defint64val=1,               TYPE_UINT64,    0},  \
{GNB_CONFIG_STRING_MINRXTXTIME,                  NULL,   0,            .iptr=NULL,  .defintval=2,                 TYPE_INT,       0},  \
{GNB_CONFIG_STRING_ULPRBBLACKLIST,               NULL,   0,            .strptr=NULL,.defstrval="",                TYPE_STRING,    0},  \
{GNB_CONFIG_STRING_UMONDEFAULTDRB,               NULL, PARAMFLAG_BOOL, .uptr=NULL,  .defuintval=0,                TYPE_UINT,      0},  \
{GNB_CONFIG_STRING_FORCE256QAMOFF, GNB_CONFIG_HLP_FORCE256QAMOFF, PARAMFLAG_BOOL, .iptr=NULL, .defintval=0,       TYPE_INT,       0},  \
{GNB_CONFIG_STRING_ENABLE_SDAP, GNB_CONFIG_HLP_STRING_ENABLE_SDAP, PARAMFLAG_BOOL,.iptr=NULL, .defintval=1,       TYPE_INT,       0},  \
{GNB_CONFIG_STRING_GNB_DU_ID, GNB_CONFIG_HLP_GNB_DU_ID,  0,           .u64ptr=NULL, .defint64val=1,               TYPE_UINT64,    0},  \
{GNB_CONFIG_STRING_GNB_CU_UP_ID, GNB_CONFIG_HLP_GNB_CU_UP_ID, 0,      .u64ptr=NULL, .defint64val=1,               TYPE_UINT64,    0},  \
{GNB_CONFIG_STRING_USE_DELTA_MCS, GNB_CONFIG_HLP_USE_DELTA_MCS, 0,    .iptr=NULL,   .defintval=0,                 TYPE_INT,       0},  \
{GNB_CONFIG_STRING_FORCEUL256QAMOFF, GNB_CONFIG_HLP_FORCEUL256QAMOFF, 0,.iptr=NULL, .defintval=0,                 TYPE_INT,       0},  \
{GNB_CONFIG_STRING_MAXMIMOLAYERS, GNB_CONFIG_HLP_MAXMIMOLAYERS, 0,     .iptr=NULL,  .defintval=-1,                TYPE_INT,       0},  \
{GNB_CONFIG_STRING_DISABLE_HARQ, GNB_CONFIG_HLP_DISABLE_HARQ, PARAMFLAG_BOOL, .iptr=NULL, .defintval=0,           TYPE_INT,       0},  \
{GNB_CONFIG_STRING_NUM_DL_HARQPROCESSES, GNB_CONFIG_HLP_NUM_DL_HARQ, 0, .iptr=NULL, .defintval=16,                TYPE_INT,       0},  \
{GNB_CONFIG_STRING_NUM_UL_HARQPROCESSES, GNB_CONFIG_HLP_NUM_UL_HARQ, 0, .iptr=NULL, .defintval=16,                TYPE_INT,       0},  \
{GNB_CONFIG_STRING_UESS_AGG_LEVEL_LIST, \
                    GNB_CONFIG_HLP_UESS_AGG_LEVEL_LIST,  0,       .iptr=NULL,       .defintarrayval=NULL,         TYPE_INTARRAY,  0},  \
{GNB_CONFIG_STRING_CU_SIB_LIST,                  GNB_CONFIG_HLP_CU_SIBS, 0, .iptr=NULL, .defintarrayval=0,        TYPE_INTARRAY,  0},  \
{GNB_CONFIG_STRING_DU_SIB_LIST,                  GNB_CONFIG_HLP_DU_SIBS, 0, .iptr=NULL, .defintarrayval=0,        TYPE_INTARRAY,  0},  \
{GNB_CONFIG_STRING_CONFIG_REP, GNB_CONFIG_HLP_CONFIG_REP, 0,          .strptr=NULL, .defstrval="ssb_rsrp",        TYPE_STRING,    0},  \
{GNB_CONFIG_STRING_1ST_ACTIVE_BWP,               NULL,   0,            .iptr=NULL,  .defintval=0,                 TYPE_INT,       0},  \
{GNB_CONFIG_STRING_LIMIT_RSRP_REPORT,            NULL,   0,            .iptr=NULL,  .defintval=0,                 TYPE_INT,       0},  \
}
// clang-format on


#define GNB_GNB_ID_IDX                  0
#define GNB_CELL_TYPE_IDX               1
#define GNB_GNB_NAME_IDX                2
#define GNB_TRACKING_AREA_CODE_IDX      3
#define GNB_MOBILE_COUNTRY_CODE_IDX_OLD 4
#define GNB_MOBILE_NETWORK_CODE_IDX_OLD 5
#define GNB_TRANSPORT_S_PREFERENCE_IDX  6
#define GNB_LOCAL_S_ADDRESS_IDX         7
#define GNB_REMOTE_S_ADDRESS_IDX        8
#define GNB_LOCAL_S_PORTC_IDX           9
#define GNB_REMOTE_S_PORTC_IDX          10
#define GNB_LOCAL_S_PORTD_IDX           11
#define GNB_REMOTE_S_PORTD_IDX          12
#define GNB_PDSCH_ANTENNAPORTS_N1_IDX   13
#define GNB_PDSCH_ANTENNAPORTS_N2_IDX   14
#define GNB_PDSCH_ANTENNAPORTS_XP_IDX   15
#define GNB_PUSCH_ANTENNAPORTS_IDX      16
#define GNB_DO_TCI_IDX                  17
#define GNB_DO_CSIRS_IDX                18
#define GNB_DO_SRS_IDX                  19
#define GNB_NRCELLID_IDX                20
#define GNB_MINRXTXTIME_IDX             21
#define GNB_ULPRBBLACKLIST_IDX          22
#define GNB_UMONDEFAULTDRB_IDX          23
#define GNB_FORCE256QAMOFF_IDX          24
#define GNB_ENABLE_SDAP_IDX             25
#define GNB_GNB_DU_ID_IDX               26
#define GNB_GNB_CU_UP_ID_IDX            27
#define GNB_USE_DELTA_MCS_IDX           28
#define GNB_FORCEUL256QAMOFF_IDX        29
#define GNB_MAXMIMOLAYERS_IDX           30
#define GNB_DISABLE_HARQ_IDX            31
#define GNB_NUM_DL_HARQ_IDX             32
#define GNB_NUM_UL_HARQ_IDX             33
#define GNB_UESS_AGG_LEVEL_LIST_IDX     34
#define GNB_CU_SIBS_IDX                 35
#define GNB_DU_SIBS_IDX                 36
#define GNB_CONFIG_REP_IDX              37
#define GNB_1ST_ACTIVE_BWP_IDX          38
#define GNB_LIMIT_RSRP_REPORT_IDX       39

#define TRACKING_AREA_CODE_OKRANGE {0x0001,0xFFFD}
#define NUM_DL_HARQ_OKVALUES {2,4,6,8,10,12,16,32}
#define NUM_UL_HARQ_OKVALUES {16,32}

#define GNBPARAMS_CHECK {                                         \
  { .s5 = { NULL } },                                             \
  { .s5 = { NULL } },                                             \
  { .s5 = { NULL } },                                             \
  { .s2 = { config_check_intrange, TRACKING_AREA_CODE_OKRANGE } },\
  { .s5 = { NULL } },                                             \
  { .s5 = { NULL } },                                             \
  { .s5 = { NULL } },                                             \
  { .s5 = { NULL } },                                             \
  { .s5 = { NULL } },                                             \
  { .s5 = { NULL } },                                             \
  { .s5 = { NULL } },                                             \
  { .s5 = { NULL } },                                             \
  { .s5 = { NULL } },                                             \
  { .s5 = { NULL } },                                             \
  { .s5 = { NULL } },                                             \
  { .s5 = { NULL } },                                             \
  { .s5 = { NULL } },                                             \
  { .s5 = { NULL } },                                             \
  { .s5 = { NULL } },                                             \
  { .s3a = { config_checkstr_assign_integer, \
             {"none", "periodic", "aperiodic"}, \
             {NO_SRS, PERIODIC_SRS, APERIODIC_SRS}, \
             3 } }, \
  { .s5 = { NULL } },                                             \
  { .s5 = { NULL } },                                             \
  { .s5 = { NULL } },                                             \
  { .s5 = { NULL } },                                             \
  { .s5 = { NULL } },                                             \
  { .s5 = { NULL } },                                             \
  { .s5 = { NULL } },                                             \
  { .s5 = { NULL } },                                             \
  { .s5 = { NULL } },                                             \
  { .s5 = { NULL } },                                             \
  { .s5 = { NULL } },                                             \
  { .s5 = { NULL } },                                             \
  { .s1 =  { config_check_intval, NUM_DL_HARQ_OKVALUES,8 } },     \
  { .s1 =  { config_check_intval, NUM_UL_HARQ_OKVALUES,2 } },     \
  { .s5 = { NULL } },                                             \
  { .s5 = { NULL } },                                             \
  { .s5 = { NULL } },                                             \
  { .s3a = { config_checkstr_assign_integer, \
             {"ssb_rsrp", "ssb_sinr", "cri_rsrp"}, \
             {SSB_RSRP, SSB_SINR, CRI_RSRP}, \
             3 } }, \
  { .s5 = { NULL } },                                             \
  { .s5 = { NULL } },                                             \
}

/*-------------------------------------------------------------------------------------------------------------------------------------------------*/

#define GNB_CONFIG_STRING_BWP_LIST                      "bwp_list"

#define GNB_CONFIG_STRING_BWP_SCS     "scs"
#define GNB_CONFIG_STRING_BWP_START   "bwpStart"
#define GNB_CONFIG_STRING_BWP_SIZE    "bwpSize"

#define GNB_BWP_SCS_IDX       0
#define GNB_BWP_START_IDX     1
#define GNB_BWP_SIZE_IDX      2

#define GNBBWPPARAMS_DESC {                                                                  \
 {GNB_CONFIG_STRING_BWP_SCS,            NULL,   0,            .iptr=NULL,  .defintarrayval=0,            TYPE_INT,  0},  \
 {GNB_CONFIG_STRING_BWP_START,          NULL,   0,            .iptr=NULL,  .defintarrayval=0,            TYPE_INT,  0},  \
 {GNB_CONFIG_STRING_BWP_SIZE,           NULL,   0,            .iptr=NULL,  .defintarrayval=0,            TYPE_INT,  0},  \
}

#define BWPPARAMS_CHECK {                                         \
  { .s5 = { NULL } },                                             \
  { .s5 = { NULL } },                                             \
  { .s5 = { NULL } },                                             \
}

/*-------------------------------------------------------------------------------------------------------------------------------------------------*/

/* Neighbour Cell Configurations*/
#define GNB_CONFIG_STRING_NEIGHBOUR_LIST "neighbour_list"
// clang-format off
#define GNB_NEIGHBOUR_LIST_PARAM_LIST {                                                                  \
/*   optname                                                  helpstr                                 paramflags                    XXXptr     def val          type    numelt */ \
  {GNB_CONFIG_STRING_NRCELLID,                              "cell nrCell Id which has neighbours",              PARAMFLAG_MANDATORY,           .u64ptr=NULL, .defint64val=0,               TYPE_UINT64,    0},    \
}
// clang-format on
#define GNB_CONFIG_STRING_NEIGHBOUR_CELL_LIST "neighbour_cell_configuration"

#define GNB_CONFIG_STRING_NEIGHBOUR_CELL_PHYSICAL_ID "physical_cellId"
#define GNB_CONFIG_STRING_NEIGHBOUR_CELL_ABS_FREQ_SSB "absoluteFrequencySSB"
#define GNB_CONFIG_STRING_NEIGHBOUR_CELL_SCS "subcarrierSpacing"
#define GNB_CONFIG_STRING_NEIGHBOUR_CELL_BAND "band"
#define GNB_CONFIG_STRING_NEIGHBOUR_TRACKING_ARE_CODE "tracking_area_code"
#define GNB_CONFIG_STRING_NEIGHBOUR_PLMN "plmn"
/* SIB3 fields (intra-frequency) - per-neighbor */
#define GNB_CONFIG_STRING_NEIGHBOUR_CELL_Q_OFFSET_CELL "q_OffsetCell"
#define GNB_CONFIG_STRING_NEIGHBOUR_CELL_Q_RXLEVMIN_OFFSET_CELL "q_RxLevMinOffsetCell"
#define GNB_CONFIG_STRING_NEIGHBOUR_CELL_Q_QUALMIN_OFFSET_CELL "q_QualMinOffsetCell"
/* SIB4 per-frequency fields are configured in frequency_list.frequency_config */

// clang-format off
#define GNBNEIGHBOURCELLPARAMS_DESC { /*   optname     helpstr     paramflags     XXXptr     def     val     type     numelt   */        \
  {GNB_CONFIG_STRING_GNB_ID,                                                                                                             \
    "neighbour cell's gNB ID", PARAMFLAG_MANDATORY, .uptr=NULL, .defintval=0, TYPE_UINT, 0,                                              \
    .chkPptr=NULL},                                                                                                                      \
  {GNB_CONFIG_STRING_NRCELLID,                                                                                                           \
    "neighbour cell nrCell Id", PARAMFLAG_MANDATORY, .u64ptr=NULL, .defint64val=0, TYPE_UINT64, 0,                                       \
    .chkPptr=NULL},                                                                                                                      \
  {GNB_CONFIG_STRING_NEIGHBOUR_CELL_PHYSICAL_ID,                                                                                         \
    "neighbour cell physical id", PARAMFLAG_MANDATORY, .uptr=NULL, .defuintval=0, TYPE_UINT, 0,                                          \
    .chkPptr = &(checkedparam_t){.s2 = {config_check_uintrange, {0, 1007}}}},                                                            \
  {GNB_CONFIG_STRING_NEIGHBOUR_CELL_ABS_FREQ_SSB,                                                                                        \
    "neighbour cell absolute frequency SSB (0..3279165)", PARAMFLAG_MANDATORY, .i64ptr=NULL, .defint64val=0, TYPE_INT64, 0,              \
    .chkPptr = &(checkedparam_t){.s2 = {config_check_intrange, {0, 3279165}}}},                                                          \
  {GNB_CONFIG_STRING_NEIGHBOUR_CELL_SCS,                                                                                                 \
    "neighbour cell scs (15/30/60/120 kHz)", PARAMFLAG_MANDATORY, .uptr=NULL, .defuintval=0, TYPE_UINT, 0,                               \
    .chkPptr = &(checkedparam_t){.s1 = {config_check_intval, {0, 1, 2, 3}, 4}}},                                                         \
  {GNB_CONFIG_STRING_NEIGHBOUR_CELL_BAND,                                                                                                \
    "neighbour cell band (1..1024)", PARAMFLAG_MANDATORY, .uptr=NULL, .defuintval=78, TYPE_UINT, 0,                                      \
    .chkPptr = &(checkedparam_t){.s2 = {config_check_uintrange, {1, 1024}}}},                                                            \
  {GNB_CONFIG_STRING_NEIGHBOUR_TRACKING_ARE_CODE,                                                                                        \
    "neighbour cell tracking area (0..16777215)", PARAMFLAG_MANDATORY, .uptr=NULL, .defuintval=0, TYPE_UINT, 0,                          \
    .chkPptr = &(checkedparam_t){.s2 = {config_check_uintrange, {0, 16777215}}}},                                                        \
  {GNB_CONFIG_STRING_NEIGHBOUR_CELL_Q_OFFSET_CELL,                                                                                       \
    "Q-OffsetCell for SIB3/SIB4 (Q-OffsetRange, default 0)", 0, .iptr=NULL, .defintval=0, TYPE_INT, 0,                                   \
    .chkPptr = &(checkedparam_t){.s1 = {config_check_intval,                                                                             \
    {-24, -22, -20, -18, -16, -14, -12, -10, -8, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24}, 31}}}, \
  {GNB_CONFIG_STRING_NEIGHBOUR_CELL_Q_RXLEVMIN_OFFSET_CELL,                                                                              \
    "Q-RxLevMinOffsetCell for SIB3/SIB4 (1..8 or -1=disabled)", 0, .iptr=NULL, .defintval=-1, TYPE_INT, 0,                               \
    .chkPptr = &(checkedparam_t){.s1 = {config_check_intval, {-1, 1, 2, 3, 4, 5, 6, 7, 8}, 9}}},                                         \
  {GNB_CONFIG_STRING_NEIGHBOUR_CELL_Q_QUALMIN_OFFSET_CELL,                                                                               \
    "Q-QualMinOffsetCell for SIB3/SIB4 (1..8 or -1=disabled)", 0, .iptr=NULL, .defintval=-1, TYPE_INT, 0,                                \
    .chkPptr = &(checkedparam_t){.s1 = {config_check_intval, {-1, 1, 2, 3, 4, 5, 6, 7, 8}, 9}}},                                         \
}
// clang-format on

/* Per-frequency neighbour configuration (frequency_list) */
#define GNB_CONFIG_STRING_FREQUENCY_LIST "frequency_list"
#define GNB_CONFIG_STRING_FREQUENCY_ABS_FREQ_SSB "absoluteFrequencySSB"
#define GNB_CONFIG_STRING_FREQUENCY_SCS "subcarrierSpacing"
#define GNB_CONFIG_STRING_FREQUENCY_BAND "band"
#define GNB_CONFIG_STRING_FREQUENCY_CONFIG "frequency_config"

#define GNB_CONFIG_STRING_FREQUENCY_CELL_RESEL_PRIO "cellReselectionPriority"
#define GNB_CONFIG_STRING_FREQUENCY_THRESH_X_HIGH_P "threshX_HighP"
#define GNB_CONFIG_STRING_FREQUENCY_THRESH_X_LOW_P "threshX_LowP"
#define GNB_CONFIG_STRING_FREQUENCY_THRESH_X_HIGH_Q "threshX_HighQ"
#define GNB_CONFIG_STRING_FREQUENCY_THRESH_X_LOW_Q "threshX_LowQ"
#define GNB_CONFIG_STRING_FREQUENCY_Q_OFFSET_FREQ "q_OffsetFreq"
#define GNB_CONFIG_STRING_FREQUENCY_Q_RXLEVMIN "q_RxLevMin"
#define GNB_CONFIG_STRING_FREQUENCY_T_RESEL_NR "t_ReselectionNR"

// clang-format off
#define GNBFREQUENCYPARAMS_DESC {                                                                  \
/*   optname                                     helpstr                          paramflags      XXXptr     def val    type      numelt */ \
  {GNB_CONFIG_STRING_FREQUENCY_ABS_FREQ_SSB, "inter-frequency abs freq ssb",     PARAMFLAG_MANDATORY, .i64ptr=NULL, .defint64val=0, TYPE_INT64, 0}, \
  {GNB_CONFIG_STRING_FREQUENCY_SCS,          "inter-frequency scs",              PARAMFLAG_MANDATORY, .uptr=NULL,   .defuintval=0,  TYPE_UINT,  0}, \
  {GNB_CONFIG_STRING_FREQUENCY_BAND,         "inter-frequency band",             PARAMFLAG_MANDATORY, .uptr=NULL,   .defuintval=78, TYPE_UINT,  0}, \
}

#define GNBFREQUENCYCONFIGPARAMS_DESC {                                                                 \
  {GNB_CONFIG_STRING_FREQUENCY_CELL_RESEL_PRIO,                                                         \
    "SIB4 cellReselectionPriority (0..7, -1=disabled)", 0, .iptr=NULL, .defintval=-1, TYPE_INT, 0,      \
    .chkPptr = &(checkedparam_t){.s2 = {config_check_intrange, {-1, 7}}}},                              \
  {GNB_CONFIG_STRING_FREQUENCY_THRESH_X_HIGH_P,                                                         \
    "SIB4 threshX-HighP (0..31)", 0, .iptr=NULL, .defintval=0, TYPE_INT, 0,                             \
    .chkPptr = &(checkedparam_t){.s2 = {config_check_intrange, {0, 31}}}},                              \
  {GNB_CONFIG_STRING_FREQUENCY_THRESH_X_LOW_P,                                                          \
    "SIB4 threshX-LowP (0..31)", 0, .iptr=NULL, .defintval=0, TYPE_INT, 0,                              \
    .chkPptr = &(checkedparam_t){.s2 = {config_check_intrange, {0, 31}}}},                              \
  {GNB_CONFIG_STRING_FREQUENCY_THRESH_X_HIGH_Q,                                                         \
    "SIB4 threshX-HighQ (0..31, -1=disabled)", 0, .iptr=NULL, .defintval=-1, TYPE_INT, 0,               \
    .chkPptr = &(checkedparam_t){.s2 = {config_check_intrange, {-1, 31}}}},                             \
  {GNB_CONFIG_STRING_FREQUENCY_THRESH_X_LOW_Q,                                                          \
    "SIB4 threshX-LowQ (0..31, -1=disabled)", 0, .iptr=NULL, .defintval=-1, TYPE_INT, 0,                \
    .chkPptr = &(checkedparam_t){.s2 = {config_check_intrange, {-1, 31}}}},                             \
  {GNB_CONFIG_STRING_FREQUENCY_Q_OFFSET_FREQ,                                                           \
    "SIB4 q-OffsetFreq (Q-OffsetRange values, default 0)", 0, .iptr=NULL, .defintval=0, TYPE_INT, 0,    \
    .chkPptr = &(checkedparam_t){.s1 = {config_check_intval, {-24, -22, -20, -18, -16, -14, -12, -10,   \
    -8, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24}, 31}}},         \
  {GNB_CONFIG_STRING_FREQUENCY_Q_RXLEVMIN,                                                              \
    "SIB4 per-carrier Q-RxLevMin (-70..-22, default -22)", 0, .iptr=NULL, .defintval=-22, TYPE_INT, 0,  \
    .chkPptr = &(checkedparam_t){.s2 = {config_check_intrange, {-70, -22}}}},                           \
  {GNB_CONFIG_STRING_FREQUENCY_T_RESEL_NR,                                                              \
    "SIB4 per-carrier t-ReselectionNR (0..7, default 0)", 0, .uptr=NULL, .defuintval=0, TYPE_UINT, 0,   \
    .chkPptr = &(checkedparam_t){.s2 = {config_check_uintrange, {0, 7}}}},                              \
}
// clang-format on

/* Indexes into GNBFREQUENCYCONFIGPARAMS_DESC */
#define GNB_CONFIG_FREQUENCY_CELL_RESEL_PRIO_IDX 0
#define GNB_CONFIG_FREQUENCY_THRESH_X_HIGH_P_IDX 1
#define GNB_CONFIG_FREQUENCY_THRESH_X_LOW_P_IDX 2
#define GNB_CONFIG_FREQUENCY_THRESH_X_HIGH_Q_IDX 3
#define GNB_CONFIG_FREQUENCY_THRESH_X_LOW_Q_IDX 4
#define GNB_CONFIG_FREQUENCY_Q_OFFSET_FREQ_IDX 5
#define GNB_CONFIG_FREQUENCY_Q_RXLEVMIN_IDX 6
#define GNB_CONFIG_FREQUENCY_T_RESEL_NR_IDX 7

/* New Measurement Configurations*/

#define GNB_CONFIG_STRING_MEASUREMENT_CONFIGURATION "nr_measurement_configuration"
#define MEASUREMENT_EVENTS_PERIODICAL "Periodical"
#define MEASUREMENT_EVENTS_A2 "A2"
#define MEASUREMENT_EVENTS_A3 "A3"

#define MEASUREMENT_EVENTS_OFFSET "offset"
#define MEASUREMENT_EVENTS_HYSTERESIS "hysteresis"
#define MEASUREMENT_EVENTS_TIME_TO_TRIGGER "time_to_trigger"
#define MEASUREMENT_EVENTS_THRESHOLD "threshold"
#define MEASUREMENT_EVENTS_PERIODICAL_BEAM_MEASUREMENT "includeBeamMeasurements"
#define MEASUREMENT_EVENTS_PERIODICAL_NR_OF_RS_INDEXES "maxNrofRS_IndexesToReport"
#define MEASUREMENT_EVENTS_PCI_ID "physCellId"
#define MEASUREMENT_EVENT_ENABLE "enable"
// clang-format off
#define MEASUREMENT_A3_GLOBALPARAMS_DESC                                                                                      \
  {                                                                                                                               \
        {MEASUREMENT_EVENTS_PCI_ID, "neighbour PCI for A3Report", 0, .i64ptr = NULL, .defint64val = -1, TYPE_INT64, 0},           \
        {MEASUREMENT_EVENTS_TIME_TO_TRIGGER, "a3 time to trigger", 0, .i64ptr = NULL, .defint64val = 1, TYPE_INT64, 0}, \
        {MEASUREMENT_EVENTS_OFFSET, "a3 offset", 0, .i64ptr = NULL, .defint64val = 60, TYPE_INT64, 0},                  \
        {MEASUREMENT_EVENTS_HYSTERESIS, "a3 hysteresis", 0, .i64ptr = NULL, .defint64val = 0, TYPE_INT64, 0},           \
  }

#define MEASUREMENT_A2_GLOBALPARAMS_DESC                                                                                      \
  {                                                                                                                               \
        {MEASUREMENT_EVENT_ENABLE, "enable the event", 0, .i64ptr = NULL, .defint64val = 1, TYPE_INT64, 0}, \
        {MEASUREMENT_EVENTS_TIME_TO_TRIGGER, "a2 time to trigger", 0, .i64ptr = NULL, .defint64val = 1, TYPE_INT64, 0}, \
        {MEASUREMENT_EVENTS_THRESHOLD, "a2 threshold", 0, .i64ptr = NULL, .defint64val = 60, TYPE_INT64, 0},            \
  }

#define MEASUREMENT_PERIODICAL_GLOBALPARAMS_DESC                                                                                      \
  {                                                                                                                               \
        {MEASUREMENT_EVENT_ENABLE, "enable the event", 0, .i64ptr = NULL, .defint64val = 1, TYPE_INT64, 0}, \
        {MEASUREMENT_EVENTS_PERIODICAL_BEAM_MEASUREMENT, "includeBeamMeasurements", PARAMFLAG_BOOL, .i64ptr = NULL, .defint64val = 1, TYPE_INT64, 0}, \
        {MEASUREMENT_EVENTS_PERIODICAL_NR_OF_RS_INDEXES, "maxNrofRS_IndexesToReport", 0, .i64ptr = NULL, .defint64val = 4, TYPE_INT64, 0},            \
  }
// clang-format on

#define MEASUREMENT_EVENTS_PCI_ID_IDX 0
#define MEASUREMENT_EVENTS_ENABLE_IDX 0
#define MEASUREMENT_EVENTS_TIMETOTRIGGER_IDX 1
#define MEASUREMENT_EVENTS_A2_THRESHOLD_IDX 2
#define MEASUREMENT_EVENTS_OFFSET_IDX 2
#define MEASUREMENT_EVENTS_HYSTERESIS_IDX 3
#define MEASUREMENT_EVENTS_INCLUDE_BEAM_MEAS_IDX 1
#define MEASUREMENT_EVENTS_MAX_RS_INDEX_TO_REPORT 2

/*-------------------------------------------------------------------------------------------------------------------------------------*/
/*                                            SIB2 cell reselection configuration parameters                                           */
/*-------------------------------------------------------------------------------------------------------------------------------------*/

#define GNB_CONFIG_STRING_SIB2_CONFIG "sib2_config"

/* clang-format off */

/* SIB2 top-level parameters (per cell) */
#define GNB_CONFIG_STRING_SIB2_Q_HYST               "q_Hyst"
#define GNB_CONFIG_STRING_SIB2_CELLRESEL_PRIORITY   "cellReselectionPriority"
#define GNB_CONFIG_STRING_SIB2_THRESH_SERVING_LOW_P "threshServingLowP"
#define GNB_CONFIG_STRING_SIB2_THRESH_SERVING_LOW_Q "threshServingLowQ"
#define GNB_CONFIG_STRING_SIB2_S_NONINTRASEARCH_P   "s_NonIntraSearchP"
#define GNB_CONFIG_STRING_SIB2_S_NONINTRASEARCH_Q   "s_NonIntraSearchQ"
#define GNB_CONFIG_STRING_SIB2_Q_RXLEVMIN           "q_RxLevMin"
#define GNB_CONFIG_STRING_SIB2_Q_QUALMIN            "q_QualMin"
#define GNB_CONFIG_STRING_SIB2_S_INTRASEARCH_P      "s_IntraSearchP"
#define GNB_CONFIG_STRING_SIB2_S_INTRASEARCH_Q      "s_IntraSearchQ"
#define GNB_CONFIG_STRING_SIB2_T_RESEL_NR           "t_ReselectionNR"
#define GNB_CONFIG_STRING_SIB2_DERIVE_SSB_IDX       "deriveSSB_IndexFromCell"

/* SIB2 speedStateReselectionPars parameters */
#define GNB_CONFIG_STRING_SIB2_SPEED_T_EVAL          "speed_t_Evaluation"
#define GNB_CONFIG_STRING_SIB2_SPEED_T_HYST_NORMAL   "speed_t_HystNormal"
#define GNB_CONFIG_STRING_SIB2_SPEED_N_CELL_CHG_MED  "speed_n_CellChangeMedium"
#define GNB_CONFIG_STRING_SIB2_SPEED_N_CELL_CHG_HIGH "speed_n_CellChangeHigh"
#define GNB_CONFIG_STRING_SIB2_SPEED_SF_MEDIUM       "speed_sf_Medium"
#define GNB_CONFIG_STRING_SIB2_SPEED_SF_HIGH         "speed_sf_High"

#define GNBSIB2PARAMS_DESC { /*   optname       helpstr       paramflags XXXptr     def val    type     numelt */               \
  {GNB_CONFIG_STRING_SIB2_Q_HYST,                                                                                               \
    "SIB2 q-Hyst (0,1,2,3,4,5,6,8,10..24 dB)", 0, .iptr=NULL, .defintval=0, TYPE_INT, 0,                                        \
    .chkPptr = &(checkedparam_t){.s1 = {config_check_intval, {0, 1, 2, 3, 4, 5, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24}, 16}}},   \
  {GNB_CONFIG_STRING_SIB2_CELLRESEL_PRIORITY,                                                                                   \
    "SIB2 cellReselectionPriority (0..7)", 0, .iptr=NULL, .defintval=0, TYPE_INT, 0,                                            \
    .chkPptr = &(checkedparam_t){.s2 = {config_check_intrange, {0, 7}}}},                                                       \
  {GNB_CONFIG_STRING_SIB2_THRESH_SERVING_LOW_P,                                                                                 \
    "SIB2 threshServingLowP (0..31)", 0, .iptr=NULL, .defintval=0, TYPE_INT, 0,                                                 \
    .chkPptr = &(checkedparam_t){.s2 = {config_check_intrange, {0, 31}}}},                                                      \
  {GNB_CONFIG_STRING_SIB2_THRESH_SERVING_LOW_Q,                                                                                 \
    "SIB2 threshServingLowQ (0..31, -1=disabled)", 0, .iptr=NULL, .defintval=0, TYPE_INT, 0,                                    \
    .chkPptr = &(checkedparam_t){.s2 = {config_check_intrange, {-1, 31}}}},                                                     \
  {GNB_CONFIG_STRING_SIB2_S_NONINTRASEARCH_P,                                                                                   \
    "SIB2 s-NonIntraSearchP (0..31, -1=disabled)", 0, .iptr=NULL, .defintval=-1, TYPE_INT, 0,                                   \
    .chkPptr = &(checkedparam_t){.s2 = {config_check_intrange, {-1, 31}}}},                                                     \
  {GNB_CONFIG_STRING_SIB2_S_NONINTRASEARCH_Q,                                                                                   \
    "SIB2 s-NonIntraSearchQ (0..31, -1=disabled)", 0, .iptr=NULL, .defintval=-1, TYPE_INT, 0,                                   \
    .chkPptr = &(checkedparam_t){.s2 = {config_check_intrange, {-1, 31}}}},                                                     \
  {GNB_CONFIG_STRING_SIB2_Q_RXLEVMIN,                                                                                           \
    "SIB2 q-RxLevMin (-70..-22)", 0, .iptr=NULL, .defintval=-56, TYPE_INT, 0,                                                   \
    .chkPptr = &(checkedparam_t){.s2 = {config_check_intrange, {-70, -22}}}},                                                   \
  {GNB_CONFIG_STRING_SIB2_Q_QUALMIN,                                                                                            \
    "SIB2 q-QualMin (-43..-12 or -1=disabled)", 0, .iptr=NULL, .defintval=-1, TYPE_INT, 0,                                      \
    .chkPptr = &(checkedparam_t){.s1 = {config_check_intval,                                                                    \
    {-1,  -43, -42, -41, -40, -39, -38, -37, -36, -35, -34, -33, -32, -31, -30, -29, -28,                                       \
    -27, -26, -25, -24, -23, -22, -21, -20, -19, -18, -17, -16, -15, -14, -13, -12}, 33}}},                                     \
  {GNB_CONFIG_STRING_SIB2_S_INTRASEARCH_P,                                                                                      \
    "SIB2 s-IntraSearchP (0..31)", 0, .iptr=NULL, .defintval=22, TYPE_INT, 0,                                                   \
    .chkPptr = &(checkedparam_t){.s2 = {config_check_intrange, {0, 31}}}},                                                      \
  {GNB_CONFIG_STRING_SIB2_S_INTRASEARCH_Q,                                                                                      \
    "SIB2 s-IntraSearchQ (0..31, -1=disabled)", 0, .iptr=NULL, .defintval=-1, TYPE_INT, 0,                                      \
    .chkPptr = &(checkedparam_t){.s2 = {config_check_intrange, {-1, 31}}}},                                                     \
  {GNB_CONFIG_STRING_SIB2_T_RESEL_NR,                                                                                           \
    "SIB2 t-ReselectionNR (0..7)", 0, .iptr=NULL, .defintval=1, TYPE_INT, 0,                                                    \
    .chkPptr = &(checkedparam_t){.s2 = {config_check_intrange, {0, 7}}}},                                                       \
  {GNB_CONFIG_STRING_SIB2_DERIVE_SSB_IDX,                                                                                       \
    "SIB2 deriveSSB-IndexFromCell (0/1)", PARAMFLAG_BOOL, .iptr=NULL, .defintval=1, TYPE_INT, 0},                               \
  {GNB_CONFIG_STRING_SIB2_SPEED_T_EVAL,                                                                                         \
    "SIB2 speed t-Evaluation (0=s30,1=s60,2=s120,3=s180,4=s240)", 0, .iptr=NULL, .defintval=0, TYPE_INT, 0,                     \
    .chkPptr = &(checkedparam_t){.s2 = {config_check_intrange, {0, 4}}}},                                                       \
  {GNB_CONFIG_STRING_SIB2_SPEED_T_HYST_NORMAL,                                                                                  \
    "SIB2 speed t-HystNormal (0=s30,1=s60,2=s120,3=s180,4=s240)", 0, .iptr=NULL, .defintval=0, TYPE_INT, 0,                     \
    .chkPptr = &(checkedparam_t){.s2 = {config_check_intrange, {0, 4}}}},                                                       \
  {GNB_CONFIG_STRING_SIB2_SPEED_N_CELL_CHG_MED,                                                                                 \
    "SIB2 speed n-CellChangeMedium (1..16)", 0, .iptr=NULL, .defintval=1, TYPE_INT, 0,                                          \
    .chkPptr = &(checkedparam_t){.s2 = {config_check_intrange, {1, 16}}}},                                                      \
  {GNB_CONFIG_STRING_SIB2_SPEED_N_CELL_CHG_HIGH,                                                                                \
    "SIB2 speed n-CellChangeHigh (1..16)", 0, .iptr=NULL, .defintval=2, TYPE_INT, 0,                                            \
    .chkPptr = &(checkedparam_t){.s2 = {config_check_intrange, {1, 16}}}},                                                      \
  {GNB_CONFIG_STRING_SIB2_SPEED_SF_MEDIUM,                                                                                      \
    "SIB2 speed sf-Medium (0=dB-6,1=dB-4,2=dB-2,3=dB0)", 0, .iptr=NULL, .defintval=1, TYPE_INT, 0,                              \
    .chkPptr = &(checkedparam_t){.s2 = {config_check_intrange, {0, 3}}}},                                                       \
  {GNB_CONFIG_STRING_SIB2_SPEED_SF_HIGH,                                                                                        \
    "SIB2 speed sf-High (0=dB-6,1=dB-4,2=dB-2,3=dB0)", 0, .iptr=NULL, .defintval=0, TYPE_INT, 0,                                \
    .chkPptr = &(checkedparam_t){.s2 = {config_check_intrange, {0, 3}}}},                                                       \
}
/* clang-format on */

/* PLMN ID configuration */

#define GNB_CONFIG_STRING_PLMN_LIST                     "plmn_list"

#define GNB_CONFIG_STRING_MOBILE_COUNTRY_CODE           "mcc"
#define GNB_CONFIG_STRING_MOBILE_NETWORK_CODE           "mnc"
#define GNB_CONFIG_STRING_MNC_DIGIT_LENGTH              "mnc_length"

#define GNB_MOBILE_COUNTRY_CODE_IDX     0
#define GNB_MOBILE_NETWORK_CODE_IDX     1
#define GNB_MNC_DIGIT_LENGTH            2

#define GNBPLMNPARAMS_DESC {                                                                  \
/*   optname                              helpstr               paramflags XXXptr     def val          type    numelt */ \
  {GNB_CONFIG_STRING_MOBILE_COUNTRY_CODE, "mobile country code",        0, .uptr=NULL, .defuintval=1000, TYPE_UINT, 0},    \
  {GNB_CONFIG_STRING_MOBILE_NETWORK_CODE, "mobile network code",        0, .uptr=NULL, .defuintval=1000, TYPE_UINT, 0},    \
  {GNB_CONFIG_STRING_MNC_DIGIT_LENGTH,    "length of the MNC (2 or 3)", 0, .uptr=NULL, .defuintval=0,    TYPE_UINT, 0},    \
}

#define MCC_MNC_OKRANGES           {0,999}
#define MNC_DIGIT_LENGTH_OKVALUES  {2,3}

#define PLMNPARAMS_CHECK {                                           \
  { .s2 = { config_check_intrange, MCC_MNC_OKRANGES } },             \
  { .s2 = { config_check_intrange, MCC_MNC_OKRANGES } },             \
  { .s1 = { config_check_intval,   MNC_DIGIT_LENGTH_OKVALUES, 2 } }, \
}


/* SNSSAI ID configuration */

#define GNB_CONFIG_STRING_SNSSAI_LIST                   "snssaiList"

#define GNB_CONFIG_STRING_SLICE_SERVICE_TYPE            "sst"
#define GNB_CONFIG_STRING_SLICE_DIFFERENTIATOR          "sd"

#define GNB_SLICE_SERVICE_TYPE_IDX       0
#define GNB_SLICE_DIFFERENTIATOR_IDX     1

#define GNBSNSSAIPARAMS_DESC {                                                                  \
/*   optname                               helpstr                 paramflags XXXptr     def val              type    numelt */ \
  {GNB_CONFIG_STRING_SLICE_SERVICE_TYPE,   "slice service type",           0, .uptr=NULL, .defuintval=1,        TYPE_UINT, 0},    \
  {GNB_CONFIG_STRING_SLICE_DIFFERENTIATOR, "slice differentiator",         0, .uptr=NULL, .defuintval=0xffffff, TYPE_UINT, 0},   \
}

#define SLICE_SERVICE_TYPE_OKRANGE        {0, 255}
#define SLICE_DIFFERENTIATOR_TYPE_OKRANGE {0, 0xffffff}

#define SNSSAIPARAMS_CHECK {                                           \
  { .s2 = { config_check_intrange, SLICE_SERVICE_TYPE_OKRANGE } },        \
  { .s2 = { config_check_intrange, SLICE_DIFFERENTIATOR_TYPE_OKRANGE } }, \
}

/* AMF configuration parameters section name */
#define GNB_CONFIG_STRING_AMF_IP_ADDRESS                "amf_ip_address"

/* SRB1 configuration parameters names   */


#define GNB_CONFIG_STRING_AMF_IPV4_ADDRESS              "ipv4"

/*-------------------------------------------------------------------------------------------------------------------------------------*/
/*                                            MME configuration parameters                                                             */
/*   optname                                          helpstr   paramflags    XXXptr       defXXXval         type           numelt     */
/*-------------------------------------------------------------------------------------------------------------------------------------*/
#define GNBNGPARAMS_DESC {  \
  {GNB_CONFIG_STRING_AMF_IPV4_ADDRESS,                   NULL,      0,         .uptr=NULL,   .defstrval=NULL,   TYPE_STRING,   0},          \
}

#define GNB_AMF_IPV4_ADDRESS_IDX          0

/*---------------------------------------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------------------------------------*/
/* TIMERS configuration parameters section name */
#define GNB_CONFIG_STRING_TIMERS_CONFIG                  "TIMERS"

/* TIMERS configuration parameters names   */
#define GNB_CONFIG_STRING_TIMERS_SR_PROHIBIT_TIMER       "sr_ProhibitTimer"
#define GNB_CONFIG_STRING_TIMERS_SR_TRANS_MAX            "sr_TransMax"
#define GNB_CONFIG_STRING_TIMERS_SR_PROHIBIT_TIMER_V1700 "sr_ProhibitTimer_v1700"
#define GNB_CONFIG_STRING_TIMERS_T300                    "t300"
#define GNB_CONFIG_STRING_TIMERS_T301                    "t301"
#define GNB_CONFIG_STRING_TIMERS_T310                    "t310"
#define GNB_CONFIG_STRING_TIMERS_N310                    "n310"
#define GNB_CONFIG_STRING_TIMERS_T311                    "t311"
#define GNB_CONFIG_STRING_TIMERS_N311                    "n311"
#define GNB_CONFIG_STRING_TIMERS_T319                    "t319"

/*-------------------------------------------------------------------------------------------------------------------------------------*/
/*                                            TIMERS configuration parameters                                                          */
/*   optname                                          helpstr   paramflags    XXXptr       defXXXval         type           numelt     */
/*-------------------------------------------------------------------------------------------------------------------------------------*/
#define GNB_TIMERS_PARAMS_DESC {  \
{GNB_CONFIG_STRING_TIMERS_SR_PROHIBIT_TIMER,          NULL,     0,            .iptr=NULL,  .defintval=0,     TYPE_INT,      0},       \
{GNB_CONFIG_STRING_TIMERS_SR_TRANS_MAX,               NULL,     0,            .iptr=NULL,  .defintval=64,    TYPE_INT,      0},       \
{GNB_CONFIG_STRING_TIMERS_SR_PROHIBIT_TIMER_V1700,    NULL,     0,            .iptr=NULL,  .defintval=0,     TYPE_INT,      0},       \
{GNB_CONFIG_STRING_TIMERS_T300,                       NULL,     0,            .iptr=NULL,  .defintval=400,   TYPE_INT,      0},       \
{GNB_CONFIG_STRING_TIMERS_T301,                       NULL,     0,            .iptr=NULL,  .defintval=400,   TYPE_INT,      0},       \
{GNB_CONFIG_STRING_TIMERS_T310,                       NULL,     0,            .iptr=NULL,  .defintval=2000,  TYPE_INT,      0},       \
{GNB_CONFIG_STRING_TIMERS_N310,                       NULL,     0,            .iptr=NULL,  .defintval=10,    TYPE_INT,      0},       \
{GNB_CONFIG_STRING_TIMERS_T311,                       NULL,     0,            .iptr=NULL,  .defintval=3000,  TYPE_INT,      0},       \
{GNB_CONFIG_STRING_TIMERS_N311,                       NULL,     0,            .iptr=NULL,  .defintval=1,     TYPE_INT,      0},       \
{GNB_CONFIG_STRING_TIMERS_T319,                       NULL,     0,            .iptr=NULL,  .defintval=400,   TYPE_INT,      0},       \
}

#define GNB_TIMERS_SR_PROHIBIT_TIMER_IDX       0
#define GNB_TIMERS_SR_TRANS_MAX_IDX            1
#define GNB_TIMERS_SR_PROHIBIT_TIMER_V1700_IDX 2
#define GNB_TIMERS_T300_IDX                    3
#define GNB_TIMERS_T301_IDX                    4
#define GNB_TIMERS_T310_IDX                    5
#define GNB_TIMERS_N310_IDX                    6
#define GNB_TIMERS_T311_IDX                    7
#define GNB_TIMERS_N311_IDX                    8
#define GNB_TIMERS_T319_IDX                    9

/*-------------------------------------------------------------------------------------------------------------------------------------*/
/*                                            RedCap configuration parameters                                                          */
/*-------------------------------------------------------------------------------------------------------------------------------------*/

#define GNB_CONFIG_HLP_STRING_CELL_BARRED_REDCAP1_RX_R17         "Value barred means that the cell is barred for a RedCap UE supporting 1 Rx branch\n"
#define GNB_CONFIG_HLP_STRING_CELL_BARRED_REDCAP2_RX_R17         "Value barred means that the cell is barred for a RedCap UE supporting 2 Rx branches\n"
#define GNB_CONFIG_HLP_STRING_INTRA_FREQ_RESELECTION_REDCAP_R17  "Controls cell selection/reselection to intra-frequency cells for RedCap UEs when this cell is barred\n"

#define GNB_CONFIG_STRING_REDCAP                            "RedCap"
#define GNB_CONFIG_STRING_CELL_BARRED_REDCAP1_RX_R17        "cellBarredRedCap1Rx_r17"
#define GNB_CONFIG_STRING_CELL_BARRED_REDCAP2_RX_R17        "cellBarredRedCap2Rx_r17"
#define GNB_CONFIG_STRING_INTRA_FREQ_RESELECTION_REDCAP_R17 "intraFreqReselectionRedCap_r17"

#define GNB_REDCAP_PARAMS_DESC { \
{GNB_CONFIG_STRING_CELL_BARRED_REDCAP1_RX_R17,        GNB_CONFIG_HLP_STRING_CELL_BARRED_REDCAP1_RX_R17,             0,        .i8ptr=NULL,     .defintval=-1,      TYPE_INT8,      0},\
{GNB_CONFIG_STRING_CELL_BARRED_REDCAP2_RX_R17,        GNB_CONFIG_HLP_STRING_CELL_BARRED_REDCAP2_RX_R17,             0,        .i8ptr=NULL,     .defintval=-1,      TYPE_INT8,      0},\
{GNB_CONFIG_STRING_INTRA_FREQ_RESELECTION_REDCAP_R17, GNB_CONFIG_HLP_STRING_INTRA_FREQ_RESELECTION_REDCAP_R17,      0,        .u8ptr=NULL,     .defuintval=0,      TYPE_UINT8,     0},\
}

#define GNB_REDCAP_CELL_BARRED_REDCAP1_RX_R17_IDX            0
#define GNB_REDCAP_CELL_BARRED_REDCAP2_RX_R17_IDX            1
#define GNB_REDCAP_INTRA_FREQ_RESELECTION_REDCAP_R17_IDX     2

/*-------------------------------------------------------------------------------------------------------------------------------------*/
/*                                            PTRS configuration parameters                                                          */
/*-------------------------------------------------------------------------------------------------------------------------------------*/

#define GNB_CONFIG_STRING_PTRS                                           "phaseTrackingRS"
#define GNB_CONFIG_STRING_DLPTRSFREQDENSITY0_0                           "dl_ptrsFreqDensity0_0"
#define GNB_CONFIG_STRING_DLPTRSFREQDENSITY1_0                           "dl_ptrsFreqDensity1_0"
#define GNB_CONFIG_STRING_DLPTRSTIMEDENSITY0_0                           "dl_ptrsTimeDensity0_0"
#define GNB_CONFIG_STRING_DLPTRSTIMEDENSITY1_0                           "dl_ptrsTimeDensity1_0"
#define GNB_CONFIG_STRING_DLPTRSTIMEDENSITY2_0                           "dl_ptrsTimeDensity2_0"
#define GNB_CONFIG_STRING_DLPTRSEPRERATIO_0                              "dl_ptrsEpreRatio_0"
#define GNB_CONFIG_STRING_DLPTRSREOFFSET_0                               "dl_ptrsReOffset_0"
#define GNB_CONFIG_STRING_ULPTRSFREQDENSITY0_0                           "ul_ptrsFreqDensity0_0"
#define GNB_CONFIG_STRING_ULPTRSFREQDENSITY1_0                           "ul_ptrsFreqDensity1_0"
#define GNB_CONFIG_STRING_ULPTRSTIMEDENSITY0_0                           "ul_ptrsTimeDensity0_0"
#define GNB_CONFIG_STRING_ULPTRSTIMEDENSITY1_0                           "ul_ptrsTimeDensity1_0"
#define GNB_CONFIG_STRING_ULPTRSTIMEDENSITY2_0                           "ul_ptrsTimeDensity2_0"
#define GNB_CONFIG_STRING_ULPTRSREOFFSET_0                               "ul_ptrsReOffset_0"
#define GNB_CONFIG_STRING_ULPTRSMAXPORTS_0                               "ul_ptrsMaxPorts_0"
#define GNB_CONFIG_STRING_ULPTRSPOWER_0                                  "ul_ptrsPower_0"

#define GNB_PTRS_PARAMS_DESC { \
{GNB_CONFIG_STRING_DLPTRSFREQDENSITY0_0,   NULL,  0,  .iptr=NULL,  .defintval=0,  TYPE_INT, 0}, \
{GNB_CONFIG_STRING_DLPTRSFREQDENSITY1_0,   NULL,  0,  .iptr=NULL,  .defintval=0,  TYPE_INT, 0}, \
{GNB_CONFIG_STRING_DLPTRSTIMEDENSITY0_0,   NULL,  0,  .iptr=NULL,  .defintval=-1, TYPE_INT, 0}, \
{GNB_CONFIG_STRING_DLPTRSTIMEDENSITY1_0,   NULL,  0,  .iptr=NULL,  .defintval=-1, TYPE_INT, 0}, \
{GNB_CONFIG_STRING_DLPTRSTIMEDENSITY2_0,   NULL,  0,  .iptr=NULL,  .defintval=-1, TYPE_INT, 0}, \
{GNB_CONFIG_STRING_DLPTRSEPRERATIO_0,      NULL,  0,  .iptr=NULL,  .defintval=-1, TYPE_INT, 0}, \
{GNB_CONFIG_STRING_DLPTRSREOFFSET_0,       NULL,  0,  .iptr=NULL,  .defintval=-1, TYPE_INT, 0}, \
{GNB_CONFIG_STRING_ULPTRSFREQDENSITY0_0,   NULL,  0,  .iptr=NULL,  .defintval=0,  TYPE_INT, 0}, \
{GNB_CONFIG_STRING_ULPTRSFREQDENSITY1_0,   NULL,  0,  .iptr=NULL,  .defintval=0,  TYPE_INT, 0}, \
{GNB_CONFIG_STRING_ULPTRSTIMEDENSITY0_0,   NULL,  0,  .iptr=NULL,  .defintval=-1, TYPE_INT, 0}, \
{GNB_CONFIG_STRING_ULPTRSTIMEDENSITY1_0,   NULL,  0,  .iptr=NULL,  .defintval=-1, TYPE_INT, 0}, \
{GNB_CONFIG_STRING_ULPTRSTIMEDENSITY2_0,   NULL,  0,  .iptr=NULL,  .defintval=-1, TYPE_INT, 0}, \
{GNB_CONFIG_STRING_ULPTRSREOFFSET_0,       NULL,  0,  .iptr=NULL,  .defintval=-1, TYPE_INT, 0}, \
{GNB_CONFIG_STRING_ULPTRSMAXPORTS_0,       NULL,  0,  .iptr=NULL,  .defintval=0,  TYPE_INT, 0}, \
{GNB_CONFIG_STRING_ULPTRSPOWER_0,          NULL,  0,  .iptr=NULL,  .defintval=0,  TYPE_INT, 0}}

#define GNB_DLPTRSFREQDENSITY0_0_IDX   0
#define GNB_DLPTRSFREQDENSITY1_0_IDX   1
#define GNB_DLPTRSTIMEDENSITY0_0_IDX   2
#define GNB_DLPTRSTIMEDENSITY1_0_IDX   3
#define GNB_DLPTRSTIMEDENSITY2_0_IDX   4
#define GNB_DLPTRSEPRERATIO_0_IDX      5
#define GNB_DLPTRSREOFFSET_0_IDX       6
#define GNB_ULPTRSFREQDENSITY0_0_IDX   7
#define GNB_ULPTRSFREQDENSITY1_0_IDX   8
#define GNB_ULPTRSTIMEDENSITY0_0_IDX   9
#define GNB_ULPTRSTIMEDENSITY1_0_IDX  10
#define GNB_ULPTRSTIMEDENSITY2_0_IDX  11
#define GNB_ULPTRSREOFFSET_0_IDX      12
#define GNB_ULPTRSMAXPORTS_0_IDX      13
#define GNB_ULPTRSPOWER_0_IDX         14

/*---------------------------------------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------------------------------------*/
/* SCTP configuration parameters section name */
#define GNB_CONFIG_STRING_SCTP_CONFIG                    "SCTP"

/* SCTP configuration parameters names   */
#define GNB_CONFIG_STRING_SCTP_INSTREAMS                 "SCTP_INSTREAMS"
#define GNB_CONFIG_STRING_SCTP_OUTSTREAMS                "SCTP_OUTSTREAMS"



/*-----------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*                                            SRB1 configuration parameters                                                                                  */
/*   optname                                          helpstr   paramflags    XXXptr                             defXXXval         type           numelt     */
/*-----------------------------------------------------------------------------------------------------------------------------------------------------------*/
#define GNBSCTPPARAMS_DESC {  \
{GNB_CONFIG_STRING_SCTP_INSTREAMS,                       NULL,   0,   .uptr=NULL,   .defintval=-1,    TYPE_UINT,   0},       \
{GNB_CONFIG_STRING_SCTP_OUTSTREAMS,                      NULL,   0,   .uptr=NULL,   .defintval=-1,    TYPE_UINT,   0}        \
}

#define GNB_SCTP_INSTREAMS_IDX          0
#define GNB_SCTP_OUTSTREAMS_IDX         1
/*-----------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* S1 interface configuration parameters section name */
#define GNB_CONFIG_STRING_NETWORK_INTERFACES_CONFIG     "NETWORK_INTERFACES"

#define GNB_IPV4_ADDRESS_FOR_NG_AMF_IDX            0
#define GNB_IPV4_ADDR_FOR_NGU_IDX                  1
#define GNB_PORT_FOR_NGU_IDX                       2
#define GNB_IPV4_ADDR_FOR_X2C_IDX                  3
#define GNB_PORT_FOR_X2C_IDX                       4

/* S1 interface configuration parameters names   */
#define GNB_CONFIG_STRING_GNB_IPV4_ADDRESS_FOR_S1U      "GNB_IPV4_ADDRESS_FOR_S1U"
#define GNB_CONFIG_STRING_GNB_PORT_FOR_S1U              "GNB_PORT_FOR_S1U"

#define GNB_CONFIG_STRING_GNB_IPV4_ADDRESS_FOR_NG_AMF   "GNB_IPV4_ADDRESS_FOR_NG_AMF"
#define GNB_CONFIG_STRING_GNB_IPV4_ADDR_FOR_NGU         "GNB_IPV4_ADDRESS_FOR_NGU"
#define GNB_CONFIG_STRING_GNB_PORT_FOR_NGU              "GNB_PORT_FOR_NGU"

/* X2 interface configuration parameters names */
#define GNB_CONFIG_STRING_ENB_IPV4_ADDR_FOR_X2C	        "GNB_IPV4_ADDRESS_FOR_X2C"
#define GNB_CONFIG_STRING_ENB_PORT_FOR_X2C				"GNB_PORT_FOR_X2C"

/*--------------------------------------------------------------------------------------------------------------------------------------------------*/
/*                                            S1 interface configuration parameters                                                                 */
/*   optname                                            helpstr   paramflags    XXXptr              defXXXval             type           numelt     */
/*--------------------------------------------------------------------------------------------------------------------------------------------------*/
#define GNBNETPARAMS_DESC {  \
      {GNB_CONFIG_STRING_GNB_IPV4_ADDRESS_FOR_NG_AMF,        NULL,      0,        .strptr=NULL,        .defstrval=NULL,      TYPE_STRING,      0}, \
      {GNB_CONFIG_STRING_GNB_IPV4_ADDR_FOR_NGU,              NULL,      0,        .strptr=&gnb_ipv4_address_for_NGU, .defstrval="127.0.0.1",TYPE_STRING,   0},	\
      {GNB_CONFIG_STRING_GNB_PORT_FOR_NGU,                   NULL,      0,        .uptr=&gnb_port_for_NGU,           .defintval=2152L,      TYPE_UINT,     0},	\
      {GNB_CONFIG_STRING_ENB_IPV4_ADDR_FOR_X2C,              NULL,      0,        .strptr=NULL,                      .defstrval=NULL,       TYPE_STRING,   0},	\
      {GNB_CONFIG_STRING_ENB_PORT_FOR_X2C,                   NULL,      0,        .uptr=NULL,                        .defintval=0L,         TYPE_UINT,     0}, \
      {GNB_CONFIG_STRING_GNB_IPV4_ADDRESS_FOR_S1U,           NULL,      0,        .strptr=&gnb_ipv4_address_for_S1U, .defstrval="127.0.0.1",TYPE_STRING,   0}, \
      {GNB_CONFIG_STRING_GNB_PORT_FOR_S1U,                   NULL,      0,        .uptr=&gnb_port_for_S1U,           .defintval=2152L,       TYPE_UINT,     0}	\
  }
/*-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

/* Xn configuration section */
#define GNB_CONFIG_STRING_XN_PARAMETERS                              "Xn_INTERFACE"

#define GNB_CONFIG_XN_ENABLE_IDX                                     0
#define GNB_CONFIG_STRING_GNB_IPV4_ADDRESS_FOR_XNC_IDX               1
#define GNB_CONFIG_STRING_GNB_PORT_FOR_XNC_IDX                       2

#define GNB_CONFIG_XN_ENABLE                                         "enable_xn"
#define GNB_CONFIG_STRING_GNB_IPV4_ADDRESS_FOR_XNC                   "gnb_ipv4_address_for_xnc"
#define GNB_CONFIG_STRING_GNB_PORT_FOR_XNC                           "gnb_port_for_xnc"

#define XNPARAMS_DESC { \
  {GNB_CONFIG_XN_ENABLE,                       "enable or disable xn interface", PARAMFLAG_BOOL,  .iptr=NULL,   .defstrval=0,      TYPE_INT,    0}, \
  {GNB_CONFIG_STRING_GNB_IPV4_ADDRESS_FOR_XNC, "interface ip address for xnc",   0,               .strptr=NULL, .defstrval=0,      TYPE_STRING, 0}, \
  {GNB_CONFIG_STRING_GNB_PORT_FOR_XNC,         "port for xnc",                   0,               .uptr=NULL,   .defstrval=0L,     TYPE_UINT,   0}, \
}

#define GNB_CONFIG_STRING_CANDIDATE_GNB_IPV4_ADDRESS_FOR_XNC         "candidate_gnb_ipv4_address_for_xnc"

#define GNB_CONFIG_STRING_CANDIDATE_GNB_IPV4_ADDRESS_FOR_XNC_IDX     0

#define GNB_CONFIG_STRING_CANDIDATE_GNB_ADDRESS_FOR_XNC         "ip"

#define XN_CANDIDATE_PARAMS_DESC { \
  {GNB_CONFIG_STRING_CANDIDATE_GNB_ADDRESS_FOR_XNC, "candidate node ip address for xnc", 0, .strptr=NULL, .defstrval=0, TYPE_STRING, 0}, \
}

/* E1 configuration section */
#define GNB_CONFIG_STRING_E1_PARAMETERS                   "E1_INTERFACE"

#define GNB_CONFIG_E1_CU_TYPE_IDX                         0
#define GNB_CONFIG_E1_IPV4_ADDRESS_CUCP                   1
#define GNB_CONFIG_E1_IPV4_ADDRESS_CUUP 2

#define GNB_CONFIG_STRING_E1_CU_TYPE                      "type"
#define GNB_CONFIG_STRING_E1_IPV4_ADDRESS_CUCP "ipv4_cucp"
#define GNB_CONFIG_STRING_E1_IPV4_ADDRESS_CUUP "ipv4_cuup"

// clang-format off
#define GNBE1PARAMS_DESC { \
  {GNB_CONFIG_STRING_E1_CU_TYPE,           NULL, 0, .strptr=NULL, .defstrval=NULL, TYPE_STRING, 0}, \
  {GNB_CONFIG_STRING_E1_IPV4_ADDRESS_CUCP, NULL, 0, .strptr=NULL, .defstrval=NULL, TYPE_STRING, 0}, \
  {GNB_CONFIG_STRING_E1_IPV4_ADDRESS_CUUP, NULL, 0, .strptr=NULL, .defstrval=NULL, TYPE_STRING, 0}, \
  }
// clang-format on
/* L1 configuration section names   */
#define CONFIG_STRING_L1_LIST                              "L1s"


/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* security configuration                                                                                                                                                           */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
#define CONFIG_STRING_SECURITY             "security"

#define SECURITY_CONFIG_CIPHERING          "ciphering_algorithms"
#define SECURITY_CONFIG_INTEGRITY          "integrity_algorithms"
#define SECURITY_CONFIG_DO_DRB_CIPHERING   "drb_ciphering"
#define SECURITY_CONFIG_DO_DRB_INTEGRITY   "drb_integrity"

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*   security configuration                                                                                                                                                         */
/*   optname                               help                                          paramflags         XXXptr               defXXXval                 type              numelt */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
#define SECURITY_GLOBALPARAMS_DESC { \
    {SECURITY_CONFIG_CIPHERING,            "preferred ciphering algorithms\n",            0,               .strlistptr=NULL,     .defstrlistval=NULL,       TYPE_STRINGLIST,  0}, \
    {SECURITY_CONFIG_INTEGRITY,            "preferred integrity algorithms\n",            0,               .strlistptr=NULL,     .defstrlistval=NULL,       TYPE_STRINGLIST,  0}, \
    {SECURITY_CONFIG_DO_DRB_CIPHERING,     "use ciphering for DRBs",                      0,               .strptr=NULL,         .defstrval="yes",          TYPE_STRING,      0}, \
    {SECURITY_CONFIG_DO_DRB_INTEGRITY,     "use integrity for DRBs",                      0,               .strptr=NULL,         .defstrval="no",           TYPE_STRING,      0}, \
}

#define SECURITY_CONFIG_CIPHERING_IDX          0
#define SECURITY_CONFIG_INTEGRITY_IDX          1
#define SECURITY_CONFIG_DO_DRB_CIPHERING_IDX   2
#define SECURITY_CONFIG_DO_DRB_INTEGRITY_IDX   3

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

#define CONFIG_STRING_NR_RLC_LIST "rlc"
#define CONFIG_STRING_NR_PDCP_LIST "pdcp"

#define CONFIG_NR_RLC_T_POLL_RETRANSMIT "t_poll_retransmit"
#define CONFIG_NR_RLC_T_REASSEMBLY "t_reassembly"
#define CONFIG_NR_RLC_T_STATUS_PROHIBIT "t_status_prohibit"
#define CONFIG_NR_RLC_POLL_PDU "poll_pdu"
#define CONFIG_NR_RLC_POLL_BYTE "poll_byte"
#define CONFIG_NR_RLC_MAX_RETX_THRESHOLD "max_retx_threshold"
#define CONFIG_NR_RLC_SN_FIELD_LENGTH "sn_field_length"

/*----------------------------------------------------------------------*/
/* nr rlc srb configuration                                             */
/*----------------------------------------------------------------------*/

#define CONFIG_STRING_NR_RLC_SRB "rlc.srb"

#define CONFIG_NR_RLC_SRB_T_POLL_RETRANSMIT_IDX 0
#define CONFIG_NR_RLC_SRB_T_REASSEMBLY_IDX 1
#define CONFIG_NR_RLC_SRB_T_STATUS_PROHIBIT_IDX 2
#define CONFIG_NR_RLC_SRB_POLL_PDU_IDX 3
#define CONFIG_NR_RLC_SRB_POLL_BYTE_IDX 4
#define CONFIG_NR_RLC_SRB_MAX_RETX_THRESHOLD_IDX 5
#define CONFIG_NR_RLC_SRB_SN_FIELD_LENGTH_IDX 6

#define NR_RLC_SRB_GLOBALPARAMS_DESC { \
    { .optname = CONFIG_NR_RLC_T_POLL_RETRANSMIT, \
      .defstrval = "ms45", \
      .helpstr = "poll retransmit timer", .paramflags = 0, .strptr = NULL, .type = TYPE_STRING, .numelt = 0, \
      .chkPptr = &(checkedparam_t){ .s3a = { .f3a = config_checkstr_assign_integer, \
          .okstrval = { VALUES_NR_RLC_T_POLL_RETRANSMIT_STR }, \
          .setintval = { VALUES_NR_RLC_T_POLL_RETRANSMIT }, \
          .num_okstrval = SIZEOF_NR_RLC_T_POLL_RETRANSMIT }}}, \
    { .optname = CONFIG_NR_RLC_T_REASSEMBLY, \
      .defstrval = "ms35", \
      .helpstr = "reassembly timer", .paramflags = 0, .strptr = NULL, .type = TYPE_STRING, .numelt = 0, \
      .chkPptr = &(checkedparam_t){ .s3a = { .f3a = config_checkstr_assign_integer, \
          .okstrval = { VALUES_NR_RLC_T_REASSEMBLY_STR }, \
          .setintval = { VALUES_NR_RLC_T_REASSEMBLY }, \
          .num_okstrval = SIZEOF_NR_RLC_T_REASSEMBLY }}}, \
    { .optname = CONFIG_NR_RLC_T_STATUS_PROHIBIT, \
      .defstrval = "ms0", \
      .helpstr = "status prohibit timer", .paramflags = 0, .strptr = NULL, .type = TYPE_STRING, .numelt = 0, \
      .chkPptr = &(checkedparam_t){ .s3a = { .f3a = config_checkstr_assign_integer, \
          .okstrval = { VALUES_NR_RLC_T_STATUS_PROHIBIT_STR }, \
          .setintval = { VALUES_NR_RLC_T_STATUS_PROHIBIT }, \
          .num_okstrval = SIZEOF_NR_RLC_T_STATUS_PROHIBIT }}}, \
    { .optname = CONFIG_NR_RLC_POLL_PDU, \
      .defstrval = "infinity", \
      .helpstr = "pollPDU", .paramflags = 0, .strptr = NULL, .type = TYPE_STRING, .numelt = 0, \
      .chkPptr = &(checkedparam_t){ .s3a = { .f3a = config_checkstr_assign_integer, \
          .okstrval = { VALUES_NR_RLC_POLL_PDU_STR }, \
          .setintval = { VALUES_NR_RLC_POLL_PDU }, \
          .num_okstrval = SIZEOF_NR_RLC_POLL_PDU }}}, \
    { .optname = CONFIG_NR_RLC_POLL_BYTE, \
      .defstrval = "infinity", \
      .helpstr = "pollByte", .paramflags = 0, .strptr = NULL, .type = TYPE_STRING, .numelt = 0, \
      .chkPptr = &(checkedparam_t){ .s3a = { .f3a = config_checkstr_assign_integer, \
          .okstrval = { VALUES_NR_RLC_POLL_BYTE_STR }, \
          .setintval = { VALUES_NR_RLC_POLL_BYTE }, \
          .num_okstrval = SIZEOF_NR_RLC_POLL_BYTE }}}, \
    { .optname = CONFIG_NR_RLC_MAX_RETX_THRESHOLD, \
      .defstrval = "t8", \
      .helpstr = "max reTX threshold", .paramflags = 0, .strptr = NULL, .type = TYPE_STRING, .numelt = 0, \
      .chkPptr = &(checkedparam_t){ .s3a = { .f3a = config_checkstr_assign_integer, \
          .okstrval = { VALUES_NR_RLC_MAX_RETX_THRESHOLD_STR }, \
          .setintval = { VALUES_NR_RLC_MAX_RETX_THRESHOLD }, \
          .num_okstrval = SIZEOF_NR_RLC_MAX_RETX_THRESHOLD }}}, \
    { .optname = CONFIG_NR_RLC_SN_FIELD_LENGTH, \
      .defstrval = "size12", \
      .helpstr = "SN size", .paramflags = 0, .strptr = NULL, .type = TYPE_STRING, .numelt = 0, \
      .chkPptr = &(checkedparam_t){ .s3a = { .f3a = config_checkstr_assign_integer, \
          .okstrval = { VALUES_NR_RLC_SN_FIELD_LENGTH_AM_STR }, \
          .setintval = { VALUES_NR_RLC_SN_FIELD_LENGTH_AM }, \
          .num_okstrval = SIZEOF_NR_RLC_SN_FIELD_LENGTH_AM }}}, \
}

/*----------------------------------------------------------------------*/
/* nr rlc drb am configuration                                          */
/*----------------------------------------------------------------------*/

#define CONFIG_STRING_NR_RLC_DRB_AM "rlc.drb_am"

#define CONFIG_NR_RLC_DRB_AM_T_POLL_RETRANSMIT_IDX 0
#define CONFIG_NR_RLC_DRB_AM_T_REASSEMBLY_IDX 1
#define CONFIG_NR_RLC_DRB_AM_T_STATUS_PROHIBIT_IDX 2
#define CONFIG_NR_RLC_DRB_AM_POLL_PDU_IDX 3
#define CONFIG_NR_RLC_DRB_AM_POLL_BYTE_IDX 4
#define CONFIG_NR_RLC_DRB_AM_MAX_RETX_THRESHOLD_IDX 5
#define CONFIG_NR_RLC_DRB_AM_SN_FIELD_LENGTH_IDX 6

#define NR_RLC_DRB_AM_GLOBALPARAMS_DESC { \
    { .optname = CONFIG_NR_RLC_T_POLL_RETRANSMIT, \
      .defstrval = "ms45", \
      .helpstr = "poll retransmit timer", .paramflags = 0, .strptr = NULL, .type = TYPE_STRING, .numelt = 0, \
      .chkPptr = &(checkedparam_t){ .s3a = { .f3a = config_checkstr_assign_integer, \
          .okstrval = { VALUES_NR_RLC_T_POLL_RETRANSMIT_STR }, \
          .setintval = { VALUES_NR_RLC_T_POLL_RETRANSMIT }, \
          .num_okstrval = SIZEOF_NR_RLC_T_POLL_RETRANSMIT }}}, \
    { .optname = CONFIG_NR_RLC_T_REASSEMBLY, \
      .defstrval = "ms15", \
      .helpstr = "reassembly timer", .paramflags = 0, .strptr = NULL, .type = TYPE_STRING, .numelt = 0, \
      .chkPptr = &(checkedparam_t){ .s3a = { .f3a = config_checkstr_assign_integer, \
          .okstrval = { VALUES_NR_RLC_T_REASSEMBLY_STR }, \
          .setintval = { VALUES_NR_RLC_T_REASSEMBLY }, \
          .num_okstrval = SIZEOF_NR_RLC_T_REASSEMBLY }}}, \
    { .optname = CONFIG_NR_RLC_T_STATUS_PROHIBIT, \
      .defstrval = "ms15", \
      .helpstr = "status prohibit timer", .paramflags = 0, .strptr = NULL, .type = TYPE_STRING, .numelt = 0, \
      .chkPptr = &(checkedparam_t){ .s3a = { .f3a = config_checkstr_assign_integer, \
          .okstrval = { VALUES_NR_RLC_T_STATUS_PROHIBIT_STR }, \
          .setintval = { VALUES_NR_RLC_T_STATUS_PROHIBIT }, \
          .num_okstrval = SIZEOF_NR_RLC_T_STATUS_PROHIBIT }}}, \
    { .optname = CONFIG_NR_RLC_POLL_PDU, \
      .defstrval = "p64", \
      .helpstr = "pollPDU", .paramflags = 0, .strptr = NULL, .type = TYPE_STRING, .numelt = 0, \
      .chkPptr = &(checkedparam_t){ .s3a = { .f3a = config_checkstr_assign_integer, \
          .okstrval = { VALUES_NR_RLC_POLL_PDU_STR }, \
          .setintval = { VALUES_NR_RLC_POLL_PDU }, \
          .num_okstrval = SIZEOF_NR_RLC_POLL_PDU }}}, \
    { .optname = CONFIG_NR_RLC_POLL_BYTE, \
      .defstrval = "kB500", \
      .helpstr = "pollByte", .paramflags = 0, .strptr = NULL, .type = TYPE_STRING, .numelt = 0, \
      .chkPptr = &(checkedparam_t){ .s3a = { .f3a = config_checkstr_assign_integer, \
          .okstrval = { VALUES_NR_RLC_POLL_BYTE_STR }, \
          .setintval = { VALUES_NR_RLC_POLL_BYTE }, \
          .num_okstrval = SIZEOF_NR_RLC_POLL_BYTE }}}, \
    { .optname = CONFIG_NR_RLC_MAX_RETX_THRESHOLD, \
      .defstrval = "t32", \
      .helpstr = "max reTX threshold", .paramflags = 0, .strptr = NULL, .type = TYPE_STRING, .numelt = 0, \
      .chkPptr = &(checkedparam_t){ .s3a = { .f3a = config_checkstr_assign_integer, \
          .okstrval = { VALUES_NR_RLC_MAX_RETX_THRESHOLD_STR }, \
          .setintval = { VALUES_NR_RLC_MAX_RETX_THRESHOLD }, \
          .num_okstrval = SIZEOF_NR_RLC_MAX_RETX_THRESHOLD }}}, \
    { .optname = CONFIG_NR_RLC_SN_FIELD_LENGTH, \
      .defstrval = "size18", \
      .helpstr = "SN size", .paramflags = 0, .strptr = NULL, .type = TYPE_STRING, .numelt = 0, \
      .chkPptr = &(checkedparam_t){ .s3a = { .f3a = config_checkstr_assign_integer, \
          .okstrval = { VALUES_NR_RLC_SN_FIELD_LENGTH_AM_STR }, \
          .setintval = { VALUES_NR_RLC_SN_FIELD_LENGTH_AM }, \
          .num_okstrval = SIZEOF_NR_RLC_SN_FIELD_LENGTH_AM }}}, \
}

/*----------------------------------------------------------------------*/
/* nr rlc drb um configuration                                          */
/*----------------------------------------------------------------------*/

#define CONFIG_STRING_NR_RLC_DRB_UM "rlc.drb_um"

#define CONFIG_NR_RLC_DRB_UM_T_REASSEMBLY_IDX 0
#define CONFIG_NR_RLC_DRB_UM_SN_FIELD_LENGTH_IDX 1

#define NR_RLC_DRB_UM_GLOBALPARAMS_DESC { \
    { .optname = CONFIG_NR_RLC_T_REASSEMBLY, \
      .defstrval = "ms15", \
      .helpstr = "reassembly timer", .paramflags = 0, .strptr = NULL, .type = TYPE_STRING, .numelt = 0, \
      .chkPptr = &(checkedparam_t){ .s3a = { .f3a = config_checkstr_assign_integer, \
          .okstrval = { VALUES_NR_RLC_T_REASSEMBLY_STR }, \
          .setintval = { VALUES_NR_RLC_T_REASSEMBLY }, \
          .num_okstrval = SIZEOF_NR_RLC_T_REASSEMBLY }}}, \
    { .optname = CONFIG_NR_RLC_SN_FIELD_LENGTH, \
      .defstrval = "size12", \
      .helpstr = "SN size", .paramflags = 0, .strptr = NULL, .type = TYPE_STRING, .numelt = 0, \
      .chkPptr = &(checkedparam_t){ .s3a = { .f3a = config_checkstr_assign_integer, \
          .okstrval = { VALUES_NR_RLC_SN_FIELD_LENGTH_UM_STR }, \
          .setintval = { VALUES_NR_RLC_SN_FIELD_LENGTH_UM }, \
          .num_okstrval = SIZEOF_NR_RLC_SN_FIELD_LENGTH_UM }}}, \
}

/*----------------------------------------------------------------------*/

#define CONFIG_NR_PDCP_SN_SIZE "sn_size"
#define CONFIG_NR_PDCP_T_REORDERING "t_reordering"
#define CONFIG_NR_PDCP_DISCARD_TIMER "discard_timer"

/*----------------------------------------------------------------------*/
/* nr pdcp drb configuration                                            */
/*----------------------------------------------------------------------*/

#define CONFIG_STRING_NR_PDCP_DRB "pdcp.drb"

#define CONFIG_NR_PDCP_DRB_SN_SIZE_IDX 0
#define CONFIG_NR_PDCP_DRB_T_REORDERING_IDX 1
#define CONFIG_NR_PDCP_DRB_DISCARD_TIMER_IDX 2

#define NR_PDCP_DRB_GLOBALPARAMS_DESC { \
    { .optname = CONFIG_NR_PDCP_SN_SIZE, \
      .defstrval = "len18bits", \
      .helpstr = "SN size", .paramflags = 0, .strptr = NULL, .type = TYPE_STRING, .numelt = 0, \
      .chkPptr = &(checkedparam_t){ .s3a = { .f3a = config_checkstr_assign_integer, \
          .okstrval = { VALUES_NR_PDCP_SN_SIZE_STR }, \
          .setintval = { VALUES_NR_PDCP_SN_SIZE }, \
          .num_okstrval = SIZEOF_NR_PDCP_SN_SIZE }}}, \
    { .optname = CONFIG_NR_PDCP_T_REORDERING, \
      .defstrval = "ms100", \
      .helpstr = "reordering timer", .paramflags = 0, .strptr = NULL, .type = TYPE_STRING, .numelt = 0, \
      .chkPptr = &(checkedparam_t){ .s3a = { .f3a = config_checkstr_assign_integer, \
          .okstrval = { VALUES_NR_PDCP_T_REORDERING_STR }, \
          .setintval = { VALUES_NR_PDCP_T_REORDERING }, \
          .num_okstrval = SIZEOF_NR_PDCP_T_REORDERING }}}, \
    { .optname = CONFIG_NR_PDCP_DISCARD_TIMER, \
      .defstrval = "infinity", \
      .helpstr = "discard timer", .paramflags = 0, .strptr = NULL, .type = TYPE_STRING, .numelt = 0, \
      .chkPptr = &(checkedparam_t){ .s3a = { .f3a = config_checkstr_assign_integer,  \
          .okstrval = { VALUES_NR_PDCP_DISCARD_TIMER_STR }, \
          .setintval = { VALUES_NR_PDCP_DISCARD_TIMER }, \
          .num_okstrval = SIZEOF_NR_PDCP_DISCARD_TIMER }}} \
}

/*----------------------------------------------------------------------*/

#endif
