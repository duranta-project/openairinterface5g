# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(nrup_test nrup_test.cpp)
target_link_libraries(nrup_test PRIVATE nr_up_f1 f1ap GTPV1U GTest::gtest minimal_lib)
target_link_libraries(nrup_test PRIVATE ITTI asn1_lte_rrc_hdrs asn1_nr_rrc_hdrs)
add_dependencies(tests nrup_test)
add_test(NAME nrup_test COMMAND ./nrup_test)
