// SPDX-License-Identifier: LicenseRef-CSSL-1.0
//
// ldpc_hexagon_imp.c — Hexagon DSP LDPC decoder, HVX-accelerated min-sum.
//
// Compiled with hexagon-clang -O3 -mhvx -mhvx-length=128B and loaded into
// the cDSP protected domain via FastRPC.
//
// Algorithm:
//   Standard min-sum belief propagation:
//     CN step: for each check-node edge, output = sign(∏ others) × min(|others|)
//     BN step: posterior = channel_LLR + Σ(CN messages);
//              extrinsic = posterior − self CN message
//   Convergence: XOR of sign bits across all connected BNs at each CN = 0
//
// cnProc_group() vectorises the CN step with 128B HVX intrinsics, processing
// Z samples (one per lifting symbol) in parallel.  Full 128-byte HVX vectors
// are used for floor(Z/128) chunks; the remaining tail bytes are handled by
// a scalar fallback.  For Z=384 (= 3×128) there is no tail — pure HVX path.

// Suppress OAI framework headers (time_meas.h etc.) inside nrLDPC_types.h
#define CODEGEN 1

// nrLDPC_mPass.h unconditionally includes common/utils/utils.h, which pulls in
// <malloc.h> — not available in the Hexagon DSP toolchain.  Block it by setting
// its include guard before nrLDPC_mPass.h is seen, and provide the one macro
// from utils.h that mPass.h actually uses.
#ifndef _UTILS_H
#define _UTILS_H
#define sizeofArray(a) ((int)(sizeof(a) / sizeof((a)[0])))
#endif

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#ifdef LDPC_SIM_STANDALONE
// Standalone Hexagon simulator build — no FastRPC / HAP framework.
#include <stdio.h>
#define FARF(level, fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
#define HAP_perf_get_time_us() ((uint64_t)0)
#else
#include <stdio.h>
#include "HAP_farf.h"
#include "HAP_power.h"
#include "HAP_perf.h"
// LDPC_HEX_VTCM: allocate working set in VTCM via HAP_compute_res_acquire.
// Including HAP_compute_res.h adds weak compute_resource_* symbols to the
// skel's dynsym; the IQ-9075 RTLD rejects unresolvable weak symbols at
// dlopen time in unsigned-PD.  Guard the include so unsigned-PD builds are
// unaffected.  Enable at cmake configure time with -DLDPC_HEX_VTCM=ON.
#if defined(LDPC_HEX_VTCM) || defined(LDPC_HEX_GATHER)
#include "HAP_compute_res.h"
#endif
#endif

/* Gather-staging sizes, needed before the VTCM budget below can be computed. */
#ifdef LDPC_HEX_GATHER
#define LDPC_GV_F_HW 32768u /* halfwords staged for one segment's E */
#define LDPC_GV_E_HW 32768u /* one deinterleaved row */
#else
#define LDPC_GV_F_HW 0u
#define LDPC_GV_E_HW 0u
#endif

#include <hexagon_types.h>
#include <hvx_hexagon_protos.h>
#include <hexagon_circ_brev_intrinsics.h> // Q6_vmaskedstoreq_QAV

#define HVX_VLEN 128 // HVX vector width in bytes (-mhvx-length=128B)

/* Profiling only. hexagon-sim's --packet_analyze reports per packet ADDRESS, so attributing
 * cycles to a kernel means finding its symbol -- and a kernel that got inlined into main
 * has no range to restrict to. Build the harness with -DLDPC_PROFILE_NOINLINE to keep the
 * kernels as real symbols. It changes inlining, so use it to find WHERE the cycles go, and
 * measure the verdict on the device. */
#ifdef LDPC_PROFILE_NOINLINE
#define LDPC_KERNEL __attribute__((noinline))
#else
#define LDPC_KERNEL
#endif
#define LDPC_HEX_MAX_BN_PER_CN_GROUP 19 // BG1 CN group 8 has 19 BNs (max across all groups)
/* PROFILING KNOB, not a tunable. Selects which stages of cnProc_fused_group are emitted,
 * so cycles can be attributed to them under hexagon-sim: bit0 DCFETCH prefetch, bit1
 * Pass 1 (min1/min2/sgn), bit2 Pass 2 (extrinsic -> tmp), bit3 the circular scatter to
 * bn_msg. Anything but 0xF produces WRONG OUTPUT and exists only to be measured; the
 * default folds away at compile time and costs nothing. */
#ifndef LDPC_CNPROC_STAGES
#define LDPC_CNPROC_STAGES 0xF
#endif

/* Pass-2 fold: write the rotated extrinsic straight to bn_msg instead of staging it in
 * tmp[] for the circular scatter to copy. See cnProc_fused_row. 0 restores the staged
 * path, which is what the fold is validated against -- the sim harness FNV64 must be
 * identical with the fold on and off, at every Z (it is, for Z = 128 / 256 / 384). */
#ifndef LDPC_CNPROC_FOLD
#define LDPC_CNPROC_FOLD 1
#endif
// Max numCnInCnGroups across all BG1 and BG2 rate variants (BG1 R=1/3 group 2 = 18, BG2 R=1/5 group 1 = 20)
#define LDPC_HEX_MAX_NC 24
// bn_msg has NR_LDPC_NUM_EDGE_BG1 (316) Z-byte slots; bn_base/Z is always in [0,316)
#define LDPC_HEX_BN_SLOTS NR_LDPC_NUM_EDGE_BG1

// Per-phase timing indices for HAP_perf_get_time_us() profiling (meta[8..39]).
#define LDPC_PHASE_SCATTER 0 // llr2llrProcBuf + llr2CnProcBuf
#define LDPC_PHASE_CNPROC 1
/* Slots 2 and 5 were named for a cn2bn/bn2cn pair that no longer exists -- cnProc_fused
 * and bnProc_fused absorbed both. They time the two input-side copies instead, and the
 * old names sent you looking for message-passing that is not there. */
#define LDPC_PHASE_LLRCOPY 2 /* copy the segment's LLRs into the working buffer */
#define LDPC_PHASE_BNPROCPC 3
#define LDPC_PHASE_BNPROC 4
#define LDPC_PHASE_LLR2PROC 5 /* llr2llrProcBuf: channel LLRs into BN-group order */
/* Slot 6 is the TERMINATION TEST, whichever form it takes -- the two are mutually
 * exclusive and never both run:
 *   use_crc (every real transport block, Kprime > 0): ldpc_llr2bits + ldpc_check_crc24
 *   !use_crc:                                         cnProcPc_fused, the parity check
 * It was named CNPROCPC when only the parity check existed, which is now the path we never
 * take -- reading the profile as "cnProcPc 3%" sends you looking for a syndrome computation
 * that does not happen. */
#define LDPC_PHASE_TERMTEST 6
/* Slot 7 is the gather. NOTE it is NOT a once-per-codeword epilogue under CRC termination:
 * from LDPC_CRC_FIRST_ITER onward it runs EVERY iteration, because the CRC needs llr_out.
 * It is the price of early termination, so do not "optimise" it without checking the
 * iteration count it is buying back. */
#define LDPC_PHASE_GATHER 7
#define LDPC_NUM_PHASES 8

// Portable LDPC decoder headers — no SIMD, no OAI framework dependencies
#include "nrLDPCdecoder_defs.h"
#include "nrLDPC_types.h"
#include "nrLDPC_lut.h"
#include "nrLDPC_init.h"
#include "nrLDPC_mPass.h"

#ifndef LDPC_SIM_STANDALONE
// qaic-generated dispatch header (not present in standalone sim build)
#include "ldpc_hexagon.h"
#include "dspqueue.h"
#include "qurt.h"
#endif

/* Outside the guard: this is plain struct/constant definitions (no dspqueue or QuRT
 * types), and the standalone sim needs LDPC_HEX_MAX_WORKERS to size g_workers[]. */
#include "ldpc_hexagon_queue.h"

// Wire parameter struct — must match ldpc_hexagon_arm.c
typedef struct __attribute__((packed)) {
  uint8_t BG;
  uint8_t R;
  uint8_t numMaxIter;
  uint8_t diag; // 0=memcpy passthrough, 1=scatter/gather roundtrip, 2+=normal BP
  uint16_t Z;
  /* Kprime and crc_type let the DSP terminate on the CRC instead of on its
   * parity check. The reference decoder stops as soon as check_crc() passes,
   * typically at iteration 2; the parity check is satisfied later, so this
   * decoder was running 4-5 iterations for the same answer -- the whole ~1.8x
   * decode-time gap against the CPU. */
  uint16_t Kprime;
  uint8_t crc_type;
  uint8_t pad2[3];
} ldpc_hex_params_t;

// =============================================================================
// Saturating arithmetic helpers
// =============================================================================

static inline int8_t sat8_add(int8_t a, int8_t b)
{
  int16_t r = (int16_t)a + (int16_t)b;
  return r > 127 ? 127 : (r < -128 ? -128 : (int8_t)r);
}

static inline int8_t sat8_sub(int8_t a, int8_t b)
{
  int16_t r = (int16_t)a - (int16_t)b;
  return r > 127 ? 127 : (r < -128 ? -128 : (int8_t)r);
}

// Absolute value for int8; handles -128 safely (clamps to 127)
static inline uint8_t abs8(int8_t v)
{
  int16_t w = (int16_t)v;
  int16_t a = w < 0 ? -w : w;
  return (uint8_t)(a > 127 ? 127 : a);
}

// =============================================================================
// BN group sizes per CN group (fixed by base-graph structure)
// =============================================================================

static const int8_t numBN_BG1[NR_LDPC_NUM_CN_GROUPS_BG1] = {3, 4, 5, 6, 7, 8, 9, 10, 19};
static const int8_t numBN_BG2[NR_LDPC_NUM_CN_GROUPS_BG2] = {3, 4, 5, 6, 8, 10};

// =============================================================================
// CN Processing — HVX-vectorised min-sum
// =============================================================================
// For every (CN i, lifting symbol z) in a group:
//   Pass 1 over all BN positions k: track first/second minimum |v|, XOR signs.
//   Pass 2: write extrinsic message for each position j:
//     mag_j  = (|v_j| == min1) ? min2 : min1   (excludes j's contribution)
//     sign_j = sgn_all XOR v_j                   (removes j's sign from product)
//     result = sign_j < 0 ? -mag_j : mag_j
//
// The Z lifting symbols are processed in parallel with 128B HVX vectors.
// floor(Z/128) full vectors are handled by HVX; the residual tail (Z % 128)
// falls back to scalar.  For Z = 384 = 3×128 there is no tail.
//
// Min-sum tie handling: the "mag = (|v_j|==min1)?min2:min1" rule approximates
// the correct "exclude-self" minimum when only one BN achieves min1.  For ties
// (multiple BNs with equal minimum) both positions return min2 rather than min1;
// this is a standard approximation used in all vectorised implementations and
// has negligible effect on convergence.

static void scalar_cnProc_group(const int8_t *cnProcBuf, int8_t *cnProcBufRes, uint32_t startAddr, int numBN, int numCN, uint16_t Z)
{
  int32_t bitOff = (int32_t)numCN * NR_LDPC_ZMAX;
  for (int i = 0; i < numCN; i++) {
    int32_t row = i * Z;
    for (int z = 0; z < (int)Z; z++) {
      int32_t samp = row + z;
      int8_t sgn_all = 0;
      uint8_t min1 = 127, min2 = 127;
      int min1_k = 0;
      for (int k = 0; k < numBN; k++) {
        int8_t v = cnProcBuf[startAddr + (uint32_t)k * bitOff + samp];
        sgn_all ^= v;
        uint8_t av = abs8(v);
        if (av < min1) {
          min2 = min1;
          min1 = av;
          min1_k = k;
        } else if (av < min2) {
          min2 = av;
        }
      }
      for (int j = 0; j < numBN; j++) {
        int8_t v_j = cnProcBuf[startAddr + (uint32_t)j * bitOff + samp];
        int8_t sgn_j = sgn_all ^ v_j;
        uint8_t mag = (j == min1_k) ? min2 : min1;
        cnProcBufRes[startAddr + (uint32_t)j * bitOff + samp] = (sgn_j & 0x80) ? -(int8_t)mag : (int8_t)mag;
      }
    }
  }
}

// Bring one 128-byte HVX-vector-sized chunk from L2 into L1 dcache.
// Q6_dcfetch_A fetches 32 bytes; four calls cover a full 128-byte vector.
// Use this instead of l2fetch when data is already in L2 (warm working set)
// and the bottleneck is L1-miss-to-L2 stall latency.
#define DCFETCH128(p)                            \
  do {                                           \
    Q6_dcfetch_A((void *)(uintptr_t)(p));        \
    Q6_dcfetch_A((void *)(uintptr_t)((p) + 32)); \
    Q6_dcfetch_A((void *)(uintptr_t)((p) + 64)); \
    Q6_dcfetch_A((void *)(uintptr_t)((p) + 96)); \
  } while (0)

/* ---------------------------------------------------------------------------------------
 * SOFTWARE PREFETCH: all three classes measured, all three REMOVED.
 *
 * Every DCFETCH in this file was a net loss on the IQ-9075. Summed over the three A/B runs
 * below (273 PRB / 1 layer / MCS 20 / SNR 13.2 / 200 slots, arms alternated inside one
 * batch, two reps), us/codeword:
 *
 *     phase       before  after
 *     scatter        29     15
 *     cnProc         97     77
 *     cn2bn          14     13
 *     bnProcPc       32     18
 *     bnProc         76     42
 *     bn2cn          15     15
 *     termtest        7      7
 *     gather         15     15
 *     total         285    202     (-29%)
 *
 *   ULSCH segments decoding 1725 -> 1403 us/slot (-19%), Total PHY proc rx 2815 -> 2495
 *   (-11%), BLER 0 throughout -- prefetch is semantically neutral, so this is pure cost.
 *
 * The lesson is not "prefetching is bad" but that DCFETCH128 is FOUR dcfetch instructions
 * and this decoder issues them in nested loops, tens per inner iteration, against data
 * that is either in VTCM (uncached, so unprefetchable) or already resident in L2. That is
 * scalar issue bandwidth spent to buy nothing, in kernels whose PMU profile (see the Pass 1
 * comment in cnProc_fused_group) shows them already short of issue slots per packet.
 *
 * If any buffer moves back out of VTCM, re-measure rather than re-enabling on principle.
 * --------------------------------------------------------------------------------------- */

/* DCFETCH on a cn_msg/bn_msg address -- the same instruction, but on memory that is not
 * cached at all.
 *
 * These prefetch loops were added when every buffer lived in the heap. cn_msg and bn_msg
 * then MOVED TO VTCM, and VTCM is not backed by L1/L2: there is nothing for a dcfetch to
 * prime, while the instruction still issues and still occupies the data unit. The prefetch
 * silently turned into pure overhead without anyone touching the loops.
 *
 * Measured on the IQ-9075, 273 PRB / 1 layer / MCS 20 / SNR 13.2 / 200 slots, both
 * variants alternated inside one batch (the phase profile drifts ~20% across batches) and
 * reproduced to 1 us over two reps -- us/codeword, prefetch ON -> OFF:
 *     cnProc    97 -> 79     (-19%)
 *     bnProcPc  32 -> 18     (-44%)
 *     bnProc    76 -> 66     (-13%)
 *     every other phase unchanged; BLER 0 either way
 *   decode 285 -> 244 us/codeword, ULSCH segments decoding 1725 -> 1560 us/slot.
 *
 * hexagon-sim --timing agrees in DIRECTION even with the buffers in heap, where the
 * prefetch was supposed to be earning its keep: dropping just the cnProc loop takes it
 * from 1427717 to 909436 cycles over 32 iterations. So there is no configuration in which
 * this has been shown to pay. The gate is nevertheless keyed to VTCM rather than removed,
 * because only the VTCM case has been measured ON THE DEVICE, and the VTCM acquire does
 * have a heap fallback -- that degraded path keeps the old behaviour.
 *
 * llrRes and llrProcBuf are deliberately still heap (scalar-read by the gather), so their
 * prefetches are a separate question -- see LDPC_HEX_LLR_DCFETCH.
 *
 * -DLDPC_HEX_MSG_DCFETCH=1 forces the old behaviour back for A/B measurement. */
#if defined(LDPC_HEX_VTCM) && !defined(LDPC_HEX_MSG_DCFETCH)
#define LDPC_HEX_MSG_DCFETCH 0
#endif
#ifndef LDPC_HEX_MSG_DCFETCH
#define LDPC_HEX_MSG_DCFETCH 1
#endif
#if LDPC_HEX_MSG_DCFETCH
#define DCFETCH128_MSG(p) DCFETCH128(p)
#else
#define DCFETCH128_MSG(p) ((void)(p))
#endif

/* DCFETCH on an llrRes address. Unlike cn_msg/bn_msg this really IS cached memory, so the
 * prefetch has something to prime -- and it is still a loss, by more than the VTCM one.
 * Same rig as above, msg prefetch already off in both arms, us/codeword ON -> OFF:
 *     bnProc    66 -> 42   (-36%)
 *     bnProcPc  18 -> 18   (neutral -- its llrRes prefetch is simply free)
 *     cnProc    79 -> 78
 *   ULSCH segments decoding 1556 -> 1457 us/slot. Reproduced to 0 us over two reps.
 *
 * The reason is issue pressure, not cache behaviour: DCFETCH128 is FOUR dcfetch
 * instructions, and bnProc_fused's look-ahead fires 3 offsets x 2 buffers = 24 scalar
 * instructions per CN position, to cover loads that were going to hit L2 anyway.
 *
 * -DLDPC_HEX_LLR_DCFETCH=1 restores them. */
#ifndef LDPC_HEX_LLR_DCFETCH
#define LDPC_HEX_LLR_DCFETCH 0
#endif
#if LDPC_HEX_LLR_DCFETCH
#define DCFETCH128_LLR(p) DCFETCH128(p)
#else
#define DCFETCH128_LLR(p) ((void)(p))
#endif

/* DCFETCH on the incoming codeword LLRs in the scatter (nrLDPC_llr2CnProcBuf). Heap
 * source, streamed once, nothing has touched the data yet -- the one place a prefetch had a
 * plausible story. It loses too, and by the largest relative margin of the three:
 *     scatter  29 -> 15 us/codeword  (-48%)
 * with every other phase unchanged. ULSCH segments decoding 1455 -> 1403 us/slot,
 * reproduced to 0.1 us over two reps.
 *
 * -DLDPC_HEX_IN_DCFETCH=1 restores them. */
#ifndef LDPC_HEX_IN_DCFETCH
#define LDPC_HEX_IN_DCFETCH 0
#endif
#if LDPC_HEX_IN_DCFETCH
#define DCFETCH128_IN(p) DCFETCH128(p)
#else
#define DCFETCH128_IN(p) ((void)(p))
#endif

// 2D L2 prefetch: `height` rows of `width` bytes, rows spaced `stride` bytes apart.
// Descriptor format (Hexagon ISA): stride→bits[63:48], width→bits[47:32], height→bits[31:16].
// Only useful when data is NOT already in L2 (cold working set / DRAM misses).
static inline void hvx_l2fetch_2d(const void *base, uint32_t stride, uint32_t width, uint32_t height)
{
  uint64_t desc =
      ((uint64_t)(stride & 0xFFFFu) << 48) | ((uint64_t)(width & 0xFFFFu) << 32) | ((uint64_t)(height & 0xFFFFu) << 16) | 0ULL;
  Q6_l2fetch_AP((void *)(uintptr_t)base, desc);
}

// HVX circular copy for Z ∈ {128, 256, 384} (multiples of HVX_VLEN).
// Computes dst[z] = src[(z + r) % Z] for z = 0..Z-1.
// Equivalent to nrLDPC_circ_memcpy (r = cs) and nrLDPC_inv_circ_memcpy
// (r = cs==0 ? 0 : Z-cs).  Caller must ensure Z is one of the three values.
static inline void hvx_circ_copy(int8_t *restrict dst, const int8_t *restrict src, uint32_t r, uint16_t Z)
{
  if (Z == (uint16_t)HVX_VLEN) {
    HVX_Vector vs = *(HVX_UVector *)src;
    *(HVX_UVector *)dst = Q6_V_valign_VVR(vs, vs, r);
  } else if (Z == 2u * (uint16_t)HVX_VLEN) {
    HVX_Vector vs0 = *(HVX_UVector *)src;
    HVX_Vector vs1 = *(HVX_UVector *)(src + 128);
    HVX_Vector q0, q1;
    if (r < 128u) {
      q0 = Q6_V_valign_VVR(vs1, vs0, r);
      q1 = Q6_V_valign_VVR(vs0, vs1, r);
    } else {
      uint32_t rr = r - 128u;
      q0 = Q6_V_valign_VVR(vs0, vs1, rr);
      q1 = Q6_V_valign_VVR(vs1, vs0, rr);
    }
    *(HVX_UVector *)dst = q0;
    *(HVX_UVector *)(dst + 128) = q1;
  } else {
    // Z == 3 * HVX_VLEN (= 384)
    HVX_Vector vs0 = *(HVX_UVector *)src;
    HVX_Vector vs1 = *(HVX_UVector *)(src + 128);
    HVX_Vector vs2 = *(HVX_UVector *)(src + 256);
    HVX_Vector q0, q1, q2;
    if (r < 128u) {
      q0 = Q6_V_valign_VVR(vs1, vs0, r);
      q1 = Q6_V_valign_VVR(vs2, vs1, r);
      q2 = Q6_V_valign_VVR(vs0, vs2, r);
    } else if (r < 256u) {
      uint32_t rr = r - 128u;
      q0 = Q6_V_valign_VVR(vs2, vs1, rr);
      q1 = Q6_V_valign_VVR(vs0, vs2, rr);
      q2 = Q6_V_valign_VVR(vs1, vs0, rr);
    } else {
      uint32_t rr = r - 256u;
      q0 = Q6_V_valign_VVR(vs0, vs2, rr);
      q1 = Q6_V_valign_VVR(vs1, vs0, rr);
      q2 = Q6_V_valign_VVR(vs2, vs1, rr);
    }
    *(HVX_UVector *)dst = q0;
    *(HVX_UVector *)(dst + 128) = q1;
    *(HVX_UVector *)(dst + 256) = q2;
  }
}

/* Store the first n bytes of v to dst (n <= HVX_VLEN), leaving the rest of the 128-byte
 * window intact by read-modify-write.
 *
 * This is how every kernel here handles a Z that is not a multiple of the vector length.
 * The alternative -- a scalar loop over the residual bytes -- is correct but ruinous, since
 * cn_msg and bn_msg live in VTCM where each scalar access is ~20 cycles and the residual is
 * touched per edge per iteration. It needs no alignment, unlike a masked vector store.
 *
 * Callers must ensure the 128-byte window at dst is readable; all buffers here are sized
 * for the Z=384 case, and the allocations carry HVX_VLEN of slack on top. */
static inline void hvx_store_partial(int8_t *dst, HVX_Vector v, uint32_t n)
{
  if (n >= (uint32_t)HVX_VLEN) {
    *(HVX_UVector *)dst = v;
  } else {
    HVX_VectorPred q = Q6_Q_vsetq2_R((int32_t)n);
    *(HVX_UVector *)dst = Q6_V_vmux_QVV(q, v, *(HVX_UVector *)dst);
  }
}

/* Vector block copy. The reference scatter/gather and the input copy are all sequences of
 * memcpy, and libc's is not using the vector unit here: the 26 KB input copy alone measured
 * 13 us, about 1.4 bytes/cycle. A 128-byte-at-a-time loop is the whole trick. The residual
 * is copied as bytes rather than with a partial vector store, so this neither reads nor
 * writes outside [src, src+n) / [dst, dst+n) -- the input buffer is FastRPC-mapped and the
 * last segment's end is a real boundary. */
static inline void hvx_memcpy(int8_t *restrict dst, const int8_t *restrict src, uint32_t n)
{
  uint32_t i = 0;
  for (; i + (uint32_t)HVX_VLEN <= n; i += (uint32_t)HVX_VLEN)
    *(HVX_UVector *)(dst + i) = *(HVX_UVector *)(src + i);
  for (; i < n; i++)
    dst[i] = src[i];
}

