/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Routines for UE PRACH physical channel
 */
#include "PHY/sse_intrin.h"
#include "PHY/impl_defs_nr.h"
#include "PHY/defs_nr_UE.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_proto_ue.h"
#include "PHY/MODULATION/nr_modulation.h"
#include "executables/nr-ue-fd.h"
#include "PHY/NR_TRANSPORT/nr_transport_common_proto.h"

#include "common/utils/LOG/log.h"

#include "T.h"

#include "openair1/PHY/NR_TRANSPORT/nr_prach.h"

#include "nr_prach_lut.h"

/* Preparation must not abort for a transform unavailable in this build. */
static bool prach_idft_supported(int dftlen)
{
#define PRACH_IDFT_CASE(size) case size:
  switch (dftlen) {
    FOREACH_IDFTSZ(PRACH_IDFT_CASE)
    return true;
    default:
      return false;
  }
#undef PRACH_IDFT_CASE
}

/* Fill the frequency bins used by the UE transform and by the external Low-PHY. */
static int prach_generate_bins(const nr_prach_waveform_key_t *key,
                               uint8_t preamble_index,
                               const c16_t roots[64][839],
                               c16_t *prachF)
{
  const int prach_sequence_length = key->sequence_length;
  const int N_ZC = prach_sequence_length ? 139 : 839;
  const int NCS = key->ncs;
  const int rootSequenceIndex = key->root_sequence_index;
  const int restricted_set = key->restricted_set;
  const int dftlen = key->dftlen;
  const int16_t tx_amp = key->amplitude;
  int k = key->frequency_start;
  if (preamble_index >= 64 || NCS < 0 || NCS >= N_ZC || k < 0 || k >= dftlen || restricted_set > 1 || (restricted_set && NCS == 0))
    return -1;
  // First compute physical root sequence
  /************************************************************************
  * 4G and NR NCS tables are slightly different and depend on prach format
  * Table 6.3.3.1-5:  for preamble formats with delta_f_RA = 1.25 Khz (formats 0,1,2)
  * Table 6.3.3.1-6:  for preamble formats with delta_f_RA = 5 Khz (formats 3)
  * NOTE: Restricted set type B is not implemented
  *************************************************************************/

  const uint16_t *prach_root_sequence_map =
      (prach_sequence_length == 0) ? prach_root_sequence_map_0_3 : prach_root_sequence_map_abc;
  int preamble_offset, preamble_shift = 0, first_nonzero_root_idx = 0;
  if (restricted_set == 0) {
    // This is the relative offset (for unrestricted case) in the root sequence table (5.7.2-4 from 36.211) for the given preamble index
    preamble_offset = NCS == 0 ? preamble_index : preamble_index / (N_ZC / NCS);
    // This is the \nu corresponding to the preamble index
    preamble_shift = NCS == 0 ? 0 : preamble_index % (N_ZC / NCS);
    preamble_shift *= NCS;
  } else { // This is the high-speed case

    uint16_t nr_du[NR_PRACH_SEQ_LEN_L];
    nr_fill_du(N_ZC, prach_root_sequence_map, nr_du);
    int preamble_index0 = preamble_index;
    // set preamble_offset to initial rootSequenceIndex and look if we need more root sequences for this
    // preamble index and find the corresponding cyclic shift
    preamble_offset = 0; // relative rootSequenceIndex;

    bool not_found = true;
    while (not_found) {
      // current root depending on rootSequenceIndex and preamble_offset
      int index = (rootSequenceIndex + preamble_offset) % N_ZC;
      uint16_t n_group_ra = 0;

      if (index >= N_ZC - 1 || preamble_offset >= N_ZC - 1)
        return -1;

      int u = prach_root_sequence_map[index];
      int n_shift_ra, d_start = INT16_MAX, n_shift_ra_bar;
      if ( (nr_du[u]<(N_ZC/3)) && (nr_du[u]>=NCS) ) {
        n_shift_ra     = nr_du[u]/NCS;
        d_start        = (nr_du[u]<<1) + (n_shift_ra * NCS);
        n_group_ra     = N_ZC/d_start;
        n_shift_ra_bar = max(0, (N_ZC - nr_du[u] * 2 - n_group_ra * d_start) / N_ZC);
      } else if (nr_du[u] >= N_ZC / 3 && nr_du[u] <= (N_ZC - NCS) / 2) {
        n_shift_ra = (N_ZC - nr_du[u] * 2) / NCS;
        d_start = N_ZC - nr_du[u] * 2 + n_shift_ra * NCS;
        n_group_ra     = nr_du[u]/d_start;
        n_shift_ra_bar = min(n_shift_ra, max(0, (nr_du[u] - n_group_ra * d_start) / NCS));
      } else {
        n_shift_ra     = 0;
        n_shift_ra_bar = 0;
      }

      // This is the number of cyclic shifts for the current root u
      int numshift = (n_shift_ra * n_group_ra) + n_shift_ra_bar;
      if (numshift>0 && preamble_index0==preamble_index)
        first_nonzero_root_idx = preamble_offset;

      if (preamble_index0 < numshift) {
        not_found = false;
        preamble_shift = (d_start * (preamble_index0/n_shift_ra)) + ((preamble_index0%n_shift_ra)*NCS);

      } else { // skip to next rootSequenceIndex and recompute parameters
        preamble_offset++;
        preamble_index0 -= numshift;
      }
    }
  }

  /********************************************************
   *
   * In function init_prach_tables:
   * to compute quantized roots of unity ru(n) = INT16_MAX * exp j*[ (2 * PI * n) / N_ZC ]
   *
   * In compute_prach_seq:
   * to calculate Xu = DFT xu = xu (inv_u*k) * Xu[0] (This is a Zadoff-Chou sequence property: DFT ZC sequence is another ZC
   * sequence)
   *
   * In generate_prach:
   * to do the cyclic-shifted DFT by multiplying Xu[k] * ru[k*preamble_shift] as:
   * If X[k] = DFT x(n) -> X_shifted[k] = DFT x(n+preamble_shift) = X[k] * exp -j*[ (2*PI*k*preamble_shift) / N_ZC ]
   *
   *********************************************************/

  if (preamble_offset - first_nonzero_root_idx >= key->num_root_sequences)
    return -1;
  const c16_t *Xu = roots[preamble_offset - first_nonzero_root_idx];

  memset(prachF, 0, dftlen * sizeof(*prachF));
  for (int offset = 0, offset2 = 0; offset < N_ZC; offset++, offset2 += preamble_shift) {
    if (offset2 >= N_ZC)
      offset2 -= N_ZC;
    const c16_t Xu_t = c16xmulConstShift(Xu[offset], tx_amp, 15);
    const double w = 2 * M_PI * (double)offset2 / N_ZC;
    const c16_t ru = {.r = (int16_t)(floor(INT16_MAX * cos(w))), .i = (int16_t)(floor(INT16_MAX * sin(w)))};
    const c16_t p = c16mulShift(Xu_t, ru, 15);
    prachF[k++] = p;
    if (k == dftlen)
      k = 0;
  }
  return 0;
}

