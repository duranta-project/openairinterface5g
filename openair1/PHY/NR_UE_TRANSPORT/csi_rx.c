/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/***********************************************************************
*
* FILENAME    :  csi_rx.c
*
* MODULE      :
*
* DESCRIPTION :  function to receive the channel state information
*
************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "executables/nr-softmodem-common.h"
#include "nr_transport_proto_ue.h"
#include "PHY/NR_REFSIG/nr_refsig.h"
#include "common/utils/nr/nr_common.h"
#include "PHY/NR_UE_ESTIMATION/filt16a_32.h"
#if defined(__riscv) && defined(__riscv_vector)
#include <riscv_vector.h>
#endif

//#define NR_CSIRS_DEBUG
//#define NR_CSIIM_DEBUG

extern openair0_config_t openair0_cfg[MAX_CARDS];

void nr_det_A_MF_2x2(int32_t *a_mf_00,
                     int32_t *a_mf_01,
                     int32_t *a_mf_10,
                     int32_t *a_mf_11,
                     int32_t *det_fin,
                     const unsigned short nb_rb) {

#if defined(__riscv) && defined(__riscv_vector)
  /* det[k] = |(r00*r11 - i00*i11) - (r01*r10 - i01*i10)|, int32 wrapping (matches
   * the madd/sub/abs_epi32 path). vlseg2 (re/im) + widening mul; abs = max(x,-x). */
  const int16_t *p00 = (const int16_t *)a_mf_00, *p01 = (const int16_t *)a_mf_01;
  const int16_t *p10 = (const int16_t *)a_mf_10, *p11 = (const int16_t *)a_mf_11;
  int N = 3 * nb_rb * 4;
  for (int n = 0; n < N;) {
    size_t vl = __riscv_vsetvl_e16mf2(N - n);
    int o = 2 * n;
    vint16mf2x2_t V00 = __riscv_vlseg2e16_v_i16mf2x2(p00 + o, vl), V11 = __riscv_vlseg2e16_v_i16mf2x2(p11 + o, vl);
    vint16mf2x2_t V01 = __riscv_vlseg2e16_v_i16mf2x2(p01 + o, vl), V10 = __riscv_vlseg2e16_v_i16mf2x2(p10 + o, vl);
    vint16mf2_t r00 = __riscv_vget_v_i16mf2x2_i16mf2(V00, 0), i00 = __riscv_vget_v_i16mf2x2_i16mf2(V00, 1);
    vint16mf2_t r11 = __riscv_vget_v_i16mf2x2_i16mf2(V11, 0), i11 = __riscv_vget_v_i16mf2x2_i16mf2(V11, 1);
    vint16mf2_t r01 = __riscv_vget_v_i16mf2x2_i16mf2(V01, 0), i01 = __riscv_vget_v_i16mf2x2_i16mf2(V01, 1);
    vint16mf2_t r10 = __riscv_vget_v_i16mf2x2_i16mf2(V10, 0), i10 = __riscv_vget_v_i16mf2x2_i16mf2(V10, 1);
    vint32m1_t ad = __riscv_vsub_vv_i32m1(__riscv_vwmul_vv_i32m1(r00, r11, vl), __riscv_vwmul_vv_i32m1(i00, i11, vl), vl);
    vint32m1_t bc = __riscv_vsub_vv_i32m1(__riscv_vwmul_vv_i32m1(r01, r10, vl), __riscv_vwmul_vv_i32m1(i01, i10, vl), vl);
    vint32m1_t d = __riscv_vsub_vv_i32m1(ad, bc, vl);
    __riscv_vse32_v_i32m1(det_fin + n, __riscv_vmax_vv_i32m1(d, __riscv_vneg_v_i32m1(d, vl), vl), vl);
    n += (int)vl;
  }
  return;
#endif
  simde__m128i ad_re_128, bc_re_128, det_re_128;

  simde__m128i *a_mf_00_128 = (simde__m128i *)a_mf_00;
  simde__m128i *a_mf_01_128 = (simde__m128i *)a_mf_01;
  simde__m128i *a_mf_10_128 = (simde__m128i *)a_mf_10;
  simde__m128i *a_mf_11_128 = (simde__m128i *)a_mf_11;
  simde__m128i *det_fin_128 = (simde__m128i *)det_fin;

  for (int rb = 0; rb<3*nb_rb; rb++) {

    //complex multiplication (I_a+jQ_a)(I_d+jQ_d) = (I_aI_d - Q_aQ_d) + j(Q_aI_d + I_aQ_d)
    //The imag part is often zero, we compute only the real part
    ad_re_128 = simde_mm_madd_epi16(oai_mm_conj(a_mf_00_128[0]), a_mf_11_128[0]); //Re: I_a0*I_d0 - Q_a1*Q_d1

    //complex multiplication (I_b+jQ_b)(I_c+jQ_c) = (I_bI_c - Q_bQ_c) + j(Q_bI_c + I_bQ_c)
    //The imag part is often zero, we compute only the real part
    bc_re_128 = simde_mm_madd_epi16(oai_mm_conj(a_mf_01_128[0]), a_mf_10_128[0]); //Re: I_b0*I_c0 - Q_b1*Q_c1

    det_re_128 = simde_mm_sub_epi32(ad_re_128, bc_re_128);

    //det in Q30 format
    det_fin_128[0] = simde_mm_abs_epi32(det_re_128);

    det_fin_128+=1;
    a_mf_00_128+=1;
    a_mf_01_128+=1;
    a_mf_10_128+=1;
    a_mf_11_128+=1;
  }
}

void nr_squared_matrix_element(int32_t *a,
                               int32_t *a_sq,
                               const unsigned short nb_rb) {
#if defined(__riscv) && defined(__riscv_vector)
  /* a_sq[k] = re_k^2 + im_k^2 (== madd_epi16(a,a)): vlseg2 + vwmul/vwmacc */
  const int16_t *p = (const int16_t *)a;
  int N = 3 * nb_rb * 4;
  for (int n = 0; n < N;) {
    size_t vl = __riscv_vsetvl_e16mf2(N - n);
    vint16mf2x2_t v = __riscv_vlseg2e16_v_i16mf2x2(p + 2 * n, vl);
    vint16mf2_t r = __riscv_vget_v_i16mf2x2_i16mf2(v, 0), i = __riscv_vget_v_i16mf2x2_i16mf2(v, 1);
    __riscv_vse32_v_i32m1(a_sq + n, __riscv_vwmacc_vv_i32m1(__riscv_vwmul_vv_i32m1(r, r, vl), i, i, vl), vl);
    n += (int)vl;
  }
  return;
#endif
  simde__m128i *a_128 = (simde__m128i *)a;
  simde__m128i *a_sq_128 = (simde__m128i *)a_sq;
  for (int rb=0; rb<3*nb_rb; rb++) {
    a_sq_128[0] = simde_mm_madd_epi16(a_128[0], a_128[0]);
    a_sq_128+=1;
    a_128+=1;
  }
}

