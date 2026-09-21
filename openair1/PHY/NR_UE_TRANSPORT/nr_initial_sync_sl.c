/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Sidelink specific part of the initial UE synchronisation procedure.
 *
 * Finding the SL-SSB in the received samples (PSS correlation, SSS detection) is done by the
 * common SS/PBCH block search in nr_initial_sync.c, which the sidelink and the downlink
 * share. What is left here is decoding the PSBCH carried by the block and handing the SL-MIB
 * contents to the upper layers.
 */

#include "PHY/defs_nr_UE.h"
#include "PHY/TOOLS/tools_defs.h"
#include "PHY/NR_REFSIG/ss_pbch_nr.h"
#include "PHY/NR_UE_ESTIMATION/nr_estimation.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_proto_ue.h"
#include "SCHED_NR_UE/defs.h"

/* Symbol of the SL-SSB that follows `symbol` and carries PSBCH: symbol 0 and symbols 5 to 12
   carry PSBCH, symbols 1 to 4 carry PSS and SSS (TS 38.211 8.4.3.1). */
static int next_psbch_symbol(int symbol)
{
  return (symbol == 0) ? 5 : symbol + 1;
}

bool sl_nr_psbch_detection(nr_ue_ssb_scan_t *ssbInfo,
                           const c16_t rxdataF[SL_N_SYMBOLS_SSB][ssbInfo->fp->nb_antennas_rx][ssbInfo->fp->ofdm_symbol_size])
{
  PHY_VARS_NR_UE *ue = ssbInfo->ue;
  const NR_DL_FRAME_PARMS *fp = ssbInfo->fp;
  const int slss_id = ssbInfo->nidCell;

  int16_t psbch_e_rx[SL_NR_POLAR_PSBCH_E_NORMAL_CP + 2] = {0};
  int16_t psbch_unClipped[SL_NR_POLAR_PSBCH_E_NORMAL_CP + 2] = {0};
  int psbch_e_rx_offset = 0;

  for (int symbol = 0; symbol < SL_N_SYMBOLS_SSB; symbol = next_psbch_symbol(symbol)) {
    __attribute__((aligned(32))) c16_t dl_ch_estimates[fp->nb_antennas_rx][fp->ofdm_symbol_size];
    for (int aarx = 0; aarx < fp->nb_antennas_rx; aarx++) {
      nr_pbch_channel_estimation(fp,
                                 &ue->SL_UE_PHY_PARAMS,
                                 dl_ch_estimates[aarx],
                                 ssbInfo->proc,
                                 symbol,
                                 0,
                                 0,
                                 ssbInfo->gscnInfo.ssbFirstSC,
                                 rxdataF[symbol][aarx],
                                 true,
                                 slss_id);
    }
    nr_generate_psbch_llr(fp, rxdataF[symbol], dl_ch_estimates, symbol, &psbch_e_rx_offset, psbch_e_rx, psbch_unClipped);
    ssbInfo->adjust_rxgain = nr_sl_psbch_rsrp_measurements(ue, &ue->SL_UE_PHY_PARAMS, fp, symbol, rxdataF[symbol], false);
  }

  if (nr_psbch_decode(ue, psbch_e_rx, ssbInfo->proc, psbch_e_rx_offset, slss_id, NULL, ssbInfo->psbchPayload) != 0) {
    LOG_I(PHY, "SIDELINK SLSS SEARCH: SLSS id %d found but PSBCH CRC not OK\n", slss_id);
    return false;
  }

  // retrieve DFN and slot number from SL-MIB
  const uint32_t psbch_payload = *(uint32_t *)ssbInfo->psbchPayload;
  ssbInfo->frameNumber = ((psbch_payload & 0x0700) >> 1) | ((psbch_payload & 0xFE0000) >> 17);
  ssbInfo->slotOffset = ((psbch_payload & 0x010000) >> 10) | ((psbch_payload & 0xFC000000) >> 26);
  // The SL-SSB always starts on the first symbol of its slot.
  ssbInfo->symbolOffset = ssbInfo->slotOffset * fp->symbols_per_slot;

  LOG_A(PHY,
        "UE[%d] SIDELINK SLSS SEARCH: PSBCH RX OK. SL-MIB: DFN:%d, slot:%d\n",
        ue->Mod_id,
        ssbInfo->frameNumber,
        ssbInfo->slotOffset);
  return true;
}

void sl_nr_apply_slss_sync(PHY_VARS_NR_UE *ue, const UE_nr_rxtx_proc_t *proc, const nr_ue_ssb_scan_t *res)
{
  sl_nr_ue_phy_params_t *sl_ue = &ue->SL_UE_PHY_PARAMS;
  SL_NR_SYNC_PARAMS_t *sync_params = &sl_ue->sync_params;

  sync_params->N_sl_id = res->nidCell;
  sync_params->N_sl_id1 = nr_cell_id_to_nid1(true, res->nidCell);
  sync_params->N_sl_id2 = nr_cell_id_to_nid2(true, res->nidCell);
  sync_params->freq_offset = res->freqOffset;
  sync_params->ssb_offset = res->ssbOffset;
  sync_params->DFN = res->frameNumber;
  sync_params->slot_offset = res->slotOffset;

  nr_sidelink_indication_t sl_indication;
  sl_nr_rx_indication_t rx_ind = {0};
  nr_fill_sl_indication(&sl_indication, &rx_ind, NULL, proc, ue, NULL);
  nr_fill_sl_rx_indication(&rx_ind, SL_NR_RX_PDU_TYPE_SSB, ue, 1, (void *)res->psbchPayload, res->nidCell);

  LOG_D(PHY, "Sidelink SLSS SEARCH PSBCH RX OK. Send SL-SSB TO MAC\n");

  if (ue->if_inst && ue->if_inst->sl_indication)
    ue->if_inst->sl_indication(&sl_indication);
}