/* Same copy, but the residual goes out as a masked vector read-modify-write instead of
 * bytes. Requires HVX_VLEN of readable slack past src and dst, so it is only for buffers we
 * own -- never for the FastRPC-mapped input.
 *
 * It exists because those buffers moved to VTCM, where a SCALAR access is ~20 cycles. With
 * Z not a multiple of 128 the byte tails alone dominated: the scatter/gather do 22 copies
 * of Z bytes each, and at Zc=240 that took the whole decode from 171 to 279 us. Vector tail,
 * and it is back to 171. */
static inline void hvx_memcpy_slack(int8_t *restrict dst, const int8_t *restrict src, uint32_t n)
{
  uint32_t i = 0;
  for (; i + (uint32_t)HVX_VLEN <= n; i += (uint32_t)HVX_VLEN)
    *(HVX_UVector *)(dst + i) = *(HVX_UVector *)(src + i);
  if (i < n)
    hvx_store_partial(dst + i, *(HVX_UVector *)(src + i), n - i);
}

/* HVX forms of nrLDPC_llr2llrProcBuf / nrLDPC_llrRes2llrOut, which are pure memcpy
 * sequences -- one bulk copy plus startColParity copies of Z bytes each. Kept here rather
 * than changed in nrLDPC_mPass.h, which the ARM decoder shares. The index arithmetic
 * mirrors the reference exactly; the only difference is who does the copying. */
static inline void hvx_llr2llrProcBuf(t_nrLDPC_lut *p_lut, const int8_t *llr, int8_t *llrProcBuf, uint16_t Z, uint8_t BG)
{
  const uint8_t numBn2CnG1 = p_lut->numBnInBnGroups[0];
  const uint32_t startColParity = (BG == 1) ? NR_LDPC_START_COL_PARITY_BG1 : NR_LDPC_START_COL_PARITY_BG2;
  const uint16_t *lut_addr = p_lut->llr2llrProcBufAddr;
  const uint8_t *lut_bnpos = p_lut->llr2llrProcBufBnPos;

  if (numBn2CnG1 > 0)
    hvx_memcpy_slack(&llrProcBuf[0], &llr[startColParity * (uint32_t)Z], (uint32_t)numBn2CnG1 * Z);

  for (uint32_t i = 0; i < startColParity; i++) {
    const uint32_t idxBn = (uint32_t)lut_addr[i] + (uint32_t)lut_bnpos[i] * Z;
    hvx_memcpy_slack(&llrProcBuf[idxBn], llr, Z);
    llr += Z;
  }
}

static inline void hvx_llrRes2llrOut(t_nrLDPC_lut *p_lut, int8_t *llrOut, const int8_t *llrRes, uint16_t Z, uint8_t BG)
{
  const uint8_t numBn2CnG1 = p_lut->numBnInBnGroups[0];
  const uint32_t startColParity = (BG == 1) ? NR_LDPC_START_COL_PARITY_BG1 : NR_LDPC_START_COL_PARITY_BG2;
  const uint16_t *lut_addr = p_lut->llr2llrProcBufAddr;
  const uint8_t *lut_bnpos = p_lut->llr2llrProcBufBnPos;

  if (numBn2CnG1 > 0)
    hvx_memcpy_slack(&llrOut[startColParity * (uint32_t)Z], llrRes, (uint32_t)numBn2CnG1 * Z);

  int8_t *p_llrOut = llrOut;
  for (uint32_t i = 0; i < startColParity; i++) {
    const uint32_t idxBn = (uint32_t)lut_addr[i] + (uint32_t)lut_bnpos[i] * Z;
    hvx_memcpy_slack(p_llrOut, &llrRes[idxBn], Z);
    p_llrOut += Z;
  }
}

/* Address tables for bnProc_fused, precomputed once per (BG, Z, R).
 *
 * bnProc_fused rebuilt all of this on EVERY BP iteration: slot_to_bngrp[316], and then for
 * each of its 274 written edges a divide by NR_LDPC_ZMAX, a table lookup and two multiplies
 * by Z. --packet_analyze put 14312 scalar instructions in one call -- about 45 per edge --
 * against only 23% of packets containing any HVX at all, with DCACHE_DEMAND_MISS = 2. The
 * kernel was not waiting on memory or on its arithmetic; it was drowning in address
 * arithmetic, and this part of it depends only on the LUT and Z, which do not change
 * between iterations.
 *
 * Two entries because a transport block uses at most two rates: segments r < first_rE2 take
 * R, the rest take R2 (38.212 5.4.2.1). They are built by the DISPATCHING thread before it
 * releases the workers, so the four workers only ever read them -- building lazily inside
 * the kernel would race. Anything that cannot guarantee that (the per-segment RPC path, the
 * diag paths) passes NULL and bnProc_fused computes inline exactly as before. */
#define LDPC_ADDR_CACHE_N 2
typedef struct {
  uint8_t valid;
  uint8_t BG;
  uint8_t R;
  uint16_t Z;
  uint16_t n_edge; /* entries filled in llrPos/bn_addr */
  uint8_t slot_to_bngrp[LDPC_HEX_BN_SLOTS];
  uint32_t llrPos[NR_LDPC_NUM_EDGE_BG1];
  uint32_t bn_addr[NR_LDPC_NUM_EDGE_BG1];
} ldpc_addr_cache_t;

static ldpc_addr_cache_t g_addr_cache[LDPC_ADDR_CACHE_N];

/* Write-back BN count per CN group: mirrors bn2cn's BNs[] array in
 * nrLDPC_mPass.h. There is one table PER BASE GRAPH and they are NOT prefixes of
 * each other -- BG2 diverges from BG1 at groups 4 and 5 (8,10 against 6,7).
 *
 * Using the BG1 table for BG2 wrote too few BNs for those groups, so every edge
 * address from group 4 on was wrong and BG2 decoded to noise: BLER 1.0 at any Z
 * and any rate. Msg3 is always BG2, so random access could never complete with
 * the offload enabled, while BG1 data traffic was unaffected -- which is why
 * every --phy-test benchmark looked clean and only a live UE exposed it. */
static const int LDPC_BNS_WRITE_BG1[NR_LDPC_NUM_CN_GROUPS_BG1] = {2, 3, 4, 5, 6, 7, 8, 9, 19};
static const int LDPC_BNS_WRITE_BG2[NR_LDPC_NUM_CN_GROUPS_BG2] = {2, 3, 4, 5, 8, 10};

/* Byte stride between the BN slabs of CN group g in cn_msg. The layout is the
 * full-graph one at every code rate -- BG1 R13, BG2 R15 -- exactly as in OAI's
 * llr2CnProcBuf, cnProc and bn2cnProcBuf; only the number of CNs USED per group
 * shrinks with the rate. Taking the stride from the per-rate numCnInCnGroups
 * instead read and wrote the wrong slabs in every group whose count differs from
 * the full graph's: BG1 R23 (group 4) and BG2 R13/R23 decoded to noise, while
 * R13, R15 and BG1 R89 (only groups 0 and 8, unchanged) were unaffected. */
static inline int32_t ldpc_cn_slab_stride(uint8_t BG, int g)
{
  return (int32_t)((BG == 1) ? lut_numCnInCnGroups_BG1_R13[g] : lut_numCnInCnGroups_BG2_R15[g]) * NR_LDPC_ZMAX;
}

/* Fill the slot -> BN-group map. sa_bn[] and lut_sb[] are always ZMAX-relative (multiples
 * of NR_LDPC_ZMAX) whatever Z is, so dividing by NR_LDPC_ZMAX gives the exact slot index. */
static void ldpc_build_slot_map(t_nrLDPC_lut *p_lut, uint8_t *slot_to_bngrp)
{
  const uint8_t *nb_bn = p_lut->numBnInBnGroups;
  const uint32_t *sa_bn = p_lut->startAddrBnGroups;
  int ci = 0, num_ci = 0;
  for (int bg = 0; bg < NR_LDPC_NUM_BN_GROUPS_BG1_R13; bg++)
    if (nb_bn[bg])
      num_ci++;
  for (int bg = 0; bg < NR_LDPC_NUM_BN_GROUPS_BG1_R13; bg++) {
    if (!nb_bn[bg])
      continue;
    uint32_t s0 = sa_bn[ci] / NR_LDPC_ZMAX;
    uint32_t s1 = (ci + 1 < num_ci) ? sa_bn[ci + 1] / NR_LDPC_ZMAX : (uint32_t)LDPC_HEX_BN_SLOTS;
    for (uint32_t sl = s0; sl < s1; sl++)
      slot_to_bngrp[sl] = (uint8_t)ci;
    ci++;
  }
}

/* Walks (g, j, i) in EXACTLY the order bnProc_fused consumes them; the kernel then just
 * steps a running index. Any change to that loop nest has to change here too -- the sim
 * harness FNV64 is what catches it if they drift. */
static void ldpc_addr_cache_build(ldpc_addr_cache_t *ac, t_nrLDPC_lut *p_lut, uint8_t BG, uint16_t Z, uint8_t R)
{
  const uint8_t *numCn = p_lut->numCnInCnGroups;
  const uint16_t *sa_llr = p_lut->startAddrBnGroupsLlr;

  ldpc_build_slot_map(p_lut, ac->slot_to_bngrp);

  /* Both the group count and the table are per-BG. numCnInCnGroups for BG2 is
   * declared [NR_LDPC_NUM_CN_GROUPS_BG2] = 6 and is NOT zero-padded to the BG1
   * length, so walking 9 groups also read three entries off the end of it. */
  const int n_cn_groups = (BG == 1) ? NR_LDPC_NUM_CN_GROUPS_BG1 : NR_LDPC_NUM_CN_GROUPS_BG2;
  const int *bns_write = (BG == 1) ? LDPC_BNS_WRITE_BG1 : LDPC_BNS_WRITE_BG2;

  uint32_t e = 0;
  for (int g = 0; g < n_cn_groups; g++) {
    int nc = (int)numCn[g];
    if (!nc)
      continue;
    for (int j = 0; j < bns_write[g]; j++) {
      const uint32_t *lut_sb = arrPos(p_lut->startAddrBnProcBuf[g], j);
      const uint8_t *lut_bp = arrPos(p_lut->bnPosBnProcBuf[g], j);
      for (int i = 0; i < nc; i++) {
        if (e >= NR_LDPC_NUM_EDGE_BG1) {
          ac->valid = 0;
          return;
        }
        uint32_t bn_base = lut_sb[i];
        uint8_t bnPos = lut_bp ? lut_bp[i] : 0;
        int bnGrp = ac->slot_to_bngrp[bn_base / NR_LDPC_ZMAX];
        ac->llrPos[e] = (uint32_t)sa_llr[bnGrp] + (uint32_t)bnPos * Z;
        ac->bn_addr[e] = bn_base + (uint32_t)bnPos * Z;
        e++;
      }
    }
  }
  ac->n_edge = (uint16_t)e;
  ac->BG = BG;
  ac->Z = Z;
  ac->R = R;
  ac->valid = 1;
}

static const ldpc_addr_cache_t *ldpc_addr_cache_find(uint8_t BG, uint16_t Z, uint8_t R)
{
  for (int i = 0; i < LDPC_ADDR_CACHE_N; i++) {
    const ldpc_addr_cache_t *ac = &g_addr_cache[i];
    if (ac->valid && ac->BG == BG && ac->Z == Z && ac->R == R)
      return ac;
  }
  return NULL;
}

/* Look up, building on demand. SINGLE-THREADED CALLERS ONLY -- the per-segment dspqueue
 * handler and the synchronous RPC entry, both of which run on one thread at a time. The
 * pool workers must never call this: their entries are built by the dispatching thread
 * before it releases them, precisely so that four workers do not race to fill the same
 * arrays. Clobbering entry 0 here is harmless, because the batch path rebuilds both entries
 * for its own (BG, Z, R) pair on every batch. */
static const ldpc_addr_cache_t *ldpc_addr_cache_get(t_nrLDPC_lut *p_lut, uint8_t BG, uint16_t Z, uint8_t R)
{
  const ldpc_addr_cache_t *ac = ldpc_addr_cache_find(BG, Z, R);
  if (ac)
    return ac;
  g_addr_cache[0].valid = 0;
  ldpc_addr_cache_build(&g_addr_cache[0], p_lut, BG, Z, R);
  return g_addr_cache[0].valid ? &g_addr_cache[0] : NULL;
}

/* Scratch slot size for the rotation scratch buffers: a whole Z plus one vector of wrap
 * replica (see hvx_circ_copy_pad). */
#define LDPC_TMP_SLOT (NR_LDPC_ZMAX + HVX_VLEN)

/* hvx_circ_copy with the Z dispatch resolved at compile time.
 *
 * hvx_circ_copy tests Z against 128/256/384 on every call, and the scatter calls it once per
 * edge -- 316 times per codeword -- on a value that is invariant for the whole codeword.
 * --packet_analyze on the scatter: only 32% of committed packets contain any HVX, the other
 * 2983 carry 6479 scalar instructions (~20 per edge) of address arithmetic and loop control,
 * and mispredicts are 10% of cycles. Hoisting the dispatch to once per codeword is the same
 * fix the cnProc row loop needed. */
__attribute__((always_inline)) static inline void hvx_circ_copy_nv(int8_t *restrict dst,
                                                                   const int8_t *restrict src,
                                                                   uint32_t r,
                                                                   const int NV)
{
  if (NV == 1) {
    HVX_Vector vs = *(HVX_UVector *)src;
    *(HVX_UVector *)dst = Q6_V_valign_VVR(vs, vs, r);
  } else if (NV == 2) {
    HVX_Vector vs0 = *(HVX_UVector *)src;
    HVX_Vector vs1 = *(HVX_UVector *)(src + 128);
    HVX_Vector q0, q1;
    if (r < 128u) {
      q0 = Q6_V_valign_VVR(vs1, vs0, r);
      q1 = Q6_V_valign_VVR(vs0, vs1, r);
    } else {
      uint32_t rr = r - 128u;
      q0 = Q6_V_valign_VVR(vs0, vs1, rr);
      q1 = Q6_V_valign_VVR(vs1, vs0, rr);
    }
    *(HVX_UVector *)dst = q0;
    *(HVX_UVector *)(dst + 128) = q1;
  } else {
    HVX_Vector vs0 = *(HVX_UVector *)src;
    HVX_Vector vs1 = *(HVX_UVector *)(src + 128);
    HVX_Vector vs2 = *(HVX_UVector *)(src + 256);
    HVX_Vector q0, q1, q2;
    if (r < 128u) {
      q0 = Q6_V_valign_VVR(vs1, vs0, r);
      q1 = Q6_V_valign_VVR(vs2, vs1, r);
      q2 = Q6_V_valign_VVR(vs0, vs2, r);
    } else if (r < 256u) {
      uint32_t rr = r - 128u;
      q0 = Q6_V_valign_VVR(vs2, vs1, rr);
      q1 = Q6_V_valign_VVR(vs0, vs2, rr);
      q2 = Q6_V_valign_VVR(vs1, vs0, rr);
    } else {
      uint32_t rr = r - 256u;
      q0 = Q6_V_valign_VVR(vs0, vs2, rr);
      q1 = Q6_V_valign_VVR(vs1, vs0, rr);
      q2 = Q6_V_valign_VVR(vs2, vs1, rr);
    }
    *(HVX_UVector *)dst = q0;
    *(HVX_UVector *)(dst + 128) = q1;
    *(HVX_UVector *)(dst + 256) = q2;
  }
}

// Circular copy for arbitrary Z: dst[z] = src[(z+r)%Z] for z in [0,Z).
// Use hvx_circ_copy instead for Z in {128,256,384} (register-only, no scratch).
//
// src must have HVX_VLEN bytes of WRITABLE slack after Z; src[Z..Z+127] is overwritten
// with a replica of src[0..127]. That one store is what makes the whole thing branchless:
// with the ring's start repeated after its end, destination chunk m is just the 128 bytes
// at src + ((m*128 + r) mod Z) -- a single unaligned load, no valign, no pair selection,
// and no scalar pass at all.
//
// The version this replaces assembled the rotated copy with two scalar memcpys into a
// stack buffer before storing it out. That is two extra passes over the data per edge per
// iteration, and it is why odd Z stayed ~7x slower than Z=256 even after the scalar tails
// were gone. Use hvx_circ_copy_ro when src is read-only.
static void hvx_circ_copy_pad(int8_t *restrict dst, int8_t *restrict src, uint32_t r, uint16_t Z)
{
  *(HVX_UVector *)(src + Z) = *(HVX_UVector *)src; /* wrap replica */

  uint32_t padded = (((uint32_t)Z + HVX_VLEN - 1u) / HVX_VLEN) * HVX_VLEN;
  for (uint32_t m = 0; m < padded; m += HVX_VLEN) {
    uint32_t o = m + r; /* m < Z and r < Z, so o < 2Z */
    if (o >= (uint32_t)Z)
      o -= (uint32_t)Z;
    hvx_store_partial(dst + m, *(HVX_UVector *)(src + o), (uint32_t)Z - m);
  }
}

// Same rotation, for a src that must not be written (the channel-LLR scatter reads the
// caller's llr buffer). Stages through a local ring copy, then stores out. Called once per
// codeword rather than once per edge per iteration, so the extra pass does not matter.
static void hvx_circ_copy_ro(int8_t *restrict dst, const int8_t *restrict src, uint32_t r, uint16_t Z)
{
  int8_t ring[LDPC_TMP_SLOT] __attribute__((aligned(128)));

  /* hvx_memcpy, not __builtin_memcpy: the source may be in VTCM, where a scalar access
   * costs ~20 cycles and bypasses L1/L2 entirely. */
  if (r == 0) {
    hvx_memcpy_slack(ring, src, (uint32_t)Z);
  } else {
    uint32_t rem = (uint32_t)Z - r;
    hvx_memcpy_slack(ring, src + r, rem); // src[r..Z-1] -> ring[0..rem-1]
    hvx_memcpy_slack(ring + rem, src, r); // src[0..r-1] -> ring[rem..Z-1]
  }

  uint32_t padded = (((uint32_t)Z + HVX_VLEN - 1u) / HVX_VLEN) * HVX_VLEN;
  for (uint32_t i = 0; i < padded; i += HVX_VLEN)
    hvx_store_partial(dst + i, *(HVX_UVector *)(ring + i), (uint32_t)Z - i);
}

// HVX scatter: channel LLRs → cn_msg (replaces nrLDPC_llr2CnProcBuf_BG1).
// Works for any Z; uses hvx_circ_copy for aligned Z, hvx_circ_copy_ro otherwise.
/* Body of the BG1 channel-LLR scatter, specialised on NV = Z/HVX_VLEN (0 = the general
 * odd-Z path). Dispatched ONCE per codeword: Z is invariant, and the version that tested it
 * inside the innermost loop spent two thirds of its committed packets on scalar work. */
__attribute__((always_inline)) static inline void hvx_llr2CnProcBuf_BG1_body(t_nrLDPC_lut *p_lut,
                                                                             const int8_t *llr,
                                                                             int8_t *cn_msg,
                                                                             uint16_t Z,
                                                                             const int NV)
{
  const uint8_t *numCn = p_lut->numCnInCnGroups;
  const uint32_t *sa = p_lut->startAddrCnGroups;

  // Group 0: 3 BN connections, 1D LUT (j-indexed only, no inner i-loop)
  {
    uint32_t bitOff = (uint32_t)lut_numCnInCnGroups_BG1_R13[0] * NR_LDPC_ZMAX;
    const uint16_t *lut_cs = p_lut->circShift[0].d;
    const uint8_t *lut_pos = p_lut->posBnInCnProcBuf[0].d;
    for (int j = 0; j < 3; j++) {
      const int8_t *src = llr + (uint32_t)lut_pos[j] * Z;
      int8_t *dst0 = &cn_msg[sa[0] + (uint32_t)j * bitOff];
      if (NV)
        hvx_circ_copy_nv(dst0, src, (uint32_t)lut_cs[j], NV);
      else
        hvx_circ_copy_ro(dst0, src, (uint32_t)lut_cs[j], Z);
    }
  }

  // Groups 1-8: 2D LUT via arrPos, BNs = {4,5,6,7,8,9,10,19}
  static const int BNs[] = {3, 4, 5, 6, 7, 8, 9, 10, 19};
  for (int bn = 1; bn < (int)(sizeof(BNs) / sizeof(BNs[0])); bn++) {
    int nc = (int)numCn[bn];
    if (!nc)
      continue;
    uint32_t bitOff = (uint32_t)lut_numCnInCnGroups_BG1_R13[bn] * NR_LDPC_ZMAX;
    for (int j = 0; j < BNs[bn]; j++) {
      const uint16_t *lut_cs = arrPos(p_lut->circShift[bn], j);
      const uint8_t *lut_pos = arrPos(p_lut->posBnInCnProcBuf[bn], j);
      int8_t *dst = &cn_msg[sa[bn] + (uint32_t)j * bitOff];
      /* TRIED AND SLOWER -- do not redo. --packet_analyze names the two worst
       * packets here precisely: `r19 += mpyi(r11,r24)` computing
       * llr + lut_pos[i]*Z, and the `vmemu(r19)` immediately after it, each stalling
       * 2.00 cycles per commit because the vector load cannot issue until the address
       * multiply retires. Computing edge i+1's address before copying edge i -- the
       * same one-step-behind shape that works in the cnProc rotation -- makes it
       * WORSE at every Z: 15072 -> 15807 (Z=384), 12942 -> 14082 (256), 27450 -> 29331
       * (240). The extra bound check and live pointer cost more than the chain. */
      for (int i = 0; i < nc; i++, dst += Z) {
        const int8_t *src = llr + (uint32_t)lut_pos[i] * Z;
        if (NV)
          hvx_circ_copy_nv(dst, src, (uint32_t)lut_cs[i], NV);
        else
          hvx_circ_copy_ro(dst, src, (uint32_t)lut_cs[i], Z);
      }
    }
  }
}

LDPC_KERNEL static void hvx_llr2CnProcBuf_BG1(t_nrLDPC_lut *p_lut, const int8_t *llr, int8_t *cn_msg, uint16_t Z)
{
  const int nv = (Z % (uint16_t)HVX_VLEN == 0) ? (int)(Z / (uint16_t)HVX_VLEN) : 0;
  if (nv == 3)
    hvx_llr2CnProcBuf_BG1_body(p_lut, llr, cn_msg, Z, 3);
  else if (nv == 2)
    hvx_llr2CnProcBuf_BG1_body(p_lut, llr, cn_msg, Z, 2);
  else if (nv == 1)
    hvx_llr2CnProcBuf_BG1_body(p_lut, llr, cn_msg, Z, 1);
  else
    hvx_llr2CnProcBuf_BG1_body(p_lut, llr, cn_msg, Z, 0);
}

// HVX scatter: channel LLRs → cn_msg (replaces nrLDPC_llr2CnProcBuf_BG2).
// Works for any Z; uses hvx_circ_copy for aligned Z, hvx_circ_copy_ro otherwise.
static void hvx_llr2CnProcBuf_BG2(t_nrLDPC_lut *p_lut, const int8_t *llr, int8_t *cn_msg, uint16_t Z)
{
  const uint8_t *numCn = p_lut->numCnInCnGroups;
  const uint32_t *sa = p_lut->startAddrCnGroups;

  static const int BNs[] = {3, 4, 5, 6, 8, 10};
  int aligned = (Z % (uint16_t)HVX_VLEN == 0);
  for (int bn = 0; bn < (int)(sizeof(BNs) / sizeof(BNs[0])); bn++) {
    int nc = (int)numCn[bn];
    if (!nc)
      continue;
    uint32_t bitOff = (uint32_t)lut_numCnInCnGroups_BG2_R15[bn] * NR_LDPC_ZMAX;
    for (int j = 0; j < BNs[bn]; j++) {
      const uint16_t *lut_cs = arrPos(p_lut->circShift[bn], j);
      const uint8_t *lut_pos = arrPos(p_lut->posBnInCnProcBuf[bn], j);
      int8_t *dst = &cn_msg[sa[bn] + (uint32_t)j * bitOff];
      const int8_t *src0 = llr + (uint32_t)lut_pos[0] * Z;
      for (uint32_t off = 0; off < (uint32_t)Z; off += HVX_VLEN)
        DCFETCH128_IN(src0 + off);
      for (int i = 0; i < nc; i++, dst += Z) {
        const int8_t *src = llr + (uint32_t)lut_pos[i] * Z;
        if (i + 1 < nc) {
          const int8_t *nxt = llr + (uint32_t)lut_pos[i + 1] * Z;
          for (uint32_t off = 0; off < (uint32_t)Z; off += HVX_VLEN)
            DCFETCH128_IN(nxt + off);
        }
        if (aligned)
          hvx_circ_copy(dst, src, (uint32_t)lut_cs[i], Z);
        else
          hvx_circ_copy_ro(dst, src, (uint32_t)lut_cs[i], Z);
      }
    }
  }
}

