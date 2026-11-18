# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_ue_low_phy test_ue_low_phy.cpp test_ref.c)
target_link_libraries(test_ue_low_phy PRIVATE
  -Wl,--start-group ue_low_phy PHY_NR_UE PHY_NR_COMMON nr_phy_common nr_common crc_byte ds UTIL CONFIG_LIB -Wl,--end-group m GTest::gtest)
add_dependencies(tests test_ue_low_phy)
add_test(NAME test_ue_low_phy COMMAND ./test_ue_low_phy)
set_tests_properties(test_ue_low_phy PROPERTIES
  ENVIRONMENT "LSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/lsan.supp")

add_executable(test_ue_low_phy_fd_host test_fd_host.cpp)
target_link_libraries(test_ue_low_phy_fd_host PRIVATE ue_low_phy_port GTest::gtest minimal_lib LOG)
add_dependencies(tests test_ue_low_phy_fd_host)
add_test(NAME test_ue_low_phy_fd_host COMMAND ./test_ue_low_phy_fd_host)
