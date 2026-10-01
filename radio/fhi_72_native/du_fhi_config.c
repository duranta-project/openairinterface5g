/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "du_fhi_config.h"
#include <string.h>
#include "assertions.h"
#include "common/config/config_userapi.h"
#include "common/utils/utils.h"
#include "common/utils/LOG/log.h"

#define CONFIG_SECTION_FHI_NATIVE "fhi_72_native"

#define CONFIG_STRING_DPDK_DEVICES "dpdk_devices"
#define CONFIG_STRING_WORKER_CORE "worker_core"
#define CONFIG_STRING_EXTRA_EAL_ARGS "extra_eal_args"
#define CONFIG_STRING_RU_ADDR "ru_addr"
#define CONFIG_STRING_MTU "mtu"
#define CONFIG_STRING_COMP_TYPE "comp_type"
#define CONFIG_STRING_IQ_WIDTH "iq_width"
#define CONFIG_STRING_T1A_CP_DL "T1a_cp_dl"
#define CONFIG_STRING_T1A_CP_UL "T1a_cp_ul"
#define CONFIG_STRING_T1A_UP "T1a_up"
#define CONFIG_STRING_TA3 "Ta3"
#define CONFIG_STRING_PRACH_EAXC_OFFSET "prach_eaxc_offset"
#define CONFIG_STRING_PRACH_KBAR "prach_kbar"

#define COMP_TYPE_CHECK                                                                        \
  &(checkedparam_t)                                                                            \
  {                                                                                             \
    .s3a = { config_checkstr_assign_integer, {"none", "bfp", "blkscale", "ulaw"}, {0, 1, 2, 3}, 4 } \
  }

// clang-format off
#define CMDLINE_PARAMS_DESC_FHI_NATIVE \
{ \
  {CONFIG_STRING_DPDK_DEVICES,   "DPDK devices to use for the native DU fronthaul.",   PARAMFLAG_MANDATORY, .strptr=NULL, .defstrval=NULL,     TYPE_STRINGLIST, 0}, \
  {CONFIG_STRING_WORKER_CORE,    "CPU core for the DU fronthaul RX/timer/TX worker.",  PARAMFLAG_MANDATORY, .iptr=NULL,   .defintval=-1,       TYPE_INT,        0}, \
  {CONFIG_STRING_EXTRA_EAL_ARGS, "Extra arguments passed to RTE_EAL_INIT.",            0,                   .strptr=NULL, .defstrval=NULL,     TYPE_STRINGLIST, 0}, \
  {CONFIG_STRING_RU_ADDR,        "O-RU MAC addresses, used to prepare Ethernet headers.", PARAMFLAG_MANDATORY, .strptr=NULL, .defstrval=NULL,  TYPE_STRINGLIST, 0}, \
  {CONFIG_STRING_MTU,            "MTU for RX and TX.",                                  0,                   .iptr=NULL,   .defintval=9600,     TYPE_INT,        0}, \
  {CONFIG_STRING_COMP_TYPE,      "DL/UL U-plane compression method: none, bfp, blkscale, or ulaw.", 0,       .strptr=NULL, .defstrval="none",   TYPE_STRING,     0, COMP_TYPE_CHECK}, \
  {CONFIG_STRING_IQ_WIDTH,       "IQ bit width when compression is enabled.",           0,                   .uptr=NULL,   .defintval=16,       TYPE_UINT,       0}, \
  {CONFIG_STRING_T1A_CP_DL,      "DL C-Plane advance window [min,max] in uS.",          0,                   .iptr=NULL,   .defintarrayval=NULL, TYPE_INTARRAY,  0}, \
  {CONFIG_STRING_T1A_CP_UL,      "UL/PRACH C-Plane advance window [min,max] in uS.",    0,                   .iptr=NULL,   .defintarrayval=NULL, TYPE_INTARRAY,  0}, \
  {CONFIG_STRING_T1A_UP,         "DL U-Plane advance window [min,max] in uS.",          0,                   .iptr=NULL,   .defintarrayval=NULL, TYPE_INTARRAY,  0}, \
  {CONFIG_STRING_TA3,            "UL U-Plane RX reassembly window [min,max] in uS.",    0,                   .iptr=NULL,   .defintarrayval=NULL, TYPE_INTARRAY,  0}, \
  {CONFIG_STRING_PRACH_EAXC_OFFSET, "PRACH eAxC offset.",                               0,                   .u8ptr=NULL,  .defuintval=0,       TYPE_UINT8,      0}, \
  {CONFIG_STRING_PRACH_KBAR,     "PRACH kbar offset.",                                  0,                   .uptr=NULL,   .defuintval=4,       TYPE_UINT,       0}, \
}
// clang-format on

