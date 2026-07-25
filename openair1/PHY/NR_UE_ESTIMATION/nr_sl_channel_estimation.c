/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.0  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */


#include <string.h>

#include "PHY/sse_intrin.h"
#include "PHY/defs_gNB.h"
#include "PHY/NR_REFSIG/nr_refsig.h"
#include "PHY/NR_REFSIG/sl_refsig_defs.h"
#include "PHY/NR_REFSIG/dmrs_nr.h"
#include "PHY/NR_UE_ESTIMATION/filt16a_32.h"
#include "PHY/nr_phy_common/inc/nr_phy_common.h"
#include "common/utils/nr/nr_common.h"
#include "executables/softmodem-common.h"


//#define DEBUG_CH
//#define DEBUG_PUSCH

#define NO_INTERP 1
#define dBc(x,y) (dB_fixed(((int32_t)(x))*(x) + ((int32_t)(y))*(y)))

__attribute__((always_inline)) inline c16_t c32x16cumulVectVectWithSteps(c16_t *in1,
                                                                         int *offset1,
                                                                         const int step1,
                                                                         c16_t *in2,
                                                                         int *offset2,
                                                                         const int step2,
                                                                         const int modulo2,
                                                                         const int N) {

  int localOffset1=*offset1;
  int localOffset2=*offset2;
  c32_t cumul={0};
  for (int i=0; i<N; i++) {
    cumul=c32x16maddShift(in1[localOffset1], in2[localOffset2], cumul, 15);
    localOffset1+=step1;
    localOffset2= (localOffset2 + step2) % modulo2;
  }
  *offset1=localOffset1;
  *offset2=localOffset2;
  return c16x32div(cumul, N);
}

