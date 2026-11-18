/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "PHY/defs_gNB.h"
#include "PHY/defs_nr_common.h"
#include "modulation_UE.h"
#include "nr_modulation.h"
#include "executables/nr-ue-fd.h"
#include "PHY/NR_UE_ESTIMATION/nr_estimation.h"
#include "PHY/nr_phy_common/inc/nr_phy_common.h"
#include <common/utils/LOG/log.h>
#include <stdatomic.h>
#include <sched.h>

/* rxdataF should be 16 bytes aligned */
void nr_symbol_fep(const NR_DL_FRAME_PARMS *frame_parms,
                   const int slot,
                   const unsigned char symbol,
                   const int link_type,
                   c16_t *rxdata[frame_parms->nb_antennas_rx],
                   c16_t *rxdataF[frame_parms->nb_antennas_rx],
                   time_stats_t* dft_stats)
{
  AssertFatal(symbol < frame_parms->symbols_per_slot,
              "slot_fep: symbol must be between 0 and %d\n",
              frame_parms->symbols_per_slot - 1);
  AssertFatal(slot < frame_parms->slots_per_frame, "slot_fep: Ns must be between 0 and %d\n", frame_parms->slots_per_frame - 1);

  dft_size_idx_t dftsize = get_dft(frame_parms->ofdm_symbol_size);
  const bool is_sl = (link_type == link_type_sl);
  const int nb_rb = is_sl ? frame_parms->N_RB_SL : frame_parms->N_RB_DL;
  for (unsigned char aa = 0; aa < frame_parms->nb_antennas_rx; aa++) {
    // OFDM Demod
    if (dft_stats) start_meas(dft_stats);
    dft(dftsize, (int16_t *)rxdata[aa], (int16_t *)rxdataF[aa], 1);
    if (dft_stats) stop_meas(dft_stats);

    // FFT-shift
    fftshift_inplace(rxdataF[aa], nb_rb * NR_NB_SC_PER_RB, frame_parms->ofdm_symbol_size);

    // Phase compensation
    apply_nr_rotation_symbol_fftshifted_RX(
        frame_parms->symbols_per_slot,
        frame_parms->slots_per_subframe,
        frame_parms->timeshift_rotation_shared ? frame_parms->timeshift_rotation_shared : frame_parms->timeshift_symbol_rotation,
        rxdataF[aa],
        frame_parms->symbol_rotation[link_type],
        nb_rb,
        slot,
        symbol);
  }
}

/* offset of the samples of an OFDM symbol (after 1/8th of its cyclic prefix, to avoid ISI) from the start of its slot */
static unsigned int symbol_offset_in_slot(const PHY_VARS_NR_UE *ue,
                                          const NR_DL_FRAME_PARMS *frame_parms,
                                          unsigned int slot,
                                          unsigned int symbol,
                                          enum nr_Link linktype)
{
  bool is_sl = (linktype == link_type_sl);
  bool is_synchronized = (ue) ? ue->is_synchronized : false;
  unsigned int nb_prefix_samples = frame_parms->nb_prefix_samples;
  unsigned int nb_prefix_samples0 = (is_synchronized || is_sl) ? frame_parms->nb_prefix_samples0 : nb_prefix_samples;

  unsigned int offset = 0;
  const unsigned int abs_symbol = slot * frame_parms->symbols_per_slot + symbol;
  for (int idx_symb = slot * frame_parms->symbols_per_slot; idx_symb <= abs_symbol; idx_symb++)
    offset += (idx_symb % (0x7 << frame_parms->numerology_index)) ? nb_prefix_samples : nb_prefix_samples0;
  offset += frame_parms->ofdm_symbol_size * symbol;
  // use OFDM symbol from within 1/8th of the CP to avoid ISI
  return offset - nb_prefix_samples / frame_parms->ofdm_offset_divisor;
}

/* frequency offset compensation (rx_offset: index of the first sample in the frame, for the phase) and OFDM demodulation
   of one symbol whose samples start at rxdata_symb_ptr[] */
