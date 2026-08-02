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
| NXP A72 (gNB)     | | | | | native w128 |
| K3 X100 (RISC-V)  | | | | | SIMDe (not native RVV) |
| K3 A100 (RISC-V)  | | | | | SIMDe; LLR ~4× slower here |

## Re-measure after register-fusion
Same A/B; the register-fusion should shrink the fused-LLR column further (no L1 scratch
round-trip), most visibly on the memory-bound targets and the cheap-LLR configs.
