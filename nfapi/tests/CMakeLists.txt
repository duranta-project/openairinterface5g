# SPDX-License-Identifier: LicenseRef-CSSL-1.0

if (ENABLE_TESTS)
    add_subdirectory(p5)
    add_subdirectory(p7)

    add_executable(nfapi_pull_push_test nfapi_pull_push_test.c)
    target_link_libraries(nfapi_pull_push_test PRIVATE nfapi_common pthread UTIL ${T_LIB} minimal_lib)
    add_dependencies(tests nfapi_pull_push_test)
    add_test(nfapi_pull_push_test nfapi_pull_push_test)
    set_tests_properties(nfapi_pull_push_test PROPERTIES LABELS "fapi")
endif ()
