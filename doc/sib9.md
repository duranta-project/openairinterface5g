<!-- SPDX-License-Identifier: CC-BY-4.0 -->
# SIB9 UTC configuration

SIB9 is optional. Static UTC requires an explicit `timeInfoUTC` in 10 ms units
since 1900-01-01, from 0 through 549755813887. Zero is a timestamp, not a mode.
Optional `dayLightSavingTime` (0..2), `leapSeconds` (-127..128) and
`localTimeOffset` (-63..64, 15-minute units) are absent unless configured.

A CU `sib9_config = { timeSource = "static"; timeInfoUTC = 390898880000L; };`
selects a fixed value. `timeSource = "realtime"` forbids `timeInfoUTC` and samples
CLOCK_REALTIME to construct the initial standard SIB9 container. DU refresh mode
is configured independently; no private mode is carried in the F1 timestamp.

The CU initial sample alone is not a continuously updated broadcast time.
