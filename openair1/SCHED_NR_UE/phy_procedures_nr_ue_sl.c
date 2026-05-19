/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#define _GNU_SOURCE

#include "PHY/defs_nr_UE.h"
#include <openair1/PHY/TOOLS/phy_scope_interface.h>
#include "common/utils/LOG/log.h"
#include "UTIL/OPT/opt.h"
#include "intertask_interface.h"
#include "T.h"
#include "PHY/MODULATION/modulation_UE.h"
#include "PHY/NR_UE_ESTIMATION/nr_estimation.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_proto_ue.h"
#include "PHY/CODING/nrPolar_tools/nr_polar_psbch_defs.h"
#include "openair1/PHY/nr_phy_common/inc/nr_phy_common.h"
#include "PHY/NR_UE_TRANSPORT/nr_slsch.h"
#include "executables/nr-uesoftmodem.h"

void nr_fill_sl_indication(nr_sidelink_indication_t *sl_ind,
                           sl_nr_rx_indication_t *rx_ind,
                           sl_nr_sci_indication_t *sci_ind,
                           const UE_nr_rxtx_proc_t *proc,
                           PHY_VARS_NR_UE *ue,
                           void *phy_data)
{
  memset((void *)sl_ind, 0, sizeof(nr_sidelink_indication_t));

  sl_ind->module_id = ue->Mod_id;
  sl_ind->hfn_rx = proc->hfn_rx;
  sl_ind->frame_rx = proc->frame_rx;
  sl_ind->slot_rx = proc->nr_slot_rx;
  sl_ind->hfn_tx = proc->hfn_tx;
  sl_ind->frame_tx = proc->frame_tx;
  sl_ind->slot_tx = proc->nr_slot_tx;
  sl_ind->phy_data = phy_data;
  sl_ind->slot_type = SIDELINK_SLOT_TYPE_RX;

  if (rx_ind) {
    sl_ind->rx_ind = rx_ind; //  hang on rx_ind instance
    sl_ind->sci_ind = NULL;
  }
  if (sci_ind) {
    sl_ind->rx_ind = NULL;
    sl_ind->sci_ind = sci_ind;
  }
}

void nr_fill_sl_rx_indication(sl_nr_rx_indication_t *rx_ind,
                              uint8_t pdu_type,
                              PHY_VARS_NR_UE *ue,
                              uint16_t n_pdus,
                              void *typeSpecific,
                              uint16_t rx_slss_id)
{
  if (n_pdus > 1) {
    LOG_E(NR_PHY, "In %s: multiple number of SL PDUs not supported yet...\n", __FUNCTION__);
  }

  sl_nr_ue_phy_params_t *sl_phy_params = &ue->SL_UE_PHY_PARAMS;

  switch (pdu_type) {
    case SL_NR_RX_PDU_TYPE_SLSCH:
    case SL_NR_RX_PDU_TYPE_SLSCH_PSFCH: {
        sl_nr_slsch_pdu_t *rx_slsch_pdu = &rx_ind->rx_indication_body[n_pdus - 1].rx_slsch_pdu;
        slsch_status_t *slsch_status = (slsch_status_t *)typeSpecific;
        rx_slsch_pdu->pdu        = slsch_status->b;
        rx_slsch_pdu->pdu_length = slsch_status->TBS;
        rx_slsch_pdu->harq_pid   = slsch_status->harq_pid;
        rx_slsch_pdu->ack_nack   = (slsch_status->rxok==true) ? 1 : 0;

        LOG_D(NR_MAC, "%4d.%2d Received %s SLSCH\n", rx_ind->sfn, rx_ind->slot, rx_slsch_pdu->ack_nack ? "Correct" : "Incorrect");
        if (slsch_status->rxok==true) ue->SL_UE_PHY_PARAMS.pssch.rx_ok++;
        else                          ue->SL_UE_PHY_PARAMS.pssch.rx_errors[0]++;
      }
      break;	    
      break;
    case FAPI_NR_RX_PDU_TYPE_SSB: {
      sl_nr_ssb_pdu_t *ssb_pdu = &rx_ind->rx_indication_body[n_pdus - 1].ssb_pdu;
      if (typeSpecific) {
        uint8_t *psbch_decoded_output = (uint8_t *)typeSpecific;
        memcpy(ssb_pdu->psbch_payload, psbch_decoded_output, 4); // 4 bytes of PSBCH payload bytes
        ssb_pdu->rsrp_dbm = sl_phy_params->psbch.rsrp_dBm_per_RE;
        ssb_pdu->rx_slss_id = rx_slss_id;
        ssb_pdu->decode_status = true;
        LOG_D(NR_PHY,
              "SL-IND: SSB to MAC. rsrp:%d, slssid:%d, payload:%x\n",
              ssb_pdu->rsrp_dbm,
              ssb_pdu->rx_slss_id,
              *((uint32_t *)(ssb_pdu->psbch_payload)));
      } else
        ssb_pdu->decode_status = false;
    } break;
    default:
      break;
  }

  rx_ind->rx_indication_body[n_pdus - 1].pdu_type = pdu_type;
  rx_ind->number_pdus = n_pdus;
}

