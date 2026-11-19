/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * Tests the DSP side of the Hexagon LDPC offload under hexagon-sim, with no board.
 *
 * The decoder source is compiled in directly (LDPC_SIM_STANDALONE drops only the
 * QuRT worker pool and the FastRPC entry points), and each code block goes through
 * ldpc_decode_one(): deinterleave, rate dematch with HARQ accumulation, decode with
 * CRC termination, and the hard decision. That is the per-segment path the skel runs
 * in production, on one worker with heap buffers.
 *
 * The transmit side is independent of the code under test: OAI's reference encoder
 * (LDPCencoder, gen_code 0) and a direct transcription of TS 38.212 5.4.2 for bit
 * selection and interleaving. The test passes when every code block decodes to the
 * transmitted bits. Two more groups of checks compare the HVX kernels with their
 * scalar references: scatter/gather/copy, the CRC, and the cnProcPc parity test.
 *
 * Exit status is 0 on success. The cycle counts it prints are the simulator's.
 */

#define LDPC_SIM_STANDALONE 1
#include "../src/ldpc_hexagon_imp.c"
#include "openair1/PHY/CODING/nrLDPC_encoder/ldpc_encoder.c"
#include <hexagon_sim_timer.h>
#include <math.h>

static int g_fail;

#define CHECK(cond, ...)              \
  do {                                \
    if (!(cond)) {                    \
      printf("  FAIL: " __VA_ARGS__); \
      printf("\n");                   \
      g_fail++;                       \
    }                                 \
  } while (0)

/* ---- deterministic noise ------------------------------------------------- */

static uint32_t g_rng = 1;
static uint32_t rng_u32(void)
{
  g_rng = g_rng * 1664525u + 1013904223u;
  return g_rng;
}
static double rng_gauss(void)
{
  double u1 = ((rng_u32() >> 8) + 1.0) / 16777217.0;
  double u2 = (rng_u32() >> 8) / 16777216.0;
  return sqrt(-2.0 * log(u1)) * cos(6.283185307179586 * u2);
}

/* ---- transmit side ---------------------------------------------------------- */

/* nr_get_R_ldpc_decoder() (nr_tbs_tools.c): the decoder's rate index from the
 * furthest position any round has filled in the circular buffer. */
static uint8_t rate_index(int rv, uint32_t E, uint8_t BG, uint16_t Z, uint32_t *llrLen)
{
  const uint32_t N = (BG == 1) ? 66u * Z : 50u * Z;
  uint32_t infoBits = irx_index_k0[BG - 1][rv] * Z + E;
  if (infoBits > N)
    infoBits = N;
  if (infoBits > *llrLen)
    *llrLen = infoBits;
  const double r = (double)((BG == 1) ? 22u * Z : 10u * Z) / (*llrLen + 2u * Z);
  if (BG == 2)
    return r < 0.3333 ? 15 : (r < 0.6667 ? 13 : 23);
  return r < 0.6667 ? 13 : (r < 0.8889 ? 23 : 89);
}

/* TS 38.212 5.4.2.1 bit selection, one bit at a time, skipping filler bits. */
static void tx_rate_match(const uint8_t *d, uint8_t *e, uint32_t E, uint32_t Ncb, uint32_t k0, uint32_t Foffset, uint32_t F)
{
  uint32_t k = 0, j = 0;
  while (k < E) {
    const uint32_t i = (k0 + j++) % Ncb;
    if (i >= Foffset && i < Foffset + F)
      continue;
    e[k++] = d[i];
  }
}

/* TS 38.212 5.4.2.2 bit interleaving. */
static void tx_interleave(const uint8_t *e, uint8_t *f, uint32_t E, uint8_t Qm)
{
  for (uint32_t j = 0; j < E / Qm; j++)
    for (uint32_t i = 0; i < Qm; i++)
      f[i + j * Qm] = e[i * (E / Qm) + j];
}

/* ---- one transport block ------------------------------------------------------ */

