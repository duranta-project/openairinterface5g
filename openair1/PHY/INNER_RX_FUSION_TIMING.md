<!-- SPDX-License-Identifier: LicenseRef-CSSL-1.0 -->
# inner_rx compute-fusion — timing

Tracks the inner_rx (channel compensation + detector/LLR) cost, **unfused vs fused** (`OAI_FUSE`), so
the tiled fusion (L1 scratch) can be compared across platforms and bandwidths.

## Current measurements (2026-11-07)

Branch state: tiled fusion on develop + PR356, with the fixes below. Same binary, `OAI_FUSE=0` vs `1`, fixed
`OAI_RNGSEED`, median of 3, default channel (AWGN, no `-g`), 30 kHz SCS, 273 PRB, per slot. UL: `RX PUSCH Symbol
Processing time` (4 Rx). DL: `RX_PDSCH_STATS` (2 Rx; includes the channel estimation, the same in both modes).

### One core

Isolated core (`isolcpus`, `nohz_full`) except on the Zen 5 and the Rock 5A, which have none (pinned only). The
Rock 5A build is Release (the others RelWithDebInfo) and runs on an A76 core (cores 4-7; 0-3 are A55).

| case | x86 Zen 5 (Ryzen AI MAX+ 395) | x86 Sapphire Rapids (Xeon Gold 6433N) | GH200 (Neoverse-V2) | DGX Spark (GB10, X925 core) | Rock 5A (RK3588, A76 core) |
|---|---|---|---|---|---|
| UL 1-layer 64QAM               | 76.1 → 71.7 µs (−6%) | 224.7 → 191.1 (−15%) | 355.3 → 297.2 (−16%) | 196.3 → 157.2 (−20%) | 1025 → 892 (−13%) |
| UL 2-layer 64QAM `-D lbest`    | 739 → 707 (−4%)      | 1610 → 1489 (−7.5%)  | 2847 → 2528 (−11%)   | 1749 → 1626 (−7%) | 9437 → 8776 (−7%) |
| UL 2-layer 256QAM `-D lbest`   | 843 → 795 (−6%)      | 1892 → 1732 (−8.5%)  | 3896 → 3572 (−8.5%)  | 2312 → 2209 (−4.5%) | 11646 → 11062 (−5%) |
| DL 1-layer 64QAM               | 108.2 → 102.6 (−5%)  | 389.5 → 359.6 (−8%)  | 353.3 → 306.3 (−13%) | 201.2 → 169.1 (−16%) | 1871 → 1722 (−8%) |
| DL 2-layer 64QAM `-E lbest`    | 760 → 731 (−4%)      | 1819 → 1731 (−5%)    | 2690 → 2518 (−6%)    | 1674 → 1618 (−3%) | 10015 → 9697 (−3%) |

Small allocation, 106 PRB, 2 Rx, DL per symbol (`DLSCH_CHANNEL_COMPENSATION_STATS` + `DLSCH_LLR_STATS`, the LLR stat
including the layer demap + descramble in both modes): 1-layer 16QAM 0.66 → 0.63 µs (−5%, Zen 5) and 4.01 → 3.12 µs
(−22%, GH200); 2-layer QPSK `-E ml` 2.96 → 2.69 µs (−9%) and 16.73 → 13.43 µs (−20%).

### Several cores: gNB thread pool

`sudo nr_ulsim ... -Y 6,7,8,9,10,11` (one pool thread per isolated core, SCHED_FIFO), main thread on core 5. Floating
pool threads (`-C`) do not spread over isolated cores (no load balancing there) and pinning needs CAP_SYS_NICE.
`nr_dlsim` has no pinned UE pool, so this is UL only. The Rock 5A has no isolated cores: there the pool is 4
floating threads (`-C 4`) on the four A76 cores (`taskset -c 4-7`), balanced by the kernel, no root needed.

| case | Sapphire Rapids, 6 threads (one-core gain) | GH200, 6 threads (one-core gain) | Rock 5A, 4 threads (one-core gain) |
|---|---|---|---|
| UL 1-layer 64QAM             | 79.5 → 69.8 µs (−12%; −15%) | 76.2 → 70.5 (−7.5%; −16%) | 468 → 404 (−13.7%; −13%) |
| UL 2-layer 64QAM `-D lbest`  | 327 → 292 (−10.8%; −7.5%)   | 511 → 458 (−10.4%; −11%) | 3086 → 2775 (−10.1%; −7%) |
| UL 2-layer 256QAM `-D lbest` | 382 → 335 (−12.5%; −8.5%)   | 696 → 641 (−7.8%; −8.5%) | 3702 → 3349 (−9.5%; −5%) |

