/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <gtest/gtest.h>
#include "chanmod_engine.h"
#include "ntn_sim_clock_publisher.h"
#include "openair1/SIMULATION/TOOLS/ntn_sim_clock.h"
#include <cmath>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

extern "C" {
#include "common/config/config_userapi.h"
#include "openair1/SIMULATION/TOOLS/sim.h"
static softmodem_params_t softmodem_params;
softmodem_params_t *get_softmodem_params(void)
{
  return &softmodem_params;
}
void exit_function(const char *file, const char *function, const int line, const char *s, const int assert_flag)
{
  (void)assert_flag;
  fprintf(stderr, "FATAL: %s at %s:%s:%d\n", s, file, function, line);
  exit(EXIT_FAILURE);
}
}

configmodule_interface_t *uniqCfg = NULL;

TEST(ChanmodApplyEffectsTest, DopplerRotation)
{
  c16_t data[2] = {{32767, 0}, {32767, 0}};
  float phase = 0.0f;
  chanmod_apply_effects(data, 2, M_PI_2, &phase, 0.0, -1000.0f);
  EXPECT_NEAR(data[0].r, 32767, 1);
  EXPECT_NEAR(data[0].i, 0, 1);
  EXPECT_NEAR(data[1].r, 0, 1);
  EXPECT_NEAR(data[1].i, 32767, 1);
  EXPECT_NEAR(phase, M_PI, 1e-6);
}

TEST(ChanmodApplyEffectsTest, PathLossScalesMagnitude)
{
  c16_t data[1] = {{32767, 0}};
  // -6.02 dB ~= half amplitude
  chanmod_apply_effects(data, 1, 0.0, nullptr, -6.0206, -1000.0f);
  EXPECT_NEAR(data[0].r, 16383, 5);
}

class ChanmodTxEngineTest : public ::testing::Test {
 protected:
  std::vector<uint64_t> sink_timestamps_;
  std::vector<std::vector<c16_t>> sink_blocks_;

  ChanmodWriteFn RecordingSink()
  {
    return [this](openair0_timestamp_t ts, void **buff, int n, int nant, int) {
      sink_timestamps_.push_back((uint64_t)ts);
      c16_t *samples = static_cast<c16_t *>(buff[0]);
      sink_blocks_.emplace_back(samples, samples + n);
      return n;
    };
  }
};

TEST_F(ChanmodTxEngineTest, DiscontinuousSourceTimestampResetsHistory)
{
  ChanmodTxEngine engine(1, 10);
  engine.start(0);
  channel_desc_t model = {};
  model.noise_power_dB = -1000.0f;
  model.channel_offset = 2;

  c16_t first[4] = {{1, 0}, {2, 0}, {3, 0}, {4, 0}};
  c16_t *samples1[] = {first};
  engine.write(samples1, 1, 4, 0, 0, &model, {}, RecordingSink());

  c16_t second[4] = {{5, 0}, {6, 0}, {7, 0}, {8, 0}};
  c16_t *samples2[] = {second};
  engine.write(samples2, 1, 4, 9, 0, &model, {}, RecordingSink());

  ASSERT_EQ(sink_timestamps_.size(), 2u);
  EXPECT_EQ(sink_timestamps_[1], 9u);
  EXPECT_EQ(sink_blocks_[1][0].r, 0);
  EXPECT_EQ(sink_blocks_[1][1].r, 0);
  EXPECT_EQ(sink_blocks_[1][2].r, 5);
  EXPECT_EQ(sink_blocks_[1][3].r, 6);
  EXPECT_EQ(engine.next_output_timestamp(), 13u);
}

