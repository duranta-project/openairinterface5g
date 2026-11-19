# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_nr_layer_demapping test_layer_demapping.cpp)
target_link_libraries(test_nr_layer_demapping PRIVATE nr_phy_common GTest::gtest minimal_lib)
add_dependencies(tests test_nr_layer_demapping)
add_test(NAME test_nr_layer_demapping COMMAND ./test_nr_layer_demapping)

add_executable(test_llr test_llr.cpp)
target_link_libraries(test_llr PRIVATE PHY_NR SIMU GTest::gtest)
add_dependencies(tests test_llr)
add_test(NAME test_llr
  COMMAND ./test_llr)

add_executable(test_llr_no_avx_256 test_llr.cpp)
target_link_libraries(test_llr_no_avx_256 PRIVATE PHY_NR_NO_AVX_256 SIMU GTest::gtest)
add_dependencies(tests test_llr_no_avx_256)
add_test(NAME test_llr_no_avx_256
  COMMAND ./test_llr_no_avx_256)

add_executable(test_csi_pmi test_csi_pmi.cpp)
target_link_libraries(test_csi_pmi PRIVATE nr_csi_pmi GTest::gtest GTest::gtest_main)
add_dependencies(tests test_csi_pmi)
add_test(NAME test_csi_pmi COMMAND ./test_csi_pmi)
