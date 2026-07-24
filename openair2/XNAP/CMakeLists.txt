# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_subdirectory(lib)
add_subdirectory(MESSAGES)

add_library(xnap xnap_common.c xnap_gNB.c xnap_gNB_itti_messaging.c xnap_gNB_handlers.c xnap_gNB_encoder.c)
target_link_libraries(xnap PUBLIC asn1_xnap xnap_lib)
target_link_libraries(xnap PRIVATE asn1_lte_rrc_hdrs asn1_nr_rrc_hdrs)

if(ENABLE_TESTS)
  add_subdirectory(tests)
endif()



