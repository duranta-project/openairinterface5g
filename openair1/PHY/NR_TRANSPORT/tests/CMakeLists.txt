# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_nr_dbt test_nr_dbt.c)
target_link_libraries(test_nr_dbt PRIVATE PHY_NR minimal_lib)
add_dependencies(tests test_nr_dbt)
add_test(NAME test_nr_dbt
  COMMAND ./test_nr_dbt)
