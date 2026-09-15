<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# Static beam-angle demo: 4-antenna analog beamforming with a 0°-boresight beam (falcon-gh200)

This is a complete, standalone runbook for the "static UE position" beamforming
demo: a fixed O-RU codebook beam, with the UE held at a sequence of static
angular offsets via a ray-traced channel model injected through vrtsim's CIR
database (`cirdb`, `model_id=5`). It covers every step end to end — building
the dependencies, building OAI, generating the channel files, running the
O-RU/DU/UE split, and tearing down — plus the full results from two angle
sweeps validating the beamforming: §9 with the beam at 0° boresight (UE swept
`-20°` to `+20°`), and §10-§11 with the beam steered to `+15°` (UE swept `+40°`
down to `-10°`), which confirms the SINR/RSRP peak actually moves to track the
steered beam rather than staying fixed at boresight.

It assumes the machine (`falcon-gh200.sboai.cs.eurecom.fr`, an NVIDIA
GH200-equipped ARM box) and is written against the state that produced the
results in §8 below. It is a condensed, self-contained sibling of
[README-falcon-gh200-7.2split_catb_beamforming_support.md](README-falcon-gh200-7.2split_catb_beamforming_support.md),
which has the deeper investigation history (channel-model timing bugs found
and fixed along the way) if you want that context.
```

## 1. Build DPDK

O-RAN release K needs DPDK 22.11.7 or 24.11.4 — don't use the pre-installed
`mlnx-dpdk` OS package (`/opt/mellanox/dpdk/...`, DPDK 22.11.0, wrong O-RAN
release):

```bash
mkdir ~/test_dir_72split && cd ~/test_dir_72split

wget https://fast.dpdk.org/rel/dpdk-24.11.4.tar.xz && tar xf dpdk-24.11.4.tar.xz
cd dpdk-stable-24.11.4
meson setup --prefix=$(pwd)/../dpdk-inst build
ninja -C build && ninja -C build install
export DPDK_INST=~/test_dir_72split/dpdk-inst
```

## 2. Build o-du-phy (xRAN fronthaul library)

```bash
cd ~/test_dir_72split
git clone https://github.com/openairinterface/o-du-phy.git phy-k
cd phy-k && git checkout 11.1.7   # K release; OAI's radio/fhi_72/CMakeLists.txt checks for exactly this version
```
**NOTE - 11.1.7 fix outlined below in the 11.1.8 release**
**mlx5-specific fix required before building** — xran's RX mbuf pool is 18
bytes too small for Mellanox NICs (Intel NICs, what xran was tested against,
treat `.mtu` as the full frame size; `mlx5` treats it as L3-payload-only and
adds 18 bytes of L2/CRC on top), so `rte_eth_rx_queue_setup()` fails and the
O-RU/DU calls `rte_panic()` on startup without this. Edit
`fhi_lib/lib/ethernet/xran_ethernet.h`:

```diff
-#define MBUF_POOL_ELEMENT (MAX_RX_LEN + RTE_PKTMBUF_HEADROOM)
+#define MBUF_POOL_ELEMENT (MAX_RX_LEN + RTE_PKTMBUF_HEADROOM + 64) /* +64: mlx5 adds L2/CRC overhead on top of .mtu that xran does not account for */
```

This fix is **not** part of the 11.1.7 checkout — it's a local, uncommitted
edit that must be reapplied any time `phy-k` is freshly cloned/checked out.

Build `libxran.so` (the DU-side `oran_fhlib_5g` target loads this at runtime
via `LD_LIBRARY_PATH`, not statically — no OAI rebuild needed if you change
this file later, just rebuild `libxran.so` and restart the DU):

```bash
cd ~/test_dir_72split/phy-k/fhi_lib/lib
export RTE_SDK=~/test_dir_72split/dpdk-stable-24.11.4
export RTE_TARGET=build
make XRAN_DIR=~/test_dir_72split/phy-k/fhi_lib XRAN_LIB_SO=1 MLOG=0 -j$(nproc)
```

> [!NOTE]
> `fhi_lib/build.sh LIBXRANSO` looks like the "normal" way to build this, but
> its `LIBXRANSO=1` flag never reaches the Makefile's `XRAN_LIB_SO` variable
> (a pre-existing naming mismatch in `phy-k`, unrelated to anything here) —
> it silently builds `libxran.a` instead. Invoke `make` directly with
> `XRAN_LIB_SO=1` as above, or you'll get a stale/wrong-version `.so`.

Confirm `build/libxran.so` was just rebuilt (recent mtime) before moving on.

## 3. Build openairinterface5g

```bash
cd ~
git clone https://github.com/duranta-project/openairinterface5g.git
cd openairinterface5g
git checkout -b personal/<you>/2x2_catb  # or reuse an existing branch
git rebase origin/develop                # keep current with upstream fixes (esp. GPU channel emulation — see §3b)
```

### 3a. Required uncommitted source patches
If not already merged to develop, need to pull in changes from:
https://github.com/duranta-project/openairinterface5g/pull/535


### 3b. Build

```bash
cd ~/openairinterface5g/cmake_targets
rm -rf ran_build/build
export PKG_CONFIG_PATH=~/test_dir_72split/dpdk-inst/lib/aarch64-linux-gnu/pkgconfig
export LIBRARY_PATH=~/test_dir_72split/dpdk-inst/lib/aarch64-linux-gnu:$LIBRARY_PATH
./build_oai -I
./build_oai --gNB --nrRU --nrUE --ninja -t oran_fhlib_5g \
    --cmake-opt -Dxran_LOCATION=$HOME/test_dir_72split/phy-k/fhi_lib/lib \
    --cmake-opt "-DENABLE_CHANNEL_SIM_CUDA=ON" \
    --cmake-opt "-DCMAKE_POSITION_INDEPENDENT_CODE=ON" \
    --cmake-opt "-DCMAKE_CUDA_COMPILER=/usr/local/cuda/bin/nvcc"