// =============================================================================
// cnProc_fused_group — HVX min-sum + scatter to bn_msg in one pass
// =============================================================================
// Replaces hvx_cnProc_group + nrLDPC_cn2bnProcBuf scatter.
//
// After computing the CN extrinsic for all numBN edges of CN i, the results
// are written into a stack scratch buffer (tmp), then inv_circ_memcpy'd
// directly into bn_msg at the BN-indexed addresses given by the LUT.
//
// Working set for tmp: numBN × NR_LDPC_ZMAX = 19 × 384 = 7296 bytes.
// NR_LDPC_ZMAX=384=3×128 → tmp[k] at k*384 is 128-byte aligned (HVX-safe).
// The entire tmp buffer fits in L1 (32 KB), so the scatter from tmp → bn_msg
// reads from L1 rather than L2, eliminating the 121 KB cnProcBufRes readback
// that the old separate scatter pass required.

/* One CN row, with Pass 2 writing the ROTATED extrinsic straight into bn_msg.
 *
 * Why: --packet_analyze says cnProc is bound by HVX issue bandwidth (COPROC_BUSY is 72% of
 * its stall cycles, CU_BUSY only 10%), and the tmp[] round-trip was pure HVX op count --
 * Pass 2 stored every extrinsic vector to tmp[] and the circular scatter loaded it right
 * back. Producing the rotation from registers removes both, ~2900 of the kernel's ~7100
 * vector memory ops per BP iteration.
 *
 * NV (= Z/HVX_VLEN) MUST be a literal at every call site. Pass 2 has to run k-outer /
 * chunk-inner, because a rotated store needs several chunks of the SAME k at once, and that
 * makes the per-chunk 2-best state (m1/m2/sg) chunk-indexed. With a variable bound those
 * arrays go to the stack and the reloads cost more than the fold saves -- the same trap as
 * the 2-layer 16QAM metric loop in inner_rx_ml2_hvx.c. With NV literal the loops unroll,
 * the indices are constants and all of it stays in registers.
 *
 * The rotation keeps only THREE vectors live rather than all NV. dst chunk m is
 * valign(e[q+1], e[q]) with q = (m + r/HVX_VLEN) mod NV, so walking q upwards emits one
 * store per chunk one step behind the producer, and the ring's wrap needs e[0] kept to the
 * end: hence first / prev / cur. m is only ever a store OFFSET, never a register index.
 *
 * This reproduces hvx_circ_copy exactly -- dst[j] = src[(j + r) mod Z] -- by a different
 * route, so the two can drift. The sim harness FNV64 over cn_msg/bn_msg/llrRes/llrout is
 * the guard: it must not move.
 *
 * TRIED AND SLOWER -- do not redo. Pass 2 re-reads every vk that Pass 1 already loaded,
 * 1422 vector loads per BP iteration, so the obvious next step is to hold the row's cn_msg
 * vectors in registers and fuse the two passes. It only fits for low-degree CN groups
 * (NV*numBN vectors plus 3*NV of 2-best state, against 32 registers), but BG1 R=1/3 has
 * 51% of its 316 edges at degree <= 6, so the coverage looked worth having.
 *
 * On hardware it loses, monotonically in the degree covered (273 PRB / 1 layer / MCS 20,
 * two reps): cnProc 52 us/codeword off, 53 fusing degree <= 6, 57 fusing <= 7; ULSCH
 * segments decoding 1289 -> 1312 -> 1321 us/slot. Not spills -- no variant has a
 * stack-relative vmem anywhere in its disassembly.
 *
 * cnProc is ALU-bound, not load-bound: HVXPIPE_ALU is 80514 against 28440 vector loads,
 * and on the device cn_msg is in VTCM, where a load is ~1 cycle and rides in the same
 * packet as the ALU work that is the actual constraint. Removing loads frees nothing,
 * while holding 12-21 vectors live costs the scheduler the freedom to interleave.
 *
 * Two traps, both worth remembering. The probe that sold the idea deleted Pass 2's load by
 * substituting a register and showed 652205 -> 549369 cycles (15.8%) -- but that let the
 * compiler fold the dependent vabs and vcmp_eq as well, so it measured "remove the load AND
 * its ALU work". A probe that removes a value must keep the arithmetic that consumed it.
 * And hexagon-sim agreed only in sign and only barely (2.3% at best, where hardware lost
 * 10%): it holds these buffers in heap and models no VTCM, so it overvalues every removed
 * load. */
__attribute__((always_inline)) static inline void cnProc_fused_row(const int8_t *cn_msg,
                                                                   int8_t *bn_msg,
                                                                   uint32_t startAddr,
                                                                   int numBN,
                                                                   int numCN,
                                                                   int i,
                                                                   uint16_t Z,
                                                                   int32_t bitOff,
                                                                   int32_t row,
                                                                   const uint16_t *lut_cs,
                                                                   const uint32_t *lut_sb,
                                                                   const uint8_t *lut_bp,
                                                                   int cs_dim,
                                                                   int sb_dim,
                                                                   int bp_dim,
                                                                   HVX_Vector vzero,
                                                                   HVX_Vector vmax_ub,
                                                                   const int NV)
{
  HVX_Vector m1[3], m2[3], sg[3];

  /* Pass 1, chunk-outer: the 2-best and sign reduction over all k, per chunk.
   *
   * The 2-best update is THREE ALU ops, not the four the textbook form needs:
   *     cand  = max(vmin1, vabs)      // the OLD vmin1, so this must come first
   *     vmin2 = min(vmin2, cand)
   *     vmin1 = min(vmin1, vabs)
   * against cmp -> mux -> min -> min. If vabs < vmin1 the max IS the old vmin1, which is
   * exactly what the mux selected; otherwise it is vabs, likewise; ties agree as well.
   * Bit-exact -- the sim FNV64 does not move at any Z.
   *
   * It is worth the trouble because --packet_analyze says this kernel is ALU-bound
   * (HVXPIPE_ALU 80514 against 28440 vector loads), so an ALU op removed is the only kind
   * of saving that reaches the bottom line. See the register-fusion note above for what
   * happens when you go after the loads instead. */
#pragma clang loop unroll(full)
  for (int c = 0; c < NV; c++) {
    const int32_t voff = c * HVX_VLEN;
    HVX_Vector vmin1 = vmax_ub, vmin2 = vmax_ub, vsgn = vzero;
    for (int k = 0; k < numBN; k++) {
      HVX_Vector vk = *(HVX_UVector *)(cn_msg + startAddr + (int32_t)k * bitOff + row + voff);
      HVX_Vector vabs = Q6_Vb_vabs_Vb(vk);
      vsgn = Q6_V_vxor_VV(vsgn, vk);
      /* 2-best in three ALU ops, not four -- see the note on Pass 1 below. */
      HVX_Vector cand = Q6_Vub_vmax_VubVub(vmin1, vabs);
      vmin2 = Q6_Vub_vmin_VubVub(vmin2, cand);
      vmin1 = Q6_Vub_vmin_VubVub(vmin1, vabs);
    }
    m1[c] = vmin1;
    m2[c] = vmin2;
    sg[c] = vsgn;
  }

  /* Pass 2, k-outer: extrinsic per chunk, rotated into bn_msg as it is produced. */
  for (int k = 0; k < numBN; k++) {
    uint32_t bn_addr = lut_sb[k * sb_dim + i];
    if (lut_bp)
      bn_addr += (uint32_t)lut_bp[k * bp_dim + i] * Z;
    int8_t *dst = &bn_msg[bn_addr];
    const uint32_t cs = (uint32_t)lut_cs[k * cs_dim + i];
    const uint32_t r = (cs == 0u) ? 0u : (uint32_t)Z - cs;
    const uint32_t sh = r / (uint32_t)HVX_VLEN; /* whole chunks; < NV */
    const uint32_t rr = r % (uint32_t)HVX_VLEN;

    HVX_Vector efirst = vzero, eprev = vzero, ecur = vzero;
#pragma clang loop unroll(full)
    for (int c = 0; c < NV; c++) {
      const int32_t voff = c * HVX_VLEN;
      HVX_Vector vk = *(HVX_UVector *)(cn_msg + startAddr + (int32_t)k * bitOff + row + voff);
      HVX_Vector vabs = Q6_Vb_vabs_Vb(vk);
      HVX_Vector vmag = Q6_V_vmux_QVV(Q6_Q_vcmp_eq_VbVb(vabs, m1[c]), m2[c], m1[c]);
      HVX_Vector vsign_j = Q6_V_vxor_VV(sg[c], vk);
      HVX_VectorPred is_neg = Q6_Q_vcmp_gt_VbVb(vzero, vsign_j);
      ecur = Q6_V_vmux_QVV(is_neg, Q6_Vb_vsub_VbVb(vzero, vmag), vmag);
      if (c == 0) {
        efirst = ecur;
      } else {
        /* q = c-1 is now complete: (e[q+1], e[q]) = (ecur, eprev) */
        uint32_t m = (uint32_t)(c - 1) + (uint32_t)NV - sh; /* in [1, 2*NV-1] */
        if (m >= (uint32_t)NV)
          m -= (uint32_t)NV;
        *(HVX_UVector *)(dst + m * HVX_VLEN) = Q6_V_valign_VVR(ecur, eprev, rr);
      }
      eprev = ecur;
    }
    /* the wrap: q = NV-1, whose upper neighbour is chunk 0 */
    {
      uint32_t m = (uint32_t)(NV - 1) + (uint32_t)NV - sh;
      if (m >= (uint32_t)NV)
        m -= (uint32_t)NV;
      *(HVX_UVector *)(dst + m * HVX_VLEN) = Q6_V_valign_VVR(efirst, ecur, rr);
    }
  }
}

LDPC_KERNEL static void cnProc_fused_group(const int8_t *cn_msg,
                                           int8_t *bn_msg,
                                           uint32_t startAddr,
                                           int numBN,
                                           int numCN,
                                           int32_t bitOff, // ldpc_cn_slab_stride()
                                           uint16_t Z,
                                           const uint16_t *lut_cs, // circShift[g].d:          index [k*cs_dim + i]
                                           const uint32_t *lut_sb, // startAddrBnProcBuf[g].d: index [k*sb_dim + i]
                                           const uint8_t *lut_bp, // bnPosBnProcBuf[g].d:     index [k*bp_dim + i]
                                           /* Row strides of those tables, their dim2 -- what OAI's arrPos()
                                            * uses. NOT numCN: circShift is per Z and always full-graph width
                                            * (20 for BG2 group 1), while numCN shrinks with the code rate (8
                                            * at BG2 R13). Indexing it by numCN read the wrong edge's shift for
                                            * every k > 0 at any rate below the full graph. */
                                           int cs_dim,
                                           int sb_dim,
                                           int bp_dim)
{
  /* Chunk count ROUNDED UP: process ceil(Z/128) whole vectors and let the last one run
   * past Z into padding, rather than dropping to a scalar tail for the Z % 128 bytes.
   *
   * This is safe, not merely tolerable. HVX is lane-wise, so the lanes past Z compute
   * garbage independently of the valid lanes, and Pass 2 deposits that garbage in
   * tmp[]'s padding -- tmp has a 384-byte slot per k, and the scatter below copies only
   * Z bytes out of it, so the garbage is never read. Pass 1's over-read cannot leave the
   * k-slab either: rows are at stride Z inside a slab of numCN*NR_LDPC_ZMAX, and
   * (numCN-1)*Z + padded <= numCN*384 because both Z and padded are <= 384.
   *
   * The scalar tail this replaces was catastrophic, not just slow: it did two passes of
   * numBN SCALAR reads per tail byte, and cn_msg is in VTCM where a scalar access is
   * ~20 cycles. At Z=240 cnProc cost 9288192 cycles against Z=256's 174552 -- 53x more
   * for less data. It also meant Z < 128 was decoded entirely scalar (full = 0). */
  int32_t padded = (int32_t)(((uint32_t)Z + HVX_VLEN - 1u) / HVX_VLEN) * HVX_VLEN;

  int8_t tmp[LDPC_HEX_MAX_BN_PER_CN_GROUP * LDPC_TMP_SLOT] __attribute__((aligned(128)));

  HVX_Vector vzero = Q6_V_vzero();
  HVX_Vector vmax_ub = Q6_Vb_vsplat_R(0x7f);

  /* Whole chunks per row, or 0 when Z is not a multiple of the vector length. Only the
   * exact-multiple case can skip tmp[]: a partial chunk needs the scalar tail, which
   * writes bytes rather than vectors. NR_LDPC_ZMAX is 384, so 1..3 is exhaustive. */
  const int nv = (Z % (uint16_t)HVX_VLEN == 0) ? (int)(Z / (uint16_t)HVX_VLEN) : 0;

  for (int i = 0; i < numCN; i++) {
    int32_t row = i * Z;

    /* Literal NV per call: see cnProc_fused_row. */
#if LDPC_CNPROC_FOLD
    if (nv == 3) {
      cnProc_fused_row(cn_msg,
                       bn_msg,
                       startAddr,
                       numBN,
                       numCN,
                       i,
                       Z,
                       bitOff,
                       row,
                       lut_cs,
                       lut_sb,
                       lut_bp,
                       cs_dim,
                       sb_dim,
                       bp_dim,
                       vzero,
                       vmax_ub,
                       3);
      continue;
    } else if (nv == 2) {
      cnProc_fused_row(cn_msg,
                       bn_msg,
                       startAddr,
                       numBN,
                       numCN,
                       i,
                       Z,
                       bitOff,
                       row,
                       lut_cs,
                       lut_sb,
                       lut_bp,
                       cs_dim,
                       sb_dim,
                       bp_dim,
                       vzero,
                       vmax_ub,
                       2);
      continue;
    } else if (nv == 1) {
      cnProc_fused_row(cn_msg,
                       bn_msg,
                       startAddr,
                       numBN,
                       numCN,
                       i,
                       Z,
                       bitOff,
                       row,
                       lut_cs,
                       lut_sb,
                       lut_bp,
                       cs_dim,
                       sb_dim,
                       bp_dim,
                       vzero,
                       vmax_ub,
                       1);
      continue;
    }
#else
    (void)nv;
#endif

    // Process each 128-byte chunk: pass1 (min/sgn) then pass2 (extrinsic → tmp)
    for (int32_t voff = 0; voff < padded; voff += HVX_VLEN) {
      // Prime L1 for cn_msg reads (same benefit as before)
#if (LDPC_CNPROC_STAGES & 1)
      for (int k = 0; k < numBN; k++)
        DCFETCH128_MSG(cn_msg + startAddr + (int32_t)k * bitOff + row + voff);
#endif

      /* Pass 1: min1, min2, sgn_all.
       *
       * PMU under hexagon-sim --timing, per BP iteration: ICACHE_DEMAND_MISS 0,
       * DCACHE_DEMAND_MISS 61, CU_PKT_READY_NOT_DISPATCHED 0, yet 61% of cycles are
       * bubbles, 64% of committed packets carry EXACTLY ONE bubble, and packets hold
       * only 2.12 instructions of 4 slots. So: not memory, not issue slots.
       *
       * --packet_analyze then attributed the stalls: COPROC_BUSY (the HVX
       * coprocessor is busy) is 72% of stall cycles and 37.5% of all cycles, while
       * CU_BUSY -- the category that CONTAINS register interlocks -- is 5%. The
       * kernel is waiting on HVX issue bandwidth, not on itself. See the README for
       * the full table and the two levers that follow from it (fold the rotation
       * into Pass 2 to drop tmp[]; fuse Pass 1 and Pass 2 for low-degree groups).
       *
       * TRIED AND SLOWER -- do not redo. The obvious reading of "one bubble per
       * packet" is that m1/m2/sgn form a serial chain across k (m2's path
       * vcmp->vmux->vmin is three dependent ops deep), so the reduction was split
       * into W independent (m1,m2) streams merged at the end with
       *   min1 = min(a1,b1);  min2 = min(max(a1,b1), min(a2,b2))
       * which is exact including multiplicity -- the FNV64 over cn_msg/bn_msg/
       * llrRes/llrout was identical for W=1..5. It was also monotonically SLOWER:
       * 1427717 cycles at W=1 against 1478943 / 1483389 / 1496196 / 1487942 for
       * W=2..5 (cnProc alone, 32 iterations). The dependency chain is therefore not
       * what this loop is waiting on. */
      HVX_Vector vmin1 = vmax_ub, vmin2 = vmax_ub, vsgn_all = vzero;
#if (LDPC_CNPROC_STAGES & 2)
      for (int k = 0; k < numBN; k++) {
        HVX_Vector vk = *(HVX_UVector *)(cn_msg + startAddr + (int32_t)k * bitOff + row + voff);
        HVX_Vector vabs = Q6_Vb_vabs_Vb(vk);
        vsgn_all = Q6_V_vxor_VV(vsgn_all, vk);
        /* 2-best in three ALU ops, not four -- see the note on Pass 1 below. */
        HVX_Vector cand = Q6_Vub_vmax_VubVub(vmin1, vabs);
        vmin2 = Q6_Vub_vmin_VubVub(vmin2, cand);
        vmin1 = Q6_Vub_vmin_VubVub(vmin1, vabs);
      }
#endif

      // Pass 2: extrinsic for each k → tmp[k][voff..voff+127]
#if (LDPC_CNPROC_STAGES & 4)
      for (int k = 0; k < numBN; k++) {
        HVX_Vector vk = *(HVX_UVector *)(cn_msg + startAddr + (int32_t)k * bitOff + row + voff);
        HVX_Vector vabs = Q6_Vb_vabs_Vb(vk);
        HVX_Vector vmag = Q6_V_vmux_QVV(Q6_Q_vcmp_eq_VbVb(vabs, vmin1), vmin2, vmin1);
        HVX_Vector vsign_j = Q6_V_vxor_VV(vsgn_all, vk);
        HVX_VectorPred is_neg = Q6_Q_vcmp_gt_VbVb(vzero, vsign_j);
        HVX_Vector vneg = Q6_Vb_vsub_VbVb(vzero, vmag);
        *(HVX_UVector *)(tmp + (int32_t)k * LDPC_TMP_SLOT + voff) = Q6_V_vmux_QVV(is_neg, vneg, vmag);
      }
#endif
    }

    // Scatter: inv_circ_memcpy from L1 tmp[k] → bn_msg at BN-indexed address
#if (LDPC_CNPROC_STAGES & 8)
    for (int k = 0; k < numBN; k++) {
      uint32_t bn_addr = lut_sb[k * sb_dim + i];
      if (lut_bp)
        bn_addr += (uint32_t)lut_bp[k * bp_dim + i] * Z;
      int8_t *tsrc = tmp + (int32_t)k * LDPC_TMP_SLOT;
      uint32_t cs = (uint32_t)lut_cs[k * cs_dim + i];
      uint32_t r = (cs == 0u) ? 0u : (uint32_t)Z - cs;
      if (Z % (uint16_t)HVX_VLEN == 0)
        hvx_circ_copy(&bn_msg[bn_addr], tsrc, r, Z);
      else
        hvx_circ_copy_pad(&bn_msg[bn_addr], tsrc, r, Z);
    }
#endif
  }
}

static void cnProc_fused(const int8_t *cn_msg, int8_t *bn_msg, t_nrLDPC_lut *p_lut, uint16_t Z, uint8_t BG)
{
  const int8_t *tbl = (BG == 1) ? numBN_BG1 : numBN_BG2;
  int ngrp = (BG == 1) ? NR_LDPC_NUM_CN_GROUPS_BG1 : NR_LDPC_NUM_CN_GROUPS_BG2;
  for (int g = 0; g < ngrp; g++) {
    int numCN = (int)p_lut->numCnInCnGroups[g];
    if (!numCN)
      continue;
    cnProc_fused_group(cn_msg,
                       bn_msg,
                       p_lut->startAddrCnGroups[g],
                       (int)tbl[g],
                       numCN,
                       ldpc_cn_slab_stride(BG, g),
                       Z,
                       p_lut->circShift[g].d,
                       p_lut->startAddrBnProcBuf[g].d,
                       p_lut->bnPosBnProcBuf[g].d,
                       p_lut->circShift[g].dim2,
                       p_lut->startAddrBnProcBuf[g].dim2,
                       p_lut->bnPosBnProcBuf[g].dim2);
  }
}

// =============================================================================
// HVX helpers — saturating int8 arithmetic via int16 widening
// =============================================================================
// HVX has no single-instruction signed-byte saturating add/sub.
// Strategy: widen to int16 (Q6_Wh_vsxt_Vb), add/sub in 16-bit, clamp to
// [-128,127] with vmin/vmax, pack low bytes back.
//
// Q6_Wh_vsxt_Vb DEINTERLEAVES: even-indexed bytes → lo halfwords,
// odd-indexed bytes → hi halfwords.  After accumulation, vpacke(hi, lo)
// therefore gives [even_results | odd_results], not the correct linear
// order.  Q6_Vb_vshuff_Vb re-interleaves the two halves:
//   vshuff([e0..e63 | o0..o63]) → [e0,o0, e1,o1, ..., e63,o63]
// This must be applied after every vpacke to restore linear byte order.

/* Saturating signed-byte add/sub. These are ONE instruction each on HVX --
 * Vd.b = vadd(Vu.b, Vv.b):sat, CVI_VA, SLOT0123 -- and used to be about twelve: sign-extend
 * both operands to halfword pairs, add, clamp with vmin/vmax against splatted +127/-128,
 * pack the low bytes, vshuff back into order. Identical result, and it runs per edge per
 * 128-byte chunk in bnProc_fused and hvx_bnProcPc, so the difference is not small.
 *
 * The v127/vn128 arguments are what the emulation needed; kept so the call sites do not all
 * have to change, and unused. */
static inline HVX_Vector hvx_sat8_add(HVX_Vector va, HVX_Vector vb, HVX_Vector v127, HVX_Vector vn128)
{
  (void)v127;
  (void)vn128;
  return Q6_Vb_vadd_VbVb_sat(va, vb);
}
static inline HVX_Vector hvx_sat8_sub(HVX_Vector va, HVX_Vector vb, HVX_Vector v127, HVX_Vector vn128)
{
  (void)v127;
  (void)vn128;
  return Q6_Vb_vsub_VbVb_sat(va, vb);
}

// =============================================================================
// cnProcPc_fused — parity check using llrRes (posterior LLRs, BN-indexed)
// =============================================================================
// llrRes[b] = posterior LLR for BN b, written by hvx_bnProcPc.  The parity
// check XORs sign(llrRes[b]) across all BNs b connected to each CN.
//
// slot_to_bngrp[] — built once at entry, replaces the 30-entry linear BN-group
//   search that was O(numBnGroups) per edge. sa_bn[] values are NR_LDPC_ZMAX
//   multiples, so bn_base/NR_LDPC_ZMAX is the exact integer slot index.
//
// Two-pass per CN to hide L2 latency for llrRes and eliminate par[] memory WAR:
//   Pass 1 (over k edges): O(1) slot-table lookup → llrPos_arr[k] + DCFETCH all
//     three 128-byte chunks of each llrRes column ahead of the HVX loop.
//   Pass 2 (over k edges): HVX valign+vxor into register accumulators vpar0/1/2.
//     No stores to par[] during accumulation → no store-to-load (WAR) stalls.
//
// Parity check: horizontal unsigned max over vpar0/1/2 using 7-step vror+vmax
//   reduction.  Byte[0] of the result is the max of all Z bytes; if >= 0x80
//   at least one parity byte has its sign bit set → parity failure.
//   Cost: ~15 HVX ops per CN vs the old 384-iteration scalar loop.

