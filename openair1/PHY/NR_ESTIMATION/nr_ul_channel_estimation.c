/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_common.h"
#include <string.h>

#include "nr_ul_estimation.h"
#include "PHY/sse_intrin.h"
#include "PHY/NR_REFSIG/nr_refsig.h"
#include "PHY/NR_REFSIG/dmrs_nr.h"
#include "PHY/NR_REFSIG/ptrs_nr.h"
#include "PHY/NR_TRANSPORT/nr_transport_proto.h"
#include "PHY/NR_UE_ESTIMATION/filt16a_32.h"
#include "PHY/NR_TRANSPORT/nr_sch_dmrs.h"
#include "PHY/NR_REFSIG/ul_ref_seq_nr.h"
#include "executables/softmodem-common.h"
#include "nr_phy_common.h"
#include "openair1/PHY/TOOLS/phy_scope_interface.h"
#include "T.h"

//#define DEBUG_CH
//#define DEBUG_PUSCH
//#define SRS_DEBUG

#define NO_INTERP 1
#define dBc(x, y) (dB_fixed(((int32_t)(x)) * (x) + ((int32_t)(y)) * (y)))

typedef struct puschAntennaProc_s {
  unsigned char Ns;
  int nl;
  unsigned short p;
  unsigned char symbol;
  unsigned short bwp_start_subcarrier;
  int aarx;
  int numAntennas;
  const nfapi_nr_pusch_pdu_t *pusch_pdu;
  int *max_ch;
  c16_t *pilot;
  int *nest_count;
  uint64_t *noise_amp2;
  delay_t *delay;
  int chest_freq;
  NR_gNB_PUSCH *pusch_vars;
  NR_DL_FRAME_PARMS *frame_parms;
  c16_t **rxdataF;
  task_ans_t *ans;
  scopeData_t *scope;
  c16_t *pusch_ch_est_dmrs_pos_slot_mem;
  int dmrs_symbol_start_idx;
} puschAntennaProc_t;

__attribute__((always_inline)) inline c16_t c32x16cumulVectVectWithSteps(c16_t *in1,
                                                                         int *offset1,
                                                                         const int step1,
                                                                         c16_t *in2,
                                                                         int *offset2,
                                                                         const int step2,
                                                                         const int modulo2,
                                                                         const int N)
{
  int localOffset1 = *offset1;
  int localOffset2 = *offset2;
  c32_t cumul = {0};
  for (int i = 0; i < N; i++) {
    cumul = c32x16maddShift(in1[localOffset1], in2[localOffset2], cumul, 15);
    localOffset1 += step1;
    localOffset2 = (localOffset2 + step2) % modulo2;
  }
  *offset1 = localOffset1;
  *offset2 = localOffset2;
  return c16x32div(cumul, N);
}