Fixes since the 2026-11-02 numbers that change these figures:
- **Last tile >= 16 REs** (fused 2-layer). 273 PRB = 3276 REs = 34 × 96 + 12: the 12-RE last tile sent the
  64/256QAM L-best kernel to its scalar Q15 fallback (~1 µs/RE), making the fused 2-layer path ~45% slower
  on Zen 5 at 64QAM. The last tile now starts earlier and overlaps the previous one.
- **SIMD codeword store** (fused 2-layer, demap + descramble fold). The per-tile layer interleave and
  descramble was a scalar loop; it is now `nr_layer_demapping` + a vectorized ±1 multiply. This was the
  whole remaining x86 2-layer penalty (QPSK: +28% → −9%).
- **Cheap zero-phase-error CPE.** Without PTRS the CPE is (32767, 0); its full complex multiply is replaced by
  the equivalent `x - (x > 0)`, `max -32767` (bit-exact), in both the unfused and the fused compensation, so
  both columns got cheaper and there is less left for the fusion to save.
- **aarch64 builds only the 128-bit fused kernels** (the 256-bit ones were compiled unused).

Findings:
- **Fusion wins on every platform and case**: 3-6% on the cache-rich Zen 5, 5-15% on Sapphire Rapids, 6-16% on
  the GH200, 3-20% on the DGX X925, 3-13% on the Rock 5A A76. It removes a fixed cost, the compensation memory round trip, so the
  percentage is round trip ÷ total: largest for 1 layer and cheap 2-layer detectors, smallest where the near-ML
  LLR dominates (2-layer 64/256QAM).
- **With the gNB thread pool the 2-layer gain grows** on Sapphire Rapids (7.5-8.5% → 11-12.5%, six cores) and on
  the Rock 5A (5-7% → 9.5-10%, four A76 cores): the cores share the memory bandwidth the fusion saves. The GH200 is
  already bandwidth-bound on one core and stays at 8-10%. `nr_dlsim -d N` does not parallelize the PDSCH symbol
  processing, so there is no DL multi-core figure.
  The 1-layer gain shrinks with six threads: ~12 µs of work per thread, where the pool dispatch weighs more.
- **The detector is still the wall**: UL 2-layer 256QAM L-best is ~0.8-3.6 ms/slot on one server-class core
  (500 µs slot), ~11 ms on one A76, ~0.34-0.64 ms with six server cores and ~3.3 ms with the four A76 cores;
  it needs the symbol-level threading, and 273-PRB 2-layer 256QAM is out of reach for an RK3588 gNB.

### 2-layer 256QAM: int16 vs fp32 metric (`pusch_mimo_hires` / `--pdsch-mimo-hires`, sims `-J`)

The int16 256QAM metric (1/4 scale) flips the sign of small-|LLR| bits: a 0.2-0.6 dB loss at 70% HARQ throughput
for MCS 20-26 and ~1.4 dB at MCS 27 (4 Rx, TDL-A), ~1 dB at MCS 20-21 with 2 Rx (see `nr_compute_ML_llr`).
The fp32-metric kernel tracks the float reference; full ML always uses it, L-best on request. Detector cost
alone (`bench_rx2l`, one core, 2-layer LLR stage per slot, 273 PRB):

| | x86 Zen 5 | x86 Sapphire Rapids | GH200 (Neoverse-V2) | DGX Spark (GB10, X925 core) | Rock 5A (A76 core) |
|---|---|---|---|---|---|
| 256QAM L-best, int16            | 652 µs | 1399 µs | 3035 µs | 1814 µs | 8864 µs |
| 256QAM L-best, fp32 (`lbesthr`) | 759 µs (+16%) | 1674 µs (+20%) | 3944 µs (+30%) | 2253 µs (+24%) | 11002 µs (+24%) |
| 256QAM full ML (fp32)           | 9349 µs (int16 was 13350) | 20690 µs | 48642 µs | — | — |
| 64QAM L-best, int16 (ref.)      | 554 µs | 1148 µs | 1958 µs | 1255 µs | 6629 µs |

NEON has 4 fp32 lanes per register against 8 int16, hence the larger fp32 cost on aarch64. On the DGX, cores
5-9 and 15-19 are the Cortex-X925s; 0-4 and 10-14 are A725s (about 2.8x slower here).

## Method

