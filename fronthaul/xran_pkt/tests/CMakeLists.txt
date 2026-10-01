# SPDX-License-Identifier: LicenseRef-CSSL-1.0

find_package(PkgConfig REQUIRED)
pkg_check_modules(PCAP REQUIRED libpcap)
add_executable(test_xran_pkt test_xran_pkt.c)
target_link_libraries(test_xran_pkt PRIVATE xran_pkt ${dpdk_LIBRARIES})
target_compile_options(test_xran_pkt PRIVATE ${dpdk_CFLAGS})
target_include_directories(test_xran_pkt PRIVATE ../)
add_dependencies(tests test_xran_pkt)
add_test(NAME test_xran_pkt COMMAND test_xran_pkt --no-pci --no-huge)
set_property(TEST test_xran_pkt PROPERTY LABELS fronthaul)

add_executable(xran_pcap_dump xran_pcap_dump.c)
target_link_libraries(xran_pcap_dump PRIVATE xran_pkt ${dpdk_LIBRARIES} ${PCAP_LIBRARIES})
target_compile_options(xran_pcap_dump PRIVATE ${dpdk_CFLAGS} -march=native)
target_include_directories(xran_pcap_dump PRIVATE ../ ${PCAP_INCLUDE_DIRS})


