<!-- SPDX-License-Identifier: CC-BY-4.0 -->

<h1 align="center">
    <a href="https://lfnetworking.org/projects/duranta/"><img src="https://raw.githubusercontent.com/duranta-project/governance/main/logos/Duranta-Logo-Color.png" alt="Duranta OAI" width="550"></a>
</h1>

<p align="center">
    <a href="https://github.com/duranta-project/openairinterface5g/releases/tag/v3.0.0"><img src="https://img.shields.io/badge/release-v3.0.0-rebeccapurple?style=flat-square&logo=github" alt="Release v3.0.0"></a>
    <a href="https://github.com/duranta-project/openairinterface5g/blob/develop/LICENSE"><img src="https://img.shields.io/badge/license-CSSL--v1.0-rebeccapurple" alt="License"></a>
</p>

<p align="center">
    <a href="https://releases.ubuntu.com/22.04/"><img src="https://img.shields.io/badge/Ubuntu-22.04-E95420?logo=ubuntu&logoColor=white" alt="Supported OS Ubuntu 22"></a>
    <a href="https://releases.ubuntu.com/24.04/"><img src="https://img.shields.io/badge/Ubuntu-24.04-E95420?logo=ubuntu&logoColor=white" alt="Supported OS Ubuntu 24"></a>
    <a href="https://releases.ubuntu.com/26.04/"><img src="https://img.shields.io/badge/Ubuntu-26.04-E95420?logo=ubuntu&logoColor=white" alt="Supported OS Ubuntu 26"></a>
    <a href="https://www.redhat.com/en/technologies/linux-platforms/enterprise-linux"><img src="https://img.shields.io/badge/RHEL-9-EE0000?logo=redhat&logoColor=white" alt="Supported OS RHEL9"></a>
    <a href="https://getfedora.org/en/workstation/"><img src="https://img.shields.io/badge/Fedora-44-51A2DA?logo=fedora&logoColor=white" alt="Supported OS Fedora 44"></a>
</p>

<p align="center">
    <a href="https://jenkins-oai.eurecom.fr/job/RAN-Ubuntu-Image-Builder/"><img src="https://img.shields.io/jenkins/build?jobUrl=https%3A%2F%2Fjenkins-oai.eurecom.fr%2Fjob%2FRAN-Ubuntu-Image-Builder%2F&label=build-Ubuntu-x86%20Images"></a>
    <a href="https://jenkins-oai.eurecom.fr/job/RAN-RHEL-Cluster-Image-Builder/"><img src="https://img.shields.io/jenkins/build?jobUrl=https%3A%2F%2Fjenkins-oai.eurecom.fr%2Fjob%2FRAN-RHEL-Cluster-Image-Builder%2F&label=build-RHEL-Cluster%20Images"></a>
    <a href="https://jenkins-oai.eurecom.fr/job/RAN-Ubuntu-ARM-Image-Builder/"><img src="https://img.shields.io/jenkins/build?jobUrl=https%3A%2F%2Fjenkins-oai.eurecom.fr%2Fjob%2FRAN-Ubuntu-ARM-Image-Builder%2F&label=build-Ubuntu-ARM%20Images"></a>
    <a href="https://jenkins-oai.eurecom.fr/job/RAN-Ubuntu-Jetson-Image-Builder/"><img src="https://img.shields.io/jenkins/build?jobUrl=https%3A%2F%2Fjenkins-oai.eurecom.fr%2Fjob%2FRAN-Ubuntu-Jetson-Image-Builder%2F&label=build-Ubuntu-ARMv8%20Images"></a>
</p>

