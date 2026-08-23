# Network Slicing (3GPP-oriented MAC Implementation)

This document describes the **frequency-domain network slicing** feature in the OAI
gNB MAC scheduler: how it maps to 3GPP concepts, how it is configured, and where
the code lives.

For the general modular scheduler pipeline (collect → RI/PMI → beam → TDA → MCS →
RB alloc), see [MAC/scheduler-architecture.md](MAC/scheduler-architecture.md).

For end-to-end lab bring-up (Open5GS, rfsim, FlexRIC xApp), see the workspace
[`nws/scripts/readme.md`](../../nws/scripts/readme.md).

---

## Scope

| In scope | Out of scope (this feature) |
|----------|-----------------------------|
| Per-slice **PRB range** allocation on DL/UL | Per-slice CORESET / separate PDCCH search spaces |
| S-NSSAI-based UE-to-slice mapping (SST + SD) | RAN slicing at L1 beam / cell level |
| `dedicated` / `min` / `max` PRB ratio policy | CN slice selection (NSSF) — handled by core |
| Per-direction scheduler (`dl_scheduler_type` / `ul_scheduler_type`) | |
| Intra-slice UE scheduling via modular `nr_*_proportional_fair` | |
| E2 Slice SM read/write of NS policy (RAN func) | |

Isolation is **frequency-domain**: each slice gets a contiguous PRB window for the
slot; UEs are scheduled inside that window with proportional-fair logic.

---

## 3GPP mapping

### S-NSSAI

Slices are identified by **SST** (8-bit Slice/Service Type) and **SD** (24-bit Slice
Differentiator), matching 5G S-NSSAI:

```c
typedef struct {
  uint32_t sst : 8;
  uint32_t sd  : 24;
} slice_nssai_t;
```

UEs are associated with a slice via:

- DRB `nssai` on logical channel config (from RRC / NGAP), and
- `nr_mac_get_ue_effective_nssai()` for scheduling decisions.

The default slice `SST=1, SD=0xffffff` is reserved and must not be used as a
data-slice target in E2 SET requests.

### RAN resource model (PRB ratios)

Each slice policy uses three ratios (0.0–1.0), with:

`dedicated_prb_ratio ≤ min_prb_ratio ≤ max_prb_ratio`

| Ratio | Meaning |
|-------|---------|
| **dedicated** | Non-shareable guarantee; always reserved (Pass 1) |
| **min** | Minimum when the slice has traffic; prioritized shareable part = `min − dedicated` (Pass 2) |
| **max** | Hard upper cap; shareable pool above min = `max − min` (Pass 3) |

Algorithm details: [`slice_prb_allocator/README.md`](../openair2/LAYER2/NR_MAC_gNB/slice_prb_allocator/README.md).

### CORESET / PDCCH

CORESET and CCE resources are **cell-wide and shared** across slices. NWS lab configs
tune `coreset_duration` and `uess_agg_levels` so multiple UEs (across slices) can
obtain DL/UL DCIs in the same slot. This is a **deployment requirement** for
multi-slice experiments, not per-slice CORESET partitioning.

Typical slice experiment settings (106 PRB):

```yaml
uess_agg_levels: [8, 8, 8, 4, 2]   # sized for ~34-CCE CORESET
coreset_duration: 2                 # 2 OFDM symbols; use for all slice experiments
```

`coreset_duration` is parsed in `gnb_config.c` → `fix_scc()` and stored in
`nr_mac_config_t.coreset_duration`.

---

## Scheduler types

```c
typedef enum {
  SCHE_PF = 0,  /* Proportional Fair (default) */
  SCHE_NS = 1   /* Network Slicing (frequency-domain) */
} scheduler_type_t;
```

DL and UL are configured **independently**:

```yaml
MACRLCs:
  - dl_scheduler_type: 0   # 0 = SCHE_PF, 1 = SCHE_NS
    ul_scheduler_type: 1
```

Common lab modes (see `nws/scripts/readme.md`):

| Mode | DL | UL | Notes |
|------|----|----|-------|
| `NSUL` | PF | NS | Stable default in many NWS configs |
| `NSDL` | NS | PF | DL NS needs shared `remainUEs` across slices |
| `NSBOTH` | NS | NS | Both directions sliced |
| `PF` | PF | PF | No RAN slicing; slices in YAML ignored for scheduler |

