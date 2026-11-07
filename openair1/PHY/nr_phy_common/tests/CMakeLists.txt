# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_nr_layer_demapping test_layer_demapping.cpp)
target_link_libraries(test_nr_layer_demapping PRIVATE nr_phy_common GTest::gtest minimal_lib)
add_dependencies(tests test_nr_layer_demapping)
add_test(NAME test_nr_layer_demapping COMMAND ./test_nr_layer_demapping)

add_executable(test_llr test_llr.cpp)
target_link_libraries(test_llr PRIVATE PHY_NR SIMU GTest::gtest minimal_lib)
add_dependencies(tests test_llr)
add_test(NAME test_llr
  COMMAND ./test_llr)

add_executable(test_llr_no_avx_256 test_llr.cpp)
target_link_libraries(test_llr_no_avx_256 PRIVATE PHY_NR_NO_AVX_256 SIMU GTest::gtest minimal_lib)
add_dependencies(tests test_llr_no_avx_256)
add_test(NAME test_llr_no_avx_256
  COMMAND ./test_llr_no_avx_256)

# 2-layer L-best MIMO LLR kernels: width equivalence (w128==w256==w512) + float-model fidelity
add_executable(test_llr_2layer test_llr_2layer.cpp)
target_link_libraries(test_llr_2layer PRIVATE PHY_NR nr_phy_common GTest::gtest minimal_lib)
add_dependencies(tests test_llr_2layer)
add_test(NAME test_llr_2layer
  COMMAND ./test_llr_2layer)

add_executable(bench_rx2l bench_rx2l.cpp)
target_link_libraries(bench_rx2l PRIVATE PHY_NR nr_phy_common minimal_lib)

# GPU detectors: bit-exact against the CPU fp32 kernels (skipped without a device), and slot-level timing
if (ENABLE_PHY_GPU)
  # the CPU fp32 kernels without FMA contraction (symbols *_w128_exact): the GPU kernels must match them bit for bit
  add_library(nr_lbest_simd_exact OBJECT ../src/nr_lbest_simd.c)
  target_compile_definitions(nr_lbest_simd_exact PRIVATE NRLB_W=128 NRLB_TAG=_exact)
  target_compile_options(nr_lbest_simd_exact PRIVATE -ffp-contract=off)
  target_link_libraries(nr_lbest_simd_exact PRIVATE UTIL PHY_COMMON)
  target_include_directories(nr_lbest_simd_exact PRIVATE ../inc/)
  add_executable(test_detector_gpu test_detector_gpu.cpp test_inner_rx_gpu.cpp $<TARGET_OBJECTS:nr_lbest_simd_exact>)
  target_link_libraries(test_detector_gpu PRIVATE nr_phy_gpu PHY_NR nr_phy_common GTest::gtest minimal_lib)
  add_dependencies(tests test_detector_gpu)
  add_test(NAME test_detector_gpu COMMAND ./test_detector_gpu)

  add_executable(bench_detector_gpu bench_detector_gpu.cpp)
  target_link_libraries(bench_detector_gpu PRIVATE nr_phy_gpu minimal_lib)

  add_executable(bench_inner_rx_gpu bench_inner_rx_gpu.cpp)
  target_link_libraries(bench_inner_rx_gpu PRIVATE nr_phy_gpu PHY_NR nr_phy_common minimal_lib)
endif()
