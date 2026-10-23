/*
 * SPDX-License-Identifier: Apache-2.0
 * Original file: Copyright 2020 Intel.
 * Copyright 2026 OpenAirInterface Authors
 */

#ifndef _XRAN_PKT_UP_H_
#define _XRAN_PKT_UP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "xran_pkt.h"

#define IQ_PAIR_NUM_IN_RB 12
#define MAX_DATA_SECTIONS_NUM 273
#define MAX_IQ_BIT_WIDTH 16

/* currently library supports I and Q sizes of 8 and 16 bits each */
#define IQ_BITS MAX_IQ_BIT_WIDTH

/*
 * Structure holding data section header fields
 * It is repeated for every section ID in xRAN packet
 */

/**
 ******************************************************************************
 * @ingroup xran_up_pkt
 *
 * @description
 *       Structure holding data section header fields
 *       It is repeated for every section ID in xRAN packet
 *       5.4.5	Coding of Information Elements - Application Layer, Sections
 *       for U-plane as per 6.3.2	DL/UL Data
 *****************************************************************************/
struct data_section_hdr {
  union {
    uint32_t all_bits;
    struct {
      uint32_t num_prbu: 8; /**< 5.4.5.6 number of contiguous PRBs per control section */
      uint32_t start_prbu: 10; /**< 5.4.5.4 starting PRB of control section */
      uint32_t sym_inc: 1; /**< 5.4.5.3 symbol number increment command XRAN_SYMBOLNUMBER_xxxx */
      uint32_t rb: 1; /**< 5.4.5.2 resource block indicator, XRAN_RBIND_xxx */
      uint32_t sect_id: 12; /**< 5.4.5.1 section identifier */
    };
  } fields;
} __rte_packed;

/*
 ******************************************************************************
 * @ingroup xran_up_pkt
 *
 * @description
 *       Structure holds compression header structure and field reserved for future use.
 *       reserved goes always with udCompHdr in u-plane pkt
 *       U-plane as per 6.3.2   DL/UL Data
 *****************************************************************************/
struct data_section_compression_hdr {
  struct compression_hdr ud_comp_hdr;
  uint8_t rsrvd; /**< This parameter provides 1 byte for future definition,
  should be set to all zeros by the sender and ignored by the receiver.
  This field is only present when udCompHdr is present, and is absent when
  the static IQ format and compression method is configured via the M-Plane */

  /* TODO: support for Block Floating Point compression */
  /* udCompMeth  0000b = no compression	absent*/
};

/*
 ******************************************************************************
 * @ingroup xran_up_pkt
 *
 * @description
 *       Structure holds the compression parameters by the compression header.
 *       may not be present by udCompMeth in 6.3.3.13
 *****************************************************************************/
union compression_params {
  struct block_fl_point {
    uint8_t exponent: 4;
    uint8_t reserved: 4;
  } blockFlPoint;
  struct block_scaling {
    uint8_t sblockScaler;
  } blockScaling;
  struct u_law {
    uint8_t compShift: 4;
    uint8_t compBitWidth: 4;
  } uLaw;
} __rte_packed;

/*
 ******************************************************************************
 * @ingroup xran_up_pkt
 *
 * @description
 *       Structure holds an IQ sample pair
 *       U-plane as per 6.3.2   DL/UL Data
 *       Each bit field size is defined with IQ_BITS macro
 *       Currently supported I and Q sizes are 8 and 16 bits
 *****************************************************************************/
struct rb_map {
  int16_t i_sample : IQ_BITS; /**< This parameter is the In-phase sample value */
  int16_t q_sample : IQ_BITS; /**< This parameter is the Quadrature sample value */
} __rte_packed;

/**
 ******************************************************************************
 * @ingroup xran_common_pkt
 *
 * @description
 *       Structure holds complete xran u-plane packet header
 *       3.1.1	Ethernet Encapsulation
 *****************************************************************************/
struct xran_up_pkt_hdr {
  struct xran_ecpri_hdr ecpri_hdr; /**< eCPRI Transport Header */
  struct radio_app_common_hdr app_hdr; /**< eCPRI Transport Header */
  struct data_section_hdr data_sec_hdr;
} __rte_packed;

/**
 ******************************************************************************
 * @ingroup xran_common_pkt
 *
 * @description
 *       Structure holds complete xran u-plane packet header with compression
 *       3.1.1	Ethernet Encapsulation
 *****************************************************************************/
struct xran_up_pkt_hdr_comp {
  struct xran_ecpri_hdr ecpri_hdr; /**< eCPRI Transport Header */
  struct radio_app_common_hdr app_hdr; /**< eCPRI Transport Header */
  struct data_section_hdr data_sec_hdr; /**< data section Header */
  struct data_section_compression_hdr data_comp_hdr; /** Compression Header */
} __rte_packed;

/**
 ******************************************************************************
 * @ingroup xran_common_pkt
 *
 * @description
 *       Structure holds complete ethernet and xran u-plane packet header
 *       3.1.1	Ethernet Encapsulation
 *****************************************************************************/
struct eth_xran_up_pkt_hdr {
  struct rte_ether_hdr eth_hdr;
  struct xran_up_pkt_hdr xran_hdr;
} __rte_packed;

struct eth_xran_up_pkt_hdr_comp {
  struct rte_ether_hdr eth_hdr;
  struct xran_up_pkt_hdr_comp xran_hdr;
} __rte_packed;

#ifdef __cplusplus
}
#endif

#endif