static void nr_psbch_symbol_process(PHY_VARS_NR_UE *ue,
                                    const UE_nr_rxtx_proc_t *proc,
                                    const int symbol,
                                    const c16_t rxdataF[][ue->frame_parms.ofdm_symbol_size],
                                    int *psbch_e_rx_offset,
                                    int16_t psbch_e_rx[SL_NR_POLAR_PSBCH_E_NORMAL_CP + 2],
                                    int16_t psbch_unClipped[SL_NR_POLAR_PSBCH_E_NORMAL_CP + 2],
                                    c16_t dl_ch_estimates_time[ue->frame_parms.nb_antennas_rx][ue->frame_parms.ofdm_symbol_size])
{
  sl_nr_ue_phy_params_t *sl_phy_params = &ue->SL_UE_PHY_PARAMS;
  NR_DL_FRAME_PARMS *fp = &sl_phy_params->sl_frame_params;
  int slss_id = sl_phy_params->sl_config.sl_sync_source.rx_slss_id;

  __attribute__((aligned(32))) c16_t dl_ch_estimates[fp->nb_antennas_rx][fp->ofdm_symbol_size];
  start_meas(&sl_phy_params->channel_estimation_stats);
  for (int aarx = 0; aarx < fp->nb_antennas_rx; aarx++) {
    nr_pbch_channel_estimation(fp,
                               &ue->SL_UE_PHY_PARAMS,
                               dl_ch_estimates[aarx],
                               proc,
                               symbol,
                               0,
                               0,
                               fp->ssb_start_subcarrier,
                               rxdataF[aarx],
                               true,
                               slss_id);
    if (symbol == 12) {
      freq2time(ue->frame_parms.ofdm_symbol_size, (int16_t *)&dl_ch_estimates[aarx], (int16_t *)&dl_ch_estimates_time[aarx]);
    }
  }
  stop_meas(&sl_phy_params->channel_estimation_stats);

  if (symbol == 12)
    UEscopeCopy(ue,
                psbchDlChEstimateTime,
                (void *)dl_ch_estimates_time,
                sizeof(c16_t),
                fp->nb_antennas_rx,
                fp->ofdm_symbol_size,
                0);

  nr_generate_psbch_llr(fp, rxdataF, dl_ch_estimates, symbol, psbch_e_rx_offset, psbch_e_rx, psbch_unClipped);

  ue->adjust_rxgain = nr_sl_psbch_rsrp_measurements(ue, sl_phy_params, fp, symbol, rxdataF, false);
}

static unsigned int get_psbch_symbol_bitmap(const int num_symbols)
{
  unsigned int b = 0;
  for (int s = 0; s < num_symbols;) {
    b |= (0x1 << s);
    s = (s == 0) ? 5 : s + 1;
  }
  return b;
}

static bool is_psbch_symbol(const unsigned bitmap, const unsigned int symbol)
{
  return ((bitmap >> symbol) == 1);
}

static int nr_psbch_process(PHY_VARS_NR_UE *ue,
                            nr_phy_data_t *phy_data,
                            const UE_nr_rxtx_proc_t *proc,
                            const int symbol,
                            const c16_t rxdataF[][ue->frame_parms.ofdm_symbol_size],
                            int *psbch_e_rx_offset,
                            int16_t psbch_e_rx[SL_NR_POLAR_PSBCH_E_NORMAL_CP + 2],
                            int16_t psbch_unClipped[SL_NR_POLAR_PSBCH_E_NORMAL_CP + 2],
                            c16_t dl_ch_estimates_time[ue->frame_parms.nb_antennas_rx][ue->frame_parms.ofdm_symbol_size])
{
  sl_nr_ue_phy_params_t *sl_phy_params = &ue->SL_UE_PHY_PARAMS;
  NR_DL_FRAME_PARMS *fp = &sl_phy_params->sl_frame_params;
  const unsigned int numsymb = (fp->Ncp) ? SL_NR_NUM_SYMBOLS_SSB_EXT_CP : SL_NR_NUM_SYMBOLS_SSB_NORMAL_CP;
  const unsigned int symbol_bitmap = get_psbch_symbol_bitmap(numsymb);
  if (!is_psbch_symbol(symbol_bitmap, symbol)) {
    return 0;
  }

  const unsigned int last_symbol = numsymb - 1;
  int sampleShift = 0;

  nr_psbch_symbol_process(ue, proc, symbol, rxdataF, psbch_e_rx_offset, psbch_e_rx, psbch_unClipped, dl_ch_estimates_time);

  if (symbol == last_symbol) {
    const int slss_id = sl_phy_params->sl_config.sl_sync_source.rx_slss_id;
    uint8_t decoded_pdu[4] = {0};
    const int psbchSuccess = nr_psbch_decode(ue, psbch_e_rx, proc, *psbch_e_rx_offset, slss_id, phy_data, decoded_pdu);

    /* SV: Is this needed? */
    if (ue->no_timing_correction == 0 && psbchSuccess == 0) {
      LOG_D(NR_PHY, "start adjust sync slot = %d no timing %d\n", proc->nr_slot_rx, ue->no_timing_correction);
      sampleShift = nr_adjust_synch_ue(fp, ue, dl_ch_estimates_time, proc->frame_rx, proc->nr_slot_rx, 16384);
    }
  }

  UEscopeCopy(ue, psbchRxdataF_comp, psbch_unClipped, sizeof(c16_t), fp->nb_antennas_rx, *psbch_e_rx_offset / 2, 0);
  UEscopeCopy(ue, psbchLlr, psbch_e_rx, sizeof(int16_t), fp->nb_antennas_rx, *psbch_e_rx_offset, 0);

  return sampleShift;
}


