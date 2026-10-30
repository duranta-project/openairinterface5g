/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief s1ap procedures for overload messages within eNB
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "intertask_interface.h"

#include "s1ap_common.h"
#include "s1ap_eNB_defs.h"

#include "s1ap_eNB.h"
#include "s1ap_eNB_ue_context.h"
#include "s1ap_eNB_encoder.h"
#include "s1ap_eNB_overload.h"
#include "s1ap_eNB_management_procedures.h"

#include "assertions.h"
#include "S1AP_ProtocolIE-Container.h"

int s1ap_eNB_handle_overload_start(sctp_assoc_t assoc_id, uint32_t stream, S1AP_S1AP_PDU_t *pdu)
{
    s1ap_eNB_mme_data_t     *mme_desc_p;
    S1AP_OverloadStart_t    *container;
    S1AP_OverloadStartIEs_t *ie;

    DevAssert(pdu != NULL);

    container = &pdu->choice.initiatingMessage->value.choice.OverloadStart;

    S1AP_FIND_PROTOCOLIE_BY_ID(S1AP_OverloadStartIEs_t, ie, container,
                               S1AP_ProtocolIE_ID_id_OverloadResponse, true);
    if (ie != NULL) {
        DevCheck(ie->value.choice.OverloadResponse.present ==
                 S1AP_OverloadResponse_PR_overloadAction,
                 S1AP_OverloadResponse_PR_overloadAction, 0, 0);
    }
    else
    {
        return -1;
    }
    /* Non UE-associated signalling -> stream 0 */
    if (stream != 0) {
      S1AP_ERROR("[SCTP %u] Received s1 overload start on stream != 0 (%u)\n",
                 assoc_id, stream);
      return -1;
    }

    if ((mme_desc_p = s1ap_eNB_get_MME(NULL, assoc_id, 0)) == NULL) {
        /* No MME context associated */
        return -1;
    }

    /* Mark the MME as overloaded and set the overload state according to
     * the value received.
     */
    mme_desc_p->state = S1AP_ENB_OVERLOAD;
    mme_desc_p->overload_state =
        ie->value.choice.OverloadResponse.choice.overloadAction;

    return 0;
}

int s1ap_eNB_handle_overload_stop(sctp_assoc_t assoc_id, uint32_t stream, S1AP_S1AP_PDU_t *pdu)
{
    /* We received Overload stop message, meaning that the MME is no more
     * overloaded. This is an empty message, with only message header and no
     * Information Element.
     */
    DevAssert(pdu != NULL);

    s1ap_eNB_mme_data_t *mme_desc_p;

    /* Non UE-associated signalling -> stream 0 */
    if (stream != 0) {
      S1AP_ERROR("[SCTP %u] Received s1 overload stop on stream != 0 (%u)\n",
                 assoc_id, stream);
      return -1;
    }

    if ((mme_desc_p = s1ap_eNB_get_MME(NULL, assoc_id, 0)) == NULL) {
        /* No MME context associated */
        return -1;
    }

    mme_desc_p->state = S1AP_ENB_STATE_CONNECTED;
    mme_desc_p->overload_state = S1AP_NO_OVERLOAD;
    return 0;
}
