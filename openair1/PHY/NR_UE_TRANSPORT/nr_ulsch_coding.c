/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*! \file PHY/NR_UE_TRANSPORT/nr_ulsch_coding_slot.c
 */

#include "PHY/NR_UE_TRANSPORT/nr_transport_proto_ue.h"
#include "PHY/CODING/coding_defs.h"
#include "PHY/CODING/coding_extern.h"
#include "PHY/CODING/nrLDPC_extern.h"
#include "PHY/CODING/nrLDPC_coding/nrLDPC_coding_interface.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_ue.h"
#include "T_messages_creator.h"
#include "executables/nr-uesoftmodem.h"

int nr_ulsch_pre_encoding(PHY_VARS_NR_UE *ue,
                          const NR_UE_ULSCH_t *ulsch,
                          const uint32_t frame,
                          const uint8_t slot,
                          const unsigned int *G,
                          const int nb_ulsch,
                          const uint8_t *ULSCH_ids)
{
  for (uint_fast8_t pusch_id = 0; pusch_id < nb_ulsch; pusch_id++) {
    const uint8_t ULSCH_id = ULSCH_ids[pusch_id];
    const uint8_t harq_pid = ulsch[ULSCH_id].pusch_pdu.pusch_data.harq_process_id;

    /////////////////////////parameters and variables initialization/////////////////////////

    NR_UL_UE_HARQ_t *harq_process = &ue->ul_harq_processes[harq_pid];
    const nfapi_nr_ue_pusch_pdu_t *pusch_pdu = &ulsch->pusch_pdu;
    const uint16_t nb_rb = pusch_pdu->rb_size;
    const uint32_t A = pusch_pdu->pusch_data.tb_size << 3;
    const uint8_t Qm = pusch_pdu->qam_mod_order;
    // target_code_rate is in 0.1 units
    const float Coderate = (float)pusch_pdu->target_code_rate / 10240.0f;

    LOG_D(NR_PHY, "ulsch coding nb_rb %d, Nl = %d\n", nb_rb, pusch_pdu->nrOfLayers);
    LOG_D(NR_PHY, "ulsch coding A %d G %d mod_order %d Coderate %f\n", A, G[pusch_id], Qm, Coderate);
    LOG_D(NR_PHY, "harq_pid %d, pusch_data.new_data_indicator %d\n", harq_pid, pusch_pdu->pusch_data.new_data_indicator);

    ///////////////////////// a---->| add CRC |---->b /////////////////////////

    const int max_payload_bytes = MAX_NUM_NR_ULSCH_SEGMENTS_PER_LAYER * pusch_pdu->nrOfLayers * 1056;
    int B;
    if (A > NR_MAX_PDSCH_TBS) {
      // Add 24-bit crc (polynomial A) to payload
      const unsigned int crc = crc24a(harq_process->payload_AB, A) >> 8;
      harq_process->payload_AB[A >> 3] = ((uint8_t *)&crc)[2];
      harq_process->payload_AB[1 + (A >> 3)] = ((uint8_t *)&crc)[1];
      harq_process->payload_AB[2 + (A >> 3)] = ((uint8_t *)&crc)[0];
      B = A + 24;
      AssertFatal((A / 8) + 4 <= max_payload_bytes, "A %d is too big (A/8+4 = %d > %d)\n", A, (A / 8) + 4, max_payload_bytes);
    } else {
      // Add 16-bit crc (polynomial A) to payload
      const unsigned int crc = crc16(harq_process->payload_AB, A) >> 16;
      harq_process->payload_AB[A >> 3] = ((uint8_t *)&crc)[1];
      harq_process->payload_AB[1 + (A >> 3)] = ((uint8_t *)&crc)[0];
      B = A + 16;
      AssertFatal((A / 8) + 3 <= max_payload_bytes, "A %d is too big (A/8+3 = %d > %d)\n", A, (A / 8) + 3, max_payload_bytes);
    }

#if T_TRACER
    {
      // capture Tx Payload via T-Tracer
      log_ul_payload_tx_bits(frame, slot, &ue->frame_parms, pusch_pdu,
                             get_num_dmrs(pusch_pdu->ul_dmrs_symb_pos),
                             get_dmrs_port(0, pusch_pdu->dmrs_ports),
                             (const uint8_t *)harq_process->payload_AB,
                             pusch_pdu->pusch_data.tb_size);
    }
#endif
    ///////////////////////// b---->| block segmentation |---->c /////////////////////////

    harq_process->BG = pusch_pdu->ldpcBaseGraph;

    harq_process->Kb = nr_segmentation(harq_process->payload_AB,
                                       harq_process->c,
                                       B,
                                       &harq_process->C,
                                       &harq_process->K,
                                       &harq_process->Z,
                                       &harq_process->F,
                                       harq_process->BG);
    if (harq_process->C > MAX_NUM_NR_DLSCH_SEGMENTS_PER_LAYER * pusch_pdu->nrOfLayers) {
      LOG_E(PHY, "nr_segmentation.c: too many segments %d, B %d\n", harq_process->C, B);
      return (-1);
    }
  } // pusch_id
  return 0;
}

