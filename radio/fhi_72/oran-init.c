/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "xran_fh_o_du.h"
#include "xran_pkt.h"
#include "xran_pkt_up.h"
#include "rte_ether.h"

#include "oran-config.h"
#include "oran-init.h"
#include "oaioran.h"

#include "common/utils/assertions.h"
#include "common/utils/LOG/log.h"
#include "common_lib.h"

/* PRACH data samples are 32 bits wide (16bits for I/Q). Each packet contains
 * 840 samples for long sequence or 144 for short sequence. The payload length
 * is 840*16*2/8 octets.*/
#ifdef FCN_1_2_6_EARLIER
#define PRACH_PLAYBACK_BUFFER_BYTES (144 * 4L)
#else
#define PRACH_PLAYBACK_BUFFER_BYTES (840 * 4L)
#endif

// structure holding allocated memory for ports (multiple DUs) and sectors
// (multiple CCs)
static oran_port_instance_t gPortInst[XRAN_PORTS_NUM][XRAN_MAX_SECTOR_NR];
void *gxran_handle[XRAN_PORTS_NUM];

static struct xran_fh_init g_fh_init = {0};
static struct xran_fh_config g_fh_config[XRAN_PORTS_NUM] = {0};
static nr_prach_info_t g_prach_info[XRAN_PORTS_NUM] = {0};

static uint32_t get_nSW_ToFpga_FTH_TxBufferLen(int mu, int sections)
{
  uint32_t xran_max_sections_per_slot = RTE_MAX(sections, XRAN_MIN_SECTIONS_PER_SLOT);
  uint32_t overhead = xran_max_sections_per_slot
                      * (RTE_PKTMBUF_HEADROOM + sizeof(struct rte_ether_hdr) + sizeof(struct xran_ecpri_hdr)
                         + sizeof(struct radio_app_common_hdr) + sizeof(struct data_section_hdr));
  if (mu <= 1) {
    return 13168 + overhead; /* 273*12*4 + 64* + ETH AND ORAN HDRs */
  } else if (mu == 3) {
    return 3328 + overhead;
  } else {
    assert(false && "numerology not supported\n");
  }
}

static uint32_t get_nFpgaToSW_FTH_RxBufferLen(int mu)
{
  /* note: previous code checked MTU:
   * mu <= 1: return mtu > XRAN_MTU_DEFAULT ? 13168 : XRAN_MTU_DEFAULT;
   * mu == 3: return mtu > XRAN_MTU_DEFAULT ? 3328 : XRAN_MTU_DEFAULT;
   * but I don't understand the interest: if the buffer is a big bigger, there
   * is no problem, or we could just set the MTU size as buffer size?!
   * Go with Max for the moment
   */
  if (mu <= 1) {
    return 13168; /* 273*12*4 + 64*/
  } else if (mu == 3) {
    return 3328;
  } else {
    assert(false && "numerology not supported\n");
  }
}

static struct xran_prb_map get_xran_prb_map(const struct xran_fh_config *f, const uint8_t dir, const int16_t start_sym, const int16_t num_sym)
{
  struct xran_prb_map prbmap = {
      .dir = dir,
      .xran_port = 0,
      .band_id = 0,
      .cc_id = 0,
      .ru_port_id = 0,
      .tti_id = 0,
      .nPrbElm = 1, // represents the number of fragments on DL only; calculated in xran_init_PrbMap_from_cfg()/xran_init_PrbMap_by_symbol_from_cfg()
  };
  struct xran_prb_elm *e = &prbmap.prbMap[0];
  e->nStartSymb = start_sym;
  e->numSymb = num_sym;
  e->nRBStart = 0;
  uint8_t mu_number = f->mu_number[0];
  e->nRBSize = (dir == XRAN_DIR_DL) ? f->perMu[mu_number].nDLRBs : f->perMu[mu_number].nULRBs;
  e->nBeamIndex = 0;
  e->compMethod = f->ru_conf.compMeth;
  e->iqWidth = f->ru_conf.iqWidth;
  memset(&prbmap.sFrontHaulRxPacketCtrl, 0, XRAN_NUM_OF_SYMBOL_PER_SLOT * sizeof(struct xran_rx_packet_ctl));
  return prbmap;
}

