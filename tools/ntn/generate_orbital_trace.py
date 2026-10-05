#!/usr/bin/env python3
"""LEO orbital trace generator using Skyfield + SGP4."""

from __future__ import annotations

import argparse
import csv
import os

import numpy as np
from skyfield.api import EarthSatellite, load, wgs84
from skyfield.framelib import itrs

from trace_utils import (
    ecef_to_geodetic_wgs84,
    parse_position_triplet,
)

DEFAULT_START_OFFSET_S = 385.0
DEFAULT_GND_ECEF_M = (0.0, 0.0, 6378137.0)

DEFAULT_TLE = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "data", "default_leo600.tle"
)


def load_tle_entries(path: str) -> list[tuple[str, str, str]]:
    """Load all TLE entries from a file."""
    with open(path) as f:
        lines = [line.strip() for line in f if line.strip()]

    entries = []
    i = 0
    while i < len(lines):
        if lines[i].startswith("1 "):
            entries.append(("UNKNOWN", lines[i], lines[i + 1]))
            i += 2
        else:
            entries.append((lines[i], lines[i + 1], lines[i + 2]))
            i += 3

    if not entries:
        raise ValueError(f"No valid TLE entries in {path}")
    return entries


def find_pass_culmination(sat, station, ts, t0):
    """Find the first pass culmination after t0."""
    t1 = ts.tt_jd(t0.tt + 1.0)
    t_ev, events = sat.find_events(station, t0, t1, altitude_degrees=0.0)
    culms = t_ev[events == 1]
    if len(culms) == 0:
        raise RuntimeError("No satellite pass found within 1 day of TLE epoch")
    return culms[0]


def resolve_station(args) -> dict[str, object]:
    """Resolve ground station from CLI args."""
    if args.gnd_position is not None and args.station_ecef is not None:
        raise ValueError("Use either --gnd-position or --station-ecef, not both")

    if args.gnd_position is not None:
        x_m, y_m, z_m = parse_position_triplet(args.gnd_position)
    elif args.station_ecef is not None:
        x_m, y_m, z_m = tuple(args.station_ecef)
    else:
        x_m, y_m, z_m = DEFAULT_GND_ECEF_M

    lat_deg, lon_deg, alt_m = ecef_to_geodetic_wgs84(x_m, y_m, z_m)

    return {
        "lat_deg": float(lat_deg),
        "lon_deg": float(lon_deg),
        "alt_m": float(alt_m),
        "ecef_m": (float(x_m), float(y_m), float(z_m)),
    }


def generate_trace(
    sat,
    ts,
    start_time,
    granularity: str,
    duration: int,
) -> tuple[list[dict], str]:
    """Generate orbital trace rows (absolute ITRS/ECEF state only)."""
    n = duration + 1

    if granularity == "ms":
        dt_s = np.arange(n) * 0.001
        time_col = np.arange(n)
        time_label = "time_ms"
    else:
        dt_s = np.arange(n, dtype=float)
        time_col = np.arange(n)
        time_label = "time_s"

    times = ts.tt_jd(start_time.tt + dt_s / 86400.0)

    geocentric = sat.at(times)
    sat_pos, sat_vel = geocentric.frame_xyz_and_velocity(itrs)
    sat_pos_km = sat_pos.km
    sat_vel_kps = sat_vel.km_per_s

    rows = []
    for i in range(n):
        rows.append(
            {
                time_label: int(time_col[i]),
                "pos_x": round(float(sat_pos_km[0, i] * 1e3), 3),
                "pos_y": round(float(sat_pos_km[1, i] * 1e3), 3),
                "pos_z": round(float(sat_pos_km[2, i] * 1e3), 3),
                "vel_x": round(float(sat_vel_kps[0, i] * 1e3), 3),
                "vel_y": round(float(sat_vel_kps[1, i] * 1e3), 3),
                "vel_z": round(float(sat_vel_kps[2, i] * 1e3), 3),
            }
        )
    return rows, time_label


