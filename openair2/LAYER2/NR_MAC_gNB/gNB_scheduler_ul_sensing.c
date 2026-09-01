/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Sensing-aware UL scheduling: a TDA selector that prefers shorter TDAs
 * to free symbols for sensing, the per-slot scan that publishes the free tiles,
 * and (Aerial only) a dummy PUSCH that forces the L1 to capture sensing IQ.
 */

#include "gNB_scheduler_ul_sensing.h"
#include "LAYER2/NR_MAC_gNB/mac_proto.h"
#include "LAYER2/NR_MAC_COMMON/nr_mac_common.h"
#include "common/utils/nr/nr_common.h"

/* Is this (absolute) slot hard-reserved for sensing, i.e. its slot-in-TDD-period
 * is in the configured sensing_target_slots[] list? When true, the scheduler
 * blocks every UE allocator from it, then frees it just before the scan. Public
 * seam for the UL conflict-tolerance sites (PRACH/Msg3/SRS) and reserve/restore. */
bool nr_mac_ul_slot_is_sensing_reserved(const nr_cell_sched_t *cell, int slot)
{
  const int period = cell->frame_structure.numb_slots_period;
  if (period <= 0)
    return false;
  const int slot_in_period = slot % period;
  const nr_mac_config_t *rc = &cell->radio_config;
  for (int i = 0; i < rc->num_sensing_target_slots; i++)
    if (rc->sensing_target_slots[i] == slot_in_period)
      return true;
  return false;
}

/* Beams active this period (1 unless multi-beam is configured). */
static inline int nr_mac_active_beams(const nr_cell_sched_t *cell)
{
  return cell->beam_info.beam_allocation ? cell->beam_info.beams_per_period : 1;
}

/* Hard-reserve the just-reseeded UL slot for sensing: stamp every beam's row
 * 0x3FFF so no UE allocator can claim it. No-op unless the slot is reserved.
 * prev_slot is the absolute slot the scheduler reseeded. */
void nr_mac_sensing_reserve_ul_slot(nr_cell_sched_t *cell, int prev_slot, frame_t frame)
{
  if (!nr_mac_ul_slot_is_sensing_reserved(cell, prev_slot))
    return;
  const int size = cell->vrb_map_UL_size;
  const int slots_frame = cell->frame_structure.numb_slots_frame;
  const int num_beams = nr_mac_active_beams(cell);
  for (int b = 0; b < num_beams; b++) {
    uint16_t *row = &cell->common_channels.vrb_map_UL[b][(prev_slot % size) * MAX_BWP_SIZE];
    for (int prb = 0; prb < MAX_BWP_SIZE; prb++)
      row[prb] = 0x3FFF;
  }
  if ((frame & 0x7F) == 0)
    LOG_I(NR_MAC,
          "Sensing slot reserved: %d.%d -- UE UL blocked\n",
          (prev_slot / slots_frame) % MAX_FRAME_NUMBER,
          prev_slot % slots_frame);
}

/* Undo the reserve for the current slot just before the scan, restoring the row
 * to ulprbbl so the scanner sees a clean full-PRB window. No-op unless reserved. */
void nr_mac_sensing_restore_ul_slot(nr_cell_sched_t *cell, frame_t frame, slot_t slot)
{
  if (!nr_mac_ul_slot_is_sensing_reserved(cell, slot))
    return;
  const int slots_frame = cell->frame_structure.numb_slots_frame;
  const int buf_idx = ul_buffer_index(frame, slot, slots_frame, cell->vrb_map_UL_size);
  const int num_beams = nr_mac_active_beams(cell);
  for (int b = 0; b < num_beams; b++) {
    uint16_t *row = &cell->common_channels.vrb_map_UL[b][buf_idx * MAX_BWP_SIZE];
    /* Reset to the static block list so the reserved slot is clean for sensing. */
    memcpy(row, &cell->ulprbbl, sizeof(uint16_t) * MAX_BWP_SIZE);
  }
}

#ifdef E3_AGENT
#include <stdatomic.h>
#include <pthread.h>
#include <inttypes.h>
#include "common/ran_context.h"
#include "openair2/E3AP/service_models/pub_channel.h"
#include "common/utils/seqlock.h"
/* The consumer accessor API + nr_mac_sensing_publish_meta_t come from the
 * sensing types header (pulled in via gNB_scheduler_ul_sensing.h). */

