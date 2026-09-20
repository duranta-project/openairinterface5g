#!/bin/bash
# SPDX-License-Identifier: LicenseRef-CSSL-1.0

#
# E2SM-DAPP rfsim test: nearRT-RIC + xApp + gNB under rfsim, no radio, no core.
#
#   nearRT-RIC up -> gNB E2 SETUP advertising E2SM-DAPP (RAN function 255)
#   -> xApp finds that RAN function and installs both report styles
#   -> xApp sends spectrum controls, the bridge accepts and acknowledges them
#   -> both subscriptions deleted -> clean gNB shutdown.
#
# What this does NOT cover, deliberately: indications, and with them the dApp end
# of the bridge. Both report styles are event-driven -- Format 1 carries a dApp
# report, Format 2 fires when a dApp attaches or detaches -- and a dApp can only
# attach once an E3 service model is registered. None is in tree yet, so the E3
# agent comes up advertising an empty RAN function list, no dApp attaches, and
# neither indication is ever emitted. What is covered is the whole
# subscribe/control/delete path on RAN function 255. Once the E3 service models
# land, fold these assertions into ci-scripts/e3-rfsim-test.sh, which already
# brings up a dApp.
#
# Usage: ci-scripts/e2sm-dapp-rfsim-test.sh
# Env:   BUILD_DIR       nr-softmodem build dir (default: cmake_targets/ran_build/build)
#        FLEXRIC_BUILD   flexric build dir      (default: openair2/E2AP/flexric/build)
#        CONF            gNB config to derive from (default: the pci0 rfsim conf)
#        XAPP_RUN_S      seconds to let the xApp run (default: 25)
#        BOOT_TIMEOUT_S  gNB bring-up timeout   (default: 90)

set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${REPO_ROOT}/cmake_targets/ran_build/build}"
FLEXRIC_BUILD="${FLEXRIC_BUILD:-${REPO_ROOT}/openair2/E2AP/flexric/build}"
CONF="${CONF:-${REPO_ROOT}/targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.sa.band78.fr1.106PRB.pci0.rfsim.conf}"
XAPP_RUN_S="${XAPP_RUN_S:-25}"
BOOT_TIMEOUT_S="${BOOT_TIMEOUT_S:-90}"
SHUTDOWN_TIMEOUT_S="${SHUTDOWN_TIMEOUT_S:-40}"

WORK="$(mktemp -d /tmp/e2sm-dapp-ci.XXXXXX)"
RIC_PID=""
GNB_PID=""
XAPP_PID=""
FAILURES=0

log()  { echo "[dapp-ci] $*"; }
fail() { echo "[dapp-ci] FAIL: $*" >&2; FAILURES=$((FAILURES + 1)); }

kill_wait() {
    local pid="$1" sig="$2"
    [ -n "${pid}" ] || return 0
    kill -0 "${pid}" 2>/dev/null || return 0
    kill "-${sig}" "${pid}" 2>/dev/null
    local n=0
    while kill -0 "${pid}" 2>/dev/null && [ "${n}" -lt "${SHUTDOWN_TIMEOUT_S}" ]; do
        sleep 1; n=$((n + 1))
    done
    kill -9 "${pid}" 2>/dev/null
}

CONF_CLEANUP=""
cleanup() {
    kill_wait "${XAPP_PID}" TERM
    kill_wait "${GNB_PID}" INT
    kill_wait "${RIC_PID}" TERM
    [ -n "${CONF_CLEANUP}" ] && rm -f "${CONF_CLEANUP}"
    rm -rf "${WORK}"
}
trap cleanup EXIT

# ---- preconditions -----------------------------------------------------------
RIC_BIN="${FLEXRIC_BUILD}/examples/ric/nearRT-RIC"
XAPP_BIN="${FLEXRIC_BUILD}/examples/xApp/c/spectrum/xapp_spectrum"

[ -x "${BUILD_DIR}/nr-softmodem" ] || { echo "[dapp-ci] nr-softmodem not found in ${BUILD_DIR}" >&2; exit 2; }
[ -x "${RIC_BIN}" ]  || { echo "[dapp-ci] nearRT-RIC not found: ${RIC_BIN}" >&2; exit 2; }
[ -x "${XAPP_BIN}" ] || { echo "[dapp-ci] xapp_spectrum not found: ${XAPP_BIN}" >&2; exit 2; }
[ -f "${CONF}" ]     || { echo "[dapp-ci] conf not found: ${CONF}" >&2; exit 2; }

