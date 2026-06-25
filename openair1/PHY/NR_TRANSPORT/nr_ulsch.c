/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Top-level routines for the reception of the PUSCH TS 38.211 v 15.4.0
 */

#include <stdint.h>
#include "PHY/NR_TRANSPORT/nr_transport_common_proto.h"
#include "PHY/NR_TRANSPORT/nr_ulsch.h"
#include "SCHED_NR/sched_nr.h"

static void dump_pusch_pdu(int instance, int frame, int slot, nfapi_nr_pusch_pdu_t *pusch_pdu)
{
  LOG_D(PHY,
        "[gNB %d] %d.%d ULSCH PDU pdu_bit_map %u "
        "rnti %u "
        "handle %u "
        "bwp_size %u "
        "bwp_start %u "
        "subcarrier_spacing %u "
        "cyclic_prefix %u "
        "target_code_rate %u "
        "qam_mod_order %u "
        "mcs_index %u "
        "mcs_table %u "
        "transform_precoding %u "
        "data_scrambling_id %u "
        "nrOfLayers %u "
        "ul_dmrs_symb_pos %u "
        "dmrs_config_type %u "
        "ul_dmrs_scrambling_id %u "
        "scid %u "
        "num_dmrs_cdm_grps_no_data %u "
        "dmrs_ports %u "
        "resource_alloc %u "
        "rb_start %u "
        "rb_size %u "
        "vrb_to_prb_mapping %u "
        "frequency_hopping %u "
        "tx_direct_current_location %u "
        "uplink_frequency_shift_7p5khz %u "
        "start_symbol_index %u "
        "nr_of_symbols %u "
        "tbslbrm %u "
        "ldpcBaseGraph %u "
        "pusch_data->rv_index %u "
        "pusch_data->harq_process_id %u "
        "pusch_data->new_data_indicator %u "
        "pusch_data->num_cb %u\n",
        instance,
        frame,
        slot,
        pusch_pdu->pdu_bit_map,
        pusch_pdu->rnti,
        pusch_pdu->handle,
        pusch_pdu->bwp_size,
        pusch_pdu->bwp_start,
        pusch_pdu->subcarrier_spacing,
        pusch_pdu->cyclic_prefix,
        pusch_pdu->target_code_rate,
        pusch_pdu->qam_mod_order,
        pusch_pdu->mcs_index,
        pusch_pdu->mcs_table,
        pusch_pdu->transform_precoding,
        pusch_pdu->data_scrambling_id,
        pusch_pdu->nrOfLayers,
        pusch_pdu->ul_dmrs_symb_pos,
        pusch_pdu->dmrs_config_type,
        pusch_pdu->ul_dmrs_scrambling_id,
        pusch_pdu->scid,
        pusch_pdu->num_dmrs_cdm_grps_no_data,
        pusch_pdu->dmrs_ports,
        pusch_pdu->resource_alloc,
        pusch_pdu->rb_start,
        pusch_pdu->rb_size,
        pusch_pdu->vrb_to_prb_mapping,
        pusch_pdu->frequency_hopping,
        pusch_pdu->tx_direct_current_location,
        pusch_pdu->uplink_frequency_shift_7p5khz,
        pusch_pdu->start_symbol_index,
        pusch_pdu->nr_of_symbols,
        pusch_pdu->maintenance_parms_v3.tbSizeLbrmBytes,
        pusch_pdu->maintenance_parms_v3.ldpcBaseGraph,
        pusch_pdu->pusch_data.rv_index,
        pusch_pdu->pusch_data.harq_process_id,
        pusch_pdu->pusch_data.new_data_indicator,
        pusch_pdu->pusch_data.num_cb);
}


