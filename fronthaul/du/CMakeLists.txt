# SPDX-License-Identifier: LicenseRef-CSSL-1.0

# use env var PKG_CONFIG_PATH to override paths to libdpdk.pc
pkg_check_modules(dpdk REQUIRED libdpdk>=22.11)

add_library(du_packet_processor du_packet_processor.c du_tx_scheduler.c)
target_include_directories(du_packet_processor PUBLIC ./)
target_link_libraries(du_packet_processor PUBLIC ${dpdk_LIBRARIES} PRIVATE xran_pkt UTIL fh_compression)

add_library(du_io du_io.c)
target_include_directories(du_io PUBLIC ./)
target_link_libraries(du_io PUBLIC fh_common ${dpdk_LIBRARIES} PRIVATE fh_compression)
target_include_directories(du_io PRIVATE ${dpdk_INCLUDE_DIRS})

add_library(du_fh du_fh.c)
target_include_directories(du_fh PUBLIC ./)
target_link_libraries(du_fh PUBLIC fh_common du_io xran_pkt du_packet_processor fh_compression ${dpdk_LIBRARIES})
target_include_directories(du_fh PRIVATE ${dpdk_INCLUDE_DIRS})

if (ENABLE_TESTS)
    add_subdirectory(tests)
endif()