/* ---- Sensing publish notification ----
 * Wakes a consumer (the Spectrum SM worker) when a new sensing-range write
 * happens. Only the small metadata travels here, in a ring indexed by the
 * channel's publish_sequence: the MAC tick can bunch several UL slots
 * back-to-back, so a late consumer drains every publish in order instead of
 * keeping only the newest. 8 entries outlast any observed burst while the
 * deepest catch-up stays inside the 10 ms after which the per-(beam, slot)
 * range storage is recycled. See openair2/E3AP/service_models/pub_channel.h. */
#define SENSING_PUB_RING_SLOTS 8u
static pub_channel_t g_sensing_chan = PUB_CHANNEL_INIT;
static nr_mac_sensing_publish_meta_t g_sensing_ring[SENSING_PUB_RING_SLOTS];

/* Reader for the sensing-range snapshot (declared in the sensing types header).
 * Lock-free seqlock against the writer below; bounded spin (8 tries) so the
 * worker stays responsive even when it lands on the slot being written. */
bool nr_mac_get_sensing_ranges(int mod_id, int beam, int slot, sensing_range_t *out_ranges, int max_out, uint8_t *out_n)
{
  if (!out_ranges || !out_n || max_out <= 0)
    return false;
  if (beam < 0 || beam >= MAX_NUM_BEAM_PERIODS)
    return false;
  if (!RC.nrmac || !RC.nrmac[mod_id])
    return false;
  NR_COMMON_channels_t *cc = &RC.nrmac[mod_id]->cells[0].common_channels;
  const int idx = slot % NR_MAX_SLOTS_PER_FRAME;

  for (int attempts = 0; attempts < 8; ++attempts) {
    uint32_t begun = seqlock_read_begin(&cc->e3_sensing_seq[beam][idx]);
    if (begun & 1u)
      continue; /* mid-write — retry */

    uint8_t n = cc->e3_n_sensing_ranges[beam][idx];
    if (n > MAX_SENSING_RANGES)
      n = MAX_SENSING_RANGES; /* bound to source width even on a torn read */
    if (n > max_out)
      n = (uint8_t)max_out;
    /* Storage is sensing_range_t, so this copy is type-identical. */
    memcpy(out_ranges, cc->e3_sensing_ranges[beam][idx], n * sizeof(sensing_range_t));

    if (!seqlock_read_retry(&cc->e3_sensing_seq[beam][idx], begun)) {
      *out_n = n;
      return true;
    }
  }
  return false;
}

/* Writer side, called after nr_scan_sensing_tiles(). Stores the (beam, slot)
 * ranges under the seqlock, then wakes one waiter. One call = one wake event. */
void nr_mac_record_sensing_ranges(NR_COMMON_channels_t *cc,
                                  int beam,
                                  int frame,
                                  int slot,
                                  const sensing_range_t *ranges,
                                  int n_ranges)
{
  if (!cc)
    return;
  if (beam < 0 || beam >= MAX_NUM_BEAM_PERIODS)
    return;
  if (n_ranges < 0)
    n_ranges = 0;
  if (n_ranges > MAX_SENSING_RANGES)
    n_ranges = MAX_SENSING_RANGES;
  const int idx = slot % NR_MAX_SLOTS_PER_FRAME;

  /* Publish into the per-(beam, slot) seqlock; lock-free, the random-access
   * reader nr_mac_get_sensing_ranges() retries on a torn read. */
  const uint32_t seq = seqlock_write_begin(&cc->e3_sensing_seq[beam][idx]);
  if (n_ranges > 0) {
    memcpy(cc->e3_sensing_ranges[beam][idx], ranges, (size_t)n_ranges * sizeof(sensing_range_t));
  }
  cc->e3_n_sensing_ranges[beam][idx] = (uint8_t)n_ranges;
  seqlock_write_end(&cc->e3_sensing_seq[beam][idx], seq);

  /* Wake the Spectrum SM worker: stamp this publish's ring slot + bump, all
   * under the lock. The ranges are NOT copied here — consumers fetch them via
   * nr_mac_get_sensing_ranges() under its own per-(beam, slot) seqlock. */
  const uint64_t ts_ns = pub_channel_now_ns();
  pub_channel_lock(&g_sensing_chan);
  nr_mac_sensing_publish_meta_t *m = &g_sensing_ring[(g_sensing_chan.publish_sequence + 1) % SENSING_PUB_RING_SLOTS];
  m->beam = (uint16_t)beam;
  m->frame = (uint16_t)frame;
  m->slot = (uint16_t)slot;
  m->timestamp_ns = ts_ns;
  pub_channel_publish_and_wake(&g_sensing_chan);
  pub_channel_unlock(&g_sensing_chan);
}

