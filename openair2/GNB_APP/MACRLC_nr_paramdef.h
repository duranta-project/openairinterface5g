/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief definition of configuration parameters for all gNodeB modules
 */

#ifndef __GNB_APP_MACRLC_NR_PARAMDEF__H__
#define __GNB_APP_MACRLC_NR_PARAMDEF__H__

/*----------------------------------------------------------------------------------------------------------------------------------------------------*/

/* MACRLC configuration section names   */
#define MACRLC_LIST                          "MACRLCs"

/* MACRLC configuration parameters names   */
#define MACRLC_TRANSPORT_N_PREFERENCE        "tr_n_preference"
#define MACRLC_LOCAL_N_ADDRESS               "local_n_address"
#define MACRLC_LOCAL_N_ADDRESS_F1U           "local_n_address_f1u"
#define MACRLC_REMOTE_N_ADDRESS              "remote_n_address"
#define MACRLC_LOCAL_N_PORTD                 "local_n_portd"
#define MACRLC_REMOTE_N_PORTD                "remote_n_portd"
#define MACRLC_TRANSPORT_S_PREFERENCE        "tr_s_preference"
#define MACRLC_TRANSPORT_S_SHM_PREFIX "tr_s_shm_prefix"
#define MACRLC_TRANSPORT_S_POLL_CORE "tr_s_poll_core"
#define MACRLC_LOCAL_S_ADDRESS               "local_s_address"
#define MACRLC_REMOTE_S_ADDRESS              "remote_s_address"
#define MACRLC_LOCAL_S_PORTC                 "local_s_portc"
#define MACRLC_LOCAL_S_PORTD                 "local_s_portd"
#define MACRLC_ULSCH_MAX_FRAME_INACTIVITY    "ulsch_max_frame_inactivity"
#define MACRLC_PUSCHTARGETSNRX10             "pusch_TargetSNRx10"
#define MACRLC_PUCCHTARGETSNRX10             "pucch_TargetSNRx10"
#define MACRLC_UL_PRBBLACK_SNR_THRESHOLD     "ul_prbblack_SNR_threshold"
#define MACRLC_PUCCHFAILURETHRES             "pucch_FailureThres"
#define MACRLC_PUSCHFAILURETHRES             "pusch_FailureThres"
#define MACRLC_DL_BLER_TARGET_UPPER          "dl_bler_target_upper"
#define MACRLC_DL_BLER_TARGET_LOWER          "dl_bler_target_lower"
#define MACRLC_DL_MIN_MCS                    "dl_min_mcs"
#define MACRLC_DL_MAX_MCS                    "dl_max_mcs"
#define MACRLC_UL_BLER_TARGET_UPPER          "ul_bler_target_upper"
#define MACRLC_UL_BLER_TARGET_LOWER          "ul_bler_target_lower"
#define MACRLC_UL_MIN_MCS                    "ul_min_mcs"
#define MACRLC_UL_MAX_MCS                    "ul_max_mcs"
#define MACRLC_DL_HARQ_ROUND_MAX             "dl_harq_round_max"
#define MACRLC_UL_HARQ_ROUND_MAX             "ul_harq_round_max"
#define MACRLC_MIN_GRANT_PRB                 "min_grant_prb"
#define MACRLC_IDENTITY_PM                   "identity_precoding_matrix"
#define MACRLC_ANALOG_BEAMFORMING            "set_analog_beamforming"
#define MACRLC_BEAM_DURATION                 "beam_duration"
#define MACRLC_BEAMS_PERIOD                  "beams_per_period"
#define MACRLC_BEAM_WEIGHTS_LIST             "beam_weights"
#define MACRLC_DBT_FILE                      "dbt_file"
#define MACRLC_PUSCH_RSSI_THRESHOLD          "pusch_RSSI_Threshold"
#define MACRLC_PUCCH_RSSI_THRESHOLD          "pucch_RSSI_Threshold"
#define MACRLC_STATS_MAX_UE                  "stats_max_ue"
#define MACRLC_DL_PREPROCESSOR_POLICY        "dl_preprocessor_policy"
#define MACRLC_DL_RI_PMI_SELECT_POLICY       "dl_ri_pmi_select_policy"
#define MACRLC_DL_TDA_SELECT_POLICY          "dl_tda_select_policy"
#define MACRLC_DL_BEAM_SELECT_POLICY         "dl_beam_select_policy"
#define MACRLC_DL_MCS_SELECT_POLICY          "dl_mcs_select_policy"
#define MACRLC_DL_RB_ALLOC_POLICY            "dl_rb_alloc_policy"
#define MACRLC_DL_LCID_ALLOC_POLICY          "dl_lcid_alloc_policy"
#define MACRLC_UL_PREPROCESSOR_POLICY        "ul_preprocessor_policy"
#define MACRLC_UL_RI_TPMI_SELECT_POLICY      "ul_ri_tpmi_select_policy"
#define MACRLC_UL_TDA_SELECT_POLICY          "ul_tda_select_policy"
#define MACRLC_UL_BEAM_SELECT_POLICY         "ul_beam_select_policy"
#define MACRLC_UL_MCS_SELECT_POLICY          "ul_mcs_select_policy"
#define MACRLC_UL_RB_ALLOC_POLICY            "ul_rb_alloc_policy"
#define MACRLC_DL_HARQ_RESULT_OBSERVERS      "dl_harq_result_observers"
#define MACRLC_UL_HARQ_RESULT_OBSERVERS      "ul_harq_result_observers"
#define MACRLC_SPATIAL_STREAM_IDX            "spatial_stream_index"

