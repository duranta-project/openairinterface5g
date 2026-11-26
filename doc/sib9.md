<!-- SPDX-License-Identifier: CC-BY-4.0 -->
# Basic SIB9: static UTC and system UTC

SIB9 is enabled only when `cu_sibs` contains 9. The CU builds a standard SIB9 and
sends its UPER container in F1 Setup. The DU validates it, builds SIB1 and the SI
messages, and may refresh basic UTC from its own system clock. A monolithic gNB
uses the same construction/publication path without the SCTP round trip.

## Configuration

In the active CU `gNBs` entry:

```text
cu_sibs = (2, 9);
sib9_config = {
  timeSource = "static";
  timeInfoUTC = 390898880000L;
};
```

In the corresponding DU `gNBs` entry:

```text
sib9_timing = { mode = "static"; };
```

This example repeats a fixed value corresponding to 2023-11-14 22:13:20 UTC.
It is a decoder fixture, not a clock that advances with elapsed time.

For system UTC, configure the CU with:

```text
cu_sibs = (2, 9);
sib9_config = { timeSource = "realtime"; };
```

Configure each DU independently with:

```text
sib9_timing = { mode = "realtime"; };
```

The CU samples its clock to build the initial container. A realtime DU replaces
that value before transmission. The refresh policy is local configuration; F1
carries only the standard SIB9 fields. A static DU repeats the received value, including
when its CU initially sampled realtime. Use matching configurations for the
intended operation. A remote DU reads its own host clock, not the CU or 5GC clock.
For a monolithic gNB include both blocks in its active `gNBs` entry.

| Configuration | Scope | Default / requirement | Wire meaning |
|---|---|---|---|
| `cu_sibs` contains 9 | CU | Optional; feature otherwise disabled | Standard SIB9 container |
| `timeSource` | CU | `static`; allowed values `static`, `realtime` | Local construction policy only |
| `timeInfoUTC` | CU | Required for static, forbidden for realtime | 0..549755813887, 10-ms units since 1900-01-01 |
| `dayLightSavingTime` | CU | Absent unless supplied; 0..2 | Optional two-bit field; 3 is reserved |
| `leapSeconds` | CU | Absent unless supplied; -127..128 | Optional GPS-minus-UTC metadata |
| `localTimeOffset` | CU | Absent unless supplied; -63..64 | Optional offset in 15-minute units |
| `sib9_timing.mode` | DU | `static`; allowed values `static`, `realtime` | Local refresh policy only |

Supplied zero metadata is present, while omission remains absent. There are no
negative omission aliases. No leap value, timezone or daylight-saving information
is inferred from the operating system. `CLOCK_REALTIME` is converted from Unix
UTC directly; leap metadata is not added to that conversion. Zero UTC is valid
content. Configuration changes require the normal restart/setup procedure.

`sib9_config` supports basic `timeInfo` and the optional metadata listed above.
ReferenceTimeInfo-r16, TSS events and late extensions have no configuration
interface. The existing ASN.1 grammar is unchanged. Input containing a parallel
R16 reference clock is rejected because basic UTC refresh cannot keep it consistent.

## Realtime and failure behaviour

The reference instant is the first radio-frame boundary at or after the end of
the actual SIB9 SI window. One CLOCK_REALTIME read is projected from estimated
RX-buffer end through the known RX-to-TX scheduling lead to that instant. The
result is rounded down to the field's 10-ms units. Repetitions for multiple SSBs
within the same window reuse identical bytes. Each later window samples again;
forward/backward host-clock steps are reflected at the next fresh window.

The inherited scheduling default is rf16, or 160 ms. The calculation reads the
actual SIB1 window length and scheduler occasion; other periodicities use their configured cadence.
Timestamp-only refreshes change neither the SIB1 valueTag nor SI notifications.

A failed clock read, conversion or encoding skips SIB9 for that window. It does
not reuse the previous window or CU seed, and the following window retries.
SIB2/3/4 have a separate SI payload and continue independently. SIB9 remains
scheduled in SIB1 during a skipped window; automatic availability withdrawal requires a separate SI-change procedure.

