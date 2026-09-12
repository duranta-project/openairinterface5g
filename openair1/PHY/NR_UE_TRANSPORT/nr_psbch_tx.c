/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "PHY/defs_nr_UE.h"
#include "PHY/NR_REFSIG/nr_mod_table.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_proto_ue.h"
#include "PHY/CODING/nrPolar_tools/nr_polar_psbch_defs.h"
#include "PHY/MODULATION/nr_modulation.h"
#include "PHY/gold.h"

// #define SL_DEBUG

/**
 *This function performs PSBCH SCrambling as described in 38.211.
 *Input parameter "output" is scrambled and the scrambled output is stored in this parameter.
 *id - SLSS ID used for C_INIT
 *length is the length of the buffer.
 */
void sl_psbch_scrambling(uint32_t *output, uint32_t id, uint16_t length)
{
  uint32_t x1, x2, s = 0;
  // x1 is set in gold_generic
  x2 = id; // C_INIT

#ifdef SL_DEBUG
  for (int i = 0; i < 56; i++) {
    printf("\nBEFORE SCRAMBLING output[%d]:0x%x\n", i, output[i]);
  }
#endif

  // get initial 32 scrambing bits
  s = gold_generic(&x1, &x2, 1);
#ifdef SL_DEBUG
  printf("s: %04x\t", s);
#endif

  // scramble in 32bit chunks
  int i = 0;
  while (i + 32 <= length) {
    output[i >> 5] ^= s;

    i += 32;
    s = gold_generic(&x1, &x2, 0);
#ifdef SL_DEBUG
    printf("s: %04x\t", s);
#endif
  }

  // scramble remaining bits
  for (; i < length; ++i) {
    output[i >> 5] ^= ((s >> (i & 0x1f) & 1) << (i & 0x1f));
  }

#ifdef SL_DEBUG
  for (int i = 0; i < 56; i++) {
    printf("\nAFTER SCRAMBLING output[%d]:0x%x\n", i, output[i]);
  }
#endif
}

/**
 *This function RE MAPS PSS, SSS sequences as described in 38.211.
 *txF is the data in frequency domain, sync_seq = PSS or SSS seq
 *startsym = 1 for PSS, 3 for SSS
 *re_offset = sample which points to first RE + SSB start RE
 *scaling factor =  scaling factor used for PSS, SSS (determined according to PSBCH pwr)
 *symbol size = OFDM symbol size used for RE Mapping
 */
void sl_map_pss_or_sss(c16_t *txF,
                       int16_t *sync_seq,
                       uint16_t startsym,
                       uint16_t re_offset,
                       uint16_t scaling_factor,
                       uint16_t symbol_size)
{
#ifdef SL_DEBUG
  printf("%s. DEBUG PSBCH TX: RE MAPPING of PSS/SSS \n", __func__);
  printf("Input Params - StartSYM:%d, NUMSYM:%d, RE_OFFSET:%d, num_REs:%d, scaling_factor:%d, symbol_size:%d\n",
         startsym,
         SL_NR_NUM_PSS_OR_SSS_SYMBOLS,
         re_offset,
         SL_NR_NUM_PSBCH_RE_IN_ONE_SYMBOL,
         scaling_factor,
         symbol_size);
#endif

  // RE Mapping of SL-PSS, SL-SSS
  for (int l = startsym; l < (startsym + SL_NR_NUM_PSS_OR_SSS_SYMBOLS); l++) {
    int k = re_offset;
    int index = 0, offset = 0;

    for (int m = 0; m < SL_NR_NUM_PSBCH_RE_IN_ONE_SYMBOL; m++) {
      offset = l * symbol_size + k;
      if ((m < 2) || (m >= (SL_NR_NUM_PSBCH_RE_IN_ONE_SYMBOL - 3))) {
        txF[offset].r = 0; // Set REs 0,1,129,130,131 = 0

#ifdef SL_DEBUG
        printf("sym:%d, RE:%d, txF[%d]:%d.%d \n", l, m, offset, txF[offset].r, txF[offset].i);
#endif

      } else {
        txF[offset].r = (sync_seq[index] * scaling_factor) >> 15;

#ifdef SL_DEBUG
        printf("sym:%d, RE:%d, txF[%d]:%d.%d, syncseq[%d]:%d \n",
               l,
               m,
               offset,
               txF[offset].r,
               txF[offset].i,
               index,
               sync_seq[index]);
#endif

        index++;
      }
      txF[offset].i = 0;
      k++;
    }
  }
}

