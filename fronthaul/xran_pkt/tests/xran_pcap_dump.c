/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pcap.h>
#include <rte_common.h>
#include <rte_eal.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_byteorder.h>
#include "xran_pkt_api.h"

const char *fft_size_to_string(enum xran_cp_fftsize fft_size)
{
  switch (fft_size) {
    case XRAN_FFTSIZE_128:
      return "128";
    case XRAN_FFTSIZE_256:
      return "256";
    case XRAN_FFTSIZE_512:
      return "512";
    case XRAN_FFTSIZE_1024:
      return "1024";
    case XRAN_FFTSIZE_2048:
      return "2048";
    case XRAN_FFTSIZE_4096:
      return "4096";
    case XRAN_FFTSIZE_1536:
      return "1536";
    default:
      return "Unknown";
  }
}

/* Global config for tests - same as in test_xran_pkt.c */
struct xran_eaxcid_config g_eaxcid_config = {.mask_cuPortId = 0xF000,
                                             .mask_bandSectorId = 0x0F00,
                                             .mask_ccId = 0x00F0,
                                             .mask_ruPortId = 0x000F,
                                             .bit_cuPortId = 12,
                                             .bit_bandSectorId = 8,
                                             .bit_ccId = 4,
                                             .bit_ruPortId = 0};

struct dump_ctx {
  struct rte_mempool *mp;
  int pkt_count;
};

void packet_handler(u_char *user, const struct pcap_pkthdr *pkthdr, const u_char *packet)
{
  struct dump_ctx *ctx = (struct dump_ctx *)user;
  ctx->pkt_count++;

  printf("--- Packet #%d (%u bytes) ---\n", ctx->pkt_count, pkthdr->caplen);

  struct rte_mbuf *mbuf = rte_pktmbuf_alloc(ctx->mp);
  if (!mbuf) {
    printf("  Error: failed to allocate mbuf\n");
    return;
  }

  char *dst = rte_pktmbuf_append(mbuf, pkthdr->caplen);
  memcpy(dst, packet, pkthdr->caplen);

  struct rte_ether_hdr *eth = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr *);
  uint16_t eth_type = rte_be_to_cpu_16(eth->ether_type);

  if (eth_type == 0x8100) { // VLAN
    struct rte_vlan_hdr *vlan = (struct rte_vlan_hdr *)(eth + 1);
    eth_type = rte_be_to_cpu_16(vlan->eth_proto);
    rte_pktmbuf_adj(mbuf, sizeof(struct rte_ether_hdr) + sizeof(struct rte_vlan_hdr));
  } else {
    rte_pktmbuf_adj(mbuf, sizeof(struct rte_ether_hdr));
  }

  if (eth_type == 0xAEFE) {
    // eCPRI
    struct xran_ecpri_hdr *ecpri_hdr;
    struct xran_recv_packet_info pkt_info;

    if (xran_parse_ecpri_hdr(mbuf, &ecpri_hdr, &pkt_info) == 0) {
      if (pkt_info.msg_type == ECPRI_IQ_DATA) {
        printf("  U-Plane (IQ Data)\n");
        // Now call xran_extract_iq_samples for U-plane
        void *out_iq;
        uint8_t cc_id, ant_id, frame, sf, slot, sym, filter;
        union ecpri_seq_id seq;
        uint16_t num_prb, start_prb, sym_inc, rb, sect_id;
        uint8_t meth, width;

        int32_t ret = xran_extract_iq_samples(mbuf,
                                              &g_eaxcid_config,
                                              &out_iq,
                                              &cc_id,
                                              &ant_id,
                                              &frame,
                                              &sf,
                                              &slot,
                                              &sym,
                                              &filter,
                                              &seq,
                                              &num_prb,
                                              &start_prb,
                                              &sym_inc,
                                              &rb,
                                              &sect_id,
                                              0,
                                              0,
                                              &meth,
                                              &width);

        if (ret > 0) {
          printf("  eCPRI SeqID: %d  CC: %d  Ant: %d\n", seq.bits.seq_id, cc_id, ant_id);
          printf("  Frame: %d  Subframe: %d  Slot: %d  Symbol: %d\n", frame, sf, slot, sym);
          printf("  Section ID: %d  NumPRB: %d  StartPRB: %d\n", sect_id, num_prb, start_prb);
        } else {
          printf("  Error: xran_extract_iq_samples failed for IQ_DATA\n");
        }
      } else if (pkt_info.msg_type == ECPRI_RT_CONTROL_DATA) {
        printf("  C-Plane (RT Control Data)\n");
        uint8_t cc_id, ant_id;
        xran_decompose_cid(ecpri_hdr->ecpri_xtc_id, &g_eaxcid_config, NULL, NULL, &cc_id, &ant_id);
        printf("  CC: %d  Ant: %d  SeqID: %d\n", cc_id, ant_id, pkt_info.seq_id);

        struct xran_cp_radioapp_common_header *apphdr =
            (struct xran_cp_radioapp_common_header *)rte_pktmbuf_adj(mbuf, sizeof(struct xran_ecpri_hdr));
        if (apphdr == NULL) {
          printf("  Error: issue extracting apphdr\n");
        } else {
          uint32_t fields = rte_be_to_cpu_32(apphdr->field.all_bits);
          uint8_t frame = (fields >> 16) & 0xFF;
          uint8_t subframe = (fields >> 12) & 0x0F;
          uint8_t slot = (fields >> 6) & 0x3F;
          uint8_t start_sym = (fields) & 0x3F;
          uint8_t dir = (fields >> 31) & 0x01;

          printf("  Frame: %d  Subframe: %d  Slot: %d  StartSym: %d  Dir: %s\n",
                 frame,
                 subframe,
                 slot,
                 start_sym,
                 dir ? "DL" : "UL");
          printf("  Section Type: %d  NumSections: %d\n", apphdr->sectionType, apphdr->numOfSections);

          switch (apphdr->sectionType) {
            case XRAN_CP_SECTIONTYPE_3: {
              struct xran_cp_radioapp_section3_header *hdr = (struct xran_cp_radioapp_section3_header *)apphdr;
              printf("  [Sec 3] fftSize: %s  uScs: %d  cpLength: %d  timeOffset: %d  udCompMeth: %d  udIqWidth: %d\n",
                     fft_size_to_string(hdr->frameStructure.fftSize),
                     hdr->frameStructure.uScs,
                     hdr->cpLength,
                     hdr->timeOffset,
                     hdr->udComp.udCompMeth,
                     hdr->udComp.udIqWidth);
              struct xran_cp_radioapp_section3 *section =
                  (struct xran_cp_radioapp_section3 *)rte_pktmbuf_adj(mbuf, sizeof(struct xran_cp_radioapp_section3_header));
              if (section) {
                *((uint64_t *)section) = rte_be_to_cpu_64(*((uint64_t *)section));
                printf("  [Sec 3] SectionID: %d  StartPRB: %d  NumPRB: %d\n",
                       section->hdr.u1.common.sectionId,
                       section->hdr.u1.common.startPrbc,
                       section->hdr.u1.common.numPrbc);
              }
              break;
            }
            case XRAN_CP_SECTIONTYPE_1: {
              struct xran_cp_radioapp_section1_header *hdr = (struct xran_cp_radioapp_section1_header *)apphdr;
              printf("  [Sec 1] udCompMeth: %d  udIqWidth: %d\n", hdr->udComp.udCompMeth, hdr->udComp.udIqWidth);
              struct xran_cp_radioapp_section1 *section =
                  (struct xran_cp_radioapp_section1 *)rte_pktmbuf_adj(mbuf, sizeof(struct xran_cp_radioapp_section1_header));
              if (section) {
                section->hdr.u1.second_4byte = rte_be_to_cpu_32(section->hdr.u1.second_4byte);
                section->hdr.u.first_4byte = rte_be_to_cpu_32(section->hdr.u.first_4byte);
                printf("  [Sec 1] SectionID: %d  StartPRB: %d  NumPRB: %d  NumSym: %d  BeamID: %d\n",
                       section->hdr.u1.common.sectionId,
                       section->hdr.u1.common.startPrbc,
                       section->hdr.u1.common.numPrbc,
                       section->hdr.u.s1.numSymbol,
                       section->hdr.u.s1.beamId);
              }
              break;
            }
            default:
              printf("  Unsupported Section Type %d\n", apphdr->sectionType);
              break;
          }
        }
      } else {
        printf("  Msg Type: 0x%02x (eCPRI Payl Size: %d)\n", pkt_info.msg_type, pkt_info.payload_len);
      }
    } else {
      printf("  Error: xran_parse_ecpri_hdr failed\n");
    }
  } else {
    printf("  Non-eCPRI packet (EtherType: 0x%04x)\n", eth_type);
  }

  rte_pktmbuf_free(mbuf);
}

