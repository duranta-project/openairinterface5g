<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# Tools for performance monitoring

Several tools can be used for performance analysis of OpenAirInterface.
Different tools have different purposes.

## Debugging purpose

* A profiler such as [perf](https://perfwiki.github.io/main/) enables to
  dissect the contribution of processes and their different parts to processing
  times and computing resource usage.
* A tracers such as [Tracy](#performance-analysis-with-the-tracy-profiler)
  enables an inclusive and in depth monitoring of the performance of a
  specified part of software.

## Benchmarking purpose

* The [time stats tool](#processing-times-statistics-with-the-time-stats) is an
  embedded processing time recorder for macroscopic computing blocks.  It is
  thought for high-level benchmarking and research purposes as it provides a
  high-level view of processing times of typical air interface processing
  blocks.
* The [real-time probes](#real-time-behavior-analysis-with-the-real-time-probes)
  are a tool for the extensive recording of completion times and context
  elements of the processing of complete layers of the stack for their
  offline analysis

## Performance analysis with the Tracy profiler

### Overview

From the Tracy manual:

> Tracy is a real-time, nanosecond resolution hybrid frame and sampling
> profiler that you can use for remote or embedded telemetry of games and other
> applications. It can profile CPU, GPU, memory allocations, locks, context
> switches, [...]

- Sources are on [Github](https://github.com/wolfpld/tracy)
- There is a [web demo](https://tracy.nereid.pl/)
- You can [watch an intro video](https://youtu.be/ghXk3Bk5F2U?t=37)
- You can [read the manual](https://github.com/wolfpld/tracy/releases/download/v0.13.1/tracy.pdf)

### OAI Integration

To enable Tracy, compile `-DTRACY_ENABLE=ON` in cmake. Note that `build_oai`
has no native switch, but you can use `--cmake-opt -DTRACY_ENABLE=ON` instead.

Furthermore, you will need the Tracy profiler:

- Windows hosts: There is a precompiled `tracy-profiler.exe` on Github
- Linux hosts: Compile tracy-profiler from source as described in the manual.

Start the OAI executable you want to profile. Then, open the profiler, and
click on connect to connect to the executable.

It is also possible to collect data from within docker containers:

- Open port 8086.
- To collect CPU data, make sure that you run docker with `--privileged --mount
  "type=bind,source=/sys/kernel/debug,target=/sys/kernel/debug,readonly" --user
  0:0 --pid=host` or provide the corresponding options in docker-compose.

### Instrumentation

Instrumentation is done via the header `common/instrumentation.h`. A couple of
places in OAI have been instrumented already, search for the macros mentioned
in `common/instrumentation.h`.

In short, main features already in use:

- Measure specific code regions by surrounding them with `TracyCZone(ctx, true);`
  and `TracyCZoneEnd(ctx);`.
- Record individual "Tracy frames" (in the OAI context, that's likely one 4G/5G
  slot) with `TracyCFrameMark;`
- Plot values using `TracyCPlot(name, val);`

More information about these macros can be found in the manual.

Make sure to link `utils` into the static library you are modifying to get
tracy header definitions.

## Processing times statistics with the time stats

### Purpose

The time stats tool is thought as an internal tool for recording statistics on
processing stats of macroscopic computing blocks mostly for research purposes.
As internal tool, it is or can be freely adapted to measure timings according
to specific needs of researchers or engineers.

**It is not** intended for debugging. For performance debugging tools, refer to
the [debugging tools section](#debugging-purpose).

### Features

* Timer based on Posix real time clocks.
* Accumulate successive measurements. Merge function enables to merge many
  timers measuring parallel processes.
* Provides average time, standard deviation and, optionally, time distribution.
* May be started and stopped on full DL or UL slots only for clean and relevant
  measurements.
* Already embedded in the stack to provide processing times of typical air
  interface processing blocks.

### Usage

A number of timers are already included in the nr-softmodem and PHY simulators
to provide statistics on the main macroscopic blocks:

* In PHY simulators, option `-P` enables to display the statistics in the log.
* In the nr-softmodem, the time statistics are displayed in file
  `nrL1_stats.log` with a refresh every second when option `-q` is provided.
  This option may take an argument to display different statistics:
  * Option `-q` or `-q 1` displays average, standard deviation and maximum of
    the measured times from softmodem start.
  * Option `-q 2` displays average, standard deviation and distribution of the
    measured times recorded over one second.

The time stats tool is available through the header
[time_meas.h](../../common/utils/time_meas.h).  It is implemented in this
header and in the source file [time_meas.c](../../common/utils/time_meas.c).  A
timer is a typedef struct `time_stats_t` that can be started with `start_meas`,
stopped with `stop_meas`, merged in another timer with `merge_meas` and reset
with `reset_meas`.  The start, stop and merge functions have `_on_dl` and
`_on_ul` variants to measure only full DL or UL slots.  Tracking the full
distibution of processing times is optional and requires to enable the sorted
list of the timer with `init_time_stats_sorted_list`.  Then the list shall be
released with `free_time_stats_sorted_list` when finishing to use it.

## Real-Time behavior analysis with the real-time probes

### Purpose

The real-time probes are an internal tool for the extensive recording of
completion times and context elements of the processing
of complete layers of the stack for their offline analysis.
The combination of processing time and context enable to perform complete
analysis of the real-time behavior of processing in a relevant context
including through long runs.
As internal tool, it is or can be freely adapted to measure timings according
to specific needs of researchers or engineers.
The current integration includes measurement of the complete
physical layer of the NR softmodem in the uplink and downlink directions.

The tool is not primarily intended for debugging but may be used for
monitoring the relation between computing context and computing time outliers.
For performance debugging tools, refer to
the [debugging tools section](#debugging-purpose).

### Features

* Uses the time stats timer for time recoding.
  It is based on Posix real time clocks.
* Optional live real-time statistics with configurable deadline and histogram.
* Optional capture ability with full log of processing time and context
  per slot. Asynchronous dump to csv file.
* Provided capture analysis tool as python scripts.
  Provides summary as text and json and cummulative distribution function
  plots with configurable deadlines.

### Usage

Full statistic and capture abilities are integrated in the stack
for physical layer (L1) transmission (L1TX) and reception (L1RX).
This ability can be enabled and configured in sections
`rt_probe_l1tx` and `rt_probe_l1rx` of the configuration file.  
Live stats paprameters:
* `stats_enabled`
  1 to enable live stats, 0 (default) to disable
* `report_period`
  records period for displayin the live stats (defaults to 20000)
* `late_threshold_us`
  configurable thresholds in microseconds for counting records
  as late in the live stats
* `threshold0_us`
  first thresholds in microseconds for overrun count
  and histogram in the live stats
* `threshold1_us`
  second thresholds in microseconds for overrun count
  and histogram in the live stats
* `threshold2_us`
  third thresholds in microseconds for overrun count
  and histogram in the live stats
* `threshold3_us`
  fourth thresholds in microseconds for overrun count
  and histogram in the live stats
Capture parameters:
* `capture_enabled`
  1 to enable capture, 0 (default) to disable
* `capture_async_flush_enabled`
  1 to enable asynchronous flush of captured records, 0 (default) to disable
* `capture_final_dump_enabled`
  1 to enable final dump of captured records, 0 (default) to disable
* `capture_records`
  number of records in the records buffer (defaults to 20000)

example:
```
rt_probe_l1tx = {
  stats_enabled = 1;
  report_period = 20000;
  late_threshold_us = 1000;
  threshold0_us = 250;
  threshold1_us = 375;
  threshold2_us = 500;
  threshold3_us = 675;
  capture_enabled = 1;
  capture_async_flush_enabled = 1;
  capture_final_dump_enabled = 1;
  capture_records = 20000;
};

rt_probe_l1rx = {
  stats_enabled = 1;
  report_period = 20000;
  late_threshold_us = 1000;
  threshold0_us = 500;
  threshold1_us = 1000;
  threshold2_us = 1500;
  threshold3_us = 2000;
  capture_enabled = 1;
  capture_async_flush_enabled = 1;
  capture_final_dump_enabled = 1;
  capture_records = 20000;
};
```

Captures are written to `/tmp/rt_probe_l1tx_records.csv` and `/tmp/rt_probe_l1rx_records.csv`.
The capture analysis tools are located in `tools/rt_deadline/`.

Typical analysis commands are:
```
python3 tools/rt_deadline/analyze_l1tx_capture.py /tmp/rt_probe_l1tx_records.csv --slot-us 500 --deadline-us 1000 --plot-ecdf --plot-ccdf --output-dir analysis_tx --json-summary analysis_tx/l1tx_summary.json | tee analysis_tx/l1tx_summary.txt
```

```
python3 tools/rt_deadline/analyze_l1rx_capture.py /tmp/rt_probe_l1rx_records.csv --slot-us 500 --deadline-us 1000 --plot-ecdf --plot-ccdf --output-dir analysis_rx --json-summary analysis_rx/l1rx_summary.json | tee analysis_rx/l1rx_summary.txt
```

The real-time probes are available through the header
[rt_probe.h](../../common/utils/rt_probe.h).  Thay are implemented in this
header and in the source file [rt_probe.c](../../common/utils/rt_probe.c).  A
probe is a typedef struct `rt_probe_t` that can record a record for statistics
summary with `rt_probe_record`, and/or capture with `rt_probe_capture_record`.
The probe should be initialized with `rt_probe_init` and configured with
`rt_probe_set_config`. Configuration can be loaded from the configuration
file with `rt_probe_load_config`. To capture context, it is necessary to use
also  `rt_probe_set_capture_schema` which sets the schema or type of the probe
between L1TX and L1RX (schema `RT_DEADLINE_CAPTURE_SCHEMA_L1TX` resp
`RT_DEADLINE_CAPTURE_SCHEMA_L1RX,`),
