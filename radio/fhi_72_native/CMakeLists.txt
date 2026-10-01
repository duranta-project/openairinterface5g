# SPDX-License-Identifier: LicenseRef-CSSL-1.0

if(NOT OAI_RU_FRONTHAUL)
  message(FATAL_ERROR "OAI_FHI72_NATIVE requires OAI_RU_FRONTHAUL to be ON (it links against fronthaul/du)")
endif()

pkg_check_modules(dpdk REQUIRED libdpdk)

# du_fhi_core has no L2 dependency (TDD classification, catch-up, UL-grant look-ahead,
# FFT-shift helpers); du_fhi_prach/du_fhi_isolate need get_nr_prach_occasion_info_from_index() /
# get_nr_prach_sched_from_info() / get_next_nr_prach(), resolved at dlopen time against the host
# gNB executable for the MODULE below (radio/fhi_72/CMakeLists.txt's oran_fhlib_5g does the same
# for its own OAI-internal calls), but requiring a real link-time dependency for a plain
# executable. Kept as a separate static library so unit tests that only need the TDD/catch-up/
# look-ahead logic can link it alone, without dragging in L2.
add_library(du_fhi_core STATIC du_fhi_core.c)
target_include_directories(du_fhi_core PUBLIC ${dpdk_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(du_fhi_core PUBLIC du_fh du_packet_processor du_io xran_pkt fh_compression ${dpdk_LIBRARIES})

add_library(du_fhlib_native MODULE
            du_fhi_config.c
            du_fhi_prach.c
            du_fhi_isolate.c
)
set_target_properties(du_fhlib_native PROPERTIES COMPILE_FLAGS "-fvisibility=hidden -march=native")
target_include_directories(du_fhlib_native PRIVATE ${dpdk_INCLUDE_DIRS})
target_link_libraries(du_fhlib_native PRIVATE du_fhi_core log_headers)
set_target_properties(du_fhlib_native PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if (ENABLE_TESTS)
  add_subdirectory(tests)
endif()
