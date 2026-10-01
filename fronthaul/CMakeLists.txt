# SPDX-License-Identifier: LicenseRef-CSSL-1.0

find_package(PkgConfig REQUIRED)
pkg_check_modules(dpdk REQUIRED libdpdk)
add_subdirectory(common)
add_subdirectory(oru)
add_subdirectory(du)
