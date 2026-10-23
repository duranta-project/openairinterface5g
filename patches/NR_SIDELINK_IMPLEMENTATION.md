<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# NR Sidelink Implementation

NR sidelink enables UEs to communicate directly over the PC5 radio interface.
User data travels between UEs without being forwarded through a gNB or the
mobile core. Sidelink can operate within network coverage or outside it,
depending on how synchronization and radio resources are configured.

In resource allocation Mode 1, the network schedules sidelink resources. In
Mode 2, UEs select resources autonomously from configured resource pools.
Synchronization gives the participating UEs a common timing reference;
control information describes the transmissions, and shared-channel resources
carry the user data.

This document describes the OAI NR sidelink Mode 2 implementation, its main
configuration options, and the procedures used to run and validate it. The
implementation supports out-of-coverage UE-to-UE communication with static
sidelink preconfiguration.

[[_TOC_]]

## Scope

In Mode 2, a UE selects PSCCH and PSSCH resources from a preconfigured resource
pool without a gNB scheduler. OAI supports the following UE roles:

| Role | Command-line options | Behavior |
|---|---|---|
| Synchronization reference | `--sl-mode 2 --sync-ref 4` | Uses local timing and transmits SL-SSB/PSBCH. |
| Nearby UE | `--sl-mode 2` | Receives SL-SSB/PSBCH and synchronizes to the reference UE. |

After synchronization, the UEs select sidelink resources autonomously and
exchange user-plane traffic over PC5. The current implementation supports one
sidelink frequency, one bandwidth part, one active RX pool, and one active TX
pool.

Reference configuration files are available for the
[synchronization-reference UE](../../targets/PROJECTS/NR-SIDELINK/CONF/sl_sync_ref.conf),
the [nearby UE](../../targets/PROJECTS/NR-SIDELINK/CONF/sl_ue1.conf), and a
[resource-pool example](../../targets/PROJECTS/NR-SIDELINK/CONF/sidelink_preconfig_1rxpool_1txpool.conf).
The carrier, numerology, bandwidth part, TDD pattern, sidelink symbols,
SL-SSB occasions, resource-pool geometry, and PSFCH configuration must be
compatible at both UEs.

## Architecture

```mermaid
flowchart LR
    RRC["RRC sidelink preconfiguration"] --> MAC["MAC resource-pool configuration"]
    MAC --> SELECT["Mode 2 resource selection"]
    SELECT --> SCI["SCI and SL-SCH scheduling"]
    SCI --> PHY["PSCCH and PSSCH processing"]
    PHY --> HARQ["HARQ processing"]
    HARQ --> PSFCH["PSFCH feedback"]
```

### Synchronization

The synchronization-reference UE transmits the sidelink synchronization signal
block (SL-SSB), including PSBCH. A nearby UE searches for the SL-SSB, decodes
the sidelink MIB, and adopts the reference timing before normal sidelink
scheduling starts.

### Resource-pool handling

RRC provides the static sidelink preconfiguration to MAC. MAC derives the
physical TX and RX pool occasions from the TDD pattern, sidelink symbol
allocation, SL-SSB occasions, and `sl-TimeResource` bitmap.

Mode 2 resource selection uses the configured selection window and the active
resource pool. The scheduler considers available RLC data, sensing information,
and previous transmissions when selecting resources.

### PSCCH and PSSCH

PSCCH carries SCI-1A, which describes the PSSCH allocation and related resource
reservations. PSSCH carries SCI-2 and the sidelink transport block. The
implementation supports allocations spanning one or more configured
subchannels and the configured sidelink DM-RS patterns.

The transmit and receive paths use the SCI allocation to configure channel
estimation, demodulation, rate matching, and SL-SCH decoding. The supported
modulation and coding scheme is selected through the sidelink configuration
and command-line options.

### PSFCH and HARQ

When sidelink HARQ feedback is enabled, the receiving UE derives the PSFCH
occasion and resource from the PSSCH allocation and resource-pool
configuration. Feedback behavior depends on the SCI-2 format, cast type, and
HARQ-feedback configuration.

A sidelink HARQ process retains the transport block and scheduling information
required for retransmission. ACK completes the process. NACK, DTX, or feedback
timeout may trigger retransmission according to the configured HARQ behavior.

## Configuration

Configuration values are expressed either as physical values or as ASN.1
enumeration indices. The form expected by each parameter must be preserved.

