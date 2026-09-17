/**
 * @file ue_split7_interface.h
 * @brief C API for the UE-centric 7.1 functional split: Low-PHY (CP/FFT, RF side)
 *        vs. High-PHY (frequency-domain processing, host side).
 */

#ifndef UE_SPLIT7_INTERFACE_H
#define UE_SPLIT7_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>
#include "common/platform_types.h" // c16_t: a leaf header (stdint/stdbool only), not the PHY include tree

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
  uint32_t frame_number; ///< Frame number (wrapped 0..1023 or absolute; resolved to closest frame by device)
  uint32_t slot_number; ///< Slot index within the frame
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
  c16_t **rx_td_buffers; ///< Device-owned RX time-domain frame buffers [num_rx_antennas]
  c16_t **rx_fd_buffers; ///< Device-owned RX frequency-domain ring buffers [num_rx_antennas]

  /** @brief Initialize/configure the device. Must be called before start(). */
  ue_split7_status_t (*configure)(struct ue_split7_device *dev, const ue_split7_config_t *config);

  /** @brief Start symbol-based RF TX/RX. Device must be synchronized first. */
  ue_split7_status_t (*start)(struct ue_split7_device *dev);

  /** @brief Stop RF transmission and reception. */
  ue_split7_status_t (*stop)(struct ue_split7_device *dev);

  /* ---------------------- Normal Symbol-Based API ---------------------- */

  /**
   * @brief Receive num_symbols consecutive OFDM symbols' frequency-domain REs (CP removal + FFT + 38.211 §5.3 symbol rotation done internally). Blocking.
   *
   * The device allocates and owns the time-domain frame buffer (see dev->rx_td_buffers)
   * and the frequency-domain sample buffers (see dev->rx_fd_buffers). Thread-safe. FFT + rotation is performed
   * once per symbol into the FD buffer and cached; subsequent requests for the same symbol return the cached FD
   * data without recomputing.
   *
   * If wait_next_slot() has already reported this (frame_number, slot_number), the
   * time-domain RX already happened there -- this call only does the FFT, no
   * blocking RX. Otherwise (a standalone caller that never calls wait_next_slot(),
   * or a request for a slot it hasn't reported) it receives the requested symbols
   * itself first, exactly as if wait_next_slot() didn't exist.
   *
   * @param frame_number Frame number (wrapped 0..1023 or absolute; resolved to closest frame by device).
   * @param slot_number Slot index within frame_number.
   * @param start_symbol First symbol index (0..13) of the contiguous batch.
   * @param num_symbols Number of consecutive symbols starting at start_symbol.
   * @param buffers Optional output: array of pointers (size num_buffers). On success, if non-NULL,
   *        buffers[antenna_idx] is populated with a pointer into the device-owned frequency-domain
   *        buffer for the start of the batch (num_symbols * dev->config.fft_size REs).
   * @param num_buffers Must equal dev->config.num_rx_antennas.
   */
  ue_split7_status_t (*read_symbols)(struct ue_split7_device *dev,
                                     uint32_t frame_number,
                                     uint16_t slot_number,
                                     uint8_t start_symbol,
                                     uint8_t num_symbols,
                                     c16_t **buffers,
                                     uint16_t num_buffers);

  /**
   * @brief Contribute num_symbols consecutive OFDM symbols' frequency-domain REs
   *        for client_id (IFFT + CP done internally once all clients have contributed).
   *
   * Multiple independent, spatially co-located UE instances ("clients", see
   * register_client()) can share one device: this call sums client_id's REs,
   * per symbol, into a device-owned FD accumulator instead of transmitting them
   * directly. Once every client register_client() has ever handed an id to --
   * that has also called seed_slot_tracking() at least once -- has contributed
   * to a given symbol (via this call or skip_symbols()), that symbol's combined
   * REs get the mandatory 38.211 §5.3 phase rotation, IDFT, and CP applied once,
   * and are transmitted. A single-client caller sees no observable difference
   * from a device with no other clients registered.
   *
   * IFFT + CP output lands in a device-owned 2-frame TX time-domain buffer
   * (same 2-frame-ring convention as the RX side's rx_td_buffers) rather than a
   * fresh per-call allocation. A small mutex serializes the accumulate-and-maybe-
   * finalize step across clients; this call never blocks waiting for other
   * clients to contribute -- whichever call happens to complete the set does the
   * finalize work itself before returning, everyone else returns immediately
   * after its own REs are consumed.
   *
   * @param client_id Id returned by register_client().
   * @param frame_number Frame number (wrapped 0..1023 or absolute; resolved to closest frame by device).
   * @param slot_number Slot index within frame_number.
   * @param start_symbol First symbol index (0..13) of the contiguous batch.
   * @param num_symbols Number of consecutive symbols starting at start_symbol.
   * @param buffers One entry per active TX antenna: buffers[antenna_idx] points to
   *        num_symbols * dev->config.fft_size consecutive REs, symbol-major (i.e.
   *        buffers[antenna_idx][local_symbol * dev->config.fft_size + subcarrier]).
   *        Each symbol MUST supply exactly dev->config.fft_size REs (the full FFT bin
   *        count, not a smaller BWP-sized buffer) -- passing fewer triggers an
   *        out-of-bounds read in the IDFT path. Read (summed into the accumulator),
   *        not mutated -- unlike the single-client version of this call, rotation is
   *        applied once to the shared accumulator, not per client, so the caller's
   *        buffer is left untouched.
   * @param num_buffers Must equal dev->config.num_tx_antennas.
   */
  ue_split7_status_t (*write_symbols)(struct ue_split7_device *dev,
                                      uint32_t client_id,
                                      uint32_t frame_number,
                                      uint16_t slot_number,
                                      uint8_t start_symbol,
                                      uint8_t num_symbols,
                                      c16_t **buffers,
                                      uint16_t num_buffers);

  /**
   * @brief Check in for num_symbols consecutive OFDM symbols on behalf of
   *        client_id without contributing any REs.
   *
   * Equivalent to write_symbols() with an all-zero contribution: lets a
   * synchronized client with nothing to send this range still complete the
   * per-symbol combine set, so it doesn't stall every other client.
   *
   * @param client_id Id returned by register_client().
   * @param frame_number Frame number (wrapped 0..1023 or absolute; resolved to closest frame by device).
   * @param slot_number Slot index within frame_number.
   * @param start_symbol First symbol index (0..13) of the contiguous range.
   * @param num_symbols Number of consecutive symbols starting at start_symbol.
   */
  ue_split7_status_t (*skip_symbols)(struct ue_split7_device *dev,
                                     uint32_t client_id,
                                     uint32_t frame_number,
                                     uint16_t slot_number,
                                     uint8_t start_symbol,
                                     uint8_t num_symbols);

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
   * @brief Atomically add sample_shift_samples to accumulated RX and TX timing offsets.
   *
   * Adjusting RX timing shifts the frame boundary, which inherently affects UL (TX) timing as well.
   * On subsequent RX, a positive offset trashes samples before receiving into the
   * time-domain buffer; a negative offset copies previously received samples to
   * the beginning and receives proportionally fewer samples from the antenna.
   * On subsequent TX, a positive offset transmits dummy samples for the offset duration;
   * a negative offset transmits fewer samples trimmed from the beginning of the buffer.
   * The applied offsets are atomically subtracted after adjustment.
   *
   * @param sample_shift_samples Number of samples to shift.
   */
  ue_split7_status_t (*adjust_rx_timing_offset)(struct ue_split7_device *dev, int32_t sample_shift_samples);

  /**
   * @brief Atomically add sample_shift_samples to the accumulated TX timing offset.
   *
   * On subsequent TX, a positive offset transmits dummy samples for the offset duration;
   * a negative offset transmits fewer samples trimmed from the beginning of the buffer.
   * The applied offset is atomically subtracted after adjustment.
   *
   * @param sample_shift_samples Number of samples to shift.
   */
  ue_split7_status_t (*adjust_tx_timing_offset)(struct ue_split7_device *dev, int32_t sample_shift_samples);

  /** @brief Backward-compatibility alias for adjust_rx_timing_offset. */
  ue_split7_status_t (*adjust_rx_timing)(struct ue_split7_device *dev, int32_t sample_shift_samples);

  /* --------------------- Host-decoupled Slot Timing --------------------- */

  /**
   * @brief Register a new UL client (an independent, spatially co-located UE
   *        instance sharing this device's RF chain) and hand out its id.
   *
   * Purely bookkeeping -- does not mark the client synchronized (see
   * seed_slot_tracking()) and never blocks. Up to a small fixed number of
   * clients are supported (implementation-defined bound); returns
   * UE_SPLIT7_ERR_NO_MEMORY once exhausted. A device with exactly one
   * registered, synchronized client behaves exactly as a single-client device.
   *
   * @param client_id Output: this client's id, to pass to every other call below.
   */
  ue_split7_status_t (*register_client)(struct ue_split7_device *dev, uint32_t *client_id);

  /**
   * @brief Seed the frame number for the next wait_next_slot() call, and mark
   *        client_id synchronized (participating in the UL combine from now on).
   *
   * Call once per client, right after that client's own start_sync()/re-sync,
   * with the MIB-decoded SFN (already corrected for frames_since_capture). Only
   * the first-ever call across all clients actually seeds the frame/slot
   * tracker and TX timing anchor (they're shared -- one RF clock, one Timing
   * Advance, for every co-located client); every call, first or not, marks
   * client_id as one write_symbols()/skip_symbols() must hear from every symbol
   * from now on.
   *
   * @param client_id Id returned by register_client().
   * @param frame_number SFN (0..1023) of the frame at slot 0 symbol 0, at
   *        the sample position sync_task_func() anchored the read pointer to.
   */
  ue_split7_status_t (*seed_slot_tracking)(struct ue_split7_device *dev, uint32_t client_id, uint32_t frame_number);

  /**
   * @brief Block until the next slot's samples have arrived; report its absolute frame/slot.
   *
   * Call once per slot instead of the host incrementing its own counter --
   * the Low-PHY owns the sample clock (circ_read_idx/circ_write_idx) and is
   * the sole source of truth for slot number, so this can't silently drift
   * the way an independent counter could. Must be called in slot order,
   * after seed_slot_tracking() has anchored the first slot.
   *
   * DL/RX is shared across every client (one physical antenna, one downlink
   * signal) and is NOT client-scoped: exactly one caller -- one orchestrating
   * thread driving every client's MAC/PHY -- is expected to call this (and
   * read_symbols()) once per slot on behalf of all of them.
   *
   * This is also where the actual time-domain RX happens: the whole slot's
   * samples are received here (CP removal + FFT are not -- that stays lazy,
   * per read_symbols() call) and every symbol's FFT-done state is reset to
   * not-yet-processed. A read_symbols() call for the same (frame, slot) this
   * call just reported skips straight to the FFT; it does not RX again.
   *
   * @param frame_number Output: absolute frame number of the now-available slot
   *        (SFN modulo 1024). Feed it straight into read_symbols()/write_symbols()/
   *        write_prach() -- there is no need to unwrap or track it yourself.
   * @param slot_number Output: slot index within that frame.
   */
  ue_split7_status_t (*wait_next_slot)(struct ue_split7_device *dev, uint32_t *frame_number, uint16_t *slot_number);

  /**
   * @brief Transmit an unaligned frequency-domain signal (e.g. PRACH preamble)
   *        on behalf of client_id.
   *
   * Fire-and-forget: builds the time-domain waveform (CP + repetitions) now,
   * exactly as before, but instead of transmitting it immediately, enqueues it
   * (owned heap copy) with its absolute RF sample range and returns right away.
   * The waveform is added, sample-for-sample, into whichever write_symbols()/
   * skip_symbols()-combined symbol(s) it overlaps in time, whenever that
   * finalize happens to run -- so it composes correctly regardless of what
   * other clients are doing in the same symbol (one mid-PRACH, another already
   * in normal UL, etc.). The queue has a small fixed depth; returns
   * UE_SPLIT7_ERR_BUSY if full.
   *
   * @param client_id Id returned by register_client() (kept for tracing; not
   *        otherwise load-bearing, since Timing Advance/the RF clock are shared).
   */
  ue_split7_status_t (*write_prach)(struct ue_split7_device *dev, uint32_t client_id, const ue_split7_prach_tx_params_t *params);

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
