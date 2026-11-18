# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_ul_chest_interp test_ul_chest_interp.cpp)
target_link_libraries(test_ul_chest_interp PRIVATE GTest::gtest LOG minimal_lib)
add_dependencies(tests test_ul_chest_interp)
add_test(NAME test_ul_chest_interp COMMAND ./test_ul_chest_interp)
