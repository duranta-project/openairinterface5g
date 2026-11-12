# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_nr_ue_power_procedures
  test_nr_ue_power_procedures.cpp)
target_link_libraries(test_nr_ue_power_procedures PRIVATE nr_ue_power_procedures GTest::gtest minimal_lib)
add_dependencies(tests test_nr_ue_power_procedures)
add_test(NAME test_nr_ue_power_procedures
  COMMAND ./test_nr_ue_power_procedures)

add_executable(test_nr_ue_ra_procedures test_nr_ue_ra_procedures.cpp nr_ue_test_params.c)
target_link_libraries(test_nr_ue_ra_procedures PRIVATE nr_ue_ra_procedures nr_ue_power_procedures GTest::gtest minimal_lib MAC_UE_NR)
add_dependencies(tests test_nr_ue_ra_procedures)
add_test(NAME test_nr_ue_ra_procedures
  COMMAND ./test_nr_ue_ra_procedures)

add_executable(test_nr_sl_slot_bitmap test_nr_sl_slot_bitmap.c)
target_link_libraries(test_nr_sl_slot_bitmap PRIVATE nr_sl_slot_bitmap)
add_dependencies(tests test_nr_sl_slot_bitmap)
add_test(NAME test_nr_sl_slot_bitmap
  COMMAND ./test_nr_sl_slot_bitmap)

add_executable(test_nr_ue_prs_config test_nr_ue_prs_config.c ../config_ue.c ../main_ue_nr.c)
target_include_directories(test_nr_ue_prs_config PRIVATE ${OPENAIR2_DIR}/LAYER2)
target_compile_options(test_nr_ue_prs_config PRIVATE -ffunction-sections -fdata-sections)
target_link_options(test_nr_ue_prs_config PRIVATE -Wl,--gc-sections,--no-export-dynamic,--wrap=get_mac_inst)
target_link_libraries(test_nr_ue_prs_config PRIVATE asn1_nr_rrc asn1_lte_rrc_hdrs nr_common minimal_lib ds m)
add_dependencies(tests test_nr_ue_prs_config)
add_test(NAME test_nr_ue_prs_config COMMAND test_nr_ue_prs_config)
