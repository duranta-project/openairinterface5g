# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_nr_layer_demapping test_layer_demapping.cpp)
target_link_libraries(test_nr_layer_demapping PRIVATE nr_phy_common GTest::gtest minimal_lib)
add_dependencies(tests test_nr_layer_demapping)
add_test(NAME test_nr_layer_demapping COMMAND ./test_nr_layer_demapping)

add_executable(test_nr_ml_llr test_ml_llr.cpp)
target_link_libraries(test_nr_ml_llr PRIVATE PHY_NR GTest::gtest minimal_lib)
add_dependencies(tests test_nr_ml_llr)
add_test(NAME test_nr_ml_llr COMMAND ./test_nr_ml_llr)

# same kernels built as the 128-bit variant used on aarch64 (PHY_NR_NO_AVX_256 defines USE_128BIT)
add_executable(test_nr_ml_llr_128 test_ml_llr.cpp ../src/nr_compute_llr.c)
target_link_libraries(test_nr_ml_llr_128 PRIVATE PHY_NR_NO_AVX_256 GTest::gtest minimal_lib)
add_dependencies(tests test_nr_ml_llr_128)
add_test(NAME test_nr_ml_llr_128 COMMAND ./test_nr_ml_llr_128)
