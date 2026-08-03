<!-- SPDX-License-Identifier: LicenseRef-CSSL-1.0 -->
# inner_rx compute-fusion — timing

Tracks the inner_rx (channel compensation + detector/LLR) cost, **unfused vs fused**, so we
can compare across platforms/bandwidths **now (tiled fusion, L1 scratch)** and **after the
register-fusion step** (no scratch: one read → compute → one write).

## Method

A/B on the same binary via the `OAI_FUSE` gate (isolates the fusion; no other change):
```
OAI_FUSE=0 ./nr_dlsim -n300 <cfg> -E -P     # unfused: COMP + LLR run separately
OAI_FUSE=1 ./nr_dlsim -n300 <cfg> -E -P     # fused:   COMP skipped (~0), fused kernel in LLR
# metric = DLSCH_CHANNEL_COMPENSATION_STATS + DLSCH_LLR_STATS  (fused: LLR only)
```
- `-P` enables cpu_meas + prints "UE function statistics". `-E` = do_ml (near-ML path).
- gNB side: `nr_ulsim -y2 -W2 -z2 -P` (`-y` n_tx, `-W` layers, `-z` n_rx).
- Times are **per per-symbol invocation** (trials = n_slots × ~13 PDSCH data symbols/slot).
  **Per slot ≈ 13×.** A 30 kHz slot is 500 µs.
- The fusion saves the compensation's memory round-trip → benefit ∝ `compensation / total`.
- **x86 is cache-rich → the round-trip it eliminates is cheap here; the numbers are the muted
  case.** The memory-bound targets (RK3588 A76, NXP A72, K3 A100) should show larger deltas,
  and there the near-ML LLR itself is memory-bound (~4× slower on A100).

## x86 (dev machine), 2×2, TDL, fixed OAI_RNGSEED — µs per per-symbol

### 40 MHz — R106 (1272 REs/symbol)

| config | unfused COMP+LLR | fused (LLR) | delta | fused/slot (×13) |
|---|---|---|---|---|
| 1-layer 16QAM (-e14) | 0.37 + 0.18 = 0.55 | 0.47  | ~15% | ~6 µs |
| 2-layer QPSK         | 1.13 + 1.22 = 2.35 | 1.80  | ~23% | ~23 µs |
| 2-layer 64QAM full-ML| 1.39 + 35.95 = 37.34 | 36.86 | ~1.3% | **~479 µs** |
| 2-layer 256QAM L-best| 1.40 + 34.32 = 35.72 | 35.22 | ~1.4% | ~458 µs |

### 100 MHz — R273 (3276 REs/symbol)

| config | unfused COMP+LLR | fused (LLR) | delta | fused/slot (×13) |
|---|---|---|---|---|
| 1-layer 16QAM (-e14) | 0.95 + 0.50 = 1.45 | 1.16  | ~20% | ~15 µs |
| 2-layer QPSK         | (run stalled — TODO) | | | |
| 2-layer 64QAM full-ML| 3.97 + 97.51 = 101.48 | 97.56 | ~3.9% | **~1268 µs** |
| 2-layer 256QAM L-best| 3.76 + 62.52 = 66.28 | 66.79 | ~-0.8% (noise) | ~868 µs |

### Findings (x86)
- **Fusion benefit ∝ compensation/total**: big where the LLR is cheap (1-layer, 2-layer QPSK →
  15–23%), tiny where the near-ML LLR dominates (~1–4%, near x86 noise).
- **Grows with bandwidth** (1-layer 15%→20%, 2-layer 64QAM 1.3%→3.9% from 40→100 MHz): more REs
  ⇒ more compensation round-trip saved. Confirms the memory-traffic hypothesis; expect stronger
  on memory-bound targets.
- **The near-ML LLR is the wall, and it is compute-bound**: 2-layer full-ML is ~479 µs/slot @40 MHz
  and **~1268 µs/slot @100 MHz** — over a 500 µs slot on one core. Fusion barely touches it; that
  is where the **L-best reduced-search** (256QAM L-best already ~35% cheaper than 64QAM full-ML
  @100 MHz) and the eventual **register-fusion** matter.

## aarch64 / RISC-V — TODO (fused kernels now native-w128 on aarch64; RVV via SIMDe until realign)

