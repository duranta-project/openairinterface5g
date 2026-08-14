/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef __PLATFORM_TYPES_H__
#define __PLATFORM_TYPES_H__

#include <stdint.h>
#include <stdbool.h>

#define ALIGNARRAYSIZE(a, b) (((a + b - 1) / b) * b)
#define ALNARS_16_4(a) ALIGNARRAYSIZE(a, 4)

typedef struct complexd {
  double r;
  double i;
} cd_t;

typedef struct complexf {
  float r;
  float i;
} cf_t;

typedef struct complex8 {
  int8_t r;
  int8_t i;
} c8_t;

typedef struct complex16 {
  int16_t r;
  int16_t i;
} c16_t;

typedef struct complex32 {
  int32_t r;
  int32_t i;
} c32_t;

typedef struct complex64 {
  int64_t r;
  int64_t i;
} c64_t;

//-----------------------------------------------------------------------------
// GENERIC ACCESS STRATUM TYPES
//-----------------------------------------------------------------------------
typedef int32_t sdu_size_t;
typedef uint32_t frame_t;
typedef int32_t sframe_t;
typedef uint32_t sub_frame_t;
typedef uint32_t slot_t;
typedef uint16_t module_id_t;
typedef uint8_t slice_id_t;
typedef uint8_t eNB_index_t;
typedef uint64_t ue_id_t;
typedef long rb_id_t;
typedef long srb_id_t;

typedef bool MBMS_flag_t;
#define MBMS_FLAG_NO false
#define MBMS_FLAG_YES true

typedef bool eNB_flag_t;
#define ENB_FLAG_NO false
#define ENB_FLAG_YES true

typedef bool gNB_flag_t;
#define GNB_FLAG_NO false
#define GNB_FLAG_YES true

typedef bool srb_flag_t;
#define SRB_FLAG_NO false
#define SRB_FLAG_YES true

typedef bool sl_discovery_flag_t;
#define SL_DISCOVERY_FLAG_NO false
#define SL_DISCOVERY_FLAG_YES true

typedef enum {
  TDD = 1,
  FDD = 0
} frame_type_t;

typedef unsigned int crc_t;
typedef sdu_size_t tbs_size_t;
typedef sdu_size_t tb_size_t;
typedef unsigned int logical_chan_id_t;
typedef unsigned int num_tb_t;
typedef uint8_t mac_enb_index_t;

//-----------------------------------------------------------------------------
// RLC TYPES
//-----------------------------------------------------------------------------
typedef unsigned int mui_t;
typedef unsigned int confirm_t;
typedef int32_t rlc_buffer_occupancy_t;
typedef signed int rlc_op_status_t;

typedef enum rlc_mode_e { RLC_MODE_NONE = 0, RLC_MODE_AM = 1, RLC_MODE_UM = 2, RLC_MODE_TM = 4 } rlc_mode_t;

/*! \struct  mac_rlc_status_resp_t
 * \brief Primitive exchanged between RLC and MAC informing about the buffer occupancy of the RLC protocol instance.
 */
typedef struct {
  rlc_buffer_occupancy_t bytes_in_buffer; /*!< \brief Bytes buffered in RLC protocol instance. */
  rlc_buffer_occupancy_t pdus_in_buffer; /*!< \brief Number of PDUs buffered in RLC protocol instance (OBSOLETE). */
  frame_t head_sdu_creation_time; /*!< \brief Head SDU creation time. */
  sdu_size_t head_sdu_remaining_size_to_send; /*!< \brief remaining size of sdu: could be the total size or the remaining size of
                                                 already segmented sdu */
  bool head_sdu_is_segmented; /*!< \brief 0 if head SDU has not been segmented, 1 if already segmented */
} mac_rlc_status_resp_t;

#define SDU_CONFIRM_NO false
#define SDU_CONFIRM_YES true
//-----------------------------------------------------------------------------
// PDCP TYPES
//-----------------------------------------------------------------------------
typedef uint16_t pdcp_sn_t;
typedef uint32_t pdcp_hfn_t;

