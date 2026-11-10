# SPDX-License-Identifier: LicenseRef-CSSL-1.0

find_package(sctp REQUIRED)

add_library(nfapi_pnf_common_lib
        src/pnf_common.c
        src/pnf_interface_common.c
        src/pnf_p7_common.c
        src/pnf_p7_interface_common.c)
target_link_libraries(nfapi_pnf_common_lib PUBLIC nfapi_lte_lib)
target_link_libraries(nfapi_pnf_common_lib PRIVATE sctp::sctp)
target_include_directories(nfapi_pnf_common_lib PUBLIC public_inc)
target_include_directories(nfapi_pnf_common_lib PUBLIC inc)

add_library(nfapi_lte_pnf_lib
        src/pnf_lte.c
        src/pnf_interface_lte.c
        src/pnf_p7_lte.c
        src/pnf_p7_interface_lte.c)
target_link_libraries(nfapi_lte_pnf_lib PUBLIC nfapi_pnf_common_lib nfapi_lte_lib)
target_link_libraries(nfapi_lte_pnf_lib PRIVATE asn1_lte_rrc_hdrs sctp::sctp)

add_library(nfapi_nr_pnf_lib
        src/pnf_nr.c
        src/pnf_interface_nr.c
        src/pnf_p7_nr.c
        src/pnf_p7_interface_nr.c)
target_link_libraries(nfapi_nr_pnf_lib PRIVATE asn1_nr_rrc_hdrs nr_fapi_p7)
target_link_libraries(nfapi_nr_pnf_lib PUBLIC nfapi_pnf_common_lib nfapi_nr_lib)
target_include_directories(nfapi_nr_pnf_lib PUBLIC public_inc)
target_include_directories(nfapi_nr_pnf_lib PUBLIC inc)