A/B on the same binary via the `OAI_FUSE` gate (isolates the fusion; no other change):
```
OAI_FUSE=0 ./nr_dlsim -n300 <cfg> -E lbest -P     # unfused: COMP + LLR run separately
OAI_FUSE=1 ./nr_dlsim -n300 <cfg> -E lbest -P     # fused:   COMP skipped (~0), fused kernel in LLR
# metric = DLSCH_CHANNEL_COMPENSATION_STATS + DLSCH_LLR_STATS  (fused: LLR only)
```
- `-P` enables cpu_meas + prints "UE function statistics". `-E mmse|ml|lbest` selects the multi-layer
  receiver (the measurements below predate it and used the former `-E` = near-ML switch).
- gNB side: `nr_ulsim -y2 -W2 -z2 -P` (`-y` n_tx, `-W` layers, `-z` n_rx, `-D lbest|ml|mmse`).
- Times are **per per-symbol invocation** (trials = n_slots × ~13 PDSCH data symbols/slot).
  **Per slot ≈ 13×.** A 30 kHz slot is 500 µs.
- The fusion saves the compensation's memory round-trip → benefit ∝ `compensation / total`.
- **x86 is cache-rich → the round-trip it eliminates is cheap here; the numbers are the muted
  case.** The memory-bound targets (RK3588 A76, NXP A72, K3 A100) should show larger deltas,
  and there the near-ML LLR itself is memory-bound (~4× slower on A100).

## Historical measurements (before 2026-11-07)

Kept as the record. Two bugs fixed on 2026-11-07 (see above) affected the **fused 2-layer** figures below:
the scalar last tile at 273 PRB with the 64/256QAM L-best kernels, and the scalar codeword store whenever the
fused 2-layer path wrote the codeword (gNB store fold). The fused 2-layer numbers at 100 MHz, and the GH200 /
Sapphire Rapids gNB rows, are therefore pessimistic; the 1-layer and unfused figures are not affected. The
"x86 dev machine" is not the Zen 5 of the current tables.

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

## aarch64 / RISC-V (fused kernels native w128 on aarch64; RVV via SIMDe until realign)

| platform | config | unfused | fused | delta | notes |
|---|---|---|---|---|---|
| RK3588 A76 (UE, Rock 5A) | 2L-64QAM full-ML @40 MHz | 29.71+479.76=509.47 | 496.72 | ~2.5% | native w128; **BLER/BER identical fused vs unfused (validated on real NEON)**; comp cost **-43%** when fused (29.71→16.96 absorbed = memory round-trip saved), but full-ML LLR dominates (~497 µs/sym ≈ **6.5 ms/slot**, ~13× over 500 µs → needs L-best reduced-search). TODO: measure 1-layer / 2L-QPSK here where fusion should shine (compensation a bigger fraction). |
| RK3588 A76 (UE, Rock 5A) | 2L-64QAM **L-best** (PAT=1) @40 MHz | 29.87+125.62=155.49 | 142.33 | **~8.5%** | native w128; BER identical fused vs unfused (real NEON); L-best LLR ~3.4× cheaper than full-ML (126 vs 480 µs/sym); same ~44% comp cut (29.87→16.71) but now a bigger share → bigger overall %. ~1.85 ms/slot (viable with per-symbol threading across cores). |
| RK3588 A76 (UE, Rock 5A) | 2L-64QAM full-ML @100 MHz | 75.12+1239.64=1314.76 | 1282.29 | ~2.5% | comp 2.5× vs 40 MHz, LLR 2.6× → **delta bandwidth-invariant** on this memory-bound core; ~43% comp cut (42.6 absorbed vs 75.1). BER identical. |
| RK3588 A76 (UE, Rock 5A) | 2L-64QAM **L-best** (PAT=1) @100 MHz | 75.37+333.15=408.52 | 375.77 | **~8.0%** | same ~43% comp cut; delta ≈ 40 MHz L-best (8.5%). L-best LLR ~3.7× cheaper than full-ML. BER identical. |
| RK3588 A76 (UE, Rock 5A) | 1-layer 64QAM @100 MHz, 2 Rx | 20.96+5.50=26.46 | 24.54 | ~7.3% | comp cut only ~9% (19.04 absorbed vs 20.96) — **no rho to save** + per-tile LLR-call overhead vs a very cheap 5.5 µs LLR. Register-fusion (inline LLR, no per-tile call/scratch) should lift this. BER identical. |
| GH200 Grace (gNB, Neoverse-V2) | 2L-64QAM **L-best** (PAT=1) @100 MHz, 4 Rx | 672.23+1956.00=2628.23 | 2388.18 | **~9.1%** | `nr_ulsim -y2 -z4 -W2 -R273 -m25`, per slot (12 sym). Big out-of-order aarch64 server core = **the other end** of the A76. Comp **~98.6% absorbed** (672.23→9.47) — far more than A76's ~43%: the V2 is bandwidth-constrained relative to its huge compute, so the comp DRAM round-trip is proportionally much costlier. LLR rises 1956→2379 (comp *compute* moves in, plus the scalar last tile and codeword store fixed on 2026-11-07) but the ~250 µs round-trip vanishes → net -240 µs. BER ~2.2e-4 both (seeds differ — timing run, not a bit-exact check). Tiled fusion; register variant not built (see below). |
| GH200 Grace (gNB, Neoverse-V2) | 2L-64QAM **full-ML** @100 MHz, 4 Rx | 674.25+6177.28=6851.53 | 6620.20 | **~3.4%** | comp **~98.6% absorbed** (674.25→9.27), saving ~231 µs — **same absolute round-trip as the L-best run** (~235 µs), but only 3.4% here because full-ML LLR (6611) is 2.78× the L-best LLR (2379). Fusion saves a fixed comp-round-trip; its % = round-trip / detector-cost. Full-ML LLR vs L-best → **L-best 2.78× cheaper** on this core, *less* than the A76's ~3.7× (big OoO V2 hides full-ML's extra candidates via ILP). Detector is the wall: 6.6 ms/slot ≈ 13× a 500 µs budget (L-best 2.4 ms ≈ 4.8×). |
| NXP A72 (gNB)     | | | | | native w128 |
| K3 X100 (RISC-V)  | | | | | SIMDe (not native RVV) |
| K3 A100 (RISC-V)  | | | | | SIMDe; LLR ~4× slower here |

