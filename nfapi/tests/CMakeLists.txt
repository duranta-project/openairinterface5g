# SPDX-License-Identifier: LicenseRef-CSSL-1.0

if (ENABLE_TESTS)
    add_subdirectory(p5)
    add_subdirectory(p7)

    add_executable(nfapi_pull_push_test nfapi_pull_push_test.c)
    target_link_libraries(nfapi_pull_push_test PRIVATE nfapi_common pthread UTIL ${T_LIB} minimal_lib)
    add_dependencies(tests nfapi_pull_push_test)
    add_test(nfapi_pull_push_test nfapi_pull_push_test)
    set_tests_properties(nfapi_pull_push_test PROPERTIES LABELS "fapi")

    add_executable(nfapi_push_pull_bench_le nfapi_push_pull_bench.c)
    target_link_libraries(nfapi_push_pull_bench_le PRIVATE nfapi_common_le minimal_lib)
    add_dependencies(tests nfapi_push_pull_bench_le)
    add_test(benchmark_nfapi_push_pull_le nfapi_push_pull_bench_le)
    set_tests_properties(benchmark_nfapi_push_pull_le PROPERTIES LABELS "fapi")

    add_executable(nfapi_push_pull_bench_be nfapi_push_pull_bench.c)
    target_link_libraries(nfapi_push_pull_bench_be PRIVATE nfapi_common_be minimal_lib)
    add_dependencies(tests nfapi_push_pull_bench_be)
    add_test(benchmark_nfapi_push_pull_be nfapi_push_pull_bench_be)
    set_tests_properties(benchmark_nfapi_push_pull_be PROPERTIES LABELS "fapi")
endif ()
