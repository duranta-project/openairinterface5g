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
