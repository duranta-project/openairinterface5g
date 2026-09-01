/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Aerial-specific sensing logic: a dummy "capture" PUSCH that points
 * cuBB's L1 at a free UL window so it materializes the slot's IQ. Compiled only
 * for ENABLE_AERIAL (Aerial/cuBB) — monolithic OAI L1 captures the whole UL slot
 * regardless and would assert on a CRC-failing dummy PUSCH. Split out of
 * gNB_scheduler_ul_sensing.c so the Aerial path (incl. future beamforming) grows
 * here, not in the scheduler-agnostic sensing file.
 */

#ifdef ENABLE_AERIAL

#include "gNB_scheduler_ul_sensing.h"
#include "LAYER2/NR_MAC_gNB/mac_proto.h"
#include "LAYER2/NR_MAC_COMMON/nr_mac_common.h"
#include "common/utils/nr/nr_common.h"

#include <string.h>

/* Part 3: Aerial dummy PUSCH injection — points cuBB's L1 at a free window so it
 * computes rxdataF and ships the slot's IQ. One PUSCH per slot is enough (cuBB
 * capture is slot-level). Compiles out for OAI L1, which captures regardless. */

/* Monotonic HARQ-process-ID counter for sensing PUSCH. Because the RNTI is
 * constant, the hpid must not repeat while cuBB still has the previous slot's
 * PUSCH in flight; cycling through 16 ids (~8 ms at mu=1) stays clear of that. */
static uint32_t g_sensing_hpid_seq = 0;

