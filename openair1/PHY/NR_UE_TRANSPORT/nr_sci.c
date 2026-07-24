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

/*! \file PHY/NR_UE_TRANSPORT/nr_sci.c
* \brief Implements SCI encoding and PSCCH TX procedures for sidelink. Based on nr_dci.c.
* \author R. Knopp
* \date 2023
* \version 0.1
* \company Eurecom
*/

#include "PHY/defs_nr_UE.h"
#include "PHY/MODULATION/nr_modulation.h"
#include "PHY/NR_REFSIG/nr_refsig.h"
#include "common/utils/nr/nr_common.h"
#include "PHY/CODING/coding_defs.h"

//#define DEBUG_PSCCH_DMRS
//#define DEBUG_SCI
//#define DEBUG_CHANNEL_CODING

void nr_sci_scrambling(uint32_t *in, uint32_t size, uint32_t Nid, uint32_t scrambling_RNTI, uint32_t *out,int sci2_flag)
{
  int roundedSz = ((size + 31) / 32);
  uint32_t *seq = gold_cache(sci2_flag > 0 ? (Nid << 15) + 1010 : (scrambling_RNTI << 16) + Nid, roundedSz);
  for (int i = 0; i < roundedSz; i++)
    out[i] = in[i] ^ seq[i];
}