void nr_numer_2x2(int32_t *a_00_sq,
                  int32_t *a_01_sq,
                  int32_t *a_10_sq,
                  int32_t *a_11_sq,
                  int32_t *num_fin,
                  const unsigned short nb_rb) {
#if defined(__riscv) && defined(__riscv_vector)
  /* num[k] = a00_sq[k]+a11_sq[k] + a01_sq[k]+a10_sq[k] (int32) */
  int N = 3 * nb_rb * 4;
  for (int n = 0; n < N;) {
    size_t vl = __riscv_vsetvl_e32m1(N - n);
    vint32m1_t s = __riscv_vadd_vv_i32m1(
        __riscv_vadd_vv_i32m1(__riscv_vle32_v_i32m1(a_00_sq + n, vl), __riscv_vle32_v_i32m1(a_11_sq + n, vl), vl),
        __riscv_vadd_vv_i32m1(__riscv_vle32_v_i32m1(a_01_sq + n, vl), __riscv_vle32_v_i32m1(a_10_sq + n, vl), vl), vl);
    __riscv_vse32_v_i32m1(num_fin + n, s, vl);
    n += (int)vl;
  }
  return;
#endif
  simde__m128i *a_00_sq_128 = (simde__m128i *)a_00_sq;
  simde__m128i *a_01_sq_128 = (simde__m128i *)a_01_sq;
  simde__m128i *a_10_sq_128 = (simde__m128i *)a_10_sq;
  simde__m128i *a_11_sq_128 = (simde__m128i *)a_11_sq;
  simde__m128i *num_fin_128 = (simde__m128i *)num_fin;
  for (int rb=0; rb<3*nb_rb; rb++) {
    simde__m128i sq_a_plus_sq_d_128 = simde_mm_add_epi32(a_00_sq_128[0], a_11_sq_128[0]);
    simde__m128i sq_b_plus_sq_c_128 = simde_mm_add_epi32(a_01_sq_128[0], a_10_sq_128[0]);
    num_fin_128[0] = simde_mm_add_epi32(sq_a_plus_sq_d_128, sq_b_plus_sq_c_128);
    num_fin_128+=1;
    a_00_sq_128+=1;
    a_01_sq_128+=1;
    a_10_sq_128+=1;
    a_11_sq_128+=1;
  }
}

bool is_csi_rs_in_symbol(const fapi_nr_dl_config_csirs_pdu_rel15_t csirs_config_pdu, const int symbol) {

  bool ret = false;

  // 38.211-Table 7.4.1.5.3-1: CSI-RS locations within a slot
  switch(csirs_config_pdu.row){
    case 1:
    case 2:
    case 3:
    case 4:
    case 6:
    case 9:
      if(symbol == csirs_config_pdu.symb_l0) {
        ret = true;
      }
      break;
    case 5:
    case 7:
    case 8:
    case 10:
    case 11:
    case 12:
      if(symbol == csirs_config_pdu.symb_l0 || symbol == (csirs_config_pdu.symb_l0+1) ) {
        ret = true;
      }
      break;
    case 13:
    case 14:
    case 16:
    case 17:
      if(symbol == csirs_config_pdu.symb_l0 || symbol == (csirs_config_pdu.symb_l0+1) ||
          symbol == csirs_config_pdu.symb_l1 || symbol == (csirs_config_pdu.symb_l1+1)) {
        ret = true;
      }
      break;
    case 15:
    case 18:
      if(symbol == csirs_config_pdu.symb_l0 || symbol == (csirs_config_pdu.symb_l0+1) || symbol == (csirs_config_pdu.symb_l0+2) ) {
        ret = true;
      }
      break;
    default:
      AssertFatal(0==1, "Row %d is not valid for CSI Table 7.4.1.5.3-1\n", csirs_config_pdu.row);
  }

  return ret;
}

static int nr_get_csi_rs_signal(const PHY_VARS_NR_UE *ue,
                                const UE_nr_rxtx_proc_t *proc,
                                const fapi_nr_dl_config_csirs_pdu_rel15_t *csirs_config_pdu,
                                const nr_csi_info_t *nr_csi_info,
                                const csi_mapping_parms_t *csi_mapping,
                                const int CDM_group_size,
                                c16_t csi_rs_received_signal[][ue->frame_parms.samples_per_slot_wCP],
                                uint32_t *rsrp,
                                int *rsrp_dBm,
                                const c16_t rxdataF[][ue->frame_parms.samples_per_slot_wCP])
{
  const NR_DL_FRAME_PARMS *fp = &ue->frame_parms;
  uint16_t meas_count = 0;
  uint32_t rsrp_sum = 0;

  for (int ant_rx = 0; ant_rx < fp->nb_antennas_rx; ant_rx++) {

    for (int rb = csirs_config_pdu->start_rb; rb < (csirs_config_pdu->start_rb+csirs_config_pdu->nr_of_rbs); rb++) {

      // for freq density 0.5 checks if even or odd RB
      if(csirs_config_pdu->freq_density <= 1 && csirs_config_pdu->freq_density != (rb % 2)) {
        continue;
      }

      for (int cdm_id = 0; cdm_id < csi_mapping->size; cdm_id++) {
        for (int s = 0; s < CDM_group_size; s++)  {

          // loop over frequency resource elements within a group
          for (int kp = 0; kp <= csi_mapping->kprime; kp++) {

            uint16_t k = (fp->first_carrier_offset + (rb * NR_NB_SC_PER_RB) + csi_mapping->koverline[cdm_id] + kp) % fp->ofdm_symbol_size;

            // loop over time resource elements within a group
            for (int lp = 0; lp <= csi_mapping->lprime; lp++) {
              uint16_t symb = lp + csi_mapping->loverline[cdm_id];
              uint64_t symbol_offset = symb * fp->ofdm_symbol_size;
              const c16_t *rx_signal = &rxdataF[ant_rx][symbol_offset];
              c16_t *rx_csi_rs_signal = &csi_rs_received_signal[ant_rx][symbol_offset];
              rx_csi_rs_signal[k].r = rx_signal[k].r;
              rx_csi_rs_signal[k].i = rx_signal[k].i;

              rsrp_sum += (((int32_t)(rx_csi_rs_signal[k].r)*rx_csi_rs_signal[k].r) +
                           ((int32_t)(rx_csi_rs_signal[k].i)*rx_csi_rs_signal[k].i));

              meas_count++;

#ifdef NR_CSIRS_DEBUG
              int dataF_offset = proc->nr_slot_rx * fp->samples_per_slot_wCP;
              uint16_t port_tx = s + csi_mapping->j[cdm_id] * CDM_group_size;
              c16_t *tx_csi_rs_signal = &nr_csi_info->csi_rs_generated_signal[port_tx][symbol_offset + dataF_offset];
              LOG_I(NR_PHY,
                    "l,k (%2d,%4d) |\tport_tx %d (%4d,%4d)\tant_rx %d (%4d,%4d)\n",
                    symb,
                    k,
                    port_tx+3000,
                    tx_csi_rs_signal[k].r,
                    tx_csi_rs_signal[k].i,
                    ant_rx,
                    rx_csi_rs_signal[k].r,
                    rx_csi_rs_signal[k].i);
#else
              UNUSED(proc);
              UNUSED(nr_csi_info);
#endif
            }
          }
        }
      }
    }
  }


  *rsrp = rsrp_sum/meas_count;
  *rsrp_dBm = dB_fixed(*rsrp) + 30 - SQ15_SQUARED_NORM_FACTOR_DB
              - ((int)openair0_cfg[ue->rf_map.card].rx_gain[0] - (int)openair0_cfg[ue->rf_map.card].rx_gain_offset[0])
              - dB_fixed(ue->frame_parms.ofdm_symbol_size);

#ifdef NR_CSIRS_DEBUG
  LOG_I(NR_PHY, "RSRP = %i (%i dBm)\n", *rsrp, *rsrp_dBm);
#endif

  return 0;
}

uint32_t calc_power_csirs(const uint16_t *x, const fapi_nr_dl_config_csirs_pdu_rel15_t *csirs_config_pdu)
{
  uint64_t sum_x = 0;
  uint64_t sum_x2 = 0;
  uint16_t size = 0;
  for (int rb = 0; rb < csirs_config_pdu->nr_of_rbs; rb++) {
    if (csirs_config_pdu->freq_density <= 1 && csirs_config_pdu->freq_density != ((rb + csirs_config_pdu->start_rb) % 2)) {
      continue;
    }
    sum_x = sum_x + x[rb];
    sum_x2 = sum_x2 + x[rb] * x[rb];
    size++;
  }
  return sum_x2 / size - (sum_x / size) * (sum_x / size);
}