static void build_sensing_pusch_pdu(nfapi_nr_ul_tti_request_t *req,
                                    rnti_t rnti,
                                    int bwp_size,
                                    int bwp_start,
                                    int scs,
                                    int phys_cell_id,
                                    int dmrs_TypeA_Position,
                                    int start_symbol,
                                    int num_symbols,
                                    int mcs,
                                    int rb_size,
                                    int rb_start_loc,
                                    int nrOfLayers,
                                    int num_beams,
                                    const int *beams)
{
  _Static_assert(SENSING_MAX_BEAMS <= NFAPI_MAX_NUM_BG_IF, "SENSING_MAX_BEAMS exceeds FAPI dig_bf_interface_list[] capacity");

  AssertFatal(req->n_pdus < sizeof(req->pdus_list) / sizeof(req->pdus_list[0]),
              "UL_tti_req_ahead n_pdus overflow %d\n",
              req->n_pdus);

  /* Bound the operator-configured PRB window to the UL BWP so a misconfigured
   * sensing_pusch_rb_start/rb_size can't point cuBB at an out-of-grid region. */
  AssertFatal(rb_start_loc >= 0 && rb_size > 0 && rb_start_loc + rb_size <= bwp_size,
              "sensing PUSCH PRB window [%d, +%d) is outside the UL BWP (size %d)\n",
              rb_start_loc,
              rb_size,
              bwp_size);

  req->pdus_list[req->n_pdus].pdu_type = NFAPI_NR_UL_CONFIG_PUSCH_PDU_TYPE;
  req->pdus_list[req->n_pdus].pdu_size = sizeof(nfapi_nr_pusch_pdu_t);
  nfapi_nr_pusch_pdu_t *pusch_pdu = &req->pdus_list[req->n_pdus].pusch_pdu;
  memset(pusch_pdu, 0, sizeof(*pusch_pdu));

  /* (mcs, rb_size, rb_start_loc, nrOfLayers, num_beams, beams[]) now come
   * from nr_mac_config_t which the operator sets via the sensing_pusch_*
   * keys in wnc.conf. See gnb_paramdef.h for the descriptors and the
   * GNB_SENSING_PUSCH_*_IDX defines. */
  const int dmrs_type = 0; /* type 1 */

  uint16_t dmrs_symb_pos = get_l_prime(num_symbols,
                                       typeB, /* mapping type B for short allocations */
                                       pusch_dmrs_pos2,
                                       pusch_len1,
                                       start_symbol,
                                       dmrs_TypeA_Position);

  pusch_pdu->pdu_bit_map = PUSCH_PDU_BITMAP_PUSCH_DATA;
  pusch_pdu->rnti = rnti;
  pusch_pdu->handle = 0;

  pusch_pdu->bwp_size = bwp_size;
  pusch_pdu->bwp_start = bwp_start;
  pusch_pdu->subcarrier_spacing = scs;
  pusch_pdu->cyclic_prefix = 0;

  pusch_pdu->target_code_rate = nr_get_code_rate_ul(mcs, 0);
  pusch_pdu->qam_mod_order = nr_get_Qm_ul(mcs, 0);
  pusch_pdu->mcs_index = mcs;
  pusch_pdu->mcs_table = 0;
  pusch_pdu->transform_precoding = NR_PUSCH_Config__transformPrecoder_disabled;
  pusch_pdu->data_scrambling_id = phys_cell_id;
  pusch_pdu->nrOfLayers = nrOfLayers;

  pusch_pdu->ul_dmrs_symb_pos = dmrs_symb_pos;
  pusch_pdu->dmrs_config_type = dmrs_type;
  pusch_pdu->ul_dmrs_scrambling_id = phys_cell_id;
  pusch_pdu->pusch_identity = phys_cell_id;
  pusch_pdu->scid = 0;
  /* DMRS port bitmap must cover all layers, and DMRS type 1 fits 2 ports per
   * CDM group, so >2 layers need a second CDM group with no data. Mirrors the
   * production UL fill in gNB_scheduler_ulsch.c. */
  pusch_pdu->num_dmrs_cdm_grps_no_data = (nrOfLayers <= 2) ? 1 : 2;
  pusch_pdu->dmrs_ports = (1u << nrOfLayers) - 1;

  pusch_pdu->resource_alloc = 1;
  pusch_pdu->rb_start = rb_start_loc;
  pusch_pdu->rb_size = rb_size;
  pusch_pdu->vrb_to_prb_mapping = 0;
  pusch_pdu->frequency_hopping = 0;
  pusch_pdu->start_symbol_index = start_symbol;
  pusch_pdu->nr_of_symbols = num_symbols;

  pusch_pdu->pusch_data.rv_index = 0;
  {
    /* Monotonic hpid (see g_sensing_hpid_seq above). */
    uint32_t seq = __atomic_fetch_add(&g_sensing_hpid_seq, 1u, __ATOMIC_RELAXED);
    pusch_pdu->pusch_data.harq_process_id = (uint8_t)(seq & 0xFu);
  }
  pusch_pdu->pusch_data.new_data_indicator = 1;
  pusch_pdu->pusch_data.num_cb = 0;

  /* Beamforming: one PRG over the full BWP. Clamp num_beams to the FAPI
   * dig_bf_interface_list[] capacity so this builder can't overflow. */
  if (num_beams > NFAPI_MAX_NUM_BG_IF)
    num_beams = NFAPI_MAX_NUM_BG_IF;
  if (num_beams < 0)
    num_beams = 0;
  pusch_pdu->beamforming.num_prgs = 1;
  pusch_pdu->beamforming.prg_size = pusch_pdu->bwp_size;
  pusch_pdu->beamforming.dig_bf_interface = num_beams;
  for (int i = 0; i < num_beams; i++) {
    pusch_pdu->beamforming.prgs_list[0].dig_bf_interface_list[i].beam_idx = beams[i];
  }

  /* TBS must match exactly what the real UL scheduler/cuBB compute, else cuBB
   * rejects the dummy PUSCH and captures no IQ. Mirrors N_PRB_DMRS in
   * gNB_scheduler_primitives.c. */
  int N_PRB_DMRS = pusch_pdu->num_dmrs_cdm_grps_no_data * ((dmrs_type == 0) ? 6 : 4);
  int N_DMRS_SLOT = get_num_dmrs(dmrs_symb_pos);
  uint32_t TBS = nr_compute_tbs(pusch_pdu->qam_mod_order,
                                pusch_pdu->target_code_rate,
                                rb_size,
                                num_symbols,
                                N_PRB_DMRS * N_DMRS_SLOT,
                                0,
                                0,
                                nrOfLayers)
                 >> 3;
  pusch_pdu->pusch_data.tb_size = TBS;

  req->n_pdus++;
  req->n_ulsch++;
}

