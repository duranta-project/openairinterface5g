/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * channel_pipeline_gpu.cu: overlap-add block convolution with a fixed 256-point FFT.
 *
 * Faster implementation that direct conv for higher TX antenna count and/or longer CIR length
 * Fixed size FFT size is utilized to account for range of CIR up to 128 samples without massive cycle tradeoff
 * Init method sized for worst case at runtime based on #samples, with fixed 64TX antennas
 *
 */

#include "oai_cuda.h"
#include <cstdint>
#include <cstdio>
#include <cuda_runtime.h>
#include <cufft.h>
#include <nvtx3/nvToolsExt.h>
#include "common/platform_types.h"
#include "common/utils/assertions.h"

#define CHECK_CUDA(val)                                                                          \
  {                                                                                              \
    if (val != cudaSuccess) {                                                                    \
      fprintf(stderr, "CUDA Error at %s:%d: %s\n", __FILE__, __LINE__, cudaGetErrorString(val)); \
      exit(EXIT_FAILURE);                                                                        \
    }                                                                                            \
  }

#define CHECK_CUFFT(val)                                                           \
  {                                                                                \
    if (val != CUFFT_SUCCESS) {                                                    \
      fprintf(stderr, "cuFFT Error at %s:%d: %d\n", __FILE__, __LINE__, (int)val); \
      exit(EXIT_FAILURE);                                                          \
    }                                                                              \
  }

// Fixed FFT block size for the overlap-add decomposition. channel_length must stay below this
// (checked below) so that the per-block signal length B = GPU_FFT_SIZE - channel_length + 1
// stays positive.
#define GPU_FFT_SIZE 256

// Worst-case bounds every GPU buffer and cuFFT plan is sized for, once, at init. A call whose
// num_samples/nb_tx/nb_rx/channel_length exceeds one of these aborts via AssertFatal rather than
// silently reallocating/replanning.
#define GPU_MAX_SAMPLES 65536
#define GPU_MAX_NUM_TX_ANT 64
#define GPU_MAX_NUM_RX_ANT 64
// Both TX and RX can independently reach GPU_MAX_NUM_TX_ANT/GPU_MAX_NUM_RX_ANT (e.g. 4 TX on the UE
// side into 64 RX on the gNB side), but sizing chan_time/chan_freq/chan_plan for the full
// GPU_MAX_NUM_TX_ANT * GPU_MAX_NUM_RX_ANT cross product is wasteful; cap the total number of
// simultaneous tx*rx channel links instead.
#define GPU_MAX_NUM_LINKS (GPU_MAX_NUM_TX_ANT * 4)
// Practical cap on channel taps for the fixed-size buffers below; raise if a channel model needs
// a longer impulse response (this grows GPU_MAX_NUM_BLOCKS, and with it every FFT buffer, as
// GPU_MAX_CHANNEL_LENGTH shrinks the per-block window GPU_MIN_BLOCK_LEN).
#define GPU_MAX_CHANNEL_LENGTH 128
// Smallest possible per-block signal length, i.e. the block length at channel_length ==
// GPU_MAX_CHANNEL_LENGTH, which yields the largest possible num_blocks for GPU_MAX_SAMPLES.
#define GPU_MIN_BLOCK_LEN (GPU_FFT_SIZE - GPU_MAX_CHANNEL_LENGTH + 1)
#define GPU_MAX_NUM_BLOCKS (((GPU_MAX_SAMPLES + GPU_MAX_CHANNEL_LENGTH - 1) + GPU_MIN_BLOCK_LEN - 1) / GPU_MIN_BLOCK_LEN)
// Upper bound on accum_len = (num_blocks - 1) * block_len + GPU_FFT_SIZE for any valid
// (num_samples, channel_length): (num_blocks - 1) * block_len < x_len = num_samples +
// channel_length - 1, so accum_len < num_samples + channel_length - 1 + GPU_FFT_SIZE.
#define GPU_MAX_ACCUM_LEN (GPU_MAX_SAMPLES + GPU_MAX_CHANNEL_LENGTH - 1 + GPU_FFT_SIZE)

