# SPDX-License-Identifier: LicenseRef-CSSL-1.0

set(DPDK_TEST_ARGS "--no-huge" "--iova-mode=va" "--no-pci" "-m" "1024")

add_executable(test_du_tx_scheduler test_du_tx_scheduler.c)
target_link_libraries(test_du_tx_scheduler PRIVATE du_tx_scheduler log_headers UTIL xran_pkt fh_compression PkgConfig::dpdk)
target_include_directories(test_du_tx_scheduler PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../common/xran_pkt)
add_dependencies(tests test_du_tx_scheduler)
add_test(NAME test_du_tx_scheduler COMMAND test_du_tx_scheduler ${DPDK_TEST_ARGS} "--file-prefix=test_du_tx_scheduler" "--vdev=net_null2" "--vdev=net_null3")
set_property(TEST test_du_tx_scheduler PROPERTY LABELS fronthaul)
