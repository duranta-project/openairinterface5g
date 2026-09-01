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

#### Test Profile 1

|Parameter   |Value                           |
|------------|--------------------------------|
|Machine     |AMD Ryzen 9 7945HX              |
|Architecture|x86_64                          |

##### nr_dlsim

256 QAM modulation, 6 thread pool cores

DL Processing time is the `PHY proc tx` value reported by `nr_dlsim -P`, for gNB.

###### SNR 20 / MCS 20

|Bandwidth MHz/PRB|Layers|DL Processing (us)|Test Command|
|-----------------|------|------------------|------------|
|40(106)|1|66.70|`./nr_dlsim -n1000 -s20 -S20.2 -e20 -b106 -R106 -X 8,9,10,11,12,13,14,15 -P -q1`|
||2 (2 antennas)|87.96|`./nr_dlsim -n1000 -s20 -S20.2 -e20 -b106 -R106 -X 8,9,10,11,12,13,14,15 -P -q1 -x2 -z2 -y2`|
||(4 antennas)|97.09|`./nr_dlsim -n1000 -s20 -S20.2 -e20 -b106 -R106 -X 8,9,10,11,12,13,14,15 -P -q1 -x2 -z4 -y4`|
|100(273)|1|104.17|`./nr_dlsim -n1000 -s20 -S20.2 -e20 -b273 -R273 -X 8,9,10,11,12,13,14,15 -P -q1`|
||2 (2 antennas)|170.40|`./nr_dlsim -n1000 -s20 -S20.2 -e20 -b273 -R273 -X 8,9,10,11,12,13,14,15 -P -q1 -x2 -z2 -y2`|
||(4 antennas)|192.94|`./nr_dlsim -n1000 -s20 -S20.2 -e20 -b273 -R273 -X 8,9,10,11,12,13,14,15 -P -q1 -x2 -z4 -y4`|

###### SNR 30 / MCS 25

|Bandwidth MHz/PRB|Layers|DL Processing (us)|Test Command|
|-----------------|------|------------------|------------|
|40(106)|1|67.01|`./nr_dlsim -n1000 -s30 -S30.2 -e25 -b106 -R106 -X 8,9,10,11,12,13,14,15 -P -q1`|
||2 (2 antennas)|95.87|`./nr_dlsim -n1000 -s30 -S30.2 -e25 -b106 -R106 -X 8,9,10,11,12,13,14,15 -P -q1 -x2 -z2 -y2`|
||(4 antennas)|104.97|`./nr_dlsim -n1000 -s30 -S30.2 -e25 -b106 -R106 -X 8,9,10,11,12,13,14,15 -P -q1 -x2 -z4 -y4`|
|100(273)|1|109.78|`./nr_dlsim -n1000 -s30 -S30.2 -e25 -b273 -R273 -X 8,9,10,11,12,13,14,15 -P -q1`|
||2 (2 antennas)|193.51|`./nr_dlsim -n1000 -s30 -S30.2 -e25 -b273 -R273 -X 8,9,10,11,12,13,14,15 -P -q1 -x2 -z2 -y2`|
||(4 antennas)|212.92|`./nr_dlsim -n1000 -s30 -S30.2 -e25 -b273 -R273 -X 8,9,10,11,12,13,14,15 -P -q1 -x2 -z4 -y4`|

##### nr_ulsim

64 QAM modulation, 8 thread pool cores

UL Processing time is the `Total PHY proc rx` value reported by `nr_ulsim -P`, for gNB.

###### SNR 20 / MCS 20

|Bandwidth MHz/PRB|Layers|UL Processing (us)|Test Command|
|-----------------|------|------------------|------------|
|40(106)|1|140.17|`./nr_ulsim -n1000 -s20 -S20 -m20 -r106 -R106 -C8 -P`|
||2|321.56|`./nr_ulsim -n1000 -s20 -S20 -m20 -r106 -R106 -C8 -P -W2 -z2 -y2`|
|100(273)|1|249.84|`./nr_ulsim -n1000 -s20 -S20 -m20 -r273 -R273 -C8 -P`|
||2|892.62|`./nr_ulsim -n1000 -s20 -S20 -m20 -r273 -R273 -C8 -P -W2 -z2 -y2`|

###### SNR 30 / MCS 25

|Bandwidth MHz/PRB|Layers|UL Processing (us)|Test Command|
|-----------------|------|------------------|------------|
|40(106)|1|107.65|`./nr_ulsim -n1000 -s30 -S30 -m25 -r106 -R106 -C8 -P`|
||2|305.61|`./nr_ulsim -n1000 -s30 -S30 -m25 -r106 -R106 -C8 -P -W2 -z2 -y2`|
|100(273)|1|228.84|`./nr_ulsim -n1000 -s30 -S30 -m25 -r273 -R273 -C8 -P`|
||2|889.85|`./nr_ulsim -n1000 -s30 -S30 -m25 -r273 -R273 -C8 -P -W2 -z2 -y2`|

