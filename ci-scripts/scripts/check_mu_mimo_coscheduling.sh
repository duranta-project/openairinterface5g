#!/bin/bash
# SPDX-License-Identifier: MIT

set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 <min-co-scheduling-events>" >&2
    exit 1
fi

MIN_EVENTS="$1"
GNB_CONTAINER="rfsim5g-oai-gnb"

# Each UE's periodic ulsch_rounds stats line (main.c) carries a cumulative
# "MU-MIMO co-scheduled N" count, incremented in mu_coschedule_partner()
# (gNB_scheduler_ulsch_MU_MIMO_policies.c) whenever that UE was actually
# paired onto shared resources with another UE. Its staying at 0 across every
# UE means candidates were never found orthogonal enough to pair (e.g. a
# broken SRS channel estimate), even if UL data throughput looks fine.
#
# Capture the logs once into a variable rather than piping "docker logs"
# straight into grep/head: any downstream command that stops reading before
# "docker logs" finishes writing (head -N, grep -mN, both included) sends it
# SIGPIPE, which under `set -o pipefail` fails this whole script via `set -e`
# before ever reaching the real pass/fail check below -- even though the
# count itself was already computed correctly.
LOGS=$(docker logs "${GNB_CONTAINER}" 2>&1)

# The field is cumulative per UE across the whole run, so take the highest
# value seen for any single UE as the representative count (not a sum, which
# would double-count each event across its anchor and partner).
COUNT=$(grep -o "MU-MIMO co-scheduled [0-9]*" <<< "${LOGS}" | awk '{print $3}' | sort -n | tail -1)
COUNT="${COUNT:-0}"

echo "MU-MIMO co-scheduling events observed (max per-UE cumulative count): ${COUNT} (minimum required: ${MIN_EVENTS})"
if [[ "${COUNT}" -gt 0 ]]; then
    echo "Sample periodic stats lines showing non-zero co-scheduling:"
    # This is purely informational: swallow any pipefail-induced exit here
    # (e.g. from head closing early) with "|| true" rather than fighting it --
    # the real pass/fail decision is the explicit check below, not this line.
    { grep "MU-MIMO co-scheduled [1-9]" <<< "${LOGS}" | tail -5 ; } || true
fi

[[ "${COUNT}" -ge "${MIN_EVENTS}" ]]
