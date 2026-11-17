# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(remote_sim_server
    uicc.c
    atr.c
    tpdu-apdu.c
    main.c
    net.c
    pl2303.c
    pcsc.c
    usim.c
    util.c
    log.c
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PCSC libpcsclite)

if (PCSC_FOUND)
    message(STATUS "libpcsclite found, include support for PC/SC in remote_sim_server")

    set_source_files_properties(main.c PROPERTIES
        COMPILE_DEFINITIONS "PCSC"
    )

    set_source_files_properties(pcsc.c PROPERTIES
        COMPILE_DEFINITIONS "PCSC"
        COMPILE_OPTIONS "${PCSC_CFLAGS}"
    )

    target_link_libraries(remote_sim_server PRIVATE ${PCSC_LDFLAGS})
else()
    message(STATUS "libpcsclite not found, PC/SC not supported in remote_sim_server")
endif()
