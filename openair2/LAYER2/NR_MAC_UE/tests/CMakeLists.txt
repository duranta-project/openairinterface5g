# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_nr_ue_power_procedures
  test_nr_ue_power_procedures.cpp)
target_link_libraries(test_nr_ue_power_procedures PRIVATE nr_ue_power_procedures GTest::gtest minimal_lib)
add_dependencies(tests test_nr_ue_power_procedures)
add_test(NAME test_nr_ue_power_procedures
  COMMAND ./test_nr_ue_power_procedures)

add_executable(test_nr_ue_ra_procedures test_nr_ue_ra_procedures.cpp)
target_link_libraries(test_nr_ue_ra_procedures PRIVATE nr_ue_ra_procedures nr_ue_power_procedures GTest::gtest minimal_lib MAC_UE_NR)
add_dependencies(tests test_nr_ue_ra_procedures)
add_test(NAME test_nr_ue_ra_procedures
  COMMAND ./test_nr_ue_ra_procedures)

add_executable(test_nr_ue_dai_wrap test_nr_ue_dai_wrap.cpp)
target_link_libraries(test_nr_ue_dai_wrap PRIVATE nr_ue_ra_procedures nr_ue_power_procedures GTest::gtest minimal_lib MAC_UE_NR)
add_dependencies(tests test_nr_ue_dai_wrap)
add_test(NAME test_nr_ue_dai_wrap
  COMMAND ./test_nr_ue_dai_wrap)
