<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# Spectrum sensing + dApp control on the OAI gNB — design guide

This explains, component by component, **how the spectrum-sensing and dApp-control feature works** on this OAI gNB (running on the NVIDIA Aerial PHY split). It is written to be read top-to-bottom: each section says what a piece does, how it works, what data it moves, and which configuration controls it. There is no source code here — only the mechanisms and the reasoning behind them.

---

## 1. The big picture

There are three actors:

- **gNB (OAI, the "L2/MAC")** — schedules the radio, and additionally produces *sensing telemetry* (a map of which time-frequency resources are free) and accepts *control* from a dApp (e.g. "block these PRBs").
- **Aerial L1 (NVIDIA cuBB, the "PHY")** — does the actual radio signal processing and produces the raw IQ samples.
- **dApp** — an external application that consumes the telemetry + IQ, decides which spectrum is occupied, and sends control/reports back.

The end-to-end flow, once per uplink slot:

```text
                        ┌─────────────────────── gNB (OAI MAC) ───────────────────────┐
   radio  ──►  Aerial   │  scheduler                                                   │
   (IQ)        L1 (cuBB)│    ├─ 1. SENSING MASK   : scan the free PRB/symbol grid       │
                │       │    ├─ 2. CAPTURE PUSCH  : force cuBB to compute IQ there      │
                │       │    └─ 3. E3 TELEMETRY   : publish ranges (+ IQ ref) to dApps  │
                │       └──────────────────────────────────────────────────────────────┘
                │                       │                         ▲
                ▼ IQ (RF=2)             ▼ sensing ranges (RF=1)    │ PRB-block control / report (RF=1)
              ┌─────────────────────────────────────────────────────────────────────┐
              │                              dApp                                     │
              │  reads IQ + ranges  →  subcarrier power  →  detect occupied PRBs  →   │
              │  (a) block them on the gNB   and/or   (b) report them to an xApp      │
              └─────────────────────────────────────────────────────────────────────┘
```

Two important framing points:

1. **Sensing telemetry never changes scheduling.** The mask and the ranges are derived from a *copy* of the scheduler state and shipped out. The scheduler's own resource map is untouched by sensing. (The one thing that *does* touch the radio is the optional capture PUSCH — see §3 — and the dApp's PRB-block control — see §6.)
2. **IQ vs ranges come from two different agents.** In this deployment the dApp gets **IQ from cuBB's own E3 agent (RF=2)** and **sensing ranges from the OAI gNB (Spectrum SM, RF=1)**. It lines them up by `(sfn, slot)`. The gNB *also* ships an IQ service model (also RF=2 — RF ids are per-agent, and 2 means "the IQ KPM SM" on both agents), but it is not used here because cuBB already provides the IQ.

---

## 2. The sensing mask — a map of free spectrum

**What it is.** Every uplink (or mixed) slot, the gNB builds a list of **"sensing tiles"**: rectangles in the time-frequency grid `(start symbol, number of symbols) × (start PRB, number of PRBs)` that are *not* used by any scheduled transmission. Each tile is one `sensing_range_t`. Together they describe the spectrum that is free for sensing in that slot.

**How it works.**
1. After the scheduler has placed all its uplink allocations for the slot, a scan reads the slot's resource-block map and finds the contiguous free regions, emitting one tile per region.
2. The scan works on a **local copy** of the resource map, so finding (and reserving) tiles cannot influence any scheduling decision. This is the guarantee that "sensing is telemetry-only."

**The "perfect mask" refinement.** A naive free-region scan would mark band-edge resources as free even though the UE transmits control signals there *autonomously* (PUCCH for scheduling requests / CSI reports / HARQ-ACK, SRS, PRACH, and the cell-common PUCCH at both band edges). Those reception PDUs are sometimes dropped on sensing slots, so the scan can't "see" them. To avoid telling the dApp that occupied control resources are free, a helper computes the **UL control occupancy from RRC configuration** (what the UE *will* transmit) and unions it into the mask before the tiles are emitted. The result is a zero-leak mask: a tile is emitted only if it is genuinely free, including of autonomous UL control. This refinement is also telemetry-only — it only changes which tiles the dApp is told about, never the scheduler.

**What controls it.** `sensing_enabled` (see §8) is the master switch: the scan returns no tiles when it is off (`nr_scan_sensing_tiles` checks it first), which in turn suppresses the capture PUSCH (§3 — a no-op with zero tiles), and the E3 publish block checks the flag itself as well. With sensing disabled the whole scan→capture→publish pipeline is inert; with it enabled, the scan runs on every UL/MIXED slot.

---

## 3. The capture PUSCH — making cuBB produce IQ where we sensed

**The problem it solves.** On the Aerial split, cuBB only computes the frequency-domain IQ for resources that a scheduled PDU points at. If a slot has no uplink reception scheduled, cuBB produces no IQ for it — so the dApp would have nothing to look at on exactly the free slots we want to sense.

