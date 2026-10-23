# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(du_tx_scheduler du_tx_scheduler.c)
target_include_directories(du_tx_scheduler PUBLIC ./)
target_link_libraries(du_tx_scheduler PUBLIC xran_pkt fh_compression PkgConfig::dpdk PRIVATE UTIL)

add_library(du_packet_processor du_packet_processor.c)
target_include_directories(du_packet_processor PUBLIC ./)
target_link_libraries(du_packet_processor PUBLIC fh_compression PkgConfig::dpdk PRIVATE xran_pkt UTIL)

if (ENABLE_TESTS)
    add_subdirectory(tests)
endif()
