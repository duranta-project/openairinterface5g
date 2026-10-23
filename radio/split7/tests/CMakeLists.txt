# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_split7 test_split7.c)
set_target_properties(test_split7 PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(test_split7 PRIVATE
  -Wl,--start-group split7 PHY_NR_UE PHY_NR_COMMON nr_phy_common nr_common crc_byte ds UTIL CONFIG_LIB -Wl,--end-group m)
add_dependencies(tests test_split7)
add_test(NAME test_split7 COMMAND test_split7)
