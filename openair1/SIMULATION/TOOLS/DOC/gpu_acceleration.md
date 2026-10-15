<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# GPU-Accelerated Channel Simulation

[[_TOC_]]

## Overview

This document describes the CUDA-based GPU acceleration pipeline for the OAI channel simulation. The primary goal of this feature is to offload the computationally intensive `multipath_channel` convolution and `add_noise` functions from the CPU to the GPU. This overcomes existing performance bottlenecks and enables large-scale, real-time physical layer simulation that is not feasible with the CPU-based models.

The result is a complete, high-performance pipeline integrated into the OAI simulators. The feature is supported by a new benchmark suite created to validate correctness and analyze performance across a wide range of channel models and hardware configurations.

For automated CPU-versus-GPU validation on dedicated test servers, see the
[GPU channel-emulation test runner](../gpu_acceleration_test/README.md).

## Implementation

This feature is implemented across a set of new CUDA source files and integrated into the existing OAI simulation framework.

### Multipath Channel Convolution (`multipath_channel.cu`)

The core of the channel simulation is the multipath convolution. On the CPU, this is a sequential process where each output sample is calculated by iterating through all channel taps and accumulating the results. To parallelize this on the GPU, we assign one CUDA thread to calculate one output sample.

The key to making this parallel approach efficient is the use of a tiled convolution pattern with `__shared__` memory. Instead of having every thread read from slow global memory for each step of the convolution, threads in a block cooperate to pre-fetch all the data they will need into the on-chip shared memory.

To further optimize this, the responsibility for handling boundary conditions (the "halo" of past samples) has been moved from the GPU kernel to the host. The host code now prepares a larger input buffer that is **pre-padded** with `channel_length - 1` zeros before the actual signal data. This allows the kernel to be significantly simplified by removing the previous boundary-checking logic.

```cpp
// Snippet from the multipath_channel_kernel in multipath_channel.cu

// Step 1: Each thread cooperates to load a tile of the pre-padded signal
// into fast shared memory.
const int padding_len = channel_length - 1;
const int padded_num_samples = num_samples + padding_len;

for (int k = tid; k < shared_mem_size; k += blockDim.x) {
    int load_idx = block_start_idx + k;
    // Unconditional load from the padded buffer
    int interleaved_idx = 2 * (j * padded_num_samples + load_idx);
    tx_shared[k] = make_float2(tx_sig[interleaved_idx], tx_sig[interleaved_idx + 1]);
}
__syncthreads();
```

Once the data is staged in fast shared memory, each thread can perform the full convolution for its assigned output sample. The inner loop iterates through the `channel_length` taps, and all reads of the input signal (`tx_sample`) come from the `tx_shared` buffer.

```cpp
// Snippet from multipath_channel_kernel in multipath_channel.cu
// Step 2: Each thread performs convolution using the data in shared memory
    for (int l = 0; l < channel_length; l++) {
        // Read the historical transmit sample from the shared memory
        float2 tx_sample = tx_shared[tid + (channel_length - 1) - l];

        // Get the corresponding channel weight from global memory
        int chan_link_idx = ii + (j * nb_rx);
        float2 chan_weight = d_channel_coeffs[chan_link_idx * channel_length + l];

        // Perform the complex multiply-accumulate
        rx_tmp = complex_add(rx_tmp, complex_mul(tx_sample, chan_weight));
    }
    __syncthreads();
}

// Write the final result from the thread's private register to global memory...
```

### Noise Generation (`phase_noise.cu`)

After the multipath convolution, the next stage of the pipeline is to add realistic noise to the signal. This includes both Additive White Gaussian Noise (AWGN) and phase noise.

The CPU implementation performs this in a simple loop. For each sample of the signal, it calls a standard C library function to generate pseudo-random numbers, scales them appropriately, and adds them to the sample. This process is sequential and computationally inexpensive for a single signal but becomes a bottleneck when simulating many channels that all require high-quality random numbers simultaneously.

The GPU port follows the same parallelization strategy as the multipath kernel: one CUDA thread is assigned to process one single sample. The main challenge is generating millions of high-quality random numbers in parallel. To achieve this, the implementation relies on NVIDIA's **`cuRAND`** library.

