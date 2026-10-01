<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# Spectrum Service Model (SM) Architecture

This document describes the E3 Spectrum Service Model (RAN Function ID = 1) in
OpenAirInterface: the controls it accepts from a dApp, the sensing-range
telemetry it emits, and its role in the xApp–dApp bridge.

## Overview

The Spectrum SM is a **control-in / telemetry-out** service model:

1. **Control** — two controls the dApp (or a relayed xApp control) can issue:
   - `prbBlacklist` (control_id 1): block a set of PRBs in the scheduler.
   - `sensingPolicy` (control_id 2): tell the UL scheduler which symbols to keep
     free per slot so the sensing scan sees clean spectrum.
2. **Telemetry** (TIDs 1–5): one indication per MAC sensing publish, carrying a
   reference to the per-slot **sensing ranges** written into a shared-memory ring.
3. **xApp–dApp bridge**: when built with `E2_AGENT`, PRB information is exchanged
   between an xApp (over E2) and the dApp (over E3).

**Post-FFT IQ telemetry is NOT served by this SM.** IQ is served by the
**L1-KPM SM (RAN Function ID = 2)** over the `/e3_ran_buffers` shared-memory
region -- see the [L1-KPM SM README](../l1_kpm_sm/README.md). The Spectrum SM
carries only L2 sensing/control.

The wire encoding (ASN.1, JSON or Protocol Buffers) is selected at runtime in the
config file (`E3Configuration.encoding`); the encodings are field-for-field
equivalent and one is active per run.

```text
┌───────────────────────────────────────────────────────────────────────────┐
│                                   dApp                                    │
│      (reads /e3_l2_sensing and /e3_ran_buffers via read-only mmap)        │
└────────┬───────────────────────▲──────────────────────────▲───────────────┘
         │ controls (RF=1)       │ sensing-range reference  │ IQ reference
         │ prbBlacklist /            │ indications (RF=1)       │ indications (RF=2)
         ▼ sensingPolicy         │                          │
┌───────────────────────────────────────────────────────────────────────────┐
│           E3 agent (libe3; encoding selected at runtime from conf)        │
└────────┬───────────────────────▲──────────────────────────▲───────────────┘
         ▼                       │                          │
┌───────────────────┐   ┌────────┴──────────┐     ┌─────────┴─────────┐
│ Spectrum SM       │   │ Spectrum SM       │     │ L1-KPM SM     │
│ control dispatch  │   │ telemetry worker  │     │ telemetry worker  │
└────────┬──────────┘   └────────▲──────────┘     └─────────▲─────────┘
         ▼                       │ woken per publish        │ woken per publish
┌───────────────────┐   ┌────────┴──────────┐     ┌─────────┴─────────┐
│ MAC: PRB block    │   │ /e3_l2_sensing    │     │ /e3_ran_buffers   │
│ mask + sensing    │   │ ring ◄── MAC      │     │ shm ◄── PHY       │
│ policy state      │   │ sensing scan      │     │ rxdataF push      │
└───────────────────┘   └───────────────────┘     └───────────────────┘
```

The MAC half is the Spectrum *RAN function*: `openair2/E3AP/ran_func_spectrum.c`
(slot reservation, the scan, the sensing policy and the UL TDA selector, the
Aerial capture PUSCH) and `ran_func_spectrum_prb_block.c` (PRB blocking). The
scheduler only calls the hooks declared in `ran_func_spectrum_extern.h`; see
[README.md](README.md).

---

## 1. Control Path

The E3 framework dispatches an incoming control to `spectrum_sm_process_control()`
(in `spectrum_sm.c`), which switches on `control_id`:

```text
dApp ──(E3 control, RF=1)──► E3 agent ──► spectrum_sm_process_control()
                                           │ control_id=1        │ control_id=2
                                           ▼                     ▼
                              spectrum_process_prb_block()  spectrum_process_sensing_policy()
                                           │                     │
                                           ▼                     ▼
                                 set_prb_block_mask()       set_sensing_policy()
                                           │                     │
                                           ▼                     ▼
                              apply_prb_block_masks()      mask-aware UL TDA selector
                              (per-slot stamp into         (keeps the masked symbols
                               vrb_map / vrb_map_UL)        free, per slot)
```