static void nr_pusch_antenna_processing(void *arg)
{
  puschAntennaProc_t *rdata = (puschAntennaProc_t *)arg;
  unsigned char Ns = rdata->Ns;
  int nl = rdata->nl;
  unsigned short p = rdata->p;
  unsigned char symbol = rdata->symbol;
  int aarx = rdata->aarx;
  int numAntennas = rdata->numAntennas;
  unsigned short bwp_start_subcarrier = rdata->bwp_start_subcarrier;
  const nfapi_nr_pusch_pdu_t *pusch_pdu = rdata->pusch_pdu;
  int *max_ch = rdata->max_ch;
  c16_t *pilot = rdata->pilot;
  uint64_t noise_amp2 = *(rdata->noise_amp2);
  int nest_count = *(rdata->nest_count);
  delay_t *delay = rdata->delay;

  const int chest_freq = rdata->chest_freq;
  NR_gNB_PUSCH *pusch_vars = rdata->pusch_vars;
  c16_t **ul_ch_estimates = (c16_t **)pusch_vars->ul_ch_estimates;
  NR_DL_FRAME_PARMS *frame_parms = rdata->frame_parms;
  const int symbolSize = frame_parms->ofdm_symbol_size;
  const int delta = get_delta(p, pusch_pdu->dmrs_config_type);
  const int symbol_offset = symbolSize * symbol;
  const int k0 = bwp_start_subcarrier;
  const int nb_rb_pusch = pusch_pdu->rb_size;
  const uint8_t num_sp_streams = rdata->pusch_pdu->param_v4.numSpatialStreamIndices;
  for (int antenna = aarx; antenna < aarx + numAntennas; antenna++) {
    c16_t ul_ls_est[symbolSize] __attribute__((aligned(32)));
    memset(ul_ls_est, 0, sizeof(c16_t) * symbolSize);
    c16_t *rxdataF = (c16_t *)&rdata->rxdataF[antenna][symbol_offset];
    c16_t *ul_ch = &ul_ch_estimates[nl * num_sp_streams + antenna][symbol_offset];
    memset(ul_ch, 0, sizeof(*ul_ch) * symbolSize);

    LOG_D(PHY, "symbol_offset %d, OFDM size %d, Ns = %d, k0 = %d, symbol %d\n", symbol_offset, symbolSize, Ns, k0, symbol);

#ifdef DEBUG_PUSCH
    LOG_I(PHY, "symbol_offset %d, delta %d\n", symbol_offset, delta);
    LOG_I(PHY, "ch est pilot, N_RB_UL %d\n", frame_parms->N_RB_UL);
    LOG_I(PHY,
          "bwp_start_subcarrier %d, k0 %d, first_carrier %d, nb_rb_pusch %d\n",
          bwp_start_subcarrier,
          k0,
          frame_parms->first_carrier_offset,
          nb_rb_pusch);
    LOG_I(PHY, "ul_ch addr %p \n", ul_ch);
#endif

    if (pusch_pdu->dmrs_config_type == pusch_dmrs_type1 && chest_freq == 0) {
      c16_t *pil = pilot;
      int re_offset = k0;
      LOG_D(PHY, "PUSCH estimation DMRS type 1, Freq-domain interpolation");
      int pilot_cnt = 0;
#if T_TRACER
      int ch_est_cnt = 0; // To trace channel coefficients
#endif

      for (int n = 0; n < 3 * nb_rb_pusch; n++) {
        // LS estimation
        c32_t ch = {0};

        for (int k_line = 0; k_line <= 1; k_line++) {
          re_offset = (k0 + (n << 2) + (k_line << 1) + delta) % symbolSize;
          ch = c32x16maddShift(*pil, rxdataF[re_offset], ch, 16);
          pil++;
        }

        c16_t ch16 = {.r = (int16_t)ch.r, .i = (int16_t)ch.i};
        *max_ch = max(abs(ch.r), abs(ch.i));
        for (int k = pilot_cnt << 1; k < (pilot_cnt << 1) + 4; k++) {
          ul_ls_est[k] = ch16;
        }
//------------------Write channel parameters to Memory  for data recording ------------------//
#if T_TRACER
        if (T_ACTIVE(T_GNB_PHY_UL_FD_CHAN_EST_DMRS_POS) && nl == 0) {
          // Trace channel coefficients
          c16_t *pusch_ch_est_dmrs_pos_slot_mem = rdata->pusch_ch_est_dmrs_pos_slot_mem;
          int dmrs_symbol_start_idx = rdata->dmrs_symbol_start_idx;
          pusch_ch_est_dmrs_pos_slot_mem[dmrs_symbol_start_idx + delta + ch_est_cnt] = ch16; // 0, 2, 4, 6, 8, location of REs
          pusch_ch_est_dmrs_pos_slot_mem[dmrs_symbol_start_idx + delta + ch_est_cnt + 2] = ch16; // 0, 2, 4, 6, 8, location of REs
        }
        ch_est_cnt += 4;
#endif
        pilot_cnt += 2;
      }
      c16_t ch_estimates_time[frame_parms->ofdm_symbol_size] __attribute__((aligned(32)));
      nr_est_delay(frame_parms->ofdm_symbol_size, ul_ls_est, ch_estimates_time, delay);
      if (rdata->scope && antenna == 0) {
        metadata mt = {.slot = -1, .frame = -1};
        scopeData_t *tmp = rdata->scope;
        tmp->copyData(tmp, gNBulDelay, ch_estimates_time, sizeof(c16_t), 1, frame_parms->ofdm_symbol_size, 0, &mt);
      }
      int delay_idx = get_delay_idx(delay->est_delay, MAX_DELAY_COMP);
      c16_t *ul_delay_table = frame_parms->delay_table[delay_idx];

#ifdef DEBUG_PUSCH
      printf("Estimated delay = %i\n", delay->est_delay >> 1);
#endif

      pilot_cnt = 0;
      for (int n = 0; n < 3 * nb_rb_pusch; n++) {
        // Channel interpolation
        for (int k_line = 0; k_line <= 1; k_line++) {
          // Apply delay
          int k = pilot_cnt << 1;
          c16_t ch16 = c16mulShift(ul_ls_est[k], ul_delay_table[k], 8);

#ifdef DEBUG_PUSCH
          re_offset = (k0 + (n << 2) + (k_line << 1)) % symbolSize;
          c16_t *rxF = &rxdataF[re_offset];
          printf("pilot %4d: pil -> (%6d,%6d), rxF -> (%4d,%4d), ch -> (%4d,%4d)\n",
                 pilot_cnt,
                 pil->r,
                 pil->i,
                 rxF->r,
                 rxF->i,
                 ch.r,
                 ch.i);
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
      ul_ch = &ul_ch_estimates[nl * num_sp_streams + antenna][symbol_offset];
      int inv_delay_idx = get_delay_idx(-delay->est_delay, MAX_DELAY_COMP);
      c16_t *ul_inv_delay_table = frame_parms->delay_table[inv_delay_idx];
      for (int n = 0; n < 3 * nb_rb_pusch; n++) {
        for (int k_line = 0; k_line <= 1; k_line++) {
          int k = pilot_cnt << 1;
          ul_ch[k] = c16mulShift(ul_ch[k], ul_inv_delay_table[k], 8);
          ul_ch[k + 1] = c16mulShift(ul_ch[k + 1], ul_inv_delay_table[k + 1], 8);
          noise_amp2 += c16amp2(c16sub(ul_ls_est[k], ul_ch[k]));
          noise_amp2 += c16amp2(c16sub(ul_ls_est[k + 1], ul_ch[k + 1]));

#ifdef DEBUG_PUSCH
          re_offset = (k0 + (n << 2) + (k_line << 1)) % symbolSize;
          printf("ch -> (%4d,%4d), ch_inter -> (%4d,%4d)\n", ul_ls_est[k].r, ul_ls_est[k].i, ul_ch[k].r, ul_ch[k].i);
#endif
          pilot_cnt++;
          nest_count += 2;
        }
      }

    } else if (pusch_pdu->dmrs_config_type == pusch_dmrs_type2
               && chest_freq == 0) { // pusch_dmrs_type2  |p_r,p_l,d,d,d,d,p_r,p_l,d,d,d,d|
      LOG_D(PHY, "PUSCH estimation DMRS type 2, Freq-domain interpolation\n");
      c16_t *pil = pilot;
      c16_t *rx = &rxdataF[delta];
      for (int n = 0; n < nb_rb_pusch * NR_NB_SC_PER_RB; n += 6) {
        c16_t ch0 = c16mulShift(*pil, rx[(k0 + n) % symbolSize], 15);
        pil++;
        c16_t ch1 = c16mulShift(*pil, rx[(k0 + n + 1) % symbolSize], 15);
        pil++;
        c16_t ch = c16addShift(ch0, ch1, 1);
        *max_ch = max(abs(ch.r), abs(ch.i));
        multadd_real_four_symbols_vector_complex_scalar(filt8_rep4, ch, &ul_ls_est[n]);
        ul_ls_est[n + 4] = ch;
        ul_ls_est[n + 5] = ch;
        noise_amp2 += c16amp2(c16sub(ch0, ch));
        nest_count += 1;
      }

      // Delay compensation
      c16_t ch_estimates_time[frame_parms->ofdm_symbol_size] __attribute__((aligned(32)));
      nr_est_delay(frame_parms->ofdm_symbol_size, ul_ls_est, ch_estimates_time, delay);
      if (rdata->scope && antenna == 0) {
        metadata mt = {.slot = -1, .frame = -1};
        scopeData_t *tmp = rdata->scope;
        tmp->copyData(tmp, gNBulDelay, ch_estimates_time, sizeof(c16_t), 1, frame_parms->ofdm_symbol_size, 0, &mt);
      }
      int delay_idx = get_delay_idx(-delay->est_delay, MAX_DELAY_COMP);
      c16_t *ul_delay_table = frame_parms->delay_table[delay_idx];
      for (int n = 0; n < nb_rb_pusch * NR_NB_SC_PER_RB; n++) {
        ul_ch[n] = c16mulShift(ul_ls_est[n], ul_delay_table[n % 6], 8);
      }

    }
    // this is case without frequency-domain linear interpolation, just take average of LS channel estimates of 6 DMRS REs and use a
    // common value for the whole PRB
    else if (pusch_pdu->dmrs_config_type == pusch_dmrs_type1) {
      LOG_D(PHY, "PUSCH estimation DMRS type 1, no Freq-domain interpolation\n");
      c16_t *rxF = &rxdataF[delta];
      int pil_offset = 0;
      int re_offset = k0;
      c16_t ch;

      // First PRB
      ch = c32x16cumulVectVectWithSteps(pilot, &pil_offset, 1, rxF, &re_offset, 2, symbolSize, 6);

#if NO_INTERP
      for (c16_t *end = ul_ch + 12; ul_ch < end; ul_ch++)
        *ul_ch = ch;
#else
      c16multaddVectRealComplex(filt8_avlip0, &ch, ul_ch, 8);
      ul_ch += 8;
      c16multaddVectRealComplex(filt8_avlip1, &ch, ul_ch, 8);
      ul_ch += 8;
      c16multaddVectRealComplex(filt8_avlip2, &ch, ul_ch, 8);
      ul_ch -= 12;
#endif

      for (int pilot_cnt = 6; pilot_cnt < 6 * (nb_rb_pusch - 1); pilot_cnt += 6) {
        ch = c32x16cumulVectVectWithSteps(pilot, &pil_offset, 1, rxF, &re_offset, 2, symbolSize, 6);
        *max_ch = max(abs(ch.r), abs(ch.i));

#if NO_INTERP
        for (c16_t *end = ul_ch + 12; ul_ch < end; ul_ch++)
          *ul_ch = ch;
#else
        ul_ch[3].r += (ch.r * 1365) >> 15; // 1/12*16384
        ul_ch[3].i += (ch.i * 1365) >> 15; // 1/12*16384

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
      ch = c32x16cumulVectVectWithSteps(pilot, &pil_offset, 1, rxF, &re_offset, 2, symbolSize, 6);

#if NO_INTERP
      for (c16_t *end = ul_ch + 12; ul_ch < end; ul_ch++)
        *ul_ch = ch;
#else
      ul_ch[3].r += (ch.r * 1365) >> 15; // 1/12*16384
      ul_ch[3].i += (ch.i * 1365) >> 15; // 1/12*16384

      ul_ch += 4;
      c16multaddVectRealComplex(filt8_avlip3, &ch, ul_ch, 8);
      ul_ch += 8;
      c16multaddVectRealComplex(filt8_avlip6, &ch, ul_ch, 8);
#endif
    } else { // this is case without frequency-domain linear interpolation, just take average of LS channel estimates of 4 DMRS REs
             // and use a common value for the whole PRB
      LOG_D(PHY, "PUSCH estimation DMRS type 2, no Freq-domain interpolation");
      c16_t *pil = pilot;
      int re_offset = (k0 + delta) % symbolSize;
      c32_t ch0 = {0};
      // First PRB
      ch0 = c32x16mulShift(*pil, rxdataF[re_offset], 15);
      pil++;
      re_offset = (re_offset + 1) % symbolSize;
      ch0 = c32x16maddShift(*pil, rxdataF[re_offset], ch0, 15);
      pil++;
      re_offset = (re_offset + 5) % symbolSize;
      ch0 = c32x16maddShift(*pil, rxdataF[re_offset], ch0, 15);
      re_offset = (re_offset + 1) % symbolSize;
      ch0 = c32x16maddShift(*pil, rxdataF[re_offset], ch0, 15);
      pil++;
      re_offset = (re_offset + 5) % symbolSize;

      c16_t ch = c16x32div(ch0, 4);
#if NO_INTERP
      for (c16_t *end = ul_ch + 12; ul_ch < end; ul_ch++)
        *ul_ch = ch;
#else
      c16multaddVectRealComplex(filt8_avlip0, &ch, ul_ch, 8);
      ul_ch += 8;
      c16multaddVectRealComplex(filt8_avlip1, &ch, ul_ch, 8);
      ul_ch += 8;
      c16multaddVectRealComplex(filt8_avlip2, &ch, ul_ch, 8);
      ul_ch -= 12;
#endif

      for (int pilot_cnt = 4; pilot_cnt < 4 * (nb_rb_pusch - 1); pilot_cnt += 4) {
        c32_t ch0;
        ch0 = c32x16mulShift(*pil, rxdataF[re_offset], 15);
        pil++;
        re_offset = (re_offset + 1) % symbolSize;

        ch0 = c32x16maddShift(*pil, rxdataF[re_offset], ch0, 15);
        pil++;
        re_offset = (re_offset + 5) % symbolSize;

        ch0 = c32x16maddShift(*pil, rxdataF[re_offset], ch0, 15);
        pil++;
        re_offset = (re_offset + 1) % symbolSize;

        ch0 = c32x16maddShift(*pil, rxdataF[re_offset], ch0, 15);
        pil++;
        re_offset = (re_offset + 5) % symbolSize;

        ch = c16x32div(ch0, 4);
        *max_ch = max(abs(ch.r), abs(ch.i));

#if NO_INTERP
        for (c16_t *end = ul_ch + 12; ul_ch < end; ul_ch++)
          *ul_ch = ch;
#else
        ul_ch[3] = c16maddShift(ch, (c16_t){1365, 1365}, (c16_t){0, 0}, 15); // 1365 = 1/12*16384 (full range is +/- 32768)
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
      ch0 = c32x16mulShift(*pil, rxdataF[re_offset], 15);
      pil++;
      re_offset = (re_offset + 1) % symbolSize;

      ch0 = c32x16maddShift(*pil, rxdataF[re_offset], ch0, 15);
      pil++;
      re_offset = (re_offset + 5) % symbolSize;

      ch0 = c32x16maddShift(*pil, rxdataF[re_offset], ch0, 15);
      pil++;
      re_offset = (re_offset + 1) % symbolSize;

      ch0 = c32x16maddShift(*pil, rxdataF[re_offset], ch0, 15);
      pil++;
      re_offset = (re_offset + 5) % symbolSize;

      ch = c16x32div(ch0, 4);
#if NO_INTERP
      for (c16_t *end = ul_ch + 12; ul_ch < end; ul_ch++)
        *ul_ch = ch;
#else
      ul_ch[3] = c16maddShift(ch, (c16_t){1365, 1365}, (c16_t){0, 0}, 15); // 1365 = 1/12*16384 (full range is +/- 32768)
      ul_ch += 4;
      c16multaddVectRealComplex(filt8_avlip3, &ch, ul_ch, 8);
      ul_ch += 8;
      c16multaddVectRealComplex(filt8_avlip6, &ch, ul_ch, 8);
#endif
    }

#ifdef DEBUG_PUSCH
    ul_ch = &ul_ch_estimates[nl * num_sp_streams + aarx][symbol_offset];
    for (int idxP = 0; idxP < ceil((float)nb_rb_pusch * 12 / 8); idxP++) {
      for (int idxI = 0; idxI < 8; idxI++) {
        printf("%d\t%d\t", ul_ch[idxP * 8 + idxI].r, ul_ch[idxP * 8 + idxI].i);
      }
      printf("%d\n", idxP);
    }
#endif
    // update the values inside the arrays
    *(rdata->noise_amp2) = noise_amp2;
    *(rdata->nest_count) = nest_count;
  }
  completed_task_ans(rdata->ans);
}

int nr_pusch_channel_estimation(PHY_VARS_gNB *gNB,
                                c16_t **rxdataF,
                                unsigned char Ns,
                                int nl,
                                unsigned short p,
                                uint8_t lp,
                                unsigned char symbol,
                                NR_gNB_PUSCH *pusch_vars,
                                unsigned short bwp_start_subcarrier,
                                const nfapi_nr_pusch_pdu_t *pusch_pdu,
                                int *max_ch,
                                uint32_t *nvar,
                                c16_t *pusch_dmrs_slot_mem,
                                c16_t *pusch_ch_est_dmrs_pos_slot_mem)
{
  c16_t pilot[3280] __attribute__((aligned(32)));

#ifdef DEBUG_CH
  FILE *debug_ch_est;
  debug_ch_est = fopen("debug_ch_est.txt", "w");
#endif

  const int nb_rb_pusch = pusch_pdu->rb_size;

  //------------------generate DMRS------------------//
  NR_DL_FRAME_PARMS *fp = &gNB->frame_parms;
  if (pusch_pdu->transform_precoding == transformPrecoder_disabled) {
    // Note: pilot returned by the following function is already the complex conjugate of the transmitted DMRS
    const uint32_t *gold = nr_gold_pusch(fp->N_RB_UL,
                                         fp->symbols_per_slot,
                                         gNB->gNB_config.cell_config.phy_cell_id.value,
                                         pusch_pdu->scid,
                                         Ns,
                                         symbol);
    float beta_dmrs_pusch = get_beta_dmrs(pusch_pdu->num_dmrs_cdm_grps_no_data, pusch_pdu->dmrs_config_type == pusch_dmrs_type2);
    int16_t dmrs_scaling = (1 / beta_dmrs_pusch) * (1 << 14);
    nr_pusch_dmrs_rx(fp->Ncp,
                     gold,
                     pilot,
                     (1000 + p),
                     lp % 2,
                     nb_rb_pusch,
                     (pusch_pdu->bwp_start + pusch_pdu->rb_start) * NR_NB_SC_PER_RB,
                     pusch_pdu->dmrs_config_type,
                     dmrs_scaling);
  } else { // if transform precoding or SC-FDMA is enabled in Uplink
    // NR_SC_FDMA supports type1 DMRS so only 6 DMRS REs per RB possible
    const int index = get_index_for_dmrs_lowpapr_seq(nb_rb_pusch * (NR_NB_SC_PER_RB / 2));
    const uint8_t u = pusch_pdu->dfts_ofdm.low_papr_group_number;
    const uint8_t v = pusch_pdu->dfts_ofdm.low_papr_sequence_number;
    c16_t *dmrs_seq = gNB_dmrs_lowpaprtype1_sequence[u][v][index];
    LOG_D(PHY, "Transform Precoding params. u: %d, v: %d, index for dmrsseq: %d\n", u, v, index);
    AssertFatal(index >= 0,
                "Num RBs not configured according to 3GPP 38.211 section 6.3.1.4. For PUSCH with transform precoding, num RBs "
                "cannot be multiple of any other primenumber other than 2,3,5\n");
    AssertFatal(dmrs_seq != NULL, "DMRS low PAPR seq not found, check if DMRS sequences are generated");
    nr_pusch_lowpaprtype1_dmrs_rx(fp->Ncp, dmrs_seq, pilot, 1000, 0, nb_rb_pusch, 0, pusch_pdu->dmrs_config_type);
#ifdef DEBUG_PUSCH
    printf("NR_UL_CHANNEL_EST: index %d, u %d,v %d\n", index, u, v);
    LOG_M("gNb_DMRS_SEQ.m", "gNb_DMRS_SEQ", dmrs_seq, 6 * nb_rb_pusch, 1, 1);
#endif
  }
  //------------------------------------------------//

#ifdef DEBUG_PUSCH

  for (int i = 0; i < (6 * nb_rb_pusch); i++) {
    LOG_I(PHY, "In %s: %d + j*(%d)\n", __FUNCTION__, pilot[i].r, pilot[i].i);
  }

#endif

  //------------------Write DMRS to Memory for Data Recording ------------------//
  int dmrs_symbol_start_idx = symbol * pusch_pdu->nrOfLayers * nb_rb_pusch * NR_NB_SC_PER_RB + nl * nb_rb_pusch * NR_NB_SC_PER_RB;
#if T_TRACER
  if (T_ACTIVE(T_GNB_PHY_UL_FD_DMRS) && nl == 0) {
    // used by T-Tracer to trace DMRS slot grid
    int dmrs_delta = 0; // intialize it to zero currently, derive it later from above functions
    for (int i = 0; i < (6 * nb_rb_pusch); i++) {
      // the generated DMRs is a complex conjugate of mod table, so flip the sign of imag. part
      pusch_dmrs_slot_mem[dmrs_symbol_start_idx + dmrs_delta + i * 2].r = pilot[i].r; // 0, 2, 4, 6, 8, location of REs
      pusch_dmrs_slot_mem[dmrs_symbol_start_idx + dmrs_delta + i * 2].i = -pilot[i].i; // 0, 2, 4, 6, 8, location of REs
    }
  }
#endif

  int nest_count = 0;
  uint64_t noise_amp2 = 0;
  delay_t *delay = &pusch_vars->delay;
  memset(delay, 0, sizeof(*delay));

  int nb_antennas_rx = pusch_pdu->param_v4.numSpatialStreamIndices;
  delay_t delay_arr[nb_antennas_rx];
  uint64_t noise_amp2_arr[nb_antennas_rx];
  int max_ch_arr[nb_antennas_rx];
  int nest_count_arr[nb_antennas_rx];

  for (int i = 0; i < nb_antennas_rx; ++i) {
    max_ch_arr[i] = *max_ch;
    nest_count_arr[i] = nest_count;
    noise_amp2_arr[i] = noise_amp2;
    delay_arr[i] = *delay;
  }

  notifiedFIFO_t respPuschAarx;
  initNotifiedFIFO(&respPuschAarx);
  start_meas(&gNB->pusch_channel_estimation_antenna_processing_stats);
  int numAntennas = gNB->dmrs_num_antennas_per_thread;
  int num_jobs = CEILIDIV(nb_antennas_rx, numAntennas);
  puschAntennaProc_t rdatas[num_jobs];
  memset(rdatas, 0, sizeof(rdatas));
  task_ans_t ans;
  init_task_ans(&ans, num_jobs);
  for (int job_id = 0; job_id < num_jobs; job_id++) {
    puschAntennaProc_t *rdata = &rdatas[job_id];
    task_t task = {.func = nr_pusch_antenna_processing, .args = rdata};

    // Local init in the current loop
    rdata->Ns = Ns;
    rdata->nl = nl;
    rdata->p = p;
    rdata->symbol = symbol;
    rdata->aarx = job_id * numAntennas;
    rdata->numAntennas = numAntennas;
    rdata->bwp_start_subcarrier = bwp_start_subcarrier;
    rdata->pusch_pdu = pusch_pdu;
    rdata->max_ch = &max_ch_arr[rdata->aarx];
    rdata->pilot = pilot;
    rdata->nest_count = &nest_count_arr[rdata->aarx];
    rdata->noise_amp2 = &noise_amp2_arr[rdata->aarx];
    rdata->delay = &delay_arr[rdata->aarx];
    rdata->frame_parms = fp;
    rdata->pusch_vars = pusch_vars;
    rdata->chest_freq = gNB->chest_freq;
    rdata->rxdataF = rxdataF;
    rdata->scope = gNB->scopeData;
    rdata->ans = &ans;
    rdata->pusch_ch_est_dmrs_pos_slot_mem = pusch_ch_est_dmrs_pos_slot_mem;
    rdata->dmrs_symbol_start_idx = dmrs_symbol_start_idx;
    // Call the nr_pusch_antenna_processing function
    if (job_id == num_jobs - 1) {
      // Run the last job inline
      nr_pusch_antenna_processing(rdata);
    } else {
      pushTpool(&gNB->threadPool, task);
    }
  } // Antenna Loop

  join_task_ans(&ans);

  stop_meas(&gNB->pusch_channel_estimation_antenna_processing_stats);
  for (int aarx = 0; aarx < nb_antennas_rx; aarx++) {
    *max_ch = max(*max_ch, max_ch_arr[aarx]);
    noise_amp2 += noise_amp2_arr[aarx];
    nest_count += nest_count_arr[aarx];
  }
  // get the maximum delay
  *delay = delay_arr[0];
  for (int aarx = 1; aarx < nb_antennas_rx; aarx++) {
    if (delay_arr[aarx].est_delay >= delay->est_delay) {
      *delay = delay_arr[aarx];
    }
  }

#ifdef DEBUG_CH
  fclose(debug_ch_est);
#endif

  if (nvar && nest_count > 0) {
    *nvar = (uint32_t)(noise_amp2 / nest_count);
  }

  return 0;
}

/*******************************************************************
 *
 * NAME :         nr_pusch_ptrs_processing
 *
 * PARAMETERS :   gNB         : gNB data structure
 *                rel15_ul    : UL parameters
 *                pusch_vars  : PUSCH data
 *                nr_tti_rx   : slot rx TTI
 *            dmrs_symbol_flag: DMRS Symbol Flag
 *                symbol      : OFDM Symbol
 *                nb_re_pusch : PUSCH RE's
 *                nb_re_pusch : PUSCH RE's
 *
 * RETURN :       nothing
 *
 * DESCRIPTION :
 *  If ptrs is enabled process the symbol accordingly
 *  1) Estimate phase noise per PTRS symbol
 *  2) Interpolate PTRS estimated value in TD after all PTRS symbols
 *  3) Compensated DMRS based estimated signal with PTRS estimation for slot
 *********************************************************************/
// #define DEBUG_UL_PTRS
void nr_pusch_ptrs_processing(PHY_VARS_gNB *gNB,
                              NR_DL_FRAME_PARMS *frame_parms,
                              const nfapi_nr_pusch_pdu_t *rel15_ul,
                              NR_gNB_PUSCH *pusch_vars,
                              uint8_t nr_tti_rx,
                              unsigned char symbol,
                              int nb_rx_ant,
                              uint32_t nb_re_pusch)
{
  int32_t *ptrs_re_symbol = NULL;
  int8_t ret = 0;
  uint8_t symbInSlot = rel15_ul->start_symbol_index + rel15_ul->nr_of_symbols;
  uint8_t startSymbIndex = rel15_ul->start_symbol_index;
  uint8_t nbSymb = rel15_ul->nr_of_symbols;
  uint8_t L_ptrs = rel15_ul->pusch_ptrs.ptrs_time_density;
  uint8_t K_ptrs = rel15_ul->pusch_ptrs.ptrs_freq_density;
  uint16_t dmrsSymbPos = rel15_ul->ul_dmrs_symb_pos;
  uint16_t nb_rb = rel15_ul->rb_size;
  uint8_t ptrsReOffset = rel15_ul->pusch_ptrs.ptrs_ports_list[0].ptrs_re_offset;

  /* loop over antennas */
  for (int aarx = 0; aarx < nb_rx_ant; aarx++) {
    c16_t *phase_per_symbol = (c16_t *)pusch_vars->ptrs_phase_per_slot[aarx];
    ptrs_re_symbol = &pusch_vars->ptrs_re_per_slot;
    *ptrs_re_symbol = 0;
    phase_per_symbol[symbol].i = 0;
    /* set DMRS estimates to 0 angle with magnitude 1 */
    if (is_dmrs_symbol(symbol, dmrsSymbPos)) {
      /* set DMRS real estimation to 32767 */
      phase_per_symbol[symbol].r = INT16_MAX; // 32767
#ifdef DEBUG_UL_PTRS
      printf("[PHY][PTRS]: DMRS Symbol %d -> %4d + j*%4d\n", symbol, phase_per_symbol[symbol].r, phase_per_symbol[symbol].i);
#endif
    } else { // real ptrs value is set to 0
      phase_per_symbol[symbol].r = 0;
    }

    if (symbol == startSymbIndex) {
      pusch_vars->ptrs_symbols = 0;
      set_ptrs_symb_idx(&pusch_vars->ptrs_symbols, nbSymb, startSymbIndex, 1 << L_ptrs, dmrsSymbPos);
    }

    /* Check if current symbol contains PTRS */
    if (is_ptrs_symbol(symbol, pusch_vars->ptrs_symbols)) {
      /*------------------------------------------------------------------------------------------------------- */
      /* 1) Estimate common phase error per PTRS symbol                                                                */
      /*------------------------------------------------------------------------------------------------------- */
      const uint32_t *gold = nr_gold_pusch(frame_parms->N_RB_UL,
                                           frame_parms->symbols_per_slot,
                                           gNB->gNB_config.cell_config.phy_cell_id.value,
                                           rel15_ul->scid,
                                           nr_tti_rx,
                                           symbol);
      nr_ptrs_cpe_estimation(K_ptrs,
                             ptrsReOffset,
                             nb_rb,
                             rel15_ul->rnti,
                             frame_parms->ofdm_symbol_size,
                             &pusch_vars->rxdataF_comp[aarx][(symbol * nb_re_pusch)],
                             gold,
                             (int16_t *)&phase_per_symbol[symbol],
                             ptrs_re_symbol);
    }

    /* For last OFDM symbol at each antenna perform interpolation and compensation for the slot*/
    if (symbol == (symbInSlot - 1)) {
      /*------------------------------------------------------------------------------------------------------- */
      /* 2) Interpolate PTRS estimated value in TD */
      /*------------------------------------------------------------------------------------------------------- */
      /* If L-PTRS is > 0 then we need interpolation */
      if (L_ptrs > 0) {
        ret = nr_ptrs_process_slot(dmrsSymbPos, pusch_vars->ptrs_symbols, (int16_t *)phase_per_symbol, startSymbIndex, nbSymb);
        if (ret != 0) {
          LOG_W(PHY, "[PTRS] Compensation is skipped due to error in PTRS slot processing !!\n");
        }
      }

      /*------------------------------------------------------------------------------------------------------- */
      /* 3) Compensated DMRS based estimated signal with PTRS estimation                                        */
      /*--------------------------------------------------------------------------------------------------------*/
      for (uint8_t i = startSymbIndex; i < symbInSlot; i++) {
        /* DMRS Symbol has 0 phase so no need to rotate the respective symbol */
        /* Skip rotation if the slot processing is wrong */
        if ((!is_dmrs_symbol(i, dmrsSymbPos)) && (ret == 0)) {
#ifdef DEBUG_UL_PTRS
          printf("[PHY][UL][PTRS]: Rotate Symbol %2d with  %d + j* %d\n", i, phase_per_symbol[i].r, phase_per_symbol[i].i);
#endif
          rotate_cpx_vector(&pusch_vars->rxdataF_comp[aarx][i * nb_re_pusch],
                            &phase_per_symbol[i],
                            &pusch_vars->rxdataF_comp[aarx][i * nb_re_pusch],
                            (nb_rb * NR_NB_SC_PER_RB),
                            15);
        } // if not DMRS Symbol
      } // symbol loop
    } // last symbol check
  } // Antenna loop
}

int nr_srs_ls_channel_estimation(int ant,
                                 int p_index,
                                 uint16_t ofdm_symbol_size,
                                 uint16_t first_carrier_offset,
                                 uint8_t N_symb_SRS,
                                 const nfapi_nr_srs_pdu_t *srs_pdu,
                                 const nr_srs_info_t *nr_srs_info,
                                 const c16_t *srs_generated_signal,
                                 c16_t srs_received_signal[ofdm_symbol_size * N_symb_SRS],
                                 c16_t srs_ls_estimated_channel[ofdm_symbol_size * N_symb_SRS],
                                 delay_t *delay)
{
#ifdef SRS_DEBUG
  LOG_I(NR_PHY, "Calling %s function\n", __FUNCTION__);
#endif

  const uint64_t subcarrier_offset = srs_pdu->bwp_start * NR_NB_SC_PER_RB;

  const uint8_t N_ap = 1 << srs_pdu->num_ant_ports;
  const uint8_t K_TC = 2 << srs_pdu->comb_size;
  const uint16_t m_SRS_b = get_m_srs(srs_pdu->config_index, srs_pdu->bandwidth_index);
  const uint16_t M_sc_b_SRS = m_SRS_b * NR_NB_SC_PER_RB / K_TC;
  uint8_t fd_cdm = N_ap;
  if (N_ap == 4 && ((K_TC == 2 && srs_pdu->cyclic_shift >= 4) || (K_TC == 4 && srs_pdu->cyclic_shift >= 6))) {
    fd_cdm = 2;
  }

  memset(srs_ls_estimated_channel, 0, ofdm_symbol_size * N_symb_SRS * sizeof(c16_t));

  for (int srs_symb = 0; srs_symb < N_symb_SRS; srs_symb++) {
    uint16_t srs_symbol_offset = srs_symb * ofdm_symbol_size;

#ifdef SRS_DEBUG
    LOG_I(NR_PHY, "====================== UE port %d --> gNB Rx antenna %i ======================\n", p_index, ant);
    LOG_I(NR_PHY, "============================== SRS symbol index %d ===========================\n", srs_symb);
#else
    UNUSED(ant);
#endif

    uint16_t subcarrier = subcarrier_offset + nr_srs_info->k_0_p[p_index][srs_symb];

    c16_t ls_estimated = {0};
    for (int k = 0; k < M_sc_b_SRS; k++) {
      if (k % fd_cdm == 0) {
        ls_estimated = (c16_t){0, 0};
        uint16_t subcarrier_cdm = subcarrier;

        for (int cdm_idx = 0; cdm_idx < fd_cdm; cdm_idx++) {
          c16_t generated_srs = srs_generated_signal[srs_symbol_offset + subcarrier_cdm];
          c16_t received_srs = srs_received_signal[srs_symbol_offset + subcarrier_cdm];
          // We know that nr_srs_info->srs_generated_signal_bits bits are enough to represent the real and imaginary parts of
          // generated_srs. So we only need a nr_srs_info->srs_generated_signal_bits shift to ensure that the result fits into 16
          // bits.
          ls_estimated = c16maddConjShift(generated_srs, received_srs, ls_estimated, nr_srs_info->srs_generated_signal_bits);

          // Subcarrier increment
          subcarrier_cdm = subcarrier_cdm + K_TC;
        }
      }

      for (int ktc = 0; ktc < K_TC && srs_symbol_offset + subcarrier + ktc < ofdm_symbol_size * N_symb_SRS; ktc++) {
        srs_ls_estimated_channel[srs_symbol_offset + subcarrier + ktc] = ls_estimated;
      }

#ifdef SRS_DEBUG
      int subcarrier_log = subcarrier - subcarrier_offset;
      if (subcarrier_log < 0) {
        subcarrier_log = subcarrier_log + ofdm_symbol_size;
      }
      if (subcarrier_log % 12 == 0) {
        LOG_I(NR_PHY, "------------------------------------ %d ------------------------------------\n", subcarrier_log / 12);
        LOG_I(NR_PHY, "\t  __genRe________genIm__|____rxRe_________rxIm__|____lsRe________lsIm_\n");
      }
      LOG_I(NR_PHY,
            "(%4i) %6i\t%6i  |  %6i\t%6i  |  %6i\t%6i\n",
            subcarrier_log,
            srs_generated_signal[srs_symbol_offset + subcarrier].r,
            srs_generated_signal[srs_symbol_offset + subcarrier].i,
            srs_received_signal[srs_symbol_offset + subcarrier].r,
            srs_received_signal[srs_symbol_offset + subcarrier].i,
            ls_estimated.r,
            ls_estimated.i);
#endif

      // Subcarrier increment
      subcarrier = subcarrier + K_TC;
    } // for (int k = 0; k < M_sc_b_SRS; k++)

    // Delay estimation
    if (srs_symb == 0) {
      c16_t ch_estimates_time[ofdm_symbol_size] __attribute__((aligned(32)));
      nr_est_delay(ofdm_symbol_size, srs_ls_estimated_channel, ch_estimates_time, delay);
    }
  } // for (int srs_symb = 0; srs_symb < N_symb_SRS; srs_symb++)

  return 0;
}

void nr_srs_noise_power_estimation(uint16_t ofdm_symbol_size,
                                   uint16_t first_carrier_offset,
                                   uint8_t N_symb_SRS,
                                   const nfapi_nr_srs_pdu_t *srs_pdu,
                                   const nr_srs_info_t *nr_srs_info,
                                   uint32_t signal_power,
                                   const c16_t srs_received_noise[ofdm_symbol_size * N_symb_SRS],
                                   uint32_t *noise_power,
                                   int16_t *noise_power_per_rb)
{
  const uint64_t subcarrier_offset = srs_pdu->bwp_start * NR_NB_SC_PER_RB;
  const uint16_t m_SRS_b = get_m_srs(srs_pdu->config_index, srs_pdu->bandwidth_index);
  int tot_subcarriers = m_SRS_b * NR_NB_SC_PER_RB;

  uint16_t subcarrier = subcarrier_offset + nr_srs_info->k_0_p[0][0];

  *noise_power = signal_energy_nodc(&srs_received_noise[subcarrier], tot_subcarriers) / tot_subcarriers;

  // Compute SNR per RB on symbol 0
  subcarrier = subcarrier_offset + nr_srs_info->k_0_p[0][0];
  for (int rb = 0; rb < m_SRS_b; rb++) {
    noise_power_per_rb[rb] += signal_energy_nodc(&srs_received_noise[subcarrier], NR_NB_SC_PER_RB);
    noise_power_per_rb[rb] = max(noise_power_per_rb[rb], 1);
    subcarrier += NR_NB_SC_PER_RB;

#ifdef SRS_DEBUG
    LOG_I(NR_PHY,
          "[RB %3i] noise_power_per_rb = %i, SNR_per_rb = %i dB\n",
          rb,
          noise_power_per_rb[rb],
          dB_fixed(signal_power) - dB_fixed(noise_power_per_rb[rb]));
#endif
  }

#ifdef SRS_DEBUG
  int32_t signal_power_dB = dB_fixed(signal_power);
  int32_t noise_power_dB = dB_fixed(*noise_power);
  LOG_I(NR_PHY,
        "signal_power = %i dB, noise_power = %i dB, SNR = %i dB\n",
        signal_power_dB,
        noise_power_dB,
        signal_power_dB - noise_power_dB);
#endif
}

int nr_srs_channel_interpolation(int p_index,
                                 uint16_t ofdm_symbol_size,
                                 uint16_t first_carrier_offset,
                                 uint8_t N_symb_SRS,
                                 const nfapi_nr_srs_pdu_t *srs_pdu,
                                 const nr_srs_info_t *nr_srs_info,
                                 const c16_t srs_ls_estimated_channel[ofdm_symbol_size * N_symb_SRS],
                                 int est_delay,
                                 c16_t srs_received_noise[ofdm_symbol_size * N_symb_SRS],
                                 c16_t srs_estimated_channel_freq[ofdm_symbol_size * N_symb_SRS],
                                 c16_t srs_estimated_channel_time[NR_SRS_IDFT_OVERSAMP_FACTOR * ofdm_symbol_size],
                                 c16_t srs_estimated_channel_time_shifted[NR_SRS_IDFT_OVERSAMP_FACTOR * ofdm_symbol_size],
                                 uint32_t *signal_power,
                                 c16_t delay_table[2 * MAX_DELAY_COMP + 1][NR_MAX_OFDM_SYMBOL_SIZE])
{
#ifdef SRS_DEBUG
  LOG_I(NR_PHY, "Calling %s function\n", __FUNCTION__);
#endif

  const uint64_t subcarrier_offset = srs_pdu->bwp_start * NR_NB_SC_PER_RB;
  const uint64_t first_subcarrier = (first_carrier_offset - (ofdm_symbol_size >> 1)) + srs_pdu->bwp_start * NR_NB_SC_PER_RB;

  const uint8_t K_TC = 2 << srs_pdu->comb_size;
  const uint16_t m_SRS_b = get_m_srs(srs_pdu->config_index, srs_pdu->bandwidth_index);
  const uint16_t M_sc_b_SRS = m_SRS_b * NR_NB_SC_PER_RB / K_TC;

  c16_t srs_estimated_channel_freq_avg[ofdm_symbol_size];
  memset(srs_estimated_channel_freq_avg, 0, ofdm_symbol_size * sizeof(c16_t));

  for (int srs_symb = 0; srs_symb < N_symb_SRS; srs_symb++) {
    uint16_t srs_symbol_offset = srs_symb * ofdm_symbol_size;

#ifdef SRS_DEBUG
    LOG_I(NR_PHY, "============================== UE port %d ====================================\n", p_index);
    LOG_I(NR_PHY, "============================== SRS symbol index %d ===========================\n", srs_symb);
#endif

    c16_t srs_est[ofdm_symbol_size] __attribute__((aligned(32)));
    memset(srs_est, 0, (ofdm_symbol_size) * sizeof(c16_t));

    // Start of buffer is 32 byte aligned.
    uint16_t subcarrier_abs = 0;
    c16_t *srs_estimated_channel16 = &srs_est[subcarrier_abs];

    uint16_t subcarrier = subcarrier_offset + nr_srs_info->k_0_p[p_index][srs_symb];

    int delay_idx = get_delay_idx(est_delay, MAX_DELAY_COMP);
    const c16_t *srs_delay_table = delay_table[delay_idx];

    // Delay table might be FFT shift sensitive. Not sure.
    uint16_t subcarrier_delay =
        (first_carrier_offset + subcarrier_offset + nr_srs_info->k_0_p[p_index][srs_symb]) % ofdm_symbol_size;
    for (int k = 0; k < M_sc_b_SRS; k++) {
      // Apply delay
      c16_t ls_estimated =
          c16mulShift(srs_ls_estimated_channel[srs_symbol_offset + subcarrier], srs_delay_table[subcarrier_delay], 8);

      // Channel interpolation
      if (srs_pdu->comb_size == 0) {
        if (k == 0) { // First subcarrier case
          // filt8_start is {12288,8192,4096,0,0,0,0,0}
          c16multaddVectRealComplex(filt8_start, &ls_estimated, srs_estimated_channel16, 8);
        } else if (k == (M_sc_b_SRS - 1)) { // End of OFDM symbol or last subcarrier cases
          // filt8_end is {4096,8192,12288,16384,0,0,0,0}
          c16multaddVectRealComplex(filt8_end, &ls_estimated, srs_estimated_channel16, 8);
        } else if (k % 2 == 1) { // 1st middle case
          // filt8_middle2 is {4096,8192,8192,8192,4096,0,0,0}
          c16multaddVectRealComplex(filt8_middle2, &ls_estimated, srs_estimated_channel16, 8);
        } else if (k % 2 == 0) { // 2nd middle case
          // filt8_middle4 is {0,0,4096,8192,8192,8192,4096,0}
          c16multaddVectRealComplex(filt8_middle4, &ls_estimated, srs_estimated_channel16, 8);
          srs_estimated_channel16 = &srs_est[subcarrier_abs];
        }
      } else {
        if (k == 0) { // First subcarrier case
          // filt16_start is {12288,8192,8192,8192,4096,0,0,0,0,0,0,0,0,0,0,0}
          c16multaddVectRealComplex(filt16_start, &ls_estimated, srs_estimated_channel16, 16);
        } else if (k == (M_sc_b_SRS - 1)) { // End of OFDM symbol or last subcarrier cases
          // filt16_end is {4096,8192,8192,8192,12288,16384,16384,16384,0,0,0,0,0,0,0,0}
          c16multaddVectRealComplex(filt16_end, &ls_estimated, srs_estimated_channel16, 16);
        } else { // Middle case
          // filt16_middle4 is {4096,8192,8192,8192,8192,8192,8192,8192,4096,0,0,0,0,0,0,0}
          c16multaddVectRealComplex(filt16_middle4, &ls_estimated, srs_estimated_channel16, 16);
          srs_estimated_channel16 = &srs_est[subcarrier_abs];
        }
      }

      // Subcarrier increment
      subcarrier += K_TC;
      subcarrier_delay = CIRCULAR_INC(subcarrier_delay, K_TC, ofdm_symbol_size);
      subcarrier_abs += K_TC;
    } // for (int k = 0; k < M_sc_b_SRS; k++)

    // Revert delay
    int inv_delay_idx = get_delay_idx(-est_delay, MAX_DELAY_COMP);
    const c16_t *srs_inv_delay_table = delay_table[inv_delay_idx];
    subcarrier_abs = 0;
    subcarrier_delay = CIRCULAR_INC(first_carrier_offset + subcarrier_offset + nr_srs_info->k_0_p[p_index][0], 0, ofdm_symbol_size);

    for (int k = 0; k < K_TC * M_sc_b_SRS; k++) {
      srs_est[subcarrier_abs] = c16mulShift(srs_est[subcarrier_abs], srs_inv_delay_table[subcarrier_delay], 8);
      // Subcarrier increment
      subcarrier_delay = CIRCULAR_INC(subcarrier_delay, 1, ofdm_symbol_size);
      subcarrier_abs++;
    }

    // Copy first negative SC.
    subcarrier_abs = first_subcarrier + nr_srs_info->k_0_p[p_index][srs_symb];
    memcpy(&srs_estimated_channel_freq[srs_symbol_offset + subcarrier_abs],
           srs_est,
           (ofdm_symbol_size - subcarrier_abs) * sizeof(c16_t));

    // Average srs channel estimates over multiple symbols
    int16_t scale_factor = (1 << 15) / N_symb_SRS;
    multadd_complex_vector_real_scalar(&srs_estimated_channel_freq[srs_symbol_offset],
                                       scale_factor,
                                       srs_estimated_channel_freq_avg,
                                       ofdm_symbol_size);

#ifdef SRS_DEBUG
    subcarrier = CIRCULAR_INC(subcarrier_offset + nr_srs_info->k_0_p[p_index][srs_symb], 0, ofdm_symbol_size);
    subcarrier_abs = first_subcarrier + nr_srs_info->k_0_p[p_index][srs_symb];

    for (int k = 0; k < K_TC * M_sc_b_SRS; k++) {
      int subcarrier_log = subcarrier - subcarrier_offset;
      if (subcarrier_log < 0) {
        subcarrier_log = subcarrier_log + ofdm_symbol_size;
      }

      if (subcarrier_log % 12 == 0) {
        LOG_I(NR_PHY,
              "---------------------------- PRB %d, symbol %d -------------------------------\n",
              subcarrier_log / 12,
              srs_symb);
        LOG_I(NR_PHY, "\t  __lsRe__________lsIm__|____intRe_______intIm__|____noiRe_______noiIm__\n");
      }

      LOG_I(NR_PHY,
            "(%4i) %6i\t%6i  |  %6i\t%6i  |  %6i\t%6i\n",
            subcarrier_log,
            srs_ls_estimated_channel[srs_symbol_offset + subcarrier].r,
            srs_ls_estimated_channel[srs_symbol_offset + subcarrier].i,
            srs_estimated_channel_freq[srs_symbol_offset + subcarrier_abs].r,
            srs_estimated_channel_freq[srs_symbol_offset + subcarrier_abs].i,
            srs_received_noise[srs_symbol_offset + subcarrier].r,
            srs_received_noise[srs_symbol_offset + subcarrier].i);

      // Subcarrier increment
      subcarrier = CIRCULAR_INC(subcarrier, 1, ofdm_symbol_size);
      subcarrier_abs++;
    }
#endif
  }

  // Convert to time domain
  int16_t ofdm_symbol_size_half = ofdm_symbol_size >> 1;
  int16_t ofdm_os_size = NR_SRS_IDFT_OVERSAMP_FACTOR * ofdm_symbol_size;
  int16_t ofdm_os_size_half = ofdm_os_size >> 1;
  int16_t start_offset = ofdm_os_size - ofdm_symbol_size_half;

  c16_t chF_interpol[ofdm_os_size] __attribute__((aligned(32)));
  memset(chF_interpol, 0, sizeof(chF_interpol));

  // Place SRS channel estimates in FFT shifted format for oversampling
  memcpy(&chF_interpol[0], &srs_estimated_channel_freq_avg[ofdm_symbol_size_half], ofdm_symbol_size_half * sizeof(c16_t));
  memcpy(&chF_interpol[start_offset], &srs_estimated_channel_freq_avg[0], ofdm_symbol_size_half * sizeof(c16_t));

  // Convert to time domain oversampled
  freq2time(ofdm_os_size, (int16_t *)chF_interpol, (int16_t *)srs_estimated_channel_time);

  // Do FFT shift
  memcpy(srs_estimated_channel_time_shifted, &srs_estimated_channel_time[ofdm_os_size_half], ofdm_os_size_half * sizeof(c16_t));

  memcpy(&srs_estimated_channel_time_shifted[ofdm_os_size_half], srs_estimated_channel_time, ofdm_os_size_half * sizeof(c16_t));

  // Compute wideband SNR on the symbol 0
  int tot_subcarriers = m_SRS_b * NR_NB_SC_PER_RB;
  uint16_t subcarrier_abs = first_subcarrier + nr_srs_info->k_0_p[p_index][0];
  *signal_power = signal_energy_nodc(&srs_estimated_channel_freq[subcarrier_abs], tot_subcarriers);

  if (*signal_power == 0) {
    LOG_W(NR_PHY, "Received SRS signal power is 0\n");
    return -1;
  }

  return 0;
}
