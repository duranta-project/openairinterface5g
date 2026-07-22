/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Top-level routines for demodulating the PUSCH physical channel from 36.211 V8.6 2009-03
 */

#include "PHY/defs_eNB.h"
//#include "PHY/phy_extern.h"
#include "transport_eNB.h"
#include "PHY/sse_intrin.h"
#include "transport_common_proto.h"
#include "PHY/LTE_ESTIMATION/lte_estimation.h"
//#include "PHY/MODULATION/modulation_eNB.h"

#include "T.h"

//extern int **ulchmag_eren;
//eren

static const short jitter[8]  __attribute__ ((aligned(16))) = {1,0,0,1,0,1,1,0};
static const short jitterc[8] __attribute__ ((aligned(16))) = {0,1,1,0,1,0,0,1};

#if defined(__aarch64__) || defined(__riscv)

void lte_idft(LTE_DL_FRAME_PARMS *frame_parms,
              uint32_t *z,
              uint16_t Msc_PUSCH)
{
  simde__m128i idft_in128[3][1200];
  simde__m128i idft_out128[3][1200];
  simde__m128i norm128;

  int16_t *idft_in0 = (int16_t *)idft_in128[0];
  int16_t *idft_out0 = (int16_t *)idft_out128[0];

  int16_t *idft_in1 = (int16_t *)idft_in128[1];
  int16_t *idft_out1 = (int16_t *)idft_out128[1];

  int16_t *idft_in2 = (int16_t *)idft_in128[2];
  int16_t *idft_out2 = (int16_t *)idft_out128[2];

  uint32_t *z0;
  uint32_t *z1;
  uint32_t *z2;
  uint32_t *z3;
  uint32_t *z4;
  uint32_t *z5;
  uint32_t *z6;
  uint32_t *z7;
  uint32_t *z8;
  uint32_t *z9;
  uint32_t *z10 = NULL;
  uint32_t *z11 = NULL;

  int i;
  int ip;

  LOG_T(PHY, "Doing lte_idft for Msc_PUSCH %d\n", Msc_PUSCH);

  if (frame_parms->Ncp == 0) {
    /* Normal cyclic prefix. */
    z0 = z;
    z1 = z0 + frame_parms->N_RB_DL * 12;
    z2 = z1 + frame_parms->N_RB_DL * 12;

    /* Pilot symbol. */
    z3 = z2 + 2 * frame_parms->N_RB_DL * 12;

    z4 = z3 + frame_parms->N_RB_DL * 12;
    z5 = z4 + frame_parms->N_RB_DL * 12;
    z6 = z5 + frame_parms->N_RB_DL * 12;
    z7 = z6 + frame_parms->N_RB_DL * 12;
    z8 = z7 + frame_parms->N_RB_DL * 12;

    /* Pilot symbol. */
    z9 = z8 + 2 * frame_parms->N_RB_DL * 12;

    z10 = z9 + frame_parms->N_RB_DL * 12;

    /* SRS symbol. */
    z11 = z10 + frame_parms->N_RB_DL * 12;
  } else {
    /* Extended cyclic prefix. */
    z0 = z;
    z1 = z0 + frame_parms->N_RB_DL * 12;

    /* Pilot symbol. */
    z2 = z1 + 2 * frame_parms->N_RB_DL * 12;

    z3 = z2 + frame_parms->N_RB_DL * 12;
    z4 = z3 + frame_parms->N_RB_DL * 12;
    z5 = z4 + frame_parms->N_RB_DL * 12;
    z6 = z5 + frame_parms->N_RB_DL * 12;

    /* Pilot symbol. */
    z7 = z6 + 2 * frame_parms->N_RB_DL * 12;

    z8 = z7 + frame_parms->N_RB_DL * 12;

    /* SRS symbol. */
    z9 = z8 + frame_parms->N_RB_DL * 12;
  }

  /* Conjugate the input symbols. */
  for (i = 0; i < (Msc_PUSCH >> 2); i++) {
    ((simde__m128i *)z0)[i] = oai_mm_conj(((simde__m128i *)z0)[i]);

    ((simde__m128i *)z1)[i] = oai_mm_conj(((simde__m128i *)z1)[i]);

    ((simde__m128i *)z2)[i] = oai_mm_conj(((simde__m128i *)z2)[i]);

    ((simde__m128i *)z3)[i] = oai_mm_conj(((simde__m128i *)z3)[i]);

    ((simde__m128i *)z4)[i] = oai_mm_conj(((simde__m128i *)z4)[i]);

    ((simde__m128i *)z5)[i] = oai_mm_conj(((simde__m128i *)z5)[i]);

    ((simde__m128i *)z6)[i] = oai_mm_conj(((simde__m128i *)z6)[i]);

    ((simde__m128i *)z7)[i] = oai_mm_conj(((simde__m128i *)z7)[i]);

    ((simde__m128i *)z8)[i] = oai_mm_conj(((simde__m128i *)z8)[i]);

    ((simde__m128i *)z9)[i] = oai_mm_conj(((simde__m128i *)z9)[i]);

    if (frame_parms->Ncp == NORMAL) {
      ((simde__m128i *)z10)[i] = oai_mm_conj(((simde__m128i *)z10)[i]);

      ((simde__m128i *)z11)[i] = oai_mm_conj(((simde__m128i *)z11)[i]);
    }
  }

  /* Pack four transforms into each legacy input buffer. */
  for (i = 0, ip = 0; i < Msc_PUSCH; i++, ip += 4) {
    ((uint32_t *)idft_in0)[ip] = z0[i];
    ((uint32_t *)idft_in0)[ip + 1] = z1[i];
    ((uint32_t *)idft_in0)[ip + 2] = z2[i];
    ((uint32_t *)idft_in0)[ip + 3] = z3[i];

    ((uint32_t *)idft_in1)[ip] = z4[i];
    ((uint32_t *)idft_in1)[ip + 1] = z5[i];
    ((uint32_t *)idft_in1)[ip + 2] = z6[i];
    ((uint32_t *)idft_in1)[ip + 3] = z7[i];

    ((uint32_t *)idft_in2)[ip] = z8[i];
    ((uint32_t *)idft_in2)[ip + 1] = z9[i];

    if (frame_parms->Ncp == 0) {
      ((uint32_t *)idft_in2)[ip + 2] = z10[i];
      ((uint32_t *)idft_in2)[ip + 3] = z11[i];
    }
  }

  const dft_size_idx_t dftsize = get_dft(Msc_PUSCH);

  switch (Msc_PUSCH) {
    case 12:
      dft(dftsize, idft_in0, idft_out0, 0);

      dft(dftsize, idft_in1, idft_out1, 0);

      dft(dftsize, idft_in2, idft_out2, 0);

      norm128 = simde_mm_set1_epi16(9459);

      for (i = 0; i < 12; i++) {
        ((simde__m128i *)idft_out0)[i] = simde_mm_slli_epi16(simde_mm_mulhi_epi16(((simde__m128i *)idft_out0)[i], norm128), 1);

        ((simde__m128i *)idft_out1)[i] = simde_mm_slli_epi16(simde_mm_mulhi_epi16(((simde__m128i *)idft_out1)[i], norm128), 1);

        ((simde__m128i *)idft_out2)[i] = simde_mm_slli_epi16(simde_mm_mulhi_epi16(((simde__m128i *)idft_out2)[i], norm128), 1);
      }
      break;

    default:
      dft(dftsize, idft_in0, idft_out0, 1);

      dft(dftsize, idft_in1, idft_out1, 1);

      dft(dftsize, idft_in2, idft_out2, 1);
      break;
  }

  /* Restore the symbol-major LTE layout. */
  for (i = 0, ip = 0; i < Msc_PUSCH; i++, ip += 4) {
    z0[i] = ((uint32_t *)idft_out0)[ip];
    z1[i] = ((uint32_t *)idft_out0)[ip + 1];
    z2[i] = ((uint32_t *)idft_out0)[ip + 2];
    z3[i] = ((uint32_t *)idft_out0)[ip + 3];

    z4[i] = ((uint32_t *)idft_out1)[ip];
    z5[i] = ((uint32_t *)idft_out1)[ip + 1];
    z6[i] = ((uint32_t *)idft_out1)[ip + 2];
    z7[i] = ((uint32_t *)idft_out1)[ip + 3];

    z8[i] = ((uint32_t *)idft_out2)[ip];
    z9[i] = ((uint32_t *)idft_out2)[ip + 1];

    if (frame_parms->Ncp == 0) {
      z10[i] = ((uint32_t *)idft_out2)[ip + 2];
      z11[i] = ((uint32_t *)idft_out2)[ip + 3];
    }
  }

  /* Conjugate the output symbols. */
  for (i = 0; i < (Msc_PUSCH >> 2); i++) {
    ((simde__m128i *)z0)[i] = oai_mm_conj(((simde__m128i *)z0)[i]);

    ((simde__m128i *)z1)[i] = oai_mm_conj(((simde__m128i *)z1)[i]);

    ((simde__m128i *)z2)[i] = oai_mm_conj(((simde__m128i *)z2)[i]);

    ((simde__m128i *)z3)[i] = oai_mm_conj(((simde__m128i *)z3)[i]);

    ((simde__m128i *)z4)[i] = oai_mm_conj(((simde__m128i *)z4)[i]);

    ((simde__m128i *)z5)[i] = oai_mm_conj(((simde__m128i *)z5)[i]);

    ((simde__m128i *)z6)[i] = oai_mm_conj(((simde__m128i *)z6)[i]);

    ((simde__m128i *)z7)[i] = oai_mm_conj(((simde__m128i *)z7)[i]);

    ((simde__m128i *)z8)[i] = oai_mm_conj(((simde__m128i *)z8)[i]);

    ((simde__m128i *)z9)[i] = oai_mm_conj(((simde__m128i *)z9)[i]);

    if (frame_parms->Ncp == NORMAL) {
      ((simde__m128i *)z10)[i] = oai_mm_conj(((simde__m128i *)z10)[i]);

      ((simde__m128i *)z11)[i] = oai_mm_conj(((simde__m128i *)z11)[i]);
    }
  }
}

