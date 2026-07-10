/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <string.h>
#include <stdlib.h>
#include "common/platform_types.h"
#include "common/utils/LOG/log.h"
#include "intertask_interface.h"
#include "xnap_messages_types.h"

void *xnap_task(void *args)
{
  UNUSED(args);
  LOG_I(XNAP, "Starting XnAP task\n");
  itti_mark_task_ready(TASK_XNAP);

  while (1) {
    MessageDef *msg = NULL;
    itti_receive_msg(TASK_XNAP, &msg);
    const instance_t instance = ITTI_MSG_DESTINATION_INSTANCE(msg);
    const int msgType = ITTI_MSG_ID(msg);
    LOG_D(XNAP, "XnAP received %s for instance %ld\n", ITTI_MSG_NAME(msg), instance);

    switch (msgType) {
      default:
        LOG_E(XNAP, "Unknown message type %d (%s)\n", msgType, ITTI_MSG_NAME(msg));
        break;
    }

    int result = itti_free(ITTI_MSG_ORIGIN_ID(msg), msg);
    AssertFatal(result == EXIT_SUCCESS, "Failed to free ITTI message (%d)\n", result);
    msg = NULL;
  }
}