| control_id | Name | Wire message | Handler | Effect |
|---|---|---|---|---|
| 1 | `prbBlacklist` | `Spectrum-PRBBlacklistControl` | `spectrum_process_prb_block()` | Install/clear a PRB block |
| 2 | `sensingPolicy` | `Spectrum-SensingPolicyControl` | `spectrum_process_sensing_policy()` | Set/clear the per-slot sensing mask |

Each handler decodes the payload, drives the MAC, and replies with a positive or
negative message ACK (a negative ACK is paired with a `LOG_W` naming the cause).

### 1.1 PRB block (control_id 1)

`Spectrum-PRBBlacklistControl` carries `blacklistedPRBs` (the full current list of PRB
indices to block) plus optional `samplingThreshold` / `validityPeriod`.

```asn1
Spectrum-PRBBlacklistControl ::= SEQUENCE {
    blacklistedPRBs    SEQUENCE (SIZE (0..maxPRBs)) OF INTEGER (0..maxPRBIndex),
    samplingThreshold  INTEGER (0..100)   OPTIONAL,
    validityPeriod     INTEGER (1..3600)  OPTIONAL
}
```

`spectrum_process_prb_block()`:

1. Decodes the payload (`spectrum_decode_prb_control()`).
2. Builds a per-PRB symbol bitmap: `0x3FFF` (all 14 OFDM symbols occupied) for
   each requested PRB, `0` elsewhere.
3. Calls `set_prb_block_mask()` for **both** directions (`PRB_BLOCK_DIR_UL` and
   `PRB_BLOCK_DIR_DL`) — air interference is bidirectional and the wire format
   carries no direction field, so the one list drives UL and DL.

A non-empty list **replaces** the persistent block set; an **empty list clears**
it. The dApp sends the full current list on every change.

The block is per-cell state owned by `ran_func_spectrum_prb_block.c`, allocated
by `prb_block_attach()` from `e3_spectrum_mac_attach_cell()`; no block data lives
in the MAC structures.
Enforcement is a per-slot stamp: `apply_prb_block_masks()` OR's the block bitmap
into `vrb_map` (DL) and `vrb_map_UL` at slot start, so every downstream
scheduling step treats blocked PRBs as occupied. Static control channels are
protected and per-channel collisions are reported; see
`PRB_BLOCK_LIMITATIONS.md` and the policy notes in `ran_func_spectrum_prb_block.c`.

### 1.2 Sensing policy (control_id 2)

`Spectrum-SensingPolicyControl` carries `maskPerSlot` — one symbol mask per slot
in the frame; a set bit means "keep this symbol free for sensing".

```asn1
Spectrum-SensingPolicyControl ::= SEQUENCE {
    maskPerSlot     SEQUENCE (SIZE (1..maxSlotsFrame)) OF INTEGER (0..maxSymbolMask),
    deactivate      BOOLEAN DEFAULT FALSE,
    validityPeriod  INTEGER (1..3600) OPTIONAL
}
```

`spectrum_process_sensing_policy()` decodes it (`spectrum_decode_sensing_policy()`)
and forwards it to `set_sensing_policy()` in `ran_func_spectrum.c`, which
drives the mask-aware UL TDA selector. When `deactivate` is set, the policy is
cleared regardless of the mask. `set_sensing_policy()` validates the mask length
against the MAC's slots-per-frame and returns false (→ negative ACK) on mismatch.
The MAC sensing scan/reserve/publish machinery itself is documented in
[README.md](README.md).

---

## 2. Sensing-Range Telemetry (Indication Path)

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

### 2.1 Indication by reference

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

### 2.2 The `/e3_l2_sensing` ring (`spectrum_sensing_ring.c`)

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

## 3. RAN Function Metadata

`create_spectrum_sm_model()` builds the `e3_c_service_model_desc_t` registered
with the E3 agent: RAN Function ID 1, control IDs `{1, 2}`, telemetry IDs
`{1..5}`, and a `Spectrum-RanFunctionData` blob (SM name, version, description)
encoded with the active encoder. `spectrum_sm_set_handle()` provides the handle
used to emit indications and ACKs.

---

## 4. xApp–dApp Interaction (E2–E3 Bridge)

When built with `E2_AGENT`, PRB information is exchanged between an external xApp
(over E2) and the local dApp (over E3). Two ASN.1 structures support this:

