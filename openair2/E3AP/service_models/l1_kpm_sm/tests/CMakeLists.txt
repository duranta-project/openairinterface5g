# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_l1_kpm_enc test_l1_kpm_enc.c)
add_dependencies(tests test_l1_kpm_enc)
target_link_libraries(test_l1_kpm_enc PRIVATE l1_kpm_sm LOG ${T_LIB} CONFIG_LIB)
add_test(NAME test_l1_kpm_enc COMMAND ./test_l1_kpm_enc)

# The codecs are optional in the SM, so they are optional in its test too; the
# guards must match the ones on the l1_kpm_sm target.
if(JSON_C_FOUND)
  target_compile_definitions(test_l1_kpm_enc PRIVATE E3_SM_HAVE_JSON)
  target_include_directories(test_l1_kpm_enc PRIVATE ${JSON_C_INCLUDE_DIRS})
  target_link_libraries(test_l1_kpm_enc PRIVATE ${JSON_C_LIBRARIES})
endif()

if(L1_KPM_SM_HAVE_PROTOBUF)
  target_compile_definitions(test_l1_kpm_enc PRIVATE E3_SM_HAVE_PROTOBUF)
  target_include_directories(test_l1_kpm_enc PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/../MESSAGES ${PROTOBUFC_INCLUDE_DIRS})
  target_link_libraries(test_l1_kpm_enc PRIVATE l1_kpm_sm_pb)
endif()
