/**
 * @file ue_split7_interface.h
 * @brief C API for the UE-centric 7.1 functional split: Low-PHY (CP/FFT, RF side)
 *        vs. High-PHY (frequency-domain processing, host side).
 */

#ifndef UE_SPLIT7_INTERFACE_H
#define UE_SPLIT7_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque: avoids pulling in the whole PHY include tree just for a pointer field. */
struct NR_DL_FRAME_PARMS_s;
typedef struct NR_DL_FRAME_PARMS_s NR_DL_FRAME_PARMS;

/* ========================================================================== */
/*                         COMMON DEFINITIONS & TYPES                         */
/* ========================================================================== */

/**
 * @brief Return status codes for the 7.1 split interface.
 */
typedef enum {
  UE_SPLIT7_SUCCESS = 0, ///< Operation completed successfully
  UE_SPLIT7_ERR_GENERIC = -1, ///< Generic error
  UE_SPLIT7_ERR_INVALID_PARAM = -2, ///< Invalid parameter passed
  UE_SPLIT7_ERR_BUSY = -3, ///< Interface is busy (e.g., sync already running)
  UE_SPLIT7_ERR_TIMEOUT = -4, ///< Operation timed out
  UE_SPLIT7_ERR_STATE = -5, ///< Invalid state for this operation
  UE_SPLIT7_ERR_NO_MEMORY = -6 ///< Out of memory or buffer too small
} ue_split7_status_t;

/**
 * @brief Configuration parameters for the 7.1 functional split.
 */
typedef struct {
  uint64_t dl_carrier_freq_hz; ///< Downlink center frequency in Hz
  uint64_t ul_carrier_freq_hz; ///< Uplink center frequency in Hz
  uint32_t sample_rate_hz; ///< ADC/DAC sampling rate in Hz
  uint16_t fft_size; ///< FFT size (e.g., 512, 1024, 2048, 4096)
  uint16_t num_tx_antennas; ///< Number of TX antenna ports
  uint16_t num_rx_antennas; ///< Number of RX antenna ports
  uint16_t cp_len_normal; ///< Cyclic Prefix length in samples for normal symbols
  uint16_t cp_len_symbol0; ///< Cyclic Prefix length in samples for symbol 0 of a slot
  uint8_t scs_khz; ///< Subcarrier Spacing in kHz (15, 30, 60, 120)
  uint16_t nr_band; ///< Operating 3GPP NR frequency band (e.g. n257/n258/n260/n261 exceed 255)
  uint16_t N_RB_DL; ///< Number of downlink resource blocks

  /* Required. Host's already-computed frame params (already run through
   * nr_init_frame_parms_ue()/init_symbol_rotation()), reused as-is so the Low-PHY
   * can't numerically drift from the monolithic UE. configure() rejects NULL --
   * there is no standalone derivation fallback in this device; callers that need
   * one (e.g. unit tests) must build a valid NR_DL_FRAME_PARMS themselves. */
  const NR_DL_FRAME_PARMS *frame_parms;
} ue_split7_config_t;

/**
 * @brief Complex IQ sample representation in frequency domain.
 */
typedef struct {
  int16_t r; ///< Real (In-phase) component
  int16_t i; ///< Imaginary (Quadrature) component
} ue_split7_iq_t;

/**
 * @brief Metadata associated with an OFDM symbol.
 */
typedef struct {
  uint64_t timestamp_samples; ///< Absolute time in samples (serving cell timing grid)
  uint32_t frame_number; ///< System Frame Number (SFN, 0..1023)
  uint16_t slot_number; ///< Slot index within the frame
  uint8_t symbol_number; ///< Symbol index within the slot (0..13 for normal CP)
  uint8_t antenna_port; ///< Logical antenna port ID
  uint32_t flags; ///< Control flags (e.g. PRACH, beamforming settings)
} ue_split7_symbol_meta_t;