#ifdef LDPC_SIM_STANDALONE
/* Scalar oracle for cnProcPc_fused, built only into the standalone sim harness.
 *
 * Deliberately the dumbest possible transcription of the definition --
 *   par[z] = XOR over k of llrRes[llrPos_k + ((z + cs_k) mod Z)],  fail if any par[z] < 0
 * -- with no vectors and no chunking, so it cannot share a bug with the HVX version. It
 * exists because the sim's FNV64 covers only the message buffers, and cnProcPc returns a
 * flag and writes nothing, so the vector implementation had no check on it at all. That is
 * how its ring stayed wrong for every Z below 384 without anyone noticing. */
static int32_t cnProcPc_ref(const int8_t *llrRes, t_nrLDPC_lut *p_lut, uint16_t Z, uint8_t BG)
{
  const int8_t *tbl = (BG == 1) ? numBN_BG1 : numBN_BG2;
  int ngrp = (BG == 1) ? NR_LDPC_NUM_CN_GROUPS_BG1 : NR_LDPC_NUM_CN_GROUPS_BG2;
  const uint8_t *nb_bn = p_lut->numBnInBnGroups;
  const uint32_t *sa_bn = p_lut->startAddrBnGroups;
  const uint16_t *sa_llr = p_lut->startAddrBnGroupsLlr;

  uint8_t slot_to_bngrp[LDPC_HEX_BN_SLOTS];
  {
    int ci = 0, num_ci = 0;
    for (int bg = 0; bg < NR_LDPC_NUM_BN_GROUPS_BG1_R13; bg++)
      if (nb_bn[bg])
        num_ci++;
    for (int bg = 0; bg < NR_LDPC_NUM_BN_GROUPS_BG1_R13; bg++) {
      if (!nb_bn[bg])
        continue;
      uint32_t s0 = sa_bn[ci] / NR_LDPC_ZMAX;
      uint32_t s1 = (ci + 1 < num_ci) ? sa_bn[ci + 1] / NR_LDPC_ZMAX : (uint32_t)LDPC_HEX_BN_SLOTS;
      for (uint32_t sl = s0; sl < s1; sl++)
        slot_to_bngrp[sl] = (uint8_t)ci;
      ci++;
    }
  }

  for (int g = 0; g < ngrp; g++) {
    int nc = (int)p_lut->numCnInCnGroups[g];
    if (!nc)
      continue;
    int nb = (int)tbl[g];
    const uint16_t *lut_cs = p_lut->circShift[g].d;
    const uint32_t *lut_sb = p_lut->startAddrBnProcBuf[g].d;
    const uint8_t *lut_bp = p_lut->bnPosBnProcBuf[g].d;
    const int cs_dim = p_lut->circShift[g].dim2; /* row strides: see cnProc_fused_group */
    const int sb_dim = p_lut->startAddrBnProcBuf[g].dim2;
    const int bp_dim = p_lut->bnPosBnProcBuf[g].dim2;

    for (int i = 0; i < nc; i++) {
      for (uint32_t z = 0; z < (uint32_t)Z; z++) {
        int8_t par = 0;
        for (int k = 0; k < nb; k++) {
          uint32_t bn_base = lut_sb[k * sb_dim + i];
          uint8_t bnPos = lut_bp ? lut_bp[k * bp_dim + i] : 0;
          int bnGrp = slot_to_bngrp[bn_base / NR_LDPC_ZMAX];
          uint32_t llrPos = (uint32_t)sa_llr[bnGrp] + (uint32_t)bnPos * Z;
          uint32_t cs = (uint32_t)lut_cs[k * cs_dim + i];
          uint32_t bz = z + cs;
          if (bz >= (uint32_t)Z)
            bz -= (uint32_t)Z;
          par ^= llrRes[llrPos + bz];
        }
        if (par & 0x80)
          return 1;
      }
    }
  }
  return 0;
}
#endif // LDPC_SIM_STANDALONE

LDPC_KERNEL static int32_t cnProcPc_fused(const int8_t *llrRes,
                                          t_nrLDPC_lut *p_lut,
                                          uint16_t Z,
                                          uint8_t BG,
                                          const ldpc_addr_cache_t *ac)
{
  const int8_t *tbl = (BG == 1) ? numBN_BG1 : numBN_BG2;
  int ngrp = (BG == 1) ? NR_LDPC_NUM_CN_GROUPS_BG1 : NR_LDPC_NUM_CN_GROUPS_BG2;

  const uint16_t *sa_llr = p_lut->startAddrBnGroupsLlr;

  // O(1) slot->bnGrp lookup, from the cache when there is one (same map bnProc uses).
  uint8_t slot_local[LDPC_HEX_BN_SLOTS];
  const uint8_t *slot_to_bngrp;
  if (ac) {
    slot_to_bngrp = ac->slot_to_bngrp;
  } else {
    ldpc_build_slot_map(p_lut, slot_local);
    slot_to_bngrp = slot_local;
  }

  uint32_t llrPos_arr[LDPC_HEX_MAX_BN_PER_CN_GROUP];
  /* Chunks needed to cover Z, rounded up, and the valid lane count in the last one. */
  const uint32_t nchunk = (((uint32_t)Z + HVX_VLEN - 1u) / HVX_VLEN);
  const uint32_t n_last = (uint32_t)Z - (nchunk - 1u) * HVX_VLEN;
  HVX_Vector vzero = Q6_V_vzero();

  for (int g = 0; g < ngrp; g++) {
    int nc = (int)p_lut->numCnInCnGroups[g];
    if (!nc)
      continue;
    int nb = (int)tbl[g];
    const uint16_t *lut_cs = p_lut->circShift[g].d;
    const uint32_t *lut_sb = p_lut->startAddrBnProcBuf[g].d;
    const uint8_t *lut_bp = p_lut->bnPosBnProcBuf[g].d;
    const int cs_dim = p_lut->circShift[g].dim2; /* row strides: see cnProc_fused_group */
    const int sb_dim = p_lut->startAddrBnProcBuf[g].dim2;
    const int bp_dim = p_lut->bnPosBnProcBuf[g].dim2;

    for (int i = 0; i < nc; i++) {
      // Pass 1: O(1) BN group lookup + DCFETCH — fires all L2 prefetches
      // before the HVX loop so llrRes columns are warm in L1 by pass 2.
      for (int k = 0; k < nb; k++) {
        uint32_t bn_base = lut_sb[k * sb_dim + i];
        uint8_t bnPos = lut_bp ? lut_bp[k * bp_dim + i] : 0;
        int bnGrp = slot_to_bngrp[bn_base / NR_LDPC_ZMAX];
        uint32_t lp = (uint32_t)sa_llr[bnGrp] + (uint32_t)bnPos * Z;
        llrPos_arr[k] = lp;
        DCFETCH128_LLR(llrRes + lp);
        DCFETCH128_LLR(llrRes + lp + 128);
        DCFETCH128_LLR(llrRes + lp + 256);
      }

      /* Pass 2: par[z] = XOR over k of llrRes[llrPos_k + ((z + cs_k) mod Z)],
       * accumulated one 128-byte chunk at a time, entirely in registers.
       *
       * BUG, fixed here: this used to load exactly three vectors at col, col+128,
       * col+256 and valign them as if the ring were always 3x128, guarding the
       * accumulation with `full`. For any Z below 384 the wrap region then read the
       * NEXT llrRes slot instead of the ring's start, so the parity check was wrong
       * -- it erred toward reporting failure, which cost iterations rather than
       * accuracy (mean iterations sat at the maximum for every Z except 384) and was
       * masked in production, where use_crc is always true and this is never called.
       *
       * The ring is now handled for any Z. Chunk m wants the 128 bytes at ring
       * offset o = (m*128 + cs) mod Z. Only the first n = min(128, Z - m*128) lanes
       * of it are real. w = Z - o bytes are available before the ring wraps, so:
       *   w >= n : one unaligned load, no wrap in the part we use
       *   w <  n : lanes [0,w) from that load, lanes [w,n) from the ring start
       *            shifted right by w -- valign(B, vzero, 128-w) does the shift,
       *            and vsetq2(w) selects between them
       * At most one wrap per chunk, because n <= Z. The load can read up to 127
       * bytes past the slot; those lanes are never used, and llrRes carries
       * HVX_VLEN of slack. */
      HVX_Vector vpar[3] = {vzero, vzero, vzero};

      for (int k = 0; k < nb; k++) {
        uint32_t llrPos = llrPos_arr[k];
        uint32_t cs = (uint32_t)lut_cs[k * cs_dim + i];
        const int8_t *col = llrRes + llrPos;
        HVX_Vector vstart = *(HVX_UVector *)col; /* ring start, for the wrap */

#pragma clang loop unroll(full)
        for (uint32_t m = 0; m < 3u; m++) {
          if (m >= nchunk)
            continue;
          uint32_t n = (uint32_t)Z - m * HVX_VLEN;
          if (n > (uint32_t)HVX_VLEN)
            n = HVX_VLEN;
          uint32_t o = m * HVX_VLEN + cs; /* < 2Z */
          if (o >= (uint32_t)Z)
            o -= (uint32_t)Z;
          uint32_t w = (uint32_t)Z - o;
          HVX_Vector v = *(HVX_UVector *)(col + o);
          if (w < n) {
            HVX_Vector shifted = Q6_V_valign_VVR(vstart, vzero, (uint32_t)HVX_VLEN - w);
            v = Q6_V_vmux_QVV(Q6_Q_vsetq2_R((int32_t)w), v, shifted);
          }
          vpar[m] = Q6_V_vxor_VV(vpar[m], v);
        }
      }

      /* The last chunk's lanes past Z accumulated garbage; zero them before the
       * reduction, or a padding lane could report a parity failure that is not one. */
      if (n_last < (uint32_t)HVX_VLEN)
        vpar[nchunk - 1u] = Q6_V_vmux_QVV(Q6_Q_vsetq2_R((int32_t)n_last), vpar[nchunk - 1u], vzero);

      // Vectorized parity check: unsigned horizontal max over the live chunks.
      // If the max of any byte is >= 0x80 the sign bit is set → parity fail.
      // 7-step vror+vmax reduces 128 bytes to byte[0] in log2(128) rounds.
      HVX_Vector vfail = vpar[0];
      if (nchunk > 1u)
        vfail = Q6_Vub_vmax_VubVub(vfail, vpar[1]);
      if (nchunk > 2u)
        vfail = Q6_Vub_vmax_VubVub(vfail, vpar[2]);
      vfail = Q6_Vub_vmax_VubVub(vfail, Q6_V_vror_VR(vfail, 64));
      vfail = Q6_Vub_vmax_VubVub(vfail, Q6_V_vror_VR(vfail, 32));
      vfail = Q6_Vub_vmax_VubVub(vfail, Q6_V_vror_VR(vfail, 16));
      vfail = Q6_Vub_vmax_VubVub(vfail, Q6_V_vror_VR(vfail, 8));
      vfail = Q6_Vub_vmax_VubVub(vfail, Q6_V_vror_VR(vfail, 4));
      vfail = Q6_Vub_vmax_VubVub(vfail, Q6_V_vror_VR(vfail, 2));
      vfail = Q6_Vub_vmax_VubVub(vfail, Q6_V_vror_VR(vfail, 1));
      if ((uint32_t)Q6_R_vextract_VR(vfail, 0) & 0x80u)
        return 1;
    }
  }
  return 0;
}

// =============================================================================
// BN Processing — parity-check variant (HVX bnProcPc)
// =============================================================================
// For each BN b in group g (cnidx+1 connected CNs):
//   llrRes[b]         = sat8( llrProcBuf[b] + Σ_k bnProcBuf[k, b] )
//   bnProcBufRes[0,b] = llrProcBuf[b]   (stored for bnProc subtraction)
//
// Buffer layout in bnProcBuf/bnProcBufRes:
//   CN layer k: byte range [startBn + k * numBn * ZMAX  ..  + numBn * ZMAX - 1]
//   BN b within layer k: bytes [k*cnOff + b]  (b in 0..numBn*Z-1)
//
// HVX path: outer loop steps 128 bytes at a time; inner k-loop accumulates
// int16 sums in two register-pair halves, then saturates and packs to int8.
// All accesses within a group are sequential — no cache-miss-latency issue.

/* FNV-1a, matching the sim harness so hashes are comparable across both. */
/* Stage checksums for bisecting a divergence between the batch and per-segment
 * paths. Build with -DLDPC_HEX_DBG_HASH; hashing 121 kB three times per decode is
 * far too expensive to leave enabled. There is no working FARF channel on this
 * platform, so the response struct is the only way to get DSP state out. */
__attribute__((unused)) static uint64_t ldpc_fnv64(const void *p, uint32_t n)
{
  const uint8_t *q = (const uint8_t *)p;
  uint64_t h = 1469598103934665603ULL;
  for (uint32_t i = 0; i < n; i++) {
    h ^= q[i];
    h *= 1099511628211ULL;
  }
  return h;
}

LDPC_KERNEL static void hvx_bnProcPc(t_nrLDPC_lut *p_lut,
                                     const int8_t *bn_msg,
                                     const int8_t *llrProcBuf,
                                     int8_t *llrRes,
                                     uint16_t Z)
{
  const uint8_t *numBn = p_lut->numBnInBnGroups;
  const uint32_t *sBn = p_lut->startAddrBnGroups;
  const uint16_t *sLlr = p_lut->startAddrBnGroupsLlr;

  HVX_Vector v127 = Q6_Vh_vsplat_R(127);
  HVX_Vector vn128 = Q6_Vh_vsplat_R(-128);

  // Group 0: BNs connected to exactly 1 CN.
  // bnProcBufRes write removed: degree-1 BNs' extrinsic = channel LLR
  // is never read back (bnProc_fused skips this group, bn2cn skips last-j).
  {
    uint32_t nb = (uint32_t)numBn[0];
    uint32_t total = nb * (uint32_t)Z;
    uint32_t sa = sBn[0];
    uint32_t sl = sLlr[0];
    for (uint32_t b = 0; b < total; b += HVX_VLEN)
      hvx_store_partial(llrRes + sl + b,
                        hvx_sat8_add(*(HVX_UVector *)(llrProcBuf + sl + b), *(HVX_UVector *)(bn_msg + sa + b), v127, vn128),
                        total - b);
  }

  // Groups 1+: accumulate cnidx+1 CN messages per BN
  uint8_t idxBnGroup = 0;
  for (int cnidx = 1; cnidx < NR_LDPC_NUM_BN_GROUPS_BG1_R13; cnidx++) {
    if (numBn[cnidx] == 0)
      continue;
    idxBnGroup++;

    uint32_t nb = (uint32_t)numBn[cnidx];
    uint32_t cnOff = nb * NR_LDPC_ZMAX;
    uint32_t total = nb * (uint32_t)Z;
    uint32_t sa = sBn[idxBnGroup];
    uint32_t sl = sLlr[idxBnGroup];
    for (uint32_t b = 0; b < total; b += HVX_VLEN) {
      for (int k = 0; k <= cnidx; k++)
        DCFETCH128_MSG(bn_msg + sa + (uint32_t)k * cnOff + b);

      HVX_VectorPair wsum = Q6_Wh_vsxt_Vb(*(HVX_UVector *)(llrProcBuf + sl + b));
      HVX_Vector sum_lo = Q6_V_lo_W(wsum);
      HVX_Vector sum_hi = Q6_V_hi_W(wsum);

      for (int k = 0; k <= cnidx; k++) {
        HVX_VectorPair wbn = Q6_Wh_vsxt_Vb(*(HVX_UVector *)(bn_msg + sa + (uint32_t)k * cnOff + b));
        sum_lo = Q6_Vh_vadd_VhVh(sum_lo, Q6_V_lo_W(wbn));
        sum_hi = Q6_Vh_vadd_VhVh(sum_hi, Q6_V_hi_W(wbn));
      }

      /* vpackhb:sat clamps to [-128,127] and narrows in one instruction, replacing
       * four vmin/vmax against splatted +-limits plus the plain vpacke. The vshuff
       * still undoes the halfword de-interleave from vsxt. */
      hvx_store_partial(llrRes + sl + b, Q6_Vb_vshuff_Vb(Q6_Vb_vpack_VhVh_sat(sum_hi, sum_lo)), total - b);
    }
  }
}

// =============================================================================
// bnProc_fused — BN extrinsic + scatter to cn_msg in one pass (BG1 only)
// =============================================================================
// Replaces hvx_bnProc + nrLDPC_bn2cnProcBuf_BG1.
//
// Iterates over the same set of CN edges as bn2cn (j = 0..BNs_write[g]-1 per
// group, mirroring nrLDPC_bn2cnProcBuf_BG1's write set).  For each edge:
//   1. Look up the BN's llrRes address by searching startAddrBnGroups for the
//      BN group (linear scan, ≤30 entries, called 316 times per iteration).
//   2. Compute extrinsic = sat8(llrRes[b] - bn_msg[b]) into a 384-byte tmp.
//   3. nrLDPC_circ_memcpy tmp → cn_msg at the CN-indexed address.
// The last BN position in each group (parity BN, degree-1) is excluded,
// exactly as bn2cn excluded it; cn_msg at those positions retains the channel
// LLR written by llr2CnProcBuf_BG1 during initialisation.

LDPC_KERNEL static void bnProc_fused(const int8_t *bn_msg,
                                     const int8_t *llrRes,
                                     int8_t *cn_msg,
                                     t_nrLDPC_lut *p_lut,
                                     uint16_t Z,
                                     uint8_t BG,
                                     const ldpc_addr_cache_t *ac)
{
  const uint8_t *numCn = p_lut->numCnInCnGroups;
  const uint32_t *sa_cn = p_lut->startAddrCnGroups;
  const uint16_t *sa_llr = p_lut->startAddrBnGroupsLlr;

  HVX_Vector v127 = Q6_Vh_vsplat_R(127);
  HVX_Vector vn128 = Q6_Vh_vsplat_R(-128);

  int8_t tmp[LDPC_TMP_SLOT] __attribute__((aligned(128))); // used by Z != 384 path
  int32_t padded = (int32_t)(((uint32_t)Z + HVX_VLEN - 1u) / HVX_VLEN) * HVX_VLEN;

  /* Write-back BN count per CN group, shared with ldpc_addr_cache_build so the two
   * cannot disagree about the edge order. Per-BG: see the tables' comment. */
  const int n_cn_groups = (BG == 1) ? NR_LDPC_NUM_CN_GROUPS_BG1 : NR_LDPC_NUM_CN_GROUPS_BG2;
  const int *BNs_write = (BG == 1) ? LDPC_BNS_WRITE_BG1 : LDPC_BNS_WRITE_BG2;

  // Flat slot→bnGrp table: sa_bn[] and lut_sb[] are always ZMAX-relative (multiples of
  // NR_LDPC_ZMAX=384) regardless of the actual Z.  Dividing by NR_LDPC_ZMAX gives the
  // exact integer slot index in [0, NR_LDPC_NUM_EDGE_BG1).
  uint8_t slot_to_bngrp[LDPC_HEX_BN_SLOTS];
  if (!ac)
    ldpc_build_slot_map(p_lut, slot_to_bngrp);
  uint32_t e_cache = 0; /* running index into the precomputed flat tables */

  // Per-(g,j) address tables: hoists the BN-group lookup out of the hot i-loop
  // and provides the leading DCFETCH enough fill distance.
  uint32_t llrPos_loc[LDPC_HEX_MAX_NC];
  uint32_t bn_addr_loc[LDPC_HEX_MAX_NC];

  for (int g = 0; g < n_cn_groups; g++) {
    int nc = (int)numCn[g];
    if (!nc)
      continue;
    const int32_t bitOff = ldpc_cn_slab_stride(BG, g);
    int nj = BNs_write[g];

    for (int j = 0; j < nj; j++) {
      const uint16_t *lut_cs = arrPos(p_lut->circShift[g], j);
      const uint32_t *lut_sb = arrPos(p_lut->startAddrBnProcBuf[g], j);
      const uint8_t *lut_bp = arrPos(p_lut->bnPosBnProcBuf[g], j);

      /* (llrPos, bn_addr) for all nc positions. With a cache they were computed
       * once for this (BG, Z, R) before the workers started, and the loop nest here
       * consumes them in the same order they were written -- so this is a pointer
       * bump rather than nc divides, lookups and multiplies, every BP iteration. */
      /* restrict is here on principle -- the int8_t stores to cn_msg below cannot
       * alias these uint32_t reads -- but measurably it changed nothing, so do not
       * credit it with the win.
       *
       * Note hexagon-sim reports this cached path 35% SLOWER than recomputing
       * (239547 -> 322350 cycles at Z=384) while the DEVICE makes it 26% faster
       * (bnProc 31 -> 23 us/codeword). Fourth time the two have disagreed on a
       * verdict this session and the widest gap yet; the device decides. The sim has
       * every buffer in heap, so trading scalar arithmetic for table loads looks bad
       * there in a way it is not when cn_msg/bn_msg/llrRes are in VTCM. */
      const uint32_t *restrict llrPos_tab;
      const uint32_t *restrict bn_addr_tab;
      if (ac && e_cache + (uint32_t)nc <= ac->n_edge) {
        llrPos_tab = ac->llrPos + e_cache;
        bn_addr_tab = ac->bn_addr + e_cache;
        e_cache += (uint32_t)nc;
      } else {
        for (int i = 0; i < nc; i++) {
          uint32_t bn_base = lut_sb[i];
          uint8_t bnPos = lut_bp ? lut_bp[i] : 0;
          int bnGrp = slot_to_bngrp[bn_base / NR_LDPC_ZMAX];
          llrPos_loc[i] = sa_llr[bnGrp] + (uint32_t)bnPos * Z;
          bn_addr_loc[i] = bn_base + (uint32_t)bnPos * Z;
        }
        llrPos_tab = llrPos_loc;
        bn_addr_tab = bn_addr_loc;
      }

      // Leading DCFETCH for i=0: covers exactly Z bytes per buffer.
      if (nc > 0) {
        for (uint32_t off = 0; off < (uint32_t)Z; off += HVX_VLEN) {
          DCFETCH128_LLR(llrRes + llrPos_tab[0] + off);
          DCFETCH128_MSG(bn_msg + bn_addr_tab[0] + off);
        }
      }

      for (int i = 0; i < nc; i++) {
        uint32_t llrPos = llrPos_tab[i];
        uint32_t bn_addr = bn_addr_tab[i];

        // Look-ahead DCFETCH for i+1.
        if (i + 1 < nc) {
          for (uint32_t off = 0; off < (uint32_t)Z; off += HVX_VLEN) {
            DCFETCH128_LLR(llrRes + llrPos_tab[i + 1] + off);
            DCFETCH128_MSG(bn_msg + bn_addr_tab[i + 1] + off);
          }
        }

        int8_t *cdst = &cn_msg[sa_cn[g] + (int32_t)j * bitOff + i * Z];
        uint32_t cs = (uint32_t)lut_cs[i];

        if (Z == (uint16_t)HVX_VLEN) {
          // Z=128: one vector, circular rotate in-place.
          HVX_Vector r0 = hvx_sat8_sub(*(HVX_UVector *)(llrRes + llrPos), *(HVX_UVector *)(bn_msg + bn_addr), v127, vn128);
          *(HVX_UVector *)cdst = Q6_V_valign_VVR(r0, r0, cs);
        } else if (Z == 2u * (uint16_t)HVX_VLEN) {
          // Z=256: two vectors, valign across pair.
          HVX_Vector r0 = hvx_sat8_sub(*(HVX_UVector *)(llrRes + llrPos), *(HVX_UVector *)(bn_msg + bn_addr), v127, vn128);
          HVX_Vector r1 =
              hvx_sat8_sub(*(HVX_UVector *)(llrRes + llrPos + 128), *(HVX_UVector *)(bn_msg + bn_addr + 128), v127, vn128);
          HVX_Vector q0, q1;
          if (cs < 128u) {
            q0 = Q6_V_valign_VVR(r1, r0, cs);
            q1 = Q6_V_valign_VVR(r0, r1, cs);
          } else {
            uint32_t rr = cs - 128u;
            q0 = Q6_V_valign_VVR(r0, r1, rr);
            q1 = Q6_V_valign_VVR(r1, r0, rr);
          }
          *(HVX_UVector *)cdst = q0;
          *(HVX_UVector *)(cdst + 128) = q1;
        } else if (Z == 3u * (uint16_t)HVX_VLEN) {
          // Z=384: three vectors.
          HVX_Vector res0 = hvx_sat8_sub(*(HVX_UVector *)(llrRes + llrPos), *(HVX_UVector *)(bn_msg + bn_addr), v127, vn128);
          HVX_Vector res1 =
              hvx_sat8_sub(*(HVX_UVector *)(llrRes + llrPos + 128), *(HVX_UVector *)(bn_msg + bn_addr + 128), v127, vn128);
          HVX_Vector res2 =
              hvx_sat8_sub(*(HVX_UVector *)(llrRes + llrPos + 256), *(HVX_UVector *)(bn_msg + bn_addr + 256), v127, vn128);
          HVX_Vector q0, q1, q2;
          if (cs < 128u) {
            q0 = Q6_V_valign_VVR(res1, res0, cs);
            q1 = Q6_V_valign_VVR(res2, res1, cs);
            q2 = Q6_V_valign_VVR(res0, res2, cs);
          } else if (cs < 256u) {
            uint32_t rr = cs - 128u;
            q0 = Q6_V_valign_VVR(res2, res1, rr);
            q1 = Q6_V_valign_VVR(res0, res2, rr);
            q2 = Q6_V_valign_VVR(res1, res0, rr);
          } else {
            uint32_t rr = cs - 256u;
            q0 = Q6_V_valign_VVR(res0, res2, rr);
            q1 = Q6_V_valign_VVR(res1, res0, rr);
            q2 = Q6_V_valign_VVR(res2, res1, rr);
          }
          *(HVX_UVector *)cdst = q0;
          *(HVX_UVector *)(cdst + 128) = q1;
          *(HVX_UVector *)(cdst + 256) = q2;
        } else {
          /* Z not in {128,256,384}, including Z < 128. Compute ceil(Z/128)
           * whole vectors into tmp and let the last run past Z: tmp is a
           * 384-byte scratch and hvx_circ_copy_pad reads only Z bytes of it, so
           * the padding lanes are never used. HVX is lane-wise, so they do not
           * disturb the valid ones. This replaces a scalar tail that read
           * bn_msg -- VTCM -- a byte at a time, per edge, per iteration. */
          for (int32_t voff = 0; voff < padded; voff += HVX_VLEN)
            *(HVX_UVector *)(tmp + voff) =
                hvx_sat8_sub(*(HVX_UVector *)(llrRes + llrPos + voff), *(HVX_UVector *)(bn_msg + bn_addr + voff), v127, vn128);
          hvx_circ_copy_pad(cdst, tmp, (uint32_t)cs, Z);
        }
      }
    }
  }
}

