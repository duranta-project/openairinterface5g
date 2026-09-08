#!/usr/bin/env python3
"""Publish CIR taps to vrtsim over nanomsg (NN_PUB), with live control.

Shared publisher for static and Sionna CIR sources. Speaks FlatBuffers
``Phy.Taps`` (taps.fbs) and the line-oriented control protocol
(``gain <f>`` / ``status`` / ``quit``) used by ``set-gain``.

Sources (``--source`` / ``TAPS_SOURCE`` / ``SIONNA_SOURCE``):
  auto    try Sionna (see taps/sionna/sionna_cir.py); fall back to static
  sionna  require Sionna TDL; error out if unavailable
  rt      require Sionna RT; error out if unavailable
  static  deterministic single-tap CIR; topology from --static-mode
          (``awgn`` identity, ``dft`` unitary, ``diag`` uncoupled, ``tx0`` legacy)

Dual endpoint (DL + UL, ASET defaults):
  ./publish_taps.py --dual --source static

vrtsim binds nothing: the publisher nn_bind()s the PUB socket and vrtsim
(nr-oru / nr-ue, via --vrtsim.taps-socket) connects as the SUB.
"""

from __future__ import annotations

import argparse
import ctypes
import ctypes.util
import functools
import os
import socket
import sys
import threading
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

import numpy as np

# FlatBuffers bindings generated from taps.fbs at image build time (flatc --python).
import flatbuffers
from Phy.Taps import (
    TapsStart,
    TapsAddId,
    TapsAddNumTxAntennas,
    TapsAddNumRxAntennas,
    TapsAddTapsLen,
    TapsAddTaps,
    TapsEnd,
)

# --------------------------------------------------------------------------- #
# Minimal ctypes binding to libnanomsg (same library vrtsim links against).
# --------------------------------------------------------------------------- #
AF_SP = 1
NN_PUB = 32  # NN_PROTO_PUBSUB (2) * 16 + 0

_libname = ctypes.util.find_library("nanomsg") or "libnanomsg.so.5"
_nn = ctypes.CDLL(_libname)
_nn.nn_socket.restype = ctypes.c_int
_nn.nn_socket.argtypes = [ctypes.c_int, ctypes.c_int]
_nn.nn_bind.restype = ctypes.c_int
_nn.nn_bind.argtypes = [ctypes.c_int, ctypes.c_char_p]
_nn.nn_send.restype = ctypes.c_int
_nn.nn_send.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_int]
_nn.nn_close.restype = ctypes.c_int
_nn.nn_close.argtypes = [ctypes.c_int]
_nn.nn_errno.restype = ctypes.c_int
_nn.nn_errno.argtypes = []
_nn.nn_strerror.restype = ctypes.c_char_p
_nn.nn_strerror.argtypes = [ctypes.c_int]


def _nn_err() -> str:
    return _nn.nn_strerror(_nn.nn_errno()).decode()


def _ensure_sionna_path() -> None:
    """Make sionna_cir importable from common/ (host) or beside this file (Docker)."""
    here = Path(__file__).resolve().parent
    for cand in (here, here.parent / "sionna"):
        s = str(cand)
        if cand.is_dir() and s not in sys.path:
            sys.path.insert(0, s)


# --------------------------------------------------------------------------- #
# Endpoint state + config
# --------------------------------------------------------------------------- #
@dataclass
class EndpointConfig:
    label: str
    url: str
    ctl_path: str
    num_tx: int
    num_rx: int
    L: int = 1
    id: int = 0
    gain: float = 1.0
    period_ms: int = 100
    source: str = "auto"
    static_mode: str = "awgn"


@dataclass
class State:
    gain: float = 1.0
    num_tx: int = 0
    num_rx: int = 0
    L: int = 1
    id: int = 0
    period_ms: int = 100
    running: bool = True
    lock: threading.Lock = field(default_factory=threading.Lock)