| platform | config | unfused | fused | delta | notes |
|---|---|---|---|---|---|
| RK3588 A76 (UE, Rock 5A) | 2L-64QAM full-ML @40 MHz | 29.71+479.76=509.47 | 496.72 | ~2.5% | native w128; **BLER/BER identical fused vs unfused (validated on real NEON)**; comp cost **-43%** when fused (29.71→16.96 absorbed = memory round-trip saved), but full-ML LLR dominates (~497 µs/sym ≈ **6.5 ms/slot**, ~13× over 500 µs → needs L-best reduced-search). TODO: measure 1-layer / 2L-QPSK here where fusion should shine (compensation a bigger fraction). |
| RK3588 A76 (UE, Rock 5A) | 2L-64QAM **L-best** (PAT=1) @40 MHz | 29.87+125.62=155.49 | 142.33 | **~8.5%** | native w128; BER identical fused vs unfused (real NEON); L-best LLR ~3.4× cheaper than full-ML (126 vs 480 µs/sym); same ~44% comp cut (29.87→16.71) but now a bigger share → bigger overall %. ~1.85 ms/slot (viable with per-symbol threading across cores). |
| RK3588 A76 (UE, Rock 5A) | 2L-64QAM full-ML @100 MHz | 75.12+1239.64=1314.76 | 1282.29 | ~2.5% | comp 2.5× vs 40 MHz, LLR 2.6× → **delta bandwidth-invariant** on this memory-bound core; ~43% comp cut (42.6 absorbed vs 75.1). BER identical. |
| RK3588 A76 (UE, Rock 5A) | 2L-64QAM **L-best** (PAT=1) @100 MHz | 75.37+333.15=408.52 | 375.77 | **~8.0%** | same ~43% comp cut; delta ≈ 40 MHz L-best (8.5%). L-best LLR ~3.7× cheaper than full-ML. BER identical. |
| RK3588 A76 (UE, Rock 5A) | 1-layer 64QAM @100 MHz, 2 Rx | 20.96+5.50=26.46 | 24.54 | ~7.3% | comp cut only ~9% (19.04 absorbed vs 20.96) — **no rho to save** + per-tile LLR-call overhead vs a very cheap 5.5 µs LLR. Register-fusion (inline LLR, no per-tile call/scratch) should lift this. BER identical. |
| GH200 Grace (gNB, Neoverse-V2) | 2L-64QAM **L-best** (PAT=1) @100 MHz, 4 Rx | 672.23+1956.00=2628.23 | 2388.18 | **~9.1%** | `nr_ulsim -y2 -z4 -W2 -R273 -m25`, per slot (12 sym). Big out-of-order aarch64 server core = **the other end** of the A76. Comp **~98.6% absorbed** (672.23→9.47) — far more than A76's ~43%: the V2 is bandwidth-constrained relative to its huge compute, so the comp DRAM round-trip is proportionally much costlier. LLR rises 1956→2379 (comp *compute* moves in) but the ~250 µs round-trip vanishes → net -240 µs. BER ~2.2e-4 both (seeds differ — timing run, not a bit-exact check). Tiled fusion; register variant not built (see below). |
| GH200 Grace (gNB, Neoverse-V2) | 2L-64QAM **full-ML** @100 MHz, 4 Rx | — | 9.31+6611.06=6620.37 | — | fused-only reference. Full-ML LLR **6611** vs L-best **2379** → **L-best 2.78× cheaper** on this core, *less* than the A76's ~3.7×: the big OoO V2 hides part of full-ML's extra candidates behind ILP, so candidate reduction pays less here. Detector compute is the wall (6.6 ms/slot ≈ 13× a 500 µs budget; L-best 2.4 ms ≈ 4.8×) — only reduced-search moves it, not fusion (comp fully absorbed, 9.31 µs). |
| NXP A72 (gNB)     | | | | | native w128 |
| K3 X100 (RISC-V)  | | | | | SIMDe (not native RVV) |
| K3 A100 (RISC-V)  | | | | | SIMDe; LLR ~4× slower here |

