# OAI RISC-V RVV Port — SIMD Work Assessment

Status: planning document (2026-07). Scope: replacing the SIMDe-scalar fallback in
the NR PHY hot paths with hand-written RISC-V Vector (RVV) intrinsics.

LDPC **encode** and **decode** inner kernels are already RVV-ported and performing
well. This document assesses the remaining SIMD work.

---

## 1. Current state and how RVV plugs in

- Almost all x86 SIMD in the PHY flows through **`openair1/PHY/sse_intrin.h`** →
  **SIMDe** (`simde__m128i` / `simde__m256i` + the `oai_mm_*` / `oai_mm256_*`
  helpers). SIMDe has **no RVV backend**, so on RISC-V every one of these lowers to
  **scalar** code: correct, builds, but slow.
- 132 files include `sse_intrin.h`; 122 use `simde__m128i/256i`. The NR datapath
  outside LDPC has **no RVV path** today.
- Existing RVV code is confined to `PHY/CODING` (LDPC enc/dec kernels, turbo/CRC
  guards) plus a small `__riscv` region already stubbed in `PHY/TOOLS/oai_dfts.c`.

### Integration model (from the LDPC work)

- RVV kernels are guarded by `#if defined(__riscv) && defined(__riscv_vector)` and
  written **VLA-style** (`vsetvl` loops, `e8m1` for LDPC). There is **no runtime
  dispatch** — RVV vs. SIMDe-scalar is selected at **compile time**. That is fine
  for a dedicated RISC-V build.
- **Build gotcha:** the default `OAI_RISCV_MARCH` in the top `CMakeLists.txt` is
  `rv64gc_zba_zbb_zbs_zicond` — **no `v` extension**, so `__riscv_vector` is *not*
  defined and the RVV code compiles out. Any RVV build must override the march to
  include `v` (and ideally pin a minimum VLEN, e.g. `..._zvl256b`). This needs to be
  settled and documented before more RVV work lands.

---

## 2. Strategic recommendation: port the primitive layer first

Two small, shared headers back a disproportionate share of the datapath. Porting
them to RVV once unlocks many call sites for free:

1. **`PHY/sse_intrin.h`** — the `oai_mm_*` / `oai_mm256_*` complex-arithmetic
   helpers: `conj`, `swap`, `smadd`, `pack`, `cpx_mult`, `cpx_mult_conj`,
   `separate_real_imag_parts`. Used heavily by `nr_channel_compensation.c` and
   `nr_compute_llr.c`.
2. **`PHY/TOOLS/tools_defs.h`** — the `c16_t` complex-vector inline kernels
   (`c16mulShift`, `c16multaddVectRealComplex`, `mult_complex_vectors`,
   `rotate_cpx_vector`, `dot_product`, ~93 sites). These are the SIMD backing for
   **NR channel estimation** — `NR_ESTIMATION/nr_ul_channel_estimation.c` (~114
   helper sites, gNB UL) and `NR_UE_ESTIMATION/nr_dl_channel_estimation.c` (~141
   sites, UE DL) show *zero* raw intrinsics but are entirely SIMD-bound through this
   header. Port `tools_defs.h` and both channel-estimators improve with no changes
   to their own source.

This is the highest-leverage work and should precede the per-file ports below.

---

## 3. Recurring x86 → RVV op mapping

Most of the datapath is **interleaved int16 complex (`c16_t`, Q15)**. Almost no
float32 in hot paths. The common ops and their RVV equivalents:

