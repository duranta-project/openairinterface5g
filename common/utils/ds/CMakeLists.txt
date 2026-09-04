# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(ds OBJECT
            byte_array.c
            byte_array_producer.c
            seq_arr.c
            hashtable.c
            obj_hashtable.c
            spsc_q.c
            work_q.c
)

target_include_directories(ds PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(ENABLE_TESTS)
  add_subdirectory(tests)
endif()
