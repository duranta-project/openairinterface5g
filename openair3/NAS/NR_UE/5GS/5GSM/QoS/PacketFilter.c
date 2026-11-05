/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "PacketFilter.h"
#include "common/utils/LOG/log.h"
#include "fgs_nas_utils.h"
#include "openair3/UTILS/conversions.h"
#include <string.h>
#include <arpa/inet.h>
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
