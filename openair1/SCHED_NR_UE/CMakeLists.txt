# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(nr_ue_dl_slots nr_ue_dl_slots.c)
target_link_libraries(nr_ue_dl_slots PUBLIC thread-pool PRIVATE utils)
target_include_directories(nr_ue_dl_slots PUBLIC .)

if (ENABLE_TESTS)
  add_subdirectory(tests)
endif()
