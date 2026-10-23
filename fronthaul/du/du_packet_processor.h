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
} du_histogram_t;

typedef struct {
  uint64_t total_uplane_received;
  uint64_t uplane_err_early;
  uint64_t uplane_err_late;
  uint64_t ul_tdd_mismatch;
  uint64_t application_too_slow;
  uint64_t out_of_mbufs;
  uint64_t expect_out_of_window; // UL expectation for a symbol already released, or too far ahead
  du_histogram_t ul_uplane_hist;
  
  uint64_t total_prach_uplane_received;
  uint64_t prach_uplane_err_early;
  uint64_t prach_uplane_err_late;
  uint64_t prach_missing_expect_job;
  uint64_t prach_expect_out_of_window; // PRACH occasion already past its release, or a frame ahead
  du_histogram_t prach_uplane_hist;
} du_packet_processor_stats_t;

// Creates a packet processor. Ta3_min_uS/Ta3_max_uS bound how long after OTA a UL U-Plane or PRACH
// packet is accepted. The TDD pattern repeats every tdd_pattern_length_slots slots and ends with
// num_ul_slots UL slots, preceded by num_ul_symbols UL symbols; in FDD every symbol is UL and the
// pattern is ignored. PRACH packets use eAxC ports from prach_eaxc_offset up. Returns the context.
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

// Prints the stats and frees the context.
void cleanup_du_packet_processor(void *context);

// Advances the clock to absolute_symbol. Releases, in symbol order, every UL symbol job whose Ta3
// window has closed, and an empty job for every UL symbol nothing was expected for once its OTA
// has passed. Releases every PRACH job whose last symbol's window has closed. Called once per
// symbol from the fronthaul timer.
void du_packet_processor_handle_absolute_symbol_tick(void *context, uint64_t absolute_symbol);

// Takes a UL U-Plane packet (mbuf starting at the eCPRI header). Keeps it if it arrives inside Ta3
// for a symbol and antenna announced with du_packet_processor_expect_ul_symbol(), otherwise frees
// it and counts why. A job is released early once all expected PRBs have arrived and every
// earlier UL symbol has been released, so jobs always come out in symbol order.
void du_packet_processor_handle_uplane_packet(void *context, void *pkt);

// Announces that num_prb PRBs of UL U-Plane will arrive for absolute_symbol on antenna ant_id,
// compressed with comp_method and iq_width; call it together with the UL C-Plane grant, before
// the symbol's Ta3 window opens. A symbol already released, too far ahead, or not UL is refused
// and counted. The first call for a symbol sets its compression. section_id and start_prb are not
// checked against the packets, and calling it twice for the same antenna counts the PRBs twice.
void du_packet_processor_expect_ul_symbol(void *context, uint64_t absolute_symbol, int ant_id, int section_id, int start_prb, int num_prb, fh_comp_method_t comp_method, uint8_t iq_width);

// Number of UL symbol jobs ready to be read.
int du_packet_processor_get_ready_ul_job_count(void *context);

// Reads the next ready UL symbol job into rxdataF (nb_rx antennas, num_prb PRBs each, zero where
// nothing arrived) and returns its time. Blocks until a job is ready.
void du_packet_processor_read_ul_iq(void *context, uint32_t **rxdataF, int nb_rx, uint64_t *hyper_frame, int *frame, int *slot, int *symbol);

// Announces a PRACH occasion of num_symbols symbols from start_absolute_symbol in slot_in_frame,
// for PRACH antenna ant_id (counted from 0, not the eAxC port), compressed with comp_method and
// iq_width. The sequence starts kbar values into the received PRBs. There is one occasion per
// slot_in_frame and antenna; a new call replaces the previous one. An occasion already past its
// release, or a frame or more ahead, is refused and counted. section_id, start_prb and num_prb
// are not checked against the packets.
void du_packet_processor_expect_prach_occasion(void *context, uint64_t start_absolute_symbol, int num_symbols, int slot_in_frame, int ant_id, int section_id, int start_prb, int num_prb, fh_comp_method_t comp_method, uint8_t iq_width, int kbar);

// Takes a PRACH U-Plane packet (mbuf starting at the eCPRI header). Adds its IQ to the announced
// occasion if it arrives inside Ta3 for one of the occasion's symbols, and frees it.
void du_packet_processor_handle_prach_uplane_packet(void *context, void *pkt);

// Number of PRACH jobs ready to be read.
int du_packet_processor_get_ready_prach_job_count(void *context);

// Reads the next ready PRACH job: the 139 accumulated subcarriers starting at kbar, as I/Q
// pairs, and its occasion's time, antenna and section. Blocks until a job is ready.
void du_packet_processor_read_prach_iq(void *context, int16_t *rxdata, uint64_t *hyper_frame, int *frame, int *slot, int *antenna, int *section_id);

// Copies the stats.
void du_packet_processor_get_stats(void *context, du_packet_processor_stats_t *out);

// Logs the stats and the receive timing histograms, then clears the histograms.
void du_packet_processor_print_stats(void *context);

#ifdef __cplusplus
}
#endif
