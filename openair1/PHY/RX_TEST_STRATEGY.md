# NR PHY RX — Cross-Architecture Test Strategy

**Goal.** A portable **non-SIMD baseline** plus a **centralized, cross-architecture** correctness test suite for the RX signal-processing kernels (channel compensation, LLR, descramble, and the fused `inner_rx` of `RX_FUSION_ASSESSMENT.md`). The baseline lets us (a) verify each step is correct in isolation and (b) compare every architecture — RVV (RISC-V), NEON/SVE (aarch64), HVX (Hexagon), AIE, AVX/SSE (x86) — against **one common reference**, ideally in CI without needing every physical board.

This formalizes what the `openair1/PHY/rvv_harness/` binaries already do ad-hoc (byte-exact RVV-vs-reference checks, run by hand on the K3 board) into OAI's `ctest` framework, with QEMU as the centralized runner.

---

## 0. TL;DR

- **The scalar reference is a *three-in-one*:** the executable **spec**, the portable **fallback** implementation for any ISA without a kernel, and the **oracle** every SIMD kernel is tested against. It is not throwaway test code.
- **Two oracle tiers, selected per kernel by whether it touches float:**
  - **Bit-exact** vs a scalar *fixed-point* reference for pure-integer kernels → guarantees cross-arch *consistency* (all ISAs produce identical bytes).
  - **Functional / tolerance** vs a *float* golden model for float-bearing kernels (e.g. the L-best ZF seed) → validates algorithm accuracy where ISAs legitimately differ in the last bit.
- **QEMU centralizes correctness**, and its killer feature is **VLEN**: `qemu-riscv64 -cpu rv64,v=true,vlen=256|1024` tests *both* K3 vector widths on an x86 CI host with no board and no `/proc/set_ai_thread`. **QEMU = correctness gate; board = performance + hardware validation** — two complementary lanes.
- Build on what exists: OAI already has the `ctest` scaffolding and float L-best references; the gap is a *canonical* scalar spec, `add_test` registration for the SIMD kernels, and the QEMU runner.

---

## 1. What already exists (and the gaps)

| Have | Where | Gap to close |
|---|---|---|
| Byte-exact RVV-vs-reference harnesses | `openair1/PHY/rvv_harness/*.c` (run manually on board via scp/ssh) | not in `ctest`; reference is x86-native/SIMDe, not a canonical scalar spec |
| Float scalar reference kernels | `nr_compute_llr.c:3964` (`nr_qam64_llr_2layer_lbest`), `:4169` (256QAM); L=64 == exact max-log | only for L-best; no fixed-point scalar spec for the integer kernels |
| A functional (tolerance) oracle in production | `OAI_LBEST_DBG` / `OAI_LBEST_DBG256` (signDisagree vs float ML) | analysis-only env gate, not a registered test |
| `ctest` infra + PHY unit-test precedent | `openair1/PHY/{MODULATION,NR_TRANSPORT,TOOLS,INIT}/tests`, `CODING/TESTBENCH`, ~70 `add_test` | no RX-kernel tests registered; no cross-arch/QEMU lane |
| Deterministic inputs | harnesses use seeded RNG; `nr_dlsim` has `OAI_RNGSEED` | not centralized into shared golden-vector generators |

Net: the *pieces* exist. This strategy connects them — one canonical baseline, registered tests, a QEMU runner spanning arches and VLENs.

---

## 2. Principle — the scalar reference is spec + fallback + oracle

For every kernel `K`, write a plain-C scalar `K_ref` that is:
1. **Spec** — obviously correct, no SIMD subtleties; it *defines* the fixed-point contract (saturation, rounding, shift order).
2. **Fallback** — a real (slow) implementation any platform runs before it has a hand-written kernel. On the fusion work this is literally "**kernel #0**": the scalar `nr_rx_process_tile` that a new ISA starts from (see `RX_FUSION_ASSESSMENT.md` §3).
3. **Oracle** — the single thing all SIMD variants are compared against, so no arch is validated merely by matching *another* arch (which can be wrong in the same way).

