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
  /* no active SCTP association */
  XNAP_PEER_STATE_DISCONNECTED = 0,
  /* SCTP up, XnSetup in progress */
  XNAP_PEER_STATE_WAITING,
  /* XnSetup exchange complete */
  XNAP_PEER_STATE_CONNECTED,
} xnap_peer_state_t;

/* State of one peer gNB Xn connection.
 * Before SCTP connects: assoc_id == -1, tree is ordered by cnx_id.
 * After SCTP connects : call xnap_peer_set_assoc_id() which removes the
 * node, sets the real assoc_id, and re-inserts so the tree is then
 * ordered by assoc_id.  All subsequent lookups use assoc_id.
 */
typedef struct xnap_peer_s {
  RB_ENTRY(xnap_peer_s) entry;
  /* unique per-candidate index, set at init */
  uint16_t          cnx_id;
  /* -1 until SCTP association is up */
  sctp_assoc_t      assoc_id;
  xnap_peer_state_t state;
  /* negotiated SCTP in-streams */
  uint16_t          in_streams;
  /* negotiated SCTP out-streams */
  uint16_t          out_streams;
  /* next UE-associated stream to use (cycles 1..out_streams-1) */
  uint16_t          nextstream;
  /* filled after Xn Setup Response */
  uint32_t          remote_gnb_id;
  /* filled after Xn Setup Response */
  xnap_setup_req_t remote_setup_info;
} xnap_peer_t;

/* Per-local-gNB Xn state, indexed by instance number. */
typedef struct xnap_gnb_inst_s {
  instance_t        instance;
  uint32_t          gnb_id;
  /* local gNB's own identity/capabilities */
  xnap_setup_req_t setup_info;
  xnap_net_config_t net_config;
  /* SCTP multi-socket fd from SCTP_INIT_MSG_MULTI_CNF */
  int               multi_sd;
  /* number of candidates inserted into tree */
  uint8_t           nb_peers;
  RB_HEAD(xnap_peer_map, xnap_peer_s) peers;
} xnap_gnb_inst_t;

RB_PROTOTYPE(xnap_peer_map, xnap_peer_s, entry, xnap_peer_compare);

xnap_gnb_inst_t *getCxtXn(instance_t instance);

/* Lookup by real assoc_id (used after SCTP connects). */
xnap_peer_t *getXnPeerByAssoc(xnap_gnb_inst_t *inst, sctp_assoc_t assoc_id);

/* Lookup by cnx_id (used before SCTP connects, e.g. on SCTP_NEW_ASSOCIATION). */
xnap_peer_t *getXnPeerByCnxId(xnap_gnb_inst_t *inst, uint16_t cnx_id);

/* Transition a peer from cnx_id-keyed to assoc_id-keyed:
 * removes from tree, sets assoc_id, re-inserts. */
void xnap_peer_set_assoc_id(xnap_gnb_inst_t *inst, xnap_peer_t *peer, sctp_assoc_t assoc_id);

void createXninst(instance_t instance, xnap_setup_req_t *setup_info, xnap_net_config_t *net_config);

#define XNAP_NON_UE_STREAM_ID 0

#endif /* XNAP_COMMON_H_ */
