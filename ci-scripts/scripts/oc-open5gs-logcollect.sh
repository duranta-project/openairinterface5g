#!/bin/bash
# SPDX-License-Identifier: MIT

set -e

function die() { echo $@; exit 1; }
[ $# -eq 3 ] || die "usage: $0 <path-to-dir> <namespace> <dir>"

OC_DIR=${1}
OC_NS=${2}
DIR=${3}

cat ${OC_DIR}/oc-password | oc login -u oaicicd --server https://api.oai.cs.eurecom.fr:6443 --insecure-skip-tls-verify > /dev/null
oc project ${OC_NS} > /dev/null
oc describe pod > ${DIR}/describe-pods-post-test.log
oc get pods.metrics.k8s &> ${DIR}/nf-resource-consumption.log
oc logs -l app.kubernetes.io/name=open5gs --tail=-1 > ${DIR}/open5gs.log
oc logout > /dev/null
