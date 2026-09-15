#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-CSSL-1.0
"""Launch an OAI UE with initial NTN frequency, timing, and position hints."""

import argparse
import bisect
import csv
import math
import os
from pathlib import Path
import re
import stat
import sys
import tempfile

from ntn_position import geodetic_to_ecef, load_position


def load_profile(path):
    start_unix_s = None
    initial_dl_service_doppler_hz = None
    rows = []
    with open(path, encoding="utf-8") as stream:
        for line_number, line in enumerate(stream, 1):
            stripped = line.strip()
            if stripped.startswith("# start_unix_s="):
                try:
                    start_unix_s = float(stripped.split("=", 1)[1])
                except ValueError as error:
                    raise ValueError(f"invalid profile epoch on line {line_number}") from error
            elif stripped.startswith("# initial_dl_service_doppler_hz="):
                try:
                    initial_dl_service_doppler_hz = float(stripped.split("=", 1)[1])
                except ValueError as error:
                    raise ValueError(f"invalid initial DL service Doppler on line {line_number}") from error
            elif stripped and not stripped.startswith("#"):
                try:
                    row = tuple(float(value) for value in next(csv.reader([stripped])))
                except ValueError as error:
                    raise ValueError(f"invalid profile row on line {line_number}") from error
                if len(row) != 4 or not all(math.isfinite(value) for value in row):
                    raise ValueError(f"invalid profile row on line {line_number}")
                if rows and row[0] <= rows[-1][0]:
                    raise ValueError(f"non-increasing profile time on line {line_number}")
                rows.append(row)

    if start_unix_s is None or not math.isfinite(start_unix_s):
        raise ValueError("profile has no valid # start_unix_s metadata")
    if len(rows) < 2:
        raise ValueError("profile must contain at least two rows")
    if initial_dl_service_doppler_hz is not None and not math.isfinite(initial_dl_service_doppler_hz):
        raise ValueError("profile has invalid initial DL service Doppler metadata")
    return start_unix_s, initial_dl_service_doppler_hz, rows


def initial_ue_hints(rows, initial_dl_service_doppler_hz=None):
    offset_s = 0.0
    times = [row[0] for row in rows]
    if offset_s < times[0] or offset_s > times[-1]:
        raise ValueError(
            f"simulation start offset {offset_s:.3f} s is outside "
            f"[{times[0]:.3f}, {times[-1]:.3f}] s"
        )

    index = bisect.bisect_right(times, offset_s) - 1
    left = index
    right = index + 1
    if right == len(rows):
        left -= 1
        right -= 1
    drift_us_per_s = 1000.0 * (rows[right][1] - rows[left][1]) / (rows[right][0] - rows[left][0])
    initial_fo = initial_dl_service_doppler_hz if initial_dl_service_doppler_hz is not None else rows[index][2]
    return offset_s, initial_fo, drift_us_per_s


def has_option(command, option):
    return any(argument == option or argument.startswith(f"{option}=") for argument in command)


def update_ue_position(config_path, ecef):
    path = Path(config_path)
    text = path.read_text(encoding="utf-8")
    section_pattern = re.compile(r"(?ms)(^\s*position0\s*=\s*\{)(.*?)(^[ \t]*\}[ \t]*;?)")
    sections = list(section_pattern.finditer(text))
    if len(sections) != 1:
        raise ValueError(f"expected exactly one position0 block in {config_path}")

    section = sections[0]
    body = section.group(2)
    for coordinate, value in zip(("x", "y", "z"), ecef):
        coordinate_pattern = re.compile(rf"(?m)^(\s*{coordinate}\s*=\s*)[^;]+(;.*)$")
        body, replacements = coordinate_pattern.subn(rf"\g<1>{value:.3f}\g<2>", body)
        if replacements != 1:
            raise ValueError(f"expected exactly one {coordinate} coordinate in position0 block of {config_path}")

    closing = section.group(3)
    if not closing.rstrip().endswith(";"):
        closing = closing.rstrip() + ";"
    updated = text[:section.start()] + section.group(1) + body + closing + text[section.end():]
    mode = stat.S_IMODE(path.stat().st_mode)
    temporary_path = None
    try:
        with tempfile.NamedTemporaryFile("w", encoding="utf-8", dir=path.parent,
                                         prefix=f".{path.name}.", delete=False) as stream:
            temporary_path = Path(stream.name)
            stream.write(updated)
            stream.flush()
            os.fsync(stream.fileno())
        temporary_path.chmod(mode)
        os.replace(temporary_path, path)
    finally:
        if temporary_path and temporary_path.exists():
            temporary_path.unlink()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", required=True, help="UTC-dated NTN profile CSV")
    parser.add_argument("--ue-position-file", help="OCUDU ue-position.cfg path")
    parser.add_argument("--ue-config", help="UE libconfig file whose position0 block will be updated")
    parser.add_argument("ue_command", nargs=argparse.REMAINDER, help="UE command, preceded by --")
    args = parser.parse_args()
    command = args.ue_command
    if command and command[0] == "--":
        command = command[1:]
    if not command:
        parser.error("missing UE command after --")
    if bool(args.ue_position_file) != bool(args.ue_config):
        parser.error("--ue-position-file and --ue-config must be used together")
    if args.ue_config and has_option(command, "-O"):
        parser.error("UE command must not set -O when --ue-config is used")
    try:
        _, initial_dl_service_doppler_hz, rows = load_profile(args.profile)
        offset_s, initial_fo, time_drift = initial_ue_hints(rows, initial_dl_service_doppler_hz)
        position = load_position(args.ue_position_file) if args.ue_position_file else None
        ecef = geodetic_to_ecef(*position) if position else None
        if ecef:
            update_ue_position(args.ue_config, ecef)
    except (OSError, ValueError) as error:
        parser.error(str(error))

    if args.ue_config:
        command.extend(["-O", args.ue_config])
    if not has_option(command, "--initial-fo"):
        command.extend(["--initial-fo", f"{initial_fo:.2f}"])
    if not has_option(command, "--ntn-initial-time-drift"):
        command.extend(["--ntn-initial-time-drift", f"{time_drift:.3f}"])
    print(f"NTN simulation-start offset {offset_s:.3f} s: "
          f"initial FO {initial_fo:.2f} Hz, "
          f"time drift {time_drift:.3f} us/s", file=sys.stderr, flush=True)
    if position:
        print(f"OCUDU UE position {args.ue_position_file}: lat {position[0]:.6f}, lon {position[1]:.6f}, "
              f"alt {position[2]:.3f} m; updated {args.ue_config} position0 to "
              f"ECEF {ecef[0]:.3f}, {ecef[1]:.3f}, {ecef[2]:.3f} m",
              file=sys.stderr, flush=True)
    os.execvp(command[0], command)


if __name__ == "__main__":
    main()
