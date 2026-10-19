/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * Interface between the UE PHY and a frequency-domain (FD) Low-PHY: when
 * PHY_VARS_NR_UE.fd_ifdevice is set, the PHY exchanges frequency-domain
 * symbols with it in place of FFT/IFFT on the radio's samples.
 */

#ifndef NR_UE_FD_H
#define NR_UE_FD_H

#include <stdbool.h>
#include "PHY/defs_nr_UE.h"

/// FD-UE interface device (nr_ue_fd_ifdevice_t)
struct nr_ue_fd_ifdevice_s {
  /// Copies a demodulated DL symbol, one pointer per RX antenna. False when it is no longer available.
  bool (*read_symbol)(nr_ue_fd_ifdevice_t *dev, int frame, int slot, int symbol, c16_t *const *dst);
  /// Takes a TX slot's frequency-domain symbols in place of OFDM modulation.
  void (*write_slot)(nr_ue_fd_ifdevice_t *dev,
                     int hfn,
                     int frame,
                     int slot,
                     int nb_ant_tx,
                     c16_t **txdataF,
                     const bool *was_symbol_used);
  /// Takes a frequency-domain PRACH preamble in place of IDFT and CP insertion.
  void (*write_prach)(nr_ue_fd_ifdevice_t *dev,
                      int frame,
                      int slot,
                      const c16_t *prachF,
                      int dftlen,
                      int Ncp,
                      int prach_start,
                      int repetitions);
  void *priv;
};

#endif
