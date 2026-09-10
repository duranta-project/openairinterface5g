# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_ring_buffer test_ring_buffer.cpp)
target_link_libraries(test_ring_buffer PRIVATE ring_buffer GTest::gtest)
add_dependencies(tests test_ring_buffer)
add_test(NAME test_ring_buffer
  COMMAND ./test_ring_buffer)

add_executable(test_zmq_radio test_zmq_radio.cpp)
target_link_libraries(test_zmq_radio PRIVATE zmq_radio_static UTIL CONFIG_LIB GTest::gtest zmq shlib_loader)
add_dependencies(tests test_zmq_radio)
add_test(NAME test_zmq_radio
    COMMAND ./test_zmq_radio)
