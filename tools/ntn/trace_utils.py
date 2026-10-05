#!/usr/bin/env python3
"""Shared coordinate helpers for the NTN trace tools."""

from __future__ import annotations

import json
import math
import re

WGS84_A_M = 6378137.0
WGS84_F = 1.0 / 298.257223563
WGS84_B_M = WGS84_A_M * (1.0 - WGS84_F)
WGS84_E2 = WGS84_F * (2.0 - WGS84_F)
WGS84_EP2 = (WGS84_A_M**2 - WGS84_B_M**2) / WGS84_B_M**2


def parse_position_triplet(text: str) -> tuple[float, float, float]:
    """Parse an ECEF triplet from JSON-ish or comma-separated text."""
    cleaned = text.strip()

    try:
        parsed = json.loads(cleaned)
    except json.JSONDecodeError:
        parsed = None

    if isinstance(parsed, dict):
        try:
            return float(parsed["x"]), float(parsed["y"]), float(parsed["z"])
        except KeyError as exc:
            raise ValueError("Position JSON must contain x, y, and z") from exc

    match_values = dict(re.findall(r"([xyz])\s*[:=]\s*([-+0-9.eE]+)", cleaned))
    if len(match_values) == 3:
        return (
            float(match_values["x"]),
            float(match_values["y"]),
            float(match_values["z"]),
        )

    parts = [p.strip() for p in cleaned.split(",") if p.strip()]
    if len(parts) == 3:
        return float(parts[0]), float(parts[1]), float(parts[2])

    raise ValueError(
        "Unable to parse position. Use JSON like "
        '\'{"x": -2706714, "y": -4261882, "z": 3885680}\' '
        "or comma-separated x,y,z."
    )


def ecef_to_geodetic_wgs84(
    x_m: float, y_m: float, z_m: float
) -> tuple[float, float, float]:
    """Convert ECEF metres to geodetic WGS-84 coordinates."""
    p = math.hypot(x_m, y_m)
    if p == 0.0:
        lat = math.copysign(math.pi / 2.0, z_m)
        lon = 0.0
        alt = abs(z_m) - WGS84_B_M
        return math.degrees(lat), math.degrees(lon), alt

    theta = math.atan2(z_m * WGS84_A_M, p * WGS84_B_M)
    sin_theta = math.sin(theta)
    cos_theta = math.cos(theta)

    lat = math.atan2(
        z_m + WGS84_EP2 * WGS84_B_M * sin_theta**3,
        p - WGS84_E2 * WGS84_A_M * cos_theta**3,
    )
    lon = math.atan2(y_m, x_m)

    sin_lat = math.sin(lat)
    n = WGS84_A_M / math.sqrt(1.0 - WGS84_E2 * sin_lat * sin_lat)
    alt = p / math.cos(lat) - n
    return math.degrees(lat), math.degrees(lon), alt


SPEED_OF_LIGHT_MPS = 299792458.0


def geodetic_up(x_m: float, y_m: float, z_m: float) -> tuple[float, float, float]:
    """Local geodetic 'up' (zenith) unit vector at an ECEF point."""
    lat, lon, _ = ecef_to_geodetic_wgs84(x_m, y_m, z_m)
    lat_r, lon_r = math.radians(lat), math.radians(lon)
    return (
        math.cos(lat_r) * math.cos(lon_r),
        math.cos(lat_r) * math.sin(lon_r),
        math.sin(lat_r),
    )


def look_angles(
    sat_ecef: tuple[float, float, float],
    ue_ecef: tuple[float, float, float],
    sat_vel: tuple[float, float, float] | None = None,
) -> tuple[float, float, float]:
    """Look geometry of a satellite from a ground point.

    Returns (elevation_deg, range_m, range_rate_mps); range_rate > 0 = receding
    (used for the Doppler sign). ``sat_vel`` may be None (range_rate = 0).
    """
    los = (
        sat_ecef[0] - ue_ecef[0],
        sat_ecef[1] - ue_ecef[1],
        sat_ecef[2] - ue_ecef[2],
    )
    rng = math.sqrt(los[0] ** 2 + los[1] ** 2 + los[2] ** 2)
    if rng == 0.0:
        return 90.0, 0.0, 0.0
    los_u = (los[0] / rng, los[1] / rng, los[2] / rng)
    up = geodetic_up(*ue_ecef)
    sin_e = max(-1.0, min(1.0, sum(los_u[i] * up[i] for i in range(3))))
    elev = math.degrees(math.asin(sin_e))
    rate = 0.0
    if sat_vel is not None:
        rate = sum(sat_vel[i] * los_u[i] for i in range(3))
    return elev, rng, rate


def elevation_deg(
    sat_ecef: tuple[float, float, float],
    ue_ecef: tuple[float, float, float],
) -> float:
    """Elevation angle (deg) of a satellite seen from a ground ECEF point."""
    return look_angles(sat_ecef, ue_ecef)[0]


def doppler_hz(range_rate_mps: float, carrier_hz: float) -> float:
    """First-order Doppler shift (Hz). Receding (rate>0) -> negative shift."""
    return -range_rate_mps / SPEED_OF_LIGHT_MPS * carrier_hz
