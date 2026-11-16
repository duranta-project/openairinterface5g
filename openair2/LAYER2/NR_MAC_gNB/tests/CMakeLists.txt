# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_nr_mac_gnb
  test_nr_mac_gnb.cpp
  test_nr_mac_gnb_common.c
  test_nr_other_sib_ssb_index.c
  test_nr_other_sib_ssb_index.cpp
)
target_link_libraries(test_nr_mac_gnb PRIVATE L2_NR MAC_NR_COMMON SCHED_NR_LIB CONFIG_LIB UTIL ITTI physim_common time_management_core softmodem_common ${T_LIB} asn1_nr_rrc_hdrs asn1_lte_rrc_hdrs GTest::gtest)
add_dependencies(tests test_nr_mac_gnb)
include(GoogleTest)
# one process per test, see test_nr_mac_gnb.cpp
gtest_discover_tests(test_nr_mac_gnb)

# nr_srs_estimate_aoa() (nr_srs_aoa.h) is header-only and self-contained (no MAC/RRC/logging
# dependencies), so this test links only what the header itself needs - not the L2_NR library.
add_executable(test_nr_srs_aoa test_nr_srs_aoa.c)
target_include_directories(test_nr_srs_aoa PRIVATE
    ${OPENAIR_DIR}
    ${OPENAIR2_DIR}
)
target_link_libraries(test_nr_srs_aoa PRIVATE m)
add_dependencies(tests test_nr_srs_aoa)
add_test(NAME test_nr_srs_aoa COMMAND test_nr_srs_aoa)
