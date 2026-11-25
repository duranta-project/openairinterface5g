/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "du_fhi_core.h"
#include "common/config/config_userapi.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// OAI Linkage Satisfiers
void exit_function(const char *file, const char *function, const int line, const char *s, const int assertflag)
{
  fprintf(stderr, "Error at %s:%s:%d - %s\n", file, function, line, s ? s : "None");
  exit(1);
}
configmodule_interface_t *uniqCfg = NULL;

static du_fh_config_t make_tdd_cfg(void)
{
  du_fh_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.fdd_mode = false;
  cfg.tdd_pattern.num_dl_slots = 3;
  cfg.tdd_pattern.num_ul_slots = 1;
  cfg.tdd_pattern.num_dl_symbols = 6;
  cfg.tdd_pattern.num_ul_symbols = 4;
  cfg.tdd_pattern.tdd_pattern_length_slots = 5;
  return cfg;
}

static void test_tdd_classification(void)
{
  printf("Testing TDD symbol classification...\n");
  du_fh_config_t cfg = make_tdd_cfg();

  // Slots 0-2: fully DL.
  for (int slot = 0; slot < 3; slot++) {
    for (int symbol = 0; symbol < NR_SYMBOLS_PER_SLOT; symbol++) {
      assert(du_fhi_is_dl_symbol(&cfg, slot, symbol));
      assert(!du_fhi_is_ul_symbol(&cfg, slot, symbol));
    }
  }

  // Slot 3: mixed -- symbols [0,6) DL, [10,14) UL, [6,10) guard (neither).
  for (int symbol = 0; symbol < 6; symbol++) {
    assert(du_fhi_is_dl_symbol(&cfg, 3, symbol));
    assert(!du_fhi_is_ul_symbol(&cfg, 3, symbol));
  }
  for (int symbol = 6; symbol < 10; symbol++) {
    assert(!du_fhi_is_dl_symbol(&cfg, 3, symbol));
    assert(!du_fhi_is_ul_symbol(&cfg, 3, symbol));
  }
  for (int symbol = 10; symbol < NR_SYMBOLS_PER_SLOT; symbol++) {
    assert(!du_fhi_is_dl_symbol(&cfg, 3, symbol));
    assert(du_fhi_is_ul_symbol(&cfg, 3, symbol));
  }

  // Slot 4: fully UL.
  for (int symbol = 0; symbol < NR_SYMBOLS_PER_SLOT; symbol++) {
    assert(!du_fhi_is_dl_symbol(&cfg, 4, symbol));
    assert(du_fhi_is_ul_symbol(&cfg, 4, symbol));
  }

  // Periodicity: slot 8 (= 3 mod 5) behaves like slot 3.
  assert(du_fhi_is_dl_symbol(&cfg, 8, 0));
  assert(du_fhi_is_ul_symbol(&cfg, 8, 13));

  printf("PASS\n");
}

// UL slots end the period: with no mixed slot they still follow the guard, not the DL slots
static void test_tdd_ul_at_period_end(void)
{
  printf("Testing TDD UL slots at the end of the period...\n");
  du_fh_config_t cfg = make_tdd_cfg();
  cfg.tdd_pattern.num_dl_slots = 3;
  cfg.tdd_pattern.num_ul_slots = 2;
  cfg.tdd_pattern.num_dl_symbols = 0;
  cfg.tdd_pattern.num_ul_symbols = 0;
  for (int symbol = 0; symbol < NR_SYMBOLS_PER_SLOT; symbol++) {
    assert(du_fhi_is_ul_symbol(&cfg, 3, symbol));
    assert(du_fhi_is_ul_symbol(&cfg, 4, symbol));
    assert(!du_fhi_is_ul_symbol(&cfg, 2, symbol));
  }

  // Two DL slots, two guard slots, one UL slot
  cfg.tdd_pattern.num_dl_slots = 2;
  cfg.tdd_pattern.num_ul_slots = 1;
  for (int symbol = 0; symbol < NR_SYMBOLS_PER_SLOT; symbol++) {
    for (int slot = 2; slot < 4; slot++) {
      assert(!du_fhi_is_dl_symbol(&cfg, slot, symbol));
      assert(!du_fhi_is_ul_symbol(&cfg, slot, symbol));
    }
    assert(du_fhi_is_ul_symbol(&cfg, 4, symbol));
  }
  printf("PASS\n");
}