def main():
    parser = argparse.ArgumentParser(
        description="Generate LEO orbital trace using Skyfield + SGP4"
    )
    parser.add_argument(
        "-g",
        "--granularity",
        choices=["ms", "s"],
        default="s",
        help="Time granularity: ms or s (default: s)",
    )
    parser.add_argument(
        "-t",
        "--duration",
        type=int,
        default=300,
        help="Duration in units of granularity (default: 300)",
    )
    parser.add_argument(
        "--tle",
        type=str,
        default=DEFAULT_TLE,
        help=f"Path to TLE file (default: {DEFAULT_TLE})",
    )
    parser.add_argument(
        "--sat-index",
        type=int,
        default=0,
        help="Index of satellite in multi-entry TLE file (default: 0)",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=str,
        default=None,
        help="Output CSV path (auto-generated if not specified)",
    )
    parser.add_argument(
        "--start-offset",
        type=float,
        default=DEFAULT_START_OFFSET_S,
        help=f"Seconds before closest approach to start trace (default: {DEFAULT_START_OFFSET_S})",
    )
    parser.add_argument(
        "--gnd-position",
        dest="gnd_position",
        type=str,
        default=None,
        help='Ground station ECEF in JSON-ish form, e.g. \'{"x": -2706714, "y": -4261882, "z": 3885680}\'',
    )
    parser.add_argument(
        "--station-ecef",
        nargs=3,
        type=float,
        metavar=("X_M", "Y_M", "Z_M"),
        default=None,
        help="Ground station ECEF metres as three numbers",
    )
    args = parser.parse_args()

    ts = load.timescale()
    entries = load_tle_entries(args.tle)
    if args.sat_index >= len(entries):
        raise IndexError(
            f"--sat-index {args.sat_index} out of range (file has {len(entries)} entries)"
        )

    name, line1, line2 = entries[args.sat_index]
    sat = EarthSatellite(line1, line2, name, ts)
    print(f"Satellite: {name}")
    print(f"  TLE epoch: {sat.epoch.utc_strftime('%Y-%m-%d %H:%M:%S UTC')}")

    station_info = resolve_station(args)
    station = wgs84.latlon(
        station_info["lat_deg"],
        station_info["lon_deg"],
        elevation_m=station_info["alt_m"],
    )
    print(
        "  Ground station: "
        f"lat={station_info['lat_deg']:.6f} deg, "
        f"lon={station_info['lon_deg']:.6f} deg, alt={station_info['alt_m']:.1f} m"
    )

    culmination = find_pass_culmination(sat, station, ts, ts.tt_jd(sat.epoch.tt))
    start_time = ts.tt_jd(culmination.tt - args.start_offset / 86400.0)
    print(f"  Pass culmination: {culmination.utc_strftime('%Y-%m-%d %H:%M:%S UTC')}")
    print(f"  Trace start: {start_time.utc_strftime('%Y-%m-%d %H:%M:%S UTC')}")

    rows, time_label = generate_trace(
        sat, ts, start_time, args.granularity, args.duration
    )

    if args.output is None:
        args.output = f"orbital_trace_{args.granularity}_{args.duration}.csv"

    fieldnames = [
        time_label,
        "pos_x",
        "pos_y",
        "pos_z",
        "vel_x",
        "vel_y",
        "vel_z",
    ]

    os.makedirs(os.path.dirname(args.output) or ".", exist_ok=True)
    with open(args.output, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

    vels = [(r["vel_x"] ** 2 + r["vel_y"] ** 2 + r["vel_z"] ** 2) ** 0.5 for r in rows]
    radii_km = [
        (r["pos_x"] ** 2 + r["pos_y"] ** 2 + r["pos_z"] ** 2) ** 0.5 / 1e3 for r in rows
    ]
    print(f"\nWrote {len(rows)} rows to {args.output}")
    print(f"  Granularity: {args.granularity}, Duration: {args.duration}")
    print(f"  Velocity mag: {min(vels):.1f} - {max(vels):.1f} m/s")
    print(f"  Geocentric r: {min(radii_km):.1f} - {max(radii_km):.1f} km")


if __name__ == "__main__":
    main()
