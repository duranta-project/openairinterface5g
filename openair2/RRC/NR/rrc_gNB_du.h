/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef RRC_GNB_DU_H_
#define RRC_GNB_DU_H_

#include <netinet/in.h>
#include <netinet/sctp.h>
#include <stdint.h>
#include <stdio.h>
struct f1ap_setup_req_s;
struct f1ap_lost_connection_t;
struct gNB_RRC_INST_s;
struct nr_rrc_du_container_t;
struct nr_rrc_cell_container_t;
struct f1ap_gnb_du_configuration_update_s;
struct f1ap_served_cell_info_t;
struct f1ap_trp_information_req_s;
struct f1ap_measurement_req_s;

#include "f1ap_messages_types.h"
#include "ngap_messages_types.h"

void rrc_gNB_process_f1_setup_req(f1ap_setup_req_t *req, sctp_assoc_t assoc_id);
void rrc_CU_process_f1_lost_connection(struct gNB_RRC_INST_s *rrc, f1ap_lost_connection_t *lc, sctp_assoc_t assoc_id);
void rrc_gNB_process_f1_du_configuration_update(struct f1ap_gnb_du_configuration_update_s *conf_up, sctp_assoc_t assoc_id);

struct nr_rrc_du_container_t *get_du_for_ue(struct gNB_RRC_INST_s *rrc, uint32_t ue_id);

void dump_du_info(const struct gNB_RRC_INST_s *rrc, FILE *f);

int get_ssb_arfcn(const struct nr_rrc_cell_container_t *cell);

struct nr_rrc_du_container_t *find_target_du(struct gNB_RRC_INST_s *rrc, sctp_assoc_t source_assoc_id);

// the assoc_id might be 0 (if the DU goes offline). Below helper macro to
// print an error and return from the function in that case
#define RETURN_IF_INVALID_ASSOC_ID(assoc_id)                               \
  {                                                                        \
    if (assoc_id == 0) {                                                   \
      LOG_E(NR_RRC, "cannot send data: invalid assoc_id 0, DU offline\n"); \
      return;                                                              \
    }                                                                      \
  }

void trigger_f1_reset(struct gNB_RRC_INST_s *rrc, sctp_assoc_t du_assoc_id);

void rrc_send_paging_to_dus(struct gNB_RRC_INST_s *rrc, const nr_tai_t tais[], uint8_t n_tai, f1ap_paging_t *f1ap_msg);

void rrc_send_trp_information_request_to_dus(struct gNB_RRC_INST_s *rrc, struct f1ap_trp_information_req_s *msg);
void rrc_handle_trp_information_response(struct gNB_RRC_INST_s *rrc, sctp_assoc_t assoc_id, f1ap_trp_information_resp_t *resp);
void rrc_handle_trp_information_failure(struct gNB_RRC_INST_s *rrc, sctp_assoc_t assoc_id, f1ap_trp_information_failure_t *fail);
void rrc_trp_transaction_tick(struct gNB_RRC_INST_s *rrc);
void rrc_send_positioning_measurement_request_to_dus(struct gNB_RRC_INST_s *rrc, struct f1ap_measurement_req_s *msg);

#endif /* RRC_GNB_DU_H_ */
