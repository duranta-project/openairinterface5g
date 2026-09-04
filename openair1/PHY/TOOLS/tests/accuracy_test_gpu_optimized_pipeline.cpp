/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// Accuracy regression test for the GPU implementation (cuda_channel_pipeline() in
// channel_pipeline_gpu.cu) against the CPU reference implementation (channel_convolution_cpu()).
// For every parameter combination in the same ArgsProduct() grid used by
// benchmark_channel_pipeline.cpp, this test drives a configurable number of independent random
// input stimuli through both implementations and requires the GPU output to match the CPU
// reference to better than 60dB SNR.

#define MAX_SAMPLE_LENGTH (65536)

#include <gtest/gtest.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>
#include <sstream>
#include <tuple>
#include <vector>
#include "oai_cuda.h"
#include "test_channel_pipeline_tools.h"
#include "channel_pipeline.h"
extern "C" {
#include "openair1/SIMULATION/TOOLS/sim.h"
}
configmodule_interface_t *uniqCfg = NULL;

extern "C" void exit_function(const char *file, const char *function, const int line, const char *s, const int assert)
{
  fprintf(stderr, "FATAL: %s at %s:%s:%d\n", s, file, function, line);
  exit(EXIT_FAILURE);
}

// Number of distinct random input stimuli to try per parameter combination. Overridable on the
// command line with -iterations=N / --iterations=N (parsed and stripped before gtest sees argv).
static int g_num_iterations = 1;

static void parse_and_consume_iterations_flag(int &argc, char **argv)
{
  for (int i = 1; i < argc; ++i) {
    const char *val = nullptr;
    if (strncmp(argv[i], "--iterations=", 13) == 0) {
      val = argv[i] + 13;
    } else if (strncmp(argv[i], "-iterations=", 12) == 0) {
      val = argv[i] + 12;
    }
    if (val) {
      g_num_iterations = atoi(val);
      for (int j = i; j < argc - 1; ++j) {
        argv[j] = argv[j + 1];
      }
      --argc;
      return;
    }
  }
}

#ifdef CHANNEL_SIM_CUDA

// generate_random_signal_float() (used by the perf benchmark) draws taps uniformly from
// [-1000, 1000] with no normalization. Summed over nb_tx * channel_length taps per output
// sample, that routinely overflows the int16 range every implementation truncates its output
// into, and the resulting wraparound is hypersensitive to the tiny rounding differences between
// numerically-distinct-but-equivalent implementations (e.g. FFT-based vs direct time-domain
// convolution): a fraction-of-a-unit difference right at a wrap boundary produces an error up to
// 65536 on that sample.
//
// Scaling tap magnitude as K/sqrt(nb_tx * channel_length) keeps the convolution sum's standard
// deviation independent of nb_tx/channel_length (each of the nb_tx*channel_length independent
// per-tap terms has variance ~ K^2/(nb_tx*channel_length), so the N in the N-term sum's variance
// cancels the 1/N from each term - this is the usual CLT normalization). With tx samples uniform
// in [-1000, 1000] (std ~577) and this scaling, the sum's std works out to ~471*K regardless of
// nb_tx/channel_length. K=6.0 targets an output std of ~2800: about 11x below int16 saturation
// (so wraparound is negligible even from many random draws) while still ~80dB above the int16
// quantization noise floor, leaving headroom above the 60dB accuracy bar for real algorithmic
// divergence to actually show up instead of being masked by quantization.
static void generate_scaled_channel_taps(cf_t *sig, int channel_length, int nb_tx)
{
  const float max_tap_mag = 6.0f / std::sqrt((float)nb_tx * (float)channel_length);
  for (int i = 0; i < channel_length; i++) {
    sig[i].r = (((rand() % 2000) - 1000) / 1000.0f) * max_tap_mag;
    sig[i].i = (((rand() % 2000) - 1000) / 1000.0f) * max_tap_mag;
  }
}

// SNR (dB) between the reference and candidate outputs, in the spirit of an accuracy figure of
// merit: 10*log10(reference power / error power). Bit-identical outputs report +inf.
static double compute_snr_db(const std::vector<c16_t *> &ref, const std::vector<c16_t *> &test, int nb_rx, int num_samples)
{
  double signal_power = 0.0;
  double noise_power = 0.0;
  for (int r = 0; r < nb_rx; ++r) {
    for (int i = 0; i < num_samples; ++i) {
      double rr = ref[r][i].r;
      double ri = ref[r][i].i;
      double tr = test[r][i].r;
      double ti = test[r][i].i;
      signal_power += rr * rr + ri * ri;
      double der = rr - tr;
      double dei = ri - ti;
      noise_power += der * der + dei * dei;
    }
  }
  if (noise_power == 0.0) {
    return std::numeric_limits<double>::infinity();
  }
  if (signal_power == 0.0) {
    return 0.0;
  }
  return 10.0 * std::log10(signal_power / noise_power);
}

class GpuAccuracyTest : public ::testing::TestWithParam<std::tuple<int, int, int, int>> {
 protected:
  void SetUp() override
  {
    gpu_ctx = cuda_channel_pipeline_init(MAX_SAMPLE_LENGTH, 64);
  }