---

## Architecture

NS scheduling is a **two-layer** design: a slice PRB allocator runs first (frequency
isolation), then the standard modular UE scheduler runs **once per slice** inside the
allocated window (intra-slice proportional fair).

### Layer model

```
┌─────────────────────────────────────────────────────────────────────────┐
│  Layer 0 — MAC entry (per DL slot / per reachable UL slot)              │
│  nr_schedule_ue_spec()          nr_schedule_ulsch()                     │
│       │                                │                                │
│       ▼                                ▼                                │
│  pre_processor_dl                pre_processor_ul                       │
│  nr_dlsch_preprocessor           nr_ulsch_preprocessor                  │
│  (PF vs NS via scheduler_type_*) (PF vs NS via scheduler_type_*)        │
└───────────────────────────────┬─────────────────────────────────────────┘
                                │
        ┌───────────────────────┴───────────────────────┐
        │ SCHE_PF                                         │ SCHE_NS
        ▼                                                 ▼
  nr_*_schedule()                              nr_*_schedule_ns()
  (full BWP)                                   │
                                               ├─ per beam ─────────────────┐
                                               │                            │
                                               ▼                            ▼
                                    slice_prb_allocator          candidate filter (UL)
                                    (required_prbs → ranges)     or UE list (DL)
                                               │                            │
                                               └──────────┬─────────────────┘
                                                          ▼
                                               nr_*_schedule(slice_prb, remainUEs)
                                               DL: collect inside schedule
                                               UL: filter pre-collected candidates
                                               → … → nr_*_proportional_fair
                                               (RB search limited to slice PRB range)
└─────────────────────────────────────────────────────────────────────────┘
```

| Layer | Responsibility | Key functions / objects |
|-------|----------------|-------------------------|
| **Preprocessor** | Branch PF vs NS; UL also iterates K2 / target UL slots | `nr_dlsch_preprocessor`, `nr_ulsch_preprocessor` |
| **Slice (PRB)** | Demand estimation, ratio policy, contiguous PRB ranges | `slice_scheduler_{dl,ul}`, `slice_sch_schedule()` |
| **UE (intra-slice)** | Per-UE MCS, beam, TDA, CCE, RB placement inside window | `nr_dl_schedule()`, `nr_ul_schedule()` |
| **RB policy** | PF retx → minimal grant → new data; slice-bounded search | `nr_*_proportional_fair`, `get_rb_alloc_slice()` |

DL and UL maintain **separate** slice schedulers (`slice_scheduler_dl`,
`slice_scheduler_ul`) so each direction can use different ratio policies and
`dl_scheduler_type` / `ul_scheduler_type` independently.

### Per-slot execution — downlink

`nr_dlsch_preprocessor()` branches on `scheduler_type_dl`. When `SCHE_NS`,
`nr_dl_schedule_ns()` runs **once per DL slot**:

```
for each beam_idx:
  1. Build UEs_in_this_beam (UE->UE_beam_index == beam_idx)
  2. slice_sch_update_total_prbs(slice_scheduler_dl, n_rb_sched[beam])
  3. For each configured slice:
       a. Sum required_prbs over UEs matching slice S-NSSAI:
          - RLC bytes per LCID (incl. SRB STATUS-trigger edge cases)
          - pending DL HARQ retx rbSize
          - floor at min_sched_prbs (max(min_grant_prb, 5)) when demand > 0
       b. slice_sch_update_require(sst, sd, required_prbs)
  4. slice_sch_schedule()  →  slice_prb_range_t[] per slice
  5. Rotate slice order: start = (frame * slots_per_frame + slot) % num_ranges
  6. For each slice s in rotated order:
       a. Build UEs_in_this_slice (effective nssai matches allocation[s])
       b. nr_dl_schedule(mac, UEs_in_this_slice, …, &allocation[s], remainUEs)
```

Inside `nr_dl_schedule()` with a non-NULL `slice_prb`:

1. `collect_dl_candidates()` — only UEs in the slice subset
2. Pipeline stages 2–5 — same as `SCHE_PF` (RI/PMI, beam, TDA, MCS)
3. `nr_dl_proportional_fair()` — `slice_rb_start` / `slice_rb_end` constrain RB search;
   `n_rb_avail = slice_prb->num_prbs`; `max_num_ue = remainUEs[beam]`