#else

void lte_idft(LTE_DL_FRAME_PARMS *frame_parms, uint32_t *z, uint16_t Msc_PUSCH)
{
  const dft_size_idx_t dftsize = get_dft(Msc_PUSCH);

  const size_t symbol_stride = (size_t)frame_parms->N_RB_DL * 12;

  static const uint8_t normal_cp_symbols[12] = {
      0, 1, 2,
      4, 5, 6, 7, 8, 9,
      11, 12, 13
  };

  static const uint8_t extended_cp_symbols[10] = {
      0, 1,
      3, 4, 5, 6, 7,
      9, 10, 11
  };

  const uint8_t *symbol_indexes =
      frame_parms->Ncp == 0
          ? normal_cp_symbols
          : extended_cp_symbols;

  const int number_of_symbols =
      frame_parms->Ncp == 0 ? 12 : 10;

  c16_t idft_input[Msc_PUSCH] __attribute__((aligned(64)));

  c16_t idft_output[Msc_PUSCH] __attribute__((aligned(64)));

  const size_t bytes =
      (size_t)Msc_PUSCH * sizeof(c16_t);

  for (int s = 0; s < number_of_symbols; s++) {
    c16_t *symbol = (c16_t *)(z + (size_t)symbol_indexes[s] * symbol_stride);

    memcpy(idft_input, symbol, bytes);

    idft(dftsize,
         (int16_t *)idft_input,
         (int16_t *)idft_output,
         1);

    memcpy(symbol, idft_output, bytes);
  }
}

#endif

int32_t ulsch_qpsk_llr(LTE_DL_FRAME_PARMS *frame_parms,
                       int32_t **rxdataF_comp,
                       int16_t *ulsch_llr,
                       uint8_t symbol,
                       uint16_t nb_rb,
                       int16_t **llrp) {
  simde__m128i *rxF=(simde__m128i *)&rxdataF_comp[0][(symbol*frame_parms->N_RB_DL*12)];
  simde__m128i **llrp128 = (simde__m128i **)llrp;
  int i;

  for (i=0; i<(nb_rb*3); i++) {
    *(*llrp128) = *rxF;
    rxF++;
    (*llrp128)++;
  }

  return(0);
}

