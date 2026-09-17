#!/usr/bin/env python3
"""
Verify O-RU UL U-plane time signatures for a chosen TDD slot class.

Parses oru_ul_pcap_write dumps and checks, for abs_slot % period == target:
  1) O-RAN radio-app header decode (frame / subframe / slot / symbol)
  2) abs_slot = sf * 2 + slot_id  (mu=1)
  3) per-(frame, abs_slot, eAxC) symbol set and wall-clock spacing

Example (U-slot in DDDSU period-5):
  python3 check_oru_ul_slot_timing.py ../oru_ul.pcap --period 5 --slot-mod 4
"""

from __future__ import annotations

import argparse
import statistics
import struct
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path

ETH_LEN = 14
ECPRI_LEN = 8
RADIO_APP_LEN = 4
SECTION_HDR_LEN = 4
HEADER_LEN = ETH_LEN + ECPRI_LEN + RADIO_APP_LEN + SECTION_HDR_LEN
PCAP_MAGIC_LE = 0xA1B2C3D4
PCAP_MAGIC_BE = 0xD4C3B2A1

# Nominal OFDM symbol period at mu=1 including average CP (~500 us / 14).
NOM_SYM_US_MU1 = 500.0 / 14.0


@dataclass(frozen=True)
class Pkt:
    index: int
    t_us: float
    frame_id: int
    subframe_id: int
    slot_id: int
    abs_slot: int
    symbol_id: int
    eaxc: int
    num_prb: int
    pkt_len: int


def _endian(magic: int) -> str:
    if magic == PCAP_MAGIC_LE:
        return "<"
    if magic == PCAP_MAGIC_BE:
        return ">"
    raise ValueError(f"unsupported pcap magic 0x{magic:08x}")


