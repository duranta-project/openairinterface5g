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