void ulsch_16qam_llr(LTE_DL_FRAME_PARMS *frame_parms,
                     int32_t **rxdataF_comp,
                     int16_t *ulsch_llr,
                     int32_t **ul_ch_mag,
                     uint8_t symbol,
                     uint16_t nb_rb,
                     int16_t **llrp) {
  int i;
  simde__m128i *rxF=(simde__m128i *)&rxdataF_comp[0][(symbol*frame_parms->N_RB_DL*12)];
  simde__m128i *ch_mag;
  simde__m128i mmtmpU0;
  simde__m128i **llrp128=(simde__m128i **)llrp;
  ch_mag =(simde__m128i *)&ul_ch_mag[0][(symbol*frame_parms->N_RB_DL*12)];

  for (i=0; i<(nb_rb*3); i++) {
    mmtmpU0 = simde_mm_abs_epi16(rxF[i]);
    mmtmpU0 = simde_mm_subs_epi16(ch_mag[i],mmtmpU0);
    (*llrp128)[0] = simde_mm_unpacklo_epi32(rxF[i],mmtmpU0);
    (*llrp128)[1] = simde_mm_unpackhi_epi32(rxF[i],mmtmpU0);
    (*llrp128)+=2;
    //    print_bytes("rxF[i]",&rxF[i]);
    //    print_bytes("rxF[i+1]",&rxF[i+1]);
  }

}

void ulsch_64qam_llr(LTE_DL_FRAME_PARMS *frame_parms,
                     int32_t **rxdataF_comp,
                     int16_t *ulsch_llr,
                     int32_t **ul_ch_mag,
                     int32_t **ul_ch_magb,
                     uint8_t symbol,
                     uint16_t nb_rb,
                     int16_t **llrp) {
  int i;
  int32_t **llrp32=(int32_t **)llrp;
  simde__m128i *rxF=(simde__m128i *)&rxdataF_comp[0][(symbol*frame_parms->N_RB_DL*12)];
  simde__m128i *ch_mag,*ch_magb;
  simde__m128i mmtmpU1,mmtmpU2;
  ch_mag =(simde__m128i *)&ul_ch_mag[0][(symbol*frame_parms->N_RB_DL*12)];
  ch_magb =(simde__m128i *)&ul_ch_magb[0][(symbol*frame_parms->N_RB_DL*12)];

  if(LOG_DEBUGFLAG(DEBUG_ULSCH)) {
    LOG_UI(PHY,"symbol %d: mag %d, magb %d\n",symbol,simde_mm_extract_epi16(ch_mag[0],0),simde_mm_extract_epi16(ch_magb[0],0));
  }

  for (i=0; i<(nb_rb*3); i++) {
    mmtmpU1 = simde_mm_subs_epi16(ch_mag[i], simde_mm_abs_epi16(rxF[i]));
    mmtmpU2 = simde_mm_subs_epi16(ch_magb[i], simde_mm_abs_epi16(mmtmpU1));
    (*llrp32)[0]  = simde_mm_extract_epi32(rxF[i],0);
    (*llrp32)[1]  = simde_mm_extract_epi32(mmtmpU1,0);
    (*llrp32)[2]  = simde_mm_extract_epi32(mmtmpU2,0);
    (*llrp32)[3]  = simde_mm_extract_epi32(rxF[i],1);
    (*llrp32)[4]  = simde_mm_extract_epi32(mmtmpU1,1);
    (*llrp32)[5]  = simde_mm_extract_epi32(mmtmpU2,1);
    (*llrp32)[6]  = simde_mm_extract_epi32(rxF[i],2);
    (*llrp32)[7]  = simde_mm_extract_epi32(mmtmpU1,2);
    (*llrp32)[8]  = simde_mm_extract_epi32(mmtmpU2,2);
    (*llrp32)[9]  = simde_mm_extract_epi32(rxF[i],3);
    (*llrp32)[10] = simde_mm_extract_epi32(mmtmpU1,3);
    (*llrp32)[11] = simde_mm_extract_epi32(mmtmpU2,3);
    (*llrp32)+=12;
  }

}

