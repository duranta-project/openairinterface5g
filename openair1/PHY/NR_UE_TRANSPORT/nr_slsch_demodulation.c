#include "PHY/defs_gNB.h"
#include "PHY/phy_extern.h"
#include "PHY/impl_defs_top.h"
#include "PHY/NR_TRANSPORT/nr_sch_dmrs.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_proto_ue.h"
#include "PHY/NR_REFSIG/dmrs_nr.h"
#include "PHY/NR_REFSIG/ptrs_nr.h"
#include "PHY/NR_ESTIMATION/nr_ul_estimation.h"
#include "PHY/defs_nr_common.h"
#include "common/utils/nr/nr_common.h"
#include "PHY/NR_REFSIG/sl_refsig_defs.h"
#include "executables/nr-uesoftmodem.h"
#include "PHY/nr_phy_common/inc/nr_phy_common.h"
#include "SCHED_NR_UE/defs.h"
#include "openair1/PHY/MODULATION/modulation_UE.h"
#include "PHY/sse_intrin.h"

//#define DEBUG_CH_COMP
//#define DEBUG_RB_EXT
//#define DEBUG_CH_MAG
//#define ML_DEBUG

#define INVALID_VALUE 255

/* duplicate, already in nr_ulsch_demodulation.c
void nr_idft(int32_t *z, uint32_t Msc_PUSCH) {


  simde__m128i idft_in128[1][3240], idft_out128[1][3240];
  simde__m128i norm128;
  int16_t *idft_in0 = (int16_t*)idft_in128[0], *idft_out0 = (int16_t*)idft_out128[0];

  int i, ip;

  LOG_T(PHY,"Doing lte_idft for Msc_PUSCH %d\n",Msc_PUSCH);

  if ((Msc_PUSCH % 1536) > 0) {
    // conjugate input
    for (i = 0; i < (Msc_PUSCH>>2); i++) {
      *&(((simde__m128i*)z)[i]) = oai_mm_conj(*&(((simde__m128i*)z)[i]));
    }
    for (i = 0, ip = 0; i < Msc_PUSCH; i++, ip+=4)
      ((uint32_t*)idft_in0)[ip+0] = z[i];
  }

  switch (Msc_PUSCH) {
    case 12:
      dft(DFT_12,(int16_t *)idft_in0, (int16_t *)idft_out0,0);

      norm128 = simde_mm_set1_epi16(9459);

      for (i = 0; i < 12; i++) {
        ((simde__m128i*)idft_out0)[i] = simde_mm_slli_epi16(simde_mm_mulhi_epi16(((simde__m128i*)idft_out0)[i], norm128), 1);
      }

      break;

    case 24:
      dft(DFT_24,idft_in0, idft_out0, 1);
      break;

    case 36:
      dft(DFT_36,idft_in0, idft_out0, 1);
      break;

    case 48:
      dft(DFT_48,idft_in0, idft_out0, 1);
      break;

    case 60:
      dft(DFT_60,idft_in0, idft_out0, 1);
      break;

    case 72:
      dft(DFT_72,idft_in0, idft_out0, 1);
      break;

    case 96:
      dft(DFT_96,idft_in0, idft_out0, 1);
      break;

    case 108:
      dft(DFT_108,idft_in0, idft_out0, 1);
      break;

    case 120:
      dft(DFT_120,idft_in0, idft_out0, 1);
      break;

    case 144:
      dft(DFT_144,idft_in0, idft_out0, 1);
      break;

    case 180:
      dft(DFT_180,idft_in0, idft_out0, 1);
      break;

    case 192:
      dft(DFT_192,idft_in0, idft_out0, 1);
      break;

    case 216:
      dft(DFT_216,idft_in0, idft_out0, 1);
      break;

    case 240:
      dft(DFT_240,idft_in0, idft_out0, 1);
      break;

    case 288:
      dft(DFT_288,idft_in0, idft_out0, 1);
      break;

    case 300:
      dft(DFT_300,idft_in0, idft_out0, 1);
      break;

    case 324:
      dft(DFT_324,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 360:
      dft(DFT_360,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 384:
      dft(DFT_384,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 432:
      dft(DFT_432,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 480:
      dft(DFT_480,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 540:
      dft(DFT_540,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 576:
      dft(DFT_576,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 600:
      dft(DFT_600,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 648:
      dft(DFT_648,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 720:
      dft(DFT_720,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 768:
      dft(DFT_768,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 864:
      dft(DFT_864,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 900:
      dft(DFT_900,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 960:
      dft(DFT_960,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 972:
      dft(DFT_972,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 1080:
      dft(DFT_1080,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 1152:
      dft(DFT_1152,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 1200:
      dft(DFT_1200,idft_in0, idft_out0, 1);
      break;

    case 1296:
      dft(DFT_1296,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 1440:
      dft(DFT_1440,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 1500:
      dft(DFT_1500,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 1536:
      //dft(DFT_1536,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      idft(IDFT_1536,(int16_t*)z, (int16_t*)z, 1);
      break;

    case 1620:
      dft(DFT_1620,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 1728:
      dft(DFT_1728,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 1800:
      dft(DFT_1800,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 1920:
      dft(DFT_1920,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 1944:
      dft(DFT_1944,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 2160:
      dft(DFT_2160,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 2304:
      dft(DFT_2304,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 2400:
      dft(DFT_2400,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 2592:
      dft(DFT_2592,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 2700:
      dft(DFT_2700,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 2880:
      dft(DFT_2880,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 2916:
      dft(DFT_2916,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 3000:
      dft(DFT_3000,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    case 3072:
      //dft(DFT_3072,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      idft(IDFT_3072,(int16_t*)z, (int16_t*)z, 1);
      break;

    case 3240:
      dft(DFT_3240,(int16_t*)idft_in0, (int16_t*)idft_out0, 1);
      break;

    default:
      // should not be reached
      LOG_E( PHY, "Unsupported Msc_PUSCH value of %"PRIu16"\n", Msc_PUSCH );
      return;
  }

  if ((Msc_PUSCH % 1536) > 0) {
    for (i = 0, ip = 0; i < Msc_PUSCH; i++, ip+=4)
      z[i] = ((uint32_t*)idft_out0)[ip];

    // conjugate output
    for (i = 0; i < (Msc_PUSCH>>2); i++) {
      ((simde__m128i*)z)[i] = oai_mm_conj(((simde__m128i*)z)[i]);

    }
  }

}
*/


