/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "PHY/defs_UE.h"
#include "modulation_UE.h"
#include "PHY/LTE_ESTIMATION/lte_estimation.h"

//#define DEBUG_FEP

#define SOFFSET 0

int slot_fep(PHY_VARS_UE *ue,
             unsigned char l,
             unsigned char Ns,
             int sample_offset,
             int no_prefix,
             int reset_freq_est) {
  LTE_DL_FRAME_PARMS *frame_parms = &ue->frame_parms;
  LTE_UE_COMMON *common_vars   = &ue->common_vars;
  uint8_t eNB_id = 0;//ue_common_vars->eNb_id;
  unsigned char aa;
  unsigned char symbol = l+((7-frame_parms->Ncp)*(Ns&1)); ///symbol within sub-frame
  unsigned int nb_prefix_samples = (no_prefix ? 0 : frame_parms->nb_prefix_samples);
  unsigned int nb_prefix_samples0 = (no_prefix ? 0 : frame_parms->nb_prefix_samples0);
  unsigned int subframe_offset;//,subframe_offset_F;
  unsigned int slot_offset;
  int i;
  unsigned int frame_length_samples = frame_parms->samples_per_tti * 10;
  unsigned int rx_offset;
  /*LTE_UE_DLSCH_t **dlsch_ue = phy_vars_ue->dlsch_ue[eNB_id];
  unsigned char harq_pid = dlsch_ue[0]->current_harq_pid;
  LTE_DL_UE_HARQ_t *dlsch0_harq = dlsch_ue[0]->harq_processes[harq_pid];
  int uespec_pilot[9][1200];*/
  int tmp_dft_in[2048] __attribute__ ((aligned (32)));  // This is for misalignment issues for 6 and 15 PRBs
  int s = frame_parms->ofdm_symbol_size;
  if (s != 128 && s != 256 && s != 512 && s != 1024 && s != 1536 && s != 2048)
    s = 512;
  const dft_size_idx_t dftsizeidx = get_dft(s);
  if (no_prefix) {
    subframe_offset = frame_parms->ofdm_symbol_size * frame_parms->symbols_per_tti * (Ns>>1);
    slot_offset = frame_parms->ofdm_symbol_size * (frame_parms->symbols_per_tti>>1) * (Ns%2);
  } else {
    subframe_offset = frame_parms->samples_per_tti * (Ns>>1);
    slot_offset = (frame_parms->samples_per_tti>>1) * (Ns%2);
  }

  //  subframe_offset_F = frame_parms->ofdm_symbol_size * frame_parms->symbols_per_tti * (Ns>>1);
 
  if (l<0 || l>=7-frame_parms->Ncp) {
    printf("slot_fep: l must be between 0 and %d\n",7-frame_parms->Ncp);
    return(-1);
  }

  if (Ns<0 || Ns>=20) {
    printf("slot_fep: Ns must be between 0 and 19\n");
    return(-1);
  }

  for (aa=0; aa<frame_parms->nb_antennas_rx; aa++) {
    memset(&common_vars->common_vars_rx_data_per_thread[ue->current_thread_id[Ns>>1]].rxdataF[aa][frame_parms->ofdm_symbol_size*symbol],0,frame_parms->ofdm_symbol_size*sizeof(int));
    rx_offset = sample_offset + slot_offset + nb_prefix_samples0 + subframe_offset - SOFFSET;
    // Align with 256 bit
    //    rx_offset = rx_offset&0xfffffff8;

    if (l==0) {
      if (rx_offset > (frame_length_samples - frame_parms->ofdm_symbol_size))
        memcpy((short *)&common_vars->rxdata[aa][frame_length_samples],
               (short *)&common_vars->rxdata[aa][0],
               frame_parms->ofdm_symbol_size*sizeof(int));

      if ((rx_offset&7)!=0) {  // if input to dft is not 256-bit aligned, issue for size 6,15 and 25 PRBs
        memcpy((void *)tmp_dft_in,
               (void *)&common_vars->rxdata[aa][rx_offset % frame_length_samples],
               frame_parms->ofdm_symbol_size*sizeof(int));
        dft(dftsizeidx,(int16_t *)tmp_dft_in,
            (int16_t *)&common_vars->common_vars_rx_data_per_thread[ue->current_thread_id[Ns>>1]].rxdataF[aa][frame_parms->ofdm_symbol_size*symbol],1);
      } else { // use dft input from RX buffer directly
        start_UE_TIMING(ue->rx_dft_stats);
        dft(dftsizeidx,(int16_t *)&common_vars->rxdata[aa][(rx_offset) % frame_length_samples],
            (int16_t *)&common_vars->common_vars_rx_data_per_thread[ue->current_thread_id[Ns>>1]].rxdataF[aa][frame_parms->ofdm_symbol_size*symbol],1);
        stop_UE_TIMING(ue->rx_dft_stats);
      }
    } else {
      rx_offset += (frame_parms->ofdm_symbol_size+nb_prefix_samples)*l;// +
      //                   (frame_parms->ofdm_symbol_size+nb_prefix_samples)*(l-1);
#ifdef DEBUG_FEP
      //  if (ue->frame <100)
      LOG_D(PHY,"slot_fep: frame %d: slot %d, symbol %d, nb_prefix_samples %d, nb_prefix_samples0 %d, slot_offset %d, subframe_offset %d, sample_offset %d,rx_offset %d, frame_length_samples %d\n",
            ue->proc.proc_rxtx[(Ns>>1)&1].frame_rx,Ns, symbol,
            nb_prefix_samples,nb_prefix_samples0,slot_offset,subframe_offset,sample_offset,rx_offset,frame_length_samples);
#endif

      if (rx_offset > (frame_length_samples - frame_parms->ofdm_symbol_size))
        memcpy((void *)&common_vars->rxdata[aa][frame_length_samples],
               (void *)&common_vars->rxdata[aa][0],
               frame_parms->ofdm_symbol_size*sizeof(int));

      start_UE_TIMING(ue->rx_dft_stats);

      if ((rx_offset&7)!=0) {  // if input to dft is not 128-bit aligned, issue for size 6 and 15 PRBs
        memcpy((void *)tmp_dft_in,
               (void *)&common_vars->rxdata[aa][(rx_offset) % frame_length_samples],
               frame_parms->ofdm_symbol_size*sizeof(int));
        dft(dftsizeidx,(int16_t *)tmp_dft_in,
            (int16_t *)&common_vars->common_vars_rx_data_per_thread[ue->current_thread_id[Ns>>1]].rxdataF[aa][frame_parms->ofdm_symbol_size*symbol],1);
      } else { // use dft input from RX buffer directly
        dft(dftsizeidx,(int16_t *)&common_vars->rxdata[aa][(rx_offset) % frame_length_samples],
            (int16_t *)&common_vars->common_vars_rx_data_per_thread[ue->current_thread_id[Ns>>1]].rxdataF[aa][frame_parms->ofdm_symbol_size*symbol],1);
      }

      stop_UE_TIMING(ue->rx_dft_stats);
    }


#ifdef DEBUG_FEP
    //  if (ue->frame <100)
    printf("slot_fep: frame %d: symbol %d rx_offset %u\n", ue->proc.proc_rxtx[(Ns>>1)&1].frame_rx, symbol,rx_offset);
#endif
  }

  if (ue->perfect_ce == 0) {
    if ((l==0) || (l==(4-frame_parms->Ncp))) {
      for (aa=0; aa<frame_parms->nb_antenna_ports_eNB; aa++) {
#ifdef DEBUG_FEP
        printf("Channel estimation eNB %d, aatx %d, slot %d, symbol %d\n",eNB_id,aa,Ns,l);
#endif
        start_UE_TIMING(ue->dlsch_channel_estimation_stats);
        lte_dl_channel_estimation(ue,eNB_id,0,
                                  Ns,
                                  aa,
                                  l,
                                  symbol);
        stop_UE_TIMING(ue->dlsch_channel_estimation_stats);

        for (i=0; i<ue->measurements.n_adj_cells; i++) {
          lte_dl_channel_estimation(ue,eNB_id,i+1,
                                    Ns,
                                    aa,
                                    l,
                                    symbol);
        }
      }

      // do frequency offset estimation here!
      // use channel estimates from current symbol (=ch_t) and last symbol (ch_{t-1})
#ifdef DEBUG_FEP
      printf("Frequency offset estimation\n");
#endif

      if (l==(4-frame_parms->Ncp)) {
        start_UE_TIMING(ue->dlsch_freq_offset_estimation_stats);
        lte_est_freq_offset(common_vars->common_vars_rx_data_per_thread[ue->current_thread_id[Ns>>1]].dl_ch_estimates[0],
                            frame_parms,
                            l,
                            &common_vars->freq_offset,
                            reset_freq_est);
        stop_UE_TIMING(ue->dlsch_freq_offset_estimation_stats);
      }
    }
  }

#ifdef DEBUG_FEP
  printf("slot_fep: Ns %d l %d, done\n",Ns,l);
#endif
  return(0);
}

