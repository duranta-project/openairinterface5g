/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Top-level routines for implementing LDPC encoding of transport channels
 */

#include "openair1/PHY/CODING/nrLDPC_coding/nrLDPC_coding_segment/nr_rate_matching.h"
#include "PHY/sse_intrin.h"
#include "openair1/PHY/CODING/nrLDPC_defs.h"
#include "openair1/PHY/CODING/nrLDPC_coding/nrLDPC_coding_interface.h"
#include "common/utils/LOG/log.h"
#include "nrLDPC_cuda_unpack_output.h"

#include <syscall.h>

//#define DEBUG_LDPC_ENCODING
//#define DEBUG_LDPC_ENCODING_FREE 1

static const uint8_t index_k0[2][4] = {{0, 17, 33, 56}, {0, 13, 25, 43}};

extern uint32_t **d_host;

static int nr_rate_matching_ldpc32(uint32_t Tbslbrm,
                                   uint8_t BG,
                                   uint16_t Z,
                                   uint32_t *d,
                                   uint32_t *e,
                                   uint8_t C,
                                   uint32_t F,
                                   uint32_t Foffset,
                                   uint8_t rvidx,
                                   uint32_t E)
{
  if (C == 0) {
    LOG_E(PHY, "nr_rate_matching: invalid parameter C %d\n", C);
    return -1;
  }

  // Bit selection
  uint32_t N = (BG == 1) ? (66 * Z) : (50 * Z);
  uint32_t Ncb;
  if (Tbslbrm == 0)
    Ncb = N;
  else {
    uint32_t Nref = 3 * Tbslbrm / (2 * C); // R_LBRM = 2/3
    Ncb = min(N, Nref);
  }

  uint32_t ind = (index_k0[BG - 1][rvidx] * Ncb / N) * Z;

#ifdef RM_DEBUG
  printf("nr_rate_matching_ldpc: E %u, F %u, Foffset %u, k0 %u, Ncb %u, rvidx %d, Tbslbrm %u\n",
         E,
         F,
         Foffset,
         ind,
         Ncb,
         rvidx,
         Tbslbrm);
#endif

  if (Foffset > E) {
    LOG_E(PHY,
          "nr_rate_matching: invalid parameters (Foffset %d > E %d) F %d, k0 %d, Ncb %d, rvidx %d, Tbslbrm %d\n",
          Foffset,
          E,
          F,
          ind,
          Ncb,
          rvidx,
          Tbslbrm);
    return -1;
  }
  if (Foffset > Ncb) {
    LOG_E(PHY, "nr_rate_matching: invalid parameters (Foffset %d > Ncb %d)\n", Foffset, Ncb);
    return -1;
  }

  if (ind >= Foffset && ind < (F + Foffset))
    ind = F + Foffset;

  uint32_t k = 0;
  if (ind < Foffset) { // case where we have some bits before the filler and the rest after
    memcpy((void *)e, (void *)(d + ind), (Foffset - ind) << 2);

    if (E + F <= Ncb - ind) { // E+F doesn't contain all coded bits
      memcpy((void *)(e + Foffset - ind), (void *)(d + Foffset + F), (E - Foffset + ind) << 2);
      k = E;
    } else {
      memcpy((void *)(e + Foffset - ind), (void *)(d + Foffset + F), (Ncb - Foffset - F) << 2);
      k = Ncb - F - ind;
    }
  } else {
    if (E <= Ncb - ind) { // E+F doesn't contain all coded bits
      memcpy((void *)(e), (void *)(d + ind), E << 2);
      k = E;
    } else {
      memcpy((void *)(e), (void *)(d + ind), (Ncb - ind) << 2);
      k = Ncb - ind;
    }
  }

  while (k < E) { // case where we do repetitions (low mcs)
    for (ind = 0; (ind < Ncb) && (k < E); ind++) {
#ifdef RM_DEBUG
      printf("RM_TX k%u Ind: %u (%d)\n", k, ind, d[ind]);
#endif

      if (ind == Foffset)
        ind = F + Foffset; // skip filler bits

      e[k++] = d[ind];
    }
  }

  return 0;
}