| Field | Representation | Description |
|---|---|---|
| `sl_AdaptiveMCS` | Boolean | Enables BLER-based MCS adaptation when set to `1`. |
| `sl_PSSCH_NumSubchannels` | Physical count | Number of contiguous subchannels used for PSSCH. |
| `sl_LengthSymbols` | ASN.1 index | Values `0` through `7` represent 7 through 14 sidelink symbols. |
| `sl_TimeResourcePSCCH` | ASN.1 index | `0` selects 2 symbols; `1` selects 3 symbols. |
| `sl_FreqResourcePSCCH` | ASN.1 index | Selects 10, 12, 15, 20, or 25 PSCCH RBs. |
| `sl_SubchannelSize` | ASN.1 index | Selects 10, 12, 15, 20, 25, 50, 75, or 100 RBs. |
| `sl_PSFCH_Period` | ASN.1 index | `0`, `1`, `2`, and `3` represent `sl0`, `sl1`, `sl2`, and `sl4`. |
| `sl_NumMuxCS_Pair` | ASN.1 index | Values `0` through `3` represent one through four cyclic-shift pairs. |
| `sl_MinTimeGapPSFCH` | ASN.1 index | `0` represents `sl2`; `1` represents `sl3`. |
| `sl_PSFCH_CandidateResourceType` | ASN.1 index | `0` selects `startSubCH`; `1` selects `allocSubCH`. |
| `sl_SelectionWindow` | Milliseconds | Accepted values are 1, 5, 10, and 20. |
| `sl_SensingWindow` | Milliseconds | Accepted values are 100 and 1100. |
| `sl_ResourceReservePeriod` | Milliseconds | Accepted values are 0 through 1000 in steps of 100. |

For PSFCH operation, corresponding TX and RX pools at both UEs must use a
compatible PSFCH period, minimum time gap, RB set, and cyclic-shift
configuration. `sl_PSSCH_NumSubchannels` must be between one and the number of
subchannels in the active resource pool.

## Build

From the repository root:

```bash
mkdir -p build
cd build
cmake .. -GNinja -DENABLE_PHYSIM_TESTS=ON
ninja nr-uesoftmodem nr_scisim
```

## Run two UEs with the RF simulator

The UEs must use separate network namespaces because both create
`oaitun_ue1`. From `build/`, start the synchronization-reference UE in the host
namespace:

```bash
sudo ./nr-uesoftmodem \
  -O ../targets/PROJECTS/NR-SIDELINK/CONF/sl_sync_ref.conf \
  --sl-mode 2 --sync-ref 4 --rfsim
```

Create a namespace for the nearby UE, then start the UE from a shell in that
namespace:

```bash
sudo ../tools/scripts/multi-ue.sh -c1 -e
./nr-uesoftmodem \
  -O ../targets/PROJECTS/NR-SIDELINK/CONF/sl_ue1.conf \
  --sl-mode 2 --rfsim --rfsimulator.[0].serveraddr 10.201.1.100
```

Open another shell in the namespace to generate traffic:

```bash
sudo ../tools/scripts/multi-ue.sh -o1
ip address show oaitun_ue1
ping -I oaitun_ue1 -c 20 10.0.0.5
```

For a throughput test, start an `iperf3` server for the
synchronization-reference UE and a client for the nearby UE:

```bash
iperf3 -s -B 10.0.0.5
iperf3 -c 10.0.0.5 -B 10.0.0.1 -u -b 6M -t 30 -i 1
```

After stopping both UEs, remove the namespace:

```bash
sudo ../tools/scripts/multi-ue.sh -d1
```

## Validation

Run the sidelink SCI simulator from the repository root:

```bash
build/nr_scisim
```

The simulator exercises PSCCH/SCI, PSSCH/SL-SCH, and PSFCH processing. PHY
changes should also be checked with the quick physical-simulator suite before
running the complete suite:

```bash
cd build
ctest -R '^physim\.' --output-on-failure
```

For an RF-simulator test, confirm that the nearby UE synchronizes to the
reference UE, sidelink traffic passes in both directions, and the expected
PSCCH, PSSCH, PSFCH, and HARQ activity appears in the UE logs.

## Limitations

The implementation currently targets static, out-of-coverage Mode 2
operation. It supports one sidelink frequency, one bandwidth part, one active
RX pool, and one active TX pool. Additional resource-pool topologies,
standards-complete sensing-based exclusion, congestion control, and
multi-destination resource selection require further development and
validation.