int front_end_fft(PHY_VARS_UE *ue,
                  unsigned char l,
                  unsigned char Ns,
                  int sample_offset,
                  int no_prefix) {
  LTE_DL_FRAME_PARMS *frame_parms = &ue->frame_parms;
  LTE_UE_COMMON *common_vars   = &ue->common_vars;
  unsigned char aa;
  unsigned char symbol = l+((7-frame_parms->Ncp)*(Ns&1)); ///symbol within sub-frame
  unsigned int nb_prefix_samples = (no_prefix ? 0 : frame_parms->nb_prefix_samples);
  unsigned int nb_prefix_samples0 = (no_prefix ? 0 : frame_parms->nb_prefix_samples0);
  unsigned int subframe_offset;//,subframe_offset_F;
  unsigned int slot_offset;
  unsigned int frame_length_samples = frame_parms->samples_per_tti * 10;
  unsigned int rx_offset;
  uint8_t  threadId;
  /*LTE_UE_DLSCH_t **dlsch_ue = phy_vars_ue->dlsch_ue[eNB_id];
  unsigned char harq_pid = dlsch_ue[0]->current_harq_pid;
  LTE_DL_UE_HARQ_t *dlsch0_harq = dlsch_ue[0]->harq_processes[harq_pid];
  int uespec_pilot[9][1200];*/
  int tmp_dft_in[2048] __attribute__ ((aligned (32)));  // This is for misalignment issues for 6 and 15 PRBs
  int s = frame_parms->ofdm_symbol_size;
  if (s != 128 && s != 256 && s != 512 && s != 1024 && s != 1536 && s != 2048)
    s = 512;
  dft_size_idx_t dftsizeidx = get_dft(s);

  if (no_prefix) {
    subframe_offset = frame_parms->ofdm_symbol_size * frame_parms->symbols_per_tti * (Ns>>1);
    slot_offset = frame_parms->ofdm_symbol_size * (frame_parms->symbols_per_tti>>1) * (Ns%2);
  } else {
    subframe_offset = frame_parms->samples_per_tti * (Ns>>1);
    slot_offset = (frame_parms->samples_per_tti>>1) * (Ns%2);
  }

  //  subframe_offset_F = frame_parms->ofdm_symbol_size * frame_parms->symbols_per_tti * (Ns>>1);

  if (l<0 || l>=7-frame_parms->Ncp) {
    printf("slot_fep: l must be between 0 and %d\n",7-frame_parms->Ncp);
    return(-1);
  }

  if (Ns<0 || Ns>=20) {
    printf("slot_fep: Ns must be between 0 and 19\n");
    return(-1);
  }

  threadId = ue->current_thread_id[Ns>>1];

  for (aa=0; aa<frame_parms->nb_antennas_rx; aa++) {
    // change thread index
    memset(&common_vars->common_vars_rx_data_per_thread[threadId].rxdataF[aa][frame_parms->ofdm_symbol_size*symbol],0,frame_parms->ofdm_symbol_size*sizeof(int));
    rx_offset = sample_offset + slot_offset + nb_prefix_samples0 + subframe_offset - SOFFSET;
    // Align with 256 bit
    //    rx_offset = rx_offset&0xfffffff8;

    if (l==0) {
      if (rx_offset > (frame_length_samples - frame_parms->ofdm_symbol_size))
        memcpy((short *)&common_vars->rxdata[aa][frame_length_samples],
               (short *)&common_vars->rxdata[aa][0],
               frame_parms->ofdm_symbol_size*sizeof(int));

      if ((rx_offset&7)!=0) {  // if input to dft is not 256-bit aligned, issue for size 6,15 and 25 PRBs
        memcpy((void *)tmp_dft_in,
               (void *)&common_vars->rxdata[aa][rx_offset % frame_length_samples],
               frame_parms->ofdm_symbol_size*sizeof(int));
        dft(dftsizeidx,(int16_t *)tmp_dft_in,
            (int16_t *)&common_vars->common_vars_rx_data_per_thread[threadId].rxdataF[aa][frame_parms->ofdm_symbol_size*symbol],1);
      } else { // use dft input from RX buffer directly
        start_meas(&ue->rx_dft_stats);
        dft(dftsizeidx,(int16_t *)&common_vars->rxdata[aa][(rx_offset) % frame_length_samples],
            (int16_t *)&common_vars->common_vars_rx_data_per_thread[threadId].rxdataF[aa][frame_parms->ofdm_symbol_size*symbol],1);
        stop_meas(&ue->rx_dft_stats);
      }
    } else {
      rx_offset += (frame_parms->ofdm_symbol_size+nb_prefix_samples)*l;// +
      //                   (frame_parms->ofdm_symbol_size+nb_prefix_samples)*(l-1);
#ifdef DEBUG_FEP
      //  if (ue->frame <100)
      LOG_D(PHY,
            "slot_fep: frame %d: slot %d, threadId %d, symbol %d, nb_prefix_samples %d, nb_prefix_samples0 %d, slot_offset %d, subframe_offset %d, sample_offset %d,rx_offset %d, frame_length_samples %d\n",
            ue->proc.proc_rxtx[threadId].frame_rx,Ns, threadId,symbol,
            nb_prefix_samples,nb_prefix_samples0,slot_offset,subframe_offset,sample_offset,rx_offset,frame_length_samples);
#endif

      if (rx_offset > (frame_length_samples - frame_parms->ofdm_symbol_size))
        memcpy((void *)&common_vars->rxdata[aa][frame_length_samples],
               (void *)&common_vars->rxdata[aa][0],
               frame_parms->ofdm_symbol_size*sizeof(int));

      start_meas(&ue->rx_dft_stats);

      if ((rx_offset&7)!=0) {  // if input to dft is not 128-bit aligned, issue for size 6 and 15 PRBs
        memcpy((void *)tmp_dft_in,
               (void *)&common_vars->rxdata[aa][(rx_offset) % frame_length_samples],
               frame_parms->ofdm_symbol_size*sizeof(int));
        dft(dftsizeidx,(int16_t *)tmp_dft_in,
            (int16_t *)&common_vars->common_vars_rx_data_per_thread[threadId].rxdataF[aa][frame_parms->ofdm_symbol_size*symbol],1);
      } else { // use dft input from RX buffer directly
        dft(dftsizeidx,(int16_t *)&common_vars->rxdata[aa][(rx_offset) % frame_length_samples],
            (int16_t *)&common_vars->common_vars_rx_data_per_thread[threadId].rxdataF[aa][frame_parms->ofdm_symbol_size*symbol],1);
      }

      stop_meas(&ue->rx_dft_stats);
    }

#ifdef DEBUG_FEP
    //  if (ue->frame <100)
    printf("slot_fep: frame %d: symbol %d rx_offset %u\n", ue->proc.proc_rxtx[threadId].frame_rx, symbol,rx_offset);
#endif

  }

  return(0);
}

