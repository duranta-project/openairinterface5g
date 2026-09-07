/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*! \file PHY/CODING/nrLDPC_coding/nrLDPC_coding_segment/nrLDPC_coding_segment_encoder.c
 * \brief Top-level routines for implementing LDPC encoding of transport channels
 */

#include "nr_rate_matching.h"
#include "PHY/CODING/coding_extern.h"
#include "PHY/CODING/coding_defs.h"
#include "PHY/CODING/nrLDPC_coding/nrLDPC_coding_interface.h"
#include "PHY/CODING/nrLDPC_extern.h"
#include "common/utils/LOG/log.h"
#include "common/utils/nr/nr_common.h"
#include <openair2/UTIL/OPT/opt.h>

#include <syscall.h>

#define DEBUG_LDPC_ENCODING
//#define DEBUG_LDPC_ENCODING_FREE 1

/**
 * \brief write nrLDPC_coding_segment_encoder output with interleaver output
 * the interleaver output has 8 bits from 8 different segments per byte
 * nrLDPC_coding_segment_encoder is made of concatenated packed segments
 * \param f input interleaved segment, 8 bits from 8 segments per byte
 * \param E number of bits per segment in f
 * \param f2 input interleaved segment, 8 bits from 8 segments per byte
 * interleaved with E2 if there was a E shift in the segment group
 * \param E2 number of bits per segment in f2
 * \param E2_first_segment index of the first segment of size E2 if there is a E shift in the segment group
 * \param nb_segments number of segments in the group
 * \param output nrLDPC_coding_segment_encoder with concatenated segments and packed bits
 * \param Eoffset offset in number of bits of the first segment of the segment group within output
 */
