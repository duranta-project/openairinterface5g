/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <string.h>
#include <unistd.h>

#include "common/platform_types.h"
#include "common/utils/LOG/log.h"
#include "common/config/config_userapi.h"
#include "gtp_itf.h"

extern "C" {
#include "openair2/F1AP/f1ap_common.h"
#include "openair2/LAYER2/nr_up/nr_up_ddds_policy.h"
#include "openair2/LAYER2/nr_up/nr_up_f1u.h"
#include "openair2/LAYER2/nr_up/nr_up_flow_control.h"

softmodem_params_t *get_softmodem_params(void)
{
  abort();
  return NULL;
}
}

static instance_t init_gtp(const char *ip, uint16_t port)
{
  openAddr_t iface = {0};
  DevAssert(strlen(ip) <= sizeof(iface.originHost));
  strcpy(iface.originHost, ip);
  snprintf(iface.originService, sizeof(iface.originService), "%d", port);
  snprintf(iface.destinationService, sizeof(iface.destinationService), "%d", port);
  return gtpv1Init(iface);
}

static in_addr_t get_addr(const char *ip)
{
  in_addr_t addr = {0};
  inet_pton(AF_INET, ip, &addr);
  return addr;
}

static transport_layer_addr_t get_tl_addr(const char *ip)
{
  in_addr_t addr = get_addr(ip);
  transport_layer_addr_t tl_addr = {.length = 32};
  memcpy(tl_addr.buffer, &addr, 4);
  return tl_addr;
}

static void tick_ms(int n)
{
  for (int i = 0; i < n; i++)
    nr_up_ms_tick();
}

static void setup_local_drb(ue_id_t ue_id, uint8_t drb_id)
{
  nr_up_init_f1u(get_nr_up_if());
  nr_up_add_drb(ue_id, drb_id);
}

struct f1_gtp_pair {
  instance_t ep_rx;
  instance_t ep_tx;
  uint32_t ue_id;
  int bearer_id;
};

/* Per-test expect for the shared F1-U RX callback (PDU check and optional DDDS reply). */
struct f1_rx_expect {
  ue_id_t ue_id;
  rb_id_t drb_id;
  sdu_size_t size;
  const uint8_t *pdu;
  bool reply_ddds;
  uint32_t ddds_dbs;
  int count;
};

static f1_gtp_pair *g_f1;
static f1_rx_expect g_rx;
static int g_ddds_ind_count;

static bool recv_f1_dl(protocol_ctxt_t *ctxt,
                       const srb_flag_t srb_flagP,
                       const rb_id_t rb_idP,
                       const mui_t muiP,
                       const confirm_t confirmP,
                       const sdu_size_t size,
                       unsigned char *const buf,
                       const pdcp_transmission_mode_t modeP,
                       const uint32_t *sourceL2Id,
                       const uint32_t *destinationL2Id,
                       bool is_poll);

static f1_gtp_pair setup_f1_gtp(uint32_t ue_id, int bearer_id, uint16_t port, gtpDddsCallback ddds_cb = nullptr)
{
  const char *ip_rx = "127.0.0.1";
  const char *ip_tx = "127.0.0.2";
  f1_gtp_pair p = {};
  p.ue_id = ue_id;
  p.bearer_id = bearer_id;
  p.ep_rx = init_gtp(ip_rx, port);
  p.ep_tx = init_gtp(ip_tx, port);
  DevAssert(p.ep_rx >= 1 && p.ep_tx >= 1);

  transport_layer_addr_t null_addr = {.length = 32};
  teid_t t_rx = newGtpuCreateTunnel(p.ep_rx, ue_id, bearer_id, bearer_id, -1, null_addr, recv_f1_dl, NULL, NULL, NULL);
  transport_layer_addr_t tl_rx = get_tl_addr(ip_rx);
  teid_t t_tx = newGtpuCreateTunnel(p.ep_tx, ue_id, bearer_id, bearer_id, t_rx, tl_rx, NULL, NULL, NULL, ddds_cb);
  GtpuUpdateTunnelOutgoingAddressAndTeid(p.ep_rx, ue_id, bearer_id, get_addr(ip_tx), t_tx);

  createF1inst(0, NULL, NULL);
  getCxt(0)->gtpInst = p.ep_tx;
  nr_up_init_f1u(get_nr_up_if());
  return p;
}

static void teardown_f1_gtp(f1_gtp_pair *p)
{
  destroyF1inst(0);
  EXPECT_EQ(newGtpuDeleteAllTunnels(p->ep_rx, p->ue_id), 0);
  EXPECT_EQ(newGtpuDeleteAllTunnels(p->ep_tx, p->ue_id), 0);
  EXPECT_EQ(gtpv1Term(p->ep_rx), 0);
  EXPECT_EQ(gtpv1Term(p->ep_tx), 0);
}