static void symbol_fo_fep(PHY_VARS_NR_UE *ue,
                          const NR_DL_FRAME_PARMS *frame_parms,
                          unsigned int slot,
                          unsigned int symbol,
                          c16_t rxdataF[][frame_parms->samples_per_slot_wCP],
                          enum nr_Link linktype,
                          unsigned int rx_offset,
                          c16_t *rxdata_symb_ptr[frame_parms->nb_antennas_rx],
                          c16_t tmp_dft_in[][frame_parms->ofdm_symbol_size])
{
  c16_t *rxdataF_symb_ptr[frame_parms->nb_antennas_rx];
  for (unsigned char aa = 0; aa < frame_parms->nb_antennas_rx; aa++) {
    rxdataF_symb_ptr[aa] = &rxdataF[aa][frame_parms->ofdm_symbol_size * symbol];
    if (ue && ue->cont_fo_comp) {
      start_meas_nr_ue_phy(ue, RX_FO_COMPENSATION_STATS);
      nr_fo_compensation(ue->dl_Doppler_shift + ue->freq_offset,
                         frame_parms->samples_per_subframe,
                         rx_offset,
                         rxdata_symb_ptr[aa],
                         tmp_dft_in[aa],
                         frame_parms->ofdm_symbol_size);
      stop_meas_nr_ue_phy(ue, RX_FO_COMPENSATION_STATS);
      rxdata_symb_ptr[aa] = tmp_dft_in[aa];
    }
  }
  time_stats_t *dft_stats = NULL;
  if (ue)
    dft_stats = &ue->phy_cpu_stats.cpu_time_stats[RX_DFT_STATS];
  nr_symbol_fep(frame_parms, slot, symbol, linktype, rxdata_symb_ptr, rxdataF_symb_ptr, dft_stats);
}

int nr_slot_fep(PHY_VARS_NR_UE *ue,
                const NR_DL_FRAME_PARMS *frame_parms,
                unsigned int frame,
                unsigned int slot,
                unsigned int symbol,
                c16_t rxdataF[][frame_parms->samples_per_slot_wCP],
                enum nr_Link linktype,
                uint32_t sample_offset,
                c16_t **rxdata)
{
  AssertFatal(symbol < frame_parms->symbols_per_slot,
              "slot_fep: symbol must be between 0 and %d\n",
              frame_parms->symbols_per_slot - 1);
  AssertFatal(slot < frame_parms->slots_per_frame, "slot_fep: Ns must be between 0 and %d\n", frame_parms->slots_per_frame - 1);

  // FD-UE: the Low-PHY demodulated the symbol already.
  if (ue && ue->fd_ifdevice) {
    c16_t *dst[frame_parms->nb_antennas_rx];
    for (int aa = 0; aa < frame_parms->nb_antennas_rx; aa++)
      dst[aa] = &rxdataF[aa][frame_parms->ofdm_symbol_size * symbol];
    if (!ue->fd_ifdevice->read_symbol(ue->fd_ifdevice, frame, slot, symbol, dst)) {
      for (int aa = 0; aa < frame_parms->nb_antennas_rx; aa++)
        memset(dst[aa], 0, frame_parms->ofdm_symbol_size * sizeof(c16_t));
      return -1;
    }
    return 0;
  }

  bool is_sl = (linktype == link_type_sl);
  // For Sidelink 16 frames worth of samples is processed to find SSB, for 5G-NR 2.
  const unsigned int total_samples = (is_sl) ? 16 * frame_parms->samples_per_frame : 2 * frame_parms->samples_per_frame;

  const unsigned int rx_offset = get_samples_slot_timestamp(frame_parms, slot)
                                 + symbol_offset_in_slot(ue, frame_parms, slot, symbol, linktype) + sample_offset;

  LOG_D(PHY,
        "slot_fep: slot %d, symbol %d, rx_offset %u energy %d\n",
        slot,
        symbol,
        rx_offset,
        dB_fixed(signal_energy((int32_t *)&rxdata[0][rx_offset], frame_parms->ofdm_symbol_size)));

  c16_t tmp_dft_in[frame_parms->nb_antennas_rx][frame_parms->ofdm_symbol_size] __attribute__((aligned(32)));
  c16_t *rxdata_symb_ptr[frame_parms->nb_antennas_rx];
  for (unsigned char aa = 0; aa < frame_parms->nb_antennas_rx; aa++) {
    // This happens only during initial sync
    if (rx_offset + frame_parms->ofdm_symbol_size > total_samples) {
      // we have to wrap on the end
      memcpy(&tmp_dft_in[aa][0], &rxdata[aa][rx_offset], (total_samples - rx_offset) * sizeof(int32_t));
      memcpy(&tmp_dft_in[aa][total_samples - rx_offset],
             &rxdata[aa][0],
             (frame_parms->ofdm_symbol_size - (total_samples - rx_offset)) * sizeof(int32_t));
      rxdata_symb_ptr[aa] = tmp_dft_in[aa];
    } else {
      rxdata_symb_ptr[aa] = &rxdata[aa][rx_offset];
    }
  }
  symbol_fo_fep(ue, frame_parms, slot, symbol, rxdataF, linktype, rx_offset, rxdata_symb_ptr, tmp_dft_in);
  return 0;
}