#### Test Profile 2

|Parameter   |Value                           |
|------------|--------------------------------|
|Machine     |DGX Spark, Cortex-X925, 20 cores|
|Architecture|aarch64                         |

##### nr_dlsim

256 QAM modulation, 6 thread pool cores

DL Processing time is the `PHY proc tx` value reported by `nr_dlsim -P`, for gNB.

###### SNR 20 / MCS 20

|Bandwidth MHz/PRB|Layers|DL Processing (us)|Test Command|
|-----------------|------|------------------|------------|
|40(106)|1|104.04|`./nr_dlsim -n1000 -s20 -S20.2 -e20 -b106 -R106 -X 8,9,10,11,12,13,14,15 -P -q1`|
||2 (2 antennas)|161.55|`./nr_dlsim -n1000 -s20 -S20.2 -e20 -b106 -R106 -X 8,9,10,11,12,13,14,15 -P -q1 -x2 -z2 -y2`|
||(4 antennas)|191.94|`./nr_dlsim -n1000 -s20 -S20.2 -e20 -b106 -R106 -X 8,9,10,11,12,13,14,15 -P -q1 -x2 -z4 -y4`|
|100(273)|1|189.12|`./nr_dlsim -n1000 -s20 -S20.2 -e20 -b273 -R273 -X 8,9,10,11,12,13,14,15 -P -q1`|
||2 (2 antennas)|345.56|`./nr_dlsim -n1000 -s20 -S20.2 -e20 -b273 -R273 -X 8,9,10,11,12,13,14,15 -P -q1 -x2 -z2 -y2`|
||(4 antennas)|418.26|`./nr_dlsim -n1000 -s20 -S20.2 -e20 -b273 -R273 -X 8,9,10,11,12,13,14,15 -P -q1 -x2 -z4 -y4`|

###### SNR 30 / MCS 25

|Bandwidth MHz/PRB|Layers|DL Processing (us)|Test Command|
|-----------------|------|------------------|------------|
|40(106)|1|103.05|`./nr_dlsim -n1000 -s30 -S30.2 -e25 -b106 -R106 -X 8,9,10,11,12,13,14,15 -P -q1`|
||2 (2 antennas)|169.22|`./nr_dlsim -n1000 -s30 -S30.2 -e25 -b106 -R106 -X 8,9,10,11,12,13,14,15 -P -q1 -x2 -z2 -y2`|
||(4 antennas)|197.41|`./nr_dlsim -n1000 -s30 -S30.2 -e25 -b106 -R106 -X 8,9,10,11,12,13,14,15 -P -q1 -x2 -z4 -y4`|
|100(273)|1|194.23|`./nr_dlsim -n1000 -s30 -S30.2 -e25 -b273 -R273 -X 8,9,10,11,12,13,14,15 -P -q1`|
||2 (2 antennas)|354.08|`./nr_dlsim -n1000 -s30 -S30.2 -e25 -b273 -R273 -X 8,9,10,11,12,13,14,15 -P -q1 -x2 -z2 -y2`|
||(4 antennas)|428.30|`./nr_dlsim -n1000 -s30 -S30.2 -e25 -b273 -R273 -X 8,9,10,11,12,13,14,15 -P -q1 -x2 -z4 -y4`|

##### nr_ulsim

64 QAM modulation, 8 thread pool cores

UL Processing time is the `Total PHY proc rx` value reported by `nr_ulsim -P`, for gNB.

###### SNR 20 / MCS 20

|Bandwidth MHz/PRB|Layers|UL Processing (us)|Test Command|
|-----------------|------|------------------|------------|
|40(106)|1|824.76|`./nr_ulsim -n1000 -s20 -S20 -m20 -r106 -R106 -C8 -P`|
||2|3349.06|`./nr_ulsim -n1000 -s20 -S20 -m20 -r106 -R106 -C8 -P -W2 -z2 -y2`|
|100(273)|1|1612.73|`./nr_ulsim -n1000 -s20 -S20 -m20 -r273 -R273 -C8 -P`|
||2|7899.00|`./nr_ulsim -n1000 -s20 -S20 -m20 -r273 -R273 -C8 -P -W2 -z2 -y2`|

###### SNR 30 / MCS 25

|Bandwidth MHz/PRB|Layers|UL Processing (us)|Test Command|
|-----------------|------|------------------|------------|
|40(106)|1|656.31|`./nr_ulsim -n1000 -s30 -S30 -m25 -r106 -R106 -C8 -P`|
||2|3187.49|`./nr_ulsim -n1000 -s30 -S30 -m25 -r106 -R106 -C8 -P -W2 -z2 -y2`|
|100(273)|1|1509.84|`./nr_ulsim -n1000 -s30 -S30 -m25 -r273 -R273 -C8 -P`|
||2|7389.53|`./nr_ulsim -n1000 -s30 -S30 -m25 -r273 -R273 -C8 -P -W2 -z2 -y2`|

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