extern int dmrs_pscch_mask[2];
int nr_slsch_procedures(PHY_VARS_NR_UE *ue, int frame_rx, int slot_rx, int SLSCH_id, const UE_nr_rxtx_proc_t *proc, nr_phy_data_t *phy_data, bool is_csi_rs_slot, int8_t *ack_nack_rcvd, int num_acks) {


  sl_nr_ue_phy_params_t *sl_phy_params = &ue->SL_UE_PHY_PARAMS;
  NR_DL_FRAME_PARMS *fp = &sl_phy_params->sl_frame_params;
  sl_nr_rx_config_pssch_pdu_t *slsch_pdu = &phy_data->nr_sl_pssch_pdu; //ue->slsch[SLSCH_id].harq_process->slsch_pdu;
  sl_nr_rx_config_pssch_sci_pdu_t *pssch_pdu = &phy_data->nr_sl_pssch_sci_pdu; //ue->slsch[SLSCH_id].harq_process->pssch_pdu;

  uint8_t  freq_density = 0;
  uint8_t  nr_of_rbs = 0;
  if (is_csi_rs_slot) { 
    AssertFatal(1==0,"Don't do SL CSI-RS for now\n");
    /*
    freq_density = ue->csirs_vars[0]->csirs_config_pdu.freq_density;
    nr_of_rbs = ue->csirs_vars[0]->csirs_config_pdu.nr_of_rbs;
    AssertFatal((freq_density == 1) || (nr_of_rbs > 0), "CSI-RS parameters are not properly configured\n");
*/
    }
  int harq_pid = slsch_pdu->harq_pid;
  uint16_t nb_re_dmrs;
  uint16_t start_symbol = 1;
  uint16_t number_symbols = pssch_pdu->pssch_numsym;
  ue->slsch[SLSCH_id].harq_process->harq_to_be_cleared=true;
  uint8_t number_dmrs_symbols = 0;
  for (int l = start_symbol; l < start_symbol + number_symbols; l++)
    number_dmrs_symbols += ((pssch_pdu->dmrs_symbol_position)>>l)&0x01;

  nb_re_dmrs = 6;

  uint32_t rb_size                   = pssch_pdu->num_subch*pssch_pdu->subchannel_size;
  int sci1_dmrs_overlap = pssch_pdu->dmrs_symbol_position & dmrs_pscch_mask[pssch_pdu->pscch_numsym-2];
  int sci2_re = get_NREsci2(pssch_pdu->sci2_alpha_times_100,
                            pssch_pdu->sci2_len,
                            pssch_pdu->sci2_beta_offset,
                            pssch_pdu->pssch_numsym,
                            pssch_pdu->pscch_numsym,
                            pssch_pdu->pscch_numrbs,
                            pssch_pdu->l_subch,
                            pssch_pdu->subchannel_size,
                            pssch_pdu->targetCodeRate);

  uint8_t nr_rbs_w_csi_rs = nr_of_rbs / freq_density;
  uint8_t subcarriers_used = get_nrUE_params()->nb_antennas_tx > 2 ? 2 : get_nrUE_params()->nb_antennas_tx;
  int num_CSI_REs = is_csi_rs_slot ? nr_rbs_w_csi_rs * subcarriers_used : 0;
  uint16_t sci1_re = pssch_pdu->pscch_numsym * pssch_pdu->pscch_numrbs * NR_NB_SC_PER_RB;
  uint32_t G = nr_get_G_SL(rb_size,
                           number_symbols,
                           nb_re_dmrs,
                           number_dmrs_symbols, // number of dmrs symbols irrespective of single or double symbol dmrs
                           sci1_dmrs_overlap,
                           sci1_re,
                           pssch_pdu->pscch_numrbs,
                           sci2_re,
                           num_CSI_REs,
                           pssch_pdu->mod_order,
                           pssch_pdu->num_layers);

  AssertFatal(G>0,"G is 0 : rb_size %u, number_symbols %d, nb_re_dmrs %d, number_dmrs_symbols %d, qam_mod_order %u, nrOfLayer %u\n",
              rb_size,
              number_symbols,
              nb_re_dmrs,
              number_dmrs_symbols, // number of dmrs symbols irrespective of single or double symbol dmrs
              pssch_pdu->mod_order,
              pssch_pdu->num_layers);
  LOG_D(NR_PHY,"slot %d rb_size %d, number_symbols %d, nb_re_dmrs %d, dmrs symbol positions %d, number_dmrs_symbols %d, qam_mod_order %d, nrOfLayer %d\n",
        slot_rx,
        rb_size,
        number_symbols,
        nb_re_dmrs,
        pssch_pdu->dmrs_symbol_position,
        number_dmrs_symbols, // number of dmrs symbols irrespective of single or double symbol dmrs
        pssch_pdu->mod_order,
        pssch_pdu->num_layers);

  //----------------------------------------------------------
  //--------------------- SLSCH decoding ---------------------
  //----------------------------------------------------------


  nfapi_nr_pusch_pdu_t pusch_pdu;

  pusch_pdu.rb_size = rb_size;
  pusch_pdu.qam_mod_order = pssch_pdu->mod_order;
  pusch_pdu.mcs_index = slsch_pdu->mcs;
  pusch_pdu.nrOfLayers = pssch_pdu->num_layers;
  pusch_pdu.pusch_data.tb_size=slsch_pdu->tb_size;
  uint32_t A = slsch_pdu->tb_size<<3;
  pusch_pdu.target_code_rate=slsch_pdu->target_coderate;
  float Coderate = (float) (slsch_pdu->target_coderate) / 10240.0f;
  pusch_pdu.pusch_data.rv_index=slsch_pdu->rv_index;
  
  if ((A <=292) || ((A<=3824) && (Coderate <= 0.6667)) || Coderate <= 0.25){
    pusch_pdu.maintenance_parms_v3.ldpcBaseGraph=2;
  }
  else{
    pusch_pdu.maintenance_parms_v3.ldpcBaseGraph=1;
  }
  pusch_pdu.maintenance_parms_v3.tbSizeLbrmBytes=slsch_pdu->tbslbrm>>3;

  LOG_D(NR_PHY, "%4d.%2d Calling nr_slsch_decoding\n", frame_rx, slot_rx);
  int nbDecode =
      nr_slsch_decoding(ue, SLSCH_id, ue->pssch_vars[SLSCH_id].llr, fp, &pusch_pdu, frame_rx, slot_rx, harq_pid, G, proc, phy_data, ack_nack_rcvd, num_acks);


  sl_nr_rx_indication_t sl_rx_indication;       
  nr_sidelink_indication_t sl_indication;       
  slsch_status_t slsch_status;
  ue->slsch[SLSCH_id].active = false;
  NR_UL_gNB_HARQ_t *harq_process = &ue->slsch[SLSCH_id].harq_process[harq_pid]; 
  slsch_status.b = harq_process->b;
  slsch_status.TBS = slsch_pdu->tb_size;
  slsch_status.harq_pid =  harq_pid;
  slsch_status.rxok = nbDecode>0 ? true : false;
  LOG_D(NR_PHY, "%4d.%2d SLSCH %s received ok \n", proc->frame_rx, proc->nr_slot_rx,nbDecode>0 ? "" : "not");
  sl_rx_indication.sfn = proc->frame_rx;
  sl_rx_indication.slot = proc->nr_slot_rx;
  sl_rx_indication.rx_indication_body[0].rx_slsch_pdu.ack_nack_rcvd = (uint8_t*)calloc(num_acks, sizeof(uint8_t));
  memcpy((void*)sl_rx_indication.rx_indication_body[0].rx_slsch_pdu.ack_nack_rcvd, (void*)ack_nack_rcvd,
         num_acks * sizeof(uint8_t));
  sl_rx_indication.rx_indication_body[0].rx_slsch_pdu.num_acks_rcvd = num_acks;
  uint8_t pdu_type = phy_data->sl_rx_action == SL_NR_CONFIG_TYPE_RX_PSSCH_SLSCH_PSFCH ? SL_NR_RX_PDU_TYPE_SLSCH_PSFCH : SL_NR_RX_PDU_TYPE_SLSCH;

  nr_fill_sl_rx_indication(&sl_rx_indication, pdu_type, ue, 1, (void*)&slsch_status, 0);
  nr_fill_sl_indication(&sl_indication,&sl_rx_indication,NULL,proc,ue,phy_data);
  if (ue->if_inst && ue->if_inst->sl_indication)
    ue->if_inst->sl_indication(&sl_indication);

  return nbDecode;
}

