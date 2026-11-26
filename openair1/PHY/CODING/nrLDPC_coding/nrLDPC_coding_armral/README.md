<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# LDPC decoder on the Arm RAN Acceleration Library (`libldpc_armral.so`)

`libldpc_armral.so` is an LDPC library for aarch64 CPUs. It has the same
interface as the default `libldpc.so` and uses the same segment coding (rate
matching, deinterleaving, thread pool tasks) and the same encoder. Only the
decoding of a code block is done by the layered offset min-sum decoder of the
[Arm RAN Acceleration Library](https://gitlab.arm.com/networking/ral)
(ArmRAL, BSD-3-Clause), which runs on NEON or SVE.

It works for the gNB (PUSCH) and the nrUE (PDSCH), and in the physical
simulators.

## Decoder behaviour

- **Early termination** on the code block CRC: CRC24B for a transport block
  with several code blocks, the transport block CRC (CRC24A or CRC16) for a
  single code block. ArmRAL checks it after every iteration. Without a CRC
  check function (e.g. `ldpctest`), the decoder stops when all parity checks
  are satisfied.
- **LLR scaling:** ArmRAL clamps its input to ±32 and decodes with scaled
  offset min-sum on int8 (offset 2, scale 0.75), so its result depends on the
  LLR scale. In OAI that scale depends on the receiver and the modulation
  (mean |LLR| of a few units with a weak front end, 10-100 in
  `nr_dlsim`/`nr_ulsim`). Large LLRs saturate the messages during the
  iterations (error floor at low and medium code rates); small LLRs lose the
  check messages to the fixed offset (high code rates fail, and with a weak
  receiver all rates). Each code block's mean |LLR| is therefore brought into
  the range the decoder needs, by code rate R = K'/E (E = rate-matched bits,
  from the position of the last non-zero LLR, without fillers):
  - R ≤ 2/3: target T = max(36·R, 6); above T the LLRs are scaled down to T,
    below T/2 multiplied by the integer T/mean (lossless, at most 16);
  - R > 2/3: no down-scaling; below a mean of 32 multiplied by the integer
    64/mean (at most 16).

  The rule was derived with `ldpctest` (BG1/BG2, rates 1/5 to 22/25, 8 and 20
  iterations) and checked on `nr_dlsim`/`nr_ulsim` cases not used to derive it
  (QPSK to 256QAM, MCS tables 1 and 2, 1 and 2 layers, 24 to 273 PRB, LLRs
  reduced 4 to 32 times to emulate a weak receiver) and over a cable.
- **Return value** as for `libldpc.so`: the number of iterations, or
  `numMaxIter` if the code block failed (the abort flag of the transport block
  is then set, like with OAI's decoder).

## Limits

- ArmRAL needs fewer filler bits than non-punctured message bits
  (K' > 2 Z_c). Code block segmentation per TS 38.212 5.2.2 always gives
  that; only artificial cases such as `ldpctest -l1` stop with an assertion.
- Effective code rates close to 1 (about 0.95 and above) are less tested
  than the rest of the range.

## Build

ArmRAL 26.01 or later is needed (tested with 26.07). Its CRC functions use
the polynomial multiply instructions (PMULL, part of the Armv8 crypto
extension), so the CPU must have them (`pmull` in `/proc/cpuinfo`). ArmRAL
has to be built as position-independent code, because it is linked into a
shared module. Example for a Cortex-A76 (Raspberry Pi 5) with NEON; use
`-DARMRAL_ARCH=SVE` (or `SVE2`) on CPUs with SVE:

```bash
git clone https://gitlab.arm.com/networking/ral.git
cd ral
git checkout armral-26.07
cmake -S . -B build -GNinja -DCMAKE_BUILD_TYPE=Release -DARMRAL_ARCH=NEON \
      -DARMRAL_OPT_FLAGS=-mcpu=native -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
      -DBUILD_TESTING=OFF -DCMAKE_INSTALL_PREFIX=/opt/armral
ninja -C build armral
sudo cmake --install build
```

The FHI 7.2 library (`-DOAI_FHI72=ON`, BFP compression on aarch64) uses the
same ArmRAL installation; its functions are unchanged from 25.01 to 26.10.
Both libraries link their own copy of ArmRAL, and `libldpc_armral.so` keeps
its ArmRAL symbols local, so they can be loaded into the same gNB.

Then build OAI with the library, from the top of the OAI tree:

```bash
cmake -B build -GNinja -DENABLE_LDPC_ARMRAL=ON -Darmral_LOCATION=/opt/armral
ninja -C build
```

## Usage

Select the library at run time, as for the other LDPC libraries:

```bash
sudo ./nr-uesoftmodem ... --loader.ldpc.shlibversion _armral
sudo ./nr-softmodem ... --loader.ldpc.shlibversion _armral
./nr_dlsim ... --loader.ldpc.shlibversion _armral
./ldpctest ... -v _armral
```

## Tests

With `-DENABLE_PHYSIM_TESTS=ON`, ctest gets `ldpctest`, `nr_dlsim` and
`nr_ulsim` cases with the label of the library (`ctest -R ldpc_armral`).
