/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* Bounds regression tests for the NR P7 unpackers.
 *
 * Several P7 PDUs carry a count octet/word followed by that many elements, and
 * the decoded elements land in fixed-capacity arrays. The production unpackers
 * must reject a count larger than its destination array (SCF 222 bounds), the
 * same way pullarray*() already does, instead of writing past the array.
 *
 * Each test crafts a message body with one oversized count and calls the real
 * production unpacker, which must return 0. Built with -DSANITIZE_ADDRESS=ON
 * these bodies trigger a heap-buffer-overflow write before the bounds checks
 * were added, and pass after. */

#include "nfapi/tests/nr_fapi_test.h"
#include "nr_fapi_p7.h"
#include "common/config/config_userapi.h"

configmodule_interface_t *uniqCfg = NULL;

/* small append helpers over a flat byte buffer */
typedef struct {
  uint8_t buf[4608];
  size_t pos;
} body_t;

static void put8(body_t *b, uint8_t v)
{
  b->buf[b->pos++] = v;
}
static void put16(body_t *b, uint16_t v)
{
  b->buf[b->pos++] = v >> 8;
  b->buf[b->pos++] = v & 0xff;
}
static void pad(body_t *b, size_t n)
{
  for (size_t i = 0; i < n && b->pos < sizeof(b->buf); i++)
    b->buf[b->pos++] = 0;
}

/* Call a production unpacker on the crafted body, into a heap target sized to
 * exactly one message so an out-of-bounds write lands outside the allocation.
 * cleanup frees anything the unpacker allocated inside msg. */
