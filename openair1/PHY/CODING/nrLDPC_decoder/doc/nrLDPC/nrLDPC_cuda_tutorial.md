# OAI nrLDPC GPU Decoder Tutorial

TODO: integrate into Sagar's tutorial

## Supported Configurations

| Parameter | CPU Baseline  | GPU Implementation |
| --- | --- | --- |
| **Base Graph (BG)** | 1, 2 | **1** |
| **Lifting Size ($Z_c$)** | All  | **$\ge 128$** |
| **Code Rate (R)** | BG1: 1/3, 2/3, 8/9 <br><br> BG2: 1/5, 1/3, 2/3 | **BG1: 1/3, 2/3, 8/9** |


*Table of content*

[[_TOC_]]

## Prerequisites

### Tested System Configuration

The GPU decoder has been successfully compiled and verified using the following baseline hardware and software environment:

* **GPU:** NVIDIA GH200 480GB
* **NVIDIA Driver:** 575.57.08
* **CUDA Toolkit:** 12.9
* **CMake:** 3.22.1
* **GCC (C/C++ Compiler):** 12.3.0

**Other Validated Platforms**
Beyond the Grace Hopper architecture, the implementation has also been successfully validated across a range of NVIDIA ecosystems, including:

* NVIDIA L40S 
* NVIDIA DGX Spark(GB10)
* NVIDIA Jetson
* NVIDIA A100

### Monitoring & Profiling

To examine runtime details, hardware utilization, and kernel execution efficiency, we recommend using the following official NVIDIA developer tools:

* **`nvidia-smi`**: Use this for real-time monitoring of VRAM usage, overall GPU utilization, and power consumption during the end-to-end tests.
* **`nsys` (Nsight Systems)**: Use this for system-wide performance analysis. It is highly effective for observing the CUDA Graph execution timeline and CPU-GPU pipeline scheduling behaviors.
* **`ncu` (Nsight Compute)**: Use this for deep, kernel-level profiling. It helps verify SM occupancy, shared memory usage, and the execution efficiency of the SIMD4 vectorized PTX instructions.



## Unit Test

### Build

Use the `build_oai` script to compile with CUDA support. If you have previously built a CPU-only version, ensure you clean the build directory or verify the dependencies:

```bash
./build_oai --ninja --phy_simulators --build-lib ldpc_cuda
```
```bash
cd ran_build/build/
```

### ldpctest

`ldpctest` is the foundational tool for evaluating the performance and correctness of the LDPC encoder/decoder.

**Basic Parameters:**
* `-r`: Nominator rate. Supported values: 1, 2, 22. (Default: 1)
* `-d`: Denominator rate. Supported values: 3, 5, 25. (Default: 3)
* `-n`: Number of trials (e.g., `-n300` runs 300 trials).
* `-S`: Number of segments (determines the workload size).
* `-s`: Starting SNR in dBm.
* `-i`: Maximum number of iterations. *(Note: Due to internal index offset implementation, the parameter `-i4` executes 5 decoding iterations.)*
* `-G`: **Enable GPU decoder**. (Omitting this parameter defaults to the CPU baseline).

#### Latency & Baseline Test

To compare the single-segment processing capability between CPU and GPU, set the segments parameter to 1 (`-S1`).

**BG1 Rate 1/3:**

* CPU: `./ldpctest -r1 -n300 -S1 -s4 -i4`
* GPU: `./ldpctest -r1 -n300 -S1 -s4 -i4 -G`

**BG1 Rate 2/3:**

* CPU: `./ldpctest -r2 -n300 -S1 -s4 -i4`
* GPU: `./ldpctest -r2 -n300 -S1 -s4 -i4 -G`

**BG1 Rate 8/9:**

* CPU: `./ldpctest -r22 -d25 -n300 -S1 -s4 -i4`
* GPU: `./ldpctest -r22 -d25 -n300 -S1 -s4 -i4 -G`

#### Maximum Throughput Test

To evaluate the maximum throughput of the GPU, increase the `-S` parameter to add parallel decoding workloads. The current GPU version in `ldpctest` supports a maximum of **128** segments.

Run the following commands to benchmark the maximum GPU throughput:

