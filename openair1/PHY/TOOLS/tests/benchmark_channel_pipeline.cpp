/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <algorithm>
#include <sstream>
#include <time.h>
#include <getopt.h>
#include "oai_cuda.h"
#include "common/config/config_userapi.h"
#include <memory>
#include <string>
#include "benchmark/benchmark.h"
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

#define MAX_SAMPLE_LENGTH (65536)

#ifdef CHANNEL_SIM_CUDA
static void BM_channel_convolution_gpu(benchmark::State &state)
{
  int nb_rx = state.range(0);
  int nb_tx = state.range(1);
  int num_samples = state.range(2);
  int channel_length = state.range(3);

  // nb_tx=64 and nb_rx>4 are outside the currently supported GPU pipeline configuration;
  // Support all lesser link level configs and skip unsupported configs
  if (nb_tx * nb_rx > (64 * 4)) {
    state.SkipWithMessage("nb_tx=64 with nb_rx > 4  is not currently supported");
    return;
  }
  // Skip any configs where RX is larger than TX as this is not a realistic case
  if (nb_tx < nb_rx) {
    state.SkipWithMessage("No configs where RX antenna count is > TX antenna count");
    return;
  }

  size_t num_input_samples = num_samples + channel_length - 1;
  std::vector<c16_t *> input(nb_tx);
  for (int i = 0; i < nb_tx; ++i) {
    input[i] = new c16_t[num_input_samples];
  }

  std::vector<c16_t *> output(nb_rx);
  for (int i = 0; i < nb_rx; ++i) {
    output[i] = new c16_t[num_samples];
  }

  std::vector<cf_t *> channel(nb_rx * nb_tx);
  for (int i = 0; i < nb_rx * nb_tx; ++i) {
    channel[i] = new cf_t[channel_length];
  }

  for (int i = 0; i < nb_rx * nb_tx; ++i) {
    generate_random_signal_float(channel[i], channel_length);
  }

  void *gpu_context = cuda_channel_pipeline_init(MAX_SAMPLE_LENGTH, nb_tx, nb_rx, channel_length);

  for (int aatx = 0; aatx < nb_tx; aatx++) {
    generate_random_signal(input[aatx], num_input_samples);
  }

  size_t total_samples = 0;
  for (auto _ : state) {
    cuda_channel_pipeline(gpu_context,
                          (const cf_t **)channel.data(),
                          (const c16_t **)input.data(),
                          nullptr,
                          num_input_samples,
                          output.data(),
                          nullptr,
                          num_samples,
                          num_samples,
                          channel_length,
                          nb_tx,
                          nb_rx,
                          0.0f);
    total_samples += num_samples;
  }
  state.counters["MSPS"] = benchmark::Counter(total_samples / 1000000.f, benchmark::Counter::kIsRate);

  cuda_channel_pipeline_shutdown(gpu_context);
}

#endif

static void BM_channel_convolution_cpu(benchmark::State &state)
{
  int nb_rx = state.range(0);
  int nb_tx = state.range(1);
  int num_samples = state.range(2);
  int channel_length = state.range(3);

  size_t num_input_samples = num_samples + channel_length - 1;
  std::vector<c16_t *> input(nb_tx);
  for (int i = 0; i < nb_tx; ++i) {
    input[i] = new c16_t[num_input_samples];
  }

  std::vector<c16_t *> output(nb_rx);
  for (int i = 0; i < nb_rx; ++i) {
    output[i] = new c16_t[num_samples];
  }

  std::vector<cf_t *> channel(nb_rx * nb_tx);
  for (int i = 0; i < nb_rx * nb_tx; ++i) {
    channel[i] = new cf_t[channel_length];
  }

  for (int i = 0; i < nb_rx * nb_tx; ++i) {
    generate_random_signal_float(channel[i], channel_length);
  }
  for (int aatx = 0; aatx < nb_tx; aatx++) {
    generate_random_signal(input[aatx], num_input_samples);
  }

  size_t total_samples = 0;
  for (auto _ : state) {
    channel_convolution_cpu((const cf_t **)channel.data(),
                            (const c16_t **)input.data(),
                            nullptr,
                            num_input_samples,
                            output.data(),
                            nullptr,
                            num_samples,
                            num_samples,
                            channel_length,
                            nb_tx,
                            nb_rx);
    total_samples += num_samples;
  }
  state.counters["MSPS"] = benchmark::Counter(total_samples / 1000000.0f, benchmark::Counter::kIsRate);

  for (int i = 0; i < nb_tx; ++i)
    delete[] input[i];
  for (int i = 0; i < nb_rx * nb_tx; ++i)
    delete[] channel[i];
  for (int i = 0; i < nb_rx; ++i) {
    delete[] output[i];
  }
}

