# Shared tap assets

Shared FlatBuffers schema (`taps.fbs`), publisher (`publish_taps.py`), and
control client (`set-gain`) used by the static tap producer (and future CIR sources).

Local host setup (Docker images already do this at build time):

```bash
sudo apt-get install -y flatbuffers-compiler libnanomsg5
flatc --python taps.fbs          # generates Phy/
pip3 install -r requirements.txt
```

`--source static` defaults to `--static-mode awgn` (identity H; noise from
`channelmod.noise_power_dBFS`). Use `TAPS_STATIC_MODE=dft` for unitary DFT, or
`tx0` for the legacy TX0-only SIMO tap.
