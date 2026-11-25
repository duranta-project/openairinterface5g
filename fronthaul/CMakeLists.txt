# SPDX-License-Identifier: LicenseRef-CSSL-1.0

find_package(PkgConfig REQUIRED)
# use env var PKG_CONFIG_PATH to override paths to libdpdk.pc
pkg_check_modules(dpdk REQUIRED IMPORTED_TARGET libdpdk>=22.11)
add_subdirectory(common)
add_subdirectory(oru)
add_subdirectory(du)

if (ENABLE_TESTS)
    add_subdirectory(tests)
endif()
