<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# L1-KPM service model (RAN Function ID = 2)

The L1-KPM SM streams post-FFT, frequency-domain IQ from the gNB PHY to dApps.
It is telemetry-only: it advertises no control identifiers. It is wire-compatible
with NVIDIA cuBB's KPM SM, so a dApp written against Aerial connects unchanged.

The SM registers telemetry IDs 1 (IQ samples), 4 (timestamp), 5 (SFN) and 6 (slot).
The wire encoding (ASN.1, JSON or Protocol Buffers) is selected at runtime with
`E3Configuration.encoding`; the three are field-for-field equivalent and one is
active per run.

## Data path

```text
PHY uplink receive (per UL/MIXED slot)
  e3_ran_buffers_push_rxdataF() ── IQ ──► /e3_ran_buffers (POSIX shm, FP16)
                                              │ wakes the SM worker
                                              ▼
                              L1KPM-Indication (slot indices, sfn, slot, timestamp)
                                              │ reference only
                                              ▼
                                        dApp (read-only mmap)
```

- **Capture point**: `phy_procedures_gNB_uespec_RX()` in
  `openair1/SCHED_NR/phy_procedures_nr_gNB.c` calls `e3_ran_buffers_push_rxdataF()`
  for each UL/MIXED slot, but only while at least one dApp is subscribed to RF=2.
  The copy is of `rxdataF`, before equalization: transmitted signal, channel, noise
  and interference, which suits spectrum monitoring.
- **Transport**: the `/e3_ran_buffers` POSIX shared-memory region
  (`e3_ran_buffers.c`), created with mode 0644 so a dApp in another container can
  map it read-only. IQ is stored as FP16 pairs with layout `[ant][sym][prb][sc]`
  (4 antennas, 14 symbols, 273 PRBs, 12 subcarriers); a smaller PHY grid is
  zero-filled to that shape.
- **Indication by reference**: the indication carries the buffer/write indices, the
  timestamp, `sfn`, `slot` and, optionally, `cellId`, `nRxAnt` and `validSymbolMask`.
  The IQ itself is never on the wire. `validSymbolMask` flags the symbols of a
  MIXED slot that carry genuine off-air uplink; the leading DL/guard symbols hold the
  gNB's own transmit leakage and should be discarded.
- **FP16 scale**: IQ is multiplied by a fixed factor before the FP16 conversion.
  It is a per-front-end constant chosen at build time: 1/2048 (cuPHY's baseline, also
  right for 7.2-split front ends) by default, 1/128 when the build targets a USRP
  (`E3_FP16_BETA_USRP`, set in `CMakeLists.txt`). A dApp rescales by the inverse.

## Files

| File | Purpose |
|------|---------|
| `l1_kpm_sm.c` / `l1_kpm_sm.h` | SM descriptor and telemetry worker callbacks |
| `l1_kpm_enc.c` / `l1_kpm_enc.h` | Encode the indication and the RAN-function data (encoding selected at runtime) |
| `e3_ran_buffers.c` / `e3_ran_buffers.h` | `/e3_ran_buffers` region: layout, PHY-side producer, publish hand-off |
| `MESSAGES/ASN1/V1/` | ASN.1 message definitions |
| `MESSAGES/PROTO/V1/` | Protocol Buffers message definitions |
| `tests/` | Encoder round-trip tests (run with `ctest` when `ENABLE_TESTS` is on) |

The worker loop and the shared-memory region lifecycle are shared with the Spectrum SM
and live in `../e3_sm_worker.c` and `../e3_shm_region.c`.
