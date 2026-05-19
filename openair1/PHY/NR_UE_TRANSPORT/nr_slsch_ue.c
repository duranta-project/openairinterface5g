/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Top-level routines for transmission of the PSSCH TS 38.211 v 16.3.0
 */
#include <stdint.h>
#include "PHY/gold.h"
#include "PHY/NR_REFSIG/dmrs_nr.h"
#include "PHY/NR_REFSIG/ptrs_nr.h"
#include "PHY/NR_REFSIG/sl_refsig_defs.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_ue.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_proto_ue.h"
#include "PHY/MODULATION/nr_modulation.h"
#include "PHY/MODULATION/modulation_common.h"
#include "common/utils/assertions.h"
#include "common/utils/nr/nr_common.h"
#include "common/utils/LOG/vcd_signal_dumper.h"
#include "PHY/NR_TRANSPORT/nr_transport_common_proto.h"
#include "PHY/NR_TRANSPORT/nr_sch_dmrs.h"
#include "PHY/NR_TRANSPORT/nr_dci.h"
#include "PHY/defs_nr_common.h"
#include "PHY/TOOLS/tools_defs.h"
#include "executables/nr-softmodem.h"
#include "executables/nr-uesoftmodem.h"
#include "executables/softmodem-common.h"
#include "PHY/NR_REFSIG/ul_ref_seq_nr.h"
#include <openair2/UTIL/OPT/opt.h>

//#define DEBUG_PUSCH_MAPPING
//#define DEBUG_MAC_PDU
//#define DEBUG_DFT_IDFT

//extern int32_t uplink_counter;

void nr_pssch_codeword_scrambling_sci(uint32_t *in,
                                      uint32_t size,
                                      uint32_t Nid,
                                      uint32_t* out)
{
  uint8_t reset, b_idx;
  uint32_t x1 = 0, x2 = 0, s = 0;

  reset = 1;
  x2 = (Nid<<15) + 1010;

  for (int i=0; i<size; i++) {
    b_idx = i&0x1f;
    if (b_idx==0) {
      s = gold_generic(&x1, &x2, reset);
      reset = 0;
      if (i)
        out++;
    }
    *out ^= (((in[i])&1) ^ ((s>>b_idx)&1))<<b_idx;
    //printf("i %d b_idx %d in %d s 0x%08x out 0x%08x\n", i, b_idx, in[i], s, *out);
  }
}
void nr_pssch_codeword_scrambling_sci_2layer(uint32_t *in,
                                             uint32_t size,
                                             uint32_t Nid,
                                             uint32_t* out)
{
  uint8_t reset, b_idx;
  uint32_t x1 = 0, x2 = 0, s = 0;

  reset = 1;
  x2 = (Nid<<15) + 1010;

  for (int i=0; i<size; i+=4) {
    b_idx = i&0x1f;
    if (b_idx==0) {
      s = gold_generic(&x1, &x2, reset);
      reset = 0;
      if (i)
        out++;
    }
    *out ^= (((in[i])&1) ^ ((s>>b_idx)&1))<<b_idx;
    *out ^= (((in[i+1])&1) ^ ((s>>(b_idx+1))&1))<<(b_idx+1);
    *out ^= (((in[i])&1) ^ ((s>>b_idx)&1))<<(b_idx+2);
    *out ^= (((in[i+1])&1) ^ ((s>>(b_idx+1))&1))<<(b_idx+3);
    //printf("i %d b_idx %d in %d s 0x%08x out 0x%08x\n", i, b_idx, in[i], s, *out);
  }
}

int dmrs_pscch_mask[2] = {7,15} ;

