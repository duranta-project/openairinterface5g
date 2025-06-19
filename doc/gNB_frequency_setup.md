<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# 5G gNB Frequency Configuration

Configuring the frequency settings for a 5G gNB involves specifying multiple parameters in the gNB configuration file. This guide outlines the necessary parameters and provides step-by-step instructions for configuration.

[TOC]

## Configuration parameters

To configure the gNB with the required frequency and bandwidth, modify following parameters in the configuration file:

### `gNBs` Section

* `absoluteFrequencySSB`: ARFCN (frequency number) of the middle frequency of the SSB. This value must align with the NR frequency raster defined in TS 38.104, Section 5.4.3.1.
  * Steps to compute this value are provided below.
* `dl_absoluteFrequencyPointA`: ARFCN (frequency number) for Point A, lowest subcarrier of the reference resource block (Common RB 0).
  * Steps to compute this value are provided below.
* `dl_frequencyBand`: downlink NR frequency band number.
* `dl_subcarrierSpacing`: downlink subcarrier spacing.
  * *Allowed values:* `0` = 15 kHz, `1` = 30 kHz, `2` = 60 kHz, `3` = 120 kHz
* `dl_carrierBandwidth`: number of PRBs for downlink.
* `initialDLBWPsubcarrierSpacing`: initial downlink BWP subcarrier spacing.
  * *Allowed values:* `0` = 15 kHz, `1` = 30 kHz, `2` = 60 kHz, `3` = 120 kHz
* `initialDLBWPcontrolResourceSetZero`: CORESET0 index (valid configurations, based on SCS and frequency bandwith, are given in TS 38.213, Table 13.1 to Table 13.14).
* `ul_frequencyBand`: uplink NR frequency band number.
* `ul_absoluteFrequencyPointA`: ARFCN (frequency number) for Point A, lowest subcarrier of the reference resource block (Common RB 0).
  * **Set only for FDD configurations.**
  * Frequency separation between DL and UL channels for a specific 5G NR band is defined in 38.101-1 5.4.4-1 for FR1 bands and in 38.101-5 5.4.4-1 for FR1 NTN bands. `ul_absoluteFrequencyPointA` is calculated by substracting the given frequency offset from the `dl_absoluteFrequencyPointA`.
* `ul_carrierBandwidth`: number of PRBs for uplink.
* `initialULBWPsubcarrierSpacing`: initial uplink BWP subcarrier spacing.
  * *Allowed values:* `0` = 15 kHz, `1` = 30 kHz, `2` = 60 kHz, `3` = 120 kHz
* `subcarrierSpacing`: general subcarrier spacing for both DL and UL.
  * *Allowed values:* `0` = 15 kHz, `1` = 30 kHz, `2` = 60 kHz, `3` = 120 kHz

### `RUs` Section

* `bands`: NR frequency band number for RU(s).

## Determining the ARFCN value for `absoluteFrequencySSB` and `dl_absoluteFrequencyPointA`

Follow these steps to find the ARFCN values for `absoluteFrequencySSB` and `dl_absoluteFrequencyPointA`:
1. Convert RU center frequency to ARFCN value
    * Example tool: [5G NR ARFCN Calculator](https://5g-tools.com/5g-nr-arfcn-calculator/)
    * Input the center frequency of the RU (in MHz) into the tool to calculate the ARFCN value.

2. Enter parameters into the online tool for NR Point A computation
    * Example tool: [NR Reference Point A Tool](https://www.sqimway.com/nr_refA.php)
    * Enter the NR frequency band, ARFCN value for center frequency (use the value calculated in step 1), subcarrier spacing, CORESET0 index (`initialDLBWPcontrolResourceSetZero` in the gNB config file) and bandwidth into the tool.

3. Obtain required values
    * From the tool's output, use the following results for your configuration:
      * Arfcn GSCN: This corresponds to `absoluteFrequencySSB` - value for `absoluteFrequencySSB` can be selected freely from the table, if `Coreset(0) check` is "ok".
      * Point A Arfcnc: This corresponds to `dl_absoluteFrequencyPointA`.

4. Set `dl_absoluteFrequencyPointA` and `absoluteFrequencySSB` in the gNB configuration file

## Example calculation
1. Convert RU center frequency to ARFCN value
    * Center frequency = 3450 MHz
    * Center frequency ARFCN = 630000

2. Enter parameters into the online tool for NR Point A computation
    * Open the [NR Reference Point A Tool](https://www.sqimway.com/nr_refA.php) and input the following values:
      * Frequency band = n78
      * NR Arfcn scan (center frequency) = 630000
      * SCS = 30 kHz
      * Bandwidth = 100 MHz
      * Control ResourceSet Zero = 11

3. Obtain required values
    * The tool provides following output values:
      * Arfcn GSCN (`absoluteFrequencySSB`) = 630048
      * Point A Arfcnc (`dl_absoluteFrequencyPointA`) = 626724
