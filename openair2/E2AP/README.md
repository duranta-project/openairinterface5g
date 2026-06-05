<!-- SPDX-License-Identifier: CC-BY-4.0 -->


# Overview

This tutorial describes the steps of deployment 5G OAI RAN, with integrated E2 agent, and a nearRT-RIC using O-RAN compliant FlexRIC.

[[_TOC_]]

# 1. Mandatory prerequisites of FlexRIC before starting with E2 agent
* [GCC compiler](https://gitlab.eurecom.fr/mosaic5g/flexric#111-gcc-compiler)
* [(opt.) configure Wireshark](https://gitlab.eurecom.fr/mosaic5g/flexric#112-opt-wireshark)
* [Mandatory dependencies](https://gitlab.eurecom.fr/mosaic5g/flexric#121-mandatory-dependencies)


# 2. Deployment

## 2.1 OAI RAN
OAI E2 agent contains:
* RAN functions: exposure of service models capabilities 
* FlexRIC submodule: for message enc/dec

Currently available versions:
|            |E2SM-KPM v2.03|E2SM-KPM v3.00|
|:-----------|:-------------|:-------------|
| E2AP v1.01 | Y            | Y            |
| E2AP v2.03 | Y (default)  | Y            |
| E2AP v3.01 | Y            | Y            |

Note: E2SM-KPM `v2.01` is supported only in FlexRIC, but not in OAI.

### 2.1.1 Clone the OAI repository
```bash
git clone https://gitlab.eurecom.fr/oai/openairinterface5g
```

### 2.1.2 Build OAI with E2 Agent

#### Using build_oai script
```bash
cd openairinterface5g/cmake_targets/
./build_oai -I  # if you never installed OAI, use this command once before the next line
# 
./build_oai --gNB --nrUE --build-e2 --cmake-opt -DE2AP_VERSION=E2AP_VX --cmake-opt -DKPM_VERSION=KPM_VY --ninja
```
where `X`=`1`,`2`,`3`, and `Y`=`2_03`,`3_00`.

 * `-I` option is to install pre-requisites, you only need it the first time you build the softmodem or when some oai dependencies have changed.
 * `-w` option is to select the radio head support you want to include in your build. Radio head support is provided via a shared library, which is called the "oai device" The build script creates a soft link from `liboai_device.so` to the true device which will be used at run-time (here the USRP one, `liboai_usrpdevif.so`). The RF simulator is implemented as a specific device replacing RF hardware, it can be specifically built using `-w SIMU` option, but is also built during any softmodem build.
 * `--gNB` is to build the `nr-softmodem` and `nr-cuup` executables and all required shared libraries
 * `--nrUE` is to build the `nr-uesoftmodem` executable and all required shared libraries
 * `--ninja` is to use the ninja build tool, which speeds up compilation
 * `--build-e2` option is to use the E2 agent, integrated within E2 nodes (gNB-mono, DU, CU, CU-UP, CU-CP).

#### Using cmake directly
```bash
cd openairinterface5g
mkdir build && cd build
cmake .. -GNinja -DE2_AGENT=ON -DE2AP_VERSION=E2AP_VX -DKPM_VERSION=KPM_VY
ninja nr-softmodem nr-cuup nr-uesoftmodem params_libconfig rfsimulator
```
where `X`=`1`,`2`,`3`, and `Y`=`2_03`,`3_00`.


## 2.2 FlexRIC
Important to note: OAI RAN and FlexRIC must be compiled with the same E2AP and E2SM-KPM versions.

### 2.2.1 Case 1: OAI RAN and FlexRIC on the same machine
Instead of cloning the new FlexRIC repository, feel free to use FlexRIC submodule as a nearRT-RIC + xApp framework, apart from the E2 agent side. In order to achieve this, please follow the next steps:
```bash
cd openairinterface5g/openair2/E2AP/flexric
git submodule init && git submodule update  # only if OAI RAN wasn't previously compiled with --build-e2 or -DE2_AGENT options
```
and continue as described in the [build FlexRIC section](https://gitlab.eurecom.fr/mosaic5g/flexric#22-build-flexric).

### 2.2.2 Case 2: OAI RAN and FlexRIC on different machines
The [FlexRIC submodule compilation step](#221-case-1-oai-ran-and-flexric-on-the-same-machine) is **mandatory** as the OAI RAN must use the Service Models (which are built as shared libraries).
To build FlexRIC on the other machine, follow the complete [FlexRIC installation](https://gitlab.eurecom.fr/mosaic5g/flexric#2-flexric-installation) process.


# 3. Service Models available in OAI RAN

## 3.1 O-RAN
For a deeper understanding, we recommend that users of FlexRIC familiarize themselves with O-RAN WG3 specifications available at the [O-RAN specifications page](https://orandownloadsweb.azurewebsites.net/specifications).

The following specifications are recommended:
* `O-RAN.WG3.E2GAP-v02.00` - nearRT-RIC architecture & E2 General Aspects and Principles: important for a general understanding for E2
* `O-RAN.WG3.E2AP-version` - E2AP protocol description
* `O-RAN.WG3.E2SM-KPM-version` - E2SM-KPM Service Model description
* `O-RAN.WG3.E2SM-RC-v01.03` - E2SM-RC Service Model description

### 3.1.1 E2SM-KPM
As mentioned in section [2.1 OAI RAN](#21-oai-ran), we support KPM `v2.03/v3.00` which all use ASN.1 encoding.

Per O-RAN specifications, 5G measurements supported by KPM are specified in 3GPP TS 28.552.

From 3GPP TS 28.552, we support the following list:
  * `DRB.PdcpSduVolumeDL`
  * `DRB.PdcpSduVolumeUL`
  * `DRB.RlcSduDelayDl`
  * `DRB.UEThpDl`
  * `DRB.UEThpUl`
  * `RRU.PrbTotDl`
  * `RRU.PrbTotUl`

From `O-RAN.WG3.E2SM-KPM-version` specification, we implemented:
  * REPORT Service Style 4 ("Common condition-based, UE-level" - section 7.4.5) - fetch above measurements per each UE based on the common S-NSSAI `(1, 0xffffff)` condition

### 3.1.2 E2SM-RC
We support E2SM-RC `v1.03` which uses ASN.1 encoding.

From `O-RAN.WG3.E2SM-RC-v01.03` specification, we implemented:
  * REPORT Service Style 1 ("Message copy" - section 7.4.2) - aperiodic subscription for "RRC Message" and "UE ID"
  * REPORT Service Style 4 ("UE Information" - section 7.4.5) - aperiodic subscription for "UE RRC State Change"
  * CONTROL Service Style 1 ("Radio Bearer Control" - section 7.6.2) - "QoS flow mapping configuration"; please be aware that this functionality is defined as per O-RAN "To control the multiplexing of QoS flows to a DRB",
    but OAI RAN doesn't support multiple QoS flows in one DRB. Therefore, this use case was adjusted for creation of new DRB, instead of creation of new QoS flow in the existing DRB.
    More information can be found in the branch [qoe-e2](https://gitlab.eurecom.fr/oai/openairinterface5g/-/tree/qoe-e2?ref_type=heads). It's not merged due to incompliance with O-RAN specifications.
    We showcased this demo in the O-RAN F2F Osaka meeting. Please feel free to download the [demo video](https://lf-o-ran-sc.atlassian.net/wiki/download/attachments/13566077/oai-flexric-demo-ric.mp4?api=v2) and [accompanying presentation](https://lf-o-ran-sc.atlassian.net/wiki/download/attachments/13566077/oai-FlexRIC-demo.pdf?api=v2).

## 3.2 Custom Service Models
In addition, we support custom Service Models for L2/L3. Please find the KPIs for each layer in the following structs:
* MAC: `mac_ue_stats_impl_t`
* RLC: `rlc_radio_bearer_stats_t`
* PDCP: `pdcp_radio_bearer_stats_t`
* GTP: `gtp_ngu_t_stats_t`

All use plain encoding, i.e., no ASN.1, but write the binary data into network messages.

There exist two additional custom Service Models, SLICE and TC (traffic control), but they are not supported in OAI RAN. They can only be tested with [E2 agent emulators](https://gitlab.eurecom.fr/mosaic5g/flexric#4-deployment) within FlexRIC framework.

# 4. Start the process
At this point, we assume the 5G Core Network is already running in the background. For more information, please follow the [5GCN tutorial](../../doc/NR_SA_Tutorial_OAI_CN5G.md).

Optionally run Wireshark and capture E2AP traffic.

The E2 node (gNB-mono, DU, CU, CU-UP, CU-CP) configuration file must contain the `e2_agent` section. Please adjust the following example to your needs:
```bash
e2_agent = {
  near_ric_ip_addr = "127.0.0.1";
  sm_dir = "/usr/local/lib/flexric/";
}
```

* start the E2 nodes

  As per `O-RAN.WG3.E2SM-v02.00` specifications, `UE ID` (section 6.2.2.6) representation in OAI is:
  |                       | gNB-mono        | CU              | CU-CP           | CU-UP                 | DU                 |
  |:----------------------|:----------------|:----------------|:----------------|:----------------------|:-------------------|
  | CHOICE UE ID case     | GNB_UE_ID_E2SM  | GNB_UE_ID_E2SM  | GNB_UE_ID_E2SM  | GNB_CU_UP_UE_ID_E2SM  | GNB_DU_UE_ID_E2SM  |
  | AMF UE NGAP ID        | amf_ue_ngap_id  | amf_ue_ngap_id  | amf_ue_ngap_id  |                       |                    |
  | GUAMI                 | guami           | guami           | guami           |                       |                    |
  | gNB-CU UE F1AP ID     |                 | rrc_ue_id       |                 |                       | rrc_ue_id          |
  | gNB-CU-CP UE E1AP ID  |                 |                 | rrc_ue_id       | rrc_ue_id             |                    |
  | RAN UE ID             | rrc_ue_id       | rrc_ue_id       | rrc_ue_id       | rrc_ue_id             | rrc_ue_id          |

  * start the gNB-mono
    ```bash
    cd <path-to>/build
    sudo ./nr-softmodem -O <path-to>/targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.sa.band78.fr1.106PRB.usrpb210.conf --gNBs.[0].min_rxtxtime 6 --rfsim
    ```

  * if CU/DU split is used, start the gNB as follows
    ```bash
    cd <path-to>/build
    sudo ./nr-softmodem -O <path-to>/targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb-du.sa.band78.106prb.rfsim.pci0.conf --rfsim
    sudo ./nr-softmodem -O <path-to>/targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb-cu.sa.f1.conf
    ```

  * if CU-CP/CU-UP/DU split is used, start the gNB as follows
    ```bash
    cd <path-to>/build
    sudo ./nr-softmodem -O <path-to>/targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb-du.sa.band78.106prb.rfsim.pci0.conf --rfsim
    ./nr-softmodem -O <path-to>/ci-scripts/conf_files/gnb-cucp.sa.f1.conf --gNBs.[0].plmn_list.[0].mcc 001 --gNBs.[0].plmn_list.[0].mnc 01 --gNBs.[0].local_s_address "127.0.0.3" --gNBs.[0].amf_ip_address.[0].ipv4 "192.168.70.132" --gNBs.[0].E1_INTERFACE.[0].ipv4_cucp "127.0.0.3" --gNBs.[0].NETWORK_INTERFACES.GNB_IPV4_ADDRESS_FOR_NG_AMF "192.168.70.129" --e2_agent.near_ric_ip_addr "127.0.0.1" --e2_agent.sm_dir "/usr/local/lib/flexric/"
    sudo ./nr-cuup -O <path-to>/ci-scripts/conf_files/gnb-cuup.sa.f1.conf --gNBs.[0].plmn_list.[0].mcc 001 --gNBs.[0].plmn_list.[0].mnc 01 --gNBs.[0].local_s_address "127.0.0.6" --gNBs.[0].E1_INTERFACE.[0].ipv4_cucp "127.0.0.3" --gNBs.[0].E1_INTERFACE.[0].ipv4_cuup "127.0.0.6" --gNBs.[0].NETWORK_INTERFACES.GNB_IPV4_ADDRESS_FOR_NG_AMF "192.168.70.129" --gNBs.[0].NETWORK_INTERFACES.GNB_IPV4_ADDRESS_FOR_NGU "192.168.70.129" --e2_agent.near_ric_ip_addr "127.0.0.1" --e2_agent.sm_dir "/usr/local/lib/flexric/"  --rfsim 

    ```

* start the nrUE
  ```bash
  cd <path-to>/build
  # for gNB-mono
  sudo ./nr-uesoftmodem -r 106 --numerology 1 --band 78 -C 3619200000 --rfsim --uicc0.imsi 001010000000001 --rfsimulator.[0].serveraddr 127.0.0.1
  # for CU/DU and CU-CP/CU-UP/DU split
  sudo ./nr-uesoftmodem -r 106 --numerology 1 --band 78 -C 3450720000 --rfsim --uicc0.imsi 001010000000001 --rfsimulator.[0].serveraddr 127.0.0.1
  ```
  Note: [multi-UE rfsim deployment tutorial](../../doc/NR_SA_Tutorial_OAI_multi_UE.md?ref_type=heads#run-multiple-ues-in-rfsimulator)

* start the nearRT-RIC
  ```bash
  cd flexric # or openairinterface5g/openair2/E2AP/flexric
  ./build/examples/ric/nearRT-RIC
  ```

`XAPP_DURATION` environment variable overwrites the default xApp duration of 20s.
* Start different xApps
  Important to note: if no RIC INDICATION is received by any of the xApps, either the 
  * start the KPM monitor xApp - measurements stated in [3.1.1 E2SM-KPM](#311-e2sm-kpm) for each UE that matches S-NSSAI `(1, 0xffffff)` common criteria
    ```bash
    cd flexric # or openairinterface5g/openair2/E2AP/flexric
    XAPP_DURATION=30 ./build/examples/xApp/c/monitor/xapp_kpm_moni
    ```
    Note: we assume that each UE has only 1 DRB; CU-UP does not store the slices, therefore "coarse filtering" is used; **if no RIC INDICATION message received**, please check if:
          (1) UE is connected to the gNB
          (2) UE has a PDU session (LCID 4)
          (3) UE supports the slice `(1, 0xffffff)`

  * start the RC monitor xApp - aperiodic subscriptions for "UE RRC State Change", "RRC Message" copy (`RRC Reconfiguration`, `Measurement Report`, `Security Mode Complete`, `RRC Setup Complete`), and "UE ID" when `RRC Setup Complete` and/or `F1 UE Context Setup Request` detected
    ```bash
    cd flexric # or openairinterface5g/openair2/E2AP/flexric
    XAPP_DURATION=30 ./build/examples/xApp/c/monitor/xapp_rc_moni
    ```

  * start the KPM monitor and RC control xApp - this xApp is only a PoC. It collects the measurements stated in [3.1.1 E2SM-KPM](#311-e2sm-kpm) and sends the RC Control message
    for "QoS flow mapping configuration". However, no control is being done within the RAN itself. Please refer to [3.1.2 E2SM-RC](#312-e2sm-rc) for detailed explanation.
    ```bash
    cd flexric # or openairinterface5g/openair2/E2AP/flexric
    XAPP_DURATION=30 ./build/examples/xApp/c/kpm_rc/xapp_kpm_rc
    ```

  * start the (MAC + RLC + PDCP + GTP) monitor xApp
    ```bash
    cd flexric # or openairinterface5g/openair2/E2AP/flexric
    XAPP_DURATION=30 ./build/examples/xApp/c/monitor/xapp_gtp_mac_rlc_pdcp_moni
    ```
    Note: GTP is not yet implemented in CU-UP; **if no RIC INDICATION message received**, please check if:
          (1) UE is connected to the gNB
          (2) UE has a PDU session (LCID 4) - if no PDU session, only MAC/GTP stats will be sent

    * if `XAPP_MULTILANGUAGE` option is enabled, start the python xApps:
      ```bash
      XAPP_DURATION=30 python3 build/examples/xApp/python3/xapp_mac_rlc_pdcp_gtp_moni.py # only supported by the gNB-mono
      ```

The latency that you observe in your monitor xApp is the latency from the E2 Agent to the nearRT-RIC and xApp. 
Therefore, FlexRIC is well suited for use cases with ultra low-latency requirements.
Additionally, all the data received in the `xapp_gtp_mac_rlc_pdcp_moni` xApp is also written to `/tmp/xapp_db_` in case that offline data processing is wanted (e.g., Machine Learning/Artificial Intelligence applications). You can browse the data using e.g., `sqlitebrowser`.

Please note:
* KPM SM database is not been updated, therefore commented in `flexric/src/xApp/db/sqlite3/sqlite3_wrapper.c:1152`
* RC SM database is not yet implemented.

# 5. O-RAN SC nearRT-RIC interoperability

The E2AP port for OSC nearRT-RIC is 36422, but the default value in E2 agent is 36421. Before proceeding with integration, please set the `e2ap_server_port` to 36422, and recompile the OAI.

## 5.1 H release
We showcased the QoE use case during the O-RAN F2F meeting Phoenix, October 2023:
* [recorded presentation](https://zoom.us/rec/play/N5mnAQUcEVRf8HN6qLYa4k7kjNq3bK4hQiYqHGv9KUoLfcR6GHiE-GvnmAudT6xccmZSbkxxYHRwTaxk.Zi7d8Sl1kQ6Sk1SH?canPlayFromShare=true&from=share_recording_detail&continueMode=true&componentName=rec-play&originRequestUrl=https%3A%2F%2Fzoom.us%2Frec%2Fshare%2FwiYXulPlAqIIDY_vLPQSGqYIj-e5Ef_UCxveMjrDNGgXLLvEcDF4v1cmVBe8imb4.WPi-DA_dfPDBQ0FH). This use case is shown after 4 minutes (04:00).
* [accompanying flyer](https://openairinterface.org/wp-content/uploads/2023/10/demo2-phoenix.pdf)

The same was shown in the following events:
* [Joint OSC/OSFG-OAI Workshop](https://openairinterface.org/joint-osc-oai-workshop-end-to-end-open-source-reference-designs-for-o-ran/)

In order to reproduce this testbed, please follow the [O-RAN SC nearRT-RIC installation guide](https://lf-o-ran-sc.atlassian.net/wiki/spaces/GS/overview), and the [kpm-rc-xapp](https://github.com/mirazabal/kpm_rc-xapp) xApp used. Please note that we cannot give support for the O-RAN SC nearRT-RIC.

## 5.2 J release
The OAI E2 Agent has been successfully integrated with the J release of the O-RAN OSC nearRT-RIC, leveraging the xDevSM framework developed by the MMWG at the University of Bologna and the WIoT at Northeastern University.

xDevSM framework uses KPM SM (`libkpm_sm.so`) and RC SM (`librc_sm.so`) libraries from FlexRIC. Therefore, only E2AP is validated between OAI E2 Agent and OSC nearRT-RIC.

Please follow [the xDevSM framework tutorial](https://openrangym.com/tutorials/xdevsm-tutorial) in order to reproduce this testbed, and try [the example xApps](https://github.com/wineslab/xDevSM-xapps-examples) built using this framework.
