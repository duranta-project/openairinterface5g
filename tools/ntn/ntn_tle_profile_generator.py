#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-CSSL-1.0
"""Generate delay and Doppler CSV profiles from TLE data using Skyfield."""

import sys
import argparse
import numpy as np
from datetime import datetime, timezone
from ntn_position import geodetic_to_ecef, load_position

try:
    from skyfield.api import load, EarthSatellite, wgs84
    from sgp4.api import Satrec, WGS84
except ImportError:
    print("Error: skyfield python package is required. Install via 'pip install skyfield numpy'", file=sys.stderr)
    sys.exit(1)

# Default LEO Satellite TLE (600 km orbit, 55 deg inclination)
DEFAULT_TLE_LINE1 = "1 99999U 24001A   24001.00000000  .00000000  00000-0  00000-0 0  9991"
DEFAULT_TLE_LINE2 = "2 99999  55.0000 100.0000 0001000  90.0000 270.0000 14.85000000    16"
SPEED_OF_LIGHT = 299792458.0  # m/s
DEFAULT_UE_POSITION = (48.8566, 2.3522, 1.0)
DEFAULT_GATEWAY_POSITION = (48.8566, 2.3522, 1.0)


def parse_starting_time(value):
    try:
        start_time = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError as exc:
        raise argparse.ArgumentTypeError("expected ISO-8601 UTC time, e.g. 2026-09-08T12:00:00.000Z") from exc
    if start_time.tzinfo is None:
        return start_time.replace(tzinfo=timezone.utc)
    return start_time.astimezone(timezone.utc)


def load_starting_time_config(path):
    """Load the serving satellite epoch from an OCUDU sat.yml file."""
    try:
        import yaml
    except ImportError as exc:
        raise ValueError("PyYAML is required for --starting-time-config") from exc

    try:
        with open(path, encoding="utf-8") as stream:
            config = yaml.safe_load(stream)
        satellites = config["ntn"]["satellites"]
        epoch = satellites[0]["epoch_timestamp"]
    except (KeyError, IndexError, TypeError, yaml.YAMLError) as exc:
        raise ValueError(f"Invalid OCUDU satellite config {path}: expected ntn.satellites[0].epoch_timestamp") from exc

    if isinstance(epoch, datetime):
        return epoch.replace(tzinfo=timezone.utc) if epoch.tzinfo is None else epoch.astimezone(timezone.utc)
    if not isinstance(epoch, str):
        raise ValueError(f"Invalid epoch_timestamp in {path}: expected an ISO-8601 timestamp")
    try:
        return parse_starting_time(epoch)
    except argparse.ArgumentTypeError as exc:
        raise ValueError(f"Invalid epoch_timestamp in {path}: {epoch}") from exc


def load_tle(tle_file=None):
    """Load a TLE from a file or the built-in default."""
    if tle_file:
        with open(tle_file, 'r') as f:
            lines = [line.strip() for line in f if line.strip()]
        if len(lines) >= 3:
            name, l1, l2 = lines[0], lines[1], lines[2]
        elif len(lines) >= 2:
            name, l1, l2 = "NTN_SAT", lines[0], lines[1]
        else:
            raise ValueError(f"Invalid TLE file {tle_file}: expected at least 2 lines")
        return name, l1, l2

    return "NTN_SAT", DEFAULT_TLE_LINE1, DEFAULT_TLE_LINE2


def _range_and_range_rate(sat, ground_loc, times, num_steps):
    """Return one-way range (m) and range rate (m/s, positive = receding)."""
    diff = (sat - ground_loc).at(times)
    pos_km = np.asarray(diff.position.km).reshape(3, -1)
    vel_kms = np.asarray(diff.velocity.km_per_s).reshape(3, -1)
    dist_m = np.linalg.norm(pos_km, axis=0) * 1000.0
    unit_pos = pos_km / np.linalg.norm(pos_km, axis=0)
    range_rate_ms = np.sum(vel_kms * unit_pos, axis=0) * 1000.0
    return dist_m, range_rate_ms


def calculate_directional_doppler(service_range_rate_ms, feeder_range_rate_ms,
                                  service_dl_freq_hz, service_ul_freq_hz,
                                  feeder_dl_freq_hz, feeder_ul_freq_hz,
                                  sat_type):
    """Return physical DL and UL Dopplers using positive range rate for recession."""
    if sat_type == "transparent":
        dl_hz = -(service_range_rate_ms * service_dl_freq_hz + feeder_range_rate_ms * feeder_dl_freq_hz) / SPEED_OF_LIGHT
        ul_hz = -(service_range_rate_ms * service_ul_freq_hz + feeder_range_rate_ms * feeder_ul_freq_hz) / SPEED_OF_LIGHT
    else:
        dl_hz = -(service_range_rate_ms * service_dl_freq_hz) / SPEED_OF_LIGHT
        ul_hz = -(service_range_rate_ms * service_ul_freq_hz) / SPEED_OF_LIGHT
    return dl_hz, ul_hz