static int nr_csi_rs_channel_estimation(
    const NR_DL_FRAME_PARMS *fp,
    const fapi_nr_dl_config_csirs_pdu_rel15_t *csirs_config_pdu,
    const nr_csi_info_t *nr_csi_info,
    const c16_t **csi_rs_generated_signal,
    const c16_t csi_rs_received_signal[][fp->samples_per_slot_wCP],
    const csi_mapping_parms_t *csi_mapping,
    const int CDM_group_size,
    c16_t csi_rs_ls_estimated_channel[][csi_mapping->ports][fp->ofdm_symbol_size],
    c16_t csi_rs_estimated_channel_freq[][csi_mapping->ports][fp->ofdm_symbol_size],
    int16_t *log2_re,
    int16_t *log2_maxh,
    uint32_t *noise_power)
{
  const int meas_bitmap = csirs_config_pdu->measurement_bitmap;
  AssertFatal(meas_bitmap != 1, "No need to do CSI-RS channel estimation for only RSRP measurment\n");
  *noise_power = 0;
  int maxh = 0;
  int count = 0;

  for (int ant_rx = 0; ant_rx < fp->nb_antennas_rx; ant_rx++) {

    /// LS channel estimation

    const uint16_t stop_rb = csirs_config_pdu->start_rb + csirs_config_pdu->nr_of_rbs;
    for(uint16_t port_tx = 0; port_tx < csi_mapping->ports; port_tx++) {
      memset(csi_rs_ls_estimated_channel[ant_rx][port_tx], 0, fp->ofdm_symbol_size * sizeof(c16_t));
    }

    for (int rb = csirs_config_pdu->start_rb; rb < stop_rb; rb++) {
      // for freq density 0.5 checks if even or odd RB
      if(csirs_config_pdu->freq_density <= 1 && csirs_config_pdu->freq_density != (rb % 2)) {
        continue;
      }

      for (int cdm_id = 0; cdm_id < csi_mapping->size; cdm_id++) {
        for (int s = 0; s < CDM_group_size; s++)  {

          uint16_t port_tx = s + csi_mapping->j[cdm_id] * CDM_group_size;

          // loop over frequency resource elements within a group
          for (int kp = 0; kp <= csi_mapping->kprime; kp++) {
            uint16_t kinit_rx = (fp->first_carrier_offset + rb * NR_NB_SC_PER_RB) % fp->ofdm_symbol_size;
            uint16_t k_rx = kinit_rx + csi_mapping->koverline[cdm_id] + kp;
            uint16_t kinit_tx = rb * NR_NB_SC_PER_RB;
            uint16_t k_tx = kinit_tx + csi_mapping->koverline[cdm_id] + kp;

            // loop over time resource elements within a group
            for (int lp = 0; lp <= csi_mapping->lprime; lp++) {
              uint16_t symb = lp + csi_mapping->loverline[cdm_id];
              uint64_t symbol_offset = symb * fp->ofdm_symbol_size;
              const c16_t *tx_csi_rs_signal = &csi_rs_generated_signal[port_tx][symbol_offset];
              const c16_t *rx_csi_rs_signal = &csi_rs_received_signal[ant_rx][symbol_offset];
              c16_t tmp =
                  c16MulConjShift(tx_csi_rs_signal[k_tx], rx_csi_rs_signal[k_rx], nr_csi_info->csi_rs_generated_signal_bits);
              if (csirs_config_pdu->csi_type != 0) {
                // This is not just the LS estimation for each (k,l), but also the sum of the different contributions
                // for the sake of optimizing the memory used.
                csi_rs_ls_estimated_channel[ant_rx][port_tx][kinit_tx].r += tmp.r;
                csi_rs_ls_estimated_channel[ant_rx][port_tx][kinit_tx].i += tmp.i;
              } else {
                // for tracking we want estimates of all sub carriers having CSI-RS
                csi_rs_ls_estimated_channel[ant_rx][port_tx][k_tx].r = tmp.r;
                csi_rs_ls_estimated_channel[ant_rx][port_tx][k_tx].i = tmp.i;
              }
            }
          }
        }
      }
    }

#ifdef NR_CSIRS_DEBUG
    for(int symb = 0; symb < fp->symbols_per_slot; symb++) {
      if(!is_csi_rs_in_symbol(*csirs_config_pdu,symb)) {
        continue;
      }
      for(int k = 0; k < fp->ofdm_symbol_size; k++) {
        LOG_I(NR_PHY, "l,k (%2d,%4d) | ", symb, k);
        for(uint16_t port_tx = 0; port_tx < csi_mapping->ports; port_tx++) {
          uint64_t symbol_offset = symb * fp->ofdm_symbol_size;
          c16_t *tx_csi_rs_signal = (c16_t*)&csi_rs_generated_signal[port_tx][symbol_offset+dataF_offset];
          c16_t *rx_csi_rs_signal = (c16_t*)&csi_rs_received_signal[ant_rx][symbol_offset];
          c16_t *csi_rs_ls_estimated_channel16 = csi_rs_ls_estimated_channel[ant_rx][port_tx];
          printf("port_tx %d --> ant_rx %d, tx (%4d,%4d), rx (%4d,%4d), ls (%4d,%4d) | ",
                 port_tx+3000, ant_rx,
                 tx_csi_rs_signal[k].r, tx_csi_rs_signal[k].i,
                 rx_csi_rs_signal[k].r, rx_csi_rs_signal[k].i,
                 csi_rs_ls_estimated_channel16[k].r, csi_rs_ls_estimated_channel16[k].i);
        }
        printf("\n");
      }
    }
#endif

    /// Channel interpolation

    for(uint16_t port_tx = 0; port_tx < csi_mapping->ports; port_tx++) {
      memset(csi_rs_estimated_channel_freq[ant_rx][port_tx], 0, (fp->ofdm_symbol_size) * sizeof(c16_t));
    }

    for (int rb = csirs_config_pdu->start_rb; rb < stop_rb; rb++) {
      // for freq density 0.5 checks if even or odd RB
      if(csirs_config_pdu->freq_density <= 1 && csirs_config_pdu->freq_density != (rb % 2)) {
        continue;
      }

      count++;

      uint16_t k = rb * NR_NB_SC_PER_RB;
      for(uint16_t port_tx = 0; port_tx < csi_mapping->ports; port_tx++) {
        c16_t csi_rs_ls_estimated_channel16 = csi_rs_ls_estimated_channel[ant_rx][port_tx][k];
        c16_t *csi_rs_estimated_channel16 = &csi_rs_estimated_channel_freq[ant_rx][port_tx][k];
        if (k == 0) { // Start of OFDM symbol case or first occupied subcarrier case
          multadd_real_vector_complex_scalar(filt24_start, csi_rs_ls_estimated_channel16, csi_rs_estimated_channel16, 24);
        } else if (rb == (stop_rb - 1)) { // End of OFDM symbol case or Last occupied subcarrier case
          multadd_real_vector_complex_scalar(filt24_end, csi_rs_ls_estimated_channel16, csi_rs_estimated_channel16 - 12, 24);
        } else { // Middle case
          multadd_real_vector_complex_scalar(filt24_middle, csi_rs_ls_estimated_channel16, csi_rs_estimated_channel16 - 12, 24);
        }
      }
    }

    // we need only ls estimates for tracking CSI. So stop here.
    if (meas_bitmap == 0)
      continue;

    /// Power noise estimation
    AssertFatal(csirs_config_pdu->nr_of_rbs > 0, " nr_of_rbs needs to be greater than 0\n");
    uint16_t noise_real[fp->nb_antennas_rx][csi_mapping->ports][csirs_config_pdu->nr_of_rbs];
    uint16_t noise_imag[fp->nb_antennas_rx][csi_mapping->ports][csirs_config_pdu->nr_of_rbs];
    for (int rb = csirs_config_pdu->start_rb; rb < stop_rb; rb++) {
      if (csirs_config_pdu->freq_density <= 1 && csirs_config_pdu->freq_density != (rb % 2)) {
        continue;
      }
      uint16_t k = rb * NR_NB_SC_PER_RB;
      for(uint16_t port_tx = 0; port_tx < csi_mapping->ports; port_tx++) {
        c16_t *csi_rs_ls_estimated_channel16 = &csi_rs_ls_estimated_channel[ant_rx][port_tx][k];
        c16_t *csi_rs_estimated_channel16 = &csi_rs_estimated_channel_freq[ant_rx][port_tx][k];
        noise_real[ant_rx][port_tx][rb-csirs_config_pdu->start_rb] = abs(csi_rs_ls_estimated_channel16->r-csi_rs_estimated_channel16->r);
        noise_imag[ant_rx][port_tx][rb-csirs_config_pdu->start_rb] = abs(csi_rs_ls_estimated_channel16->i-csi_rs_estimated_channel16->i);
        maxh = cmax3(maxh, abs(csi_rs_estimated_channel16->r), abs(csi_rs_estimated_channel16->i));
      }
    }
    for(uint16_t port_tx = 0; port_tx < csi_mapping->ports; port_tx++) {
      *noise_power += (calc_power_csirs(noise_real[ant_rx][port_tx], csirs_config_pdu) + calc_power_csirs(noise_imag[ant_rx][port_tx],csirs_config_pdu));
    }

#ifdef NR_CSIRS_DEBUG
    for(int k = 0; k < fp->ofdm_symbol_size; k++) {
      int rb = k / NR_NB_SC_PER_RB;
      LOG_I(NR_PHY, "(k = %4d) |\t", k);
      for(uint16_t port_tx = 0; port_tx < csi_mapping->ports; port_tx++) {
        c16_t *csi_rs_ls_estimated_channel16 = &csi_rs_ls_estimated_channel[ant_rx][port_tx][0];
        c16_t *csi_rs_estimated_channel16 = &csi_rs_estimated_channel_freq[ant_rx][port_tx][0];
        printf("Channel port_tx %d --> ant_rx %d : ls (%4d,%4d), int (%4d,%4d), noise (%4d,%4d) | ",
               port_tx + 3000,
               ant_rx,
               csi_rs_ls_estimated_channel16[k].r,
               csi_rs_ls_estimated_channel16[k].i,
               csi_rs_estimated_channel16[k].r,
               csi_rs_estimated_channel16[k].i,
               rb >= stop_rb ? 0 : noise_real[ant_rx][port_tx][rb - csirs_config_pdu->start_rb],
               rb >= stop_rb ? 0 : noise_imag[ant_rx][port_tx][rb - csirs_config_pdu->start_rb]);
      }
      printf("\n");
    }
#endif

  }

  if (meas_bitmap > 1) {
    *noise_power /= (fp->nb_antennas_rx * csi_mapping->ports);
    *log2_maxh = log2_approx(maxh - 1);
    *log2_re = log2_approx(count - 1);
  }

#ifdef NR_CSIRS_DEBUG
  LOG_I(NR_PHY, "Noise power estimation based on CSI-RS: %i\n", *noise_power);
#endif
  return 0;
}

