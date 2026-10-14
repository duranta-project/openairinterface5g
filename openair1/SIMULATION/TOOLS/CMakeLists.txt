# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(noise_device noise_device.c)
target_link_libraries(noise_device PRIVATE log_headers SIMU)
target_include_directories(noise_device PUBLIC ./)
add_subdirectory(batched_fft)
add_library(channel_pipeline
  channel_pipeline.c
  channel_pipeline_simd.c
  channel_pipeline_scalar.c
  channel_pipeline_avx2.c
  channel_pipeline_avx512.c
  channel_pipeline_neon.c
  channel_pipeline_sve2.c
)
# x86 kernels are picked at runtime, so they are built for their ISA whatever the global flags.
if (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
  set_source_files_properties(channel_pipeline_avx2.c PROPERTIES COMPILE_OPTIONS "-mavx2;-mfma")
  set_source_files_properties(channel_pipeline_avx512.c PROPERTIES COMPILE_OPTIONS "-mavx512f;-mavx2;-mfma")
endif()
if (ENABLE_CHANNEL_SIM_CUDA)
  target_sources(channel_pipeline PRIVATE
    channel_pipeline_gpu.cu
  )
endif()
target_link_libraries(channel_pipeline PUBLIC thread-pool noise_device PRIVATE batched_fft)
if (ENABLE_CHANNEL_SIM_CUDA)
  target_link_libraries(channel_pipeline PUBLIC CUDA::toolkit CUDA::cufft)
endif()

if(ENABLE_CHANNEL_SIM_CUDA)
    add_library(oai_cuda_lib STATIC
        multipath_channel.cu
        phase_noise.cu
        channel_pipeline.cu
    )

    target_link_libraries(oai_cuda_lib PUBLIC CUDA::toolkit)

    target_include_directories(oai_cuda_lib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(channel_pipeline PUBLIC oai_cuda_lib)
endif()
