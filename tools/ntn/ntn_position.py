#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-CSSL-1.0
"""OCUDU position-file parsing and WGS-84 ECEF conversion."""

import math


WGS84_SEMI_MAJOR_AXIS_M = 6378137.0
WGS84_FLATTENING = 1.0 / 298.257223563
WGS84_ECCENTRICITY_SQUARED = WGS84_FLATTENING * (2.0 - WGS84_FLATTENING)


def load_position(path):
    """Load latitude, longitude, and altitude from an OCUDU position file."""
    values = {}
    with open(path, encoding="utf-8") as stream:
        for line_number, line in enumerate(stream, 1):
            stripped = line.strip().rstrip(",")
            if not stripped or stripped.startswith("#"):
                continue
            key, separator, value = stripped.partition(":")
            if not separator or key not in ("latitude", "longitude", "altitude") or key in values:
                raise ValueError(f"Invalid position entry in {path}:{line_number}")
            try:
                values[key] = float(value.strip())
            except ValueError as error:
                raise ValueError(f"Invalid position value in {path}:{line_number}") from error
    missing = {"latitude", "longitude", "altitude"} - values.keys()
    if missing:
        raise ValueError(f"Missing {', '.join(sorted(missing))} in {path}")
    if not all(math.isfinite(value) for value in values.values()):
        raise ValueError(f"Non-finite position value in {path}")
    if not -90.0 <= values["latitude"] <= 90.0 or not -180.0 <= values["longitude"] <= 180.0:
        raise ValueError(f"Position outside latitude/longitude range in {path}")
    return values["latitude"], values["longitude"], values["altitude"]


def geodetic_to_ecef(latitude_deg, longitude_deg, altitude_m):
    """Convert WGS-84 geodetic coordinates to ECEF metres."""
    latitude_rad = math.radians(latitude_deg)
    longitude_rad = math.radians(longitude_deg)
    sin_latitude = math.sin(latitude_rad)
    radius = WGS84_SEMI_MAJOR_AXIS_M / math.sqrt(1.0 - WGS84_ECCENTRICITY_SQUARED * sin_latitude**2)
    x_m = (radius + altitude_m) * math.cos(latitude_rad) * math.cos(longitude_rad)
    y_m = (radius + altitude_m) * math.cos(latitude_rad) * math.sin(longitude_rad)
    z_m = (radius * (1.0 - WGS84_ECCENTRICITY_SQUARED) + altitude_m) * sin_latitude
    if not all(math.isfinite(value) for value in (x_m, y_m, z_m)):
        raise ValueError("Non-finite ECEF position")
    return x_m, y_m, z_m
