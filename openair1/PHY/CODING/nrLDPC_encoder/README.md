# The NR LDPC Encoder: Algebraic Factoring and What Followed

*Design note, revised 2026-08-12. Describes how the encoders in this directory are
generated and why they look the way they do. Supersedes the original pre-implementation
proposal.*

---

## Summary

The generated 5G NR LDPC encoders expanded every parity row into a XOR of shifted
systematic terms. That is correct but throws away the base graph's structure: the four
core parity groups `P0..P3` end up pre-inverted into every row that references them, so
rows that should cost a handful of terms cost 70 to 171.

Recovering that structure symbolically over GF(2) reduces the arithmetic by 6.6x (BG1)
and 8.5x (BG2), identically at every lifting size. Realising it took three further steps
that mattered as much as the algebra: software-pipelining the kernel so no loop barrier
is needed, balancing what goes in each stage, and — the largest single effect on small
cores — abandoning the pre-rotated input layout.

Measured end to end, LDPC parity generation is 4.8x to 12x faster depending on base graph
and CPU, and the encoder is no longer the dominant cost of DLSCH encoding.

```
ldpctest, parity generation, us/CB      BG1 K'=8448 r1/3      BG2 K'=3840 r1/5
  Cortex-A72   (LX2160A)                97.80 ->  14.73        91.86 ->  7.76
  Cortex-A76   (RK3588)                 25.10 ->   5.28        22.78 ->  2.97
  Neoverse V2  (GH200)                  10.87 ->   2.16        10.37 ->  1.34
  Cortex-X925  (GB10)                    8.48 ->   1.44         5.95 ->  0.87

nr_dlsim, MCS 27 / 273 PRB, GH200       DLSCH encoding 99.92 -> 64.69 us/slot
                                        Eff Throughput 100.00, BLER 0.0
```

---

## 1. What the encoder has to compute

For BG1 the base graph is 46 x 68 over lifting size `Zc`: 22 systematic columns, four
*core* parity columns (22..25), and 42 *extension* parity columns (26..67), one per
extension row. BG2 is 42 x 52 with 10 systematic columns and the same four-column core.

Every nonzero entry is a cyclic shift, so a row is a GF(2) sum of shifted copies of
column groups. Writing `rot(X,s)` for a cyclic shift by `s` within a group, the base
graph says:

```text
core rows      0..3     involve systematic columns and columns 22..25 (dual diagonal)
extension rows i >= 4   involve systematic columns, a subset of 22..25,
                        and their own column 26+(i-4) with shift 0
```

The extension column appearing exactly once, with shift zero, is what makes encoding
straightforward: once `P0..P3` are known, every extension row is a direct sum.

---

## 2. The original architecture: full expansion

The stock generator solved for `P0..P3` symbolically and substituted the result into
every row that referenced them. What it emitted was, for each of the 46 rows, a single
flat XOR over shifted systematic groups:

```c
d2[0] = XOR(alignr(c2[20],c2[19],3), XOR(alignr(c2[5],c2[4],12), ... ));   // ~70 terms
```

This has a real advantage: all 46 rows are independent, so there is no dependency chain
anywhere and the scheduler has unlimited freedom. It is also what made per-row selection
conceivable, since no row needs any other.

The cost is that the four core groups are pre-inverted into everything. Term counts from
the repository's own tables (`no_shift_values_*`, `pointer_shift_values_*`,
`Gen_shift_values_*_Z_*`):

```text
BG1  2109 terms across 46 rows     rows 8, 11, 31 alone cost 171, 156, 153
BG2  1473 terms across 42 rows
```

For BG1 Zc=384 at 128 bit that is 2063 `xor` plus 1986 `alignr` per chunk iteration,
over 24 iterations — about 97k SIMD ALU operations per code block.

*Incidental:* the tables contain two redundant term pairs in row 8 (175 counted, 171
distinct), which the stock generator emitted as real work. GF(2) normalisation during
generation removes them.

---

## 3. Recovering the base graph

### 3.1 The kernel/extension split

For extension row `i >= 4`, seek