int psbch_pscch_pssch_processing(PHY_VARS_NR_UE *ue, const UE_nr_rxtx_proc_t *proc, nr_phy_data_t *phy_data)
{
  int frame_rx = proc->frame_rx;
  int nr_slot_rx = proc->nr_slot_rx;
  sl_nr_ue_phy_params_t *sl_phy_params = &ue->SL_UE_PHY_PARAMS;
  NR_DL_FRAME_PARMS *fp = &sl_phy_params->sl_frame_params;
  bool is_csi_rs_slot = false;
  int8_t *ack_nack_rcvd = NULL;

  int sampleShift = INT_MAX;

  start_meas(&sl_phy_params->phy_proc_sl_rx);
  
  LOG_D(NR_PHY, " ****** Sidelink RX-Chain for Frame.Slot %d.%d ******  \n", frame_rx % 1024, nr_slot_rx);

  const uint32_t rxdataF_sz = fp->samples_per_slot_wCP;
  __attribute__((aligned(32))) c16_t rxdataF[fp->nb_antennas_rx][rxdataF_sz];

  if ((frame_rx&127) == 0) {
      LOG_I(NR_PHY,"============================================\n");

      LOG_A(NR_PHY,"[UE%d] %d:%d PSBCH Stats: TX %u, RX ok %u, RX not ok %u\n",
                                                      ue->Mod_id, frame_rx, nr_slot_rx,
                                                      sl_phy_params->psbch.num_psbch_tx,
                                                      sl_phy_params->psbch.rx_ok,
                                                      sl_phy_params->psbch.rx_errors);

      LOG_A(NR_PHY,"[UE%d] %d:%d PSCCH Stats: TX %u, RX ok %u\n",
                                                      ue->Mod_id, frame_rx, nr_slot_rx,
                                                      sl_phy_params->pscch.num_pscch_tx,
                                                      sl_phy_params->pscch.rx_ok);

      LOG_A(NR_PHY,"[UE%d] %d:%d PSSCH/SCI2 Stats: TX %u, RX ok %u, RX not ok %u\n",
                                                      ue->Mod_id, frame_rx, nr_slot_rx,
                                                      sl_phy_params->pssch.num_pssch_sci2_tx,
                                                      sl_phy_params->pssch.rx_sci2_ok,
                                                      sl_phy_params->pssch.rx_sci2_errors);
      LOG_A(NR_PHY,"[UE%d] %d:%d PSSCH Stats: TX %u, RX ok %u, RX not ok (%u/%u/%u/%u)\n",
                                                      ue->Mod_id, frame_rx, nr_slot_rx,
                                                      sl_phy_params->pssch.num_pssch_tx,
                                                      sl_phy_params->pssch.rx_ok,
                                                      sl_phy_params->pssch.rx_errors[0],
                                                      sl_phy_params->pssch.rx_errors[1],
                                                      sl_phy_params->pssch.rx_errors[2],
                                                      sl_phy_params->pssch.rx_errors[3]);
      LOG_A(NR_PHY, "[UE%d] %d:%d PSFCH Stats: TX %u\n",
                                                      ue->Mod_id, frame_rx, nr_slot_rx,
                                                      sl_phy_params->psfch.num_psfch_tx
                                                      );
      LOG_I(NR_PHY,"============================================\n");
  }
    
  if (phy_data->sl_rx_action == SL_NR_CONFIG_TYPE_RX_PSBCH) {
    LOG_D(NR_PHY, " ----- PSBCH RX TTI: frame.slot %d.%d ------  \n", frame_rx % 1024, nr_slot_rx);

    __attribute__((aligned(32))) struct complex16 dl_ch_estimates_time[fp->nb_antennas_rx][fp->ofdm_symbol_size];

    int16_t psbch_e_rx[SL_NR_POLAR_PSBCH_E_NORMAL_CP + 2] = {0};
    int16_t psbch_unClippled[SL_NR_POLAR_PSBCH_E_NORMAL_CP + 2] = {0};
    int e_rx_offset = 0;
    /* TODO: Remove loop over symbols in later commit. */
    for (int sym = 0; sym < NR_SYMBOLS_PER_SLOT; sym++) {
      nr_slot_fep(ue, fp, proc->nr_slot_rx, sym, rxdataF, link_type_sl, 0, ue->common_vars.rxdata);
      __attribute__((aligned(32))) c16_t rxdataF_symb[fp->nb_antennas_rx][fp->ofdm_symbol_size];
      for (int aarx = 0; aarx < fp->nb_antennas_rx; aarx++) {
        /* TODO: Remove this buffer reshaping in later commit after rxdataF is in right format */
        memcpy(rxdataF_symb[aarx], &rxdataF[aarx][sym * fp->ofdm_symbol_size], sizeof(c16_t) * fp->ofdm_symbol_size);
      }
      sampleShift =
          nr_psbch_process(ue, phy_data, proc, sym, rxdataF_symb, &e_rx_offset, psbch_e_rx, psbch_unClippled, dl_ch_estimates_time);
    }

    if (frame_rx % 64 == 0) {
      LOG_I(NR_PHY, "============================================\n");

      LOG_I(NR_PHY,
            "[UE%d] %d:%d PSBCH Stats: TX %d, RX ok %d, RX not ok %d\n",
            ue->Mod_id,
            frame_rx,
            nr_slot_rx,
            sl_phy_params->psbch.num_psbch_tx,
            sl_phy_params->psbch.rx_ok,
            sl_phy_params->psbch.rx_errors);

      LOG_I(NR_PHY, "============================================\n");
    }
  }
  else if (phy_data->sl_rx_action == SL_NR_CONFIG_TYPE_RX_PSCCH){

    fapi_nr_dl_config_dci_dl_pdu_rel15_t *rel15 = &phy_data->phy_pdcch_config.pdcch_config[0];
    LOG_D(NR_PHY,"pscch_numsym = %d\n",phy_data->nr_sl_pscch_pdu.pscch_numsym);
    LOG_D(NR_PHY,"pscch_startrb = %d\n",phy_data->nr_sl_pscch_pdu.pscch_startrb);
    LOG_D(NR_PHY,"pscch_numrbs = %d\n",phy_data->nr_sl_pscch_pdu.pscch_numrbs);
    LOG_D(NR_PHY,"pscch_dmrs_scrambling_id = %d\n",phy_data->nr_sl_pscch_pdu.pscch_dmrs_scrambling_id);

    LOG_D(NR_PHY,"pscch_num_subch= %d\n",phy_data->nr_sl_pscch_pdu.num_subch);
    LOG_D(NR_PHY,"pscch_subchannel_size = %d\n",phy_data->nr_sl_pscch_pdu.subchannel_size);
    LOG_D(NR_PHY,"pscch_l_subch = %d\n",phy_data->nr_sl_pscch_pdu.l_subch);
    LOG_D(NR_PHY,"pscch_pssch_numsym = %d\n",phy_data->nr_sl_pscch_pdu.pssch_numsym);
    LOG_D(NR_PHY,"sense_pscch = %d\n",phy_data->nr_sl_pscch_pdu.sense_pscch);

    rel15->rnti = 0;
    rel15->BWPSize = phy_data->nr_sl_pscch_pdu.num_subch * phy_data->nr_sl_pscch_pdu.subchannel_size;
    rel15->BWPStart = phy_data->nr_sl_pscch_pdu.pscch_startrb;
    rel15->SubcarrierSpacing = fp->subcarrier_spacing;
    rel15->coreset.frequency_domain_resource[0] = phy_data->nr_sl_pscch_pdu.pscch_startrb;
    rel15->coreset.frequency_domain_resource[1] = phy_data->nr_sl_pscch_pdu.pscch_numrbs;
    rel15->coreset.CoreSetType = NFAPI_NR_CSET_CONFIG_PDCCH_CONFIG;
    rel15->coreset.StartSymbolIndex = 1;
    rel15->coreset.RegBundleSize = 0;
    rel15->coreset.duration = phy_data->nr_sl_pscch_pdu.pscch_numsym;
    rel15->coreset.pdcch_dmrs_scrambling_id = phy_data->nr_sl_pscch_pdu.pscch_dmrs_scrambling_id;
    rel15->coreset.scrambling_rnti = 1010;
    rel15->coreset.tci_present_in_dci = 0;

    rel15->number_of_candidates = phy_data->nr_sl_pscch_pdu.l_subch;
    rel15->num_dci_options = 1;
    rel15->dci_length_options[0] = phy_data->nr_sl_pscch_pdu.sci_1a_length;
    // L now provides the number of PRBs used by PSCCH instead of the number of CCEs
    rel15->L[0] = phy_data->nr_sl_pscch_pdu.pscch_numrbs * phy_data->nr_sl_pscch_pdu.pscch_numsym;
    // This provides the offset of the candidate of PSCCH in RBs instead of CCEs
    rel15->CCE[0] = 0;
    phy_data->phy_pdcch_config.nb_search_space = 1;
    pdcch_processing(ue, proc, phy_data,1);

    LOG_D(NR_PHY,"returned from pscch processing\n");
  }
  else if (phy_data->sl_rx_action == SL_NR_CONFIG_TYPE_RX_PSSCH_SCI) {
    sl_nr_rx_config_pssch_sci_pdu_t *pssch_pdu = &phy_data->nr_sl_pssch_sci_pdu;
    LOG_D(NR_PHY,"sci2_len = %d\n",pssch_pdu->sci2_len);
    LOG_D(NR_PHY,"sci2_beta_offset = %d\n",pssch_pdu->sci2_beta_offset);
    LOG_D(NR_PHY,"sci2_alpha_times_100= %d\n",pssch_pdu->sci2_alpha_times_100);
    LOG_D(NR_PHY,"pssch_targetCodeRate = %d\n",pssch_pdu->targetCodeRate);
    LOG_D(NR_PHY,"pssch_num_layers = %d\n",pssch_pdu->num_layers);
    LOG_D(NR_PHY,"dmrs_symbol_position = %d\n",pssch_pdu->dmrs_symbol_position);
    int num_dmrs = 0;
    for (int s = 0; s < pssch_pdu->pssch_numsym; s++)
      num_dmrs += (pssch_pdu->dmrs_symbol_position >> s) & 1;
    LOG_D(NR_PHY,"num_dmrs = %d\n",num_dmrs);
    LOG_D(NR_PHY,"Nid = %x\n",pssch_pdu->Nid);

    LOG_D(NR_PHY,"startrb = %d\n",pssch_pdu->startrb);
    LOG_D(NR_PHY,"pscch_numsym = %d\n",pssch_pdu->pscch_numsym);
    LOG_D(NR_PHY,"pscch_numrbs = %d\n",pssch_pdu->pscch_numrbs);
    LOG_D(NR_PHY,"num_subch= %d\n",pssch_pdu->num_subch);
    LOG_D(NR_PHY,"subchannel_size = %d\n",pssch_pdu->subchannel_size);
    LOG_D(NR_PHY,"l_subch = %d\n",pssch_pdu->l_subch);
    LOG_D(NR_PHY,"pssch_numsym = %d\n",pssch_pdu->pssch_numsym);
    LOG_D(NR_PHY,"sense_pssch = %d\n",pssch_pdu->sense_pssch);
    ue->slsch->harq_process->pssch_pdu = &phy_data->nr_sl_pssch_sci_pdu;
    uint8_t  freq_density = 0;
    uint8_t  nr_of_rbs = 0;
    if (is_csi_rs_slot) {
      AssertFatal(1==0,"Don't do SL CSIRS for now\n");
      /*
      freq_density = ue->csirs_vars[0]->csirs_config_pdu.freq_density;
      nr_of_rbs = ue->csirs_vars[0]->csirs_config_pdu.nr_of_rbs;
      AssertFatal((freq_density == 1) || (nr_of_rbs > 0), "CSI-RS parameters are not properly configured\n");
*/
      }    
    int sci2_re = get_NREsci2(pssch_pdu->sci2_alpha_times_100,
                              pssch_pdu->sci2_len,
                              pssch_pdu->sci2_beta_offset,
                              pssch_pdu->pssch_numsym,
                              pssch_pdu->pscch_numsym,
                              pssch_pdu->pscch_numrbs,
                              pssch_pdu->l_subch,
                              pssch_pdu->subchannel_size,
                              pssch_pdu->targetCodeRate);
    int nb_re_dmrs = 6;

    uint32_t rb_size                   = pssch_pdu->num_subch*pssch_pdu->subchannel_size;
    int sci1_dmrs_overlap = pssch_pdu->dmrs_symbol_position & dmrs_pscch_mask[pssch_pdu->pscch_numsym-2];

    uint8_t nr_rbs_w_csi_rs = nr_of_rbs / freq_density;
    uint8_t subcarriers_used = get_nrUE_params()->nb_antennas_tx > 2 ? 2 : get_nrUE_params()->nb_antennas_tx;
    int num_CSI_REs = is_csi_rs_slot ? nr_rbs_w_csi_rs * subcarriers_used : 0;
    uint16_t sci1_re = pssch_pdu->pscch_numsym * pssch_pdu->pscch_numrbs * NR_NB_SC_PER_RB;    
    uint16_t start_symbol = 1;
    uint16_t number_symbols = pssch_pdu->pssch_numsym;
    uint8_t number_dmrs_symbols = 0;
    for (int l = start_symbol; l < start_symbol + number_symbols; l++)
      number_dmrs_symbols += ((pssch_pdu->dmrs_symbol_position)>>l)&0x01;
    uint32_t G = nr_get_G_SL(rb_size,
                             pssch_pdu->pssch_numsym,
                             num_dmrs,
                             number_dmrs_symbols, // number of dmrs symbols irrespective of single or double symbol dmrs
                             sci1_dmrs_overlap,
                             sci1_re,
                             pssch_pdu->pscch_numrbs,
                             sci2_re,
                             num_CSI_REs,
                             pssch_pdu->mod_order,
                             pssch_pdu->num_layers);
    LOG_D(NR_PHY,"Starting slot FEP for SLSCH (symbol %d to %d) pscch_numsym %d pssch_numsym %d REs with SCI2 %d G %d\n",
          1 + pssch_pdu->pscch_numsym, pssch_pdu->pssch_numsym,
          pssch_pdu->pscch_numsym, pssch_pdu->pssch_numsym, sci2_re, G);
    for (int sym=1+pssch_pdu->pscch_numsym; sym<=pssch_pdu->pssch_numsym;sym++) {
      nr_slot_fep(ue, fp, proc->nr_slot_rx, sym, rxdataF, link_type_sl, 0, ue->common_vars.rxdata);
    }

    int16_t *llrs = (int16_t*)__builtin_alloca_with_align(G*sizeof(int16_t),64);
    nr_rx_pssch(ue,
                proc,
                phy_data,
                rxdataF_sz,
                rxdataF,
		llrs,
                0,
                frame_rx,
                nr_slot_rx,
                0,
                &is_csi_rs_slot);
    if (phy_data->sl_rx_action == SL_NR_CONFIG_TYPE_RX_PSSCH_SLSCH_PSFCH) {
      ack_nack_rcvd = calloc(phy_data->num_psfch_pdus, sizeof(ack_nack_rcvd));
      LOG_D(NR_PHY, "num_psfch_pdus: %d\n", phy_data->num_psfch_pdus);
      for (int k = 0; k < phy_data->num_psfch_pdus; k++) {
        sl_nr_tx_rx_config_psfch_pdu_t *psfch_pdu = &phy_data->psfch_pdu_list[k];
        LOG_D(NR_PHY, "%s start_symbol_index %d, sl_bwp_start %d, sequence_hop_flag %d, \
            second_hop_prb %d, prb %d, nr_of_symbols %d, initial_cyclic_shift %d, hopping_id %d, \
            group_hop_flag %d, freq_hop_flag %d, bit_len_harq %d\n",
            __FUNCTION__,
            psfch_pdu->start_symbol_index, psfch_pdu->sl_bwp_start,
            psfch_pdu->sequence_hop_flag, psfch_pdu->second_hop_prb, psfch_pdu->prb,
            psfch_pdu->nr_of_symbols, psfch_pdu->initial_cyclic_shift, psfch_pdu->hopping_id,
            psfch_pdu->group_hop_flag, psfch_pdu->freq_hop_flag, psfch_pdu->bit_len_harq);
        nr_slot_fep(ue, fp, proc->nr_slot_rx, psfch_pdu->start_symbol_index, rxdataF, link_type_sl, 0, ue->common_vars.rxdata);
        ack_nack_rcvd[k] = nr_ue_decode_psfch0(ue,
                                            frame_rx,
                                            nr_slot_rx,
                                            rxdataF,
                                            psfch_pdu);
      }
      free(phy_data->psfch_pdu_list);
      phy_data->psfch_pdu_list = NULL;
    }
    NR_gNB_PUSCH *pssch_vars = &ue->pssch_vars[0];
    pssch_vars->ulsch_power_tot = 0;
    pssch_vars->ulsch_noise_power_tot = 0;
    for (int aarx = 0; aarx < fp->nb_antennas_rx; aarx++) {
      pssch_vars->ulsch_power[aarx] /= num_dmrs;
      pssch_vars->ulsch_power_tot += pssch_vars->ulsch_power[aarx];
      pssch_vars->ulsch_noise_power[aarx] /= num_dmrs;
      pssch_vars->ulsch_noise_power_tot += pssch_vars->ulsch_noise_power[aarx];
    }
    if (dB_fixed_x10(pssch_vars->ulsch_power_tot) < dB_fixed_x10(pssch_vars->ulsch_noise_power_tot) + ue->pssch_thres) {

      LOG_D(NR_PHY,
            "PSSCH not detected in %d.%d (%d,%d,%d)\n",
            frame_rx,
            nr_slot_rx,
            dB_fixed_x10(pssch_vars->ulsch_power_tot),
            dB_fixed_x10(pssch_vars->ulsch_noise_power_tot),
            ue->pssch_thres);
      pssch_vars->ulsch_power_tot = pssch_vars->ulsch_noise_power_tot;
      pssch_vars->DTX = 1;
      //if (stats)
      //  stats->ulsch_stats.DTX++;
      // nr_fill_indication(gNB, frame_rx, slot_rx, ULSCH_id, ulsch->harq_pid, 1, 1);
      //pssch_DTX++;
      //  continue;
    } else {
      pssch_vars->DTX = 0;
      int totalDecode = nr_slsch_procedures(ue, frame_rx, nr_slot_rx, 0, proc, phy_data, is_csi_rs_slot, ack_nack_rcvd, phy_data->num_psfch_pdus);
      LOG_D(NR_PHY,
            "Total %d decoded PSSCH detected in %d.%d (%d,%d,%d)\n",
            totalDecode,
            frame_rx,
            nr_slot_rx,
            dB_fixed_x10(pssch_vars->ulsch_power_tot),
            dB_fixed_x10(pssch_vars->ulsch_noise_power_tot),
            ue->pssch_thres);
    }
  }
  UEscopeCopy(ue, commonRxdataF, rxdataF, sizeof(int32_t), fp->nb_antennas_rx, rxdataF_sz, 0);

  return sampleShift;
}

