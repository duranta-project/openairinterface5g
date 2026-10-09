/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "openair1/PHY/NR_TRANSPORT/nr_dbt.h"
#include "common/utils/assertions.h"

struct configmodule_interface_s;
struct configmodule_interface_s *uniqCfg = NULL;
void exit_function(const char *file, const char *function, const int line, const char *s, const int assert)
{
  if (assert)
    abort();
  exit(EXIT_SUCCESS);
}

#define SYMBOL_SIZE 256
#define NUM_RB 20
#define NUM_SYMBOLS 14
#define SLOT_SIZE (SYMBOL_SIZE * NUM_SYMBOLS)
#define MAX_PORTS 3
#define MAX_BEAMS 3

#define CHECK(cond, ...)                                \
  do {                                                  \
    if (!(cond)) {                                      \
      printf("%s:%d: %s: ", __FILE__, __LINE__, #cond); \
      printf(__VA_ARGS__);                              \
      printf("\n");                                     \
      exit(1);                                          \
    }                                                   \
  } while (0)

static int16_t sat16(int32_t v)
{
  return v > INT16_MAX ? INT16_MAX : v < INT16_MIN ? INT16_MIN : (int16_t)v;
}

// what rotate_add_cpx_vector() adds for one sample: the Q15 product, saturated
static c16_t term(c16_t x, c16_t w)
{
  return (c16_t){sat16(((int32_t)x.r * w.r - (int32_t)x.i * w.i) >> 15), sat16(((int32_t)x.r * w.i + (int32_t)x.i * w.r) >> 15)};
}

static c16_t add(c16_t a, c16_t b)
{
  return (c16_t){sat16(a.r + b.r), sat16(a.i + b.i)};
}

static bool is_zero(c16_t x)
{
  return x.r == 0 && x.i == 0;
}

static bool eq(c16_t a, c16_t b)
{
  return a.r == b.r && a.i == b.i;
}

// a gNB with num_bb baseband ports and num_beams beams; in holds one symbol of a logical port
typedef struct {
  PHY_VARS_gNB *gNB;
  c16_t bb[MAX_PORTS][SLOT_SIZE];
  c16_t *bb_ptr[MAX_PORTS];
  c16_t in[MAX_PORTS][SYMBOL_SIZE];
  nfapi_nr_txru_t txru[MAX_BEAMS][MAX_PORTS];
  nfapi_nr_dig_beam_t beams[MAX_BEAMS];
} test_gnb_t;

static test_gnb_t *setup(int num_bb, int num_beams, const c16_t w[][MAX_PORTS], const uint16_t *beam_ids)
{
  test_gnb_t *t = calloc(1, sizeof(*t));
  t->gNB = calloc(1, sizeof(*t->gNB));
  NR_DL_FRAME_PARMS *fp = &t->gNB->frame_parms;
  fp->ofdm_symbol_size = SYMBOL_SIZE;
  fp->N_RB_DL = NUM_RB;
  fp->symbols_per_slot = NUM_SYMBOLS;
  fp->slots_per_subframe = 2;
  NR_gNB_COMMON *c = &t->gNB->common_vars;
  c->dbt = true;
  c->num_tx_bb = num_bb;
  for (int i = 0; i < MAX_PORTS; i++)
    t->bb_ptr[i] = t->bb[i];
  c->txdataF = t->bb_ptr;
  for (int b = 0; b < num_beams; b++) {
    for (int n = 0; n < num_bb; n++)
      t->txru[b][n] = (nfapi_nr_txru_t){w[b][n].r, w[b][n].i};
    t->beams[b] = (nfapi_nr_dig_beam_t){beam_ids[b], t->txru[b]};
    c->dbt_lut[beam_ids[b]] = &t->beams[b];
  }
  return t;
}

static void teardown(test_gnb_t *t)
{
  free(t->gNB);
  free(t);
}

// fills logical port p with a recognizable pattern
static void fill(test_gnb_t *t, int p)
{
  for (int k = 0; k < SYMBOL_SIZE; k++)
    t->in[p][k] = (c16_t){(int16_t)(1000 + 37 * k + 101 * p), (int16_t)(-500 + 11 * k - 53 * p)};
}

static nfapi_nr_tx_precoding_and_beamforming_t whole_pdu(int num_interfaces, const uint16_t *beam_per_interface)
{
  nfapi_nr_tx_precoding_and_beamforming_t pb = {.num_prgs = 1, .prg_size = NUM_RB};
  for (int i = 0; i < num_interfaces; i++)
    pb.prgs_list[0].dig_bf_interface_list[i].beam_idx = beam_per_interface[i];
  return pb;
}

// beamforms RBs [rb, rb + n) of logical port p in symbol l
static void beamform(test_gnb_t *t, const nfapi_nr_tx_precoding_and_beamforming_t *pb, int interface, int p, int slot, int l, int rb, int n)
{
  nr_dbt_beamform(t->gNB, pb, interface, slot, l, rb, rb, n, t->in[p] + rb * NR_NB_SC_PER_RB);
}

// two logical ports onto three baseband ports: every baseband port is the weighted sum of both
static void test_combines_all_logical_ports(void)
{
  const c16_t w[2][MAX_PORTS] = {{{16384, 0}, {0, 16384}, {-8192, 8192}}, {{0, -16384}, {8192, 0}, {12000, -3000}}};
  const uint16_t ids[2] = {7, 300};
  test_gnb_t *t = setup(3, 2, w, ids);
  const int l = 3, rb = 4, n = 5;
  fill(t, 0);
  fill(t, 1);
  const nfapi_nr_tx_precoding_and_beamforming_t pb = whole_pdu(2, ids);
  for (int p = 0; p < 2; p++)
    beamform(t, &pb, p, p, 0, l, rb, n);
  for (int txru = 0; txru < 3; txru++)
    for (int k = rb * NR_NB_SC_PER_RB; k < (rb + n) * NR_NB_SC_PER_RB; k++) {
      const c16_t expect = add(term(t->in[0][k], w[0][txru]), term(t->in[1][k], w[1][txru]));
      CHECK(eq(t->bb[txru][l * SYMBOL_SIZE + k], expect), "txru %d k %d", txru, k);
    }
  teardown(t);
}

// only the symbol and the RBs of the call are written
static void test_other_symbols_and_rbs_untouched(void)
{
  const c16_t w[1][MAX_PORTS] = {{{32767, 0}, {32767, 0}}};
  const uint16_t ids[1] = {5};
  test_gnb_t *t = setup(2, 1, w, ids);
  fill(t, 0);
  const nfapi_nr_tx_precoding_and_beamforming_t pb = whole_pdu(1, ids);
  const int l = 9, rb = 6, n = 3;
  beamform(t, &pb, 0, 0, 0, l, rb, n);
  for (int txru = 0; txru < 2; txru++)
    for (int i = 0; i < SLOT_SIZE; i++) {
      const int k = i % SYMBOL_SIZE;
      const bool inside = i / SYMBOL_SIZE == l && k >= rb * NR_NB_SC_PER_RB && k < (rb + n) * NR_NB_SC_PER_RB;
      if (inside)
        CHECK(eq(t->bb[txru][i], term(t->in[0][k], w[0][txru])), "txru %d i %d", txru, i);
      else
        CHECK(is_zero(t->bb[txru][i]), "txru %d i %d written", txru, i);
    }
  teardown(t);
}

// two PDUs on the same REs (MU-MIMO) add up instead of the second overwriting the first
static void test_overlapping_pdus_accumulate(void)
{
  const c16_t w[2][MAX_PORTS] = {{{16384, 0}}, {{0, 16384}}};
  const uint16_t ids[2] = {1, 2};
  test_gnb_t *t = setup(1, 2, w, ids);
  fill(t, 0);
  fill(t, 1);
  const nfapi_nr_tx_precoding_and_beamforming_t pb0 = whole_pdu(1, &ids[0]);
  const nfapi_nr_tx_precoding_and_beamforming_t pb1 = whole_pdu(1, &ids[1]);
  beamform(t, &pb0, 0, 0, 0, 5, 2, 4);
  beamform(t, &pb1, 0, 1, 0, 5, 2, 4);
  for (int k = 2 * NR_NB_SC_PER_RB; k < 6 * NR_NB_SC_PER_RB; k++) {
    const c16_t expect = add(term(t->in[0][k], w[0][0]), term(t->in[1][k], w[1][0]));
    CHECK(eq(t->bb[0][5 * SYMBOL_SIZE + k], expect), "k %d", k);
  }
  teardown(t);
}

// a zero weight contributes nothing: a diagonal table leaves the other baseband port empty
static void test_diagonal_table(void)
{
  const c16_t w[2][MAX_PORTS] = {{{32767, 0}, {0, 0}}, {{0, 0}, {32767, 0}}};
  const uint16_t ids[2] = {0, 1};
  test_gnb_t *t = setup(2, 2, w, ids);
  fill(t, 0);
  const nfapi_nr_tx_precoding_and_beamforming_t pb = whole_pdu(1, &ids[0]);
  beamform(t, &pb, 0, 0, 0, 0, 0, NUM_RB);
  for (int k = 0; k < NUM_RB * NR_NB_SC_PER_RB; k++) {
    CHECK(eq(t->bb[0][k], term(t->in[0][k], w[0][0])), "k %d", k);
    CHECK(is_zero(t->bb[1][k]), "k %d", k);
  }
  teardown(t);
}

// with phase compensation, the rotation of the symbol (in its slot) is applied with the weight
static void test_phase_compensation(void)
{
  const c16_t w[1][MAX_PORTS] = {{{16384, 8192}, {-4096, 20000}}};
  const uint16_t ids[1] = {3};
  test_gnb_t *t = setup(2, 1, w, ids);
  t->gNB->phase_comp = true;
  const int slot = 3, l = 6;
  const int idx = (slot % t->gNB->frame_parms.slots_per_subframe) * NUM_SYMBOLS + l;
  const c16_t rot = {23170, -23170};
  t->gNB->frame_parms.symbol_rotation[0][idx] = rot;
  fill(t, 0);
  const nfapi_nr_tx_precoding_and_beamforming_t pb = whole_pdu(1, ids);
  beamform(t, &pb, 0, 0, slot, l, 1, 2);
  for (int txru = 0; txru < 2; txru++)
    for (int k = NR_NB_SC_PER_RB; k < 3 * NR_NB_SC_PER_RB; k++) {
      const c16_t expect = term(t->in[0][k], c16mulShift(w[0][txru], rot, 15));
      CHECK(eq(t->bb[txru][l * SYMBOL_SIZE + k], expect), "txru %d k %d", txru, k);
    }
  teardown(t);
}

int main(void)
{
  test_combines_all_logical_ports();
  test_other_symbols_and_rbs_untouched();
  test_overlapping_pdus_accumulate();
  test_diagonal_table();
  test_phase_compensation();
  printf("all nr_dbt tests passed\n");
  return 0;
}
