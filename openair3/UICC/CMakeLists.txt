# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(lib_uicc OBJECT usim_interface.c pdu_session.c)
target_include_directories(lib_uicc PRIVATE .)
target_link_libraries(lib_uicc PRIVATE log_headers sim_http)
