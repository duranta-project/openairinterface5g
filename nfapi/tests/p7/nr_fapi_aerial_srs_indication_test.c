/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#include "dci_payload_utils.h"
#include "nr_fapi_p7.h"
#include "nr_fapi_p7_utils.h"

// cuBB SRS.indication (scf_fapi_srs_ind_t with SCF_FAPI_10_04_SRS) built byte by byte, as cuBB lays it
// out: codebook reports (tag 1) in the nvIPC data buffer at the offset carried in value, beam-management
// reports (tag 2) unpadded right after a 0xFFFFFFFF value, null reports (report_type 0) with length 0.

enum { TAG_DATA_BUF = 1, TAG_INLINE = 2 };

static uint8_t msg[4096];
static uint8_t data[4096];

typedef struct {
  uint16_t rnti;
  uint8_t report_type;
  uint16_t tag;
  uint32_t length;
  uint32_t value;
  const uint8_t *inline_report;
} srs_pdu_t;

static uint8_t *put_header(int num_pdus)
{
  uint8_t *p = msg, *end = msg + sizeof(msg);
  AssertFatal(push16(100, &p, end) && push16(9, &p, end) && push16(0, &p, end) && push8(num_pdus, &p, end), "\n");
  return p;
}

static void put_pdu(uint8_t **p, const srs_pdu_t *pdu)
{
  uint8_t *end = msg + sizeof(msg);
  AssertFatal(push32(pdu->rnti << 8, p, end) && push16(pdu->rnti, p, end) && push16(31, p, end) && pushs16(-120, p, end)
                  && push8(1, p, end) && push8(pdu->report_type, p, end) && push16(pdu->tag, p, end)
                  && push32(pdu->length, p, end) && push32(pdu->value, p, end),
              "\n");
  if (pdu->inline_report) {
    memcpy(*p, pdu->inline_report, pdu->length);
    *p += pdu->length;
  }
}

static void fill_bytes(uint8_t *buf, int len, uint8_t seed)
{
  for (int i = 0; i < len; i++)
    buf[i] = seed + 7 * i;
}

static bool unpack(uint8_t *msg_end, int data_len, nfapi_nr_srs_indication_t *ind)
{
  uint8_t *r = msg, *d = data;
  return aerial_unpack_nr_srs_indication(&r, msg_end, &d, data + data_len, ind);
}

static void check_report(const nfapi_nr_srs_indication_pdu_t *pdu, uint16_t rnti, const uint8_t *report, uint32_t length)
{
  AssertFatal(pdu->rnti == rnti, "rnti %04x, expected %04x\n", pdu->rnti, rnti);
  AssertFatal(pdu->report_tlv.length == length, "rnti %04x: length %u, expected %u\n", rnti, pdu->report_tlv.length, length);
  // the report words are assembled little-endian, matching the byte order on the wire
  AssertFatal(memcmp(pdu->report_tlv.value, report, length) == 0, "rnti %04x: report differs\n", rnti);
}

// Reports placed out of PDU order: each one must be fetched at its own offset
static void test_codebook_offsets(void)
{
  uint8_t a[24], b[40], c[16];
  fill_bytes(a, sizeof(a), 1);
  fill_bytes(b, sizeof(b), 2);
  fill_bytes(c, sizeof(c), 3);
  memcpy(data, c, sizeof(c));
  memcpy(data + 16, a, sizeof(a));
  memcpy(data + 40, b, sizeof(b));

  uint8_t *p = put_header(3);
  put_pdu(&p, &(srs_pdu_t){.rnti = 0x1001, .report_type = 1, .tag = TAG_DATA_BUF, .length = sizeof(a), .value = 16});
  put_pdu(&p, &(srs_pdu_t){.rnti = 0x1002, .report_type = 1, .tag = TAG_DATA_BUF, .length = sizeof(b), .value = 40});
  put_pdu(&p, &(srs_pdu_t){.rnti = 0x1003, .report_type = 1, .tag = TAG_DATA_BUF, .length = sizeof(c), .value = 0});

  nfapi_nr_srs_indication_t ind = {0};
  AssertFatal(unpack(p, 80, &ind), "unpack failed\n");
  AssertFatal(ind.number_of_pdus == 3, "%d PDUs\n", ind.number_of_pdus);
  check_report(&ind.pdu_list[0], 0x1001, a, sizeof(a));
  check_report(&ind.pdu_list[1], 0x1002, b, sizeof(b));
  check_report(&ind.pdu_list[2], 0x1003, c, sizeof(c));
  free_srs_indication(&ind);
}

