<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# LDPC Decoder — Hexagon DSP Offload

Offloads 5G NR LDPC belief-propagation decoding from the A78 application cores
to the Qualcomm Hexagon Compute DSP (cDSP) on the DragonWing IQ-9075 via
FastRPC.

## Directory layout

```
nrLDPC_coding_hexagon/
├── CMakeLists.txt            ARM side (libldpc_hexagon.so) + DSP external project
├── dsp/CMakeLists.txt        DSP side (libldpc_hexagon_skel.so), hexagon-clang
├── inc/
│   ├── ldpc_hexagon.idl      FastRPC IDL (qaic → stub + skel + shared header)
│   ├── ldpc_hexagon_batch.h  TB-level batch API
│   └── ldpc_hexagon_queue.h  dspqueue packet format
└── src/
    ├── nrLDPC_coding_hexagon.c  nrLDPC_coding interface (loader entry points)
    ├── ldpc_hexagon_arm.c       ARM side of the offload
    └── ldpc_hexagon_imp.c       DSP decoder
```

## Interface

The IDL defines a single RPC function:

```
long decode(in   sequence<uint8> params,    // 8-byte packed t_nrLDPC_dec_params
            in   sequence<uint8> llr,        // input LLRs (numLLR × int8)
            rout sequence<uint8> llr_out,    // posterior LLRs (numLLR × int8)
            rout sequence<uint8> meta);      // 4-byte LE uint32 = iterations completed
```

`params` wire format (8 bytes):

| Offset | Field      | Type    | Notes                                  |
|--------|------------|---------|----------------------------------------|
| 0      | BG         | uint8   | Base graph 1 or 2                      |
| 1      | R          | uint8   | Rate: 13, 23, 89 (BG1); 15, 13, 23 (BG2) |
| 2      | numMaxIter | uint8   | Maximum BP iterations                  |
| 3      | diag       | uint8   | 0=memcpy passthrough, 1=scatter/gather roundtrip, 2+=full BP |
| 4–5    | Z          | uint16  | Lifting size (2–384), little-endian    |
| 6–7    | —          | uint16  | Reserved                               |

The DSP always returns posterior LLRs in `llr_out`. The ARM stub converts to the
requested `outMode` (bit-packed / int8 / LLR pass-through) locally, and the
`check_crc` function pointer never crosses the FastRPC boundary.

## ARM stub (`ldpc_hexagon_arm.c` + `ldpc_encoder_optim8segmulti.c`)

`libldpc_hexagon.so` exports the same four symbols as `libldpc.so` so it is a
drop-in replacement for testing with `ldpctest` and the segmentation layer:

| Symbol         | Source                               | Description                       |
|----------------|--------------------------------------|-----------------------------------|
| `LDPCinit()`   | ldpc_hexagon_arm.c                   | Opens FastRPC session, rpcmem init |
| `LDPCshutdown()` | ldpc_hexagon_arm.c                 | Closes session, releases rpcmem   |
| `LDPCdecoder()` | ldpc_hexagon_arm.c                  | Offloads decode to cDSP            |
| `LDPCencoder()` | ldpc_encoder_optim8segmulti.c       | Runs on ARM (not offloaded)        |

The encoder is included unchanged from the standard `libldpc.so` build and
links against `ldpc_segment` and `ldpc_gen_HEADERS` exactly as it does there.
Offloading the encoder to the DSP may be evaluated separately.

`LDPCdecoder` flow:
1. Calls `nrLDPC_init` to derive `numLLR` from BG/Z/R.
2. Uses four ION-backed rpcmem buffers pre-allocated in `LDPCinit` (params, LLR in, LLR out, meta).
3. Copies input LLRs, calls `ldpc_hexagon_decode` via FastRPC.
4. Reads `numIter` from meta; sets abort flag if `numIter >= numMaxIter`.
5. Converts `llr_out` to the requested output format with a scalar loop.

## DSP implementation (`ldpc_hexagon_imp.c`)

Scalar min-sum belief propagation using the existing portable OAI headers:

| Header reused        | Role                                             |
|----------------------|--------------------------------------------------|
| `nrLDPC_types.h`     | Struct definitions (`t_nrLDPC_lut`, etc.)        |
| `nrLDPCdecoder_defs.h` | Constants (ZMAX, buffer sizes, group counts)   |
| `nrLDPC_lut.h`       | All static LUT arrays (circShift, BN addresses…) |
| `nrLDPC_init.h`      | LUT pointer setup (`nrLDPC_init`)                |
| `nrLDPC_mPass.h`     | Circular memcpy (`nrLDPC_llr2CnProcBuf`, etc.)   |