int32_t nr_prach_generate_waveform(const nr_prach_waveform_key_t *key,
                                   uint8_t preamble_index,
                                   const c16_t roots[64][839],
                                   c16_t *prach)
{
  if (key->frequency_domain)
    return prach_generate_bins(key, preamble_index, roots, prach);
  if (!prach_idft_supported(key->dftlen))
    return -1;
  c16_t prachF[key->dftlen] __attribute__((aligned(32)));
  if (prach_generate_bins(key, preamble_index, roots, prachF) < 0)
    return -1;

  const idft_size_idx_t idft_size = get_idft(key->dftlen);
  idft(idft_size, (int16_t *)prachF, (int16_t *)prach, 1);
  return signal_energy((int *)prach, min(256, key->dftlen));
}

static void prach_dimensions(const NR_DL_FRAME_PARMS *fp,
                             int prach_sequence_length,
                             int mu,
                             int prach_fmt_id,
                             int prachStartSymbol,
                             int slot,
                             int *body_length,
                             int *prefix_length)
{
  // Ncp and dftlen here is given in terms of T_s wich is 30.72MHz sampling
  int dftlen, Ncp;
  if (prach_sequence_length == 0) {
    AssertFatal(prach_fmt_id >= 0 && prach_fmt_id <= 3, "Unknown PRACH format ID %d for sequence length 839\n", prach_fmt_id);
    const int ncp[4] = {3168, 21024, 4688, 3168};
    const int dft[4] = {24576, 24576, 24576, 6144};
    Ncp = ncp[prach_fmt_id];
    dftlen = dft[prach_fmt_id];
  } else {
    AssertFatal(prach_fmt_id >= 4 && prach_fmt_id <= 10, "Unknown PRACH format ID %d\n", prach_fmt_id);
    const int ncp[7] = {288, 576, 864, 216, 936, 1240, 2048};
    Ncp = ncp[prach_fmt_id - 4] >> mu;
    dftlen = 2048 >> mu;
  }

  // actually what we should be checking here is how often the current prach crosses a 0.5ms boundary. I am not quite sure for
  // which paramter set this would be the case, so I will ignore it for now and just check if the prach starts on a 0.5ms boundary
  if (fp->numerology_index == 0) {
    if (prachStartSymbol == 0 || prachStartSymbol == 7)
      Ncp += 16;
  } else {
    if (slot % (fp->slots_per_subframe / 2) == 0 && prachStartSymbol == 0)
      Ncp += 16;
  }

  switch (fp->samples_per_subframe) {
    case 7680:
      // 5 MHz @ 7.68 Ms/s
      Ncp >>= 2;
      dftlen >>= 2;
      break;

    case 15360:
      // 10, 15 MHz @ 15.36 Ms/s
      Ncp >>= 1;
      dftlen >>= 1;
      break;

    case 23040:
      // 20 MHz @ 23.04 Ms/s
      Ncp = (Ncp * 3) / 4;
      dftlen = (dftlen * 3) / 4;
      break;

    case 30720:
      // 20, 25, 30 MHz @ 30.72 Ms/s
      break;

    case 46080:
      // 40 MHz @ 46.08 Ms/s
      Ncp = (Ncp * 3) / 2;
      dftlen = (dftlen * 3) / 2;
      break;

    case 61440:
      // 40, 50, 60 MHz @ 61.44 Ms/s
      Ncp <<= 1;
      dftlen <<= 1;
      break;

    case 92160:
      // 50, 60, 70, 80, 90 MHz @ 92.16 Ms/s
      Ncp *= 3;
      dftlen *= 3;
      break;

    case 122880:
      // 70, 80, 90, 100 MHz @ 122.88 Ms/s
      Ncp <<= 2;
      dftlen <<= 2;
      break;

    case 184320:
      // 100 MHz @ 184.32 Ms/s
      Ncp = Ncp * 6;
      dftlen = dftlen * 6;
      break;

    case 245760:
      // 200 MHz @ 245.76 Ms/s
      Ncp <<= 3;
      dftlen <<= 3;
      break;

    default:
      AssertFatal(1 == 0, "sample rate %f MHz not supported for numerology %d\n", fp->samples_per_subframe / 1000.0, mu);
  }

  *body_length = dftlen;
  *prefix_length = Ncp;
}

