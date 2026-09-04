/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*! \file PHY/NR_UE_TRANSPORT/nr_dlsch_decoding_slot.c
 */

#include "PHY/defs_nr_UE.h"
#include "SCHED_NR_UE/harq_nr.h"
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

/*! \brief Prepare necessary parameters for nrLDPC_coding_interface
 */
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
  nrLDPC_TB_decoding_parameters_t TB_parameters = {0};
  nrLDPC_slot_decoding_parameters_t slot_parameters = {
    .frame = proc->frame_rx,
    .slot = proc->nr_slot_rx,
    .nb_TBs = 1,
    .threadPool = &get_nrUE_params()->Tpool,
    .TBs = &TB_parameters
  };

  uint8_t dmrs_Type = dlsch_config->dmrsConfigType;
  int harq_pid = dlsch_config->harq_process_nbr;
  AssertFatal(dmrs_Type == 0 || dmrs_Type == 1, "Illegal dmrs_type %d\n", dmrs_Type);
  fapi_nr_dl_cw_info_t *cw_info = &dlsch->cw_info;
  NR_DL_UE_HARQ_t *harq_process = &phy_vars_ue->dl_harq_processes[cw_idx][dlsch_config->harq_process_nbr];
  LOG_D(PHY, "Round %d RV idx %d\n", harq_process->DLround, cw_info->rv);
  uint8_t nb_re_dmrs;
  if (dmrs_Type == NFAPI_NR_DMRS_TYPE1)
    nb_re_dmrs = 6 * dlsch_config->n_dmrs_cdm_groups;
  else
    nb_re_dmrs = 4 * dlsch_config->n_dmrs_cdm_groups;
  uint16_t dmrs_length = get_num_dmrs(dlsch_config->dlDmrsSymbPos);

  TB_parameters.harq_unique_pid = 2 * harq_pid + cw_idx;
  TB_parameters.G = G;
  TB_parameters.nb_rb = number_rbs;
  TB_parameters.Qm = cw_info->qamModOrder;
  TB_parameters.mcs = cw_info->mcs;
  TB_parameters.nb_layers = cw_info->Nl;
  TB_parameters.BG = cw_info->ldpcBaseGraph;
  TB_parameters.A = cw_info->TBS;
  TB_parameters.processedSegments = &harq_process->processedSegments;

  float Coderate = (float)cw_info->targetCodeRate / 10240.0f;

  LOG_D(PHY,
        "%d.%d DLSCH %d Decoding, harq_pid %d TBS %d G %d nb_re_dmrs %d length dmrs %d mcs %d Nl %d nb_symb_sch %d nb_rb %d Qm %d "
        "Coderate %f\n",
        slot_parameters.frame,
        slot_parameters.slot,
        cw_idx,
        harq_pid,
        cw_info->TBS,
        G,
        nb_re_dmrs,
        dmrs_length,
        TB_parameters.mcs,
        TB_parameters.nb_layers,
        dlsch_config->number_symbols,
        TB_parameters.nb_rb,
        TB_parameters.Qm,
        Coderate);

  if (harq_process->first_rx == 1) {
    // This is a new packet, so compute quantities regarding segmentation
    nr_segmentation(NULL,
                    NULL,
                    lenWithCrc(1, TB_parameters.A), // We give a max size in case of 1 segment
                    &TB_parameters.C,
                    &TB_parameters.K,
                    &TB_parameters.Z, // [hna] Z is Zc
                    &TB_parameters.F,
                    TB_parameters.BG);
    harq_process->C = TB_parameters.C;
    harq_process->K = TB_parameters.K;
    harq_process->Z = TB_parameters.Z;
    harq_process->F = TB_parameters.F;

    if (harq_process->C > MAX_NUM_NR_DLSCH_SEGMENTS_PER_LAYER * TB_parameters.nb_layers) {
      LOG_E(PHY, "nr_segmentation.c: too many segments %d, A %d\n", harq_process->C, TB_parameters.A);
      return;
    }

    if (LOG_DEBUGFLAG(DEBUG_DLSCH_DECOD) && (!slot_parameters.frame % 100))
      LOG_I(PHY, "K %d C %d Z %d nl %d \n", harq_process->K, harq_process->C, harq_process->Z, TB_parameters.nb_layers);
  } else {
    // This is not a new packet, so retrieve previously computed quantities regarding segmentation
    TB_parameters.C = harq_process->C;
    TB_parameters.K = harq_process->K;
    TB_parameters.Z = harq_process->Z;
    TB_parameters.F = harq_process->F;
  }

  if (LOG_DEBUGFLAG(DEBUG_DLSCH_DECOD))
    LOG_I(PHY, "Segmentation: C %d, K %d\n", harq_process->C, harq_process->K);

  TB_parameters.max_ldpc_iterations = dlsch->max_ldpc_iterations;
  TB_parameters.rv_index = cw_info->rv;
  TB_parameters.tbslbrm = dlsch_config->tbslbrm;
  TB_parameters.abort_decode = &harq_process->abort_decode;
  set_abort(&harq_process->abort_decode, false);

  TB_parameters.llr = dlsch_llr;
  LOG_D(NR_PHY,
        "Decoding C %d, Qm %d, TB_parameters.llr %p\n",
        TB_parameters.C,
        TB_parameters.Qm,
        TB_parameters.llr);
  int E = nr_get_E(TB_parameters.G, TB_parameters.C, TB_parameters.Qm, TB_parameters.nb_layers, 0);
  if (E < 0)
    return;
  TB_parameters.c = harq_process->c;
  TB_parameters.d = harq_process->d;
  TB_parameters.E = E;
  TB_parameters.E2 = TB_parameters.E;
  TB_parameters.first_rE2 = TB_parameters.C;
  for (int r = 1; r < TB_parameters.C; r++) {
    int Er = nr_get_E(TB_parameters.G, TB_parameters.C, TB_parameters.Qm, TB_parameters.nb_layers, r);
    if (Er != TB_parameters.E) {
      TB_parameters.E2 = Er;
      TB_parameters.first_rE2 = r;
      break;
    }
  }
  TB_parameters.R = nr_get_R_ldpc_decoder(TB_parameters.rv_index,
                                          TB_parameters.E,
                                          TB_parameters.BG,
                                          TB_parameters.Z,
                                          &harq_process->llrLen,
                                          harq_process->DLround);
  TB_parameters.d_to_be_cleared = harq_process->first_rx == 1;
  for (int r = 0; r < TB_parameters.C; r++)
    TB_parameters.decodeSuccess[r] = false;
  reset_meas(&TB_parameters.ts_ldpc_decode);

  for (int r = 0; r < TB_parameters.C; r++) {
    int Etmp = nr_get_E(TB_parameters.G, TB_parameters.C, TB_parameters.Qm, TB_parameters.nb_layers, r);
    if (Etmp != TB_parameters.E) {
      TB_parameters.E2 = Etmp;
      TB_parameters.R2 = nr_get_R_ldpc_decoder(TB_parameters.rv_index,
                                               TB_parameters.E2,
                                               TB_parameters.BG,
                                               TB_parameters.Z,
                                               &harq_process->llrLen,
                                               harq_process->DLround);
      TB_parameters.first_rE2 = r;
      break;
    }
  }

  int ret_decoder = phy_vars_ue->nrLDPC_coding_interface.nrLDPC_coding_decoder(&slot_parameters);

  if (ret_decoder != 0) {
    LOG_E(PHY, "nrLDPC_coding_decoder returned an error: %d\n", ret_decoder);
    return;
  }

  bool crcok = true;
  for (int r = 0; r < TB_parameters.C; r++)
    if (TB_parameters.decodeSuccess[r] == false) {
      LOG_D(PHY, "downlink segment error %d/%d\n", r, harq_process->C);
      crcok = false;
      break;
    }

  if (crcok) {
    uint8_t *output = b;
    const uint8_t *in = harq_process->c;
    const int sz = (harq_process->K - harq_process->F) / 8 - (harq_process->C > 1 ? 3 : 0);
    for (int r = 0; r < TB_parameters.C; r++) {
      memcpy(output, in, sz);
      output += sz;
      in += harq_process->K / 8;
    }
  } else {
    LOG_D(PHY, "frame=%d, slot=%d, first_rx=%d, rv_index=%d\n", proc->frame_rx, proc->nr_slot_rx, harq_process->first_rx, cw_info->rv);
  }

  merge_meas(&phy_vars_ue->phy_cpu_stats.cpu_time_stats[DLSCH_LDPC_DECODING_STATS], &TB_parameters.ts_ldpc_decode);

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
            TB_parameters.d_to_be_cleared,
            cw_info->TBS,
            TB_parameters.A,
            harq_process->K,
            harq_process->Z,
            harq_process->F,
            TB_parameters.E,
            TB_parameters.E2,
            TB_parameters.first_rE2,
            TB_parameters.R,
            TB_parameters.R2,
            TB_parameters.Qm,
            TB_parameters.nb_layers);
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

  if (harq_process->decodeResult) {
    LOG_D(PHY, "%d.%d DLSCH received ok \n", proc->frame_rx, proc->nr_slot_rx);
    harq_process->status = NR_SCH_IDLE;
    dlsch->last_iteration_cnt = dlsch->max_ldpc_iterations - 1;
  } else {
    LOG_D(PHY, "%d.%d DLSCH received nok \n", proc->frame_rx, proc->nr_slot_rx);
    kpiStructure.nb_nack++;
    dlsch->last_iteration_cnt = dlsch->max_ldpc_iterations;
    UEdumpScopeData(phy_vars_ue, proc->nr_slot_rx, proc->frame_rx, "DLSCH_NACK");
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