static void test_fdd_classification(void)
{
  printf("Testing FDD symbol classification...\n");
  du_fh_config_t cfg = make_tdd_cfg();
  cfg.fdd_mode = true;

  for (int slot = 0; slot < 20; slot++) {
    for (int symbol = 0; symbol < NR_SYMBOLS_PER_SLOT; symbol++) {
      assert(du_fhi_is_dl_symbol(&cfg, slot, symbol));
      assert(du_fhi_is_ul_symbol(&cfg, slot, symbol));
    }
  }
  printf("PASS\n");
}

// Fills a period from one letter per slot: D full DL, U full UL, S the given partial slot
static void fill_period(symbol_direction_t dirs[][NR_SYMBOLS_PER_SLOT], const char *slots, int dl_syms, int ul_syms)
{
  for (int slot = 0; slots[slot]; slot++) {
    for (int sym = 0; sym < NR_SYMBOLS_PER_SLOT; sym++) {
      symbol_direction_t dir = SYMBOL_DIR_GU;
      if (slots[slot] == 'D' || (slots[slot] == 'S' && sym < dl_syms))
        dir = SYMBOL_DIR_DL;
      if (slots[slot] == 'U' || (slots[slot] == 'S' && sym >= NR_SYMBOLS_PER_SLOT - ul_syms))
        dir = SYMBOL_DIR_UL;
      dirs[slot][sym] = dir;
    }
  }
}

static void test_derive_tdd_pattern(void)
{
  printf("Testing TDD pattern derivation...\n");
  symbol_direction_t dirs[10][NR_SYMBOLS_PER_SLOT];
  du_fh_tdd_pattern_t tdd;

  fill_period(dirs, "DDDSU", 6, 4);
  assert(du_fhi_derive_tdd_pattern(5, dirs, &tdd));
  assert(tdd.num_dl_slots == 3 && tdd.num_dl_symbols == 6 && tdd.num_ul_symbols == 4 && tdd.num_ul_slots == 1);

  // DL symbols and UL symbols in two consecutive slots: neither count may clear the other
  fill_period(dirs, "DDSSU", 6, 0);
  fill_period(dirs + 3, "S", 0, 4);
  assert(du_fhi_derive_tdd_pattern(5, dirs, &tdd));
  assert(tdd.num_dl_slots == 2 && tdd.num_dl_symbols == 6 && tdd.num_ul_symbols == 4 && tdd.num_ul_slots == 1);

  fill_period(dirs, "DDDDDDDDDU", 0, 0);
  assert(du_fhi_derive_tdd_pattern(10, dirs, &tdd));
  assert(tdd.num_dl_slots == 9 && tdd.num_dl_symbols == 0 && tdd.num_ul_symbols == 0 && tdd.num_ul_slots == 1);

  // Two patterns back to back: UL slots in the middle of the period
  fill_period(dirs, "DDDSUDDDSU", 6, 4);
  assert(!du_fhi_derive_tdd_pattern(10, dirs, &tdd));

  // A flexible symbol inside an otherwise DL slot
  fill_period(dirs, "DDDSU", 6, 4);
  dirs[1][13] = SYMBOL_DIR_GU;
  assert(!du_fhi_derive_tdd_pattern(5, dirs, &tdd));
  printf("PASS\n");
}

int main(void)
{
  test_tdd_classification();
  test_tdd_ul_at_period_end();
  test_fdd_classification();
  test_derive_tdd_pattern();
  printf("--- All du_fhi_core symbol tests passed ---\n");
  return 0;
}
