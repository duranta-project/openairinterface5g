/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef XNAP_GNB_HANDLERS_H_
#define XNAP_GNB_HANDLERS_H_

#include <stdint.h>
#include "common/platform_types.h"
#include "openair2/COMMON/sctp_messages_types.h"
#include "xnap_common.h"

void xnap_handle_xn_setup_message(instance_t instance, xnap_gnb_inst_t *inst, xnap_peer_t *peer, int sctp_shutdown);

#endif /* XNAP_GNB_HANDLERS_H_ */

