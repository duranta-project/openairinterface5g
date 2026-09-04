# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_compile_options(-Wunused-parameter)
add_library(rfsimulator MODULE
            simulator.cpp
            apply_channelmod.c
            ../../openair1/PHY/TOOLS/signal_energy.c
)
target_link_libraries(rfsimulator PRIVATE SIMU log_headers)
set_target_properties(rfsimulator PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_executable(replay_node stored_node.c)
target_link_libraries (replay_node minimal_lib)

if (ENABLE_TESTS)
  add_library(rfsimulator_static
              simulator.cpp
              apply_channelmod.c
              ../../openair1/PHY/TOOLS/signal_energy.c
  )
  target_link_libraries(rfsimulator_static PUBLIC SIMU log_headers)
  add_subdirectory(tests)
endif()
