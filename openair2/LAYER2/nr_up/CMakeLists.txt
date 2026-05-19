# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(nr_up STATIC nr_up.c)
target_link_libraries(nr_up PRIVATE ds log_headers)

add_library(nr_up_pdcp_if STATIC nr_up_pdcp_if.c nr_up_flow_control.c)
target_link_libraries(nr_up_pdcp_if PUBLIC nr_up PRIVATE ds log_headers)

# gNB mono: same PDCP_CUCP_CUUP compile flag as nr_pdcp_gnb
add_library(nr_up_direct OBJECT nr_up_direct.c)
target_compile_definitions(nr_up_direct PRIVATE PDCP_CUCP_CUUP)
target_link_libraries(nr_up_direct PUBLIC nr_up_pdcp_if PRIVATE log_headers)

# UE: same source, no PDCP_CUCP_CUUP
add_library(nr_up_ue OBJECT nr_up_direct.c)
target_link_libraries(nr_up_ue PUBLIC nr_up_pdcp_if PRIVATE log_headers)

add_library(nr_up_f1 STATIC nr_up_f1u.c)
target_link_libraries(nr_up_f1 PUBLIC nr_up_pdcp_if PRIVATE f1ap asn1_nr_rrc_hdrs asn1_lte_rrc_hdrs)
