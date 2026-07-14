# Reduced-complexity soft-output MIMO detection (L-best / partial marginalization)

Design note for the L-best 2-layer kernels and the >2-layer hybrid detector in
`nr_compute_llr.c`. Self-contained derivation so the reasoning is version-controlled
and not dependent on any external notes/session.

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
This is `nr_qam{16,64,256}_llr_2layer_lbest` (float) and `nr_qam64_llr_2layer_lbest_q15`
/ `_simd16` (fixed-point/SIMD), and the generic `nr_lbest_2layer_re`.

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
fixed `L` the hybrid cost is roughly the same for 16/64/256QAM; only the candidate count and
the (cheap, vector-aligned for 256QAM) output packing differ.

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

## 7. Implementation / gating

All gated behind `OAI_LBEST` (default off → production full-search/MMSE, unchanged). Wired in
the **UE RX (downlink PDSCH)** path only so far (`nr_dlsch_demodulation.c`,
`phy_procedures_nr_ue.c`); the **gNB RX (uplink PUSCH)** is a TODO (kernels are shared).

- `OAI_LBEST=1` — master switch.
- `OAI_LBEST_PAT` — 64QAM 2-layer candidate pattern (0=3×3 full-BLER, 1=6-cand, 2=5-plus).
- `OAI_LBEST_L256` — 256QAM 2-layer candidate count (default 256 = full ML).
- `OAI_LBEST3` (1=hybrid, 2=full-ML ref) and `OAI_LBEST_L3` — 3-layer.

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
- **`OAI_LBEST_DBG=1`** (`nr_compute_llr.c`, 64QAM 2-layer case) — recomputes layer-0 LLRs with the
  exact float full-ML reference (`nr_qam64_llr_2layer_lbest`, L=64) on the *same* inputs and prints,
  per ~200k LLRs: sign disagreement, mean |LLR|, int8-clip fraction, best-fit scale vs the reference,
  and post-scale residual. Used to prove the fixed-point kernels against ground truth — it showed the
  legacy full-search is faithful (`fitScale≈1.0`) while the reduced-search L-best runs hot, mostly via
  its overconfident ±`NR_LBEST_Q_LLR_SAT` empty-subset fallback rather than a clean scale factor.

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

- Kernels: QPSK 3/4-layer (exhaustive); 16QAM 3/4-layer (hybrid; is reduced-L worthwhile?);
  64QAM 4-layer (nested deflation); 256QAM multi-layer (likely ≥4RX + MMSE — verify first).
- SIMD the 3-layer hybrid (deflate → existing SIMD 2-layer kernel for 16/64QAM).
- Wire the same hooks into the gNB RX (uplink PUSCH), 2- and 4-layer.
- Eval methodology: closed-loop PMI in nr_dlsim (and TPMI for nr_ulsim); audit UE PMI and
  gNB TPMI estimation + usage (both suspected suboptimal) — upstream of the detector work.
