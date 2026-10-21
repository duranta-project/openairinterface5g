# Reduced-complexity soft-output MIMO detection (L-best / partial marginalization)

Design note for the L-best 2-layer kernels and the >2-layer hybrid detector in
`nr_compute_llr.c`. Self-contained derivation so the reasoning is version-controlled
and not dependent on any external notes/session. Sections 1–5 are the algorithm;
Section 7 is the production implementation (fixed-point, the 128/256/512-bit SIMD
family, gating, and validation).

## 1. System model and ML metric

Per resource element (RE), after the matched filter / channel compensation:

- Received model `y = H x + n`, with `H = [h_1 ... h_N]` (N transmit layers),
  `x_i` drawn from a unit-energy square QAM constellation, `n` white (var σ²).
- Matched-filter (MRC) outputs `z_i = h_i^H y` (these are `rxdataF_comp[layer]`).
- Gram matrix `R = H^H H`, entries `ρ_ij = h_i^H h_j` (`ρ_ii = ||h_i||²`, real).
  (These are `rho_dl[i][j]`; the kernels recover `ρ_ii` from the `n1`-scaled `ch_mag`.)

Dropping the constant `||y||²`, the per-hypothesis **max-log metric** (½ convention,
larger = better) is

```
M(x) = Σ_i [ Re{x_i* z_i} − ½ ρ_ii |x_i|² ]  −  Σ_{i<j} Re{ ρ_ij x_i* x_j }
```

The bit LLR of layer `t` is `max_{x_t: bit=0} g(x_t) − max_{x_t: bit=1} g(x_t)`, where
`g(x_t) = max over the other layers of M`. Everything below is about how to compute
`g(x_t)` cheaply.

## 2. The conditional slice (2-layer building block)

For two layers (target `x_1`, nuisance `x_2`), the inner `max` over `x_2` is **exact in
O(1)**: for a fixed `x_1`, the best `x_2` is the nearest constellation point to the
conditional estimate

```
x_2*(x_1) = Q_S( (z_2 − conj(ρ_12) x_1) / ρ_22 )          (Q_S = slice to nearest QAM point)
```

So `g(x_1) = max over candidate x_1 of d(x_1, x_2*(x_1))`. Enumerating all `M` values of
`x_1` (M = points/layer) gives **exact 2-layer ML at O(M)** (not O(M²)). Taking only the
`L ≤ M` candidates nearest a linear (ZF/MMSE) seed gives the reduced **L-best** search.
This is `nr_qam{16,64,256}_llr_2layer_lbest` (float reference) and, for **64QAM and
256QAM**, the production fixed-point/SIMD kernels `nr_qam{64,256}_llr_2layer_lbest_q15_simd16`
(§7).

## 3. The continuous approximation collapses to ZF (for any N)

If, instead of slicing the nuisance to the discrete alphabet, we minimize the metric over
**continuous** nuisance `x_rest`, we get an orthogonal projection:

```
min_{x_rest ∈ ℂ^{N-1}} ||y − h_1 x_1 − H_rest x_rest||² = ||P⊥ (y − h_1 x_1)||²,
   P⊥ = I − H_rest (H_rest^H H_rest)^{-1} H_rest^H
```

which reduces to a **single-stream** metric in `x_1` with

```
α = ρ_11 − ρ_{1,rest} R_rest^{-1} ρ_{rest,1}     (Schur complement = ||P⊥ h_1||²)
ζ = z_1  − ρ_{1,rest} R_rest^{-1} z_rest
x̂_1 = ζ / α      (= the ZF / decorrelator estimate of x_1)
```

i.e. **the continuous approximation IS zero-forcing, for any number of layers.** It carries
no gain over a plain linear receiver. The value at N ≥ 3 comes only from keeping *some*
layers discrete — the hybrid below.

## 4. The N ≥ 3 hybrid: selective deflation

Split the nuisance into a **discrete head** (`k` layers kept and searched/sliced) and a
**continuous tail** (the rest, projected away). Pure-ZF is `k=0`; exact ML is `k=N−1`.
The useful middle is small `k`.

**Which layers to project (the selection criterion).** Projecting layer `j` costs only the
loss of effective channel energy `α`; for a single layer that loss is

```
Δα_j = |ρ_1j|² / ρ_jj = ρ_11 · cos²θ_1j ,   cos²θ_1j = |ρ_1j|² / (ρ_11 ρ_jj)
```