def build_taps_msg(msg_id: int, num_tx: int, num_rx: int, L: int, cir: np.ndarray) -> bytes:
    """Serialize a Phy.Taps FlatBuffer.

    Layout matches taps.fbs: taps[aarx + num_rx*aatx][0..L) as interleaved
    (re, im) float32. ``cir`` is complex, shape (num_tx, num_rx, L).
    """
    flat = np.zeros((num_tx * num_rx * L, 2), dtype=np.float32)
    for aatx in range(num_tx):
        for aarx in range(num_rx):
            link = aarx + num_rx * aatx
            base = link * L
            flat[base : base + L, 0] = np.real(cir[aatx, aarx]).astype(np.float32)
            flat[base : base + L, 1] = np.imag(cir[aatx, aarx]).astype(np.float32)
    interleaved = np.ascontiguousarray(flat.reshape(-1), dtype=np.float32)

    builder = flatbuffers.Builder(interleaved.nbytes + 64)
    vec = builder.CreateNumpyVector(interleaved)
    TapsStart(builder)
    TapsAddId(builder, msg_id)
    TapsAddNumTxAntennas(builder, num_tx)
    TapsAddNumRxAntennas(builder, num_rx)
    TapsAddTapsLen(builder, L)
    TapsAddTaps(builder, vec)
    builder.Finish(TapsEnd(builder))
    return bytes(builder.Output())


STATIC_MODES = ("awgn", "dft", "diag", "tx0")


def static_cir(num_tx: int, num_rx: int, L: int, gain: float, mode: str = "awgn") -> np.ndarray:
    """Deterministic single-tap CIR, shape (num_tx, num_rx, L).

    Every mode is scaled so that each RX antenna receives the same total power
    for a given gain (assuming equal power per TX port), so switching mode or
    TX port count does not change the SNR that ``noise_power_dBFS`` targets.

    awgn  identity coupling (H = gain * I, wrapped if rectangular). Flat,
          full-rank when square; AWGN noise still comes from
          channelmod.noise_power_dBFS on the vrtsim side.
    dft   truncated unitary DFT. Rank min(num_tx, num_rx) and well conditioned
          under any PMI, including the [1,-1]/sqrt(2) 2-port precoder that
          nulls an all-ones channel.
    diag  alias of awgn (kept for older scripts).
    tx0   legacy TX0-only fan-out: rank 1, so only one layer is usable.

    For a single TX port all modes degenerate to the same fan-out to every RX.
    """
    if mode not in STATIC_MODES:
        raise ValueError(f"unknown static CIR mode {mode!r}, expected one of {', '.join(STATIC_MODES)}")

    cir = np.zeros((num_tx, num_rx, L), dtype=np.complex64)
    if mode == "tx0":
        cir[0, :, 0] = gain
    elif mode in ("awgn", "diag"):
        for aarx in range(num_rx):
            cir[aarx % num_tx, aarx, 0] = gain
    else:
        n = max(num_tx, num_rx)
        aatx = np.arange(num_tx).reshape(-1, 1)
        aarx = np.arange(num_rx).reshape(1, -1)
        h = np.exp(-2j * np.pi * aatx * aarx / n) / np.sqrt(num_tx)
        cir[:, :, 0] = (gain * h).astype(np.complex64)
    return cir


def make_cir_source(cfg: EndpointConfig):
    """Return a callable cir(num_tx, num_rx, L, gain) -> complex ndarray."""
    static = functools.partial(static_cir, mode=cfg.static_mode)
    if cfg.source == "static":
        sys.stderr.write(f"{cfg.label}: using static CIR source (mode {cfg.static_mode})\n")
        return static

    _ensure_sionna_path()
    try:
        if cfg.source == "rt":
            from sionna_cir import SionnaRTCIRSource

            src = SionnaRTCIRSource(label=cfg.label, num_tx=cfg.num_tx, num_rx=cfg.num_rx, L=cfg.L)
            source_name = "Sionna RT"
        else:
            from sionna_cir import SionnaCIRSource

            src = SionnaCIRSource(label=cfg.label, num_tx=cfg.num_tx, num_rx=cfg.num_rx, L=cfg.L)
            source_name = "Sionna TDL"

        def _sionna(num_tx, num_rx, L, gain):
            return gain * src.cir(num_tx, num_rx, L)

        sys.stderr.write(f"{cfg.label}: using {source_name} CIR source\n")
        return _sionna
    except (ImportError, NotImplementedError, FileNotFoundError, ValueError) as e:
        if cfg.source in {"sionna", "rt"}:
            sys.stderr.write(f"{cfg.label}: --source {cfg.source} requested but unavailable: {e}\n")
            raise
        sys.stderr.write(
            f"{cfg.label}: Sionna unavailable ({e}); falling back to static CIR (mode {cfg.static_mode})\n"
        )
        return static


