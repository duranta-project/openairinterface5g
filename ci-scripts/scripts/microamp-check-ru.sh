#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Wait until the Microamp FR2 RU is reachable and PTP-synchronized, and print
# its configuration.
#
# usage: microamp-check-ru.sh [bandwidth-in-MHz] [RU-management-IP]
#
# Note: the CI framework aborts custom scripts after 90s, so the overall
# waiting time is kept well below that.

set -e

BANDWIDTH=${1:-100}
RU_IP=${2:-10.10.0.117}
RU_USER=remctl
RU_PASS=microampcfg
# maximum PTP offset (rms, in ns) we consider synchronized
PTP_RMS_THRESHOLD=100
REACHABLE_RETRIES=25
PTP_RETRIES=10

ssh_ru() {
  sshpass -p "$RU_PASS" ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
    -o ConnectTimeout=10 "$RU_USER@$RU_IP" "$@" < /dev/null
}

echo "→ Waiting for Microamp RU at $RU_IP to be reachable..."
i=0
while [ $i -lt $REACHABLE_RETRIES ]; do
  if CFG=$(ssh_ru get-cfg 2>/dev/null); then
    echo "✓ RU is reachable"
    break
  fi
  i=$((i+1))
  sleep 2
done
if [ $i -eq $REACHABLE_RETRIES ]; then
  echo "✗ RU not reachable after $((REACHABLE_RETRIES*2))s"
  exit 1
fi

echo "$CFG"
if ! echo "$CFG" | grep -qiE "^[[:space:]]*Bandwidth:[[:space:]]*${BANDWIDTH}M"; then
  echo "✗ RU is not configured for ${BANDWIDTH} MHz"
  exit 1
fi
echo "✓ RU configured for ${BANDWIDTH} MHz"

echo "→ Checking that the RU is PTP-synchronized..."
i=0
while [ $i -lt $PTP_RETRIES ]; do
  # keep the last reported rms value of ptp4l, "none" if ptp4l reported nothing
  RMS=$(ssh_ru logs-ptp 2>/dev/null | awk '/rms/ { rms=$8; found=1 } END { print found ? rms+0 : "none" }')
  if [ "$RMS" != "none" ] && [ "$RMS" -le $PTP_RMS_THRESHOLD ]; then
    echo "✓ RU is PTP-synchronized (ptp4l rms ${RMS}ns)"
    exit 0
  fi
  echo "  ptp4l rms = ${RMS}ns, waiting..."
  i=$((i+1))
  sleep 3
done

echo "✗ RU not PTP-synchronized (last ptp4l rms ${RMS}ns > ${PTP_RMS_THRESHOLD}ns)"
exit 1
