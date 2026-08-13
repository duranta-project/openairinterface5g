# gNB MAC Scheduler Architecture

## Overview

The gNB MAC scheduler runs once per DL slot. It allocates PDSCH resources for the
current slot and PUSCH resources for a future UL slot (selected via K2), sending the
UL DCI grant in the current DL slot. The DL and UL schedulers follow the same pipeline
design: a fixed orchestration loop calls a sequence of **pluggable function pointers**,
each handling one stage of the scheduling decision. Any stage can be replaced
independently without touching the rest of the pipeline.

Both schedulers send their DCIs in the same DL slot and share the same physical
CORESET/CCE resources. The UL scheduler runs first (`nr_schedule_ulsch` in
`gNB_scheduler.c`), then the DL scheduler (`nr_schedule_ue_spec`). CCEs are claimed on
a first-come basis via `get_cce_index` + `fill_pdcch_vrb_map`, so UL DCIs take
priority. If CCE space is tight, DL UEs are more likely to be skipped. The two
schedulers use separate FAPI containers (`UL_dci_req` for UL grants, `DL_req` for DL
grants) but the underlying CCE map is shared.

All scheduler code lives under `openair2/LAYER2/NR_MAC_gNB/`. The DL scheduler is in
[`gNB_scheduler_dlsch.c`](../../openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_dlsch.c),
the UL scheduler in
[`gNB_scheduler_ulsch.c`](../../openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_ulsch.c),
and shared primitives in
[`gNB_scheduler_primitives.c`](../../openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_primitives.c).
Data structures and typedefs are in
[`nr_mac_gNB.h`](../../openair2/LAYER2/NR_MAC_gNB/nr_mac_gNB.h).
Default policy implementations are in
[`gNB_scheduler_dlsch_default_policies.c`](../../openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_dlsch_default_policies.c)
and
[`gNB_scheduler_ulsch_default_policies.c`](../../openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_ulsch_default_policies.c).
Named policy and observer registries are declared in
[`nr_sched_registries.h`](../../openair2/LAYER2/NR_MAC_gNB/nr_sched_registries.h).

## Selecting scheduler policies

Every existing scheduler function pointer can be selected by name in the
`MACRLCs` section. All selectors default to `"default"`, so configurations that
omit them retain the current scheduler behavior:

```ini
MACRLCs = (
  {
    dl_preprocessor_policy = "default";
    dl_ri_pmi_select_policy = "default";
    dl_tda_select_policy = "default";
    dl_beam_select_policy = "default";
    dl_mcs_select_policy = "default";
    dl_rb_alloc_policy = "default";
    dl_lcid_alloc_policy = "default";

    ul_preprocessor_policy = "default";
    ul_ri_tpmi_select_policy = "default";
    ul_tda_select_policy = "default";
    ul_beam_select_policy = "default";
    ul_mcs_select_policy = "default";
    ul_rb_alloc_policy = "default";

    dl_harq_result_observers = ();
    ul_harq_result_observers = ();
  }
);
```

DL and UL HARQ result observers are separate lists, execute in configuration
order, and receive ACK, NACK, DTX, and scheduler-missing results for their
direction. Each callback also receives `mac->sched_stateful_data`, the persistent
context shared with the configured scheduler policies. Observer callbacks run
synchronously under the MAC scheduler lock and therefore must not block.

Built-in implementations register themselves with `SCHED_REGISTRY_ADD`. The
`--phy-test` mode continues to override both configured preprocessors with the
registered `phytest` implementations.

## Scheduler Pipeline

Both DL and UL follow the same 7-stage pipeline, plus a DL-only 8th stage.

