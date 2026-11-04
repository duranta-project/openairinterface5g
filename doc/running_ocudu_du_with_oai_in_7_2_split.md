<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# Running the OCUDU DU with OAI in the O-RAN 7.2 split

This guide shows how to run the OCUDU DU with the OAI O-RU, the OAI UE and the
OAI 5G core on one host. The DU and the O-RU use the O-RAN 7.2 fronthaul. It
covers x86_64 and aarch64 hosts. It has all the configs and commands that you
need.

OCUDU is an open-source 5G CU/DU project with a BSD-3-Clause-Open-MPI license.
Its source is at `https://gitlab.com/ocudu/ocudu.git`. This guide uses its
`gnb_split_7_2` application: CU and DU in one process, with an O-RAN 7.2
fronthaul.

The setup was tested on two hosts:

- an aarch64 host: NVIDIA Grace (Neoverse V2), Mellanox mlx5 NIC
- an x86_64 host: AMD EPYC 9374F, Intel E810 NIC

Related documents in this folder:

- [ORAN_FHI7.2_Tutorial.md](ORAN_FHI7.2_Tutorial.md): DPDK, PTP and host setup
- [ORAN_FHI7.2_ORU_Tutorial.md](ORAN_FHI7.2_ORU_Tutorial.md): `nr-oru`
  parameters and self-diagnosis report
- [NR_SA_Tutorial_OAI_CN5G.md](NR_SA_Tutorial_OAI_CN5G.md): OAI 5G core in
  Docker

## 1. Test chain

| Part | Binary | Interface |
|---|---|---|
| DU | OCUDU `gnb_split_7_2` (CU and DU in one process) | DPDK on VF0 |
| O-RU | OAI `nr-oru` | DPDK on VF1. vrtsim server on the radio side |
| UE | OAI `nr-uesoftmodem` | vrtsim client (shared memory). No DPDK |
| Core | OAI CN5G v2.2.1 | Docker containers. `oai-ext-dn` is the ping and iperf3 peer |

The DU and the O-RU use two SR-IOV VFs of one PF. The NIC eswitch carries the
fronthaul from VF to VF. No cable or switch is necessary.

```
 OCUDU gnb ─VF0─┐               ┌─VF1─ nr-oru ══ vrtsim ══ nr-uesoftmodem
                └── PF eswitch ─┘                                │
                                                     oaitun_ue1  │
 N2/N3 ── Docker bridge ── OAI CN5G (AMF, SMF, UPF, oai-ext-dn) ─┘
```

## 2. Reference results

Use these values to check a new host. All runs use OAI `develop` `88245ba846`
and OCUDU `release_26_10`. The x86_64 runs used the steps of this guide, from a
fresh build. The aarch64 runs used the same configs. CSI-RS is on in all runs. A
staircase run increases the iperf3 UDP rate by 10 Mbit/s every 30 s, and stops
at the first step with more than 5 % loss.

| Run | x86_64 (E810) | aarch64 (mlx5) |
|---|---|---|
| Ping | 100/100 both ways | 100/100 both ways |
| DL staircase | Clean to 130. 140: 129 Mbit/s, 6.7 % loss | Clean to 130. 140: 129 Mbit/s, 6.7 % loss |
| UL staircase | Clean to 40. Ceiling 41.6 Mbit/s. 60: 15 % loss | Clean to 40. Ceiling 42.2 Mbit/s. 60: 14 % loss |

- No run had a UE assert.
- A new host is good when it gives these limits within about 2 Mbit/s.

> **Use CSI-RS on** (`csi_rs_enabled: true`).

## 3. Values that do not change between hosts

The configuration templates (section 8) hold all cell, fronthaul and core
values. This table gives the reason for the values that are not obvious.

