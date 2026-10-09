/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _NR_SDAP_GNB_H_
#define _NR_SDAP_GNB_H_

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include "common/platform_types.h"

struct nr_sdap_entity_s;

/*
 * TS 37.324 4.4 Functions
 * Transfer of user plane data
 * Downlink - gNB
 * Uplink   - nrUE
 */
bool sdap_data_req(protocol_ctxt_t *ctxt_p,
                   const ue_id_t ue_id,
                   const srb_flag_t srb_flag,
                   const mui_t mui,
                   const confirm_t confirm,
                   const sdu_size_t sdu_buffer_size,
                   unsigned char *const sdu_buffer,
                   const pdcp_transmission_mode_t pt_mode,
                   const uint32_t *sourceL2Id,
                   const uint32_t *destinationL2Id,
                   const uint8_t qfi,
                   const bool rqi,
                   const int pdusession_id);

/*
 * TS 37.324 4.4 Functions
 * Transfer of user plane data
 * Uplink   - gNB
 * Downlink - nrUE
 */
void sdap_data_ind(int drb_id, int is_gnb, int pdusession_id, ue_id_t ue_id, char *buf, int size);

void start_sdap_tun_gnb_first_ue_default_pdu_session(ue_id_t ue_id, int pdu_session_id);
void nr_sdap_generate_gnb_tun_ifname(char *ifname, ue_id_t ue_id);
void nr_sdap_tun_bind(struct nr_sdap_entity_s *entity, int sock, int qfi);
void nr_sdap_tun_stop_reader(pthread_t *thread);
void nr_sdap_tun_start_reader(struct nr_sdap_entity_s *entity, pthread_t *thread, char *name);
void nr_sdap_tun_start_idle_listener(ue_id_t ue_id, int pdu_session_id, int sock, pthread_t *thread);

#endif