— i.e. **the normalized correlation (angle) between the target and layer `j`, NOT its raw
power.** So **project the layers most ORTHOGONAL to the target** (ZF-nulling them is nearly
free) and **keep the most ALIGNED ones discrete** (where the finite-alphabet ML gain over ZF
is largest). This per-RE orthogonality-based selection is the part that appears
under-documented vs. the SNR-ordered subset selection in the cited literature.

**N=3, k=1 (project one layer `c`, keep `d` discrete).** The projected set is a single layer,
so the deflation is a **scalar** Schur complement (one real reciprocal `1/ρ_cc`, no matrix
inverse):

```
z_i'  = z_i  − (ρ_ic / ρ_cc) z_c                 for i ∈ {t, d}
ρ_ij' = ρ_ij − ρ_ic conj(ρ_jc) / ρ_cc            for i,j ∈ {t, d}
```

Then run the 2-layer conditional-slice LLR (Section 2) on the deflated `(t, d)` pair. The
2-layer kernel's own ZF seed on the deflated inputs equals the **full N-layer ZF** estimate
of `x_t` (Schur quotient property), so candidate centring is correct automatically.
This is `nr_qam_llr_3layer_hybrid`; `nr_qam_llr_3layer_ml` is the exact reference
(`max` over discrete `(x_t, x_n1)` with conditional `x_n2` slice).

**N=4** is the same with **successive scalar deflations** (nested Schur): project two tail
layers one at a time (each a scalar `1/ρ_cc` on the running-deflated Gram) down to a 2-layer
problem — never needs an explicit matrix inverse.

## 5. Complexity (per target layer, per RE; M = points/layer)

| scheme | cost | note |
|---|---|---|
| MMSE (linear) | ~O(1) | |
| 2-layer L-best (L candidates) | O(L) | conditional slice keeps it linear |
| 2-layer exact ML (L = M) | O(M) | |
| N=3 hybrid, k=1 | O(M) | scalar deflation + 2-layer; **not O(M²)** |
| N=3 exact ML reference | O(M²) | enumerate two layers jointly |
| QPSK N=3 / N=4 exact | O(16) / O(64) | small enough to stay exhaustive (M=4) |

The conditional slice is what decouples per-candidate cost from constellation order, so at a
fixed `L` the hybrid *operation count* is roughly the same for 16/64/256QAM; only the candidate
count and the output-bit packing differ. These are op-counts, **not wall-clock** — see §7 for
which kernels are fixed-point/SIMD (production: 2-layer 64QAM and 256QAM) vs scalar `float`
(analysis-only: the 16QAM 2-layer and all >2-layer references), and §7c for measured wall-clock.

## 6. Measured gains (channel dependence)

**Methodology (important).** ML is enabled by the dlsim `-E` flag (`ue->do_ml`); `OAI_LBEST`
only selects the *kernel variant*. Both are required — without `-E` the receiver silently runs
MMSE. Compare at a **well-scaled tx amplitude** (`-Q30`, ~2× the low `-Q36` default): the MMSE
path is fixed-point (int16) while the ML kernels are float, so at low amplitude MMSE is
quantization-limited and the comparison is unfair.

**3-layer, TDL-A, 4T4R, `-Q30`, SNR @ 70% throughput** (PRELIMINARY, low-n ~100–300; `-n1000`
pending):

| Mod. (MCS) | Hybrid | Full-ML | MMSE | Hybrid vs MMSE | Full-ML vs MMSE | Hybrid loss |
|---|---|---|---|---|---|---|
| 16QAM (12) | 11.3 dB | 11.1 dB | 12.8 dB | **1.5 dB** | 1.7 dB | 0.2 dB |
| 64QAM (19) | 16.7 dB | 16.6 dB | 18.6 dB | **1.9 dB** | 2.0 dB | 0.1 dB |

Takeaways: the ML-over-MMSE gain is a real **~1.5–2.0 dB** and grows with constellation order;
the single continuous ZF deflation in the hybrid costs only **~0.1–0.2 dB** vs exact full-ML, so
the cheap O(M) hybrid is the right production kernel. (Earlier much-larger figures — e.g. ~5 dB on
the "fake" channel — were confounded by a missing `-E` and/or the low-amplitude fixed-point-MMSE
artifact and are retracted; 2-layer and 256QAM gains need re-measuring under `-E`/`-Q30`.)

**Rank/conditioning:** 256QAM with ≥2 layers needs ≥4 RX; below that neither ML nor MMSE converges
on TDL — physics, not a detector issue.

### 6a. Reduced-search (L-best) viability — resolved