#define HLP_MACRLC_UL_PRBBLACK "SNR threshold to decide whether a PRB will be blacklisted or not"
#define HLP_MACRLC_DL_BLER_UP "Upper threshold of BLER to decrease DL MCS"
#define HLP_MACRLC_DL_BLER_LO "Lower threshold of BLER to increase DL MCS"
#define HLP_MACRLC_DL_MIN_MCS "Minimum DL MCS that should be used"
#define HLP_MACRLC_DL_MAX_MCS "Maximum DL MCS that should be used"
#define HLP_MACRLC_UL_BLER_UP "Upper threshold of BLER to decrease UL MCS"
#define HLP_MACRLC_UL_BLER_LO "Lower threshold of BLER to increase UL MCS"
#define HLP_MACRLC_UL_MIN_MCS "Minimum UL MCS that should be used"
#define HLP_MACRLC_UL_MAX_MCS "Maximum UL MCS that should be used"
#define HLP_MACRLC_DL_HARQ_MAX "Maximum number of DL HARQ rounds"
#define HLP_MACRLC_UL_HARQ_MAX "Maximum number of UL HARQ rounds"
#define HLP_MACRLC_MIN_GRANT_PRB "Minimal Periodic ULSCH Grant PRBs"
#define HLP_MACRLC_IDENTITY_PM "Flag to use only identity matrix in DL precoding"
#define HLP_MACRLC_AB "Flag to enable analog beamforming"
#define HLP_MACRLC_BEAM_DURATION "number of consecutive slots for a given set of beams"
#define HLP_MACRLC_BEAMS_PERIOD "set of beams that can be simultaneously allocated in a period"
#define HLP_MACRLC_DBT_FILE "File path to CSV file to read digital beamforming table"
#define HLP_MACRLC_PUSCH_RSSI_THRESHOLD "Limits PUSCH TPC commands based on RSSI to prevent ADC railing. Value range [-1280, 0], unit 0.1 dBm/dBFS"
#define HLP_MACRLC_PUCCH_RSSI_THRESHOLD "Limits PUCCH TPC commands based on RSSI to prevent ADC railing. Value range [-1280, 0], unit 0.1 dBm/dBFS"
#define HLP_MACRLC_STATS_MAX_UE "Maximum number of UEs before disabling periodical output (0 to disable)"
#define HLP_SCHED_POLICY "Name of the registered scheduler policy"
#define HLP_SCHED_OBSERVERS "Ordered list of registered scheduler observers"
#define HLP_MACRLC_SPATIAL_STREAM_INDEX "Array of RU antenna ports / eAxCIDs to be used by L1. This may only be applicable for MU-MIMO. Value range [0, 15]"

