# RVV LDPC Encoder Port: Conclusions and Implementation Plan

## Goal

Add an RVV backend for the OpenAirInterface LDPC encoder generator that works efficiently on both:

- 256-bit RVV implementations, such as the SpacemiT X100
- 1024-bit RVV implementations, such as the SpacemiT A100

The implementation should avoid reproducing fixed-width x86 `alignr` or `permutex2var` sequences when RVV can express the same operation more directly.

---

## Key conclusion

The generator already computes a width-independent logical byte index for every LDPC source term:

```c
int index =
    var * 2 * Zc
    + (i3 * Zc
       + (Gen_shift_values[pointer_shift_values[temp_prime] + i4] + 1) % Zc)
      % Zc;
```

The fixed-width SIMD backends only decompose this logical index afterward:

```c
vector_index = index >> shift;
byte_offset  = index & mask;
```

where:

```c
vector_width_bytes = 1 << shift;
mask = vector_width_bytes - 1;
```

For SSE, AVX2, and AVX-512, this decomposition is needed because the generated code treats `c` as an array of fixed-width vector objects and constructs an unaligned vector window using `alignr` or `permutex2var`.

For RVV, the same source window can normally be loaded directly from the flat byte buffer:

```c
vuint8m1_t x =
    __riscv_vle8_v_u8m1(c + position + index, vl);
```

Therefore:

> The RVV backend should retain and use the raw logical `index` values. It should not generate separate source-offset tables for each VLEN unless benchmarking shows a concrete benefit.

---

## Why the raw index is VLEN-independent

The current generated fixed-width form is conceptually:

```c
alignr(
    c2[(index >> shift) + 1],
    c2[index >> shift],
    index & mask
)
```

This returns one vector-width-sized byte window beginning at logical byte offset `index`.

That is equivalent to:

```c
load_vector_bytes((uint8_t *)c2 + index);
```

In the generated loop, `c2` is advanced by one SIMD block:

```c
c2 = &csimd[i2];
```

For a vector width of `B` bytes, this means:

```c
(uint8_t *)c2 == c + i2 * B;
```

So the complete source address is:

```c
c + i2 * B + index
```

For an RVV vector-length-agnostic loop, use a byte position instead:

```c
c + position + index
```

The same logical `index` works for 16-, 32-, 64-, and 128-byte vector widths.

---

## Output mapping

The current generator writes:

```c
d2[(Zc * row) >> shift] = result;
```

while `d2` already points to SIMD block `i2`.

In byte-address form, the destination is simply:

```c
d + row * Zc + position
```

The RVV store should therefore be:

```c
__riscv_vse8_v_u8m1(d + row * Zc + position, result, vl);
```

This removes all VLEN-dependent indexing from the `d[]` side.

---

## Recommended RVV kernel structure

Use byte elements with `LMUL=1`:

```c
#include <riscv_vector.h>
```

Suggested generated structure:

```c
static inline void
ldpc_BG1_Zc384_byte_rvv(uint8_t *c, uint8_t *d)
{
    for (size_t position = 0; position < 384; ) {
        size_t vl = __riscv_vsetvl_e8m1(384 - position);

        const uint8_t *c2 = c + position;
        uint8_t *d2 = d + position;

        /*
         * Generated code for every base-graph row.
         * Each source term uses c2 + logical_index.
         */

        vuint8m1_t acc0 =
            __riscv_vle8_v_u8m1(c2 + ROW0_INDEX_0, vl);

        vuint8m1_t x =
            __riscv_vle8_v_u8m1(c2 + ROW0_INDEX_1, vl);

        acc0 = __riscv_vxor_vv_u8m1(acc0, x, vl);

        /* More generated loads and XORs... */

        __riscv_vse8_v_u8m1(d2 + 0 * 384, acc0, vl);

        position += vl;
    }
}
```

This single generated function should run on both:

- X100: `VLMAX(e8,m1) = 32` bytes
- A100: `VLMAX(e8,m1) = 128` bytes

For `Zc = 384`, that gives:

- 12 iterations on X100
- 3 iterations on A100

No VLEN-specific source table is required.

---

## Generator changes

Add an RVV generation mode with the following behavior.

### 1. Keep the raw logical indices

The generator already fills `indlist[]` with raw `index` values for the `alignr` and `permutex` paths.

For RVV, always preserve:

```c
indlist[nind++] = index;
```

Do not transform the index into:

```c
(index >> shift, index & mask)
```

during generation.

### 2. Emit byte-addressed RVV loads

For every source term:

```c
__riscv_vle8_v_u8m1(c2 + indlist[k], vl)
```

No RVV gather, slide, `alignr` emulation, or two-vector permutation is required for this access pattern.

### 3. Emit byte-addressed output stores

For row `i1`:

```c
__riscv_vse8_v_u8m1(d + i1 * Zc + position, acc, vl);
```

### 4. Use a VLA loop

Prefer:

```c
for (size_t position = 0; position < Zc; ) {
    size_t vl = __riscv_vsetvl_e8m1(Zc - position);
    ...
    position += vl;
}
```

This allows one generated implementation to support arbitrary RVV VLEN values.

### 5. Do not select by the current `vl`