typedef struct {
  const char *name;
  uint8_t BG;
  uint16_t Z;
  uint16_t Kprime; /* bits per code block including its CRC; a multiple of 8 */
  uint16_t C;
  uint8_t Qm;
  uint32_t E; /* code blocks r < first_rE2 */
  uint32_t E2; /* the rest */
  uint16_t first_rE2;
  double sigma; /* noise standard deviation per BPSK symbol of amplitude 1 */
} tb_case_t;

#define LLR_SCALE 16.0
#define MAX_ITER 8

typedef struct {
  uint8_t *a; /* transmitted bits, packed MSB first, Kprime per block */
  int16_t *llr; /* soft input for the whole transport block */
  uint32_t G;
} tb_tx_t;

/* Encode C code blocks of random bits and transmit them with redundancy version rv. */
static void tb_transmit(const tb_case_t *tc, tb_tx_t *tx, int rv, int new_data)
{
  const uint32_t K = (tc->BG == 1 ? 22u : 10u) * tc->Z;
  const uint32_t N = (tc->BG == 1 ? 66u : 50u) * tc->Z;
  const uint32_t F = K - tc->Kprime;
  const uint32_t Foffset = tc->Kprime - 2u * tc->Z;
  const uint32_t k0 = irx_index_k0[tc->BG - 1][rv] * tc->Z; /* Ncb = N: no LBRM */
  const uint32_t kbytes = (K + 7u) / 8u;
  uint8_t *in = calloc(kbytes, 1);
  uint8_t *d = calloc(N, 1);
  uint8_t *e = malloc(tc->E2 > tc->E ? tc->E2 : tc->E);
  uint8_t *f = malloc(tc->E2 > tc->E ? tc->E2 : tc->E);

  ldpc_crc_tabs_init();
  uint32_t off = 0;
  for (uint16_t r = 0; r < tc->C; r++) {
    uint8_t *a = tx->a + (size_t)r * (tc->Kprime / 8u);
    if (new_data) {
      for (uint32_t i = 0; i < (tc->Kprime - 24u) / 8u; i++)
        a[i] = (uint8_t)(rng_u32() >> 24);
      const uint32_t crc = ldpc_crc24_ref(a, tc->Kprime - 24, tc->C > 1 ? g_crc24b_tab : g_crc24a_tab) >> 8;
      a[(tc->Kprime - 24u) / 8u + 0] = (uint8_t)(crc >> 16);
      a[(tc->Kprime - 24u) / 8u + 1] = (uint8_t)(crc >> 8);
      a[(tc->Kprime - 24u) / 8u + 2] = (uint8_t)crc;
    }
    memset(in, 0, kbytes); /* filler bits are 0 */
    memcpy(in, a, tc->Kprime / 8u);
    encoder_implemparams_t impp = {.gen_code = 0, .K = K, .Kb = tc->BG == 1 ? 22 : 10, .Zc = tc->Z, .F = F, .BG = tc->BG};
    uint8_t *inp[1] = {in};
    const int n = LDPCencoder(inp, d, &impp);
    CHECK(n == (int)N, "%s: encoder returned %d bits, expected %u", tc->name, n, (unsigned)N);

    const uint32_t E = (r < tc->first_rE2) ? tc->E : tc->E2;
    tx_rate_match(d, e, E, N, k0, Foffset, F);
    tx_interleave(e, f, E, tc->Qm);
    for (uint32_t i = 0; i < E; i++) {
      double y = (f[i] ? -1.0 : 1.0) + tc->sigma * rng_gauss();
      double l = nearbyint(LLR_SCALE * y);
      tx->llr[off + i] = (int16_t)(l > 32767 ? 32767 : (l < -32768 ? -32768 : l));
    }
    off += E;
  }
  tx->G = off;
  free(in);
  free(d);
  free(e);
  free(f);
}

/* Decode the transport block on the DSP path. Returns the number of code blocks
 * that decoded to the transmitted bits; *cycles gets the simulator cycles. */
