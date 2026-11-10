/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#pragma once

#include "fh_compression.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HIST_SIZE 64
#define ORU_MAX_BF_WEIGHTS 64
#define MAX_DL_STREAMS_PER_SYMBOL 16 // cap on distinct (eaxc, beam) pairs declared for one DL symbol
// Cap on U-Plane PRB-run fragments stored for one DL symbol, across all (eaxc, beam) pairs. O-RAN CUS
// 5.5 allows application fragmentation: a 273-PRB symbol needs ten uncompressed or six BFP9
// fragments at MTU 1500, so this is 16 per antenna for 4 antennas.
#define MAX_DL_FRAGMENTS_PER_SYMBOL 64
// PRBs of IQ scratch read_dl_iq_streams() needs: one full band per (eaxc, beam) pair. Fragments are
// packed back to back and admitted only while their PRB total fits, whatever the fragment count.
#define DL_IQ_ARENA_PRBS(num_prb) (MAX_DL_STREAMS_PER_SYMBOL * (num_prb))
#define NUM_RU_PORT_IDS 16 // The eAxC layout allocates four bits to the RU port.
#define MAX_ANTENNAS NUM_RU_PORT_IDS // eAxCs (RU ports) per direction: 16 DL layers for cuBB mMIMO
#define ORU_MAX_SRS_EAXC 64 // SRS eAxC IDs: one per RX antenna, up to a 64R mMIMO RU

// Flow kinds of the eAxC lists, each with its own eAxC ID space: DL Section Type 1, UL Section Type 1, Section Type 3,
// and SRS, which cuBB sends as UL Section Type 1 on eAxC IDs of its own
typedef enum { ORU_EAXC_DL, ORU_EAXC_UL, ORU_EAXC_PRACH, ORU_EAXC_SRS, ORU_EAXC_NUM_KINDS } oru_eaxc_kind_t;

// eAxC IDs as the O-DU assigns them (CUS 5.1.3.2.7), e.g. cuBB's eAxC_id_pdsch, eAxC_id_pusch, eAxC_id_prach and
// eAxC_id_srs. ID i of a kind is stream (or antenna) i of that kind. ID i of the SRS list is always RX antenna i. Only the
// key_mask bits of an eAxC ID are compared.
typedef struct {
  uint16_t key_mask;
  int num[ORU_EAXC_NUM_KINDS];
  uint16_t id[ORU_EAXC_NUM_KINDS][ORU_MAX_SRS_EAXC];
} oru_eaxc_lists_t;

// Stream index of each masked eAxC ID for each kind, ORU_EAXC_NONE if the ID is not in that kind
#define ORU_EAXC_NONE 0xFF
typedef struct {
  uint16_t key_mask;
  uint8_t index[ORU_EAXC_NUM_KINDS][1 << 16];
} oru_eaxc_map_t;

// One C-Plane flow: its eAxC ID as received (ecpriRtcid), its kind and its stream (or antenna) index, -1 if
// the ID is not in its kind
typedef struct {
  uint16_t eaxc;
  oru_eaxc_kind_t kind;
  int index;
} oru_flow_t;

static inline int oru_eaxc_list_max(oru_eaxc_kind_t kind)
{
  return kind == ORU_EAXC_SRS ? ORU_MAX_SRS_EAXC : MAX_ANTENNAS;
}

// Stream index of eaxc among the flows of kind, or -1
static inline int oru_eaxc_index(const oru_eaxc_map_t *map, oru_eaxc_kind_t kind, uint16_t eaxc)
{
  const uint8_t i = map->index[kind][eaxc & map->key_mask];
  return i == ORU_EAXC_NONE ? -1 : i;
}

// Lists for an O-DU without eAxC lists: the RU port (4 low bits) is the DL and UL stream index, and PRACH stream i is on
// RU port prach_eaxc_offset + i. No SRS.
oru_eaxc_lists_t oru_eaxc_ru_port_lists(int prach_eaxc_offset);
void oru_eaxc_map_init(oru_eaxc_map_t *map, const oru_eaxc_lists_t *lists);
// Flow of a C-Plane message: Section Type 3 is PRACH, Section Type 1 is DL, or SRS if eaxc is an SRS eAxC, else UL
oru_flow_t oru_eaxc_classify(const oru_eaxc_map_t *map, bool prach, bool dl, uint16_t eaxc);