The `cuRAND` library is stateful, meaning each thread needs its own unique generator "state" to produce an independent sequence of random numbers. In our implementation, a pool of `curandState_t` objects is initialized once in global memory. When the noise kernel is launched, each thread is assigned a unique state from this pool. The thread uses its state to generate random numbers, and then writes the updated state back to global memory for the next use.

A simplified view of this logic within the `add_noise_and_phase_noise_kernel` is shown below:

```cpp
// Simplified snippet from add_noise_and_phase_noise_kernel in phase_noise.cu
__global__ void add_noise_and_phase_noise_kernel(...)
{
    // Each thread calculates its unique sample index 'i' and antenna index 'ii'...

    // Load this thread's unique random number generator state
    curandState_t local_state = states[ii * num_samples + i];

    // Generate random numbers using the local state
    // Generates two normal-distributed numbers for AWGN (real and imaginary)
    float2 awgn = curand_normal2(&local_state);
    //    Generates one normal-distributed number for phase noise
    float phase_error = curand_normal(&local_state) * pn_std_dev;

    // Apply noise to the input signal from the previous kernel stage, and then aplu phase_error

    // Write the updated state back to global memory for the next simulation run
    states[ii * num_samples + i]= local_state;

    // Clamp the final floating-point value and store it as a short integer
    output_sig[ii * num_samples + i]= make_short2(...);
}
```

### Pipeline Orchestration (`channel_pipeline.cu`)

The `channel_pipeline.cu` file serves two primary functions: providing an interoperable API and orchestrating kernel execution.

Its first purpose is to act as an abstraction layer between the host C/C++ code of the OAI simulator and the CUDA C++ device code. It exposes a C-style Application Programming Interface (API) using `extern "C"`, which allows the standard C-based components to call the GPU pipeline without needing to handle CUDA-specific syntax or complexities.

Its second purpose is to orchestrate the execution of the individual CUDA kernels. It defines the logical flow of the channel simulation by launching the `multipath_channel_kernel` and `add_noise_and_phase_noise_kernel` in sequence and managing the data dependencies between them.

The functions within this file, such as `run_channel_pipeline_cuda`, implement a consistent workflow. Upon being called, they first cast the generic `void*` pointers received from the C host into their specific CUDA data types. The implementation then contains the conditional logic, via preprocessor directives, to support the various memory models. Following data setup, the functions define the CUDA execution grid and block dimensions and launch the two kernels sequentially. The output of the multipath kernel is directed to an intermediate buffer, which serves as the input for the subsequent noise kernel. Finally, the functions manage synchronization with the host and handle the transfer of the final results from the GPU back to host memory.

*The external API of these pipeline functions has been streamlined. The function signatures no longer require a separate `tx_sig_interleaved` parameter, instead relying on a single, pre-padded host buffer (`h_tx_sig_pinned`) as the authoritative source for the input signal, which simplifies the data flow from the simulators.*

---

## Memory Management Models

To provide flexibility and enable performance testing on different hardware architectures, the project now supports three distinct memory management models. The desired model is selected at build time by passing a flag to the CMake build command.

### ATS Hybrid Model (`-DUSE_ATS_MEMORY=ON`)

This is a hybrid approach that leverages Address Translation Services (ATS), a hardware feature allowing the GPU to directly access host-allocated memory. In our implementation, the large input signal buffer is allocated in host memory using `malloc`. The GPU kernel is then able to read this data directly from the host, eliminating the need for an initial Host-to-Device `cudaMemcpy` for that buffer. Intermediate and output buffers are still allocated on the device, and the final result is copied back to the host explicitly. Due to its better performance compared to the other methods, it's also the default behaviour when no flag is specified.

### Unified Memory Model (`-DUSE_UNIFIED_MEMORY=ON`)

