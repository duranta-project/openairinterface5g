// SPDX-License-Identifier: LicenseRef-CSSL-1.0
//
// ldpc_hexagon_arm.c — ARM-side LDPC decoder stub.
//
// This file is compiled (with the qaic-generated ldpc_hexagon_stub.c) into
// libldpc_hexagon.so, a drop-in replacement for libldpc.so that offloads
// belief-propagation iterations to the Hexagon Compute DSP via FastRPC.
//
// Exported symbols match the standard libldpc interface:
//   LDPCinit()     — open FastRPC session, initialise rpcmem
//   LDPCshutdown() — tear down session and rpcmem
//   LDPCdecoder()  — offload decode call to cDSP, post-process output
//
// The DSP always returns posterior LLRs (int8, numLLR bytes). The ARM stub
// performs the outMode conversion (bit-pack / int8 / LLR pass-through) so
// the DSP side remains format-agnostic.
//
// FastRPC / rpcmem docs: Hexagon SDK → ipc/fastrpc/

#define _GNU_SOURCE // pthread_attr_setaffinity_np, cpu_set_t

#include "ldpc_hexagon.h" // qaic-generated from ldpc_hexagon.idl
#include "ldpc_hexagon_queue.h" // dspqueue packet format
#include "ldpc_hexagon_batch.h" // TB-level batch API
#include "remote.h" // CDSP_DOMAIN_ID, fastrpc_mmap/munmap
#include "rpcmem.h"
#include "dspqueue.h"

#include <stdint.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

// OAI LDPC interface
#include "PHY/CODING/nrLDPC_decoder/nrLDPCdecoder_defs.h"
#include "PHY/CODING/nrLDPC_decoder/nrLDPC_types.h"
#include "PHY/CODING/nrLDPC_decoder/nrLDPC_init.h"
#include "openair1/PHY/CODING/coding_defs.h"

// ---------------------------------------------------------------------------
// Bit conversion (scalar — avoids pulling in the SIMD bnProc headers here)
// ---------------------------------------------------------------------------

// Output matches nrLDPC_llr2bitPacked: bit (7-i) of byte b = sign(llr[b*8+i])
// i.e. MSB-first within each byte — bit 7 = first LLR in group, bit 0 = eighth.
static inline void arm_llr2bitPacked(uint8_t *out, const int8_t *llr, uint32_t numLLR)
{
  uint32_t nbytes = (numLLR + 7) >> 3;
  for (uint32_t b = 0; b < nbytes; b++) {
    uint8_t byte = 0;
    for (int i = 0; i < 8 && b * 8u + i < numLLR; i++)
      if (llr[b * 8 + i] < 0)
        byte |= (uint8_t)(1u << (7 - i));
    out[b] = byte;
  }
}

// Output: one int8_t per bit (0 or 1)
static inline void arm_llr2bit(uint8_t *out, const int8_t *llr, uint32_t numLLR)
{
  for (uint32_t i = 0; i < numLLR; i++)
    out[i] = llr[i] < 0 ? 1u : 0u;
}

// Wire parameter struct (8 bytes packed, must match ldpc_hexagon_imp.c)
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

static int g_domain = CDSP_DOMAIN_ID;
static remote_handle64 dsp_hdl = (remote_handle64)-1;
static int rpcmem_up = 0;

// dspqueue spinning worker — created in LDPCinit, used in LDPCdecoder.
// worker_start blocks permanently on the DSP; a pthread keeps it running.
static dspqueue_t g_queue = NULL;
static int g_llr_fd = -1;
static int g_llrout_fd = -1;
static int g_buffers_mapped = 0;
static uint64_t g_queue_id = 0; // exported queue ID
static pthread_t g_worker_thread;
static int g_worker_running = 0;

typedef struct {
  remote_handle64 hdl;
  uint64_t queue_id;
} worker_args_t;

static worker_args_t g_worker_args;

static void *worker_thread_func(void *arg)
{
  worker_args_t *a = (worker_args_t *)arg;
  // This call blocks for the lifetime of the spinning DSP worker.
  int rc = ldpc_hexagon_worker_start(a->hdl, a->queue_id);
  if (rc)
    fprintf(stderr, "ldpc_hexagon: worker_start returned %d\n", rc);
  return NULL;
}

// Pre-allocated ION-backed rpcmem buffers (allocated once in LDPCinit,
// freed in LDPCshutdown).  Reusing avoids per-call alloc/free cycles that
// exhaust the rpcmem pool after a few hundred codewords.
#define LDPC_HEX_MAX_LLR 27648 // NR_LDPC_MAX_NUM_LLR rounded up to 64-byte boundary
// meta layout: [0..3] numIter, [4..7] dsp_clk_hz, [8..39] 8×uint32 phase_us (µs each)
#define LDPC_HEX_META_LEN 40
static ldpc_hex_params_t *g_rpc_params = NULL;
static int8_t *g_rpc_llr = NULL;
static int8_t *g_rpc_llrout = NULL;
static uint8_t *g_rpc_meta = NULL;

// Per-phase profiling accumulators (reset each LDPCinit, printed at LDPCshutdown).
static uint64_t g_phase_us_total[8];
static uint32_t g_decode_count;
/* DSP-side wall for the whole batch, ACCUMULATED. Printing only the first batch
 * (which is what this used to do) reports a cold-cache, first-touch figure --
 * with DSP-side prep that includes the initial allocation and clear of `d`, ~760
 * kB at single layer -- and it disagreed with the ARM-observed average by 3x. */
static uint64_t g_batch_wall_us_total;
static uint32_t g_batch_count;
/* Per-worker claim totals and the batch-wall spread.  The DSP returns claims on EVERY
 * batch but only the first was ever printed, and the first is the one batch whose
 * distribution means nothing: ldpc_pool_start() spawns the workers during it, so they
 * join staggered and it always looks lopsided (3/1/1/13 seen).  Without the steady-state
 * figure there is no way to tell a real imbalance from that startup artefact.  Min/max
 * wall matters too: for a real-time budget the worst batch is the number that has to fit,
 * not the mean. */
static uint64_t g_claims_total[4];
/* Where ldpc_hex_batch_run() actually spends its time.  ts_ldpc_decode reports ~3000 us
 * against a DSP wall of ~1790, and with prep on the DSP the ~1200 us difference must be
 * inside this call.  But "dispatch overhead" is three things with three different fixes --
 * the write and its cache maintenance on the shared buffers, the wake, and response
 * handling -- and only the wait is time the RX deferral can hide. */
static uint64_t g_disp_write_us, g_disp_wait_us, g_disp_post_us;
static uint32_t g_batch_wall_min_us = 0xffffffffu;
static uint32_t g_batch_wall_max_us;

/* ---- TB-level batch buffers -------------------------------------------------
 *
 * Sized for LDPC_HEX_BATCH_CB code blocks. 144 (NR_LDPC_MAX_NUM_CB) would need
 * 3.8 MB of contiguous ION for the input alone; 96 covers every configuration we
 * run (273 PRB, 2 layers, 256QAM tops out around 143 only at the very largest
 * TBS) and a caller whose C exceeds the cap is told so by ldpc_hex_batch_caps()
 * and falls back to the per-segment path rather than truncating. */
#define LDPC_HEX_BATCH_CB 96u
#define LDPC_HEX_BATCH_LLR_STR LDPC_HEX_MAX_LLR /* Kc*Z, BG1 worst case */
#define LDPC_HEX_BATCH_OUT_STR 1152u /* (22*384)/8 = 1056, padded */
static int8_t *g_batch_llr = NULL;
static uint8_t *g_batch_out = NULL;
static uint8_t *g_batch_meta = NULL;
static int g_batch_llr_fd = -1, g_batch_out_fd = -1, g_batch_meta_fd = -1;
static int g_batch_mapped = 0;

/* ---- second CDSP (dual-device data split) ---------------------------------------
 *
 * SA8775P has two compute DSPs. inner-RX is nowhere near LDPC in cost (1:6.8 at one
 * layer, 1:7.7 at two), so a FUNCTIONAL split would leave one device ~87% idle; the
 * second device takes LDPC SEGMENTS instead, which the existing pull queue already
 * suits -- it keeps balancing dynamically WITHIN each device.
 *
 * The llr and out buffers are SHARED: one rpcmem allocation mapped into both domains,
 * with each device addressing segments by absolute index. out_stride is 1152 = 9 x 128,
 * so the two devices' output ranges never share a cache line.
 *
 * `meta` is PER DEVICE, and that is not an optimisation. It is one byte per segment, so
 * a shared buffer would put both devices' entries in the same 128-byte line; each would
 * hold it dirty with only its own bytes valid, and the later flush would clobber the
 * other's iteration counts.
 *
 * Off by default (OAI_LDPC_DUAL=1). If anything about the second device fails -- open,
 * mapping, queue, worker -- g_ndev stays 1 and every segment goes to CDSP0, which is
 * exactly the previous behaviour. */
#define LDPC_NDEV_MAX 2
static int g_ndev = 1;
static remote_handle64 g_hdl2 = (remote_handle64)-1;
static dspqueue_t g_queue2 = NULL;
static uint64_t g_queue_id2 = 0;
static pthread_t g_worker_thread2;
static int g_worker_running2 = 0;
static worker_args_t g_worker_args2;
static uint8_t *g_batch_meta2 = NULL;
static int g_batch_meta2_fd = -1;

