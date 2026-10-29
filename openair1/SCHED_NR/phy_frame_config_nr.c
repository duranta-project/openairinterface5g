/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/***********************************************************************
*
* FILENAME    :  phy_frame_configuration_nr.c
*
* DESCRIPTION :  functions related to FDD/TDD configuration for NR
*                see TS 38.213 11.1 Slot configuration
*                and TS 38.331 for RRC configuration
*
************************************************************************/

#include "PHY/defs_nr_common.h"
#include "PHY/defs_gNB.h"
#include "SCHED_NR/phy_frame_config_nr.h"
#include "common/utils/nr/nr_common.h"

#ifdef NR_UNIT_TEST
  #define FILE_NAME                " "
  #define LINE_FILE                (0)
  #define NR_TST_PHY_PRINTF(...)   printf(__VA_ARGS__)
#else
  #define FILE_NAME                (__FILE__)
  #define LINE_FILE                (__LINE__)
  #define NR_TST_PHY_PRINTF(...)
#endif

/** @brief This function processes TDD dedicated configuration for NR
 *         by processing the tdd_slot_bitmap and period_cfg, and
 *         allocates memory and fills max_num_of_symbol_per_slot_list
 *         in the nfapi config request (cfg)
 *  @ref   see TS 38.213 11.1 Slot configuration
 *  @param cfg NR config request structure pointer
 *  @param fs TDD configuration pointer
 *  @returns void
 */
void set_tdd_config_nr(nfapi_nr_config_request_scf_t *cfg, frame_structure_t *fs)
{
  AssertFatal(fs != NULL, "frame_structure must be non-NULL");

  int slot_number = 0;
  int nb_slots_to_set = fs->numb_slots_frame;
  int slot_index = 0;
  tdd_period_config_t *pc = &fs->period_cfg;
  tdd_bitmap_t *tdd_slot_bitmap = pc->tdd_slot_bitmap;

  LOG_I(NR_PHY,
        "Set TDD Period Configuration: %d periods per frame, %d slots to be configured (%d DL, %d UL)\n",
        fs->numb_period_frame,
        nb_slots_to_set,
        pc->num_dl_slots,
        pc->num_ul_slots);

  // Allocate memory formax_num_of_symbol_per_slot_list items
  cfg->tdd_table.max_tdd_periodicity_list = calloc_or_fail(nb_slots_to_set, sizeof(nfapi_nr_max_tdd_periodicity_t));
  for (int slot = 0; slot < nb_slots_to_set; slot++) {
    nfapi_nr_max_tdd_periodicity_t *p_list = &cfg->tdd_table.max_tdd_periodicity_list[slot];
    p_list->max_num_of_symbol_per_slot_list = calloc_or_fail(NR_SYMBOLS_PER_SLOT, sizeof(*p_list->max_num_of_symbol_per_slot_list));
    for (int sym = 0; sym < NR_SYMBOLS_PER_SLOT; sym++) {
      // for each symbol, assign the TLV tag for usage when packing
      p_list->max_num_of_symbol_per_slot_list[sym].slot_config.tl.tag = NFAPI_NR_CONFIG_SLOT_CONFIG_TAG;
    }
  }

  // Loop through all periods to be configured
  while (slot_number != nb_slots_to_set) {
    slot_index = get_slot_idx_in_period(slot_number, fs);
    /* Fill max_tdd_periodicity_list */
    nfapi_nr_max_tdd_periodicity_t *list = &cfg->tdd_table.max_tdd_periodicity_list[slot_number];
    // FULL DOWNLINK SLOTS
    if (tdd_slot_bitmap[slot_index].slot_type == TDD_NR_DOWNLINK_SLOT) {
      for (int sym = 0; sym < NR_SYMBOLS_PER_SLOT; sym++) {
        list->max_num_of_symbol_per_slot_list[sym % NR_SYMBOLS_PER_SLOT].slot_config.value = 0;
        if ((sym + 1) % NR_SYMBOLS_PER_SLOT == 0) {
          slot_number++;
          list += 1;
        }
      }
    }
    // FLEXIBLE SLOTS
    if (tdd_slot_bitmap[slot_index].slot_type == TDD_NR_MIXED_SLOT) {
      int nrofDLSymbolsInSlot = tdd_slot_bitmap[slot_index].num_dl_symbols;
      int nrofULSymbolsInSlot = tdd_slot_bitmap[slot_index].num_ul_symbols;
      AssertFatal(nrofDLSymbolsInSlot + nrofULSymbolsInSlot < NR_SYMBOLS_PER_SLOT,
                  "illegal symbol configuration DL %d, UL %d\n",
                  nrofDLSymbolsInSlot,
                  nrofULSymbolsInSlot);
      // DL Symbols
      for (int sym = 0; sym < nrofDLSymbolsInSlot; sym++) {
        list->max_num_of_symbol_per_slot_list[sym].slot_config.value = 0;
      }
      // Flexible Symbols
      for (int sym = nrofDLSymbolsInSlot; sym < NR_SYMBOLS_PER_SLOT - nrofULSymbolsInSlot; sym++) {
        list->max_num_of_symbol_per_slot_list[sym].slot_config.value = 2;
      }
      // UL Symbols
      for (int sym = NR_SYMBOLS_PER_SLOT - nrofULSymbolsInSlot; sym < NR_SYMBOLS_PER_SLOT; sym++) {
        list->max_num_of_symbol_per_slot_list[sym].slot_config.value = 1;
      }
      slot_number++;
      list += 1;
    }
    // FULL UPLINK SLOTS
    if (tdd_slot_bitmap[slot_index].slot_type == TDD_NR_UPLINK_SLOT) {
      for (int sym = 0; sym < NR_SYMBOLS_PER_SLOT; sym++) {
        list->max_num_of_symbol_per_slot_list[sym % NR_SYMBOLS_PER_SLOT].slot_config.value = 1;
        if ((sym + 1) % NR_SYMBOLS_PER_SLOT == 0) {
          slot_number++;
          list += 1;
        }
      }
    }
  }
  // Dump 1 period on the output
  for (int s = 0; s < fs->numb_slots_period; s++) {
    if (tdd_slot_bitmap[s].slot_type == TDD_NR_MIXED_SLOT) {
      char flexi_slot[NR_SYMBOLS_PER_SLOT + 1];
      memset(flexi_slot, 0, sizeof(flexi_slot));
      for (int symb = 0; symb < NR_SYMBOLS_PER_SLOT; symb++) {
        uint8_t val = cfg->tdd_table.max_tdd_periodicity_list[s].max_num_of_symbol_per_slot_list[symb].slot_config.value;
        char *symb_type_s[] = {"D", "U", "F"};
        flexi_slot[symb] = *symb_type_s[val];
      }
      flexi_slot[NR_SYMBOLS_PER_SLOT] = '\0';
      LOG_I(NR_PHY, "TDD period configuration: slot %d is FLEXIBLE: %s\n", s, flexi_slot);
    } else {
      LOG_I(NR_PHY,
            "TDD period configuration: slot %d is %s\n",
            s,
            tdd_slot_bitmap[s].slot_type == TDD_NR_DOWNLINK_SLOT ? "DOWNLINK" : "UPLINK");
    }
  }
}