# --------------------------------------------------------------------------- #
# Live control (line protocol compatible with taps/common/set-gain)
# --------------------------------------------------------------------------- #
def _handle_client(fd: socket.socket, st: State, peer: Optional[State]) -> None:
    try:
        data = fd.recv(256)
    except OSError:
        fd.close()
        return
    if not data:
        fd.close()
        return
    line = data.decode(errors="replace").split("\n", 1)[0]

    if line.startswith("gain "):
        try:
            g = float(line[5:])
        except ValueError:
            reply = "err bad gain value\n"
        else:
            with st.lock:
                st.gain = g
            sys.stderr.write(f"control: gain -> {g:.6f}\n")
            reply = f"ok gain={g:.6f}\n"
    elif line == "status":
        with st.lock:
            reply = (
                f"ok gain={st.gain:.6f} tx={st.num_tx} rx={st.num_rx} "
                f"L={st.L} id={st.id} period_ms={st.period_ms}\n"
            )
    elif line == "quit":
        reply = "ok quitting\n"
        st.running = False
        if peer is not None:
            peer.running = False
    else:
        reply = "err unknown cmd (use: gain <f> | status | quit)\n"

    try:
        fd.sendall(reply.encode())
    except OSError:
        pass
    fd.close()


def control_thread(ctl_path: str, st: State, peer: Optional[State]) -> None:
    try:
        os.unlink(ctl_path)
    except FileNotFoundError:
        pass
    srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    try:
        srv.bind(ctl_path)
    except OSError as e:
        sys.stderr.write(f"control bind({ctl_path}): {e}\n")
        srv.close()
        return
    srv.listen(4)
    srv.settimeout(0.2)
    sys.stderr.write(
        f"Control socket: {ctl_path} (commands: gain <f> | status | quit)\n"
    )
    while st.running:
        try:
            cfd, _ = srv.accept()
        except socket.timeout:
            continue
        except OSError:
            break
        _handle_client(cfd, st, peer)
    srv.close()
    try:
        os.unlink(ctl_path)
    except FileNotFoundError:
        pass


# --------------------------------------------------------------------------- #
# Publisher
# --------------------------------------------------------------------------- #
def run_publisher(cfg: EndpointConfig, st: State, peer: Optional[State]) -> int:
    st.gain = cfg.gain
    st.num_tx = cfg.num_tx
    st.num_rx = cfg.num_rx
    st.L = cfg.L
    st.id = cfg.id
    st.period_ms = cfg.period_ms

    cir_source = make_cir_source(cfg)

    sock = _nn.nn_socket(AF_SP, NN_PUB)
    if sock < 0:
        sys.stderr.write(f"{cfg.label}: nn_socket: {_nn_err()}\n")
        return 1
    if _nn.nn_bind(sock, cfg.url.encode()) < 0:
        sys.stderr.write(f"{cfg.label}: nn_bind({cfg.url}): {_nn_err()}\n")
        _nn.nn_close(sock)
        return 1

    sys.stderr.write(
        f"{cfg.label}: publishing taps on {cfg.url} "
        f"(id={cfg.id} tx={cfg.num_tx} rx={cfg.num_rx} L={cfg.L} "
        f"gain={cfg.gain:.3f} source={cfg.source}) every {cfg.period_ms} ms\n"
    )

    ctl = threading.Thread(target=control_thread, args=(cfg.ctl_path, st, peer), daemon=True)
    ctl.start()

    while st.running:
        with st.lock:
            gain, msg_id, num_tx, num_rx, L, period_ms = (
                st.gain, st.id, st.num_tx, st.num_rx, st.L, st.period_ms,
            )
        cir = cir_source(num_tx, num_rx, L, gain)
        msg = build_taps_msg(msg_id, num_tx, num_rx, L, cir)
        buf = ctypes.create_string_buffer(msg, len(msg))
        n = _nn.nn_send(sock, buf, len(msg), 0)
        if n < 0:
            sys.stderr.write(f"{cfg.label}: nn_send: {_nn_err()}\n")
        time.sleep(period_ms / 1000.0)

    ctl.join(timeout=1.0)
    _nn.nn_close(sock)
    return 0


def _default_source() -> str:
    return os.environ.get("TAPS_SOURCE") or os.environ.get("SIONNA_SOURCE") or "auto"


def _default_static_mode() -> str:
    return os.environ.get("TAPS_STATIC_MODE") or "awgn"


