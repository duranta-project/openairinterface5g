<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# External NTN assistance over UDP

This opt-in O&M interface supplies dated satellite state and common timing
advance to an existing NTN serving cell. OAI broadcasts ordinary SIB19: the UE
does not receive UDP messages and does not need to implement this interface.
The producer can be an orbit service, an onboard navigation adapter or an RF
channel emulator. The interface does not generate or compensate RF impairments.

## Build and enable

The interface is built by default (`ENABLE_NTN_ASSISTANCE=ON`); no additional
`build_oai` switch is required. The normal `build_oai -I` dependency installer
installs Jansson development files (`libjansson-dev` on Debian/Ubuntu or
`jansson-devel` on RPM-based systems). Direct CMake users must provide the same
dependency. To omit the interface and its Jansson dependency from a minimal
build, configure with `-DENABLE_NTN_ASSISTANCE=OFF`. An existing CMake cache
retains an explicitly selected OFF value; changing the default does not override
it. The shared top-level configuration also resolves Jansson for a UE-only build
unless this opt-out is selected; the UE does not run the UDP service.

Runtime remains disabled by default: merely building the interface does not
start its listener or control thread. With an otherwise working NTN gNB
configuration, add this top-level section:

```text
ntn_assistance = {
  enabled = 1;
  bind_address = "127.0.0.1";
  peer_address = "127.0.0.1";
  port = 9760;
  allow_remote = 0;
};
```

`port = 9760;` is shown explicitly for clarity and may be omitted to use that
default. `enabled = 1;` is required to start the service. These fields can also
be supplied as normal OAI overrides, such as `--ntn_assistance.port 9760`.

The existing NTN configuration must contain one SIB19-only v17 SI message.
Static configuration still supplies cell identity, the SI schedule, k-offset
and other unchanged NTN fields. Do not run another updater for the same cell.
When this interface owns a cell, its legacy RFsim SIB19 updater is excluded.

Remote use requires `allow_remote = 1`, explicit numeric IPv4 addresses and an
isolated/trusted management network. Source-address and producer-port checks
are **not authentication, encryption or protection against IP spoofing**. Do
not expose this service directly to an untrusted network. No firewall settings
are changed by OAI. Binding to an unavailable address fails startup.

## Datagram and ownership rules

Each UTF-8 JSON object occupies one UDP datagram, at most 1200 bytes. No line
terminator is required. Duplicate keys, unknown fields, invalid numbers,
unsupported versions/kinds and truncated or oversized datagrams are rejected.
Malformed, wrong-peer and rate-limited datagrams receive no reply. Well-formed
correlated requests can receive a diagnostic `result` instead of success.
The service allows a burst of eight packets and replenishes one packet/ms.

Use one persistent UDP socket for the producer's lifetime. Identify the cell
by MCC followed by the two/three MNC digits in `plmn` and its 36-bit NR cell
identity in `nci` (not PCI). The following are **canonical decimal strings**,
not floating-point JSON numbers: `request`, `nci`, `session`, `sequence`, epoch
`generation` and `subframe`. Leading zeros are prohibited except for `"0"`.
PLMN is a separate fixed-width string and retains its leading zeros.

Send `hello` to acquire/discover the current producer session:

```json
{"version":1,"type":"hello","request":"1","cell":{"plmn":"00101","nci":"1"}}
```

Substitute the configured cell identity. A successful response has
`type:"status"`, the same `request`, `result:"ok"`, a nonzero `session` and
capabilities. Use the returned session on subsequent `time`, `status` and
`update` requests. Changing the local UDP port creates a different producer.
Ownership can transfer only after five seconds of inactivity and with no
pending or currently schedulable active update. Sessions are not credentials.

Correlate responses, set a finite receive timeout and handle loss/reordering.
Accepted update sequences strictly increase within a session. An invalid or
stale update does not consume its sequence or partially change the channel
assistance. An acknowledgement means **staged**, not transmitted or received.
If an acknowledgement is lost, inspect status; do not assume the update failed.

## Epoch and state

`time` and `status` use the hello fields plus `session`. A valid
`scheduler_epoch` is an extended downlink **1 ms subframe** and generation.
It is neither UTC nor a sample count, and is independent of numerology. Normal
SFN wrap preserves the generation; a timeline discontinuity changes it.
Never infer absolute time from a host process start, packet arrival or SFN alone.

The producer dates the physical state at this cell epoch before sending it;
the gNB does not replace that epoch with its current scheduling time plus a
lead when the datagram arrives. Delivery or encoding delay can therefore make
an update too late for admission/publication, but cannot silently move the time
that its state describes. For physical emulation this removes arrival-relative
epoch-stamping error, not the producer's clock-mapping uncertainty, radio-time
observation error, timestamp/ASN quantization or uncalibrated RF frontend delay.
The optional observation below accounts for scheduler-to-radio timing using an
RX sample anchor and explicit TX advance; the scheduler epoch alone is not a
calibrated RF timestamp.

