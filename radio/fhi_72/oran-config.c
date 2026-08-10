/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "oran-config.h"
#include "oran-params.h"
#include "common/utils/assertions.h"
#include "common_lib.h"

#include "xran_fh_o_du.h"
#include "xran_cp_api.h"
#include "rte_ether.h"
#include <rte_ethdev.h>

#include "stdio.h"
#include "string.h"
#include "log.h"

#ifdef OAI_MPLANE
#include "mplane/ru-mplane-api.h"
#endif

static void print_fh_eowd_cmn(unsigned index, const struct xran_ecpri_del_meas_cmn *eowd_cmn)
{
  printf("\
    eowd_cmn[%u]:\n\
      initiator_en %d\n\
      numberOfSamples %d\n\
      filterType %d\n\
      responseTo %ld\n\
      measVf %d\n\
      measState %d\n\
      measId %d\n\
      measMethod %d\n\
      owdm_enable %d\n\
      owdm_PlLength %d\n",
      index,
      eowd_cmn->initiator_en,
      eowd_cmn->numberOfSamples,
      eowd_cmn->filterType,
      eowd_cmn->responseTo,
      eowd_cmn->measVf,
      eowd_cmn->measState,
      eowd_cmn->measId,
      eowd_cmn->measMethod,
      eowd_cmn->owdm_enable,
      eowd_cmn->owdm_PlLength);
}

static void print_fh_init_io_cfg(const struct xran_io_cfg *io_cfg)
{
  printf("\
  io_cfg:\n\
    id %d (%s)\n\
    num_vfs %d\n\
    num_rxq %d\n\
    dpdk_dev [%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s]\n\
    bbdev_dev %s\n\
    bbdev_mode %d\n\
    dpdkIoVaMode %d\n\
    dpdkMemorySize %d\n\
    core %d\n\
    system_core %d\n\
    pkt_proc_core %016lx\n\
    pkt_proc_core_64_127 %016lx\n\
    pkt_aux_core %d\n\
    timing_core %d\n\
    port (filled within xran library)\n\
    io_sleep %d\n\
    nEthLinePerPort %d\n\
    nEthLineSpeed %d\n\
    one_vf_cu_plane %d\n",
      io_cfg->id,
      io_cfg->id == 0 ? "O-DU" : "O-RU",
      io_cfg->num_vfs,
      io_cfg->num_rxq,
      io_cfg->dpdk_dev[XRAN_UP_VF],
      io_cfg->dpdk_dev[XRAN_CP_VF],
      io_cfg->dpdk_dev[XRAN_UP_VF1],
      io_cfg->dpdk_dev[XRAN_CP_VF1],
      io_cfg->dpdk_dev[XRAN_UP_VF2],
      io_cfg->dpdk_dev[XRAN_CP_VF2],
      io_cfg->dpdk_dev[XRAN_UP_VF3],
      io_cfg->dpdk_dev[XRAN_CP_VF3],
      io_cfg->dpdk_dev[XRAN_UP_VF4],
      io_cfg->dpdk_dev[XRAN_CP_VF4],
      io_cfg->dpdk_dev[XRAN_UP_VF5],
      io_cfg->dpdk_dev[XRAN_CP_VF5],
      io_cfg->dpdk_dev[XRAN_UP_VF6],
      io_cfg->dpdk_dev[XRAN_CP_VF6],
      io_cfg->dpdk_dev[XRAN_UP_VF7],
      io_cfg->dpdk_dev[XRAN_CP_VF7],
      io_cfg->bbdev_dev[0],
      io_cfg->bbdev_mode,
      io_cfg->dpdkIoVaMode,
      io_cfg->dpdkMemorySize,
      io_cfg->core,
      io_cfg->system_core,
      io_cfg->pkt_proc_core,
      io_cfg->pkt_proc_core_64_127,
      io_cfg->pkt_aux_core,
      io_cfg->timing_core,
      io_cfg->io_sleep,
      io_cfg->nEthLinePerPort,
      io_cfg->nEthLineSpeed,
      io_cfg->one_vf_cu_plane);

  print_fh_eowd_cmn(io_cfg->id, &io_cfg->eowd_cmn[io_cfg->id]);
  printf("eowd_port (filled within xran library)\n");
  printf("\
    bbu_offload %d\n",
      io_cfg->bbu_offload);
}

static void print_fh_init_eaxcid_conf(const struct xran_eaxcid_config *eaxcid_conf)
{
  printf("\
  eAxCId_conf:\n\
    mask_cuPortId 0x%04x\n\
    mask_bandSectorId 0x%04x\n\
    mask_ccId 0x%04x\n\
    mask_ruPortId 0x%04x\n\
    bit_cuPortId %d\n\
    bit_bandSectorId %d\n\
    bit_ccId %d\n\
    bit_ruPortId %d\n",
      eaxcid_conf->mask_cuPortId,
      eaxcid_conf->mask_bandSectorId,
      eaxcid_conf->mask_ccId,
      eaxcid_conf->mask_ruPortId,
      eaxcid_conf->bit_cuPortId,
      eaxcid_conf->bit_bandSectorId,
      eaxcid_conf->bit_ccId,
      eaxcid_conf->bit_ruPortId);
}

static void print_ether_addr(const char *pre, int num_ether, const struct rte_ether_addr *addrs)
{
  printf("%s [", pre);
  for (int i = 0; i < num_ether; ++i) {
    char buf[18];
    rte_ether_format_addr(buf, 18, &addrs[i]);
    printf("%s", buf);
    if (i != num_ether - 1)
      printf(", ");
  }
  printf("]\n");
}

void print_fh_init(const struct xran_fh_init *fh_init)
{
  printf("xran_fh_init:\n");
  print_fh_init_io_cfg(&fh_init->io_cfg);
  for (int32_t o_xu_id = 0; o_xu_id < fh_init->xran_ports; o_xu_id++) {
    print_fh_init_eaxcid_conf(&fh_init->eAxCId_conf[o_xu_id]);
  }
  printf("\
  xran_ports %d\n\
  dpdkBasebandFecMode %d\n\
  dpdkBasebandDevice %s\n\
  filePrefix %s\n\
  mtu %d\n\
  p_o_du_addr %s\n",
      fh_init->xran_ports,
      fh_init->dpdkBasebandFecMode,
      fh_init->dpdkBasebandDevice,
      fh_init->filePrefix,
      fh_init->mtu,
      fh_init->p_o_du_addr);
  if (fh_init->p_o_ru_addr) print_ether_addr("  p_o_ru_addr", fh_init->xran_ports * fh_init->io_cfg.num_vfs, (struct rte_ether_addr *)fh_init->p_o_ru_addr);
  else if (fh_init->p_o_du_addr) print_ether_addr("  p_o_du_addr", fh_init->xran_ports * fh_init->io_cfg.num_vfs, (struct rte_ether_addr *)fh_init->p_o_du_addr);
  printf("\
  totalBfWeights %d\n\
  mlogxranenable %d\n\
  dlCpProcBurst %d\n",
      fh_init->totalBfWeights,
      fh_init->mlogxranenable,
      fh_init->dlCpProcBurst);
}

