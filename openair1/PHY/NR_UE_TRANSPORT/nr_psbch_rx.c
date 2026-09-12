/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "PHY/defs_nr_UE.h"
#include "PHY/CODING/nrPolar_tools/nr_polar_psbch_defs.h"
#include "PHY/CODING/nrPolar_tools/nr_polar_defs.h"
#include "common/utils/LOG/log.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_proto_ue.h"
#include "PHY/TOOLS/phy_scope_interface.h"
#include "PHY/nr_phy_common/inc/nr_phy_common.h"

// #define DEBUG_PSBCH

static void nr_psbch_extract(uint32_t dataF_sz,
                             const c16_t rxdataF[][dataF_sz],
                             const struct complex16 dl_ch_estimates[][dataF_sz],
                             struct complex16 rxdataF_ext[][SL_NR_NUM_PSBCH_DATA_RE_IN_ONE_SYMBOL],
                             struct complex16 dl_ch_estimates_ext[][SL_NR_NUM_PSBCH_DATA_RE_IN_ONE_SYMBOL],
                             uint32_t symbol,
                             const NR_DL_FRAME_PARMS *frame_params)
{
  uint16_t rb;
  uint8_t i, j, aarx;
  struct complex16 *dl_ch0_ext, *rxF_ext;
  const uint8_t nb_rb = SL_NR_NUM_PSBCH_RBS_IN_ONE_SYMBOL;

  AssertFatal((symbol == 0 || symbol >= 5), "SIDELINK: PSBCH DMRS not contained in symbol %d \n", symbol);

  for (aarx = 0; aarx < frame_params->nb_antennas_rx; aarx++) {
    unsigned int rx_offset = frame_params->ssb_start_subcarrier;

    const c16_t *rxF = rxdataF[aarx];
    rxF_ext = rxdataF_ext[aarx];

    const c16_t *dl_ch0 = dl_ch_estimates[aarx];
    dl_ch0_ext = dl_ch_estimates_ext[aarx];

#ifdef DEBUG_PSBCH
    LOG_I(PHY, "extract_rbs: rx_offset=%d, symbol %u\n", (rx_offset + (symbol * frame_params->ofdm_symbol_size)), symbol);
#endif

    for (rb = 0; rb < nb_rb; rb++) {
      j = 0;

      for (i = 0; i < NR_NB_SC_PER_RB; i++) {
        if (i % 4 != 0) {
          rxF_ext[j] = rxF[rx_offset];
          dl_ch0_ext[j] = dl_ch0[i];

#ifdef DEBUG_PSBCH

          LOG_I(PHY,
                "rxF ext[%d] = (%d,%d) rxF [%u]= (%d,%d)\n",
                (9 * rb) + j,
                rxF_ext[j].r,
                rxF_ext[j].i,
                rx_offset,
                rxF[rx_offset].r,
                rxF[rx_offset].i);
          LOG_I(PHY,
                "dl ch0 ext[%d] = (%d,%d)  dl_ch0 [%d]= (%d,%d)\n",
                (9 * rb) + j,
                dl_ch0_ext[j].r,
                dl_ch0_ext[j].i,
                i,
                dl_ch0[i].r,
                dl_ch0[i].i);
#endif
          j++;
        }
        rx_offset++;
      }

      rxF_ext += SL_NR_NUM_PSBCH_DATA_RE_IN_ONE_RB;
      dl_ch0_ext += SL_NR_NUM_PSBCH_DATA_RE_IN_ONE_RB;
      dl_ch0 += NR_NB_SC_PER_RB;
    }

#ifdef DEBUG_PSBCH
    char filename[40], varname[25];
    sprintf(filename, "psbch_dlch_sym_%d.m", symbol);
    sprintf(varname, "psbch_dlch%d.m", symbol);
    LOG_M(filename, varname, (void *)dl_ch0, frame_params->ofdm_symbol_size, 1, 1);
    sprintf(filename, "psbch_dlchext_sym_%d.m", symbol);
    sprintf(varname, "psbch_dlchext%d.m", symbol);
    LOG_M(filename, varname, (void *)&dl_ch_estimates_ext[0][0], SL_NR_NUM_PSBCH_DATA_RE_IN_ONE_SYMBOL, 1, 1);
#endif
  }

  return;
}