static nr_prach_waveform_key_t prach_waveform_key(const NR_DL_FRAME_PARMS *fp,
                                                  const fapi_nr_config_request_t *config,
                                                  const fapi_nr_ul_config_prach_pdu *pdu,
                                                  int16_t amplitude,
                                                  int dftlen,
                                                  bool frequency_domain)
{
  const fapi_nr_prach_config_t *prach = &config->prach_config;
  const fapi_nr_num_prach_fd_occasions_t *fd = &prach->num_prach_fd_occasions_list[pdu->num_ra];
  int k = 12 * fd->k1 - 6 * fp->N_RB_UL;
  if (k < 0)
    k += fp->ofdm_symbol_size;
  k *= get_prach_K(prach->prach_sequence_length, pdu->prach_format, fp->numerology_index, prach->prach_sub_c_spacing);
  k += get_PRACH_k_bar(prach->prach_sub_c_spacing, fp->numerology_index);
  return (nr_prach_waveform_key_t){.sequence_length = prach->prach_sequence_length,
                                   .root_sequence_index = pdu->root_seq_id,
                                   .num_root_sequences = 64,
                                   .restricted_set = pdu->restricted_set,
                                   .ncs = pdu->num_cs,
                                   .dftlen = dftlen,
                                   .frequency_start = k,
                                   .amplitude = amplitude,
                                   .frequency_domain = frequency_domain};
}

