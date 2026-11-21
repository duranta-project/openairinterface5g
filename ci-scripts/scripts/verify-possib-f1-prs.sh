#!/bin/bash
# SPDX-License-Identifier: MIT

for ((attempt = 0; attempt < 60; attempt++)); do
  cu_log=$(docker logs rfsim5g-oai-cu 2>&1)
  du_log=$(docker logs rfsim5g-oai-du-pci0 2>&1)
  ue_log=$(docker logs rfsim5g-oai-nr-ue 2>&1)
  if grep -q 'Request PRS for PosSIB' <<< "$cu_log" \
      && grep -q 'Sending positioning assistance control' <<< "$cu_log" \
      && grep -q 'activated.*posSibType6-1' <<< "$du_log" \
      && grep -q 'PosSIB configured 1 PRS target(s)' <<< "$ue_log" \
      && grep -q 'Applied 1 PRS target(s) from source 1' <<< "$ue_log"; then
    grep -m 1 'Request PRS for PosSIB' <<< "$cu_log"
    grep -m 1 'Sending positioning assistance control' <<< "$cu_log"
    grep -m 1 'activated.*posSibType6-1' <<< "$du_log"
    grep -m 1 'PosSIB configured 1 PRS target(s)' <<< "$ue_log"
    grep -m 1 'Applied 1 PRS target(s) from source 1' <<< "$ue_log"
    exit 0
  fi
  sleep 1
done
echo 'F1 PRS exchange or PosSIB activation and reception was not observed' >&2
exit 1
