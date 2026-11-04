# SPDX-License-Identifier: LicenseRef-CSSL-1.0

set(NR_PHY_COMMON_SRC src/nr_phy_common.c
                      src/nr_channel_compensation.c
                      src/nr_compute_llr.c
                      src/nr_layer_demapping.c)

# 2-layer L-best SIMD kernels: one source, compiled once per vector width into its own object
# (nr_lbest_simd.c, NRLB_W = 128/256/512); nr_compute_llr.c dispatches to the native width.
foreach(w 128 256 512)
  add_library(nr_lbest_simd_w${w} OBJECT src/nr_lbest_simd.c)
  target_compile_definitions(nr_lbest_simd_w${w} PRIVATE NRLB_W=${w})
  target_link_libraries(nr_lbest_simd_w${w} PRIVATE UTIL PHY_COMMON)
  target_include_directories(nr_lbest_simd_w${w} PRIVATE inc/)
  set_target_properties(nr_lbest_simd_w${w} PROPERTIES POSITION_INDEPENDENT_CODE ON)
  list(APPEND NR_PHY_COMMON_SRC $<TARGET_OBJECTS:nr_lbest_simd_w${w}>)
endforeach()

add_library(nr_phy_common ${NR_PHY_COMMON_SRC})
target_link_libraries(nr_phy_common PRIVATE UTIL PHY_COMMON)
target_include_directories(nr_phy_common PUBLIC inc/)

# same as nr_phy_common but limited to 128-bit SIMD, to test the non-AVX2 paths on x86
add_library(nr_phy_common_no_avx_256 ${NR_PHY_COMMON_SRC})
target_link_libraries(nr_phy_common_no_avx_256 PRIVATE UTIL PHY_COMMON)
target_include_directories(nr_phy_common_no_avx_256 PUBLIC inc/)
target_compile_definitions(nr_phy_common_no_avx_256 PRIVATE USE_128BIT)

add_library(nr_ue_phy_meas src/nr_ue_phy_meas.c)
target_include_directories(nr_ue_phy_meas PUBLIC inc/)
target_link_libraries(nr_ue_phy_meas PUBLIC utils)

if (ENABLE_TESTS)
  add_subdirectory(tests)
endif()