static int tb_receive(const tb_case_t *tc,
                      const tb_tx_t *tx,
                      int rv,
                      int d_clear,
                      uint32_t *llrLen,
                      int use_addr_cache,
                      uint8_t *iters,
                      uint64_t *cycles)
{
  const uint32_t K = (tc->BG == 1 ? 22u : 10u) * tc->Z;
  uint32_t len1 = *llrLen, len2 = *llrLen;
  const uint8_t R = rate_index(rv, tc->E, tc->BG, tc->Z, &len1);
  const uint8_t R2 = rate_index(rv, tc->E2, tc->BG, tc->Z, &len2);
  *llrLen = len1 > len2 ? len1 : len2;

  ldpc_q_batch_req_t br;
  memset(&br, 0, sizeof br);
  br.BG = tc->BG;
  br.R = R;
  br.R2 = R2;
  br.numMaxIter = MAX_ITER;
  br.Z = tc->Z;
  br.C = tc->C;
  br.first_rE2 = tc->first_rE2;
  br.Kprime = tc->Kprime;
  br.K = (uint16_t)K;
  br.crc_type = tc->C > 1 ? 1 : 0;
  br.nworkers = 1;
  /* The ARM side's layout: one 128-byte aligned slot per code block. The hard
   * decision writes whole HVX vectors, which assumes that alignment. */
  br.out_stride = (((K + 7u) / 8u) + 127u) & ~127u;
  br.prep_on_dsp = 1;
  br.Qm = tc->Qm;
  br.rv_index = (uint8_t)rv;
  br.d_clear = (uint8_t)d_clear;
  br.E = tc->E;
  br.E2 = tc->E2;
  br.F = K - tc->Kprime;
  br.tbslbrm = 0;
  br.harq_pid = 3;
  br.G = tx->G;

  uint8_t *out = memalign(128, (size_t)tc->C * br.out_stride);
  memset(out, 0, (size_t)tc->C * br.out_stride);
  uint8_t meta[LDPC_HEX_BATCH_MAX_CB];
  memset(meta, 0xff, sizeof meta);

  /* Without the address cache the kernels compute every edge address inline; with
   * it they read them. Both must give the same bits and iteration counts. */
  for (int i = 0; i < LDPC_ADDR_CACHE_N; i++)
    g_addr_cache[i].valid = 0;
  if (use_addr_cache) {
    for (int k = 0; k < 2; k++) {
      t_nrLDPC_dec_params dp = {.BG = tc->BG, .Z = tc->Z, .R = k ? R2 : R, .numMaxIter = MAX_ITER};
      t_nrLDPC_lut lut;
      memset(&lut, 0, sizeof lut);
      nrLDPC_init(&dp, &lut);
      ldpc_addr_cache_build(&g_addr_cache[k], &lut, tc->BG, tc->Z, dp.R);
    }
  }

  g_job.br = &br;
  g_job.llr_base = (const int8_t *)tx->llr;
  g_job.out_base = out;
  g_job.meta = meta;
  g_job.out_bytes = br.out_stride;

  uint32_t phase_us[LDPC_NUM_PHASES] = {0};
  int good = 0;
  const uint64_t t0 = hexagon_sim_read_pcycles();
  for (int r = 0; r < tc->C; r++)
    ldpc_decode_one(&g_workers[0], r, phase_us);
  *cycles = hexagon_sim_read_pcycles() - t0;

  for (int r = 0; r < tc->C; r++) {
    const uint8_t *a = tx->a + (size_t)r * (tc->Kprime / 8u);
    const int same = memcmp(out + (size_t)r * br.out_stride, a, tc->Kprime / 8u) == 0;
    iters[r] = meta[r];
    if (same && meta[r] < MAX_ITER)
      good++;
  }
  free(out);
  return good;
}

