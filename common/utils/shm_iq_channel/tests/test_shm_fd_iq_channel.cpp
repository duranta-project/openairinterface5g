/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "gtest/gtest.h"
#include <atomic>
#include <string>
#include <thread>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>

extern "C" {
#include "shm_fd_iq_channel.h"
}

namespace {

const uint64_t first = 1000;
const int n_re_dl = 36, n_re_ul = 24;

// Two clients, each with 2 DL and 1 UL streams
ShmFDIQGeometry geometry()
{
  return ShmFDIQGeometry{.n_re_dl = n_re_dl,
                         .n_re_ul = n_re_ul,
                         .nb_ant_dl = 4,
                         .nb_ant_ul = 2,
                         .ring_symbols = 28,
                         .nb_clients = 2,
                         .nb_msg = 2,
                         .msg_max_size = 64};
}

std::string name()
{
  return "/test_shm_fd_iq_" + std::to_string(getpid()) + "_" + testing::UnitTest::GetInstance()->current_test_info()->name();
}

std::vector<c16_t> pattern(int n, uint64_t symbol, int ant)
{
  std::vector<c16_t> v(n);
  for (int i = 0; i < n; i++)
    v[i] = c16_t{(int16_t)(symbol * 7 + ant * 1000 + i), (int16_t)-i};
  return v;
}

struct Pair {
  ShmFDIQChannel *server;
  ShmFDIQChannel *client;
  explicit Pair(const std::string &n, ShmFDIQGeometry g = geometry())
  {
    server = shm_fd_iq_channel_create(n.c_str(), &g, first);
    std::thread t([&] {
      usleep(20000);
      shm_fd_iq_channel_produce(server, 1);
    });
    client = shm_fd_iq_channel_connect(n.c_str(), 2000);
    t.join();
  }
  ~Pair()
  {
    shm_fd_iq_channel_destroy(client);
    shm_fd_iq_channel_destroy(server);
  }
};

IQChannelErrorType tx(ShmFDIQChannel *ch, uint64_t s, int first_ant, int nb_ant, int n_re)
{
  std::vector<std::vector<c16_t>> d;
  std::vector<const c16_t *> p;
  for (int a = 0; a < nb_ant; a++)
    d.push_back(pattern(n_re, s, first_ant + a));
  for (auto &v : d)
    p.push_back(v.data());
  return shm_fd_iq_channel_tx(ch, s, first_ant, nb_ant, p.data());
}

// rx, then checks the pattern of s (or silence) when there is no error
IQChannelErrorType rx(ShmFDIQChannel *ch, uint64_t s, int first_ant, int nb_ant, int n_re, bool silent = false)
{
  std::vector<std::vector<c16_t>> d(nb_ant, std::vector<c16_t>(n_re));
  std::vector<c16_t *> p;
  for (auto &v : d)
    p.push_back(v.data());
  const IQChannelErrorType ret = shm_fd_iq_channel_rx(ch, s, first_ant, nb_ant, p.data());
  if (ret == CHANNEL_NO_ERROR)
    for (int a = 0; a < nb_ant; a++) {
      const auto ref = silent ? std::vector<c16_t>(n_re) : pattern(n_re, s, first_ant + a);
      EXPECT_EQ(0, memcmp(ref.data(), d[a].data(), n_re * sizeof(c16_t))) << "symbol " << s << " stream " << first_ant + a;
    }
  return ret;
}

TEST(ShmFdIqChannel, DownlinkWindow)
{
  Pair p(name());
  ASSERT_NE(p.client, nullptr);
  const uint64_t c = shm_fd_iq_channel_get_current_symbol(p.server);
  EXPECT_EQ(c, first + 1);
  EXPECT_EQ(tx(p.server, c - 1, 0, 4, n_re_dl), CHANNEL_ERROR_TOO_LATE);
  EXPECT_EQ(tx(p.server, c + 14, 0, 4, n_re_dl), CHANNEL_ERROR_TOO_EARLY);
  for (uint64_t s = c; s < c + 14; s++) {
    if (s != c + 3) {
      EXPECT_EQ(tx(p.server, s, 0, 4, n_re_dl), CHANNEL_NO_ERROR);
    }
  }
  EXPECT_EQ(rx(p.client, c, 0, 2, n_re_dl), CHANNEL_ERROR_TOO_EARLY);
  shm_fd_iq_channel_produce(p.server, 14);
  for (uint64_t s = c; s < c + 14; s++)
    EXPECT_EQ(rx(p.client, s, 0, 4, n_re_dl, s == c + 3), CHANNEL_NO_ERROR) << s;
  EXPECT_EQ(rx(p.client, c, 0, 4, n_re_dl, true), CHANNEL_NO_ERROR); // read once
  shm_fd_iq_channel_produce(p.server, 1);
  EXPECT_EQ(rx(p.client, c, 0, 4, n_re_dl), CHANNEL_ERROR_TOO_LATE);
}

TEST(ShmFdIqChannel, ClientsShareStreams)
{
  Pair p(name());
  std::thread t([&] {
    usleep(20000);
    shm_fd_iq_channel_produce(p.server, 1);
  });
  ShmFDIQChannel *other = shm_fd_iq_channel_connect(name().c_str(), 2000);
  t.join();
  ASSERT_NE(other, nullptr);
  const uint64_t c = shm_fd_iq_channel_get_current_symbol(p.server);
  ASSERT_EQ(tx(p.server, c, 0, 4, n_re_dl), CHANNEL_NO_ERROR);
  ASSERT_EQ(tx(p.client, c, 0, 1, n_re_ul), CHANNEL_NO_ERROR);
  ASSERT_EQ(tx(other, c, 1, 1, n_re_ul), CHANNEL_NO_ERROR);
  shm_fd_iq_channel_produce(p.server, 1);
  EXPECT_EQ(rx(p.client, c, 0, 2, n_re_dl), CHANNEL_NO_ERROR);
  EXPECT_EQ(rx(other, c, 2, 2, n_re_dl), CHANNEL_NO_ERROR);
  EXPECT_EQ(rx(p.server, c, 0, 2, n_re_ul), CHANNEL_NO_ERROR);
  shm_fd_iq_channel_destroy(other);
}

TEST(ShmFdIqChannel, UplinkLateWriteIsDropped)
{
  Pair p(name());
  const uint64_t c = shm_fd_iq_channel_get_current_symbol(p.server);
  EXPECT_EQ(tx(p.client, c, 0, 1, n_re_ul), CHANNEL_NO_ERROR);
  shm_fd_iq_channel_produce(p.server, 2);
  EXPECT_EQ(tx(p.client, c + 1, 0, 1, n_re_ul), CHANNEL_ERROR_TOO_LATE);
  EXPECT_EQ(rx(p.server, c, 0, 1, n_re_ul), CHANNEL_NO_ERROR);
  EXPECT_EQ(rx(p.server, c + 1, 0, 1, n_re_ul, true), CHANNEL_NO_ERROR);
}

TEST(ShmFdIqChannel, Messages)
{
  Pair p(name());
  const uint64_t c = shm_fd_iq_channel_get_current_symbol(p.server);
  const char a[] = "first", b[] = "second";
  EXPECT_EQ(shm_fd_iq_channel_tx_msg(p.client, c, a, sizeof(a)), CHANNEL_NO_ERROR);
  EXPECT_EQ(shm_fd_iq_channel_tx_msg(p.client, c, b, sizeof(b)), CHANNEL_NO_ERROR); // two for one symbol
  EXPECT_EQ(shm_fd_iq_channel_tx_msg(p.client, c + 1, a, sizeof(a)), CHANNEL_ERROR_NO_DATA); // 2 entries
  char out[64];
  size_t len;
  EXPECT_EQ(shm_fd_iq_channel_rx_msg(p.server, c, 0, out, sizeof(out), &len), CHANNEL_ERROR_TOO_EARLY);
  shm_fd_iq_channel_produce(p.server, 1);
  EXPECT_EQ(shm_fd_iq_channel_tx_msg(p.client, c, a, sizeof(a)), CHANNEL_ERROR_TOO_LATE);
  std::vector<std::string> got;
  for (int e = 0; e < 2; e++)
    if (shm_fd_iq_channel_rx_msg(p.server, c, e, out, sizeof(out), &len) == CHANNEL_NO_ERROR)
      got.emplace_back(out, len - 1);
  ASSERT_EQ(got.size(), 2u);
  EXPECT_EQ(got[0], "first");
  EXPECT_EQ(got[1], "second");
  EXPECT_EQ(shm_fd_iq_channel_rx_msg(p.server, c, 0, out, 3, &len), CHANNEL_ERROR_NO_DATA); // no room
  EXPECT_EQ(shm_fd_iq_channel_rx_msg(p.server, c - 1, 0, out, sizeof(out), &len), CHANNEL_ERROR_NO_DATA);
  // Entries free up once the server can no longer read them
  const uint64_t now = shm_fd_iq_channel_get_current_symbol(p.server);
  EXPECT_EQ(shm_fd_iq_channel_tx_msg(p.client, now, a, sizeof(a)), CHANNEL_ERROR_NO_DATA);
  shm_fd_iq_channel_produce(p.server, 14);
  EXPECT_EQ(shm_fd_iq_channel_tx_msg(p.client, now + 14, a, sizeof(a)), CHANNEL_NO_ERROR);
}

TEST(ShmFdIqChannel, ConnectWaitsForAServer)
{
  const std::string n = name();
  const ShmFDIQGeometry g = geometry();
  ShmFDIQChannel *server = nullptr;
  std::atomic<bool> done{false};
  std::thread t([&] {
    usleep(100000);
    server = shm_fd_iq_channel_create(n.c_str(), &g, first);
    while (!done) {
      shm_fd_iq_channel_produce(server, 14);
      usleep(1000);
    }
  });
  ShmFDIQChannel *client = shm_fd_iq_channel_connect(n.c_str(), 3000);
  done = true;
  t.join();
  ASSERT_NE(client, nullptr);
  EXPECT_GT(shm_fd_iq_channel_get_current_symbol(client), first);
  shm_fd_iq_channel_destroy(client);
  shm_fd_iq_channel_destroy(server);
}

TEST(ShmFdIqChannel, ConnectRejectsAStaleServer)
{
  const std::string n = name();
  const ShmFDIQGeometry g = geometry();
  ShmFDIQChannel *stale = shm_fd_iq_channel_create(n.c_str(), &g, first); // never produces
  EXPECT_EQ(shm_fd_iq_channel_connect(n.c_str(), 1500), nullptr);
  std::atomic<bool> done{false};
  ShmFDIQChannel *server = nullptr;
  std::thread t([&] {
    usleep(300000);
    server = shm_fd_iq_channel_create(n.c_str(), &g, 5 * first);
    while (!done) {
      shm_fd_iq_channel_produce(server, 1);
      usleep(1000);
    }
  });
  ShmFDIQChannel *client = shm_fd_iq_channel_connect(n.c_str(), 5000);
  done = true;
  t.join();
  ASSERT_NE(client, nullptr);
  EXPECT_GT(shm_fd_iq_channel_get_current_symbol(client), 5 * first);
  shm_fd_iq_channel_destroy(client);
  shm_fd_iq_channel_destroy(server);
  shm_fd_iq_channel_destroy(stale); // the name is not its own any more: only unmaps
}

TEST(ShmFdIqChannel, Wait)
{
  Pair p(name());
  const uint64_t c = shm_fd_iq_channel_get_current_symbol(p.client);
  EXPECT_EQ(shm_fd_iq_channel_wait(p.client, c, 0), 0);
  EXPECT_EQ(shm_fd_iq_channel_wait(p.client, c + 1, 20000), 1);
  std::thread t([&] {
    usleep(20000);
    shm_fd_iq_channel_produce(p.server, 3);
  });
  EXPECT_EQ(shm_fd_iq_channel_wait(p.client, c + 3, 0), 0);
  t.join();
  std::thread a([&] {
    usleep(20000);
    shm_fd_iq_channel_abort(p.client);
  });
  EXPECT_EQ(shm_fd_iq_channel_wait(p.client, c + 100, 0), -1);
  a.join();
  EXPECT_TRUE(shm_fd_iq_channel_is_stopped(p.client));
  EXPECT_FALSE(shm_fd_iq_channel_is_stopped(p.server));
}

TEST(ShmFdIqChannel, ServerStopReleasesClient)
{
  Pair *p = new Pair(name());
  ShmFDIQChannel *client = p->client;
  p->client = nullptr;
  std::thread t([&] {
    usleep(20000);
    shm_fd_iq_channel_destroy(p->server);
    p->server = nullptr;
  });
  EXPECT_EQ(shm_fd_iq_channel_wait(client, first + 100, 0), -1);
  t.join();
  EXPECT_TRUE(shm_fd_iq_channel_is_stopped(client));
  shm_fd_iq_channel_destroy(client);
  delete p;
}

// Client in a child process: a UL symbol for each DL symbol, as many as the server produces
TEST(ShmFdIqChannel, OtherProcess)
{
  const std::string n = name();
  const ShmFDIQGeometry g = geometry();
  const int count = 2000;
  const pid_t pid = fork();
  if (pid == 0) {
    ShmFDIQChannel *c = shm_fd_iq_channel_connect(n.c_str(), 5000);
    if (!c)
      _exit(2);
    int bad = 0;
    uint64_t s = shm_fd_iq_channel_get_current_symbol(c);
    const uint64_t end = first + count;
    while (s < end && shm_fd_iq_channel_wait(c, s + 1, 1000000) == 0) {
      if (rx(c, s, 0, 2, n_re_dl) != CHANNEL_NO_ERROR)
        bad++;
      tx(c, s + 4, 0, 1, n_re_ul);
      s++;
    }
    shm_fd_iq_channel_destroy(c);
    _exit(bad > count / 20 || s < end ? 1 : 0);
  }
  ShmFDIQChannel *server = shm_fd_iq_channel_create(n.c_str(), &g, first);
  int ul_ok = 0;
  for (uint64_t s = first; s < first + count + 8; s++) {
    tx(server, s, 0, 4, n_re_dl);
    usleep(200);
    shm_fd_iq_channel_produce(server, 1);
    // The client may write late: count the symbols that arrived whole
    std::vector<c16_t> ul(n_re_ul);
    c16_t *dst[1] = {ul.data()};
    const auto ref = pattern(n_re_ul, s, 0);
    if (shm_fd_iq_channel_rx(server, s, 0, 1, dst) == CHANNEL_NO_ERROR && memcmp(ref.data(), ul.data(), sizeof(c16_t) * n_re_ul) == 0)
      ul_ok++;
  }
  int status = 0;
  waitpid(pid, &status, 0);
  shm_fd_iq_channel_destroy(server);
  ASSERT_TRUE(WIFEXITED(status));
  EXPECT_EQ(WEXITSTATUS(status), 0);
  EXPECT_GT(ul_ok, count / 2);
}

} // namespace

int main(int argc, char **argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
