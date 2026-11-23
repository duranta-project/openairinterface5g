/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#ifndef SHM_IQ_CHANNEL_H
#define SHM_IQ_CHANNEL_H

#include "../threadPool/pthread_utils.h"
#include <stdint.h>
#include <stdbool.h>
#include <semaphore.h>
#include "shm_iq_channel.h"

#define SHM_MAGIC_NUMBER 0x12345678

/**
 * ShmTdIqChannel is a shared memory bidirectional time domain IQ channel with a single clock
 * source. The server (clock source) shall create the channel while the client should connect to
 * it.
 *
 * To write samples, send them via a pointer passed to shm_td_iq_channel_tx.
 * To read samples, receive them via a pointer passed to shm_td_iq_channel_rx. You can either wait until
 * the sample are available using shm_td_iq_channel_wait or poll using shm_td_iq_channel_rx until the return
 * value is not CHANNEL_ERROR_TOO_EARLY.
 * To indicate that samples are ready to be read by the client, call shm_td_iq_channel_produce_samples
 * (server only).
 *
 * The timestamps used in the API are in absolute samples from the time the client/server connection started.
 */

typedef uint32_t sample_t;

typedef struct ShmTDIQChannel_s ShmTDIQChannel;

/**
 * @brief Creates a shared memory IQ channel.
 *
 * @param name The name of the shared memory segment.
 * @param num_tx_ant The number of TX antennas.
 * @param num_rx_ant The number of RX antennas.
 * @return A pointer to the created ShmTDIQChannel structure.
 */
ShmTDIQChannel *shm_td_iq_channel_create(const char *name, int num_tx_ant, int num_rx_ant);

/**
 * @brief Connects to an existing shared memory IQ channel.
 *
 * @param name The name of the shared memory segment.
 * @param timeout_in_seconds The timeout in seconds for the connection attempt.
 * @return A pointer to the connected ShmTDIQChannel structure.
 */
ShmTDIQChannel *shm_td_iq_channel_connect(const char *name, int timeout_in_seconds);

/**
 * @brief Transmit data in the channel
 *
 * @param channel The ShmTDIQChannel structure.
 * @param timestamp The timestamp for which to get the TX IQ data slot.
 * @param num_samples The number of samples to write.
 * @param antenna The antenna index.
 * @param tx_iq_data The TX IQ data to write.
 *
 * @return CHANNEL_NO_ERROR if successful, error type otherwise
 */
IQChannelErrorType shm_td_iq_channel_tx(ShmTDIQChannel *channel,
                                        uint64_t timestamp,
                                        uint64_t num_samples,
                                        int antenna,
                                        const sample_t *tx_iq_data);

/**
 * @brief Receive iq data from the channel
 *
 * @param channel The ShmTDIQChannel structure.
 * @param timestamp The timestamp for which to get the RX IQ data slot.
 * @param num_samples The number of samples to read.
 * @param antenna The antenna index.
 * @param tx_iq_data pointer to the RX IQ data slot.
 * @return CHANNEL_NO_ERROR if successful, error type otherwise
 */
IQChannelErrorType shm_td_iq_channel_rx(ShmTDIQChannel *channel,
                                        uint64_t timestamp,
                                        uint64_t num_samples,
                                        int antenna,
                                        sample_t *tx_iq_data);

/**
 * @brief Advances the time in the channel by specified number of samples
 *
 * @param channel The ShmTDIQChannel structure.
 * @param num_symbols The number of samples to produce.
 */
void shm_td_iq_channel_produce_samples(ShmTDIQChannel *channel, uint64_t num_samples);

/**
 * @brief Wait until sample at the specified timestamp is available
 *
 * @param channel The ShmTDIQChannel structure.
 * @param timestamp The timestamp for which to wait.
 * @param timeout_uS The timeout in microseconds to wait for the sample. 0 means wait indefinitely.
 *
 * @return 0 if the sample is available, 1 if timed out
 */
int shm_td_iq_channel_wait(ShmTDIQChannel *channel, uint64_t timestamp, uint64_t timeout_uS);

/**
 * @brief Aborts the IQ channel causing the wait to return immediately
 *
 * @param channel The ShmTDIQChannel structure.
 */
void shm_td_iq_channel_abort(ShmTDIQChannel *channel);

/**
 * @brief Checks if the IQ channel is aborted.
 *
 * @param channel The ShmTDIQChannel structure.
 * @return True if the channel is aborted, false otherwise.
 */
bool shm_td_iq_channel_is_aborted(const ShmTDIQChannel *channel);

/**
 * @brief Destroys the shared memory IQ channel.
 *
 * @param channel The ShmTDIQChannel structure.
 */
void shm_td_iq_channel_destroy(ShmTDIQChannel *channel);

/**
 * @brief Returns current sample
 *
 * @param channel The ShmTDIQChannel structure.
 *
 * @return Current time as sample count since beginning of transmission
 */
uint64_t shm_td_iq_channel_get_current_sample(const ShmTDIQChannel *channel);

#endif
