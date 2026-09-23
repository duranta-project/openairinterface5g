/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_mac_common.h"
#include "common/utils/nr/nr_common.h"

#define MAX_NUM_PERIODS 8

uint16_t get_ul_bitmap(const frame_structure_t *fs, int slot)
{
  const int slot_period = slot % fs->numb_slots_period;
  const tdd_bitmap_t *bm = &fs->period_cfg.tdd_slot_bitmap[slot_period];
  /* For some reason, we only store the number of symbols if it's mixed */
  const int num_ul_symbols = bm->slot_type == TDD_NR_MIXED_SLOT ? bm->num_ul_symbols : 14;
  return SL_to_bitmap(14 - num_ul_symbols, num_ul_symbols);
}

int get_slots_per_frame_from_scs(int scs)
{
  const int nr_slots_per_frame[5] = {10, 20, 40, 80, 160};
  return nr_slots_per_frame[scs];
}

const float tdd_ms_period_pattern[] = {0.5, 0.625, 1.0, 1.25, 2.0, 2.5, 5.0, 10.0};
const float tdd_ms_period_ext[] = {3.0, 4.0};

/**
 * @brief Retrieves the periodicity in milliseconds for the given TDD pattern
 *        depending on the presence of the extension
 * @param pattern Pointer to the NR_TDD_UL_DL_Pattern_t pattern structure
 * @return Periodicity value in milliseconds.
 */
static float get_tdd_periodicity(NR_TDD_UL_DL_Pattern_t *pattern)
{
  if (!pattern->ext1) {
    LOG_D(NR_MAC, "Setting TDD configuration period to dl_UL_TransmissionPeriodicity %ld\n", pattern->dl_UL_TransmissionPeriodicity);
    return tdd_ms_period_pattern[pattern->dl_UL_TransmissionPeriodicity];
  } else {
    DevAssert(pattern->ext1->dl_UL_TransmissionPeriodicity_v1530 != NULL);
    LOG_D(NR_MAC,
          "Setting TDD configuration period to dl_UL_TransmissionPeriodicity_v1530 %ld\n",
          *pattern->ext1->dl_UL_TransmissionPeriodicity_v1530);
    return tdd_ms_period_ext[*pattern->ext1->dl_UL_TransmissionPeriodicity_v1530];
  }
}

/**
 * @brief Determines the TDD period index based on pattern periodicities
 * @note  Not applicabile to pattern extension
 * @param tdd Pointer to the NR_TDD_UL_DL_ConfigCommon_t containing patterns
 * @return Index of the TDD period in tdd_ms_period_pattern
 */
int get_tdd_period_idx(NR_TDD_UL_DL_ConfigCommon_t *tdd)
{
  int tdd_period_idx = 0;
  float pattern1_ms = get_tdd_periodicity(&tdd->pattern1);
  float pattern2_ms = tdd->pattern2 ? get_tdd_periodicity(tdd->pattern2) : 0.0;
  bool found_match = false;
  // Find matching TDD period in the predefined list of periodicities
  for (int i = 0; i < MAX_NUM_PERIODS; i++) {
    if ((pattern1_ms + pattern2_ms) == tdd_ms_period_pattern[i]) {
      tdd_period_idx = i;
      LOG_I(NR_MAC,
            "TDD period index = %d, based on the sum of dl_UL_TransmissionPeriodicity "
            "from Pattern1 (%f ms) and Pattern2 (%f ms): Total = %f ms\n",
            tdd_period_idx,
            pattern1_ms,
            pattern2_ms,
            pattern1_ms + pattern2_ms);
      found_match = true;
      break;
    }
  }
  // Assert if no match was found
  AssertFatal(found_match, "The sum of pattern1_ms and pattern2_ms does not match any value in tdd_ms_period_pattern");
  return tdd_period_idx;
}

/**
 * @brief Configures the TDD period, including bitmap, in the frame structure
 *
 * This function sets the TDD period configuration for DL, UL, and mixed slots in
 * the specified frame structure instance, according to the given pattern, and
 * updates the bitmap.
 *
 * @param pattern NR_TDD_UL_DL_Pattern_t configuration containing TDD slots and symbols configuration
 * @param fs frame_structure_t pointer
 * @param curr_total_slot current position in the slot bitmap to start from
 *
 * @return Number of slots in the configured TDD period
 */