```

- `-DENABLE_CHANNEL_SIM_CUDA=ON`: this demo's channel model runs on
  falcon-gh200's integrated GPU (FFT overlap-add convolution), not the CPU
  thread-pool path. `nvcc` isn't on `PATH` on this machine, so pass
  `CMAKE_CUDA_COMPILER` explicitly.
- `-DCMAKE_POSITION_INDEPENDENT_CODE=ON` is required alongside it on
  aarch64: the CUDA object code ends up linked into `libvrtsim.so` (a shared
  module) even though its own library target is `STATIC`, and without PIC
  nvcc's device-code relocations fail at link time with
  `relocation R_AARCH64_ADR_PREL_PG_HI21 ... can not be used when making a
  shared object`.

Confirm the CUDA path actually built in (not silently falling back to CPU):
```bash
nm ~/openairinterface5g/cmake_targets/ran_build/build/libvrtsim.so | grep -i cuda_channel_pipeline
# expect: cuda_channel_pipeline, cuda_channel_pipeline_init, cuda_channel_pipeline_shutdown
```

The 4x4 conf files used below
(`gnb.band77.mu1.106rb.4x4_beamforming.conf`,
`ru.band77.mu1.106rb.4x4_beamforming.conf`, in
`targets/PROJECTS/GENERIC-NR-5GC/CONF/`) are untracked, machine-specific
files derived from the existing `*_2x2_catb.conf` pair (same VF PCI
addresses/MACs/cores/loopback IPs — nothing to re-edit for a different
machine setup). Key deltas from the 2x2 baseline:

**DU (`gnb...4x4_beamforming.conf`)**: `pdsch_AntennaPorts_XP=1`,
`maxMIMO_layers=1` (single DL logical stream), `pusch_AntennaPorts=4`
(UL-only, must match `RUs.nb_rx` — this is a real memory-safety fix, not
just tuning: it sizes `Prx`/`common_vars.rxdataF`, and leaving it at `1`
while `RUs.nb_rx=4` is a heap overflow), `RUs.nb_tx=RUs.nb_rx=4`.

**O-RU (`ru...4x4_beamforming.conf`)**: `nb_tx=nb_rx=nb_fh_streams=4`,
`prach_eaxc_offset=4` (must be `>= max(nb_tx, nb_rx)` or PRACH silently
misroutes and never recovers), `codebook_nb_beams=1` (a second beam crashed
the DU — not yet root-caused, don't add one back), and a **diagonal**
`codebook_weights` matrix (`beam × RUs.nb_tx × nb_fh_streams × (Re,Im)` =
`1×4×4×2` = 32 Q15 values) with all four physical elements in phase — this
is the fixed 0°-boresight beam this whole demo points at:

```
codebook_weights = [32767,0,     0,0,     0,0,     0,0,
                         0,0, 32767,0,     0,0,     0,0,
                         0,0,     0,0, 32767,0,     0,0,
                         0,0,     0,0,     0,0, 32767,0];