/* results computed from the RU ring samples, the same for all the UEs of the RU: one UE computes an entry, the others
   copy it. An entry is identified by key (the ring index of its samples) and params (what else its computation depends
   on); seq is odd while the entry is written */
typedef struct {
  _Atomic uint32_t seq;
  _Atomic uint64_t key;
  uint64_t params[NR_UE_SHARED_PARAMS];
  c16_t *data;
} __attribute__((aligned(64))) nr_ue_shared_entry_t;

typedef struct {
  int nb; // entries
  int capacity; // c16_t per entry
  nr_ue_shared_entry_t *e;
} nr_ue_shared_table_t;

typedef struct nr_fep_cache_s {
  int nb_ant;
  int sz;
  nr_ue_shared_table_t t[NR_SHARED_TABLES];
} nr_fep_cache_t;

static void shared_table_init(nr_ue_shared_table_t *t, int nb, int capacity)
{
  t->nb = nb;
  t->capacity = capacity;
  t->e = malloc16_clear(nb * sizeof(*t->e));
  for (int i = 0; i < nb; i++) {
    t->e[i].data = malloc16(capacity * sizeof(c16_t));
    atomic_init(&t->e[i].seq, 0);
    atomic_init(&t->e[i].key, UINT64_MAX);
  }
}

nr_fep_cache_t *nr_fep_cache_create(int nb_antennas_rx, int ofdm_symbol_size)
{
  nr_fep_cache_t *c = malloc16_clear(sizeof(*c));
  c->nb_ant = nb_antennas_rx;
  c->sz = ofdm_symbol_size;
  // several slots of symbols: the UEs of an RU process the same slot within a few slots
  shared_table_init(&c->t[NR_SHARED_SYMB], 64, nb_antennas_rx * ofdm_symbol_size);
  // a few CORESETs per PDCCH symbol, the LLRs of one symbol are fewer than its subcarriers
  shared_table_init(&c->t[NR_SHARED_PDCCH_LLR], 256, ofdm_symbol_size);
  // polar decoding of the PDCCH candidates: input LLRs (up to aggregation level 16) and result
  shared_table_init(&c->t[NR_SHARED_POLAR], 512, 16 * 108 / 2 + 8);
  return c;
}

/* entry lookup: the data of the entry starts with match_len values equal to match (if any), then the result */
static int shared_get(nr_fep_cache_t *c,
                      int table,
                      unsigned int idx,
                      uint64_t key,
                      const uint64_t params[NR_UE_SHARED_PARAMS],
                      const c16_t *match,
                      int match_len,
                      int nparts,
                      int part_len,
                      c16_t *const dst[nparts])
{
  nr_ue_shared_table_t *t = c->t + table;
  if (match_len + nparts * part_len > t->capacity)
    return -1;
  nr_ue_shared_entry_t *e = t->e + idx % t->nb;
  for (int spin = 0;; spin++) {
    const uint32_t s1 = atomic_load_explicit(&e->seq, memory_order_acquire);
    if (atomic_load_explicit(&e->key, memory_order_acquire) != key)
      break;
    if (s1 & 1) { // being computed by another UE
      if (spin < 200)
        simde_mm_pause();
      else
        sched_yield();
      continue;
    }
    if (memcmp(e->params, params, sizeof(e->params)) || (match_len && memcmp(e->data, match, match_len * sizeof(c16_t))))
      break;
    for (int p = 0; p < nparts; p++)
      memcpy(dst[p], e->data + match_len + p * part_len, part_len * sizeof(c16_t));
    atomic_thread_fence(memory_order_acquire);
    if (atomic_load_explicit(&e->seq, memory_order_relaxed) == s1) // not overwritten during the copy
      return 1;
    break;
  }
  uint32_t s = atomic_load_explicit(&e->seq, memory_order_relaxed);
  if ((s & 1) || !atomic_compare_exchange_strong(&e->seq, &s, s + 1))
    return -1; // another UE writes this entry for another key
  atomic_store_explicit(&e->key, key, memory_order_release);
  memcpy(e->params, params, sizeof(e->params));
  if (match_len)
    memcpy(e->data, match, match_len * sizeof(c16_t));
  return 0;
}

int nr_ue_shared_get(nr_fep_cache_t *c,
                     int table,
                     unsigned int idx,
                     uint64_t key,
                     const uint64_t params[NR_UE_SHARED_PARAMS],
                     int nparts,
                     int part_len,
                     c16_t *const dst[nparts])
{
  return shared_get(c, table, idx, key, params, NULL, 0, nparts, part_len, dst);
}

