#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-CSSL-1.0
"""
Exhaustive parameter-space correctness sweeps for the NR PHY simulators.

Fixes SNR at a single high-SINR point and sweeps one structural parameter
(e.g. resource-block allocation offset) across its entire legal range,
checking for bit errors, non-zero exit codes, and ASan/UBSan reports.

Usage:
  ./cmake_targets/build_oai -P --sanitize-address --build-dir physim-sweep-asan
  python3 tools/physim_sweep/physim_sweep.py \\
      --build-dir cmake_targets/ran_build/physim-sweep-asan/build

See README.md for the configuration format and how to add new sweeps.
"""
import argparse
import concurrent.futures
import dataclasses
import os
import re
import subprocess
import sys
import time
import xml.etree.ElementTree as ET
from pathlib import Path

import yaml

# UBSan violations are a standalone "<file>:<line>: runtime error: ..." line,
# not an "ERROR: <Sanitizer>" banner like ASan/LSan.
SANITIZER_ERROR_RE = re.compile(r"ERROR: (AddressSanitizer|LeakSanitizer|UndefinedBehaviorSanitizer)"
                                 r"|^.+: runtime error: .+$", re.MULTILINE)
ANSI_ESCAPE_RE = re.compile(r"\x1b\[[0-9;]*m")
DEFAULT_ASAN_OPTIONS = "abort_on_error=0:halt_on_error=1:print_stats=0"
DEFAULT_UBSAN_OPTIONS = "halt_on_error=1:print_stacktrace=1"

# Some simulators loop `SNR < snr1` (strict); snr0 == snr1 would run zero
# trials. The epsilon guarantees exactly one iteration at the requested SINR.
SINR_EPSILON = 0.05
SNR_FLAG = "-s"
SNR_END_FLAG = "-S"


@dataclasses.dataclass
class RunResult:
    channel: str
    group: str
    argv: list
    returncode: int
    duration: float
    ok: bool
    reason: str
    log_path: Path = None


def expand_axis(axis: dict, stride: int) -> list:
    if "values" in axis:
        values = list(axis["values"])
    elif "range" in axis:
        start, stop = axis["range"]
        step = axis.get("step", 1)
        values = list(range(start, stop + 1, step))
    else:
        raise ValueError(f"axis must have 'values' or 'range': {axis}")
    if stride > 1:
        values = values[::stride]
    return values


def axis_args(flag, value) -> list:
    # flag/value as lists sweeps several flags together as one point.
    if isinstance(flag, list):
        args = []
        for f, v in zip(flag, value):
            args += [f, str(v)]
        return args
    return [flag, str(value)]


def axis_label(flag, value) -> str:
    if isinstance(flag, list):
        return "_".join(f"{f.lstrip('-')}{v}" for f, v in zip(flag, value))
    return f"{flag.lstrip('-')}{value}"


def build_argv(binary_path: Path, fixed_args: list, sinr: float, flag, value) -> list:
    return ([str(binary_path)] + [str(a) for a in fixed_args]
             + [SNR_FLAG, str(sinr), SNR_END_FLAG, str(sinr + SINR_EPSILON)]
             + axis_args(flag, value))