```
                         DL                              UL
                    ┌────────────┐                  ┌────────────┐
                    │  1.Collect │                  │  1.Collect │  ◄── nr_ulsch_preprocessor
                    │ Candidates │                  │ Candidates │      (iterates over
                    └─────┬──────┘                  └─────┬──────┘       reachable UL slots)
                          │                               │
                    ┌─────┴──────┐                  ┌─────┴──────┐
                    │  2. RI/PMI │                  │  2. RI/TPMI│  ◄─┐
                    │ Selection  │                  │ Selection  │    │
                    └─────┬──────┘                  └─────┬──────┘    │
                          │                               │           │
                    ┌─────┴──────┐                  ┌─────┴──────┐    │
                    │  3. Beam   │                  │  3. Beam   │    │
                    │ Selection  │                  │ Selection  │    │
                    └─────┬──────┘                  └─────┬──────┘    │
                          │                               │           │ nr_ul_schedule
                    ┌─────┴──────┐                  ┌─────┴──────┐    │ (per UL slot)
                    │  4. TDA    │                  │  4. TDA    │    │
                    │ Selection  │                  │ Selection  │    │
                    └─────┬──────┘                  └─────┬──────┘    │
                          │                               │           │
                    ┌─────┴──────┐                  ┌─────┴──────┐    │
                    │  5. MCS    │                  │  5. MCS    │    │
                    │ Selection  │                  │ Selection  │    │
                    └─────┬──────┘                  └─────┬──────┘    │
                          │                               │           │
                    ┌─────┴──────┐                  ┌─────┴──────┐    │
                    │  6. RB     │                  │  6. RB     │    │
                    │ Allocation │                  │ Allocation │    │
                    │ (per beam) │                  │ (per beam) │    │
                    └─────┬──────┘                  └─────┬──────┘    │
                          │                               │           │
                    ┌─────┴──────┐                  ┌─────┴──────┐    │
                    │  7.Dispatch│                  │  7.Dispatch│  ◄─┘
                    │  (fixed)   │                  │  (fixed)   │
                    └─────┬──────┘                  └────────────┘
                          │
                    ┌─────┴──────┐
                    │  8. LCID   │  (DL only)
                    │ Allocation │
                    └────────────┘
```

### UL preprocessor split

The UL pipeline is split across two functions. The preprocessor
(`nr_ulsch_preprocessor`) runs a loop over **multiple candidate UL slots**: for each DL
slot there may be several reachable UL slots (depending on K2), and the scheduler must
pick one. The loop iterates until the DCI budget is exhausted or no TDA can reach
further UL slots.

**Only step 1 (candidate collection) lives in the preprocessor loop.** Candidates depend
on `sched_frame`/`sched_slot` (e.g. inactivity checks), so they are rebuilt each
iteration. The preprocessor also runs a lightweight TDA reachability gate
(`get_num_ul_tda == 0 → break`) to short-circuit the loop when no TDA can reach the
target slot.

**Steps 2–7 live in `nr_ul_schedule`**, called once per UL slot with the collected
candidates. `k2` is passed as a parameter for TDA selection. This mirrors the DL
structure where all stages run in a single function.

Beam selection (step 3) runs before TDA selection (step 4) so that TDA selection can
use the allocated beam to check the correct VRB map and pick the best TDA per beam.

The preprocessor itself is a pluggable function pointer (`pre_processor_ul`), so a
custom implementation can replace the entire outer strategy — including how many UL
slots to target and what to do when the DCI budget is limited.

### Candidate struct

Each candidate is an `nr_{dl,ul}_candidate_t` struct that flows through the pipeline,
accumulating decisions. Its fields are split into three sections:

1. **Identity / scheduling state** — set by collect, read-only afterwards: `UE` pointer,
   `rnti`, HARQ state, pending bytes, BLER estimate, current MCS, BWP geometry, QoS.
2. **CSI observations** — set by collect: `cqi` (DL), `beam_rsrp[]`, `beam_sinr[]` (per-SSB).
3. **gNB decisions** — written by the stage indicated below. Some live in the
   embedded `sched_pdsch` / `sched_pusch` sub-struct, others are `alloc_`-prefixed
   fields on the candidate itself:
   - `sched_p{d,u}sch.nrOfLayers`, `.pm_index` (DL) / `.tpmi` (UL) — set by ri_pmi/tpmi_select
   - `sched_p{d,u}sch.time_domain_allocation`, `.tda_info`, `alloc_slbitmap` — set by tda_select
   - `alloc_beam_idx`, `alloc_dci_beam_idx` (UL), etc. — set by beam_select
   - `sched_p{d,u}sch.mcs` — set by mcs_select
   - `sched_p{d,u}sch.rbStart`, `.rbSize` — set by rb_alloc
   - `alloc_cce_index`, `alloc_aggregation_level`, `alloc_sched_pdcch` — set by commit_alloc

