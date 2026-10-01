<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# Phase 7 E2E status (du_fronthaul branch)

## 2026-09-30: parity with the vendor path reached

Stock OAI UE attaches, gets a PDU session and pings 0% loss both ways over the native DU
(3/3 runs, vendor 2/3 on the same host; PUCCH DTX 0, DL BLER 0). The earlier sections below are
history; the causes of the "synch Failed" bug were:

1. **ru_thread clock was open-loop.** DL slots were paced with `usleep()`, UL slots by blocking
   on UL jobs, plus a frozen "timing correction". It drifted from `fh_timer`, so every DL send
   was "too late" (0 DL U-Plane on the wire). Now `du_fh` emits one event per OTA slot (DL or
   UL) from the timer tick once the slot's UL window closed, like xran's per-slot rx callback,
   and `south_in` blocks on it (`du_fh_wait_slot`).
2. **Pre-fftshift PRB layout.** Since 044ca835d6 `txdataF_BF`/`rxdataF` hold symbols in wire
   order (first negative subcarrier first) and `rxdataF` is indexed per `RU_RX_SLOT_DEPTH` slot.
   The shim still swapped the band halves (SSB landed 53 PRBs off) and ignored the slot index.
3. **No PRACH C-Plane.** Section type 3 was never sent; now built with xran's formulas.
   PRACH IQ was also never handed to L1 (`prach_l1rx_queue`), and occasions were looked up with
   the subframe instead of the slot.
4. **Ta3 window.** The O-RU sends UL one symbol after OTA; `Ta3 = (100, 500)` rejected 82% of
   UL U-Plane as early (dead PUCCH). The native conf now uses `Ta3 = (0, 500)`.

Status of the native DU fronthaul (`radio/fhi_72_native/`) E2E validation.
Unit tests (`test_du_fhi_symbols`, `test_du_fhi_lookahead`,
`test_du_packet_processor`, `test_du_tx_scheduler`, `test_du_fh`) all pass as
of this writing.

## Recap: what Phase 5 delivered (already committed/stable)

`radio/fhi_72_native/` — a from-scratch compat shim exposing the same
`transport_init`/`get_internal_parameter`/`fh_if4p5_south_in`/`fh_if4p5_south_out`
ABI as the vendor `radio/fhi_72/` wrapper, backed by `fronthaul/du/du_fh.h`
instead of libxran. Files: `du_fhi_core.[ch]` (TDD classification, catch-up,
UL-grant look-ahead, FFT-shift helpers — no L2 dependency), `du_fhi_prach.[ch]`
(PRACH scheduling, needs L2), `du_fhi_isolate.[ch]` (the ABI surface),
`du_fhi_config.[ch]` (config loader).

## Phase 7 goal

Pair `nr-softmodem` (DU, using `du_fhlib_native.so` instead of vendor
`oai_transpro`) against `nr-oru` (RU, already on the native `fronthaul/oru`
stack) over the user's DPDK VF loopback (`0000:05:02.2/.3` ↔ `0000:05:02.0/.1`),
using the existing `ru.band77.mu1.106rb.1x1.conf` / `test_oru.yaml` harness,
and get `nr-uesoftmodem` to sync (SSB/PBCH → PRACH → RRC).

## New files/changes this phase (uncommitted)

- `targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.band77.mu1.106rb.fhi_native.1x1.conf`
  — new conf, copy of the vendor `...fhi.1x1.conf` with the `fhi_72` section
  replaced by a `fhi_72_native` section (dpdk devices, worker_core, ru_addr,
  mtu, comp_type, iq_width, T1a_cp_dl/ul, T1a_up, Ta3, prach_eaxc_offset/kbar)
  and a `device: { name = "du_fhlib_native"; };` block added. Reuses the same
  VFs/MACs as the vendor conf (no new loopback wiring needed). Numerology,
  num_prbs, duplex mode, TDD pattern, and PRACH config index are **not**
  duplicated here — `du_fhi_config.c`'s `get_du_fh_options()` derives them from
  `openair0_cfg`/`split7` (same source `radio/fhi_72/oran-config.c` uses),
  matching how the vendor section already works.
- `radio/CMakeLists.txt` — added `OAI_FHI72_NATIVE` option.
- Transport selection mechanism confirmed: `device.name` config key (generic
  `radio/COMMON/common_lib.c` `load_lib()`/`DEVICE_SECTION` path, same lever
  rfsimulator/USRP use) — no build-time symlink fighting needed, vendor and
  native coexist as separate `.so`s in the same build dir.