// =============================================================================
// DSP session context — forward declaration (full definition before open/close)
// =============================================================================
// 2-buffer message-passing design (permutation fused into cnProc/bnProc):
//   cn_msg:    BN→CN extrinsic messages, CN-indexed  (was cnProcBuf)
//   bn_msg:    CN→BN extrinsic messages, BN-indexed  (was bnProcBuf)
//   cnProcBufRes and bnProcBufRes eliminated — scatter is done inline.
/* Everything one concurrent decode needs. Was a single set of session-wide
 * buffers plus file-static scratch, which is exactly what stopped segments being
 * decoded concurrently: two decodes in flight would have shared cn_msg, bn_msg,
 * the CRC bit buffer and the prep staging. One of these per worker instead.
 *
 * The file-static scratch was not laziness -- the FastRPC thread stack is 16 kB
 * and these buffers are tens of kB, so they cannot be locals. Per-worker heap
 * (and VTCM for the HVX-only ones) is the shape that satisfies both. */
#define LDPC_MAX_WORKERS LDPC_HEX_MAX_WORKERS /* shared: see ldpc_hexagon_queue.h */
typedef struct {
  int8_t *cn_msg; // BN->CN extrinsics, CN-indexed (121 KB, VTCM)
  int8_t *bn_msg; // CN->BN extrinsics, BN-indexed (121 KB, VTCM)
  int8_t *llrRes; // posterior LLRs in BN layout   (27 KB, heap: scalar-read)
  int8_t *llrProcBuf; // channel LLRs in BN layout     (27 KB, heap: scalar-read)
  int8_t *llrout; // gather target for the batch path
  uint8_t *crc_bits; // packed hard bits for the per-iteration CRC
  int16_t *prep_e; // deinterleave output
  int16_t *prep_z; // int16 staging before the saturate
  int8_t *prep_out; // saturated decoder input
  int16_t *gv_f; // VTCM gather staging: source
  int16_t *gv_e; // VTCM gather staging: one deinterleaved row
  /* Stage checksums, filled by ldpc_scalar_core and returned to the ARM:
   *   [0] cn_msg after the LLR scatter   -- input staging
   *   [1] bn_msg after the first cnProc  -- check-node pass
   *   [2] cn_msg after the first bnProc  -- bit-node pass
   * Whichever first differs between the batch and per-segment paths names the
   * stage that is wrong for BG2. */
  uint64_t dbg_hash[3];
} ldpc_worker_t;

typedef struct {
  uint32_t dsp_clk_hz;
#ifndef LDPC_SIM_STANDALONE
  dspqueue_t dq; // async packet queue (NULL until worker_start)
#endif
#ifdef LDPC_HEX_VTCM
  void *vtcm_base;
  unsigned int vtcm_ctx;
#endif
} ldpc_dsp_ctx_t;

/* One working set per concurrent decode. g_nworkers is how many are actually in
 * use; 1 reproduces the previous single-threaded behaviour exactly. */
static ldpc_worker_t g_workers[LDPC_MAX_WORKERS];
/* Bytes of VTCM actually granted (0 = none/heap). Reported to the ARM in the batch
 * response, because on this platform no FARF channel reaches the host: printf forwarding
 * is off, FARF(ERROR) does not reach /dev/kmsg, and mini_dm is not available. The response
 * struct is the only diagnostic path that demonstrably works. */
static uint32_t g_vtcm_granted = 0;
static int g_nworkers = 1;

#ifdef LDPC_HEX_VTCM
// VTCM holds every buffer the kernels touch: cn_msg, bn_msg, llrRes, llrProcBuf.
//
// llrRes and llrProcBuf were deliberately kept in heap for a long time, because the gather
// read them with a scalar memcpy and a scalar VTCM read bypasses L1/L2 for ~20 cycles. That
// reason died when the gather, scatter and input copy all became HVX: every access to these
// two is now a vector access, and an HVX read of VTCM is 1 cycle.
//
// EVERY SLICE MUST BE 128-BYTE ALIGNED. NR_LDPC_MAX_NUM_LLR is 27000, which is not a
// multiple of the vector length, so carving the llr buffers out at their natural size
// misaligns everything after them -- including gv_f, whose vgather/vscatter staging
// REQUIRES an aligned VTCM address. That is what broke the first attempt at this: BLER 1.0
// and everything ~2x slower, which looked like a short VTCM grant and was not. The grant
// was never short; reporting it (see g_vtcm_granted) showed 4 MB handed back for a 1.43 MB
// request, because single_page rounds up to a 4 MB page.
#define LDPC_VTCM_ALIGN(n) (((uint32_t)(n) + HVX_VLEN - 1u) & ~((uint32_t)HVX_VLEN - 1u))
#define LDPC_VTCM_LLR_SZ LDPC_VTCM_ALIGN(NR_LDPC_MAX_NUM_LLR + HVX_VLEN)
//
// cn_msg and bn_msg are accessed exclusively via HVX vector loads/stores
// (1-cycle VTCM latency), so they benefit fully from VTCM placement.
/* HVX_VLEN of slack per buffer: the kernels process ceil(Z/128) whole vectors and let the
 * last one run past Z rather than dropping to a scalar tail, so a read can reach up to
 * 127 bytes beyond the last group. Cheaper than bounds-checking the inner loops. */
#define VTCM_MSGS_SIZE                                                                                         \
  (LDPC_VTCM_ALIGN(NR_LDPC_SIZE_CN_PROC_BUF + HVX_VLEN) + LDPC_VTCM_ALIGN(NR_LDPC_SIZE_BN_PROC_BUF + HVX_VLEN) \
   + 2u * LDPC_VTCM_LLR_SZ)
// = 2 x 121,344 = 242,688 B.
//
// Budget, measured on this device: 8 MB total, 7 MB available, largest single
// page 4 MB. Against that: these buffers 237 kB, the gather staging 128 kB, and
// the inner-RX offload ~554 kB when it runs alongside -- about 919 kB of 7 MB.
// Even four-way segment-parallel decoding later is ~2 MB. There is room; the
// constraint to watch is the 4 MB single-page limit, not capacity.
//
// ONE acquisition covers both this and the gather staging, rather than two
// contexts in one PD.
#define VTCM_PER_WORKER (VTCM_MSGS_SIZE + (LDPC_GV_F_HW + LDPC_GV_E_HW) * 2u)
#define VTCM_TOTAL_SIZE (VTCM_PER_WORKER * LDPC_MAX_WORKERS)
#endif

// =============================================================================
// Main decoder core — uses pre-allocated buffers from session context
// =============================================================================
// Returns number of BP iterations completed.

// =============================================================================
// Scalar reference BN functions — used to isolate HVX correctness issues.
// These produce the same results as hvx_bnProcPc / hvx_bnProc but with no
// HVX code, so they run correctly on any Hexagon v73 core.
// =============================================================================

static void scalar_ref_bnProcPc(t_nrLDPC_lut *p_lut,
                                const int8_t *bnProcBuf,
                                int8_t *bnProcBufRes,
                                const int8_t *llrProcBuf,
                                int8_t *llrRes,
                                uint16_t Z)
{
  const uint8_t *numBn = p_lut->numBnInBnGroups;
  const uint32_t *sBn = p_lut->startAddrBnGroups;
  const uint16_t *sLlr = p_lut->startAddrBnGroupsLlr;

  // Group 0: degree-1 BNs
  {
    uint32_t nb = (uint32_t)numBn[0];
    uint32_t total = nb * (uint32_t)Z;
    uint32_t sa = sBn[0];
    uint32_t sl = sLlr[0];
    for (uint32_t b = 0; b < total; b++) {
      bnProcBufRes[sa + b] = llrProcBuf[sl + b]; // extrinsic = channel LLR
      llrRes[sl + b] = sat8_add(llrProcBuf[sl + b], bnProcBuf[sa + b]);
    }
  }

  // Groups 1+
  uint8_t idxBnGroup = 0;
  for (int cnidx = 1; cnidx < NR_LDPC_NUM_BN_GROUPS_BG1_R13; cnidx++) {
    if (numBn[cnidx] == 0)
      continue;
    idxBnGroup++;
    uint32_t nb = (uint32_t)numBn[cnidx];
    uint32_t cnOff = nb * NR_LDPC_ZMAX;
    uint32_t total = nb * (uint32_t)Z;
    uint32_t sa = sBn[idxBnGroup];
    uint32_t sl = sLlr[idxBnGroup];
    for (uint32_t b = 0; b < total; b++) {
      int16_t s = (int16_t)llrProcBuf[sl + b];
      for (int k = 0; k <= cnidx; k++)
        s += (int16_t)bnProcBuf[sa + (uint32_t)k * cnOff + b];
      llrRes[sl + b] = s > 127 ? 127 : (s < -128 ? -128 : (int8_t)s);
    }
  }
}

static void scalar_ref_bnProc(t_nrLDPC_lut *p_lut, const int8_t *bnProcBuf, int8_t *bnProcBufRes, const int8_t *llrRes, uint16_t Z)
{
  const uint8_t *numBn = p_lut->numBnInBnGroups;
  const uint32_t *sBn = p_lut->startAddrBnGroups;
  const uint16_t *sLlr = p_lut->startAddrBnGroupsLlr;

  uint8_t idxBnGroup = 0;
  for (int cnidx = 1; cnidx < NR_LDPC_NUM_BN_GROUPS_BG1_R13; cnidx++) {
    if (numBn[cnidx] == 0)
      continue;
    idxBnGroup++;
    uint32_t nb = (uint32_t)numBn[cnidx];
    uint32_t cnOff = nb * NR_LDPC_ZMAX;
    uint32_t total = nb * (uint32_t)Z;
    uint32_t sa = sBn[idxBnGroup];
    uint32_t sl = sLlr[idxBnGroup];
    for (int k = 0; k <= cnidx; k++) {
      uint32_t off = (uint32_t)k * cnOff;
      for (uint32_t b = 0; b < total; b++)
        bnProcBufRes[sa + off + b] = sat8_sub(llrRes[sl + b], bnProcBuf[sa + off + b]);
    }
  }
}

// =============================================================================
// CRC-24 on the DSP, for CRC-based early termination.
// =============================================================================
// The reference decoder stops as soon as check_crc() passes -- typically at
// iteration 2 -- while this decoder stopped when its PARITY check was satisfied,
// which happens later (4-5 iterations for the same answer). That was the whole
// ~1.8x decode-time gap against the CPU: same bits, same graph, more iterations.
//
// check_crc cannot cross FastRPC (function pointer), so it is reimplemented here.
// OAI has TWO CRC implementations -- the original table-driven one and an
// Intel/PMULL carry-less-multiply one -- and takes the PMULL path whenever the
// bit length is byte-aligned, which ours is (Kprime-24 = 8336). Hexagon has no
// PMULL, so this is the TABLE path. Verified on the host that the two agree
// bit-for-bit at 8336 bits, so terminating on this is the reference's decision.
//
// Polynomials copied from crc_byte.c -- do not retype them from memory.

#define LDPC_POLY24A 0x864cfb00u
#define LDPC_POLY24B 0x80006300u

/* Four tables per polynomial, for slicing-by-4: t[0] is the classic byte table, and
 * t[k][i] is the same value pushed through k more zero bytes. 8 KB total for both polys. */
static uint32_t g_crc24a_tab[4][256];
static uint32_t g_crc24b_tab[4][256];
static int g_crc_tabs_ready = 0;

static uint32_t ldpc_crcbit(uint8_t b, uint32_t poly)
{
  uint32_t crc = 0, c = (uint32_t)b << 24;
  for (int i = 8; i != 0; i--) {
    if ((1u << 31) & (c ^ crc))
      crc = (crc << 1) ^ poly;
    else
      crc <<= 1;
    c <<= 1;
  }
  return crc;
}

static void ldpc_crc_tabs_init(void)
{
  if (g_crc_tabs_ready)
    return;
  for (int c = 0; c < 256; c++) {
    g_crc24a_tab[0][c] = ldpc_crcbit((uint8_t)c, LDPC_POLY24A);
    g_crc24b_tab[0][c] = ldpc_crcbit((uint8_t)c, LDPC_POLY24B);
  }
  /* t[k][i] = t[k-1][i] pushed through one more zero byte, which is exactly one
   * application of the byte step with data byte 0. */
  for (int k = 1; k < 4; k++)
    for (int c = 0; c < 256; c++) {
      uint32_t a = g_crc24a_tab[k - 1][c];
      uint32_t b = g_crc24b_tab[k - 1][c];
      g_crc24a_tab[k][c] = (a << 8) ^ g_crc24a_tab[0][a >> 24];
      g_crc24b_tab[k][c] = (b << 8) ^ g_crc24b_tab[0][b >> 24];
    }
  g_crc_tabs_ready = 1;
}

#ifdef LDPC_SIM_STANDALONE
/* Byte-at-a-time reference, built only into the sim harness. It is the oracle the sliced
 * version is checked against; ldpc_crc24() below must agree with it for every input. */
static uint32_t ldpc_crc24_ref(const uint8_t *in, int bitlen, const uint32_t (*tab)[256])
{
  int octetlen = bitlen / 8;
  const int resbit = bitlen % 8;
  uint32_t crc = 0;
  while (octetlen-- > 0)
    crc = (crc << 8) ^ tab[0][(*in++) ^ (crc >> 24)];
  if (resbit > 0)
    crc = (crc << resbit) ^ tab[0][((*in) >> (8 - resbit)) ^ (crc >> (32 - resbit))];
  return crc;
}
#endif // LDPC_SIM_STANDALONE

/* Slicing-by-4.
 *
 * --packet_analyze on the CRC termination path: ldpc_check_crc24 was 7701 cycles per call
 * against ldpc_llr2bits's 926, entirely scalar (0 of 3246 packets contained HVX) with
 * CU_BUSY at 94% of stalls -- a register interlock, which is what a byte-at-a-time table
 * CRC is: crc feeds the next index, one byte per step, over 1056 bytes, every iteration.
 * The worst packets were all the `immext` carrying the table base.
 *
 * Slicing-by-4 folds four bytes into ONE dependent step: the four table lookups are
 * independent of each other and only their XOR feeds the next iteration, so the chain is a
 * quarter as long. The identity is the standard one for an MSB-first 32-bit register, which
 * this is -- the 24-bit CRC sits in bits [31:8] and the low byte is always zero. Bytes are
 * assembled big-endian explicitly rather than word-loaded, because the register order is
 * defined by the CRC, not by the machine. */
static uint32_t ldpc_crc24(const uint8_t *in, int bitlen, const uint32_t (*tab)[256])
{
  int octetlen = bitlen / 8;
  const int resbit = bitlen % 8;
  uint32_t crc = 0;

  while (octetlen >= 4) {
    const uint32_t w = ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16) | ((uint32_t)in[2] << 8) | (uint32_t)in[3];
    const uint32_t d = crc ^ w;
    crc = tab[3][(d >> 24) & 0xffu] ^ tab[2][(d >> 16) & 0xffu] ^ tab[1][(d >> 8) & 0xffu] ^ tab[0][d & 0xffu];
    in += 4;
    octetlen -= 4;
  }
  while (octetlen-- > 0)
    crc = (crc << 8) ^ tab[0][(*in++) ^ (crc >> 24)];
  if (resbit > 0)
    crc = (crc << resbit) ^ tab[0][((*in) >> (8 - resbit)) ^ (crc >> (32 - resbit))];
  return crc;
}

/* Mirrors check_crc() in crc_byte.c: trailing 24 bits are the transmitted CRC.
 * crc_type 0 = CRC24_A, 1 = CRC24_B (the enum order in coding_defs.h). */
LDPC_KERNEL static int ldpc_check_crc24(const uint8_t *bytes, uint32_t n, uint8_t crc_type)
{
  if (n < 32u)
    return 0;
  uint32_t oldcrc = 0;
  for (int i = 0; i < 3; i++)
    oldcrc |= (uint32_t)bytes[(n >> 3) - 3 + i] << ((2 - i) << 3);
  oldcrc &= 0x00ffffffu;
  const uint32_t crc = ldpc_crc24(bytes, (int)n - 24, crc_type == 0 ? g_crc24a_tab : g_crc24b_tab) >> 8;
  return crc == oldcrc;
}

/* Hard-decide the first nbits posterior LLRs into MSB-first packed bytes,
 * matching arm_llr2bitPacked() on the ARM side. 32 LLRs -> 4 bytes.
 *
 * Two earlier versions were both too slow, and each for its own reason:
 *
 *  1. A bit-at-a-time inner loop: ~8*nbits operations. Over 8448 bits it cost
 *     MORE than the iteration the CRC check saves -- the CRC decode came out
 *     slower than the parity one (18,911 vs 15,474 us/slot).
 *  2. The classic multiply trick (mask the sign bits, multiply by
 *     0x8040201008040201, take the top byte). That is the right answer on a
 *     machine with a 64x64 multiplier; Hexagon has 32x32->64 only, so each one
 *     expands into a cluster of mpyu + adds. At 8 LLRs per multiply the pack
 *     still measured 79 us/codeword, ~22 us per iteration.
 *
 * The scalar byte-vector unit does the whole job instead: vcmpb.gt puts one bit
 * per byte into a predicate, tfrpr moves a predicate into a general register,
 * and brev+swiz convert the LSB-first order those produce into the MSB-first
 * order the CRC needs.
 *
 * NOT vitpack, which looks like the instruction for joining two predicates and
 * is not: it takes the EVEN bits of Ps and the ODD bits of Pt into a single
 * byte, so it concatenates nothing. Using it here packed garbage, every segment
 * ran to numMaxIter because the CRC could never match, and the decode still
 * produced correct output (the ARM-side CRC re-check masked it) -- a silent
 * 25% loss. Verified instruction by instruction under hexagon-sim.
 *
 * The bit order is NOT obvious -- two of the three versions of this function had
 * it wrong -- so the exact sequence below was checked byte-for-byte against the
 * bit-at-a-time reference under hexagon-sim, over lengths spanning the ragged
 * tail. Re-verify there, not on hardware, if you touch it: on hardware a wrong
 * order costs iterations without ever failing a test. */
LDPC_KERNEL static void ldpc_llr2bits(uint8_t *out, const int8_t *llr, uint32_t nbits)
{
  uint32_t k = 0;

  /* Tier 1 -- HVX, 1024 LLRs to 128 bytes. vcmp gives one predicate bit per
   * byte; vand turns that into 0/1 bytes; vrmpy then weights each group of
   * four by {8,4,2,1}, so one word holds one MSB-first nibble. Two pack levels
   * bring 256 words down to 256 nibble-bytes, where halfword g happens to hold
   * nibble 2g in its low byte and 2g+1 in its high byte -- so the output byte
   * is the low byte of (h << 4) | (h >> 8), and a final pack collects them.
   *
   * Unaligned loads (HVX_UVector -> vmemu) on purpose. llr_out is an rpcmem
   * allocation and therefore page-aligned today, but nothing in the interface
   * promises that and the penalty is negligible. */
  {
    const HVX_Vector vzero = Q6_V_vzero();
    const uint32_t ngrp = nbits >> 10;
    for (uint32_t g = 0; g < ngrp; g++, k += 1024) {
      HVX_Vector r[8];
      for (int i = 0; i < 8; i++) {
        const HVX_Vector v = *(const HVX_UVector *)(llr + k + i * 128);
        /* the hard decision is llr < 0, i.e. 0 > llr */
        const HVX_Vector s = Q6_V_vand_QR(Q6_Q_vcmp_gt_VbVb(vzero, v), 0x01010101);
        r[i] = Q6_Vuw_vrmpy_VubRub(s, 0x01020408); /* weights {8,4,2,1} */
      }
      const HVX_Vector n0 = Q6_Vb_vpacke_VhVh(Q6_Vh_vpacke_VwVw(r[3], r[2]), Q6_Vh_vpacke_VwVw(r[1], r[0]));
      const HVX_Vector n1 = Q6_Vb_vpacke_VhVh(Q6_Vh_vpacke_VwVw(r[7], r[6]), Q6_Vh_vpacke_VwVw(r[5], r[4]));
      const HVX_Vector t0 = Q6_V_vor_VV(Q6_Vh_vasl_VhR(n0, 4), Q6_Vuh_vlsr_VuhR(n0, 8));
      const HVX_Vector t1 = Q6_V_vor_VV(Q6_Vh_vasl_VhR(n1, 4), Q6_Vuh_vlsr_VuhR(n1, 8));
      *(HVX_Vector *)(out + (g << 7)) = Q6_Vb_vpacke_VhVh(t1, t0);
    }
  }

  /* Tier 2 -- scalar byte-vector, 32 LLRs to 4 bytes. Worth having as well as
   * tier 1: K' is not a multiple of 1024 (8448 leaves 256), and finishing the
   * remainder one bit at a time costs more than the whole vector part. */
  for (; k + 32u <= nbits; k += 32) {
    uint64_t w0, w1, w2, w3;
    memcpy(&w0, llr + k, sizeof w0); /* little-endian: byte i = LLR k+i */
    memcpy(&w1, llr + k + 8, sizeof w1);
    memcpy(&w2, llr + k + 16, sizeof w2);
    memcpy(&w3, llr + k + 24, sizeof w3);
    const int p0 = Q6_p_vcmpb_gt_PP(0, w0), p1 = Q6_p_vcmpb_gt_PP(0, w1);
    const int p2 = Q6_p_vcmpb_gt_PP(0, w2), p3 = Q6_p_vcmpb_gt_PP(0, w3);
    uint32_t word = (uint32_t)Q6_R_equals_p(p0) | ((uint32_t)Q6_R_equals_p(p1) << 8) | ((uint32_t)Q6_R_equals_p(p2) << 16)
                    | ((uint32_t)Q6_R_equals_p(p3) << 24);
    word = (uint32_t)Q6_R_swiz_R(Q6_R_brev_R((int)word));
    memcpy(out + (k >> 3), &word, sizeof word);
  }

  /* Tier 3 -- fewer than 32 LLRs left, including any ragged tail. Also the
   * reference the other two tiers were verified against. */
  for (; k < nbits; k++) {
    if ((k & 7u) == 0)
      out[k >> 3] = 0;
    if (llr[k] < 0)
      out[k >> 3] |= (uint8_t)(1u << (7 - (k & 7)));
  }
}

