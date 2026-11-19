/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*! \file PHY/NR_UE_TRANSPORT/nr_dlsch_decoding_slot.c
 */

#include "PHY/defs_nr_UE.h"
#include "PHY/CODING/coding_extern.h"
#include "PHY/CODING/coding_defs.h"
#include "PHY/CODING/nrLDPC_coding/nrLDPC_coding_interface.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_proto_ue.h"
#include "SCHED_NR_UE/defs.h"
#include "SIMULATION/TOOLS/sim.h"
#include "executables/nr-uesoftmodem.h"
#include "PHY/CODING/nrLDPC_extern.h"
#include "common/utils/nr/nr_common.h"
#include "openair1/PHY/TOOLS/phy_scope_interface.h"
#include "nfapi/open-nFAPI/nfapi/public_inc/nfapi_nr_interface.h"

static extended_kpi_ue kpiStructure = {0};

extended_kpi_ue* getKPIUE(void) {
  return &kpiStructure;
}

void nr_ue_dlsch_init(NR_UE_DLSCH_t *dlsch_list, int num_dlsch, uint8_t max_ldpc_iterations) {
  for (int i=0; i < num_dlsch; i++) {
    NR_UE_DLSCH_t *dlsch = dlsch_list + i;
    memset(dlsch, 0, sizeof(NR_UE_DLSCH_t));
    dlsch->max_ldpc_iterations = max_ldpc_iterations;
  }
}

void nr_dlsch_unscrambling(int16_t *llr, uint32_t size, uint8_t q, uint32_t Nid, uint32_t n_RNTI)
{
  nr_codeword_unscrambling(llr, size, q, Nid, n_RNTI);
}

