/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "du_fhi_prach.h"
#include "common/utils/nr/nr_common.h"
#include "common/utils/fsn.h"
#include "openair1/PHY/NR_TRANSPORT/nr_transport_proto.h"

// Declared in openair2/LAYER2/NR_MAC_COMMON/nr_mac_common.h, whose own include chain pulls in
// ASN.1-generated headers (NR_MIB.h et al.) not on this MODULE's include path. Only the L2
// static library provides that path; a MODULE library instead resolves this symbol at dlopen
// time against the host gNB executable (same pattern radio/fhi_72/oran-init.c relies on for
// its own OAI-internal calls), so pulling the full header just for this prototype isn't needed.
bool get_nr_prach_sched_from_info(nr_prach_info_t info,
                                  int config_index,
                                  int frame,
                                  int slot,
                                  int mu,
                                  frequency_range_t freq_range,
                                  uint16_t *RA_sfn_index,
                                  uint8_t unpaired);

#define DU_FHI_MAX_ANTENNAS 4
#define PRACH_FILTER_INDEX_012 1
#define PRACH_FILTER_INDEX_3 2
#define PRACH_FILTER_INDEX_ABC 3

static bool is_long_format(uint32_t format)
{
  return (format & 0xff) < 4;
}

// N_CP^RA in units of kappa (TS 38.211 Tables 6.3.3.1-1 and 6.3.3.1-2, short formats before 2^-mu scaling)
static int prach_ncp(uint32_t format)
{
  switch (format & 0xff) {
    case 0: return 3168;
    case 1: return 21024;
    case 2: return 4688;
    case 3: return 3168;
    case 0xa1: return 288;
    case 0xa2: return 576;
    case 0xa3: return 864;
    case 0xb1: return 216;
    case 0xb2: return 360;
    case 0xb3: return 504;
    case 0xb4: return 936;
    case 0xc0: return 1240;
    case 0xc2: return 2048;
    default: AssertFatal(false, "unknown PRACH format 0x%x\n", format);
  }
  return 0;
}

static int prach_num_prb(uint32_t format)
{
  return is_long_format(format) ? 72 : 12;
}

// Section type 3 fields, computed as the vendor xran library does (xran_main.c/xran_cp_proc.c), so both
// fronthaul implementations put the same PRACH C-Plane on the wire.
static du_tx_prach_section_t prach_section(const du_fh_config_t *cfg, const du_fhi_prach_config_t *prach_cfg, int slot)
{
  uint32_t format = prach_cfg->prach_info.format;
  bool long_format = is_long_format(format);
  int start_symbol = prach_cfg->prach_info.start_symbol;
  int slots_per_subframe = 1 << cfg->numerology;
  int slot_in_subframe = slot % slots_per_subframe;

  int time_offset = prach_ncp(format);
  if (start_symbol > 0)
    time_offset += start_symbol * (2048 + 144);
  if (!long_format) {
    time_offset >>= cfg->numerology;
    if (start_symbol > 0 && (slot_in_subframe == 0 || slot_in_subframe == slots_per_subframe >> 1))
      time_offset += 16;
  }

  // freqOffset in units of half PRACH subcarriers from the channel center (O-RAN CUS 7.5.3.11)
  double pusch_scs_khz = 15 << cfg->numerology;
  double prach_scs_khz = long_format ? ((format & 0xff) == 3 ? 5.0 : 1.25) : pusch_scs_khz;
  double ratio = pusch_scs_khz / prach_scs_khz;
  int freq_offset = (int)(-cfg->num_prbs * NR_NB_SC_PER_RB * ratio + prach_cfg->freq_start * NR_NB_SC_PER_RB * ratio * 2);

  int num_symbol = 1;
  if (!long_format) {
    if ((format & 0xff) == 0xc2)
      num_symbol = 4;
    else if ((format & 0xff) == 0xc0)
      num_symbol = 1;
    else
      num_symbol = prach_cfg->prach_info.N_dur;
  }

  int filter_index = PRACH_FILTER_INDEX_ABC;
  int scs = cfg->numerology;
  if (long_format) {
    filter_index = (format & 0xff) == 3 ? PRACH_FILTER_INDEX_3 : PRACH_FILTER_INDEX_012;
    scs = (format & 0xff) == 3 ? 14 : 12;
  }

  return (du_tx_prach_section_t){.filter_index = filter_index,
                                 .time_offset = time_offset,
                                 .fft_size = prach_cfg->fft_size,
                                 .scs = scs,
                                 .section_id = 0,
                                 .beam_id = 0,
                                 .num_symbol = num_symbol,
                                 .start_prb = 0,
                                 .num_prb = prach_num_prb(format),
                                 .freq_offset = freq_offset};
}

