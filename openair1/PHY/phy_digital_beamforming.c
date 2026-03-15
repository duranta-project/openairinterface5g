/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "phy_digital_beamforming.h"
#include "defs_gNB.h"

// #define DBF_DEBUG

/* Check for the first port's beam ID for MSB value. We trust MAC to send same MSB in all PDUs. */
static bool check_low_phy_bf(const struct nr_grid *nrg)
{
  const uint16_t beam_id = nrg[0].grid_info[0].beam_id;
  return (IS_BIT_SET(beam_id, 15));
}

/* Add PUSCH sched info for BF. */
static int fill_pusch_grid_info(const uint32_t frame, const uint32_t slot, const nfapi_nr_pusch_pdu_t *pdu, struct nr_grid_slot *nrg)
{
  int ret = 0;
  uint16_t stream_idx = 0;
  /* According to FAPI, when dig_bf_interface = 0 the BF is straight wire which means baseband and logical ports
  are mapped one to one. So, there could be baseband ports not mapped to any logical ports. */
  bool no_bf = pdu->beamforming.dig_bf_interface == 0;
  uint16_t beam_id = no_bf ? 0 : pdu->beamforming.prgs_list[0].dig_bf_interface_list[stream_idx].beam_idx;
  do {
    struct grid_info info = {.beam_id = beam_id,
                             .start_prb = pdu->rb_start + pdu->bwp_start,
                             .num_prb = pdu->rb_size,
                             .start_symbol = pdu->start_symbol_index,
                             .num_symbols = pdu->nr_of_symbols};

    uint16_t port_id = pdu->param_v4.spatialStreamIndices[stream_idx];
    struct nr_grid *nrg_p = nrg->grid + port_id;
    nrg_p->grid_info[nrg_p->num_sections++] = info;

    LOG_D(PHY, "%d.%d Adding PUSCH grid info for port %d\n", frame, slot, port_id);
    stream_idx++;
    beam_id = no_bf ? 0 : pdu->beamforming.prgs_list[0].dig_bf_interface_list[stream_idx].beam_idx;
  } while (stream_idx < pdu->param_v4.numSpatialStreamIndices);
  return ret;
}

/* Add PUSCH sched info for BF. */
static int fill_pucch_grid_info(const uint32_t frame, const uint32_t slot, const nfapi_nr_pucch_pdu_t *pdu, struct nr_grid_slot *nrg)
{
  int ret = 0;
  uint32_t stream_idx = 0;
  bool no_bf = pdu->beamforming.dig_bf_interface == 0;
  uint16_t beam_id = no_bf ? 0 : pdu->beamforming.prgs_list[0].dig_bf_interface_list[stream_idx].beam_idx;
  do {
    struct grid_info info = {.beam_id = beam_id,
                             .start_prb = pdu->prb_start + pdu->bwp_start,
                             .num_prb = pdu->prb_size,
                             .start_symbol = pdu->start_symbol_index,
                             .num_symbols = pdu->nr_of_symbols};

    uint16_t port_id = pdu->param_v4.spatialStreamIndices[stream_idx];
    struct nr_grid *nrg_p = nrg->grid + port_id;
    nrg_p->grid_info[nrg_p->num_sections++] = info;

    LOG_D(PHY, "%d.%d Adding PUCCH grid info for port %d\n", frame, slot, port_id);
    stream_idx++;
    beam_id = no_bf ? 0 : pdu->beamforming.prgs_list[0].dig_bf_interface_list[stream_idx].beam_idx;
  } while (stream_idx < pdu->param_v4.numSpatialStreamIndices);
  return ret;
}

/* Add PRACH sched info for BF. */
static int fill_prach_grid_info(const uint32_t frame, const uint32_t slot, const nfapi_nr_prach_pdu_t *pdu, const RU_t *ru, struct nr_grid_slot *nrg)
{
  int ret = 0;
  uint32_t stream_idx = 0;
  bool no_bf = pdu->beamforming.dig_bf_interface == 0;
  uint16_t beam_id = no_bf ? 0 : pdu->beamforming.prgs_list[0].dig_bf_interface_list[stream_idx].beam_idx;
  do {
    const int fdm_idx = pdu->num_ra;
    const int start_re = ru->config.prach_config.num_prach_fd_occasions_list[fdm_idx].k1.value;
    const int start_rb = start_re / NR_NB_SC_PER_RB;
    // TODO: configure for long format;
    if (ru->config.prach_config.prach_sequence_length.value != 1) {
      LOG_E(PHY, "Beamforming not implemented for long format\n");
      return -1;
    }
    const int num_rb = NR_PRACH_SEQ_LEN_S / NR_NB_SC_PER_RB;
    struct grid_info info = {.beam_id = beam_id,
                             .start_prb = start_rb,
                             .num_prb = num_rb,
                             .start_symbol = pdu->prach_start_symbol,
                             .num_symbols = NR_SYMBOLS_PER_SLOT - pdu->prach_start_symbol};
    // For now lets beamform till the end of PRACH slot.

    uint16_t port_id = pdu->param_v4.spatialStreamIndices[stream_idx];
    struct nr_grid *nrg_p = nrg->grid + port_id;
    nrg_p->grid_info[nrg_p->num_sections++] = info;

    LOG_D(PHY, "%d.%d Adding PRACH grid info for port %d\n", frame, slot, port_id);
    stream_idx++;
    beam_id = no_bf ? 0 : pdu->beamforming.prgs_list[0].dig_bf_interface_list[stream_idx].beam_idx;
  } while (stream_idx < pdu->param_v4.numSpatialStreamIndices);
  return ret;
}