```bash
# BG1 R13 Max Throughput
./ldpctest -r1 -n30 -S128 -s4 -i4 -G
```
```bash
# BG1 R23 Max Throughput
./ldpctest -r2 -n30 -S128 -s4 -i4 -G
```
```bash
# BG1 R89 Max Throughput
./ldpctest -r22 -d25 -n30 -S128 -s4 -i4 -G
```

### Downlink Simulator (dlsim)

`nr_dlsim` is used to simulate the physical downlink shared channel (PDSCH). To offload LDPC decoding to the GPU, you must specify the CUDA shared library and enable the GPU flag.

#### Mandatory Flags for GPU Acceleration

* `--loader.ldpc.shlibversion _cuda`: Directs the dynamic loader to use the CUDA-accelerated LDPC library.
* `-Q` : Enables the GPU decoding path within the simulator logic.

#### Workload & Segment Calculation

Unlike `ldpctest` where you can manually set the number of segments, in `dlsim`, the number of segments (Code Blocks) is determined dynamically by the Transport Block Size (TBS). The TBS is defined by a combination of the MCS index (`-e`), the number of allocated PRBs (`-R`, `-b`), and the number of MIMO layers (`-x`). To specify the iterations, use `-I`.

Understanding the resulting segment count is crucial for evaluating GPU performance, as higher segment counts better utilize the GPU's parallel processing capabilities.

#### Execution Examples

**1. Target: ~ 9 Segments (Rate 1/3)**
Using MCS 13 with 273 PRBs and a single layer generates a workload of approximately 9 segments per slot.

```bash
./nr_dlsim -n100 -s20 -e13 -R273 -b273 -x1 -y1 -z1 -P -Q --loader.ldpc.shlibversion _cuda
```

**2. Target: ~ 18 Segments (Rate 2/3)**
Increasing the MCS to 22 while keeping 273 PRBs and 1 layer doubles the throughput requirement, resulting in about 18 segments.

```bash
./nr_dlsim -n100 -s20 -e22 -R273 -b273 -x1 -y1 -z1 -P -Q --loader.ldpc.shlibversion _cuda
```

**3. Target: ~ 50 Segments (Rate 8/9, High Workload)**
Using MCS 27 combined with 2x2 MIMO (`-x2 -y2 -z2`) forces a massive Transport Block Size, pushing the workload to approximately 50 segments. This scenario highly benefits from the Node-Based GPU architecture.

```bash
./nr_dlsim -n100 -s40 -e27 -R273 -b273 -z2 -x2 -y2 -P -Q --loader.ldpc.shlibversion _cuda
```

*(Note: The `-P` flag enables performance printing, allowing you to observe the decoding time per slot)*



### Uplink Simulator (ulsim)

`nr_ulsim` is used to simulate the physical uplink shared channel (PUSCH). The GPU acceleration logic follows the same architectural pattern as `dlsim`.

> **Parameter Mapping Notice:**
> When transitioning from `dlsim` to `ulsim`, the command-line arguments for identical physical layer parameters are different:
> * **MCS Index**: Use `-m` (instead of `-e`).
> * **Allocated PRBs**: Use `-r` (instead of `-b`).
> * **MIMO Layers**: Use `-W` (instead of `-x`).

To properly observe the GPU performance, we use the `-P` flag to print the execution statistics. After running a test, locate the `ULSCH segments decoding time` field in the `gNB RX` section of the output log. This metric isolates the time spent executing the LDPC decoding iterations.

#### Execution Examples

**1. Target: ~ 9 Segments (Rate 1/3)**
Using MCS 13 with 273 PRBs and a single layer.

```bash
./nr_ulsim -n100 -s20 -m13 -R273 -r273 -W1 -y1 -z1 -P -Q --loader.ldpc.shlibversion _cuda
```

**2. Target: ~ 18 Segments (Rate 2/3)**
Increasing to MCS 22, maintaining 273 PRBs and a single layer.

```bash
./nr_ulsim -n100 -s20 -m22 -R273 -r273 -W1 -y1 -z1 -P -Q --loader.ldpc.shlibversion _cuda
```

**3. Target: ~ 50 Segments (Rate 8/9, Massive Workload)**
Using MCS 27 with 273 PRBs and 2x2 MIMO (`-W2 -y2 -z2`).
```bash
./nr_ulsim -n100 -s40 -m27 -R273 -r273 -W2 -y2 -z2 -P -Q --loader.ldpc.shlibversion _cuda
```

## E2E Test

To be continued :）

