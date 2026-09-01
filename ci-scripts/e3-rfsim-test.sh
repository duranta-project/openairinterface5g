#!/bin/bash
# SPDX-License-Identifier: LicenseRef-CSSL-1.0

#
# E3 rfsim smoke test: bring up the gNB under rfsim (no radio, no core) and
# drive the two E3 service models with spectrum dApp from the dApp-library.
#
#   setup -> subscribe (Spectrum RF=1 + L1-KPM RF=2) -> indications flowing on
#   both -> dApp reads the IQ -> release -> clean SM stop ->
#   clean gNB shutdown.
#
# The dApp runs headless (no GUI) for a bounded --timed window and exits on its
# own. The proof rides on the gNB log, the dApp's own stats log, and the
# dApp exit code.
#
# The dApp's E3 endpoints are tcp 9990/9991/9999, so the derived
# conf pins those ports for every encoding; the gNB serves whatever
# encoding we force on the same ports.
#
# Usage: ci-scripts/e3-rfsim-test.sh [encoding ...] (default: asn1 json protobuf)
# Env:   BUILD_DIR  nr-softmodem build dir   (default: cmake_targets/ran_build/build)
#        CONF       gNB config to derive from (default: the 106-PRB band78 conf)
#        DAPP_TIMED dApp run seconds           (default: 20)
#        SKIP_PIP   set to 1 to skip `pip install -U dapps` (use the env as-is)
#        BOOT_TIMEOUT_S  gNB bring-up timeout  (default: 90)
#
# The dApp is the PyPI `dapps` package; the test runs its shipped example
# (`python -m examples.spectrum_dapp`), which instantiates
# spectrum.SpectrumSharingDApp and runs its E3 lifecycle headless.

