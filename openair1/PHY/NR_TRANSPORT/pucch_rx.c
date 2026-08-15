/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Top-level routines for decoding the PUCCH physical channel
 */
#include <stdio.h>
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
#include "PHY/CODING/nrSmallBlock/nr_small_block_defs.h"
#include "PHY/NR_TRANSPORT/nr_transport_common_proto.h"
#include "PHY/NR_TRANSPORT/nr_transport_proto.h"
#include "PHY/NR_REFSIG/nr_refsig.h"
#include "common/utils/LOG/log.h"
#include "nfapi/oai_integration/vendor_ext.h"
#include "nfapi/oai_integration/vendor_ext.h"
#include "SCHED_NR/sched_nr.h"
#include "bits.h"

#include "T.h"
#include "nr_phy_common.h"

//#define DEBUG_NR_PUCCH_RX 1
void nr_fill_pucch(PHY_VARS_gNB *gNB, int frame, int slot, nfapi_nr_pucch_pdu_t *pucch_pdu)
{
  LOG_D(PHY,
        "%4d.%2d UE %04x Programming PUCCH format %d, nb_harq %d, nb_sr %d, nb_csi %d\n",
        frame,
        slot,
        pucch_pdu->rnti,
        pucch_pdu->format_type,
        pucch_pdu->bit_len_harq,
        pucch_pdu->sr_flag,
        pucch_pdu->bit_len_csi_part1);
  NR_gNB_PUCCH_job_t pucch = {.frame = frame, .slot = slot, .pucch_pdu = *pucch_pdu};
  if (gNB->common_vars.beam_id) {
    int fapi_beam_idx = pucch_pdu->beamforming.prgs_list[0].dig_bf_interface_list[0].beam_idx;
    int bitmap = SL_to_bitmap(pucch_pdu->start_symbol_index, pucch_pdu->nr_of_symbols);
    const nfapi_nr_spatial_stream_index_t *p = &pucch_pdu->param_v4;
    const uint16_t ant_port = p->numSpatialStreamIndices > 0 ? p->spatialStreamIndices[0] : 0;
    beam_index_allocation(fapi_beam_idx, ant_port, 1, NR_SYMBOLS_PER_SLOT, slot, bitmap, gNB->frame_parms.nb_antennas_rx, gNB->common_vars.beam_id);
  }
  bool found = spsc_q_put(&gNB->pucch_queue, &pucch, sizeof(pucch));
  if (!found)
    LOG_W(NR_PHY, "PUCCH list is full: dropping PUCCH UE %04x\n", pucch_pdu->rnti);
}

int get_pucch0_cs_lut_index(PHY_VARS_gNB *gNB, const nfapi_nr_pucch_pdu_t *pucch_pdu)
{
  int i = 0;

#ifdef DEBUG_NR_PUCCH_RX
  printf("getting index for LUT with %d entries, Nid %d\n", gNB->pucch0_lut.nb_id, pucch_pdu->hopping_id);
#endif

  for (i = 0; i < gNB->pucch0_lut.nb_id; i++) {
    if (gNB->pucch0_lut.Nid[i] == pucch_pdu->hopping_id)
      break;
  }
#ifdef DEBUG_NR_PUCCH_RX
  printf("found index %d\n", i);
#endif
  if (i < gNB->pucch0_lut.nb_id)
    return (i);

#ifdef DEBUG_NR_PUCCH_RX
  printf("Initializing PUCCH0 LUT index %i with Nid %d\n", i, pucch_pdu->hopping_id);
#endif
  // initialize
  gNB->pucch0_lut.Nid[gNB->pucch0_lut.nb_id] = pucch_pdu->hopping_id;
  for (int slot = 0; slot < 10 << pucch_pdu->subcarrier_spacing; slot++)
    for (int symbol = 0; symbol < 14; symbol++)
      gNB->pucch0_lut.lut[gNB->pucch0_lut.nb_id][slot][symbol] =
          (int)floor(nr_cyclic_shift_hopping(pucch_pdu->hopping_id, 0, 0, symbol, 0, slot) / 0.5235987756);
  gNB->pucch0_lut.nb_id++;
  return (gNB->pucch0_lut.nb_id - 1);
}

static const int16_t idft12_re[12][12] = {
    {23170, 23170, 23170, 23170, 23170, 23170, 23170, 23170, 23170, 23170, 23170, 23170},
    {23170, 20066, 11585, 0, -11585, -20066, -23170, -20066, -11585, 0, 11585, 20066},
    {23170, 11585, -11585, -23170, -11585, 11585, 23170, 11585, -11585, -23170, -11585, 11585},
    {23170, 0, -23170, 0, 23170, 0, -23170, 0, 23170, 0, -23170, 0},
    {23170, -11585, -11585, 23170, -11585, -11585, 23170, -11585, -11585, 23170, -11585, -11585},
    {23170, -20066, 11585, 0, -11585, 20066, -23170, 20066, -11585, 0, 11585, -20066},
    {23170, -23170, 23170, -23170, 23170, -23170, 23170, -23170, 23170, -23170, 23170, -23170},
    {23170, -20066, 11585, 0, -11585, 20066, -23170, 20066, -11585, 0, 11585, -20066},
    {23170, -11585, -11585, 23170, -11585, -11585, 23170, -11585, -11585, 23170, -11585, -11585},
    {23170, 0, -23170, 0, 23170, 0, -23170, 0, 23170, 0, -23170, 0},
    {23170, 11585, -11585, -23170, -11585, 11585, 23170, 11585, -11585, -23170, -11585, 11585},
    {23170, 20066, 11585, 0, -11585, -20066, -23170, -20066, -11585, 0, 11585, 20066}};

static const int16_t idft12_im[12][12] = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                          {0, 11585, 20066, 23170, 20066, 11585, 0, -11585, -20066, -23170, -20066, -11585},
                                          {0, 20066, 20066, 0, -20066, -20066, 0, 20066, 20066, 0, -20066, -20066},
                                          {0, 23170, 0, -23170, 0, 23170, 0, -23170, 0, 23170, 0, -23170},
                                          {0, 20066, -20066, 0, 20066, -20066, 0, 20066, -20066, 0, 20066, -20066},
                                          {0, 11585, -20066, 23170, -20066, 11585, 0, -11585, 20066, -23170, 20066, -11585},
                                          {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                          {0, -11585, 20066, -23170, 20066, -11585, 0, 11585, -20066, 23170, -20066, 11585},
                                          {0, -20066, 20066, 0, -20066, 20066, 0, -20066, 20066, 0, -20066, 20066},
                                          {0, -23170, 0, 23170, 0, -23170, 0, 23170, 0, -23170, 0, 23170},
                                          {0, -20066, -20066, 0, 20066, 20066, 0, -20066, -20066, 0, 20066, 20066},
                                          {0, -11585, -20066, -23170, -20066, -11585, 0, 11585, 20066, 23170, 20066, 11585}};
