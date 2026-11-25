/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * nrLDPC_coding_hexagon.c — TB-level coding interface for the Hexagon offload.
 *
 * Replaces nrLDPC_coding_segment's decoder inside libldpc_hexagon.so. The
 * difference that matters is ONE DSP dispatch per transport block instead of one
 * per code block.
 *
 * Why. The per-segment path had to busy-poll for each response: blocking costs
 * ~326 us on this platform (measured -- 30 segments went from 12,780 to 22,550
 * us/slot when sleeping per segment), so 30 sleeps is 9.8 ms of pure wake
 * latency. But polling holds an ARM core for the DSP's ENTIRE compute, 11,250
 * us/slot at 273 PRB, so the offload freed no ARM time whatsoever -- and on a DU
 * that is the whole point, because those cores are wanted for TX. Measured
 * four-way: offloading LDPC took ARM-busy time from 13,180 to 18,292 us/slot,
 * i.e. it made the thing it was supposed to help strictly worse.
 *
 * One dispatch means one ~326 us wake against 11,250 us of compute, so the ARM
 * sleeps through the decode. It is also the prerequisite for decoding segments
 * concurrently across HVX contexts: per-segment, the ARM serialises on every
 * wait, so the DSP can never hold more than one code block in flight.
 *
 * What still runs here on the ARM: deinterleaving, rate dematching (~970 us/slot
 * combined) and the int16->int8 saturate. Those are the next thing to move
 * across, at which point `d` -- the HARQ soft-combining buffer, which must
 * survive between rounds -- has to become DSP-visible memory owned per HARQ
 * process. That is an allocation-ownership change in OAI, not just moving code,
 * which is why it is not in this commit.
 *
 * Falls back to one dispatch per code block (LDPCdecoder, the same entry point
 * ldpctest uses) when the batch buffers are unavailable or the transport block
 * has more code blocks than they were sized for. Correct but slower, and it
 * holds an ARM core for the whole decode -- which is the thing the batch exists
 * to avoid.
 */

#include <dlfcn.h>
#include "nr_rate_matching.h"
#include "PHY/CODING/coding_extern.h"
#include "PHY/CODING/coding_defs.h"
#include "PHY/CODING/nrLDPC_coding/nrLDPC_coding_interface.h"
#include "PHY/CODING/nrLDPC_extern.h"
#include "PHY/CODING/nrLDPC_decoder/nrLDPC_types.h"
#include "common/utils/LOG/log.h"
#include "ldpc_hexagon_batch.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <simde/x86/sse4.1.h>

int32_t LDPCinit(void);
int32_t LDPCshutdown(void);

/* Deinterleave + rate-dematch + saturate one code block, writing the decoder's
 * int8 input DIRECTLY into the shared batch slot.
 *
 * The reference builds this on the stack and lets the decoder copy it into
 * rpcmem, which at 273 PRB is 783 kB of memcpy per slot for nothing. Returns
 * false if rate dematching fails, in which case the segment is left to be
 * reported as a failure rather than decoded from garbage. */
static bool prep_segment(const nrLDPC_TB_decoding_parameters_t *tb, int r, uint8_t Kc, int8_t *dst)
{
  const int K = tb->K;
  const int Kprime = K - tb->F;
  const int Z = tb->Z;

  int E, llr_offset;
  if (r < tb->first_rE2) {
    E = tb->E;
    llr_offset = r * tb->E;
  } else {
    E = tb->E2;
    llr_offset = tb->first_rE2 * tb->E + (r - tb->first_rE2) * tb->E2;
  }

  int16_t harq_e[E];
  nr_deinterleaving_ldpc(E, tb->Qm, harq_e, tb->llr + llr_offset);

  int16_t *d = tb->d + r * Kc * Z;
  if (nr_rate_matching_ldpc_rx(tb->tbslbrm,
                               tb->BG,
                               Z,
                               d,
                               harq_e,
                               tb->C,
                               tb->rv_index,
                               tb->d_to_be_cleared,
                               E,
                               tb->F,
                               K - tb->F - 2 * Z)
      == -1) {
    LOG_E(PHY,
          "hexagon batch: rate matching failed, BG %d Z %d C %d rv %d E %d F %d K %d\n",
          tb->BG,
          Z,
          tb->C,
          tb->rv_index,
          E,
          tb->F,
          K);
    return false;
  }

  /* Same assembly as the reference: zero the first 2Z, filler bits to 127, coded
   * bits either side of the filler, then saturate int16 -> int8. */
  int16_t z[68 * 384 + 16] __attribute__((aligned(16)));
  memset(z, 0, 2 * Z * sizeof(*z));
  memset(z + Kprime, 127, tb->F * sizeof(*z));
  memcpy(z + 2 * Z, d, (Kprime - 2 * Z) * sizeof(*z));
  memcpy(z + K, d + (K - 2 * Z), (Kc * Z - K) * sizeof(*z));

  simde__m128i *pv = (simde__m128i *)&z;
  simde__m128i *pl = (simde__m128i *)dst;
  for (int i = 0, j = 0; j < ((Kc * Z) >> 4) + 1; i += 2, j++)
    pl[j] = simde_mm_packs_epi16(pv[i], pv[i + 1]);
  return true;
}

