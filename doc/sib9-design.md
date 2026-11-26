<!-- SPDX-License-Identifier: CC-BY-4.0 -->
# Basic SIB9 design and traceability

## Data flow and publication

The CU parses `sib9_config`, constructs SIB9 and UPER-encodes it for F1 Setup.
The DU validates every activated cell and its complete SI list, builds candidate
SI messages and copies of SIB1 for all cells, then publishes the complete Setup
under the scheduler lock. No cell status is advertised until every candidate
encoding succeeds.
The ordinary SI scheduler selects the payload for each transmission.

The CU owns the configured timestamp/metadata and its outgoing encoded buffers.
The DU owns independently decoded input. Candidate trees, SIB1 and all three SI
payloads are encoded before any active pointer/length/mapping changes. A rejected
candidate frees only candidate allocations. The publication result distinguishes
rejected, unchanged and published. `nr_mac_configure_f1_sibs` prepares every
cell before committing any candidate. A malformed cell or late encoding failure
rejects the whole Setup and retains all previous cell snapshots; an initial
rejection cannot activate a cell. A replaced F1 Setup copy is released.

Ordinary SIB2/3/4, basic SIB9 and the existing v17 SIB19 use distinct payloads/windows.
SIB19 carries satellite/NTN assistance. Its existing path is covered
by a regression fixture. SIB9
refresh owns a temporary copy of its SI tree, changes only timeInfoUTC and swaps
its tree/bytes after successful encoding. No F1 buffer, scratch encoder buffer or
cross-cell global pointer backs a retained broadcast. Teardown frees the retained
SIB9 SI tree with its ASN.1 descriptor in `nr_mac_cell_free`. Unchanged Setup still resets the time cache.

## Host-to-window estimate

Let N be slots per 10-ms frame, X the current unwrapped scheduled TX slot,
R the corresponding RX-slot start, W the SIB9 window start, and L its length.
The next frame boundary is B = N * ceil((W + L) / N).
The host clock is sampled while preparing the first SIB9 occasion and is treated
as an estimate of RX-buffer end R + 1. The estimated target instant is therefore:

```text
lead_slots = (X - R) modulo (1024 * N)
projection_ns = (B - X + lead_slots - 1) * (10,000,000 / N)
estimated_unix_ns = sampled_CLOCK_REALTIME_ns + projection_ns
timeInfoUTC = floor((estimated_unix_ns + 2,208,988,800 * 1,000,000,000) / 10,000,000)
```

The final formula is evaluated using checked wide integer arithmetic. Invalid
nanoseconds, negative projection or a value outside the 39-bit UTC domain reject
the window. The slot context must match the scheduled TX frame/slot and give a
plausible forward RX-to-TX distance. Unsupported external nFAPI contexts are not
silently interpreted as local radio timing. Normal SFN wrap advances the unwrapped
slot counter; discontinuity resets cached window identity.

The host sample can occur later than RX-buffer end. Processing, buffering and RF
delays therefore remain unmeasured error sources. TS 38.331 defines the target
boundary; associating the host sample with RX-buffer end is an implementation
estimate. CLOCK_REALTIME synchronization is the operator's responsibility.

## Requirement, source, implementation and test mapping

The following clauses define the implemented fields and procedures:

- [TS 38.331 V17.3.0](https://www.etsi.org/deliver/etsi_ts/138300_138399/138331/17.03.00_60/ts_138331v170300p.pdf):
  6.3.1 SIB9, printed pp.474-475 (PDF pages 475-476); SI acquisition 5.2.2.3.2,
  pp.44-46; SI change rules 5.2.2.2.2, pp.42-43.
- [TS 38.470 V19.0.0](https://www.etsi.org/deliver/etsi_ts/138400_138499/138470/19.00.00_60/ts_138470v190000p.pdf):
  5.2.2, printed p.10 (PDF page 11), CU/DU SI ownership and DU SIB9 re-encoding.
- [TS 38.473 V19.1.0](https://www.etsi.org/deliver/etsi_ts/138400_138499/138473/19.01.00_60/ts_138473v190100p.pdf):
  8.2.3, pp.35-38, F1 Setup; 9.3.1.42, p.324, CU SI container.

| Requirement | Source | Production implementation | Verification |
|---|---|---|---|
| Legal basic timestamp, units, epoch, optional metadata | 38.331 6.3.1 | `nr_sib9_read_profile`, `get_SIB9_NR`, `nr_sib9_time_to_utc` | `test_sib9_config`: independent zero/one UPER bytes, 1900 origin, field endpoints, metadata overflow, omission versus zero |
| Explicit static/realtime policy; zero is content | Local interface policy | `nr_sib9_read_profile`, `nr_sib9_read_mode` | Actual libconfig fixtures, invalid modes, conflicting fixed/realtime value, read failure |
| Standard CU SIB container over F1 | 38.470 5.2.2; 38.473 8.2.3/9.3.1.42 | `do_SIB9_NR`, CU Setup builder, F1 SI-list encoder | `f1ap_lib_test`, `test_sib9_pipeline`: production APER, two cells/two SIBs, sender buffers freed |
| Valid complete SI snapshots | 38.331 SI acquisition; local atomicity policy | `decode_cu_si`, `prepare_si_candidate`, `nr_mac_configure_f1_sibs`, `update_SIB1_NR_SI` | Pipeline rejection/rollback, malformed/duplicate/unsupported input, second-cell encode failure, Setup-wide rollback, replacement/removal and active cell teardown |
| Dedicated SIB9 SI and sibling isolation | Local failure-isolation policy | SIB1 scheduling builder, other-SIB scheduler, `nr_sib9_payload` | Separate decoded mappings/payloads, failed window with unchanged SIB1/SIB2 |
| Correct radio-window reference instant | 38.331 SIB9 field description | `nr_sib9_slot`, `nr_sib9_payload` | `test_sib9_timing`: known RX/TX lead, frame rounding, rollover and numerologies |
| Constant bytes within one window | Local cache policy | Per-cell `sib9_window` | Different SSB occasions share a read/payload; following window resamples |
| Time-only refresh does not change tags | 38.331 SIB9 field description | Refresh changes only SIB9 UTC | Byte-for-byte SIB1 preservation through refresh and failures |
| Failed input never yields stale fallback | Local host-estimate policy | Window attempt/valid state and fallible encoders | Failed read, failed encode, invalid range/context; same-window suppression and next-window recovery |
| Actual receiver compatibility | Implementation evidence, not a normative accuracy claim | Existing stock UE decoder | Self-contained static/realtime/disabled split and monolithic RF-simulator cases |

## Interfaces and exclusions

Public configuration-to-wire mapping is in [the user guide](sib9.md). Internally,
`tx_func` supplies a stack-owned `nr_slot_time_t` through the local
`nr_slot_indication` callback. `nr_mac_handle_slot_indication` resolves the cell
by PHY ID and calls the scheduler under its lock. `nr_sib9_slot` retains only
scalar timing context, never the caller's pointer. External nFAPI callers pass
NULL; the PNF adapter ignores the local context. No encoded F1/nFAPI field or
radio-plugin ABI changes.

The baseline ASN.1 source is unchanged. The implementation excludes R16 precise
reference-time construction, TSS, dedicated time delivery, propagation-delay
compensation, UHD clock/rate-domain repair, runtime CU profile transactions,
automatic SI availability withdrawal, 5GC time services and a new NR-DC UE.

Tests call production parsers, constructors, APER/UPER codecs, publication and
refresh functions. Clock/encoder failures are injected at narrow external call
boundaries. Fixed expected calendars, wire bytes and frame calculations are not
derived by the same production helper under test. Benchmarks measure refresh
and cached payload preparation separately. The local review bundle records the tested revisions, commands and results.
`ci-scripts/tests/test_run_sib9.py` checks cleanup after Docker stop and removal
failures, including retry when a container could not be removed.

## Configuration-to-wire validation

| Local input | Scope / default / presence | Legal value and unit | Wire / validation |
|---|---|---|---|
| `cu_sibs` | CU; existing default excludes 9 | Existing supported SIB list; add 9 explicitly | F1 SIB type 9; CU SIB-list validation |
| `sib9_config.timeSource` | CU; static | static or realtime | None; `nr_sib9_read_profile` |
| `timeInfoUTC` | CU; required static, forbidden realtime | 0..549755813887; 10 ms since 1900 | `timeInfo.timeInfoUTC`; signed config bound check before conversion |
| `dayLightSavingTime` | CU; absent unless supplied | 0..2; standard two-bit DST code | Optional two-bit field; 64-bit input bound check, constructor and received constraints |
| `leapSeconds` | CU; absent unless supplied | -127..128; GPS minus UTC seconds | Optional integer; 64-bit input bound check, constructor and received constraints |
| `localTimeOffset` | CU; absent unless supplied | -63..64; 15-minute units | Optional integer; 64-bit input bound check, constructor and received constraints |
| `sib9_timing.mode` | Each DU; static | static or realtime | None; `nr_sib9_read_mode`; realtime requires integrated L1 |

Zero is present content when configured. No mode or omitted-field sentinel is
encoded. The CU profile is fixed for its normal configuration lifetime; each DU
selects its local refresh policy independently. Unknown mode names and illegal
or contradictory values fail startup. No OS-derived metadata is inferred.

## Allocation, ownership and failures

| Object | Allocation / transfer | Release | Commit point / rejection |
|---|---|---|---|
| CU standalone SIB9 | Constructor owns ASN.1; encoder returns owned byte array | CU Setup builder frees tree; F1 copy lifecycle frees encoded buffers | Invalid profile fails before CU construction |
| Incoming CU SI bytes | F1 message owns bytes; decoder borrows only during call | F1 message lifecycle | No borrowed buffer retained |
| DU candidate SI trees | Strict UPER decode; moved into local candidate lists | Common reject/unchanged cleanup, correct ASN.1 descriptor | Any malformed member rejects the entire Setup |
| Setup candidate array | Bounded by configured/requested cell counts; independent per-cell candidates | All rejected candidates or successful ownership transfers | Reject duplicate/unknown cells and PCI mismatch before any cell publication |
| Candidate SIB1 | Deep copy of active per-cell SIB1 | Rejected/unchanged copy freed; old active tree freed on replacement | All candidate encodings succeed before pointer/byte/length swaps |
| Active regular/v17 payloads | Copied into per-cell fixed arrays | Cell lifetime | Published with matching SIB1 under scheduler lock |
| Active SIB9 tree/payload | Candidate ownership transferred to per-cell state | Replacement/removal or `nr_mac_cell_free` | Rejected candidate retains prior snapshot; initial rejection does not activate |
| F1 Setup response copy | Owned deep copy after successful SI publication | Next accepted Setup and normal MAC teardown | Reject returns before successful activation path |
| Realtime candidate | Deep copy of cell-owned SIB9 SI; update UTC and encode scratch bytes | Failure frees candidate; success frees/replaces old tree | Failed window emits no SIB9, retries next window; SIB1/siblings untouched |
| Window cache | Per-cell scalar state | Reset on replacement, repeated accepted Setup, slot discontinuity, cell teardown | Timestamp-only refresh does not update valueTag |

F1 UE-context Reset clears UE state, not broadcast cell SI. Repeated Setup resets
the cache. Cell teardown releases retained SI; no new F1 Reset semantics are added.

## Existing implementation to this contribution

| Earlier component | Decision | Reason / replacement |
|---|---|---|
| Basic SIB9 constructor, standalone codec | Keep and simplify | Basic timestamp plus explicit optional metadata only |
| CU fixed-time configuration | Rewrite | Static requires explicit value; default static; no legacy omission aliases |
| Multi-SIB F1 list allocation | Keep | Standard containers require one independent allocation per SIB |
| F1/DU SI assembly | Rewrite | Validate complete list and stage every cell's SIB1 plus payloads before Setup-wide publication |
| SIB1 mapping and SI scheduling | Rewrite | Dedicated SIB9 window/payload; retain existing SIB19 handling |
| Generic time-provider enum/interface | Remove | Two local policies need no provider framework |
| Radio/UHD sample-domain timing | Remove | Host estimate needs only internal RX/TX slot positions |
| Host UTC refresh | Rewrite | Per-cell, per-window sample; checked epoch conversion; no stale fallback |
| R16 reference-time and precision helpers | Remove | No precise-time construction or manufactured uncertainty |
| TSS, RRC time events and runtime profile transactions | Remove | Outside basic broadcast scope |
| ASN.1 grammar/compatibility generator | Remove | Existing basic ASN.1 type suffices; independently fixed vectors retained |
| Opaque-extension configuration | Remove | No public arbitrary-byte configuration |
| Legacy CI and lab/modem/core tooling | Rewrite/exclude | Self-contained RFsim profiles and stock UE observations |
| Stage diagnostics, temporary logs and site paths | Remove | Only operator-facing refresh unavailable/recovered messages remain |
| Test evidence and legal provenance | Keep separately | New tests accompany production changes; historical results are not new results |

The local cleanup audit records the decision for each file in the earlier series.

## Changed-file inventory

The four change groups cover configuration/codec, atomic F1 publication, DU
refresh and receiver tests. The inventory below records file ownership and the
complete diff against the recorded develop base.

| File | Kind | Patches | Net lines + / - |
|---|---|---|---|
| `CMakeLists.txt` | existing | 1, 2, 3 | +3 / -0 |
| `ci-scripts/conf_files/sib9/cu.conf` | new | 4 | +51 / -0 |
| `ci-scripts/conf_files/sib9/du-mcg.conf` | new | 4 | +199 / -0 |
| `ci-scripts/conf_files/sib9/du-scg.conf` | new | 4 | +199 / -0 |
| `ci-scripts/make_sib9_runtime.py` | new | 4 | +50 / -0 |
| `ci-scripts/run_sib9.py` | new | 4 | +272 / -0 |
| `ci-scripts/tests/test_run_sib9.py` | new | 4 | +53 / -0 |
| `ci-scripts/xml_files/container_sib9_basic.xml` | new | 4 | +12 / -0 |
| `doc/sib9-design.md` | new | 4 | +217 / -0 |
| `doc/sib9.md` | new | 1, 2, 3, 4 | +176 / -0 |
| `executables/nr-gnb.c` | existing | 3 | +2 / -1 |
| `nfapi/oai_integration/vnf/nfapi_nr_vnf.c` | existing | 3 | +11 / -11 |
| `openair2/F1AP/lib/f1ap_interface_management.c` | existing | 2 | +9 / -7 |
| `openair2/F1AP/tests/f1ap_lib_test.c` | existing | 2 | +4 / -1 |
| `openair2/GNB_APP/gnb_config.c` | existing | 2, 3 | +29 / -0 |
| `openair2/GNB_APP/gnb_sib9.c` | new | 1 | +71 / -0 |
| `openair2/GNB_APP/gnb_sib9.h` | new | 1 | +9 / -0 |
| `openair2/LAYER2/NR_MAC_gNB/config.c` | existing | 2, 3 | +200 / -90 |
| `openair2/LAYER2/NR_MAC_gNB/gNB_scheduler.c` | existing | 3 | +7 / -1 |
| `openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_bch.c` | existing | 2, 3 | +11 / -4 |
| `openair2/LAYER2/NR_MAC_gNB/mac_proto.h` | existing | 2, 3 | +9 / -2 |
| `openair2/LAYER2/NR_MAC_gNB/mac_rrc_dl_handler.c` | existing | 2 | +17 / -2 |
| `openair2/LAYER2/NR_MAC_gNB/nr_mac_cell.c` | existing | 2 | +1 / -0 |
| `openair2/LAYER2/NR_MAC_gNB/nr_mac_fapi.c` | existing | 3 | +2 / -2 |
| `openair2/LAYER2/NR_MAC_gNB/nr_mac_fapi.h` | existing | 3 | +4 / -1 |
| `openair2/LAYER2/NR_MAC_gNB/nr_mac_gNB.h` | existing | 2, 3 | +10 / -0 |
| `openair2/LAYER2/NR_MAC_gNB/nr_radio_config.c` | existing | 2 | +68 / -15 |
| `openair2/LAYER2/NR_MAC_gNB/nr_radio_config.h` | existing | 2 | +2 / -1 |
| `openair2/LAYER2/NR_MAC_gNB/nr_sib9_mac.c` | new | 3 | +98 / -0 |
| `openair2/LAYER2/NR_MAC_gNB/nr_sib9_mac.h` | new | 3 | +17 / -0 |
| `openair2/LAYER2/NR_MAC_gNB/tests/test_nr_other_sib_ssb_index.c` | existing | 2 | +1 / -1 |
| `openair2/NR_PHY_INTERFACE/NR_IF_Module.c` | existing | 3 | +3 / -1 |
| `openair2/NR_PHY_INTERFACE/NR_IF_Module.h` | existing | 3 | +6 / -1 |
| `openair2/RRC/NR/MESSAGES/asn1_msg.c` | existing | 1 | +19 / -0 |
| `openair2/RRC/NR/MESSAGES/asn1_msg.h` | existing | 1 | +2 / -0 |
| `openair2/RRC/NR/MESSAGES/tests/CMakeLists.txt` | existing | 1, 2, 3 | +50 / -0 |
| `openair2/RRC/NR/MESSAGES/tests/test_sib9_config.c` | new | 1 | +165 / -0 |
| `openair2/RRC/NR/MESSAGES/tests/test_sib9_fixture.h` | new | 2 | +82 / -0 |
| `openair2/RRC/NR/MESSAGES/tests/test_sib9_pipeline.c` | new | 2 | +235 / -0 |
| `openair2/RRC/NR/MESSAGES/tests/test_sib9_timing.c` | new | 3 | +156 / -0 |
| `openair2/RRC/NR/nr_rrc_defs.h` | existing | 2 | +3 / -0 |
| `openair2/RRC/NR/nr_sib9.c` | new | 1 | +58 / -0 |
| `openair2/RRC/NR/nr_sib9.h` | new | 1 | +30 / -0 |
| `openair2/RRC/NR/rrc_gNB_du.c` | existing | 2 | +13 / -0 |
| `tests/nrdlbench/dlbench.c` | existing | 3 | +1 / -1 |

The local review bundle supplies per-file purpose, exact source SHA-256 values,
ordered patch hashes and fresh application/reversal verification.