| Item | Value | Reason |
|---|---|---|
| SSB | ARFCN 669312 (4039.68 MHz). CORESET#0 index 11. UE `--ssb 180` | OCUDU places the SSB itself and has no key for offsetToPointA or k_SSB |
| PRACH | Config index 159 (B4, slot 19). msg1 start 0, root 1 | OCUDU rejects PRACH in special slots (152). `nr-oru` supports only B4 |
| VLAN | 30, as a port VLAN on the PF | The NIC adds and removes the tag. The applications send untagged frames |
| OCUDU `mtu` | 9000 | On mlx5, the OCUDU mbuf (9600 + 128 B, no Rx scatter) is too small for the default MTU 9600 |
| OCUDU `cplane_prach_fft_len` | 256 | Gives PRACH frameStructure 0x81 (FFT 256, 30 kHz) |
| OCUDU `ru_reference_level_dBFS` | -4.96 | OCUDU does not start without an IQ scaling setting |
| UE `--uecap_file uecap_ports1_n77.xml` | `uecap_ports1.xml` with band 77 (section 8) | OCUDU does not send maxMIMO-Layers. Without a UE capability file for band 77, the UE asserts on the first DCI 1_1 |

## 4. Host file

All commands, scripts and configuration files of this guide take the host values
from one file: `~/ocudu_env.sh`. Write it once for your host. Run `source
~/ocudu_env.sh` in each new terminal.

The values below are for the x86_64 test host:

```bash
# ~/ocudu_env.sh
# Host part: change these values for your host.
export PF=enp1s0f0np0          # fronthaul PF (ethtool -i). Never the SSH or backhaul port
export DU_VF=0000:01:01.0      # readlink /sys/class/net/$PF/device/virtfn0
export RU_VF=0000:01:01.1      # readlink /sys/class/net/$PF/device/virtfn1
export NET=192.168.73          # core subnet: a /26 that is free on the host (ip -br addr)
export D=$HOME/dpdk-24.11.4    # DPDK 24.11.4 prefix (section 6.2)
export DLIB=$D/lib/x86_64-linux-gnu
export RP="-L$DLIB -Wl,--disable-new-dtags,-rpath,$DLIB"   # "" when DPDK is in /usr/local
# Cores (cat /sys/devices/system/cpu/isolated). Each part on its own isolated
# cores, with no overlap. These x86_64 counts are the tested minimum.
export HK=0-5                  # housekeeping cores: builds, Docker, iperf3
export DU_CPUS=6-14,28-31 ORU_CPUS=15-22 UE_CPUS=23-27
# OCUDU threads, on DU_CPUS: EAL, timing, Tx/Rx, 3 RU cores, then the worker pool
export DU_EAL=6 DU_TIMING=7 DU_TXRX=8 DU_RU=9-11 DU_POOL=12-14,28-31
# nr-oru threads, on ORU_CPUS: Rx, 4 TP cores, north, south, Tx
export ORU_RX=15 ORU_TP=16,17,18,19 ORU_NORTH=20 ORU_SOUTH=21 ORU_TX=22

# Common part: no change necessary.
export BLD=$HOME/openairinterface5g/build RUN=$HOME/ocudu_run CN=$HOME/oai-cn5g-ocudu
export CONF=$HOME/openairinterface5g/targets/PROJECTS/GENERIC-NR-5GC/CONF
export PKG_CONFIG_PATH=$DLIB/pkgconfig
export DPDK_APPS='nr-softmodem|nr-oru|nr-uesoftmodem|gnb|testpmd|dpdk-testpmd'
export UE_ARGS="-O $CN/uicc.conf -r 106 --numerology 1 --band 77 -C 4049760000 \
  --ssb 180 --device.name vrtsim --vrtsim.role client \
  --log_config.global_log_options utc_time --uecap_file $RUN/uecap_ports1_n77.xml"
```

On the aarch64 test host, the host part is:

```bash
export PF=enP2p1s0f1np1 DU_VF=0002:01:02.2 RU_VF=0002:01:02.3 NET=192.168.81
export D=/usr/local DLIB=/usr/local/lib/aarch64-linux-gnu RP=""
export HK=0-3,65-71 DU_CPUS=4-29 ORU_CPUS=30-45 UE_CPUS=46-64
export DU_EAL=4 DU_TIMING=5 DU_TXRX=6 DU_RU=7-9 DU_POOL=10-29
export ORU_RX=30 ORU_TP=31,32,33,34 ORU_NORTH=35 ORU_SOUTH=36 ORU_TX=37
```

