## TMYTEK Beam Control

This directory contains two separate parts:

- `tmytek_spi_config.h` and the OAI-integrated NI USRP driver provide timestamped runtime beam switching.
- `tlkcore/` provides optional Ethernet-only TMYTEK array initialization and keeps the arrays in fast-parallel mode while OAI runs.

The updated TLKCore controller does not include or depend on `lib_usrp_spi` for this workflow. Fast beam steering is implemented using the USRP library integrated within OAI.

### Build TLKCore

#### 1. Clone the vendor examples

```bash
mkdir -p /home/user/workarea
cd /home/user/workarea
git clone https://github.com/tmytek/tlkcore-examples
```

#### 2. Install build prerequisites

On Debian or Ubuntu, install the compiler, CMake, Python development files, pybind11, and pip:

```bash
sudo apt-get update
sudo apt-get install build-essential cmake python3-dev python3-pip pybind11-dev
```

The vendor C++ wrapper also requires the Python packages listed in `requirements.txt`. Use the system `python3` installation and follow the vendor [Python sample guide](https://github.com/tmytek/tlkcore-examples/blob/master/examples/Python/README.md). TMYTEK currently supports Python 3.8, 3.10, and 3.12. Use the same selected Python version for the vendor Python extension modules and `libtlkcore_lib.so`.

#### 3. Install vendor Python dependencies

```bash
cd /home/user/workarea/tlkcore-examples/examples/C_Cpp/lib_tlkcore_cpp
python3 -m pip install -r requirements.txt
```

The vendor `requirements.txt` includes `psutil`, `pyserial`, `ft4222`, and `pybind11-global`. Use the Python version supported by the checked-out vendor example when installing these dependencies.

#### 4. Build and install the TLKCore C++ wrapper

```bash
cd /home/user/workarea/tlkcore-examples/examples/C_Cpp/lib_tlkcore_cpp
mkdir -p build
cd build
cmake ..
make install
```

If CMake cannot find pybind11, use the Python and pybind11 CMake options described by the vendor project. The resulting `libtlkcore_lib.so` is expected at:

```text
tlkcore-examples/examples/C_Cpp/lib_tlkcore_cpp/libtlkcore_lib.so
```

### OAI controller dependency

The OAI `tlkcore_fbs` controller uses the header-only [nlohmann/json](https://github.com/nlohmann/json) library to parse `device.conf`. Install the development package before configuring the optional OAI target:

```bash
sudo apt-get update
sudo apt-get install nlohmann-json3-dev
test -f /usr/include/nlohmann/json.hpp
```

If the package is installed in a non-standard location, provide its include directory with `CMAKE_INCLUDE_PATH` when configuring the build. The controller also requires a C++17 compiler, which is provided by the `build-essential` package above.

### Provide vendor antenna assets

Before running the controller, obtain the TMYTEK antenna assets for the configured arrays from your vendor package or hardware provider. These files are not distributed with OAI because they are hardware-specific calibration and antenna data.

Copy the vendor `files/` directory contents into the local runtime directory:

```bash
cp -a /path/to/vendor/files/. /path/to/openairinterface5g/radio/TMYTEK/tlkcore/files/
```

The directory must contain:

- **AAKIT tables**: antenna-kit definitions whose names match `AAKIT_NAME` in `config/device.conf`.
- **Frequency calibration tables**: calibration CSV files for each configured beamformer and operating frequency.
- **BeamTable/**: vendor beam-configuration metadata used by TLKCore.

Typical vendor asset names follow patterns such as:

```text
files/AAKIT_<antenna-kit-name>.csv
files/<beamformer-serial>-<frequency>GHz.csv
files/BeamTable/
```

The exact filenames depend on the antenna kit, beamformer serial number, and operating frequency. Ensure the supplied AAKIT and calibration files match the devices listed in `config/device.conf`.

### Build the Ethernet-only controller

Configure OAI with the optional target enabled. The TLKCore controller itself does not add a separate UHD or Boost dependency, but `OAI_USRP=ON` requires the normal OAI USRP/UHD build dependencies:

`DOAI_TLKCORE_ROOT` points to the C++ wrapper root used at build time. The runtime `--tlkcore-root` path is different: it points to the vendor Python directory containing `tlkcore/` and `TMYConfig.py`.

```bash
cd openairinterface5g
cmake -S . -B build-tmytek -GNinja \
	-DOAI_USRP=ON \
	-DOAI_TMYTEK_TLKCORE=ON \
	-DOAI_TLKCORE_ROOT=/home/user/workarea/tlkcore-examples/examples/C_Cpp \
	-DENABLE_TESTS=ON
cmake --build build-tmytek --target tlkcore_fbs
```

The device configuration is [config/device.conf](tlkcore/config/device.conf). In the vendor schema, `BF` means beamformer and `UD` means up/down converter. The file contains TLKCore BF/UD Ethernet configuration, the adopted BF metadata (`RFMode` and `spi_beamID`), and the required UD `STATE` block used by TLKCore during UD initialization.

### Example `device.conf`

The complete configuration used by the example controller is:

```json
{
	"VERSION": "1.0.0",
	"BF_LAYERS": {
		"D2310E003-28": {
			"AAKIT_NAME": "TMYTEK_28ONE_4x4_2303-O28AK41C-045",
			"BEAM_CONFIG": "config/CustomBatch8Beams_D2310E003-28.csv",
			"targetFreq": 28,
			"RFMode": 0,
			"spi_beamID": 1
		},
		"D2310L020-28": {
			"AAKIT_NAME": "TMYTEK_28LITE_4x4_2303-28AK344A-020",
			"BEAM_CONFIG": "config/CustomBatchBeams_D2310L020-28.csv",
			"targetFreq": 28,
			"RFMode": 1,
			"spi_beamID": 1
		}
	},
	"UD_LAYERS": {
		"UD-BD22460025-24": {
			"STATE": {
				"PLO_LOCK": 1,
				"CH1": 1,
				"CH2": 1,
				"OUT_10M": 0,
				"OUT_100M": 0,
				"SOURCE_100M": 0,
				"LED_100M": 0,
				"PWR_5V": 0,
				"PWR_9V": 0
			},
			"targetFreq": 28,
			"ifFrequency": 2.65
		}
	}
}
```

Configuration fields:

- `BEAM_CONFIG`: path to the beam codebook for the antenna array.
- `targetFreq`: target RF frequency in GHz. For a UD layer, this is the RF output frequency.
- `RFMode`: initial RF mode, where `0` is TX and `1` is RX.
- `spi_beamID`: initial beam ID metadata for the OAI-controlled antenna array.
- `ifFrequency`: UD intermediate frequency in GHz.
- `STATE`: initial UD hardware state applied during TLKCore initialization.

### Run the controller

The OAI `tlkcore_fbs` controller initializes all configured beamformers and downconverters, applies the beam tables, enables fast-parallel mode, and remains running while OAI controls beam IDs:

The runtime package is provided in the cloned vendor example at `examples/lib`. The `--tlkcore-root` option must point to the directory containing `tlkcore/` and `TMYConfig.py`. Do not point it to `lib_tlkcore_cpp/`, which contains only the C++ wrapper library and header.

```bash
cd radio/TMYTEK/tlkcore
../../../build-tmytek/radio/TMYTEK/tlkcore/tlkcore_fbs \
	--config config/device.conf \
	--tlkcore-root /home/user/workarea/tlkcore-examples/examples/C_Cpp/examples/lib
```

Keep this process running before and during the OAI gNB. Stop it with `Ctrl+C` or `SIGTERM`; it disables fast-parallel mode and returns the arrays to normal mode before exiting. A forced `SIGKILL` or power loss cannot perform this cleanup.

Then start OAI separately with the RU configured for TMYTEK:

```text
gpio_controller = "tmytek";
```

OAI's `trx_set_beam()` path owns runtime SPI beam switching. The OAI `tlkcore_fbs` controller only initializes the arrays and does not send runtime SPI beam IDs.

For each configured beamformer, the OAI `tlkcore_fbs` controller first reads `RFMode` from `device.conf` and initializes the beamformer in that mode using a neutral beam position. It then applies the complete beam table from `BEAM_CONFIG`. This ensures that the beam table is loaded after the antenna array has been placed in its configured TX or RX mode.

### OAI analog beamforming path

For the complete scheduler and RU-side Beam API description, see [Analog Beamforming](../../doc/analog_beamforming.md). The runtime path for this TMYTEK integration is:

```text
MAC beam selection
	-> NR PHY per-symbol beam_id table
	-> RU fh_south_ctrl()
	-> trx_set_beams()
	-> USRP X410 UHD SPI command
	-> TMYTEK beamformer phase register and latch pulse
```

The OAI `tlkcore_fbs` controller configures the antenna array over Ethernet and keeps it in fast-parallel mode. OAI then performs timestamped runtime beam switching through the integrated USRP driver. The `tlkcore_fbs` process must remain running while OAI is active; stopping it gracefully restores the beamformers to normal mode.

The `tmytek` controller currently supports one beam per USRP device and runtime TX phase steering. The OAI Beam API does not carry an RF direction, so runtime RX mode switching and independent RX beam selection are not supported.

### Setup

The reference deployment contains two independent RF chains:

- **gNB:** OAI gNB -> USRP X410 -> up/downconverter -> mmWave beamformer
- **UE:** OAI UE -> USRP X410 -> up/downconverter -> mmWave beamformer

The gNB and UE beamformers exchange the downlink signal over the air. Measurement reports travel back over the wired uplink between the OAI UE and OAI gNB. Connect each X410 to its corresponding local beamformer and downconverter, and connect each X410 to its OAI host before starting the software.

```text
				 Downlink over the air
	  +----------------------------------------------+
	  |                                              v
  +-----------+   +-----------+   +----------------+  +----------------+
  | OAI gNB   |-->| NI USRP   |-->| Up/downconverter|->| gNB beamformer |
  +-----------+   | X410      |   +----------------+  +----------------+
	  ^         +-----------+
	  | wired measurement reports
	  |
  +-----------+   +-----------+   +----------------+  +----------------+
  | OAI UE    |<--| NI USRP   |<--| Up/downconverter|<-| UE beamformer  |
  +-----------+   | X410      |   +----------------+  +----------------+
			+-----------+
```

This diagram shows the logical signal path and does not replace the hardware vendor's cabling, clocking, power, or frequency-converter configuration requirements.

### Configuration assets

The configuration template references:

- `CustomBatch8Beams_D2310E003-28.csv`
- `CustomBatchBeams_D2310L020-28.csv`

These tables are included under `tlkcore/config/`. Replace them only with beam tables authorized for the corresponding antenna serial numbers and antenna kits.

### Limitation

**Antenna Array Gain:** The antenna-array gain registers are assumed to be programmed during array initialization. The OAI runtime path writes phase-selection commands only.

**RF mode:** The TLKCore configuration can select the initial RF mode for each antenna array. However, the OAI Beam API provides only a beam identifier, antenna count, and timestamp; it does not provide an RF direction or TX/RX mode for each runtime beam update. The current USRP driver initializes its TMYTEK runtime state in TX mode, writes the TX phase register over SPI, and pulses the beamformer latch signal. RX mode switching and independent RX beam selection are therefore not supported by the current runtime Beam API.

Supporting RX beam control would require a separate RF-mode control mechanism coordinated with the RU TDD slot direction.
