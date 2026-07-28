# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(nr_rlc_core OBJECT
            nr_rlc_entity.c
            nr_rlc_entity_am.c
            nr_rlc_entity_tm.c
            nr_rlc_entity_um.c
            nr_rlc_pdu.c
            nr_rlc_sdu.c
            nr_rlc_rx_manager.c)
target_include_directories(nr_rlc_core PRIVATE ../../..)
target_link_libraries(nr_rlc_core PRIVATE log_headers)
add_library(nr_rlc STATIC
            asn1_utils.c
            nr_rlc_oai_api.c
            nr_rlc_oai_api_nr_up.c
            nr_rlc_ue_manager.c
)
# L2_NR: for PDCP
target_link_libraries(nr_rlc PUBLIC pthread f1ap nr_rlc_core log_headers)
target_include_directories(nr_rlc PRIVATE ../../..)
target_link_libraries(nr_rlc PRIVATE asn1_lte_rrc asn1_nr_rrc)

if(ENABLE_TESTS)
  add_subdirectory(tests)
endif()
