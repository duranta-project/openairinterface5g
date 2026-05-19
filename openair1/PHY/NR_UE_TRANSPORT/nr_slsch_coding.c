/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*! \file PHY/NR_UE_TRANSPORT/nr_slsch_coding.c
 * \brief SLSCH LDPC encoding, TS 38.212 clause 8.2
 */

#include "PHY/NR_UE_TRANSPORT/nr_transport_proto_ue.h"
#include "PHY/CODING/coding_defs.h"
#include "PHY/CODING/nrLDPC_coding/nrLDPC_coding_interface.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_ue.h"
#include "executables/nr-uesoftmodem.h"

int nr_slsch_encoding(PHY_VARS_NR_UE *ue,
                      const sl_nr_tx_config_pscch_pssch_pdu_t *pscch_pssch_pdu,
                      uint8_t harq_pid,
                      const uint32_t frame,
                      const uint8_t slot,
                      unsigned int G)
{
  start_meas_nr_ue_phy(ue, ULSCH_ENCODING_STATS);

  NR_UL_UE_HARQ_t *harq_process = &ue->sl_harq_processes[harq_pid];
  DevAssert(harq_process->C > 0);
  nrLDPC_segment_encoding_parameters_t segments[harq_process->C];
  memset(segments, 0, sizeof(segments));

  nrLDPC_TB_encoding_parameters_t TB = {// the LDPC backends key per-HARQ state on this id, shared with the UL processes
                                        .harq_unique_pid = 2 * harq_pid,
                                        .nb_rb = pscch_pssch_pdu->l_subch * pscch_pssch_pdu->subchannel_size,
                                        .Qm = pscch_pssch_pdu->mod_order,
                                        .mcs = pscch_pssch_pdu->mcs,
                                        .nb_layers = pscch_pssch_pdu->num_layers,
                                        .rv_index = pscch_pssch_pdu->rv_index,
                                        .G = G,
                                        .tbslbrm = pscch_pssch_pdu->tbslbrm,
                                        .A = pscch_pssch_pdu->tb_size,
                                        .C = harq_process->C,
                                        .K = harq_process->K,
                                        .Z = harq_process->Z,
                                        .F = harq_process->F,
                                        .BG = harq_process->BG,
                                        .Kb = harq_process->Kb,
                                        .segments = segments};

  memset(harq_process->f, 0, 14 * TB.nb_rb * 12 * 16);
  TB.output = harq_process->f;

  for (int r = 0; r < TB.C; r++) {
    int E = nr_get_E(TB.G, TB.C, TB.Qm, TB.nb_layers, r);
    if (E < 0)
      return -1;
    segments[r].c = harq_process->c[r];
    segments[r].E = E;
    reset_meas(&segments[r].ts_ldpc_encode);
  }

  nrLDPC_slot_encoding_parameters_t slot_parameters = {.frame = frame,
                                                       .slot = slot,
                                                       .nb_TBs = 1,
                                                       .threadPool = &get_nrUE_params()->Tpool,
                                                       .TBs = &TB};
  ue->nrLDPC_coding_interface.nrLDPC_coding_encoder(&slot_parameters);

  for (int r = 0; r < TB.C; r++)
    merge_meas(&ue->phy_cpu_stats.cpu_time_stats[ULSCH_LDPC_ENCODING_STATS], &segments[r].ts_ldpc_encode);

  stop_meas_nr_ue_phy(ue, ULSCH_ENCODING_STATS);
  return 0;
}
