# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(nr_up_pdcp_if STATIC
            nr_up_pdcp_if.c
)
target_link_libraries(nr_up_pdcp_if PUBLIC log_headers)
target_include_directories(nr_up_pdcp_if PUBLIC ../../..)