int nr_ue_shared_get_match(nr_fep_cache_t *c,
                           int table,
                           uint64_t key,
                           const uint64_t params[NR_UE_SHARED_PARAMS],
                           const c16_t *match,
                           int match_len,
                           int len,
                           c16_t *dst)
{
  c16_t *d[1] = {dst};
  return shared_get(c, table, key, key, params, match, match_len, 1, len, d);
}

void nr_ue_shared_put_match(nr_fep_cache_t *c, int table, uint64_t key, int match_len, int len, const c16_t *src)
{
  nr_ue_shared_entry_t *e = c->t[table].e + key % c->t[table].nb;
  memcpy(e->data + match_len, src, len * sizeof(c16_t));
  atomic_store_explicit(&e->seq, atomic_load_explicit(&e->seq, memory_order_relaxed) + 1, memory_order_release);
}

void nr_ue_shared_put(nr_fep_cache_t *c, int table, unsigned int idx, int nparts, int part_len, c16_t *const src[nparts])
{
  nr_ue_shared_entry_t *e = c->t[table].e + idx % c->t[table].nb;
  for (int p = 0; p < nparts; p++)
    memcpy(e->data + p * part_len, src[p], part_len * sizeof(c16_t));
  atomic_store_explicit(&e->seq, atomic_load_explicit(&e->seq, memory_order_relaxed) + 1, memory_order_release);
}

int nr_slot_fep_rx(PHY_VARS_NR_UE *ue,
                   const NR_DL_FRAME_PARMS *frame_parms,
                   unsigned int frame,
                   unsigned int slot,
                   unsigned int symbol,
                   c16_t rxdataF[][frame_parms->samples_per_slot_wCP])
{
  const NR_UE_COMMON *common_vars = &ue->common_vars;
  if (common_vars->rx_slot_len == 0 || ue->fd_ifdevice)
    return nr_slot_fep(ue, frame_parms, frame, slot, symbol, rxdataF, link_type_dl, 0, common_vars->rxdata);

  AssertFatal(symbol < frame_parms->symbols_per_slot,
              "slot_fep: symbol must be between 0 and %d\n",
              frame_parms->symbols_per_slot - 1);
  const unsigned int offset = symbol_offset_in_slot(ue, frame_parms, slot, symbol, link_type_dl);
  AssertFatal(offset + frame_parms->ofdm_symbol_size <= common_vars->rx_slot_len,
              "symbol %u of slot %u beyond the %d samples of the slot\n",
              symbol,
              slot,
              common_vars->rx_slot_len);
  c16_t tmp_dft_in[frame_parms->nb_antennas_rx][frame_parms->ofdm_symbol_size] __attribute__((aligned(32)));
  c16_t *rxdata_symb_ptr[frame_parms->nb_antennas_rx];
  for (int aa = 0; aa < frame_parms->nb_antennas_rx; aa++)
    rxdata_symb_ptr[aa] = common_vars->rx_slot[aa] + offset;
  const unsigned int rx_offset = get_samples_slot_timestamp(frame_parms, slot) + offset;

  // the same samples give the same symbol to all the UEs of the RU (no UE specific frequency offset compensation)
  nr_fep_cache_t *c = common_vars->fep_cache;
  if (c && !ue->cont_fo_comp && c->nb_ant == frame_parms->nb_antennas_rx && c->sz == frame_parms->ofdm_symbol_size) {
    const uint64_t key = ((common_vars->rx_slot_ring_ts + offset) << 4) | symbol;
    const uint64_t params[NR_UE_SHARED_PARAMS] = {slot, frame_parms->N_RB_DL, frame_parms->dl_CarrierFreq, 0};
    const unsigned int idx = (frame * frame_parms->slots_per_frame + slot) * NR_SYMBOLS_PER_SLOT + symbol;
    c16_t *dst[c->nb_ant];
    for (int aa = 0; aa < c->nb_ant; aa++)
      dst[aa] = &rxdataF[aa][frame_parms->ofdm_symbol_size * symbol];
    const int got = nr_ue_shared_get(c, NR_SHARED_SYMB, idx, key, params, c->nb_ant, c->sz, dst);
    if (got == 1)
      return 0;
    if (got == 0) {
      nr_symbol_fep(frame_parms, slot, symbol, link_type_dl, rxdata_symb_ptr, dst, &ue->phy_cpu_stats.cpu_time_stats[RX_DFT_STATS]);
      nr_ue_shared_put(c, NR_SHARED_SYMB, idx, c->nb_ant, c->sz, dst);
      return 0;
    }
  }
  symbol_fo_fep(ue, frame_parms, slot, symbol, rxdataF, link_type_dl, rx_offset, rxdata_symb_ptr, tmp_dft_in);
  return 0;
}

