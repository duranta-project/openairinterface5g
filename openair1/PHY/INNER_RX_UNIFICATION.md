<!-- SPDX-License-Identifier: LicenseRef-CSSL-1.0 -->
# inner_rx unification — design notes

Goal: **one shared `inner_rx` used by both the UE (PDSCH) and gNB (PUSCH)**, doing everything
from channel compensation through the codeword LLR. The per-RE detector/LLR kernels are already
shared (`nr_phy_common`); what remains duplicated is the **orchestration** — the ~100 lines of
if/else that pick the detector for `(nl, mod, do_ml, fuse_mode)` and drive comp → detect → demap →
descramble. This note records the map of the two paths, the proposed shared contract, and why the
boundary sits **after extraction**.

Status when written (2026-08-03): comp → detect → demap → descramble is now fused and shared on
both sides (see INNER_RX_FUSION_TIMING.md). Extraction is still per-side. The two orchestration
blocks are `inner_rx` in `nr_ulsch_demodulation.c` (gNB) and the per-symbol block in `nr_rx_pdsch`
(`nr_dlsch_demodulation.c`, UE).

## Why the boundary is *after* extraction (not merged into compensation)

Extraction (`nr_ulsch_extract_rbs` / UE equivalent) is an **irregular → compact gather**: it walks
the full OFDM symbol and packs just the *data* REs — skipping DMRS holes (per `dmrs_config_type`),
RB-allocation gaps, PTRS/CSI-RS REs — into a dense, aligned `rxFext`/`chFext`. The indices are
data/config-dependent.

Compensation is the opposite — a **regular SIMD stream** over that dense array (`((NRLB_VI*)chFext)[blk]`,
aligned loads, uniform per-block math). These are fundamentally different access patterns, and the
seam between them is the right place to keep a materialization. Reasons **not** to merge extraction
into compensation:

