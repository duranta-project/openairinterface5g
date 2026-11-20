<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# Support Hardware and Operating System

This document contains the list of CPUs, RAM, NICs, accelerators and operating
systems which are used for testing in the OAI lab.

The below hardware can support USRPs, 7.2 FR1 or FR2 O-RUs. 

**Table of Contents**

[[_TOC_]]

## List of Tested/Supported CPUs 

|Hardware                                  |Remark for performance testing           |
|------------------------------------------|-----------------------------------------|
|Intel(R) Xeon(R) Gold 6354 36-Core        |With or without ACC100                   |
|Intel(R) Xeon(R) Gold 6433N 32-Core       |Use VRAN Boost                           |
|INTEL(R) XEON(R) GOLD 6542Y 24-core       |                                         |
|AMD EPYC 9374F 32-Core Processor          |With or without AMD Xilinx T2 Telco Card |
|AMD EPYC 8534P 64-Core Processor          |With AMD Xilinx T2 Telco Card            |
|AMD EPYC 9575F 64-Core Processor          |With or without AMD Xilinx T2 Telco Card |
|SuperMicro ARS-111GL-NHR, ARM Neoverse-V2 |With or without LDPC GPU offload         |
|DGX Spark, GB10 Dell, HPE, Nvidia         |With LPDC GPU offload                    |
|AMD Ryzen 9 PRO 7945 12-Core Processor    |                                         |

## List of Tested/Supported SFP NICs

|Network Interface Cards    |
|---------------------------|
|Intel E810-C               |
|Intel X710                 |
|Intel XXV710               |
|Intel E810-XXV             |
|Mellanox ConnectX-7        |
|Mellanox ConnectX-6        |

## List of Tested/Supported L1 Look-aside Accelerators for OAI physical layer

|Supported L1 Accelerators  |
|---------------------------|
|Intel ACC100               |
|Intel ACC200               |
|AMD Xilinx T2 Telco Card   |
|ORS Aurora                 |

## List of supported operating systems

|Operating System           |
|---------------------------|
|Ubuntu 22.04/24.04/26.04   |
|Red Hat 9.X                |
|Rocky   9.X                |
|Fedora 43/44               |
|Debian 11/12               |

## Notes

1. We recommend DDR4 or DDR5 RAMs, though most of the new systems are coming with DDR5 RAMs
2. We recommend to use M2 NVMe SSD for storage or SSD with high IOPS
