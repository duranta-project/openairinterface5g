<!-- SPDX-License-Identifier: CC-BY-4.0 -->

[[_TOC_]]

The telnet NTN module (`telnetsrv_ntn.c`) updates the NTN assistance information
that the gNB broadcasts in SIB19 while it runs. It is meant for setups in which
the satellite channel is emulated outside of the gNB, e.g. SDR boards connected
through a hardware channel emulator: the emulator, or the script driving it,
knows where the satellite is and pushes it to the gNB. With RFsimulator, the LEO
channel models update SIB19 themselves and this module is not needed.

# General usage

Run the nr-softmodem with an NTN configuration (see the [NTN configuration
page](../../../../doc/ntn-configuration.md)), activate telnet and load the `ntn`
module:
```
./nr-softmodem -O <config> --telnetsrv --telnetsrv.shrmod ntn
```

# Update SIB19

```
ntn update_sib19 <ta_common> <drift> <drift_variant> <x> <y> <z> <vx> <vy> <vz> <epoch_lead_ms>
ntn update_sib19_utc <ta_common> <drift> <drift_variant> <x> <y> <z> <vx> <vy> <vz> <epoch_utc_ms>
```

All values are integers. The first nine are in the units of the corresponding
SIB19 fields (TS 38.331), and the last one sets `epochTime-r17`:

| Argument | SIB19 field | Unit |
|---|---|---|
| `ta_common` | `ta-Common-r17` | 4.072 × 10^-3 µs |
| `drift` | `ta-CommonDrift-r17` | 0.2 × 10^-3 µs/s, 0 = absent |
| `drift_variant` | `ta-CommonDriftVariant-r17` | 0.2 × 10^-4 µs/s², 0 = absent |
| `x`, `y`, `z` | `positionX-r17`, `positionY-r17`, `positionZ-r17` | 1.3 m, ECEF |
| `vx`, `vy`, `vz` | `velocityVX-r17`, `velocityVY-r17`, `velocityVZ-r17` | 0.06 m/s, ECEF |
| `epoch_lead_ms` | `epochTime-r17` | ms, 1 to 10239 |
| `epoch_utc_ms` | `epochTime-r17` | ms since 1970-01-01 UTC, 1 to 10239 ms ahead |

`epochTime-r17` is the instant that the other values refer to. The update
takes effect at once, in every cell with an NTN configuration; the epoch only
dates the values. The source of the update does not need to know the SFN of the
gNB: it computes the satellite state for an instant ahead, and gives that
instant in one of two ways.

- `update_sib19` takes it as a lead over the moment the gNB receives the
  command: the source computes the state for `now + epoch_lead_ms`, and the gNB
  sets `epochTime-r17` that far ahead of its current frame and subframe. No
  common clock is needed, and the command works with RFsimulator. The time the
  command takes to reach the gNB moves the epoch later by as much.
- `update_sib19_utc` takes it as a UTC time. When the command arrives, the gNB
  turns it into a lead over its own clock (`CLOCK_REALTIME`) and sets
  `epochTime-r17` in the same way. The time the command takes to reach the gNB
  then does not move the epoch, but the clock of the source has to agree with
  the clock of the gNB host: the same host, or hosts synchronized by NTP or PTP.
  An epoch that is not 1 to 10239 ms ahead of the gNB clock is refused, not
  moved. The gNB refuses this command with RFsimulator, which does not run at
  wall-clock speed.

In both cases the epoch is counted from the frame and subframe that the gNB
schedules, a few slots ahead of the radio. At 7 km/s of radial velocity, each
ms of epoch error (transfer time, clock difference) is about 47 ns of
round-trip timing error, a hundredth of the cyclic prefix at 15 kHz.

The epoch has to be far enough ahead to cover the time until the UE has decoded
the next update: the update interval, the SIB19 periodicity and the transfer of
the command. An epoch that is already in the past when the UE reads SIB19 is
taken by the UE as almost one SFN cycle (10.24 s) in the future. As a rule of
thumb, place it at least `2 x (update interval + 200 ms)` ahead, e.g. 2500 ms
for one update per second. RFsimulator does not run at wall-clock speed, so
when trying the module with RFsimulator, pace the updates on the frames of the
gNB rather than on a wall-clock timer, or the epoch can age into the past
between two updates.

For example, with the LEO values of the NTN configuration page and an epoch
2.5 s ahead, as a lead and as a UTC time (GNU `date`):
```
echo "ntn update_sib19 4634000 -230000 0 0 -2166908 4910784 0 115246 50853 2500" | nc -N 127.0.0.1 9090
echo "ntn update_sib19_utc 4634000 -230000 0 0 -2166908 4910784 0 115246 50853 $(($(date +%s%3N) + 2500))" | nc -N 127.0.0.1 9090
```
