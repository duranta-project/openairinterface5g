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

/* A peer is inserted when SCTP association is live and keyed only by assoc_id
 * and is removed from the tree the moment its association goes down. */
typedef struct xnap_peer_s {
  RB_ENTRY(xnap_peer_s) entry;
  sctp_assoc_t      assoc_id;
  xnap_peer_state_t state;
  /* negotiated SCTP in-streams */
  uint16_t          in_streams;
  /* negotiated SCTP out-streams */
  uint16_t          out_streams;
  /* filled after Xn Setup Response */
  uint32_t          remote_gnb_id;
} xnap_peer_t;

/* Per-local-gNB Xn state, indexed by instance number. */
typedef struct xnap_gnb_inst_s {
  instance_t        instance;
  uint32_t          gnb_id;
  /* local gNB's own identity/capabilities */
  xnap_setup_req_t setup_info;
  xnap_net_config_t net_config;
  /* number of peers currently in the tree (live SCTP association) */
  uint8_t           nb_peers;
  RB_HEAD(xnap_peer_map, xnap_peer_s) peers;
} xnap_gnb_inst_t;

RB_PROTOTYPE(xnap_peer_map, xnap_peer_s, entry, xnap_peer_compare);

xnap_gnb_inst_t *xnap_get_inst(instance_t instance);

/* Lookup a connected/connecting peer by its (unique) assoc_id. */
xnap_peer_t *xnap_get_peer_by_assoc(xnap_gnb_inst_t *inst, sctp_assoc_t assoc_id);

/* Allocate a peer for a newly-up SCTP association, insert it in the tree
 * keyed by assoc_id, and return it. AssertFatal()s if assoc_id is already
 * present (would indicate an SCTP/ITTI-layer bug). */
xnap_peer_t *xnap_add_peer(xnap_gnb_inst_t *inst, sctp_assoc_t assoc_id, uint16_t in_streams, uint16_t out_streams);

/* Remove and free a peer, e.g. on SCTP shutdown/close. */
void xnap_remove_peer(xnap_gnb_inst_t *inst, xnap_peer_t *peer);

void xnap_create_inst(instance_t instance, xnap_setup_req_t *setup_info, xnap_net_config_t *net_config);

#define XNAP_NON_UE_STREAM_ID 0

#endif /* XNAP_COMMON_H_ */
