# SPDX-License-Identifier: LicenseRef-CSSL-1.0

# Split 7.1 Low-PHY: OFDM core, timebase, client port, engine. Role neutral.
add_library(split7 STATIC split7_wait.c split7_ofdm.c split7_timebase.c split7_port.c split7_engine.c)
target_include_directories(split7 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(split7 PUBLIC radio_common)

# UE anchor service: cell search on the engine's RX ring.
add_library(split7_ue_sync STATIC split7_sync.c)
target_link_libraries(split7_ue_sync PUBLIC split7)

if(ENABLE_TESTS)
  add_subdirectory(tests)
endif()