nr_prach_lut_config_t nr_ue_prach_lut_config(const PHY_VARS_NR_UE *ue, bool frequency_domain)
{
  const NR_DL_FRAME_PARMS *fp = &ue->frame_parms;
  const fapi_nr_prach_config_t *prach = &ue->nrUE_config.prach_config;
  const nr_prach_preparation_t *preparation = &ue->prach_preparation;
  nr_prach_lut_config_t config = {0};
  if (fp->samples_per_subframe && fp->ofdm_symbol_size && preparation->num_formats <= 2 && prach->num_prach_fd_occasions <= 8
      && prach->num_prach_fd_occasions_list) {
    /* CFRA and PDCCH orders can use indices outside the contention-based subset. */
    config.num_preambles = 64;
    for (int f = 0; f < preparation->num_formats; f++) {
      const int format = preparation->formats[f];
      if (format > 10 || (format >= 4) != prach->prach_sequence_length)
        continue;
      int dftlen, ncp;
      prach_dimensions(fp, prach->prach_sequence_length, prach->prach_sub_c_spacing, format, 0, 0, &dftlen, &ncp);
      for (int fd = 0; fd < prach->num_prach_fd_occasions; fd++) {
        const fapi_nr_ul_config_prach_pdu pdu = {.num_ra = fd,
                                                 .root_seq_id = prach->num_prach_fd_occasions_list[fd].prach_root_sequence_index,
                                                 .prach_format = format,
                                                 .restricted_set = prach->restricted_set_config,
                                                 .num_cs = preparation->ncs[f]};
        const nr_prach_waveform_key_t key = prach_waveform_key(fp, &ue->nrUE_config, &pdu, AMP, dftlen, frequency_domain);
        bool duplicate = false;
        for (unsigned i = 0; i < config.num_keys; i++)
          duplicate |= nr_prach_waveform_key_equal(&config.keys[i], &key);
        if (!duplicate)
          config.keys[config.num_keys++] = key;
      }
    }
  }
  return config;
}

/* Standalone PHY callers can generate one preamble without preparing a table. */
static int32_t generate_uncached_prach(const nr_prach_waveform_key_t *key, uint8_t preamble, c16_t *out)
{
  c16_t roots[64][839] __attribute__((aligned(32)));
  const int nzc = key->sequence_length == 0 ? 839 : 139;
  int num_roots = key->num_root_sequences;
  if (!key->restricted_set && key->ncs >= 0 && key->ncs < nzc)
    num_roots = min(num_roots, key->ncs == 0 ? preamble + 1 : preamble / (nzc / key->ncs) + 1);
  compute_nr_prach_seq(key->sequence_length, num_roots, key->root_sequence_index, roots);
  c16_t row[key->dftlen] __attribute__((aligned(32)));
  const int32_t power = nr_prach_generate_waveform(key, preamble, roots, row);
  AssertFatal(power >= 0, "Unsupported PRACH waveform configuration\n");
  memcpy(out, row, key->dftlen * sizeof(*out));
  return power;
}

static int prach_start_sample(const NR_DL_FRAME_PARMS *fp, const fapi_nr_ul_config_prach_pdu *pdu, uint8_t slot)
{
  const int prachStartSymbol = pdu->prach_start_symbol;
  int prach_start;
  if (prachStartSymbol == 0) {
    prach_start = 0;
  } else if (fp->slots_per_subframe == 1) {
    if (prachStartSymbol <= 7)
      prach_start =
          (fp->ofdm_symbol_size + fp->nb_prefix_samples) * (prachStartSymbol - 1) + (fp->ofdm_symbol_size + fp->nb_prefix_samples0);
    else
      prach_start = (fp->ofdm_symbol_size + fp->nb_prefix_samples) * (prachStartSymbol - 2)
                    + (fp->ofdm_symbol_size + fp->nb_prefix_samples0) * 2;
  } else {
    if (slot % (fp->slots_per_subframe / 2) == 0)
      prach_start =
          (fp->ofdm_symbol_size + fp->nb_prefix_samples) * (prachStartSymbol - 1) + (fp->ofdm_symbol_size + fp->nb_prefix_samples0);
    else
      prach_start = (fp->ofdm_symbol_size + fp->nb_prefix_samples) * prachStartSymbol;
  }

  return prach_start;
}