<p align="center">
  <a href="https://hub.docker.com/r/oaisoftwarealliance/oai-gnb"><img alt="Docker Pulls" src="https://img.shields.io/docker/pulls/oaisoftwarealliance/oai-gnb?label=gNB%20docker%20pulls&color=2496ED"></a>
  <a href="https://hub.docker.com/r/oaisoftwarealliance/oai-nr-ue"><img alt="Docker Pulls" src="https://img.shields.io/docker/pulls/oaisoftwarealliance/oai-nr-ue?label=nrUE%20docker%20pulls&color=2496ED"></a>
  <a href="https://hub.docker.com/r/oaisoftwarealliance/oai-enb"><img alt="Docker Pulls" src="https://img.shields.io/docker/pulls/oaisoftwarealliance/oai-enb?label=eNB%20docker%20pulls&color=2496ED"></a>
  <a href="https://hub.docker.com/r/oaisoftwarealliance/oai-lte-ue"><img alt="Docker Pulls" src="https://img.shields.io/docker/pulls/oaisoftwarealliance/oai-lte-ue?label=lteUE%20docker%20pulls&color=2496ED"></a>
  <a href="https://hub.docker.com/r/oaisoftwarealliance/oai-nr-cuup"><img alt="Docker Pulls" src="https://img.shields.io/docker/pulls/oaisoftwarealliance/oai-nr-cuup?label=nr-cuup%20docker%20pulls&color=2496ED"></a>
</p>

# Duranta - OpenAirInterface

Duranta OpenAirInterface RAN delivers and maintains an open-source cellular
wireless software stack for 4G, 5G and future networking technologies. It
supports simulation, prototyping, and end-to-end deployments on
Commercial-Off-The-Shelf (COTS) hardware. Built for research and
experimentation, it provides standard-compliant interfaces and is released
under the Collaborative Standards Software License v1.0 (CSSL).

## License

The source code is distributed under [**CSSL v1.0**](./LICENSE).
Some files, such as for orchestration, are distributed under
[MIT license](./LICENSES/preferred/MIT.txt). Documentation is distributed under
[Creative Commons Attribution 4.0 International license](./LICENSES/preferred/CC-BY-4.0.txt).

All the files without an explicit copyright header have an implicit "Copyright
of OpenAirInterface Authors".

Please see [NOTICE](./NOTICE) for other licenses which are used in the software,
and [LICENSES](./LICENSES/) for the full license texts.

In the past OAI source code has been re-licensed several times, here is the
history:

1. [CSSL v1.0](./LICENSE): starting tag [2026.w14](https://github.com/duranta-project/openairinterface5g/releases/tag/2026.w14)
2. [OAI Public License v1.1](./LICENSES/deprecated/OAI-PL-v1.1.txt): starting tag [v0.6.1](https://github.com/duranta-project/openairinterface5g/releases/tag/v0.6.1) till [af4b0d53](https://github.com/duranta-project/openairinterface5g/commit/af4b0d53e9f4c3bf0e5f82f58781eac07f312b86)
3. [OAI Public License v1.0](https://github.com/duranta-project/openairinterface5g/blob/v0.4/LICENSE): starting tag [v0.4](https://github.com/duranta-project/openairinterface5g/releases/tag/v0.4) till [v0.6.1](https://github.com/duranta-project/openairinterface5g/releases/tag/v0.6.1)
4. [GPL 3](https://github.com/duranta-project/openairinterface5g/blob/v0.3/COPYING): starting tag [v0.1](https://github.com/duranta-project/openairinterface5g/releases/tag/v0.1) till [v0.4](https://github.com/duranta-project/openairinterface5g/releases/tag/v0.4) (only initial implementation of 4G)

## Getting Started

The [documentation overview](./doc/README.md) is the entry point to all
guides, tutorials, and design documents. To contribute, read the
[contributing guidelines](./CONTRIBUTING.md). Sub-system documentation may also
live in the corresponding directories; to list all READMEs, run
`find . -iname "readme*"`.

Stable releases are listed on the
[releases page](https://github.com/duranta-project/openairinterface5g/releases)
with notable changes in the [changelog](./CHANGELOG.md). Weekly integration
tags (`YYYY.wXX`) mark the integration branches merged into `develop`.

## RAN Repository Structure

The repository is organized as follows:

```
openairinterface5g
├── .github           : GitHub workflows and repository configuration
├── LICENSES          : Preferred, deprecated, and exception license texts
├── charts            : Helm charts for running 4G and 5G physical simulators
├── ci-scripts        : OAI CI framework, test scenarios, and configuration files
├── CMakeLists.txt    : Top-level CMake configuration
├── cmake_targets     : Build and dependency-management utilities
├── common            : Common configuration, utilities, and shared infrastructure
├── doc               : Documentation
├── docker            : Dockerfiles for building Ubuntu, RHEL, and Rocky Linux images
├── executables       : Top-level gNB, eNB, UE, CU, DU, and O-RU entry points
├── fronthaul         : Native OAI 7.2x fronthaul implementation
├── maketags          : Script to generate Emacs tags
├── nfapi             : FAPI and nFAPI implementations for the MAC-PHY interface
├── openair1          : LTE and NR physical layer
├── openair2          : MAC, RLC, PDCP, SDAP, RRC, and application protocols
├── openair3          : NAS, S1AP, NGAP, GTP, and related layer-3 protocols
├── openshift         : OpenShift build configs and image streams used by CI
├── radio             : Radio and simulation drivers
├── targets           : Example configuration files and legacy resources
├── tests             : Component and integration tests
└── tools             : Developer and CI analysis, formatting, and plotting tools
```

## Governance

This repository is part of [Duranta](https://lfnetworking.org/projects/duranta/),
an [LF Networking](https://lfnetworking.org/) project overseen by its Technical
Steering Committee (TSC). See the
[governance repository](https://github.com/duranta-project/governance) for:

- [Technical Charter](https://github.com/duranta-project/governance/blob/main/Duranta_Project_Technical_Charter.pdf)
- [Contributor License Agreement](https://github.com/duranta-project/governance/blob/main/docs/easy_cla_process.md)
- [TSC members, maintainers, and collaborators](https://github.com/duranta-project/governance/blob/main/CONTRIBUTING.md)

Governance decisions are discussed in the
[TSC Meetings](https://github.com/duranta-project/openairinterface5g/wiki/Technical-Steering-Committee-(TSC)--Meeting).

## Community and Support

Before asking a question or reporting an issue, check the existing
[issues](https://github.com/duranta-project/openairinterface5g/issues) and
[discussions](https://github.com/duranta-project/openairinterface5g/discussions).

- [GitHub Issues](https://github.com/duranta-project/openairinterface5g/issues):
  report bugs and request features.
- [GitHub Discussions](https://github.com/duranta-project/openairinterface5g/discussions):
  ask questions, share ideas, and follow announcements.
- [Mailing Lists](https://github.com/duranta-project/openairinterface5g/wiki/MailingList):
  technical questions and troubleshooting by email.
- [Developer Meetings](https://github.com/duranta-project/openairinterface5g/wiki/Developer-Meetings):
  follow ongoing development and technical discussions.
- [Security Policy](./SECURITY.md): how to report security vulnerabilities.

### What to Include in Your Request

To get a quick answer, include:

- A clear subject, prefixed with `[Query]` for questions or `[Problem]` for
  problems (mailing lists).
- For problems, a short description and the steps to reproduce it.
- The commit (`git rev-parse HEAD`) or release tag you are using, and any
  local changes or patches (`git diff`).
- The command line used to start the affected gNB, DU, CU, CU-CP, CU-UP, or UE.
- Configuration files (`.conf` or `.yaml`) and logs (`.log` or `.txt`) of the
  affected gNB, DU, CU, CU-CP, CU-UP, or UE.
- For performance questions, the OS, kernel version, CPU, RAM, network card,
  and a diagram of your setup.
- Diagrams rather than photos or screenshots, whenever possible.
- Whether you used an AI tool to write the request or analyze the problem,
  as `TOOL-NAME: LLM-MODEL-VERSION`. Keep AI-generated text short and to the
  point; do not submit long AI-written descriptions.