/*-------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*                                            MacRLC  configuration parameters                                                                           */
/*   optname                                            helpstr   paramflags    XXXptr              defXXXval                  type           numelt     */
/*-------------------------------------------------------------------------------------------------------------------------------------------------------*/
// clang-format off
#define MACRLCPARAMS_DESC { \
  {MACRLC_TRANSPORT_N_PREFERENCE,      NULL,                     0, .strptr=NULL, .defstrval="local_L1",      TYPE_STRING,  0}, \
  {MACRLC_LOCAL_N_ADDRESS,             NULL,                     0, .strptr=NULL, .defstrval="127.0.0.1",     TYPE_STRING,  0}, \
  {MACRLC_REMOTE_N_ADDRESS,            NULL,                     0, .uptr=NULL,   .defstrval="127.0.0.2",     TYPE_STRING,  0}, \
  {MACRLC_LOCAL_N_PORTD,               NULL,                     0, .uptr=NULL,   .defintval=50011,           TYPE_UINT,    0}, \
  {MACRLC_REMOTE_N_PORTD,              NULL,                     0, .uptr=NULL,   .defintval=50011,           TYPE_UINT,    0}, \
  {MACRLC_TRANSPORT_S_PREFERENCE,      NULL,                     0, .strptr=NULL, .defstrval="local_RRC",     TYPE_STRING,  0}, \
  {MACRLC_LOCAL_S_ADDRESS,             NULL,                     0, .uptr=NULL,   .defstrval="127.0.0.1",     TYPE_STRING,  0}, \
  {MACRLC_REMOTE_S_ADDRESS,            NULL,                     0, .uptr=NULL,   .defstrval="127.0.0.2",     TYPE_STRING,  0}, \
  {MACRLC_LOCAL_S_PORTC,               NULL,                     0, .uptr=NULL,   .defintval=50020,           TYPE_UINT,    0}, \
  {MACRLC_LOCAL_S_PORTD,               NULL,                     0, .uptr=NULL,   .defintval=50021,           TYPE_UINT,    0}, \
  {MACRLC_ULSCH_MAX_FRAME_INACTIVITY,  NULL,                     0, .uptr=NULL,   .defintval=10,              TYPE_UINT,    0}, \
  {MACRLC_PUSCHTARGETSNRX10,           NULL,                     0, .iptr=NULL,   .defintval=200,             TYPE_INT,     0}, \
  {MACRLC_PUCCHTARGETSNRX10,           NULL,                     0, .iptr=NULL,   .defintval=150,             TYPE_INT,     0}, \
  {MACRLC_UL_PRBBLACK_SNR_THRESHOLD,   HLP_MACRLC_UL_PRBBLACK,   0, .iptr=NULL,   .defintval=10,              TYPE_INT,     0}, \
  {MACRLC_PUCCHFAILURETHRES,           NULL,                     0, .iptr=NULL,   .defintval=10,              TYPE_INT,     0}, \
  {MACRLC_PUSCHFAILURETHRES,           NULL,                     0, .iptr=NULL,   .defintval=10,              TYPE_INT,     0}, \
  {MACRLC_DL_BLER_TARGET_UPPER,        HLP_MACRLC_DL_BLER_UP,    0, .dblptr=NULL, .defdblval=0.15,            TYPE_DOUBLE,  0}, \
  {MACRLC_DL_BLER_TARGET_LOWER,        HLP_MACRLC_DL_BLER_LO,    0, .dblptr=NULL, .defdblval=0.05,            TYPE_DOUBLE,  0}, \
  {MACRLC_DL_MIN_MCS,                  HLP_MACRLC_DL_MIN_MCS,    0, .u8ptr=NULL,  .defintval=0,               TYPE_UINT8,   0}, \
  {MACRLC_DL_MAX_MCS,                  HLP_MACRLC_DL_MAX_MCS,    0, .u8ptr=NULL,  .defintval=28,              TYPE_UINT8,   0}, \
  {MACRLC_UL_BLER_TARGET_UPPER,        HLP_MACRLC_UL_BLER_UP,    0, .dblptr=NULL, .defdblval=0.15,            TYPE_DOUBLE,  0}, \
  {MACRLC_UL_BLER_TARGET_LOWER,        HLP_MACRLC_UL_BLER_LO,    0, .dblptr=NULL, .defdblval=0.05,            TYPE_DOUBLE,  0}, \
  {MACRLC_UL_MIN_MCS,                  HLP_MACRLC_UL_MIN_MCS,    0, .u8ptr=NULL,  .defintval=0,               TYPE_UINT8,   0}, \
  {MACRLC_UL_MAX_MCS,                  HLP_MACRLC_UL_MAX_MCS,    0, .u8ptr=NULL,  .defintval=28,              TYPE_UINT8,   0}, \
  {MACRLC_DL_HARQ_ROUND_MAX,           HLP_MACRLC_DL_HARQ_MAX,   0, .u8ptr=NULL,  .defintval=4,               TYPE_UINT8,   0}, \
  {MACRLC_UL_HARQ_ROUND_MAX,           HLP_MACRLC_UL_HARQ_MAX,   0, .u8ptr=NULL,  .defintval=4,               TYPE_UINT8,   0}, \
  {MACRLC_MIN_GRANT_PRB,               HLP_MACRLC_MIN_GRANT_PRB, 0, .u16ptr=NULL, .defintval=5,               TYPE_UINT16,  0}, \
  {MACRLC_IDENTITY_PM,                 HLP_MACRLC_IDENTITY_PM,   PARAMFLAG_BOOL, .u8ptr=NULL, .defintval=0,   TYPE_UINT8,   0}, \
  {MACRLC_LOCAL_N_ADDRESS_F1U,         NULL,                     0, .strptr=NULL, .defstrval=NULL,            TYPE_STRING,  0}, \
  {MACRLC_TRANSPORT_S_SHM_PREFIX,      NULL,                     0, .strptr=NULL, .defstrval="nvipc",         TYPE_STRING,  0}, \
  {MACRLC_TRANSPORT_S_POLL_CORE,       NULL,                     0, .i8ptr=NULL,  .defintval=-1,              TYPE_INT8,    0}, \
  {MACRLC_ANALOG_BEAMFORMING,          HLP_MACRLC_AB,            0, .strptr=NULL, .defstrval="none",          TYPE_STRING,  0}, \
  {MACRLC_BEAM_DURATION,               HLP_MACRLC_BEAM_DURATION, 0, .u8ptr=NULL,  .defintval=1,               TYPE_UINT8,   0}, \
  {MACRLC_BEAMS_PERIOD,                HLP_MACRLC_BEAMS_PERIOD,  0, .u8ptr=NULL,  .defintval=1,               TYPE_UINT8,   0}, \
  {MACRLC_BEAM_WEIGHTS_LIST,           NULL,                     0, .iptr=NULL,   .defintarrayval=0,          TYPE_INTARRAY,0}, \
  {MACRLC_DBT_FILE,                    HLP_MACRLC_DBT_FILE,      0, .strptr=NULL, .defstrval=NULL,            TYPE_STRING,  0}, \
  {MACRLC_PUSCH_RSSI_THRESHOLD,        HLP_MACRLC_PUSCH_RSSI_THRESHOLD, \
                                                                               0, .iptr=NULL,   .defintval=0,               TYPE_INT,     0}, \
  {MACRLC_PUCCH_RSSI_THRESHOLD,        HLP_MACRLC_PUCCH_RSSI_THRESHOLD, \
                                                                               0, .iptr=NULL,   .defintval=0,               TYPE_INT,     0}, \
  {MACRLC_STATS_MAX_UE,                HLP_MACRLC_STATS_MAX_UE,  0, .iptr=NULL,   .defintval=8,               TYPE_INT,     0}, \
  {MACRLC_DL_PREPROCESSOR_POLICY,      HLP_SCHED_POLICY,         0, .strptr=NULL,  .defstrval="default",       TYPE_STRING,  0}, \
  {MACRLC_DL_RI_PMI_SELECT_POLICY,     HLP_SCHED_POLICY,         0, .strptr=NULL,  .defstrval="default",       TYPE_STRING,  0}, \
  {MACRLC_DL_TDA_SELECT_POLICY,        HLP_SCHED_POLICY,         0, .strptr=NULL,  .defstrval="default",       TYPE_STRING,  0}, \
  {MACRLC_DL_BEAM_SELECT_POLICY,       HLP_SCHED_POLICY,         0, .strptr=NULL,  .defstrval="default",       TYPE_STRING,  0}, \
  {MACRLC_DL_MCS_SELECT_POLICY,        HLP_SCHED_POLICY,         0, .strptr=NULL,  .defstrval="default",       TYPE_STRING,  0}, \
  {MACRLC_DL_RB_ALLOC_POLICY,          HLP_SCHED_POLICY,         0, .strptr=NULL,  .defstrval="default",       TYPE_STRING,  0}, \
  {MACRLC_DL_LCID_ALLOC_POLICY,        HLP_SCHED_POLICY,         0, .strptr=NULL,  .defstrval="default",       TYPE_STRING,  0}, \
  {MACRLC_UL_PREPROCESSOR_POLICY,      HLP_SCHED_POLICY,         0, .strptr=NULL,  .defstrval="default",       TYPE_STRING,  0}, \
  {MACRLC_UL_RI_TPMI_SELECT_POLICY,    HLP_SCHED_POLICY,         0, .strptr=NULL,  .defstrval="default",       TYPE_STRING,  0}, \
  {MACRLC_UL_TDA_SELECT_POLICY,        HLP_SCHED_POLICY,         0, .strptr=NULL,  .defstrval="default",       TYPE_STRING,  0}, \
  {MACRLC_UL_BEAM_SELECT_POLICY,       HLP_SCHED_POLICY,         0, .strptr=NULL,  .defstrval="default",       TYPE_STRING,  0}, \
  {MACRLC_UL_MCS_SELECT_POLICY,        HLP_SCHED_POLICY,         0, .strptr=NULL,  .defstrval="default",       TYPE_STRING,  0}, \
  {MACRLC_UL_RB_ALLOC_POLICY,          HLP_SCHED_POLICY,         0, .strptr=NULL,  .defstrval="default",       TYPE_STRING,  0}, \
  {MACRLC_DL_HARQ_RESULT_OBSERVERS,    HLP_SCHED_OBSERVERS,      0, .strlistptr=NULL, .defstrlistval=NULL,    TYPE_STRINGLIST, 0}, \
  {MACRLC_UL_HARQ_RESULT_OBSERVERS,    HLP_SCHED_OBSERVERS,      0, .strlistptr=NULL, .defstrlistval=NULL,    TYPE_STRINGLIST, 0}, \
  {MACRLC_SPATIAL_STREAM_IDX,          HLP_MACRLC_SPATIAL_STREAM_INDEX, \
                                                                               0, .uptr=NULL,   .defintarrayval=0,          TYPE_INTARRAY,0}, \
}
// clang-format off

