/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "split7_test_ref.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "PHY/defs_nr_UE.h"
#include "PHY/INIT/nr_parms.h"
#include "PHY/MODULATION/modulation_UE.h"
#include "PHY/MODULATION/nr_modulation.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_proto_ue.h"
#include "common/config/config_userapi.h"
#include "common/utils/LOG/log.h"
#include "executables/nr-uesoftmodem.h"
#include "executables/softmodem-common.h"

/* Stubs for what the PHY libraries reference but these tests never reach. */
softmodem_params_t *get_softmodem_params(void)
{
  static softmodem_params_t p;
  return &p;
}
nrUE_params_t nrUE_params;
nrUE_params_t *get_nrUE_params(void)
{
  return &nrUE_params;
}
configmodule_interface_t *uniqCfg = NULL;
void exit_function(const char *file, const char *function, const int line, const char *s, const int assert)
{
  fprintf(stderr, "exit_function %s:%d %s: %s\n", file, line, function, s);
  abort();
}
openair0_config_t openair0_cfg_g[MAX_CARDS];
int openair0_device_load(openair0_device_t *device, openair0_config_t *openair0_cfg)
{
  return -1;
}

void split7_test_init(NR_DL_FRAME_PARMS *fp)
{
  logInit();
  set_glog(OAILOG_WARNING);
  load_dftslib();
  const nrUE_cell_params_t cell = {.band = 78, .rf_frequency = 3319680000ULL, .numerology = 1, .N_RB_DL = 106, .ssb_start = 516};
  nr_init_frame_parms_ue_sa(fp, &cell);
  fp->nb_antennas_rx = 1;
  fp->nb_antennas_tx = 1;
  fp->ofdm_offset_divisor = 8;
  init_symbol_rotation(fp);
  init_timeshift_rotation(fp->ofdm_symbol_size,
                          fp->N_RB_DL * NR_NB_SC_PER_RB,
                          fp->nb_prefix_samples,
                          fp->ofdm_offset_divisor,
                          fp->timeshift_symbol_rotation);
}

void split7_test_ref_mod(NR_DL_FRAME_PARMS *fp, int slot, const c16_t *txdataF, c16_t *out)
{
  // Rotated in place: work on a copy.
  const size_t size = fp->symbols_per_slot * fp->ofdm_symbol_size * sizeof(c16_t);
  c16_t *copy = malloc16(size);
  memcpy(copy, txdataF, size);
  bool used[NR_SYMBOLS_PER_SLOT];
  for (int s = 0; s < NR_SYMBOLS_PER_SLOT; s++)
    used[s] = true;
  c16_t *txf[1] = {copy}, *txd[1] = {out};
  nr_tx_rotation_and_ofdm_mod(slot, fp, 1, txf, txd, link_type_ul, used, false);
  free(copy);
}

void split7_test_ref_demod(NR_DL_FRAME_PARMS *fp, int slot, int symbol, c16_t *rxdata, c16_t *out)
{
  PHY_VARS_NR_UE *ue = calloc(1, sizeof(*ue));
  ue->is_synchronized = 1;
  c16_t(*rxdataF)[fp->samples_per_slot_wCP] = malloc16_clear(fp->samples_per_slot_wCP * sizeof(c16_t));
  c16_t *rx[1] = {rxdata};
  nr_slot_fep(ue, fp, 0, slot, symbol, rxdataF, link_type_dl, 0, rx);
  memcpy(out, rxdataF[0] + symbol * fp->ofdm_symbol_size, fp->ofdm_symbol_size * sizeof(c16_t));
  free(rxdataF);
  free(ue);
}