/* Block until a publish newer than *inout_seq arrives (timeout_ns: 0 = poll,
 * UINT64_MAX = forever, else a CLOCK_MONOTONIC deadline). *inout_seq is the
 * consumer's read cursor: each true return hands back the oldest unconsumed
 * publish and advances by one, so bunched publishes drain across successive
 * calls. A fresh consumer (cursor 0) starts at the newest; one that fell
 * behind the ring skips ahead to the oldest meta still held. */
bool nr_mac_wait_for_sensing_publish(uint64_t timeout_ns, uint64_t *inout_seq, nr_mac_sensing_publish_meta_t *out_meta)
{
  if (!inout_seq || !out_meta)
    return false;

  nr_mac_sensing_publish_meta_t snap[SENSING_PUB_RING_SLOTS];
  uint64_t newest = *inout_seq;
  if (!pub_channel_wait(&g_sensing_chan, g_sensing_ring, snap, sizeof(snap), timeout_ns, &newest))
    return false;

  uint64_t next = *inout_seq + 1;
  if (*inout_seq == 0) {
    next = newest; /* fresh consumer: no history owed, start at the newest */
  } else if (next + SENSING_PUB_RING_SLOTS <= newest) {
    /* Overran the ring: the older metas are already overwritten. Should not
     * happen (the ring outlasts any observed tick burst); rate-limited. */
    static unsigned overruns;
    if (overruns++ % 100 == 0)
      LOG_W(NR_MAC,
            "sensing publish ring overrun: consumer %" PRIu64 " publishes behind (count %u)\n",
            newest - *inout_seq,
            overruns);
    next = newest - SENSING_PUB_RING_SLOTS + 1;
  }
  *out_meta = snap[next % SENSING_PUB_RING_SLOTS];
  *inout_seq = next;
  return true;
}

/* Force-wake every nr_mac_wait_for_sensing_publish() waiter by bumping
 * publish_seq (so the predicate flips) and broadcasting the condvar; used to
 * unblock indefinite waiters at SM teardown. */
void nr_mac_signal_sensing_shutdown(void)
{
  pub_channel_signal_shutdown(&g_sensing_chan);
}
#endif /* E3_AGENT */

/* OR a (PRB-span x symbol-run) rectangle into occupancy[]: clamp the symbols to
 * the 0..14 grid, then set the symbol bitmap on every in-grid PRB of the span. */
static inline void mark_occupancy_rect(uint16_t *occupancy, int rb_start, int num_rb, int start_symbol, int num_symbols)
{
  if (start_symbol < 0)
    start_symbol = 0;
  if (start_symbol + num_symbols > 14)
    num_symbols = 14 - start_symbol;
  if (num_symbols <= 0)
    return;
  const uint16_t symbol_mask = SL_to_bitmap(start_symbol, num_symbols);
  for (int prb = rb_start; prb < rb_start + num_rb; prb++)
    if (prb >= 0 && prb < MAX_BWP_SIZE)
      occupancy[prb] |= symbol_mask;
}

/* OR the (PRB x symbol) footprint of PUCCH resource `res_id` (both freq hops,
 * absolute carrier PRB) into occupancy[]. Used for the SR and periodic-CSI PUCCH
 * resources, which the UE transmits autonomously on their occasions. */