```

## 4. Ray-tracing channel model: generate the static angle-offset files

Clone the ray-tracing emulator (once):
```bash
git clone https://gitlab.eurecom.fr/oai/raytracing-channel-emulator.git
cd raytracing-channel-emulator
git checkout bf-experiments
cd server/beamforming
pip install sionna
```

For each static UE angle `$DEG` in the sweep, generate a paired
downlink/uplink CIR entry (a single "hold" event, `rxstart == rxstop`, so
every snapshot is identical at that angle) — `-gnb_tx_ant_elements 4
-ue_rx_ant_elements 2` matches this demo's actual topology
(`RUs.nb_tx=4`, UE = 2 antennas):

```bash
DEG=<angle, e.g. -20, -15, -10, -5, 0, 5, 10, 15, 20>
NAME=$([ "${DEG:0:1}" = "-" ] && echo "neg${DEG#-}deg" || echo "${DEG}deg")

python3 generate_moving_rx_cir.py \
  -tx_position=-10,80 \
  -tx_height 27 \
  -tx_boresight 0 \
  -carrier_frequency 3.5e9 \
  -antenna_spacing 0.5 \
  -gnb_tx_ant_elements 4 \
  -ue_rx_ant_elements 2 \
  -bandwidth 40e6 \
  -sampling_frequency 61.44e6 \
  -taper none \
  -rx_starting_deg=$DEG \
  -rx_starting_distance 70 \
  -rx_starting_height 1.5 \
  -event "[$DEG,70,$DEG,70,0.5,5]" \
  --out-bin ./static_${NAME}_cir.bin \
  --out-bin-ul ./static_${NAME}_cir_ul.bin \
  --out-beamforming ./static_${NAME}_beamforming.yaml
```

This produces two CIR entries per angle: `static_${NAME}_cir.yaml`/`.bin`
(downlink, `n_tx=4, n_rx=2` — for the O-RU) and
`static_${NAME}_cir_ul.yaml`/`.bin` (uplink, `n_tx=2, n_rx=4` — for the UE;
the reciprocal transpose of the downlink entry, same ray trace, no
re-tracing). `cirdb_connect()`'s antenna-shape check is strict with no
reshape, so the O-RU and UE each need their own file — see §7.

Copy the generated files to falcon (`server/beamforming/` on falcon,
matching the local layout) if generated locally:
```bash
scp static_${NAME}_cir*.bin static_${NAME}_cir*.yaml \
    falcon:~/raytracing-channel-emulator/server/beamforming/
```

For this demo's sweep, generate all nine: `-20 -15 -10 -5 0 5 10 15 20`.

## 5. Provision the SR-IOV VFs

```bash
cd ~/openairinterface5g/cmake_targets
#Highly reccomend pusshing the following to an executable shell script:
#!/usr/bin/env bash

#hardware; confirm nothing else is using these VFs and capture a baseline
#you can compare against (and restore, if something looks off) before you
#change anything:
#```bash
cat /sys/class/net/enP2p1s0f1np1/device/sriov_numvfs   # how many VFs exist now
ip link show enP2p1s0f1np1                              # MACs/flags of any existing VFs
ethtool -g enP2p1s0f1np1                                # current RX/TX ring sizes
ethtool -S enP2p1s0f1v0 2>&1 | grep -iE 'steer|vport_unicast'   # baseline HW counters, if v0/v1 already exist
ethtool -S enP2p1s0f1v1 2>&1 | grep -iE 'steer|vport_unicast'
ps aux | grep -i "nr-\|dpdk" | grep -v grep             # anyone already running something on this port?
#```
#If VFs already exist and something is actively using them, stop here and
#check with whoever owns that process — don't reprovision out from under
#someone else's run.


