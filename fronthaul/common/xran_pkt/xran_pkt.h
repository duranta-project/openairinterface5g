/*
 * SPDX-License-Identifier: Apache-2.0
 * Original file: Copyright 2020 Intel.
 * Copyright 2026 OpenAirInterface Authors
 */

/* ORAN-WG4.CUS.0-v01.00 O-RAN Fronthaul Working Group
                         Control, User and Synchronization Plane Specification
*/

/*
 * Layer common to data and control packets
 */

#ifndef _XRAN_PKT_H_
#define _XRAN_PKT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_common.h>
#include <rte_ether.h>
#include <rte_byteorder.h>

/**
 *****************************************************************************
 * @file xran_pkt.h
 *
 * @defgroup xran_common_pkt XRAN Packet definitions and functions
 * @ingroup xran
 *
 * @description
 *      Definitions and support functions to process XRAN packet
 *****************************************************************************/

#define ECPRI_MAX_PAYLOAD_SIZE 65535 /**< Max packet size taken in this implementation */

/* XRAN spec: For this encapsulation, either the eCPRI Ethertype or the IEEE 1914.3 Ethertype shall be use */
#define XRAN_ETHER_TYPE 0xAEFE /**< defined by eCPRI Specification V1.1 */

#define XRAN_ECPRI_VER 0x0001 /**< eCPRI protocol revision 3.1.3.1.1 */
#define XRAN_PAYLOAD_VER 0x0001 /**< Payload version 5.4.4.2 */

#define VLAN_ID 0 /**< Default Tag protocol identifier (TPID)*/
#define VLAN_PCP 7 /**< U-Plane and C-Plane only see Table 3 5 : Quality of service classes */

#define XRAN_MTU_DEFAULT RTE_ETHER_MTU
#define XRAN_APP_LAYER_MAX_SIZE_L2_DEFAUT                                                                   \
  (XRAN_MTU_DEFAULT - 8) /**< In case of L2 only solution, application layer maximum transmission unit size \
                              is standard IEEE 802.3 Ethernet frame payload                                 \
                              size (1500 bytes) ? transport overhead (8 bytes) = 1492 bytes (or larger for Jumbo frames) */

#ifndef OK
#define OK 0 /* Function executed correctly */
#endif
#ifndef FAIL
#define FAIL 1 /* Function failed to execute */
#endif
#define NS_PER_SEC 1000000000LL

/**
 ******************************************************************************
 * @ingroup xran_common_pkt
 *
 * @description
 *       eCPRI message types
 *       as per eCPRI spec 3.2.4. Message Types
 *****************************************************************************/
enum ecpri_msg_type {
  ECPRI_IQ_DATA = 0x00, /**< U-plane: IQ data */
  ECPRI_BIT_SEQUENCE = 0x01, /* msg type is not supported */
  ECPRI_RT_CONTROL_DATA = 0x02, /**< C-plane: Control */

  /* Below msg types are not supported */
  ECPRI_GEN_DATA_TRANSFER = 0x03,
  ECPRI_REMOTE_MEM_ACCESS = 0x04,
  ECPRI_DELAY_MEASUREMENT = 0x05,
  ECPRI_REMOTE_RESET = 0x06,
  ECPRI_EVENT_INDICATION = 0x07,
  ECPRI_MSG_TYPE_MAX
};

/**
 ******************************************************************************
 * @ingroup xran_common_pkt
 *
 * @description
 *       eCPRI Timestamp for one-way delay measurements format per IEEE-1588
 *       clause 5.3.3.
 *****************************************************************************/

typedef struct {
  uint16_t secs_msb; // 6 bytes for seconds
  uint32_t secs_lsb;
  uint32_t ns; // 4 bytes for nanoseconds
} TimeStamp;

/**
 ******************************************************************************
 * @ingroup xran_common_pkt
 *
 * @description
 *       eCPRI action types
 *       as per eCPRI spec Table 8 action Types
 *****************************************************************************/
