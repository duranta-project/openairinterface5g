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

# 2-layer L-best MIMO LLR kernels: width equivalence (w128==w256==w512) + float-model fidelity
add_executable(test_llr_2layer test_llr_2layer.cpp)
target_link_libraries(test_llr_2layer PRIVATE PHY_NR nr_phy_common GTest::gtest minimal_lib)
add_dependencies(tests test_llr_2layer)
add_test(NAME test_llr_2layer
  COMMAND ./test_llr_2layer)
