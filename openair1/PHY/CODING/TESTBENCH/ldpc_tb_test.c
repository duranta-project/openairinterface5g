/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// Transport-block level LDPC test through the slot coding interface (nrLDPC_coding_interface.h): a TB
// is encoded with the CPU library (CRC, segmentation, LDPC, rate matching, interleaving) and decoded
// from noiseless LLRs with the library under test (-v, e.g. _cuda). TBs of different sizes alternate,
// so the decoder sees code block counts C changing between calls. With limited buffer rate matching,
// Ncb = min(N, 3 TBS_LBRM / (2 C)) shrinks as C grows: a large TB after a small one must decode as well
// as it does on its own, i.e. nothing a decoder keeps across TBs may leak into the next one.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "PHY/CODING/nrLDPC_coding/nrLDPC_coding_interface.h"
#include "PHY/CODING/coding_defs.h"
#include "PHY/NR_TRANSPORT/nr_transport_common_proto.h"
#include "common/utils/threadPool/thread-pool.h"
#include "common/utils/LOG/log.h"

#define BASE_GRAPH 1
#define QM 6
#define NL 2
#define TBSLBRM 475584 // 273 PRBs, 2 layers, 64QAM (as cuBB sends it)
#define MAX_SEG 64

typedef struct {
  const char *name;
  uint32_t A; // TBS in bits
  uint32_t G; // rate matched bits (multiple of C * NL * QM: equal E for every code block)
  int16_t llr_mag; // noiseless LLR magnitude
  uint8_t rv; // redundancy version, transmitted (and decoded) on its own
} tb_shape_t;

// C = 25: Ncb = N (no limit). C = 36: Ncb = 19816 < N = 25344, so the circular buffer leaves the end of
// each segment's decoder input unused. The 25-block TB is sent as rv 3 (k0 = 56 Z, so its LLRs fill the
// whole circular buffer, like a retransmission on a live link) at full confidence, the 36-block one as
// rv 0 with low LLRs that still decode on their own: input it should never see would dominate its own.
static const tb_shape_t shapes[] = {{"C=25 (Ncb=N)", 204976, 25 * NL * QM * 1048, 127, 3}, {"C=36 (Ncb<N)", 303240, 471744, 6, 0}};

typedef struct {
  uint8_t *b; // TB + CRC
  uint8_t *c[MAX_SEG]; // code blocks
  uint32_t C, K, Z, F, Kb;
  uint8_t *f; // encoder output, packed bits (LSB first)
  int16_t *llr;
  uint8_t *dec; // decoder output
  int16_t *d; // decoder soft buffer (CPU libraries)
} tb_t;

static void make_tb(tb_t *tb, const tb_shape_t *s, nrLDPC_coding_interface_t *enc, tpool_t *tp, unsigned seed)
{
  const uint32_t A = s->A, B = A + 24;
  memset(tb, 0, sizeof(*tb));
  tb->b = calloc(B / 8 + 64, 1);
  srand(seed);
  for (uint32_t i = 0; i < A / 8; i++)
    tb->b[i] = rand();
  const unsigned int crc = crc24a(tb->b, A) >> 8;
  tb->b[A >> 3] = ((uint8_t *)&crc)[2];
  tb->b[1 + (A >> 3)] = ((uint8_t *)&crc)[1];
  tb->b[2 + (A >> 3)] = ((uint8_t *)&crc)[0];
  for (int r = 0; r < MAX_SEG; r++)
    tb->c[r] = aligned_alloc(64, 8448 / 8 + 64);
  tb->Kb = nr_segmentation(tb->b, tb->c, B, &tb->C, &tb->K, &tb->Z, &tb->F, BASE_GRAPH);

  const size_t f_bytes = (s->G / 8 + 64 + 63) & ~63;
  tb->f = aligned_alloc(64, f_bytes);
  memset(tb->f, 0, f_bytes);
  nrLDPC_segment_encoding_parameters_t seg[MAX_SEG] = {0};
  for (int r = 0; r < tb->C; r++) {
    seg[r].c = tb->c[r];
    seg[r].E = nr_get_E(s->G, tb->C, QM, NL, r);
  }
  nrLDPC_TB_encoding_parameters_t tbp = {.harq_unique_pid = 0,
                                         .nb_rb = 273,
                                         .Qm = QM,
                                         .mcs = 0,
                                         .nb_layers = NL,
                                         .BG = BASE_GRAPH,
                                         .rv_index = s->rv,
                                         .G = s->G,
                                         .tbslbrm = TBSLBRM,
                                         .A = A,
                                         .Kb = tb->Kb,
                                         .K = tb->K,
                                         .Z = tb->Z,
                                         .F = tb->F,
                                         .C = tb->C,
                                         .segments = seg,
                                         .output = tb->f};
  nrLDPC_slot_encoding_parameters_t slot = {.nb_TBs = 1, .threadPool = tp, .TBs = &tbp};
  AssertFatal(enc->nrLDPC_coding_encoder(&slot) == 0, "encoder failed\n");

  // Noiseless channel: +llr_mag for a 0, -llr_mag for a 1.
  tb->llr = aligned_alloc(64, ((s->G * sizeof(int16_t)) + 63) & ~63);
  for (uint32_t j = 0; j < s->G; j++)
    tb->llr[j] = ((tb->f[j >> 3] >> (j & 7)) & 1) ? -s->llr_mag : s->llr_mag;
  tb->dec = aligned_alloc(64, MAX_SEG * 8448 / 8 + 64);
  tb->d = aligned_alloc(64, MAX_SEG * 68 * 384 * sizeof(int16_t));
}