static void print_prach_config(const struct xran_prach_config *prach_conf)
{
  printf("\
    prach_config:\n\
       nPrachConfIdx %d\n\
       nPrachSubcSpacing %d\n\
       nPrachZeroCorrConf %d\n\
       nPrachRestrictSet %d\n\
       nPrachRootSeqIdx %d\n\
       nPrachFreqStart %d\n\
       nPrachFreqOffset %d\n\
       nPrachFilterIdx %d\n\
       startSymId %d\n\
       lastSymId %d\n\
       startPrbc %d\n\
       numPrbc %d\n\
       timeOffset %d\n\
       freqOffset %d\n\
       prachEaxcOffset %d\n\
       nPrachConfIdxLTE %d\n\
       nprachformat %d\n\
       periodicity %d\n\
       startTime %d\n\
       suboffset %d\n\
       numSubCarriers %d\n\
       nRep %d\n",
      prach_conf->nPrachConfIdx,
      prach_conf->nPrachSubcSpacing,
      prach_conf->nPrachZeroCorrConf,
      prach_conf->nPrachRestrictSet,
      prach_conf->nPrachRootSeqIdx,
      prach_conf->nPrachFreqStart,
      prach_conf->nPrachFreqOffset,
      prach_conf->nPrachFilterIdx,
      prach_conf->startSymId,
      prach_conf->lastSymId,
      prach_conf->startPrbc,
      prach_conf->numPrbc,
      prach_conf->timeOffset,
      prach_conf->freqOffset,
      prach_conf->prachEaxcOffset,
      prach_conf->nPrachConfIdxLTE,
      prach_conf->nprachformat,
      prach_conf->periodicity,
      prach_conf->startTime,
      prach_conf->suboffset,
      prach_conf->numSubCarriers,
      prach_conf->nRep);
}

static void print_srs_config(const struct xran_srs_config *srs_conf)
{
  printf("\
  srs_config:\n\
    symbMask %04x\n\
    srsEaxcOffset %d\n",
      srs_conf->symbMask,
      srs_conf->srsEaxcOffset
      );
}

static void print_frame_config(const struct xran_frame_config *frame_conf)
{
  printf("\
  frame_conf:\n\
    nFrameDuplexType %s\n\
    nTddPeriod %d\n",
      frame_conf->nFrameDuplexType == XRAN_TDD ? "TDD" : "FDD",
      frame_conf->nTddPeriod);
  for (int i = 0; i < frame_conf->nTddPeriod; ++i) {
    printf("    sSlotConfig[%d]: ", i);
    for (int s = 0; s < XRAN_NUM_OF_SYMBOL_PER_SLOT; ++s) {
      uint8_t nSymbolType = frame_conf->sSlotConfig[i].nSymbolType[s];
      printf("%c", nSymbolType == 0 ? 'D' : (nSymbolType == 1 ? 'U' : 'G'));
    }
    printf("\n");
  }
}

static void print_ru_config(uint8_t mu_number, const struct xran_ru_config *ru_conf)
{
  printf("\
  ru_config:\n\
    xranTech %s\n\
    xranCat %s\n\
    xranCompHdrType %s\n\
    iqWidth %d\n\
    compMeth %d\n\
    iqWidth_PRACH %d\n\
    compMeth_PRACH %d\n\
    fftSize[mu_number] %d\n\
    byteOrder %s\n\
    iqOrder %s\n\
    xran_max_frame %d\n",
      ru_conf->xranTech == XRAN_RAN_5GNR ? "NR" : "LTE",
      ru_conf->xranCat == XRAN_CATEGORY_A ? "A" : "B",
      ru_conf->xranCompHdrType == XRAN_COMP_HDR_TYPE_DYNAMIC ? "dynamic" : "static",
      ru_conf->iqWidth,
      ru_conf->compMeth,
      ru_conf->iqWidth_PRACH,
      ru_conf->compMeth_PRACH,
      ru_conf->fftSize[mu_number],
      ru_conf->byteOrder == XRAN_NE_BE_BYTE_ORDER ? "network/BE" : "CPU/LE",
      ru_conf->iqOrder == XRAN_I_Q_ORDER ? "I_Q" : "Q_I",
      ru_conf->xran_max_frame);
}

void print_fh_per_mu_cfg(const struct xran_fh_per_mu_cfg *perMu)
{
  printf("\
  perMu:\n\
    nDLBandwidth %d\n\
    nULBandwidth %d\n",
      perMu->nDLBandwidth,
      perMu->nULBandwidth);

  print_prach_config(&perMu->prach_conf);

  printf("\
    freqOffset %d\n\
    nDLFftSize %d\n\
    nULFftSize %d\n\
    eaxcOffset %d\n\
    nDLRBs %d\n\
    nULRBs %d\n\
    Tadv_cp_dl %d\n\
    T2a_min_cp_dl %d\n\
    T2a_max_cp_dl %d\n\
    T2a_min_cp_ul %d\n\
    T2a_max_cp_ul %d\n\
    T2a_min_up %d\n\
    T2a_max_up %d\n\
    Ta3_min %d\n\
    Ta3_max %d\n\
    T1a_min_cp_dl %d\n\
    T1a_max_cp_dl %d\n\
    T1a_min_cp_ul %d\n\
    T1a_max_cp_ul %d\n\
    T1a_min_up %d\n\
    T1a_max_up %d\n\
    Ta4_min %d\n\
    Ta4_max %d\n\
    prachEnable %d\n\
    prachConfigIndex %d\n\
    prachConfigIndexLTE %d\n\
    nbIotUlScs %d\n\
    adv_tx_time %d\n",
      perMu->freqOffset,
      perMu->nDLFftSize,
      perMu->nULFftSize,
      perMu->eaxcOffset,
      perMu->nDLRBs,
      perMu->nULRBs,
      perMu->Tadv_cp_dl,
      perMu->T2a_min_cp_dl,
      perMu->T2a_max_cp_dl,
      perMu->T2a_min_cp_ul,
      perMu->T2a_max_cp_ul,
      perMu->T2a_min_up,
      perMu->T2a_max_up,
      perMu->Ta3_min,
      perMu->Ta3_max,
      perMu->T1a_min_cp_dl,
      perMu->T1a_max_cp_dl,
      perMu->T1a_min_cp_ul,
      perMu->T1a_max_cp_ul,
      perMu->T1a_min_up,
      perMu->T1a_max_up,
      perMu->Ta4_min,
      perMu->Ta4_max,
      perMu->prachEnable,
      perMu->prachConfigIndex,
      perMu->prachConfigIndexLTE,
      perMu->nbIotUlScs,
      perMu->adv_tx_time);
}

void print_fh_config(const struct xran_fh_config *fh_config)
{
  printf("xran_fh_config:\n");

  printf("\
  dpdk_port %d\n\
  sector_id %d\n\
  nCC %d\n\
  neAxc %d\n\
  neAxcUl %d\n\
  nAntElmTRx %d\n\
  nDLAbsFrePointA %d\n\
  nULAbsFrePointA %d\n\
  nDLCenterFreqARFCN %d\n\
  nULCenterFreqARFCN %d\n\
  ttiCb %p\n\
  ttiCbParam %p\n",
      fh_config->dpdk_port,
      fh_config->sector_id,
      fh_config->nCC,
      fh_config->neAxc,
      fh_config->neAxcUl,
      fh_config->nAntElmTRx,
      fh_config->nDLAbsFrePointA,
      fh_config->nULAbsFrePointA,
      fh_config->nDLCenterFreqARFCN,
      fh_config->nULCenterFreqARFCN,
      fh_config->ttiCb,
      fh_config->ttiCbParam);
  uint8_t mu_number = fh_config->mu_number[0];
  print_fh_per_mu_cfg(&fh_config->perMu[mu_number]);
  printf("\
  enableCP %d\n\
  srsEnable %d\n\
  srsEnableCp %d\n\
  SrsDelaySym %d\n\
  puschMaskEnable %d\n\
  puschMaskSlot %d\n\
  debugStop %d\n\
  debugStopCount %d\n\
  DynamicSectionEna %d\n\
  GPS_Alpha %d\n\
  GPS_Beta %d\n",
      fh_config->enableCP,
      fh_config->srsEnable,
      fh_config->srsEnableCp,
      fh_config->SrsDelaySym,
      fh_config->puschMaskEnable,
      fh_config->puschMaskSlot,
      fh_config->debugStop,
      fh_config->debugStopCount,
      fh_config->DynamicSectionEna,
      fh_config->GPS_Alpha,
      fh_config->GPS_Beta);

  print_srs_config(&fh_config->srs_conf);
  print_frame_config(&fh_config->frame_conf);
  print_ru_config(mu_number, &fh_config->ru_conf);

  printf("\
  bbdev_enc %p\n\
  bbdev_dec %p\n\
  tx_cp_eAxC2Vf %p\n\
  tx_up_eAxC2Vf %p\n\
  rx_cp_eAxC2Vf %p\n\
  rx_up_eAxC2Vf %p\n\
  log_level %d\n\
  max_sections_per_slot %d\n\
  max_sections_per_symbol %d\n\
  RunSlotPrbMapBySymbolEnable %d\n\
  dssEnable %d\n\
  dssPeriod %d\n\
  technology[XRAN_MAX_DSS_PERIODICITY] (not filled as DSS disabled)\n\
  numMUs %d\n\
  mu_number[0] %d\n\
  nNumerology[0] %d\n",
      fh_config->bbdev_enc,
      fh_config->bbdev_dec,
      fh_config->tx_cp_eAxC2Vf,
      fh_config->tx_up_eAxC2Vf,
      fh_config->rx_cp_eAxC2Vf,
      fh_config->rx_up_eAxC2Vf,
      fh_config->log_level,
      fh_config->max_sections_per_slot,
      fh_config->max_sections_per_symbol,
      fh_config->RunSlotPrbMapBySymbolEnable,
      fh_config->dssEnable,
      fh_config->dssPeriod,
      fh_config->numMUs,
      fh_config->mu_number[0],
      fh_config->nNumerology[0]);
}

