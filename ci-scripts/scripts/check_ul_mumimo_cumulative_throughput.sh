#!/bin/bash
# SPDX-License-Identifier: MIT

set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "Usage: $0 <cumulative-threshold-mbps> <vrtsim-timescale>" >&2
    exit 1
fi

# The threshold applies to the over-the-air throughput: vrtsim runs the radio
# at <vrtsim-timescale> times real time, so the over-the-air rate is the
# wall-clock rate iperf3 measures divided by the timescale.
THRESHOLD_MBPS="$1"
TIMESCALE="$2"

LOG_DIR="../cmake_targets/log/container_5g_vrtsim_mumimo_gh.xml.d"

# client logs of the concurrent UL Iperf testcase, one per UE (iperf3 run with
# --get-server-output, see Iperf_Module() in cls_oaicitest.py)
shopt -s nullglob
ue1_matches=( "${LOG_DIR}"/*-iperf_client_rfsim5g_ue.log )
ue2_matches=( "${LOG_DIR}"/*-iperf_client_rfsim5g_ue2.log )
if [[ ${#ue1_matches[@]} -eq 0 || ${#ue2_matches[@]} -eq 0 ]]; then
    echo "Could not find the iperf client logs of both UEs in ${LOG_DIR} (cwd: $(pwd))" >&2
    exit 1
fi
UE_LOGS=("${ue1_matches[-1]}" "${ue2_matches[-1]}")

# Received bitrate in Mbps: the last "receiver" line, i.e. the server's own report,
# as parsed by Iperf_analyzeV3UDP() in cls_oaicitest.py
received_mbps() {
    awk '
        /receiver/ {
            for (i = 1; i <= NF; i++)
                if ($i ~ /^[0-9.]+$/ && $(i+1) ~ /bits\/sec$/) {
                    val = $i
                    if ($(i+1) ~ /^K/)       val /= 1000
                    else if ($(i+1) ~ /^G/)  val *= 1000
                    else if ($(i+1) !~ /^M/) val /= 1000000
                }
        }
        END { if (val != "") printf "%.4f", val }
    ' "$1"
}

total=0
for i in 0 1; do
    log="${UE_LOGS[i]}"
    mbps=$(received_mbps "${log}")
    if [[ -z "${mbps}" ]]; then
        echo "Could not parse a receiver bitrate from ${log}" >&2
        exit 1
    fi
    ip=$(awk '/ local .* connected to / { print $4; exit }' "${log}")
    total=$(awk -v a="${total}" -v b="${mbps}" 'BEGIN { printf "%.4f", a + b }')
    awk -v ue="UE$((i + 1)) (${ip})" -v v="${mbps}" -v ts="${TIMESCALE}" \
        'BEGIN { printf "%s\n  over the air: %.2f Mbps\n  wall-clock  : %.2f Mbps\n\n", ue, v / ts, v }'
done

TOTAL_OTA_MBPS=$(awk -v v="${total}" -v ts="${TIMESCALE}" 'BEGIN { printf "%.2f", v / ts }')
printf 'Cumulative UL throughput\n  over the air: %s Mbps (threshold %s Mbps)\n  wall-clock  : %.2f Mbps\n  timescale   : %s\n' \
    "${TOTAL_OTA_MBPS}" "${THRESHOLD_MBPS}" "${total}" "${TIMESCALE}"

awk -v t="${TOTAL_OTA_MBPS}" -v th="${THRESHOLD_MBPS}" 'BEGIN { exit !(t >= th) }'