static int nr_csi_rs_ri_estimation(const PHY_VARS_NR_UE *ue,
                                   const fapi_nr_dl_config_csirs_pdu_rel15_t *csirs_config_pdu,
                                   const uint8_t N_ports,
                                   c16_t csi_rs_estimated_channel_freq[][N_ports][ue->frame_parms.ofdm_symbol_size],
                                   const int16_t log2_maxh,
                                   uint8_t *rank_indicator)
{
  const NR_DL_FRAME_PARMS *frame_parms = &ue->frame_parms;
  const int16_t cond_dB_threshold = 5;
  int count = 0;
  *rank_indicator = 0;

  if (ue->frame_parms.nb_antennas_rx == 1 || N_ports == 1) {
    return 0;
  } else if( !(ue->frame_parms.nb_antennas_rx == 2 && N_ports == 2) ) {
    LOG_W(NR_PHY, "Rank indicator computation is not implemented for %i x %i system\n",
          ue->frame_parms.nb_antennas_rx, N_ports);
    return -1;
  }

  /* Example 2x2: Hh x H =
  *            | conjch00 conjch10 | x | ch00 ch01 | = | conjch00*ch00+conjch10*ch10 conjch00*ch01+conjch10*ch11 |
  *            | conjch01 conjch11 |   | ch10 ch11 |   | conjch01*ch00+conjch11*ch10 conjch01*ch01+conjch11*ch11 |
  */

  c16_t csi_rs_estimated_conjch_ch[frame_parms->nb_antennas_rx][N_ports][frame_parms->nb_antennas_rx][N_ports]
                                  [frame_parms->ofdm_symbol_size] __attribute__((aligned(32)));
  int32_t csi_rs_estimated_A_MF[N_ports][N_ports][frame_parms->ofdm_symbol_size] __attribute__((aligned(32)));
  int32_t csi_rs_estimated_A_MF_sq[N_ports][N_ports][frame_parms->ofdm_symbol_size] __attribute__((aligned(32)));
  int32_t csi_rs_estimated_determ_fin[frame_parms->ofdm_symbol_size] __attribute__((aligned(32)));
  int32_t csi_rs_estimated_numer_fin[frame_parms->ofdm_symbol_size] __attribute__((aligned(32)));
  const uint8_t sum_shift = 1; // log2(2x2) = 2, which is a shift of 1 bit
  
  for (int rb = csirs_config_pdu->start_rb; rb < (csirs_config_pdu->start_rb+csirs_config_pdu->nr_of_rbs); rb++) {

    if (csirs_config_pdu->freq_density <= 1 && csirs_config_pdu->freq_density != (rb % 2)) {
      continue;
    }
    uint16_t k = rb * NR_NB_SC_PER_RB;

    for (int ant_rx_conjch = 0; ant_rx_conjch < frame_parms->nb_antennas_rx; ant_rx_conjch++) {
      for(uint16_t port_tx_conjch = 0; port_tx_conjch < N_ports; port_tx_conjch++) {
        for (int ant_rx_ch = 0; ant_rx_ch < frame_parms->nb_antennas_rx; ant_rx_ch++) {
          for(uint16_t port_tx_ch = 0; port_tx_ch < N_ports; port_tx_ch++) {

            // conjch x ch computation
            nr_conjch0_mult_ch1(&csi_rs_estimated_channel_freq[ant_rx_conjch][port_tx_conjch][k],
                                &csi_rs_estimated_channel_freq[ant_rx_ch][port_tx_ch][k],
                                &csi_rs_estimated_conjch_ch[ant_rx_conjch][port_tx_conjch][ant_rx_ch][port_tx_ch][k],
                                1,
                                log2_maxh);

            // construct Hh x H elements
            if(ant_rx_conjch == ant_rx_ch) {
              nr_a_sum_b((c16_t *)&csi_rs_estimated_A_MF[port_tx_conjch][port_tx_ch][k],
                         (c16_t *)&csi_rs_estimated_conjch_ch[ant_rx_conjch][port_tx_conjch][ant_rx_ch][port_tx_ch][k],
                         1);
            }
          }
        }
      }
    }

    // compute the determinant of A_MF (denominator)
    nr_det_A_MF_2x2(&csi_rs_estimated_A_MF[0][0][k],
                    &csi_rs_estimated_A_MF[0][1][k],
                    &csi_rs_estimated_A_MF[1][0][k],
                    &csi_rs_estimated_A_MF[1][1][k],
                    &csi_rs_estimated_determ_fin[k],
                    1);

    // compute the square of A_MF (numerator)
    nr_squared_matrix_element(&csi_rs_estimated_A_MF[0][0][k], &csi_rs_estimated_A_MF_sq[0][0][k], 1);
    nr_squared_matrix_element(&csi_rs_estimated_A_MF[0][1][k], &csi_rs_estimated_A_MF_sq[0][1][k], 1);
    nr_squared_matrix_element(&csi_rs_estimated_A_MF[1][0][k], &csi_rs_estimated_A_MF_sq[1][0][k], 1);
    nr_squared_matrix_element(&csi_rs_estimated_A_MF[1][1][k], &csi_rs_estimated_A_MF_sq[1][1][k], 1);
    nr_numer_2x2(&csi_rs_estimated_A_MF_sq[0][0][k],
                 &csi_rs_estimated_A_MF_sq[0][1][k],
                 &csi_rs_estimated_A_MF_sq[1][0][k],
                 &csi_rs_estimated_A_MF_sq[1][1][k],
                 &csi_rs_estimated_numer_fin[k],
                 1);

#ifdef NR_CSIRS_DEBUG
    for(uint16_t port_tx_conjch = 0; port_tx_conjch < N_ports; port_tx_conjch++) {
      for(uint16_t port_tx_ch = 0; port_tx_ch < N_ports; port_tx_ch++) {
        c16_t *csi_rs_estimated_A_MF_k = (c16_t *)&csi_rs_estimated_A_MF[port_tx_conjch][port_tx_ch][k];
        LOG_I(NR_PHY, "(%i) csi_rs_estimated_A_MF[%i][%i] = (%i, %i)\n",
              k, port_tx_conjch, port_tx_ch, csi_rs_estimated_A_MF_k->r, csi_rs_estimated_A_MF_k->i);
        c16_t *csi_rs_estimated_A_MF_sq_k = (c16_t *)&csi_rs_estimated_A_MF_sq[port_tx_conjch][port_tx_ch][k];
        LOG_I(NR_PHY, "(%i) csi_rs_estimated_A_MF_sq[%i][%i] = (%i, %i)\n",
              k, port_tx_conjch, port_tx_ch, csi_rs_estimated_A_MF_sq_k->r, csi_rs_estimated_A_MF_sq_k->i);
      }
    }
    LOG_I(NR_PHY, "(%i) csi_rs_estimated_determ_fin = %i\n", k, csi_rs_estimated_determ_fin[k]);
    LOG_I(NR_PHY, "(%i) csi_rs_estimated_numer_fin = %i\n", k, csi_rs_estimated_numer_fin[k] >> sum_shift);
#endif

    // compute the conditional number
    for (int sc_idx=0; sc_idx < NR_NB_SC_PER_RB; sc_idx++) {
      int8_t csi_rs_estimated_denum_db = dB_fixed(csi_rs_estimated_determ_fin[k + sc_idx]);
      int8_t csi_rs_estimated_numer_db = dB_fixed(csi_rs_estimated_numer_fin[k + sc_idx] >> sum_shift);
      int8_t cond_db = csi_rs_estimated_numer_db - csi_rs_estimated_denum_db;

#ifdef NR_CSIRS_DEBUG
      LOG_I(NR_PHY, "csi_rs_estimated_denum_db = %i\n", csi_rs_estimated_denum_db);
      LOG_I(NR_PHY, "csi_rs_estimated_numer_db = %i\n", csi_rs_estimated_numer_db);
      LOG_I(NR_PHY, "cond_db = %i\n", cond_db);
#endif

      if (cond_db < cond_dB_threshold) {
        count++;
      } else {
        count--;
      }
    }
  }

  // conditional number is lower than cond_dB_threshold in half on more REs
  if (count > 0) {
    *rank_indicator = 1;
  }

#ifdef NR_CSIRS_DEBUG
  LOG_I(NR_PHY, "count = %i\n", count);
  LOG_I(NR_PHY, "rank = %i\n", (*rank_indicator)+1);
#endif

  return 0;
}

