# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_nr_rate_matching test_nr_rate_matching.cpp ../nr_rate_matching.c)
target_link_libraries(test_nr_rate_matching PRIVATE GTest::gtest minimal_lib)
add_dependencies(tests test_nr_rate_matching)
add_test(NAME test_nr_rate_matching
  COMMAND ./test_nr_rate_matching)
