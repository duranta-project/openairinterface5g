/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#ifndef SHM_IQ_CHANNEL_COMMON_H
#define SHM_IQ_CHANNEL_COMMON_H

/// Types shared by the time domain (shm_td_iq_channel.h) and frequency domain
/// (shm_fd_iq_channel.h) shared memory IQ channels.

typedef enum IQChannelType { IQ_CHANNEL_TYPE_SERVER, IQ_CHANNEL_TYPE_CLIENT } IQChannelType;

typedef enum IQChannelErrorType {
  CHANNEL_NO_ERROR,
  CHANNEL_ERROR_TOO_LATE,
  CHANNEL_ERROR_TOO_EARLY,
  CHANNEL_ERROR_NOT_CONNECTED,
  CHANNEL_ERROR_NO_DATA, ///< frequency domain: nothing (intact) was written for that time
} IQChannelErrorType;

#endif
