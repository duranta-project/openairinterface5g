/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>
#include "xran_fh_o_du.h"
#include "xran_compression.h"
#include "armral_bfp_compression.h"

#if defined(__arm__) || defined(__aarch64__)
#else
// xran_cp_api.h uses SIMD, but does not include it
#include <immintrin.h>
#endif
#include "xran_cp_api.h"
#include "xran_sync_api.h"
#include "oran_isolate.h"
#include "oran-init.h"
#include "oaioran.h"
#include <rte_ethdev.h>

#include "oran-config.h" // for g_kbar

#define N_SC_PER_PRB 12

int xran_is_prach_slot(uint8_t PortId, uint32_t subframe_id, uint32_t slot_id, uint8_t mu);
#include "common/utils/LOG/log.h"

/* Prints TX_TOTAL, RX_TOTAL, RX_ON_TIME, RX_ERR_DROP counters every 128 frames. */
void print_fhi_counters(const int xran_port, const uint8_t nb_ant, const int frame, const int slot)
{
  static int64_t old_rx_counter[XRAN_PORTS_NUM] = {0};
  static int64_t old_tx_counter[XRAN_PORTS_NUM] = {0};
  struct xran_common_counters x_counters[XRAN_PORTS_NUM];

  if ((frame & 0x7f) == 0 && slot == 0 && xran_get_common_counters(gxran_handle[xran_port], &x_counters[xran_port]) == XRAN_STATUS_SUCCESS) {
    LOG_I(HW,
          "[%s%d][rx %7ld pps %7ld kbps %7ld][tx %7ld pps %7ld kbps %7ld][Total Msgs_Rcvd %ld]\n",
          "o-du ",
          xran_port,
          x_counters[xran_port].rx_counter,
          x_counters[xran_port].rx_counter - old_rx_counter[xran_port],
          x_counters[xran_port].rx_bytes_per_sec * 8 / 1000L,
          x_counters[xran_port].tx_counter,
          x_counters[xran_port].tx_counter - old_tx_counter[xran_port],
          x_counters[xran_port].tx_bytes_per_sec * 8 / 1000L,
          x_counters[xran_port].Total_msgs_rcvd);
    LOG_I(HW,
          "[%s%d][RX Timing][on time: %7lu, early %7lu late %7lu corrupt %7lu, duplicated %7lu]\n",
          "o_du",
          xran_port,
          x_counters[xran_port].Rx_on_time,
          x_counters[xran_port].Rx_early,
          x_counters[xran_port].Rx_late,
          x_counters[xran_port].Rx_corrupt,
          x_counters[xran_port].Rx_pkt_dupl);
    for (int rxant = 0; rxant < nb_ant; rxant++)
      LOG_I(HW,
            "[%s%d][pusch%d %7ld prach%d %7ld]\n",
            "o_du",
            xran_port,
            rxant,
            x_counters[xran_port].rx_pusch_packets[rxant],
            rxant,
            x_counters[xran_port].rx_prach_packets[rxant]);
    LOG_I(HW,
          "[%s%d][drop errors %7d ecpri errors %7d cp errors %7d up errors %7d pusch errors %7d prach errors %7d]\n",
	  "o_du",
          xran_port,
          x_counters[xran_port].rx_err_drop,
          x_counters[xran_port].rx_err_ecpri,
          x_counters[xran_port].rx_err_cp,
          x_counters[xran_port].rx_err_up,
          x_counters[xran_port].rx_err_pusch,
          x_counters[xran_port].rx_err_prach);
    if (x_counters[xran_port].rx_counter > old_rx_counter[xran_port])
      old_rx_counter[xran_port] = x_counters[xran_port].rx_counter;
    if (x_counters[xran_port].tx_counter > old_tx_counter[xran_port])
      old_tx_counter[xran_port] = x_counters[xran_port].tx_counter;
  }
}

