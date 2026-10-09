<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# Spectrum Service Model (SM) Architecture

This document describes the E3 Spectrum Service Model (RAN Function ID = 1) in
OpenAirInterface: the sensing-range telemetry it emits, and how it is fed by the
gNB MAC.

## Overview

The Spectrum SM is a **telemetry-out** service model. It advertises no control
identifiers: a dApp can observe the RAN through it but cannot act on it.

1. **Telemetry** (TIDs 1-5): one indication per MAC sensing publish, carrying a
   reference to the per-slot **sensing ranges** written into a shared-memory ring.

**Post-FFT IQ telemetry is NOT served by this SM.** IQ is served by the
**L1-KPM SM (RAN Function ID = 2)** over the `/e3_ran_buffers` shared-memory
region -- see the [L1-KPM SM README](../l1_kpm_sm/README.md). The Spectrum SM
carries only L2 sensing.

The wire encoding (ASN.1, JSON or Protocol Buffers) is selected at runtime in the
config file (`E3Configuration.encoding`); the encodings are field-for-field
equivalent and one is active per run.

```text
┌───────────────────────────────────────────────────────────────────────────┐
│                                   dApp                                    │
│      (reads /e3_l2_sensing and /e3_ran_buffers via read-only mmap)        │
└──────────────────────────────▲──────────────────────────▲─────────────────┘
                               │ sensing-range reference  │ IQ reference
                               │ indications (RF=1)       │ indications (RF=2)
┌──────────────────────────────┴──────────────────────────┴─────────────────┐
│           E3 agent (libe3; encoding selected at runtime from conf)        │
└──────────────────────────────▲──────────────────────────▲─────────────────┘
                               │                          │
                     ┌─────────┴─────────┐      ┌─────────┴─────────┐
                     │ Spectrum SM       │      │ L1-KPM SM         │
                     │ telemetry worker  │      │ telemetry worker  │
                     └─────────▲─────────┘      └─────────▲─────────┘
                               │ woken per publish        │ woken per publish
                     ┌─────────┴─────────┐      ┌─────────┴─────────┐
                     │ /e3_l2_sensing    │      │ /e3_ran_buffers   │
                     │ ring ◄── MAC      │      │ shm ◄── PHY       │
                     │ sensing scan      │      │ rxdataF push      │
                     └───────────────────┘      └───────────────────┘
```

The MAC half (slot reservation, the scan, the Aerial capture PUSCH) is the
Spectrum *RAN function*, `openair2/E3AP/ran_func_spectrum.c`. The scheduler only
calls the three hooks in `ran_func_spectrum_extern.h`; see
[README.md](README.md).

---

## 1. Sensing-Range Telemetry (Indication Path)

The SM registers telemetry IDs 1–5 and runs a **worker thread** driven by the
libe3 SM lifecycle callbacks. The worker sleeps until the MAC publishes a new
sensing result (or its period elapses), then emits one indication.

```text
MAC UL scheduler (per scheduled UL/MIXED slot)
  nr_mac_sensing_scan_and_publish() ── ranges ──► publish channel (wakes worker)
                                                          │
                                            Spectrum SM worker thread
                                                          │ writes one ring slot
                                                          ▼
                                                 /e3_l2_sensing shm ring
                                                          │ shm reference only
                                                          ▼
                                     Spectrum-SensingIndication ──► dApp (mmap read)
```

### 1.1 Indication by reference

Rather than inlining up to `MAX_SENSING_RANGES` (128) ranges in every indication,
the ranges are written **out of band** into the `/e3_l2_sensing` shared-memory
ring, and the indication carries only a small reference.

```asn1
Spectrum-SensingIndication ::= SEQUENCE {
    timestamp   INTEGER,                    -- CLOCK_MONOTONIC ns at publish
    sfn         INTEGER (0..65535),
    slot        INTEGER (0..65535),
    beam        INTEGER (0..3) OPTIONAL,
    shmName     ...,                        -- e.g. "/e3_l2_sensing"
    writeIdx    ...,                        -- ring slot index just written
    nRanges     ...                         -- live sensing_range_t records, 0..128
}
```

The dApp maps the region read-only and reads the raw `sensing_range_t` records at
`writeIdx` directly — no per-indication range parse, far less wire traffic.

### 1.2 The `/e3_l2_sensing` ring (`spectrum_sensing_ring.c`)

- **Layout**: a 64-byte header (`version`, `slot_count`, `slot_stride`,
  `max_ranges`, `range_size`) followed by `SPECTRUM_SENSING_RING_SLOTS` (256)
  slots. Each slot is self-tagged with `{sfn, slot, beam, n_ranges, timestamp_ns,
  seq}` and a fixed-size `ranges[MAX_SENSING_RANGES]` array. Total ≈ 530 KB.
- **Freshness**: the dApp checks a slot's `(sfn, slot)` against the indication
  before trusting it, so a wrapped-over (stale) slot is dropped, never used. At
  ~one write per UL slot (~0.5 ms), 256 slots ≈ 128 ms of history — far more than
  the dApp's read latency.
- **Concurrency**: single producer (the worker thread), so no lock — the slot is
  fully written before its indication is sent. The header's `slot_stride` /
  `range_size` let the dApp check its compiled layout against the live producer
  and bail on a mismatch.

---

## 2. RAN Function Metadata

`create_spectrum_sm_model()` builds the `e3_c_service_model_desc_t` registered
with the E3 agent: RAN Function ID 1, no control IDs, telemetry IDs `{1..5}`, and
a `Spectrum-RanFunctionData` blob (SM name, version, description) encoded with
the active encoder. `spectrum_sm_set_handle()` provides the handle used to emit
indications.

---

## 3. Files Reference

Spectrum SM

| File | Purpose |
|------|---------|
| `spectrum_sm.c` / `spectrum_sm.h` | SM descriptor and telemetry worker |
| `spectrum_enc.c` / `spectrum_enc.h` | Encode the sensing indication + RAN-function data (encoding selected at runtime) |
| `spectrum_sensing_ring.c` / `spectrum_sensing_ring.h` | `/e3_l2_sensing` shm ring producer + layout |
| `ran_func_spectrum_stub.c` | Weak fallbacks for binaries that do not link the MAC |
| `MESSAGES/ASN1/V1/e3sm_spectrum.asn` | Wire message definitions |

Spectrum RAN function (MAC side)

| File | Purpose |
|------|---------|
| `openair2/E3AP/ran_func_spectrum.c` | Slot reservation, sensing scan, publish, and the sensing configuration |
| `openair2/E3AP/ran_func_spectrum_aerial.c` | Aerial-only capture PUSCH |
| `openair2/E3AP/ran_func_spectrum_extern.h` | What the scheduler may call |
| `openair2/E3AP/ran_func_spectrum_types.h` | Data types and read API shared with the SM |
| [README.md](README.md) | Sensing scan/reserve/publish design |

---

## 4. Key Functions

- `create_spectrum_sm_model()` -- build the SM descriptor for E3 registration.
- `spectrum_sm_set_handle()` -- provide the handle used to emit indications.
- `spectrum_sensing_ring_write()` -- write one slot of ranges into `/e3_l2_sensing`.
- `nr_mac_get_sensing_ranges()` / `nr_mac_wait_for_sensing_publish()` -- the SM's read side of the MAC's per-slot snapshot.
