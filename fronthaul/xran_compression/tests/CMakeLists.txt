# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_xran_compression test_compression.cpp)
target_link_libraries(test_xran_compression PRIVATE xran_compression ${dpdk_LIBRARIES})
target_compile_options(test_xran_compression PRIVATE ${dpdk_CFLAGS} -march=native -Wno-psabi)
target_include_directories(test_xran_compression PRIVATE ../)
add_dependencies(tests test_xran_compression)
add_test(NAME test_xran_compression COMMAND test_xran_compression)
set_property(TEST test_xran_compression PROPERTY LABELS fronthaul)

find_package(benchmark REQUIRED)
add_executable(benchmark_xran_compression benchmark_compression.cpp)
target_link_libraries(benchmark_xran_compression PRIVATE xran_compression benchmark::benchmark ${dpdk_LIBRARIES})
target_compile_options(benchmark_xran_compression PRIVATE ${dpdk_CFLAGS} -march=native -Wno-psabi)
target_include_directories(benchmark_xran_compression PRIVATE ../)
add_dependencies(tests benchmark_xran_compression)