An earlier verdict held that the 2-layer reduced search (`L < M`) loses several dB on correlated
TDL channels and is usable only on low-correlation / flat channels. **That verdict was an
LLR-scaling artifact.** Those measurements used *cool* LLR scaling, which depresses BLER regardless
of the search. With the hot `-2` ML `log2_maxh` (§7a) the reduced search is faithful on 3GPP TDL:
the float ground-truth comparator measures `<0.35%` sign disagreement (64QAM) between the
9-candidate (3×3) reduced search and exact full-ML on TDL-A, and the **256QAM 3×3/9-candidate
L-best matches the full 256-point max-log ML in coded BLER on TDL-A (verified UE + gNB)** — a
~28× candidate reduction with no BLER loss. So the 3×3 reduced search is the ML default for both
64QAM and 256QAM (`OAI_LBEST_PAT`=0 / `OAI_LBEST_PAT256`=1), not a flat-channel-only option.

The one real caveat is the empty-subset fallback: when a bit value has no candidate in the searched
window, the LLR must come from elsewhere. 64QAM uses a saturating `±NR_LBEST_Q_LLR_SAT` (rarely
fires from a 3×3 window). 256QAM's coarse/MSB bits are *never* spanned by a local 3×3 window, so a
hard saturation there would be systematically wrong — this is what the **graded-MSB fallback**
(§7b) fixes.

## 7. Implementation / gating

All gated behind `OAI_LBEST` (default off → production full-search/MMSE, unchanged). Wired into
**both** RX directions — UE RX (downlink PDSCH: `nr_dlsch_demodulation.c`, `phy_procedures_nr_ue.c`)
and gNB RX (uplink PUSCH: `nr_ulsch_demodulation.c`) — via the shared kernels in `nr_compute_llr.c`.

- `OAI_LBEST=1` — master switch.
- `OAI_LBEST_PAT` — 64QAM 2-layer candidate pattern (0=3×3 [ML default], 1=6-cand, 2=5-plus).
- `OAI_LBEST_PAT256` — 256QAM 2-layer candidate set (0=5×5/25, **1=3×3/9 [ML default]**, 2=5-plus, 3=full 16×16 ML).
- `OAI_LBEST3` (1=hybrid, 2=full-ML ref) and `OAI_LBEST_L3` — 3-layer.
- `OAI_LBEST_W512`/`_W256`/`_W128` — x86 SIMD-width override for A/B timing (see §7b).
- `OAI_LBEST_DBG` / `_DBG256` — float-reference comparator, analysis only (§7a).

**Kernel implementation status.** The **2-layer 64QAM and 256QAM** paths are production-grade
fixed-point/SIMD: the reduced-search `nr_qam{64,256}_llr_2layer_lbest_q15_simd16` (both with a
128/256/512-bit SIMD family, §7b) plus the int16 full-search `nr_qam64_llr_2layer`. The
`nr_qam{16,64,256}_llr_2layer_lbest` **float** kernels are the reference/validation model. The
2-layer **16QAM** path (`nr_qam16_llr_2layer`) and all **>2-layer** kernels
(`nr_qam_llr_3layer_hybrid` / `_ml`) remain scalar `float`, analysis-only — SIMD-izing the 3/4-layer
hybrid is future work (§9).

### 7a. LLR scaling and the analysis knobs

The LDPC decoder is **8-bit**: the int16 demod LLRs are narrowed to int8 by a saturating pack to
±127 (`nrLDPC_coding_segment_decoder.c`, `simde_mm_packs_epi16`, no down-shift), and every offload
backend is 8-bit too. So the LLR magnitude that reaches the decoder is set upstream by the output
shift `log2_maxh` (`nr_dlsch_demodulation.c`), which is **mode-specific**:

```
nl==1:            (log2_approx(avgs)>>1) + 1 + log2_approx(nbRx>>1)   // single-layer: +1 guard
nl>1, MMSE:       (log2_approx(avgs)>>1)     + log2_approx(nbRx>>1)   // multi-layer linear: +0
nl>1, ML (do_ml): (log2_approx(avgs)>>1) - 2 + log2_approx(nbRx>>1)  // multi-layer ML:     -2
```

A *smaller* `log2_maxh` = less right-shift = larger LLRs. The ML branch's empirical **`-2`** runs
the ML LLRs hot on purpose, and a dlsim sweep confirms it is near-optimal: colder loses, hotter
(`-3`) loses, and much hotter (`-4`) **overflows the int16 channel compensation and fails
completely**. The window is narrow; `-2` sits in it. (All ML modes share this branch, so the L-best
and full-search kernels get the *same* input scale — differences between them come from the kernels,
not `log2_maxh`.)

Two **analysis-only** knobs are left in the tree for retuning/verification (both default to the
current behaviour, so a normal build is unchanged):