/** @details xran-specific callback, called when all packets for given CC and
 * 1/4, 1/2, 3/4, all symbols of a slot arrived. Currently, only used to get
 * timing information and unblock another thread in xran_fh_rx_read_slot()
 * through a message queue on a full slot boundary. */
void oai_xran_fh_rx_callback(void *pCallbackTag, xran_status_t status, uint8_t mu)
{
  struct xran_cb_tag *callback_tag = (struct xran_cb_tag *)pCallbackTag;

  const int slots_in_sf = 1 << mu;
  const int sf_in_frame = 10;

  uint32_t tti = callback_tag->slotiId;
  uint32_t frame = XranGetFrameNum(tti, 0, sf_in_frame, slots_in_sf);
  uint32_t subframe = XranGetSubFrameNum(tti, slots_in_sf, sf_in_frame);
  uint32_t slot = XranGetSlotNum(tti, slots_in_sf);

  uint32_t rx_sym = callback_tag->symbol & 0xFF;
  uint32_t ru_id = callback_tag->oXuId;

  LOG_D(HW, "rx_callback at %4d.%3d (subframe %d), rx_sym %d ru_id %d\n", frame, slot, subframe, rx_sym, ru_id);

  if (rx_sym == 7) { // in F release this value is defined as XRAN_FULL_CB_SYM (full slot (offset + 7))
    oran_buf_list_t *bufs = get_xran_buffers(ru_id);
    struct xran_fh_config *fh_config = get_xran_fh_config(ru_id);
    for (uint16_t cc_id = 0; cc_id < 1 /* fh_config->nCC */; cc_id++) { // OAI does not support multiple CC yet.
      for(uint32_t ant_id = 0; ant_id < fh_config->neAxcUl; ant_id++) {
        struct xran_prb_map *pRbMap = (struct xran_prb_map *)bufs->dstcp[ant_id][tti % XRAN_N_FE_BUF_LEN].pBuffers->pData;
        AssertFatal(pRbMap != NULL, "(%d:%d:%d)pRbMap == NULL. Aborting.\n", cc_id, tti % XRAN_N_FE_BUF_LEN, ant_id);
        for (uint32_t sym_id = 0; sym_id < XRAN_NUM_OF_SYMBOL_PER_SLOT; sym_id++) {
          pRbMap->sFrontHaulRxPacketCtrl[sym_id].nRxPkt = 0;
        }
      }
    }
  }
}

void oai_xran_fh_rx_prach_callback(void *pCallbackTag, xran_status_t status, uint8_t mu)
{
  struct xran_cb_tag *callback_tag = (struct xran_cb_tag *)pCallbackTag;

  const int slots_in_sf = 1 << mu;
  const int sf_in_frame = 10;

  uint32_t tti = callback_tag->slotiId;
  uint32_t frame = XranGetFrameNum(tti, 0, sf_in_frame, slots_in_sf);
  uint32_t subframe = XranGetSubFrameNum(tti, slots_in_sf, sf_in_frame);
  uint32_t slot = XranGetSlotNum(tti, slots_in_sf);

  uint32_t rx_sym = callback_tag->symbol & 0xFF;
  uint32_t ru_id = callback_tag->oXuId;

  LOG_D(HW, "prach_rx_callback at %4d.%3d (subframe %d), rx_sym %d ru_id %d\n", frame, slot, subframe, rx_sym, ru_id);

  if (rx_sym == 7) { // in F release this value is defined as XRAN_FULL_CB_SYM (full slot (offset + 7))
    oran_buf_list_t *bufs = get_xran_buffers(ru_id);
    struct xran_fh_config *fh_config = get_xran_fh_config(ru_id);
    for (uint16_t cc_id = 0; cc_id < 1 /* fh_config->nCC */; cc_id++) { // OAI does not support multiple CC yet.
      for(uint32_t ant_id = 0; ant_id < fh_config->neAxcUl; ant_id++) {
        struct xran_prb_map *pRbMap = (struct xran_prb_map *)bufs->prachdstdecomp[ant_id][tti % XRAN_N_FE_BUF_LEN].pBuffers->pData;
        AssertFatal(pRbMap != NULL, "(%d:%d:%d)pRbMapPrach == NULL. Aborting.\n", cc_id, tti % XRAN_N_FE_BUF_LEN, ant_id);
        for (uint32_t sym_id = 0; sym_id < XRAN_NUM_OF_SYMBOL_PER_SLOT; sym_id++) {
          pRbMap->sFrontHaulRxPacketCtrl[sym_id].nRxPkt = 0;
        }
      }
    }
  }
}