static struct xran_prb_map get_xran_prb_map_prach(const struct xran_fh_config *f)
{
  struct xran_prb_map prbmap = {
      .dir = XRAN_DIR_UL,
      .xran_port = 0,
      .band_id = 0,
      .cc_id = 0,
      .ru_port_id = 0,
      .tti_id = 0,
      .nPrbElm = 1,
  };
  struct xran_prb_elm *e = &prbmap.prbMap[0];
  e->nStartSymb = 0;
  e->numSymb = 14;
  e->nRBStart = 0;
  e->nRBSize = 12;
  e->nBeamIndex = 0;
  e->compMethod = f->ru_conf.compMeth;
  e->iqWidth = f->ru_conf.iqWidth;
  memset(&prbmap.sFrontHaulRxPacketCtrl, 0, XRAN_NUM_OF_SYMBOL_PER_SLOT * sizeof(struct xran_rx_packet_ctl));
  return prbmap;
}

static uint32_t next_power_2(uint32_t num)
{
  uint32_t power = 2;
  while (power < num)
    power <<= 1;
  return power;
}

static uint32_t oran_allocate_uplane_buffers(
    void *instHandle,
    struct xran_buffer_list list[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
    struct xran_flat_buffer buf[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN][XRAN_NUM_OF_SYMBOL_PER_SLOT],
    uint32_t ant,
    uint32_t bufSize)
{
  xran_status_t status;
  uint32_t pool;
  /* xran_bm_init() uses rte_pktmbuf_pool_create() which recommends to use a power of two for the buffers;
    the E release sample app didn't take this into account, but we introduced it ourselves;
    the F release sample app took this into account, so we can proudly say we assumed correctly */
  uint32_t numBufs = next_power_2(XRAN_N_FE_BUF_LEN * ant * XRAN_NUM_OF_SYMBOL_PER_SLOT) - 1;
  status = xran_bm_init(instHandle, &pool, numBufs, bufSize);
  AssertFatal(XRAN_STATUS_SUCCESS == status, "Failed at xran_bm_init(), status %d\n", status);
  printf("xran_bm_init() hInstance %p poolIdx %u elements %u size %u\n", instHandle, pool, numBufs, bufSize);
  int count = 0;
  for (uint32_t a = 0; a < ant; ++a) {
    for (uint32_t j = 0; j < XRAN_N_FE_BUF_LEN; ++j) {
      list[a][j].pBuffers = &buf[a][j][0];
      for (uint32_t k = 0; k < XRAN_NUM_OF_SYMBOL_PER_SLOT; ++k) {
        struct xran_flat_buffer *fb = &list[a][j].pBuffers[k];
        fb->nElementLenInBytes = bufSize;
        fb->nNumberOfElements = 1;
        fb->nOffsetInBytes = 0;
        void *ptr;
        void *mb;
        status = xran_bm_allocate_buffer(instHandle, pool, &ptr, &mb);
        AssertFatal(XRAN_STATUS_SUCCESS == status && ptr != NULL && mb != NULL,
                    "Failed at xran_bm_allocate_buffer(), status %d\n",
                    status);
        count++;
        fb->pData = ptr;
        fb->pCtrl = mb;
        memset(ptr, 0, bufSize);
      }
    }
  }
  printf("xran_bm_allocate_buffer() hInstance %p poolIdx %u count %d\n", instHandle, pool, count);
  return pool;
}

typedef struct oran_mixed_slot {
  uint32_t idx;
  uint32_t num_dlsym;
  uint32_t num_ulsym;
  uint32_t start_ulsym;
} oran_mixed_slot_t;
static oran_mixed_slot_t get_mixed_slot_info(const struct xran_frame_config *fconfig)
{
  oran_mixed_slot_t info = {0};
  for (size_t sl = 0; sl < fconfig->nTddPeriod; ++sl) {
    info.num_dlsym = info.num_ulsym = 0;
    for (size_t sym = 0; sym < XRAN_NUM_OF_SYMBOL_PER_SLOT; ++sym) {
      uint8_t t = fconfig->sSlotConfig[sl].nSymbolType[sym];
      if (t == 0 /* DL */) {
        info.num_dlsym++;
      } else if (t == 1 /* UL */) {
        if (info.num_ulsym == 0)
          info.start_ulsym = sym;
        info.num_ulsym++;
      } else if (t == 2 /* Mixed */) {
        info.idx = sl;
      } else {
        AssertFatal(false, "unknown symbol type %d\n", t);
      }
    }
    if (info.idx > 0)
      return info;
  }
  AssertFatal(false, "could not find mixed slot!\n");
  return info;
}

typedef struct oran_cplane_prb_config {
  uint8_t nTddPeriod;
  uint32_t mixed_slot_index;
  struct xran_prb_map slotMap;
  struct xran_prb_map mixedSlotMap;
} oran_cplane_prb_config;

static void oran_allocate_cplane_buffers(void *instHandle,
                                         struct xran_buffer_list list[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                                         struct xran_flat_buffer buf[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN],
                                         uint32_t ant,
                                         uint32_t sect,
                                         uint32_t mtu,
                                         const struct xran_fh_config *fh_config,
                                         uint32_t size_of_prb_map,
                                         oran_cplane_prb_config *prb_conf)
{
  xran_status_t status;
  uint32_t count1 = 0;
  uint32_t poolPrb;
  uint32_t numBufsPrb = next_power_2(XRAN_N_FE_BUF_LEN * ant * XRAN_NUM_OF_SYMBOL_PER_SLOT) - 1;
  uint32_t bufSizePrb = size_of_prb_map;
  status = xran_bm_init(instHandle, &poolPrb, numBufsPrb, bufSizePrb);
  AssertFatal(XRAN_STATUS_SUCCESS == status, "Failed at xran_bm_init(), status %d\n", status);
  printf("xran_bm_init() hInstance %p poolIdx %u elements %u size %u\n", instHandle, poolPrb, numBufsPrb, bufSizePrb);

  for (uint32_t a = 0; a < ant; a++) {
    for (uint32_t j = 0; j < XRAN_N_FE_BUF_LEN; ++j) {
      list[a][j].pBuffers = &buf[a][j];
      struct xran_flat_buffer *fb = list[a][j].pBuffers;
      fb->nElementLenInBytes = bufSizePrb;
      fb->nNumberOfElements = 1;
      fb->nOffsetInBytes = 0;
      void *ptr;
      void *mb;
      status = xran_bm_allocate_buffer(instHandle, poolPrb, &ptr, &mb);
      AssertFatal(XRAN_STATUS_SUCCESS == status && ptr != NULL && mb != NULL,
                  "Failed at xran_bm_allocate_buffer(), status %d\n",
                  status);
      count1++;
      fb->pData = ptr;
      fb->pCtrl = mb;

      struct xran_prb_map *src = &prb_conf->slotMap;
      // get mixed slot map if in TDD and in mixed slot
      if (prb_conf->nTddPeriod != 0 && (j % prb_conf->nTddPeriod) == prb_conf->mixed_slot_index)
        src = &prb_conf->mixedSlotMap;
      if (fh_config->RunSlotPrbMapBySymbolEnable) {
        uint8_t mu_number = fh_config->mu_number[0];
        xran_init_PrbMap_by_symbol_from_cfg(src, ptr, mtu, fh_config->perMu[mu_number].nDLRBs);
      } else {
        xran_init_PrbMap_from_cfg(src, ptr, mtu);
      }
    }
  }
  printf("xran_bm_allocate_buffer() hInstance %p poolIdx %u count %u\n", instHandle, poolPrb, count1);
}

static void oran_allocate_buffers(void *handle,
                                  int xran_inst,
                                  int num_sectors,
                                  oran_port_instance_t *portInstances,
                                  uint32_t mtu,
                                  const struct xran_fh_config *fh_config)
{
  AssertFatal(num_sectors == 1, "only support one sector at the moment\n");
  oran_port_instance_t *pi = &portInstances[0];
  AssertFatal(handle != NULL, "no handle provided\n");
  uint32_t xran_max_antenna_nr = RTE_MAX(fh_config->neAxc, fh_config->neAxcUl);
  uint32_t xran_max_sections_per_slot = RTE_MAX(fh_config->max_sections_per_slot, XRAN_MIN_SECTIONS_PER_SLOT);

#if defined(__arm__) || defined(__aarch64__)
    // ARM-specific memory allocation
    int ret = posix_memalign((void**)&pi->buf_list, 256, sizeof(*pi->buf_list));
    AssertFatal(ret == 0, "out of memory\n");
#else
    // Intel-specific memory allocation
    pi->buf_list = _mm_malloc(sizeof(*pi->buf_list), 256);
#endif
  AssertFatal(pi->buf_list != NULL, "out of memory\n");
  oran_buf_list_t *bl = pi->buf_list;

  xran_status_t status;
  printf("xran_sector_get_instances() o_xu_id %d xran_handle %p\n", xran_inst, handle);
  status = xran_sector_get_instances(xran_inst, handle, num_sectors, &pi->instanceHandle);
  printf("-> hInstance %p\n", pi->instanceHandle);
  AssertFatal(status == XRAN_STATUS_SUCCESS, "get sector instance failed for XRAN nInstanceNum %d\n", xran_inst);

  // DL/UL PRB mapping depending on the duplex mode
  struct xran_prb_map dlPm = get_xran_prb_map(fh_config, XRAN_DIR_DL, 0, 14);
  struct xran_prb_map ulPm = get_xran_prb_map(fh_config, XRAN_DIR_UL, 0, 14);
  struct xran_prb_map prachPm = get_xran_prb_map_prach(fh_config);
  struct xran_prb_map dlPmMixed = {0};
  struct xran_prb_map ulPmMixed = {0};
  uint32_t idx = 0;

  if (fh_config->frame_conf.nFrameDuplexType == XRAN_TDD) {
    oran_mixed_slot_t info = get_mixed_slot_info(&fh_config->frame_conf);
    dlPmMixed = get_xran_prb_map(fh_config, XRAN_DIR_DL, 0, info.num_dlsym);
    ulPmMixed = get_xran_prb_map(fh_config, XRAN_DIR_UL, info.start_ulsym, info.num_ulsym);
    idx = info.idx;
  }

  oran_cplane_prb_config dlConf = {
      .nTddPeriod = fh_config->frame_conf.nTddPeriod,
      .mixed_slot_index = idx,
      .slotMap = dlPm,
      .mixedSlotMap = dlPmMixed,
  };

  oran_cplane_prb_config ulConf = {
      .nTddPeriod = fh_config->frame_conf.nTddPeriod,
      .mixed_slot_index = idx,
      .slotMap = ulPm,
      .mixedSlotMap = ulPmMixed,
  };

  uint32_t numPrbElm = (fh_config->RunSlotPrbMapBySymbolEnable) ? XRAN_NUM_OF_SYMBOL_PER_SLOT : xran_get_num_prb_elm(&dlPm, mtu);
  uint32_t size_of_prb_map = sizeof(struct xran_prb_map) + sizeof(struct xran_prb_elm) * (numPrbElm);
  oran_cplane_prb_config prachConf = {
      .nTddPeriod = fh_config->frame_conf.nTddPeriod,
      .mixed_slot_index = idx,
      .slotMap = prachPm,
      .mixedSlotMap = prachPm,
  };

  uint32_t numPrbElmPrach = (fh_config->RunSlotPrbMapBySymbolEnable) ? XRAN_NUM_OF_SYMBOL_PER_SLOT : xran_get_num_prb_elm(&prachPm, mtu);
  uint32_t size_of_prb_map_prach  = sizeof(struct xran_prb_map) + sizeof(struct xran_prb_elm) * (numPrbElmPrach);

  // PDSCH
  const uint32_t txBufSize = get_nSW_ToFpga_FTH_TxBufferLen(fh_config->nNumerology[0], fh_config->max_sections_per_slot);
  oran_allocate_uplane_buffers(pi->instanceHandle, bl->src, bl->bufs.tx, xran_max_antenna_nr, txBufSize);
  oran_allocate_cplane_buffers(pi->instanceHandle,
                               bl->srccp,
                               bl->bufs.tx_prbmap,
                               xran_max_antenna_nr,
                               xran_max_sections_per_slot,
                               mtu,
                               fh_config,
                               size_of_prb_map,
                               &dlConf);

  // PUSCH
  const uint32_t rxBufSize = get_nFpgaToSW_FTH_RxBufferLen(fh_config->nNumerology[0]);
  oran_allocate_uplane_buffers(pi->instanceHandle, bl->dst, bl->bufs.rx, xran_max_antenna_nr, rxBufSize);
  oran_allocate_cplane_buffers(pi->instanceHandle,
                               bl->dstcp,
                               bl->bufs.rx_prbmap,
                               xran_max_antenna_nr,
                               xran_max_sections_per_slot,
                               mtu,
                               fh_config,
                               size_of_prb_map,
                               &ulConf);

  // PRACH
  const uint32_t prachBufSize = PRACH_PLAYBACK_BUFFER_BYTES;
  oran_allocate_uplane_buffers(pi->instanceHandle, bl->prachdst, bl->bufs.prach, xran_max_antenna_nr, prachBufSize);
  oran_allocate_cplane_buffers(pi->instanceHandle,
                               bl->prachdstdecomp,
                               bl->bufs.prachdecomp,
                               xran_max_antenna_nr,
                               xran_max_sections_per_slot,
                               mtu,
                               fh_config,
                               size_of_prb_map_prach,
                               &prachConf);

  struct xran_buffer_list *src[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN];
  struct xran_buffer_list *srccp[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN];
  struct xran_buffer_list *dst[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN];
  struct xran_buffer_list *dstcp[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN];
  struct xran_buffer_list *prachdst[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN];
  struct xran_buffer_list *prachdstdecomp[XRAN_MAX_ANTENNA_NR][XRAN_N_FE_BUF_LEN];
  for (uint32_t a = 0; a < XRAN_MAX_ANTENNA_NR; ++a) {
    for (uint32_t j = 0; j < XRAN_N_FE_BUF_LEN; ++j) {
      src[a][j] = &bl->src[a][j];
      srccp[a][j] = &bl->srccp[a][j];
      dst[a][j] = &bl->dst[a][j];
      dstcp[a][j] = &bl->dstcp[a][j];
      prachdst[a][j] = &bl->prachdst[a][j];
      prachdstdecomp[a][j] = &bl->prachdstdecomp[a][j];
    }
  }

  xran_5g_fronthault_config(pi->instanceHandle, src, srccp, dst, dstcp, oai_xran_fh_rx_callback, &portInstances->pusch_tag, fh_config->nNumerology[0]);
  xran_5g_prach_req(pi->instanceHandle, prachdst, prachdstdecomp, oai_xran_fh_rx_prach_callback, &portInstances->prach_tag, fh_config->nNumerology[0]);
}

void *oai_oran_initialize(struct xran_fh_init *xran_fh_init, struct xran_fh_config *xran_fh_config)
{
  int32_t xret = 0;

  xran_mem_mgr_leak_detector_init();

  print_fh_init(xran_fh_init);
  xret = xran_init(0, NULL, xran_fh_init, NULL, gxran_handle);
  if (xret != XRAN_STATUS_SUCCESS) {
    printf("xran_init failed %d\n", xret);
    exit(-1);
  }

  for (int32_t o_xu_id = 0; o_xu_id < xran_fh_init->xran_ports; o_xu_id++) {
    if (gxran_handle[o_xu_id] == NULL) {
      printf("xran_init for RU%d failed\n", o_xu_id);
      exit(-1);
    } else {
      printf("RU%d handle = %p\n", o_xu_id, gxran_handle[o_xu_id]);
    }
  }

  /** process all the O-RU|O-DU for use case */
  for (int32_t o_xu_id = 0; o_xu_id < xran_fh_init->xran_ports; o_xu_id++) {
    print_fh_config(&xran_fh_config[o_xu_id]);
    xret = xran_open(gxran_handle[o_xu_id], &xran_fh_config[o_xu_id]);
    if (xret != XRAN_STATUS_SUCCESS) {
      printf("xran_open failed %d\n", xret);
      exit(-1);
    }

    int sector = 0;
    printf("Initialize ORAN port instance %d (%d) sector %d\n", o_xu_id, xran_fh_init->xran_ports, sector);
    oran_port_instance_t *pi = &gPortInst[o_xu_id][sector];
    struct xran_cb_tag tag = {.cellId = sector, .oXuId = o_xu_id};
    pi->prach_tag = tag;
    pi->pusch_tag = tag;

    oran_allocate_buffers(gxran_handle[0], o_xu_id, 1, pi, xran_fh_init->mtu, &xran_fh_config[o_xu_id]);
    if ((xret = xran_timingsource_reg_tticb(NULL, oai_physide_dl_tti_call_back, NULL, 10, XRAN_CB_TTI)) != XRAN_STATUS_SUCCESS) {
      printf("xran_timingsource_reg_tticb failed %d\n", xret);
      exit(-1);
    }

    // retrieve and store prach duration
    uint8_t mu = xran_fh_config[o_xu_id].nNumerology[0];
    uint8_t idx = xran_fh_config[o_xu_id].perMu[mu].prach_conf.nPrachConfIdx;
    const struct xran_frame_config *fc = &xran_fh_config[o_xu_id].frame_conf;
    g_prach_info[o_xu_id] = get_nr_prach_occasion_info_from_index(idx, mu > 2 ? FR2 : FR1, fc->nFrameDuplexType == XRAN_FDD ? duplex_mode_FDD : duplex_mode_TDD);
  }

  // store config after xran initialization -- xran makes modifications to
  // these structs during initialization
  memcpy(&g_fh_init, xran_fh_init, sizeof(*xran_fh_init));
  memcpy(&g_fh_config, xran_fh_config, sizeof(*xran_fh_config) * xran_fh_init->xran_ports);

  return gxran_handle;
}

oran_buf_list_t *get_xran_buffers(uint32_t port_id)
{
  struct xran_fh_init *fh_init = get_xran_fh_init();
  DevAssert(port_id < fh_init->xran_ports);
  return gPortInst[port_id][0].buf_list;
}

struct xran_fh_init *get_xran_fh_init(void)
{
  return &g_fh_init;
}

struct xran_fh_config *get_xran_fh_config(uint32_t port_id)
{
  struct xran_fh_init *fh_init = get_xran_fh_init();
  DevAssert(port_id < fh_init->xran_ports);
  return &g_fh_config[port_id];
}

nr_prach_info_t get_prach_info(uint32_t port_id)
{
  struct xran_fh_init *fh_init = get_xran_fh_init();
  DevAssert(port_id < fh_init->xran_ports);
  return g_prach_info[port_id];
}
