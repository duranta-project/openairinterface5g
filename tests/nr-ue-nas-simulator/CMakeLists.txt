# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(nr-ue-nas-simulator-test nr-ue-nas-simulator.c
               ${OPENAIR2_DIR}/GNB_APP/gnb_config_common.c
               ${OPENAIR2_DIR}/GNB_APP/gnb_config_ng.c)

target_link_libraries(nr-ue-nas-simulator-test PRIVATE nr_nas remote_sim nr_common lib_uicc usim_lib)
target_link_libraries(nr-ue-nas-simulator-test PRIVATE
                      CONFIG_LIB ITTI SCTP_CLIENT
                      f1ap ngap SECURITY GTPV1U
                      UTIL softmodem_common)
target_link_libraries(nr-ue-nas-simulator-test PRIVATE asn1_lte_rrc_hdrs asn1_nr_rrc_hdrs)
add_dependencies(nr-ue-nas-simulator-test params_libconfig)
