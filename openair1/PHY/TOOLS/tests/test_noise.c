/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include "PHY/TOOLS/tools_defs.h"
#include "SIMULATION/TOOLS/sim.h"
#include "SIMULATION/TOOLS/oai_cuda.h"
#include "common/utils/LOG/log.h"
#include "common/utils/utils.h"
#include <cuda_runtime.h>

configmodule_interface_t *uniqCfg = NULL;

void generate_random_signal(float **sig_re, float **sig_im, int nb_ant, int num_samples)
{
  for (int i = 0; i < nb_ant; i++) {
    for (int j = 0; j < num_samples; j++) {
      sig_re[i][j] = (float)((rand() % 20000) - 10000);
      sig_im[i][j] = (float)((rand() % 20000) - 10000);
    }
  }
}

int main(int argc, char **argv)
{
  logInit();
  randominit(0);

  int nb_rx_configs[] = {1, 2, 4, 8};
  int num_samples_configs[] = {30720, 61440, 122880};
  int num_trials = 100;
  float snr_db = 10.0f;

  printf("Starting Noise Generation Benchmark (CPU vs. CUDA)\n");
  printf("Averaging each test case over %d trials.\n", num_trials);
  printf("---------------------------------------------------------------------------------------------\n");
  printf("%-15s | %-15s | %-15s | %-15s | %-15s\n", "Antennas", "Signal Length", "CPU Time (us)", "CUDA Time (us)", "Speedup");
  printf("---------------------------------------------------------------------------------------------\n");

  for (int r = 0; r < sizeof(nb_rx_configs) / sizeof(int); r++) {
    for (int s = 0; s < sizeof(num_samples_configs) / sizeof(int); s++) {
      int nb_rx = nb_rx_configs[r];
      int num_samples = num_samples_configs[s];

      float **r_re = malloc(nb_rx * sizeof(float *));
      float **r_im = malloc(nb_rx * sizeof(float *));
      for (int i = 0; i < nb_rx; i++) {
        r_re[i] = malloc(num_samples * sizeof(float));
        r_im[i] = malloc(num_samples * sizeof(float));
      }

      c16_t **output_cpu = malloc(nb_rx * sizeof(c16_t *));
      c16_t **output_gpu = malloc(nb_rx * sizeof(c16_t *));
      output_cpu[0] = malloc(nb_rx * num_samples * sizeof(c16_t));
      output_gpu[0] = malloc(nb_rx * num_samples * sizeof(c16_t));
      for (int i = 1; i < nb_rx; i++) {
        output_cpu[i] = output_cpu[0] + i * num_samples;
        output_gpu[i] = output_gpu[0] + i * num_samples;
      }

      void *d_r_sig, *d_output_sig, *d_curand_states;
      void *h_r_sig_pinned, *h_output_sig_pinned;

#if defined(USE_UNIFIED_MEMORY)
      cudaMallocManaged(&d_r_sig, nb_rx * num_samples * sizeof(float) * 2, cudaMemAttachGlobal);
      cudaMallocManaged(&d_output_sig, nb_rx * num_samples * sizeof(short) * 2, cudaMemAttachGlobal);

      int deviceId;
      cudaGetDevice(&deviceId);
      cudaMemAdvise(d_r_sig, nb_rx * num_samples * sizeof(float) * 2, cudaMemAdviseSetReadMostly, deviceId);
      cudaMemAdvise(d_output_sig, nb_rx * num_samples * sizeof(short) * 2, cudaMemAdviseSetPreferredLocation, deviceId);
      cudaMemAdvise(d_output_sig, nb_rx * num_samples * sizeof(short) * 2, cudaMemAdviseSetAccessedBy, cudaCpuDeviceId);

      // Pinned memory is not used in the UM path for this wrapper
      h_r_sig_pinned = NULL;
      h_output_sig_pinned = NULL;
#else
      cudaMalloc(&d_r_sig, nb_rx * num_samples * sizeof(float) * 2);
      cudaMalloc(&d_output_sig, nb_rx * num_samples * sizeof(short) * 2);
      cudaMallocHost(&h_r_sig_pinned, nb_rx * num_samples * sizeof(float) * 2);
      cudaMallocHost(&h_output_sig_pinned, nb_rx * num_samples * sizeof(short) * 2);
#endif
      d_curand_states = create_and_init_curand_states_cuda(nb_rx * num_samples, time(NULL));

      double total_cpu_ns = 0;
      double total_gpu_ns = 0;

      double ts = 1.0 / 30.72e6;
      float signal_power = 1.0f;
      float sigma2 = signal_power / powf(10.0f, snr_db / 10.0f);

      for (int t = 0; t < num_trials; t++) {
        generate_random_signal(r_re, r_im, nb_rx, num_samples);

        struct timespec start, end;

        clock_gettime(CLOCK_MONOTONIC, &start);
        add_noise_float(output_cpu, (const float **)r_re, (const float **)r_im, sigma2, num_samples, 0, ts, 0, false, nb_rx);
        clock_gettime(CLOCK_MONOTONIC, &end);
        total_cpu_ns += (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);

        clock_gettime(CLOCK_MONOTONIC, &start);
        add_noise_cuda((const float **)r_re,
                       (const float **)r_im,
                       output_gpu,
                       num_samples,
                       nb_rx,
                       sigma2,
                       ts,
                       0,
                       0,
                       0,
                       0,
                       d_r_sig,
                       d_output_sig,
                       d_curand_states,
                       h_r_sig_pinned,
                       h_output_sig_pinned);
        cudaDeviceSynchronize();
        clock_gettime(CLOCK_MONOTONIC, &end);
        total_gpu_ns += (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
      }

      double avg_cpu_us = (total_cpu_ns / num_trials) / 1000.0;
      double avg_gpu_us = (total_gpu_ns / num_trials) / 1000.0;
      double speedup = (avg_gpu_us > 0) ? (avg_cpu_us / avg_gpu_us) : 0;

      printf("%-15d | %-15d | %-15.2f | %-15.2f | %-15.2fx\n", nb_rx, num_samples, avg_cpu_us, avg_gpu_us, speedup);

      for (int i = 0; i < nb_rx; i++) {
        free(r_re[i]);
        free(r_im[i]);
      }
      free(r_re);
      free(r_im);
      free(output_cpu[0]);
      free(output_gpu[0]);
      free(output_cpu);
      free(output_gpu);

#if defined(USE_UNIFIED_MEMORY)
      cudaFree(d_r_sig);
      cudaFree(d_output_sig);
#else
      cudaFree(d_r_sig);
      cudaFree(d_output_sig);
      cudaFreeHost(h_r_sig_pinned);
      cudaFreeHost(h_output_sig_pinned);
#endif
      destroy_curand_states_cuda(d_curand_states);
    }
  }

  printf("---------------------------------------------------------------------------------------------\n");
  printf("Benchmark finished.\n");

  return 0;
}