set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${REPO_ROOT}/cmake_targets/ran_build/build}"
CONF="${CONF:-${REPO_ROOT}/targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.band78.sa.fr1.106PRB.usrpx400.conf}"
DAPP_TIMED="${DAPP_TIMED:-20}"
SKIP_PIP="${SKIP_PIP:-0}"
BOOT_TIMEOUT_S="${BOOT_TIMEOUT_S:-90}"
SHUTDOWN_TIMEOUT_S="${SHUTDOWN_TIMEOUT_S:-40}"
ENCODINGS=("$@")
[ $# -eq 0 ] && ENCODINGS=(asn1 json protobuf)

WORK="$(mktemp -d /tmp/e3-rfsim-ci.XXXXXX)"
GNB_PID=""
FAILURES=0

log()  { echo "[e3-ci] $*"; }
fail() { echo "[e3-ci] FAIL: $*" >&2; FAILURES=$((FAILURES + 1)); }

cleanup() {
    if [ -n "${GNB_PID}" ] && kill -0 "${GNB_PID}" 2>/dev/null; then
        kill -INT "${GNB_PID}" 2>/dev/null; sleep 3
        kill -9 "${GNB_PID}" 2>/dev/null
    fi
    rm -rf "${WORK}"
}
trap cleanup EXIT

# ---- preconditions -----------------------------------------------------------
[ -x "${BUILD_DIR}/nr-softmodem" ] || { echo "[e3-ci] nr-softmodem not found in ${BUILD_DIR}" >&2; exit 2; }
[ -f "${CONF}" ]                   || { echo "[e3-ci] conf not found: ${CONF}" >&2; exit 2; }
if [ "${SKIP_PIP}" != "1" ]; then
    echo "[e3-ci] installing/upgrading the dApp-library (pip install -U dapps)"
    python3 -m pip install -U dapps >/dev/null 2>&1 \
        || { echo "[e3-ci] 'pip install -U dapps' failed" >&2; exit 2; }
fi
# The shipped example (examples.spectrum_dapp) must import from the installed wheel.
python3 -c "import e3interface, spectrum.spectrum_dapp, examples.spectrum_dapp" 2>/dev/null \
    || { echo "[e3-ci] dApp-library not importable after install (need dapps>=0.2.1 with shipped examples)" >&2; exit 2; }
for port in 9990 9991 9999; do
    if ss -ltn 2>/dev/null | grep -q ":${port} "; then
        echo "[e3-ci] port ${port} already in use (stale gNB?)" >&2; exit 2
    fi
done

# ---- one full pass per encoding ----------------------------------------------
run_encoding() {
    local enc="$1"
    local fails_before=${FAILURES}
    local conf="${WORK}/gnb_${enc}.conf"
    local gnb_log="${WORK}/gnb_${enc}.log"
    local dapp_log="${WORK}/dapp_${enc}.log"

    # Derive a hermetic conf: force the requested E3 encoding, pin the dApp's
    # hardcoded tcp ports (9990 setup / 9991 publisher / 9999 subscriber), and
    # point NGAP at an unreachable AMF so the run needs no core.
    sed -e "s/^\([[:space:]]*encoding[[:space:]]*=[[:space:]]*\)\"[a-z0-9]*\";/\1\"${enc}\";/" \
        -e "s/^\([[:space:]]*setup_port[[:space:]]*=[[:space:]]*\)[0-9]*;/\19990;/" \
        -e "s/^\([[:space:]]*publisher_port[[:space:]]*=[[:space:]]*\)[0-9]*;/\19991;/" \
        -e "s/^\([[:space:]]*subscriber_port[[:space:]]*=[[:space:]]*\)[0-9]*;/\19999;/" \
        -e "/amf_ip_address/ s/\(ipv4[[:space:]]*=[[:space:]]*\)\"[0-9.]*\"/\1\"127.0.0.99\"/" \
        "${CONF}" > "${conf}"
    grep -q "= \"${enc}\";" "${conf}" || { fail "${enc}: could not set encoding in the derived conf"; return; }

    log "=== ${enc}: launching gNB (rfsim, core-less, ports 9990/9991/9999) ==="
    ( cd "${BUILD_DIR}" && exec ./nr-softmodem -O "${conf}" --rfsim \
        --rfsimulator.[0].serveraddr server --gNBs.[0].min_rxtxtime 6 ) \
        > "${gnb_log}" 2>&1 &
    GNB_PID=$!

    local waited=0
    until grep -q "Frame.Slot" "${gnb_log}" 2>/dev/null; do
        sleep 2; waited=$((waited + 2))
        if ! kill -0 "${GNB_PID}" 2>/dev/null; then
            fail "${enc}: gNB exited during bring-up"; tail -20 "${gnb_log}" >&2; GNB_PID=""; return
        fi
        if [ "${waited}" -ge "${BOOT_TIMEOUT_S}" ]; then
            fail "${enc}: gNB did not reach Frame.Slot in ${BOOT_TIMEOUT_S}s"; kill -9 "${GNB_PID}"; GNB_PID=""; return
        fi
    done
    log "${enc}: gNB up after ${waited}s"

    # ---- run the spectrum dApp (headless, bounded) -----------------------------
    # The dApp exits on its own --timed; timeout is a defensive upper bound. It
    # writes to a fixed /tmp/{e3,dapp}.log; truncate them so this leg is isolated,
    # then snapshot them into the work dir for the stats assertions below.
    local applog="${WORK}/dapp_${enc}.stats"
    : > /tmp/dapp.log 2>/dev/null; : > /tmp/e3.log 2>/dev/null
    local rc=0
    timeout $((DAPP_TIMED + 30)) \
        python3 -m examples.spectrum_dapp \
        --link zmq --transport tcp --encoding-method "${enc}" --timed "${DAPP_TIMED}" --control \
        > "${dapp_log}" 2>&1 || rc=$?
    cp -f /tmp/dapp.log "${applog}"            2>/dev/null
    cp -f /tmp/e3.log   "${WORK}/e3_${enc}.log" 2>/dev/null

    # ---- assertions (every encoding is asserted; a failure fails the suite) ----
    # dApp must exit cleanly.
    [ "${rc}" -eq 0 ] || { fail "${enc}: dApp exited ${rc} (expected 0)"; tail -20 "${dapp_log}" >&2; }

    # ---- dApp-side assertions --------------------------------------------------
    # Setup accepted, both subscriptions positive, and the L1-KPM (RF=2) IQ
    # handled with zero read-drops.
    grep -q "E3 Setup Response outcome: True" "${applog}" \
        || fail "${enc}: dApp setup not accepted (no 'outcome: True')"
    [ "$(grep -c "Positive subscription response" "${applog}")" -ge 2 ] \
        || fail "${enc}: fewer than 2 positive subscription responses on the dApp side"
    grep -qE "\[SHM\] final stats: handled=[1-9][0-9]* read_drops=0 " "${applog}" \
        || fail "${enc}: dApp did not process IQ (want handled>0, read_drops=0): $(grep -o '\[SHM\] final stats:.*' "${applog}" | tail -1)"

    # ---- gNB-side assertions ----------------------------------------------------
    # Both SMs subscribed and stopped on release; KPM (RF=2) emitted IQ.
    sleep 2
    local expect
    for expect in \
        "\[SPECTRUM-SM\] started (first subscription)" \
        "\[KPM-SM\] started (first subscription)" \
        "\[SPECTRUM-SM\] first indication batch" \
        "\[KPM-SM\] first indication batch" \
        "\[SPECTRUM\] prbBlock:" \
        "\[SPECTRUM-SM\] stopped (last subscription gone)" \
        "\[KPM-SM\] stopped (last subscription gone)"
    do
        grep -q "${expect}" "${gnb_log}" || fail "${enc}: gNB log missing '${expect}'"
    done
    # The gNB selected the encoder we forced in the derived conf.
    local encname
    case "${enc}" in asn1) encname="ASN.1";; json) encname="JSON";; protobuf) encname="Protocol Buffers";; *) encname="${enc}";; esac
    grep -q "Encoding RAN function data with ${encname} encoder" "${gnb_log}" \
        || fail "${enc}: gNB did not select the ${encname} encoder"
    if grep -qE "prbBlock: .*-> ok=0|sensingPolicy: .*-> ok=0" "${gnb_log}"; then
        fail "${enc}: gNB refused a control"; grep -E "ok=0" "${gnb_log}" | head -3 >&2
    fi
    if grep -q "Assertion" "${gnb_log}"; then
        fail "${enc}: assertion failure in the gNB log"; grep "Assertion" "${gnb_log}" | head -3 >&2
    fi

    # ---- clean shutdown ----------------------------------------------------------
    kill -INT "${GNB_PID}" 2>/dev/null
    local down=0
    while kill -0 "${GNB_PID}" 2>/dev/null; do
        sleep 1; down=$((down + 1))
        [ "${down}" -ge "${SHUTDOWN_TIMEOUT_S}" ] && { fail "${enc}: gNB ignored SIGINT"; kill -9 "${GNB_PID}"; break; }
    done
    GNB_PID=""
    grep -q "Bye." "${gnb_log}" || fail "${enc}: no clean shutdown marker in the gNB log"
    for shm in /dev/shm/e3_ran_buffers /dev/shm/e3_l2_sensing; do
        [ -e "${shm}" ] && fail "${enc}: ${shm} left behind after shutdown"
    done
    if [ "${FAILURES}" -eq "${fails_before}" ]; then
        log "=== ${enc}: PASS ==="
    else
        log "=== ${enc}: FAIL ($((FAILURES - fails_before)) assertion(s)) ==="
    fi
}

for enc in "${ENCODINGS[@]}"; do
    run_encoding "${enc}"
done

if [ "${FAILURES}" -gt 0 ]; then
    echo "[e3-ci] ${FAILURES} failure(s)" >&2
    exit 1
fi
log "ALL PASS (${ENCODINGS[*]})"
exit 0
