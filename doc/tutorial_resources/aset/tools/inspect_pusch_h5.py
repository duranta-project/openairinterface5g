#!/usr/bin/env python3
"""
Inspect an Aerial PUSCH_H5DUMP (PUSCH_Debug*.h5) produced on a CRC failure.

Prints the FAPI/PUSCH parameters, the quality metrics cuPHY itself computed
(RSSI/RSRP/SINR pre+post eq, noise var, CFO, timing), and the per-symbol energy
of the received IQ (DataRx0) inside the grant so you can tell at a glance whether
the Msg3 samples actually landed where Aerial expected them.

The 96-byte / 800-byte PUSCH_Debug*.h5 are startup stubs; a real dump is ~350 KB.
Pick the largest file, not the newest.

Usage:
  python3 inspect_pusch_h5.py [PUSCH_Debug....h5] [--h5-dir DIR]
  python3 inspect_pusch_h5.py --dmrs --chest   # DMRS corr + cuPHY HEst/LLR

Requires: h5py, numpy
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


PARAM_KEYS = [
    "rnti", "scid", "enableTfPrcd", "dmrsScrmId", "dmrsSymLocBmsk",
    "nDmrsCdmGrpsNoData", "dmrsPortBmsk", "dmrsAddlnPos", "dmrsMaxLen",
    "startPrb", "nPrb", "startSym", "nSym", "nUeLayers",
    "qamModOrder", "targetCodeRate", "rv", "ndi", "harqProcessId",
    "tbSizeBytes", "dataScramId", "puschIdentity",
    "ldpc_bg", "ldpc_Zc", "ldpc_K", "ldpc_num_CBs",
    "slotNumPerCell", "slotNumPerUeGrp", "tbCrcs", "cbCrcs",
]

QUALITY_KEYS = [
    "Rssi", "Rsrp", "SinrPreEq", "SinrPostEq",
    "NoiseVarPreEqPerUe", "NoiseVarPostEq",
    "CfoEstHzPerUe", "ToEstMicroSecPerUe",
]


def scalar(h: h5py.File, key: str):
    if key not in h:
        return None
    v = np.asarray(h[key]).reshape(-1)
    return v[0] if v.size else None


def struct_to_complex(ds) -> np.ndarray:
    arr = np.asarray(ds)
    if arr.dtype.names and "re" in arr.dtype.names and "im" in arr.dtype.names:
        return arr["re"].astype(np.float32) + 1j * arr["im"].astype(np.float32)
    return arr.astype(np.complex64)


def gold_seq(c_init: int, length: int) -> np.ndarray:
    """38.211 5.2.1 length-31 Gold sequence, Nc=1600."""
    Nc = 1600
    n = length + Nc + 31
    x1 = np.zeros(n, dtype=np.int8)
    x2 = np.zeros(n, dtype=np.int8)
    x1[0] = 1
    for i in range(31):
        x2[i] = (c_init >> i) & 1
    for i in range(n - 31):
        x1[i + 31] = (x1[i + 3] ^ x1[i]) & 1
        x2[i + 31] = (x2[i + 3] ^ x2[i + 2] ^ x2[i + 1] ^ x2[i]) & 1
    return (x1[Nc:Nc + length] ^ x2[Nc:Nc + length]).astype(np.int8)


def pusch_dmrs_seq(n_slot: int, l: int, n_id: int, n_scid: int,
                   n_symb_slot: int, n_re_dmrs: int) -> np.ndarray:
    """38.211 6.4.1.1: PUSCH DMRS QPSK sequence for `n_re_dmrs` DMRS REs."""
    c_init = ((2 ** 17) * (n_symb_slot * n_slot + l + 1) * (2 * n_id + 1)
              + 2 * n_id + n_scid) % (2 ** 31)
    c = gold_seq(c_init, 2 * n_re_dmrs)
    r = (1 / np.sqrt(2)) * (1 - 2 * c[0::2]) + 1j * (1 / np.sqrt(2)) * (1 - 2 * c[1::2])
    return r.astype(np.complex64)


def dmrs_check(h: h5py.File, iq: np.ndarray) -> None:
    """Correlate the DMRS symbol in DataRx0 against the local 38.211 sequence."""
    sp = int(scalar(h, "startPrb") or 0)
    npr = int(scalar(h, "nPrb") or 0)
    n_id = int(scalar(h, "dmrsScrmId") or 0)
    n_scid = int(scalar(h, "scid") or 0)
    n_slot = int(scalar(h, "slotNumPerCell") or 0)
    n_symb = int(scalar(h, "N_symb_slot") or 14) if "N_symb_slot" in h else 14
    bmsk = int(scalar(h, "dmrsSymLocBmsk") or 0)
    dmrs_syms = [b for b in range(14) if (bmsk >> b) & 1]
    print(f"\n=== DMRS check (N_ID={n_id} n_SCID={n_scid} slot={n_slot} "
          f"sym={dmrs_syms} PRB[{sp}:{sp+npr}]) ===")
    if not dmrs_syms:
        print("  no DMRS symbol in bitmask")
        return

    n_ant = iq.shape[0]
    # DMRS config type 1, port 0 -> even subcarriers (comb-2, delta=0)
    re0 = sp * 12
    combs = (("even", 0), ("odd", 1))
    rows = []
    for l in dmrs_syms:
        # full DMRS sequence indexed by DMRS RE across whole band, then slice grant
        n_re_dmrs_band = 273 * 6  # even REs across max band
        seq_band = pusch_dmrs_seq(n_slot, l, n_id, n_scid, n_symb, n_re_dmrs_band)
        for cname, delta in combs:
            for ant in range(n_ant):
                # received DMRS REs on this comb within the grant
                rx = iq[ant, l, re0 + delta: re0 + npr * 12: 2]
                # matching local sequence entries (DMRS RE index = prb*6 + within)
                m0 = sp * 6
                loc = seq_band[m0: m0 + rx.size]
                if rx.size == 0 or loc.size < rx.size:
                    continue
                loc = loc[: rx.size]
                # channel per RE = rx / dmrs (dmrs is unit magnitude)
                h_est = rx * np.conj(loc)
                num = np.abs(h_est.sum())
                den = np.sqrt(np.sum(np.abs(rx) ** 2)) * np.sqrt(len(rx))
                corr = num / den if den > 0 else 0.0
                # residual after removing flat channel estimate
                h_mean = h_est.mean()
                resid = rx - h_mean * loc
                snr = (np.abs(h_mean) ** 2 * len(rx)) / (np.sum(np.abs(resid) ** 2) + 1e-12)
                snr_db = 10 * np.log10(snr) if snr > 0 else float("-inf")
                rows.append((l, cname, ant, corr, abs(h_mean), snr_db, float(np.abs(rx).mean())))

    if not rows:
        print("  (no DMRS REs to report)")
        return

    hdr = f"{'sym':>3}  {'comb':<5}  {'ant':>3}  {'|corr|':>7}  {'|H|':>8}  {'SNR dB':>7}  {'|rx|':>8}"
    print(f"  {hdr}")
    print(f"  {'-' * len(hdr)}")
    for l, cname, ant, corr, hmag, snr_db, rxm in rows:
        print(f"  {l:3d}  {cname:<5}  {ant:3d}  {corr:7.4f}  {hmag:8.4f}  {snr_db:7.1f}  {rxm:8.4f}")


def chest_check(h: h5py.File) -> None:
    """Inspect cuPHY's own channel estimate / LLRs / TB vs the strong on-wire DMRS."""
    print("\n=== cuPHY internals (HEst / Eq / LLR / TB) ===")

    if "HEst0" in h:
        he = struct_to_complex(h["HEst0"])
        # typical shape (nUe, nRE, nLayer, nAnt) e.g. (1, 96, 1, 2)
        print(f"  HEst0 shape={he.shape} |mean|={np.abs(he).mean():.4g} "
              f"|max|={np.abs(he).max():.4g}")
        if he.ndim >= 4:
            for a in range(he.shape[-1]):
                print(f"    ant{a} |mean|={np.abs(he[..., a]).mean():.4g} "
                      f"|max|={np.abs(he[..., a]).max():.4g}")
        # flatness: std/mean of |H| — collapsed estimate looks near-zero + noisy
        mag = np.abs(he).reshape(-1)
        if mag.mean() > 0:
            print(f"    |H| std/mean={mag.std() / mag.mean():.3f} "
                  f"(frac |H|<1e-3 = {np.mean(mag < 1e-3):.3f})")

    if "EqCoeff0" in h:
        eq = struct_to_complex(h["EqCoeff0"])
        print(f"  EqCoeff0 shape={eq.shape} |mean|={np.abs(eq).mean():.4g} "
              f"|max|={np.abs(eq).max():.4g}")

    if "Ree0" in h:
        ree = np.asarray(h["Ree0"]).astype(np.float32)
        print(f"  Ree0 shape={ree.shape} mean={ree.mean():.4g} "
              f"min={ree.min():.4g} max={ree.max():.4g}")

    for k in ("LLR0", "deRmLLR0"):
        if k not in h:
            continue
        a = np.asarray(h[k]).astype(np.float32)
        print(f"  {k:8s} shape={a.shape} |mean|={np.abs(a).mean():.4g} "
              f"|max|={np.abs(a).max():.4g} "
              f"frac|L|<0.5={np.mean(np.abs(a) < 0.5):.3f}")

    tb_crc = scalar(h, "tbCrcs")
    cb_crc = scalar(h, "cbCrcs")
    print(f"  tbCrcs={tb_crc}  cbCrcs={cb_crc}  "
          f"({'FAIL' if tb_crc not in (0, None) else 'PASS'})")

    if "TbPayload" in h:
        tb = np.asarray(h["TbPayload"]).astype(np.uint8).reshape(-1)
        nbytes = int(scalar(h, "tbSizeBytes") or tb.size)
        print(f"  TbPayload[{nbytes}B] = {tb[:nbytes].tolist()}")