/**
 * This function Generates the PSBCH DATA Modulation symbols and RE MAPS PSBCH Modulated symbols
 * and PSBCH DMRS sequences as described in 38.211.
 * txF is the data in frequency domain
 * payload is the PSBCH payload (SL-MIB given by higher layers)
 * id - SLSS ID used for knowing which DMRS sequence to be used.
 * Cp - NORMAL of extended Cyclic prefix
 * startsym = 0 and then PSBCH is mapped from symbols 5-13 if normal , 5-11 if extended
 * re_offset = sample which points to first RE + SSB start RE
 * scaling factor =  scaling factor used for PSS, SSS (determined according to PSBCH pwr)
 * symbol size = OFDM symbol size used for RE Mapping
 */

void sl_generate_and_map_psbch(c16_t *txF,
                               uint32_t *payload,
                               uint16_t id,
                               uint16_t cp,
                               uint16_t re_offset,
                               uint16_t scaling_factor,
                               uint16_t symbol_size,
                               c16_t *psbch_dmrs)
{
  uint64_t psbch_a_reversed = 0;
  uint16_t num_psbch_modsym = 0, numsym = 0;
  const int mod_order = 2; // QPSK
  uint32_t encoder_output[SL_NR_POLAR_PSBCH_E_DWORD];
  struct complex16 psbch_modsym[SL_NR_NUM_PSBCH_MODULATED_SYMBOLS];

  LOG_D(PHY, "PSBCH TX: Generation accg to 38.212, 38.211. SLSS id:%d\n", id);

  // Encoder reversal
  for (int i = 0; i < SL_NR_POLAR_PSBCH_PAYLOAD_BITS; i++)
    psbch_a_reversed |= (((uint64_t)*payload >> i) & 1) << (31 - i);

#ifdef SL_DEBUG
  printf("DEBUG PSBCH TX: 38.212 PSBCH CRC + Channel coding (POLAR) + Rate Matching:\n");
  printf("PSBCH payload:%x, Reversed Payload:%016lx\n", *payload, psbch_a_reversed);
#endif

  /// CRC, coding and rate matching
  polar_encoder_fast(&psbch_a_reversed,
                     (void *)encoder_output,
                     0,
                     0,
                     SL_NR_POLAR_PSBCH_MESSAGE_TYPE,
                     SL_NR_POLAR_PSBCH_PAYLOAD_BITS,
                     SL_NR_POLAR_PSBCH_AGGREGATION_LEVEL);

#ifdef SL_DEBUG
  for (int i = 0; i < SL_NR_POLAR_PSBCH_E_DWORD; i++)
    printf("encoderoutput[%d]: 0x%08x\t", i, encoder_output[i]);
  printf("\n");
#endif

  /// 38.211 Scrambling
  if (cp) { // EXT Cyclic prefix
    sl_psbch_scrambling(encoder_output, id, SL_NR_POLAR_PSBCH_E_EXT_CP); // for Extended Cyclic prefix
    num_psbch_modsym = SL_NR_POLAR_PSBCH_E_EXT_CP / mod_order;
    numsym = SL_NR_NUM_SYMBOLS_SSB_EXT_CP;
    AssertFatal(1 == 0, "EXT CP is not yet supported\n");
  } else { // Normal CP
    sl_psbch_scrambling(encoder_output, id, SL_NR_POLAR_PSBCH_E_NORMAL_CP); // for Cyclic prefix
    num_psbch_modsym = SL_NR_POLAR_PSBCH_E_NORMAL_CP / mod_order;
    numsym = SL_NR_NUM_SYMBOLS_SSB_NORMAL_CP;
  }

  LOG_D(PHY, "PSBCH TX: 38.211 Scrambling done. Number of bits:%d \n", SL_NR_POLAR_PSBCH_E_NORMAL_CP);

#ifdef SL_DEBUG
  printf("38211 STEP: PSBCH Scrambling \n");
  for (int i = 0; i < SL_NR_POLAR_PSBCH_E_NORMAL_CP / 32; i++)
    printf("Scrambleroutput[%d]: 0x%08x\t", i, encoder_output[i]);
  printf("\n");
#endif

#ifdef SL_DEBUG
  printf("SIDELINK PSBCH TX: 38211 STEP: QPSK Modulation of PSBCH symbols:%d, symbols in PSBCH:%d\n", num_psbch_modsym, numsym);
#endif

  /// 38.211 QPSK modulation
  nr_modulation(encoder_output, num_psbch_modsym * mod_order, mod_order, (int16_t *)psbch_modsym);

  // RE MApping of PSBCH and PSBCH DMRS
  int index = 0, dmrs_index = 0;
  const int numre = SL_NR_NUM_PSBCH_RE_IN_ONE_SYMBOL;

#ifdef SL_DEBUG
  LOG_M("sl_psbch_data_symbols.m", "psbch_sym", (void *)psbch_modsym, num_psbch_modsym, 1, 1);
  LOG_M("sl_psbch_dmrs_symbols.m", "psbch_dmrs", (void *)psbch_dmrs, SL_NR_NUM_PSBCH_DMRS_RE, 1, 1);
#endif

#ifdef SL_DEBUG
  printf("\nMapping Sidelink PSBCH DMRS, PSBCH modulation symbols to 132 REs\n");
#endif

#ifdef SL_DEBUG
  printf("%s. DEBUG PSBCH TX: RE MAPPING of PSBCH DATA AND DMRS \n", __func__);
  printf("Input Params - StartSYM:%d, NUMSYM:%d, RE_OFFSET:%d, num_REs:%d, scaling_factor:%d, symbol_size:%d\n",
         0,
         numsym,
         re_offset,
         numre,
         scaling_factor,
         symbol_size);
#endif

  for (int l = 0; l < numsym;) {
    int k = re_offset;
    int symbol_offset = l * symbol_size;
    int offset = 0;

    for (int m = 0; m < numre; m++) {
      // Maps PSBCH DMRS in every 4th RE ex:0,4,....128
      // Maps PSBCH in all other REs ex: 1,2,3,5,6,...127,129,130,131

      offset = symbol_offset + k;

#ifdef SL_DEBUG
      printf("symbol:%d, symbol_offset:%d, k:%d, re:%d, sampleoffset:%d ", l, symbol_offset, k, m, offset);
#endif

      if (m % 4 == 0) {
        txF[offset] = c16xmulConstShift(psbch_dmrs[dmrs_index], scaling_factor, 15);

#ifdef SL_DEBUG
        printf("txF[%d]:%d,%d, psbch_dmrs[%d]:%d,%d ",
               offset,
               txF[offset].r,
               txF[offset].i,
               dmrs_index,
               psbch_dmrs[dmrs_index].r,
               psbch_dmrs[dmrs_index].i);
#endif

        dmrs_index++;

      } else {
        txF[offset] = c16xmulConstShift(psbch_modsym[index], scaling_factor, 15);

#ifdef SL_DEBUG
        printf("txF[%d]:%d,%d, psbch_modsym[%d]:%d,%d\n",
               offset,
               txF[offset].r,
               txF[offset].i,
               index,
               psbch_modsym[index].r,
               psbch_modsym[index].i);
#endif

        index++;
      }

      k++;
    }

    LOG_D(PHY,
          "PSBCH TX: 38211 STEP: RE MAPPING OF PSBCH, PSBCH DMRS DONE. symbol:%d, first RE offset:%d, Last RE offset:%d, Num PSBCH "
          "DATA REs:%d, Num PSBCH DMRS REs:%d\n",
          l,
          symbol_offset + re_offset,
          offset,
          index,
          dmrs_index);

    l = (l == 0) ? 5 : l + 1;
  }
}

