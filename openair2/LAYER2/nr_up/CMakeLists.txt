# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(nr_up_rlc_queue STATIC nr_up_rlc_queue.c)
target_link_libraries(nr_up_rlc_queue PRIVATE asn1_nr_rrc_hdrs log_headers)

add_library(nr_up_direct INTERFACE)
target_link_libraries(nr_up_direct INTERFACE nr_up_rlc_queue)

add_library(nr_up_ue INTERFACE)
target_link_libraries(nr_up_ue INTERFACE nr_up_rlc_queue)

add_library(nr_up_f1ap INTERFACE)
target_link_libraries(nr_up_f1ap INTERFACE nr_up_rlc_queue)