static int nr_csi_rs_pmi_estimation(const PHY_VARS_NR_UE *ue,
                                    const fapi_nr_dl_config_csirs_pdu_rel15_t *csirs_config_pdu,
                                    const uint8_t N_ports,
                                    const c16_t csi_rs_estimated_channel_freq[][N_ports][ue->frame_parms.ofdm_symbol_size],
                                    const uint32_t interference_plus_noise_power,
                                    const uint8_t rank_indicator,
                                    const int16_t log2_re,
                                    uint8_t *i2,
                                    int32_t *precoded_sinr_dB)
{
  const NR_DL_FRAME_PARMS *frame_parms = &ue->frame_parms;
  const uint32_t ipn = interference_plus_noise_power == 0 ? 1 : interference_plus_noise_power;
  // i1 is a three-element vector in the form of [i11 i12 i13], when CodebookType is specified as 'Type1SinglePanel'.
  // Note that i13 is not applicable when the number of transmission layers is one of {1, 5, 6, 7, 8}.
  // i2, for 'Type1SinglePanel' codebook type, it is a scalar when PMIMode is specified as 'wideband', and when PMIMode
  // is specified as 'subband' or when PRGSize, the length of the i2 vector equals to the number of subbands or PRGs.
  // Note that when the number of CSI-RS ports is 2, the applicable codebook type is 'Type1SinglePanel'. In this case,
  // the precoding matrix is obtained by a single index (i2 field here) based on TS 38.214 Table 5.2.2.2.1-1.
  // The first column is applicable if the UE is reporting a Rank = 1, whereas the second column is applicable if the
  // UE is reporting a Rank = 2.

  if (N_ports == 1) {
    // SISO case: SINR = E[|h|^2] / noise_power. No PMI to estimate.
    int64_t signal_power = 0;
    int count = 0;

    for (int rb = csirs_config_pdu->start_rb; rb < (csirs_config_pdu->start_rb + csirs_config_pdu->nr_of_rbs); rb++) {
      if (csirs_config_pdu->freq_density <= 1 && csirs_config_pdu->freq_density != (rb % 2)) {
        continue;
      }
      uint16_t k = rb * NR_NB_SC_PER_RB;

      const c16_t h = csi_rs_estimated_channel_freq[0][0][k];
      signal_power += (int64_t)h.r * h.r + (int64_t)h.i * h.i;
      count++;
    }

    if (count > 0) {
      const int64_t avg_signal_power = signal_power / count;
      // Non RF devices like ZMQ has virtually zero noise. So here we make noise as 1 to return maximum sinr.
      const uint32_t sinr = avg_signal_power / ipn;
      *precoded_sinr_dB = dB_fixed(sinr);
    }

    return 0;
  }

  if(rank_indicator == 0 || rank_indicator == 1) {
    c64_t sum[4] = {0};
    c64_t sum2[4] = {0};
    int64_t tested_precoded_sinr[4] = {0};

    for (int rb = csirs_config_pdu->start_rb; rb < (csirs_config_pdu->start_rb+csirs_config_pdu->nr_of_rbs); rb++) {

      if (csirs_config_pdu->freq_density <= 1 && csirs_config_pdu->freq_density != (rb % 2)) {
        continue;
      }
      uint16_t k = rb * NR_NB_SC_PER_RB;
      for (int ant_rx = 0; ant_rx < frame_parms->nb_antennas_rx; ant_rx++) {
        const c16_t p0 = csi_rs_estimated_channel_freq[ant_rx][0][k];
        const c16_t p1 = csi_rs_estimated_channel_freq[ant_rx][1][k];

        // H_p0 + 1*H_p1 = (H_p0_re + H_p1_re) + 1j*(H_p0_im + H_p1_im)
        sum[0].r += (p0.r + p1.r);
        sum[0].i += (p0.i + p1.i);
        sum2[0].r += (sum[0].r * sum[0].r) >> log2_re;
        sum2[0].i += (sum[0].i * sum[0].i) >> log2_re;

        // H_p0 + 1j*H_p1 = (H_p0_re - H_p1_im) + 1j*(H_p0_im + H_p1_re)
        sum[1].r += (p0.r - p1.i);
        sum[1].i += (p0.i + p1.r);
        sum2[1].r += (sum[1].r * sum[1].r) >> log2_re;
        sum2[1].i += (sum[1].i * sum[1].i) >> log2_re;

        // H_p0 - 1*H_p1 = (H_p0_re - H_p1_re) + 1j*(H_p0_im - H_p1_im)
        sum[2].r += (p0.r - p1.r);
        sum[2].i += (p0.i - p1.i);
        sum2[2].r += (sum[2].r * sum[2].r) >> log2_re;
        sum2[2].i += (sum[2].i * sum[2].i) >> log2_re;

        // H_p0 - 1j*H_p1 = (H_p0_re + H_p1_im) + 1j*(H_p0_im - H_p1_re)
        sum[3].r += (p0.r + p1.i);
        sum[3].i += (p0.i - p1.r);
        sum2[3].r += (sum[3].r * sum[3].r) >> log2_re;
        sum2[3].i += (sum[3].i * sum[3].i) >> log2_re;
      }
    }

    // We should perform >>nr_csi_info->log2_re here for all terms, but since sum2_re and sum2_im can be high values,
    // we performed this above.
    for(int p = 0; p<4; p++) {
      int64_t power_re = sum2[p].r - (sum[p].r >> log2_re) * (sum[p].r >> log2_re);
      int64_t power_im = sum2[p].i - (sum[p].i >> log2_re) * (sum[p].i >> log2_re);
      tested_precoded_sinr[p] = (power_re + power_im) / ipn;
    }

    if(rank_indicator == 0) {
      for(int tested_i2 = 0; tested_i2 < 4; tested_i2++) {
        if(tested_precoded_sinr[tested_i2] > tested_precoded_sinr[i2[0]]) {
          i2[0] = tested_i2;
        }
      }
      *precoded_sinr_dB = dB_fixed(tested_precoded_sinr[i2[0]]);
    } else {
      i2[0] = tested_precoded_sinr[0]+tested_precoded_sinr[2] > tested_precoded_sinr[1]+tested_precoded_sinr[3] ? 0 : 1;
      *precoded_sinr_dB = dB_fixed((tested_precoded_sinr[i2[0]] + tested_precoded_sinr[i2[0]+2])>>1);
    }

  } else {
    LOG_W(NR_PHY, "PMI computation is not implemented for rank indicator %i\n", rank_indicator+1);
    return -1;
  }

  return 0;
}