# full reprovision — clears any stuck per-VF hardware state, not just first-time setup
echo 0 | sudo tee /sys/class/net/enP2p1s0f1np1/device/sriov_numvfs
sleep 2
echo 2 | sudo tee /sys/class/net/enP2p1s0f1np1/device/sriov_numvfs
sleep 2

# MACs are retained across the cycle, but every other VF attribute resets to
# its default and must be reapplied — note the ip-link keyword is `state`,
# not `link-state` (the latter is only how `ip link show` *displays* it):
sudo ip link set enP2p1s0f1np1 vf 0 mac 02:00:00:00:00:01  # DU VF
sudo ip link set enP2p1s0f1np1 vf 1 mac 02:00:00:00:00:02  # RU VF
sudo ip link set enP2p1s0f1np1 vf 0 state enable
sudo ip link set enP2p1s0f1np1 vf 1 state enable
sudo ip link set enP2p1s0f1np1 vf 0 trust on
sudo ip link set enP2p1s0f1np1 vf 1 trust on
sudo ip link set enP2p1s0f1np1 vf 0 vlan 30
sudo ip link set enP2p1s0f1np1 vf 1 vlan 30

# ring sizes also reset to the driver default (1024/1024) — max them out again:
sudo ethtool -G enP2p1s0f1np1 rx 8160 tx 8160

# bring the VF netdevs themselves up with jumbo MTU (they come back DOWN/mtu 1500):
sudo ip link set enP2p1s0f1v0 mtu 9700 up
sudo ip link set enP2p1s0f1v1 mtu 9700 up

# confirm before moving on:
ip link show enP2p1s0f1np1     # expect: vlan 30, spoof checking off, link-state enable, trust on on both VFs
ethtool -S enP2p1s0f1v0 | grep -i steer   # expect: rx_steer_missed_packets stays at/near 0 as traffic starts

echo "SR-IOV setup complete on Falcon (single-host)"

```

This is a full VF reprovision (not just first-time setup) — it clears any
stuck per-VF hardware state from a prior session, so run it at the start of
every fresh session even if the VF netdevs already look correctly
configured. Stale VF state is the single most common cause of "RAR
reception fails forever." It assigns fixed MACs to the two VFs (DU:
`02:00:00:00:00:01`, RU: `02:00:00:00:00:02`), enables trust/VLAN 30 on
both, maxes out ring sizes, and brings the VF netdevs up with jumbo MTU. It
prints its own before/after diagnostics — confirm the final `ip link
show`/`ethtool -S` output shows `vlan 30, spoof checking off, link-state
enable, trust on` on both VFs and `rx_steer_missed_packets` near 0 before
moving on.

## 6. tmux session + core pinning

|Process        |Allocated CPUs|
|----------------|------------------------------|
|DU: XRAN (`system_core`/`io_core`/`worker_cores`) |4,5,6 (exclusive)|
|DU: `L1_rx_thread_core`/`L1_tx_thread_core`        |7,8 (must be pinned, not `-1`)|
|DU: `--thread-pool`                                |9-29 (must exclude 4-8)|
|O-RU (`nr-oru`) |30-45|
|UE              |46-64|

```bash
tmux new-session -d -s beamform_static -c ~/openairinterface5g/cmake_targets/ran_build/build
tmux send-keys -t beamform_static 'export LD_LIBRARY_PATH=~/test_dir_72split/phy-k/fhi_lib/lib/build:$LD_LIBRARY_PATH' C-m
tmux split-window -h -t beamform_static -c ~/openairinterface5g/cmake_targets/ran_build/build
tmux split-window -v -t beamform_static -c ~/openairinterface5g/cmake_targets/ran_build/build
tmux select-layout -t beamform_static tiled
tmux attach -t beamform_static
```

## 7. Run the O-RU / DU / UE split

For each angle `$DEG`/`$NAME` from §4, run all three processes — O-RU
first, then DU once the O-RU shows up, then UE once the DU shows up. The
O-RU loads the **downlink** CIR entry (`static_${NAME}_cir.*`); the UE loads
the **uplink** entry (`static_${NAME}_cir_ul.*`) — these are *not*
interchangeable (`cirdb_connect()`'s antenna-shape check is strict). Both
use `--vrtsim.cirdb_model_id 5` (RT-Beam-forming).

```bash
# pane 0: O-RU
sudo -E taskset -c 30-45 ./nr-oru \
  -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/ru.band77.mu1.106rb.4x4_beamforming.conf \
  --device.name vrtsim --vrtsim.role server --vrtsim.ue_config.[0].antennas 2x2 \
  --RUs.[0].sl_ahead 8 \
  --vrtsim.cirdb 1 \
  --vrtsim.cirdb_yaml ~/raytracing-channel-emulator/server/beamforming/static_${NAME}_cir.yaml \
  --vrtsim.cirdb_file ~/raytracing-channel-emulator/server/beamforming/static_${NAME}_cir.bin \
  --vrtsim.cirdb_model_id 5

