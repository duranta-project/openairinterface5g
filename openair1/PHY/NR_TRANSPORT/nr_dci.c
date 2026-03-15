/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Implements DCI encoding and PDCCH TX procedures (38.212/38.213/38.214). V15.4.0 2019-01.
 */


#include "nr_dci.h"
#include "nr_dlsch.h"
#include "nr_sch_dmrs.h"
#include "PHY/MODULATION/nr_modulation.h"
#include "common/utils/nr/nr_common.h"
#include "SCHED_NR/sched_nr.h"

//#define DEBUG_PDCCH_DMRS
//#define DEBUG_DCI
//#define DEBUG_CHANNEL_CODING

static void nr_pdcch_scrambling(uint32_t *in, uint32_t size, uint32_t Nid, uint32_t scrambling_RNTI, uint32_t *out)
{
  int roundedSz = ((size + 31) / 32);
  uint32_t *seq = gold_cache((scrambling_RNTI << 16) + Nid, roundedSz);
  LOG_D(NR_PHY_DCI, "PDCCH scrambling_RNTI %x \n", scrambling_RNTI);
  for (int i = 0; i < roundedSz; i++)
    out[i] = in[i] ^ seq[i];
}

/* Get spatial stream indices for this DCI. */
static inline uint8_t get_dci_ant_port_index(const nfapi_v4_pdcch_pdu_parameters_t *p, int dci_index, uint16_t ssi[MAX_NUM_SPATIAL_STREAMS])
{
  uint8_t num_ssi = 0;
  for (uint_fast16_t i = 0; i < p->numSpatialStreams; i++) {
    if (dci_index == p->dci_spatialStreamIndices[i].dci_index) {
      ssi[num_ssi++] = p->dci_spatialStreamIndices[i].spatial_stream_index;
    }
  }

  /*  If no streams provided by MAC, PHY assumes all DCI in this PDU belong to
  same beam and takes SSI of other DCI */
  if (num_ssi == 0)
    for (uint_fast16_t i = 0; i < p->numSpatialStreams; i++)
      ssi[num_ssi++] = p->dci_spatialStreamIndices[i].spatial_stream_index;

  return num_ssi;
}