4. Dispatch scheduled candidates; decrement `remainUEs[beam]` by count scheduled
5. Retx candidates that could not be placed in the slice PRB range → HARQ abort

### Per-slot execution — uplink

`nr_ulsch_preprocessor()` has an **outer** loop (reachable UL slots via K2, DCI
budget). Each iteration:

1. `collect_ul_candidates()` on the full connected UE list (shared by PF and NS)
2. If `scheduler_type_ul == SCHE_NS` → `nr_ul_schedule_ns(candidates, n_cand, …)`
   else → `nr_ul_schedule(candidates, n_cand, …, slice_prb = NULL)`

Inside `nr_ul_schedule_ns()` (same beam / slice / rotate pattern as DL):

```
for each beam_idx:
  … slice_sch_update_require / slice_sch_schedule …
  for each slice s (rotated order):
    filter_ul_candidates_for_slice(candidates, beam_idx, allocation[s])
    nr_ul_schedule_candidates()  →  copy + nr_ul_schedule(slice_prb, remainUEs)
```

`nr_ul_schedule()` in slice mode mirrors DL: slice fields on `nr_ul_sched_params_t`,
shared `remainUEs`, retx abort on failure. `remainUEs` is decremented in the
dispatch loop when each UE is committed.

### Shared vs isolated resources

| Resource | Scope under NS | Notes |
|----------|----------------|-------|
| **PRB ranges** | Per slice (isolated windows) | Allocator output `[start_prb, end_prb)`; PF searches only inside window |
| **VRB map** | Cell-wide (shared) | Allocations from slice A mark bits used for slice B in the same slot |
| **CORESET / CCE** | Cell-wide (shared) | `remainUEs` caps total DCIs per slot (max 4); slices compete for budget |
| **Beams** | Per UE | NS loops `beam_idx`; slice allocator runs per beam independently |

Frequency isolation is **soft at scheduling time**: the allocator assigns windows;
`get_rb_alloc_slice()` enforces that UE grants stay inside the slice. There is no
separate VRB map per slice.

### DCI budget (`remainUEs`)

FAPI limits concurrent DCIs (`MAX_DCI_CORESET`, typically 4 per slot). NS uses one
shared `remainUEs[num_beams]` array for **all slices** in a slot:

- Initialized to `max_num_ue` (4) at the start of `nr_*_schedule_ns()`
- Each successful `nr_*_schedule()` call consumes from `remainUEs[beam]`
- **Slice order rotates** per slot (`start = (frame * slots + slot) % num_slices`) so
  no slice permanently starves for DCIs when PRB ranges are fixed

PRB ranges from `slice_sch_schedule()` do **not** rotate — only UE scheduling order does.

### Slice allocation result

```c
typedef struct {
  slice_nssai_t slice_id;  /* SST + SD */
  int start_prb;           /* inclusive */
  int end_prb;             /* exclusive */
  int num_prbs;            /* end_prb - start_prb */
} slice_prb_range_t;
```

Passed into `nr_dl_schedule()` / `nr_ul_schedule()` as `slice_prb`; copied into
sched params as `slice_rb_start` / `slice_rb_end`.

### Init-time preprocessor selection

`nr_mac_init_scheduler()` in `main.c` always binds the same preprocessors (after gNB
YAML is loaded). PF vs NS is selected **inside** the preprocessor via
`scheduler_type_dl` / `scheduler_type_ul`:

```c
mac->pre_processor_dl = nr_dlsch_preprocessor;
mac->pre_processor_ul = nr_ulsch_preprocessor;
```

`nr_dlsch_preprocessor()` / `nr_ulsch_preprocessor()` branch:

```c
if (mac->scheduler_type_dl == SCHE_NS)
  nr_dl_schedule_ns(...);
else
  nr_dl_schedule(..., NULL, NULL);

/* UL: collect_ul_candidates() first, then */
if (nr_mac->scheduler_type_ul == SCHE_NS)
  nr_ul_schedule_ns(..., candidates, n_cand, ...);
else
  nr_ul_schedule(..., candidates, n_cand, ..., NULL, NULL);
```

Called from `mac_top_init_gNB()` (defaults) and again from `gnb_config.c` after
`set_slice_config()`.

