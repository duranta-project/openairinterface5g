<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# Trace-based LEO NTN with RFsimulator: step by step

This tutorial runs gNB and nrUE over RFsimulator with a LEO satellite that
follows a real orbit, given as a trace file, instead of the built-in circular orbit.
Run the commands in order. For the background and all parameters, see
[ntn-configuration.md](./ntn-configuration.md#trace-driven-leo-satellite).

[[_TOC_]]

## 0. Before you start

You need:

- the transparent LEO example of [ntn-configuration.md](./ntn-configuration.md) working,
  i.e. OAI built (`nr-softmodem`, `nr-uesoftmodem`, rfsimulator) and a 5G core network
  that this gNB and UE can attach to,
- python3.

All commands below are for bash. Set the path of your OAI repository once:

```bash
OAI=~/openairinterface5g   # adapt to your clone
```

## 1. Choose a trace

A trace is a CSV file with the satellite position and velocity (ECEF) over time.
A 300 s sample trace of a Starlink pass is provided:

```bash
TRACE=$OAI/tools/ntn/data/orbital_trace_s_300.csv
head -3 $TRACE
```

```
time_s,pos_x,pos_y,pos_z,vel_x,vel_y,vel_z
0,-4165355.542,-5103358.603,1412186.247,4093.643,-1684.703,5943.4
1,-4161259.338,-5105040.275,1418128.68,4098.808,-1678.667,5941.548
```

To use your own satellite pass instead, see [tools/ntn](../tools/ntn/README.md).

## 2. Compute the matching parameters

The gNB and UE must start with values that match the trace start.
Compute them from the trace, the UE and gNB positions (ECEF, in m) and the DL frequency.
Here, UE and gNB are both placed below the middle of the pass:

```bash
POS=-3310204.0,-5006785.1,2157610.7
python3 $OAI/tools/ntn/calc_ntn_parameters.py $TRACE --ue=$POS --gnb=$POS --fc 2488400000
```

The values in the `SAT_LEO_TRANS` part of the output are used in step 3 and step 4:

```
=== SIB19 ephemeris at trace start (both modes) ===
  positionX/Y/Z-r17     = -3204120, -3925660, 1086297
  velocityVX/VY/VZ-r17  = 68227, -28078, 99057
...
=== SAT_LEO_TRANS ===
  ta-Common-r17            = 1865301
  ta-CommonDrift-r17       = -227522
  cellSpecificKoffset_r17  = 16   # max RTT 15.19 ms
  rfsimulator.prop_delay   = 8   # one-way delay 7.60 ms
  --initial-fo             = 56634   # DL Doppler 56.63 kHz
  --ntn-initial-time-drift = -46   # us/s
```

> Use `cellSpecificKoffset_r17 = 40` rather than the minimum printed here:
> with smaller values, the UE currently does not receive SIB2/SIB19 and does not attach.

## 3. Prepare the configuration files

Start from the configuration files of the built-in LEO example, in a new directory:

```bash
mkdir -p ~/ntn-trace && cd ~/ntn-trace
cp $OAI/ci-scripts/conf_files/gnb.sa.band254.u0.25prb.rfsim.ntn-leo.conf gnb.conf
cp $OAI/ci-scripts/conf_files/nrue.uicc.ntn-leo.conf ue.conf
cp $OAI/targets/PROJECTS/GENERIC-NR-5GC/CONF/channelmod_rfsimu_LEO_trace.conf .
```

If you changed the core network settings (PLMN, AMF IP address, SIM data, ...) for the
built-in LEO example, apply the same changes to `gnb.conf` and `ue.conf`.

**Channel model:** replace the built-in LEO channel model with the trace-based one
(`channelmod_rfsimu_LEO_trace.conf`), and point it to the trace:

```bash
for f in gnb.conf ue.conf; do
  sed -i '/^channelmod = {/,/^};/d' $f
  echo '@include "channelmod_rfsimu_LEO_trace.conf"' >> $f
done
sed -i "s#\"../tools/ntn/data/orbital_trace_s_300.csv\"#\"$TRACE\"#" channelmod_rfsimu_LEO_trace.conf
```

`channelmod_rfsimu_LEO_trace.conf` places UE and gNB at `POS` of step 2.
If you use other positions, change `pos_ue_*` and `pos_gnb_*` there as well.

**Parameters from step 2:**

```bash
set_val() { sed -i -E "s/^(\s*$1\s*=\s*)[^;]*;.*/\1$2;/" $3; }

# gNB: SIB19 at trace start
set_val cellSpecificKoffset_r17 40 gnb.conf
set_val ta-Common-r17 1865301 gnb.conf
set_val ta-CommonDrift-r17 -227522 gnb.conf
set_val positionX-r17 -3204120 gnb.conf
set_val positionY-r17 -3925660 gnb.conf
set_val positionZ-r17 1086297 gnb.conf
set_val velocityVX-r17 68227 gnb.conf
set_val velocityVY-r17 -28078 gnb.conf
set_val velocityVZ-r17 99057 gnb.conf

# gNB and UE: RFsimulator propagation delay
set_val prop_delay 8 gnb.conf
set_val prop_delay 8 ue.conf

# UE: its position, same as pos_ue_* of the channel model
set_val x -3310204.0 ue.conf
set_val y -5006785.1 ue.conf
set_val z 2157610.7 ue.conf
```

Check the result:

```bash
grep -E "Koffset|ta-Common|position[XYZ]|velocityV|prop_delay|@include" gnb.conf
grep -E "^\s*[xyz] =|prop_delay|@include" ue.conf
```

## 4. Run

In a first terminal, start the gNB and wait until it is connected to the core network:

```bash
cd $OAI/cmake_targets/ran_build/build
sudo ./nr-softmodem -O ~/ntn-trace/gnb.conf --rfsim
```

In a second terminal, start the UE with `--initial-fo` and `--ntn-initial-time-drift` from step 2:

```bash
cd $OAI/cmake_targets/ran_build/build
sudo ./nr-uesoftmodem -O ~/ntn-trace/ue.conf --rfsim \
  --time-sync-I 0.1 --ntn-initial-time-drift -46 --initial-fo 56634 --cont-fo-comp 2
```

## 5. Check the result

Both gNB and UE load the trace at start:

```
[OCM]    I Loaded satellite trace .../orbital_trace_s_300.csv: 301 samples, 0.000-300.000 s
```

Every second, the channel model prints the satellite state and the resulting delay and Doppler,
e.g. on the UE (downlink):

```
[HW]     I Satellite orbit (trace): time 1.527598 s, Position = (-4159095.464254, -5105924.336753, 1421262.916823), Velocity = (4101.531461, -1675.481363, 5940.566668)
[HW]     I Downlink delay 7.526002 ms, Doppler service link 56.582101 kHz
```

The UE receives SIB19 with the satellite position from the trace, then attaches:

```
[NR_RRC] A Found SIB19
[NR_MAC] I NTN Config Rxd. Epoch HFN: 0, Epoch SFN: 289, Epoch Subframe: 0, k_offset: 40ms, N_Common_Ta: 7.531441ms, drift: -45.484400µs/s, variant: 0.015840µs/s²
[NR_RRC] I [UE 0] RNTI 0xde9c State = NR_RRC_CONNECTED
```

Then, as for any RFsimulator setup, ping the core network through the UE tunnel interface:

```bash
ping -I oaitun_ue1 <IP of the core network data network gateway>
```

The link stays up while the satellite passes: the delay decreases from 7.6 ms to about
2.4 ms in the middle of the pass and increases again, and the Doppler changes sign.
After the end of the trace (here 300 s), the satellite stays at its last position.

## Regenerative satellite (gNB on board)

For `SAT_LEO_REGEN`, use the `SAT_LEO_REGEN` part of the output of step 2, and in step 3 additionally run:

```bash
sed -i 's/SAT_LEO_TRANS/SAT_LEO_REGEN/' channelmod_rfsimu_LEO_trace.conf
set_val ta-Common-r17 0 gnb.conf
set_val ta-CommonDrift-r17 0 gnb.conf
set_val prop_delay 4 gnb.conf
set_val prop_delay 4 ue.conf
```

and start the UE with:

```bash
sudo ./nr-uesoftmodem -O ~/ntn-trace/ue.conf --rfsim \
  --time-sync-I 0.1 --ntn-initial-time-drift -23 --initial-fo 56634 --cont-fo-comp 3
```

## Troubleshooting

| Symptom | What to check |
| --- | --- |
| `Failed to load satellite trace` | the path in `channelmod_rfsimu_LEO_trace.conf` (use an absolute path) and the CSV header |
| UE decodes SIB1, but never `Found SIB19` | `cellSpecificKoffset_r17` must be 40 (step 2) |
| UE stuck in random access | step 2 values not applied, or UE and channel model positions differ |
| No `Satellite orbit (trace)` lines | `options = ("chanmod")` missing in the `rfsimulator` section |