static void nr_sensing_add_pucch_occupancy(const NR_PUCCH_Config_t *pc,
                                           long res_id,
                                           int bwp_start,
                                           uint16_t occupancy[MAX_BWP_SIZE])
{
  if (pc == NULL || pc->resourceToAddModList == NULL)
    return;
  for (int j = 0; j < pc->resourceToAddModList->list.count; j++) {
    const NR_PUCCH_Resource_t *r = pc->resourceToAddModList->list.array[j];
    if (r->pucch_ResourceId != res_id)
      continue;
    int len = 1; /* PRBs; format0/1/4 occupy 1 PRB */
    uint16_t mask = 0; /* symbol bitmap */
    switch (r->format.present) {
      case NR_PUCCH_Resource__format_PR_format0:
        mask = SL_to_bitmap(r->format.choice.format0->startingSymbolIndex, r->format.choice.format0->nrofSymbols);
        break;
      case NR_PUCCH_Resource__format_PR_format1:
        mask = SL_to_bitmap(r->format.choice.format1->startingSymbolIndex, r->format.choice.format1->nrofSymbols);
        break;
      case NR_PUCCH_Resource__format_PR_format2:
        len = r->format.choice.format2->nrofPRBs;
        mask = SL_to_bitmap(r->format.choice.format2->startingSymbolIndex, r->format.choice.format2->nrofSymbols);
        break;
      case NR_PUCCH_Resource__format_PR_format3:
        len = r->format.choice.format3->nrofPRBs;
        mask = SL_to_bitmap(r->format.choice.format3->startingSymbolIndex, r->format.choice.format3->nrofSymbols);
        break;
      case NR_PUCCH_Resource__format_PR_format4:
        mask = SL_to_bitmap(r->format.choice.format4->startingSymbolIndex, r->format.choice.format4->nrofSymbols);
        break;
      default:
        return;
    }
    if (mask == 0)
      return;
    const int hop = (r->secondHopPRB != NULL) ? (int)*r->secondHopPRB : -1;
    for (int p = 0; p < len; p++) {
      const int rb0 = bwp_start + r->startingPRB + p;
      if (rb0 >= 0 && rb0 < MAX_BWP_SIZE)
        occupancy[rb0] |= mask;
      if (hop >= 0) {
        const int rb1 = bwp_start + hop + p;
        if (rb1 >= 0 && rb1 < MAX_BWP_SIZE)
          occupancy[rb1] |= mask;
      }
    }
    return; /* resource found and reserved */
  }
}

/* Reserve the PRACH msg1 band, but only on actual PRACH-occasion slots and only
 * its symbols. Re-derived from cell config (mirrors schedule_nr_prach()) since
 * the reserved-slot reset wiped it from vrb_map. */
static void nr_sensing_add_prach_occupancy(const nr_cell_sched_t *cell,
                                           frame_t frame,
                                           slot_t slot,
                                           uint16_t occupancy[MAX_BWP_SIZE])
{
  const NR_COMMON_channels_t *cc = &cell->common_channels;
  const NR_ServingCellConfigCommon_t *scc = cc->ServingCellConfigCommon;
  if (scc == NULL || !is_ul_slot(slot, &cell->frame_structure))
    return;
  NR_BWP_UplinkCommon_t *iul = scc->uplinkConfigCommon->initialUplinkBWP;
  if (iul->rach_ConfigCommon == NULL || iul->rach_ConfigCommon->choice.setup == NULL)
    return;
  const NR_RACH_ConfigCommon_t *rcc = iul->rach_ConfigCommon->choice.setup;
  const NR_RACH_ConfigGeneric_t *rcg = &rcc->rach_ConfigGeneric;
  const nfapi_nr_config_request_scf_t *cfg = &cell->config;
  const uint8_t config_index = rcg->prach_ConfigurationIndex;
  const uint8_t fdm = cfg->prach_config.num_prach_fd_occasions.value;
  const int ul_mu = scc->uplinkConfigCommon->frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing;
  NR_MsgA_ConfigCommon_r16_t *msgacc =
      (iul->ext1 && iul->ext1->msgA_ConfigCommon_r16) ? iul->ext1->msgA_ConfigCommon_r16->choice.setup : NULL;
  const int mu = nr_get_prach_or_ul_mu(msgacc, rcc, ul_mu);
  const frequency_range_t fr = get_freq_range_from_arfcn(scc->downlinkConfigCommon->frequencyInfoDL->absoluteFrequencyPointA);
  uint16_t RA_sfn_index = 0xffff;
  if (!get_nr_prach_sched_from_info(cc->prach_info, config_index, frame, slot, mu, fr, &RA_sfn_index, cc->frame_type))
    return; /* not a PRACH occasion on this slot — leave PRBs free */

  const int bwp_start = NRRIV2PRBOFFSET(iul->genericParameters.locationAndBandwidth, MAX_BWP_SIZE);
  const int n_ra_rb = get_N_RA_RB(cfg->prach_config.prach_sub_c_spacing.value, ul_mu);
  const int rb0 = bwp_start + rcg->msg1_FrequencyStart;
  const int w = n_ra_rb * (fdm > 0 ? fdm : 1);
  const uint16_t format0 = cc->prach_info.format & 0xff;
  const uint32_t N_dur = (format0 < 4) ? 14u : (uint32_t)cc->prach_info.N_dur;
  const int s0 = cc->prach_info.start_symbol;
  const int ns = (int)(cc->prach_info.N_t_slot * N_dur);
  mark_occupancy_rect(occupancy, rb0, w, s0, ns);
}

