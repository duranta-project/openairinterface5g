/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef PACKET_FILTER_H_
#define PACKET_FILTER_H_

#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>

/* Packet filter component type identifiers - TS 24.501 Table 9.11.4.13.1 */
#define PF_COMP_MATCH_ALL 0x01 /* Match-all type */
#define PF_COMP_IPV4_REMOTE_ADDR 0x10 /* IPv4 remote address */
#define PF_COMP_IPV4_LOCAL_ADDR 0x11 /* IPv4 local address */
#define PF_COMP_IPV6_REMOTE_ADDR_PREFIX 0x21 /* IPv6 remote address/prefix length */
#define PF_COMP_IPV6_LOCAL_ADDR_PREFIX 0x23 /* IPv6 local address/prefix length */
#define PF_COMP_PROTOCOL_ID_NEXT_HDR 0x30 /* Protocol identifier/Next header type */
#define PF_COMP_SINGLE_LOCAL_PORT 0x40 /* Single local port type */
#define PF_COMP_LOCAL_PORT_RANGE 0x41 /* Local port range type */
#define PF_COMP_SINGLE_REMOTE_PORT 0x50 /* Single remote port type */
#define PF_COMP_REMOTE_PORT_RANGE 0x51 /* Remote port range type */
#define PF_COMP_SECURITY_PARAM_INDEX 0x60 /* Security parameter index type */
#define PF_COMP_TYPE_OF_SERVICE 0x70 /* Type of service/Traffic class type */
#define PF_COMP_FLOW_LABEL 0x80 /* Flow label type */
#define PF_COMP_DESTINATION_MAC_ADDR 0x81 /* Destination MAC address type */
#define PF_COMP_SOURCE_MAC_ADDR 0x82 /* Source MAC address type */
#define PF_COMP_8021Q_CTAG_VID 0x83 /* 802.1Q C-TAG VID type */
#define PF_COMP_8021Q_STAG_VID 0x84 /* 802.1Q S-TAG VID type */
#define PF_COMP_8021Q_CTAG_PCPDEI 0x85 /* 802.1Q C-TAG PCP/DEI type */
#define PF_COMP_8021Q_STAG_PCPDEI 0x86 /* 802.1Q S-TAG PCP/DEI type */
#define PF_COMP_ETHERTYPE 0x87 /* Ethertype type */

/* Packet filter direction - TS 24.501 Table 9.11.4.13.1 */
#define PF_DIR_RESERVED 0b00 /* Reserved */
#define PF_DIR_DOWNLINK 0b01 /* Downlink only */
#define PF_DIR_UPLINK 0b10 /* Uplink only */
#define PF_DIR_BIDIRECTIONAL 0b11 /* Bidirectional */

/* Maximum number of packet filter components per filter */
#define MAX_PF_COMPONENTS 16

/**
 * Packet filter component - decoded representation
 */
typedef struct packet_filter_component_s {
  uint8_t type; /* Component type identifier */
  union {
    struct {
      struct in_addr addr;
      struct in_addr mask;
    } ipv4;
    struct {
      struct in6_addr addr;
      uint8_t prefix_len;
    } ipv6;
    struct {
      uint16_t port_low;
      uint16_t port_high;
    } port_range;
    uint16_t single_port;
    uint8_t protocol;
    uint32_t flow_label;
    uint32_t spi;
    uint8_t tos_traffic_class;
    uint8_t mac_addr[6];
    uint16_t vlan_id;
  } value;
} packet_filter_component_t;

/**
 * Decoded packet filter with all components
 */
typedef struct packet_filter_decoded_s {
  uint8_t pf_id; /* Packet filter identifier */
  uint8_t direction; /* Packet filter direction */
  uint8_t num_components;
  packet_filter_component_t components[MAX_PF_COMPONENTS];
} packet_filter_decoded_t;

/**
 * @brief Decode packet filter contents from buffer
 * @param buf Buffer containing packet filter component types and values
 * @param length Length of packet filter contents
 * @param pf Output: decoded packet filter
 * @return Number of bytes decoded, or -1 on error
 */
int decode_packet_filter_contents(uint8_t *buf, uint8_t length, packet_filter_decoded_t *pf);

/**
 * @brief Match an IP packet against a packet filter
 * @param pf Packet filter to match against
 * @param ip_pkt Pointer to IP packet (IPv4 or IPv6 header)
 * @param pkt_len Length of the packet
 * @return true if packet matches, false otherwise
 */
bool packet_filter_match(const packet_filter_decoded_t *pf, const uint8_t *ip_pkt, size_t pkt_len);

#endif /* PACKET_FILTER_H_ */
