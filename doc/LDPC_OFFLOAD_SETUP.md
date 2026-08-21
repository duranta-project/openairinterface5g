<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# OAI LDPC offload (O-RAN AAL/DPDK BBDEV)

**Table of Contents**

[[_TOC_]]

This documentation describes the integration of LDPC coding for lookaside acceleration using O-RAN AAL/DPDK BBDEV in OAI, along with its usage.
For details on the implementation, please consult the [developer notes](../openair1/PHY/CODING/nrLDPC_coding/nrLDPC_coding_aal/README.md).

## Requirements

In principle, any lookaside LDPC accelerator supporting the O-RAN AAL/DPDK BBDEV should work.
However, the current implementation has only been validated for the AMD T2 Telco Accelerator Card, Intel ACC100, and Intel ACC200 (VRB1).
Therefore, your mileage may vary when using other BBDEV devices as there may be some hardware-specific changes required -- contributions are welcome!

### DPDK Version Requirements

The following DPDK versions are supported:
- For the AMD T2 Telco Accelerator Card, DPDK20.11+ is supported.
- As for the Intel ACC100/ACC200, only DPDK22.11+ is supported.

### Tested Devices/ DPDK versions

#### AMD T2 Telco Accelerator Card

- DPDK20.11.9
- DPDK22.11
- DPDK22.11.3

> [!NOTE]
> FPGA bitstream image and the corresponding patch file (e.g., `AMD-T2-SDFEC_25-03-1.patch` for DPDK22.11/DPDK22.11.3) required.

#### Intel ACC100

- DPDK22.11.7*.
- DPDK23.11.3*.
- DPDK24.11.2.

