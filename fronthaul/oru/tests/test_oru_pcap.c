/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pcap.h>
#include <rte_common.h>
#include <rte_eal.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_byteorder.h>
#include "oru_packet_processor.h"
#include "xran_pkt_api.h"
#include "log.h"
#include "common/config/config_userapi.h"

#define MAX_ANTENNAS 4

// OAI Linkage Satisfiers
void exit_function(const char *file, const char *function, const int line, const char *s, const int assertflag)
{
  fprintf(stderr, "Error at %s:%s:%d - %s\n", file, function, line, s ? s : "None");
  exit(1);
}
configmodule_interface_t *uniqCfg = NULL;

struct rte_mempool *mp = NULL;
uint64_t g_total_uplane_sent = 0;

void *test_alloc_mbuf(void *io_controller)
{
  return rte_pktmbuf_alloc(mp);
}

void test_send_mbuf(void *io_controller, struct rte_mbuf **mbufs, uint32_t num_mbufs)
{
  for (uint32_t i = 0; i < num_mbufs; i++) {
    g_total_uplane_sent++;
    rte_pktmbuf_free(mbufs[i]);
  }
}

int main(int argc, char *argv[])
{
  if (argc < 11) {
    printf(
        "Usage: %s <pcap_file> <initial_symbol> <num_dl_slots> <num_ul_slots> <num_dl_symbols> <num_ul_symbols> "
        "<tdd_pattern_length_slots> <mtu> <prach_eaxc_offset> -- <eal args>\n",
        argv[0]);
    return 1;
  }

  int eal_args_start = 1;
  while (eal_args_start < argc && strncmp("--", argv[eal_args_start], strlen("--")) != 0) {
    eal_args_start++;
  }

  if (eal_args_start < argc) {
    if (rte_eal_init(argc - eal_args_start, argv + eal_args_start) < 0) {
      fprintf(stderr, "Error: EAL initialization failed\n");
      return 1;
    }
  } else {
    // Default EAL args if none provided
    char *default_argv[] = {argv[0], "--no-huge", "--iova-mode=va", "--no-pci", "--file-prefix=test_oru_pcap"};
    if (rte_eal_init(5, default_argv) < 0) {
      fprintf(stderr, "Error: EAL initialization failed\n");
      return 1;
    }
  }
  logInit();

  mp = rte_pktmbuf_pool_create("test_pool", 4096, 0, 0, 10240, rte_socket_id());
  assert(mp != NULL);

  uint64_t initial_symbol = atoll(argv[2]);
  int num_dl_slots = atoi(argv[3]);
  int num_ul_slots = atoi(argv[4]);
  int num_dl_symbols = atoi(argv[5]);
  int num_ul_symbols = atoi(argv[6]);
  int tdd_pattern_length_slots = atoi(argv[7]);
  size_t mtu = atoi(argv[8]);
  int prach_eaxc_offset = atoi(argv[9]);

  if (eal_args_start < 10) {
    printf("Error: Missing '--' separator for EAL arguments\n");
    return 1;
  }

  printf("Starting test with parameters:\n");
  printf("  pcap: %s\n", argv[1]);
  printf("  initial_symbol: %lu\n", initial_symbol);
  printf("  TDD: DL %d slots + %d sym, UL %d slots + %d sym, Pattern Length %d slots\n",
         num_dl_slots,
         num_dl_symbols,
         num_ul_slots,
         num_ul_symbols,
         tdd_pattern_length_slots);
  printf("  MTU: %zu\n", mtu);
  printf("  PRACH eAxC Offset: %d\n", prach_eaxc_offset);

  void *ctx = init_packet_processor(1,
                                    273,
                                    0,
                                    1500,
                                    0,
                                    1200,
                                    num_dl_slots,
                                    num_ul_slots,
                                    num_dl_symbols,
                                    num_ul_symbols,
                                    tdd_pattern_length_slots,
                                    test_alloc_mbuf,
                                    test_send_mbuf,
                                    NULL,
                                    mtu,
                                    prach_eaxc_offset,
                                    FH_COMP_NONE,
                                    0);
  assert(ctx != NULL);

  char errbuf[PCAP_ERRBUF_SIZE];
  pcap_t *pcap = pcap_open_offline(argv[1], errbuf);
  if (!pcap) {
    fprintf(stderr, "Error: Could not open pcap file '%s': %s\n", argv[1], errbuf);
    return 1;
  }

  struct pcap_pkthdr *pkthdr;
  const u_char *packet;
  int pkt_count = 0;
  int ecpri_count = 0;

  double first_ts = 0;
  bool synced = false;

  uint64_t last_tick_sym = initial_symbol;

  static uint32_t *txdataF[MAX_ANTENNAS] = {NULL};
  for (int i = 0; i < MAX_ANTENNAS; i++)
    txdataF[i] = malloc(273 * 12 * sizeof(uint32_t));

  while (pcap_next_ex(pcap, &pkthdr, &packet) >= 0) {
    pkt_count++;
    double ts = pkthdr->ts.tv_sec + pkthdr->ts.tv_usec / 1000000.0;

    struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mp);
    if (!mbuf) {
      fprintf(stderr, "Failed to allocate mbuf at packet %d\n", pkt_count);
      break;
    }

    char *dst = rte_pktmbuf_append(mbuf, pkthdr->caplen);
    memcpy(dst, packet, pkthdr->caplen);

    struct rte_ether_hdr *eth = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr *);
    uint16_t eth_type = rte_be_to_cpu_16(eth->ether_type);
    size_t hdr_len = sizeof(struct rte_ether_hdr);

    if (eth_type == 0x8100) { // VLAN
      struct rte_vlan_hdr *vlan = (struct rte_vlan_hdr *)(eth + 1);
      eth_type = rte_be_to_cpu_16(vlan->eth_proto);
      hdr_len += sizeof(struct rte_vlan_hdr);
    }

    if (eth_type == 0xAEFE) {
      ecpri_count++;
      rte_pktmbuf_adj(mbuf, hdr_len);

      struct xran_ecpri_hdr *ecpri = rte_pktmbuf_mtod(mbuf, struct xran_ecpri_hdr *);

      if (!synced && ecpri->cmnhdr.bits.ecpri_mesg_type == ECPRI_RT_CONTROL_DATA) {
        struct xran_cp_radioapp_common_header *apphdr = (void *)((uint8_t *)ecpri + sizeof(struct xran_ecpri_hdr));
        uint32_t fields = rte_be_to_cpu_32(apphdr->field.all_bits);
        uint8_t frame = (fields >> 16) & 0xFF;
        uint8_t subframe = (fields >> 12) & 0x0F;
        uint8_t slot = (fields >> 6) & 0x3F;
        uint8_t start_sym = (fields) & 0x3F;

        uint64_t target_sym = (uint64_t)frame * 280 + (subframe * 2 + slot) * 14 + start_sym;
        handle_absolute_symbol_tick(ctx, initial_symbol);
        last_tick_sym = initial_symbol;
        first_ts = ts;
        synced = true;
        printf("Synced to pcap! First C-plane target sym: %lu, setting initial_symbol: %lu\n", target_sym, initial_symbol);
      }

      if (synced) {
        uint64_t elapsed_syms = (uint64_t)((ts - first_ts) / 0.0000357142857);
        uint64_t current_sym = initial_symbol + elapsed_syms;

        if (current_sym > last_tick_sym) {
          for (uint64_t s = last_tick_sym + 1; s <= current_sym; s++) {
            handle_absolute_symbol_tick(ctx, s);

            // For UL verification, we call write_ul_iq for every symbol.
            // It will internally check TDD and UL C-plane presence.
            ul_job_t job;
            int poll_ret = poll_ul_job(ctx, &job);
            if (poll_ret == 0) {
              for (int i = 0; i < job.num_symbols; i++) {
                write_ul_iq(ctx, txdataF[0], job.symbol + i, &job);
              }
            }
            int frame = (s / 280) % 1024;
            int slot = (s / 14) % 20;
            int sym = s % 14;
            write_prach_iq(ctx, txdataF, MAX_ANTENNAS, frame, slot, sym);

            // Drain ready jobs to prevent ring overflow
            int f, sl, sy;
            uint64_t hf;
            while (get_ready_job_count(ctx) > 0) {
              read_dl_iq(ctx, txdataF, MAX_ANTENNAS, &hf, &f, &sl, &sy, NULL);
            }
          }
          last_tick_sym = current_sym;
        }

        if (ecpri->cmnhdr.bits.ecpri_mesg_type == ECPRI_IQ_DATA) {
          handle_uplane_packet(ctx, mbuf);
        } else if (ecpri->cmnhdr.bits.ecpri_mesg_type == ECPRI_RT_CONTROL_DATA) {
          handle_cplane_packet(ctx, mbuf);
        } else {
          rte_pktmbuf_free(mbuf);
        }
      } else {
        rte_pktmbuf_free(mbuf);
      }
    } else {
      rte_pktmbuf_free(mbuf);
    }
  }

  // Flush remaining symbols
  for (int i = 0; i < 100; i++) {
    handle_absolute_symbol_tick(ctx, ++last_tick_sym);
    // Drain ready jobs to prevent ring overflow during flush
    int f, sl, sy;
    uint64_t hf;
    while (get_ready_job_count(ctx) > 0) {
      read_dl_iq(ctx, txdataF, MAX_ANTENNAS, &hf, &f, &sl, &sy, NULL);
    }
  }

  printf("\nProcessing complete.\n");
  printf("Total packets: %d\n", pkt_count);
  printf("eCPRI packets: %d\n", ecpri_count);

  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  printf("ORU Packet Processor Stats:\n");
  printf("  Total C-Plane Packets: %lu\n", stats.total_cplane);
  printf("  Total U-Plane Received: %lu\n", stats.total_uplane_received);
  printf("  Total U-Plane Sent: %lu\n", stats.total_uplane_sent);
  printf("  C-Plane Header Errors: %lu\n", stats.cplane_err_hdr);
  printf("  C-Plane Protocol Version Errors: %lu\n", stats.cplane_err_ver);
  printf("  C-Plane Timing Early Errors: %lu\n", stats.cplane_err_early);
  printf("  C-Plane Timing Late Errors: %lu\n", stats.cplane_err_late);
  printf("  C-Plane Duplicate Packet Errors: %lu\n", stats.cplane_err_dup);
  printf("  U-Plane Timing Early Errors: %lu\n", stats.uplane_err_early);
  printf("  U-Plane Timing Late Errors: %lu\n", stats.uplane_err_late);
  printf("  U-Plane Duplicate Packet Errors: %lu\n", stats.uplane_err_dup);
  printf("  U-Plane Missing C-Plane Errors: %lu\n", stats.uplane_missing_cplane);
  printf("  UL C-Plane Missing: %lu\n", stats.ul_cplane_missing);
  printf("  DL TDD Mismatch: %lu\n", stats.dl_tdd_mismatch);
  printf("  UL TDD Mismatch: %lu\n", stats.ul_tdd_mismatch);
  printf("  Out of Mbufs: %lu\n", stats.out_of_mbufs);
  printf("  Application Too Slow Errors: %lu\n", stats.application_too_slow);

  pcap_close(pcap);
  cleanup_packet_processor(ctx);
  if (mp)
    rte_mempool_free(mp);
  for (int i = 0; i < MAX_ANTENNAS; i++)
    free(txdataF[i]);

  return 0;
}
