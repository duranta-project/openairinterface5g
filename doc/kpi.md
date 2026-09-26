# Key Performance Indicators for the OpenAirInterface Code Base

The goal of this document is to provide some Key Performance Indicators (KPI) for openairinterface5g RAN and UE stack. 

For every test we have mentioned the test/host system, but the same results can be achieved on [other systems](./Supported_Hardware_Operating_System.md)

## 1. `nr-softmodem` Performance in `oai-gNB` and `oai-gNB-du` Modes for FR1 bands

### Application level throughput for USRP

#### Test Profile

The following results apply to the TDD configuration below:

|Parameter          |Value                   |
|-------------------|------------------------|
|Band               |n41                     |
|SCS                |30 kHz                  |
|DL test TDD Pattern|`DDDSU`, 2.5ms          |
|UL test TDD Pattern|`DDSUU`, 2.5ms          |

- Test System: AMD Ryzen 9 7950X 16-Core Processor
- Radio: USRP N310 
- UE: Quectel RM500Q
- Environment: OTA, distance: 2m
- Application level throughput tested using iperf3 UDP

|Bandwidth MHz/PRB|Layers|DL Throughput (Mbps)|UL Throughput (Mbps)|
|-----------------|-----:|-------------------:|-------------------:|
|20(51)           |1     |72                  |39                  |
|                 |2     |143                 |65                  |
|                 |4     |258                 |X                   |
|40(106)          |1     |152                 |81                  |
|                 |2     |304                 |154                 |
|                 |4     |550                 |X                   |
|60(162)          |1     |233                 |123                 |
|                 |2     |466                 |175                 |
|                 |4     |730                 |X                   |
|80(217)          |1     |310                 |80                  |
|                 |2     |622                 |140                 |
|                 |4     |X                   |X                   |
|100(273)         |1     |400                 |101                 |
|                 |2     |800                 |120                 |

### Application level throughput for O-RAN 7.2 Fronthaul

#### Test Profile

The following results apply to the TDD configuration below:

|Parameter          |Value                   |
|-------------------|------------------------|
|Band               |n78/n77                 |
|SCS                |30 kHz                  |
|DL test TDD Pattern|`DDDSU`, 2.5ms, 10D2G2U |
|UL test TDD Pattern|`DDSUU`, 2.5ms, 6D4G4U  |

- Test System: AMD EPYC 9575F 64-Core Processor
- Radio: Benetel 550 O-RU
- UE: Quectel RM520N
- Environment: OTA, distance: 2m
- Uncompressed mode with avx512 disabled at compile time
- Application level throughput tested using iperf3 UDP

9b BFP static compression, 4T4R

|Bandwidth MHz/PRB|Layers|DL Throughput (Mbps)|UL Throughput (Mbps)|
|-----------------|-----:|-------------------:|-------------------:|
|40(106)          |1     |158                 |79                  |
|                 |2     |315                 |118                 |
|100(273)         |1     |412                 |180                 |
|                 |2     |820                 |250                 |
|                 |4     |1400                |X                   |

16b no compression, 2T2R

|Bandwidth MHz/PRB|Layers|DL Throughput (Mbps)|UL Throughput (Mbps)|
|-----------------|-----:|-------------------:|-------------------:|
|40(106)          |1     |158                 |67                  |
|                 |2     |315                 |83                  |
|100(273)         |1     |412                 |160                 |
|                 |2     |820                 |200                 |

## 2. `nr-softmodem` Performance in `oai-gNB` and `oai-gNB-du` Modes for FR2 bands

### Test Profile

The following results apply to the TDD configuration below:

|Parameter          |Value                   |
|-------------------|------------------------|
|Band               |257                     |
|SCS                |120 kHz                 |
|DL test TDD Pattern|`DDDSU`, 0.625ms, 10D2U |
|UL test TDD Pattern|`DDDSU`, 0.625ms, 64D4U |

- Test System: AMD EPYC 9575F 64-Core Processor
- Radio: MicroAmp, LiteON FR2
- UE: Quectel RG530F
- Environment: OTA, distance: 2m
- Application level throughput tested using iperf3 UDP

#### KPI

|Bandwidth MHz/PRB|Layers|DL Throughput (Mbps)|UL Throughput (Mbps)|
|-----------------|-----:|-------------------:|-------------------:|
|100(66)          |1     |X                   |X                   |
|                 |2     |550                 |x                   |
|200(132)         |1     |500                 |86                  |
|                 |2     |890                 |x                   |

Round trip time (measured using icmp ping): 4.526 ms

With `ulsch_max_frame_inactivity= 0;`

## 3. Performance Metrics for OAI Block Tests

### 3.1 Physical Simulators (`physims`)

For execution details, see [physical-simulators.md](./physical-simulators.md).

The tables below report the gNB processing time, in microseconds, averaged over
1000 frames (`-n1000`), for a set of bandwidth, layer, SNR and MCS combinations:

- TX processing is the `PHY proc tx` value reported by `nr_dlsim -P`
- RX processing is the `Total PHY proc rx` value reported by `nr_ulsim -P`
- DLSCH encoding is the `DLSCH encoding time` reported by `nr_dlsim -P`
- ULSCH decoding is the `ULSCH total decoding time` reported by `nr_ulsim -P`

`nr_dlsim` covers the DL only and `nr_ulsim` the UL only, hence the antenna
configurations do not fully overlap: entries marked `-` are not measured.

