# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(radio_common STATIC common_lib.c record_player.c)
target_include_directories(radio_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(radio_common PUBLIC dl log_headers shlib_loader)

# UE-centric 7.1 functional split device
add_library(ue_split7 STATIC ue_split7_device.c)
target_link_libraries(ue_split7 PUBLIC radio_common)

# Unit test for UE-centric 7.1 functional split device
add_executable(test_ue_split7 test_ue_split7.cpp)
set_target_properties(test_ue_split7 PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(test_ue_split7 PRIVATE
  ue_split7
  PHY_NR_UE
  PHY_NR_COMMON
  nr_phy_common
  nr_common
  crc_byte
  ds
  UTIL
  CONFIG_LIB
)
if(ENABLE_TESTS)
  add_dependencies(tests test_ue_split7)
  add_test(NAME test_ue_split7 COMMAND test_ue_split7)
endif()

# Compatibility verification: UE_fd_thread vs UE_thread timing equivalence
add_executable(test_fd_ue_compat test_fd_ue_compat.cpp)
set_target_properties(test_fd_ue_compat PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(test_fd_ue_compat PRIVATE
  ue_split7
  PHY_NR_UE
  PHY_NR_COMMON
  nr_phy_common
  nr_common
  crc_byte
  ds
  UTIL
  CONFIG_LIB
)
if(ENABLE_TESTS)
  add_dependencies(tests test_fd_ue_compat)
  add_test(NAME test_fd_ue_compat COMMAND test_fd_ue_compat)
endif()
