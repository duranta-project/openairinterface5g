/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "oru_io.h"
#include "oru_pcap.h"
#include <rte_ether.h>
#include <stdio.h>
#include <stdlib.h>
#include <rte_ethdev.h>
#include <rte_flow.h>
#include <rte_errno.h>
#include "assertions.h"
#include <rte_version.h>

static int configure_ru_flows(oru_io_t *io, uint16_t port_id, int num_du_macs, struct rte_ether_addr *du_macs)
{
  AssertFatal(num_du_macs > 0 && num_du_macs <= 2,
              "Invalid number of DU MACs. Expected one MAC for UPlane & CPlane or 2 separate MACs\n");
  struct rte_flow_error error;
  struct rte_flow_attr attr = {.ingress = 1};

  // Action: Queue 0
  struct rte_flow_action actions[2];
  struct rte_flow_action_queue queue = {.index = 0};

  actions[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
  actions[0].conf = &queue;
  actions[1].type = RTE_FLOW_ACTION_TYPE_END;

  struct rte_ether_addr local_mac;
  rte_eth_macaddr_get(port_id, &local_mac);

  for (int i = 0; i < num_du_macs; i++) {
    struct rte_flow_item_eth eth_spec, eth_mask;
    memset(&eth_spec, 0, sizeof(eth_spec));
    memset(&eth_mask, 0, sizeof(eth_mask));

    rte_ether_addr_copy(&du_macs[i], &eth_spec.src);
    memset(&eth_mask.src, 0xFF, 6);
    memset(&eth_mask.dst, 0x00, 6);

    struct rte_flow_item_vlan vlan_spec, vlan_mask;
    memset(&vlan_spec, 0, sizeof(vlan_spec));
    memset(&vlan_mask, 0, sizeof(vlan_mask));

    // Wildcard eCPRI specs: Zero-initialized maps to a match-all filter for eCPRI layer
    struct rte_flow_item_ecpri ecpri_spec;
    struct rte_flow_item_ecpri ecpri_mask;
    memset(&ecpri_spec, 0, sizeof(struct rte_flow_item_ecpri));
    memset(&ecpri_mask, 0, sizeof(struct rte_flow_item_ecpri));

    // Rule 1: Match all Untagged eCPRI traffic from DU MAC
    struct rte_flow_item pattern_ecpri[3];
    memset(pattern_ecpri, 0, sizeof(pattern_ecpri));
    pattern_ecpri[0].type = RTE_FLOW_ITEM_TYPE_ETH;
    pattern_ecpri[0].spec = &eth_spec;
    pattern_ecpri[0].mask = &eth_mask;
    pattern_ecpri[1].type = RTE_FLOW_ITEM_TYPE_ECPRI;
    pattern_ecpri[1].spec = &ecpri_spec;
    pattern_ecpri[1].mask = &ecpri_mask;
    pattern_ecpri[2].type = RTE_FLOW_ITEM_TYPE_END;

    struct rte_flow *flow_ecpri = rte_flow_create(port_id, &attr, pattern_ecpri, actions, &error);
    if (!flow_ecpri) {
      fprintf(stderr, "Warning: Failed to create ECPRI flow for DU MAC %d on port %u: %s\n", i + 1, port_id, error.message);
    } else {
      flow_config_t *flow_config = &io->configured_flows[io->num_configured_flows++];
      flow_config->port_id = port_id;
      flow_config->flow = flow_ecpri;
    }

    // Rule 2: Match all VLAN Tagged eCPRI traffic from DU MAC
    struct rte_flow_item pattern_vlan_ecpri[4];
    memset(pattern_vlan_ecpri, 0, sizeof(pattern_vlan_ecpri));
    pattern_vlan_ecpri[0].type = RTE_FLOW_ITEM_TYPE_ETH;
    pattern_vlan_ecpri[0].spec = &eth_spec;
    pattern_vlan_ecpri[0].mask = &eth_mask;
    pattern_vlan_ecpri[1].type = RTE_FLOW_ITEM_TYPE_VLAN;
    pattern_vlan_ecpri[1].spec = &vlan_spec;
    pattern_vlan_ecpri[1].mask = &vlan_mask;
    pattern_vlan_ecpri[2].type = RTE_FLOW_ITEM_TYPE_ECPRI;
    pattern_vlan_ecpri[2].spec = &ecpri_spec;
    pattern_vlan_ecpri[2].mask = &ecpri_mask;
    pattern_vlan_ecpri[3].type = RTE_FLOW_ITEM_TYPE_END;

    struct rte_flow *flow_vlan_ecpri = rte_flow_create(port_id, &attr, pattern_vlan_ecpri, actions, &error);
    if (!flow_vlan_ecpri) {
      fprintf(stderr, "Warning: Failed to create VLAN ECPRI flow for DU MAC %d on port %u: %s\n", i + 1, port_id, error.message);
    } else {
      flow_config_t *flow_config = &io->configured_flows[io->num_configured_flows++];
      flow_config->port_id = port_id;
      flow_config->flow = flow_vlan_ecpri;
    }
  }

  return 0;
}

static int configure_dpdk_port(uint16_t port_id, oru_io_config_t *conf, struct rte_mempool *pool)
{
  struct rte_eth_dev_info dev_info;
  int ret = rte_eth_dev_info_get(port_id, &dev_info);
  if (ret != 0) {
    fprintf(stderr, "Error during getting device (port %u) info: %s\n", port_id, strerror(-ret));
    return ret;
  }

  struct rte_eth_conf port_conf = {
      .rxmode =
          {
              .mq_mode = RTE_ETH_MQ_RX_NONE,
              .mtu = conf->mtu,
              .offloads = 0,
          },
      .txmode =
          {
              .mq_mode = RTE_ETH_MQ_TX_NONE,
              .offloads = 0,
          },
  };

  if (dev_info.rx_offload_capa & RTE_ETH_RX_OFFLOAD_SCATTER) {
    port_conf.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_SCATTER;
  }

  if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE) {
    port_conf.txmode.offloads |= RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;
  }

  ret = rte_eth_dev_configure(port_id, 1, 1, &port_conf);
  if (ret < 0) {
    fprintf(stderr, "Failed to configure port %u: %s\n", port_id, rte_strerror(-ret));
    return ret;
  }

  ret = rte_eth_dev_set_mtu(port_id, conf->mtu);
  if (ret < 0) {
    fprintf(stderr, "Warning: Failed to set MTU %u on port %u: %s\n", conf->mtu, port_id, rte_strerror(-ret));
  }

  ret = rte_eth_rx_queue_setup(port_id, 0, 1024, rte_eth_dev_socket_id(port_id), NULL, pool);
  if (ret < 0) {
    fprintf(stderr, "Failed to setup RX queue 0 on port %u: %s\n", port_id, rte_strerror(-ret));
    return ret;
  }

  ret = rte_eth_tx_queue_setup(port_id, 0, 1024, rte_eth_dev_socket_id(port_id), NULL);
  if (ret < 0) {
    fprintf(stderr, "Failed to setup TX queue 0 on port %u: %s\n", port_id, rte_strerror(-ret));
    return ret;
  }

  ret = rte_eth_dev_start(port_id);
  if (ret < 0) {
    fprintf(stderr, "Failed to start port %u: %s\n", port_id, rte_strerror(-ret));
    return ret;
  }

  return 0;
}

