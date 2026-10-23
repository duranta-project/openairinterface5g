# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(du_tx_scheduler du_tx_scheduler.c)
target_include_directories(du_tx_scheduler PUBLIC ./)
target_link_libraries(du_tx_scheduler PUBLIC xran_pkt fh_compression PkgConfig::dpdk PRIVATE UTIL)

add_library(du_packet_processor du_packet_processor.c)
target_include_directories(du_packet_processor PUBLIC ./)
target_link_libraries(du_packet_processor PUBLIC fh_compression PkgConfig::dpdk PRIVATE xran_pkt UTIL)

add_library(du_io du_io.c)
target_include_directories(du_io PUBLIC ./)
target_link_libraries(du_io PUBLIC fh_common PkgConfig::dpdk PRIVATE fh_compression)

add_library(du_fh du_fh.c)
target_include_directories(du_fh PUBLIC ./)
target_link_libraries(du_fh PUBLIC fh_common du_io xran_pkt du_packet_processor du_tx_scheduler fh_compression PkgConfig::dpdk)

if (ENABLE_TESTS)
    add_subdirectory(tests)
endif()