static void write_task_output(uint8_t *f,
                              uint32_t E,
                              uint8_t *f2,
                              uint32_t E2,
                              uint32_t E2_first_segment,
                              uint32_t nb_segments,
                              uint8_t *output,
                              uint32_t Eoffset)
{

#if defined(__AVX512VBMI__)
  uint64_t *output_p = (uint64_t*)output;
  __m512i inc = _mm512_set1_epi8(0x1);

  for (int i=0;i<E2;i+=64) {
    uint32_t Eoffset2 = Eoffset;
    __m512i bitperm = _mm512_set1_epi64(0x3830282018100800);
    if (i<E) {
      for (int j=0; j < E2_first_segment; j++) {
        // Note: Here and below for AVX2, we are using the 64-bit SIMD instruction
        // instead of C >>/<< because when the Eoffset2_bit is 64 or 0, the <<
        // and >> operations are undefined and in fact don't give "0" which is
        // what we want here. The SIMD version do give 0 when the shift is 64
        uint32_t Eoffset2_byte = Eoffset2 >> 6;
        uint32_t Eoffset2_bit = Eoffset2 & 63;
        __m64 tmp = (__m64)_mm512_bitshuffle_epi64_mask(((__m512i *)f)[i >> 6],bitperm);
        *(__m64*)(output_p + Eoffset2_byte)   = _mm_or_si64(*(__m64*)(output_p + Eoffset2_byte),_mm_slli_si64(tmp,Eoffset2_bit));
        *(__m64*)(output_p + Eoffset2_byte+1) = _mm_or_si64(*(__m64*)(output_p + Eoffset2_byte+1),_mm_srli_si64(tmp,(64-Eoffset2_bit)));
        Eoffset2 += E;
        bitperm = _mm512_add_epi8(bitperm ,inc);
      }
    } else {
      for (int j=0; j < E2_first_segment; j++) {
        Eoffset2 += E;
        bitperm = _mm512_add_epi8(bitperm ,inc);
      }
    }
    for (int j=E2_first_segment; j < nb_segments; j++) {
      uint32_t Eoffset2_byte = Eoffset2 >> 6;
      uint32_t Eoffset2_bit = Eoffset2 & 63;
      __m64 tmp = (__m64)_mm512_bitshuffle_epi64_mask(((__m512i *)f2)[i >> 6],bitperm);
      *(__m64*)(output_p + Eoffset2_byte)   = _mm_or_si64(*(__m64*)(output_p + Eoffset2_byte),_mm_slli_si64(tmp,Eoffset2_bit));
      *(__m64*)(output_p + Eoffset2_byte+1) = _mm_or_si64(*(__m64*)(output_p + Eoffset2_byte+1),_mm_srli_si64(tmp,(64-Eoffset2_bit)));
      Eoffset2 += E2;
      bitperm = _mm512_add_epi8(bitperm ,inc);
    }
    output_p++;
  }

#elif defined(__aarch64__)
  uint16_t *output_p = (uint16_t*)output;
  const int8_t __attribute__ ((aligned (16))) ucShift[8][16] = {
    {0,1,2,3,4,5,6,7,0,1,2,3,4,5,6,7},     // segment 0
    {-1,0,1,2,3,4,5,6,-1,0,1,2,3,4,5,6},   // segment 1
    {-2,-1,0,1,2,3,4,5,-2,-1,0,1,2,3,4,5}, // segment 2
    {-3,-2,-1,0,1,2,3,4,-3,-2,-1,0,1,2,3,4}, // segment 3
    {-4,-3,-2,-1,0,1,2,3,-4,-3,-2,-1,0,1,2,3}, // segment 4
    {-5,-4,-3,-2,-1,0,1,2,-5,-4,-3,-2,-1,0,1,2}, // segment 5
    {-6,-5,-4,-3,-2,-1,0,1,-6,-5,-4,-3,-2,-1,0,1}, // segment 6
    {-7,-6,-5,-4,-3,-2,-1,0,-7,-6,-5,-4,-3,-2,-1,0}}; // segment 7
  const uint8_t __attribute__ ((aligned (16))) masks[16] = 
      {0x1,0x2,0x4,0x8,0x10,0x20,0x40,0x80,0x1,0x2,0x4,0x8,0x10,0x20,0x40,0x80};
  int8x16_t vshift[8];
  for (int n=0;n<8;n++) vshift[n] = vld1q_s8(ucShift[n]);
  uint8x16_t vmask  = vld1q_u8(masks);

  for (int i=0;i<E2;i+=16) {
    uint32_t Eoffset2 = Eoffset;
    if (i<E) {	
      for (int j=0; j < E2_first_segment; j++) {
        uint32_t Eoffset2_byte = Eoffset2 >> 4;
        uint32_t Eoffset2_bit = Eoffset2 & 15;
        uint8x16_t cshift = vandq_u8(vshlq_u8(((uint8x16_t*)f)[i >> 4],vshift[j]),vmask);
        int32_t tmp = (int)vaddv_u8(vget_low_u8(cshift));
        tmp += (int)(vaddv_u8(vget_high_u8(cshift))<<8);
        *(output_p + Eoffset2_byte)   |= (uint16_t)(tmp<<Eoffset2_bit);
        *(output_p + Eoffset2_byte+1) |= (uint16_t)(tmp>>(16-Eoffset2_bit));
        Eoffset2 += E;
      }
    } else {
      for (int j=0; j < E2_first_segment; j++) {
        Eoffset2 += E;
      }
    }
    for (int j=E2_first_segment; j < nb_segments; j++) {
      uint32_t Eoffset2_byte = Eoffset2 >> 4;
      uint32_t Eoffset2_bit = Eoffset2 & 15;
      uint8x16_t cshift = vandq_u8(vshlq_u8(((uint8x16_t*)f2)[i >> 4],vshift[j]),vmask);
      int32_t tmp = (int)vaddv_u8(vget_low_u8(cshift));
      tmp += (int)(vaddv_u8(vget_high_u8(cshift))<<8);
      *(output_p + Eoffset2_byte)   |= (uint16_t)(tmp<<Eoffset2_bit);
      *(output_p + Eoffset2_byte+1) |= (uint16_t)(tmp>>(16-Eoffset2_bit));
      Eoffset2 += E2;
    }
    output_p++;
  }
       
#else
  uint32_t *output_p = (uint32_t*)output;

  for (int i=0; i < E2; i += 32) {
    uint32_t Eoffset2 = Eoffset;
    if (i < E) {
      for (int j = 0; j < E2_first_segment; j++) {
        // Note: Here and below, we are using the 64-bit SIMD instruction
        // instead of C >>/<< because when the Eoffset2_bit is 64 or 0, the <<
        // and >> operations are undefined and in fact don't give "0" which is
        // what we want here. The SIMD version do give 0 when the shift is 64
        uint32_t Eoffset2_byte = Eoffset2 >> 5;
        uint32_t Eoffset2_bit = Eoffset2 & 31;
        int tmp = _mm256_movemask_epi8(_mm256_slli_epi16(((__m256i *)f)[i >> 5], 7 - j));
        __m64 tmp64 = _mm_set1_pi32(tmp);
        __m64 out64 = _mm_set_pi32(*(output_p + Eoffset2_byte + 1), *(output_p + Eoffset2_byte));
        __m64 tmp64b = _mm_or_si64(out64, _mm_slli_pi32(tmp64, Eoffset2_bit));
        __m64 tmp64c = _mm_or_si64(out64, _mm_srli_pi32(tmp64, (32 - Eoffset2_bit)));
        *(output_p + Eoffset2_byte) = _m_to_int(tmp64b);
        *(output_p + Eoffset2_byte + 1) = _m_to_int(_mm_srli_si64(tmp64c, 32));
        Eoffset2 += E;
      }
    } else {
      for (int j = 0; j < E2_first_segment; j++) {
        Eoffset2 += E;
      }
    } 
    for (int j = E2_first_segment; j < nb_segments; j++) {
      uint32_t Eoffset2_byte = Eoffset2 >> 5;
      uint32_t Eoffset2_bit = Eoffset2 & 31;
      int tmp = _mm256_movemask_epi8(_mm256_slli_epi16(((__m256i *)f2)[i >> 5], 7 - j));
      __m64 tmp64 = _mm_set1_pi32(tmp);
      __m64 out64 = _mm_set_pi32(*(output_p + Eoffset2_byte + 1), *(output_p + Eoffset2_byte));
      __m64 tmp64b = _mm_or_si64(out64, _mm_slli_pi32(tmp64, Eoffset2_bit));
      __m64 tmp64c = _mm_or_si64(out64, _mm_srli_pi32(tmp64, (32 - Eoffset2_bit)));
      *(output_p + Eoffset2_byte)  = _m_to_int(tmp64b);
      *(output_p + Eoffset2_byte + 1) = _m_to_int(_mm_srli_si64(tmp64c, 32));
      Eoffset2 += E2;
    }
    output_p++;
  } 
#endif
}

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
typedef struct ldpc8blocks_args_s {
  nrLDPC_TB_encoding_parameters_t *nrLDPC_TB_encoding_parameters;
  encoder_implemparams_t impp;
  uint8_t *f;
  uint8_t *f2;
} ldpc8blocks_args_t;