//************************************************************************//
void nr_decode_pucch0(PHY_VARS_gNB *gNB,
                      c16_t **rxdataF,
                      int frame,
                      int slot,
                      nfapi_nr_uci_pucch_pdu_format_0_1_t *uci_pdu,
                      const nfapi_nr_pucch_pdu_t *pucch_pdu)
{
  NR_DL_FRAME_PARMS *frame_parms = &gNB->frame_parms;
  int soffset = (slot % RU_RX_SLOT_DEPTH) * frame_parms->symbols_per_slot * frame_parms->ofdm_symbol_size;

  AssertFatal(pucch_pdu->bit_len_harq > 0 || pucch_pdu->sr_flag > 0,
              "Either bit_len_harq (%d) or sr_flag (%d) must be > 0\n",
              pucch_pdu->bit_len_harq,
              pucch_pdu->sr_flag);

  /* it might be that the stats list is full: In this case, we will simply
   * write to some memory on the stack instead of the UE's UCI stats */
  NR_gNB_UCI_STATS_t stack_uci_stats = {0};
  NR_gNB_UCI_STATS_t *uci_stats = &stack_uci_stats;
  NR_gNB_PHY_STATS_t *phy_stats = get_phy_stats(gNB, pucch_pdu->rnti);
  if (phy_stats != NULL) {
    phy_stats->frame = frame;
    uci_stats = &phy_stats->uci_stats;
  }

  int nr_sequences;
  const uint8_t *mcs;
  if (pucch_pdu->bit_len_harq == 0) {
    mcs = table1_mcs;
    nr_sequences = 1;
  } else if (pucch_pdu->bit_len_harq == 1) {
    mcs = table1_mcs;
    nr_sequences = 4 >> (1 - pucch_pdu->sr_flag);
  } else {
    mcs = table2_mcs;
    nr_sequences = 8 >> (1 - pucch_pdu->sr_flag);
  }

  LOG_D(PHY,
        "pucch0: nr_symbols %d, start_symbol %d, prb_start %d, second_hop_prb %d,  group_hop_flag %d, sequence_hop_flag %d, O_ACK "
        "%d, O_SR %d, mcs %d initial_cyclic_shift %d\n",
        pucch_pdu->nr_of_symbols,
        pucch_pdu->start_symbol_index,
        pucch_pdu->prb_start,
        pucch_pdu->second_hop_prb,
        pucch_pdu->group_hop_flag,
        pucch_pdu->sequence_hop_flag,
        pucch_pdu->bit_len_harq,
        pucch_pdu->sr_flag,
        mcs[0],
        pucch_pdu->initial_cyclic_shift);

  int cs_ind = get_pucch0_cs_lut_index(gNB, pucch_pdu);
  /*
   * Implement TS 38.211 Subclause 6.3.2.3.1 Sequence generation
   * Defining cyclic shift hopping TS 38.211 Subclause 6.3.2.2.2
   * in TS 38.213 Subclause 9.2.1 it is said that:
   * for PUCCH format 0 or PUCCH format 1, the index of the cyclic shift
   * is indicated by higher layer parameter PUCCH-F0-F1-initial-cyclic-shift
   */
  int prb_offset[2] = {pucch_pdu->bwp_start + pucch_pdu->prb_start, pucch_pdu->bwp_start + pucch_pdu->prb_start};

  pucch_GroupHopping_t pucch_GroupHopping = pucch_pdu->group_hop_flag + (pucch_pdu->sequence_hop_flag << 1);
  // the value of u,v (delta always 0 for PUCCH) has to be calculated according
  // to TS 38.211 Subclause 6.3.2.2.1
  uint8_t u[2] = {0}, v[2] = {0};
  nr_group_sequence_hopping(pucch_GroupHopping, pucch_pdu->hopping_id, 0, slot, u,
                            v); // calculating u and v value first hop
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

  const uint8_t num_sp_streams = pucch_pdu->param_v4.numSpatialStreamIndices;

  c64_t xr[num_sp_streams][pucch_pdu->nr_of_symbols][12] __attribute__((aligned(32)));
  memset(xr, 0, sizeof(xr));

  int64_t xrtmag = 0, xrtmag_next = 0;
  uint8_t maxpos = 0;
  uint8_t index = 0;

  int nb_re_pucch = 12 * pucch_pdu->prb_size; // prb size is 1
  int64_t signal_energy = 0, signal_energy_ant0 = 0;

  for (int l = 0; l < pucch_pdu->nr_of_symbols; l++) {
    uint8_t l2 = l + pucch_pdu->start_symbol_index;

    re_offset[l] = (12 * prb_offset[l]) + frame_parms->first_carrier_offset;
    if (re_offset[l] >= frame_parms->ofdm_symbol_size)
      re_offset[l] -= frame_parms->ofdm_symbol_size;

    for (int aa = 0; aa < num_sp_streams; aa++) {
      c16_t rp[nb_re_pucch];
      memset(rp, 0, sizeof(rp));
      c16_t *tmp_rp = &rxdataF[aa][soffset + l2 * frame_parms->ofdm_symbol_size];
      if (re_offset[l] + nb_re_pucch > frame_parms->ofdm_symbol_size) {
        int neg_length = frame_parms->ofdm_symbol_size - re_offset[l];
        int pos_length = nb_re_pucch - neg_length;
        memcpy(rp, &tmp_rp[re_offset[l]], neg_length * sizeof(*tmp_rp));
        memcpy(&rp[neg_length], tmp_rp, pos_length * sizeof(*tmp_rp));
      } else
        memcpy(rp, &tmp_rp[re_offset[l]], nb_re_pucch * sizeof(*tmp_rp));

      for (int n = 0; n < nb_re_pucch; n++) {
        xr[aa][l][n].r = (int32_t)x_re[l][n] * rp[n].r + (int32_t)x_im[l][n] * rp[n].i;
        xr[aa][l][n].i = (int32_t)x_re[l][n] * rp[n].i - (int32_t)x_im[l][n] * rp[n].r;
#ifdef DEBUG_NR_PUCCH_RX
        printf("x (%d,%d), xr (%ld,%ld)\n", x_re[l][n], x_im[l][n], xr[aa][l][n].r, xr[aa][l][n].i);
#endif
      }
      int energ = signal_energy_nodc(rp, nb_re_pucch);
      signal_energy += energ;
      if (aa == 0)
        signal_energy_ant0 += energ;
    }
  }
  signal_energy /= (pucch_pdu->nr_of_symbols * num_sp_streams);
  signal_energy_ant0 /= pucch_pdu->nr_of_symbols;
  int pucch_power_dBtimes10 = 10 * dB_fixed(signal_energy);

  // int32_t no_corr = 0;
  int seq_index = 0;

  for (int i = 0; i < nr_sequences; i++) {
    c64_t corr[num_sp_streams][2];
    for (int aa = 0; aa < num_sp_streams; aa++) {
      for (int l = 0; l < pucch_pdu->nr_of_symbols; l++) {
        seq_index =
            (pucch_pdu->initial_cyclic_shift + mcs[i] + gNB->pucch0_lut.lut[cs_ind][slot][l + pucch_pdu->start_symbol_index]) % 12;
#ifdef DEBUG_NR_PUCCH_RX
        printf("PUCCH symbol %d seq %d, seq_index %d, mcs %d\n", l, i, seq_index, mcs[i]);
#endif
        corr[aa][l] = (c64_t){0};
        for (int n = 0; n < 12; n++) {
          corr[aa][l].r += xr[aa][l][n].r * idft12_re[seq_index][n] + xr[aa][l][n].i * idft12_im[seq_index][n];
          corr[aa][l].i += xr[aa][l][n].r * idft12_im[seq_index][n] - xr[aa][l][n].i * idft12_re[seq_index][n];
        }
        corr[aa][l].r >>= 31;
        corr[aa][l].i >>= 31;
      }
    }
    LOG_D(PHY,
          "PUCCH IDFT[%d/%d] = (%ld,%ld)=>%f\n",
          mcs[i],
          seq_index,
          corr[0][0].r,
          corr[0][0].i,
          10 * log10((double)squaredMod(corr[0][0])));
    if (pucch_pdu->nr_of_symbols == 2)
      LOG_D(PHY,
            "PUCCH 2nd symbol IDFT[%d/%d] = (%ld,%ld)=>%f\n",
            mcs[i],
            seq_index,
            corr[0][1].r,
            corr[0][1].i,
            10 * log10((double)squaredMod(corr[0][1])));
    int64_t temp = 0;
    if (pucch_pdu->freq_hop_flag == 0) {
      if (pucch_pdu->nr_of_symbols == 1) { // non-coherent correlation
        for (int aa = 0; aa < num_sp_streams; aa++)
          temp += squaredMod(corr[aa][0]);
      } else {
        for (int aa = 0; aa < num_sp_streams; aa++) {
          c64_t corr2;
          csum(corr2, corr[aa][0], corr[aa][1]);
          // coherent combining of 2 symbols and then complex modulus for
          // single-frequency case
          temp += corr2.r * corr2.r + corr2.i * corr2.i;
        }
      }
    } else {
      // full non-coherent combining of 2 symbols for frequency-hopping case
      for (int aa = 0; aa < num_sp_streams; aa++)
        temp += squaredMod(corr[aa][0]) + squaredMod(corr[aa][1]);
    }

    if (temp > xrtmag) {
      xrtmag_next = xrtmag;
      xrtmag = temp;
      LOG_D(PHY, "Sequence %d xrtmag %ld xrtmag_next %ld\n", i, xrtmag, xrtmag_next);
      maxpos = i;
      uci_stats->current_pucch0_stat0 = 0;
      int64_t temp2 = 0, temp3 = 0;
      for (int aa = 0; aa < num_sp_streams; aa++) {
        temp2 += squaredMod(corr[aa][0]);
        if (pucch_pdu->nr_of_symbols == 2)
          temp3 += squaredMod(corr[aa][1]);
      }
      uci_stats->current_pucch0_stat0 = dB_fixed64(temp2);
      if (pucch_pdu->nr_of_symbols == 2)
        uci_stats->current_pucch0_stat1 = dB_fixed64(temp3);
    } else if (temp > xrtmag_next)
      xrtmag_next = temp;
  }

  int xrtmag_dBtimes10 = 10 * (int)dB_fixed64(xrtmag / (12 * pucch_pdu->nr_of_symbols));
  int xrtmag_next_dBtimes10 = 10 * (int)dB_fixed64(xrtmag_next / (12 * pucch_pdu->nr_of_symbols));
#ifdef DEBUG_NR_PUCCH_RX
  printf("PUCCH 0 : maxpos %d\n", maxpos);
#endif

  index = maxpos;
  int pucch0_n00 = gNB->measurements.n0_subband_power_tot_dB[prb_offset[0]];
  int pucch0_n01 = gNB->measurements.n0_subband_power_tot_dB[prb_offset[1]];
  LOG_D(PHY, "n00[%d] = %d, n01[%d] = %d\n", prb_offset[0], pucch0_n00, prb_offset[1], pucch0_n01);

  uci_stats->pucch0_n00 = pucch0_n00;
  uci_stats->pucch0_n01 = pucch0_n01;
  uci_stats->pucch0_thres = gNB->pucch0_thres;

  // estimate CQI for MAC (from antenna port 0 only)
  int max_n0 =
      max(gNB->measurements.n0_subband_power_tot_dB[prb_offset[0]], gNB->measurements.n0_subband_power_tot_dB[prb_offset[1]]);
  const int SNRtimes10 = pucch_power_dBtimes10 - (10 * max_n0);
  int cqi;
  if (SNRtimes10 < -640)
    cqi = 0;
  else if (SNRtimes10 > 635)
    cqi = 255;
  else
    cqi = (640 + SNRtimes10) / 5;

  bool no_conf = false;
  if (nr_sequences > 1) {
    if (/*xrtmag_dBtimes10 < (30+xrtmag_next_dBtimes10) ||*/ SNRtimes10 < gNB->pucch0_thres) {
      no_conf = true;
      LOG_D(PHY,
            "%d.%d PUCCH bad confidence: %d threshold, %d, %d, %d\n",
            frame,
            slot,
            gNB->pucch0_thres,
            SNRtimes10,
            xrtmag_dBtimes10,
            xrtmag_next_dBtimes10);
    }
  }
  gNB->bad_pucch += no_conf;
  // first bit of bitmap for sr presence and second bit for acknack presence
  uci_pdu->pduBitmap = pucch_pdu->sr_flag | ((pucch_pdu->bit_len_harq > 0) << 1);
  uci_pdu->pucch_format = 0; // format 0
  uci_pdu->rnti = pucch_pdu->rnti;
  uci_pdu->ul_cqi = cqi;
  uci_pdu->timing_advance = 0xffff; // currently not valid
  uci_pdu->rssi = 1280 - (10 * dB_fixed(32767 * 32767) - dB_fixed_times10(signal_energy));

  if (pucch_pdu->bit_len_harq == 0) {
    uci_pdu->sr.sr_confidence_level = SNRtimes10 < gNB->pucch0_thres;
    uci_stats->pucch0_sr_trials++;
    if (xrtmag_dBtimes10 > (10 * max_n0 + 100)) {
      uci_pdu->sr.sr_indication = 1;
      uci_stats->pucch0_positive_SR++;
      LOG_D(PHY, "PUCCH0 got positive SR. Cumulative number of positive SR %d\n", uci_stats->pucch0_positive_SR);
    } else {
      uci_pdu->sr.sr_indication = 0;
    }
  } else if (pucch_pdu->bit_len_harq == 1) {
    uci_pdu->harq.num_harq = 1;
    uci_pdu->harq.harq_confidence_level = no_conf;
    uci_pdu->harq.harq_list[0].harq_value = !(index & 0x01);
    LOG_D(PHY,
          "[DLSCH/PDSCH/PUCCH] %d.%d HARQ %s with confidence level %s xrt_mag "
          "%d xrt_mag_next %d pucch_power_dBtimes10 %d n0 %d "
          "(%d,%d) pucch0_thres %d, "
          "cqi %d, SNRtimes10 %d, energy %f\n",
          frame,
          slot,
          uci_pdu->harq.harq_list[0].harq_value == 0 ? "ACK" : "NACK",
          uci_pdu->harq.harq_confidence_level == 0 ? "good" : "bad",
          xrtmag_dBtimes10,
          xrtmag_next_dBtimes10,
          pucch_power_dBtimes10,
          max_n0,
          pucch0_n00,
          pucch0_n01,
          gNB->pucch0_thres,
          cqi,
          SNRtimes10,
          10 * log10((double)signal_energy_ant0));

    if (pucch_pdu->sr_flag == 1) {
      uci_pdu->sr.sr_indication = (index > 1);
      uci_pdu->sr.sr_confidence_level = no_conf;
      if (uci_pdu->sr.sr_indication == 1 && uci_pdu->sr.sr_confidence_level == 0) {
        uci_stats->pucch0_positive_SR++;
        LOG_D(PHY, "PUCCH0 got positive SR. Cumulative number of positive SR %d\n", uci_stats->pucch0_positive_SR);
      }
    }
    uci_stats->pucch01_trials++;
  } else {
    uci_pdu->harq.num_harq = 2;
    uci_pdu->harq.harq_confidence_level = no_conf;

    uci_pdu->harq.harq_list[1].harq_value = !(index & 0x01);
    uci_pdu->harq.harq_list[0].harq_value = !((index >> 1) & 0x01);
    LOG_D(PHY,
          "[DLSCH/PDSCH/PUCCH] %d.%d HARQ values (%s, %s) with confidence level %s, xrt_mag %d xrt_mag_next %d "
          "pucch_power_dBtimes10 %d n0 %d (%d,%d) "
          "pucch0_thres %d, cqi %d, SNRtimes10 %d\n",
          frame,
          slot,
          uci_pdu->harq.harq_list[1].harq_value == 0 ? "ACK" : "NACK",
          uci_pdu->harq.harq_list[0].harq_value == 0 ? "ACK" : "NACK",
          uci_pdu->harq.harq_confidence_level == 0 ? "good" : "bad",
          xrtmag_dBtimes10,
          xrtmag_next_dBtimes10,
          pucch_power_dBtimes10,
          max_n0,
          pucch0_n00,
          pucch0_n01,
          gNB->pucch0_thres,
          cqi,
          SNRtimes10);
    if (pucch_pdu->sr_flag == 1) {
      uci_pdu->sr.sr_indication = (index > 3) ? 1 : 0;
      uci_pdu->sr.sr_confidence_level = no_conf;
      if (uci_pdu->sr.sr_indication == 1 && uci_pdu->sr.sr_confidence_level == 0) {
        uci_stats->pucch0_positive_SR++;
        LOG_D(PHY, "PUCCH0 got positive SR. Cumulative number of positive SR %d\n", uci_stats->pucch0_positive_SR);
      }
    }
  }
}
//*****************************************************************//
void nr_decode_pucch1(PHY_VARS_gNB *gNB,
                      c16_t **rxdataF,
                      int frame,
                      int slot,
                      nfapi_nr_uci_pucch_pdu_format_0_1_t *uci_pdu,
                      const nfapi_nr_pucch_pdu_t *pucch_pdu)
{
  /*
   * Implement TS 38.211 Subclause 6.3.2.4.1 Sequence modulation
   *
   */
  NR_DL_FRAME_PARMS *frame_parms = &gNB->frame_parms;
  const uint8_t n_rx = pucch_pdu->param_v4.numSpatialStreamIndices;

  const int soffset = (slot & 3) * frame_parms->symbols_per_slot * frame_parms->ofdm_symbol_size;
  // lprime is the index of the OFDM symbol in the slot that corresponds to the first OFDM symbol of the PUCCH transmission in the
  // slot given by [5, TS 38.213]
  const int lprime = pucch_pdu->start_symbol_index;
  // mcs = 0 except for PUCCH format 0
  const uint8_t mcs = 0;
  // r_u_v_alpha_delta_re and r_u_v_alpha_delta_im tables containing the sequence y(n) for the PUCCH, when they are multiplied by
  // d(0) r_u_v_alpha_delta_dmrs_re and r_u_v_alpha_delta_dmrs_im tables containing the sequence for the DM-RS.
  c16_t r_u_v_alpha_delta[12], r_u_v_alpha_delta_dmrs[12];
  /*
   * in TS 38.213 Subclause 9.2.1 it is said that:
   * for PUCCH format 0 or PUCCH format 1, the index of the cyclic shift
   * is indicated by higher layer parameter PUCCH-F0-F1-initial-cyclic-shift
   */
  /*
   * the complex-valued symbol d_0 shall be multiplied with a sequence r_u_v_alpha_delta(n): y(n) = d_0 * r_u_v_alpha_delta(n)
   */
  // the value of u,v (delta always 0 for PUCCH) has to be calculated according to TS 38.211 Subclause 6.3.2.2.1
  uint8_t u = 0, v = 0; //,delta=0;

  // Intra-slot frequency hopping shall be assumed when the higher-layer parameter intraSlotFrequencyHopping is provided,
  // regardless of whether the frequency-hop distance is zero or not,
  // otherwise no intra-slot frequency hopping shall be assumed
  // uint8_t PUCCH_Frequency_Hopping = 0 ; // from higher layers
  const bool intraSlotFrequencyHopping = pucch_pdu->prb_start != pucch_pdu->second_hop_prb;
  float inv_sqrt2 = 0.70710678118f; // 1 / sqrt(2)
  int64_t signal_energy = 0, signal_energy_ant0 = 0;
  uint8_t nb_re_pucch = pucch_pdu->prb_size * 12;
  pucch_GroupHopping_t pucch_GroupHopping = pucch_pdu->group_hop_flag + (pucch_pdu->sequence_hop_flag << 1);
  int16_t amp = 0x7FFF;
  int xrtmag_dBtimes10 = 0;

  NR_gNB_UCI_STATS_t stack_uci_stats = {0};
  NR_gNB_UCI_STATS_t *uci_stats = &stack_uci_stats;
  NR_gNB_PHY_STATS_t *phy_stats = get_phy_stats(gNB, pucch_pdu->rnti);
  if (phy_stats != NULL) {
    phy_stats->frame = frame;
    uci_stats = &phy_stats->uci_stats;
  }

#ifdef DEBUG_NR_PUCCH_RX
  printf("\t [nr_decode_pucch1] intraSlotFrequencyHopping = %d \n", intraSlotFrequencyHopping);
  printf("\t [nr_decode_pucch1] soffset= %d\n", soffset);
#endif
  /*
   * Implementing TS 38.211 Subclause 6.3.2.4.2 Mapping to physical resources
   */
// This value has to be calculated from mprime*12*table_6_3_2_4_1_1_N_SF_mprime_PUCCH_1_noHop[pucch_symbol_length]+m*12+n
#define MAX_SIZE_Z 168 
  c16_t z_rx[16][MAX_SIZE_Z] = {0};
  c16_t z_dmrs_rx[16][MAX_SIZE_Z] = {0};
  c16_t z[16][12] = {0};
  const int half_nb_rb_dl = frame_parms->N_RB_DL >> 1;
  const bool nb_rb_is_even = (frame_parms->N_RB_DL & 1) == 0;
  int prb_start = pucch_pdu->prb_start;
  for (int l = 0; l < pucch_pdu->nr_of_symbols; l++) { // extracting data and dmrs from rxdataF
    if (intraSlotFrequencyHopping && (l >= floor(pucch_pdu->nr_of_symbols / 2))) { // intra-slot hopping enabled, we need
      // to calculate new offset PRB
      prb_start = pucch_pdu->bwp_start + pucch_pdu->second_hop_prb;
    }
    int re_offset = (l + pucch_pdu->start_symbol_index) * frame_parms->ofdm_symbol_size;
    if (nb_rb_is_even) {
      if (prb_start < half_nb_rb_dl) // if number RBs in bandwidth is even and
                                                // current PRB is lower band
        re_offset += 12 * prb_start + frame_parms->first_carrier_offset;
      else // if number RBs in bandwidth is even and current PRB is upper band
        re_offset += 12 * (prb_start - half_nb_rb_dl);
    } else {
      if (prb_start < half_nb_rb_dl) // if number RBs in bandwidth is odd  and
                                                // current PRB is lower band
        re_offset += 12 * prb_start + frame_parms->first_carrier_offset;
      else if (prb_start > half_nb_rb_dl) // if number RBs in bandwidth is odd
                                                     // and current PRB is upper band
        re_offset += 12 * (prb_start - half_nb_rb_dl) - 6;
      else // if number RBs in bandwidth is odd  and current PRB contains DC
        re_offset += 12 * prb_start + frame_parms->first_carrier_offset;
    }

    for (int n = 0; n < 12; n++) {
      const int current_subcarrier = (l / 2) * 12 + n;
      if (n == 6 && prb_start == half_nb_rb_dl && !nb_rb_is_even) {
        // if number RBs in bandwidth is odd  and current PRB contains DC, we need to recalculate the offset when n=6 (for second
        // half PRB)
        re_offset = ((l + pucch_pdu->start_symbol_index) * frame_parms->ofdm_symbol_size);
      }

      if (l % 2 == 1) // mapping PUCCH or DM-RS according to TS38.211 subclause 6.4.1.3.1
        for (int r = 0; r < n_rx; r++) {
          z_rx[r][current_subcarrier] = rxdataF[r][soffset + re_offset];
          z[r][n] = z_rx[r][current_subcarrier];
        }
      else
        for (int r = 0; r < n_rx; r++) {
          z_dmrs_rx[r][current_subcarrier] = rxdataF[r][soffset + re_offset];
          z[r][n] = z_dmrs_rx[r][current_subcarrier];
        }

#ifdef DEBUG_NR_PUCCH_RX
      printf(
          "\t [nr_decode_pucch1] mapping %s to RE \t amp=%d "
          "\tofdm_symbol_size=%d \tN_RB_DL=%d \tfirst_carrier_offset=%d "
          "\tz_pucch[%d]=rxptr(%d/%d)=(x_n(l=%d,n=%d)=(%d,%d))\n",
          l % 2 ? "PUCCH" : "DM-RS",
          amp,
          frame_parms->ofdm_symbol_size,
          frame_parms->N_RB_DL,
          frame_parms->first_carrier_offset,
          current_subcarrier,
          soffset + re_offset,
	  re_offset - (l + pucch_pdu->start_symbol_index) * frame_parms->ofdm_symbol_size,
          l,
          n,
          rxdataF[0][soffset + re_offset].r,
          rxdataF[0][soffset + re_offset].i);
#endif
      re_offset++;
    } // end sc loop

    // compute signal energy

    for (int r = 0; r < n_rx; r++) {
      int energ = signal_energy_nodc(z[r], nb_re_pucch);
      signal_energy += energ;
      if (!r)
        signal_energy_ant0 += energ;
    }

  } // end symbols loop

  signal_energy /= (pucch_pdu->nr_of_symbols * n_rx);
  signal_energy_ant0 /= pucch_pdu->nr_of_symbols;
  int pucch_power_dBtimes10 = 10 * dB_fixed(signal_energy);
  int max_n0 = max(gNB->measurements.n0_subband_power_tot_dB[pucch_pdu->bwp_start + prb_start],
                   gNB->measurements.n0_subband_power_tot_dB[pucch_pdu->bwp_start + pucch_pdu->second_hop_prb]);
  const int SNRtimes10 = pucch_power_dBtimes10 - (10 * max_n0);

  LOG_D(PHY,
        "signal_energy %lld signal_energy_ant0 %lld pucch_power_dBtimes10 %d max_n0 %d SNRtimes10 %d\n",
        (long long)signal_energy,
        (long long)signal_energy_ant0,
        pucch_power_dBtimes10,
        max_n0,
        SNRtimes10);
  int cqi;
  if (SNRtimes10 < -640)
    cqi = 0;
  else if (SNRtimes10 > 635)
    cqi = 255;
  else
    cqi = (640 + SNRtimes10) / 5;

  cd_t y[16] = {0}, y1[16] = {0};
  // generating transmitted sequence and dmrs
  for (int l = 0; l < pucch_pdu->nr_of_symbols; l++) {
#ifdef DEBUG_NR_PUCCH_RX
    printf("\t [nr_decode_pucch1] for symbol l=%d, lprime=%d\n", l, lprime);
#endif
    // y_n contains the complex value d multiplied by the sequence r_u_v
    // if frequency hopping is disabled, intraSlotFrequencyHopping is not
    // provided
    //              n_hop = 0
    // if frequency hopping is enabled,  intraSlotFrequencyHopping is provided
    //              n_hop = 0 for first hop
    //              n_hop = 1 for second hop
    const int n_hop = intraSlotFrequencyHopping && l >= pucch_pdu->nr_of_symbols / 2 ? 1 : 0;

#ifdef DEBUG_NR_PUCCH_RX
    printf("\t [nr_decode_pucch1] entering function nr_group_sequence_hopping with n_hop=%d, nr_tti_tx=%d\n", n_hop, slot);
#endif
    nr_group_sequence_hopping(pucch_GroupHopping, pucch_pdu->hopping_id, n_hop, slot, &u, &v); // calculating u and v value
    // Defining cyclic shift hopping TS 38.211 Subclause 6.3.2.2.2
    double alpha = nr_cyclic_shift_hopping(pucch_pdu->hopping_id, pucch_pdu->initial_cyclic_shift, mcs, l, lprime, slot);
    for (int n = 0; n < 12; n++) { // generating low papr sequences
      const c16_t angle = {lround(32767 * cos(alpha * n)), lround(32767 * sin(alpha * n))};
      const c16_t table = {table_5_2_2_2_2_Re[u][n], table_5_2_2_2_2_Im[u][n]};
      if (l % 2 == 1)
        r_u_v_alpha_delta[n] = c16mulShift(angle, table, 15);
      else
        r_u_v_alpha_delta_dmrs[n] = c16mulRealShift(c16mulShift(angle, table, 15), amp, 15);
    }
    /*
     * The block of complex-valued symbols y(n) shall be block-wise spread with the orthogonal sequence wi(m)
     * (defined in table_6_3_2_4_1_2_Wi_Re and table_6_3_2_4_1_2_Wi_Im)
     * z(mprime*12*table_6_3_2_4_1_1_N_SF_mprime_PUCCH_1_noHop[pucch_symbol_length]+m*12+n)=wi(m)*y(n)
     *
     * The block of complex-valued symbols r_u_v_alpha_dmrs_delta(n) for DM-RS shall be block-wise spread with the orthogonal
     * sequence wi(m) (defined in table_6_3_2_4_1_2_Wi_Re and table_6_3_2_4_1_2_Wi_Im)
     * z(mprime*12*table_6_4_1_3_1_1_1_N_SF_mprime_PUCCH_1_noHop[pucch_symbol_length]+m*12+n)=wi(m)*y(n)
     *
     */
    // the orthogonal sequence index for wi(m) defined in TS 38.213 Subclause 9.2.1
    // the index of the orthogonal cover code is from a set determined as described in [4, TS 38.211]
    // and is indicated by higher layer parameter PUCCH-F1-time-domain-OCC
    // In the PUCCH_Config IE, the PUCCH-format1, timeDomainOCC field
    const int w_index = pucch_pdu->time_domain_occ_idx;
    if (intraSlotFrequencyHopping == false) { // intra-slot hopping disabled
#ifdef DEBUG_NR_PUCCH_RX
      printf(
          "\t [nr_decode_pucch1] block-wise spread with the orthogonal sequence wi(m) if intraSlotFrequencyHopping = %d, "
          "intra-slot hopping disabled\n",
          intraSlotFrequencyHopping);
#endif
      // mprime is 0 in this not hopping case
      // N_SF_mprime_PUCCH_1 contains N_SF_mprime from table 6.3.2.4.1-1
      // (depending on number of PUCCH symbols nrofSymbols, mprime and
      // intra-slot hopping enabled/disabled) N_SF_mprime_PUCCH_1 contains
      // N_SF_mprime from table 6.4.1.3.1.1-1 (depending on number of PUCCH
      // symbols nrofSymbols, mprime and intra-slot hopping enabled/disabled)
      // N_SF_mprime_PUCCH_1 contains N_SF_mprime from table 6.3.2.4.1-1
      // (depending on number of PUCCH symbols nrofSymbols, mprime=0 and
      // intra-slot hopping enabled/disabled) N_SF_mprime_PUCCH_1 contains
      // N_SF_mprime from table 6.4.1.3.1.1-1 (depending on number of PUCCH
      // symbols nrofSymbols, mprime=0 and intra-slot hopping enabled/disabled)
      // mprime is 0 if no intra-slot hopping / mprime is {0,1} if intra-slot
      // hopping

      // only if intra-slot hopping not enabled (PUCCH)
      int N_SF_mprime_PUCCH_1 = table_6_3_2_4_1_1_N_SF_mprime_PUCCH_1_noHop[pucch_pdu->nr_of_symbols - 1];
      // only if intra-slot hopping not enabled (DM-RS)
      int N_SF_mprime_PUCCH_DMRS_1 = table_6_4_1_3_1_1_1_N_SF_mprime_PUCCH_1_noHop[pucch_pdu->nr_of_symbols - 1]; 
      if (l % 2 == 1) {
        for (int m = 0; m < N_SF_mprime_PUCCH_1; m++) {
          c16_t table = {table_6_3_2_4_1_2_Wi_Re[N_SF_mprime_PUCCH_1][w_index][m],
                         table_6_3_2_4_1_2_Wi_Im[N_SF_mprime_PUCCH_1][w_index][m]};
          if (l / 2 == m) {
            for (int r = 0; r < n_rx; r++) {
              for (int n = 0; n < 12; n++) {
                c16_t *zPtr = z_rx[r] + m * 12 + n;
                *zPtr = c16MulConjShift(table, *zPtr, 15);
                // multiplying with conjugate of low papr sequence
                *zPtr = c16MulConjShift(r_u_v_alpha_delta[n], *zPtr, 16);
              }
            }
          }
        }
      } else {
        for (int m = 0; m < N_SF_mprime_PUCCH_DMRS_1; m++) {
          const c16_t table = {table_6_3_2_4_1_2_Wi_Re[N_SF_mprime_PUCCH_DMRS_1][w_index][m],
                               table_6_3_2_4_1_2_Wi_Im[N_SF_mprime_PUCCH_DMRS_1][w_index][m]};
          if (l / 2 == m) {
            for (int r = 0; r < n_rx; r++) {
              for (int n = 0; n < 12; n++) {
                c16_t *zDmrsPtr = z_dmrs_rx[r] + m * 12 + n;
                *zDmrsPtr = c16MulConjShift(table, *zDmrsPtr, 15);
                // finding channel coeffcients by dividing received dmrs with actual dmrs and storing them in z_dmrs_re_rx and
                // z_dmrs_im_rx arrays
                *zDmrsPtr = c16MulConjShift(r_u_v_alpha_delta_dmrs[n], *zDmrsPtr, 16);
              }
            }
          }
        }
      }
    }

    if (intraSlotFrequencyHopping == true) { // intra-slot hopping enabled
#ifdef DEBUG_NR_PUCCH_RX
      printf(
          "\t [nr_decode_pucch1] block-wise spread with the orthogonal sequence wi(m) if intraSlotFrequencyHopping = %d, "
          "intra-slot hopping enabled\n",
          intraSlotFrequencyHopping);
#endif
      // N_SF_mprime_PUCCH_1 contains N_SF_mprime from table 6.3.2.4.1-1
      // (depending on number of PUCCH symbols nrofSymbols, mprime and
      // intra-slot hopping enabled/disabled) N_SF_mprime_PUCCH_1 contains
      // N_SF_mprime from table 6.4.1.3.1.1-1 (depending on number of PUCCH
      // symbols nrofSymbols, mprime and intra-slot hopping enabled/disabled)
      // N_SF_mprime_PUCCH_1 contains N_SF_mprime from table 6.3.2.4.1-1
      // (depending on number of PUCCH symbols nrofSymbols, mprime=0 and
      // intra-slot hopping enabled/disabled) N_SF_mprime_PUCCH_1 contains
      // N_SF_mprime from table 6.4.1.3.1.1-1 (depending on number of PUCCH
      // symbols nrofSymbols, mprime=0 and intra-slot hopping enabled/disabled)
      // mprime is 0 if no intra-slot hopping / mprime is {0,1} if intra-slot
      // hopping

      // only if intra-slot hopping enabled mprime = 0 (PUCCH)
      int N_SF_mprime_PUCCH_1 = table_6_3_2_4_1_1_N_SF_mprime_PUCCH_1_m0Hop[pucch_pdu->nr_of_symbols - 1]; 
      // only if intra-slot hopping enabled mprime = 0 (DM-RS)
      int N_SF_mprime_PUCCH_DMRS_1 = table_6_4_1_3_1_1_1_N_SF_mprime_PUCCH_1_m0Hop[pucch_pdu->nr_of_symbols - 1]; 
      // only if intra-slot hopping enabled mprime = 0 (PUCCH)
      int N_SF_mprime0_PUCCH_1 = table_6_3_2_4_1_1_N_SF_mprime_PUCCH_1_m0Hop[pucch_pdu->nr_of_symbols - 1];
      // only if intra-slot hopping enabled mprime = 0 (DM-RS)
      int N_SF_mprime0_PUCCH_DMRS_1 = table_6_4_1_3_1_1_1_N_SF_mprime_PUCCH_1_m0Hop[pucch_pdu->nr_of_symbols - 1]; 
#ifdef DEBUG_NR_PUCCH_RX
      printf(
          "\t [nr_decode_pucch1] w_index = %d, N_SF_mprime_PUCCH_1 = %d, N_SF_mprime_PUCCH_DMRS_1 = %d, N_SF_mprime0_PUCCH_1 = %d, "
          "N_SF_mprime0_PUCCH_DMRS_1 = %d\n",
          w_index,
          N_SF_mprime_PUCCH_1,
          N_SF_mprime_PUCCH_DMRS_1,
          N_SF_mprime0_PUCCH_1,
          N_SF_mprime0_PUCCH_DMRS_1);
#endif

      for (int mprime = 0; mprime < 2; mprime++) { // mprime can get values {0,1}
        if (l % 2 == 1) {
          for (int m = 0; m < N_SF_mprime_PUCCH_1; m++) {
            c16_t table = {table_6_3_2_4_1_2_Wi_Re[N_SF_mprime_PUCCH_1][w_index][m],
                           table_6_3_2_4_1_2_Wi_Im[N_SF_mprime_PUCCH_1][w_index][m]};
            if (floor(l / 2) * 12 == (mprime * 12 * N_SF_mprime0_PUCCH_1) + (m * 12)) {
              for (int r = 0; r < n_rx; r++) {
                for (int n = 0; n < 12; n++) {
                  c16_t *zPtr = z_rx[r] + (mprime * 12 * N_SF_mprime0_PUCCH_1) + (m * 12) + n;
                  *zPtr = c16MulConjShift(table, *zPtr, 15);
                  *zPtr = c16MulConjShift(r_u_v_alpha_delta[n], *zPtr, 15);
                }
              }
            }
          }
        }

        else {
          for (int m = 0; m < N_SF_mprime_PUCCH_DMRS_1; m++) {
            c16_t table = {table_6_3_2_4_1_2_Wi_Re[N_SF_mprime_PUCCH_1][w_index][m],
                           table_6_3_2_4_1_2_Wi_Im[N_SF_mprime_PUCCH_1][w_index][m]};
            if (floor(l / 2) * 12 == (mprime * 12 * N_SF_mprime0_PUCCH_DMRS_1) + (m * 12)) {
              for (int r = 0; r < n_rx; r++) {
                for (int n = 0; n < 12; n++) {
                  c16_t *zDmrsPtr = z_dmrs_rx[r] + (mprime * 12 * N_SF_mprime0_PUCCH_DMRS_1) + (m * 12) + n;
                  *zDmrsPtr = c16MulConjShift(table, *zDmrsPtr, 15);
                  // finding channel coeffcients by dividing received dmrs with actual dmrs and storing them in z_dmrs_re_rx and
                  // z_dmrs_im_rx arrays
                  *zDmrsPtr = c16MulConjShift(r_u_v_alpha_delta_dmrs[n], *zDmrsPtr, 15);
                }
              }
            }
          }
        }

        N_SF_mprime_PUCCH_1 =
            table_6_3_2_4_1_1_N_SF_mprime_PUCCH_1_m1Hop[pucch_pdu->nr_of_symbols
                                                        - 1]; // only if intra-slot hopping enabled mprime = 1 (PUCCH)
        N_SF_mprime_PUCCH_DMRS_1 =
            table_6_4_1_3_1_1_1_N_SF_mprime_PUCCH_1_m1Hop[pucch_pdu->nr_of_symbols
                                                          - 1]; // only if intra-slot hopping enabled mprime = 1 (DM-RS)
      }
    }
  } // end of symbols loop

  cd_t H[16] = {0}, H1[16] = {0};
  const double half_nb_symbols = pucch_pdu->nr_of_symbols / 2.0;
  for (int r = 0; r < n_rx; r++) {
    for (int l = 0; l <= half_nb_symbols; l++) {
      if (intraSlotFrequencyHopping == false) {
        for (int n = 0; n < 12; n++) {
          H[r].r += z_dmrs_rx[r][l * 12 + n].r / half_nb_symbols / 12;
          H[r].i += z_dmrs_rx[r][l * 12 + n].i / half_nb_symbols / 12;
          y[r].r += z_rx[r][l * 12 + n].r / half_nb_symbols / 12;
          y[r].i += z_rx[r][l * 12 + n].i / half_nb_symbols / 12;
        }
      } else { // with Frequency-hopping
        if (l < pucch_pdu->nr_of_symbols / 4) {
          for (int n = 0; n < 12; n++) {
            H[r].r += z_dmrs_rx[r][l * 12 + n].r / half_nb_symbols / 12;
            H[r].i += z_dmrs_rx[r][l * 12 + n].i / half_nb_symbols / 12;
            y[r].r += z_rx[r][l * 12 + n].r / half_nb_symbols / 12;
            y[r].i += z_rx[r][l * 12 + n].i / half_nb_symbols / 12;
          }
        } else {
          for (int n = 0; n < 12; n++) {
            H1[r].r += z_dmrs_rx[r][l * 12 + n].r / half_nb_symbols / 12;
            H1[r].i += z_dmrs_rx[r][l * 12 + n].i / half_nb_symbols / 12;
            y1[r].r += z_rx[r][l * 12 + n].r / half_nb_symbols / 12;
            y1[r].i += z_rx[r][l * 12 + n].i / half_nb_symbols / 12;
          }
        }
      }
    }
  }
  // mrc combining to obtain z_re and z_im
  cd_t dp1 = {0}, dm1 = {0}, d0 = {0}, d1 = {0}, d2 = {0}, d3 = {0};
  double dp1mag = 0, dm1mag = 0, d0mag = 0, d1mag = 0, d2mag = 0, d3mag = 0;
  // complex-valued symbol d_re, d_im containing complex-valued symbol d(0):
  for (int r = 0; r < n_rx; r++) {
    if (pucch_pdu->bit_len_harq == 1) // BPSK
    {
      dp1.r = H[r].r + inv_sqrt2 * (y[r].r + y[r].i);
      dp1.i = H[r].i + inv_sqrt2 * (y[r].i - y[r].r);
      dm1.r = H[r].r + inv_sqrt2 * (-y[r].r - y[r].i);
      dm1.i = H[r].i + inv_sqrt2 * (y[r].r - y[r].i);
      dp1mag += squaredMod(dp1);
      dm1mag += squaredMod(dm1);

      LOG_D(PHY,
            "r %d y : (%f,%f) H (%f,%f) dp1 : (%f,%f) : %f dm1 : (%f,%f) : %f\n",
            r,
            y[r].r,
            y[r].i,
            H[r].r,
            H[r].i,
            dp1.r,
            dp1.i,
            dp1mag,
            dm1.r,
            dm1.i,
            dm1mag);

      if (intraSlotFrequencyHopping == true) {
        dp1.r = H1[r].r + inv_sqrt2 * (y1[r].r + y1[r].i);
        dp1.i = H1[r].i + inv_sqrt2 * (y1[r].i - y1[r].r);
        dm1.r = H1[r].r + inv_sqrt2 * (-y1[r].r - y1[r].i);
        dm1.i = H1[r].i + inv_sqrt2 * (y1[r].r - y1[r].i);
        dp1mag += squaredMod(dp1);
        dm1mag += squaredMod(dm1);
      }
      if (r == n_rx - 1) {
        if (dp1mag > dm1mag) {
          uci_pdu->harq.harq_list[0].harq_value = 1;
          xrtmag_dBtimes10 = 10 * (int)dB_fixed64(dp1mag / (12 * pucch_pdu->nr_of_symbols));
        } else {
          //*payload = 1;
          uci_pdu->harq.harq_list[0].harq_value = 0;
          xrtmag_dBtimes10 = 10 * (int)dB_fixed64(dm1mag / (12 * pucch_pdu->nr_of_symbols));
        }
      }
    } else if (pucch_pdu->bit_len_harq == 2) // QPSK
    {
      // d0 = H + (1 - j)*y
      d0.r = H[r].r + inv_sqrt2 * (y[r].r + y[r].i);
      d0.i = H[r].i + inv_sqrt2 * (y[r].i - y[r].r);
      d0mag += squaredMod(d0);

      // d1 = H + (-1 - j)*y
      d1.r = H[r].r + inv_sqrt2 * (-y[r].r + y[r].i);
      d1.i = H[r].i + inv_sqrt2 * (-y[r].r - y[r].i);
      d1mag += squaredMod(d1);

      // d2 = H + (1 + j)*y
      d2.r = H[r].r + inv_sqrt2 * (y[r].r - y[r].i);
      d2.i = H[r].i + inv_sqrt2 * (y[r].i + y[r].r);
      d2mag += squaredMod(d2);

      // d3 = H + (-1 + j)*y
      d3.r = H[r].r + inv_sqrt2 * (-y[r].r - y[r].i);
      d3.i = H[r].i + inv_sqrt2 * (y[r].r - y[r].i);
      d3mag += squaredMod(d3);

      // with frequency hopping
      if (intraSlotFrequencyHopping == true) {
        d0.r = H1[r].r + inv_sqrt2 * (y1[r].r + y1[r].i);
        d0.i = H1[r].i + inv_sqrt2 * (y1[r].i - y1[r].r);
        d0mag += squaredMod(d0);

        d1.r = H1[r].r + inv_sqrt2 * (-y1[r].r + y1[r].i);
        d1.i = H1[r].i + inv_sqrt2 * (-y1[r].r - y1[r].i);
        d1mag += squaredMod(d1);

        d2.r = H1[r].r + inv_sqrt2 * (y1[r].r - y1[r].i);
        d2.i = H1[r].i + inv_sqrt2 * (y1[r].i + y1[r].r);
        d2mag += squaredMod(d2);

        d3.r = H1[r].r + inv_sqrt2 * (-y1[r].r - y1[r].i);
        d3.i = H1[r].i + inv_sqrt2 * (y1[r].r - y1[r].i);
        d3mag += squaredMod(d3);
      }

      LOG_D(PHY,
            "r %d y : (%f,%f) H (%f,%f) d0 : (%f,%f) : %f d1 : (%f,%f) : %f d2 : (%f,%f) : %f d3 : (%f,%f) : %f\n",
            r,
            y[r].r,
            y[r].i,
            H[r].r,
            H[r].i,
            d0.r,
            d0.i,
            d0mag,
            d1.r,
            d1.i,
            d1mag,
            d2.r,
            d2.i,
            d2mag,
            d3.r,
            d3.i,
            d3mag);

      if (r == n_rx - 1) {
        if (d0mag >= d1mag && d0mag >= d2mag && d0mag >= d3mag) {
          uci_pdu->harq.harq_list[0].harq_value = 1;
          uci_pdu->harq.harq_list[1].harq_value = 1;
          xrtmag_dBtimes10 = 10 * (int)dB_fixed64(d0mag / (12 * pucch_pdu->nr_of_symbols));
        } else if (d1mag >= d0mag && d1mag >= d2mag && d1mag >= d3mag) {
          uci_pdu->harq.harq_list[0].harq_value = 1;
          uci_pdu->harq.harq_list[1].harq_value = 0;
          xrtmag_dBtimes10 = 10 * (int)dB_fixed64(d1mag / (12 * pucch_pdu->nr_of_symbols));
        } else if (d2mag >= d0mag && d2mag >= d1mag && d2mag >= d3mag) {
          uci_pdu->harq.harq_list[0].harq_value = 0;
          uci_pdu->harq.harq_list[1].harq_value = 1;
          xrtmag_dBtimes10 = 10 * (int)dB_fixed64(d2mag / (12 * pucch_pdu->nr_of_symbols));
        } else {
          uci_pdu->harq.harq_list[0].harq_value = 0;
          uci_pdu->harq.harq_list[1].harq_value = 0;
          xrtmag_dBtimes10 = 10 * (int)dB_fixed64(d3mag / (12 * pucch_pdu->nr_of_symbols));
        }
      }
    }
  }

  bool no_conf = false;
  if (pucch_pdu->bit_len_harq > 0 || pucch_pdu->sr_flag > 0) {
    if (/*xrtmag_dBtimes10 < (30+xrtmag_next_dBtimes10) ||*/ SNRtimes10 <= gNB->pucch0_thres) {
      no_conf = true;
      LOG_D(PHY, "%d.%d PUCCH F1 bad confidence: %d threshold, %d,\n", frame, slot, gNB->pucch0_thres, SNRtimes10);
    }
  }
  gNB->bad_pucch += no_conf;
  // first bit of bitmap for sr presence and second bit for acknack presence
  uci_pdu->pduBitmap = pucch_pdu->sr_flag | ((pucch_pdu->bit_len_harq > 0) << 1);
  uci_pdu->pucch_format = 1; // format 1
  uci_pdu->rnti = pucch_pdu->rnti;
  uci_pdu->ul_cqi = cqi;
  uci_pdu->timing_advance = 0xffff; // currently not valid
  uci_pdu->rssi = 1280 - (10 * dB_fixed(32767 * 32767) - dB_fixed_times10(signal_energy_ant0));

  if (pucch_pdu->bit_len_harq == 0) {
    uci_pdu->sr.sr_confidence_level = SNRtimes10 < gNB->pucch0_thres;
    uci_stats->pucch1_sr_trials++;
    if (xrtmag_dBtimes10 >= (10 * max_n0 /*+100*/)) {
      uci_pdu->sr.sr_indication = 1;
      uci_stats->pucch1_positive_SR++;
      LOG_D(PHY, "PUCCH1 got positive SR. Cumulative number of positive SR %d\n", uci_stats->pucch1_positive_SR);
    } else {
      uci_pdu->sr.sr_indication = 0;
    }
  } else if (pucch_pdu->bit_len_harq == 1) {
    uci_pdu->harq.num_harq = 1;
    uci_pdu->harq.harq_confidence_level = no_conf;
    LOG_D(PHY,
          "[PUCCH F1] %d.%d HARQ %s with confidence level %s"
          " xrtmag_dBtimes10 %d pucch_power_dBtimes10 %d n0 %d "
          "pucch0_thres %d, "
          "cqi %d, SNRtimes10 %d, energy %f\n",
          frame,
          slot,
          uci_pdu->harq.harq_list[0].harq_value == 0 ? "ACK" : "NACK",
          uci_pdu->harq.harq_confidence_level == 0 ? "good" : "bad",
          xrtmag_dBtimes10,
          pucch_power_dBtimes10,
          max_n0,
          gNB->pucch0_thres, // using same pucch threshold for both pucch 0 and pucch 1.
          cqi,
          SNRtimes10,
          10 * log10((double)signal_energy_ant0));

    if (pucch_pdu->sr_flag == 1) {
      uci_pdu->sr.sr_indication = 0; // Upper layers determine if SR when the PUCCH resource is an SR resource or not
      uci_pdu->sr.sr_confidence_level = no_conf;
      if (uci_pdu->sr.sr_indication == 1 && uci_pdu->sr.sr_confidence_level == 0) {
        uci_stats->pucch1_positive_SR++;
        LOG_D(PHY, "PUCCH F1 got positive SR. Cumulative number of positive SR %d\n", uci_stats->pucch1_positive_SR);
      }
    }
    uci_stats->pucch11_trials++;
  } else {
    uci_pdu->harq.num_harq = 2;
    uci_pdu->harq.harq_confidence_level = no_conf;
    LOG_D(PHY,
          "[PUCCH F1] %d.%d HARQ values (%s, %s) with confidence level %s, xrtmag_dBtimes10 %d pucch_power_dBtimes10 %d n0 %d "
          "pucch0_thres %d, cqi %d, SNRtimes10 %d\n",
          frame,
          slot,
          uci_pdu->harq.harq_list[1].harq_value == 0 ? "ACK" : "NACK",
          uci_pdu->harq.harq_list[0].harq_value == 0 ? "ACK" : "NACK",
          uci_pdu->harq.harq_confidence_level == 0 ? "good" : "bad",
          xrtmag_dBtimes10,
          pucch_power_dBtimes10,
          max_n0,
          gNB->pucch0_thres, // using same pucch threshold for both pucch 0 and pucch 1.
          cqi,
          SNRtimes10);
    if (pucch_pdu->sr_flag == 1) {
      uci_pdu->sr.sr_indication = 0; // Upper layers determine if SR when the PUCCH resource is an SR resource or not
      uci_pdu->sr.sr_confidence_level = no_conf;
      if (uci_pdu->sr.sr_indication == 1 && uci_pdu->sr.sr_confidence_level == 0) {
        uci_stats->pucch1_positive_SR++;
        LOG_D(PHY, "PUCCH F1 got positive SR. Cumulative number of positive SR %d\n", uci_stats->pucch1_positive_SR);
      }
    }
  }
}