void ulsch_detection_mrc(LTE_DL_FRAME_PARMS *frame_parms,
                         int32_t **rxdataF_comp,
                         int32_t **ul_ch_mag,
                         int32_t **ul_ch_magb,
                         uint8_t symbol,
                         uint16_t nb_rb) {
  simde__m128i *rxdataF_comp128_0=NULL,*ul_ch_mag128_0=NULL,*ul_ch_mag128_0b=NULL;
  simde__m128i *rxdataF_comp128_1=NULL,*ul_ch_mag128_1=NULL,*ul_ch_mag128_1b=NULL;
  simde__m128i *rxdataF_comp128_2=NULL,*ul_ch_mag128_2=NULL,*ul_ch_mag128_2b=NULL;
  simde__m128i *rxdataF_comp128_3=NULL,*ul_ch_mag128_3=NULL,*ul_ch_mag128_3b=NULL;
  int32_t i;

  if (frame_parms->nb_antennas_rx>1) {
    rxdataF_comp128_0   = (simde__m128i *)&rxdataF_comp[0][symbol*frame_parms->N_RB_DL*12];
    rxdataF_comp128_1   = (simde__m128i *)&rxdataF_comp[1][symbol*frame_parms->N_RB_DL*12];
    ul_ch_mag128_0      = (simde__m128i *)&ul_ch_mag[0][symbol*frame_parms->N_RB_DL*12];
    ul_ch_mag128_1      = (simde__m128i *)&ul_ch_mag[1][symbol*frame_parms->N_RB_DL*12];
    ul_ch_mag128_0b     = (simde__m128i *)&ul_ch_magb[0][symbol*frame_parms->N_RB_DL*12];
    ul_ch_mag128_1b     = (simde__m128i *)&ul_ch_magb[1][symbol*frame_parms->N_RB_DL*12];
    if (frame_parms->nb_antennas_rx>2) { 
      rxdataF_comp128_2   = (simde__m128i *)&rxdataF_comp[2][symbol*frame_parms->N_RB_DL*12];
      ul_ch_mag128_2      = (simde__m128i *)&ul_ch_mag[2][symbol*frame_parms->N_RB_DL*12];
      ul_ch_mag128_2b     = (simde__m128i *)&ul_ch_magb[2][symbol*frame_parms->N_RB_DL*12];
    }
    if (frame_parms->nb_antennas_rx>3) { 
      rxdataF_comp128_3   = (simde__m128i *)&rxdataF_comp[3][symbol*frame_parms->N_RB_DL*12];
      ul_ch_mag128_3      = (simde__m128i *)&ul_ch_mag[3][symbol*frame_parms->N_RB_DL*12];
      ul_ch_mag128_3b     = (simde__m128i *)&ul_ch_magb[3][symbol*frame_parms->N_RB_DL*12];
    }

    // MRC on each re of rb, both on MF output and magnitude (for 16QAM/64QAM llr computation)
    if (frame_parms->nb_antennas_rx==2) 
      for (i=0; i<nb_rb*3; i++) {
        rxdataF_comp128_0[i] = simde_mm_srai_epi16(simde_mm_adds_epi16(rxdataF_comp128_0[i],rxdataF_comp128_1[i]),1);
        ul_ch_mag128_0[i]    = simde_mm_srai_epi16(simde_mm_adds_epi16(ul_ch_mag128_0[i],ul_ch_mag128_1[i]),1);
        ul_ch_mag128_0b[i]   = simde_mm_srai_epi16(simde_mm_adds_epi16(ul_ch_mag128_0b[i],ul_ch_mag128_1b[i]),1);
        rxdataF_comp128_0[i] = simde_mm_add_epi16(rxdataF_comp128_0[i], *(simde__m128i *)&jitterc[0]);
      }
    if (frame_parms->nb_antennas_rx==3)
      for (i=0; i<nb_rb*3; i++) {
        rxdataF_comp128_0[i] = simde_mm_srai_epi16(simde_mm_adds_epi16(rxdataF_comp128_0[i],simde_mm_adds_epi16(rxdataF_comp128_1[i],rxdataF_comp128_2[i])),1);
        ul_ch_mag128_0[i]    = simde_mm_srai_epi16(simde_mm_adds_epi16(ul_ch_mag128_0[i],simde_mm_adds_epi16(ul_ch_mag128_1[i],ul_ch_mag128_2[i])),1);
        ul_ch_mag128_0b[i]   = simde_mm_srai_epi16(simde_mm_adds_epi16(ul_ch_mag128_0b[i],simde_mm_adds_epi16(ul_ch_mag128_1b[i],ul_ch_mag128_2b[i])),1);
        rxdataF_comp128_0[i] = simde_mm_add_epi16(rxdataF_comp128_0[i], *(simde__m128i *)&jitterc[0]);
      }
     if (frame_parms->nb_antennas_rx==4)
      for (i=0; i<nb_rb*3; i++) {
        rxdataF_comp128_0[i] = simde_mm_srai_epi16(simde_mm_adds_epi16(rxdataF_comp128_0[i],simde_mm_adds_epi16(rxdataF_comp128_1[i],simde_mm_adds_epi16(rxdataF_comp128_2[i],rxdataF_comp128_3[i]))),2);
        ul_ch_mag128_0[i]    = simde_mm_srai_epi16(simde_mm_adds_epi16(ul_ch_mag128_0[i],simde_mm_adds_epi16(ul_ch_mag128_1[i],simde_mm_adds_epi16(ul_ch_mag128_2[i],ul_ch_mag128_3[i]))),2);
        ul_ch_mag128_0b[i]   = simde_mm_srai_epi16(simde_mm_adds_epi16(ul_ch_mag128_0b[i],simde_mm_adds_epi16(ul_ch_mag128_1b[i],simde_mm_adds_epi16(ul_ch_mag128_2b[i],ul_ch_mag128_3b[i]))),2);
        rxdataF_comp128_0[i] = simde_mm_add_epi16(rxdataF_comp128_0[i], *(simde__m128i *)&jitterc[0]);
      }
  }

}

void ulsch_extract_rbs_single(int32_t **rxdataF,
                              int32_t **rxdataF_ext,
                              uint32_t first_rb,
                              uint32_t nb_rb,
                              uint8_t l,
                              uint8_t Ns,
                              LTE_DL_FRAME_PARMS *frame_parms) {
  uint16_t nb_rb1,nb_rb2;
  uint8_t aarx;
  int32_t *rxF,*rxF_ext;
  //uint8_t symbol = l+Ns*frame_parms->symbols_per_tti/2;
  uint8_t symbol = l+((7-frame_parms->Ncp)*(Ns&1)); ///symbol within sub-frame
  AssertFatal((frame_parms->nb_antennas_rx>0) && (frame_parms->nb_antennas_rx<5),
              "nb_antennas_rx not in (1-4)\n");

  for (aarx=0; aarx<frame_parms->nb_antennas_rx; aarx++) {
    nb_rb1 = cmin(cmax((int)(frame_parms->N_RB_UL) - (int)(2*first_rb),(int)0),(int)(2*nb_rb));    // 2 times no. RBs before the DC
    nb_rb2 = 2*nb_rb - nb_rb1;                                   // 2 times no. RBs after the DC

    if(LOG_DEBUGFLAG(DEBUG_ULSCH)) {
      LOG_UI(PHY,"ulsch_extract_rbs_single: 2*nb_rb1 = %d, 2*nb_rb2 = %d\n",nb_rb1,nb_rb2);
    }

    rxF_ext   = &rxdataF_ext[aarx][(symbol*frame_parms->N_RB_UL*12)];

    if (nb_rb1) {
      rxF = &rxdataF[aarx][(first_rb*12 + frame_parms->first_carrier_offset + symbol*frame_parms->ofdm_symbol_size)];
      memcpy(rxF_ext, rxF, nb_rb1*6*sizeof(int));
      rxF_ext += nb_rb1*6;

      if (nb_rb2)  {
        rxF = &rxdataF[aarx][(symbol*frame_parms->ofdm_symbol_size)];
        memcpy(rxF_ext, rxF, nb_rb2*6*sizeof(int));
        rxF_ext += nb_rb2*6;
      }
    } else { //there is only data in the second half
      rxF = &rxdataF[aarx][(6*(2*first_rb - frame_parms->N_RB_UL) + symbol*frame_parms->ofdm_symbol_size)];
      memcpy(rxF_ext, rxF, nb_rb2*6*sizeof(int));
      rxF_ext += nb_rb2*6;
    }
  }
}

