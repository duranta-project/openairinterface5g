#!/bin/bash
# SPDX-License-Identifier: MIT

for ((attempt = 0; attempt < 30; attempt++)); do
  gnb_log=$(docker logs rfsim5g-oai-gnb 2>&1)
  ue_log=$(docker logs rfsim5g-oai-nr-ue 2>&1)
  if grep -q 'activated.*posSibType6-1' <<< "$gnb_log" \
      && grep -q 'PosSIB configured 1 PRS target(s)' <<< "$ue_log" \
      && grep -q 'Applied 1 PRS target(s) from source 1' <<< "$ue_log"; then
    grep -m 1 'activated.*posSibType6-1' <<< "$gnb_log"
    grep -m 1 'PosSIB configured 1 PRS target(s)' <<< "$ue_log"
    grep -m 1 'Applied 1 PRS target(s) from source 1' <<< "$ue_log"
    exit 0
  fi
  sleep 1
done

echo 'PosSIB activation, reception, or PRS application was not observed' >&2
exit 1
