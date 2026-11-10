# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(nfapi_pnf_lib
        src/pnf.c
        src/pnf_interface_common.c
        src/pnf_interface_lte.c
        src/pnf_interface_nr.c
        src/pnf_p7.c
        src/pnf_p7_interface_common.c
        src/pnf_p7_interface_lte.c
        src/pnf_p7_interface_nr.c)
target_link_libraries(nfapi_pnf_lib PRIVATE asn1_lte_rrc_hdrs asn1_nr_rrc_hdrs)
target_link_libraries(nfapi_pnf_lib PUBLIC nr_fapi_p7)
target_link_libraries(nfapi_pnf_lib PUBLIC nfapi_nr_lib nfapi_lte_lib)

if(OAI_WLS)
    target_compile_definitions(nfapi_pnf_lib PRIVATE ENABLE_WLS)
    target_link_libraries(nfapi_pnf_lib PUBLIC wls_integration_lib)
else()
    target_compile_definitions(nfapi_pnf_lib PRIVATE ENABLE_SOCKET)
    target_link_libraries(nfapi_pnf_lib PUBLIC nfapi_socket_lib)
endif()
target_include_directories(nfapi_pnf_lib PUBLIC public_inc)
target_include_directories(nfapi_pnf_lib PUBLIC inc)
