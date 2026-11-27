/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <string.h>
#include "common/config/config_paramdesc.h"
#include "common/utils/LOG/log.h"
#include "openair2/SDAP/nr_sdap/nr_sdap_entity.h"

configmodule_interface_t *uniqCfg = NULL;
static nr_sdap_entity_t entity;

// Isolate the rule table from RRC/PDCP entity creation; use the production matcher and installer.
nr_sdap_entity_t *nr_sdap_get_entity(ue_id_t ue_id, int pdusession_id)
{
  return ue_id == 0 && pdusession_id == 1 ? &entity : NULL;
}

static uint8_t match_port(uint16_t port)
{
  uint8_t packet[sizeof(struct iphdr) + sizeof(struct udphdr)] = {0};
  struct iphdr ip = {.version = 4, .ihl = 5, .protocol = IPPROTO_UDP};
  struct udphdr udp = {.source = htons(port), .dest = htons(53000)};
  memcpy(packet, &ip, sizeof(ip));
  memcpy(packet + sizeof(ip), &udp, sizeof(udp));
  return nr_sdap_match_ul_packet(&entity, packet, sizeof(packet));
}

int main(void)
{
  logInit();
  nr_sdap_qos_rules_init(&entity);
  packet_filter_decoded_t fallback = {.pf_id = 1, .direction = PF_DIR_UPLINK, .num_components = 1};
  fallback.components[0].type = PF_COMP_MATCH_ALL;
  packet_filter_decoded_t dedicated = {.pf_id = 1, .direction = PF_DIR_BIDIRECTIONAL, .num_components = 1};
  dedicated.components[0].type = PF_COMP_SINGLE_LOCAL_PORT;
  dedicated.components[0].value.single_port = 52080;
  AssertFatal(nr_sdap_qos_rule_add(0, 1, 3, 9, 10, true, &fallback, 1), "Default installation failed\n");
  AssertFatal(nr_sdap_qos_rule_add(0, 1, 7, 5, 7, false, &dedicated, 1), "Dedicated installation failed\n");
  AssertFatal(match_port(52080) == 5 && match_port(52082) == 9, "Initial rules did not classify packets\n");

  // Replacing an ID must remove its previous filter, keep one entry and preserve the default.
  dedicated.components[0].value.single_port = 52082;
  AssertFatal(nr_sdap_qos_rule_add(0, 1, 7, 6, 8, false, &dedicated, 1), "Replacement failed\n");
  AssertFatal(seq_arr_size(&entity.qos_rules) == 2 && entity.qfi == 9 && entity.use_packet_filters,
              "Replacement changed the rule count or default\n");
  AssertFatal(match_port(52080) == 9 && match_port(52082) == 6, "Old rule filter remained after replacement\n");
  AssertFatal(!nr_sdap_qos_rule_add(0, 1, 3, 6, 8, false, &dedicated, 1),
              "Existing default ID collision accepted\n");
  AssertFatal(!nr_sdap_qos_rule_add(0, 1, 7, 6, 8, true, &dedicated, 1),
              "New default ID collision accepted\n");
  AssertFatal(match_port(52082) == 6 && entity.qfi == 9, "Rejected collision changed installed rules\n");
  nr_sdap_qos_rule_remove(0, 1, 7);
  AssertFatal(match_port(52082) == 9, "Default classification lost after dedicated deletion\n");
  nr_sdap_qos_rule_remove(0, 1, 3);
  AssertFatal(!entity.use_packet_filters && seq_arr_size(&entity.qos_rules) == 0, "Rule deletion left stale state\n");
  nr_sdap_qos_rules_free(&entity);
  return 0;
}
