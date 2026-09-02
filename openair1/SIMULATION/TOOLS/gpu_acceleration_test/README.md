# GPU vs CPU vrtsim Channel Emulation Testing

This directory contains the GH server test runner used to GPU vrtsim channel emulation. The matrix scales the gNB side up to 64 antennas while keeping the UE side capped at 4 antennas for the larger cases, and additionally regression-tests asymmetric UE antenna configs (UE TX antennas != UE RX antennas): `2x1` (2 TX / 1 RX) and `4x2` (4 TX / 2 RX), each run at the matching gNB antenna scale (2 and 4 respectively).

## 1. Clone

```bash
mkdir -p ~/oai-testing
cd ~/oai-testing

git clone -b personal/blasiec/gpu_chan_accel https://github.com/ConnorBlasie/openairinterface5g.git
git clone https://gitlab.eurecom.fr/oai/raytracing-channel-emulator.git
```

If this branch has already been merged, clone `develop` instead.

## 2. Build

### GPU build

```bash
cd ~/oai-testing/openairinterface5g/cmake_targets
mkdir -p build_gpu
cd build_gpu

cmake ../.. -GNinja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DOAI_VRTSIM_TAPS_CLIENT=OFF \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DUSE_ATS_MEMORY=ON \
  -DENABLE_CHANNEL_SIM_CUDA=ON \
  -DENABLE_TESTS=ON \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DCMAKE_CUDA_COMPILER=/usr/local/cuda-12/bin/nvcc

cmake --build . --target \
  vrtsim rfsimulator nr-softmodem nr-uesoftmodem ldpc params_libconfig params_yaml \
  test_channel_pipeline benchmark_channel_pipeline test_channel_scalability accuracy_test_gpu_optimized_pipeline \
  -j"$(nproc)"
```

## 3. Generate the CIR Database

The automated matrix uses `1x1`, `2x2`, `4x4`, then gNB-heavy `8x4`, `16x4`, `32x4`, and `64x4` symmetric channel shapes, plus the asymmetric UE regression shapes `2x1` and `4x2`. `CIR_generator.py` has no `--antennas`/`--models` selection flags — every run unconditionally generates all TDL-A..E models against its full built-in shape table (`1x1` through `64x64`, including `2x1` and `4x2`), so no extra options are needed to cover the new regression cases. Generate both short and long delay-spread databases before running the matrix.

```bash
cd ~/oai-testing/raytracing-channel-emulator/server/external_taps

python3 cir_generator.py --out cir_db_short.bin \
  --models TDL-A TDL-B \
  --antennas 1x1 2x1 2x2 4x2 4x4 8x2 8x4 16x2 16x4 32x2 32x4 64x2 64x4 \
  --delay-spreads 10 \
  --speeds 1.5 \
  --snapshots 2000

python3 cir_generator.py --out cir_db_long.bin \
  --models TDL-A TDL-B \
  --antennas 1x1 2x1 2x2 4x2 4x4 8x2 8x4 16x2 16x4 32x2 32x4 64x2 64x4 \
  --delay-spreads 30 \
  --speeds 1.5 \
  --snapshots 2000 \
  --max-taps 32
```

## 4. Run the Automated Test Matrix

```bash
cd ~/oai-testing/openairinterface5g/openair1/SIMULATION/TOOLS/gpu_acceleration_test
chmod +x run_vrtsim_gpu_cpu_matrix.sh
./run_vrtsim_gpu_cpu_matrix.sh
```

Logs are written to `~/oai-testing/logs` by default and named:

```text
<build>_<antenna>_<chanlen>_<run>_<gnb|ue>.log
```

For example, `gpu_64x4_long_1_gnb.log`.

The main runtime knobs can be overridden with environment variables:

```bash
RUN_DURATION_SECONDS=120 RUNS_PER_CONFIG=3 ./run_vrtsim_gpu_cpu_matrix.sh
```