/* ---- DSP-side segment prep: deinterleave, rate-dematch, saturate ------------
 *
 * Ported from nr_rate_matching.c so the ARM does not have to do it. On the ARM
 * this was ~1,050 us/slot; more importantly it is the last thing standing between
 * the inner-RX offload and this one, because once prep is here the codeword can
 * be handed over on the DSP and never cross the FastRPC boundary at all.
 *
 * `d` -- the rate-dematching output, which HARQ ACCUMULATES ACROSS ROUNDS -- now
 * lives here, keyed on harq_unique_pid. It never needs ARM access, so it is plain
 * DSP heap rather than rpcmem: no mapping, no transfer, nothing to keep in sync.
 * Same shape as the GPU implementation in develop, which cudaMallocs d and keys
 * it the same way; harq_unique_pid is already in
 * nrLDPC_TB_decoding_parameters_t, so nothing in OAI has to change to allow it.
 */

static const uint8_t irx_index_k0[2][4] = {{0, 17, 33, 56}, {0, 13, 25, 43}};

/* `d` per HARQ process, and WITHIN it one region PER CODE BLOCK -- OAI keeps
 * `tb->d + r * Kc * Z`, C separate accumulators, not one shared buffer. Getting
 * that wrong is invisible in the first round, because prep and decode are
 * interleaved per segment so each segment reads back what it just wrote; it only
 * shows on a RETRANSMISSION, where every segment's history has been overwritten
 * by the segments after it. Measured as HARQ never recovering: n_errors
 * (69/100, 69/69, 69/69, 69/69) against native's (62/100, 0/62, 0/0, 0/0).
 *
 * Sized on first use and grown if a bigger TB arrives. LDPC_HEX_MAX_HARQ covers
 * the 16 processes NR allows per UE; a pid outside the table falls back to slot
 * 0, which loses HARQ history but never corrupts memory. NOTE the footprint:
 * C * N * 2 bytes per process, ~1.5 MB at C=30 BG1 Z=384, so a many-UE DU wants
 * a real eviction policy here rather than a 16-entry table. */
#define LDPC_HEX_MAX_HARQ 16u
typedef struct {
  uint32_t pid;
  uint32_t words; /* int16 entries allocated */
  int16_t *d;
  int valid;
} ldpc_harq_slot_t;
static ldpc_harq_slot_t g_harq[LDPC_HEX_MAX_HARQ];

/* words_needed is the TOTAL for the transport block: C * per-segment stride. */
static int16_t *ldpc_harq_d(uint32_t pid, uint32_t words_needed)
{
  ldpc_harq_slot_t *sl = NULL;
  for (unsigned i = 0; i < LDPC_HEX_MAX_HARQ; i++) {
    if (g_harq[i].valid && g_harq[i].pid == pid) {
      sl = &g_harq[i];
      break;
    }
  }
  if (!sl) {
    for (unsigned i = 0; i < LDPC_HEX_MAX_HARQ; i++)
      if (!g_harq[i].valid) {
        sl = &g_harq[i];
        break;
      }
    if (!sl)
      sl = &g_harq[0]; /* table full: reuse, see note above */
    sl->pid = pid;
    sl->valid = 1;
  }
  if (sl->words < words_needed) {
    free(sl->d);
    sl->d = (int16_t *)malloc((size_t)words_needed * sizeof(int16_t));
    sl->words = sl->d ? words_needed : 0;
    /* A grown buffer holds no usable history, so the caller must clear it;
     * ldpc_batch_prep does that whenever d_to_be_cleared or a resize says so. */
  }
  return sl->d;
}

/* ---- VTCM gather deinterleave -----------------------------------------------
 *
 * The deinterleave is a stride-Qm read, and Qm is 6 for 64QAM. There is no
 * power-of-two shuffle for that: HVX vdeal, x86 unpack and NEON zip/uzp all stop
 * at powers of two, which is why OAI's SIMD interleaver special-cases 2/4/8 and
 * leaves 6 scalar. AVX-512 gets around it with permutex2var_epi8, an arbitrary
 * two-source byte permute; the HVX equivalent for a MULTI-vector gather is
 * vgather, whose source and destination must both be in VTCM.
 *
 * So stage the segment's codeword slice in VTCM and gather from it. Offsets are
 * 16-bit, so they cannot span the whole segment: instead the offset pattern is
 * fixed at 2*i*Qm (max 756 for Qm=6, i < 64) and the BASE advances by 2*64*Qm per
 * block. Qm-agnostic -- 6 costs exactly what 8 does.
 *
 * This needs VTCM, which this skel did not previously acquire at all. Falls back
 * to the scalar path if the acquire fails, so an unsigned-PD device still works.
 */
#ifdef LDPC_HEX_GATHER

/* Vector copy. Never scalar-memcpy VTCM: scalar access bypasses L1/L2 and lands
 * at SRAM latency -- that mistake once cost 66% of a slot in the inner-RX work. */
static inline void ldpc_vcopy(void *dst, const void *src, uint32_t bytes)
{
  uint32_t i = 0;
  for (; i + 128u <= bytes; i += 128u)
    *(HVX_UVector *)((uint8_t *)dst + i) = *(const HVX_UVector *)((const uint8_t *)src + i);
  if (i < bytes)
    memcpy((uint8_t *)dst + i, (const uint8_t *)src + i, bytes - i);
}

/* Returns 1 if it did the work, 0 to fall back to the scalar path. */
static int ldpc_deinterleave_gather(ldpc_worker_t *w, uint32_t E, uint8_t Qm, int16_t *e, const int16_t *f)
{
  const uint32_t n = E / Qm;
  if (E > LDPC_GV_F_HW || n > LDPC_GV_E_HW || Qm == 0)
    return 0;
  if (!w->gv_f || !w->gv_e) /* no VTCM: caller falls back to the scalar path */
    return 0;

  ldpc_vcopy(w->gv_f, f, E * (uint32_t)sizeof(int16_t));

  HVX_Vector voff;
  {
    uint16_t tmp[64];
    for (int i = 0; i < 64; i++)
      tmp[i] = (uint16_t)(2u * (uint32_t)i * Qm);
    memcpy(&voff, tmp, sizeof voff);
  }

  const uint32_t nblk = n / 64u;
  const uint32_t range = 2u * 64u * Qm; /* bytes one block's offsets span */
  for (uint32_t j = 0; j < Qm; j++) {
    for (uint32_t b = 0; b < nblk; b++) {
      const int16_t *base = w->gv_f + (size_t)b * 64u * Qm + j;
      /* the builtin takes the base as an integer address; uintptr_t/Word32
       * are not available in this skel's include set */
      Q6_vgather_ARMVh((HVX_Vector *)(w->gv_e + (size_t)b * 64u), (int)(long)base, (int)range, voff);
    }
    if (nblk)
      ldpc_vcopy(e + (size_t)j * n, w->gv_e, nblk * 64u * (uint32_t)sizeof(int16_t));
    for (uint32_t i = nblk * 64u; i < n; i++) /* < 64 left */
      e[(size_t)j * n + i] = f[(size_t)i * Qm + j];
  }
  return 1;
}
#endif /* LDPC_HEX_GATHER */

/* nr_deinterleaving_ldpc: Qm interleaved -> Qm contiguous runs of E/Qm.
 *
 * Structured the way the ARM reference is -- walk f ONCE, sequentially, with Qm
 * rotating write pointers -- and NOT as "for each j, read f with stride Qm".
 * Both do E loads and E stores, but the strided-read version was measured at
 * roughly 5 cycles per element on this core: a stride-Qm int16 read defeats the
 * prefetcher and every access is a separate cache line's worth of latency, while
 * the sequential version streams. This is the one prep stage that does NOT
 * vectorise cleanly, because Qm = 6 for 64QAM and a 6-way deal is not an HVX
 * primitive (vdeal is power-of-2 only, and vgather needs VTCM). */
static void ldpc_deinterleave(ldpc_worker_t *w, uint32_t E, uint8_t Qm, int16_t *e, const int16_t *f)
{
  const uint32_t n = E / Qm;
#ifdef LDPC_HEX_GATHER
  if (ldpc_deinterleave_gather(w, E, Qm, e, f))
    return;
#endif
  switch (Qm) {
    case 2: {
      int16_t *p0 = e, *p1 = e + n;
      for (uint32_t i = 0; i < n; i++) {
        *p0++ = *f++;
        *p1++ = *f++;
      }
    } break;
    case 4: {
      int16_t *p0 = e, *p1 = p0 + n, *p2 = p1 + n, *p3 = p2 + n;
      for (uint32_t i = 0; i < n; i++) {
        *p0++ = *f++;
        *p1++ = *f++;
        *p2++ = *f++;
        *p3++ = *f++;
      }
    } break;
    case 6: {
      int16_t *p0 = e, *p1 = p0 + n, *p2 = p1 + n, *p3 = p2 + n, *p4 = p3 + n, *p5 = p4 + n;
      for (uint32_t i = 0; i < n; i++) {
        *p0++ = *f++;
        *p1++ = *f++;
        *p2++ = *f++;
        *p3++ = *f++;
        *p4++ = *f++;
        *p5++ = *f++;
      }
    } break;
    case 8: {
      int16_t *p0 = e, *p1 = p0 + n, *p2 = p1 + n, *p3 = p2 + n, *p4 = p3 + n, *p5 = p4 + n, *p6 = p5 + n, *p7 = p6 + n;
      for (uint32_t i = 0; i < n; i++) {
        *p0++ = *f++;
        *p1++ = *f++;
        *p2++ = *f++;
        *p3++ = *f++;
        *p4++ = *f++;
        *p5++ = *f++;
        *p6++ = *f++;
        *p7++ = *f++;
      }
    } break;
    default:
      for (uint32_t j = 0; j < Qm; j++) {
        int16_t *dst = e + (size_t)j * n;
        for (uint32_t i = 0; i < n; i++)
          dst[i] = f[(size_t)i * Qm + j];
      }
      break;
  }
}

/* d[0..n) += si[0..n), int16, WRAPPING on overflow exactly as the reference's
 * `d[ind] += soft_input[k]` does -- Q6_Vh_vadd_VhVh is the non-saturating add
 * (verified: 32752 + 32 gives -32752). Unaligned on both sides: the runs start
 * wherever the RV offset and the filler boundaries put them. */
static inline void ldpc_rm_acc(int16_t *d, const int16_t *si, uint32_t n)
{
  uint32_t i = 0;
  for (; i + 64u <= n; i += 64u) {
    const HVX_Vector vd = *(const HVX_UVector *)(d + i);
    const HVX_Vector vs = *(const HVX_UVector *)(si + i);
    *(HVX_UVector *)(d + i) = Q6_Vh_vadd_VhVh(vd, vs);
  }
  for (; i < n; i++)
    d[i] += si[i];
}

/* int16 -> int8 saturating pack. Q6_Vb_vpack_VhVh_sat takes the LOW half from its
 * SECOND operand (verified under hexagon-sim; guessing this wrong is the same
 * class of bug that cost a day on the CRC bit-packer). */
static void ldpc_prep_saturate(int8_t *out, const int16_t *in, uint32_t n)
{
  uint32_t i = 0;
  for (; i + 128u <= n; i += 128u) {
    const HVX_Vector lo = *(const HVX_UVector *)(in + i);
    const HVX_Vector hi = *(const HVX_UVector *)(in + i + 64u);
    *(HVX_UVector *)(out + i) = Q6_Vb_vpack_VhVh_sat(hi, lo);
  }
  for (; i < n; i++) {
    const int16_t v = in[i];
    out[i] = (int8_t)(v > 127 ? 127 : (v < -128 ? -128 : v));
  }
}

/* nr_rate_matching_ldpc_rx, same bit selection and the same ACCUMULATION into d
 * (+=, not =) that makes HARQ combining work. Returns 0 on success. */
static int ldpc_rate_dematch(uint32_t Tbslbrm,
                             uint8_t BG,
                             uint16_t Z,
                             int16_t *d,
                             const int16_t *soft_input,
                             uint8_t C,
                             uint8_t rvidx,
                             uint8_t clear,
                             uint32_t E,
                             uint32_t F,
                             uint32_t Foffset)
{
  if (C == 0)
    return -1;
  const uint32_t N = (BG == 1) ? (66u * Z) : (50u * Z);
  uint32_t Ncb = N;
  if (Tbslbrm != 0) {
    const uint32_t Nref = (3u * Tbslbrm / (2u * C)); /* R_LBRM = 2/3 */
    Ncb = (N < Nref) ? N : Nref;
  }
  uint32_t ind = ((uint32_t)irx_index_k0[BG - 1][rvidx] * Ncb / N) * Z;
  if (Foffset > E || Foffset > Ncb)
    return -1;

  /* Clear and accumulate the whole circular buffer, not only the part this
   * round's code rate reads: the decoder's rate index comes from the number of
   * LLRs received so far, so a later HARQ round can read further into d than
   * round 0 did, and it must find that part cleared and accumulated. */
  if (clear)
    memset(d, 0, (size_t)Ncb * sizeof(int16_t));

    /* Whole runs at a time. */
#define LDPC_RM_RUN(from, to)                  \
  do {                                         \
    const uint32_t _a = (from), _b = (to);     \
    if (_b > _a && k < E) {                    \
      uint32_t _n = _b - _a;                   \
      if (_n > E - k)                          \
        _n = E - k;                            \
      ldpc_rm_acc(d + _a, soft_input + k, _n); \
      k += _n;                                 \
      ind = _a + _n;                           \
    } else {                                   \
      ind = _b;                                \
    }                                          \
  } while (0)

  uint32_t k = 0;
  if (ind < Foffset)
    LDPC_RM_RUN(ind, Foffset);
  if (ind >= Foffset && ind < Foffset + F)
    ind = Foffset + F;
  LDPC_RM_RUN(ind, Ncb);
  while (k < E) {
    const uint32_t k_before = k;
    LDPC_RM_RUN(0, Foffset);
    LDPC_RM_RUN(Foffset + F, Ncb);
    if (k == k_before)
      break; /* no progress possible; refuse to spin */
  }
#undef LDPC_RM_RUN
  return 0;
}

/* Build one code block's int8 decoder input from the TB's codeword LLRs. */
static int ldpc_batch_prep(ldpc_worker_t *w,
                           const ldpc_q_batch_req_t *br,
                           const int16_t *cw_llr,
                           uint16_t r,
                           int16_t *d,
                           int8_t *out,
                           uint32_t numLLR)
{
  const uint32_t E = (r < br->first_rE2) ? br->E : br->E2;
  const uint32_t llr_off =
      (r < br->first_rE2) ? (uint32_t)r * br->E : (uint32_t)br->first_rE2 * br->E + ((uint32_t)r - br->first_rE2) * br->E2;
  if (E > NR_LDPC_MAX_NUM_LLR)
    return -1;

  /* Defensive: prep_e holds NR_LDPC_MAX_NUM_LLR int16 and ldpc_deinterleave
   * writes E of them. The ARM will not ask when E does not fit, but refuse
   * rather than overrun into the neighbouring prep buffers if it ever does. */
  if (E > NR_LDPC_MAX_NUM_LLR)
    return -1;
  ldpc_deinterleave(w, E, br->Qm, w->prep_e, cw_llr + llr_off);

  const uint32_t Kprime_z = (uint32_t)br->K - br->F; /* the z layout's K', not the CRC one */
  const uint32_t Kc_Z = numLLR; /* what the R-specific graph covers */

  /* The decoder reads d[0 .. Kc_Z - 2Z) and nothing beyond it, so the
   * saturate stops at Kc_Z: at BG1 that is 35Z instead of 68Z at R=2/3. The
   * dematch cannot stop there (see ldpc_rate_dematch). */
  if (ldpc_rate_dematch(br->tbslbrm,
                        br->BG,
                        br->Z,
                        d,
                        w->prep_e,
                        (uint8_t)br->C,
                        br->rv_index,
                        br->d_clear,
                        E,
                        br->F,
                        Kprime_z - 2u * br->Z)
      != 0)
    return -1;

  memset(w->prep_z, 0, 2u * br->Z * sizeof(int16_t));
  memset(w->prep_z + Kprime_z, 127, (size_t)br->F * sizeof(int16_t));
  memcpy(w->prep_z + 2u * br->Z, d, (Kprime_z - 2u * br->Z) * sizeof(int16_t));
  memcpy(w->prep_z + br->K, d + (br->K - 2u * br->Z), (Kc_Z - br->K) * sizeof(int16_t));

  /* int16 -> int8 saturating, over Kc_Z only -- the reference packed the full
   * 68Z regardless of rate, which at R=2/3 is nearly twice the work. */
  ldpc_prep_saturate(out, w->prep_z, Kc_Z);
  return 0;
}

/* First iteration on which the CRC is tested. The test is not free (gather +
 * pack + CRC), and an iteration that could never have converged is a test spent
 * for nothing. At 273 PRB / MCS 20 / 2 layers / SNR 13.2 the earliest
 * termination observed over 1530 codewords is iteration 3, so testing from 2
 * costs nothing measurable and skips one test per codeword.
 *
 * Raising this trades a cheaper decode for the risk of one wasted iteration on
 * a codeword that would have converged sooner -- which is exactly what happens
 * at high SNR or low code rate, where convergence is fast. 2 is the conservative
 * choice; do not raise it without re-measuring the iteration histogram at the
 * operating point you care about. */
#define LDPC_CRC_FIRST_ITER 2

static int32_t ldpc_scalar_core(const int8_t *p_llr,
                                uint8_t *llr_out,
                                uint32_t numLLR,
                                t_nrLDPC_lut *p_lut,
                                uint8_t BG,
                                uint16_t Z,
                                uint8_t numMaxIter,
                                ldpc_worker_t *w,
                                int skip_gather,
                                uint16_t Kprime,
                                uint8_t crc_type,
                                uint32_t *phase_us,
                                const ldpc_addr_cache_t *ac)
{
  int8_t *cn_msg = w->cn_msg;
  int8_t *bn_msg = w->bn_msg;
  int8_t *llrRes = w->llrRes;
  int8_t *llrProcBuf = w->llrProcBuf;
  int32_t numIter = 0;
  uint64_t _t;

  memset(cn_msg, 0, NR_LDPC_SIZE_CN_PROC_BUF);
  memset(bn_msg, 0, NR_LDPC_SIZE_BN_PROC_BUF);
  // nrLDPC_llr2llrProcBuf does not write every position (the 4 higher-degree
  // parity BNs in BG1 whose LLRs it skips).  The reference decoder zero-
  // initialises llrProcBuf on the stack; replicate that here.
  memset(llrProcBuf, 0, NR_LDPC_MAX_NUM_LLR);

  _t = HAP_perf_get_time_us();
  hvx_memcpy(llrRes, p_llr, numLLR);
  phase_us[LDPC_PHASE_LLRCOPY] += (uint32_t)(HAP_perf_get_time_us() - _t);

  _t = HAP_perf_get_time_us();
  hvx_llr2llrProcBuf(p_lut, llrRes, llrProcBuf, Z, BG);
  phase_us[LDPC_PHASE_LLR2PROC] += (uint32_t)(HAP_perf_get_time_us() - _t);

  _t = HAP_perf_get_time_us();
  if (BG == 1)
    hvx_llr2CnProcBuf_BG1(p_lut, llrRes, cn_msg, Z);
  else
    hvx_llr2CnProcBuf_BG2(p_lut, llrRes, cn_msg, Z);
  phase_us[LDPC_PHASE_SCATTER] += (uint32_t)(HAP_perf_get_time_us() - _t);
#ifdef LDPC_HEX_DBG_HASH
  w->dbg_hash[0] = ldpc_fnv64(cn_msg, NR_LDPC_SIZE_CN_PROC_BUF);
#endif

  // First iteration without parity check
  _t = HAP_perf_get_time_us();
  cnProc_fused(cn_msg, bn_msg, p_lut, Z, BG);
  phase_us[LDPC_PHASE_CNPROC] += (uint32_t)(HAP_perf_get_time_us() - _t);
#ifdef LDPC_HEX_DBG_HASH
  w->dbg_hash[1] = ldpc_fnv64(bn_msg, NR_LDPC_SIZE_BN_PROC_BUF);
#endif

  _t = HAP_perf_get_time_us();
  hvx_bnProcPc(p_lut, bn_msg, llrProcBuf, llrRes, Z);
  phase_us[LDPC_PHASE_BNPROCPC] += (uint32_t)(HAP_perf_get_time_us() - _t);

  _t = HAP_perf_get_time_us();
  bnProc_fused(bn_msg, llrRes, cn_msg, p_lut, Z, BG, ac);
  phase_us[LDPC_PHASE_BNPROC] += (uint32_t)(HAP_perf_get_time_us() - _t);
#ifdef LDPC_HEX_DBG_HASH
  w->dbg_hash[2] = ldpc_fnv64(cn_msg, NR_LDPC_SIZE_CN_PROC_BUF);
#endif

  // BP iteration loop with parity check.
  // cnProcPc runs immediately after bnProcPc so llrRes is still hot in L1.
  // bnProc_fused is skipped when converged or on the last iteration (cn_msg
  // is not needed again in either case), saving one unnecessary scatter pass.
  /* Kprime > 0 means the caller gave us a CRC to check, so terminate on it --
   * the reference's criterion. The PARITY check is then not computed at all:
   * it is only a termination test, BP does not need it, and it costs 2-9% of
   * the decode. With no CRC available (the legacy ldpctest path) fall back to
   * parity exactly as before. */
  /* Only CRC24_A (0) and CRC24_B (1) are implemented here -- ldpc_check_crc24()
   * treats any non-zero crc_type as CRC24_B. OAI's crcType() returns CRC16 (2)
   * for every single-segment transport block under NR_MAX_PDSCH_TBS, which is
   * every Msg3. Checking 24 bits of a CRC16-protected block never matches, so
   * the decoder burned all numMaxIter iterations and returned numMaxIter --
   * and OAI's success test is `numIter < numMaxIter`, so a PERFECTLY DECODED
   * codeword was discarded. Random access could therefore never complete with
   * the offload on, while BG1 data traffic (CRC24) was unaffected.
   *
   * Fall back to the parity check for CRC types we do not implement: it is the
   * pre-CRC behaviour, it is correct, and the iteration saving it gives up is
   * negligible on blocks this small. Implementing CRC16 here would restore the
   * early exit and is the better long-term fix. */
  const int crc_supported = (crc_type == 0u) || (crc_type == 1u);
  const int use_crc = (Kprime > 0u) && (Kprime <= numLLR) && crc_supported;
  if (use_crc)
    ldpc_crc_tabs_init();

  int32_t pcRes = 1;
  int done = 0;
  int crc_gathered = 0;
  while (numIter < (int32_t)numMaxIter && !done) {
    _t = HAP_perf_get_time_us();
    cnProc_fused(cn_msg, bn_msg, p_lut, Z, BG);
    phase_us[LDPC_PHASE_CNPROC] += (uint32_t)(HAP_perf_get_time_us() - _t);

    _t = HAP_perf_get_time_us();
    hvx_bnProcPc(p_lut, bn_msg, llrProcBuf, llrRes, Z);
    phase_us[LDPC_PHASE_BNPROCPC] += (uint32_t)(HAP_perf_get_time_us() - _t);

    numIter++;

    if (use_crc && numIter >= LDPC_CRC_FIRST_ITER) {
      /* Gather + hard-decide + CRC, in the same place the reference does
       * it. The gather is the cost of the test (~3% per iteration); it pays
       * for itself the moment it saves one iteration. */
      _t = HAP_perf_get_time_us();
      if (skip_gather)
        hvx_memcpy((int8_t *)llr_out, llrRes, numLLR);
      else
        hvx_llrRes2llrOut(p_lut, (int8_t *)llr_out, llrRes, Z, BG);
      phase_us[LDPC_PHASE_GATHER] += (uint32_t)(HAP_perf_get_time_us() - _t);

      _t = HAP_perf_get_time_us();
      ldpc_llr2bits(w->crc_bits, (const int8_t *)llr_out, Kprime);
      done = ldpc_check_crc24(w->crc_bits, Kprime, crc_type);
      phase_us[LDPC_PHASE_TERMTEST] += (uint32_t)(HAP_perf_get_time_us() - _t);
      pcRes = done ? 0 : 1;
      crc_gathered = 1;
    } else if (use_crc) {
      /* Below LDPC_CRC_FIRST_ITER: no termination test at all. */
      pcRes = 1;
    } else {
      // cnProcPc immediately after bnProcPc — llrRes hot in L1.
      _t = HAP_perf_get_time_us();
      pcRes = cnProcPc_fused(llrRes, p_lut, Z, BG, ac);
      phase_us[LDPC_PHASE_TERMTEST] += (uint32_t)(HAP_perf_get_time_us() - _t);
      done = (pcRes == 0);
    }

    // Skip bnProc_fused when converged or on the last iteration: cn_msg is
    // not consumed again, and llrRes stays warm for gather.
    if (!done && numIter < (int32_t)numMaxIter) {
      _t = HAP_perf_get_time_us();
      bnProc_fused(bn_msg, llrRes, cn_msg, p_lut, Z, BG, ac);
      phase_us[LDPC_PHASE_BNPROC] += (uint32_t)(HAP_perf_get_time_us() - _t);
    }
  }
  /* When the CRC test ran, the gather it needed already produced llr_out for
   * the final iteration; skip the trailing one so it is not paid twice. Not
   * simply `use_crc`: with numMaxIter < LDPC_CRC_FIRST_ITER the loop never
   * reaches the test, and llr_out would be left unwritten. */
  const int gather_done = crc_gathered;

  if (!gather_done) {
    _t = HAP_perf_get_time_us();
    if (skip_gather)
      hvx_memcpy((int8_t *)llr_out, llrRes, numLLR);
    else
      hvx_llrRes2llrOut(p_lut, (int8_t *)llr_out, llrRes, Z, BG);
    phase_us[LDPC_PHASE_GATHER] += (uint32_t)(HAP_perf_get_time_us() - _t);
  }

  return numIter;
}

