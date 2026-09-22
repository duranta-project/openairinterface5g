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

add_executable(test_pos_sib_config test_pos_sib_config.c ../config.c ../nr_radio_config.c)
target_include_directories(test_pos_sib_config PRIVATE ${OPENAIR2_DIR}/LAYER2 ${OPENAIR2_DIR}/F1AP/lib)
target_compile_options(test_pos_sib_config PRIVATE -ffunction-sections -fdata-sections)
target_link_options(test_pos_sib_config PRIVATE -Wl,--gc-sections,--no-export-dynamic)
target_link_libraries(test_pos_sib_config PRIVATE asn1_nr_rrc asn1_lte_rrc_hdrs nr_common minimal_lib ds m)
add_dependencies(tests test_pos_sib_config)
add_test(NAME test_pos_sib_config COMMAND test_pos_sib_config)

add_executable(test_si_window test_si_window.c)
target_include_directories(test_si_window PRIVATE ${OPENAIR2_DIR}/LAYER2)
target_compile_options(test_si_window PRIVATE -ffunction-sections -fdata-sections)
target_link_options(test_si_window PRIVATE -Wl,--gc-sections,--no-export-dynamic)
target_link_libraries(test_si_window PRIVATE asn1_nr_rrc_hdrs asn1_lte_rrc_hdrs minimal_lib m)
add_dependencies(tests test_si_window)
add_test(NAME test_si_window COMMAND test_si_window)