/** @details Read PRACH data from xran buffers.
 * If I/Q compression (bitwidth < 16 bits) is configured, decompresses the data
 * before writing.
 *
 * Function is blocking and waits for next frame/slot combination. It is unblocked
 * by oai_xran_fh_rx_prach_callback(). It writes the current slot into parameters frame/slot. */
int xran_fh_rx_prach_read_slot(const int tti, const int xran_port, const uint8_t nb_ant, const struct xran_ru_config *ru_conf, const struct xran_prach_config *prach_conf, oran_buf_list_t *bufs, prach_item_t p)
{
  for (int aa = p.ant_start; aa < p.ant_start + nb_ant; aa++) {
    for (uint8_t sym_idx = prach_conf->startSymId; sym_idx <= prach_conf->lastSymId; sym_idx++) {
      int16_t *dst, *src;
      int idx = 0;
      // hardcoded to use only first prach occasion
      dst = (int16_t *)p.prach_buf[aa + (xran_port * nb_ant) - p.ant_start][0];
      struct xran_prb_map * pPrbMap = (struct xran_prb_map *)bufs->prachdstdecomp[aa - p.ant_start][tti % XRAN_N_FE_BUF_LEN].pBuffers->pData;
      struct xran_rx_packet_ctl *p_rx_packet_ctl = &pPrbMap->sFrontHaulRxPacketCtrl[sym_idx];
      uint16_t num_prbu = p_rx_packet_ctl->nRBSize[0]; // or prach_conf->numPrbc
      uint16_t N_ZC = (num_prbu <= 12) ? 139 : 839;
      int32_t nRxPkt = p_rx_packet_ctl->nRxPkt;
      if (nRxPkt == 0) {
        LOG_E(HW, "tti %d xran_port %d ant_id %d sym_idx %d nRxPkt %d (expected 1 packet)\n", tti, xran_port, aa, sym_idx, nRxPkt);
        memset(&dst[sym_idx], 0, N_ZC * 2 * sizeof(*dst));
        continue;
      } else {
        src = (int16_t *)p_rx_packet_ctl->pData[0];
        if (src == NULL) { // protection
          LOG_E(HW, "tti %d xran_port %d ant_id %d sym_idx %d src = NULL\n", tti, xran_port, aa, sym_idx);
          memset(&dst[sym_idx], 0, N_ZC * 2 * sizeof(*dst));
          continue;
        }
      }
      /* convert Network order to host order */
      if (ru_conf->compMeth_PRACH == XRAN_COMPMETHOD_NONE) {
        if (sym_idx == prach_conf->startSymId) {
          for (idx = 0; idx < N_ZC * 2; idx++) {
            dst[idx] = ((int16_t)ntohs(src[idx + g_kbar]));
          }
        } else {
          for (idx = 0; idx < N_ZC * 2; idx++) {
            dst[idx] += ((int16_t)ntohs(src[idx + g_kbar]));
          }
        }
      } else if (ru_conf->compMeth_PRACH == XRAN_COMPMETHOD_BLKFLOAT) {
        int16_t local_dst[num_prbu * 2 * N_SC_PER_PRB] __attribute__((aligned(64)));

#if defined(__i386__) || defined(__x86_64__)
        struct xranlib_decompress_request bfp_decom_req = {};
        struct xranlib_decompress_response bfp_decom_rsp = {};
        int payload_len = (3 * ru_conf->iqWidth_PRACH + 1) * num_prbu;

        bfp_decom_req.data_in = (int8_t *)src;
        bfp_decom_req.numRBs = num_prbu;
        bfp_decom_req.len = payload_len;
        bfp_decom_req.compMethod = XRAN_COMPMETHOD_BLKFLOAT;
        bfp_decom_req.iqWidth = ru_conf->iqWidth_PRACH;

        bfp_decom_rsp.data_out = (int16_t *)local_dst;
        bfp_decom_rsp.len = 0;
        xranlib_decompress_avx512(&bfp_decom_req, &bfp_decom_rsp);
#elif defined(__arm__) || defined(__aarch64__)
        armral_bfp_decompression(ru_conf->iqWidth_PRACH, num_prbu, (int8_t *)src, (int16_t *)local_dst);
#else
        AssertFatal(1 == 0, "BFP decompression not supported on this architecture");
#endif
        if (sym_idx == prach_conf->startSymId)
          for (idx = 0; idx < (N_ZC * 2); idx++)
            dst[idx] = local_dst[idx + g_kbar];
        else
          for (idx = 0; idx < (N_ZC * 2); idx++)
            dst[idx] += (local_dst[idx + g_kbar]);
      } // COMPMETHOD_BLKFLOAT
    } // sym_idx
  } // aa

  return (0);
}

