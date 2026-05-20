# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(nr_up STATIC nr_up.c)

add_library(nr_up_pdcp_if STATIC nr_up_pdcp_if.c nr_up_backend_if.c)
target_link_libraries(nr_up_pdcp_if PUBLIC nr_up PRIVATE ds log_headers)

add_library(nr_up_rlc_queue STATIC nr_up_rlc_queue.c)
target_link_libraries(nr_up_rlc_queue PRIVATE asn1_nr_rrc_hdrs log_headers)

add_library(nr_up_direct OBJECT nr_up_direct.c)
target_link_libraries(nr_up_direct PUBLIC nr_up_pdcp_if nr_up_rlc_queue PRIVATE log_headers)

add_library(nr_up_ue STATIC nr_up_ue.c)
target_link_libraries(nr_up_ue PUBLIC nr_up_pdcp_if nr_up_rlc_queue)

add_library(nr_up_f1ap STATIC nr_up_f1u.c)
target_link_libraries(nr_up_f1ap PUBLIC nr_up_pdcp_if PRIVATE f1ap asn1_nr_rrc_hdrs asn1_lte_rrc_hdrs)

add_library(nr_up_du STATIC nr_up_du.c)