| x86 (SSE/AVX2)              | RVV equivalent                          | Notes |
|-----------------------------|-----------------------------------------|-------|
| `madd_epi16`                | `vwmacc` (widening SEW16→32) + `vredsum` for reductions | core complex MAC |
| `mulhi_epi16`               | `vmulh.vv` (SEW16)                       | |
| `mulhrs_epi16`              | `vsmul` (fractional rounding mul)        | **rounding differs — verify bit-exactness** |
| `adds/subs_epi16`           | `vsadd`/`vssub` (SEW16)                  | saturation matches |
| `slli/srai_epi16/32`        | `vsll`/`vsra`                            | |
| `packs_epi32/16`            | `vnclip` (saturating narrow)             | |
| `set1_epi16`                | `vmv.v.x`                                | |
| `sign_epi16` (conjugate)    | `vneg` + `vmerge`, or mul by ±1          | |
| `separate_real_imag` / `unpacklo/hi` deinterleave | **`vlseg2e16`** (segment load) | RVV deinterleaves for free — big win |
| interleave / re-im combine  | **`vsseg2e16`** (segment store)          | likewise |
| `shuffle_epi8`              | `vrgather.vv` (byte indices)             | |
| `_mm_clmulepi64_si128` (PCLMULQDQ, CRC folding) | `clmul`/`clmulh` (**Zbc** scalar) or `vclmul`/`vclmulh` (**Zvbc** vector) | needs `zbc`/`zvbc` in `-march` |
| `permutex2var_epi8/16` (AVX512VBMI) | `vrgatherei16` + mask/merge (2-src)| no 1:1; needs redesign |
| `movemask_epi8`             | `vmsne` → `vcpop` / mask store           | bit extraction |
| `bitshuffle_epi64_mask` (AVX512 BITALG) | manual mask + bit align       | no analogue — hardest |

Segment load/store (`vlsegNe*`/`vssegNe*`) is the RVV feature that makes the
complex-interleave and modulation-interleave patterns clean; it replaces most of the
`unpack`/`shuffle` machinery that x86 needs.

---

## 4. Component-by-component assessment

Complexity: **Easy** = elementwise; **Medium** = complex-MAC / segment de-interleave
/ reductions; **Hard** = cross-lane permute, bit packing, bit-exact saturation trees.

### 4.1 `PHY/nr_phy_common/` — LLR + channel compensation (HIGHEST RX value)

| File / function | Hot? | Sites | Complexity |
|---|---|---|---|
| `nr_compute_llr.c` (whole file) | yes | ~1040 | mixed — biggest single RX consumer |
| &nbsp;&nbsp;`nr_16/64/256qam_llr` (single-layer, in `nr_phy_common.c`) | yes | ~124 | **Easy** — abs/subs + strided store |
| &nbsp;&nbsp;`nr_channel_compensation` | yes | ~38/66 | **Medium** — cpx_mult_conj + `mulhrs` scaling |
| &nbsp;&nbsp;`nr_channel_level`, `nr_est_delay` | yes | — | **Medium** — reductions / peak-extract |
| &nbsp;&nbsp;`nr_qpsk_llr_2layer` | yes (MIMO) | ~132 | **Medium** |
| &nbsp;&nbsp;`nr_qam16_llr_2layer` | yes (MIMO) | ~176 | **Hard** — dense mulhi/max/slli saturation chains |
| &nbsp;&nbsp;`nr_qam64_llr_2layer` | yes (MIMO) | ~276 | **Hard** — largest kernel |
| &nbsp;&nbsp;`nr_compute_ML_llr` / `nr_construct_HhH_elements` | yes (MIMO) | ~61 | **Hard** — MIMO matrix arith |

`csirs`/`srs`/`ue_phy_meas` here are scalar — no work. **Correctness hazard:** the
2-layer/ML kernels are dominated by bit-exact int16 saturation (`subs_epi16`,
`mulhi_epi16`, `mulhrs_epi16`); RVV `vsmul` rounding must be validated against the
scalar reference.

### 4.2 `PHY/TOOLS/` — DFT + complex-vector kernels

| File | Hot? | Sites | Complexity |
|---|---|---|---|
| `tools_defs.h` (c16 inline kernels) | yes | ~93 | **Medium** — regular complex loops, segment loads fit well |
| `oai_dfts.c` (DFT/IDFT, 82 fwd + 14 inv sizes) | yes | ~540–670 | **Hard** — defer (see below) |
| `signal_energy.c` | yes | ~47 | **Medium** — `madd` accumulate + `vredsum` |
| `oai_arith_operations.c` | yes | ~28 | **Medium** — re/im deinterleave (segment load) |
| `cdot_prod.c` | yes | ~15 | **Easy/Medium** — single complex dot-product |
| `dB/invSqrt/sqrt/log2/angle/get_sin_cos` | — | 0 | scalar/LUT — leave |
| scopes / smbv / calibration / file_output | no | 0 | leave |

