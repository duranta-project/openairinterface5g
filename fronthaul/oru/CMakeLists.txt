# SPDX-License-Identifier: LicenseRef-CSSL-1.0


add_library(oru_pcap oru_pcap.c)
target_include_directories(oru_pcap PUBLIC ./)
target_link_libraries(oru_pcap PUBLIC xran_pkt UTIL pthread PkgConfig::dpdk)

add_library(oru_io oru_io.c)
target_include_directories(oru_io PUBLIC ./)
target_link_libraries(oru_io PUBLIC fh_common PkgConfig::dpdk PRIVATE oru_pcap)

add_library(oru_packet_processor oru_packet_processor.c)
target_include_directories(oru_packet_processor PUBLIC ./)
target_link_libraries(oru_packet_processor PUBLIC fh_compression PkgConfig::dpdk PRIVATE xran_pkt UTIL oru_pcap)

add_library(oru_beamforming oru_beamforming.c)
target_include_directories(oru_beamforming PUBLIC ./)
target_link_libraries(oru_beamforming PUBLIC oru_packet_processor PRIVATE UTIL)

add_library(oru_fh oru_fh.c)
target_include_directories(oru_fh PUBLIC ./)
target_link_libraries(oru_fh PUBLIC fh_common oru_io xran_pkt oru_packet_processor oru_pcap PkgConfig::dpdk)

if (ENABLE_TESTS)
    add_subdirectory(tests)
endif()
