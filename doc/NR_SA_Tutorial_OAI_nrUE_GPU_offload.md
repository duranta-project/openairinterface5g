# OAI 5G NR SA tutorial with OAI nrUE with GPU Offload

This tutorial explains how OAI nrUE can use GPU to offload LDPC decoder. In the future we might offload more functions on the GPU. 

**Table of Contents**

[[_TOC_]]

## Current Status

The nrUE GPU offload functionality have been tested on below platforms:

|Platform       |Version     | OS/Linux Kernel                       |
|---------------|------------|---------------------------------------|
|Jetson AGX Orin|L4T 36.4.7  | U22/5.15.148-tegra (64K page variant) |
|DGX Spark      |7.4.0       | U24/6.14.0-1013-nvidia-64k            |

The above versions are tested versions, you are free to try on newer versions as well. 

You can use another UE platform with GPU as well but it is preferred to use unified memory GPU variant.

### KPIs

The below throughput numbers are similar for TCP/UDP, the gNB was configured for 5 ms TDD pattern (6D3U and Special slot 10D0U) for Nvidia Aerial Testbed. 

The UE was in SISO mode.

Testbed Architecture: 

UE <--> Over the Air 1.5m to 2m distance <--> WNC RU <--> gNB/DU server

| Platform    | Radio     | Bandwidth | DL Throughput | UL Throughput |
| ----------- | --------- | --------- | ------------- | ------------- |
| Jetson Orin | B210      | 10 MHz    | 12 Mbps       | 7.5 Mbps      |
| Jetson Orin | B210      | 20 MHz    | 20 Mbps       | 9.5 Mbps      |
| Jetson Orin | B210      | 30 MHz    | 61 Mbps       | 33 Mbps       |
| Jetson Orin | B210      | 40 MHz    | 69 Mbps       | 46 Mbps       |
| DGX Spark   | B210      | 40 MHz    | 86 Mbps       | 46 Mbps       |
| DGX Spark   | N310/x410 | 100 MHz   | 231 Mbps      | 118 Mbps      |

Stability or long running test with peak throughput for nrUE was performed for 5hrs.

All the numbers are with LDPC GPU offload.

`NOTE`: At the time of writing we only tested the UE in SISO. 
We are working supporting multi antenna use case.

## Aerial Testbed Configuration

If you are using aerial testbed with OAI then depending on the bandwidth you want to use, you will have to adapt the value of `ds_offset_dl` in cuphycontroller configuration file. 

For our testing environment we adapted the `ds_offset_dl` values as below:

| Bandwidth   | ds_offset_dl |
| ----------- | -------------| 
| 10 MHz      | 14           | 
| 20 MHz      | 12           | 
| 30 MHz      | 10           | 
| 40 MHz      | 9            | 
| 100 MHz     | 6            |

On the WNC radio we have the below configuration: 

```
transmit-power 24
transmit-power-scale 0.0
compress tx static bfp iq-bitwidth 9
compress rx static bfp iq-bitwidth 9
compress prach static bfp iq-bitwidth 9
```

You can adjust the scaling on the RU or you can adjust it in aerial configuration file. 

**NOTE**: You might have to adjust these values for your environment, so that the UE can detect the cell and perform random-access.

## Configure the nrUE Hardware Platform

### Configure Jetson AGX Orin

At the time of writing this tutorial AGX Orin only had Jetpack 6.X release which has U22/5.15.148-tegra, for GPU offload on ARM systems it is preferred to use 64K page size kernel. 

To change the kernel to 64K page size variant you will have to manually build the kernel. Probably for Jetpack 7.X it will be easier. Before building the kernel make sure AGX Orin has the up to date kernel for your current distribution. 

NVIDIA-SMI Version: 540.4.0
CUDA Version: 12.6
CPU: Cortex-A78AE (12)
OS: U22
Kernel: 5.15.148-tegra
NVCC: Build cuda_12.6.r12.6/compiler.34714021_0
UHD: UHD_4.9.0.HEAD-0-g006d7f76 (v4.9.0.0)

