# SPDX-License-Identifier: LicenseRef-CSSL-1.0

set(DPDK_TEST_ARGS "--no-huge" "--iova-mode=va" "--no-pci" "-m" "1024")

add_executable(test_du_packet_processor test_du_packet_processor.c)
target_link_libraries(test_du_packet_processor PRIVATE du_packet_processor log_headers UTIL xran_pkt fh_compression ${dpdk_LIBRARIES})
target_include_directories(test_du_packet_processor PRIVATE ${dpdk_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/../../common/xran_pkt)
add_dependencies(tests test_du_packet_processor)
add_test(NAME test_du_packet_processor COMMAND test_du_packet_processor ${DPDK_TEST_ARGS} "--file-prefix=test_du_packet_processor" "--vdev=net_null0" "--vdev=net_null1")
set_property(TEST test_du_packet_processor PROPERTY LABELS fronthaul)

add_executable(test_du_tx_scheduler test_du_tx_scheduler.c)
target_link_libraries(test_du_tx_scheduler PRIVATE du_packet_processor log_headers UTIL xran_pkt fh_compression ${dpdk_LIBRARIES})
target_include_directories(test_du_tx_scheduler PRIVATE ${dpdk_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/../../common/xran_pkt)
add_dependencies(tests test_du_tx_scheduler)
add_test(NAME test_du_tx_scheduler COMMAND test_du_tx_scheduler ${DPDK_TEST_ARGS} "--file-prefix=test_du_tx_scheduler" "--vdev=net_null2" "--vdev=net_null3")
set_property(TEST test_du_tx_scheduler PROPERTY LABELS fronthaul)

add_executable(test_du_fh test_du_fh.c)
target_link_libraries(test_du_fh PRIVATE du_fh log_headers UTIL xran_pkt fh_compression ${dpdk_LIBRARIES})
target_include_directories(test_du_fh PRIVATE ${dpdk_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/../../common/xran_pkt)
add_dependencies(tests test_du_fh)
add_test(NAME test_du_fh COMMAND test_du_fh ${DPDK_TEST_ARGS} "--file-prefix=test_du_fh" "--vdev=net_null4" "--vdev=net_null5")
set_property(TEST test_du_fh PROPERTY LABELS fronthaul)
