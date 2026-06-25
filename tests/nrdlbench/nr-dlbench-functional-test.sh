#!/bin/bash
# SPDX-License-Identifier: LicenseRef-CSSL-1.0

# usage: nr-dlbench-functional-test.sh <nr_dlbench-binary> [extra nr_dlbench args]

if [ $# -lt 1 ]; then
  echo "usage: $0 <nr_dlbench-binary> [extra nr_dlbench args]"
  exit 1
fi

BIN=$1
shift

# Small, fast config: 1 UE, 24 RBs (10 MHz, the minimum that fits a CORESET),
# 200 slots. Caller-supplied args (e.g. -A <depth> for AM mode) are appended.
set -x
OUT=$("${BIN}" -u 1 -R 24 -n 200 -L 0 "$@" 2>&1)
RET=$?
set +x

echo "${OUT}"

if [ ${RET} -ne 0 ]; then
  echo "FAIL: nr_dlbench exited with code ${RET}"
  exit 1
fi
if ! echo "${OUT}" | grep -q "=== Results"; then
  echo "FAIL: no results summary in nr_dlbench output"
  exit 1
fi
echo "PASS"
exit 0