def calculate_ntn_delay_doppler(sat, ts, start_datetime, duration_sec, step_sec,
                                gateway_loc, ue_loc, service_dl_freq_hz, service_ul_freq_hz,
                                feeder_dl_freq_hz, feeder_ul_freq_hz, sat_type="transparent"):
    """Return time offsets, one-way propagation delay, and directional physical Doppler."""
    num_steps = int(np.ceil(duration_sec / step_sec)) + 1
    time_offsets = np.linspace(0, duration_sec, num_steps)

    start_ts = start_datetime.timestamp()
    t_utc = [datetime.fromtimestamp(start_ts + dt, tz=timezone.utc) for dt in time_offsets]
    times = ts.from_datetimes(t_utc)

    dist_m_service, vr_service = _range_and_range_rate(sat, ue_loc, times, num_steps)

    if sat_type == "transparent":
        dist_m_feeder, vr_feeder = _range_and_range_rate(sat, gateway_loc, times, num_steps)
        total_dist_m = dist_m_service + dist_m_feeder
    else:
        vr_feeder = np.zeros_like(vr_service)
        total_dist_m = dist_m_service

    dl_doppler_hz, ul_doppler_hz = calculate_directional_doppler(
        vr_service,
        vr_feeder,
        service_dl_freq_hz,
        service_ul_freq_hz,
        feeder_dl_freq_hz,
        feeder_ul_freq_hz,
        sat_type,
    )
    if sat_type == "transparent":
        service_dl_doppler_hz = -(vr_service * service_dl_freq_hz) / SPEED_OF_LIGHT
    else:
        service_dl_doppler_hz = dl_doppler_hz
    delay_ms = (total_dist_m / SPEED_OF_LIGHT) * 1000.0

    return time_offsets, delay_ms, dl_doppler_hz, ul_doppler_hz, service_dl_doppler_hz


