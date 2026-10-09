# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_llr test_llr.cpp)
target_link_libraries(test_llr PRIVATE PHY_NR GTest::gtest minimal_lib)
add_dependencies(tests test_llr)
add_test(NAME test_llr
  COMMAND ./test_llr)

add_executable(test_llr_no_avx_256 test_llr.cpp)
target_link_libraries(test_llr_no_avx_256 PRIVATE PHY_NR_NO_AVX_256 GTest::gtest minimal_lib)
add_dependencies(tests test_llr_no_avx_256)
add_test(NAME test_llr_no_avx_256
  COMMAND ./test_llr_no_avx_256)

add_executable(test_nr_dbt test_nr_dbt.c)
target_link_libraries(test_nr_dbt PRIVATE PHY_NR minimal_lib)
add_dependencies(tests test_nr_dbt)
add_test(NAME test_nr_dbt
  COMMAND ./test_nr_dbt)
