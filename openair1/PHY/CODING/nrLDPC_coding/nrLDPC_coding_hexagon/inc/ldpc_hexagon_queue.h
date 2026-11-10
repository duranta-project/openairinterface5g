// SPDX-License-Identifier: LicenseRef-CSSL-1.0
//
// ldpc_hexagon_queue.h — dspqueue packet format shared between ARM stub and
// Hexagon DSP skel.  Included by both ldpc_hexagon_arm.c and ldpc_hexagon_imp.c.
//
// Wire layout:
//   Request  (ARM→DSP): bufs[0]=llr_in (FLUSH_SENDER), bufs[1]=llr_out
//                        msg = ldpc_q_req_t (16 bytes)
//   Response (DSP→ARM): bufs[0]=llr_out (FLUSH_SENDER+INVALIDATE_RECIPIENT+DEREF)
//                        msg = ldpc_q_resp_t (48 bytes)

#ifndef LDPC_HEXAGON_QUEUE_H
#define LDPC_HEXAGON_QUEUE_H

/* Concurrent decode contexts on the DSP.  Shared because it sizes BOTH sides: the ARM
 * clamp on OAI_LDPC_WORKERS here, and on the DSP g_workers[], g_go[], claims[] and
 * VTCM_TOTAL_SIZE = VTCM_PER_WORKER * this.  It also bounds the claims field below,
 * which packs one byte per worker into a uint32 -- raising this past 4 needs that
 * widened too, and needs the VTCM budget rechecked, since the acquire is for the
 * total and will simply fail if the PD cannot back it. */
#define LDPC_HEX_MAX_WORKERS 4

#include <stdint.h>

#define LDPC_QMSG_DECODE 1u
#define LDPC_QMSG_RESULT 2u
#define LDPC_QMSG_STOP 3u // ARM sends to terminate DSP spinning worker
#define LDPC_QMSG_BATCH 4u // one dispatch for a whole transport block
#define LDPC_QMSG_BRESULT 5u

/* Batch request: ALL code blocks of one transport block in a single dispatch.
 *
 * Why this exists. Per-segment dispatch forced the ARM to BUSY-POLL for the
 * response, because blocking costs ~326 us (measured: 30 segments went from
 * 12,780 to 22,550 us/slot when sleeping per segment). Polling means the ARM core
 * is held for the DSP's entire compute -- 11,250 us/slot -- so the offload freed
 * no ARM time at all, which on a DU is the whole point: those cores are wanted
 * for TX. One dispatch per TB replaces 30 waits with one, so the ARM can sleep
 * through the decode. It is also the prerequisite for decoding segments
 * concurrently across HVX contexts: with per-segment dispatch the ARM serialises
 * on every wait, so the DSP can never have more than one segment in flight.
 *
 * Everything here is per-TB except the code rate, which takes one of two values:
 * segments r < first_rE2 use R, the rest use R2 (38.212 5.4.2.1 gives the last
 * C - first_rE2 segments one extra Qm*Nl bits each). The DSP derives each
 * segment's numLLR from (BG, Z, R) itself, so the ARM does not send it.
 *
 * Buffers: bufs[0] = llr_in, C segments at llr_stride bytes each (int8, the
 * saturated `l` the reference decoder takes); bufs[1] = out, C segments at
 * out_stride bytes each (packed hard bits, K bits per segment, MSB-first);
 * bufs[2] = meta, per-segment iteration counts written back by the DSP.
 *
 * Per-segment results do NOT ride in the response message: dspqueue caps
 * messages well below the 144 bytes NR_LDPC_MAX_NUM_CB would need. */