This model simplifies memory management by creating a single, managed memory space that is accessible to both the CPU and the GPU. Memory is allocated once using `cudaMallocManaged`. The CUDA runtime and driver then automatically handle the migration of data to whichever processor is accessing it (e.g., moving data to the GPU's memory when a kernel is launched). This removes the need for most explicit `cudaMemcpy` calls. While the CUDA driver automatically migrates data on-demand, the implementation includes further optimizations to guide this process for better performance. `cudaMemAdvise` is used to provide performance hints to the driver, such as setting a buffer's preferred location to the GPU or marking it as mostly read-only. Additionally, `cudaMemPrefetchAsync` is also used to explicitly move data to the GPU's memory ahead of time. This ensures the data is already local when a kernel begins execution, which helps to hide the latency of the data migration.

### Explicit Copy Model

This is the traditional CUDA programming model. The host and device have separate and distinct memory spaces. The programmer is responsible for all data management. Memory must be allocated on the device using `cudaMalloc`, and data must be manually transferred between the host and device using explicit `cudaMemcpy` calls before and after a kernel launch. This configuration can be run with other flags turned off by using `-DUSE_ATS_MEMORY=OFF` and `-DUSE_UNIFIED_MEMORY=OFF`.

-----

## Project Integration

To ensure the feature is modular, the integration into the OAI project was handled at both the build system and source code levels. All of the new CUDA source files (`channel_pipeline.cu`, `multipath_channel.cu`, `phase_noise.cu`) are compiled into a single static library named `oai_cuda_lib`. This is defined in the main `CMakeLists.txt` file and is controlled by the `ENABLE_CHANNEL_SIM_CUDA` CMake option, which is disabled by default. When this option is activated (`-DENABLE_CHANNEL_SIM_CUDA=ON`), the build system compiles the CUDA library and also defines a global `CHANNEL_SIM_CUDA` preprocessor macro that is visible to the rest of the project.

This `CHANNEL_SIM_CUDA` macro is then used within the simulator files, such as `dlsim.c` and `ulsim.c`, to conditionally compile all CUDA-related code. This approach allows for the inclusion of the `oai_cuda.h` header, and the pre-allocation of GPU memory at startup, all without affecting the standard CPU-only build.

Inside the main processing loop, an `if (use_cuda)` statement acts as the primary runtime switch. To enable a direct comparison, timing measurement calls have been added to both the GPU and CPU execution paths. Also, both paths now source their data from the same common, pre-padded host buffer. The GPU path receives a pointer to the entire padded buffer and leverages its optimized kernel, while the CPU path uses a temporary array of offset pointers to work on the same data without being aware of the padding. This ensures both computations operate on the exact same source data, isolating the performance measurement to the channel processing itself. To execute the simulation on the GPU, the `-f cuda` flag must be provided at runtime; otherwise, the program defaults to this refined CPU implementation.

Finally, the build system links the `nr_dlsim` and `nr_ulsim` executables against the `oai_cuda_lib` only when the feature is enabled, creating a clean separation between the two codebases.

---

## Benchmark and Analysis Suite

To validate the correctness, measure performance, and analyze the scalability of the GPU pipeline, a dedicated suite of tests was developed. The source code for these benchmarks is located in the `openair1/PHY/TOOLS/tests/` directory. This suite includes both focused unitary tests for individual components and a comprehensive benchmark for the end-to-end pipeline, which is the primary tool for performance evaluation.

### `test_channel_scalability.c`

This program is the main tool for evaluating the end-to-end GPU channel simulation pipeline. It's designed to be highly configurable, allowing for the simulation of a wide variety of workloads and execution strategies.

#### Configuration and Usage

The benchmark is configured at runtime using a set of command-line flags.

| Flag | Long Version | Argument | Description |
| :--- | :--- | :--- | :--- |
| `-c` | `--num-channels` | `<N>` | Sets the number of independent channels to simulate. |
| `-t` | `--nb-tx` | `<N>` | Sets the number of transmit antennas. |
| `-r` | `--nb-rx` | `<N>` | Sets the number of receive antennas. |
| `-s` | `--num-samples` | `<N>` | Sets the number of samples per signal. |
| `-l` | `--ch-len` | `<N>` | Sets the length of the channel impulse response. |
| `-n` | `--trials` | `<N>` | Sets the number of times each test is run for averaging. |
| `-S` | `--sum-outputs` | (none) | Enables interference simulation by summing channel outputs. (Using unique input signals) |
| `-m` | `--mode` | `<mode>` | Sets the GPU execution mode: `serial`, `stream`, or `batch`. |
| `-h` | `--help` | (none) | Displays the help message with all options. |

#### Execution Modes

The `-m` or `--mode` flag controls the strategy used to execute the pipeline for multiple channels:

  * **`serial`**: This mode processes each channel sequentially. It loops through all channels and makes a synchronous call to `run_channel_pipeline_cuda` for each one, waiting for it to complete before starting the next. 
  * **`stream`**: This mode is designed to test concurrency. It launches all channel simulations asynchronously into separate CUDA streams by calling `run_channel_pipeline_cuda_streamed` in a loop. This allows the GPU hardware to overlap the execution of many kernels.
  * **`batch`**: It aggregates the data for all channels on the host and makes a single call to `run_channel_pipeline_cuda_batched`. This launches one massive, unified kernel to process all channels at once, minimizing CPU overhead and maximizing GPU throughput.

### `test_channel_simulation.c`

The `test_channel_simulation.c` program was one of the initial benchmarks created for this project. Its primary function is to perform a direct performance comparison between the full, sequential CPU-based channel pipeline and the original synchronous GPU-based pipeline implemented in `run_channel_pipeline_cuda`. It served as an early-stage tool to validate the end-to-end functionality of the basic GPU port and to obtain initial speedup measurements before more advanced optimizations were developed.

### `test_multipath.c`

The `test_multipath.c` program is a unitary test designed specifically to validate the `multipath_channel_cuda` function in isolation. The key feature of this test is the `verify_results` function, which performs a sample-by-sample comparison of the output arrays from both versions and calculates the Mean Squared Error (MSE). If the error is below a predefined tolerance, the test is marked as *"PASSED"* in the final report, confirming that the GPU kernel is producing numerically correct results.

### `test_noise.c`

Similar to the multipath test, `test_noise.c` is a unitary test that focuses on a single component: the `add_noise_cuda` function. Its purpose is to validate the correctness of the GPU-based noise generation and measure its performance against the equivalent CPU version.

-----

### Direct CPU vs. GPU Speedup

To provide a clear, baseline performance comparison, the `test_channel_simulation` benchmark was run. This tool directly compares the execution time of the sequential, `float`-based CPU pipeline against the baseline synchronous GPU pipeline (`run_channel_pipeline_cuda`). The following tests were executed on a GH200 server using the ATS memory model. The results demonstrate the performance gains achieved by offloading the channel simulation to the GPU.

| Channel Type | MIMO Config | Signal Length | CPU Pipeline (µs) | GPU Pipeline (µs) | Overall Speedup |
| :--- | :--- | :--- | :--- | :--- | :--- |
Short Channel   | 1x1             | 30720           | 654.12               | 45.60                | 14.34          x
Short Channel   | 2x2             | 30720           | 2145.07              | 57.48                | 37.32          x
Short Channel   | 4x4             | 30720           | 7611.24              | 84.20                | 90.40          x
Short Channel   | 8x8             | 30720           | 28190.40             | 148.07               | 190.39         x
Short Channel   | 1x1             | 61440           | 1305.85              | 54.16                | 24.11          x
Short Channel   | 2x2             | 61440           | 4290.31              | 80.34                | 53.40          x
Short Channel   | 4x4             | 61440           | 15264.63             | 136.62               | 111.73         x
Short Channel   | 8x8             | 61440           | 56561.05             | 255.82               | 221.10         x
Short Channel   | 1x1             | 122880          | 2617.06              | 79.33                | 32.99          x
Short Channel   | 2x2             | 122880          | 8585.16              | 129.69               | 66.20          x
Short Channel   | 4x4             | 122880          | 30543.07             | 233.36               | 130.88         x
Short Channel   | 8x8             | 122880          | 113185.64            | 468.10               | 241.80         x
Long Channel    | 1x1             | 30720           | 1068.25              | 44.37                | 24.08          x
Long Channel    | 2x2             | 30720           | 3795.86              | 58.86                | 64.49          x
Long Channel    | 4x4             | 30720           | 14209.40             | 89.08                | 159.51         x
Long Channel    | 8x8             | 30720           | 54579.90             | 164.72               | 331.36         x
Long Channel    | 1x1             | 61440           | 2139.73              | 56.02                | 38.20          x
Long Channel    | 2x2             | 61440           | 7608.75              | 83.18                | 91.47          x
Long Channel    | 4x4             | 61440           | 28463.66             | 145.80               | 195.23         x
Long Channel    | 8x8             | 61440           | 109785.85            | 289.96               | 378.62         x
Long Channel    | 1x1             | 122880          | 4267.25              | 80.65                | 52.91          x
Long Channel    | 2x2             | 122880          | 15209.56             | 135.12               | 112.56         x
Long Channel    | 4x4             | 122880          | 56951.08             | 249.47               | 228.29         x
Long Channel    | 8x8             | 122880          | 222368.44            | 524.97               | 423.59         x