## Register-fusion (`OAI_FUSE=2`, commit b361b05971)
Per-block MRC + magnitudes + LLR computed in registers, LLR stored directly (compile-time-index
extracts, no stack spill) — no L1 tile scratch and no per-tile LLR call. Bit-exact with unfused
and with the tiled fusion (`OAI_FUSE=1`), all mod orders, w256 and w128. A/B is the same gate:
```
OAI_FUSE=1 ./nr_dlsim -n300 <cfg> -P   # tiled fusion  (L1 scratch + per-tile call)
OAI_FUSE=2 ./nr_dlsim -n300 <cfg> -P   # register fusion (no scratch, no call)
# 1-layer decider cfg: -s24 -S25 -R273 -b273 -e17 -x1 -y1 -z2   (64QAM, 2 Rx, 100 MHz)
```

### x86 (dev machine) — register vs tiled, 1-layer 64QAM @100 MHz
| variant | fused LLR |
|---|---|
| tiled (FUSE=1)    | 1.91 µs |
| register (FUSE=2) | 2.17 µs (~+14%) |

**x86 says register LOSES to tiled — but x86 cannot decide this.** Interleaving compensation+LLR
per block raises register pressure / hurts scheduling; on cache-rich x86 that cost exceeds the
(nearly free) L1 tile-scratch the register form removes. On the memory-bound A76/A100 the scratch
round-trip is more expensive (bigger saving) **but** the core is more register-constrained (bigger
pressure cost) — the two effects pull opposite ways, so the register-fusion must be measured on
the A76 to know if it's worth keeping over the tiled fusion.

### aarch64 (RK3588 A76) — DECIDED: register is a wash, keep tiled
1-layer 64QAM @100 MHz, 2 Rx, `OAI_RNGSEED=888`, BER bit-identical (1.592530e-04) both:

| variant | fused LLR |
|---|---|
| unfused (ref, earlier) | 26.46 µs |
| tiled (FUSE=1)    | 24.46 µs (~7.5% vs unfused) |
| register (FUSE=2) | 24.54 µs (+0.3% vs tiled — noise) |

**Verdict: register-fusion does NOT beat the tiled fusion.** On the memory-bound A76 it's a wash
(+0.3%); on cache-rich x86 it's ~14% slower. The tiled fusion already captured the whole
memory-round-trip win (26.46 → 24.46); eliminating the scratch + per-tile call on top adds nothing
for 1-layer, because 1-layer's scratch is tiny (rxComp + mag, **no rho**) and the LLR is only
~24 µs. **Keep the tiled fusion as the default;** the register path stays gated (`OAI_FUSE=2`,
bit-exact) but is not a win for single-layer.

### 2-layer register-fusion — NOT built (evidence says wash), decided 2026-08-02
Considered extending register-fusion to 2-layer, where the scratch is bigger (rxC0/1 + mag0/1 +
rho01/10, six arrays vs the single layer's three). Reading the actual detectors settled it without
building:
- The 2-layer detectors are **compute-bound, not memory-bound on their scratch**. The L-best 64QAM
  kernel (`nr_qam64_llr_2layer_lbest_q15_simd`, the default hot path) does **5 input loads** per
  16-RE iteration (z1, z2, rho, cm0, cm1 via `nrlbw_load`) followed by **~100+ SIMD ops** — seed,
  a 9-candidate metric grid, per-axis max reductions, LLR pack. Full-ML (`nr_qam64_llr_2layer`)
  does far more. Input loads are <5% of the work; register-fusing them saves <5%.
- Corroborated by the ~3.4× speedup L-best gets from candidate reduction on the A76 (a
  memory-bound kernel wouldn't speed up that much from doing less *compute*). The
  "L-best is memory-bound (~4× on A100)" observation is a **RISC-V/SIMDe** artifact (256-bit
  emulated as 2×128 + narrow datapath), not an A76 property.
- The **tiled fusion already removed the DRAM round-trip** for all six scratch arrays; register-
  fusion only removes the residual **L1** load — and the 1-layer A76 test measured that L1-load
  elimination at **~0** (wash). 2-layer does 15–50× more detector compute per RE, so the L1-load
  fraction is *smaller*, not bigger ⇒ 2-layer register-fusion is a wash with higher confidence
  than 1-layer.

Conclusion: **register-fusion does not help 2-layer either.** The tiled fusion is the shipping form
for both 1- and 2-layer; the 1-layer register path stays gated (`OAI_FUSE=2`, bit-exact) as a
reference but is not a win. The near-ML/L-best detector compute is the wall — only reduced-search
(L-best: ~3.4× on A76) moves it, not fusion.