void nr_pssch_extract_rbs(int rxFSz,
                          c16_t rxdataF[][rxFSz],
			  int rxdataFextSz,
			  c16_t rxdataF_ext[][rxdataFextSz],
			  c16_t chF_ext[][rxdataFextSz],
                          NR_gNB_PUSCH *pusch_vars,
                          int slot,
                          unsigned char symbol,
			  uint32_t dmrs_symbol,
                          uint8_t is_dmrs_symbol,
                          uint8_t is_csirs_symbol,
                          uint32_t bwp_start,
                          uint32_t rb_start,
                          uint32_t rb_size,
                          uint32_t nrOfLayers,
                          uint32_t num_dmrs_cdm_grps_no_data,
                          uint32_t dmrs_config_type,
                          NR_DL_FRAME_PARMS *frame_parms,
                          nfapi_nr_dl_tti_csi_rs_pdu_rel15_t *csi_params) {
 
  unsigned short start_re, re, nb_re_pusch;
  unsigned char aarx, aatx;
  uint32_t rxF_ext_index = 0;
  uint32_t ul_ch0_ext_index = 0;
  uint32_t ul_ch0_index = 0;
  c16_t *rxF,*rxF_ext;
  c16_t *ul_ch0,*ul_ch0_ext;
  int soffset = 0; /*(slot&3)*frame_parms->symbols_per_slot*frame_parms->ofdm_symbol_size;*/

#ifdef DEBUG_RB_EXT
  printf("--------------------symbol = %d-----------------------\n", symbol);
  printf("--------------------ch_ext_index = %d-----------------------\n", symbol*NR_NB_SC_PER_RB * rb_size);
#endif

  uint8_t is_data_re;
  start_re = (frame_parms->first_carrier_offset + (rb_start + bwp_start) * NR_NB_SC_PER_RB)%frame_parms->ofdm_symbol_size;
  nb_re_pusch = NR_NB_SC_PER_RB * rb_size;

  int nb_re_pusch2 = nb_re_pusch + (nb_re_pusch&7);
  for (aarx = 0; aarx < frame_parms->nb_antennas_rx; aarx++) {

    rxF = &rxdataF[aarx][soffset+(symbol * frame_parms->ofdm_symbol_size)];
    rxF_ext = &rxdataF_ext[aarx][symbol * nb_re_pusch2]; // [hna] rxdataF_ext isn't contiguous in order to solve an alignment problem ib llr computation in case of mod_order = 4, 6
    AssertFatal(soffset + (symbol * frame_parms->ofdm_symbol_size) + start_re < rxFSz, "rxF offset is greater than the buffer size\n");
    AssertFatal(symbol * nb_re_pusch2 + nb_re_pusch < nb_re_pusch2 * frame_parms->symbols_per_slot, "Copied PUSCH data is more than rxF_ext size\n");
    LOG_D(NR_PHY,"symbol %d : rxF energy %d\n",symbol,dB_fixed(signal_energy_nodc(rxF,frame_parms->ofdm_symbol_size))); 
    if (is_dmrs_symbol == 0) {
      if (is_csirs_symbol == 0) {
        if (start_re + nb_re_pusch <= frame_parms->ofdm_symbol_size) {
          memcpy((void*)rxF_ext, (void*)&rxF[start_re*2], nb_re_pusch*sizeof(int32_t));
        } else {
          int neg_length = frame_parms->ofdm_symbol_size-start_re;
          int pos_length = nb_re_pusch-neg_length;
          memcpy((void*)rxF_ext, (void*)&rxF[start_re*2], neg_length*sizeof(int32_t));
          memcpy((void*)&rxF_ext[2*neg_length], (void*)rxF, pos_length*sizeof(int32_t));
        }

        for (aatx = 0; aatx < nrOfLayers; aatx++) {
          ul_ch0 = (c16_t*)&pusch_vars->ul_ch_estimates[aatx*frame_parms->nb_antennas_rx + aarx][dmrs_symbol*frame_parms->ofdm_symbol_size]; // update channel estimates if new dmrs symbol are available
          ul_ch0_ext = &chF_ext[aatx*frame_parms->nb_antennas_rx + aarx][symbol*nb_re_pusch2];
          memcpy((void*)ul_ch0_ext, (void*)ul_ch0,nb_re_pusch*sizeof(int32_t));
        }
      } else {
        int16_t csi_rs_rb = csi_params->start_rb;
        for (aatx = 0; aatx < nrOfLayers; aatx++) {
          ul_ch0 = (c16_t*)&pusch_vars->ul_ch_estimates[aatx*frame_parms->nb_antennas_rx + aarx][dmrs_symbol*frame_parms->ofdm_symbol_size]; // update channel estimates if new dmrs symbol are available
          ul_ch0_ext = &chF_ext[aatx*frame_parms->nb_antennas_rx + aarx][symbol*nb_re_pusch2];

          rxF_ext_index = 0;
          ul_ch0_ext_index = 0;
          ul_ch0_index = 0;
          for (re = 0; re < nb_re_pusch; re++) {
            uint8_t is_csi_rs = 0;
            uint16_t k = start_re + re;
            if ((k >= csi_params->start_rb * NR_NB_SC_PER_RB) && (re % NR_NB_SC_PER_RB == 0) && (csi_rs_rb < csi_params->nr_of_rbs)) {
              csi_rs_params_t table_params = {0};
              // TODO missing: get_csi_rs_params_from_table(csi_params, &table_params);
              port_freq_indices_t *port_freq_indices = (port_freq_indices_t *)malloc(table_params.ports*sizeof(port_freq_indices));
              // TOD missing: get_csi_rs_freq_ind_sl(frame_parms, csi_rs_rb, csi_params, &table_params, port_freq_indices);
              if (k == port_freq_indices[aatx].k) {
                is_csi_rs = 1;
                csi_rs_rb++;
              }
              free(port_freq_indices);
              port_freq_indices = NULL;
            }

            if (++k >= frame_parms->ofdm_symbol_size) {
              k -= frame_parms->ofdm_symbol_size;
            }

            // save only data and respective channel estimates
            if (is_csi_rs == 0) {
              if (aatx == 0) {
                rxF_ext[rxF_ext_index]     = (rxF[ ((start_re + re)*2)      % (frame_parms->ofdm_symbol_size*2)]);
                rxF_ext[rxF_ext_index + 1] = (rxF[(((start_re + re)*2) + 1) % (frame_parms->ofdm_symbol_size*2)]);
                rxF_ext_index +=2;
              }

              ul_ch0_ext[ul_ch0_ext_index] = ul_ch0[ul_ch0_index];
              ul_ch0_ext_index++;

            }
            ul_ch0_index++;
          }
        }
      }
    } else {

      for (aatx = 0; aatx < nrOfLayers; aatx++) {
        ul_ch0 = (c16_t*)&pusch_vars->ul_ch_estimates[aatx*frame_parms->nb_antennas_rx + aarx][dmrs_symbol*frame_parms->ofdm_symbol_size]; // update channel estimates if new dmrs symbol are available
        ul_ch0_ext = &chF_ext[aatx*frame_parms->nb_antennas_rx + aarx][symbol*nb_re_pusch2];

        rxF_ext_index = 0;
        ul_ch0_ext_index = 0;
        ul_ch0_index = 0;
        for (re = 0; re < nb_re_pusch; re++) {
          uint16_t k = start_re + re;
          is_data_re = allowed_xlsch_re_in_dmrs_symbol(k, start_re, frame_parms->ofdm_symbol_size, num_dmrs_cdm_grps_no_data, dmrs_config_type);
          if (++k >= frame_parms->ofdm_symbol_size) {
            k -= frame_parms->ofdm_symbol_size;
          }

          #ifdef DEBUG_RB_EXT
          printf("re = %d, is_dmrs_symbol = %d, symbol = %d\n", re, is_dmrs_symbol, symbol);
          #endif

          // save only data and respective channel estimates
          if (is_data_re == 1) {
            if (aatx == 0) {
              rxF_ext[rxF_ext_index]     = (rxF[ ((start_re + re)*2)      % (frame_parms->ofdm_symbol_size*2)]);
              rxF_ext[rxF_ext_index + 1] = (rxF[(((start_re + re)*2) + 1) % (frame_parms->ofdm_symbol_size*2)]);
              rxF_ext_index +=2;
            }

            ul_ch0_ext[ul_ch0_ext_index] = ul_ch0[ul_ch0_index];
            ul_ch0_ext_index++;

            #ifdef DEBUG_RB_EXT
            printf("dmrs symb %d: rxF_ext[%u] = (%d,%d), ul_ch0_ext[%u] = (%d,%d)\n",
                 is_dmrs_symbol,rxF_ext_index>>1, rxF_ext[rxF_ext_index],rxF_ext[rxF_ext_index+1],
                 ul_ch0_ext_index,  ((int16_t*)&ul_ch0_ext[ul_ch0_ext_index])[0],  ((int16_t*)&ul_ch0_ext[ul_ch0_ext_index])[1]);
            #endif          
          }
          ul_ch0_index++;
        }
      }
    }
  }
}