static dspqueue_t dev_queue(int i)
{
  return i ? g_queue2 : g_queue;
}
static uint8_t *dev_meta(int i)
{
  return i ? g_batch_meta2 : g_batch_meta;
}
static int dev_meta_fd(int i)
{
  return i ? g_batch_meta2_fd : g_batch_meta_fd;
}

// ---------------------------------------------------------------------------
// LDPCinit — open FastRPC session to cDSP and pre-allocate ION buffers.
// ---------------------------------------------------------------------------
static int32_t LDPCinit_body(void)
{
  // ldpctest calls LDPCinit() before every codeword to reset the decoder.
  // Re-opening the FastRPC session and re-allocating ION buffers on every
  // call would exhaust the rpcmem pool within a few codewords.  Return
  // immediately if the session is already open.
  if (dsp_hdl != (remote_handle64)-1)
    return 0;

  // Signed PD: testsig-0xa889b1e6.so deployed to /usr/lib/rfsa/adsp/ enables
  // signed PD on this board, giving access to VTCM, HAP_power_set, and full
  // clock voting.  Do NOT set DSPRPC_CONTROL_UNSIGNED_MODULE — that would
  // force unsigned PD and override the testsig.
  (void)0; // signed PD — no session control needed

  // rpcmem_init() opens /dev/dma_heap/system so libcdsprpc.so can DMA-transfer
  // fastrpc_shell_unsigned_3 (~1.26 MB) during remote_handle64_open() below.
  // Must be called before ldpc_hexagon_open().
  // NOTE: rpcmem.a is NOT linked (see CMakeLists.txt); this calls the device's
  // real rpcmem_init from libcdsprpc.so, not the SDK's deprecated stub.
  rpcmem_init();
  rpcmem_up = 1;

  /* OAI_LDPC_DOMAIN=1 targets the SECOND compute DSP. SA8775P has two, and the
   * device exposes /dev/fastrpc-cdsp1, so the decode could scale out across
   * both -- but only if the skel is loadable there AND signed PD carries over,
   * since VTCM and the TURBO clock vote both depend on it. This gate exists to
   * answer those questions before any of the split-across-domains work.
   *
   * fastrpc >= 1.0.4 on Linux HLOS needs the domain in the URI; the
   * qaic-generated ldpc_hexagon_URI does not include it. */
  {
    const char *e = getenv("OAI_LDPC_DOMAIN");
    g_domain = (e && e[0] == '1') ? CDSP1_DOMAIN_ID : CDSP_DOMAIN_ID;
  }
  const char *uri = (g_domain == CDSP1_DOMAIN_ID) ? ldpc_hexagon_URI CDSP1_DOMAIN : ldpc_hexagon_URI CDSP_DOMAIN;
  fprintf(stderr, "ldpc_hexagon: opening on domain %d (%s)\n", g_domain, g_domain == CDSP1_DOMAIN_ID ? "cdsp1" : "cdsp");

  int err = ldpc_hexagon_open(uri, &dsp_hdl);
  if (err) {
    fprintf(stderr, "ldpc_hexagon_open failed: %d\n", err);
    rpcmem_deinit();
    rpcmem_up = 0;
    return err;
  }

  // Pre-allocate ION buffers for the maximum possible LLR count.
  // This avoids per-codeword alloc/free cycles that exhaust the rpcmem pool.
  g_rpc_params = rpcmem_alloc(RPCMEM_HEAP_ID_SYSTEM, RPCMEM_DEFAULT_FLAGS, sizeof(ldpc_hex_params_t));
  g_rpc_llr = rpcmem_alloc(RPCMEM_HEAP_ID_SYSTEM, RPCMEM_DEFAULT_FLAGS, LDPC_HEX_MAX_LLR);
  g_rpc_llrout = rpcmem_alloc(RPCMEM_HEAP_ID_SYSTEM, RPCMEM_DEFAULT_FLAGS, LDPC_HEX_MAX_LLR);
  g_rpc_meta = rpcmem_alloc(RPCMEM_HEAP_ID_SYSTEM, RPCMEM_DEFAULT_FLAGS, LDPC_HEX_META_LEN);

  if (!g_rpc_params || !g_rpc_llr || !g_rpc_llrout || !g_rpc_meta) {
    fprintf(stderr, "ldpc_hexagon: rpcmem_alloc for pre-allocated buffers failed\n");
    ldpc_hexagon_close(dsp_hdl);
    dsp_hdl = (remote_handle64)-1;
    if (g_rpc_params) {
      rpcmem_free(g_rpc_params);
      g_rpc_params = NULL;
    }
    if (g_rpc_llr) {
      rpcmem_free(g_rpc_llr);
      g_rpc_llr = NULL;
    }
    if (g_rpc_llrout) {
      rpcmem_free(g_rpc_llrout);
      g_rpc_llrout = NULL;
    }
    if (g_rpc_meta) {
      rpcmem_free(g_rpc_meta);
      g_rpc_meta = NULL;
    }
    rpcmem_deinit();
    rpcmem_up = 0;
    return -1;
  }
  memset(g_phase_us_total, 0, sizeof(g_phase_us_total));
  g_decode_count = 0;
  fprintf(stderr,
          "ldpc_hexagon: pre-allocated %d-byte ION buffers (params+llr+llrout+meta)\n",
          (int)(sizeof(ldpc_hex_params_t) + 2 * LDPC_HEX_MAX_LLR + LDPC_HEX_META_LEN));

  // Pre-map ION buffers to DSP so dspqueue writes don't need per-call mapping.
  g_llr_fd = rpcmem_to_fd(g_rpc_llr);
  g_llrout_fd = rpcmem_to_fd(g_rpc_llrout);
  if (g_llr_fd < 0 || g_llrout_fd < 0) {
    fprintf(stderr, "ldpc_hexagon: rpcmem_to_fd failed (llr=%d llrout=%d)\n", g_llr_fd, g_llrout_fd);
    goto fail_queue;
  }
  if (fastrpc_mmap(g_domain, g_llr_fd, g_rpc_llr, 0, LDPC_HEX_MAX_LLR, FASTRPC_MAP_FD)) {
    fprintf(stderr, "ldpc_hexagon: fastrpc_mmap llr failed\n");
    goto fail_queue;
  }
  if (fastrpc_mmap(g_domain, g_llrout_fd, g_rpc_llrout, 0, LDPC_HEX_MAX_LLR, FASTRPC_MAP_FD)) {
    fastrpc_munmap(g_domain, g_llr_fd, NULL, 0);
    fprintf(stderr, "ldpc_hexagon: fastrpc_mmap llrout failed\n");
    goto fail_queue;
  }
  g_buffers_mapped = 1;

  /* Batch buffers. Failure here is NOT fatal: ldpc_hex_batch_bufs() then
   * reports unavailable and the coding layer keeps the per-segment path. */
  {
    const uint32_t llr_sz = LDPC_HEX_BATCH_CB * LDPC_HEX_BATCH_LLR_STR;
    const uint32_t out_sz = LDPC_HEX_BATCH_CB * LDPC_HEX_BATCH_OUT_STR;
    const uint32_t meta_sz = (uint32_t)sizeof(ldpc_q_batch_meta_t);
    g_batch_llr = rpcmem_alloc(RPCMEM_HEAP_ID_SYSTEM, RPCMEM_DEFAULT_FLAGS, llr_sz);
    g_batch_out = rpcmem_alloc(RPCMEM_HEAP_ID_SYSTEM, RPCMEM_DEFAULT_FLAGS, out_sz);
    g_batch_meta = rpcmem_alloc(RPCMEM_HEAP_ID_SYSTEM, RPCMEM_DEFAULT_FLAGS, meta_sz);
    if (g_batch_llr && g_batch_out && g_batch_meta) {
      g_batch_llr_fd = rpcmem_to_fd(g_batch_llr);
      g_batch_out_fd = rpcmem_to_fd(g_batch_out);
      g_batch_meta_fd = rpcmem_to_fd(g_batch_meta);
      if (g_batch_llr_fd >= 0 && g_batch_out_fd >= 0 && g_batch_meta_fd >= 0
          && !fastrpc_mmap(g_domain, g_batch_llr_fd, g_batch_llr, 0, llr_sz, FASTRPC_MAP_FD)
          && !fastrpc_mmap(g_domain, g_batch_out_fd, g_batch_out, 0, out_sz, FASTRPC_MAP_FD)
          && !fastrpc_mmap(g_domain, g_batch_meta_fd, g_batch_meta, 0, meta_sz, FASTRPC_MAP_FD)) {
        g_batch_mapped = 1;
        fprintf(stderr,
                "ldpc_hexagon: batch buffers ready (%u CB, %u kB in, %u kB out)\n",
                LDPC_HEX_BATCH_CB,
                llr_sz >> 10,
                out_sz >> 10);
      } else {
        fprintf(stderr, "ldpc_hexagon: batch buffer mapping failed -- per-segment path\n");
      }
    } else {
      fprintf(stderr, "ldpc_hexagon: batch buffer alloc failed -- per-segment path\n");
    }
  }

  // Create dspqueue (ARM side) — no ARM-side packet callback; we busy-poll reads.
  {
    int qerr = dspqueue_create(g_domain, 0, 0, 0, NULL, NULL, NULL, &g_queue);
    if (qerr) {
      fprintf(stderr, "ldpc_hexagon: dspqueue_create failed: 0x%08x\n", (unsigned)qerr);
      goto fail_queue;
    }
    qerr = dspqueue_export(g_queue, &g_queue_id);
    if (qerr) {
      fprintf(stderr, "ldpc_hexagon: dspqueue_export failed: 0x%08x\n", (unsigned)qerr);
      dspqueue_close(g_queue);
      g_queue = NULL;
      goto fail_queue;
    }
    // Spawn pthread that blocks on worker_start (spinning DSP loop).
    // The DSP worker runs until we write LDPC_QMSG_STOP to the queue.
    g_worker_args.hdl = dsp_hdl;
    g_worker_args.queue_id = g_queue_id;
    if (pthread_create(&g_worker_thread, NULL, worker_thread_func, &g_worker_args)) {
      fprintf(stderr, "ldpc_hexagon: pthread_create for worker failed\n");
      dspqueue_close(g_queue);
      g_queue = NULL;
      goto fail_queue;
    }
    g_worker_running = 1;
    fprintf(stderr, "ldpc_hexagon: dspqueue spinning worker spawned (queue_id=%llu)\n", (unsigned long long)g_queue_id);
  }

  /* Bring up the SECOND CDSP, if asked and if the batch buffers exist. Every failure
   * path here leaves g_ndev == 1, so the caller silently keeps the single-device
   * behaviour rather than half-configuring the split. */
  if (g_batch_mapped && getenv("OAI_LDPC_DUAL") && getenv("OAI_LDPC_DUAL")[0] == '1') {
    const uint32_t llr_sz = LDPC_HEX_BATCH_CB * LDPC_HEX_BATCH_LLR_STR;
    const uint32_t out_sz = LDPC_HEX_BATCH_CB * LDPC_HEX_BATCH_OUT_STR;
    const uint32_t meta_sz = (uint32_t)sizeof(ldpc_q_batch_meta_t);
    int ok = 0;
    do {
      int e2 = ldpc_hexagon_open(ldpc_hexagon_URI CDSP1_DOMAIN, &g_hdl2);
      if (e2) {
        fprintf(stderr, "ldpc_hexagon: cdsp1 open failed %d -- single device\n", e2);
        break;
      }
      /* The SAME llr/out fds mapped into the second domain. If ION refuses a second
       * mapping this is where it shows up, and it is the one assumption of the whole
       * split. meta is a separate allocation -- see the note on false sharing. */
      if (fastrpc_mmap(CDSP1_DOMAIN_ID, g_batch_llr_fd, g_batch_llr, 0, llr_sz, FASTRPC_MAP_FD)
          || fastrpc_mmap(CDSP1_DOMAIN_ID, g_batch_out_fd, g_batch_out, 0, out_sz, FASTRPC_MAP_FD)) {
        fprintf(stderr, "ldpc_hexagon: cdsp1 shared-buffer mmap failed -- single device\n");
        break;
      }
      g_batch_meta2 = rpcmem_alloc(RPCMEM_HEAP_ID_SYSTEM, RPCMEM_DEFAULT_FLAGS, meta_sz);
      if (!g_batch_meta2) {
        fprintf(stderr, "ldpc_hexagon: cdsp1 meta alloc failed -- single device\n");
        break;
      }
      g_batch_meta2_fd = rpcmem_to_fd(g_batch_meta2);
      if (g_batch_meta2_fd < 0 || fastrpc_mmap(CDSP1_DOMAIN_ID, g_batch_meta2_fd, g_batch_meta2, 0, meta_sz, FASTRPC_MAP_FD)) {
        fprintf(stderr, "ldpc_hexagon: cdsp1 meta mmap failed -- single device\n");
        break;
      }
      if (dspqueue_create(CDSP1_DOMAIN_ID, 0, 0, 0, NULL, NULL, NULL, &g_queue2) || dspqueue_export(g_queue2, &g_queue_id2)) {
        fprintf(stderr, "ldpc_hexagon: cdsp1 queue create/export failed -- single device\n");
        break;
      }
      g_worker_args2.hdl = g_hdl2;
      g_worker_args2.queue_id = g_queue_id2;
      if (pthread_create(&g_worker_thread2, NULL, worker_thread_func, &g_worker_args2)) {
        fprintf(stderr, "ldpc_hexagon: cdsp1 worker thread failed -- single device\n");
        break;
      }
      g_worker_running2 = 1;
      ok = 1;
    } while (0);
    if (ok) {
      g_ndev = 2;
      fprintf(stderr, "ldpc_hexagon: DUAL CDSP active -- segments split across cdsp0+cdsp1\n");
    }
  }

  return 0;

fail_queue:
  fprintf(stderr, "ldpc_hexagon: dspqueue init failed — falling back to synchronous FastRPC\n");
  return 0; // non-fatal: LDPCdecoder falls back to ldpc_hexagon_decode()
}

