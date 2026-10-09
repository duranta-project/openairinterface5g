# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(sim_clock_shim SHARED sim_clock_shim.c)
target_include_directories(sim_clock_shim PRIVATE ${OPENAIR_DIR})
set_target_properties(sim_clock_shim PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