# pane 1: DU — start once pane 0 shows the O-RU is up
sudo -E taskset -c 4-29 ./nr-softmodem \
  -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.band77.mu1.106rb.4x4_beamforming.conf \
  --thread-pool 9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29 \
  --gNBs.[0].min_rxtxtime 6

# pane 2: UE — start once pane 1 shows the DU is up
sudo -E taskset -c 46-64 ./nr-uesoftmodem \
  -C 4049760000 -r 106 --numerology 1 --ssb 516 \
  --device.name vrtsim --vrtsim.role client --ue-nb-ant-tx 2 --ue-nb-ant-rx 2 \
  --vrtsim.cirdb 1 \
  --vrtsim.cirdb_yaml ~/raytracing-channel-emulator/server/beamforming/static_${NAME}_cir_ul.yaml \
  --vrtsim.cirdb_file ~/raytracing-channel-emulator/server/beamforming/static_${NAME}_cir_ul.bin \
  --vrtsim.cirdb_model_id 5
```

Notes on the flags:
- All four `--vrtsim.cirdb*` params need the `vrtsim.` config-section
  prefix, same as `--vrtsim.role`/`--vrtsim.ue_config.[0].antennas` right
  next to them — a bare `--cirdb`/`--cirdb_yaml`/etc. is rejected as
  "unknown option" and kills the process.
- `--vrtsim.ue_config.[0].antennas` describes the *UE's* own antenna count
  to the vrtsim server, not the RU's — it stays `2x2` regardless of the
  conf files' 4-antenna RU configuration.
- The O-RU log line `CIRDB: UE angle at snapshot N/S (...): X.XX deg from
  tx boresight` should read the angle you just generated for, flat across
  all snapshots (a static "hold" event) — check this before trusting any
  further results from that run.

Let it run at least ~30 seconds per angle before collecting metrics, to get
past initial sync/RA and see a few periodic MAC stats prints.

**Metrics to collect:**
```bash
grep -c "RRCRelease" /tmp/ue.log
grep -c "SR not served" /tmp/ue.log
grep -c "RA procedure succeeded" /tmp/ue.log
grep -c "synch Failed" /tmp/ue.log     # >0 at this stage means the UE never synchronized at this angle
grep "DL Chan" /tmp/ue.log | tail -6   # UE-reported SINR/RSRP
grep -oE "SNR [0-9.-]+ \([0-9.-]+\) RSSI [0-9.]+" /tmp/du.log | tail -3   # DU-reported UL SNR/RSSI -- see caveat in §8
```

## 8. Teardown (per angle, and at the end of the session)

Between angles, and always at the end of the session:

```bash
tmux kill-session -t beamform_static   # only when fully done with the session
sudo pkill -9 -f "\./nr-oru"
sudo pkill -9 -f "\./nr-softmodem"
sudo pkill -9 -f "\./nr-uesoftmodem"
sudo rm -rf /var/run/dpdk/du /var/run/dpdk/ru
sudo find /dev/hugepages -maxdepth 1 -type f -exec rm -f {} \;
sudo ethtool -G enP2p1s0f1np1 rx 1024 tx 1024
echo 0 | sudo tee /sys/class/net/enp1s0f1np1/device/sriov_numvfs
echo 0 | sudo tee /sys/class/net/enP2p1s0f1np1/device/sriov_numvfs
```

Verify clean before disconnecting or handing the machine to someone else:
```bash
[ "$(cat /sys/class/net/enP2p1s0f1np1/device/sriov_numvfs)" = "0" ] && \
  [ -z "$(ps aux | grep "nr-" | grep -v grep)" ] && \
  echo "CLEAN — safe to disconnect" || echo "NOT CLEAN — cleanup incomplete"
