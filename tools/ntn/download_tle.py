#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-CSSL-1.0

"""Download and filter TLE data from CelesTrak.

Usage examples:
    python download_tle.py starlink
    python download_tle.py starlink --name-contains "[DTC]" -o starlink_dtc.tle
    python download_tle.py starlink --min-alt-km 330 --max-alt-km 380
"""

from __future__ import annotations

import argparse
import math
import os
import socket
import urllib.error
import urllib.request

CELESTRAK_BASE = "https://celestrak.org/NORAD/elements/gp.php"
USER_AGENT = "OAI-ntn-tools/1.0"
MU_EARTH_M3_S2 = 3.986004418e14
R_EARTH_M = 6378137.0

def load_tle_entries_from_text(data: str) -> list[tuple[str, str, str]]:
    """Parse TLE text into (name, line1, line2) tuples."""
    lines = [line.strip() for line in data.splitlines() if line.strip()]
    entries = []
    i = 0
    while i < len(lines):
        if lines[i].startswith("1 ") and i + 1 < len(lines):
            entries.append(("UNKNOWN", lines[i], lines[i + 1]))
            i += 2
        elif i + 2 < len(lines):
            entries.append((lines[i], lines[i + 1], lines[i + 2]))
            i += 3
        else:
            raise ValueError("Malformed TLE response from CelesTrak")

    if not entries:
        raise ValueError("No valid TLE entries found in response")
    return entries


def tle_entries_to_text(entries: list[tuple[str, str, str]]) -> str:
    """Serialize TLE entries back to text."""
    output = []
    for name, line1, line2 in entries:
        if name and name != "UNKNOWN":
            output.append(name)
        output.extend([line1, line2])
    return "\n".join(output) + "\n"


def estimate_altitude_km(line2: str) -> float:
    """Estimate altitude from TLE mean motion."""
    if len(line2) < 63:
        raise ValueError(f"Malformed TLE line 2: {line2}")

    mean_motion_field = line2[52:63].strip()
    if not mean_motion_field:
        raise ValueError(f"Missing mean motion in TLE line 2: {line2}")

    mean_motion_rev_per_day = float(mean_motion_field)
    mean_motion_rad_s = mean_motion_rev_per_day * 2.0 * math.pi / 86400.0
    semi_major_axis_m = (MU_EARTH_M3_S2 / (mean_motion_rad_s**2)) ** (1.0 / 3.0)
    return semi_major_axis_m / 1000.0 - R_EARTH_M / 1000.0


def fetch_url(url: str) -> str:
    """Fetch URL, retrying without environment proxies on proxy failure."""
    handlers = [None, urllib.request.ProxyHandler({})]
    last_exc = None

    for idx, proxy_handler in enumerate(handlers):
        opener = urllib.request.build_opener(proxy_handler) if proxy_handler else urllib.request.build_opener()
        req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
        try:
            with opener.open(req, timeout=30) as resp:
                return resp.read().decode("utf-8")
        except urllib.error.URLError as exc:
            last_exc = exc
            proxy_related = isinstance(exc.reason, socket.error) or "proxy" in str(exc).lower()
            if idx == 0 and proxy_related:
                print("Download via environment proxy failed, retrying without proxies...")
                continue
            raise

    raise RuntimeError(f"Download failed: {last_exc}")


def filter_entries(
    entries: list[tuple[str, str, str]],
    name_contains: list[str],
    min_alt_km: float | None,
    max_alt_km: float | None,
) -> list[tuple[str, str, str]]:
    """Filter entries by satellite name and approximate altitude."""
    filtered = []
    for name, line1, line2 in entries:
        name_lc = name.lower()
        if any(token.lower() not in name_lc for token in name_contains):
            continue

        if min_alt_km is not None or max_alt_km is not None:
            alt_km = estimate_altitude_km(line2)
            if min_alt_km is not None and alt_km < min_alt_km:
                continue
            if max_alt_km is not None and alt_km > max_alt_km:
                continue

        filtered.append((name, line1, line2))
    return filtered


def build_url(group: str) -> tuple[str, str]:
    """Build CelesTrak query URL and default file name."""
    return f"{CELESTRAK_BASE}?GROUP={group}&FORMAT=tle", f"{group}.tle"


def download_tle(
    group: str,
    output_path: str | None = None,
    name_contains: list[str] | None = None,
    min_alt_km: float | None = None,
    max_alt_km: float | None = None,
) -> None:
    """Download and optionally filter TLE data."""
    url, default_name = build_url(group)
    if output_path is None:
        output_path = default_name

    print(f"Downloading from: {url}")
    data = fetch_url(url)
    entries = load_tle_entries_from_text(data)
    original_count = len(entries)

    name_contains = name_contains or []
    filtered_entries = filter_entries(entries, name_contains, min_alt_km, max_alt_km)

    if not filtered_entries:
        raise RuntimeError(
            "No TLE entries matched the requested filters. "
            "Try a wider altitude band or remove the name filter."
        )

    os.makedirs(os.path.dirname(output_path) or ".", exist_ok=True)
    with open(output_path, "w") as f:
        f.write(tle_entries_to_text(filtered_entries))

    print(f"Matched {len(filtered_entries)} / {original_count} TLE entries")
    if name_contains:
        print(f"  Name contains: {', '.join(name_contains)}")
    if min_alt_km is not None or max_alt_km is not None:
        min_text = str(min_alt_km) if min_alt_km is not None else "-inf"
        max_text = str(max_alt_km) if max_alt_km is not None else "inf"
        print(f"  Altitude filter: {min_text} .. {max_text} km")
    print(f"Saved filtered TLE data to {output_path}")


def main():
    parser = argparse.ArgumentParser(description="Download and filter TLE data from CelesTrak")
    parser.add_argument(
        "group",
        type=str,
        help="Satellite group name (e.g. starlink, oneweb, active)",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=str,
        default=None,
        help="Output file path (default: <group>.tle)",
    )
    parser.add_argument(
        "--name-contains",
        action="append",
        default=[],
        help='Keep only entries whose name contains this substring, e.g. --name-contains "[DTC]"',
    )
    parser.add_argument(
        "--min-alt-km",
        type=float,
        default=None,
        help="Minimum approximate altitude in km",
    )
    parser.add_argument(
        "--max-alt-km",
        type=float,
        default=None,
        help="Maximum approximate altitude in km",
    )
    args = parser.parse_args()

    download_tle(
        group=args.group,
        output_path=args.output,
        name_contains=args.name_contains,
        min_alt_km=args.min_alt_km,
        max_alt_km=args.max_alt_km,
    )


if __name__ == "__main__":
    main()
