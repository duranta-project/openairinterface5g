#!/bin/bash
# SPDX-License-Identifier: MIT

set -e

function die() { echo $@; exit 1; }
[ $# -eq 2 ] || die "usage: $0 <path-to-dir> <namespace>"

OC_DIR=${1}
OC_NS=${2}

cat ${OC_DIR}/oc-password | oc login -u oaicicd --server https://api.oai.cs.eurecom.fr:6443 --insecure-skip-tls-verify > /dev/null
oc project ${OC_NS} > /dev/null
helm install --wait open5gs ${OC_DIR}/open5gs-charts/.
oc logout > /dev/null