// Decodes tb (its own rv, fresh soft buffer) as HARQ process pid; true if every code block passes CRC.
static bool decode_tb(tb_t *tb, const tb_shape_t *s, nrLDPC_coding_interface_t *dec, tpool_t *tp, int pid)
{
  uint32_t processed = 0;
  decode_abort_t abort_decode;
  init_abort(&abort_decode);
  int llrLen = 0;
  nrLDPC_TB_decoding_parameters_t tbp = {.harq_unique_pid = pid,
                                         .processedSegments = &processed,
                                         .nb_rb = 273,
                                         .Qm = QM,
                                         .mcs = 0,
                                         .nb_layers = NL,
                                         .BG = BASE_GRAPH,
                                         .rv_index = s->rv,
                                         .max_ldpc_iterations = 8,
                                         .abort_decode = &abort_decode,
                                         .G = s->G,
                                         .tbslbrm = TBSLBRM,
                                         .A = s->A,
                                         .K = tb->K,
                                         .Z = tb->Z,
                                         .F = tb->F,
                                         .C = tb->C,
                                         .llr = tb->llr,
                                         .c = tb->dec,
                                         .d = tb->d,
                                         .d_to_be_cleared = true};
  tbp.E = nr_get_E(s->G, tb->C, QM, NL, 0);
  tbp.E2 = tbp.E;
  tbp.first_rE2 = tb->C;
  tbp.R = nr_get_R_ldpc_decoder(s->rv, tbp.E, BASE_GRAPH, tb->Z, &llrLen, 0);
  nrLDPC_slot_decoding_parameters_t slot = {.nb_TBs = 1, .threadPool = tp, .TBs = &tbp};
  AssertFatal(dec->nrLDPC_coding_decoder(&slot) == 0, "decoder failed\n");
  bool ok = true;
  for (int r = 0; r < tb->C; r++)
    ok &= tbp.decodeSuccess[r];
  return ok;
}

int main(int argc, char **argv)
{
  char *version = "";
  int rounds = 10;
  for (int opt; (opt = getopt(argc, argv, "v:n:h")) != -1;) {
    if (opt == 'v')
      version = optarg;
    else if (opt == 'n')
      rounds = atoi(optarg);
    else {
      printf("%s [-v LDPC library version under test, e.g. _cuda] [-n rounds]\n", argv[0]);
      return opt == 'h' ? 0 : 1;
    }
  }
  nrLDPC_coding_interface_t enc = {0}, dec = {0};
  AssertFatal(load_nrLDPC_coding_interface("", &enc, 16) == 0, "cannot load the CPU LDPC library\n");
  AssertFatal(load_nrLDPC_coding_interface(version, &dec, 16) == 0, "cannot load LDPC library %s\n", version);
  tpool_t tp;
  initFloatingCoresTpool(4, &tp, false, "ldpc");

  tb_t tbs[2];
  for (int s = 0; s < 2; s++) {
    make_tb(&tbs[s], &shapes[s], &enc, &tp, 1 + s);
    printf("%s: A %u, C %u, Z %u, G %u, E %d, rv %d, LLR %d\n", shapes[s].name, shapes[s].A, tbs[s].C, tbs[s].Z, shapes[s].G,
           nr_get_E(shapes[s].G, tbs[s].C, QM, NL, 0), shapes[s].rv, shapes[s].llr_mag);
  }

  // Each shape on its own first, then alternating. Every decode is a first transmission of its own HARQ
  // process, so failures can only come from state carried over between TBs.
  int fail[2] = {0}, n[2] = {0};
  const bool alone = decode_tb(&tbs[1], &shapes[1], &dec, &tp, 0);
  printf("%s alone: %s\n", shapes[1].name, alone ? "ok" : "FAIL");
  for (int i = 0; i < 2 * rounds; i++) {
    const int s = i & 1;
    n[s]++;
    fail[s] += !decode_tb(&tbs[s], &shapes[s], &dec, &tp, 1 + i % 8);
  }
  for (int s = 0; s < 2; s++)
    printf("%s alternating: %d/%d failed\n", shapes[s].name, fail[s], n[s]);

  free_nrLDPC_coding_interface(&dec);
  const bool ok = alone && fail[0] == 0 && fail[1] == 0;
  printf("%s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
