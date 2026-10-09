# SPDX-License-Identifier: MIT

function die() { echo $@; exit 1; }
[ $# -ge 1 ] || die "usage: $0 <directory> [ctest-options]"

set -x
DIR=$1
shift
CTEST_OPT=$@

cd ${DIR}/cmake_targets/ran_build/build
ninja ldpctest polartest smallblocktest nr_pbchsim nr_dlschsim nr_ulschsim nr_dlsim nr_ulsim nr_pucchsim nr_prachsim nr_psbchsim

# get a JSON description of all tests to run
ctest ${CTEST_OPT} --show-only=json-v1 &> ${DIR}/desc-tests.json
JSON_RES=$?

# we run the T2 offload tests, so provide some additional permissions
# to be able to initialize DPDK
sudo setcap cap_dac_override,cap_sys_admin+ep nr_ulsim
sudo setcap cap_dac_override,cap_sys_admin+ep nr_dlsim
numactl --cpunodebind=1 --membind=1 ctest ${CTEST_OPT} --output-junit ${DIR}/results-run.xml --test-output-size-passed 100000 --test-output-size-failed 100000 &>> ${DIR}/physim_log.txt
RUN_RES=$?
cp Testing/Temporary/LastTestsFailed.log ${DIR}/
cp Testing/Temporary/LastTest.log ${DIR}/

# if both were successful, return 0
# TODO not sure
#[[ $JSON_RES -eq 0 && $RUN_RES -eq 0 ]] && exit 0

exit 0