> [!NOTE]
> [Patch](https://github.com/DPDK/dpdk/commit/fdde63a1dfc129d0a510a831aa98253b36a2a1cd) required for pre-DPDK24.11 versions when using the Intel ACC100.

#### Intel ACC200 (also known as VRB1)
- DPDK22.11.7.
- DPDK23.11.3.
- DPDK24.11.2.

## System Setup
### DPDK installation

> [!IMPORTANT]
> - If you are using the AMD T2 Telco Accelerator Card, you will need to apply the vendor-supplied patches before compiling DPDK.
> - If you are using the Intel ACC100, you will need to [patch](https://github.com/DPDK/dpdk/commit/fdde63a1dfc129d0a510a831aa98253b36a2a1cd) the ACC100's driver if you are using DPDK22.11 or DPDK23.11. 


Refer to the [guide](./ORAN_FHI7.2_Tutorial.md?ref_type=heads#dpdk-data-plane-development-kit) to install, and then validate your DPDK installation. Ensure that you follow the DPDK version requirements for your specific device.

<details open> 
<summary> Notes on DPDK patching/installation for AMD T2 Telco Accelerator Card. </summary>

> The following instructions apply to `AMD-T2-SDFEC_25-03-1.patch`, compatible with DPDK22.11 and DPDK.22.11.3.
> For older patches:
> - `ACCL_BBDEV_DPDK20.11.3_ldpc_3.1.918.patch`, refer to the T2 documentation in [2026.w28](https://github.com/duranta-project/openairinterface5g/releases/tag/2026.w28).
> - `ACL_BBDEV_DPDK20.11.3_BL_1006_build_1105_dev_branch_MCT_optimisations_1106_physical_std.patch`, refer to the T2 documentation in [2023.w48](https://github.com/duranta-project/openairinterface5g/releases/tag/2023.w48).

```bash
# Get DPDK source code for DPDK22.11.3
wget http://fast.dpdk.org/rel/dpdk-22.11.3.tar.xz
tar xvf dpdk-22.11.3.tar.xz && cd dpdk-stable-22.11.3
git apply ~/AMD-T2-SDFEC_25-03-1.patch
# Install DPDK
meson setup build
# Use "meson setup --prefix=/opt/dpdk-t2 build" for installation into non-default installation path
ninja -C build
sudo ninja install
sudo ldconfig
```

</details>

### System configuration

#### Setting up Hugepages

First, we must setup hugepages on the system.
In our setup, we setup 16 of the 1G hugepages.
Apart from 1G, 2MB hugepages works too, but make sure to allocate a sufficient number of them.

```
# sudo dpdk-hugepages.py -p 1G --setup 16G
```

#### Locating the Accelerator

Next, we check whether our system can detect our accelerator using `dpdk-devbind.py`.
You should see Baseband devices detected by DPDK, as follows:
```
# sudo dpdk-devbind.py -s
...
Baseband devices using DPDK-compatible driver
=============================================
0000:f7:00.0 'Device 57c0' unused=vfio-pci
...
```

As you can see here, our Intel ACC200 has the address of `0000:f7:00.0`.
Depending on the accelerator you are using, the address may vary.

#### Loading VFIO-PCI and enabling SR-IOV
Following, make sure to load the `vfio-pci` kernel modules and ensure that SR-IOV is enabled.

```
# sudo modprobe vfio-pci enable_sriov=1 disable_idle_d3=1
```

#### Binding the Accelerator with `vfio-pci`

Lastly, we bind our accelerator with the `vfio-pci` driver.
```
# sudo dpdk-devbind.py --bind=vfio-pci 0000:f7:00.0
```

> [!NOTE]
> For the AMD T2 Telco Accelerator Card, we can use this device directly in a single process.  
> If you use the T2 Telco Accelerator Card in more than one process in parallel, read the following hidden note.  
> If you use an Intel vRAN accelerator, read the following section.

<details> 
<summary> Notes on enabling Virtual Functions (VFs) for the AMD T2 Telco Accelerator Card. </summary>

This section explains how to enable the VFs in order to use the T2 Telco Accelerator Card in multiple processes or containers.
This feature is available only for **DPDK 20.11.9** with patch `ACCL_BBDEV_DPDK20.11.3_ldpc_3.2.patch` and the corresponding board firmware.

##### Clone and Build the `igb_uio` kernel module

```bash
git clone http://dpdk.org/git/dpdk-kmods ~/dpdk-kmods
cd ~/dpdk-kmods/linux/igb_uio
make
```

##### Insert the `igb_uio` kernel module

Instructions below this line should be followed upon each system restart.

```bash
cd ~/dpdk-kmods/linux/igb_uio
sudo modprobe uio
sudo insmod igb_uio.ko 
lsmod | grep uio
```

##### Bind the devices

First bind the Physical Function to `igb_uio`.

```bash
sudo ~/dpdk-stable-20.11.9/usertools/dpdk-devbind.py -b igb_uio 0000:f7:00.0
```
Then create the VFs, there are 2 in this example but there can be up to 16 VFs.

```bash
echo 2 | sudo tee /sys/bus/pci/devices/0000\:f7\:00.0/max_vfs
```

Finally, bind the VFs to `vfio-pci`.

```bash
sudo ~/dpdk-stable-20.11.9/usertools/dpdk-devbind.py -b vfio-pci 0000:f7:00.4
sudo ~/dpdk-stable-20.11.9/usertools/dpdk-devbind.py -b vfio-pci 0000:f7:00.5
```

##### Run the dpdk-admin app

This app was built with DPDK and is located in the `app` directory in the build directory.

**IMPORTANT:**
- Make sure no other `dpdk-admin` app is running and no running task are yet trying to use the VFs.  
  Otherwise the system may get in a deadlock.
- Keep the `dpdk-admin` command running while using the VFs.
  Do not stop it while any process is still using any VF.
- `-l <cpu>` corresponds to the list of cores used by the DPDK threads of the admin app.  
  It is isolated and exclusively reserved to the dpdk-admin app.
- Pass the Physical Function address to option `-a`.  

```bash
sudo ~/dpdk-stable-20.11.9/build/app/dpdk-admin -a 0000:f7:00.0 --file-prefix PF -l 7 2>&1
```

</details>

#### Additional Steps for Intel vRAN Accelerators

> [!IMPORTANT]
> - Currently, we only support using the Virtual Functions (VFs) of the Intel vRAN accelerators, but not the Physical Function (PF).
> - One key advantage of using VFs is that this allows us to share the accelerator with other DU instances on the same machine, which is common in practice.

If you are using an Intel vRAN accelerator, you will need to use the [pf_bb_config](https://github.com/intel/pf-bb-config) tool to configure the accelerator beforehand. 

##### pf_bb_config
For more details, please consult the `pf_bb_config` README.

```
# git clone https://github.com/intel/pf-bb-config
# cd ~/pf-bb-config
# ./build.sh
```
This clones and builds the `pf_bb_config` binary.

Next, we show an example for the Intel ACC200.
We use an existing configuration located at `./vrb1/vrb1_config_16vf.cfg`.

Here, it is necessary to specify a VFIO token (in this case, we use the UUID `00112233-4455-6677-8899-aabbccddeeff`).
Note that in practice, a random UUID should be used.
```
# sudo ./pf_bb_config VRB1 -v 00112233-4455-6677-8899-aabbccddeeff -c vrb1/vrb1_config_16vf.cfg
== pf_bb_config Version v25.01-0-g812e032 ==
VRB1 PF [0000:f7:00.0] configuration complete!
Log file = /var/log/pf_bb_cfg_0000:f7:00.0.log
```

##### Creating VFs

Finally, we create the VF(s) for our accelerator. 
In this example, we only create one SR-IOV VF.
```
# echo 1 | sudo tee /sys/bus/pci/devices/0000:f7:00.0/sriov_numvfs
```

If you encounter any error when creating the VF(s), e.g., `tee: '/sys/bus/pci/devices/0000:f7:00.0/sriov_numvfs': No such file or directory`, then try enabling SR-IOV again.
```
# echo 1 | sudo tee /sys/module/vfio_pci/parameters/enable_sriov
```

After you have successfully created the VF, you should see an additional baseband device, in our case, it is `0000:f7:00.1`. 
We will use this device with OAI later.
The newly created VF should also be using the same `vfio-pci` driver as the PF, if it is not, you will need to do a `dpdk-devbind.py` to bind it with `vfio-pci`. 
```
# sudo dpdk-devbind.py -s
...
Baseband devices using DPDK-compatible driver
=============================================
0000:f7:00.0 'Device 57c0' drv=vfio-pci unused=
0000:f7:00.1 'Device 57c0' drv=vfio-pci unused=
...
```

## Building OAI with ORAN-AAL
OTA deployment is precisely described in the following tutorial:
- [NR_SA_Tutorial_COTS_UE](./NR_SA_Tutorial_COTS_UE.md)
Instead of section *3.2 Build OAI gNB* from the tutorial, run the following commands:

```bash
# Get openairinterface5g source code
git clone https://github.com/duranta-project/openairinterface5g.git ~/openairinterface5g
cd ~/openairinterface5g
git checkout develop

# Install OAI dependencies
cd ~/openairinterface5g/cmake_targets
./build_oai -I

# Build OAI gNB
cd ~/openairinterface5g
source oaienv
cd cmake_targets
./build_oai -w USRP --ninja --gNB -P --build-lib "ldpc_aal" -C
```

A shared object file `libldpc_aal.so` will be created during the compilation.
This object is conditionally compiled. 
The selection of the library to compile is done using `--build-lib ldpc_aal`.

> [!NOTE]
> The required DPDK poll mode driver has to be present on the host machine and required DPDK version has to be installed on the host, prior to building OAI.

## O-RAN AAL DPDK EAL parameters
To configure O-RAN AAL/DPDK BBDEV, you can set the following parameters via the command line of PHY simulators or nr-softmodem:

> [!NOTE]
> The group parameter name has been renamed from `nrLDPC_coding_t2` to
> `nrLDPC_coding_aal` to better reflect that it is a generic AAL accelerator
> card.

- `nrLDPC_coding_aal.dpdk_dev` - **mandatory** parameter, specifies the PCI address of our accelerator. It must follow the format `WWWW:XX:YY.Z`.

- `nrLDPC_coding_aal.dpdk_core_list` - **mandatory** parameter, specifies the CPU cores assigned to DPDK . 
Ensure that the CPU cores specified in `nrLDPC_coding_aal.dpdk_core_list` are available and not used by other processes to avoid conflicts.

- `nrLDPC_coding_aal.dpdk_prefix` - optional parameter, DPDK shared data file prefix, by default set to *b6*.

- `nrLDPC_coding_aal.vfio_vf_token` - optional parameter, VFIO token set for the VF, if applicable.

- `nrLDPC_coding_aal.num_harq_codeblock` - optional parameter, size of the HARQ buffer in terms of the number of 32kB blocks, by default set to *512* (maximum for the T2; as for the ACCs, this can be further increased).

- `nrLDPC_coding_aal.is_t2` - optional parameter, set this to 1 when using the AMD T2 Telco Accelerator Card.

> [!NOTE]
> These parameters can also be provided in a configuration file.

Example for the ACC200:
```
nrLDPC_coding_aal : {
  dpdk_dev : "0000:f7:00.1";
  dpdk_core_list : "14-15";
  vfio_vf_token: "00112233-4455-6677-8899-aabbccddeeff";
};

loader : {
  ldpc : {
    shlibversion : "_aal";
  };
};
```

## Running OAI with O-RAN AAL

In general, to offload of the channel coding to the LDPC accelerator, we use use the `--loader.ldpc.shlibversion _aal` option.
Reminder, if you are using the Xilinx T2 card, make sure to set `--nrLDPC_coding_aal.is_t2 1`.

### 5G PHY simulators

#### nr_ulsim

Example command:
```bash
cd ~/openairinterface5g
source oaienv
cd cmake_targets/ran_build/build
sudo ./nr_ulsim -n100 -s20 -m20 -r273 -R273 --loader.ldpc.shlibversion _aal --nrLDPC_coding_aal.dpdk_dev 0000:f7:00.1 --nrLDPC_coding_aal.dpdk_core_list 0-1 --nrLDPC_coding_aal.vfio_vf_token 00112233-4455-6677-8899-aabbccddeeff
```
#### nr_dlsim

Example command:
```bash
cd ~/openairinterface5g
source oaienv
cd cmake_targets/ran_build/build
sudo ./nr_dlsim -n300 -s30 -R 106 -e 27 --loader.ldpc.shlibversion _aal --nrLDPC_coding_aal.dpdk_dev 0000:f7:00.1 --nrLDPC_coding_aal.dpdk_core_list 0-1 --nrLDPC_coding_aal.vfio_vf_token 00112233-4455-6677-8899-aabbccddeeff
```

### OTA test

#### Running OAI gNB with USRP B210/FHI72

When running the gNB **with FHI 7.2**, it is not necessary to provide the `--nrLDPC_coding_aal.dpdk_core_list` argument
since the core list specified for FHI 7.2 will be used for DPDK.
If it is provided, the AAL core list wil be ignored.  

> [!NOTE]
> Ensure that the xRAN library is built using the same DPDK version as the accelerator poll-mode driver.

Example command:
```bash
cd ~/openairinterface5g
source oaienv
cd cmake_targets/ran_build/build
sudo ./nr-softmodem -O ~/gnb.conf --loader.ldpc.shlibversion _aal --nrLDPC_coding_aal.dpdk_dev 0000:f7:00.1 --nrLDPC_coding_aal.dpdk_core_list 14-15 --nrLDPC_coding_aal.vfio_vf_token 00112233-4455-6677-8899-aabbccddeeff
```

## Known Issue(s)

### Potential Low Throughput

The current implementation has been tested to work in an end-to-end setup and is functional. 
However, depending on the accelerator in use,
there are still opportunities for optimization, particularly in LDPC decoding performance, which is an area of ongoing improvement. 
As such, downlink/uplink throughput may be suboptimal with the default configurations, but enhancements are actively being explored.

For example, to achieve better E2E performance with the current implementation with Intel ACC 100 and 200 (vRAN Boost),
we recommend the following adjustments:
1. Increasing the number of LDPC decoding iterations of the L1, e.g., `max_ldpc_iterations` to 200.
2. Increasing the BLER targets of the MAC scheduler. 

Example configuration snippet:
```
...
MACRLCs = (
{
  tr_s_preference             = "local_L1";
  tr_n_preference             = "local_RRC";
  pusch_TargetSNRx10          = 180;
  pucch_TargetSNRx10          = 220;
  dl_bler_target              = .25;
  ul_bler_target              = .25;
  pusch_FailureThres          = 1000;
  ul_max_mcs                  = 28;

}
);

L1s = (
{
  tr_n_preference       = "local_mac";
  prach_dtx_threshold   = 120;
  pucch0_dtx_threshold  = 100;
  ofdm_offset_divisor   = 8; #set this to UINT_MAX for offset 0
  max_ldpc_iterations = 200;
}
);
...
```
