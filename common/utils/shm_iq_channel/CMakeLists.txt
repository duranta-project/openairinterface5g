# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(shm_td_iq_channel shm_td_iq_channel.c)
target_include_directories(shm_td_iq_channel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(shm_fd_iq_channel shm_fd_iq_channel.c)
target_include_directories(shm_fd_iq_channel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(shm_fd_iq_channel PRIVATE pthread)

if (ENABLE_TESTS)
  add_subdirectory(tests)
endif()
