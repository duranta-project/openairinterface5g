/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef __DU_FH_H__
#define __DU_FH_H__

#include "du_io.h"
#include <stdint.h>
#include <stdbool.h>
#include "fh_compression.h"
#include "du_packet_processor.h"
#include "du_tx_scheduler.h"

/**
 * DPDK Configuration, mirroring oru_fh_dpdk_config_t
 */
typedef struct {
  char *dpdk_devices[MAX_RU_PORTS];
  int num_dpdk_devices;
  char **extra_eal_args;
  int num_extra_eal_args;
} du_fh_dpdk_config_t;

/**
 * TDD Pattern configuration, mirroring oru_fh_tdd_pattern_t
 */
typedef struct {
  uint32_t num_ul_slots;
  uint32_t num_dl_slots;
  uint32_t num_ul_symbols;
  uint32_t num_dl_symbols;
  uint32_t tdd_pattern_length_slots;
} du_fh_tdd_pattern_t;

/**
 * DU Fronthaul Configuration
 */
typedef struct {
  fh_comp_method_t comp_type;
  uint8_t iq_width; // 0 defaults to 16 (uncompressed); set explicitly when comp_type != FH_COMP_NONE
  int numerology;
  uint16_t num_prbs;
  uint16_t mtu;
  char *ru_mac_addrs[MAX_RU_PORTS];
  int num_ru_mac_addrs;
  du_fh_dpdk_config_t dpdk_conf;
  int worker_core;
  bool fdd_mode;
  du_fh_tdd_pattern_t tdd_pattern;
  uint32_t T1a_cp_dl_min_uS;
  uint32_t T1a_cp_dl_max_uS;
  uint32_t T1a_cp_ul_min_uS;
  uint32_t T1a_cp_ul_max_uS;
  uint32_t T1a_up_min_uS;
  uint32_t T1a_up_max_uS;
  uint32_t Ta3_min_uS;
  uint32_t Ta3_max_uS;
  int prach_eaxc_offset;
  int prach_kbar;
} du_fh_config_t;

/**
 * @brief Initialize the O-DU Fronthaul interface.
 *
 * @param cfg Pointer to the fronthaul configuration structure.
 * @return void* Pointer to the initialized fronthaul handle, or NULL on failure.
 */
void *du_fh_init(du_fh_config_t *cfg);

/**
 * @brief Clean up and release resources used by the O-DU Fronthaul interface.
 *
 * @param handle Pointer to the fronthaul handle.
 */
void du_fh_cleanup(void *handle);

/**
 * @brief Start the O-DU Fronthaul processing threads and loops.
 *
 * @param handle Pointer to the fronthaul handle.
 * @return int 0 on success, negative on error.
 */
int du_fh_start(void *handle);

/**
 * @brief Stop the O-DU Fronthaul processing.
 *
 * @param handle Pointer to the fronthaul handle.
 */
void du_fh_stop(void *handle);

/**
 * @brief Schedule DL IQ data to be sent at the correct time.
 *
 * @param handle Pointer to the fronthaul handle.
 * @param txdataF Array of pointers to buffers containing the frequency-domain IQ samples (per TX antenna).
 * @param nb_tx Number of TX antennas.
 * @param hyper_frame Absolute GPS hyper-frame number.
 * @param frame Frame number.
 * @param slot Slot number.
 * @param symbol Symbol number.
 * @param sections Array of PRB-group/beam sections.
 * @param num_sections Number of sections.
 */
void du_fh_tx_send_dl_iq(void *handle, uint32_t **txdataF, int nb_tx, uint64_t hyper_frame, int frame, int slot, int symbol, const du_tx_dl_section_t *sections, int num_sections);

/**
 * @brief Schedule an UL grant (C-Plane) to be sent to the RU.
 *
 * @param handle Pointer to the fronthaul handle.
 * @param hyper_frame Absolute GPS hyper-frame number.
 * @param frame Target frame number.
 * @param slot Target slot number.
 * @param start_symbol Start symbol for the UL grant.
 * @param ant_id Antenna ID.
 * @param sections Array of PRB-group/beam sections.
 * @param num_sections Number of sections.
 */
void du_fh_schedule_ul_grant(void *handle, uint64_t hyper_frame, int frame, int slot, int start_symbol, int ant_id, const du_tx_dl_section_t *sections, int num_sections);

