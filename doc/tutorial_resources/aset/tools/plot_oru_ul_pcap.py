#!/usr/bin/env python3
"""
Parse O-RU UL U-plane pcap (oru_ul_pcap_write format) and plot IQ magnitude.

Packet layout written by oaioran_ru.c:
  [0:14)   Ethernet (dst/src MAC + ethertype 0xAEFE; VLAN stripped in dump)
  [14:22)  eCPRI header (8 B)
  [22:26)  O-RAN radio application common header (4 B)
  [26:30)  O-RAN data section header (4 B)
  [30:  )  IQ samples: int16 I, int16 Q per subcarrier (network byte order)

Captures with ORU_UL_PCAP_CHAN=all also include eCPRI msg-type 2 C-plane frames
(typically 56 B). Those are skipped here; only U-plane IQ (msg type 0) is plotted.

Requires: numpy, matplotlib
"""

from __future__ import annotations

import argparse
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

# Offsets after the synthetic Ethernet header in the O-RU pcap dump.
ETH_LEN = 14
ECPRI_LEN = 8
RADIO_APP_LEN = 4
SECTION_HDR_LEN = 4
HEADER_LEN = ETH_LEN + ECPRI_LEN + RADIO_APP_LEN + SECTION_HDR_LEN

ECPRI_MSG_IQ = 0
ECPRI_MSG_RTC = 2  # C-plane

PCAP_MAGIC_LE = 0xA1B2C3D4
PCAP_MAGIC_BE = 0xD4C3B2A1

# Typical ASET sizes (uncompressed IQ after HEADER_LEN).
PUSCH_NUM_PRB = 106
PRACH_NUM_PRB = 12


@dataclass(frozen=True)
class UplanePacket:
    index: int
    frame_id: int
    subframe_id: int
    slot_id: int
    symbol_id: int
    eaxc_ru_port: int
    num_prb: int
    magnitude: np.ndarray  # shape (num_sc,)


def _pcap_endian(magic: int) -> str:
    if magic == PCAP_MAGIC_LE:
        return "<"
    if magic == PCAP_MAGIC_BE:
        return ">"
    raise ValueError(f"unsupported pcap magic 0x{magic:08x}")


def _parse_radio_app(pkt: bytes, offset: int) -> tuple[int, int, int, int]:
    # O-RAN radio_app_common_hdr (xran_pkt.h): data_feature, frame_id, sf_slot_sym (BE).
    frame_id = pkt[offset + 1]
    sf_slot_sym = struct.unpack_from(">H", pkt, offset + 2)[0]
    symbol_id = sf_slot_sym & 0x3F
    slot_id = (sf_slot_sym >> 6) & 0x3F
    subframe_id = (sf_slot_sym >> 12) & 0xF
    return frame_id, subframe_id, slot_id, symbol_id


def _parse_section_hdr(pkt: bytes, offset: int) -> tuple[int, int]:
    word = struct.unpack_from(">I", pkt, offset)[0]
    num_prb = word & 0xFF
    section_id = (word >> 20) & 0xFFF
    return num_prb, section_id


def _infer_num_sc(pkt_len: int) -> int | None:
    """Return subcarrier count from IQ payload length, or None if not IQ-sized."""
    payload_bytes = pkt_len - HEADER_LEN
    if payload_bytes <= 0 or payload_bytes % 4 != 0:
        return None
    return payload_bytes // 4


def _iq_magnitude(pkt: bytes, iq_offset: int, num_sc: int) -> np.ndarray:
    raw = pkt[iq_offset : iq_offset + num_sc * 4]
    if len(raw) < num_sc * 4:
        raise ValueError(f"short IQ payload: got {len(raw)} bytes, need {num_sc * 4}")
    # O-RAN U-plane IQ is big-endian int16 on the wire (htons in O-RU TX path).
    iq = np.frombuffer(raw, dtype=">i2").reshape(num_sc, 2)
    return np.hypot(iq[:, 0].astype(np.float64), iq[:, 1].astype(np.float64))