typedef enum pdcp_transmission_mode_e {
  PDCP_TRANSMISSION_MODE_UNKNOWN = 0,
  PDCP_TRANSMISSION_MODE_CONTROL = 1,
  PDCP_TRANSMISSION_MODE_DATA = 2,
  PDCP_TRANSMISSION_MODE_TRANSPARENT = 3
} pdcp_transmission_mode_t;
//-----------------------------------------------------------------------------
// IP DRIVER / PDCP TYPES
//-----------------------------------------------------------------------------
typedef uint16_t tcp_udp_port_t;
typedef enum ip_traffic_type_e {
  TRAFFIC_IPVX_TYPE_UNKNOWN = 0,
  TRAFFIC_IPV6_TYPE_UNICAST = 1,
  TRAFFIC_IPV6_TYPE_MULTICAST = 2,
  TRAFFIC_IPV6_TYPE_UNKNOWN = 3,
  TRAFFIC_IPV4_TYPE_UNICAST = 5,
  TRAFFIC_IPV4_TYPE_MULTICAST = 6,
  TRAFFIC_IPV4_TYPE_BROADCAST = 7,
  TRAFFIC_IPV4_TYPE_UNKNOWN = 8,
  TRAFFIC_PC5S_SIGNALLING = 9,
  TRAFFIC_PC5S_SESSION_INIT = 10
} ip_traffic_type_t;

typedef enum {
  PDCCH_AGG_LEVEL1 = 0,
  PDCCH_AGG_LEVEL2,
  PDCCH_AGG_LEVEL4,
  PDCCH_AGG_LEVEL8,
  PDCCH_AGG_LEVEL16,
  NUM_PDCCH_AGG_LEVELS
} Pdcch_Aggregation_Level_t;

typedef struct net_ip_address_s {
  unsigned ipv4: 1;
  unsigned ipv6: 1;
  char ipv4_address[16];
  char ipv6_address[46];
} net_ip_address_t;

//-----------------------------------------------------------------------------
// RRC TYPES
//-----------------------------------------------------------------------------
typedef uint32_t mbms_session_id_t;
typedef uint16_t mbms_service_id_t;
typedef uint16_t rnti_t;
typedef uint8_t mme_code_t;
typedef uint32_t m_tmsi_t;

// Random UE identity length = 40 bits
#define NOT_A_RANDOM_UE_IDENTITY (uint64_t)0xFFFFFFFF
#define NOT_A_RNTI (rnti_t)0
#define M_RNTI (rnti_t)0xFFFD
#define P_RNTI (rnti_t)0xFFFE
#define SI_RNTI (rnti_t)0xFFFF
#define CBA_RNTI (rnti_t)0xfff4
typedef enum config_action_e {
  CONFIG_ACTION_NULL = 0,
  CONFIG_ACTION_ADD = 1,
  CONFIG_ACTION_REMOVE = 2,
  CONFIG_ACTION_MODIFY = 3,
  CONFIG_ACTION_SET_SECURITY_MODE = 4,
  CONFIG_ACTION_MBMS_ADD = 10,
  CONFIG_ACTION_MBMS_MODIFY = 11
} config_action_t;

/* Maximum size of any message we might send or receive (e.g., via a socket) */
#define MAX_MESSAGE_SIZE 8192

typedef struct nsa_msg_t {
  uint8_t msg_type;
  uint8_t msg_buffer[MAX_MESSAGE_SIZE];
} nsa_msg_t;

typedef enum nr_lcid_rb_type { NR_LCID_NONE = 0, NR_LCID_SRB = 1, NR_LCID_DRB = 2 } nr_lcid_rb_type;

typedef struct nr_lcid_rb_t {
  nr_lcid_rb_type type;
  union {
    int srb_id;
    int drb_id;
  } choice;
} nr_lcid_rb_t;

typedef struct transport_layer_addr_s {
  /** Transport Layer Address in bytes:
   * - 4 bytes for IPv4 (RFC 791), 16 bytes for IPv6 (RFC 2460),
   * - 20 bytes for both IPv4 and IPv6, with IPv4 in the first 4 bytes. */
  uint8_t length;
  /// Buffer: address in network byte order
  uint8_t buffer[20];
} transport_layer_addr_t;

/** @brief GTP tunnel configuration */
typedef struct {
  // Tunnel endpoint identifier
  uint32_t teid;
  // Transport layer address
  transport_layer_addr_t addr;
} gtpu_tunnel_t;

//-----------------------------------------------------------------------------
// GTPV1U TYPES
//-----------------------------------------------------------------------------
typedef uint32_t teid_t; // tunnel endpoint identifier
typedef uint8_t ebi_t; // eps bearer id

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
// may be ITTI not enabled, but type instance is useful also for OTG,
typedef intptr_t instance_t;

typedef struct protocol_ctxt_s {
  module_id_t module_id; /*!< \brief  Virtualized module identifier      */
  eNB_flag_t enb_flag; /*!< \brief  Flag to indicate eNB (1) or UE (0) */
  instance_t instance; /*!< \brief  ITTI or OTG module identifier      */
  ue_id_t rntiMaybeUEid;
  frame_t frame; /*!< \brief  LTE frame number.*/
  sub_frame_t subframe; /*!< \brief  LTE sub frame number.*/
  eNB_index_t eNB_index; /*!< \brief  valid for UE indicating the index of connected eNB(s)      */
  bool brOption;
} protocol_ctxt_t;

