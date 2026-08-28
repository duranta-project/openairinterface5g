/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef __GTPUNEW_ITF_H__
#define __GTPUNEW_ITF_H__

#include <stdint.h>
#include <limits.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define GTPNOK -1

# define GTPU_HEADER_OVERHEAD_MAX 64

typedef struct {
  /* TX counters (outgoing GTP-U G-PDUs) */
  uint64_t tx_pkts;           /* packets successfully sent */
  uint64_t tx_bytes;          /* payload bytes successfully sent */
  uint64_t tx_drop_no_tunnel; /* dropped: no matching UE/bearer found */
  uint64_t tx_drop_send_fail; /* dropped: sendmsg() returned an error */

  /* RX counters (incoming GTP-U G-PDUs from UDP socket) */
  uint64_t rx_pkts;           /* G-PDUs delivered to lower layer */
  uint64_t rx_bytes;          /* payload bytes delivered */
  uint64_t rx_drop_malformed; /* dropped: bad GTP header or length */
  uint64_t rx_drop_unknown_teid; /* dropped: TEID not in te2ue_mapping */
  uint64_t rx_drop_refused;   /* dropped: lower-layer callback returned false */
} gtpu_stats_t;

#include "common/platform_types.h"
#ifdef __cplusplus
extern "C" {
#endif

/* forward declaration */
struct protocol_ctxt_s;
typedef struct protocol_ctxt_s protocol_ctxt_t;
struct gtpv1u_enb_create_tunnel_req_s;
typedef struct gtpv1u_enb_create_tunnel_req_s gtpv1u_enb_create_tunnel_req_t;
struct gtpv1u_enb_create_tunnel_resp_s;
typedef struct gtpv1u_enb_create_tunnel_resp_s gtpv1u_enb_create_tunnel_resp_t;
struct gtpv1u_enb_delete_tunnel_req_s;
typedef struct gtpv1u_enb_delete_tunnel_req_s gtpv1u_enb_delete_tunnel_req_t;
struct gtpv1u_enb_create_x2u_tunnel_req_s;
typedef struct gtpv1u_enb_create_x2u_tunnel_req_s gtpv1u_enb_create_x2u_tunnel_req_t;
struct gtpv1u_enb_create_x2u_tunnel_resp_s;
typedef struct gtpv1u_enb_create_x2u_tunnel_resp_s gtpv1u_enb_create_x2u_tunnel_resp_t;
struct gtpv1u_gnb_create_tunnel_req_s;
typedef struct gtpv1u_gnb_create_tunnel_req_s gtpv1u_gnb_create_tunnel_req_t;
struct gtpv1u_gnb_create_tunnel_resp_s;
typedef struct gtpv1u_gnb_create_tunnel_resp_s gtpv1u_gnb_create_tunnel_resp_t;
struct gtpv1u_gnb_delete_tunnel_req_s;
typedef struct gtpv1u_gnb_delete_tunnel_req_s gtpv1u_gnb_delete_tunnel_req_t;

  typedef bool (*gtpCallback)(protocol_ctxt_t  *ctxt_pP,
                              const srb_flag_t     srb_flagP,
                              const rb_id_t        rb_idP,
                              const mui_t          muiP,
                              const confirm_t      confirmP,
                              const sdu_size_t     sdu_buffer_sizeP,
                              unsigned char *const sdu_buffer_pP,
                              const pdcp_transmission_mode_t modeP,
                              const uint32_t *sourceL2Id,
                              const uint32_t *destinationL2Id);

  typedef bool (*gtpCallbackSDAP)(protocol_ctxt_t  *ctxt_pP,
                                  const ue_id_t        ue_id,
                                  const srb_flag_t     srb_flagP,
                                  const mui_t          muiP,
                                  const confirm_t      confirmP,
                                  const sdu_size_t     sdu_buffer_sizeP,
                                  unsigned char *const sdu_buffer_pP,
                                  const pdcp_transmission_mode_t modeP,
                                  const uint32_t *sourceL2Id,
                                  const uint32_t *destinationL2Id,
                                  const uint8_t   qfi,
                                  const bool      rqi,
                                  const int       pdusession_id);

  typedef struct openAddr_s {
    char originHost[HOST_NAME_MAX];
    char originService[HOST_NAME_MAX];
    char destinationHost[HOST_NAME_MAX];
    char destinationService[HOST_NAME_MAX];
    instance_t originInstance;
  } openAddr_t;

  typedef struct extensionHeader_s{
    uint8_t buffer[500];
    uint8_t length;
  }extensionHeader_t;

  // the init function create a gtp instance and return the gtp instance id
  // the parameter originInstance will be sent back in each message from gtp to the creator
  void gtpv1uProcessTimeout(int handle,void *arg);
  int gtpv1u_create_s1u_tunnel(const instance_t instance,
                               const gtpv1u_enb_create_tunnel_req_t *create_tunnel_req,
                               gtpv1u_enb_create_tunnel_resp_t *create_tunnel_resp,
                               gtpCallback callBack);
  int gtpv1u_update_s1u_tunnel(const instance_t instanceP,
                               const gtpv1u_enb_create_tunnel_req_t   *create_tunnel_req_pP,
                               const rnti_t prior_rnti
                               );

  int gtpv1u_delete_s1u_tunnel( const instance_t instance, const gtpv1u_enb_delete_tunnel_req_t *const req_pP);
  int gtpv1u_delete_all_s1u_tunnel(const instance_t instance, const rnti_t rnti);

  int gtpv1u_create_x2u_tunnel(const instance_t instanceP,
                               const gtpv1u_enb_create_x2u_tunnel_req_t   *const create_tunnel_req_pP,
                               gtpv1u_enb_create_x2u_tunnel_resp_t *const create_tunnel_resp_pP);

  int gtpv1u_delete_x2u_tunnel( const instance_t instanceP,
                                const gtpv1u_enb_delete_tunnel_req_t *const req_pP);
  int gtpv1u_create_ngu_tunnel(const instance_t instanceP,
                               const gtpv1u_gnb_create_tunnel_req_t *const create_tunnel_req_pP,
                               gtpv1u_gnb_create_tunnel_resp_t *const create_tunnel_resp_pP,
                               gtpCallback callBack,
                               gtpCallbackSDAP callBackSDAP);

  int gtpv1u_update_ue_id(const instance_t instanceP, ue_id_t old_ue_id, ue_id_t new_ue_id);

  // New API
  teid_t newGtpuCreateTunnel(instance_t instance,
                             ue_id_t ue_id,
                             int incoming_bearer_id,
                             int outgoing_bearer_id,
                             teid_t outgoing_teid,
                             transport_layer_addr_t remoteAddr,
                             gtpCallback callBack,
                             gtpCallbackSDAP callBackSDAP);

  void GtpuUpdateTunnelOutgoingAddressAndTeid(instance_t instance,
                                    ue_id_t ue_id,
                                    ebi_t bearer_id,
                                              in_addr_t newOutgoingAddr,
                                              teid_t newOutgoingTeid);

  int newGtpuDeleteOneTunnel(instance_t instance, ue_id_t ue_id, int rb_id);
  int newGtpuDeleteAllTunnels(instance_t instance, ue_id_t ue_id);

  void gtpv1uSendDirect(instance_t instance, ue_id_t ue_id, int bearer_id, uint8_t *buf, size_t len, bool seqNumFlag, bool npduNumFlag);
  void gtpv1uSendDirectWithQFI(instance_t instance, ue_id_t ue_id, int bearer_id, int qfi, uint8_t *buf, size_t len);

  void gtpv1uSendDirectWithNRUSeqNum(instance_t instance,
                                     ue_id_t ue_id,
                                     int bearer_id,
                                     uint8_t *buf,
                                     size_t len);

  instance_t gtpv1Init(openAddr_t context);
  int gtpv1Term(instance_t inst);
  void *gtpv1uTask(void *args);
  bool gtpu_get_stats(instance_t instance, gtpu_stats_t *out);
  void gtpu_log_stats(instance_t instance);

#ifdef __cplusplus
}
#endif
#endif