void nr_slsch_scale_channel(int buffer_length,
	           	    c16_t sl_ch_estimates_ext[][buffer_length],
                            NR_DL_FRAME_PARMS *frame_parms,
                            uint8_t symbol,
                            uint8_t is_dmrs_symbol,
			    uint32_t nrOfLayers,
                            uint32_t len,
                            unsigned short nb_rb,
                            int shift_ch_ext)
{

  // Determine scaling amplitude based the symbol
  int b = 3;
  short ch_amp = 1024 * 8;
  if (shift_ch_ext > 3) {
    b = 0;
    ch_amp >>= (shift_ch_ext - 3);
    if (ch_amp == 0) {
      ch_amp = 1;
    }
  } else {
    b -= shift_ch_ext;
  }
  simde__m128i ch_amp128 = simde_mm_set1_epi16(ch_amp); // Q3.13
  LOG_D(PHY, "Scaling PUSCH Chest in OFDM symbol %d by %d, pilots %d nb_rb %d NCP %d symbol %d\n", symbol, ch_amp, is_dmrs_symbol, nb_rb, frame_parms->Ncp, symbol);

  uint32_t nb_rb_0 = len / 12 + ((len % 12) ? 1 : 0);
  int off = ((nb_rb & 1) == 1) ? 4 : 0;
  for (int aatx = 0; aatx < nrOfLayers; aatx++) {
    for (int aarx = 0; aarx < frame_parms->nb_antennas_rx; aarx++) {
      simde__m128i *sl_ch128 = (simde__m128i *)&sl_ch_estimates_ext[aatx * frame_parms->nb_antennas_rx + aarx][symbol * (off + (nb_rb * NR_NB_SC_PER_RB))];
      for (int rb = 0; rb < nb_rb_0; rb++) {
        sl_ch128[0] = simde_mm_mulhi_epi16(sl_ch128[0], ch_amp128);
        sl_ch128[0] = simde_mm_slli_epi16(sl_ch128[0], b);

        sl_ch128[1] = simde_mm_mulhi_epi16(sl_ch128[1], ch_amp128);
        sl_ch128[1] = simde_mm_slli_epi16(sl_ch128[1], b);

        sl_ch128[2] = simde_mm_mulhi_epi16(sl_ch128[2], ch_amp128);
        sl_ch128[2] = simde_mm_slli_epi16(sl_ch128[2], b);
        sl_ch128 += 3;
      }
    }
  }
}

//compute average channel_level on each (TX,RX) antenna pair
void nr_slsch_channel_level(int buffer_length,
		            c16_t sl_ch_estimates_ext[][buffer_length],
                            NR_DL_FRAME_PARMS *frame_parms,
                            int32_t *avg,
                            uint8_t symbol,
                            uint32_t len,
                            uint32_t nrOfLayers,
                            unsigned short nb_rb)
{

  short rb;
  unsigned char aatx, aarx;
  simde__m128i *sl_ch128, avg128U;

  int16_t x = factor2(len);
  int16_t y = (len)>>x;

  uint32_t nb_rb_0 = len/12 + ((len%12)?1:0);

  int off = ((nb_rb&1) == 1)? 4:0;

  for (aatx = 0; aatx < nrOfLayers; aatx++) {
    for (aarx = 0; aarx < frame_parms->nb_antennas_rx; aarx++) {
      //clear average level
      avg128U = simde_mm_setzero_si128();

      sl_ch128=(simde__m128i *)&sl_ch_estimates_ext[aatx*frame_parms->nb_antennas_rx+aarx][symbol*(off+(nb_rb*12))];

      for (rb = 0; rb < nb_rb_0; rb++) {
        avg128U = simde_mm_add_epi32(avg128U, simde_mm_srai_epi32(simde_mm_madd_epi16(sl_ch128[0], sl_ch128[0]), x));
        avg128U = simde_mm_add_epi32(avg128U, simde_mm_srai_epi32(simde_mm_madd_epi16(sl_ch128[1], sl_ch128[1]), x));
        avg128U = simde_mm_add_epi32(avg128U, simde_mm_srai_epi32(simde_mm_madd_epi16(sl_ch128[2], sl_ch128[2]), x));
        sl_ch128+=3;
      }

      avg[aatx*frame_parms->nb_antennas_rx+aarx] = (((int32_t*)&avg128U)[0] +
                                                    ((int32_t*)&avg128U)[1] +
                                                    ((int32_t*)&avg128U)[2] +
                                                    ((int32_t*)&avg128U)[3]) / y;
    }
  }
}

