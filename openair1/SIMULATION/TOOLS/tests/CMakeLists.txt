# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_cdl_channel test_cdl_channel.cpp)
target_link_libraries(test_cdl_channel PRIVATE cdl_channel GTest::gtest minimal_lib LOG)
add_dependencies(tests test_cdl_channel)
add_test(NAME test_cdl_channel COMMAND test_cdl_channel)