static void nr_interleaving_ldpc32(uint32_t E, uint8_t Qm, uint32_t *e, uint32_t *f)
{
  const uint32_t EQm = E / Qm;
  memset(f, 0, E * sizeof(uint32_t));
  switch (Qm) {
    case 2: {
      uint32_t *e0 = e;
      uint32_t *e1 = e + EQm;
      int i = 0;
      for (; i < EQm; i++) {
        *f++ = *e0++;
        *f++ = *e1++;
      }
    } break;
    case 4: {
      uint32_t *e0 = e;
      uint32_t *e1 = e0 + EQm;
      uint32_t *e2 = e1 + EQm;
      uint32_t *e3 = e2 + EQm;
      int i = 0;
      for (; i < EQm; i++) {
        *f++ = *e0++;
        *f++ = *e1++;
        *f++ = *e2++;
        *f++ = *e3++;
      }
    } break;
    case 6: {
      uint32_t *e0 = e;
      uint32_t *e1 = e0 + EQm;
      uint32_t *e2 = e1 + EQm;
      uint32_t *e3 = e2 + EQm;
      uint32_t *e4 = e3 + EQm;
      uint32_t *e5 = e4 + EQm;
      int i = 0;
#if 0
      simde__m128i *e0_128 = (simde__m128i *)e0;
      simde__m128i *e1_128 = (simde__m128i *)e1;
      simde__m128i *e2_128 = (simde__m128i *)e2;
      simde__m128i *e3_128 = (simde__m128i *)e3;
      simde__m128i *e4_128 = (simde__m128i *)e4;
      simde__m128i *e5_128 = (simde__m128i *)e5;
      simde__m128i *f128 = (simde__m128i *)f;
      for (; i < (EQm & ~3); i += 4) {
        simde__m128i e0j = simde_mm_loadu_si128(e0_128++);
        simde__m128i e1j = simde_mm_loadu_si128(e1_128++);
        simde__m128i e2j = simde_mm_loadu_si128(e2_128++);
        simde__m128i e3j = simde_mm_loadu_si128(e3_128++);
        simde__m128i e4j = simde_mm_loadu_si128(e4_128++);
        simde__m128i e5j = simde_mm_loadu_si128(e5_128++);

        simde__m128i tmp0 = simde_mm_unpacklo_epi32(e0j, e1j);   // e0(i) e1(i) e0(i+1) e1(i+1) 
        simde__m128i tmp1 = simde_mm_unpacklo_epi32(e2j, e3j);   // e2(i) e3(i) e2(i+1) e3(i+1) 
        simde__m128i tmp2 = simde_mm_unpacklo_epi32(e4j, e5j);   // e4(i) e5(i) e4(i+1) e5(i+1) 
								 
        simde_mm_storeu_si128(f128++,simde_mm_unpacklo_epi64(tmp0, tmp1));                        // e0(i) e1(i) e2(i) e3(i) 
        simde_mm_storeu_si128(f128++,simde_mm_unpacklo_epi64(tmp2,simde_mm_unpackhi_epi64(tmp0,tmp0))); // e4(i) e5(i) e0(i+1) e1(i+1) 
	simde_mm_storeu_si128(f128++,simde_mm_unpackhi_epi64(tmp1,tmp2));                         // e2(i+1) e3(i+1) e4(i+1) e5(i+1)
        tmp0 = simde_mm_unpackhi_epi32(e0j, e1j);   // e0(i+2) e1(i+2) e0(i+3) e1(i+3) 
        tmp1 = simde_mm_unpackhi_epi32(e2j, e3j);   // e2(i+2) e3(i+2) e2(i+3) e3(i+3) 
        tmp2 = simde_mm_unpackhi_epi32(e4j, e5j);   // e4(i+2) e5(i+2) e4(i+3) e5(i+3) 
								 
        simde_mm_storeu_si128(f128++,simde_mm_unpacklo_epi64(tmp0, tmp1));                        // e0(i+2) e1(i+2) e2(i+2) e3(i+2) 
        simde_mm_storeu_si128(f128++,simde_mm_unpacklo_epi64(tmp2,simde_mm_unpackhi_epi64(tmp0,tmp0))); // e4(i+2) e5(i+2) e0(i+3) e1(i+3) 
	simde_mm_storeu_si128(f128++,simde_mm_unpackhi_epi64(tmp1,tmp2));                         // e2(i+3) e3(i+3) e4(i+3) e5(i+3)
      }
      e0 = (uint32_t *)e0_128;
      e1 = (uint32_t *)e1_128;
      e2 = (uint32_t *)e2_128;
      e3 = (uint32_t *)e3_128;
      e4 = (uint32_t *)e4_128;
      e5 = (uint32_t *)e5_128;
      f  = (uint32_t *)f128;
#endif
      for (; i < EQm; i++) {
        *f++ = *e0++;
        *f++ = *e1++;
        *f++ = *e2++;
        *f++ = *e3++;
        *f++ = *e4++;
        *f++ = *e5++;
      }
    } break;
    case 8: {
      uint32_t *e0 = e;
      uint32_t *e1 = e0 + EQm;
      uint32_t *e2 = e1 + EQm;
      uint32_t *e3 = e2 + EQm;
      uint32_t *e4 = e3 + EQm;
      uint32_t *e5 = e4 + EQm;
      uint32_t *e6 = e5 + EQm;
      uint32_t *e7 = e6 + EQm;

      int i = 0;

      simde__m128i *e0_128 = (simde__m128i *)e0;
      simde__m128i *e1_128 = (simde__m128i *)e1;
      simde__m128i *e2_128 = (simde__m128i *)e2;
      simde__m128i *e3_128 = (simde__m128i *)e3;
      simde__m128i *e4_128 = (simde__m128i *)e4;
      simde__m128i *e5_128 = (simde__m128i *)e5;
      simde__m128i *e6_128 = (simde__m128i *)e6;
      simde__m128i *e7_128 = (simde__m128i *)e7;
      simde__m128i *f128 = (simde__m128i *)f;

      for (; i < (EQm & ~3); i += 4) {
        simde__m128i e0j = simde_mm_loadu_si128(e0_128++);
        simde__m128i e1j = simde_mm_loadu_si128(e1_128++);
        simde__m128i e2j = simde_mm_loadu_si128(e2_128++);
        simde__m128i e3j = simde_mm_loadu_si128(e3_128++);
        simde__m128i e4j = simde_mm_loadu_si128(e4_128++);
        simde__m128i e5j = simde_mm_loadu_si128(e5_128++);
        simde__m128i e6j = simde_mm_loadu_si128(e6_128++);
        simde__m128i e7j = simde_mm_loadu_si128(e7_128++);

        simde__m128i tmp0 = simde_mm_unpacklo_epi32(e0j, e1j); // e0(i) e1(i) e0(i+1) e1(i+1)
        simde__m128i tmp1 = simde_mm_unpacklo_epi32(e2j, e3j); // e2(i) e3(i) e2(i+1) e3(i+1)
        simde__m128i tmp2 = simde_mm_unpacklo_epi32(e4j, e5j); // e4(i) e5(i) e4(i+1) e5(i+1)
        simde__m128i tmp3 = simde_mm_unpacklo_epi32(e6j, e7j); // e6(i) e7(i) e6(i+1) e7(i+1)

        simde_mm_storeu_si128(f128++, simde_mm_unpacklo_epi64(tmp0, tmp1)); // e0(i) e1(i) e2(i) e3(i)
        simde_mm_storeu_si128(f128++, simde_mm_unpacklo_epi64(tmp2, tmp3)); // e4(i) e5(i) e6(i) e7(i)
        simde_mm_storeu_si128(f128++, simde_mm_unpackhi_epi64(tmp0, tmp1)); // e0(i+1) e1(i+1) e2(i+1) e3(i+1)
        simde_mm_storeu_si128(f128++, simde_mm_unpackhi_epi64(tmp2, tmp3)); // e4(i+1) e5(i+1) e6(i+1) e7(i+1)

        tmp0 = simde_mm_unpackhi_epi32(e0j, e1j); // e0(i+2) e1(i+2) e0(i+3) e1(i+3)
        tmp1 = simde_mm_unpackhi_epi32(e2j, e3j); // e2(i+2) e3(i+2) e2(i+3) e3(i+3)
        tmp2 = simde_mm_unpackhi_epi32(e4j, e5j); // e4(i+2) e5(i+2) e4(i+3) e5(i+3)
        tmp3 = simde_mm_unpackhi_epi32(e6j, e7j); // e6(i+2) e7(i+2) e6(i+3) e7(i+3)

        simde_mm_storeu_si128(f128++, simde_mm_unpacklo_epi64(tmp0, tmp1)); // e0(i+2) e1(i+2) e2(i+2) e3(i+2)
        simde_mm_storeu_si128(f128++, simde_mm_unpacklo_epi64(tmp2, tmp3)); // e4(i+2) e5(i+2) e6(i+2) e7(i+2)
        simde_mm_storeu_si128(f128++, simde_mm_unpackhi_epi64(tmp0, tmp1)); // e0(i+3) e1(i+3) e2(i+3) e3(i+3)
        simde_mm_storeu_si128(f128++, simde_mm_unpackhi_epi64(tmp2, tmp3)); // e4(i+3) e5(i+3) e6(i+3) e7(i+3)
      }
      e0 = (uint32_t *)e0_128;
      e1 = (uint32_t *)e1_128;
      e2 = (uint32_t *)e2_128;
      e3 = (uint32_t *)e3_128;
      e4 = (uint32_t *)e4_128;
      e5 = (uint32_t *)e5_128;
      e6 = (uint32_t *)e6_128;
      e7 = (uint32_t *)e7_128;
      f = (uint32_t *)f128;

      for (; i < EQm; i++) {
        *f++ = *e0++;
        *f++ = *e1++;
        *f++ = *e2++;
        *f++ = *e3++;
        *f++ = *e4++;
        *f++ = *e5++;
        *f++ = *e6++;
        *f++ = *e7++;
      }
    } break;
    default:
      AssertFatal(false, "Should be here!\n");
  }
}