int nr_ulsch_encoding(PHY_VARS_NR_UE *ue,
                      NR_UE_ULSCH_t *ulsch,
                      const uint32_t frame,
                      const uint8_t slot,
                      unsigned int *G,
                      int nb_ulsch,
                      uint8_t *ULSCH_ids)
{
  start_meas_nr_ue_phy(ue, ULSCH_ENCODING_STATS);

  nrLDPC_TB_encoding_parameters_t TBs[nb_ulsch];
  memset(TBs, 0, sizeof(TBs));
  nrLDPC_slot_encoding_parameters_t slot_parameters = {.frame = frame,
                                                       .slot = slot,
                                                       .nb_TBs = nb_ulsch,
                                                       .threadPool = &get_nrUE_params()->Tpool,
                                                       .TBs = TBs};

  int max_num_segments = 0;
  for (uint_fast8_t pusch_id = 0; pusch_id < nb_ulsch; pusch_id++) {
    const uint8_t ULSCH_id = ULSCH_ids[pusch_id];
    const uint8_t harq_pid = ulsch[ULSCH_id].pusch_pdu.pusch_data.harq_process_id;
    NR_UL_UE_HARQ_t *harq_process = &ue->ul_harq_processes[harq_pid];
    max_num_segments = max(max_num_segments, harq_process->C);
  }

  nrLDPC_segment_encoding_parameters_t segments[nb_ulsch][max_num_segments];
  memset(segments, 0, sizeof(segments));

  for (uint8_t pusch_id = 0; pusch_id < nb_ulsch; pusch_id++) {
    uint8_t ULSCH_id = ULSCH_ids[pusch_id];
    uint8_t harq_pid = ulsch[ULSCH_id].pusch_pdu.pusch_data.harq_process_id;

    nrLDPC_TB_encoding_parameters_t *TB_parameters = &TBs[pusch_id];
    NR_UL_UE_HARQ_t *harq_process = &ue->ul_harq_processes[harq_pid];
    const nfapi_nr_ue_pusch_pdu_t *pusch_pdu = &ulsch[ULSCH_id].pusch_pdu;
    const uint16_t nb_rb = pusch_pdu->rb_size;
    TB_parameters->harq_unique_pid = 2 * harq_pid + ULSCH_id;
    TB_parameters->C = harq_process->C;
    TB_parameters->K = harq_process->K;
    TB_parameters->Z = harq_process->Z;
    TB_parameters->F = harq_process->F;
    TB_parameters->BG = harq_process->BG;
    TB_parameters->Kb = harq_process->Kb;
    TB_parameters->nb_rb = nb_rb;
    TB_parameters->Qm = pusch_pdu->qam_mod_order;
    TB_parameters->mcs = pusch_pdu->mcs_index;
    TB_parameters->nb_layers = pusch_pdu->nrOfLayers;
    TB_parameters->rv_index = pusch_pdu->pusch_data.rv_index;
    TB_parameters->G = G[pusch_id];
    TB_parameters->tbslbrm = pusch_pdu->tbslbrm;
    TB_parameters->A = pusch_pdu->pusch_data.tb_size / 8;
    TB_parameters->segments = segments[pusch_id];

    memset(harq_process->f, 0, 14 * nb_rb * 12 * 16);
    TB_parameters->output = harq_process->f;

    for (int r = 0; r < TB_parameters->C; r++) {
      nrLDPC_segment_encoding_parameters_t *segment_parameters = &TB_parameters->segments[r];
      int E = nr_get_E(TB_parameters->G, TB_parameters->C, TB_parameters->Qm, TB_parameters->nb_layers, r);
      if (E < 0)
         return -1;
      segment_parameters->c = harq_process->c[r];
      segment_parameters->E = E;
      reset_meas(&segment_parameters->ts_ldpc_encode);

    } // TB_parameters->C
  } // pusch_id

  ///////////////////////// | LDCP coding | ////////////////////////////////////
  ue->nrLDPC_coding_interface.nrLDPC_coding_encoder(&slot_parameters);

  for (uint8_t pusch_id = 0; pusch_id < nb_ulsch; pusch_id++) {
    nrLDPC_TB_encoding_parameters_t *TB_parameters = &TBs[pusch_id];
    for (int r = 0; r < TB_parameters->C; r++) {
      nrLDPC_segment_encoding_parameters_t *segment_parameters = &TB_parameters->segments[r];
      merge_meas(&ue->phy_cpu_stats.cpu_time_stats[ULSCH_LDPC_ENCODING_STATS], &segment_parameters->ts_ldpc_encode);
    }
  }

  stop_meas_nr_ue_phy(ue, ULSCH_ENCODING_STATS);
  return 0;
}
