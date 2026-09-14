<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# OAI 5G NR SA tutorial with OAI nrUE

**Table of Contents**

[[_TOC_]]

##  1. Scenario
In this tutorial we describe how to configure and run a 5G end-to-end setup with OAI CN5G, OAI gNB and OAI nrUE.

Minimum hardware requirements:

- Laptop/Desktop/Server for OAI CN5G and OAI gNB

    - Operating System: [Ubuntu Desktop 26.04.1 LTS, Intel or AMD 64-bit architecture](https://releases.ubuntu.com/resolute/ubuntu-26.04.1-desktop-amd64.iso)
    - CPU: 8 cores x86_64 @ 3.5 GHz
    - RAM: 32 GB

- Laptop for UE

    - Operating System: [Ubuntu Desktop 26.04.1 LTS, Intel or AMD 64-bit architecture](https://releases.ubuntu.com/resolute/ubuntu-26.04.1-desktop-amd64.iso)
    - CPU: 8 cores x86_64 @ 3.5 GHz
    - RAM: 8 GB

- [USRP B210](https://www.ettus.com/all-products/ub210-kit/), [USRP N300](https://www.ettus.com/all-products/USRP-N300/) or [USRP X300](https://www.ettus.com/all-products/x300-kit/)

    - Please identify the network interface(s) on which the USRP is connected and update the gNB configuration file


## 2. OAI CN5G

### 2.1 OAI CN5G pre-requisites

Please install and configure OAI CN5G as described here:
[OAI CN5G](NR_SA_Tutorial_OAI_CN5G.md)


## 3. OAI gNB and OAI nrUE

### 3.1 OAI gNB and OAI nrUE pre-requisites

#### Build UHD from source
```bash
# https://files.ettus.com/manual/page_build_guide.html
sudo apt install -y autoconf automake build-essential ccache cmake cpufrequtils doxygen ethtool g++ git inetutils-tools libboost-all-dev libncurses-dev libusb-1.0-0 libusb-1.0-0-dev libusb-dev python3-dev python3-mako python3-numpy python3-requests python3-scipy python3-setuptools python3-ruamel.yaml libgrpc++-dev libprotobuf-dev protobuf-compiler-grpc

git clone https://github.com/EttusResearch/uhd.git ~/uhd
cd ~/uhd
git checkout v4.11.0.0
cd host
mkdir build
cd build
cmake ../
make -j $(nproc)
make test # This step is optional
sudo make install
sudo ldconfig
sudo uhd_images_downloader
```

### 3.2 Build OAI gNB and OAI nrUE

```bash
# Get openairinterface5g source code
git clone https://github.com/duranta-project/openairinterface5g.git ~/openairinterface5g
cd ~/openairinterface5g
git checkout develop

# Install OAI dependencies
cd ~/openairinterface5g/cmake_targets
./build_oai -I

# nrscope dependencies
sudo apt install -y libforms-dev libforms-bin

# Build OAI gNB
cd ~/openairinterface5g/cmake_targets
./build_oai -w USRP --ninja --nrUE --gNB --build-lib "nrscope" -C
```

## 4. Run OAI CN5G and OAI gNB

### 4.1 Run OAI CN5G

```bash
cd ~/oai-cn5g
docker compose up -d
```

### 4.2 Run OAI gNB

**Note:** From tag `2024.w45`, OAI gNB runs by default in standalone (SA) mode.  
In earlier versions the default mode was non-standalone (NSA).  
If you are using an earlier version than `2024.w45`, you should add the `--sa` argument to the sample commands below to obtain a correct behavior.

#### USRP B210
```bash
cd ~/openairinterface5g/cmake_targets/ran_build/build
sudo ./nr-softmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.sa.band78.fr1.106PRB.usrpb210.conf --gNBs.[0].min_rxtxtime 6 -E --continuous-tx
```
#### USRP N300
```bash
cd ~/openairinterface5g/cmake_targets/ran_build/build
sudo ./nr-softmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.sa.band77.fr1.273PRB.2x2.usrpn300.conf --gNBs.[0].min_rxtxtime 6 --usrp-tx-thread-config 1
```

#### USRP X300
```bash
cd ~/openairinterface5g/cmake_targets/ran_build/build
sudo ./nr-softmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.sa.band77.fr1.273PRB.2x2.usrpn300.conf --gNBs.[0].min_rxtxtime 6 --usrp-tx-thread-config 1 -E --continuous-tx
```

#### RFsimulator
```bash
cd ~/openairinterface5g/cmake_targets/ran_build/build
sudo ./nr-softmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.sa.band78.fr1.106PRB.usrpb210.conf --gNBs.[0].min_rxtxtime 6 --rfsim
```

#### RFsimulator in FR2
```bash
cd ~/openairinterface5g/cmake_targets/ran_build/build
sudo ./nr-softmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.sa.band257.u3.32prb.usrpx410.conf --rfsim
```

## 5. OAI UE

### 5.1 Run OAI nrUE

**Note:** From tag `2024.w45`, OAI nrUE runs by default in standalone (SA) mode.  
In earlier versions the default mode was non-standalone (NSA).  
If you are using an earlier version than `2024.w45`, you should add the `--sa` argument to the sample commands below to obtain a correct behavior.

#### USRP B210
Important notes:
- This should be run in a second Ubuntu 22.04 host, other than gNB
- It only applies when running OAI gNB with USRP B210

Run OAI nrUE with USRP B210
```bash
cd ~/openairinterface5g/cmake_targets/ran_build/build
sudo ./nr-uesoftmodem -r 106 --numerology 1 --band 78 -C 3619200000 --ue-fo-compensation -E --uicc0.imsi 001010000000001
```

#### RFsimulator
Important notes:
- This should be run on the same host as the OAI gNB
- It only applies when running OAI gNB with RFsimulator

Run OAI nrUE with RFsimulator
```bash
cd ~/openairinterface5g/cmake_targets/ran_build/build
sudo ./nr-uesoftmodem -r 106 --numerology 1 --band 78 -C 3619200000 --uicc0.imsi 001010000000001 --rfsim
```

#### RFsimulator in FR2
Important notes:
- This should be run on the same host as the OAI gNB
- It only applies when running OAI gNB with RFsimulator in FR2

Run OAI nrUE with RFsimulator in FR2
```bash
cd ~/openairinterface5g/cmake_targets/ran_build/build
sudo ./nr-uesoftmodem -r 32 --numerology 3 --band 257 -C 27533280000 --uicc0.imsi 001010000000001 --ssb 72 --rfsim
```

#### Connection to an NG-Core

A configuration file can be fed to the nrUE command line in order to connect to the local NGC.

The nrUE configuration file (e.g. [ue.conf](../targets/PROJECTS/GENERIC-NR-5GC/CONF/ue.conf)) is structured in a key-value format and contains the relevant UICC parameters that are necessary to authenticate the UE to the local 5GC. E.g.:

```shell
uicc0 = {
  imsi = "001010000000001";
  key = "fec86ba6eb707ed08905757b1bb44b8f";
  opc = "C42449363BBAD02B66D16BC975D77CC1";
  pdu_sessions = ({ dnn = "oai"; nssai_sst = 1; });
}
```

Further information about the meaning of these parameters is described in the
[dedicated nrUE configuration page](./runmodem-nrue.md).

The UE configuration must match the one of the network's AMF. When running the
`nr-uesoftmodem`, one can specify the nrUE configuration file using the `-O`
option. E.g.:

```bash
sudo ./nr-uesoftmodem --rfsim --rfsimulator.[0].serveraddr 127.0.0.1 -r 106 --numerology 1 --band 78 -C 3619200000 -O ~/nrue.uicc.conf
```

The command line option `--uicc0.imsi`  can override the IMSI value in the
configuration file if necessary (e.g. when running multiple UEs): `--uicc0.imsi
001010000000001`.

More details can also be found at
[ci-scripts/yaml_files/5g_rfsimulator/README.md](../ci-scripts/yaml_files/5g_rfsimulator/README.md).

**Note:** From tag `2024.w45`, OAI nrUE runs by default in standalone (SA) mode.  
In earlier versions the default mode was non-standalone (NSA).  
If you are using an earlier version than `2024.w45`, you should add the `--sa` argument to the sample commands above to obtain a correct behavior.

### 5.2 End-to-end connectivity test
- Ping test from the UE host to the CN5G
```bash
ping 192.168.70.135 -I oaitun_ue1
```

## 6. Advanced configurations (optional)

### 6.1 USRP N300 and X300 Ethernet Tuning

Please also refer to the official [USRP Host Performance Tuning Tips and Tricks](https://kb.ettus.com/USRP_Host_Performance_Tuning_Tips_and_Tricks) tuning guide.

The following steps are recommended. Please change the network interface(s) as required. Also, you should have 10Gbps interface(s): if you use two cables, you should have the XG firmware. Refer to the [N300 Getting Started Guide](https://kb.ettus.com/USRP_N300/N310/N320/N321_Getting_Started_Guide) for more information.

* Use an MTU of 9000: how to change this depends on the network management tool. In the case of Network Manager, this can be done from the GUI.
* Increase the kernel socket buffer (also done by the USRP driver in OAI)
* Increase Ethernet Ring Buffers: `sudo ethtool -G <ifname> rx 4096 tx 4096`
* Disable hyper-threading in the BIOS (This step is optional)
* Optional: Disable KPTI Protections for Spectre/Meltdown for more performance. **This is a security risk.** Add `mitigations=off nosmt` in your grub config and update grub. (This step is optional)

Example code to run:
```
for ((i=0;i<$(nproc);i++)); do sudo cpufreq-set -c $i -r -g performance; done
sudo sysctl -w net.core.wmem_max=62500000
sudo sysctl -w net.core.rmem_max=62500000
sudo sysctl -w net.core.wmem_default=62500000
sudo sysctl -w net.core.rmem_default=62500000
sudo ethtool -G enp1s0f0 tx 4096 rx 4096
```

### 6.2 Real-time performance workarounds
- Enable Performance Mode `sudo cpupower idle-set -D 0`
- If you get real-time problems on heavy UL traffic, reduce the maximum UL MCS using an additional command-line switch: `--MACRLCs.[0].ul_max_mcs 14`.
- You can also reduce the number of LDPC decoder iterations, which will make the LDPC decoder take less time: `--L1s.[0].max_ldpc_iterations 4`.

### 6.3 Uplink issues related with noise on the DC carriers
- There is noise on the DC carriers on N300 and especially the X300 in UL. To avoid their use or shift them away from the center to use more UL spectrum, use the `--tune-offset <Hz>` command line switch, where `<Hz>` is ideally half the bandwidth, or possibly less.

### 6.4 Timing-related Problems
- Sometimes, the nrUE would keep repeating RA procedure because of Msg3 failure at the gNB. If it happens, add the `-A` option at the nrUE and/or gNB side, e.g., `-A 45`. This modifies the timing advance (in samples). Adjust +/-5 if the issue persists.
- This can be necessary since certain USRPs have larger signal delays than others; it is therefore specific to the used USRP model.
- The x310 and B210 are found to work with the default configuration; N310 and x410 can benefit from setting this timing advance.
- For example if the OAI UE uses the X410 and the gNB based on [Nvidia Aerial and Foxconn](./Aerial_FAPI_Split_Tutorial.md) a timing advance of 90 has been found to work well.  


### 6.5 Lower latency on user plane
- To lower latency on the user plane, you can force the UE to be scheduled constantly in uplink: `--MACRLCs.[0].ulsch_max_frame_inactivity 0` .
