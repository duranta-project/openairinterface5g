# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_asn1_msg
  test_asn1_msg.cpp
  ${OPENAIR2_DIR}/LAYER2/nr_pdcp/asn1_utils.c
  ${OPENAIR2_DIR}/LAYER2/nr_rlc/asn1_utils.c)
target_link_libraries(test_asn1_msg PRIVATE nr_rrc SECURITY minimal_lib GTest::gtest UTIL nr_common)
add_dependencies(tests test_asn1_msg)
add_test(NAME test_asn1_msg
    COMMAND ./test_asn1_msg)