/* Called by scheduled_response with mac->if_mutex held, before table replacement is possible. */
bool nr_ue_select_prach_waveform(PHY_VARS_NR_UE *ue, uint8_t gNB_id, uint8_t slot, int16_t amplitude)
{
  NR_UE_PRACH *prach = ue->prach_vars[gNB_id];
  const fapi_nr_ul_config_prach_pdu *pdu = &prach->prach_pdu;
  const fapi_nr_prach_config_t *config = &ue->nrUE_config.prach_config;
  prach->waveform_ready = false;
  if (pdu->num_ra >= config->num_prach_fd_occasions)
    return false;
  int dftlen, ncp;
  prach_dimensions(&ue->frame_parms,
                   config->prach_sequence_length,
                   config->prach_sub_c_spacing,
                   pdu->prach_format,
                   pdu->prach_start_symbol,
                   slot,
                   &dftlen,
                   &ncp);
  const nr_prach_waveform_key_t key =
      prach_waveform_key(&ue->frame_parms, &ue->nrUE_config, pdu, amplitude, dftlen, ue->fd_ifdevice != NULL);
  if (!nr_prach_lut_copy(ue->prach_lut,
                         &key,
                         pdu->ra_PreambleIndex,
                         prach->waveform,
                         sizeofArray(prach->waveform),
                         &prach->waveform_power))
    return false;
  prach->waveform_length = dftlen;
  prach->prefix_length = ncp;
  prach->waveform_start = prach_start_sample(&ue->frame_parms, pdu, slot);
  prach->waveform_ready = true;
  return true;
}

int32_t generate_nr_prach(PHY_VARS_NR_UE *ue, uint8_t gNB_id, int frame, uint8_t slot, int16_t tx_amp, c16_t **txData)
{
  NR_UE_PRACH *prach = ue->prach_vars[gNB_id];
  const fapi_nr_ul_config_prach_pdu *pdu = &prach->prach_pdu;
  const NR_DL_FRAME_PARMS *fp = &ue->frame_parms;
  nr_ue_fd_ifdevice_t *fd_ifdevice = ue->fd_ifdevice;
  LOG_I(PHY, "PRACH [UE %d] in frame.slot %d.%d, preambleIndex = %d\n", ue->Mod_id, frame, slot, pdu->ra_PreambleIndex);
  DevAssert(pdu->prach_format < sizeofArray(prach_repetitions));
  const int repetitions = prach_repetitions[pdu->prach_format];
  int dftlen = prach->waveform_length;
  int ncp = prach->prefix_length;
  int start = prach->waveform_start;
  int32_t power = prach->waveform_power;
  if (!prach->waveform_ready) {
    const fapi_nr_prach_config_t *config = &ue->nrUE_config.prach_config;
    prach_dimensions(fp,
                     config->prach_sequence_length,
                     config->prach_sub_c_spacing,
                     pdu->prach_format,
                     pdu->prach_start_symbol,
                     slot,
                     &dftlen,
                     &ncp);
    start = prach_start_sample(fp, pdu, slot);
    const nr_prach_waveform_key_t key = prach_waveform_key(fp, &ue->nrUE_config, pdu, tx_amp, dftlen, fd_ifdevice != NULL);
    DevAssert(dftlen <= sizeofArray(prach->waveform));
    power = generate_uncached_prach(&key, pdu->ra_PreambleIndex, prach->waveform);
  }
  if (fd_ifdevice) {
    fd_ifdevice->write_prach(fd_ifdevice, frame, slot, prach->waveform, dftlen, ncp, start, repetitions);
    return 0;
  }

  // Clear every antenna and slot occupied by PRACH before placing the preamble on antenna 0.
  const int prach_slots = pdu->prach_format < 4 ? get_long_prach_dur(pdu->prach_format, fp->numerology_index) : 1;
  int slots_len = 0;
  for (int s = 0; s < prach_slots; s++)
    slots_len += get_samples_per_slot((slot + s) % fp->slots_per_frame, fp);
  for (int aa = 0; aa < fp->nb_antennas_tx; aa++)
    memset(txData[aa], 0, slots_len * sizeof(*txData[aa]));
  nr_prach_copy_body(prach->waveform, dftlen, ncp, repetitions, txData[0] + start);
  return power;
}
