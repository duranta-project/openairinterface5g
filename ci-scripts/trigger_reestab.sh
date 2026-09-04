#!/bin/bash
# SPDX-License-Identifier: LicenseRef-CSSL-1.0
# Outage-based reestablishment trigger (event-driven, simulation-speed agnostic).
#
# Usage: ./trigger_reestab.sh <ue-ip> <ue-telnet-port> <gnb-ip> <gnb-telnet-port> <ue-container>
#
# Creates a radio outage (ploss 100 via UE channelmod), waits for the UE to
# declare RLF (T310 expiry or CBRA failure), restores the channel, waits for the
# RRC reestablishment to complete on the UE side, then confirms the gNB counter
# ("reestab 1"). All waits are event-driven on the UE log, so timing is
# independent of the RF-sim speed; a fixed wall-clock outage cannot work (too
# short: T310 never expires; too long: the UE leaves RRC_CONNECTED via T311).

set -u

UE_IP=${1:?usage: ./trigger_reestab.sh <ue-ip> <ue-telnet-port> <gnb-ip> <gnb-telnet-port> <ue-container>}
UE_PORT=${2:?missing <ue-telnet-port>}
GNB_IP=${3:?missing <gnb-ip>}
GNB_PORT=${4:?missing <gnb-telnet-port>}
UE_CONTAINER=${5:?missing <ue-container>}

OUTAGE_CAP_S=${OUTAGE_CAP_S:-90}     # wall-clock cap waiting for UE RLF
POLL_S=${POLL_S:-0.2}                # UE-log poll period
REESTAB_WALL_CAP_S=${REESTAB_WALL_CAP_S:-180}  # cap waiting for UE-side reestab
REESTAB_POLLS=${REESTAB_POLLS:-15}   # x 2 s polls to confirm the gNB counter
# "RAR reception failed" count triggering RLF: preambleTransMax(6)+1 in all three
# DU configs. Must be exact: the RA problem indication fires at preambleTransMax+1
# and the MAC reset aborts the attempts ~50 ms later.
RAR_FAIL_THRESHOLD=${RAR_FAIL_THRESHOLD:-7}

chanmod() { echo "channelmod modify 0 $1" | ncat --send-only "$UE_IP" "$UE_PORT"; }
gnb() { echo "ci $1" | ncat "$GNB_IP" "$GNB_PORT"; }

# restore the channel on every exit path
trap 'chanmod "ploss 0" >/dev/null 2>&1' EXIT

# 1. radio outage
if ! chanmod "ploss 100"; then
  echo "ERROR: channelmod ploss 100 to $UE_IP:$UE_PORT failed" >&2
  exit 1
fi
echo "outage started on $UE_IP ($UE_CONTAINER), waiting for UE RLF..."

# 2. wait for RLF (T310 expiry or SR-exhaustion -> CBRA failure). The restore
#    must land only after one of these: earlier lets the UE recover on its own
#    (CBRA success, T310 reset) and no reestablishment happens. Counts are
#    compared to a pre-outage baseline so prior failed RAs cannot satisfy them.
baseline_fails=$(docker logs --tail 5000 "$UE_CONTAINER" 2>&1 | grep -c "RAR reception failed")
deadline=$((SECONDS + OUTAGE_CAP_S))
rlf_path=""
while [ "$SECONDS" -lt "$deadline" ]; do
  log=$(docker logs --tail 5000 "$UE_CONTAINER" 2>&1)
  if grep -q "Timer T310 expired" <<<"$log"; then
    rlf_path="T310 expiry"
    break
  fi
  fails=$(grep -c "RAR reception failed" <<<"$log")
  if [ "$fails" -ge $((baseline_fails + RAR_FAIL_THRESHOLD)) ]; then
    rlf_path="RACH failure (RAR reception failed x$((fails - baseline_fails)))"
    break
  fi
  sleep "$POLL_S"
done

if [ -z "$rlf_path" ]; then
  echo "ERROR: UE RLF not observed within ${OUTAGE_CAP_S}s (channel restored)" >&2
  exit 1
fi
echo "UE RLF observed after ~$((OUTAGE_CAP_S - (deadline - SECONDS)))s via $rlf_path, restoring channel"

# 3. restore NOW, not only via the EXIT trap: the UE cannot re-sync while ploss
#    100 is active, so a deferred restore would push the reestablishment
#    completion past this script's verification window. The trap stays as a
#    safety net for the error paths.
chanmod "ploss 0" >/dev/null 2>&1 || echo "WARNING: channel restore (ploss 0) failed" >&2

# 4. wait for the UE-side reestablishment, event-driven like the RLF wait, with
#    a pre-outage baseline so an earlier completion cannot satisfy it.
baseline_complete=$(docker logs --tail 5000 "$UE_CONTAINER" 2>&1 | grep -c "Generating RRCReestablishmentComplete")
deadline=$((SECONDS + REESTAB_WALL_CAP_S))
while [ "$SECONDS" -lt "$deadline" ]; do
  completes=$(docker logs --tail 5000 "$UE_CONTAINER" 2>&1 | grep -c "Generating RRCReestablishmentComplete")
  [ "$completes" -gt "$baseline_complete" ] && break
  sleep "$POLL_S"
done

# 5. confirm on the gNB side: the counter increments a few ms of simulated time
#    after the UE sends the message, so retry rather than rely on one poll.
for i in $(seq 1 "$REESTAB_POLLS"); do
  if gnb "get_reestab_count" | grep -qE 'UE RNTI [0-9a-f]{4} reestab 1'; then
    echo "reestablishment observed at the gNB"
    exit 0
  fi
  sleep 2
done

echo "ERROR: reestablishment not observed after outage" >&2
exit 1