The policy (`rb_alloc`) writes to `sched_p{d,u}sch.rbStart/rbSize/mcs` via the
`COMMIT_ALLOC` / `COMMIT_UL_ALLOC` macro, which also runs CCE validation and sets
`cand->scheduled = true`. Only candidates with that flag set are dispatched.

---

### Stage 1. Candidate Collection (fixed)

**Functions:** `collect_dl_candidates()`, `collect_ul_candidates()`

Iterates over connected UEs and builds an array of `nr_{dl,ul}_candidate_t` structs.
Populates the identity/state and CSI sections. Not a function pointer — fixed scaffolding
that feeds the pipeline.

### Stage 2. RI / PMI Selection

**Function pointers:** `mac->dl_ri_pmi_select` (`nr_dl_ri_pmi_select_fn`),
`mac->ul_ri_tpmi_select` (`nr_ul_ri_tpmi_select_fn`)

Sets rank and precoder index per candidate. In DL, rank and PMI come from the UE's CSI
report (RI + PMI). In UL, the gNB has full authority and reads rank and TPMI from SRS
feedback.

**DL outputs:** `cand->sched_pdsch.nrOfLayers`, `cand->sched_pdsch.pm_index`.
**UL outputs:** `cand->sched_pusch.nrOfLayers`, `cand->sched_pusch.tpmi`.

### Stage 3. Beam Selection

**Function pointers:** `mac->dl_beam_select` (`nr_dl_beam_select_fn`),
`mac->ul_beam_select` (`nr_ul_beam_select_fn`)

Assigns a beam structure index to each candidate. For UL, two beams are needed — one
for the DCI slot, one for the PUSCH slot — since they may fall in different beam periods.
Candidates that fail beam allocation (no beam available) are marked with `skipped=true`.

Placed before TDA selection so that TDA selection can use the allocated beam to check
the correct VRB map and pick the best TDA per beam.

**Outputs:** `cand->alloc_beam_idx` (PUSCH/PDSCH beam), `cand->alloc_dci_beam_idx` (UL DCI beam).

### Stage 4. TDA Selection

**Function pointers:** `mac->dl_tda_select` (`nr_dl_tda_select_fn`),
`mac->ul_tda_select` (`nr_ul_tda_select_fn`)

Picks the Time Domain Allocation for each candidate. For UL retransmissions, also
validates feasibility: if the TDA differs from the original transmission (e.g. because
targeting a different UL slot), the TBS must be preservable with the new symbol/DMRS
layout. Infeasible retx candidates are marked with `skipped=true`.

**Outputs:** `cand->sched_p{d,u}sch.time_domain_allocation`, `.tda_info`, `cand->alloc_slbitmap`. UL retx
candidates get `cand->retx_rbSize` set.
**Returns:** number of surviving candidates.

### Stage 5. MCS Selection

**Function pointers:** `mac->dl_mcs_select` (`nr_dl_mcs_select_fn`),
`mac->ul_mcs_select` (`nr_ul_mcs_select_fn`)

Runs for **all surviving candidates** — including those that may ultimately not be
scheduled due to CCE failure. Sets `sched_p{d,u}sch.mcs` from BLER stats or SINR, and persists
the decision to `{ul,dl}_bler_stats.mcs` so continuity is maintained across slots even
for unscheduled UEs.

Placed after beam selection so custom implementations can use `alloc_beam_idx` and
`beam_rsrp/sinr` for beam-aware MCS adaptation.

### Stage 6. RB Allocation (per beam)

**Function pointers:** `mac->dl_rb_alloc` (`nr_dl_rb_alloc_fn`),
`mac->ul_rb_alloc` (`nr_ul_rb_alloc_fn`)

The core resource allocation decision and the most likely stage to be replaced. Called
once per beam with candidates filtered to that beam. Decides which UEs to schedule and
how many PRBs each gets. MCS is already set on `sched_p{d,u}sch.mcs` by the previous
stage; the policy may refine it (e.g. downward PHR adjustment).