__global__ void gather_zero_pad_tx_block_kernel(const c16_t *const *__restrict__ tx_sig0,
                                                const c16_t *const *__restrict__ tx_sig1,
                                                int num_samples_tx_sig0,
                                                int x_len,
                                                int block_len,
                                                cufftComplex *__restrict__ tx_time)
{
  int aatx = blockIdx.y;
  int block = blockIdx.z;
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= GPU_FFT_SIZE)
    return;
  cufftComplex val = {0.0f, 0.0f};
  if (idx < block_len) {
    int sample = block * block_len + idx;
    if (sample < x_len) {
      c16_t s;
      if (sample < num_samples_tx_sig0) {
        s = tx_sig0[aatx][sample];
      } else {
        s = tx_sig1[aatx][sample - num_samples_tx_sig0];
      }
      val.x = (float)s.r;
      val.y = (float)s.i;
    }
  }
  tx_time[(aatx * GPU_MAX_NUM_BLOCKS + block) * GPU_FFT_SIZE + idx] = val;
}

// blockIdx.y indexes `channel` in the caller's link order (rx * nb_tx + aatx, matching
// freq_multiply_accumulate_block_kernel below). chan_time/chan_freq/chan_plan are sized for
// GPU_MAX_NUM_LINKS total links (not the GPU_MAX_NUM_TX_ANT * GPU_MAX_NUM_RX_ANT cross product), so
// this writes at the contiguous caller_link position rather than a fixed max-antenna stride;
// cuda_channel_pipeline() asserts nb_tx * nb_rx <= GPU_MAX_NUM_LINKS to keep this in bounds.
__global__ void gather_zero_pad_channel_kernel(const cf_t *const *__restrict__ channel,
                                               int channel_length,
                                               int nb_tx,
                                               cufftComplex *__restrict__ chan_time)
{
  int caller_link = blockIdx.y;
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= GPU_FFT_SIZE)
    return;
  cufftComplex val = {0.0f, 0.0f};
  if (idx < channel_length) {
    cf_t c = channel[caller_link][idx];
    val.x = c.r;
    val.y = c.i;
  }
  chan_time[caller_link * GPU_FFT_SIZE + idx] = val;
}

__global__ void freq_multiply_accumulate_block_kernel(const cufftComplex *__restrict__ tx_freq,
                                                      const cufftComplex *__restrict__ chan_freq,
                                                      int nb_tx,
                                                      cufftComplex *__restrict__ rx_freq_accum)
{
  int rx = blockIdx.y;
  int block = blockIdx.z;
  int bin = blockIdx.x * blockDim.x + threadIdx.x;
  if (bin >= GPU_FFT_SIZE)
    return;
  cufftComplex acc = {0.0f, 0.0f};
  for (int aatx = 0; aatx < nb_tx; aatx++) {
    cufftComplex a = tx_freq[(aatx * GPU_MAX_NUM_BLOCKS + block) * GPU_FFT_SIZE + bin];
    int link = rx * nb_tx + aatx;
    cufftComplex b = chan_freq[link * GPU_FFT_SIZE + bin];
    acc.x += a.x * b.x - a.y * b.y;
    acc.y += a.x * b.y + a.y * b.x;
  }
  rx_freq_accum[(rx * GPU_MAX_NUM_BLOCKS + block) * GPU_FFT_SIZE + bin] = acc;
}

// Overlap-add: each block's inverse-FFT result covers GPU_FFT_SIZE samples of the full linear
// convolution starting at block * block_len, and adjacent blocks overlap by
// channel_length - 1 = GPU_FFT_SIZE - block_len samples, so this must add rather than overwrite.
__global__ void scatter_add_block_kernel(const cufftComplex *__restrict__ rx_time,
                                         int block_len,
                                         int accum_len,
                                         cufftComplex *__restrict__ rx_accum)
{
  int rx = blockIdx.y;
  int block = blockIdx.z;
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= GPU_FFT_SIZE)
    return;
  int pos = block * block_len + idx;
  if (pos >= accum_len)
    return;
  cufftComplex v = rx_time[(rx * GPU_MAX_NUM_BLOCKS + block) * GPU_FFT_SIZE + idx];
  float scale = 1.0f / (float)GPU_FFT_SIZE;
  atomicAdd(&rx_accum[rx * accum_len + pos].x, v.x * scale);
  atomicAdd(&rx_accum[rx * accum_len + pos].y, v.y * scale);
}