/* Mark every UL reception the gNB will actually receive this slot
 * (PUSCH/PUCCH/SRS, from UL_tti_req_ahead) — the dynamic ones config-gating
 * can't predict. PRACH is handled separately; absolute carrier PRB. */
static void nr_sensing_add_committed_ul_occupancy(const nr_cell_sched_t *cell,
                                                  frame_t frame,
                                                  slot_t slot,
                                                  uint16_t occupancy[MAX_BWP_SIZE])
{
  const int slots_frame = cell->frame_structure.numb_slots_frame;
  const int idx = ul_buffer_index(frame, slot, slots_frame, cell->UL_tti_req_ahead_size);
  const nfapi_nr_ul_tti_request_t *req = &cell->UL_tti_req_ahead[idx];
  for (int i = 0; i < req->n_pdus; i++) {
    const nfapi_nr_ul_tti_request_number_of_pdus_t *pdu = &req->pdus_list[i];
    int rb0 = 0, w = 0, hop = -1, s0 = 0, ns = 0;
    switch (pdu->pdu_type) {
      case NFAPI_NR_UL_CONFIG_PUSCH_PDU_TYPE: {
        const nfapi_nr_pusch_pdu_t *p = &pdu->pusch_pdu;
        rb0 = p->bwp_start + p->rb_start;
        w = p->rb_size;
        s0 = p->start_symbol_index;
        ns = p->nr_of_symbols;
        break;
      }
      case NFAPI_NR_UL_CONFIG_PUCCH_PDU_TYPE: {
        const nfapi_nr_pucch_pdu_t *p = &pdu->pucch_pdu;
        rb0 = p->bwp_start + p->prb_start;
        w = (p->prb_size > 0) ? p->prb_size : 1;
        s0 = p->start_symbol_index;
        ns = p->nr_of_symbols;
        if (p->freq_hop_flag)
          hop = p->bwp_start + p->second_hop_prb;
        break;
      }
      case NFAPI_NR_UL_CONFIG_SRS_PDU_TYPE: {
        const nfapi_nr_srs_pdu_t *p = &pdu->srs_pdu;
        rb0 = p->bwp_start;
        w = p->bwp_size;
        s0 = NR_SYMBOLS_PER_SLOT - 1 - p->time_start_position;
        ns = 1 << p->num_symbols;
        break;
      }
      default: /* PRACH: see nr_sensing_add_prach_occupancy() */
        continue;
    }
    if (w <= 0 || ns <= 0)
      continue;
    mark_occupancy_rect(occupancy, rb0, w, s0, ns);
    if (hop >= 0)
      mark_occupancy_rect(occupancy, hop, w, s0, ns);
  }
}

/* Rebuild the periodic UL control occupancy (SR / periodic CSI / SRS PUCCH) the
 * UE transmits autonomously per RRC config, which the reserved-slot VRB reset
 * erased. Occasion-gated (only on each resource's configured slots) and written
 * into occupancy[] for the caller to OR in — never touches vrb_map or scheduling.
 * occupancy[] is indexed by absolute carrier PRB; returns the PRB count reserved. */
