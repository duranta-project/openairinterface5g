/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#ifndef SHM_FD_IQ_CHANNEL_H
#define SHM_FD_IQ_CHANNEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "common/platform_types.h"
#include "shm_iq_channel.h"

/**
 * Frequency domain counterpart of ShmTDIQChannel: a server (the time source) and its clients
 * exchange OFDM symbols, n_re REs per antenna stream, DL from the server, UL to it. Clients
 * share the channel by antenna streams, each using its own range.
 *
 * Time is in absolute symbols chosen by the server: the current symbol (the first one not
 * produced yet) starts at first_symbol and shm_fd_iq_channel_produce() moves it on. With c the
 * current symbol and H half the ring:
 * - tx of symbol s: c <= s < c + H, else CHANNEL_ERROR_TOO_LATE / CHANNEL_ERROR_TOO_EARLY
 * - rx of symbol s: c - H <= s < c, else CHANNEL_ERROR_TOO_LATE / CHANNEL_ERROR_TOO_EARLY
 * rx zeroes what it read: a symbol not written again reads as silence.
 *
 * Clients also post messages tagged with a symbol (same window). A client connects only to a
 * server whose current symbol moves.
 */

typedef struct ShmFDIQChannel_s ShmFDIQChannel;

typedef struct {
  uint32_t n_re_dl;
  uint32_t n_re_ul;
  uint32_t nb_ant_dl; ///< streams from the server
  uint32_t nb_ant_ul; ///< streams to the server
  uint32_t ring_symbols; ///< even
  uint32_t nb_clients; ///< for the clients to split the streams by
  uint32_t nb_msg; ///< message entries
  uint32_t msg_max_size; ///< bytes
} ShmFDIQGeometry;

ShmFDIQChannel *shm_fd_iq_channel_create(const char *name, const ShmFDIQGeometry *geo, uint64_t first_symbol);
/// NULL if no live server within timeout_ms.
ShmFDIQChannel *shm_fd_iq_channel_connect(const char *name, int timeout_ms);
/// Server: stops, unmaps and unlinks the channel. Client: unmaps it.
void shm_fd_iq_channel_destroy(ShmFDIQChannel *channel);
const ShmFDIQGeometry *shm_fd_iq_channel_geometry(const ShmFDIQChannel *channel);

/// Writes symbol to the caller's TX streams first_ant..first_ant + nb_ant - 1.
IQChannelErrorType shm_fd_iq_channel_tx(ShmFDIQChannel *channel, uint64_t symbol, int first_ant, int nb_ant, const c16_t *const *data);
/// Reads symbol from the caller's RX streams first_ant..first_ant + nb_ant - 1, then zeroes them.
IQChannelErrorType shm_fd_iq_channel_rx(ShmFDIQChannel *channel, uint64_t symbol, int first_ant, int nb_ant, c16_t *const *dst);

/// Client: posts a message for symbol (tx window). CHANNEL_ERROR_NO_DATA if no entry is free.
IQChannelErrorType shm_fd_iq_channel_tx_msg(ShmFDIQChannel *channel, uint64_t symbol, const void *msg, size_t len);
/// Server: copies entry's message if it was posted for symbol (rx window), else CHANNEL_ERROR_NO_DATA.
IQChannelErrorType shm_fd_iq_channel_rx_msg(const ShmFDIQChannel *channel, uint64_t symbol, int entry, void *msg, size_t max_len, size_t *len);

/// Server: moves the current symbol on and wakes the waiters.
void shm_fd_iq_channel_produce(ShmFDIQChannel *channel, uint64_t num_symbols);
uint64_t shm_fd_iq_channel_get_current_symbol(const ShmFDIQChannel *channel);
/// Waits until the current symbol reaches symbol. timeout_us 0: forever.
/// 0 reached, 1 timed out, -1 aborted or the server stopped.
int shm_fd_iq_channel_wait(ShmFDIQChannel *channel, uint64_t symbol, uint64_t timeout_us);
/// Makes this process' waits return -1, for good.
void shm_fd_iq_channel_abort(ShmFDIQChannel *channel);
bool shm_fd_iq_channel_is_stopped(const ShmFDIQChannel *channel);

#endif
