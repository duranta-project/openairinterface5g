<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# dApp PRB-block: how it works and known limitations

## How it works

A dApp (or an xApp, relayed through the dApp) can ask the gNB to stop using some
PRBs — for radar avoidance, spectrum sharing, etc. It sends a PRB-block command on
the Spectrum service model; the gNB stores the requested PRBs as a per-cell mask
(one bit per symbol, per PRB, for downlink and uplink) and **OR's that mask into the
scheduler's resource map at the start of every slot**. From then on every allocator
sees those PRBs as already occupied and simply schedules around them. The dApp
always sends the complete current set; the gNB mirrors it (it is not incremental),
and an empty set clears the block.

Two things sit on top of that enforcement:

- **Crash-safety.** Some channels are placed on fixed PRBs (PRACH, Msg3, SRS,
  PUCCH, SIB). When one of those lands inside the block, the gNB skips that
  reservation gracefully (with a rate-limited warning) instead of failing an
  assertion — a block that hits a dynamically-scheduled channel degrades it,
  it does not crash the gNB.

- **Collision detection for fixed per-UE signals.** PUCCH, SRS and NZP-CSI-RS sit
  on fixed PRBs assigned to each UE by RRC. The gNB keeps a small per-UE table of
  where those signals live, refreshed whenever a UE's bandwidth-part configuration
  is applied. It scans that table whenever a block is installed, and re-checks a UE
  whenever it (re)configures, and logs any signal the block now lands on. Each
  signal is checked against the block for its own direction — PUCCH and SRS
  (uplink) against the uplink block, CSI-RS (downlink) against the downlink block.
  This is **detection only** today: it tells an operator which UEs a block
  disturbs, and is the place where a future version would move the affected signal
  off the blocked PRBs (via an RRC reconfiguration) instead of just logging.

The rest of this document lists behaviors the block control surface exposes but
does **not** mitigate. Most are pre-existing gNB design choices, not faults of the
block control plane; an operator should understand them to use the API safely.

## 1. The block is enforced uniformly, and detection only logs

Every requested PRB is stamped, in both directions, for all channels — there is no
per-channel policy (no "protect this channel / route that one around the block").
A detected collision with a PUCCH/SRS resource is logged, not acted on: nothing is
relocated. Steering allocations around the block per channel, and relocating a hit
signal via RRC, are future work.

## 2. Cell-common channels are not protected

This is the most important operational limitation. SSB, CORESET0, PRACH and the
cell-common PUCCH are **not** kept clear of the block — a block over their PRBs
takes effect and degrades them. An already-connected UE tolerates a brief block,
but a block over the downlink SSB/CORESET0 region **stops a new UE from
synchronizing or attaching** (it cannot decode the broadcast / the SIB1 control
channel). Keep the dApp's mask off the cell-common PRBs, or expect the broadcast
and random-access path to degrade while such a block is active.

## 3. SRS beam management overwrites the operator PRB blacklist

`handle_nr_srs_measurements()` (`gNB_scheduler_ulsch.c`) rebuilds `cell->ulprbbl`
from scratch on every SRS beam-management report:

```c
memset(ulprbbl, 0, num_rbs * sizeof(uint16_t));
```

That is the operator's static UL blacklist, not the dApp block (which lives in
`prb_block_state_t` and is re-stamped every slot), so a dApp block is unaffected.
It does mean the two mechanisms are not interchangeable: anything written into
`ulprbbl` while `do_SRS` beam management is active is erased on the next report.
Use the dApp PRB-block control, not `ulprbbl`, for dynamic blocking.

## 4. Dedicated PUCCH / SRS under a block is gracefully dropped

If the block overlaps a UE's dedicated PUCCH (scheduling request, periodic CSI,
HARQ-ACK) or its SRS, the gNB skips that reception gracefully (a rate-limited
warning) rather than asserting, and the collision is reported by the detection
described above. That UE loses SR/CSI/SRS for as long as the block overlaps those
PRBs (buffer-status-driven uplink still flows via other grants) and recovers on the
next occasion once the block lifts. Keep the uplink mask off connected UEs'
dedicated PUCCH/SRS PRBs, or accept transient SR/CSI/SRS loss.

## 5. CSI-RS still transmits on blocked downlink PRBs

CSI-RS is configured at RRC setup on fixed PRBs and symbols, and the gNB emits it
unconditionally — the radio transmits it regardless of the scheduler's resource
map. So a downlink block over a UE's CSI-RS PRBs does not silence it: the dApp sees
CSI-RS energy there and the UE's CSI feedback is unaffected. The collision is
reported by the detection described above (CSI-RS is checked against the downlink
block), but detection does not silence it. The resource map is the MAC's allocation
tracker, not a transmission gate (the operator's static PRB blacklist had the same
property).

## 6. A prolonged block can stall a HARQ retransmission

A retransmission must reuse its original transport-block size, so it cannot shrink
to fit around a block. If a block sits continuously on the only PRBs a
retransmission can use, the scheduler keeps retrying it until the coarse inactivity
timeout drops the UE — so the block can *stall* a UE, not just throttle it, and
there is no give-up guard or log trace for this case today. Keep a sustained uplink
block off an active UE's only retransmission PRBs.

## 7. Reception is not gated by the block

The resource map controls MAC allocation, not radio reception. If a UE transmits on
a blocked uplink PRB anyway (for example PRACH from a UE that does not know about
the block), the radio still receives it. The dApp's "silence" on a blocked PRB is
best-effort against new MAC scheduling, not a hard guarantee against all activity.

## 8. Configured Grants ignore the block — by absence

The gNB does not implement Configured-Grant uplink scheduling, so this is not a
concern today. If Configured-Grant support is added, revisit this: a UE with a
configured grant would transmit on its preallocated PRBs regardless of the mask.

## 9. Very high subcarrier spacings need stamp-cost tuning

Re-stamping the whole uplink resource ring on a block change costs time
proportional to the ring size. At 30 kHz spacing (sub-6 GHz) this is a few
microseconds, far under the slot budget. At mm-wave spacings the working set is
much larger and can approach the (shorter) slot deadline, so a block change could
make a scheduler tick overrun. This deployment targets sub-6 GHz; mm-wave would need
the stamp split across several ticks.

## 10. Released PRBs return within about one ring cycle

After the block shrinks or clears, freed uplink PRBs may stay marked as occupied for
up to one uplink-ring cycle (about 10–20 ms at 30 kHz spacing) until the ring slices
are refreshed; the gNB tracks the just-released bits so collision checks and sensing
stay coherent across that window.

## Source

- `gNB_scheduler_prb_block.{c,h}` — the per-cell block state, the per-slot stamp,
  and the graceful-skip helpers.
- `periodic_alloc_registry.{c,h}` / `blocked_prbs_collision_handler.{c,h}` — the
  per-UE signal table and the collision detection.
- `openair2/E3AP/service_models/spectrum_sm/spectrum_sm.c` — where a PRB-block
  command from the dApp is received and applied.