static void get_u128_mask(const paramdef_t *pd, uint64_t *u0t63, uint64_t *u64t127)
{
  DevAssert(pd != NULL);
  AssertFatal(pd->numelt > 0, "no entries for creation of mask\n");
  for (int i = 0; i < pd->numelt; ++i) {
    int num = pd->iptr[i];
    AssertFatal(num >= 0 && num < 128, "cannot put element of %d in 128-bit mask\n", num);
    uint64_t *mask = num < 64 ? u0t63 : u64t127;
    *mask |= 1LL << (num % 64);
  }
}

char bbdev_dev[32] = "";
char bbdev_vfio_vf_token[64] = "";

static bool set_fh_io_cfg(struct xran_io_cfg *io_cfg, const paramdef_t *fhip, int nump, const int num_rus)
{
  DevAssert(fhip != NULL);
  int num_dev = gpd(fhip, nump, ORAN_CONFIG_DPDK_DEVICES)->numelt;
  AssertFatal(num_dev > 0, "need to provide DPDK devices for O-RAN 7.2 Fronthaul\n");
  AssertFatal(num_dev < 17, "too many DPDK devices for O-RAN 7.2 Fronthaul\n");

  int app_id_index = config_paramidx_fromname((paramdef_t *)fhip, nump, ORAN_CONFIG_APP_ID);
  AssertFatal(app_id_index >= 0,"Index for %s config option not found!\n", ORAN_CONFIG_APP_ID);
  io_cfg->id = config_get_processedint(config_get_if(), (paramdef_t *)&fhip[app_id_index]);
  LOG_A(PHY, "Initializing XRAN layer as %s\n", io_cfg->id == XRAN_APP_ID_O_DU ? "O-DU" : "O-RU");

  io_cfg->num_vfs = num_dev; // number of VFs for C-plane and U-plane (should be even); max = XRAN_VF_MAX
  io_cfg->num_rxq = 1; // number of RX queues per VF
  for (int i = 0; i < num_dev; ++i) {
    io_cfg->dpdk_dev[i] = strdup(gpd(fhip, nump, ORAN_CONFIG_DPDK_DEVICES)->strlistptr[i]); // VFs devices
  }
  int dpdk_iova_mode_idx = config_paramidx_fromname((paramdef_t *)fhip, nump, ORAN_CONFIG_DPDK_IOVA_MODE);
  AssertFatal(dpdk_iova_mode_idx >= 0,"Index for dpdk_iova_mode config option not found!");
  io_cfg->dpdkIoVaMode = config_get_processedint(config_get_if(), (paramdef_t *)&fhip[dpdk_iova_mode_idx]); // IOVA mode
  io_cfg->dpdkMemorySize = *gpd(fhip, nump, ORAN_CONFIG_DPDK_MEM_SIZE)->uptr; // DPDK max memory allocation

  /* the following core assignment is needed for rte_eal_init() function within xran library;
    these parameters are machine specific */
  io_cfg->core = *gpd(fhip, nump, ORAN_CONFIG_IO_CORE)->iptr; // core used for IO; absolute CPU core ID for xran library, it should be an isolated core
  io_cfg->system_core = *gpd(fhip, nump, ORAN_CONFIG_SYSTEM_CORE)->iptr; // absolute CPU core ID for DPDK control threads, it should be an isolated core
  get_u128_mask(gpd(fhip, nump, ORAN_CONFIG_WORKER_CORES), &io_cfg->pkt_proc_core, &io_cfg->pkt_proc_core_64_127); // worker masks 0-63 and 64-127
  io_cfg->pkt_aux_core = 0; // sample app says 0 = "do not start"
  io_cfg->timing_core = *gpd(fhip, nump, ORAN_CONFIG_IO_CORE)->iptr; // core used by xran

  // io_cfg->port[XRAN_VF_MAX] // VFs ports; filled within xran library
  io_cfg->io_sleep = 0; // enable sleep on PMD cores; 0 -> no sleep
  io_cfg->nEthLinePerPort = *gpd(fhip, nump, ORAN_CONFIG_NETHPERPORT)->uptr; // 1, 2, 3 total number of links per O-RU (Fronthaul Ethernet link)
  io_cfg->nEthLineSpeed = *gpd(fhip, nump, ORAN_CONFIG_NETHSPEED)->uptr; // 10G,25G,40G,100G speed of Physical connection on O-RU
  io_cfg->num_mbuf_alloc = NUM_MBUFS; // number of mbuf allocated by DPDK (optimal is n = (2^q - 1))
  io_cfg->num_mbuf_vf_alloc = NUM_MBUFS_VF; // number of mbuf allocated by DPDK (optimal is n = (2^q - 1))
  io_cfg->one_vf_cu_plane = (io_cfg->num_vfs == num_rus); // C-plane and U-plane use one VF

  /* eCPRI One-Way Delay Measurements common settings for O-DU and O-RU;
    use owdm to calculate T12 and T34 -> CUS specification, section 2.3.3.3;
    this is an optional feature that RU might or might not support;
    to verify if RU supports, please check in the official RU documentation or
    via M-plane the o-ran-ecpri-delay@<version>.yang capability;
    this functionality is improved in F release */
  /* if RU does support, io_cfg->eowd_cmn[0] should only be filled as id = O_DU; io_cfg->eowd_cmn[1] only used if id = O_RU */
  const uint16_t owdm_enable = *gpd(fhip, nump, ORAN_CONFIG_ECPRI_OWDM)->uptr;
  if (owdm_enable) {
    io_cfg->eowd_cmn[XRAN_APP_ID_O_DU].initiator_en = io_cfg->id == XRAN_APP_ID_O_DU ? 1 : 0; // 1 -> initiator (always O-DU), 0 -> recipient (always O-RU)
    io_cfg->eowd_cmn[XRAN_APP_ID_O_DU].numberOfSamples = 8; // total number of samples to be collected and averaged per port
    io_cfg->eowd_cmn[XRAN_APP_ID_O_DU].filterType = 0; // 0 -> simple average based on number of measurements; not used in xran in both E and F releases
    io_cfg->eowd_cmn[XRAN_APP_ID_O_DU].responseTo = 10000000; // response timeout in [ns]
    io_cfg->eowd_cmn[XRAN_APP_ID_O_DU].measVf = 0; // VF using the OWD transmitter; within xran, the measurements are calculated per each supported VF, but starts from measVf
    io_cfg->eowd_cmn[XRAN_APP_ID_O_DU].measState = 0; // the state of the OWD transmitter; 0 -> OWDMTX_INIT (enum xran_owdm_tx_state)
    io_cfg->eowd_cmn[XRAN_APP_ID_O_DU].measId = 0; // measurement ID to be used by the transmitter
    io_cfg->eowd_cmn[XRAN_APP_ID_O_DU].measMethod = 0; // measurement method; 0 -> XRAN_REQUEST (enum xran_owd_meas_method)
    io_cfg->eowd_cmn[XRAN_APP_ID_O_DU].owdm_enable = 1; // 1 -> enabled; 0 -> disabled
    io_cfg->eowd_cmn[XRAN_APP_ID_O_DU].owdm_PlLength = 40; // payload in the measurement packet; 40 <= PlLength <= 1400
  }
  /* eCPRI OWDM per port variables for O-DU; this parameter is filled within xran library */
  // eowd_port[0][XRAN_VF_MAX]

  io_cfg->bbu_offload = 0; // enable packet handling on BBU cores

  return true;
}