/**
 * @brief Container for frequency-domain REs of a single OFDM symbol.
 *
 * For read_symbol(): re_buffer/num_subcarriers are OUTPUT-only. The device owns the
 * buffer re_buffer points to and reuses it on the next read_symbol() call, so a caller
 * that needs the data to outlive that call must copy it out first.
 * For write_symbol(): re_buffer is the caller-owned input buffer to transmit.
 */
typedef struct {
  ue_split7_symbol_meta_t meta; ///< Metadata for the symbol (slot/symbol number are caller input to read_symbol())
  uint32_t num_subcarriers; ///< Number of active subcarriers (FFT size or bandwidth part)
  ue_split7_iq_t *re_buffer; ///< Frequency-domain REs [num_subcarriers]
} ue_split7_symbol_buffer_t;

/* ========================================================================== */
/*                       1. SYNCHRONIZATION SERVICE                           */
/* ========================================================================== */

/**
 * @brief Configuration for the cell search and synchronization process.
 */
typedef struct {
  uint32_t arfcn; ///< ARFCN of the channel to search/sync
  uint16_t scs_khz; ///< SCS of the SSB to search (15, 30, 120, 240 kHz)
  uint8_t ssb_pattern; ///< SSB pattern type (Case A, B, C, D, E)
  uint64_t ssb_bitmap; ///< Bitmap indicating transmitted SSBs in the burst (up to 64)
  uint8_t ssb_periodicity_ms; ///< Periodicity of SSB burst (5, 10, 20, 40, 80, 160 ms)
  uint32_t timeout_ms; ///< Maximum duration to search before timing out
  int16_t expected_pci; ///< Physical Cell ID to search for (-1 to search for any)
} ue_split7_sync_config_t;

/**
 * @brief Output results of a successful cell synchronization.
 */
typedef struct {
  uint16_t physical_cell_id; ///< Detected Physical Cell ID (0..1007)
  int32_t freq_offset_hz; ///< Carrier Frequency Offset (CFO) in Hz
  int64_t timing_offset_samples; ///< Sample offset relative to the start of the search window
  uint8_t best_ssb_index; ///< Decoded SSB index (0..63) carrying the MIB below
  float ssb_rsrp_dbm; ///< Measured RSRP of the best SSB
  float ssb_rsrq_db; ///< Measured RSRQ of the best SSB
  float ssb_rssi_dbm; ///< Measured RSSI of the best SSB

  /* SUCCESS only ever means PBCH decoded (not just PSS/SSS) -- a UE with only
   * PSS/SSS has no SFN and can't proceed to RRC, so the Low-PHY keeps searching. */
  bool mib_decoded; ///< Always true when status == UE_SPLIT7_SUCCESS
  uint8_t mib_payload[3]; ///< Raw decoded MIB payload bits (BCCH-BCH PDU, <=3 bytes)
  uint8_t mib_additional_bits; ///< Extra bits: 4 LSB of SFN, half-frame bit, SSB-subcarrier-offset MSB
  int32_t half_frame_bit; ///< Half-frame bit determined during PBCH detection
  int32_t symbol_offset; ///< PBCH DM-RS symbol offset determined during PBCH detection
  uint16_t ssb_start_subcarrier; ///< Actual SSB subcarrier-0 offset the Low-PHY's GSCN scan landed
                                 ///< on -- may differ from any value the host assumed before sync;
                                 ///< the host must adopt this for correct CORESET0/SIB1 location.

  /* Search runs against an already-captured window but takes real wall-clock time,
   * during which the live stream (separate thread) moves ahead; the Low-PHY's read
   * pointer is advanced to match (sync_task_func()). This is that advance in whole
   * frames (mod 1024) -- the host must apply it to the decoded SFN too, since both
   * now refer to the caught-up frame, not the one the MIB bits were decoded from. */
  uint32_t frames_since_capture;
} ue_split7_sync_result_t;

struct ue_split7_device;

