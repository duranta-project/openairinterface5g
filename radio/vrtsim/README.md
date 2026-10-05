<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# Overview

This implements a shared memory realtime and near-realtime radio interface.
This is performed using `shm_td_iq_channel` which handles the shared memory
interface.

# Architecture

The `vrtsim` architecture follows a server-client model:

- **Server (gNB)**: On startup, the server writes connection information to a
  file as provided in `--vrtsim.descriptor`, default location is
  `/tmp/vrtsim_connection`. This file contains information required by the client
  to establish communication.
- **Client (UE)**: The client reads this file from, verifies its contents, and uses
  the provided information to open and connect to the shared memory channel.
- **Shared Memory Channel**: Both server and client use the `shm_td_iq_channel` to
  exchange IQ samples and control data in real time or near-real time.

## Channel modelling:


# Limitations

 - Only 1gNB to 1nrUE: gNB as a server, UE as a client.

# Usage

On the UE and gNB: use `device.name vrtsim` command line argument.

Additionally on gNB use `vrtsim.role server` and optionally
`vrtsim.timescale <timescale>` to set the timescale. Timescale 1.0
is the default and means realtime.

To use `vrtsim` in containers, you have to enable inter-process communication,
and mount the directory of the connection information in a shared volume
between server and clients. For instance, in docker and default
`vrtsim.connection_descriptor`, set `ipc: host` and share the server's and
client's `/tmp` directory through a volume. For an example, refer to
[this docker-compose file](../../ci-scripts/yaml_files/5g_vrtsim_cirdb/docker-compose.yaml).

Channel modelling can be enabled by adding `vrtsim.chanmod 1` to the
command line and should work the same as channel modelling in rfsimulator,
see rfsimulator [documentation](../rfsimulator/README.md) with a slight difference:
channel modelling is done on transmission instead of reception. To refect this,
the model names are slightly different: the server expects a model named
`server_tx_channel_model` while the client expects a model named `client_tx_channel_model`

Without CUDA, the convolution runs on the CPU thread pool (`vrtsim.tpool`). The CPU
pipeline picks, per call, between a direct convolution and an FFT overlap-save
convolution (faster for longer channels and more antennas), and uses the best SIMD
kernels the CPU supports at runtime (AVX512F, AVX2, NEON; SVE2 when its vectors are
wider than 128 bits), independently of the flags the binary was built with.

Additionally, `taps_client` is available in vrtsim. This allows to connect to
channel emulation server and receive external taps (or channel impulse responses).
To use external taps, use `--taps-socket` and provide a nanomsg PUB socket address
that will publish the taps.

# Debugging

## Realtime issues

Realtime issues can prevent the UE from connecting to the gNB, especially with
channel modelling enabled. Consider adjusting `timescale` parameter until
the UE can connect reliably.

On exit, vrtsim will output a histogram of the measured transmission budget, which
is defined as the number of microseconds before the transmitted samples could be
read by peer.

Example:

```
[HW]   VRTSIM: Average TX budget 939.210 uS (more is better)
[HW]   VRTSIM: TX budget histogram: 216 samples
[HW]   Bin 0    [0.0 - 100.0uS]:                0
[HW]   Bin 1    [100.0 - 200.0uS]:              0
[HW]   Bin 2    [200.0 - 300.0uS]:              0
[HW]   Bin 3    [300.0 - 400.0uS]:              0
[HW]   Bin 4    [400.0 - 500.0uS]:              0
[HW]   Bin 5    [500.0 - 600.0uS]:              0
[HW]   Bin 6    [600.0 - 700.0uS]:              0
[HW]   Bin 7    [700.0 - 800.0uS]:              0
[HW]   Bin 8    [800.0 - 900.0uS]:              0
[HW]   Bin 9    [900.0 - 1000.0uS]:             117
[HW]   Bin 10   [1000.0 - 1100.0uS]:            0
[HW]   Bin 11   [1100.0 - 1200.0uS]:            0
[HW]   Bin 12   [1200.0 - 1300.0uS]:            0
[HW]   Bin 13   [1300.0 - 1400.0uS]:            0
[HW]   Bin 14   [1400.0 - 1500.0uS]:            0
[HW]   Bin 15   [1500.0 - 1600.0uS]:            0
[HW]   Bin 16   [1600.0 - 1700.0uS]:            0
[HW]   Bin 17   [1700.0 - 1800.0uS]:            0
[HW]   Bin 18   [1800.0 - 1900.0uS]:            0
[HW]   Bin 19   [1900.0 - 2000.0uS]:            0
[HW]   Bin 20   [2000.0 - 2100.0uS]:            0
[HW]   Bin 21   [2100.0 - 2200.0uS]:            0
[HW]   Bin 22   [2200.0 - 2300.0uS]:            0
[HW]   Bin 23   [2300.0 - 2400.0uS]:            0
[HW]   Bin 24   [2400.0 - 2500.0uS]:            0
[HW]   Bin 25   [2500.0 - 2600.0uS]:            0
[HW]   Bin 26   [2600.0 - 2700.0uS]:            0
[HW]   Bin 27   [2700.0 - 2800.0uS]:            0
[HW]   Bin 28   [2800.0 - 2900.0uS]:            0
[HW]   Bin 29   [2900.0 - 3000.0uS]:            0
```

Samples in bin 0 indicate realtime issues.