static bool set_fh_bbdev_params(struct xran_fh_init *fh_init)
{
  struct xran_io_cfg *io_cfg = &fh_init->io_cfg;
  io_cfg->bbdev_dev[0] = NULL; // BBDev dev name; max devices = 1
  fh_init->dpdkVfioVfToken = NULL; // BBDev dev token; max devices = 1
  char *shlibversion = NULL; // version of the LDPC coding library
  paramdef_t LoaderParams_shlibversion[] = {{"shlibversion", NULL, 0, .strptr = &shlibversion, .defstrval = NULL, TYPE_STRING, 0, NULL}};
  config_get(config_get_if(), LoaderParams_shlibversion, sizeofArray(LoaderParams_shlibversion), "loader.ldpc");
  if (shlibversion != NULL && strncmp(shlibversion, "_aal", 4) == 0) {
    uint32_t is_t2 = 0;    // If not 0 then include the BBDEV device in the EAL init for FHI
    char *dpdk_dev = NULL;          // PCI address of the card
    char *vfio_vf_token = NULL;     // vfio token for the bbdev card
    paramdef_t LoaderParams[] = {
      {"is_t2", NULL, 0, .uptr = &is_t2, .defuintval = 0, TYPE_UINT, 0, NULL},
      {"dpdk_dev", NULL, 0, .strptr = &dpdk_dev, .defstrval = NULL, TYPE_STRING, 0, NULL},
      {"vfio_vf_token", NULL, 0, .strptr = &vfio_vf_token, .defstrval = NULL, TYPE_STRING, 0, NULL}
    };
    config_get(config_get_if(), LoaderParams, sizeofArray(LoaderParams), "nrLDPC_coding_aal");

    if (!is_t2) {
      AssertFatal(dpdk_dev!=NULL, "nrLDPC_coding_aal.dpdk_dev was not provided");
      snprintf(&bbdev_dev[0], sizeof(bbdev_dev), "%s", dpdk_dev);
      io_cfg->bbdev_dev[0] = &bbdev_dev[0]; // BBDev dev name; max devices = 1
      if(vfio_vf_token != NULL) {
        snprintf(&bbdev_vfio_vf_token[0], sizeof(bbdev_vfio_vf_token), "%s", vfio_vf_token);
        fh_init->dpdkVfioVfToken = &bbdev_vfio_vf_token[0]; // BBDev dev token; max devices = 1
      } else {
        fh_init->dpdkVfioVfToken = NULL; // BBDev dev token; max devices = 1
      }
      io_cfg->bbdev_mode = XRAN_BBDEV_MODE_HW_ON; // DPDK for BBDev
    } else {
      io_cfg->bbdev_mode = XRAN_BBDEV_NOT_USED; // DPDK for BBDev
    }
  } else {
    io_cfg->bbdev_mode = XRAN_BBDEV_NOT_USED; // DPDK for BBDev
  }

  return true;
}

#ifdef OAI_MPLANE
static bool set_fh_eaxcid_conf_mplane(struct xran_eaxcid_config *eaxcid_conf, enum xran_category cat, const ru_session_list_t *ru_session_list)
{
  xran_mplane_t *xran_mplane = &ru_session_list->ru_session[0].xran_mplane;
  switch (cat) {
    case XRAN_CATEGORY_A:
      eaxcid_conf->mask_cuPortId = xran_mplane->du_port_bitmask;
      eaxcid_conf->mask_bandSectorId = xran_mplane->band_sector_bitmask;
      eaxcid_conf->mask_ccId = xran_mplane->ccid_bitmask;
      eaxcid_conf->mask_ruPortId = xran_mplane->ru_port_bitmask;
      eaxcid_conf->bit_cuPortId = xran_mplane->du_port;
      eaxcid_conf->bit_bandSectorId = xran_mplane->band_sector; // total number of band sectors supported by O-RU should be retrieved by M-plane - <max-num-bands> && <max-num-sectors>
      eaxcid_conf->bit_ccId = xran_mplane->ccid; // total number of CC supported by O-RU should be retrieved by M-plane - <max-num-component-carriers>
      eaxcid_conf->bit_ruPortId = xran_mplane->ru_port;
      break;
    case XRAN_CATEGORY_B:
      eaxcid_conf->mask_cuPortId = 0xf000;
      eaxcid_conf->mask_bandSectorId = 0x0c00;
      eaxcid_conf->mask_ccId = 0x0300;
      eaxcid_conf->mask_ruPortId = 0x000f;
      eaxcid_conf->bit_cuPortId = 12;
      eaxcid_conf->bit_bandSectorId = 10;
      eaxcid_conf->bit_ccId = 8;
      eaxcid_conf->bit_ruPortId = 0;
      break;
    default:
      return false;
  }

  return true;
}
#else
static bool __attribute__((unused)) set_fh_eaxcid_conf(struct xran_eaxcid_config *eaxcid_conf, enum xran_category cat)
{
  /* CUS specification, section 3.1.3.1.6
    DU_port_ID - used to differentiate processing units at O-DU (e.g., different baseband cards).
    BandSector_ID - aggregated cell identifier (distinguishes bands and sectors supported by the O-RU).
    CC_ID - distinguishes Carrier Components supported by the O-RU.
    RU_Port_ID - designates logical flows such as data layers or spatial streams, and logical flows such as separate
                 numerologies (e.g. PRACH) or signaling channels requiring special antenna assignments such as SRS.
    The assignment of the DU_port_ID, BandSector_ID, CC_ID, and RU_Port_ID
    as part of the eAxC ID is done solely by the O-DU via the M-plane.
    Each ID field has a flexible bit allocation, but the total eAxC ID field length is fixed, 16 bits. */
  switch (cat) {
    case XRAN_CATEGORY_A:
      eaxcid_conf->mask_cuPortId = 0xf000;
      eaxcid_conf->mask_bandSectorId = 0x0f00;
      eaxcid_conf->mask_ccId = 0x00f0;
      eaxcid_conf->mask_ruPortId = 0x000f;
      eaxcid_conf->bit_cuPortId = 0;
      eaxcid_conf->bit_bandSectorId = 0; // total number of band sectors supported by O-RU should be retrieved by M-plane - <max-num-bands> && <max-num-sectors>
      eaxcid_conf->bit_ccId = 0; // total number of CC supported by O-RU should be retrieved by M-plane - <max-num-component-carriers>
      eaxcid_conf->bit_ruPortId = 0;
      break;
    case XRAN_CATEGORY_B:
      eaxcid_conf->mask_cuPortId = 0xf000;
      eaxcid_conf->mask_bandSectorId = 0x0c00;
      eaxcid_conf->mask_ccId = 0x0300;
      eaxcid_conf->mask_ruPortId = 0x000f;
      eaxcid_conf->bit_cuPortId = 12;
      eaxcid_conf->bit_bandSectorId = 10;
      eaxcid_conf->bit_ccId = 8;
      eaxcid_conf->bit_ruPortId = 0;
      break;
    default:
      return false;
  }

  return true;
}
#endif

