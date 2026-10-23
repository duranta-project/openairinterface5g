# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(du_tx_scheduler du_tx_scheduler.c)
target_include_directories(du_tx_scheduler PUBLIC ./)
target_link_libraries(du_tx_scheduler PUBLIC xran_pkt fh_compression PkgConfig::dpdk PRIVATE UTIL)

if (ENABLE_TESTS)
    add_subdirectory(tests)
endif()
