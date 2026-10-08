# SPDX-License-Identifier: LicenseRef-CSSL-1.0

find_package(PkgConfig REQUIRED)
pkg_check_modules(dpdk REQUIRED libdpdk)
add_subdirectory(core)
add_subdirectory(xran_pkt)
add_subdirectory(xran_compression)
add_subdirectory(oru)
