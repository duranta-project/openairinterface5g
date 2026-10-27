# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_gaussian_rng test_gaussian_rng.c)
target_link_libraries(test_gaussian_rng PRIVATE SIMU minimal_lib m pthread)
add_dependencies(tests test_gaussian_rng)
add_test(NAME test_gaussian_rng COMMAND test_gaussian_rng)
set_tests_properties(test_gaussian_rng PROPERTIES TIMEOUT 60)
