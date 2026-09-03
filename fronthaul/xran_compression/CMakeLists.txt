# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(xran_compression STATIC
    xran_compression.cpp
    xran_bfp_ref.cpp
    xran_bfp_uplane.cpp
    xran_bfp_uplane_9b16rb.cpp
)

target_include_directories(xran_compression PUBLIC ./ ../xran_pkt)
target_compile_options(xran_compression PRIVATE ${dpdk_CFLAGS} -march=native -Wno-psabi)
target_link_libraries(xran_compression PRIVATE ${dpdk_LIBRARIES})
target_include_directories(xran_compression PUBLIC ${dpdk_INCLUDE_DIRS})

if (ENABLE_TESTS)
    add_subdirectory(tests)
endif()