/** @brief Check if symbol in slot is UL.
 *
 * @param frame_conf xran frame configuration
 * @param slot the current (absolute) slot (number)
 * @param sym_idx the current symbol index */
static bool is_tdd_ul_symbol(const struct xran_frame_config *frame_conf, int slot, int sym_idx)
{
  int tdd_period = frame_conf->nTddPeriod;
  int slot_in_period = slot % tdd_period;
  /* check if symbol is UL */
  return frame_conf->sSlotConfig[slot_in_period].nSymbolType[sym_idx] == 1 /* UL */;
}

/** @brief Check if current slot is DL or guard/mixed without UL (i.e., current
 * slot is not UL). */
bool is_tdd_dl_guard_slot(const struct xran_frame_config *frame_conf, int slot)
{
  return !is_tdd_ul_symbol(frame_conf, slot, 0);
}

/** @brief Check if current slot is UL or guard/mixed without DL (i.e., current
 * slot is not UL). */
bool is_tdd_ul_guard_slot(const struct xran_frame_config *frame_conf, int slot)
{
  return is_tdd_ul_symbol(frame_conf, slot, XRAN_NUM_OF_SYMBOL_PER_SLOT - 1);
}

/** @details Read PUSCH data from xran buffers.
 * If I/Q compression (bitwidth < 16 bits) is configured, decompresses the data
 * before writing.
 *
 * Function is blocking and waits for next frame/slot combination. It is unblocked
 * by oai_xran_fh_rx_callback(). It writes the current slot into parameters
 * frame/slot. */