```

If continuing to the next angle in the same session, skip the `tmux
kill-session` and VF-disable steps — just kill the three processes, clean
`/var/run/dpdk`/hugepages, then re-run `./sriov-falcon.sh` (§5) before
starting the next angle's O-RU/DU/UE.

## 9. Results: 0°-boresight beam, UE swept from -20° to +20°

Fixed beam (§3b's `codebook_weights`, boresight, no tilt), UE held static
at each angle in turn, 5° steps, 4 physical TX/RX antennas, ray-traced CIR
channel model (`cirdb model_id=5`) on both DL and UL.

| UE Angle | Sync? | Avg DL SINR (dB) | DL RSRP (dBm) | RA success / RRCRelease | SR not served | UL errors/DTX (DU) |
|---|---|---|---|---|---|---|
| -20° | ✅ Yes | ~42.8 | -36 | 5 / 0 | 2 | 0 err / 15 DTX |
| -15° | ✅ Yes | ~44.8 | -33 | 5 / 0 | 2 | 0 err / 13 DTX |
| -10° | ✅ Yes | ~45.8 | -31 | 5 / 0 | 2 | 0 err / 10 DTX |
| -5°  | ✅ Yes | ~46.1 | -30 | 5 / 0 | 2 | 0 err / 10 DTX |
| 0° (boresight) | ✅ Yes | ~46.7 | -29 | 5 / 0 | 2 | 0 err / 8 DTX |
| +5°  | ✅ Yes | ~45.9 | -30 | 5 / 0 | 2 | 0 err / 8 DTX |
| +10° | ✅ Yes | ~45.9 | -31 | 5 / 0 | 2 | 0 err / 8 DTX |
| +15° | ✅ Yes | ~44.8 | -33 | 5 / 0 | 2 | 0 err / 10 DTX |
| +20° | ✅ Yes | ~42.9 | -36 | 5 / 0 | 2 | 0 err / 13 DTX |

Every angle in the sweep synchronized successfully — none of the offsets
tested (up to ±20°) were far enough off this beam's main lobe to break sync.

**What the sweep shows:**

- **RSRP roll-off is the clearest signal**: -29 dBm at boresight,
  degrading smoothly and symmetrically to -36 dBm at both ±20° (a 7 dB
  drop at the sweep's edges), with a monotonic falloff in between on both
  sides.
- **DL SINR follows the same shape**: peaks at ~46.7 dB on-boresight, drops
  to ~42.8-42.9 dB at ±20°.
- **The pattern is essentially symmetric** about 0° (e.g. -10°/-31 dBm vs.
  +10°/-31 dBm; -15°/-33 dBm vs. +15°/-33 dBm) — exactly what a
  boresight-pointed beam with no directional bias should produce.
- **Connection health is identical across every angle** (RA success count,
  zero RRCRelease, SR-not-served count) — confirms the SINR/RSRP variation
  is a genuine beam-pattern effect, not a stability artifact of any
  particular angle.

**Caveat on the UL numbers**: the DU-reported UL `SNR`/`RSSI` column is
**not included as a beamforming metric** — it reads the same
`SNR ≈ -39/-40 dB, RSSI ≈ 6373` at every single angle, which is a
pre-existing `uint16_t` wraparound artifact in the DU's RSSI computation
(`phy_procedures_nr_gNB.c`), independent of the real channel/beam angle
(root-caused in the companion README, not yet fixed). The DL SINR/RSRP
figures above are the reliable, angle-sensitive metrics from this demo; the
UL-side DU reading is a known measurement bug, not a real per-angle effect.

## 10. Second validation pass: steer the beam to +15° instead of boresight

§9 confirmed the beam pattern is symmetric and centered on 0° when the
codebook points at boresight — but that alone doesn't prove the codebook
weights actually *steer* anything, since a symmetric roll-off around 0° is
also what a broken/no-op codebook would produce if 0° just happens to be
where the array is physically pointed. This second pass changes the
codebook to steer to **+15°** and re-sweeps the UE, to confirm the SINR/RSRP
peak actually *moves* to track the new beam direction rather than staying
fixed at 0°.

### 10a. Derive the steered codebook weights

The existing boresight codebook (§3b) applies the same in-phase weight
(`32767,0`) to every antenna — the degenerate case of a ULA steering vector
at `θ=0`. For a non-zero steering angle, the per-element phase step follows
the standard far-field ULA formula for element spacing `d` (in wavelengths):

```
Δφ(θ) = 360° · (d/λ) · sin(θ)
```

With `d/λ = 0.5` (this array's `-antenna_spacing 0.5` from §4) and
`θ = 15°`: `Δφ = 180° · sin(15°) = 46.587°` per element. The codebook
applies the **negative** of this per physical antenna index `n = 0,1,2,3`
(not centered — `n=0` is antenna 0, `n=3` is antenna 3, matching `RUs.nb_tx`
order) so the array combines coherently at a receiver positioned at `+15°`.
The sign here matters — get it backwards and the beam points at `-15°`
instead:

```python
import numpy as np
theta_beam_deg = 15.0
theta = np.deg2rad(theta_beam_deg)
dphi_per_element = 2*np.pi*0.5*np.sin(theta)  # radians per element step

