# NR RX Fusion — Restructuring Assessment (Step 1)

**Goal.** Fuse the RX per-symbol chain — *extraction → channel compensation → LLR → descrambling* — so that the intermediate results stay resident in registers / L1 instead of being written to full-slot (or full-symbol) scratch buffers and read back. This trades **memory traffic for compute**, which is the right trade for accelerators and cores with modest L1 and wide vector units: SpaceMIT K3 A100 (RVV, VLEN=1024), Qualcomm Hexagon (HVX), AMD AIE, ARM (NEON/SVE, small L1), and possibly CUDA/HIP GPUs.

This repo drives the **RISC-V (RVV)** implementation; the restructuring is meant to be platform-agnostic so the parallel **aarch64 / Hexagon / AIE** efforts implement the same *tile* contract. A north-star goal is a single **channel-agnostic `inner_rx`** shared across the NR *data* channels — PDSCH (UE), PUSCH (gNB), and Sidelink PSSCH (§3a). Correctness bar for the refactor: **functionally equivalent** (same BLER; minor numerical differences from reordering are acceptable), *not* bit-exact — this gives freedom to reorder and fuse kernels.

Scope of this document: (1) as-is dataflow + buffer inventory for both chains, (2) what must be restructured, (3) a proposed *process-one-tile* interface for the parallel teams, (4) how to preserve the observability taps at zero cost when disabled.

---

## 0. TL;DR — the three findings that shape everything

1. **The gNB UL chain is already ~fused at OFDM-symbol granularity; the UE DL chain is not.**
   `nr_pusch_symbol_processing` → `inner_rx` (`nr_ulsch_demodulation.c:406,224`) keeps a per-symbol tile in stack buffers (`rxFext`/`chFext`) and runs extract → compensate → LLR back-to-back, then demaps + **descrambles that symbol's LLRs immediately** (`:465-485`). UL is our reference "compute-over-memory" template.
   The UE DL chain (`nr_rx_pdsch`, `nr_dlsch_demodulation.c:744`) **defers LLR + demap to the last symbol** (`:1121` gate, then a loop over *all* symbols) and descrambles the **whole codeword** later (`nr_dlsch_unscrambling`, `nr_dlsch_decoding.c:36`). That deferral is the *sole reason* DL needs full-slot `rxdataF_comp`, `dl_ch_mag*`, `rho_dl`.
   **Why UL already has this shape:** per-symbol processing is what lets the gNB dispatch OFDM symbols to **different CPU cores** (`nr_pusch_symbol_processing` runs as a thread-pool task, one per symbol group). The UE will gain the same symbol-level parallelism — so **R1 (un-deferring the DL chain) is a prerequisite for UE symbol-parallelism as well as the memory win**; the fusion and threading roadmaps share this restructuring.

2. **Extraction is pure memory movement and should be *dissolved*, not ported.**
   `nr_dlsch_extract_rbs` / `nr_ulsch_extract_rbs` are scalar gathers/`memcpy` (no arithmetic on samples, no RVV). Their only job is to pack data REs (skipping DMRS/CSI) into contiguous `*_ext` arrays. In a fused kernel the strided/masked grid load folds directly into the compensation kernel's `vlseg2` input, and `rxdataF_ext` / `dl_ch_estimates_ext` disappear.

3. **The compute-dense middle stages already have validated RVV kernels.**
   `nr_channel_compensation` (RVV `nr_channel_compensation.c:61`), the single-layer QAM LLRs (`nr_phy_common.c:73/86/101`), the 2-layer L-best (this session), and descramble (`nr_scrambling.c:48/82`) are all hand-written RVV, per-RE-vector, bit-exact-validated in `openair1/PHY/rvv_harness/`. **Fusion is mostly a *dataflow* restructuring, not new kernel math** — the pieces exist; they need to be composed per-tile instead of per-full-buffer.