- **`OAI_ML_MAXH_OFF=<n>`** (`nr_dlsch_demodulation.c`, default `-2`) — overrides the ML-branch
  `log2_maxh` offset above, for sweeping ML LLR hotness without a rebuild.
- **`OAI_LBEST_DBG=1`** / **`OAI_LBEST_DBG256=1`** (`nr_compute_llr.c`, 64QAM / 256QAM 2-layer) —
  recomputes layer-0 LLRs with the float reference (`nr_qam{64,256}_llr_2layer_lbest`) on the *same*
  inputs and prints, per ~200k LLRs: sign disagreement, mean |LLR|, int8-clip fraction, best-fit scale
  vs the reference, and post-scale residual. This is the ground-truth check for the fixed-point kernels
  (the same statistic the ctest in §7d applies). After the graded-MSB fallback (§7b), 256QAM tracks the
  reference cleanly (`fitScale≈1.0`, low residual); 64QAM's coarser 3×3 window still runs somewhat hot.

### 7b. Width-parameterized SIMD family (128 / 256 / 512-bit) and the 256QAM fallback

The two production kernels are written **once** and instantiated at three vector widths, so the same
source compiles to NEON (aarch64), AVX2, and AVX-512:

- `nr_lbest_simd_width.h` — the width abstraction: for `NRLB_W ∈ {128, 256, 512}` it defines the
  vector types, the `NRLB_MM(op)` intrinsic prefix, the name mangling (`_w128/_w256/_w512`), and the
  cross-lane helpers (`widen`/`pack32`/`load` deinterleave / `store_llr` transpose).
- `nr_lbest_qam{64,256}_simd.c.inc` — the kernel bodies, elementwise ops via `NRLB_MM(...)`,
  cross-lane via the `nrlbw_*` helpers. `nr_compute_llr.c` includes each `.inc` once per width.

**Runtime selection** (`nr_lbest_simd_width_mode`, announced once on stderr): aarch64 runs **w128**
natively (SIMDe→NEON, avoiding the inefficient 256→2×128 emulation); x86 runs **w256** by default,
with **`OAI_LBEST_W512=1`** opting into AVX-512 (compiled in only when `__AVX512{F,BW,VL}__` is a build
target). `OAI_LBEST_W256/_W128` downgrade for A/B timing or NEON regression on x86. **All widths are
bit-exact** — verified by the ctest (§7d) and the deterministic gNB (identical BER across widths).

**AVX-512 specifics.** AVX-512 breaks the 128/256 vector-mask model: integer/float compares return
k-masks (lifted back to vectors with `movm`), there is no vector-mask `blendv` (emulated with
`movepi8_mask` + `mask_blend`, i.e. `vpmovb2m`+`vpblendmb`), and `round_ps`→`roundscale_ps`. These are
localized in the width header's `NRLB_BLENDV/CMPGT16/CMPEQ16/CMPPS_SI/ROUNDPS` wrappers so the `.inc`
bodies are unchanged. w512 is **opt-in** because the gain is datapath-dependent (§7c).

**256QAM graded-MSB fallback.** A local 3×3 window around the seed spans the fine (LSB) bits of a
PAM-16 axis but *never* the coarse/MSB bits. Rather than emit a hard `±sat` for an unspanned bit
(systematically wrong), the kernel emits a **graded per-axis LLR** `g·[(l1*−e)² − (l0*−e)²]` computed
in closed form from the soft seed estimate `e` (nearest level with the bit = 0 vs = 1). This is what
makes the 3×3/9-candidate 256QAM search match full 256-point ML in BLER.

### 7c. Measured performance

Single-thread `RX PUSCH LLR`, 2-layer, 273 PRB, gNB ulsim. **L-best vs full-ML** (the candidate
reduction; ISA-independent gain — larger on narrower/slower cores):

| Box | ISA | full-ML | L-best | speedup |
|---|---|---|---|---|
| EPYC Turin 9575F | AVX2 (w256) | 1192 µs | 837 µs | 1.4× |
| GH200 (Neoverse V2) | NEON (w128) | 6187 µs | 1966 µs | 3.1× |
| DGX Spark (GB10) | NEON (w128) | 12648 µs | 3336 µs | 3.8× |

**AVX-512 (w256 → w512)**, single-thread LLR line: the gain is real only on a **true 512-bit
datapath**, and is capped for 256QAM by the k-mask emulation (§7b):

| Part | datapath | 64QAM | 256QAM |
|---|---|---|---|
| EPYC Turin 9575F | true 512 | 1.26× | 1.05× |
| Ryzen AI MAX+ 395 (Strix Halo) | 2×256 double-pump | 1.13× | ~1.0× (wash) |

