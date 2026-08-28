/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef XNAP_COMMON_H_
#define XNAP_COMMON_H_

#include "tree.h"
#include "common/platform_types.h"
#include "openair2/COMMON/sctp_messages_types.h"
#include "openair2/COMMON/xnap_messages_types.h"

typedef enum {
  /* SCTP up, XnSetup in progress */
  XNAP_PEER_STATE_WAITING = 0,
  /* XnSetup exchange complete */
  XNAP_PEER_STATE_CONNECTED,
} xnap_peer_state_t;

/* A peer is inserted when SCTP is up and keyed solely by assoc_id, and
 * a peer is removed from the tree the moment its association goes down. */
typedef struct xnap_peer_s {
  RB_ENTRY(xnap_peer_s) entry;
  sctp_assoc_t assoc_id;
  xnap_peer_state_t state;
  /* negotiated SCTP in-streams */
  uint16_t in_streams;
  /* negotiated SCTP out-streams */
  uint16_t out_streams;
  /* filled after Xn Setup Response */
  uint32_t remote_gnb_id;
} xnap_peer_t;

/* Per-local-gNB Xn state, indexed by instance number. */
typedef struct xnap_gnb_inst_s {
  /* local gNB's own identity/capabilities */
  xnap_setup_req_t setup_info;
  xnap_net_config_t net_config;
  RB_HEAD(xnap_peer_map, xnap_peer_s) peers;
  /* set once Xn bring-up (listener bind + initial dials) has been started
   * for this instance; never cleared, independent of peer connectivity */
  bool xn_started;
} xnap_gnb_inst_t;

RB_PROTOTYPE(xnap_peer_map, xnap_peer_s, entry, xnap_peer_compare);

xnap_gnb_inst_t *xnap_get_inst(instance_t instance);

void xnap_create_inst(instance_t instance, xnap_setup_req_t *setup_info, xnap_net_config_t *net_config);

#endif /* XNAP_COMMON_H_ */