TEST_F(ChanmodTxEngineTest, SmallForwardGapPreservesHistory)
{
  ChanmodTxEngine engine(1, 10);
  engine.start(0);
  channel_desc_t model = {};
  model.noise_power_dB = -1000.0f;
  model.channel_offset = 2;
  std::vector<uint64_t> update_timestamps;
  ChanmodUpdateFn update = [&](uint64_t ts, size_t) { update_timestamps.push_back(ts); };

  c16_t first[4] = {{1, 0}, {2, 0}, {3, 0}, {4, 0}};
  c16_t *samples1[] = {first};
  engine.write(samples1, 1, 4, 0, 0, &model, update, RecordingSink());

  c16_t second[4] = {{5, 0}, {6, 0}, {7, 0}, {8, 0}};
  c16_t *samples2[] = {second};
  engine.write(samples2, 1, 4, 6, 0, &model, update, RecordingSink());

  ASSERT_EQ(sink_timestamps_.size(), 2u);
  EXPECT_EQ(sink_timestamps_[1], 4u);
  EXPECT_EQ(sink_blocks_[1][0].r, 3);
  EXPECT_EQ(sink_blocks_[1][1].r, 4);
  EXPECT_EQ(sink_blocks_[1][2].r, 0);
  EXPECT_EQ(sink_blocks_[1][3].r, 0);
  EXPECT_EQ(engine.next_output_timestamp(), 8u);
  ASSERT_EQ(update_timestamps.size(), 2u);
  EXPECT_EQ(update_timestamps[0], 0u);
  EXPECT_EQ(update_timestamps[1], 4u) << "update() must use deliver_ts_ (output/transport clock)";
}

TEST_F(ChanmodTxEngineTest, DelayDecreaseSelectsShiftedWindowFromRawHistory)
{
  ChanmodTxEngine engine(1, 10);
  engine.start(0);
  channel_desc_t model = {};
  model.noise_power_dB = -1000.0f; // disable noise so this test is bit-exact
  model.channel_offset = 4;

  // T=0, D=4 selects [-4, 0), before any source samples exist.
  c16_t first[4] = {{1, 0}, {2, 0}, {3, 0}, {4, 0}};
  c16_t *samples1[] = {first};
  engine.write(samples1, 1, 4, 0, 0, &model, {}, RecordingSink());
  ASSERT_EQ(sink_blocks_.size(), 1u);
  ASSERT_EQ(sink_blocks_[0].size(), 4u);
  EXPECT_EQ(sink_blocks_[0][0].r, 0);
  EXPECT_EQ(sink_blocks_[0][1].r, 0);
  EXPECT_EQ(sink_blocks_[0][2].r, 0);
  EXPECT_EQ(sink_blocks_[0][3].r, 0);

  // T=4, D=0 selects the newly appended block [4, 8).
  model.channel_offset = 0;
  c16_t second[4] = {{5, 0}, {6, 0}, {7, 0}, {8, 0}};
  c16_t *samples2[] = {second};
  engine.write(samples2, 1, 4, 4, 0, &model, {}, RecordingSink());
  ASSERT_EQ(sink_blocks_.size(), 2u);
  ASSERT_EQ(sink_blocks_[1].size(), 4u);
  EXPECT_EQ(sink_blocks_[1][0].r, 5);
  EXPECT_EQ(sink_blocks_[1][1].r, 6);
  EXPECT_EQ(sink_blocks_[1][2].r, 7);
  EXPECT_EQ(sink_blocks_[1][3].r, 8);
}

TEST_F(ChanmodTxEngineTest, PartialDelayIncreaseWithBacklogSelectsFreshWindow)
{
  // Regression: queued-output replay produced {3,4,3,4} instead of {1,2,3,4} after D=2 -> D=4.
  ChanmodTxEngine engine(1, 10);
  engine.start(0);
  channel_desc_t model = {};
  model.noise_power_dB = -1000.0f; // disable noise so this test is bit-exact
  model.channel_offset = 2;

  // T=0, D=2 selects [-2, 2): two zeros followed by {1,2}.
  c16_t first[4] = {{1, 0}, {2, 0}, {3, 0}, {4, 0}};
  c16_t *samples1[] = {first};
  engine.write(samples1, 1, 4, 0, 0, &model, {}, RecordingSink());
  ASSERT_EQ(sink_blocks_.size(), 1u);
  EXPECT_EQ(sink_blocks_[0][0].r, 0);
  EXPECT_EQ(sink_blocks_[0][1].r, 0);
  EXPECT_EQ(sink_blocks_[0][2].r, 1);
  EXPECT_EQ(sink_blocks_[0][3].r, 2);

  // T=4, D=4 selects the entire first block [0, 4).
  model.channel_offset = 4;
  c16_t second[4] = {{5, 0}, {6, 0}, {7, 0}, {8, 0}};
  c16_t *samples2[] = {second};
  engine.write(samples2, 1, 4, 4, 0, &model, {}, RecordingSink());
  ASSERT_EQ(sink_blocks_.size(), 2u);
  ASSERT_EQ(sink_blocks_[1].size(), 4u);
  EXPECT_EQ(sink_blocks_[1][0].r, 1);
  EXPECT_EQ(sink_blocks_[1][1].r, 2);
  EXPECT_EQ(sink_blocks_[1][2].r, 3);
  EXPECT_EQ(sink_blocks_[1][3].r, 4);
}

