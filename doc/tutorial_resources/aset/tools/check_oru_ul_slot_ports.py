#!/usr/bin/env python3
"""
Per-eAxC U-/C-plane completeness for one O-RAN frameId.abs_slot.

Useful for catching:
  - missing early U-plane symbols on eAxC 1 when Type-1 C-plane arrives late
  - PUSCH U on a PRACH-only RO (stale Type-1)
  - 8-bit frameId aliases (clusters ~2.56 s apart)

Example:
  python3 check_oru_ul_slot_ports.py /tmp/oru_ul.pcap --frame 141 --abs-slot 4
  python3 check_oru_ul_slot_ports.py /tmp/oru_ul.pcap --frame 117 --abs-slot 9
"""

from __future__ import annotations

import argparse
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
ECPRI_MSG_IQ = 0
ECPRI_MSG_RTC = 2  # C-plane


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
    msg: int
    pkt_len: int
    num_prb: int
    section_id: int


def _endian(magic: int) -> str:
    if magic == PCAP_MAGIC_LE:
        return "<"
    if magic == PCAP_MAGIC_BE:
        return ">"
    raise ValueError(f"unsupported pcap magic 0x{magic:08x}")


def read_packets(path: Path, mu: int = 1) -> list[Pkt]:
    slots_per_subframe = 1 << mu
    out: list[Pkt] = []
    with path.open("rb") as f:
        gh = f.read(24)
        if len(gh) < 24:
            raise ValueError("truncated pcap")
        endian = _endian(struct.unpack_from("I", gh, 0)[0])
        idx = 0
        while True:
            ph = f.read(16)
            if len(ph) < 16:
                break
            ts_sec, ts_usec, incl_len, _ = struct.unpack(endian + "IIII", ph)
            pkt = f.read(incl_len)
            if len(pkt) < HEADER_LEN:
                idx += 1
                continue
            msg = pkt[ETH_LEN + 1]
            frame_id = pkt[ETH_LEN + ECPRI_LEN + 1]
            sf_slot_sym = struct.unpack_from(">H", pkt, ETH_LEN + ECPRI_LEN + 2)[0]
            symbol_id = sf_slot_sym & 0x3F
            slot_id = (sf_slot_sym >> 6) & 0x3F
            subframe_id = (sf_slot_sym >> 12) & 0xF
            eaxc = pkt[ETH_LEN + 5] & 0x0F
            word = struct.unpack_from(">I", pkt, ETH_LEN + ECPRI_LEN + RADIO_APP_LEN)[0]
            num_prb = word & 0xFF
            section_id = (word >> 20) & 0xFFF
            if num_prb == 0 and incl_len >= HEADER_LEN + 48:
                payload = incl_len - HEADER_LEN
                num_prb = (payload // 4) // 12
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
                    msg=msg,
                    pkt_len=incl_len,
                    num_prb=num_prb,
                    section_id=section_id,
                )
            )
            idx += 1
    return out


def classify(p: Pkt) -> str:
    if p.msg == ECPRI_MSG_RTC or p.pkt_len < 100:
        return "CP"
    if p.pkt_len > 1000:
        return "PUSCH"
    if 500 <= p.pkt_len <= 700:
        return "PRACH"
    return f"LEN{p.pkt_len}"


def cluster_by_time(pkts: list[Pkt], gap_us: float) -> list[list[Pkt]]:
    if not pkts:
        return []
    ordered = sorted(pkts, key=lambda p: p.t_us)
    clusters: list[list[Pkt]] = [[ordered[0]]]
    for p in ordered[1:]:
        if p.t_us - clusters[-1][-1].t_us < gap_us:
            clusters[-1].append(p)
        else:
            clusters.append([p])
    return clusters