/// suppress compiler warning for unused arguments
#define UNUSED(x) (void)x;

#define UE_MODULE_ID_TO_INSTANCE(mODULE_iD) mODULE_iD + RC.nb_inst
#define ENB_MODULE_ID_TO_INSTANCE(mODULE_iD) mODULE_iD
#define UE_INSTANCE_TO_MODULE_ID(iNSTANCE) iNSTANCE - RC.nb_inst
#define ENB_INSTANCE_TO_MODULE_ID(iNSTANCE) iNSTANCE

// NR
#define GNB_MODULE_ID_TO_INSTANCE(mODULE_iD) mODULE_iD

#define MODULE_ID_TO_INSTANCE(mODULE_iD, iNSTANCE, eNB_fLAG)                                                          \
  do {                                                                                                                \
    iNSTANCE = eNB_fLAG == ENB_FLAG_YES ? ENB_MODULE_ID_TO_INSTANCE(mODULE_iD) : UE_MODULE_ID_TO_INSTANCE(mODULE_iD); \
  } while (0)

#define INSTANCE_TO_MODULE_ID(iNSTANCE, mODULE_iD, eNB_fLAG)                                                         \
  do {                                                                                                               \
    mODULE_iD = eNB_fLAG == ENB_FLAG_YES ? ENB_INSTANCE_TO_MODULE_ID(iNSTANCE) : UE_INSTANCE_TO_MODULE_ID(iNSTANCE); \
  } while (0)

#define PROTOCOL_CTXT_COMPUTE_MODULE_ID(CtXt_h) INSTANCE_TO_MODULE_ID((CtXt_h)->instance, (CtXt_h)->module_id, (CtXt_h)->enb_flag)

#define PROTOCOL_CTXT_COMPUTE_INSTANCE(CtXt_h) MODULE_ID_TO_INSTANCE((CtXt_h)->module_id, (CtXt_h)->instance, (CtXt_h)->enb_flag)

#define PROTOCOL_CTXT_SET_BY_MODULE_ID(Ctxt_Pp, mODULE_iD, eNB_fLAG, rNTI, fRAME, sUBfRAME, eNB_iNDEX) \
  do {                                                                                                 \
    (Ctxt_Pp)->module_id = mODULE_iD;                                                                  \
    (Ctxt_Pp)->enb_flag = eNB_fLAG;                                                                    \
    (Ctxt_Pp)->rntiMaybeUEid = rNTI;                                                                   \
    (Ctxt_Pp)->frame = fRAME;                                                                          \
    (Ctxt_Pp)->subframe = sUBfRAME;                                                                    \
    (Ctxt_Pp)->eNB_index = eNB_iNDEX;                                                                  \
    (Ctxt_Pp)->brOption = false; /* set a default value */                                             \
    PROTOCOL_CTXT_COMPUTE_INSTANCE(Ctxt_Pp);                                                           \
  } while (0)

#define PROTOCOL_CTXT_SET_BY_INSTANCE(Ctxt_Pp, iNSTANCE, eNB_fLAG, rNTI, fRAME, sUBfRAME) \
  do {                                                                                    \
    (Ctxt_Pp)->instance = iNSTANCE;                                                       \
    (Ctxt_Pp)->enb_flag = eNB_fLAG;                                                       \
    (Ctxt_Pp)->rntiMaybeUEid = rNTI;                                                      \
    (Ctxt_Pp)->frame = fRAME;                                                             \
    (Ctxt_Pp)->subframe = sUBfRAME;                                                       \
    (Ctxt_Pp)->eNB_index = 0; /* set a default value */                                   \
    (Ctxt_Pp)->brOption = false; /* set a default value */                                \
    PROTOCOL_CTXT_COMPUTE_MODULE_ID(Ctxt_Pp);                                             \
  } while (0)

#define PROTOCOL_CTXT_FMT "[FRAME %05u][%s][MOD %02d][RNTI %" PRIx64 "]"
#define PROTOCOL_CTXT_ARGS(CTXT_Pp) \
  (CTXT_Pp)->frame, ((CTXT_Pp)->enb_flag == ENB_FLAG_YES) ? "eNB" : " UE", (CTXT_Pp)->module_id, (CTXT_Pp)->rntiMaybeUEid

static inline int ceil_mod(const unsigned int v, const unsigned int mod)
{
  return ((v + mod - 1) / mod) * mod;
}

#ifdef __cplusplus
extern "C" {
#endif
void exit_function(const char *file, const char *function, const int line, const char *s, const int assert);
#ifdef __cplusplus
}
#endif
#endif