uint8_t *get_ether_addr(const char *addr, struct rte_ether_addr *ether_addr)
{
#pragma GCC diagnostic push
  // the following line disables the deprecated warning
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  int ret = rte_ether_unformat_addr(addr, ether_addr);
#pragma GCC diagnostic pop
  if (ret == 0)
    return (uint8_t *)ether_addr;
  return NULL;
}

static bool set_fh_init(void *mplane_api, struct xran_fh_init *fh_init, enum xran_category xran_cat)
{
  memset(fh_init, 0, sizeof(*fh_init));

  // verify oran section is present: we don't have a list but the below returns
  // numelt > 0 if the block is there
  paramlist_def_t pl = {0};
  strncpy(pl.listname, CONFIG_STRING_ORAN, sizeof(pl.listname) - 1);
  config_getlist(config_get_if(), &pl, NULL, 0, /* prefix */ NULL);
  if (pl.numelt == 0) {
    printf("Configuration section \"%s\" not present: cannot initialize fhi_lib!\n", CONFIG_STRING_ORAN);
    return false;
  }

  paramdef_t fhip[] = ORAN_GLOBALPARAMS_DESC;
  checkedparam_t fhip_CheckParams[] = ORAN_GLOBALPARAMS_CHECK_DESC;
  static_assert(sizeofArray(fhip) == sizeofArray(fhip_CheckParams),
		"fhip and fhip_CheckParams should have the same size");
  int nump = sizeofArray(fhip);
  config_set_checkfunctions(fhip, fhip_CheckParams, nump);
  int ret = config_get(config_get_if(), fhip, nump, CONFIG_STRING_ORAN);
  if (ret <= 0) {
    printf("problem reading section \"%s\"\n", CONFIG_STRING_ORAN);
    return false;
  }

  paramdef_t FHconfigs[] = ORAN_FH_DESC;
  paramlist_def_t FH_ConfigList = {CONFIG_STRING_ORAN_FH};
  char aprefix[MAX_OPTNAME_SIZE] = {0};
  sprintf(aprefix, "%s", CONFIG_STRING_ORAN);
  const int nfh = sizeofArray(FHconfigs);
  config_getlist(config_get_if(), &FH_ConfigList, FHconfigs, nfh, aprefix);

#ifdef OAI_MPLANE
  ru_session_list_t *ru_session_list = (ru_session_list_t *)mplane_api;
  int num_rus = ru_session_list->num_rus;
  fh_init->xran_ports = num_rus; // since we use xran as O-DU, xran_ports is set to the number of RUs
  if (!set_fh_io_cfg(&fh_init->io_cfg, fhip, nump, num_rus))
    return false;
  if (!set_fh_bbdev_params(fh_init))
    return false;
  for (int32_t o_xu_id = 0; o_xu_id < fh_init->xran_ports; o_xu_id++) {
    if (!set_fh_eaxcid_conf_mplane(&fh_init->eAxCId_conf[o_xu_id], xran_cat, ru_session_list))
      return false;
  }

  /* maximum transmission unit (MTU) is the size of the largest protocol data unit (PDU) that can be
    communicated in a single xRAN network layer transaction. Based on the MTU size, xran calculates the number
    of DL fragments (nPrbElm) needed for transmission of one symbol. */
  fh_init->mtu = ru_session_list->ru_session[0].xran_mplane.mtu; // we suppose that each RU supports the same MTU size

  int num_ru_addr = (fh_init->io_cfg.one_vf_cu_plane) ? num_rus : 2*num_rus;
  fh_init->p_o_ru_addr = calloc(num_ru_addr, sizeof(struct rte_ether_addr));
  AssertFatal(fh_init->p_o_ru_addr != NULL, "out of memory\n");
  for (int i = 0; i < num_rus; i++) {
    struct rte_ether_addr *ea = (struct rte_ether_addr *)fh_init->p_o_ru_addr;
    for (int j = 0; j < num_ru_addr/num_rus; j++) {
      if (get_ether_addr(ru_session_list->ru_session[i].xran_mplane.ru_mac_addr, &ea[i+j]) == NULL) {
        printf("could not read ethernet address '%s' for RU!\n", ru_session_list->ru_session[i].xran_mplane.ru_mac_addr);
        return false;
      }
    }
  }
#else
  int num_rus = FH_ConfigList.numelt; // based on the number of fh_config sections -> number of RUs
  fh_init->xran_ports = num_rus;

  if (!set_fh_io_cfg(&fh_init->io_cfg, fhip, nump, num_rus))
    return false;
  if (!set_fh_bbdev_params(fh_init))
    return false;
  for (int32_t o_xu_id = 0; o_xu_id < fh_init->xran_ports; o_xu_id++) {
    if (!set_fh_eaxcid_conf(&fh_init->eAxCId_conf[o_xu_id], xran_cat))
      return false;
  }

  /* maximum transmission unit (MTU) is the size of the largest protocol data unit (PDU) that can be
    communicated in a single xRAN network layer transaction. Based on the MTU size, xran calculates the number
    of DL fragments (nPrbElm) needed for transmission of one symbol. */
  fh_init->mtu = *gpd(fhip, nump, ORAN_CONFIG_MTU)->uptr;

  if (fh_init->io_cfg.id == XRAN_APP_ID_O_DU) {
    int num_ru_addr = gpd(fhip, nump, ORAN_CONFIG_RU_ADDR)->numelt;
    fh_init->p_o_ru_addr = calloc(num_ru_addr, sizeof(struct rte_ether_addr));
    char **ru_addrs = gpd(fhip, nump, ORAN_CONFIG_RU_ADDR)->strlistptr;
    AssertFatal(fh_init->p_o_ru_addr != NULL, "out of memory\n");
    for (int i = 0; i < num_ru_addr; ++i) {
      struct rte_ether_addr *ea = (struct rte_ether_addr *)fh_init->p_o_ru_addr;
      if (get_ether_addr(ru_addrs[i], &ea[i]) == NULL) {
        printf("could not read ethernet address '%s' for RU!\n", ru_addrs[i]);
        return false;
      }
    }
    // DPDK retreives DU MAC address within the xran library with rte_eth_macaddr_get() function
    fh_init->p_o_du_addr = NULL;
  } else {
    int num_du_addr = gpd(fhip, nump, ORAN_CONFIG_DU_ADDR)->numelt;
    fh_init->p_o_du_addr = calloc(num_du_addr, sizeof(struct rte_ether_addr));
    char **du_addrs = gpd(fhip, nump, ORAN_CONFIG_DU_ADDR)->strlistptr;
    AssertFatal(fh_init->p_o_du_addr != NULL, "out of memory\n");
    for (int i = 0; i < num_du_addr; ++i) {
      struct rte_ether_addr *ea = (struct rte_ether_addr *)fh_init->p_o_du_addr;
      if (get_ether_addr(du_addrs[i], &ea[i]) == NULL) {
        printf("could not read ethernet address '%s' for DU!\n", du_addrs[i]);
        return false;
      }
    }
    // DPDK retreives RU MAC address within the xran library with rte_eth_macaddr_get() function
    fh_init->p_o_ru_addr = NULL;
  }
#endif

  fh_init->dpdkBasebandFecMode = 0; // DPDK Baseband FEC device mode (0-SW, 1-HW); not used in xran
  fh_init->dpdkBasebandDevice = NULL; // DPDK Baseband device address; not used in xran
  /* used to specify a unique prefix for shared memory, and files created by multiple DPDK processes;
    it is necessary */
  fh_init->filePrefix = strdup(*gpd(fhip, nump, ORAN_CONFIG_FILE_PREFIX)->strptr);
  fh_init->totalBfWeights = 0; // only used if id = O_RU (for emulation); C-plane extension types; section 5.4.6 of CUS spec

  fh_init->mlogxranenable = 0; // enable mlog; 0 -> disabled
  fh_init->dlCpProcBurst = 0; /* 1 -> DL CP processing will be done on single symbol,
                                 0 -> DL CP processing will be spread across all allowed symbols and multiple cores to reduce burstiness */

  return true;
}

