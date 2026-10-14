# SPDX-License-Identifier: LicenseRef-CSSL-1.0

# Limited float32 FFT: B same-size transforms per call, one per SIMD lane (see batched_fft.h).
add_library(batched_fft
  batched_fft.c
  batched_fft_scalar.c
  batched_fft_avx2.c
  batched_fft_avx512.c
  batched_fft_neon.c
  batched_fft_sve2.c
)
# x86 kernels are picked at runtime, so they are built for their ISA whatever the global flags.
if (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
  set_source_files_properties(batched_fft_avx2.c PROPERTIES COMPILE_OPTIONS "-mavx2;-mfma")
  set_source_files_properties(batched_fft_avx512.c PROPERTIES COMPILE_OPTIONS "-mavx512f;-mavx2;-mfma")
endif()
target_include_directories(batched_fft PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(batched_fft PRIVATE m)

if (ENABLE_TESTS)
  add_subdirectory(tests)
endif()