## 5. Before you start

1. Make sure that no other process uses the PF, its VFs or the hugepages:

   ```bash
   source ~/ocudu_env.sh
   pgrep -a -x "$DPDK_APPS"
   docker ps
   grep -E 'HugePages_(Total|Free)' /proc/meminfo
   ip link show $PF
   ```

2. Write down the PF mtu, the VF count of the PF and the Docker state. Section 10
   sets them back.

> **Safety:** never change a VF setting or `sriov_numvfs` while a DPDK process
> has the VF or its PF open. This hung all SSH logins on a lab host. Stop the
> process first.

## 6. Build

Build on the host that runs the binaries. Use only the housekeeping cores:
prefix each build command with `taskset -c $HK nice`. The full build takes about
12 minutes on 6 cores of the x86_64 test host, without ccache.

### 6.1 Compilers

| Build | aarch64 host | x86_64 host | Rule |
|---|---|---|---|
| OAI | default gcc 12.3 | `gcc-12` | gcc 14 and later fail with `implicit declaration of function nr_dump_frame_parms` |
| OCUDU `release_26_10` | `gcc-11` | `gcc-11`, with `yaml-cpp` 0.8.0 built with g++-11 | Use the compiler of the `yaml-cpp` library. Else the link fails with `GLIBCXX_3.4.32` |

### 6.2 DPDK 24.11.4

If `pkg-config --modversion libdpdk` gives 24.11.4, use it. Else build it in
your home folder:

```bash
cd ~ && wget http://fast.dpdk.org/rel/dpdk-24.11.4.tar.xz
tar xf dpdk-24.11.4.tar.xz
cd dpdk-stable-24.11.4
meson setup --prefix=$HOME/dpdk-24.11.4 -Dtests=false build
ninja -C build install
```

The OAI and OCUDU builds link with `RP` of `~/ocudu_env.sh`. `RP` is necessary
when DPDK is not in the default linker path:

- Without `-L`, `ld` can find another DPDK in `/usr/local`.
- Without `--disable-new-dtags`, the binary gets `RUNPATH`. `RUNPATH` does not
  apply to the indirect DPDK libraries, and `sudo` drops `LD_LIBRARY_PATH`. The
  binary then stops at start with `librte_*.so.25 => not found`.

### 6.3 OAI (`nr-oru`, `nr-uesoftmodem`)

Build from this repository. The tests used `develop` at commit `88245ba846`.

```bash
source ~/ocudu_env.sh
cd ~/openairinterface5g && mkdir -p build && cd build
cmake .. -GNinja -DOAI_RU_FRONTHAUL=ON \
  -DCMAKE_C_COMPILER=gcc-12 -DCMAKE_CXX_COMPILER=g++-12 \
  -DCMAKE_EXE_LINKER_FLAGS="$RP"
ninja nr-oru nr-uesoftmodem vrtsim params_libconfig coding ldpc dfts
```

- When the default gcc is 12 or older, remove the two compiler options.
- Check the result: `ldd nr-oru | grep "not found"` must give no line.

### 6.4 OCUDU `release_26_10` (`gnb_split_7_2`)

Get the release tag:

```bash
# release_26_10 = e0db566aac
mkdir -p ~/ocudu && cd ~/ocudu
git clone https://gitlab.com/ocudu/ocudu.git src
git -C src checkout release_26_10
```

If the system `yaml-cpp` was built with a gcc newer than 11, build a static copy
with g++-11 first:

```bash
git clone --depth 1 -b 0.8.0 https://github.com/jbeder/yaml-cpp.git \
  ~/yaml-cpp-src
cmake -S ~/yaml-cpp-src -B ~/yaml-cpp-src/build -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=gcc-11 -DCMAKE_CXX_COMPILER=g++-11 \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DYAML_BUILD_SHARED_LIBS=OFF \
  -DYAML_CPP_BUILD_TESTS=OFF -DYAML_CPP_BUILD_TOOLS=OFF \
  -DCMAKE_INSTALL_PREFIX=$HOME/yaml-cpp-0.8.0-gcc11
ninja -C ~/yaml-cpp-src/build install
```

