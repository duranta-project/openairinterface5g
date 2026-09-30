#!/bin/bash
# SPDX-License-Identifier: MIT

set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 <cumulative-threshold-mbps>" >&2
    exit 1
fi

THRESHOLD_MBPS="$1"

LOG_DIR="../cmake_targets/log/xml_files/container_5g_vrtsim_mumimo_gh.xml.d"

if [[ ! -d "${LOG_DIR}" ]]; then
    echo "Could not find the archived log directory at ${LOG_DIR} (cwd: $(pwd))" >&2
    exit 1
fi

shopt -s nullglob
ue1_matches=( "${LOG_DIR}"/*-iperf_client_rfsim5g_ue.log )
ue2_matches=( "${LOG_DIR}"/*-iperf_client_rfsim5g_ue2.log )

if [[ ${#ue1_matches[@]} -eq 0 || ${#ue2_matches[@]} -eq 0 ]]; then
    echo "Could not find archived iperf client logs in ${LOG_DIR}" >&2
    exit 1
fi

UE1_LOG="${ue1_matches[-1]}"
UE2_LOG="${ue2_matches[-1]}"

# Extract the "receiver"-side bitrate (the actual over-the-air delivered rate,
# reported via iperf3's --get-server-output) and normalize it to Mbps. Mirrors
# the sender/receiver line format parsed by Iperf_analyzeV3UDP() in cls_oaicitest.py.
extract_receiver_mbps() {
    awk '
        /receiver/ {
            for (i = 1; i <= NF; i++) {
                if ($i ~ /^[0-9.]+$/ && $(i+1) ~ /bits\/sec$/) {
                    val = $i
                    unit = $(i+1)
                    break
                }
            }
            if (unit ~ /^K/)      printf "%.4f", val / 1000
            else if (unit ~ /^M/) printf "%.4f", val
            else if (unit ~ /^G/) printf "%.4f", val * 1000
            else                  printf "%.4f", val / 1000000
            exit
        }
    ' "$1"
}

UE1_MBPS=$(extract_receiver_mbps "${UE1_LOG}")
UE2_MBPS=$(extract_receiver_mbps "${UE2_LOG}")

if [[ -z "${UE1_MBPS}" || -z "${UE2_MBPS}" ]]; then
    echo "Could not parse a receiver bitrate from one or both UE iperf logs" >&2
    exit 1
fi

TOTAL_MBPS=$(awk -v a="${UE1_MBPS}" -v b="${UE2_MBPS}" 'BEGIN { printf "%.4f", a + b }')

echo "UE1 UL received : ${UE1_MBPS} Mbps"
echo "UE2 UL received : ${UE2_MBPS} Mbps"
echo "Cumulative UL throughput: ${TOTAL_MBPS} Mbps (threshold: ${THRESHOLD_MBPS} Mbps)"

awk -v t="${TOTAL_MBPS}" -v th="${THRESHOLD_MBPS}" 'BEGIN { exit !(t >= th) }'
