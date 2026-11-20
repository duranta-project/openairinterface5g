# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_subdirectory(MESSAGES)

add_library(nrppa
  nrppa_common.c
  nrppa_gNB.c
  nrppa_gNB_config.c
  nrppa_gNB_handlers.c
  nrppa_gNB_decoder.c
  nrppa_gNB_ue_context.c
  nrppa_gNB_location_information_transfer.c
  nrppa_gNB_encoder.c
  nrppa_gNB_measurement_information_transfer.c
  nrppa_pos_assistance.c
  )

target_link_libraries(nrppa PUBLIC asn1_nrppa)
target_link_libraries(nrppa PRIVATE asn1_lte_rrc_hdrs asn1_nr_rrc_hdrs log_headers)


if(ENABLE_TESTS)
  add_executable(test_nrppa_pos_assistance tests/test_nrppa_pos_assistance.c nrppa_pos_assistance.c)
  target_link_libraries(test_nrppa_pos_assistance PRIVATE asn1_nrppa utils)
  add_test(NAME test_nrppa_pos_assistance COMMAND test_nrppa_pos_assistance)
  add_executable(test_nrppa_trp_config tests/test_nrppa_trp_config.c nrppa_gNB_config.c)
  target_link_libraries(test_nrppa_trp_config PRIVATE CONFIG_LIB nr_common minimal_lib asn1_nr_rrc_hdrs asn1_lte_rrc_hdrs)
  add_dependencies(test_nrppa_trp_config params_libconfig)
  add_dependencies(tests test_nrppa_trp_config)
  foreach(scenario ids_only coordinates origin partial mismatched_coordinates missing_ids mismatched_ids excessive_count)
    add_test(NAME test_nrppa_trp_config.${scenario} COMMAND test_nrppa_trp_config ${scenario})
    set_tests_properties(test_nrppa_trp_config.${scenario} PROPERTIES ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}")
  endforeach()
endif()