int oru_io_init(oru_io_t *io, oru_io_config_t *conf)
{
  memset(io, 0, sizeof(*io));
  if (!io || !conf || conf->num_ports == 0)
    return -1;
  io->conf = *conf;

  // 1. Create Mbuf Pool if requested
  if (conf->mbuf_count > 0) {
    io->rx_pool = rte_pktmbuf_pool_create("oru_rx_pool", conf->mbuf_count, 256, 0, conf->mbuf_data_room, rte_socket_id());
    if (!io->rx_pool) {
      fprintf(stderr, "Failed to create mbuf pool: %s\n", rte_strerror(rte_errno));
      return -1;
    }
    io->tx_pool = rte_pktmbuf_pool_create("oru_tx_pool", conf->mbuf_count, 256, 0, conf->mbuf_data_room, rte_socket_id());
    if (!io->tx_pool) {
      fprintf(stderr, "Failed to create mbuf pool: %s\n", rte_strerror(rte_errno));
      return -1;
    }
    io->mbuf_size = conf->mbuf_data_room; // Record data size for fragmentation awareness
  }

  // 2. Configure Ports
  for (uint16_t i = 0; i < conf->num_ports; i++) {
    if (configure_dpdk_port(conf->port_ids[i], conf, io->rx_pool) < 0)
      return -1;

    rte_eth_macaddr_get(conf->port_ids[i], &io->local_macs[i]);
    if (rte_eth_dev_get_mtu(conf->port_ids[i], &io->mtus[i]) < 0) {
      fprintf(stderr, "Failed to get MTU for port %u, defaulting to 1500\n", conf->port_ids[i]);
      io->mtus[i] = 1500;
    }
    printf("ORU_IO: Port %u: MAC %02X:%02X:%02X:%02X:%02X:%02X, MTU %u\n",
           conf->port_ids[i],
           io->local_macs[i].addr_bytes[0],
           io->local_macs[i].addr_bytes[1],
           io->local_macs[i].addr_bytes[2],
           io->local_macs[i].addr_bytes[3],
           io->local_macs[i].addr_bytes[4],
           io->local_macs[i].addr_bytes[5],
           io->mtus[i]);
  }

  // 3. Initialize Timer
  if (fh_timer_init(&io->timer, conf->numerology) < 0)
    return -1;
  if (conf->clock_timebase == FH_CLOCK_TAI)
    fh_timer_set_timebase(&io->timer, FH_CLOCK_TAI);
  fh_timer_register_cb(&io->timer, conf->timer_cb, conf->timer_user_data);

  // 4. Initialize Send (on primary port)
  if (fh_send_init(&io->send, &io->timer, conf->port_ids[0], 0) < 0)
    return -1;

  // 5. Initialize Recv (on all configured ports)
  if (conf->num_ports == 1) {
    // Configure all configured MACs for 1 port
    fh_recv_q_conf_t rx_q;
    rx_q.port_id = conf->port_ids[0];
    rx_q.queue_id = 0;
    int ret = configure_ru_flows(io, conf->port_ids[0], conf->num_macs, conf->du_macs);
    if (ret != 0) {
      fprintf(stderr, "Failed to configure flows for port %u\n", conf->port_ids[0]);
    }
    if (fh_recv_init(&io->recv, &io->timer, &rx_q, 1, conf->rx_cb, conf->rx_user_data) < 0)
      return -1;
  } else if (conf->num_ports == 2) {
    fh_recv_q_conf_t rx_qs[MAX_RU_PORTS];
    for (uint16_t i = 0; i < conf->num_ports; i++) {
      struct rte_ether_addr *du_mac = &conf->du_macs[i];
      if (i == 1 && conf->num_macs == 1) {
        du_mac = &conf->du_macs[0];
      }
      rx_qs[i].port_id = conf->port_ids[i];
      rx_qs[i].queue_id = 0;

      // 6. Configure Flows
      int ret = configure_ru_flows(io, conf->port_ids[i], 1, du_mac);
      if (ret != 0) {
        fprintf(stderr, "Failed to configure flows for port %u\n", conf->port_ids[i]);
      }
    }
    if (fh_recv_init(&io->recv, &io->timer, rx_qs, conf->num_ports, conf->rx_cb, conf->rx_user_data) < 0)
      return -1;
  } else {
    fprintf(stderr, "Unsupported number of ports: %d\n", conf->num_ports);
    return -1;
  }
  return 0;
}