void nr_generate_dci(PHY_VARS_gNB *gNB,
                     const nfapi_nr_dl_tti_pdcch_pdu_rel15_t *pdcch_pdu_rel15,
                     NR_DL_FRAME_PARMS *frame_parms,
                     int slot)
{
  // fill reg list per symbol
  int reg_list[MAX_DCI_CORESET][NR_MAX_PDCCH_AGG_LEVEL * NR_NB_REG_PER_CCE];
  nr_fill_reg_list(reg_list, pdcch_pdu_rel15);
  // compute rb_offset and n_prb based on frequency allocation
  int n_rb, cset_start;
  get_coreset_rballoc(pdcch_pdu_rel15->FreqDomainResource, &n_rb, &cset_start);
  int additional_offset = 0;
  // first common RB of the first group of 6 PRBs has common RB index equal to 6 * ⌈BWP_start / 6⌉
  if (pdcch_pdu_rel15->CoreSetType == 1)
    additional_offset = (pdcch_pdu_rel15->BWPStart + 5) / 6 * 6 - pdcch_pdu_rel15->BWPStart;
  int rb_offset = cset_start + additional_offset;
  uint16_t cset_start_sc = (pdcch_pdu_rel15->BWPStart + rb_offset) * NR_NB_SC_PER_RB;
  int idx1 = pdcch_pdu_rel15->StartSymbolIndex+pdcch_pdu_rel15->DurationSymbols;
  int idx2 = (((n_rb + rb_offset + pdcch_pdu_rel15->BWPStart) * 3) + 15) & ~15;
  c16_t mod_dmrs[idx1][idx2] __attribute__((aligned(16)));

  for (int d = 0; d < pdcch_pdu_rel15->numDlDci; d++) {
    /*The coreset is initialised
     * in frequency: the first subcarrier is obtained by adding the first CRB overlapping the SSB and the rb_offset for coreset 0
     * or the rb_offset for other coresets
     * in time: by its first slot and its first symbol*/
    const nfapi_nr_dl_dci_pdu_t *dci_pdu = &pdcch_pdu_rel15->dci_pdu[d];

    uint32_t cset_start_symb = pdcch_pdu_rel15->StartSymbolIndex;
    uint32_t cset_nsymb = pdcch_pdu_rel15->DurationSymbols;
    int dci_idx = 0;

    LOG_D(NR_PHY_DCI, "pdcch: Coreset rb_offset %d, nb_rb %d BWP Start %d\n", rb_offset, n_rb, pdcch_pdu_rel15->BWPStart);
    LOG_D(NR_PHY_DCI,
          "pdcch: Coreset starting subcarrier %d on symbol %d (%d symbols)\n",
          cset_start_sc,
          cset_start_symb,
          cset_nsymb);
    // DMRS length is per OFDM symbol
    uint32_t dmrs_length = (n_rb+pdcch_pdu_rel15->BWPStart)*6; //2(QPSK)*3(per RB)*6(REG per CCE)
    uint32_t encoded_length = dci_pdu->AggregationLevel*108; //2(QPSK)*9(per RB)*6(REG per CCE)
    if (dci_pdu->RNTI != 0xFFFF)
      LOG_D(NR_PHY_DCI,
            "DL_DCI : rb_offset %d, nb_rb %d, DMRS length per symbol %d\t DCI encoded length %d (precoder_granularity %d, "
            "reg_mapping %d), Scrambling_Id %d, ScramblingRNTI %x, PayloadSizeBits %d AggregationLevel %d\n",
            rb_offset,
            n_rb,
            dmrs_length,
            encoded_length,
            pdcch_pdu_rel15->precoderGranularity,
            pdcch_pdu_rel15->CceRegMappingType,
            dci_pdu->ScramblingId,
            dci_pdu->ScramblingRNTI,
            dci_pdu->PayloadSizeBits,
            dci_pdu->AggregationLevel);
    dmrs_length += rb_offset*6; // To accommodate more DMRS symbols in case of rb offset
      
    /// DMRS QPSK modulation
    for (int symb = cset_start_symb; symb < cset_start_symb + pdcch_pdu_rel15->DurationSymbols; symb++) {
      const uint32_t *gold = nr_gold_pdcch(frame_parms->N_RB_DL, frame_parms->symbols_per_slot, dci_pdu->ScramblingId, slot, symb);
      nr_modulation(gold, dmrs_length, DMRS_MOD_ORDER, (int16_t *)mod_dmrs[symb]); // Qm = 2 as DMRS is QPSK modulated

#ifdef DEBUG_PDCCH_DMRS
      if(dci_pdu->RNTI!=0xFFFF) {
        for (int i=0; i<dmrs_length>>1; i++)
          printf("symb %d i %d %p gold seq 0x%08x mod_dmrs %d %d\n",
                 symb,
                 i,
                 &gold_pdcch_dmrs[symb][i >> 5],
                 gold_pdcch_dmrs[symb][i >> 5],
                 mod_dmrs[symb][i].r,
                 mod_dmrs[symb][i].i);
      }
#endif
    }
    
    /// DCI payload processing
    // CRC attachment + Scrambling + Channel coding + Rate matching
    uint32_t encoder_output[NR_MAX_DCI_SIZE_DWORD];

    uint16_t n_RNTI = dci_pdu->RNTI;
    uint16_t Nid    = dci_pdu->ScramblingId;
    uint16_t scrambling_RNTI = dci_pdu->ScramblingRNTI;

    polar_encoder_fast((uint64_t*)dci_pdu->Payload, (void*)encoder_output, n_RNTI, 1, 
                       NR_POLAR_DCI_MESSAGE_TYPE, dci_pdu->PayloadSizeBits, dci_pdu->AggregationLevel);
#ifdef DEBUG_CHANNEL_CODING
//debug dump dci
    printf("polar rnti %x,length %d, L %d\n",n_RNTI, dci_pdu->PayloadSizeBits,pdcch_pdu_rel15->dci_pdu->AggregationLevel);
    printf("DCI PDU: [0]->0x%lx \t [1]->0x%lx\n",
	   ((uint64_t*)dci_pdu->Payload)[0], ((uint64_t*)dci_pdu->Payload)[1]);
    printf("Encoded Payload (length:%u dwords):\n", encoded_length>>5);
    
    for (int i=0; i<encoded_length>>5; i++)
      printf("[%d]->0x%08x \t", i,encoder_output[i]);    

    printf("\n");
#endif
    /// Scrambling
    uint32_t scrambled_output[NR_MAX_DCI_SIZE_DWORD]= {0};
    nr_pdcch_scrambling(encoder_output, encoded_length, Nid, scrambling_RNTI, scrambled_output);
#ifdef DEBUG_CHANNEL_CODING
    printf("scrambled output: [0]->0x%08x \t [1]->0x%08x \t [2]->0x%08x \t [3]->0x%08x\t [4]->0x%08x\t [5]->0x%08x\t \
[6]->0x%08x \t [7]->0x%08x \t [8]->0x%08x \t [9]->0x%08x\t [10]->0x%08x\t [11]->0x%08x\n",
	   scrambled_output[0], scrambled_output[1], scrambled_output[2], scrambled_output[3], scrambled_output[4],scrambled_output[5],
	   scrambled_output[6], scrambled_output[7], scrambled_output[8], scrambled_output[9], scrambled_output[10],scrambled_output[11] );
#endif
    /// QPSK modulation
    c16_t mod_dci[NR_MAX_DCI_SIZE / 2] __attribute__((aligned(16)));
    nr_modulation(scrambled_output, encoded_length, DMRS_MOD_ORDER, (int16_t *)mod_dci); // Qm = 2 as DMRS is QPSK modulated
#ifdef DEBUG_DCI
    
    for (int i=0; i<encoded_length>>1; i++)
      printf("i %d mod_dci %d %d\n", i, mod_dci[i].r, mod_dci[i].i);

#endif

    /// Resource mapping
    uint16_t amp = gNB->TX_AMP;
    int num_regs = dci_pdu->AggregationLevel * NR_NB_REG_PER_CCE / pdcch_pdu_rel15->DurationSymbols;

    // Update grid info. Create new section for each DCI. We assume REGs are contiguous and not interleaved
    const nfapi_nr_tx_precoding_and_beamforming_t *pb = &dci_pdu->precodingAndBeamforming;
    uint16_t beam_id = (pb->dig_bf_interfaces > 0) ? pb->prgs_list[0].dig_bf_interface_list[0].beam_idx : 0;
    uint16_t ssi[MAX_NUM_SPATIAL_STREAMS];
    get_dci_ant_port_index(&pdcch_pdu_rel15->param_v4, d, ssi);
    c16_t **txdataF = gNB->common_vars.txdataF;
    // Update grid info to send to oru for beamforming
    update_grid_info(gNB->common_vars.tx_grid_info,
                     ssi[0], // no dbf in phy. so only one log port to oru since no precoding for dci for the moment.
                     beam_id,
                     pdcch_pdu_rel15->BWPStart + rb_offset + reg_list[d][0],
                     reg_list[d][num_regs - 1] - reg_list[d][0],
                     cset_start_symb,
                     pdcch_pdu_rel15->DurationSymbols);

    /*Mapping the encoded DCI along with the DMRS */
    for(int symbol_idx = 0; symbol_idx < pdcch_pdu_rel15->DurationSymbols; symbol_idx++) {
      // allocating rbs per symbol
      for (int reg_count = 0; reg_count < num_regs; reg_count++) {
        int k = cset_start_sc + reg_list[d][reg_count] * NR_NB_SC_PER_RB;
        LOG_D(NR_PHY_DCI, "REG %d k %d\n", reg_list[d][reg_count], k);

        int l = cset_start_symb + symbol_idx;

        // dmrs index depends on reference point for k according to 38.211 7.4.1.3.2
        int dmrs_idx;
        if (pdcch_pdu_rel15->CoreSetType == NFAPI_NR_CSET_CONFIG_PDCCH_CONFIG)
          dmrs_idx = (reg_list[d][reg_count] + pdcch_pdu_rel15->BWPStart + rb_offset) * 3;
        else
          dmrs_idx = (reg_list[d][reg_count] + rb_offset) * 3;

        int k_prime = 0;

        c16_t tmp_dataF[NR_NB_SC_PER_RB];
        for (int m = 0; m < NR_NB_SC_PER_RB; m++) {
          if (m == (k_prime << 2) + 1) { // DMRS if not already mapped
            tmp_dataF[m] = c16mulRealShift(mod_dmrs[l][dmrs_idx], amp, 15);

#ifdef DEBUG_PDCCH_DMRS
            LOG_I(NR_PHY_DCI,
                  "PDCCH DMRS %d: l %d position %d => (%d,%d)\n",
                  dmrs_idx,
                  l,
                  k,
                  tmp_dataF[m].r,
                  tmp_dataF[m].i);
#endif

            dmrs_idx++;
            k_prime++;

          } else { // DCI payload
            tmp_dataF[m] = c16mulRealShift(mod_dci[dci_idx], amp, 15);
            dci_idx++;
          }
        } // m
        nr_tx_precoder_and_beamformer(tmp_dataF,
                                      0,
                                      txdataF,
                                      l * frame_parms->ofdm_symbol_size + k,
                                      frame_parms->nb_antennas_tx,
                                      ssi,
                                      pb,
                                      gNB->gNB_config.pmi_list.pmi_pdu,
                                      &gNB->gNB_config.dbt_config,
                                      1);
      } // reg_count
    } // symbol_idx

    LOG_D(NR_PHY_DCI,
          "DCI: payloadSize = %d | payload = %llx\n",
          dci_pdu->PayloadSizeBits,
          *(unsigned long long *)dci_pdu->Payload);
  } // for (int d=0;d<pdcch_pdu_rel15->numDlDci;d++)
}
