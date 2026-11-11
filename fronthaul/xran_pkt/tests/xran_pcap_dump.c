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
#include "xran_pkt_bfw.h"

#define MAX_BF_WEIGHTS 64

void exit_function(const char *file, const char *function, const int line, const char *s, const int assertflag)
{
  fprintf(stderr, "Error at %s:%s:%d - %s\n", file, function, line, s ? s : "None");
  exit(1);
}

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
  int error_count; // bumped on any malformed/unparseable content, so this doubles as a regression check
  int num_bf_weights; // ext1 weights per section; 0 = don't decode ext1 weights
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
          ctx->error_count++;
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
              uint8_t *sec_ptr = (uint8_t *)rte_pktmbuf_adj(mbuf, sizeof(struct xran_cp_radioapp_section1_header));
              if (sec_ptr) {
                const uint8_t *pkt_end = sec_ptr + rte_pktmbuf_data_len(mbuf);
                bool malformed = false;
                for (int i = 0; i < apphdr->numOfSections && !malformed; i++) {
                  if ((size_t)(pkt_end - sec_ptr) < sizeof(struct xran_cp_radioapp_section1)) {
                    printf("      malformed section %d: header past end of packet\n", i);
                    ctx->error_count++;
                    break;
                  }
                  struct xran_cp_radioapp_section1 sec_copy;
                  memcpy(&sec_copy, sec_ptr, sizeof(sec_copy));
                  *((uint64_t *)&sec_copy) = rte_be_to_cpu_64(*((uint64_t *)&sec_copy));
                  printf("  [Sec 1] SectionID: %d  StartPRB: %d  NumPRB: %d  NumSym: %d  BeamID: %d\n",
                         sec_copy.hdr.u1.common.sectionId,
                         sec_copy.hdr.u1.common.startPrbc,
                         sec_copy.hdr.u1.common.numPrbc,
                         sec_copy.hdr.u.s1.numSymbol,
                         sec_copy.hdr.u.s1.beamId);
                  // Extensions sit right after the section-1 header. extType/ef live in the
                  // extension's first byte (bits 6-0 / bit 7) and extLen in its second byte -
                  // the same raw-byte layout struct xran_cp_radioapp_section_ext1 uses, no
                  // 16-bit byte-order conversion needed (see its comment).
                  uint8_t *ext_ptr = sec_ptr + sizeof(struct xran_cp_radioapp_section1);
                  int ef = sec_copy.hdr.u.s1.ef;
                  while (ef) {
                    if (pkt_end - ext_ptr < 2) {
                      printf("      malformed extension: header past end of packet\n");
                      ctx->error_count++;
                      malformed = true;
                      break;
                    }
                    uint8_t extType = ext_ptr[0] & 0x7F;
                    uint8_t next_ef = (ext_ptr[0] >> 7) & 1;
                    uint8_t extLen = ext_ptr[1];
                    if (extLen == 0 || (size_t)extLen * 4 > (size_t)(pkt_end - ext_ptr)) {
                      printf("      malformed extension: extLen=%d\n", extLen);
                      ctx->error_count++;
                      malformed = true;
                      break;
                    }
                    if (extType == XRAN_CP_SECTIONEXTCMD_1 && ctx->num_bf_weights == 0) {
                      printf("      [Ext1] %d bytes, weights not decoded (num_bf_weights not given)\n", extLen * 4);
                    } else if (extType == XRAN_CP_SECTIONEXTCMD_1) {
                      c16_t weights[MAX_BF_WEIGHTS];
                      int n = xran_decode_bfw_ext1(ext_ptr, (size_t)extLen * 4, ctx->num_bf_weights, weights);
                      if (n < 0) {
                        printf("      [Ext1] malformed beamforming-weights extension\n");
                        ctx->error_count++;
                      } else {
                        printf("      [Ext1] %d beamforming weight(s):", n);
                        for (int w = 0; w < n; w++)
                          printf(" (%d%+di)", weights[w].r, weights[w].i);
                        printf("\n");
                      }
                    } else {
                      printf("      [Ext%d] unsupported extension type, skipping\n", extType);
                    }
                    ext_ptr += (size_t)extLen * 4;
                    ef = next_ef;
                  }
                  sec_ptr = ext_ptr;
                }
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
    printf("Usage: %s <pcap_file> [num_bf_weights]\n", argv[0]);
    return 1;
  }
  int num_bf_weights = argc > 2 ? atoi(argv[2]) : 0;
  if (num_bf_weights < 0 || num_bf_weights > MAX_BF_WEIGHTS) {
    printf("num_bf_weights must be in [0..%d]\n", MAX_BF_WEIGHTS);
    return 1;
  }

  /* Minimal EAL init */
  char *eal_argv[] = {argv[0], "--no-huge", "--no-pci", "-c", "1", "--log-level", "0", "--file-prefix=xran_pcap_dump"};
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

  struct dump_ctx ctx = {.mp = mp, .pkt_count = 0, .error_count = 0, .num_bf_weights = num_bf_weights};

  printf("Dumping packets from %s...\n", argv[1]);
  if (pcap_loop(pcap, 0, packet_handler, (u_char *)&ctx) < 0) {
    fprintf(stderr, "Error: pcap_loop failed\n");
    ctx.error_count++;
  }

  printf("Dumping complete. %d packets processed, %d error(s).\n", ctx.pkt_count, ctx.error_count);

  pcap_close(pcap);
  rte_mempool_free(mp);

  return ctx.error_count > 0 ? 1 : 0;
}