static int nr_sensing_add_ul_ctrl_occupancy(gNB_MAC_INST *mac,
                                            const nr_cell_sched_t *cell,
                                            frame_t frame,
                                            slot_t slot,
                                            uint16_t occupancy[MAX_BWP_SIZE])
{
  const int slots_frame = cell->frame_structure.numb_slots_frame;
  const int sfn_sf = frame * slots_frame + slot;

  /* Cover every RRC-connected UE, not just active ones: an inactive UE still
   * transmits its configured SR/CSI/SRS per RRC (e.g. SR to recover from a UL
   * failure), so an is_active gate would leak those PRBs into the free-tile
   * telemetry. Telemetry-only, so no scheduling impact. */
  UE_iterator (mac->UE_info.connected_ue_list, UE) {
    const NR_UE_UL_BWP_t *ul_bwp = &UE->current_UL_BWP;
    const int bwp_start = ul_bwp->BWPStart;
    const int bwp_size = ul_bwp->BWPSize;
    const NR_PUCCH_Config_t *pc = ul_bwp->pucch_Config;

    /* (1) SR: reserve the SR PUCCH resource only on its periodic occasion,
     * mirroring nr_sr_reporting()'s (sfn_sf - offset) % period test. */
    if (pc != NULL && pc->schedulingRequestResourceToAddModList != NULL) {
      for (int i = 0; i < pc->schedulingRequestResourceToAddModList->list.count; i++) {
        const NR_SchedulingRequestResourceConfig_t *sr = pc->schedulingRequestResourceToAddModList->list.array[i];
        if (sr->resource == NULL)
          continue;
        int period = 0, offset = 0;
        find_period_offset_SR(sr, &period, &offset);
        if (period <= 0 || ((sfn_sf - offset) % period) != 0)
          continue;
        nr_sensing_add_pucch_occupancy(pc, *sr->resource, bwp_start, occupancy);
      }
    }

    /* (2) Periodic CSI report: reserve the CSI PUCCH resource only on its
     * occasion, mirroring nr_csi_meas_reporting()'s csi_period_offset test. */
    const NR_CSI_MeasConfig_t *csi = UE->sc_info.csi_MeasConfig;
    if (pc != NULL && csi != NULL && csi->csi_ReportConfigToAddModList != NULL) {
      for (int i = 0; i < csi->csi_ReportConfigToAddModList->list.count; i++) {
        const NR_CSI_ReportConfig_t *rep = csi->csi_ReportConfigToAddModList->list.array[i];
        if (rep->reportConfigType.present != NR_CSI_ReportConfig__reportConfigType_PR_periodic
            || rep->reportConfigType.choice.periodic == NULL)
          continue;
        int period = 0, offset = 0;
        csi_period_offset(rep, NULL, &period, &offset);
        if (period <= 0 || ((sfn_sf - offset) % period) != 0)
          continue;
        if (rep->reportConfigType.choice.periodic->pucch_CSI_ResourceList.list.count <= 0)
          continue;
        const NR_PUCCH_CSI_Resource_t *cres = rep->reportConfigType.choice.periodic->pucch_CSI_ResourceList.list.array[0];
        nr_sensing_add_pucch_occupancy(pc, cres->pucch_Resource, bwp_start, occupancy);
      }
    }

    /* (3) Periodic SRS: gate on this slot's occasion, then reserve the SRS
     * symbol mask across the full BWP — mirrors nr_configure_srs()'s own
     * vrb_map_UL occupancy. No-op when SRS is not configured (do_SRS=0). */
    const NR_SRS_Config_t *sc = ul_bwp->srs_Config;
    if (sc != NULL && sc->srs_ResourceSetToAddModList != NULL && sc->srs_ResourceToAddModList != NULL) {
      for (int rs = 0; rs < sc->srs_ResourceSetToAddModList->list.count; rs++) {
        const NR_SRS_ResourceSet_t *set = sc->srs_ResourceSetToAddModList->list.array[rs];
        if (set->resourceType.present != NR_SRS_ResourceSet__resourceType_PR_periodic || set->srs_ResourceIdList == NULL)
          continue;
        for (int r1 = 0; r1 < set->srs_ResourceIdList->list.count; r1++) {
          for (int r2 = 0; r2 < sc->srs_ResourceToAddModList->list.count; r2++) {
            const NR_SRS_Resource_t *res = sc->srs_ResourceToAddModList->list.array[r2];
            if (*set->srs_ResourceIdList->list.array[r1] != res->srs_ResourceId
                || res->resourceType.present != NR_SRS_Resource__resourceType_PR_periodic)
              continue;
            const uint16_t period = srs_period[res->resourceType.choice.periodic->periodicityAndOffset_p.present];
            const uint16_t offset = get_nr_srs_offset(res->resourceType.choice.periodic->periodicityAndOffset_p);
            if (period == 0 || ((sfn_sf - offset) % period) != 0)
              continue;
            const int num = 1 << res->resourceMapping.nrofSymbols; /* 1,2,4 */
            const int l0 = NR_SYMBOLS_PER_SLOT - 1 - res->resourceMapping.startPosition;
            mark_occupancy_rect(occupancy, bwp_start, bwp_size, l0, num);
          }
        }
      }
    }
  }

  /* (4) Cell-common PRACH (occasion-gated): the band-edge RA region UEs
   * transmit preambles on, also wiped from vrb_map on reserved slots. */
  nr_sensing_add_prach_occupancy(cell, frame, slot, occupancy);

  /* (5) Committed UL receptions for this slot (PUSCH/PUCCH/SRS) from the gNB's
   * own scheduled-reception list — catches dynamic HARQ-ACK PUCCH and PUSCH at
   * their exact locations, which periodic config-gating (1)-(3) cannot predict. */
  nr_sensing_add_committed_ul_occupancy(cell, frame, slot, occupancy);

  int n_prb = 0;
  for (int rb = 0; rb < MAX_BWP_SIZE; rb++)
    if (occupancy[rb])
      n_prb++;
  return n_prb;
}