int xran_fh_rx_read_slot(const int tti, const int xran_port, const uint8_t nb_ant, const int fft_size, oran_buf_list_t *bufs, int32_t **rxdataF)
{
  void *ptr = NULL;
  int32_t *pos = NULL;
  int idx = 0;

  static int outcnt = 0;

  int slot_offset_rxdata = 3 & (tti % 20); // to finalize
  uint32_t slot_size = 4 * 14 * fft_size;
  uint8_t *start_ptr = NULL;
  for (uint8_t ant_id = 0; ant_id < nb_ant; ant_id++) {
    uint8_t *rx_data = (uint8_t *)rxdataF[ant_id + (xran_port * nb_ant)];
    start_ptr = rx_data + (slot_size * slot_offset_rxdata);

    struct xran_prb_map *pPrbMap = (struct xran_prb_map *)bufs->dstcp[ant_id][tti % XRAN_N_FE_BUF_LEN].pBuffers->pData;
    struct xran_prb_elm *pRbElm = &pPrbMap->prbMap[0];

    for (int32_t sym_idx = pRbElm->nStartSymb; sym_idx < pRbElm->nStartSymb + pRbElm->numSymb; sym_idx++) {

      uint8_t *src = (uint8_t *)ptr;

      struct xran_rx_packet_ctl *p_rx_packet_ctl = &pPrbMap->sFrontHaulRxPacketCtrl[sym_idx];
      uint32_t one_rb_size =
          (((pRbElm->iqWidth == 0) || (pRbElm->iqWidth == 16)) ? (N_SC_PER_PRB * 2 * 2) : (3 * pRbElm->iqWidth + 1));
      int32_t nRxPkt = p_rx_packet_ctl->nRxPkt;
      LOG_D(HW, "nRxPkt %d\n", nRxPkt);
      for (int pkt_idx = 0; pkt_idx < nRxPkt; pkt_idx++) {
        uint8_t *pData;
        // to finalize - maybe no need for this if condition
        if (9600 < p_rx_packet_ctl->nRBSize[pkt_idx] * one_rb_size)
          pData = bufs->dst[ant_id][tti % XRAN_N_FE_BUF_LEN]
                      .pBuffers[sym_idx % XRAN_NUM_OF_SYMBOL_PER_SLOT]
                      .pData;
        else
          pData = p_rx_packet_ctl->pData[pkt_idx];
        int numRB = p_rx_packet_ctl->nRBSize[pkt_idx];
        int startRB = p_rx_packet_ctl->nRBStart[pkt_idx];
        // num_prbu & start_prbu are for UL U-plane only
        LOG_D(HW, "p_rx_packet_ctl[%d] startRB[%d]:numRB[%d]\n", pkt_idx, startRB, numRB);

        ptr = pData;
        pos = (int32_t *)(start_ptr + (4 * sym_idx * fft_size));
        if (ptr == NULL || pos == NULL)
          continue;
        src = pData;
        if (pRbElm->compMethod == XRAN_COMPMETHOD_NONE) {
          // NOTE: gcc 11 knows how to generate AVX2 for this!
          for (idx = 0; idx < (numRB * N_SC_PER_PRB) * 2; idx++)
            ((int16_t *)pos)[idx + startRB * N_SC_PER_PRB * 2] = ((int16_t)ntohs(((uint16_t *)src)[idx])) >> 2;
        } else if (pRbElm->compMethod == XRAN_COMPMETHOD_BLKFLOAT) {
#if defined(__i386__) || defined(__x86_64__)
          struct xranlib_decompress_request bfp_decom_req = {};
          struct xranlib_decompress_response bfp_decom_rsp = {};

          int16_t payload_len = (3 * pRbElm->iqWidth + 1) * numRB;

          bfp_decom_req.data_in = (int8_t *)src;
          bfp_decom_req.numRBs = numRB;
          bfp_decom_req.len = payload_len;
          bfp_decom_req.compMethod = pRbElm->compMethod;
          bfp_decom_req.iqWidth = pRbElm->iqWidth;

          bfp_decom_rsp.data_out = (int16_t *)(pos + startRB * N_SC_PER_PRB);
          bfp_decom_rsp.len = 0;

          xranlib_decompress_avx512(&bfp_decom_req, &bfp_decom_rsp);
#elif defined(__arm__) || defined(__aarch64__)
          armral_bfp_decompression(pRbElm->iqWidth, numRB, (int8_t *)src, (int16_t *)(pos + startRB * N_SC_PER_PRB));
#else
          AssertFatal(1 == 0, "BFP compression not supported on this architecture");
#endif
              outcnt++;
        } else {
          printf("pRbElm->compMethod == %d is not supported\n", pRbElm->compMethod);
          exit(-1);
        }
      } // pkt_idx
    } // sym_idx
  } // ant_ind

  return (0);
}