int nr_symbol_fep_ul(const NR_DL_FRAME_PARMS *fp,
                     const c16_t *rxdata,
                     c16_t *rxdataF,
                     unsigned char symbol,
                     unsigned char slot,
                     int sample_offset)
{
  dft_size_idx_t dftsize = get_dft(fp->ofdm_symbol_size);
  // This is for misalignment issues
  int32_t tmp_dft_in[fp->ofdm_symbol_size] __attribute__((aligned(32)));

  // offset of first OFDM symbol
  uint32_t prefix_length = get_samples_symbol_duration(fp, slot, symbol, 1) - fp->ofdm_symbol_size;
  unsigned int rxdata_offset =
      get_samples_slot_timestamp(fp, slot) + get_samples_symbol_timestamp(fp, slot, symbol) + prefix_length;
  // use OFDM symbol from within 1/8th of the CP to avoid ISI
  rxdata_offset -= (fp->nb_prefix_samples / fp->ofdm_offset_divisor);

  int16_t *rxdata_ptr;
  if (rxdata_offset >= sample_offset)
    rxdata_offset -= sample_offset;
  else
    rxdata_offset += fp->samples_per_frame - sample_offset;

  if (rxdata_offset + fp->ofdm_symbol_size > fp->samples_per_frame) {
    memcpy(&tmp_dft_in[0],
           &rxdata[rxdata_offset],
           (fp->samples_per_frame - rxdata_offset) * sizeof(int32_t));
    memcpy(&tmp_dft_in[fp->samples_per_frame - rxdata_offset],
           &rxdata[0],
           (fp->ofdm_symbol_size - fp->samples_per_frame + rxdata_offset) * sizeof(int32_t));
    rxdata_ptr = (int16_t *)tmp_dft_in;
  } else {
    // use dft input from RX buffer directly
    rxdata_ptr = (int16_t *)&rxdata[rxdata_offset];
  }

  dft(dftsize, rxdata_ptr, (int16_t *)rxdataF, 1);

  return 0;
}

void apply_nr_rotation_symbol_fftshifted_RX(const int symbols_per_slot,
                                            const int slots_per_subframe,
                                            const c16_t *shift_rot,
                                            c16_t *rxdataF,
                                            const c16_t *rot,
                                            const int nb_rb,
                                            const int slot,
                                            const int symbol)
{
  const int symb_offset = (slot % slots_per_subframe) * symbols_per_slot;
  c16_t rot2 = rot[symbol + symb_offset];
  rot2.i = -rot2.i;
  rotate_cpx_vector(rxdataF, rot2, rxdataF, nb_rb * NR_NB_SC_PER_RB, 15);
  mult_cpx_vector(rxdataF, shift_rot, rxdataF, nb_rb * NR_NB_SC_PER_RB, 15);
}

void nr_ofdm_demod_and_rx_rotation(c16_t **rxdata,
                                   c16_t **rxdataF,
                                   const NR_DL_FRAME_PARMS *fp,
                                   int nb_antennas,
                                   int slot,
                                   int slot_offsetF,
                                   enum nr_Link linktype,
                                   bool was_symbol_used[NR_SYMBOLS_PER_SLOT])
{
  for (int aa = 0; aa < nb_antennas; aa++) {
    for (uint8_t symbol = 0; symbol < fp->symbols_per_slot; symbol++) {
      if (was_symbol_used[symbol] == true) {
        // OFDM Demod
        nr_symbol_fep_ul(fp, &rxdata[aa][0], &rxdataF[aa][slot_offsetF + symbol * fp->ofdm_symbol_size], symbol, slot, 0);
        // FFT-shift
        fftshift_inplace(&rxdataF[aa][slot_offsetF + symbol * fp->ofdm_symbol_size],
                         fp->N_RB_UL * NR_NB_SC_PER_RB,
                         fp->ofdm_symbol_size);
        // Phase compensation
        apply_nr_rotation_symbol_fftshifted_RX(fp->symbols_per_slot,
                                               fp->slots_per_subframe,
                                               fp->timeshift_symbol_rotation,
                                               &rxdataF[aa][slot_offsetF + symbol * fp->ofdm_symbol_size],
                                               fp->symbol_rotation[linktype],
                                               fp->N_RB_UL,
                                               slot,
                                               symbol);
      }
    }
  }
}