/* Add SRS sched info for BF. */
static int fill_srs_grid_info(const uint32_t frame, const uint32_t slot, const nfapi_nr_srs_pdu_t *pdu, struct nr_grid_slot *nrg)
{
  /*
    Beamforming of SRS signals is very unlikely to happen. If the gNB decides to BF SRS, then the gird info
    should be updated with reMask to exactly specify REs and its beams ID.
  */
  int ret = 0;
  uint32_t stream_idx = 0;
  bool no_bf = pdu->beamforming.dig_bf_interface == 0;
  uint16_t beam_id = no_bf ? 0 : pdu->beamforming.prgs_list[0].dig_bf_interface_list[stream_idx].beam_idx;
  do {
    struct grid_info info = {.beam_id = beam_id,
                             .start_prb = pdu->bwp_start,
                             .num_prb = pdu->bwp_size,
                             .start_symbol = pdu->time_start_position,
                             .num_symbols = 1 << pdu->num_symbols};

    uint16_t port_id = pdu->srs_parameters_v4.Ul_spatial_stream_ports[stream_idx];
    struct nr_grid *nrg_p = nrg->grid + port_id;
    nrg_p->grid_info[nrg_p->num_sections++] = info;

    LOG_D(PHY, "%d.%d Adding SRS grid info for port %d\n", frame, slot, port_id);
    stream_idx++;
    beam_id = pdu->beamforming.prgs_list[0].dig_bf_interface_list[stream_idx].beam_idx;
  } while (stream_idx < pdu->srs_parameters_v4.num_ul_spatial_streams_ports);
  return ret;
}

/* Populate grid info for current slot. */
void fill_rx_grid_info(const RU_t *ru,
                       const uint32_t frame,
                       const uint32_t slot,
                       const nfapi_nr_ul_tti_request_number_of_pdus_t *ul_pdu,
                       struct nr_grid_slot *nrg)
{
  int ret = 0;
  switch (ul_pdu->pdu_type) {
    case NFAPI_NR_UL_CONFIG_PUSCH_PDU_TYPE:
      ret = fill_pusch_grid_info(frame, slot, &ul_pdu->pusch_pdu, nrg);
      break;

    case NFAPI_NR_UL_CONFIG_PUCCH_PDU_TYPE:
      ret = fill_pucch_grid_info(frame, slot, &ul_pdu->pucch_pdu, nrg);
      break;

    case NFAPI_NR_UL_CONFIG_PRACH_PDU_TYPE:
      ret = fill_prach_grid_info(frame, slot, &ul_pdu->prach_pdu, ru, nrg);
      break;

    case NFAPI_NR_UL_CONFIG_SRS_PDU_TYPE:
      ret = fill_srs_grid_info(frame, slot, &ul_pdu->srs_pdu, nrg);
      break;
  }
  if (ret < 0) {
    LOG_E(PHY, "Error configuring UL slots for beamforming\n");
    return;
  }
}

/*
   Based on FAPI's beam id MSB, decide if beam ID is passed to remote RU.
   If BF is done on remote RU (7.2 split), then fill the xran buffer with BF info immediately.
*/
void send_rx_grid_info(RU_t *ru, struct nr_grid_slot *nrg)
{
  const bool low_phy_bf = check_low_phy_bf(nrg->grid);
  if (low_phy_bf) {
    // Call xran function to fill cplane buffer with beam and PRB info for this UL slot.
    if (ru->fh_south_out_ctrl)
      ru->fh_south_out_ctrl(ru, nrg->frame, nrg->slot, 0, nrg->grid);
  }
  // HiPhy BF is done in PHY procedures.
}

void tx_beamforming_if(RU_t *ru)
{
  struct nr_grid *nrg = ru->common.ru_tx_grid;

  const NR_DL_FRAME_PARMS *fp = &ru->gNB_list[0]->frame_parms;

  const int num_logical_ports = fp->nb_antennas_tx;
  // No allocations. Set memory to 0 and exit.
  if (!nrg->num_sections) {
    for (int l = 0; l < num_logical_ports; l++)
      memset(ru->common.txdataF_BF[l], 0, sizeof(c16_t) * fp->samples_per_slot_wCP);
  } else {
    /* If HiPhy BF, the data is already beamformed in PHY procedures. */
    for (int l = 0; l < num_logical_ports; l++)
      memcpy(ru->common.txdataF_BF[l],
             ru->gNB_list[0]->common_vars.txdataF[l],
             sizeof(c16_t) * fp->samples_per_slot_wCP);
  }
}