static void ldpc8blocks(void *p)
{
  ldpc8blocks_args_t *args = (ldpc8blocks_args_t *)p;
  nrLDPC_TB_encoding_parameters_t *nrLDPC_TB_encoding_parameters = args->nrLDPC_TB_encoding_parameters;
  encoder_implemparams_t *impp = &args->impp;

  uint8_t mod_order = nrLDPC_TB_encoding_parameters->Qm;
  uint16_t nb_rb = nrLDPC_TB_encoding_parameters->nb_rb;
  uint32_t A = nrLDPC_TB_encoding_parameters->A;

  unsigned int G = nrLDPC_TB_encoding_parameters->G;
  LOG_D(PHY, "dlsch coding A %d K %d G %d (nb_rb %d, mod_order %d)\n", A, impp->K, G, nb_rb, (int)mod_order);

  // nrLDPC_encoder output is in "d"
  // let's make this interface happy!
  uint8_t d[68 * 384] __attribute__((aligned(64)));
  uint8_t *c[nrLDPC_TB_encoding_parameters->C];
  unsigned int macro_segment, macro_segment_end;

  
  macro_segment = impp->first_seg;
  macro_segment_end = (impp->n_segments > impp->first_seg + 8) ? impp->first_seg + 8 : impp->n_segments;
  for (int r = 0; r < nrLDPC_TB_encoding_parameters->C; r++)
    c[r] = nrLDPC_TB_encoding_parameters->segments[r].c;
  start_meas(&nrLDPC_TB_encoding_parameters->segments[impp->first_seg].ts_ldpc_encode);
  LDPCencoder(c, d, impp);
  stop_meas(&nrLDPC_TB_encoding_parameters->segments[impp->first_seg].ts_ldpc_encode);
  // Compute where to place in output buffer that is concatenation of all segments

#ifdef DEBUG_LDPC_ENCODING
  LOG_D(PHY, "rvidx in encoding = %d\n", nrLDPC_TB_encoding_parameters->rv_index);
#endif
  const uint32_t E = nrLDPC_TB_encoding_parameters->segments[macro_segment].E;
  uint32_t E2=E;
  bool Eshift=false;
  uint32_t Emax = E;
  for (int s=macro_segment;s<macro_segment_end;s++)
      if (nrLDPC_TB_encoding_parameters->segments[s].E != E) {
	 E2=nrLDPC_TB_encoding_parameters->segments[s].E;
         Eshift=true;
         if(E2 > Emax)
           Emax = E2;
         break;
      }	 
    

  LOG_D(NR_PHY,
        "Rate Matching, Code segment %d...%d/%d (coded bits (G) %u, E %d, E2 %d Filler bits %d, Filler offset %d mod_order %d, nb_rb "
          "%d,nrOfLayer %d)...\n",
        macro_segment,
        macro_segment_end,
        impp->n_segments,
        G,
        E,E2,
        impp->F,
        impp->K - impp->F - 2 * impp->Zc,
        mod_order,
        nb_rb,
        nrLDPC_TB_encoding_parameters->nb_layers);
/*
  printf("Rate Matching, Code segment %d...%d/%d (coded bits (G) %u, E %d, E2 %d Filler bits %d, Filler offset %d mod_order %d, nb_rb "
          "%d,nrOfLayer %d)...\n",
        macro_segment,
        macro_segment_end,
        impp->n_segments,
        G,
        E,E2,
        impp->F,
        impp->K - impp->F - 2 * impp->Zc,
        mod_order,
        nb_rb,
        nrLDPC_TB_encoding_parameters->nb_layers);
*/
  uint32_t Tbslbrm = nrLDPC_TB_encoding_parameters->tbslbrm;

  uint8_t e[Emax] __attribute__((aligned(64)));
  // Interleaver outputs are stored in the output arrays
  uint8_t *f = args->f;
  uint8_t *f2 = args->f2;
  bzero(e, Emax);
  bzero(f, ceil_mod(E, 64));
  if (Eshift) {
    bzero(f2, ceil_mod(E2, 64));
  }
  start_meas(&nrLDPC_TB_encoding_parameters->segments[macro_segment].ts_rate_match);
  nr_rate_matching_ldpc(Tbslbrm,
                        impp->BG,
                        impp->Zc,
                        d,
                        e,
                        impp->n_segments,
                        impp->F,
                        impp->K - impp->F - 2 * impp->Zc,
                        nrLDPC_TB_encoding_parameters->rv_index,
                        Emax);

  stop_meas(&nrLDPC_TB_encoding_parameters->segments[macro_segment].ts_rate_match);
  if (impp->K - impp->F - 2 * impp->Zc > E) {
    LOG_E(PHY,
          "dlsch coding A %d  Kr %d G %d (nb_rb %d, mod_order %d)\n",
          A,
          impp->K,
          G,
          nb_rb,
          (int)mod_order);

    LOG_E(NR_PHY,
          "Rate Matching, Code segments %d...%d/%d (coded bits (G) %u, E %d, Kr %d, Filler bits %d, Filler offset %d mod_order %d, "
          "nb_rb %d)...\n",
          macro_segment,
	  macro_segment_end,
          impp->n_segments,
          G,
          E,
          impp->K,
          impp->F,
          impp->K - impp->F - 2 * impp->Zc,
          mod_order,
          nb_rb);
  }
  start_meas(&nrLDPC_TB_encoding_parameters->segments[macro_segment].ts_interleave);
  nr_interleaving_ldpc(E,
                       mod_order,
                       e,
                       f);
  if (Eshift)
    nr_interleaving_ldpc(E2,
                         mod_order,
                         e,
                         f2);
  stop_meas(&nrLDPC_TB_encoding_parameters->segments[macro_segment].ts_interleave);

  completed_task_ans(impp->ans);
}