#### Update kernel to 64K page size

Login to Jetson AGX Orin

```bash
sudo apt update
sudo apt upgrade
# reboot
```

**CAUTION**: While building the kernel if you make a mistake you have to flash again the Jetson. We are just providing these steps to help you build the kernel with 64K page size. If you have questions or issues building the kernel you can check the [official nvidia documentation](https://docs.nvidia.com/jetson/archives/r36.4.4/DeveloperGuide/SD/Kernel/KernelCustomization.html#sd-kernel-kernelcustomization)

prepare the kernel source code:

```bash
sudo apt install git-core build-essential bc
cd ~/
wget https://developer.nvidia.com/downloads/embedded/l4t/r36_release_v4.4/sources/public_sources.tbz2
# Build Jetson Linux Kernel
tar -xf public_sources.tbz2
cd ~/Linux_for_Tegra/source
tar -xf kernel_src.tbz2
tar -xf kernel_oot_modules_src.tbz2
tar -xf nvidia_kernel_display_driver_source.tbz2
```

the default kernel configuration file used is `kernel/kernel-jammy-src/arch/arm64/configs/defconfig` it is mentioned in `kernel_src_build_env.sh`. 
Add `CONFIG_ARM64_64K_PAGES=y` in `kernel/kernel-jammy-src/arch/arm64/configs/defconfig`.

Build the kernel, out-of-tree modules and dtbs:

```bash
make -C kernel
sudo -E make install -C kernel
cp kernel/kernel-jammy-src/arch/arm64/boot/Image 
  /boot/Image
# Building the NVIDIA Out-of-Tree Modules
cd ~/Linux_for_Tegra/source
make modules
sudo -E make modules_install
sudo nv-update-initrd
make dtbs
cp kernel-devicetree/generic-dts/dtbs/* /boot/dtbs/
```

#### Configure operating system

```bash
jetson_clocks
touch /etc/sysctl.d/rt.conf
## add below content using vim/vi/nano
kernel.sched_rt_runtime_us=-1
kernel.timer_migration=0
sudo cpufreq-set -c 0 -g performance
sudo cpufreq-set -c 5 -g performance
sudo cpufreq-set -c 9 -g performance
```

### Configure DGX Spark

To setup DGX spark you would need to follow the below steps

NVIDIA-SMI Version: 580.95.05
CUDA Version: 13.0
OS: Ubuntu 24
Kernel: 6.14.0-1013-nvidia-64k (You can use a higher version)
NVCC: Build cuda_13.0.r13.0/compiler.36424714_0
UHD: UHD_4.9.0.HEAD-0-g006d7f76 (v4.9.0.0)
Efficient cores (Cortex-A725): 0,1,2,3,4,10,11,12,13,14
Performance cores (Cortex-X925): 5,6,7,8,9,15,16,17,18,19

You can use `sudo cpupower -c 0-19 frequency-info` to check the above core distribution. 


```bash
sudo apt update
sudo apt upgrade
#reboot
sudo /etc/apt/apt.conf.d/20auto-upgrades
APT::Periodic::Update-Package-Lists "0";
APT::Periodic::Unattended-Upgrade "0";
# Install 64K kernel corresponding to your running kernel
sudo apt install linux-image-6.14.0-1013-nvidia-64k
# You can also fix the kernel so it is not changed with apt upgrade
sudo sed -i 's/^GRUB_DEFAULT=.*/GRUB_DEFAULT="Advanced options for DGX OS
GNU\/Linux>DGX OS GNU\/Linux, with Linux 6.14.0-1013-nvidia-64k"/'
/etc/default/grub
#Configure the command line to isolate the cores
cat <<"EOF" | sudo tee /etc/default/grub.d/cmdline.cfg
GRUB_CMDLINE_LINUX="$GRUB_CMDLINE_LINUX pci=realloc=off pci=pcie_bus_safe
default_hugepagesz=512M hugepagesz=512M hugepages=24 tsc=reliable
processor.max_cstate=0 audit=0 idle=poll rcu_nocb_poll nosoftlockup
irqaffinity=0-3 isolcpus=managed_irq,domain,4-19
nohz_full=4-19 rcu_nocbs=4-19 earlycon module_blacklist=nouveau
acpi_power_meter.force_cap_on=y numa_balancing=disable init_on_alloc=0
preempt=none ras=off iommu=off mitgations=off kpti=off skew_tick=1 nowatchdog
mce=ignore_ce transparent_hugepage=never"
EOF
touch /etc/sysctl.d/rt.conf
## add below content using vim/vi/nano
kernel.sched_rt_runtime_us=-1
kernel.timer_migration=0
sudo update-grub
sudo reboot
# after reboot you can make a script to set the gpu clock speed to maximum  
nvidia-smi -lgc 2000
sudo cpupower frequency-set --governor performance
sudo cpupower idle-set -D 0
```

### Configuration for Ethernet based USRP

If you want to use Ethernet based USRP like N3XX/X3XX/X4XX you would need to run the below commands: 

```bash
sudo sysctl -w net.core.wmem_max=62500000
sudo sysctl -w net.core.rmem_max=62500000
sudo sysctl -w net.core.wmem_default=62500000
sudo sysctl -w net.core.rmem_default=62500000
sudo ethtool -G <usrp-interface> tx <maximum-value> rx <maximum-value>
```

*NOTE*: Currently we finalizing container images for OAI-NR-UE with GPU offload, we will provide it soon for container based deployments.

## Using nrUE with B2XX

### Build OAI-nrUE

```bash
mkdir ~/oai-nr-ue
git clone https://gitlab.eurecom.fr/oai/openairinterface5g.git ~/oai-nr-ue
cd ~/oai-nr-ue
git checkout ldpc_offload_cuda
cd ~/oai-nr-ue/openairinterface5g/cmake_targets
#Install third-party dependencies
./build_oai -I -w USRP
# Install nrue with USRP support, default nvcc location is /usr/local/bin/nvcc
./build_oai --ninja --build-lib ldpc_cuda --gNB --nrUE -w USRP -C --cmake-opt -DCMAKE_CUDA_COMPILER=<nvcc-location>
``` 

## Start the nrUE process

Create a UE configuration file based on the PLMN, slicing information, DNN and DNN type you have configured for the UE in the core network.

```bash
cd ~/oai-nr-ue/
# touch nr-ue.conf
```

Sample UE configuration file, you can use the below configuration file and adjust the values as per your testing environment: 

```bash
cat nr-ue.conf

uicc0 = {
imsi = "001010000000001";
key = "fec86ba6eb707ed08905757b1bb44b8f";
opc= "C42449363BBAD02B66D16BC975D77CC1";
pdu_sessions = ({ dnn = "oai"; nssai_sst = 1; });
}
```

To start the `nr-uesoftmodem` you would need to know the center frequency, bandwidth, band and ssb. This information is printed in the OAI-gNB logs. You can search for the line `[NR_MAC] Command line parameters for OAI UE` for example:

```bash
[NR_MAC] Command line parameters for OAI UE: -C 3350010000 -r 106 --numerology 1 --band 78 --ssb 513
```

Once you have this information you can start the `nr-uesoftmodem` process: 

**Command for Jetson AGX Orin**: 

**MANDATORY**: Before starting the UE process it is important to configure the gNB configuration file with `min_rxtxtime` as 6 or higher integer value.

```bash
cd ~/oai-nr-ue/openairinterface5g/cmake_targets/
sudo ./nr-uesoftmodem -O ~/oai-nr-ue/nr-ue.conf -C 3350010000 -r 106 --ssb 513 --numerology 1 --band 78 --thread-pool -1 --num-dl-actors 6 --num-ul-actors 2 -E --loader.ldpc.shlibversion _cuda --usrp-tx-thread-config 1 --ue-fo-compensation
```

**Command for DGX Spark**: 

**MANDATORY**: Before starting the UE process it is important to configure the gNB configuration file with `min_rxtxtime` as 6 or higher integer value.

```bash
cd ~/oai-nr-ue/openairinterface5g/cmake_targets/

sudo taskset -c "5-9,15-19" ./nr-uesoftmodem -O ~/oai-nr-ue/nr-ue.conf -C 3350010000 -r 106 --ssb 513 --numerology 1 --band 78 --thread-pool -1 -E  --loader.ldpc.shlibversion _cuda --ue-fo-compensation
```

In the above command task-set allows running the UE only on the performance cores. 

Understand the command which you just used: 

The main RF/cell settings are:

- `-O`: load the UE config file.
- `-C 3900000000`: set DL carrier to 3.9 GHz.
- `-r 106`: use 106 PRBs
- `--numerology 1`: numerology 1, meaning 30 kHz subcarrier spacing.
- `--band 77`: NR band n77.
- `--ssb 516`: expect the SSB to start at sub-carrier 516.
- `--ue-fo-compensation`: enable initial frequency-offset compensation, useful OTA when clocks are not perfectly aligned.
- `--usrp-tx-thread-config 1`: enable an extra USRP TX processing thread
- `-E`: enable threequarter_fs, meaning the UE uses three-quarter sampling rate mode. In OAI this reduces sample rate / transport load where supported.

The parallelism/offload settings are:

- `--thread-pool -1`: create a thread-pool worker with no CPU affinity.
- `--num-dl-actors 6`: use 6 DL actor threads, parallel RX/PDSCH processing workers.
- `--num-ul-actors 2`: use 2 UL actor threads, parallel TX/PUSCH/PUCCH preparation and transmit workers.
- `--loader.ldpc.shlibversion_cuda`: load the CUDA-backed LDPC shared library variant.

When the UE starts it starts with highest gains, but sometimes you would need to adjust the tx/rx gains based on the UEs position from the RU/gNB. To manipulate the gains you can use the below options: 

- `--ue-rxgain x`: set UE RX gain.
- `--ue-txgain x`: set UE TX gain.
- `-A 50`: apply a command-line sample advance of 50 samples to compensate RF timing offset.

Once the UE connects you will see the tunnel interface `oaitun_ue1` with ip-address in the subnet which is configured at SMF. You can open another terminal and from there you can do traffic test. 

## Using nrUE with X3XX/N3XX/x4XX

### Build OAI-nrUE

For Ethernet based USRPs it is recommended to use DPDK mostly when you want to test for 60-100 MHz bandwidth. If you don't want to use DPDK then you can build the `nr-uesoftmodem` using the previous section. 

```bash
mkdir ~/oai-nr-ue
git clone https://gitlab.eurecom.fr/oai/openairinterface5g.git ~/oai-nr-ue
cd ~/oai-nr-ue
git checkout ldpc_offload_cuda
cd ~/oai-nr-ue/openairinterface5g/cmake_targets
#Install third-party dependencies
./build_oai -I
#Build and Install DPDK
cd ~/
wget https://fast.dpdk.org/rel/dpdk-24.11.4.tar.xz
sudo apt install wget xz-utils libnuma-dev meson libibverbs-dev rdma-core python3-pyelftools
meson setup build -Dplatform=generic
ninja -C build
sudo ninja install -C build
#Build and Install UHD
sudo apt install autoconf automake build-essential ccache cmake cpufrequtils doxygen ethtool g++ git inetutils-tools libboost-all-dev libncurses-dev libusb-1.0-0 libusb-1.0-0-dev libusb-dev python3-dev python3-mako python3-numpy python3-requests python3-scipy python3-setuptools python3-ruamel.yaml
git clone https://github.com/EttusResearch/uhd ~/uhd && cd uhd
git checkout v4.9.0.0
cd host && mkdir build && cd build
cmake ../ -GNinja
ninja
sudo ninja install
# Install nrue with USRP support, default nvcc location is /usr/local/bin/nvcc
./build_oai --ninja --build-lib ldpc_cuda --gNB --nrUE -w USRP -C --cmake-opt -DCMAKE_CUDA_COMPILER=<nvcc-location>
```

Create the DPDK interfaces for the UHD, use the below script and modify it for your environment. We are using VLAN 80 for DPDK in our environment:  

```bash
vim setup_sriov_usrp.sh
```

```bash
#!/bin/sh
set -xeu

pci_addr()
{
    PF_IF=$1
    VF_INDEX=$2
    SYSFS_PATH="/sys/class/net/${PF_IF}/device/virtfn${VF_INDEX}"

    if [ ! -e "$SYSFS_PATH" ]; then
            echo "VF $VF_INDEX not found for interface $PF_IF"
                exit 1
    fi
    PCI_ADDR=$(basename "$(readlink "$SYSFS_PATH")")
    echo "$PCI_ADDR"
}
IF_NAME=enP2p1s0f1np1 ## interface to reach to the USRP on the UHD host
HOST_MAC_ADD=00:11:22:33:44:67
VLAN=80
MTU=9000
NUM_VFs=1
sudo ethtool -G $IF_NAME rx 8160 tx 8160
sudo sh -c "echo 0 > /sys/class/net/$IF_NAME/device/sriov_numvfs"
sudo sh -c "echo $NUM_VFs > /sys/class/net/$IF_NAME/device/sriov_numvfs"
HOST_PCI_ADDR=$(pci_addr $IF_NAME 0)
# this next 2 lines is for C/U planes
sudo ip link set $IF_NAME vf 0 mac $HOST_MAC_ADD vlan $VLAN spoofchk off mtu $MTU
sleep 1
sudo dpdk-devbind.py --unbind $HOST_PCI_ADDR
sudo modprobe mlx5_core
sudo dpdk-devbind.py --bind mlx5_core $HOST_PCI_ADDR
echo "Successfully configured interface for USRP:
  - Mac-address: $HOST_MAC_ADD, VLAN: $VLAN, PCI: $HOST_PCI_ADDR"
exit 0
```

Create `/etc/uhd/uhd.conf` configuration file for allowing UHD to use dpdk for reaching to the USRP:

```txt
[use_dpdk=1]
dpdk_mtu=9000
dpdk_driver=/usr/local/lib/aarch64-linux-gnu/dpdk/pmds-25.0/
dpdk_corelist=5,6,7,8,9
[dpdk_mac=00:11:22:33:44:67]
dpdk_lcore=6
dpdk_ipv4=192.168.80.25/24
```

The ip-address 192.168.80.25/24 is the UHD host ip-address. Make sure you adjust the `dpdk_driver` location. 


### Start the nrUE process

Create a UE configuration file based on the PLMN, slicing information, DNN and DNN type you have configured for the UE in the core network. It is similar to previous section. 

To start the `nr-uesoftmodem` process: 

**Command for DGX Spark**: 

```bash
sudo devlink dev reload pci/<HOST_PCI_ADDR> && sudo taskset -c "5-19" ./nr-uesoftmodem --ue-fo-compensation --thread-pool 15,16,17,18,19 --loader.ldpc.shlibversion _cuda -O ~/oai-nr-ue/nr-ue.conf --usrp-args "addr=192.168.80.251,use_dpdk=1" -C 3350010000 -r 273 --numerology 1 --band 78 --ssb 1515 --usrp-tx-thread-config 1
```

DPDK deployment have some issues currently and we need to reload the pci interface before every run. The devlink command `sudo devlink dev reload pci/<HOST_PCI_ADDR>` is doing that. 

If you need to adjust the tx/rx gains then you will need to take the parameters from previous section. 