static void run_case(const tb_case_t *tc)
{
  printf("%s: BG%u Z=%u K'=%u C=%u Qm=%u E=%u/%u\n",
         tc->name,
         tc->BG,
         tc->Z,
         tc->Kprime,
         tc->C,
         tc->Qm,
         (unsigned)tc->E,
         (unsigned)tc->E2);
  tb_tx_t tx;
  tx.a = calloc((size_t)tc->C * (tc->Kprime / 8u), 1);
  tx.llr = calloc((size_t)tc->C * (tc->E > tc->E2 ? tc->E : tc->E2) + 128, sizeof(int16_t));
  g_rng = 0x1234u + tc->Z;
  tb_transmit(tc, &tx, 0, 1);

  uint8_t it[2][LDPC_HEX_BATCH_MAX_CB];
  uint64_t cyc[2];
  int good[2];
  for (int ac = 0; ac < 2; ac++) {
    uint32_t llrLen = 0;
    good[ac] = tb_receive(tc, &tx, 0, 1, &llrLen, ac, it[ac], &cyc[ac]);
  }
  CHECK(good[0] == tc->C, "%s: %d of %u code blocks decoded", tc->name, good[0], tc->C);
  CHECK(good[1] == good[0] && memcmp(it[0], it[1], tc->C) == 0,
        "%s: address cache changes the result (%d vs %d decoded)",
        tc->name,
        good[1],
        good[0]);
  printf("  %d/%u decoded, iterations", good[0], tc->C);
  for (int r = 0; r < tc->C; r++)
    printf(" %u", it[0][r]);
  printf(", %llu cycles/code block (%llu with address cache)\n",
         (unsigned long long)(cyc[0] / tc->C),
         (unsigned long long)(cyc[1] / tc->C));
  free(tx.a);
  free(tx.llr);
}

/* HARQ: a first transmission too noisy to decode, then a retransmission with another
 * redundancy version that only decodes if the soft buffer kept the first one.
 *
 * The HARQ process first carries an earlier transport block, which itself needed rv0 and
 * rv2, so its soft buffer holds that block's accumulated history. The new block's first
 * round must clear everything a later round reads -- and a later round reads further
 * than the first, because the rate index drops as the filled length grows. */
static void run_harq(void)
{
  const tb_case_t prev = {"harq-prev", 1, 384, 7680, 1, 4, 9216, 9216, 1, 0.3};
  const tb_case_t tc = {"harq", 1, 384, 7680, 1, 4, 9216, 9216, 1, 0.65};
  printf("%s: BG%u Z=%u K'=%u rv0 then rv2, sigma %.2f\n", tc.name, tc.BG, tc.Z, tc.Kprime, tc.sigma);
  tb_tx_t tx;
  tx.a = calloc(tc.Kprime / 8u, 1);
  tx.llr = calloc(tc.E + 128, sizeof(int16_t));
  uint8_t it[LDPC_HEX_BATCH_MAX_CB];
  uint64_t cyc;
  uint32_t llrLen = 0;

  g_rng = 0x77u;
  tb_transmit(&prev, &tx, 0, 1);
  tb_receive(&prev, &tx, 0, 1, &llrLen, 1, it, &cyc);
  tb_transmit(&prev, &tx, 2, 0);
  tb_receive(&prev, &tx, 2, 0, &llrLen, 1, it, &cyc);

  g_rng = 0x4a5u;
  llrLen = 0;
  tb_transmit(&tc, &tx, 0, 1);
  const int r0 = tb_receive(&tc, &tx, 0, 1, &llrLen, 1, it, &cyc);
  tb_transmit(&tc, &tx, 2, 0);
  const int r1 = tb_receive(&tc, &tx, 2, 0, &llrLen, 1, it, &cyc);

  /* and the same retransmission alone, into a cleared buffer */
  uint32_t llrLen2 = 0;
  const int alone = tb_receive(&tc, &tx, 2, 1, &llrLen2, 1, it, &cyc);

  printf("  rv0 alone %s, rv2 alone %s, rv0+rv2 %s\n",
         r0 ? "decoded" : "failed",
         alone ? "decoded" : "failed",
         r1 ? "decoded" : "failed");
  CHECK(!r0 && !alone, "harq: a single transmission decoded, so the test shows nothing; raise sigma");
  CHECK(r1 == 1, "harq: the combined transmissions did not decode");
  free(tx.a);
  free(tx.llr);
}

/* ---- HVX kernels against their scalar references ----------------------------------- */

