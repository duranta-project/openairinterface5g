/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * Regression tests for independent UL and DL PDCP sequence number lengths
 * (3GPP TS 38.323 clause 6.3.2 & clause 7.2; TS 38.331 PDCP-Config):
 * covers 12/12, 18/18, 18/12 (UL 18-bit, DL 12-bit), and 12/18 (UL 12-bit,
 * DL 18-bit) between UE and gNB PDCP entities.
 */

#include "gtest/gtest.h"
#include <cstring>
#include <vector>

extern "C" {
#include "openair2/LAYER2/nr_pdcp/nr_pdcp_entity.h"
#include "common/utils/LOG/log.h"
}

namespace {

struct delivered_sdu_record {
  std::vector<char> payload;
  uint32_t count;
  int header_size;
};

static void record_delivered_sdu(void *data,
                                 nr_pdcp_entity_t *entity,
                                 char *buf,
                                 int size,
                                 const nr_pdcp_integrity_data_t *msg_integrity)
{
  UNUSED(entity);
  auto *out = static_cast<std::vector<delivered_sdu_record> *>(data);
  delivered_sdu_record rec;
  rec.payload.assign(buf, buf + size);
  rec.count = msg_integrity ? msg_integrity->count : 0;
  rec.header_size = msg_integrity ? msg_integrity->header_size : 0;
  out->push_back(rec);
}

static void noop_deliver_pdu(void *data, ue_id_t ue_id, int rb_id, char *buf, int size, int sdu_id)
{
  UNUSED(data);
  UNUSED(ue_id);
  UNUSED(rb_id);
  UNUSED(buf);
  UNUSED(size);
  UNUSED(sdu_id);
}

static void verify_bidirectional_drb_transfer(int sn_size_ul, int sn_size_dl)
{
  SCOPED_TRACE(::testing::Message() << "UL_SN=" << sn_size_ul << " DL_SN=" << sn_size_dl);

  nr_pdcp_entity_security_keys_and_algos_t sec = {};
  sec.ciphering_algorithm = 2; // NEA2
  sec.integrity_algorithm = 2; // NIA2
  for (int i = 0; i < NR_K_KEY_SIZE; i++) {
    sec.ciphering_key[i] = static_cast<uint8_t>(0x10 + i);
    sec.integrity_key[i] = static_cast<uint8_t>(0x80 + i);
  }

  std::vector<delivered_sdu_record> gnb_rx_sdus;
  std::vector<delivered_sdu_record> ue_rx_sdus;

  nr_pdcp_entity_t *gnb = new_nr_pdcp_entity(NR_PDCP_DRB_AM,
                                             1,
                                             1,
                                             1,
                                             false,
                                             false,
                                             record_delivered_sdu,
                                             &gnb_rx_sdus,
                                             noop_deliver_pdu,
                                             nullptr,
                                             sn_size_ul,
                                             sn_size_dl,
                                             100,
                                             -1,
                                             &sec);

  nr_pdcp_entity_t *ue = new_nr_pdcp_entity(NR_PDCP_DRB_AM,
                                            0,
                                            1,
                                            1,
                                            false,
                                            false,
                                            record_delivered_sdu,
                                            &ue_rx_sdus,
                                            noop_deliver_pdu,
                                            nullptr,
                                            sn_size_ul,
                                            sn_size_dl,
                                            100,
                                            -1,
                                            &sec);

  // Verify directional state constants per TS 38.323 clause 7.2
  EXPECT_EQ(ue->sn_size_tx, sn_size_ul);
  EXPECT_EQ(ue->sn_size_rx, sn_size_dl);
  EXPECT_EQ(ue->window_size, 1 << (sn_size_dl - 1));
  EXPECT_EQ(gnb->sn_size_tx, sn_size_dl);
  EXPECT_EQ(gnb->sn_size_rx, sn_size_ul);
  EXPECT_EQ(gnb->window_size, 1 << (sn_size_ul - 1));

  const char ul_msg[] = "Uplink DRB SDU payload over PDCP";
  char pdu_buf[256] = {};
  int ul_pdu_len = ue->process_sdu(ue, const_cast<char *>(ul_msg), sizeof(ul_msg), 0, pdu_buf, sizeof(pdu_buf));
  const int expected_ul_hdr = (sn_size_ul == SHORT_SN_SIZE) ? SHORT_PDCP_HEADER_SIZE : LONG_PDCP_HEADER_SIZE;
  EXPECT_EQ(ul_pdu_len, static_cast<int>(sizeof(ul_msg)) + expected_ul_hdr + PDCP_INTEGRITY_SIZE);

  gnb->recv_pdu(gnb, pdu_buf, ul_pdu_len);
  ASSERT_EQ(gnb_rx_sdus.size(), 1u);
  EXPECT_EQ(gnb_rx_sdus[0].header_size, expected_ul_hdr);
  ASSERT_EQ(gnb_rx_sdus[0].payload.size(), sizeof(ul_msg));
  EXPECT_EQ(std::memcmp(gnb_rx_sdus[0].payload.data(), ul_msg, sizeof(ul_msg)), 0);

  const char dl_msg[] = "Downlink DRB SDU payload over PDCP";
  std::memset(pdu_buf, 0, sizeof(pdu_buf));
  int dl_pdu_len = gnb->process_sdu(gnb, const_cast<char *>(dl_msg), sizeof(dl_msg), 0, pdu_buf, sizeof(pdu_buf));
  const int expected_dl_hdr = (sn_size_dl == SHORT_SN_SIZE) ? SHORT_PDCP_HEADER_SIZE : LONG_PDCP_HEADER_SIZE;
  EXPECT_EQ(dl_pdu_len, static_cast<int>(sizeof(dl_msg)) + expected_dl_hdr + PDCP_INTEGRITY_SIZE);

  ue->recv_pdu(ue, pdu_buf, dl_pdu_len);
  ASSERT_EQ(ue_rx_sdus.size(), 1u);
  EXPECT_EQ(ue_rx_sdus[0].header_size, expected_dl_hdr);
  ASSERT_EQ(ue_rx_sdus[0].payload.size(), sizeof(dl_msg));
  EXPECT_EQ(std::memcmp(ue_rx_sdus[0].payload.data(), dl_msg, sizeof(dl_msg)), 0);

  ue->delete_entity(ue);
  gnb->delete_entity(gnb);
}

} // namespace

TEST(nr_pdcp_sn_sizes, equal_12_12)
{
  verify_bidirectional_drb_transfer(SHORT_SN_SIZE, SHORT_SN_SIZE);
}

TEST(nr_pdcp_sn_sizes, equal_18_18)
{
  verify_bidirectional_drb_transfer(LONG_SN_SIZE, LONG_SN_SIZE);
}

TEST(nr_pdcp_sn_sizes, asymmetric_ul18_dl12)
{
  verify_bidirectional_drb_transfer(LONG_SN_SIZE, SHORT_SN_SIZE);
}

TEST(nr_pdcp_sn_sizes, asymmetric_ul12_dl18)
{
  verify_bidirectional_drb_transfer(SHORT_SN_SIZE, LONG_SN_SIZE);
}

int main(int argc, char **argv)
{
  logInit();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