def read_uplane_packets(
    path: Path,
    max_packets: int | None = None,
    num_prb: int = 106,
    channel: str = "all",
) -> list[UplanePacket]:
    packets: list[UplanePacket] = []
    skipped_cplane = 0
    skipped_channel = 0
    skipped_odd = 0

    with path.open("rb") as f:
        gh = f.read(24)
        if len(gh) < 24:
            raise ValueError("truncated pcap global header")
        magic = struct.unpack_from("I", gh, 0)[0]
        endian = _pcap_endian(magic)

        idx = 0
        while True:
            ph = f.read(16)
            if len(ph) < 16:
                break
            _ts_sec, _ts_usec, incl_len, _orig_len = struct.unpack(endian + "IIII", ph)
            pkt = f.read(incl_len)
            file_idx = idx
            idx += 1

            if max_packets is not None and len(packets) >= max_packets:
                break
            if len(pkt) < HEADER_LEN:
                continue

            ecpri_msg = pkt[ETH_LEN + 1]
            if ecpri_msg != ECPRI_MSG_IQ:
                # C-plane (msg 2) and anything else: no IQ payload for this plotter.
                skipped_cplane += 1
                continue

            num_sc = _infer_num_sc(len(pkt))
            if num_sc is None:
                skipped_odd += 1
                continue

            hdr_num_prb, _section_id = _parse_section_hdr(pkt, ETH_LEN + ECPRI_LEN + RADIO_APP_LEN)
            # Prefer section header when set; else infer from payload (PRACH may not be 12*N).
            if hdr_num_prb > 0 and hdr_num_prb * 12 == num_sc:
                pkt_num_prb = hdr_num_prb
            elif num_sc % 12 == 0:
                pkt_num_prb = num_sc // 12
            else:
                pkt_num_prb = hdr_num_prb if hdr_num_prb > 0 else num_prb

            if channel == "pusch" and pkt_num_prb != PUSCH_NUM_PRB and hdr_num_prb != PUSCH_NUM_PRB:
                skipped_channel += 1
                continue
            if channel == "prach" and pkt_num_prb != PRACH_NUM_PRB and hdr_num_prb != PRACH_NUM_PRB:
                skipped_channel += 1
                continue

            ecpri_pcid = pkt[ETH_LEN + 5]
            eaxc_ru_port = ecpri_pcid & 0x0F
            frame_id, subframe_id, slot_id, symbol_id = _parse_radio_app(pkt, ETH_LEN + ECPRI_LEN)

            mag = _iq_magnitude(pkt, HEADER_LEN, num_sc)
            packets.append(
                UplanePacket(
                    index=file_idx,
                    frame_id=frame_id,
                    subframe_id=subframe_id,
                    slot_id=slot_id,
                    symbol_id=symbol_id,
                    eaxc_ru_port=eaxc_ru_port,
                    num_prb=pkt_num_prb,
                    magnitude=mag,
                )
            )

    if skipped_cplane or skipped_channel or skipped_odd:
        print(
            f"skipped: cplane={skipped_cplane} channel_filter={skipped_channel} odd_iq={skipped_odd}",
            file=sys.stderr,
        )
    return packets


def _label(pkt: UplanePacket) -> str:
    abs_slot = pkt.subframe_id * 2 + pkt.slot_id  # mu=1
    return (
        f"pkt {pkt.index}: f={pkt.frame_id} sf={pkt.subframe_id} slot={pkt.slot_id}"
        f" (abs {abs_slot}) sym={pkt.symbol_id} eAxC ruPort={pkt.eaxc_ru_port}"
    )


def plot_magnitude_line(packets: list[UplanePacket], indices: list[int], out: Path | None) -> None:
    fig, ax = plt.subplots(figsize=(14, 5))
    for i in indices:
        if i < 0 or i >= len(packets):
            print(f"warning: packet index {i} out of range (0..{len(packets)-1})", file=sys.stderr)
            continue
        pkt = packets[i]
        ax.plot(pkt.magnitude, linewidth=0.8, label=_label(pkt))
    ax.set_xlabel("Subcarrier index (0 = start PRB, DC-split order on wire)")
    ax.set_ylabel("|IQ| magnitude")
    ax.set_title("O-RU UL U-plane IQ magnitude per subcarrier")
    ax.grid(True, alpha=0.3)
    ax.legend(fontsize=8, loc="upper right")
    fig.tight_layout()
    if out:
        fig.savefig(out, dpi=150)
        print(f"wrote {out}")
    else:
        plt.show()


def _select_slot_group(
    packets: list[UplanePacket], ref_index: int
) -> tuple[tuple[int, int, int, int, int], dict[int, UplanePacket]]:
    """Collect one symbol->packet map for the (frame, subframe, slot, eAxC) of ref_index."""
    ref = packets[ref_index]
    key = (ref.frame_id, ref.subframe_id, ref.slot_id, ref.eaxc_ru_port)
    by_symbol: dict[int, UplanePacket] = {}
    for pkt in packets:
        if (pkt.frame_id, pkt.subframe_id, pkt.slot_id, pkt.eaxc_ru_port) == key:
            by_symbol.setdefault(pkt.symbol_id, pkt)
    return key, by_symbol