enum ecpri_action_type {
  ECPRI_REQUEST = 0x00, /* Uses Time Stamp T1 and Comp Delay 1 */
  ECPRI_REQUEST_W_FUP = 0x01, /* Uses 0 for Time Stamp and Comp Delay 1 */
  ECPRI_RESPONSE = 0x02, /* Uses Time Stamp T2 and Comp Delay 2 */
  ECPRI_REMOTE_REQ = 0x03, /* Uses 0 for Time Stamp and Comp Delay */
  ECPRI_REMOTE_REQ_W_FUP = 0x04, /* Uses 0 for Time Stamp and Comp Delay */
  ECPRI_FOLLOW_UP = 0x05, /* Uses Time Info and Comp Delay Info */
  ECPRI_ACTION_TYPE_MAX
};

/**
 ******************************************************************************
 * @ingroup xran_common_pkt
 *
 * @description
 *       see 3.1.3.1.7 ecpriSeqid (message identifier)
 *****************************************************************************/
union ecpri_seq_id {
  struct {
    uint8_t seq_id: 8; /**< Sequence ID */
    uint8_t sub_seq_id: 7; /**< Subsequence ID */
    uint8_t e_bit: 1; /**< E bit */
  } bits;
  struct {
    uint16_t data_num_1;
  } data;
} __rte_packed;

#define ecpri_seq_id_bitfield_seq_id 0
#define ecpri_seq_id_bitfield_sub_seq_id 8
#define ecpri_seq_id_bitfield_e_bit 15

/**
 ******************************************************************************
 * @ingroup xran_common_pkt
 *
 * @description
 *       Structure holds common eCPRI header as per
 *       Table 3 1 : eCPRI Transport Header Field Definitions
 *****************************************************************************/
union xran_ecpri_cmn_hdr {
  struct {
    uint8_t ecpri_concat: 1; /**< 3.1.3.1.3 eCPRI concatenation indicator */
    uint8_t ecpri_resv: 3; /**< 3.1.3.1.2 eCPRI reserved */
    uint8_t ecpri_ver: 4; /**< 3.1.3.1.1 eCPRI protocol revision, defined in XRAN_ECPRI_VER */
    uint8_t ecpri_mesg_type; /**< 3.1.3.1.4 eCPRI message type, defined in ecpri_msg_type */
    uint16_t ecpri_payl_size; /**< 3.1.3.1.5 eCPRI payload size, without common header and any padding bytes */
  } bits;
  struct {
    uint32_t data_num_1;
  } data;
} __rte_packed;

#define xran_ecpri_cmn_hdr_bitfield_EcpriVer 4
#define xran_ecpri_cmn_hdr_bitfield_EcpriMsgType 8
/**
 ******************************************************************************
 * @ingroup xran_common_pkt
 *
 * @description
 *       Structure holds common eCPRI delay measuurement header as per
 *       Table 2.17 : eCPRI One-Way delay measurement message
 *****************************************************************************/
struct xran_ecpri_delay_meas_pl {
  uint8_t MeasurementID; /**< Table 2-17 Octet 5     */
  uint8_t ActionType; /**< Table 2-17 Octet 6     */
  TimeStamp ts; /**< Table 2-17 Octet 7-16  */
  int64_t CompensationValue; /**< Table 2-17 Octet 17    */
  uint8_t DummyBytes[1400]; /**< Table 2-17 Octet 25    */
} /*__rte_packed*/;

/**
 ******************************************************************************
 * @ingroup xran_common_pkt
 *
 * @description
 *       Structure holds common eCPRI cmn header per eCPRI figure 8 and the measurement delay header and pl per
 *       eCPRI Figure 23  : eCPRI One-Way delay measurement message
 *****************************************************************************/
struct xran_ecpri_del_meas_pkt {
  union xran_ecpri_cmn_hdr cmnhdr;
  struct xran_ecpri_delay_meas_pl deMeasPl;
} /*__rte_packed*/;

/**
 ******************************************************************************
 * @ingroup xran_common_pkt
 *
 * @description
 *       Structure holds eCPRI transport header as per
 *       Table 3 1 : eCPRI Transport Header Field Definitions
 *****************************************************************************/
struct xran_ecpri_hdr {
  union xran_ecpri_cmn_hdr cmnhdr;
  rte_be16_t ecpri_xtc_id; /**< 3.1.3.1.6 real time control data / IQ data transfer message series identifier */
  union ecpri_seq_id ecpri_seq_id; /**< 3.1.3.1.7 message identifier */
} __rte_packed;

/**
 ******************************************************************************
 * @ingroup xran_common_pkt
 *
 * @description
 *      Enum used to set xRAN packet data direction (gNB Tx/Rx 5.4.4.1)
 *      uplink or downlink
 *****************************************************************************/