CMake 4.x needs `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` for `yaml-cpp` 0.8.0.

Then build the release:

```bash
source ~/ocudu_env.sh
mkdir -p ~/ocudu/build && cd ~/ocudu/build
cmake ../src -GNinja -DCMAKE_BUILD_TYPE=Release -DENABLE_DPDK=ON \
  -DMCPU=native -DASSERT_LEVEL=AUTO \
  -DBUILD_TESTING=OFF -DCMAKE_C_COMPILER=gcc-11 -DCMAKE_CXX_COMPILER=g++-11 \
  -DCMAKE_PREFIX_PATH=$HOME/yaml-cpp-0.8.0-gcc11 \
  -DCMAKE_EXE_LINKER_FLAGS="$RP"
ninja gnb_split_7_2
```

- Remove `-DCMAKE_PREFIX_PATH` when the system `yaml-cpp` links with gcc-11.
- The binary is `~/ocudu/build/apps/gnb_split_7_2/gnb`. It prints `OCUDU gNB
  (commit e0db566aac)` at start.
- The CMake output must show `Found libdpdk, version 24.11.4`. If it shows
  `could not be satisfied`, CMake builds OCUDU without DPDK and gives no error.
  The `gnb` then stops at start with `'hal' section is present but the
  application was built without DPDK support`. Run `source ~/ocudu_env.sh`
  (it sets `PKG_CONFIG_PATH`), then run `cmake` again.
- The aarch64 host needed these extra packages: `libmbedtls-dev`,
  `libgtest-dev`, `libdw-dev`. FFTW3F and ZeroMQ were already installed. The
  x86_64 host did not need `libdw-dev`.
- `-DMCPU=native` builds for the CPU of the build host: NEON on aarch64, AVX2
  and AVX-512 on the x86_64 host.

## 7. Set up the environment

### 7.1 Hugepages and isolated cores

The steps in this guide do not change these. The host must already have:

- isolated cores (`isolcpus`, `nohz_full`) for the DU, the O-RU and the UE
  (section 4)
- at least 16 GB of free hugepages

To set them up, see [ORAN_FHI7.2_Tutorial.md](ORAN_FHI7.2_Tutorial.md),
"Configure your server".

### 7.2 SR-IOV

Save this script as `~/sriov.sh`. Run it before each run. It takes all host
values from `~/ocudu_env.sh`, and works on Intel (`ice`) and mlx5 PFs.

```bash
#!/bin/bash
# Make 2 VFs on $PF: VF0 = DU, VF1 = O-RU. Port VLAN 30.
set -ex
source ~/ocudu_env.sh
if pgrep -a -x "$DPDK_APPS"; then echo "BUSY: do not change the VFs"; exit 1; fi
sudo ip link set $PF mtu 9700
echo 0 | sudo tee /sys/class/net/$PF/device/sriov_numvfs
echo 2 | sudo tee /sys/class/net/$PF/device/sriov_numvfs
sleep 1
VFS=$(basename -a $(readlink /sys/class/net/$PF/device/virtfn[01]))
if [ "$(echo $VFS)" != "$DU_VF $RU_VF" ]; then
  echo "DU_VF and RU_VF are not VF0 and VF1 of $PF: $VFS"; exit 1
fi
# Intel (iavf VF): bind the VFs to vfio-pci.
# mlx5: no bind. DPDK uses the mlx5 kernel driver.
if [ "$(basename $(readlink /sys/class/net/$PF/device/driver))" = ice ]; then
  for v in $VFS; do
    sudo $D/bin/dpdk-devbind.py -u $v || true
    sudo $D/bin/dpdk-devbind.py -b vfio-pci $v
  done
fi
# Set MAC and VLAN after the bind: the iavf unbind resets the VF MAC.
# "state enable": the VF link stays up when the PF has no cable.
# Else no frame goes from VF to VF.
sudo ip link set $PF vf 0 mac 02:00:00:00:00:01 vlan 30 spoofchk off \
  trust on state enable
sudo ip link set $PF vf 1 mac 02:00:00:00:00:02 vlan 30 spoofchk off \
  trust on state enable
ip link show $PF | grep vf
```