This is a stronger position than the current harness, which compares RVV to x86-native/SIMDe — if both share a bug, the harness passes. A hand-written scalar spec is the independent ground truth.

---

## 3. Two oracle tiers

Which tier a kernel uses is a **property of the kernel**, declared in its test:

### 3a. Bit-exact (integer kernels) — cross-arch consistency gate
Pure fixed-point kernels — channel compensation, QAM LLR, descramble, the L-best *integer* metric/LLR — must match the scalar fixed-point spec **byte-for-byte**, on every arch. This is the current harness rigor (`0/1536 mismatches`), retargeted to the canonical spec. OAI already depends on exact int16 saturation/rounding (`RVV_PORT_ASSESSMENT.md` §5: "bit-exactness is the dominant risk"), so a single fixed-point spec that all ISAs match is both feasible and the strongest guard against silent per-arch drift.

**Decision baked in:** even though the fusion refactor's bar is "functionally equivalent" at the *chain* level, the *integer kernels* hold the harder **one-canonical-fixed-point-spec, all-arches-byte-match** bar. Freedom to reorder lives in the dataflow, not in the per-RE arithmetic.

### 3b. Functional / tolerance (float-bearing kernels) — algorithm-accuracy gate
Kernels with a floating-point step legitimately differ across ISAs in the last bit (IEEE rounding, `vfdiv`, FMA contraction). The L-best ZF seed is the canonical case: RVV and x86 diverge slightly, so byte-exact is *wrong* — we validate by **`signDisagree` vs a float max-log reference** and a decode/BLER check (exactly `OAI_LBEST_DBG`). These kernels declare a tolerance (sign-agreement %, or max abs LLR error, or BLER delta) rather than a byte match.

| kernel family | tier | reference |
|---|---|---|
| channel compensation, QAM LLR, descramble | bit-exact | fixed-point scalar spec |
| L-best integer metric + LLR output | bit-exact | fixed-point scalar spec |
| L-best / MMSE ZF **seed** (float) | tolerance | float ZF + `signDisagree` vs float max-log |
| full 2-layer detector (seed+search+LLR) end-to-end | tolerance | float max-log ref + BLER |

---

## 4. Target architecture of the suite

Three reusable pieces + registration:

1. **`nr_phy_test_ref`** (shared support lib): the canonical scalar fixed-point references, the float golden models, and **deterministic seeded vector generators** so every arch and every CI run tests *identical* inputs (formalize the harness RNG / `OAI_RNGSEED`). Golden vectors cover: modulation orders (QPSK/16/64/256), layer counts (1/2/3), edge magnitudes (near int16 saturation), and the DMRS/CSI-punctured RE patterns.
2. **Per-kernel driver** (generalize the current harness `main()`): pull vectors from the generator, run `K_ref` and `K_arch`, assert bit-exact **or** within-tolerance per §3, print a mismatch/`signDisagree` summary. One template, parameterized by kernel.
3. **`add_test` registration**: each kernel becomes a `ctest`. On the **native x86** build this runs immediately (SIMDe/AVX vs scalar). For cross builds, the test binary is launched through the emulator (§5).
4. **A CMake toggle** — `CROSSTEST_EMULATOR` (e.g. `qemu-riscv64;-cpu;rv64,v=true,vlen=1024`) — so `ctest` transparently runs the cross-compiled binaries under QEMU; parameterize a second registration at `vlen=256`.

---

## 5. QEMU — the centralized correctness lane

`qemu-user` runs a cross-compiled binary on the x86 host, translating the guest ISA. This gives a **board-free CI gate** for every architecture from one machine.

```bash
# RVV, both K3 vector widths — no board, no /proc/set_ai_thread
qemu-riscv64 -cpu rv64,v=true,vlen=1024 ./rvv_chcomp_test
qemu-riscv64 -cpu rv64,v=true,vlen=256  ./rvv_chcomp_test
# aarch64 NEON/SVE
qemu-aarch64 ./neon_chcomp_test
# Hexagon
qemu-hexagon ./hvx_chcomp_test
```