/* Smallest transport block worth sending to the DSP, in CODE BLOCKS.
 *
 * The dispatch costs a fixed round trip whatever the payload, so below some size
 * the ARM simply finishes first. Measured, nr_ulsim, ULSCH segments decoding:
 *
 *     RB    shape        ARM        CDSP       delta
 *      8    Z=160 C=1     57.5 us    356.7 us   +299
 *     20    Z=384 C=1     90.9 us    346.1 us   +255
 *     50    Z=224 C=2    126.2 us    209.1 us    +83
 *    100    Z=288 C=3    466.5 us    383.8 us    -83
 *    200    Z=352 C=5    904.8 us    774.0 us   -131
 *    273    Z=384 C=6   1133.7 us    661.2 us   -472
 *
 * The crossover is between C=2 and C=3, so three code blocks is the gate. It is
 * expressed in code blocks rather than bytes because that is what the batch
 * dispatches and what the cost scales with.
 *
 * This matters beyond the microseconds: Msg3 is C=1, so every random access was
 * paying ~300 us to be decoded MORE SLOWLY than the ARM would have done it, on
 * the one exchange in an attach that has a hard deadline.
 *
 * OAI_LDPC_MIN_C overrides it; 0 disables the gate and sends everything to the
 * DSP, which is how the table above was measured. */
#define LDPC_HEX_MIN_C_DEFAULT 3

static int ldpc_hex_min_c(void)
{
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("OAI_LDPC_MIN_C");
    v = (e && e[0]) ? atoi(e) : LDPC_HEX_MIN_C_DEFAULT;
    if (v < 0)
      v = 0;
  }
  return v;
}

/* The CPU reference decoder, for transport blocks below the gate. Same symbol
 * the shadow comparison uses, from the in-tree libldpc.so. If it cannot be
 * loaded we fall through to the DSP rather than failing the block: slower is
 * better than dropped. */
typedef int32_t (*ldpc_ref_dec_t)(t_nrLDPC_dec_params *, int8_t *, uint8_t *, t_nrLDPC_time_stats *, decode_abort_t *);

static ldpc_ref_dec_t ldpc_hex_cpu_decoder(void)
{
  static ldpc_ref_dec_t ref = NULL;
  static int tried = 0;
  if (!tried) {
    tried = 1;
    void *h = dlopen("libldpc.so", RTLD_NOW | RTLD_LOCAL);
    if (h)
      ref = (ldpc_ref_dec_t)dlsym(h, "LDPCdecoder");
    LOG_W(PHY,
          "hexagon: CPU decoder for small TBs (C < %d) %s\n",
          ldpc_hex_min_c(),
          ref ? "loaded" : "NOT FOUND -- small TBs stay on the DSP");
  }
  return ref;
}

/* `max_num_pxsch` is unused: the batch buffers are shared across PxSCHs and are
 * sized once by LDPCinit(), not per channel. The parameter is part of the
 * nrLDPC_coding_init_t ABI, so it has to be in the signature -- declaring this
 * (void) and letting the loader call it with an argument is what the previous
 * version did. */
int32_t nrLDPC_coding_init(int max_num_pxsch)
{
  (void)max_num_pxsch;
  int32_t rc = LDPCinit();
  /* Resolve the CPU decoder here rather than on first use. It only serves
   * transport blocks below the small-TB gate, but LDPC_HEX_MIN_C_DEFAULT is 3,
   * so ordinary small uplink grants reach it -- and resolving it lazily put a
   * dlopen() inside nrLDPC_coding_decoder(), which runs under the L1 RX
   * deadline. Failing to find it is not fatal: those TBs stay on the DSP. */
  if (rc == 0 && ldpc_hex_min_c() > 0)
    ldpc_hex_cpu_decoder();
  return rc;
}

