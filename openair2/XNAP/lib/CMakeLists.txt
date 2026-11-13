# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(xnap_lib OBJECT
            xnap_lib_common.c
            xnap_gNB_interface_management.c
            xnap_gNB_mobility_management.c
)

target_link_libraries(xnap_lib PRIVATE asn1_xnap log_headers)

if(ENABLE_TESTS)
  target_compile_definitions(xnap_lib PRIVATE ENABLE_TESTS)
endif()

