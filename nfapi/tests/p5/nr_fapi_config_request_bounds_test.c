/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* Bounds regression tests for the CONFIG.request unpacker.
 *
 * Several CONFIG.request TLVs are repeated: per-FD-occasion PRACH TLVs, SSB
 * masks, beam IDs and TDD slot configs. The unpacker stores each repetition at
 * a running index into an array whose size comes from an earlier TLV (or is
 * fixed). Each test crafts a TLV sequence that indexes past that array, or uses
 * it before it exists, and calls the real production unpacker, which must
 * return 0. Before the bounds checks were added these sequences wrote out of
 * bounds, dereferenced NULL, or aborted. The last test checks that consecutive
 * PRACH FD occasions without unused root sequences land in separate entries. */

#include "nfapi/tests/nr_fapi_test.h"
#include "nr_fapi_p5.h"
#include "nr_fapi_p5_utils.h"

typedef struct {
  uint8_t buf[4096];
  size_t pos;
} body_t;

/* encode with the library's own helpers, so the byte order matches the unpacker */
static void put8(body_t *b, uint8_t v)
{
  uint8_t *p = b->buf + b->pos;
  b->pos += push8(v, &p, b->buf + sizeof(b->buf));
}
static void put16(body_t *b, uint16_t v)
{
  uint8_t *p = b->buf + b->pos;
  b->pos += push16(v, &p, b->buf + sizeof(b->buf));
}
static void put32(body_t *b, uint32_t v)
{
  uint8_t *p = b->buf + b->pos;
  b->pos += push32(v, &p, b->buf + sizeof(b->buf));
}
/* TLV header, value written by the caller, then padding to 4 bytes (SCF 225 2.3.2.1) */
static void tlv_start(body_t *b, uint16_t tag, uint16_t length)
{
  put16(b, tag);
  put16(b, length);
}
static void tlv_pad(body_t *b, uint16_t length)
{
  for (int i = 0; i < get_tlv_padding(length); i++)
    put8(b, 0);
}
static void tlv8(body_t *b, uint16_t tag, uint8_t v)
{
  tlv_start(b, tag, 1);
  put8(b, v);
  tlv_pad(b, 1);
}
static void tlv16(body_t *b, uint16_t tag, uint16_t v)
{
  tlv_start(b, tag, 2);
  put16(b, v);
  tlv_pad(b, 2);
}
static void tlv32(body_t *b, uint16_t tag, uint32_t v)
{
  tlv_start(b, tag, 4);
  put32(b, v);
}

/* one complete PRACH FD occasion without unused root sequences, in packer order */
static void prach_fd_occasion(body_t *b, uint16_t k1)
{
  tlv16(b, NFAPI_NR_CONFIG_PRACH_ROOT_SEQUENCE_INDEX_TAG, 1);
  tlv8(b, NFAPI_NR_CONFIG_NUM_ROOT_SEQUENCES_TAG, 1);
  tlv16(b, NFAPI_NR_CONFIG_K1_TAG, k1);
  tlv8(b, NFAPI_NR_CONFIG_PRACH_ZERO_CORR_CONF_TAG, 1);
  tlv16(b, NFAPI_NR_CONFIG_NUM_UNUSED_ROOT_SEQUENCES_TAG, 0);
}

static uint8_t run_unpack(body_t *b, nfapi_nr_config_request_scf_t *msg)
{
  /* the unpacker starts with the num_tlv octet */
  memmove(b->buf + 1, b->buf, b->pos);
  b->buf[0] = 0;
  b->pos++;
  uint8_t *read = b->buf;
  return unpack_nr_config_request(&read, b->buf + b->pos, msg, NULL);
}

/* the message is heap allocated so that a write past it is caught by ASan */
#define EXPECT_REJECT(name, body)                                 \
  do {                                                            \
    nfapi_nr_config_request_scf_t *msg = calloc(1, sizeof(*msg)); \
    uint8_t rc = run_unpack(&(body), msg);                        \
    free_config_request(msg);                                     \
    free(msg);                                                    \
    if (rc != 0) {                                                \
      printf("FAIL: %s: CONFIG.request accepted\n", name);        \
      return 1;                                                   \
    }                                                             \
    printf("OK: %s: CONFIG.request rejected\n", name);            \
  } while (0)

static int test_ssb_mask_count(void)
{
  body_t b = {0};
  for (int i = 0; i < 3; i++) /* ssb_mask_list[] has 2 entries */
    tlv32(&b, NFAPI_NR_CONFIG_SSB_MASK_TAG, 0xffffffff);
  EXPECT_REJECT("3 SSB_MASK TLVs", b);
  return 0;
}

static int test_beam_id_count(void)
{
  body_t b = {0};
  for (int i = 0; i < 65; i++) /* ssb_beam_id_list[] has 64 entries */
    tlv8(&b, NFAPI_NR_CONFIG_BEAM_ID_TAG, i);
  EXPECT_REJECT("65 BEAM_ID TLVs", b);
  return 0;
}

static int test_prach_without_num_occasions(void)
{
  body_t b = {0};
  tlv16(&b, NFAPI_NR_CONFIG_K1_TAG, 1); /* no NUM_PRACH_FD_OCCASIONS before it */
  EXPECT_REJECT("K1 before NUM_PRACH_FD_OCCASIONS", b);
  return 0;
}

static int test_prach_too_many_occasions(void)
{
  body_t b = {0};
  tlv8(&b, NFAPI_NR_CONFIG_NUM_PRACH_FD_OCCASIONS_TAG, 1);
  prach_fd_occasion(&b, 1);
  prach_fd_occasion(&b, 2); /* second occasion, only one announced */
  EXPECT_REJECT("2 PRACH FD occasions for num_prach_fd_occasions 1", b);
  return 0;
}