```text
P_i = R_i(S) + rot(P0,s0) + rot(P1,s1) + rot(P2,s2) + rot(P3,s3)
```

where `R_i(S)` holds only systematic terms and most of the `Pk` terms are absent. This is
the structure the expansion destroyed, and it is recoverable by symmetric difference over
GF(2): if `rot(Pk,s)` is a summand of `P_i`, then `P_i xor rot(Pk,s)` is sparse.

### 3.2 Back-substituting the kernel

The original proposal scoped this to `i >= 4` and treated `P0..P3` as a fixed cost. That
left the largest single block untouched: after factoring the extension rows, the four
kernel rows are 336 of the 534 remaining terms — 63%.

The kernel factors against itself, via the dual-diagonal back-substitution the expansion
had eliminated. BG1 at Zc=384:

```text
P0 :  67 terms   seed: the four core rows' systematic parts summed; irreducible
P1 :  84 terms -> rot(P0,1) + 17 systematic  = 18
P2 : 101 terms -> P0 + P1    + 16 systematic = 18
P3 :  84 terms -> rot(P0,1)  + 17 systematic = 18
```

`336 -> 121`, and the total from `534 -> 319`. So:

```text
extension factoring only     2109 -> 534    3.94x
+ kernel back-substitution   2109 -> 319    6.60x
```

The kernel is a dependency chain, not four independent rows — `P0 -> P1 -> P2` and
`P0 -> P3` — which becomes the central implementation problem in section 5.

### 3.3 Extension rows

17 of BG1's 42 extension rows reference the kernel; the other 25 are already sparse (2 to
8 terms) and are emitted unchanged. The dense ones collapse completely:

```text
P5  :  73 -> rot(P0,157)                 + 6      P30 : 104 -> rot(P2, 90)              + 3
P8  : 171 -> rot(P0,67) + rot(P2,170)    + 7      P31 : 153 -> rot(P0,258)+rot(P3,256)  + 2
P11 : 156 -> rot(P0,334)+ rot(P1,115)    + 5      P32 : 104 -> rot(P2,287)              + 3
P13 :  88 -> rot(P1,370)                 + 4      P35 :  70 -> rot(P0,266)              + 3
P15 :  89 -> rot(P3,269)                 + 5      P37 :  86 -> rot(P1,115)              + 2
P16 :  71 -> rot(P0, 57)                 + 4      P42 : 103 -> rot(P2,218)              + 2
P20 :  71 -> rot(P0, 59)                 + 4      P43 :  87 -> rot(P3,168)              + 3
P24 :  71 -> rot(P0,234)                 + 4      P44 :  70 -> rot(P0,274)              + 3
P29 :  87 -> rot(P3, 78)                 + 3
```

The worst case after factoring is 9 terms (row 8, from 171). There is no row for which
the expanded form is cheaper, so the per-row DIRECT/FACTORED choice the original proposal
called for does not exist.

### 3.4 Why the residual is the base graph

This is not a search that happens to succeed — it recovers 38.212 Table 5.3.2-2. After
factoring, the residual of row `i` is exactly that row's systematic base-graph entries,
and the shifts are the base-graph values for columns 22..25:

```text
row 4  -> 2 residual, no kernel reference     BG1 row 4 = {col 0, col 1, col 26}
row 5  -> 6 residual + P0                     BG1 row 5 = 6 systematic + col 22
```

The consequence that matters practically: **the structure cannot depend on `Zc`**, because
the base graph does not. Verified over all 51 BG1 lifting sizes — identical residual counts
and kernel-reference sets throughout, with a constant factored total of 319 terms for every
`Zc >= 20`. Below that the ratio degrades (2.13x at Zc=2) only because shift collisions
modulo a small `Zc` make the *expanded* form artificially cheap; those sizes never reach
the SIMD encoders.

It also means no new table is needed. The generator derives everything from the shift
tables already in the tree.

### 3.5 The one structural variation

The kernel reference *pattern* is identical at every `Zc` (`P1<-P0`, `P2<-P0,P1`,
`P3<-P0`), as are the residual counts (67/17/16/17) and the residual column sets. Only the
shift constants vary, in two families:

```text
46 of 51 Zc                P1=rot(P0,1)  P2=P0+P1        P3=rot(P0,1)
5  of 51 Zc  {13,26,52,104,208}   P1=rot(P0,0)  P2=rot(P0,105)  P3=rot(P0,0)
```

The second family is the `a=13` lifting set, and it is not really a second case: 105 mod
13/26/52/104 = 1. It is one base-graph constant reduced modulo `Zc`. Only Zc=208 is in the
live dispatch set, and it is the one lifting size where the pipeline lag of section 5.1
exceeds one chunk.

### 3.6 BG2

BG2 was listed as a non-goal ("unless the factoring framework makes it trivial"). It was
trivial, and it factors better:

```text
BG2  1473 -> 173 terms   8.04x - 8.57x     (BG1: 6.27x - 6.61x)
```

Its kernel is structurally simpler — all three back-substitution shifts are zero:

```text
P1 = P0 + 6 systematic
P2 = P1 + 8 systematic
P3 = P0 + 8 systematic
```

so it is entirely chunk-local and needs no pipelining at all. 27 of its 38 extension rows
reference the kernel. The same generator handles both base graphs with no BG-specific code
beyond `nrows`/`ncols`.

---

## 4. Deriving it

`ldpc_generate_factored.c`, reachable as `gen_code == 5`. Rows are built as GF(2) sets of
`(column, shift)` monomials directly from the shift tables, then factored by symmetric
difference. Three things it has to get right:

**Candidate shifts by correlation, not enumeration.** For rows `A` and `B`,

```text
|A xor rot(B,s)| = |A| + |B| - 2*hist[s]        hist[s] = |A intersect rot(B,s)|
```

so one pass over matching-column term pairs ranks every shift at once.

**Ties at the top matter.** Several shifts can give the same residual size while only one
admits a good *second* reference. Taking a single locally-best first shift silently
produces non-minimal results on exactly the four rows that need two references (BG1 rows
2, 8, 11, 31). The generator tries all shifts within a slack of the best. This was found
the hard way, twice.

**Guards, not assumptions.** It refuses to emit — rather than emitting something wrong —
when a kernel row references another kernel row non-chunk-locally, or when the pipeline
lag would not fit the loop. The latter correctly rejects Zc=16, where a parity group is a
single chunk.

Generator-time equivalence is checked by reconstruction; runtime equivalence by the tests
in section 7.

---

## 5. From algebra to code

The 6.6x is an operation count. Getting it took four decisions, and the first two moved
the result more than the algebra did.

### 5.1 A loop barrier costs more than the arithmetic saves

Kernel back-substitution alone removes 10.2% of BG1's terms. Emitted the obvious way — all
chunks of `P0`, barrier, then everything else — it came out **slower than the unfactored
encoder**:

```text
reference expanded  (2109 terms)   14637 ns/CB   1.000x
split-loop          (1894 terms)   14641 ns/CB   0.992x   <- slower
software-pipelined  (1894 terms)   11107 ns/CB   1.318x
```

Identical arithmetic in both of the factored rows. The entire difference is structure.

The fix is to pipeline rather than split: at chunk `i2` compute `P0[i2]`, and in the same
iteration compute the kernel rows for chunk `i2-LAG`, whose `P0` reads are then all
satisfied by chunks already produced. `LAG = max(shift/W)+1` is derived per lifting size
and per width; it is 1 almost everywhere and 7 for Zc=208 at 128 bit. An epilogue handles
the final `LAG` chunks, where the rotations wrap.

### 5.2 Stage rebalancing is worth 1.4x

Once the extension rows are factored too, a barrier before them is unavoidable — they
reference kernel shifts up to 370 bytes, 23 of 24 chunks, so no lag covers it. But *what
goes in each stage* matters:

```text
naive 2-stage      (319 terms)   3508 ns/CB   4.173x
rebalanced stages  (319 terms)   2518 ns/CB   5.813x
```