Each allocation must go through the `COMMIT_ALLOC` / `COMMIT_UL_ALLOC` macro, which
writes `sched_p{d,u}sch.rbStart/rbSize/mcs` onto the candidate, runs CCE validation
(and PUCCH validation on DL), and sets `cand->scheduled = true`. UEs that fail CCE are
skipped transparently.

**Outputs:** `cand->scheduled` flag on accepted candidates.
**Returns:** number of scheduled UEs.

### Stage 7. Dispatch (fixed)

Not a function pointer. Iterates over candidates with `scheduled == true`, reads the `alloc_*` fields
from each candidate, applies CCE results to `sched_ctrl`, marks the VRB map, and calls
`apply_{dl,ul}_{new_transmission,retransmission}` to build the final
`NR_sched_{pusch,pdsch}_t` and trigger FAPI message preparation.

### Stage 8. Per-LCID Byte Allocation (DL only)

**Function pointer:** `mac->dl_lcid_alloc` (`nr_dl_lcid_alloc_fn`)

Called inside `generate_dl_mac_pdu` for initial DL transmissions. Decides how many bytes
each logical channel gets within the available TBS. The candidate flows from the dispatch
stage into `post_process_dlsch` → `generate_dl_mac_pdu`, giving this stage access to the
full candidate context (per-LCID pending bytes, QoS, priority).

The function pointer receives the MAC instance, the candidate, the TBS available after
MAC CEs, and writes an output array `lcid_alloc[NR_MAX_NUM_LCID]` specifying the byte
budget per LCID. The MAC PDU builder then executes the plan: it iterates over LCs in
config order, requests up to `lcid_alloc[lcid]` bytes from RLC for each, and builds the
MAC subheaders. RLC may return fewer bytes than budgeted; the builder handles this
transparently.

**Inputs:** `candidate->pending_bytes_per_lcid[]`, `candidate->fiveQI`, `candidate->priority`,
`candidate->nssai`, `tbs_available`.
**Outputs:** `lcid_alloc[NR_MAX_NUM_LCID]` — byte budget per LCID.

Not invoked for retransmissions (which reuse the stored transport block) or for phy-test
mode (which fills random data). In these cases the candidate pointer is NULL.

---

## Default Implementations

All function pointers are assigned in
[`main.c`](../../openair2/LAYER2/NR_MAC_gNB/main.c) at startup.
The `COMMIT_ALLOC` / `COMMIT_UL_ALLOC` macros are defined in
[`mac_proto.h`](../../openair2/LAYER2/NR_MAC_gNB/mac_proto.h).

### `nr_dl_ri_pmi_select_default`, `nr_ul_ri_tpmi_select_default`

DL reads rank and PMI from the UE's CSI report (`csi_ri`, `csi_pm_index` on
`sched_ctrl`). Retransmissions reuse rank/PMI from the original HARQ process.
UL reads rank and TPMI from SRS feedback (`srs_feedback` on `sched_ctrl`).
Both write `cand->sched_p{d,u}sch.nrOfLayers` and the respective precoder index.

### `nr_dl_tda_select_default`, `nr_ul_tda_select_default`

Assigns the same TDA to all candidates in a slot (picks the first valid one for the
scheduled frame/slot). For UL retransmissions, calls `check_ul_retx_feasibility` to
verify TBS preservation under the new symbol/DMRS layout.

### `nr_dl_beam_select_default`, `nr_ul_beam_select_default`

Uses OAI's beam management framework (`NR_beam_info_t`). For UL, allocates both a DCI
beam and a PUSCH beam. Candidates that fail beam allocation are compacted out.

### `nr_dl_mcs_select_default`, `nr_ul_mcs_select_default`

BLER-based (when `harq_round_max > 1`): calls `nr_adapt_mcs_from_bler` for candidates
with a fresh BLER estimate; holds current MCS otherwise. SINR-based (when
`harq_round_max == 1`): maps measured PUSCH SINR to MCS via lookup table. In both
cases, persists to `{dl,ul}_bler_stats.mcs`.

### `nr_dl_proportional_fair`, `nr_ul_proportional_fair`