def find_data_rx(h: h5py.File) -> str:
    for k in ("DataRx0", "dataRx0", "DataRx", "dataRx"):
        if k in h:
            return k
    for k in h.keys():
        if "datarx" in k.lower():
            return k
    raise KeyError(f"no DataRx* in {list(h.keys())[:20]}")


def pick_h5(explicit: Path | None, h5_dir: Path) -> Path:
    if explicit is not None:
        return explicit
    cands = sorted(h5_dir.glob("PUSCH_Debug*.h5"),
                   key=lambda p: p.stat().st_size, reverse=True)
    real = [p for p in cands if p.stat().st_size > 4096]
    if not real:
        raise FileNotFoundError(
            f"no real PUSCH_Debug*.h5 (>4KB) under {h5_dir}; "
            f"largest found: {[(p.name, p.stat().st_size) for p in cands[:3]]}"
        )
    return real[0]


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("h5", type=Path, nargs="?", default=None,
                    help="PUSCH_Debug*.h5 (default: largest under --h5-dir)")
    ap.add_argument("--h5-dir", type=Path,
                    default=Path.home() / "aerial-cuda-accelerated-ran")
    ap.add_argument("--full-band", action="store_true",
                    help="also print full-band per-symbol energy")
    ap.add_argument("--dmrs", action="store_true",
                    help="correlate DMRS symbol vs local 38.211 sequence (both combs)")
    ap.add_argument("--chest", action="store_true",
                    help="print cuPHY HEst0 / EqCoeff / LLR / TbPayload")
    args = ap.parse_args()

    try:
        path = pick_h5(args.h5, args.h5_dir)
    except FileNotFoundError as e:
        print(e, file=sys.stderr)
        return 1
    print(f"H5: {path} ({path.stat().st_size} bytes)\n")

    try:
        h = h5py.File(path, "r")
    except OSError as e:
        print(f"cannot open {path}: {e}\n"
              "hint: this is likely a truncated startup stub; pick the largest file",
              file=sys.stderr)
        return 1

    with h:
        print("=== PUSCH params ===")
        for k in PARAM_KEYS:
            v = scalar(h, k)
            if v is not None:
                print(f"  {k:22s} = {v}")

        tf = scalar(h, "enableTfPrcd")
        if tf is not None:
            print(f"\n  transform precoding: {'ENABLED (DFT-s-OFDM)' if int(tf) else 'disabled (CP-OFDM)'}")

        print("\n=== quality metrics (cuPHY computed) ===")
        for k in QUALITY_KEYS:
            if k in h:
                v = np.asarray(h[k]).reshape(-1)
                print(f"  {k:22s} = {np.round(v[:4], 4)}")
        if "RssiFull" in h:
            rf = np.asarray(h["RssiFull"])  # (1, nAnt, nSym)
            print(f"  RssiFull shape={rf.shape} "
                  f"min={rf.min():.3f} max={rf.max():.3f} mean={rf.mean():.3f}")

        dr_key = find_data_rx(h)
        iq = struct_to_complex(h[dr_key])  # (nAnt, nSym, nRE)
        n_ant, n_sym, n_re = iq.shape
        print(f"\n=== {dr_key} {iq.shape} "
              f"peak={np.abs(iq).max():.5g} mean={np.abs(iq).mean():.5g} ===")

        sp = int(scalar(h, "startPrb") or 0)
        npr = int(scalar(h, "nPrb") or 0)
        ss = int(scalar(h, "startSym") or 0)
        ns = int(scalar(h, "nSym") or 0)
        re0, re1 = sp * 12, min((sp + npr) * 12, n_re)
        print(f"grant: PRB[{sp}:{sp+npr}] sym[{ss}:{ss+ns}] RE[{re0}:{re1}]\n")

        for ant in range(n_ant):
            for sym in range(n_sym):
                seg = iq[ant, sym, re0:re1]
                if seg.size == 0:
                    continue
                in_grant = ss <= sym < ss + ns
                tag = " <-grant" if in_grant else ""
                print(f"  ant{ant} sym{sym:2d}: |mean|={np.abs(seg).mean():.4g} "
                      f"|peak|={np.abs(seg).max():.4g}{tag}")
            print()

        # off-grant noise floor vs in-grant energy (crude SNR proxy on the wire)
        grant_syms = [s for s in range(n_sym) if ss <= s < ss + ns]
        off_syms = [s for s in range(n_sym) if s not in grant_syms]
        if grant_syms and off_syms:
            g = np.abs(iq[:, grant_syms, re0:re1]).mean()
            o = np.abs(iq[:, off_syms, re0:re1]).mean()
            ratio_db = 20 * np.log10(g / o) if o > 0 else float("inf")
            print(f"grant |mean|={g:.4g}  off-grant |mean|={o:.4g}  "
                  f"ratio={ratio_db:.1f} dB (on-wire, pre-eq)")

        if args.dmrs:
            dmrs_check(h, iq)

        if args.chest:
            chest_check(h)

        if args.full_band:
            print("\nfull-band |mean| per sym ant0:",
                  np.round(np.abs(iq[0]).mean(axis=1), 4))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
