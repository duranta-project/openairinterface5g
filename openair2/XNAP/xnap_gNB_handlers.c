/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "xnap_gNB_handlers.h"
#include "xnap_gNB.h"
#include "xnap_common.h"
#include "common/utils/LOG/log.h"
#include "common/utils/ocp_itti/intertask_interface.h"
#include "assertions.h"
#include "xnap_gNB_encoder.h"
#include "lib/xnap_gNB_interface_management.h"
#include "lib/xnap_gNB_mobility_management.h"
#include "aper_decoder.h"

/* Helper for SCTP Shutdown or Xn setup */
void xnap_handle_xn_setup_message(instance_t instance, xnap_gnb_inst_t *inst, xnap_peer_t *peer, int sctp_shutdown)
{
  if (sctp_shutdown) {
    if (peer->state == XNAP_PEER_STATE_CONNECTED || peer->state == XNAP_PEER_STATE_WAITING) {
      LOG_W(XNAP, "[gNB %ld] Xn peer assoc_id %d (gNB_id 0x%x) disconnected\n", instance, peer->assoc_id, peer->remote_gnb_id);

      /* Notify RRC only if XnSetup had completed — i.e. the peer was CONNECTED
       * and RRC holds a candidate entry for it.  WAITING peers never reached
       * RRC so there is nothing to remove. */
      if (peer->state == XNAP_PEER_STATE_CONNECTED && peer->remote_gnb_id != 0) {
        MessageDef *msg = itti_alloc_new_message(TASK_XNAP, inst->instance, XNAP_PEER_SHUTDOWN_IND);
        XNAP_PEER_SHUTDOWN_IND(msg).gnb_id = peer->remote_gnb_id;
        itti_send_msg_to_task(TASK_RRC_GNB, inst->instance, msg);
      }

      peer->state = XNAP_PEER_STATE_DISCONNECTED;
      /* TODO: release XNAP UE contexts using this link */
    }
  } else {
    peer->state = XNAP_PEER_STATE_CONNECTED;
    LOG_I(XNAP, "[gNB %ld] Xn peer assoc_id %d (gNB_id 0x%x) setup complete — notifying RRC\n",
          instance, peer->assoc_id, peer->remote_gnb_id);

    /* Notify RRC so it can register this peer as an Xn-capable candidate */
    MessageDef *msg = itti_alloc_new_message(TASK_XNAP, inst->instance, XNAP_SETUP_IND);
    XNAP_SETUP_IND(msg).gnb_id   = peer->remote_gnb_id;
    XNAP_SETUP_IND(msg).assoc_id = peer->assoc_id;
    itti_send_msg_to_task(TASK_RRC_GNB, inst->instance, msg);
  }
}
