<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# How to run the blind receiver in an NTN LEO configuration

This tutorial complements [How to run a NTN configuration](./ntn-configuration.md).
It follows the same assumptions and structure, but enables the blind CFO-PSS
receiver implemented in the `tfm/stage1-final` branch.

It assumes that:

- OAI has already been compiled with `nr-softmodem` and `nr-uesoftmodem`;
- a terrestrial setup, including the core network, already works;
- the gNB can connect to the AMF using the selected configuration;
- gNB and UE are running on the same computer through RFsimulator.

The purpose of this tutorial is only to demonstrate the functional difference
between the original receiver and the blind receiver. Core-network installation
and the initial terrestrial setup are outside its scope.

## NTN channel

The example uses the transparent LEO channel model `SAT_LEO_TRANS`. It
simulates the propagation delay and Doppler of a transparent satellite in a
circular orbit at an altitude of 600 km.

The required channel configuration is already included in:

```text
targets/PROJECTS/GENERIC-NR-5GC/CONF/channelmod_rfsimu_LEO_satellite.conf
```

The gNB and UE configuration files used below already select this channel and
enable the RFsimulator channel model. No separate channel command is needed.

## gNB

Open the first terminal in the root of the OAI repository and enter the build
directory:

```bash
cd cmake_targets
```

Start the LEO NTN gNB:

```bash
sudo ./ran_build/build/nr-softmodem -O ../ci-scripts/conf_files/gnb.sa.band254.u0.25prb.rfsim.ntn-leo.conf --rfsim
```

Wait until the gNB has connected to the core and RFsimulator is waiting for the
UE. Typical messages are:

```text
Received NGSetupResponse
Running as server waiting
```

If the terrestrial setup uses different AMF, PLMN or network-interface values,
keep those working values in the gNB configuration. The blind receiver does
not change the core-network configuration.

## NR UE

Open a second terminal in the OAI repository and enter the build directory:

```bash
cd cmake_targets
```

### Blind receiver

Start the UE without providing the initial Doppler and enable the blind
receiver:

```bash
sudo ./ran_build/build/nr-uesoftmodem -O ../ci-scripts/conf_files/nrue.uicc.ntn-leo.conf --rfsim --rfsimulator.[0].serveraddr 127.0.0.1 --time-sync-I 0.1 --ntn-initial-time-drift -46 --initial-fo 0 --ue-fo-compensation --cont-fo-comp 2 --ue-pss-cfo-search
```

The blind receiver searches jointly for CFO, PSS sequence and timing. The
selected CFO is then compensated before the existing OAI SSS and PBCH
processing.

A successful execution should show that the blind receiver is enabled and
should continue through cell detection, PBCH, SIB19 and registration. Typical
messages are:

```text
UE_pss_cfo_search 1
Found SIB19
PDU Session Establishment Accept
```

The simulated Doppler is normally close to 57 kHz for the initial orbital
position, but its exact value evolves with satellite time.

## Comparison with the original receiver

Stop the UE and gNB with `Ctrl+C`. Restart the gNB with the same command as
before and wait until it is ready.

Then start the UE with the same blind configuration but without
`--ue-pss-cfo-search`:

```bash
sudo ./ran_build/build/nr-uesoftmodem -O ../ci-scripts/conf_files/nrue.uicc.ntn-leo.conf --rfsim --rfsimulator.[0].serveraddr 127.0.0.1 --time-sync-I 0.1 --ntn-initial-time-drift -46 --initial-fo 0 --ue-fo-compensation --cont-fo-comp 2
```

The original receiver is expected to repeat initial-synchronization attempts
without reaching successful PBCH decoding. This is the baseline problem that
the blind receiver addresses.

For a fair comparison, the two UE commands differ only in the presence of:

```text
--ue-pss-cfo-search
```

## Receiver options

The blind CFO-PSS receiver provides the following NR UE options:

| Option | Default | Purpose |
|---|---:|---|
| `--ue-pss-cfo-search` | disabled | Enables the blind joint CFO, PSS sequence and timing search. |
| `--ue-fo-compensation` | disabled | Applies the CFO estimated during synchronization. It is required when the blind receiver is enabled. |
| `--initial-fo <Hz>` | `0` | Applies an initial CFO known in advance. Keep it at zero for a fully blind test. |
| `--ue-pss-cfo-coarse-span <Hz>` | `60000` | Sets the positive and negative limits of the coarse search. |
| `--ue-pss-cfo-coarse-step <Hz>` | `5000` | Sets the separation between coarse CFO hypotheses. |
| `--ue-pss-cfo-fine-span <Hz>` | `6000` | Sets the fine-search range around the winning coarse CFO. |
| `--ue-pss-cfo-fine-step <Hz>` | `400` | Sets the separation between fine CFO hypotheses. |

With the default values, the coarse search covers `-60` to `+60 kHz` in
`5 kHz` steps. The fine search then covers `-6` to `+6 kHz` around the coarse
winner in `400 Hz` steps.

The span must be non-negative, the step must be positive and zero must belong
to each grid. Increasing the span or reducing the step increases the number of
hypotheses and therefore the acquisition time.

The example command also uses general NTN and RFsimulator options:

| Option | Purpose |
|---|---|
| `--rfsim` | Uses RFsimulator instead of physical radio hardware. |
| `--rfsimulator.[0].serveraddr 127.0.0.1` | Connects the UE to a gNB running on the same computer. |
| `--ntn-initial-time-drift -46` | Compensates the initial LEO downlink timing drift, expressed in microseconds per second. |
| `--time-sync-I 0.1` | Configures the integral coefficient of the time-synchronization loop. |
| `--cont-fo-comp 2` | Continuously tracks Doppler after initial synchronization and applies the corresponding uplink pre-compensation. |

The LEO channel model and its propagation parameters come from the
configuration files. The receiver options only control how the UE searches
for and compensates the received signal.
