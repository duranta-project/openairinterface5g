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

int main(void)
{
  test_tdd_classification();
  test_fdd_classification();
  printf("--- All du_fhi_core symbol tests passed ---\n");
  return 0;
}