def run_one(channel: str, group: str, binary_path: Path, fixed_args: list, sinr: float, flag: str, value,
            build_dir: Path, log_dir: Path, timeout: float) -> RunResult:
    argv = build_argv(binary_path, fixed_args, sinr, flag, value)
    env = os.environ.copy()
    env["LD_LIBRARY_PATH"] = f"{build_dir}:{env.get('LD_LIBRARY_PATH', '')}"
    env["ASAN_OPTIONS"] = env.get("ASAN_OPTIONS", DEFAULT_ASAN_OPTIONS)
    env["UBSAN_OPTIONS"] = env.get("UBSAN_OPTIONS", DEFAULT_UBSAN_OPTIONS)

    start = time.monotonic()
    try:
        proc = subprocess.run(argv, env=env, cwd=build_dir, capture_output=True, text=True,
                               timeout=timeout)
        returncode = proc.returncode
        output = proc.stdout + proc.stderr
        timed_out = False
    except subprocess.TimeoutExpired as e:
        returncode = -1

        def _to_text(x):
            return x.decode(errors="replace") if isinstance(x, bytes) else (x or "")
        output = _to_text(e.stdout) + _to_text(e.stderr)
        timed_out = True
    output = ANSI_ESCAPE_RE.sub("", output)
    duration = time.monotonic() - start

    if timed_out:
        ok, reason = False, f"timed out after {timeout:.0f}s"
    elif SANITIZER_ERROR_RE.search(output):
        ok, reason = False, SANITIZER_ERROR_RE.search(output).group(0)
    elif returncode != 0:
        ok, reason = False, f"nonzero exit code {returncode}"
    else:
        ok, reason = True, "ok"

    log_path = None
    if not ok:
        log_dir.mkdir(parents=True, exist_ok=True)
        log_path = log_dir / f"{channel}.{group}.{axis_label(flag, value)}.log"
        log_path.write_text("$ " + " ".join(argv) + "\n\n" + output)

    return RunResult(channel, group, argv, returncode, duration, ok, reason, log_path)


def load_config(path: Path) -> dict:
    with open(path) as f:
        return yaml.safe_load(f)


def select_groups(config: dict, channels: list, groups: list):
    selected = []
    for channel, cdef in config["channels"].items():
        if channels and channel not in channels:
            continue
        for g in cdef["groups"]:
            if groups and g["name"] not in groups:
                continue
            selected.append((channel, cdef, g))
    return selected


def write_junit(results: list, out_path: Path):
    by_group = {}
    for r in results:
        by_group.setdefault((r.channel, r.group), []).append(r)

    testsuites = ET.Element("testsuites")
    by_channel = {}
    for (channel, group), rs in by_group.items():
        by_channel.setdefault(channel, []).append((group, rs))

    for channel, groups in by_channel.items():
        n_tests = sum(len(rs) for _, rs in groups)
        n_fail = sum(1 for _, rs in groups for r in rs if not r.ok)
        ts = ET.SubElement(testsuites, "testsuite", name=channel,
                            tests=str(n_tests), failures=str(n_fail))
        for group, rs in groups:
            n_fail_g = sum(1 for r in rs if not r.ok)
            duration = sum(r.duration for r in rs)
            tc = ET.SubElement(ts, "testcase", classname=f"physim_sweep.{channel}",
                                name=group, time=f"{duration:.2f}")
            if n_fail_g:
                failing = [r for r in rs if not r.ok]
                msg = f"{n_fail_g}/{len(rs)} points failed"
                detail_lines = []
                for r in failing[:20]:
                    detail_lines.append(f"  {' '.join(r.argv)} -> {r.reason} ({r.log_path})")
                if len(failing) > 20:
                    detail_lines.append(f"  ... and {len(failing) - 20} more")
                failure = ET.SubElement(tc, "failure", message=msg)
                failure.text = "\n".join(detail_lines)
    ET.ElementTree(testsuites).write(out_path, encoding="unicode", xml_declaration=True)