nr_slot_type_t nr_slot_select(const nfapi_nr_config_request_scf_t *cfg, int nr_frame, int nr_slot)
{
  (void) nr_frame;

  /* FDD slots support both full-slot directions. */
  int check_slot = 0;

  if (cfg->cell_config.frame_duplex_type.value == FDD) {
    return NR_BOTH_SLOT;
  }

  for (int symbol_count = 0; symbol_count < NR_SYMBOLS_PER_SLOT; symbol_count++) {
    if (cfg->tdd_table.max_tdd_periodicity_list[nr_slot].max_num_of_symbol_per_slot_list[symbol_count].slot_config.value == 1) {
      check_slot++;
    }
  }

  if (check_slot == NR_SYMBOLS_PER_SLOT) {
    return (NR_UPLINK_SLOT);
  }

  check_slot = 0;

  for (int symbol_count = 0; symbol_count < NR_SYMBOLS_PER_SLOT; symbol_count++) {
    if (cfg->tdd_table.max_tdd_periodicity_list[nr_slot].max_num_of_symbol_per_slot_list[symbol_count].slot_config.value == 0) {
      check_slot++;
    }
  }

  if (check_slot == NR_SYMBOLS_PER_SLOT) {
    return (NR_DOWNLINK_SLOT);
  } else {
    return (NR_MIXED_SLOT);
  }
}

void do_tdd_config_sim(PHY_VARS_gNB *gNB, int mu)
{
  frame_structure_t fs = {.frame_type = TDD};
  fs.numb_slots_frame = (1 << mu) * NR_NUMBER_OF_SUBFRAMES_PER_FRAME;
  fs.numb_period_frame = get_nb_periods_per_frame(gNB->gNB_config.tdd_table.tdd_period.value);
  fs.numb_slots_period = fs.numb_slots_frame / fs.numb_period_frame;
  tdd_period_config_t *pc = &fs.period_cfg;
  // Set DL/UL slots
  switch (mu)
  {
  case 0:
    pc->num_dl_slots = 7;
    pc->num_ul_slots = 2;
    break;
  
  case 1:
    pc->num_dl_slots = 7;
    pc->num_ul_slots = 2;
    break;

  case 3:
    pc->num_dl_slots = 7;
    pc->num_ul_slots = 2;
    break;

  default:
    break;
  }
  for (int i = 0; i < pc->num_dl_slots; i++) {
    pc->tdd_slot_bitmap[i].slot_type = TDD_NR_DOWNLINK_SLOT;
    pc->tdd_slot_bitmap[i].num_dl_symbols = 0;
    pc->tdd_slot_bitmap[i].num_ul_symbols = 0;
  }
  for (int i = pc->num_dl_slots; i < fs.numb_slots_period - pc->num_ul_slots; i++) {
    pc->tdd_slot_bitmap[i].slot_type = TDD_NR_MIXED_SLOT;
    pc->tdd_slot_bitmap[i].num_dl_symbols = 6;
    pc->tdd_slot_bitmap[i].num_ul_symbols = 4;
  }
  for (int i = fs.numb_slots_period - pc->num_ul_slots; i < fs.numb_slots_period; i++) {
    pc->tdd_slot_bitmap[i].slot_type = TDD_NR_UPLINK_SLOT;
    pc->tdd_slot_bitmap[i].num_dl_symbols = 0;
    pc->tdd_slot_bitmap[i].num_ul_symbols = 0;
  }
  set_tdd_config_nr(&gNB->gNB_config, &fs);
}