__global__ void extract_and_noise_block_kernel(const cufftComplex *__restrict__ rx_accum,
                                               int accum_len,
                                               int window_offset,
                                               c16_t *__restrict__ *rx_sig0,
                                               c16_t *__restrict__ *rx_sig1,
                                               int num_samples_rx_sig0,
                                               int num_samples,
                                               float noise_power,
                                               curandState_t *curand_states)
{
  int rx = blockIdx.y;
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= num_samples)
    return;
  cufftComplex v = rx_accum[rx * accum_len + (window_offset + i)];
  float r = v.x;
  float im = v.y;
  if (noise_power > 0.0f) {
    curandState_t local_state = curand_states[rx * num_samples + i];
    float2 awgn = curand_normal2(&local_state);
    r += awgn.x * noise_power;
    im += awgn.y * noise_power;
    curand_states[rx * num_samples + i] = local_state;
  }
  if (i < num_samples_rx_sig0) {
    rx_sig0[rx][i].r = r;
    rx_sig0[rx][i].i = im;
  } else {
    rx_sig1[rx][i - num_samples_rx_sig0].r = r;
    rx_sig1[rx][i - num_samples_rx_sig0].i = im;
  }
}

struct GpuContext {
  cudaStream_t stream;
  curandState_t *curand_states;
  size_t curand_states_size;

  cufftHandle tx_plan;
  cufftHandle chan_plan;
  cufftHandle rx_plan;

  cufftComplex *d_tx_time;
  cufftComplex *d_tx_freq;
  cufftComplex *d_chan_time;
  cufftComplex *d_chan_freq;
  cufftComplex *d_rx_freq_accum;
  cufftComplex *d_rx_time;
  cufftComplex *d_rx_accum;
};

// max_samples is agnotic of num TX antennas as it allocs for worst case define above
extern "C" void *cuda_channel_pipeline_init(int max_samples, int num_tx_antenna)
{
  int dev = 0;
  struct cudaDeviceProp prop;
  CHECK_CUDA(cudaGetDeviceProperties(&prop, dev));
  int pageable;
  cudaDeviceGetAttribute(&pageable, cudaDevAttrPageableMemoryAccess, dev);
  if (!(pageable)) {
    return NULL;
  }

  GpuContext *ctx = new GpuContext();
  CHECK_CUDA(cudaStreamCreate(&ctx->stream));

  AssertFatal(max_samples <= GPU_MAX_SAMPLES,
              "max_samples %d exceeds the fixed max %d the preallocated GPU buffers/plans support\n",
              max_samples,
              GPU_MAX_SAMPLES);
  AssertFatal(num_tx_antenna > 0 && num_tx_antenna <= GPU_MAX_NUM_TX_ANT,
              "num_tx_antenna %d exceeds the fixed max %d the preallocated GPU buffers/plans support\n",
              num_tx_antenna,
              GPU_MAX_NUM_TX_ANT);

  // Sized for the worst case (GPU_MAX_SAMPLES symbol samples x GPU_MAX_NUM_RX_ANT antennas)
  // Calling cufftPlan1d during runtime will use too many cycles and lead to UE disconnection
  ctx->curand_states_size = (size_t)GPU_MAX_SAMPLES * GPU_MAX_NUM_RX_ANT;
  ctx->curand_states = (curandState_t *)create_and_init_curand_states_cuda(ctx->curand_states_size, time(NULL));

  const size_t tx_needed = (size_t)GPU_MAX_NUM_TX_ANT * GPU_MAX_NUM_BLOCKS * GPU_FFT_SIZE;
  const size_t chan_needed = (size_t)GPU_MAX_NUM_LINKS * GPU_FFT_SIZE;
  const size_t rx_needed = (size_t)GPU_MAX_NUM_RX_ANT * GPU_MAX_NUM_BLOCKS * GPU_FFT_SIZE;
  const size_t rx_accum_needed = (size_t)GPU_MAX_NUM_RX_ANT * GPU_MAX_ACCUM_LEN;

  CHECK_CUDA(cudaMalloc(&ctx->d_tx_time, tx_needed * sizeof(cufftComplex)));
  CHECK_CUDA(cudaMalloc(&ctx->d_tx_freq, tx_needed * sizeof(cufftComplex)));
  CHECK_CUDA(cudaMalloc(&ctx->d_chan_time, chan_needed * sizeof(cufftComplex)));
  CHECK_CUDA(cudaMalloc(&ctx->d_chan_freq, chan_needed * sizeof(cufftComplex)));
  CHECK_CUDA(cudaMalloc(&ctx->d_rx_freq_accum, rx_needed * sizeof(cufftComplex)));
  CHECK_CUDA(cudaMalloc(&ctx->d_rx_time, rx_needed * sizeof(cufftComplex)));
  CHECK_CUDA(cudaMalloc(&ctx->d_rx_accum, rx_accum_needed * sizeof(cufftComplex)));

  // Created once, for the worst-case batch count, and never re-planned: this is what removes the
  // per-call cufftDestroy/cufftPlan1d churn that used to happen whenever the incoming symbol size
  // (and therefore num_blocks) changed.
  CHECK_CUFFT(cufftPlan1d(&ctx->tx_plan, GPU_FFT_SIZE, CUFFT_C2C, GPU_MAX_NUM_TX_ANT * GPU_MAX_NUM_BLOCKS));
  CHECK_CUFFT(cufftPlan1d(&ctx->chan_plan, GPU_FFT_SIZE, CUFFT_C2C, GPU_MAX_NUM_LINKS));
  CHECK_CUFFT(cufftPlan1d(&ctx->rx_plan, GPU_FFT_SIZE, CUFFT_C2C, GPU_MAX_NUM_RX_ANT * GPU_MAX_NUM_BLOCKS));
  cufftSetStream(ctx->tx_plan, ctx->stream);
  cufftSetStream(ctx->chan_plan, ctx->stream);
  cufftSetStream(ctx->rx_plan, ctx->stream);

  return (void *)ctx;
}

