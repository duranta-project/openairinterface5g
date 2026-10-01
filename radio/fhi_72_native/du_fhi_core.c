/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "du_fhi_core.h"
#include <string.h>
#include "common/utils/LOG/log.h"
#include "common/utils/nr/nr_common.h"

#define DU_FHI_MAX_ANTENNAS 4

uint64_t du_fhi_absolute_slot_number(uint64_t hyper_frame, int frame, int slot, int slots_per_frame)
{
  return (hyper_frame * 1024ULL + (uint64_t)frame) * (uint64_t)slots_per_frame + (uint64_t)slot;
}

bool du_fhi_is_dl_symbol(const du_fh_config_t *cfg, int slot, int symbol)
{
  if (cfg->fdd_mode)
    return true;
  const du_fh_tdd_pattern_t *tdd = &cfg->tdd_pattern;
  int period_len = tdd->tdd_pattern_length_slots > 0 ? tdd->tdd_pattern_length_slots : 1;
  int slot_in_period = slot % period_len;
  if (slot_in_period < (int)tdd->num_dl_slots)
    return true;
  if (slot_in_period == (int)tdd->num_dl_slots)
    return symbol < (int)tdd->num_dl_symbols;
  return false;
}

bool du_fhi_is_ul_symbol(const du_fh_config_t *cfg, int slot, int symbol)
{
  if (cfg->fdd_mode)
    return true;
  const du_fh_tdd_pattern_t *tdd = &cfg->tdd_pattern;
  int period_len = tdd->tdd_pattern_length_slots > 0 ? tdd->tdd_pattern_length_slots : 1;
  int slot_in_period = slot % period_len;
  if (slot_in_period > (int)tdd->num_dl_slots)
    return true;
  if (slot_in_period == (int)tdd->num_dl_slots)
    return symbol >= (NR_SYMBOLS_PER_SLOT - (int)tdd->num_ul_symbols);
  return false;
}

// The anchor is fh_timer's real position *at the instant of the caller's query*, which can
// differ from the caller's own frame by a slot or two; near the 1023->0 wrap that small skew
// can straddle the boundary, so correct hyper_frame to match whichever side of the wrap the
// caller's frame is actually on. Split out from du_fhi_query_hyper_frame() so this arithmetic
// is testable without a real du_fh handle/system clock.
int du_fhi_hyper_frame_wrap_adjustment(uint32_t anchor_frame, int caller_frame)
{
  int diff = (int)anchor_frame - caller_frame;
  if (diff > 512)
    return 1;
  if (diff < -512)
    return -1;
  return 0;
}

uint64_t du_fhi_query_hyper_frame(void *du_fh_handle, int frame)
{
  uint64_t hyper_frame;
  uint32_t anchor_frame, anchor_slot;
  struct timespec ts;
  du_fh_get_utc_anchor_point(du_fh_handle, &hyper_frame, &anchor_frame, &anchor_slot, &ts);
  return hyper_frame + du_fhi_hyper_frame_wrap_adjustment(anchor_frame, frame);
}

uint64_t du_fhi_advance_ul_schedule_lookahead(du_fhi_state_t *st, int frame, int slot, int nb_rx)
{
  int slots_per_frame = 10 << st->cfg.numerology;
  uint64_t hyper_frame = du_fhi_query_hyper_frame(st->du_fh_handle, frame);
  uint64_t target = du_fhi_absolute_slot_number(hyper_frame, frame, slot, slots_per_frame) + st->ul_lookahead_slots;

  uint64_t start = st->have_scheduled ? st->last_scheduled_absolute_slot + 1 : target;
  if (start > target)
    start = target;

  du_tx_dl_section_t section = {.beam_id = 0, .start_prb = 0, .num_prb = st->cfg.num_prbs, .section_id = 0};
  int nb_rx_bounded = nb_rx > DU_FHI_MAX_ANTENNAS ? DU_FHI_MAX_ANTENNAS : nb_rx;

  for (uint64_t abs_slot = start; abs_slot <= target; abs_slot++) {
    uint64_t abs_frame_num = abs_slot / (uint64_t)slots_per_frame;
    int t_slot = (int)(abs_slot % (uint64_t)slots_per_frame);
    uint64_t t_hyper_frame = abs_frame_num / 1024ULL;
    int t_frame = (int)(abs_frame_num % 1024ULL);

    for (int symbol = 0; symbol < NR_SYMBOLS_PER_SLOT; symbol++) {
      if (!du_fhi_is_ul_symbol(&st->cfg, t_slot, symbol))
        continue;
      int abs_symbol_in_frame = t_slot * NR_SYMBOLS_PER_SLOT + symbol;
      uint64_t absolute_symbol =
          (t_hyper_frame * 1024ULL + (uint64_t)t_frame) * (uint64_t)slots_per_frame * NR_SYMBOLS_PER_SLOT + abs_symbol_in_frame;
      for (int ant = 0; ant < nb_rx_bounded; ant++) {
        du_fh_schedule_ul_grant(st->du_fh_handle, t_hyper_frame, t_frame, t_slot, symbol, ant, &section, 1);
        du_fh_expect_ul_symbol(st->du_fh_handle, absolute_symbol, ant, 0, 0, st->cfg.num_prbs, st->cfg.comp_type, st->cfg.iq_width);
      }
    }
  }

  st->last_scheduled_absolute_slot = target;
  st->have_scheduled = true;
  return target;
}

int du_fhi_count_ul_symbols_in_slot(const du_fh_config_t *cfg, int slot)
{
  int count = 0;
  for (int symbol = 0; symbol < NR_SYMBOLS_PER_SLOT; symbol++)
    if (du_fhi_is_ul_symbol(cfg, slot, symbol))
      count++;
  return count;
}