typedef struct {
  c16_t cw[16];
} cw_t;
static cw_t pucch2_3_3bit[8] __attribute__((aligned(32)));
static cw_t pucch2_3_4bit[16] __attribute__((aligned(32)));
static cw_t pucch2_3_5bit[32] __attribute__((aligned(32)));
static cw_t pucch2_3_6bit[64] __attribute__((aligned(32)));
static cw_t pucch2_3_7bit[128] __attribute__((aligned(32)));
static cw_t pucch2_3_8bit[256] __attribute__((aligned(32)));
static cw_t pucch2_3_9bit[512] __attribute__((aligned(32)));
static cw_t pucch2_3_10bit[1024] __attribute__((aligned(32)));
static cw_t pucch2_3_11bit[2048] __attribute__((aligned(32)));

static cw_t *pucch2_3_lut[9] =
    {pucch2_3_3bit, pucch2_3_4bit, pucch2_3_5bit, pucch2_3_6bit, pucch2_3_7bit, pucch2_3_8bit, pucch2_3_9bit, pucch2_3_10bit, pucch2_3_11bit};

typedef struct {
  int16_t cw[4];
} cw4bit_t;
static cw4bit_t pucch2_3_polar_4bit[16] __attribute__((aligned(32)));
static simde__m128i pucch2_3_polar_llr_num_lut[256];

