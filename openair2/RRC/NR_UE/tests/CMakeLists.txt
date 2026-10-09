# openair2/RRC/NR_UE/tests/CMakeLists.txt  
# SPDX-License-Identifier: LicenseRef-CSSL-1.0  
  
add_executable(test_full_config  
  test_full_config.cpp
  ../rrc_UE.c
  ../rrc_timers_and_constants.c  
  ${OPENAIR2_DIR}/RRC/NR_UE/L2_interface_ue.c
  ${OPENAIR2_DIR}/RRC/NR_UE/verify_RRC.c
  ${OPENAIR2_DIR}/LAYER2/nr_pdcp/asn1_utils.c  
  ${OPENAIR2_DIR}/LAYER2/nr_rlc/asn1_utils.c)  
target_include_directories(test_full_config PRIVATE  
  ${OPENAIR2_DIR}/RRC/NR_UE  
  ${OPENAIR2_DIR}/RRC/NR  
  ${OPENAIR2_DIR}/RRC/NR/MESSAGES
  ${OPENAIR2_DIR}/LAYER2/NR_MAC_UE
  ${OPENAIR2_DIR}/SDAP/nr_sdap 
  ${OPENAIR3_DIR}/NAS/NR_UE  
  ${OPENAIR3_DIR}/NAS/NR_UE/5GS
  ${OPENAIR3_DIR}/NAS/NR_UE/5GS/5GMM/MSG
  ${OPENAIR3_DIR}/NAS/NR_UE/5GS/5GMM/IES)  
target_link_libraries(test_full_config PRIVATE  
  GTest::gtest  
  UTIL  
  SECURITY  
  minimal_lib  
  nr_common  
  asn1_nr_rrc_hdrs  
  asn1_lte_rrc_hdrs  
  nr_rlc  
  nr_rrc  
  ITTI 
  MAC_UE_NR  
  ${T_LIB})  
target_compile_definitions(test_full_config PRIVATE ENABLE_TESTS)  
add_dependencies(tests test_full_config)  
add_test(NAME test_full_config COMMAND test_full_config)