#define MACRLCPARAMS_CHECK { \
  { .s5 = { NULL } }, \
  { .s5 = { NULL } }, \
  { .s5 = { NULL } }, \
  { .s5 = { NULL } }, \
  { .s5 = { NULL } }, \
  { .s5 = { NULL } }, \
  { .s5 = { NULL } }, \
  { .s5 = { NULL } }, \
  { .s5 = { NULL } }, \
  { .s5 = { NULL } }, \
  { .s5 = { NULL } }, \
  { .s5 = { NULL } }, \
  { .s5 = { NULL } }, \
  { .s5 = { NULL } }, \
  { .s5 = { NULL } }, \
  { .s5 = { NULL } }, \
  { .s5 = { NULL } }, \
  { .s5 = { NULL } }, \
  { .s2 = { config_check_intrange, {0, 31} } }, /* DL min MCS */ \
  { .s2 = { config_check_intrange, {0, 31} } }, /* DL max MCS */ \
  { .s5 = { NULL } }, \
  { .s5 = { NULL } }, \
  { .s2 = { config_check_intrange, {0, 31} } }, /* UL min MCS */ \
  { .s2 = { config_check_intrange, {0, 31} } }, /* UL max MCS */ \
  { .s2 = { config_check_intrange, {1, 8} } }, /* DL max HARQ rounds */ \
  { .s2 = { config_check_intrange, {1, 8} } }, /* UL max HARQ rounds */ \
  { .s5 = { NULL } }, \
  { .s2 = { NULL } }, \
  { .s5 = { NULL } }, \
  { .s5 = { NULL } }, \
  { .s2 = { NULL } }, \
  { .s3a = { config_checkstr_assign_integer, \
             {"none", "preconfigured", "lophy"}, \
             {NO_BEAM_MODE, PRECONFIGURED_BEAM_IDX, LOPHY_BEAM_IDX}, \
             3 } }, \
  { .s5 = { NULL } }, \
  { .s5 = { NULL } }, \
  { .s5 = { NULL } }, \
  { .s5 = { NULL } }, \
  { .s2 =  { config_check_intrange, {-1280, 0}} }, /* PUSCH RSSI threshold range */ \
  { .s2 =  { config_check_intrange, {-1280, 0}} }, /* PUCCH RSSI threshold range */ \
  { .s5 = { NULL } }, \
  { .s5 = { NULL } }, /* DL preprocessor policy */ \
  { .s5 = { NULL } }, /* DL RI/PMI policy */ \
  { .s5 = { NULL } }, /* DL TDA policy */ \
  { .s5 = { NULL } }, /* DL beam policy */ \
  { .s5 = { NULL } }, /* DL MCS policy */ \
  { .s5 = { NULL } }, /* DL RB-allocation policy */ \
  { .s5 = { NULL } }, /* DL LCID-allocation policy */ \
  { .s5 = { NULL } }, /* UL preprocessor policy */ \
  { .s5 = { NULL } }, /* UL RI/TPMI policy */ \
  { .s5 = { NULL } }, /* UL TDA policy */ \
  { .s5 = { NULL } }, /* UL beam policy */ \
  { .s5 = { NULL } }, /* UL MCS policy */ \
  { .s5 = { NULL } }, /* UL RB-allocation policy */ \
  { .s5 = { NULL } }, /* DL HARQ observers */ \
  { .s5 = { NULL } }, /* UL HARQ observers */ \
  { .s2 = { NULL } }, /* Spatial stream index */ \
}

/*---------------------------------------------------------------------------------------------------------------------------------------------------------*/
#endif
