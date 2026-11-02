# Remote CIR taps for vrtsim

`vrtsim` takes CIRs over nanomsg (`--vrtsim.taps-socket`). Aset images need
`OAI_VRTSIM_TAPS_CLIENT=ON` (enabled in the aset build Dockerfile).
Without that rebuild, `--vrtsim.taps-socket` asserts - use base `docker compose up` only.

## Compose: `taps` service

```bash
cd doc/tutorial_resources/aset
./build-containers.sh
# the `taps` service is part of the base docker-compose.yml
docker compose up --build
```

The `taps` container runs shared [`../common/publish_taps.py`](../common/publish_taps.py)
with `--dual --source static`, publishing DL and UL CIR taps on separate nanomsg
sockets (O-RU server / UE client).

Default static topology is `--static-mode awgn`: identity H (full-rank when
square) plus AWGN from `channelmod.noise_power_dBFS` in the RU/UE confs.
Alternatives:

| Mode | Behaviour |
|------|-----------|
| `awgn` (default) | H = gain * I (wrapped); flat AWGN MIMO |
| `dft` | Unitary DFT; robust under any 2-port PMI |
| `diag` | Alias of `awgn` |
| `tx0` | Legacy TX0-only fan-out (rank 1 / 1xN SIMO) |

Override via compose env `TAPS_STATIC_MODE` or CLI `--static-mode`.

Live gain (no restart). Default unit is **dB** (converted to linear amplitude):

```bash
docker exec taps set-gain --dl -20          # DL ~ -20 dB
docker exec taps set-gain --ul -6           # UL
docker exec taps set-gain --dl 0            # 0 dB (unity)
docker exec taps set-gain --dl --linear 0.1 # same as -20 dB, amplitude form
docker exec taps set-gain --ul status
```

Control sockets inside the container: `/tmp/vrtsim_taps_dl.ctl` and
`/tmp/vrtsim_taps_ul.ctl` (or pass the path explicitly to `set-gain`).

## Host run (optional)

```bash
cd doc/tutorial_resources/aset/taps/common
sudo apt-get install -y flatbuffers-compiler libnanomsg5 python3-pip
flatc --python taps.fbs
pip3 install -r requirements.txt
python3 publish_taps.py --dual --source static &
# optional: TAPS_STATIC_MODE=dft python3 publish_taps.py --dual --source static
./set-gain --dl -20
./set-gain --ul -6
```

Single-endpoint mode is still supported for testing:

```bash
python3 publish_taps.py -u ipc:///tmp/vrtsim_taps_server.ipc -t 2 -r 2 \
  -c /tmp/vrtsim_taps_dl.ctl --source static --static-mode awgn
```