int32_t nrLDPC_coding_shutdown(void)
{
  return LDPCshutdown();
}

int32_t nrLDPC_coding_decoder(nrLDPC_slot_decoding_parameters_t *slot)
{
  int8_t *batch_llr = NULL;
  uint8_t *batch_out = NULL;
  uint32_t llr_stride = 0, out_stride = 0, max_cb = 0, max_llr = 0;
  bool batch_ok = (ldpc_hex_batch_bufs(&batch_llr, &llr_stride, &batch_out, &out_stride) == 0);
  ldpc_hex_batch_caps(&max_cb, &max_llr);

  /* OAI_LDPC_NOBATCH=1 forces the per-segment path.  Slower by design, but it is
   * the ONLY path carrying the OAI_LDPC_SHADOW comparison against the CPU
   * reference decoder -- with the batch active, setting OAI_LDPC_SHADOW does
   * nothing at all and looks like a clean result.  (OAI_LDPC_BATCH_SHADOW is not
   * a substitute: it compares two DSP paths, so when both are wrong it reports
   * zero differences.)  Use this to answer "is the DSP decoder bit-exact for
   * this codeword shape", then take it back off. */
  {
    static int nobatch = -1;
    if (nobatch < 0) {
      const char *e = getenv("OAI_LDPC_NOBATCH");
      nobatch = (e && e[0] && e[0] != '0') ? 1 : 0;
      if (nobatch)
        LOG_W(PHY,
              "hexagon: OAI_LDPC_NOBATCH set -- per-segment path, slower;"
              " this is what makes OAI_LDPC_SHADOW reachable\n");
    }
    if (nobatch)
      batch_ok = false;
  }

  for (int tb_id = 0; tb_id < slot->nb_TBs; tb_id++) {
    nrLDPC_TB_decoding_parameters_t *tb = &slot->TBs[tb_id];
    const uint8_t Kc = (tb->BG == 2) ? 52 : 68;
    const uint32_t need = (uint32_t)Kc * tb->Z;

    *tb->processedSegments = 0;

    /* Below the gate the ARM finishes first, so use it. Reuses the per-segment
     * loop below, with the CPU decoder swapped in for the DSP one. */
    ldpc_ref_dec_t cpu_dec = NULL;
    if (ldpc_hex_min_c() > 0 && tb->C < ldpc_hex_min_c())
      cpu_dec = ldpc_hex_cpu_decoder();

    if (cpu_dec || !batch_ok || tb->C > max_cb || need > llr_stride) {
      /* ---- FALLBACK ONLY: one dispatch per code block, segments sequential ----
       *
       * This is NOT how the offload normally runs. The default path is the batch
       * below: one dispatch for the whole transport block, after which the DSP
       * decodes its segments concurrently across four HVX worker contexts (see
       * the QuRT pool in ldpc_hexagon_imp.c). We only get here when the batch
       * buffers are unmapped, the TB has more code blocks or longer codewords
       * than they were sized for, or the TB is small enough that the ARM beats
       * the DSP outright (cpu_dec).
       *
       * Slower, and it holds an ARM core for the whole decode -- which is the
       * thing the batch exists to avoid -- but correct, and far better than
       * failing the TB, which is what a transport block larger than the batch
       * buffers used to get. */
      static bool warned = false;
      if (!warned && !cpu_dec) {
        warned = true;
        LOG_W(PHY,
              "hexagon: batch unavailable (mapped %d, C %d vs max %u, need %u vs stride %u)"
              " -- per-segment path for this and any similar TB\n",
              (int)batch_ok,
              tb->C,
              max_cb,
              need,
              llr_stride);
      }
      /* NR_LDPC_MAX_NUM_LLR, not need: the decoder uses its INPUT buffer as scratch
       * and writes well past numLLR. Sizing this to the codeword (need + 64) let it
       * run thousands of bytes off the end -- "malloc(): corrupted top size" AFTER an
       * otherwise clean decode, at Z 16, 26, 28, 32 and 48 but not 12, 14, 22 or 40,
       * which is why it read as a lifting-size bug and not an allocation one. Bisected:
       * +2048 still corrupts, +8192 does not. */
      int8_t *seg = malloc(NR_LDPC_MAX_NUM_LLR + 64);
      /* Same reasoning as seg: the decoder writes its output at a fixed stride
       * rather than K/8, so size this to the maximum codeword too. */
      uint8_t *bits = malloc((NR_LDPC_MAX_NUM_LLR >> 3) + 64);
      if (!seg || !bits) {
        free(seg);
        free(bits);
        LOG_E(PHY, "hexagon: fallback allocation failed, TB reported as failed\n");
        for (int r = 0; r < tb->C; r++) {
          tb->decodeSuccess[r] = false;
          memset(tb->c + r * (tb->K >> 3), 0, tb->K >> 3);
        }
        continue;
      }
      for (int r = 0; r < tb->C; r++) {
        bool ok = prep_segment(tb, r, Kc, seg);
        if (ok) {
          t_nrLDPC_dec_params dp = {.check_crc = check_crc};
          dp.BG = tb->BG;
          dp.Z = tb->Z;
          dp.R = (r < tb->first_rE2) ? tb->R : tb->R2;
          dp.numMaxIter = tb->max_ldpc_iterations;
          dp.outMode = nrLDPC_outMode_BIT;
          dp.Kprime = lenWithCrc(tb->C, tb->A);
          dp.crc_type = crcType(tb->C, tb->A);
          t_nrLDPC_time_stats ts = {0};
          ok = (cpu_dec ? cpu_dec(&dp, seg, bits, &ts, tb->abort_decode) : LDPCdecoder(&dp, seg, bits, &ts, tb->abort_decode))
               < dp.numMaxIter;
        }
        tb->decodeSuccess[r] = ok;
        if (ok) {
          memcpy(tb->c + r * (tb->K >> 3), bits, tb->K >> 3);
          *tb->processedSegments = *tb->processedSegments + 1;
        } else {
          memset(tb->c + r * (tb->K >> 3), 0, tb->K >> 3);
        }
      }
      free(seg);
      free(bits);
      continue;
    }

    /* ---- one dispatch, one sleep ---- */
    ldpc_hex_batch_t b = {
        .BG = tb->BG,
        .R = tb->R,
        .R2 = tb->R2,
        .numMaxIter = tb->max_ldpc_iterations,
        .Z = (uint16_t)tb->Z,
        .C = (uint16_t)tb->C,
        .first_rE2 = (uint16_t)tb->first_rE2,
        .Kprime = (uint16_t)lenWithCrc(tb->C, tb->A),
        .K = (uint16_t)tb->K,
        .crc_type = (uint8_t)crcType(tb->C, tb->A),
    };
    /* ---- prep: on the DSP if we can, else here ----
     *
     * OAI_LDPC_DSPPREP=1 hands the TB's codeword LLRs to the DSP, which then does
     * deinterleave + rate-dematch + saturate itself and keeps `d` -- the HARQ
     * accumulator -- resident, keyed on harq_unique_pid. That takes ARM prep from
     * 1,055 to 63 us/slot (all that is left is one 904 kB copy).
     *
     * DEFAULT ON. Averaged over 200 slots the DSP side costs 250-360 us of DSP
     * wall against 500-1,000 us of ARM prep saved, and measured total RX is lower
     * with it on at both one and two layers.
     *
     * It looked like a large regression at first -- "~6,500 us of DSP time" -- but
     * that came from a batch-wall counter that reported only the FIRST batch: cold
     * caches plus the first-touch allocation and clear of `d`. The averaged figure
     * is what the numbers above come from. OAI_LDPC_DSPPREP=0 reverts to ARM prep. */
    static int dsp_prep = -1;
    if (dsp_prep < 0) {
      const char *e = getenv("OAI_LDPC_DSPPREP");
      dsp_prep = (e && e[0] == '0') ? 0 : 1;
    }
    /* The DSP deinterleaves E int16 per segment into a fixed prep_e buffer, so a
     * block whose E exceeds that capacity has to be prepped here instead. It is
     * not a rare corner: any single-segment low-rate TB can exceed it, and
     * BG2 Z=384 C=1 (E = 31800 vs 27000) overran prep_e and decoded to noise. */
    const bool prep_fits = tb->E <= LDPC_HEX_PREP_E_MAX && tb->E2 <= LDPC_HEX_PREP_E_MAX;
    bool prep_ok[NR_LDPC_MAX_NUM_CB];
    bool prepped_on_dsp = false;

    if (dsp_prep && prep_fits) {
      int16_t *cw = NULL;
      uint32_t max_g = 0;
      if (ldpc_hex_batch_cw(&cw, &max_g) == 0 && tb->G <= max_g) {
        memcpy(cw, tb->llr, (size_t)tb->G * sizeof(int16_t));
        b.prep_on_dsp = 1;
        b.Qm = tb->Qm;
        b.rv_index = tb->rv_index;
        b.d_clear = tb->d_to_be_cleared ? 1 : 0;
        b.E = tb->E;
        b.E2 = tb->E2;
        b.F = tb->F;
        b.tbslbrm = tb->tbslbrm;
        b.harq_pid = tb->harq_unique_pid;
        b.G = tb->G;
        for (int r = 0; r < tb->C; r++)
          prep_ok[r] = true;
        prepped_on_dsp = true;
      }
    }
    if (!prepped_on_dsp) {
      for (int r = 0; r < tb->C; r++)
        prep_ok[r] = prep_segment(tb, r, Kc, batch_llr + (size_t)r * llr_stride);
    }

    uint8_t iters[NR_LDPC_MAX_NUM_CB];
    start_meas(&tb->ts_ldpc_decode);
    const int rc = ldpc_hex_batch_run(&b, iters);
    stop_meas(&tb->ts_ldpc_decode);

    /* ---- OAI_LDPC_BATCH_SHADOW=1: check the batch against the per-segment path
     * ----
     * Both decode the SAME prepped buffer, so a mismatch localises the fault to
     * the batch dispatch or the DSP's batch loop; agreement would put it in prep.
     * The per-segment path is the validated one -- its BLER matches the CPU. */
    {
      static int shadow = -1;
      if (shadow < 0) {
        const char *e = getenv("OAI_LDPC_BATCH_SHADOW");
        shadow = (e && e[0] && e[0] != '0') ? 1 : 0;
      }
      if (shadow && rc == 0) {
        static unsigned long tb_seen = 0;
        unsigned bad_seg = 0, first_bad = 0, first_byte = 0;
        uint8_t ref[68 * 384 / 8 + 64];
        for (int r = 0; r < tb->C; r++) {
          t_nrLDPC_dec_params dp = {.check_crc = check_crc};
          dp.BG = tb->BG;
          dp.Z = tb->Z;
          dp.R = (r < tb->first_rE2) ? tb->R : tb->R2;
          dp.numMaxIter = tb->max_ldpc_iterations;
          dp.outMode = nrLDPC_outMode_BIT;
          dp.Kprime = lenWithCrc(tb->C, tb->A);
          dp.crc_type = crcType(tb->C, tb->A);
          t_nrLDPC_time_stats ts = {0};
          decode_abort_t ab;
          memset(&ab, 0, sizeof ab);
          const int it = LDPCdecoder(&dp, batch_llr + (size_t)r * llr_stride, ref, &ts, &ab);
          const int nb = tb->K >> 3;
          if (memcmp(ref, batch_out + (size_t)r * out_stride, nb) != 0) {
            if (!bad_seg) {
              first_bad = (unsigned)r;
              for (int q = 0; q < nb; q++)
                if (ref[q] != batch_out[(size_t)r * out_stride + q]) {
                  first_byte = (unsigned)q;
                  break;
                }
            }
            bad_seg++;
          }
          (void)it;
        }
        if (tb_seen++ < 4 || bad_seg)
          fprintf(stderr,
                  "ldpc batch shadow: TB C=%d Z=%d BG=%d R=%d/%d first_rE2=%d K=%d Kprime=%d"
                  " crc=%d -> %u/%d segments differ (first seg %u byte %u)\n",
                  tb->C,
                  tb->Z,
                  tb->BG,
                  tb->R,
                  tb->R2,
                  tb->first_rE2,
                  tb->K,
                  lenWithCrc(tb->C, tb->A),
                  crcType(tb->C, tb->A),
                  bad_seg,
                  tb->C,
                  first_bad,
                  first_byte);
        /* TEMP: iters distinguishes "never converged" (== numMaxIter) from
         * "converged to the wrong answer / bad extraction" (< numMaxIter). */
        fprintf(stderr, "ldpc batch shadow: iters[0]=%u of maxIter=%d\n", (unsigned)iters[0], tb->max_ldpc_iterations);
      }
    }

    /* ---- results ---- */
    for (int r = 0; r < tb->C; r++) {
      const bool ok = (rc == 0) && prep_ok[r] && (iters[r] < tb->max_ldpc_iterations);
      tb->decodeSuccess[r] = ok;
      if (ok) {
        memcpy(tb->c + r * (tb->K >> 3), batch_out + (size_t)r * out_stride, tb->K >> 3);
        *tb->processedSegments = *tb->processedSegments + 1;
      } else {
        memset(tb->c + r * (tb->K >> 3), 0, tb->K >> 3);
      }
    }
  }
  return 0;
}