int nr_csi_rs_cqi_estimation(const uint32_t precoded_sinr,
                             uint8_t *cqi) {

  *cqi = 0;

  // Default SINR table for an AWGN channel for SISO scenario, considering 0.1 BLER condition and TS 38.214 Table 5.2.2.1-2
  if(precoded_sinr>0 && precoded_sinr<=2) {
    *cqi = 4;
  } else if(precoded_sinr==3) {
    *cqi = 5;
  } else if(precoded_sinr>3 && precoded_sinr<=5) {
    *cqi = 6;
  } else if(precoded_sinr>5 && precoded_sinr<=7) {
    *cqi = 7;
  } else if(precoded_sinr>7 && precoded_sinr<=9) {
    *cqi = 8;
  } else if(precoded_sinr==10) {
    *cqi = 9;
  } else if(precoded_sinr>10 && precoded_sinr<=12) {
    *cqi = 10;
  } else if(precoded_sinr>12 && precoded_sinr<=15) {
    *cqi = 11;
  } else if(precoded_sinr==16) {
    *cqi = 12;
  } else if(precoded_sinr>16 && precoded_sinr<=18) {
    *cqi = 13;
  } else if(precoded_sinr==19) {
    *cqi = 14;
  } else if(precoded_sinr>19) {
    *cqi = 15;
  }

  return 0;
}

static void nr_csi_im_power_estimation(const PHY_VARS_NR_UE *ue,
                                       const fapi_nr_dl_config_csiim_pdu_rel15_t *csiim_config_pdu,
                                       uint32_t *interference_plus_noise_power,
                                       const c16_t rxdataF[][ue->frame_parms.samples_per_slot_wCP])
{
  const NR_DL_FRAME_PARMS *frame_parms = &ue->frame_parms;

  const uint16_t end_rb = csiim_config_pdu->start_rb + csiim_config_pdu->nr_of_rbs > csiim_config_pdu->bwp_size ?
                          csiim_config_pdu->bwp_size : csiim_config_pdu->start_rb + csiim_config_pdu->nr_of_rbs;

  int32_t count = 0;
  int32_t sum_re = 0;
  int32_t sum_im = 0;
  int32_t sum2_re = 0;
  int32_t sum2_im = 0;

  int l_csiim[4] = {-1, -1, -1, -1};

  for(int symb_idx = 0; symb_idx < 4; symb_idx++) {

    uint8_t symb = csiim_config_pdu->l_csiim[symb_idx];
    bool done = false;
    for (int symb_idx2 = 0; symb_idx2 < symb_idx; symb_idx2++) {
      if (l_csiim[symb_idx2] == symb) {
        done = true;
      }
    }

    if (done) {
      continue;
    }

    l_csiim[symb_idx] = symb;
    uint64_t symbol_offset = symb*frame_parms->ofdm_symbol_size;

    for (int ant_rx = 0; ant_rx < frame_parms->nb_antennas_rx; ant_rx++) {

      const c16_t *rx_signal = &rxdataF[ant_rx][symbol_offset];

      for (int rb = csiim_config_pdu->start_rb; rb < end_rb; rb++) {

        uint16_t sc0_offset = (frame_parms->first_carrier_offset + rb*NR_NB_SC_PER_RB) % frame_parms->ofdm_symbol_size;

        for (int sc_idx = 0; sc_idx < 4; sc_idx++) {

          uint16_t sc = sc0_offset + csiim_config_pdu->k_csiim[sc_idx];
          if (sc >= frame_parms->ofdm_symbol_size) {
            sc -= frame_parms->ofdm_symbol_size;
          }

#ifdef NR_CSIIM_DEBUG
          LOG_I(NR_PHY, "(ant_rx %i, sc %i) real %i, imag %i\n", ant_rx, sc, rx_signal[sc].r, rx_signal[sc].i);
#endif

          if (sc == 0) // skip DC for noise power estimation
            continue;
          sum_re += rx_signal[sc].r;
          sum_im += rx_signal[sc].i;
          sum2_re += rx_signal[sc].r * rx_signal[sc].r;
          sum2_im += rx_signal[sc].i * rx_signal[sc].i;
          count++;
        }
      }
    }
  }

  int32_t power_re = sum2_re / count - (sum_re / count) * (sum_re / count);
  int32_t power_im = sum2_im / count - (sum_im / count) * (sum_im / count);

  *interference_plus_noise_power = power_re + power_im;

#ifdef NR_CSIIM_DEBUG
  LOG_I(NR_PHY, "interference_plus_noise_power based on CSI-IM = %i\n", *interference_plus_noise_power);
#endif
}