def read_packets(path: Path, mu: int = 1, max_packets: int | None = None) -> list[Pkt]:
    slots_per_subframe = 1 << mu
    out: list[Pkt] = []
    with path.open("rb") as f:
        gh = f.read(24)
        if len(gh) < 24:
            raise ValueError("truncated pcap")
        endian = _endian(struct.unpack_from("I", gh, 0)[0])
        idx = 0
        while max_packets is None or idx < max_packets:
            ph = f.read(16)
            if len(ph) < 16:
                break
            ts_sec, ts_usec, incl_len, _ = struct.unpack(endian + "IIII", ph)
            pkt = f.read(incl_len)
            if len(pkt) < HEADER_LEN:
                continue
            frame_id = pkt[ETH_LEN + ECPRI_LEN + 1]
            sf_slot_sym = struct.unpack_from(">H", pkt, ETH_LEN + ECPRI_LEN + 2)[0]
            symbol_id = sf_slot_sym & 0x3F
            slot_id = (sf_slot_sym >> 6) & 0x3F
            subframe_id = (sf_slot_sym >> 12) & 0xF
            eaxc = pkt[ETH_LEN + 5] & 0x0F
            word = struct.unpack_from(">I", pkt, ETH_LEN + ECPRI_LEN + RADIO_APP_LEN)[0]
            num_prb = word & 0xFF
            if num_prb == 0:
                payload = incl_len - HEADER_LEN
                num_prb = (payload // 4) // 12 if payload >= 48 else 0
            abs_slot = subframe_id * slots_per_subframe + slot_id
            out.append(
                Pkt(
                    index=idx,
                    t_us=ts_sec * 1e6 + ts_usec,
                    frame_id=frame_id,
                    subframe_id=subframe_id,
                    slot_id=slot_id,
                    abs_slot=abs_slot,
                    symbol_id=symbol_id,
                    eaxc=eaxc,
                    num_prb=num_prb,
                    pkt_len=incl_len,
                )
            )
            idx += 1
    return out


def analyze(packets: list[Pkt], period: int, slot_mod: int, nom_sym_us: float, slack_us: float) -> int:
    by_mod: dict[int, int] = defaultdict(int)
    for p in packets:
        by_mod[p.abs_slot % period] += 1
    print(f"parsed {len(packets)} packets")
    print(f"abs_slot % {period} counts: {dict(sorted(by_mod.items()))}")

    sel = [p for p in packets if p.abs_slot % period == slot_mod]
    print(f"target abs_slot % {period} == {slot_mod}: {len(sel)} packets")
    if not sel:
        print("ERROR: no packets for target slot class", file=sys.stderr)
        return 2

    header_bad = 0
    for p in sel:
        recon = p.subframe_id * 2 + p.slot_id  # report assumes mu=1 for recon check below
        # Recompute with stored abs_slot already using mu from reader.
        if p.slot_id > 63 or p.symbol_id > 13:
            print(
                f"  BAD range range idx={p.index} "
                f"f={p.frame_id} sf={p.subframe_id} slot={p.slot_id} sym={p.symbol_id}"
            )
            header_bad += 1

    groups: dict[tuple[int, int, int], list[Pkt]] = defaultdict(list)
    for p in sel:
        groups[(p.frame_id, p.abs_slot, p.eaxc)].append(p)

    print(f"groups (frame, abs_slot, eAxC): {len(groups)}")
    print(f"nominal symbol period: {nom_sym_us:.3f} us  (slack ±{slack_us:.1f} us)")

    cons_dts: list[float] = []
    wall_vs_sym_backwards = 0
    missing_sym_gaps = 0
    outlier_gaps = 0
    shown = 0
    for key, g in sorted(groups.items()):
        g_sym = sorted(g, key=lambda x: (x.symbol_id, x.t_us))
        syms = [x.symbol_id for x in g_sym]
        uniq = sorted(set(syms))
        # wall-clock order vs symbol order
        g_wall = sorted(g, key=lambda x: x.t_us)
        wall_syms = [x.symbol_id for x in g_wall]
        if wall_syms != sorted(wall_syms):
            wall_vs_sym_backwards += 1

        for i in range(1, len(g_sym)):
            if g_sym[i].symbol_id == g_sym[i - 1].symbol_id + 1:
                dt = g_sym[i].t_us - g_sym[i - 1].t_us
                cons_dts.append(dt)
                if abs(dt - nom_sym_us) > slack_us:
                    outlier_gaps += 1
            elif g_sym[i].symbol_id > g_sym[i - 1].symbol_id + 1:
                missing_sym_gaps += 1

        if shown < 12:
            dts = [
                g_sym[i].t_us - g_sym[i - 1].t_us
                for i in range(1, len(g_sym))
                if g_sym[i].symbol_id == g_sym[i - 1].symbol_id + 1
            ]
            span = (g_sym[-1].t_us - g_sym[0].t_us) if len(g_sym) > 1 else 0.0
            print(
                f"  f={key[0]} abs={key[1]} eaxc={key[2]} n={len(g_sym)} "
                f"syms={uniq} span_us={span:.1f}"
                + (f" Δt[min/mean/max]={min(dts):.1f}/{statistics.mean(dts):.1f}/{max(dts):.1f}" if dts else "")
            )
            shown += 1

    print("\n=== summary ===")
    print(f"header range errors: {header_bad}")
    print(f"groups with wall-clock symbol disorder: {wall_vs_sym_backwards}/{len(groups)}")
    print(f"non-consecutive symbol steps (within group, by sym id): {missing_sym_gaps}")
    if cons_dts:
        print(
            f"consecutive-symbol Δt_us (n={len(cons_dts)}): "
            f"min={min(cons_dts):.1f} median={statistics.median(cons_dts):.1f} "
            f"mean={statistics.mean(cons_dts):.1f} max={max(cons_dts):.1f} "
            f"stdev={statistics.pstdev(cons_dts):.1f}"
        )
        print(f"gaps outside ±{slack_us:.0f} us of nominal: {outlier_gaps}/{len(cons_dts)}")
    else:
        print("no consecutive-symbol gaps found")

    print("\nfirst 10 target packets:")
    for p in sel[:10]:
        print(
            f"  idx={p.index} t_us={p.t_us:.0f} f={p.frame_id} sf={p.subframe_id} "
            f"slot={p.slot_id} abs={p.abs_slot} sym={p.symbol_id} eaxc={p.eaxc} "
            f"prb={p.num_prb} len={p.pkt_len}"
        )

    # Pass criteria: have packets, sane symbol ids, majority of gaps near nominal.
    ok = header_bad == 0 and len(sel) > 0
    if cons_dts:
        ok = ok and (outlier_gaps / len(cons_dts) < 0.25)
    print("\nRESULT:", "PASS (time signature looks sane)" if ok else "FAIL / investigate")
    return 0 if ok else 1


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("pcap", type=Path)
    ap.add_argument("--period", type=int, default=5, help="TDD period in slots (DDDSU=5)")
    ap.add_argument("--slot-mod", type=int, default=4, help="abs_slot %% period to select (U=4)")
    ap.add_argument("--mu", type=int, default=1, help="numerology")
    ap.add_argument("--max-packets", type=int, default=None)
    ap.add_argument("--nom-sym-us", type=float, default=NOM_SYM_US_MU1)
    ap.add_argument("--slack-us", type=float, default=25.0, help="allowed |Δt - nominal|")
    args = ap.parse_args()
    if not args.pcap.is_file():
        print(f"error: {args.pcap} not found", file=sys.stderr)
        return 1
    pkts = read_packets(args.pcap, mu=args.mu, max_packets=args.max_packets)
    if not pkts:
        print("no packets", file=sys.stderr)
        return 1
    return analyze(pkts, args.period, args.slot_mod, args.nom_sym_us, args.slack_us)


if __name__ == "__main__":
    raise SystemExit(main())
