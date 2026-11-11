<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# Uplink spectrum sensing on the OAI gNB — design guide

This explains how the uplink spectrum-sensing feature works: what the gNB
computes, how it reaches a dApp, and how it behaves with each of the two L1
options (the OAI L1 and the NVIDIA Aerial L1). For the service model itself (the
indication path, the shared-memory ring, the file layout) see
[SPECTRUM_SM_ARCHITECTURE.md](SPECTRUM_SM_ARCHITECTURE.md).

The feature is **telemetry-out only**. A dApp can observe the RAN through it; it
cannot act on it. The one thing that does touch the scheduler is the operator's
own slot reservation (see *Slot reservation*), which is configuration, not a dApp control.

---

## The big picture

Three actors:

- **gNB MAC** — schedules the radio and, additionally, works out which
  time-frequency resources were left free in each uplink slot.
- **L1 (PHY)** — does the signal processing and produces the raw IQ samples.
  Either the OAI L1 or NVIDIA Aerial (cuBB).
- **dApp** — an external application that consumes the ranges and the IQ and
  decides which spectrum is occupied.

Once per uplink slot:

```text
              ┌──────────────────── gNB MAC ─────────────────────┐
              │  after the UL allocations of the slot are placed:  │
              │   1. SCAN     find the free PRB/symbol tiles       │
              │   2. CAPTURE  (Aerial only) make L1 produce IQ     │
              │   3. PUBLISH  ranges -> shared-memory ring, E3     │
              └───────────────────────┬───────────────────────────┘
                                      │ sensing ranges (RF=1)
   IQ (RF=2)                          ▼
   ─────────────────────────────►   dApp   (joins IQ and ranges on (sfn, slot))
```

### With the OAI L1

