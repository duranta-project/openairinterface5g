# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(channel_emulation channel_emulation.cpp chanmod_engine.cpp ntn_sim_clock_publisher.cpp)
target_include_directories(channel_emulation PUBLIC ./ ../zmq)
target_link_libraries(channel_emulation PUBLIC log_headers ring_buffer SIMU)

add_library(ntn_sim_clock_shim SHARED ../../tools/ntn/ntn_sim_clock_shim.c)
target_include_directories(ntn_sim_clock_shim PRIVATE ${OPENAIR_DIR})
set_target_properties(ntn_sim_clock_shim PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if (ENABLE_TESTS)
  add_subdirectory(tests)
endif()