/* Part 2: Sensing-tile scan. Finds the free (symbol, PRB-span) rectangles in a
 * LOCAL copy of vrb_map_UL (ORing in occasion-gated UL-control occupancy) and
 * emits one absolute-PRB sensing_range_t per tile. Telemetry-only: never writes
 * the live vrb_map. Returns 0 when sensing is off or on non-UL slots. The tiles
 * are what the Spectrum SM (RF=1) publishes. */
static int nr_scan_sensing_tiles(gNB_MAC_INST *mac,
                                 nr_cell_sched_t *cell,
                                 frame_t frame,
                                 slot_t slot,
                                 sensing_range_t *ranges,
                                 int max_ranges)
{
  if (!cell->sensing_enabled)
    return 0;

  /* Real UL/MIXED gate first: get_ul_bitmap() returns 0x3FFF for any non-MIXED
   * slot, so without this check a pure DL slot would look fully UL and we'd
   * publish bogus ranges for it. */
  if (!nr_slot_is_ul_or_mixed(&cell->frame_structure, slot))
    return 0;

  /* Safe now: 0x3FFF for UL slots, just the UL-symbol bits for MIXED. */
  const uint16_t ul_bitmap = get_ul_bitmap(&cell->frame_structure, slot);
  if (ul_bitmap == 0)
    return 0;

  NR_ServingCellConfigCommon_t *scc = cell->common_channels.ServingCellConfigCommon;
  /* Scan the FULL UL carrier (absolute PRBs), not the initial UL BWP: bounding
   * to the BWP hid PRBs >= 75 from the tiles, blanking the band the detector
   * must police. Safe because every UL allocator still notches itself out of
   * vrb_map wherever it lands. */
  const struct NR_FrequencyInfoUL *ful = scc->uplinkConfigCommon->frequencyInfoUL;
  int bwp_size = ful->scs_SpecificCarrierList.list.array[0]->carrierBandwidth;
  int bwp_start = ful->scs_SpecificCarrierList.list.array[0]->offsetToCarrier;

  int slots_frame = cell->frame_structure.numb_slots_frame;
  const int buf_idx = ul_buffer_index(frame, slot, slots_frame, cell->vrb_map_UL_size);
  /* beam 0 for now */
  uint16_t *vrb_map_UL = &cell->common_channels.vrb_map_UL[0][buf_idx * MAX_BWP_SIZE];

  /* Subtract the dApp UL block (effective) from the occupancy so blocked PRBs
   * read as FREE here and still surface as tiles: the dApp blocked them for
   * interference but still watches them via the sensing telemetry. The bits stay
   * set in vrb_map_UL (schedulers keep refusing them); only this scan subtracts
   * them. The LIVE snapshot is read; a mid-interim shrink self-corrects on the
   * next reseed. */

  /* Add the RRC-configured periodic UL control occupancy (SR + periodic CSI +
   * SRS) the UE transmits autonomously but that the reserved-slot VRB reset
   * erased — occasion-gated, OR'd into the LOCAL sense_vrb copy only (scheduling
   * untouched), so those cells aren't emitted as free tiles on their occasions.
   * See nr_sensing_add_ul_ctrl_occupancy(). */
  uint16_t ctrl_occ[MAX_BWP_SIZE];
  memset(ctrl_occ, 0, sizeof(ctrl_occ));
  const int n_ctrl_prb = nr_sensing_add_ul_ctrl_occupancy(mac, cell, frame, slot, ctrl_occ);

  /* Build the block-subtracted, control-occupancy-augmented snapshot: subtract
   * the dApp block (when present), then OR the per-slot control occupancy. */
  uint16_t sense_vrb[MAX_BWP_SIZE];
  for (int rb = 0; rb < MAX_BWP_SIZE; rb++)
    sense_vrb[rb] = vrb_map_UL[rb] | ctrl_occ[rb];
  if (n_ctrl_prb > 0)
    LOG_D(NR_MAC, "%d.%d sensing mask: %d PRB(s) reserved for UL control\n", frame, slot, n_ctrl_prb);

  /* PRB-granular scan: for each free UL symbol, emit one sensing_range_t per
   * contiguous PRB span clear at that symbol (num_symbols = 1). Anything already
   * in vrb_map_UL (PRACH/SRS/PUCCH/SR/CSI/PUSCH/Msg3) excludes itself; the dApp
   * may coalesce adjacent tiles. */
  int n_ranges = 0;
  for (int sym = 0; sym < 14 && n_ranges < max_ranges; sym++) {
    if (!(ul_bitmap & (1u << sym)))
      continue;
    const uint16_t sym_bit = (uint16_t)(1u << sym);

    int prb = bwp_start;
    const int prb_end = bwp_start + bwp_size;
    while (prb < prb_end && n_ranges < max_ranges) {
      /* skip PRBs occupied at this symbol, then measure the next free run */
      while (prb < prb_end && (sense_vrb[prb] & sym_bit))
        prb++;
      const int run_start = prb;
      while (prb < prb_end && !(sense_vrb[prb] & sym_bit))
        prb++;
      const int run_len = prb - run_start;
      if (run_len <= 0)
        break;

      /* rb_start is ABSOLUTE (carrier/grid PRB index, the same coordinate as
       * the vrb_map_UL[prb] index walked here and the L1 rxdataF tap) so
       * dApps line it up directly. The Aerial dummy-PUSCH path converts back
       * to BWP-relative on its way into FAPI; see nr_fill_sensing_pusch(). */
      ranges[n_ranges].start_symbol = sym;
      ranges[n_ranges].num_symbols = 1;
      ranges[n_ranges].rb_start = run_start;
      ranges[n_ranges].rb_size = run_len;
      n_ranges++;

      LOG_D(NR_MAC, "Sensing tile %d.%d: sym %d RB %d+%d (abs)\n", frame, slot, sym, run_start, run_len);
    }
  }

  return n_ranges;
}

