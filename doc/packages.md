<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# OpenAirInterface Packages

This document describes how to build Deb/RPM packages with OAI's build system, cmake.

[[_TOC_]]

## Create packages

Packages are created by running `cpack` after configuring OAI with the appropriate options and building the executables.

First, configure the project with `cmake` and select which type of package you want to build. Then, compile, and finally run `cpack`:

```bash
cd ~/openairinterface5g
mkdir build && cd build
cmake .. -GNinja <options>
ninja
cpack
```

where `<options>` is for:

- packaging **USRP**: `-DOAI_USRP=ON -DPACKAGING_USRP=ON`
- packaging **LTE**: `-DPACKAGING_LTE=ON`
- packaging **NR**: `-DPACKAGING_NR=ON`
- packaging **COMMON**: `-DPACKAGING_COMMON=ON -DENABLE_TELNETSRV=ON`
- packaging **PHYSIM**: `-DPACKAGING_PHYSIM=ON`

By default, deb packages are built. If you want to build for RPM, also provide `-DPACKAGING_RPM=ON`

For example, to package all available packages, run:

```bash
cmake .. -GNinja -DOAI_USRP=ON -DPACKAGING_USRP=ON -DPACKAGING_LTE=ON -DPACKAGING_NR=ON -DPACKAGING_COMMON=ON -DENABLE_TELNETSRV=ON -DPACKAGING_PHYSIM=ON
ninja
cpack
```

## Package installation

In order to install the packages, run:

### Ubuntu/Debian

    dpkg -i liboai-common.deb liboai-usrp.deb oai-lte.deb oai-nr.deb oai-physim.deb

### CentOS/Fedora

    rpm -i liboai-common.rpm liboai-usrp.rpm oai-lte.rpm oai-nr.rpm oai-physim.rpm


**NOTE**: After oai-nr and oai-lte installation, you'll have services running in systemd, for more information check Systemd paragraph.


## Usage

### Use debug symbols

- Install gdb with `apt` or `dnf`
- Install both stripped package and the corresponding debug symbol one
- Use gbd on you executable, example: ```gdb --args /usr/bin/nr-softmodem -O
 /etc/openairinterface/nr/gnb.sa.band78.fr1.106PRB.usrpb210.conf -E
 --continuous-tx -A -2 --device.name oai_usrpdevif```
- Then use ```run```

### Systemd

Once installed, oai-nr and oai-lte have **services** which run in systemd
(`nr-softmodem` and `nr-cuup` for oai-nr and `lte-softmodem` for oai-lte),
here is a list of useful **commands**:

- start
- stop
- restart
- reload
- status
- enable (enable service at boot)
- disable (disable service at boot)
- is-enabled

To use them:

    sudo systemctl [command] [service]

By default, following configuration files are selected:
- **nr-softmodem**: `gnb.sa.band78.fr1.106PRB.usrpb210.conf`
- **nr-cuup**: `gnb-cuup.sa.f1.conf`
- **lte-softmodem**: `enb.band7.tm1.50PRB.usrpb210.conf`

To modify the configuration:
- Select the configuration file you wish from the following locations:
    - `/etc/openairinterface/nr`
    - `/etc/openairinterface/lte`
    - Or any custom configuration file you have created.
- Edit the service configuration under `/lib/systemd/system`.

Main lines to be modified are

    ExecStart=/usr/bin/nr-softmodem -O /etc/openairinterface/nr/gnb.sa.band78.fr1.106PRB.usrpb210.conf -E --continuous-tx -A -2 --device.name oai_usrpdevif

This line specifies the command and its parameters that the service will execute.

    Restart=on-failure
    RestartSec=5s

These directives define the service’s behavior in case of failure, including whether it should restart and the delay before restarting.

**NOTE**: Additional configuration options are available for further customization.
For full customization please check [systemd services documentation](https://www.freedesktop.org/software/systemd/man/latest/systemd.service.html)

After making your changes, apply them by running:

    sudo systemctl daemon-reload && sudo systemctl restart nr-softmodem.service

For instructions on using a custom configuration file, see the [setup documentation](./gNB_frequency_setup.md).



To access service logs:

    sudo journalctl -u [service] --since "today"


To follow service logs:

    sudo journalctl -u [service] --follow


## Purge

To uninstall packages and automatically stop and remove services in systemd, run:

### Ubuntu/Debian

    dpkg -r liboai-common liboai-usrp oai-lte oai-nr oai-physim

### CentOS/Fedora

    rpm -e liboai-common liboai-usrp oai-lte oai-nr oai-physim
