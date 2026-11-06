/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * UE anchor service: PSS/SSS search (the band's GSCNs or one SSB position) plus PBCH decode
 * on captures of the device's RX ring, retried on fresh captures until success,
 * timeout or cancellation. The anchor numbers frames so that abs_frame % 1024 is
 * the SFN carried by the MIB.
 */

#include <string.h>
#include <time.h>
#include "split7_device.h"
#include "split7_device_internal.h"
#include "PHY/defs_nr_UE.h"
#include "PHY/NR_REFSIG/pss_nr.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_proto_ue.h"
#include "common/utils/LOG/log.h"
#include "common/utils/nr/nr_common.h"

typedef struct {
  int nr_band;
  int ssb_first_sc;
} cell_search_ctx_t;

static uint64_t elapsed_ms(const struct timespec *start)
{
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (now.tv_sec - start->tv_sec) * 1000 + (now.tv_nsec - start->tv_nsec) / 1000000;
}

// SFN of the frame carrying the SSB: 6 MSBs from the MIB (after the BCCH-BCH
// message-type bit), 4 LSBs from the PBCH payload's extra bits, bit-reversed
// exactly as nr_ue_decode_mib() reads them.
static int mib_sfn(const fapiPbch_t *pbch)
{
  const int msb = (pbch->decoded_output[0] >> 1) & 0x3f;
  int lsb = 0;
  for (int i = 0; i < 4; i++)
    lsb |= ((pbch->xtra_byte >> i) & 1) << (3 - i);
  return (msb << 4) | lsb;
}

static int cell_search(split7_device_t *dev, void *ctx_v, int expected_pci, uint32_t timeout_ms)
{
  cell_search_ctx_t *ctx = ctx_v;
  const NR_DL_FRAME_PARMS *fp = split7_device_fp(dev);
  const int fft_size = fp->ofdm_symbol_size;
  const int mu = fp->numerology_index;
  const int num_rx = fp->nb_antennas_rx;
  // 20 ms (SSB period, TS 38.213 §4.1) + 1 slot: the capture starts at an arbitrary phase.
  const int capture_samples = 2 * fp->samples_per_frame + get_samples_per_slot(0, fp);
  // pss_search_time_nr() correlates up to fft_size samples past rxdata_size.
  const int alloc_samples = capture_samples + fft_size;

  c16_t *rx[SPLIT7_MAX_ANT];
  for (int a = 0; a < num_rx; a++)
    rx[a] = malloc16_clear(alloc_samples * sizeof(c16_t));
  c16_t *rxdataF_buf = malloc16_clear(4 * num_rx * fft_size * sizeof(c16_t));
  c16_t *pssTime_buf = malloc16_clear(3 * fft_size * sizeof(c16_t));
  AssertFatal(rxdataF_buf && pssTime_buf, "OOM\n");

  nr_gscn_info_t gscn_list[MAX_GSCN_BAND];
  int num_gscn = 0;
  if (ctx->nr_band > 0)
    num_gscn = get_scan_ssb_first_sc((double)fp->dl_CarrierFreq, fp->N_RB_DL, ctx->nr_band, mu, gscn_list);
  if (num_gscn <= 0) {
    gscn_list[0] = (nr_gscn_info_t){.ssbFirstSC = ctx->ssb_first_sc};
    num_gscn = 1;
  }

  nr_ssb_search_params_t sp = {
      .dl_CarrierFreq = fp->dl_CarrierFreq,
      .sampling_rate = fp->samples_per_subframe * 1000,
      .slots_per_frame = fp->slots_per_frame,
      .slots_per_subframe = fp->slots_per_subframe,
      .numerology_index = mu,
      .ofdm_symbol_size = fft_size,
      .ofdm_offset_divisor = fp->ofdm_offset_divisor,
      .nb_antennas_rx = num_rx,
      .symbols_per_slot = fp->symbols_per_slot,
      .N_RB_DL = fp->N_RB_DL,
      .nb_prefix_samples = fp->nb_prefix_samples,
      .nb_prefix_samples0 = fp->nb_prefix_samples0,
      .subcarrier_spacing = fp->subcarrier_spacing,
      .samples_per_slot_wCP = fp->samples_per_slot_wCP,
      .target_nid_cell = expected_pci,
      .rxdataF = rxdataF_buf,
      .pssTime = pssTime_buf,
      .rxdata = rx,
      .rxdata_size = capture_samples,
  };
  c16_t(*pssTime)[fft_size] = (c16_t(*)[fft_size])pssTime_buf;
  c16_t(*rxdataF_4sym)[num_rx][fft_size] = (c16_t(*)[num_rx][fft_size])rxdataF_buf;

  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC, &start);
  int rc = -1;
  while (!split7_device_service_cancelled(dev)) {
    if (timeout_ms > 0 && elapsed_ms(&start) >= timeout_ms) {
      LOG_W(PHY, "[split7] cell search timed out after %u ms\n", timeout_ms);
      break;
    }
    const int64_t live = split7_device_rx_time(dev);
    const int64_t capture_ts = live - alloc_samples;
    bool captured = capture_ts >= 0;
    for (int a = 0; a < num_rx && captured; a++)
      captured = split7_device_rx_copy(dev, a, capture_ts, alloc_samples, rx[a]);
    if (!captured) {
      usleep(1000);
      continue;
    }

    fapiPbch_t pbch = {0};
    int half_frame_bit = 0, ssb_index = 0, symbol_offset = 0, ssb_start_subcarrier = 0;
    bool found = false;
    for (int g = 0; g < num_gscn && !found; g++) {
      ssb_start_subcarrier = gscn_list[g].ssbFirstSC;
      for (int nid2 = 0; nid2 < 3; nid2++)
        generate_pss_nr_time(fft_size, fp->first_carrier_offset, nid2, ssb_start_subcarrier, pssTime[nid2]);
      memset(rxdataF_buf, 0, 4 * num_rx * fft_size * sizeof(c16_t));
      sp.ssb_start_subcarrier = ssb_start_subcarrier;
      found = nr_search_ssb_common(&sp);
      LOG_I(PHY,
            "[split7] SSB search (GSCN %d ssbFirstSC %d): found %d pss %d nid2 %d pos %d\n",
            gscn_list[g].gscn,
            ssb_start_subcarrier,
            found,
            sp.pss_res.success,
            sp.pss_res.nid2,
            sp.pss_res.pos);
      // PSS/SSS alone gives no SFN: require the MIB, as nr_scan_ssb() does.
      if (found) {
        const UE_nr_rxtx_proc_t proc = {0};
        found = nr_pbch_detection(&proc,
                                  fp,
                                  sp.sss_res.nid_cell,
                                  1, // symbol 0 is the PSS
                                  ssb_start_subcarrier,
                                  &half_frame_bit,
                                  &ssb_index,
                                  &symbol_offset,
                                  &pbch,
                                  rxdataF_4sym);
      }
    }
    if (!found)
      continue;

    // pss_res.pos is the correlation peak, one CP past the start of the PSS symbol.
    const int64_t pss_ts = capture_ts + sp.pss_res.pos - sp.nb_prefix_samples;
    // PSS symbol position inside its frame (38.211 §5.3.1: one long CP per half subframe).
    const int n_symb_prefix0 = symbol_offset / (7 * (1 << mu)) + 1;
    const int64_t pss_in_frame = (int64_t)n_symb_prefix0 * (fft_size + fp->nb_prefix_samples0)
                                 + (int64_t)(symbol_offset - n_symb_prefix0) * (fft_size + fp->nb_prefix_samples);
    const int64_t frame_ts = pss_ts - pss_in_frame;
    const int sfn = mib_sfn(&pbch);

    split7_cell_t cell = {
        .pci = sp.sss_res.nid_cell,
        .ssb_start_subcarrier = ssb_start_subcarrier,
        .ssb_index = ssb_index,
        .mib_xtra = pbch.xtra_byte,
        .half_frame_bit = half_frame_bit,
        .symbol_offset = symbol_offset,
        .mib_abs_frame = sfn,
    };
    memcpy(cell.mib, pbch.decoded_output, sizeof(cell.mib));
    LOG_A(PHY,
          "[split7] cell found: PCI %d, SSB index %d, SFN %d at sample %ld (search took %lu ms)\n",
          cell.pci,
          ssb_index,
          sfn,
          frame_ts,
          elapsed_ms(&start));
    split7_device_set_anchor(dev, frame_ts, sfn, &cell);
    rc = 0;
    break;
  }

  for (int a = 0; a < num_rx; a++)
    free(rx[a]);
  free(rxdataF_buf);
  free(pssTime_buf);
  return rc;
}

void split7_device_enable_cell_search(split7_device_t *dev, int nr_band, int ssb_first_sc)
{
  static cell_search_ctx_t ctx;
  ctx.nr_band = nr_band;
  ctx.ssb_first_sc = ssb_first_sc;
  split7_device_set_anchor_service(dev, cell_search, &ctx);
}