/// Decoding parameters of a DLSCH codeword, except the decoded segments buffer; false: nothing to decode
static bool nr_dlsch_decoding_prepare(PHY_VARS_NR_UE *ue,
                                      const UE_nr_rxtx_proc_t *proc,
                                      NR_UE_DLSCH_t *dlsch,
                                      int cw_idx,
                                      fapi_nr_dl_config_dlsch_pdu_rel15_t *dlsch_config,
                                      int16_t *dlsch_llr,
                                      int number_rbs,
                                      int G,
                                      nrLDPC_TB_decoding_parameters_t *TB)
{
  uint8_t dmrs_Type = dlsch_config->dmrsConfigType;
  int harq_pid = dlsch_config->harq_process_nbr;
  AssertFatal(dmrs_Type == 0 || dmrs_Type == 1, "Illegal dmrs_type %d\n", dmrs_Type);
  fapi_nr_dl_cw_info_t *cw_info = &dlsch->cw_info;
  NR_DL_UE_HARQ_t *harq_process = &ue->dl_harq_processes[cw_idx][dlsch_config->harq_process_nbr];
  if (harq_process->activated_frame != proc->frame_rx || harq_process->activated_slot != proc->nr_slot_rx) {
    LOG_W(PHY, "%d.%d DLSCH harq %d late decode aborted, re-armed %d.%d\n",
          proc->frame_rx, proc->nr_slot_rx, harq_pid, harq_process->activated_frame, harq_process->activated_slot);
    return false;
  }
  LOG_D(PHY, "Round %d RV idx %d\n", harq_process->DLround, cw_info->rv);
  uint8_t nb_re_dmrs;
  if (dmrs_Type == NFAPI_NR_DMRS_TYPE1)
    nb_re_dmrs = 6 * dlsch_config->n_dmrs_cdm_groups;
  else
    nb_re_dmrs = 4 * dlsch_config->n_dmrs_cdm_groups;
  uint16_t dmrs_length = get_num_dmrs(dlsch_config->dlDmrsSymbPos);

  TB->harq_unique_pid = 2 * harq_pid + cw_idx;
  TB->G = G;
  TB->nb_rb = number_rbs;
  TB->Qm = cw_info->qamModOrder;
  TB->mcs = cw_info->mcs;
  TB->nb_layers = cw_info->Nl;
  TB->BG = cw_info->ldpcBaseGraph;
  TB->A = cw_info->TBS;
  TB->processedSegments = &harq_process->processedSegments;

  float Coderate = (float)cw_info->targetCodeRate / 10240.0f;

  LOG_D(PHY,
        "%d.%d DLSCH %d Decoding, harq_pid %d TBS %d G %d nb_re_dmrs %d length dmrs %d mcs %d Nl %d nb_symb_sch %d nb_rb %d Qm %d "
        "Coderate %f\n",
        proc->frame_rx,
        proc->nr_slot_rx,
        cw_idx,
        harq_pid,
        cw_info->TBS,
        G,
        nb_re_dmrs,
        dmrs_length,
        TB->mcs,
        TB->nb_layers,
        dlsch_config->number_symbols,
        TB->nb_rb,
        TB->Qm,
        Coderate);

  if (harq_process->first_rx == 1) {
    // This is a new packet, so compute quantities regarding segmentation
    nr_segmentation(NULL,
                    NULL,
                    lenWithCrc(1, TB->A), // We give a max size in case of 1 segment
                    &TB->C,
                    &TB->K,
                    &TB->Z, // [hna] Z is Zc
                    &TB->F,
                    TB->BG);
    harq_process->C = TB->C;
    harq_process->K = TB->K;
    harq_process->Z = TB->Z;
    harq_process->F = TB->F;

    if (harq_process->C > MAX_NUM_NR_DLSCH_SEGMENTS_PER_LAYER * TB->nb_layers) {
      LOG_E(PHY, "nr_segmentation.c: too many segments %d, A %d\n", harq_process->C, TB->A);
      return false;
    }

    if (LOG_DEBUGFLAG(DEBUG_DLSCH_DECOD) && (!proc->frame_rx % 100))
      LOG_I(PHY, "K %d C %d Z %d nl %d \n", harq_process->K, harq_process->C, harq_process->Z, TB->nb_layers);
  } else {
    // This is not a new packet, so retrieve previously computed quantities regarding segmentation
    TB->C = harq_process->C;
    TB->K = harq_process->K;
    TB->Z = harq_process->Z;
    TB->F = harq_process->F;
  }

  if (LOG_DEBUGFLAG(DEBUG_DLSCH_DECOD))
    LOG_I(PHY, "Segmentation: C %d, K %d\n", harq_process->C, harq_process->K);

  TB->d_to_be_cleared = harq_process->first_rx == 1;
  /* Buffer now holds this TB. Clear first_rx only after that, so a skipped decode still re-segments. */
  harq_process->first_rx = 0;
  /* Restart llrLen when the buffer is cleared, even if DLround is already non-zero. */
  const int llr_round = TB->d_to_be_cleared ? 0 : harq_process->DLround;

  TB->max_ldpc_iterations = dlsch->max_ldpc_iterations;
  TB->rv_index = cw_info->rv;
  TB->tbslbrm = dlsch_config->tbslbrm;
  TB->abort_decode = &harq_process->abort_decode;
  set_abort(&harq_process->abort_decode, false);

  TB->llr = dlsch_llr;
  LOG_D(NR_PHY, "Decoding C %d, Qm %d, TB->llr %p\n", TB->C, TB->Qm, TB->llr);
  int E = nr_get_E(TB->G, TB->C, TB->Qm, TB->nb_layers, 0);
  if (E < 0)
    return false;
  TB->d = harq_process->d;
  TB->E = E;
  TB->E2 = TB->E;
  TB->first_rE2 = TB->C;
  for (int r = 1; r < TB->C; r++) {
    int Er = nr_get_E(TB->G, TB->C, TB->Qm, TB->nb_layers, r);
    if (Er != TB->E) {
      TB->E2 = Er;
      TB->first_rE2 = r;
      break;
    }
  }
  TB->R = nr_get_R_ldpc_decoder(TB->rv_index, TB->E, TB->BG, TB->Z, &harq_process->llrLen, llr_round);
  for (int r = 0; r < TB->C; r++)
    TB->decodeSuccess[r] = false;

  for (int r = 0; r < TB->C; r++) {
    reset_single_meas(&TB->ts_ldpc_decode[r]);
    int Etmp = nr_get_E(TB->G, TB->C, TB->Qm, TB->nb_layers, r);
    if (Etmp != TB->E) {
      TB->E2 = Etmp;
      TB->R2 = nr_get_R_ldpc_decoder(TB->rv_index, TB->E2, TB->BG, TB->Z, &harq_process->llrLen, llr_round);
      TB->first_rE2 = r;
      break;
    }
  }
  return true;
}