- Full build (`nr-softmodem`, `nr-oru`, `nr-uesoftmodem`, `vrtsim`, etc.) done
  in `/home/bpodrygajlo/github/worktrees/du_fronthaul/build_du` with
  `-DOAI_RU_FRONTHAUL=ON -DOAI_FHI72_NATIVE=ON -DENABLE_TESTS=ON` (vendor
  `OAI_FHI72` intentionally left off to skip the external xran download/build).

## Bugs found and fixed this phase

1. **Hyperframe domain mismatch (catastrophic, fixed).** The shim's own
   `hyper_frame_estimate`, tracked locally via a wrap-detection heuristic
   starting from 0, was in a completely different numbering domain than
   `fh_timer`'s real absolute-symbol clock (`fronthaul/common/fh_timer.c`
   computes it from GPS-epoch-relative wall-clock time — a huge number).
   Every DL/UL scheduling call computed an `absolute_symbol` far in
   `du_tx_scheduler`/`du_packet_processor`'s "past" relative to their
   real-time-anchored clocks, so ~97-100% of scheduling calls were dropped as
   "too late", with the drop rate *growing* over the run (confirmed via two
   stats snapshots: 148K→688K drops while successful sends stayed flat at
   4968) — a genuine unbounded drift, not a fixed offset.
   - **Fix** (per user's suggestion): stopped tracking hyper_frame locally.
     `du_fhi_core.c` now has `du_fhi_query_hyper_frame(du_fh_handle, frame)`,
     which calls `du_fh_get_utc_anchor_point()` (the fronthaul library's own
     real-time hyperframe source) fresh on every `south_out` call, with a
     small ±1 correction (`du_fhi_hyper_frame_wrap_adjustment()`, unit tested)
     for the rare case where the query-time anchor and the caller's own frame
     straddle the 1023→0 wrap boundary.
   - **Result:** DL scheduling went from ~97% "too late" drops to zero drops.

2. **`south_in` per-symbol vs per-slot granularity mismatch (fixed).**
   `ru_thread` expects one `fh_if4p5_south_in()` call to deliver a *whole
   slot's* worth of UL data (vendor's xran aggregates all antennas/symbols of
   a slot into one ready notification before signaling); `du_fh_read_ul_iq()`
   is per-symbol. The shim was calling it exactly once per `south_in`
   invocation and overwriting `ru_thread`'s own `*frame`/`*slot` counters with
   whatever single symbol happened to be next in the ready queue. A first fix
   (draining `du_fhi_count_ul_symbols_in_slot()` worth of reads per call)
   narrowed but did not eliminate the problem.
   - **Root cause, fully identified:** vendor's xran produces one ready
     notification *per real slot*, DL or UL, which is why vendor's south_in
     can safely resync `ru_thread`'s `(frame, slot)` against it on every call.
     `du_fh` only ever produces ready jobs for slots that actually carry UL
     symbols under the TDD pattern — there is nothing to resync against on a
     pure-DL slot. Forcing a resync from a UL slot's data on every call
     repeatedly snapped `ru_thread`'s `(frame, slot)` back to whichever
     UL-bearing slot (3 or 4 in this 5-slot-period config) was last read,
     starving every pure-DL slot — where `phy_procedures_gNB_TX`
     (SSB/PBCH/PDSCH generation) actually runs — of ever being reached.
   - **Fix:** `du_fhi_south_in()` no longer resyncs `ru_thread`'s counters at
     all. It trusts the caller-supplied `(*frame, *slot)` as ground truth
     (`proc->tti_rx`/`frame_rx` set directly from them), and only uses UL job
     reads to (a) fill `rxdataF` when the current slot has UL symbols
     scheduled, logging (not correcting) any mismatch, and (b) pace the loop
     via `du_fh_read_ul_iq()`'s blocking wait. Pure-DL slots — for which
     nothing paces the loop — get a plain `usleep(slot_duration_uS)` instead
     of racing ahead unbounded.
   - **Result:** "Received Time doesn't correspond" mismatch spam went from
     hundreds of thousands per run to **zero**. DL content became non-zero
     for the first time (5000+ non-zero-content hits logged at frame=100,
     slot=0, symbols 0 and 2-10 — exactly where SSB/PBCH/SIB1 PDSCH should
     land). DU↔RU packet counts stayed healthy at very high volume (one run:
     1,247,994 DL U-Plane symbols scheduled / 1,247,712 sent by the DU vs.
     1,243,427 U-Plane packets received at the RU; U-Plane Timing Late
     Errors: 26 out of ~470K UL packets — negligible).

## Where it stands now: parity with the vendor path, not full UE sync

With both fixes in place, `nr-uesoftmodem` still fails `synch Failed` on
every attempt against the native DU.

