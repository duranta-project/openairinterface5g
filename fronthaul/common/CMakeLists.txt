# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(fh_timer fh_timer.c)
target_include_directories(fh_timer PUBLIC ./)
target_include_directories(fh_timer PRIVATE ${dpdk_INCLUDE_DIRS})
target_link_libraries(fh_timer PRIVATE ${dpdk_LIBRARIES} m pthread)
target_link_libraries(fh_timer PUBLIC UTIL)

add_library(fh_send fh_send.c)
target_include_directories(fh_send PUBLIC ./)
target_include_directories(fh_send PRIVATE ${dpdk_INCLUDE_DIRS})
target_link_libraries(fh_send PUBLIC fh_timer UTIL)
target_link_libraries(fh_send PRIVATE ${dpdk_LIBRARIES})

add_library(fh_recv fh_recv.c)
target_include_directories(fh_recv PUBLIC ./)
target_include_directories(fh_recv PRIVATE ${dpdk_INCLUDE_DIRS})
target_link_libraries(fh_recv PRIVATE ${dpdk_LIBRARIES})
target_link_libraries(fh_recv PUBLIC fh_timer UTIL)

add_library(fh_compression fh_compression.c)
target_include_directories(fh_compression PUBLIC ./)
target_link_libraries(fh_compression PRIVATE UTIL)

add_library(fh_common INTERFACE)
target_link_libraries(fh_common INTERFACE fh_timer fh_send fh_recv)
target_include_directories(fh_common INTERFACE ./)

add_subdirectory(xran_pkt)

if (ENABLE_TESTS)
    add_subdirectory(tests)
endif()