#define EXPECT_REJECT(unpack_fn, body, type, cleanup)                  \
  do {                                                                 \
    type *msg = calloc(1, sizeof(*msg));                               \
    uint8_t *read = (body).buf;                                        \
    uint8_t *end = (body).buf + (body).pos;                            \
    uint8_t rc = unpack_fn(&read, end, msg);                           \
    cleanup;                                                           \
    free(msg);                                                         \
    if (rc != 0) {                                                     \
      printf("FAIL: %s accepted an out-of-range count\n", #unpack_fn); \
      return 1;                                                        \
    }                                                                  \
    printf("OK: %s rejected an out-of-range count\n", #unpack_fn);     \
  } while (0)

/* Same as EXPECT_REJECT, for a count or length that the message is too short to hold: the
 * unpacker must reject it before allocating for it, so list must still be NULL. */
#define EXPECT_REJECT_NO_ALLOC(unpack_fn, body, type, list, cleanup)                    \
  do {                                                                                  \
    type *msg = calloc(1, sizeof(*msg));                                                \
    uint8_t *read = (body).buf;                                                         \
    uint8_t *end = (body).buf + (body).pos;                                             \
    uint8_t rc = unpack_fn(&read, end, msg);                                            \
    const bool allocated = (list) != NULL;                                              \
    cleanup;                                                                            \
    free(msg);                                                                          \
    if (rc != 0 || allocated) {                                                         \
      printf("FAIL: %s %s %s\n", #unpack_fn, rc ? "accepted" : "allocated for", #list); \
      return 1;                                                                         \
    }                                                                                   \
    printf("OK: %s rejected %s before allocating it\n", #unpack_fn, #list);             \
  } while (0)

static void put32(body_t *b, uint32_t v)
{
  put16(b, v >> 16);
  put16(b, v & 0xffff);
}

static int test_ul_tti_n_group(void)
{
  body_t b = {0};
  put16(&b, 0); /* SFN */
  put16(&b, 0); /* Slot */
  put8(&b, 0); /* n_pdus */
  put8(&b, 0); /* rach_present */
  put8(&b, 0); /* n_ulsch */
  put8(&b, 0); /* n_ulcch */
  put8(&b, NFAPI_MAX_NUM_GROUPS + 1); /* n_group: one past groups_list[] */
  pad(&b, NFAPI_MAX_NUM_GROUPS + 1); /* one n_ue octet per group */
  EXPECT_REJECT(unpack_ul_tti_request, b, nfapi_nr_ul_tti_request_t, (void)0);
  return 0;
}

static int test_dl_tti_n_pdus(void)
{
  body_t b = {0};
  put16(&b, 0); /* SFN */
  put16(&b, 0); /* Slot */
  put8(&b, NFAPI_NR_MAX_DL_TTI_PDUS + 1); /* nPDUs: one past dl_tti_pdu_list[] */
  put8(&b, 0); /* nGroup */
  pad(&b, 200); /* PDU type/size octets for the loop to consume */
  EXPECT_REJECT(unpack_dl_tti_request, b, nfapi_nr_dl_tti_request_t, (void)0);
  return 0;
}

static int test_dl_tti_n_ue(void)
{
  body_t b = {0};
  put16(&b, 0); /* SFN */
  put16(&b, 0); /* Slot */
  put8(&b, 0); /* nPDUs */
  put8(&b, 1); /* nGroup = 1 */
  put8(&b, 13); /* nUe[0]: one past PduIdx[0][12] */
  pad(&b, 13); /* PduIdx octets */
  EXPECT_REJECT(unpack_dl_tti_request, b, nfapi_nr_dl_tti_request_t, (void)0);
  return 0;
}

static int test_ul_dci_num_pdus(void)
{
  body_t b = {0};
  put16(&b, 0); /* SFN */
  put16(&b, 0); /* Slot */
  put8(&b, NFAPI_NR_MAX_NB_CORESETS + 1); /* numPdus: one past ul_dci_pdu_list[] */
  pad(&b, 200);
  EXPECT_REJECT(unpack_ul_dci_request, b, nfapi_nr_ul_dci_request_t, (void)0);
  return 0;
}

static int test_tx_data_num_pdus(void)
{
  body_t b = {0};
  put16(&b, 0); /* SFN */
  put16(&b, 0); /* Slot */
  /* Number_of_PDUs: one past pdu_list[]; 0x0011 exceeds the max in either byte order */
  put16(&b, NFAPI_NR_MAX_TX_REQUEST_PDUS + 1);
  pad(&b, 200);
  EXPECT_REJECT(unpack_tx_data_request, b, nfapi_nr_tx_data_request_t, (void)0);
  return 0;
}

static int test_rach_num_preamble(void)
{
  body_t b = {0};
  put16(&b, 0); /* sfn */
  put16(&b, 0); /* slot */
  put8(&b, 1); /* number_of_pdus = 1 */
  /* rach indication body */
  put16(&b, 0); /* phy_cell_id */
  put8(&b, 0); /* symbol_index */
  put8(&b, 0); /* slot_index */
  put8(&b, 0); /* freq_index */
  put8(&b, 0); /* avg_rssi */
  put8(&b, 0); /* avg_snr */
  put8(&b, 65); /* num_preamble: one past preamble_list[64] */
  pad(&b, 120);
  EXPECT_REJECT(unpack_nr_rach_indication, b, nfapi_nr_rach_indication_t, free(msg->pdu_list));
  return 0;
}

static int test_srs_toa_num_ta(void)
{
  body_t b = {0};
  put16(&b, 0); /* sfn */
  put16(&b, 0); /* slot */
  put8(&b, 1); /* number_of_pdus */
  put16(&b, 0); /* rnti */
  put8(&b, NFAPI_NR_MAX_NUM_TA_NSEC + 1); /* num_ta: one past ta_offset_nsec[] */
  pad(&b, 120);
  EXPECT_REJECT(unpack_nr_srs_toa_vendor_ext_indication, b, nfapi_nr_srs_toa_vendor_ext_indication_t, free(msg->pdu_list));
  return 0;
}

/* The counts below are the same in either byte order. Each message is far shorter than the
 * count it declares. */
static int test_rx_data_number_of_pdus(void)
{
  body_t b = {0};
  put16(&b, 0); /* sfn */
  put16(&b, 0); /* slot */
  put16(&b, 0xffff); /* number_of_pdus */
  pad(&b, 64);
  EXPECT_REJECT_NO_ALLOC(unpack_nr_rx_data_indication, b, nfapi_nr_rx_data_indication_t, msg->pdu_list, free(msg->pdu_list));
  return 0;
}

static int test_rx_data_pdu_length(void)
{
  body_t b = {0};
  put16(&b, 0); /* sfn */
  put16(&b, 0); /* slot */
  put16(&b, 0x0101); /* number_of_pdus = 257, which the 4 kB message can hold */
  put32(&b, 0); /* handle */
  put16(&b, 0); /* rnti */
  put8(&b, 0); /* harq_id */
  put32(&b, 0xffffffff); /* pdu_length: 4 GB */
  put8(&b, 0); /* ul_cqi */
  put16(&b, 0); /* timing_advance */
  put16(&b, 0); /* rssi */
  pad(&b, 257 * 16);
  EXPECT_REJECT_NO_ALLOC(
      unpack_nr_rx_data_indication, b, nfapi_nr_rx_data_indication_t, msg->pdu_list[0].pdu, free(msg->pdu_list[0].pdu);
      free(msg->pdu_list));
  return 0;
}

static int test_crc_number_crcs(void)
{
  body_t b = {0};
  put16(&b, 0); /* sfn */
  put16(&b, 0); /* slot */
  put16(&b, 0xffff); /* number_crcs */
  pad(&b, 64);
  EXPECT_REJECT_NO_ALLOC(unpack_nr_crc_indication, b, nfapi_nr_crc_indication_t, msg->crc_list, free(msg->crc_list));
  return 0;
}

static int test_uci_num_ucis(void)
{
  body_t b = {0};
  put16(&b, 0); /* sfn */
  put16(&b, 0); /* slot */
  put16(&b, 0xffff); /* num_ucis */
  pad(&b, 64);
  EXPECT_REJECT_NO_ALLOC(unpack_nr_uci_indication, b, nfapi_nr_uci_indication_t, msg->uci_list, free(msg->uci_list));
  return 0;
}

static int test_srs_number_of_pdus(void)
{
  body_t b = {0};
  put16(&b, 0); /* sfn */
  put16(&b, 0); /* slot */
  put16(&b, 0); /* control_length */
  put8(&b, 0xff); /* number_of_pdus */
  pad(&b, 64);
  EXPECT_REJECT_NO_ALLOC(unpack_nr_srs_indication, b, nfapi_nr_srs_indication_t, msg->pdu_list, free(msg->pdu_list));
  return 0;
}

int main(void)
{
  fapi_test_init();

  if (test_ul_tti_n_group())
    return 1;
  if (test_dl_tti_n_pdus())
    return 1;
  if (test_dl_tti_n_ue())
    return 1;
  if (test_ul_dci_num_pdus())
    return 1;
  if (test_tx_data_num_pdus())
    return 1;
  if (test_rach_num_preamble())
    return 1;
  if (test_srs_toa_num_ta())
    return 1;
  if (test_rx_data_number_of_pdus())
    return 1;
  if (test_rx_data_pdu_length())
    return 1;
  if (test_crc_number_crcs())
    return 1;
  if (test_uci_num_ucis())
    return 1;
  if (test_srs_number_of_pdus())
    return 1;

  printf("All P7 bounds tests passed\n");
  return 0;
}