static int nrLDPC_launch_TB_encoding(nrLDPC_slot_encoding_parameters_t *nrLDPC_slot_encoding_parameters,
                                      int dlsch_id,
                                      thread_info_tm_t *t_info,
                                      uint8_t **f,
                                      uint8_t **f2)
{
  nrLDPC_TB_encoding_parameters_t *nrLDPC_TB_encoding_parameters = &nrLDPC_slot_encoding_parameters->TBs[dlsch_id];

  encoder_implemparams_t common_segment_params = {
    .n_segments = nrLDPC_TB_encoding_parameters->C,
    .tinput = nrLDPC_slot_encoding_parameters->tinput,
    .tinput_memcpy = nrLDPC_slot_encoding_parameters->tinput_memcpy,
    .tprep = nrLDPC_slot_encoding_parameters->tprep,
    .tparity = nrLDPC_slot_encoding_parameters->tparity,
    .toutput = nrLDPC_slot_encoding_parameters->toutput,
    .tconcat = nrLDPC_slot_encoding_parameters->tconcat,
    .Kb = nrLDPC_TB_encoding_parameters->Kb,
    .Zc = nrLDPC_TB_encoding_parameters->Z,
    .BG = nrLDPC_TB_encoding_parameters->BG,
    .output = nrLDPC_TB_encoding_parameters->output, 
    .K = nrLDPC_TB_encoding_parameters->K,
    .F = nrLDPC_TB_encoding_parameters->F,
  };

  const size_t n_tasks = (common_segment_params.n_segments + 7) >> 3;

  for (int j = 0; j < n_tasks; j++) {
    ldpc8blocks_args_t *perJobImpp = &((ldpc8blocks_args_t *)t_info->buf)[t_info->len];
    DevAssert(t_info->len < t_info->cap);
    t_info->len += 1;

    perJobImpp->impp = common_segment_params;
    perJobImpp->impp.first_seg = j * 8;
    perJobImpp->impp.ans = t_info->ans;
    perJobImpp->nrLDPC_TB_encoding_parameters = nrLDPC_TB_encoding_parameters;
    perJobImpp->f = f[j];
    perJobImpp->f2 = f2[j];

    task_t t = {.func = ldpc8blocks, .args = perJobImpp};
    pushTpool(nrLDPC_slot_encoding_parameters->threadPool, t);
  }
  return n_tasks;
}