void phy_procedures_nrUE_SL_TX(PHY_VARS_NR_UE *ue, const UE_nr_rxtx_proc_t *proc, nr_phy_data_tx_t *phy_data, c16_t **txp)
{
  int slot_tx = proc->nr_slot_tx;
  int frame_tx = proc->frame_tx;
  int tx_action = 0;
  const char *sl_tx_actions[] = {"PSBCH", "PSCCH_PSSCH", "PSCCH_PSSCH_PSFCH", "PSCCH_PSSCH_CSI_RS"};
  if (phy_data->sl_tx_action == SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH_CSI_RS) {
    LOG_D(NR_PHY, "Generating %s (%d.%d)\n", sl_tx_actions[phy_data->sl_tx_action - SL_NR_CONFIG_TYPE_TX_PSBCH], frame_tx, slot_tx);
  }

  sl_nr_ue_phy_params_t *sl_phy_params = &ue->SL_UE_PHY_PARAMS;
  NR_DL_FRAME_PARMS *fp = &sl_phy_params->sl_frame_params;

  const int samplesF_per_slot = NR_SYMBOLS_PER_SLOT * fp->ofdm_symbol_size;
  c16_t txdataF_buf[fp->nb_antennas_tx * samplesF_per_slot] __attribute__((aligned(32)));
  memset(txdataF_buf, 0, sizeof(txdataF_buf));
  c16_t *txdataF[fp->nb_antennas_tx]; /* workaround to be compatible with current txdataF usage in all tx procedures. */
  for (int i = 0; i < fp->nb_antennas_tx; ++i)
    txdataF[i] = &txdataF_buf[i * samplesF_per_slot];

  LOG_D(NR_PHY, "****** start Sidelink TX-Chain for AbsSubframe %d.%d ******\n", frame_tx, slot_tx);

  start_meas(&sl_phy_params->phy_proc_sl_tx);

  if (phy_data->sl_tx_action == SL_NR_CONFIG_TYPE_TX_PSBCH) {
    sl_nr_tx_config_psbch_pdu_t *psbch_vars = &phy_data->psbch_vars;
    nr_tx_psbch(ue, frame_tx, slot_tx, psbch_vars, txdataF);
    sl_phy_params->psbch.num_psbch_tx++;

    if (frame_tx % 64 == 0) {
      LOG_I(NR_PHY, "============================================\n");

      LOG_I(NR_PHY,
            "[UE%d] %d:%d PSBCH Stats: TX %d, RX ok %d, RX not ok %d\n",
            ue->Mod_id,
            frame_tx,
            slot_tx,
            sl_phy_params->psbch.num_psbch_tx,
            sl_phy_params->psbch.rx_ok,
            sl_phy_params->psbch.rx_errors);

      LOG_I(NR_PHY, "============================================\n");
    }
    tx_action = 1;
  }
  else if (phy_data->sl_tx_action == SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH ||
           phy_data->sl_tx_action == SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH_CSI_RS ||
           phy_data->sl_tx_action == SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH_PSFCH) {
    if (phy_data->sl_tx_action >= SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH && phy_data->sl_tx_action <= SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH_CSI_RS)
      LOG_D(NR_PHY, "(%d.%d) Sending %s\n", frame_tx, slot_tx, sl_tx_actions[phy_data->sl_tx_action - SL_NR_CONFIG_TYPE_TX_PSBCH]);
    phy_data->pscch_Nid = nr_generate_sci1(ue, txdataF[0], fp, AMP, slot_tx, &phy_data->nr_sl_pssch_pscch_pdu) &0xFFFF;
    nfapi_nr_dl_tti_csi_rs_pdu_rel15_t *csi_params = (nfapi_nr_dl_tti_csi_rs_pdu_rel15_t *)&phy_data->nr_sl_pssch_pscch_pdu.nr_sl_csi_rs_pdu;
    csi_params->scramb_id = phy_data->pscch_Nid % (1 << 10);
    nr_ue_slsch_procedures(ue, phy_data->nr_sl_pssch_pscch_pdu.harq_pid, frame_tx, slot_tx, phy_data, txdataF);

    sl_phy_params->pscch.num_pscch_tx ++;
    sl_phy_params->pssch.num_pssch_sci2_tx ++;
    sl_phy_params->pssch.num_pssch_tx ++;
    if (phy_data->sl_tx_action == SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH_CSI_RS) {
      uint16_t beta_csirs = get_softmodem_params()->sl_mode ? (uint16_t)(AMP * (ceil(sqrt(phy_data->nr_sl_pssch_pscch_pdu.num_layers / fp->nb_antennas_tx)))) & 0xFFFF : AMP;
      LOG_D(NR_PHY, "Tx beta_csirs: %d, scramb_id %i (%d.%d)\n", beta_csirs, csi_params->scramb_id, frame_tx, slot_tx);
      AssertFatal(false, "CSI_RS not supported\n");
      //nr_generate_csi_rs(fp,
      //                   (int32_t **)txdataF,
      //                   beta_csirs,
      //                   ue->nr_csi_info,
      //                   csi_params,
      //                   slot_tx,
      //                   NULL,
      //                   NULL,
      //                   NULL,
      //                   NULL,
      //                   NULL,
      //                   NULL,
      //                   NULL,
      //                   NULL);
    }
    if (phy_data->sl_tx_action == SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH_PSFCH) {
      for (int k = 0; k < phy_data->nr_sl_pssch_pscch_pdu.num_psfch_pdus; k++) {
        nr_generate_psfch0(ue,
                          txdataF,
                          fp,
                          AMP,
                          slot_tx,
                          &phy_data->nr_sl_pssch_pscch_pdu.psfch_pdu_list[k]);
      }
      sl_phy_params->psfch.num_psfch_tx ++;
      free(phy_data->nr_sl_pssch_pscch_pdu.psfch_pdu_list);
      phy_data->nr_sl_pssch_pscch_pdu.psfch_pdu_list = NULL;
    }
    tx_action = 1;
  }
    
  bool was_symbol_used[NR_SYMBOLS_PER_SLOT];
  for (int i = 0; i < 14; i++)
    was_symbol_used[i] = true;
  if (tx_action) {
    LOG_D(NR_PHY, "Sending Uplink data \n");
    nr_tx_rotation_and_ofdm_mod(proc->nr_slot_tx,
                                fp,
                                fp->nb_antennas_tx,
                                txdataF,
                                txp,
                                link_type_sl,
                                was_symbol_used,
                                ue->no_phase_pre_comp);
  }

  LOG_D(NR_PHY, "****** end Sidelink TX-Chain for AbsSubframe %d.%d ******\n", frame_tx, slot_tx);
  stop_meas(&sl_phy_params->phy_proc_sl_tx);
}

