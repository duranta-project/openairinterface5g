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
```

All values are integers. The first nine are in the units of the corresponding
SIB19 fields (TS 38.331) and have to lie in their ranges:

| Argument | SIB19 field | Unit | Range |
|---|---|---|---|
| `ta_common` | `ta-Common-r17` | 4.072 × 10^-3 µs | 0 to 66485757 |
| `drift` | `ta-CommonDrift-r17` | 0.2 × 10^-3 µs/s, 0 = absent | -257303 to 257303 |
| `drift_variant` | `ta-CommonDriftVariant-r17` | 0.2 × 10^-4 µs/s², 0 = absent | 0 to 28949 |
| `x`, `y`, `z` | `positionX-r17`, `positionY-r17`, `positionZ-r17` | 1.3 m, ECEF | -33554432 to 33554431 |
| `vx`, `vy`, `vz` | `velocityVX-r17`, `velocityVY-r17`, `velocityVZ-r17` | 0.06 m/s, ECEF | -131072 to 131071 |
| `epoch_lead_ms` | `epochTime-r17` | ms | 1 to 10230 |

The other values describe the satellite at the instant `epoch_lead_ms` after the
moment the gNB receives the command. The gNB sets `epochTime-r17` that far ahead
of its current frame and subframe, so the source of the update does not need to
know the SFN of the gNB: it computes the satellite state for the instant
`now + epoch_lead_ms` and sends it with that lead. The gNB broadcasts the update
from the next SIB19 window on, in every cell with an NTN configuration.

A command with a value out of range, a missing value or anything after the lead
is refused and changes nothing. The gNB also refuses updates until its scheduler
runs.

The lead has to cover the time until the UE has decoded the next update: the
update interval, the SIB19 periodicity and the transfer of the command. An epoch
that is already in the past when the UE reads SIB19 is taken by the UE as almost
one SFN cycle (10.24 s) in the future. As a rule of thumb, use
`epoch_lead_ms >= 2 x (update interval + 200 ms)`, e.g. 2500 ms for one update
per second. The lead is at most 10230 ms, one SFN cycle less one frame: the UE
tells the cycle of the epoch from its SFN alone, so the epoch must not come back
to the current frame. If the epoch passes before a newer update arrives, e.g.
because the source has stopped, the gNB stops broadcasting SIB19 until the next
update. RFsimulator does not run at wall-clock speed, so when trying the
module with RFsimulator, pace the updates on the frames of the gNB rather than
on a wall-clock timer, or the epoch can age into the past between two updates.

For example, with the LEO values of the NTN configuration page and a lead of
2.5 s:
```
echo "ntn update_sib19 4634000 -230000 0 0 -2166908 4910784 0 115246 50853 2500" | nc -N 127.0.0.1 9090
```