`#define CODEGEN 1` suppresses the `time_meas.h` dependency inside
`nrLDPC_types.h`. `sizeofArray` is defined locally before including
`nrLDPC_mPass.h` to avoid pulling in `common/utils/utils.h`.

### Algorithm

Standard flooding-schedule min-sum BP:

**Initialisation**
- `nrLDPC_llr2llrProcBuf` — scatter channel LLRs into LLR processing buffer
- `nrLDPC_llr2CnProcBuf_BG{1,2}` — scatter channel LLRs into CN processing buffer

**Each iteration**
1. `scalar_cnProc` — CN step (min-sum, scalar)
2. `nrLDPC_cn2bnProcBuf_BG{1,2}` — CN results → BN processing buffer (circular memcpy)
3. `scalar_bnProcPc` — BN posterior sum: `llrRes = sat8(llrProcBuf + Σ_k bnProcBuf[k])`
4. `scalar_bnProc` — BN extrinsic: `bnProcBufRes[k] = sat8(llrRes − bnProcBuf[k])`
5. `nrLDPC_bn2cnProcBuf_BG{1,2}` — extrinsics → CN processing buffer
6. `scalar_cnProcPc` — parity check: XOR of sign bits at each CN; returns 0 if all pass

**Output**
- `nrLDPC_llrRes2llrOut` — gather `llrRes` into external LLR order

### CN processing detail

`cnProc_group` processes one CN group (e.g. G3, G4, …, G19 for BG1):

```
for each (CN i, lifting symbol z):
    Pass 1 over all numBN positions k:
        sgn_all ^= cnProcBuf[startAddr + k*bitOff + i*Z + z]
        track min1, min2, min1_k
    Pass 2 — write extrinsic for each position j:
        mag_j  = (j == min1_k) ? min2 : min1
        sgn_j  = sgn_all ^ cnProcBuf[..j..]   // product of signs excluding j
        result = (sgn_j & 0x80) ? -mag_j : mag_j
```

`bitOff = numCN × NR_LDPC_ZMAX` — stride between BN position layers (bytes).

### Buffer layout in `bnProcBuf` / `bnProcBufRes`

BN groups are indexed by connectivity degree (`cnidx` = number of connected CNs − 1):

```
bnProcBuf[lut_startAddrBnGroups[g] + k * numBn * ZMAX + b]
                                      ↑ CN layer k         ↑ BN sample b ∈ [0, numBn*Z)
```

Only `numBn × Z` bytes are valid within each `numBn × ZMAX`-byte layer; the
extra `(ZMAX − Z) × numBn` bytes provide alignment for all lifting sizes without
recomputing offsets.

### Working buffers

All six processing buffers are heap-allocated on the DSP to stay within the
cDSP thread stack limit (~64 KB):

| Buffer         | Size (bytes)              |
|----------------|---------------------------|
| cnProcBuf      | 316 × 384 = 121 344       |
| cnProcBufRes   | 316 × 384 = 121 344       |
| bnProcBuf      | 316 × 384 = 121 344       |
| bnProcBufRes   | 316 × 384 = 121 344       |
| llrRes         | 27 000                    |
| llrProcBuf     | 27 000                    |
| **Total**      | **~540 KB**               |

## Build

The offload is part of the normal DragonWing cross build (see the DragonWing
section of `doc/cross-compile.md`); enable it with `-DENABLE_LDPC_HEXAGON=ON`:

```sh
cmake ../../.. -GNinja \
    -DCMAKE_TOOLCHAIN_FILE=../../../cmake_targets/cross-arm-dragonwing.cmake \
    -DNATIVE_DIR=../build -DENABLE_LDPC_HEXAGON=ON
ninja ldpc_hexagon ldpctest
```

This builds both halves:

- `libldpc_hexagon.so` (ARM, aarch64 GCC), next to the other LDPC modules;
- `libldpc_hexagon_skel.so` (DSP, `hexagon-clang`), built as an external
  project with `cmake_targets/cross-hexagon-cdsp.cmake` and installed in the
  same build directory. It is rebuilt with every build, so the two halves
  cannot drift apart.

The FastRPC stub and skel glue are generated from `inc/ldpc_hexagon.idl` by
`qaic` at build time. `cmake_targets/tools/MODULES/FindHexagonSDK.cmake` finds
the SDK from `HEXAGON_SDK_ROOT` (cmake variable or environment).