static bool recv_f1_dl(protocol_ctxt_t *ctxt,
                       const srb_flag_t srb_flagP,
                       const rb_id_t rb_idP,
                       const mui_t muiP,
                       const confirm_t confirmP,
                       const sdu_size_t size,
                       unsigned char *const buf,
                       const pdcp_transmission_mode_t modeP,
                       const uint32_t *sourceL2Id,
                       const uint32_t *destinationL2Id,
                       bool is_poll)
{
  UNUSED(srb_flagP);
  UNUSED(muiP);
  UNUSED(confirmP);
  UNUSED(modeP);
  UNUSED(sourceL2Id);
  UNUSED(destinationL2Id);
  EXPECT_EQ(ctxt->rntiMaybeUEid, g_rx.ue_id);
  EXPECT_EQ(rb_idP, g_rx.drb_id);
  EXPECT_EQ(size, g_rx.size);
  EXPECT_TRUE(is_poll);
  if (g_rx.pdu != NULL && size > 0) {
    EXPECT_EQ(memcmp(buf, g_rx.pdu, size), 0);
  }
  if (g_rx.reply_ddds) {
    DevAssert(g_f1 != NULL);
    /* Stand in for nr_up_f1u_ddds without linking nr_up_du (that pulls RLC). */
    gtpv1uSendDirect(g_f1->ep_rx,
                     ctxt->rntiMaybeUEid,
                     rb_idP,
                     NULL,
                     0,
                     false,
                     false,
                     true,
                     g_rx.ddds_dbs);
  }
  g_rx.count++;
  return true;
}

static void test_ddds_indication(ue_id_t ue_id, rb_id_t drb_id, uint32_t dbs)
{
  EXPECT_EQ(ue_id, g_rx.ue_id);
  EXPECT_EQ(drb_id, g_rx.drb_id);
  EXPECT_EQ(dbs, g_rx.ddds_dbs);
  g_ddds_ind_count++;
  nr_up_f1u_ddds_indication(ue_id, drb_id, dbs);
}

/* DRB never synced: first precheck ALLOWs with Report Polling, then a second
 * precheck ALLOWs without RP once the poll window is armed. */
TEST(nrup, flow_control_allow_rp_never_synced)
{
  const ue_id_t ue_id = 1;
  const uint8_t drb_id = 1;
  setup_local_drb(ue_id, drb_id);
  tick_ms(20);

  nr_up_user_data_req_t req = {.ue_id = ue_id, .drb_id = drb_id, .pdu_max_size = 100};
  EXPECT_EQ(nr_up_flow_control_precheck(&req), NR_UP_FLOW_ALLOW_RP);
  EXPECT_EQ(nr_up_flow_control_precheck(&req), NR_UP_FLOW_ALLOW);

  nr_up_release_drb(ue_id, drb_id);
}

/* Synced Desired Buffer Size is 0: precheck DROPs and nr_up_f1u still sends
 * an empty G-PDU with Report Polling so the peer can refresh DDDS. */
TEST(nrup, flow_control_drop_poll)
{
  const ue_id_t ue_id = 2;
  const uint8_t drb_id = 1;
  g_rx = {.ue_id = ue_id, .drb_id = drb_id, .size = 0};
  f1_gtp_pair gtp = setup_f1_gtp(ue_id, drb_id, 4569);
  nr_up_add_drb(ue_id, drb_id);
  nr_up_sync_dbs(ue_id, drb_id, 0);
  tick_ms(20);

  nr_up_user_data_req_t req = {.ue_id = ue_id, .drb_id = drb_id, .pdu_max_size = 100};
  EXPECT_EQ(nr_up_flow_control_precheck(&req), NR_UP_FLOW_DROP);
  usleep(100 * 1000);
  EXPECT_EQ(g_rx.count, 1);

  nr_up_release_drb(ue_id, drb_id);
  teardown_f1_gtp(&gtp);
}

/* Synced non-zero Desired Buffer Size that fits the PDU: precheck ALLOWs.
 * Includes a large DBS so the soft-drop threshold cannot wrap uint32. */