int main(int argc, char *argv[])
{
  if (argc < 2) {
    printf("Usage: %s <pcap_file>\n", argv[0]);
    return 1;
  }

  /* Minimal EAL init */
  char *eal_argv[] = {argv[0], "--no-huge", "--no-pci", "-c", "1", "--log-level", "0"};
  int eal_argc = sizeof(eal_argv) / sizeof(eal_argv[0]);
  if (rte_eal_init(eal_argc, eal_argv) < 0) {
    fprintf(stderr, "Error: EAL initialization failed\n");
    return 1;
  }

  char errbuf[PCAP_ERRBUF_SIZE];
  pcap_t *pcap = pcap_open_offline(argv[1], errbuf);
  if (!pcap) {
    fprintf(stderr, "Error: Could not open pcap file '%s': %s\n", argv[1], errbuf);
    return 1;
  }

  struct rte_mempool *mp = rte_pktmbuf_pool_create("dump_pool", 1024, 0, 0, 9000, rte_socket_id());
  if (!mp) {
    fprintf(stderr, "Error: Failed to create mempool\n");
    pcap_close(pcap);
    return 1;
  }

  struct dump_ctx ctx = {.mp = mp, .pkt_count = 0};

  printf("Dumping packets from %s...\n", argv[1]);
  if (pcap_loop(pcap, 0, packet_handler, (u_char *)&ctx) < 0) {
    fprintf(stderr, "Error: pcap_loop failed\n");
  }

  printf("Dumping complete. %d packets processed.\n", ctx.pkt_count);

  pcap_close(pcap);
  rte_mempool_free(mp);

  return 0;
}
