# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(time_management STATIC
            time_manager.c)

add_library(time_management_core OBJECT
            time_source.c
            time_server.c
            time_client.c
            time_timer.c)

target_link_libraries(time_management PUBLIC time_management_core)
target_link_libraries(time_management_core PRIVATE log_headers)

if(ENABLE_TESTS)
  add_subdirectory(tests)
endif()
