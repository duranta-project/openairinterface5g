#!/bin/bash
# SPDX-License-Identifier: MIT

set -e
initial=$(docker logs rfsim5g-oai-du-pci0 2>&1 | grep -c 'activated.*posSibType6-1' || true)
docker restart rfsim5g-oai-cu >/dev/null
for ((attempt = 0; attempt < 90; attempt++)); do
  current=$(docker logs rfsim5g-oai-du-pci0 2>&1 | grep -c 'activated.*posSibType6-1' || true)
  if (( current > initial )); then
    docker logs rfsim5g-oai-du-pci0 2>&1 | grep 'activated.*posSibType6-1' | tail -1
    exit 0
  fi
  sleep 1
done
echo 'PosSIB was not activated again after the CU restarted' >&2
exit 1