- **xApp → dApp**: `Spectrum-PRBBlockedControl { blockedPRBs }` — the xApp
  suggests PRBs to block; the E2 agent hands the control to the E3 agent, which
  forwards it to the connected dApp.
- **dApp → xApp**: `Spectrum-PRBBlacklistReport { blacklistedPRBs }` — the dApp
  reports its current PRB set; it arrives at the E3 agent as a dApp report.

The E3 agent installs `e2_e3_bridge()` as its dApp-report handler in `e3_init()`;
that handler calls `generate_e2_indication_from_e3_dapp_report()` (declared in
`../../../E2AP/RAN_FUNCTION/O-RAN/ran_func_dapp_extern.h`) to emit an E2
indication to subscribed xApps.

```text
┌──────┐   Spectrum-PRBBlockedControl    ┌──────────┐   E3   ┌──────┐
│ xApp │ ─────────────────────────────►  │ E2 agent │ ─────► │ dApp │
│      │ ◄─────────────────────────────  │          │ ◄───── │      │
└──────┘   Spectrum-PRBBlacklistReport   └──────────┘        └──────┘
```

Both the xApp and the dApp can act on the RAN independently; this channel lets
them exchange PRB-level information first, enabling coordinated decisions.

---

## 5. Files Reference

RAN — Spectrum SM

| File | Purpose |
|------|---------|
| `spectrum_sm.c` / `spectrum_sm.h` | SM descriptor, control dispatch, telemetry worker |
| `spectrum_dec.c` / `spectrum_dec.h` | Decode `prbBlacklist` / `sensingPolicy` control payloads |
| `spectrum_enc.c` / `spectrum_enc.h` | Encode the sensing indication + RAN-function data (encoding selected at runtime) |
| `spectrum_sensing_ring.c` / `spectrum_sensing_ring.h` | `/e3_l2_sensing` shm ring producer + layout |
| `MESSAGES/ASN1/V1/e3sm_spectrum.asn` | Wire message definitions |

RAN — Spectrum RAN function (`openair2/E3AP/`)

| File | Purpose |
|------|---------|
| `ran_func_spectrum.c` | Slot reservation, sensing scan/publish, sensing policy, mask-aware UL TDA selector, sensing configuration |
| `ran_func_spectrum_prb_block.c` | `set_prb_block_mask()`, per-slot `apply_prb_block_masks()` enforcement |
| `ran_func_spectrum_collision.c` / `ran_func_spectrum_registry.c` | Blocked-PRB collision handling and the periodic-allocation registry |
| `ran_func_spectrum_aerial.c` | Aerial-only capture PUSCH |
| `ran_func_spectrum_extern.h` | What the scheduler may call |
| `ran_func_spectrum_types.h` | Data types and entry points shared with the SM |
| `spectrum_sm/ran_func_spectrum_stub.c` | Weak fallbacks for binaries that do not link the MAC |
| [README.md](README.md) | Sensing scan/reserve/publish design |
| [PRB_BLOCK_LIMITATIONS.md](PRB_BLOCK_LIMITATIONS.md) | PRB-block behaviour and limitations |

xApp–dApp bridge

| File | Purpose |
|------|---------|
| `openair2/E3AP/e3_agent.c` / `e3_agent.h` | E3 agent lifecycle; installs `e2_e3_bridge()` as the dApp-report handler |
| `openair2/E2AP/RAN_FUNCTION/O-RAN/ran_func_dapp_extern.h` | Declares `generate_e2_indication_from_e3_dapp_report()` |

---

## 6. Key Functions

- `create_spectrum_sm_model()` — build the SM descriptor for E3 registration.
- `spectrum_sm_set_handle()` — provide the handle used to emit indications/ACKs.
- `spectrum_sm_process_control()` — control dispatcher (switch on `control_id`).
- `spectrum_process_prb_block()` — apply `prbBlacklist` via `set_prb_block_mask()`.
- `spectrum_process_sensing_policy()` — apply `sensingPolicy` via `set_sensing_policy()`.
- `spectrum_decode_prb_control()` / `spectrum_decode_sensing_policy()` — decode control payloads.
- `spectrum_free_decoded_control()` / `spectrum_free_sensing_policy()` — free decoded payloads.
- `spectrum_sensing_ring_write()` — write one slot of ranges into `/e3_l2_sensing`.