enum xran_pkt_dir {
  XRAN_DIR_UL = 0, /**< UL direction */
  XRAN_DIR_DL = 1, /**< DL direction */
  XRAN_DIR_MAX
};

/**
 ******************************************************************************
 * @ingroup xran_common_pkt
 *
 * @description
 *       Structure holds components of radio application header
 *       5.4.4 Coding of Information Elements - Application Layer, Common
 *       for U-plane as per 6.3.2	DL/UL Data
 *****************************************************************************/
struct radio_app_common_hdr {
  /* Octet 9 */
  union {
    uint8_t value;
    struct {
      uint8_t filter_id: 4; /**< This parameter defines an index to the channel filter to be
                                  used between IQ data and air interface, both in DL and UL.
                                  For most physical channels filterIndex =0000b is used which
                                  indexes the standard channel filter, e.g. 100MHz channel filter
                                  for 100MHz nominal carrier bandwidth. (see 5.4.4.3 for more) */
      uint8_t payl_ver: 3; /**< This parameter defines the payload protocol version valid
                                for the following IEs in the application layer. In this version of
                                the specification payloadVersion=001b shall be used. */
      uint8_t data_direction: 1; /**< This parameter indicates the gNB data direction. */
    };
  } data_feature;

  /* Octet 10 */
  uint8_t frame_id: 8; /**< This parameter is a counter for 10 ms frames (wrapping period 2.56 seconds) */

  /* Octet 11 */
  /* Octet 12 */
  union {
    uint16_t value;
    struct {
      uint16_t symb_id: 6; /**< This parameter identifies the first symbol number within slot,
                                      to which the information of this message is applies. */
      uint16_t slot_id: 6; /**< This parameter is the slot number within a 1ms sub-frame. All slots in
                               one sub-frame are counted by this parameter, slotId running from 0 to Nslot-1.
                               In this version of the specification the maximum Nslot=16, All
                               other values of the 6 bits are reserved for future use. */
      uint16_t subframe_id: 4; /**< This parameter is a counter for 1 ms sub-frames within 10ms frame. */
    };
  } sf_slot_sym;

} __rte_packed;

/**
 ******************************************************************************
 * @ingroup xran_common_pkt
 *
 * @description
 *      This parameter defines the compression method and IQ bit width for the
 *      user data in the data section.  This field is absent from U-Plane messages
 *      when the static IQ format and compression method is configured via the M-Plane.
 *      In this way a single compression method and IQ bit width is provided
 *     (per UL and DL, per LTE and NR) without adding more overhead to U-Plane messages.
 *****************************************************************************/
struct compression_hdr {
  uint8_t ud_comp_meth: 4;
  /**< udCompMeth|  compression method         |udIqWidth meaning
  ---------------+-----------------------------+--------------------------------------------
  0000b          | no compression              |bitwidth of each uncompressed I and Q value
  0001b          | block floating point        |bitwidth of each I and Q mantissa value
  0010b          | block scaling               |bitwidth of each I and Q scaled value
  0011b          | mu-law                      |bitwidth of each compressed I and Q value
  0100b          | modulation compression      |bitwidth of each compressed I and Q value
  0100b - 1111b  | reserved for future methods |depends on the specific compression method
  */
  uint8_t ud_iq_width: 4; /**< Bit width of each I and each Q
                               16 for udIqWidth=0, otherwise equals udIqWidth e.g. udIqWidth = 0000b means I and Q are each 16 bits
                             wide; e.g. udIQWidth = 0001b means I and Q are each 1 bit wide; e.g. udIqWidth = 1111b means I and Q
                             are each 15 bits wide
                               */
} __rte_packed;

/**
 ******************************************************************************
 * @ingroup xran_common_pkt
 *
 * @description
 *       Structure holds common xran packet header
 *       3.1.1	Ethernet Encapsulation
 *****************************************************************************/
struct xran_pkt_comm_hdr {
  struct rte_ether_hdr eth_hdr; /**< Ethernet Header */
  struct xran_ecpri_hdr ecpri_hdr; /**< eCPRI Transport Header */
} __rte_packed;

enum xran_mbuf_mem_op_id { MBUF_KEEP, MBUF_FREE };

#ifdef __cplusplus
}
#endif

#endif