void ulsch_correct_ext(int32_t **rxdataF_ext,
                       int32_t **rxdataF_ext2,
                       uint16_t symbol,
                       LTE_DL_FRAME_PARMS *frame_parms,
                       uint16_t nb_rb) {
  int32_t i,j,aarx;
  int32_t *rxF_ext2,*rxF_ext;

  for (aarx=0; aarx<frame_parms->nb_antennas_rx; aarx++) {
    rxF_ext2 = &rxdataF_ext2[aarx][symbol*12*frame_parms->N_RB_UL];
    rxF_ext  = &rxdataF_ext[aarx][2*symbol*12*frame_parms->N_RB_UL];

    for (i=0,j=0; i<12*nb_rb; i++,j+=2) {
      rxF_ext2[i] = rxF_ext[j];
    }
  }
}



void ulsch_channel_compensation(int32_t **rxdataF_ext,
                                int32_t **ul_ch_estimates_ext,
                                int32_t **ul_ch_mag,
                                int32_t **rxdataF_comp,
                                LTE_DL_FRAME_PARMS *frame_parms,
                                uint8_t symbol,
                                uint16_t nb_rb,
                                uint8_t output_shift) {

  simde__m128i *ul_ch128, *ul_ch_mag128, *rxdataF128, *rxdataF_comp128;
  uint8_t aarx;//,symbol_mod;


  for (aarx=0; aarx<frame_parms->nb_antennas_rx; aarx++) {
    ul_ch128          = (simde__m128i *)&ul_ch_estimates_ext[aarx][symbol*frame_parms->N_RB_DL*12];
    ul_ch_mag128      = (simde__m128i *)&ul_ch_mag[aarx][symbol*frame_parms->N_RB_DL*12];
    rxdataF128        = (simde__m128i *)&rxdataF_ext[aarx][symbol*frame_parms->N_RB_DL*12];
    rxdataF_comp128   = (simde__m128i *)&rxdataF_comp[aarx][symbol*frame_parms->N_RB_DL*12];

    for (uint16_t rb=0; rb < nb_rb; rb++) {

      simde__m128i mmtmpU0,mmtmpU1,mmtmpU2;

      // just compute channel magnitude without scaling, this is done after equalization for SC-FDMA
      mmtmpU0 = simde_mm_srai_epi32(simde_mm_madd_epi16(ul_ch128[3*rb + 0], ul_ch128[3*rb + 0]), output_shift);
      mmtmpU1 = simde_mm_srai_epi32(simde_mm_madd_epi16(ul_ch128[3*rb + 1], ul_ch128[3*rb + 1]), output_shift);
      mmtmpU2 = simde_mm_srai_epi32(simde_mm_madd_epi16(ul_ch128[3*rb + 2], ul_ch128[3*rb + 2]), output_shift);

      mmtmpU0 = simde_mm_packs_epi32(mmtmpU0, mmtmpU1);
      mmtmpU1 = simde_mm_packs_epi32(mmtmpU2, mmtmpU2);

      ul_ch_mag128[3*rb + 0] = simde_mm_unpacklo_epi16(mmtmpU0, mmtmpU0);
      ul_ch_mag128[3*rb + 1] = simde_mm_unpackhi_epi16(mmtmpU0, mmtmpU0);
      ul_ch_mag128[3*rb + 2] = simde_mm_unpacklo_epi16(mmtmpU1, mmtmpU1);
      //LOG_I(PHY, "comp: ant %d symbol %d rb %d => %d,%d,%d (output_shift %d)\n", aarx, symbol, rb, *((int16_t *)&ul_ch_mag128[0]), *((int16_t *)&ul_ch_mag128[1]), *((int16_t *)&ul_ch_mag128[2]), output_shift );

    }
    mult_cpx_conj_vector((c16_t *)ul_ch128, (c16_t *)rxdataF128, (c16_t *)rxdataF_comp128, 12 * nb_rb, output_shift);

    // Add a jitter to compensate for the saturation in "packs" resulting in a bias on the DC after IDFT
    for (uint16_t rb = 0; rb < 3*nb_rb; rb++) {
      rxdataF_comp128[rb] = simde_mm_add_epi16(rxdataF_comp128[rb], *(simde__m128i *)jitter);
    }
  }

}

void ulsch_channel_level(int32_t **drs_ch_estimates_ext,
                         LTE_DL_FRAME_PARMS *frame_parms,
                         int32_t *avg,
                         uint16_t nb_rb) {
  int16_t rb;
  uint8_t aarx;
  simde__m128i *ul_ch128;
  simde__m128 avg128U;

  for (aarx=0; aarx<frame_parms->nb_antennas_rx; aarx++) {
    //clear average level
    avg128U = simde_mm_setzero_ps();
    ul_ch128=(simde__m128i *)drs_ch_estimates_ext[aarx];

    for (rb=0; rb<nb_rb; rb++) {
      avg128U = simde_mm_add_ps(avg128U,simde_mm_cvtepi32_ps(simde_mm_madd_epi16(ul_ch128[0],ul_ch128[0])));
      avg128U = simde_mm_add_ps(avg128U,simde_mm_cvtepi32_ps(simde_mm_madd_epi16(ul_ch128[1],ul_ch128[1])));
      avg128U = simde_mm_add_ps(avg128U,simde_mm_cvtepi32_ps(simde_mm_madd_epi16(ul_ch128[2],ul_ch128[2])));
      ul_ch128+=3;
    }

    DevAssert( nb_rb );
    avg[aarx] = (int)((((float *)&avg128U)[0] +
                       ((float *)&avg128U)[1] +
                       ((float *)&avg128U)[2] +
                       ((float *)&avg128U)[3])/(float)(nb_rb*12));
  }

}

static int ulsch_power_LUT[750];

void init_ulsch_power_LUT(void) {
  int i;

  for (i=0; i<750; i++) ulsch_power_LUT[i] = (int)ceil((pow(2.0,(double)i/100) - 1.0));
}