void nr_generate_psbch_llr(const NR_DL_FRAME_PARMS *frame_parms,
                           const c16_t rxdataF[][frame_parms->ofdm_symbol_size],
                           const c16_t dl_ch_estimates[][frame_parms->ofdm_symbol_size],
                           int symbol,
                           int *psbch_e_rx_offset,
                           int16_t psbch_e_rx[SL_NR_POLAR_PSBCH_E_NORMAL_CP + 2],
                           int16_t psbch_unClipped[SL_NR_POLAR_PSBCH_E_NORMAL_CP + 2])
{
  const uint16_t nb_re = SL_NR_NUM_PSBCH_DATA_RE_IN_ONE_SYMBOL;
  __attribute__((aligned(32))) struct complex16 rxdataF_ext[frame_parms->nb_antennas_rx][nb_re + 1];
  __attribute__((aligned(32))) struct complex16 dl_ch_estimates_ext[frame_parms->nb_antennas_rx][nb_re + 1];
  // memset(dl_ch_estimates_ext,0, sizeof  dl_ch_estimates_ext);
  nr_psbch_extract(frame_parms->ofdm_symbol_size, rxdataF, dl_ch_estimates, rxdataF_ext, dl_ch_estimates_ext, symbol, frame_parms);
#ifdef DEBUG_PSBCH
  LOG_I(PHY, "PSBCH RX Symbol %d ofdm size %d\n", symbol, frame_parms->ofdm_symbol_size);
#endif

  double log2_maxh = 0;
  int max_h = 0;
  if (symbol == 0) {
    int avg[frame_parms->nb_antennas_rx];
    nr_channel_level(0, PBCH_MAX_RE_PER_SYMBOL, dl_ch_estimates_ext, frame_parms->nb_antennas_rx, avg, nb_re);
    max_h = avg[0];
    for (int i = 1; i < frame_parms->nb_antennas_rx; i++)
      max_h = cmax(avg[i], max_h);
    log2_maxh = 5 + (log2_approx(max_h) / 2); // LLR32 crc error. LLR 16 CRC works
  }
#ifdef DEBUG_PSBCH
  LOG_I(PHY, "PSBCH RX log2_maxh = %f (%d)\n", log2_maxh, max_h);
#endif

  __attribute__((aligned(32))) struct complex16 rxdataF_comp[frame_parms->nb_antennas_rx][nb_re + 1];
  nr_pbch_channel_compensation(rxdataF_ext, dl_ch_estimates_ext, nb_re, rxdataF_comp, frame_parms,
                               log2_maxh); // log2_maxh+I0_shift

  nr_pbch_quantize(psbch_e_rx + *psbch_e_rx_offset, (short *)rxdataF_comp[0], SL_NR_NUM_PSBCH_DATA_BITS_IN_ONE_SYMBOL);
  memcpy(psbch_unClipped + *psbch_e_rx_offset, (short *)rxdataF_comp[0], SL_NR_NUM_PSBCH_DATA_BITS_IN_ONE_SYMBOL);
  *psbch_e_rx_offset += SL_NR_NUM_PSBCH_DATA_BITS_IN_ONE_SYMBOL;
}

int nr_psbch_decode(PHY_VARS_NR_UE *ue,
                    int16_t psbch_e_rx[SL_NR_POLAR_PSBCH_E_NORMAL_CP + 2],
                    const UE_nr_rxtx_proc_t *proc,
                    int psbch_e_rx_len,
                    int slss_id,
                    nr_phy_data_t *phy_data,
                    uint8_t decoded_pdu[4])
{
  // un-scrambling
  LOG_D(PHY, "PSBCH RX POLAR DECODING: total PSBCH bits:%d, rx_slss_id:%d\n", psbch_e_rx_len, slss_id);

  nr_pbch_unscrambling(psbch_e_rx, slss_id, 0, 0, psbch_e_rx_len, 0, 0, 0, NULL);
  // polar decoding de-rate matching
  uint64_t tmp = 0;
  const uint32_t decoderState = polar_decoder_int16(psbch_e_rx,
                                                    (uint64_t *)&tmp,
                                                    0,
                                                    SL_NR_POLAR_PSBCH_MESSAGE_TYPE,
                                                    SL_NR_POLAR_PSBCH_PAYLOAD_BITS,
                                                    SL_NR_POLAR_PSBCH_AGGREGATION_LEVEL);

  uint32_t psbch_payload = tmp;

  if (decoderState) {
    LOG_D(PHY, "%d:%d PSBCH RX: NOK \n", proc->frame_rx, proc->nr_slot_rx);
    return (decoderState);
  }

  // Decoder reversal
  uint32_t a_reversed = 0;

  for (int i = 0; i < SL_NR_POLAR_PSBCH_PAYLOAD_BITS; i++)
    a_reversed |= (((uint64_t)psbch_payload >> i) & 1) << (31 - i);

  psbch_payload = a_reversed;

  *((uint32_t *)decoded_pdu) = psbch_payload;

#ifdef DEBUG_PSBCH
  for (int i = 0; i < 4; i++) {
    LOG_I(PHY, "decoded_output[%d]:%x\n", i, decoded_pdu[i]);
  }
#endif

  ue->symbol_offset = 0;

  // retrieve DFN and slot number from SL-MIB
  uint32_t DFN = 0, slot_offset = 0;
  DFN = (((psbch_payload & 0x0700) >> 1) | ((psbch_payload & 0xFE0000) >> 17));
  slot_offset = (((psbch_payload & 0x010000) >> 10) | ((psbch_payload & 0xFC000000) >> 26));

  LOG_D(PHY, "PSBCH RX SL-MIB:%x, decoded DFN:slot %d:%d, %x\n", psbch_payload, DFN, slot_offset, *(uint32_t *)decoded_pdu);

  nr_sidelink_indication_t sl_indication;
  sl_nr_rx_indication_t rx_ind = {0};
  uint16_t number_pdus = 1;

  sl_nr_ue_phy_params_t *sl_phy_params = &ue->SL_UE_PHY_PARAMS;
  uint16_t rx_slss_id = sl_phy_params->sl_config.sl_sync_source.rx_slss_id;

  uint8_t *result = NULL;
  result = decoded_pdu;
  sl_phy_params->psbch.rx_ok++;
  LOG_I(NR_PHY,
        "[UE%d] %d:%d PSBCH RX:OK. RSRP: %d dB/RE\n",
        ue->Mod_id,
        proc->frame_rx,
        proc->nr_slot_rx,
        sl_phy_params->psbch.rsrp_dB_per_RE);

  nr_fill_sl_indication(&sl_indication, &rx_ind, NULL, proc, ue, phy_data);
  nr_fill_sl_rx_indication(&rx_ind, SL_NR_RX_PDU_TYPE_SSB, ue, number_pdus, (void *)result, rx_slss_id);

  if (ue->if_inst && ue->if_inst->sl_indication)
    ue->if_inst->sl_indication(&sl_indication);

  return 0;
}