**Control experiment run to isolate whether this is a bug in the native
module or something environmental:** copied the pre-built vendor
`liboran_fhlib_5g.so` (+ its `libxran.so` dependency) from the main repo's
`develop`-branch build into this worktree's `build_du/`, built a throwaway
conf selecting it via `device: { name = "oran_fhlib_5g"; }` on top of the
otherwise-unmodified vendor `fhi_72` section, and ran the exact same
`nr-oru` + `nr-uesoftmodem` pairing against it.

**Result: the vendor module shows the same failure mode in this environment.**
The vendor-backed gNB transmitted healthy, steady DL traffic (confirmed via
its own `[o-du 0][tx ... pps ... kbps]` counters). The UE got one partial
detection — `Frame 626, slot 0, SSB Index 0. Error decoding PBCH!` (i.e. it
found the SSB but failed to decode PBCH) — then fell back into the same
`synch Failed` retry loop indefinitely, never reaching stable sync, over a
sustained run.

This means the native fronthaul module has reached **functional parity**
with the vendor module in this specific test environment: both move DL/UL
data reliably end-to-end, and neither achieves a stable UE sync here. The
most likely explanation is environmental/channel-related rather than a
fronthaul-transport bug — `nr-oru`'s own log shows it's running a TDL-A
fading channel model via vrtsim (`Model 0 (TDL-A), DS 10.0ns, Speed 1.5m/s`),
which is a plausible source of occasional-to-persistent PBCH decode failures
independent of which fronthaul transport is underneath. This was not
root-caused further — it's now arguably out of scope for "get the native
DU fronthaul working," since the vendor reference has the identical symptom.

## Cleanup done

The temporary debug instrumentation added during this investigation
(periodic `du_fh_print_stats()` call and the DL-content non-zero scan in
`du_fhi_south_out()`) has been removed from `du_fhi_isolate.c`. The
control-test artifacts (copied vendor `.so`, throwaway conf) were also
removed — they were never part of the repo, only dropped into `build_du/`
and `/tmp`.

## Files touched this phase (uncommitted, `git status` at time of writing)

```
 M radio/CMakeLists.txt
?? radio/fhi_72_native/                                     (Phase 5+7 work)
?? targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.band77.mu1.106rb.fhi_native.1x1.conf
```
(`build_du/` is gitignored and not shown.)

## Suggested next steps

1. If full UE RRC sync is still wanted, the likely next lever is the vrtsim
   channel model config (try a non-fading/AWGN-only or perfect-channel model
   to isolate PBCH decode from fading effects) rather than further changes to
   `radio/fhi_72_native/` — the transport layer now appears healthy on both
   the native and vendor paths.
2. Phase 7's original exit criterion ("Full split-7.2 loopback DL+UL+PRACH
   passes") should probably be re-scoped given the above: the DL+UL packet
   exchange *does* pass reliably; PRACH hasn't been exercised yet since the
   UE never reaches the RA procedure. Consider treating transport-level
   parity with the vendor module as the practical Phase 7 success criterion
   in this environment, with full UE attach deferred pending the channel
   model question.
3. Commit the `radio/fhi_72_native/` Phase 5 work plus the Phase 7 fixes in
   `du_fhi_core.c`/`du_fhi_isolate.c` (the hyperframe-query change and the
   `south_in` redesign) — all currently uncommitted.

## 2026-09-04: rebased onto develop, re-verified

Branch rebased from `fb944fbad6` onto current `develop` (`ceccfc8ffa4`, 280
commits ahead); the single `DU fronthaul` commit replayed with no conflicts,
uncommitted WIP (`radio/fhi_72_native/`, `radio/CMakeLists.txt`, the
`fhi_native` conf) stashed/restored across the rebase cleanly.

- **Build break from the rebase (fixed):** `du_fhi_config.c`'s
  `get_du_fh_options()` read `openair0_cfg->nr_scs_for_raster`, which commit
  `efcbf14c274` ("Remove nr_scs_for_raster, introduce mu for 7.2") removed
  from `openair0_config_t` somewhere in the 280 rebased-over commits, moving
  the numerology into `split7_config.mu` instead (see how
  `radio/fhi_72/oran-config.c` already reads it). Fixed by changing the read
  to `openair0_cfg->split7.mu`.