### Findings (aarch64: A76 little-core ↔ GH200 big-core)
- **The tiled fusion saves a fixed compensation round-trip, detector-independent.** On the GH200 it
  is ~235 µs/slot whether the detector is L-best (240) or full-ML (231); the compensation is
  ~98.6% absorbed either way. So the *percentage* is just round-trip ÷ detector-cost: **9.1% for
  L-best, 3.4% for full-ML** on the same core. Identical shape on the A76 (~8% L-best, ~2.5%
  full-ML). This is the whole fusion story in one line — fusion removes a constant, the detector
  sets the denominator.
- **Comp absorption scales with how bandwidth-bound the core is:** ~43% on the A76, ~98.6% on the
  GH200. The Neoverse-V2 has so much compute vs memory bandwidth that the comp DRAM round-trip is
  nearly its entire comp cost, so fusion erases almost all of it.
- **The detector is the wall on every platform, and only reduced-search moves it.** L-best is
  2.78× cheaper than full-ML on the GH200 vs ~3.7× on the A76 — the big out-of-order core hides
  more of full-ML's extra candidates behind ILP, so candidate reduction pays less there (but still
  the dominant lever: full-ML 6.6 ms/slot ≈ 13× a 500 µs budget, L-best 2.4 ms ≈ 4.8×).

## Register-fusion (`OAI_FUSE=2`, commit b361b05971) — dropped

**Not carried over in the 2026-11-02 rebase onto develop** (wash on A76/A72, loss on x86; 2-layer not worth
building, see below). This section is the record of that decision. Revive it from the pre-rebase branch
(`backup/dl-inner-rx-pr356-2026-11-02`) only if the K3 A100 measurement asks for it.
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

### aarch64 — register ≈ tiled
1-layer 64QAM @100 MHz, 2 Rx, `OAI_RNGSEED=888`, BER bit-identical (1.592530e-04) both:

| variant | fused LLR |
|---|---|
| unfused (ref, earlier) | 26.46 µs |
| tiled (FUSE=1)    | 24.46 µs (~7.5% vs unfused) |
| register (FUSE=2) | 24.54 µs (+0.3% vs tiled — noise) |

Measured wash so far:
- **RK3588 A76, gcc12:** FUSE=1 ≈ FUSE=2 (table above; broader configs also equivalent).
- **NXP A72 (gNB), gcc13:** FUSE=1 ≈ FUSE=2 — a *newer* compiler did not flip it either.
- **x86, gcc12:** FUSE=2 ~14% slower.