static void BM_channel_convolution_tpool(benchmark::State &state)
{
  auto method = (channel_pipeline_method_t)state.range(0);
  int nb_rx = state.range(1);
  int nb_tx = state.range(2);
  int num_samples = state.range(3);
  int channel_length = state.range(4);
  channel_pipeline_t *pipeline = channel_pipeline_init(0.0f);
  channel_pipeline_set_method(pipeline, method);
  state.SetLabel(method == CHANNEL_PIPELINE_METHOD_FFT ? "fft" : method == CHANNEL_PIPELINE_METHOD_DIRECT ? "direct" : "auto");

  size_t num_input_samples = num_samples + channel_length - 1;
  std::vector<c16_t *> input(nb_tx);
  for (int i = 0; i < nb_tx; ++i) {
    input[i] = new c16_t[num_input_samples];
  }

  std::vector<c16_t *> output(nb_rx);
  for (int i = 0; i < nb_rx; ++i) {
    output[i] = new c16_t[num_samples];
  }

  std::vector<cf_t *> channel(nb_rx * nb_tx);
  for (int i = 0; i < nb_rx * nb_tx; ++i) {
    channel[i] = new cf_t[channel_length];
  }

  for (int i = 0; i < nb_rx * nb_tx; ++i) {
    generate_random_signal_float(channel[i], channel_length);
  }
  for (int aatx = 0; aatx < nb_tx; aatx++) {
    generate_random_signal(input[aatx], num_input_samples);
  }

  void *tpool = init_tpool(16);
  channel_pipeline_channel_t *pc = channel_pipeline_channel_init();
  channel_pipeline_channel_set(pc, (const cf_t **)channel.data(), nb_tx, nb_rx, channel_length);

  size_t total_samples = 0;
  for (auto _ : state) {
    channel_pipeline(pipeline,
                     tpool,
                     pc,
                     (const c16_t **)input.data(),
                     nullptr,
                     num_input_samples,
                     output.data(),
                     nullptr,
                     num_samples,
                     num_samples,
                     0.0f);
    total_samples += num_samples;
  }
  state.counters["MSPS"] = benchmark::Counter(total_samples / 1000000.f, benchmark::Counter::kIsRate);

  channel_pipeline_channel_free(pc);
  channel_pipeline_shutdown(pipeline);

  destroy_tpool(tpool);

  for (int i = 0; i < nb_tx; ++i)
    delete[] input[i];
  for (int i = 0; i < nb_rx * nb_tx; ++i)
    delete[] channel[i];
  for (int i = 0; i < nb_rx; ++i) {
    delete[] output[i];
  }
}

#ifdef CHANNEL_SIM_CUDA
BENCHMARK(BM_channel_convolution_gpu)
    ->ArgsProduct({
        {1, 2, 4, 8, 16, 64}, // nb_rx
        {1, 2, 4, 8, 16, 64}, // nb_tx
        {1024, 2048, 30720}, // num_samples
        {8, 16, 32, 64}, // channel_length
    })
    ->Iterations(50);
#endif

BENCHMARK(BM_channel_convolution_cpu)
    ->ArgsProduct({
        {1, 2, 4}, // nb_rx
        {1, 2, 4}, // nb_tx
        {1024, 2048, 30720}, // num_samples
        {8, 16, 32, 64}, // channel_length
    })
    ->Iterations(10);

BENCHMARK(BM_channel_convolution_tpool)
    ->ArgsProduct({
        {CHANNEL_PIPELINE_METHOD_AUTO, CHANNEL_PIPELINE_METHOD_DIRECT, CHANNEL_PIPELINE_METHOD_FFT},
        {1, 2, 4}, // nb_rx
        {1, 2, 4}, // nb_tx
        {1024, 2048, 30720, 61440}, // num_samples
        {8, 16, 32, 64, 128}, // channel_length
    })
    ->UseRealTime()
    ->MinTime(0.1);

// 8x2 (rx x tx)
BENCHMARK(BM_channel_convolution_tpool)
    ->ArgsProduct({
        {CHANNEL_PIPELINE_METHOD_AUTO, CHANNEL_PIPELINE_METHOD_DIRECT, CHANNEL_PIPELINE_METHOD_FFT},
        {8}, // nb_rx
        {2}, // nb_tx
        {1024, 2048, 30720, 61440}, // num_samples
        {8, 16, 32, 64, 128}, // channel_length
    })
    ->UseRealTime()
    ->MinTime(0.1);