The gNB itself is the source of both streams, from one E3 agent: the ranges on
the Spectrum SM (RF=1) and the IQ on the L1-KPM SM (RF=2, fed by a tap in the
PHY's uplink receive path that copies `rxdataF` for every UL/MIXED slot). The
OAI L1 transforms every uplink slot regardless of whether anything is scheduled
in it, so **no capture PUSCH is needed** and none is compiled in.

On a slot reserved for sensing (see *Slot reservation*) no UE is granted uplink, so the `rxdataF`
the tap exports there is noise plus whatever else is on the channel — the signal
the dApp wants to look at.

### With the Aerial L1

cuBB only computes frequency-domain IQ for resources that a scheduled PDU points
at, so an uplink slot with nothing scheduled would give the dApp nothing to look
at. The gNB therefore injects a capture PUSCH (see *The capture PUSCH*). The IQ is then supplied by
cuBB's own E3 agent (RF=2) and the ranges by the gNB's (RF=1); the dApp lines
them up by `(sfn, slot)`. RF ids are per agent, so RF=2 means "the IQ KPM SM" on
both.

### Any L1

The design does not depend on a particular L1. What the MAC does — reserve the
slot, scan the free tiles, publish the ranges (see *The sensing scan*, *Slot reservation*, *E3 telemetry transport*) — works on its own
scheduling state and the RRC configuration, so it is identical whichever L1 sits
below. The L1 only matters for the IQ:

- **OAI L1, any radio path.** The IQ tap reads `rxdataF` in the gNB's uplink
  receive function, after the radio path has filled it. USRP, the RF simulator
  and the O-RAN 7.2 split (`fhi_72`, where the RU delivers frequency-domain IQ
  over the fronthaul) all fill the same buffer before that point, so the tap and
  the RF=2 stream see the same thing on each. On a slot reserved for sensing
  that is what the channel carried with no UE scheduled. It has been run on the
  RF simulator; the 7.2 and USRP paths follow from where the tap sits and have not
  been exercised with sensing.
- **Aerial (cuBB).** IQ exists only for scheduled PDUs, hence the capture PUSCH
  (see *The capture PUSCH*). This is the one L1-specific piece and it is compiled only for Aerial.
- **Any other L1** that can hand the dApp its uplink IQ, tagged with `(sfn, slot)`
  (for instance by its own E3 agent, as cuBB does), fits without change: the
  ranges carry the same stamp and the dApp joins the two streams on it.

---

## The sensing scan — a map of free spectrum

**What it is.** Every uplink (or mixed) slot, the gNB builds a list of *sensing
tiles*: rectangles in the time-frequency grid `(symbol) × (start PRB, number of
PRBs)` that no scheduled transmission uses. Each tile is one `sensing_range_t`,
with the PRB index absolute in the carrier so it lines up directly with the L1 IQ
tap.

**How it works.** After the scheduler has placed all of the slot's uplink
allocations, a scan reads the slot's resource-block map and emits one tile per
contiguous free run at each free symbol. It works on a **local copy** of the map,
so it cannot influence any scheduling decision.

**The control-channel refinement.** A plain free-region scan would report
band-edge resources as free even though UEs transmit control there autonomously
(PUCCH for SR/CSI/HARQ-ACK, SRS, PRACH). On a reserved slot those reservations
are not in the map. A helper therefore derives the uplink control occupancy from
the RRC configuration — including for RRC-connected UEs that are currently
inactive, which still transmit those resources — and from the receptions the
scheduler has already committed, and unions it into the local copy before the
tiles are emitted. A tile is only emitted if it is genuinely free.

**What controls it.** Sensing is enabled for a cell iff `sensing_target_slots`
is configured (see *Configuration reference*). When it is not, the scan, the capture PUSCH and the publish
are all inert.

---

## Slot reservation

The operator names uplink slots, as indices within the TDD period, in
`sensing_target_slots`. On those slots the MAC stamps the whole uplink resource
map as occupied before any UE allocator runs, so no UE is granted uplink there.
Just before the scan the slot is restored to its static state, so the scanner
sees a clean full-PRB window. **This is the only piece that takes resources from
UEs: each reserved slot is lost to them.** Leave it empty unless dedicated
sensing slots are wanted.

---

## The capture PUSCH (Aerial only)

**What it does.** On a free uplink slot the gNB injects one dummy PUSCH addressed
to a reserved identity (`SENSING_RNTI`). It carries no data and is expected to
fail its CRC; its only purpose is to make cuBB run its uplink receive chain and
fill the IQ buffer for the slot.

- **It yields to real traffic.** It is injected only when no real
  PUSCH/PUCCH/SRS is scheduled in the slot.
- **One per slot.** A single PDU is enough to trigger cuBB's slot-level capture.
- **Transport-block size.** cuBB recomputes the transport-block size from the
  PUSCH parameters and rejects the PDU if the gNB's value differs. The DMRS
  overhead is therefore sized the way the normal uplink scheduler sizes it, so the
  two agree for multi-layer captures.
- **Compiled in only for the Aerial build** (`ENABLE_AERIAL`).
- **Its shape is operator-configured** (PRB window, MCS, layers, beams) with the
  `sensing_pusch_*` keys (see *Configuration reference*).

---

## E3 telemetry transport

The gNB runs an E3 agent (a thin adapter over the external `libe3`, which owns
the transport and threads) and a set of service models. Each is one topic a dApp
can subscribe to, identified by a RAN-function id:

| RF id | Service model | Carries |
|------:|---------------|---------|
| 1 | **Spectrum SM** | the sensing ranges (the tiles from *The sensing scan*) |
| 2 | **L1-KPM SM** | PHY IQ metadata, from the OAI PHY tap |

### Spectrum SM (RF=1)

It sends *indications* carrying `(sfn, slot, beam, timestamp)` plus a **reference**
to the slot's ranges, not the ranges themselves:

- Every sensing publish writes the ranges into a shared-memory ring
  (`/e3_l2_sensing`), regardless of the emission period; the period throttles only
  the indications.
- The dApp memory-maps the ring read-only and reads the ranges directly. The
  message stays small however many tiles there are, and the `(sfn, slot)` stamped
  in both the message and the ring slot lets the dApp detect and discard an
  overwritten read.

The wire encoding — ASN.1, JSON or Protocol Buffers — is selected at runtime with
`E3Configuration.encoding`. All carry the ranges by reference.

### The shared SM worker

Both telemetry emitters use one background-thread driver that handles the
period/on-data wait loop, the lifecycle hooks and the per-dApp fan-out; each SM
supplies only its differences through a small table of callbacks. The cadence
follows the periodicity the subscribed dApps declare (the fastest request wins):

- **on-data** (period 0/unset): one indication per publish, i.e. one per UL slot.
- **periodic** (period > 0, microseconds): one per interval, re-using the latest
  snapshot.

---

## Configuration reference

Everything E3 related, sensing included, is in the **`E3Configuration`** section.
`targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.sa.band78.106prb.rfsim.e3.conf` is a complete example.

| Key | What it does |
|-----|--------------|
| `sensing_target_slots` | Slots (indices within the TDD period) to hard-reserve for sensing (see *Slot reservation*). Empty or absent: sensing is off. Each index must be smaller than the TDD period, otherwise startup aborts. |
| `sensing_pusch_mcs` (default 9) | MCS index (table 0) of the capture PUSCH. |
| `sensing_pusch_rb_size` (1) / `sensing_pusch_rb_start` (0) | PRB window of the capture PUSCH, BWP-relative. |
| `sensing_pusch_nrOfLayers` (1) | Layers of the capture PUSCH; drives the digital-beamforming fan-out, not spatial multiplexing. |
| `sensing_pusch_beams` (beam 0) | Beam indices the capture PUSCH is fanned out to. |

The `sensing_pusch_*` keys are read by every build but only used with the Aerial
L1 (see *The capture PUSCH*). The section is read once; it is a singleton, so it applies to every cell
the gNB serves, and the ranges are currently published for the first cell (and
beam 0).

The transport keys of the same section are `link`, `transport`, `encoding`, the
port triplet `setup_port` / `subscriber_port` / `publisher_port` (0 = the libe3
default), and `enabled_sms`.

---

## The safety boundary — telemetry vs what touches the radio

| Piece | Touches the scheduler / radio? |
|-------|--------------------------------|
| The scan and the control-channel refinement | **No** — works on a copy; telemetry only. |
| The E3 service models, ranges and IQ references (see *E3 telemetry transport*) | **No** — read-only export. |
| The capture PUSCH (Aerial only) | **Yes** — injects an uplink grant, but only on slots with no real uplink. |
| `sensing_target_slots` reservation (see *Slot reservation*) | **Yes** — takes whole slots from UEs. |

Anything marked "No" can be changed freely without affecting UE service;
anything marked "Yes" needs a scheduling/throughput review.

---

## Scope and limitations

- **Uplink only.** Sensing looks at what the gNB *receives*. The downlink is not
  sensed and there is no plan to: in a downlink slot the gNB is the transmitter,
  so observing the channel there would need a separate receiver that a gNB does
  not have.
- **TDD only.** `sensing_target_slots` indexes slots within the TDD period, and
  reservation works by stamping specific slots. Under FDD every slot is uplink and
  downlink at once, so there is no slot to reserve without taking uplink from
  UEs continuously, and a different scheme would be needed. If the option is set
  on an FDD cell the gNB logs a warning and leaves sensing off.
- **One cell, one beam.** The ranges are published for the first cell and beam 0.
- **Ranges are per slot**, from the resource map and the RRC configuration; the
  IQ is the source of truth for what is actually on the air.