**Verdict so far: register-fusion is a wash on aarch64 (A76/A72) and a loss on x86 — the tiled
fusion is the shipping default.** The tiled fusion already captured the whole memory-round-trip win
(26.46 → 24.46); eliminating the L1 scratch + per-tile call on top adds nothing for 1-layer (tiny
scratch — rxComp + mag, **no rho** — and a ~24 µs LLR).

At the time the path was kept gated pending a gcc15/clang retest; it was dropped in the 2026-11-02 rebase
instead. **The one open candidate is RISC-V K3 A100.** The A100 is memory-bound (L-best ~4x slower under SIMDe), i.e. exactly the regime
where eliminating the L1-scratch round-trip should pay — unlike the cache-rich/register-cheap
A76/A72/x86 where it's a wash. Two gating measurements before the PR: (1) aarch64 gcc15/clang;
(2) **K3 A100 FUSE=1 vs FUSE=2 — testable NOW via SIMDe** (the register .inc compiles on RISC-V; no
native-RVV realign needed first). If both stay a wash -> drop the register path for a clean
single-fusion PR. If the A100 flips positive -> keep it, default-on only where it wins (A100),
gated elsewhere.

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
for both 1- and 2-layer; the 1-layer register path was not a win and has been dropped. The near-ML/L-best detector compute is the wall — only reduced-search
(L-best: ~3.4× on A76) moves it, not fusion.

## Demap + descramble folded into the inner-RX store (gNB + UE)
The layer demapping and codeword descrambling that used to run as separate per-symbol passes
after the LLR are now folded into the fused inner-RX store:
- **1-layer** (`nr_inner_rx_1layer` + `scramble`): per-tile LLRs are multiplied by the ±1
  descrambling sequence at the store (1-layer needs no demap — per-layer order == codeword order).
- **2-layer** (`nr_inner_rx_2layer_ml` codeword mode): the detector writes per-layer into L1 tile
  scratch, then a per-tile pass interleaves the two layers (`nr_layer_demapping`) and descrambles
  (vectorized ±1 multiply) into the codeword buffer.

gNB wiring writes the final codeword directly for single-UE, non-PTRS, non-transform-precoding
symbols and skips the post-pass. Bit-exact (deterministic self-check vs `nr_layer_demapping` +
unscramble, all mod orders). On x86 the eliminated passes were ~34 µs demap + ~13 µs unscramble per
slot (2-layer 64QAM). The UE now uses the same fold (its descrambling moved out of the decoder), and
both sides share the `nr_inner_rx` dispatch. Current costs are in the tables at the top.

## Follow-ups / open items
- **Compiler sweep.** All timing so far is **gcc12**. The fusion payoff hinges on the compiler
  keeping the L1 tile scratch in registers and scheduling the fused loops, so **gcc13/gcc15 and
  clang (clang15 is already the RISC-V toolchain)** could change the delta. Re-run the A/B under
  each and compare; a newer compiler may shift where fusion helps.
- **Full-ML 2-layer nondeterminism (pre-existing; not seen in the 2026-11 matrix runs, where 64QAM
  full ML was identical across runs and builds).** `nr_ulsim` 2-layer full-ML (`nr_qam64_llr_2layer`)
  gives run-to-run varying `errors_scrambling` for identical code+seed (e.g. FUSE=0: 3128 then
  2878), while L-best and 1-layer are deterministic. Present on the baseline (independent of the
  fusion/memset/descramble work), so a latent issue in that detector or its inputs (likely an
  uninitialized/padding read). Low impact on BLER curves (averages out) but it breaks
  cross-invocation bit-exact A/B for that path — validate full-ML via single-run self-checks.

### x86 gNB confirmation (Intel Xeon Gold 6433N, Sapphire Rapids) — historical
`nr_ulsim` 2L-64QAM L-best @100 MHz, 4 Rx, MCS25, `OAI_FUSE=1` — with the demap+descramble
store-fold, **`RX PUSCH layer demapping` and `RX PUSCH unscrambling` both read 0.00 µs** (folded
into the LLR store), on both AVX2 (w256) and **AVX-512 (w512, then selected with `OAI_LBEST_W512=1`; PR356
has since removed that knob and runs the native width)**. AVX-512 w512
L-best is functional and, with `-C8` threading, RX PUSCH wall time ~902 µs (vs ~2295 µs AVX2 single).
`OAI_FUSE=1` vs `=2` LLR ~1939 vs ~1934 µs (2-layer takes the tiled path for both — register variant
is 1-layer only — so this is not a register-vs-tiled comparison). Confirms the store-fold across x86
SIMD widths; feeds the compiler/ISA sweep follow-up.