def analyze_cluster(cluster: list[Pkt], cluster_idx: int, expect_syms: int, verbose: bool) -> int:
    """Print one occasion; return number of port completeness failures."""
    t0 = cluster[0].t_us
    span = cluster[-1].t_us - t0
    print(
        f"\n=== cluster {cluster_idx}: n={len(cluster)} span_us={span:.0f} "
        f"idx={cluster[0].index}..{cluster[-1].index} ==="
    )

    by_port_syms: dict[int, set[int]] = defaultdict(set)
    by_port_kind: dict[int, dict[str, int]] = defaultdict(lambda: defaultdict(int))
    cp_lines: list[str] = []
    u_lines: list[str] = []

    for p in sorted(cluster, key=lambda x: x.index):
        kind = classify(p)
        by_port_kind[p.eaxc][kind] += 1
        if kind == "CP":
            cp_lines.append(
                f"  CP   idx={p.index:5d} eaxc={p.eaxc} sect={p.section_id} "
                f"prb={p.num_prb} len={p.pkt_len}"
            )
        else:
            by_port_syms[p.eaxc].add(p.symbol_id)
            u_lines.append(
                f"  {kind:5} idx={p.index:5d} eaxc={p.eaxc} sym={p.symbol_id:2d} "
                f"sect={p.section_id} prb={p.num_prb} len={p.pkt_len}"
            )

    if cp_lines:
        print("-- C-plane --")
        for line in cp_lines:
            print(line)
    if verbose and u_lines:
        print("-- U-plane --")
        for line in u_lines:
            print(line)

    print("-- per-port U-plane --")
    failures = 0
    ports = sorted(set(by_port_kind) | set(by_port_syms))
    for port in ports:
        kinds = dict(by_port_kind[port])
        syms = sorted(by_port_syms[port])
        missing = [s for s in range(expect_syms) if s not in by_port_syms[port]]
        u_kinds = {k: n for k, n in kinds.items() if k in ("PUSCH", "PRACH")}
        if u_kinds:
            expect = expect_syms
            if "PRACH" in u_kinds and "PUSCH" not in u_kinds:
                prach_expect = max(syms) + 1 if syms else 0
                missing = [s for s in range(prach_expect) if s not in by_port_syms[port]]
                expect = prach_expect
            status = "OK" if not missing else "MISSING"
            if missing:
                failures += 1
            print(
                f"  port {port}: {kinds} syms={syms} missing={missing} -> {status}"
                f" (expect 0..{expect - 1})"
            )
        else:
            print(f"  port {port}: {kinds} (C-plane only)")

    all_kinds: set[str] = set()
    for k in by_port_kind.values():
        all_kinds |= set(k)
    if "PUSCH" in all_kinds and "PRACH" in all_kinds:
        print("  WARN: both PUSCH and PRACH U-plane in this occasion")
        failures += 1

    return failures


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("pcap", type=Path, help="ORU UL pcap path")
    ap.add_argument("--frame", type=int, required=True, help="O-RAN frameId (8-bit)")
    ap.add_argument("--abs-slot", type=int, required=True, help="Absolute slot in frame (mu=1: sf*2+slot)")
    ap.add_argument("--mu", type=int, default=1)
    ap.add_argument(
        "--gap-ms",
        type=float,
        default=5.0,
        help="Wall-clock gap to split 8-bit frameId aliases (default 5 ms)",
    )
    ap.add_argument(
        "--cluster",
        type=int,
        default=None,
        help="Only analyze this cluster index (default: all)",
    )
    ap.add_argument("--expect-syms", type=int, default=14, help="Expected PUSCH symbols per port")
    ap.add_argument("-v", "--verbose", action="store_true", help="List every U-plane packet")
    args = ap.parse_args()

    pkts = read_packets(args.pcap, mu=args.mu)
    frame_id = args.frame % 256
    sel = [p for p in pkts if p.frame_id == frame_id and p.abs_slot == args.abs_slot]
    print(
        f"pcap={args.pcap} parsed={len(pkts)} match frameId={frame_id} "
        f"abs_slot={args.abs_slot}: {len(sel)}"
    )
    if not sel:
        print("ERROR: no matching packets", file=sys.stderr)
        return 2

    sf = args.abs_slot // (1 << args.mu)
    slot = args.abs_slot % (1 << args.mu)
    print(f"equiv subframe_id={sf} slotId={slot} (mu={args.mu})")

    clusters = cluster_by_time(sel, gap_us=args.gap_ms * 1e3)
    print(f"clusters (gap>{args.gap_ms} ms): {len(clusters)}")
    if len(clusters) > 1:
        print("note: multiple clusters => 8-bit frameId aliases (~2.56 s apart), not true duplicates")
        for i in range(1, len(clusters)):
            dt = clusters[i][0].t_us - clusters[i - 1][0].t_us
            print(f"  gap cluster{i - 1}->#{i}: {dt / 1e3:.1f} ms (~{dt / 10000:.1f} frames@10ms)")

    total_fail = 0
    for i, cl in enumerate(clusters):
        if args.cluster is not None and i != args.cluster:
            continue
        total_fail += analyze_cluster(cl, i, args.expect_syms, args.verbose)

    if total_fail:
        print(f"\nRESULT: FAIL ({total_fail} port/occasion issues)")
        return 1
    print("\nRESULT: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
