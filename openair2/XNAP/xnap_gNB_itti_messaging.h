/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief xnap itti messages handlers for gNB
 */

#ifndef XNAP_GNB_ITTI_MESSAGING_H_
#define XNAP_GNB_ITTI_MESSAGING_H_

#include <assertions.h>
#include <netinet/sctp.h>
#include <stdint.h>

void xnap_gNB_itti_send_sctp_data(instance_t instance,
                                   sctp_assoc_t assoc_id,
                                   uint8_t *buffer,
                                   uint32_t length,
                                   uint16_t stream);

#endif /* XNAP_GNB_ITTI_MESSAGING_H_ */

