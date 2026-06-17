/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Top-level routines for generating PUSCH physical channel from 36.211 V8.6 2009-03
 */
#include "PHY/defs_UE.h"
#include "PHY/phy_extern_ue.h"
#include "PHY/CODING/coding_defs.h"
#include "PHY/CODING/coding_extern.h"
#include "PHY/LTE_UE_TRANSPORT/transport_ue.h"
#include "PHY/LTE_UE_TRANSPORT/transport_proto_ue.h"
#include "PHY/LTE_TRANSPORT/transport_eNB.h"
#include "common/utils/LOG/vcd_signal_dumper.h"
#include "PHY/LTE_REFSIG/lte_refsig.h"
#include "PHY/LTE_TRANSPORT/transport_vars.h"


//#define DEBUG_ULSCH_MODULATION

#if defined(__aarch64__)

void dft_lte(int32_t *z,
             struct complex16 *input,
             int32_t Msc_PUSCH,
             uint8_t Nsymb)
{
  simde__m128i dft_in128[4][1200];
  simde__m128i dft_out128[4][1200];

  uint32_t *dft_in0 = (uint32_t *)dft_in128[0];
  uint32_t *dft_out0 = (uint32_t *)dft_out128[0];

  uint32_t *dft_in1 = (uint32_t *)dft_in128[1];
  uint32_t *dft_out1 = (uint32_t *)dft_out128[1];

  uint32_t *dft_in2 = (uint32_t *)dft_in128[2];
  uint32_t *dft_out2 = (uint32_t *)dft_out128[2];

  uint32_t *d0;
  uint32_t *d1;
  uint32_t *d2;
  uint32_t *d3;
  uint32_t *d4;
  uint32_t *d5;
  uint32_t *d6;
  uint32_t *d7;
  uint32_t *d8;
  uint32_t *d9;
  uint32_t *d10;
  uint32_t *d11;

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
  uint32_t *z10;
  uint32_t *z11;

  uint32_t i;
  uint32_t ip;

  simde__m128i norm128;

  (void)Nsymb;

  d0 = (uint32_t *)input;
  d1 = d0 + Msc_PUSCH;
  d2 = d1 + Msc_PUSCH;
  d3 = d2 + Msc_PUSCH;
  d4 = d3 + Msc_PUSCH;
  d5 = d4 + Msc_PUSCH;
  d6 = d5 + Msc_PUSCH;
  d7 = d6 + Msc_PUSCH;
  d8 = d7 + Msc_PUSCH;
  d9 = d8 + Msc_PUSCH;
  d10 = d9 + Msc_PUSCH;
  d11 = d10 + Msc_PUSCH;

  for (i = 0, ip = 0; i < Msc_PUSCH; i++, ip += 4) {
    dft_in0[ip] = d0[i];
    dft_in0[ip + 1] = d1[i];
    dft_in0[ip + 2] = d2[i];
    dft_in0[ip + 3] = d3[i];

    dft_in1[ip] = d4[i];
    dft_in1[ip + 1] = d5[i];
    dft_in1[ip + 2] = d6[i];
    dft_in1[ip + 3] = d7[i];

    dft_in2[ip] = d8[i];
    dft_in2[ip + 1] = d9[i];
    dft_in2[ip + 2] = d10[i];
    dft_in2[ip + 3] = d11[i];
  }

  const dft_size_idx_t dftsize = get_dft(Msc_PUSCH);

  if (dftsize == DFT_SIZE_IDXTABLESIZE) {
    LOG_E(PHY,
          "Internal error, not modulating the slot, "
          "Msc_PUSCH = %d\n",
          Msc_PUSCH);
    return;
  }

  switch (Msc_PUSCH) {
    case 12:
      dft(dftsize, (int16_t *)dft_in0, (int16_t *)dft_out0, 0);

      dft(dftsize, (int16_t *)dft_in1, (int16_t *)dft_out1, 0);

      dft(dftsize, (int16_t *)dft_in2, (int16_t *)dft_out2, 0);

      norm128 = simde_mm_set1_epi16(9459);

      for (i = 0; i < 12; i++) {
        ((simde__m128i *)dft_out0)[i] = simde_mm_slli_epi16(simde_mm_mulhi_epi16(((simde__m128i *)dft_out0)[i], norm128), 1);

        ((simde__m128i *)dft_out1)[i] = simde_mm_slli_epi16(simde_mm_mulhi_epi16(((simde__m128i *)dft_out1)[i], norm128), 1);

        ((simde__m128i *)dft_out2)[i] = simde_mm_slli_epi16(simde_mm_mulhi_epi16(((simde__m128i *)dft_out2)[i], norm128), 1);
      }
      break;

    default:
      dft(dftsize, (int16_t *)dft_in0, (int16_t *)dft_out0, 1);

      dft(dftsize, (int16_t *)dft_in1, (int16_t *)dft_out1, 1);

      dft(dftsize, (int16_t *)dft_in2, (int16_t *)dft_out2, 1);
      break;
  }

  z0 = (uint32_t *)z;
  z1 = z0 + Msc_PUSCH;
  z2 = z1 + Msc_PUSCH;
  z3 = z2 + Msc_PUSCH;
  z4 = z3 + Msc_PUSCH;
  z5 = z4 + Msc_PUSCH;
  z6 = z5 + Msc_PUSCH;
  z7 = z6 + Msc_PUSCH;
  z8 = z7 + Msc_PUSCH;
  z9 = z8 + Msc_PUSCH;
  z10 = z9 + Msc_PUSCH;
  z11 = z10 + Msc_PUSCH;

  for (i = 0, ip = 0; i < Msc_PUSCH; i++, ip += 4) {
    z0[i] = dft_out0[ip];
    z1[i] = dft_out0[ip + 1];
    z2[i] = dft_out0[ip + 2];
    z3[i] = dft_out0[ip + 3];

    z4[i] = dft_out1[ip];
    z5[i] = dft_out1[ip + 1];
    z6[i] = dft_out1[ip + 2];
    z7[i] = dft_out1[ip + 3];

    z8[i] = dft_out2[ip];
    z9[i] = dft_out2[ip + 1];
    z10[i] = dft_out2[ip + 2];
    z11[i] = dft_out2[ip + 3];
  }
}

