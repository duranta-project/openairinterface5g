# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_asn1_msg
  test_asn1_msg.cpp
  ${OPENAIR2_DIR}/LAYER2/nr_pdcp/asn1_utils.c
  ${OPENAIR2_DIR}/LAYER2/nr_rlc/asn1_utils.c)
target_link_libraries(test_asn1_msg PRIVATE nr_rrc SECURITY minimal_lib GTest::gtest UTIL nr_common)
add_dependencies(tests test_asn1_msg)
add_test(NAME test_asn1_msg
    COMMAND ./test_asn1_msg)

option(SIB9_TEST_SANITIZERS "Instrument SIB9 test translation units with ASan/UBSan" OFF)
add_executable(test_sib9_config test_sib9_config.c
  ${OPENAIR2_DIR}/GNB_APP/gnb_sib9.c ${OPENAIR2_DIR}/RRC/NR/nr_sib9.c)
target_link_libraries(test_sib9_config PRIVATE nr_rrc SECURITY minimal_lib UTIL nr_common)
target_link_options(test_sib9_config PRIVATE -Wl,--wrap=clock_gettime)
add_dependencies(test_sib9_config params_libconfig)
if(SIB9_TEST_SANITIZERS)
  target_compile_options(test_sib9_config PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(test_sib9_config PRIVATE -fsanitize=address,undefined)
endif()
add_dependencies(tests test_sib9_config)
add_test(NAME test_sib9_config COMMAND test_sib9_config)

set_tests_properties(test_sib9_config PROPERTIES ENVIRONMENT "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:params_libconfig>")

add_executable(test_sib9_pipeline test_sib9_pipeline.c
  ${OPENAIR2_DIR}/RRC/NR/nr_sib9.c
  ${OPENAIR2_DIR}/LAYER2/NR_MAC_gNB/config.c
  ${OPENAIR2_DIR}/LAYER2/NR_MAC_gNB/nr_radio_config.c
  ${OPENAIR2_DIR}/LAYER2/NR_MAC_gNB/nr_mac_cell.c
  ${OPENAIR2_DIR}/LAYER2/NR_MAC_gNB/gNB_scheduler_pcch.c)
target_compile_options(test_sib9_pipeline PRIVATE -ffunction-sections -fdata-sections)
target_link_options(test_sib9_pipeline PRIVATE -Wl,--gc-sections -Wl,--no-export-dynamic -Wl,--wrap=try_encode_SIB_NR)
target_link_libraries(test_sib9_pipeline PRIVATE nr_rrc f1ap_lib asn1_f1ap SECURITY minimal_lib UTIL nr_common m)
if(SIB9_TEST_SANITIZERS)
  target_compile_options(test_sib9_pipeline PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(test_sib9_pipeline PRIVATE -fsanitize=address,undefined)
endif()
add_dependencies(tests test_sib9_pipeline)
add_test(NAME test_sib9_pipeline COMMAND test_sib9_pipeline)

foreach(target IN ITEMS test_sib9_timing benchmark_sib9)
  add_executable(${target} test_sib9_timing.c
    ${OPENAIR2_DIR}/RRC/NR/nr_sib9.c
    ${OPENAIR2_DIR}/LAYER2/NR_MAC_gNB/config.c
    ${OPENAIR2_DIR}/LAYER2/NR_MAC_gNB/nr_radio_config.c
    ${OPENAIR2_DIR}/LAYER2/NR_MAC_gNB/nr_sib9_mac.c)
  target_compile_options(${target} PRIVATE -ffunction-sections -fdata-sections)
  target_link_options(${target} PRIVATE -Wl,--gc-sections -Wl,--no-export-dynamic
    -Wl,--wrap=clock_gettime -Wl,--wrap=encode_sysinfo_ie)
  target_link_libraries(${target} PRIVATE nr_rrc f1ap_lib asn1_f1ap SECURITY minimal_lib UTIL nr_common m)
  add_dependencies(tests ${target})
endforeach()
if(SIB9_TEST_SANITIZERS)
  target_compile_options(test_sib9_timing PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(test_sib9_timing PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME test_sib9_timing COMMAND test_sib9_timing)
add_test(NAME benchmark_sib9 COMMAND benchmark_sib9 --benchmark)