// PRACH guard interval. Raymond: "[it] is not in the configuration, (i.e. it
// is deterministic depending on others). LiteON must hard-code this in the
// O-RU itself, benetel doesn't (as O-RAN specifies). So we will need to tell
// the driver what the case is and provide"
// this is a hack
int g_kbar;

static bool set_fh_prach_config(void *mplane_api,
                                const openair0_config_t *oai0,
                                const uint32_t max_num_ant,
                                const paramdef_t *prachp,
                                int nprach,
                                struct xran_prach_config *prach_config)
{
  const split7_config_t *s7cfg = &oai0->split7;

  prach_config->nPrachConfIdx = s7cfg->prach_index; // PRACH Configuration Index
  prach_config->nPrachSubcSpacing = oai0->nr_scs_for_raster; // 0 -> 15kHz, 1 -> 30kHz, 2 -> 60kHz, 3 -> 120kHz
  prach_config->nPrachZeroCorrConf = 0; // PRACH zeroCorrelationZoneConfig; should be saved from config file; not used in xran
  prach_config->nPrachRestrictSet = 0; /* PRACH restrictedSetConfig; should be saved from config file; 0 = unrestricted,
                                          1 = restricted type A, 2=restricted type B; not used in xran */
  prach_config->nPrachRootSeqIdx = 0; // PRACH Root Sequence Index; should be saved from config file; 1 = 839, 2 = 139; not used in xran
  prach_config->nPrachFreqStart = s7cfg->prach_freq_start; // PRACH frequency start (MSG1)
  prach_config->nPrachFreqOffset = (s7cfg->prach_freq_start * 12 - oai0->num_rb_dl * 6) * 2; // PRACH frequency offset
  prach_config->nPrachFilterIdx = 0; /* PRACH filter index; not used in xran;
                                        in E release hardcoded to XRAN_FILTERINDEX_PRACH_ABC (preamble format A1~3, B1~4, C0, C2)
                                        in F release properly calculated */

  /* Return values after initialization */
  prach_config->startSymId = 0;
  prach_config->lastSymId = 0;
  prach_config->startPrbc = 0;
  prach_config->numPrbc = 0;
  prach_config->timeOffset = 0;
  prach_config->freqOffset = 0;
  prach_config->nPrachConfIdxLTE = 0; // used only if DSS enabled and technology is XRAN_RAN_LTE

  /* xran defines PDSCH eAxC IDs as [0...Ntx-1];
     xran defines PUSCH eAxC IDs as [0...Nrx-1];
     xran assumes PRACH offset >= max(Ntx, Nrx). However, we made a workaround that xran supports PRACH eAxC IDs same as PUSCH eAxC IDs.
     This is achieved with is_prach and filter_id parameters in the patch.
     Please note that this approach only applies to the RUs that support this functionality, e.g. LITEON RU. */
#ifdef OAI_MPLANE
  xran_mplane_t *xran_mplane = (xran_mplane_t *)mplane_api;
  prach_config->prachEaxcOffset = xran_mplane->prach_offset;
#else
  const paramdef_t *pd = gpd(prachp, nprach, ORAN_PRACH_CONFIG_EAXC_OFFSET);
  prach_config->prachEaxcOffset = pd->paramflags & PARAMFLAG_PARAMSET ? *pd->u8ptr : max_num_ant;
#endif

  g_kbar = *gpd(prachp, nprach, ORAN_PRACH_CONFIG_KBAR)->uptr;

  return true;
}

static bool set_fh_frame_config(const openair0_config_t *oai0, struct xran_frame_config *frame_config)
{
  const split7_config_t *s7cfg = &oai0->split7;
  frame_config->nFrameDuplexType = oai0->duplex_mode == duplex_mode_TDD ? XRAN_TDD : XRAN_FDD; // Frame Duplex type:  0 -> FDD, 1 -> TDD

  if (frame_config->nFrameDuplexType == XRAN_FDD)
    return true;

  // TDD periodicity
  frame_config->nTddPeriod = s7cfg->n_tdd_period;

  // TDD Slot configuration
  struct xran_slot_config *sc = &frame_config->sSlotConfig[0];
  for (int slot = 0; slot < frame_config->nTddPeriod; ++slot)
    for (int sym = 0; sym < 14; ++sym)
      sc[slot].nSymbolType[sym] = s7cfg->slot_dirs[slot].sym_dir[sym];

  return true;
}

static bool set_fh_ru_config(void *mplane_api, const paramdef_t *rup, uint16_t fftSize, int nru, enum xran_category xran_cat, uint8_t mu_number, struct xran_ru_config *ru_config)
{
  ru_config->xranTech = XRAN_RAN_5GNR; // 5GNR or LTE
  ru_config->xranCat = xran_cat; // mode: Catergory A or Category B
#ifdef OAI_MPLANE
  xran_mplane_t *xran_mplane = (xran_mplane_t *)mplane_api;
  ru_config->xranCompHdrType = xran_mplane->comp_hdr_type;
  ru_config->iqWidth = xran_mplane->iq_width;
  ru_config->iqWidth_PRACH = xran_mplane->iq_width;
#else
  int comp_type_idx = config_paramidx_fromname(rup, nru, ORAN_RU_COMP_HDR_TYPE);
  AssertFatal(comp_type_idx >= 0, "Index for %s config option not found!\n", ORAN_RU_COMP_HDR_TYPE);
  ru_config->xranCompHdrType = config_get_processedint(config_get_if(), (paramdef_t *)&rup[comp_type_idx]); // 0 -> XRAN_COMP_HDR_TYPE_DYNAMIC, 1 -> XRAN_COMP_HDR_TYPE_STATIC
  ru_config->iqWidth = *gpd(rup, nru, ORAN_RU_CONFIG_IQWIDTH)->uptr; // IQ bit width
  AssertFatal(ru_config->iqWidth <= 16, "IQ Width cannot be > 16!\n");
  ru_config->iqWidth_PRACH = *gpd(rup, nru, ORAN_RU_CONFIG_IQWIDTH_PRACH)->uptr; // IQ bit width for PRACH
  AssertFatal(ru_config->iqWidth_PRACH <= 16, "IQ Width for PRACH cannot be > 16!\n");
#endif
  ru_config->compMeth = ru_config->iqWidth < 16 ? XRAN_COMPMETHOD_BLKFLOAT : XRAN_COMPMETHOD_NONE; // compression method
  ru_config->compMeth_PRACH = ru_config->iqWidth_PRACH < 16 ? XRAN_COMPMETHOD_BLKFLOAT : XRAN_COMPMETHOD_NONE; // compression method for PRACH

  AssertFatal(fftSize > 0, "FFT size cannot be 0\n");
  ru_config->fftSize[mu_number] = fftSize; // FFT Size
  ru_config->byteOrder = XRAN_NE_BE_BYTE_ORDER; // order of bytes in int16_t in buffer; big or little endian
  ru_config->iqOrder = XRAN_I_Q_ORDER; // order of IQs in the buffer
  ru_config->xran_max_frame = 0; // max frame number supported; if not specified, default of 1023 is used
  return true;
}

