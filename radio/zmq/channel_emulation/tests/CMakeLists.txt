# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_chanmod_engine test_chanmod_engine.cpp)
target_link_libraries(test_chanmod_engine PRIVATE channel_emulation UTIL CONFIG_LIB GTest::gtest)
add_dependencies(tests test_chanmod_engine)
add_test(NAME test_chanmod_engine
  COMMAND ./test_chanmod_engine)
