# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(channel_emulation channel_emulation.cpp chanmod_engine.cpp sim_clock_publisher.cpp)
target_include_directories(channel_emulation PUBLIC ./ ..)
target_link_libraries(channel_emulation PUBLIC log_headers ring_buffer SIMU)

if (ENABLE_TESTS)
  add_subdirectory(tests)
endif()
