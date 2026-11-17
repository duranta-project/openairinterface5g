# SPDX-License-Identifier: LicenseRef-CSSL-1.0

if (ENABLE_TESTS)
  add_subdirectory(test)
endif()
add_library(thread-pool thread-pool.c task_ans.c core_mem.c)
target_link_libraries(thread-pool PRIVATE utils)
target_include_directories(thread-pool PUBLIC .)