#else

void dft_lte(int32_t *z, struct complex16 *input, int32_t Msc_PUSCH, uint8_t Nsymb)
{
  const dft_size_idx_t dftsize = get_dft(Msc_PUSCH);

  c16_t dft_in[Msc_PUSCH] __attribute__((aligned(64)));

  c16_t dft_out[Msc_PUSCH] __attribute__((aligned(64)));

  const c16_t *src = (const c16_t *)input;
  c16_t *dst = (c16_t *)z;

  const size_t bytes =
      (size_t)Msc_PUSCH * sizeof(c16_t);

  for (uint8_t symbol = 0; symbol < Nsymb; symbol++) {
    const size_t offset = (size_t)symbol * (size_t)Msc_PUSCH;

    memcpy(dft_in, src + offset, bytes);

    dft(dftsize,
        (int16_t *)dft_in,
        (int16_t *)dft_out,
        1);

    memcpy(dst + offset, dft_out, bytes);
  }
}
#endif

void ulsch_modulation(int32_t **txdataF,
                      short amp,
                      uint32_t frame,
                      uint32_t subframe,
                      LTE_DL_FRAME_PARMS *frame_parms,
                      LTE_UE_ULSCH_t *ulsch)
{

  uint8_t qam64_table_offset_re = 0;
  uint8_t qam64_table_offset_im = 0;
  uint8_t qam16_table_offset_re = 0;
  uint8_t qam16_table_offset_im = 0;
  short gain_lin_QPSK;

  DevAssert(frame_parms);

  int re_offset,re_offset0,i,ulsch_Msymb,j,k,nsymb,Msc_PUSCH,l;
  //  uint8_t harq_pid = (rag_flag == 1) ? 0 : subframe2harq_pid_tdd(frame_parms->tdd_config,subframe);
  uint8_t harq_pid = subframe2harq_pid(frame_parms,frame,subframe);
  uint8_t Q_m;
  int32_t *txptr;
  uint32_t symbol_offset;
  uint16_t first_rb;
  uint16_t nb_rb;
  int G;

  uint32_t x1, x2, s=0;
  uint8_t c;

  if (!ulsch) {
    printf("ulsch_modulation.c: Null ulsch\n");
    return;
  }

  // x1 is set in gold_generic
  x2 = (ulsch->rnti<<14) + (subframe<<9) + frame_parms->Nid_cell; //this is c_init in 36.211 Sec 6.3.1

  if (harq_pid>=8) {
    printf("ulsch_modulation.c: Illegal harq_pid %d\n",harq_pid);
    return;
  }

  first_rb = ulsch->harq_processes[harq_pid]->first_rb;
  nb_rb = ulsch->harq_processes[harq_pid]->nb_rb;

  if (nb_rb == 0) {
    printf("ulsch_modulation.c: Frame %u, Subframe %u Illegal nb_rb %d\n",frame,subframe,nb_rb);
    return;
  }

  if (first_rb > frame_parms->N_RB_UL) {
    printf("ulsch_modulation.c: Frame %u, Subframe %u Illegal first_rb %d\n",frame,subframe,first_rb);
    return;
  }

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_ULSCH_MODULATION, VCD_FUNCTION_IN);

  Q_m = get_Qm_ul(ulsch->harq_processes[harq_pid]->mcs);

  G = (int)ulsch->harq_processes[harq_pid]->nb_rb * (12 * Q_m) * (ulsch->Nsymb_pusch);


  // Mapping
  nsymb = (frame_parms->Ncp==0) ? 14:12;
  Msc_PUSCH = ulsch->harq_processes[harq_pid]->nb_rb*12;