// Derives {num_dl_slots, num_ul_slots, num_dl_symbols, num_ul_symbols} from the per-symbol TDD
// frame structure openair0_cfg->split7 already carries (populated from servingCellConfigCommon
// before transport_init() runs, same source radio/fhi_72/oran-config.c's set_fh_frame_config()
// reads) -- assumes the simple "N full-DL slots, one DL/UL mixed slot, M full-UL slots per
// period" shape du_fh_tdd_pattern_t models (matches fronthaul/oru's own tdd_pattern_t).
static void derive_tdd_pattern(const split7_config_t *s7cfg, du_fh_tdd_pattern_t *tdd)
{
  tdd->tdd_pattern_length_slots = s7cfg->n_tdd_period;
  tdd->num_dl_slots = 0;
  tdd->num_ul_slots = 0;
  tdd->num_dl_symbols = 0;
  tdd->num_ul_symbols = 0;

  for (int slot = 0; slot < s7cfg->n_tdd_period; slot++) {
    const symbol_direction_t *dirs = s7cfg->slot_dirs[slot].sym_dir;
    bool all_dl = true, all_ul = true;
    for (int sym = 0; sym < 14; sym++) {
      if (dirs[sym] != SYMBOL_DIR_DL)
        all_dl = false;
      if (dirs[sym] != SYMBOL_DIR_UL)
        all_ul = false;
    }
    if (all_dl) {
      tdd->num_dl_slots++;
      continue;
    }
    if (all_ul) {
      tdd->num_ul_slots++;
      continue;
    }
    int dl_syms = 0;
    while (dl_syms < 14 && dirs[dl_syms] == SYMBOL_DIR_DL)
      dl_syms++;
    int ul_syms = 0;
    while (ul_syms < 14 && dirs[13 - ul_syms] == SYMBOL_DIR_UL)
      ul_syms++;
    tdd->num_dl_symbols = dl_syms;
    tdd->num_ul_symbols = ul_syms;
  }
}