// Send CP DL/UL packets
int xran_send_cp_slot(const int tti, const int slot, const int xran_port, const uint8_t nb_ant, uint16_t **beams, struct xran_buffer_list buf[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN])
{
  for (uint8_t ant_id = 0; ant_id < nb_ant; ant_id++) {
    struct xran_prb_map *pPrbMap = (struct xran_prb_map *)buf[ant_id][tti % XRAN_N_FE_BUF_LEN].pBuffers->pData;
    // (1) nPrbElm is the number of fragments; (2) For Liteon FR2 with RunSlotPrbMapBySymbolEnable each idxElm matches to one symbol
    LOG_D(HW, "pPrbMap->nPrbElm %d\n", pPrbMap->nPrbElm);
    for (uint32_t idxElm = 0; idxElm < pPrbMap->nPrbElm; idxElm++) {
      struct xran_prb_elm *pRbElm = &pPrbMap->prbMap[idxElm];
      int numRB = pRbElm->UP_nRBSize;
      int startRB = pRbElm->UP_nRBStart;
      LOG_D(HW, "pPrbMap[%d] : PRBstart %d nPRBs %d\n", idxElm, startRB, numRB);

      for (int32_t sym_idx = pRbElm->nStartSymb; sym_idx < pRbElm->nStartSymb + pRbElm->numSymb; sym_idx++) {
        LOG_D(HW, "ant_id %d sym_idx %d pPrbMap[%d] (startRB %d numRB %d)\n", ant_id, sym_idx, idxElm, startRB, numRB);
        pRbElm->nBeamIndex = beams[slot * XRAN_NUM_OF_SYMBOL_PER_SLOT + sym_idx][ant_id + (xran_port * nb_ant)];
      }
    }
  }
  return (0);
}

/** @brief Compress (if needed) one PRB-element's IQ payload for one symbol into `dst` and
 * return the payload length. Shared by both PRB-map layouts of xran_fh_tx_send_slot(). */
static int16_t xran_compress_iq_section(int32_t *pos_start, uint8_t *dst, int numRB, uint8_t compMethod, uint8_t iqWidth)
{
  int16_t payload_len = 0;

  if (compMethod == XRAN_COMPMETHOD_NONE) {
    payload_len = numRB * N_SC_PER_PRB * 4L;
    /* convert to Network order */
    // NOTE: ggc 11 knows how to generate AVX2 for this!
    for (int idx = 0; idx < (numRB * N_SC_PER_PRB) * 2; idx++)
      ((uint16_t *)dst)[idx] = htons(((uint16_t *)pos_start)[idx]);
  } else if (compMethod == XRAN_COMPMETHOD_BLKFLOAT) {
    payload_len = (3 * iqWidth + 1) * numRB;

    /* Although arm intrinsics natively handle unaligned memory
    access, we use a 64 byte aligned input here for maximum
    performance. So the src_compr buffer is used for both x86 and arm.
    */
    uint32_t src_compr[numRB * N_SC_PER_PRB] __attribute__((aligned(64)));

    /* Copy from txdataF with current symbol's PRB start (nRBStart) +
    current section's PRB start (UP_nPRBStart) */
    memcpy(src_compr, pos_start, (numRB * N_SC_PER_PRB) * sizeof(*pos_start));

#if defined(__i386__) || defined(__x86_64__)
    struct xranlib_compress_request bfp_com_req = {};
    struct xranlib_compress_response bfp_com_rsp = {};

    bfp_com_req.data_in = (int16_t *)src_compr;

    bfp_com_req.numRBs = numRB;
    bfp_com_req.len = payload_len;
    bfp_com_req.compMethod = compMethod;
    bfp_com_req.iqWidth = iqWidth;

    bfp_com_rsp.data_out = (int8_t *)dst;
    bfp_com_rsp.len = 0;

    xranlib_compress_avx512(&bfp_com_req, &bfp_com_rsp);
#elif defined(__arm__) || defined(__aarch64__)
    armral_bfp_compression(iqWidth, numRB, (int16_t *)src_compr, (int8_t *)dst);
#else
    AssertFatal(1 == 0, "BFP compression not supported on this architecture");
#endif
  } else {
    printf("compMethod == %d is not supported\n", compMethod);
    exit(-1);
  }

  return payload_len;
}