DSP build options (pass them to the main cmake invocation, they are forwarded):

| Option | Default | Meaning |
|--------|---------|---------|
| `HEXAGON_DSP_ARCH` | `v73` | CDSP architecture (QCS9075: v73) |
| `LDPC_HEX_VTCM` | `ON` | keep the decoder working set in VTCM; needs a signed protection domain (testsig) |
| `LDPC_HEX_GATHER` | `ON` | `vgather` deinterleave from VTCM; falls back to scalar if VTCM cannot be acquired |

Without a testsig on the device, build with `-DLDPC_HEX_VTCM=OFF`: the
`compute_resource_*` symbols are absent from the unsigned DSP runtime and the
skel fails to load with `AEE_EUNABLETOLOAD`.

### Deploy

```sh
scp libldpc_hexagon.so      <target>:<oai dir>/
scp libldpc_hexagon_skel.so <target>:/usr/lib/rfsa/adsp/
```

`/usr/lib/rfsa/adsp/` is the first directory the DSP loader scans for skels;
`/vendor/lib/rfsa/adsp/` comes second, so a stale skel there is shadowed. A skel
from another build is the most likely cause of an offload that loads but returns
wrong results: the batch request/response layout is shared between the halves,
so deploy them together.

Run with `--loader.ldpc.shlibversion _hexagon`.

### Test without a board (hexagon-sim)

`ldpc_hexagon_simtest` runs the DSP decoder under `hexagon-sim`, the Hexagon
instruction-set simulator shipped in the SDK, on the build host. It needs the
SDK but no DragonWing, so it is the test to run where there is no target, CI
included. It is built and registered with the other tests by `-DENABLE_TESTS=ON`:

```sh
cmake ../../.. -GNinja \
    -DCMAKE_TOOLCHAIN_FILE=../../../cmake_targets/cross-arm-dragonwing.cmake \
    -DNATIVE_DIR=../build -DENABLE_LDPC_HEXAGON=ON -DENABLE_TESTS=ON
ninja ldpc_hexagon
ctest -R ldpc_hexagon_sim      # about 45 s
```

It encodes with OAI's own encoder, rate-matches, interleaves and adds noise,
then decodes with the production `ldpc_decode_one()`, DSP-side prep included.
It covers BG1 and BG2 at several lifting sizes (including Z not a multiple of
128), every decoder code rate, two code blocks per transport block, filler
bits, and HARQ combining across redundancy versions; plus direct checks of the
CRC and of the kernels against scalar references. It prints
`PASSED: 0 failure(s)` or the cases that failed. `hexagon-sim` exits 0 whatever
the program returns, so the test is judged on that line.

Without the cmake, from the build directory:

```sh
$HEXAGON_SDK_ROOT/tools/HEXAGON_Tools/*/Tools/bin/hexagon-sim -mv73 ./ldpc_hexagon_simtest
```

The simulator models no FastRPC, dspqueue or QuRT threads, so the test calls the
decoder directly and leaves the worker pool and the ARM side to `ldpctest` on
the board.

### Test with ldpctest

`ldpctest -v _hexagon` loads `libldpc_hexagon.so`:

```sh
# BG1, K'=8448, rate 1/3, 100 trials, Eb/N0 = 3 dB
./ldpctest -v _hexagon -l 8448 -r 1 -d 3 -n 100 -s 3
```

If the skel does not load (`AEE_EUNABLETOLOAD`), check it is in
`/usr/lib/rfsa/adsp/`, built for the right `HEXAGON_DSP_ARCH`, and with
`LDPC_HEX_VTCM` matching the protection domain (see above).

