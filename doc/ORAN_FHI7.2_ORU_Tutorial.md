<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# OAI O-RU (nr-oru) Configuration and Usage Guide

This document provides a guide for configuring, building, and running the
O-RU executable (`nr-oru`) in OAI. It focuses on the specific configuration
parameters, runtime usage, timing synchronization modes, and implementation
limitations of the O-RU.

For common prerequisites, server hardware setup, DPDK installation, and
PTP synchronization details, please refer to the main
[OAI 7.2 Fronthaul Interface Tutorial](./ORAN_FHI7.2_Tutorial.md).

---

## 1. Compilation

To build the `nr-oru` executable, you must enable the `OAI_RU_FRONTHAUL` CMake
option during the build process.

Ensure you have built and installed DPDK and the ORAN FHI library (as
described in the [OAI 7.2 Fronthaul Interface Tutorial](./ORAN_FHI7.2_Tutorial.md#build-oran-fronthaul-interface-library)).

> [!NOTE]
> DPDK version 22.11 or superior is required for the O-RU

### Building with CMake and Ninja

From your repository root:

```bash
mkdir -p build && cd build
cmake .. -GNinja -DOAI_RU_FRONTHAUL=ON -Dxran_LOCATION=$HOME/phy/fhi_lib/lib
ninja nr-oru
```

This will produce the `nr-oru` executable in `build/`.

---

## 2. Configuration Parameters

The O-RU config file is passed via the `-O` flag. The config file requires
two main configuration blocks:
1. `ORUs`: Configures the O-RU itself and its ORAN 7.2 fronthaul.
2. `RUs`: Configures the underlying OAI radio unit parameters (reused
   by `main_nr_ru.c`).

An example configuration file can be found at
[ru.band77.mu1.106rb.1x1.conf](../targets/PROJECTS/GENERIC-NR-5GC/CONF/ru.band77.mu1.106rb.1x1.conf).

### `ORUs.[0]` parameters

| Parameter | Type | Description | Default |
| :--- | :--- | :--- | :--- |
| `tx_bw` | Integer Array | TX bandwidth list per CC (in PRBs) | `[273]` |
| `rx_bw` | Integer Array | RX bandwidth list per CC (in PRBs) | `[273]` |
| `carrier_tx` | Integer Array | TX carrier frequencies per CC (in KHz) | `[3430560]` |
| `carrier_rx` | Integer Array | RX carrier frequencies per CC (in KHz) | `[3430560]` |
| `frame_type` | Integer | Frame duplex type (`0` = FDD, `1` = TDD) | `1` (TDD) |
| `prach_config_index`| Integer | PRACH configuration index | `152` |
| `prach_msg1_start` | Integer | PRACH MSG1 frequency offset / start PRB | `0` |
| `mu` | Integer | SCS numerology index (e.g. `1` = 30kHz) | `1` |
| `tdd_period` | Integer | TDD periodicity index (0-9) | `5` |
| `num_dl_slots` | Integer | Number of DL slots in TDD period | `3` |
| `num_ul_slots` | Integer | Number of UL slots in TDD period | `1` |
| `num_dl_symbols` | Integer | Number of DL symbols in the mixed slot | `7` |
| `num_ul_symbols` | Integer | Number of UL symbols in the mixed slot | `3` |
| `tx_core` | Integer | CPU core for the South (Split 8) write thread | `-1` |

### `ORUs.[0].fronthaul` parameters

| Parameter | Type | Description | Default / Mandatory |
| :--- | :--- | :--- | :--- |
| `dpdk_devices` | String List | PCIe address list of DPDK interfaces | **Mandatory** |
| `rx_core` | Integer | CPU core for the DPDK RX worker thread | **Mandatory** |
| `north_cores` | Integer Array | CPU affinity per DL north reader (`-1` = unpinned). **List length is the number of DL readers** (max `8`) | `[-1]` (one unpinned reader) |
| `south_core` | Integer | CPU core for the south (UL) reader thread (`-1` = unpinned) | `-1` |
| `ul_worker_cores` | Integer Array | CPU cores for UL Tpool workers; empty falls back to `RUs.tp_cores` | `[]` |
| `du_mac_addr` | String List | Destination MAC addresses of the DU | **Mandatory** |
| `T2a_up` | Int Array (2) | Timing window bounds `(T2a_up_min, T2a_up_max)` | **Mandatory** |
| `T2a_cp` | Int Array (2) | Timing window bounds `(T2a_cp_min, T2a_cp_max)` | **Mandatory** |
| `mtu` | Integer | Maximum Transmission Unit (MTU) | `9600` |
| `prach_eaxc_offset`| Integer | Offset for PRACH eAxC ID mapping | `0` |
| `bfw_num_weights` | Integer | Weights in each ext1/ext11 weight vector from the DU (L_TRX), codebook mode only. `0` = `nb_tx`. Must be `0` or from `nb_tx` to `64`. The O-RU applies the first `nb_tx` (DL) or `nb_rx` (UL) weights. cuBB static weights need `64` | `0` |
| `extra_eal_args` | String List | EAL arguments passed to DPDK | `[]` |

### `RUs.[0]` threadpool parameters

Uplink symbol processing (see [Parallel Uplink Processing](#ii-parallel-uplink-processing))
is dispatched onto `RU_t`'s standard threadpool, so it is configured the same way as any other
OAI RU threadpool:

| Parameter | Type | Description | Default / Mandatory |
| :--- | :--- | :--- | :--- |
| `num_tp_cores` | Integer | Number of UL worker threads in the threadpool | **Mandatory** |
| `tp_cores` | Integer Array | CPU core affinity per worker thread (`-1` = unpinned), length `>= num_tp_cores` | **Mandatory** |

---

## 3. Timing and Synchronization Modes

The `nr-oru` executable runs as an adapter between the O-RAN FH 7.2
interface (connected to the O-DU) and a Split 8 RF device (connected to the
antennas). O-RU currently only supports `vrtsim` and USRP-like devices. It
supports two synchronization paradigms:

### A. Hardware PTP-Synchronized Devices
If the South RF device supports converting UTC timestamps to sample indices
(via `get_timestamp`), the O-RU aligns itself using UTC.
1. The O-RU fetches the reference UTC anchor point from the O-DU via ORAN
   Control Plane using `oru_fh_get_utc_anchor_point`.
2. It queries the RF device using `get_timestamp` to locate the exact
   starting sample index for the specified anchor frame/slot.
3. It discards preceding samples and begins aligned transmission/reception.

> [!IMPORTANT]
> The PTP synchronization option is currently only supported when using the
> virtual RF device (`vrtsim`) as the south interface.

### B. USRP/Generic RF Device Synchronization (Split 8)
For setups using USRP (e.g. B210) without hardware-level UTC sample tagging,
a software synchronization mechanism is used:
1. The O-RU reads a series of throwaway samples in a loop (100 iterations)
   to let the USRP settle.
2. It calls `oru_fh_get_utc_anchor_point` to capture the current O-RAN
   frame/slot boundary time.
3. It maps the USRP sample stream's relative timestamp to this anchor point,
   propagating it between the `south_read` and `north_read` threads.
4. **Note**: USRP RX is always slightly delayed from the Over-the-Air (OTA)
   time due to software/host buffering. This delay is compensated by
   adjusting the O-RU's delay profile parameters (`T2a_up` and `T2a_cp`).

---

## 4. Architecture

`nr-oru` implements two key features to meet real-time processing
requirements:

### I. Dedicated South Write Thread
Writing IQ samples to a some USRP RF devices via `trx_write_func` incurs
a static latency per call.
* To minimize this penalty, a dedicated `south_write_thread` runs on the
  CPU core specified by `tx_core`.
* The thread is TDD-aware and coalesces contiguous symbols within a slot,
  writing them in a single batch to the RF device (`tx_rf_symbols`). This
  reduces CPU overhead compared to symbol-by-symbol writing.

### II. Parallel Uplink Processing
For received U-plane symbols (PUSCH/PRACH), the O-RU must perform cyclic
prefix (CP) removal, FFT, link-level phase compensation (conjugate
rotation), and contiguous format packing.
* Each antenna-symbol is dispatched as a task onto `RU_t`'s shared
  threadpool (configured via `num_tp_cores`/`tp_cores`, see
  [RUs threadpool parameters](#rus0-threadpool-parameters)) instead of a
  dedicated per-antenna worker thread pool.
* Tasks are queued in a lock-free ring (`UL_WORK_QUEUE_DEPTH` = 128 entries).
  If the threadpool falls behind and the ring fills up, new UL jobs are
  **dropped** rather than blocking the reader thread; dropped jobs are
  counted and reported by the [self-diagnostic report](#6-self-diagnostic-report).
* All RX antennas for a symbol are processed in parallel across the
  threadpool's worker threads, ensuring the data is processed and sent
  back to the O-DU within the timing budget.

### III. Parallel Downlink Processing
For transmitted DL symbols, the O-RU must perform phase rotation, an
FFT-shift, and cyclic prefix (CP) insertion before handing samples to the
South (Split 8) write thread.
* One `oru_north_read_worker` runs per `north_cores` entry, each executing
  the full read/process/publish loop for a symbol (e.g. `north_cores =
  [-1, -1, -1, -1]` starts four unpinned readers; `north_cores = [21, 22]`
  starts two pinned ones).
* Because several threads complete symbols concurrently, completions can
  arrive out of order. A generic reorder buffer
  (`common/utils/symbol_reorder`) tracks completions by absolute symbol
  index and reports a contiguous high-water mark; each symbol is released
  to `oru_south_write_thread` as soon as it individually completes,
  instead of being held back (head-of-line-blocked) behind an earlier,
  still-incomplete neighbor.
* `oru_south_write_thread` blocks on `symbol_reorder_wait_at_least()` and
  is woken directly whenever the contiguous frontier advances.

---

## 5. Timing Delay Profile (Lookahead Windows)

The O-RU validates incoming DL packets using the timing windows configured in
`T2a_up` and `T2a_cp`.

The O-RU supports large delay profiles and lookaheads **greater than 5ms**.

---

## 6. Self-Diagnostic Report

`nr-oru` continuously measures the wall-clock time spent processing each DL
symbol and each UL antenna-symbol, and once per hyperframe (every 1024
frames) logs a report (`oru_self_diagnosis`) comparing that measured cost
against the real-time symbol budget.

The report includes, per direction (DL/UL):
* Number of symbols (or antenna-symbols) processed in the window.
* Per-thread processing time (Avg/Max).
* **Effective** processing time — the per-thread time scaled by how many
  worker threads (`north_cores` length for DL, `num_tp_cores` for UL) actually
  run concurrently on the available CPU cores.
* A safety margin relative to the physical symbol duration, with a
  `PASS` / `WARNING` (margin < 20%) / `CRITICAL` (budget exceeded)
  diagnosis.
* For UL, the number of jobs dropped because the threadpool queue was
  full — any nonzero count is reported as `CRITICAL`.

The report ends with an `OVERALL STATUS: PASS` or `OVERALL STATUS: FAIL`
line, letting a deployment be validated for real-time operation just by
watching the log (`LOG_I`/`LOG_W`/`LOG_E`, component `PHY`).

---

## 7. Existing Limitations

When configuring and deploying `nr-oru`, be aware of the following design and
implementation constraints:

1. **Supported Devices**: The O-RU currently only supports `vrtsim` and
   USRP-like devices (using standard UHD drivers)
2. **TDD Only**
3. **PRACH Format B4 Only**: The O-RU only supports short PRACH Format B4
   (NFAPI format 8)
4. **Single PRACH Occasion**: The number of PRACH occasions per subframe
   (`num_prach_ocas`) is hardcoded to `1` in `prepare_prach_item`.
5. **No ORAN Compression Support**: The O-RU packet processor only supports
   uncompressed U-plane packets (16-bit IQ samples). Incoming DL packets
   with compression enabled will trigger a crash
   (`AssertFatal(compMeth == 0, "Compression not supported\n")`). The O-DU
   must be configured in uncompressed mode.
6. **USRP Timing Jitter**: In USRP Split 8 mode, synchronization depends on
   software time-tagging. While functional, it has higher timing jitter
   compared to a hardware-synchronized PTP interface. Ensure lookahead
   windows (`T2a_up`/`T2a_cp`) are set wide enough to absorb this jitter.

---

## 8. Execution Examples

### Running the O-RU

```bash
sudo -E ./nr-oru -O ../targets/PROJECTS/GENERIC-NR-5GC/CONF/ru.band77.mu1.106rb.1x1.conf --device.name vrtsim --vrtsim.role server
```

*Note 1: Drop `--device.name vrtsim --vrtsim.role server` when running with a USRP.*

### Running the matching O-DU (gNB)

```bash
sudo -E ./nr-softmodem -O ../targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.band77.mu1.106rb.fhi.1x1.conf
```

### Running the UE

```bash
sudo -E ./nr-uesoftmodem -C 4049760000 -r 106 --numerology 1 --ssb 516 --device.name vrtsim
```