**`oai_dfts.c` is the single hardest file:** 82 mixed-radix forward sizes + 14
inverse, 15 butterfly primitives, fixed 32-byte twiddle tables, and pervasive
fixed-lane `permutevar8x32` / `shuffle_epi8` / `insertf128` re-im interleave that map
poorly to RVV's length-agnostic model — it needs re-expression as
strided/segmented loads + `vrgather`, not a 1:1 swap. **Two mitigations:**
(1) `oai_dfts_neon.c` is an existing full hand-port that proves the transform can be
re-vectorized and gives a lane-level structural template; (2) per your note, a large
DFT PR is expected upstream — **defer `oai_dfts.c` to a second phase** and let that
PR simplify the surface first. No AVX512 anywhere in TOOLS.

### 4.3 `PHY/MODULATION/nr_modulation.c` — TX mapper / precoder / DFT

| Function | Hot? | Sites | Complexity |
|---|---|---|---|
| `cmac*_prec` + `nr_layer_precoder_simd` (gNB DL beamforming) | yes | ~46 | **Medium** — int16 cpx-MAC, seg-load + `vwmacc` + `vnclip` + `vsadd` |
| `nr_dft` (PUSCH transform-precoding renorm) | yes | ~3 | **Easy** |
| `nr_modulation` (QAM mapper, LUT) | yes | ~3 | **Easy** — indexed load / block store, no arithmetic |
| `nr_layer_mapping` (388–706) | **dead code** | ~55 | **do not port — remove.** Fused into `nr_modulation_layer_mapping`; the call-site conditions were removed but the function body was left behind. Pure cross-lane permute, no live callers. |

Does **not** use `oai_mm_*` helpers (rolls its own `cmac*`). **AVX512 check:** has a
`cmac*_prec512` (`__m512i`) path selected by macro — confirm the RISC-V build takes
the SSE/simde path and no `__m512i` type leaks in.

### 4.4 `PHY/CODING/nrLDPC_coding/nrLDPC_coding_segment/` — coding orchestration

SIMD is concentrated in 3 functions across 2 files (encoder/decoder kernels are
already RVV):

| Function | File | Complexity |
|---|---|---|
| `nr_rate_matching_ldpc` / `_rx` (bit select + LLR/HARQ combine) | `nr_rate_matching.c` | **Easy** — scalar today; RX `+=` → `vle16/vadd/vse16` |
| `nr_deinterleaving_ldpc` | `nr_rate_matching.c` | **Easy/Medium** — scalar strided scatter → `vsseg` |
| `nr_process_decode_segment` int16→int8 pack | decoder.c | **Easy** — `vnclip` saturating narrow |
| `nr_interleaving_ldpc` (modulation interleave, Qm 2/4/6/8) | `nr_rate_matching.c` | **Medium** — RVV `vsseg2/4/6/8e8` fits; Qm=6 + tail need care |
| `write_task_output` (bit-to-byte packing) | encoder.c | **Hard** — `bitshuffle_epi64_mask` / `movemask_epi8`; no RVV analogue |

Parent `nrLDPC_coding/` is a pure dlopen dispatch layer (no SIMD). The `aal` backend
is a separate bbdev/DPDK offload path — out of scope. **AVX512VBMI** paths
(`permutex2var_epi8/16`, `bitshuffle`) here are the features with no RVV equivalent;
the `USE128BIT`/scalar fallbacks are what RISC-V uses today.

---

### 4.5 `PHY/CODING/crc.h` + `crc_byte.c` — CRC (PCLMULQDQ carry-less multiply)

`crc.h` implements CRC24A/B, CRC16, CRC11, CRC6 both as scalar LUT / slice-by-2/4
tables **and** as an Intel PCLMULQDQ folding kernel (`crc32_calc_pclmulqdq`,
`crc32_folding_round`, `crc32_reduce_128_to_64/64_to_32`).

**Current RISC-V behavior:** `crc_byte.c` gates the PCLMULQDQ path behind
`USE_INTEL_CRC`, which is only set when `__SSE4_1__ || __aarch64__`. On RISC-V it is
**compiled out**, so CRC runs on the **scalar LUT / slice-by-4** implementation — not
even SIMDe. Correct and reasonable; CRC is per-TB and far cheaper than LDPC/LLR/DFT.

