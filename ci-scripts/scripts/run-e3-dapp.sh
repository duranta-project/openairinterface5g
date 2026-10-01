#!/bin/bash
# SPDX-License-Identifier: LicenseRef-CSSL-1.0

#
# Drives the E3 agent of a gNB container (yaml_files/5g_rfsimulator_e3) with the
# spectrum dApp of the dApp library, run on the test host, and checks both sides:
#
#   setup -> subscribe (Spectrum RF=1 + L1-KPM RF=2) -> indications flowing on
#   both -> dApp reads the IQ and sends a PRB-block control -> release
#
# The gNB is deployed and stopped by the pipeline; the proof rides on the gNB
# log (docker logs), the dApp's own stats log and the dApp exit code. A clean
# gNB shutdown is checked by the pipeline (Undeploy analysis, EndsWithBye).
#
# Usage: run-e3-dapp.sh <encoding> <gnb-container>
#   encoding        asn1 | json | protobuf, the one the gNB was started with
#   gnb-container   name of the running gNB container
# Env:   DAPP_TIMED   dApp run seconds       (default: 20)
#        DAPPS_SPEC   pip requirement of the dApp library (default: dapps>=0.2.1)
#        SKIP_PIP     set to 1 to use the installed dApp library as is
#        BOOT_TIMEOUT_S  wait for the gNB to reach Frame.Slot (default: 90)
#
# The dApp is the PyPI `dapps` package; it runs its shipped example
# (`python -m examples.spectrum_dapp`), headless, for a bounded window.

set -u

ENC="${1:?usage: run-e3-dapp.sh <encoding> <gnb-container>}"
GNB="${2:?usage: run-e3-dapp.sh <encoding> <gnb-container>}"
DAPP_TIMED="${DAPP_TIMED:-20}"
DAPPS_SPEC="${DAPPS_SPEC:-dapps>=0.2.1}"
SKIP_PIP="${SKIP_PIP:-0}"
BOOT_TIMEOUT_S="${BOOT_TIMEOUT_S:-90}"

WORK="$(mktemp -d /tmp/e3-dapp-ci.XXXXXX)"
FAILURES=0

fail() { echo "[e3-ci] FAIL: $*" >&2; FAILURES=$((FAILURES + 1)); }
trap 'rm -rf "${WORK}"' EXIT

gnb_log() { docker logs "${GNB}" 2>&1; }

if [ "${SKIP_PIP}" != "1" ]; then
    echo "[e3-ci] installing the dApp library (pip install '${DAPPS_SPEC}')"
    python3 -m pip install -U "${DAPPS_SPEC}" >/dev/null 2>&1 \
        || { echo "[e3-ci] pip install '${DAPPS_SPEC}' failed" >&2; exit 2; }
fi
# The shipped example must import from the installed wheel.
python3 -c "import e3interface, spectrum.spectrum_dapp, examples.spectrum_dapp" 2>/dev/null \
    || { echo "[e3-ci] dApp library not importable (need dapps>=0.2.1 with its shipped examples)" >&2; exit 2; }

# ---- the gNB is up -----------------------------------------------------------
waited=0
until gnb_log | grep -q "Frame.Slot"; do
    sleep 2; waited=$((waited + 2))
    [ "$(docker inspect -f '{{.State.Running}}' "${GNB}" 2>/dev/null)" = "true" ] \
        || { echo "[e3-ci] ${GNB} is not running" >&2; gnb_log | tail -20 >&2; exit 1; }
    [ "${waited}" -ge "${BOOT_TIMEOUT_S}" ] \
        && { echo "[e3-ci] ${GNB} did not reach Frame.Slot in ${BOOT_TIMEOUT_S}s" >&2; exit 1; }
done
echo "[e3-ci] ${ENC}: gNB up after ${waited}s"

# ---- run the spectrum dApp (headless, bounded) ---------------------------------
# The dApp exits on its own --timed; timeout is a defensive upper bound. It writes
# to a fixed /tmp/{e3,dapp}.log; truncate them so this run is isolated.
: > /tmp/dapp.log 2>/dev/null; : > /tmp/e3.log 2>/dev/null
rc=0
timeout $((DAPP_TIMED + 30)) \
    python3 -m examples.spectrum_dapp \
    --link zmq --transport tcp --encoding-method "${ENC}" --timed "${DAPP_TIMED}" --control \
    > "${WORK}/dapp.out" 2>&1 || rc=$?
cp -f /tmp/dapp.log "${WORK}/dapp.stats" 2>/dev/null

# ---- dApp side -----------------------------------------------------------------
[ "${rc}" -eq 0 ] || { fail "${ENC}: dApp exited ${rc} (expected 0)"; tail -20 "${WORK}/dapp.out" >&2; }
grep -q "E3 Setup Response outcome: True" "${WORK}/dapp.stats" \
    || fail "${ENC}: dApp setup not accepted (no 'outcome: True')"
[ "$(grep -c "Positive subscription response" "${WORK}/dapp.stats")" -ge 2 ] \
    || fail "${ENC}: fewer than 2 positive subscription responses on the dApp side"
grep -qE "\[SHM\] final stats: handled=[1-9][0-9]* read_drops=0 " "${WORK}/dapp.stats" \
    || fail "${ENC}: dApp did not process IQ (want handled>0, read_drops=0): $(grep -o '\[SHM\] final stats:.*' "${WORK}/dapp.stats" | tail -1)"

# ---- gNB side ------------------------------------------------------------------
sleep 2
gnb_log > "${WORK}/gnb.log"
for expect in \
    "Sensing mode enabled" \
    "\[SPECTRUM-SM\] started (first subscription)" \
    "\[KPM-SM\] started (first subscription)" \
    "\[SPECTRUM-SM\] first indication batch" \
    "\[KPM-SM\] first indication batch" \
    "\[SPECTRUM-SM\] prbBlock:" \
    "\[SPECTRUM-SM\] stopped (last subscription gone)" \
    "\[KPM-SM\] stopped (last subscription gone)"
do
    grep -q "${expect}" "${WORK}/gnb.log" || fail "${ENC}: gNB log missing '${expect}'"
done
case "${ENC}" in asn1) encname="ASN.1";; json) encname="JSON";; protobuf) encname="Protocol Buffers";; *) encname="${ENC}";; esac
grep -q "Encoding RAN function data with ${encname} encoder" "${WORK}/gnb.log" \
    || fail "${ENC}: gNB did not select the ${encname} encoder"
if grep -qE "prbBlock: .*-> ok=0|sensingPolicy: .*-> ok=0" "${WORK}/gnb.log"; then
    fail "${ENC}: gNB refused a control"; grep -E "ok=0" "${WORK}/gnb.log" | head -3 >&2
fi
if grep -q "Assertion" "${WORK}/gnb.log"; then
    fail "${ENC}: assertion failure in the gNB log"; grep "Assertion" "${WORK}/gnb.log" | head -3 >&2
fi

if [ "${FAILURES}" -gt 0 ]; then
    echo "[e3-ci] ${ENC}: ${FAILURES} failure(s)" >&2
    exit 1
fi
echo "[e3-ci] ${ENC}: PASS"
exit 0