static bool set_maxmin_pd(const paramdef_t *pd, int num, const char *name, uint16_t *min, uint16_t *max)
{
  const paramdef_t *p = gpd(pd, num, name);
  if (p->numelt != 2) {
    printf("parameter list \"%s\" should have exactly two parameters (max&min), but has %d\n", name, num);
    return false;
  }
  *min = p->uptr[0];
  *max = p->uptr[1];
  if (*min > *max) {
    printf("min parameter of \"%s\" is larger than max!\n", name);
    return false;
  }
  return true;
}

static bool set_fh_per_mu_cfg(void *mplane_api, int ru_idx, int num_rus, const openair0_config_t *oai0, struct xran_fh_per_mu_cfg *perMu)
{
  char aprefix[MAX_OPTNAME_SIZE] = {0};

  paramdef_t FHconfigs[] = ORAN_FH_DESC;
  paramlist_def_t FH_ConfigList = {CONFIG_STRING_ORAN_FH};
  sprintf(aprefix, "%s", CONFIG_STRING_ORAN);
  const int nfh = sizeofArray(FHconfigs);
  config_getlist(config_get_if(), &FH_ConfigList, FHconfigs, nfh, aprefix);
  if (FH_ConfigList.numelt == 0) {
    printf("No configuration section \"%s\" found inside \"%s\": cannot initialize fhi_lib!\n", CONFIG_STRING_ORAN_FH, aprefix);
    return false;
  }
  paramdef_t *fhp = FH_ConfigList.paramarray[ru_idx];

  paramdef_t prachp[] = ORAN_PRACH_DESC;
  int nprach = sizeofArray(prachp);
  sprintf(aprefix, "%s.%s.[%d].%s", CONFIG_STRING_ORAN, CONFIG_STRING_ORAN_FH, ru_idx, CONFIG_STRING_ORAN_PRACH);
  int ret = config_get(config_get_if(), prachp, nprach, aprefix);
  if (ret < 0) {
    printf("No configuration section \"%s\": cannot initialize fhi_lib!\n", aprefix);
    return false;
  }

  perMu->nDLBandwidth = 0; // Carrier bandwidth for in MHz. Value: 5->400. Not used in K release
  perMu->nULBandwidth = 0; // Carrier bandwidth for in MHz. Value: 5->400. Not used in K release
  if (!set_fh_prach_config(mplane_api, oai0, RTE_MAX(oai0->tx_num_channels / num_rus, oai0->rx_num_channels / num_rus), prachp, nprach, &perMu->prach_conf))
    return false;
  perMu->freqOffset = 0;

  perMu->nDLFftSize = oai0->split7.fftSize; // DL FFT size
  perMu->nULFftSize = oai0->split7.fftSize; // UL FFT size

  perMu->eaxcOffset = 0; // Starting value of Eaxc for PDSCH, PUSCH packets (Absolute value) of this numerology. Should be unique across all numerologies for the RU

  perMu->nDLRBs = oai0->num_rb_dl; // DL PRB
  perMu->nULRBs = oai0->num_rb_dl; // UL PRB

  /* DU delay profile */
  if (!set_maxmin_pd(fhp, nfh, ORAN_FH_CONFIG_T1A_CP_DL, &perMu->T1a_min_cp_dl, &perMu->T1a_max_cp_dl)) // E - min not used in xran, max yes; F - both min and max are used in xran
    return false;
  if (!set_maxmin_pd(fhp, nfh, ORAN_FH_CONFIG_T1A_CP_UL, &perMu->T1a_min_cp_ul, &perMu->T1a_max_cp_ul)) // both E and F - min not used in xran, max yes
    return false;
  if (!set_maxmin_pd(fhp, nfh, ORAN_FH_CONFIG_T1A_UP, &perMu->T1a_min_up, &perMu->T1a_max_up)) // both E and F - min not used in xran, max yes
    return false;
  if (!set_maxmin_pd(fhp, nfh, ORAN_FH_CONFIG_TA4, &perMu->Ta4_min, &perMu->Ta4_max)) // both E and F - min not used in xran, max yes
    return false;

  perMu->prachEnable = 1; // enable PRACH
  const split7_config_t *s7cfg = &oai0->split7;
  perMu->prachConfigIndex = s7cfg->prach_index; // TS36.211 - Table 5.7.1-2 : PRACH Configuration Index
  perMu->prachConfigIndexLTE = 0; // PRACH Configuration Index for LTE in dss case

  perMu->nbIotUlScs = XRAN_NBIOT_UL_SCS_15; /* Applicable only for NB-IOT (mu=4). NBIOT supports asymmetric SCS usage in
                 downlink and uplink directions. xran library will use this parameter to derive
                 slot-duration for UL NB-IOT:
                 XRAN_NBIOT_UL_SCS_15: slot-duration=1ms
                 XRAN_NBIOT_UL_SCS_3_75: slot-duration=2ms */
  perMu->adv_tx_time = 0; // Time by which the packet should be transmitted in advance (microseconds)
  return true;
}

xran_active_numerologies_per_tti activeMUs;

static bool set_activeMUs(xran_active_numerologies_per_tti *p_activeMUs, uint8_t mu)
{
  for (int i = 0; i < XRAN_N_FE_BUF_LEN; i++) {
    for (int j = 0; j < XRAN_MAX_NUM_MU; j++) {
      p_activeMUs->numerology[i][j] = j == mu;
    }
  }
  return true;
}

