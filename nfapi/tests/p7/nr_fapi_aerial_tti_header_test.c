/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#include "dci_payload_utils.h"
#include "nr_fapi_p7.h"
#include "nr_fapi_p7_utils.h"

// DL_TTI/UL_TTI header layout the Aerial codec writes for cuBB 26.2

static uint8_t buf[1 << 16];

static void fill_dl_tti(nfapi_nr_dl_tti_request_t *req)
{
  req->SFN = 512;
  req->Slot = 7;
  // 2 PDCCH (3 DCIs), 1 PDSCH, 2 CSI-RS, 1 SSB
  const uint16_t types[] = {NFAPI_NR_DL_TTI_PDCCH_PDU_TYPE,
                            NFAPI_NR_DL_TTI_PDSCH_PDU_TYPE,
                            NFAPI_NR_DL_TTI_CSI_RS_PDU_TYPE,
                            NFAPI_NR_DL_TTI_PDCCH_PDU_TYPE,
                            NFAPI_NR_DL_TTI_CSI_RS_PDU_TYPE,
                            NFAPI_NR_DL_TTI_SSB_PDU_TYPE};
  req->dl_tti_request_body.nPDUs = sizeofArray(types);
  for (int i = 0; i < sizeofArray(types); i++)
    req->dl_tti_request_body.dl_tti_pdu_list[i].PDUType = types[i];
  req->dl_tti_request_body.dl_tti_pdu_list[0].pdcch_pdu.pdcch_pdu_rel15.numDlDci = 2;
  req->dl_tti_request_body.dl_tti_pdu_list[3].pdcch_pdu.pdcch_pdu_rel15.numDlDci = 1;
}

static void fill_ul_tti(nfapi_nr_ul_tti_request_t *req)
{
  req->SFN = 1023;
  req->Slot = 19;
  req->rach_present = 1;
  // 1 PRACH, 2 PUSCH, PUCCH F0, F1, F2, F3, F4, 1 SRS
  const struct {
    uint16_t type;
    uint8_t pucch_format;
  } pdus[] = {{NFAPI_NR_UL_CONFIG_PRACH_PDU_TYPE, 0},
              {NFAPI_NR_UL_CONFIG_PUSCH_PDU_TYPE, 0},
              {NFAPI_NR_UL_CONFIG_PUCCH_PDU_TYPE, 0},
              {NFAPI_NR_UL_CONFIG_PUCCH_PDU_TYPE, 2},
              {NFAPI_NR_UL_CONFIG_PUSCH_PDU_TYPE, 0},
              {NFAPI_NR_UL_CONFIG_PUCCH_PDU_TYPE, 1},
              {NFAPI_NR_UL_CONFIG_PUCCH_PDU_TYPE, 3},
              {NFAPI_NR_UL_CONFIG_PUCCH_PDU_TYPE, 4},
              {NFAPI_NR_UL_CONFIG_SRS_PDU_TYPE, 0}};
  req->n_pdus = sizeofArray(pdus);
  for (int i = 0; i < sizeofArray(pdus); i++) {
    req->pdus_list[i].pdu_type = pdus[i].type;
    req->pdus_list[i].pucch_pdu.format_type = pdus[i].pucch_format;
  }
}

static void test_dl_tti(void)
{
  nfapi_nr_dl_tti_request_t *req = calloc_or_fail(1, sizeof(*req));
  fill_dl_tti(req);
  uint8_t *p = buf;
  AssertFatal(pack_dl_tti_request(req, &p, buf + sizeof(buf)), "DL_TTI pack failed\n");

  uint8_t *r = buf;
  uint16_t sfn, slot;
  AssertFatal(pull16(&r, &sfn, p) && pull16(&r, &slot, p), "\n");
  AssertFatal(sfn == 512 && slot == 7, "sfn %d slot %d\n", sfn, slot);
  uint16_t n_pdus, n[5];
  uint8_t n_group;
  AssertFatal(pull16(&r, &n_pdus, p) && pullarray16(&r, n, 5, 5, p) && pull8(&r, &n_group, p), "\n");
  AssertFatal(n_pdus == 6 && n_group == 0, "nPDUs %d nGroup %d\n", n_pdus, n_group);
  const uint16_t want[5] = {2, 1, 2, 1, 3};
  AssertFatal(memcmp(n, want, sizeof(want)) == 0, "nPDUsOfEachType %d %d %d %d %d\n", n[0], n[1], n[2], n[3], n[4]);

  nfapi_nr_dl_tti_request_t *out = calloc_or_fail(1, sizeof(*out));
  uint8_t *u = buf;
  AssertFatal(unpack_dl_tti_request(&u, p, out), "DL_TTI unpack failed\n");
  AssertFatal(u == p, "DL_TTI unpack consumed %td of %td bytes\n", u - buf, p - buf);
  AssertFatal(out->dl_tti_request_body.nPDUs == 6, "unpacked nPDUs %d\n", out->dl_tti_request_body.nPDUs);
  free(out);
  free(req);
}

static void test_ul_tti(void)
{
  nfapi_nr_ul_tti_request_t *req = calloc_or_fail(1, sizeof(*req));
  fill_ul_tti(req);
  uint8_t *p = buf;
  AssertFatal(pack_ul_tti_request(req, &p, buf + sizeof(buf)), "UL_TTI pack failed\n");

  uint8_t *r = buf;
  uint16_t sfn, slot;
  uint8_t n_group;
  AssertFatal(pull16(&r, &sfn, p) && pull16(&r, &slot, p), "\n");
  AssertFatal(sfn == 1023 && slot == 19, "sfn %d slot %d\n", sfn, slot);
  uint16_t n_pdus, n[6];
  AssertFatal(pull16(&r, &n_pdus, p) && pullarray16(&r, n, 6, 6, p) && pull8(&r, &n_group, p), "\n");
  AssertFatal(n_pdus == 9, "nPDUs %d\n", n_pdus);
  const uint16_t want[6] = {1, 2, 2, 3, 1, 0};
  AssertFatal(memcmp(n, want, sizeof(want)) == 0,
              "nPDUsOfEachType %d %d %d %d %d %d\n",
              n[0],
              n[1],
              n[2],
              n[3],
              n[4],
              n[5]);
  AssertFatal(n_group == 0, "nGroup %d\n", n_group);

  // The Aerial UL_TTI PDUs don't round-trip (OAI only packs them for cuBB), so check
  // the header unpack on a message without PDUs
  nfapi_nr_ul_tti_request_t empty = {.SFN = 1, .Slot = 2, .rach_present = 1};
  p = buf;
  AssertFatal(pack_ul_tti_request(&empty, &p, buf + sizeof(buf)), "UL_TTI pack failed\n");
  nfapi_nr_ul_tti_request_t *out = calloc_or_fail(1, sizeof(*out));
  uint8_t *u = buf;
  AssertFatal(unpack_ul_tti_request(&u, p, out), "UL_TTI unpack failed\n");
  AssertFatal(u == p, "UL_TTI unpack consumed %td of %td bytes\n", u - buf, p - buf);
  AssertFatal(p - buf == 19, "UL_TTI header is %td bytes\n", p - buf);
  AssertFatal(out->SFN == 1 && out->Slot == 2 && out->n_pdus == 0 && out->n_group == 0,
              "unpacked sfn %d slot %d nPDUs %d nGroup %d\n",
              out->SFN,
              out->Slot,
              out->n_pdus,
              out->n_group);
  free(out);
  free(req);
}

int main()
{
  fapi_test_init();
  test_dl_tti();
  test_ul_tti();
  return 0;
}
