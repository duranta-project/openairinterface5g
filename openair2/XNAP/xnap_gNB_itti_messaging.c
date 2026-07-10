/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdlib.h>
#include <string.h>
#include "intertask_interface.h"
#include "sctp_messages_types.h"

void xnap_gNB_itti_send_sctp_data(instance_t instance,
                                   sctp_assoc_t assoc_id,
                                   uint8_t *buffer,
                                   uint32_t length,
                                   uint16_t stream)
{
  MessageDef *msg = itti_alloc_new_message(TASK_XNAP, instance, SCTP_DATA_REQ);
  sctp_data_req_t *req = &msg->ittiMsg.sctp_data_req;
  req->assoc_id      = assoc_id;
  req->buffer        = buffer;
  req->buffer_length = length;
  req->stream        = stream;
  itti_send_msg_to_task(TASK_SCTP, instance, msg);
}
