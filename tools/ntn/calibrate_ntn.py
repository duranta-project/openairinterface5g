#!/usr/bin/env python3
"""Compute the NTN configuration values matching an orbital trace.

Given a trace (see generate_orbital_trace.py) and the UE / gNB ground positions used in the
channelmod section, print the SIB19 parameters for the gNB configuration and the UE command
line options for both SAT_LEO_TRANS and SAT_LEO_REGEN. Values are taken at trace start (t = 0),
which is where the rfsimulator channel model starts. Nothing is written: apply after review.
"""

from __future__ import annotations

import argparse
import csv
import math

from trace_utils import SPEED_OF_LIGHT_MPS as C
from trace_utils import doppler_hz, look_angles, parse_position_triplet

R17_POS = 1.3  # m per unit, 38.331 EphemerisInfo-r17 position
R17_VEL = 0.06  # m/s per unit, 38.331 EphemerisInfo-r17 velocity
TA_UNIT = 4.072e-9  # s, ta-Common-r17
DRIFT_UNIT = 0.2e-9  # s/s, ta-CommonDrift-r17


def load_trace(path: str) -> list[tuple[float, tuple, tuple]]:
    """Return [(t_s, pos, vel)], accepting time_s or time_ms and any column order."""
    rows = []
    with open(path) as f:
        reader = csv.DictReader(f)
        scale = 1e-3 if "time_ms" in reader.fieldnames else 1.0
        tcol = "time_ms" if scale != 1.0 else "time_s"
        for r in reader:
            pos = (float(r["pos_x"]), float(r["pos_y"]), float(r["pos_z"]))
            vel = (float(r["vel_x"]), float(r["vel_y"]), float(r["vel_z"]))
            rows.append((float(r[tcol]) * scale, pos, vel))
    if len(rows) < 2:
        raise ValueError(f"{path}: need at least 2 rows")
    return rows


def link(rows, ground):
    """Per-sample (one-way delay s, range rate m/s) between ground and satellite."""
    out = []
    for _, pos, vel in rows:
        _, rng, rate = look_angles(pos, ground, vel)
        out.append((rng / C, rate))
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("trace", help="orbital trace CSV")
    parser.add_argument("--ue", required=True, help="UE ECEF position (m), e.g. '-3310204,-5006785,2157610'")
    parser.add_argument("--gnb", help="gNB ground station ECEF position (m), for SAT_LEO_TRANS")
    parser.add_argument("--fc", type=float, required=True, help="DL carrier frequency (Hz)")
    args = parser.parse_args()

    rows = load_trace(args.trace)
    dt = rows[1][0] - rows[0][0]
    ue = parse_position_triplet(args.ue)
    service = link(rows, ue)

    _, pos0, vel0 = rows[0]
    print(f"trace {args.trace}: {len(rows)} rows, {rows[0][0]:.3f}-{rows[-1][0]:.3f} s")
    print("\n=== SIB19 ephemeris at trace start (both modes) ===")
    print("  positionX/Y/Z-r17     = " + ", ".join(str(round(p / R17_POS)) for p in pos0))
    print("  velocityVX/VY/VZ-r17  = " + ", ".join(str(round(v / R17_VEL)) for v in vel0))

    def print_mode(name, delay0, doppler0, rtt_max_ms, ue_drift_s_s, ta_common, ta_drift):
        print(f"\n=== {name} ===")
        print(f"  ta-Common-r17            = {ta_common}")
        print(f"  ta-CommonDrift-r17       = {ta_drift}")
        print(f"  cellSpecificKoffset_r17  = {math.ceil(rtt_max_ms)}   # max RTT {rtt_max_ms:.2f} ms")
        print(f"  rfsimulator.prop_delay   = {math.ceil(delay0 * 1e3)}   # one-way delay {delay0 * 1e3:.2f} ms")
        print(f"  --initial-fo             = {round(doppler0)}   # DL Doppler {doppler0 / 1e3:.2f} kHz")
        print(f"  --ntn-initial-time-drift = {round(ue_drift_s_s * 1e6)}   # us/s")

    # REGEN: gNB on the satellite, service link only
    s_delay_drift = (service[1][0] - service[0][0]) / dt
    print_mode(
        "SAT_LEO_REGEN",
        service[0][0],
        doppler_hz(service[0][1], args.fc),
        2 * max(d for d, _ in service) * 1e3,
        s_delay_drift,
        0,
        0,
    )

    if args.gnb is None:
        return
    # TRANS: feeder link in ta-Common, both links in the channel
    feeder = link(rows, parse_position_triplet(args.gnb))
    f_delay_drift = (feeder[1][0] - feeder[0][0]) / dt
    print_mode(
        "SAT_LEO_TRANS",
        feeder[0][0] + service[0][0],
        doppler_hz(feeder[0][1], args.fc) + doppler_hz(service[0][1], args.fc),
        2 * max(f[0] + s[0] for f, s in zip(feeder, service)) * 1e3,
        2 * f_delay_drift,
        round(2 * feeder[0][0] / TA_UNIT),
        round(2 * f_delay_drift / DRIFT_UNIT),
    )


if __name__ == "__main__":
    main()
