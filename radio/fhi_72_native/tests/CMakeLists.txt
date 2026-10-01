# SPDX-License-Identifier: LicenseRef-CSSL-1.0

set(DPDK_TEST_ARGS "--no-huge" "--iova-mode=va" "--no-pci" "-m" "1024")

add_executable(test_du_fhi_symbols test_du_fhi_symbols.c)
target_link_libraries(test_du_fhi_symbols PRIVATE du_fhi_core log_headers UTIL CONFIG_LIB ${dpdk_LIBRARIES})
target_include_directories(test_du_fhi_symbols PRIVATE ${dpdk_INCLUDE_DIRS})
add_dependencies(tests test_du_fhi_symbols)
add_test(NAME test_du_fhi_symbols COMMAND test_du_fhi_symbols)
set_property(TEST test_du_fhi_symbols PROPERTY LABELS fronthaul)

add_executable(test_du_fhi_lookahead test_du_fhi_lookahead.c)
target_link_libraries(test_du_fhi_lookahead PRIVATE du_fhi_core log_headers UTIL CONFIG_LIB ${dpdk_LIBRARIES})
target_include_directories(test_du_fhi_lookahead PRIVATE ${dpdk_INCLUDE_DIRS})
add_dependencies(tests test_du_fhi_lookahead)
add_test(NAME test_du_fhi_lookahead COMMAND test_du_fhi_lookahead ${DPDK_TEST_ARGS} "--file-prefix=test_du_fhi_lookahead" "--vdev=net_null6")
set_property(TEST test_du_fhi_lookahead PROPERTY LABELS fronthaul)