# A private service-model directory holding only this build's plugins. The
# loaders scan the directory, so a plugin left over from another install would
# be loaded too.
SM_DIR="${WORK}/sm"
mkdir -p "${SM_DIR}"
found_dapp=0
while IFS= read -r so; do
    cp -f "${so}" "${SM_DIR}/"
    case "${so}" in *libdapp_sm.so) found_dapp=1;; esac
done < <(find "${FLEXRIC_BUILD}/src/sm" -name 'lib*_sm.so' -print)
[ "${found_dapp}" -eq 1 ] || { echo "[dapp-ci] libdapp_sm.so not built in ${FLEXRIC_BUILD}" >&2; exit 2; }
log "service models staged in ${SM_DIR}: $(ls "${SM_DIR}" | tr '\n' ' ')"

# ---- derive a hermetic conf --------------------------------------------------
# Point the E2 agent at the local RIC and at the staged plugin directory, and
# park NGAP on an unreachable AMF so the run needs no core.
# The conf uses @include with a path relative to its own directory, so the
# derived copy has to sit next to the original.
GNB_CONF="$(dirname "${CONF}")/.e2sm-dapp-ci-$$.conf"
CONF_CLEANUP="${GNB_CONF}"
sed -e "s|^\([[:space:]]*sm_dir[[:space:]]*=[[:space:]]*\)\"[^\"]*\"|\1\"${SM_DIR}/\"|" \
    -e "s|^\([[:space:]]*near_ric_ip_addr[[:space:]]*=[[:space:]]*\)\"[^\"]*\"|\1\"127.0.0.1\"|" \
    -e "/amf_ip_address/ s|\(ipv4[[:space:]]*=[[:space:]]*\)\"[0-9.]*\"|\1\"127.0.0.99\"|" \
    "${CONF}" > "${GNB_CONF}"
grep -q "sm_dir *= *\"${SM_DIR}/\"" "${GNB_CONF}" \
    || { echo "[dapp-ci] could not set sm_dir in the derived conf" >&2; exit 2; }

# ---- nearRT-RIC --------------------------------------------------------------
RIC_LOG="${WORK}/ric.log"
log "launching nearRT-RIC"
( cd "${FLEXRIC_BUILD}" && exec "${RIC_BIN}" -p "${SM_DIR}/" ) > "${RIC_LOG}" 2>&1 &
RIC_PID=$!
sleep 3
kill -0 "${RIC_PID}" 2>/dev/null || { echo "[dapp-ci] nearRT-RIC died at startup" >&2; tail -20 "${RIC_LOG}" >&2; exit 2; }

# ---- gNB ---------------------------------------------------------------------
# E3 is configured on the command line: the parameters are
# PARAMFLAG_CMDLINE_NOPREFIXENABLED, so no conf edit is needed for them.
GNB_LOG="${WORK}/gnb.log"
log "launching gNB (rfsim, core-less, E2 + E3)"
( cd "${BUILD_DIR}" && exec ./nr-softmodem -O "${GNB_CONF}" --rfsim \
    --rfsimulator.[0].serveraddr server --gNBs.[0].min_rxtxtime 6 \
    --E3Configuration.link zmq --E3Configuration.transport ipc ) \
    > "${GNB_LOG}" 2>&1 &
GNB_PID=$!

waited=0
until grep -q "Frame.Slot" "${GNB_LOG}" 2>/dev/null; do
    sleep 2; waited=$((waited + 2))
    if ! kill -0 "${GNB_PID}" 2>/dev/null; then
        fail "gNB exited during bring-up"; tail -30 "${GNB_LOG}" >&2; GNB_PID=""; exit 1
    fi
    if [ "${waited}" -ge "${BOOT_TIMEOUT_S}" ]; then
        fail "gNB did not reach Frame.Slot in ${BOOT_TIMEOUT_S}s"; tail -30 "${GNB_LOG}" >&2; exit 1
    fi
done
log "gNB up after ${waited}s"