/* (ENABLE_AERIAL) Inject one dummy "capture" PUSCH into UL_tti_req_ahead[slot] so
 * cuBB computes rxdataF and ships the slot's IQ. No-op if any real PUSCH/PUCCH/SRS
 * is already scheduled this slot. Symbol window comes from the first free-symbol
 * run in the tiles; PRB/MCS/layers/beams from the operator sensing_pusch_* config. */
void nr_fill_sensing_pusch(nr_cell_sched_t *cell, frame_t frame, slot_t slot, const sensing_range_t *ranges, int n_ranges)
{
  if (n_ranges <= 0)
    return;

  NR_ServingCellConfigCommon_t *scc = cell->common_channels.ServingCellConfigCommon;
  NR_BWP_UplinkCommon_t *initialUL = scc->uplinkConfigCommon->initialUplinkBWP;
  int bwp_size = NRRIV2BW(initialUL->genericParameters.locationAndBandwidth, MAX_BWP_SIZE);
  int bwp_start = NRRIV2PRBOFFSET(initialUL->genericParameters.locationAndBandwidth, MAX_BWP_SIZE);
  int scs = initialUL->genericParameters.subcarrierSpacing;
  int phys_cell_id = *scc->physCellId;

  int slots_frame = cell->frame_structure.numb_slots_frame;
  const int buf_idx = ul_buffer_index(frame, slot, slots_frame, cell->vrb_map_UL_size);
  nfapi_nr_ul_tti_request_t *req = &cell->UL_tti_req_ahead[buf_idx];

  /* Inject only when no real UL reception (PUSCH/PUCCH/SRS) is already scheduled
   * this slot — a real one makes cuBB capture the IQ anyway. PRACH doesn't count
   * (it doesn't populate the rxdataF tap), so we still inject on PRACH-only slots. */
  for (int i = 0; i < req->n_pdus; i++) {
    const uint16_t t = req->pdus_list[i].pdu_type;
    if (t == NFAPI_NR_UL_CONFIG_PUSCH_PDU_TYPE || t == NFAPI_NR_UL_CONFIG_PUCCH_PDU_TYPE || t == NFAPI_NR_UL_CONFIG_SRS_PDU_TYPE)
      return;
  }

  /* Use the first contiguous run of free symbols (from the tiles) as the dummy
   * PUSCH's symbol placement; its PRB window/MCS/layers/beams come from the
   * sensing_pusch_* config, not the tiles, so the PDU stays well-formed. */
  uint16_t free_syms = 0;
  for (int i = 0; i < n_ranges; i++) {
    for (int s = ranges[i].start_symbol; s < ranges[i].start_symbol + ranges[i].num_symbols; s++)
      if (s >= 0 && s < 14)
        free_syms |= (uint16_t)(1u << s);
  }
  int sym = 0;
  while (sym < 14 && !(free_syms & (1u << sym)))
    sym++;
  if (sym >= 14)
    return; /* no free symbol (shouldn't happen when n_ranges > 0) */
  const int start = sym;
  while (sym < 14 && (free_syms & (1u << sym)))
    sym++;
  const int length = sym - start;

  /* One PUSCH per slot is enough to trigger cuBB's slot-level IQ capture (the
   * fine-grained tiles still reach the dApp via the Spectrum SM); this only
   * forces the FH IQ to materialise. ENABLE_AERIAL-gated because monolithic OAI
   * would assert on the LDPC base-graph for a CRC-failing dummy PUSCH. */
  const nr_mac_config_t *rc = &cell->radio_config;
  rnti_t rnti = SENSING_RNTI;
  build_sensing_pusch_pdu(req,
                          rnti,
                          bwp_size,
                          bwp_start,
                          scs,
                          phys_cell_id,
                          scc->dmrs_TypeA_Position,
                          start,
                          length,
                          rc->sensing_pusch_mcs,
                          rc->sensing_pusch_rb_size,
                          rc->sensing_pusch_rb_start,
                          rc->sensing_pusch_nrOfLayers,
                          rc->sensing_pusch_num_beams,
                          rc->sensing_pusch_beams);

  LOG_D(NR_MAC,
        "Sensing PUSCH %d.%d: sym %d+%d RB %d+%d (cap=1, %d tile(s) to dApp)\n",
        frame,
        slot,
        start,
        length,
        rc->sensing_pusch_rb_start,
        rc->sensing_pusch_rb_size,
        n_ranges);
}

#endif /* ENABLE_AERIAL */