/* ---- segment-parallel decoding across HVX contexts -------------------------
 *
 * The decode is 309 us/codeword of DSP compute and ~30 code blocks per transport
 * block, so it is ~9 ms/slot of work that is entirely independent per block --
 * separate LUTs are read-only, and every mutable buffer is now per worker. What
 * previously prevented it was not the data but the DISPATCH: one FastRPC packet
 * per code block meant the ARM waited between blocks, so the DSP never had more
 * than one in flight. Batching removed that, and this uses it.
 *
 * Work is pulled, not pushed: each thread takes the next unclaimed block with an
 * atomic increment. Code blocks do not all take the same number of iterations --
 * the CRC terminates early and by different amounts -- so a static split would
 * leave threads idle waiting for the unlucky one. */
typedef struct {
  const ldpc_q_batch_req_t *br;
  const int8_t *llr_base;
  uint8_t *out_base;
  uint8_t *meta;
  uint32_t out_bytes;
  volatile int next_seg; /* claimed with __sync_fetch_and_add */
  volatile int done_count;
  volatile int claims[LDPC_MAX_WORKERS];
} ldpc_batch_job_t;

static ldpc_batch_job_t g_job;

/* Decode one code block. Everything mutable is in `w`, everything shared is
 * read-only (the request, the LUTs, the input buffer), which is what makes this
 * safe to run on several HVX contexts at once. */
static void ldpc_decode_one(ldpc_worker_t *w, int r_in, uint32_t *phase_us)
{
  const ldpc_q_batch_req_t *br = g_job.br;
  const uint16_t r = (uint16_t)r_in;
  const uint32_t out_bytes = ((uint32_t)br->K + 7u) >> 3;
  /* 38.212 5.4.2.1: the last C - first_rE2 code blocks carry one
   * extra Qm*Nl bits, so they can land on a different rate index
   * and therefore a different numLLR. Derive both here rather
   * than making the ARM send a per-segment table. */
  t_nrLDPC_dec_params dp;
  memset(&dp, 0, sizeof dp);
  dp.BG = br->BG;
  dp.Z = br->Z;
  dp.numMaxIter = br->numMaxIter;
  dp.R = (r < br->first_rE2) ? br->R : br->R2;

  t_nrLDPC_lut lut;
  memset(&lut, 0, sizeof lut);
  const uint32_t numLLR = nrLDPC_init(&dp, &lut);
  int32_t it = (int32_t)br->numMaxIter;
  const int8_t *seg_in = NULL;
  if (numLLR) {
    if (br->prep_on_dsp) {
      /* bufs[0] is the TB codeword (int16); prep it here.
       * One `d` region per code block, stride N. */
      const uint32_t dstride = (br->BG == 1) ? (66u * br->Z) : (50u * br->Z);
      int16_t *dbase = ldpc_harq_d(br->harq_pid, (uint32_t)br->C * dstride);
      if (dbase
          && ldpc_batch_prep(w, br, (const int16_t *)g_job.llr_base, r, dbase + (size_t)r * dstride, w->prep_out, numLLR) == 0)
        seg_in = w->prep_out;
    } else if (numLLR <= br->llr_stride) {
      seg_in = g_job.llr_base + (size_t)r * br->llr_stride;
    }
  }
  if (seg_in) {
    const ldpc_addr_cache_t *ac = ldpc_addr_cache_find(br->BG, br->Z, dp.R);
    it = ldpc_scalar_core(seg_in,
                          (uint8_t *)w->llrout,
                          numLLR,
                          &lut,
                          br->BG,
                          br->Z,
                          br->numMaxIter,
                          w,
                          0,
                          br->Kprime,
                          br->crc_type,
                          phase_us,
                          ac);
    /* Hard-decide K bits straight into the caller's slot. */
    uint8_t *op = g_job.out_base + (size_t)r * br->out_stride;
    ldpc_llr2bits(op, w->llrout, br->K);

    /* The iteration count ALONE is not a success signal when
     * terminating on the CRC: a codeword whose CRC first
     * passes on the last iteration returns numMaxIter, which
     * is indistinguishable from never converging, and OAI
     * then discards a CORRECT codeword. That cost a BLER of
     * 0.167 against the CPU's 0.000 while the decoded bits
     * were byte-identical to the per-segment path -- 0 of 30
     * segments differed. So decide it here, where the packed
     * bits already are, and report a count OAI can read:
     * below numMaxIter iff the CRC actually passes. */
    /* Only CRC24_A/B are implemented; crcType() gives CRC16 (2)
     * for every single-segment TB under NR_MAX_PDSCH_TBS, i.e.
     * every Msg3. Running that through ldpc_check_crc24() can
     * never pass, and the else-branch below then FORCES
     * it = numMaxIter, marking a correctly decoded codeword as
     * failed. Skip the verdict for CRC types we cannot check and
     * keep the decoder's own convergence result. */
    if (br->Kprime > 0u && (br->crc_type == 0u || br->crc_type == 1u)) {
      ldpc_crc_tabs_init();
      const int ok = ldpc_check_crc24(op, br->Kprime, br->crc_type);
      if (ok) {
        if (it >= (int32_t)br->numMaxIter)
          it = (int32_t)br->numMaxIter - 1;
      } else {
        it = (int32_t)br->numMaxIter;
      }
    }
  } else {
    memset(g_job.out_base + (size_t)r * br->out_stride, 0, out_bytes);
  }
  g_job.meta[r] = (uint8_t)(it > 255 ? 255 : it);
}

/* Everything below is the QuRT worker pool and the FastRPC entry points. The
 * standalone simulator test (../test) stops here: it runs ldpc_decode_one() and
 * everything above it, single-threaded, without dspqueue.h or qurt.h. */
#ifndef LDPC_SIM_STANDALONE

static volatile uint32_t g_phase_accum[LDPC_NUM_PHASES];
static qurt_sem_t g_go[LDPC_MAX_WORKERS];
static qurt_sem_t g_done;
static qurt_thread_t g_tid[LDPC_MAX_WORKERS];
static void *g_stack[LDPC_MAX_WORKERS];
static int g_pool_started;
#define LDPC_WORKER_STACK 32768u

static void ldpc_pool_body(void *arg)
{
  const int wi = (int)(long)arg;
  ldpc_worker_t *w = &g_workers[wi];
  /* Lock an HVX context up front. On v65+ the OS context-switches HVX anyway,
   * so this is about avoiding the switch cost, not correctness. */
  const int lk = qurt_hvx_lock(QURT_HVX_MODE_128B);
  if (lk != 0)
    FARF(ALWAYS, "ldpc_hexagon: worker %d could not lock HVX (%d)", wi, lk);
  for (;;) {
    qurt_sem_down(&g_go[wi]);
    if (!g_job.br) /* shutdown */
      break;
    uint32_t local_phase[LDPC_NUM_PHASES] = {0};
    /* Segment range for THIS device. Indices stay absolute so both CDSPs address
     * the shared buffers identically; only the range differs. seg_count == 0 is a
     * caller that does not split, i.e. the whole TB. */
    const int seg_first = (int)g_job.br->seg_first;
    const int seg_end = seg_first + (g_job.br->seg_count ? (int)g_job.br->seg_count : (int)g_job.br->C);
    for (;;) {
      const int r = seg_first + __sync_fetch_and_add((int *)&g_job.next_seg, 1);
      if (r >= seg_end || r >= (int)g_job.br->C)
        break;
      ldpc_decode_one(w, r, local_phase);
      g_job.claims[wi]++;
    }
    /* phase totals are diagnostics; accumulate under the same atomic */
    for (int i = 0; i < LDPC_NUM_PHASES; i++)
      __sync_fetch_and_add((int *)&g_phase_accum[i], (int)local_phase[i]);
    __sync_fetch_and_add((int *)&g_job.done_count, 1);
    qurt_sem_up(&g_done);
  }
  qurt_hvx_unlock();
}

static int ldpc_pool_start(int n)
{
  if (g_pool_started)
    return g_nworkers;
  qurt_sem_init_val(&g_done, 0);
  int started = 0;
  for (int wi = 0; wi < n && wi < LDPC_MAX_WORKERS; wi++) {
    qurt_sem_init_val(&g_go[wi], 0);
    g_stack[wi] = malloc(LDPC_WORKER_STACK);
    if (!g_stack[wi])
      break;
    qurt_thread_attr_t attr;
    qurt_thread_attr_init(&attr);
    qurt_thread_attr_set_name(&attr, "ldpc_seg");
    qurt_thread_attr_set_stack_addr(&attr, g_stack[wi]);
    qurt_thread_attr_set_stack_size(&attr, LDPC_WORKER_STACK);
    const int inherited = qurt_thread_get_priority(qurt_thread_get_id());
    const unsigned short prio = (inherited > 0 && inherited < 256) ? (unsigned short)inherited : 128;
    qurt_thread_attr_set_priority(&attr, prio);
    if (qurt_thread_create(&g_tid[wi], &attr, ldpc_pool_body, (void *)(long)wi) != QURT_EOK) {
      free(g_stack[wi]);
      g_stack[wi] = NULL;
      break;
    }
    started++;
  }
  g_pool_started = 1;
  g_nworkers = started > 0 ? started : 1;
  FARF(ALWAYS, "ldpc_hexagon: segment pool %d thread(s), %d HVX units reported", started, qurt_hvx_get_units());
  return g_nworkers;
}

// =============================================================================
// FastRPC session lifecycle and RPC entry point
// =============================================================================

// Increment this counter any time you need to confirm a fresh skel is loaded.
// Returned in every dspqueue response so the ARM can print it without FARF/logcat.
#define LDPC_SKEL_VERSION 7 /* v7: BG2 write-back table + CN group count in bnProc_fused/addr cache */

// =============================================================================
// dspqueue spinning worker — tight DSP polling loop, zero-interrupt decode path.
// =============================================================================
// ldpc_hexagon_worker_start is a BLOCKING FastRPC call: DSP imports the queue,
// then spins in dspqueue_read_noblock() until it receives LDPC_QMSG_STOP.
// While spinning, the DSP is "awake" — ARM can write packets via
// dspqueue_write_noblock() with zero syscalls (no doorbell fired).
// ARM polls the response queue with dspqueue_read_noblock() — no futex wait.
// Combined overhead: ~2–10 µs (cache flush + ring-buffer round-trip) vs ~305 µs
// for synchronous FastRPC.

AEEResult ldpc_hexagon_worker_start(remote_handle64 handle, uint64 queue_id)
{
  ldpc_dsp_ctx_t *ctx = (ldpc_dsp_ctx_t *)(uintptr_t)handle;
  if (!ctx)
    return AEE_EBADPARM;
  if (ctx->dq)
    return AEE_EITEMBUSY;

  int err = dspqueue_import(queue_id, NULL, NULL, NULL, &ctx->dq);
  if (err) {
    FARF(ERROR, "ldpc_hexagon: dspqueue_import failed 0x%08x", (unsigned)err);
    return err;
  }
  FARF(ALWAYS, "ldpc_hexagon: dspqueue worker spinning (queue_id=%llu)", (unsigned long long)queue_id);

  // Tight spinning loop — never sleeps until STOP packet received.
  // dspqueue_read_noblock() returning AEE_EWOULDBLOCK means queue is empty;
  // we loop back immediately (no QuRT yield / no sleep).
  struct dspqueue_buffer bufs[3];
  /* Either request type arrives on this queue; msg_type is first in both. */
  union {
    ldpc_q_req_t d;
    ldpc_q_batch_req_t b;
    uint8_t raw[64];
  } msg;
  ldpc_q_req_t req;
  uint32_t flags, msg_len, num_bufs;

#define LDPC_IDLE_TIMEOUT_US (180u * 1000u * 1000u) /* 3 min: a live DU is never this quiet */
#define LDPC_IDLE_SPIN_CHECK 200000u
  /* ---- idle watchdog ----------------------------------------------------------------
   *
   * The worker SPINS on dspqueue_read_noblock and, without this, leaves the loop only on an
   * explicit STOP packet. So any ARM-side death that skips STOP -- a timeout kill, a crash,
   * Ctrl-C -- leaves this thread spinning forever inside the blocking worker_start FastRPC
   * call. The PD cannot then be reclaimed, because a thread never returns from the call, and
   * each abandoned run leaks one. Enough of them and remote_handle64_open starts failing with
   * -1 for every client, which reads as "the DSP is broken" and is only cleared by restarting
   * the subsystem -- or, if that wedges, rebooting the board.
   *
   * Bounding the idle spin lets the worker return so the PD is reclaimed. The timeout is
   * deliberately long: a DU sees an UL slot every 2.5 ms, so minutes of silence means the
   * peer is gone, not idle. Cost on the hot path is one increment and a compare -- the clock
   * is only read once every LDPC_IDLE_SPIN_CHECK spins, because HAP_perf_get_time_us() in the spin would add
   * latency to exactly the path the spinning exists to keep short. */
  uint64_t wd_last = HAP_perf_get_time_us();
  uint32_t wd_spins = 0;
  while (1) {
    err = dspqueue_read_noblock(ctx->dq, &flags, 3, &num_bufs, bufs, sizeof(msg), &msg_len, (uint8_t *)&msg);
    if (err == AEE_EWOULDBLOCK) {
      if (++wd_spins >= LDPC_IDLE_SPIN_CHECK) {
        wd_spins = 0;
        if (HAP_perf_get_time_us() - wd_last > LDPC_IDLE_TIMEOUT_US) {
          FARF(ALWAYS, "ldpc_hexagon: idle watchdog fired, releasing the PD");
          break;
        }
      }
      continue;
    }
    wd_spins = 0;
    wd_last = HAP_perf_get_time_us();
    if (err != 0) {
      FARF(ERROR, "ldpc_hexagon: dspqueue_read_noblock err 0x%08x", (unsigned)err);
      break;
    }
    if (msg_len < 4)
      continue;

    if (msg.d.msg_type == LDPC_QMSG_STOP)
      break;

    /* ---- TB-level batch: every code block in one dispatch ----
     * The ARM sleeps once instead of polling through the whole decode, which
     * is what lets the offload actually free an ARM core. Segments are still
     * decoded serially here; the point of the batch is that they no longer
     * have to be, since the ARM is not waiting between them. */
    if (msg.d.msg_type == LDPC_QMSG_BATCH) {
      if (msg_len < sizeof(ldpc_q_batch_req_t) || num_bufs < 3) {
        FARF(ERROR, "ldpc_hexagon: short batch packet len=%u bufs=%u", (unsigned)msg_len, (unsigned)num_bufs);
        continue;
      }
      const ldpc_q_batch_req_t *br = &msg.b;
      ldpc_worker_t *w = &g_workers[0];
      const int8_t *llr_base = (const int8_t *)bufs[0].ptr;
      uint8_t *out_base = (uint8_t *)bufs[1].ptr;
      uint8_t *meta = (uint8_t *)bufs[2].ptr;
      uint32_t phase_us[LDPC_NUM_PHASES] = {0};
      const uint32_t out_bytes = ((uint32_t)br->K + 7u) >> 3;
      uint32_t done = 0, status = 0;

      /* Precompute bnProc's address tables for both rates this TB can use, HERE --
       * on the dispatching thread, before any worker is released. Building them
       * inside the kernel would have four workers racing to fill the same arrays. */
      {
        t_nrLDPC_dec_params dp0;
        t_nrLDPC_lut lut0;
        const uint8_t rates[2] = {br->R, br->R2};
        for (int v = 0; v < LDPC_ADDR_CACHE_N; v++) {
          g_addr_cache[v].valid = 0;
          if (v == 1 && br->R2 == br->R)
            break;
          memset(&dp0, 0, sizeof dp0);
          dp0.BG = br->BG;
          dp0.Z = br->Z;
          dp0.numMaxIter = br->numMaxIter;
          dp0.R = rates[v];
          memset(&lut0, 0, sizeof lut0);
          if (nrLDPC_init(&dp0, &lut0))
            ldpc_addr_cache_build(&g_addr_cache[v], &lut0, br->BG, br->Z, rates[v]);
        }
      }
      const int nw = ldpc_pool_start(br->nworkers ? br->nworkers : 1);
      const uint64_t batch_t0 = HAP_perf_get_time_us();
      if (br->C == 0 || out_bytes > br->out_stride) {
        status = 1;
      } else {
        if (nw > 1) {
          /* fan out: workers claim blocks with an atomic increment */
          g_job.br = br;
          g_job.llr_base = llr_base;
          g_job.out_base = out_base;
          g_job.meta = meta;
          g_job.out_bytes = out_bytes;
          g_job.next_seg = 0;
          g_job.done_count = 0;
          memset((void *)g_job.claims, 0, sizeof g_job.claims);
          memset((void *)g_phase_accum, 0, sizeof g_phase_accum);
          for (int wi = 0; wi < nw; wi++)
            qurt_sem_up(&g_go[wi]);
          for (int wi = 0; wi < nw; wi++)
            qurt_sem_down(&g_done);
          for (int i = 0; i < LDPC_NUM_PHASES; i++)
            phase_us[i] += g_phase_accum[i];
          /* Report what THIS device decoded, not the whole TB: with the batch
           * split the ARM validates each response against that device's share. */
          done = br->seg_count ? br->seg_count : (uint32_t)br->C;
        } else {
          g_job.br = br;
          g_job.llr_base = llr_base;
          g_job.out_base = out_base;
          g_job.meta = meta;
          g_job.out_bytes = out_bytes;
          /* Serial path honours the same range, so nworkers == 1 behaves like
           * the pool with one worker rather than silently decoding the whole TB
           * on both devices. */
          const uint16_t s0 = br->seg_first;
          const uint16_t sn = br->seg_count ? br->seg_count : br->C;
          for (uint16_t r = s0; r < s0 + sn && r < br->C; r++) {
            ldpc_decode_one(w, (int)r, phase_us);
            done++;
          }
        }
      }

      /* CACHE FLAGS ONLY -- no DEREF. These buffers are fastrpc_mmap'd for
       * the session lifetime, so there is no reference count to maintain,
       * and the batch request deliberately sends no REF. A DEREF with no
       * matching REF is not merely redundant: dspqueue_read on the ARM
       * rejects the whole packet with AEE_EBADPARM, which reads as "packet
       * too large" and sends you looking at sizes. The input needs no
       * response entry at all -- the DSP only read it. */
      /* Flush ONLY the segments this device decoded. With the TB split across both
       * CDSPs, flushing the whole range would write back lines this device never
       * wrote. out_stride is 1152 = 9 x 128, so segment boundaries fall on cache
       * lines and the two devices' ranges never share one.
       *
       * `meta` is different and is why it is a PER-DEVICE buffer: it is ONE BYTE per
       * segment, so a shared meta would put both devices' entries in the same
       * 128-byte line. Each would hold that line dirty with only its own bytes valid
       * and the later flush would clobber the other's -- silent corruption of the
       * iteration counts. Per-device buffers make the whole range safe to flush. */
      /* Describe the WHOLE output range, from the mapped base -- do NOT narrow this
       * to the device's own segments.
       *
       * Narrowing it looked like an obvious saving and was wrong twice over. The
       * response ptr has to be the address the buffer was mapped at; an offset into
       * the mapping does not resolve, so the ARM-side INVALIDATE is silently skipped
       * and the ARM reads stale lines for that device's half. That cost ~5% of
       * transport blocks, only on the device with a non-zero seg_first, which is
       * exactly the shape of "works single, fails split".
       *
       * The wide range is safe: a device only ever holds DIRTY lines for the segments
       * it decoded, and the request now invalidates this buffer on the way in, so
       * flushing the rest writes back nothing. */
      struct dspqueue_buffer rb[2];
      memset(rb, 0, sizeof rb);
      rb[0].fd = bufs[1].fd;
      rb[0].ptr = out_base;
      rb[0].size = (uint32_t)br->C * br->out_stride;
      rb[0].flags = DSPQUEUE_BUFFER_FLAG_FLUSH_SENDER | DSPQUEUE_BUFFER_FLAG_INVALIDATE_RECIPIENT;
      rb[1].fd = bufs[2].fd;
      rb[1].ptr = meta;
      rb[1].size = br->C;
      rb[1].flags = DSPQUEUE_BUFFER_FLAG_FLUSH_SENDER | DSPQUEUE_BUFFER_FLAG_INVALIDATE_RECIPIENT;

      ldpc_q_batch_resp_t bresp;
      memset(&bresp, 0, sizeof bresp);
      bresp.msg_type = LDPC_QMSG_BRESULT;
      bresp.C = done;
      bresp.dsp_clk_hz = ctx->dsp_clk_hz;
      bresp.skel_version = LDPC_SKEL_VERSION;
      /* Segment 0 only: enough to bisect, and every TB we debug has C>=1. */
      bresp.dbg_hash[0] = g_workers[0].dbg_hash[0];
      bresp.dbg_hash[1] = g_workers[0].dbg_hash[1];
      bresp.dbg_hash[2] = g_workers[0].dbg_hash[2];
      bresp.status = status;
      bresp.nworkers = (uint32_t)nw;
      bresp.hvx_units = (uint32_t)qurt_hvx_get_units();
      /* Not a boolean any more: the number of VTCM bytes granted. "Non-zero context"
       * is not the same as "got what I asked for", and a short grant silently
       * overlaps the per-worker slices. */
      bresp.vtcm_ok = g_vtcm_granted;
      bresp.batch_us = (uint32_t)(HAP_perf_get_time_us() - batch_t0);
      bresp.claims = ((uint32_t)(g_job.claims[0] & 0xff)) | ((uint32_t)(g_job.claims[1] & 0xff) << 8)
                     | ((uint32_t)(g_job.claims[2] & 0xff) << 16) | ((uint32_t)(g_job.claims[3] & 0xff) << 24);
      memcpy(bresp.phase_us, phase_us, sizeof phase_us);
      err = dspqueue_write(ctx->dq, 0, 2, rb, sizeof bresp, (const uint8_t *)&bresp, DSPQUEUE_TIMEOUT_NONE);
      if (err != 0)
        FARF(ERROR, "ldpc_hexagon: batch response write err 0x%08x", (unsigned)err);
      continue;
    }

    req = msg.d;
    if (req.msg_type != LDPC_QMSG_DECODE || msg_len < sizeof(req) || num_bufs < 2) {
      FARF(ERROR,
           "ldpc_hexagon: unexpected packet type=%u msg_len=%u bufs=%u",
           (unsigned)req.msg_type,
           (unsigned)msg_len,
           (unsigned)num_bufs);
      continue;
    }

    const int8_t *p_llr = (const int8_t *)bufs[0].ptr;
    uint8_t *llr_out = (uint8_t *)bufs[1].ptr;

    t_nrLDPC_dec_params dp;
    memset(&dp, 0, sizeof(dp));
    dp.BG = req.BG;
    dp.Z = req.Z;
    dp.R = req.R;
    dp.numMaxIter = req.numMaxIter;

    t_nrLDPC_lut lut;
    memset(&lut, 0, sizeof(lut));
    uint32_t numLLR = nrLDPC_init(&dp, &lut);

    uint32_t phase_us[LDPC_NUM_PHASES] = {0};
    int32_t numIter = 0;
    if (numLLR && numLLR == (uint32_t)req.numLLR)
      /* legacy per-segment path: always worker 0, it is serialised by
       * construction (one dispatch, one response, one decode). */
      numIter = ldpc_scalar_core(p_llr,
                                 llr_out,
                                 numLLR,
                                 &lut,
                                 req.BG,
                                 req.Z,
                                 req.numMaxIter,
                                 &g_workers[0],
                                 0,
                                 req.Kprime,
                                 req.crc_type,
                                 phase_us,
                                 ldpc_addr_cache_get(&lut, req.BG, req.Z, req.R));

    // Release input ref; flush+invalidate output so ARM sees updated LLRs.
    struct dspqueue_buffer resp_bufs[2];
    resp_bufs[0].fd = bufs[0].fd;
    resp_bufs[0].size = 0;
    resp_bufs[0].offset = 0;
    resp_bufs[0].ptr = NULL;
    resp_bufs[0].address = 0;
    resp_bufs[0].flags = DSPQUEUE_BUFFER_FLAG_DEREF;
    resp_bufs[1].fd = bufs[1].fd;
    resp_bufs[1].size = req.numLLR;
    resp_bufs[1].offset = 0;
    resp_bufs[1].ptr = llr_out;
    resp_bufs[1].address = 0;
    resp_bufs[1].flags = DSPQUEUE_BUFFER_FLAG_DEREF | DSPQUEUE_BUFFER_FLAG_FLUSH_SENDER | DSPQUEUE_BUFFER_FLAG_INVALIDATE_RECIPIENT;

    ldpc_q_resp_t resp;
    resp.msg_type = LDPC_QMSG_RESULT;
    resp.numIter = (uint32_t)numIter;
    resp.dsp_clk_hz = ctx->dsp_clk_hz;
    resp.skel_version = LDPC_SKEL_VERSION;
    resp.dbg_hash[0] = g_workers[0].dbg_hash[0];
    resp.dbg_hash[1] = g_workers[0].dbg_hash[1];
    resp.dbg_hash[2] = g_workers[0].dbg_hash[2];
    memcpy(resp.phase_us, phase_us, sizeof(phase_us));

    err = dspqueue_write(ctx->dq, 0, 2, resp_bufs, sizeof(resp), (const uint8_t *)&resp, DSPQUEUE_TIMEOUT_NONE);
    if (err != 0)
      FARF(ERROR, "ldpc_hexagon: dspqueue_write (response) err 0x%08x", (unsigned)err);
  }

  dspqueue_close(ctx->dq);
  ctx->dq = NULL;
  FARF(ALWAYS, "ldpc_hexagon: dspqueue worker exited");
  return AEE_SUCCESS;
}

