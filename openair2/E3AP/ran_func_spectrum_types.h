/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * Dependency-free sensing data types AND the consumer accessor API of the
 * Spectrum RAN function, split out of ran_func_spectrum.h so a consumer that
 * only needs the data shapes / read API (the Spectrum SM) can include this
 * without pulling in any MAC header.
 */
#ifndef RAN_FUNC_SPECTRUM_TYPES_H
#define RAN_FUNC_SPECTRUM_TYPES_H

/* --- Reserved sensing-RNTI ---------------------------------------------- *
 * One RNTI reserved for the sensing capture PUSCH: nr_mac_get_new_rnti() skips
 * it so no real UE is assigned it, and is_sensing_rnti() tests it. One is enough
 * — a single capture PUSCH per slot drives Aerial's slot-level IQ. */
#define SENSING_RNTI 0x1240
#define is_sensing_rnti(r) ((r) == SENSING_RNTI)

/* --- Sensing range descriptor ------------------------------------------- */
/* One entry per (symbol, contiguous free-PRB span) tile. 128 covers the
 * worst case (14 symbols x several PRB islands each) with headroom. */
#define MAX_SENSING_RANGES 128

typedef struct {
  int start_symbol;
  int num_symbols;
  int rb_start; /* ABSOLUTE carrier-relative PRB index (not BWP-relative);
                 * dApps consume it directly against the L1 IQ tap. */
  int rb_size;
} sensing_range_t;

/* --- Consumer accessor API ---------------------------------------------- *
 * The dependency-free read side of the per-(beam, slot) sensing-range snapshot
 * the scheduler hook computes. A consumer like the Spectrum SM uses them
 * wait-then-fetch. Defined in ran_func_spectrum.c; weak fallbacks in
 * service_models/spectrum_sm/ran_func_spectrum_stub.c cover binaries that do
 * not link the MAC. */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- Controls the Spectrum SM applies to the RAN ------------------------- *
 * Declared with the same header on both sides -- the definition in
 * ran_func_spectrum_prb_block.c and the weak fallback in the SM library -- so
 * the compiler, not the linker, checks that they agree. The types are only
 * forward declared: the SM does not need the MAC headers. */
struct gNB_MAC_INST_s;
struct nr_cell_sched_s;

typedef enum { PRB_BLOCK_DIR_DL = 0, PRB_BLOCK_DIR_UL = 1 } prb_block_dir_t;

/* Replace a direction's PRB block (NULL/len 0 = clear). REPLACE semantics: the
 * dApp sends the full current set on every change. False if the cell has no
 * block state. Takes the MAC instance too: the registry scan an install triggers
 * runs under its sched_lock. */
bool set_prb_block_mask(struct gNB_MAC_INST_s *mac,
                        struct nr_cell_sched_s *cell,
                        prb_block_dir_t dir,
                        const uint16_t *mask,
                        int len);

/* Small wake-up payload returned by nr_mac_wait_for_sensing_publish() — just
 * which (beam, slot) was last written, so the consumer can fetch its ranges.
 * The writer may overwrite that cell before the fetch; that's fine, the
 * consumer just reads the newest "latest snapshot". */
typedef struct {
  uint16_t beam; /* MAX_NUM_BEAM_PERIODS-1 .. fits in u16 */
  uint16_t frame; /* SFN, 0..1023 */
  uint16_t slot; /* slot index in frame, 0..NR_MAX_SLOTS_PER_FRAME-1 */
  uint64_t timestamp_ns; /* CLOCK_MONOTONIC at publish time */
} nr_mac_sensing_publish_meta_t;

/* Copy up to max_out ranges from the (beam, slot) snapshot into out_ranges and
 * the count into *out_n. Returns false on seqlock contention (transient — the
 * caller may retry) or if beam/slot are out of range (permanent). */
bool nr_mac_get_sensing_ranges(int mod_id, int beam, int slot, sensing_range_t *out_ranges, int max_out, uint8_t *out_n);

/* Block until a newer write than *inout_seq is published, or the timeout fires,
 * or shutdown wakes us. *inout_seq is the caller's read cursor: each true
 * return fills out_meta with the OLDEST write the caller has not consumed yet
 * and advances the cursor by one, so bunched publishes are drained in order
 * across successive calls. timeout_ns: 0 = poll, UINT64_MAX = wait forever.
 * Returns true on a publish OR a shutdown wake — so when it returns true the
 * caller must also check its own running flag to tell the two apart. */
bool nr_mac_wait_for_sensing_publish(uint64_t timeout_ns, uint64_t *inout_seq, nr_mac_sensing_publish_meta_t *out_meta);

/* Wake any thread blocked in nr_mac_wait_for_sensing_publish() (e.g. at
 * shutdown). The waiter returns once and must re-check its own running flag. */
void nr_mac_signal_sensing_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* RAN_FUNC_SPECTRUM_TYPES_H */
