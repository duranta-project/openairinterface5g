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

# aarch64: the FP16 L-best (ARMv8.2 FP16 vector arithmetic, selected at run time on CPUs that have it)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
  add_library(nr_lbest_simd_f16 OBJECT src/nr_lbest_simd.c)
  target_compile_definitions(nr_lbest_simd_f16 PRIVATE NRLB_W=128 NRLB_F16)
  target_compile_options(nr_lbest_simd_f16 PRIVATE -march=armv8.2-a+fp16)
  target_link_libraries(nr_lbest_simd_f16 PRIVATE UTIL PHY_COMMON)
  target_include_directories(nr_lbest_simd_f16 PRIVATE inc/)
  set_target_properties(nr_lbest_simd_f16 PROPERTIES POSITION_INDEPENDENT_CODE ON)
  list(APPEND NR_PHY_COMMON_SRC $<TARGET_OBJECTS:nr_lbest_simd_f16>)
endif()

add_library(nr_phy_common ${NR_PHY_COMMON_SRC})
target_link_libraries(nr_phy_common PRIVATE UTIL PHY_COMMON)
target_include_directories(nr_phy_common PUBLIC inc/)

# same as nr_phy_common but limited to 128-bit SIMD, to test the non-AVX2 paths on x86
add_library(nr_phy_common_no_avx_256 ${NR_PHY_COMMON_SRC})
target_link_libraries(nr_phy_common_no_avx_256 PRIVATE UTIL PHY_COMMON)
target_include_directories(nr_phy_common_no_avx_256 PUBLIC inc/)
target_compile_definitions(nr_phy_common_no_avx_256 PRIVATE USE_128BIT)

# CUDA/HIP 2-layer detectors, bit-exact with the fp32 CPU kernels (backend: LDPC_GPU_BACKEND, top-level CMakeLists.txt).
# Contraction off and IEEE division keep the GPU arithmetic identical to the SIMD one.
add_boolean_option(ENABLE_PHY_GPU OFF "Build the CUDA/HIP NR inner-RX detector kernels" OFF)
if (ENABLE_PHY_GPU)
  add_library(nr_phy_gpu STATIC gpu/nr_detector_gpu.cu gpu/nr_inner_rx_gpu.cu)
  target_include_directories(nr_phy_gpu PUBLIC inc/ ${OPENAIR_DIR}/common)
  set_target_properties(nr_phy_gpu PROPERTIES POSITION_INDEPENDENT_CODE ON)
  if (LDPC_GPU_BACKEND STREQUAL "HIP")
    enable_language(HIP)
    set_source_files_properties(gpu/nr_detector_gpu.cu gpu/nr_inner_rx_gpu.cu PROPERTIES LANGUAGE HIP)
    set(NR_PHY_GPU_HIP_ARCHS "gfx90a;gfx1151" CACHE STRING "HIP architectures of the NR inner-RX GPU kernels")
    set_target_properties(nr_phy_gpu PROPERTIES HIP_ARCHITECTURES "${NR_PHY_GPU_HIP_ARCHS}")
    target_compile_options(nr_phy_gpu PRIVATE $<$<COMPILE_LANGUAGE:HIP>:-ffp-contract=off>)
    target_link_libraries(nr_phy_gpu PRIVATE hip::device PUBLIC hip::host)
  else()
    enable_language(CUDA)
    find_package(CUDAToolkit REQUIRED)
    # GH200 = 90, GB10 (DGX Spark) = 121
    if (CMAKE_VERSION VERSION_LESS 3.24) # no "native" before
      set(NR_PHY_GPU_CUDA_ARCHS "90" CACHE STRING "CUDA architectures of the NR inner-RX GPU kernels")
    else()
      set(NR_PHY_GPU_CUDA_ARCHS "native" CACHE STRING "CUDA architectures of the NR inner-RX GPU kernels")
    endif()
    set_target_properties(nr_phy_gpu PROPERTIES CUDA_ARCHITECTURES "${NR_PHY_GPU_CUDA_ARCHS}")
    target_compile_options(nr_phy_gpu PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:--fmad=false>)
    target_link_libraries(nr_phy_gpu PUBLIC CUDA::cudart)
  endif()

  # inner-RX offload module (nr_inner_rx_offload.h): --loader.inner_rx.shlibversion _gpu
  add_library(inner_rx_gpu MODULE gpu/nr_irx_gpu.c)
  target_link_libraries(inner_rx_gpu PRIVATE nr_phy_gpu)
  set_target_properties(inner_rx_gpu PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
  foreach(t nr-softmodem nr_ulsim)
    if (TARGET ${t})
      add_dependencies(${t} inner_rx_gpu)
    endif()
  endforeach()
endif()

add_library(nr_ue_phy_meas src/nr_ue_phy_meas.c)
target_include_directories(nr_ue_phy_meas PUBLIC inc/)
target_link_libraries(nr_ue_phy_meas PUBLIC utils)

if (ENABLE_TESTS)
  add_subdirectory(tests)
endif()