Stages 2–6 inside `nr_*_schedule()` still use the **same** function pointers as PF
(`dl_rb_alloc` → `nr_dl_proportional_fair`, etc.). NS does not replace those; it
only branches in the preprocessor, runs the slice allocator, and passes slice-window
sched params.

Function pointer types: `nr_pp_impl_dl` / `nr_pp_impl_ul` in `nr_mac_gNB.h`.

### Slice-aware sched params

When `nr_dl_schedule()` / `nr_ul_schedule()` run inside NS, `nr_*_sched_params_t` carries:

| Field | Meaning |
|-------|---------|
| `slice_rb_start` | Absolute PRB start of slice PRB range (`-1` = full BWP) |
| `slice_rb_end` | Absolute PRB end, exclusive (`-1` = full BWP) |
| `n_rb_avail[]` | Set to `slice_prb->num_prbs` for PF fair-share within the slice |

RB allocation uses `get_rb_alloc_slice()` to limit search to the intersection of the
UE BWP and `[slice_rb_start, slice_rb_end)`.

### PF vs NS — same UE scheduler

Both modes use `nr_dl_proportional_fair` / `nr_ul_proportional_fair` for RB allocation.
NS adds:

- Slice PRB windows from `slice_prb_allocator`
- Per-slice UE subsets filtered by `nr_mac_get_ue_effective_nssai()`
- Slice-bounded RB search and slice-sized `n_rb_avail`
- Cross-slice `remainUEs` accounting and retx HARQ abort when retx does not fit

### Fallback

If `slice_scheduler_*` is NULL or has zero slices, `nr_*_schedule_ns` falls back to
full-band `nr_dl_schedule()` / `nr_ul_schedule_candidates()` with `slice_prb = NULL`
(same modular PF path as `SCHE_PF`). UL reuses the candidates already collected in
`nr_ulsch_preprocessor()`.

### High-level call graph (reference)

```
nr_schedule_ue_spec / nr_schedule_ulsch
        │
        ▼
  nr_dlsch_preprocessor / nr_ulsch_preprocessor
        │
        ├── scheduler_type_* == SCHE_PF
        │      └── nr_dl_schedule() / nr_ul_schedule()
        │            └── nr_*_proportional_fair
        │
        └── scheduler_type_* == SCHE_NS
               └── nr_dl_schedule_ns() / nr_ul_schedule_ns()
                     ├── slice_prb_allocator
                     └── per slice: nr_*_schedule(slice_prb, remainUEs)
                           └── nr_*_proportional_fair (slice PRB range)
```

UL note: `collect_ul_candidates()` runs once per K2 iteration in
`nr_ulsch_preprocessor()` before the PF/NS branch; NS filters that snapshot per slice
instead of re-collecting.

---

## Configuration

### gNB YAML: `Slices` block

Parsed in `gnb_config.c` → `set_slice_config()`. Only active when at least one
direction uses `SCHE_NS`.

```yaml
Slices:
  - slice_id: 1
    sst: 1
    sd: 0x000001
    dedicated_prb_ratio: 0.0
    min_prb_ratio: 0.0
    max_prb_ratio: 100.0
    # Optional per-direction overrides:
    # dl_dedicated_prb_ratio, dl_min_prb_ratio, dl_max_prb_ratio
    # ul_dedicated_prb_ratio, ul_min_prb_ratio, ul_max_prb_ratio
```

Constraints enforced at config time:

- `dedicated ≤ min ≤ max` per slice
- Sum of `dedicated` ratios ≤ 1.0 per direction (DL and UL separately)

### PLMN / core alignment

NWS configs list matching S-NSSAIs under `plmn_list.snssaiList` and map UEs to
slices via Open5GS subscriber `sst` / `sd`. Core and RAN slice identifiers must
match for correct UE-to-slice mapping.

Example configs: `nws/configs/gnb/gnb.sa.band78.*.yaml`.

---

## E2 / O-RAN control

Slice policy can be read and updated over E2:

| Component | Path |
|-----------|------|
| RAN function | `openair2/E2AP/RAN_FUNCTION/CUSTOMIZED/ran_func_slice.c` |
| Policy source | `slice_scheduler_dl` / `slice_scheduler_ul` when `SCHE_NS` active |
| xApp (NWS) | `nws/scripts/xapp/` — REST on port 18080, Slice SM indications |