typedef struct {
  uint64_t hist[HIST_SIZE];
  int64_t sum;
  uint64_t count;
} txrx_histogram_t;

// One continuous PRB run, decompressed. read_dl_iq_streams() returns these ungrouped - more than
// one stream can share an ant_id (multiple beams on one eaxc) or a beam_id (one beam split across
// sections). Placing/summing into the final per-antenna buffer is the caller's job.
typedef struct {
  uint8_t ant_id; // RU port / eaxc this fragment targets
  uint16_t beam_id; // beam this PRB run was declared under
  int start_prb;
  int num_prb;
  uint32_t *iq; // num_prb * NR_NB_SC_PER_RB samples (same packed format as txdataF elsewhere)
} dl_iq_stream_t;

typedef struct {
  int section_id;
  int comp_method;
  int iq_width;
} opaque_response_data_t;

typedef struct {
  opaque_response_data_t response_payload;
  uint64_t hyper_frame;
  int frame;
  int slot_in_frame;
  int symbol;
  int num_symbols;
  int antenna_id; // eaxc: physical RX antenna (passthrough) or beam output stream (codebook)
  uint16_t eaxc; // eAxC ID of the corresponding C-Plane message (ecpriRtcid)
  bool srs; // antenna_id is the physical RX antenna: SRS is sent per RX antenna and not beamformed, also in codebook mode
  uint16_t beam_id; // beam this section was declared under (UL Rx beamforming)
  int start_prb;
  int num_prb;
} ul_job_t;

typedef struct {
  uint64_t total_cplane;
  uint64_t cplane_received_dl;
  uint64_t cplane_received_ul;
  uint64_t cplane_received_prach;
  uint64_t cplane_received_srs; // UL Section Type 1 on an SRS eAxC, also counted in cplane_received_ul
  uint64_t cplane_received_other;
  uint64_t total_uplane_received;
  uint64_t total_uplane_sent;
  uint64_t cplane_err_hdr; // apphdr or section extraction
  uint64_t cplane_err_ver; // payloadVer
  uint64_t cplane_err_early;
  uint64_t cplane_err_late;
  uint64_t cplane_err_dup; // duplicate cplane
  uint64_t cplane_err_dup_dl;
  uint64_t cplane_err_dup_ul;
  uint64_t cplane_err_dup_prach;
  uint64_t cplane_section_repeated; // sectionId cited again within one message, ignored after its first citation
  uint64_t ul_cplane_err_invalid_num_symbols;
  uint64_t cplane_ext1_received;
  uint64_t cplane_ext11_received;
  uint64_t cplane_err_sect_ext1; // malformed ext1: bad extLen or doesn't decode
  uint64_t cplane_err_sect_ext11; // malformed ext11: bad extLen or doesn't decode
  uint64_t cplane_err_sect_ext_chain; // ef promises an extension but the packet ends
  uint64_t cplane_err_sect_ext_other; // malformed extension of another type: bad extLen
  uint64_t uplane_err_late;
  uint64_t uplane_err_early;
  uint64_t uplane_err_dup;
  uint64_t uplane_err_prb_range; // start_prbu/num_prbu from the wire fell outside [0, ctx->num_prb)
  uint64_t uplane_err_short_payload; // packet payload shorter than num_prbu/iqWidth/compMeth implied
  uint64_t uplane_missing_cplane;
  uint64_t dl_stream_pool_exhausted; // distinct (eaxc, beam) pairs for one DL symbol exceeded MAX_DL_STREAMS_PER_SYMBOL
  uint64_t dl_stream_sections_exhausted; // sections sharing one (eaxc, beam) stream exceeded MAX_SECTIONS_PER_DL_STREAM
  uint64_t dl_fragments_pool_exhausted; // PRB-run fragments for one DL symbol exceeded MAX_DL_FRAGMENTS_PER_SYMBOL or the IQ arena
  uint64_t invalid_eaxc_id; // eaxc/antenna id from a C-Plane or U-Plane packet was out of MAX_ANTENNAS range
  uint64_t application_too_slow;
  uint64_t dl_tdd_mismatch;
  uint64_t ul_tdd_mismatch;
  uint64_t out_of_mbufs;
  uint64_t ul_cplane_missing;
  uint64_t prach_cplane_missing;
  uint64_t prach_cplane_missing_ant;
  uint64_t prach_cplane_missing_inactive;
  uint64_t prach_cplane_missing_stale;
  uint64_t prach_cplane_missing_early;
  uint64_t prach_out_of_mbufs;
  uint64_t prach_jobs_pool_exhausted; // PRACH start symbols for one slot and stream exceeded MAX_PRACH_JOBS
  uint64_t prach_fdm_unsupported; // PRACH sections dropped as a second FDM occasion at the same start symbol
  uint64_t prach_sections_unserved; // PRACH sections released without any U-Plane sent, e.g. TDM occasions nr-oru doesn't extract
  txrx_histogram_t dl_uplane_hist;
  txrx_histogram_t dl_cplane_hist;
  txrx_histogram_t ul_cplane_hist;
  txrx_histogram_t prach_cplane_hist;
  int64_t ul_uplane_ota_delay_sum;
  uint64_t ul_uplane_ota_delay_count;
} oru_packet_processor_stats_t;