#### Example commands

```bash
# DL: 40 MHz (106 PRB), 1 layer, SNR 20, MCS 15
./nr_dlsim -n1000 -s20 -S20.2 -e15 -b106 -R106 -X <list of isolated CPUs> -P
# UL: 40 MHz (106 PRB), 1 layer, SNR 20, MCS 15
./nr_ulsim -n1000 -s20 -S20 -m15 -r106 -R106 -C8 -P
```

To obtain the other entries of the tables, adapt the following options:

|Option                                 |Meaning                                                        |
|---------------------------------------|---------------------------------------------------------------|
|`-b`/`-R` (nr_dlsim), `-r`/`-R` (nr_ulsim)|Number of PRBs, e.g. `106` for 40 MHz, `273` for 100 MHz    |
|`-s`/`-S`                              |SNR, e.g. `-s20 -S20.2` (nr_dlsim) or `-s20 -S20` (nr_ulsim)   |
|`-e` (nr_dlsim), `-m` (nr_ulsim)       |MCS index, e.g. `20` or `25`                                   |
|`-x` (nr_dlsim), `-W` (nr_ulsim)       |Number of layers, e.g. `-x2` or `-W2`                          |
|`-z`/`-y`                              |Number of RX/TX antennas, e.g. `-z2 -y2` or `-z4 -y4`          |

#### Test Profile 1

|Parameter   |Value                           |
|------------|--------------------------------|
|Machine     |AMD Ryzen 9 7945HX              |
|Architecture|x86_64                          |

|SNR/MCS|Bandwidth MHz/PRB|Configuration|TX Processing - nr_dlsim (us)|DLSCH Encoding - nr_dlsim (us)|RX Processing - nr_ulsim (us)|ULSCH Decoding - nr_ulsim (us)|
|-------|-----------------|-------------|------------------------------:|--------------------------------:|------------------------------:|--------------------------------:|
|20/15|40(106)|1 layer|44.86|30.52|119.81|81.85|
|||2 layers / 2 antennas|52.90|27.51|233.62|121.25|
|||2 layers / 4 antennas|63.41|27.87|-|-|
||100(273)|1 layer|57.85|28.70|212.31|139.86|
|||2 layers / 2 antennas|94.79|38.12|442.09|211.39|
|||2 layers / 4 antennas|118.37|37.60|-|-|
|30/25|40(106)|1 layer|41.08|25.64|121.39|77.98|
|||2 layers / 2 antennas|58.41|31.61|340.42|121.84|
|||2 layers / 4 antennas|69.55|32.17|-|-|
||100(273)|1 layer|68.81|37.36|225.67|133.66|
|||2 layers / 2 antennas|109.75|49.11|748.93|262.83|
|||2 layers / 4 antennas|140.59|55.78|-|-|

#### Test Profile 2

|Parameter   |Value                           |
|------------|--------------------------------|
|Machine     |DGX Spark, Cortex-X925, 20 cores|
|Architecture|aarch64                         |

|SNR/MCS|Bandwidth MHz/PRB|Configuration|TX Processing - nr_dlsim (us)|DLSCH Encoding - nr_dlsim (us)|RX Processing - nr_ulsim (us)|ULSCH Decoding - nr_ulsim (us)|
|-------|-----------------|-------------|------------------------------:|--------------------------------:|------------------------------:|--------------------------------:|
|20/15|40(106)|1 layer|78.87|46.69|156.54|97.61|
|||2 layers / 2 antennas|112.56|52.44|332.56|147.42|
|||2 layers / 4 antennas|142.34|54.21|-|-|
||100(273)|1 layer|126.82|58.30|274.24|168.85|
|||2 layers / 2 antennas|239.51|91.51|677.19|269.18|
|||2 layers / 4 antennas|310.43|92.67|-|-|
|30/25|40(106)|1 layer|81.34|48.89|153.56|92.56|
|||2 layers / 2 antennas|129.66|69.08|552.92|140.98|
|||2 layers / 4 antennas|158.74|69.13|-|-|
||100(273)|1 layer|151.22|82.04|294.53|178.25|
|||2 layers / 2 antennas|276.59|126.84|1276.87|300.83|
|||2 layers / 4 antennas|350.57|128.13|-|-|

## 4. `nr-uesoftmodem`

### Test Profile

The following results apply to the TDD configuration below:

|Parameter|Value  |
|---------|-------|
|Band     |n78/n77|
|SCS      |30 kHz |
|QAM      |64     |
|Mode     |SISO   |

Testbed Architecture:

UE <--> Over the Air 1.5m to 2m distance <--> USRP/RU <--> gNB/DU server

| Platform    | UE-Radio  | Bandwidth | DL Throughput | UL Throughput |
| ----------- | --------- | --------- | ------------: | ------------: |
| Jetson Orin | B210      | 10 MHz    | 12 Mbps       | 7.5 Mbps      |
| Jetson Orin | B210      | 20 MHz    | 20 Mbps       | 9.5 Mbps      |
| Jetson Orin | B210      | 30 MHz    | 61 Mbps       | 33 Mbps       |
| Jetson Orin | B210      | 40 MHz    | 69 Mbps       | 46 Mbps       |
| DGX Spark   | B210      | 40 MHz    | 86 Mbps       | 46 Mbps       |
| DGX Spark   | N310/x410 | 100 MHz   | 231 Mbps      | 118 Mbps      |
