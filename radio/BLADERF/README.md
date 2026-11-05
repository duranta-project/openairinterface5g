<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# BladeRF 2.0 Micro Documentation

[TOC]

## Install BladeRF 2.0 Micro Libraries

You can install the BladeRF library by using `build_oai -I -w BLADERF`, which
will install it from the package manager if the distribution supports it, or
falls back to a build from source otherwise.

If you intentionally want to install BladeRF from source, you can follow
[Nuand's Getting Started
guide](https://github.com/Nuand/bladeRF/wiki/Getting-Started%3A-Linux#building-bladerf-libraries-and-tools-from-source)
to do so.

## Configure the device

### Image Flash

Download the latest FX3 firmware and FPGA images from Nuand's website.  There
are [fx3 images](http://www.nuand.com/fx3_images/) and [FPGA
images](http://www.nuand.com/fpga_images/). Download and save the corresponding
files to your computer, then install the FX3 firmware

    sudo bladeRF-cli -f bladeRF_fw_latest.img

and install the FPGA image (this is x40):

    sudo bladeRF-cli -L hostedx40-latest.rbf

### Use of the configuration file

This configuration will help the bladeRF board to load the bitstream images
automatically, and can be used to set some options automatically that are not
configured by OAI.

Create the bladeRF config file:

```bash
$ cat ~/.config/Nuand/bladeRF/bladerf.conf
# Load the hostedx40.rbf FPGA image, set the trim DAC, and
# configure the frequency
fpga ~/.config/Nuand/bladeRF/image.rbf
# trimdac 592
# frequency 2.4G
```

More information is available at [Nuand's
website](https://nuand.com/libbladeRF-doc/v2.5.0/configfile.html).

### Useful commands for `bladerf-cli`

`bladeRF-cli` can be used to query various device details.

Show the version of bladeRF and device details

    bladeRF-cli --exec version
    bladeRF-cli --exec info

Probe for a specific device

    bladeRF-cli --probe

Print some information on device capabilities and current configuration
(bandwidth, frequency, clocking and sample rate, various features)

    bladeRF-cli --exec print

### Calibration

Retrieve calibration serial number:

    bladerf-cli --exec info

Go to the [Nuand calibration page](https://www.nuand.com/calibration) and enter
your serial number. The website tells you to run something like:

    sudo bladeRF-cli -i
    flash_init_cal 40 0x9271

Actual values depend on your device and serial number.

You can also rerun a calibration like so

    sudo bladeRF-cli -i
    cal lms
    cal dc rxtx

## OAI

### Build BladeRF driver

You can use `build_oai` to build with BladeRF support, e.g.,

    cd openairinterface5g/cmake_targets/
    ./build_oai --ninja --nrUE --gNB -w BLADERF

or you can use cmake directly

    cd openairinterface5g/
    cmake -B build -G Ninja -DOAI_BLADERF=ON
    cmake --build build -t oai_bladerfdevif    # to build only the driver
    cmake --build build                        # to build it all

For more information about the build, please refer to [the build
documentation](../../doc/BUILD.md).

### Adapt the OAI-gNB configuration file to your system

You can start from a [sample BladeRF configuration
fiel](../../targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.sa.band78.51PRB.bladerf2.0xa4.conf).

The only BladeRF-specific configuration resides in `RUs.[0].sdr_addrs`. This
string is passed to
[`bladerf_open()`](https://www.nuand.com/libbladeRF-doc/v2.6.0/group___f_n___i_n_i_t.html#gab341ac98615f393da9158ea59cdb6a24)
and can be used to select a specific BladeRF device. For instance, to open a
specific device with serial 730f5a36545849d9aa71ca6cf50e8ca0 on USB, you would
add `libusb: serial=730f5a36545849d9aa71ca6cf50e8ca0`.

The TX gain is automatically chosen to be the maximum, and can be reduced with
`att_tx`. The RX gain is taken from `RUs.[0].max_rxgain`, and can be attenuated
with `att_rx`.

The rest of the paramaters (PLMN, frequencies, IP addresses) is the same as in
any other configuration. Refer to the [general
tutorials](../..//doc/NR_SA_Tutorial_OAI_CN5G.md) for more information.

### Run the setup

Assuming you built from source above:

    cd ~/openairinterface5g/build/
    sudo ./nr-softmodem -O ../targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.sa.band78.51PRB.bladerf2.0xa4.conf -E

### In case of problems

Refer back to the calibration section above. If not done yet, consider
calibrating your device.

You can get more logs from BladeRF. To do so, you have to edit the code and
modify this line inside [`bladerf_lib.c`](./bladerf_lib.c).

```patch
-  bladerf_log_set_verbosity(BLADERF_LOG_LEVEL_INFO);
+  bladerf_log_set_verbosity(BLADERF_LOG_LEVEL_VERBOSE);
```

in function `device_init()`.
