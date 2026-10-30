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
#include "common/utils/threadPool/notified_fifo.h"
#include <rte_ethdev.h>


#define N_SC_PER_PRB 12

/* Must stay registered: xran only enables U-plane RX and C-plane (its internal first_call, set at the
 * first SFN wrap in tti_to_phy_cb()) if a TTI callback exists. */
int oai_physide_dl_tti_call_back(void *param, uint8_t mu)
{
  return 0;
}

int xran_is_prach_slot(uint8_t PortId, uint32_t subframe_id, uint32_t slot_id, uint8_t mu);
#include "common/utils/LOG/log.h"

/* Prints TX_TOTAL, RX_TOTAL, RX_ON_TIME, RX_ERR_DROP counters every 128 frames. */
void print_fhi_counters(struct xran_fh_init fh_init, struct xran_fh_config *fh_config)
{
  static int64_t old_rx_counter[XRAN_PORTS_NUM] = {0};
  static int64_t old_tx_counter[XRAN_PORTS_NUM] = {0};
  struct xran_common_counters x_counters[XRAN_PORTS_NUM];

  for (int xran_port = 0; xran_port < fh_init.xran_ports; xran_port++) {
    if (xran_get_common_counters(gxran_handle[xran_port], &x_counters[xran_port]) == XRAN_STATUS_SUCCESS) {
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
            "o-du ",
            xran_port,
            x_counters[xran_port].Rx_on_time,
            x_counters[xran_port].Rx_early,
            x_counters[xran_port].Rx_late,
            x_counters[xran_port].Rx_corrupt,
            x_counters[xran_port].Rx_pkt_dupl);
      for (int rxant = 0; rxant < fh_config[xran_port].neAxcUl; rxant++)
        LOG_I(HW,
              "[%s%d][pusch%d %7ld prach%d %7ld]\n",
              "o-du ",
              xran_port,
              rxant,
              x_counters[xran_port].rx_pusch_packets[rxant],
              rxant,
              x_counters[xran_port].rx_prach_packets[rxant]);
      LOG_I(HW,
            "[%s%d][drop errors %7d ecpri errors %7d cp errors %7d up errors %7d pusch errors %7d prach errors %7d]\n",
	    "o-du ",
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
}

#define MAX_SLOT_BACKLOG 3 // beyond this, skip to the newest slot

/* One element per slot complete on all xran ports, pulled by split7_thread() */
static notifiedFIFO_t slot_fifo;
static atomic_int slot_fifo_len;
/* Ports may have different UL deadlines (per-RU Ta4_max): signal a slot once all ports completed it.
 * Not atomic: xran runs all ports' deadline callbacks on one lcore (worker_core[0], or the timing
 * core without workers). */
static bool port_done[XRAN_PORTS_NUM];
static int num_ports_done;
static uint32_t ports_tti; // slot that port_done/num_ports_done refer to

static void reset_ports_done(void)
{
  memset(port_done, 0, sizeof(port_done));
  num_ports_done = 0;
}

void oai_xran_slot_sync_init(void)
{
  initNotifiedFIFO(&slot_fifo);
  atomic_store(&slot_fifo_len, 0);
  reset_ports_done();
}

static void slot_ready(uint32_t tti, uint32_t ru_id)
{
  DevAssert(ru_id < XRAN_PORTS_NUM);
  const int num_ports = get_xran_fh_init()->xran_ports;
  if (tti != ports_tti) {
    if (num_ports_done > 0)
      LOG_W(HW, "slot tti %u completed on %d of %d xran ports, dropped\n", ports_tti, num_ports_done, num_ports);
    ports_tti = tti;
    reset_ports_done();
  }
  if (port_done[ru_id])
    return;
  port_done[ru_id] = true;
  if (++num_ports_done < num_ports)
    return;
  reset_ports_done();

  notifiedFIFO_elt_t *e = newNotifiedFIFO_elt(sizeof(oai_xran_slot_t), 0, NULL, NULL);
  *(oai_xran_slot_t *)NotifiedFifoData(e) = (oai_xran_slot_t){.tti = tti};
  atomic_fetch_add(&slot_fifo_len, 1);
  pushNotifiedFIFO(&slot_fifo, e);
}

void oai_xran_wait_slot(oai_xran_slot_t *s)
{
  notifiedFIFO_elt_t *e = pullNotifiedFIFO(&slot_fifo);
  atomic_fetch_sub(&slot_fifo_len, 1);

  if (atomic_load(&slot_fifo_len) >= MAX_SLOT_BACKLOG) {
    const oai_xran_slot_t old = *(oai_xran_slot_t *)NotifiedFifoData(e);
    notifiedFIFO_elt_t *f;
    while ((f = pollNotifiedFIFO(&slot_fifo)) != NULL) {
      atomic_fetch_sub(&slot_fifo_len, 1);
      delNotifiedFIFO_elt(e);
      e = f;
    }
    const oai_xran_slot_t *cur = NotifiedFifoData(e);
    LOG_W(HW, "split 7.2 UL processing late, skipping %d => %d\n", old.tti, cur->tti);
  }

  *s = *(oai_xran_slot_t *)NotifiedFifoData(e);
  delNotifiedFIFO_elt(e);
}

/** @details xran-specific callback, called when all packets for given CC and
 * 1/4, 1/2, 3/4, all symbols of a slot arrived. On a full slot boundary, clears
 * the PUSCH packet counters of the previous slot and signals the slot to
 * split7_thread() once all xran ports completed it. */
void oai_xran_fh_rx_callback(void *pCallbackTag, xran_status_t status, uint8_t mu)
{
  struct xran_cb_tag *callback_tag = (struct xran_cb_tag *)pCallbackTag;

  uint32_t tti = callback_tag->slotiId;
  uint32_t cb_sym = callback_tag->symbol & 0xFF;
  uint32_t port_id = callback_tag->oXuId;

  LOG_D(HW, "rx_callback: tti[%d] cb_sym[%d] port_id[%d]\n", tti, cb_sym, port_id);

  uint32_t prev_tti = tti + XRAN_N_FE_BUF_LEN - 1;
  if (cb_sym == XRAN_FULL_CB_SYM) {
    oran_buf_list_t *bufs = get_xran_buffers(port_id);
    struct xran_fh_config *fh_config = get_xran_fh_config(port_id);
    for (uint16_t cc_id = 0; cc_id < 1 /* fh_config->nCC */; cc_id++) { // OAI does not support multiple CC yet.
      for(uint32_t ant_id = 0; ant_id < fh_config->neAxcUl; ant_id++) {
        struct xran_prb_map *pRbMap = (struct xran_prb_map *)bufs->dstcp[ant_id][prev_tti % XRAN_N_FE_BUF_LEN].pBuffers->pData;
        AssertFatal(pRbMap != NULL, "(%d:%d:%d)pRbMap == NULL. Aborting.\n", cc_id, prev_tti % XRAN_N_FE_BUF_LEN, ant_id);
        for (uint32_t sym_id = 0; sym_id < XRAN_NUM_OF_SYMBOL_PER_SLOT; sym_id++) {
          pRbMap->sFrontHaulRxPacketCtrl[sym_id].nRxPkt = 0;
        }
      }
    }
    // UL deadline of this slot passed: its PUSCH and PRACH data are complete
    slot_ready(tti, port_id);
  }
}

void oai_xran_fh_rx_prach_callback(void *pCallbackTag, xran_status_t status, uint8_t mu)
{
  struct xran_cb_tag *callback_tag = (struct xran_cb_tag *)pCallbackTag;

  uint32_t tti = callback_tag->slotiId;
  uint32_t cb_sym = callback_tag->symbol & 0xFF;
  uint32_t port_id = callback_tag->oXuId;

  LOG_D(HW, "prach_rx_callback: tti[%d] cb_sym[%d] port_id[%d]\n", tti, cb_sym, port_id);

  uint32_t prev_tti = tti + XRAN_N_FE_BUF_LEN - 1;
  if (cb_sym == XRAN_FULL_CB_SYM) {
    oran_buf_list_t *bufs = get_xran_buffers(port_id);
    struct xran_fh_config *fh_config = get_xran_fh_config(port_id);
    for (uint16_t cc_id = 0; cc_id < 1 /* fh_config->nCC */; cc_id++) { // OAI does not support multiple CC yet.
      for(uint32_t ant_id = 0; ant_id < fh_config->neAxcUl; ant_id++) {
        struct xran_prb_map *pRbMap = (struct xran_prb_map *)bufs->prachdstdecomp[ant_id][prev_tti % XRAN_N_FE_BUF_LEN].pBuffers->pData;
        AssertFatal(pRbMap != NULL, "(%d:%d:%d)pRbMapPrach == NULL. Aborting.\n", cc_id, prev_tti % XRAN_N_FE_BUF_LEN, ant_id);
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
 * Called per xran port and antenna, from the RU thread pool or split7_thread(),
 * once oai_xran_fh_rx_callback() signalled the slot. */
void xran_fh_rx_prach_read_slot_ant(const struct xran_ru_config *ru_conf, const struct xran_prach_config *prach_conf, const int kbar, struct xran_buffer_list *prachdstdecomp, c16_t *prach_dst)
{
  struct xran_prb_map *pPrbMap = (struct xran_prb_map *)prachdstdecomp->pBuffers->pData;
  for (uint8_t sym_idx = prach_conf->startSymId; sym_idx <= prach_conf->lastSymId; sym_idx++) {
    int16_t *dst, *src;
    int idx = 0;
    dst = (int16_t *)prach_dst;
    struct xran_rx_packet_ctl *p_rx_packet_ctl = &pPrbMap->sFrontHaulRxPacketCtrl[sym_idx];
    uint16_t num_prbu = p_rx_packet_ctl->nRBSize[0]; // or prach_conf->numPrbc
    uint16_t N_ZC = (num_prbu <= 12) ? 139 : 839;
    int32_t nRxPkt = p_rx_packet_ctl->nRxPkt;
    if (nRxPkt == 0) {
      memset(&dst[sym_idx], 0, N_ZC * 2 * sizeof(*dst));
      continue;
    } else {
      src = (int16_t *)p_rx_packet_ctl->pData[0];
      if (src == NULL) { // protection
        LOG_E(HW, "PRACH sym_idx %d src = NULL\n", sym_idx);
        memset(&dst[sym_idx], 0, N_ZC * 2 * sizeof(*dst));
        continue;
      }
    }
    /* convert Network order to host order */
    if (ru_conf->compMeth_PRACH == XRAN_COMPMETHOD_NONE) {
      if (sym_idx == prach_conf->startSymId) {
        for (idx = 0; idx < N_ZC * 2; idx++) {
          dst[idx] = ((int16_t)ntohs(src[idx + kbar]));
        }
      } else {
        for (idx = 0; idx < N_ZC * 2; idx++) {
          dst[idx] += ((int16_t)ntohs(src[idx + kbar]));
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
          dst[idx] = local_dst[idx + kbar];
      else
        for (idx = 0; idx < (N_ZC * 2); idx++)
          dst[idx] += (local_dst[idx + kbar]);
    } // COMPMETHOD_BLKFLOAT
  } // sym_idx
}

/** @details Read PUSCH data from xran buffers.
 * If I/Q compression (bitwidth < 16 bits) is configured, decompresses the data
 * before writing.
 * Called per xran port and antenna, from the RU thread pool or split7_thread(),
 * once oai_xran_fh_rx_callback() signalled the slot. */
void xran_fh_rx_read_slot_ant(const int fft_size, struct xran_buffer_list *dstcp, int32_t *rxdataF_ant)
{
  struct xran_prb_map *pPrbMap = (struct xran_prb_map *)dstcp->pBuffers->pData;
  struct xran_prb_elm *pRbElm = &pPrbMap->prbMap[0];

  for (int32_t sym_idx = pRbElm->nStartSymb; sym_idx < pRbElm->nStartSymb + pRbElm->numSymb; sym_idx++) {
    int32_t *pos = rxdataF_ant + sym_idx * fft_size;
    struct xran_rx_packet_ctl *p_rx_packet_ctl = &pPrbMap->sFrontHaulRxPacketCtrl[sym_idx];
    int32_t nRxPkt = p_rx_packet_ctl->nRxPkt;
    LOG_D(HW, "nRxPkt %d\n", nRxPkt);
    for (int pkt_idx = 0; pkt_idx < nRxPkt; pkt_idx++) {
      uint8_t *src = p_rx_packet_ctl->pData[pkt_idx];
      int numRB = p_rx_packet_ctl->nRBSize[pkt_idx];
      int startRB = p_rx_packet_ctl->nRBStart[pkt_idx];
      // num_prbu & start_prbu are for UL U-plane only
      LOG_D(HW, "p_rx_packet_ctl[%d] startRB[%d]:numRB[%d]\n", pkt_idx, startRB, numRB);

      if (pRbElm->compMethod == XRAN_COMPMETHOD_NONE) {
        // NOTE: gcc 11 knows how to generate AVX2 for this!
        for (int idx = 0; idx < (numRB * N_SC_PER_PRB) * 2; idx++)
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
      } else {
        printf("pRbElm->compMethod == %d is not supported\n", pRbElm->compMethod);
        exit(-1);
      }
    } // pkt_idx
  } // sym_idx
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
 * return the payload length. Shared by both PRB-map layouts of xran_fh_tx_send_slot_ant(). */
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

/** @details Write PDSCH IQ-data of one antenna from OAI txdataF_BF buffer to xran buffers.
 * If I/Q compression (bitwidth < 16 bits) is configured, compresses the data before writing.
 * srccp/src are the antenna's C-plane PRB map and U-plane buffers of the slot, so antennas are
 * independent. Handles either PRB-map layout:
 * - fragmented: nPrbElm is the number of frequency fragments; dst_cursor is carried across
 *   elements per symbol
 * - per-symbol (Liteon FR2, RunSlotPrbMapBySymbolEnable): nPrbElm == XRAN_NUM_OF_SYMBOL_PER_SLOT
 *   and each element maps to exactly one symbol; dst_cursor is a no-op for this case since
 *   each symbol is only ever touched once
 * Both are handled by the same data-driven loop: correctness comes entirely from each
 * element's own nStartSymb/numSymb. */
void xran_fh_tx_send_slot_ant(const int fft_size, int32_t *txdataF_ant, struct xran_buffer_list *srccp, struct xran_buffer_list *src)
{
  struct xran_prb_map *pPrbMap = (struct xran_prb_map *)srccp->pBuffers->pData;
  uint8_t *dst_cursor[XRAN_NUM_OF_SYMBOL_PER_SLOT] = {NULL};

  for (uint32_t idxElm = 0; idxElm < pPrbMap->nPrbElm; idxElm++) {
    struct xran_prb_elm *p_prbMapElm = &pPrbMap->prbMap[idxElm];
    int16_t start_totalRB = p_prbMapElm->nRBStart;
    int16_t startRB = p_prbMapElm->UP_nRBStart;
    int16_t numRB = p_prbMapElm->UP_nRBSize;

    for (int32_t sym_idx = p_prbMapElm->nStartSymb; sym_idx < p_prbMapElm->nStartSymb + p_prbMapElm->numSymb; sym_idx++) {
      struct xran_section_desc *p_sec_desc = &p_prbMapElm->sec_desc[sym_idx];

      uint8_t *base = src->pBuffers[sym_idx].pData;
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