An update is a complete dated state, for example:

```json
{
  "version":1,"type":"update","request":"3",
  "cell":{"plmn":"00101","nci":"1"},"session":"123","sequence":"1",
  "subject":{"kind":"serving"},
  "epoch":{"kind":"cell","generation":"2","subframe":"12500"},
  "ephemeris":{"kind":"ecef","position_m":[7000000,0,0],"velocity_mps":[0,7000,0]},
  "ta":{"common_us":0,"drift_us_per_s":0,"drift_variant_us_per_s2":0},
  "ul_sync_validity_s":5
}
```

The example numbers are illustrative, not a usable satellite scenario. Obtain
the current session/generation and date the physical state at the requested
epoch. Provide enough lead for encoding, the configured SI schedule and the
complete repetition window. The accepted future horizon is 10239 subframes;
past epochs or other generations are rejected. New accepted states coalesce
the pending candidate. Publication swaps the complete MAC NTN configuration
and encoded SIB19 together at an eligible SI boundary. Repetitions in the same
window use the same selected state.

| Field | Physical units | Quantization | Allowed quantized integer |
|---|---|---|---|
| Each position component | metres, ECEF | 1.3 m | -33554432..33554431 |
| Each velocity component | m/s, derivative in rotating ECEF | 0.06 m/s | -131072..131071 |
| `common_us` | microseconds | 0.004072 µs | 0..66485757 |
| `drift_us_per_s` | µs/s | 0.0002 µs/s | -257303..257303 |
| `drift_variant_us_per_s2` | µs/s² | 0.00002 µs/s² | 0..28949 |

Finite physical values must already lie inside the representable range; they
are rounded to the nearest grid point, ties away from zero, never clipped.
Velocity is not inertial-frame velocity. The unsigned TA drift-variant field
is the TS 38.331 field, not unrestricted signed acceleration.
`ul_sync_validity_s` accepts 5,10,...,60,120,180,240,900. Do not substitute total
service-link propagation delay for common TA: select the correct physical NTN
architecture. In particular, a regenerative service link can have zero common TA
while still having nonzero physical propagation delay.

`status` distinguishes latest staged state from the active state and reports
`scheduled_at` after scheduler admission. Neither proves RF delivery or UE
decoding. After the dated broadcast window expires, OAI suppresses stale SIB19
instead of presenting the same wrapped epoch as a future cycle. UE use of an
already received state for its signalled UL validity duration is a different
matter. Loss of the producer does not mute the gNB; an emulator must independently
enforce its own controller-loss and RF safety policy.

## Optional radio-time observation

For one local RF RU, gNB 0 and CC 0, `time` can also return `radio_time` from the
already-open UHD device. Unsupported, stale or discontinuous observations are
`null`; never replace that with an invented zero offset. The interface does not
open another USRP or change its clock/time source. The generic cell-time
interface remains usable without a radio backend observation.

The record contains an extended cell `epoch`, `anchor_generation`, signed tick
strings `rx_frame_ticks`, `now_ticks`, `tx_advance_ticks`, the configured
`timestamp_rate_hz`, and monotonic observation/query brackets. Its reference is
`rx_sample_zero`. Project an epoch by its integer subframe difference at the
reported rate; subtract TX advance **once** to obtain the associated TX sample
timestamp. A backend's configured tick rate is not a frequency calibration.
The helper rejects host or device observations older than 200 ms.

The producer must establish its clock mapping and uncertainty independently.
Shared PPS is one possible mapping, not a protocol requirement. Without it,
bracketed observations can bound offset; rate error makes that bound grow.
Reacquire after a session/generation change and stop using a mapping once its
declared uncertainty limit is exceeded. These records alone do not establish
UTC alignment, RF frontend group delay, phase coherence or inter-radio frequency
lock. The two host monotonic clocks must not be treated as the same clock.

## Scope and checks

Version 1 supports serving-cell ECEF state, common TA and cell-relative epochs.
Capabilities and typed `kind` fields reserve explicit extension points for
orbital elements, neighbours, handover and other time representations; these
features are not silently accepted or claimed today. OAI does not propagate a
TLE through this interface. Existing whole-stack slot storage limits the current
implementation to µ0..3; the wire format has no fixed sample rate or SCS.

With tests enabled, build `test_ntn_assistance_codec`,
`test_ntn_assistance_server`, `test_ntn_assistance_publisher` and
`test_ntn_radio_time`, then run `ctest --output-on-failure -R '^test_ntn_'`.
The fixtures exercise production parsing, quantization, ownership, real ASN
encoding/decoding, SI-window consistency, clock discontinuities and fake-backend
failure paths. They do not replace live SIB19/traffic or physical-clock tests.

The physical fields follow TS 38.331 V17.3.0 NTN-Config/EphemerisInfo; the
ephemeris O&M role is described in TS 38.300 §16.14.7. The JSON/UDP transport is
an OAI implementation interface, not a new 3GPP air-interface message.