The naive split puts the kernel in stage 1 and all 42 extension rows in stage 2. Stage 1
is then a thin serial chain — `P0 -> kt -> P1 -> P2`, ~121 terms covering four dependent
steps. Moving the 25 extension rows that *don't* reference the kernel into stage 1 gives
the scheduler ~100 independent terms to overlap it against. Same arithmetic, same barrier,
1.4x faster.

### 5.3 Reading the kernel

`rot(Pk,s)` at arbitrary `s` is served from a doubled scratch copy of `P0..P3`
(`4 * 2 * Zc` = 3 KB), so any rotation is one unaligned load at a constant offset. Within
stage 1 the lag bound guarantees the read stays inside what has been produced, so the
kernel is read straight from the output there and the scratch is built once, afterwards.

### 5.4 The shift mechanism is ISA-specific, and measured

Three ways exist to obtain a byte-rotated vector:

```text
alignr / ext          128-bit only in practice: on AVX2 and AVX512 it is lane-local
                      and cannot express a full-width rotation at all
permutex2var_epi8     AVX512+VBMI, a true cross-64-byte permute
unaligned load        from the doubled buffer; works on every ISA, no shuffle
```

Two measurements decided this. First, `vpermi2b` costs **1.73x an unaligned 64-byte load**
on Sapphire Rapids (0.742 vs 0.428 ns/op) — it issues on one port while loads have several.
Corroborated independently: the stock 512-bit permutex encoder does *half* the loop
iterations of the stock 256-bit one and still takes 1.55x longer.

Second, at 128 bit the unaligned-load form is *not* better, despite isolated
microbenchmarks suggesting a 3-4% gain. In `ldpctest` it is consistently slower:

```text
parity generation      alignr        uniform loadu
BG1 K'=8448 r1/3       2.100 us      2.196 us       +4.6%
BG2 K'=3840 r1/5       1.250 us      1.390 us      +11.0%
```

`alignr` keeps systematic operands in registers across the many rows that share them,
which the load form gives up; that only shows under real register and cache pressure. So
the emitter chooses by width: **alignr at 128 bit, unaligned load at 256/512** where
alignr cannot work anyway.

### 5.5 The input layout: the largest effect on small cores

The stock encoders that cannot use `alignr` read a *pre-rotated* input — the systematic
buffer replicated `simd_size` times so every shift becomes an aligned load. That trades
memory footprint for instruction count, and the trade is far worse than it looks.

```text
BG1 Zc=384, x86 AVX2      2 * 22 * 384 * 64 = 1.03 MB per code block
BG2 Zc=384, aarch64       2 * 10 * 384 * 32 =  245 KB per code block
factored (any width)      2 * ncols * Zc    = 16.9 KB (BG1) / 7.7 KB (BG2)
```

Every factored encoder indexes the doubled buffer directly, so the replication is needed
only for lifting sizes with no factored variant. On aarch64 BG1 was already on alignr, but
**BG2 was not on alignr on any platform** — it paid the full 245 KB.

The penalty is twofold, and both halves are measurable. The copy itself:

```text
BG2 Zc=384 input preparation   4.867 -> 0.122 us    stack VLA 540 KB -> 16.9 KB
```

and, separately, the locality of *reading* a 245 KB buffer — see section 8, where it turns
out to be the dominant term on small cores.

---

## 6. Width selection is a microarchitecture question

The factored representation is width-independent — the same 319 / 173 terms whatever the
register size — so the same factorisation feeds a 128-, 256- and 512-bit emitter. Which to
*use* is not an ISA question: Zen 4, Zen 5 and Sapphire Rapids all define `__AVX512F__`,
but Zen 4 executes 512-bit operations as 2x256.

```text
factored, BG1 Zc=384, ns/CB              128     256     512   -> best
  EPYC 9374F      Genoa      (Zen 4)    1922     808     872      256
  EPYC 9575F      Turin      (Zen 5)    1516     630     438      512
  Ryzen AI MAX+   Strix Halo (Zen 5)    1448     612     425      512
  Xeon Gold 6433N Sapphire Rapids       2488    1012     827      512
```

The best achievable is nearly identical on Genoa and Sapphire Rapids (808 vs 827 ns) but
reached at *different widths*. Evaluating on Genoa alone would have concluded AVX512 was
not worth building.