static bool set_fh_config(void *mplane_api, int ru_idx, int num_rus, enum xran_category xran_cat, const openair0_config_t *oai0, struct xran_fh_config *fh_config)
{
  AssertFatal(num_rus == 1 || num_rus == 2, "only support 1 or 2 RUs as of now\n");
  AssertFatal(ru_idx < num_rus, "illegal ru_idx %d: must be < %d\n", ru_idx, num_rus);
  DevAssert(oai0->tx_num_channels > 0 && oai0->rx_num_channels > 0 && oai0->num_distributed_ru > 0);
  DevAssert(oai0->tx_bw > 0 && oai0->rx_bw > 0);
  DevAssert(oai0->tx_freq[0] > 0);
  for (int i = 1; i < oai0->tx_num_channels; ++i)
    DevAssert(oai0->tx_freq[0] == oai0->tx_freq[i]);
  DevAssert(oai0->rx_freq[0] > 0);
  for (int i = 1; i < oai0->rx_num_channels; ++i)
    DevAssert(oai0->rx_freq[0] == oai0->rx_freq[i]);
  paramdef_t FHconfigs[] = ORAN_FH_DESC;
  paramlist_def_t FH_ConfigList = {CONFIG_STRING_ORAN_FH};
  char aprefix[MAX_OPTNAME_SIZE] = {0};
  sprintf(aprefix, "%s", CONFIG_STRING_ORAN);
  const int nfh = sizeofArray(FHconfigs);
  config_getlist(config_get_if(), &FH_ConfigList, FHconfigs, nfh, aprefix);
  if (FH_ConfigList.numelt == 0) {
    printf("No configuration section \"%s\" found inside \"%s\": cannot initialize fhi_lib!\n", CONFIG_STRING_ORAN_FH, aprefix);
    return false;
  }
  paramdef_t *fhp = FH_ConfigList.paramarray[ru_idx];

  paramdef_t rup[] = ORAN_RU_DESC;
  int nru = sizeofArray(rup);
  sprintf(aprefix, "%s.%s.[%d].%s", CONFIG_STRING_ORAN, CONFIG_STRING_ORAN_FH, ru_idx, CONFIG_STRING_ORAN_RU);
  int ret = config_get(config_get_if(), rup, nru, aprefix);
  if (ret < 0) {
    printf("No configuration section \"%s\": cannot initialize fhi_lib!\n", aprefix);
    return false;
  }

  memset(fh_config, 0, sizeof(*fh_config));

  fh_config->dpdk_port = ru_idx; // DPDK port number used for FH
  fh_config->sector_id = 0; // Band sector ID for FH; not used in xran
  fh_config->nCC = 1; // number of Component carriers supported on FH; M-plane info
  fh_config->neAxc = RTE_MAX(oai0->tx_num_channels / num_rus, oai0->rx_num_channels / num_rus); // number of eAxc supported on one CC = max(PDSCH, PUSCH)
  fh_config->neAxcUl = oai0->rx_num_channels / num_rus; // number of eAxc supported on one CC for UL direction = PUSCH
  fh_config->nAntElmTRx = 0; // number of antenna elements for TX and RX = SRS; used only if XRAN_CATEGORY_B
  fh_config->nDLAbsFrePointA = 0; // Abs Freq Point A of the Carrier Center Frequency for in KHz Value; not used in xran
  fh_config->nULAbsFrePointA = 0; // Abs Freq Point A of the Carrier Center Frequency for in KHz Value; not used in xran
  fh_config->nDLCenterFreqARFCN = 0; // center frequency for DL in NR-ARFCN; not used in xran
  fh_config->nULCenterFreqARFCN = 0; // center frequency for UL in NR-ARFCN; not used in xran
  fh_config->ttiCb = NULL; // check tti_to_phy_cb(), tx_cp_dl_cb() and tx_cp_ul_cb => first_call
  fh_config->ttiCbParam = NULL; // check tti_to_phy_cb(), tx_cp_dl_cb() and tx_cp_ul_cb => first_call

  uint8_t mu_number = oai0->nr_scs_for_raster;
  if(!set_fh_per_mu_cfg(mplane_api, ru_idx, num_rus, oai0, &fh_config->perMu[mu_number]))
    return false;

  fh_config->enableCP = 1; // enable C-plane
  fh_config->srsEnable = 0; // enable SRS; used only if XRAN_CATEGORY_B
  fh_config->puschMaskEnable = 0; // enable PUSCH mask; only used if id = O_RU
  fh_config->puschMaskSlot = 0; // specific which slot PUSCH channel masked; only used if id = O_RU
  fh_config->csirsEnable = 0; // enable CSI-RS (Cat B specific)
  fh_config->debugStop = 0; // enable auto stop; only used if id = O_RU
  fh_config->debugStopCount = 0; // enable auto stop after number of Tx packets; not used in xran
  fh_config->DynamicSectionEna = 0; // enable dynamic C-Plane section allocation
  fh_config->GPS_Alpha = 0; // refers to alpha as defined in section 9.7.2 of ORAN spec. this value should be alpha*(1/1.2288ns), range 0 - 1e7 (ns); offset_nsec = (pConf->GPS_Beta - offset_sec * 100) * 1e7 + pConf->GPS_Alpha
  fh_config->GPS_Beta = 0; // beta value as defined in section 9.7.2 of ORAN spec. range -32767 ~ +32767; offset_sec = pConf->GPS_Beta / 100
  fh_config->numMUs = 1;
  fh_config->mu_number[0] = mu_number; /* 0 -> 15kHz,  1 -> 30kHz,  2 -> 60kHz, 3 -> 120kHz, 4 -> 240kHz */
  fh_config->nNumerology[0] = mu_number; /* 0 -> 15kHz,  1 -> 30kHz,  2 -> 60kHz, 3 -> 120kHz, 4 -> 240kHz */

  /* SRS only used if XRAN_CATEGORY_B
    Note: srs_config->eAxC_offset >= prach_config->eAxC_offset + PRACH */
  // fh_config->srs_conf = {0};
  if (!set_fh_frame_config(oai0, &fh_config->frame_conf))
    return false;
  if (!set_fh_ru_config(mplane_api, rup, oai0->split7.prach_fftSize, nru, xran_cat, mu_number, &fh_config->ru_conf))
    return false;

  fh_config->bbdev_enc = NULL; // call back to poll BBDev encoder
  fh_config->bbdev_dec = NULL; // call back to poll BBDev decoder

  /* CUS specification, section 3.1.3.1.6 
    This parameter is an eAxC identifier (eAxC ID) and identifies the specific data flow associated with each
    C-Plane (ecpriRtcid) or U-Plane (ecpriPcid) message.
    Each of bellow parameters is a matrix [XRAN_MAX_SECTOR_NR][XRAN_MAX_ANTENNA_NR*2 + XRAN_MAX_ANT_ARRAY_ELM_NR] */
  // fh_config->tx_cp_eAxC2Vf // mapping of C-Plane (ecpriRtcid) to VF; not used in xran
  // fh_config->tx_up_eAxC2Vf // mapping of U-Plane (ecpriPcid) to VF; not used in xran
  // fh_config->rx_cp_eAxC2Vf // mapping of C-Plane (ecpriRtcid) to VF; not used in xran
  // fh_config->rx_up_eAxC2Vf // mapping of U-Plane (ecpriPcid) to VF; not used in xran

  fh_config->log_level = 1; // configuration of log level; 1 -> enabled

  /* Parameters that should be retreived via M-plane
    O-RU defines them for Section Type 1 (Most DL/UL radio channels) and 3 (PRACH and mixed-numerology channels)
    Note: When PRACH having same numerology as other UL channels, Section type 1 can alternatively be used by O-DU for PRACH signaling.
          In this case, O-RU is not expected to perform any PRACH specific processing. */
  fh_config->max_sections_per_slot = 0; // not used in xran
  fh_config->max_sections_per_symbol = 0; // not used in xran

  fh_config->RunSlotPrbMapBySymbolEnable = *gpd(fhp, nfh, ORAN_CONFIG_CP_MULTISECTION)->uptr; // enable PRB mapping by symbol with multisection

  fh_config->dssEnable = 0; // enable DSS (extension-9)
  fh_config->dssPeriod = 0; // DSS pattern period for LTE/NR
  // fh_config->technology[XRAN_MAX_DSS_PERIODICITY] // technology array represents slot is LTE(0)/NR(1); used only if DSS enabled
  if (!set_activeMUs(&activeMUs, oai0->nr_scs_for_raster))
    return false;
  fh_config->activeMUs = &activeMUs;

  return true;
}

bool get_xran_config(void *mplane_api, const struct openair0_config *openair0_cfg, struct xran_fh_init *fh_init, struct xran_fh_config *fh_config)
{
  /* This xran integration release is only valid for O-RU CAT A.
    Therefore, each FH parameter is hardcoded to CAT A.
    If you are interested in CAT B, please be aware that parameters of fh_init and fh_config structs must be modified accordingly. */
  enum xran_category xran_cat = XRAN_CATEGORY_A;

  if (!set_fh_init(mplane_api, fh_init, xran_cat)) {
    printf("could not read FHI 7.2/ORAN config\n");
    return false;
  }

#ifdef OAI_MPLANE
  ru_session_list_t *ru_session_list = (ru_session_list_t *)mplane_api;
  for (int32_t o_xu_id = 0; o_xu_id < fh_init->xran_ports; o_xu_id++) {
    xran_mplane_t *xran_mplane = &ru_session_list->ru_session[o_xu_id].xran_mplane;
    if (!set_fh_config(xran_mplane, o_xu_id, fh_init->xran_ports, xran_cat, openair0_cfg, &fh_config[o_xu_id])) {
      MP_LOG_I("could not read FHI 7.2/RU-specific config\n");
      return false;
    }
  }
#else
  for (int32_t o_xu_id = 0; o_xu_id < fh_init->xran_ports; o_xu_id++) {
    if (!set_fh_config(NULL, o_xu_id, fh_init->xran_ports, xran_cat, openair0_cfg, &fh_config[o_xu_id])) {
      printf("could not read FHI 7.2/RU-specific config\n");
      return false;
    }
  }
#endif

  return true;
}
