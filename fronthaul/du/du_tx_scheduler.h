#pragma once

#include "fh_compression.h"
#include "xran_pkt_api.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <rte_mbuf.h>

#ifdef __cplusplus
extern "C" {
#endif

// One PRB group within a DL symbol, with its own beam. A symbol may have several.
typedef struct {
  uint16_t beam_id;
  uint16_t start_prb;
  uint16_t num_prb;
  uint16_t section_id;
} du_tx_dl_section_t;

// Section type 3 (PRACH) C-Plane fields, O-RAN CUS 7.5.2
typedef struct {
  uint8_t filter_index;
  uint16_t time_offset;
  uint8_t fft_size; // exponent of 2
  uint8_t scs;
  uint16_t section_id;
  uint16_t beam_id;
  uint8_t num_symbol;
  uint16_t start_prb;
  uint8_t num_prb;
  int32_t freq_offset; // in half PRACH subcarriers relative to the channel center
} du_tx_prach_section_t;

typedef void *(*du_tx_alloc_func_t)(void *io_controller);
typedef void (*du_tx_send_func_t)(void *io_controller, struct rte_mbuf **mbufs, uint32_t num_mbufs);

void *init_du_tx_scheduler(int numerology,
                           int num_prb,
                           uint32_t T1a_up_min_uS,
                           uint32_t T1a_up_max_uS,
                           uint32_t T1a_cp_dl_min_uS,
                           uint32_t T1a_cp_dl_max_uS,
                           uint32_t T1a_cp_ul_min_uS,
                           uint32_t T1a_cp_ul_max_uS,
                           size_t mtu,
                           fh_comp_method_t comp_method,
                           uint8_t iq_width,
                           struct xran_eaxcid_config eaxcid_config,
                           du_tx_alloc_func_t alloc_func,
                           du_tx_send_func_t send_func,
                           void *io_controller);

void cleanup_du_tx_scheduler(void *context);

// Called from fh_timer's per-symbol callback (fh_timer_register_cb) to drive dispatch.
void du_tx_handle_absolute_symbol_tick(void *context, uint64_t absolute_symbol);

// Enqueues DL IQ for one symbol, to be sent later at the correct T1a-windowed time.
// txdataF: one buffer per antenna, each holding the full num_prb worth of frequency-domain IQ.
// sections: the PRB-group/beam list for this symbol (num_sections entries).
// Returns immediately; does not send synchronously.
void du_tx_schedule_dl_iq(void *context,
                          uint32_t **txdataF,
                          int nb_tx,
                          uint64_t hyper_frame,
                          int frame,
                          int slot,
                          int symbol,
                          const du_tx_dl_section_t *sections,
                          int num_sections);

void du_tx_schedule_ul_grant(void *context,
                             uint64_t hyper_frame,
                             int frame,
                             int slot,
                             int start_symbol,
                             int ant_id,
                             const du_tx_dl_section_t *sections,
                             int num_sections);

// Schedule a PRACH (section type 3) C-Plane message T1a_cp_ul ahead of the occasion.
void du_tx_schedule_prach(void *context,
                          uint64_t hyper_frame,
                          int frame,
                          int slot,
                          int start_symbol,
                          int ant_id,
                          const du_tx_prach_section_t *prach);

// Stats, mirroring the shape of du_packet_processor_stats_t.
typedef struct {
  uint64_t total_dl_scheduled;
  uint64_t total_dl_sent;
  uint64_t app_too_late_tx;
  uint64_t out_of_mbufs;
  uint64_t total_cplane_dl_sent;
  uint64_t total_cplane_ul_sent;
  uint64_t dl_cplane_too_late;
  uint64_t ul_grant_too_late;
} du_tx_scheduler_stats_t;

void du_tx_get_stats(void *context, du_tx_scheduler_stats_t *out);
void du_tx_print_stats(void *context);

#ifdef __cplusplus
}
#endif
