<!-- SPDX-License-Identifier: CC-BY-4.0 -->

[[_TOC_]]

# USRP device documentation

## General

OAI works with most common USRP models, like B200, B200mini/B205mini, B210,
X310, N300, N310, N320, X410. This is achieved by using the Ettus Universal
Hardware Driver (UHD) (https://github.com/EttusResearch/uhd). The file
[`usrp_lib.cpp`](./usrp_lib.cpp) provides an abstraction layer of UHD to OAI.

Example files can be found in the `ci-scripts/conf_files/` directory with a
`usrp` in the name, for instance
[`gnb.sa.band77.162prb.usrpn310.2x2.conf`](../../ci-scripts/conf_files/gnb.sa.band77.162prb.usrpn310.2x2.conf).

## Build

The OAI USRP driver in leverages the [USRP Hardware
Driver](https://github.com/EttusResearch/uhd) to interface with a USRP.
`build_oai` has support for installing UHD from package manager (Ubuntu) or from
source (other distributions or on request). See [`BUILD.md`](../../doc/BUILD.md) for
more information.

Note that OAI comes with a patch for UHD as found in `cmake_targets/tools/`.
This patch is automatically applied when building UHD from source with
`build_oai`, and improves the TX/RX switching times of UHD. If you cannot apply
this patch, the `--continuous-tx` option can be an alternative as described
further below.

## Configuration

The USRP can be configured in the RU section of the config file. The field
`sdr_addrs` uses the [USRP device identification
string](https://files.ettus.com/manual/page_identification.html). Here are a
few examples

```bash
sdr_addrs = "addr=192.168.10.2" # uses a single 10Gb Ethernet interface on an N3x0 or X3x0 or X4x0
sdr_addrs = "addr=192.168.10.2,second_addr=192.168.20.2" # uses 2 10Gb Ethernet interfaces on a N3x0 or X3x0 or X4x0 (requires that you flashed the FPGA wth the XG image)
sdr_addrs = "serial=ABC1234" # globally unique identifier of this device
```

you can also use the multi USRP feature and specify multiple USRPs, in which case you will get the aggregated number of channels on all the devices

```bash
sdr_addrs = "addr0=192.168.10.2,addr1=192.168.30.2"
```

You can specify to use external or interal clock or time source either by
adding the parameters in the `sdr_addrs` field or by using the fields
`clock_src` or `time_src`

```bash
sdr_addrs = "addr=192.168.10.2,clock_source=external,time_source=external"
```

is equivalent to

```bash
sdr_addrs = "addr=192.168.10.2"
clock_src = "external"
time_src  = "external"
```

Valid choices for clock and time source are `internal`, `external`, and `gpsdo`.

Note 1: the USRP remembers the choice of the clock source. If you want to make
sure it uses always the same, always specify the `clock_source` and
`time_source`.

Note 2: when using multiple USRPs they always have to be synchronized using
`external` or `gpsdo`

Last but not least you may specify that only a specfic subdevice of the USRP is
used. See also the section [Specifying the
Subdevice](https://files.ettus.com/manual/page_configuration.html#config_subdev)
in the UHD manual.

For example, on a USRP N310, the following fields will specify that you use
channel 0 of subdevice A. You can enter this information after providing the ip
addresses of the USRPs in `sdr_addrs`.

```bash
tx_subdev = "A:0"
rx_subdev = "A:0"
```
Here is an example of the RUs configuration for a setup with 2 USRP N310. 

```bash
RUs = (
{
  local_rf       = "yes"
  nb_tx          = 2
  nb_rx          = 2
  att_tx         = 0
  att_rx         = 0;
  bands          = [78];
  max_pdschReferenceSignalPower = -27;
  max_rxgain                    = 75;
  eNB_instances  = [0];
  sdr_addrs = "addr0=192.168.10.2,addr1=192.168.20.2,clock_source=external,time_source=external"
  tx_subdev ="A:0 B:0"
  rx_subdev ="A:1 B:1"
}
);
```

Hint: You cannot see TX/RX spread over multiple USRPs, if you use `internal` as a refrence of the clock and time source.

Furthur information about synchronization on the USRP N3xx devices can be found
[in the knowledge base article "Using Ethernet-Based
Synchronization"](https://kb.ettus.com/Using_Ethernet-Based_Synchronization_on_the_USRP%E2%84%A2_N3xx_Devices).

When combining this with the multi USRP feature you can create a distributed antenna array with only 1 channel used at each USRP.

## Further considerations

### TX/RX switching times

When using TDD (e.g., in 5G/NR band n78), the USRP has to frequently switch
between TX and RX directions. Certain USRPs, e.g., B210, might be too slow for
this, which can result in degraded radio link performance, especially in 5G/NR
operation. In order to get a good performance, there are two workarounds:

1. At runtime: you can use the `--continuous-tx` options at gNB/nrUE. This will
   continuously send TX samples, also when the current link direction is to
   receive, and we found this to improve the radio link.
2. At compile/build time: apply the UHD patch described further above, which
   obliveates the need for `--continuous-tx`.

### Three-quarter sampling

Not all sampling rates can be used with (a) given master sampling rate(s) of a
USRP. Depending on the bandwidth, you might employ (or not) three-quarter
sampling using option `-E`. As a basic rule of thump, 40MHz on B210 needs `-E`,
whereas most other bandwidths on USRPs do not (bu there are exceptions).

### Noise on DC carrier

Some USRPs have noise on the DC carrier, which can degrade the radio signal.
There are two workarounds:

1. Use the `--tune-offset` parameter, which shifts the operating frequency of
   the USRP to avoid the use of the DC carrier.
2. `--tune-offset` has a maximum shift frequency. If half the bandwidth is
   larger than `--tune-offset`, the DC carrier will still lie within the
   carrier. In this case, you can also mask these RBs by using
   `gNBs.[0].ul_prbblacklist` in the configuration file.

You can also find more information on this in the [5G/NR gNB with COTS UE
tutorial](../../doc/NR_SA_Tutorial_COTS_UE.md).

### Sample bit alignment (`rxshift` / TX shift)

`usrp_lib.cpp` registers a custom UHD CPU format, `sc16_oai` (see
`usrp_converters.cpp`), to correct for UHD's `sc16` bit alignment: UHD places
each converter's significant bits at the top of the 16-bit word and zeros the
rest, it doesn't renormalize to full scale (only `fc32` does). OAI's
fixed-point PHY expects samples in the low 12 bits instead, for MAC headroom,
so RX undoes UHD's MSB-justification and TX redoes it.

`rxshift` follows each RX converter's real ADC resolution:

| Device    | ADC                        | Resolution | `rxshift` |
|-----------|-----------------------------|-----------|-----------|
| B2xx      | AD9361                      | 12-bit    | 4 |
| X3xx/N3xx | ADS62P48 / AD9371            | 14-bit    | 2 |
| X4xx      | Xilinx RFSoC ZU28DR RF-ADC   | 12-bit    | 4 |

X4xx was previously lumped in with X3xx/N3xx at `rxshift = 2`, assuming a
14-bit ADC. Ettus's [X410](https://www.farnell.com/datasheets/3927908.pdf)
and [X440](https://www.ettus.com/wp-content/uploads/2023/09/ettus_usrp_x440_specifications.pdf)
spec sheets both list a 12-bit ADC resolution, so X4xx now gets its own
`rxshift = 4`, matching B2xx instead.

TX shift stays a flat 4 for every device, unlike `rxshift`. OAI's TX chain
never drives samples near full scale to begin with:
`gNB->TX_AMP` defaults to `1 << 9 = 512`, and with `tx_amp_backoff_dB` set,
`openair2/GNB_APP/gnb_config.c` computes it as `32767 / 10^(backoff_dB/20)` -
a default 36 dB backoff for OFDM PAPR headroom. With that much margin already
built in, tailoring the shift to each DAC's native resolution (12/14/16-bit)
wouldn't meaningfully change the transmitted signal, so the flat value is
kept. Every 6 dB of backoff costs roughly one effective bit to quantization
noise, so a higher-resolution DAC tolerates the same backoff with less
relative noise cost than a lower-resolution one.
