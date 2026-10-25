<!-- SPDX-License-Identifier: CC-BY-4.0 -->

This document outlines some information specific to system tuning for
running all executables (`nr-softmodem`, `lte-softmodem`, `nr-uesoftmodem`,
`lte-uesoftmodem`, `nr-cuup`, called softmodems for short in the following).
Also, it explains which Linux capabilities are required to run, and how to run
without sudo.

[TOC]

## Performance Tuning

Please also refer to the [advanced configuration in the
tutorial](NR_SA_Tutorial_COTS_UE.md#6-advanced-configurations-optional), which
groups many tips and tricks.

### CPU

OAI used to try to set the minimum CPU-DMA latency to 2 us by writing to
`/dev/cpu_dma_latency`. However, it is unclear if this has a significant
effect. Further, in containerized workloads, it might not be possible to set
this at all. We assume the user of OAI sets this value before starting OAI.
See the [Linux kernel
documentation](https://www.kernel.org/doc/html/latest/admin-guide/pm/cpuidle.html)
for more information.

OAI used to try to set the minimum frequency of cores running OAI to the
maximum frequency. We assume that the user sets CPU frequency policies
accordingly; in fact, tutorials generally suggest to either set a performance
CPU governor, or disable any sleep states at all. Hence, setting these
low-level parameters seems useless, and we assume the user of OAI disables
sleep states before starting OAI.

To disable all sleep states, simply run the following:
```
sudo cpupower idle-set -D0
```
Sometimes, dependencies might not be installed, in which case you should
install what `cpupower` asks you to install. Note that this is not persistent.

See the [Linux kernel
documentation](https://www.kernel.org/doc/html/latest/admin-guide/pm/cpufreq.html)
for more information.

You can disable hyper-threading in the BIOS.

You can disable KPTI Protections for Spectre/Meltdown for more performance.
**This is a security risk.** Add `mitigations=off nosmt` in your grub config and
update grub.

### Ethernet-based Radios

For ethernet-based radios, such as AW2S, some USRPs, and 7.2 radios, increase
the ringbuffers to a maximum. Read on interface `<fronthaul-interface-name>`
using option `-g`, then set it with `-G`:

```
ethtool -g <fronthaul-interface-name>
ethtool -G <fronthaul-interface-name> rx <maximum-rx-value> tx <maixmum-tx-value>
```

Also, you can increase the kernel's default and maximum read and write socket
buffer sizes to a high values, e.g., 134217728:

```
sudo sysctl -n -e -q -w net.core.rmem_default=134217728
sudo sysctl -n -e -q -w net.core.rmem_max=134217728
sudo sysctl -n -e -q -w net.core.wmem_default=134217728
sudo sysctl -n -e -q -w net.core.wmem_max=134217728
```

### System tuning
In order to get an optimal real-time behavior, a few tunings can be performed on the host system:
- The use of isolated cores for the softmodem prevents competitions on the usage of core between the softmodem and other processes.
  Core isolation is enabled through the kernel command line.  
  **Warning: modifying the kernel command line can harm the OS behavior. Proceed with caution.**  
  Refer to the [OAI 7.2 Fronthaul Interface Tutorial](./ORAN_FHI7.2_Tutorial.md) for examples.

### Softmodem tuning
The way the NR softmodem uses the computing ressource can be configured.
It can have a significant effect on the performance and real-time behavior:
- The L1 TX and L1 RX threads are the two main threads
  executing the L1 RX and L1 TX pipelines.  
  These threads are ideally assigned to two dedicated cores.
  To be dedicated, the cores should be isolated in the
  kernel parameters and not be assigned elsewhere.  
  They can be assigned to specified cores with options
  `--L1s.[0].L1_tx_thread_core` and `--L1s.[0].L1_rx_thread_core`
  followed by a core id.
- The thread pool is a group of processor cores over which
  some baseband processing worker cores execute.  
  It is configured by providing a list of core ids
  after option `--thread-pool`.  
  `-1` can also be passed instead of a core id
  in order to use a floating core.  
  By default, the thread pool is 8 floating cores.
- PDSCH generation (i.e., layer mapping and precoding) is by default executed
  in the L1 TX thread but can be multithreaded using the thread pool.  
  This is enabled by option `--L1s.[0].L1_num_tx_sym_per_thread` followed
  by the number of symbols that should be processed in each thread.  
  This option can also be set in the gNB configuration file in field
  `L1_num_tx_sym_per_thread` in the `L1s` section.

### Workarounds
If the real-time performance remains bad after tuning the system and softmodem,
some workarounds allow to lower the computing demand
at the cost of lower network performance:
- If you get real-time problems on heavy UL traffic,
  reduce the maximum UL MCS using an additional
  command-line switch: `--MACRLCs.[0].ul_max_mcs 14`.  
  This comes at the cost of a lower spectral efficiency
  (i.e., less data for the same radio resource).
- You can also reduce the number of LDPC decoder iterations,
  which will make the LDPC decoder take less time:
  `--L1s.[0].max_ldpc_iterations 4`.  
  The default number of LDPC iterations is 8.
  Lowering the number of iteration comes at the cost
  of more unsuccessful transmissions.  
  OAI offers multiple implementation of LDPC coding,
  including offloading to an accelerator,
  the number of LDPC iteration should be chosen accordingly.

### Known hardware behaviors
Here is a **non-exhaustive** list of known behaviors related to hardware architecture:
- On some AMD EPYC series processors with Zen architecture
  (at least every Zen4, Zen4c, Zen5 and Zen5c based processors
  experience this behavior),  
  the processor is made of multiple dies holding one or multiple
  core complexes which are groups of cores with an L3 cache.  
  This means that cores from different core complexes
  do not share the same L3 cache and communication between
  these cores implies inter L3 cache communication  
  within a die or, even worse, between dies,
  which has a cost in term of latency.  
  Depending on the system configuration,
  the NUMA topology may reflect this physical topology,
  which can induce even further latency for
  inter core complex communication.  
  The softmodem is sensitive to this latency and its performance
  can be harmed if it uses cores across the border of dies or core complexes,  
  especially when multithreading of PDSCH generation is enabled
  (argument of `--L1s.[0].L1_num_tx_sym_per_thread` is superior to 0).  
  **Solution**: We recommend to use only one core complex
  or one die for allocating cores to nr-softmodem process.
- NUMA architecture: Make sure you don't assign cores to nr-softmodem from
  different numa nodes. Cores from different numa cores can induce latency.
  It is preferred to use the cores from numa node
  which is used by the Fronthaul NIC.

**Example**: How to assign cores to `nr-softmodem` process for gNB/DU connected to an o-ran 7.2 O-RU on EPYC 9575F (64 Zen5)
assuming that we have configured `L1s.[0].L1_num_tx_sym_per_thread = 1`

There are 8 cores per core complex and one core complex per die. You can visulize this topology using 

```bash
# Ubuntu 25.04
for X in $(seq 0 63); do echo -n "cpu$X -> die "; cat /sys/devices/system/cpu/cpu$X/topology/die_id; done
```
   
A 100MHz 4x4 FR1 gNB/DU with FHI 7.2 can be executed with full capabilities (4DL and 2UL layers)
on a single core complex (cpus 0-7 in this example) by folowing the instructions of the
[OAI 7.2 Fronthaul Interface Tutorial](./ORAN_FHI7.2_Tutorial.md)
with the following core assignment:

- `L1s.[0].L1_tx_thread_core = 0`
- `L1s.[0].L1_rx_thread_core = 1`
- `fhi_72.system_core = 3`
- `fhi_72.io_core = 4`
- `fhi_72.worker_cores.[0] = 5` (minimum 1 core)
- `thread-pool: 2,3,6,7`

## Capabilities

Historically, all softmodems are executed as `root`, typically using `sudo`.
This remains a possibility, but we do not recommend it due to security
considerations. Rather, consider giving specific capabilities as outlined
below. Read `capabilities(7)` (`man 7 capabilities`) for more information on
each of the below capabilities.

> Note that we tested this using 5G executables; 4G should work, but have not
been tested as extensively. If in doubt, run eNB/lteUE using `sudo`. The below
comments on capabilities apply in general as well; however, 4G executable might
not warn about missing capabilities or just fail.

Refer to any of the docker-compose files under `ci-scripts/` to see how to give
capabilities in docker. If you run from source, you can use `setcap` to mark a
process to run with specific capabilities. For instance, you can add the "general
capabilities" as described further below on the files like this:

```
sudo setcap cap_sys_nice+ep ./nr-softmodem
sudo setcap cap_ipc_lock+ep ./nr-softmodem
sudo setcap cap_sys_nice+ep ./nr-uesoftmodem
sudo setcap cap_ipc_lock+ep ./nr-uesoftmodem
sudo setcap cap_net_admin+ep ./nr-uesoftmodem
```

To make only temporary changes to capabilities, use `capsh`. It needs to be
started with all capabilities, so needs `sudo`. To drop all capabilities,
issue:
```
sudo -E capsh --drop="cap_sys_nice,cap_chown,cap_dac_read_search,cap_fowner,cap_fsetid,\
cap_kill,cap_setgid,cap_setuid,cap_setpcap,cap_linux_immutable,cap_net_bind_service,\
cap_net_broadcast,cap_net_admin,cap_net_raw,cap_ipc_lock,cap_ipc_owner,cap_sys_module,\
cap_sys_rawio,cap_sys_chroot,cap_sys_ptrace,cap_sys_pacct,cap_sys_admin,cap_sys_boot,\
cap_sys_resource,cap_sys_time,cap_sys_tty_config,cap_mknod,cap_lease,cap_audit_write,\
cap_audit_control,cap_setfcap,cap_mac_override,cap_mac_admin,cap_syslog,cap_wake_alarm,\
cap_block_suspend,cap_audit_read,cap_perfmon,cap_bpf,cap_checkpoint_restore" --print -- \
  -c "/absolute/path/to/nr-softmodem -O /absolute/path/to/config.conf"
```

(For readability, the command has been separated onto multiple lines through
`\`). To run with `SYS_NICE`, remove the first capability (`cap_sys_nice`)
from the list of dropped capabilities.

### General capabilities

- `SYS_NICE`: required by all softmodems to assign a higher priority to
  threads to increase the likelihood that the Linux scheduler schedules a
  thread for CPU time. Also, in some configurations, CPU pinning is employed to
  ensure consistent performance, also known as setting a CPU affinity.

  This capability is necessary by default when running any softmodem, for
  setting real-time requirements on processing threads. To allow to run without
  these requirements, the softmodems check if `SYS_NICE` is available, and
  skips any thread priority and affinity settings if the capability is not
  available. This allows to run any softmodem without root privileges in RFsim;
  you can see this by either observing a corresponding warning at the beginning
  of the execution, or the fact that no affinity/default priority is set for
  new threads.

- `IPC_LOCK`: OAI tries to lock all its virtual address space into RAM,
  preventing that memory from being paged to the swap area. If this capability
  is not present, a warning is printed.

- `NET_ADMIN`: Required at the UE to open the interface `oaitun_ue1` or eNB/gNB
  in noS1 mode. 5G executables will throw an error if they cannot create or
  modify an interface, but will continue running. It is therefore possible to
  run without this capability, but you cannot inject any traffic into the
  system. 4G executables might need this requirement, and possibly fail.

### Capabilities with DPDK

Additionally to the "general capabilities" above, you need these capabilities
when running with 7.2 fronthaul, which uses the xran library with a dependency
on DPDK:

- `IPC_LOCK` (becomes mandatory with DPDK)
- `SYS_RESOURCE`
- `NET_RAW`
- `SYS_ADMIN`: This is required by DPDK when using IOVA PA (Physical Address)
   mode to read `/proc/self/pagemaps`. However, if DPDK EAL is configured to use
   IOVA VA (Virtual Address) mode, this capability is no longer required.

### Capabilities with UHD

You don't need any additional capabilities for UHD beyond the "general
capabilities" for performance outlined above. Make sure that the USB device(s)
are readable and writable by the current user (e.g., `uhd_usrp_probe` can be
executed by a non-root user).

### Capabilities with AW2S

You don't need any additional capabilities for AW2S beyond the "general
capabilities" for performance outlined above.

### Other radios

Other radios have not been tested. If they do not work without additional
capabilities beyond the "general capabilities", please file a bug report.