TEST(nrup, flow_control_allow_synced)
{
  const uint8_t drb_id = 1;
  const uint32_t dbs_values[] = {10000, 50000000};
  for (size_t i = 0; i < sizeof(dbs_values) / sizeof(dbs_values[0]); i++) {
    const ue_id_t ue_id = 3 + i;
    setup_local_drb(ue_id, drb_id);
    nr_up_sync_dbs(ue_id, drb_id, dbs_values[i]);

    nr_up_user_data_req_t req = {.ue_id = ue_id, .drb_id = drb_id, .pdu_max_size = 100};
    EXPECT_EQ(nr_up_flow_control_precheck(&req), NR_UP_FLOW_ALLOW) << "dbs=" << dbs_values[i];

    nr_up_release_drb(ue_id, drb_id);
  }
}

/* nr_up_ddds_precheck (TS 38.425 clause 5.4.2.1): Report Polling, DBS hit/leave 0,
 * and timer-based refresh when Desired Buffer Size changed. */
TEST(nrup, ddds_precheck_poll_and_zero)
{
  nr_up_ddds_t st = {0};
  EXPECT_TRUE(nr_up_ddds_precheck(&st, 1000, true));
  EXPECT_TRUE(st.has_last_report);
  EXPECT_EQ(st.last_desired_buffer_size, 1000u);

  EXPECT_FALSE(nr_up_ddds_precheck(&st, 1000, false));

  EXPECT_TRUE(nr_up_ddds_precheck(&st, 0, false));
  EXPECT_EQ(st.last_desired_buffer_size, 0u);

  EXPECT_TRUE(nr_up_ddds_precheck(&st, 500, false));
  EXPECT_EQ(st.last_desired_buffer_size, 500u);

  EXPECT_FALSE(nr_up_ddds_precheck(&st, 500, false));
  EXPECT_FALSE(nr_up_ddds_precheck(&st, 600, false));
  tick_ms(20);
  EXPECT_TRUE(nr_up_ddds_precheck(&st, 600, false));
}

/* CU deliver_pdu_drb sends the G-PDU on F1-U with Report Polling set; the
 * DU-side GTP callback must receive the PDU and see is_poll. */
TEST(nrup, f1_dl_deliver)
{
  const uint32_t ue_id = 21;
  const uint8_t drb_id = 1;
  uint8_t pdu[3] = {0x11, 0x22, 0x33};
  g_rx = {.ue_id = ue_id, .drb_id = drb_id, .size = 3, .pdu = pdu};
  f1_gtp_pair gtp = setup_f1_gtp(ue_id, drb_id, 4568);
  nr_up_add_drb(ue_id, drb_id);
  nr_up_sync_dbs(ue_id, drb_id, 65535);

  nr_up_user_data_req_t req = {
      .ue_id = ue_id,
      .drb_id = drb_id,
      .pdcp_sn = 7,
      .pdu_max_size = sizeof(pdu),
      .pdu = {.len = sizeof(pdu), .buf = pdu},
      .report_polling = true,
  };
  nr_up_deliver_pdu_drb(&req);

  usleep(100 * 1000);
  EXPECT_EQ(g_rx.count, 1);

  nr_up_release_drb(ue_id, drb_id);
  teardown_f1_gtp(&gtp);
}

/* CU TX (gtpv1uSendDirectWithNRUSeqNum) does not set Report Delivered yet, so this test
 * injects GTP-U + NR-UP DL USER DATA with that IE. The receiver then sends DL DATA DELIVERY STATUS. */
TEST(nrup, f1_ddds_on_report_polling)
{
  const uint32_t ue_id = 20;
  const uint8_t drb_id = 1;
  uint8_t pdu[3] = {0x01, 0x02, 0x03};
  g_rx = {.ue_id = ue_id, .drb_id = drb_id, .size = 3, .pdu = pdu, .reply_ddds = true, .ddds_dbs = 4096};
  g_ddds_ind_count = 0;
  f1_gtp_pair gtp = setup_f1_gtp(ue_id, drb_id, 4570, test_ddds_indication);
  g_f1 = &gtp;
  nr_up_add_drb(ue_id, drb_id);
  nr_up_sync_dbs(ue_id, drb_id, 65535);

  nr_up_user_data_req_t req = {
      .ue_id = ue_id,
      .drb_id = drb_id,
      .pdu_max_size = sizeof(pdu),
      .pdu = {.len = sizeof(pdu), .buf = pdu},
      .report_polling = true,
  };
  nr_up_deliver_pdu_drb(&req);

  usleep(100 * 1000);
  EXPECT_EQ(g_rx.count, 1);
  EXPECT_EQ(g_ddds_ind_count, 1);

  nr_up_release_drb(ue_id, drb_id);
  g_f1 = NULL;
  teardown_f1_gtp(&gtp);
}

int main(int argc, char **argv)
{
  logInit();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
