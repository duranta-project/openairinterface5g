# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(nr-cu-nrppa-test nr-cu-nrppa.c
               ${OPENAIR2_DIR}/GNB_APP/gnb_config_common.c
               ${OPENAIR2_DIR}/GNB_APP/gnb_config_ng.c)

target_link_libraries(nr-cu-nrppa-test PRIVATE nr_nas remote_sim nr_common lib_uicc usim_lib)
target_link_libraries(nr-cu-nrppa-test PRIVATE
                      CONFIG_LIB ITTI SCTP_CLIENT
                      f1ap ngap SECURITY
                      UTIL softmodem_common nrppa)
target_link_libraries(nr-cu-nrppa-test PRIVATE asn1_lte_rrc_hdrs asn1_nr_rrc_hdrs)
add_dependencies(nr-cu-nrppa-test params_libconfig)
