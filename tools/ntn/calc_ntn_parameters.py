#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-CSSL-1.0

"""Print the gNB (SIB19) and UE parameters matching an orbital trace at its start."""

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

    s_delay_drift = (service[1][0] - service[0][0]) / dt

    # SAT_LEO_REGEN: gNB is on the satellite, only service link for both delay and doppler
    print_mode(
        "SAT_LEO_REGEN",
        # [NOBUG] Delay/TA: service link only
        delay0=service[0][0],
        rtt_max_ms=2 * max(s[0] for s in service) * 1e3,
        ue_drift_s_s=s_delay_drift,
        # [NOBUG] Doppler: service link only
        doppler0=doppler_hz(service[0][1], args.fc),
        # [NOBUG] Common TA: none, the UE computes the service link delay from the ephemeris
        ta_common=0,
        ta_drift=0,
    )

    if args.gnb is None:
        return
    feeder = link(rows, parse_position_triplet(args.gnb))
    f_delay_drift = (feeder[1][0] - feeder[0][0]) / dt

    # SAT_LEO_TRANS: UE <-> SAT <-> gNB on the ground
    print_mode(
        "SAT_LEO_TRANS",
        # [NOBUG] Delay/TA: service link + feeder link
        delay0=service[0][0] + feeder[0][0],
        rtt_max_ms=2 * max(s[0] + f[0] for s, f in zip(service, feeder)) * 1e3,
        ue_drift_s_s=s_delay_drift + f_delay_drift,
        # [NOBUG] Doppler: service link only, the feeder link Doppler is compensated by the network
        doppler0=doppler_hz(service[0][1], args.fc),
        # [NOBUG] Common TA: feeder link round trip, signalled in SIB19
        ta_common=round(2 * feeder[0][0] / TA_UNIT),
        ta_drift=round(2 * f_delay_drift / DRIFT_UNIT),
    )


if __name__ == "__main__":
    main()