class ChanmodRxEngineTest : public ::testing::Test {
 protected:
  size_t reads_ = 0;

  // Fixed-size blocks with distinct sequential values and contiguous timestamps.
  ChanmodReadFn SequentialSource()
  {
    return [this](openair0_timestamp_t *ts, void **buff, int n, int) {
      *ts = (openair0_timestamp_t)(reads_ * n);
      c16_t *out = static_cast<c16_t *>(buff[0]);
      for (int i = 0; i < n; i++)
        out[i] = {(int16_t)(reads_ * n + i + 1), 0};
      reads_++;
      return n;
    };
  }
};

TEST_F(ChanmodRxEngineTest, UpdateUsesOutputDomainTimestampNotSourceTimestamp)
{
  ChanmodRxEngine engine(1, 10);
  engine.start(0);
  channel_desc_t model = {};
  model.noise_power_dB = -1000.0f;
  model.channel_offset = 2;

  std::vector<uint64_t> update_timestamps;
  ChanmodUpdateFn update = [&](uint64_t ts, size_t) { update_timestamps.push_back(ts); };
  ChanmodReadFn partial_source = [this](openair0_timestamp_t *ts, void **buff, int, int) {
    constexpr int chunk_size = 3;
    *ts = reads_ * chunk_size;
    c16_t *samples = static_cast<c16_t *>(buff[0]);
    for (int i = 0; i < chunk_size; ++i)
      samples[i] = {(int16_t)(reads_ * chunk_size + i + 1), 0};
    ++reads_;
    return chunk_size;
  };

  // Two partial source reads advance the source clock to 6 while output advances to 4.
  c16_t out[4];
  c16_t *outs[] = {out};
  engine.read(outs, 1, 4, &model, update, partial_source);
  ASSERT_EQ(update_timestamps.size(), 1u);
  EXPECT_EQ(update_timestamps[0], 0u);

  engine.read(outs, 1, 4, &model, update, partial_source);
  ASSERT_EQ(update_timestamps.size(), 2u);
  EXPECT_EQ(update_timestamps[1], 4u) << "update() must use deliver_ts_ (output/transport clock), not push_ts_ (queue-fill clock)";
}

TEST_F(ChanmodRxEngineTest, DelayDecreaseSelectsFreshWindowWithoutStaleBacklog)
{
  ChanmodRxEngine engine(1, 10);
  engine.start(0);
  channel_desc_t model = {};
  model.noise_power_dB = -1000.0f; // disable noise so this test is bit-exact
  model.channel_offset = 4;

  // Drive delay through the callback to exercise update ordering.
  int call = 0;
  ChanmodUpdateFn update = [&](uint64_t, size_t) { model.channel_offset = (call++ == 0) ? 4 : 0; };

  // T=0, D=4 selects [-4, 0), requiring no source pull.
  c16_t out1[4];
  c16_t *outs1[] = {out1};
  engine.read(outs1, 1, 4, &model, update, SequentialSource());
  EXPECT_EQ(out1[0].r, 0);
  EXPECT_EQ(out1[1].r, 0);
  EXPECT_EQ(out1[2].r, 0);
  EXPECT_EQ(out1[3].r, 0);

  // T=4, D=0 selects [4, 8), requiring two source pulls.
  c16_t out2[4];
  c16_t *outs2[] = {out2};
  engine.read(outs2, 1, 4, &model, update, SequentialSource());
  EXPECT_EQ(out2[0].r, 5);
  EXPECT_EQ(out2[1].r, 6);
  EXPECT_EQ(out2[2].r, 7);
  EXPECT_EQ(out2[3].r, 8);
}