void nr_ue_slsch_procedures(PHY_VARS_NR_UE *UE,
                            const unsigned char harq_pid,
                            const uint32_t frame,
                            const uint8_t slot,
                            nr_phy_data_tx_t *phy_data,
                            c16_t **txdataF)
{
  LOG_D(PHY,"nr_ue_ulsch_procedures hard_id %d %d.%d\n",harq_pid,frame,slot);

  int Wf[2], Wt[2];
  int l_prime[2], delta;
  uint8_t nb_dmrs_re_per_rb;
  int i;
  int sample_offsetF, N_RE_prime;

  bool is_csi_rs_slot = false;
  if (phy_data->sl_tx_action == SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH_CSI_RS)
    is_csi_rs_slot = true;
  nfapi_nr_dl_tti_csi_rs_pdu_rel15_t *csi_params = is_csi_rs_slot ? (nfapi_nr_dl_tti_csi_rs_pdu_rel15_t *)&phy_data->nr_sl_pssch_pscch_pdu.nr_sl_csi_rs_pdu : NULL;
  if (csi_params)
    LOG_D(NR_PHY, "Tx start_rb %i, cdm_type %i, csi_type %i, freq_density %i, nr_of_rbs %i, row %i symb_l0 %i is_csi_rs_slot %i\n",
          csi_params->start_rb, csi_params->cdm_type, csi_params->csi_type, csi_params->freq_density, csi_params->nr_of_rbs, csi_params->row, csi_params->symb_l0, is_csi_rs_slot);
  int      N_PRB_oh = 0; // higher layer (RRC) parameter xOverhead in PUSCH-ServingCellConfig
  uint16_t number_dmrs_symbols = 0;

  NR_UE_ULSCH_t *ulsch_ue = &phy_data->ulsch;
  sl_nr_tx_config_pscch_pssch_pdu_t *pscch_pssch_pdu = &phy_data->nr_sl_pssch_pscch_pdu;
  NR_UL_UE_HARQ_t *harq_process_ul_ue = &UE->sl_harq_processes[harq_pid]; 

  NR_DL_FRAME_PARMS *frame_parms = pscch_pssch_pdu == NULL ? &UE->frame_parms : &UE->SL_UE_PHY_PARAMS.sl_frame_params;
  int start_symbol          = (1+pscch_pssch_pdu->pssch_startsym);
  uint16_t ul_dmrs_symb_pos = pscch_pssch_pdu->dmrs_symbol_position;
  uint8_t number_of_symbols = pscch_pssch_pdu->pssch_numsym;
  uint8_t dmrs_type         = pusch_dmrs_type1;
  uint16_t start_rb         = pscch_pssch_pdu->startrb;
  uint16_t nb_rb            = pscch_pssch_pdu->l_subch * pscch_pssch_pdu->subchannel_size;
  uint8_t Nl                = pscch_pssch_pdu->num_layers;
  uint8_t mod_order         = pscch_pssch_pdu->mod_order;
  uint16_t rnti             = 0;
  uint8_t cdm_grps_no_data  = 1;
  uint16_t start_sc         = frame_parms->first_carrier_offset + start_rb*NR_NB_SC_PER_RB;
  uint16_t Tpmi             = 0;
 
  if (start_sc >= frame_parms->ofdm_symbol_size)
    start_sc -= frame_parms->ofdm_symbol_size;
  ulsch_ue->Nid_cell = frame_parms->Nid_cell;
  uint8_t first_dmrs_symbol = 0;
  bool is_first_dmrs_symbol = true;
  for (int i = start_symbol; i < start_symbol + number_of_symbols; i++) {
    if((ul_dmrs_symb_pos >> i) & 0x01) {
      number_dmrs_symbols += 1;
      if (is_first_dmrs_symbol) {
        first_dmrs_symbol = i;
        is_first_dmrs_symbol = false;
      }
      if (csi_params && csi_params->symb_l0 != 0)
        AssertFatal(i != csi_params->symb_l0, "CSI-RS  (symb_l0 %d) MUST not be sent in DMRS symbol (%d)\n", csi_params->symb_l0, i);
    }
  }

  if (csi_params && csi_params->symb_l0 != 0)
    AssertFatal(csi_params->symb_l0 > pscch_pssch_pdu->pscch_numsym, "CSI-RS (symb_l0 %d) MUST not be sent in PSCCH symbol (%d)\n", csi_params->symb_l0, pscch_pssch_pdu->pscch_numsym);
  nb_dmrs_re_per_rb = ((dmrs_type == pusch_dmrs_type1) ? 6:4)*cdm_grps_no_data;

  //LOG_I(NR_PHY,"%s TX %x : start_rb %d nb_rb %d mod_order %d Nl %d Tpmi %d bwp_start %d start_sc %d start_symbol %d num_symbols %d cdmgrpsnodata %d num_dmrs %d dmrs_re_per_rb %d\n",pscch_pssch_pdu==NULL?"PUSCH":"PSSCH",
   //     rnti,start_rb,nb_rb,mod_order,Nl,Tpmi,pscch_pssch_pdu==NULL?pusch_pdu->bwp_start:0,start_sc,start_symbol,number_of_symbols,cdm_grps_no_data,number_dmrs_symbols,nb_dmrs_re_per_rb);
  // TbD num_of_mod_symbols is set but never used
  uint16_t num_CSI_REs = is_csi_rs_slot ? get_nRECSI_RS(csi_params->freq_density, csi_params->nr_of_rbs, get_nrUE_params()->nb_antennas_tx) : 0;
  int num_CSI_REs_per_RB = is_csi_rs_slot ? (num_CSI_REs/csi_params->nr_of_rbs) : 0;
  N_RE_prime = NR_NB_SC_PER_RB * number_of_symbols - nb_dmrs_re_per_rb * number_dmrs_symbols - N_PRB_oh - num_CSI_REs_per_RB;
  harq_process_ul_ue->num_of_mod_symbols = N_RE_prime*nb_rb;

  /////////////////////////ULSCH coding/////////////////////////
  ///////////
  int sci2_re = get_NREsci2(pscch_pssch_pdu->sci2_alpha_times_100,
                            pscch_pssch_pdu->sci2_payload_len,
                            pscch_pssch_pdu->sci2_beta_offset,
                            pscch_pssch_pdu->pssch_numsym,
                            pscch_pssch_pdu->pscch_numsym,
                            pscch_pssch_pdu->pscch_numrbs,
                            pscch_pssch_pdu->l_subch,
                            pscch_pssch_pdu->subchannel_size,
			    pscch_pssch_pdu->target_coderate);
  //if (pscch_pssch_pdu) LOG_I(NR_PHY,"dmrs_symbol_position %x, pscch_numsym %d\n",pscch_pssch_pdu->dmrs_symbol_position,pscch_pssch_pdu->pscch_numsym);
  AssertFatal(pscch_pssch_pdu->pscch_numsym==2 || pscch_pssch_pdu->pscch_numsym==3,"illegal pscch_numsym %d\n",pscch_pssch_pdu->pscch_numsym);
  int sci1_dmrs_overlap = pscch_pssch_pdu->dmrs_symbol_position & dmrs_pscch_mask[pscch_pssch_pdu->pscch_numsym-2];
  uint16_t sci1_re = pscch_pssch_pdu->pscch_numsym * pscch_pssch_pdu->pscch_numrbs * NR_NB_SC_PER_RB;
  unsigned int G = nr_get_G_SL(nb_rb, number_of_symbols, 6, number_dmrs_symbols, sci1_dmrs_overlap, sci1_re, pscch_pssch_pdu->pscch_numrbs, sci2_re, num_CSI_REs, mod_order, Nl);

  // Following code checks, after PSCCH symbols and DMRS symbols, whether PSSCH symbols are used by SCI2 or not,
  // If true, then CSI-RS MUST not be sent in those PSSCH symbols containing SCI2.
  if (csi_params && csi_params->symb_l0 != 0) {
    int32_t next_symbs_sci2_re = 0;
    int32_t sci1_re = 12 * pscch_pssch_pdu->pscch_numrbs;
    int32_t non_sci1_re = 12 * nb_rb - sci1_re;
    next_symbs_sci2_re = first_dmrs_symbol <= pscch_pssch_pdu->pscch_numsym ? sci2_re - (non_sci1_re / 2 - (non_sci1_re * (pscch_pssch_pdu->pscch_numsym - 1)) - (12 * nb_rb) / 2) : sci2_re - (12 * nb_rb) / 2;
    int8_t remaining_sci2_symb = next_symbs_sci2_re > 0 ? ceil(next_symbs_sci2_re / (12 * nb_rb)) : 0;
    int8_t non_csi_rs_symbs = pscch_pssch_pdu->pscch_numsym + 1 + remaining_sci2_symb; // 1 is for first dmrs symbol
    AssertFatal(csi_params->symb_l0 > non_csi_rs_symbs, "CSI-RS MUST not be sent in PSSCH symbol containing SCI2");
  }

  uint32_t Gsci2 = sci2_re*2*Nl;
  ws_trace_t tmp = {.nr = true,
                    .direction = DIRECTION_UPLINK,
                    .pdu_buffer = harq_process_ul_ue->payload_AB,
                    .pdu_buffer_size = pscch_pssch_pdu->tb_size,
                    .ueid = 0,
                    .rntiType = WS_C_RNTI,
                    .rnti = rnti,
                    .sysFrame = frame,
                    .subframe = slot,
                    .harq_pid = harq_pid,
                    .oob_event = 0,
                    .oob_event_value = 0};
  trace_pdu(&tmp);

  uint8_t ULSCH_ids = 0;

  if (nr_ulsch_encoding(UE, ulsch_ue, pscch_pssch_pdu,harq_pid, frame, slot , &G, 1, &ULSCH_ids) == -1)
    return;
 
  
  uint32_t sci2_encoded_output[sci2_re*2];
  
  if (pscch_pssch_pdu) {
    LOG_D(NR_PHY,"Generating SCI2/PSSCH with %d RE, payload %llx\n",sci2_re,*(unsigned long long*)pscch_pssch_pdu->sci2_payload);
    // do SCI2 encoding
    polar_encoder_fast((uint64_t*)pscch_pssch_pdu->sci2_payload, (void*)sci2_encoded_output, 0, 1, 
                       NR_POLAR_SCI2_MESSAGE_TYPE, 
                       pscch_pssch_pdu->sci2_payload_len, sci2_re);
  }

  ///////////
  ////////////////////////////////////////////////////////////////////

  /////////////////////////SLSCH scrambling/////////////////////////
  ///////////

  uint32_t available_bits = G;
  uint32_t scrambled_output[(available_bits>>5)+1];
  uint32_t scrambled_output_sci[(Gsci2>>5)+1];
  memset(scrambled_output, 0, ((available_bits>>5)+1)*sizeof(uint32_t));
  memset(scrambled_output_sci, 0, ((Gsci2>>5)+1)*sizeof(uint32_t));

//  for (int i=0;i<(Gsci2>>5)+1;i++) LOG_I(NR_PHY,"sci2_encoded[%d] %x\n",i,sci2_encoded_output[i]); 
//  for (int g=0;g<G;g++) LOG_I(NR_PHY,"coded_output_f[%d] %d\n",g,harq_process_ul_ue->f[g]);
//  LOG_I(NR_PHY,"Scrambling with Nid %x\n",phy_data->pscch_Nid);
  nr_pusch_codeword_scrambling(harq_process_ul_ue->f,
                               G,
                               phy_data->pscch_Nid,
                               1010,
                               false,
			       NULL,
                               scrambled_output);
  if (Nl==1) 
    nr_sci_scrambling(sci2_encoded_output,
                        Gsci2,
                        phy_data->pscch_Nid,1010,
                        scrambled_output_sci,1);
  else
    nr_pssch_codeword_scrambling_sci_2layer(sci2_encoded_output,
                                 Gsci2,
                                 phy_data->pscch_Nid,
                                 scrambled_output_sci);                                          
  /////////////
  //////////////////////////////////////////////////////////////////////////

  /////////////////////////ULSCH modulation/////////////////////////
  ///////////

  int max_num_re = Nl*number_of_symbols*nb_rb*NR_NB_SC_PER_RB;
  int32_t d_mod[max_num_re] __attribute__ ((aligned(16)));

  if (Gsci2 > 0) {
    nr_modulation(scrambled_output_sci, // assume one codeword for the moment
                  Gsci2,
                  2,
                  (int16_t *)d_mod);
    //for (int i=0;i<Gsci2;i+=2) LOG_I(NR_PHY,"SCI2 RE %d/%d: (%d,%d)\n",i/2,Gsci2/2,((int16_t*)d_mod)[i],((int16_t*)d_mod)[i+1]);
    int32_t d_mod2[max_num_re] __attribute__ ((aligned(16)));
    nr_modulation(scrambled_output, // assume one codeword for the moment
                  available_bits,
                  mod_order,
                  (int16_t *)d_mod2);
    LOG_D(NR_PHY,"SCI bits %d (sci2_re %d), PSSCH bits %d (PSCCH RE %d), max_re %d\n",Gsci2,sci2_re,available_bits,available_bits/mod_order,max_num_re);
    memcpy(d_mod+sci2_re,d_mod2,available_bits*sizeof(int32_t)/mod_order);
  }
  else
    nr_modulation(scrambled_output, // assume one codeword for the moment
                  available_bits,
                  mod_order,
                  (int16_t *)d_mod);
   
  ///////////
  ////////////////////////////////////////////////////////////////////////

  /////////////////////////DMRS Modulation/////////////////////////
  ///////////

  uint16_t n_dmrs = (start_rb + nb_rb)*((dmrs_type == pusch_dmrs_type1) ? 6:4);
  c16_t mod_dmrs[n_dmrs] __attribute((aligned(16)));

  /////////////////////////SLSCH layer mapping/////////////////////////
  ///////////

  c16_t tx_layers[Nl][(available_bits/mod_order)+sci2_re] __attribute__((aligned(64)));
  nr_ue_layer_mapping((c16_t *)d_mod,
                      Nl,
                      (available_bits/mod_order)+sci2_re,
                      tx_layers);

  ///////////
  ////////////////////////////////////////////////////////////////////////


  /////////////////////////SLSCH RE mapping/////////////////////////
  ///////////

  int encoded_length = frame_parms->N_RB_UL*14*NR_NB_SC_PER_RB*mod_order*Nl;
  c16_t tx_precoding[Nl][encoded_length] __attribute__((aligned(64))); 

  for (int nl=0; nl < Nl; nl++) {
    uint8_t k_prime = 0;
    uint16_t m = 0;
    
#ifdef DEBUG_PUSCH_MAPPING
    LOG_I(NR_PHY,"NR_ULSCH_UE: Value of CELL ID %d /t, u %d \n", frame_parms->Nid_cell, u);
#endif

    int dmrs_port = get_dmrs_port(nl,Nl);
    if (dmrs_port < 0) return;
    // DMRS params for this dmrs port
    get_Wt(Wt, dmrs_port, dmrs_type);
    get_Wf(Wf, dmrs_port, dmrs_type);
    delta = get_delta(dmrs_port, dmrs_type);

    for (int l=start_symbol; l<start_symbol+number_of_symbols; l++) {

      uint16_t k = start_sc;
      uint16_t n = 0;
      uint8_t is_dmrs_sym = 0;
      uint8_t is_csi_rs_sym = 0;
      uint16_t dmrs_idx = 0;
      int16_t csi_rs_rb = 0;
      int is_pscch_sym = 0;
      if (l<(start_symbol + pscch_pssch_pdu->pscch_numsym)) { 
        is_pscch_sym = 1; 
      }

      if (is_csi_rs_slot && l == csi_params->symb_l0) {
        is_csi_rs_sym = 1;
        csi_rs_rb = csi_params->start_rb;
      }

      if ((ul_dmrs_symb_pos >> l) & 0x01) {
        is_dmrs_sym = 1;

        
        dmrs_idx = start_rb*6;

        // TODO: performance improvement, we can skip the modulation of DMRS symbols outside the bandwidth part
        // Perform this on gold sequence, not required when SC FDMA operation is done,
        LOG_D(PHY,"DMRS in symbol %d\n",l);
        uint32_t pssch_dmrs[((frame_parms->N_RB_UL * 12) >> 5) + 1];
        nr_init_pssch_dmrs_oneshot(frame_parms,phy_data->pscch_Nid,pssch_dmrs,slot,l);
        nr_modulation(pssch_dmrs, n_dmrs*2, DMRS_MOD_ORDER, (int16_t*)mod_dmrs); // currently only codeword 0 is modulated. Qm = 2 as DMRS is QPSK modulated
       } else {
          dmrs_idx = 0;
       }

      for (i=0; i< nb_rb*NR_NB_SC_PER_RB; i++) {
        uint8_t is_dmrs = 0;
        uint8_t is_csi_rs = 0;

        if (is_pscch_sym && i==(pscch_pssch_pdu->startrb)) {
           i+=(pscch_pssch_pdu->pscch_numrbs*NR_NB_SC_PER_RB);
           k+=(pscch_pssch_pdu->pscch_numrbs*NR_NB_SC_PER_RB);
           if (is_dmrs_sym) { 
              dmrs_idx+=(6*pscch_pssch_pdu->pscch_numrbs);
              n+=(3*pscch_pssch_pdu->pscch_numrbs);
           }
        }

        LOG_D(NR_PHY, "symbol %d re %d/%d k %d\n", l, i, nb_rb*NR_NB_SC_PER_RB, k);
        sample_offsetF = l*frame_parms->ofdm_symbol_size + k;

        if (is_dmrs_sym) {
          if (k == ((start_sc+get_dmrs_freq_idx_ul(n, k_prime, delta, dmrs_type))%frame_parms->ofdm_symbol_size))
            is_dmrs = 1;
        }

        if (is_csi_rs_sym) {
          AssertFatal(1==0,"No SL CSI-RS for now\n");
	  /*
          if ((k >= csi_params->start_rb * NR_NB_SC_PER_RB) && (i % NR_NB_SC_PER_RB == 0) && (csi_rs_rb < csi_params->nr_of_rbs)) {
            csi_rs_params_t table_params;
            get_csi_rs_params_from_table(csi_params, &table_params);
            port_freq_indices_t *port_freq_indices = (port_freq_indices_t *)malloc(table_params.ports*sizeof(port_freq_indices));
            get_csi_rs_freq_ind_sl(frame_parms, csi_rs_rb, csi_params, &table_params, port_freq_indices);
            if (k == port_freq_indices[nl].k) {
              is_csi_rs = 1;
              csi_rs_rb++;
              LOG_D(NR_PHY, "Tx port_freq_indices.p %i, port_freq_indices.k %d, is_csi_rs %d, k = %i, RE %i, csi_rs_rb %i\n",
                    port_freq_indices[nl].p, port_freq_indices[nl].k, is_csi_rs, k, i, csi_rs_rb);
            }
            free(port_freq_indices);
            port_freq_indices = NULL;
          }*/
        }

        if (is_dmrs == 1) {
          tx_precoding[nl][sample_offsetF].r = (Wt[l_prime[0]]*Wf[k_prime]*AMP*mod_dmrs[dmrs_idx].r) >> 15;
          tx_precoding[nl][sample_offsetF].i = (Wt[l_prime[0]]*Wf[k_prime]*AMP*mod_dmrs[dmrs_idx].i) >> 15;

#ifdef DEBUG_PUSCH_MAPPING
          LOG_I(NR_PHY,"DMRS: Layer: %d\t, dmrs_idx %d\t l %d \t k %d \t k_prime %d \t n %d \t dmrs: %d %d\n",
                 nl, dmrs_idx, l, k, k_prime, n, 
		 tx_precoding[nl][sample_offsetF].r,
                 tx_precoding[nl][sample_offsetF].i);
#endif

          dmrs_idx++;
          k_prime++;
          k_prime&=1;
          n+=(k_prime)?0:1;
      
        } else if (!is_dmrs_sym || allowed_xlsch_re_in_dmrs_symbol(k, start_sc, frame_parms->ofdm_symbol_size, cdm_grps_no_data, dmrs_type)) {
          if (!is_csi_rs) {
            tx_precoding[nl][sample_offsetF].r = tx_layers[nl][m].r;
            tx_precoding[nl][sample_offsetF].i = tx_layers[nl][m].i;
          } else {
            tx_precoding[nl][sample_offsetF] = (c16_t){.r=0,.i=0};
          }

#ifdef DEBUG_PUSCH_MAPPING
          LOG_I(NR_PHY,"DATA: layer %d\t m %d\t l %d \t k %d \t tx_precoding: %d %d\n",
                 nl, m, l, k, 
		 tx_precoding[nl][sample_offsetF].r,
                 tx_precoding[nl][sample_offsetF].i);
#endif

          if (!is_csi_rs)
            m++;

        } else {
          tx_precoding[nl][sample_offsetF] = (c16_t){.r=0,.i=0};
        }

        if (++k >= frame_parms->ofdm_symbol_size)
          k -= frame_parms->ofdm_symbol_size;
      } //for (i=0; i< nb_rb*NR_NB_SC_PER_RB; i++) 
    }//for (l=start_symbol; l<start_symbol+number_of_symbols; l++)
  }//for (nl=0; nl < Nl; nl++)



  /////////////////////////ULSCH precoding/////////////////////////
  ///////////
  ///Layer Precoding and Antenna port mapping
  // tx_layers 0-3 are mapped on antenna ports
  // The precoding info is supported by nfapi such as num_prgs, prg_size, prgs_list and pm_idx
  // The same precoding matrix is applied on prg_size RBs, Thus
  //        pmi = prgs_list[rbidx/prg_size].pm_idx, rbidx =0,...,rbSize-1
  // The Precoding matrix:
  for (int ap=0; ap<frame_parms->nb_antennas_tx; ap++) {
    for (int l=start_symbol; l<start_symbol+number_of_symbols; l++) {
      uint16_t k = start_sc;
      int is_pscch_sym = 0;
      if (pscch_pssch_pdu && l<(start_symbol + pscch_pssch_pdu->pscch_numsym)) { 
        is_pscch_sym = 1; 
      }

      for (int rb=0; rb<nb_rb; rb++) {
        if (is_pscch_sym && rb==(pscch_pssch_pdu->startrb)) {
           k+=(pscch_pssch_pdu->pscch_numrbs*NR_NB_SC_PER_RB);
	   if (k>=frame_parms->ofdm_symbol_size) k-=frame_parms->ofdm_symbol_size;
	   rb=pscch_pssch_pdu->startrb+pscch_pssch_pdu->pscch_numrbs;
        }
        //get pmi info
        uint8_t pmi=Tpmi;
          
        if (pmi == 0) {//unitary Precoding
          if (k + NR_NB_SC_PER_RB <= frame_parms->ofdm_symbol_size) { // RB does not cross DC
            if (ap<Nl) 
              memcpy(&txdataF[ap][l*frame_parms->ofdm_symbol_size  + k],
                     &tx_precoding[ap][2*(l*frame_parms->ofdm_symbol_size + k)],
                     NR_NB_SC_PER_RB*sizeof(int32_t));
            else
              memset(&txdataF[ap][l*frame_parms->ofdm_symbol_size + k],
                     0,
                     NR_NB_SC_PER_RB*sizeof(int32_t));
          } else { // RB does cross DC
            int neg_length = frame_parms->ofdm_symbol_size - k;
            int pos_length = NR_NB_SC_PER_RB - neg_length;
            if (ap<Nl) {
              memcpy(&txdataF[ap][l*frame_parms->ofdm_symbol_size + k],
                     &tx_precoding[ap][2*(l*frame_parms->ofdm_symbol_size + k)],
                     neg_length*sizeof(int32_t));
              memcpy(&txdataF[ap][l*frame_parms->ofdm_symbol_size],
                     &tx_precoding[ap][2*(l*frame_parms->ofdm_symbol_size)],
                     pos_length*sizeof(int32_t));
            } else {
              memset(&txdataF[ap][l*frame_parms->ofdm_symbol_size + k],
                     0,
                     neg_length*sizeof(int32_t));
              memset(&txdataF[ap][l*frame_parms->ofdm_symbol_size],
                     0,
                     pos_length*sizeof(int32_t));
            }
          }
          k += NR_NB_SC_PER_RB;
          if (k >= frame_parms->ofdm_symbol_size) {
            k -= frame_parms->ofdm_symbol_size;
          }
        }
        else {
          //get the precoding matrix weights:
          const char *W_prec;
          switch (frame_parms->nb_antennas_tx) {
            case 1://1 antenna port
              W_prec = nr_W_1l_2p[pmi][ap];
              break;
            case 2://2 antenna ports
              if (Nl == 1)//1 layer
                W_prec = nr_W_1l_2p[pmi][ap];
              else//2 layers
                W_prec = nr_W_2l_2p[pmi][ap];
              break;
            case 4://4 antenna ports
              if (Nl == 1)//1 layer
                W_prec = nr_W_1l_4p[pmi][ap];
              else if (Nl == 2)//2 layers
                W_prec = nr_W_2l_4p[pmi][ap];
              else if (Nl == 3)//3 layers
                W_prec = nr_W_3l_4p[pmi][ap];
              else//4 layers
                W_prec = nr_W_4l_4p[pmi][ap];
              break;
            default:
              LOG_D(PHY,"Precoding 1,2, or 4 antenna ports are currently supported\n");
              W_prec = nr_W_1l_2p[pmi][ap];
              break;
          }

          for (int i=0; i<NR_NB_SC_PER_RB; i++) {
            int32_t re_offset = l*frame_parms->ofdm_symbol_size + k;
            c16_t precodatatx_F = nr_layer_precoder(encoded_length,tx_precoding, W_prec, Nl, re_offset);
            txdataF[ap][re_offset].r = precodatatx_F.r;
            txdataF[ap][re_offset].i = precodatatx_F.i;
                            
            if (++k >= frame_parms->ofdm_symbol_size) {
              k -= frame_parms->ofdm_symbol_size;
            }
          }
        }
      } //RB loop
    } // symbol loop
  }// port loop
}

