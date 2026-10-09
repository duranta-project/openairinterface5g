<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# Overview

This document describes the E3 Agent for dApps integrated in OAI gNB, and explains how to
build, configure, and connect a dApp.

[[_TOC_]]

dApps can be real-time microservices co-located with the RAN node (DU or CU). Unlike xApps,
which run on a nearRT-RIC and operate at 10 ms–1 s timescales, dApps execute directly on the
RAN node and interact with user-plane data that is unavailable to RICs due to timing or
privacy constraints. This enables sub-millisecond control loops for use cases such as spectrum
sharing, positioning, and scheduling optimization.

The E3 interface connects the gNB to co-located dApps. The `libe3` library implements both
sides of this interface — the RAN agent (this module) and the dApp client — using a unified
C++/Python API that hides transport, encoding, and protocol complexity.

For a complete description of the dApp architecture and the E3 interface, refer to:

- [Paper: dApp architecture and E3 interface (Computer Networks, 2025)](https://doi.org/10.1016/j.comnet.2025.111342)
- [Tutorial: deploying dApps with OAI](https://openrangym.com/tutorials/dapps-oai)
- [dApp Python library](https://pypi.org/project/dapps/)

## Prerequisites — libe3

The E3 Agent requires the external `libe3` library (any 0.2.x release; this tree is tested with
0.2.3), discovered at build time with `pkg-config`. It is not vendored in OAI. Install it once, with
every wire encoding, so the gNB can serve whichever one its dApp uses:

```bash
git clone --branch 0.2.3 https://github.com/wineslab/libe3 && cd libe3
./build_libe3 -I                          # system prerequisites, if not already present
./build_libe3 --all-encodings --install   # into /usr/local; asks for sudo
```

The build then finds it with no further setup. A libe3 installed elsewhere (`--prefix DIR`) needs
`DIR/lib/pkgconfig` on `PKG_CONFIG_PATH` when building OAI.

## Build OAI with the E3 Agent

The E3 Agent is **opt-in** and gated behind the `E3_AGENT` CMake option (`OFF` by default).
The default OAI build is entirely unaffected when `E3_AGENT` is not set.

```bash
cd cmake_targets/
./build_oai -w SIMU --ninja --gNB --nrUE --build-e3
```

To build without the E3 Agent, omit `--build-e3`. To clean and rebuild, add the `-c` flag.

## Configuration

The agent reads an optional `E3Configuration` block from the gNB configuration file.
If the block is absent, the agent falls back to built-in defaults (`posix`/`ipc`).

```
E3Configuration : {
    link      = "zmq";   # posix | zmq
    transport = "ipc";   # tcp | sctp | ipc
};
```

The same parameters can be set on the command line without editing the config file:

```bash
sudo ./nr-softmodem -O <gnb.conf> ... --E3Configuration.link zmq --E3Configuration.transport ipc
```

Valid link/transport combinations:

| link  | transport |
|-------|-----------|
| zmq   | ipc       |
| zmq   | tcp       |
| posix | ipc       |
| posix | tcp       |
| posix | sctp      |

## Service models

A service model (SM) defines one topic a dApp can subscribe to. It is identified on
the wire by a RAN-function id and registered with the agent at startup.

| RF id | Service model | Carries | Details |
|------:|---------------|---------|---------|
| 1 | Spectrum SM | uplink sensing ranges: the free time-frequency tiles of each UL slot | [`service_models/spectrum_sm`](service_models/spectrum_sm/README.md) |
| 2 | L1-KPM SM | post-FFT IQ from the PHY | [`service_models/l1_kpm_sm`](service_models/l1_kpm_sm/README.md) |

Both are telemetry-out only. Code is split by what it depends on:

- `service_models/<sm>/` holds the message definitions (ASN.1 and Protocol Buffers),
  the encoders and the SM callbacks. `service_models/e3_sm_worker.c` (the telemetry
  thread driver) and `service_models/e3_shm_region.c` (shared-memory lifecycle) are
  shared by all SMs.
- `ran_func_*.c` in this directory is the part of a RAN function that reads or writes
  gNB state. The Spectrum RAN function is coupled to the MAC and is linked into the
  MAC library, so the scheduler only calls the hooks in `ran_func_spectrum_extern.h`.

The wire encoding (`asn1`, `json` or `protobuf`) is chosen per run with
`E3Configuration.encoding` (default `asn1`) and applies to every SM. The keys
`setup_port`, `subscriber_port` and `publisher_port` (0 = libe3 default) and
`enabled_sms` (SM ids to register, empty = all) live in the same section, as do the
sensing keys described in the Spectrum SM README.
[`targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.sa.band78.106prb.rfsim.e3.conf`](../../targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.sa.band78.106prb.rfsim.e3.conf)
is a complete example.

## dApp development

dApps are written in Python using the `dapps` library, in a virtual environment on the machine
where libe3 is installed (its Python binding, `libe3py`, is built against it when pip installs it):

```bash
pip install "dapps[all]"
```

A dApp subclasses the `DApp` base class and implements service-model-specific encode/decode
and a control loop. The library handles the E3 connection transparently: setup handshake,
subscription, indication receive, and control send.

For a full end-to-end example including service model usage, follow the
[tutorial on the OpenRAN Gym website](https://openrangym.com/tutorials/dapps-oai).

## Start the process

### Start the gNB with E3 agent enabled

The example below uses RFsim for local testing. A 5G Core Network must already be running;
see [`doc/NR_SA_Tutorial_OAI_CN5G.md`](../../doc/NR_SA_Tutorial_OAI_CN5G.md).

```bash
cd cmake_targets/ran_build/build
sudo ./nr-softmodem \
  -O <path>/targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.sa.band78.fr1.106PRB.pci0.rfsim.conf \
  --rfsim \
  --E3Configuration.link zmq --E3Configuration.transport ipc
```

If the agent initializes correctly, the log will show `Init E3 Agent` and the setup socket
will appear at `/tmp/dapps/setup`.

An `E3AP` log component is available for debugging: pass `--log_config.e3ap_log_level debug`
to increase verbosity.

### Connect a dApp

Follow the [deployment tutorial](https://openrangym.com/tutorials/dapps-oai) for a complete
end-to-end example.