**Two problems to clean up in `crc.h`:**
- Line 309: typo `defineD(__riscv)` (should be `defined`). Verified it evaluates
  *false*, so it silently falls to the `#else`.
- Lines 309/334/363: the `defined(__riscv)` branches call the **raw x86**
  `_mm_clmulepi64_si128`, which does not exist on RISC-V, and `sse_intrin.h` only
  includes `simde/x86/clmul.h` for ARM — so neither the raw nor the simde clmul is
  actually available on RISC-V. These branches are dead today (gated off by
  `USE_INTEL_CRC`) but are incorrect and should be removed or repurposed.

**RVV/accel path (optional, low priority):** the folding CRC maps naturally to
RISC-V **Zbc** scalar carry-less multiply (`clmul`/`clmulh` on the two 64-bit halves,
replacing `_mm_clmulepi64_si128`; byte-swaps via `rev8`/`vrgather`), or **Zvbc**
vector clmul. Neither `zbc` nor `zvbc` is in the default `-march`. Complexity is
**Medium** (reflection/byte-order care), but value is low unless profiling shows CRC
hot at high throughput — the scalar slice-by-4 is already adequate.

---

## 5. Cross-cutting concerns

- **Bit-exactness is the dominant risk.** The LLR/demod kernels rely on exact x86
  int16 saturation and rounding semantics. Build a byte-exact comparison harness
  (RVV vs. scalar-SIMDe reference) per kernel before trusting any port. `vsmul`
  (for `mulhrs_epi16`) is the specific rounding to watch.
- **AVX512 leak check.** `nr_modulation.c` and `NR_TRANSPORT/nr_dlsch.c` contain
  macro-gated `__m512i` blocks. Verify no `__m512i` type reaches the RISC-V build
  (they should fall to SSE/simde). Everything else AVX512 is inside LDPC.
- **`crc.h` has a live bug** (`defineD` typo + `defined(__riscv)` branches calling
  non-existent x86 clmul) that is currently masked only because `USE_INTEL_CRC` is
  off on RISC-V. Fix or delete those branches to avoid a landmine if CRC gating
  changes. Enabling a fast CRC on RISC-V means `zbc`/`zvbc` in `-march`.
- **No RVV analogue** for `permutex2var` (2-source permute), `bitshuffle_epi64_mask`,
  and `movemask` — the VBMI interleaving and bit-packing paths need genuine
  redesign, not translation.
- **VLA tail handling.** Follow the LDPC pattern (`vsetvl` per iteration) so one
  kernel serves any VLEN (X100 256-bit and A100 1024-bit both work).
- **Runtime dispatch (optional).** Current model is compile-time only. If a single
  binary must serve multiple RISC-V targets, add a `__riscv_vlenb()`-based selector;
  otherwise compile-time gating is sufficient.

---

## 5b. Running on the K3 board (X100 / A100)

The SpaceMIT K3 has two vector clusters that must be selected explicitly:

| Cluster | CPUs | VLEN | notes |
|---|---|---|---|
| **X100** | `cpu0`–`cpu7` | 256-bit (`vlenb=32`) | default; no special launch |
| **A100** | `cpu8`–`cpu15` | 1024-bit (`vlenb=128`) | requires the `/proc/set_ai_thread` switch **before any vector op** |

**The A100 switch.** Writing the thread's PID to `/proc/set_ai_thread` moves it to the
A100 cluster and changes VLEN to 1024 *at runtime*. Two hard rules:
1. **VLEN must be constant for a function's stack frame.** Do the switch, then call the
   vector work as a separate `noinline` function so its prologue reads the post-switch
   `vlenb` and sizes spill slots correctly (keep `main()` scalar). The simplest way to
   guarantee this is to switch *before* `exec`, so the whole program starts at VLEN=1024.
2. **Run from an interactive shell.** A non-interactive `ssh host '…'` invocation dies at
   init (the "CPU channel pipeline" pre-alloc) — this hits even old binaries, so it is an
   environment quirk, not the code. Use `ssh -t`, a login shell, or run on the box.

**Launch recipes** (switch-before-exec form, no in-program switch needed):