static uint8_t set_tdd_bmap_period(const NR_TDD_UL_DL_Pattern_t *pattern, tdd_period_config_t *pc, int8_t curr_total_slot)
{
  int8_t n_dl_slot = pattern->nrofDownlinkSlots;
  int8_t n_ul_slot = pattern->nrofUplinkSlots;
  int8_t n_dl_symbols = pattern->nrofDownlinkSymbols;
  int8_t n_ul_symbols = pattern->nrofUplinkSymbols;

  // Total slots in the period: if DL/UL symbols are present, is mixed slot
  const bool has_mixed_slot = (n_ul_symbols + n_dl_symbols) > 0;
  int8_t total_slot = has_mixed_slot ? n_dl_slot + n_ul_slot + 1 : n_dl_slot + n_ul_slot;

  /** Update TDD period configuration:
   * mixed slots with DL symbols are counted as DL slots
   * mixed slots with UL symbols are counted as UL slots */
  pc->num_dl_slots += (n_dl_slot + (n_dl_symbols > 0));
  pc->num_ul_slots += (n_ul_slot + (n_ul_symbols > 0));

  // Populate the slot bitmap for each slot in the TDD period
  for (int i = 0; i < total_slot; i++) {
    tdd_bitmap_t *bitmap = &pc->tdd_slot_bitmap[i + curr_total_slot];
    if (i < n_dl_slot)
      bitmap->slot_type = TDD_NR_DOWNLINK_SLOT;
    else if ((i == n_dl_slot) && has_mixed_slot) {
      bitmap->slot_type = TDD_NR_MIXED_SLOT;
      bitmap->num_dl_symbols = n_dl_symbols;
      bitmap->num_ul_symbols = n_ul_symbols;
    } else if (n_ul_slot)
      bitmap->slot_type = TDD_NR_UPLINK_SLOT;
  }

  LOG_I(NR_MAC,
        "Set TDD configuration period to: %d DL slots, %d UL slots, %d slots per period (NR_TDD_UL_DL_Pattern is %d DL slots, %d "
        "UL slots, %d DL symbols, %d UL symbols)\n",
        pc->num_dl_slots,
        pc->num_ul_slots,
        total_slot,
        n_dl_slot,
        n_ul_slot,
        n_dl_symbols,
        n_ul_symbols);
  AssertFatal(n_dl_symbols + n_ul_symbols < 14,
              "number of mixed slot symbols must be smaller than 14: DL %d, UL %d\n",
              n_dl_symbols,
              n_ul_symbols);

  return total_slot;
}

/**
 * @brief Configures TDD patterns (1 or 2) in the frame structure
 *
 * This function sets up the TDD configuration in the frame structure by applying
 * DL and UL patterns as defined in NR_TDD_UL_DL_ConfigCommon.
 * It handles both TDD pattern1 and pattern2.
 *
 * @param tdd NR_TDD_UL_DL_ConfigCommon_t pointer containing pattern1 and pattern2
 * @param fs  Pointer to the frame structure to update with the configured TDD patterns.
 *
 * @return void
 */
static void config_tdd_patterns(const NR_TDD_UL_DL_ConfigCommon_t *tdd, frame_structure_t *fs)
{
  int num_of_patterns = 1;
  uint8_t nb_slots_p2 = 0;
  // Reset num dl/ul slots
  tdd_period_config_t *pc = &fs->period_cfg;
  pc->num_dl_slots = 0;
  pc->num_ul_slots = 0;
  // Pattern1
  uint8_t nb_slots_p1 = set_tdd_bmap_period(&tdd->pattern1, pc, 0);
  // Pattern2
  if (tdd->pattern2) {
    num_of_patterns++;
    nb_slots_p2 = set_tdd_bmap_period(tdd->pattern2, pc, nb_slots_p1);
  }
  LOG_I(NR_MAC,
        "Configured %d TDD patterns (total slots: pattern1 = %d, pattern2 = %d)\n",
        num_of_patterns,
        nb_slots_p1,
        nb_slots_p2);
  AssertFatal(nb_slots_p1 + nb_slots_p2 == fs->numb_slots_period,
              "total number of slots must equal to %d: p1 %d + p2 %d\n",
              fs->numb_slots_period,
              nb_slots_p1,
              nb_slots_p2);
}