static double get_cfo(const double phase_diff, const int sym0, const int sym1, const NR_DL_FRAME_PARMS *fp)
{
  const double one_fs = 1.0 / (fp->samples_per_frame * 100);
  int sample_count = 0;
  for (int s = sym0 + 1; s <= sym1; s++) {
    const int prefix = (s % (7 * (1 << fp->numerology_index))) ? fp->nb_prefix_samples : fp->nb_prefix_samples0;
    sample_count += (fp->ofdm_symbol_size + prefix);
  }
  const double delta_time = sample_count * one_fs;
  const double cfo = phase_diff / (2 * M_PI * delta_time);
  return cfo;
}

static void nr_ue_trs_processing(PHY_VARS_NR_UE *ue,
                                 const c16_t res0_est[][1][ue->frame_parms.ofdm_symbol_size],
                                 const c16_t res1_est[][1][ue->frame_parms.ofdm_symbol_size],
                                 const c16_t freq_interp_est[][1][ue->frame_parms.ofdm_symbol_size],
                                 const fapi_nr_dl_config_csirs_pdu_rel15_t *csirs_config_pdu,
                                 const csi_mapping_parms_t *csi_mapping,
                                 const int sym0,
                                 const int sym1,
                                 int *cfo,
                                 int *time_offset)
{
  AssertFatal((sym0 > -1) && (sym1 > -1) && (sym1 > sym0), "Invalid symbol index for TRS estimation\n");
  const NR_DL_FRAME_PARMS *fp = &ue->frame_parms;
  // CFO estimation
  const int ant_rx = 0; // Estimate only on first antenna port
  cd_t phase_diff = {0.0};
  AssertFatal(csirs_config_pdu->freq_density == 3,
              "CSI-RS for tracking must have freq density of 3 but has %d\n",
              csirs_config_pdu->freq_density);
  AssertFatal(csi_mapping->kprime == 0 && csi_mapping->lprime == 0, "Invalid kprime, lprime for CSI-RS for tracking (row 1)\n");
  for (int rb = csirs_config_pdu->start_rb; rb < (csirs_config_pdu->start_rb + csirs_config_pdu->nr_of_rbs); rb++) {
    for (int cdm_id = 0; cdm_id < csi_mapping->size; cdm_id++) {
      uint16_t kinit = rb * NR_NB_SC_PER_RB;
      uint16_t k = kinit + csi_mapping->koverline[cdm_id];

      const c16_t *res0 = res0_est[ant_rx][0];
      const c16_t *res1 = res1_est[ant_rx][0];
      phase_diff.r += res1[k].r * res0[k].r + res1[k].i * res0[k].i;
      phase_diff.i += res1[k].i * res0[k].r - res1[k].r * res0[k].i;
    }
  }
  *cfo = (int)get_cfo(atan2(phase_diff.i, phase_diff.r), sym0, sym1, fp);

  if (time_offset) {
    // Time offset estimation
    __attribute__((aligned(32))) c16_t time_est[fp->ofdm_symbol_size];
    delay_t delay = {0};
    nr_est_delay(fp->ofdm_symbol_size, freq_interp_est[ant_rx][0], time_est, &delay);
    *time_offset = delay.delay_max_pos;
  }
}

void nr_ue_csi_im_procedures(PHY_VARS_NR_UE *ue,
                             const c16_t rxdataF[][ue->frame_parms.samples_per_slot_wCP],
                             const fapi_nr_dl_config_csiim_pdu_rel15_t *csiim_config_pdu)
{

#ifdef NR_CSIIM_DEBUG
  LOG_I(NR_PHY, "csiim_config_pdu->bwp_size = %i\n", csiim_config_pdu->bwp_size);
  LOG_I(NR_PHY, "csiim_config_pdu->bwp_start = %i\n", csiim_config_pdu->bwp_start);
  LOG_I(NR_PHY, "csiim_config_pdu->subcarrier_spacing = %i\n", csiim_config_pdu->subcarrier_spacing);
  LOG_I(NR_PHY, "csiim_config_pdu->start_rb = %i\n", csiim_config_pdu->start_rb);
  LOG_I(NR_PHY, "csiim_config_pdu->nr_of_rbs = %i\n", csiim_config_pdu->nr_of_rbs);
  LOG_I(NR_PHY, "csiim_config_pdu->k_csiim = %i.%i.%i.%i\n", csiim_config_pdu->k_csiim[0], csiim_config_pdu->k_csiim[1], csiim_config_pdu->k_csiim[2], csiim_config_pdu->k_csiim[3]);
  LOG_I(NR_PHY, "csiim_config_pdu->l_csiim = %i.%i.%i.%i\n", csiim_config_pdu->l_csiim[0], csiim_config_pdu->l_csiim[1], csiim_config_pdu->l_csiim[2], csiim_config_pdu->l_csiim[3]);
#endif

  nr_csi_im_power_estimation(ue, csiim_config_pdu, &ue->nr_csi_info->interference_plus_noise_power, rxdataF);
  ue->nr_csi_info->csi_im_meas_computed = true;
}