int get_du_fh_options(du_fh_config_t *cfg, const openair0_config_t *openair0_cfg, frequency_range_t *prach_freq_range)
{
  memset(cfg, 0, sizeof(*cfg));

  cfg->numerology = openair0_cfg->split7.mu;
  cfg->num_prbs = (uint16_t)openair0_cfg->num_rb_dl;
  cfg->fdd_mode = (openair0_cfg->duplex_mode == duplex_mode_FDD);
  *prach_freq_range = cfg->numerology > 2 ? FR2 : FR1;
  if (!cfg->fdd_mode)
    derive_tdd_pattern(&openair0_cfg->split7, &cfg->tdd_pattern);

  paramdef_t param[] = CMDLINE_PARAMS_DESC_FHI_NATIVE;
  int nump = sizeofArray(param);
  int ret = config_get(config_get_if(), param, nump, CONFIG_SECTION_FHI_NATIVE);
  if (ret <= 0) {
    LOG_E(HW, "problem reading section \"%s\"\n", CONFIG_SECTION_FHI_NATIVE);
    return -1;
  }

  du_fh_dpdk_config_t *dpdk_conf = &cfg->dpdk_conf;
  int num_dpdk_devices = gpd(param, nump, CONFIG_STRING_DPDK_DEVICES)->numelt;
  AssertFatal(num_dpdk_devices > 0 && num_dpdk_devices <= MAX_RU_PORTS,
              "Invalid number of DPDK devices (%d). Configure 1 to %d devices\n",
              num_dpdk_devices,
              MAX_RU_PORTS);
  dpdk_conf->num_dpdk_devices = num_dpdk_devices;
  for (int i = 0; i < num_dpdk_devices; i++) {
    dpdk_conf->dpdk_devices[i] = gpd(param, nump, CONFIG_STRING_DPDK_DEVICES)->strlistptr[i];
  }
  dpdk_conf->extra_eal_args = gpd(param, nump, CONFIG_STRING_EXTRA_EAL_ARGS)->strlistptr;
  dpdk_conf->num_extra_eal_args = gpd(param, nump, CONFIG_STRING_EXTRA_EAL_ARGS)->numelt;

  cfg->num_ru_mac_addrs = gpd(param, nump, CONFIG_STRING_RU_ADDR)->numelt;
  AssertFatal(cfg->num_ru_mac_addrs > 0 && cfg->num_ru_mac_addrs <= MAX_RU_PORTS,
              "Invalid number of RU MAC addresses (%d). Configure 1 to %d\n",
              cfg->num_ru_mac_addrs,
              MAX_RU_PORTS);
  for (int i = 0; i < cfg->num_ru_mac_addrs; i++) {
    cfg->ru_mac_addrs[i] = gpd(param, nump, CONFIG_STRING_RU_ADDR)->strlistptr[i];
    AssertFatal(strlen(cfg->ru_mac_addrs[i]) == 17, "Invalid MAC address\n");
  }

  int comp_type_idx = config_paramidx_fromname(param, nump, CONFIG_STRING_COMP_TYPE);
  AssertFatal(comp_type_idx >= 0, "Index for %s config option not found!\n", CONFIG_STRING_COMP_TYPE);
  cfg->comp_type = (fh_comp_method_t)config_get_processedint(config_get_if(), &param[comp_type_idx]);
  cfg->iq_width = (uint8_t)*gpd(param, nump, CONFIG_STRING_IQ_WIDTH)->uptr;
  cfg->worker_core = *gpd(param, nump, CONFIG_STRING_WORKER_CORE)->iptr;
  cfg->mtu = (uint16_t)*gpd(param, nump, CONFIG_STRING_MTU)->iptr;
  cfg->prach_eaxc_offset = *gpd(param, nump, CONFIG_STRING_PRACH_EAXC_OFFSET)->u8ptr;
  cfg->prach_kbar = *gpd(param, nump, CONFIG_STRING_PRACH_KBAR)->uptr;

  const paramdef_t *t1a_cp_dl = gpd(param, nump, CONFIG_STRING_T1A_CP_DL);
  AssertFatal(t1a_cp_dl->numelt == 2, "Two parameters required for %s\n", CONFIG_STRING_T1A_CP_DL);
  cfg->T1a_cp_dl_min_uS = t1a_cp_dl->iptr[0];
  cfg->T1a_cp_dl_max_uS = t1a_cp_dl->iptr[1];

  const paramdef_t *t1a_cp_ul = gpd(param, nump, CONFIG_STRING_T1A_CP_UL);
  AssertFatal(t1a_cp_ul->numelt == 2, "Two parameters required for %s\n", CONFIG_STRING_T1A_CP_UL);
  cfg->T1a_cp_ul_min_uS = t1a_cp_ul->iptr[0];
  cfg->T1a_cp_ul_max_uS = t1a_cp_ul->iptr[1];

  const paramdef_t *t1a_up = gpd(param, nump, CONFIG_STRING_T1A_UP);
  AssertFatal(t1a_up->numelt == 2, "Two parameters required for %s\n", CONFIG_STRING_T1A_UP);
  cfg->T1a_up_min_uS = t1a_up->iptr[0];
  cfg->T1a_up_max_uS = t1a_up->iptr[1];

  const paramdef_t *ta3 = gpd(param, nump, CONFIG_STRING_TA3);
  AssertFatal(ta3->numelt == 2, "Two parameters required for %s\n", CONFIG_STRING_TA3);
  cfg->Ta3_min_uS = ta3->iptr[0];
  cfg->Ta3_max_uS = ta3->iptr[1];

  return 0;
}
