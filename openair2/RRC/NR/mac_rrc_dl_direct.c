/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <netinet/in.h>
#include <netinet/sctp.h>
#include <stdbool.h>
#include "assertions.h"
#include "f1ap_messages_types.h"
#include "mac_rrc_dl.h"
#include "nr_rrc_defs.h"
#include "openair2/LAYER2/NR_MAC_gNB/mac_rrc_dl_handler.h"

static void f1_reset_cu_initiated_direct(sctp_assoc_t assoc_id, const f1ap_reset_t *reset)
{
  AssertFatal(assoc_id == -1, "illegal assoc_id %d\n", assoc_id);
  f1_reset_cu_initiated(reset);
}

static void f1_reset_acknowledge_du_initiated_direct(sctp_assoc_t assoc_id, const f1ap_reset_ack_t *ack)
{
  UNUSED(assoc_id);
  (void)ack;
  AssertFatal(false, "%s() not implemented yet\n", __func__);
}

static void f1_setup_response_direct(sctp_assoc_t assoc_id, const f1ap_setup_resp_t *resp)
{
  AssertFatal(assoc_id == -1, "illegal assoc_id %d\n", assoc_id);
  f1_setup_response(resp);
}

static void f1_setup_failure_direct(sctp_assoc_t assoc_id, const f1ap_setup_failure_t *fail)
{
  AssertFatal(assoc_id == -1, "illegal assoc_id %d\n", assoc_id);
  f1_setup_failure(fail);
}

static void gnb_du_configuration_update_ack_direct(sctp_assoc_t assoc_id, const f1ap_gnb_du_configuration_update_acknowledge_t *ack)
{
  AssertFatal(assoc_id == -1, "illegal assoc_id %d\n", assoc_id);
  gnb_du_configuration_update_acknowledge(ack);
}

static void ue_context_setup_request_direct(sctp_assoc_t assoc_id, const f1ap_ue_context_setup_req_t *req)
{
  AssertFatal(assoc_id == -1, "illegal assoc_id %d\n", assoc_id);
  ue_context_setup_request(req);
}

static void ue_context_modification_request_direct(sctp_assoc_t assoc_id, const f1ap_ue_context_mod_req_t *req)
{
  AssertFatal(assoc_id == -1, "illegal assoc_id %d\n", assoc_id);
  ue_context_modification_request(req);
}

static void ue_context_modification_confirm_direct(sctp_assoc_t assoc_id, const f1ap_ue_context_modif_confirm_t *confirm)
{
  AssertFatal(assoc_id == -1, "illegal assoc_id %d\n", assoc_id);
  ue_context_modification_confirm(confirm);
}

static void ue_context_modification_refuse_direct(sctp_assoc_t assoc_id, const f1ap_ue_context_modif_refuse_t *refuse)
{
  AssertFatal(assoc_id == -1, "illegal assoc_id %d\n", assoc_id);
  ue_context_modification_refuse(refuse);
}

static void ue_context_release_command_direct(sctp_assoc_t assoc_id, const f1ap_ue_context_rel_cmd_t *cmd)
{
  AssertFatal(assoc_id == -1, "illegal assoc_id %d\n", assoc_id);
  ue_context_release_command(cmd);
}

static void dl_rrc_message_transfer_direct(sctp_assoc_t assoc_id, const f1ap_dl_rrc_message_t *dl_rrc)
{
  AssertFatal(assoc_id == -1, "illegal assoc_id %d\n", assoc_id);
  dl_rrc_message_transfer(dl_rrc);
}

static void f1_paging_transfer_direct(sctp_assoc_t assoc_id, const f1ap_paging_t *paging)
{
  AssertFatal(assoc_id == -1, "illegal assoc_id %d\n", assoc_id);
  f1_paging(paging);
}

void mac_rrc_dl_direct_init(nr_mac_rrc_dl_if_t *mac_rrc)
{
  mac_rrc->f1_reset = f1_reset_cu_initiated_direct;
  mac_rrc->f1_reset_acknowledge = f1_reset_acknowledge_du_initiated_direct;
  mac_rrc->f1_setup_response = f1_setup_response_direct;
  mac_rrc->f1_setup_failure = f1_setup_failure_direct;
  mac_rrc->gnb_du_configuration_update_acknowledge = gnb_du_configuration_update_ack_direct;
  mac_rrc->ue_context_setup_request = ue_context_setup_request_direct;
  mac_rrc->ue_context_modification_request = ue_context_modification_request_direct;
  mac_rrc->ue_context_modification_confirm = ue_context_modification_confirm_direct;
  mac_rrc->ue_context_modification_refuse = ue_context_modification_refuse_direct;
  mac_rrc->ue_context_release_command = ue_context_release_command_direct;
  mac_rrc->dl_rrc_message_transfer = dl_rrc_message_transfer_direct;
  mac_rrc->paging_transfer = f1_paging_transfer_direct;
}