/**
 * @brief Callback for reporting synchronization results asynchronously.
 *
 * @param dev Pointer to the split 7 device.
 * @param status Status of the sync procedure (e.g. SUCCESS, TIMEOUT).
 * @param result Pointer to the sync results (valid if status == SUCCESS).
 * @param user_data User context pointer.
 */
typedef void (*ue_split7_sync_callback_t)(struct ue_split7_device *dev,
                                          ue_split7_status_t status,
                                          const ue_split7_sync_result_t *result,
                                          void *user_data);

/**
 * @brief Parameters for transmitting an unaligned frequency-domain channel (e.g., PRACH).
 */
typedef struct {
  const ue_split7_iq_t *samples; ///< Array of frequency-domain IQ samples representing the preamble
  uint32_t num_samples; ///< Length of the samples array (number of subcarriers)
  uint32_t fft_size; ///< FFT size for PRACH modulator (e.g. 2048, 6144, 24576)
  int32_t time_offset_samples; ///< Time offset in samples relative to the slot/symbol boundary
  uint32_t cp_len_samples; ///< Cyclic Prefix length in samples to prepend
  uint64_t timestamp_samples; ///< Absolute device-timeline timestamp of the slot start (same
                              ///< clock as ue_split7_symbol_buffer_t::meta.timestamp_samples;
                              ///< NOT reconstructible from slot_number, which recurs every frame).
  uint32_t slot_number; ///< Slot coordinate (informational; NOT a substitute for timestamp_samples)
  uint8_t symbol_number; ///< Symbol coordinate
  int32_t frequency_offset_scs; ///< Frequency offset in subcarriers relative to DC subcarrier
  uint8_t repetition_count; ///< Number of preamble repetitions
  uint8_t antenna_port; ///< TX antenna port ID
} ue_split7_prach_tx_params_t;

/* ========================================================================== */
/*                      2. 7.1 DEVICE STRUCTURE DEFINITION                    */
/* ========================================================================== */