extern uint32_t **LDPCencoder32(uint8_t **input, encoder_implemparams_t *impp);

/**
 * \typedef ldpc8blocks_args_t
 * \struct ldpc8blocks_args_s
 * \brief Arguments of an encoding task
 * encode up to 8 code blocks
 * \var nrLDPC_TB_encoding_parameters TB encoding parameters as defined in the coding library interface
 * \var impp encoder implementation specific parameters for the task
 * \var f first interleaver output to be filled by the task
 * \var f2 second interleaver output to be filled by the task
 * in case of a shift of E in the code blocks group processed by the task
 */

static void ldpcnblocks(nrLDPC_TB_encoding_parameters_t *nrLDPC_TB_encoding_parameters, encoder_implemparams_t impp)
{
  uint8_t mod_order = nrLDPC_TB_encoding_parameters->Qm;
  uint16_t nb_rb = nrLDPC_TB_encoding_parameters->nb_rb;
  uint32_t A = nrLDPC_TB_encoding_parameters->A;

  unsigned int G = nrLDPC_TB_encoding_parameters->G;
  LOG_D(PHY, "dlsch coding A %d K %d G %d (nb_rb %d, mod_order %d)\n", A, impp.K, G, nb_rb, (int)mod_order);

  // nrLDPC_encoder output is in "d"
  // let's make this interface happy!
  //  uint32_t d[4][68*384];
  // uint8_t *c[nrLDPC_TB_encoding_parameters->C];
  extern uint32_t **input_host;

  if (!nrLDPC_TB_encoding_parameters->c_dev)
    for (int r = 0; r < nrLDPC_TB_encoding_parameters->C; r++) {
      input_host[r] = (uint32_t *)nrLDPC_TB_encoding_parameters->segments[r].c;
    }
  start_meas(&nrLDPC_TB_encoding_parameters->segments[impp.first_seg].ts_ldpc_encode);
  LDPCencoder32(nrLDPC_TB_encoding_parameters->c_dev ? nrLDPC_TB_encoding_parameters->c_dev : (uint8_t **)input_host, &impp);
  stop_meas(&nrLDPC_TB_encoding_parameters->segments[impp.first_seg].ts_ldpc_encode);
  // Compute where to place in output buffer that is concatenation of all segments

#ifdef DEBUG_LDPC_ENCODING
  LOG_D(PHY, "rvidx in encoding = %d\n", nrLDPC_TB_encoding_parameters->rv_index);
#endif
  const uint32_t E = nrLDPC_TB_encoding_parameters->segments[0].E;
  uint32_t E2 = E;
  uint32_t Emax = E;
  int n_seg = nrLDPC_TB_encoding_parameters->C >> 5;
  int n_seg2 = n_seg;
  if ((nrLDPC_TB_encoding_parameters->C & 31) > 0)
    n_seg2++;
  int r_shift = n_seg2;
  int r_shift2 = nrLDPC_TB_encoding_parameters->C;
  for (int s = 0; s < nrLDPC_TB_encoding_parameters->C; s++) {
    // printf("segment %d E %d\n",s,nrLDPC_TB_encoding_parameters->segments[s].E);
    if (nrLDPC_TB_encoding_parameters->segments[s].E != E) {
      E2 = nrLDPC_TB_encoding_parameters->segments[s].E;
      if (E2 > Emax)
        Emax = E2;
      r_shift = s >> 5;
      r_shift2 = s;
      // printf("r_shift %d, r_shift2 %d\n",r_shift,r_shift2);
      break;
    }
  }

  LOG_D(NR_PHY,
        "Rate Matching, Code segment %d...%d r_shift %d n_seg2 %d (coded bits (G) %u, E %d, E2 %d Filler bits %d, Filler offset %d "
        "mod_order %d, nb_rb "
        "%d,nrOfLayer %d)...\n",
        0,
        impp.n_segments - 1,
        r_shift,
        n_seg2,
        G,
        E,
        E2,
        impp.F,
        impp.K - impp.F - 2 * impp.Zc,
        mod_order,
        nb_rb,
        nrLDPC_TB_encoding_parameters->nb_layers);
  /*
    printf("Rate Matching, Code segment 0..%d r_shift %d r_shift2 %d n_seg2 %d (coded bits (G) %u, E %d, E2 %d Filler bits %d,
    Filler offset %d mod_order %d, nb_rb "
            "%d,nrOfLayer %d)...\n",
          impp.n_segments-1,
    r_shift,
    r_shift2,
    n_seg2,
          G,
          E,E2,
          impp.F,
          impp.K - impp.F - 2 * impp.Zc,
          mod_order,
          nb_rb,
          nrLDPC_TB_encoding_parameters->nb_layers);
  */

  uint32_t Tbslbrm = nrLDPC_TB_encoding_parameters->tbslbrm;

  uint32_t e[E * (r_shift + 1)];
  size_t siz = max(1, n_seg2 - r_shift);
  uint32_t e2[E2 * siz];
  uint32_t f[E * (r_shift + 1)] __attribute__((aligned(64)));
  uint32_t f2[E2 * siz] __attribute__((aligned(64)));

  // Interleaver outputs are stored in the output arrays
  uint8_t *output = nrLDPC_TB_encoding_parameters->output;

  memset(e, 0, sizeof(e));
  memset(f, 0, sizeof(f));
  if (1 /*r_shift < n_seg2*/) {
    memset(e2, 0, sizeof(e2));
    memset(f2, 0, sizeof(f2));
  }

  for (int r = 0; r < n_seg2; r++) {
    if (r <= r_shift)
      nr_rate_matching_ldpc32(Tbslbrm,
                              impp.BG,
                              impp.Zc,
                              d_host[r],
                              e + (r * E),
                              impp.n_segments,
                              impp.F,
                              impp.K - impp.F - 2 * impp.Zc,
                              nrLDPC_TB_encoding_parameters->rv_index,
                              E);
    if (r >= r_shift)
      nr_rate_matching_ldpc32(Tbslbrm,
                              impp.BG,
                              impp.Zc,
                              d_host[r],
                              e2 + ((r - r_shift) * E2),
                              impp.n_segments,
                              impp.F,
                              impp.K - impp.F - 2 * impp.Zc,
                              nrLDPC_TB_encoding_parameters->rv_index,
                              E2);
    /*
     if (r==(n_seg2-1)) {
       for (int i=0;i<16;i++) printf("rm: %x %x\n",d[n_seg2-1][i],e2[((n_seg2-1)*E2)+i]);
     }
     */
  }
  if (impp.K - impp.F - 2 * impp.Zc > E) {
    LOG_E(PHY, "dlsch coding A %d  Kr %d G %d (nb_rb %d, mod_order %d)\n", A, impp.K, G, nb_rb, (int)mod_order);

    LOG_E(NR_PHY,
          "Rate Matching, Code segments 0..%d (coded bits (G) %u, E %d, Kr %d, Filler bits %d, Filler offset %d mod_order %d, "
          "nb_rb %d)...\n",
          impp.n_segments,
          G,
          E,
          impp.K,
          impp.F,
          impp.K - impp.F - 2 * impp.Zc,
          mod_order,
          nb_rb);
  }

  // printf("interleaving r_shift %d, n_seg2 %d\n",r_shift,n_seg2);

  for (int r = 0; r <= r_shift; r++)
    nr_interleaving_ldpc32(E, mod_order, e + E * r, f + E * r);

  for (int r = r_shift; r < n_seg2; r++)
    nr_interleaving_ldpc32(E2, mod_order, e2 + E2 * (r - r_shift), f2 + E2 * (r - r_shift));
  /*
    for (int i=0;i<16;i++) printf("intl (f offset %d): %x %x\n",(n_seg2-1)*E2,e2[((n_seg2-1)*E2)+i],f2[((n_seg2-1)*E2)+i]);
    printf("-------------------\n");
    for (int i=E2-16;i<E2;i++) printf("intl (f offset %d): %x %x\n",(n_seg2-1)*E2,e2[((n_seg2-1)*E2)+i],f2[((n_seg2-1)*E2)+i]);
    */

  nr_ldpc_cuda_unpack_output(f, E, f2, E2, r_shift, r_shift2, nrLDPC_TB_encoding_parameters->C, output);
}

int nrLDPC_coding_encoder32(nrLDPC_slot_encoding_parameters_t *nrLDPC_slot_encoding_parameters,
                            nrLDPC_TB_encoding_parameters_t *nrLDPC_TB_encoding_parameters)
{
  encoder_implemparams_t common_segment_params = {
      .n_segments = nrLDPC_TB_encoding_parameters->C,
      .Kb = nrLDPC_TB_encoding_parameters->Kb,
      .Zc = nrLDPC_TB_encoding_parameters->Z,
      .BG = nrLDPC_TB_encoding_parameters->BG,
      .output = nrLDPC_TB_encoding_parameters->output,
      .K = nrLDPC_TB_encoding_parameters->K,
      .F = nrLDPC_TB_encoding_parameters->F,
  };

  ldpcnblocks(nrLDPC_TB_encoding_parameters, common_segment_params);

  return 0;
}