- **Stale-plugin ABI mismatch (found, root-caused, fixed by full rebuild —
  not a code bug):** after the rebase, an incremental `make` of just the
  target binaries left `libvrtsim.so` un-rebuilt (mtime predated the rebase)
  while `nr-oru` itself got relinked against the post-rebase
  `openair0_device_t`/`openair0_config_t` layout. Loading the stale plugin
  into the fresh executable corrupted the device vtable — calling
  `rfdevice.get_timestamp()` actually jumped into `vrtsim_end()` (visible via
  `gdb bt`: `oru_north_read_worker` → a `get_timestamp` call landing in
  `vrtsim_end`/`shm_td_iq_channel_abort`, SIGSEGV on a mutex lock in freed
  memory) and crashed `nr-oru` within ~3s. Root cause: `make <target>` doesn't
  rebuild dlopen'd device `.so`s that aren't link-time dependencies of the
  target; 10461 of ~11000 `.o` files in `build_du` were stale relative to the
  post-rebase `common_lib.h`. Fixed with a full `make -j$(nproc) all`, after
  which `libvrtsim.so` rebuilt (confirmed fresh mtime) and the crash was
  gone. **Lesson: after rebasing a worktree branch whose base moved past a
  struct-layout change in a shared header, always do a full rebuild before
  trusting an E2E run — target-scoped incremental builds can silently leave
  ABI-mismatched dlopen'd plugins in place.**
- **E2E re-run result (superseded, see correction below):** initially looked
  unchanged from the Phase 7 finding above — DU↔RU exchanged well over a
  million U-Plane symbols with zero errors, `nr-uesoftmodem` still hit
  `synch Failed`. This was **wrong** — see next section.

## 2026-09-04 correction: "parity with vendor" was a false conclusion

User pushed back: they can connect the UE with the vendor xran library, which
contradicts the Phase 7 "control experiment" claim that vendor also fails
`synch Failed` in this environment. Re-investigation found **two confounds**,
both now fixed/isolated, and **one still-open, now-confirmed-real bug**:

1. **Build-type confound (fixed).** `build_du` was configured
   `CMAKE_BUILD_TYPE=Debug` (unoptimized, `-g` only), unlike the working
   reference build (`CMAKE_BUILD_TYPE` unset → defaults to `RelWithDebInfo`,
   `-O2 -g -DNDEBUG`, per top-level `CMakeLists.txt:136-141`). The Debug build
   made `nr-oru`'s real-time DL packet-processing loop exceed its own symbol
   budget (effective ~40.2-40.6us vs. 35.71us budget — visible via nr-oru's
   own `[DIAGNOSIS] DL CRITICAL` / `OVERALL STATUS: FAIL` self-check, which
   the original Phase 7 write-up never looked at). Confirmed as a pure
   build-config issue (not fronthaul-specific) by pairing `build_du`'s
   `nr-oru` against the **vendor** DU: same overrun reproduced. Fix:
   `cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo .` + full `make -j$(nproc) all`.
   **Always build this dir RelWithDebInfo/Release, never Debug, before
   trusting any real-time/E2E result.**
2. **Stale DPDK/process state (fixed).** Leftover RU/DU/UE processes from
   earlier kills kept holding DPDK VFIO device handles and
   `/var/run/dpdk/<prefix>/config` locks — a single `pkill -f "a|b|c"` call
   silently failed to match/kill them in this environment (needs verifying
   with `ps` and killing remaining PIDs individually), causing spurious
   `EAL: Cannot create lock` / `DPDK device not found` crashes on the next
   launch that looked like fronthaul bugs but weren't.
3. **Real bug, now actually isolated (still open).** With the RelWithDebInfo
   rebuild and clean DPDK state, RU-side real-time performance is fully
   healthy (`DL PASS`, 0 FAIL) for both vendor and native DU fronthaul. Under
   these fair conditions: **vendor xran reliably syncs the UE** (full
   RRC_CONNECTED, NAS registration, clean SINR ~43dB) within ~12s — this
   directly disproves the original "control experiment" (which must have been
   run under contention/an unfair timing race, or hit the same undetected
   build/state confounds). **Native DU fronthaul still fails UE sync 100% of
   the time** — 600+ `synch Failed` attempts over 45+s, zero PBCH/SSB
   detection ever — even with zero transport-level symbol drops and healthy
   RU processing throughout. This is a genuine, reproducible, native-DU-
   fronthaul-specific bug, most likely in SSB/PBCH DL content generation or
   timing (`fronthaul/du/du_tx_scheduler.c`, `du_packet_processor.c`,
   `radio/fhi_72_native/du_fhi_*.c`) — not environmental, not a build
   artifact, not RU-side throughput. **Root cause not yet found.** Next step:
   capture and diff the actual DL C-Plane/U-Plane wire content for the SSB
   symbols between native and vendor paths (pcap) to find the concrete
   corruption, rather than trusting aggregate symbol-count stats — those
   looked "healthy" in the original Phase 7 run and hid this bug.