/** @brief Write one antenna's IQ payload for one slot into the xran wire buffers, for either
 * PRB-map layout xran_fh_tx_send_slot() can see:
 * - fragmented: nPrbElm is the number of frequency fragments; dst_cursor is carried across
 *   elements per symbol
 * - per-symbol (Liteon FR2, RunSlotPrbMapBySymbolEnable): nPrbElm == XRAN_NUM_OF_SYMBOL_PER_SLOT
 *   and each element maps to exactly one symbol; dst_cursor is a no-op for this case since
 *   each symbol is only ever touched once
 * Both are handled by the same data-driven loop: correctness comes entirely from each
 * element's own nStartSymb/numSymb. */
static void xran_fh_tx_send_slot_ant(struct xran_prb_map *pPrbMap,
                                      int32_t *txdataF_ant,
                                      const int fft_size,
                                      struct xran_buffer_list *src_slot)
{
  uint8_t *dst_cursor[XRAN_NUM_OF_SYMBOL_PER_SLOT] = {NULL};

  for (uint32_t idxElm = 0; idxElm < pPrbMap->nPrbElm; idxElm++) {
    struct xran_prb_elm *p_prbMapElm = &pPrbMap->prbMap[idxElm];
    int16_t start_totalRB = p_prbMapElm->nRBStart;
    int16_t startRB = p_prbMapElm->UP_nRBStart;
    int16_t numRB = p_prbMapElm->UP_nRBSize;

    for (int32_t sym_idx = p_prbMapElm->nStartSymb; sym_idx < p_prbMapElm->nStartSymb + p_prbMapElm->numSymb; sym_idx++) {
      struct xran_section_desc *p_sec_desc = &p_prbMapElm->sec_desc[sym_idx];

      uint8_t *base = src_slot->pBuffers[sym_idx].pData;
      if (dst_cursor[sym_idx] == NULL)
        dst_cursor[sym_idx] = base;

      int32_t *pos = &txdataF_ant[sym_idx * fft_size];
      // Start of this section
      int32_t *pos_start = pos + (start_totalRB + startRB) * N_SC_PER_PRB;

      uint8_t *dst = xran_add_hdr_offset(dst_cursor[sym_idx], p_prbMapElm->compMethod);
      int16_t payload_len = xran_compress_iq_section(pos_start, dst, numRB, p_prbMapElm->compMethod, p_prbMapElm->iqWidth);

      p_sec_desc->iq_buffer_offset = RTE_PTR_DIFF(dst, base);
      p_sec_desc->iq_buffer_len = payload_len;

      dst_cursor[sym_idx] = xran_add_hdr_offset(dst + payload_len, p_prbMapElm->compMethod);
    } // sym_idx
  } // idxElm
}

/** @details Write PDSCH IQ-data from OAI txdataF_BF buffer to xran buffers. If
 * I/Q compression (bitwidth < 16 bits) is configured, compresses the data
 * before writing. */
int xran_fh_tx_send_slot(const int tti, const int xran_port, const uint8_t nb_ant, const int fft_size, int32_t **txdataF_BF, oran_buf_list_t *bufs)
{
  for (uint8_t ant_id = 0; ant_id < nb_ant; ant_id++) {
    struct xran_prb_map *pPrbMap = (struct xran_prb_map *)bufs->srccp[ant_id][tti % XRAN_N_FE_BUF_LEN].pBuffers->pData;

    xran_fh_tx_send_slot_ant(pPrbMap, txdataF_BF[ant_id + (xran_port * nb_ant)], fft_size, &bufs->src[ant_id][tti % XRAN_N_FE_BUF_LEN]);

    // The tti should be updated as it increased.
    pPrbMap->tti_id = tti;
  } // ant_id
  return (0);
}
