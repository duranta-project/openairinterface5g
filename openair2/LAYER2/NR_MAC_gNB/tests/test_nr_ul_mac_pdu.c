/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* Decoding of UL MAC PDUs (TS 38.321 §6.1.2) by nr_process_mac_pdu(): MAC SDUs handed to RLC, and the UL buffer
 * estimated from Long (Truncated) BSRs (TS 38.321 §6.1.3.1). */

#include <string.h>

#include "common/utils/assertions.h"
#include "LAYER2/NR_MAC_gNB/mac_proto.h"
#include "LAYER2/nr_rlc/nr_rlc_oai_api.h"

// any value not expected after decoding, to check that a BSR overwrote it
#define UL_BUFFER_UNSET 12345

static gNB_MAC_INST mac;
static nr_cell_sched_t cell;
static NR_UE_info_t UE;
static uint8_t buf[64];
static nr_rlc_data_ind_t data_ind[8];
static int num_data_ind;

/* decodes the TB of len octets received from a UE with a DRB on LCID 4 and returns the estimated UL buffer; the
 * octets following the TB in memory are set to 0xff and must not be read */
static int decode(const uint8_t *tb, int len)
{
  DevAssert(len < (int)sizeof(buf));
  memset(buf, 0xff, sizeof(buf));
  memcpy(buf, tb, len);
  memset(&UE, 0, sizeof(UE));
  UE.rnti = 0x1234;
  UE.UE_sched_ctrl.estimated_ul_buffer = UL_BUFFER_UNSET;
  seq_arr_init(&UE.UE_sched_ctrl.lc_config, sizeof(nr_lc_config_t));
  const nr_lc_config_t drb = {.lcid = 4};
  nr_mac_add_lcid(&UE.UE_sched_ctrl, &drb);
  num_data_ind = 0;
  int ret = nr_process_mac_pdu(&mac, &cell, &UE, 0, 0, buf, len, 0, data_ind, &num_data_ind);
  AssertFatal(ret == 0, "nr_process_mac_pdu() returned %d\n", ret);
  seq_arr_free(&UE.UE_sched_ctrl.lc_config, NULL);
  return UE.UE_sched_ctrl.estimated_ul_buffer;
}

/* Buffer sizes from TS 38.321 Table 6.1.3.1-2, one index above the reported one, as the gNB overestimates to account
 * for headers: reported index 10 -> 20 bytes (index 11), reported index 20 -> 38 bytes (index 21). */

void test_ul_mac_pdu_long_bsr(void)
{
  // Long BSR: LCG0 and LCG1, Buffer Size indexes 10 and 20; padding
  const uint8_t tb[] = {0x3e, 3, 0x03, 10, 20, 0x3f, 0, 0};
  const int estim = decode(tb, sizeof(tb));
  AssertFatal(num_data_ind == 0, "%d MAC SDUs, expected none\n", num_data_ind);
  AssertFatal(estim == 20 + 38, "estimated UL buffer %d, expected %d\n", estim, 20 + 38);
}

void test_ul_mac_pdu_sdu_and_long_truncated_bsr(void)
{
  // MAC SDU on LCID 4 (3 octets); Long Truncated BSR: LCG0 and LCG1 have data, room for one Buffer Size field (index
  // 10); padding
  const uint8_t tb[] = {0x04, 3, 0xaa, 0xbb, 0xcc, 0x3c, 2, 0x03, 10, 0x3f, 0, 0};
  const int estim = decode(tb, sizeof(tb));
  AssertFatal(num_data_ind == 1, "%d MAC SDUs, expected 1\n", num_data_ind);
  const nr_rlc_data_ind_t *sdu = &data_ind[0];
  AssertFatal(sdu->ch == 4 && sdu->buf == &buf[2] && sdu->len == 3,
              "MAC SDU LCID %d offset %td length %d, expected LCID 4 offset 2 length 3\n",
              sdu->ch,
              sdu->buf - buf,
              sdu->len);
  AssertFatal(estim == 20, "estimated UL buffer %d, expected 20\n", estim);
}

void test_ul_mac_pdu_long_truncated_bsr_without_buffer_size(void)
{
  // Long Truncated BSR: all LCGs have data, no room for a Buffer Size field; padding
  const uint8_t tb[] = {0x3c, 1, 0xff, 0x3f, 0, 0, 0, 0, 0, 0, 0, 0};
  const int estim = decode(tb, sizeof(tb));
  AssertFatal(estim == 0, "estimated UL buffer %d, expected 0\n", estim);
}

void test_ul_mac_pdu_long_bsr_without_bitmap_at_end_of_tb(void)
{
  // Long BSR: LCG0, Buffer Size index 10; Long BSR with L = 0 as last subPDU, ignored
  const uint8_t tb[] = {0x3e, 2, 0x01, 10, 0x3e, 0};
  const int estim = decode(tb, sizeof(tb));
  AssertFatal(estim == 20, "estimated UL buffer %d, expected 20\n", estim);
}
