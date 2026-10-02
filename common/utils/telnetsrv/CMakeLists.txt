# SPDX-License-Identifier: LicenseRef-CSSL-1.0

#!
#* \brief: build rules and checks for web server shared libraries

add_library(telnetsrv MODULE
            telnetsrv.c
            telnetsrv_proccmd.c
            telnetsrv_loader.c
            telnetsrv_measurements.c
)
target_link_libraries(telnetsrv PRIVATE history)
target_link_libraries(telnetsrv PRIVATE asn1_nr_rrc_hdrs asn1_lte_rrc_hdrs)
target_link_libraries(telnetsrv PRIVATE UTIL)

message(STATUS "Add enb specific telnet functions in libtelnetsrv_enb.so")
add_library(telnetsrv_enb MODULE
            telnetsrv_enb_measurements.c
            telnetsrv_enb_phycmd.c
)
add_dependencies(telnetsrv telnetsrv_enb)
target_link_libraries(telnetsrv_enb PRIVATE asn1_nr_rrc_hdrs asn1_lte_rrc_hdrs log_headers)

message(STATUS "No specific telnet functions for gnb")

message(STATUS "No specific telnet functions for 4Gue")

message(STATUS "Add 5Gue specific telnet functions in libtelnetsrv_5Gue.so")
add_library(telnetsrv_5Gue MODULE telnetsrv_5Gue_measurements.c)
add_dependencies(telnetsrv telnetsrv_5Gue)
target_link_libraries(telnetsrv_5Gue PRIVATE asn1_nr_rrc_hdrs asn1_lte_rrc_hdrs UTIL)

message(STATUS "Add CI specific telnet functions in libtelnetsrv_ci.so")
add_library(telnetsrv_ci MODULE telnetsrv_ci.c)
target_link_libraries(telnetsrv_ci PRIVATE asn1_nr_rrc_hdrs asn1_lte_rrc_hdrs)
add_dependencies(telnetsrv telnetsrv_ci)

message(STATUS "Add CI specific telnet functions for nrUE in telnetsrv_ciUE.so")
add_library(telnetsrv_ciUE MODULE telnetsrv_ciUE.c)
target_link_libraries(telnetsrv_ciUE PRIVATE asn1_nr_rrc_hdrs asn1_lte_rrc_hdrs UTIL NR_L2_UE nr_nas)
add_dependencies(telnetsrv telnetsrv_ciUE)

message(STATUS "Add bearer specific telnet functions in libtelnetsrv_bearer.so")
add_library(telnetsrv_bearer MODULE telnetsrv_bearer.c)
target_link_libraries(telnetsrv_bearer PRIVATE asn1_nr_rrc_hdrs asn1_lte_rrc_hdrs)
add_dependencies(telnetsrv telnetsrv_bearer)

message(STATUS "Add RRC specific telnet functions in libtelnetsrv_rrc.so")
add_library(telnetsrv_rrc MODULE telnetsrv_rrc.c)
target_link_libraries(telnetsrv_rrc PRIVATE asn1_nr_rrc_hdrs asn1_lte_rrc_hdrs)
add_dependencies(telnetsrv telnetsrv_rrc)

message(STATUS "Add CI specific telnet functions in libtelnetsrv_o1.so")
add_library(telnetsrv_o1 MODULE telnetsrv_o1.c)
target_link_libraries(telnetsrv_o1 PRIVATE asn1_nr_rrc_hdrs asn1_lte_rrc_hdrs)
add_dependencies(telnetsrv telnetsrv_o1)

message(STATUS "Add NTN specific telnet functions in libtelnetsrv_ntn.so")
add_library(telnetsrv_ntn MODULE telnetsrv_ntn.c)
target_link_libraries(telnetsrv_ntn PRIVATE asn1_nr_rrc_hdrs asn1_lte_rrc_hdrs)
add_dependencies(telnetsrv telnetsrv_ntn)

# all libraries should be written to root build dir
set_target_properties(telnetsrv telnetsrv_enb telnetsrv_5Gue telnetsrv_ci telnetsrv_ciUE
                      telnetsrv_bearer telnetsrv_rrc telnetsrv_o1 telnetsrv_ntn
                      PROPERTIES LIBRARY_OUTPUT_DIRECTORY ../../..
)