def main():
    parser = argparse.ArgumentParser(description="NTN TLE Delay and Doppler Generator using Skyfield")
    parser.add_argument("--tle-file", type=str, help="Path to TLE file")
    parser.add_argument("--sat-type", type=str, choices=["transparent", "regenerative"], default="transparent",
                        help="Satellite payload architecture")
    parser.add_argument("--ue-position-file", help="OCUDU ue-position.cfg path")
    parser.add_argument("--ue-lat", type=float, default=None, help="UE latitude (deg)")
    parser.add_argument("--ue-lon", type=float, default=None, help="UE longitude (deg)")
    parser.add_argument("--ue-alt", type=float, default=None, help="UE altitude (m)")
    parser.add_argument("--gateway-position-file", help="OCUDU gw-position.cfg path")
    parser.add_argument("--gateway-lat", "--gnb-lat", dest="gateway_lat", type=float, default=None,
                        help="Gateway/gNB latitude (deg); feeder-link endpoint in transparent mode")
    parser.add_argument("--gateway-lon", "--gnb-lon", dest="gateway_lon", type=float, default=None,
                        help="Gateway/gNB longitude (deg); feeder-link endpoint in transparent mode")
    parser.add_argument("--gateway-alt", "--gnb-alt", dest="gateway_alt", type=float, default=None,
                        help="Gateway/gNB altitude (m); feeder-link endpoint in transparent mode")
    parser.add_argument("--dl-freq", type=float, default=2185e6,
                        help="Default DL frequency for service and feeder links in Hz (default: 2185 MHz)")
    parser.add_argument("--ul-freq", type=float, default=1995e6,
                        help="Default UL frequency for service and feeder links in Hz (default: 1995 MHz)")
    parser.add_argument("--service-dl-freq", type=float, default=None,
                        help="Service-link DL frequency in Hz; default: --dl-freq")
    parser.add_argument("--service-ul-freq", type=float, default=None,
                        help="Service-link UL frequency in Hz; default: --ul-freq")
    parser.add_argument("--feeder-dl-freq", dest="feeder_dl_freq", type=float, default=None,
                        help="Feeder-link DL frequency in Hz, transparent mode only; "
                             "default: --dl-freq")
    parser.add_argument("--feeder-ul-freq", dest="feeder_ul_freq", type=float, default=None,
                        help="Feeder-link UL frequency in Hz, transparent mode only; "
                             "default: --ul-freq")
    parser.add_argument("--duration", type=float, default=60.0, help="Simulation duration in seconds (default: 60s)")
    parser.add_argument("--step", type=float, default=0.01, help="Time step in seconds (default: 0.01s)")
    starting_time = parser.add_mutually_exclusive_group()
    starting_time.add_argument("--starting-time", type=parse_starting_time, default=None,
                               help="UTC start time as ISO-8601 (e.g. 2026-09-08T12:00:00.000Z); default: now")
    starting_time.add_argument("--starting-time-config",
                               help="OCUDU sat.yml file providing ntn.satellites[0].epoch_timestamp")
    parser.add_argument("--out-file", "-o", type=str, required=True, help="Output profile CSV file path")
    args = parser.parse_args()

    for name, value in (("--dl-freq", args.dl_freq),
                        ("--ul-freq", args.ul_freq),
                        ("--service-dl-freq", args.service_dl_freq),
                        ("--service-ul-freq", args.service_ul_freq),
                        ("--feeder-dl-freq", args.feeder_dl_freq),
                        ("--feeder-ul-freq", args.feeder_ul_freq)):
        if value is not None and (not np.isfinite(value) or value <= 0.0):
            parser.error(f"{name} must be a finite, positive frequency in Hz")

    service_dl_freq_hz = args.service_dl_freq if args.service_dl_freq is not None else args.dl_freq
    service_ul_freq_hz = args.service_ul_freq if args.service_ul_freq is not None else args.ul_freq
    feeder_dl_freq_hz = args.feeder_dl_freq if args.feeder_dl_freq is not None else args.dl_freq
    feeder_ul_freq_hz = args.feeder_ul_freq if args.feeder_ul_freq is not None else args.ul_freq

    try:
        start_dt = load_starting_time_config(args.starting_time_config) if args.starting_time_config else args.starting_time
        ue_position = load_position(args.ue_position_file) if args.ue_position_file else DEFAULT_UE_POSITION
    except (OSError, ValueError) as error:
        parser.error(str(error))
    ue_lat = args.ue_lat if args.ue_lat is not None else ue_position[0]
    ue_lon = args.ue_lon if args.ue_lon is not None else ue_position[1]
    ue_alt = args.ue_alt if args.ue_alt is not None else ue_position[2]

    try:
        gateway_position = load_position(args.gateway_position_file) if args.gateway_position_file else DEFAULT_GATEWAY_POSITION
    except (OSError, ValueError) as error:
        parser.error(str(error))
    gateway_lat = args.gateway_lat if args.gateway_lat is not None else gateway_position[0]
    gateway_lon = args.gateway_lon if args.gateway_lon is not None else gateway_position[1]
    gateway_alt = args.gateway_alt if args.gateway_alt is not None else gateway_position[2]

    name, l1, l2 = load_tle(args.tle_file)
    ts = load.timescale()
    sat = EarthSatellite.from_satrec(Satrec.twoline2rv(l1, l2, WGS84), ts)
    sat.name = name

    ue_loc = wgs84.latlon(ue_lat, ue_lon, ue_alt)
    gateway_loc = wgs84.latlon(gateway_lat, gateway_lon, gateway_alt)

    start_dt = start_dt or datetime.now(timezone.utc)
    time_offsets, delay_ms, dl_doppler_hz, ul_doppler_hz, service_dl_doppler_hz = calculate_ntn_delay_doppler(
        sat, ts, start_dt, args.duration, args.step, gateway_loc, ue_loc,
        service_dl_freq_hz, service_ul_freq_hz, feeder_dl_freq_hz, feeder_ul_freq_hz, args.sat_type
    )

    with open(args.out_file, "w") as f:
        f.write(f"# start_unix_s={start_dt.timestamp():.6f}\n")
        f.write(f"# initial_dl_service_doppler_hz={service_dl_doppler_hz[0]:.2f}\n")
        f.write("# time_s,delay_ms,dl_doppler_hz,ul_doppler_hz\n")
        for t_s, d_ms, dl_hz, ul_hz in zip(time_offsets, delay_ms, dl_doppler_hz, ul_doppler_hz):
            f.write(f"{t_s:.4f},{d_ms:.6f},{dl_hz:.2f},{ul_hz:.2f}\n")
    print(f"[NTN Skyfield] Saved TLE profile to {args.out_file} ({len(time_offsets)} samples)")
    ue_x_m, ue_y_m, ue_z_m = geodetic_to_ecef(ue_lat, ue_lon, ue_alt)
    print("[NTN Skyfield] OAI UE position (ECEF meters):")
    print(f"position0 = {{ x = {ue_x_m:.3f}; y = {ue_y_m:.3f}; z = {ue_z_m:.3f}; }}")


if __name__ == "__main__":
    main()