def main():
    script_dir = Path(__file__).resolve().parent
    p = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--build-dir", type=Path, required=True,
                    help="Directory containing the ASAN-built physim executables "
                         "(e.g. cmake_targets/ran_build/physim-sweep-asan/build)")
    p.add_argument("--config", type=Path, default=script_dir / "sweeps.yaml",
                    help="Sweep configuration file (default: sweeps.yaml next to this script)")
    p.add_argument("--channel", action="append", default=[],
                    help="Only run this channel (repeatable). Default: all channels.")
    p.add_argument("--group", action="append", default=[],
                    help="Only run this group name (repeatable). Default: all groups.")
    p.add_argument("--list", action="store_true", help="List selected channels/groups and exit")
    p.add_argument("--stride", type=int, default=1,
                    help="Only run every Nth point of each swept axis, for a quick smoke run "
                         "(default: 1, i.e. exhaustive)")
    p.add_argument("--jobs", type=int, default=os.cpu_count() or 4,
                    help="Number of simulator runs to execute in parallel")
    p.add_argument("--timeout", type=float, default=120.0,
                    help="Per-run timeout in seconds")
    p.add_argument("--log-dir", type=Path, default=Path("physim_sweep_results"),
                    help="Directory to store logs of failing runs")
    p.add_argument("--junit", type=Path, default=None,
                    help="Write a JUnit XML report to this path")
    p.add_argument("--fail-fast", action="store_true",
                    help="Stop submitting new runs after the first failure")
    p.add_argument("-v", "--verbose", action="store_true", help="Print every run, not just failures")
    args = p.parse_args()
    args.build_dir = args.build_dir.resolve()

    config = load_config(args.config)
    selection = select_groups(config, args.channel, args.group)
    if not selection:
        print("No channels/groups matched the given --channel/--group filters", file=sys.stderr)
        return 1

    for channel, cdef, group in selection:
        if "sinr" not in group:
            print(f"{channel}.{group['name']}: group is missing required 'sinr' key", file=sys.stderr)
            return 1

    jobs = []
    for channel, cdef, group in selection:
        binary_path = args.build_dir / cdef["binary"]
        values = expand_axis(group["axis"], args.stride)
        for value in values:
            jobs.append((channel, cdef, group, binary_path, value))

    if args.list:
        for channel, cdef, group, binary_path, value in jobs:
            argv = build_argv(binary_path, group["fixed_args"], group["sinr"], group["axis"]["flag"], value)
            print(f"{channel}.{group['name']}: {' '.join(argv)}")
        print(f"\n{len(jobs)} runs total", file=sys.stderr)
        return 0

    missing = sorted({str(binary_path) for _, _, _, binary_path, _ in jobs if not binary_path.is_file()})
    if missing:
        print("Missing simulator executables (build with --sanitize-address first):", file=sys.stderr)
        for m in missing:
            print(f"  {m}", file=sys.stderr)
        return 1

    print(f"Running {len(jobs)} sweep points across {len(selection)} groups "
          f"(build-dir={args.build_dir}, jobs={args.jobs}, stride={args.stride})")

    results = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as ex:
        futures = {}
        for channel, cdef, group, binary_path, value in jobs:
            fut = ex.submit(run_one, channel, group["name"], binary_path, group["fixed_args"], group["sinr"],
                             group["axis"]["flag"], value, args.build_dir, args.log_dir, args.timeout)
            futures[fut] = (channel, group["name"], axis_label(group["axis"]["flag"], value))

        for fut in concurrent.futures.as_completed(futures):
            channel, group_name, label = futures[fut]
            try:
                r = fut.result()
            except Exception as e:
                r = RunResult(channel, group_name, [], -1, 0.0, False, f"tool error: {e!r}", None)
            results.append(r)
            if not r.ok:
                print(f"FAIL {r.channel}.{r.group} {label}: {r.reason} "
                      f"[{' '.join(r.argv)}]")
                if r.log_path:
                    print(f"     log: {r.log_path}")
                if args.fail_fast:
                    for other in futures:
                        other.cancel()
                    break
            elif args.verbose:
                print(f"ok   {r.channel}.{r.group} {label} ({r.duration:.1f}s)")

    n_fail = sum(1 for r in results if not r.ok)
    n_ran = len(results)
    print(f"\n{n_ran - n_fail}/{n_ran} points passed"
          + (f", {n_fail} FAILED" if n_fail else ""))

    if args.junit:
        write_junit(results, args.junit)
        print(f"JUnit report written to {args.junit}")

    return 1 if n_fail else 0


if __name__ == "__main__":
    sys.exit(main())