void init_pucch2_3_luts()
{
  for (int b = 3; b < 12; b++) {
    for (int cw = 0; cw < (1 << b); cw++) {
      uint32_t out = encodeSmallBlock(cw, b);
      uint16_t *tmp = (uint16_t *)pucch2_3_lut[b - 3][cw].cw;
      for (int j = 0; j < 32; j++)
        *tmp++ = (out & (1U << j)) > 0 ? -1 : 1;
    }
  }
  for (int i = 0; i < 16; i++) {
    int16_t *lut_i = pucch2_3_polar_4bit[i].cw;
    *lut_i++ = (i & 0x1) <= 0;
    *lut_i++ = (i & 0x2) <= 0;
    *lut_i++ = (i & 0x4) <= 0;
    *lut_i++ = (i & 0x8) <= 0;
  }
  for (int cw = 0; cw < 256; cw++) {
    int16_t *lut_num_i = (int16_t *)&pucch2_3_polar_llr_num_lut[cw];
    *lut_num_i++ = (cw & 0x1) <= 0;
    *lut_num_i++ = (cw & 0x10) <= 0;
    *lut_num_i++ = (cw & 0x2) <= 0;
    *lut_num_i++ = (cw & 0x20) <= 0;
    *lut_num_i++ = (cw & 0x4) <= 0;
    *lut_num_i++ = (cw & 0x40) <= 0;
    *lut_num_i++ = (cw & 0x8) <= 0;
    *lut_num_i++ = (cw & 0x80) <= 0;
#ifdef DEBUG_NR_PUCCH_RX
    // log_dump(PHY, pucch2_polar_llr_num_lut, 8, LOG_DUMP_C16, "lut_num %d:", i);
#endif
  }
}

