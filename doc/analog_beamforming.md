<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# Implementation of Analog Beamforming

This document explains the implementation of analog beamforming in OAI codebase.

[[_TOC_]]

## Introduction to analog beamforming

Beamforming is a technique applied to antenna arrays to create a directional radiation pattern. This often consists in providing a different phase shift to each element of the array such that signals with a different angle of arrival/departure experience a change in radiation pattern because of constructive or destructive interference.

There are three main beamforming techniques: analog, digital and hybrid. The names refer to the phase shift application before or after the digital to analog conversion (or analog to digital in reception). When we speak about analog beamforming we generally refer to a technique where the phase shifts that produce the beam steering are applied by the radio unit (RU) choosing from a finite set of steering directions. The advantage of analog beamforming is a simplified analog circuitry and therefore reduced costs.

The presence of a limited number of predefined beams at RU poses constraints to the scheduler at gNB. As a matter of fact, the scheduler can serve only a limited number of beams, depending on the RU characteristics (possibly only 1), in a given time scale, that also depends on the RU characteristics (e.g. 1 slot or 1 symbol). This limitation doesn't exist for digital beamforming.

Analog beamforming implementation also allows to enable distributed antenna systems (DAS), where each beam corresponds to one antenna (or a set of antennas) of the system. In this scenario, the scheduler constraint is alleviated because normally the number of concurrent beams allowed equals the total number of beams.

## Configuration file fields for analog beamforming

A set of parameters in configuration files controls the implementation of analog beamforming and instructs the scheduler on how to behave in such scenarios. Since most notably this technique in 5G is employed in FR2, the configuration file example currently available is a RFsim one for band 261. [Config file example](../ci-scripts/conf_files/gnb.sa.band257.u3.66prb.rfsim.conf)

In the `MACRLC` section of configuration files, there are three new parameters: `set_analog_beamforming`, `beam_duration` and `beams_per_period`. The explanation of these parameters is here provided:
- `set_analog_beamforming` can be set to "none", "preconfigured" or "lophy" (default value is "none")
- `beam_duration` is the number of slots (currently minimum duration of a beam) the scheduler is tied to a beam (default value is 1)
- `beams_per_period` is the number of concurrent beams the RU can handle in the beam duration (default value is 1)
- `beam_weights` is a vector field containing the set of beam indices to be provided by the OAI L1 to the RU is also required. In current implementation, the number of beam indices should be equal to the number of SSBs transmitted

Setting analog beamforming to "preconfigured" or "lophy" changes the way FAPI beam index is treated. By setting "preconfigured", we instruct L1 to look up in Hi-PHY preconfigured DBM beam index. By setting "lophy", we instruct L2 to directly signal to Lo-PHY the beam index (e.g. over 7.2x fronthaul).

DAS is enabled by setting to 1 the parameter `enable_das` in the L1 section of the configuration file. In case of DAS enabled, the field `beam_weights` in `MACRLC` section can be omitted and the number of beams per period equals the total number of beams.

## Implementation in OAI scheduler

A new MAC structure `NR_beam_info_t` controls the behavior of the scheduler in presence of analog beamforming. Besides the already mentioned parameters `beam_duration` and `beams_per_period`, the structure also holds a matrix `beam_allocation[i][j]`, whose indices `i` and `j` stands respectively for the number of beams in the period and the slot index (the size of the latter depends on the frame characteristics).
This matrix contains the beams already allocated in a given slot, to flag the scheduler to use one of these to schedule a UE in one of these beams. If the matrix is full (all the beams in the given period, e.g. slot) are already allocated, the scheduler can't allocate a UE in a new beam.
To this goal, we extended the virtual resource block (VRB) map by one dimension to also contain information per allocated beam. As said, the scheduler can independently schedule users in a number of beams up to `beams_per_period` concurrently.

It is important to note that in current implementation, there are several periodical channels, e.g. PRACH or PUCCH for CSI et cetera, that have the precendence in being assigned a beam, that is because the scheduling is automatic, set in RRC configuration, and not up to the scheduler. For these instances, we assume the beam is available (if not there are assertions to stop the process). For data channels, the currently implemented PF scheduler is used. The only modification is that a UE can be served only if there is a free beam available or the one of the beams already in use correspond to that UE beam.

