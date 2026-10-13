# physim_sweep

Exhaustive, high-SINR structural-parameter sweeps for the NR PHY link
simulators (`nr_pucchsim`, `nr_dlsim`, ...), run under AddressSanitizer/UBSan.

## Why this exists

`openair1/SIMULATION/tests/CMakeLists.txt` registers a fast suite of point
tests (`add_physim_test`): a handful of (SNR, config) points chosen to catch
link-performance regressions in a few minutes of `ctest`. That suite does not
catch a bug that only shows up at one specific structural value, e.g. a
starting-PRB computation that is wrong only near the edge of the BWP.

This tool takes the opposite approach for a smaller set of parameters: fix
SNR at a clean, high-SINR operating point and sweep one structural parameter
across its entire legal range, checking for bit errors, non-zero exit codes,
and ASan/UBSan reports at every point. It is meant to run as a long,
occasional job (nightly / on-demand), not on every commit — a full run is
thousands of simulator invocations.

For structural parameters bounded by BWP size, groups are duplicated across
"round" (106 RB) and the max, non-power-of-two NR bandwidth (273 RB) where
the simulator supports it.

Beyond BWP-bound structural parameters (PRB offset, SSB subcarrier offset,
MCS, NCS_config), three more dimensions are covered:

- **Numerology** (`numerology_sweep` groups, μ ∈ {0, 1, 3}): every simulator
  that exposes a numerology flag on its CLI gets one, at the
  (numerology, bandwidth) pairs already proven by the point-test suite.
  `nr_pucchsim` has none — it runs at a fixed internal numerology.
- **Symbols per slot** (`*_num_symbols_sweep_*` groups): PUCCH format 1
  (`-i`, 4..14 symbols) and PUSCH (`-b`, 1..14 symbols). PDSCH has no
  equivalent — `nr_dlsim` doesn't expose a symbol-count CLI flag.
- **PRACH format** (`prach_format_sweep_106rb`): format is derived from `-c`
  (`config_index`) via the 3GPP config-index-to-format table.

A failing point is a real bug the sweep found, not a configuration mistake to
work around — leave it in `sweeps.yaml` rather than adjusting the axis to
dodge it. Check `--log-dir` for the failing run's full output.

## Quick start

```sh
# 1. Build the physim executables with ASan
./cmake_targets/build_oai -P --sanitize-address --build-dir physim-sweep-asan

# 2. Run every configured sweep
python3 tools/physim_sweep/physim_sweep.py \
    --build-dir cmake_targets/ran_build/physim-sweep-asan/build

# Quick smoke run (1 in 8 points of every axis) while iterating on a change
python3 tools/physim_sweep/physim_sweep.py \
    --build-dir cmake_targets/ran_build/physim-sweep-asan/build --stride 8

# Just one channel/group, verbosely
python3 tools/physim_sweep/physim_sweep.py \
    --build-dir cmake_targets/ran_build/physim-sweep-asan/build \
    --channel nr_pucchsim --group format0_1bit_prb_sweep_51rb -v

# CI-style run: parallel, JUnit report, logs of failures kept for triage
python3 tools/physim_sweep/physim_sweep.py \
    --build-dir cmake_targets/ran_build/physim-sweep-asan/build \
    --jobs 16 --junit physim_sweep_results/junit.xml --log-dir physim_sweep_results
```

Exit code is non-zero if any point failed. Failing runs have their full
stdout/stderr saved under `--log-dir` (default `physim_sweep_results/`).

Run `--list` instead of executing to see exactly which commands a given
selection expands to:

```sh
python3 tools/physim_sweep/physim_sweep.py --build-dir <dir> \
    --channel nr_pucchsim --list
```

To sweep under UBSan too, build with `--sanitize-undefined` (or `--sanitize`
for ASan+UBSan together) instead of `--sanitize-address`.

## Adding a new channel or sweep

Edit `sweeps.yaml`. Each channel needs the executable name (`binary`, must
exist in the ASan build dir) and a list of `groups`. Each group is one
simulator invocation template plus the one parameter being swept:

```yaml
channels:
  nr_pucchsim:
    binary: nr_pucchsim
    groups:
      - name: format0_1bit_prb_sweep_51rb   # must be unique within the channel
        fixed_args: ["-R", "51", "-P", "0", "-b", "1", "-i", "1", "-n", "1000"]
        sinr: 20                            # single SINR point; do not put -s/-S in fixed_args
        axis: {flag: "-r", range: [0, 50]}  # inclusive; optional "step" key
        # or: axis: {flag: "-Z", values: [0, 1, 2, 5, 10, 15]}
```

An axis is usually one flag sweeping one value. Some parameters only have a
legal value in combination with another fixed one — numerology is the main
example: each numerology only accepts specific 3GPP bandwidths, so `N_RB_DL`
has to change together with `-m`/`-u`. For that, `flag` and `values` both
become lists — each inner list is one point's values for the corresponding
flags, in order:

```yaml
      - name: numerology_sweep
        fixed_args: ["-n", "300"]
        sinr: 25
        axis: {flag: ["-u", "-R"], values: [[0, 25], [1, 106], [3, 32]]}
        # expands to: ... -u 0 -R 25   /   -u 1 -R 106   /   -u 3 -R 32
```

`sinr` is turned into `-s <sinr> -S <sinr+0.05>` by the tool itself — do not
put `-s`/`-S` in `fixed_args`.

Guidelines for picking a sweep:

- Pick a parameter with a well-defined legal range tied to another fixed
  parameter (a PRB offset bounded by BWP size, a cyclic shift bounded by comb
  size, an MCS index bounded by the MCS table).
- Fix a single SINR point per group, high enough that every point in the
  swept range passes cleanly on a correct implementation.
- Keep `-n`/trial counts capped at 1000. Runtime is meant to come from
  covering many configs, not from a huge trial count per run.
- A new group is picked up automatically; no changes to `physim_sweep.py`
  are needed.

## How pass/fail is determined

A run is a PASS only if all of the following hold:
- the simulator process exits with code 0,
- it does not time out (`--timeout`, default 120s), and
- neither stdout nor stderr contains an ASan/LSan/UBSan report.

## Relationship to `ctest`

This tool is intentionally standalone rather than wired into
`add_physim_test`/`ctest`: registering every sweep point as its own `ctest`
test would balloon the test count into the thousands. Use
`openair1/SIMULATION/tests/` for fast per-commit checks, and this tool for
occasional/nightly exhaustive structural-correctness sweeps.