  void TearDown() override
  {
    cuda_channel_pipeline_shutdown(gpu_ctx);
  }

  void *gpu_ctx = nullptr;
};

TEST_P(GpuAccuracyTest, MatchesCpuReferenceWithin60dB)
{
  const int nb_rx = std::get<0>(GetParam());
  const int nb_tx = std::get<1>(GetParam());
  const int num_samples = std::get<2>(GetParam());
  const int channel_length = std::get<3>(GetParam());

  // nb_tx=64 and nb_rx>4 are outside the currently supported GPU pipeline configuration;
  // Support all lesser link level configs and skip unsupported configs
  if (nb_tx * nb_rx > (64 * 4)) {
    GTEST_SKIP() << "nb_tx=64 with nb_rx > 4  is not currently supported";
  }
  // Skip any configs where RX is larger than TX as this is not a realistic case
  if (nb_tx < nb_rx) {
    GTEST_SKIP() << "No configs where RX antenna count is > TX antenna count";
  }

  // Noise generation draws from per-call curand state, so enabling it on the GPU side would
  // diverge from the CPU reference (which has no noise generation) for reasons unrelated to
  // convolution accuracy. Disabling it isolates the comparison to the convolution math itself.
  const float noise_power = 0.0f;

  const size_t num_input_samples = num_samples + channel_length - 1;

  std::vector<c16_t *> input(nb_tx);
  for (auto &p : input) {
    p = new c16_t[num_input_samples];
  }

  std::vector<cf_t *> channel(nb_rx * nb_tx);
  for (auto &p : channel) {
    p = new cf_t[channel_length];
  }

  std::vector<c16_t *> cpu_output(nb_rx);
  for (auto &p : cpu_output) {
    p = new c16_t[num_samples];
  }

  std::vector<c16_t *> gpu_output(nb_rx);
  for (auto &p : gpu_output) {
    p = new c16_t[num_samples];
  }

  for (int iter = 0; iter < g_num_iterations; ++iter) {
    for (int i = 0; i < nb_tx; ++i) {
      generate_random_signal(input[i], num_input_samples);
    }
    for (int i = 0; i < nb_rx * nb_tx; ++i) {
      generate_scaled_channel_taps(channel[i], channel_length, nb_tx);
    }

    channel_convolution_cpu((const cf_t **)channel.data(),
                            (const c16_t **)input.data(),
                            nullptr,
                            num_input_samples,
                            cpu_output.data(),
                            nullptr,
                            num_samples,
                            num_samples,
                            channel_length,
                            nb_tx,
                            nb_rx);

    cuda_channel_pipeline(gpu_ctx,
                          (const cf_t **)channel.data(),
                          (const c16_t **)input.data(),
                          nullptr,
                          num_input_samples,
                          gpu_output.data(),
                          nullptr,
                          num_samples,
                          num_samples,
                          channel_length,
                          nb_tx,
                          nb_rx,
                          noise_power);

    double snr_db = compute_snr_db(cpu_output, gpu_output, nb_rx, num_samples);
    printf("channel_pipeline_gpu: nb_rx=%d nb_tx=%d num_samples=%d channel_length=%d iteration=%d snr_db=%f\n",
           nb_rx,
           nb_tx,
           num_samples,
           channel_length,
           iter,
           snr_db);
    EXPECT_GT(snr_db, 60.0) << "channel_pipeline_gpu diverged from the CPU reference implementation: nb_rx=" << nb_rx
                            << " nb_tx=" << nb_tx << " num_samples=" << num_samples << " channel_length=" << channel_length
                            << " iteration=" << iter << " snr_db=" << snr_db;
  }

  for (auto p : input) {
    delete[] p;
  }
  for (auto p : channel) {
    delete[] p;
  }
  for (auto p : cpu_output) {
    delete[] p;
  }
  for (auto p : gpu_output) {
    delete[] p;
  }
}

INSTANTIATE_TEST_SUITE_P(ChannelPipelineGpu,
                         GpuAccuracyTest,
                         ::testing::Combine(::testing::Values(1, 2, 4, 8, 16, 32, 64), // nb_rx
                                            ::testing::Values(1, 2, 4, 8, 16, 32, 64), // nb_tx
                                            ::testing::Values(1024, 15360, 30720), // num_samples
                                            ::testing::Values(8, 16, 32, 64, 128) // channel_length
                                            ),
                         [](const ::testing::TestParamInfo<GpuAccuracyTest::ParamType> &info) {
                           int nb_rx = std::get<0>(info.param);
                           int nb_tx = std::get<1>(info.param);
                           int num_samples = std::get<2>(info.param);
                           int channel_length = std::get<3>(info.param);
                           std::ostringstream name;
                           name << "Rx" << nb_rx << "_Tx" << nb_tx << "_Samples" << num_samples << "_ChanLen" << channel_length;
                           return name.str();
                         });

#endif // CHANNEL_SIM_CUDA

int main(int argc, char **argv)
{
  logInit();
  randominit();
  parse_and_consume_iterations_flag(argc, argv);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
