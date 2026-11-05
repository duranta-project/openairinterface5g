/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef PACKET_FILTER_H_
#define PACKET_FILTER_H_

#include <stdint.h>
#include "common/5g_packet_filter.h"

/**
 * @brief Decode packet filter contents from buffer
 * @param buf Buffer containing packet filter component types and values
 * @param length Length of packet filter contents
 * @param pf Output: decoded packet filter
 * @return Number of bytes decoded, or -1 on error
 */
int decode_packet_filter_contents(uint8_t *buf, uint8_t length, packet_filter_decoded_t *pf);

#endif /* PACKET_FILTER_H_ */
