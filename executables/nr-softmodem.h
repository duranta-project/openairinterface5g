/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_SOFTMODEM_H
#define NR_SOFTMODEM_H

#include <executables/nr-softmodem-common.h>

#include "PHY/defs_gNB.h"

#define DEFAULT_DLF 2680000000

/***************************************************************************************************************************************/
/* command line options definitions, CMDLINE_XXXX_DESC macros are used to initialize paramdef_t arrays which are then used as argument
   when calling config_get or config_getlist functions                                                                                 */

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*                                            command line parameters common to eNodeB and UE                                                                           */
/*   optname                helpstr                 paramflags        XXXptr                              defXXXval                   type         numelt               */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// clang-format off
#define CMDLINE_PARAMS_DESC_GNB { \
  {"l" ,                    CONFIG_HLP_DLNL_PHYTEST,   0,                .uptr=&target_dl_Nl,                 .defintval=0,                     TYPE_UINT,   0},        \
  {"L" ,                    CONFIG_HLP_ULNL_PHYTEST,   0,                .uptr=&target_ul_Nl,                 .defintval=0,                     TYPE_UINT,   0},        \
  {"p" ,                    CONFIG_HLP_DLPMI_PHYTEST,  0,                .uptr=&target_dl_pmi,                .defintval=0,                     TYPE_UINT,   0},        \
  {"m" ,                    CONFIG_HLP_DLMCS_PHYTEST,  0,                .uptr=&target_dl_mcs,                .defintval=0,                     TYPE_UINT,   0},        \
  {"n" ,                    CONFIG_HLP_DLMCST_PHYTEST, 0,                .uptr=&target_dl_mcs_table_index,    .defintval=0,                     TYPE_UINT,   0},        \
  {"t" ,                    CONFIG_HLP_ULMCS_PHYTEST,  0,                .uptr=&target_ul_mcs,                .defintval=0,                     TYPE_UINT,   0},        \
  {"u" ,                    CONFIG_HLP_ULMCST_PHYTEST, 0,                .uptr=&target_ul_mcs_table_index,    .defintval=0,                     TYPE_UINT,   0},        \
  {"M" ,                    CONFIG_HLP_DLBW_PHYTEST,   0,                .uptr=&target_dl_bw,                 .defintval=0,                     TYPE_UINT,   0},        \
  {"T" ,                    CONFIG_HLP_ULBW_PHYTEST,   0,                .uptr=&target_ul_bw,                 .defintval=0,                     TYPE_UINT,   0},        \
  {"D" ,                    CONFIG_HLP_DLBM_PHYTEST,   0,                .u64ptr=&dlsch_slot_bitmap,          .defint64val=0,                   TYPE_UINT64, 0},        \
  {"U" ,                    CONFIG_HLP_ULBM_PHYTEST,   0,                .u64ptr=&ulsch_slot_bitmap,          .defint64val=0,                   TYPE_UINT64, 0},        \
  {"Dmod" ,                 CONFIG_HLP_DLMOD_PHYTEST,   0,               .uptr=&dlsch_slot_modval,            .defintval=0,                     TYPE_UINT,   0},        \
  {"Umod" ,                 CONFIG_HLP_ULMOD_PHYTEST,   0,               .uptr=&ulsch_slot_modval,            .defintval=0,                     TYPE_UINT,   0},        \
  {"usrp-tx-thread-config", CONFIG_HLP_USRP_THREAD,    0,                .iptr=&usrp_tx_thread,               .defstrval=0,                     TYPE_INT,    0},        \
  {"uecap_file",            CONFIG_HLP_UECAP_FILE,     0,                .strptr=&uecap_file,                 .defstrval="./uecap_ports1.xml",  TYPE_STRING, 0},        \
}
// clang-format on

extern uint32_t target_dl_Nl;
extern uint32_t target_ul_Nl;
extern uint32_t target_dl_pmi;
extern uint32_t target_dl_mcs;
extern uint32_t target_dl_mcs_table_index;
extern uint32_t target_ul_mcs;
extern uint32_t target_ul_mcs_table_index;
extern uint32_t target_dl_bw;
extern uint32_t target_ul_bw;
extern uint64_t dlsch_slot_bitmap;
extern uint64_t ulsch_slot_bitmap;
extern char *uecap_file;
extern uint32_t dlsch_slot_modval;
extern uint32_t ulsch_slot_modval;

// In nr-gnb.c
extern void init_gNB();
extern void stop_gNB(int);

// In nr-ru.c
extern void init_NR_RU(configmodule_interface_t *cfg, char *);
extern void start_NR_RU(void);
extern void stop_RU(int nb_ru);
extern void kill_NR_RU_proc(int inst);
extern void set_function_spec_param(RU_t *ru);
extern void ru_tx_func(void *param);

void init_eNB_afterRU(void);

void init_pdcp(void);

void fill_rf_config(RU_t *ru, char *rf_config_file);

#endif
