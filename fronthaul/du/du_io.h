/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef DU_IO_H
#define DU_IO_H

#include <stdint.h>
#include <stdbool.h>
#include <rte_ether.h>
#include <rte_mbuf.h>
#include "fh_timer.h"
#include "fh_send.h"
#include "fh_recv.h"

#define MAX_RU_PORTS 2
#define MAX_RU_MACS 2
#define ECPRI_ETHER_TYPE 0xAEFE
#define MAX_CONFIGURED_FLOWS MAX_RU_PORTS *MAX_RU_MACS * 2

typedef struct {
  uint16_t port_ids[MAX_RU_PORTS];
  uint16_t num_ports;
  struct rte_ether_addr ru_macs[MAX_RU_MACS];
  uint16_t num_macs;
  int numerology;
  fh_recv_cb rx_cb;
  void *rx_user_data;
  fh_timer_cb timer_cb;
  void *timer_user_data;

  // DPDK parameters
  uint16_t mbuf_data_room;
  uint32_t mbuf_count;
  uint16_t mtu;
} du_io_config_t;

typedef struct {
  struct rte_flow *flow;
  int port_id;
} flow_config_t;

typedef struct {
  fh_timer_t timer;
  fh_send_t send;
  fh_recv_t recv;
  du_io_config_t conf;
  struct rte_ether_addr local_macs[MAX_RU_PORTS];
  uint16_t mtus[MAX_RU_PORTS];
  struct rte_mempool *rx_pool;
  struct rte_mempool *tx_pool;
  uint16_t mbuf_size;
  flow_config_t configured_flows[MAX_CONFIGURED_FLOWS];
  int num_configured_flows;
} du_io_t;

/**
 * @brief Initialize the O-DU Fronthaul library.
 * This function orchestrates timer, send, and recv components,
 * and configures hardware rte_flows for eCPRI filtering.
 *
 * @param io Pointer to du_io handle
 * @param conf Pointer to DU configuration (ports, MACs, numerology, callbacks)
 * @return int 0 on success, negative on error
 */
int du_io_init(du_io_t *io, du_io_config_t *conf);

/**
 * @brief Send U-Plane traffic to the primary O-DU MAC.
 * Automatically prepends Ethernet header before symbol-scheduled transmission.
 *
 * @param io Pointer to du_io handle
 * @param mbufs Array of user-plane mbufs
 * @param num_mbufs Number of mbufs
 * @return int Number of mbufs enqueued, or negative on error
 */
int du_io_send_uplane(du_io_t *io, struct rte_mbuf **mbufs, uint32_t num_mbufs);

/**
 * @brief Register a callback for high-precision symbol timer events.
 *
 * @param io Pointer to du_io handle
 * @param cb Callback function
 * @param user_data User data for callback
 */
void du_io_register_timer_cb(du_io_t *io, fh_timer_cb cb, void *user_data);

/**
 * @brief Run the O-DU fronthaul loops.
 *
 * @param io Pointer to du_io handle
 * @param worker_lcore Lcore ID to run the integrated receiver and timer
 * @return int 0 on success, negative on error
 */
int du_io_run(du_io_t *io, unsigned int worker_lcore);

/**
 * @brief Stop the O-DU fronthaul loops.
 *
 * @param io Pointer to du_io handle
 */
void du_io_stop(du_io_t *io);

/**
 * @brief Clean up du_io resources.
 *
 * @param io Pointer to du_io handle
 */
void du_io_cleanup(du_io_t *io);

/**
 * @brief get mbuf
 * @param io Pointer to du_io handle
 * @return struct rte_mempool * Pointer to mbuf pool
 */
struct rte_mbuf *du_io_get_sendbuf(du_io_t *io);

#endif /* DU_IO_H */