int nrLDPC_coding_encoder(nrLDPC_slot_encoding_parameters_t *nrLDPC_slot_encoding_parameters)
{
  int nbTasks = 0;

  uint32_t Emax = 0;
  for (int dlsch_id = 0; dlsch_id < nrLDPC_slot_encoding_parameters->nb_TBs; dlsch_id++) {
    // Compute number of tasks to encode TB
    nrLDPC_TB_encoding_parameters_t *nrLDPC_TB_encoding_parameters = &nrLDPC_slot_encoding_parameters->TBs[dlsch_id];
    size_t n_seg = (nrLDPC_TB_encoding_parameters->C / 8 + ((nrLDPC_TB_encoding_parameters->C & 7) == 0 ? 0 : 1));
    nbTasks += n_seg;

    // Search for maximum E for sizing encoder output f and f2
    for (int seg_id = 0; seg_id < nrLDPC_TB_encoding_parameters->C; seg_id++) {
      uint32_t E = nrLDPC_TB_encoding_parameters->segments[seg_id].E;
      Emax = E > Emax ? E : Emax;
    }
  }

  // Create f and f2 to old encoding tasks outputs
  uint32_t Emax_ceil_mod = ceil_mod(Emax, 64);
  uint8_t f[nbTasks][Emax_ceil_mod] __attribute__((aligned(64)));
  uint8_t f2[nbTasks][Emax_ceil_mod] __attribute__((aligned(64)));

  ldpc8blocks_args_t arr[nbTasks];
  task_ans_t ans;
  init_task_ans(&ans, nbTasks);
  thread_info_tm_t t_info = {.buf = (uint8_t *)arr, .len = 0, .cap = nbTasks, .ans = &ans};

  int nbEncode = 0;
  nbTasks = 0;
  for (int dlsch_id = 0; dlsch_id < nrLDPC_slot_encoding_parameters->nb_TBs; dlsch_id++) {
    // For easier indexing we store the pointers to sub arrays of f and f2 in pointer arrays
    // Then a function to which we pass the pointer arrays can directly use f_2d[j] ans f2_2d[j]
    nrLDPC_TB_encoding_parameters_t *nrLDPC_TB_encoding_parameters = &nrLDPC_slot_encoding_parameters->TBs[dlsch_id];
    size_t n_seg = (nrLDPC_TB_encoding_parameters->C / 8 + ((nrLDPC_TB_encoding_parameters->C & 7) == 0 ? 0 : 1));
    uint8_t *f_2d[n_seg];
    uint8_t *f2_2d[n_seg];
    for (int j = 0; j < n_seg; j++) {
      f_2d[j] = &f[nbTasks + j][0];
      f2_2d[j] = &f2[nbTasks + j][0];
    }

    // Launch tasks encoding TB
    nbEncode += nrLDPC_launch_TB_encoding(nrLDPC_slot_encoding_parameters, dlsch_id, &t_info, f_2d, f2_2d);

    nbTasks += n_seg;
  }
  if (nbEncode < nbTasks) {
    completed_many_task_ans(&ans, nbTasks - nbEncode);
  }
  // Execute thread pool tasks
  join_task_ans(&ans);

  // Write output
  time_stats_t *tconcat = nrLDPC_slot_encoding_parameters->tconcat;
  if (tconcat != NULL)
    start_meas(tconcat);
  nbTasks = 0;
  for (int dlsch_id = 0; dlsch_id < nrLDPC_slot_encoding_parameters->nb_TBs; dlsch_id++) {
    nrLDPC_TB_encoding_parameters_t *nrLDPC_TB_encoding_parameters = &nrLDPC_slot_encoding_parameters->TBs[dlsch_id];
    uint32_t C = nrLDPC_TB_encoding_parameters->C;
    size_t n_seg = (C / 8 + ((C & 7) == 0 ? 0 : 1));

    for (int j = 0; j < n_seg; j++) {
      unsigned int macro_segment = j * 8;
      unsigned int macro_segment_end = (C > macro_segment + 8) ? macro_segment + 8 : C;
      const uint32_t E = nrLDPC_TB_encoding_parameters->segments[macro_segment].E;
      uint32_t E2 = E, E2_first_segment = macro_segment_end - macro_segment;
      uint32_t Emax = E;
      for (int s = macro_segment; s < macro_segment_end; s++) {
        if (nrLDPC_TB_encoding_parameters->segments[s].E != E) {
           E2 = nrLDPC_TB_encoding_parameters->segments[s].E;
           E2_first_segment = s-macro_segment;
           if(E2 > Emax)
             Emax = E2;
           break;
        }
      }

 
      uint32_t Eoffset=0;
      for (int s=0; s<macro_segment; s++)
        Eoffset += (nrLDPC_TB_encoding_parameters->segments[s].E); 

      write_task_output(&f[nbTasks + j][0],
                        E,
                        &f2[nbTasks + j][0],
                        E2,
                        E2_first_segment,
                        macro_segment_end - macro_segment,
                        nrLDPC_TB_encoding_parameters->output,
                        Eoffset);
    }
    nbTasks += n_seg;
  }
  if (tconcat != NULL)
    stop_meas(tconcat);

  return 0;
}
