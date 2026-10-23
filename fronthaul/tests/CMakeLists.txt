# SPDX-License-Identifier: LicenseRef-CSSL-1.0

# Tests that wire the O-DU and O-RU libraries to each other
set(DPDK_TEST_ARGS "--no-huge" "--iova-mode=va" "--no-pci" "-m" "1024")

add_executable(test_du_oru_loopback test_du_oru_loopback.c)
target_link_libraries(test_du_oru_loopback PRIVATE du_tx_scheduler du_packet_processor oru_packet_processor xran_pkt fh_compression log_headers UTIL PkgConfig::dpdk)
target_include_directories(test_du_oru_loopback PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../common/xran_pkt)
add_dependencies(tests test_du_oru_loopback)
add_test(NAME test_du_oru_loopback COMMAND test_du_oru_loopback ${DPDK_TEST_ARGS} "--file-prefix=test_du_oru_loopback")
set_property(TEST test_du_oru_loopback PROPERTY LABELS "fronthaul;du_oru")