TEST_F(ChanmodRxEngineTest, StartupWithDelayExceedingBlockReturnsZerosWithoutOverPulling)
{
  // Regression: unsigned window-end underflow caused unbounded source pulls for [-8, -4).
  ChanmodRxEngine engine(1, 10);
  engine.start(0);
  channel_desc_t model = {};
  model.noise_power_dB = -1000.0f;
  model.channel_offset = 8;

  c16_t out[4];
  c16_t *outs[] = {out};
  const uint64_t ts = engine.read(outs, 1, 4, &model, {}, SequentialSource());
  EXPECT_EQ(ts, 0u);
  EXPECT_EQ(out[0].r, 0);
  EXPECT_EQ(out[1].r, 0);
  EXPECT_EQ(out[2].r, 0);
  EXPECT_EQ(out[3].r, 0);
  EXPECT_EQ(reads_, 0u);
}

TEST_F(ChanmodRxEngineTest, PartialDelayIncreaseWithBacklogSelectsFreshWindow)
{
  ChanmodRxEngine engine(1, 10);
  engine.start(0);
  channel_desc_t model = {};
  model.noise_power_dB = -1000.0f; // disable noise so this test is bit-exact
  model.channel_offset = 2;

  // T=0, D=2 selects [-2, 2): two zeros followed by {1,2}.
  c16_t out1[4];
  c16_t *outs1[] = {out1};
  engine.read(outs1, 1, 4, &model, {}, SequentialSource());
  EXPECT_EQ(out1[0].r, 0);
  EXPECT_EQ(out1[1].r, 0);
  EXPECT_EQ(out1[2].r, 1);
  EXPECT_EQ(out1[3].r, 2);

  // T=4, D=4 selects [0, 4), already retained in history.
  model.channel_offset = 4;
  c16_t out2[4];
  c16_t *outs2[] = {out2};
  engine.read(outs2, 1, 4, &model, {}, SequentialSource());
  EXPECT_EQ(out2[0].r, 1);
  EXPECT_EQ(out2[1].r, 2);
  EXPECT_EQ(out2[2].r, 3);
  EXPECT_EQ(out2[3].r, 4);
}

TEST(NtnSimClockPublisherTest, ConvertsSamplesToUnixTime)
{
  char name[64];
  snprintf(name, sizeof(name), "/oai_ntn_sim_clock_test_%d", getpid());
  ASSERT_EQ(setenv("OAI_NTN_SIM_CLOCK_SHM", name, 1), 0);

  auto publisher = NtnSimClockPublisher::create(1000.25, 1000.0);
  ASSERT_NE(publisher, nullptr);
  publisher->start(100);
  publisher->publish(350);

  const int fd = shm_open(name, O_RDONLY, 0);
  ASSERT_GE(fd, 0);
  void *mapping = mmap(nullptr, sizeof(ntn_sim_clock_shared_t), PROT_READ, MAP_SHARED, fd, 0);
  ASSERT_NE(mapping, MAP_FAILED);
  const auto *clock = static_cast<const ntn_sim_clock_shared_t *>(mapping);
  EXPECT_EQ(__atomic_load_n(&clock->magic, __ATOMIC_ACQUIRE), NTN_SIM_CLOCK_MAGIC);
  EXPECT_EQ(clock->version, NTN_SIM_CLOCK_VERSION);
  EXPECT_EQ(__atomic_load_n(&clock->unix_time_ns, __ATOMIC_ACQUIRE), 1000500000000ULL);

  munmap(mapping, sizeof(ntn_sim_clock_shared_t));
  close(fd);
  publisher.reset();
  shm_unlink(name);
  unsetenv("OAI_NTN_SIM_CLOCK_SHM");
}

int main(int argc, char **argv)
{
  logInit();
  randominit();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