# E2 SETUP must have completed before the xApp can see the node.
waited=0
until grep -q "E2 SETUP RESPONSE rx" "${GNB_LOG}" 2>/dev/null; do
    sleep 1; waited=$((waited + 1))
    if [ "${waited}" -ge 30 ]; then
        fail "no E2 SETUP RESPONSE within 30s of the gNB coming up"
        grep -iE "e2|setup" "${GNB_LOG}" | tail -10 >&2
        exit 1
    fi
done
log "E2 SETUP completed"

# ---- xApp --------------------------------------------------------------------
# The xApp runs until signalled; it loops sending a control every 5 s.
XAPP_LOG="${WORK}/xapp.log"
log "launching xapp_spectrum for ${XAPP_RUN_S}s"
( cd "${FLEXRIC_BUILD}" && exec "${XAPP_BIN}" -p "${SM_DIR}/" ) > "${XAPP_LOG}" 2>&1 &
XAPP_PID=$!
sleep "${XAPP_RUN_S}"
if ! kill -0 "${XAPP_PID}" 2>/dev/null; then
    fail "xApp exited early"; tail -30 "${XAPP_LOG}" >&2
else
    kill_wait "${XAPP_PID}" TERM
fi
XAPP_PID=""

# ---- assertions --------------------------------------------------------------
# xApp side. Finding the RAN function at all is the proof that the gNB
# advertised E2SM-DAPP: the xApp asserts the definition decodes as DAPP, so a
# missing or misencoded RAN function definition aborts it.
for expect in \
    "[DAPP RC]: Connected E2 nodes = 1" \
    "[DAPP xApp] RAN function @idx=" \
    "[DAPP RC]: Installing DAPP (E3 data) (style 1) subscription on E2 node 0" \
    "[DAPP RC]: Installing DAPP (subscription map) (style 2) subscription on E2 node 0" \
    "[DAPP RC]: Sending control variant" \
    "[xApp]: Successfully received CONTROL response" \
    "[DAPP RC]: Test xApp run SUCCESSFULLY"
do
    grep -qF "${expect}" "${XAPP_LOG}" || fail "xApp log missing '${expect}'"
done
# No indication is asserted. Both report styles are event-driven: Format 1 needs
# a dApp report, and Format 2 is emitted when a dApp attaches or detaches, not
# when an xApp subscribes. With no dApp there is no event, so neither fires.
# That is the part of the bridge this test cannot reach; see the header.

# ---- shut the gNB down before reading its log ---------------------------------
# The gNB's stdout is block-buffered through the redirect and only flushes when
# the process exits, so asserting on the log while it is still running is a race.
kill_wait "${GNB_PID}" INT
grep -q "Bye." "${GNB_LOG}" || fail "no clean shutdown marker in the gNB log"
GNB_PID=""

# ---- gNB-side assertions -----------------------------------------------------
# Two subscriptions installed and both deleted, on RAN function 255.
n_subs="$(grep -cF "RIC_SUBSCRIPTION_REQUEST rx RAN_FUNC_ID 255" "${GNB_LOG}")"
[ "${n_subs}" -ge 2 ] \
    || fail "gNB saw ${n_subs} subscription requests on RAN function 255, want 2"
n_del="$(grep -cF "RIC_SUBSCRIPTION_DELETE_REQUEST rx RAN_FUNC_ID 255" "${GNB_LOG}")"
[ "${n_del}" -ge 2 ] \
    || fail "gNB saw ${n_del} subscription deletes on RAN function 255, want 2"
grep -q "CONTROL ACKNOWLEDGE tx" "${GNB_LOG}" \
    || fail "gNB did not acknowledge an xApp control"

# With no dApp attached the bridge has nowhere to forward a control and says so.
# That is expected here; what must not happen is an assertion.
if grep -q "Assertion" "${GNB_LOG}"; then
    fail "assertion failure in the gNB log"; grep "Assertion" "${GNB_LOG}" | head -3 >&2
fi
if grep -q "Assertion" "${RIC_LOG}"; then
    fail "assertion failure in the nearRT-RIC log"; grep "Assertion" "${RIC_LOG}" | head -3 >&2
fi

if [ "${FAILURES}" -gt 0 ]; then
    echo "[dapp-ci] ${FAILURES} failure(s); logs kept in ${WORK}" >&2
    # Keep the logs, but still take the derived conf back out of the source tree.
    trap - EXIT
    kill_wait "${RIC_PID}" TERM
    rm -f "${CONF_CLEANUP}"
    exit 1
fi
log "ALL PASS"
