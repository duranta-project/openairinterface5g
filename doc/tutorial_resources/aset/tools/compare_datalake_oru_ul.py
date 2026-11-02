#!/usr/bin/env python3
"""
Compare Aerial Data Lake (ClickHouse fapi + fh) against an O-RU UL pcap dump.

Mirrors pyaerial/notebooks/datalake_pusch_decoding.ipynb:

    fh_samp = int16.view(float16).astype(float32)
    rx_slot = swapaxes(fh_samp.view(complex64).reshape(4, 14, 273*12), 2, 0)
    # -> (n_re, 14, n_ant) with n_re=3276, n_ant=4

Data Lake always stores a fixed-size buffer (see data_lake.hpp):
    nPrbs = 273*12*14*4;  numFhSamples = nPrbs*2;   # = 366912 int16
even when the cell uses fewer antennas / PRBs (e.g. 2x106). Active IQ lives in
antennas [0..nRxAnt) and PRBs [0..BWPSize).

Requires: clickhouse_connect, numpy; matplotlib optional for --out-dir
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

import numpy as np

try:
    import clickhouse_connect
except ImportError as exc:  # pragma: no cover
    raise SystemExit("clickhouse_connect is required: pip install clickhouse-connect") from exc

# Fixed Data Lake FH layout (cuPHY-CP/data_lake/data_lake.hpp).
FH_MAX_ANT = 4
FH_MAX_PRB = 273
FH_N_SYM = 14
FH_N_RE = FH_MAX_PRB * 12  # 3276
FH_N_SAMPLES = FH_MAX_ANT * FH_N_SYM * FH_N_RE * 2  # 366912 int16 (I/Q float16)

# O-RU pcap layout (oru_ul_pcap_write).
ETH_LEN = 14
ECPRI_LEN = 8
RADIO_APP_LEN = 4
SECTION_HDR_LEN = 4
HEADER_LEN = ETH_LEN + ECPRI_LEN + RADIO_APP_LEN + SECTION_HDR_LEN

FAPI_COLS = [
    "CellId",
    "rnti",
    "TransformPrecoding",
    "ulDmrsSymbPos",
    "ulDmrsScramblingId",
    "SCID",
    "puschIdentity",
    "dmrsConfigType",
    "numDmrsCdmGrpsNoData",
    "dmrsPorts",
    "StartSymbolIndex",
    "NrOfSymbols",
    "rbStart",
    "rbSize",
    "BWPSize",
    "BWPStart",
    "mcsIndex",
    "qamModOrder",
    "targetCodeRate",
    "TBSize",
    "tbCrcFail",
    "CQI",
    "rssi",
    "timingAdvance",
]


def gold_sequence(c_init: int, length: int, nc: int = 1600) -> np.ndarray:
    n = length + nc + 31
    x1 = np.zeros(n, dtype=np.int8)
    x2 = np.zeros(n, dtype=np.int8)
    x1[0] = 1
    for i in range(31):
        x2[i] = (c_init >> i) & 1
    for i in range(n - 31):
        x1[i + 31] = (x1[i + 3] ^ x1[i]) & 1
        x2[i + 31] = (x2[i + 3] ^ x2[i + 2] ^ x2[i + 1] ^ x2[i]) & 1
    return (x1[nc : nc + length] ^ x2[nc : nc + length]) & 1


def pusch_dmrs_cp_ofdm(n_slot: int, symbol: int, n_id: int = 0, n_scid: int = 0, n_dmrs: int = 48) -> np.ndarray:
    """38.211 CP-OFDM PUSCH DMRS (config type 1, even REs)."""
    c_init = ((1 << 17) * (14 * n_slot + symbol + 1) * (2 * n_id + 1) + 2 * n_id + n_scid) % (1 << 31)
    c = gold_sequence(c_init, 2 * n_dmrs)
    return (1 - 2 * c[0::2]) / np.sqrt(2) + 1j * (1 - 2 * c[1::2]) / np.sqrt(2)


def corr_abs(a: np.ndarray, b: np.ndarray) -> float:
    return float(np.abs(np.vdot(a, b)) / (np.linalg.norm(a) * np.linalg.norm(b) + 1e-30))


def _pad_ants(rx: np.ndarray, n_ant_src: int) -> np.ndarray:
    """Pad (n_re, 14, n_ant_src) -> (max(n_re, FH_N_RE), 14, FH_MAX_ANT)."""
    n_re = rx.shape[0]
    out_re = max(n_re, FH_N_RE)
    out = np.zeros((out_re, FH_N_SYM, FH_MAX_ANT), dtype=np.complex64)
    out[:n_re, :, :n_ant_src] = rx
    return out


def unpack_fh_candidates(fh_data, n_rx_ant: int, active_prb: int) -> list[tuple[str, np.ndarray, int]]:
    """
    Return list of (label, rx_slot, n_prb_used) interpretations.

    rx_slot shape is always (RE, 14, 4) after padding.
    """
    raw = np.asarray(fh_data, dtype=np.int16)
    out: list[tuple[str, np.ndarray, int]] = []

    # A) notebook: float16, fixed 4 ant x 273 PRB (full buffer)
    if raw.size == FH_N_SAMPLES:
        f16 = raw.view(np.float16).astype(np.float32)
        rx = np.swapaxes(f16.view(np.complex64).reshape(FH_MAX_ANT, FH_N_SYM, FH_N_RE), 2, 0)
        out.append(("notebook_4x273", rx, FH_MAX_PRB))

    # B) float16 packed as nRxAnt x active_prb in the leading samples
    need = n_rx_ant * FH_N_SYM * active_prb * 12 * 2
    if raw.size >= need and need > 0:
        f16 = raw[:need].view(np.float16).astype(np.float32)
        rx = np.swapaxes(f16.view(np.complex64).reshape(n_rx_ant, FH_N_SYM, active_prb * 12), 2, 0)
        out.append((f"tight_{n_rx_ant}x{active_prb}", _pad_ants(rx, n_rx_ant), active_prb))

    # C) notebook axis order but only first nRxAnt planes are valid; still 273 PRB stride
    if raw.size == FH_N_SAMPLES:
        f16 = raw.view(np.float16).astype(np.float32)
        rx = np.swapaxes(f16.view(np.complex64).reshape(FH_MAX_ANT, FH_N_SYM, FH_N_RE), 2, 0)
        # zero unused ants already zero in pad; keep label distinct for scoring active_prb window
        out.append(("notebook_4x273_score_active", rx, active_prb))

    # D) alternate axis order: (ant, RE, sym) instead of (ant, sym, RE)
    if raw.size == FH_N_SAMPLES:
        f16 = raw.view(np.float16).astype(np.float32)
        c = f16.view(np.complex64).reshape(FH_MAX_ANT, FH_N_RE, FH_N_SYM)
        rx = np.transpose(c, (1, 2, 0))  # (RE, sym, ant)
        out.append(("ant_RE_sym_4x273", rx, FH_MAX_PRB))

    # E) (sym, ant, RE)
    if raw.size == FH_N_SAMPLES:
        f16 = raw.view(np.float16).astype(np.float32)
        c = f16.view(np.complex64).reshape(FH_N_SYM, FH_MAX_ANT, FH_N_RE)
        rx = np.transpose(c, (2, 0, 1))  # (RE, sym, ant)
        out.append(("sym_ant_RE_4x273", rx, FH_MAX_PRB))

    return out


def score_layout(rx: np.ndarray, n_rx_ant: int, n_prb: int, start_sym: int, rb_start: int, rb_size: int) -> float:
    """Score = mean |IQ| on grant REs at DMRS symbol (higher => likelier correct layout)."""
    re0 = rb_start * 12
    re1 = min((rb_start + rb_size) * 12, n_prb * 12, rx.shape[0])
    if re1 <= re0:
        return 0.0
    return float(np.abs(rx[re0:re1, start_sym, :n_rx_ant]).mean())


def select_fh_layout(
    fh_data,
    n_rx_ant: int,
    active_prb: int,
    start_sym: int,
    rb_start: int,
    rb_size: int,
    layout: str,
    verbose: bool,
) -> tuple[str, np.ndarray, int]:
    cands = unpack_fh_candidates(fh_data, n_rx_ant, active_prb)
    if not cands:
        raise ValueError("no FH unpack candidates")

    scored = []
    for label, rx, n_prb in cands:
        sc = score_layout(rx, n_rx_ant, n_prb, start_sym, rb_start, rb_size)
        scored.append((sc, label, rx, n_prb))

    if verbose or layout == "auto":
        print(f"\n=== FH packing scores (grant sym{start_sym} PRB[{rb_start}:{rb_start+rb_size}]) ===")
        for sc, label, _, n_prb in sorted(scored, key=lambda x: -x[0]):
            print(f"  {label:28s}  score={sc:.6g}  n_prb={n_prb}")

    if layout == "auto":
        sc, label, rx, n_prb = max(scored, key=lambda x: x[0])
        print(f"  -> selected {label}")
        return label, rx, n_prb

    for sc, label, rx, n_prb in scored:
        if label == layout or label.startswith(layout):
            return label, rx, n_prb
    raise SystemExit(f"unknown --layout {layout!r}; choose auto or one of: {[c[0] for c in cands]}")


def read_pcap_slot_group(path: Path, max_packets: int = 2000) -> dict[tuple, dict[int, np.ndarray]]:
    """Read oru_ul.pcap into {(frame, sf, slot, eaxc): {symbol: complex[n_re]}}."""
    groups: dict[tuple, dict[int, np.ndarray]] = {}
    with path.open("rb") as f:
        gh = f.read(24)
        if len(gh) < 24:
            raise ValueError("truncated pcap")
        magic = struct.unpack_from("I", gh, 0)[0]
        endian = "<" if magic == 0xA1B2C3D4 else ">"
        for _ in range(max_packets):
            ph = f.read(16)
            if len(ph) < 16:
                break
            incl_len = struct.unpack(endian + "IIII", ph)[2]
            pkt = f.read(incl_len)
            if len(pkt) < HEADER_LEN:
                continue
            eaxc = pkt[ETH_LEN + 5] & 0x0F
            frame_id = pkt[ETH_LEN + ECPRI_LEN + 1]
            sf_slot_sym = struct.unpack_from(">H", pkt, ETH_LEN + ECPRI_LEN + 2)[0]
            symbol_id = sf_slot_sym & 0x3F
            slot_id = (sf_slot_sym >> 6) & 0x3F
            subframe_id = (sf_slot_sym >> 12) & 0xF
            n_sc = (len(pkt) - HEADER_LEN) // 4
            iq = np.frombuffer(pkt[HEADER_LEN : HEADER_LEN + n_sc * 4], dtype=">i2").reshape(n_sc, 2)
            c = iq[:, 0].astype(np.float64) + 1j * iq[:, 1].astype(np.float64)
            key = (frame_id, subframe_id, slot_id, eaxc)
            groups.setdefault(key, {})[symbol_id] = c
    return groups


def print_fapi_row(rec, index: int) -> None:
    print(f"\n=== FAPI[{index}] SFN.Slot {int(rec.SFN)}.{int(rec.Slot)}  TsTaiNs={rec.TsTaiNs} ===")
    for col in FAPI_COLS:
        if col in rec.index:
            print(f"  {col:24s} {rec[col]}")
    # FAPI 10.02: TransformPrecoding 0=enabled (DFT-s-OFDM), 1=disabled (CP-OFDM)
    tp = int(rec.TransformPrecoding) if "TransformPrecoding" in rec.index else -1
    if tp == 0:
        print("  !! TransformPrecoding=0 => L1 expects DFT-s-OFDM DMRS (ZC), not CP-OFDM gold/QPSK")
    elif tp == 1:
        print("  TransformPrecoding=1 => L1 expects CP-OFDM (disabled TP)")


def summarize_rx_slot(
    rx_slot: np.ndarray,
    n_rx_ant: int,
    active_prb: int,
    start_sym: int,
    n_sym: int,
    rb_start: int,
    rb_size: int,
) -> None:
    print(
        f"  rx_slot shape (RE,sym,ant)={rx_slot.shape}  "
        f"using ant[0:{n_rx_ant}) PRB[0:{active_prb})"
    )
    print(f"  global |IQ|: peak={np.abs(rx_slot).max():.6g} mean={np.abs(rx_slot).mean():.6g}")
    active = rx_slot[: active_prb * 12, :, :n_rx_ant]
    print(f"  active window |IQ|: peak={np.abs(active).max():.6g} mean={np.abs(active).mean():.6g}")

    for ant in range(n_rx_ant):
        print(f"  --- ant {ant} ---")
        for sym in range(start_sym, min(start_sym + n_sym, FH_N_SYM)):
            mag = np.abs(rx_slot[: active_prb * 12, sym, ant])
            prb_mean = mag.reshape(active_prb, 12).mean(axis=1)
            alloc = prb_mean[rb_start : rb_start + rb_size]
            other = np.concatenate([prb_mean[:rb_start], prb_mean[rb_start + rb_size :]]) if rb_size < active_prb else np.array([0.0])
            print(
                f"    sym {sym:2d}: peak={mag.max():.5g}  "
                f"mean_alloc_PRB={alloc.mean():.5g}  mean_other_PRB={other.mean():.5g}"
            )
            if sym == start_sym:
                a = mag[rb_start * 12 : (rb_start + rb_size) * 12]
                even, odd = float(a[0::2].mean()), float(a[1::2].mean())
                print(f"           comb even={even:.5g} odd={odd:.5g} ratio={even / (odd + 1e-30):.2f}")
                print(f"           first 12 |IQ|: {np.round(a[:12], 5)}")


def dmrs_corr_report(
    rx_slot: np.ndarray,
    n_rx_ant: int,
    slot: int,
    start_sym: int,
    rb_start: int,
    rb_size: int,
    n_id: int,
    n_scid: int,
) -> None:
    n_dmrs = rb_size * 6
    ref = pusch_dmrs_cp_ofdm(slot, start_sym, n_id=n_id, n_scid=n_scid, n_dmrs=n_dmrs)
    print(f"  DMRS corr vs 38.211 CP-OFDM (slot={slot} sym={start_sym} N_ID={n_id} n_SCID={n_scid}):")
    for ant in range(n_rx_ant):
        re0 = rb_start * 12
        re1 = (rb_start + rb_size) * 12
        rx = rx_slot[re0:re1:2, start_sym, ant]
        if np.linalg.norm(rx) < 1e-12:
            print(f"    ant {ant}: empty")
            continue
        print(f"    ant {ant}: corr={corr_abs(rx, ref):.4f}")


def compare_pcap_to_fh(
    rx_slot: np.ndarray,
    n_rx_ant: int,
    pcap_groups: dict,
    slot: int,
    start_sym: int,
    n_sym: int,
    active_prb: int,
) -> None:
    candidates = []
    for (frame, sf, slot_id, eaxc), by_sym in pcap_groups.items():
        abs_slot = sf * 2 + slot_id  # mu=1
        if abs_slot == slot and start_sym in by_sym:
            candidates.append(((frame, sf, slot_id, eaxc), by_sym))
    if not candidates:
        print("  pcap: no group with matching abs slot / DMRS symbol")
        return

    key, _ = next((c for c in candidates if c[0][3] == 0), candidates[0])
    print(f"  pcap match example key frame/sf/slot/eaxc={key}")

    n_re_active = active_prb * 12
    for ant in range(n_rx_ant):
        match = next((c for c in candidates if c[0][3] == ant), None)
        if match is None:
            continue
        _, syms = match
        for sym in range(start_sym, start_sym + n_sym):
            if sym not in syms:
                continue
            fh = rx_slot[:n_re_active, sym, ant]
            pc = syms[sym][:n_re_active]
            n = min(len(fh), len(pc))
            a = fh[:n] / (np.linalg.norm(fh[:n]) + 1e-30)
            b = pc[:n] / (np.linalg.norm(pc[:n]) + 1e-30)
            print(
                f"    ant{ant} sym{sym}: shape-corr={corr_abs(a, b):.4f}  "
                f"FH_peak={np.abs(fh).max():.5g}  pcap_peak={np.abs(pc).max():.1f}"
            )


def maybe_plot(
    rx_slot: np.ndarray,
    n_rx_ant: int,
    active_prb: int,
    start_sym: int,
    n_sym: int,
    out_dir: Path,
    tag: str,
) -> None:
    import matplotlib.pyplot as plt

    out_dir.mkdir(parents=True, exist_ok=True)
    symbols = list(range(start_sym, min(start_sym + n_sym, FH_N_SYM)))
    n_re_plot = min(active_prb, 16) * 12
    for ant in range(n_rx_ant):
        grid = np.abs(rx_slot[:n_re_plot, symbols, ant]).T
        fig, ax = plt.subplots(figsize=(12, 4))
        im = ax.imshow(grid, aspect="auto", origin="lower", interpolation="nearest", cmap="viridis")
        ax.set_yticks(range(len(symbols)))
        ax.set_yticklabels(symbols)
        ax.set_xlabel("RE index (first 16 PRBs)")
        ax.set_ylabel("OFDM symbol")
        ax.set_title(f"Data Lake FH |IQ| — {tag} ant{ant}")
        fig.colorbar(im, ax=ax, label="|IQ|")
        fig.tight_layout()
        path = out_dir / f"datalake_fh_heatmap_{tag}_ant{ant}.png"
        fig.savefig(path, dpi=150)
        print(f"  wrote {path}")
        plt.close(fig)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default="localhost")
    ap.add_argument("--port", type=int, default=8123)
    ap.add_argument("--limit", type=int, default=5, help="earliest fapi rows to list")
    ap.add_argument("--index", type=int, default=0, help="which fetched fapi row to analyze")
    ap.add_argument("--active-prb", type=int, default=106, help="PRBs in use inside the FH buffer")
    ap.add_argument(
        "--layout",
        default="auto",
        help="FH unpack layout: auto (default) | notebook_4x273 | tight_2x106 | ant_RE_sym_4x273 | ...",
    )
    ap.add_argument("--pcap", type=Path, default=None, help="optional oru_ul.pcap")
    ap.add_argument("--out-dir", type=Path, default=None, help="optional heatmap directory")
    ap.add_argument("--rnti-ne", type=int, default=20000, help="exclude RNTI (notebook default)")
    args = ap.parse_args()

    client = clickhouse_connect.get_client(host=args.host, port=args.port)
    q = f"select * from fapi where rnti != {args.rnti_ne} order by TsTaiNs limit {args.limit}"
    fapi = client.query_df(q)
    if fapi.empty:
        fapi = client.query_df(f"select * from fapi order by TsTaiNs limit {args.limit}")
    if fapi.empty:
        print("no rows in fapi", file=sys.stderr)
        return 1

    print(f"fetched {len(fapi)} fapi rows from {args.host}")
    for i in range(len(fapi)):
        print_fapi_row(fapi.iloc[i], i)

    idx = min(max(args.index, 0), len(fapi) - 1)
    rec = fapi.iloc[idx]
    cell_id = int(rec.CellId)
    fh = client.query_df(
        f"""select * from fh where
            TsTaiNs == toDateTime64('{rec.TsTaiNs.timestamp()}', 9) and
            CellId == {cell_id}"""
    )
    if fh.empty:
        fh = client.query_df(
            f"""select * from fh where
                TsTaiNs == toDateTime64('{rec.TsTaiNs.timestamp()}', 9) and
                SFN == {int(rec.SFN)} and Slot == {int(rec.Slot)}"""
        )
    if fh.empty:
        print(f"\nNo FH row for TsTaiNs={rec.TsTaiNs} CellId={cell_id}", file=sys.stderr)
        return 1

    fh_row = fh.iloc[0]
    n_rx = int(fh_row.nRxAnt)
    print(f"\n=== FH match SFN.Slot {int(fh_row.SFN)}.{int(fh_row.Slot)} nRxAnt={n_rx} nUEs={int(fh_row.nUEs)} ===")
    print(f"  fhData length={len(fh_row.fhData)} (fixed buffer expect {FH_N_SAMPLES})")

    start_sym = int(rec.StartSymbolIndex)
    n_sym = int(rec.NrOfSymbols)
    rb_start = int(rec.rbStart)
    rb_size = int(rec.rbSize)
    slot = int(rec.Slot)

    label, rx_slot, layout_prb = select_fh_layout(
        fh_row.fhData,
        n_rx_ant=n_rx,
        active_prb=args.active_prb,
        start_sym=start_sym,
        rb_start=rb_start,
        rb_size=rb_size,
        layout=args.layout,
        verbose=True,
    )
    active_prb = min(args.active_prb, layout_prb)

    summarize_rx_slot(rx_slot, n_rx, active_prb, start_sym, n_sym, rb_start, rb_size)
    dmrs_corr_report(
        rx_slot,
        n_rx_ant=n_rx,
        slot=slot,
        start_sym=start_sym,
        rb_start=rb_start,
        rb_size=rb_size,
        n_id=int(rec.ulDmrsScramblingId),
        n_scid=int(rec.SCID),
    )

    if args.pcap is not None:
        if not args.pcap.is_file():
            print(f"pcap not found: {args.pcap}", file=sys.stderr)
            return 1
        print(f"\n=== compare to {args.pcap} (layout={label}) ===")
        groups = read_pcap_slot_group(args.pcap)
        compare_pcap_to_fh(rx_slot, n_rx, groups, slot, start_sym, n_sym, active_prb)

    if args.out_dir is not None:
        tag = f"sfn{int(rec.SFN)}_slot{slot}_{label}"
        maybe_plot(rx_slot, n_rx, active_prb, start_sym, n_sym, args.out_dir, tag)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
