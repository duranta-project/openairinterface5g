#pragma once

#include "fh_compression.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

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
  uint64_t total_uplane_received;
  uint64_t uplane_err_early;
  uint64_t uplane_err_late;
  uint64_t ul_tdd_mismatch;
  uint64_t application_too_slow;
  uint64_t out_of_mbufs;
  txrx_histogram_t ul_uplane_hist;
  
  uint64_t total_prach_uplane_received;
  uint64_t prach_uplane_err_early;
  uint64_t prach_uplane_err_late;
  uint64_t prach_missing_expect_job;
  txrx_histogram_t prach_uplane_hist;
} du_packet_processor_stats_t;

void *init_du_packet_processor(int numerology,
                               int num_prb,
                               uint32_t Ta3_min_uS,
                               uint32_t Ta3_max_uS,
                               int num_ul_slots,
                               int num_ul_symbols,
                               int tdd_pattern_length_slots,
                               bool fdd_mode,
                               size_t mtu,
                               int prach_eaxc_offset);

void cleanup_du_packet_processor(void *context);

void du_pp_handle_absolute_symbol_tick(void *context, uint64_t absolute_symbol);

void du_pp_handle_uplane_packet(void *context, void *pkt);

void du_pp_expect_ul_symbol(void *context, uint64_t absolute_symbol, int ant_id, int section_id, int start_prb, int num_prb, fh_comp_method_t comp_method, uint8_t iq_width);

int du_pp_get_ready_ul_job_count(void *context);

void du_pp_read_ul_iq(void *context, uint32_t **rxdataF, int nb_rx, uint64_t *hyper_frame, int *frame, int *slot, int *symbol);

// Non-blocking: reads the next ready UL symbol job only if its absolute symbol is <= last_absolute_symbol.
// Returns false if no job is ready or the next one is past the bound (it is kept for the next call).
bool du_pp_read_ul_iq_upto(void *context, uint32_t **rxdataF, int nb_rx, uint64_t last_absolute_symbol, uint64_t *absolute_symbol);

// Symbols after OTA at which a UL symbol's receive window closes and its job becomes ready (Ta3 max).
uint32_t du_pp_get_ul_window_symbols(void *context);

void du_pp_expect_prach_occasion(void *context, uint64_t start_absolute_symbol, int num_symbols, int slot_in_frame, int ant_id, int section_id, int start_prb, int num_prb, fh_comp_method_t comp_method, uint8_t iq_width, int kbar);

void du_pp_handle_prach_uplane_packet(void *context, void *pkt);

int du_pp_get_ready_prach_job_count(void *context);

void du_pp_read_prach_iq(void *context, int16_t *rxdata, uint64_t *hyper_frame, int *frame, int *slot, int *antenna, int *section_id);

void du_pp_get_stats(void *context, du_packet_processor_stats_t *out);

void du_pp_print_stats(void *context);

#ifdef __cplusplus
}
#endif