for n in range(4):
    phase = -dphi_per_element * n
    re = round(32767*np.cos(phase))
    im = round(32767*np.sin(phase))
    print(f"n={n} phase={np.degrees(phase):8.3f} deg  Re={re:6d} Im={im:6d}")
```

```
n=0 phase=   0.000 deg  Re= 32767 Im=     0
n=1 phase= -46.587 deg  Re= 22519 Im=-23803
n=2 phase= -93.175 deg  Re= -1815 Im=-32717
n=3 phase=-139.762 deg  Re=-25013 Im=-21166
```

This sign convention (`-Δφ(θ)·n`, matching `beamforming.py`'s
`calculate_beam_coefficients()` in the ray-tracing emulator) was confirmed
correct empirically, not just derived on paper: §11's sweep shows the
SINR/RSRP peak land at `+15°`/`+20°`, not `-15°`/`-20°` — if the sign were
backwards, the peak would have shown up on the wrong side of 0°.

### 10b. Update the O-RU conf

Before editing, back up the boresight version (to restore it, or to diff
against later):
```bash
cp ~/openairinterface5g/targets/PROJECTS/GENERIC-NR-5GC/CONF/ru.band77.mu1.106rb.4x4_beamforming.conf \
   ~/ru.band77.mu1.106rb.4x4_beamforming.conf.boresight_backup