#ifdef DEBUG_ULSCH_MODULATION
  LOG_D(PHY,"ulsch_modulation.c: Doing modulation (rnti %x,x2 %x) for G=%d bits, harq_pid %d , nb_rb %d, Q_m %d, Nsymb_pusch %d (nsymb %d), subframe %d\n",
        ulsch->rnti,x2,G,harq_pid,ulsch->harq_processes[harq_pid]->nb_rb,Q_m, ulsch->Nsymb_pusch,nsymb,subframe);
#endif

  // scrambling (Note the placeholding bits are handled in ulsch_coding.c directly!)
  //printf("ulsch bits: ");
  s = gold_generic(&x1, &x2, 1);
  k=0;

  //printf("G %d\n",G);
  for (i=0; i<(1+(G>>5)); i++) {
    for (j=0; j<32; j++,k++) {
      c = (uint8_t)((s>>j)&1);

      if (ulsch->h[k] == PUSCH_x) {
        //  printf("i %d: PUSCH_x\n",i);
        ulsch->b_tilde[k] = 1;
      } else if (ulsch->h[k] == PUSCH_y) {
        //  printf("i %d: PUSCH_y\n",i);
        ulsch->b_tilde[k] = ulsch->b_tilde[k-1];
      } else {
        ulsch->b_tilde[k] = (ulsch->h[k]+c)&1;
        //  printf("i %d : %d (h %d c %d)\n", (i<<5)+j,ulsch->b_tilde[k],ulsch->h[k],c);
      }

    }

    s = gold_generic(&x1, &x2, 0);
  }

  //printf("\n");


  gain_lin_QPSK = (short)((amp*ONE_OVER_SQRT2_Q15)>>15);


  // Modulation

  ulsch_Msymb = G/Q_m;
  /// Modulated "d"-sequences (for definition see 36-211 V8.6 2009-03, p.14)
  struct complex16 d[MAX_NUM_RE] __attribute__((aligned(32)));
  if(ulsch->cooperation_flag == 2)
    // For Distributed Alamouti Scheme in Collabrative Communication
  {
    for (i=0,j=Q_m; i<ulsch_Msymb; i+=2,j+=2*Q_m) {

      switch (Q_m) {

      case 2:


        //UE1, -x1*
        d[i].r = (ulsch->b_tilde[j] == 1)  ? (gain_lin_QPSK) : -gain_lin_QPSK;
        d[i].i = (ulsch->b_tilde[j+1] == 1)? (-gain_lin_QPSK) : gain_lin_QPSK;
        //      if (i<Msc_PUSCH)
        //  printf("input %d (%p): %d,%d\n", i,&ulsch->d[i],((int16_t*)&ulsch->d[i])[0],((int16_t*)&ulsch->d[i])[1]);

        // UE1, x0*
        d[i+1].r = (ulsch->b_tilde[j-2] == 1)  ? (-gain_lin_QPSK) : gain_lin_QPSK;
        d[i+1].i = (ulsch->b_tilde[j-1] == 1)? (gain_lin_QPSK) : -gain_lin_QPSK;

        break;

      case 4:


        //UE1,-x1*
        qam16_table_offset_re = 0;
        qam16_table_offset_im = 0;

        if (ulsch->b_tilde[j] == 1)
          qam16_table_offset_re+=2;

        if (ulsch->b_tilde[j+1] == 1)
          qam16_table_offset_im+=2;



        if (ulsch->b_tilde[j+2] == 1)
          qam16_table_offset_re+=1;

        if (ulsch->b_tilde[j+3] == 1)
          qam16_table_offset_im+=1;


        d[i].r =-(int16_t)(((int32_t)amp*qam16_table[qam16_table_offset_re])>>15);
        d[i].i =(int16_t)(((int32_t)amp*qam16_table[qam16_table_offset_im])>>15);

        //UE1,x0*
        qam16_table_offset_re = 0;
        qam16_table_offset_im = 0;

        if (ulsch->b_tilde[j-4] == 1)
          qam16_table_offset_re+=2;

        if (ulsch->b_tilde[j-3] == 1)
          qam16_table_offset_im+=2;


        if (ulsch->b_tilde[j-2] == 1)
          qam16_table_offset_re+=1;

        if (ulsch->b_tilde[j-1] == 1)
          qam16_table_offset_im+=1;


        //    ((int16_t*)&ulsch->d[i+1])[0]=-(int16_t)(((int32_t)amp*qam16_table[qam16_table_offset_re])>>15);
        //    ((int16_t*)&ulsch->d[i+1])[1]=(int16_t)(((int32_t)amp*qam16_table[qam16_table_offset_im])>>15);
        d[i+1].r=(int16_t)(((int32_t)amp*qam16_table[qam16_table_offset_re])>>15);
        d[i+1].i=-(int16_t)(((int32_t)amp*qam16_table[qam16_table_offset_im])>>15);


        break;

      case 6:



        //UE1,-x1*FPGA_UE
        qam64_table_offset_re = 0;
        qam64_table_offset_im = 0;

        if (ulsch->b_tilde[j] == 1)
          qam64_table_offset_re+=4;

        if (ulsch->b_tilde[j+1] == 1)
          qam64_table_offset_im+=4;

        if (ulsch->b_tilde[j+2] == 1)
          qam64_table_offset_re+=2;


        if (ulsch->b_tilde[j+3] == 1)
          qam64_table_offset_im+=2;

        if (ulsch->b_tilde[j+4] == 1)
          qam64_table_offset_re+=1;

        if (ulsch->b_tilde[j+5] == 1)
          qam64_table_offset_im+=1;


        d[i].r=-(int16_t)(((int32_t)amp*qam64_table[qam64_table_offset_re])>>15);
        d[i].i=(int16_t)(((int32_t)amp*qam64_table[qam64_table_offset_im])>>15);

        //UE1,x0*
        qam64_table_offset_re = 0;
        qam64_table_offset_im = 0;

        if (ulsch->b_tilde[j-6] == 1)
          qam64_table_offset_re+=4;

        if (ulsch->b_tilde[j-5] == 1)
          qam64_table_offset_im+=4;

        if (ulsch->b_tilde[j-4] == 1)
          qam64_table_offset_re+=2;


        if (ulsch->b_tilde[j-3] == 1)
          qam64_table_offset_im+=2;

        if (ulsch->b_tilde[j-2] == 1)
          qam64_table_offset_re+=1;

        if (ulsch->b_tilde[j-1] == 1)
          qam64_table_offset_im+=1;


        d[i+1].r=(int16_t)(((int32_t)amp*qam64_table[qam64_table_offset_re])>>15);
        d[i+1].i=-(int16_t)(((int32_t)amp*qam64_table[qam64_table_offset_im])>>15);

        break;

      }//switch
    }//for
  }//cooperation_flag == 2
  else {
    for (i=0,j=0; i<ulsch_Msymb; i++,j+=Q_m) {

      switch (Q_m) {

      case 2:
        // TODO: this has to be updated!!!

        d[i].r = (ulsch->b_tilde[j] == 1)  ? (-gain_lin_QPSK) : gain_lin_QPSK;
        d[i].i = (ulsch->b_tilde[j+1] == 1)? (-gain_lin_QPSK) : gain_lin_QPSK;
        //        if (i<Msc_PUSCH)
        //    printf("input %d/%d Msc_PUSCH %d (%p): %d,%d\n", i,Msymb,Msc_PUSCH,&ulsch->d[i],((int16_t*)&ulsch->d[i])[0],((int16_t*)&ulsch->d[i])[1]);

        break;

      case 4:

        qam16_table_offset_re = 0;
        qam16_table_offset_im = 0;

        if (ulsch->b_tilde[j] == 1)
          qam16_table_offset_re+=2;

        if (ulsch->b_tilde[j+1] == 1)
          qam16_table_offset_im+=2;

        if (ulsch->b_tilde[j+2] == 1)
          qam16_table_offset_re+=1;

        if (ulsch->b_tilde[j+3] == 1)
          qam16_table_offset_im+=1;


        d[i].r=(int16_t)(((int32_t)amp*qam16_table[qam16_table_offset_re])>>15);
        d[i].i=(int16_t)(((int32_t)amp*qam16_table[qam16_table_offset_im])>>15);
        //      printf("input(16qam) %d (%p): %d,%d\n", i,&ulsch->d[i],((int16_t*)&ulsch->d[i])[0],((int16_t*)&ulsch->d[i])[1]);
        break;

      case 6:


        qam64_table_offset_re = 0;
        qam64_table_offset_im = 0;

        if (ulsch->b_tilde[j] == 1)
          qam64_table_offset_re+=4;

        if (ulsch->b_tilde[j+1] == 1)
          qam64_table_offset_im+=4;

        if (ulsch->b_tilde[j+2] == 1)
          qam64_table_offset_re+=2;

        if (ulsch->b_tilde[j+3] == 1)
          qam64_table_offset_im+=2;

        if (ulsch->b_tilde[j+4] == 1)
          qam64_table_offset_re+=1;

        if (ulsch->b_tilde[j+5] == 1)
          qam64_table_offset_im+=1;


        d[i].r=(int16_t)(((int32_t)amp*qam64_table[qam64_table_offset_re])>>15);
        d[i].i=(int16_t)(((int32_t)amp*qam64_table[qam64_table_offset_im])>>15);

        break;

      }
    }
  }// normal symbols


  // Transform Precoding

  dft_lte(ulsch->z,d,Msc_PUSCH,ulsch->Nsymb_pusch);

  DevAssert(txdataF);

  re_offset0 = frame_parms->first_carrier_offset + (ulsch->harq_processes[harq_pid]->first_rb*12);

  if (re_offset0>frame_parms->ofdm_symbol_size) {
    re_offset0 -= frame_parms->ofdm_symbol_size;
    //    re_offset0++;
  }

  //    printf("re_offset0 %d\n",re_offset0);
  //  printf("txdataF %p\n",&txdataF[0][0]);
  for (j=0,l=0; l<(nsymb-ulsch->srs_active); l++) {
    re_offset = re_offset0;
    symbol_offset = (uint32_t)frame_parms->ofdm_symbol_size*(l+(subframe*nsymb));
#ifdef DEBUG_ULSCH_MODULATION
    printf("ulsch_mod (SC-FDMA) symbol %d (subframe %u): symbol_offset %u\n",l,subframe,symbol_offset);
#endif
    txptr = &txdataF[0][symbol_offset];

    if (((frame_parms->Ncp == 0) && ((l==3) || (l==10)))||
        ((frame_parms->Ncp == 1) && ((l==2) || (l==8)))) {
    }
    // Skip reference symbols
    else {
      //      printf("copying %d REs\n",Msc_PUSCH);
      for (i=0; i<Msc_PUSCH; i++,j++) {

#ifdef DEBUG_ULSCH_MODULATION
        printf("re_offset %d (%p): %d,%d => %p\n", re_offset,&ulsch->z[j],((int16_t*)&ulsch->z[j])[0],((int16_t*)&ulsch->z[j])[1],&txptr[re_offset]);
#endif //DEBUG_ULSCH_MODULATION
        txptr[re_offset++] = ulsch->z[j];

        if (re_offset==frame_parms->ofdm_symbol_size)
          re_offset = 0;
      }
    }
  }

}