// FastRPC's helper threads inherit whatever thread creates them, and OAI reaches
// LDPCinit() lazily from whichever thread decodes first -- a Tpool worker or
// L1_rx_thread, both SCHED_RR 97 pinned to a single core. pthreads default to
// PTHREAD_INHERIT_SCHED and the affinity mask is inherited unconditionally, so
// libcdsprpc's internal threads would otherwise land at RT 97 on that core and
// contend with the very thread waiting on them. Under SCHED_RR an equal-priority
// wakeup does not preempt, so each response waits out an RR peer's quantum; on
// the inner-RX offload that cost 11,252 us per slot against 1,114 us, and dropped
// 85% of UL slots. Same fix here, same reasoning as inner_rx_hexagon_arm.c.
// PTHREAD_EXPLICIT_SCHED is load-bearing: without it the attr's policy is
// ignored and the creator's RT parameters are inherited regardless.
static void *ldpc_boot_thread(void *p)
{
  *(int32_t *)p = LDPCinit_body();
  return NULL;
}

int32_t LDPCinit(void)
{
  if (dsp_hdl != (remote_handle64)-1)
    return 0;

  pthread_attr_t attr;
  if (pthread_attr_init(&attr) != 0)
    return LDPCinit_body();
  pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
  const struct sched_param sp = {.sched_priority = 0};
  pthread_attr_setschedparam(&attr, &sp);

  cpu_set_t all;
  CPU_ZERO(&all);
  const long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
  for (long i = 0; i < (ncpu > 0 ? ncpu : 1); i++)
    CPU_SET((int)i, &all);
  pthread_attr_setaffinity_np(&attr, sizeof all, &all);

  int32_t rc = -1;
  pthread_t boot;
  const int cerr = pthread_create(&boot, &attr, ldpc_boot_thread, &rc);
  pthread_attr_destroy(&attr);
  if (cerr != 0) {
    // Non-fatal: initialising on the caller's thread is the old behaviour,
    // and a working offload with bad thread placement beats no offload.
    fprintf(stderr,
            "ldpc_hexagon: bootstrap pthread_create failed (%d), initialising on"
            " the calling thread -- expect degraded RX timing\n",
            cerr);
    return LDPCinit_body();
  }
  pthread_join(boot, NULL);
  return rc;
}

// ---------------------------------------------------------------------------
// LDPCshutdown — close FastRPC session.
// ---------------------------------------------------------------------------
static void print_phase_profile(void)
{
  if (g_decode_count == 0)
    return;
  static const char *const phase_names[8] = {/* slot 6 is the termination test: llr2bits+crc24 under CRC termination (the normal
                                              * path), or the cnProcPc parity check when CRC is unavailable -- never both.
                                              * slot 7, the gather, runs EVERY iteration under CRC termination, not once.
                                              * slots 2 and 5 were named cn2bn/bn2cn for a message-passing pair that no longer
                                              * exists -- cnProc_fused and bnProc_fused absorbed both. They time the input-side
                                              * copies. */
                                             "scatter",
                                             "cnProc",
                                             "llrcopy",
                                             "bnProcPc",
                                             "bnProc",
                                             "llr2proc",
                                             "termtest",
                                             "gather"};
  uint64_t compute_us = 0;
  for (int i = 0; i < 8; i++)
    compute_us += g_phase_us_total[i];
  fprintf(stderr, "\nDSP phase profile (%u codewords, µs/codeword, QTIMER 19.2 MHz):\n", g_decode_count);
  for (int i = 0; i < 8; i++) {
    uint64_t avg = g_phase_us_total[i] / g_decode_count;
    fprintf(stderr,
            "  %-10s %5llu us  (%3.0f%%)\n",
            phase_names[i],
            (unsigned long long)avg,
            compute_us > 0 ? 100.0 * (double)g_phase_us_total[i] / (double)compute_us : 0.0);
  }
  fprintf(stderr,
          "  %-10s %5llu us  (DSP compute, excl. FastRPC overhead)\n",
          "total",
          (unsigned long long)(compute_us / g_decode_count));
  if (g_batch_count)
    fprintf(stderr,
            "  %-10s %5llu us  (DSP wall per batch, avg over %u batches)\n",
            "batch",
            (unsigned long long)(g_batch_wall_us_total / g_batch_count),
            g_batch_count);
  if (g_batch_count) {
    const uint64_t cl = g_claims_total[0] + g_claims_total[1] + g_claims_total[2] + g_claims_total[3];
    fprintf(stderr,
            "  %-10s write %llu + wait %llu + post %llu us per batch (wait contains the DSP)\n",
            "dispatch",
            (unsigned long long)(g_disp_write_us / g_batch_count),
            (unsigned long long)(g_disp_wait_us / g_batch_count),
            (unsigned long long)(g_disp_post_us / g_batch_count));
    fprintf(stderr, "  %-10s %5u..%u us (min..max batch wall)\n", "spread", g_batch_wall_min_us, g_batch_wall_max_us);
    if (cl)
      fprintf(stderr,
              "  %-10s %llu/%llu/%llu/%llu blocks (%.0f/%.0f/%.0f/%.0f%% of %llu, steady state)\n",
              "claims",
              (unsigned long long)g_claims_total[0],
              (unsigned long long)g_claims_total[1],
              (unsigned long long)g_claims_total[2],
              (unsigned long long)g_claims_total[3],
              100.0 * (double)g_claims_total[0] / (double)cl,
              100.0 * (double)g_claims_total[1] / (double)cl,
              100.0 * (double)g_claims_total[2] / (double)cl,
              100.0 * (double)g_claims_total[3] / (double)cl,
              (unsigned long long)cl);
  }
  /* Reset BOTH counters, not just the codeword one: leaving g_batch_count
   * running made the second print a cumulative average over a different window
   * than the first, so the same run reported two different batch walls. */
  g_decode_count = 0;
  g_batch_wall_us_total = 0;
  g_batch_count = 0;
  for (int i = 0; i < 4; i++)
    g_claims_total[i] = 0;
  g_batch_wall_min_us = 0xffffffffu;
  g_batch_wall_max_us = 0;
  g_disp_write_us = g_disp_wait_us = g_disp_post_us = 0;
}