def main(argv: Optional[list] = None) -> int:
    p = argparse.ArgumentParser(description="Publish static/Sionna CIR taps to vrtsim.")
    p.add_argument("--dual", action="store_true", help="publish DL + UL with ASET defaults")
    p.add_argument("--source", choices=["auto", "sionna", "rt", "static"],
                   default=_default_source())
    p.add_argument("--static-mode", choices=list(STATIC_MODES), default=_default_static_mode(),
                   help="static CIR topology (awgn = identity H; noise from noise_power_dBFS)")
    p.add_argument("-L", "--taps-len", type=int, default=1, help="taps per link")
    p.add_argument("-p", "--period-ms", type=int, default=100)
    p.add_argument("-i", "--id", type=int, default=0)

    # Single-endpoint options
    p.add_argument("-u", "--url", default=None, help="nanomsg PUB bind URL")
    p.add_argument("-t", "--num-tx", type=int, default=None)
    p.add_argument("-r", "--num-rx", type=int, default=None)
    p.add_argument("-g", "--gain", type=float, default=1.0)
    p.add_argument("-c", "--ctl", default="/tmp/vrtsim_taps_ctl.sock")

    # Dual-endpoint overrides (env-backed so compose files can tune them)
    p.add_argument("--dl-url", default=os.environ.get("SIONNA_DL_URL", "ipc:///tmp/vrtsim_taps_server.ipc"))
    p.add_argument("--dl-t", type=int, default=int(os.environ.get("SIONNA_DL_TX", "2")))
    p.add_argument("--dl-r", type=int, default=int(os.environ.get("SIONNA_DL_RX", "2")))
    p.add_argument("--dl-ctl", default=os.environ.get("TAPS_DL_CTL_SOCK", "/tmp/vrtsim_taps_dl.ctl"))
    p.add_argument("--dl-g", type=float, default=float(os.environ.get("SIONNA_DL_GAIN", "1.0")))
    p.add_argument("--ul-url", default=os.environ.get("SIONNA_UL_URL", "ipc:///tmp/vrtsim_taps_client.ipc"))
    p.add_argument("--ul-t", type=int, default=int(os.environ.get("SIONNA_UL_TX", "1")))
    p.add_argument("--ul-r", type=int, default=int(os.environ.get("SIONNA_UL_RX", "2")))
    p.add_argument("--ul-ctl", default=os.environ.get("TAPS_UL_CTL_SOCK", "/tmp/vrtsim_taps_ul.ctl"))
    p.add_argument("--ul-g", type=float, default=float(os.environ.get("SIONNA_UL_GAIN", "1.0")))
    ns = p.parse_args(argv)

    # argparse does not check `choices` against defaults, so TAPS_STATIC_MODE needs its own check.
    if ns.static_mode not in STATIC_MODES:
        p.error(f"static mode must be one of {', '.join(STATIC_MODES)}, got {ns.static_mode!r}")

    if ns.dual:
        dl = EndpointConfig("DL", ns.dl_url, ns.dl_ctl, ns.dl_t, ns.dl_r,
                            ns.taps_len, ns.id, ns.dl_g, ns.period_ms, ns.source, ns.static_mode)
        ul = EndpointConfig("UL", ns.ul_url, ns.ul_ctl, ns.ul_t, ns.ul_r,
                            ns.taps_len, ns.id, ns.ul_g, ns.period_ms, ns.source, ns.static_mode)
        dl_state, ul_state = State(), State()
        dl_thread = threading.Thread(target=run_publisher, args=(dl, dl_state, ul_state))
        ul_thread = threading.Thread(target=run_publisher, args=(ul, ul_state, dl_state))
        dl_thread.start()
        ul_thread.start()
        try:
            while dl_thread.is_alive() or ul_thread.is_alive():
                dl_thread.join(timeout=0.5)
                ul_thread.join(timeout=0.5)
        except KeyboardInterrupt:
            dl_state.running = ul_state.running = False
            dl_thread.join()
            ul_thread.join()
        return 0

    if ns.url is None or ns.num_tx is None or ns.num_rx is None:
        p.error("single-endpoint mode needs -u <url> -t <num_tx> -r <num_rx> (or use --dual)")
    single = EndpointConfig("PUB", ns.url, ns.ctl, ns.num_tx, ns.num_rx,
                            ns.taps_len, ns.id, ns.gain, ns.period_ms, ns.source, ns.static_mode)
    return run_publisher(single, State(), None)


if __name__ == "__main__":
    sys.exit(main())