```

Replace the `codebook_weights` block from §3b with the steered values —
same diagonal layout (`beam × RUs.nb_tx × nb_fh_streams × (Re,Im)`), only
the four non-zero diagonal entries change:

```diff
-  codebook_weights = [32767,0,     0,0,     0,0,     0,0,
-                           0,0, 32767,0,     0,0,     0,0,
-                           0,0,     0,0, 32767,0,     0,0,
-                           0,0,     0,0,     0,0, 32767,0];
+  codebook_weights = [32767,0,         0,0,         0,0,         0,0,
+                          0,0,     22519,-23803,     0,0,         0,0,
+                          0,0,         0,0,     -1815,-32717,     0,0,
+                          0,0,         0,0,         0,0,     -25013,-21166];
```

No source rebuild is needed — this is a runtime conf value, read fresh by
`nr-oru` on every startup.

### 10c. Generate the additional UE-position CIR files

The UE-position channel files depend only on transmitter/receiver geometry
(§4), not on the O-RU's codebook — the six angles already generated for §9
(`0, 5, 10, 15, -5, -10`) are reused as-is. This sweep additionally needs
`40, 35, 30, 25, 20` — generate those with the same §4 command, sweeping
`$DEG` over those five values, and copy them to falcon the same way.

### 10d. Run the sweep

Identical procedure to §7/§8, just with the steered conf from §10b and the
angle list `40 35 30 25 20 15 10 5 0 -5 -10` (UE swept from `+40°` down to
`-10°`, 5° steps). Same per-angle O-RU/DU/UE launch commands, same metrics
collection, same teardown between angles.

> [!NOTE]
> **One angle in this sweep (`+20°`) failed to synchronize on the first
> automated pass** (0 RA successes, repeated `synch Failed`), sandwiched
> between two neighbors (`+15°`, `+25°`) that both synced cleanly with
> excellent SINR. That pattern — an isolated failure between two strong,
> successful neighbors, with no physical reason for a null that close to
> the beam's expected peak — is the signature of a transient
> infrastructure hiccup (e.g. VF/hugepage state not fully settled between
> back-to-back angles in the automated loop), not a real RF effect.
> Confirmed by isolating and rerunning `+20°` standalone: it synced cleanly
> and produced the *best* SINR/RSRP of the entire sweep (~48.8 dB, -30 dBm),
> consistent with sitting right at the beam's peak. If a single angle
> fails in isolation surrounded by strong neighbors, always rerun it alone
> before concluding it's a real null — see §11 for the corrected value.

## 11. Results: beam steered to +15°, UE swept from +40° to -10°

Steered beam (§10b's `codebook_weights`, +15° off boresight), UE held
static at each angle in turn, 5° steps, same 4-antenna/ray-traced-CIR setup
as §9.

| UE Angle | Sync? | Avg DL SINR (dB) | DL RSRP (dBm) | RA success / RRCRelease | SR not served | UL DTX (DU) |
|---|---|---|---|---|---|---|
| +40° | ✅ Yes | ~42.1 | -37 | 5 / 0 | 2 | 8 |
| +35° | ✅ Yes | ~44.6 | -34 | 5 / 0 | 2 | 8 |
| +30° | ✅ Yes | ~47.2 | -32 | 5 / 0 | 2 | 13 |
| +25° | ✅ Yes | ~48.0 | -31 | 5 / 0 | 2 | 10 |
| **+20°** | ✅ Yes* | ~48.8 | -30 | 5 / 0 | 2 | 13 |
| **+15° (beam peak)** | ✅ Yes | ~49.0 | -30 | 5 / 0 | 2 | 10 |
| +10° | ✅ Yes | ~48.1 | -30 | 5 / 0 | 2 | 9 |
| +5°  | ✅ Yes | ~46.8 | -31 | 5 / 0 | 2 | 8 |
| 0°   | ✅ Yes | ~44.1 | -33 | 5 / 0 | 2 | 10 |
| -5°  | ✅ Yes | ~41.4 | -37 | 5 / 0 | 2 | 8 |
| -10° | ✅ Yes | ~34.5 | -44 | 5 / 0 | 2 | 10 |

*`+20°` failed in the first automated pass — see the §10d note. The values
shown here are from the standalone rerun, which synced cleanly.

**What this sweep confirms:**

- **The beam is genuinely steered to +15°, not boresight.** The SINR/RSRP
  peak sits at `+15°`/`+20°` (~49 dB, -30 dBm) — a full 15-20° away from
  where §9's boresight sweep peaked. This is the key validation this pass
  adds over §9: the codebook weights don't just produce *some* symmetric
  pattern, they demonstrably move the array's main lobe to the angle they
  were computed for, and the sign convention derived in §10a is confirmed
  correct (the peak landed on `+15°`, not `-15°`).
- **The pattern is now asymmetric around 0°, exactly as expected for an
  off-boresight beam**: falling off faster toward `-10°` (-44 dBm) than
  toward `+40°` (-37 dBm) at a comparable angular distance from the peak —
  a beam pointed off-center produces asymmetric coverage, unlike §9's
  symmetric boresight pattern.
- **Connection health is stable at every angle** (RA succeeds, 0
  RRCRelease, SR-not-served flat at 2 everywhere) — same as §9, confirming
  the SINR/RSRP variation is the beam pattern, not a stability issue.
- Same UL SNR/RSSI caveat as §9 applies here — not shown in this table for
  that reason.

The steered conf is left in place on falcon after this sweep, with the
original boresight version backed up at
`~/ru.band77.mu1.106rb.4x4_beamforming.conf.boresight_backup` (§10b) for
anyone who wants to restore §9's exact configuration.

Simualtion values show that the vrtsim 7.2 split is within expected range 
Angle	dB down
-10°	-15.71
-5°	-8.00
0°	-4.00
5°	-1.65
10°	-0.39
15°	0.00 (peak)
20°	-0.37
25°	-1.50
30°	-3.42
35°	-6.31
40°	-10.64