To remove the VFs, see section 10.

For more detail on VFs and DPDK drivers, see
[ORAN_FHI7.2_Tutorial.md](ORAN_FHI7.2_Tutorial.md), "Configure Network
Interfaces and DPDK VFs".

### 7.3 PTP

All parts run on one host and use the same clock. PTP lock is not necessary.

- If `ptp4l` is locked to a grandmaster, keep `ptp4l` and `phc2sys` on.
- If `ptp4l` has no grandmaster, stop `phc2sys` for the session. Else it can
  move the system clock.

### 7.4 5G core

You can use your own core. Then set the PLMN, TAC, subscriber and IP addresses
of your core in the DU and UE files. This section uses the core in
`doc/tutorial_resources/oai-cn5g` of this repository. For the general steps, see
[NR_SA_Tutorial_OAI_CN5G.md](NR_SA_Tutorial_OAI_CN5G.md).

1. Copy the core folder and set the subnet and the PLMN:

   ```bash
   source ~/ocudu_env.sh
   cp -r ~/openairinterface5g/doc/tutorial_resources/oai-cn5g $CN
   cd $CN
   sed -i "s/192\.168\.70\./$NET./g" docker-compose.yaml conf/config.yaml
   sed -i -e 's/mcc: 001/mcc: 208/' -e 's/mnc: 01/mnc: 95/' \
     -e 's/tac: 0x0001/tac: 0xA000/' conf/config.yaml
   ```

   The default subnet 192.168.70.128/26 is often in use on shared hosts. Use the
   free subnet `$NET` of section 4. If other containers on the host already use
   the default names (`oai-amf`, `mysql`, `oai-ext-dn` and more), add a suffix
   to each `container_name` in `docker-compose.yaml`. Then use the new names in
   the `docker exec` commands.

2. Save the subscriber as `$CN/add_sub.sql`. The default database has no
   subscriber for PLMN 208/95.

   ```sql
   INSERT INTO `AuthenticationSubscription` (`ueid`, `authenticationMethod`, `encPermanentKey`, `protectionParameterId`, `sequenceNumber`, `authenticationManagementField`, `algorithmId`, `encOpcKey`, `encTopcKey`, `vectorGenerationInHss`, `n5gcAuthMethod`, `rgAuthenticationInd`, `supi`) VALUES
       ('208950000000001', '5G_AKA', 'fec86ba6eb707ed08905757b1bb44b8f', 'fec86ba6eb707ed08905757b1bb44b8f', '{\"sqn\": \"000000000000\", \"sqnScheme\": \"NON_TIME_BASED\", \"lastIndexes\": {\"ausf\": 0}}', '8000', 'milenage', 'C42449363BBAD02B66D16BC975D77CC1', NULL, NULL, NULL, NULL, '208950000000001');
   INSERT INTO `SessionManagementSubscriptionData` (`ueid`, `servingPlmnid`, `singleNssai`, `dnnConfigurations`) VALUES
   ('208950000000001', '20895', '{\"sst\": 1, \"sd\": \"FFFFFF\"}','{\"oai\":{\"pduSessionTypes\":{ \"defaultSessionType\": \"IPV4\"},\"sscModes\": {\"defaultSscMode\": \"SSC_MODE_1\"},\"5gQosProfile\": {\"5qi\": 6,\"arp\":{\"priorityLevel\": 15,\"preemptCap\": \"NOT_PREEMPT\",\"preemptVuln\":\"PREEMPTABLE\"},\"priorityLevel\":1},\"sessionAmbr\":{\"uplink\":\"1000Mbps\", \"downlink\":\"1000Mbps\"},\"staticIpAddress\":[{\"ipv4Addr\": \"10.0.0.2\"}]},\"ims\":{\"pduSessionTypes\":{ \"defaultSessionType\": \"IPV4V6\"},\"sscModes\": {\"defaultSscMode\": \"SSC_MODE_1\"},\"5gQosProfile\": {\"5qi\": 2,\"arp\":{\"priorityLevel\": 15,\"preemptCap\": \"NOT_PREEMPT\",\"preemptVuln\":\"PREEMPTABLE\"},\"priorityLevel\":1},\"sessionAmbr\":{\"uplink\":\"1000Mbps\", \"downlink\":\"1000Mbps\"}}}');
   ```