// A beam-management report sits inline, unpadded; the PDU after it must still parse
static void test_beam_management_inline(void)
{
  uint8_t bm[23], cb[16];
  fill_bytes(bm, sizeof(bm), 4);
  fill_bytes(cb, sizeof(cb), 5);
  memcpy(data, cb, sizeof(cb));

  uint8_t *p = put_header(2);
  put_pdu(&p,
          &(srs_pdu_t){.rnti = 0x2001,
                       .report_type = 1,
                       .tag = TAG_INLINE,
                       .length = sizeof(bm),
                       .value = 0xFFFFFFFF,
                       .inline_report = bm});
  put_pdu(&p, &(srs_pdu_t){.rnti = 0x2002, .report_type = 1, .tag = TAG_DATA_BUF, .length = sizeof(cb), .value = 0});

  nfapi_nr_srs_indication_t ind = {0};
  AssertFatal(unpack(p, sizeof(cb), &ind), "unpack failed\n");
  AssertFatal(ind.number_of_pdus == 2, "%d PDUs\n", ind.number_of_pdus);
  check_report(&ind.pdu_list[0], 0x2001, bm, sizeof(bm));
  check_report(&ind.pdu_list[1], 0x2002, cb, sizeof(cb));
  free_srs_indication(&ind);
}

// Null reports (e.g. SRS order timeout) carry nothing and must not disturb the other PDUs
static void test_null_reports(void)
{
  uint8_t cb[16];
  fill_bytes(cb, sizeof(cb), 6);
  memcpy(data, cb, sizeof(cb));

  uint8_t *p = put_header(3);
  put_pdu(&p, &(srs_pdu_t){.rnti = 0x3001, .report_type = 0, .tag = TAG_DATA_BUF});
  put_pdu(&p, &(srs_pdu_t){.rnti = 0x3002, .report_type = 1, .tag = TAG_DATA_BUF, .length = sizeof(cb), .value = 0});
  put_pdu(&p, &(srs_pdu_t){.rnti = 0x3003, .report_type = 0, .tag = TAG_INLINE});

  nfapi_nr_srs_indication_t ind = {0};
  AssertFatal(unpack(p, sizeof(cb), &ind), "unpack failed\n");
  AssertFatal(ind.number_of_pdus == 3, "%d PDUs\n", ind.number_of_pdus);
  for (int i = 0; i < 3; i += 2) {
    const nfapi_nr_srs_indication_pdu_t *pdu = &ind.pdu_list[i];
    AssertFatal(pdu->report_type == 0 && pdu->report_tlv.length == 0 && pdu->report_tlv.value[0] == 0,
                "null report %d: report_type %d length %u value[0] %u\n",
                i,
                pdu->report_type,
                pdu->report_tlv.length,
                pdu->report_tlv.value[0]);
  }
  check_report(&ind.pdu_list[1], 0x3002, cb, sizeof(cb));
  free_srs_indication(&ind);
}

static void test_rejected(void)
{
  // report beyond the end of the data buffer
  uint8_t *p = put_header(1);
  put_pdu(&p, &(srs_pdu_t){.rnti = 0x4001, .report_type = 1, .tag = TAG_DATA_BUF, .length = 16, .value = 8});
  nfapi_nr_srs_indication_t ind = {0};
  AssertFatal(!unpack(p, 16, &ind), "report past the data buffer accepted\n");
  free_srs_indication(&ind);

  // offset beyond the end of the data buffer
  p = put_header(1);
  put_pdu(&p, &(srs_pdu_t){.rnti = 0x4002, .report_type = 1, .tag = TAG_DATA_BUF, .length = 4, .value = 1000});
  ind = (nfapi_nr_srs_indication_t){0};
  AssertFatal(!unpack(p, 16, &ind), "offset past the data buffer accepted\n");
  free_srs_indication(&ind);

  // report larger than the value buffer
  p = put_header(1);
  put_pdu(&p,
          &(srs_pdu_t){.rnti = 0x4003,
                       .report_type = 1,
                       .tag = TAG_DATA_BUF,
                       .length = sizeof(ind.pdu_list->report_tlv.value) + 4,
                       .value = 0});
  ind = (nfapi_nr_srs_indication_t){0};
  AssertFatal(!unpack(p, sizeof(data), &ind), "oversized report accepted\n");
  free_srs_indication(&ind);

  // unknown tag carrying a report
  p = put_header(1);
  put_pdu(&p, &(srs_pdu_t){.rnti = 0x4004, .report_type = 1, .tag = 3, .length = 4, .value = 0});
  ind = (nfapi_nr_srs_indication_t){0};
  AssertFatal(!unpack(p, sizeof(data), &ind), "unknown report tag accepted\n");
  free_srs_indication(&ind);
}

int main()
{
  fapi_test_init();
  test_codebook_offsets();
  test_beam_management_inline();
  test_null_reports();
  test_rejected();
  return 0;
}