def plot_magnitude_heatmap(packets: list[UplanePacket], ref_index: int, out: Path | None) -> None:
    key, by_symbol = _select_slot_group(packets, ref_index)
    frame_id, subframe_id, slot_id, eaxc = key
    abs_slot = subframe_id * 2 + slot_id  # mu=1

    symbols = sorted(by_symbol)
    num_sc = by_symbol[symbols[0]].magnitude.size
    # y-axis = symbol (one row per OFDM symbol), x-axis = RE / subcarrier index.
    grid = np.full((len(symbols), num_sc), np.nan)
    for row, sym in enumerate(symbols):
        grid[row, :] = by_symbol[sym].magnitude

    fig, ax = plt.subplots(figsize=(14, 5))
    im = ax.imshow(grid, aspect="auto", origin="lower", interpolation="nearest", cmap="viridis")
    ax.set_xlabel("RE / subcarrier index (0 = start PRB, ascending on wire)")
    ax.set_ylabel("OFDM symbol")
    ax.set_yticks(range(len(symbols)))
    ax.set_yticklabels(symbols)
    ax.set_title(
        f"IQ magnitude — frame {frame_id} sf {subframe_id} slot {slot_id}"
        f" (abs {abs_slot}) eAxC ruPort {eaxc}"
    )
    fig.colorbar(im, ax=ax, label="|IQ|")
    fig.tight_layout()
    if out:
        fig.savefig(out, dpi=150)
        print(f"wrote {out}  (symbols {symbols})")
    else:
        plt.show()


def plot_all_packets_overview(packets: list[UplanePacket], out: Path | None) -> None:
    if not packets:
        return
    peaks = np.array([p.magnitude.max() for p in packets])
    means = np.array([p.magnitude.mean() for p in packets])
    fig, axes = plt.subplots(2, 1, figsize=(14, 6), sharex=True)
    axes[0].plot(peaks, linewidth=0.8)
    axes[0].set_ylabel("peak |IQ|")
    axes[0].set_title("Per-packet IQ magnitude stats")
    axes[0].grid(True, alpha=0.3)
    axes[1].plot(means, linewidth=0.8, color="tab:orange")
    axes[1].set_ylabel("mean |IQ|")
    axes[1].set_xlabel("Packet index in pcap")
    axes[1].grid(True, alpha=0.3)
    fig.tight_layout()
    if out:
        fig.savefig(out, dpi=150)
        print(f"wrote {out}")
    else:
        plt.show()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pcap", type=Path, help="path to oru_ul.pcap")
    parser.add_argument(
        "--packets",
        type=int,
        nargs="+",
        default=[0],
        help="packet indices to plot as line charts (default: 0)",
    )
    parser.add_argument("--max-packets", type=int, default=None, help="stop after N U-plane packets")
    parser.add_argument("--num-prb", type=int, default=106, help="PRBs if section header uses 0")
    parser.add_argument(
        "--channel",
        choices=("all", "pusch", "prach"),
        default="pusch",
        help="U-plane filter: pusch=106 PRB, prach=12 PRB, all=both (default: pusch)",
    )
    parser.add_argument(
        "--mode",
        choices=("line", "heatmap", "overview", "all"),
        default="all",
        help="plot style (default: all)",
    )
    parser.add_argument("--out-dir", type=Path, default=None, help="save PNGs here instead of showing")
    args = parser.parse_args()

    if not args.pcap.is_file():
        print(f"error: {args.pcap} not found", file=sys.stderr)
        return 1

    packets = read_uplane_packets(
        args.pcap,
        max_packets=args.max_packets,
        num_prb=args.num_prb,
        channel=args.channel,
    )
    if not packets:
        print("no packets parsed", file=sys.stderr)
        return 1

    print(f"parsed {len(packets)} U-plane packets ({args.channel}) from {args.pcap}")
    for i in range(min(5, len(packets))):
        p = packets[i]
        print(
            f"  [{i}] file_idx={p.index} f={p.frame_id} sf={p.subframe_id} slot={p.slot_id}"
            f" sym={p.symbol_id} eAxC={p.eaxc_ru_port} nPRB={p.num_prb}"
            f" peak={p.magnitude.max():.1f} mean={p.magnitude.mean():.1f}"
        )

    out_dir = args.out_dir
    if out_dir:
        out_dir.mkdir(parents=True, exist_ok=True)

    if args.mode in ("line", "all"):
        out = (out_dir / "oru_ul_magnitude_line.png") if out_dir else None
        plot_magnitude_line(packets, args.packets, out)
    if args.mode in ("heatmap", "all"):
        idx = args.packets[0] if args.packets else 0
        idx = min(max(idx, 0), len(packets) - 1)
        out = (out_dir / "oru_ul_magnitude_heatmap.png") if out_dir else None
        plot_magnitude_heatmap(packets, idx, out)
    if args.mode in ("overview", "all"):
        out = (out_dir / "oru_ul_magnitude_overview.png") if out_dir else None
        plot_all_packets_overview(packets, out)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
