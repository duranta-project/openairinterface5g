#!/bin/bash
# SPDX-License-Identifier: MIT

function die() { echo $@; exit 1; }

[ $# -eq 3 ] || die "usage: $0 <path-to-log-file> <log-dir> <remote_machine>"

export PS4='[\D{%Y-%m-%d %H:%M:%S}] '
LOG_FILE=${1}
LOG_DIR=${2}
REMOTE=${3}
LOCAL_FILE=${LOG_DIR}/$(basename ${LOG_FILE})

set -x
ssh ${REMOTE} "tail -n 10000 '${LOG_FILE}'" > ${LOCAL_FILE} < /dev/null || die "cannot collect ${LOG_FILE} from ${REMOTE}"