A CPUID family table can express this only for parts that have been tested, and cannot
express within-family variation. So the width is **measured at startup**: encode a dummy
block at each compiled width, keep the fastest, prefer the narrower on a tie. Cost is about
0.6 ms. The CPUID table remains as a fallback (`OAI_LDPC_NO_CALIBRATE`), and
`OAI_LDPC_SIMD_WIDTH` overrides both.

That measurement runs from an **ELF constructor**, so it happens when
`load_module_version_shlib` dlopens `libldpc.so` — not inside the first encode, where it
would have cost 0.6 ms within a slot and raced between threads. It is not called from
`LDPCinit()` because that lives in `nrLDPC_decoder.c`, shared with `libldpc_orig.so`, which
does not include the encoder file. The lazy path remains for callers that reach the encoder
without the module being dlopen'd.

One caveat: the constructor runs on whichever core performs the dlopen, before thread
affinity is set. That only matters on a heterogeneous CPU, and the x86 parts carrying
AVX512 are homogeneous.

```text
                                128     256     512   -> calibrated   cpuid table
Genoa       (family 0x19)      1.919   0.810   0.882      256            256
Turin       (0x1A model 0x02)  1.505   0.610   0.417      512            512
Strix Halo  (0x1A model 0x70)  2.110   0.877   0.590      512            512
Sapphire Rapids (0x6 / 0x8F)   2.484   1.010   0.811      512            512
```

**Calibration and the table agree on all four parts tested — no case has been found where
calibration wins.** Strix Halo was tested specifically because Zen 5 APUs were the most
likely counterexample (mobile parts have been reported to double-pump AVX512 where server
parts do not, and both are family 0x1A); it turned out to be full width, so the table's
`family >= 0x1A -> 512` rule held.

Calibration is therefore insurance rather than a fix: it costs 0.6 ms and removes the need
to guess for parts nobody has run this on — Zen 5c-based APUs, future AMD families, and
Intel parts where AVX-512 frequency licensing might outweigh the width. It has not yet been
shown to beat the table on hardware.

---

## 7. Measurements

### 7.1 aarch64, ldpctest, pinned

```text
parity generation (us)          stock  factored  speedup      encoder total  speedup
BG1 K'=8448 r1/3
  Cortex-A72   (LX2160A)        97.80    14.73    6.64x      104.16 -> 20.88  4.99x
  Cortex-A76   (RK3588)         25.10     5.28    4.76x       27.83 ->  7.83  3.55x
  Neoverse V2  (GH200)          10.87     2.16    5.04x       11.93 ->  3.23  3.69x
  Cortex-X925  (GB10)            8.48     1.44    5.88x        9.29 ->  2.24  4.14x
BG2 K'=3840 r1/5
  Cortex-A72                    91.86     7.76   11.84x       96.23 -> 12.08  7.97x
  Cortex-A76                    22.78     2.97    7.66x       24.67 ->  4.65  5.30x
  Neoverse V2                   10.37     1.34    7.77x       11.09 ->  2.04  5.44x
  Cortex-X925                    5.95     0.87    6.87x        6.46 ->  1.35  4.77x
```

**`tparity` wraps the input preparation** — the "prep" statistic is nested inside it, not a
sibling. The parity column therefore includes the doubling, and for stock BG2 the
replication. This matters when comparing against the encoder-only figures below.

Rate does not affect the encoder: `-r/-d` never reaches it, and for K'=8448 BG1 the
puncturing terms are zero, so the r1/3, r2/3 and r22/25 rows are three measurements of an
identical computation. Their spread is a useful noise estimate — 0.2% on the GH200, ~4% on
the A72.

### 7.2 x86, encoder only, one binary, identical flags

```text
Sapphire Rapids, BG1 Zc=384
  stock AVX512 permutex    15430 ns/CB    1.00x
  stock AVX2 256            9986 ns/CB    1.55x
  factored 512               808 ns/CB   19.09x   (12.35x vs the best stock)
  factored 256              1010 ns/CB   15.28x
  factored 128              1321 ns/CB   11.68x
```