uint32_t nr_generate_sci(PHY_VARS_NR_UE *ue,
                         nfapi_nr_dl_tti_pdcch_pdu_rel15_t *pdcch_pdu_rel15,
                         c16_t *txdataF,
                         int16_t amp,
                         NR_DL_FRAME_PARMS *frame_parms,
                         int slot) {

  uint16_t cset_start_sc;
  uint8_t cset_start_symb, cset_nsymb;
  int k,l,k_prime,dci_idx, dmrs_idx;

  // compute rb_offset and n_prb based on frequency allocation
  // for SCI: FreqDomainResource[0] = startRB, FreqDomainResource[1] = numRBs
  int rb_offset = pdcch_pdu_rel15->FreqDomainResource[0];
  int n_rb = pdcch_pdu_rel15->FreqDomainResource[1];

  cset_start_sc = frame_parms->first_carrier_offset + (pdcch_pdu_rel15->BWPStart + rb_offset) * NR_NB_SC_PER_RB;

  c16_t mod_dmrs[pdcch_pdu_rel15->StartSymbolIndex+pdcch_pdu_rel15->DurationSymbols][(((n_rb+rb_offset+pdcch_pdu_rel15->BWPStart)*6+15)>>5)<<5] __attribute__((aligned(16)));

  uint32_t sci_Nid = 0;

  for (int d=0;d<pdcch_pdu_rel15->numDlDci;d++) {
    /*The coreset is initialised
     * in frequency: the first subcarrier is obtained by adding the first CRB overlapping the SSB and the rb_offset for coreset 0
     * or the rb_offset for other coresets
     * in time: by its first slot and its first symbol*/
    const nfapi_nr_dl_dci_pdu_t *dci_pdu = &pdcch_pdu_rel15->dci_pdu[d];

    uint32_t **gold_pscch_dmrs = ue->nr_gold_pscch_dmrs[slot];

    cset_start_symb = pdcch_pdu_rel15->StartSymbolIndex;
    cset_nsymb = pdcch_pdu_rel15->DurationSymbols;
    dci_idx = 0;
    LOG_D(NR_PHY, "pscch: rb_offset %d, nb_rb %d BWP Start %d\n",rb_offset,n_rb,pdcch_pdu_rel15->BWPStart);
    LOG_D(NR_PHY, "pscch: starting subcarrier %d on symbol %d (%d symbols)\n", cset_start_sc, cset_start_symb, cset_nsymb);
    // DMRS length is per OFDM symbol
    uint32_t dmrs_length = (n_rb+pdcch_pdu_rel15->BWPStart)*6; //2(QPSK)*3(per RB)*6(REG per CCE)
    uint32_t encoded_length = dci_pdu->AggregationLevel*18; //2(QPSK)*9(per RB) for SCI
    if (dci_pdu->RNTI != 0xFFFF)
      LOG_D(PHY, "SCI : rb_offset %d, nb_rb %d, DMRS length per symbol %d\t SCI encoded length %d (precoder_granularity %d, reg_mapping %d), Scrambling_Id %d, ScramblingRNTI %x, PayloadSizeBits %d\n",
            rb_offset, n_rb,dmrs_length, encoded_length,pdcch_pdu_rel15->precoderGranularity,pdcch_pdu_rel15->CceRegMappingType,
            dci_pdu->ScramblingId,dci_pdu->ScramblingRNTI,dci_pdu->PayloadSizeBits);
    dmrs_length += rb_offset*6; // To accommodate more DMRS symbols in case of rb offset

    /// DMRS QPSK modulation
    for (int symb=cset_start_symb; symb<cset_start_symb + pdcch_pdu_rel15->DurationSymbols; symb++) {

      nr_modulation(gold_pscch_dmrs[symb], dmrs_length, DMRS_MOD_ORDER, (int16_t*)mod_dmrs[symb]); //Qm = 2 as DMRS is QPSK modulated

#ifdef DEBUG_PSCCH_DMRS
      if(dci_pdu->RNTI!=0xFFFF) {
        for (int i=0; i<dmrs_length>>1; i++)
          printf("symb %d i %d %p gold seq 0x%08x mod_dmrs %d %d\n", symb, i,
                 &gold_pscch_dmrs[symb][i>>5],gold_pscch_dmrs[symb][i>>5], mod_dmrs[symb][i].r, mod_dmrs[symb][i].i);
      }
#endif
    }

    /// SCI payload processing
    // CRC attachment + Scrambling + Channel coding + Rate matching
    uint32_t encoder_output[NR_MAX_DCI_SIZE_DWORD];

    uint16_t n_RNTI = dci_pdu->RNTI;
    uint16_t Nid    = dci_pdu->ScramblingId;
    uint16_t scrambling_RNTI = dci_pdu->ScramblingRNTI;
    uint64_t p;
    memcpy(&p, dci_pdu->Payload, sizeof(p));
    LOG_I(NR_PHY, "SCI-1A payload %lx (%lx)\n", p, *(uint64_t*)dci_pdu->Payload);
    polar_encoder_fast(&p, (void*)encoder_output, n_RNTI, 1,
                       NR_POLAR_SCI_MESSAGE_TYPE,
                       dci_pdu->PayloadSizeBits, dci_pdu->AggregationLevel);
#ifdef DEBUG_CHANNEL_CODING
//debug dump sci
    printf("polar rnti %x,length %d, L %d\n",n_RNTI, dci_pdu->PayloadSizeBits,pdcch_pdu_rel15->dci_pdu->AggregationLevel);
    printf("SCI PDU: [0]->0x%lx \t [1]->0x%lx\n",
	   ((uint64_t*)dci_pdu->Payload)[0], ((uint64_t*)dci_pdu->Payload)[1]);
    printf("Encoded Payload (length:%u dwords):\n", encoded_length>>5);

    for (int i=0; i<encoded_length>>5; i++)
      printf("[%d]->0x%08x \t", i,encoder_output[i]);

    printf("\n");
#endif
    /// Scrambling
    uint32_t scrambled_output[NR_MAX_DCI_SIZE_DWORD]= {0};
    nr_sci_scrambling(encoder_output, encoded_length, Nid, scrambling_RNTI, scrambled_output, 0);
#ifdef DEBUG_CHANNEL_CODING
    printf("scrambled output: [0]->0x%08x \t [1]->0x%08x \t [2]->0x%08x \t [3]->0x%08x\t [4]->0x%08x\t [5]->0x%08x\n \
[6]->0x%08x \t [7]->0x%08x \t [8]->0x%08x \t [9]->0x%08x\t [10]->0x%08x\t [11]->0x%08x\n",
	   scrambled_output[0], scrambled_output[1], scrambled_output[2], scrambled_output[3], scrambled_output[4],scrambled_output[5],
	   scrambled_output[6], scrambled_output[7], scrambled_output[8], scrambled_output[9], scrambled_output[10],scrambled_output[11] );
#endif
    /// QPSK modulation
    c16_t mod_dci[encoded_length] __attribute__((aligned(16)));
    nr_modulation(scrambled_output, encoded_length, DMRS_MOD_ORDER, (int16_t*)mod_dci); //Qm = 2 as DMRS is QPSK modulated
#ifdef DEBUG_SCI

    for (int i=0; i<encoded_length>>1; i++)
      printf("i %d mod_dci %d %d\n", i, mod_dci[i].r, mod_dci[i].i );

#endif

    /// Resource mapping

    if (cset_start_sc >= frame_parms->ofdm_symbol_size)
      cset_start_sc -= frame_parms->ofdm_symbol_size;

    int num_regs = dci_pdu->AggregationLevel/pdcch_pdu_rel15->DurationSymbols;

    /*Mapping the encoded SCI along with the DMRS */
    for(int symbol_idx = 0; symbol_idx < pdcch_pdu_rel15->DurationSymbols; symbol_idx++) {
      // allocating rbs per symbol
      for (int reg_count = 0; reg_count < num_regs; reg_count++) {
        if (reg_count == 0) k = cset_start_sc + pdcch_pdu_rel15->dci_pdu[d].CceIndex * NR_NB_SC_PER_RB;
        if (k >= frame_parms->ofdm_symbol_size)
          k -= frame_parms->ofdm_symbol_size;

        l = cset_start_symb + symbol_idx;
        // dmrs index depends on reference point for k according to 38.211 7.4.1.3.2
        if (pdcch_pdu_rel15->CoreSetType == NFAPI_NR_CSET_CONFIG_PDCCH_CONFIG)
          dmrs_idx = reg_count * 3;
        else
          dmrs_idx = (pdcch_pdu_rel15->dci_pdu[d].CceIndex + rb_offset + reg_count) * 3;

        k_prime = 0;

        for (int m = 0; m < NR_NB_SC_PER_RB; m++) {
          if (m == (k_prime << 2) + 1) { // DMRS if not already mapped
            txdataF[l * frame_parms->ofdm_symbol_size + k].r = (amp * mod_dmrs[l][dmrs_idx].r) >> 15;
            txdataF[l * frame_parms->ofdm_symbol_size + k].i = (amp * mod_dmrs[l][dmrs_idx].i) >> 15;

#ifdef DEBUG_PSCCH_DMRS
            LOG_I(PHY,
                  "PSCCH DMRS %d: l %d position %d => (%d,%d)\n",
                  dmrs_idx,
                  l,
                  k,
                  txdataF[l * frame_parms->ofdm_symbol_size + k].r,
                  txdataF[l * frame_parms->ofdm_symbol_size + k].i);
#endif

            dmrs_idx++;
            k_prime++;

          } else { // SCI payload
            txdataF[l * frame_parms->ofdm_symbol_size + k].r = (amp * mod_dci[dci_idx].r) >> 15;
            txdataF[l * frame_parms->ofdm_symbol_size + k].i = (amp * mod_dci[dci_idx].i) >> 15;
#ifdef DEBUG_SCI
            LOG_I(PHY,
                  "PSCCH: l %d position %d => (%d,%d)\n",
                  l,
                  k,
                  txdataF[l * frame_parms->ofdm_symbol_size + k].r,
                  txdataF[l * frame_parms->ofdm_symbol_size + k].i);
#endif

            dci_idx++;
          }

          k++;

          if (k >= frame_parms->ofdm_symbol_size)
            k -= frame_parms->ofdm_symbol_size;
        } // m
      } // reg_count
    } // symbol_idx

    LOG_D(NR_PHY,
          "SCI: payloadSize = %d | payload = %llx\n",
          dci_pdu->PayloadSizeBits,
          *(unsigned long long *)dci_pdu->Payload);

    // PSSCH DMRS + PSSCH/SLSCH scrambling n_ID (38.211 8.3.1.1) is derived from the
    // SCI-1A CRC, not from the DMRS scrambling id. The RX obtains it from the polar
    // decoder as (crc24c(payload) & 0xFFFF); reproduce the identical computation here
    // (mirrors polar_decoder_int16() with ones_flag=1) so TX and RX agree.
    {
      uint64_t Ar = *(uint64_t *)dci_pdu->Payload;
      const int len = dci_pdu->PayloadSizeBits;
      Ar &= (len < 64) ? ((1ULL << len) - 1) : ~0ULL;
      uint32_t crc = 0;
      if (len <= 32) {
        uint32_t Aprime = (uint32_t)(Ar << (32 - len));
        uint8_t A[3 + 4] = {0xff, 0xff, 0xff,
                            (uint8_t)(Aprime >> 24), (uint8_t)(Aprime >> 16),
                            (uint8_t)(Aprime >> 8), (uint8_t)Aprime};
        crc = (crc24c(A, 24 + len) >> 8) & 0xffffff;
      } else {
        uint64_t Aprime = Ar << (64 - len);
        uint8_t A[3 + 8] = {0xff, 0xff, 0xff,
                            (uint8_t)(Aprime >> 56), (uint8_t)(Aprime >> 48),
                            (uint8_t)(Aprime >> 40), (uint8_t)(Aprime >> 32),
                            (uint8_t)(Aprime >> 24), (uint8_t)(Aprime >> 16),
                            (uint8_t)(Aprime >> 8),  (uint8_t)Aprime};
        crc = (crc24c(A, 24 + len) >> 8) & 0xffffff;
      }
      sci_Nid = crc & 0xFFFF;
      LOG_I(NR_PHY, "TX SCI-1A CRC-derived Nid=%u (payload 0x%llx, len %d)\n",
            sci_Nid, (unsigned long long)Ar, len);
    }
  } // for (int d=0;d<pdcch_pdu_rel15->numDlDci;d++)
  return sci_Nid;
}