/* Per-slot sensing driver: scan the UL for free tiles, then (Aerial) inject a
 * capture PUSCH and (E3) publish the ranges. Inert when sensing is disabled.
 * Defined after nr_scan_sensing_tiles so it can stay static; the Aerial
 * capture-PUSCH (nr_fill_sensing_pusch) lives in gNB_scheduler_ul_sensing_aerial.c. */
void nr_mac_sensing_scan_and_publish(gNB_MAC_INST *mac, nr_cell_sched_t *cell, frame_t frame, slot_t slot)
{
  sensing_range_t sensing_ranges[MAX_SENSING_RANGES];
  int n_sensing = nr_scan_sensing_tiles(mac, cell, frame, slot, sensing_ranges, MAX_SENSING_RANGES);
  if (n_sensing > 0)
    LOG_D(NR_MAC, "%d.%d: identified %d sensing tile(s)\n", frame, slot, n_sensing);
#ifdef ENABLE_AERIAL
  nr_fill_sensing_pusch(cell, frame, slot, sensing_ranges, n_sensing);
#endif
#ifdef E3_AGENT
  /* Publish every UL/MIXED slot (even n_sensing==0, to clear last frame's stale
   * ranges); DL slots have no consumer. */
  if (cell->sensing_enabled && nr_slot_is_ul_or_mixed(&cell->frame_structure, slot))
    nr_mac_record_sensing_ranges(&cell->common_channels, /*beam=*/0, frame, slot, sensing_ranges, n_sensing);
#endif
}
