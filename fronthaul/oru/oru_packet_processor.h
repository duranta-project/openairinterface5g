/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#pragma once

#include "fh_compression.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HIST_SIZE 64

typedef struct {
  uint64_t hist[HIST_SIZE];
  int64_t sum;
  uint64_t count;
} txrx_histogram_t;

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
  int antenna_id;
  int start_prb;
  int num_prb;
} ul_job_t;

typedef struct {
  uint64_t total_cplane;
  uint64_t cplane_received_dl;
  uint64_t cplane_received_ul;
  uint64_t cplane_received_prach;
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
  uint64_t uplane_err_late;
  uint64_t uplane_err_early;
  uint64_t uplane_err_dup;
  uint64_t uplane_missing_cplane;
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
  uint64_t prach_jobs_pool_exhausted;
  txrx_histogram_t dl_uplane_hist;
  txrx_histogram_t dl_cplane_hist;
  txrx_histogram_t ul_cplane_hist;
  txrx_histogram_t prach_cplane_hist;
  int64_t ul_uplane_ota_delay_sum;
  uint64_t ul_uplane_ota_delay_count;
} oru_packet_processor_stats_t;

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
                            int prach_eaxc_offset,
                            fh_comp_method_t dl_comp_method,
                            int prach_kbar);
void write_ul_iq(void *context, uint32_t *rxdataF, int symbol, const ul_job_t *job);
void write_prach_iq(void *context, uint32_t **txdataF, int nb_rx, int frame, int slot_in_frame, int symbol);
void cleanup_packet_processor(void *context);
void handle_absolute_symbol_tick(void *context, uint64_t absolute_symbol);
void handle_uplane_packet(void *context, void *pkt);
void handle_cplane_packet(void *context, void *pkt);
void print_packet_processor_stats(void *context);
void get_packet_processor_stats(void *context, oru_packet_processor_stats_t *out_stats);
void read_dl_iq(void *context, uint32_t **txdataF, int nb_tx, uint64_t *hyper_frame, int *frame, int *slot, int *symbol, uint16_t *beam_ids);
int get_ready_job_count(void *context);
int poll_ul_job(void *context, ul_job_t *job);
void get_dl_symbol_bitmask(void *context, const uint8_t **bitmask, uint16_t *bit_length);

#ifdef __cplusplus
}
#endif
