/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <gtest/gtest.h>
#include <vector>
#include <tuple>
#include <cmath>
#include <cstdlib>
#include "oai_cuda.h"
#include "test_channel_pipeline_tools.h"
#include "channel_pipeline.h"

#define MAX_SAMPLE_LENGTH (65536)

extern "C" {
#include "openair1/SIMULATION/TOOLS/sim.h"
}
configmodule_interface_t *uniqCfg = NULL;

extern "C" void exit_function(const char *file, const char *function, const int line, const char *s, const int assert)
{
  fprintf(stderr, "FATAL: %s at %s:%s:%d\n", s, file, function, line);
  exit(EXIT_FAILURE);
}

class ChannelConvolutionTest : public ::testing::TestWithParam<std::tuple<int, int, int>> {
 protected:
  void SetUp() override
  {
#ifdef CHANNEL_SIM_CUDA
    gpu_context = cuda_channel_pipeline_init(MAX_SAMPLE_LENGTH, 4);
#endif
    tpool = init_tpool(8);
    channel_pipeline_init(0.0f);
  }

  void TearDown() override
  {
#ifdef CHANNEL_SIM_CUDA
    cuda_channel_pipeline_shutdown(gpu_context);
#endif
    destroy_tpool(tpool);
    channel_pipeline_shutdown();
  }

  void *gpu_context = nullptr;
  void *tpool = nullptr;
};

#ifdef CHANNEL_SIM_CUDA
TEST_P(ChannelConvolutionTest, CompareCpuGpu)
{
  int nb_rx = std::get<0>(GetParam());
  int nb_tx = std::get<1>(GetParam());
  int num_samples = std::get<2>(GetParam());
  int channel_length = 16;

  // nb_tx=64 and nb_rx>4 are outside the currently supported GPU pipeline configuration;
  // Support all lesser link level configs and skip unsupported configs
  if (nb_tx * nb_rx > (64 * 4)) {
    GTEST_SKIP() << "nb_tx=64 with nb_rx > 4  is not currently supported";
  }
  // Skip any configs where RX is larger than TX as this is not a realistic case
  if (nb_tx < nb_rx) {
    GTEST_SKIP() << "No configs where RX antenna count is > TX antenna count";
  }

  // The input buffer must be padded at the beginning to handle the convolution history.
  size_t num_input_samples = num_samples + channel_length - 1;

  // generate_random_signal() draws samples uniformly from [-1000, 1000]. Summed over
  // nb_tx * channel_length taps per output sample, that can overflow the int16 range both
  // implementations truncate their output into; near a wrap boundary, a fraction-of-a-unit
  // rounding difference between the CPU (direct time-domain) and GPU (FFT-based) convolution
  // produces a huge integer delta despite the underlying math being nearly identical. Right-shift
  // the input by 2 bits to cap it below 2^8 so the convolution sum stays well within int16 range.
  constexpr int kInputScaleShift = 2;
  std::vector<c16_t *> input(nb_tx);
  for (int i = 0; i < nb_tx; ++i) {
    input[i] = new c16_t[num_input_samples];
    generate_random_signal(input[i], num_input_samples);
    for (size_t j = 0; j < num_input_samples; ++j) {
      input[i][j].r >>= kInputScaleShift;
      input[i][j].i >>= kInputScaleShift;
    }
  }

  std::vector<cf_t *> channel(nb_rx * nb_tx);
  for (int i = 0; i < nb_rx * nb_tx; ++i) {
    channel[i] = new cf_t[channel_length];
    generate_random_signal_float(channel[i], channel_length);
    for (size_t j = 0; j < channel_length; ++j) {
      channel[i][j].r *= 0.25;
      channel[i][j].i *= 0.25;
    }
  }

  std::vector<c16_t *> output_cpu(nb_rx);
  std::vector<c16_t *> output_gpu(nb_rx);
  for (int i = 0; i < nb_rx; ++i) {
    output_cpu[i] = new c16_t[num_samples];
    output_gpu[i] = new c16_t[num_samples];
    memset(output_cpu[i], 0, num_samples * sizeof(c16_t));
    memset(output_gpu[i], 0, num_samples * sizeof(c16_t));
  }

  // Run CPU implementation
  channel_convolution_cpu((const cf_t **)channel.data(),
                          (const c16_t **)input.data(),
                          nullptr,
                          num_input_samples,
                          output_cpu.data(),
                          nullptr,
                          num_samples,
                          num_samples,
                          channel_length,
                          nb_tx,
                          nb_rx);

  // Run GPU implementation
  cuda_channel_pipeline(gpu_context,
                        (const cf_t **)channel.data(),
                        (const c16_t **)input.data(),
                        nullptr,
                        num_input_samples,
                        output_gpu.data(),
                        nullptr,
                        num_samples,
                        num_samples,
                        channel_length,
                        nb_tx,
                        nb_rx,
                        0.0f);

  // Compare results - increase LSbit limit to 3 as GPU does frequency processing and CPU does time processing
  for (int r = 0; r < nb_rx; ++r) {
    for (int i = 0; i < num_samples; ++i) {
      EXPECT_LE(std::abs(output_cpu[r][i].r - output_gpu[r][i].r), 7) << "Real part mismatch at rx=" << r << " sample=" << i;
      EXPECT_LE(std::abs(output_cpu[r][i].i - output_gpu[r][i].i), 7) << "Imag part mismatch at rx=" << r << " sample=" << i;
    }
  }

  // Cleanup
  for (int i = 0; i < nb_tx; ++i)
    delete[] input[i];
  for (int i = 0; i < nb_rx * nb_tx; ++i)
    delete[] channel[i];
  for (int i = 0; i < nb_rx; ++i) {
    delete[] output_cpu[i];
    delete[] output_gpu[i];
  }
}
#endif