// Pre-processing for LLR computation
//==============================================================================================
//
void nr_pssch_channel_compensation(uint32_t buffer_length,
				   uint32_t nb_rx_ant,
                                   c16_t rxFext[][buffer_length],
                                   c16_t chFext[][buffer_length],
                                   c16_t ch_maga[][buffer_length],
                                   c16_t ch_magb[][buffer_length],
                                   c16_t ch_magc[][buffer_length],
                                   c16_t **rxComp,
                                   uint32_t  nrOfLayers,
                                   c16_t rho[][nrOfLayers][buffer_length],
                                   uint32_t symbol,
                                   uint32_t mod_order,
                                   uint32_t output_shift) {

  simde__m256i QAM_ampa_256 = simde_mm256_setzero_si256();
  simde__m256i QAM_ampb_256 = simde_mm256_setzero_si256();
  simde__m256i QAM_ampc_256 = simde_mm256_setzero_si256();

  if (mod_order == 4) {
    QAM_ampa_256 = simde_mm256_set1_epi16(QAM16_n1);
    QAM_ampb_256 = simde_mm256_setzero_si256();
    QAM_ampc_256 = simde_mm256_setzero_si256();
  }
  else if (mod_order == 6) {
    QAM_ampa_256 = simde_mm256_set1_epi16(QAM64_n1);
    QAM_ampb_256 = simde_mm256_set1_epi16(QAM64_n2);
    QAM_ampc_256 = simde_mm256_setzero_si256();
  }
  else if (mod_order == 8) {
    QAM_ampa_256 = simde_mm256_set1_epi16(QAM256_n1);
    QAM_ampb_256 = simde_mm256_set1_epi16(QAM256_n2);
    QAM_ampc_256 = simde_mm256_set1_epi16(QAM256_n3);
  }

  for (int aatx = 0; aatx < nrOfLayers; aatx++) {
    simde__m256i *rxComp_256 = (simde__m256i *)&rxComp[aatx * nb_rx_ant][symbol * buffer_length];
    simde__m256i *rxF_ch_maga_256 = (simde__m256i *)ch_maga[aatx];
    simde__m256i *rxF_ch_magb_256 = (simde__m256i *)ch_magb[aatx];
    simde__m256i *rxF_ch_magc_256 = (simde__m256i *)ch_magc[aatx];
    for (int aarx = 0; aarx < nb_rx_ant; aarx++) {
      simde__m256i *rxF_256 = (simde__m256i *)rxFext[aarx];
      simde__m256i *chF_256 = (simde__m256i *)chFext[aatx*nb_rx_ant + aarx];

      for (int i = 0; i < buffer_length >> 3; i++) 
      {
        // MRC        }
        simde__m256i comp = oai_mm256_cpx_mult_conj(chF_256[i], rxF_256[i], output_shift);
        rxComp_256[i] = simde_mm256_add_epi16(rxComp_256[i], comp); 

        if (mod_order > 2) {
          simde__m256i mag = oai_mm256_smadd(chF_256[i], chF_256[i], output_shift); // |h|^2
          // pack and duplicate
          mag = simde_mm256_packs_epi32(mag, mag);
          mag = simde_mm256_unpacklo_epi16(mag, mag);

          rxF_ch_maga_256[i] = simde_mm256_add_epi16(rxF_ch_maga_256[i], simde_mm256_mulhrs_epi16(mag, QAM_ampa_256));

          if (mod_order > 4)
            rxF_ch_magb_256[i] = simde_mm256_add_epi16(rxF_ch_magb_256[i], simde_mm256_mulhrs_epi16(mag, QAM_ampb_256));

          if (mod_order > 6)
            rxF_ch_magc_256[i] = simde_mm256_add_epi16(rxF_ch_magc_256[i], simde_mm256_mulhrs_epi16(mag, QAM_ampc_256));
        }        
      }
      if (nrOfLayers > 1) {
        for (int atx = 0; atx < nrOfLayers; atx++) {
          simde__m256i *rho_256 = (simde__m256i *)rho[aatx][atx];
          simde__m256i *chF_256 = (simde__m256i *)chFext[aatx*nb_rx_ant + aarx];
          simde__m256i *chF2_256 = (simde__m256i *)chFext[atx*nb_rx_ant + aarx];
          for (int i = 0; i < buffer_length >> 3; i++) {
            rho_256[i] = simde_mm256_adds_epi16(rho_256[i], oai_mm256_cpx_mult_conj(chF_256[i], chF2_256[i], output_shift));
          }
        }
      }
    }
  }

}

void nr_slsch_compute_llr(c16_t *rxdataF_comp,
                          c16_t *ul_ch_mag,
                          c16_t *ul_ch_magb,
                          c16_t *ul_ch_magc,
                          int16_t *slsch_llr,
                          uint32_t nb_re,
                          uint8_t symbol,
                          uint8_t mod_order)
{
  switch(mod_order) {
    case 2:
      nr_qpsk_llr(rxdataF_comp, slsch_llr, nb_re);
      break;
    case 4:
      nr_16qam_llr(rxdataF_comp, ul_ch_mag, slsch_llr, nb_re);
      break;
    case 6:
    nr_64qam_llr(rxdataF_comp, ul_ch_mag, ul_ch_magb, slsch_llr, nb_re);
      break;
    case 8:
    nr_256qam_llr(rxdataF_comp, ul_ch_mag, ul_ch_magb, ul_ch_magc, slsch_llr, nb_re);
      break;
    default:
      AssertFatal(false, "nr_slsch_compute_llr: invalid Qm value, symbol = %d, Qm = %d\n",symbol, mod_order);
      break;
  }
}