3. Save the UE SIM file as `$CN/uicc.conf`:

   ```
   uicc0 = {
     imsi = "208950000000001";
     key = "fec86ba6eb707ed08905757b1bb44b8f";
     opc= "C42449363BBAD02B66D16BC975D77CC1";
     pdu_sessions = ({ dnn = "oai"; nssai_sst = 1; });
   }
   ```

4. If Docker is stopped, start it for this session only: `sudo systemctl start
   docker`. Do not enable it. Docker changes `net.ipv4.ip_forward` and the
   iptables rules.

5. Start the core and add the subscriber. The database does not keep data
   between starts, so do this at each start:

   ```bash
   cd $CN && docker compose up -d
   # wait until all 10 containers are healthy
   watch -n 5 'docker ps --filter health=healthy --format "{{.Names}}" | wc -l'
   docker exec -i mysql mysql -uroot -plinux oai_db < add_sub.sql
   ping -c1 $NET.132                                   # AMF
   ```

6. Stop the core: `docker compose down`.

## 8. Configuration files

The configuration files are templates in
`targets/PROJECTS/GENERIC-NR-5GC/CONF` of this repository. `envsubst` writes the
host values of `~/ocudu_env.sh` into them:

```bash
source ~/ocudu_env.sh
mkdir -p $RUN && cd $RUN
envsubst < $CONF/ocudu.sa.band77.106prb.fhi72.1x1-oru.yaml > ocudu.yml
envsubst < $CONF/ru.sa.band77.106prb.fhi72.1x1-ocudu.conf > oru.conf
sed 's#<bandNR>78</bandNR>#<bandNR>77</bandNR>#g' \
  $CONF/uecap_ports1.xml > uecap_ports1_n77.xml
```

Check that `~/ocudu_env.sh` sets all template variables. This command must give
no line:

```bash
for v in $(envsubst -v "$(cat $CONF/ocudu.sa.band77.106prb.fhi72.1x1-oru.yaml \
    $CONF/ru.sa.band77.106prb.fhi72.1x1-ocudu.conf)"); do
  [ -n "${!v}" ] || echo "not set: $v"
done
```

- `uecap_ports1_n77.xml`: the UE capability files in the repository have band
  78 only. The UE takes its layer count from the band combination of the
  serving band. With the band 78 file, the UE asserts `max_mimo_layers > 0` on
  the first DCI 1_1.
- `nr-oru` exits on SIGTERM without `Bye.`. This is known behavior.

## 9. Run

Use one terminal for each part, or start each part in the background with its
own log file. Run all commands from `$RUN`, after `source ~/ocudu_env.sh`.

### 9.1 Start

1. Clean up the files of the last run (section 9.4).
2. Start the core (section 7.4, step 5).
3. Make the VFs: `bash ~/sriov.sh`.
4. Start the O-RU. Wait 5 s.

   ```bash
   sudo -E taskset -c $ORU_CPUS $BLD/nr-oru -O oru.conf \
     --device.name vrtsim --vrtsim.role server \
     --log_config.global_log_options utc_time > oru.log 2>&1 &
   ```

5. Start the DU:

   ```bash
   sudo taskset -c $DU_CPUS ~/ocudu/build/apps/gnb_split_7_2/gnb -c ocudu.yml
   ```

6. Wait until the DU fronthaul runs. Then wait 3 s more. Use the `nr-oru` DL
   C-plane count in `oru.log`: the line `Total C-Plane Packets received: N (DL:
   M, ...` must show M > 0. `nr-oru` writes it every 10 s. Do not wait for
   `Started the operation of the Open Fronthaul interface` in `ocudu_gnb.log`:
   OCUDU writes this log late.

   If the UE starts before the DU fronthaul, the UE retunes to a wrong frequency
   and does not attach.