// 2x8 (rx x tx)
BENCHMARK(BM_channel_convolution_tpool)
    ->ArgsProduct({
        {CHANNEL_PIPELINE_METHOD_AUTO, CHANNEL_PIPELINE_METHOD_DIRECT, CHANNEL_PIPELINE_METHOD_FFT},
        {2}, // nb_rx
        {8}, // nb_tx
        {1024, 2048, 30720, 61440}, // num_samples
        {8, 16, 32, 64, 128}, // channel_length
    })
    ->UseRealTime()
    ->MinTime(0.1);

// --pool_cores=a,b,...: pin pool worker i to core i of the list, else workers float.
static std::vector<std::string> pool_cores;

// One call of the default (auto) method, on a pool of the given size: time per call.
static void BM_channel_pipeline_threads(benchmark::State &state)
{
  const int nb_rx = state.range(0);
  const int nb_tx = state.range(1);
  const int num_samples = state.range(2);
  const int channel_length = state.range(3);
  const int threads = state.range(4);
  channel_pipeline_t *pipeline = channel_pipeline_init(0.0f);

  const size_t num_input_samples = num_samples + channel_length - 1;
  std::vector<std::vector<c16_t>> input(nb_tx, std::vector<c16_t>(num_input_samples));
  std::vector<std::vector<c16_t>> output(nb_rx, std::vector<c16_t>(num_samples));
  std::vector<std::vector<cf_t>> channel(nb_rx * nb_tx, std::vector<cf_t>(channel_length));
  std::vector<const c16_t *> in_ptr(nb_tx);
  std::vector<c16_t *> out_ptr(nb_rx);
  std::vector<const cf_t *> ch_ptr(nb_rx * nb_tx);
  for (int i = 0; i < nb_tx; i++) {
    generate_random_signal(input[i].data(), num_input_samples);
    in_ptr[i] = input[i].data();
  }
  for (int i = 0; i < nb_rx; i++)
    out_ptr[i] = output[i].data();
  for (int i = 0; i < nb_rx * nb_tx; i++) {
    generate_random_signal_float(channel[i].data(), channel_length);
    ch_ptr[i] = channel[i].data();
  }

  void *tpool;
  if (pool_cores.empty()) {
    tpool = init_tpool(threads);
  } else {
    if (threads > (int)pool_cores.size()) {
      state.SkipWithMessage("more threads than --pool_cores");
      channel_pipeline_shutdown(pipeline);
      return;
    }
    std::string cores = pool_cores[0];
    for (int i = 1; i < threads; i++)
      cores += "," + pool_cores[i];
    tpool = init_tpool_cores(cores.c_str());
  }
  channel_pipeline_channel_t *pc = channel_pipeline_channel_init();
  channel_pipeline_channel_set(pc, ch_ptr.data(), nb_tx, nb_rx, channel_length);

  for (auto _ : state)
    channel_pipeline(pipeline, tpool, pc, in_ptr.data(), nullptr, num_input_samples, out_ptr.data(), nullptr, num_samples, num_samples, 0.0f);
  state.counters["MSPS"] = benchmark::Counter((double)num_samples * state.iterations() / 1e6, benchmark::Counter::kIsRate);

  channel_pipeline_channel_free(pc);
  channel_pipeline_shutdown(pipeline);
  destroy_tpool(tpool);
}

// 8T8R O-RU at 273 PRB, 30 kHz SCS (122.88 Msps): one OFDM symbol (4096 + 288 samples) and one
// slot (61440 samples), DL 8 tx to 1 or 2 rx and UL 1 or 2 tx to 8 rx.
BENCHMARK(BM_channel_pipeline_threads)
    ->ArgsProduct({
        {1, 2}, // nb_rx
        {8}, // nb_tx
        {4384, 61440}, // num_samples
        {1, 4, 8, 64, 128}, // channel_length
        {1, 2, 3, 4, 8}, // pool threads
    })
    ->ArgsProduct({
        {8}, // nb_rx
        {1, 2}, // nb_tx
        {4384, 61440}, // num_samples
        {1, 4, 8, 64, 128}, // channel_length
        {1, 2, 3, 4, 8}, // pool threads
    })
    ->Unit(benchmark::kMicrosecond)
    ->UseRealTime()
    ->MinTime(0.2)
    // With --benchmark_repetitions: the fastest repetition is the least disturbed by other load.
    ->ComputeStatistics("min", [](const std::vector<double> &v) { return *std::min_element(v.begin(), v.end()); });

int main(int argc, char **argv)
{
  logInit();
  randominit();
  benchmark::Initialize(&argc, argv);
  for (int i = 1; i < argc; i++) {
    const std::string opt = "--pool_cores=";
    if (std::string(argv[i]).rfind(opt, 0) != 0)
      continue;
    std::stringstream list(argv[i] + opt.size());
    for (std::string core; std::getline(list, core, ',');)
      pool_cores.push_back(core);
  }
  benchmark::RunSpecifiedBenchmarks();
  return 0;
}
