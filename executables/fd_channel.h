/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * FD channel between an O-RU simulating FD-UEs (nr-oru-ue.h) and nr-uesoftmodem --ue-fd
 * --ue-fd-attach: one shm_fd_iq_channel, created by the O-RU once its time is known. Symbols
 * are 3GPP symbols counted from hyper frame 0. Client u is UE u: DL streams
 * u * ue_nb_rx.., UL streams u * ue_nb_tx.. (ue_nb_rx = nb_ant_dl / nb_clients), each the
 * n_rb * 12 REs of the carrier from the lowest subcarrier, unrotated. A UE's PRACH is a
 * message (fd_channel_prach_t) at the first symbol of the slot.
 */

#ifndef FD_CHANNEL_H
#define FD_CHANNEL_H

#include <stdint.h>
#include "common/platform_types.h"

#define FD_CHANNEL_RING_SLOTS 20
#define FD_CHANNEL_PRACH_MAX_BINS 839

/// PRACH sequence: n_zc bins from bin k (modulo dftlen) of a dftlen-point PRACH DFT.
typedef struct {
  int32_t ue;
  int32_t dftlen;
  int32_t k;
  int32_t n_zc;
  c16_t bins[FD_CHANNEL_PRACH_MAX_BINS];
} fd_channel_prach_t;

#endif