/**
 *This function prepares the PSBCH block and RE MAPS PSS, SSS, PSBCH DATA, PSBCH DMRS into buffer txF.
 *Called by the L1 Scheduler when MAC triggers PHY to send PSBCH
 *UE is the UE context.
 *frame, slot points to the TTI in which PSBCH TX will be transmitted
 */
void nr_tx_psbch(PHY_VARS_NR_UE *UE, uint32_t frame_tx, uint32_t slot_tx, sl_nr_tx_config_psbch_pdu_t *psbch_vars, c16_t **txdataF)
{
  sl_nr_ue_phy_params_t *sl_ue_phy_params = &UE->SL_UE_PHY_PARAMS;
  uint16_t slss_id = psbch_vars->tx_slss_id;
  NR_DL_FRAME_PARMS *sl_fp = &sl_ue_phy_params->sl_frame_params;
  uint32_t psbch_payload = *((uint32_t *)psbch_vars->psbch_payload);

  LOG_D(PHY, "PSBCH TX: slss-id %d, psbch payload %x \n", slss_id, psbch_payload);

  // Insert FN and Slot number into SL-MIB
  uint32_t mask = ~(0x700 | 0xFE0000 | 0x10000 | 0xFC000000);
  psbch_payload &= mask;
  psbch_payload |= ((frame_tx % 1024) << 1) & 0x700;
  psbch_payload |= ((frame_tx % 1024) << 17) & 0xFE0000;
  psbch_payload |= (slot_tx << 10) & 0x10000;
  psbch_payload |= (slot_tx << 26) & 0xFC000000;

  LOG_D(PHY, "PSBCH TX: Frame.Slot %d.%d. Payload::0x%08x, slssid:%d\n", frame_tx, slot_tx, psbch_payload, slss_id);

  // GENERATE Sidelink PSS,SSS Sequences, PSBCH DMRS Symbols, PSBCH Symbols
  int16_t *sl_pss = &sl_ue_phy_params->init_params.sl_pss[slss_id / 336][0];
  int16_t *sl_sss = &sl_ue_phy_params->init_params.sl_sss[slss_id][0];

  uint16_t re_offset = sl_fp->ssb_start_subcarrier;
  uint16_t symbol_size = sl_fp->ofdm_symbol_size;
  // TBD: Need to be replaced by function which calculates scaling factor based on psbch tx power
  uint16_t scaling_factor = AMP;

  struct complex16 *txF = &txdataF[0][0];
  uint16_t startsym = SL_NR_PSS_START_SYMBOL;

#ifdef SL_DEBUG
  printf("DEBUG PSBCH TX: MAP PSS. startsym:%d, PSS RE START:%d, scaling factor:%d\n", startsym, re_offset, scaling_factor);
#endif
  sl_map_pss_or_sss(txF, sl_pss, startsym, re_offset, scaling_factor, symbol_size); // PSS

  startsym += SL_NR_NUM_PSS_SYMBOLS;
#ifdef SL_DEBUG
  printf("DEBUG PSBCH TX: MAP SSS. startsym:%d, SSS RE START:%d, scaling factor:%d\n", startsym, re_offset, scaling_factor);
#endif
  sl_map_pss_or_sss(txF, sl_sss, startsym, re_offset, scaling_factor, symbol_size); // SSS

#ifdef SL_DEBUG
  printf("DEBUG PSBCH TX: MAP PSBCH DATA AND DMRS. cyclicPrefix:%d, PSS RE START:%d, scaling factor:%d\n",
         sl_fp->Ncp,
         re_offset,
         scaling_factor);
#endif

  struct complex16 *psbch_dmrs = &sl_ue_phy_params->init_params.psbch_dmrs_modsym[slss_id][0];

  sl_generate_and_map_psbch(txF, &psbch_payload, slss_id, sl_fp->Ncp, re_offset, scaling_factor, symbol_size, psbch_dmrs);

#ifdef SL_DEBUG
  printf("DEBUG PSBCH TX: txdataF Prepared\n");
#endif

#ifdef SL_DEBUG
  LOG_M("sl_psbch_block.m", "sl_txF", (void *)txdataF[0], symbol_size * 14, 1, 1);
#endif
}
