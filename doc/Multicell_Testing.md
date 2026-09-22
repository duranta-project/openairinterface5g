# Multi-cell Testing with RFsimulator

This document describes how to configure and run a multi-cell gNB using the RFsimulator.
The setup runs one monolithic `nr-softmodem` instance serving two cells, but only one
cell can be active on the air at a time (see [Current limitation](#current-limitation--one-cell-broadcasts-at-a-time)).

## Configuration

The sample config is at:

```
targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.sa.band77.51prb.rfsim.2cells.yaml
```

### Config file structure

Each cell is expressed as a separate `gNBs[]` entry, all sharing the same `gNB_ID`
and `gNB_name`. Transport and core-network parameters (`amf_ip_address`,
`NETWORK_INTERFACES`, `SCTP`, `TIMERS`) are only read from the first entry and must
not be repeated. There must be one `MACRLCs[]` entry per cell.

```yaml
gNBs:
  # Cell 0 — gNB-wide params (AMF, SCTP, TIMERS) live here
  - gNB_ID: "0xe00"
    gNB_name: gNB-OAI
    tracking_area_code: 1
    nr_cellid: "12345678"
    amf_ip_address: [...]
    NETWORK_INTERFACES: {...}
    TIMERS: {...}
    servingCellConfigCommon:
      - physCellId: 0
        absoluteFrequencySSB: 667200   # ~4008 MHz
        dl_absoluteFrequencyPointA: 666672
        ...

  # Cell 1 — per-cell params only; gNB-wide params are ignored here
  - gNB_ID: "0xe00"
    gNB_name: gNB-OAI
    tracking_area_code: 2
    nr_cellid: "11111111"
    servingCellConfigCommon:
      - physCellId: 1
        absoluteFrequencySSB: 677376   # ~4161 MHz
        dl_absoluteFrequencyPointA: 676848
        ...

MACRLCs:
  - tr_s_preference: local_L1    # one entry per cell
    tr_n_preference: local_RRC
  - tr_s_preference: local_L1
    tr_n_preference: local_RRC
```

### Cell summary

|             | Cell 0 | Cell 1 |
|-------------|--------|--------|
| PCI         | 0      | 1      |
| NR Cell ID  | 12345678 | 11111111 |
| SSB ARFCN   | 667200 | 677376 |
| SSB frequency | ~4008 MHz | ~4161 MHz |
| Point A ARFCN | 666672 | 676848 |
| Bandwidth   | 51 PRBs, 30 kHz SCS | 51 PRBs, 30 kHz SCS |
| Band        | n77 | n77 |

## Current limitation — one cell broadcasts at a time

With the current implementation, only one cell can drive the PHY (i.e. have its carrier
frequency applied to the RFsimulator) at a time. The active cell is selected by a guard
in `openair2/LAYER2/NR_MAC_gNB/config.c` around the `NR_PHY_config_req` call:

```c
// Select cells[0] for cell 0, cells[1] for cell 1, etc.
if (NFAPI_MODE == NFAPI_MONOLITHIC && cell == &nrmac->cells[0]) {
```

Change the index, then recompile:

```bash
# from the build directory
ninja nr-softmodem nr-uesoftmodem
```

When the gNB starts it logs the exact UE command-line parameters for the active cell:

```
[PHY]    Cell 0 command line parameters for OAI UE: -C <freq_hz> -r 51 --numerology 1 --ssb <ssb_offset>
```

Use those values when starting the UE. Only start the UE that matches the active cell's
frequency; the other cell's MAC scheduling runs but its carrier is not visible on the air.

## Running

All commands are run from the build directory. Open two terminals plus one per UE.

### Terminal 1 — gNB (both cells configured, one active on the air)

```bash
sudo ./nr-softmodem \
  -O <path-to>/targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.sa.band77.51prb.rfsim.2cells.yaml \
  --rfsim
```

The gNB prints the active cell's parameters on startup — use them for the UE command below.

### Terminal 2 — UE (connect to the active cell)

Use the `-C`, `-r`, `--numerology`, and `--ssb` values printed by the gNB log:

```bash
sudo ./nr-uesoftmodem \
  -O <path-to>/targets/PROJECTS/GENERIC-NR-5GC/CONF/ue.conf \
  --uicc0.imsi 001010000000001 \
  --rfsim -C <freq_hz> -r 51 --numerology 1 --ssb <ssb_offset>
```

## Adding a third cell

1. Add a third `gNBs[]` entry with a distinct `physCellId`, `nr_cellid`, and
   `absoluteFrequencySSB`/`dl_absoluteFrequencyPointA` pair.
2. Add a third `MACRLCs[]` entry (identical to the existing ones).
3. To test it, update the guard in `config.c` to `cells[2]` and recompile.

`NR_MAX_CELLS` in `openair2/LAYER2/NR_MAC_gNB/nr_mac_gNB.h` is the compile-time upper
bound on cells per MAC instance (currently 4).
