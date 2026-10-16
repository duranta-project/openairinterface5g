# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_nr_csi_im test_nr_csi_im.c)
target_link_libraries(test_nr_csi_im PRIVATE
  -Wl,--start-group PHY_NR_UE PHY_NR_COMMON -Wl,--end-group
  ds minimal_lib m)
add_dependencies(tests test_nr_csi_im)
add_test(NAME test_nr_csi_im COMMAND test_nr_csi_im)

add_executable(test_nr_pss_cfo_search_config
  test_nr_pss_cfo_search_config.c
  ../nr_pss_cfo_search.c)
add_dependencies(tests test_nr_pss_cfo_search_config)
add_test(NAME test_nr_pss_cfo_search_config COMMAND test_nr_pss_cfo_search_config)

add_executable(test_nr_pss_cfo_hypothesis
  test_nr_pss_cfo_hypothesis.c
  ../nr_pss_cfo_hypothesis.c)
target_link_libraries(test_nr_pss_cfo_hypothesis PRIVATE PHY_NR m)
add_dependencies(tests test_nr_pss_cfo_hypothesis)
add_test(NAME test_nr_pss_cfo_hypothesis COMMAND test_nr_pss_cfo_hypothesis)

add_executable(test_nr_pss_cfo_coarse_search
  test_nr_pss_cfo_coarse_search.c
  ../nr_pss_cfo_coarse_search.c
  ../nr_pss_cfo_hypothesis.c
  ../nr_pss_cfo_search.c)
target_link_libraries(test_nr_pss_cfo_coarse_search PRIVATE PHY_NR m)
add_dependencies(tests test_nr_pss_cfo_coarse_search)
add_test(NAME test_nr_pss_cfo_coarse_search COMMAND test_nr_pss_cfo_coarse_search)

add_executable(test_nr_pss_cfo_fine_search
  test_nr_pss_cfo_fine_search.c
  ../nr_pss_cfo_fine_search.c
  ../nr_pss_cfo_coarse_search.c
  ../nr_pss_cfo_hypothesis.c
  ../nr_pss_cfo_search.c)
target_link_libraries(test_nr_pss_cfo_fine_search PRIVATE PHY_NR m)
add_dependencies(tests test_nr_pss_cfo_fine_search)
add_test(NAME test_nr_pss_cfo_fine_search COMMAND test_nr_pss_cfo_fine_search)

add_executable(test_nr_pss_cfo_stage1
  test_nr_pss_cfo_stage1.c
  ../nr_pss_cfo_stage1.c
  ../nr_pss_cfo_fine_search.c
  ../nr_pss_cfo_coarse_search.c
  ../nr_pss_cfo_hypothesis.c
  ../nr_pss_cfo_search.c)
target_link_libraries(test_nr_pss_cfo_stage1 PRIVATE PHY_NR m)
add_dependencies(tests test_nr_pss_cfo_stage1)
add_test(NAME test_nr_pss_cfo_stage1 COMMAND test_nr_pss_cfo_stage1)