/// Checks the decoded TB, copies it to b, updates the HARQ process
static void nr_dlsch_decoding_result(PHY_VARS_NR_UE *ue,
                                     const UE_nr_rxtx_proc_t *proc,
                                     NR_UE_DLSCH_t *dlsch,
                                     int cw_idx,
                                     fapi_nr_dl_config_dlsch_pdu_rel15_t *dlsch_config,
                                     nrLDPC_TB_decoding_parameters_t *TB,
                                     uint8_t *b,
                                     int number_rbs,
                                     int G)
{
  const int harq_pid = dlsch_config->harq_process_nbr;
  const fapi_nr_dl_cw_info_t *cw_info = &dlsch->cw_info;
  NR_DL_UE_HARQ_t *harq_process = &ue->dl_harq_processes[cw_idx][harq_pid];
  const uint8_t *c = TB->c;
  const uint8_t nb_re_dmrs = (dlsch_config->dmrsConfigType == NFAPI_NR_DMRS_TYPE1 ? 6 : 4) * dlsch_config->n_dmrs_cdm_groups;
  const uint16_t dmrs_length = get_num_dmrs(dlsch_config->dlDmrsSymbPos);
  const float Coderate = (float)cw_info->targetCodeRate / 10240.0f;
  bool crcok = true;
  for (int r = 0; r < TB->C; r++) {
    merge_single_meas(&ue->phy_cpu_stats.cpu_time_stats[DLSCH_LDPC_DECODING_STATS], &TB->ts_ldpc_decode[r]);

    if (TB->decodeSuccess[r] == false && crcok) {
      LOG_D(PHY, "downlink segment error %d/%d\n", r, harq_process->C);
      crcok = false;
    }
  }

  if (crcok) {
    uint8_t *output = b;
    const uint8_t *in = c;
    const int sz = (harq_process->K - harq_process->F) / 8 - (harq_process->C > 1 ? 3 : 0);
    for (int r = 0; r < TB->C; r++) {
      memcpy(output, in, sz);
      output += sz;
      in += harq_process->K / 8;
    }
  } else {
    LOG_D(PHY, "frame=%d, slot=%d, first_rx=%d, rv_index=%d\n", proc->frame_rx, proc->nr_slot_rx, TB->d_to_be_cleared, cw_info->rv);
  }

  kpiStructure.nb_total++;
  kpiStructure.blockSize = cw_info->TBS;
  kpiStructure.dl_mcs = cw_info->mcs;
  kpiStructure.nofRBs = number_rbs;

  harq_process->decodeResult = harq_process->processedSegments == harq_process->C;

  if (harq_process->decodeResult && harq_process->C > 1) {
    /* check global CRC */
    // we have regrouped the transport block
    if (!check_crc(b, lenWithCrc(1, cw_info->TBS), crcType(1, cw_info->TBS))) {
      LOG_E(PHY,
            " Frame %d.%d LDPC global CRC fails, but individual LDPC CRC succeeded. %d segs "
            "(harq_pid %d cw %d first_rx %d round %d rv %d clear %d | TBS %d A %d K %d Z %d F %d "
            "| E %d E2 %d first_rE2 %d R %d R2 %d Qm %d Nl %d)\n",
            proc->frame_rx,
            proc->nr_slot_rx,
            harq_process->C,
            harq_pid,
            cw_idx,
            harq_process->first_rx,
            harq_process->DLround,
            cw_info->rv,
            TB->d_to_be_cleared,
            cw_info->TBS,
            TB->A,
            harq_process->K,
            harq_process->Z,
            harq_process->F,
            TB->E,
            TB->E2,
            TB->first_rE2,
            TB->R,
            TB->R2,
            TB->Qm,
            TB->nb_layers);
      harq_process->decodeResult = false;
    }
  }

  if (harq_process->decodeResult) {
    // We search only a reccuring OAI error that propagates all 0 packets with a 0 CRC, so we
    const int sz = cw_info->TBS / 8;
    if (b[sz] == 0 && b[sz + 1] == 0) {
      // do the check only if the 2 first bytes of the CRC are 0 (it can be CRC16 or CRC24)
      int i = 0;
      while (b[i] == 0 && i < sz)
        i++;
      if (i == sz) {
        LOG_E(PHY,
              "received all 0 pdu (TBS %d, mcs %d, C %d, nb_rb %d, decodedSegments %d) consider it false reception, even if the "
              "TS 38.212 7.2.1 says only we should attach the "
              "corresponding CRC, and nothing prevents to have a all 0 packet\n",
              cw_info->TBS,
              cw_info->mcs,
              harq_process->C,
              number_rbs,
              harq_process->processedSegments);
        harq_process->decodeResult = false;
      }
    }
  }

  /* LOG_D only: this runs on a SCHED_FIFO thread; LOG_I formatting was enough to miss slot deadlines. */
  if (harq_process->DLround > 0 || !harq_process->decodeResult)
    LOG_D(PHY,
          "%d.%d DL HARQ combine harq %d r%d rv %d %s cleared %d llrLen %d E %d G %d\n",
          proc->frame_rx,
          proc->nr_slot_rx,
          harq_pid,
          harq_process->DLround,
          TB->rv_index,
          harq_process->decodeResult ? "ok" : "nok",
          TB->d_to_be_cleared,
          harq_process->llrLen,
          TB->E,
          TB->G);

  if (harq_process->decodeResult) {
    LOG_D(PHY, "%d.%d DLSCH received ok \n", proc->frame_rx, proc->nr_slot_rx);
    /* Only retire if this decode still owns the PID; a late finish must not idle a newer grant. */
    if (harq_process->activated_frame == proc->frame_rx && harq_process->activated_slot == proc->nr_slot_rx) {
      harq_process->status = NR_SCH_IDLE;
    } else {
      LOG_W(PHY, "%d.%d DLSCH harq %d decoded ok but re-armed %d.%d, not retiring\n",
            proc->frame_rx, proc->nr_slot_rx, harq_pid, harq_process->activated_frame, harq_process->activated_slot);
    }
    dlsch->last_iteration_cnt = dlsch->max_ldpc_iterations - 1;
  } else {
    LOG_D(PHY, "%d.%d DLSCH received nok \n", proc->frame_rx, proc->nr_slot_rx);
    kpiStructure.nb_nack++;
    dlsch->last_iteration_cnt = dlsch->max_ldpc_iterations;
    UEdumpScopeData(ue, proc->nr_slot_rx, proc->frame_rx, "DLSCH_NACK");
  }

  LOG_D(PHY,
        "%d.%d DLSCH Decoded, harq_pid %d, cw_idx %d round %d, result: %d TBS %d (%d) G %d nb_re_dmrs %d length dmrs %d mcs %d Nl %d "
        "nb_symb_sch %d "
        "nb_rb %d Qm %d Coderate %f\n",
        proc->frame_rx,
        proc->nr_slot_rx,
        harq_pid,
        cw_idx,
        harq_process->DLround,
        harq_process->decodeResult,
        cw_info->TBS,
        cw_info->TBS / 8,
        G,
        nb_re_dmrs,
        dmrs_length,
        cw_info->mcs,
        cw_info->Nl,
        dlsch_config->number_symbols,
        number_rbs,
        cw_info->qamModOrder,
        Coderate);
}

