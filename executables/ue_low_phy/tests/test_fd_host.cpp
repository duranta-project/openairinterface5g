/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * FD host tests: per-client DL rings and their FFT layout, UL slots taken with and without
 * deadlines, PRACH entries, membership, backpressure, a client in another process through
 * shared memory, 3GPP time.
 */

#include "gtest/gtest.h"
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>

extern "C" {
#include "executables/ue_low_phy/fd_host.h"
#include "common/utils/LOG/log.h"
}

namespace {

const int sps = 14;

ue_low_phy_geometry_t geometry(uint32_t max_clients = 4)
{
  return ue_low_phy_geometry_t{.fft_size = 128,
                               .n_rb_dl = 6,
                               .n_rb_ul = 6,
                               .nb_rx = 2,
                               .nb_tx = 1,
                               .symbols_per_slot = sps,
                               .slots_per_frame = 20,
                               .ring_symbols = 40 * sps,
                               .prach_max_samples = 256,
                               .max_clients = max_clients,
                               .dl_per_client = 1,
                               .rx_ring_symbols = 4 * sps};
}

c16_t re(int v)
{
  return c16_t{(int16_t)v, (int16_t)-v};
}

struct Host {
  ue_low_phy_fd_host_t *h;
  ue_low_phy_port_t *port;
  explicit Host(const char *name = nullptr, ue_low_phy_geometry_t g = geometry())
  {
    h = ue_low_phy_fd_host_create(name, &g);
    port = ue_low_phy_fd_host_port(h);
  }
  ~Host()
  {
    ue_low_phy_fd_host_stop(h);
    ue_low_phy_fd_host_destroy(h);
  }
};

// Posts symbols of slot (bit s of used) holding value base + s, then the record.
void post_slot(ue_low_phy_client_t *c, uint64_t slot, uint32_t used, int base, int prach = -1)
{
  std::vector<c16_t> data(6 * 12);
  for (int s = 0; s < sps; s++) {
    if (!(used & (1u << s)))
      continue;
    for (auto &x : data)
      x = re(base + s);
    c16_t *p = data.data();
    ue_low_phy_client_tx_symbol(c, slot * sps + s, &p, 1);
  }
  ue_low_phy_client_slot_done(c, slot, used, prach, 0);
}

TEST(FdHost, DlRingFftShifted)
{
  Host host;
  ue_low_phy_client_t c;
  ASSERT_EQ(ue_low_phy_client_open(host.port, &c), 0);
  ue_low_phy_fd_host_update(host.h);
  ASSERT_TRUE(ue_low_phy_fd_host_client_open(host.h, c.id));
  const uint64_t sym = 3 * sps + 5;
  for (int a = 0; a < 2; a++) {
    c16_t *dst = ue_low_phy_fd_host_dl_symbol(host.h, c.id, a, sym);
    for (int k = 0; k < 72; k++)
      dst[k] = re(1 + k + 100 * a);
  }
  std::vector<c16_t> out0(128, re(999)), out1(128, re(999));
  c16_t *dst[2] = {out0.data(), out1.data()};
  EXPECT_FALSE(ue_low_phy_client_rx_symbol(&c, sym, dst, 2)); // not published
  ue_low_phy_fd_host_publish(host.h, 4 * sps);
  ASSERT_TRUE(ue_low_phy_client_rx_symbol(&c, sym, dst, 2));
  // FFT shifted as nr_symbol_fep() leaves rxdataF: the 72 REs first, zeros after
  for (int a = 0; a < 2; a++) {
    const c16_t *o = dst[a];
    for (int k = 0; k < 72; k++)
      EXPECT_EQ(o[k].r, 1 + k + 100 * a);
    for (int k = 72; k < 128; k++)
      EXPECT_EQ(o[k].r, 0);
  }
}

TEST(FdHost, DlRingsArePerClient)
{
  Host host;
  ue_low_phy_client_t a, b;
  ASSERT_EQ(ue_low_phy_client_open(host.port, &a), 0);
  ASSERT_EQ(ue_low_phy_client_open(host.port, &b), 0);
  for (int k = 0; k < 72; k++) {
    ue_low_phy_fd_host_dl_symbol(host.h, a.id, 0, 0)[k] = re(7);
    ue_low_phy_fd_host_dl_symbol(host.h, b.id, 0, 0)[k] = re(9);
  }
  ue_low_phy_fd_host_publish(host.h, sps);
  std::vector<c16_t> o(128);
  c16_t *dst = o.data();
  ASSERT_TRUE(ue_low_phy_client_rx_symbol(&a, 0, &dst, 1));
  EXPECT_EQ(o[0].r, 7);
  ASSERT_TRUE(ue_low_phy_client_rx_symbol(&b, 0, &dst, 1));
  EXPECT_EQ(o[0].r, 9);
}

TEST(FdHost, DlOverwrittenAfterRing)
{
  Host host;
  ue_low_phy_client_t c;
  ASSERT_EQ(ue_low_phy_client_open(host.port, &c), 0);
  ue_low_phy_fd_host_publish(host.h, 4 * sps);
  std::vector<c16_t> o(128);
  c16_t *dst = o.data();
  EXPECT_TRUE(ue_low_phy_client_rx_symbol(&c, 1 * sps, &dst, 1));
  // The ring holds 4 slots, one of them being overwritten: slot 1 is gone once 5 are published
  ue_low_phy_fd_host_publish(host.h, 5 * sps);
  EXPECT_FALSE(ue_low_phy_client_rx_symbol(&c, 1 * sps, &dst, 1));
  EXPECT_EQ(ue_low_phy_client_wait_slot(&c, 1, 0), -1);
  EXPECT_EQ(ue_low_phy_client_wait_slot(&c, 3, 0), 0);
}

TEST(FdHost, UlSlotTaken)
{
  Host host;
  ue_low_phy_client_t c;
  ASSERT_EQ(ue_low_phy_client_open(host.port, &c), 0);
  ASSERT_TRUE(ue_low_phy_client_join(&c, 2, 0));
  ue_low_phy_fd_host_update(host.h);
  ue_low_phy_fd_ul_slot_t ul;
  ASSERT_TRUE(ue_low_phy_fd_host_take_slot(host.h, c.id, 1, false, &ul));
  EXPECT_FALSE(ul.member); // joined at 2
  ue_low_phy_fd_host_finish_slot(host.h, 1);
  EXPECT_FALSE(ue_low_phy_fd_host_take_slot(host.h, c.id, 2, false, &ul)); // not complete yet
  post_slot(&c, 2, 0b1001, 40);
  ASSERT_TRUE(ue_low_phy_fd_host_take_slot(host.h, c.id, 2, false, &ul));
  EXPECT_TRUE(ul.member);
  EXPECT_EQ(ul.used_symbols, 0b1001u);
  EXPECT_EQ(ul.prach, nullptr);
  EXPECT_EQ(ue_low_phy_fd_host_ul_symbol(host.h, c.id, 0, 2 * sps)[0].r, 40);
  EXPECT_EQ(ue_low_phy_fd_host_ul_symbol(host.h, c.id, 0, 2 * sps + 3)[71].r, 43);
  ue_low_phy_fd_host_finish_slot(host.h, 2);
  EXPECT_EQ(ue_low_phy_client_stats(&c).late_stretches, 0u);
}

TEST(FdHost, DeadlineMakesMissingSlotSilent)
{
  Host host;
  ue_low_phy_client_t c;
  ASSERT_EQ(ue_low_phy_client_open(host.port, &c), 0);
  ASSERT_TRUE(ue_low_phy_client_join(&c, 0, 0));
  ue_low_phy_fd_host_update(host.h);
  ue_low_phy_fd_ul_slot_t ul;
  EXPECT_FALSE(ue_low_phy_fd_host_take_slot(host.h, c.id, 0, false, &ul));
  ASSERT_TRUE(ue_low_phy_fd_host_take_slot(host.h, c.id, 0, true, &ul));
  EXPECT_TRUE(ul.member);
  EXPECT_EQ(ul.used_symbols, 0u);
  ue_low_phy_fd_host_finish_slot(host.h, 0);
  EXPECT_EQ(ue_low_phy_client_stats(&c).late_stretches, 1u);
  // Its late record lands in the ring but is not the record of the slot a ring later
  post_slot(&c, 0, 1, 5);
  const uint64_t later = host.port->mem->ring_slots;
  for (uint64_t s = 1; s < later; s++) {
    post_slot(&c, s, 0, 0);
    ASSERT_TRUE(ue_low_phy_fd_host_take_slot(host.h, c.id, s, false, &ul));
    ue_low_phy_fd_host_finish_slot(host.h, s);
  }
  EXPECT_FALSE(ue_low_phy_fd_host_take_slot(host.h, c.id, later, false, &ul));
}

TEST(FdHost, SkippedSlotsAreComplete)
{
  Host host;
  ue_low_phy_client_t c;
  ASSERT_EQ(ue_low_phy_client_open(host.port, &c), 0);
  ASSERT_TRUE(ue_low_phy_client_join(&c, 0, 0));
  ue_low_phy_fd_host_update(host.h);
  ue_low_phy_client_skip_through(&c, 3);
  ue_low_phy_fd_ul_slot_t ul;
  for (uint64_t s = 0; s <= 3; s++) {
    ASSERT_TRUE(ue_low_phy_fd_host_take_slot(host.h, c.id, s, false, &ul));
    EXPECT_EQ(ul.used_symbols, 0u);
    ue_low_phy_fd_host_finish_slot(host.h, s);
  }
  EXPECT_EQ(ue_low_phy_client_stats(&c).late_stretches, 0u);
}

TEST(FdHost, PrachTakenAndReleased)
{
  Host host;
  ue_low_phy_client_t c;
  ASSERT_EQ(ue_low_phy_client_open(host.port, &c), 0);
  ASSERT_TRUE(ue_low_phy_client_join(&c, 0, 0));
  ue_low_phy_fd_host_update(host.h);
  std::vector<c16_t> bins(139, re(3));
  const int e = ue_low_phy_client_tx_prach(&c, 0, 10, 20, 139, 1, bins.data());
  ASSERT_GE(e, 0);
  post_slot(&c, 0, 0, 0, e);
  ue_low_phy_fd_ul_slot_t ul;
  ASSERT_TRUE(ue_low_phy_fd_host_take_slot(host.h, c.id, 0, false, &ul));
  ASSERT_NE(ul.prach, nullptr);
  EXPECT_EQ(ul.prach->dftlen, 139);
  EXPECT_EQ(ul.prach->time_offset, 10);
  EXPECT_EQ(reinterpret_cast<const c16_t *>(ul.prach + 1)[138].r, 3);
  ue_low_phy_fd_host_finish_slot(host.h, 0);
  EXPECT_EQ(ue_low_phy_port_prach_entry(host.port, c.id, e)->tag, 0u);
  // An entry whose record never came (slot taken at its deadline) is released too
  const int orphan = ue_low_phy_client_tx_prach(&c, 1, 0, 20, 139, 1, bins.data());
  ASSERT_GE(orphan, 0);
  ASSERT_TRUE(ue_low_phy_fd_host_take_slot(host.h, c.id, 1, true, &ul));
  ue_low_phy_fd_host_finish_slot(host.h, 1);
  EXPECT_EQ(ue_low_phy_port_prach_entry(host.port, c.id, orphan)->tag, 0u);
}

TEST(FdHost, LeaveAndClose)
{
  Host host;
  ue_low_phy_client_t a, b;
  ASSERT_EQ(ue_low_phy_client_open(host.port, &a), 0);
  ASSERT_EQ(ue_low_phy_client_open(host.port, &b), 0);
  ASSERT_TRUE(ue_low_phy_client_join(&a, 0, 0));
  ASSERT_TRUE(ue_low_phy_client_join(&b, 0, 0));
  ue_low_phy_fd_host_update(host.h);
  // a leaves after slot 1: its leave returns once the host finished slot 1
  ue_low_phy_client_skip_through(&a, 1);
  EXPECT_FALSE(ue_low_phy_client_leave(&a, 1, 0));
  ue_low_phy_fd_host_update(host.h);
  ue_low_phy_fd_ul_slot_t ul;
  for (uint64_t s = 0; s <= 2; s++) {
    ASSERT_TRUE(ue_low_phy_fd_host_take_slot(host.h, a.id, s, true, &ul));
    EXPECT_EQ(ul.member, s <= 1) << s;
    ASSERT_TRUE(ue_low_phy_fd_host_take_slot(host.h, b.id, s, true, &ul));
    ue_low_phy_fd_host_finish_slot(host.h, s);
  }
  EXPECT_TRUE(ue_low_phy_client_leave(&a, 1, 0));
  EXPECT_EQ(a.mem->state, UE_LOW_PHY_CLIENT_OPEN);
  // b closes while a member: dropped by the next update
  std::thread closer([&] { ue_low_phy_client_close(&b); });
  while (b.mem->state != UE_LOW_PHY_CLIENT_CLOSING)
    usleep(100);
  ue_low_phy_fd_host_update(host.h);
  closer.join();
  EXPECT_EQ(b.mem->state, UE_LOW_PHY_CLIENT_FREE);
  EXPECT_FALSE(ue_low_phy_fd_host_client_open(host.h, b.id));
}

TEST(FdHost, Backpressure)
{
  Host host;
  ue_low_phy_client_t c;
  ASSERT_EQ(ue_low_phy_client_open(host.port, &c), 0);
  ASSERT_TRUE(ue_low_phy_client_join(&c, 0, 0));
  ue_low_phy_fd_host_update(host.h);
  const uint64_t R = host.port->mem->ring_slots;
  EXPECT_TRUE(ue_low_phy_client_try_slot_done(&c, R - 2, 0, -1, 0));
  EXPECT_FALSE(ue_low_phy_client_try_slot_done(&c, R - 1, 0, -1, 0));
  ue_low_phy_fd_ul_slot_t ul;
  ASSERT_TRUE(ue_low_phy_fd_host_take_slot(host.h, c.id, 0, true, &ul));
  ue_low_phy_fd_host_finish_slot(host.h, 0);
  EXPECT_TRUE(ue_low_phy_client_try_slot_done(&c, R - 1, 0, -1, 0));
}

TEST(FdHost, ClientInAnotherProcess)
{
  const std::string name = "test_fd_host_" + std::to_string(getpid());
  Host host(name.c_str());
  ue_low_phy_port_set_time(host.port, ue_low_phy_3gpp_symbol(&host.port->mem->geo, 0, 1000, 3, 0));
  for (int k = 0; k < 72; k++)
    ue_low_phy_fd_host_dl_symbol(host.h, 0, 1, sps + 2)[k] = re(11);
  ue_low_phy_fd_host_publish(host.h, 2 * sps);

  const pid_t pid = fork();
  ASSERT_GE(pid, 0);
  if (pid == 0) {
    // Child: attach, check time and DL, post one UL slot. Exit status = failed check.
    ue_low_phy_port_t *port = ue_low_phy_port_attach(name.c_str(), 2000);
    if (!port)
      _exit(1);
    ue_low_phy_client_t c;
    if (ue_low_phy_client_open(port, &c) != 0 || c.id != 0)
      _exit(2);
    uint64_t symbol0;
    if (!ue_low_phy_client_time(&c, &symbol0, 1000000))
      _exit(3);
    const ue_low_phy_3gpp_time_t t = ue_low_phy_3gpp_time(&port->mem->geo, symbol0 + sps + 2);
    if (t.frame != 1000 || t.slot != 4 || t.symbol != 2)
      _exit(4);
    std::vector<c16_t> a0(128), a1(128);
    c16_t *dst[2] = {a0.data(), a1.data()};
    if (!ue_low_phy_client_rx_symbol(&c, sps + 2, dst, 2) || a1[0].r != 11)
      _exit(5);
    if (!ue_low_phy_client_join(&c, 1, 0))
      _exit(6);
    post_slot(&c, 1, 0b10, 60);
    ue_low_phy_port_destroy(port);
    _exit(0);
  }
  ue_low_phy_fd_ul_slot_t ul;
  bool taken = false;
  for (int i = 0; i < 2000 && !taken; i++) {
    ue_low_phy_fd_host_update(host.h);
    taken = ue_low_phy_fd_host_take_slot(host.h, 0, 1, false, &ul) && ul.member;
    if (!taken)
      usleep(1000);
  }
  int status = -1;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status));
  ASSERT_EQ(WEXITSTATUS(status), 0);
  ASSERT_TRUE(taken);
  EXPECT_EQ(ul.used_symbols, 0b10u);
  EXPECT_EQ(ue_low_phy_fd_host_ul_symbol(host.h, 0, 0, sps + 1)[0].r, 61);
}

TEST(FdHost, AttachTimesOutWithoutHost)
{
  EXPECT_EQ(ue_low_phy_port_attach("test_fd_host_nobody", 50), nullptr);
}

TEST(FdHost, ThreeGppTime)
{
  const ue_low_phy_geometry_t g = geometry();
  const uint64_t s = ue_low_phy_3gpp_symbol(&g, 3, 1023, 19, 13);
  const ue_low_phy_3gpp_time_t t = ue_low_phy_3gpp_time(&g, s);
  EXPECT_EQ(t.hfn, 3u);
  EXPECT_EQ(t.frame, 1023);
  EXPECT_EQ(t.slot, 19);
  EXPECT_EQ(t.symbol, 13);
  const ue_low_phy_3gpp_time_t next = ue_low_phy_3gpp_time(&g, s + 1);
  EXPECT_EQ(next.hfn, 4u);
  EXPECT_EQ(next.frame, 0);
}

} // namespace

int main(int argc, char **argv)
{
  logInit();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