Two independent facts in that table. The 19.09x conflates factoring (6.6x) with dropping
permutex (~1.5-1.7x); they were not separated inside the encoder, only at the primitive
level. And **the stock AVX512 permutex path is 1.55x slower than the stock AVX2 path**, so
machines advertising AVX512VBMI currently select the slower of the two — a pre-existing
pessimisation, unrelated to factoring.

### 7.3 Full chain

```text
nr_dlsim, MCS 27 / 273 PRB / 1 layer, GH200      stock    factored
  DLSCH encoding time                            99.92 ->  64.69 us   1.54x
    LDPC parity generation                       11.36 ->   2.27 us   5.00x
    segmentation                                  5.74 ->   5.76 us     -
    LDPC input processing                         1.86 ->   1.85 us     -
    scrambling                                    2.53 ->   2.33 us     -
  Eff Throughput 100.00, Channel BLER 0.0, Avg round 1.00 on both
```

35 us saved per slot at full bandwidth and highest MCS. The LDPC encoder is no longer the
dominant cost of DLSCH encoding: of the remaining 64.69 us, parity is about 9 us (four code
blocks) against 5.76 us of segmentation.

### 7.4 Code size

```text
.text per encoder     BG1 Zc=384   25236 -> 5108 bytes   4.9x
                      BG2 Zc=384    9008 -> 2888 bytes   3.1x
```

### 7.5 Correctness

```text
70 generated encoders x 1000 trials   vs a scalar reference built from the shift tables,
                                      independent of any existing encoder
30 dispatched (BG,Zc) x 200 trials    through encode_parity_check_part_optim, at each of
  x 4 width settings x 5 machines     auto/128/256/512, on aarch64, Genoa, Turin,
                                      Strix Halo and Sapphire Rapids
same, with NO_FACTORED                the fallback path is unaffected
ldpctest                              BLER 0/100 at 3.5 dB
nr_dlsim MCS27 273PRB                 BLER 0.0 at 100.00 throughput
```

Trial patterns include all-zero, all-one, single-bit and full random *bytes* — random bytes
rather than 0/1 exercise all eight bit planes per trial. All widths produce bit-identical
output.

---

## 8. Where the speedup actually comes from

Two hypotheses were advanced and both were wrong; hardware counters settled it.