At full core load (Turin −C8, whole RX-PUSCH wall) the 64QAM w512 gain dilutes to ~1.14×. Hence w512
is opt-in, not default. **Real-time:** the gNB is TDD with dominant DL, so the budget per UL slot is
the UL-slot *period* (≈2.5 ms for DDDSU @30 kHz), not one 500 µs slot — DGX Spark −C8 (RX-PUSCH
1.8 ms wall) fits, and Turin (−C8 total gNB RX 500 µs) has large margin; LDPC offloads to GPU on
GPU-equipped boxes.

### 7d. Validation

- **ctest `test_llr_2layer`** (`openair1/PHY/NR_TRANSPORT/tests/`, `-DENABLE_TESTS=ON`): (A) **width
  equivalence** — `w128 == w256 == w512` bit-identical (memcmp) for 64/256QAM and all patterns; (B)
  **fidelity vs the float model** — the §7a best-fit-scale / residual / sign-disagreement statistic on
  clean constellation inputs (observed: 64QAM 0 % sign flips, residFrac 0.29; 256QAM 0 %, residFrac 0).
- **Deterministic gNB ulsim** (`OAI_RNGSEED`) + `OAI_LBEST_DBG/_DBG256` — the on-real-channel check,
  used to confirm width bit-exactness and float-model tracking end-to-end.

## 8. References

Partial-marginalization soft MIMO detection lineage (DOIs to re-verify):

- **[PM]** E. G. Larsson, J. Jaldén, "Fixed-Complexity Soft MIMO Detection via Partial
  Marginalization," IEEE Trans. Signal Process., 56(8):3397–3407, 2008.
  doi:10.1109/TSP.2008.925260
- **[PM-HO]** D. Persson, E. G. Larsson, "Partial Marginalization Soft MIMO Detection with
  Higher-Order Constellations," IEEE Trans. Signal Process., 59(1):453–458, 2011.
  doi:10.1109/TSP.2010.2068293
- **[SUMIS]** M. Čirković, E. G. Larsson, "SUMIS: Near-Optimal Soft-In Soft-Out MIMO
  Detection with Low and Fixed Complexity," IEEE Trans. Signal Process., 2014.
- **[BCH]** D. W. Waters, J. R. Barry, "The Chase Family of Detection Algorithms for MIMO
  Channels," IEEE Trans. Signal Process., 56(2):739–747, 2008. doi:10.1109/TSP.2007.911315
- **[LSD]** B. M. Hochwald, S. ten Brink, "Achieving Near-Capacity on a Multiple-Antenna
  Channel," IEEE Trans. Commun., 51(3):389–399, 2003. doi:10.1109/TCOMM.2003.809789
- **[VB]** P. W. Wolniansky, G. J. Foschini, G. D. Golden, R. A. Valenzuela, "V-BLAST...,"
  Proc. URSI ISSSE, 1998.

Apparently under-documented vs. the above (potential novelty): (i) the per-RE
orthogonality-based layer-selection criterion (Section 4) and (ii) coded (LDPC) BLER
evaluation on 3GPP TDL/CDL channels (the literature is mostly uncoded BER on i.i.d. Rayleigh).

## 9. Open items

Done since the first draft: production fixed-point/SIMD **256QAM** 2-layer kernel; the
**128/256/512-bit** SIMD family incl. **AVX-512**; graded-MSB 256QAM fallback; wiring into the
**gNB RX (PUSCH)**; the `test_llr_2layer` ctest.

Remaining:
- **>2-layer SIMD**: the 3/4-layer hybrid is still scalar float — SIMD-ize it (deflate → the existing
  2-layer SIMD kernel). Then the next PR: 4-layer (nested deflation), QPSK/16QAM 3/4-layer coverage.
- **Interferer-slice approximation** (2-layer): the one remaining lever to cut per-candidate cost
  further; backend-agnostic (helps NEON most). Not bit-exact — needs TDL BLER validation before default.
- **256QAM AVX-512**: only ~1.05× on true-512 (§7c) because compares/blends round-trip through vectors;
  a native-k-mask reduction would lift it toward the 64QAM ratio (do only if a 256QAM-512 load needs it).
- **QPSK/16QAM 2-layer**: no float reference exists, so they lack the §7d validation — add float refs
  first, then the same fidelity test applies.
- **Eval methodology**: closed-loop PMI in nr_dlsim (and TPMI for nr_ulsim); audit UE PMI and gNB TPMI
  estimation + usage (both suspected suboptimal) — upstream of the detector work.