static void __attribute__((destructor)) ldpc_hexagon_profile_destructor(void)
{
  print_phase_profile();
}

int32_t LDPCshutdown(void)
{
  print_phase_profile();

  // Stop dspqueue spinning worker: send STOP packet, wait for pthread to join,
  // then close the ARM-side queue handle (DSP already closed its side).
  if (g_queue) {
    ldpc_q_req_t stop_pkt;
    stop_pkt.msg_type = LDPC_QMSG_STOP;
    stop_pkt.BG = stop_pkt.R = stop_pkt.numMaxIter = stop_pkt.pad0 = 0;
    stop_pkt.Z = stop_pkt.numLLR = stop_pkt.pad1 = 0;
    dspqueue_write(g_queue, 0, 0, NULL, sizeof(stop_pkt.msg_type), (const uint8_t *)&stop_pkt, 2000000);
  }
  if (g_worker_running) {
    pthread_join(g_worker_thread, NULL);
    g_worker_running = 0;
  }
  if (g_queue) {
    dspqueue_close(g_queue);
    g_queue = NULL;
  }
  if (g_buffers_mapped) {
    fastrpc_munmap(g_domain, g_llr_fd, NULL, 0);
    fastrpc_munmap(g_domain, g_llrout_fd, NULL, 0);
    g_buffers_mapped = 0;
  }
  g_llr_fd = g_llrout_fd = -1;

  if (dsp_hdl != (remote_handle64)-1) {
    ldpc_hexagon_close(dsp_hdl);
    dsp_hdl = (remote_handle64)-1;
  }
  if (g_rpc_params) {
    rpcmem_free(g_rpc_params);
    g_rpc_params = NULL;
  }
  if (g_rpc_llr) {
    rpcmem_free(g_rpc_llr);
    g_rpc_llr = NULL;
  }
  if (g_rpc_llrout) {
    rpcmem_free(g_rpc_llrout);
    g_rpc_llrout = NULL;
  }
  if (g_rpc_meta) {
    rpcmem_free(g_rpc_meta);
    g_rpc_meta = NULL;
  }
  if (rpcmem_up) {
    rpcmem_deinit();
    rpcmem_up = 0;
  }
  return 0;
}

// ---------------------------------------------------------------------------
// LDPCdecoder — offload one code-block decode to the Hexagon cDSP.
//
// p_decParams->check_crc is a function pointer and cannot cross the FastRPC
// boundary. When set, parity is checked via iter count instead (the ARM side
// repeats CRC after the DSP returns; this path is future work).
// ---------------------------------------------------------------------------
/* OAI_LDPC_CRC=0 makes the DSP terminate on the parity check instead of the
 * segment CRC. Both are correct; which one is faster depends on how many
 * iterations the CRC saves against what the test costs, so keep them switchable
 * on one binary rather than one build. Read once -- this is per segment. */
static int ldpc_hex_crc_enabled(void)
{
  static int cached = -1;
  if (cached < 0) {
    const char *e = getenv("OAI_LDPC_CRC");
    cached = (e && e[0] == '0') ? 0 : 1;
  }
  return cached;
}

