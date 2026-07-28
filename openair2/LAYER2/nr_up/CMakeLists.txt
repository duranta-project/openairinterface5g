# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(nr_up STATIC
            nr_up.c
)
target_link_libraries(nr_up PUBLIC log_headers)
target_include_directories(nr_up PUBLIC ${CMAKE_SOURCE_DIR})

add_library(nr_up_pdcp_if STATIC
            nr_up_pdcp_if.c
            nr_up_backend_if.c
)
target_link_libraries(nr_up_pdcp_if PUBLIC nr_up log_headers utils ds)
target_include_directories(nr_up_pdcp_if PUBLIC ../../..)

add_library(nr_up_rlc_queue STATIC
            nr_up_rlc_queue.c
)
target_link_libraries(nr_up_rlc_queue PRIVATE asn1_nr_rrc_hdrs)
target_include_directories(nr_up_rlc_queue PRIVATE ${CMAKE_SOURCE_DIR})

add_library(nr_up_direct STATIC
            nr_up_monolithic.c
)
target_link_libraries(nr_up_direct PUBLIC nr_up_pdcp_if nr_up_rlc_queue)
target_compile_definitions(nr_up_direct PUBLIC NR_UP_MONO_BACKEND)
target_include_directories(nr_up_direct PRIVATE ${CMAKE_SOURCE_DIR})

add_library(nr_up_f1ap STATIC
            nr_up_f1u.c
)
target_link_libraries(nr_up_f1ap PUBLIC nr_up_pdcp_if)
target_link_libraries(nr_up_f1ap PRIVATE f1ap asn1_nr_rrc asn1_lte_rrc)
target_include_directories(nr_up_f1ap PRIVATE ${CMAKE_SOURCE_DIR})