// worker_stop is not used for the spinning worker — ARM sends LDPC_QMSG_STOP via
// the queue instead.  This stub satisfies the IDL; it's a no-op on the DSP.
AEEResult ldpc_hexagon_worker_stop(remote_handle64 handle, uint32 *status)
{
  (void)handle;
  *status = 0;
  return AEE_SUCCESS;
}

int ldpc_hexagon_open(const char *uri, remote_handle64 *handle)
{
  FARF(ALWAYS, "ldpc_hexagon_open: skel v%d (diag:cn2bn=ION_cpy,bn2cn=llrProcBuf,scatter=circ)", LDPC_SKEL_VERSION);
  ldpc_dsp_ctx_t *ctx = calloc(1, sizeof(ldpc_dsp_ctx_t));
  if (!ctx)
    return -1;

  /* ---- per-worker working sets ----
   * cn_msg/bn_msg go in VTCM: HVX-only access, and measured worth 19% of the
   * decode. llrRes/llrProcBuf deliberately do NOT: the gather
   * (nrLDPC_llrRes2llrOut) reads them with SCALAR accesses, and scalar reads
   * bypass L1/L2 to hit VTCM SRAM at ~20 cycles. Same distinction as before,
   * now replicated per worker. */
  g_nworkers = 1;
#ifdef LDPC_HEX_VTCM
  {
    compute_res_attr_t attr;
    HAP_compute_res_attr_init(&attr);
    HAP_compute_res_attr_set_vtcm_param(&attr, VTCM_TOTAL_SIZE, 1 /* single_page */);
    ctx->vtcm_ctx = HAP_compute_res_acquire(&attr, 0 /* no timeout */);
    if (ctx->vtcm_ctx == 0) {
      /* nothing here needs one page; retry fragmented before giving up */
      HAP_compute_res_attr_init(&attr);
      HAP_compute_res_attr_set_vtcm_param(&attr, VTCM_TOTAL_SIZE, 0);
      ctx->vtcm_ctx = HAP_compute_res_acquire(&attr, 0);
    }
    /* Read back what was ACTUALLY granted, not just whether the acquire returned a
     * context. The v2 getter reports the size, and a grant can legitimately be smaller
     * than the request -- that is what set_vtcm_param_v2's min_vtcm_size exists for.
     * Believing a non-zero context means "got what I asked for" is how a too-large
     * request turns into per-worker slices that overlap.
     *
     * printf, not FARF: on Linux HLOS there is no logcat, and the fastrpc driver
     * forwards DSP stdout to the ARM process (see OPTIMISATION-NOTES.md, "Viewing FARF output"). */
    unsigned int vtcm_granted = 0;
    if (ctx->vtcm_ctx != 0) {
      void *p2 = NULL;
      int rc2 = HAP_compute_res_attr_get_vtcm_ptr_v2(&attr, &p2, &vtcm_granted);
      if (rc2 == 0 && p2) {
        ctx->vtcm_base = p2;
      } else {
        ctx->vtcm_base = HAP_compute_res_attr_get_vtcm_ptr(&attr);
        vtcm_granted = 0; /* unknown */
      }
    }
    printf("[ldpc-dsp] VTCM req=%u granted=%u base=%p per_worker=%u workers=%d -> %s\n",
           (unsigned)VTCM_TOTAL_SIZE,
           vtcm_granted,
           ctx->vtcm_base,
           (unsigned)VTCM_PER_WORKER,
           LDPC_MAX_WORKERS,
           ctx->vtcm_base ? "ok" : "FAILED, heap fallback");
    /* ERROR level is the one that reaches the ARM kernel ring buffer on Linux HLOS
     * (no logcat here); read it with dmesg. Not an error -- it is the only channel. */
    FARF(ERROR,
         "ldpc-dsp VTCM req=%u granted=%u base=%p per_worker=%u workers=%d %s",
         (unsigned)VTCM_TOTAL_SIZE,
         vtcm_granted,
         ctx->vtcm_base,
         (unsigned)VTCM_PER_WORKER,
         LDPC_MAX_WORKERS,
         ctx->vtcm_base ? "ok" : "FAILED");
    /* A short grant is worse than none: the slices would overlap and silently corrupt
     * every worker above the first. Refuse it and take the heap path instead. */
    g_vtcm_granted = ctx->vtcm_base ? vtcm_granted : 0u;
    if (ctx->vtcm_base && vtcm_granted != 0 && vtcm_granted < (unsigned)VTCM_TOTAL_SIZE) {
      ctx->vtcm_base = NULL;
      g_vtcm_granted = 0u;
    }
    FARF(ALWAYS,
         "ldpc_hexagon: VTCM %u B granted %u for %d workers -> %s",
         (unsigned)VTCM_TOTAL_SIZE,
         vtcm_granted,
         LDPC_MAX_WORKERS,
         ctx->vtcm_base ? "ok" : "FAILED, heap fallback");
  }
#endif
  for (int wi = 0; wi < LDPC_MAX_WORKERS; wi++) {
    ldpc_worker_t *w = &g_workers[wi];
    int vtcm_llr = 0; /* llrRes/llrProcBuf came from VTCM: not ours to free */
#ifdef LDPC_HEX_VTCM
    if (ctx->vtcm_base) {
      uint8_t *p = (uint8_t *)ctx->vtcm_base + (size_t)wi * VTCM_PER_WORKER;
      w->cn_msg = (int8_t *)p;
      p += LDPC_VTCM_ALIGN(NR_LDPC_SIZE_CN_PROC_BUF + HVX_VLEN);
      w->bn_msg = (int8_t *)p;
      p += LDPC_VTCM_ALIGN(NR_LDPC_SIZE_BN_PROC_BUF + HVX_VLEN);
      w->llrRes = (int8_t *)p;
      p += LDPC_VTCM_LLR_SZ;
      w->llrProcBuf = (int8_t *)p;
      p += LDPC_VTCM_LLR_SZ;
      vtcm_llr = 1;
#ifdef LDPC_HEX_GATHER
      w->gv_f = (int16_t *)p;
      w->gv_e = w->gv_f + LDPC_GV_F_HW;
#endif
    }
#endif
    if (!w->cn_msg) {
      w->cn_msg = malloc(NR_LDPC_SIZE_CN_PROC_BUF + HVX_VLEN);
      w->bn_msg = malloc(NR_LDPC_SIZE_BN_PROC_BUF + HVX_VLEN);
    }
    if (!vtcm_llr) { /* heap fallback: VTCM unavailable, or a no-VTCM build */
      w->llrRes = malloc(NR_LDPC_MAX_NUM_LLR + HVX_VLEN);
      w->llrProcBuf = malloc(NR_LDPC_MAX_NUM_LLR + HVX_VLEN);
    }
    w->llrout = memalign(128, NR_LDPC_MAX_NUM_LLR + HVX_VLEN); /* slack: vector tail */
    w->crc_bits = memalign(128, (((NR_LDPC_MAX_NUM_LLR + 7) / 8) + 127) & ~127u);
    w->prep_e = memalign(128, NR_LDPC_MAX_NUM_LLR * sizeof(int16_t));
    w->prep_z = memalign(128, (68 * 384 + 16) * sizeof(int16_t));
    w->prep_out = memalign(128, NR_LDPC_MAX_NUM_LLR);
    if (!w->cn_msg || !w->bn_msg || !w->llrRes || !w->llrProcBuf || !w->llrout || !w->crc_bits || !w->prep_e || !w->prep_z
        || !w->prep_out) {
      FARF(ERROR, "ldpc_hexagon: worker %d allocation failed", wi);
      free(ctx);
      return -1;
    }
  }

  // Step 1: declare this a compute-class client so the system knows we need
  // full performance (not audio/voice power profile).
  {
    HAP_power_request_t areq;
    memset(&areq, 0, sizeof(areq));
    areq.type = HAP_power_set_apptype;
    areq.apptype = HAP_POWER_COMPUTE_CLIENT_CLASS;
    int arc = HAP_power_set(NULL, &areq);
    FARF(ALWAYS, "ldpc_hexagon: set_apptype(COMPUTE) rc=%d", arc);
  }

  // Step 2: explicitly power up HVX so the HVX units are active.
  {
    HAP_power_request_t hreq;
    memset(&hreq, 0, sizeof(hreq));
    hreq.type = HAP_power_set_HVX;
    hreq.hvx.power_up = TRUE;
    int hrc = HAP_power_set(NULL, &hreq);
    FARF(ALWAYS, "ldpc_hexagon: set_HVX(power_up) rc=%d", hrc);
  }

  // Step 3: vote for TURBO corner on core + bus, disable DCVS scaling and
  // all DSP low-power sleep modes.  The vote persists for the session lifetime.
  HAP_power_request_t req;
  memset(&req, 0, sizeof(req));
  req.type = HAP_power_set_DCVS_v3;
  req.dcvs_v3.set_dcvs_enable = TRUE;
  req.dcvs_v3.dcvs_enable = FALSE; // pin clock, disable DCVS scaling
  req.dcvs_v3.set_latency = TRUE;
  req.dcvs_v3.latency = 1; // 1 µs: tightest wakeup latency
  req.dcvs_v3.set_core_params = TRUE;
  req.dcvs_v3.core_params.min_corner = HAP_DCVS_VCORNER_TURBO;
  req.dcvs_v3.core_params.max_corner = HAP_DCVS_VCORNER_TURBO;
  req.dcvs_v3.core_params.target_corner = HAP_DCVS_VCORNER_TURBO;
  req.dcvs_v3.set_bus_params = TRUE;
  req.dcvs_v3.bus_params.min_corner = HAP_DCVS_VCORNER_TURBO;
  req.dcvs_v3.bus_params.max_corner = HAP_DCVS_VCORNER_TURBO;
  req.dcvs_v3.bus_params.target_corner = HAP_DCVS_VCORNER_TURBO;
  req.dcvs_v3.set_sleep_disable = TRUE;
  req.dcvs_v3.sleep_disable = HAP_DCVS_LPM_LEVEL1; // disable all sleep modes
  int rc = HAP_power_set(NULL, &req);
  FARF(ALWAYS, "ldpc_hexagon: HAP_power_set(DCVS_v3/TURBO) rc=%d", rc);

  // Step 4: fallback via the older (deprecated but different code path)
  // HAP_power_request_abs API.  Pass 100% clock/bus so it selects max corner.
  // On unsigned-PD where DCVS_v3 votes may be silently capped, this second
  // request via a different RPC path may be honoured.
  {
    int arc = HAP_power_request_abs(1000, 1000, 1);
    FARF(ALWAYS, "ldpc_hexagon: HAP_power_request_abs(1000MHz) rc=%d", arc);
  }

  // Read back the actual granted clock frequency.
  // NOTE: HAP_power_get returns 0 in unsigned-PD on some targets (same
  // restriction as hardware cycle counters).  Store whatever we get.
  HAP_power_response_t resp;
  memset(&resp, 0, sizeof(resp));
  resp.type = HAP_power_get_clk_Freq;
  HAP_power_get(NULL, &resp);
  ctx->dsp_clk_hz = resp.clkFreqHz;
  FARF(ALWAYS, "ldpc_hexagon: DSP core clock after vote = %u Hz (%u MHz)", ctx->dsp_clk_hz, ctx->dsp_clk_hz / 1000000u);

  *handle = (remote_handle64)(uintptr_t)ctx;
  FARF(RUNTIME_HIGH, "ldpc_hexagon: session opened, working buffers pre-allocated");
  return 0;
}

int ldpc_hexagon_close(remote_handle64 handle)
{
  ldpc_dsp_ctx_t *ctx = (ldpc_dsp_ctx_t *)(uintptr_t)handle;
  if (ctx) {
    if (ctx->dq) {
      dspqueue_close(ctx->dq);
      ctx->dq = NULL;
    }
    for (int wi = 0; wi < LDPC_MAX_WORKERS; wi++) {
      ldpc_worker_t *w = &g_workers[wi];
      if (ctx->vtcm_ctx == 0) { /* heap fallback: these are ours to free */
        free(w->cn_msg);
        free(w->bn_msg);
        free(w->llrRes);
        free(w->llrProcBuf);
      }
      free(w->llrout);
      free(w->crc_bits);
      free(w->prep_e);
      free(w->prep_z);
      free(w->prep_out);
      memset(w, 0, sizeof *w);
    }
#ifdef LDPC_HEX_VTCM
    if (ctx->vtcm_ctx != 0)
      HAP_compute_res_release(ctx->vtcm_ctx);
#endif
    free(ctx);
  }
  FARF(RUNTIME_HIGH, "ldpc_hexagon: session closed");
  return 0;
}

// =============================================================================
// Main RPC entry point — called by the qaic-generated skel dispatcher
// =============================================================================

int ldpc_hexagon_decode(remote_handle64 handle,
                        const uint8_t *params,
                        int paramsLen,
                        const uint8_t *llr,
                        int llrLen,
                        uint8_t *llr_out,
                        int llr_outLen,
                        uint8_t *meta,
                        int metaLen)
{
  if (paramsLen < (int)sizeof(ldpc_hex_params_t)) {
    FARF(ERROR, "ldpc_hexagon: params too short (%d < %d)", paramsLen, (int)sizeof(ldpc_hex_params_t));
    return -1;
  }

  ldpc_hex_params_t p;
  memcpy(&p, params, sizeof(p));

  FARF(RUNTIME_HIGH, "ldpc_hexagon: BG=%u Z=%u R=%u maxIter=%u numLLR_in=%d", p.BG, p.Z, p.R, p.numMaxIter, llrLen);

  // Build LUT on the DSP side (pure pointer assignments into static arrays)
  t_nrLDPC_dec_params dp;
  memset(&dp, 0, sizeof(dp));
  dp.BG = p.BG;
  dp.Z = p.Z;
  dp.R = p.R;
  dp.numMaxIter = p.numMaxIter;

  t_nrLDPC_lut lut;
  memset(&lut, 0, sizeof(lut));
  uint32_t numLLR = nrLDPC_init(&dp, &lut);
  if (!numLLR || (int)numLLR > llrLen || (int)numLLR > llr_outLen) {
    FARF(ERROR, "ldpc_hexagon: bad numLLR=%u (llrLen=%d out=%d)", numLLR, llrLen, llr_outLen);
    return -1;
  }

  // diag=0: raw memcpy passthrough; diag=1: scatter/gather roundtrip
  // diag=2: full BP with gather; diag=3: full BP, return raw llrRes (skip gather)
  // diag=4: llr2CnProcBuf only, return first numLLR bytes of cnProcBuf
  if (p.diag == 0) {
    memcpy(llr_out, llr, numLLR);
    if (metaLen >= 4) {
      uint32_t z = 0;
      memcpy(meta, &z, 4);
    }
    return 0;
  }
  if (p.diag == 1) {
    int8_t *llrProcBuf = calloc(NR_LDPC_MAX_NUM_LLR, 1);
    int8_t *llrRes = calloc(NR_LDPC_MAX_NUM_LLR, 1);
    if (llrProcBuf && llrRes) {
      nrLDPC_llr2llrProcBuf(&lut, (int8_t *)llr, llrProcBuf, p.Z, p.BG);
      memcpy(llrRes, llrProcBuf, NR_LDPC_MAX_NUM_LLR);
      nrLDPC_llrRes2llrOut(&lut, (int8_t *)llr_out, llrRes, p.Z, p.BG);
    }
    free(llrProcBuf);
    free(llrRes);
    if (metaLen >= 4) {
      uint32_t z = 0;
      memcpy(meta, &z, 4);
    }
    return 0;
  }
  if (p.diag == 4) {
    // Scatter LLRs into cnProcBuf, then return:
    //   llr_out[0..63]  = LUT diagnostic header (uint32_t fields)
    //   llr_out[64..]   = cnProcBuf[0..numLLR-65]
    // Header layout (16 × uint32_t):
    //   [0] circShift[0].d as uint32_t pointer
    //   [1] circShift[0].d[0] (first shift value, or 0xDEAD if d==NULL)
    //   [2] circShift[0].dim1
    //   [3] circShift[0].dim2
    //   [4] numCnInCnGroups[0]
    //   [5] startAddrCnGroups[0]
    //   [6] numBnInBnGroups[0]
    //   [7] startAddrBnGroups[0]
    //   [8..11] cnProcBuf[0..3] before scatter (should be 0)
    //   [12..15] cnProcBuf[0..3] after scatter (should be non-zero)
    int8_t *cnProcBuf4 = g_workers[0].cn_msg;
    memset(cnProcBuf4, 0, NR_LDPC_SIZE_CN_PROC_BUF);

    uint32_t hdr[16];
    hdr[0] = (uint32_t)(uintptr_t)lut.circShift[0].d;
    hdr[1] = lut.circShift[0].d ? (uint32_t)lut.circShift[0].d[0] : 0xDEADu;
    hdr[2] = (uint32_t)lut.circShift[0].dim1;
    hdr[3] = (uint32_t)lut.circShift[0].dim2;
    hdr[4] = (uint32_t)lut.numCnInCnGroups[0];
    hdr[5] = (uint32_t)lut.startAddrCnGroups[0];
    hdr[6] = (uint32_t)lut.numBnInBnGroups[0];
    hdr[7] = (uint32_t)lut.startAddrBnGroups[0];
    hdr[8] = (uint32_t)(uint8_t)cnProcBuf4[0]; // before scatter (should be 0)
    hdr[9] = hdr[10] = hdr[11] = 0;

    if (p.BG == 1)
      nrLDPC_llr2CnProcBuf_BG1(&lut, (int8_t *)llr, cnProcBuf4, p.Z);
    else
      nrLDPC_llr2CnProcBuf_BG2(&lut, (int8_t *)llr, cnProcBuf4, p.Z);

    hdr[12] = (uint32_t)(uint8_t)cnProcBuf4[0]; // after scatter (should be llr[332])
    hdr[13] = (uint32_t)(uint8_t)cnProcBuf4[1];
    hdr[14] = (uint32_t)(uint8_t)cnProcBuf4[384]; // start of BN j=1 slot
    hdr[15] = (uint32_t)(uint8_t)cnProcBuf4[768]; // start of BN j=2 slot

    memcpy(llr_out, hdr, 64);
    if ((int)numLLR > 64)
      memcpy(llr_out + 64, cnProcBuf4, numLLR - 64);

    if (metaLen >= 4) {
      uint32_t z = 0;
      memcpy(meta, &z, 4);
    }
    return 0;
  }
  // diag=2, diag=3: full BP below

  ldpc_dsp_ctx_t *ctx = (ldpc_dsp_ctx_t *)(uintptr_t)handle;

  int skip_gather = (p.diag == 3) ? 1 : 0;
  uint32_t phase_us[LDPC_NUM_PHASES] = {0};
  int32_t numIter = ldpc_scalar_core((const int8_t *)llr,
                                     llr_out,
                                     numLLR,
                                     &lut,
                                     p.BG,
                                     p.Z,
                                     p.numMaxIter,
                                     &g_workers[0],
                                     skip_gather,
                                     p.Kprime,
                                     p.crc_type,
                                     phase_us,
                                     ldpc_addr_cache_get(&lut, p.BG, p.Z, p.R));

  FARF(RUNTIME_HIGH, "ldpc_hexagon: done in %d iter", numIter);

  if (metaLen >= 4) {
    uint32_t n = (uint32_t)numIter;
    memcpy(meta, &n, 4);
  }
  if (metaLen >= 8) {
    uint32_t clk = ctx->dsp_clk_hz;
    memcpy(meta + 4, &clk, 4);
  }
  if (metaLen >= (int)(8 + LDPC_NUM_PHASES * 4))
    memcpy(meta + 8, phase_us, LDPC_NUM_PHASES * sizeof(uint32_t));

  return 0;
}

#endif // !LDPC_SIM_STANDALONE (worker pool + FastRPC)
