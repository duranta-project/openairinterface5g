# SPDX-License-Identifier: LicenseRef-CSSL-1.0

pkg_check_modules(zmq REQUIRED libzmq)
add_library(ring_buffer ring_buffer.cpp)
add_library(zmq_imported zmq_imported.cpp)
target_include_directories(zmq_imported PUBLIC ./)
target_link_libraries(zmq_imported PUBLIC ${zmq_LIBRARIES} log_headers)

target_include_directories(ring_buffer PUBLIC ./)
add_library(oai_zmqdevif SHARED zmq_radio.cpp)
target_link_libraries(oai_zmqdevif PUBLIC ${zmq_LIBRARIES} ring_buffer log_headers zmq_imported channel_emulation)
target_include_directories(oai_zmqdevif PUBLIC ${zmq_INCLUDE_DIRS} ../channel_emulation)
set_target_properties(oai_zmqdevif PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_library(zmq_radio_static STATIC zmq_radio.cpp)
target_link_libraries(zmq_radio_static PUBLIC ${zmq_LIBRARIES} ring_buffer log_headers zmq_imported channel_emulation)
target_include_directories(zmq_radio_static PUBLIC ${zmq_INCLUDE_DIRS} ../channel_emulation)

if (ENABLE_TESTS)
  add_subdirectory(tests)
endif()