Proportional-fair scheduler with three phases:

- **Phase 1 — Retransmissions:** find the largest free block >= `retx_rbSize`.
- **Phase 2 — Minimal-grant UEs:** in DL, targets UEs with no pending RLC data
  (`pending_bytes == 0`) that still need a TA command or beam-switch MAC CE. In UL,
  targets inactive UEs (`sched_inactive`) that need scheduling for TA/SR. Both get a
  minimum grant (`min_rb`).
- **Phase 3 — New data:** sort by PF weight (`pending_bytes / avg_throughput`), allocate
  the largest free block to each UE in order.

The UL policy also checks PHR (Power Headroom) and adjusts MCS/RBs accordingly.
All phases use `COMMIT_ALLOC` / `COMMIT_UL_ALLOC` to validate CCE (and PUCCH on DL).

### `nr_dl_lcid_alloc_default`

Greedy fill: sets each LCID's budget to its full `pending_bytes_per_lcid` value. The
execution loop in `generate_dl_mac_pdu` then fills LCIDs in LC config order until the
TBS is exhausted, reproducing the original first-come-first-served behavior. A custom
implementation could use the candidate's QoS fields (`fiveQI`, `priority`, `nssai`)
to implement weighted fair queuing, strict priority with rate limiting, or slice-aware
allocation across LCIDs.

### `nr_dlsch_preprocessor`, `nr_ulsch_preprocessor`

Top-level orchestrators that run the full pipeline above. In phy-test mode, replaced by
`nr_preprocessor_phytest` / `nr_ul_preprocessor_phytest` which bypass the staged pipeline.

---

## Function Pointer Reference

### DL

| Field on `gNB_MAC_INST`  | Typedef                   | Default                       | Stage |
|--------------------------|---------------------------|-------------------------------|-------|
| `pre_processor_dl`       | `nr_pp_impl_dl`           | `nr_dlsch_preprocessor`       | Orchestrator |
| `dl_ri_pmi_select`       | `nr_dl_ri_pmi_select_fn`  | `nr_dl_ri_pmi_select_default` | 2. RI/PMI |
| `dl_beam_select`         | `nr_dl_beam_select_fn`    | `nr_dl_beam_select_default`   | 3. Beam |
| `dl_tda_select`          | `nr_dl_tda_select_fn`     | `nr_dl_tda_select_default`    | 4. TDA |
| `dl_mcs_select`          | `nr_dl_mcs_select_fn`     | `nr_dl_mcs_select_default`    | 5. MCS |
| `dl_rb_alloc`            | `nr_dl_rb_alloc_fn`       | `nr_dl_proportional_fair`     | 6. RB alloc |
| `dl_lcid_alloc`          | `nr_dl_lcid_alloc_fn`     | `nr_dl_lcid_alloc_default`    | 8. LCID alloc |

### UL

| Field on `gNB_MAC_INST`  | Typedef                   | Default                        | Stage |
|--------------------------|---------------------------|--------------------------------|-------|
| `pre_processor_ul`       | `nr_pp_impl_ul`           | `nr_ulsch_preprocessor`        | Orchestrator |
| `ul_ri_tpmi_select`      | `nr_ul_ri_tpmi_select_fn` | `nr_ul_ri_tpmi_select_default` | 2. RI/TPMI |
| `ul_beam_select`         | `nr_ul_beam_select_fn`    | `nr_ul_beam_select_default`    | 3. Beam |
| `ul_tda_select`          | `nr_ul_tda_select_fn`     | `nr_ul_tda_select_default`     | 4. TDA |
| `ul_mcs_select`          | `nr_ul_mcs_select_fn`     | `nr_ul_mcs_select_default`     | 5. MCS |
| `ul_rb_alloc`            | `nr_ul_rb_alloc_fn`       | `nr_ul_proportional_fair`      | 6. RB alloc |

---

## Key Data Structures

All defined in `nr_mac_gNB.h`.

### Candidate (`nr_{dl,ul}_candidate_t`)

Flows through the pipeline accumulating decisions. Three sections:

**Identity / scheduling state** (set by collect, read-only after):
`UE` pointer, `rnti`, `is_retx`, `retx_harq_pid`, `retx_rbSize`, `pending_bytes`,
`pending_bytes_per_lcid[NR_MAX_NUM_LCID]`, `avg_throughput`, `bler`, `current_mcs`,
`max_mcs`, `bwp_start`, `bwp_size`, `fiveQI`, `priority`, `nssai`.

**CSI observations** (set by collect, read-only after):
`cqi` (DL CQI), `beam_rsrp[]`, `beam_sinr[]` (per-SSB L1-RSRP/SINR, `INT16_MIN` = no data).

**gNB decisions** (written by the named stage — some in `sched_p{d,u}sch`, some `alloc_*`):
`sched_p{d,u}sch.nrOfLayers`, `.pm_index`/`.tpmi`, `.time_domain_allocation`, `.tda_info`,
`.mcs`, `.rbStart`, `.rbSize`;
`alloc_slbitmap`, `alloc_beam_idx`, `alloc_dci_beam_idx` (UL),
`alloc_cce_index`, `alloc_aggregation_level`, `alloc_sched_pdcch`.

### Sched Params (`nr_{dl,ul}_sched_params_t`)

Per-beam context passed to `rb_alloc`. Contains the MAC instance, slot info, VRB map,
available RBs, BLER thresholds, TDA info, and serving cell config.

---

## Writing a Custom Scheduler

### Replacing the RB allocation policy

Write a function matching the typedef:

```c
int my_ul_rb_alloc(const nr_ul_sched_params_t *params,
                   nr_ul_candidate_t *candidates,
                   int n_candidates)
{
    int n_scheduled = 0;
    for (int i = 0; i < n_candidates; i++) {
        nr_ul_candidate_t *cand = &candidates[i];
        // ... decide rbStart, rbSize, mcs ...
        // COMMIT_UL_ALLOC writes sched_pusch fields, validates CCE, sets cand->scheduled:
        COMMIT_UL_ALLOC(params, cand, rbStart, rbSize, mcs, n_scheduled);
    }
    return n_scheduled;
}
```

Register it in `openair2/LAYER2/NR_MAC_gNB/main.c`:

```c
RC.nrmac[i]->ul_rb_alloc = my_ul_rb_alloc;
```

### Replacing the per-LCID allocation policy (DL)

Write a function matching the typedef:

```c
void my_lcid_alloc(const gNB_MAC_INST *mac,
                   const nr_dl_candidate_t *candidate,
                   int tbs_available,
                   int lcid_alloc[NR_MAX_NUM_LCID])
{
    memset(lcid_alloc, 0, NR_MAX_NUM_LCID * sizeof(int));
    // ... use candidate->pending_bytes_per_lcid[], candidate->fiveQI,
    //     candidate->priority, tbs_available to decide budgets ...
    for (int lcid = 0; lcid < NR_MAX_NUM_LCID; lcid++)
        lcid_alloc[lcid] = my_budget_for(lcid);
}
```

Register it:

```c
RC.nrmac[i]->dl_lcid_alloc = my_lcid_alloc;
```

### Replacing other stages

Any function pointer can be replaced independently:

```c
RC.nrmac[i]->ul_ri_tpmi_select = my_ri_tpmi_select;
RC.nrmac[i]->ul_mcs_select     = my_mcs_select;
RC.nrmac[i]->ul_beam_select    = my_beam_select;
```

### Recommendations

- **Always use `COMMIT_ALLOC` / `COMMIT_UL_ALLOC`** to accept an allocation. It writes
  `sched_p{d,u}sch` fields onto the candidate, validates CCE (and PUCCH on DL), and sets
  `cand->scheduled`.
- **Do not mark the VRB map** — `commit_alloc` / `commit_ul_alloc` handles that inside the macro.
- `sched_p{d,u}sch.mcs` is already set by `mcs_select` when your policy runs. You may
  refine it downward (e.g. PHR); pass the final value to `COMMIT_ALLOC` / `COMMIT_UL_ALLOC`.
- Candidates are non-const: the policy is allowed to write `sched_p{d,u}sch` and `alloc_*`
  fields. Do not modify identity/state or CSI fields.