If fixed-VLEN generated variants are retained for experimentation, dispatch by hardware vector width:

```c
size_t vlenb = __riscv_vlenb();
```

Do not select a table based on the current `vl`, because `vl` may be smaller on the tail iteration.

---

## Multiple XOR accumulators

The original nested XOR expression creates one long dependency chain.

For rows with many terms, generate several independent accumulators:

```c
vuint8m1_t a0 = __riscv_vmv_v_x_u8m1(0, vl);
vuint8m1_t a1 = __riscv_vmv_v_x_u8m1(0, vl);
vuint8m1_t a2 = __riscv_vmv_v_x_u8m1(0, vl);
vuint8m1_t a3 = __riscv_vmv_v_x_u8m1(0, vl);

for (...) {
    a0 = __riscv_vxor_vv_u8m1(
        a0,
        __riscv_vle8_v_u8m1(c2 + index0, vl),
        vl);

    a1 = __riscv_vxor_vv_u8m1(
        a1,
        __riscv_vle8_v_u8m1(c2 + index1, vl),
        vl);

    a2 = __riscv_vxor_vv_u8m1(
        a2,
        __riscv_vle8_v_u8m1(c2 + index2, vl),
        vl);

    a3 = __riscv_vxor_vv_u8m1(
        a3,
        __riscv_vle8_v_u8m1(c2 + index3, vl),
        vl);
}

a0 = __riscv_vxor_vv_u8m1(a0, a1, vl);
a2 = __riscv_vxor_vv_u8m1(a2, a3, vl);
a0 = __riscv_vxor_vv_u8m1(a0, a2, vl);
```

Start with four accumulators and benchmark. Eight may also be worthwhile on the A100, but register pressure should be measured.

---

## Table-driven versus fully unrolled code

Two RVV code-generation strategies are worth benchmarking.

### Fully unrolled

Generate one explicit load and XOR per source term.

Advantages:

- constant offsets
- compiler can schedule loads aggressively
- no table lookup or loop overhead

Disadvantages:

- large generated functions
- instruction-cache pressure
- potentially very large compile times

### Table-driven

Generate one raw index table per row:

```c
static const uint16_t row0_indices[] = {
    307,
    76,
    205,
    /* ... */
};
```

Then execute a compact reduction loop.

Advantages:

- much smaller generated source and binary
- simpler generator
- easier validation

Disadvantages:

- runtime index loads
- less compile-time scheduling
- possible loop overhead

Recommended first implementation:

1. Generate raw index tables.
2. Use four accumulators.
3. Benchmark against a fully unrolled version for one representative BG/Zc combination.
4. Choose based on measured X100 and A100 performance.

The raw index tables are still independent of VLEN.

---

## Important validation point: padding and bounds

The RVV load:

```c
__riscv_vle8_v_u8m1(c + position + index, vl);
```

may read up to:

```c
position + index + vl - 1
```

The existing x86 implementation also reads a complete vector window using adjacent SIMD objects, so the required data should already exist in the prepared or duplicated `c` layout.

Nevertheless, verify explicitly that:

- all source rows have enough backing storage for the largest `position + index + vl`
- the final partial-VL iteration does not cross an invalid allocation boundary
- doubled or rotated source regions are initialized exactly as assumed

Add debug assertions during development.

---

## Correctness test plan

For every supported BG and Zc:

1. Generate scalar reference output.
2. Generate existing SSE/AVX output where available.
3. Generate RVV output on X100 and A100.
4. Compare the complete `d[]` buffer byte-for-byte.
5. Compare the final punctured encoder output byte-for-byte.

Test at least:

- smallest and largest supported Zc
- Zc divisible by 16, 32, 64, and 128
- Zc not divisible by hardware VLEN, to exercise tail `vl`
- BG1 and BG2
- random inputs
- all-zero input
- all-one input
- single-bit inputs

Also run the existing OAI LDPC unit tests and the `nr_dlsim` / `nr_ulsim` simulations.

---

## Performance measurements

Measure separately on X100 and A100:

- cycles or elapsed time per code block
- encoded throughput
- table-driven versus unrolled
- one, two, four, and eight accumulators
- `LMUL=1` versus larger LMUL only if larger LMUL improves useful work
- instruction-cache effects
- generated code size
- compiler output for redundant `vsetvli`

The initial implementation should use `e8,m1`.

---

## Suggested implementation order

1. Add a simple generated RVV backend using raw `indlist[]` indices.
2. Emit direct `vle8` loads from `c + position + index`.
3. Emit direct `vse8` stores to `d + row * Zc + position`.
4. Use a VLA `vsetvl` loop.
5. Validate against scalar output for every BG/Zc.
6. Add four independent XOR accumulators.
7. Benchmark table-driven versus fully unrolled generation.
8. Only investigate slides or gathers if direct unaligned loads are measurably slower.

---

## Bottom line

The fixed-width x86 generator needs `alignr` or `permutex2var` because it expresses the source as fixed-width vector objects.

The RVV backend should instead use the already-computed logical byte indices and perform direct unit-stride byte loads:

```c
source = vle8(c + position + index, vl);
destination = d + row * Zc + position;
```

This produces one vector-length-agnostic generated implementation that can run on both the 256-bit X100 and the 1024-bit A100.
