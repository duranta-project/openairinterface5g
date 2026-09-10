<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# Overview

`--zmq.[0].options chanmod` enables delay, Doppler, path loss, and noise on ZMQ TX and RX.
Without this option, the driver uses the ordinary ZMQ path. The engine operates on `c16_t`
buffers through read/write callbacks; it does not access sockets.

# Architecture

The `channel_emulation` library decorates a radio device's raw callbacks and owns one engine
and channel model per direction, with history and Doppler phase per antenna. The radio driver
retains ownership of transport state. TX and RX share the offline profile, not mutable model state.

- Each output block selects `nsamps` raw samples at `T - channel_offset`, like RFsim's
  shifted read window. Unavailable history is zero-filled.
- Doppler, path gain, and noise are applied to each output block using the same scaling formulas
  as RFsim. Doppler phase advances with the output sample stream.
- Profile lookup runs once per output block using elapsed output samples. RX also publishes
  `start_unix_s + elapsed_samples / sample_rate` for an external process clock shim.

# Usage

```
--device.name oai_zmqdevif --zmq.[0].options chanmod --zmq.[0].modelname <model> [--zmq.[0].ntn_file <profile.csv>]
```

`modelname` supports the single-tap `AWGN` and `SAT_LEO_TLE` models and defaults to `AWGN`.
`ntn_file` is an optional offline NTN delay/Doppler profile CSV (requires `modelname SAT_LEO_TLE`).
Profiles use `time_s,delay_ms,dl_doppler_hz,ul_doppler_hz`.
OAI UE RX selects `dl_doppler_hz` and UE TX selects `ul_doppler_hz`; both use the profile delay.

### Example: OAI NR UE over ZMQ with a static AWGN model

```
sudo ./nr-uesoftmodem -r 106 --numerology 1 --band 78 -C 3619200000 \
  --device.name oai_zmqdevif --zmq.[0].options chanmod --zmq.[0].modelname AWGN \
  --zmq.[0].tx_channels tcp://127.0.0.1:4557 --zmq.[0].rx_channels tcp://127.0.0.1:4556 --ssb 516
```

### Example: dynamic NTN delay/Doppler over ZMQ

See [../../../doc/ntn-configuration.md](../../../doc/ntn-configuration.md) for the full walkthrough
(generating the offline profile, then enabling chanmod on ZMQ).

# Limitations

- Requires contiguous, ordered source samples. ZMQ carries no wire timestamps or sequence numbers
  for reconstructing gaps, duplicates, or reordering.
- History is bounded; evicted samples are zero-filled if requested later.
- No channel-matrix or multipath convolution. The intended models are single-tap identity
  channels such as `AWGN` and `SAT_LEO_TLE`, not arbitrary RFsim models.
- The shared simulation clock advances at RX block granularity. It is intended for simulation;
  preloading it into a process changes that process's realtime, TAI, `time()`, and `gettimeofday()` values.
