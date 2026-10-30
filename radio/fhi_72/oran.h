/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _ORAN_H_
#define _ORAN_H_

#include "common/utils/time_meas.h" // time_stats_t
#include "radio/COMMON/common_lib.h" // duplex_mode_t

struct PHY_VARS_gNB_s;

/*! \brief direction of a slot; bitmask, SLOT_DIR_MIXED = SLOT_DIR_DL | SLOT_DIR_UL */
typedef enum { SLOT_DIR_DL = 1, SLOT_DIR_UL = 2, SLOT_DIR_MIXED = 3 } slot_direction_t;

/*! \brief Split 7.2 configuration */
typedef struct split7_config {
  /*! duplexing mode */
  duplex_mode_t duplex_mode;
  /*! number of downlink resource blocks */
  int num_rb_dl;
  /*! number of TX channels (=TX antennas) */
  int tx_num_channels;
  /*! number of RX channels (=RX antennas) */
  int rx_num_channels;
  /*! number of distributed radio-units */
  int num_distributed_ru;
  /*! TX/RX bandwidth in Hz */
  double tx_bw;
  double rx_bw;
  /*! TX/RX center frequency in Hz, index: [0..tx/rx_num_channels[ */
  double tx_freq[OPENAIR0_MAX_ANTENNAS];
  double rx_freq[OPENAIR0_MAX_ANTENNAS];
  /*! TX gain in dB, index: [0..tx_num_channels[ */
  double tx_gain[OPENAIR0_MAX_ANTENNAS];
  /*! Common numerology */
  int mu;
  /*! number of slots per frame */
  int slots_per_frame;
  /*! number of slots TX is processed ahead of RX */
  int sl_ahead;
  /*! direction of each slot in a frame, index: [0..slots_per_frame[; FDD: all SLOT_DIR_MIXED */
  slot_direction_t slot_dir[160];
  /*! frequency-domain RX buffers, index: [0..rx_num_channels[; points to RU_t common.rxdataF */
  int32_t **rxdataF;
  /*! precoded frequency-domain TX buffers, index: [0..tx_num_channels[; points to RU_t common.txdataF_BF */
  int32_t **txdataF_BF;
  /*! PRACH index used for PRACH */
  int prach_index;
  /*! PRACH frequency start, from RRC's msg1-FrequencyStart */
  int prach_freq_start;
  /*! PRACH sequence offset in the PRACH PRBs, in int16 (2 per subcarrier): 2 * k_bar of TS 38.211 Table 6.3.3.2-1 */
  int prach_kbar;
  /*! the TDD period length, if TDD indicated */
  int n_tdd_period;
  /*! TDD frame structure, if TDD indicated */
  struct {
    symbol_direction_t sym_dir[14];
  } slot_dirs[160];
  /*! this is the exponent in 2^X for the FFT size */
  uint16_t fftSize;

  /*! this is the ceil of the exponent in 2^X for the prach FFT size */
  uint16_t prach_fftSize;
  // M-plane related parameters
  uint16_t dl_k0[5];
  uint16_t ul_k0[5];
  uint16_t cp_prefix0;
  uint16_t cp_prefix_other;

  time_stats_t prec_bf;
  time_stats_t dl_cp;
  time_stats_t dl_up;
  time_stats_t ul_cp;
  time_stats_t ul_up;

} split7_config_t;

/* split 7.2 fronthaul callbacks used by the gNB TX path (tx_func()) */
typedef struct fhi72_transport_s {
  split7_config_t s7cfg;
  pthread_t pthread;
  void (*prec_bf)(struct PHY_VARS_gNB_s *gNB, int frame, int slot);
  void (*dl_up)(split7_config_t *s7cfg, int frame, int slot);
  void (*cp)(split7_config_t *s7cfg, uint16_t **beam_id, int frame, int slot);
} fhi72_transport_t;

typedef int (*fhi72_init_t)(struct PHY_VARS_gNB_s *gNB);

#endif /* _ORAN_H_ */