## Beams in phy-test scheduler

In phy-test mode, beams are assigned to PDSCH slots in the same manner as SSB slots with the only addition that it repeats for every TDD period in a frame. For example if PDSCH is scheduled on all DL slot for TDD format DDDDDDDSUU, and with SSB bit map = `0b1010101` and the following beam parameters in config file,
- `beam_weights` = [10,11,12,13]
- `beam_duration` = 1
- `beams_per_period` = 1

The DL slots in every TDD period will have beams 10,11,12,13,0,0,0

## FAPI implementation

To be noted that in our implementation analog beamforming is only partially supported in split mode.
The index based beamforming relies on the Tx precoding and beamforming PDU definition of beam-ID, where MSB is used to signal if the ID can be directly used or is it a pointer to a pre-stored vector of weights.
This definition of beam-ID is present only in the most recent versions of SCF PHY API specifications (at least from v8 and later, possibly from v6).

In addition to that, a `config_request` structure defined as vendor extension (`nfapi_nr_analog_beamforming_ve_t`) configures the lower layers at initialization with the following information:
- `analog_bf_vendor_ext` which can assume values 1 or 0 for enabling or disabling analog beamforming

Therefore, in case of analog beamforming, L2 provides in each channel FAPI message information about the beam index via the beam-ID parameter with MSB set to 1.

## L1 implementation

The total number of logical antenna ports available at L1 is same as `pusch_AntennaPorts * beams_per_period` in UL and `pdsch_AntennaPorts_N1 * pdsch_AntennaPorts_N2 * pdsch_AntennaPorts_XP * beams_per_period` in DL.
To handle multiple concurrent beams, L2 uses spatial stream indices specified by FAPI to signal L1 on which logical ports to use for a DL or UL signal. The config file parameter `spatial_stream_index` can be used to specify an array of logical port indices to be used. If this parameter is not provided then the indices defaults to `[0 ... pusch_AntennaPorts - 1]`. This parameter is particularly useful when a specific subset of eAxCID has to be used.
In case of DAS, since each beam corresponds to a specific antenna port, the `beam_index_allocation` function is simplified in the sense that the beam index corresponds to the antenna port index of the frequency domain buffers.

## RU implementation

The implementation is still work in progress.

The first dimension of the Tx and Rx buffers contains the number of Tx/Rx antennas which is at least the number of logical antenna ports.

### Beam signalling to the radio

L1 stores the beam index of each symbol in `gNB->common_vars.beam_id[slot * symbols_per_slot + symbol][antenna]`. Once per slot, the RU walks that table and calls the radio API `trx_set_beams(device, beams, num_beams, timestamp)` whenever the beam changes from one symbol to the next. The timestamp allows the radio driver to apply the beam at the right sample, ahead of the corresponding transmission. A radio driver that cannot steer beams leaves `trx_set_beams` unset.

### Beam control of an external beamformer over USRP

The USRP driver programs an external beamformer through the GPIO connector of the radio. The type of beamformer is selected with the `gpio_controller` parameter in the `RUs` section of the configuration file, which accepts:
- `generic`: the beam index is written to the GPIO pins directly, which limits the number of beams to 8
- `interdigital`: for the InterDigital mmWave frontend
- `tmytek`: for TMYTEK beamformers, where the beam index is sent over SPI

If `gpio_controller` is omitted, beam control is disabled. Note that this parameter also sets up the TDD switching signals of the frontend, so it is required whenever such a frontend is attached, even without analog beamforming.

The `tmytek` controller requires a USRP X410 and UHD 4.1 or later, because it relies on the SPI engine of the radio. The beam index provided by L1 is translated into a write of the beamformer phase register, followed by a pulse of the latch pin. The SPI protocol configuration is described in `radio/TMYTEK/tmytek_spi_config.h`, separately from the USRP driver that carries it. Beam indices are 0-based in OAI and 1-based in the TMYTEK codebook, so `beam_weights = [0, 1, 2]` selects the first three beams of the codebook. The gain registers of the beamformer are not written at runtime and have to be programmed when the array is initialised.

### TMYTEK integration

See the [TMYTEK beam control guide](../radio/TMYTEK/README.md) for the hardware setup and integration instructions.
