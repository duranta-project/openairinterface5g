#!/bin/bash
# SPDX-License-Identifier: MIT

set -x
CTEST_OPT=$@

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
numactl --cpunodebind=1 --membind=1 ctest --test-dir ${BDIR} ${CTEST_OPT} --output-junit results-run.xml --test-output-size-passed 100000 --test-output-size-failed 100000 &>> physim_log.txt
RUN_RES=$?
cp ${BDIR}/results-run.xml .
cp ${BDIR}/Testing/Temporary/LastTestsFailed.log .
cp ${BDIR}/Testing/Temporary/LastTest.log .

# if both were successful, return 0
# TODO not sure
#[[ $JSON_RES -eq 0 && $RUN_RES -eq 0 ]] && exit 0

exit 0