7. Start the UE:

   ```bash
   sudo -E taskset -c $UE_CPUS $BLD/nr-uesoftmodem $UE_ARGS > ue.log 2>&1 &
   ```

8. Wait for the PDU session. `ip -4 addr show oaitun_ue1` shows `10.0.0.2` after
   1-3 s.

In the OCUDU console, type `t` to show the metrics (CQI, MCS, HARQ NOK) every
second.

### 9.2 Traffic

Send UL traffic first, for example the UL ping below. The core forwards DL
traffic to the UE only after the first UL packet of the PDU session. Before
that, DL ping and DL iperf3 get no reply.

```bash
# Ping UL and DL, every 0.2 s
ping -I oaitun_ue1 -i 0.2 -c 300 $NET.135
docker exec oai-ext-dn ping -i 0.2 -c 300 10.0.0.2

# The UE-side iperf3 runs in a container with host network,
# on the housekeeping cores
IPERF="docker run --rm --network host --cpuset-cpus $HK \
  --entrypoint iperf3 oaisoftwarealliance/trf-gen-cn5g:latest"

# DL iperf3 UDP, one step: server on the UE side, client in oai-ext-dn
$IPERF -s -1 -B 10.0.0.2 -p 5201 &
sleep 1; docker exec oai-ext-dn iperf3 -c 10.0.0.2 -p 5201 -u -b 100M \
  -t 30 --get-server-output

# UL iperf3 UDP, one step: server in oai-ext-dn, client on the UE side
docker exec -d oai-ext-dn iperf3 -s -1 -p 5202
sleep 1; $IPERF -c $NET.135 -B 10.0.0.2 -p 5202 -u -b 30M \
  -t 30 --get-server-output
```

For a staircase, repeat one step for each rate: DL 10 to 150 Mbit/s, UL 10 to 60
Mbit/s, in 10 Mbit/s steps. Stop at the first step with more than 5 % loss.
Compare with section 2.

### 9.3 Stop

1. Stop the UE: `sudo pkill -TERM -x nr-uesoftmodem`.
2. Stop the DU: type `q` in its console. If it does not stop in 20 s, `sudo
   pkill -TERM -x gnb`.
3. Stop the O-RU: `sudo pkill -TERM -x nr-oru`.
4. If a process hangs, get its stacks before you kill it: `sudo gdb -p <pid>
   -batch -ex 'thread apply all bt'`.
5. Check `ue.log`: no `Assertion`, and `Bye.` at the end.
6. Stop the core: `docker compose down` in `$CN`.

### 9.4 Clean up after each run

`nr-oru` and the OCUDU DU can leave hugepage files and DPDK runtime files after
SIGTERM. Remove them only when no DPDK process runs:

```bash
if pgrep -a -x "$DPDK_APPS"; then
  echo "BUSY: do not clean"
else
  sudo rm -f /dev/hugepages/rumap_* /dev/hugepages/ocudu_dumap_*
  sudo rm -rf /var/run/dpdk/ru /var/run/dpdk/ocudu_du
  sudo rm -f /dev/shm/vrtsim_channel
fi
grep -E 'HugePages_(Total|Free)' /proc/meminfo
```

Remove only the files of your own runs. Do not remove other users' hugepage
files.

## 10. Tear down

At the end of the session, put the host back to the state that you wrote down in
section 5:

1. Make sure that no DPDK process runs: `pgrep -a -x "$DPDK_APPS"`.
2. Clean up (section 9.4). Make sure that your hugepages are free.
3. Remove the VFs: `echo 0 | sudo tee /sys/class/net/$PF/device/sriov_numvfs`.
   Set the PF mtu back. If the VFs existed before the session, make them again
   with their old settings.
4. If you started Docker, stop it. Restore the firewall and Docker state of your
   host.
5. Make sure that `ptp4l` and `phc2sys` are in their state from before.

## 11. Known limits

- `nr-oru`: PRACH format B4 only, TDD only.
- OCUDU: no PRACH in special slots, no config key for the SSB position.
- OCUDU warns that its PUCCH overlaps the PRACH in PRB [0, 6). This had no
  effect with CSI-RS on.
- There was no 300 s soak with CSI-RS on.