/** @brief A split 7 UE device: config plus the interface's function pointers. */
typedef struct ue_split7_device {
  ue_split7_config_t config; ///< Active configuration of the device
  void *priv; ///< Private driver state pointer (for USRP/hardware specific data)

  /** @brief Initialize/configure the device. Must be called before start(). */
  ue_split7_status_t (*configure)(struct ue_split7_device *dev, const ue_split7_config_t *config);

  /** @brief Start symbol-based RF TX/RX. Device must be synchronized first. */
  ue_split7_status_t (*start)(struct ue_split7_device *dev);

  /** @brief Stop RF transmission and reception. */
  ue_split7_status_t (*stop)(struct ue_split7_device *dev);

  /* ---------------------- Normal Symbol-Based API ---------------------- */

  /**
   * @brief Receive one OFDM symbol's frequency-domain REs (CP removal + FFT done internally). Blocking.
   * @param buffers One entry per active RX antenna: buffers[antenna_idx]. Caller sets meta.slot_number/
   *        symbol_number as input; on success the device fills meta.timestamp_samples, num_subcarriers,
   *        and re_buffer with a pointer into the device's own buffer (see ue_split7_symbol_buffer_t) --
   *        a future revision may let callers read that pointer directly instead of copying it out.
   * @param num_buffers Must equal dev->config.num_rx_antennas.
   */
  ue_split7_status_t (*read_symbol)(struct ue_split7_device *dev, ue_split7_symbol_buffer_t *buffers, uint16_t num_buffers);

  /**
   * @brief Transmit one OFDM symbol's frequency-domain REs (IFFT + CP done internally).
   * @param buffers One entry per active TX antenna: buffers[antenna_idx]. Each
   *        buffers[i].re_buffer MUST contain exactly dev->config.fft_size REs (the full
   *        FFT bin count, not a smaller BWP-sized buffer) -- passing fewer triggers an
   *        out-of-bounds read in the IDFT path. The implementation applies the mandatory
   *        38.211 §5.3 phase rotation to re_buffer IN PLACE, mutating the caller-supplied
   *        buffer; callers must not reuse/inspect it afterwards expecting the original
   *        (pre-rotation) contents.
   * @param num_buffers Must equal dev->config.num_tx_antennas.
   */
  ue_split7_status_t (*write_symbol)(struct ue_split7_device *dev, const ue_split7_symbol_buffer_t *buffers, uint16_t num_buffers);

  /* ---------------------- Synchronization Service ---------------------- */

  /**
   * @brief Trigger async time-domain cell search (PSS/SSS/PBCH); callback reports the result.
   * @param sync_config Frequency, pattern, SCS, expected PCI, timeout.
   */
  ue_split7_status_t (*start_sync)(struct ue_split7_device *dev,
                                   const ue_split7_sync_config_t *sync_config,
                                   void *pool,
                                   ue_split7_sync_callback_t callback,
                                   void *user_data);

  /** @brief Abort an ongoing synchronization search. */
  ue_split7_status_t (*stop_sync)(struct ue_split7_device *dev);

  /** @brief Apply a Timing Advance (from RAR/MAC-CE) to subsequent UL symbol TX. */
  ue_split7_status_t (*set_timing_advance)(struct ue_split7_device *dev, uint32_t ta_samples);

  /**
   * @brief Nudge the RX/TX sample-timing reference by sample_shift_samples.
   *
   * Call once per frame from the High-PHY's PBCH timing-error measurement
   * (nr_adjust_synch_ue()) to track drift between the Low-PHY's nominal
   * per-symbol window and the cell's real sample clock -- split7's equivalent
   * of the monolithic UE's readBlockSize/writeBlockSize adjustment. Without
   * it the RX FFT window drifts until it straddles a symbol boundary,
   * surfacing as intermittent PBCH/PDCCH decode failures.
   *
   * @param sample_shift_samples Same sign/value as the monolithic UE's
   *        shiftForNextFrame; pass nr_adjust_synch_ue()'s return through unmodified.
   */
  ue_split7_status_t (*adjust_rx_timing)(struct ue_split7_device *dev, int32_t sample_shift_samples);

  /* --------------------- Host-decoupled Slot Timing --------------------- */

  /**
   * @brief Seed the frame number for the next wait_next_slot() call.
   *
   * Call once, right after start_sync()/re-sync, with the MIB-decoded SFN
   * (already corrected for frames_since_capture). From then on the Low-PHY
   * tracks frame/slot numbering itself from elapsed samples.
   *
   * @param frame_number SFN (0..1023) of the frame at slot 0 symbol 0, at
   *        the sample position sync_task_func() anchored the read pointer to.
   */
  ue_split7_status_t (*seed_slot_tracking)(struct ue_split7_device *dev, uint32_t frame_number);

  /**
   * @brief Block until the next slot's samples have arrived; report its absolute frame/slot.
   *
   * Call once per slot instead of the host incrementing its own counter --
   * the Low-PHY owns the sample clock (circ_read_idx/circ_write_idx) and is
   * the sole source of truth for slot number, so this can't silently drift
   * the way an independent counter could. Must be called in slot order,
   * after seed_slot_tracking() has anchored the first slot.
   *
   * @param frame_number Output: SFN (0..1023) of the now-available slot.
   * @param slot_number Output: slot index within that frame.
   */
  ue_split7_status_t (*wait_next_slot)(struct ue_split7_device *dev, uint32_t *frame_number, uint16_t *slot_number);

  /** @brief Transmit an unaligned frequency-domain signal (e.g. PRACH preamble). */
  ue_split7_status_t (*write_prach)(struct ue_split7_device *dev, const ue_split7_prach_tx_params_t *params);

} ue_split7_device_t;

/**
 * @brief Allocate and initialise a concrete ue_split7_device_t implementation.
 * @return Pointer to the device, or NULL on allocation failure.
 */
ue_split7_device_t *ue_split7_device_create(void);
void ue_split7_device_free(ue_split7_device_t *dev);

#ifdef __cplusplus
}
#endif

#endif // UE_SPLIT7_INTERFACE_H