1. **The extracted array is *reused*, not round-tripped.** This is the decisive difference from the
   comp→LLR fusion. There, `rxComp`/`mag`/`rho` were written once and read once — a true round-trip,
   so eliminating the materialization was a clean win. Extraction's output is read **many times** by
   compensation: `rxFext` once per Rx antenna (MRC), and `chFext` **three ways** — conj-mult (MRC),
   magnitude (`smadd`), and 2-layer `rho` (cross-correlation). Inlining the gather would re-run it on
   every one of those reads (the tile is far larger than the register file, so "gather once into
   registers" doesn't hold). Materialize-once-stream-many is the *correct* call here; merging
   multiplies the expensive irregular work instead of removing it.
2. **Access pattern.** Merged, the compensation's contiguous vector loads become **indexed gathers**.
   On NEON there is no efficient gather → it degrades to scalar load + pack, destroying vectorization.
   On RVV (`vluxei`) gather is first-class so a merge is *plausible* there, but the multiple-reuse
   argument still favors materialize-once even on RVV; on x86/GH200 it is a clear loss.
3. **Decoupling irregular from regular.** Extraction owns the branchy, standard-specific DMRS/RB/PTRS
   layout logic; compensation stays a clean numeric kernel. Merging drags config-dependent branching
   into the hot math loop.
4. **RE count.** The valid-data-RE count (after DMRS/PTRS removal) is only known post-extraction and
   sets the LLR length + codeword offsets. A distinct extraction stage hands a clean count downstream.

The only scenario that would flip this — extraction's compact-array round-trip dominating **and**
compensation reading it once — is not our case (compensation is a heavy multi-reuse consumer). So the
shared `inner_rx` starts at **post-extraction** (`rxFext`/`chFext` in), and extraction stays in the
per-side wrappers. (Merging extraction is a separate, harder, largely platform-specific question —
revisit only for RVV, and even there weigh it against multi-reuse.)

### Refinement: skip the copy on data symbols (pointer-pass), gather only on special symbols

The "materialize because reused" argument justifies the **gather** (irregular→regular), not a plain
copy. Extraction actually does two jobs, and only one earns its keep:
- **Gather** (compact around DMRS/PTRS/CSI-RS holes or RB gaps) — genuine value, but only on the
  **special symbols** (DMRS-bearing, PTRS, and CSI-RS on the UE). Materialize into scratch there.
- **Plain contiguous copy** — pure overhead on the **majority** of symbols (pure data + contiguous
  allocation): reading `rxdataF` directly 3× is identical to reading a copy 3×, so the copy adds
  nothing.

So for a symbol with **no DMRS/PTRS/CSI-RS and a single contiguous RB run** (RA type-1, no type-0
bitmap gaps, no VRB↔PRB interleaving, no intra-slot hopping), skip the memcpy and pass a **pointer**
`&rxdataF[sym*ofdm_size + first_sc]` (+ channel-estimate pointer) straight to `inner_rx`. Special
symbols keep the gather-into-scratch path. This removes the per-symbol extraction memcpy for most
symbols without merging the gather into the hot compensation loop.

Implications for the shared contract:
- `inner_rx` must take a **row stride** (or per-antenna pointer array) instead of assuming the compact
  `buffer_length` stride — a direct `rxdataF` pointer has stride `ofdm_symbol_size`. This is a clean
  generalization (arguably makes the contract better). The per-side wrapper's "extraction" then
  returns *(base ptr, stride)*: the direct pointer for data symbols, or a gathered-scratch pointer
  for special symbols. Same post-extraction boundary; common case pays **zero** copy.

To verify in code before implementing:
- **Alignment** — `first_rb*48` bytes is always 16-byte aligned (NEON/native-128, RVV fine) but
  32-byte aligned only when `first_rb` is even → the direct path needs **unaligned loads** on
  AVX2/512 (check whether the kernels' `(NRLB_VI*)` derefs are aligned or already `loadu`).
- **Channel-estimate layout** — whether the estimate for the data REs is a contiguous per-symbol
  slice (pointer works), full-BW per symbol (same contiguity test), or time-interpolated /
  held constant across data symbols (then it may be symbol-invariant → extract once, even better).
- **Allocation contiguity** — the concrete non-contiguity trigger is **RA type-0 (RBG bitmap)**;
  when the bitmap is used the allocated RBs may have gaps, so those symbols fall to the gather path
  (also VRB↔PRB interleaving for PDSCH, and intra-slot hopping). **RA type-1** (contiguous
  `startRB + nRB`) is the pointer fast-path. The wrapper already has the RA type from the DCI/config,
  so the pointer-vs-gather choice is a cheap up-front check (type-1 & not-special-symbol → pointer;
  type-0 / interleaved / hopping → gather). A type-0 bitmap that is itself one contiguous run could
  also take the fast path, but gating on type-1 is the simple, safe default.

## Dispatch map: gNB `inner_rx` vs UE `nr_rx_pdsch` (post-extraction)

| Stage | gNB (`nr_ulsch_demodulation.c`) | UE (`nr_dlsch_demodulation.c`) | Verdict |
|---|---|---|---|
| **1. Fuse gates** | `fuse_1layer = fuse && nl==1 && !transformPrec && !ptrs`; `fuse_2layer_ml = fuse && nl==2 && (qam<=6 \|\| (qam==8 && gnb_lbest)) && !ptrs` | `fuse_1layer = fuse && nl==1 && !ptrs`; `fuse_2layer_ml = fuse && nl==2 && do_ml && (qam<=6 \|\| (qam==8 && ml256)) && !ptrs` | **Divergent (reconcilable)** — same shape; side-only predicates (gNB `transformPrec`; UE `do_ml`/`ml256`; gNB `gnb_lbest` vs UE `ml256` for 256QAM) → config flags |
| **2. Compensation** | `nr_channel_compensation` (skip if fused); memsets gated `!fuse_skip_comp` | `nr_channel_compensation` (skip if fused) | **Common** (shared fn). gNB-only memset-gating is a wrapper detail |
| **3. MMSE pre-pass** | *(none — folded into `nr_compute_MMSE_llr`)* | `nr_dlsch_mmse` for `(nl>2 && !ml3) \|\| (nl==2 && !do_ml)` — equalizes `rxdataF_comp` in place | **Divergent** — the detector-unification gap; gNB fused it, UE hasn't |
| **4. PTRS** | `nr_pusch_ptrs_processing` | `nr_pdsch_ptrs_processing` | **Divergent (side-inherent)** — stays in wrappers |
| **5a. 1-layer** | fused → `nr_inner_rx_1layer(llr_cw, scramble)`; else per-layer `nr_compute_llr` | fused(tiled) → `nr_inner_rx_1layer(llr_cw, scramble)`; `OAI_FUSE=2` → `nr_inner_rx_1layer_reg`; else `nr_dlsch_llr` | **Common + UE extras** — fused path identical; `nr_compute_llr` ≡ `nr_dlsch_llr` (same `nr_XXqam_llr` kernels, layer loop inside vs outside → merge); `_reg` is UE-only |
| **5b. 2-layer ML** (qam<=6, or 256+lbest/ml256) | fused → `nr_inner_rx_2layer_ml(llr_cw, scramble)`; else `nr_compute_ML_llr` | same two | **Common** |
| **5c. 2-layer MMSE / 256** | `nr_compute_MMSE_llr` (fused per-RE MMSE+LLR) | 256+do_ml → `nr_compute_MMSE_llr`; 2L !do_ml → (stage-3 `nr_dlsch_mmse`) + `nr_dlsch_llr` | **Partly common** — `nr_compute_MMSE_llr` shared; UE keeps the legacy split for `!do_ml` |
| **5d. 3-layer** | `nb_layer!=2` → per-layer `nr_compute_llr` (MRC only, no joint >2L) | `ml3` → `nr_qam_llr_3layer_{hybrid,ml}` | **Divergent** — `ml3` is UE-only |
| **6. Demap + descramble** | fused: at store; non-fused: post-pass in caller | fused: at store; non-fused: `nr_layer_demapping` + descramble | **Common** (made so this session) |

## Three classes of divergence

1. **Reconcilable via config** (belongs *inside* the shared fn): fuse-gate predicates
   (`transformPrec`/`do_ml`/`ml256`/`lbest`), 1-layer non-fused wrapper (`nr_compute_llr` ≡
   `nr_dlsch_llr` → pick one), the fused 1-/2-layer paths, `nr_compute_ML_llr`,
   `nr_compute_MMSE_llr`, demap+descramble.
2. **UE-only detectors** (need a home — config-selected branches or a detector table): `_reg`
   variant, `ml256`, `ml3`, the `nr_dlsch_mmse` + `nr_dlsch_llr` legacy MMSE split.
3. **Side-inherent glue** (stays in thin wrappers, *outside* the shared fn): PTRS
   (`pusch`/`pdsch`), scope/observability taps, gNB memset-gating, buffer ownership, extraction.

## Proposed shared contract

```
nr_inner_rx(rxFext, chFext,        // extracted inputs (produced per side)
            mag/rho scratch,       // or computed inline by the fused kernels
            cfg,                   // {nl, mod, do_ml, ml256, ml3, lbest, fuse_mode, output_shift, nvar}
            scramble,              // per-symbol +-1 slice, or NULL
            llr_cw)                // codeword output
   -> valid_re / status
```
Wrappers keep: extraction, PTRS, scope, memset-gating, buffer alloc — and pass `cfg` + `scramble` +
`llr_cw`.

## Open decision (how to sequence the merge)

The one real blocker to a *clean* merge is 5c/5d — the UE still carries `nr_dlsch_mmse` and `ml3`
the gNB doesn't. Two options:

- **(a) Unify the proven-common spine now** (stages 2, 5a-fused, 5b, 5c-`MMSE_llr`, 6) behind the
  shared signature; leave 5c-`!do_ml` / 5d as documented side callbacks the shared fn invokes.
  Immediate dedup, both sides call the shared fn, detector-unification collapses the callbacks later.
- **(b) Detector-unification first** — drop `nr_dlsch_mmse` via L-best deflation (see the
  detector-unification note) so 5c/5d converge, *then* unify. Cleaner end state, more upfront work.

Recommendation: **(a)** — unify the spine now with (b) as follow-up. (Pending user sign-off.)