**It is not the instruction cache.** The stock BG1 encoder is 25 KB of `.text`, which
looked like a problem for small cores. It is not: on Zen 4 (32 KB L1I, *smaller* than the
A72's 48 KB) it takes 0.3 L1I refills per code block, and on the A72 itself 0.1. The loop
body executes 24 times per code block and stays resident.

Decomposing cycles into instructions and IPC, per code block, 128-bit encoders:

```text
Cortex-A72        cycles    insn    IPC       cyc ratio = insn ratio x IPC ratio
  BG1 stock       195966  190337   0.97
  BG1 factored     25936   28774   1.11        7.56x   =   6.61x   x   1.14x
  BG2 stock       129854   70837   0.55
  BG2 factored     12191   15354   1.26       10.65x   =   4.61x   x   2.29x

Zen 4
  BG1 stock       102247  204899   2.00
  BG1 factored      8242   23397   2.84       12.40x   =   8.76x   x   1.42x
  BG2 stock        30083   47722   1.59
  BG2 factored      4211   11703   2.78        7.14x
```

**BG1 is an instruction-count win and nothing else** — 6.61x measured against 6.60x from
the term count, with a small IPC bonus.

**BG2 is a data-cache result.** Its instruction ratio is only 4.61x, *below* its 8.51x term
ratio, because the factored version issues more instructions per term:

```text
instructions per term    BG1 stock 3.77   BG1 factored 3.76
                         BG2 stock 2.00   BG2 factored 3.70
```

Stock BG2 is the only one of the four not on alignr: pre-rotated aligned loads cost ~2
instructions per term instead of ~3.8. It pays for that with the 245 KB replicated buffer
against a 32 KB L1D — hence IPC 0.55. The factored version reads a 7.7 KB doubled buffer
that fits, and runs at 1.26. **BG2 wins 10.65x despite issuing more instructions, purely on
locality**, and the effect scales inversely with memory subsystem quality:

```text
BG2 speedup    A72 11.84x  |  Neoverse V2 7.77x  |  A76 7.66x  |  X925 6.87x
                 (weakest)                                        (strongest)
```

This is direct evidence for the AVX2 case: that path uses the same layout at 1.03 MB per
code block, so the win there is likely larger than the term count alone predicts.

---

## 9. Next phase: per-RV puncturing

The encoder currently computes all 46 parity rows for every transmission, whatever the
rate matcher will actually consume. That is measurable directly — the rate never reaches
the encoder, so the cost is flat:

```text
BG1 K'=8448 stock encoding    rate 1/3   12.618 us
                              rate 2/3   12.021 us
                              rate 22/25 11.966 us
```

At high rate most of those rows are discarded. Generating only the rows a given
`(E, RV, Ncb, filler)` selects is the remaining structural saving, and the factored
encoder makes it considerably easier than the expanded one would have.

### 9.1 What the windows cost

Approximate row ranges for the idealised full-buffer case, with the factored cost of each
window (kernel build included):

```text
case                      direct  factored  speedup   kernel needed
rate 1/3   all rows         2105       319    6.60x   P0..P3
2/3  RV0   P0..P12           772       180    4.29x   P0..P3
2/3  RV1   P0..P29          1298       259    5.01x   P0..P3
2/3  RV2   P13..P45         1333       260    5.13x   P0..P3
2/3  RV3   P36..P45          368       157    2.34x   P0..P3
8/9  RV0   P0..P4            338       123    2.75x   P0..P3
8/9  RV1   P0..P21          1117       227    4.92x   P0..P3
8/9  RV2   P13..P37         1055       231    4.57x   P0..P3
8/9  RV3   P36..P45          368       157    2.34x   P0..P3
```

Against the 319 terms of a full generation, a selected window costs 123 to 260 — so on top
of Phase 1 this is worth about **1.2-1.3x typically, and up to 2.6x** in the most
favourable case (8/9 RV0). On the `nr_dlsim` budget that is roughly 1-2 us of a 64.69 us
slot today; it becomes proportionally more attractive once the input/output path in
section 10 is addressed, and on any platform where parity is a larger share.

### 9.2 Two simplifications the factoring provides

**No direct-versus-factored decision.** The original proposal expected a per-window mode
choice, on the theory that a tail window excluding the kernel might prefer direct
generation. Measurement says otherwise: factoring wins in *every* window, including RV3.
Only 4 of the 10 rows in P36..P45 reference the kernel, but they carry 346 of the window's
368 terms — paying 121 to build the kernel in order to remove 346 is clearly profitable.
So the runtime always factors, and there is no second code path to write or validate.

**The kernel chain prunes.** Dependencies are `P0 -> P1 -> P2` and `P0 -> P3`, so a
selection touching only P0-referencing rows needs 67 terms rather than 121. Build the
transitive closure of the kernels the selected rows actually reference. In the windows
above the closure happens to be all four every time, so this is a refinement rather than
the main effect — but it is nearly free to implement.

### 9.3 Implementation notes

- Derive the row ranges from the **existing** 38.212 rate-matching walk rather than
  duplicating that logic in the encoder. One or two contiguous parity intervals is the
  natural representation; a small row mask is equally workable.
- Use the exact integer `E`. Nominal rates do not align to whole `Zc` groups, so a
  hard-coded per-MCS encoder would be wrong as well as unmaintainable.
- Computing a whole parity group when only part of it is consumed is the right first
  implementation; partial-group generation is not worth the complexity.
- The L1 is stateless across HARQ transmissions, so each slot independently computes what
  that `(E, RV)` needs. No HARQ state, no cross-slot caching.

### 9.4 What makes this riskier than Phase 1

Phase 1 could not change the transmitted bits — it computes the same 46 rows and was
verified bit-exact against a scalar reference. Row selection couples the encoder to the
rate matcher, and the failure mode is a wrong codeword rather than a slow one. The cases
that need care are exactly the awkward ones: filler bits, LBRM / reduced `Ncb`,
circular-buffer wraparound, and `E` not divisible by `Zc`.

Validation should compare the **final rate-matched bitstream** — not the parity rows —
against the current implementation, for RV 0..3 across representative `E` spanning ~1/3
through above 8/9, at every supported `Zc`.

---

## 10. What is not done

```text
BG2 Zc 72/88/104/120       8-byte aligned only; stock 64-bit encoders retained. The
                           generator is parameterised on shift/mask, so a 64-bit path
                           is a small change, but untested.
Zc = 16                    a parity group is one chunk; correctly rejected. Not dispatched.
AVX512 permutex path       still selected under NO_FACTORED despite being 1.55x slower
                           than the stock AVX2 path on Sapphire Rapids.
RISC-V RVV                 ported against the expanded encoder in a separate context, not
                           re-ported. `vslideup`/`vslidedown` have no lane restriction, and
                           the unaligned-load form works directly.
input/output processing    now ~1.0 us of BG1's 3.13 us encoding call, and segmentation is
                           5.76 us in nr_dlsim against ~9 us of parity. The next thing
                           worth attacking, and free of rate-matching risk.
stock encoder ASAN         the stock path reports a negative-size memcpy under ASAN on both
                           AMD machines. Pre-existing, not on the factored path, unexplained.
```

---

## Appendix: measurement method, and what it cost to get right

Platform notes: `falcon-gh200` (Neoverse V2, 72 cores), `dgx2-oai` (GB10, Cortex-X925),
`rock-5a` (RK3588, Cortex-A76 cores 4-7), `lx2160acex7` (LX2160A, Cortex-A72),
`peafowl` (EPYC 9374F, Genoa), `murrelet` (EPYC 9575F, Turin), `hafnium` (Ryzen AI MAX+ 395,
Strix Halo), `blabber` (Xeon Gold 6433N, Sapphire Rapids).

`bench_factored_ab.sh` A/Bs the two configurations through `ldpctest`, rebuilding only
`libldpc.so`. `icache_probe.c` reads cycles/instructions/L1I through `perf_event_open`
directly, so no perf userspace tool is needed.

Things that produced plausible but wrong numbers, all of which now fail loudly instead:

```text
sed pattern containing /*     '*' is a regex metacharacter, so the NO_FACTORED marker was
                              never removed and both columns measured stock -> uniform 1.00x
generator probed on PATH      ninja installed but a Makefile-configured tree
libldpc.so is dlopen'd        ldpctest has no build dependency on it, so 'make ldpctest'
                              left the previous module in place and the A/B compared a
                              build against itself -> uniform 1.00x again
hardcoded build directory     -> a full table of n/a rather than an error
repo-relative include path    on remote machines the harness compiled the checked-out
                              branch's file, not the one under test
```

The script now md5s `libldpc.so` between the two builds and refuses to print numbers if
they match. That guard caught the third case.

Two measurement lessons worth keeping:

**Isolated microbenchmarks misled twice on this workload**, both times in the optimistic
direction — the uniform-loadu decision (section 5.4) and the earlier 6.8x figure for BG1
Zc=384. Absolute encoder timings move 5.81x-6.80x for the same comparison purely with
binary layout. Prefer `ldpctest` and `nr_dlsim`.

**Pin the benchmark.** Unpinned on a 72-core host the same binary drifts 9-10% from
scheduler migration alone; pinned to an isolated core the spread is 1.1%. The machine was
idle in both cases.

---

## Source files

```text
ldpc_generate_factored.c              symbolic factoring and emission (gen_code == 5)
ldpc_encoder.c                        generator hook
ldpc_encode_parity_check.c            width dispatch, calibration, input preparation
ldpc{Zc}_factored_byte_{128,256,512}.c        BG1, 38/22/10 encoders
ldpc_BG2_Zc{Zc}_factored_byte_{128,256,512}.c BG2
bench_factored_ab.sh                  stock vs factored through ldpctest
icache_probe.c                        cycles/instructions/L1I via perf_event_open
```
