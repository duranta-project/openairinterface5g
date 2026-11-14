# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_asn1_msg
  test_asn1_msg.cpp
  ${OPENAIR2_DIR}/LAYER2/nr_pdcp/asn1_utils.c
  ${OPENAIR2_DIR}/LAYER2/nr_rlc/asn1_utils.c)
target_link_libraries(test_asn1_msg PRIVATE nr_rrc SECURITY minimal_lib GTest::gtest UTIL nr_common)
add_dependencies(tests test_asn1_msg)
add_test(NAME test_asn1_msg
    COMMAND ./test_asn1_msg)

add_executable(benchmark_nr_rrc_asn1
  benchmark_nr_rrc_asn1.cpp
  ${OPENAIR2_DIR}/LAYER2/nr_pdcp/asn1_utils.c
  ${OPENAIR2_DIR}/LAYER2/nr_rlc/asn1_utils.c)
target_link_libraries(benchmark_nr_rrc_asn1 PRIVATE nr_rrc SECURITY minimal_lib benchmark::benchmark UTIL nr_common)
add_dependencies(tests benchmark_nr_rrc_asn1)
add_test(NAME benchmark_nr_rrc_asn1
    COMMAND ./benchmark_nr_rrc_asn1)
