/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * O-RU south side with FD-UEs instead of a radio, on an FD channel (fd_channel.h). Each UE's
 * REs pass through its channel model, with no DFT, cyclic prefix or rotation. Per UE and RE,
 * H is ue_nb_rx x nb_txru: DL y_r = sum_t H[r][t] x_t, UL y_a = sum_u sum_t H_u[t][a] x_u,t.
 */

#ifndef NR_ORU_UE_H
#define NR_ORU_UE_H

#include <stdbool.h>
#include <stdint.h>
#include "PHY/defs_nr_common.h"
#include "PHY/impl_defs_nr.h"
#include "executables/fd_channel.h"

typedef enum {
  ORU_UE_CHANNEL_MODEL_FLAT,
  ORU_UE_CHANNEL_MODEL_CDL_A, ///< TR 38.901, B..E follow
  ORU_UE_CHANNEL_MODEL_CDL_B,
  ORU_UE_CHANNEL_MODEL_CDL_C,
  ORU_UE_CHANNEL_MODEL_CDL_D,
  ORU_UE_CHANNEL_MODEL_CDL_E,
} oru_ue_channel_model_type_t;

typedef struct {
  oru_ue_channel_model_type_t type;
  double delay_spread_ns;
  double speed_mps;
  double gain_db;
  uint64_t seed;
} oru_ue_channel_model_t;

typedef struct {
  const char *channel_name; ///< shared memory name of the FD channel
  int num_ues;
  int ue_nb_rx;
  int ue_nb_tx;
  bool dl_publish_air; ///< publish DL at air time, not as soon as assembled
  const oru_ue_channel_model_t *models; ///< [num_ues]
  int txru_rows; ///< CDL TXRU array, spacings in wavelengths; cols 0: nb_txru / rows
  int txru_cols;
  double txru_dv;
  double txru_dh;
} oru_ue_config_t;

typedef struct oru_ue oru_ue_t;

/// nb_txru: the O-RU's TX = RX antennas.
oru_ue_t *oru_ue_create(const oru_ue_config_t *cfg, const NR_DL_FRAME_PARMS *fp, int nb_txru);
void oru_ue_destroy(oru_ue_t *ues);

/// True once UE ue's channel model is built: until then its DL is silent and its UL and PRACH
/// dropped.
bool oru_ue_channel_ready(oru_ue_t *ues, int ue);

/// Creates the FD channel, its current symbol at first_symbol (3GPP symbols).
void oru_ue_start(oru_ue_t *ues, uint64_t first_symbol);

/// DL symbol, re[t] the n_rb_dl * 12 REs of TXRU t. Thread safe for distinct symbols.
void oru_ue_dl_symbol(oru_ue_t *ues, uint64_t symbol, const c16_t *const *re);

/// Publishes the symbols below end: the UEs read their DL, their UL is final. Thread safe.
void oru_ue_dl_publish(oru_ue_t *ues, uint64_t end);

/// Published UL symbol (frame % 1024, slot, symbol) of each O-RU antenna a with ant[a], in DFT
/// layout (fft_size). Read once. False before oru_ue_start().
bool oru_ue_ul_symbol(oru_ue_t *ues, int frame, int slot, int symbol, c16_t *const *ant);

/// PRACH of published slot (frame % 1024, slot) at O-RU antenna a: n_zc bins from bin k of a
/// dftlen-point DFT.
void oru_ue_prach(oru_ue_t *ues, int frame, int slot, int k, int dftlen, int n_zc, int nb_ant, c16_t (*rxsigF)[NR_PRACH_SEQ_LEN_L]);

#endif
