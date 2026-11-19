# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_nr_oru_ue test_nr_oru_ue.c)
target_link_libraries(test_nr_oru_ue PRIVATE oru_ue minimal_lib LOG m)
add_dependencies(tests test_nr_oru_ue)
add_test(NAME test_nr_oru_ue COMMAND ./test_nr_oru_ue)