int oru_io_send_uplane(oru_io_t *io, struct rte_mbuf **mbufs, uint32_t num_mbufs)
{
  for (uint32_t i = 0; i < num_mbufs; i++) {
    struct rte_ether_hdr *eth = (struct rte_ether_hdr *)rte_pktmbuf_prepend(mbufs[i], sizeof(struct rte_ether_hdr));
    if (!eth)
      continue;

    eth->ether_type = rte_cpu_to_be_16(ECPRI_ETHER_TYPE);
    rte_ether_addr_copy(&io->conf.du_macs[0], &eth->dst_addr);
    rte_ether_addr_copy(&io->local_macs[0], &eth->src_addr);

    oru_pcap_write_uplane(mbufs[i], oru_pcap_mbuf_is_prach(mbufs[i]));
  }

  return fh_send_immediate(&io->send, mbufs, num_mbufs);
}

void oru_io_register_timer_cb(oru_io_t *io, fh_timer_cb cb, void *user_data)
{
  if (!io)
    return;
  fh_timer_register_cb(&io->timer, cb, user_data);
}

int oru_io_run(oru_io_t *io, unsigned int worker_lcore)
{
  return rte_eal_remote_launch(fh_recv_run, &io->recv, worker_lcore);
}

void oru_io_stop(oru_io_t *io)
{
  fh_timer_stop(&io->timer);
  fh_recv_stop(&io->recv);
}

void oru_io_cleanup(oru_io_t *io)
{
  for (int i = 0; i < io->num_configured_flows; i++) {
    rte_flow_destroy(io->configured_flows[i].port_id, io->configured_flows[i].flow, NULL);
  }
  if (io->rx_pool) {
    rte_mempool_free(io->rx_pool);
    io->rx_pool = NULL;
  }
  if (io->tx_pool) {
    rte_mempool_free(io->tx_pool);
    io->tx_pool = NULL;
  }
}

struct rte_mbuf *oru_io_get_sendbuf(oru_io_t *io)
{
  return rte_pktmbuf_alloc(io->tx_pool);
}