**What it does.** On a free uplink slot, the gNB injects a single **dummy "capture" PUSCH** — a normal-looking uplink grant addressed to a reserved sensing identity (`SENSING_RNTI`). It carries no real data and is *expected to fail its CRC*; its only purpose is to make cuBB run its uplink receive chain and fill the IQ buffer for that slot. The dApp then reads that IQ (from cuBB's RF=2 path) and analyses it.

**Key properties.**
- **It yields to real traffic.** The capture PUSCH is injected *only* when no real PUSCH/PUCCH/SRS is already scheduled in that slot — when a UE is actively transmitting, the gNB skips the dummy and the UE's own uplink produces the IQ.
- **One per slot ("cap=1").** A single dummy PUSCH is enough to trigger cuBB's slot-level IQ capture, and it keeps cuBB on its well-tested low-PDU-count path.
- **Aerial-only.** This whole mechanism is compiled in only for the Aerial build; a monolithic OAI L1 captures the whole slot anyway and does not need prompting.
- **Its shape is operator-configured** (PRB window, MCS, number of layers, beams) via the `sensing_pusch_*` keys (see §8).

**Two cuBB-specific behaviours bear on the capture PUSCH (see §7a–b):** cuBB recomputes and validates the dummy PUSCH's transport-block size, and it emits an "empty" uplink-control indication for the data-only capture slot. The gNB accommodates both.

---

## 4. E3 telemetry transport — getting the data to the dApp

E3 is the channel between the gNB and dApps. The gNB runs an **E3 agent** (a thin adapter over the external `libe3` library, which owns the transport, threads, and the reliable control protocol) plus a small set of **service models (SMs)**. Each SM is one "topic" a dApp can subscribe to, identified by a **RAN-function ID**:

| RF id | Service model | Direction | Carries |
|------:|---------------|-----------|---------|
| 1 | **Spectrum SM** | control in + report out + telemetry out | dApp’s PRB-block commands; the gNB’s sensing PRB report; the sensing ranges (the tiles from §2) |
| 2 | (NVIDIA cuBB's KPM SM) | telemetry out | IQ samples — *provided by cuBB, not the gNB* |
| 2 | **L1-KPM SM** (OAI agent) | telemetry out | PHY IQ metadata — *present but unused here; deliberately the same id as cuBB's KPM, in the OAI agent's own namespace* |

### 4a. Spectrum SM telemetry (RF=1) — shipping the sensing ranges

This is the SM the dApp uses for the mask. It sends *indications* carrying `(sfn, slot, beam, timestamp)` plus a reference to the slot's sensing ranges: one per sensing publish (every scheduled UL/MIXED slot) in the default on-data mode, or one per period when the subscribed dApps declare a periodicity. The wire encoding (JSON or ASN.1/APER) is **selected at runtime** in the config file (`E3Configuration.encoding`) — see §8.

Either encoding carries the ranges the same way — out-of-band:
- The ranges are written into a **shared-memory ring** (`/e3_l2_sensing`) on **every** sensing publish, regardless of the emission period — the ring is the full sensing record and the periodicity throttles only the indications, symmetric with the L1 side where `/e3_ran_buffers` is written on every UL/MIXED slot. The indication carries only a *reference* — the shm name, a write index, and a count — plus `(sfn, slot)`.
- The dApp memory-maps the ring read-only and reads the ranges directly. The message stays tiny regardless of tile count, and the `(sfn, slot)` stamped in both the message and the ring slot lets the dApp detect a torn/overwritten read and discard it.
- Both encodings carry the ranges by reference, not inline, so the ASN.1 schema has no inline `SensingRange` type. A dApp on either kind of build must be able to `mmap` the ring.

### 4b. L1-KPM SM (RF=2, OAI agent) — IQ metadata (present, unused here)

This SM owns a cuBB-compatible shared-memory region (`/e3_ran_buffers`) into which the PHY writes converted IQ, and it advertises a reference to it. It exists for parity/standalone use, but in this deployment cuBB's own agent (its RF=2) already supplies the IQ, so the dApp subscribes there instead and this SM is dormant.

### 4c. The shared SM worker (`e3_sm_worker`)

The two telemetry emitters (the Spectrum SM’s sensing ranges and the L1 SM’s IQ) have near-identical worker plumbing and share one **driver**: a single background-thread engine that handles the period/on-data wait loop, the lifecycle (start/stop/destroy) hooks, and the per-dApp fan-out of one indication per subscriber. Each emitter plugs in only its *differences* (where its data comes from, how it encodes, which shm it owns) through a small table of callbacks. The two emitters remain fully independent at runtime — separate threads, separate wake sources, separate shared memory — this is purely a compile-time de-duplication so there is one place to maintain the worker logic.

Each worker has two cadence modes, driven by the periodicity the subscribed
dApps declare in their E3 subscription (the agent re-derives it on every
subscription change; with several subscribers the fastest request wins):
- **on-data** (period 0/unset): emit one indication per publish (lowest latency — one per UL slot).
- **periodic** (period > 0, microseconds): emit at a fixed interval, re-using the latest snapshot.

### 4d. Spectrum SM (RF=1) — the control + report side

This is the bidirectional SM:
- **In:** the dApp sends commands — most importantly "block this set of PRBs (uplink and/or downlink)." The SM hands the PRB list to the scheduler (see §6). The command arrives in the build-selected wire format (§8).
- **Out:** the gNB can also *report* the detector's occupied PRBs as an indication, which the gNB relays onward to a higher-layer xApp.

**Lifecycle nuance (deliberate):** the SM is *force-started* when the agent registers it, so controls are accepted from gNB boot, before any dApp subscribes. But `libe3` ties the running state to subscriptions: when a dApp that subscribed later releases its *last* subscription, `libe3` stops the SM, and further controls are NACKed ("no running SM") until a new subscription restarts it. That is intentional — a torn-down session should not keep actuating the scheduler — and on SM stop the gNB defensively clears any active PRB block and sensing policy.

---

## 5. PRB-block actuation — letting the dApp reserve spectrum

When the dApp decides some PRBs are occupied (or an xApp tells it to), it sends a PRB-block command on the Spectrum SM. The gNB then **excludes those PRBs from scheduling**:

- The blocked-PRB set is OR'd into the scheduler's resource-block map for uplink and downlink at the start of every slot, so the allocators never place a UE there. Every requested PRB is stamped, for all channels — the block is enforced uniformly, with no per-channel policy.
- **Crash-safety.** The fixed cell-channel reservation steps (PRACH, Msg3, SRS, PUCCH, SIB) notice when their PRBs fall inside the block and skip that allocation gracefully (a rate-limited log) instead of asserting, so a block that lands on a dynamically-scheduled channel degrades it rather than crashing the gNB.
- **Collisions with fixed per-UE signals are detected at events.** PUCCH, SRS and NZP-CSI-RS sit on fixed PRBs assigned by RRC. The gNB keeps a small per-UE table of where those signals live, refreshed whenever a UE's bandwidth-part configuration is applied; it scans that table when a block is installed and re-checks a UE when it (re)configures, logging any signal the block lands on. Each signal is checked against the block for its own direction (PUCCH/SRS uplink, CSI-RS downlink). Today this only logs — it is the hook where a future version would relocate the affected signal off the blocked PRBs via an RRC reconfiguration.
- **Cell-common channels are not protected.** SSB, CORESET0, PRACH and the cell-common PUCCH are not kept clear of the block — a block over them takes effect and degrades the broadcast / random-access path. An already-connected UE tolerates a brief block, but a block over the downlink SSB/CORESET0 region stops a new UE from synchronizing. Keeping them schedulable is future work.
- **A sustained block can stall a HARQ retransmission.** A retransmission must reuse its original transport-block size, so it cannot shrink around a block; if a block sits on the only PRBs it can use, it keeps retrying until the UE is dropped by the inactivity timeout. There is no give-up guard for this case today.

The dApp sends the *complete* current PRB set on every change; the gNB mirrors it (it is not incremental).

---

## 6. Reliable dApp→gNB control

The dApp→gNB hop shares a lossy publish/subscribe channel with the high-rate report stream, so a single control command could be dropped. For commands that must not be lost (re-issuing an xApp's block), the dApp sends them **reliably**: each command carries a sequence number, the gNB acknowledges every command it applies, and the dApp retransmits an unacknowledged command at a fixed interval until it is acked (or a retry limit is hit). Duplicates (from retransmits) are detected by sequence number and applied once. This reliability layer lives inside `libe3`; the gNB side simply acks. The detector's own best-effort auto-block does not use this — only the must-not-lose re-issue path does.

---

## 7. Aerial-specific correctness behaviour

cuBB behaves differently from a monolithic OAI L1 in three places. §7a and §7b are what let the capture PUSCH (§3) work without destabilising the gNB; §7c is an unrelated startup-ordering race.

### 7a. Tolerate "empty" uplink-control indications
cuBB emits an uplink-control indication with **zero entries** for a slot that carried a data-only PUSCH and no PUCCH — exactly what the capture PUSCH produces. An empty indication carries nothing for the MAC, so the gNB **drops it at ingress** and processes only non-empty (real) uplink-control. (Enqueuing a zero-entry indication would trip an assertion deeper in the uplink path.)

### 7b. Capture-PUSCH transport-block size
cuBB independently recomputes the transport-block size from the PUSCH parameters and **rejects the PDU if the gNB's number disagrees**, so no IQ is captured. The gNB sizes the capture PUSCH's DMRS (reference-signal) overhead the same way the normal uplink scheduler does — scaling with the number of DMRS resource groups — so that for a multi-layer (>2-layer) capture PUSCH the transport-block size matches cuBB's and the PDU is accepted.

### 7c. Wait for the cell config before registering the PNF
cuBB's PARAM.response can arrive on the agent's receive thread before the MAC has finished applying the cell config, so the common subcarrier spacing the gNB needs to register the PNF may not be populated yet. The gNB polls for it, bounded by a timeout, and proceeds with a logged warning if it never appears. This is independent of the capture PUSCH.

> Note: §7a and §7b are what make sensing *function* under Aerial; they are unrelated to the separate uplink-throughput question (§10) — a real UE's uplink is a reception/RF matter, not a sensing matter.

---

## 8. Configuration reference

All of the gNB-side sensing knobs live **inside the `gNBs = (...)` section** of the config:

| Key | What it does |
|-----|--------------|
| `additional_ul_tdas` | Adds extra uplink time-domain allocations (shorter shapes) to the scheduler's menu so a sensing policy can prefer leaving symbols free. **Configuring it is what currently flips `sensing_enabled` on.** |
| `sensing_pusch_mcs` / `_rb_size` / `_rb_start` / `_nrOfLayers` / `_beams` | The shape of the capture PUSCH (§3): modulation/coding, PRB window, layers, beams. |
| `sensing_target_slots` | A list of slots (within the TDD period) to *hard-reserve* for sensing — on those slots the UE is pushed off so sensing gets a clean full-slot window. **Powerful but expensive: each reserved slot is taken from the UE.** Leave empty unless you want dedicated sensing slots. (In the current deployment this is unset, so no slot is reserved.) |

How `sensing_enabled` is derived: it is on if *either* `additional_ul_tdas` *or* `sensing_target_slots` is configured. It is the master switch for the whole sensing pipeline (§2): the tile scan checks it first (no tiles when off), the capture PUSCH only fires for scanned tiles, and the E3 publish block checks it again.

The E3 transport is configured in the separate `E3Configuration` block: the served port triplet (`setup_port`/`subscriber_port`/`publisher_port`) and which SM ids are enabled (`enabled_sms`). The emit cadence is not a conf key: each SM follows the periodicity the subscribed dApps declare in their E3 subscription (§4c). The **wire encoding is a config knob**: `encoding = "asn1" | "json"` (default `asn1`) selects the serving encoding at runtime, per the model agreed for libe3 upstream — both encoders are always compiled into the gNB and into `libe3` (built with `LIBE3_ENABLE_ASN1` + `LIBE3_ENABLE_JSON`), one is active per run. The three port keys default to `0` = the libe3 defaults (9990/9999/9991) when omitted; a config that sets them explicitly overrides those defaults (this deployment serves 7560/7562/7561).

> **Config gotcha:** an older standalone `sensing = { enabled, ul_mcs, ul_bw, target_slots, ... }` block is **dead** — the gNB never reads it (its key names don't match the keys above). Setting `enabled = 0` there does nothing. Use the `gNBs`-section keys.

---

## 9. The safety boundary — what is telemetry vs what touches the radio

This is the most important thing to keep straight when changing anything:

| Piece | Touches the live radio/scheduler? |
|-------|-----------------------------------|
| Sensing mask scan + "perfect mask" refinement (§2) | **No** — works on a copy; telemetry only. |
| E3 SMs / ranges / IQ-ref publishing (§4) | **No** — read-only export. |
| Capture PUSCH (§3) | **Yes** — it injects a (real) uplink grant, but only on slots with no real uplink, and yields to UEs. |
| PRB-block from the dApp (§5) | **Yes** — it removes PRBs from scheduling: the block is enforced uniformly over all requested PRBs (see §5). |
| `sensing_target_slots` reservation (§8) | **Yes** — it takes whole slots from the UE (off by default here). |

Anything labelled "telemetry only" can be changed freely without affecting UE service; anything labelled "Yes" must be reviewed for scheduling/throughput impact.

---

## 10. Known limitations / open items

- **Uplink throughput regression (separate from sensing):** on this deployment a real over-the-air UE gets much lower uplink throughput than it does elsewhere, with the downlink healthy. This has been isolated *away* from the sensing feature (capture PUSCH, the TDA machinery, slot reservation, and the dApp were each ruled out) and points to a uplink-reception/RF matter. It is tracked separately.
- **`sensing_enabled` is inferred, not explicit:** it does gate the whole scan→capture→publish pipeline (§2), but it is derived from the presence of `additional_ul_tdas`/`sensing_target_slots` rather than set by a dedicated on/off key. A future cleanup could add an explicit flag.
- See also `PRB_BLOCK_LIMITATIONS.md` for the actuation-side caveats.
