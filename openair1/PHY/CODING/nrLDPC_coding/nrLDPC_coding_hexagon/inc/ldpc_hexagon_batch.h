/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * ldpc_hexagon_batch.h — ARM-side batch decode API.
 *
 * The whole point of this API is that ONE call decodes every code block of a
 * transport block. The per-segment path (LDPCdecoder, still present for
 * ldpctest) had to busy-poll for each response, because blocking costs ~326 us
 * and 30 segments of that is 9.8 ms; polling in turn held an ARM core for the
 * DSP's entire 11,250 us of compute, so the offload freed no ARM time. With one
 * dispatch the ARM sleeps once, and the DSP is free to decode segments
 * concurrently later on -- impossible per-segment, where the ARM serialises on
 * every wait.
 *
 * Deliberately exposes no FastRPC, dspqueue or HAP types, so the coding-interface
 * implementation does not acquire a Hexagon SDK dependency.
 *
 * The caller writes prepared LLRs STRAIGHT into the shared input buffer -- that
 * is what ldpc_hex_batch_bufs() is for. Do not stage them elsewhere and copy:
 * the reference path built each segment's `l` on the stack and then copied it in,
 * which is 783 kB of pointless memcpy per slot at 273 PRB.
 */

#ifndef LDPC_HEXAGON_BATCH_H
#define LDPC_HEXAGON_BATCH_H

#include <stdint.h>

typedef struct {
  uint8_t BG;
  uint8_t R; /* code rate index for segments r < first_rE2 */
  uint8_t R2; /* ... and for the rest */
  uint8_t numMaxIter;
  uint16_t Z;
  uint16_t C; /* code blocks in this transport block */
  uint16_t first_rE2;
  uint16_t Kprime; /* CRC-bearing length; same for every segment of a TB */
  uint16_t K; /* systematic bits to pack out, including filler */
  uint8_t crc_type; /* 0 = CRC24_A, 1 = CRC24_B */
  /* prep_on_dsp: the shared input carries the TB's CODEWORD LLRs (int16, G of
   * them, write them with ldpc_hex_batch_cw()) and the DSP does the
   * deinterleave, rate dematch and saturate. `d` then lives on the DSP keyed on
   * harq_pid and never crosses the boundary. Zero keeps the ARM-side prep, with
   * C prepped int8 blocks in the input at the stride reported below. */
  uint8_t prep_on_dsp;
  uint8_t Qm;
  uint8_t rv_index;
  uint8_t d_clear; /* nrLDPC_TB_decoding_parameters_t::d_to_be_cleared */
  uint32_t E; /* rate-matched length for segments r < first_rE2 */
  uint32_t E2; /* ... and for the rest */
  uint32_t F; /* filler bits */
  uint32_t tbslbrm;
  uint32_t harq_pid; /* harq_unique_pid, selects this TB's persistent `d` */
  uint32_t G; /* codeword LLRs supplied when prep_on_dsp */
} ldpc_hex_batch_t;

/* Where to write the codeword LLRs when prep_on_dsp is set. Capacity in int16
 * entries is returned in *max_g. */
int ldpc_hex_batch_cw(int16_t **cw, uint32_t *max_g);

/* Shared buffers, valid after a successful LDPCinit(). The input holds C
 * segments at *llr_stride bytes; the output C segments at *out_stride bytes of
 * packed hard bits (K bits per segment, MSB-first, matching what the segment
 * path's nrLDPC_outMode_BIT produced). Returns 0 on success. */
int ldpc_hex_batch_bufs(int8_t **llr, uint32_t *llr_stride, uint8_t **out, uint32_t *out_stride);

/* Largest C and stride this build can carry in one dispatch. A caller with a
 * bigger transport block must fall back rather than truncate. */
void ldpc_hex_batch_caps(uint32_t *max_cb, uint32_t *max_llr_stride);

/* Submits one transport block and blocks until the DSP has decoded all of it.
 * `iters` receives C iteration counts; numMaxIter means "did not converge",
 * exactly as the per-segment return value did. Returns 0 on success. */
/* Capacity of the DSP's per-worker deinterleave buffer (prep_e), in int16
 * entries. The DSP writes E entries per segment, so a transport block whose E
 * exceeds this MUST be prepped on the ARM instead. A single-segment low-rate
 * block can easily exceed it -- BG2 Z=384 C=1 has E = 31800 against a 27000
 * capacity, which silently overran prep_e into the neighbouring prep buffers
 * and decoded to noise. Multi-segment BG1 never hits it because E is per
 * segment. Keep in step with w->prep_e in ldpc_hexagon_imp.c. */
#define LDPC_HEX_PREP_E_MAX 27000u

int ldpc_hex_batch_run(const ldpc_hex_batch_t *b, uint8_t *iters);

#endif /* LDPC_HEXAGON_BATCH_H */