int front_end_chanEst(PHY_VARS_UE *ue,
                      unsigned char l,
                      unsigned char Ns,
                      int reset_freq_est) {
  LTE_DL_FRAME_PARMS *frame_parms = &ue->frame_parms;
  LTE_UE_COMMON *common_vars   = &ue->common_vars;
  uint8_t eNB_id = 0;//ue_common_vars->eNb_id;
  unsigned char aa;
  unsigned char symbol = l+((7-frame_parms->Ncp)*(Ns&1)); ///symbol within sub-frame
  int i;

  /*LTE_UE_DLSCH_t **dlsch_ue = phy_vars_ue->dlsch_ue[eNB_id];
  unsigned char harq_pid = dlsch_ue[0]->current_harq_pid;
  LTE_DL_UE_HARQ_t *dlsch0_harq = dlsch_ue[0]->harq_processes[harq_pid];
  int uespec_pilot[9][1200];*/

  if (ue->perfect_ce == 0) {
    if ((l==0) || (l==(4-frame_parms->Ncp))) {
      for (aa=0; aa<frame_parms->nb_antenna_ports_eNB; aa++) {
#ifdef DEBUG_FEP
        printf("Channel estimation eNB %d, aatx %d, slot %d, symbol %d\n",eNB_id,aa,Ns,l);
#endif
        start_meas(&ue->dlsch_channel_estimation_stats);
        lte_dl_channel_estimation(ue,eNB_id,0,
                                  Ns,
                                  aa,
                                  l,
                                  symbol);
        stop_meas(&ue->dlsch_channel_estimation_stats);

        for (i=0; i<ue->measurements.n_adj_cells; i++) {
          lte_dl_channel_estimation(ue,eNB_id,i+1,
                                    Ns,
                                    aa,
                                    l,
                                    symbol);
        }
      }

      // do frequency offset estimation here!
      // use channel estimates from current symbol (=ch_t) and last symbol (ch_{t-1})
#ifdef DEBUG_FEP
      printf("Frequency offset estimation\n");
#endif

      if (l==(4-frame_parms->Ncp)) {
        start_meas(&ue->dlsch_freq_offset_estimation_stats);
        lte_est_freq_offset(common_vars->common_vars_rx_data_per_thread[(Ns>>1)&0x1].dl_ch_estimates[0],
                            frame_parms,
                            l,
                            &common_vars->freq_offset,
                            reset_freq_est);
        stop_meas(&ue->dlsch_freq_offset_estimation_stats);
      }
    }
  }

  return(0);
}
