#!/bin/bash
# SPDX-License-Identifier: MIT

function die() { echo $@; exit 1; }
function usage() { die "usage: $1 [-p <prefix>] <ctest-options>"; }

PREFIX=
CTEST_OPT=

while getopts ":p:h" opt; do
  case "$opt" in
    p) PREFIX="$OPTARG";;
    h) usage $0;;
    :) echo "unrecognized option -$OPTARG, using as ctest option"; CTEST_OPT=-$OPTARG; break;;
    ?) echo "unrecognized option -$OPTARG, using as ctest option"; CTEST_OPT=-$OPTARG; break;;
  esac
done
# skip what has been read above, then use the rest in CTEST_OPT
shift $(($OPTIND-1))
CTEST_OPT="${CTEST_OPT} ${@}"

echo PREFIX=$PREFIX
echo CTEST_OPT=$CTEST_OPT

set -x
# script run inside ci-scripts/, switch to root
cd ..
BDIR=cmake_targets/ran_build/build

ninja -C ${BDIR} ldpctest polartest smallblocktest nr_pbchsim nr_dlschsim nr_ulschsim nr_dlsim nr_ulsim nr_pucchsim nr_prachsim nr_psbchsim

# get a JSON description of all tests to run
ctest --test-dir ${BDIR} ${CTEST_OPT} --show-only=json-v1 &> desc-tests.json
JSON_RES=$?

# we run the T2 offload tests, so provide some additional permissions
# to be able to initialize DPDK
sudo setcap cap_dac_override,cap_sys_admin+ep ${BDIR}/nr_ulsim
sudo setcap cap_dac_override,cap_sys_admin+ep ${BDIR}/nr_dlsim
${PREFIX} ctest --test-dir ${BDIR} ${CTEST_OPT} --output-junit results-run.xml --test-output-size-passed 100000 --test-output-size-failed 100000 &>> physim_log.txt
RUN_RES=$?
cp ${BDIR}/results-run.xml .
cp ${BDIR}/Testing/Temporary/LastTestsFailed.log .
cp ${BDIR}/Testing/Temporary/LastTest.log .

# if both were successful, return 0
# TODO not sure
#[[ $JSON_RES -eq 0 && $RUN_RES -eq 0 ]] && exit 0

exit 0
