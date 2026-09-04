<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# Repository Guidelines for Agents

These guidelines provide standard default conventions for repository operations.
Explicit user instructions always take precedence and override these guidelines.

OpenAirInterface 5G: 4G/5G RAN stack (UE, eNB, gNB) implementing 3GPP standards.

## Build

Use `cmake` directly, not the `build_oai` wrapper (the exception: `./build_oai -I` once, to
install system dependencies on a fresh machine).

```bash
mkdir -p build && cd build
cmake .. -GNinja
ninja                    # or target it: ninja nr-softmodem nr-uesoftmodem
```

## Key directories

- `openair1`: PHY layer
- `openair2`: MAC / RLC / PDCP / RRC / SDAP layers
- `openair3`: NAS / NGAP / GTP layers
- `radio`: Radio drivers (rfsimulator, USRP, etc.)
- `fronthaul`: Native OAI 7.2x fronthaul split
- `executables`: gNB / UE / CU / DU top-level main entrypoints
- `ci-scripts`: Official CI test harness and execution scripts (see Verification below)

## Code style

See `doc/code-style-contrib.md`. In short: 2-space indent, no tabs, 132-column
limit, respect `.clang-format`, strong types (`c16_t` not `uint32_t` for IQ
samples), `AssertFatal()`/`DevAssert()` for invariants — not error handling.

Be concise and to the point in responses, commit messages, and code comments.
Don't leave "in-progress"/WIP/TODO markers or temporary debug logging
(`LOG_E`/print statements added only to trace a bug) in code you consider
finished — remove them before reporting the task done.

## Verification

Run relevant verification stages based on the code modified. Agents must use good judgment to
select which tests to run based on which subsystem was changed. For instance, modifying
`openair1` (PHY layer) usually means running physical layer simulation tests
(`ctest -R '^physim\.'` / phytest).

1. **Functional unit tests** — `ctest -E '^physim\.|^benchmark_'` in `build/`.
2. **Isolated benchmarks** — `ctest -R '^benchmark_'`. Skip unless the change
   touches perf-sensitive code (PHY inner loops, RLC, scheduler).
3. **Physim (phytest)** — `ctest -R '^physim\.'`. Essential when making changes to `openair1`.
   Requires configuring with `-DENABLE_PHYSIM_TESTS=ON` (the executables build unconditionally
   regardless of that flag; 0 matched tests means the flag is missing — reconfigure and rerun,
   don't treat 0 as a pass). Full run is slow; for a fast pass across every physim executable,
   use `ctest -L quick_physim` (well under 3 minutes) — good enough to catch a broken change
   quickly, but still not a substitute for the full set before release.
4. **Affected CI test case** — if the change affects RRC/NAS/PDU-session or rfsim behavior, run
   the testcase through `ci-scripts`. `run_locally.sh` takes a path to an **XML** testcase
   relative to `ci-scripts/`. Build the required images first, then run:
   ```bash
   docker build . -f docker/Dockerfile.base.ubuntu   -t ran-base
   docker build . -f docker/Dockerfile.build.ubuntu  -t ran-build
   docker build . -f docker/Dockerfile.gNB.ubuntu    -t oai-gnb
   docker build . -f docker/Dockerfile.nrUE.ubuntu   -t oai-nr-ue
   docker build . -f docker/Dockerfile.nr-cuup.ubuntu -t oai-nr-cuup
   cd ci-scripts && ./run_locally.sh xml_files/container_5g_rfsim.xml
   ```
   Rebuild the images after any code change before rerunning — Docker layer
   caching can silently serve a stale binary if only the final stage rebuilds.

**Lessons**:
- An equivalence/regression test must call the real production functions, not
  re-derive the expected values independently — a self-consistent-but-wrong
  test can pass for a long time while the code under test is broken.
- Green unit tests don't mean the feature works end-to-end — verify live
  (docker `ci-scripts` run or manual rfsim below) before calling it done.
- If a new feature fails, run the known-good baseline through the same path
  first to rule out an environment issue before debugging the feature.
- Hung process with no log output: `gdb -p <pid> -batch -ex 'thread apply all bt'`
  beats another add-logging-and-rebuild cycle.

## Manual RF-sim verification

For interactive checks beyond the automated stages,
run gNB and UE as plain host processes against a dockerized 5G core:

```bash
cd build
sudo ./nr-softmodem -O ../ci-scripts/conf_files/gnb.sa.band78.106prb.rfsim.yaml -E --rfsim \
  --gNBs.[0].NETWORK_INTERFACES.GNB_IPV4_ADDRESS_FOR_NG_AMF 192.168.71.190 --gNBs.[0].NETWORK_INTERFACES.GNB_IPV4_ADDRESS_FOR_NGU 192.168.71.190
sudo ./nr-uesoftmodem -O ../ci-scripts/conf_files/nrue.uicc.yaml -E --rfsim -r 106 --numerology 1 -C 3319680000
```

Present what you observe (RRC state, asserts, segfaults, "Bye." on shutdown)
without declaring pass/fail yourself — that judgment call is the user's.

## Notes

- Prefer adding new tests to `ctest` when you write them. Agents should always try to generate
  unit tests for new features or bug fixes.
- Agents must never post PR comments, create PRs, or push/update remote or upstream branches
  without explicit user instructions.
- Check `CONTRIBUTING.md` for licensing/contribution requirements.