**Why this is the right centralization:**
- **Both VLENs in CI** via `-cpu ...,vlen=N` — directly de-risks the VLEN-dependent failures we hit (the spacemit-gcc VLEN=1024 stack-smash `spacemit-bug-reports/…`, the vector-arg-spill L-best miscompile), without the board or the AI-thread switch.
- QEMU executes the **actual compiled binary**, so **compiler codegen bugs reproduce** — *provided CI builds with the same cross-toolchains* (spacemit-gcc for RISC-V, the vendor toolchains for others), not a generic compiler. Build the CI test binaries with the production cross-toolchain.

**Caveats — what QEMU does NOT cover (stays board-only):**
- **Performance** — QEMU timing is meaningless; all perf numbers (A100 vs X100, throughput, the memory-bound analysis) come from the board.
- **Hardware / microarch behavior** — real cache effects, hardware errata, and the SpaceMIT `/proc/set_ai_thread` VLEN-switch mechanism itself (under QEMU you set VLEN via `-cpu`, which is *simpler* but doesn't exercise the board's runtime switch).
- **AIE** — no standard QEMU; needs its own simulator/emulator for the correctness lane. The scalar oracle + golden vectors still apply — only the runner differs.

---

## 6. Two lanes, one contract

| Lane | Runs | Purpose | Gate |
|---|---|---|---|
| **Correctness** | native x86 + QEMU (RVV ×2 VLEN, aarch64, Hexagon; AIE via its sim) | bit-exact / tolerance vs the scalar spec & float model | CI, per-commit |
| **Performance** | K3 board (X100 cpu2, A100 cpu8 — see `RVV_PORT_ASSESSMENT.md` §5b) | real timing, VLEN behavior, `set_ai_thread` | manual / nightly |

Both lanes consume the **same** `nr_phy_test_ref` scalar references and golden vectors, so a board perf run and a CI correctness run agree on "correct." The board harness we already have becomes the performance lane; the ctest+QEMU work adds the correctness lane.

---

## 7. Phased plan

| Phase | Work | Verifies |
|---|---|---|
| **P0** | This strategy | — |
| **P1** | `nr_phy_test_ref` lib (fixed-point spec + float model + golden-vector gen) for one kernel (channel compensation); driver template; `add_test` on native x86 | x86 SIMDe/AVX == scalar spec |
| **P2** | `CROSSTEST_EMULATOR` CMake path; run P1 under `qemu-riscv64` at vlen=256 **and** 1024 | RVV == spec at both VLENs, in CI |
| **P3** | Migrate the remaining `rvv_harness` kernels (LLR, descramble, DFT, L-best) to the ref+driver+ctest shape; wire the tolerance tier for the float paths | full RX-kernel correctness suite, board-free |
| **P4** | Other-arch teams add `K_arch` for NEON/HVX/AIE against the **same** refs + vectors; AIE runner | one suite, all ISAs |

P1+P2 are the template; everything after is replication. The other-arch teams never re-derive test vectors or oracles — they implement `K_arch` and register it.

---

## 8. Open questions

- **Tolerance values** for the float-bearing kernels — pin `signDisagree` / max-abs-error / BLER-delta thresholds per kernel (start from the L-best's observed ~1–2% signDisagree).
- **CI toolchain pinning** — which cross-toolchain versions CI builds with (must match production so codegen bugs surface); how QEMU versions are pinned.
- **AIE correctness runner** — which simulator, and whether it can run the same driver binary.
- **Golden-vector storage** — generated on the fly from a seed (compact, reproducible) vs checked-in `.bin` fixtures (stable across ref changes). Prefer seed-generated with a versioned generator.
- **Where the fixed-point spec lives** — a dedicated `nr_phy_test_ref` under `openair1/PHY/…/tests`, sharing the exact constants (Q-format, sat points) with production headers so the spec can't drift from the kernels.

---

*Companion docs: `RX_FUSION_ASSESSMENT.md` (the `inner_rx` / tile contract whose scalar form is "kernel #0"), `RVV_PORT_ASSESSMENT.md` (§5 bit-exactness risk, §5b board run recipe), `nr_phy_common/src/nr_mimo_lbest_detector.md` (the float-reference / `signDisagree` oracle in practice).*