typedef struct __attribute__((packed)) {
  uint32_t msg_type; // LDPC_QMSG_BATCH
  uint8_t BG;
  uint8_t R; // code rate index for segments r < first_rE2
  uint8_t R2; // ... and for the rest
  uint8_t numMaxIter;
  uint16_t Z;
  uint16_t C; // code blocks in this transport block
  uint16_t first_rE2;
  uint16_t Kprime; // CRC-bearing length, same for every segment of a TB
  uint16_t K; // systematic bits to pack out, including filler
  uint8_t crc_type; // 0 = CRC24_A, 1 = CRC24_B
  /* Code blocks to decode concurrently, across HVX contexts. 1 reproduces the
   * serial behaviour exactly. Driven from the ARM (OAI_LDPC_WORKERS) so the two
   * can be compared on one binary; the DSP clamps it to the threads it actually
   * managed to start and to the HVX contexts it could lock. */
  uint8_t nworkers;
  uint32_t llr_stride; // bytes between segments in bufs[0] (prep_on_dsp = 0 only)
  uint32_t out_stride; // bytes between segments in bufs[1]
  /* ---- prep-on-DSP ----
   * With prep_on_dsp set, bufs[0] carries the TB's CODEWORD LLRs (int16, G of
   * them) instead of C pre-prepared int8 blocks, and the DSP does the
   * deinterleave, the rate dematch and the saturate itself. That removes
   * ~1,050 us/slot of ARM work, and more importantly it is what lets the
   * codeword be handed over on the DSP once inner-RX and LDPC are fused --
   * today it makes a round trip through the ARM for no reason.
   *
   * `d`, which HARQ accumulates across rounds, then lives on the DSP keyed on
   * harq_pid. It needs no ARM access at all, so it is DSP heap, not rpcmem. */
  uint8_t prep_on_dsp;
  uint8_t Qm;
  uint8_t rv_index;
  uint8_t d_clear; // nrLDPC_TB_decoding_parameters_t::d_to_be_cleared
  uint32_t E; // rate-matched length for segments r < first_rE2
  uint32_t E2; // ... and for the rest
  uint32_t F; // filler bits
  uint32_t tbslbrm;
  uint32_t harq_pid; // harq_unique_pid: selects this TB's persistent `d`
  uint32_t G; // codeword LLRs in bufs[0], for bounds checking
  /* ---- segment range, for splitting one TB across BOTH CDSPs ----
   * This device decodes segments [seg_first, seg_first + seg_count). Indices stay
   * ABSOLUTE, so both devices address the shared llr/out/meta buffers identically and
   * only the range differs. seg_count == 0 means "all C", which is what a caller that
   * does not split sends, so the single-device path is unchanged.
   *
   * The split MUST be deterministic for a given (harq_pid, C). `d` -- the soft-combining
   * buffer HARQ accumulates across rounds -- lives on the DSP that decoded the segment,
   * so a retransmission has to land on the SAME device or it combines against an empty
   * buffer. That rules out dynamic load balancing ACROSS devices; the pull queue still
   * balances dynamically WITHIN one. */
  uint16_t seg_first;
  uint16_t seg_count;
} ldpc_q_batch_req_t;

typedef struct __attribute__((packed)) {
  uint32_t msg_type; // LDPC_QMSG_BRESULT
  uint32_t C; // segments actually decoded
  uint32_t dsp_clk_hz;
  uint32_t skel_version;
  uint32_t status; // 0 = ok
  uint32_t nworkers; // threads the DSP actually decoded with
  uint32_t hvx_units; // qurt_hvx_get_units(), for sizing sanity
  uint32_t batch_us; // DSP-side wall for the whole batch: the only number
                     // that says whether the workers ran CONCURRENTLY
  uint32_t claims; // blocks claimed per worker, one byte each
  uint32_t vtcm_ok; // 1 = VTCM acquired (needs signed PD), 0 = heap fallback
  uint32_t phase_us[8];
  /* Stage checksums for segment 0, sampled inside ldpc_scalar_core. The batch
   * and per-segment paths run the SAME core on the SAME input yet disagree for
   * BG2, and everything reachable by inspection has been eliminated -- so
   * compare these to find the first stage that diverges. There is no working
   * FARF channel on this platform, so the response struct is the only way to
   * get DSP-side state out. See dbg_hash[] in ldpc_worker_t. */
  uint64_t dbg_hash[3];
} ldpc_q_batch_resp_t;

/* Per-segment iteration counts, in bufs[2]. numMaxIter means "did not converge",
 * exactly as the per-segment return value did. */
#define LDPC_HEX_BATCH_MAX_CB 144u /* NR_LDPC_MAX_NUM_CB */
typedef struct __attribute__((packed)) {
  uint8_t numIter[LDPC_HEX_BATCH_MAX_CB];
} ldpc_q_batch_meta_t;

// Request message sent ARM→DSP for each codeword decode.
// 16 bytes, two buffer references: bufs[0]=llr_in, bufs[1]=llr_out.
typedef struct __attribute__((packed)) {
  uint32_t msg_type; // LDPC_QMSG_DECODE
  uint8_t BG;
  uint8_t R;
  uint8_t numMaxIter;
  uint8_t pad0;
  uint16_t Z;
  uint16_t numLLR;
  /* Kprime > 0 and a valid crc_type switch the DSP to CRC termination, matching
   * the reference decoder's criterion. Zero keeps the old parity-check
   * behaviour, which is what the legacy ldpctest path wants (it passes
   * check_crc = NULL and has no CRC to check against). */
  uint16_t Kprime;
  uint8_t crc_type;
  uint8_t pad1;
} ldpc_q_req_t;

// Response message sent DSP→ARM when decoding is complete.
// 48 bytes, one buffer reference: bufs[0]=llr_out (flushed).
typedef struct __attribute__((packed)) {
  uint32_t msg_type; // LDPC_QMSG_RESULT
  uint32_t numIter;
  uint32_t dsp_clk_hz;
  uint32_t skel_version; // LDPC_SKEL_VERSION — visible to ARM without FARF/logcat
  uint32_t phase_us[8]; // per-phase timing (LDPC_NUM_PHASES = 8)
  uint64_t dbg_hash[3]; // same stage checksums as the batch response
} ldpc_q_resp_t;

#endif // LDPC_HEXAGON_QUEUE_H