TEST_P(ChannelConvolutionTest, CompareCpuTpool)
{
  int nb_rx = std::get<0>(GetParam());
  int nb_tx = std::get<1>(GetParam());
  int num_samples = std::get<2>(GetParam());
  int channel_length = 16;

  // The input buffer must be padded at the beginning to handle the convolution history.
  size_t num_input_samples = num_samples + channel_length - 1;

  size_t num_input_samples_input0 = num_input_samples;
  size_t num_input_samples_input1 = 0;
  if (num_samples > 5000) {
    num_input_samples_input1 = 5000;
    num_input_samples_input0 = num_input_samples - num_input_samples_input1;
  }
  std::vector<c16_t *> input(nb_tx);
  for (int i = 0; i < nb_tx; ++i) {
    input[i] = new c16_t[num_input_samples_input0];
    generate_random_signal(input[i], num_input_samples_input0);
  }
  std::vector<c16_t *> input2(nb_tx);
  if (num_input_samples_input1 > 0) {
    for (int i = 0; i < nb_tx; i++) {
      input2[i] = new c16_t[num_input_samples_input1];
      generate_random_signal(input2[i], num_input_samples_input1);
    }
  }

  std::vector<cf_t *> channel(nb_rx * nb_tx);
  for (int i = 0; i < nb_rx * nb_tx; ++i) {
    channel[i] = new cf_t[channel_length];
    generate_random_signal_float(channel[i], channel_length);
  }

  std::vector<c16_t *> output_cpu(nb_rx);
  std::vector<c16_t *> output_cpu2(nb_rx);
  std::vector<c16_t *> output_tpool(nb_rx);
  std::vector<c16_t *> output_tpool2(nb_rx);

  size_t num_output_samples_output_0 = num_samples;
  size_t num_output_samples_output_1 = 0;
  if (num_samples > 10000) {
    num_output_samples_output_1 = 10000;
    num_output_samples_output_0 = num_samples - num_output_samples_output_1;
  }

  for (int i = 0; i < nb_rx; ++i) {
    output_cpu[i] = new c16_t[num_output_samples_output_0];
    output_tpool[i] = new c16_t[num_output_samples_output_0];
    memset(output_cpu[i], 0, num_output_samples_output_0 * sizeof(c16_t));
    memset(output_tpool[i], 0, num_output_samples_output_0 * sizeof(c16_t));
  }

  if (num_output_samples_output_1 > 0) {
    for (int i = 0; i < nb_rx; ++i) {
      output_cpu2[i] = new c16_t[num_output_samples_output_1];
      output_tpool2[i] = new c16_t[num_output_samples_output_1];
      memset(output_cpu2[i], 0, num_output_samples_output_1 * sizeof(c16_t));
      memset(output_tpool2[i], 0, num_output_samples_output_1 * sizeof(c16_t));
    }
  }

  // Run CPU implementation
  channel_convolution_cpu((const cf_t **)channel.data(),
                          (const c16_t **)input.data(),
                          num_input_samples_input1 > 0 ? (const c16_t **)input2.data() : nullptr,
                          num_input_samples_input0,
                          output_cpu.data(),
                          num_output_samples_output_1 > 0 ? output_cpu2.data() : nullptr,
                          num_output_samples_output_0,
                          num_samples,
                          channel_length,
                          nb_tx,
                          nb_rx);

  // Run tpool implementation
  channel_pipeline(tpool,
                   (const cf_t **)channel.data(),
                   (const c16_t **)input.data(),
                   num_input_samples_input1 > 0 ? (const c16_t **)input2.data() : nullptr,
                   num_input_samples_input0,
                   output_tpool.data(),
                   num_output_samples_output_1 > 0 ? output_tpool2.data() : nullptr,
                   num_output_samples_output_0,
                   num_samples,
                   channel_length,
                   nb_tx,
                   nb_rx,
                   0.0f);

  // Compare results
  for (int r = 0; r < nb_rx; ++r) {
    for (uint i = 0; i < num_output_samples_output_0; ++i) {
      EXPECT_LE(std::abs(output_cpu[r][i].r - output_tpool[r][i].r), 1) << "Real part mismatch at rx=" << r << " sample=" << i;
      EXPECT_LE(std::abs(output_cpu[r][i].i - output_tpool[r][i].i), 1) << "Imag part mismatch at rx=" << r << " sample=" << i;
    }
  }

  if (num_output_samples_output_1 > 0) {
    for (int r = 0; r < nb_rx; ++r) {
      for (uint i = 0; i < num_output_samples_output_1; ++i) {
        EXPECT_LE(std::abs(output_cpu2[r][i].r - output_tpool2[r][i].r), 1) << "Real part mismatch at rx=" << r << " sample=" << i;
        EXPECT_LE(std::abs(output_cpu2[r][i].i - output_tpool2[r][i].i), 1) << "Imag part mismatch at rx=" << r << " sample=" << i;
      }
    }
  }

  // Cleanup
  for (int i = 0; i < nb_tx; ++i)
    delete[] input[i];
  if (num_input_samples_input1 > 0)
    for (int i = 0; i < nb_tx; i++)
      delete[] input2[i];
  for (int i = 0; i < nb_rx * nb_tx; ++i)
    delete[] channel[i];
  for (int i = 0; i < nb_rx; ++i) {
    delete[] output_cpu[i];
    delete[] output_tpool[i];
  }
  if (num_output_samples_output_1 > 0) {
    for (int i = 0; i < nb_rx; ++i) {
      delete[] output_cpu2[i];
      delete[] output_tpool2[i];
    }
  }
}

INSTANTIATE_TEST_SUITE_P(ChannelConvolutionTests,
                         ChannelConvolutionTest,
                         ::testing::Combine(::testing::Values(1, 2, 4),
                                            ::testing::Values(1, 2, 4),
                                            ::testing::Values(100, 1024, 6000, MAX_SAMPLE_LENGTH)),
                         [](const ::testing::TestParamInfo<ChannelConvolutionTest::ParamType> &info) {
                           int rx = std::get<0>(info.param);
                           int tx = std::get<1>(info.param);
                           int samples = std::get<2>(info.param);
                           std::ostringstream name;
                           name << "Rx" << rx << "_Tx" << tx << "_Samples" << samples;
                           return name.str();
                         });

int main(int argc, char **argv)
{
  logInit();
  randominit();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
