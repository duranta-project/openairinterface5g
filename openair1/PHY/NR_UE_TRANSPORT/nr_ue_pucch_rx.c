/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
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

/*! \file PHY/NR_TRANSPORT/pucch_rx.c
 * \brief Top-level routines for decoding the PUCCH physical channel
 * \author A. Mico Pereperez, Padarthi Naga Prasanth, Francesco Mani, Raymond Knopp
 * \date 2020
 * \version 0.2
 * \company Eurecom
 * \email:
 * \note
 * \warning
 */
#include<stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "PHY/impl_defs_nr.h"
#include "PHY/defs_nr_common.h"
#include "PHY/defs_gNB.h"
#include "PHY/sse_intrin.h"
#include "PHY/NR_UE_TRANSPORT/pucch_nr.h"
#include <openair1/PHY/CODING/nrSmallBlock/nr_small_block_defs.h>
#include "PHY/NR_TRANSPORT/nr_transport_common_proto.h"
#include "PHY/NR_TRANSPORT/nr_transport_proto.h"
#include "PHY/NR_REFSIG/nr_refsig.h"
#include "common/utils/LOG/log.h"
#include "common/utils/LOG/vcd_signal_dumper.h"
#include "nfapi/oai_integration/vendor_ext.h"
#include "nfapi/oai_integration/vendor_ext.h"
#include "executables/nr-uesoftmodem.h"
#include "T.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_proto_ue.h"

//#define DEBUG_NR_PUCCH_RX 1

int get_pucch0_cs_lut_index(PHY_VARS_NR_UE *ue, nfapi_nr_pucch_pdu_t* pucch_pdu) {

  int i = 0;

#ifdef DEBUG_NR_PUCCH_RX
  LOG_I(NR_PHY, "getting index for LUT with %d entries, Nid %d\n", ue->pucch0_lut.nb_id, pucch_pdu->hopping_id);
#endif

  for (i=0; i<ue->pucch0_lut.nb_id; i++) {
    if (ue->pucch0_lut.Nid[i] == pucch_pdu->hopping_id) break;
  }
#ifdef DEBUG_NR_PUCCH_RX
  LOG_I(NR_PHY, "found index %d\n", i);
#endif
  if (i < ue->pucch0_lut.nb_id) return(i);

#ifdef DEBUG_NR_PUCCH_RX
  LOG_I(NR_PHY, "Initializing PUCCH0 LUT index %i with Nid %d\n", i, pucch_pdu->hopping_id);
#endif
  // initialize
  ue->pucch0_lut.Nid[ue->pucch0_lut.nb_id] = pucch_pdu->hopping_id;
  for (int slot=0; slot<10<<pucch_pdu->subcarrier_spacing; slot++)
    for (int symbol=0; symbol<14; symbol++)
      ue->pucch0_lut.lut[ue->pucch0_lut.nb_id][slot][symbol] = (int)floor(nr_cyclic_shift_hopping(pucch_pdu->hopping_id, 0, 0, 0, symbol, slot) / 0.5235987756);
  ue->pucch0_lut.nb_id++;
  return(ue->pucch0_lut.nb_id-1);
}

int8_t nr_ue_decode_psfch0(PHY_VARS_NR_UE *ue,
                          int frame,
                          int slot,
                          c16_t rxdataF[][ue->SL_UE_PHY_PARAMS.sl_frame_params.samples_per_slot_wCP],
                          const sl_nr_tx_rx_config_psfch_pdu_t *psfch_pdu) {
  int8_t ack_nack_rcvd = -1;
  nfapi_nr_pucch_pdu_t pucch_pdu;
  pucch_pdu.freq_hop_flag = psfch_pdu->freq_hop_flag;
  pucch_pdu.group_hop_flag = psfch_pdu->group_hop_flag;
  pucch_pdu.sequence_hop_flag = psfch_pdu->sequence_hop_flag;
  pucch_pdu.second_hop_prb = psfch_pdu->second_hop_prb;
  pucch_pdu.nr_of_symbols = psfch_pdu->nr_of_symbols;
  pucch_pdu.start_symbol_index = psfch_pdu->start_symbol_index;
  pucch_pdu.hopping_id = psfch_pdu->hopping_id;
  pucch_pdu.prb_start = psfch_pdu->prb;
  pucch_pdu.prb_size = 1;
  pucch_pdu.bwp_start = psfch_pdu->sl_bwp_start;
  pucch_pdu.initial_cyclic_shift = psfch_pdu->initial_cyclic_shift;
  pucch_pdu.bit_len_harq = psfch_pdu->bit_len_harq;
  pucch_pdu.sr_flag = 0;
  pucch_pdu.subcarrier_spacing = 1;
  ack_nack_rcvd = nr_ue_decode_pucch0(ue,
                                      frame,
                                      slot,
                                      rxdataF,
                                      NULL,
                                      &pucch_pdu);
  return ack_nack_rcvd;
}