static void kernel_checks(uint16_t Z, uint8_t BG, uint8_t R)
{
  t_nrLDPC_dec_params dp = {.BG = BG, .Z = Z, .R = R, .numMaxIter = MAX_ITER};
  t_nrLDPC_lut lut;
  memset(&lut, 0, sizeof lut);
  const uint32_t numLLR = nrLDPC_init(&dp, &lut);
  const size_t n = NR_LDPC_MAX_NUM_LLR + 128;
  int8_t *a = calloc(n, 1), *b = calloc(n, 1), *src = calloc(n, 1);
  int bad = 0;
  for (uint32_t i = 0; i < NR_LDPC_MAX_NUM_LLR; i++)
    src[i] = (int8_t)((i * 37u + (i >> 5)) & 0xff);

  static const uint32_t lens[] = {1, 127, 128, 129, 383, 384, 385};
  for (unsigned li = 0; li <= sizeof lens / sizeof lens[0]; li++) {
    const uint32_t len = li < sizeof lens / sizeof lens[0] ? lens[li] : numLLR;
    memset(a, 0x5a, NR_LDPC_MAX_NUM_LLR);
    memset(b, 0x5a, NR_LDPC_MAX_NUM_LLR);
    memcpy(a, src, len);
    hvx_memcpy(b, src, len);
    bad += memcmp(a, b, NR_LDPC_MAX_NUM_LLR) != 0;
  }
  CHECK(!bad, "hvx_memcpy differs from memcpy (Z=%u)", Z);

  memset(a, 0, NR_LDPC_MAX_NUM_LLR);
  memset(b, 0, NR_LDPC_MAX_NUM_LLR);
  nrLDPC_llr2llrProcBuf(&lut, src, a, Z, BG);
  hvx_llr2llrProcBuf(&lut, src, b, Z, BG);
  CHECK(!memcmp(a, b, NR_LDPC_MAX_NUM_LLR), "hvx_llr2llrProcBuf differs from the reference (BG%u Z=%u)", BG, Z);

  memset(a, 0, NR_LDPC_MAX_NUM_LLR);
  memset(b, 0, NR_LDPC_MAX_NUM_LLR);
  nrLDPC_llrRes2llrOut(&lut, a, src, Z, BG);
  hvx_llrRes2llrOut(&lut, b, src, Z, BG);
  CHECK(!memcmp(a, b, NR_LDPC_MAX_NUM_LLR), "hvx_llrRes2llrOut differs from the reference (BG%u Z=%u)", BG, Z);

  if (BG == 1) {
    int8_t *ca = calloc(NR_LDPC_SIZE_CN_PROC_BUF + 128, 1), *cb = calloc(NR_LDPC_SIZE_CN_PROC_BUF + 128, 1);
    nrLDPC_llr2CnProcBuf_BG1(&lut, src, ca, Z);
    hvx_llr2CnProcBuf_BG1(&lut, src, cb, Z);
    CHECK(!memcmp(ca, cb, NR_LDPC_SIZE_CN_PROC_BUF), "hvx_llr2CnProcBuf_BG1 differs from the reference (Z=%u)", Z);
    free(ca);
    free(cb);
  }

  /* cnProcPc returns a flag and writes nothing: plant one sign bit at a time, on the
   * slot and 128-byte chunk boundaries where a ring indexing bug would show. */
  ldpc_addr_cache_t *ac = &g_addr_cache[0];
  ac->valid = 0;
  ldpc_addr_cache_build(ac, &lut, BG, Z, R);
  memset(a, 0, NR_LDPC_MAX_NUM_LLR);
  bad = cnProcPc_fused(a, &lut, Z, BG, ac) != 0 || cnProcPc_ref(a, &lut, Z, BG) != 0;
  for (uint32_t base = 0; base + Z <= numLLR; base += 7u * Z) {
    static const int off[] = {0, 1, 126, 127, 128, 129, 254, 255, 256, 257};
    for (unsigned oi = 0; oi < sizeof off / sizeof off[0]; oi++) {
      const uint32_t o = (uint32_t)off[oi] < Z ? (uint32_t)off[oi] : Z - 1u;
      a[base + o] = -1;
      bad += cnProcPc_fused(a, &lut, Z, BG, ac) != cnProcPc_ref(a, &lut, Z, BG);
      bad += cnProcPc_fused(a, &lut, Z, BG, NULL) != cnProcPc_ref(a, &lut, Z, BG);
      a[base + o] = 0;
    }
  }
  CHECK(!bad, "cnProcPc_fused disagrees with the scalar reference in %d cases (BG%u Z=%u R=%u)", bad, BG, Z, R);
  ac->valid = 0;
  free(a);
  free(b);
  free(src);
}

