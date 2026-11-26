/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */
#include "nr_mac_gNB.h"
#include "nr_radio_config.h"
#include "common/utils/LOG/log.h"
#include <limits.h>

void nr_sib9_slot(nr_cell_sched_t *cell, int frame, int slot, const nr_slot_time_t *time)
{
  nr_sib9_window_t *state = &cell->common_channels.sib9_window;
  const int spf = cell->frame_structure.numb_slots_frame;
  state->context_valid = false;
  if (spf <= 0 || frame < 0 || frame >= 1024 || slot < 0 || slot >= spf) {
    state->observed = state->attempted = state->valid = false;
    return;
  }
  const int cycle = 1024 * spf, current = frame * spf + slot;
  if (!state->observed || (current - state->last_slot + cycle) % cycle != 1) {
    state->absolute_slot = current;
    state->attempted = state->valid = false;
  } else {
    state->absolute_slot++;
  }
  state->observed = true;
  state->last_slot = current;
  if (!time || time->tx_frame != frame || time->tx_slot != slot || time->rx_frame >= 1024 || time->rx_slot >= spf)
    return;
  const int rx = time->rx_frame * spf + time->rx_slot;
  const int lead = (current - rx + cycle) % cycle;
  if (lead < 1 || lead >= cycle / 2)
    return;
  state->rx_to_tx = lead;
  state->context_valid = true;
}

const uint8_t *nr_sib9_payload(nr_cell_sched_t *cell, int rel_frame, int rel_slot, int window_slots, int *length)
{
  NR_COMMON_channels_t *cc = &cell->common_channels;
  nr_sib9_window_t *state = &cc->sib9_window;
  *length = 0;
  if (!cc->sib9_si || cc->sib9_bcch_length <= 0)
    return NULL;
  if (cc->sib9_mode == NR_SIB9_STATIC) {
    *length = cc->sib9_bcch_length;
    return cc->sib9_bcch_pdu;
  }
  const int spf = cell->frame_structure.numb_slots_frame;
  if (!state->observed || spf <= 0 || 10000000 % spf || rel_frame < 0 || rel_slot < 0 || rel_slot >= spf || window_slots <= 0
      || (int64_t)rel_frame * spf + rel_slot >= window_slots)
    return NULL;
  const int64_t start = state->absolute_slot - (int64_t)rel_frame * spf - rel_slot;
  if (!state->attempted || state->window != start) {
    state->attempted = true;
    state->valid = false;
    state->window = start;
    NR_SystemInformation_IEs_t *candidate = NULL;
    struct timespec now;
    if (!state->context_valid || clock_gettime(CLOCK_REALTIME, &now) != 0)
      goto failed;
    /* TS 38.331 section 6.3.1: UTC refers to the frame boundary at or after SI-window end. */
    const int64_t end = start + window_slots;
    const int64_t target = end + (spf - (end % spf + spf) % spf) % spf;
    /* The host sample estimates RX-buffer end. The current TX slot is ahead
     * of RX-slot start, hence subtract one slot before projecting to target. */
    const int64_t distance = target - state->absolute_slot + state->rx_to_tx - 1;
    if (distance < 0 || distance > INT64_MAX / (10000000 / spf))
      goto failed;
    uint64_t utc;
    if (!nr_sib9_time_to_utc(&now, distance * (10000000 / spf), &utc)
        || asn_copy(&asn_DEF_NR_SystemInformation_IEs, (void **)&candidate, cc->sib9_si) != 0)
      goto failed;
    NR_SIB9_t *sib9 = candidate->sib_TypeAndInfo.list.array[0]->choice.sib9;
    if (asn_uint642INTEGER(&sib9->timeInfo->timeInfoUTC, utc) != 0)
      goto failed;
    uint8_t pdu[NR_SIB9_MAX_BYTES] = {0};
    const int size = encode_sysinfo_ie(candidate, pdu, sizeof(pdu));
    if (size <= 0)
      goto failed;
    ASN_STRUCT_FREE(asn_DEF_NR_SystemInformation_IEs, cc->sib9_si);
    cc->sib9_si = candidate;
    memcpy(cc->sib9_bcch_pdu, pdu, sizeof(pdu));
    cc->sib9_bcch_length = size;
    state->valid = true;
    if (state->error_reported)
      LOG_I(NR_MAC, "Cell %lu SIB9 system UTC refresh recovered\n", cell->nr_cellid);
    state->error_reported = false;
    goto ready;
  failed:
    ASN_STRUCT_FREE(asn_DEF_NR_SystemInformation_IEs, candidate);
    if (!state->error_reported)
      LOG_W(NR_MAC, "Cell %lu SIB9 UTC refresh unavailable; failed windows are skipped\n", cell->nr_cellid);
    state->error_reported = true;
  }
ready:
  if (!state->valid)
    return NULL;
  *length = cc->sib9_bcch_length;
  return cc->sib9_bcch_pdu;
}
