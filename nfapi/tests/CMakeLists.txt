# SPDX-License-Identifier: LicenseRef-CSSL-1.0

if (ENABLE_TESTS)
    # ENABLE_AERIAL selects the cuBB wire format in the NR FAPI codec. Unless this
    # already is an Aerial build, compile a second copy of the codec with it so the
    # round-trip tests cover both formats.
    if (NOT OAI_AERIAL)
        add_library(nr_fapi_aerial
                ${NFAPI_DIR}/common/src/debug.c
                ${NFAPI_DIR}/common/src/nfapi.c
                ${NFAPI_DIR}/fapi/src/nr_fapi.c
                ${NFAPI_DIR}/fapi/src/nr_fapi_p5.c
                ${NFAPI_DIR}/fapi/src/nr_fapi_p5_utils.c
                ${NFAPI_DIR}/fapi/src/nr_fapi_p7.c
                ${NFAPI_DIR}/fapi/src/nr_fapi_p7_utils.c)
        target_include_directories(nr_fapi_aerial PUBLIC ${NFAPI_DIR}/common/public_inc ${NFAPI_DIR}/fapi/inc)
        target_compile_definitions(nr_fapi_aerial PUBLIC ENABLE_AERIAL)
        target_compile_definitions(nr_fapi_aerial PRIVATE FAPI_BYTE_ORDERING_BIG_ENDIAN)
    endif ()

    # add_nr_fapi_test(<name> <codec> <labels> [NO_AERIAL]): <name>_test against
    # <codec>, plus <name>_aerial_test against nr_fapi_aerial when it exists.
    # NO_AERIAL marks messages that don't round-trip under ENABLE_AERIAL: OAI only
    # packs them for cuBB and leaves out what cuBB doesn't take, or (SRS.indication)
    # cuBB sends a different body than OAI's PNF.
    function(add_nr_fapi_test name codec labels)
        add_executable(${name}_test ${name}_test.c)
        target_link_libraries(${name}_test PUBLIC ${codec})
        target_link_libraries(${name}_test PRIVATE pthread UTIL ${T_LIB} minimal_lib)
        add_dependencies(tests ${name}_test)
        add_test(${name}_test ${name}_test)
        set_tests_properties(${name}_test PROPERTIES LABELS "${labels}")
        if (TARGET nr_fapi_aerial AND NOT "NO_AERIAL" IN_LIST ARGN)
            add_executable(${name}_aerial_test ${name}_test.c)
            target_link_libraries(${name}_aerial_test PUBLIC nr_fapi_aerial)
            target_link_libraries(${name}_aerial_test PRIVATE pthread UTIL ${T_LIB} minimal_lib)
            add_dependencies(tests ${name}_aerial_test)
            add_test(${name}_aerial_test ${name}_aerial_test)
            set_tests_properties(${name}_aerial_test PROPERTIES LABELS "${labels};aerial")
        endif ()
    endfunction()

    add_subdirectory(p5)
    add_subdirectory(p7)
endif ()
