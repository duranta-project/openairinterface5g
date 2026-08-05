#!/usr/bin/env python3
"""
Compare Aerial PUSCH_H5DUMP (PUSCH_Debug*.h5) against Data Lake (ClickHouse fapi + fh).

H5 is produced when cuphydriver is built with #define PUSCH_H5DUMP in
  cuPHY-CP/cuphydriver/src/uplink/phypusch_aggr.cpp
Empty ~96-byte PUSCH_Debug*.h5 stubs are created at startup; a real dump
needs an OAM arm before the next CRC fail:

  PYTHONPATH=/opt/nvidia/cuBB/build.aarch64/cuPHY-CP/cuphyoam \\
    python3 /opt/nvidia/cuBB/cuPHY-CP/cuphyoam/examples/aerial_pusch_h5dump_next_crc.py

Usage:
  python3 compare_h5_datalake.py PUSCH_Debug....h5
  python3 compare_h5_datalake.py PUSCH_Debug....h5 --list-fapi
  python3 compare_h5_datalake.py PUSCH_Debug....h5 --allow-rnti-mismatch
  python3 compare_h5_datalake.py PUSCH_Debug....h5 --fapi-index 2

Requires: h5py, clickhouse_connect, numpy
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np

try:
    import h5py
except ImportError as exc:  # pragma: no cover
    raise SystemExit("h5py is required: pip install h5py") from exc

try:
    import clickhouse_connect
except ImportError as exc:  # pragma: no cover
    raise SystemExit("clickhouse_connect is required: pip install clickhouse-connect") from exc

FH_MAX_ANT = 4
FH_MAX_PRB = 273
FH_N_SYM = 14
FH_N_RE = FH_MAX_PRB * 12
FH_N_SAMPLES = FH_MAX_ANT * FH_N_SYM * FH_N_RE * 2

# H5 dataset name -> (fapi column, optional cast)
H5_TO_FAPI = [
    ("rnti", "rnti"),
    ("scid", "SCID"),
    ("enableTfPrcd", "TransformPrecoding"),  # note: naming differs; see print
    ("dmrsScrmId", "ulDmrsScramblingId"),
    ("dmrsSymLocBmsk", "ulDmrsSymbPos"),
    ("nDmrsCdmGrpsNoData", "numDmrsCdmGrpsNoData"),
    ("dmrsPortBmsk", "dmrsPorts"),
    ("startPrb", "rbStart"),
    ("nPrb", "rbSize"),
    ("startSym", "StartSymbolIndex"),
    ("nSym", "NrOfSymbols"),
    ("targetCodeRate", "targetCodeRate"),
    ("qamModOrder", "qamModOrder"),
    ("tbSizeBytes", "TBSize"),
    ("puschIdentity", "puschIdentity"),
]


def corr_abs(a: np.ndarray, b: np.ndarray) -> float:
    return float(np.abs(np.vdot(a, b)) / (np.linalg.norm(a) * np.linalg.norm(b) + 1e-30))


def as_complex(arr: np.ndarray) -> np.ndarray:
    """Interpret HDF5 array as complex IQ (keeps multi-dim shape when possible)."""
    a = np.asarray(arr)
    if np.iscomplexobj(a):
        return a.astype(np.complex64, copy=False)
    # structured re/im (float16 or float32) — Aerial PUSCH_H5DUMP DataRx0
    if a.dtype.names and "re" in a.dtype.names and "im" in a.dtype.names:
        return a["re"].astype(np.float32) + 1j * a["im"].astype(np.float32)
    if a.dtype == np.float16:
        f = a.astype(np.float32)
    elif a.dtype in (np.float32, np.float64):
        f = a.astype(np.float32, copy=False)
    elif a.dtype in (np.int16, np.uint16):
        f = a.view(np.float16).astype(np.float32)
    else:
        raise TypeError(f"unsupported dtype {a.dtype}")
    if f.size % 2:
        raise ValueError(f"odd float count {f.size} cannot form complex")
    # interleaved I/Q along last axis or flat
    if f.ndim >= 1 and f.shape[-1] % 2 == 0:
        return f.reshape(*f.shape[:-1], f.shape[-1] // 2, 2).astype(np.float32).view(np.complex64)[..., 0]
    return f.reshape(-1).view(np.complex64)


def reshape_rx_candidates(c: np.ndarray) -> list[tuple[str, np.ndarray]]:
    """
    Return (label, rx) with rx shaped (n_re, 14, n_ant).
    Tries layouts that fit common cuPHY DataRx dumps.
    """
    flat = np.asarray(c).reshape(-1)
    out: list[tuple[str, np.ndarray]] = []
    n = flat.size

    def add(label: str, arr: np.ndarray) -> None:
        # normalize to (RE, sym, ant)
        out.append((label, np.ascontiguousarray(arr.astype(np.complex64))))

    # (ant, sym, re) -> (re, sym, ant)
    for n_ant in (2, 4):
        for n_prb in (106, 273):
            n_re = n_prb * 12
            need = n_ant * FH_N_SYM * n_re
            if n == need:
                x = flat.reshape(n_ant, FH_N_SYM, n_re)
                add(f"ant_sym_re_{n_ant}x{n_prb}", np.transpose(x, (2, 1, 0)))
                x = flat.reshape(n_ant, n_re, FH_N_SYM)
                add(f"ant_re_sym_{n_ant}x{n_prb}", np.transpose(x, (1, 2, 0)))
                x = flat.reshape(FH_N_SYM, n_ant, n_re)
                add(f"sym_ant_re_{n_ant}x{n_prb}", np.transpose(x, (2, 0, 1)))

    # already (re, sym, ant)
    for n_ant in (2, 4):
        for n_prb in (106, 273):
            n_re = n_prb * 12
            if n == n_re * FH_N_SYM * n_ant:
                add(f"re_sym_ant_{n_ant}x{n_prb}", flat.reshape(n_re, FH_N_SYM, n_ant))

    return out


def score_grant(rx: np.ndarray, start_sym: int, rb_start: int, rb_size: int, n_ant: int) -> float:
    re0 = rb_start * 12
    re1 = min((rb_start + rb_size) * 12, rx.shape[0])
    if re1 <= re0 or start_sym >= rx.shape[1]:
        return 0.0
    n_ant = min(n_ant, rx.shape[2])
    return float(np.abs(rx[re0:re1, start_sym, :n_ant]).mean())


def pick_h5_rx(
    data_rx: np.ndarray,
    start_sym: int,
    rb_start: int,
    rb_size: int,
    n_ant: int,
) -> tuple[str, np.ndarray]:
    # Fast path: Aerial dump is already (nAnt, nSym, nRE) structured re/im
    c = as_complex(data_rx)
    if c.ndim == 3 and c.shape[1] == FH_N_SYM:
        # (ant, sym, re) -> (re, sym, ant)
        rx = np.transpose(c, (2, 1, 0))
        print(f"=== H5 DataRx layout: ant_sym_re (native {c.shape}) -> (re,sym,ant)={rx.shape} ===")
        return f"native_ant_sym_re_{c.shape[0]}x{c.shape[2]//12}", rx

    cands = reshape_rx_candidates(c.reshape(-1))
    if not cands:
        # last resort: treat as notebook (4,14,273*12)
        flat = c.reshape(-1)
        if flat.size == FH_MAX_ANT * FH_N_SYM * FH_N_RE:
            rx = np.swapaxes(flat.reshape(FH_MAX_ANT, FH_N_SYM, FH_N_RE), 2, 0)
            return "fallback_notebook_4x273", rx
        raise ValueError(f"cannot reshape DataRx size={np.asarray(data_rx).size}")

    scored = [(score_grant(rx, start_sym, rb_start, rb_size, n_ant), lab, rx) for lab, rx in cands]
    scored.sort(key=lambda t: -t[0])
    print("=== H5 DataRx layout scores ===")
    for sc, lab, _ in scored[:8]:
        print(f"  {lab:28s} score={sc:.6g}")
    sc, lab, rx = scored[0]
    print(f"  -> selected {lab}")
    return lab, rx


def unpack_fh_tight(fh_data, n_rx_ant: int, active_prb: int) -> np.ndarray:
    """Leading samples as float16 IQ: (n_rx_ant, 14, active_prb*12) -> (RE,14,ant) padded to 4 ant."""
    raw = np.asarray(fh_data, dtype=np.int16)
    need = n_rx_ant * FH_N_SYM * active_prb * 12 * 2
    if raw.size < need:
        raise ValueError(f"fhData len {raw.size} < need {need}")
    f16 = raw[:need].view(np.float16).astype(np.float32)
    rx = np.swapaxes(f16.view(np.complex64).reshape(n_rx_ant, FH_N_SYM, active_prb * 12), 2, 0)
    out = np.zeros((max(rx.shape[0], FH_N_RE), FH_N_SYM, FH_MAX_ANT), dtype=np.complex64)
    out[: rx.shape[0], :, :n_rx_ant] = rx
    return out


def unpack_fh_notebook(fh_data) -> np.ndarray:
    raw = np.asarray(fh_data, dtype=np.int16)
    if raw.size != FH_N_SAMPLES:
        raise ValueError(f"fhData len {raw.size} != {FH_N_SAMPLES}")
    f16 = raw.view(np.float16).astype(np.float32)
    return np.swapaxes(f16.view(np.complex64).reshape(FH_MAX_ANT, FH_N_SYM, FH_N_RE), 2, 0)


def h5_scalar(h: h5py.File, name: str, index: int = 0):
    if name not in h:
        return None
    d = np.asarray(h[name]).reshape(-1)
    if d.size == 0:
        return None
    return d[min(index, d.size - 1)].item()


def is_valid_h5(path: Path) -> bool:
    try:
        with h5py.File(path, "r") as h:
            return len(h.keys()) > 0
    except OSError:
        return False


def pick_h5(explicit: Path | None, h5_dir: Path) -> Path:
    if explicit is not None:
        return explicit
    cands = sorted(h5_dir.glob("PUSCH_Debug*.h5"), key=lambda p: p.stat().st_size, reverse=True)
    valid = [p for p in cands if p.stat().st_size > 4096 and is_valid_h5(p)]
    if not valid:
        # fall back to any non-tiny file even if open fails later with a clearer message
        valid = [p for p in cands if p.stat().st_size > 4096]
    if not valid:
        raise FileNotFoundError(
            f"no usable PUSCH_Debug*.h5 under {h5_dir} "
            f"(found {[ (p.name, p.stat().st_size) for p in cands[:5]]})"
        )
    return valid[0]


def list_h5(path: Path) -> None:
    with h5py.File(path, "r") as h:
        print(f"\n=== {path.name} datasets ({len(h.keys())}) ===")
        for k in sorted(h.keys()):
            d = h[k]
            if hasattr(d, "shape"):
                print(f"  {k:40s} shape={d.shape} dtype={d.dtype}")


def find_data_rx_key(h: h5py.File) -> str:
    for k in ("DataRx0", "dataRx0", "DataRx", "dataRx"):
        if k in h:
            return k
    for k in h.keys():
        if "datarx" in k.lower():
            return k
    raise KeyError(f"no DataRx* dataset in {list(h.keys())[:20]}...")


def compare_iq(
    h5_rx: np.ndarray,
    fh_rx: np.ndarray,
    n_ant: int,
    start_sym: int,
    n_sym: int,
    rb_start: int,
    rb_size: int,
    active_prb: int,
) -> None:
    n_re = min(active_prb * 12, h5_rx.shape[0], fh_rx.shape[0])
    n_ant = min(n_ant, h5_rx.shape[2], fh_rx.shape[2])
    print(f"\n=== IQ compare (first {active_prb} PRB, ant[0:{n_ant})) ===")
    print(f"  H5  peak={np.abs(h5_rx[:n_re,:,:n_ant]).max():.6g} mean={np.abs(h5_rx[:n_re,:,:n_ant]).mean():.6g}")
    print(f"  FH  peak={np.abs(fh_rx[:n_re,:,:n_ant]).max():.6g} mean={np.abs(fh_rx[:n_re,:,:n_ant]).mean():.6g}")

    for ant in range(n_ant):
        for sym in range(start_sym, min(start_sym + n_sym, FH_N_SYM)):
            a = h5_rx[:n_re, sym, ant]
            b = fh_rx[:n_re, sym, ant]
            # full-band shape corr (normalized)
            shape = corr_abs(a, b)
            re0 = rb_start * 12
            re1 = (rb_start + rb_size) * 12
            grant = corr_abs(a[re0:re1], b[re0:re1])
            print(
                f"  ant{ant} sym{sym}: shape-corr={shape:.4f} grant-corr={grant:.4f} "
                f"H5_peak={np.abs(a[re0:re1]).max():.5g} FH_peak={np.abs(b[re0:re1]).max():.5g}"
            )


def compare_fapi(h: h5py.File, rec) -> None:
    print("\n=== FAPI / H5 param compare ===")
    # slot / sfn if present
    for name, label in (("slotNumPerUeGrp", "Slot"), ("slotNumPerCell", "Slot")):
        v = h5_scalar(h, name)
        if v is not None and "Slot" in rec.index:
            print(f"  {label:24s} H5={int(v)}  fapi={int(rec.Slot)}  {'OK' if int(v)==int(rec.Slot) else 'DIFF'}")
            break

    for h5_name, fapi_name in H5_TO_FAPI:
        hv = h5_scalar(h, h5_name)
        if hv is None:
            continue
        if fapi_name not in rec.index:
            print(f"  {h5_name:24s} H5={hv}  (no fapi col {fapi_name})")
            continue
        fv = rec[fapi_name]
        # TransformPrecoding / enableTfPrcd: H5 may store 0/1 as "enabled" flag
        same = (int(hv) == int(fv)) if np.issubdtype(type(hv), np.integer) or isinstance(hv, (int, np.integer)) else (hv == fv)
        try:
            same = int(hv) == int(fv)
        except (TypeError, ValueError):
            same = hv == fv
        mark = "OK" if same else "DIFF"
        print(f"  {h5_name:24s} H5={hv}  fapi.{fapi_name}={fv}  {mark}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument(
        "h5",
        type=Path,
        nargs="?",
        default=None,
        help="PUSCH_Debug*.h5 path (default: newest under --h5-dir)",
    )
    ap.add_argument(
        "--h5-dir",
        type=Path,
        default=Path.home() / "aerial-cuda-accelerated-ran",
        help="directory to search for PUSCH_Debug*.h5",
    )
    ap.add_argument("--list", action="store_true", help="only list H5 datasets")
    ap.add_argument(
        "--list-fapi",
        action="store_true",
        help="list ClickHouse fapi rows matching H5 grant keys, then exit",
    )
    ap.add_argument("--host", default="localhost")
    ap.add_argument("--port", type=int, default=8123)
    ap.add_argument("--active-prb", type=int, default=106)
    ap.add_argument("--fh-layout", choices=("auto", "tight", "notebook"), default="auto")
    ap.add_argument("--rnti-ne", type=int, default=20000)
    ap.add_argument(
        "--allow-rnti-mismatch",
        action="store_true",
        help="if H5 rnti is missing from fapi, fall back to same slot/grant shape "
             "(DMRS may still correlate; data symbols will not)",
    )
    ap.add_argument(
        "--best-iq",
        action="store_true",
        help="with --allow-rnti-mismatch, pick the same-shape fapi/fh row whose "
             "grant DMRS correlates best with H5 DataRx0 (scans up to --scan-limit)",
    )
    ap.add_argument("--scan-limit", type=int, default=30,
                    help="max same-shape rows to score with --best-iq")
    ap.add_argument(
        "--fapi-index",
        type=int,
        default=0,
        help="which matching fapi row to use (after sorting by TsTaiNs)",
    )
    args = ap.parse_args()

    try:
        h5_path = pick_h5(args.h5, args.h5_dir)
    except FileNotFoundError as e:
        print(e, file=sys.stderr)
        return 1
    if args.h5 is None:
        print(f"using H5: {h5_path} ({h5_path.stat().st_size} bytes)")

    if not h5_path.is_file():
        print(f"not found: {h5_path}", file=sys.stderr)
        return 1

    try:
        list_h5(h5_path)
    except OSError as e:
        print(f"cannot open {h5_path}: {e}", file=sys.stderr)
        print("hint: truncated dumps are common if Aerial exited mid-write; pick an older/larger file", file=sys.stderr)
        return 1
    if args.list:
        return 0

    try:
        hfile = h5py.File(h5_path, "r")
    except OSError as e:
        print(f"cannot open {h5_path}: {e}", file=sys.stderr)
        return 1

    with hfile as h:
        dr_key = find_data_rx_key(h)
        data_rx = np.array(h[dr_key])
        print(f"\nDataRx key={dr_key} shape={data_rx.shape} dtype={data_rx.dtype}")

        start_sym = int(h5_scalar(h, "startSym") or 10)
        n_sym = int(h5_scalar(h, "nSym") or 3)
        rb_start = int(h5_scalar(h, "startPrb") or 0)
        rb_size = int(h5_scalar(h, "nPrb") or 8)
        rnti = int(h5_scalar(h, "rnti") or -1)
        slot = h5_scalar(h, "slotNumPerUeGrp")
        if slot is None:
            slot = h5_scalar(h, "slotNumPerCell")
        slot = int(slot) if slot is not None else None
        n_ant = 2
        if "DataRx0" in h or dr_key:
            # infer from size later; default 2 for ASET
            pass

        print(
            f"H5 grant: rnti={rnti} slot={slot} startSym={start_sym} nSym={n_sym} "
            f"rbStart={rb_start} rbSize={rb_size}"
        )

        h5_label, h5_rx = pick_h5_rx(data_rx, start_sym, rb_start, rb_size, n_ant)
        n_ant = min(n_ant, h5_rx.shape[2])

        client = clickhouse_connect.get_client(host=args.host, port=args.port)

        cols = (
            "SFN, Slot, rnti, StartSymbolIndex, rbStart, rbSize, NrOfSymbols, "
            "tbCrcFail, TransformPrecoding, TsTaiNs"
        )

        def qf(where: str, limit: int = 50, newest_first: bool = False):
            order = "TsTaiNs desc" if newest_first else "TsTaiNs"
            return client.query_df(
                f"select {cols} from fapi where {where} order by {order} limit {limit}"
            )

        exact = None
        if rnti >= 0:
            clauses = [f"rnti = {rnti}"]
            if slot is not None:
                clauses.append(f"Slot = {slot}")
            clauses += [
                f"StartSymbolIndex = {start_sym}",
                f"rbStart = {rb_start}",
                f"rbSize = {rb_size}",
            ]
            exact = qf(" and ".join(clauses))
            if exact.empty:
                exact = qf(
                    f"rnti = {rnti} and StartSymbolIndex = {start_sym} "
                    f"and rbStart = {rb_start} and rbSize = {rb_size}"
                )
            if exact.empty:
                exact = qf(f"rnti = {rnti}")

        shape_where = [
            f"rnti != {args.rnti_ne}",
            f"StartSymbolIndex = {start_sym}",
            f"rbStart = {rb_start}",
            f"rbSize = {rb_size}",
        ]
        if slot is not None:
            shape_where.insert(1, f"Slot = {slot}")
        shape = qf(" and ".join(shape_where), limit=max(50, args.scan_limit),
                   newest_first=True)

        n_exact = 0 if exact is None else len(exact)
        print(f"\n=== fapi candidates (exact rnti={rnti}: {n_exact}, same-shape: {len(shape)}) ===")
        show = exact if exact is not None and not exact.empty else shape
        if show is not None and not show.empty:
            print(show.to_string(index=True))
        else:
            print("  (none)")

        if args.list_fapi:
            return 0 if show is not None and not show.empty else 1

        if exact is not None and not exact.empty:
            fapi = exact
            match_kind = "exact-rnti"
        elif args.allow_rnti_mismatch and not shape.empty:
            fapi = shape
            match_kind = "shape-only (RNTI MISMATCH allowed)"
            print(
                "WARNING: H5 rnti not in datalake; using same-shape Msg3. "
                "DMRS can still correlate; data symbols usually will not.",
                file=sys.stderr,
            )
            if args.best_iq:
                print(f"\n=== scoring up to {args.scan_limit} fh rows vs H5 grant DMRS ===")
                scores = []
                for i, tip in fapi.head(args.scan_limit).iterrows():
                    fh_try = client.query_df(
                        f"""select * from fh where
                            TsTaiNs == toDateTime64('{tip.TsTaiNs.timestamp()}', 9) and
                            SFN == {int(tip.SFN)} and Slot == {int(tip.Slot)} limit 1"""
                    )
                    if fh_try.empty:
                        continue
                    try:
                        fh_rx = unpack_fh_tight(fh_try.iloc[0].fhData,
                                                int(fh_try.iloc[0].nRxAnt),
                                                args.active_prb)
                    except ValueError:
                        continue
                    a = h5_rx[rb_start * 12 : (rb_start + rb_size) * 12, start_sym, 0]
                    b = fh_rx[rb_start * 12 : (rb_start + rb_size) * 12, start_sym, 0]
                    # also score data symbol for stronger identity when present
                    a11 = h5_rx[rb_start * 12 : (rb_start + rb_size) * 12,
                                min(start_sym + 1, FH_N_SYM - 1), 0]
                    b11 = fh_rx[rb_start * 12 : (rb_start + rb_size) * 12,
                                min(start_sym + 1, FH_N_SYM - 1), 0]
                    sc_d = corr_abs(a, b) if np.linalg.norm(b) > 0 else 0.0
                    sc_s = corr_abs(a11, b11) if np.linalg.norm(b11) > 0 else 0.0
                    scores.append((sc_s, sc_d, int(i), tip))
                    print(
                        f"  [{i:2d}] SFN.Slot {int(tip.SFN)}.{int(tip.Slot)} "
                        f"rnti={int(tip.rnti)} dmrs={sc_d:.4f} data={sc_s:.4f}"
                    )
                if not scores:
                    print("no scorable fh rows", file=sys.stderr)
                    return 1
                scores.sort(key=lambda t: (t[0], t[1]), reverse=True)
                args.fapi_index = scores[0][2]
                print(f"  -> best row index {args.fapi_index} "
                      f"(data={scores[0][0]:.4f} dmrs={scores[0][1]:.4f})")
        else:
            print(
                f"no fapi row with rnti={rnti}. "
                "Re-run with --list-fapi, or --allow-rnti-mismatch for a shape-only compare.",
                file=sys.stderr,
            )
            return 1

        idx = min(max(args.fapi_index, 0), len(fapi) - 1)
        tip = fapi.iloc[idx]
        full = client.query_df(
            f"""select * from fapi where
                rnti = {int(tip.rnti)} and SFN = {int(tip.SFN)} and Slot = {int(tip.Slot)}
                and TsTaiNs = toDateTime64('{tip.TsTaiNs.timestamp()}', 9)
                limit 1"""
        )
        if full.empty:
            full = client.query_df(
                f"select * from fapi where rnti = {int(tip.rnti)} "
                f"and SFN = {int(tip.SFN)} and Slot = {int(tip.Slot)} "
                f"order by TsTaiNs limit 1 offset {idx}"
            )
        if full.empty:
            print("failed to re-fetch full fapi row", file=sys.stderr)
            return 1
        rec = full.iloc[0]
        print(
            f"\nmatched [{match_kind}] fapi[{idx}/{len(fapi)}] "
            f"SFN.Slot {int(rec.SFN)}.{int(rec.Slot)} rnti={int(rec.rnti)} "
            f"TsTaiNs={rec.TsTaiNs} tbCrcFail={rec.tbCrcFail}"
        )
        compare_fapi(h, rec)

        fh = client.query_df(
            f"""select * from fh where
                TsTaiNs == toDateTime64('{rec.TsTaiNs.timestamp()}', 9) and
                SFN == {int(rec.SFN)} and Slot == {int(rec.Slot)}"""
        )
        if fh.empty:
            fh = client.query_df(
                f"""select * from fh where
                    TsTaiNs == toDateTime64('{rec.TsTaiNs.timestamp()}', 9)"""
            )
        if fh.empty:
            print("no matching fh row", file=sys.stderr)
            return 1
        fh_row = fh.iloc[0]
        n_rx = int(fh_row.nRxAnt)
        print(f"fh nRxAnt={n_rx} fhData len={len(fh_row.fhData)}")

        layouts = []
        if args.fh_layout in ("auto", "tight"):
            try:
                layouts.append(("tight", unpack_fh_tight(fh_row.fhData, n_rx, args.active_prb)))
            except ValueError as e:
                print(f"  tight unpack failed: {e}")
        if args.fh_layout in ("auto", "notebook"):
            try:
                layouts.append(("notebook", unpack_fh_notebook(fh_row.fhData)))
            except ValueError as e:
                print(f"  notebook unpack failed: {e}")
        if not layouts:
            print("no FH layout available", file=sys.stderr)
            return 1

        best = None
        print("\n=== FH vs H5 layout pick ===")
        for name, fh_rx in layouts:
            a = h5_rx[rb_start * 12 : (rb_start + rb_size) * 12, start_sym, 0]
            b = fh_rx[rb_start * 12 : (rb_start + rb_size) * 12, start_sym, 0]
            sc = corr_abs(a, b) if np.linalg.norm(b) > 0 else 0.0
            print(f"  fh={name:10s} grant-corr@sym{start_sym} ant0 = {sc:.4f}")
            if best is None or sc > best[0]:
                best = (sc, name, fh_rx)
        assert best is not None
        print(f"  -> using fh layout {best[1]}")
        compare_iq(
            h5_rx,
            best[2],
            n_ant=n_rx,
            start_sym=start_sym,
            n_sym=n_sym,
            rb_start=rb_start,
            rb_size=rb_size,
            active_prb=args.active_prb,
        )
        print(f"\nH5 layout used: {h5_label}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