int32_t LDPCdecoder(t_nrLDPC_dec_params *p_decParams,
                    int8_t *p_llr,
                    uint8_t *p_out,
                    t_nrLDPC_time_stats *p_profiler,
                    decode_abort_t *ab)
{
  // Compute numLLR using the standard init path (also validates BG/Z/R).
  t_nrLDPC_lut lut;
  uint32_t numLLR = nrLDPC_init(p_decParams, &lut);
  if (!numLLR) {
    fprintf(stderr, "ldpc_hexagon: nrLDPC_init returned 0\n");
    return -1;
  }

  {
    static int p_logged = 0;
    if (!p_logged) {
      p_logged = 1;
      fprintf(stderr,
              "ldpc_hexagon: params BG=%u Z=%u R=%u maxIter=%u Kprime=%d outMode=%d"
              " crc_type=%d check_crc=%s numLLR=%u\n",
              (unsigned)p_decParams->BG,
              (unsigned)p_decParams->Z,
              (unsigned)p_decParams->R,
              (unsigned)p_decParams->numMaxIter,
              p_decParams->Kprime,
              (int)p_decParams->outMode,
              p_decParams->crc_type,
              p_decParams->check_crc ? "set" : "NULL",
              numLLR);
    }
  }

  /* Lazy init.
   *
   * LDPCinit() is called explicitly by the LEGACY loader (nrLDPC_load.c, used
   * by ldpctest). Under the TB-level ABI the entry point is nrLDPC_coding_init()
   * -- which comes from ldpc_segment and knows nothing about this backend -- so
   * nothing ever opened the FastRPC session and every decode failed with
   * "pre-allocated buffers not ready". Initialise on first use so the library
   * works under both ABIs. LDPCinit() is itself idempotent (it returns early
   * when the session is already open). */
  if (!g_rpc_params && LDPCinit() != 0) {
    fprintf(stderr, "ldpc_hexagon: lazy LDPCinit failed\n");
    return -1;
  }

  // Use pre-allocated ION-backed rpcmem buffers (avoids per-call alloc/free).
  ldpc_hex_params_t *rpc_params = g_rpc_params;
  int8_t *rpc_llr = g_rpc_llr;
  int8_t *rpc_llrout = g_rpc_llrout;
  uint8_t *rpc_meta = g_rpc_meta;

  int32_t ret = -1;
  if (!rpc_params || !rpc_llr || !rpc_llrout || !rpc_meta) {
    fprintf(stderr, "ldpc_hexagon: pre-allocated buffers not ready (numLLR=%u)\n", numLLR);
    return -1;
  }
  if (numLLR > LDPC_HEX_MAX_LLR) {
    fprintf(stderr, "ldpc_hexagon: numLLR=%u exceeds pre-alloc limit %d\n", numLLR, LDPC_HEX_MAX_LLR);
    return -1;
  }

  rpc_params->BG = p_decParams->BG;
  rpc_params->R = p_decParams->R;
  rpc_params->numMaxIter = p_decParams->numMaxIter;
  rpc_params->diag = 2;
  rpc_params->Z = p_decParams->Z;
  rpc_params->Kprime = (uint16_t)p_decParams->Kprime;
  rpc_params->crc_type = (uint8_t)p_decParams->crc_type;
  memset(rpc_params->pad2, 0, sizeof rpc_params->pad2);
  /* LLR conditioning probe.
   *
   * ldpctest quantises its own LLRs (quantize(sigma/16, ..., qbits)), so its
   * dynamic range is modest. The real receiver's LLRs arrive as int16 and are
   * saturated to int8 by the caller's packs_epi16, which can leave a large
   * fraction pinned at +-127 -- a distribution ldpctest never produces. A
   * min-sum decoder whose internal messages also saturate at int8 degrades on
   * that input in a way the synthetic test cannot show.
   *
   * OAI_LDPC_LLR_SHIFT=n scales the input down by n bits before the DSP sees
   * it; OAI_LDPC_LLR_STATS=1 reports how much of the input is clipped. */
  {
    static int shift = -1, stats = -1;
    if (shift < 0) {
      const char *e = getenv("OAI_LDPC_LLR_SHIFT");
      shift = e ? atoi(e) : 0;
      stats = getenv("OAI_LDPC_LLR_STATS") ? 1 : 0;
    }
    if (stats) {
      static int s_logged = 0;
      if (!s_logged) {
        s_logged = 1;
        unsigned sat = 0, zero = 0;
        long asum = 0;
        for (uint32_t i = 0; i < numLLR; i++) {
          const int v = p_llr[i];
          if (v == 127 || v == -128)
            sat++;
          if (v == 0)
            zero++;
          asum += v < 0 ? -v : v;
        }
        fprintf(stderr,
                "ldpc_hexagon: LLR stats numLLR=%u saturated=%u (%.1f%%)"
                " zero=%u mean|llr|=%.1f shift=%d\n",
                numLLR,
                sat,
                100.0 * sat / numLLR,
                zero,
                (double)asum / numLLR,
                shift);
      }
    }
    if (shift > 0)
      for (uint32_t i = 0; i < numLLR; i++)
        rpc_llr[i] = (int8_t)(p_llr[i] >> shift);
    else
      memcpy(rpc_llr, p_llr, numLLR);
  }

  // On first call only: run diag=0 (passthrough) and diag=1 (scatter/gather)
  // to isolate phase costs from the ARM side using wall-clock time.
  // DSP hardware cycle counters are inaccessible from user mode on this device.
  static int arm_perf_done = 0;
  if (!arm_perf_done) {
    arm_perf_done = 1;
    struct timespec ta, tb;
    uint8_t orig_diag = rpc_params->diag;

    rpc_params->diag = 0; // raw memcpy passthrough
    clock_gettime(CLOCK_MONOTONIC, &ta);
    ldpc_hexagon_decode(dsp_hdl,
                        (uint8_t *)rpc_params,
                        sizeof(*rpc_params),
                        (uint8_t *)rpc_llr,
                        (int)numLLR,
                        (uint8_t *)rpc_llrout,
                        (int)numLLR,
                        rpc_meta,
                        LDPC_HEX_META_LEN);
    clock_gettime(CLOCK_MONOTONIC, &tb);
    uint64_t rpc_us = ((uint64_t)(tb.tv_sec - ta.tv_sec) * 1000000ULL + (tb.tv_nsec - ta.tv_nsec) / 1000);
    // Verify diag=0: output must equal input exactly.
    {
      uint32_t mismatches = 0;
      for (uint32_t i = 0; i < numLLR; i++)
        if (rpc_llrout[i] != rpc_llr[i])
          mismatches++;
      if (mismatches)
        fprintf(stderr, "  diag=0 MISMATCH: %u/%u bytes differ (data transfer broken!)\n", mismatches, numLLR);
      else
        fprintf(stderr, "  diag=0 OK: output == input (%u bytes match)\n", numLLR);
    }

    rpc_params->diag = 1; // scatter + gather roundtrip
    clock_gettime(CLOCK_MONOTONIC, &ta);
    ldpc_hexagon_decode(dsp_hdl,
                        (uint8_t *)rpc_params,
                        sizeof(*rpc_params),
                        (uint8_t *)rpc_llr,
                        (int)numLLR,
                        (uint8_t *)rpc_llrout,
                        (int)numLLR,
                        rpc_meta,
                        LDPC_HEX_META_LEN);
    clock_gettime(CLOCK_MONOTONIC, &tb);
    uint64_t sg_us = ((uint64_t)(tb.tv_sec - ta.tv_sec) * 1000000ULL + (tb.tv_nsec - ta.tv_nsec) / 1000);
    // Verify diag=1: scatter+gather should be identity transformation.
    {
      uint32_t mismatches = 0;
      for (uint32_t i = 0; i < numLLR; i++)
        if (rpc_llrout[i] != rpc_llr[i])
          mismatches++;
      if (mismatches)
        fprintf(stderr, "  diag=1 MISMATCH: %u/%u bytes differ (scatter/gather not identity!)\n", mismatches, numLLR);
      else
        fprintf(stderr, "  diag=1 OK: scatter+gather is identity\n");
    }

    // diag=4: llr2CnProcBuf + LUT header.
    // llr_out[0..63] = 16 × uint32_t diagnostic header from DSP.
    // llr_out[64..]  = cnProcBuf[0..numLLR-65].
    rpc_params->diag = 4;
    ldpc_hexagon_decode(dsp_hdl,
                        (uint8_t *)rpc_params,
                        sizeof(*rpc_params),
                        (uint8_t *)rpc_llr,
                        (int)numLLR,
                        (uint8_t *)rpc_llrout,
                        (int)numLLR,
                        rpc_meta,
                        LDPC_HEX_META_LEN);
    {
      uint32_t hdr[16];
      memcpy(hdr, rpc_llrout, 64);
      fprintf(stderr, "  diag=4 LUT header from DSP:\n");
      fprintf(stderr, "    circShift[0].d=0x%08x  .d[0]=%u  dim1=%u  dim2=%u\n", hdr[0], hdr[1], hdr[2], hdr[3]);
      fprintf(stderr, "    numCnInCn[0]=%u  startAddrCn[0]=%u\n", hdr[4], hdr[5]);
      fprintf(stderr, "    numBnInBn[0]=%u  startAddrBn[0]=%u\n", hdr[6], hdr[7]);
      fprintf(stderr, "    cnProcBuf[0] before scatter=%u  after scatter=%u %u\n", hdr[8], hdr[12], hdr[13]);
      fprintf(stderr, "    cnProcBuf[384]=%u  cnProcBuf[768]=%u\n", hdr[14], hdr[15]);
      // Count pos/neg in the body (bytes 64..)
      int llr_pos = 0, llr_neg = 0, llr_zer = 0;
      int cn_pos = 0, cn_neg = 0, cn_zer = 0;
      for (uint32_t i = 0; i < numLLR; i++) {
        int8_t v = rpc_llr[i];
        if (v > 0)
          llr_pos++;
        else if (v < 0)
          llr_neg++;
        else
          llr_zer++;
      }
      for (uint32_t i = 64; i < numLLR; i++) {
        int8_t v = rpc_llrout[i];
        if (v > 0)
          cn_pos++;
        else if (v < 0)
          cn_neg++;
        else
          cn_zer++;
      }
      fprintf(stderr,
              "    llr(pos=%d neg=%d zer=%d) cnBuf[64..](pos=%d neg=%d zer=%d)\n",
              llr_pos,
              llr_neg,
              llr_zer,
              cn_pos,
              cn_neg,
              cn_zer);
      // expected: llr[332] for cnBuf[0] with cshift=332
      fprintf(stderr, "    llr[332]=%d (expected at cnBuf[0])\n", (int)(int8_t)rpc_llr[332]);
    }

    // diag=3: full BP but return raw llrRes (before gather) to check if posteriors are sane.
    rpc_params->diag = 3;
    ldpc_hexagon_decode(dsp_hdl,
                        (uint8_t *)rpc_params,
                        sizeof(*rpc_params),
                        (uint8_t *)rpc_llr,
                        (int)numLLR,
                        (uint8_t *)rpc_llrout,
                        (int)numLLR,
                        rpc_meta,
                        LDPC_HEX_META_LEN);
    {
      int pos = 0, neg = 0, zer = 0;
      for (uint32_t i = 0; i < numLLR; i++) {
        int8_t v = rpc_llrout[i];
        if (v > 0)
          pos++;
        else if (v < 0)
          neg++;
        else
          zer++;
      }
      fprintf(stderr,
              "  diag=3 raw llrRes: pos=%d neg=%d zero=%d [0..7]: %d %d %d %d %d %d %d %d\n",
              pos,
              neg,
              zer,
              (int)(int8_t)rpc_llrout[0],
              (int)(int8_t)rpc_llrout[1],
              (int)(int8_t)rpc_llrout[2],
              (int)(int8_t)rpc_llrout[3],
              (int)(int8_t)rpc_llrout[4],
              (int)(int8_t)rpc_llrout[5],
              (int)(int8_t)rpc_llrout[6],
              (int)(int8_t)rpc_llrout[7]);
    }

    fprintf(stderr, "DSP phase breakdown (ARM wall-clock):\n");
    fprintf(stderr, "  RPC overhead (diag=0):  %6llu us\n", (unsigned long long)rpc_us);
    fprintf(stderr,
            "  scatter+gather (diag=1):%6llu us  compute=%llu us\n",
            (unsigned long long)sg_us,
            (unsigned long long)(sg_us > rpc_us ? sg_us - rpc_us : 0));
    fprintf(stderr, "  full decode (diag=2):    (see ldpctest Decoding time)\n");

    rpc_params->diag = orig_diag;
  }

  uint32_t numIter = 0;

  if (g_queue) {
    // Fast path: async dspqueue — no per-call IPC round-trip.
    struct dspqueue_buffer bufs[2];
    memset(bufs, 0, sizeof(bufs));
    bufs[0].fd = g_llr_fd;
    bufs[0].size = numLLR;
    bufs[0].flags = DSPQUEUE_BUFFER_FLAG_REF | DSPQUEUE_BUFFER_FLAG_FLUSH_SENDER | DSPQUEUE_BUFFER_FLAG_INVALIDATE_RECIPIENT;
    bufs[1].fd = g_llrout_fd;
    bufs[1].size = numLLR;
    bufs[1].flags = DSPQUEUE_BUFFER_FLAG_REF | DSPQUEUE_BUFFER_FLAG_FLUSH_SENDER;

    ldpc_q_req_t req;
    req.msg_type = LDPC_QMSG_DECODE;
    req.BG = rpc_params->BG;
    req.R = rpc_params->R;
    req.numMaxIter = rpc_params->numMaxIter;
    req.pad0 = 0;
    req.Z = rpc_params->Z;
    req.numLLR = (uint16_t)numLLR;
    /* Only offer CRC termination when the caller actually gave us a CRC to
     * check; ldpctest passes check_crc = NULL. OAI_LDPC_CRC=0 forces the
     * parity-check termination instead, so the two can be compared on one
     * binary and one channel realisation. */
    req.Kprime = (p_decParams->check_crc && ldpc_hex_crc_enabled()) ? (uint16_t)p_decParams->Kprime : 0;
    req.crc_type = (uint8_t)p_decParams->crc_type;
    req.pad1 = 0;

    // write_noblock: zero-syscall when DSP worker is spinning (awake flag set).
    int qerr = dspqueue_write_noblock(g_queue, 0, 2, bufs, sizeof(req), (const uint8_t *)&req);
    if (qerr) {
      fprintf(stderr, "ldpc_hexagon: dspqueue_write_noblock failed: 0x%08x\n", (unsigned)qerr);
      return -1;
    }

    /* Busy-poll for the response by default -- the note here was that blocking
     * costs ~350 us against ~5 us polled, and at ~30 segments per slot that
     * would be 10.5 ms of wake latency.
     *
     * The cost of polling is that the ARM core is occupied for the DSP's
     * ENTIRE compute (11,250 us/slot measured), so the offload frees no ARM
     * time at all -- which on a DU is the whole point, since those cores are
     * wanted for TX. OAI_LDPC_SLEEP=1 blocks instead, to measure what the
     * wake actually costs on this platform now that the DSP worker spins. */
    ldpc_q_resp_t resp;
    uint32_t resp_flags, resp_len, resp_nbufs;
    struct dspqueue_buffer resp_bufs[2];
    static int sleep_wait = -1;
    if (sleep_wait < 0) {
      const char *e = getenv("OAI_LDPC_SLEEP");
      sleep_wait = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    if (sleep_wait) {
      qerr = dspqueue_read(g_queue,
                           &resp_flags,
                           2,
                           &resp_nbufs,
                           resp_bufs,
                           sizeof(resp),
                           &resp_len,
                           (uint8_t *)&resp,
                           1000000 /* us */);
    } else {
      for (;;) {
        qerr = dspqueue_read_noblock(g_queue, &resp_flags, 2, &resp_nbufs, resp_bufs, sizeof(resp), &resp_len, (uint8_t *)&resp);
        if (qerr != AEE_EWOULDBLOCK)
          break;
      }
    }
    if (qerr || resp_len < sizeof(resp)) {
      fprintf(stderr, "ldpc_hexagon: dspqueue_read_noblock failed: 0x%08x\n", (unsigned)qerr);
      return -1;
    }

    numIter = resp.numIter;
    if (getenv("OAI_LDPC_BATCH_SHADOW")) {
      static int nprint = 0;
      if (nprint < 6) {
        nprint++;
        fprintf(stderr,
                "ldpc PERSEG stage hashes: scatter=%016llx cnProc=%016llx bnProc=%016llx\n",
                (unsigned long long)resp.dbg_hash[0],
                (unsigned long long)resp.dbg_hash[1],
                (unsigned long long)resp.dbg_hash[2]);
      }
    }
    ret = (int32_t)numIter;
    for (int i = 0; i < 8; i++)
      g_phase_us_total[i] += resp.phase_us[i];
    g_decode_count++;

    static int dsp_clk_printed = 0;
    if (!dsp_clk_printed) {
      dsp_clk_printed = 1;
      fprintf(stderr,
              "ldpc_hexagon: DSP core clock = %u Hz (%u MHz) [via dspqueue], skel v%u\n",
              resp.dsp_clk_hz,
              resp.dsp_clk_hz / 1000000u,
              resp.skel_version);
    }
  } else {
    // Fallback: synchronous FastRPC (used when dspqueue init failed).
    int err = ldpc_hexagon_decode(dsp_hdl,
                                  (uint8_t *)rpc_params,
                                  sizeof(*rpc_params),
                                  (uint8_t *)rpc_llr,
                                  (int)numLLR,
                                  (uint8_t *)rpc_llrout,
                                  (int)numLLR,
                                  rpc_meta,
                                  LDPC_HEX_META_LEN);
    if (err) {
      fprintf(stderr, "ldpc_hexagon_decode RPC call failed: %d\n", err);
      return -1;
    }
    memcpy(&numIter, rpc_meta, 4);
    ret = (int32_t)numIter;
    {
      uint32_t ph[8];
      memcpy(ph, rpc_meta + 8, 32);
      for (int i = 0; i < 8; i++)
        g_phase_us_total[i] += ph[i];
      g_decode_count++;
    }
    static int dsp_clk_printed_fb = 0;
    if (!dsp_clk_printed_fb) {
      dsp_clk_printed_fb = 1;
      uint32_t clk_hz = 0;
      memcpy(&clk_hz, rpc_meta + 4, 4);
      fprintf(stderr, "ldpc_hexagon: DSP core clock = %u Hz (%u MHz)\n", clk_hz, clk_hz / 1000000u);
    }
  }

  // One-shot diagnostic: count pos/neg in first decode to check DSP output quality.
  static int arm_diag_done = 0;
  if (!arm_diag_done) {
    arm_diag_done = 1;
    int pos = 0, neg = 0, zer = 0;
    for (uint32_t i = 0; i < numLLR; i++) {
      int8_t v = rpc_llrout[i];
      if (v > 0)
        pos++;
      else if (v < 0)
        neg++;
      else
        zer++;
    }
    fprintf(stderr,
            "ldpc_hexagon diag: numIter=%u numLLR=%u pos=%d neg=%d zero=%d llrout[0..7]: %d %d %d %d %d %d %d %d\n",
            numIter,
            numLLR,
            pos,
            neg,
            zer,
            (int)rpc_llrout[0],
            (int)rpc_llrout[1],
            (int)rpc_llrout[2],
            (int)rpc_llrout[3],
            (int)rpc_llrout[4],
            (int)rpc_llrout[5],
            (int)rpc_llrout[6],
            (int)rpc_llrout[7]);
  }

  /* NOTE: the abort decision used to be made HERE, on the raw DSP iteration
   * count and before p_out even exists. set_abort() sets ab->failed, which is
   * TB-WIDE -- OAI checks it to abandon the remaining code blocks of the
   * transport block. So a DSP that merely used its last iteration poisoned
   * every other segment, which is why all 30 segments could decode
   * bit-identically to the reference and the TB still failed. It is decided
   * below instead, from the CRC, once the output exists. */

  // Convert posterior LLRs to the requested output format.
  switch (p_decParams->outMode) {
    case nrLDPC_outMode_BIT:
      arm_llr2bitPacked(p_out, rpc_llrout, numLLR);
      break;
    case nrLDPC_outMode_BITINT8:
      arm_llr2bit(p_out, rpc_llrout, numLLR);
      break;
    case nrLDPC_outMode_LLRINT8:
    default:
      memcpy(p_out, rpc_llrout, numLLR);
      break;
  }

  /* SHADOW COMPARISON (OAI_LDPC_SHADOW=1).
   *
   * The skel is a port of nrLDPC_decoder: same min-sum, same int8 messages,
   * same LUTs. If the arithmetic really matches, the SAME int8 LLR input must
   * give the SAME output bits. This runs the reference decoder from libldpc.so
   * on the identical input and diffs it, using real receiver LLRs rather than
   * ldpctest's synthetic ones -- which is the only place the 2.4 dB shows. */
  if (getenv("OAI_LDPC_SHADOW")) {
    typedef int32_t (*ref_dec_t)(t_nrLDPC_dec_params *, int8_t *, uint8_t *, t_nrLDPC_time_stats *, decode_abort_t *);
    static ref_dec_t ref = NULL;
    static int tried = 0;
    if (!tried) {
      tried = 1;
      void *h = dlopen("libldpc.so", RTLD_NOW | RTLD_LOCAL);
      if (h)
        ref = (ref_dec_t)dlsym(h, "LDPCdecoder");
      fprintf(stderr, "ldpc_hexagon: shadow reference %s\n", ref ? "loaded" : "NOT FOUND");
    }
    if (ref) {
      static uint8_t ref_out[NR_LDPC_MAX_NUM_LLR] __attribute__((aligned(64)));
      t_nrLDPC_time_stats ts;
      memset(&ts, 0, sizeof ts);
      decode_abort_t abort_ref;
      memset(&abort_ref, 0, sizeof abort_ref);
      const int ref_iter = ref(p_decParams, (int8_t *)p_llr, ref_out, &ts, &abort_ref);
      /* Compare the FULL K>>3 bytes the caller copies, not just Kprime:
       * memcpy(rdata->c, llrProcBuf, K >> 3) takes the filler region too. */
      const uint32_t kbytes = (uint32_t)(22u * p_decParams->Z) / 8u;
      const uint32_t nbytes = kbytes;
      uint32_t bad = 0, first = 0;
      for (uint32_t i = 0; i < nbytes; i++)
        if (((const uint8_t *)p_out)[i] != ref_out[i]) {
          if (!bad)
            first = i;
          bad++;
        }
      static unsigned n = 0, n_diff = 0;
      n++;
      if (bad)
        n_diff++;
      if (n <= 6 || (n % 30) == 0)
        fprintf(stderr,
                "ldpc_hexagon: shadow seg %u: %u/%u bytes differ (first %u),"
                " iters dsp=%d ref=%d; %u/%u segments differ so far\n",
                n,
                bad,
                nbytes,
                first,
                (int)numIter,
                ref_iter,
                n_diff,
                n);
    }
  }

  /* Decide success by CRC, here on ARM.
   *
   * OAI's segment decoder reads the RETURN VALUE as the success signal:
   * "iterations == numMaxIter" means failure and it zeroes the transport block
   * (nrLDPC_coding_segment_decoder.c). The reference decoder satisfies that by
   * breaking out of its iteration loop as soon as check_crc() passes, so it
   * returns a count below the maximum.
   *
   * This backend could not do that. check_crc is a function pointer and cannot
   * cross FastRPC, so the DSP terminates on its own parity check instead, which
   * is satisfied strictly later than the CRC -- late enough that it routinely
   * used the last iteration and returned numMaxIter. OAI then threw away a
   * CORRECT codeword: verified by forcing the return below the maximum, at
   * which point nr_ulsim went from n_errors 3/3 to 0/5 with the same bits.
   *
   * So apply the reference's own criterion on this side of the boundary, where
   * check_crc is callable. Costs one CRC over Kprime bits per segment, a few
   * microseconds against a ~300 us decode, and it also restores real failure
   * detection: a genuinely undecodable block now reports numMaxIter. */
  if (p_decParams->check_crc) {
    const int crc_ok = p_decParams->check_crc((uint8_t *)p_out, p_decParams->Kprime, p_decParams->crc_type);
    /* OAI_LDPC_CRCSTATS=1 prints a running tally of segment CRC results and an
     * iteration histogram -- how the convergence-signal bug was confirmed. */
    if (getenv("OAI_LDPC_CRCSTATS")) {
      static unsigned n_ok = 0, n_bad = 0, hist[16] = {0};
      if (crc_ok)
        n_ok++;
      else
        n_bad++;
      if (numIter < 16)
        hist[numIter]++;
      if ((n_ok + n_bad) % 30 == 0)
        fprintf(stderr,
                "ldpc_hexagon: %u segments: crc ok %u bad %u; iters 1..5 ="
                " %u %u %u %u %u\n",
                n_ok + n_bad,
                n_ok,
                n_bad,
                hist[1],
                hist[2],
                hist[3],
                hist[4],
                hist[5]);
    }
    if (crc_ok) {
      if (ret >= (int32_t)p_decParams->numMaxIter)
        ret = (int32_t)p_decParams->numMaxIter - 1;
    } else {
      ret = (int32_t)p_decParams->numMaxIter;
      set_abort(ab, true);
    }
  } else if (ret >= (int32_t)p_decParams->numMaxIter) {
    /* No CRC available (the legacy ldpctest path): fall back to the
     * iteration-count criterion, as before. */
    set_abort(ab, true);
  }

  return ret;
}

// ---------------------------------------------------------------------------
// TB-level batch decode — one dispatch, one wait, all code blocks.
// See ldpc_hexagon_batch.h for why this exists.
// ---------------------------------------------------------------------------

void ldpc_hex_batch_caps(uint32_t *max_cb, uint32_t *max_llr_stride)
{
  if (max_cb)
    *max_cb = g_batch_mapped ? LDPC_HEX_BATCH_CB : 0u;
  if (max_llr_stride)
    *max_llr_stride = LDPC_HEX_BATCH_LLR_STR;
}

int ldpc_hex_batch_bufs(int8_t **llr, uint32_t *llr_stride, uint8_t **out, uint32_t *out_stride)
{
  if (!g_batch_mapped)
    return -1;
  if (llr)
    *llr = g_batch_llr;
  if (llr_stride)
    *llr_stride = LDPC_HEX_BATCH_LLR_STR;
  if (out)
    *out = g_batch_out;
  if (out_stride)
    *out_stride = LDPC_HEX_BATCH_OUT_STR;
  return 0;
}

int ldpc_hex_batch_run(const ldpc_hex_batch_t *b, uint8_t *iters)
{
  if (!g_batch_mapped || !g_queue || !b || !iters)
    return -1;
  if (b->C == 0 || b->C > LDPC_HEX_BATCH_CB)
    return -1;

  /* The DSP writes C entries; clear only those, not all 144. */
  for (int d = 0; d < g_ndev; d++)
    memset(dev_meta(d), 0, b->C);

  struct dspqueue_buffer bufs[3];
  memset(bufs, 0, sizeof bufs);
  /* Inputs: flush our writes, and INVALIDATE the DSP's stale lines from the
   * previous TB. Leaving the invalidate off is exactly how the inner-RX
   * offload came to read a mix of stale cache and fresh DDR, which cost days:
   * the first dispatch after init was always right and every later one was
   * subtly wrong, in a way that moved with timing. */
  bufs[0].fd = g_batch_llr_fd;
  bufs[0].ptr = g_batch_llr;
  /* prep-on-DSP sends the TB codeword (int16) once; otherwise C prepped
   * int8 blocks at llr_stride each. */
  bufs[0].size = b->prep_on_dsp ? (uint32_t)b->G * (uint32_t)sizeof(int16_t) : (uint32_t)b->C * LDPC_HEX_BATCH_LLR_STR;
  bufs[0].flags = DSPQUEUE_BUFFER_FLAG_FLUSH_SENDER | DSPQUEUE_BUFFER_FLAG_INVALIDATE_RECIPIENT;
  /* Outputs: no maintenance on the way in, the DSP overwrites them and its
   * response carries the flush/invalidate. */
  bufs[1].fd = g_batch_out_fd;
  bufs[1].ptr = g_batch_out;
  bufs[1].size = (uint32_t)b->C * LDPC_HEX_BATCH_OUT_STR;
  /* INVALIDATE on the way in, which a single device did not need.
   *
   * Each device now writes and flushes only ITS segments. C varies with TB size, so the
   * split point MOVES between transport blocks -- and a device that wrote segment k in an
   * earlier batch can still hold that line dirty after k has passed to the other device.
   * Evicted later, it writes back over the other device's result. That showed up as
   * intermittent CRC failures with retransmissions that never recovered, because the
   * corruption simply recurred.
   *
   * Invalidating drops those stale lines before the DSP writes. Discarding them is
   * correct: nothing in the output buffer is live input. */
  bufs[1].flags = DSPQUEUE_BUFFER_FLAG_INVALIDATE_RECIPIENT;
  /* meta is filled in per device below -- it is the one buffer that is NOT shared */
  bufs[2].size = (uint32_t)sizeof(ldpc_q_batch_meta_t);
  bufs[2].flags = 0;

  ldpc_q_batch_req_t req;
  memset(&req, 0, sizeof req);
  req.msg_type = LDPC_QMSG_BATCH;
  req.BG = b->BG;
  req.R = b->R;
  req.R2 = b->R2;
  req.numMaxIter = b->numMaxIter;
  req.Z = b->Z;
  req.C = b->C;
  req.first_rE2 = b->first_rE2;
  req.Kprime = b->Kprime;
  req.K = b->K;
  req.crc_type = b->crc_type;
  /* OAI_LDPC_WORKERS: code blocks decoded concurrently on the DSP. Batching is
   * what made this possible at all -- per-segment dispatch serialised on the
   * ARM's wait, so the DSP could never hold more than one block in flight.
   *
   * Defaults to LDPC_MAX_WORKERS, not 1. One worker is not a conservative choice, it is
   * the worst one: at 273 PRB / 1 layer / MCS 22 it decodes in 5685 us against the A78
   * CPU's 4000, so an unset environment made the offload look like a regression. Four
   * gives 3051 us, and 2.88x on the DSP wall (18 CB x 286 us serial in ~1840 us). The
   * cap is the DSP's g_workers[]/VTCM/claims sizing, so raising it means changing
   * LDPC_MAX_WORKERS on both sides, not just here. */
  {
    static int nw = -1;
    if (nw < 0) {
      const char *e = getenv("OAI_LDPC_WORKERS");
      nw = e ? atoi(e) : LDPC_HEX_MAX_WORKERS;
      if (nw < 1)
        nw = 1;
      if (nw > LDPC_HEX_MAX_WORKERS)
        nw = LDPC_HEX_MAX_WORKERS;
    }
    req.nworkers = (uint8_t)nw;
  }
  req.llr_stride = LDPC_HEX_BATCH_LLR_STR;
  req.out_stride = LDPC_HEX_BATCH_OUT_STR;
  req.prep_on_dsp = b->prep_on_dsp;
  req.Qm = b->Qm;
  req.rv_index = b->rv_index;
  req.d_clear = b->d_clear;
  req.E = b->E;
  req.E2 = b->E2;
  req.F = b->F;
  req.tbslbrm = b->tbslbrm;
  req.harq_pid = b->harq_pid;
  req.G = b->G;

  /* Split the TB across the devices we have. The split point is a pure function of C,
   * which matters for more than tidiness: `d`, the soft-combining buffer HARQ
   * accumulates across rounds, lives on the DSP that decoded the segment, so a
   * retransmission MUST land on the same device or it combines against an empty buffer.
   * No dynamic balancing across devices for that reason -- the pull queue still balances
   * within each. */
  uint16_t seg0[LDPC_NDEV_MAX], segn[LDPC_NDEV_MAX];
  if (g_ndev == 2 && b->C >= 2) {
    seg0[0] = 0;
    segn[0] = (uint16_t)((b->C + 1) / 2);
    seg0[1] = segn[0];
    segn[1] = (uint16_t)(b->C - segn[0]);
  } else {
    seg0[0] = 0;
    segn[0] = b->C;
    seg0[1] = 0;
    segn[1] = 0;
  }
  const int ndev = (segn[1] > 0) ? 2 : 1;

  struct timespec t_w0, t_w1, t_r1, t_p1;
  clock_gettime(CLOCK_MONOTONIC, &t_w0);
  /* Post to BOTH devices before reading either: the writes are non-blocking, so the two
   * DSPs run concurrently and the wait below is max(A,B) rather than A+B. */
  for (int d = 0; d < ndev; d++) {
    req.seg_first = seg0[d];
    req.seg_count = segn[d];
    bufs[2].fd = dev_meta_fd(d);
    bufs[2].ptr = dev_meta(d);
    int werr = dspqueue_write_noblock(dev_queue(d), 0, 3, bufs, sizeof req, (const uint8_t *)&req);
    if (werr) {
      fprintf(stderr, "ldpc_hexagon: batch write_noblock dev%d failed: 0x%08x\n", d, (unsigned)werr);
      return -1;
    }
  }
  clock_gettime(CLOCK_MONOTONIC, &t_w1);
  int qerr = 0;

  /* SLEEP, do not spin. This is the entire reason for batching: one wake of
   * ~326 us against the DSP's whole compute (11,250 us/slot at 273 PRB), so
   * the ARM core is released instead of held. Per segment the same wake cost
   * 30x as much, which is why that path polls. */
  /* Read with generous limits and validate afterwards: dspqueue_read returns
   * AEE_EBADPARM ("packet too large to fit") without telling you what the real
   * sizes were, so being tight here turns a size mismatch into a mystery. */
  union {
    ldpc_q_batch_resp_t r;
    uint8_t raw[256];
  } rmsg;
  memset(&rmsg, 0, sizeof rmsg);
  ldpc_q_batch_resp_t resp;
  uint32_t rflags = 0, rlen = 0, rnbufs = 0;
  struct dspqueue_buffer rbufs[8];
  /* OAI_LDPC_SLEEP=0 polls instead of blocking, to separate wake latency from real DSP
   * time: the instrumented wait is ~2980 us against a DSP wall of ~1835, so ~1150 us is
   * turnaround rather than compute, and only measuring both tells us how much of that a
   * wake costs.  Sleeping stays the DEFAULT -- polling holds the ARM core for the DSP's
   * entire compute, which defeats the point of offloading on a DU. */
  static int batch_sleep = -1;
  if (batch_sleep < 0) {
    const char *e = getenv("OAI_LDPC_SLEEP");
    batch_sleep = (e && e[0] == '0') ? 0 : 1;
  }
  /* Read every device. Sequential reads, but they were both posted above and are
   * running concurrently, so this costs max(A,B). */
  ldpc_q_batch_resp_t resp_d[LDPC_NDEV_MAX];
  memset(resp_d, 0, sizeof resp_d);
  for (int d = 0; d < ndev; d++) {
    memset(&rmsg, 0, sizeof rmsg);
    rflags = rlen = rnbufs = 0;
    if (batch_sleep) {
      qerr = dspqueue_read(dev_queue(d),
                           &rflags,
                           8,
                           &rnbufs,
                           rbufs,
                           sizeof rmsg,
                           &rlen,
                           (uint8_t *)&rmsg,
                           1000000 /* us; bounded so a dead PD is not fatal */);
    } else {
      for (;;) {
        qerr = dspqueue_read_noblock(dev_queue(d), &rflags, 8, &rnbufs, rbufs, sizeof rmsg, &rlen, (uint8_t *)&rmsg);
        if (qerr != AEE_EWOULDBLOCK)
          break;
      }
    }
    static int shape_logged = 0;
    if (!shape_logged) {
      shape_logged = 1;
      fprintf(stderr,
              "ldpc_hexagon: batch response shape err=0x%x nbufs=%u msg_len=%u"
              " (expected <=3 bufs, %u bytes)\n",
              (unsigned)qerr,
              (unsigned)rnbufs,
              (unsigned)rlen,
              (unsigned)sizeof(ldpc_q_batch_resp_t));
    }
    resp_d[d] = rmsg.r;
    if (getenv("OAI_LDPC_BATCH_SHADOW")) {
      static int nprint = 0;
      if (nprint < 6) {
        nprint++;
        fprintf(stderr,
                "ldpc BATCH stage hashes: scatter=%016llx cnProc=%016llx bnProc=%016llx\n",
                (unsigned long long)rmsg.r.dbg_hash[0],
                (unsigned long long)rmsg.r.dbg_hash[1],
                (unsigned long long)rmsg.r.dbg_hash[2]);
      }
    }
    if (qerr) {
      fprintf(stderr, "ldpc_hexagon: batch dspqueue_read dev%d failed: 0x%08x\n", d, (unsigned)qerr);
      return -1;
    }
    if (rlen < sizeof(ldpc_q_batch_resp_t) || resp_d[d].msg_type != LDPC_QMSG_BRESULT || resp_d[d].status != 0) {
      fprintf(stderr,
              "ldpc_hexagon: batch response dev%d bad (len %u type %u status %u)\n",
              d,
              (unsigned)rlen,
              (unsigned)resp_d[d].msg_type,
              (unsigned)resp_d[d].status);
      return -1;
    }
    /* Each device reports only the segments IT decoded. */
    if (resp_d[d].C != segn[d]) {
      fprintf(stderr, "ldpc_hexagon: dev%d decoded %u of %u segments\n", d, (unsigned)resp_d[d].C, (unsigned)segn[d]);
      return -1;
    }
  }
  clock_gettime(CLOCK_MONOTONIC, &t_r1);
  /* Report against device 0 for the stats below, but take the batch wall from whichever
   * device finished last -- that, not the average, is what the slot actually waited. */
  resp = resp_d[0];
  for (int d = 1; d < ndev; d++)
    if (resp_d[d].batch_us > resp.batch_us)
      resp.batch_us = resp_d[d].batch_us;

  /* Merge the per-device iteration counts; each device wrote only its own range. */
  for (int d = 0; d < ndev; d++)
    memcpy(iters + seg0[d], dev_meta(d) + seg0[d], segn[d]);
  /* Sum compute across every device, since g_decode_count counts the whole TB.
   * Taking device 0's phases alone would halve the reported us/codeword. */
  for (int d = 0; d < ndev; d++)
    for (int i = 0; i < 8; i++)
      g_phase_us_total[i] += resp_d[d].phase_us[i];
  g_decode_count += b->C;
  g_batch_wall_us_total += resp.batch_us;
  g_batch_count++;

  static int clk_printed = 0;
  if (!clk_printed) {
    clk_printed = 1;
    fprintf(stderr,
            "ldpc_hexagon: batch path active, DSP clock %u MHz, skel v%u,"
            " decoding with %u worker(s), %u HVX units reported\n",
            resp.dsp_clk_hz / 1000000u,
            resp.skel_version,
            resp.nworkers,
            resp.hvx_units);
    /* vtcm_ok carries the BYTES granted, not a flag. A non-zero acquire context is not
     * the same as getting what was asked for, and a short grant silently overlaps the
     * per-worker slices -- so the number is what matters. No FARF channel reaches the
     * host on this platform (printf forwarding off, FARF(ERROR) absent from /dev/kmsg,
     * no mini_dm), which makes this response field the only working diagnostic path. */
    fprintf(stderr,
            "ldpc_hexagon: VTCM granted %u B (%.2f MB) -- %s (signed PD %s)\n",
            resp.vtcm_ok,
            (double)resp.vtcm_ok / (1024.0 * 1024.0),
            resp.vtcm_ok ? "acquired" : "NOT acquired",
            resp.vtcm_ok ? "ok" : "absent or refused");
    fprintf(stderr,
            "ldpc_hexagon: first batch %u us, blocks claimed %u/%u/%u/%u"
            " (first batch only -- see the averaged figure at shutdown)\n",
            resp.batch_us,
            resp.claims & 0xff,
            (resp.claims >> 8) & 0xff,
            (resp.claims >> 16) & 0xff,
            (resp.claims >> 24) & 0xff);
  }
  clock_gettime(CLOCK_MONOTONIC, &t_p1);
#define DISP_US(a, b) ((uint64_t)((b).tv_sec - (a).tv_sec) * 1000000u + ((b).tv_nsec - (a).tv_nsec) / 1000)
  g_disp_write_us += DISP_US(t_w0, t_w1);
  g_disp_wait_us += DISP_US(t_w1, t_r1);
  g_disp_post_us += DISP_US(t_r1, t_p1);
#undef DISP_US
  for (int i = 0; i < 4; i++)
    for (int d = 0; d < ndev; d++)
      g_claims_total[i] += (resp_d[d].claims >> (8 * i)) & 0xff;
  if (resp.batch_us < g_batch_wall_min_us)
    g_batch_wall_min_us = resp.batch_us;
  if (resp.batch_us > g_batch_wall_max_us)
    g_batch_wall_max_us = resp.batch_us;
  return 0;
}

int ldpc_hex_batch_cw(int16_t **cw, uint32_t *max_g)
{
  if (!g_batch_mapped)
    return -1;
  /* The codeword shares the input allocation with the prepped-block layout;
   * only one of the two is in use per dispatch. */
  if (cw)
    *cw = (int16_t *)g_batch_llr;
  if (max_g)
    *max_g = (LDPC_HEX_BATCH_CB * LDPC_HEX_BATCH_LLR_STR) / (uint32_t)sizeof(int16_t);
  return 0;
}