void nr_ue_csi_rs_procedures(PHY_VARS_NR_UE *ue,
                             const UE_nr_rxtx_proc_t *proc,
                             const c16_t rxdataF[][ue->frame_parms.samples_per_slot_wCP],
                             fapi_nr_dl_config_csirs_pdu_rel15_t *csirs_config_pdu,
                             c16_t trs_estimates[][1][ue->frame_parms.ofdm_symbol_size],
                             const int res_idx,
                             const int trs_sym0)
{

#ifdef NR_CSIRS_DEBUG
  LOG_I(NR_PHY, "csirs_config_pdu->subcarrier_spacing = %i\n", csirs_config_pdu->subcarrier_spacing);
  LOG_I(NR_PHY, "csirs_config_pdu->cyclic_prefix = %i\n", csirs_config_pdu->cyclic_prefix);
  LOG_I(NR_PHY, "csirs_config_pdu->start_rb = %i\n", csirs_config_pdu->start_rb);
  LOG_I(NR_PHY, "csirs_config_pdu->nr_of_rbs = %i\n", csirs_config_pdu->nr_of_rbs);
  LOG_I(NR_PHY, "csirs_config_pdu->csi_type = %i (0:TRS, 1:CSI-RS NZP, 2:CSI-RS ZP)\n", csirs_config_pdu->csi_type);
  LOG_I(NR_PHY, "csirs_config_pdu->row = %i\n", csirs_config_pdu->row);
  LOG_I(NR_PHY, "csirs_config_pdu->freq_domain = %i\n", csirs_config_pdu->freq_domain);
  LOG_I(NR_PHY, "csirs_config_pdu->symb_l0 = %i\n", csirs_config_pdu->symb_l0);
  LOG_I(NR_PHY, "csirs_config_pdu->symb_l1 = %i\n", csirs_config_pdu->symb_l1);
  LOG_I(NR_PHY, "csirs_config_pdu->cdm_type = %i\n", csirs_config_pdu->cdm_type);
  LOG_I(NR_PHY, "csirs_config_pdu->freq_density = %i (0: dot5 (even RB), 1: dot5 (odd RB), 2: one, 3: three)\n", csirs_config_pdu->freq_density);
  LOG_I(NR_PHY, "csirs_config_pdu->scramb_id = %i\n", csirs_config_pdu->scramb_id);
  LOG_I(NR_PHY, "csirs_config_pdu->power_control_offset = %i\n", csirs_config_pdu->power_control_offset);
  LOG_I(NR_PHY, "csirs_config_pdu->power_control_offset_ss = %i\n", csirs_config_pdu->power_control_offset_ss);
#endif

  if(csirs_config_pdu->csi_type == 2) {
    LOG_E(NR_PHY, "Handling of ZP CSI-RS not handled yet at PHY\n");
    return;
  }

  const NR_DL_FRAME_PARMS *frame_parms = &ue->frame_parms;
  csi_mapping_parms_t mapping_parms = get_csi_mapping_parms(csirs_config_pdu->row,
                                                            csirs_config_pdu->freq_domain,
                                                            csirs_config_pdu->symb_l0,
                                                            csirs_config_pdu->symb_l1);
  nr_csi_info_t *csi_info = ue->nr_csi_info;
  nr_generate_csi_rs(frame_parms,
                     &mapping_parms,
                     AMP,
                     proc->nr_slot_rx,
                     csirs_config_pdu->freq_density,
                     csirs_config_pdu->start_rb,
                     csirs_config_pdu->nr_of_rbs,
                     csirs_config_pdu->symb_l0,
                     csirs_config_pdu->symb_l1,
                     csirs_config_pdu->row,
                     csirs_config_pdu->scramb_id,
                     csirs_config_pdu->power_control_offset_ss,
                     csirs_config_pdu->cdm_type,
                     csi_info->csi_rs_generated_signal);

  csi_info->csi_rs_generated_signal_bits = log2_approx(AMP);

  /* OFDM symbol size \times sizeof(c16_t) is always multiple of 64. Since the
  estimates have first RB at start of buffer we don't need padding for 32 or 64
  byte alignment. */
  c16_t csi_rs_ls_estimated_channel[frame_parms->nb_antennas_rx][mapping_parms.ports][frame_parms->ofdm_symbol_size];
  c16_t csi_rs_estimated_channel_freq[frame_parms->nb_antennas_rx][mapping_parms.ports][frame_parms->ofdm_symbol_size];

  int CDM_group_size = get_cdm_group_size(csirs_config_pdu->cdm_type);
  c16_t csi_rs_received_signal[frame_parms->nb_antennas_rx][frame_parms->samples_per_slot_wCP];
  uint32_t rsrp = 0;
  int rsrp_dBm = 0;
  nr_get_csi_rs_signal(ue,
                       proc,
                       csirs_config_pdu,
                       csi_info,
                       &mapping_parms,
                       CDM_group_size,
                       csi_rs_received_signal,
                       &rsrp,
                       &rsrp_dBm,
                       rxdataF);

  uint32_t noise_power = 0;
  int16_t log2_re = 0;
  int16_t log2_maxh = 0;
  // if we need to measure only RSRP no need to do channel estimation
  if (csirs_config_pdu->measurement_bitmap != 1) {
    const bool use_trs_buff = (csirs_config_pdu->csi_type == 0 && res_idx == 0);
    nr_csi_rs_channel_estimation(frame_parms,
                                csirs_config_pdu,
                                csi_info,
                                (const c16_t **)csi_info->csi_rs_generated_signal,
                                csi_rs_received_signal,
                                &mapping_parms,
                                CDM_group_size,
                                (use_trs_buff) ? trs_estimates : csi_rs_ls_estimated_channel,
                                csi_rs_estimated_channel_freq,
                                &log2_re,
                                &log2_maxh,
                                &noise_power);
  }

  uint8_t rank_indicator = 0;
  // bit 1 in bitmap to indicate RI measurment
  if (csirs_config_pdu->measurement_bitmap & 2) {
    nr_csi_rs_ri_estimation(ue,
                            csirs_config_pdu,
                            mapping_parms.ports,
                            csi_rs_estimated_channel_freq,
                            log2_maxh,
                            &rank_indicator);
  }

  uint8_t i1[3] = {0};
  uint8_t i2[1] = {0};
  uint8_t cqi = 0;
  int32_t precoded_sinr_dB = 0;
  // bit 3 in bitmap to indicate RI measurment
  if (csirs_config_pdu->measurement_bitmap & 8) {
    nr_csi_rs_pmi_estimation(ue,
                             csirs_config_pdu,
                             mapping_parms.ports,
                             csi_rs_estimated_channel_freq,
                             csi_info->csi_im_meas_computed ? csi_info->interference_plus_noise_power : noise_power,
                             rank_indicator,
                             log2_re,
                             i2,
                             &precoded_sinr_dB);

    // bit 4 in bitmap to indicate RI measurment
    if(csirs_config_pdu->measurement_bitmap & 16)
      nr_csi_rs_cqi_estimation(precoded_sinr_dB, &cqi);
  }

  int trs_cfo = 0;
  const bool do_trs_est = (csirs_config_pdu->csi_type == 0) && (res_idx == 1);
  if (do_trs_est) {
    start_meas_nr_ue_phy(ue, TRS_PROCESSING);
    nr_ue_trs_processing(ue,
                         trs_estimates,
                         csi_rs_ls_estimated_channel,
                         csi_rs_estimated_channel_freq,
                         csirs_config_pdu,
                         &mapping_parms,
                         trs_sym0,
                         csirs_config_pdu->symb_l0,
                         &trs_cfo,
                         NULL); // Time offset not estimated because it is corrected using PBCH DMRS
    stop_meas_nr_ue_phy(ue, TRS_PROCESSING);
  }

  switch (csirs_config_pdu->measurement_bitmap) {
    case 0:
      if (do_trs_est)
        LOG_I(NR_PHY,
              "%d.%d TRS estimated CFO: %d Hz\n",
              proc->frame_rx,
              proc->nr_slot_rx,
              trs_cfo);
      break;
    case 1:
      LOG_I(NR_PHY, "%d.%d [UE %d] RSRP = %i dBm\n", proc->frame_rx, proc->nr_slot_rx, ue->Mod_id, rsrp_dBm);
      break;
    case 26 :
      LOG_I(NR_PHY, "RI = %i i1 = %i.%i.%i, i2 = %i, SINR = %i dB, CQI = %i\n",
            rank_indicator + 1, i1[0], i1[1], i1[2], i2[0], precoded_sinr_dB, cqi);
      break;
    case 27 :
      LOG_I(NR_PHY, "RSRP = %i dBm, RI = %i i1 = %i.%i.%i, i2 = %i, SINR = %i dB, CQI = %i\n",
            rsrp_dBm, rank_indicator + 1, i1[0], i1[1], i1[2], i2[0], precoded_sinr_dB, cqi);
      break;
    default :
      AssertFatal(false, "Not supported measurement configuration\n");
  }

  if (!ue->cont_fo_comp && do_trs_est && csirs_config_pdu->last_trs_slot) {
    trs_freq_correction(ue, trs_cfo);
  }

  // Send CSI measurements to MAC
  if (!ue->if_inst || !ue->if_inst->dl_indication)
    return;

  fapi_nr_l1_measurements_t l1_measurements = {
      .gNB_index = proc->gNB_id,
      .meas_type = NFAPI_NR_CSI_MEAS,
      .Nid_cell = frame_parms->Nid_cell,
      .is_neighboring_cell = false,
      .rsrp_dBm = rsrp_dBm,
      .rank_indicator = rank_indicator,
      .i1 = *i1,
      .i2 = *i2,
      .cqi = cqi,
      .radiolink_monitoring = RLM_no_monitoring, // TODO do be activated in case of RLM based on CSI-RS
  };
  fapi_nr_rx_indication_t rx_ind;
  rx_ind.number_pdus = 0;
  nr_fill_rx_indication(&rx_ind, FAPI_NR_MEAS_IND, ue, 0, 0, NULL, proc, (void *)&l1_measurements);
  nr_downlink_indication_t dl_indication = (nr_downlink_indication_t){
      .gNB_index = proc->gNB_id,
      .module_id = ue->Mod_id,
      .cc_id = ue->CC_id,
      .hfn = proc->hfn_rx,
      .frame = proc->frame_rx,
      .slot = proc->nr_slot_rx,
      .rx_ind = &rx_ind,
  };
  ue->if_inst->dl_indication(&dl_indication);
}
