/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "PacketFilter.h"
#include "common/utils/LOG/log.h"
#include "fgs_nas_utils.h"
#include "openair3/UTILS/conversions.h"
#include <string.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/in.h>

/**
 * @brief Get protocol name for logging
 */
static const char *get_protocol_name(uint8_t protocol)
{
  switch (protocol) {
    case IPPROTO_ICMP:
      return "ICMP";
    case IPPROTO_TCP:
      return "TCP";
    case IPPROTO_UDP:
      return "UDP";
    case IPPROTO_ICMPV6:
      return "ICMPv6";
    case IPPROTO_SCTP:
      return "SCTP";
    default:
      return "Unknown";
  }
}

/**
 * @brief Decode packet filter contents from buffer
 */
int decode_packet_filter_contents(uint8_t *buf, uint8_t length, packet_filter_decoded_t *pf)
{
  uint8_t *start = buf;
  uint8_t *end = buf + length;
  pf->num_components = 0;
  const char *dir_str = (pf->direction == PF_DIR_UPLINK) ? "UL" : (pf->direction == PF_DIR_DOWNLINK) ? "DL" : "BIDIR";

  while (buf < end && pf->num_components < MAX_PF_COMPONENTS) {
    packet_filter_component_t *comp = &pf->components[pf->num_components];
    comp->type = *buf++;

    switch (comp->type) {
      case PF_COMP_MATCH_ALL:
        // Match-all type: no value field follows the type octet
        LOG_D(NAS, "Packet Filter %d direction %s component: Match-all\n", pf->pf_id, dir_str);
        break;

      case PF_COMP_IPV4_REMOTE_ADDR:
      case PF_COMP_IPV4_LOCAL_ADDR:
        if (buf + 8 > end) {
          LOG_E(NAS, "Packet filter: IPv4 address component truncated\n");
          return -1;
        }
        memcpy(&comp->value.ipv4.addr, buf, 4);
        buf += 4;
        memcpy(&comp->value.ipv4.mask, buf, 4);
        buf += 4;
        char addr_str[INET_ADDRSTRLEN], mask_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &comp->value.ipv4.addr, addr_str, sizeof(addr_str));
        inet_ntop(AF_INET, &comp->value.ipv4.mask, mask_str, sizeof(mask_str));
        LOG_D(NAS,
              "Packet Filter %d direction %s component: IPv4 %s %s/%s\n",
              pf->pf_id,
              dir_str,
              comp->type == PF_COMP_IPV4_REMOTE_ADDR ? "remote" : "local",
              addr_str,
              mask_str);
        break;

      case PF_COMP_IPV6_REMOTE_ADDR_PREFIX:
      case PF_COMP_IPV6_LOCAL_ADDR_PREFIX:
        if (buf + 17 > end) {
          LOG_E(NAS, "Packet filter: IPv6 address component truncated\n");
          return -1;
        }
        memcpy(&comp->value.ipv6.addr, buf, 16);
        buf += 16;
        comp->value.ipv6.prefix_len = *buf++;
        char ipv6_str[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &comp->value.ipv6.addr, ipv6_str, sizeof(ipv6_str));
        LOG_D(NAS,
              "Packet Filter %d direction %s component: IPv6 %s %s/%d\n",
              pf->pf_id,
              dir_str,
              comp->type == PF_COMP_IPV6_REMOTE_ADDR_PREFIX ? "remote" : "local",
              ipv6_str,
              comp->value.ipv6.prefix_len);
        break;

      case PF_COMP_PROTOCOL_ID_NEXT_HDR:
        if (buf + 1 > end) {
          LOG_E(NAS, "Packet filter: Protocol ID component truncated\n");
          return -1;
        }
        comp->value.protocol = *buf++;
        LOG_D(NAS,
              "Packet Filter %d direction %s component: Protocol %d (%s)\n",
              pf->pf_id,
              dir_str,
              comp->value.protocol,
              get_protocol_name(comp->value.protocol));
        break;

      case PF_COMP_SINGLE_LOCAL_PORT:
      case PF_COMP_SINGLE_REMOTE_PORT:
        if (buf + 2 > end) {
          LOG_E(NAS, "Packet filter: Single port component truncated\n");
          return -1;
        }
        GET_SHORT(buf, comp->value.single_port);
        buf += 2;
        LOG_D(NAS,
              "Packet Filter %d direction %s component: %s port %d\n",
              pf->pf_id,
              dir_str,
              comp->type == PF_COMP_SINGLE_LOCAL_PORT ? "Local" : "Remote",
              comp->value.single_port);
        break;

      case PF_COMP_LOCAL_PORT_RANGE:
      case PF_COMP_REMOTE_PORT_RANGE:
        if (buf + 4 > end) {
          LOG_E(NAS, "Packet filter: Port range component truncated\n");
          return -1;
        }
        GET_SHORT(buf, comp->value.port_range.port_low);
        buf += 2;
        GET_SHORT(buf, comp->value.port_range.port_high);
        buf += 2;
        LOG_D(NAS,
              "Packet Filter %d direction %s component: %s port range %d-%d\n",
              pf->pf_id,
              dir_str,
              comp->type == PF_COMP_LOCAL_PORT_RANGE ? "Local" : "Remote",
              comp->value.port_range.port_low,
              comp->value.port_range.port_high);
        break;

      case PF_COMP_SECURITY_PARAM_INDEX:
        if (buf + 4 > end) {
          LOG_E(NAS, "Packet filter: SPI component truncated\n");
          return -1;
        }
        BUFFER_TO_UINT32(buf, comp->value.spi);
        buf += 4;
        LOG_D(NAS, "Packet Filter %d direction %s component: SPI 0x%08x\n", pf->pf_id, dir_str, comp->value.spi);
        break;

      case PF_COMP_TYPE_OF_SERVICE:
        if (buf + 2 > end) {
          LOG_E(NAS, "Packet filter: ToS component truncated\n");
          return -1;
        }
        comp->value.tos_traffic_class = *buf++;
        buf++; // skip mask
        LOG_D(NAS, "Packet Filter %d direction %s component: ToS/TC 0x%02x\n", pf->pf_id, dir_str, comp->value.tos_traffic_class);
        break;

      case PF_COMP_FLOW_LABEL:
        if (buf + 4 > end) {
          LOG_E(NAS, "Packet filter: Flow label component truncated\n");
          return -1;
        }
        BUFFER_TO_UINT32(buf, comp->value.flow_label);
        comp->value.flow_label &= 0x000FFFFF; // 20 bits
        buf += 4;
        LOG_D(NAS, "Packet Filter %d direction %s component: Flow label 0x%05x\n", pf->pf_id, dir_str, comp->value.flow_label);
        break;

      case PF_COMP_DESTINATION_MAC_ADDR:
      case PF_COMP_SOURCE_MAC_ADDR:
        if (buf + 6 > end) {
          LOG_E(NAS, "Packet filter: MAC address component truncated\n");
          return -1;
        }
        memcpy(comp->value.mac_addr, buf, 6);
        buf += 6;
        LOG_D(NAS,
              "Packet Filter %d direction %s component: %s MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
              pf->pf_id,
              dir_str,
              comp->type == PF_COMP_DESTINATION_MAC_ADDR ? "Dst" : "Src",
              comp->value.mac_addr[0],
              comp->value.mac_addr[1],
              comp->value.mac_addr[2],
              comp->value.mac_addr[3],
              comp->value.mac_addr[4],
              comp->value.mac_addr[5]);
        break;

      case PF_COMP_8021Q_CTAG_VID:
      case PF_COMP_8021Q_STAG_VID:
        if (buf + 2 > end) {
          LOG_E(NAS, "Packet filter: VLAN ID component truncated\n");
          return -1;
        }
        GET_SHORT(buf, comp->value.vlan_id);
        buf += 2;
        comp->value.vlan_id &= 0x0FFF; // 12 bits
        LOG_D(NAS,
              "Packet Filter %d direction %s component: VLAN %s-TAG VID %d\n",
              pf->pf_id,
              dir_str,
              comp->type == PF_COMP_8021Q_CTAG_VID ? "C" : "S",
              comp->value.vlan_id);
        break;

      case PF_COMP_ETHERTYPE:
        if (buf + 2 > end) {
          LOG_E(NAS, "Packet filter: Ethertype component truncated\n");
          return -1;
        }
        buf += 2; // skip for now
        break;

      default:
        LOG_W(NAS, "Packet filter: Unknown component type 0x%02x, skipping\n", comp->type);
        return -1;
    }

    pf->num_components++;
  }

  return buf - start;
}

