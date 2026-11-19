/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * O-RU south side with FD-UEs instead of a radio: hosts a UE Low-PHY port (fd_host.h) for
 * nr-uesoftmodem --ue-fd --ue-fd-attach and passes each UE's REs through its own channel,
 * with no DFT, cyclic prefix or rotation (unit gain: test FdEquivalentToTimeDomain).
 *
 * Per UE and RE, H is ue_nb_rx x nb_txru: DL y_r = sum_t H[r][t] x_t, UL (reciprocal)
 * y_a = sum_u sum_t H_u[t][a] x_u,t. Absolute symbols are 3GPP symbols.
 */

#ifndef NR_ORU_UE_H
#define NR_ORU_UE_H

#include <stdbool.h>
#include <stdint.h>
#include "PHY/defs_nr_common.h"
#include "PHY/impl_defs_nr.h"

typedef enum {
  ORU_UE_CHANNEL_FLAT, ///< per UE: gain_db, a random phase per antenna pair, UE antenna r on TXRU r % nb_txru
  ORU_UE_CHANNEL_CDL, ///< TR 38.901 CDL per UE, through the TXRU array
} oru_ue_channel_kind_t;

typedef struct {
  const char *port_name; ///< shared memory name of the port
  int max_ues;
  int ue_nb_rx;
  int ue_nb_tx;
  bool dl_publish_air; ///< publish DL symbols at their air time only, not as soon as assembled
  oru_ue_channel_kind_t channel;
  int cdl_model; ///< cdl_model_t
  double delay_spread_ns;
  double ue_speed_mps;
  double gain_db;
  int txru_rows; ///< TXRU array (CDL): rows x cols, spacings in wavelengths
  int txru_cols;
  double txru_dv;
  double txru_dh;
  uint64_t seed;
} oru_ue_config_t;

typedef struct oru_ue oru_ue_t;

/// nb_txru: the O-RU's TX = RX antennas.
oru_ue_t *oru_ue_create(const oru_ue_config_t *cfg, const NR_DL_FRAME_PARMS *fp, int nb_txru);
void oru_ue_destroy(oru_ue_t *ues);

/// Publishes the time anchor; DL symbols from first_symbol on follow.
void oru_ue_start(oru_ue_t *ues, uint64_t first_symbol);

/// DL symbol: re[t] holds the n_rb_dl * 12 REs of TXRU t (FFT shifted, unrotated). Writes every
/// open client's DL for it. Symbols of different threads may be written concurrently.
void oru_ue_dl_symbol(oru_ue_t *ues, uint64_t symbol, const c16_t *const *re);

/// Publishes DL symbols below end (no-op for those published already): those not written by
/// oru_ue_dl_symbol() are silent. Thread safe.
void oru_ue_dl_publish(oru_ue_t *ues, uint64_t end);

/// Takes UL slot slot of every client: what is missing now is silence. Once per slot, in order.
void oru_ue_ul_take_slot(oru_ue_t *ues, uint64_t slot);

/// UL symbol of each O-RU antenna a with ant[a] != NULL, in FFT layout (fft_size, not
/// shifted). False if the slot is not taken.
bool oru_ue_ul_symbol(oru_ue_t *ues, int frame, int slot, int symbol, c16_t *const *ant);

/// PRACH repetition of O-RU antenna a: n_zc bins from bin k of the UEs' dftlen-point PRACH of
/// the taken slot (frame % 1024, slot), as rx_nr_prach_ru_rep() takes them from the DFT.
void oru_ue_prach(oru_ue_t *ues, int frame, int slot, int k, int dftlen, int n_zc, int nb_ant, c16_t (*rxsigF)[NR_PRACH_SEQ_LEN_L]);

void oru_ue_stop(oru_ue_t *ues);

#endif
