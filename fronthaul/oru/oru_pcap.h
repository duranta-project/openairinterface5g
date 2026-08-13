/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef ORU_PCAP_H
#define ORU_PCAP_H

#include <stdbool.h>
#include <stdint.h>
#include <rte_mbuf.h>

/* Async UL/DL fronthaul pcap dump (diagnostics).
 *
 * tcpdump on a kernel netdev cannot see O-RU<->DU FH: both ends are DPDK and frames go
 * VF->VF through the NIC eSwitch. Enable with ORU_PCAP_PATH=/tmp/oru_fh.pcap
 * (unset = disabled), Cap with ORU_PCAP_MAX_PACKETS (default 20000).
 * ORU_PCAP_START_DELAY=<seconds> arms the capture later, so the budget is spent on
 * steady-state traffic instead of the O-RU startup phase.
 * ORU_PCAP_CHAN filters (case-insensitive):
 *   all         - every FH C/U-plane (DL RX + UL TX), no temporal gate
 *   rx          - every DU->RU frame: DL C/U-plane plus the UL and PRACH C-plane grants
 *   dl          - DU->RU DL C-plane and DL U-plane only (drops UL/PRACH C-plane)
 *   pusch       - PUSCH UL only
 *   pusch_prach - PUSCH immediately; PRACH only after first PUSCH C-plane, so the
 *                 dump is anchored at the first PUSCH instead of filling with the
 *                 PRACH occasions that recur from O-RU startup
 */

#define ORU_PCAP_CPLANE_SNAP 1024

typedef struct {
  uint8_t data[ORU_PCAP_CPLANE_SNAP];
  uint32_t len;
  bool ok;
} oru_pcap_cplane_snap_t;

void oru_pcap_init_from_env(int prach_eaxc_offset, uint16_t mtu);
bool oru_pcap_mbuf_is_prach(struct rte_mbuf *mbuf);
void oru_pcap_write_uplane(struct rte_mbuf *mbuf, bool is_prach);
/* Dump a full RX Ethernet frame when CHAN=all|rx|dl (DU->RU C/U-plane).
 * Call on the raw RX frame, before the Ethernet/VLAN header is stripped. */
void oru_pcap_write_rx_frame(struct rte_mbuf *mbuf);

/* Call before rte_pktmbuf_adj on C-plane RX so the eCPRI payload is still intact. */
void oru_pcap_cplane_begin(struct rte_mbuf *pkt, oru_pcap_cplane_snap_t *snap);
/* After accepting Type-1 UL: mark PUSCH-seen (opens the pusch_prach gate) and write
 * snap if captured. */
void oru_pcap_cplane_commit_pusch(oru_pcap_cplane_snap_t *snap);
/* After accepting Type-3: write snap if captured. */
void oru_pcap_cplane_commit_prach(oru_pcap_cplane_snap_t *snap);

#endif /* ORU_PCAP_H */