/**
 * @brief Check if IPv4 address matches filter with mask
 */
static bool ipv4_match(struct in_addr pkt_addr, struct in_addr filter_addr, struct in_addr mask)
{
  return (pkt_addr.s_addr & mask.s_addr) == (filter_addr.s_addr & mask.s_addr);
}

/**
 * @brief Check if IPv6 address matches filter with prefix length
 */
static bool ipv6_match(struct in6_addr *pkt_addr, const struct in6_addr *filter_addr, uint8_t prefix_len)
{
  uint8_t bytes = prefix_len / 8;
  uint8_t bits = prefix_len % 8;

  if (memcmp(pkt_addr, filter_addr, bytes) != 0)
    return false;

  if (bits > 0) {
    uint8_t mask = 0xFF << (8 - bits);
    if ((pkt_addr->s6_addr[bytes] & mask) != (filter_addr->s6_addr[bytes] & mask))
      return false;
  }

  return true;
}

/**
 * @brief Match an IP packet against a packet filter
 */
bool packet_filter_match(const packet_filter_decoded_t *pf, const uint8_t *ip_pkt, size_t pkt_len)
{
  if (pkt_len < 20)
    return false;

  uint8_t ip_version = (ip_pkt[0] >> 4) & 0x0F;
  struct iphdr ip4_hdr;
  struct ip6_hdr ip6_hdr;
  struct iphdr *ip4 = NULL;
  struct ip6_hdr *ip6 = NULL;
  uint8_t protocol = 0;
  uint16_t src_port = 0, dst_port = 0;
  const uint8_t *transport_hdr = NULL;

  if (ip_version == 4) {
    uint8_t ihl = ip_pkt[0] & 0x0F;
    size_t header_len = (size_t)ihl * 4;
    if (ihl < 5 || header_len > pkt_len)
      return false;

    memcpy(&ip4_hdr, ip_pkt, sizeof(ip4_hdr));
    ip4 = &ip4_hdr;
    protocol = ip4->protocol;
    transport_hdr = ip_pkt + header_len;
  } else if (ip_version == 6) {
    if (pkt_len < sizeof(ip6_hdr))
      return false;

    memcpy(&ip6_hdr, ip_pkt, sizeof(ip6_hdr));
    ip6 = &ip6_hdr;
    protocol = ip6->ip6_nxt;
    transport_hdr = ip_pkt + sizeof(ip6_hdr);
  } else {
    return false;
  }

  // Extract ports for TCP/UDP
  if ((protocol == IPPROTO_TCP || protocol == IPPROTO_UDP) && transport_hdr + 4 <= ip_pkt + pkt_len) {
    uint16_t src_port_be, dst_port_be;
    memcpy(&src_port_be, transport_hdr, sizeof(src_port_be));
    memcpy(&dst_port_be, transport_hdr + 2, sizeof(dst_port_be));
    src_port = ntohs(src_port_be);
    dst_port = ntohs(dst_port_be);
  }

  // A bidirectional filter is also treated as uplink
  const bool ul = (pf->direction == PF_DIR_UPLINK || pf->direction == PF_DIR_BIDIRECTIONAL);

  // Match all components
  for (int i = 0; i < pf->num_components; i++) {
    const packet_filter_component_t *comp = &pf->components[i];
    bool match = false;

    switch (comp->type) {
      case PF_COMP_MATCH_ALL:
        match = true;
        break;

      case PF_COMP_IPV4_REMOTE_ADDR:
        if (ip4 && ul)
          match = ipv4_match(*(struct in_addr *)&ip4->daddr, comp->value.ipv4.addr, comp->value.ipv4.mask);
        else if (ip4)
          match = ipv4_match(*(struct in_addr *)&ip4->saddr, comp->value.ipv4.addr, comp->value.ipv4.mask);
        break;

      case PF_COMP_IPV4_LOCAL_ADDR:
        if (ip4 && ul)
          match = ipv4_match(*(struct in_addr *)&ip4->saddr, comp->value.ipv4.addr, comp->value.ipv4.mask);
        else if (ip4)
          match = ipv4_match(*(struct in_addr *)&ip4->daddr, comp->value.ipv4.addr, comp->value.ipv4.mask);
        break;

      case PF_COMP_IPV6_REMOTE_ADDR_PREFIX:
        if (ip6 && ul)
          match = ipv6_match(&ip6->ip6_dst, &comp->value.ipv6.addr, comp->value.ipv6.prefix_len);
        else if (ip6)
          match = ipv6_match(&ip6->ip6_src, &comp->value.ipv6.addr, comp->value.ipv6.prefix_len);
        break;

      case PF_COMP_IPV6_LOCAL_ADDR_PREFIX:
        if (ip6 && ul)
          match = ipv6_match(&ip6->ip6_src, &comp->value.ipv6.addr, comp->value.ipv6.prefix_len);
        else if (ip6)
          match = ipv6_match(&ip6->ip6_dst, &comp->value.ipv6.addr, comp->value.ipv6.prefix_len);
        break;

      case PF_COMP_PROTOCOL_ID_NEXT_HDR:
        match = (protocol == comp->value.protocol);
        break;

      case PF_COMP_SINGLE_REMOTE_PORT:
        match = ul ? (dst_port == comp->value.single_port) : (src_port == comp->value.single_port);
        break;

      case PF_COMP_SINGLE_LOCAL_PORT:
        match = ul ? (src_port == comp->value.single_port) : (dst_port == comp->value.single_port);
        break;

      case PF_COMP_REMOTE_PORT_RANGE:
        match = ul ? (dst_port >= comp->value.port_range.port_low && dst_port <= comp->value.port_range.port_high)
                   : (src_port >= comp->value.port_range.port_low && src_port <= comp->value.port_range.port_high);
        break;

      case PF_COMP_LOCAL_PORT_RANGE:
        match = ul ? (src_port >= comp->value.port_range.port_low && src_port <= comp->value.port_range.port_high)
                   : (dst_port >= comp->value.port_range.port_low && dst_port <= comp->value.port_range.port_high);
        break;

      default:
        LOG_W(NAS, "Packet Filter %d: matching failed for component type 0x%02x\n", pf->pf_id, comp->type);
        break;
    }

    if (!match)
      return false; // All components must match
  }

  return true;
}
