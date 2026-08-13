# SPDX-License-Identifier: LicenseRef-CSSL-1.0

# use env var PKG_CONFIG_PATH to override paths to libdpdk.pc
pkg_check_modules(dpdk REQUIRED libdpdk>=22.11)

add_library(oru_pcap oru_pcap.c)
target_include_directories(oru_pcap PUBLIC ./)
target_include_directories(oru_pcap PRIVATE ${dpdk_INCLUDE_DIRS})
target_link_libraries(oru_pcap PUBLIC xran_pkt UTIL pthread ${dpdk_LIBRARIES})

add_library(oru_io oru_io.c)
target_include_directories(oru_io PUBLIC ./)
target_link_libraries(oru_io PUBLIC fh_core ${dpdk_LIBRARIES} PRIVATE oru_pcap)
target_include_directories(oru_io PRIVATE ${dpdk_INCLUDE_DIRS})

add_library(fh_compression fh_compression.c)
target_include_directories(fh_compression PUBLIC ./)
target_link_libraries(fh_compression PRIVATE UTIL)

add_library(oru_packet_processor oru_packet_processor.c)
target_include_directories(oru_packet_processor PUBLIC ./)
target_link_libraries(oru_packet_processor PUBLIC ${dpdk_LIBRARIES} PRIVATE xran_pkt UTIL fh_compression oru_pcap)

add_library(oru_fh oru_fh.c)
target_include_directories(oru_fh PUBLIC ./)
target_link_libraries(oru_fh PUBLIC fh_core oru_io xran_pkt oru_packet_processor oru_pcap ${dpdk_LIBRARIES})
target_include_directories(oru_fh PRIVATE ${dpdk_INCLUDE_DIRS})

if (ENABLE_TESTS)
    add_subdirectory(tests)
endif()