int nr_pssch_channel_estimation(PHY_VARS_NR_UE *ue,
                                int rxFSz,
                                c16_t rxdataF[][rxFSz],
                                unsigned char Ns,
                                unsigned short p,
                                unsigned char symbol,
                                int ul_id,
                                unsigned short bwp_start_subcarrier,
                                sl_nr_rx_config_pssch_sci_pdu_t *pssch_pdu,
                                int *max_ch,
                                uint32_t *nvar) {

  c16_t pilot[3280] __attribute__((aligned(32)));
  const int chest_freq = ue->chest_freq;

#ifdef DEBUG_CH
  FILE *debug_ch_est;
  debug_ch_est = fopen("debug_ch_est.txt","w");
#endif
  NR_gNB_PUSCH *pusch_vars = &ue->pssch_vars[ul_id];
  c16_t **ul_ch_estimates = (c16_t **)pusch_vars->ul_ch_estimates;
  const int symbolSize = ue->SL_UE_PHY_PARAMS.sl_frame_params.ofdm_symbol_size;
  const int soffset = 0;
  const int nushift = (p>>1)&1;
  int ch_offset     = symbolSize*symbol;
  const int symbol_offset = symbolSize*symbol;

  const int k0 = bwp_start_subcarrier;
  const int nb_rb_pusch = pssch_pdu->subchannel_size*pssch_pdu->l_subch;

  LOG_W(PHY, "In %s: ch_offset %d, soffset %d, symbol_offset %d, OFDM size %d, Ns = %d, k0 = %d, symbol %d\n",
        __FUNCTION__,
        ch_offset, soffset,
        symbol_offset,
        symbolSize,
        Ns,
        k0,
        symbol);

  //------------------generate DMRS------------------//

  // compute gold sequence based on Nid from SCI1A
  int nb_re = ue->SL_UE_PHY_PARAMS.sl_frame_params.N_RB_UL*12;
  uint32_t pssch_dmrs[(nb_re>>5)+1];
  nr_init_pssch_dmrs_oneshot(&ue->SL_UE_PHY_PARAMS.sl_frame_params,pssch_pdu->Nid,pssch_dmrs,Ns,symbol);
  float beta_dmrs_pusch = get_beta_dmrs(pssch_pdu->num_layers, pusch_dmrs_type1);
  int16_t dmrs_scaling = (1 / beta_dmrs_pusch) * (1 << 14);
  nr_pusch_dmrs_rx(NORMAL, pssch_dmrs, (c16_t *)pilot, (1000+p), 0, nb_rb_pusch,
                   (pssch_pdu->startrb)*NR_NB_SC_PER_RB, 0, dmrs_scaling);

  //------------------------------------------------//

#ifdef DEBUG_PUSCH

  for (int i = 0; i < (6 * nb_rb_pusch); i++) {
    LOG_I(PHY, "In %s: %d + j*(%d)\n", __FUNCTION__, pilot[i].r,pilot[i].i);
  }

#endif

  int nest_count = 0;
  uint64_t noise_amp2 = 0;
  c16_t ul_ls_est[symbolSize] __attribute__((aligned(32)));
  memset(ul_ls_est, 0, sizeof(c16_t) * symbolSize);
  delay_t delay;
  memset(&delay, 0, sizeof(delay));
  NR_DL_FRAME_PARMS *fp = &ue->SL_UE_PHY_PARAMS.sl_frame_params;
  int nrx = fp->nb_antennas_rx;
  c16_t ch_estimates_time[symbolSize] __attribute__((aligned(32)));
  for (int aarx=0; aarx<nrx; aarx++) {
    c16_t *rxdataF2 = (c16_t *)&rxdataF[aarx][symbol_offset];
    c16_t *ul_ch = &ul_ch_estimates[p*nrx+aarx][ch_offset];

    memset(ul_ch,0,sizeof(*ul_ch)*symbolSize);
#ifdef DEBUG_PUSCH
    LOG_I(PHY, "In %s symbol_offset %d, nushift %d\n", __FUNCTION__, symbol_offset, nushift);
    LOG_I(PHY, "In %s ch est pilot, N_RB_UL %d\n", __FUNCTION__, fp->N_RB_UL);
    LOG_I(PHY, "In %s bwp_start_subcarrier %d, k0 %d, first_carrier %d, nb_rb_pusch %d\n", __FUNCTION__, bwp_start_subcarrier, k0, fp->first_carrier_offset, nb_rb_pusch);
    LOG_I(PHY, "In %s ul_ch addr %p nushift %d\n", __FUNCTION__, ul_ch, nushift);
#endif

    if (chest_freq == 0) {
      c16_t *pil   = pilot;
      int re_offset = k0;
      LOG_D(PHY,"PSSCH estimation DMRS type 1, Freq-domain interpolation");
      // For configuration type 1: k = 4*n + 2*k' + delta,
      // where k' is 0 or 1, and delta is in Table 6.4.1.1.3-1 from TS 38.211
      int pilot_cnt = 0;
      int delta = 0; // nr_pusch_dmrs_delta(pusch_dmrs_type1, p);

      // DEBUG: coherence of the per-pilot LS estimates. coherence = |sum(ch)|^2 /
      // (N * sum(|ch|^2)) is ~1 when the DMRS correlates coherently (flat channel,
      // correct reference) and ~1/N (≈0) when the LS estimates have random phase
      // (wrong Nid / wrong DMRS positions / uncompensated rotation).
      int64_t coh_r = 0, coh_i = 0;
      uint64_t coh_e = 0;
      int coh_n = 0;

      for (int n = 0; n < 3 * nb_rb_pusch; n++) {
        // LS estimation
        c32_t ch = {0};

        for (int k_line = 0; k_line <= 1; k_line++) {
          re_offset = (k0 + (n << 2) + (k_line << 1) + delta) % symbolSize;
          ch = c32x16maddShift(*pil, rxdataF2[soffset + re_offset], ch, 16);
          pil++;
        }

        c16_t ch16 = {.r = (int16_t)ch.r, .i = (int16_t)ch.i};
        coh_r += ch16.r;
        coh_i += ch16.i;
        coh_e += (uint64_t)((int64_t)ch16.r * ch16.r + (int64_t)ch16.i * ch16.i);
        coh_n++;
        *max_ch = max(*max_ch, max(abs(ch.r), abs(ch.i)));
        for (int k = pilot_cnt << 1; k < (pilot_cnt << 1) + 4; k++) {
          ul_ls_est[k] = ch16;
        }
        pilot_cnt += 2;
      }

      double coh = (coh_e && coh_n) ? ((double)coh_r * coh_r + (double)coh_i * coh_i) / ((double)coh_n * coh_e) : 0.0;
      LOG_I(NR_PHY,
            "PSSCH chest sym %d p %d aarx %d: Nid=%d max_ch=%d pilots=%d coherence=%.3f (|sum|^2=%.0f, N*sum|.|^2=%.0f)\n",
            symbol, p, aarx, pssch_pdu->Nid, *max_ch, coh_n, coh,
            (double)coh_r * coh_r + (double)coh_i * coh_i, (double)coh_n * coh_e);

      freq2time(symbolSize, (int16_t *)ul_ls_est, (int16_t*)ch_estimates_time);

      nr_est_delay(symbolSize, ul_ls_est, ch_estimates_time, &delay);
      int pusch_delay = delay.est_delay;
      int delay_idx = get_delay_idx(pusch_delay, MAX_DELAY_COMP);
      c16_t *ul_delay_table = fp->delay_table[delay_idx];

#ifdef DEBUG_PUSCH
      LOG_I(NR_PHY,"Estimated delay = %i\n", pusch_delay >> 1);
#endif

      pilot_cnt = 0;
      for (int n = 0; n < 3*nb_rb_pusch; n++) {

        // Channel interpolation
        for (int k_line = 0; k_line <= 1; k_line++) {

          // Apply delay
          int k = pilot_cnt << 1;
          c16_t ch16 = c16mulShift(ul_ls_est[k], ul_delay_table[k], 8);

#ifdef DEBUG_PUSCH
          re_offset = (k0 + (n << 2) + (k_line << 1)) % symbolSize;
          c16_t *rxF = &rxdataF2[soffset + re_offset];
          LOG_I(NR_PHY,"pilot %4d: ul_delay` -> (%6d,%6d), rxF -> (%4d,%4d), ch -> (%4d,%4d)\n",
                 pilot_cnt, ul_delay_table[k].r, ul_delay_table[k].i, rxF->r, rxF->i, ch16.r, ch16.i);
#endif

          if (pilot_cnt == 0) {
            c16multaddVectRealComplex(filt16_ul_p0, &ch16, ul_ch, 16);
          } else if (pilot_cnt == 1 || pilot_cnt == 2) {
            c16multaddVectRealComplex(filt16_ul_p1p2, &ch16, ul_ch, 16);
          } else if (pilot_cnt == (6 * nb_rb_pusch - 1)) {
            c16multaddVectRealComplex(filt16_ul_last, &ch16, ul_ch, 16);
          } else {
            c16multaddVectRealComplex(filt16_ul_middle, &ch16, ul_ch, 16);
            if (pilot_cnt % 2 == 0) {
              ul_ch += 4;
            }
          }

          pilot_cnt++;
        }
      }

      // Revert delay
      pilot_cnt = 0;
      ul_ch = &ul_ch_estimates[p * nrx + aarx][ch_offset];
      int inv_delay_idx = get_delay_idx(-pusch_delay, MAX_DELAY_COMP);
      c16_t *ul_inv_delay_table = fp->delay_table[inv_delay_idx];
      for (int n = 0; n < 3 * nb_rb_pusch; n++) {
        for (int k_line = 0; k_line <= 1; k_line++) {
          int k = pilot_cnt << 1;
          ul_ch[k] = c16mulShift(ul_ch[k], ul_inv_delay_table[k], 8);
          ul_ch[k + 1] = c16mulShift(ul_ch[k + 1], ul_inv_delay_table[k + 1], 8);
          noise_amp2 += c16amp2(c16sub(ul_ls_est[k], ul_ch[k]));
          noise_amp2 += c16amp2(c16sub(ul_ls_est[k + 1], ul_ch[k + 1]));

#ifdef DEBUG_PUSCH
          re_offset = (k0 + (n << 2) + (k_line << 1)) % symbolSize;
          c16_t *rxF = &rxdataF2[soffset + re_offset];
          LOG_I(NR_PHY,"ch -> (%4d,%4d), ch_inter -> (%4d,%4d)\n", ul_ls_est[k].r, ul_ls_est[k].i, ul_ch[k].r, ul_ch[k].i);
#endif
          pilot_cnt++;
          nest_count += 2;
        }
      }

    } else { // no frequency-domain linear interpolation: average LS channel estimates of 6 DMRS REs per PRB
      LOG_D(PHY,"PSSCH estimation DMRS type 1, no Freq-domain interpolation\n");
      c16_t *rxF   =  &rxdataF2[soffset + nushift];
      int pil_offset = 0;
      int re_offset = k0;
      c16_t ch;

      // First PRB
      ch=c32x16cumulVectVectWithSteps(pilot, &pil_offset, 1, rxF, &re_offset, 2, symbolSize, 6);

#if NO_INTERP
      for (c16_t *end=ul_ch+12; ul_ch<end; ul_ch++)
        *ul_ch=ch;
#else
      c16multaddVectRealComplex(filt8_avlip0, &ch, ul_ch, 8);
      ul_ch += 8;
      c16multaddVectRealComplex(filt8_avlip1, &ch, ul_ch, 8);
      ul_ch += 8;
      c16multaddVectRealComplex(filt8_avlip2, &ch, ul_ch, 8);
      ul_ch -= 12;
#endif

      for (int pilot_cnt=6; pilot_cnt<6*(nb_rb_pusch-1); pilot_cnt += 6) {
        ch=c32x16cumulVectVectWithSteps(pilot, &pil_offset, 1, rxF, &re_offset, 2, symbolSize, 6);
        *max_ch = max(*max_ch, max(abs(ch.r), abs(ch.i)));

#if NO_INTERP
      for (c16_t *end=ul_ch+12; ul_ch<end; ul_ch++)
          *ul_ch=ch;
#else
        ul_ch[3].r += (ch.r * 1365)>>15; // 1/12*16384
        ul_ch[3].i += (ch.i * 1365)>>15; // 1/12*16384

        ul_ch += 4;
        c16multaddVectRealComplex(filt8_avlip3, &ch, ul_ch, 8);
        ul_ch += 8;
        c16multaddVectRealComplex(filt8_avlip4, &ch, ul_ch, 8);
        ul_ch += 8;
        c16multaddVectRealComplex(filt8_avlip5, &ch, ul_ch, 8);
        ul_ch -= 8;
#endif
      }
      // Last PRB
      ch=c32x16cumulVectVectWithSteps(pilot, &pil_offset, 1, rxF, &re_offset, 2, symbolSize, 6);

#if NO_INTERP
      for (c16_t *end=ul_ch+12; ul_ch<end; ul_ch++)
        *ul_ch=ch;
#else
      ul_ch[3].r += (ch.r * 1365)>>15; // 1/12*16384
      ul_ch[3].i += (ch.i * 1365)>>15; // 1/12*16384

      ul_ch += 4;
      c16multaddVectRealComplex(filt8_avlip3, &ch, ul_ch, 8);
      ul_ch += 8;
      c16multaddVectRealComplex(filt8_avlip6, &ch, ul_ch, 8);
#endif
    }

#ifdef DEBUG_PUSCH
    ul_ch = &ul_ch_estimates[p * ue->SL_UE_PHY_PARAMS.sl_frame_params.nb_antennas_rx + aarx][ch_offset];
    for (int idxP = 0; idxP < ceil((float)nb_rb_pusch * 12 / 8); idxP++) {
      for (int idxI = 0; idxI < 8; idxI++) {
          LOG_I(NR_PHY,"%d\t%d\t", ul_ch[idxP * 8 + idxI].r, ul_ch[idxP * 8 + idxI].i);
      }
      LOG_I(NR_PHY,"%d\n", idxP);
    }
#endif

  }

#ifdef DEBUG_CH
  fclose(debug_ch_est);
#endif

  if (nvar && nest_count > 0) {
    *nvar = (uint32_t)(noise_amp2 / nest_count);
  }

  return 0;
}

uint32_t calc_power(const int16_t *x, const uint32_t size) {
  int64_t sum_x = 0;
  int64_t sum_x2 = 0;
  for(int k = 0; k<size; k++) {
    sum_x = sum_x + x[k];
    sum_x2 = sum_x2 + x[k]*x[k];
  }

  return sum_x2/size - (sum_x/size)*(sum_x/size);
}