E2 SET updates dedicated/min/max ratios at runtime; gNB logs `NS E2 SET applied`
on success.

---

## Code map

| Area | File |
|------|------|
| Scheduler types | `openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_types.h` |
| Init / preprocessor bind | `openair2/LAYER2/NR_MAC_gNB/main.c` (`nr_mac_init_scheduler`) |
| DL preprocessor | `gNB_scheduler_dlsch.c` — `nr_dlsch_preprocessor` (PF/NS branch) |
| DL NS core | `gNB_scheduler_dlsch.c` — `nr_dl_schedule_ns`, `nr_dl_schedule` (slice mode) |
| UL preprocessor | `gNB_scheduler_ulsch.c` — `nr_ulsch_preprocessor` (collect + PF/NS branch) |
| UL NS core | `gNB_scheduler_ulsch.c` — `nr_ul_schedule_ns`, `filter_ul_candidates_for_slice`, `nr_ul_schedule_candidates` |
| Slice-aware RB policy | `gNB_scheduler_*_default_policies.c` — `nr_*_proportional_fair` |
| RB search in slice | `gNB_scheduler_primitives.c` — `get_rb_alloc_slice()`, `nr_slice_rb_bounds()` |
| PRB allocator | `slice_prb_allocator/slice_prb_allocator.c` |
| YAML slice config | `openair2/GNB_APP/gnb_config.c` — `set_slice_config` |
| CORESET duration | `openair2/GNB_APP/gnb_config.c` — `fix_scc`, `coreset_duration` |
| E2 slice SM | `openair2/E2AP/RAN_FUNCTION/CUSTOMIZED/ran_func_slice.c` |
| Telnet stats | `common/utils/telnetsrv/telnetsrv_proccmd.c` (scheduler + slice dump) |

---

## Observability

### gNB logs

At startup:

```
MAC instance 0: dl_scheduler_type=0, ul_scheduler_type=1
MAC scheduler preprocessors: DL=SCHE_PF UL=SCHE_NS
```

### Telnet

Use the MAC stats command (see telnetsrv docs) to print scheduler type and per-slice
PRB allocation when NS is enabled.

### xApp / FlexRIC

```bash
curl -s http://127.0.0.1:18080/api/v1/slices | jq .
```

---

## Code change summary (slice-aware modular PF)

Intra-slice scheduling was moved from legacy monolithic schedulers to the
same modular pipeline used by `SCHE_PF`. The slice layer (`nr_*_schedule_ns`) is
unchanged; only the per-slice UE scheduler path uses `nr_*_schedule()` now.

| Change | Purpose |
|--------|---------|
| `nr_dl_schedule(..., slice_prb, remainUEs)` | Optional NS args: slice PRB range + shared DCI budget |
| `nr_ul_schedule(..., slice_prb, remainUEs)` | Same for UL; `remainUEs` decremented on dispatch |
| `filter_ul_candidates_for_slice()` | Per-slice subset of pre-collected UL candidates |
| `nr_ul_schedule_candidates()` | Copy candidates + call `nr_ul_schedule()` (per-slice NS) |
| Unified preprocessors | `nr_*sch_preprocessor` branch on `scheduler_type_*` (no `_ns` variants) |
| `nr_*_sched_params_t::slice_rb_start/end` | Absolute PRB window passed to RB policy |
| `nr_slice_rb_bounds()` | Map absolute slice PRB range to BWP-relative bounds |
| `get_rb_alloc_slice()` | Contiguous RB search limited to slice ∩ BWP |
| `nr_*_get_rb_alloc()` in default policies | Wire slice PRB range into proportional fair |
| `nr_mac_init_scheduler()` | Always bind `nr_dlsch_preprocessor` / `nr_ulsch_preprocessor` |
| HARQ abort in `nr_*_schedule()` | Retx that cannot fit in slice PRB range is dropped |

---

## Related documentation

- [MAC/scheduler-architecture.md](MAC/scheduler-architecture.md) — modular PF pipeline
- [slice_prb_allocator/README.md](../openair2/LAYER2/NR_MAC_gNB/slice_prb_allocator/README.md) — allocation algorithm
- [nws/scripts/readme.md](../../nws/scripts/readme.md) — lab bring-up and `--sch` modes
- [nws/scripts/xapp/readme.md](../../nws/scripts/xapp/readme.md) — REST / E2 control
