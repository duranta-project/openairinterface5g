# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_nr_ue_dl_slots test_nr_ue_dl_slots.c)
target_link_libraries(test_nr_ue_dl_slots PRIVATE nr_ue_dl_slots thread-pool pthread LOG minimal_lib)
add_dependencies(tests test_nr_ue_dl_slots)
add_test(NAME test_nr_ue_dl_slots COMMAND ./test_nr_ue_dl_slots)
