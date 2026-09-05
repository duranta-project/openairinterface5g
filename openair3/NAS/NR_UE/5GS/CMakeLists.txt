# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_subdirectory(5GMM/MSG)
add_subdirectory(5GMM/IES)
add_subdirectory(5GSM/QoS)
add_subdirectory(5GSM/MSG)

if(ENABLE_TESTS)
  target_compile_definitions(fgs_5gmm_lib PRIVATE ENABLE_TESTS)
  target_compile_definitions(fgs_5gmm_ies_lib PRIVATE ENABLE_TESTS)
  target_compile_definitions(fgs_5gsm_lib PRIVATE ENABLE_TESTS)
  add_subdirectory(tests)
endif()
add_library(fgs_lib fgs_nas_lib.c)

target_link_libraries(fgs_lib PUBLIC
    fgs_5gmm_lib
    fgs_5gsm_lib
    fgs_5gmm_ies_lib
    nas_common_ies_lib
    packet_filter_lib
    ds
)

target_include_directories(fgs_lib PUBLIC . 5GMM/MSG 5GMM/IES)

if(ENABLE_TESTS)
  target_compile_definitions(fgs_lib PRIVATE ENABLE_TESTS)
endif()