void nr_fill_ulsch(PHY_VARS_gNB *gNB, int frame, int slot, nfapi_nr_pusch_pdu_t *ulsch_pdu)
{
  dump_pusch_pdu(gNB->Mod_id, frame, slot, ulsch_pdu);
  LOG_D(NR_PHY,
        "%4d.%2d Programming ULSCH RNTI %04x HARQ PID %d new data indicator %d\n",
        frame,
        slot,
        ulsch_pdu->rnti,
        ulsch_pdu->pusch_data.harq_process_id,
        ulsch_pdu->pusch_data.new_data_indicator);

  NR_gNB_PUSCH_job_t pusch = {.frame = frame, .slot = slot, .pusch_pdu = *ulsch_pdu};
  if (gNB->common_vars.beam_id) {
    int fapi_beam_idx = ulsch_pdu->beamforming.prgs_list[0].dig_bf_interface_list[0].beam_idx;
    int bitmap = SL_to_bitmap(ulsch_pdu->start_symbol_index, ulsch_pdu->nr_of_symbols);
    const nfapi_nr_spatial_stream_index_t *p = &ulsch_pdu->param_v4;
    // We assume the ports are ordered continuously. Hence only the start port idx is enough.
    uint16_t ant_port_start = p->numSpatialStreamIndices > 0 ? p->spatialStreamIndices[0] : 0;
    beam_index_allocation(fapi_beam_idx,
                          ant_port_start,
                          p->numSpatialStreamIndices,
                          NR_SYMBOLS_PER_SLOT,
                          slot,
                          bitmap,
                          gNB->frame_parms.nb_antennas_rx,
                          gNB->common_vars.beam_id);
  }
  bool done = spsc_q_put(&gNB->pusch_queue, &pusch, sizeof(pusch));
  if (!done)
    LOG_W(NR_PHY, "PUSCH queue is full: dropping PUSCH UE %04x\n", ulsch_pdu->rnti);
}

void reset_active_ulsch(PHY_VARS_gNB *gNB, int frame)
{
  // deactivate ULSCH structure after a given number of frames
  // no activity on this structure for NUMBER_FRAMES_PHY_UE_INACTIVE
  // assuming UE disconnected or some other error occurred
  for (int i = 0; i < gNB->max_nb_pusch; i++) {
    NR_gNB_ULSCH_t *ulsch = &gNB->ulsch[i];
    int diff = (frame - ulsch->frame + 1024) & 1023;
    if (ulsch->active && diff > NUMBER_FRAMES_PHY_UE_INACTIVE && diff < 100) {
      ulsch->active = false;
      LOG_D(NR_PHY,
            "Frame %d: resetting ulsch %d harq %d (programmed in %d.%d)\n",
            frame,
            i,
            ulsch->harq_pid,
            ulsch->frame,
            ulsch->slot);
    }
  }
}

void dump_pusch_stats(FILE *fd, PHY_VARS_gNB *gNB)
{
  for (int i = 0; i < MAX_MOBILES_PER_GNB; i++) {
    NR_gNB_PHY_STATS_t *stats = &gNB->phy_stats[i];
    if (stats->active && stats->frame != stats->ulsch_stats.dump_frame) {
      stats->ulsch_stats.dump_frame = stats->frame;
      for (int aa = 0; aa < gNB->frame_parms.nb_antennas_rx; aa++)
        if (aa == 0)
          fprintf(fd,
                  "ULSCH RNTI %4x, %d: ulsch_power[%d] %d,%d ulsch_noise_power[%d] %d.%d, sync_pos %d\n",
                  stats->rnti,
                  stats->frame,
                  aa,
                  stats->ulsch_stats.power[aa] / 10,
                  stats->ulsch_stats.power[aa] % 10,
                  aa,
                  stats->ulsch_stats.noise_power[aa] / 10,
                  stats->ulsch_stats.noise_power[aa] % 10,
                  stats->ulsch_stats.sync_pos);
        else
          fprintf(fd,
                  "                  ulsch_power[%d] %d.%d, ulsch_noise_power[%d] %d.%d\n",
                  aa,
                  stats->ulsch_stats.power[aa] / 10,
                  stats->ulsch_stats.power[aa] % 10,
                  aa,
                  stats->ulsch_stats.noise_power[aa] / 10,
                  stats->ulsch_stats.noise_power[aa] % 10);

      int *rt = stats->ulsch_stats.round_trials;
      fprintf(fd,
              "                 round_trials %d(%1.1e):%d(%1.1e):%d(%1.1e):%d, DTX %d, current_Qm %d, current_RI %d, total_bytes "
              "RX/SCHED %d/%d\n",
              rt[0],
              (double)rt[1] / rt[0],
              rt[1],
              (double)rt[2] / rt[0],
              rt[2],
              (double)rt[3] / rt[0],
              rt[3],
              stats->ulsch_stats.DTX,
              stats->ulsch_stats.current_Qm,
              stats->ulsch_stats.current_RI,
              stats->ulsch_stats.total_bytes_rx,
              stats->ulsch_stats.total_bytes_tx);
    }
  }
}