void rx_ulsch(PHY_VARS_eNB *eNB,
              L1_rxtx_proc_t *proc,
              uint8_t UE_id) {
  LTE_eNB_ULSCH_t **ulsch = eNB->ulsch;
  LTE_eNB_COMMON *common_vars = &eNB->common_vars;
  LTE_eNB_PUSCH *pusch_vars = eNB->pusch_vars[UE_id];
  LTE_DL_FRAME_PARMS *frame_parms = &eNB->frame_parms;
  uint32_t l,i;
  int32_t avgs;
  uint8_t log2_maxh=0,aarx;
  int32_t avgU[eNB->frame_parms.nb_antennas_rx];
  //  uint8_t harq_pid = ( ulsch->RRCConnRequest_flag== 0) ? subframe2harq_pid_tdd(frame_parms->tdd_config,subframe) : 0;
  uint8_t harq_pid;
  uint8_t Qm;
  int16_t *llrp;
  int subframe = proc->subframe_rx;

  if (ulsch[UE_id]->ue_type > 0) harq_pid =0;
  else {
    harq_pid = subframe2harq_pid(frame_parms,proc->frame_rx,subframe);
  }

  Qm = ulsch[UE_id]->harq_processes[harq_pid]->Qm;

  if(LOG_DEBUGFLAG(DEBUG_ULSCH)) {
    LOG_I(PHY,"rx_ulsch: harq_pid %d, nb_rb %d first_rb %d\n",harq_pid,ulsch[UE_id]->harq_processes[harq_pid]->nb_rb,ulsch[UE_id]->harq_processes[harq_pid]->first_rb);
  }

  AssertFatal(ulsch[UE_id]->harq_processes[harq_pid]->nb_rb > 0,
              "PUSCH (%d/%x) nb_rb=0!\n", harq_pid,ulsch[UE_id]->rnti);


  for (l=0; l<(frame_parms->symbols_per_tti-ulsch[UE_id]->harq_processes[harq_pid]->srs_active); l++) {
    if(LOG_DEBUGFLAG(DEBUG_ULSCH)) {
      LOG_I(PHY,"rx_ulsch : symbol %d (first_rb %d,nb_rb %d), rxdataF %p, rxdataF_ext %p\n",l,
            ulsch[UE_id]->harq_processes[harq_pid]->first_rb,
            ulsch[UE_id]->harq_processes[harq_pid]->nb_rb,
            common_vars->rxdataF,
            pusch_vars->rxdataF_ext);
    }

    ulsch_extract_rbs_single(common_vars->rxdataF,
                             pusch_vars->rxdataF_ext,
                             ulsch[UE_id]->harq_processes[harq_pid]->first_rb,
                             ulsch[UE_id]->harq_processes[harq_pid]->nb_rb,
                             l%(frame_parms->symbols_per_tti/2),
                             l/(frame_parms->symbols_per_tti/2),
                             frame_parms);
    lte_ul_channel_estimation(&eNB->frame_parms,
                              proc,
                              eNB->ulsch[UE_id],
                              (c16_t **)eNB->pusch_vars[UE_id]->drs_ch_estimates,
                              (c16_t **)eNB->pusch_vars[UE_id]->drs_ch_estimates_time,
                              (c16_t **)eNB->pusch_vars[UE_id]->rxdataF_ext,
                              l % (frame_parms->symbols_per_tti / 2),
                              l / (frame_parms->symbols_per_tti / 2));
  }

  int correction_factor = 1;
  int deltaMCS=1;
  int MPR_times_100Ks;

  if (deltaMCS==1) {
    // Note we're using TBS instead of sumKr, since didn't run segmentation yet!
    MPR_times_100Ks = 500*ulsch[UE_id]->harq_processes[harq_pid]->TBS/(ulsch[UE_id]->harq_processes[harq_pid]->nb_rb*12*4*ulsch[UE_id]->harq_processes[harq_pid]->Nsymb_pusch);

    if ((MPR_times_100Ks > 0)&&(MPR_times_100Ks < 750)){
      correction_factor = ulsch_power_LUT[MPR_times_100Ks];
    }else{
      correction_factor = 1;
    }
  }

  for (i=0; i<frame_parms->nb_antennas_rx; i++) {
    pusch_vars->ulsch_power[i] = signal_energy_nodc((c16_t*)pusch_vars->drs_ch_estimates[i],
                                 ulsch[UE_id]->harq_processes[harq_pid]->nb_rb*12)/correction_factor;
    LOG_D(PHY,"%4.4d.%d power harq_pid %d rb %2.2d TBS %2.2d (MPR_times_Ks %d correction %d)  power %d dBtimes10\n", proc->frame_rx, proc->subframe_rx, harq_pid,
          ulsch[UE_id]->harq_processes[harq_pid]->nb_rb, ulsch[UE_id]->harq_processes[harq_pid]->TBS,MPR_times_100Ks,correction_factor,dB_fixed_x10(pusch_vars->ulsch_power[i]));
    pusch_vars->ulsch_noise_power[i]=0;
    for (int rb=0;rb<ulsch[UE_id]->harq_processes[harq_pid]->nb_rb;rb++)
      pusch_vars->ulsch_noise_power[i]+=eNB->measurements.n0_subband_power[i][rb]/ulsch[UE_id]->harq_processes[harq_pid]->nb_rb;
    LOG_D(PHY,"noise power[%d] %d\n",i,dB_fixed_x10(pusch_vars->ulsch_noise_power[i]));
/* Check this modification w.r.t to new PUSCH modifications
    //symbol 3
    int symbol_offset = frame_parms->N_RB_UL*12*(3 - frame_parms->Ncp);
    pusch_vars->ulsch_interference_power[i] = interference_power(&pusch_vars->drs_ch_estimates[i][symbol_offset],ulsch[UE_id]->harq_processes[harq_pid]->nb_rb*12);
    pusch_vars->ulsch_power[i] = signal_power(&pusch_vars->drs_ch_estimates[i][symbol_offset],ulsch[UE_id]->harq_processes[harq_pid]->nb_rb*12);
    //symbol 3+7
    symbol_offset = frame_parms->N_RB_UL*12*((3 - frame_parms->Ncp)+(7-frame_parms->Ncp));
    pusch_vars->ulsch_interference_power[i] += interference_power(&pusch_vars->drs_ch_estimates[i][symbol_offset],ulsch[UE_id]->harq_processes[harq_pid]->nb_rb*12);
    pusch_vars->ulsch_power[i] += signal_power(&pusch_vars->drs_ch_estimates[i][symbol_offset],ulsch[UE_id]->harq_processes[harq_pid]->nb_rb*12);

    pusch_vars->ulsch_interference_power[i] = pusch_vars->ulsch_interference_power[i]/correction_factor;
    pusch_vars->ulsch_power[i] = pusch_vars->ulsch_power[i]/correction_factor;

    if(pusch_vars->ulsch_power[i]>0x20000000){
      pusch_vars->ulsch_power[i] = 0x20000000;
      pusch_vars->ulsch_interference_power[i] = 1;
    }else{
      pusch_vars->ulsch_power[i] -=  pusch_vars->ulsch_interference_power[i];
      if(pusch_vars->ulsch_power[i]<1) pusch_vars->ulsch_power[i] = 1;
      if(pusch_vars->ulsch_interference_power[i]<1)pusch_vars->ulsch_interference_power[i] = 1;
    }

    LOG_D(PHY,"%4.4d.%d power harq_pid %d rb %2.2d TBS %2.2d (MPR_times_Ks %d correction %d)  power %d dBtimes10\n", proc->frame_rx, proc->subframe_rx, harq_pid, ulsch[UE_id]->harq_processes[harq_pid]->nb_rb, ulsch[UE_id]->harq_processes[harq_pid]->TBS,MPR_times_100Ks,correction_factor,dB_fixed_x10(pusch_vars->ulsch_power[i]));
  */   
  }

  ulsch_channel_level(pusch_vars->drs_ch_estimates,
                      frame_parms,
                      avgU,
                      ulsch[UE_id]->harq_processes[harq_pid]->nb_rb);
  LOG_D(PHY,"[ULSCH] avg[0] %d\n",avgU[0]);
  avgs = 0;

  for (aarx=0; aarx<frame_parms->nb_antennas_rx; aarx++)
    avgs = cmax(avgs,avgU[aarx]);

  log2_maxh = 4+(log2_approx(avgs)/2); 
  LOG_D(PHY,"[ULSCH] log2_maxh = %d (%d,%d)\n",log2_maxh,avgU[0],avgs);

  for (l=0; l<(frame_parms->symbols_per_tti-ulsch[UE_id]->harq_processes[harq_pid]->srs_active); l++) {
    if (((frame_parms->Ncp == 0) && ((l==3) || (l==10)))||   // skip pilots
        ((frame_parms->Ncp == 1) && ((l==2) || (l==8)))) {
      l++;
    }

    ulsch_channel_compensation(
      pusch_vars->rxdataF_ext,
      pusch_vars->drs_ch_estimates,
      pusch_vars->ul_ch_mag,
      pusch_vars->rxdataF_comp,
      frame_parms,
      l,
      ulsch[UE_id]->harq_processes[harq_pid]->nb_rb,
      log2_maxh); // log2_maxh+I0_shift

    if (frame_parms->nb_antennas_rx > 1)
      ulsch_detection_mrc(frame_parms,
                          pusch_vars->rxdataF_comp,
                          pusch_vars->ul_ch_mag,
                          pusch_vars->ul_ch_magb,
                          l,
                          ulsch[UE_id]->harq_processes[harq_pid]->nb_rb);

    //    if ((eNB->measurements.n0_power_dB[0]+3)<pusch_vars->ulsch_power[0])
    if (23<pusch_vars->ulsch_power[0]) {
      freq_equalization(frame_parms,
                        (c16_t **)pusch_vars->rxdataF_comp,
                        (c16_t **)pusch_vars->ul_ch_mag,
                        (c16_t **)pusch_vars->ul_ch_magb,
                        l,
                        ulsch[UE_id]->harq_processes[harq_pid]->nb_rb * 12,
                        Qm);
    }
  }

  lte_idft(frame_parms,
           (uint32_t *)pusch_vars->rxdataF_comp[0],
           ulsch[UE_id]->harq_processes[harq_pid]->nb_rb*12);
  llrp = (int16_t *)&pusch_vars->llr[0];
  T(T_ENB_PHY_PUSCH_IQ, T_INT(0), T_INT(ulsch[UE_id]->rnti), T_INT(proc->frame_rx),
    T_INT(subframe), T_INT(ulsch[UE_id]->harq_processes[harq_pid]->nb_rb),
    T_INT(frame_parms->N_RB_UL), T_INT(frame_parms->symbols_per_tti),
    T_BUFFER(pusch_vars->rxdataF_comp[0],
             2 * /* ulsch[UE_id]->harq_processes[harq_pid]->nb_rb */ frame_parms->N_RB_UL *12*frame_parms->symbols_per_tti*2));

  for (l=0; l<frame_parms->symbols_per_tti-ulsch[UE_id]->harq_processes[harq_pid]->srs_active; l++) {
    if (((frame_parms->Ncp == 0) && ((l==3) || (l==10)))||   // skip pilots
        ((frame_parms->Ncp == 1) && ((l==2) || (l==8)))) {
      l++;
    }

    switch (Qm) {
      case 2 :
        ulsch_qpsk_llr(frame_parms,
                       pusch_vars->rxdataF_comp,
                       pusch_vars->llr,
                       l,
                       ulsch[UE_id]->harq_processes[harq_pid]->nb_rb,
                       &llrp);
        break;

      case 4 :
        ulsch_16qam_llr(frame_parms,
                        pusch_vars->rxdataF_comp,
                        pusch_vars->llr,
                        pusch_vars->ul_ch_mag,
                        l,ulsch[UE_id]->harq_processes[harq_pid]->nb_rb,
                        &llrp);
        break;

      case 6 :
        ulsch_64qam_llr(frame_parms,
                        pusch_vars->rxdataF_comp,
                        pusch_vars->llr,
                        pusch_vars->ul_ch_mag,
                        pusch_vars->ul_ch_magb,
                        l,ulsch[UE_id]->harq_processes[harq_pid]->nb_rb,
                        &llrp);
        break;

      default:
        LOG_E(PHY,"ulsch_demodulation.c (rx_ulsch): Unknown Qm!!!!\n");
        break;
    }
  }
}