> **Before you believe any timing from `ldpctest` or the DU, check that deep
> idle is disabled** — see
> [Deep idle wrecks the measurement](#deep-idle-wrecks-the-measurement-check-this-first).
> It is a sysfs write that does not survive a reboot, and with it left on
> every number here is roughly 1000 us pessimistic.

## Deep idle wrecks the measurement (check this FIRST)

The ARM thread sleeps waiting for the DSP, so every batch pays one wake. On this platform
that wake is dominated by cpuidle exit latency, not by the queue:

| cpuidle state | exit latency |
|---|---|
| `WFI` | 1 us |
| `cpu-sleep-0` | **1450 us** |
| `cpu-sleep-1` | 1763 us |

Measured at 273 PRB / 1 layer / MCS 22, `OAI_LDPC_WORKERS=4`, batch of 18 code blocks:

| | wait | DSP wall | wake | segments decoding | Total PHY proc rx |
|---|---|---|---|---|---|
| stock idle | 3000 us | 1837 us | **1163 us** | 3008 us | 5627 us |
| `state[1-9]` disabled | 2050 us | 1845 us | **205 us** | 2057 us | **3146 us** |
| polling (`OAI_LDPC_SLEEP=0`) | 1951 us | 1884 us | 67 us | 1958 us | 3803 us |

Disabling deep idle cuts the wake 5.7x and costs nothing. It also speeds up the ARM inner
receiver (1300 -> 1039 us), which pays the same exit latency. Note the last row: with deep
idle off, SLEEPING is within 100 us of POLLING while still releasing the ARM core, so the
usual "poll for latency, sleep for CPU" trade-off mostly disappears -- do not switch the
default to polling on the strength of a stock-idle measurement.

Apply this, together with the `performance` governor, after every boot -- a fresh boot
comes up with `schedutil` and the deep states enabled:

    for c in /sys/devices/system/cpu/cpu[0-9]*; do
      echo performance > $c/cpufreq/scaling_governor
      for s in $c/cpuidle/state[1-9]; do [ -d "$s" ] && echo 1 > "$s"/disable; done
    done

Check `cpu-sleep-0`'s `disable` before trusting any number here; a partially applied
state (only `cpu-sleep-1` disabled) has been observed.

## What it costs and what it buys

Measured at 273 PRB / 1 layer, 200 slots, both offloads enabled, against the
native CPU decoder on the same build:

| | segment decoding | total PHY proc rx |
|---|---|---|
| native CPU (`ldpc_optim8seg`) | 7,659 us/slot | 16,726 us/slot |
| Hexagon, batched, 4 HVX workers | **4,696 us/slot** | **10,869 us/slot** |

Per transport block the batch breaks down as roughly 330 us of HVX compute,
97 us of DSP-side serial work, 106 us of FastRPC transport and 4 us of ARM-side
write. The ARM *blocks* on `dspqueue_read` rather than polling, so the FastRPC
share is a latency charge, not a CPU charge.

The raw per-codeword ratio is 1.6-1.7x, not the ~3.5x a naive batch-versus-
single-thread comparison suggests: the batch runs four HVX worker contexts while
the CPU reference runs one core. **The reason to enable this offload is not
decode throughput — it is that it frees four A78 cores for the TX path**, which
on this board is where the real-time pressure is.

## Limitations / future work

- The per-codeword decode is still slower than one A78 core's: 110 us on an HVX
  worker against 183.58 us on the CPU is only 1.67x, and that is with four HVX
  worker contexts against one ARM core. Do not quote a figure derived from
  comparing a four-way DSP batch against a single-threaded CPU run.
- `cnProc` and `bnProc` remain the bulk of the DSP time; the termination test is
  down to 4%.

## When FastRPC opens start failing for everything

Symptom: `ldpc_hexagon_open failed: -1`, or `dspqueue_read` returning `0xc` (AEE_EEXPIRED,
a timed-out wait) -- and crucially it happens for configurations that worked minutes
earlier, including single-device ones. It reads as "the DSP is broken". It is not; it is
resource exhaustion.

Cause: both DSP workers SPIN on `dspqueue_read_noblock`, and before the idle watchdog they
left that loop only on an explicit STOP packet. Any ARM-side death that skips STOP -- a
`timeout` kill, a crash, Ctrl-C -- left the thread spinning inside the blocking
`worker_start` FastRPC call, so the PD could never be reclaimed. Each abandoned run leaked
one, and a sweep of back-to-back runs exhausted the DSP.

The idle watchdog (180 s of silence -> the worker returns) makes this self-healing: 18 runs
killed mid-flight no longer degrade anything. If you see the symptom anyway, WAIT three
minutes before concluding something is wrong.

Do not restart the CDSP (`/sys/class/remoteproc/*/state`) to clear it: the stop waits
on the spinning worker, the start takes minutes, and interrupting it leaves the remoteproc
lock held until a reboot.

## Further reading

`OPTIMISATION-NOTES.md`, next to this file, is the engineering record: how the
decoder reached its current speed, the optimisations that were tried and lost,
the bugs that are now fixed and what they looked like, and how to read the DSP's
PMU, why the intrinsics need checking under `hexagon-sim`, and the FARF,
protection-domain and VTCM notes. None of it is needed to build, deploy or run the
offload.