static const int dmrs0[11] = {0,0,1,1,1,1,2,2,2,2,3};
static const int dmrs1[11] = {2,3,4,4,5,6,7,7,8,9,10};
static const int dmrs0add[11] = {0,0,1,1,1,1,1,1,1,1,1};
static const int dmrs1add[11] = {2,3,4,4,5,6,3,3,4,4,5};
static const int dmrs2add[11] = {-1,-1,-1,-1,-1,-1,6,6,7,7,8};
static const int dmrs3add[11] = {-1,-1,-1,-1,-1,-1,8,9,10,11,12};

void nr_decode_pucch2_3(PHY_VARS_gNB *gNB,
                        c16_t **rxdataF,
                        int frame,
                        int slot,
                        nfapi_nr_uci_pucch_pdu_format_2_3_4_t *uci_pdu,
                        const nfapi_nr_pucch_pdu_t *pucch_pdu)
{
  NR_DL_FRAME_PARMS *frame_parms = &gNB->frame_parms;
  // pucch_GroupHopping_t pucch_GroupHopping = pucch_pdu->group_hop_flag + (pucch_pdu->sequence_hop_flag<<1);
  const int nb_symbols = pucch_pdu->nr_of_symbols;
  int fmt=pucch_pdu->format_type;


  AssertFatal(fmt==2 || fmt==3, "Format %d is not 2 or 3\n",fmt);
  if (fmt==2) AssertFatal(nb_symbols == 1 || nb_symbols == 2, "Illegal number of symbols  for PUCCH 2 %d\n", nb_symbols);
  if (fmt==3) AssertFatal(nb_symbols >= 4 || nb_symbols <= 14, "Illegal number of symbols  for PUCCH 2 %d\n", nb_symbols);

  AssertFatal((pucch_pdu->prb_start - ((pucch_pdu->prb_start >> 2) << 2)) == 0,
              "Current pucch2 receiver implementation requires a PRB offset multiple of 4. The one selected is %d",
              pucch_pdu->prb_start);

  // extract pucch and dmrs first

#ifdef DEBUG_NR_PUCCH_RX
  printf("Frame.Slot %d.%d PUCCH format %d RX : start_symbol_index %d numSymb %d start_prb %d numPRB %d freq_hop %d\n",frame,slot,fmt,pucch_pdu->start_symbol_index,nb_symbols,pucch_pdu->prb_start,pucch_pdu->prb_size,pucch_pdu->freq_hop_flag);
#endif
  int l2 = pucch_pdu->start_symbol_index;
  int soffset = (slot % RU_RX_SLOT_DEPTH) * frame_parms->symbols_per_slot * frame_parms->ofdm_symbol_size;
  uint16_t starting_prb = pucch_pdu->prb_start + pucch_pdu->bwp_start;
  uint16_t second_hop_prb=starting_prb;
  int re_offset[2];
  re_offset[0] = (12 * starting_prb + frame_parms->first_carrier_offset) % frame_parms->ofdm_symbol_size;
  if (nb_symbols >= 2) {
    if (pucch_pdu->freq_hop_flag) {
      second_hop_prb = pucch_pdu->second_hop_prb;
      re_offset[1] = (12 * (pucch_pdu->second_hop_prb + pucch_pdu->bwp_start) + frame_parms->first_carrier_offset)
                     % frame_parms->ofdm_symbol_size;
    } else {
      re_offset[1] = re_offset[0];
    }
  }
  AssertFatal(pucch_pdu->prb_size * nb_symbols > 1, "number of PRB*SYMB (%d,%d)< 2", pucch_pdu->prb_size, nb_symbols);

  int Prx = pucch_pdu->param_v4.numSpatialStreamIndices;
  //  AssertFatal((pucch_pdu->prb_size&1) == 0,"prb_size %d is not a multiple of2\n",pucch_pdu->prb_size);
  // use 2 for Nb antennas in case of single antenna to allow the following allocations
  const int nb_re_pucch = 12 * pucch_pdu->prb_size;
  c16_t rp[Prx][nb_symbols][nb_re_pucch];
  memset(rp, 0, sizeof(rp));

  int64_t pucch2_3_lev = 0;
  for (int aa = 0; aa < Prx; aa++) {
    for (int symb = 0; symb < nb_symbols; symb++) {
      c16_t *tmp_rp = ((c16_t *)&rxdataF[aa][soffset + (l2 + symb) * frame_parms->ofdm_symbol_size]);
      if (re_offset[(2*symb)/nb_symbols] + nb_re_pucch < frame_parms->ofdm_symbol_size) {
        memcpy(rp[aa][symb], &tmp_rp[re_offset[(2*symb)/nb_symbols]], nb_re_pucch * sizeof(c16_t));
      } else {
        int neg_length = frame_parms->ofdm_symbol_size - re_offset[symb];
        int pos_length = nb_re_pucch - neg_length;
        memcpy(rp[aa][symb], &tmp_rp[re_offset[2*symb/nb_symbols]], neg_length * sizeof(c16_t));
        memcpy(&rp[aa][symb][neg_length], tmp_rp, pos_length * sizeof(c16_t));
      }
      pucch2_3_lev += signal_energy_nodc(rp[aa][symb], nb_re_pucch);
    }
  }

  pucch2_3_lev /= Prx * nb_symbols;
  int pucch2_3_levdB = dB_fixed(pucch2_3_lev);
  int scaling = max((log2_approx64(pucch2_3_lev) >> 1) - 8, 0);
  LOG_D(NR_PHY,
        "%d.%d Decoding pucch %d for %d symbols, %d PRB, nb_harq %d, nb_sr %d, nb_csi %d/%d, pucch2_lev %d dB (scaling %d)\n",
        frame,
        slot,
	fmt,
        nb_symbols,
        pucch_pdu->prb_size,
        pucch_pdu->bit_len_harq,
        pucch_pdu->sr_flag,
        pucch_pdu->bit_len_csi_part1,
        pucch_pdu->bit_len_csi_part2,
        pucch2_3_levdB,
        scaling);

  int nc_group_size = fmt == 2 ? 2 : 1; //PRB
  int ngroup = pucch_pdu->prb_size / nc_group_size;
  if (fmt == 2 && (pucch_pdu->prb_size&1)>0) ngroup++;

  c32_t corr32[nb_symbols][ngroup][Prx];
  memset(corr32, 0, sizeof(corr32));

  int nb_re_data; 
  int nb_re_dmrs;
  int dmrspos[4];
  for (int i=0;i<4;i++) dmrspos[i]=-1;
  int ndmrs=2;

  if (fmt==2) {
    nb_re_data = 8 * pucch_pdu->prb_size;
    nb_re_dmrs = 4 * pucch_pdu->prb_size;
    ndmrs=0;
  }
  else {
    nb_re_data = 12 * pucch_pdu->prb_size;
    nb_re_dmrs = 12 * pucch_pdu->prb_size;
    if (pucch_pdu->add_dmrs_flag == 0) {
      if (pucch_pdu->freq_hop_flag == 0 && nb_symbols == 4) {
	 dmrspos[0] = 1;
         ndmrs = 1;
      }
      else {
        dmrspos[0] = dmrs0[nb_symbols-4]; 
        dmrspos[1] = dmrs1[nb_symbols-4]; 
      }
    }
    else {
      if (pucch_pdu->freq_hop_flag == 0 && nb_symbols == 4) {
	 dmrspos[0] = 1;
         ndmrs = 1;
      }
      else if (nb_symbols>=4 && nb_symbols < 10) {
        dmrspos[0] = dmrs0[nb_symbols-4]; 
        dmrspos[1] = dmrs1[nb_symbols-4]; 
      }
      else { 
        dmrspos[0] = dmrs0add[nb_symbols-4]; 
        dmrspos[1] = dmrs1add[nb_symbols-4]; 
        dmrspos[2] = dmrs2add[nb_symbols-4]; 
        dmrspos[3] = dmrs3add[nb_symbols-4]; 
	ndmrs = 4;
      }
    }
  }
  #define ALIGN 32

  size_t row_bytes = nb_re_data * sizeof(c16_t);
  size_t row_bytes_aligned = (row_bytes + ALIGN - 1) & ~(ALIGN - 1);
  size_t nb_re_data_padded = row_bytes_aligned / sizeof(c16_t);

  c16_t r_ext[Prx][nb_symbols-ndmrs][nb_re_data_padded]
      __attribute__((aligned(32)));

  c16_t r_ext2[Prx][nb_symbols-ndmrs][nb_re_data_padded]
      __attribute__((aligned(32)));

  const simde__m128i swap128 = simde_mm_set_epi8(13,
                                                 12,
                                                 15,
                                                 14,
                                                 9,
                                                 8,
                                                 11,
                                                 10,
                                                 5,
                                                 4,
                                                 7,
                                                 6,
                                                 1,
                                                 0,
                                                 3,
                                                 2);
  // prepare scrambling sequence for data
  uint32_t x2 = ((pucch_pdu->rnti) << 15) + pucch_pdu->data_scrambling_id;
#ifdef DEBUG_NR_PUCCH_RX
  printf("x2 %x\n", x2);
#endif
  c16_t scramb_data[(nb_symbols-ndmrs) * nb_re_data] __attribute__((aligned(32)));

  uint32_t *sGold = gold_cache(x2, nb_symbols * nb_re_data / 2);
  uint8_t *sGold8 = (uint8_t *)sGold;
  for (int i = 0; i < (nb_symbols-ndmrs)*nb_re_data; i += 4) {
    *(simde__m128i *)(scramb_data + i) = byte2m128i[*sGold8++];
#ifdef DEBUG_NR_PUCCH_RX
    log_dump(PHY,scramb_data+i,4, LOG_DUMP_C16, "scram %d",i);
#endif
  }

  c16_t rdmrs_ext[Prx][nb_re_dmrs * ((fmt==2)?nb_symbols : ndmrs)] __attribute__((aligned(32)));
  c16_t pil_dmrs[2][nb_re_dmrs] __attribute__((aligned(32)));
  c16_t r_u_v_alpha_delta_dmrs[(fmt==2? 1 : ndmrs)*nb_re_dmrs] __attribute__((aligned(32)));
  c16_t *r_u_v_alpha_delta_dmrs_p = r_u_v_alpha_delta_dmrs; 
  for (int d = 0; d < (fmt==2?nb_symbols : ndmrs); d++) {

    int symb = fmt == 2 ? d : dmrspos[d];

    // extract DMRS
#ifdef DEBUG_NR_PUCCH_RX
    printf("Extracting PUCCH DMRS %d (%d): nb_re_dmrs %d, symb %d\n",d,dmrspos[d],nb_re_dmrs,symb);
#endif
    for (int aa = 0; aa < Prx; aa++) {
      c16_t *rdmrs_ext_p = rdmrs_ext[aa] + nb_re_dmrs*d;
      c16_t *rp_base = rp[aa][symb];
      for (int prb = 0; prb < pucch_pdu->prb_size; prb++) {
	if (fmt==2) {
          for (int idx = 0; idx < 4; idx++) {
            rp_base++;
            *rdmrs_ext_p++ = *rp_base++;
            rp_base++;
          }
	}
	else {
           memcpy(rdmrs_ext_p,rp_base,nb_re_dmrs*sizeof(c16_t));
#ifdef DEBUG_NR_PUCCH_RX
           log_dump(PHY, rp_base,nb_re_dmrs, LOG_DUMP_C16, "Ant %d dmrs(base) %d:\n", aa,d);
#endif
	   rp_base+=nb_re_dmrs;
	   rdmrs_ext_p+=nb_re_dmrs;
	}
      }
    }

#ifdef DEBUG_NR_PUCCH_RX
    for (int aa = 0; aa < Prx; aa++)
      log_dump(PHY, rdmrs_ext[aa]+d*nb_re_dmrs, nb_re_dmrs, LOG_DUMP_C16, "Ant %d dmrs %d:\n", aa,d);
#endif

    // first compute DMRS component
    const int scramble = pucch_pdu->dmrs_scrambling_id * 2;
    uint32_t x2 =
        ((1ULL << 17) * ((frame_parms->symbols_per_slot * slot + pucch_pdu->start_symbol_index + symb + 1) * (scramble + 1))
         + scramble)
        % (1U << 31); // c_init calculation according to TS38.211 subclause
#ifdef DEBUG_NR_PUCCH_RX
    printf("slot %d, start_symbol_index %d, symbol %d, dmrs_scrambling_id %d\n",
           slot,
           pucch_pdu->start_symbol_index,
           symb,
           pucch_pdu->dmrs_scrambling_id);
#endif 
    if (fmt == 2) {
      int prb = (d==0) ? starting_prb : second_hop_prb;
	      
      uint32_t *sGold = gold_cache(x2, prb / 4 + ngroup / 2);
      // Compute pilot conjugate
      uint8_t *sGold8 = (uint8_t *)(sGold + prb / 4);
      for (int re = 0; re < nb_re_dmrs; re += 4) {
        *(simde__m128i *)(pil_dmrs[symb] + re) = oai_mm_conj(byte2m128i[*sGold8++]);
      }
    }
    else {
      // generating transmitted sequence and dmrs
      if (fmt==3) AssertFatal(nb_re_dmrs<=36,"PUCCH3 nb_re_dmrs %d not supported (should be <= 36)\n",nb_re_dmrs);
      if (fmt==4) AssertFatal(nb_re_dmrs==12,"PUCCH4 nb_re_dmrs %d not supported (should be 12)\n",nb_re_dmrs); 
      const bool intraSlotFrequencyHopping = pucch_pdu->prb_start != pucch_pdu->second_hop_prb;
      pucch_GroupHopping_t pucch_GroupHopping = pucch_pdu->group_hop_flag + (pucch_pdu->sequence_hop_flag << 1);
      int l = dmrspos[d]; 
#ifdef DEBUG_NR_PUCCH_RX
      printf("\t [nr_decode_pucch2_3] dmrs symbol l=%d (ndmrs %d)\n", l, ndmrs);
#endif
      // if frequency hopping is disabled, intraSlotFrequencyHopping is not
      // provided
      //              n_hop = 0
      // if frequency hopping is enabled,  intraSlotFrequencyHopping is provided
      //              n_hop = 0 for first hop
      //              n_hop = 1 for second hop
      const int n_hop = intraSlotFrequencyHopping && l >= pucch_pdu->nr_of_symbols / 2 ? 1 : 0;
      uint8_t u = 0, v = 0; //,delta=0;
      const uint8_t mcs = 0;
      const int lprime = pucch_pdu->start_symbol_index;

#ifdef DEBUG_NR_PUCCH_RX
      printf("\t [nr_decode_pucch2_3] entering function nr_group_sequence_hopping with n_hop=%d, nr_tti_tx=%d\n", n_hop, slot);
#endif

      nr_group_sequence_hopping(pucch_GroupHopping, pucch_pdu->hopping_id, n_hop, slot, &u, &v); // calculating u and v value
      // Defining cyclic shift hopping TS 38.211 Subclause 6.3.2.2.2
      double alpha = nr_cyclic_shift_hopping(pucch_pdu->hopping_id, pucch_pdu->initial_cyclic_shift, mcs, l, lprime, slot);
#ifdef DEBUG_NR_PUCCH_RX
      printf("\t [nr_decode_pucch2_3] alpha %f\n",alpha);
#endif
      for (int n = 0; n < nb_re_dmrs; n++) { // generating low papr sequences
        const c16_t angle = {lround(32767 * cos(alpha * n)), lround(32767 * sin(alpha * n))};
        const c16_t table = {table_5_2_2_2_2_Re[u][n], table_5_2_2_2_2_Im[u][n]};
        r_u_v_alpha_delta_dmrs[n+(d*nb_re_dmrs)] = /*c16mulRealShift(*/c16mulShift(angle, table, 15)/*, amp, 15)*/;
	    r_u_v_alpha_delta_dmrs[n+(d*nb_re_dmrs)].i = -r_u_v_alpha_delta_dmrs[n+(d*nb_re_dmrs)].i;
#ifdef DEBUG_NR_PUCCH_RX
        /*
          printf(
            "\t [nr_decode_pucch2_3] sequence generation \tu=%d \tv=%d "
            "\talpha=%lf \tr_u_v_alpha_delta[n=%d]=(%d,%d) "
            "\ty_n[n=%d]=(%f,%f)\n",
            u,
            v,
            alpha,
            n,
            r_u_v_alpha_delta[n].r,
            r_u_v_alpha_delta[n].i,
            n,
            y_n[n].r,
            y_n[n].i);
          */
#endif
      } // loop over nb_re_dmrs
#ifdef DEBUG_NR_PUCCH_RX
      log_dump(PHY, r_u_v_alpha_delta_dmrs+(d*nb_re_dmrs), nb_re_dmrs, LOG_DUMP_C16, "r_u_v_alpha_delta_%d:\n",d);
#endif
    } // format 2/3
  } // d
  // Compute delay
  c16_t ch_ls[128] __attribute__((aligned(32))) = {0};
  int lendmrs=((fmt==2) ? nb_symbols : ndmrs)*nb_re_dmrs;
  if (fmt==2) {
    c16_t rdmrs_gold[nb_re_dmrs] __attribute__((aligned(32)));
    for (int aa = 0; aa < Prx; aa++) {
      mult_complex_vectors(rdmrs_ext[aa], pil_dmrs[0], rdmrs_gold, lendmrs, 0);
      c16_t *ch_ls_ptr = ch_ls;
      c16_t *end = ch_ls_ptr + 128;
      for (int i = 0; i < nb_re_dmrs; i++)
        for (int k = 0; k < 3 && ch_ls_ptr < end; k++)
          *ch_ls_ptr++ = rdmrs_gold[i];
    }
  }
  delay_t delay = {0};

  if (fmt==2) {
    c16_t ch_temp[128] __attribute__((aligned(32)));
    nr_est_delay(128, ch_ls, ch_temp, &delay);
  }

  // Formate 3/4 Allocate memory for IFDT input buffers
#ifdef __aarch64__ // to be removed with aarch64 DFTs use non-interleaved format
  simde__m128i *fmt3_4_idft_in[Prx];
  simde__m128i *fmt3_4_idft_out=(simde__m128i*)NULL;
  int datacnt = 0;
#else
  c16_t *fmt3_4_idft_out[Prx];
#endif
  if (fmt >= 3) {
#ifdef __aarch64__ // again to be removed when aarch64 DFTS use non-interleaved format
    for (int aa = 0 ; aa < Prx ; aa++)
      fmt3_4_idft_in[aa] = __builtin_alloca_with_align(nb_re_data*sizeof(simde__m128i),32*8);
    fmt3_4_idft_out = __builtin_alloca_with_align(nb_re_data*sizeof(simde__m128i),32*8);
#else
    for (int aa = 0 ; aa < Prx ; aa++) {
      fmt3_4_idft_out[aa] = __builtin_alloca_with_align(nb_re_data*sizeof(c16_t),32*8);
    }
#endif
  }
#ifdef __aarch64 // again
  simde__m128i *interleaved_out[Prx];
  for (int aa = 0 ; aa < Prx ; aa++)
    interleaved_out[aa] = (simde__m128i*)__builtin_alloca_with_align(nb_re_data * sizeof(simde__m128i),32*8);
#endif
  int s3 = 0;
  for (int symb = 0 ; symb < nb_symbols ; symb++) { 
    if (fmt == 2) {
      // Apply delay compensation on the input
      if (delay.est_delay != 0) {
        int delay_idx = get_delay_idx(delay.est_delay, MAX_DELAY_COMP);
        // printf("pucch2 est delay %d\n",delay.est_delay);
        c16_t *delay_table = frame_parms->delay_table128[delay_idx];
        for (int aa = 0; aa < Prx; aa++)
          mult_complex_vectors(rp[aa][symb], delay_table, rp[aa][symb], nb_re_pucch, 8);
      }
      // else printf("pucch2 no delay\n");

      // extract again DMRS, and signal, after delay compensation
      for (int aa = 0; aa < Prx; aa++) {
        c16_t *r_ext_p = r_ext[aa][symb];
        c16_t *rdmrs_ext_p = rdmrs_ext[aa];
        c16_t *rp_base = rp[aa][symb];
        for (int prb = 0; prb < pucch_pdu->prb_size; prb++) {
          for (int idx = 0; idx < 4; idx++) {
            *r_ext_p++ = *rp_base++;
            *rdmrs_ext_p++ = *rp_base++;
            *r_ext_p++ = *rp_base++;
          }
        }
      } // aa
    } // fmt==2
    else if (symb != dmrspos[0] && symb != dmrspos[1] && symb != dmrspos[2] && symb != dmrspos[3]) {
      for (int aa = 0; aa < Prx; aa++) {
        memcpy(r_ext[aa][s3],rp[aa][symb],nb_re_pucch*sizeof(c16_t));
      } // aa
    }
    int d=0;
    if (symb == dmrspos[1]) d=1;
    if (symb == dmrspos[2]) d=2;
    if (symb == dmrspos[3]) d=3;
#ifdef DEBUG_NR_PUCCH_RX
    for (int aa = 0; aa < Prx; aa++) {
      if (fmt==2 || symb == dmrspos[0] || symb == dmrspos[1] || symb == dmrspos[2] || symb == dmrspos[3]) log_dump(PHY, rdmrs_ext[aa]+((fmt>2) ? (d*nb_re_dmrs) : 0), nb_re_dmrs, LOG_DUMP_C16, "after delay compensation ant %d symb %d dmrs:\n", aa,symb);
      if (fmt>2 && symb != dmrspos[0] && symb != dmrspos[1] && symb != dmrspos[2] && symb != dmrspos[3]) log_dump(PHY, r_ext[aa][s3], nb_re_data, LOG_DUMP_C16, "after delay compensation ant %d symb %d data:\n", aa,symb);
    }
#endif
    if (fmt==2 || symb == dmrspos[0] || symb == dmrspos[1] || symb == dmrspos[2] || symb == dmrspos[3]) {
      for (int aa = 0; aa < Prx; aa++) {
        c16_t *pil_ptr = (fmt==2) ? pil_dmrs[symb] : r_u_v_alpha_delta_dmrs_p;
#ifdef DEBUG_NR_PUCCH_RX
	printf("computing corr32 for symb %d, ngroup %d, nc_group_size %d\n",symb,ngroup,nc_group_size);
#endif
        for (int prb = 0; prb < pucch_pdu->prb_size; prb++) {
        // non-coherent combining across groups
          c16_t *rdmrs_p = rdmrs_ext[aa] + ((fmt==2) ? (symb*nb_re_dmrs) + (4*prb) : (d*nb_re_dmrs) + (12*prb));
          for (int z = 0; z < ((fmt==2) ? 4 : 12); z++) {
#ifdef DEBUG_NR_PUCCH_RX
            printf("prb %d, grp %d: %d.%d X %d.%d\n",prb,prb/nc_group_size,rdmrs_p->r,rdmrs_p->i,pil_ptr->r,pil_ptr->i);
#endif
            c16_t tmp = c16mulShift(*rdmrs_p++, *pil_ptr++, fmt>=3 ? 15 : scaling);
#ifdef DEBUG_NR_PUCCH_RX
	    printf("tmp = %d+j(%d)\n",tmp.r,tmp.i);
#endif
            corr32[symb][prb/nc_group_size][aa].r += tmp.r;
            corr32[symb][prb/nc_group_size][aa].i += tmp.i;
#ifdef DEBUG_NR_PUCCH_RX
	    printf("aa %d, group %d: corr32 %d+j(%d)\n",aa,prb/nc_group_size,corr32[symb][prb/nc_group_size][aa].r,corr32[symb][prb/nc_group_size][aa].i);
#endif
          } //z
        } //prb
      } // aa
      if (fmt>2) r_u_v_alpha_delta_dmrs_p+=nb_re_dmrs;
    }
#ifdef DEBUG_NR_PUCCH_RX
    if (fmt==2 || symb == dmrspos[0] || symb == dmrspos[1] || symb == dmrspos[2] || symb == dmrspos[3]) log_dump(PHY, corr32[symb][0], Prx, LOG_DUMP_C32, "corr32:");
#endif

    if (fmt>=3) { // copy rx_ext to IDFT buffers and do idft and unscrambling if required
       if (symb != dmrspos[0] && symb != dmrspos[1] && symb != dmrspos[2] && symb != dmrspos[3]) {
	  for (int aa = 0 ; aa < Prx ; aa++) {
#ifdef __aarch64__ // we need this until the aarch64 DFTs come, x86 doesn't use the legacy interleaving format for DFTs with 12
#ifdef DEBUG_NR_PUCCH_RX
	    printf("Filling idft in for symbol symb %d s3 %d datacnt %d\n",symb,s3,datacnt);
#endif
	    for (int i = 0 ; i < nb_re_data ; i++) 
	       ((c16_t*)fmt3_4_idft_in[aa])[datacnt + (4*i)] = r_ext[aa][s3][i];
	    if (datacnt == 3 || symb == (nb_symbols-1)) {
	      // we've loaded 4 OFDM symbols, take conjugates of input for IDFT
	      for (int i=0;i<nb_re_data;i++)
	        fmt3_4_idft_in[aa][i] = oai_mm_conj(fmt3_4_idft_in[aa][i]);
	      dft_size_idx_t dftsize = get_dft(nb_re_data);
  	      dft(dftsize,(int16_t*)fmt3_4_idft_in[aa],(int16_t*)fmt3_4_idft_out,1);
	      // note, the output is the conjugate of the idft, we need to correct this below
	      // transpose idft_out
	      
	      log_dump(PHY,(c16_t*)fmt3_4_idft_in[aa],nb_re_data,LOG_DUMP_C16,"idft_in(%d,%d):",s3,nb_re_data);
	      log_dump(PHY,(c16_t*)interleaved_out[aa],nb_re_data,LOG_DUMP_C16,"idft_in(%d,%d):",s3,nb_re_data);
#ifndef SCALAR_TRANSPOSE
	      int ioff = nb_re_data/4;
	      
	      for (int i=0,j=0;i<nb_re_data;i+=4,j++) {
                  simde__m128i a0 = fmt3_4_idft_out[i];
                  simde__m128i a1 = fmt3_4_idft_out[i+1];
                  simde__m128i a2 = fmt3_4_idft_out[i+2];
                  simde__m128i a3 = fmt3_4_idft_out[i+3];
		  simde__m128i b0 = simde_mm_unpacklo_epi32(a0,a1); // a00 a10 a01 a11
      		  simde__m128i b1 = simde_mm_unpacklo_epi32(a2,a3); // a20 a30 a21 a31 
		  interleaved_out[aa][j]   = simde_mm_unpacklo_epi64(b0,b1); // a00 a10 a20 a30
		  interleaved_out[aa][j+ioff] = simde_mm_unpackhi_epi64(b0,b1); // a01 a11 a21 a31
		  simde__m128i b2 = simde_mm_unpackhi_epi32(a0,a1); // a02 a12 a03 a13
      		  simde__m128i b3 = simde_mm_unpackhi_epi32(a2,a3); // a22 a32 a23 a33 
		  interleaved_out[aa][j+2*ioff] = simde_mm_unpacklo_epi64(b2,b3); // a02 a12 a22 a32
		  interleaved_out[aa][j+3*ioff] = simde_mm_unpackhi_epi64(b2,b3); // a03 a13 a23 a33
	      } 
#else
#ifdef DEBUG_NR_PUCCH_RX
 	      printf("Filling idft in for symbol symb %d s3 %d\n",symb,s3);
#endif
	      for (int i=0;i<nb_re_data;i++) {
                   ((c16_t*)interleaved_out[aa])[i] = ((c16_t*)fmt3_4_idft_out)[4*i];
                   ((c16_t*)interleaved_out[aa])[nb_re_data + i] = ((c16_t*)fmt3_4_idft_out)[1+4*i];
                   ((c16_t*)interleaved_out[aa])[2*nb_re_data + i] = ((c16_t*)fmt3_4_idft_out)[2+4*i];
                   ((c16_t*)interleaved_out[aa])[3*nb_re_data + i] = ((c16_t*)fmt3_4_idft_out)[3+4*i];
	      }
#endif
#ifdef DEBUG_NR_PUCCH_RX
	      log_dump(PHY,(c16_t*)interleaved_out[aa],nb_re_data,LOG_DUMP_C16,"idft_intl0:");
	      log_dump(PHY,((c16_t*)interleaved_out[aa])+nb_re_data,nb_re_data,LOG_DUMP_C16,"idft_intl1:");
	      log_dump(PHY,((c16_t*)interleaved_out[aa])+2*nb_re_data,nb_re_data,LOG_DUMP_C16,"idft_intl2:");
	      log_dump(PHY,((c16_t*)interleaved_out[aa])+3*nb_re_data,nb_re_data,LOG_DUMP_C16,"idft_intl3:");
#endif	       
      	      // unscrambling here
	      for (int s=3;s>=0;s--) {
#ifdef DEBUG_NR_PUCCH_RX
	         printf("Unscrambling symbol %d (%d-%d) offset %d\n",s3-s,s3,s,(s3-s)*nb_re_data/4);
#endif
                 simde__m128i *c_ptr = (simde__m128i *)scramb_data + ((s3-s)*nb_re_data/4);
                 simde__m128i *br_ptr = (simde__m128i *)r_ext[aa][s3-s];
                 simde__m128i *bi_ptr = (simde__m128i *)r_ext2[aa][s3-s];
		 for (int i=0; i < nb_re_data/4 ; i++) {
			 
		     // note: conjugate here completes the idft above
                     simde__m128i tmp = simde_mm_srai_epi16(oai_mm_conj(interleaved_out[aa][((3-s)*nb_re_data/4)+i]), scaling);
                     br_ptr[i] = simde_mm_sign_epi16(tmp, c_ptr[i]); // this contains unscrambled [Re Im] sequence 
                     bi_ptr[i] = oai_mm_conj(simde_mm_sign_epi16(simde_mm_shuffle_epi8(tmp, swap128), c_ptr[i])); // this contains unscramble [Im -Re] requence
#ifdef DEBUG_NR_PUCCH_RX
		     log_dump(PHY,(c16_t*)&tmp,4,LOG_DUMP_C16,"btilde_ptr:");
		     log_dump(PHY,(c16_t*)(c_ptr+i),4,LOG_DUMP_C16,"cptr:");
		     log_dump(PHY,(c16_t*)(br_ptr+i),4,LOG_DUMP_C16,"brptr:");
		     log_dump(PHY,(c16_t*)(bi_ptr+i),4,LOG_DUMP_C16,"biptr:");
#endif
		 }
	      }
              if (aa == (Prx-1)) {
		datacnt = -1;
	      }
	    } // datacnt = =3
#else // this is the case for the new DFT 12N routines 

	    dft_size_idx_t dftsize = get_dft(nb_re_data);
  	    idft(dftsize,(int16_t*)r_ext[aa][s3],(int16_t*)fmt3_4_idft_out[aa],1);
            simde__m128i *c_ptr = (simde__m128i *)scramb_data + (s3*nb_re_data/4);
            simde__m128i *br_ptr = (simde__m128i *)r_ext[aa][s3];
            simde__m128i *bi_ptr = (simde__m128i *)r_ext2[aa][s3];
	    for (int i=0; i < nb_re_data/4 ; i++) {
               simde__m128i tmp = simde_mm_srai_epi16(((simde__m128i *)fmt3_4_idft_out[aa])[i], scaling);
               br_ptr[i] = simde_mm_sign_epi16(tmp, c_ptr[i]); // this contains unscrambled [Re Im] sequence 
               bi_ptr[i] = oai_mm_conj(simde_mm_sign_epi16(simde_mm_shuffle_epi8(tmp, swap128), c_ptr[i])); // this contains unscramble [Im -Re] requence
#ifdef DEBUG_NR_PUCCH_RX
	       log_dump(PHY,(c16_t*)&tmp,4,LOG_DUMP_C16,"btilde_ptr:");
	       log_dump(PHY,(c16_t*)(c_ptr+i),4,LOG_DUMP_C16,"cptr:");
	       log_dump(PHY,(c16_t*)(br_ptr+i),4,LOG_DUMP_C16,"brptr:");
	       log_dump(PHY,(c16_t*)(bi_ptr+i),4,LOG_DUMP_C16,"biptr:");
#endif
	     }
#endif
	  } // aa
#ifdef __aarch64__
	  datacnt++;
#endif
       } // symb check
    } // fmt 3/4 check
    
    // apply gold sequence on data symbols (unscrambling)
    if (fmt==2) {
      for (int aa = 0; aa < Prx; aa++) {
#ifdef DEBUG_NR_PUCCH_RX
	printf("unscrambling symbol %d, nb_re_data %d\n",s3,nb_re_data);
	log_dump(PHY,scramb_data + s3*nb_re_data,nb_re_data,LOG_DUMP_C16,"c:");
	log_dump(PHY,r_ext[aa][s3],nb_re_data,LOG_DUMP_C16,"r:");
#endif
	simde__m128i *c_ptr = (simde__m128i *)(scramb_data + s3*nb_re_data);
        simde__m128i *end = (simde__m128i *)(scramb_data + (s3+1)*nb_re_data);
        for (simde__m128i *ptr = (simde__m128i *)r_ext[aa][s3], *ptr2 = (simde__m128i *)r_ext2[aa][s3]; c_ptr < end;
             ptr++, c_ptr++, ptr2++) {
          simde__m128i tmp = simde_mm_srai_epi16(*ptr, scaling);
          *ptr2 = oai_mm_conj(simde_mm_sign_epi16(simde_mm_shuffle_epi8(tmp, swap128), *c_ptr)); // r_ext(im -re)(i) * c(i)
          *ptr = simde_mm_sign_epi16(tmp, *c_ptr); // r_ext(i) * c(i)
        }
      } //aa loop
    }//fmt == 2
    if (symb != dmrspos[0] && symb != dmrspos[1] && symb != dmrspos[2] && symb != dmrspos[3]) 
      s3++;
  } // symb loop

  int nb_bit = pucch_pdu->bit_len_harq + pucch_pdu->sr_flag + pucch_pdu->bit_len_csi_part1 + pucch_pdu->bit_len_csi_part2;
  AssertFatal(nb_bit > 2 && nb_bit < 65,
              "illegal length (%d : %d,%d,%d,%d)\n",
              nb_bit,
              pucch_pdu->bit_len_harq,
              pucch_pdu->sr_flag,
              pucch_pdu->bit_len_csi_part1,
              pucch_pdu->bit_len_csi_part2);

  uint64_t decodedPayload[nb_symbols];
  memset(decodedPayload, 0, sizeof(decodedPayload));
  uint8_t corr_dB;
  int decoderState = 2;
  if (pucch2_3_levdB < gNB->measurements.n0_subband_power_avg_dB + (gNB->pucch0_thres / 10)) 
    decoderState = 1; // assuming missed detection, only attempt to decode for polar case (with CRC)
  LOG_D(NR_PHY,"pucch2_levdB %d n0+thres %d (thres %d) decoderState %d\n", pucch2_3_levdB, gNB->measurements.n0_subband_power_avg_dB + (gNB->pucch0_thres / 10), gNB->pucch0_thres / 10,decoderState);

  if (nb_bit < 12 && decoderState == 2) { // short blocklength case
    // fill corr rable wit symbols for format 3/4
    if (fmt >= 3) {
      int dmrsp=-1;
      for (int symb=0;symb<nb_symbols;symb++) {
	     if (symb == dmrspos[0] || symb == dmrspos[1] || symb == dmrspos[2] || symb == dmrspos[3]) continue;
	     if ( (symb<=(dmrspos[0] + (dmrspos[1]-dmrspos[0])/2))) // symb is around 1st DMRS
              dmrsp = dmrspos[0];
  	     else if ((ndmrs == 2 || (ndmrs == 4 && symb < dmrspos[1] + (dmrspos[2] - dmrspos[1])/2))) 
              dmrsp = dmrspos[1];
             else if (ndmrs == 4 && symb < dmrspos[2] + (dmrspos[3] - dmrspos[2])/2)
              dmrsp = dmrspos[2];
             else if (ndmrs == 4)
              dmrsp = dmrspos[3];
	     AssertFatal(dmrsp>0,"dmrsp %d should not be <=0\n",dmrsp);
             for (int group=0;group<ngroup;group++)
                for (int aa = 0 ; aa < Prx ; aa++) {
#ifdef DEBUG_NR_PUCCH_RX
	          printf("Copying core32 dmrsp %d group %d aa %d (%d,%d) to symb %d\n",dmrsp,group,aa,corr32[dmrsp][group][aa].r,corr32[dmrsp][group][aa].i,symb);
#endif
	          corr32[symb][group][aa] = corr32[dmrsp][group][aa];
	   }
      }
    }
    uint64_t corr = 0;
    int cw_ML = 0;
    for (int cw = 0; cw < 1 << nb_bit; cw++) {
      uint64_t corr_tmp = 0;
      c64_t sum_of_prod[ngroup][2][Prx];
      if (fmt == 2) {
        const simde__m128i *coeff = (const simde__m128i *)&pucch2_3_lut[nb_bit - 3][cw].cw;
        for (int aa = 0; aa < Prx; aa++) {
	  for (int g = 0 ; g < ngroup ; g++) { 
	     if (pucch_pdu->freq_hop_flag) { 
	        for (int d=0; d < 2; d++) {	
	  	   sum_of_prod[g][d][aa] = (c64_t){corr32[d][g][aa].r,corr32[d][g][aa].i};
#ifdef DEBUG_NR_PUCCH_RX
		   printf("sum_of_prod[%d][%d][%d] %d.%d\n",g,d,aa,corr32[d][g][aa].r,corr32[d][g][aa].i);
#endif
		}
	     }
	     else if (nb_symbols==1) {
	       sum_of_prod[g][0][aa] = (c64_t){corr32[0][g][aa].r,corr32[0][g][aa].i};
#ifdef DEBUG_NR_PUCCH_RX
	       printf("sum_of_prod[%d][0][%d] %d.%d\n",g,aa,corr32[0][g][aa].r,corr32[0][g][aa].i);
#endif
	     }
	     else {
	       sum_of_prod[g][0][aa] = (c64_t){corr32[0][g][aa].r+corr32[1][g][aa].r,corr32[0][g][aa].i+corr32[1][g][aa].i};
#ifdef DEBUG_NR_PUCCH_RX
	       printf("sum_of_prod[%d][0][%d] %lld.%lld (%d.%d)(%d.%d)\n",g,aa,sum_of_prod[g][0][aa].r,sum_of_prod[g][0][aa].i,corr32[0][g][aa].r,corr32[0][g][aa].i,corr32[1][g][aa].r,corr32[1][g][aa].i);
#endif
	     }
	     
	  }
          int ci=0;
          for (int symb = 0; symb < nb_symbols; symb++) {
            const simde__m128i *rext = (simde__m128i *)r_ext[aa][symb];
            const simde__m128i *rext2 = (simde__m128i *)r_ext2[aa][symb];
            for (int prb = 0; prb < pucch_pdu->prb_size; prb++) {
	      int group = prb/nc_group_size;
            // do complex correlation
              simde__m128i re = simde_mm_madd_epi16(coeff[ci], rext[prb]);
              simde__m128i im = simde_mm_madd_epi16(coeff[ci], rext2[prb]);
              simde__m128i re2 = simde_mm_madd_epi16(coeff[ci+1], rext[prb]);
              simde__m128i im2 = simde_mm_madd_epi16(coeff[ci+1], rext2[prb]);
	      re = simde_mm_add_epi32(re,re2);
	      im = simde_mm_add_epi32(im,im2);
              re = simde_mm_hadd_epi32(re, re);
              re = simde_mm_hadd_epi32(re, re);
              im = simde_mm_hadd_epi32(im, im);
              im = simde_mm_hadd_epi32(im, im);
              int32_t *re32 = (int32_t *)&re;
              int32_t *im32 = (int32_t *)&im;
              c64_t prod = (c64_t){re32[0], im32[0]};
              if (pucch_pdu->freq_hop_flag) {
		 csum(sum_of_prod[group][symb][aa], sum_of_prod[group][symb][aa],prod);
	      }
	      else {
		 csum(sum_of_prod[group][0][aa], sum_of_prod[group][0][aa],prod);
	      }
#ifdef DEBUG_NR_PUCCH_RX
              printf("pucch2 cw %d group %d aa %d ci %d: (%d,%d)+prod=(%ld,%ld)\n",
                     cw,
                     group,
                     aa,
		     ci,
                     pucch_pdu->freq_hop_flag ? sum_of_prod[group][symb][aa].r : sum_of_prod[group][0][aa].r,
                     pucch_pdu->freq_hop_flag ? sum_of_prod[group][symb][aa].i : sum_of_prod[group][0][aa].i,
                     prod.r,
                     prod.i);
#endif
	      ci+=2;
	      ci&=3;
	    } // symb
	  } // group 
	} // aa
      } // fmt==2
      else {
          const simde__m128i *modcw = (simde__m128i *)&pucch2_3_lut[nb_bit - 3][cw].cw;
          AssertFatal(ngroup==1,"only 1 frequency group tested/supported for now (1 PRB)\n");
          for (int aa = 0; aa < Prx; aa++) {
	    int ci=0;
	    // compute channel references
	    // for 2 DMRS, store both in sum_of_prod for non-coherent combining later
	    // for 4 DMRS, coherently combine the pairs 0,1 and 2,3 and store the combinations in sum_of_prod
	    for (int g=0;g<ngroup;g++)
	      for (int d=0;d<2;d++) {
		   if (ndmrs == 2) {
		     sum_of_prod[g][d][aa] = (c64_t){corr32[dmrspos[d]][g][aa].r,corr32[dmrspos[d]][g][aa].i};
#ifdef DEBUG_PUCCH_NR_RX
                     printf("ndmrs = 2 : sum_of_prod[%d][%d][%d] %lld.%lld\n",g,d,aa,sum_of_prod[g][d][aa].r,sum_of_prod[g][d][aa].i);
#endif
		   }
	           else { 
		     csum(sum_of_prod[g][d][aa],corr32[dmrspos[2*d]][g][aa],corr32[dmrspos[1+(2*d)]][g][aa]);
#ifdef DEBUG_PUCCH_NR_RX
                     printf("ndmrs = 4 : sum_of_prod[%d][%d][%d] %lld.%lld\n",g,d/2,aa,sum_of_prod[g][d/2][aa].r,sum_of_prod[g][d/2][aa].i);
#endif
		   }
	      }
	    //loop over symbols correlating within each group, add non-coherently over groups and over symbols around each DMRS 
            int cd=0;
	    for (int symb=0;symb<(nb_symbols-ndmrs);symb++) {
	      if (symb<(nb_symbols-ndmrs)/2) cd=0;
              else cd=1;	      
              for (int group = 0; group < ngroup; group++) {
                const simde__m128i *rext = (simde__m128i *)r_ext[aa][symb];
                const simde__m128i *rext2 = (simde__m128i *)r_ext2[aa][symb];
#ifdef DEBUG_NR_PUCCH_RX
	        log_dump(PHY,(c16_t*)rext,4,LOG_DUMP_C16,"rext0:");
	        log_dump(PHY,(c16_t*)rext2,4,LOG_DUMP_C16,"rext20:");
	        log_dump(PHY,(c16_t*)modcw,4,LOG_DUMP_C16,"cw0:");
		printf("ci %d\n",ci);
#endif
                simde__m128i re = simde_mm_madd_epi16(modcw[ci], rext[0]);
                simde__m128i im = simde_mm_madd_epi16(modcw[ci++], rext2[0]);
		ci &= 3; // Note: this becomes 7 for pi4_BPSK
#ifdef DEBUG_NR_PUCCH_RX
		log_dump(PHY,((int32_t*)&re),2,LOG_DUMP_C32,"re:");
	        log_dump(PHY,(c16_t*)(rext+1),4,LOG_DUMP_C16,"rext1:");
	        log_dump(PHY,(c16_t*)(rext2+1),4,LOG_DUMP_C16,"rext21:");
	        log_dump(PHY,(c16_t*)(modcw+1),4,LOG_DUMP_C16,"cw1:");
		printf("ci %d\n",ci);
#endif
                simde__m128i re2 = simde_mm_madd_epi16(modcw[ci], rext[1]);
                simde__m128i im2 = simde_mm_madd_epi16(modcw[ci++], rext2[1]);
		ci &= 3; // Note: this becomes 7 for pi4_BPSK
#ifdef DEBUG_NR_PUCCH_RX
		log_dump(PHY,((int32_t*)&re2),2,LOG_DUMP_C32,"re2:");
	        log_dump(PHY,(c16_t*)(rext+2),4,LOG_DUMP_C16,"rext2:");
	        log_dump(PHY,(c16_t*)(rext2+2),4,LOG_DUMP_C16,"rext22:");
	        log_dump(PHY,(c16_t*)(modcw+2),4,LOG_DUMP_C16,"cw2:");
		printf("ci %d\n",ci);
#endif
                simde__m128i re3 = simde_mm_madd_epi16(modcw[ci], rext[2]);
                simde__m128i im3 = simde_mm_madd_epi16(modcw[ci++], rext2[2]);
		ci &= 3; // Note: this becomes 7 for pi4_BPSK
                re = simde_mm_add_epi32(re, simde_mm_add_epi32(re2,re3));
                im = simde_mm_add_epi32(im, simde_mm_add_epi32(im2,im3));
                re = simde_mm_hadd_epi32(re, re);
                im = simde_mm_hadd_epi32(im, im);
                re = simde_mm_hadd_epi32(re, re);
                im = simde_mm_hadd_epi32(im, im);
                int32_t *re32 = (int32_t *)&re;
                int32_t *im32 = (int32_t *)&im;
		c32_t prod = (c32_t){re32[0],im32[0]};
                csum(sum_of_prod[group][cd][aa], sum_of_prod[group][cd][aa], prod);
#ifdef DEBUG_NR_PUCCH_RX
                printf("pucch fmt 3 cw %d symb %d group %d aa %d: (%d,%d), prod (%d,%d) sum_of_prod[%d][%d][%d] (%lld,%lld)\n",
                       cw,
		       symb,
                       group,
                       aa,
                       corr32[dmrspos[cd]][group][aa].r,
                       corr32[dmrspos[cd]][group][aa].i,
		       prod.r,prod.i,
		       group,cd,aa,
                       sum_of_prod[group][cd][aa].r,
                       sum_of_prod[group][cd][aa].i);
#endif
	      } //group
	    } // symb loop
  	  } // aa loop
      } //fmt==3/4
// non-coherent combining
      for (int group = 0 ; group < ngroup ; group++)
        for (int aa = 0 ; aa < Prx ; aa++) {
            corr_tmp += squaredMod(sum_of_prod[group][0][aa]);
            if (fmt > 2 || (fmt == 2 && pucch_pdu->freq_hop_flag > 0)) 
	       corr_tmp += squaredMod(sum_of_prod[group][1][aa]);
#ifdef DEBUG_NR_PUCCH_RX
	    if (fmt == 2 && pucch_pdu->freq_hop_flag == 0)
              printf("sum_of_prod[%d][0][%d] (%lld,%lld)\n",group,aa,sum_of_prod[group][0][aa].r,sum_of_prod[group][0][aa].i);
	    else 
              printf("sum_of_prod[%d][0][%d] (%lld,%lld) sum_of_prod[%d][1][%d] (%lld,%lld)\n",group,aa,sum_of_prod[group][0][aa].r,sum_of_prod[group][0][aa].i,group,aa,sum_of_prod[group][1][aa].r,sum_of_prod[group][1][aa].i);
            printf("corr_tmp %lld\n",corr_tmp);
#endif
        }
      if (corr_tmp > corr) {
        corr = corr_tmp;
        cw_ML = cw;
#ifdef DEBUG_NR_PUCCH_RX
        printf("slot %d PUCCH2 cw_ML %d, corr %lu\n", slot, cw_ML, corr);
#endif
      }
    } // cw loop
    corr_dB = dB_fixed64(corr);
#ifdef DEBUG_NR_PUCCH_RX
    printf("slot %d PUCCH2 cw_ML %d, metric %d \n", slot, cw_ML, corr_dB);
#endif
    decodedPayload[0] = (uint64_t)cw_ML;

  } else if (nb_bit >= 12) { // polar coded case

    simde__m128i llrs[pucch_pdu->prb_size * 2 * nb_symbols];
    // non-coherent LLR computation on groups of 4 REs (half-PRBs)
    uint64_t corr = 0;
    const simde__m128i ones = simde_mm_set1_epi16(1);
    for (int symb = 0; symb < nb_symbols; symb++) {
      for (int half_prb = 0; half_prb < (2 * pucch_pdu->prb_size); half_prb++) {
	int group = (6*half_prb)/(12*nc_group_size);
        simde__m128i llr_num = simde_mm_set1_epi16(0);
        simde__m128i llr_den = simde_mm_set1_epi16(0);
        for (int cw = 0; cw < 256; cw++) {
          int64_t corr_tmp = 0;
          for (int aa = 0; aa < Prx; aa++) {
            simde__m128i part1 = simde_mm_set_epi64x(0ULL, *(int64_t *)&pucch2_3_polar_4bit[cw & 15].cw);
            simde__m128i part2 = simde_mm_set_epi64x(0ULL, *(int64_t *)&pucch2_3_polar_4bit[cw >> 4].cw);
            simde__m128i factor = simde_mm_unpacklo_epi16(part1, part2);
            simde__m128i re = *(simde__m128i *)&r_ext[aa][symb][half_prb * 4];
            simde__m128i im = *(simde__m128i *)&r_ext2[aa][symb][half_prb * 4];
            simde__m128i prod_re = simde_mm_madd_epi16(re, factor);
            simde__m128i prod_im = simde_mm_madd_epi16(im, factor);
            prod_re = simde_mm_hadd_epi32(prod_re, prod_re);
            prod_im = simde_mm_hadd_epi32(prod_im, prod_im);
            prod_re = simde_mm_hadd_epi32(prod_re, prod_re);
            prod_im = simde_mm_hadd_epi32(prod_im, prod_im);
            simde__m128i prod = simde_mm_srai_epi32(simde_mm_unpacklo_epi32(prod_re, prod_im), 5);
            c64_t corr64 = (c64_t){corr32[symb][group][aa].r / (2 * nc_group_size * 4 / 2),
                                   corr32[symb][group][aa].i / (2 * nc_group_size * 4 / 2)};
            //  _mm_srai_epi64 is missing in SIMDE package, we need to update it
            c64_t prod2 = {simde_mm_extract_epi32(prod, 0), simde_mm_extract_epi32(prod, 1)};
            csum(prod2, prod2, corr64);
            corr_tmp += squaredMod(prod2) >> (Prx / 2);
            // this is for UL CQI measurement
            if (cw == 0)
              corr += squaredMod(corr32[symb][group][aa]);
          }
          simde__m128i corr16 = simde_mm_set1_epi16((int16_t)(corr_tmp >> 8));
          simde__m128i den = simde_mm_xor_si128(pucch2_3_polar_llr_num_lut[cw], ones);
          llr_num = simde_mm_max_epi16(simde_mm_mullo_epi16(corr16, pucch2_3_polar_llr_num_lut[cw]), llr_num);
          llr_den = simde_mm_max_epi16(simde_mm_mullo_epi16(corr16, den), llr_den);
        }
        // compute llrs
        llrs[half_prb + symb * 2 * pucch_pdu->prb_size] = simde_mm_subs_epi16(llr_num, llr_den);
        LOG_DDUMP(PHY, llrs + half_prb + symb * 2 * pucch_pdu->prb_size, 8, LOG_DUMP_I16, "llrs:");
      } // half_prb
    } // symb

    // run polar decoder on llrs
    decoderState =
        polar_decoder_int16((int16_t *)llrs, decodedPayload, 0, NR_POLAR_UCI_PUCCH_MESSAGE_TYPE, nb_bit, pucch_pdu->prb_size);

    // Decoder reversal
    decodedPayload[0] = reverse_bits(decodedPayload[0], nb_bit);

    if (decoderState > 0)
      decoderState = 1;
    corr_dB = dB_fixed64(corr);
    LOG_D(PHY, "metric %d dB\n", corr_dB);
  } else
    LOG_D(PHY, "PUCCH not processed: nb_bit %d decoderState %d\n", nb_bit, decoderState);

  LOG_D(PHY,"UCI decoderState %d, payload[0] %llu\n", decoderState, (unsigned long long)decodedPayload[0]);

  // estimate CQI for MAC (from antenna port 0 only)
  // TODO this computation is wrong -> to be ignored at MAC for now
  int cqi = 0xff;
  /*int SNRtimes10 =
    dB_fixed_times10(signal_energy_nodc((int32_t *)&rxdataF[0][soffset + (l2 * frame_parms->ofdm_symbol_size) + re_offset[0]],
    12 * pucch_pdu->prb_size))
    - (10 * gNB->measurements.n0_power_tot_dB);
    int cqi,bit_left;
    if (SNRtimes10 < -640) cqi=0;
    else if (SNRtimes10 >  635) cqi=255;
    else cqi=(640+SNRtimes10)/5;*/

  uci_pdu->harq.harq_bit_len = pucch_pdu->bit_len_harq;
  uci_pdu->pduBitmap = 0;
  uci_pdu->rnti = pucch_pdu->rnti;
  uci_pdu->handle = pucch_pdu->handle;
  uci_pdu->pucch_format = 0;
  uci_pdu->ul_cqi = cqi;
  uci_pdu->timing_advance = 0xffff; // currently not valid
  uci_pdu->rssi =
      1280
      - (10 * dB_fixed(32767 * 32767)
         - dB_fixed_times10(signal_energy_nodc(&rxdataF[0][soffset + (l2 * frame_parms->ofdm_symbol_size) + re_offset[0]],
                                               12 * pucch_pdu->prb_size)));
  if (pucch_pdu->bit_len_harq > 0) {
    int harq_bytes = pucch_pdu->bit_len_harq >> 3;
    if ((pucch_pdu->bit_len_harq & 7) > 0)
      harq_bytes++;
    uci_pdu->pduBitmap |= 2;
    uci_pdu->harq.harq_payload = (uint8_t *)malloc(harq_bytes);
    uci_pdu->harq.harq_crc = decoderState;
    LOG_D(PHY, "[DLSCH/PDSCH/PUCCH2] %d.%d HARQ bytes (%d) Decoder state %d\n", frame, slot, harq_bytes, decoderState);
    int i = 0;
    for (; i < harq_bytes - 1; i++) {
      uci_pdu->harq.harq_payload[i] = decodedPayload[0] & 255;
      LOG_D(PHY, "[DLSCH/PDSCH/PUCCH2] %d.%d HARQ payload (%d) = %d\n", frame, slot, i, uci_pdu->harq.harq_payload[i]);
      decodedPayload[0] >>= 8;
    }
    int bit_left = pucch_pdu->bit_len_harq - ((harq_bytes - 1) << 3);
    uci_pdu->harq.harq_payload[i] = decodedPayload[0] & ((1 << bit_left) - 1);
    LOG_D(PHY, "[DLSCH/PDSCH/PUCCH2] %d.%d HARQ payload (%d) = %d\n", frame, slot, i, uci_pdu->harq.harq_payload[i]);
    decodedPayload[0] >>= pucch_pdu->bit_len_harq;
  }

  if (pucch_pdu->sr_flag == 1) {
    uci_pdu->pduBitmap |= 1;
    uci_pdu->sr.sr_bit_len = 1;
    uci_pdu->sr.sr_payload = malloc(1);
    uci_pdu->sr.sr_payload[0] = decodedPayload[0] & 1;
    decodedPayload[0] = decodedPayload[0] >> 1;
  }
  // csi
  if (pucch_pdu->bit_len_csi_part1 > 0) {
    uci_pdu->pduBitmap |= 4;
    uci_pdu->csi_part1.csi_part1_bit_len = pucch_pdu->bit_len_csi_part1;
    int csi_part1_bytes = pucch_pdu->bit_len_csi_part1 >> 3;
    if ((pucch_pdu->bit_len_csi_part1 & 7) > 0)
      csi_part1_bytes++;
    uci_pdu->csi_part1.csi_part1_payload = (uint8_t *)malloc(csi_part1_bytes);
    uci_pdu->csi_part1.csi_part1_crc = decoderState;
    int i = 0;
    for (; i < csi_part1_bytes - 1; i++) {
      uci_pdu->csi_part1.csi_part1_payload[i] = decodedPayload[0] & 255;
      decodedPayload[0] >>= 8;
    }
    int bit_left = pucch_pdu->bit_len_csi_part1 - ((csi_part1_bytes - 1) << 3);
    uci_pdu->csi_part1.csi_part1_payload[i] = decodedPayload[0] & ((1 << bit_left) - 1);
    decodedPayload[0] = pucch_pdu->bit_len_csi_part1 < 64 ? decodedPayload[0] >> bit_left : 0;
  }

  if (pucch_pdu->bit_len_csi_part2 > 0) {
    uci_pdu->pduBitmap |= 8;
  }
}