Malformed, duplicate, unsupported or oversized F1 SI candidates are rejected as
a complete list. The old coherent SIB1/SI snapshot remains active. Initial
rejection does not release the DU's radio-activation gate. Received buffers are
not retained: decoded trees are owned per cell and freed on replacement/teardown.
An F1 Setup replay resets the window cache, even for identical content. F1 UE-only
Reset is not treated as removal of broadcast cell system information.

Realtime requires the integrated L1 RX/TX slot context. External nFAPI indications
without that local context do not provide a realtime source. Radio-plugin ABI,
UHD rate conversion, 5GC, host clock services and nFAPI/F1 wire formats are unchanged.

CPU scheduling, buffering, transport and RF delay contribute unmeasured error to
the RX-end estimate. The timestamp follows CLOCK_REALTIME, including clock steps
and loss of external synchronization. RF timing accuracy, PTP health and inter-DU
alignment require separate measurements. The receiver scenarios below use
independent SA UEs and do not exercise NR-DC activation.

## Build and reproduce

Use an out-of-source CMake/Ninja build and the standard OAI dependencies:

```sh
cmake -S . -B build -GNinja -DENABLE_TESTS=ON -DE2_AGENT=OFF \
  -DOAI_USRP=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSIB9_TEST_SANITIZERS=ON
cmake --build build --parallel 4 --target tests nr-softmodem nr-uesoftmodem \
  nr-cuup rfsimulator oai_usrpdevif
ctest --test-dir build -E '^physim\.|^benchmark_' --output-on-failure
ctest --test-dir build -R '^benchmark_sib9$' -V
```

The sanitizer option instruments the test-owned production translation units,
not the complete modem or generated ASN.1 library. The benchmark is built without
that instrumentation and reports host refresh/cache costs, not RF performance.

Complete loopback-only CU/two-DU configurations are under
`ci-scripts/conf_files/sib9`. Generate matching static, realtime or disabled
profiles without launching processes:

```sh
python3 ci-scripts/run_sib9.py --build build --mode static \
  --configs-only --output /tmp/sib9-static-configs
python3 ci-scripts/run_sib9.py --build build --mode realtime \
  --configs-only --output /tmp/sib9-realtime-configs
```

Use fresh output directories. These commands start RF-simulator processes, save
receiver logs and stop the processes when the run finishes:

```sh
python3 ci-scripts/run_sib9.py --build build --mode static --output /tmp/sib9-static-test
python3 ci-scripts/run_sib9.py --build build --mode realtime --output /tmp/sib9-realtime-test
python3 ci-scripts/run_sib9.py --build build --mode disabled --output /tmp/sib9-disabled-test
python3 ci-scripts/run_sib9.py --build build --mode realtime \
  --topology monolithic --output /tmp/sib9-monolithic-test
```

The runner sets core/memlock limits only in its children for local simulation.
There is no AMF or registration/data-session claim. It checks real stock-UE XER
for SIB1 cell identity/mapping, SIB2 and SIB9, using two fresh acquisitions. Static
UTC must equal the fixture; realtime must advance and fall inside a broad host
calendar envelope. That envelope is not a timing-accuracy tolerance. Disabled
runs observe healthy SIB2 and the absence of SIB9 mapping/reception.

To execute the container testcase with freshly built matching binaries/plugins:

```sh
python3 ci-scripts/make_sib9_runtime.py --build build \
  --output /tmp/sib9-image --tag sib9-basic:local
export SIB9_BUILD="$(realpath build)"
export SIB9_CI_OUTPUT=/tmp/sib9-container-test
export SIB9_CI_IMAGE=sib9-basic:local
cd ci-scripts
python3 main.py --repository=NONE --branch=sib9-basic --targetBranch=NONE \
  --workspace="$(realpath ..)" --local xml_files/container_sib9_basic.xml
```

The image packages the native build and its shared libraries for this RF-simulator
test; it does not exercise the production Dockerfiles. The runner, configurations
and XML testcase are included in the checkout.

Check the runner's cleanup paths with:

```sh
python3 -m unittest discover -s ci-scripts/tests -p test_run_sib9.py -v
```

See [design and requirement mapping](sib9-design.md) for source clauses, ownership,
projection arithmetic and test locations.