void rx_ulsch_emul(PHY_VARS_eNB *eNB,
                   L1_rxtx_proc_t *proc,
                   uint8_t UE_index) {
  LOG_I(PHY,"[PHY] EMUL eNB %d rx_ulsch_emul : subframe %d, UE_index %d\n",eNB->Mod_id,proc->subframe_rx,UE_index);
  eNB->pusch_vars[UE_index]->ulsch_power[0] = 31622; //=45dB;
  eNB->pusch_vars[UE_index]->ulsch_power[1] = 31622; //=45dB;
}


void dump_ulsch(PHY_VARS_eNB *eNB,int frame,int subframe,uint8_t UE_id,int round) {
  uint32_t nsymb = (eNB->frame_parms.Ncp == 0) ? 14 : 12;
  uint8_t harq_pid;
  char fname[100],vname[100];
  harq_pid = subframe2harq_pid(&eNB->frame_parms,frame,subframe);
  LOG_UI(PHY,"Dumping ULSCH in subframe %d with harq_pid %d, round %d for NB_rb %d, first_rb %d, TBS %d, Qm %d, N_symb %d\n",
         subframe,harq_pid,round,eNB->ulsch[UE_id]->harq_processes[harq_pid]->nb_rb,
         eNB->ulsch[UE_id]->harq_processes[harq_pid]->first_rb,
         eNB->ulsch[UE_id]->harq_processes[harq_pid]->TBS,eNB->ulsch[UE_id]->harq_processes[harq_pid]->Qm,
         eNB->ulsch[UE_id]->harq_processes[harq_pid]->Nsymb_pusch);
  for (int aa=0;aa<eNB->frame_parms.nb_antennas_rx;aa++)
     LOG_UI(PHY,"ulsch_power[%d] %d, ulsch_noise_power[%d] %d\n",aa,dB_fixed_x10(eNB->pusch_vars[UE_id]->ulsch_power[aa]),aa,dB_fixed_x10(eNB->pusch_vars[UE_id]->ulsch_noise_power[aa]));
  sprintf(fname,"/tmp/ulsch_r%d_d",round);
  sprintf(vname,"/tmp/ulsch_r%d_dseq",round);
  LOG_UM(fname,vname,&eNB->ulsch[UE_id]->harq_processes[harq_pid]->d[0][96],
         eNB->ulsch[UE_id]->harq_processes[harq_pid]->Kplus*3,1,0);

  if (eNB->common_vars.rxdata) {
    for (int aarx=0;aarx<eNB->frame_parms.nb_antennas_rx;aarx++) {
       sprintf(fname,"/tmp/rxsig%d_r%d.m",aarx,round);
       sprintf(vname,"rxs%d_r%d",aarx,round);
       LOG_UM(fname,vname, &eNB->common_vars.rxdata[aarx][0],eNB->frame_parms.samples_per_tti*10,1,1);

    }
  }
  if (eNB->common_vars.rxdataF) {
   for (int aarx=0;aarx<eNB->frame_parms.nb_antennas_rx;aarx++) {
       sprintf(fname,"/tmp/rxsigF%d_r%d.m",aarx,round);
       sprintf(vname,"rxsF%d_r%d",aarx,round);
       LOG_UM(fname,vname, (void *)&eNB->common_vars.rxdataF[aarx][0],eNB->frame_parms.ofdm_symbol_size*nsymb,1,1);
    }
  }
  if (eNB->pusch_vars[UE_id]->rxdataF_ext) {
    for (int aarx=0;aarx<eNB->frame_parms.nb_antennas_rx;aarx++) {
      sprintf(fname,"/tmp/rxsigF%d_ext_r%d.m",aarx,round);
      sprintf(vname,"rxsF%d_ext_r%d",aarx,round);
      LOG_UM(fname,vname, &eNB->pusch_vars[UE_id]->rxdataF_ext[aarx][0],eNB->frame_parms.N_RB_UL*12*nsymb,1,1);
    }
  }
  /*
  if (eNB->srs_vars[UE_id].srs_ch_estimates) LOG_UM("/tmp/srs_est0.m","srsest0",eNB->srs_vars[UE_id].srs_ch_estimates[0],eNB->frame_parms.ofdm_symbol_size,1,1);

  if (eNB->frame_parms.nb_antennas_rx>1)
    if (eNB->srs_vars[UE_id].srs_ch_estimates) LOG_UM("/tmp/srs_est1.m","srsest1",eNB->srs_vars[UE_id].srs_ch_estimates[1],eNB->frame_parms.ofdm_symbol_size,1,1);
  */
  for (int aarx=0;aarx<eNB->frame_parms.nb_antennas_rx;aarx++) {
    sprintf(fname,"/tmp/drs_est%d_r%d.m",aarx,round);
    sprintf(vname,"drsest%d_r%d",aarx,round);
    LOG_UM(fname,vname,eNB->pusch_vars[UE_id]->drs_ch_estimates[aarx],eNB->frame_parms.N_RB_UL*12*nsymb,1,1);

    sprintf(fname,"/tmp/ulsch%d_rxF_comp0_r%d.m",aarx,round);
    sprintf(vname,"ulsch_rxF%d_comp0_r%d",aarx,round);
    LOG_UM(fname,vname,&eNB->pusch_vars[UE_id]->rxdataF_comp[aarx][0],eNB->frame_parms.N_RB_UL*12*nsymb,1,1);
  }

  //  LOG_M("ulsch_rxF_comp1.m","ulsch0_rxF_comp1",&eNB->pusch_vars[UE_id]->rxdataF_comp[0][1][0],eNB->frame_parms.N_RB_UL*12*nsymb,1,1);
  sprintf(fname,"/tmp/ulsch_rxF_llr_r%d.m",round);
  sprintf(vname,"ulsch_llr_r%d",round);
  LOG_UM(fname,vname,eNB->pusch_vars[UE_id]->llr,
         eNB->ulsch[UE_id]->harq_processes[harq_pid]->nb_rb*12*eNB->ulsch[UE_id]->harq_processes[harq_pid]->Qm
         *eNB->ulsch[UE_id]->harq_processes[harq_pid]->Nsymb_pusch,1,0);
  sprintf(fname,"/tmp/ulsch_ch_mag_r%d.m",round);
  sprintf(vname,"ulsch_ch_mag_r%d",round);
  LOG_UM(fname,vname,&eNB->pusch_vars[UE_id]->ul_ch_mag[0][0],eNB->frame_parms.N_RB_UL*12*nsymb,1,1);
  //  LOG_UM("ulsch_ch_mag1.m","ulsch_ch_mag1",&eNB->pusch_vars[UE_id]->ul_ch_mag[1][0],eNB->frame_parms.N_RB_UL*12*nsymb,1,1);
  //#endif
}