/**
 * @brief Inform the packet processor to expect UL symbol data. This is a companion
 *        call to du_fh_schedule_ul_grant; both are made by the MAC scheduler together.
 *
 * @param handle Pointer to the fronthaul handle.
 * @param absolute_symbol Target absolute symbol index.
 * @param ant_id Antenna ID.
 * @param section_id Section ID.
 * @param start_prb Starting PRB.
 * @param num_prb Number of PRBs.
 * @param comp_method Compression method.
 * @param iq_width IQ width.
 */
void du_fh_expect_ul_symbol(void *handle, uint64_t absolute_symbol, int ant_id, int section_id, int start_prb, int num_prb, fh_comp_method_t comp_method, uint8_t iq_width);

/**
 * @brief Inform the packet processor to expect PRACH occasions.
 *
 * @param handle Pointer to the fronthaul handle.
 * @param start_absolute_symbol Starting absolute symbol.
 * @param num_symbols Number of symbols for PRACH.
 * @param slot_in_frame Slot index in the frame.
 * @param ant_id Antenna ID.
 * @param section_id Section ID.
 * @param start_prb Starting PRB.
 * @param num_prb Number of PRBs.
 * @param comp_method Compression method.
 * @param iq_width IQ width.
 * @param kbar PRACH kbar parameter.
 */
void du_fh_expect_prach_occasion(void *handle, uint64_t start_absolute_symbol, int num_symbols, int slot_in_frame, int ant_id, int section_id, int start_prb, int num_prb, fh_comp_method_t comp_method, uint8_t iq_width, int kbar);

/**
 * @brief Get the number of ready UL jobs.
 *
 * @param handle Pointer to the fronthaul handle.
 * @return int Number of ready UL jobs.
 */
int du_fh_get_ready_ul_job_count(void *handle);

/**
 * @brief Read UL symbol data (PUSCH) from the RX reassembly window.
 *
 * @param handle Pointer to the fronthaul handle.
 * @param rxdataF Array of pointers to buffers to store the received frequency-domain IQ samples (per RX antenna).
 * @param nb_rx Number of RX antennas.
 * @param hyper_frame Pointer to store the hyper-frame number.
 * @param frame Pointer to store the frame number.
 * @param slot Pointer to store the slot number.
 * @param symbol Pointer to store the symbol number.
 */
void du_fh_read_ul_iq(void *handle, uint32_t **rxdataF, int nb_rx, uint64_t *hyper_frame, int *frame, int *slot, int *symbol);

/**
 * @brief Get the number of ready PRACH jobs.
 *
 * @param handle Pointer to the fronthaul handle.
 * @return int Number of ready PRACH jobs.
 */
int du_fh_get_ready_prach_job_count(void *handle);

/**
 * @brief Read PRACH data from the RX reassembly window.
 *
 * @param handle Pointer to the fronthaul handle.
 * @param rxdata Buffer to store PRACH IQ samples.
 * @param hyper_frame Pointer to store hyper-frame.
 * @param frame Pointer to store frame.
 * @param slot Pointer to store slot.
 * @param antenna Pointer to store antenna ID.
 * @param section_id Pointer to store section ID.
 */
void du_fh_read_prach_iq(void *handle, int16_t *rxdata, uint64_t *hyper_frame, int *frame, int *slot, int *antenna, int *section_id);

/**
 * @brief Get the UTC anchor point mapping between 5G time and system time.
 *
 * @param handle Pointer to the fronthaul handle.
 * @param hyper_frame Pointer to store the reference hyperframe number (1024 frames each)
 * @param frame Pointer to store the reference frame number.
 * @param slot Pointer to store the reference slot number.
 * @param ts Pointer to a timespec structure to store the corresponding system time.
 * @return 0 on success, negative on error.
 */
int du_fh_get_utc_anchor_point(void *handle, uint64_t *hyper_frame, uint32_t *frame, uint32_t *slot, struct timespec *ts);

/**
 * @brief Get statistics for both RX (packet processor) and TX (scheduler).
 *
 * @param handle Pointer to the fronthaul handle.
 * @param rx_stats Pointer to store RX stats.
 * @param tx_stats Pointer to store TX stats.
 */
void du_fh_get_stats(void *handle, du_packet_processor_stats_t *rx_stats, du_tx_scheduler_stats_t *tx_stats);

/**
 * @brief Print statistics to stdout.
 *
 * @param handle Pointer to the fronthaul handle.
 */
void du_fh_print_stats(void *handle);

#endif