void du_fhi_advance_prach_lookahead(du_fhi_state_t *st,
                                    const du_fhi_prach_config_t *prach_cfg,
                                    uint64_t start_absolute_slot,
                                    uint64_t target_absolute_slot,
                                    int nb_rx)
{
  int slots_per_frame = 10 << st->cfg.numerology;
  int nb_rx_bounded = nb_rx > DU_FHI_MAX_ANTENNAS ? DU_FHI_MAX_ANTENNAS : nb_rx;

  for (uint64_t abs_slot = start_absolute_slot; abs_slot <= target_absolute_slot; abs_slot++) {
    uint64_t abs_frame_num = abs_slot / (uint64_t)slots_per_frame;
    int t_slot = (int)(abs_slot % (uint64_t)slots_per_frame);
    uint64_t t_hyper_frame = abs_frame_num / 1024ULL;
    int t_frame = (int)(abs_frame_num % 1024ULL);
    uint16_t ra_sfn_index = 0;
    bool is_prach = get_nr_prach_sched_from_info(prach_cfg->prach_info,
                                                 prach_cfg->prach_config_index,
                                                 t_frame,
                                                 t_slot,
                                                 st->cfg.numerology,
                                                 FR1,
                                                 &ra_sfn_index,
                                                 st->cfg.fdd_mode ? 0 : 1);
    if (!is_prach)
      continue;

    int start_symbol = (int)prach_cfg->prach_info.start_symbol;
    int num_symbols = (int)prach_cfg->prach_info.N_dur;
    du_tx_prach_section_t section = prach_section(&st->cfg, prach_cfg, t_slot);
    uint64_t start_absolute_symbol =
        (t_hyper_frame * 1024ULL + (uint64_t)t_frame) * (uint64_t)slots_per_frame * NR_SYMBOLS_PER_SLOT
        + (uint64_t)t_slot * NR_SYMBOLS_PER_SLOT + start_symbol;

    for (int ant = 0; ant < nb_rx_bounded; ant++) {
      du_fh_expect_prach_occasion(st->du_fh_handle,
                                  start_absolute_symbol,
                                  num_symbols,
                                  t_slot,
                                  ant,
                                  0,
                                  0,
                                  section.num_prb,
                                  st->cfg.comp_type,
                                  st->cfg.iq_width,
                                  st->cfg.prach_kbar);
      du_fh_schedule_prach(st->du_fh_handle, t_hyper_frame, t_frame, t_slot, start_symbol, st->cfg.prach_eaxc_offset + ant, &section);
    }
  }
}

void du_fhi_south_in_prach(void *du_fh_handle, const du_fhi_prach_config_t *prach_cfg, PHY_VARS_gNB *gNB, int frame, int slot, int numerology)
{
  if (!gNB)
    return;

  // PRACH jobs of this slot are ready once its UL window closed, i.e. before this slot's event.
  prach_item_t p;
  fsn_t now = {.f = (uint16_t)frame, .s = (uint16_t)slot, .mu = (uint8_t)numerology};
  bool have_prach = get_next_nr_prach(&gNB->prach_ru_queue, &now, &p);
  c16_t (*prach_buf)[NUMBER_OF_NR_RU_PRACH_OCCASIONS_MAX][NR_PRACH_SEQ_LEN_L] = have_prach ? (void *)p.prach_buf : NULL;

  while (du_fh_get_ready_prach_job_count(du_fh_handle) > 0) {
    int16_t iq[NR_PRACH_SEQ_LEN_L * 2];
    uint64_t prach_hf;
    int prach_f, prach_s, prach_ant, prach_section;
    du_fh_read_prach_iq(du_fh_handle, iq, &prach_hf, &prach_f, &prach_s, &prach_ant, &prach_section);
    if (!have_prach || prach_f != frame || prach_s != slot || prach_ant < p.ant_start || prach_ant >= p.ant_start + p.nb_rx)
      continue;
    // same layout as the vendor path: first occasion only, repetitions summed
    memcpy(prach_buf[prach_ant - p.ant_start][0], iq, NR_PRACH_SEQ_LEN_S * sizeof(c16_t));
  }

  if (have_prach) {
    bool success = spsc_q_put(&gNB->prach_l1rx_queue, &p, sizeof(p));
    // prach_l1rx_queue is emptied as fast as possible by rx_func(), see the vendor path
    DevAssert(success);
  }
}