static void crc_checks(void)
{
  int bad = 0;
  ldpc_crc_tabs_init();
  uint8_t *buf = calloc(2048, 1);
  for (int i = 0; i < 2048; i++)
    buf[i] = (uint8_t)(rng_u32() >> 24);
  for (int bitlen = 1; bitlen <= 1200; bitlen++)
    for (int t = 0; t < 2; t++) {
      const uint32_t(*tab)[256] = t ? g_crc24b_tab : g_crc24a_tab;
      bad += ldpc_crc24(buf, bitlen, tab) != ldpc_crc24_ref(buf, bitlen, tab);
    }
  CHECK(!bad, "ldpc_crc24 differs from the byte-at-a-time reference at %d lengths", bad);
  free(buf);
}

/* ---- worker buffers: the heap layout ldpc_hexagon_open() falls back to without VTCM ---- */

static void worker_init(ldpc_worker_t *w)
{
  w->cn_msg = memalign(128, NR_LDPC_SIZE_CN_PROC_BUF + HVX_VLEN);
  w->bn_msg = memalign(128, NR_LDPC_SIZE_BN_PROC_BUF + HVX_VLEN);
  w->llrRes = memalign(128, NR_LDPC_MAX_NUM_LLR + HVX_VLEN);
  w->llrProcBuf = memalign(128, NR_LDPC_MAX_NUM_LLR + HVX_VLEN);
  w->llrout = memalign(128, NR_LDPC_MAX_NUM_LLR + HVX_VLEN);
  w->crc_bits = memalign(128, (((NR_LDPC_MAX_NUM_LLR + 7) / 8) + 127) & ~127u);
  w->prep_e = memalign(128, NR_LDPC_MAX_NUM_LLR * sizeof(int16_t));
  w->prep_z = memalign(128, (68 * 384 + 16) * sizeof(int16_t));
  w->prep_out = memalign(128, NR_LDPC_MAX_NUM_LLR);
  w->gv_f = w->gv_e = NULL; /* no VTCM: the deinterleave takes its scalar path */
}

int main(void)
{
  worker_init(&g_workers[0]);

  printf("kernel checks\n");
  crc_checks();
  kernel_checks(384, 1, 13);
  kernel_checks(176, 1, 23);
  kernel_checks(104, 2, 15);
  kernel_checks(64, 2, 13);

  /* name, BG, Z, K', C, Qm, E, E2, first_rE2, sigma
   *
   * Every operating point has margin: OAI's CPU decoder (libldpc, 8 iterations) decodes
   * each of them on the same int8 decoder input, over 8 noise seeds. */
  static const tb_case_t cases[] = {
      {"bg1-z384-r13-2cb", 1, 384, 8448, 2, 4, 12672, 12676, 1, 0.5},
      {"bg1-z384-r23-256qam", 1, 384, 8448, 1, 8, 9216, 9216, 1, 0.35},
      {"bg1-z384-r89-filler", 1, 384, 7680, 1, 2, 8640, 8640, 1, 0.35},
      {"bg1-z176-r13-64qam", 1, 176, 3832, 1, 6, 6000, 6000, 1, 0.5},
      {"bg2-z384-r15", 2, 384, 3072, 1, 2, 13056, 13056, 1, 0.7},
      {"bg2-z104-r13", 2, 104, 1000, 1, 2, 2400, 2400, 1, 0.5},
      {"bg2-z64-r23", 2, 64, 608, 1, 2, 720, 720, 1, 0.4},
  };
  for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++)
    run_case(&cases[i]);
  run_harq();

  printf("%s: %d failure(s)\n", g_fail ? "FAILED" : "PASSED", g_fail);
  return g_fail ? 1 : 0;
}