4. **The per-PRB MMSE routine should be *deleted*, not tiled — linear equalization is already a per-RE operation.** The MMSE/ZF 2×2 (K×K) solve already lives inside the L-best seed (`rvlb64_seed`/`rvlb256_seed`). Treat pure MMSE as the **L = 1** case (per-RE solve → single-layer QAM LLR), and 3-layer nulling the same way; this removes `nr_dlsch_mmse` / `nr_mmse_2layers` and makes equalization fully fusible per-RE (finer *and* more accurate than today's per-RB averaging). The one exception is **SC-FDMA / DFT-s-OFDM**, whose whole-symbol IDFT keeps memory unavoidable — its fused span ends at equalization.

---

## 1. As-is architecture

### 1a. UE DL — `nr_rx_pdsch` (per-symbol; LLR deferred to last symbol)

| # | Stage | Function | call site | granularity |
|---|-------|----------|-----------|-------------|
| 1 | RE extraction | `nr_dlsch_extract_rbs` | `:840` | whole symbol → `*_ext` |
| 2 | channel scale (in-place) | `nr_scale_channel` | `:912` | whole symbol |
| 3 | channel level (first sym only) | `nr_channel_level` / `..._median` | `:931,945` | whole symbol |
| 4 | compensation / MF + MRC + rho | `nr_channel_compensation` | `:995` | whole symbol → **full-slot** `rxdataF_comp[symbol]` |
| 5 | MMSE (multi-layer, in-place) | `nr_dlsch_mmse` / `nr_mmse_2layers` | `:1041,1055` | per-RB |
| 6 | PTRS phase (in-place) | `nr_pdsch_ptrs_processing` | `:1103` | whole symbol |
| 7 | **LLR (deferred, loops all symbols)** | `nr_dlsch_llr` / `nr_compute_ML_llr` | `:1131-1205` | full slot |
| 8 | layer demapping (deferred) | `nr_dlsch_layer_demapping` | `:1208` | full slot |
| — | descrambling (later, per codeword) | `nr_dlsch_unscrambling` | `nr_dlsch_decoding.c:36` | full codeword |

**Buffers.** Per-symbol intermediates: `rxdataF_ext` (`:834`, single producer→consumer — prime dissolve target), `dl_ch_estimates_ext`/`chFext` (`:775`, scaled in place). **Full-slot** intermediates (backed by `pdsch_scratch_t`, `defs_nr_UE.h:447-458`): `rxdataF_comp[SYM][LAYER][buf]`, `dl_ch_mag/magb/magr`, `rho_dl[SYM][LAYER²][buf]`. These four are full-slot **only because LLR is deferred**.

### 1b. gNB UL — `nr_pusch_symbol_processing` → `inner_rx` (already symbol-fused)

Per symbol, `inner_rx` (`:224`): extract (`:261`) → compensate+rho (`:296`) → [transform-precoding: freq-eq `:314` + IDFT `:321`] → [PTRS `:330`] → LLR (`:336/348/367`). Then back in the caller: layer demap (`:470`) → **inline per-symbol descramble** `llr_dest[k]=src[k]*s_seq[k]` (`:476-485`).

**Buffers.** Per-symbol tiles on the stack: `rxFext`, `chFext`, `rho`, `rxF_ch_mag{a,b,c}` (`:245-290`), `llrs` (`:420`). The **only** cross-stage full-slot buffer is `pusch_vars->rxdataF_comp` (`defs_gNB.h:238`, written per-symbol at `symbol*buffer_length`, `:305`) — and it stays slot-wide **only to feed the scope export** (§4). Everything else is already per-symbol.

### 1c. Shared kernels & RVV readiness

| Stage | Function(s) | RVV? | compute:memory |
|---|---|---|---|
| Extract DL/UL | `nr_dlsch_extract_rbs` / `nr_ulsch_extract_rbs` | **No** (scalar gather/memcpy) | ~pure memory → *dissolve* |
| Compensation | `nr_channel_compensation` (`.c:61` RVV / `:187` SIMDe) | **Yes** | compute-heavy (~10–20 ops/RE) |
| LLR 1-layer QAM | `nr_16/64/256qam_llr(_rvv)` (`nr_phy_common.c`) | **Yes** | low ops, seg load/store bound |
| LLR 2-layer ML | `nr_qam{16,64,256}_llr_2layer`, L-best (`nr_compute_llr.c`) | Partial (QPSK + L-best done) | compute-dense (L-best) |
| Descramble | `nr_codeword_unscrambling(_init)` (`nr_scrambling.c:44/78`) | **Yes** (masked vneg / ±1 mul) | ~1 op/LLR → fusible tail |

### 1d. Memory-traffic problem statement

The staged approach round-trips the whole symbol (DL: whole *slot*) through memory once per stage. For a 106-PRB slot (1272 data RE/symbol, ~13 symbols, 2 layers, `c16_t`): each full-slot buffer is ~1272·13·2·4 ≈ **130 KB** written *and* read per stage. `rxdataF_comp` + 3 `ch_mag` + `rho` on DL is ~5 such buffers = **~1.3 MB of round-trip traffic per slot that never needs to touch DRAM/L2** if the tile stays resident. On a wide-VLEN core this traffic — not the arithmetic — sets the wall-clock (the same effect made the 2-layer L-best 4× slower on A100/VLEN=1024 than X100/VLEN=256; see `nr_mimo_lbest_detector.md`).

---

## 2. What must be restructured

Ordered by leverage and independence (each is separately shippable and functionally-equivalent-verifiable):

**R1 — DL: un-defer LLR + demap + descramble into the per-symbol loop.** *Highest leverage, lowest risk, no new kernels — and a shared prerequisite for UE symbol-parallelism (see finding #1).*
Move stages 7–8 (and descrambling) inside `nr_rx_pdsch` so each symbol produces its final LLRs before the next symbol. This alone collapses `rxdataF_comp`, `dl_ch_mag*`, `rho_dl` from **full-slot → per-symbol** (a ~13× shrink), makes DL structurally match UL's `inner_rx`, and puts the DL chain in the per-symbol shape needed to dispatch symbols to different cores (as the gNB already does). Requires: per-symbol LLR bit-offset tracking (mirror UL's `llr_offset`/`sym_bit_offset`, `nr_ulsch_demodulation.c:455,842-853`) so descramble can run on the symbol's LLR slice. Watch the in-place ordering equalize→PTRS→LLR per symbol (`:1045,1055,1108`).

**R2 — Both: dissolve extraction into compensation.** Replace `*_ext` materialization with a strided/masked grid *view* consumed directly by the compensation kernel's `vlseg2` load. Eliminates `rxdataF_ext`, `dl_ch_estimates_ext`/`rxFext`/`chFext`. The DMRS/CSI-skip logic (`nr_dlsch_demodulation.c:176-193`) becomes a load mask / per-RB-run iterator rather than a copy.

**R3 — Both: sub-symbol RE-tiling of the compensate→LLR→descramble span.** Iterate the symbol in tiles of up to `TILE` REs and keep each tile's compensated samples + magnitudes + LLRs in registers through the whole span. After R1+R2 the per-symbol buffers shrink to per-*tile* and mostly vanish into registers. This is where small-L1 targets (Hexagon/AIE) win most.

*Granularity is bounded by contiguous allocation runs, not free.* RB allocation type 0 is an **RBG bitmask** — RBG size is 2/4/8/16 PRBs per bit (BWP-dependent), **not 1** — so a data allocation is a set of contiguous *runs* of RBGs separated by gaps. A tile must not span a gap (the AVX512 path already does multi-PRB-per-iteration, but only *within* a run). Combined with DMRS/CSI-RS **puncturing** inside data-bearing symbols, the contiguous *data-RE* run length is variable and generally **not a VLEN multiple**, so every run has a **partial tail that can't fill a full register**. Consequences the tile contract must expose:
- The iterator walks contiguous data-RE runs and emits a partial final tile per run (`n_re < TILE`).
- Within a run, the LOAD is a **contiguous** vector load on pure-data symbols but a **strided/masked** load on DMRS/CSI-punctured symbols (mirroring the current fast-`memcpy` vs strided-gather extract paths, `nr_dlsch_demodulation.c:216-236`).
- Tail handling is **per-platform**: RVV absorbs it natively via `vsetvl` (variable `vl` per iteration — a tile is "whatever fits, up to TILE"); fixed-width SIMD (AVX512/NEON/HVX/AIE) needs predication/masking or a scalar cleanup for the tail.
This is the price of R2: once extraction no longer packs allocated REs into a contiguous buffer, the fragmentation + tail handling moves into the tile load. (Keeping a *thin* per-tile gather-into-registers at the run head — not a full-symbol pack — is the fallback if a platform's masked loads are too costly.)

**R4 — Delete the per-PRB MMSE/nulling routines; unify linear + nonlinear detection as one per-RE kernel parameterized by candidate-set size L.** *This is the biggest simplification and it removes MMSE as a tiling constraint (see §2a).*
The per-RE 2×2 (K×K) MMSE/ZF solve *already exists* inside the L-best seed (`rvlb64_seed`/`rvlb256_seed`: `x̂₁ = ((P₁+λ)z₁ − ρz₂)/det` per RE, where `λ`=0 gives ZF, `λ`=noise gives MMSE). Reuse it as the single equalization primitive:
- **L = 1 → pure linear MMSE receiver.** The per-RE solve yields each layer's equalized symbol, fed straight to the existing single-layer QAM LLR kernels (`nr_{16,64,256}qam_llr_rvv`). This *replaces* `nr_dlsch_mmse` / `nr_mmse_2layers` (`nr_dlsch_demodulation.c:531`, `nr_compute_llr.c:6161`) entirely — MMSE becomes inline per-RE, no separate per-PRB routine, no `Hᴴ·H` build/invert pre-pass, no full-symbol MMSE buffer.
- **L > 1 → L-best joint detection.** Same per-RE seed, then the reduced candidate search (the kernels from this session).
- **3-layer.** The hybrid detector's projection/nulling (`nr_qam_llr_3layer_hybrid`) is *already* an inline per-RE operation — same treatment.
Because equalization+detection collapses to one per-RE inline kernel, it is the natural terminal stage of the fused tile and must be **register-resident**: unroll the candidate loop and hold `max0/max1` accumulators + per-candidate terms in vector registers (≈ the 32-register budget analyzed for the 5-candidate set in `nr_mimo_lbest_detector.md`). L=1 is trivially register-resident; the L-best kernels currently are not (stack-array round-trips) and are the ones to rewrite.

**R5 — DL descramble per-tile.** Fold `nr_dlsch_unscrambling` into R1's per-symbol path as the tile tail — the UL inline `llr*s_seq` (`:478-484`) is the exact template; expand the gold sequence to ±1 once per codeword (`nr_codeword_unscrambling_init`) so the tile tail is a single masked multiply.

### 2a. Constraints that bound the tiling granularity (call-outs)

**MMSE is NOT in this list** — R4 makes it a per-RE inline operation (the L=1 solve), so it no longer bounds the tile. The remaining stages genuinely need **cross-RE / whole-symbol** context and either run as a whole-symbol pre-pass or force a symbol-sized tile:

- **PTRS phase** (`nr_pdsch_ptrs_processing` / `nr_pusch_ptrs_processing`): estimates a common phase from pilots across the **whole symbol**, then applies per-RE. Split into a whole-symbol *estimate* pre-pass + a per-tile *apply*.
- **Transform precoding (DFT-s-OFDM / SC-FDMA, UL only)**: `nr_freq_equalization` + `nr_idft` (`nr_ulsch_demodulation.c:314,321`) is a **whole-symbol** IDFT — a hard tile boundary, and memory there is unavoidable. The fused span for transform-precoded PUSCH ends at equalization; LLR runs after the IDFT on the memory-resident IDFT output.

The common, hottest case — CP-OFDM, per-RE MMSE/L-best detection (R4), no PTRS — fully supports R1–R5 with no whole-symbol pre-pass at all.

---

## 3. Proposed *process-one-tile* interface (contract for the parallel teams)

Separate a **platform-agnostic harness** (the tiled outer loop, plain C, shared by all targets) from a **per-platform inner kernel** (RVV / NEON / HVX / AIE fill this in). The teams implement one function against one descriptor; the dataflow is decided once, here.

```c
// One contiguous run of data REs within one OFDM symbol. Points into the FD grid and
// channel estimates directly — no per-stage scratch. All pointers are const inputs
// except llr_out.
typedef struct {
  const c16_t   *rxF[NR_MAX_RX];              // grid samples, this tile's REs, per rx antenna
  const c16_t   *chF[NR_MAX_LAYER][NR_MAX_RX];// channel estimates, this tile
  uint16_t       n_re;                        // REs in this tile (<= NR_RX_TILE_MAX)
  uint8_t        n_layer, n_rx, mod_order;    // problem shape
  int32_t        log2_maxh;                   // compensation scaling (from the level pre-pass)
  const int16_t *ptrs_phase;                  // NULL, or whole-symbol phase to apply per-RE
  // outputs
  int16_t       *llr_out;                     // final (descrambled) LLR slice for this tile
  const int16_t *scramble_seq;                // +/-1 sequence aligned to llr_out (NULL = skip)
  // optional observability tap (NULL => zero cost, see §4)
  const rx_tap_t *tap;
} rx_tile_t;

// The fused kernel: load(grid+chest) -> compensate(+MRC/rho) -> [apply ptrs] ->
// llr -> descramble -> store(llr_out). Keeps the tile in vector registers end-to-end.
// Platform teams provide this; the C harness calls it per tile.
void nr_rx_process_tile(const rx_tile_t *t);

// Whole-symbol pre-passes the harness runs BEFORE the tile loop when needed:
//   - channel level / log2_maxh   (already: nr_channel_level, first symbol)
//   - MMSE Hᴴ·H build+invert per RB (feeds compensate; or fold into a per-RB tile)
//   - PTRS common-phase estimate  (-> rx_tile_t.ptrs_phase)
//   - transform precoding: equalize+IDFT terminates the fused span (LLR after)
```

The harness owns: the **run iterator** — it walks the RBG-bitmask allocation as contiguous data-RE runs (never crossing a gap), applies the DMRS/CSI-RS skip, and yields tile ranges bounded by each run with a **partial final tile** (`n_re ≤ NR_RX_TILE_MAX`) per run (see R3); tile-size selection per platform (`NR_RX_TILE_MAX`); the per-symbol LLR bit-offset bookkeeping; and the tap plumbing. `nr_rx_process_tile` receives `n_re` and must handle the partial tail — trivial on RVV (`vsetvl(n_re)`), a masked/scalar cleanup on fixed-width SIMD. The tile LOAD is the strided/masked grid gather that replaces extraction (R2): contiguous on pure-data symbols, strided on DMRS/CSI-punctured symbols. Each platform provides `nr_rx_process_tile` (and may special-case `mod_order`/`n_layer` internally, exactly as the current kernels dispatch). Reference implementation: RVV, composed from the already-validated `nr_channel_compensation` (RVV), QAM-LLR (RVV), L-best (R4), and descramble (RVV) inner bodies — inlined into one loop body so no vector value ever round-trips to memory (this is also the `always_inline` discipline that fixed the L-best miscompile).

Tile-size guidance: `TILE` is a per-platform tuning knob bounded by the vector register budget of the terminal kernel (2-layer L-best is the tightest: ~12 max-log accumulators + ~10 shared ≈ near the RVV 32-register ceiling — see `nr_mimo_lbest_detector.md`). VLEN=256 → TILE=16 REs/lane group; VLEN=1024 → 64; HVX/AIE/NEON pick their own. Larger TILE amortizes loop overhead; smaller TILE eases register pressure. The harness parameterizes it so no kernel hard-codes VLEN.

### 3a. Unification target — one data-channel `inner_rx` (PDSCH / PUSCH / PSSCH)

The tile contract above is deliberately **channel-agnostic**, and that is the point: the same fused `inner_rx` should serve all three NR **data** channels — UE **PDSCH**, gNB **PUSCH**, and Sidelink **PSSCH** (landing in `develop` soon). They share identical signal math (matched filter → MMSE/L-best detect → QAM LLR → descramble) and differ only in *configuration*. Converging them is the biggest single leverage in this effort: each platform team implements `nr_rx_process_tile` **once** and it serves 3 channels × all ISAs, instead of an N-channel × M-ISA matrix of kernels.

**Scope of the union — data channels only.** PDSCH/PUSCH/PSSCH merge onto one `inner_rx`. **PSCCH is control** (a sibling of PDCCH: blind decode, SCI, polar back-end); it can share the *front-end* (extract → compensate → LLR) but not the full pipeline. So the target is "one data-channel `inner_rx`" (+ optionally "one control-channel front-end"), not one function for all four.

**The boundary that keeps the kernel channel-agnostic.** All channel-specific behaviour stays **upstream** (channel estimation, DMRS/RS layout, allocation bitmask) or **downstream** (HARQ / decode). `inner_rx` consumes channel *estimates*, not the reference signals, so DMRS-pattern differences never enter the kernel. The only channel-specifics it needs are already `rx_tile_t` fields or thin additions:

| channel-specific | how it enters the contract |
|---|---|
| RS/DMRS skip pattern | the harness run iterator, not the kernel |
| scrambling seed (RNTI / Nid / SCI-derived) | `scramble_seq` pointer |
| transform precoding (PUSCH only) | flag; terminates the fused span (SC-FDMA) |
| PTRS presence | optional whole-symbol phase pre-pass |
| layer/stream count, MU-MIMO group | `n_layer` (the UL group builds a joint multi-layer PDU today) |

**Sidelink PSSCH looks like a *simpler* instance** — CP-OFDM (no transform-precoding boundary), limited rank, SCI-derived seed. Action: have a Sidelink stakeholder confirm the descriptor covers PSSCH's DMRS pattern and seed derivation **before the contract is frozen**, so no field is discovered missing after the fact.

**Vehicle:** generalize the *existing* gNB `inner_rx` (`nr_ulsch_demodulation.c:224`) — already the closest thing to the target — rather than invent a new one. R1 (un-deferring PDSCH to per-symbol) is the prerequisite that puts DL into the same shape so it can converge here.

---

## 4. Observability taps — preserve at zero cost when disabled

The channel estimates and `rxdataF_comp` (matched-filter output) are exported for scope / analysis. In the fused path `rxdataF_comp` is **never** a full-slot buffer, so the taps must move into the per-tile path and stay free when off.

Existing taps (all already gated so they cost ~nothing when disabled — the fused path must keep it that way):

| Tap | Exports | where | gate today |
|---|---|---|---|
| DL A/C | `dl_ch_estimates_ext`, `rxdataF_ext` → scope | `dlsch_demod.c:856,902` | runtime scope try-lock flag |
| DL F/G/H | **`rxdataF_comp`** → scope + T-tracer + phy_sim | `:1211,1241,1252` | `UEScopeHasTryLock` / `#if T_TRACER` / NULL ptr |
| DL B/I | `rxdataF_ext`, `dl_ch_estimates_ext` → phy_sim (dlsim) | `:871,1261` | NULL pointer (sim only) |
| UL | `rxFext`,`chFext`,**`rxdataF_comp`**,`llr` → scope + T-tracer | `ulsch_demod.c:276,967,984` | `T_ACTIVE(...)` / `gNBTryLockScopeData` |

**Design — the tap contract (`rx_tap_t`):**

```c
typedef struct {              // non-NULL in rx_tile_t only while observation is active
  c16_t   *rxdataF_comp_dst;  // full-slot export buffer (allocated ONLY when observing)
  c16_t   *ch_mag_dst;
  c16_t   *chest_dst;
  uint32_t tile_offset;       // where this tile writes within the export buffer
} rx_tap_t;
```

Rules that keep the disabled path truly free:
1. **Allocate export buffers only when a tap is enabled.** When off, no full-slot buffer exists → no memory, no write. (The export buffer becomes the *only* full-slot buffer, and only in observe mode — acceptable, since observation is not the perf path.)
2. **Hoist the enable test outside the tile loop.** `t->tap` is set once per symbol; the branch inside `nr_rx_process_tile` is perfectly predicted and, when NULL, the compensated tile is simply never stored — the compiler drops the dead store. Two-variant codegen (a `tap`/`no-tap` template instantiation) is the fallback if any platform's predictor can't hide it.
3. **Relocate the `rxdataF_comp` consumers (DL F/G/H, UL scope) into the per-tile tap**, writing the tile slice under the existing try-lock / `T_ACTIVE` guard. The guards already return false/compile out when disabled; only the *write site* moves.
4. **The `*_ext` taps (A/B/C/I)** export buffers R2 dissolves. When a tap is active the harness keeps a thin per-symbol staging slice for just those REs (or the tap writes directly from the tile view); when inactive, nothing is materialized.

Net: **disabled observability = zero extra allocation and zero extra stores**; enabled = one gated tile-slice copy on the (already non-real-time) analysis path.

---

## 5. Phased plan

| Phase | Work | Depends | Verifies |
|---|---|---|---|
| **P0** | This assessment | — | — |
| **P1** | R1: DL un-defer LLR/demap/descramble → per-symbol; shrink 4 full-slot buffers | — | BLER unchanged (functional-equiv); measure DL `rxdataF_comp` traffic drop |
| **P2** | Define `rx_tile_t`/`nr_rx_process_tile`; RVV harness; R2+R3+R5 + **R4 L=1** (delete `nr_dlsch_mmse`/`nr_mmse_2layers`, per-RE solve → single-layer LLR) fuse single-layer & linear-MMSE DL **and** UL per-tile; relocate taps (§4) | P1 | dlsim/ulsim BLER + throughput vs staged; MMSE-config BLER unchanged; scope works when on |
| **P3** | **R4 L>1**: register-resident L-best terminal; 2-layer & 3-layer joint fused (rho / nulling in-tile) | P2 | 2-/3-layer BLER; A100 vs X100 timing (expect A100 to stop losing) |
| **P4** | Parallel teams implement `nr_rx_process_tile` for aarch64 / Hexagon / AIE against the P2 contract | P2 | per-platform kernel tests vs RVV reference |

P1 is worth doing on its own regardless of the tiling work: it's a pure structural change (no new kernels), it's the single biggest full-slot-buffer reduction, it makes DL match UL, and it puts PDSCH in the per-symbol shape needed both for UE symbol-parallelism and for converging onto the shared `inner_rx` — a prerequisite that de-risks everything after.

**Channel convergence** (PDSCH/PUSCH/PSSCH onto one `inner_rx`, §3a) is realized *as P2/P3 land* — the contract is designed for it from the start, so no separate phase is needed for PDSCH↔PUSCH. A Sidelink PSSCH pass follows once SL is in `develop`; the only prep now is having a SL stakeholder validate the `rx_tile_t` fields cover PSSCH before the contract freezes.

---

## 6. Risks / open questions

- **MMSE unification (R4)** — deleting `nr_dlsch_mmse`/`nr_mmse_2layers` in favour of the per-RE L=1 solve moves from per-RB channel averaging to per-RE inversion. Expected equal-or-better (functional-equiv), but confirm BLER on the MMSE-heavy configs (higher-order, 2-layer, correlated channels) and that the per-RE `det` regularization `λ` matches the noise scaling the old routine used.
- **PTRS** — needs the whole-symbol phase estimate first; the split (estimate pre-pass → per-tile apply) must not change results (functional-equiv helps).
- **Transform precoding / SC-FDMA (UL)** — whole-symbol IDFT is a hard tile boundary where memory is unavoidable; the fused span for DFT-s-OFDM PUSCH ends at equalization, LLR runs on the IDFT output.
- **rho / inter-layer** — the 2-layer ML needs rho per tile; the compensation kernel already produces it (`rho` arg) — keep it in-tile rather than the full-slot `rho_dl`.
- **Allocation-run tiling + partial tails (R3)** — the tile iterator must respect RBG-bitmask run boundaries (no tile spans an allocation gap) and the DMRS/CSI-RS skip must reproduce the exact RE selection the extract functions do today. Every run ends in a partial tile — free on RVV (`vsetvl`), but fixed-width SIMD teams (AVX512/NEON/HVX/AIE) must budget masked/scalar tail handling; quantify the tail overhead for the common allocation shapes (small runs = more tails). If per-platform masked loads are too costly, fall back to a thin per-tile gather at the run head.
- **Group MU-MIMO (UL)** — `nr_rx_pusch_group_tp` builds a synthetic multi-layer joint PDU; the tile contract must carry `n_layer` up to the joint layer count.
- **Two-level parallelism (symbols across cores, tiles fused within a symbol)** — UL already dispatches per-symbol-group tasks to a thread pool; the tile loop lives *inside* a task, so fusion is orthogonal to (and composes with) threading. The UE currently lacks symbol-level threading; R1 puts DL in the per-symbol shape that enables it, so plan R1 to leave a clean per-symbol task boundary the UE thread-pool dispatch can later wrap — same structure as `nr_pusch_symbol_processing`.

---

*Companion docs: `RVV_PORT_ASSESSMENT.md` (SIMD port scope/gotchas), `nr_phy_common/src/nr_mimo_lbest_detector.md` (L-best detector + the memory-bound-on-A100 evidence that motivates this work).*