static int test_unused_root_sequences_without_count(void)
{
  body_t b = {0};
  tlv8(&b, NFAPI_NR_CONFIG_NUM_PRACH_FD_OCCASIONS_TAG, 1);
  tlv16(&b, NFAPI_NR_CONFIG_UNUSED_ROOT_SEQUENCES_TAG, 1); /* no NUM_UNUSED_ROOT_SEQUENCES before it */
  EXPECT_REJECT("UNUSED_ROOT_SEQUENCES before NUM_UNUSED_ROOT_SEQUENCES", b);
  return 0;
}

static int test_invalid_scs_common(void)
{
  body_t b = {0};
  tlv8(&b, NFAPI_NR_CONFIG_FRAME_DUPLEX_TYPE_TAG, 1);
  tlv8(&b, NFAPI_NR_CONFIG_SCS_COMMON_TAG, 7); /* slots per frame are known for 0..4 only */
  EXPECT_REJECT("SCS_COMMON 7", b);
  return 0;
}

static int test_duplicate_scs_common(void)
{
  body_t b = {0};
  tlv8(&b, NFAPI_NR_CONFIG_FRAME_DUPLEX_TYPE_TAG, 1);
  tlv8(&b, NFAPI_NR_CONFIG_SCS_COMMON_TAG, 0); /* TDD table for 10 slots */
  tlv8(&b, NFAPI_NR_CONFIG_SCS_COMMON_TAG, 4); /* used to leak the 10-slot table and allocate a 160-slot one */
  EXPECT_REJECT("second SCS_COMMON after the TDD table", b);
  return 0;
}

#ifndef ENABLE_AERIAL
static int test_slot_config_count(void)
{
  body_t b = {0};
  tlv8(&b, NFAPI_NR_CONFIG_FRAME_DUPLEX_TYPE_TAG, 1);
  tlv8(&b, NFAPI_NR_CONFIG_SCS_COMMON_TAG, 0); /* 10 slots of 14 symbols */
  for (int i = 0; i < 10 * 14 + 1; i++)
    tlv8(&b, NFAPI_NR_CONFIG_SLOT_CONFIG_TAG, 0);
  EXPECT_REJECT("141 SLOT_CONFIG TLVs for 10 slots", b);
  return 0;
}

static int test_slot_config_without_tdd_table(void)
{
  body_t b = {0};
  tlv8(&b, NFAPI_NR_CONFIG_SLOT_CONFIG_TAG, 0); /* no TDD SCS_COMMON before it */
  EXPECT_REJECT("SLOT_CONFIG before the TDD table", b);
  return 0;
}
#endif

static int test_precoding_matrix_size(void)
{
  body_t b = {0};
  const uint16_t len = 2 + 6 + 5 * 1 * 4;
  tlv_start(&b, NFAPI_NR_CONFIG_PRECODING_TABLE_V6_TAG, len);
  put16(&b, 1); /* num_pm_idx */
  put16(&b, 0); /* pm_idx */
  put16(&b, 5); /* numLayers: more than 4 */
  put16(&b, 1); /* num_ant_ports */
  for (int i = 0; i < 5; i++)
    put32(&b, 0);
  tlv_pad(&b, len);
  EXPECT_REJECT("5x1 precoding matrix", b);
  return 0;
}

static int test_beamforming_table_size(void)
{
  body_t b = {0};
  const uint16_t len = 4 + 2 + 4;
  tlv_start(&b, NFAPI_NR_CONFIG_BEAMFORMING_TABLE_TAG, len);
  put16(&b, 0xffff); /* num_dig_beams */
  put16(&b, 0xffff); /* num_txrus: 65535 x 65535 weights announced, one present */
  put16(&b, 0);
  put32(&b, 0);
  tlv_pad(&b, len);
  EXPECT_REJECT("beamforming table larger than the message", b);
  return 0;
}

static int test_prach_occasions_without_unused_root_sequences(void)
{
  body_t b = {0};
  tlv8(&b, NFAPI_NR_CONFIG_NUM_PRACH_FD_OCCASIONS_TAG, 2);
  prach_fd_occasion(&b, 111);
  prach_fd_occasion(&b, 222);
  nfapi_nr_config_request_scf_t *msg = calloc(1, sizeof(*msg));
  uint8_t rc = run_unpack(&b, msg);
  const nfapi_nr_num_prach_fd_occasions_t *occ = msg->prach_config.num_prach_fd_occasions_list;
  int ok = rc != 0 && occ && occ[0].k1.value == 111 && occ[1].k1.value == 222;
  printf("%s: 2 PRACH FD occasions without unused root sequences (rc %d, k1 %d %d)\n",
         ok ? "OK" : "FAIL",
         rc,
         occ ? occ[0].k1.value : -1,
         occ ? occ[1].k1.value : -1);
  free_config_request(msg);
  free(msg);
  return !ok;
}

int main(void)
{
  fapi_test_init();

  if (test_ssb_mask_count())
    return 1;
  if (test_beam_id_count())
    return 1;
  if (test_prach_without_num_occasions())
    return 1;
  if (test_prach_too_many_occasions())
    return 1;
  if (test_unused_root_sequences_without_count())
    return 1;
  if (test_invalid_scs_common())
    return 1;
  if (test_duplicate_scs_common())
    return 1;
#ifndef ENABLE_AERIAL
  if (test_slot_config_count())
    return 1;
  if (test_slot_config_without_tdd_table())
    return 1;
#endif
  if (test_precoding_matrix_size())
    return 1;
  if (test_beamforming_table_size())
    return 1;
  if (test_prach_occasions_without_unused_root_sequences())
    return 1;

  printf("All CONFIG.request bounds tests passed\n");
  return 0;
}
