# NTN trace tools

Helpers for the trace-driven LEO satellite channel model of the rfsimulator
(`sat_trace_file` in the `channelmod` section, see
[ntn-configuration.md](../../doc/ntn-configuration.md#trace-driven-leo-satellite)).

| Script | Purpose | Dependencies |
| --- | --- | --- |
| `download_tle.py` | download/filter TLEs from CelesTrak | - |
| `generate_orbital_trace.py` | propagate one TLE (SGP4) over a pass, write the trace CSV | `numpy`, `skyfield` |
| `calibrate_ntn.py` | SIB19 / UE parameters matching a trace and ground positions | - |

`data/` holds the default TLE (`default_leo600.tle`, a 600 km polar orbit) and a
300 s sample trace (`orbital_trace_s_300.csv`, Starlink DTC pass).

## Trace format

CSV with header. Columns are identified by name, the order does not matter:

```
time_s,pos_x,pos_y,pos_z,vel_x,vel_y,vel_z
0,-4165355.542,-5103358.603,1412186.247,4093.643,-1684.703,5943.4
1,-4161259.338,-5105040.275,1418128.68,4098.808,-1678.667,5941.548
```

- `time_s` or `time_ms`: time since trace start, strictly increasing
- `pos_*`: satellite ECEF position (m), `vel_*`: satellite ECEF velocity (m/s)

## Workflow

```bash
cd tools/ntn
pip install numpy skyfield

# 1. TLEs, e.g. Starlink Direct-to-Cell
python3 download_tle.py starlink --name-contains "[DTC]" -o starlink_dtc.tle

# 2. 600 s trace of the first pass of satellite 0 over a ground station (ECEF, m),
#    starting 385 s before culmination (--start-offset)
python3 generate_orbital_trace.py -g s -t 600 --tle starlink_dtc.tle --sat-index 0 \
  --gnd-position '{"x": -2706714.0, "y": -4261882.0, "z": 3885680.0}' -o trace.csv

# 3. gNB/UE configuration values for this trace (UE and gNB positions as in channelmod)
python3 calibrate_ntn.py trace.csv --ue=-2706714,-4261882,3885680 --gnb=-2706714,-4261882,3885680 --fc 2488400000
```

`-g ms` writes a trace with 1 ms resolution (`time_ms`), `-t` is then in ms.
