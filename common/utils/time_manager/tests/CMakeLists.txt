# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_time_manager_manual test_manual.c)
target_link_libraries(test_time_manager_manual time_management_core)
target_link_libraries(test_time_manager_manual LOG CONFIG_LIB ds)

add_executable(test_time_manager_auto test_auto.c)
target_link_libraries(test_time_manager_auto time_management)
target_link_libraries(test_time_manager_auto LOG CONFIG_LIB ds)

add_dependencies(tests test_time_manager_auto)
add_test(NAME time_management_tests
  COMMAND ./test_time_manager_auto)