extern "C" void cuda_channel_pipeline_shutdown(void *context_handle)
{
  if (context_handle == nullptr) {
    return;
  }
  GpuContext *ctx = (GpuContext *)context_handle;
  cufftDestroy(ctx->tx_plan);
  cufftDestroy(ctx->chan_plan);
  cufftDestroy(ctx->rx_plan);
  cudaFree(ctx->d_tx_time);
  cudaFree(ctx->d_tx_freq);
  cudaFree(ctx->d_chan_time);
  cudaFree(ctx->d_chan_freq);
  cudaFree(ctx->d_rx_freq_accum);
  cudaFree(ctx->d_rx_time);
  cudaFree(ctx->d_rx_accum);
  CHECK_CUDA(cudaStreamDestroy(ctx->stream));
  destroy_curand_states_cuda((void *)ctx->curand_states);
  delete ctx;
}

extern "C" void cuda_channel_pipeline(void *context_handle,
                                      const cf_t **channel,
                                      const c16_t **tx_sig0,
                                      const c16_t **tx_sig1,
                                      int num_samples_tx_sig0,
                                      c16_t **rx_sig0,
                                      c16_t **rx_sig1,
                                      int num_samples_rx_sig0,
                                      int num_samples,
                                      int channel_length,
                                      int nb_tx,
                                      int nb_rx,
                                      float noise_power)
{
  nvtxRangePushA("cuda_channel_pipeline");
  AssertFatal(context_handle, "No context handle provided\n");
  AssertFatal(channel, "No channel provided\n");
  AssertFatal(tx_sig0, "No tx_sig0 provided\n");
  AssertFatal(num_samples_tx_sig0 == (num_samples + channel_length - 1) || tx_sig1, "No tx_sig1 provided\n");
  AssertFatal(rx_sig0, "No rx_sig0 provided\n");
  AssertFatal(num_samples_rx_sig0 == num_samples || rx_sig1, "No rx_sig1 provided\n");
  AssertFatal(channel_length < GPU_FFT_SIZE, "channel_length must be below the fixed %d-point block FFT size\n", GPU_FFT_SIZE);
  AssertFatal(channel_length <= GPU_MAX_CHANNEL_LENGTH,
              "channel_length %d exceeds the fixed max %d the preallocated GPU buffers/plans support\n",
              channel_length,
              GPU_MAX_CHANNEL_LENGTH);
  AssertFatal(num_samples <= GPU_MAX_SAMPLES, "num_samples %d exceeds fixed max %d\n", num_samples, GPU_MAX_SAMPLES);
  AssertFatal(nb_tx > 0 && nb_tx <= GPU_MAX_NUM_TX_ANT, "nb_tx %d exceeds fixed max %d\n", nb_tx, GPU_MAX_NUM_TX_ANT);
  AssertFatal(nb_rx > 0 && nb_rx <= GPU_MAX_NUM_RX_ANT, "nb_rx %d exceeds fixed max %d\n", nb_rx, GPU_MAX_NUM_RX_ANT);
  AssertFatal(nb_tx * nb_rx <= GPU_MAX_NUM_LINKS,
              "nb_tx * nb_rx (%d) exceeds the fixed max total channel links %d\n",
              nb_tx * nb_rx,
              GPU_MAX_NUM_LINKS);
  GpuContext *ctx = (GpuContext *)context_handle;

  int x_len = num_samples + channel_length - 1;
  int window_offset = channel_length - 1;

  // Per-block signal length: chosen so block_len + channel_length - 1 == GPU_FFT_SIZE exactly,
  // i.e. the whole 256-point IFFT result is the true linear convolution of that block - no
  // circular-convolution aliasing and no wasted zero-padding beyond what channel_length requires.
  int block_len = GPU_FFT_SIZE - channel_length + 1;
  int num_blocks = (x_len + block_len - 1) / block_len;
  int accum_len = (num_blocks - 1) * block_len + GPU_FFT_SIZE;
  AssertFatal(num_blocks <= GPU_MAX_NUM_BLOCKS, "num_blocks %d exceeds fixed max %d\n", num_blocks, GPU_MAX_NUM_BLOCKS);
  AssertFatal(accum_len <= GPU_MAX_ACCUM_LEN, "accum_len %d exceeds fixed max %d\n", accum_len, GPU_MAX_ACCUM_LEN);

  nvtxRangePushA("memset_rx_accum");
  CHECK_CUDA(cudaMemsetAsync(ctx->d_rx_accum, 0, (size_t)nb_rx * accum_len * sizeof(cufftComplex), ctx->stream));
  nvtxRangePop();

  const int threads = 256;

  nvtxRangePushA("gather_zero_pad_tx");
  dim3 txBlocks((GPU_FFT_SIZE + threads - 1) / threads, nb_tx, num_blocks);
  gather_zero_pad_tx_block_kernel<<<txBlocks, threads, 0, ctx->stream>>>(tx_sig0,
                                                                         tx_sig1,
                                                                         num_samples_tx_sig0,
                                                                         x_len,
                                                                         block_len,
                                                                         ctx->d_tx_time);
  nvtxRangePop();

  nvtxRangePushA("gather_zero_pad_channel");
  dim3 chanBlocks((GPU_FFT_SIZE + threads - 1) / threads, nb_tx * nb_rx);
  gather_zero_pad_channel_kernel<<<chanBlocks, threads, 0, ctx->stream>>>(channel, channel_length, nb_tx, ctx->d_chan_time);
  nvtxRangePop();

  nvtxRangePushA("fft_forward_tx");
  CHECK_CUFFT(cufftExecC2C(ctx->tx_plan, ctx->d_tx_time, ctx->d_tx_freq, CUFFT_FORWARD));
  nvtxRangePop();

  nvtxRangePushA("fft_forward_channel");
  CHECK_CUFFT(cufftExecC2C(ctx->chan_plan, ctx->d_chan_time, ctx->d_chan_freq, CUFFT_FORWARD));
  nvtxRangePop();

  nvtxRangePushA("freq_multiply_accumulate");
  dim3 mulBlocks((GPU_FFT_SIZE + threads - 1) / threads, nb_rx, num_blocks);
  freq_multiply_accumulate_block_kernel<<<mulBlocks, threads, 0, ctx->stream>>>(ctx->d_tx_freq,
                                                                                ctx->d_chan_freq,
                                                                                nb_tx,
                                                                                ctx->d_rx_freq_accum);
  nvtxRangePop();

  nvtxRangePushA("fft_inverse_rx");
  CHECK_CUFFT(cufftExecC2C(ctx->rx_plan, ctx->d_rx_freq_accum, ctx->d_rx_time, CUFFT_INVERSE));
  nvtxRangePop();

  nvtxRangePushA("scatter_add");
  dim3 scatterBlocks((GPU_FFT_SIZE + threads - 1) / threads, nb_rx, num_blocks);
  scatter_add_block_kernel<<<scatterBlocks, threads, 0, ctx->stream>>>(ctx->d_rx_time, block_len, accum_len, ctx->d_rx_accum);
  nvtxRangePop();

  nvtxRangePushA("extract_and_noise");
  dim3 outBlocks((num_samples + threads - 1) / threads, nb_rx);
  extract_and_noise_block_kernel<<<outBlocks, threads, 0, ctx->stream>>>(ctx->d_rx_accum,
                                                                         accum_len,
                                                                         window_offset,
                                                                         rx_sig0,
                                                                         rx_sig1,
                                                                         num_samples_rx_sig0,
                                                                         num_samples,
                                                                         noise_power,
                                                                         ctx->curand_states);
  nvtxRangePop();

  nvtxRangePushA("stream_synchronize");
  CHECK_CUDA(cudaStreamSynchronize(ctx->stream));
  nvtxRangePop();

  nvtxRangePop(); // cuda_channel_pipeline
}