```bash
# A100 (cpu8, VLEN=1024) — harness
sh -c 'echo $$ >/proc/set_ai_thread; exec ./rvv_chlevel_test 8'

# A100 — nr_dlsim / nr_ulsim (env + taskset after the switch)
sh -c 'echo $$ >/proc/set_ai_thread; exec env LD_LIBRARY_PATH=. taskset -c 8 ./nr_dlsim <args>'

# X100 (cpu2, VLEN=256) — no switch
LD_LIBRARY_PATH=. taskset -c 2 ./nr_dlsim <args>
```

**Verify the VLEN actually took** (a one-shot `__riscv_vlenb()` print in the kernel, or the
harness banner): cpu8-with-switch prints `vlenb=128` (VLEN=1024, `e16m1` vlmax=64); cpu2
prints `vlenb=32` (VLEN=256, vlmax=16). If a `taskset -c 8` run shows `vlenb=32`, the switch
did not take (missing/failed `/proc/set_ai_thread` write) and you are measuring the A100
core at the wrong VLEN.

**Perf note.** A100's wide VLEN helps *compute-dense* kernels but *loses* on memory-bound
ones (segment/strided loads, stack round-trips) — e.g. the 2-layer L-best was ~4× slower on
A100 than X100. See `nr_phy_common/src/nr_mimo_lbest_detector.md` and `RX_FUSION_ASSESSMENT.md`.

*Canonical source for the mechanism:* the `LAUNCH NOTE` comment in
`rvv_harness/rvv_chlevel_test.c` and the `use_ai()` helper in the harness `.c` files.

---

## 6. Recommended ordering and rough sizing

T-shirt sizes are relative engineering effort, not calendar time.

| Phase | Work | Size | Rationale |
|---|---|---|---|
| 0 | Settle `-march` (add `v` + min VLEN; consider `zbc`/`zvbc` for CRC), document build, stand up bit-exact test harness + a µbenchmark | S | unblocks everything; no dispatch yet |
| 0b | Fix/remove the broken `defined(__riscv)` clmul branches in `crc.h` | XS | correctness landmine, independent of the rest |
| 1 | **Primitive layer**: RVV `oai_mm_*` (sse_intrin.h) + `c16_t` kernels (tools_defs.h) | M | highest leverage; also uplifts both channel estimators for free |
| 2 | `nr_channel_compensation` + single-layer LLRs (`nr_16/64/256qam_llr`) | M | hottest RX, lowest risk |
| 3 | Modulation precoder (`cmac*` + `nr_layer_precoder_simd`), `nr_dft`, `nr_modulation` | M | core TX; AVX512 leak check |
| 4 | LDPC segment layer: rate matching, deinterleave, interleave, pack | S–M | mostly Easy; `write_task_output` is the one Hard spot |
| 5 | 2-layer / ML LLR kernels (`qam16/64_llr_2layer`, `nr_compute_ML_llr`) | L | MIMO; bit-exactness heavy |
| 6 | `oai_dfts.c` DFT/IDFT | XL | **defer** until the upstream DFT PR lands; use `oai_dfts_neon.c` as template |
| 7 | Second-tier NR_TRANSPORT/NR_UE_TRANSPORT (`pucch_rx`, `nr_scrambling`, demod drivers) | M | after the shared kernels they depend on are done |
| 8 | CRC folding via Zbc/Zvbc (optional) | S–M | scalar slice-by-4 already adequate; do only if CRC profiles hot |

Notes:
- LTE paths (`LTE_UE_TRANSPORT/dlsch_llr_computation*.c`, ~6700 sites) are the largest
  raw SIMD counts in the tree but are **out of scope for a 5G port** — ignore.
- `nr_layer_mapping` is **dead code** (fused into `nr_modulation_layer_mapping`,
  call conditions removed but body left) — delete it, don't port. `write_task_output`
  is the only genuinely Hard, isolated item; treat as optional.

---

## 7. Open questions

1. Target hardware VLEN(s) and `-march` string to standardize on (X100 256-bit,
   A100 1024-bit, K3)? This fixes the build and the benchmark matrix.
2. Compile-time-only, or is a single multi-target binary (runtime dispatch) required?
3. Timing of the upstream DFT PR — gates Phase 6.
4. Is the MIMO 2-layer/ML LLR path exercised on the RISC-V target's use cases, or can
   Phase 5 be deprioritized?