int8_t nr_ue_decode_pucch0(PHY_VARS_NR_UE *ue,
                          int frame,
                          int slot,
                          c16_t rxdataF[][ue->SL_UE_PHY_PARAMS.sl_frame_params.samples_per_slot_wCP],
                          nfapi_nr_uci_pucch_pdu_format_0_1_t *uci_pdu,
                          nfapi_nr_pucch_pdu_t *pucch_pdu)
{
  NR_DL_FRAME_PARMS *frame_parms = get_softmodem_params()->sl_mode ? &ue->SL_UE_PHY_PARAMS.sl_frame_params : &ue->frame_parms;

  int soffset = 0;
  int nr_sequences;
  const uint8_t *mcs;
  AssertFatal(pucch_pdu->sr_flag == 0, "SR flag MUST be 0 in SL\n");
  AssertFatal(pucch_pdu->bit_len_harq == 0 ||  pucch_pdu->bit_len_harq == 1, "Invalid value for bit_len_harq %d\n", pucch_pdu->bit_len_harq);
  if(pucch_pdu->bit_len_harq == 0){
    mcs = table1_mcs;
    nr_sequences = 1;
  }
  else if(pucch_pdu->bit_len_harq == 1){
    mcs = table1_mcs;
    nr_sequences = 4 >> 1;
  }

  AssertFatal(nr_sequences == 1 || nr_sequences == 2, "nr_sequences must be either 1 or 2, nr_sequences %d\n", nr_sequences);

  LOG_D(PHY, "%s pucch0: nr_symbols %d, start_symbol %d, prb_start %d, second_hop_prb %d, group_hop_flag %d, sequence_hop_flag %d, O_ACK %d, O_SR %d, mcs %d, initial_cyclic_shift %d, subcarrier_spacing %d\n",
        __FUNCTION__,
        pucch_pdu->nr_of_symbols,
        pucch_pdu->start_symbol_index,
        pucch_pdu->prb_start,
        pucch_pdu->second_hop_prb,
        pucch_pdu->group_hop_flag,
        pucch_pdu->sequence_hop_flag,
        pucch_pdu->bit_len_harq,
        pucch_pdu->sr_flag,
        mcs[0],
        pucch_pdu->initial_cyclic_shift,
        pucch_pdu->subcarrier_spacing);

  int cs_ind = get_pucch0_cs_lut_index(ue, pucch_pdu);
  /*
   * Implement TS 38.211 Subclause 6.3.2.3.1 Sequence generation
   *
   */
  /*
   * Defining cyclic shift hopping TS 38.211 Subclause 6.3.2.2.2
   */
  /*
   * in TS 38.213 Subclause 9.2.1 it is said that:
   * for PUCCH format 0 or PUCCH format 1, the index of the cyclic shift
   * is indicated by higher layer parameter PUCCH-F0-F1-initial-cyclic-shift
   */

  /*
   * Implementing TS 38.211 Subclause 6.3.2.3.1, the sequence x(n) shall be generated according to:
   * x(l*12+n) = r_u_v_alpha_delta(n)
   */
  // the value of u,v (delta always 0 for PUCCH) has to be calculated according to TS 38.211 Subclause 6.3.2.2.1
  uint8_t u[2] = {0}, v[2] = {0};

  // // x_n contains the sequence r_u_v_alpha_delta(n)
  int n, i;
  int prb_offset[2] = {pucch_pdu->bwp_start + pucch_pdu->prb_start, pucch_pdu->bwp_start + pucch_pdu->prb_start};

  pucch_GroupHopping_t pucch_GroupHopping = pucch_pdu->group_hop_flag + (pucch_pdu->sequence_hop_flag << 1);
  nr_group_sequence_hopping(pucch_GroupHopping,
                            pucch_pdu->hopping_id,
                            0,
                            slot,
                            &u[0],
                            &v[0]); // calculating u and v value first hop
  LOG_D(PHY, "pucch0: u %d, v %d\n", u[0], v[0]);

  if (pucch_pdu->freq_hop_flag == 1) {
    nr_group_sequence_hopping(pucch_GroupHopping,
                              pucch_pdu->hopping_id,
                              1,
                              slot,
                              &u[1],
                              &v[1]); // calculating u and v value second hop
    LOG_D(PHY, "pucch0 second hop: u %d, v %d\n", u[1], v[1]);
    prb_offset[1] = pucch_pdu->bwp_start + pucch_pdu->second_hop_prb;
  }

  AssertFatal(pucch_pdu->nr_of_symbols < 3, "nr_of_symbols %d not allowed\n", pucch_pdu->nr_of_symbols);
  uint32_t re_offset[2] = {0};

  const int16_t *x_re[2], *x_im[2];
  x_re[0] = table_5_2_2_2_2_Re[u[0]];
  x_im[0] = table_5_2_2_2_2_Im[u[0]];
  x_re[1] = table_5_2_2_2_2_Re[u[1]];
  x_im[1] = table_5_2_2_2_2_Im[u[1]];

  c64_t xr[frame_parms->nb_antennas_rx][pucch_pdu->nr_of_symbols][12]  __attribute__((aligned(32)));
  int64_t xrtmag = 0, xrtmag_next = 0;
  uint8_t maxpos = 0;
  uint8_t index = 0;
  LOG_D(NR_PHY, "prb_size %d\n", pucch_pdu->prb_size);
  int nb_re_pucch = 12*pucch_pdu->prb_size;  // prb size is 1
  int32_t rp[frame_parms->nb_antennas_rx][pucch_pdu->nr_of_symbols][nb_re_pucch];
  memset(rp, 0, sizeof(rp));
  int32_t *tmp_rp = NULL;

  for (int l=0; l<pucch_pdu->nr_of_symbols; l++) {
    uint8_t l2 = l + pucch_pdu->start_symbol_index;

    re_offset[l] = (12 * prb_offset[l]) + frame_parms->first_carrier_offset;
    if (re_offset[l] >= frame_parms->ofdm_symbol_size)
      re_offset[l] -= frame_parms->ofdm_symbol_size;

    for (int aa = 0; aa < frame_parms->nb_antennas_rx; aa++) {
      LOG_D(NR_PHY, "soffset %i, soffset + l2*frame_parms->ofdm_symbol_size %i %i re_offset[%d] %i\n",
           soffset, soffset + l2*frame_parms->ofdm_symbol_size,
           (soffset + l2*frame_parms->ofdm_symbol_size + nb_re_pucch), l, re_offset[l]);
      for (int z = soffset + l2*frame_parms->ofdm_symbol_size + re_offset[l]; z < (soffset + l2*frame_parms->ofdm_symbol_size + re_offset[l] + nb_re_pucch); z++)
        LOG_D(NR_PHY, "%4d.%2d z %d rxdataF (%d,%d)\n", frame, slot, z, rxdataF[aa][z].r, rxdataF[aa][z].i);
      tmp_rp = (int32_t *)&rxdataF[aa][soffset + l2 * frame_parms->ofdm_symbol_size];
      if(re_offset[l] + nb_re_pucch > frame_parms->ofdm_symbol_size) {
        int neg_length = frame_parms->ofdm_symbol_size - re_offset[l];
        int pos_length = nb_re_pucch - neg_length;
        memcpy1((void*)rp[aa][l], (void*)&tmp_rp[re_offset[l]], neg_length*sizeof(int32_t));
        memcpy1((void*)&rp[aa][l][neg_length], (void*)tmp_rp, pos_length*sizeof(int32_t));
      }
      else
        memcpy1((void*)rp[aa][l], (void*)&tmp_rp[re_offset[l]], nb_re_pucch*sizeof(int32_t));

      c16_t *r = (c16_t*)&rp[aa][l];

      for (n=0; n<nb_re_pucch; n++) {
        xr[aa][l][n].r = (int32_t)x_re[l][n] * r[n].r + (int32_t)x_im[l][n] * r[n].i;
        xr[aa][l][n].i = (int32_t)x_re[l][n] * r[n].i - (int32_t)x_im[l][n] * r[n].r;
#ifdef DEBUG_NR_PUCCH_RX
        LOG_I(NR_PHY, "x (%d,%d), r%d.%d (%d,%d), xr (%lld,%lld)\n",
               x_re[l][n], x_im[l][n], l2, re_offset[l], r[n].r, r[n].i, xr[aa][l][n].r, xr[aa][l][n].i);
#endif

      }
    }
  }

  int seq_index = 0;
  int64_t temp;

  for(i=0; i<nr_sequences; i++) {
    c64_t corr[frame_parms->nb_antennas_rx][2];
    for (int aa=0; aa<frame_parms->nb_antennas_rx; aa++) {
      for (int l=0; l<pucch_pdu->nr_of_symbols; l++) {
        seq_index = (pucch_pdu->initial_cyclic_shift+
                     mcs[i]+
                     ue->pucch0_lut.lut[cs_ind][slot][l+pucch_pdu->start_symbol_index])%12;
#ifdef DEBUG_NR_PUCCH_RX
        LOG_I(NR_PHY, "PUCCH symbol %d seq %d, seq_index %d, mcs %d , slot %d, cs_ind %d\n",
              l, i, seq_index, mcs[i], slot, cs_ind);
#endif
        corr[aa][l] = (c64_t){0};
        for (n = 0; n < 12; n++) {
          corr[aa][l].r += xr[aa][l][n].r * idft12_re[seq_index][n] + xr[aa][l][n].i * idft12_im[seq_index][n];
          corr[aa][l].i += xr[aa][l][n].r * idft12_im[seq_index][n] - xr[aa][l][n].i * idft12_re[seq_index][n];
        }
        corr[aa][l].r >>= 31;
        corr[aa][l].i >>= 31;
      }
    }
    LOG_D(PHY,"PUCCH IDFT[%d/%d] = (%ld,%ld)=>%f\n",
          mcs[i], seq_index, corr[0][0].r, corr[0][0].i,
          10*log10((double)squaredMod(corr[0][0])));
    if (pucch_pdu->nr_of_symbols == 2)
       LOG_D(PHY,"PUCCH 2nd symbol IDFT[%d/%d] = (%ld,%ld)=>%f\n",
             mcs[i], seq_index, corr[0][1].r, corr[0][1].i,
             10*log10((double)squaredMod(corr[0][1])));
    if (pucch_pdu->freq_hop_flag == 0) {
       if (pucch_pdu->nr_of_symbols == 1) {// non-coherent correlation
          temp = 0;
          for (int aa=0; aa<frame_parms->nb_antennas_rx; aa++)
            temp += squaredMod(corr[aa][0]);
        } else {
          temp = 0;
          for (int aa=0; aa<frame_parms->nb_antennas_rx; aa++) {
            c64_t corr2;
            csum(corr2, corr[aa][0], corr[aa][1]);
            // coherent combining of 2 symbols and then complex modulus for single-frequency case
            temp += corr2.r*corr2.r + corr2.i*corr2.i;
          }
        }
    } else if (pucch_pdu->freq_hop_flag == 1) {
      // full non-coherent combining of 2 symbols for frequency-hopping case
      temp = 0;
      for (int aa=0; aa<frame_parms->nb_antennas_rx; aa++)
        temp += squaredMod(corr[aa][0]) + squaredMod(corr[aa][1]);
    }
    else AssertFatal(1==0,"shouldn't happen\n");
    LOG_D(PHY, "Sequence %d temp %ld vs. xrtmag %ld xrtmag_next %ld, slot %d rx atnennas %u\n",
          i, temp, xrtmag, xrtmag_next, slot, frame_parms->nb_antennas_rx);
    if (temp > xrtmag) {
      xrtmag_next = xrtmag;
      xrtmag = temp;
      LOG_D(PHY,"Sequence %d xrtmag %ld xrtmag_next %ld, slot %d\n", i, xrtmag, xrtmag_next, slot);
      maxpos = i;
      int64_t temp2 = 0,temp3 = 0;;
      for (int aa=0; aa<frame_parms->nb_antennas_rx; aa++) {
        temp2 += squaredMod(corr[aa][0]);
        if (pucch_pdu->nr_of_symbols == 2)
         temp3 += squaredMod(corr[aa][1]);
      }
    }
    else if (temp > xrtmag_next)
      xrtmag_next = temp;
  }

#ifdef DEBUG_NR_PUCCH_RX
  LOG_D(NR_PHY, "PUCCH 0 : maxpos %d\n", maxpos);
#endif
  index = maxpos;
  uint8_t ack_nack = !(index&0x01);
  LOG_D(PHY,
        "[PSFCH RX] %d.%d HARQ %s\n",
        frame,
        slot,
        ack_nack == 0 ? "ACK" : "NACK");
  return ack_nack;
}