//==============================================================================================
extern int dmrs_pscch_mask[2];
/* Main Function */
void nr_rx_pssch(PHY_VARS_NR_UE *ue,
                 const UE_nr_rxtx_proc_t *proc,
                 nr_phy_data_t *phy_data,
                 int rxFSz,
                 c16_t rxdataF[][rxFSz],
		 int16_t *llrs,
                 uint8_t ulsch_id,
                 uint32_t frame,
                 uint8_t slot,
                 unsigned char harq_pid,
                 bool *is_csi_rs_slot)
{

  uint8_t aarx, aatx;
  uint32_t nb_re_pusch, bwp_start_subcarrier;
  int avgs = 0;

  nfapi_nr_dl_tti_csi_rs_pdu_rel15_t *csi_params = NULL;
  NR_DL_FRAME_PARMS *frame_parms = &ue->SL_UE_PHY_PARAMS.sl_frame_params;
  NR_gNB_ULSCH_t *ulsch = &ue->slsch[ulsch_id];
  sl_nr_rx_config_pssch_sci_pdu_t *pssch_pdu = ulsch->harq_process->pssch_pdu;
  uint32_t nrOfLayers = pssch_pdu->num_layers;
  uint32_t rb_start = pssch_pdu->startrb;
  uint32_t bwp_start = 0;
  uint32_t rnti = 0;

  uint32_t rb_size                   = pssch_pdu->num_subch * pssch_pdu->subchannel_size;
  uint32_t qam_mod_order             = pssch_pdu->mod_order;
  uint32_t start_symbol_index        = 1;
  uint32_t nr_of_symbols             = pssch_pdu->pssch_numsym;
  uint32_t dmrs_config_type          = 0;
  uint32_t num_dmrs_cdm_grps_no_data = 1;
  uint32_t ul_dmrs_symb_pos          = pssch_pdu->dmrs_symbol_position;
  uint32_t dmrs_ports                = pssch_pdu->num_layers;
  int sci1_re_per_symb = pssch_pdu->pscch_numrbs*NR_NB_SC_PER_RB; 
  int sci2_re = get_NREsci2(pssch_pdu->sci2_alpha_times_100,
                            pssch_pdu->sci2_len,
                            pssch_pdu->sci2_beta_offset,
                            pssch_pdu->pssch_numsym,
                            pssch_pdu->pscch_numsym,
                            pssch_pdu->pscch_numrbs,
                            pssch_pdu->l_subch,
                            pssch_pdu->subchannel_size,
                            pssch_pdu->targetCodeRate);

  int16_t sci2_llrs[(sci2_re*2)] __attribute__((aligned(16)));
  int16_t unscrambled_sci2_llrs[(sci2_re*2)] __attribute__((aligned(16)));
  uint8_t number_dmrs_symbols = 0;
  uint16_t start_symbol = 1;
  for (int l = start_symbol; l < start_symbol + nr_of_symbols; l++)
    number_dmrs_symbols += ((pssch_pdu->dmrs_symbol_position)>>l)&0x01;

  int nb_re_dmrs = 6;

  int sci1_dmrs_overlap = pssch_pdu->dmrs_symbol_position & dmrs_pscch_mask[pssch_pdu->pscch_numsym-2];

  int sci2_cnt=0;
  int sci2_left = sci2_re;
  if (phy_data->sl_rx_action == SL_NR_CONFIG_TYPE_RX_PSSCH_SLSCH_CSI_RS) {
    *is_csi_rs_slot = true;
    csi_params = (nfapi_nr_dl_tti_csi_rs_pdu_rel15_t *)&phy_data->csirs_vars.csirs_config_pdu;
  } else {
    *is_csi_rs_slot = false;
  }
  uint8_t nr_rbs_w_csi_rs = is_csi_rs_slot ? rb_size / csi_params->freq_density : 0;
  uint8_t subcarriers_used = get_nrUE_params()->nb_antennas_tx > 2 ? 2 : get_nrUE_params()->nb_antennas_tx;
  int num_CSI_REs = is_csi_rs_slot ? nr_rbs_w_csi_rs * subcarriers_used : 0;
  uint16_t sci1_re = pssch_pdu->pscch_numsym * pssch_pdu->pscch_numrbs * NR_NB_SC_PER_RB;

  uint32_t G = nr_get_G_SL(rb_size,
                           nr_of_symbols,
                           nb_re_dmrs,
                           number_dmrs_symbols, // number of dmrs symbols irrespective of single or double symbol dmrs
                           sci1_dmrs_overlap,
                           sci1_re,
                           pssch_pdu->pscch_numrbs,
                           sci2_re,
                           num_CSI_REs,
                           pssch_pdu->mod_order,
                           pssch_pdu->num_layers);

  int avg[frame_parms->nb_antennas_rx*nrOfLayers];
  int16_t *temp_llr = (int16_t *)__builtin_alloca_with_align((8 * ((3 * 8 * 6144) + 12)) * sizeof(int16_t),32);
  c16_t *temp_symbol = (c16_t *)__builtin_alloca_with_align(rb_size * NR_NB_SC_PER_RB * sizeof(int32_t),32);
  NR_gNB_PUSCH *pusch_vars = &ue->pssch_vars[ulsch_id];
  int dmrs_symbol = INVALID_VALUE;
  int cl_done = 0;

  bwp_start_subcarrier = ((rb_start + bwp_start)*NR_NB_SC_PER_RB + frame_parms->first_carrier_offset) % frame_parms->ofdm_symbol_size;
  LOG_D(PHY,"pusch %d.%d : bwp_start_subcarrier %d, rb_start %d, first_carrier_offset %d\n", frame,slot,bwp_start_subcarrier, rb_start, frame_parms->first_carrier_offset);
  LOG_D(PHY,"pusch %d.%d : ul_dmrs_symb_pos %x\n",frame,slot,ul_dmrs_symb_pos);
  LOG_D(PHY,"ulsch RX %x : start_rb %d nb_rb %d Nl %d Tpmi %d bwp_start %d start_sc %d start_symbol %d num_symbols %d cdmgrpsnodata %d num_dmrs %d dmrs_ports %d\n",
          rnti,rb_start,rb_size,
          nrOfLayers,0,bwp_start,0,start_symbol_index,nr_of_symbols,
          num_dmrs_cdm_grps_no_data,ul_dmrs_symb_pos,dmrs_ports);
  //----------------------------------------------------------
  //--------------------- Channel estimation ---------------------
  //----------------------------------------------------------
  int max_ch = 0;
  uint32_t nvar = 0;
  for(uint8_t symbol = start_symbol_index; symbol < (start_symbol_index + nr_of_symbols); symbol++) {
    uint8_t dmrs_symbol_flag = (ul_dmrs_symb_pos >> symbol) & 0x01;
    LOG_D(PHY, "symbol %d, dmrs_symbol_flag :%d\n", symbol, dmrs_symbol_flag);
    
    if (dmrs_symbol_flag == 1) {
      if (dmrs_symbol == INVALID_VALUE)
        dmrs_symbol = symbol;

      for (int nl=0; nl<nrOfLayers; nl++) {
        uint32_t nvar_tmp = 0;
	int dmrs_port = get_dmrs_port(nl,dmrs_ports);
	if (dmrs_port<0) return;
        nr_pssch_channel_estimation(ue,rxFSz,rxdataF,
                                    slot,
                                    dmrs_port,
                                    symbol,
                                    ulsch_id,
                                    bwp_start_subcarrier,
                                    pssch_pdu,
                                    &max_ch,
                                    &nvar_tmp);
        nvar += nvar_tmp;
      }
/*
      PHY_MEASUREMENTS_gNB *meas = ue->sl_measurements;
      gNB_I0_measurements(meas, frame_parms,ulsch, pusch_vars, symbol, nrOfLayers);
      allocCast2D(n0_subband_power,
                  unsigned int,
                  meas->n0_subband_power,
                  frame_parms->nb_antennas_rx,
                  frame_parms->N_RB_UL,
                  false);
      for (aarx = 0; aarx < frame_parms->nb_antennas_rx; aarx++) {
        if (symbol == start_symbol_index) {
          pusch_vars->ulsch_power[aarx] = 0;
          pusch_vars->ulsch_noise_power[aarx] = 0;
        }
        for (aatx = 0; aatx < nrOfLayers; aatx++) {
          pusch_vars->ulsch_power[aarx] += signal_energy_nodc(
              (c16_t*)&pusch_vars->ul_ch_estimates[aatx * frame_parms->nb_antennas_rx + aarx][symbol * frame_parms->ofdm_symbol_size],
              rb_size * 12);
        }
        for (int rb = 0; rb < rb_size; rb++) {
          pusch_vars->ulsch_noise_power[aarx] +=
              n0_subband_power[aarx][bwp_start + rb_start + rb] / rb_size;
        }
        LOG_D(NR_PHY,
              "aa %d, symbol %d, bwp_start%d, rb_start %d, rb_size %d: ulsch_power %d, ulsch_noise_power %d\n",
              aarx,symbol,
              bwp_start,
              rb_start,
              rb_size,
              pusch_vars->ulsch_power[aarx],
              pusch_vars->ulsch_noise_power[aarx]);
      }
*/
    }
  }

  nvar /= (nr_of_symbols * nrOfLayers * frame_parms->nb_antennas_rx);
  int off = ((rb_size&1) == 1)? 4:0;
  uint32_t rxdataF_ext_offset = 0;
  uint8_t shift_ch_ext = nrOfLayers > 1 ? log2_approx(max_ch >> 11) : 0;

  // Flag to select the receiver: (true) Nonlinear ML receiver, (false) Linear MMSE receiver
  // By default, we are using the Nonlinear ML receiver, except
  //  - for 256QAM as Nonlinear ML receiver is not implemented for 256QAM
  //  - for 64QAM as Nonlinear ML receiver requires more processing time than MMSE, and many machines are not powerful enough
  bool ml_rx = true;
  if (nrOfLayers != 2 || qam_mod_order >= 6) {
    ml_rx = false;
  }

  int ad_shift = 0;
  if (nrOfLayers == 1) {
    ad_shift = 1 + log2_approx(frame_parms->nb_antennas_rx >> 2);
  } else if (ml_rx == false) {
    ad_shift = -3; // For 2-layers, we are already doing a bit shift in the nr_ulsch_mmse_2layers() function, so we can use more bits
  }
  int ext_buffer_length = ceil_mod(rb_size * NR_NB_SC_PER_RB, 16);
  c16_t rxFext[frame_parms->nb_antennas_rx][ext_buffer_length] __attribute__((aligned(32)));
  c16_t chFext[nrOfLayers*frame_parms->nb_antennas_rx][ext_buffer_length] __attribute__((aligned(32)));
  c16_t rho[nrOfLayers][nrOfLayers][ext_buffer_length] __attribute__((aligned(32)));
  c16_t rxF_ch_maga  [nrOfLayers][ext_buffer_length] __attribute__((aligned(32)));
  c16_t rxF_ch_magb  [nrOfLayers][ext_buffer_length] __attribute__((aligned(32)));
  c16_t rxF_ch_magc  [nrOfLayers][ext_buffer_length] __attribute__((aligned(32)));

  memset(rho, 0, sizeof(rho));
  memset(rxF_ch_maga, 0, sizeof(rxF_ch_maga));
  memset(rxF_ch_magb, 0, sizeof(rxF_ch_magb));
  memset(rxF_ch_magc, 0, sizeof(rxF_ch_magc));
  memset(rxFext, 0, sizeof(rxFext));
  memset(chFext, 0, sizeof(chFext));

  int16_t scramblingSequence[G + 96] __attribute__((aligned(32)));

  nr_codeword_unscrambling_init(scramblingSequence, G, 0, pssch_pdu->Nid, 1010);

  for (uint8_t symbol = start_symbol_index; symbol < (start_symbol_index + nr_of_symbols); symbol++) {

    uint8_t csi_rs_symbol_flag = 0;
    if (*is_csi_rs_slot && (csi_params->symb_l0 == symbol)) {
      csi_rs_symbol_flag = 1;
      AssertFatal(csi_params->freq_density > 0, "freq_density MUST be greater than zero");
      AssertFatal(csi_params->nr_of_rbs > 0, "nr_of_rbs MUST be greater than zero");
      LOG_D(NR_PHY, "%d.%d symbol %i, freq_density %i symb_l0 %i csi_type %i power_control_offset %i power_control_offset_ss %i cdm_type %i row %i freq_domain %i start_rb %i nr_of_rbs %i\n",
            frame, slot, symbol,
            csi_params->freq_density,
            csi_params->symb_l0,
            csi_params->csi_type,
            csi_params->power_control_offset,
            csi_params->power_control_offset_ss,
            csi_params->cdm_type,
            csi_params->row,
            csi_params->freq_domain,
            csi_params->start_rb,
            csi_params->nr_of_rbs);
      if (phy_data->sl_rx_action == SL_NR_CONFIG_TYPE_RX_PSSCH_SLSCH_CSI_RS) {
        // FIXIT: Reconsider index of csirs_vars[0] for multiple connected UEs case
        if (phy_data->csirs_vars.active == 1) {
          LOG_D(NR_PHY, "%d.%d Received CSI-RS\n", proc->frame_rx, proc->nr_slot_rx);
	  /*
          nr_slot_fep(ue, frame_parms, proc, symbol, rxdataF, link_type_sl);
          nr_ue_csi_rs_procedures(ue, proc, rxdataF, (fapi_nr_dl_config_csirs_pdu_rel15_t*)&phy_data->csirs_vars.csirs_config_pdu);
*/
	  phy_data->csirs_vars.active = 0;
        }
      }
    }

    uint8_t dmrs_symbol_flag = (ul_dmrs_symb_pos >> symbol) & 0x01;
    int dmrs_symbol;
    if (1) // this is averaging of the DMRS
      dmrs_symbol = dmrs_symbol_flag > 0 ? symbol : get_valid_dmrs_idx_for_channel_est(ul_dmrs_symb_pos, symbol);
    else { // average of channel estimates stored in first symbol
      int end_symbol = start_symbol_index + nr_of_symbols;
      dmrs_symbol = get_next_dmrs_symbol_in_slot(ul_dmrs_symb_pos, start_symbol_index, end_symbol);
    }
    int sci2_cnt_thissymb=0;
    if (csi_rs_symbol_flag) {
      uint8_t freq_subcarriers_per_rb = 12;
      uint8_t nr_rbs_w_csi_rs = csi_params->nr_of_rbs / csi_params->freq_density;
      uint8_t nr_rbs_wo_csi_rs = (rb_size - nr_rbs_w_csi_rs);
      // Actually, kprime + 1 sub-carriers are used by csi-rs. kprime can be 0 or 1 but nb_antennas_tx can be greater than 2.
      uint8_t subcarriers_used = get_nrUE_params()->nb_antennas_tx > 2 ? 2 : get_nrUE_params()->nb_antennas_tx;
      nb_re_pusch = nr_rbs_wo_csi_rs * freq_subcarriers_per_rb  + nr_rbs_w_csi_rs * (freq_subcarriers_per_rb - subcarriers_used);
    } else if (dmrs_symbol_flag == 1) {
      if ((ul_dmrs_symb_pos >> ((symbol + 1) % frame_parms->symbols_per_slot)) & 0x01)
        AssertFatal(1==0,"Double DMRS configuration is not yet supported\n");

      dmrs_symbol = symbol;

      if (dmrs_config_type == 0) {
        // if no data in dmrs cdm group is 1 only even REs have no data
        // if no data in dmrs cdm group is 2 both odd and even REs have no data
        nb_re_pusch = rb_size *(12 - (num_dmrs_cdm_grps_no_data*6));
      }
      else {
        nb_re_pusch = rb_size *(12 - (num_dmrs_cdm_grps_no_data*4));
      }
    } 
    else {
      nb_re_pusch = rb_size * NR_NB_SC_PER_RB;
    }

    pusch_vars->ul_valid_re_per_slot[symbol] = nb_re_pusch;
    int buffer_length = ceil_mod(pusch_vars->ul_valid_re_per_slot[symbol] , 16);
    int16_t llrss[nrOfLayers][ceil_mod(buffer_length * qam_mod_order, 64)];

    LOG_D(PHY, "symbol %d: nb_re_pusch %d, DMRS symbl used for Chest :%d \n", symbol, nb_re_pusch, dmrs_symbol);
    //----------------------------------------------------------
    //--------------------- RBs extraction ---------------------
    //----------------------------------------------------------
    if (nb_re_pusch > 0) {
      LOG_D(NR_PHY,"extract RBs : frame   %d, slot %d symbol %d nb_re_pusch %d\n", frame,slot,symbol, nb_re_pusch);
      nr_pssch_extract_rbs(rxFSz, rxdataF, ext_buffer_length, rxFext,chFext,pusch_vars, slot, symbol, dmrs_symbol, dmrs_symbol_flag, csi_rs_symbol_flag, bwp_start, rb_start, rb_size, nrOfLayers, num_dmrs_cdm_grps_no_data, dmrs_config_type, frame_parms, csi_params);

      //----------------------------------------------------------
      //--------------------- Channel Scaling --------------------
      //----------------------------------------------------------
      nr_slsch_scale_channel(ext_buffer_length,
		             chFext,
                             frame_parms,
                             symbol,
                             dmrs_symbol_flag,
			     nrOfLayers,
                             nb_re_pusch,
                             rb_size,
                             shift_ch_ext);

      if (cl_done == 0) {
        nr_slsch_channel_level(ext_buffer_length,
			       chFext,
                               frame_parms,
                               avg,
                               symbol,
                               nb_re_pusch,
                               nrOfLayers,
                               rb_size);

        avgs = 0;

        for (aatx=0;aatx<nrOfLayers;aatx++)
          for (aarx=0;aarx<frame_parms->nb_antennas_rx;aarx++)
            avgs = cmax(avgs,avg[aatx*frame_parms->nb_antennas_rx+aarx]);

        pusch_vars->log2_maxh = (log2_approx(avgs) >> 1) + ad_shift;
        if (pusch_vars->log2_maxh < 0) {
          pusch_vars->log2_maxh = 0;
        }
        cl_done = 1;
      }

      //----------------------------------------------------------
      //--------------------- Channel Compensation ---------------
      //----------------------------------------------------------
      //LOG_I(PHY, "Doing channel compensations log2_maxh %d, avgs %d (%d,%d)\n" ,pusch_vars->log2_maxh, avgs,avg[0], avg[1]);
      nr_pssch_channel_compensation(ext_buffer_length,
		      		    frame_parms->nb_antennas_rx,
		                    rxFext,
                                    chFext,
                                    rxF_ch_maga,
                                    rxF_ch_magb,
                                    rxF_ch_magc,
                                    pusch_vars->rxdataF_comp,
                                    nrOfLayers,
                                    rho,
                                    symbol,
                                    qam_mod_order,
                                    pusch_vars->log2_maxh);

      /*---------------------------------------------------------------------------------------------------- */
      /*--------------------  LLRs computation  -------------------------------------------------------------*/
      /*-----------------------------------------------------------------------------------------------------*/
      int sci1_offset=0;
      if (symbol <= pssch_pdu->pscch_numsym) { 
        pusch_vars->ul_valid_re_per_slot[symbol] -= sci1_re_per_symb;
        sci1_offset=sci1_re_per_symb;
      }
      AssertFatal(nrOfLayers == 1, "Only 1 SL layer for now\n");
      if (sci2_left>0){
          LOG_D(NR_PHY, "valid_re_per_slot[%d] %d\n", symbol, pusch_vars->ul_valid_re_per_slot[symbol]);
          int available_sci2_res_in_symb = pusch_vars->ul_valid_re_per_slot[symbol];
	  int slsch_res_in_symbol;
	  LOG_D(NR_PHY,"available_sci2_res_in_symb[%d] %d (sci1_re %d)\n",symbol,available_sci2_res_in_symb,sci1_re_per_symb);
	  int sci2_cnt_prev = sci2_cnt;
	  if (available_sci2_res_in_symb < sci2_left) {
	     sci2_cnt += available_sci2_res_in_symb; // take all of the PSSCH REs for SCI2
	     memcpy(&sci2_llrs[2*sci2_cnt_prev],&pusch_vars->rxdataF_comp[0][(symbol * (off + rb_size * NR_NB_SC_PER_RB))+sci1_offset],
	                       available_sci2_res_in_symb*sizeof(int32_t));
             sci2_left-= available_sci2_res_in_symb;
	     LOG_D(NR_PHY,"SCI2 taking all available REs. sci2_left %d\n",sci2_left);
	     pusch_vars->ul_valid_re_per_slot[symbol] = 0;
	     sci2_cnt_thissymb=available_sci2_res_in_symb;
	  }
	  else { // we finish SCI2 off here
	       memcpy(&sci2_llrs[2*sci2_cnt_prev],&pusch_vars->rxdataF_comp[0][(symbol * (off + rb_size * NR_NB_SC_PER_RB))+sci1_re_per_symb],
			         sci2_left*sizeof(int32_t));
	       slsch_res_in_symbol=available_sci2_res_in_symb-sci2_left;
	       LOG_D(NR_PHY, "SCI2 taking %d REs, SLSCH taking %d\n", sci2_left, slsch_res_in_symbol);
	       pusch_vars->ul_valid_re_per_slot[symbol]=slsch_res_in_symbol;
	       sci2_cnt_thissymb=sci2_left;
               sci2_left=0;
	       //for (int i=0;i<sci2_re;i++) LOG_I(NR_PHY,"sci2_llrs [%d] %d,%d\n",i,sci2_llrs[i<<1],sci2_llrs[1+(i<<1)]);
	       //unscramble the SCI2 payload
	       nr_pdcch_unscrambling((c16_t*)sci2_llrs, 1010,sci2_re,pssch_pdu->Nid,unscrambled_sci2_llrs);
	  //     for (int i=0;i<sci2_re;i++) LOG_I(NR_PHY,"sci2_llrs [%d] %d,%d\n",i,unscrambled_sci2_llrs[i<<1],unscrambled_sci2_llrs[1+(i<<1)]);

	       uint64_t sci_estimation[2]={0};
	       uint16_t dummy;
               uint16_t crc = polar_decoder_int16(unscrambled_sci2_llrs,
                                                  sci_estimation,
						  &dummy,
                                                  1,
                                                  NR_POLAR_SCI2_MESSAGE_TYPE,
                                                  pssch_pdu->sci2_len,
                                                  sci2_re);
	       // send SCI indication with SCI2 payload and get SLSCH information if CRC is OK
	       LOG_D(NR_PHY,"SCI indication (crc %x)\n",crc);
	       if (crc==0) ue->SL_UE_PHY_PARAMS.pssch.rx_sci2_ok++;   
	       else        ue->SL_UE_PHY_PARAMS.pssch.rx_sci2_errors++;   
	       sl_nr_sci_indication_t sci_ind={0}; 
               sci_ind.sfn = frame;
               sci_ind.slot = slot;
               sci_ind.sensing_result = 0;
               sci_ind.pssch_rsrp = 0; // setting this flag to zero; measuring from sci1
               sci_ind.sci_pdu[sci_ind.number_of_SCIs].sci_format_type = SL_SCI_FORMAT_2_ON_PSSCH;
               sci_ind.sci_pdu[sci_ind.number_of_SCIs].subch_index = 0;
               sci_ind.sci_pdu[sci_ind.number_of_SCIs].pscch_rsrp = 0; // setting this flag to zero; measuring from sci1
               sci_ind.sci_pdu[sci_ind.number_of_SCIs].sci_payloadlen = pssch_pdu->sci2_len;
               sci_ind.sci_pdu[sci_ind.number_of_SCIs].Nid = dummy&65535;
 
               memcpy(sci_ind.sci_pdu[sci_ind.number_of_SCIs].sci_payloadBits,&sci_estimation,8);
               sci_ind.number_of_SCIs++;
	       nr_sidelink_indication_t sl_indication;
	       nr_fill_sl_indication(&sl_indication, NULL, &sci_ind, proc, ue, phy_data);
	       ue->if_inst->sl_indication(&sl_indication);
	       LOG_D(NR_PHY,"Returning from SCI2 SL indication\n");
	  } //sci2_res_in_symbol
      } // (sci2 REs to handle)	
      LOG_D(NR_PHY, "symbol %d: PSSCH REs %d (sci1 %d,sci2 %d)\n", symbol, pusch_vars->ul_valid_re_per_slot[symbol], sci1_offset, sci2_cnt_thissymb);
      for (aatx=0; aatx < nrOfLayers; aatx++) {
        if ((sci1_offset > 0 || sci2_cnt_thissymb > 0) && (qam_mod_order > 2)) {
          memset(temp_symbol, 0, (sci1_offset + sci2_cnt_thissymb) * sizeof(c16_t));
          memcpy(temp_symbol + sci1_offset + sci2_cnt_thissymb,
                &pusch_vars->rxdataF_comp[aatx * frame_parms->nb_antennas_rx][symbol * (off + rb_size * NR_NB_SC_PER_RB) + sci1_offset+sci2_cnt_thissymb],
                (rb_size * NR_NB_SC_PER_RB - (sci1_offset + sci2_cnt_thissymb)) * sizeof(c16_t));
          nr_slsch_compute_llr(temp_symbol,
                               rxF_ch_maga[aatx * frame_parms->nb_antennas_rx],
                               rxF_ch_magb[aatx * frame_parms->nb_antennas_rx],
                               rxF_ch_magc[aatx * frame_parms->nb_antennas_rx],
                               temp_llr,
                               rb_size * NR_NB_SC_PER_RB,
                               symbol,
                               qam_mod_order);
          if (nrOfLayers != 1) {
                  memcpy(&llrss[aatx][rxdataF_ext_offset * qam_mod_order],
                         temp_llr + (sci1_offset + sci2_cnt_thissymb) * qam_mod_order,
                         (rb_size * NR_NB_SC_PER_RB - (sci1_offset + sci2_cnt_thissymb)) * 2 * qam_mod_order);
	  }
	    
        } else {
              nr_slsch_compute_llr(&pusch_vars->rxdataF_comp[aatx * frame_parms->nb_antennas_rx][symbol * (off + rb_size * NR_NB_SC_PER_RB) + sci1_offset + sci2_cnt_thissymb],
                                rxF_ch_maga[aatx * frame_parms->nb_antennas_rx],
                                rxF_ch_magb[aatx * frame_parms->nb_antennas_rx],
                                rxF_ch_magc[aatx * frame_parms->nb_antennas_rx],
                                &llrss[aatx][rxdataF_ext_offset * qam_mod_order],
                                pusch_vars->ul_valid_re_per_slot[symbol],
                                symbol,
                                qam_mod_order);
	} // nrOfLayers !=1
      } // aatx
    /*
      if (nrOfLayers > 1) {
// layer demapping
      }
      */
	// Do unscrambling here
      int16_t *llr16 = (int16_t*)&llrs[rxdataF_ext_offset * qam_mod_order * nrOfLayers];
      int16_t *s = scramblingSequence + rxdataF_ext_offset * qam_mod_order * nrOfLayers;
      const int end = pusch_vars->ul_valid_re_per_slot[symbol] * qam_mod_order * nrOfLayers;
      for (int i = 0; i < end; i++)
        llr16[i] = llrss[0][(rxdataF_ext_offset * qam_mod_order* nrOfLayers) + i] * s[i];
      rxdataF_ext_offset += pusch_vars->ul_valid_re_per_slot[symbol];
    } // nb_re_pusch > 0
  } // symbol loop
}
