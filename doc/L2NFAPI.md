<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# L2 nFAPI Simulator Usage

## 4G L2 nFAPI simulator

This simulator allows to test L2 and above Layers using the nFAPI interface.

The UE executable is able to "simulate" multiple UEs in order to stimulate the scheduler in the eNB.

> **This simulator is available starting the `v1.0.0` release on the `master` branch.**

> **2022/03/08: CAUTION, THIS TUTORIAL IS NO LONGER VALID on the `develop` branch after the `2022.w01` tag.**

> **2022/03/08: CAUTION, THE LAST VALID TAG on `develop` branch is `2021.w51_c`.**

Currently the Continuous Integration process is validating this simulator the following way:

*  the LTE modem executable is run on one host (in our CI deployment it is a **Xenial Virtual Machine**)
*  the UE(s) modem executable is run on another host (in our CI deployment it is also a **Xenial Virtual Machine**)
*  We are testing:
   *   in S1 mode (ie we are connected to a 3rd-party EPC)
   *   in noS1 mode (no need for an EPC)

Normally it should be fine to run both executables on the same host using the `loopback` interface to communicate. **But we are not guaranting it**

![L2-sim-single-server-deployment](https://raw.githubusercontent.com/duranta-project/assets/92564035bd1a5764d6ecb566ec673200ceefef8e/openairinterface5g/images/L2-sim-single-server-deployment.png)

1. [With S1 -- eNB and UE on 2 hosts](L2NFAPI_S1.md)
2. [No S1 -- eNB and UE on 2 hosts](L2NFAPI_NOS1.md)


> **2022/03/08: Starting the `2022.w01` tag on the `develop` branch, the L2 nFAPI simulation is using a proxy.**

### EpiSci's Documentation
A more detailed tutorial is available on the [EpiSci GitHub Repository](https://github.com/EpiSci/oai-lte-5g-multi-ue-proxy#readme).

Since 5G code has been removed, this proxy allows to perform L2 nFAPI simulator for LTE only.