void nr_dlsch_decoding(PHY_VARS_NR_UE *phy_vars_ue,
                       const UE_nr_rxtx_proc_t *proc,
                       NR_UE_DLSCH_t *dlsch,
                       int cw_idx,
                       fapi_nr_dl_config_dlsch_pdu_rel15_t *dlsch_config,
                       int16_t *dlsch_llr,
                       uint8_t *b,
                       int number_rbs,
                       int G)
{
  nrLDPC_TB_decoding_parameters_t TB = {0};
  if (!nr_dlsch_decoding_prepare(phy_vars_ue, proc, dlsch, cw_idx, dlsch_config, dlsch_llr, number_rbs, G, &TB))
    return;
#ifdef LDPC_CUDA
  uint8_t *c = phy_vars_ue->dl_harq_processes[cw_idx][dlsch_config->harq_process_nbr].c; // written by the GPU: mapped memory
#else
  // decoded segments, segment r at r * K / 8: only used during this call
  uint8_t c[TB.C * (TB.K >> 3)] __attribute__((aligned(32)));
#endif
  TB.c = c;
  nrLDPC_slot_decoding_parameters_t slot_parameters = {.frame = proc->frame_rx,
                                                       .slot = proc->nr_slot_rx,
                                                       .nb_TBs = 1,
                                                       .threadPool = &get_nrUE_params()->Tpool,
                                                       .TBs = &TB};
  int ret_decoder = phy_vars_ue->nrLDPC_coding_interface.nrLDPC_coding_decoder(&slot_parameters);
  if (ret_decoder != 0) {
    LOG_E(PHY, "nrLDPC_coding_decoder returned an error: %d\n", ret_decoder);
    return;
  }
  nr_dlsch_decoding_result(phy_vars_ue, proc, dlsch, cw_idx, dlsch_config, &TB, b, number_rbs, G);
}