// Forward-declared rather than pulling in <rte_mbuf.h>: only used here as an opaque pointer type,
// and this header must stay includable by TUs (e.g. oru_beamforming.c) that don't otherwise need DPDK.
struct rte_mbuf;

typedef void *(*alloc_func_t)(void *io_controller);
typedef void (*send_func_t)(void *io_controller, struct rte_mbuf **mbufs, uint32_t num_mbufs);

void *init_packet_processor(int numerology,
                            int num_prb,
                            uint32_t T2a_cp_min_uS,
                            uint32_t T2a_cp_max_uS,
                            uint32_t T2a_up_min_uS,
                            uint32_t T2a_up_max_uS,
                            int num_dl_slots,
                            int num_ul_slots,
                            int num_dl_symbols,
                            int num_ul_symbols,
                            int tdd_pattern_length_slots,
                            void *(*alloc_mbuf)(void *io_controller),
                            void (*send_mbufs)(void *io_controller, struct rte_mbuf **mbufs, uint32_t num_mbufs),
                            void *io_controller,
                            size_t mtu,
                            const oru_eaxc_lists_t *eaxc_lists,
                            fh_comp_method_t comp_method,
                            int prach_kbar);
void write_ul_iq(void *context, uint32_t *rxdataF, int symbol, const ul_job_t *job);
void write_prach_iq(void *context, uint32_t **txdataF, int nb_rx, int frame, int slot_in_frame, int symbol);
void cleanup_packet_processor(void *context);
void set_num_bf_weights(void *context, int num_bf_weights);
// Static U-Plane compression, an M-Plane setting: comp_method in both directions at iq_width (PRACH: prach_iq_width,
// 0 = iq_width), with no udCompHdr in the U-Plane. iq_width 0 keeps the dynamic compression: DL takes the IQ width from
// the udCompHdr of each U-Plane section, and UL the method and IQ width from the C-Plane udCompHdr. cuBB always sends a
// C-Plane udCompHdr of 0.
void set_static_compression(void *context, int iq_width, int prach_iq_width);
void handle_absolute_symbol_tick(void *context, uint64_t absolute_symbol);
void handle_uplane_packet(void *context, void *pkt);
void handle_cplane_packet(void *context, void *pkt);
void print_packet_processor_stats(void *context);
void get_packet_processor_stats(void *context, oru_packet_processor_stats_t *out_stats);
// Dequeues the next ready DL symbol job into `streams`/`iq_arena` (caller-owned; iq_arena needs
// DL_IQ_ARENA_PRBS(num_prb) * NR_NB_SC_PER_RB uint32_t). Each output stream is one fragment. Returns stream count
// (0..max_streams; 0 is normal, not an error), or -1 if `context` is NULL.
int read_dl_iq_streams(void *context,
                       dl_iq_stream_t *streams,
                       uint32_t *iq_arena,
                       int max_streams,
                       uint64_t *hyper_frame,
                       int *frame,
                       int *slot,
                       int *symbol);
// Beam the DU declared (C-Plane section type 3 beamId) for PRACH stream `aarx` at `symbol` of `slot_in_frame`, or -1
// when no PRACH C-Plane section covers that symbol.
int get_prach_beam_id(void *context, int slot_in_frame, int aarx, int symbol);
int get_ready_job_count(void *context);
int poll_ul_job(void *context, ul_job_t *job);
void get_dl_symbol_bitmask(void *context, const uint8_t **bitmask, uint16_t *bit_length);

#ifdef __cplusplus
}
#endif
