<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# How to run a NTN configuration

This tutorial explains how to run non-terrestrial network (NTN) setup for both
low-earth orbit (LEO) and geostationary orbit (GEO) scenarios.

It assumes you have a working terrestrial network (TN) setup, include core
network.

[[_TOC_]]

## NTN channel

A 5G NR NTN configuration only works in a non-terrestrial setup.
Therefore either SDR boards and a dedicated NTN channel emulator are required, or RFsimulator has to be configured to simulate a NTN channel.

As shown on the [rfsimulator page](../radio/rfsimulator/README.md), RFsimulator provides different possibilities.
E.g. to perform a simple simulation of a satellite in geostationary orbit (GEO), these parameters should be added to both gNB and UE command lines:
```
--rfsimulator.[0].prop_delay 238.74
```

For simulation of a satellite in low earth orbit (LEO), two channel models have been added to rfsimulator:
- `SAT_LEO_TRANS`: transparent LEO satellite with gNB on ground
- `SAT_LEO_REGEN`: regenerative LEO satellite with gNB on board

Both channel models simulate the delay and Doppler for a circular orbit at 600 km height according to the Matlab function [dopplerShiftCircularOrbit](https://de.mathworks.com/help/satcom/ref/dopplershiftcircularorbit.html).
An example configuration to simulate a transparent LEO satellite with rfsimulator would be:
```
channelmod = {
  max_chan=10;
  modellist="modellist_rfsimu_1";
  modellist_rfsimu_1 = (
    {
      model_name     = "rfsimu_channel_enB0"
      type           = "SAT_LEO_TRANS";
      noise_power_dB = -100;
    },
    {
      model_name     = "rfsimu_channel_ue0"
      type           = "SAT_LEO_TRANS";
      noise_power_dB = -100;
    }
  );
};
```
This configuration is also provided in the file `targets/PROJECTS/GENERIC-NR-5GC/CONF/channelmod_rfsimu_LEO_satellite.conf`.

Additionally, rfsimulator has to be configured to apply the channel model.
This can be done by either providing this line in the conf file in section `rfsimulator`:
```
  options = ("chanmod");
```
Or by providing this the the command line parameters:
```
--rfsimulator.[0].options chanmod
```

### TLE-based LEO Channel Emulation for ZMQ

A channel emulator is integrated with ZMQ for TLE-based LEO test.
ZMQ replays offline TLE-generated NTN profiles with `--zmq.[0].options chanmod`. The profile contains delay and UL/DL Doppler data.
Profile lookup uses elapsed output samples, so channel evolution remains in lockstep with the
softmodem sample stream. A preload shim makes OCUDU realtime and TAI follow the same simulation clock. See
[radio/channel_emulation/README.md](../radio/channel_emulation/README.md)
for architecture and limitations.

## gNB

The main parameters to cope with the large NTN propagation delay are cellSpecificKoffset, ta-Common, ta-CommonDrift and the ephemeris data (satellite position and velocity vectors).

The parameter `ntn-UlSyncValidityDuration-r17` indicates the maximum time duration from epochtime during which the UE can apply assistance information without having acquired new assistance information. i.e this enables UE to re-read SIB19 before timer with value ulSyncValidityDuration expires. The unit of the field is in seconds. Example values for GEO: 240s, MEO: 20s, LEO: 5s.
As epochtime is not implemented yet, UE starts the timer from the TTI where SIB19 is received with ntn-UlSyncValidityDuration-r17 IE.

The parameter `cellSpecificKoffset_r17` is the scheduling offset used for the timing relationships that are modified for NTN (see TS 38.213).
The unit of the field Koffset is number of slots for a given subcarrier spacing of 15 kHz.

The parameter `ta-Common-r17` is used to provide the propagation delay between the reference point (at the gNB) and the satellite.
The granularity of ta-Common is 4.072 × 10^(-3) µs. Values are given in unit of corresponding granularity.

The parameter `ta-CommonDrift-r17` indicates the drift rate of the common TA.
The granularity of ta-CommonDrift is 0.2 × 10^(-3) µs/s. Values are given in unit of corresponding granularity.

The satellite position and velocity vartors are provided using the following parameters:

`positionX-r17`, `positionY-r17`, `positionZ-r17`:
X, Y, Z coordinate of satellite position state vector in ECEF. Unit is meter.
Step of 1.3 m. Actual value = field value * 1.3.

`velocityVX-r17`, `velocityVY-r17`, `velocityVZ-r17`:
X, Y, Z coordinate of satellite velocity state vector in ECEF. Unit is meter/second.
Step of 0.06 m/s. Actual value = field value * 0.06.

These parameters can be provided to the gNB in the conf file in the section `servingCellConfigCommon`:
```
...
# GEO satellite
      cellSpecificKoffset_r17                                       = 478;
      ta-Common-r17                                                 = 58629666; # 238.74 ms
      positionX-r17                                                 = 0;
      positionY-r17                                                 = 0;
      positionZ-r17                                                 = 32433846;
      velocityVX-r17                                                = 0;
      velocityVY-r17                                                = 0;
      velocityVZ-r17                                                = 0;
# LEO satellite
#      cellSpecificKoffset_r17                                       = 40;
#      ta-Common-r17                                                 = 4634000; # 18.87 ms
#      ta-CommonDrift-r17                                            = -230000; # -46 µs/s
#      positionX-r17                                                 = 0;
#      positionY-r17                                                 = -2166908; # -2816980.4 m
#      positionZ-r17                                                 =  4910784; #  6384019.2 m
#      velocityVX-r17                                                = 0;
#      velocityVY-r17                                                = 115246; # 6914.76 m/s
#      velocityVZ-r17                                                =  50853; # 3051.18 m/s
...
```

Besides this, some timers, e.g. `sr_ProhibitTimer_v1700`, `t300`, `t301` and `t319`,  in the conf file section `gNBs.[0].TIMERS` might need to be extended for GEO satellites.
```
...
    TIMERS :
    {
      sr_ProhibitTimer       = 0;
      sr_TransMax            = 64;
      sr_ProhibitTimer_v1700 = 512;
      t300                   = 2000;
      t301                   = 2000;
      t319                   = 2000;
    };
...
```

To improve the achievable UL and DL throughput in conditions with large RTT (esp. GEO satellites), there is a feature defined in REL17 to disable HARQ feedback.
This allows to reuse HARQ processes immediately, but it breaks compatibility with UEs not supporting this REL17 feature.
To enable this feature, the `disable_harq` flag has to be added to the gNB conf file in the section `gNBs.[0]`
```
...
    min_rxtxtime = 6;
    disable_harq = 1; // <--

    servingCellConfigCommon = (
    {
...
```

The settings for a transparent GEO satellite scenario are already provided in the file `ci-scripts/conf_files/gnb.sa.band254.u0.25prb.rfsim.ntn.conf`.
Using this conf file, an example gNB command for FDD, 5 MHz BW, 15 kHz SCS, transparent GEO satellite 5G NR NTN is this:
```
cd cmake_targets
sudo ./ran_build/build/nr-softmodem -O ../ci-scripts/conf_files/gnb.sa.band254.u0.25prb.rfsim.ntn.conf --rfsim
```

To configure NTN gNB with 32 HARQ processes in downlink and uplink, add these settings in conf files under section `gNBs.[0]`
```
...
    num_dlharq = 32;
    num_ulharq = 32;
...
```

To simulate a LEO satellite channel model with rfsimulator in UL (DL is simulated at the UE side) either the `channelmod` section as shown before has to be added to the gNB conf file, or a channelmod conf file has to be included like this:
```
@include "channelmod_rfsimu_LEO_satellite.conf"
```

The settings for a transparent LEO satellite scenario are already provided in the file `ci-scripts/conf_files/gnb.sa.band254.u0.25prb.rfsim.ntn-leo.conf`.
Using this conf file, an example gNB command for FDD, 5 MHz BW, 15 kHz SCS, trasparent LEO satellite 5G NR NTN is this:
```
cd cmake_targets
sudo ./ran_build/build/nr-softmodem -O ../ci-scripts/conf_files/gnb.sa.band254.u0.25prb.rfsim.ntn-leo.conf --rfsim
```

## NR UE

At UE side, only few parameters have to be provided, as the UE receives most relevant parameters via SIB19 from the gNB.
But to calculate the UE specific TA, the UE position has to be provided in the `ue.conf` file.
Also the LEO channel model has to be configured, e.g. by using an `@include` statement, just like on the gNB side:
```
...
position0 = {
    x = 0.0;
    y = 0.0;
    z = 6377900.0;
}

@include "channelmod_rfsimu_LEO_satellite.conf"
```

So an example NR UE command for FDD, 5MHz BW, 15 kHz SCS, transparent GEO satellite 5G NR NTN is this:
```
cd cmake_targets
sudo ./ran_build/build/nr-uesoftmodem -O ../targets/PROJECTS/GENERIC-NR-5GC/CONF/ue.conf --band 254 -C 2488400000 --CO -873500000 -r 25 --numerology 0 --ssb 60 --rfsim --rfsimulator.[0].prop_delay 238.74
```

For LEO satellite scenarios, the parameter `--ntn-initial-time-drift` must be provided via command line, as the UE needs this value to compensate for the time drift during initial sync, before SIB19 was received.
This parameter provides the drift rate of the complete DL timing (incl. feeder link and service link) in µs/s.

To perform an autonomous TA update based on the DL drift, the boolean parameter `--autonomous-ta` can be added.
If that parameter is omitted, the TA is continuously computed based on the SIB19 information.

For LEO satellite scenario we assume the LO to be very accurate and the main FO contribution comes from Doppler shift.
Therefore, we use the command line parameter `--cont-fo-comp 2` to continuously compensate the DL Doppler and pre-compensate the UL Doppler.
The initial Doppler frequency offset must be provided via command line with the parameter `--initial-fo`.

For other information on optional NR UE command line options, please refer [here](#optional-nr-ue-command-line-options).

So an example NR UE command for FDD, 5MHz BW, 15 kHz SCS, transparent LEO satellite 5G NR NTN is this:
```
cd cmake_targets
sudo ./ran_build/build/nr-uesoftmodem -O ../targets/PROJECTS/GENERIC-NR-5GC/CONF/ue.conf --band 254 -C 2488400000 --CO -873500000 -r 25 --numerology 0 --ssb 60 --rfsim --rfsimulator.[0].prop_delay 20 --rfsimulator.[0].options chanmod --time-sync-I 0.1 --ntn-initial-time-drift -46 --initial-fo 57340 --cont-fo-comp 2
```

### LEO test with OCUDU gNB over ZMQ

OAI UE can be tested with OCUDU gNB using TLE-based LEO channel emulation over ZMQ. Check OCUDU project for how to generate NTN configs from TLE data. Follow below steps:

1. **Generate the TLE profile before starting the radio**:
  ```bash
  python3 tools/ntn/ntn_tle_profile_generator.py --duration 60 --step 0.01 \
    --tle-file "$OCUDU_SCENARIO_DIR/tle_updated.txt" \
    --starting-time-config "$OCUDU_SCENARIO_DIR/sat.yml" \
    --ue-position-file "$OCUDU_SCENARIO_DIR/ue-position.cfg" \
    --gateway-position-file "$OCUDU_SCENARIO_DIR/gw-position.cfg" \
    --out-file /tmp/ntn-profile.csv
  ```
  `--tle-file` accepts a standard two- or three-line TLE file, use OCUDU generated `tle_updated.txt`. Use `--starting-time-config` with the matching `sat.yml` to read the
  serving satellite epoch broadcast by OCUDU, and use the matching `ue-position.cfg` and
  `gw-position.cfg` files for the link endpoints.

  `--sat-type` supports `transparent` and `regenerative`. DL and UL frequencies default
  to 2185 MHz and 1995 MHz. See `--help` for frequency overrides.

2. **Initialize the simulation clock and start OCUDU gNB**:
  ```bash
  ninja -C cmake_targets/ran_build/build ntn_sim_clock_shim
  sudo python3 tools/ntn/init_ntn_sim_clock.py /tmp/ntn-profile.csv
  sudo env OAI_NTN_SIM_CLOCK_SHM=/oai_ntn_sim_clock \
    LD_PRELOAD="$PWD/cmake_targets/ran_build/build/libntn_sim_clock_shim.so" \
    /path/to/ocudu_gnb ...
  ```
  The shim replaces `CLOCK_REALTIME`, `CLOCK_TAI`, `time()`, and `gettimeofday()` for OCUDU.
  Monotonic clocks used for scheduling and timeouts are unchanged. `CLOCK_TAI` uses a 37-second
  UTC offset by default; set `OAI_NTN_TAI_OFFSET` if a different offset is required.

3. **Start the OAI UE**:
  ```bash
  python3 tools/ntn/launch_ntn_ue.py --profile /tmp/ntn-profile.csv \
    --ue-position-file "$OCUDU_SCENARIO_DIR/ue-position.cfg" --ue-config /path/to/nrue.conf \
    -- ./nr-uesoftmodem ... \
    --device.name oai_zmqdevif --zmq.[0].options chanmod --zmq.[0].modelname SAT_LEO_TLE \
    --zmq.[0].ntn_file /tmp/ntn-profile.csv --zmq.[0].tx_channels ... --zmq.[0].rx_channels ...
  ```
  The launcher selects the profile row at simulation time zero. It supplies `--initial-fo` and
  `--ntn-initial-time-drift` before starting the UE. With `--ue-position-file`, it also
  converts the OCUDU latitude/longitude/altitude to WGS-84 ECEF metres, atomically updates the
  `position0` block in `--ue-config`, and passes that configuration to the UE with `-O`.