/**
 * @brief Configures the frame structure and the NFAPI config request
 *        depending on the duplex mode. If TDD, does the TDD patterns
 *        and TDD periods configuration.
 *
 * @param mu numerology
 * @param scc pointer to scc
 * @param tdd_period TDD period
 * @param frame_type type of frame structure (FDD or TDD)
 * @param fs  pointer to the frame structure to update
 */
void config_frame_structure(int mu,
                            const NR_TDD_UL_DL_ConfigCommon_t *tdd_UL_DL_ConfigurationCommon,
                            uint8_t tdd_period,
                            uint8_t frame_type,
                            frame_structure_t *fs)
{
  fs->numb_slots_frame = get_slots_per_frame_from_scs(mu);
  fs->frame_type = frame_type;
  if (frame_type == TDD) {
    fs->numb_period_frame = get_nb_periods_per_frame(tdd_period);
    fs->numb_slots_period = fs->numb_slots_frame / fs->numb_period_frame;
    config_tdd_patterns(tdd_UL_DL_ConfigurationCommon, fs);
  } else { // FDD
    fs->numb_period_frame = 1;
    fs->numb_slots_period = fs->numb_slots_frame;
  }
  AssertFatal(fs->numb_period_frame > 0, "Frame configuration cannot be configured!\n");
}

/**
 * @brief Returns true for:
 *        (1) FDD
 *        (2) Mixed slot with UL symbols
 *        (3) Full UL slot
 */
bool is_ul_slot(const slot_t slot, const frame_structure_t *fs)
{
  if (fs->frame_type == FDD)
    return true;
  const tdd_period_config_t *pc = &fs->period_cfg;
  slot_t s = get_slot_idx_in_period(slot, fs);
  return ((is_mixed_slot(s, fs) && pc->tdd_slot_bitmap[s].num_ul_symbols)
          || (pc->tdd_slot_bitmap[s].slot_type == TDD_NR_UPLINK_SLOT));
}

/**
 * @brief Returns true for:
 *        (1) FDD
 *        (2) Mixed slot with DL symbols
 *        (3) Full DL slot
 */
bool is_dl_slot(const slot_t slot, const frame_structure_t *fs)
{
  if (fs->frame_type == FDD)
    return true;
  const tdd_period_config_t *pc = &fs->period_cfg;
  slot_t s = get_slot_idx_in_period(slot, fs);
  return ((is_mixed_slot(s, fs) && pc->tdd_slot_bitmap[s].num_dl_symbols)
          || (pc->tdd_slot_bitmap[s].slot_type == TDD_NR_DOWNLINK_SLOT));
}

/**
 * @brief Returns true for:
 *        (1) Mixed slot with DL and/or UL symbols
 */
bool is_mixed_slot(const slot_t slot, const frame_structure_t *fs)
{
  if (fs->frame_type == FDD)
    return false;
  slot_t s = get_slot_idx_in_period(slot, fs);
  const tdd_period_config_t *pc = &fs->period_cfg;
  return pc->tdd_slot_bitmap[s].slot_type == TDD_NR_MIXED_SLOT;
}

/**
 * @brief Get the first UL slot index in period
 * @param fs frame structure
 * @param mixed indicates whether to include in the count also mixed slot with UL symbols or only full UL slot
 * @return slot index
 */
int get_first_ul_slot(const frame_structure_t *fs, bool mixed)
{
  DevAssert(fs);

  if (fs->frame_type == TDD) {
    for (int i = 0; i < fs->numb_slots_period; i++) {
      if ((mixed && is_ul_slot(i, fs)) || fs->period_cfg.tdd_slot_bitmap[i].slot_type == TDD_NR_UPLINK_SLOT) {
        return i;
      }
    }
  }

  return 0; // FDD
}

